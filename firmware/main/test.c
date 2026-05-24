// Hardware test harness — pour utiliser ce fichier :
//   dans CMakeLists.txt, remplacer "main.c" par "test.c" dans SRCS
// Teste séquentiellement : LED RGB, display ILI9488, scan I2C, audio, touch MPR121.

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "ili9488.h"
#include "i2c_bus.h"
#include "audio.h"
#include "mpr121.h"
#include "leds.h"
#include <stdio.h>
#include <string.h>

#define TAG "test"

// ─── Affichage ──────────────────────────────────────────────────────────────

static uint16_t s_y = 4;

static void scr(const char *msg, uint16_t color)
{
    ili9488_fill_rect(0, s_y, ILI9488_WIDTH, 18, COLOR_BLACK);
    ili9488_draw_string(4, s_y, msg, color, COLOR_BLACK, 2);
    s_y += 20;
    ESP_LOGI(TAG, "%s", msg);
}
static void ok(const char *m)   { scr(m, COLOR_GREEN); }
static void err(const char *m)  { scr(m, COLOR_RED);   }
static void info(const char *m) { scr(m, COLOR_CYAN);  }

// ─── Test 1 : LED RGB ────────────────────────────────────────────────────────

static void test_led(void)
{
    info("LED RGB (GPIO38)...");
    leds_init(38, 1);
    static const struct { uint8_t r, g, b; const char *name; } colors[] = {
        {8, 0, 0, "LED rouge"},
        {0, 8, 0, "LED vert"},
        {0, 0, 8, "LED bleu"},
        {0, 0, 0, "LED off"},
    };
    for (int i = 0; i < 4; i++) {
        leds_fill(colors[i].r, colors[i].g, colors[i].b);
        leds_show();
        info(colors[i].name);
        vTaskDelay(pdMS_TO_TICKS(600));
    }
    ok("LED RGB OK");
}

// ─── Test 2 : display ────────────────────────────────────────────────────────

static void test_display(void)
{
    info("Display barres couleur...");
    uint16_t stripe_h = ILI9488_HEIGHT / 7;
    static const uint16_t palette[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_ORANGE, COLOR_WHITE
    };
    for (int i = 0; i < 7; i++) {
        ili9488_fill_rect(0, i * stripe_h, ILI9488_WIDTH, stripe_h, palette[i]);
    }
    vTaskDelay(pdMS_TO_TICKS(1500));
    ili9488_fill(COLOR_BLACK);
    s_y = 4;
    ok("Display OK");
}

// ─── Test 3 : scan I2C ──────────────────────────────────────────────────────

static bool s_mpr_found = false;

static void test_i2c_scan(void)
{
    char line[40];
    snprintf(line, sizeof(line), "I2C SDA=%d SCL=%d 100kHz", I2C_BUS_SDA, I2C_BUS_SCL);
    info(line);

    i2c_master_bus_handle_t bus = i2c_bus_handle();
    int found = 0;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, 20) == ESP_OK) {
            found++;
            if (addr == 0x5A) {
                ok("0x5A MPR121 OK");
                s_mpr_found = true;
            } else {
                snprintf(line, sizeof(line), "0x%02X trouve", addr);
                info(line);
            }
        }
    }

    if (!s_mpr_found) {
        esp_err_t r = i2c_master_probe(bus, 0x5A, 50);
        snprintf(line, sizeof(line), "0x5A: %s", esp_err_to_name(r));
        err(line);
    }
    if (found == 0) {
        err("0 device I2C");
    }
}

// ─── Test 4 : audio ─────────────────────────────────────────────────────────

static void test_audio(void)
{
    info("Audio I2S...");
    esp_err_t ret = audio_init(i2c_bus_handle());
    if (ret != ESP_OK) { err("Audio init FAIL"); return; }

    static const uint16_t gamme[] = { 262, 294, 330, 349, 392, 440, 494, 523, 0 };
    for (int i = 0; gamme[i]; i++) {
        audio_play_tone(gamme[i], 180);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    audio_play_tone(880, 300);
    ok("Audio OK");
}

// ─── Test 5 : touch MPR121 ──────────────────────────────────────────────────

#define GRID_COLS  4
#define GRID_ROWS  3
#define CELL_W     (ILI9488_WIDTH / GRID_COLS)

static uint16_t s_grid_top;

static void draw_cell(int ch, bool touched)
{
    uint16_t cell_h = (ILI9488_HEIGHT - s_grid_top) / GRID_ROWS;
    int col = ch % GRID_COLS;
    int row = ch / GRID_COLS;
    uint16_t x = col * CELL_W + 3;
    uint16_t y = s_grid_top + row * cell_h + 3;
    uint16_t w = CELL_W - 6;
    uint16_t h = cell_h - 6;
    uint16_t bg = touched ? COLOR_GREEN : 0x2124;
    ili9488_fill_rect(x, y, w, h, bg);
    char label[3];
    snprintf(label, sizeof(label), "%d", ch);
    uint16_t lw = (ch < 10) ? 12 : 24;
    ili9488_draw_string(x + (w - lw) / 2, y + (h - 14) / 2, label, COLOR_WHITE, bg, 2);
}

static void test_touch_task(void *arg)
{
    if (!s_mpr_found) {
        err("Touch skip: MPR121 absent");
        vTaskDelete(NULL);
        return;
    }

    info("MPR121 init...");
    esp_err_t ret = mpr121_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        char msg[32];
        snprintf(msg, sizeof(msg), "MPR fail: %s", esp_err_to_name(ret));
        err(msg);
        vTaskDelete(NULL);
        return;
    }
    ok("MPR121 OK - touche!");
    vTaskDelay(pdMS_TO_TICKS(400));

    s_grid_top = s_y;
    for (int i = 0; i < MPR121_NUM_CH; i++) draw_cell(i, false);

    mpr121_data_t prev = {0}, curr;
    while (1) {
        ret = mpr121_read(&curr);
        if (ret == ESP_OK && curr.touched != prev.touched) {
            for (int i = 0; i < MPR121_NUM_CH; i++) {
                if (curr.ch[i] != prev.ch[i]) draw_cell(i, curr.ch[i]);
            }
            if (curr.touched) audio_play_tone(660, 40);
            ESP_LOGI(TAG, "touch 0x%03X", curr.touched);
            prev = curr;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── app_main ───────────────────────────────────────────────────────────────

void app_main(void)
{
    leds_init(38, 1);
    leds_clear();
    leds_show();

    ESP_ERROR_CHECK(ili9488_init());
    ili9488_fill(COLOR_BLACK);
    info("=== EscapeBox TEST ===");
    vTaskDelay(pdMS_TO_TICKS(300));

    test_led();
    test_display();

    ESP_ERROR_CHECK(i2c_bus_init());
    test_i2c_scan();
    vTaskDelay(pdMS_TO_TICKS(300));

    test_audio();
    vTaskDelay(pdMS_TO_TICKS(300));

    xTaskCreate(test_touch_task, "touch", 4096, NULL, 5, NULL);
}
