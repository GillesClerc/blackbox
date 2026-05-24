#include "ili9488.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "ili9488"

// ILI9488 SPI : 18-bit/pixel (RGB666) — 3 bytes par pixel
// Contrairement au ST7735, l'ILI9488 n'accepte pas RGB565 en SPI 4 fils
#define BYTES_PER_PIXEL 3

// Buffer DMA — multiple de 3, aligné 4 octets, en SRAM interne
#define DMA_BUF_PIXELS 1360
#define DMA_BUF_BYTES  (DMA_BUF_PIXELS * BYTES_PER_PIXEL)  // 4080 bytes

static spi_device_handle_t s_spi;
static gpio_num_t s_pin_dc  = ILI9488_PIN_DC;
static gpio_num_t s_pin_rst = ILI9488_PIN_RST;

static uint8_t s_dma_buf[DMA_BUF_BYTES] __attribute__((aligned(4)));

// Police bitmap 5x7 — ASCII 32 à 126
static const uint8_t s_font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x5F,0x00,0x00}, // !
    {0x00,0x07,0x00,0x07,0x00}, // "
    {0x14,0x7F,0x14,0x7F,0x14}, // #
    {0x24,0x2A,0x7F,0x2A,0x12}, // $
    {0x23,0x13,0x08,0x64,0x62}, // %
    {0x36,0x49,0x55,0x22,0x50}, // &
    {0x00,0x05,0x03,0x00,0x00}, // '
    {0x00,0x1C,0x22,0x41,0x00}, // (
    {0x00,0x41,0x22,0x1C,0x00}, // )
    {0x14,0x08,0x3E,0x08,0x14}, // *
    {0x08,0x08,0x3E,0x08,0x08}, // +
    {0x00,0x50,0x30,0x00,0x00}, // ,
    {0x08,0x08,0x08,0x08,0x08}, // -
    {0x00,0x60,0x60,0x00,0x00}, // .
    {0x20,0x10,0x08,0x04,0x02}, // /
    {0x3E,0x51,0x49,0x45,0x3E}, // 0
    {0x00,0x42,0x7F,0x40,0x00}, // 1
    {0x42,0x61,0x51,0x49,0x46}, // 2
    {0x21,0x41,0x45,0x4B,0x31}, // 3
    {0x18,0x14,0x12,0x7F,0x10}, // 4
    {0x27,0x45,0x45,0x45,0x39}, // 5
    {0x3C,0x4A,0x49,0x49,0x30}, // 6
    {0x01,0x71,0x09,0x05,0x03}, // 7
    {0x36,0x49,0x49,0x49,0x36}, // 8
    {0x06,0x49,0x49,0x29,0x1E}, // 9
    {0x00,0x36,0x36,0x00,0x00}, // :
    {0x00,0x56,0x36,0x00,0x00}, // ;
    {0x08,0x14,0x22,0x41,0x00}, // <
    {0x14,0x14,0x14,0x14,0x14}, // =
    {0x00,0x41,0x22,0x14,0x08}, // >
    {0x02,0x01,0x51,0x09,0x06}, // ?
    {0x32,0x49,0x79,0x41,0x3E}, // @
    {0x7E,0x11,0x11,0x11,0x7E}, // A
    {0x7F,0x49,0x49,0x49,0x36}, // B
    {0x3E,0x41,0x41,0x41,0x22}, // C
    {0x7F,0x41,0x41,0x22,0x1C}, // D
    {0x7F,0x49,0x49,0x49,0x41}, // E
    {0x7F,0x09,0x09,0x09,0x01}, // F
    {0x3E,0x41,0x49,0x49,0x7A}, // G
    {0x7F,0x08,0x08,0x08,0x7F}, // H
    {0x00,0x41,0x7F,0x41,0x00}, // I
    {0x20,0x40,0x41,0x3F,0x01}, // J
    {0x7F,0x08,0x14,0x22,0x41}, // K
    {0x7F,0x40,0x40,0x40,0x40}, // L
    {0x7F,0x02,0x0C,0x02,0x7F}, // M
    {0x7F,0x04,0x08,0x10,0x7F}, // N
    {0x3E,0x41,0x41,0x41,0x3E}, // O
    {0x7F,0x09,0x09,0x09,0x06}, // P
    {0x3E,0x41,0x51,0x21,0x5E}, // Q
    {0x7F,0x09,0x19,0x29,0x46}, // R
    {0x46,0x49,0x49,0x49,0x31}, // S
    {0x01,0x01,0x7F,0x01,0x01}, // T
    {0x3F,0x40,0x40,0x40,0x3F}, // U
    {0x1F,0x20,0x40,0x20,0x1F}, // V
    {0x3F,0x40,0x38,0x40,0x3F}, // W
    {0x63,0x14,0x08,0x14,0x63}, // X
    {0x07,0x08,0x70,0x08,0x07}, // Y
    {0x61,0x51,0x49,0x45,0x43}, // Z
    {0x00,0x7F,0x41,0x41,0x00}, // [
    {0x02,0x04,0x08,0x10,0x20}, // backslash
    {0x00,0x41,0x41,0x7F,0x00}, // ]
    {0x04,0x02,0x01,0x02,0x04}, // ^
    {0x40,0x40,0x40,0x40,0x40}, // _
    {0x00,0x01,0x02,0x04,0x00}, // `
    {0x20,0x54,0x54,0x54,0x78}, // a
    {0x7F,0x48,0x44,0x44,0x38}, // b
    {0x38,0x44,0x44,0x44,0x20}, // c
    {0x38,0x44,0x44,0x48,0x7F}, // d
    {0x38,0x54,0x54,0x54,0x18}, // e
    {0x08,0x7E,0x09,0x01,0x02}, // f
    {0x0C,0x52,0x52,0x52,0x3E}, // g
    {0x7F,0x08,0x04,0x04,0x78}, // h
    {0x00,0x44,0x7D,0x40,0x00}, // i
    {0x20,0x40,0x44,0x3D,0x00}, // j
    {0x7F,0x10,0x28,0x44,0x00}, // k
    {0x00,0x41,0x7F,0x40,0x00}, // l
    {0x7C,0x04,0x18,0x04,0x78}, // m
    {0x7C,0x08,0x04,0x04,0x78}, // n
    {0x38,0x44,0x44,0x44,0x38}, // o
    {0x7C,0x14,0x14,0x14,0x08}, // p
    {0x08,0x14,0x14,0x18,0x7C}, // q
    {0x7C,0x08,0x04,0x04,0x08}, // r
    {0x48,0x54,0x54,0x54,0x20}, // s
    {0x04,0x3F,0x44,0x40,0x20}, // t
    {0x3C,0x40,0x40,0x20,0x7C}, // u
    {0x1C,0x20,0x40,0x20,0x1C}, // v
    {0x3C,0x40,0x30,0x40,0x3C}, // w
    {0x44,0x28,0x10,0x28,0x44}, // x
    {0x0C,0x50,0x50,0x50,0x3C}, // y
    {0x44,0x64,0x54,0x4C,0x44}, // z
    {0x00,0x08,0x36,0x41,0x00}, // {
    {0x00,0x00,0x7F,0x00,0x00}, // |
    {0x00,0x41,0x36,0x08,0x00}, // }
    {0x10,0x08,0x08,0x10,0x08}, // ~
};

// ---------- Bas niveau SPI ----------

static void spi_write(const uint8_t *data, size_t len, bool is_cmd)
{
    gpio_set_level(s_pin_dc, is_cmd ? 0 : 1);
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static inline void send_cmd(uint8_t cmd)   { spi_write(&cmd, 1, true); }
static inline void send_byte(uint8_t b)    { spi_write(&b,   1, false); }

static void send_bytes(const uint8_t *data, size_t len)
{
    spi_write(data, len, false);
}

// ---------- Séquence init ILI9488 ----------

static void ili9488_init_seq(void)
{
    // Positive Gamma
    send_cmd(0xE0);
    static const uint8_t pgamma[] = {
        0x00,0x07,0x0f,0x0d,0x1b,0x0a,0x3c,
        0x78,0x4a,0x07,0x0e,0x09,0x1b,0x1e,0x0f
    };
    send_bytes(pgamma, sizeof(pgamma));

    // Negative Gamma
    send_cmd(0xE1);
    static const uint8_t ngamma[] = {
        0x00,0x22,0x24,0x06,0x12,0x07,0x36,
        0x47,0x47,0x06,0x0a,0x07,0x30,0x37,0x0f
    };
    send_bytes(ngamma, sizeof(ngamma));

    send_cmd(0xC0); send_byte(0x17); send_byte(0x15);  // Power Control 1
    send_cmd(0xC1); send_byte(0x41);                   // Power Control 2
    send_cmd(0xC5); send_byte(0x00); send_byte(0x12); send_byte(0x80); // VCOM

    // MADCTL : portrait 320x480, ordre BGR (panel standard)
    send_cmd(0x36); send_byte(0x48);

    // Pixel format : 18-bit (RGB666) — seul format fiable en SPI 4 fils
    send_cmd(0x3A); send_byte(0x66);

    send_cmd(0xB0); send_byte(0x00);                   // Interface Mode
    send_cmd(0xB1); send_byte(0xA0);                   // Frame Rate 60Hz
    send_cmd(0xB4); send_byte(0x02);                   // Inversion Control
    send_cmd(0xB6); send_byte(0x02); send_byte(0x02);  // Display Function
    send_cmd(0xE9); send_byte(0x00);                   // Set Image Function
    send_cmd(0xF7);
    send_byte(0xA9); send_byte(0x51); send_byte(0x2C); send_byte(0x82); // Adjust Ctrl 3

    send_cmd(0x11);                    // Sleep Out
    vTaskDelay(pdMS_TO_TICKS(120));
    send_cmd(0x29);                    // Display On
    vTaskDelay(pdMS_TO_TICKS(25));
}

// ---------- Init publique ----------

esp_err_t ili9488_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_pin_dc) | (1ULL << s_pin_rst),
        .mode         = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    gpio_set_level(s_pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(s_pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    spi_bus_config_t bus = {
        .mosi_io_num     = ILI9488_PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = ILI9488_PIN_SCK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DMA_BUF_BYTES,
    };
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) return ret;

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 20 * 1000 * 1000,  // 20MHz — conservateur pour breadboard
        .mode           = 0,
        .spics_io_num   = ILI9488_PIN_CS,
        .queue_size     = 7,
        // Pas de SPI_DEVICE_NO_DUMMY : on écrit en 18-bit, pas de contrainte dummy bits
    };
    ret = spi_bus_add_device(SPI2_HOST, &dev, &s_spi);
    if (ret != ESP_OK) return ret;

    ili9488_init_seq();
    ESP_LOGI(TAG, "ILI9488 initialisé %dx%d @ 20MHz SPI2", ILI9488_WIDTH, ILI9488_HEIGHT);
    return ESP_OK;
}

// ---------- Adressage ----------

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t col[] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t row[] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };
    send_cmd(0x2A); send_bytes(col, 4);
    send_cmd(0x2B); send_bytes(row, 4);
    send_cmd(0x2C);
}

// ---------- Conversion couleur ----------

static inline void rgb565_to_666(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    // R5 → 8 bits top-aligned : xxx << 3
    // G6 → 8 bits top-aligned : xxx << 2
    // B5 → 8 bits top-aligned : xxx << 3
    *r = ((c >> 11) & 0x1F) << 3;
    *g = ((c >>  5) & 0x3F) << 2;
    *b = ( c        & 0x1F) << 3;
}

// ---------- Dessin ----------

void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (w == 0 || h == 0) return;
    set_window(x, y, x + w - 1, y + h - 1);

    uint8_t r, g, b;
    rgb565_to_666(color, &r, &g, &b);

    // Remplir le buffer DMA avec la couleur répétée
    for (int i = 0; i < DMA_BUF_PIXELS; i++) {
        s_dma_buf[i * 3]     = r;
        s_dma_buf[i * 3 + 1] = g;
        s_dma_buf[i * 3 + 2] = b;
    }

    gpio_set_level(s_pin_dc, 1);
    uint32_t remaining = (uint32_t)w * h;
    while (remaining > 0) {
        uint32_t chunk = remaining > DMA_BUF_PIXELS ? DMA_BUF_PIXELS : remaining;
        spi_transaction_t t = {
            .length    = chunk * 24,   // 3 bytes * 8 bits
            .tx_buffer = s_dma_buf,
        };
        spi_device_polling_transmit(s_spi, &t);
        remaining -= chunk;
    }
}

void ili9488_fill(uint16_t color)
{
    ili9488_fill_rect(0, 0, ILI9488_WIDTH, ILI9488_HEIGHT, color);
}

void ili9488_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ILI9488_WIDTH || y >= ILI9488_HEIGHT) return;
    set_window(x, y, x, y);
    uint8_t r, g, b;
    rgb565_to_666(color, &r, &g, &b);
    uint8_t px[] = { r, g, b };
    send_bytes(px, 3);
}

void ili9488_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale)
{
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *glyph = s_font5x7[(uint8_t)(c - 32)];
    for (uint8_t col = 0; col < 5; col++) {
        uint8_t bits = glyph[col];
        for (uint8_t row = 0; row < 7; row++) {
            uint16_t px = (bits & (1 << row)) ? fg : bg;
            ili9488_fill_rect(x + col * scale, y + row * scale, scale, scale, px);
        }
    }
    ili9488_fill_rect(x + 5 * scale, y, scale, 7 * scale, bg);
}

void ili9488_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale)
{
    uint16_t cx = x;
    while (*str) {
        if (cx + 6 * scale > ILI9488_WIDTH) break;
        ili9488_draw_char(cx, y, *str++, fg, bg, scale);
        cx += 6 * scale;
    }
}

// ---------- Écran de test ----------

void ili9488_test_screen(void)
{
    // 8 bandes de couleur — vérifie R/G/B et les niveaux
    static const uint16_t colors[] = {
        COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE,
        COLOR_YELLOW, COLOR_CYAN, COLOR_ORANGE, COLOR_GRAY,
    };
    for (int i = 0; i < 8; i++) {
        ili9488_fill_rect(0, i * 60, ILI9488_WIDTH, 60, colors[i]);
    }

    // Zone centrale pour le texte
    ili9488_fill_rect(8, 192, 304, 96, COLOR_BLACK);

    ili9488_draw_string(14, 200, "ESCAPEBOX",    COLOR_GOLD,  COLOR_BLACK, 3);
    ili9488_draw_string(14, 232, "ILI9488 OK",   COLOR_WHITE, COLOR_BLACK, 2);
    ili9488_draw_string(14, 256, "320x480 18bit", COLOR_CYAN,  COLOR_BLACK, 2);

    ESP_LOGI("ili9488", "test_screen affiché");
}
