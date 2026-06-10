#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// MTCH2120 — contrôleur tactile capacitif 12 canaux — Microchip
// Adresse I2C : 0x28
#define MTCH2120_ADDR       0x28
#define MTCH2120_NUM_CH     12

typedef struct {
    uint16_t touched;       // bitmask des canaux touchés (bit 0 = ch0 ... bit 11 = ch11)
    bool     ch[MTCH2120_NUM_CH];  // état individuel par canal
} mtch2120_data_t;

// Initialise le contrôleur.
esp_err_t mtch2120_init(void);

// Lit l'état de tous les canaux.
esp_err_t mtch2120_read(mtch2120_data_t *out);

// Retourne true si le canal donné est touché.
bool mtch2120_is_touched(uint8_t channel);

// Retourne le numéro du premier canal touché, ou -1 si aucun.
int mtch2120_first_touched(void);
