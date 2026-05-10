#include "st7735.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define TAG "display"

static spi_device_handle_t s_spi;
static gpio_num_t s_pin_dc  = DISPLAY_PIN_DC;
static gpio_num_t s_pin_rst = DISPLAY_PIN_RST;

// --- SPI low-level ---

static void spi_send(const uint8_t *data, int len, bool is_cmd) {
    gpio_set_level(s_pin_dc, is_cmd ? 0 : 1);
    spi_transaction_t t = {
        .length = len * 8,
        .tx_buffer = data,
    };
    spi_device_polling_transmit(s_spi, &t);
}

static inline void send_cmd(uint8_t cmd) {
    spi_send(&cmd, 1, true);
}

static inline void send_data(uint8_t data) {
    spi_send(&data, 1, false);
}

// --- Séquence init ST7735 ---

static void st7735_init_sequence(void) {
    // Software reset
    send_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(150));

    // Sleep out
    send_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(255));

    // Frame rate
    send_cmd(0xB1);
    send_data(0x01); send_data(0x2C); send_data(0x2D);
    send_cmd(0xB2);
    send_data(0x01); send_data(0x2C); send_data(0x2D);
    send_cmd(0xB3);
    send_data(0x01); send_data(0x2C); send_data(0x2D);
    send_data(0x01); send_data(0x2C); send_data(0x2D);

    // Inversion control: column
    send_cmd(0xB4); send_data(0x07);

    // Power sequence
    send_cmd(0xC0); send_data(0xA2); send_data(0x02); send_data(0x84);
    send_cmd(0xC1); send_data(0xC5);
    send_cmd(0xC2); send_data(0x0A); send_data(0x00);
    send_cmd(0xC3); send_data(0x8A); send_data(0x2A);
    send_cmd(0xC4); send_data(0x8A); send_data(0xEE);
    send_cmd(0xC5); send_data(0x0E);

    // Inversion off
    send_cmd(0x20);

    // Memory access: MX, MV => portrait 128x160
    send_cmd(0x36); send_data(0xC8);

    // Color mode: 16-bit/pixel
    send_cmd(0x3A); send_data(0x05);

    // Column address set
    send_cmd(0x2A); send_data(0x00); send_data(0x00); send_data(0x00); send_data(0x7F);
    // Row address set
    send_cmd(0x2B); send_data(0x00); send_data(0x00); send_data(0x00); send_data(0x9F);

    // Gamma
    send_cmd(0xE0);
    send_data(0x02); send_data(0x1C); send_data(0x07); send_data(0x12);
    send_data(0x37); send_data(0x32); send_data(0x29); send_data(0x2D);
    send_data(0x29); send_data(0x25); send_data(0x2B); send_data(0x39);
    send_data(0x00); send_data(0x01); send_data(0x03); send_data(0x10);

    send_cmd(0xE1);
    send_data(0x03); send_data(0x1D); send_data(0x07); send_data(0x06);
    send_data(0x2E); send_data(0x2C); send_data(0x29); send_data(0x2D);
    send_data(0x2E); send_data(0x2E); send_data(0x37); send_data(0x3F);
    send_data(0x00); send_data(0x00); send_data(0x02); send_data(0x10);

    // Display on
    send_cmd(0x13);
    vTaskDelay(pdMS_TO_TICKS(10));
    send_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(100));
}

// --- API publique ---

void display_init(void) {
    // Config GPIO DC et RST
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << s_pin_dc) | (1ULL << s_pin_rst),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);

    // Reset hardware
    gpio_set_level(s_pin_rst, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(s_pin_rst, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Init bus SPI
    spi_bus_config_t bus = {
        .mosi_io_num = DISPLAY_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = DISPLAY_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    // Attache le device ST7735
    spi_device_interface_config_t dev = {
        .clock_speed_hz = 40 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = DISPLAY_PIN_CS,
        .queue_size = 7,
        .flags = SPI_DEVICE_NO_DUMMY,  // write-only, pas de dummy bits nécessaires
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev, &s_spi));

    st7735_init_sequence();
    ESP_LOGI(TAG, "ST7735 initialisé (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    send_cmd(0x2A);
    send_data(0x00); send_data(x0);
    send_data(0x00); send_data(x1);

    send_cmd(0x2B);
    send_data(0x00); send_data(y0);
    send_data(0x00); send_data(y1);

    send_cmd(0x2C);  // Write RAM
}

void display_fill(uint16_t color) {
    display_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    set_window(x, y, x + w - 1, y + h - 1);

    uint32_t total = w * h;
    // Buffer de 128 pixels pour envoyer par blocs
    uint8_t buf[256];
    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;
    for (int i = 0; i < 256; i += 2) {
        buf[i] = hi;
        buf[i + 1] = lo;
    }

    gpio_set_level(s_pin_dc, 1);  // data
    uint32_t remaining = total;
    while (remaining > 0) {
        uint32_t chunk = remaining > 128 ? 128 : remaining;
        spi_transaction_t t = {
            .length = chunk * 16,
            .tx_buffer = buf,
        };
        spi_device_polling_transmit(s_spi, &t);
        remaining -= chunk;
    }
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color) {
    set_window(x, y, x, y);
    uint8_t data[2] = { color >> 8, color & 0xFF };
    spi_send(data, 2, false);
}

void display_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data) {
    set_window(x, y, x + w - 1, y + h - 1);
    gpio_set_level(s_pin_dc, 1);
    uint32_t total = w * h;
    for (uint32_t i = 0; i < total; i++) {
        uint8_t buf[2] = { data[i] >> 8, data[i] & 0xFF };
        spi_transaction_t t = { .length = 16, .tx_buffer = buf };
        spi_device_polling_transmit(s_spi, &t);
    }
}

static const uint8_t sin_lut[256] = {
    127, 130, 133, 136, 139, 143, 146, 149, 152, 155, 158, 161, 164, 167, 170, 173,
    176, 179, 182, 184, 187, 190, 193, 195, 198, 200, 203, 205, 208, 210, 213, 215,
    217, 219, 221, 224, 226, 228, 229, 231, 233, 235, 236, 238, 239, 241, 242, 244,
    245, 246, 247, 248, 249, 250, 251, 251, 252, 253, 253, 254, 254, 254, 254, 254,
    255, 254, 254, 254, 254, 254, 253, 253, 252, 251, 251, 250, 249, 248, 247, 246,
    245, 244, 242, 241, 239, 238, 236, 235, 233, 231, 229, 228, 226, 224, 221, 219,
    217, 215, 213, 210, 208, 205, 203, 200, 198, 195, 193, 190, 187, 184, 182, 179,
    176, 173, 170, 167, 164, 161, 158, 155, 152, 149, 146, 143, 139, 136, 133, 130,
    127, 124, 121, 118, 115, 111, 108, 105, 102,  99,  96,  93,  90,  87,  84,  81,
     78,  75,  72,  70,  67,  64,  61,  59,  56,  54,  51,  49,  46,  44,  41,  39,
     37,  35,  33,  30,  28,  26,  25,  23,  21,  19,  18,  16,  15,  13,  12,  10,
      9,   8,   7,   6,   5,   4,   3,   3,   2,   1,   1,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   1,   1,   2,   3,   3,   4,   5,   6,   7,   8,
      9,  10,  12,  13,  15,  16,  18,  19,  21,  23,  25,  26,  28,  30,  33,  35,
     37,  39,  41,  44,  46,  49,  51,  54,  56,  59,  61,  64,  67,  70,  72,  75,
     78,  81,  84,  87,  90,  93,  96,  99, 102, 105, 108, 111, 115, 118, 121, 124,
};

static uint16_t s_framebuf[DISPLAY_WIDTH * DISPLAY_HEIGHT];

// Effet plasma optimisé : LUT sin, framebuffer complet, DMA
void display_demo_plasma(void) {
    static uint8_t frame = 0;

    for (uint16_t y = 0; y < DISPLAY_HEIGHT; y++) {
        for (uint16_t x = 0; x < DISPLAY_WIDTH; x++) {
            uint8_t v = (sin_lut[(x + frame) & 0xFF]
                       + sin_lut[(y + frame / 2) & 0xFF]
                       + sin_lut[((x + y) / 2 + frame) & 0xFF]) / 3;

            uint8_t r = sin_lut[(v + frame) & 0xFF];
            uint8_t g = sin_lut[(v + frame + 85) & 0xFF];
            uint8_t b = sin_lut[(v + frame + 170) & 0xFF];
            s_framebuf[y * DISPLAY_WIDTH + x] =
                ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        }
    }

    set_window(0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1);
    gpio_set_level(s_pin_dc, 1);
    spi_transaction_t t = {
        .length = sizeof(s_framebuf) * 8,
        .tx_buffer = s_framebuf,
    };
    spi_device_transmit(s_spi, &t);

    frame += 3;
}
