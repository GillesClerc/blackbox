#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ili9488.h"
#include "i2c_bus.h"
#include "audio.h"
#include "mpr121.h"
#include "leds.h"
#include "scenario_engine.h"
#include "cJSON.h"
#include "lvgl.h"
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

// ─── Thème visuel ────────────────────────────────────────────────────────────

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

// ─── LVGL ────────────────────────────────────────────────────────────────────

#define LV_BUF_LINES  48
#define LV_BUF_SIZE   (ILI9488_WIDTH * LV_BUF_LINES * sizeof(lv_color_t))

static lv_display_t      *s_lv_disp  = NULL;
static SemaphoreHandle_t  s_lv_mutex = NULL;

static lv_obj_t *s_header;
static lv_obj_t *s_lbl_title;
static lv_obj_t *s_dots[3];
static lv_obj_t *s_main_panel;
static lv_obj_t *s_lbl_main;
static lv_obj_t *s_hint_panel;
static lv_obj_t *s_lbl_hint_hdr;
static lv_obj_t *s_lbl_hint;
static lv_obj_t *s_input_bar;
static lv_obj_t *s_lbl_input;
static lv_obj_t *s_flash_overlay;

// ─── Typewriter (effet machine à écrire via timer LVGL) ─────────────────────

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

// ─── Musique de fond (fallback sans MP3) ─────────────────────────────────────

#ifndef HAS_AMBIENT_MP3
static const audio_bg_note_t s_ambient[] = {
    {110, 250, 1800}, {0, 0, 600}, {138, 180, 1200}, {0, 0, 400},
    {123, 200, 2000}, {0, 0, 1000}, {110, 350, 2500}, {0, 0, 1400},
    {147, 200, 1000}, {138, 150, 1500}, {0, 0, 2200},
    {110, 200, 800},  {0, 0, 3000},
};
#endif

// ─── LVGL tick / task / lock ─────────────────────────────────────────────────

static void lv_tick_cb(void *arg) { (void)arg; lv_tick_inc(1); }

static void lv_task_fn(void *arg)
{
    (void)arg;
    while (1) {
        if (xSemaphoreTake(s_lv_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            lv_timer_handler();
            xSemaphoreGive(s_lv_mutex);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void ui_lock(void)   { if (s_lv_mutex) xSemaphoreTake(s_lv_mutex, portMAX_DELAY); }
void ui_unlock(void) { if (s_lv_mutex) xSemaphoreGive(s_lv_mutex); }

// ─── Animations (callbacks d'exécution lv_anim) ──────────────────────────────

static void anim_shadow_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_shadow_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

static void anim_text_opa_cb(void *obj, int32_t v)
{
    lv_obj_set_style_text_opa((lv_obj_t *)obj, (lv_opa_t)v, 0);
}

// Fondu d'apparition du texte d'un label (0 → opaque)
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

// ─── UI helpers ──────────────────────────────────────────────────────────────

static void ui_update_dots(int step_num)
{
    for (int i = 0; i < 3; i++) {
        lv_anim_delete(s_dots[i], anim_shadow_opa_cb);  // stoppe une éventuelle pulsation
        uint32_t c;
        bool active = false;
        if      (i + 1 < step_num)  c = C_DOT_DONE;
        else if (i + 1 == step_num) { c = C_DOT_ACTIVE; active = true; }
        else                         c = C_DOT_PEND;
        lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(c), 0);

        if (active) {
            lv_obj_set_style_shadow_width(s_dots[i], 12, 0);
            lv_obj_set_style_shadow_color(s_dots[i], lv_color_hex(C_DOT_ACTIVE), 0);
            // Halo qui pulse en continu tant que l'énigme est active
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, s_dots[i]);
            lv_anim_set_exec_cb(&a, anim_shadow_opa_cb);
            lv_anim_set_values(&a, LV_OPA_20, LV_OPA_COVER);
            lv_anim_set_duration(&a, 750);
            lv_anim_set_playback_duration(&a, 750);
            lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_shadow_width(s_dots[i], 0, 0);
        }
    }
}

static uint32_t accent_for_step(int num)
{
    switch (num) {
        case 1:  return 0x00FF88;
        case 2:  return 0x4488FF;
        case 3:  return C_GOLD;
        default: return C_CYAN;
    }
}

static void ui_update_input(const char *code, int len)
{
    if (!s_lbl_input) return;
    if (len == 0) {
        lv_label_set_text(s_lbl_input, "");
        lv_obj_set_style_text_color(s_lbl_input, lv_color_hex(C_TEXT_DIM), 0);
    } else {
        char buf[24];
        snprintf(buf, sizeof(buf), "> %s_", code);
        lv_label_set_text(s_lbl_input, buf);
        lv_obj_set_style_text_color(s_lbl_input, lv_color_hex(C_GREEN), 0);
    }
}

// ─── UI : écran de jeu ──────────────────────────────────────────────────────

static void ui_create_game(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Header ──────────────────────────────────────────────────────────
    s_header = lv_obj_create(scr);
    lv_obj_set_size(s_header, 320, 46);
    lv_obj_align(s_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(s_header, lv_color_hex(C_HDR_BG), 0);
    lv_obj_set_style_bg_opa(s_header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_header, 0, 0);
    lv_obj_set_style_border_side(s_header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(s_header, 1, 0);
    lv_obj_set_style_border_color(s_header, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_border_opa(s_header, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(s_header, 0, 0);
    lv_obj_remove_flag(s_header, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_title = lv_label_create(s_header);
    lv_label_set_text(s_lbl_title, "CAPITAINE VERDIER");
    lv_obj_set_style_text_color(s_lbl_title, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_lbl_title, &lv_font_montserrat_16, 0);
    lv_obj_align(s_lbl_title, LV_ALIGN_LEFT_MID, 12, 0);

    for (int i = 0; i < 3; i++) {
        s_dots[i] = lv_obj_create(s_header);
        lv_obj_set_size(s_dots[i], 10, 10);
        lv_obj_set_style_radius(s_dots[i], 5, 0);
        lv_obj_set_style_bg_color(s_dots[i], lv_color_hex(C_DOT_PEND), 0);
        lv_obj_set_style_bg_opa(s_dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_dots[i], 0, 0);
        lv_obj_set_style_pad_all(s_dots[i], 0, 0);
        lv_obj_align(s_dots[i], LV_ALIGN_RIGHT_MID, -12 - (2 - i) * 18, 0);
    }

    // ── Panneau narratif principal ──────────────────────────────────────
    s_main_panel = lv_obj_create(scr);
    lv_obj_set_size(s_main_panel, 304, 286);
    lv_obj_align(s_main_panel, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(s_main_panel, lv_color_hex(C_PANEL), 0);
    lv_obj_set_style_bg_opa(s_main_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_main_panel, 8, 0);
    lv_obj_set_style_border_side(s_main_panel, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(s_main_panel, 3, 0);
    lv_obj_set_style_border_color(s_main_panel, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_outline_width(s_main_panel, 1, 0);
    lv_obj_set_style_outline_color(s_main_panel, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_outline_pad(s_main_panel, 0, 0);
    lv_obj_set_style_pad_left(s_main_panel, 14, 0);
    lv_obj_set_style_pad_right(s_main_panel, 12, 0);
    lv_obj_set_style_pad_top(s_main_panel, 12, 0);
    lv_obj_set_style_pad_bottom(s_main_panel, 8, 0);
    lv_obj_set_scrollbar_mode(s_main_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_main_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_main = lv_label_create(s_main_panel);
    lv_label_set_long_mode(s_lbl_main, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_main, 274);
    lv_label_set_text(s_lbl_main, "");
    lv_obj_set_style_text_color(s_lbl_main, lv_color_hex(C_TEXT), 0);
    lv_obj_set_style_text_font(s_lbl_main, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_line_space(s_lbl_main, 3, 0);
    lv_obj_align(s_lbl_main, LV_ALIGN_TOP_LEFT, 0, 0);

    // ── Panneau indices ─────────────────────────────────────────────────
    s_hint_panel = lv_obj_create(scr);
    lv_obj_set_size(s_hint_panel, 304, 96);
    lv_obj_align(s_hint_panel, LV_ALIGN_TOP_MID, 0, 340);
    lv_obj_set_style_bg_color(s_hint_panel, lv_color_hex(C_HINT_BG), 0);
    lv_obj_set_style_bg_opa(s_hint_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_hint_panel, 8, 0);
    lv_obj_set_style_border_width(s_hint_panel, 1, 0);
    lv_obj_set_style_border_color(s_hint_panel, lv_color_hex(0x1A3040), 0);
    lv_obj_set_style_pad_left(s_hint_panel, 12, 0);
    lv_obj_set_style_pad_right(s_hint_panel, 12, 0);
    lv_obj_set_style_pad_top(s_hint_panel, 8, 0);
    lv_obj_set_style_pad_bottom(s_hint_panel, 6, 0);
    lv_obj_set_scrollbar_mode(s_hint_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(s_hint_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_hint_hdr = lv_label_create(s_hint_panel);
    lv_label_set_text(s_lbl_hint_hdr, "En attente...");
    lv_obj_set_style_text_color(s_lbl_hint_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_lbl_hint_hdr, &lv_font_montserrat_14, 0);
    lv_obj_align(s_lbl_hint_hdr, LV_ALIGN_TOP_LEFT, 0, 0);

    s_lbl_hint = lv_label_create(s_hint_panel);
    lv_label_set_long_mode(s_lbl_hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_lbl_hint, 276);
    lv_label_set_text(s_lbl_hint, "");
    lv_obj_set_style_text_color(s_lbl_hint, lv_color_hex(C_HINT_COL), 0);
    lv_obj_set_style_text_font(s_lbl_hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_line_space(s_lbl_hint, 2, 0);
    lv_obj_align(s_lbl_hint, LV_ALIGN_TOP_LEFT, 0, 18);

    // ── Barre de saisie clavier ─────────────────────────────────────────
    s_input_bar = lv_obj_create(scr);
    lv_obj_set_size(s_input_bar, 304, 36);
    lv_obj_align(s_input_bar, LV_ALIGN_TOP_MID, 0, 440);
    lv_obj_set_style_bg_color(s_input_bar, lv_color_hex(C_INPUT_BG), 0);
    lv_obj_set_style_bg_opa(s_input_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_input_bar, 6, 0);
    lv_obj_set_style_border_width(s_input_bar, 1, 0);
    lv_obj_set_style_border_color(s_input_bar, lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_pad_all(s_input_bar, 0, 0);
    lv_obj_set_style_pad_left(s_input_bar, 12, 0);
    lv_obj_remove_flag(s_input_bar, LV_OBJ_FLAG_SCROLLABLE);

    s_lbl_input = lv_label_create(s_input_bar);
    lv_label_set_text(s_lbl_input, "");
    lv_obj_set_style_text_color(s_lbl_input, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(s_lbl_input, &lv_font_montserrat_16, 0);
    lv_obj_align(s_lbl_input, LV_ALIGN_LEFT_MID, 0, 0);

    // ── Flash overlay (caché par défaut, affiché par-dessus tout) ────────
    s_flash_overlay = lv_obj_create(scr);
    lv_obj_set_size(s_flash_overlay, 320, 480);
    lv_obj_set_pos(s_flash_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_flash_overlay, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_bg_opa(s_flash_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_flash_overlay, 0, 0);
    lv_obj_set_style_radius(s_flash_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_flash_overlay, 0, 0);
    lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
}

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

// ─── Callbacks actions scénario ──────────────────────────────────────────────

static void action_screen_main(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");
    const char *step = scenario_engine_current_step();
    int num, total;
    step_progress(step, &num, &total);

    ui_lock();
    ui_update_dots(num);
    lv_obj_set_style_border_color(s_main_panel, lv_color_hex(accent_for_step(num)), 0);

    tw_stop(&s_tw_hint);
    lv_label_set_text(s_lbl_hint_hdr, "En attente...");
    lv_obj_set_style_text_color(s_lbl_hint_hdr, lv_color_hex(C_TEXT_DIM), 0);
    lv_label_set_text(s_lbl_hint, "");

    tw_start(&s_tw_main, s_lbl_main, text ? text->valuestring : "", 20);
    ui_unlock();
}

static void action_screen_secondary(const char *name, const cJSON *params)
{
    (void)name;
    const cJSON *text = cJSON_GetObjectItem(params, "text");

    ui_lock();
    lv_label_set_text(s_lbl_hint_hdr, ">> INDICE");
    lv_obj_set_style_text_color(s_lbl_hint_hdr, lv_color_hex(C_HINT_COL), 0);
    anim_fade_in(s_lbl_hint_hdr, 350, 0);
    tw_start(&s_tw_hint, s_lbl_hint, text ? text->valuestring : "", 15);
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
    const cJSON *color  = cJSON_GetObjectItem(params, "color");
    const cJSON *count  = cJSON_GetObjectItem(params, "count");
    const char  *hex    = (color && color->valuestring) ? color->valuestring : "#FFD700";
    int          n      = (count && cJSON_IsNumber(count)) ? count->valueint : 4;

    uint32_t c = C_GOLD;
    if (hex[0] == '#' && strlen(hex) >= 7)
        c = (uint32_t)strtol(hex + 1, NULL, 16);

    for (int i = 0; i < n; i++) {
        leds_fill_hex(hex, 255); leds_show();
        ui_lock();
        lv_obj_set_style_bg_color(s_flash_overlay, lv_color_hex(c), 0);
        lv_obj_remove_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
        ui_unlock();
        vTaskDelay(pdMS_TO_TICKS(100));

        leds_clear(); leds_show();
        ui_lock();
        lv_obj_add_flag(s_flash_overlay, LV_OBJ_FLAG_HIDDEN);
        ui_unlock();
        vTaskDelay(pdMS_TO_TICKS(80));
    }
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
                        ui_update_input(s_code, s_code_len);
                        ui_unlock();
                        audio_play_tone(880 + i * 40, 40);
                    }
                } else if (i == 10) {
                    if (s_code_len > 0) {
                        s_code[--s_code_len] = '\0';
                        ui_lock();
                        ui_update_input(s_code, s_code_len);
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
                        ui_update_input(NULL, 0);
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

// ─── app_main ────────────────────────────────────────────────────────────────

void app_main(void)
{
    leds_init(38, 1);
    leds_clear();
    leds_show();

    ESP_ERROR_CHECK(ili9488_init());

    // LVGL
    lv_init();
    static uint8_t buf1[LV_BUF_SIZE] __attribute__((aligned(4)));
    static uint8_t buf2[LV_BUF_SIZE] __attribute__((aligned(4)));
    s_lv_disp = lv_display_create(ILI9488_WIDTH, ILI9488_HEIGHT);
    lv_display_set_flush_cb(s_lv_disp, (lv_display_flush_cb_t)ili9488_lvgl_flush);
    lv_display_set_buffers(s_lv_disp, buf1, buf2, LV_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t tick_args = { .callback = lv_tick_cb, .name = "lv_tick" };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));

    s_lv_mutex = xSemaphoreCreateMutex();

    // ── Boot screen ─────────────────────────────────────────────────────
    ui_lock();
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *boot_title = lv_label_create(scr);
    lv_label_set_text(boot_title, "ESCAPEBOX");
    lv_obj_set_style_text_color(boot_title, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(boot_title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_letter_space(boot_title, 4, 0);
    lv_obj_align(boot_title, LV_ALIGN_CENTER, 0, -50);

    lv_obj_t *boot_line = lv_obj_create(scr);
    lv_obj_set_size(boot_line, 120, 2);
    lv_obj_set_style_bg_color(boot_line, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_bg_opa(boot_line, LV_OPA_60, 0);
    lv_obj_set_style_border_width(boot_line, 0, 0);
    lv_obj_set_style_radius(boot_line, 1, 0);
    lv_obj_set_style_pad_all(boot_line, 0, 0);
    lv_obj_align(boot_line, LV_ALIGN_CENTER, 0, -18);

    lv_obj_t *boot_sub = lv_label_create(scr);
    lv_label_set_text(boot_sub, "Le Mystere du\nCapitaine Verdier");
    lv_obj_set_style_text_color(boot_sub, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_text_font(boot_sub, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(boot_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(boot_sub, LV_ALIGN_CENTER, 0, 14);

    lv_obj_t *boot_load = lv_label_create(scr);
    lv_label_set_text(boot_load, "Initialisation...");
    lv_obj_set_style_text_color(boot_load, lv_color_hex(C_TEXT_DIM), 0);
    lv_obj_set_style_text_font(boot_load, &lv_font_montserrat_14, 0);
    lv_obj_align(boot_load, LV_ALIGN_CENTER, 0, 70);

    anim_fade_in(boot_title, 500, 0);
    anim_fade_in(boot_sub, 500, 250);
    anim_fade_in(boot_load, 400, 550);
    ui_unlock();

    xTaskCreate(lv_task_fn, "lvgl", 8192, NULL, 4, NULL);
    vTaskDelay(pdMS_TO_TICKS(1100));

    // I2C + audio
    ESP_ERROR_CHECK(i2c_bus_init());
    ESP_ERROR_CHECK(audio_init(i2c_bus_handle()));

    // Scénario
    esp_err_t ret = scenario_engine_init(capitaine_verdier_json_start);
    if (ret != ESP_OK) {
        ui_lock();
        lv_label_set_text(boot_sub, "ERREUR SCENARIO");
        lv_obj_set_style_text_color(boot_sub, lv_color_hex(C_RED), 0);
        ui_unlock();
        ESP_LOGE(TAG, "scenario_engine_init: %s", esp_err_to_name(ret));
        return;
    }

    // Transition boot → jeu
    ui_lock();
    lv_obj_delete(boot_title);
    lv_obj_delete(boot_line);
    lv_obj_delete(boot_sub);
    lv_obj_delete(boot_load);
    ui_create_game();
    ui_unlock();

    scenario_engine_register_action("screen_main",      action_screen_main);
    scenario_engine_register_action("screen_secondary", action_screen_secondary);
    scenario_engine_register_action("led",              action_led);
    scenario_engine_register_action("audio",            action_audio);
    scenario_engine_register_action("servo",            action_servo);
    scenario_engine_register_action("flash",            action_flash);

    ESP_ERROR_CHECK(scenario_engine_start());

#ifdef HAS_AMBIENT_MP3
    audio_bg_mp3_start(_binary_ambient_mp3_start,
                       _binary_ambient_mp3_end - _binary_ambient_mp3_start);
#else
    audio_bg_start(s_ambient, sizeof(s_ambient) / sizeof(s_ambient[0]));
#endif

    xTaskCreate(touch_task, "touch", 4096, NULL, 5, NULL);
}
