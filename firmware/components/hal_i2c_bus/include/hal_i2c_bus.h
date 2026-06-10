// hal_i2c_bus — owner of the shared I2C bus (sensors + audio DAC backbone).
// Public API: boot-time init only. HAL implementations needing the bus
// handle include hal_i2c_bus_priv.h instead — never application code.
#pragma once
#include "esp_err.h"

// Initialise le bus I2C master (à appeler une fois depuis app_main).
esp_err_t hal_i2c_bus_init(void);
