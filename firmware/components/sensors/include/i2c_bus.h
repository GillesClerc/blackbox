#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

// Pins I2C partagées par tous les capteurs + audio DAC
#define I2C_BUS_SDA  21
#define I2C_BUS_SCL  42   // GPIO22 absent sur DevKitC-1 — GPIO42 disponible
#define I2C_BUS_FREQ 400000  // 400 kHz Fast Mode

// Initialise le bus I2C master (à appeler une fois depuis app_main).
esp_err_t i2c_bus_init(void);

// Retourne le handle bus (singleton).
i2c_master_bus_handle_t i2c_bus_handle(void);

// Helpers bas niveau pour les drivers de capteurs.
esp_err_t i2c_bus_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val);
esp_err_t i2c_bus_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out);
esp_err_t i2c_bus_read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len);
