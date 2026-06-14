#include "hal_box_auth.h"
#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "psa/crypto.h"

#define TAG "box_auth"

#define NVS_NAMESPACE  "box_creds"
#define KEY_UID        "box_uid"
#define KEY_SECRET     "box_secret"

#define UID_MAX        64   // aligné sur BOX_UID_RE serveur ({4,64})
#define SECRET_LEN     32   // HKDF-SHA256 -> 32 octets

static char    s_uid[UID_MAX + 1];
static uint8_t s_secret[SECRET_LEN];
static bool    s_provisioned;

esp_err_t hal_box_auth_init(void)
{
    s_provisioned = false;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "namespace '%s' absent: %s", NVS_NAMESPACE,
                 esp_err_to_name(err));
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t uid_len = sizeof(s_uid);
    err = nvs_get_str(nvs, KEY_UID, s_uid, &uid_len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "box_uid absent: %s", esp_err_to_name(err));
        nvs_close(nvs);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    size_t secret_len = sizeof(s_secret);
    err = nvs_get_blob(nvs, KEY_SECRET, s_secret, &secret_len);
    nvs_close(nvs);
    if (err != ESP_OK || secret_len != SECRET_LEN) {
        ESP_LOGW(TAG, "box_secret invalide (%s, len=%u)",
                 esp_err_to_name(err), (unsigned)secret_len);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    s_provisioned = true;
    ESP_LOGI(TAG, "box provisionnée: %s", s_uid);
    return ESP_OK;
}

bool hal_box_auth_is_provisioned(void)
{
    return s_provisioned;
}

const char *hal_box_auth_uid(void)
{
    return s_provisioned ? s_uid : NULL;
}

esp_err_t hal_box_auth_sign(const char *challenge, char *out_hex, size_t out_len)
{
    if (!s_provisioned) return ESP_ERR_INVALID_STATE;
    if (!challenge || !out_hex || out_len < HAL_BOX_AUTH_SIG_HEX_LEN + 1) {
        return ESP_ERR_INVALID_ARG;
    }

    // Message signé : "<box_uid>:<challenge>" (identique au serveur).
    char msg[UID_MAX + 1 + 128];
    int n = snprintf(msg, sizeof(msg), "%s:%s", s_uid, challenge);
    if (n < 0 || (size_t)n >= sizeof(msg)) return ESP_ERR_INVALID_ARG;

    // mbedTLS 4 (ESP-IDF v6.1) : l'API md.h HMAC est devenue privée, on passe
    // par PSA crypto. psa_crypto_init est idempotent.
    psa_status_t ps = psa_crypto_init();
    if (ps != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init: %d", (int)ps);
        return ESP_FAIL;
    }

    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attr, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attr, PSA_KEY_TYPE_HMAC);
    psa_set_key_bits(&attr, sizeof(s_secret) * 8);

    mbedtls_svc_key_id_t key = MBEDTLS_SVC_KEY_ID_INIT;
    ps = psa_import_key(&attr, s_secret, sizeof(s_secret), &key);
    if (ps != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key: %d", (int)ps);
        return ESP_FAIL;
    }

    uint8_t mac[SECRET_LEN];
    size_t mac_len = 0;
    ps = psa_mac_compute(key, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                         (const uint8_t *)msg, (size_t)n,
                         mac, sizeof(mac), &mac_len);
    psa_destroy_key(key);
    if (ps != PSA_SUCCESS || mac_len != SECRET_LEN) {
        ESP_LOGE(TAG, "psa_mac_compute: %d (len=%u)", (int)ps, (unsigned)mac_len);
        return ESP_FAIL;
    }

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(mac); i++) {
        out_hex[i * 2]     = hex[mac[i] >> 4];
        out_hex[i * 2 + 1] = hex[mac[i] & 0x0F];
    }
    out_hex[HAL_BOX_AUTH_SIG_HEX_LEN] = '\0';
    return ESP_OK;
}
