#include "eyes.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_gc9a01.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "eyes"

#define EYES_SPI_HOST     SPI3_HOST
#define EYES_PIXEL_CLK_HZ (40 * 1000 * 1000)

// Buffer DMA ligne complète (240 px × 2 bytes = 480) — réutilisé pour fill.
#define LINE_BUF_BYTES (EYE_WIDTH * sizeof(uint16_t))

static esp_lcd_panel_handle_t s_panel[EYE_COUNT] = {0};
static uint16_t *s_line_buf = NULL;
static bool s_initialized = false;

static esp_err_t hw_reset(void)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << EYES_PIN_RST,
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t ret = gpio_config(&io);
    if (ret != ESP_OK) return ret;

    gpio_set_level(EYES_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EYES_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(EYES_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
    return ESP_OK;
}

static esp_err_t panel_setup(int cs_gpio, esp_lcd_panel_handle_t *out)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(cs_gpio, EYES_PIN_DC, NULL, NULL);
    io_config.pclk_hz = EYES_PIXEL_CLK_HZ;
    io_config.trans_queue_depth = 10;

    esp_err_t ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)EYES_SPI_HOST, &io_config, &io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel_io_spi(cs=%d): %s", cs_gpio, esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = -1, // RST partagé, géré en HW une fois pour les deux.
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };

    esp_lcd_panel_handle_t panel = NULL;
    ret = esp_lcd_new_panel_gc9a01(io, &panel_cfg, &panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "new_panel_gc9a01(cs=%d): %s", cs_gpio, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_lcd_panel_init(panel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel_init(cs=%d): %s", cs_gpio, esp_err_to_name(ret));
        return ret;
    }
    esp_lcd_panel_invert_color(panel, true);
    esp_lcd_panel_disp_on_off(panel, true);

    *out = panel;
    return ESP_OK;
}

esp_err_t eyes_init(void)
{
    if (s_initialized) return ESP_OK;

    spi_bus_config_t bus = GC9A01_PANEL_BUS_SPI_CONFIG(EYES_PIN_SCLK, EYES_PIN_MOSI,
                                                       EYE_WIDTH * EYE_HEIGHT * sizeof(uint16_t));
    esp_err_t ret = spi_bus_initialize(EYES_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = hw_reset();
    if (ret != ESP_OK) return ret;

    ret = panel_setup(EYES_PIN_CS_L, &s_panel[EYE_LEFT]);
    if (ret != ESP_OK) return ret;

    // EYE_RIGHT (CS_R=14) non câblé physiquement en Phase 1 — ne pas
    // enregistrer le device SPI : les transactions vers un CS flottant ne
    // génèrent jamais d'interruption de complétion, ce qui bloque
    // spi_device_get_trans_result() indéfiniment dès que la queue est pleine.
    // Retirer cette ligne quand le second écran est câblé.
    // ret = panel_setup(EYES_PIN_CS_R, &s_panel[EYE_RIGHT]);
    // if (ret != ESP_OK) return ret;
    s_panel[EYE_RIGHT] = NULL;

    s_line_buf = heap_caps_malloc(LINE_BUF_BYTES, MALLOC_CAP_DMA);
    if (!s_line_buf) {
        ESP_LOGE(TAG, "line_buf alloc");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "yeux GC9A01 OK (CS_L=%d actif, CS_R=%d desactive Phase1, SPI3 @ %d MHz)",
             EYES_PIN_CS_L, EYES_PIN_CS_R, EYES_PIXEL_CLK_HZ / 1000000);
    return ESP_OK;
}

esp_lcd_panel_handle_t eyes_panel(eye_id_t eye)
{
    if (eye >= EYE_COUNT) return NULL;
    return s_panel[eye];
}

esp_err_t eyes_fill(eye_id_t eye, uint16_t color)
{
    if (!s_initialized || eye >= EYE_COUNT) return ESP_ERR_INVALID_STATE;

    // GC9A01 attend RGB565 big-endian sur le bus.
    uint16_t be = (uint16_t)(((color & 0xFF) << 8) | ((color >> 8) & 0xFF));
    for (int i = 0; i < EYE_WIDTH; i++) s_line_buf[i] = be;

    esp_lcd_panel_handle_t p = s_panel[eye];
    for (int y = 0; y < EYE_HEIGHT; y++) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(p, 0, y, EYE_WIDTH, y + 1, s_line_buf);
        if (ret != ESP_OK) return ret;
    }
    return ESP_OK;
}

esp_err_t eyes_fill_all(uint16_t color)
{
    esp_err_t r1 = eyes_fill(EYE_LEFT,  color);
    esp_err_t r2 = eyes_fill(EYE_RIGHT, color);
    return (r1 != ESP_OK) ? r1 : r2;
}

esp_err_t eyes_draw(eye_id_t eye, int x, int y, int w, int h, const uint16_t *pixels)
{
    if (!s_initialized || eye >= EYE_COUNT || !pixels) return ESP_ERR_INVALID_ARG;
    if (w <= 0 || h <= 0) return ESP_ERR_INVALID_ARG;
    return esp_lcd_panel_draw_bitmap(s_panel[eye], x, y, x + w, y + h, pixels);
}
