// Couche orchestration du visage : tâche FreeRTOS qui anime les yeux
// en continu (mode IDLE par défaut), et expose une API émotion/regard
// que les scénarios JSON peuvent piloter.

#include "ui_face.h"
#include "eyes_anim.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG "ui_face"

static eyes_anim_state_t s_state;
static SemaphoreHandle_t s_mutex = NULL;
static bool              s_started = false;

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
    while (1) {
        eyes_anim_state_t snapshot;
        xSemaphoreTake(s_mutex, portMAX_DELAY);
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
    BaseType_t ok = xTaskCreatePinnedToCore(eye_task, "eye_task",
                                            6144, NULL, 4, NULL, 1);
    if (ok != pdPASS) return ESP_FAIL;
    s_started = true;
    return ESP_OK;
}

void ui_face_set_emotion(ui_face_emotion_t e)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    apply_emotion(e, &s_state);
    xSemaphoreGive(s_mutex);
    ESP_LOGI(TAG, "emotion = %d", e);
}

void ui_face_look(ui_face_direction_t dir)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
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
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.forced_blink = true;
    xSemaphoreGive(s_mutex);
}
