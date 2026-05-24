#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

#define AUDIO_PIN_BCLK    4
#define AUDIO_PIN_LRCK    5
#define AUDIO_PIN_DOUT    6
#define PCM5122_I2C_ADDR 0x4C

esp_err_t audio_init(i2c_master_bus_handle_t bus);
esp_err_t audio_play_raw(const int16_t *samples, size_t num_samples, uint32_t sample_rate_hz);

// Joue un bip sinusoïdal avec enveloppe (anti-crissement).
void audio_play_tone(uint16_t freq_hz, uint16_t duration_ms);

// Joue une séquence de notes en une seule opération foreground (pas d'interruption bg entre notes).
// gap_ms : silence entre chaque note.
void audio_play_sequence(const uint16_t *freqs, const uint16_t *durs, int count, uint16_t gap_ms);

void audio_set_volume(uint8_t vol_percent);
void audio_stop(void);

// ── Musique de fond ───────────────────────────────────────────────────────────
// Note avec fréquence (0 = silence), durée et gap après.
typedef struct {
    uint16_t freq;
    uint16_t dur_ms;
    uint16_t gap_ms;
} audio_bg_note_t;

// Démarre une boucle de fond à base de tons synthétiques (fallback sans MP3).
void audio_bg_start(const audio_bg_note_t *notes, int count);

// Démarre la musique de fond depuis un buffer MP3 embarqué en flash.
// MP3 : 44100 Hz recommandé, mono ou stéréo, bitrate libre.
// Volume appliqué automatiquement (discret, ~15 % amplitude).
void audio_bg_mp3_start(const uint8_t *mp3_data, size_t mp3_size);

// Arrête la musique de fond (tons ou MP3).
void audio_bg_stop(void);
