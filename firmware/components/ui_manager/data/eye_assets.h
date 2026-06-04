// Wrapper qui rend defaultEye.h utilisable en ESP-IDF natif (sans PROGMEM).
//
// Le fichier defaultEye.h vient du projet Adafruit "Uncanny Eyes" (MIT,
// Phil Burgess / Paint Your Dragon, 2017+) et de son port GC9A01 par
// thelastoutpostworkshop. On le garde *pixel-identique* — seuls les
// macros Arduino sont neutralisées ci-dessous.
//
// Format embarqué (par œil, dans defaultEye.h) :
//   - sclera[SCLERA_H × SCLERA_W]                uint16_t RGB565
//   - iris[IRIS_MAP_H × IRIS_MAP_W]              uint16_t RGB565 (déplié polaire)
//   - upper[SCREEN_H × SCREEN_W]                 uint8_t  seuil paupière haut
//   - lower[SCREEN_H × SCREEN_W]                 uint8_t  seuil paupière bas
//   - polar[IRIS_H × IRIS_W]                     uint16_t LUT polaire iris

#pragma once

// Sur ESP32 les constantes `const` vont déjà en flash (.rodata), pas besoin
// de PROGMEM. On le neutralise pour pouvoir inclure le .h Adafruit tel quel.
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t  *)(addr))
#endif
#ifndef pgm_read_word
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#endif

#include <stdint.h>
#include "defaultEye.h"

// Bornes de dilatation iris : si non définies par l'asset, valeurs par défaut
// (cf. config.h Adafruit — œil humain standard).
#ifndef IRIS_MIN
#define IRIS_MIN  90
#endif
#ifndef IRIS_MAX
#define IRIS_MAX 130
#endif
