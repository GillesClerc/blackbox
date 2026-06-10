#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// MPR121 — contrôleur tactile capacitif 12 canaux — NXP
// Utilisé en Phase 1 proto (breakout AliExpress)
// Phase 2 série : remplacé par MTCH2120 directement sur PCB custom
//
// Adresse I2C : 0x5A (ADDR pin à GND)
#define MPR121_ADDR         0x5A
#define MPR121_NUM_CH       12

// Seuils touch/release — augmenter si détection à travers matériau épais
#define MPR121_TOUCH_TH     12
#define MPR121_RELEASE_TH   6

typedef struct {
    uint16_t touched;               // bitmask canaux touchés (bit 0 = ch0 ... bit 11 = ch11)
    bool     ch[MPR121_NUM_CH];     // état individuel par canal
} mpr121_data_t;

esp_err_t mpr121_init(void);
esp_err_t mpr121_read(mpr121_data_t *out);
bool      mpr121_is_touched(uint8_t channel);
int       mpr121_first_touched(void);
