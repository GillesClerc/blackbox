#pragma once
#include "esp_err.h"

// Menu de démarrage « boutons + visage » (décision design 2026-06-09) :
// navigation au keypad, le personnage répond par les yeux, les LEDs et des
// bips (le display bouche et les annonces vocales viendront avec les assets).
//
// Au boot, la box lance une invite de 4 s (double bip + LEDs douces). Sans
// toucher → jeu direct (contrainte FSD : premier déballage sans setup).
// Une touche pendant l'invite → menu :
//   ◀ = touche 4   ▶ = touche 6   ✓ = touche 11 (#)
//   items : JOUER (vert) · SCÉNARIO (ambre, ✓ = suivant, persisté en NVS
//   cloud/active_scenario) · APPAIRAGE BLE (bleu, ✓ = fenêtre 5 min)
// Sortie : ✓ sur JOUER ou 30 s d'inactivité.
//
// À appeler depuis app_main APRÈS hal_audio/hal_leds/ui_face/SD et AVANT le
// chargement du scénario (la sélection NVS doit précéder le chargeur) et
// AVANT la création de touch_task (un seul lecteur hal_touch à la fois).
// on_ble_wifi_ok est transmis à ble_prov_start (item appairage).
esp_err_t boot_menu_maybe_run(void (*on_ble_wifi_ok)(void));
