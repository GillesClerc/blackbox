#pragma once

#include <stdbool.h>
#include "esp_err.h"
#include "cJSON.h"  // bundlé dans le composant

// Types d'événements capteurs ("on:" dans le JSON)
typedef enum {
    EVT_NONE = 0,
    EVT_RFID_READ,        // uid (str)
    EVT_KEYPAD_CODE,      // code (str)
    EVT_TOUCH,            // channel (int_val) — MTCH2120
    EVT_ROTARY_VALUE,     // valeur (int_val)  — potentiomètre ADC (AS5600 retiré Phase 1)
    EVT_HALL_DETECTED,    // actif (bool_val)
    EVT_BREATH_DETECTED,  // (pas de payload)
    EVT_ACCEL_TILT,       // angle (int_val)   — LSM6DSOTR
} scenario_event_type_t;

typedef struct {
    scenario_event_type_t type;
    union {
        char str[64];      // rfid uid, keypad code
        int  int_val;      // rotary angle, tilt, touch channel
        bool bool_val;     // hall
    };
} scenario_event_t;

// Callback pour exécuter une action.
// action_name : "audio", "screen_main", "led", "servo", etc.
// params      : objet cJSON avec les paramètres (valide uniquement pendant le callback)
typedef void (*scenario_action_cb_t)(const char *action_name, const cJSON *params);

// --- API principale ---

// Charger un scénario depuis une chaîne JSON.
esp_err_t scenario_engine_init(const char *json);

// Enregistrer un handler pour un type d'action.
void scenario_engine_register_action(const char *name, scenario_action_cb_t cb);

// Démarrer la tâche moteur (à appeler après init + register).
esp_err_t scenario_engine_start(void);

// Poster un événement capteur (thread-safe, appelable depuis n'importe quelle tâche).
esp_err_t scenario_engine_post_event(const scenario_event_t *evt);

// Step courant (pour debug/affichage).
const char *scenario_engine_current_step(void);

// --- Variables runtime ---

// Stocker/lire une variable (toujours des strings en interne).
void        scenario_var_set(const char *name, const char *value);
void        scenario_var_set_int(const char *name, int value);
const char *scenario_var_get(const char *name);
int         scenario_var_get_int(const char *name, int default_val);
