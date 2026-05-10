#pragma once
#include <stdint.h>

#define DISPLAY_WIDTH  128
#define DISPLAY_HEIGHT 160

// Pins (modifiables via menuconfig si besoin)
#define DISPLAY_PIN_CLK   18
#define DISPLAY_PIN_MOSI  23
#define DISPLAY_PIN_CS     5
#define DISPLAY_PIN_DC     2
#define DISPLAY_PIN_RST    4

// Couleurs RGB565 courantes
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0

void display_init(void);
void display_fill(uint16_t color);
void display_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void display_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
void display_demo_plasma(void);
