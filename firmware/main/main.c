#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7735.h"
#include "scenario_engine.h"

#define TAG "main"

// Scénario Verdier embarqué via EMBED_TXTFILES dans CMakeLists
extern const uint8_t _binary_capitaine_verdier_json_start[];
#define VERDIER_SCENARIO ((const char *)_binary_capitaine_verdier_json_start)

// --- Boot screen ---

static void draw_boot_screen(void) {
    display_fill(COLOR_BLACK);

    display_fill_rect(0, 0, DISPLAY_WIDTH, 2, COLOR_CYAN);
    display_draw_string(10, 6, "ESCAPEBOX", COLOR_GOLD, COLOR_BLACK, 2);
    display_draw_string(22, 24, "v0.1-alpha", COLOR_CYAN, COLOR_BLACK, 1);
    display_fill_rect(0, 33, DISPLAY_WIDTH, 1, COLOR_CYAN);

    display_draw_string(4, 40, "En chantier...", COLOR_YELLOW, COLOR_BLACK, 1);
    display_draw_string(4, 54, "Claude code", COLOR_WHITE, COLOR_BLACK, 1);
    display_draw_string(4, 63, "construit", COLOR_WHITE, COLOR_BLACK, 1);
    display_draw_string(4, 72, "quelque chose.", COLOR_WHITE, COLOR_BLACK, 1);

    display_fill_rect(0, 84, DISPLAY_WIDTH, 1, COLOR_GRAY);
    display_draw_string(4, 90,  "Revenez quand", COLOR_GRAY, COLOR_BLACK, 1);
    display_draw_string(4, 99,  "le hardware", COLOR_GRAY, COLOR_BLACK, 1);
    display_draw_string(4, 108, "est arrive :)", COLOR_GRAY, COLOR_BLACK, 1);

    display_fill_rect(4, 120, 120, 10, COLOR_GRAY);
    display_fill_rect(0, DISPLAY_HEIGHT - 2, DISPLAY_WIDTH, 2, COLOR_CYAN);
}

static void boot_screen_task(void *arg) {
    static const char *status_msgs[] = {
        "compiling...   ",
        "linking...     ",
        "downloading... ",
        "debugging...   ",
        "caffeinating...",
        "hallucinating..",
        "reticulating...",
        "optimizing...  ",
    };
    static const uint16_t title_colors[] = {
        COLOR_GOLD, COLOR_ORANGE, COLOR_YELLOW, COLOR_WHITE, COLOR_YELLOW, COLOR_ORANGE,
    };

    uint8_t progress  = 0;
    uint8_t tick      = 0;
    uint8_t msg_idx   = 0;
    uint8_t color_idx = 0;

    while (1) {
        uint8_t bar_w = (progress * 118) / 100;
        display_fill_rect(5, 121, bar_w,             8, COLOR_GREEN);
        display_fill_rect(5 + bar_w, 121, 118 - bar_w, 8, 0x2104);

        char pct[8];
        snprintf(pct, sizeof(pct), " %3d%% ", progress);
        display_draw_string(4, 133, pct, COLOR_GREEN, COLOR_BLACK, 1);

        if (tick % 12 == 0) {
            display_draw_string(4, 148, status_msgs[msg_idx % 8], COLOR_GRAY, COLOR_BLACK, 1);
            msg_idx++;
        }

        if (tick % 8 == 0) {
            display_draw_string(10, 6, "ESCAPEBOX",
                                title_colors[color_idx % 6], COLOR_BLACK, 2);
            color_idx++;
        }

        progress++;
        if (progress > 100) progress = 0;
        tick++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- Handlers d'actions (mocks Phase 1 — log + affichage) ---

static void action_screen_main(const char *name, const cJSON *params) {
    const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(params, "text"));
    if (text) ESP_LOGI(TAG, "[screen_main] %s", text);
}

static void action_screen_secondary(const char *name, const cJSON *params) {
    const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(params, "text"));
    if (text) ESP_LOGI(TAG, "[screen_sec ] %s", text);
}

static void action_audio(const char *name, const cJSON *params) {
    const char *play = cJSON_GetStringValue(cJSON_GetObjectItem(params, "play"));
    ESP_LOGI(TAG, "[audio] play: %s", play ? play : "?");
}

static void action_led(const char *name, const cJSON *params) {
    const char *target = cJSON_GetStringValue(cJSON_GetObjectItem(params, "target"));
    const char *color  = cJSON_GetStringValue(cJSON_GetObjectItem(params, "color"));
    const char *mode   = cJSON_GetStringValue(cJSON_GetObjectItem(params, "mode"));
    ESP_LOGI(TAG, "[led] target=%s color=%s mode=%s",
             target ? target : "?", color ? color : "?", mode ? mode : "?");
}

static void action_servo(const char *name, const cJSON *params) {
    const char *id     = cJSON_GetStringValue(cJSON_GetObjectItem(params, "id"));
    const char *action = cJSON_GetStringValue(cJSON_GetObjectItem(params, "action"));
    ESP_LOGI(TAG, "[servo] id=%s action=%s", id ? id : "?", action ? action : "?");
}

// --- Simulateur d'événements — scénario Capitaine Verdier ---

static void simulator_task(void *arg) {
    ESP_LOGI(TAG, "simulateur démarré — injection dans 4s...");
    vTaskDelay(pdMS_TO_TICKS(4000));

    scenario_event_t evt;

    // Épreuve 0 : médaillon RFID
    ESP_LOGI(TAG, "SIM → rfid '04:VE:RD:01'");
    evt.type = EVT_RFID_READ;
    snprintf(evt.str, sizeof(evt.str), "04:VE:RD:01");
    scenario_engine_post_event(&evt);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Épreuve 1 : boussole — mauvais cap d'abord
    ESP_LOGI(TAG, "SIM → rotary 90 (mauvais)");
    evt.type = EVT_ROTARY_VALUE;
    evt.int_val = 90;
    scenario_engine_post_event(&evt);
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Bon cap : 270° (plein Ouest)
    ESP_LOGI(TAG, "SIM → rotary 270 (correct)");
    evt.int_val = 270;
    scenario_engine_post_event(&evt);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Épreuve 2 : code — mauvais d'abord
    ESP_LOGI(TAG, "SIM → keypad '1234' (mauvais)");
    evt.type = EVT_KEYPAD_CODE;
    snprintf(evt.str, sizeof(evt.str), "1234");
    scenario_engine_post_event(&evt);
    vTaskDelay(pdMS_TO_TICKS(1500));

    // Bon code : 7394 (date du naufrage inversée)
    ESP_LOGI(TAG, "SIM → keypad '7394' (correct)");
    snprintf(evt.str, sizeof(evt.str), "7394");
    scenario_engine_post_event(&evt);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Épreuve 3 : inclinaison 15°
    ESP_LOGI(TAG, "SIM → accel_tilt 15 (correct)");
    evt.type = EVT_ACCEL_TILT;
    evt.int_val = 15;
    scenario_engine_post_event(&evt);

    vTaskDelete(NULL);
}

// --- Main ---

void app_main(void) {
    ESP_LOGI(TAG, "=== EscapeBox démarrage ===");

    display_init();
    draw_boot_screen();
    xTaskCreate(boot_screen_task, "boot_anim", 4096, NULL, 4, NULL);

    ESP_ERROR_CHECK(scenario_engine_init(VERDIER_SCENARIO));
    scenario_engine_register_action("screen_main",      action_screen_main);
    scenario_engine_register_action("screen_secondary", action_screen_secondary);
    scenario_engine_register_action("audio",            action_audio);
    scenario_engine_register_action("led",              action_led);
    scenario_engine_register_action("servo",            action_servo);
    ESP_ERROR_CHECK(scenario_engine_start());

    xTaskCreate(simulator_task, "simulator", 4096, NULL, 3, NULL);
}
