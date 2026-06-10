// hal_touch — capteur tactile capacitif 12 canaux, API unique.
// Backends : MTCH2120 @0x28 (PCB Phase 2) ou MPR121 @0x5A (breakout Phase 1),
// sélectionné par probe I2C à l'init.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define HAL_TOUCH_NUM_CH  12

typedef struct {
    uint16_t touched;                // bitmask canaux touchés (bit 0 = ch0 ... bit 11 = ch11)
    bool     ch[HAL_TOUCH_NUM_CH];   // état individuel par canal
} hal_touch_data_t;

// Probe + init du backend présent sur le bus. Appelable depuis un task
// runtime : retourne l'erreur, n'abort jamais (ESP_ERR_NOT_FOUND si aucun chip).
esp_err_t hal_touch_init(void);

// Lit l'état de tous les canaux. Thread-safety : un seul task lecteur (touch_task).
esp_err_t hal_touch_read(hal_touch_data_t *out);

// État du dernier read pour le canal donné.
bool hal_touch_is_touched(uint8_t channel);

// Premier canal touché (0-11), ou -1 si aucun.
int hal_touch_first_touched(void);
