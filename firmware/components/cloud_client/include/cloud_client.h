#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Client cloud EscapeBox (docs/plans/firmware-cloud-client.md, jalon F1).
//
// Task dédiée (core 0, prio 3) qui parle à l'API web (CONFIG_ESCAPEBOX_API_URL,
// surchargeable par la clé NVS "cloud/api_url" pour tester en local) :
//   1. GET  /api/box/challenge   → nonce
//   2. POST /api/box/auth        → JWT 2h (signature via hal_box_auth_sign)
//   3. GET  /api/box/sync        → scénarios installés + firmware_update
// Le JWT vit en RAM et est renouvelé automatiquement (expiration ou 401).
// L'horloge système est réglée depuis server_time à chaque auth (pas de SNTP).
//
// Tout est non bloquant pour l'appelant : on poste une demande, la task fait
// le travail. Sans WiFi ou sans provisioning (box_creds), les demandes sont
// ignorées avec un warning — la box fonctionne 100 % offline.

// Crée la queue + la task. À appeler une fois au boot depuis app_main,
// après hal_box_auth_init() et hal_wifi_init().
esp_err_t cloud_client_init(void);

// Demande une synchronisation (auth si nécessaire puis GET /api/box/sync).
// Non bloquant. ESP_ERR_INVALID_STATE si init pas faite, ESP_FAIL si la
// queue est pleine.
esp_err_t cloud_client_request_sync(void);

#ifdef __cplusplus
}
#endif
