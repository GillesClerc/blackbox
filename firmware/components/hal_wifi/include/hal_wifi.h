#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Initialise la stack WiFi en mode station et lit les identifiants depuis la NVS
// (namespace "wifi_creds", clés "ssid"/"pass", provisionnés par
// tools/provision_box.py). Ne déclenche pas encore la connexion.
// Renvoie ESP_ERR_NVS_NOT_FOUND si aucun identifiant WiFi n'est provisionné —
// la box doit pouvoir démarrer sans réseau.
esp_err_t hal_wifi_init(void);

// Lance la connexion et bloque jusqu'à l'obtention d'une IP ou l'expiration du
// délai. Renvoie ESP_OK si connectée, ESP_FAIL sinon.
esp_err_t hal_wifi_connect(uint32_t timeout_ms);

// Applique de nouveaux identifiants (provisioning BLE) : persiste en NVS
// "wifi_creds", initialise la stack si besoin, déconnecte l'éventuelle session
// en cours et configure la STA. Ne connecte PAS — appeler hal_wifi_connect()
// ensuite. pass peut être vide (réseau ouvert). Appelable depuis un task
// runtime : retourne l'erreur, n'abort jamais.
esp_err_t hal_wifi_set_credentials(const char *ssid, const char *pass);

bool hal_wifi_is_connected(void);

#ifdef __cplusplus
}
#endif
