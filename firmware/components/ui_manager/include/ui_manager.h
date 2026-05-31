#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef enum {
    UI_SCREEN_BOOT,
    UI_SCREEN_MENU,
    UI_SCREEN_GAME,
    UI_SCREEN_SETTINGS,
    UI_SCREEN_TEST,
} ui_screen_t;

typedef struct {
    const char *title;
    const char *duration;
    const char *difficulty;
    int         index;
} ui_scenario_card_t;

typedef void (*ui_on_scenario_launch_cb_t)(int index);
typedef void (*ui_on_sync_cb_t)(void);

esp_err_t ui_manager_init(void);

void ui_show_screen(ui_screen_t screen);
ui_screen_t ui_current_screen(void);

void ui_lock(void);
void ui_unlock(void);

void ui_menu_set_scenarios(const ui_scenario_card_t *cards, int count);
void ui_menu_set_on_launch(ui_on_scenario_launch_cb_t cb);
void ui_menu_set_on_sync(ui_on_sync_cb_t cb);

// Game screen — called by scenario action callbacks
void ui_game_set_title(const char *title);
void ui_game_set_main_text(const char *text, bool typewriter);
void ui_game_set_hint(const char *text);
void ui_game_update_dots(int current, int total);
void ui_game_set_accent_color(uint32_t hex_color);
void ui_game_update_input(const char *code, int len);
void ui_game_flash(uint32_t color, int count);

// Test screen — called from sensor tasks (only updates if test screen active)
void ui_test_update_touch(uint16_t bitmap);
void ui_test_update_accel(float ax, float ay, float az, float pitch);
void ui_test_update_lux(float lux);
void ui_test_update_audio_level(uint8_t level);
