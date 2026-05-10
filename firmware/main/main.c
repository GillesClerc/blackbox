#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7735.h"
#include "scenario_engine.h"

#define TAG "main"

// Scénario de test Phase 1 — embarqué en dur, pas de SD
static const char *TEST_SCENARIO = R"({
  "meta": {
    "id": "test_phase1",
    "title": "Test Phase 1 — Sans hardware",
    "version": "1.0"
  },
  "steps": [
    {
      "id": "intro",
      "type": "narrative",
      "do": [
        {"screen_main": {"text": "Bienvenue dans EscapeBox"}},
        {"led": {"target": "all", "color": "#0000FF", "mode": "pulse"}}
      ],
      "next": "attente_rfid"
    },
    {
      "id": "attente_rfid",
      "type": "trigger",
      "on": "rfid_read",
      "expect": {"uid": "04:AB:CD:EF"},
      "timeout_sec": 30,
      "do": [
        {"audio": {"play": "intro"}},
        {"screen_main": {"text": "ÉPREUVE 1 — Entrez le code"}},
        {"led": {"target": "edges", "color": "#FF8800", "mode": "solid"}}
      ],
      "next": "code_secret"
    },
    {
      "id": "code_secret",
      "type": "input",
      "on": "keypad_code",
      "expect": {"code": "1743"},
      "timeout_sec": 120,
      "do_success": [
        {"servo": {"id": "main", "action": "open"}},
        {"audio": {"play": "victoire"}},
        {"led": {"target": "all", "color": "#00FF00", "mode": "flash"}}
      ],
      "next": "epilogue"
    },
    {
      "id": "epilogue",
      "type": "narrative",
      "do": [
        {"screen_main": {"text": "Bravo ! Temps : 3m47s"}},
        {"led": {"target": "all", "color": "#FFD700", "mode": "cycle"}}
      ],
      "next": "end"
    },
    {
      "id": "end",
      "type": "end",
      "do": [
        {"screen_main": {"text": "À bientôt !"}}
      ]
    }
  ]
})";

// --- Handlers d'actions (mocks Phase 1 — logguent et affichent sur l'écran) ---

static void action_screen_main(const char *name, const cJSON *params) {
    const char *text = cJSON_GetStringValue(cJSON_GetObjectItem(params, "text"));
    if (text) {
        ESP_LOGI(TAG, "[screen] %s", text);
        // TODO: afficher sur le vrai écran avec LVGL
    }
}

static void action_audio(const char *name, const cJSON *params) {
    const char *play = cJSON_GetStringValue(cJSON_GetObjectItem(params, "play"));
    ESP_LOGI(TAG, "[audio] play: %s", play ? play : "?");
    // TODO: lecture MP3 via I2S
}

static void action_led(const char *name, const cJSON *params) {
    const char *target = cJSON_GetStringValue(cJSON_GetObjectItem(params, "target"));
    const char *color  = cJSON_GetStringValue(cJSON_GetObjectItem(params, "color"));
    const char *mode   = cJSON_GetStringValue(cJSON_GetObjectItem(params, "mode"));
    ESP_LOGI(TAG, "[led] target=%s color=%s mode=%s",
             target ? target : "?",
             color  ? color  : "?",
             mode   ? mode   : "?");
    // TODO: WS2812 via RMT
}

static void action_servo(const char *name, const cJSON *params) {
    const char *id     = cJSON_GetStringValue(cJSON_GetObjectItem(params, "id"));
    const char *action = cJSON_GetStringValue(cJSON_GetObjectItem(params, "action"));
    ESP_LOGI(TAG, "[servo] id=%s action=%s", id ? id : "?", action ? action : "?");
    // TODO: SG90 via MCPWM
}

// --- Simulateur d'événements (remplace les vrais capteurs en Phase 1) ---

static void simulator_task(void *arg) {
    ESP_LOGI(TAG, "simulateur démarré — inject events dans 3s...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    // Simule un badge NFC
    ESP_LOGI(TAG, "SIM → rfid 04:AB:CD:EF");
    scenario_event_t evt = { .type = EVT_RFID_READ };
    snprintf(evt.str, sizeof(evt.str), "04:AB:CD:EF");
    scenario_engine_post_event(&evt);

    vTaskDelay(pdMS_TO_TICKS(3000));

    // Simule un mauvais code
    ESP_LOGI(TAG, "SIM → keypad '0000' (mauvais)");
    evt.type = EVT_KEYPAD_CODE;
    snprintf(evt.str, sizeof(evt.str), "0000");
    scenario_engine_post_event(&evt);

    vTaskDelay(pdMS_TO_TICKS(2000));

    // Simule le bon code
    ESP_LOGI(TAG, "SIM → keypad '1743' (correct)");
    snprintf(evt.str, sizeof(evt.str), "1743");
    scenario_engine_post_event(&evt);

    vTaskDelete(NULL);
}

// --- Main ---

void app_main(void) {
    ESP_LOGI(TAG, "Démarrage EscapeBox");

    // Display
    display_init();
    display_fill(COLOR_BLACK);

    // Moteur de scénario
    ESP_ERROR_CHECK(scenario_engine_init(TEST_SCENARIO));
    scenario_engine_register_action("screen_main", action_screen_main);
    scenario_engine_register_action("audio",       action_audio);
    scenario_engine_register_action("led",         action_led);
    scenario_engine_register_action("servo",       action_servo);
    ESP_ERROR_CHECK(scenario_engine_start());

    // Simulateur d'events pour tester sans hardware
    xTaskCreate(simulator_task, "simulator", 4096, NULL, 3, NULL);
}
