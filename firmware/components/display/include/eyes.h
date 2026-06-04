#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_lcd_types.h"

#define EYE_WIDTH   240
#define EYE_HEIGHT  240

typedef enum {
    EYE_LEFT  = 0,
    EYE_RIGHT = 1,
    EYE_COUNT = 2,
} eye_id_t;

// Couleurs RGB565
#define EYE_BLACK   0x0000
#define EYE_WHITE   0xFFFF
#define EYE_RED     0xF800
#define EYE_GREEN   0x07E0
#define EYE_BLUE    0x001F
#define EYE_YELLOW  0xFFE0
#define EYE_CYAN    0x07FF
#define EYE_MAGENTA 0xF81F
#define EYE_ORANGE  0xFD20
#define EYE_GOLD    0xFEA0

// Pins ESP32-S3 (cf. docs/escapebox-fsd.md section 3.2.2).
#define EYES_PIN_MOSI    38
#define EYES_PIN_SCLK    39
#define EYES_PIN_DC      41
#define EYES_PIN_RST     42
#define EYES_PIN_CS_L    40
#define EYES_PIN_CS_R    14

esp_err_t eyes_init(void);

esp_lcd_panel_handle_t eyes_panel(eye_id_t eye);

// Bloque jusqu'à ce que la transaction draw_bitmap qui vient d'être lancée
// vers `eye` soit terminée (signalée par l'ISR on_color_trans_done).
// Pattern : esp_lcd_panel_draw_bitmap(...) → eyes_wait_done(eye).
// À l'issue, le framebuffer DMA passé peut être réutilisé sans risque.
esp_err_t eyes_wait_done(eye_id_t eye);

esp_err_t eyes_fill(eye_id_t eye, uint16_t color);
esp_err_t eyes_fill_all(uint16_t color);

// Copie `pixels` (RGB565 big-endian, contigus row-major) dans la zone
// [x, y, x+w-1, y+h-1] de l'écran sélectionné. Doit être en SRAM interne
// pour DMA (cf. heap_caps_malloc(..., MALLOC_CAP_DMA)).
esp_err_t eyes_draw(eye_id_t eye, int x, int y, int w, int h, const uint16_t *pixels);
