#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Identifiants d'authentification de la box vis-à-vis du serveur EscapeBox.
//
// Le secret par box n'est JAMAIS embarqué dans le firmware : il est provisionné
// dans NVS (namespace "box_creds") par tools/provision_box.py, qui le dérive du
// BOX_MASTER_SECRET serveur via HKDF. Le firmware ne fait que le lire et signer.
//
// Flux serveur (cf. web/app/api/box/) :
//   1. GET  /api/box/challenge?box_uid=<uid>      -> challenge (nonce)
//   2. POST /api/box/auth  { box_uid, challenge,
//        challenge_response = hal_box_auth_sign(challenge) }  -> JWT 2h
//   3. GET  /api/box/sync  (Authorization: Bearer <JWT>)
// Les étapes réseau nécessitent un stack WiFi/HTTP (pas encore implémenté).

// Longueur de la réponse HMAC-SHA256 en hex (sans le terminateur).
#define HAL_BOX_AUTH_SIG_HEX_LEN 64

// Charge box_uid + secret depuis NVS. À appeler après l'init NVS
// (config_manager_init). Retourne ESP_ERR_NVS_NOT_FOUND si la box n'a pas été
// provisionnée — non fatal : la box fonctionne, l'auth serveur est juste
// indisponible.
esp_err_t hal_box_auth_init(void);

// true si la box dispose de ses identifiants (uid + secret).
bool hal_box_auth_is_provisioned(void);

// box_uid (ex: "ESP32S3-A1B2-C3D4"), ou NULL si non provisionnée.
const char *hal_box_auth_uid(void);

// Calcule challenge_response = HMAC-SHA256(secret, "<box_uid>:<challenge>"),
// écrit en hex minuscule terminé par '\0' dans out_hex.
// out_len doit valoir au moins HAL_BOX_AUTH_SIG_HEX_LEN + 1 (65).
// Retourne ESP_ERR_INVALID_STATE si non provisionnée.
esp_err_t hal_box_auth_sign(const char *challenge, char *out_hex, size_t out_len);
