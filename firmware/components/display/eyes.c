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
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define TAG "eyes"

#define EYES_SPI_HOST     SPI3_HOST
#define EYES_PIXEL_CLK_HZ (40 * 1000 * 1000)

// Buffer DMA ligne complète (240 px × 2 bytes = 480) — réutilisé pour fill.
#define LINE_BUF_BYTES (EYE_WIDTH * sizeof(uint16_t))

static esp_lcd_panel_handle_t s_panel[EYE_COUNT] = {0};
// Sémaphore binaire par œil, signalé par l'ISR à la fin de chaque draw_bitmap.
// État initial = "donné" pour que le premier wait passe sans bloquer.
static SemaphoreHandle_t s_trans_done[EYE_COUNT] = {NULL, NULL};
static uint16_t *s_line_buf = NULL;
static bool s_initialized = false;

static bool IRAM_ATTR on_trans_done(esp_lcd_panel_io_handle_t io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx)
{
    (void)io;
    (void)edata;
    eye_id_t eye = (eye_id_t)(uintptr_t)user_ctx;
    BaseType_t hp_woken = pdFALSE;
    xSemaphoreGiveFromISR(s_trans_done[eye], &hp_woken);
    return hp_woken == pdTRUE;
}

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

static esp_err_t panel_setup(int cs_gpio, eye_id_t eye, esp_lcd_panel_handle_t *out)
{
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(
        cs_gpio, EYES_PIN_DC, on_trans_done, (void *)(uintptr_t)eye);
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

    for (int e = 0; e < EYE_COUNT; e++) {
        // État initial = "vide" : eyes_wait_done() bloque jusqu'à la prochaine
        // transaction complétée par l'ISR. Pattern : draw_bitmap PUIS wait_done.
        s_trans_done[e] = xSemaphoreCreateBinary();
        if (!s_trans_done[e]) return ESP_ERR_NO_MEM;
    }

    ret = panel_setup(EYES_PIN_CS_L, EYE_LEFT, &s_panel[EYE_LEFT]);
    if (ret != ESP_OK) return ret;

    ret = panel_setup(EYES_PIN_CS_R, EYE_RIGHT, &s_panel[EYE_RIGHT]);
    if (ret != ESP_OK) return ret;

    s_line_buf = heap_caps_malloc(LINE_BUF_BYTES, MALLOC_CAP_DMA);
    if (!s_line_buf) {
        ESP_LOGE(TAG, "line_buf alloc");
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "yeux GC9A01 OK (CS_L=%d, CS_R=%d, SPI3 @ %d MHz)",
             EYES_PIN_CS_L, EYES_PIN_CS_R, EYES_PIXEL_CLK_HZ / 1000000);
    return ESP_OK;
}

esp_lcd_panel_handle_t eyes_panel(eye_id_t eye)
{
    if (eye >= EYE_COUNT) return NULL;
    return s_panel[eye];
}

esp_err_t eyes_wait_done(eye_id_t eye)
{
    if (!s_initialized || eye >= EYE_COUNT) return ESP_ERR_INVALID_STATE;
    if (!s_trans_done[eye]) return ESP_ERR_INVALID_STATE;
    // m3 : timeout 500 ms — un transfert DMA 128×128×2 B à 40 MHz prend ~0.5 ms.
    // En cas de blocage SPI inattendu, on remonte ESP_ERR_TIMEOUT plutôt que de
    // bloquer la tâche eye indéfiniment.
    if (xSemaphoreTake(s_trans_done[eye], pdMS_TO_TICKS(500)) != pdTRUE) {
        ESP_LOGE(TAG, "eyes_wait_done[%d]: timeout SPI — transfert DMA perdu ?", (int)eye);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

// M6 : NOT thread-safe — utilise s_line_buf partagé sans mutex.
// Appelée uniquement depuis app_main() au boot, avant le démarrage de ui_face_start().
// Ne pas appeler depuis plusieurs tâches simultanément.
esp_err_t eyes_fill(eye_id_t eye, uint16_t color)
{
    if (!s_initialized || eye >= EYE_COUNT) return ESP_ERR_INVALID_STATE;

    // GC9A01 attend RGB565 big-endian sur le bus.
    uint16_t be = (uint16_t)(((color & 0xFF) << 8) | ((color >> 8) & 0xFF));
    for (int i = 0; i < EYE_WIDTH; i++) s_line_buf[i] = be;

    // s_line_buf ne change pas pendant la boucle (même couleur sur toutes les
    // scanlines) → pas besoin d'attendre chaque transaction, on enfile les 240
    // scanlines en parallèle. Le driver bloque sur la queue (trans_queue_depth)
    // quand elle est pleine. À la fin, on DRAIN tous les signaux ISR cumulés
    // pour laisser le sémaphore en état "vide" — sinon les give multiples
    // restent comptés comme 1 (sémaphore binaire) et la prochaine wait_done
    // passerait sans attendre, créant une race sur le buffer DMA suivant.
    esp_lcd_panel_handle_t p = s_panel[eye];
    for (int y = 0; y < EYE_HEIGHT; y++) {
        esp_err_t ret = esp_lcd_panel_draw_bitmap(p, 0, y, EYE_WIDTH, y + 1, s_line_buf);
        if (ret != ESP_OK) return ret;
    }
    // Drain : consomme tous les give jusqu'à timeout court → queue vide.
    while (xSemaphoreTake(s_trans_done[eye], pdMS_TO_TICKS(50)) == pdTRUE) { }
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
    esp_err_t ret = esp_lcd_panel_draw_bitmap(s_panel[eye], x, y, x + w, y + h, pixels);
    if (ret == ESP_OK) eyes_wait_done(eye);
    return ret;
}
