#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Provisioning BLE (F5) : fenêtre d'appairage GATT pour configurer le WiFi
// d'une box et prouver sa possession (option B du register).
//
// Service 128-bit : e5c40001-5c25-4b10-8f46-6b9c30ac7a11 — caractéristiques
// (même base, octets 4-5 = numéro) :
//   0002 box_uid        READ    — "ESP32S3-XXXX-XXXX" (ou "UNPROVISIONED")
//   0003 wifi_ssid      WRITE   — écrire AVANT wifi_pass
//   0004 wifi_pass      WRITE   — déclenche la connexion ("" = réseau ouvert)
//   0005 status         READ+NOTIFY — "idle"/"connecting"/"wifi_ok"/"wifi_fail"
//   0006 auth_challenge WRITE   — nonce hex du serveur
//   0007 auth_response  READ+NOTIFY — HMAC hex (preuve de possession)
// Client de référence : page /devices/add (Web Bluetooth) sur box.agill.es.
// wifi_pass est write-only et n'est jamais loggé.
//
// La fenêtre se ferme seule après window_s (BLE arrêté, RAM contrôleur rendue).
// Déclencheurs : boot sans NVS wifi_creds, ou touche keypad maintenue au boot.

typedef void (*ble_prov_wifi_ok_cb_t)(void);

// Démarre la fenêtre d'appairage (advertising "EscapeBox-XXXX").
// on_wifi_ok (optionnel) est appelé depuis le worker après une connexion WiFi
// réussie — typiquement pour déclencher un sync cloud. Appelable depuis un
// task runtime : retourne l'erreur, n'abort jamais. No-op si déjà active.
esp_err_t ble_prov_start(uint32_t window_s, ble_prov_wifi_ok_cb_t on_wifi_ok);

bool ble_prov_is_active(void);

#ifdef __cplusplus
}
#endif
