#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "ili9488.h"
#include "i2c_bus.h"
#include "audio.h"
#include "mpr121.h"
#include <stdio.h>

#define TAG "main"

// ─── Helpers affichage ──────────────────────────────────────────────────────

static uint16_t s_screen_y = 4;  // curseur vertical courant

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

// ─── Test 1 : scan I2C affiché sur écran ────────────────────────────────────

static bool s_pcm_found  = false;
static bool s_mpr_found  = false;

static void test_i2c_scan(void)
{
    screen_info("Scan I2C...");
    i2c_master_bus_handle_t bus = i2c_bus_handle();

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, pdMS_TO_TICKS(10)) != ESP_OK) continue;

        char line[24];
        if (addr == 0x4C) {
            snprintf(line, sizeof(line), "  0x4C PCM5122 OK");
            screen_ok(line);
            s_pcm_found = true;
        } else if (addr == 0x5A) {
            snprintf(line, sizeof(line), "  0x5A MPR121  OK");
            screen_ok(line);
            s_mpr_found = true;
        } else {
            snprintf(line, sizeof(line), "  0x%02X inconnu", addr);
            screen_info(line);
        }
    }

    if (!s_pcm_found) screen_err("  0x4C PCM5122 ABSENT");
    if (!s_mpr_found) screen_err("  0x5A MPR121  ABSENT");
}

// ─── Test 2 : audio ─────────────────────────────────────────────────────────

static void test_audio(void)
{
    if (!s_pcm_found) {
        screen_err("Audio skip: PCM5122 absent");
        return;
    }

    esp_err_t ret = audio_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        screen_err("Audio init FAIL");
        return;
    }
    screen_ok("Audio init OK");

    screen_info("Lecture gamme...");
    static const uint16_t notes[] = { 262, 294, 330, 349, 392, 440, 494, 523, 0 };
    for (int i = 0; notes[i]; i++) {
        audio_play_tone(notes[i], 180);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    audio_play_tone(880, 80);
    vTaskDelay(pdMS_TO_TICKS(60));
    audio_play_tone(880, 80);
    screen_ok("Audio OK");
}

// ─── Test 3 : touch → écran ─────────────────────────────────────────────────

#define TOUCH_OFFSET_Y  (s_screen_y + 10)
#define GRID_COLS   4
#define GRID_ROWS   3
#define CELL_W      (ILI9488_WIDTH / GRID_COLS)   // 80

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

    esp_err_t ret = mpr121_init(i2c_bus_handle());
    if (ret != ESP_OK) {
        screen_err("MPR121 init FAIL");
        vTaskDelete(NULL);
        return;
    }
    screen_ok("MPR121 init OK");
    vTaskDelay(pdMS_TO_TICKS(500));

    // Barre de titre grille
    s_grid_top = s_screen_y + 4;
    ili9488_fill_rect(0, s_screen_y, ILI9488_WIDTH, 20, 0x000F);
    ili9488_draw_string(4, s_screen_y + 3, "TOUCH — appuie sur les pads",
                        COLOR_CYAN, 0x000F, 1);
    s_screen_y += 22;
    s_grid_top = s_screen_y;

    for (int i = 0; i < MPR121_NUM_CH; i++) draw_cell(i, false);

    mpr121_data_t prev = {0}, curr;

    while (1) {
        ret = mpr121_read(&curr);
        if (ret == ESP_OK) {
            if (curr.touched != prev.touched) {
                for (int i = 0; i < MPR121_NUM_CH; i++) {
                    if (curr.ch[i] != prev.ch[i])
                        draw_cell(i, curr.ch[i]);
                }
                if (curr.touched && s_pcm_found)
                    audio_play_tone(660, 50);
                ESP_LOGI(TAG, "touch 0x%03X", curr.touched);
                prev = curr;
            }
        } else {
            ESP_LOGW(TAG, "mpr121_read err %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── app_main ───────────────────────────────────────────────────────────────

void app_main(void)
{
    ESP_ERROR_CHECK(ili9488_init());
    ili9488_fill(COLOR_BLACK);

    screen_info("EscapeBox S3 boot");
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_ERROR_CHECK(i2c_bus_init());

    test_i2c_scan();
    vTaskDelay(pdMS_TO_TICKS(400));

    test_audio();
    vTaskDelay(pdMS_TO_TICKS(300));

    xTaskCreate(test_touch_task, "touch", 4096, NULL, 5, NULL);
}
