#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t config_manager_init(void);

uint8_t  config_get_volume(void);
void     config_set_volume(uint8_t percent);

uint8_t  config_get_brightness(void);
void     config_set_brightness(uint8_t percent);

bool     config_get_dev_mode(void);
void     config_set_dev_mode(bool enabled);
