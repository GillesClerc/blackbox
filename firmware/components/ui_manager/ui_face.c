// Couche orchestration du visage : tâche FreeRTOS qui anime les yeux
// en continu (mode IDLE par défaut), et expose une API émotion/regard
// que les scénarios JSON peuvent piloter.

#include "ui_face.h"
#include "eyes_anim.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_task_wdt.h"

#define TAG "ui_face"

// M1 : timeout court sur le mutex — l'eye_task tient le mutex <1 µs (lecture snapshot).
// 200 ms couvre tout burst CPU légitime sans bloquer indéfiniment.
#define MUTEX_TIMEOUT_MS 200

static eyes_anim_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;
static bool              s_started = false;
static uint32_t          s_mutex_timeout_count = 0;

static void apply_emotion(ui_face_emotion_t e, eyes_anim_state_t *out)
{
    out->lid_top_bias    = 0;
    out->lid_bot_bias    = 0;
    out->iris_scale_bias = 0;
    out->forced_closed   = false;

    switch (e) {
    case UI_FACE_IDLE:
        // valeurs par défaut
        break;
    case UI_FACE_HAPPY:
        // Sourire : remonte la paupière inférieure (plisse en bas)
        out->lid_bot_bias = 40;
        break;
    case UI_FACE_SAD:
        // Triste : paupière supérieure descend un peu
        out->lid_top_bias = 30;
        break;
    case UI_FACE_SURPRISED:
        // Grand ouvert + pupille dilatée
        out->lid_top_bias    = -20;
        out->lid_bot_bias    = -20;
        out->iris_scale_bias = -40;
        break;
    case UI_FACE_SLEEPY:
        // Paupières à moitié fermées
        out->lid_top_bias = 70;
        out->lid_bot_bias = 20;
        break;
    case UI_FACE_ANGRY:
        // Plissement (paupière sup. basse) + pupille contractée
        out->lid_top_bias    = 60;
        out->iris_scale_bias = 60;
        break;
    case UI_FACE_CLOSED:
        out->forced_closed = true;
        break;
    }
}

static void eye_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "eye task running");
    // TWDT : détecte un freeze de l'animation (SPI bloqué, mutex jamais rendu...).
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK) ESP_LOGW(TAG, "esp_task_wdt_add: %s", esp_err_to_name(wdt_ret));
    while (1) {
        if (wdt_ret == ESP_OK) esp_task_wdt_reset();
        eyes_anim_state_t snapshot;
        // M1 : timeout 200 ms — la section critique est <1 µs (copie struct).
        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
            s_mutex_timeout_count++;
            ESP_LOGW(TAG, "eye_task: mutex timeout #%lu", (unsigned long)s_mutex_timeout_count);
            taskYIELD();
            continue;
        }
        snapshot = s_state;
        // forced_blink est one-shot, on le reset après consommation
        s_state.forced_blink = false;
        xSemaphoreGive(s_mutex);

        eyes_anim_step(&snapshot);
        // Pas de delay : split() est blocant et régule lui-même le timing
        // via la durée des sous-mouvements.
        taskYIELD();
    }
}

esp_err_t ui_face_init(void)
{
    eyes_anim_state_init(&s_state);
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;
    esp_err_t ret = eyes_anim_init();
    if (ret != ESP_OK) return ret;
    ESP_LOGI(TAG, "ui_face init OK");
    return ESP_OK;
}

esp_err_t ui_face_start(void)
{
    if (s_started) return ESP_OK;
    // M2 : stack 8192 — draw_eye + split() récursif + overhead FreeRTOS dépassaient 6144.
    BaseType_t ok = xTaskCreatePinnedToCore(eye_task, "eye_task",
                                            8192, NULL, 4, NULL, 1);
    if (ok != pdPASS) return ESP_FAIL;
    s_started = true;
    return ESP_OK;
}

void ui_face_set_emotion(ui_face_emotion_t e)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        s_mutex_timeout_count++;
        ESP_LOGW(TAG, "set_emotion: mutex timeout #%lu", (unsigned long)s_mutex_timeout_count);
        return;
    }
    apply_emotion(e, &s_state);
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "emotion = %d", e);
}

void ui_face_look(ui_face_direction_t dir)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        s_mutex_timeout_count++;
        ESP_LOGW(TAG, "look: mutex timeout #%lu", (unsigned long)s_mutex_timeout_count);
        return;
    }
    switch (dir) {
    case UI_FACE_LOOK_CENTER:
        s_state.look_forced = false;  // reprend mouvement autonome
        s_state.look_x = 0;
        s_state.look_y = 0;
        break;
    case UI_FACE_LOOK_LEFT:
        s_state.look_forced = true;
        s_state.look_x = -400;
        s_state.look_y = 0;
        break;
    case UI_FACE_LOOK_RIGHT:
        s_state.look_forced = true;
        s_state.look_x = 400;
        s_state.look_y = 0;
        break;
    case UI_FACE_LOOK_UP:
        s_state.look_forced = true;
        s_state.look_x = 0;
        s_state.look_y = -400;
        break;
    case UI_FACE_LOOK_DOWN:
        s_state.look_forced = true;
        s_state.look_x = 0;
        s_state.look_y = 400;
        break;
    }
    xSemaphoreGive(s_mutex);
}

void ui_face_blink(void)
{
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(MUTEX_TIMEOUT_MS)) != pdTRUE) {
        s_mutex_timeout_count++;
        ESP_LOGW(TAG, "blink: mutex timeout #%lu", (unsigned long)s_mutex_timeout_count);
        return;
    }
    s_state.forced_blink = true;
    xSemaphoreGive(s_mutex);
}
