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

#define TAG "main"

// ─── Helpers affichage ──────────────────────────────────────────────────────

static uint16_t s_screen_y = 4;

static void screen_log(const char *msg, uint16_t color)
{
    ili9488_fill_rect(0, s_screen_y, ILI9488_WIDTH, 18, COLOR_BLACK);
    ili9488_draw_string(4, s_screen_y, msg, color, COLOR_BLACK, 2);
    s_screen_y += 20;
    ESP_LOGI(TAG, "%s", msg);
}

static void screen_ok(const char *msg)  { screen_log(msg, COLOR_GREEN); }
static void screen_err(const char *msg) { screen_log(msg, COLOR_RED);   }
static void screen_info(const char *msg){ screen_log(msg, COLOR_CYAN);  }

// ─── Test 1 : scan I2C ──────────────────────────────────────────────────────

static bool s_mpr_found = false;

static void test_i2c_scan(void)
{
    char line[32];
    snprintf(line, sizeof(line), "I2C SDA=%d SCL=%d 100kHz", I2C_BUS_SDA, I2C_BUS_SCL);
    screen_info(line);

    int found = 0;
    i2c_master_bus_handle_t bus = i2c_bus_handle();

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        esp_err_t ret = i2c_master_probe(bus, addr, 20);  // 20ms, API dédiée probe
        if (ret == ESP_OK) {
            found++;
            if (addr == 0x5A) {
                screen_ok("0x5A MPR121 OK");
                s_mpr_found = true;
            } else {
                snprintf(line, sizeof(line), "0x%02X trouve!", addr);
                screen_info(line);
            }
            ESP_LOGI(TAG, "I2C found 0x%02X", addr);
        }
    }

    // Diagnostic ciblé 0x5A : afficher l'erreur brute si absent
    if (!s_mpr_found) {
        esp_err_t ret = i2c_master_probe(bus, 0x5A, 50);
        snprintf(line, sizeof(line), "0x5A err:%s", esp_err_to_name(ret));
        screen_err(line);
        ESP_LOGE(TAG, "MPR121 probe: %s", esp_err_to_name(ret));
    }

    if (found == 0) {
        screen_err("0 device - bus mort?");
        // Tenter aussi les adresses autour de 0x5A (ADDR pin flottant?)
        for (uint8_t a = 0x58; a <= 0x5D; a++) {
            esp_err_t r = i2c_master_probe(bus, a, 50);
            ESP_LOGI(TAG, "probe 0x%02X: %s", a, esp_err_to_name(r));
            if (r == ESP_OK) {
                snprintf(line, sizeof(line), "0x%02X REPOND!", a);
                screen_ok(line);
                found++;
            }
        }
    }
}

// ─── Test 2 : audio I2S ─────────────────────────────────────────────────────

static void test_audio(void)
{
    screen_info("Audio I2S (hw mode)...");
    esp_err_t ret = audio_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        screen_err("Audio init FAIL");
        return;
    }
    static const uint16_t notes[] = { 262, 294, 330, 349, 392, 440, 494, 523, 0 };
    for (int i = 0; notes[i]; i++) {
        audio_play_tone(notes[i], 180);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    audio_play_tone(880, 100);
    screen_ok("Audio OK");
}

// ─── Test 3 : touch ─────────────────────────────────────────────────────────

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
    ili9488_draw_string(x + (w - lw) / 2, y + (h - 14) / 2,
                        label, COLOR_WHITE, bg, 2);
}

static void test_touch_task(void *arg)
{
    if (!s_mpr_found) {
        screen_err("Touch skip: MPR121 absent");
        vTaskDelete(NULL);
        return;
    }

    screen_info("MPR: init...");
    esp_err_t ret = mpr121_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        char msg[32];
        snprintf(msg, sizeof(msg), "MPR fail: %s", esp_err_to_name(ret));
        screen_err(msg);
        vTaskDelete(NULL);
        return;
    }
    screen_ok("MPR121 OK - touche!");
    vTaskDelay(pdMS_TO_TICKS(400));

    s_grid_top = s_screen_y;
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
    // Éteindre la LED RGB intégrée (GPIO38) dès le boot
    leds_init(38, 1);
    leds_clear();
    leds_show();

    ESP_ERROR_CHECK(ili9488_init());
    ili9488_fill(COLOR_BLACK);
    screen_info("EscapeBox S3 boot");
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_ERROR_CHECK(i2c_bus_init());
    test_i2c_scan();
    vTaskDelay(pdMS_TO_TICKS(300));

    test_audio();
    vTaskDelay(pdMS_TO_TICKS(300));

    xTaskCreate(test_touch_task, "touch", 4096, NULL, 5, NULL);
}
