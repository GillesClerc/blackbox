#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9488.h"
#include "i2c_bus.h"
#include "audio.h"
#include "mpr121.h"
#include "leds.h"
#include "scenario_engine.h"
#include "config_manager.h"
#include "ui_manager.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "main"

extern const char capitaine_verdier_json_start[] asm("_binary_capitaine_verdier_json_start");
extern const char capitaine_verdier_json_end[]   asm("_binary_capitaine_verdier_json_end");

#ifdef HAS_AMBIENT_MP3
extern const uint8_t _binary_ambient_mp3_start[];
extern const uint8_t _binary_ambient_mp3_end[];
#endif

// ─── Musique de fond (fallback sans MP3) ─────────────────────────────────────

#ifndef HAS_AMBIENT_MP3
static const audio_bg_note_t s_ambient[] = {
    {110, 250, 1800}, {0, 0, 600}, {138, 180, 1200}, {0, 0, 400},
    {123, 200, 2000}, {0, 0, 1000}, {110, 350, 2500}, {0, 0, 1400},
    {147, 200, 1000}, {138, 150, 1500}, {0, 0, 2200},
    {110, 200, 800},  {0, 0, 3000},
};
#endif

// ─── Helpers LED ─────────────────────────────────────────────────────────────

static void led_hex(const char *hex)
{
    if (!hex || hex[0] != '#' || strlen(hex) < 7)
        leds_clear();
    else
        leds_fill_hex(hex, 80);
    leds_show();
}

// ─── Audio ───────────────────────────────────────────────────────────────────

static void play_audio(const char *name)
{
    if (!name) return;

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
        static const uint16_t f[] = {110, 138, 165, 185};
        static const uint16_t d[] = {350, 250, 250, 500};
        audio_play_sequence(f, d, 4, 80);
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
            audio_play_tone(tbl[i].f, tbl[i].d);
            return;
        }
    }
    audio_play_tone(440, 120);
}

// ─── Progression ─────────────────────────────────────────────────────────────

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

static uint32_t accent_for_step(int num)
{
    switch (num) {
        case 1:  return 0x00FF88;
        case 2:  return 0x4488FF;
        case 3:  return 0xFFD700;
        default: return 0x00E5FF;
    }
}

// ─── Callbacks actions scénario ──────────────────────────────────────────────

static void action_screen_main(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    const char *step = scenario_engine_current_step();
    int num, total;
    step_progress(step, &num, &total);

    ui_lock();
    ui_game_update_dots(num, total);
    ui_game_set_accent_color(accent_for_step(num));
    ui_game_set_main_text(text ? text->valuestring : "", true);
    ui_unlock();
}

static void action_screen_secondary(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");

    ui_lock();
    ui_game_set_hint(text ? text->valuestring : "");
    ui_unlock();
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
    const cJSON *color = cJSON_GetObjectItem(params, "color");
    const cJSON *count = cJSON_GetObjectItem(params, "count");
    const char  *hex   = (color && color->valuestring) ? color->valuestring : "#FFD700";
    int          n     = (count && cJSON_IsNumber(count)) ? count->valueint : 4;

    uint32_t c = 0xFFD700;
    if (hex[0] == '#' && strlen(hex) >= 7)
        c = (uint32_t)strtol(hex + 1, NULL, 16);

    leds_fill_hex(hex, 255); leds_show();
    ui_lock();
    ui_game_flash(c, n);
    ui_unlock();
    leds_clear(); leds_show();
}

// ─── Tâche clavier MPR121 ────────────────────────────────────────────────────

#define HOLD_TICKS pdMS_TO_TICKS(2000)

static char s_code[16];
static int  s_code_len = 0;

static void touch_task(void *arg)
{
    (void)arg;
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

        // Feed test screen
        if (ui_current_screen() == UI_SCREEN_TEST) {
            ui_lock();
            ui_test_update_touch(curr.touched);
            ui_unlock();
        }

        // In menu/settings mode, touch only feeds UI (no scenario events)
        if (ui_current_screen() != UI_SCREEN_GAME) {
            prev = curr;
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        TickType_t now = xTaskGetTickCount();

        for (int i = 0; i < MPR121_NUM_CH; i++) {
            bool rose  = curr.ch[i] && !prev.ch[i];
            bool fell  = !curr.ch[i] && prev.ch[i];
            bool held  = curr.ch[i] && !hold_fired[i] && hold_start[i]
                         && (now - hold_start[i]) >= HOLD_TICKS;

            if (rose) { hold_start[i] = now; hold_fired[i] = false; }

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
                        ui_lock();
                        ui_game_update_input(s_code, s_code_len);
                        ui_unlock();
                        audio_play_tone(880 + i * 40, 40);
                    }
                } else if (i == 10) {
                    if (s_code_len > 0) {
                        s_code[--s_code_len] = '\0';
                        ui_lock();
                        ui_game_update_input(s_code, s_code_len);
                        ui_unlock();
                        audio_play_tone(440, 50);
                    }
                } else if (i == 11) {
                    if (s_code_len > 0) {
                        scenario_event_t evt = { .type = EVT_KEYPAD_CODE };
                        strncpy(evt.str, s_code, sizeof(evt.str) - 1);
                        scenario_engine_post_event(&evt);
                        s_code_len = 0; s_code[0] = '\0';
                        ui_lock();
                        ui_game_update_input(NULL, 0);
                        ui_unlock();
                    }
                }
                hold_start[i] = 0;
            }
        }

        prev = curr;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── Launch scenario callback ────────────────────────────────────────────────

static void on_scenario_launch(int index)
{
    (void)index;
    ESP_LOGI(TAG, "Launching scenario %d", index);

    esp_err_t ret = scenario_engine_init(capitaine_verdier_json_start);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "scenario_engine_init: %s", esp_err_to_name(ret));
        return;
    }

    scenario_engine_register_action("screen_main",      action_screen_main);
    scenario_engine_register_action("screen_secondary", action_screen_secondary);
    scenario_engine_register_action("led",              action_led);
    scenario_engine_register_action("audio",            action_audio);
    scenario_engine_register_action("servo",            action_servo);
    scenario_engine_register_action("flash",            action_flash);

    ui_lock();
    ui_show_screen(UI_SCREEN_GAME);
    ui_game_set_title("CAPITAINE VERDIER");
    ui_unlock();

    ret = scenario_engine_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "scenario_engine_start: %s", esp_err_to_name(ret));
        ui_lock();
        ui_game_set_main_text("ERREUR : demarrage scenario", false);
        ui_unlock();
        return;
    }

#ifdef HAS_AMBIENT_MP3
    audio_bg_mp3_start(_binary_ambient_mp3_start,
                       _binary_ambient_mp3_end - _binary_ambient_mp3_start);
#else
    audio_bg_start(s_ambient, sizeof(s_ambient) / sizeof(s_ambient[0]));
#endif
}

// ─── app_main ────────────────────────────────────────────────────────────────

void app_main(void)
{
    // Hardware init
    leds_init(38, 1);
    leds_clear();
    leds_show();
    ESP_ERROR_CHECK(ili9488_init());
    ESP_ERROR_CHECK(config_manager_init());

    // UI init + boot screen
    ESP_ERROR_CHECK(ui_manager_init());
    ui_lock();
    ui_show_screen(UI_SCREEN_BOOT);
    ui_unlock();

    vTaskDelay(pdMS_TO_TICKS(1200));

    // I2C + audio
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(audio_init(i2c_bus_handle()));
    audio_set_volume(config_get_volume());

    // Register scenarios (just one for now)
    static const ui_scenario_card_t cards[] = {
        { .title = "Le Mystere du\nCapitaine Verdier",
          .duration = "45 min",
          .difficulty = "Moyen",
          .index = 0 },
    };
    ui_menu_set_scenarios(cards, 1);
    ui_menu_set_on_launch(on_scenario_launch);

    // Show menu
    ui_lock();
    ui_show_screen(UI_SCREEN_MENU);
    ui_unlock();

    // Touch input task
    xTaskCreate(touch_task, "touch", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "EscapeBox ready — menu on-device");
}
