#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// AS5600 — encodeur magnétique angulaire 12 bits — ams OSRAM
// Adresse I2C fixe : 0x36
#define AS5600_ADDR  0x36

typedef struct {
    uint16_t raw_angle;  // 0-4095 (0-360°)
    uint16_t angle;      // 0-4095, avec start/stop position appliqués
    float    angle_deg;  // 0.0-359.9°
    bool     magnet_ok;  // aimant détecté et dans la plage
} as5600_data_t;

// Initialise le driver. Retourne ESP_ERR_NOT_FOUND si aimant absent.
esp_err_t as5600_init(i2c_master_bus_handle_t bus);

// Lit l'angle courant.
esp_err_t as5600_read(as5600_data_t *out);

// Retourne l'angle en degrés (0-360), ou -1.0 si erreur.
float as5600_angle_deg(void);
