#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define ILI9488_WIDTH   320
#define ILI9488_HEIGHT  480

// Pins ESP32-S3 — MOSI/SCK sur SPI2 IOMUX (GPIO11/12) pour perf maximale
#define ILI9488_PIN_MOSI  11
#define ILI9488_PIN_SCK   12
#define ILI9488_PIN_CS    10
#define ILI9488_PIN_DC     9
#define ILI9488_PIN_RST    8

// Couleurs RGB565 (converties en RGB666 3 bytes/pixel à l'envoi)
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_ORANGE  0xFD20
#define COLOR_GRAY    0x8410
#define COLOR_GOLD    0xFEA0

esp_err_t ili9488_init(void);
void ili9488_fill(uint16_t color);
void ili9488_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ili9488_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void ili9488_draw_char(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void ili9488_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t fg, uint16_t bg, uint8_t scale);
void ili9488_test_screen(void);

// LVGL flush callback : convertit RGB565 → RGB666 et envoie via SPI DMA.
// Passer comme flush_cb à lv_display_set_flush_cb().
void ili9488_lvgl_flush(void *disp, const void *area, uint8_t *color_map);
