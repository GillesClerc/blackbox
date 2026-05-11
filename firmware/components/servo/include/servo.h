#pragma once
#include <stdint.h>
#include "esp_err.h"

// SG90 via MCPWM — 50Hz, résolution 1µs
// Servo 0 : GPIO1 (compartiment principal)
// Servo 1 : GPIO2 (compartiment secondaire)
#define SERVO_GPIO_0        1
#define SERVO_GPIO_1        2

#define SERVO_PULSE_MIN_US  500     // 0°   (fermé)
#define SERVO_PULSE_MAX_US  2400    // 180° (ouvert)
#define SERVO_ANGLE_OPEN    180.0f
#define SERVO_ANGLE_CLOSED  0.0f

// Initialise les deux servos (timer MCPWM partagé, 50Hz).
esp_err_t servo_init(void);

// Positionne un servo à l'angle voulu (0.0 – 180.0 degrés).
esp_err_t servo_set_angle(uint8_t id, float angle);

// Raccourcis sémantiques pour le moteur de scénario.
esp_err_t servo_open(uint8_t id);
esp_err_t servo_close(uint8_t id);
