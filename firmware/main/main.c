#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9488.h"
#include "i2c_bus.h"
#include "audio.h"
#include "mpr121.h"
#include "leds.h"
#include "scenario_engine.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "main"

// JSON scénario embarqué (généré par la directive EMBED_TXTFILES du CMakeLists)
extern const char capitaine_verdier_json_start[] asm("_binary_capitaine_verdier_json_start");
extern const char capitaine_verdier_json_end[]   asm("_binary_capitaine_verdier_json_end");

// ─── Layout écran ────────────────────────────────────────────────────────────
//
//  y=0    ┌──────────────────────────────┐
//         │ barre de statut (22px)       │
//  y=22   ├──────────────────────────────┤
//         │                              │
//         │  zone narrative principale   │
//         │  (22..340, scale=2, wrap 26) │
//         │                              │
//  y=340  ├──────────────────────────────┤
//         │  zone secondaire / hint      │
//         │  (340..460, scale=2)         │
//  y=460  ├──────────────────────────────┤
//         │  zone saisie clavier (460..) │
//  y=480  └──────────────────────────────┘

#define Y_STATUS    0
#define H_STATUS   22
#define Y_MAIN     22
#define H_MAIN    318
#define Y_SECONDARY 340
#define H_SECONDARY 120
#define Y_KEYPAD    460
#define H_KEYPAD     20

// ─── LED helpers ─────────────────────────────────────────────────────────────

static void led_set_hex(const char *hex_color)
{
    if (!hex_color || hex_color[0] != '#' || strlen(hex_color) < 7) {
        leds_clear();
    } else {
        // Luminosité réduite pour le dev (LED RGB intégrée très brillante)
        leds_fill_hex(hex_color, 32);
    }
    leds_show();
}

// ─── Affichage helpers ───────────────────────────────────────────────────────

static void draw_status(const char *step_id)
{
    ili9488_fill_rect(0, Y_STATUS, ILI9488_WIDTH, H_STATUS, 0x2945);
    char buf[40];
    snprintf(buf, sizeof(buf), "EscapeBox > %s", step_id ? step_id : "...");
    ili9488_draw_string(4, Y_STATUS + 3, buf, COLOR_CYAN, 0x2945, 1);
}

static void draw_main_text(const char *text)
{
    ili9488_fill_rect(0, Y_MAIN, ILI9488_WIDTH, H_MAIN, COLOR_BLACK);
    if (!text) return;
    // Découpe les '\n' et affiche ligne par ligne (scale=2 → 16px/ligne + 4px)
    char buf[128];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    uint16_t y = Y_MAIN + 8;
    char *line = strtok(buf, "\n");
    while (line && y < Y_SECONDARY - 20) {
        ili9488_draw_string(8, y, line, COLOR_WHITE, COLOR_BLACK, 2);
        y += 22;
        line = strtok(NULL, "\n");
    }
}

static void draw_secondary_text(const char *text)
{
    ili9488_fill_rect(0, Y_SECONDARY, ILI9488_WIDTH, H_SECONDARY, 0x0820);
    if (!text) return;
    char buf[128];
    strncpy(buf, text, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    uint16_t y = Y_SECONDARY + 6;
    char *line = strtok(buf, "\n");
    while (line && y < Y_KEYPAD - 18) {
        ili9488_draw_string(8, y, line, COLOR_YELLOW, 0x0820, 2);
        y += 22;
        line = strtok(NULL, "\n");
    }
}

static void draw_keypad_input(const char *code, int len)
{
    ili9488_fill_rect(0, Y_KEYPAD, ILI9488_WIDTH, H_KEYPAD, COLOR_BLACK);
    if (len == 0) return;
    // Affiche les chiffres saisis sous forme de tirets puis chiffres
    char display[32] = "> ";
    for (int i = 0; i < len && i < 16; i++) {
        char c[2] = { code[i], '\0' };
        strcat(display, c);
    }
    ili9488_draw_string(4, Y_KEYPAD + 2, display, COLOR_GREEN, COLOR_BLACK, 2);
}

// ─── Mapping audio play → fréquence/durée ────────────────────────────────────

typedef struct { const char *name; uint16_t freq; uint16_t dur_ms; } tone_map_t;

static void play_audio(const char *play_name)
{
    static const tone_map_t tones[] = {
        {"intro_ambient",      220, 800},
        {"activation",         523, 200},
        {"correct",            659, 150},
        {"wrong",              147, 400},
        {"victoire",           784, 500},
        {"hint_medallion",     330, 300},
        {"hint_compass",       370, 300},
        {"hint_compass_final", 392, 400},
        {"hint_code",          349, 300},
        {"hint_code_final",    392, 400},
        {"hint_tilt",          415, 300},
        {NULL, 0, 0}
    };

    for (int i = 0; tones[i].name; i++) {
        if (strcmp(play_name, tones[i].name) == 0) {
            audio_play_tone(tones[i].freq, tones[i].dur_ms);
            if (strcmp(play_name, "correct") == 0) {
                vTaskDelay(pdMS_TO_TICKS(80));
                audio_play_tone(784, 150);
                vTaskDelay(pdMS_TO_TICKS(80));
                audio_play_tone(1047, 300);
            } else if (strcmp(play_name, "victoire") == 0) {
                vTaskDelay(pdMS_TO_TICKS(100));
                audio_play_tone(988, 200);
                vTaskDelay(pdMS_TO_TICKS(100));
                audio_play_tone(1175, 500);
            }
            return;
        }
    }
    // Tone générique pour les noms inconnus
    audio_play_tone(440, 150);
}

// ─── Callbacks actions scénario ───────────────────────────────────────────────

static void action_screen_main(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    draw_main_text(text ? text->valuestring : NULL);
    draw_status(scenario_engine_current_step());
}

static void action_screen_secondary(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    draw_secondary_text(text ? text->valuestring : NULL);
}

static void action_led(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *color = cJSON_GetObjectItem(params, "color");
    led_set_hex(color ? color->valuestring : NULL);
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
    // Servo absent Phase 1 — log seulement
    ESP_LOGI(TAG, "servo action (Phase 2)");
}

// ─── Tâche clavier MPR121 ────────────────────────────────────────────────────
//
// Mapping 12 canaux :
//   0-9  → chiffres 0-9
//   10   → backspace  (maintenu 2s → simule rotary_value 270°)
//   11   → confirmer  (maintenu 2s → simule rfid_read "04:VE:RD:01")
//   9    →            (maintenu 2s → simule accel_tilt 15°)

#define HOLD_TICKS   pdMS_TO_TICKS(2000)

static char  s_code[16];
static int   s_code_len = 0;

static void touch_task(void *arg)
{
    esp_err_t ret = mpr121_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MPR121 absent: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "MPR121 prêt");

    mpr121_data_t prev = {0}, curr;
    // hold_start[ch] = tick de début du maintien (0 si non maintenu)
    TickType_t hold_start[MPR121_NUM_CH] = {0};
    bool       hold_fired[MPR121_NUM_CH] = {false};

    while (1) {
        ret = mpr121_read(&curr);
        if (ret != ESP_OK) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < MPR121_NUM_CH; i++) {
            if (curr.ch[i] && !prev.ch[i]) {
                // Front montant
                hold_start[i] = now;
                hold_fired[i] = false;
            } else if (!curr.ch[i] && prev.ch[i]) {
                // Front descendant — action si pas de hold
                if (!hold_fired[i]) {
                    if (i >= 0 && i <= 9) {
                        // Chiffre
                        if (s_code_len < (int)(sizeof(s_code) - 1)) {
                            s_code[s_code_len++] = '0' + i;
                            s_code[s_code_len] = '\0';
                            draw_keypad_input(s_code, s_code_len);
                            audio_play_tone(880 + i * 50, 40);
                        }
                    } else if (i == 10) {
                        // Backspace
                        if (s_code_len > 0) {
                            s_code[--s_code_len] = '\0';
                            draw_keypad_input(s_code, s_code_len);
                            audio_play_tone(400, 60);
                        }
                    } else if (i == 11) {
                        // Confirmer → poster keypad_code
                        if (s_code_len > 0) {
                            scenario_event_t evt = { .type = EVT_KEYPAD_CODE };
                            strncpy(evt.str, s_code, sizeof(evt.str) - 1);
                            scenario_engine_post_event(&evt);
                            s_code_len = 0;
                            s_code[0] = '\0';
                            draw_keypad_input(s_code, 0);
                        }
                    }
                }
                hold_start[i] = 0;
            } else if (curr.ch[i] && !hold_fired[i]) {
                // Maintenu — vérifier durée
                if (hold_start[i] && (now - hold_start[i]) >= HOLD_TICKS) {
                    hold_fired[i] = true;
                    audio_play_tone(1200, 120);

                    scenario_event_t evt = {0};
                    if (i == 11) {
                        // Simuler rfid_read
                        evt.type = EVT_RFID_READ;
                        strncpy(evt.str, "04:VE:RD:01", sizeof(evt.str) - 1);
                        ESP_LOGI(TAG, "SIM rfid_read 04:VE:RD:01");
                    } else if (i == 10) {
                        // Simuler rotary_value 270
                        evt.type = EVT_ROTARY_VALUE;
                        evt.int_val = 270;
                        ESP_LOGI(TAG, "SIM rotary_value 270");
                    } else if (i == 9) {
                        // Simuler accel_tilt 15°
                        evt.type = EVT_ACCEL_TILT;
                        evt.int_val = 15;
                        ESP_LOGI(TAG, "SIM accel_tilt 15");
                    }
                    if (evt.type != EVT_NONE) scenario_engine_post_event(&evt);
                }
            }
        }

        prev = curr;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── app_main ────────────────────────────────────────────────────────────────

void app_main(void)
{
    // LED onboard off
    leds_init(38, 1);
    leds_clear();
    leds_show();

    // Display
    ESP_ERROR_CHECK(ili9488_init());
    ili9488_fill(COLOR_BLACK);
    draw_status("boot");
    draw_main_text("EscapeBox S3\n\nChargement...");
    vTaskDelay(pdMS_TO_TICKS(300));

    // I2C + audio
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(audio_init(i2c_bus_handle()));

    // Scénario — init d'abord (reset les handlers), register ensuite
    esp_err_t ret = scenario_engine_init(capitaine_verdier_json_start);
    if (ret != ESP_OK) {
        draw_main_text("scenario_engine_init\nFAIL");
        ESP_LOGE(TAG, "scenario_engine_init: %s", esp_err_to_name(ret));
        return;
    }

    scenario_engine_register_action("screen_main",      action_screen_main);
    scenario_engine_register_action("screen_secondary", action_screen_secondary);
    scenario_engine_register_action("led",              action_led);
    scenario_engine_register_action("audio",            action_audio);
    scenario_engine_register_action("servo",            action_servo);

    ESP_ERROR_CHECK(scenario_engine_start());
    ESP_LOGI(TAG, "Scénario démarré");

    // Tâche touch (après le moteur pour éviter les events prématurés)
    xTaskCreate(touch_task, "touch", 4096, NULL, 5, NULL);
}
