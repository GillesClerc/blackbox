#include "hal_leds.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

#define TAG "hal_leds"

// Horloge RMT : 10 MHz → 1 tick = 0.1 µs
// WS2812B timing :
//   T0H = 0.4µs →  4 ticks, T0L = 0.85µs →  9 ticks (total = 1.25µs ≈ ok)
//   T1H = 0.8µs →  8 ticks, T1L = 0.45µs →  5 ticks (total = 1.25µs ≈ ok)
//   Reset = > 50µs → géré par vTaskDelay(1) après transmission
#define RMT_CLK_HZ    (10 * 1000 * 1000)

static rmt_channel_handle_t s_chan     = NULL;
static rmt_encoder_handle_t s_encoder  = NULL;
static uint8_t  s_buf[LEDS_MAX_COUNT * 3];  // GRB order
static uint16_t s_count                = 0;

// --- Helpers couleur ---

static void hex_to_grb(const char *hex, uint8_t brightness,
                        uint8_t *g, uint8_t *r, uint8_t *b) {
    if (!hex || hex[0] != '#' || strlen(hex) < 7) {
        *g = *r = *b = 0;
        return;
    }
    unsigned long rgb = strtoul(hex + 1, NULL, 16);
    uint8_t rr = (rgb >> 16) & 0xFF;
    uint8_t gg = (rgb >>  8) & 0xFF;
    uint8_t bb = (rgb      ) & 0xFF;
    *r = (uint8_t)((uint32_t)rr * brightness / 255);
    *g = (uint8_t)((uint32_t)gg * brightness / 255);
    *b = (uint8_t)((uint32_t)bb * brightness / 255);
}

// --- Init ---

esp_err_t hal_leds_init(int gpio_pin, uint16_t num_leds) {
    if (num_leds > LEDS_MAX_COUNT) num_leds = LEDS_MAX_COUNT;
    s_count = num_leds;
    memset(s_buf, 0, sizeof(s_buf));

    rmt_tx_channel_config_t chan_cfg = {
        .gpio_num           = gpio_pin,
        .clk_src            = RMT_CLK_SRC_DEFAULT,
        .resolution_hz      = RMT_CLK_HZ,
        .mem_block_symbols  = 64,
        .trans_queue_depth  = 4,
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&chan_cfg, &s_chan));

    rmt_bytes_encoder_config_t enc_cfg = {
        .bit0 = { .duration0 = 4, .level0 = 1, .duration1 = 9, .level1 = 0 },
        .bit1 = { .duration0 = 8, .level0 = 1, .duration1 = 5, .level1 = 0 },
        .flags.msb_first = 1,
    };
    ESP_ERROR_CHECK(rmt_new_bytes_encoder(&enc_cfg, &s_encoder));
    ESP_ERROR_CHECK(rmt_enable(s_chan));

    ESP_LOGI(TAG, "%d LEDs WS2812B sur GPIO%d", num_leds, gpio_pin);
    return ESP_OK;
}

// --- Contrôle couleur ---

void hal_leds_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx >= s_count) return;
    s_buf[idx * 3]     = g;  // WS2812 order : GRB
    s_buf[idx * 3 + 1] = r;
    s_buf[idx * 3 + 2] = b;
}

void hal_leds_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (uint16_t i = 0; i < s_count; i++) hal_leds_set(i, r, g, b);
}

void hal_leds_clear(void) {
    memset(s_buf, 0, s_count * 3);
}

void hal_leds_fill_hex(const char *hex, uint8_t brightness) {
    uint8_t r, g, b;
    hex_to_grb(hex, brightness, &g, &r, &b);
    hal_leds_fill(r, g, b);
}

void hal_leds_set_hex(uint16_t idx, const char *hex, uint8_t brightness) {
    uint8_t r, g, b;
    hex_to_grb(hex, brightness, &g, &r, &b);
    hal_leds_set(idx, r, g, b);
}

// --- Transmission ---

esp_err_t hal_leds_show(void) {
    if (!s_chan || !s_encoder || s_count == 0) return ESP_ERR_INVALID_STATE;

    rmt_transmit_config_t tx_cfg = { .loop_count = 0 };
    esp_err_t ret = rmt_transmit(s_chan, s_encoder, s_buf, s_count * 3, &tx_cfg);
    if (ret != ESP_OK) return ret;

    // Timeout 200 ms — 1 LED WS2812B = 30 µs, 300 LEDs ~9 ms en transmission,
    // mais le driver RMT v6.1 a un délai interne de flush variable. 50 ms est
    // trop juste en pratique. portMAX_DELAY bloquerait indéfiniment si panne.
    esp_err_t wait_ret = rmt_tx_wait_all_done(s_chan, pdMS_TO_TICKS(200));
    if (wait_ret != ESP_OK) {
        ESP_LOGW(TAG, "hal_leds_show: rmt_tx_wait_all_done timeout");
    }
    vTaskDelay(1);  // reset pulse WS2812 (>50µs)
    return ESP_OK;
}
