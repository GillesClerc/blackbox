#pragma once
#include <stdint.h>
#include "esp_err.h"

// LSM6DSOTR — IMU 6 axes (accéléromètre + gyroscope) — STMicroelectronics
// I2C : 0x6A (SDO=GND) ou 0x6B (SDO=VDD)
#define LSM6_ADDR_DEFAULT  0x6A

typedef struct {
    float ax, ay, az;  // accélération (m/s²)
    float gx, gy, gz;  // vitesse angulaire (°/s)
    float temp_c;       // température (°C)
} hal_imu_data_t;

// Initialise le capteur. Retourne ESP_ERR_NOT_FOUND si WHO_AM_I incorrect.
esp_err_t hal_imu_init(uint8_t addr);

// Lit les 6 axes + température.
esp_err_t hal_imu_read(hal_imu_data_t *out);

// Calcule l'angle d'inclinaison avant (pitch) en degrés à partir de ax/az.
float hal_imu_pitch_deg(const hal_imu_data_t *d);

// Met le capteur en standby (économie d'énergie).
esp_err_t hal_imu_sleep(void);
esp_err_t hal_imu_wake(void);
