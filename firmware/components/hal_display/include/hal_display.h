// hal_display — 2× GC9A01 240×240 (yeux) sur SPI3.
// API opaque : aucun type esp_lcd n'est exposé, le rendu passe par
// hal_display_draw_eye() (framebuffer RGB565 → DMA, bloquant jusqu'à fin
// de transfert).
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

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

// Init boot depuis app_main (ESP_ERROR_CHECK autorisé en interne).
esp_err_t hal_display_init(void);

esp_err_t hal_display_fill_eye(eye_id_t eye, uint16_t color);
esp_err_t hal_display_fill_all(uint16_t color);

// Copie `pixels` (RGB565 big-endian, contigus row-major) dans la zone
// [x, y, x+w-1, y+h-1] de l'écran sélectionné, puis bloque jusqu'à la fin
// du transfert DMA (le framebuffer est réutilisable au retour).
// `pixels` doit être en SRAM interne DMA-capable (MALLOC_CAP_DMA).
// Thread-safety : un seul task de rendu par œil (eye_task).
esp_err_t hal_display_draw_eye(eye_id_t eye, int x, int y, int w, int h, const uint16_t *pixels);
