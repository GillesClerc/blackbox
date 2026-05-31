#include "ui_manager.h"
#include "config_manager.h"
#include "audio.h"
#include "leds.h"
#include "ili9488.h"
#include "xpt2046.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TAG "ui"

// ─── Theme ──────────────────────────────────────────────────────────────────

#define C_BG         0x080C18
#define C_PANEL      0x10182C
#define C_HINT_BG    0x0C1A28
#define C_INPUT_BG   0x0C1220
#define C_HDR_BG     0x0C1024
#define C_GOLD       0xFFD700
#define C_CYAN       0x00E5FF
#define C_TEXT       0xE8E8E8
#define C_TEXT_DIM   0x506070
#define C_HINT_COL   0xFFCC00
#define C_GREEN      0x00FF88
#define C_RED        0xFF4455
#define C_BORDER     0x1C2E4A
#define C_DOT_DONE   0x00E5FF
#define C_DOT_ACTIVE 0xFFD700
#define C_DOT_PEND   0x1A2A3A
#define C_CARD_BG    0x141E30
#define C_CARD_BORD  0x243450
#define C_BTN_BG     0x1A2840
#define C_BTN_PRESS  0x243860

// ─── LVGL internals ─────────────────────────────────────────────────────────

#define LV_BUF_LINES 48
#define LV_BUF_SIZE  (ILI9488_WIDTH * LV_BUF_LINES * sizeof(lv_color_t))

static lv_display_t      *s_disp     = NULL;
static SemaphoreHandle_t  s_lv_mutex = NULL;
static ui_screen_t        s_current  = UI_SCREEN_BOOT;

// ─── Callbacks ──────────────────────────────────────────────────────────────

static ui_on_scenario_launch_cb_t s_on_launch = NULL;
static ui_on_sync_cb_t            s_on_sync   = NULL;

// ─── Scenario cards storage ─────────────────────────────────────────────────

#define MAX_SCENARIOS 8
static ui_scenario_card_t s_scenarios[MAX_SCENARIOS];
static int                s_scenario_count = 0;

// ─── Screen objects ─────────────────────────────────────────────────────────

// Menu
static lv_obj_t *s_menu_scr       = NULL;
static lv_obj_t *s_menu_list      = NULL;

// Game
static lv_obj_t *s_game_scr       = NULL;
static lv_obj_t *s_game_header    = NULL;
static lv_obj_t *s_game_lbl_title = NULL;
static lv_obj_t *s_game_dots[6];
static int       s_game_dot_count = 3;
static lv_obj_t *s_game_main_panel = NULL;
static lv_obj_t *s_game_lbl_main   = NULL;
static lv_obj_t *s_game_hint_panel = NULL;
static lv_obj_t *s_game_lbl_hint_hdr = NULL;
static lv_obj_t *s_game_lbl_hint   = NULL;
static lv_obj_t *s_game_input_bar  = NULL;
static lv_obj_t *s_game_lbl_input  = NULL;
static lv_obj_t *s_game_flash_overlay = NULL;

// Settings
static lv_obj_t *s_settings_scr     = NULL;
static lv_obj_t *s_vol_slider       = NULL;
static lv_obj_t *s_bright_slider    = NULL;
static lv_obj_t *s_lbl_vol_val      = NULL;
static lv_obj_t *s_lbl_bright_val   = NULL;
static lv_obj_t *s_lbl_version      = NULL;

// Test
static lv_obj_t *s_test_scr         = NULL;
static lv_obj_t *s_test_vu_bar      = NULL;
static lv_obj_t *s_test_vol_label   = NULL;

// ─── Typewriter ─────────────────────────────────────────────────────────────

typedef struct {
    lv_timer_t *timer;
    lv_obj_t   *label;
    char        buf[512];
    int         pos;
} tw_t;

static tw_t s_tw_main;
static tw_t s_tw_hint;

static void tw_cb(lv_timer_t *t)
{
    tw_t *tw = lv_timer_get_user_data(t);
    int len = (int)strlen(tw->buf);
    if (tw->pos >= len) {
        lv_timer_delete(t);
        tw->timer = NULL;
        return;
    }
    tw->pos++;
    char save = tw->buf[tw->pos];
    tw->buf[tw->pos] = '\0';
    lv_label_set_text(tw->label, tw->buf);
    tw->buf[tw->pos] = save;
}

static void tw_start(tw_t *tw, lv_obj_t *label, const char *text, uint32_t ms)
{
    if (tw->timer) { lv_timer_delete(tw->timer); tw->timer = NULL; }
    strncpy(tw->buf, text ? text : "", sizeof(tw->buf) - 1);
    tw->buf[sizeof(tw->buf) - 1] = '\0';
    tw->pos   = 0;
    tw->label = label;
    lv_label_set_text(label, "");
    tw->timer = lv_timer_create(tw_cb, ms, tw);
}

static void tw_stop(tw_t *tw)
{
    if (tw->timer) { lv_timer_delete(tw->timer); tw->timer = NULL; }
}

// ─── Animations ─────────────────────────────────────────────────────────────

static void anim_shadow_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_shadow_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_text_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_fade_in(lv_obj_t *obj, uint32_t ms, uint32_t delay)
{
    lv_obj_set_style_text_opa(obj, LV_OPA_TRANSP, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, anim_text_opa_cb);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_start(&a);
}

// ─── LVGL tick / task / lock ────────────────────────────────────────────────

static void lv_tick_cb(void *arg) { (void)arg; lv_tick_inc(1); }

static void lv_task_fn(void *arg)
{
    (void)arg;
    while (1) {
        if (xSemaphoreTakeRecursive(s_lv_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGiveRecursive(s_lv_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ui_lock(void)   { if (s_lv_mutex) xSemaphoreTakeRecursive(s_lv_mutex, portMAX_DELAY); }
void ui_unlock(void) { if (s_lv_mutex) xSemaphoreGiveRecursive(s_lv_mutex); }

// ─── Helper: styled button ──────────────────────────────────────────────────

static lv_obj_t *create_btn(lv_obj_t *parent, const char *text, int w, int h)
{
    lv_obj_t *btn = lv_obj_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(C_BTN_BG), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(C_BTN_PRESS), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(lbl);

    return btn;
}

// ═══════════════════════════════════════════════════════════════════════════
//  BOOT SCREEN  (landscape 480x320)
// ═══════════════════════════════════════════════════════════════════════════

static void create_boot_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Title centered on 480x320
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESCAPEBOX");
    lv_obj_set_style_text_color(title, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_letter_space(title, 4, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -30);

    lv_obj_t *line = lv_obj_create(scr);
    lv_obj_set_size(line, 160, 2);
    lv_obj_set_style_bg_color(line, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_60, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 1, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, 6);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "Initialisation...");
    lv_obj_set_style_text_color(sub, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 28);

    anim_fade_in(title, 500, 0);
    anim_fade_in(sub, 500, 250);
}

// ═══════════════════════════════════════════════════════════════════════════
//  MENU SCREEN  (landscape 480x320)
// ═══════════════════════════════════════════════════════════════════════════

static void menu_card_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    audio_play_tone(880, 40);
    if (s_on_launch) s_on_launch(idx);
}

static void menu_settings_cb(lv_event_t *e)
{
    (void)e;
    audio_play_tone(660, 30);
    ui_show_screen(UI_SCREEN_SETTINGS);
}

static void menu_sync_cb(lv_event_t *e)
{
    (void)e;
    audio_play_tone(660, 30);
    if (s_on_sync) s_on_sync();
}

static void menu_populate_cards(void);

static void create_menu_screen(void)
{
    s_menu_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_menu_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_menu_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_menu_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: full width 480x40
    lv_obj_t *hdr = lv_obj_create(s_menu_scr);
    lv_obj_set_size(hdr, 480, 40);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(C_HDR_BG), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_border_opa(hdr, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    // "ESCAPEBOX" left
    lv_obj_t *lbl_box = lv_label_create(hdr);
    lv_label_set_text(lbl_box, "ESCAPEBOX");
    lv_obj_set_style_text_color(lbl_box, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(lbl_box, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_letter_space(lbl_box, 2, 0);
    lv_obj_align(lbl_box, LV_ALIGN_LEFT_MID, 12, 0);

    // Settings button rightmost
    lv_obj_t *btn_set = create_btn(hdr, LV_SYMBOL_SETTINGS, 36, 32);
    lv_obj_align(btn_set, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_add_event_cb(btn_set, menu_settings_cb, LV_EVENT_CLICKED, NULL);

    // Sync button left of settings
    lv_obj_t *btn_sync = create_btn(hdr, LV_SYMBOL_REFRESH, 36, 32);
    lv_obj_align(btn_sync, LV_ALIGN_RIGHT_MID, -48, 0);
    lv_obj_add_event_cb(btn_sync, menu_sync_cb, LV_EVENT_CLICKED, NULL);

    // Scenario list area: 464x264, below header with 8px side margin
    s_menu_list = lv_obj_create(s_menu_scr);
    lv_obj_set_size(s_menu_list, 464, 264);
    lv_obj_align(s_menu_list, LV_ALIGN_TOP_MID, 0, 46);
    lv_obj_set_style_bg_opa(s_menu_list, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_menu_list, 0, 0);
    lv_obj_set_style_pad_all(s_menu_list, 6, 0);
    lv_obj_set_style_pad_row(s_menu_list, 8, 0);
    lv_obj_set_flex_flow(s_menu_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(s_menu_list, LV_SCROLLBAR_MODE_AUTO);

    menu_populate_cards();
}

static void menu_populate_cards(void)
{
    if (!s_menu_list) return;
    lv_obj_clean(s_menu_list);

    if (s_scenario_count == 0) {
        lv_obj_t *lbl = lv_label_create(s_menu_list);
        lv_label_set_text(lbl, "Aucun scenario installe.\nAppuyez sur " LV_SYMBOL_REFRESH " pour synchroniser.");
        lv_obj_set_style_text_color(lbl, lv_color_hex(C_TEXT_DIM), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(lbl, 440);
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
        return;
    }

    for (int i = 0; i < s_scenario_count; i++) {
        // Cards: 440x70, wider and shorter for landscape
        lv_obj_t *card = lv_obj_create(s_menu_list);
        lv_obj_set_size(card, 440, 70);
        lv_obj_set_style_bg_color(card, lv_color_hex(C_CARD_BG), 0);
        lv_obj_set_style_bg_color(card, lv_color_hex(C_BTN_PRESS), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(card, 10, 0);
        lv_obj_set_style_border_width(card, 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(C_CARD_BORD), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(C_GOLD), LV_STATE_PRESSED);
        lv_obj_set_style_pad_all(card, 10, 0);
        // Subtle shadow
        lv_obj_set_style_shadow_width(card, 8, 0);
        lv_obj_set_style_shadow_opa(card, 40, 0);
        lv_obj_set_style_shadow_color(card, lv_color_black(), 0);
        lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(card, menu_card_cb, LV_EVENT_CLICKED,
                            (void *)(intptr_t)s_scenarios[i].index);

        // Title left
        lv_obj_t *lbl_title = lv_label_create(card);
        lv_label_set_text(lbl_title, s_scenarios[i].title);
        lv_obj_set_style_text_color(lbl_title, lv_color_hex(C_GOLD), 0);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_16, 0);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

        // Info bottom-left
        char info[64];
        snprintf(info, sizeof(info), "%s  |  %s",
                 s_scenarios[i].duration ? s_scenarios[i].duration : "?",
                 s_scenarios[i].difficulty ? s_scenarios[i].difficulty : "?");
        lv_obj_t *lbl_info = lv_label_create(card);
        lv_label_set_text(lbl_info, info);
        lv_obj_set_style_text_color(lbl_info, lv_color_hex(C_TEXT_DIM), 0);
        lv_obj_set_style_text_font(lbl_info, &lv_font_montserrat_14, 0);
        lv_obj_align(lbl_info, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        // Arrow right
        lv_obj_t *arrow = lv_label_create(card);
        lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_color(arrow, lv_color_hex(C_CYAN), 0);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_20, 0);
        lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  GAME SCREEN  (landscape 480x320)
//  Layout:
//    Header 480x38 at top
//    Left: main narrative panel 300x244 at (8, 42)
//    Right: hint panel 156x244 at (312, 42)
//    Input bar 464x30 at bottom (8, 290)
//    Flash overlay 480x320 covering everything
// ═══════════════════════════════════════════════════════════════════════════

static void create_game_screen(void)
{
    s_game_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_game_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_game_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_game_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: 480x38
    s_game_header = lv_obj_create(s_game_scr);
    lv_obj_set_size(s_game_header, 480, 38);
    lv_obj_set_pos(s_game_header, 0, 0);
    lv_obj_set_style_bg_color(s_game_header, lv_color_hex(C_HDR_BG), 0);
    lv_obj_set_style_bg_opa(s_game_header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_game_header, 0, 0);
    lv_obj_set_style_border_side(s_game_header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(s_game_header, 1, 0);
    lv_obj_set_style_border_color(s_game_header, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_border_opa(s_game_header, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(s_game_header, 0, 0);
    lv_obj_remove_flag(s_game_header, LV_OBJ_FLAG_SCROLLABLE);

    s_game_lbl_title = lv_label_create(s_game_header);
    lv_label_set_text(s_game_lbl_title, "");
    lv_obj_set_style_text_color(s_game_lbl_title, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_game_lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_align(s_game_lbl_title, LV_ALIGN_LEFT_MID, 12, 0);

    // Progress dots in header right area
    for (int i = 0; i < 6; i++) {
        s_game_dots[i] = lv_obj_create(s_game_header);
        lv_obj_set_size(s_game_dots[i], 10, 10);
        lv_obj_set_style_radius(s_game_dots[i], 5, 0);
        lv_obj_set_style_bg_color(s_game_dots[i], lv_color_hex(C_DOT_PEND), 0);
        lv_obj_set_style_bg_opa(s_game_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_game_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_game_dots[i], 0, 0);
        lv_obj_align(s_game_dots[i], LV_ALIGN_RIGHT_MID, -12 - (5 - i) * 18, 0);
        lv_obj_add_flag(s_game_dots[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Main narrative panel: 300x244, left side, below header
    s_game_main_panel = lv_obj_create(s_game_scr);
    lv_obj_set_size(s_game_main_panel, 300, 244);
    lv_obj_set_pos(s_game_main_panel, 8, 42);
    lv_obj_set_style_bg_color(s_game_main_panel, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(s_game_main_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_game_main_panel, 10, 0);
    lv_obj_set_style_border_side(s_game_main_panel, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(s_game_main_panel, 3, 0);
    lv_obj_set_style_border_color(s_game_main_panel, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_outline_width(s_game_main_panel, 1, 0);
    lv_obj_set_style_outline_color(s_game_main_panel, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_outline_pad(s_game_main_panel, 0, 0);
    lv_obj_set_style_pad_left(s_game_main_panel, 14, 0);
    lv_obj_set_style_pad_right(s_game_main_panel, 10, 0);
    lv_obj_set_style_pad_top(s_game_main_panel, 10, 0);
    lv_obj_set_style_pad_bottom(s_game_main_panel, 8, 0);
    // Subtle shadow
    lv_obj_set_style_shadow_width(s_game_main_panel, 8, 0);
    lv_obj_set_style_shadow_opa(s_game_main_panel, 40, 0);
    lv_obj_set_style_shadow_color(s_game_main_panel, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(s_game_main_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_game_main_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_game_lbl_main = lv_label_create(s_game_main_panel);
    lv_label_set_long_mode(s_game_lbl_main, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_game_lbl_main, 268);
    lv_label_set_text(s_game_lbl_main, "");
    lv_obj_set_style_text_color(s_game_lbl_main, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(s_game_lbl_main, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_line_space(s_game_lbl_main, 3, 0);
    lv_obj_align(s_game_lbl_main, LV_ALIGN_TOP_LEFT, 0, 0);

    // Hint panel: 156x244, right side, beside main panel
    s_game_hint_panel = lv_obj_create(s_game_scr);
    lv_obj_set_size(s_game_hint_panel, 156, 244);
    lv_obj_set_pos(s_game_hint_panel, 316, 42);
    lv_obj_set_style_bg_color(s_game_hint_panel, lv_color_hex(C_HINT_BG), 0);
    lv_obj_set_style_bg_opa(s_game_hint_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_game_hint_panel, 10, 0);
    lv_obj_set_style_border_width(s_game_hint_panel, 1, 0);
    lv_obj_set_style_border_color(s_game_hint_panel, lv_color_hex(0x1A3040), 0);
    lv_obj_set_style_pad_left(s_game_hint_panel, 10, 0);
    lv_obj_set_style_pad_right(s_game_hint_panel, 8, 0);
    lv_obj_set_style_pad_top(s_game_hint_panel, 8, 0);
    lv_obj_set_style_pad_bottom(s_game_hint_panel, 6, 0);
    // Subtle shadow
    lv_obj_set_style_shadow_width(s_game_hint_panel, 8, 0);
    lv_obj_set_style_shadow_opa(s_game_hint_panel, 40, 0);
    lv_obj_set_style_shadow_color(s_game_hint_panel, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(s_game_hint_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_game_hint_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_game_lbl_hint_hdr = lv_label_create(s_game_hint_panel);
    lv_label_set_text(s_game_lbl_hint_hdr, "En attente...");
    lv_obj_set_style_text_color(s_game_lbl_hint_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_game_lbl_hint_hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(s_game_lbl_hint_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_game_lbl_hint = lv_label_create(s_game_hint_panel);
    lv_label_set_long_mode(s_game_lbl_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_game_lbl_hint, 132);
    lv_label_set_text(s_game_lbl_hint, "");
    lv_obj_set_style_text_color(s_game_lbl_hint, lv_color_hex(C_HINT_COL), 0);
    lv_obj_set_style_text_font(s_game_lbl_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_line_space(s_game_lbl_hint, 2, 0);
    lv_obj_align(s_game_lbl_hint, LV_ALIGN_TOP_LEFT, 0, 20);

    // Input bar: 464x30 at bottom
    s_game_input_bar = lv_obj_create(s_game_scr);
    lv_obj_set_size(s_game_input_bar, 464, 30);
    lv_obj_set_pos(s_game_input_bar, 8, 290);
    lv_obj_set_style_bg_color(s_game_input_bar, lv_color_hex(C_INPUT_BG), 0);
    lv_obj_set_style_bg_opa(s_game_input_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_game_input_bar, 6, 0);
    lv_obj_set_style_border_width(s_game_input_bar, 1, 0);
    lv_obj_set_style_border_color(s_game_input_bar, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(s_game_input_bar, 0, 0);
    lv_obj_set_style_pad_left(s_game_input_bar, 12, 0);
    lv_obj_remove_flag(s_game_input_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_game_lbl_input = lv_label_create(s_game_input_bar);
    lv_label_set_text(s_game_lbl_input, "");
    lv_obj_set_style_text_color(s_game_lbl_input, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_game_lbl_input, &lv_font_montserrat_14, 0);
    lv_obj_align(s_game_lbl_input, LV_ALIGN_LEFT_MID, 0, 0);

    // Flash overlay: 480x320 full screen
    s_game_flash_overlay = lv_obj_create(s_game_scr);
    lv_obj_set_size(s_game_flash_overlay, 480, 320);
    lv_obj_set_pos(s_game_flash_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_game_flash_overlay, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_bg_opa(s_game_flash_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_game_flash_overlay, 0, 0);
    lv_obj_set_style_radius(s_game_flash_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_game_flash_overlay, 0, 0);
    lv_obj_add_flag(s_game_flash_overlay, LV_OBJ_FLAG_HIDDEN);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SETTINGS SCREEN  (landscape 480x320)
//  Layout:
//    Header 480x38
//    Left column (0..239): Volume slider, Brightness slider
//    Right column (240..479): Box info, version
//    Bottom row: Mode Test + Reset usine buttons (full width)
// ═══════════════════════════════════════════════════════════════════════════

static int s_version_tap_count = 0;
static int64_t s_version_tap_start = 0;

static void settings_back_cb(lv_event_t *e)
{
    (void)e;
    audio_play_tone(440, 30);
    ui_show_screen(UI_SCREEN_MENU);
}

static void vol_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    config_set_volume((uint8_t)val);
    audio_set_volume((uint8_t)val);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(s_lbl_vol_val, buf);
}

static void bright_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    config_set_brightness((uint8_t)val);
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", val);
    lv_label_set_text(s_lbl_bright_val, buf);
}

static void version_tap_cb(lv_event_t *e)
{
    (void)e;
    int64_t now = esp_timer_get_time();
    if (s_version_tap_count == 0 || (now - s_version_tap_start) > 3000000) {
        s_version_tap_count = 1;
        s_version_tap_start = now;
    } else {
        s_version_tap_count++;
    }

    if (s_version_tap_count >= 7) {
        s_version_tap_count = 0;
        bool new_val = !config_get_dev_mode();
        config_set_dev_mode(new_val);
        audio_play_tone(new_val ? 1200 : 400, 150);
        ESP_LOGI(TAG, "dev_mode=%d", new_val);

        lv_obj_t *toast = lv_label_create(s_settings_scr);
        lv_label_set_text(toast, new_val ? "Mode dev active" : "Mode dev desactive");
        lv_obj_set_style_text_color(toast, lv_color_hex(new_val ? C_GREEN : C_RED), 0);
        lv_obj_set_style_text_font(toast, &lv_font_montserrat_16, 0);
        lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -20);
        anim_fade_in(toast, 300, 0);
        lv_obj_delete_delayed(toast, 2000);
    }
}

static void reset_confirm_cb(lv_event_t *e);

static void reset_factory_cb(lv_event_t *e)
{
    (void)e;
    audio_play_tone(330, 100);

    lv_obj_t *mbox = lv_msgbox_create(s_settings_scr);
    lv_msgbox_add_title(mbox, "Reset usine");
    lv_msgbox_add_text(mbox, "Effacer WiFi, preferences\net association ?");
    lv_obj_t *btn_ok = lv_msgbox_add_footer_button(mbox, "Confirmer");
    lv_msgbox_add_footer_button(mbox, "Annuler");
    lv_obj_add_event_cb(btn_ok, reset_confirm_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_center(mbox);
}

static void reset_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_user_data(e);
    lv_msgbox_close(mbox);
    ESP_LOGW(TAG, "Factory reset requested");
    config_set_volume(70);
    config_set_brightness(80);
    config_set_dev_mode(false);
    audio_play_tone(220, 500);
}

static void settings_test_cb(lv_event_t *e)
{
    (void)e;
    audio_play_tone(880, 30);
    ui_show_screen(UI_SCREEN_TEST);
}

static void create_settings_screen(void)
{
    s_settings_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_settings_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_settings_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_settings_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: 480x38
    lv_obj_t *hdr = lv_obj_create(s_settings_scr);
    lv_obj_set_size(hdr, 480, 38);
    lv_obj_align(hdr, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(C_HDR_BG), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_border_opa(hdr, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_back = create_btn(hdr, LV_SYMBOL_LEFT, 36, 32);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(btn_back, settings_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_hdr = lv_label_create(hdr);
    lv_label_set_text(lbl_hdr, "Reglages");
    lv_obj_set_style_text_color(lbl_hdr, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(lbl_hdr, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_hdr, LV_ALIGN_CENTER, 0, 0);

    // Content area below header: starts at y=42, height=238 (320-38-44 for bottom buttons)
    // Left column (sliders): x=8, width=230
    // Right column (info): x=244, width=228

    int const col_top   = 46;
    int const col_left_x  = 8;
    int const col_right_x = 244;
    int const col_w     = 228;
    int const row_h     = 24;
    int const slider_w  = 190;

    // ── Left column: Volume ──
    lv_obj_t *lbl_vol = lv_label_create(s_settings_scr);
    lv_label_set_text(lbl_vol, "Volume");
    lv_obj_set_style_text_color(lbl_vol, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(lbl_vol, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_vol, col_left_x, col_top);

    lv_obj_t *vol_row = lv_obj_create(s_settings_scr);
    lv_obj_set_size(vol_row, col_w, row_h + 4);
    lv_obj_set_pos(vol_row, col_left_x, col_top + 20);
    lv_obj_set_style_bg_opa(vol_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(vol_row, 0, 0);
    lv_obj_set_style_pad_all(vol_row, 0, 0);
    lv_obj_remove_flag(vol_row, LV_OBJ_FLAG_SCROLLABLE);

    s_vol_slider = lv_slider_create(vol_row);
    lv_obj_set_size(s_vol_slider, slider_w, 10);
    lv_obj_align(s_vol_slider, LV_ALIGN_LEFT_MID, 0, 0);
    lv_slider_set_range(s_vol_slider, 0, 100);
    lv_slider_set_value(s_vol_slider, config_get_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_vol_slider, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_vol_slider, lv_color_hex(C_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_vol_slider, lv_color_hex(C_GOLD), LV_PART_KNOB);
    lv_obj_add_event_cb(s_vol_slider, vol_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_lbl_vol_val = lv_label_create(vol_row);
    char vbuf[8];
    snprintf(vbuf, sizeof(vbuf), "%d%%", config_get_volume());
    lv_label_set_text(s_lbl_vol_val, vbuf);
    lv_obj_set_style_text_color(s_lbl_vol_val, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_vol_val, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_vol_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // ── Left column: Brightness ──
    lv_obj_t *lbl_bright = lv_label_create(s_settings_scr);
    lv_label_set_text(lbl_bright, "Luminosite");
    lv_obj_set_style_text_color(lbl_bright, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(lbl_bright, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_bright, col_left_x, col_top + 56);

    lv_obj_t *bri_row = lv_obj_create(s_settings_scr);
    lv_obj_set_size(bri_row, col_w, row_h + 4);
    lv_obj_set_pos(bri_row, col_left_x, col_top + 76);
    lv_obj_set_style_bg_opa(bri_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bri_row, 0, 0);
    lv_obj_set_style_pad_all(bri_row, 0, 0);
    lv_obj_remove_flag(bri_row, LV_OBJ_FLAG_SCROLLABLE);

    s_bright_slider = lv_slider_create(bri_row);
    lv_obj_set_size(s_bright_slider, slider_w, 10);
    lv_obj_align(s_bright_slider, LV_ALIGN_LEFT_MID, 0, 0);
    lv_slider_set_range(s_bright_slider, 10, 100);
    lv_slider_set_value(s_bright_slider, config_get_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bright_slider, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bright_slider, lv_color_hex(C_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_bright_slider, lv_color_hex(C_GOLD), LV_PART_KNOB);
    lv_obj_add_event_cb(s_bright_slider, bright_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_lbl_bright_val = lv_label_create(bri_row);
    char bbuf[8];
    snprintf(bbuf, sizeof(bbuf), "%d%%", config_get_brightness());
    lv_label_set_text(s_lbl_bright_val, bbuf);
    lv_obj_set_style_text_color(s_lbl_bright_val, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_bright_val, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_bright_val, LV_ALIGN_RIGHT_MID, 0, 0);

    // Vertical separator between columns
    lv_obj_t *vsep = lv_obj_create(s_settings_scr);
    lv_obj_set_size(vsep, 1, 230);
    lv_obj_set_pos(vsep, 238, col_top);
    lv_obj_set_style_bg_color(vsep, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(vsep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vsep, 0, 0);
    lv_obj_set_style_pad_all(vsep, 0, 0);

    // ── Right column: Box info ──
    lv_obj_t *lbl_info_hdr = lv_label_create(s_settings_scr);
    lv_label_set_text(lbl_info_hdr, "Infos box");
    lv_obj_set_style_text_color(lbl_info_hdr, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(lbl_info_hdr, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_info_hdr, col_right_x, col_top);

    s_lbl_version = lv_label_create(s_settings_scr);
    lv_label_set_text(s_lbl_version, "Firmware: v0.1.0-dev");
    lv_obj_set_style_text_color(s_lbl_version, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_lbl_version, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_lbl_version, col_right_x, col_top + 22);
    lv_obj_add_flag(s_lbl_version, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_lbl_version, version_tap_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_heap = lv_label_create(s_settings_scr);
    char hbuf[48];
    snprintf(hbuf, sizeof(hbuf), "Heap: %lu KB", (unsigned long)(esp_get_free_heap_size() / 1024));
    lv_label_set_text(lbl_heap, hbuf);
    lv_obj_set_style_text_color(lbl_heap, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_heap, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_heap, col_right_x, col_top + 44);

    // Horizontal separator before bottom buttons
    lv_obj_t *hsep = lv_obj_create(s_settings_scr);
    lv_obj_set_size(hsep, 464, 1);
    lv_obj_set_pos(hsep, 8, 276);
    lv_obj_set_style_bg_color(hsep, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(hsep, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(hsep, 0, 0);
    lv_obj_set_style_pad_all(hsep, 0, 0);

    // ── Bottom buttons ──
    int btn_y = 282;
    int btn_h = 32;

    if (config_get_dev_mode()) {
        // Mode Test + Reset side by side
        lv_obj_t *btn_test = create_btn(s_settings_scr, "Mode Test", 224, btn_h);
        lv_obj_set_pos(btn_test, 8, btn_y);
        lv_obj_set_style_border_color(btn_test, lv_color_hex(C_CYAN), 0);
        lv_obj_add_event_cb(btn_test, settings_test_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t *btn_reset = create_btn(s_settings_scr, "Reset usine", 224, btn_h);
        lv_obj_set_pos(btn_reset, 240, btn_y);
        lv_obj_set_style_border_color(btn_reset, lv_color_hex(C_RED), 0);
        lv_obj_add_event_cb(btn_reset, reset_factory_cb, LV_EVENT_CLICKED, NULL);
    } else {
        // Reset only, full width
        lv_obj_t *btn_reset = create_btn(s_settings_scr, "Reset usine", 464, btn_h);
        lv_obj_set_pos(btn_reset, 8, btn_y);
        lv_obj_set_style_border_color(btn_reset, lv_color_hex(C_RED), 0);
        lv_obj_add_event_cb(btn_reset, reset_factory_cb, LV_EVENT_CLICKED, NULL);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  TEST SCREEN  (landscape 480x320) — 3-column dashboard
//
//  Header: 480x38
//  Left col  (x=8,   w=150): Touch, Accel, Lux
//  Center col(x=166, w=160): VU meter bar + audio state label
//  Right col (x=334, w=138): System info, action buttons
// ═══════════════════════════════════════════════════════════════════════════

static lv_obj_t *s_test_lbl_touch   = NULL;
static lv_obj_t *s_test_lbl_accel   = NULL;
static lv_obj_t *s_test_lbl_lux     = NULL;
static lv_obj_t *s_test_lbl_audio   = NULL;
static lv_obj_t *s_test_lbl_system  = NULL;
static lv_timer_t *s_test_timer     = NULL;

static void test_back_cb(lv_event_t *e)
{
    (void)e;
    if (s_test_timer) { lv_timer_delete(s_test_timer); s_test_timer = NULL; }
    audio_play_tone(440, 30);
    ui_show_screen(UI_SCREEN_SETTINGS);
}

static void test_led_cb(lv_event_t *e)
{
    (void)e;
    leds_fill(255, 0, 0); leds_show(); vTaskDelay(pdMS_TO_TICKS(200));
    leds_fill(0, 255, 0); leds_show(); vTaskDelay(pdMS_TO_TICKS(200));
    leds_fill(0, 0, 255); leds_show(); vTaskDelay(pdMS_TO_TICKS(200));
    leds_clear(); leds_show();
}

static void test_tone_cb(lv_event_t *e)
{
    (void)e;
    audio_play_tone(1000, 200);
}

extern const uint8_t _binary_ambient_mp3_start[];
extern const uint8_t _binary_ambient_mp3_end[];

static void test_ambient_cb(lv_event_t *e)
{
    (void)e;
    size_t len = _binary_ambient_mp3_end - _binary_ambient_mp3_start;
    if (len > 0) {
        audio_bg_mp3_start(_binary_ambient_mp3_start, len);
        ESP_LOGI(TAG, "ambient started (%u kB)", (unsigned)(len / 1024));
    }
    if (s_test_lbl_audio) {
        lv_label_set_text(s_test_lbl_audio, "Audio: ambient");
        lv_obj_set_style_text_color(s_test_lbl_audio, lv_color_hex(C_GREEN), 0);
    }
}

static void test_vol_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    audio_set_volume((uint8_t)val);
    config_set_volume((uint8_t)val);
    if (s_test_vol_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", val);
        lv_label_set_text(s_test_vol_label, buf);
    }
}

static const uint8_t s_filter_map[] = { 1, 3, 5, 6, 7 };

static void test_filter_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dd);
    if (sel < sizeof(s_filter_map))
        audio_set_dsp_filter(s_filter_map[sel]);
}

static void test_gain_cb(lv_event_t *e)
{
    lv_obj_t *dd = lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dd);
    audio_set_analog_gain((uint8_t)sel);
}

static void test_refresh_cb(lv_timer_t *t)
{
    (void)t;
    char buf[64];
    snprintf(buf, sizeof(buf), "Heap: %lu KB\nPSRAM: %lu KB",
             (unsigned long)(esp_get_free_heap_size() / 1024),
             (unsigned long)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    if (s_test_lbl_system) lv_label_set_text(s_test_lbl_system, buf);

    ui_test_update_audio_level(audio_get_peak_level());
}

static void create_test_screen(void)
{
    s_test_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_test_scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(s_test_scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_test_scr, LV_OBJ_FLAG_SCROLLABLE);

    // Header: 480x38
    lv_obj_t *hdr = lv_obj_create(s_test_scr);
    lv_obj_set_size(hdr, 480, 38);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(C_HDR_BG), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);
    lv_obj_set_style_border_side(hdr, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(hdr, 1, 0);
    lv_obj_set_style_border_color(hdr, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_border_opa(hdr, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *btn_back = create_btn(hdr, LV_SYMBOL_LEFT, 36, 32);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(btn_back, test_back_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_hdr = lv_label_create(hdr);
    lv_label_set_text(lbl_hdr, "Mode Test");
    lv_obj_set_style_text_color(lbl_hdr, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_text_font(lbl_hdr, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl_hdr, LV_ALIGN_CENTER, 0, 0);

    // Column geometry
    int const col_top  = 44;
    int const col_h    = 270;  // 320 - 38 - 12 margin at bottom

    // Left column panel: x=8, w=150
    lv_obj_t *col_left = lv_obj_create(s_test_scr);
    lv_obj_set_size(col_left, 150, col_h);
    lv_obj_set_pos(col_left, 8, col_top);
    lv_obj_set_style_bg_color(col_left, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(col_left, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(col_left, 10, 0);
    lv_obj_set_style_border_width(col_left, 1, 0);
    lv_obj_set_style_border_color(col_left, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(col_left, 8, 0);
    lv_obj_set_style_pad_row(col_left, 6, 0);
    lv_obj_set_style_shadow_width(col_left, 8, 0);
    lv_obj_set_style_shadow_opa(col_left, 40, 0);
    lv_obj_set_style_shadow_color(col_left, lv_color_black(), 0);
    lv_obj_set_flex_flow(col_left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(col_left, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(col_left, LV_OBJ_FLAG_SCROLLABLE);

    // Touch bitmap label
    lv_obj_t *lbl_touch_hdr = lv_label_create(col_left);
    lv_label_set_text(lbl_touch_hdr, "Touch");
    lv_obj_set_style_text_color(lbl_touch_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_touch_hdr, &lv_font_montserrat_14, 0);

    s_test_lbl_touch = lv_label_create(col_left);
    lv_label_set_text(s_test_lbl_touch, "0x000");
    lv_obj_set_style_text_color(s_test_lbl_touch, lv_color_hex(C_GREEN), 0);
    lv_obj_set_style_text_font(s_test_lbl_touch, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_test_lbl_touch, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_test_lbl_touch, 134);

    // Separator
    lv_obj_t *sep1 = lv_obj_create(col_left);
    lv_obj_set_size(sep1, 134, 1);
    lv_obj_set_style_bg_color(sep1, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(sep1, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep1, 0, 0);
    lv_obj_set_style_pad_all(sep1, 0, 0);

    // Accel label
    lv_obj_t *lbl_accel_hdr = lv_label_create(col_left);
    lv_label_set_text(lbl_accel_hdr, "Accel");
    lv_obj_set_style_text_color(lbl_accel_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_accel_hdr, &lv_font_montserrat_14, 0);

    s_test_lbl_accel = lv_label_create(col_left);
    lv_label_set_text(s_test_lbl_accel, "---");
    lv_obj_set_style_text_color(s_test_lbl_accel, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_text_font(s_test_lbl_accel, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_test_lbl_accel, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_test_lbl_accel, 134);

    // Separator
    lv_obj_t *sep2 = lv_obj_create(col_left);
    lv_obj_set_size(sep2, 134, 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(sep2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep2, 0, 0);
    lv_obj_set_style_pad_all(sep2, 0, 0);

    // Lux label
    lv_obj_t *lbl_lux_hdr = lv_label_create(col_left);
    lv_label_set_text(lbl_lux_hdr, "Lux");
    lv_obj_set_style_text_color(lbl_lux_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_lux_hdr, &lv_font_montserrat_14, 0);

    s_test_lbl_lux = lv_label_create(col_left);
    lv_label_set_text(s_test_lbl_lux, "---");
    lv_obj_set_style_text_color(s_test_lbl_lux, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_test_lbl_lux, &lv_font_montserrat_14, 0);

    // ── Center column: Audio controls — x=166, w=160
    lv_obj_t *col_center = lv_obj_create(s_test_scr);
    lv_obj_set_size(col_center, 160, col_h);
    lv_obj_set_pos(col_center, 166, col_top);
    lv_obj_set_style_bg_color(col_center, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(col_center, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(col_center, 10, 0);
    lv_obj_set_style_border_width(col_center, 1, 0);
    lv_obj_set_style_border_color(col_center, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(col_center, 6, 0);
    lv_obj_set_style_shadow_width(col_center, 8, 0);
    lv_obj_set_style_shadow_opa(col_center, 40, 0);
    lv_obj_set_style_shadow_color(col_center, lv_color_black(), 0);
    lv_obj_remove_flag(col_center, LV_OBJ_FLAG_SCROLLABLE);

    // VU bar: horizontal, compact, top of column
    lv_obj_t *lbl_vu = lv_label_create(col_center);
    lv_label_set_text(lbl_vu, "VU");
    lv_obj_set_style_text_color(lbl_vu, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_vu, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_vu, 0, 0);

    s_test_vu_bar = lv_bar_create(col_center);
    lv_obj_set_size(s_test_vu_bar, 110, 16);
    lv_obj_set_pos(s_test_vu_bar, 30, 2);
    lv_bar_set_range(s_test_vu_bar, 0, 100);
    lv_bar_set_value(s_test_vu_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_test_vu_bar, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_test_vu_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(s_test_vu_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_test_vu_bar, lv_color_hex(C_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_test_vu_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_test_vu_bar, 3, LV_PART_INDICATOR);

    s_test_lbl_audio = lv_label_create(col_center);
    lv_label_set_text(s_test_lbl_audio, "ready");
    lv_obj_set_style_text_color(s_test_lbl_audio, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_test_lbl_audio, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_test_lbl_audio, 0, 22);

    // ── Volume slider
    lv_obj_t *lbl_vol = lv_label_create(col_center);
    lv_label_set_text(lbl_vol, "Volume");
    lv_obj_set_style_text_color(lbl_vol, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_vol, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_vol, 0, 42);

    s_test_vol_label = lv_label_create(col_center);
    lv_obj_set_style_text_color(s_test_vol_label, lv_color_hex(C_GREEN), 0);
    lv_obj_set_style_text_font(s_test_vol_label, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(s_test_vol_label, 110, 42);

    lv_obj_t *sld_vol = lv_slider_create(col_center);
    lv_obj_set_size(sld_vol, 140, 14);
    lv_obj_set_pos(sld_vol, 2, 60);
    lv_slider_set_range(sld_vol, 0, 100);
    lv_slider_set_value(sld_vol, config_get_volume(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sld_vol, lv_color_hex(C_BORDER), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sld_vol, lv_color_hex(C_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sld_vol, lv_color_hex(C_TEXT), LV_PART_KNOB);
    lv_obj_set_style_pad_all(sld_vol, 3, LV_PART_KNOB);
    lv_obj_add_event_cb(sld_vol, test_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);
    {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", config_get_volume());
        lv_label_set_text(s_test_vol_label, buf);
    }

    // ── DSP filter selector
    lv_obj_t *lbl_filt = lv_label_create(col_center);
    lv_label_set_text(lbl_filt, "Filtre DSP");
    lv_obj_set_style_text_color(lbl_filt, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_filt, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_filt, 0, 82);

    lv_obj_t *dd_filt = lv_dropdown_create(col_center);
    lv_dropdown_set_options(dd_filt,
        "1 Low latency IIR\n"
        "3 High atten.\n"
        "5 Apodizing\n"
        "6 Brick-wall\n"
        "7 Ringing-less");
    lv_dropdown_set_selected(dd_filt, 4);
    lv_obj_set_size(dd_filt, 140, 30);
    lv_obj_set_pos(dd_filt, 2, 98);
    lv_obj_set_style_text_font(dd_filt, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dd_filt, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_border_color(dd_filt, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_text_color(dd_filt, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(dd_filt, &lv_font_montserrat_14, LV_PART_INDICATOR);
    lv_obj_add_event_cb(dd_filt, test_filter_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Analog gain selector
    lv_obj_t *lbl_gain = lv_label_create(col_center);
    lv_label_set_text(lbl_gain, "Gain analog.");
    lv_obj_set_style_text_color(lbl_gain, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_gain, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(lbl_gain, 0, 136);

    lv_obj_t *dd_gain = lv_dropdown_create(col_center);
    lv_dropdown_set_options(dd_gain, "0 dB (2Vrms)\n-6 dB (1Vrms)");
    lv_dropdown_set_selected(dd_gain, 0);
    lv_obj_set_size(dd_gain, 140, 30);
    lv_obj_set_pos(dd_gain, 2, 152);
    lv_obj_set_style_text_font(dd_gain, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dd_gain, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_border_color(dd_gain, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_text_color(dd_gain, lv_color_hex(C_TEXT), 0);
    lv_obj_add_event_cb(dd_gain, test_gain_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Right column: system info + buttons — x=334, w=138
    lv_obj_t *col_right = lv_obj_create(s_test_scr);
    lv_obj_set_size(col_right, 138, col_h);
    lv_obj_set_pos(col_right, 334, col_top);
    lv_obj_set_style_bg_color(col_right, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(col_right, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(col_right, 10, 0);
    lv_obj_set_style_border_width(col_right, 1, 0);
    lv_obj_set_style_border_color(col_right, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(col_right, 8, 0);
    lv_obj_set_style_pad_row(col_right, 6, 0);
    lv_obj_set_style_shadow_width(col_right, 8, 0);
    lv_obj_set_style_shadow_opa(col_right, 40, 0);
    lv_obj_set_style_shadow_color(col_right, lv_color_black(), 0);
    lv_obj_set_flex_flow(col_right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scrollbar_mode(col_right, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(col_right, LV_OBJ_FLAG_SCROLLABLE);

    // System info
    lv_obj_t *lbl_sys_hdr = lv_label_create(col_right);
    lv_label_set_text(lbl_sys_hdr, "Systeme");
    lv_obj_set_style_text_color(lbl_sys_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(lbl_sys_hdr, &lv_font_montserrat_14, 0);

    s_test_lbl_system = lv_label_create(col_right);
    lv_label_set_text(s_test_lbl_system, "---");
    lv_obj_set_style_text_color(s_test_lbl_system, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_test_lbl_system, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_test_lbl_system, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_test_lbl_system, 122);

    // Separator
    lv_obj_t *sep_r = lv_obj_create(col_right);
    lv_obj_set_size(sep_r, 122, 1);
    lv_obj_set_style_bg_color(sep_r, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_bg_opa(sep_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sep_r, 0, 0);
    lv_obj_set_style_pad_all(sep_r, 0, 0);

    // Action buttons
    lv_obj_t *btn_led = create_btn(col_right, "Test LED R/V/B", 122, 34);
    lv_obj_add_event_cb(btn_led, test_led_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_tone = create_btn(col_right, "Test Ton 1kHz", 122, 34);
    lv_obj_add_event_cb(btn_tone, test_tone_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_ambient = create_btn(col_right, "Play ambient", 122, 34);
    lv_obj_set_style_border_color(btn_ambient, lv_color_hex(C_GREEN), 0);
    lv_obj_add_event_cb(btn_ambient, test_ambient_cb, LV_EVENT_CLICKED, NULL);

    // Periodic refresh timer
    s_test_timer = lv_timer_create(test_refresh_cb, 100, NULL);
}

// ═══════════════════════════════════════════════════════════════════════════
//  Public: update test screen sensor data (called from sensor tasks)
// ═══════════════════════════════════════════════════════════════════════════

void ui_test_update_touch(uint16_t bitmap)
{
    if (!s_test_lbl_touch || s_current != UI_SCREEN_TEST) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "0x%03X [%c%c%c%c%c%c%c%c%c%c%c%c]",
             bitmap,
             (bitmap & 0x800) ? 'B' : '.', (bitmap & 0x400) ? 'A' : '.',
             (bitmap & 0x200) ? '9' : '.', (bitmap & 0x100) ? '8' : '.',
             (bitmap & 0x080) ? '7' : '.', (bitmap & 0x040) ? '6' : '.',
             (bitmap & 0x020) ? '5' : '.', (bitmap & 0x010) ? '4' : '.',
             (bitmap & 0x008) ? '3' : '.', (bitmap & 0x004) ? '2' : '.',
             (bitmap & 0x002) ? '1' : '.', (bitmap & 0x001) ? '0' : '.');
    lv_label_set_text(s_test_lbl_touch, buf);
}

void ui_test_update_accel(float ax, float ay, float az, float pitch)
{
    if (!s_test_lbl_accel || s_current != UI_SCREEN_TEST) return;
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f %.1f %.1f\ntilt=%.0f",
             (double)ax, (double)ay, (double)az, (double)pitch);
    lv_label_set_text(s_test_lbl_accel, buf);
}

void ui_test_update_lux(float lux)
{
    if (!s_test_lbl_lux || s_current != UI_SCREEN_TEST) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f lx", (double)lux);
    lv_label_set_text(s_test_lbl_lux, buf);
}

void ui_test_update_audio_level(uint8_t level)
{
    if (!s_test_vu_bar || s_current != UI_SCREEN_TEST) return;

    // Clamp to valid range
    if (level > 100) level = 100;

    lv_bar_set_value(s_test_vu_bar, (int32_t)level, LV_ANIM_OFF);

    // Color band: green 0-60, gold 60-85, red 85-100
    uint32_t color;
    if (level <= 60) {
        color = C_GREEN;
    } else if (level <= 85) {
        color = C_GOLD;
    } else {
        color = C_RED;
    }
    lv_obj_set_style_bg_color(s_test_vu_bar, lv_color_hex(color), LV_PART_INDICATOR);
}

// ═══════════════════════════════════════════════════════════════════════════
//  SCREEN NAVIGATION
// ═══════════════════════════════════════════════════════════════════════════

void ui_show_screen(ui_screen_t screen)
{
    ui_lock();
    s_current = screen;

    switch (screen) {
    case UI_SCREEN_BOOT:
        create_boot_screen();
        break;

    case UI_SCREEN_MENU:
        if (!s_menu_scr) create_menu_screen();
        else menu_populate_cards();
        lv_screen_load_anim(s_menu_scr, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
        break;

    case UI_SCREEN_GAME:
        if (!s_game_scr) create_game_screen();
        lv_screen_load_anim(s_game_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        break;

    case UI_SCREEN_SETTINGS:
        if (s_settings_scr) lv_obj_delete(s_settings_scr);
        s_settings_scr = NULL;
        create_settings_screen();
        lv_screen_load_anim(s_settings_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        break;

    case UI_SCREEN_TEST:
        if (s_test_scr) {
            if (s_test_timer) { lv_timer_delete(s_test_timer); s_test_timer = NULL; }
            lv_obj_delete(s_test_scr);
        }
        s_test_scr = NULL;
        s_test_vu_bar = NULL;
        s_test_vol_label = NULL;
        create_test_screen();
        lv_screen_load_anim(s_test_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
        break;
    }

    ui_unlock();
}

ui_screen_t ui_current_screen(void) { return s_current; }

// ═══════════════════════════════════════════════════════════════════════════
//  GAME SCREEN public API
// ═══════════════════════════════════════════════════════════════════════════

void ui_game_set_title(const char *title)
{
    if (!s_game_lbl_title) return;
    lv_label_set_text(s_game_lbl_title, title ? title : "");
}

void ui_game_set_main_text(const char *text, bool typewriter)
{
    if (!s_game_lbl_main) return;
    tw_stop(&s_tw_hint);
    lv_label_set_text(s_game_lbl_hint_hdr, "En attente...");
    lv_obj_set_style_text_color(s_game_lbl_hint_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_label_set_text(s_game_lbl_hint, "");

    if (typewriter)
        tw_start(&s_tw_main, s_game_lbl_main, text, 20);
    else
        lv_label_set_text(s_game_lbl_main, text ? text : "");
}

void ui_game_set_hint(const char *text)
{
    if (!s_game_lbl_hint) return;
    lv_label_set_text(s_game_lbl_hint_hdr, ">> INDICE");
    lv_obj_set_style_text_color(s_game_lbl_hint_hdr, lv_color_hex(C_HINT_COL), 0);
    anim_fade_in(s_game_lbl_hint_hdr, 350, 0);
    tw_start(&s_tw_hint, s_game_lbl_hint, text, 15);
}

void ui_game_update_dots(int current, int total)
{
    if (total > 6) total = 6;
    s_game_dot_count = total;

    for (int i = 0; i < 6; i++) {
        if (i >= total) {
            lv_obj_add_flag(s_game_dots[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(s_game_dots[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_game_dots[i], LV_ALIGN_RIGHT_MID,
                     -12 - (total - 1 - i) * 18, 0);

        lv_anim_delete(s_game_dots[i], anim_shadow_opa_cb);
        uint32_t c;
        bool active = false;
        if      (i + 1 < current)  c = C_DOT_DONE;
        else if (i + 1 == current) { c = C_DOT_ACTIVE; active = true; }
        else                        c = C_DOT_PEND;
        lv_obj_set_style_bg_color(s_game_dots[i], lv_color_hex(c), 0);

        if (active) {
            lv_obj_set_style_shadow_width(s_game_dots[i], 12, 0);
            lv_obj_set_style_shadow_color(s_game_dots[i], lv_color_hex(C_DOT_ACTIVE), 0);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, s_game_dots[i]);
            lv_anim_set_exec_cb(&a, anim_shadow_opa_cb);
            lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
            lv_anim_set_duration(&a, 750);
            lv_anim_set_playback_duration(&a, 750);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_shadow_width(s_game_dots[i], 0, 0);
        }
    }
}

void ui_game_set_accent_color(uint32_t hex_color)
{
    if (s_game_main_panel)
        lv_obj_set_style_border_color(s_game_main_panel, lv_color_hex(hex_color), 0);
}

void ui_game_update_input(const char *code, int len)
{
    if (!s_game_lbl_input) return;
    if (len == 0) {
        lv_label_set_text(s_game_lbl_input, "");
        lv_obj_set_style_text_color(s_game_lbl_input, lv_color_hex(C_TEXT_DIM), 0);
    } else {
        char buf[24];
        snprintf(buf, sizeof(buf), "> %s_", code);
        lv_label_set_text(s_game_lbl_input, buf);
        lv_obj_set_style_text_color(s_game_lbl_input, lv_color_hex(C_GREEN), 0);
    }
}

void ui_game_flash(uint32_t color, int count)
{
    if (!s_game_flash_overlay) return;
    for (int i = 0; i < count; i++) {
        lv_obj_set_style_bg_color(s_game_flash_overlay, lv_color_hex(color), 0);
        lv_obj_remove_flag(s_game_flash_overlay, LV_OBJ_FLAG_HIDDEN);
        ui_unlock();
        vTaskDelay(pdMS_TO_TICKS(100));
        ui_lock();
        lv_obj_add_flag(s_game_flash_overlay, LV_OBJ_FLAG_HIDDEN);
        ui_unlock();
        vTaskDelay(pdMS_TO_TICKS(80));
        ui_lock();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
//  PUBLIC setters
// ═══════════════════════════════════════════════════════════════════════════

void ui_menu_set_scenarios(const ui_scenario_card_t *cards, int count)
{
    if (count > MAX_SCENARIOS) count = MAX_SCENARIOS;
    s_scenario_count = count;
    for (int i = 0; i < count; i++)
        s_scenarios[i] = cards[i];
}

void ui_menu_set_on_launch(ui_on_scenario_launch_cb_t cb) { s_on_launch = cb; }
void ui_menu_set_on_sync(ui_on_sync_cb_t cb)             { s_on_sync = cb; }

// ═══════════════════════════════════════════════════════════════════════════
//  INIT
// ═══════════════════════════════════════════════════════════════════════════

esp_err_t ui_manager_init(void)
{
    lv_init();

    static uint8_t buf1[LV_BUF_SIZE] __attribute__((aligned(4)));
    static uint8_t buf2[LV_BUF_SIZE] __attribute__((aligned(4)));
    s_disp = lv_display_create(ILI9488_WIDTH, ILI9488_HEIGHT);
    lv_display_set_flush_cb(s_disp, (lv_display_flush_cb_t)ili9488_lvgl_flush);
    lv_display_set_buffers(s_disp, buf1, buf2, LV_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t tick_args = { .callback = lv_tick_cb, .name = "lv_tick" };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    s_lv_mutex = xSemaphoreCreateRecursiveMutex();

    // Touch input (XPT2046 on same SPI bus)
    esp_err_t touch_ret = xpt2046_init();
    if (touch_ret == ESP_OK) {
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, (lv_indev_read_cb_t)xpt2046_lvgl_read);
        ESP_LOGI(TAG, "touch input registered");
    } else {
        ESP_LOGW(TAG, "XPT2046 init failed: %s — touch disabled", esp_err_to_name(touch_ret));
    }

    xTaskCreate(lv_task_fn, "lvgl", 8192, NULL, 4, NULL);

    ESP_LOGI(TAG, "ui_manager ready");
    return ESP_OK;
}
