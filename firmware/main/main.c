#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "ili9488.h"
#include "i2c_bus.h"
#include "audio.h"
#include "mpr121.h"

#define TAG "main"

// ─── Test 1 : scan I2C ──────────────────────────────────────────────────────

static void test_i2c_scan(void)
{
    ESP_LOGI(TAG, "=== TEST 1 : scan I2C (SDA=%d SCL=%d) ===",
             I2C_BUS_SDA, I2C_BUS_SCL);

    i2c_master_bus_handle_t bus = i2c_bus_handle();
    int found = 0;

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bus, addr, pdMS_TO_TICKS(10)) == ESP_OK) {
            ESP_LOGI(TAG, "  0x%02X trouvé", addr);
            found++;
        }
    }

    if (found == 0)
        ESP_LOGW(TAG, "  aucun device — vérifier SDA/SCL");
    else
        ESP_LOGI(TAG, "  %d device(s) — PCM5122=0x4C MPR121=0x5A attendus", found);
}

// ─── Test 2 : audio ─────────────────────────────────────────────────────────

static void test_audio(void)
{
    ESP_LOGI(TAG, "=== TEST 2 : audio (BCLK=%d LRCK=%d DOUT=%d) ===",
             AUDIO_PIN_BCLK, AUDIO_PIN_LRCK, AUDIO_PIN_DOUT);

    // Do majeur — si on entend 8 notes + double bip final, toute la chaîne est OK
    static const uint16_t notes[] = { 262, 294, 330, 349, 392, 440, 494, 523, 0 };
    for (int i = 0; notes[i]; i++) {
        audio_play_tone(notes[i], 180);
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    audio_play_tone(880, 80);
    vTaskDelay(pdMS_TO_TICKS(60));
    audio_play_tone(880, 80);

    ESP_LOGI(TAG, "  séquence envoyée — doit sonner Do Re Mi Fa Sol La Si Do + bip x2");
}

// ─── Test 3 : touch → écran ─────────────────────────────────────────────────

#define HEADER_H    52
#define GRID_COLS   4
#define GRID_ROWS   3
#define CELL_W      (ILI9488_WIDTH / GRID_COLS)                    // 80
#define CELL_H      ((ILI9488_HEIGHT - HEADER_H) / GRID_ROWS)      // 142

static void draw_cell(int ch, bool touched)
{
    int col = ch % GRID_COLS;
    int row = ch / GRID_COLS;
    uint16_t x = col * CELL_W + 3;
    uint16_t y = HEADER_H + row * CELL_H + 3;
    uint16_t w = CELL_W - 6;
    uint16_t h = CELL_H - 6;

    uint16_t bg = touched ? COLOR_GREEN : 0x2124;
    ili9488_fill_rect(x, y, w, h, bg);

    char label[3];
    snprintf(label, sizeof(label), "%d", ch);
    // centré approximatif : 1 chiffre = 12px large (scale 2), 2 chiffres = 24px
    uint16_t lw = (ch < 10) ? 12 : 24;
    ili9488_draw_string(x + (w - lw) / 2, y + (h - 14) / 2, label,
                        COLOR_WHITE, bg, 2);
}

static void test_touch_task(void *arg)
{
    ESP_LOGI(TAG, "=== TEST 3 : touch → écran (polling 50Hz) ===");

    ili9488_fill(COLOR_BLACK);
    ili9488_fill_rect(0, 0, ILI9488_WIDTH, HEADER_H, 0x000F);
    ili9488_draw_string(8, 8,  "TOUCH TEST",         COLOR_CYAN, 0x000F, 3);
    ili9488_draw_string(8, 36, "MPR121  12 canaux",  COLOR_GRAY, 0x000F, 1);

    // Grille initiale (tout inactif)
    for (int i = 0; i < MPR121_NUM_CH; i++) draw_cell(i, false);

    mpr121_data_t prev = {0}, curr;

    while (1) {
        if (mpr121_read(&curr) == ESP_OK && curr.touched != prev.touched) {
            for (int i = 0; i < MPR121_NUM_CH; i++) {
                if (curr.ch[i] != prev.ch[i])
                    draw_cell(i, curr.ch[i]);
            }
            if (curr.touched)
                audio_play_tone(660, 50);   // bip court à chaque touche
            ESP_LOGI(TAG, "touch bitmask=0x%03X", curr.touched);
            prev = curr;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// ─── app_main ───────────────────────────────────────────────────────────────

void app_main(void)
{
    ESP_LOGI(TAG, "=== EscapeBox S3 — test hardware ===");

    // Display — déjà validé, sert de confirmation visuelle au boot
    ESP_ERROR_CHECK(ili9488_init());
    ili9488_test_screen();
    vTaskDelay(pdMS_TO_TICKS(800));

    // Bus I2C partagé (PCM5122 + MPR121)
    ESP_ERROR_CHECK(i2c_bus_init());

    // Test 1
    test_i2c_scan();
    vTaskDelay(pdMS_TO_TICKS(300));

    // Test 2
    ESP_ERROR_CHECK(audio_init(i2c_bus_handle()));
    test_audio();
    vTaskDelay(pdMS_TO_TICKS(300));

    // Test 3 — tourne en continu dans une tâche
    ESP_ERROR_CHECK(mpr121_init(i2c_bus_handle()));
    xTaskCreate(test_touch_task, "touch", 4096, NULL, 5, NULL);
}
