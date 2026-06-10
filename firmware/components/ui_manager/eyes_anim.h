#pragma once

#include <stdint.h>
#include "hal_display.h"   // EYE_LEFT/EYE_RIGHT, EYE_WIDTH/EYE_HEIGHT

// Modulations exposées par ui_face pour customiser le rendu sans toucher
// au pipeline Uncanny Eyes interne.
typedef struct {
    int16_t  lid_top_bias;     // -128..127, ajoute au seuil upper (positif = ferme + haut)
    int16_t  lid_bot_bias;     // -128..127, ajoute au seuil lower (positif = ferme + bas)
    int16_t  iris_scale_bias;  // -200..200, biais sur dilatation pupille
    bool     forced_closed;    // si true, paupières maintenues fermées (~yeux ferm.)
    bool     forced_blink;     // si true, déclenche un clignement immédiat
    int16_t  look_x;           // -512..512 (offset en demi-unités du raw 0-1023)
    int16_t  look_y;
    bool     look_forced;      // si false : mouvement aléatoire autonome
} eyes_anim_state_t;

// Reset to safe defaults (zeros).
void eyes_anim_state_init(eyes_anim_state_t *s);

// Init pipeline (utilise hal_display_init() côté driver — doit avoir été appelé avant).
esp_err_t eyes_anim_init(void);

// Effectue 1 frame pour 1 œil (alterne L/R en interne). Retourne ESP_OK.
// État partagé lu via les paramètres pour rester thread-safe.
void eyes_anim_step(const eyes_anim_state_t *st);
