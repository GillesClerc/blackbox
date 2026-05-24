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

extern const char    capitaine_verdier_json_start[] asm("_binary_capitaine_verdier_json_start");
extern const char    capitaine_verdier_json_end[]   asm("_binary_capitaine_verdier_json_end");

// ambient.mp3 embarqué uniquement si le fichier existe au moment du build
// (voir firmware/main/CMakeLists.txt). Sinon ces symboles sont NULL (weak).
extern const uint8_t _binary_ambient_mp3_start[] __attribute__((weak));
extern const uint8_t _binary_ambient_mp3_end[]   __attribute__((weak));

// ─── Layout écran ────────────────────────────────────────────────────────────
//
//  y=0   ┌────────────────────────────────┐
//        │ HEADER  30px (titre + étape)   │
//  y=30  ├── sep 2px ─────────────────────┤
//  y=32  │                                │
//        │  ZONE MAIN  298px              │
//        │  (texte narratif + typewriter) │
//  y=330 ├── sep 2px ─────────────────────┤
//  y=332 │  ZONE HINT  118px              │
//        │  (indices, feedback)           │
//  y=450 ├── sep 2px ─────────────────────┤
//  y=452 │  ZONE CLAVIER  28px            │
//  y=480 └────────────────────────────────┘

#define Y_HDR      0
#define H_HDR     30
#define Y_SEP1    30
#define Y_MAIN    32
#define H_MAIN   298
#define Y_SEP2   330
#define Y_HINT   332
#define H_HINT   118
#define Y_SEP3   450
#define Y_KEY    452
#define H_KEY     28

#define COL_HDR_BG    0x000C  // bleu nuit très sombre
#define COL_MAIN_BG   0x0000  // noir
#define COL_HINT_BG   0x0820  // teal foncé
#define COL_KEY_BG    0x0000
#define COL_BORDER    0x07FF  // cyan

// ─── Musique de fond : ambiance sous-marine ───────────────────────────────────

static const audio_bg_note_t s_ambient[] = {
    {110, 250, 1800},  // A2 - grave profond
    {  0,   0,  600},
    {138, 180, 1200},  // C#3
    {  0,   0,  400},
    {123, 200, 2000},  // B2
    {  0,   0, 1000},
    {110, 350, 2500},  // A2 long
    {  0,   0, 1400},
    {147, 200, 1000},  // D3
    {138, 150, 1500},  // C#3
    {  0,   0, 2200},  // longue pause
    {110, 200,  800},
    {  0,   0, 3000},  // silence étendu
};

// ─── Helpers display ─────────────────────────────────────────────────────────

// Dessine le texte caractère par caractère avec une pause entre chaque.
// Gère '\n' et le retour à la ligne automatique sur la largeur.
static void typewriter(uint16_t x0, uint16_t y0, uint16_t y_max,
                       const char *text, uint16_t fg, uint16_t bg,
                       uint8_t scale, uint16_t delay_ms)
{
    if (!text) return;
    uint16_t cw  = scale * 6;
    uint16_t lh  = scale * 8 + 4;
    uint16_t x   = x0;
    uint16_t y   = y0;
    for (const char *p = text; *p && y + lh <= y_max; p++) {
        if (*p == '\n') {
            x = x0; y += lh; continue;
        }
        if (x + cw > ILI9488_WIDTH - 4) {
            x = x0; y += lh;
            if (y + lh > y_max) break;
        }
        ili9488_draw_char(x, y, *p, fg, bg, scale);
        x += cw;
        if (delay_ms) vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static void draw_separator(uint16_t y)
{
    ili9488_fill_rect(0, y, ILI9488_WIDTH, 2, COL_BORDER);
}

// En-tête : titre du scénario + indicateur d'étape (ex. "2/3")
static void draw_header(int step_num, int step_total)
{
    ili9488_fill_rect(0, Y_HDR, ILI9488_WIDTH, H_HDR, COL_HDR_BG);
    ili9488_draw_string(8, 9, "CAPITAINE VERDIER", COLOR_GOLD, COL_HDR_BG, 1);
    if (step_num > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d / %d", step_num, step_total);
        uint16_t bw = (uint16_t)(strlen(buf) * 6);
        ili9488_draw_string(ILI9488_WIDTH - bw - 8, 9, buf, COLOR_CYAN, COL_HDR_BG, 1);
    }
    draw_separator(Y_SEP1);
}

static void draw_main_text(const char *text, uint16_t border_color)
{
    ili9488_fill_rect(0, Y_MAIN, ILI9488_WIDTH, H_MAIN, COL_MAIN_BG);
    // Bordure colorée (2px gauche + droite)
    ili9488_fill_rect(0,                   Y_MAIN, 2, H_MAIN, border_color);
    ili9488_fill_rect(ILI9488_WIDTH - 2,   Y_MAIN, 2, H_MAIN, border_color);
    draw_separator(Y_SEP2);
    typewriter(8, Y_MAIN + 10, Y_SEP2 - 4, text, COLOR_WHITE, COL_MAIN_BG, 2, 18);
}

static void clear_hint(void)
{
    ili9488_fill_rect(0, Y_HINT, ILI9488_WIDTH, H_HINT, COL_HINT_BG);
    draw_separator(Y_SEP3);
    // Label permanent discret
    ili9488_draw_string(8, Y_HINT + 6, ">> ATTENTE INDICE...", 0x39E7, COL_HINT_BG, 1);
}

static void draw_hint_text(const char *text)
{
    ili9488_fill_rect(0, Y_HINT, ILI9488_WIDTH, H_HINT, COL_HINT_BG);
    draw_separator(Y_SEP3);
    ili9488_draw_string(8, Y_HINT + 4, ">> INDICE :", COLOR_YELLOW, COL_HINT_BG, 1);
    typewriter(8, Y_HINT + 18, Y_SEP3 - 2, text, COLOR_YELLOW, COL_HINT_BG, 2, 12);
}

static void draw_input(const char *code, int len)
{
    ili9488_fill_rect(0, Y_KEY, ILI9488_WIDTH, H_KEY, COL_KEY_BG);
    if (len == 0) {
        ili9488_draw_string(8, Y_KEY + 6, "[ entrez le code... ]", 0x39E7, COL_KEY_BG, 1);
    } else {
        char buf[20] = "> ";
        strncat(buf, code, sizeof(buf) - 3);
        strcat(buf, "_");
        ili9488_draw_string(8, Y_KEY + 4, buf, COLOR_GREEN, COL_KEY_BG, 2);
    }
}

// ─── Helpers LED ─────────────────────────────────────────────────────────────

static void led_hex(const char *hex)
{
    if (!hex || hex[0] != '#' || strlen(hex) < 7)
        leds_clear();
    else
        leds_fill_hex(hex, 80);  // brightness 80/255 ≈ 31%
    leds_show();
}

// ─── Mapping audio play → tonalités ──────────────────────────────────────────

static void play_audio(const char *name)
{
    if (!name) return;

    // Séquences multi-notes (correct, wrong, victoire)
    if (strcmp(name, "correct") == 0) {
        static const uint16_t f[] = {659, 784, 1047};
        static const uint16_t d[] = {130, 130,  280};
        audio_play_sequence(f, d, 3, 30);
        return;
    }
    if (strcmp(name, "wrong") == 0) {
        static const uint16_t f[] = {220, 185};
        static const uint16_t d[] = {200, 350};
        audio_play_sequence(f, d, 2, 20);
        return;
    }
    if (strcmp(name, "victoire") == 0) {
        static const uint16_t f[] = {523, 659, 784, 1047, 1319};
        static const uint16_t d[] = {120, 120, 120,  120,  500};
        audio_play_sequence(f, d, 5, 25);
        return;
    }
    if (strcmp(name, "activation") == 0) {
        static const uint16_t f[] = {440, 554, 659};
        static const uint16_t d[] = {120, 120, 250};
        audio_play_sequence(f, d, 3, 20);
        return;
    }
    if (strcmp(name, "intro_ambient") == 0) {
        // Courte mélodie d'intro avant la boucle bg
        static const uint16_t f[] = {110, 138, 165, 185};
        static const uint16_t d[] = {350, 250, 250, 500};
        audio_play_sequence(f, d, 4, 80);
        return;
    }

    // Notes simples
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
            audio_play_tone(tbl[i].f, tbl[i].d);
            return;
        }
    }
    audio_play_tone(440, 120); // fallback
}

// ─── Étape courante → numéro (pour l'en-tête) ────────────────────────────────

static void step_progress(const char *id, int *num, int *total)
{
    *total = 3;
    if (!id)                               { *num = 0; return; }
    if (strstr(id, "medallion") ||
        strstr(id, "boussole")  ||
        strstr(id, "compass"))             { *num = 1; return; }
    if (strstr(id, "code"))                { *num = 2; return; }
    if (strstr(id, "inclinais"))           { *num = 3; return; }
    if (strstr(id, "epilogue") ||
        strstr(id, "end"))                 { *num = 3; return; }
    *num = 0;
}

// Couleur de bordure selon l'étape
static uint16_t border_for_step(int num)
{
    switch (num) {
        case 1:  return 0x07E0; // vert
        case 2:  return 0x001F; // bleu
        case 3:  return COLOR_GOLD;
        default: return COL_BORDER; // cyan
    }
}

// ─── Callbacks actions scénario ───────────────────────────────────────────────

static void action_screen_main(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");

    const char *step = scenario_engine_current_step();
    int num, total;
    step_progress(step, &num, &total);

    draw_header(num, total);
    clear_hint();           // effacer les hints de l'étape précédente
    draw_main_text(text ? text->valuestring : "", border_for_step(num));
}

static void action_screen_secondary(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    draw_hint_text(text ? text->valuestring : "");
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

static void action_flash(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *color  = cJSON_GetObjectItem(params, "color");
    const cJSON *count  = cJSON_GetObjectItem(params, "count");
    const char  *hex    = (color && color->valuestring) ? color->valuestring : "#FFD700";
    int          n      = (count && cJSON_IsNumber(count)) ? count->valueint : 4;

    // Flash LED + écran simultanément
    for (int i = 0; i < n; i++) {
        leds_fill_hex(hex, 255); leds_show();
        ili9488_fill(COLOR_GOLD);
        vTaskDelay(pdMS_TO_TICKS(100));
        leds_clear(); leds_show();
        ili9488_fill(COLOR_BLACK);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    // Rétablir l'en-tête après le flash
    const char *step = scenario_engine_current_step();
    int num, total;
    step_progress(step, &num, &total);
    draw_header(num, total);
}

// ─── Tâche clavier MPR121 ────────────────────────────────────────────────────
//
// Canaux :  0-9  → chiffres
//           10   → backspace  (hold 2s = simule rotary_value 270)
//           11   → confirmer  (hold 2s = simule rfid_read "04:VE:RD:01")
//           9    →            (hold 2s = simule accel_tilt 15°)

#define HOLD_TICKS pdMS_TO_TICKS(2000)

static char s_code[16];
static int  s_code_len = 0;

static void touch_task(void *arg)
{
    esp_err_t ret = mpr121_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "MPR121 absent: %s", esp_err_to_name(ret));
        vTaskDelete(NULL);
        return;
    }

    mpr121_data_t prev = {0}, curr;
    TickType_t hold_start[MPR121_NUM_CH] = {0};
    bool       hold_fired[MPR121_NUM_CH] = {false};

    while (1) {
        if (mpr121_read(&curr) != ESP_OK) { vTaskDelay(pdMS_TO_TICKS(50)); continue; }

        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < MPR121_NUM_CH; i++) {
            bool rose  = curr.ch[i] && !prev.ch[i];
            bool fell  = !curr.ch[i] && prev.ch[i];
            bool held  = curr.ch[i] && !hold_fired[i] && hold_start[i]
                         && (now - hold_start[i]) >= HOLD_TICKS;

            if (rose)  { hold_start[i] = now; hold_fired[i] = false; }

            if (held) {
                hold_fired[i] = true;
                audio_play_tone(1200, 100);
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
                        draw_input(s_code, s_code_len);
                        audio_play_tone(880 + i * 40, 40);
                    }
                } else if (i == 10) {
                    if (s_code_len > 0) {
                        s_code[--s_code_len] = '\0';
                        draw_input(s_code, s_code_len);
                        audio_play_tone(440, 50);
                    }
                } else if (i == 11) {
                    if (s_code_len > 0) {
                        scenario_event_t evt = { .type = EVT_KEYPAD_CODE };
                        strncpy(evt.str, s_code, sizeof(evt.str) - 1);
                        scenario_engine_post_event(&evt);
                        s_code_len = 0; s_code[0] = '\0';
                        draw_input(NULL, 0);
                    }
                }
                hold_start[i] = 0;
            }
        }

        prev = curr;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── app_main ────────────────────────────────────────────────────────────────

void app_main(void)
{
    // LED off
    leds_init(38, 1);
    leds_clear();
    leds_show();

    // Display : boot screen
    ESP_ERROR_CHECK(ili9488_init());
    ili9488_fill(COL_HDR_BG);
    ili9488_fill_rect(0, H_HDR, ILI9488_WIDTH, ILI9488_HEIGHT - H_HDR, COL_MAIN_BG);
    draw_separator(Y_SEP1);
    ili9488_draw_string(8, 9, "ESCAPEBOX", COLOR_GOLD, COL_HDR_BG, 1);
    ili9488_draw_string(8, 60, "Chargement...", COLOR_CYAN, COL_MAIN_BG, 2);
    vTaskDelay(pdMS_TO_TICKS(400));

    // I2C + audio
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(audio_init(i2c_bus_handle()));

    // Scénario
    esp_err_t ret = scenario_engine_init(capitaine_verdier_json_start);
    if (ret != ESP_OK) {
        ili9488_draw_string(8, 90, "SCENARIO FAIL", COLOR_RED, COL_MAIN_BG, 2);
        ESP_LOGE(TAG, "scenario_engine_init: %s", esp_err_to_name(ret));
        return;
    }

    scenario_engine_register_action("screen_main",      action_screen_main);
    scenario_engine_register_action("screen_secondary", action_screen_secondary);
    scenario_engine_register_action("led",              action_led);
    scenario_engine_register_action("audio",            action_audio);
    scenario_engine_register_action("servo",            action_servo);
    scenario_engine_register_action("flash",            action_flash);

    ESP_ERROR_CHECK(scenario_engine_start());

    // Musique de fond : MP3 embarqué si dispo, sinon tons synthétiques
    if (_binary_ambient_mp3_start) {
        audio_bg_mp3_start(_binary_ambient_mp3_start,
                           _binary_ambient_mp3_end - _binary_ambient_mp3_start);
    } else {
        audio_bg_start(s_ambient, sizeof(s_ambient) / sizeof(s_ambient[0]));
    }

    // Clavier
    xTaskCreate(touch_task, "touch", 4096, NULL, 5, NULL);
}
