#pragma once
#include <stdint.h>
#include "esp_err.h"

#define LEDS_DEFAULT_GPIO  16
#define LEDS_MAX_COUNT     60

// Initialise le canal RMT pour le bus WS2812B.
// gpio_pin : broche data (défaut : LEDS_DEFAULT_GPIO)
// num_leds : nombre de LEDs dans la chaîne
esp_err_t leds_init(int gpio_pin, uint16_t num_leds);

// Fixe la couleur d'une LED (0-indexed). Appliquée au prochain leds_show().
void leds_set(uint16_t idx, uint8_t r, uint8_t g, uint8_t b);

// Applique la même couleur à toutes les LEDs.
void leds_fill(uint8_t r, uint8_t g, uint8_t b);

// Éteint toutes les LEDs.
void leds_clear(void);

// Transmet le buffer courant vers le bus WS2812B.
esp_err_t leds_show(void);

// Helpers couleur hex "#RRGGBB"
void leds_fill_hex(const char *hex, uint8_t brightness);
void leds_set_hex(uint16_t idx, const char *hex, uint8_t brightness);
