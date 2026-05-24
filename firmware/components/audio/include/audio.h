#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

// Pins I2S vers PCM5122PW (modifiables via menuconfig si besoin)
#define AUDIO_PIN_BCLK    4   // ESP32-S3 DevKitC-1
#define AUDIO_PIN_LRCK    5
#define AUDIO_PIN_DOUT    6

// Adresse I2C du PCM5122PW (ADR1=0, ADR2=0)
#define PCM5122_I2C_ADDR 0x4C

// Initialise le DAC PCM5122PW via I2C + le périphérique I2S master.
// bus : handle du bus I2C partagé (initialisé par i2c_bus_init()).
esp_err_t audio_init(i2c_master_bus_handle_t bus);

// Joue des samples PCM 16-bit stéréo entrelacés (L, R, L, R...).
// Bloquant jusqu'à la fin de la transmission.
esp_err_t audio_play_raw(const int16_t *samples, size_t num_samples, uint32_t sample_rate_hz);

// Génère un bip sinusoïdal (approx. carré) à la fréquence donnée.
void audio_play_tone(uint16_t freq_hz, uint16_t duration_ms);

// Volume : 0 = muet, 100 = 0 dBFS nominal.
void audio_set_volume(uint8_t vol_percent);

// Coupe la sortie (mise en standby du DAC).
void audio_stop(void);
