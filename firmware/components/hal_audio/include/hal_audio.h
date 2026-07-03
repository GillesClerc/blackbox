#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#define AUDIO_PIN_BCLK    4
#define AUDIO_PIN_LRCK    5
#define AUDIO_PIN_DOUT    6
#define PCM5122_I2C_ADDR 0x4C

// hal_audio — mixer logiciel 4 voix sur un canal I2S unique (task audio_mixer) :
//   1. musique de fond MP3 (boucle, ducking automatique pendant un one-shot)
//   2. one-shot MP3 (voix off / SFX d'un scénario, par-dessus le fond)
//   3. bips synthétiques (queue asynchrone — ne bloque plus l'appelant)
//   4. ambiance à base de tons (fallback sans MP3)
// Toutes les voix sont sommées en 32 bits avec saturation → PCM s16 → I2S.

esp_err_t hal_audio_init(void);

// Bip sinusoïdal avec enveloppe (anti-crissement). Asynchrone : la note est
// mise en file et mixée par-dessus la musique ; l'appel revient immédiatement.
void hal_audio_play_tone(uint16_t freq_hz, uint16_t duration_ms);

// Séquence de notes (asynchrone, mise en file). gap_ms : silence entre notes.
void hal_audio_play_sequence(const uint16_t *freqs, const uint16_t *durs, int count, uint16_t gap_ms);

void hal_audio_set_volume(uint8_t vol_percent);
void hal_audio_set_dsp_filter(uint8_t program);
void hal_audio_set_analog_gain(uint8_t minus6db);
void hal_audio_stop(void);

// ── One-shot MP3 (voix off / SFX de scénario) ────────────────────────────────
// Joue le fichier une fois, mixé par-dessus la musique de fond (le fond est
// automatiquement atténué le temps de la lecture). Un nouvel appel remplace le
// one-shot en cours. MP3 44100 Hz, mono ou stéréo (contrainte des packages —
// vérifiée par tools/package_scenario.py).
// ESP_ERR_NOT_FOUND si le fichier est absent (l'appelant peut alors jouer un
// son de fallback), ESP_FAIL si trop grand (> 4 MB).
esp_err_t hal_audio_play_oneshot(const char *path);

// Coupe le one-shot en cours (no-op si aucun).
void hal_audio_oneshot_stop(void);

bool hal_audio_oneshot_active(void);

// ── Musique de fond ───────────────────────────────────────────────────────────
// Note avec fréquence (0 = silence), durée et gap après.
typedef struct {
    uint16_t freq;
    uint16_t dur_ms;
    uint16_t gap_ms;
} hal_audio_bg_note_t;

// Démarre une boucle de fond à base de tons synthétiques (fallback sans MP3).
void hal_audio_bg_start(const hal_audio_bg_note_t *notes, int count);

// Démarre la musique de fond depuis un buffer MP3 (flash ou PSRAM).
// MP3 : 44100 Hz, mono ou stéréo, bitrate libre. Boucle. Volume discret.
void hal_audio_bg_mp3_start(const uint8_t *mp3_data, size_t mp3_size);

// Arrête la musique de fond (tons ou MP3).
void hal_audio_bg_stop(void);

// Niveau de crête 0-100 du mix sortant (pour visualisation).
uint8_t hal_audio_get_peak_level(void);

// Joue un fichier MP3 (SD/LittleFS) en boucle de fond.
// ESP_ERR_NOT_FOUND si absent, ESP_FAIL si trop grand (> 4 MB).
esp_err_t hal_audio_play_bg(const char *path);
