#pragma once
#include "cJSON.h"
#include "esp_err.h"

// Validation structurelle d'un scénario JSON, AVANT de le confier au moteur.
//
// Le scénario peut venir de la SD (package cloud ou copie manuelle) : un JSON
// syntaxiquement valide mais mal typé (ex. `code: 7394` non quoté en YAML →
// nombre) faisait crasher le moteur (strcmp sur NULL). Ce module ne dépend que
// de cJSON (testable sur host) et vérifie ce que le moteur suppose :
//   - "steps" : tableau non vide d'objets ;
//   - chaque step : "id" chaîne non vide et unique, "type" connu ;
//   - trigger/input : "on" = événement connu, "expect" objet, expect.uid /
//     expect.code chaînes, timeout_sec nombre ;
//   - listes d'actions (do, do_success, do_fail, do_timeout, hints[].do) :
//     tableaux d'objets ;
//   - branch : conditions = objets {var: chaîne, op: connu, value, next} ;
//   - next / next_timeout / default / conditions[].next : chaîne désignant un
//     step existant ("end" accepté : fin du scénario).
// Règles alignées sur tools/yaml2json.py (validation côté auteur).
//
// Retourne ESP_OK, ou ESP_ERR_INVALID_ARG en loggant la première erreur.
esp_err_t scenario_validate(const cJSON *root);
