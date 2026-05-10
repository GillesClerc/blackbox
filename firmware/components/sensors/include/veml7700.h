#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// VEML7700 — capteur de lumière ambiante — Vishay
// Adresse I2C fixe : 0x10
#define VEML7700_ADDR  0x10

typedef struct {
    float lux;        // luminance calculée (lux)
    uint16_t als_raw; // valeur brute du canal ALS
    uint16_t white;   // valeur brute du canal white
} veml7700_data_t;

// Initialise et démarre les mesures (temps d'intégration 100ms).
esp_err_t veml7700_init(i2c_master_bus_handle_t bus);

// Lit la luminosité courante.
esp_err_t veml7700_read(veml7700_data_t *out);

// Retourne la luminosité en lux, ou -1.0 si erreur.
float veml7700_lux(void);
