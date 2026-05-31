#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define XPT2046_PIN_CS   14
#define XPT2046_PIN_IRQ  15

esp_err_t xpt2046_init(void);

bool xpt2046_read(uint16_t *x, uint16_t *y);

void xpt2046_lvgl_read(void *indev, void *data);
