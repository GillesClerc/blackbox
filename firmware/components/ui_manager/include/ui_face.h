#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    UI_FACE_IDLE = 0,   // mouvement aléatoire + clignement autonome
    UI_FACE_HAPPY,      // paupières légèrement plissées en bas
    UI_FACE_SAD,        // paupières supérieures basses
    UI_FACE_SURPRISED,  // pupille dilatée + paupières grand ouvert
    UI_FACE_SLEEPY,     // paupières à moitié fermées
    UI_FACE_ANGRY,      // paupières supérieures basses + pupille contractée
    UI_FACE_CLOSED,     // yeux fermés
} ui_face_emotion_t;

typedef enum {
    UI_FACE_LOOK_CENTER = 0,
    UI_FACE_LOOK_LEFT,
    UI_FACE_LOOK_RIGHT,
    UI_FACE_LOOK_UP,
    UI_FACE_LOOK_DOWN,
} ui_face_direction_t;

esp_err_t ui_face_init(void);
esp_err_t ui_face_start(void);

void ui_face_set_emotion(ui_face_emotion_t e);
void ui_face_look(ui_face_direction_t dir);
void ui_face_blink(void);
