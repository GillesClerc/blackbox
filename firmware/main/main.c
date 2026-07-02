#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "hal_display.h"
#include "ui_face.h"
#include "hal_i2c_bus.h"
#include "hal_audio.h"
#include "hal_touch.h"
#include "hal_leds.h"
#include "scenario_engine.h"
#include "config_manager.h"
#include "hal_box_auth.h"
#include "hal_wifi.h"
#include "hal_storage.h"
#include "cloud_client.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#define TAG "main"

extern const char capitaine_verdier_json_start[] asm("_binary_capitaine_verdier_json_start");
extern const char capitaine_verdier_json_end[]   asm("_binary_capitaine_verdier_json_end");

#ifdef HAS_AMBIENT_MP3
extern const uint8_t _binary_ambient_mp3_start[];
extern const uint8_t _binary_ambient_mp3_end[];
#endif

// ─── Musique de fond (fallback sans MP3) ─────────────────────────────────────

#ifndef HAS_AMBIENT_MP3
static const hal_audio_bg_note_t s_ambient[] = {
    {110, 250, 1800}, {0, 0, 600}, {138, 180, 1200}, {0, 0, 400},
    {123, 200, 2000}, {0, 0, 1000}, {110, 350, 2500}, {0, 0, 1400},
    {147, 200, 1000}, {138, 150, 1500}, {0, 0, 2200},
    {110, 200, 800},  {0, 0, 3000},
};
#endif

// ─── Scénario depuis la carte SD ─────────────────────────────────────────────

#define SCENARIO_SD_ROOT  "/sdcard/scenarios"
#define SCENARIO_JSON_MAX (256 * 1024)

// Dossier du scénario actif sur SD ("" si scénario embarqué) — sert aussi
// à localiser les assets (ambient.mp3, ...).
static char s_scenario_dir[280];

// Cherche le premier dossier de /sdcard/scenarios contenant scenario.json et
// le charge en PSRAM (NUL-terminé). NULL si SD absente ou rien d'exploitable.
static char *scenario_json_from_sd(void)
{
    DIR *root = opendir(SCENARIO_SD_ROOT);
    if (!root) return NULL;

    char *json = NULL;
    struct dirent *e;
    while (!json && (e = readdir(root)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[320];
        snprintf(path, sizeof(path), SCENARIO_SD_ROOT "/%s/scenario.json", e->d_name);
        FILE *f = fopen(path, "rb");
        if (!f) {
            char dpath[320];
            snprintf(dpath, sizeof(dpath), SCENARIO_SD_ROOT "/%s", e->d_name);
            ESP_LOGW(TAG, "pas de scenario.json dans %s :", dpath);
            hal_storage_list_dir(dpath);
            continue;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size <= 0 || size > SCENARIO_JSON_MAX) {
            ESP_LOGW(TAG, "scenario.json hors limites (%ld octets): %s", size, path);
            fclose(f);
            continue;
        }
        json = heap_caps_malloc((size_t)size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!json) {
            ESP_LOGE(TAG, "alloc PSRAM scénario échouée (%ld octets)", size);
            fclose(f);
            break;
        }
        size_t n = fread(json, 1, (size_t)size, f);
        fclose(f);
        if (n != (size_t)size) {
            ESP_LOGW(TAG, "lecture incomplète %u/%ld: %s", (unsigned)n, size, path);
            heap_caps_free(json);
            json = NULL;
            continue;
        }
        json[size] = '\0';
        snprintf(s_scenario_dir, sizeof(s_scenario_dir), SCENARIO_SD_ROOT "/%s", e->d_name);
        ESP_LOGI(TAG, "scénario SD: %s (%ld octets)", path, size);
    }
    closedir(root);
    return json;
}

// ─── Helpers LED ─────────────────────────────────────────────────────────────

static void led_hex(const char *hex)
{
    if (!hex || hex[0] != '#' || strlen(hex) < 7)
        hal_leds_clear();
    else
        hal_leds_fill_hex(hex, 80);
    hal_leds_show();
}

// ─── Audio ───────────────────────────────────────────────────────────────────

static void play_audio(const char *name)
{
    if (!name) return;

    if (strcmp(name, "correct") == 0) {
        static const uint16_t f[] = {659, 784, 1047};
        static const uint16_t d[] = {130, 130,  280};
        hal_audio_play_sequence(f, d, 3, 30);
        return;
    }
    if (strcmp(name, "wrong") == 0) {
        static const uint16_t f[] = {220, 185};
        static const uint16_t d[] = {200, 350};
        hal_audio_play_sequence(f, d, 2, 20);
        return;
    }
    if (strcmp(name, "victoire") == 0) {
        static const uint16_t f[] = {523, 659, 784, 1047, 1319};
        static const uint16_t d[] = {120, 120, 120,  120,  500};
        hal_audio_play_sequence(f, d, 5, 25);
        return;
    }
    if (strcmp(name, "activation") == 0) {
        static const uint16_t f[] = {440, 554, 659};
        static const uint16_t d[] = {120, 120, 250};
        hal_audio_play_sequence(f, d, 3, 20);
        return;
    }
    if (strcmp(name, "intro_ambient") == 0) {
        static const uint16_t f[] = {110, 138, 165, 185};
        static const uint16_t d[] = {350, 250, 250, 500};
        hal_audio_play_sequence(f, d, 4, 80);
        return;
    }

    typedef struct { const char *n; uint16_t f; uint16_t d; } m_t;
    static const m_t tbl[] = {
        {"hint_medallion",      330, 350},
        {"hint_compass",        370, 350},
        {"hint_compass_final",  392, 500},
        {"hint_code",           349, 350},
        {"hint_code_final",     392, 500},
        {"hint_tilt",           415, 350},
        {NULL, 0, 0}
    };
    for (int i = 0; tbl[i].n; i++) {
        if (strcmp(name, tbl[i].n) == 0) {
            hal_audio_play_tone(tbl[i].f, tbl[i].d);
            return;
        }
    }
    hal_audio_play_tone(440, 120);
}

// ─── Callbacks scénario (texte → log en attendant le nouveau UI) ─────────────

static void action_screen_main(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    ESP_LOGI("scenario", "[MAIN] %s", text && text->valuestring ? text->valuestring : "");
}

static void action_screen_secondary(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    ESP_LOGI("scenario", "[HINT] %s", text && text->valuestring ? text->valuestring : "");
}

static void action_led(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *color = cJSON_GetObjectItem(params, "color");
    led_hex(color ? color->valuestring : NULL);
}

static void action_audio(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *play = cJSON_GetObjectItem(params, "play");
    if (play && play->valuestring) play_audio(play->valuestring);
}

static void action_servo(const char *name, const cJSON *params)
{
    (void)name; (void)params;
    ESP_LOGI(TAG, "servo (Phase 2)");
}

// Le flash de LEDs prend ~720 ms (4 × 180 ms) avec des vTaskDelay bloquants.
// Exécuté directement dans la callback du scenario_engine, il gèlait l'animation
// des yeux (eye_task prio 4 core 1 préempté par scenario non-pinned prio 5).
// → on délègue à une task dédiée prio 3 sur core 0, communiquée par queue.
typedef struct {
    char    hex[8];   // "#RRGGBB" + null
    uint8_t count;
} flash_cmd_t;

static QueueHandle_t s_flash_queue = NULL;

static void flash_task(void *arg)
{
    (void)arg;
    flash_cmd_t cmd;
    while (1) {
        if (xQueueReceive(s_flash_queue, &cmd, pdMS_TO_TICKS(500)) != pdTRUE) continue;
        for (int i = 0; i < cmd.count; i++) {
            hal_leds_fill_hex(cmd.hex, 255); hal_leds_show();
            vTaskDelay(pdMS_TO_TICKS(100));
            hal_leds_clear();                hal_leds_show();
            vTaskDelay(pdMS_TO_TICKS(80));
        }
    }
}

static void action_flash(const char *name, const cJSON *params)
{
    (void)name;
    if (!s_flash_queue) return;
    const cJSON *color = cJSON_GetObjectItem(params, "color");
    const cJSON *count = cJSON_GetObjectItem(params, "count");
    flash_cmd_t cmd = {0};
    const char *hex = (color && color->valuestring) ? color->valuestring : "#FFD700";
    strncpy(cmd.hex, hex, sizeof(cmd.hex) - 1);
    cmd.count = (count && cJSON_IsNumber(count)) ? (uint8_t)count->valueint : 4;
    // Non-bloquant : si la queue est pleine on drop (cosmétique, pas critique).
    xQueueSend(s_flash_queue, &cmd, 0);
}

// ─── Actions scénario : visage animé ─────────────────────────────────────────

static void action_eye_blink(const char *name, const cJSON *params)
{
    (void)name; (void)params;
    ui_face_blink();
}

static void action_eye_emotion(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *type = cJSON_GetObjectItem(params, "type");
    if (!type || !type->valuestring) return;
    if      (strcmp(type->valuestring, "happy")     == 0) ui_face_set_emotion(UI_FACE_HAPPY);
    else if (strcmp(type->valuestring, "sad")       == 0) ui_face_set_emotion(UI_FACE_SAD);
    else if (strcmp(type->valuestring, "surprised") == 0) ui_face_set_emotion(UI_FACE_SURPRISED);
    else if (strcmp(type->valuestring, "sleepy")    == 0) ui_face_set_emotion(UI_FACE_SLEEPY);
    else if (strcmp(type->valuestring, "angry")     == 0) ui_face_set_emotion(UI_FACE_ANGRY);
    else if (strcmp(type->valuestring, "closed")    == 0) ui_face_set_emotion(UI_FACE_CLOSED);
    else                                                   ui_face_set_emotion(UI_FACE_IDLE);
}

static void action_eye_look(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *dir = cJSON_GetObjectItem(params, "direction");
    if (!dir || !dir->valuestring) return;
    if      (strcmp(dir->valuestring, "left")  == 0) ui_face_look(UI_FACE_LOOK_LEFT);
    else if (strcmp(dir->valuestring, "right") == 0) ui_face_look(UI_FACE_LOOK_RIGHT);
    else if (strcmp(dir->valuestring, "up")    == 0) ui_face_look(UI_FACE_LOOK_UP);
    else if (strcmp(dir->valuestring, "down")  == 0) ui_face_look(UI_FACE_LOOK_DOWN);
    else                                              ui_face_look(UI_FACE_LOOK_CENTER);
}

// ─── Tâche clavier MPR121 ────────────────────────────────────────────────────

#define HOLD_TICKS pdMS_TO_TICKS(2000)

static char s_code[16];
static int  s_code_len = 0;

static void touch_task(void *arg)
{
    (void)arg;
    esp_err_t ret = hal_touch_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "touch absent: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    hal_touch_data_t prev = {0}, curr;
    TickType_t hold_start[HAL_TOUCH_NUM_CH] = {0};
    bool       hold_fired[HAL_TOUCH_NUM_CH] = {false};

    while (1) {
        if (hal_touch_read(&curr) != ESP_OK) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < HAL_TOUCH_NUM_CH; i++) {
            bool rose  = curr.ch[i] && !prev.ch[i];
            bool fell  = !curr.ch[i] && prev.ch[i];
            bool held  = curr.ch[i] && !hold_fired[i] && hold_start[i]
                         && (now - hold_start[i]) >= HOLD_TICKS;

            if (rose) { hold_start[i] = now; hold_fired[i] = false; }

            if (held) {
                hold_fired[i] = true;
                hal_audio_play_tone(1200, 100);
                scenario_event_t evt = {0};
                if      (i == 11) { evt.type = EVT_RFID_READ;    strncpy(evt.str, "04:VE:RD:01", sizeof(evt.str)-1); }
                else if (i == 10) { evt.type = EVT_ROTARY_VALUE; evt.int_val = 270; }
                else if (i ==  9) { evt.type = EVT_ACCEL_TILT;   evt.int_val = 15;  }
                if (evt.type != EVT_NONE) scenario_engine_post_event(&evt);
            }

            if (fell && !hold_fired[i]) {
                if (i <= 9) {
                    if (s_code_len < (int)(sizeof(s_code) - 1)) {
                        s_code[s_code_len++] = '0' + i;
                        s_code[s_code_len]   = '\0';
                        ESP_LOGI(TAG, "code=%s", s_code);
                        hal_audio_play_tone(880 + i * 40, 40);
                    }
                } else if (i == 10) {
                    if (s_code_len > 0) {
                        s_code[--s_code_len] = '\0';
                        ESP_LOGI(TAG, "code=%s", s_code);
                        hal_audio_play_tone(440, 50);
                    }
                } else if (i == 11) {
                    if (s_code_len > 0) {
                        scenario_event_t evt = { .type = EVT_KEYPAD_CODE };
                        strncpy(evt.str, s_code, sizeof(evt.str) - 1);
                        scenario_engine_post_event(&evt);
                        s_code_len = 0; s_code[0] = '\0';
                    }
                }
                hold_start[i] = 0;
            }
        }

        prev = curr;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Tâche réseau de boot (core 0) : la connexion WiFi bloque jusqu'à 15 s — à
// isoler de main_task. Le HTTPS (challenge/auth/sync) vit dans cloud_client.
static void net_boot_task(void *arg)
{
    if (hal_wifi_connect(15000) == ESP_OK) {
        cloud_client_request_sync();
    }
    vTaskDelete(NULL);
}

// ─── app_main ────────────────────────────────────────────────────────────────

void app_main(void)
{
    // Hardware init
    hal_leds_init(48, 1);
    hal_leds_clear();
    hal_leds_show();

    // Task dédiée pour action_flash (sinon bloque scenario_engine ~720ms et
    // préempte eye_task sur core 1).
    s_flash_queue = xQueueCreate(4, sizeof(flash_cmd_t));
    xTaskCreatePinnedToCore(flash_task, "flash", 2048, NULL, 3, NULL, 0);

    ESP_ERROR_CHECK(hal_display_init());
    hal_display_fill_all(EYE_BLACK);
    ESP_ERROR_CHECK(config_manager_init());

    // Identifiants box (provisionnés par tools/provision_box.py). Non bloquant :
    // la box tourne sans, l'auth serveur sera juste indisponible.
    if (hal_box_auth_init() != ESP_OK) {
        ESP_LOGW(TAG, "box non provisionnée — lancer tools/provision_box.py");
    }

    ESP_ERROR_CHECK(ui_face_init());
    ESP_ERROR_CHECK(ui_face_start());

    // I2C + audio
    ESP_ERROR_CHECK(hal_i2c_bus_init());
    ESP_ERROR_CHECK(hal_audio_init());
    hal_audio_set_volume(config_get_volume());

    // Carte SD optionnelle : scénario + assets si présente, sinon embarqué.
    if (hal_storage_init() != ESP_OK) {
        ESP_LOGW(TAG, "SD absente ou illisible — scénario embarqué");
    } else {
        hal_storage_list_dir(STORAGE_MOUNT_POINT);
        hal_storage_list_dir(SCENARIO_SD_ROOT);
    }

    char *sd_json = scenario_json_from_sd();

    // Scenario engine — sans UI : les actions screen_* loggent les textes.
    esp_err_t ret = scenario_engine_init(sd_json ? sd_json : capitaine_verdier_json_start);
    if (ret != ESP_OK && sd_json) {
        ESP_LOGW(TAG, "scénario SD invalide — fallback sur le scénario embarqué");
        s_scenario_dir[0] = '\0';
        ret = scenario_engine_init(capitaine_verdier_json_start);
    }
    if (sd_json) heap_caps_free(sd_json);  // cJSON_Parse a copié le contenu
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "scenario_engine_init: %s", esp_err_to_name(ret));
    } else {
        scenario_engine_register_action("screen_main",      action_screen_main);
        scenario_engine_register_action("screen_secondary", action_screen_secondary);
        scenario_engine_register_action("led",              action_led);
        scenario_engine_register_action("audio",            action_audio);
        scenario_engine_register_action("servo",            action_servo);
        scenario_engine_register_action("flash",            action_flash);
        scenario_engine_register_action("eye_blink",        action_eye_blink);
        scenario_engine_register_action("eye_emotion",      action_eye_emotion);
        scenario_engine_register_action("eye_look",         action_eye_look);

        ret = scenario_engine_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "scenario_engine_start: %s", esp_err_to_name(ret));
        } else {
            bool bg_started = false;
            if (s_scenario_dir[0]) {
                char mp3_path[300];
                snprintf(mp3_path, sizeof(mp3_path), "%s/ambient.mp3", s_scenario_dir);
                bg_started = (hal_audio_play_bg(mp3_path) == ESP_OK);
            }
            if (!bg_started) {
#ifdef HAS_AMBIENT_MP3
                hal_audio_bg_mp3_start(_binary_ambient_mp3_start,
                                   _binary_ambient_mp3_end - _binary_ambient_mp3_start);
#else
                hal_audio_bg_start(s_ambient, sizeof(s_ambient) / sizeof(s_ambient[0]));
#endif
            }
        }
    }

    // Touch input task — m1 : pinner sur core 0 (eye_task sur core 1, I2C sur core 0).
    xTaskCreatePinnedToCore(touch_task, "touch", 4096, NULL, 5, NULL, 0);

    ESP_LOGI(TAG, "EscapeBox ready — yeux GC9A01 animés, scenario engine actif");

    // Réseau : connexion dans une tâche dédiée (bloque 15 s), puis sync cloud
    // au boot via cloud_client (task propre, pile TLS).
    if (hal_wifi_init() == ESP_OK) {
        if (cloud_client_init() != ESP_OK) {
            ESP_LOGW(TAG, "cloud_client indisponible — pas de sync");
        }
        xTaskCreatePinnedToCore(net_boot_task, "net_boot", 4096, NULL, 4, NULL, 0);
    } else {
        ESP_LOGW(TAG, "WiFi non provisionné — provision_box.py --wifi-ssid …");
    }
}
