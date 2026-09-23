#include "hal_box_auth.h"
#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "psa/crypto.h"

#define TAG "box_auth"

#define BOX_NVS_PART   "box_nvs"     // partition dédiée (partitions.csv)
#define NVS_NAMESPACE  "box_creds"
#define KEY_UID        "box_uid"
#define KEY_SECRET     "box_secret"

#define UID_MAX        64   // aligné sur BOX_UID_RE serveur ({4,64})
#define SECRET_LEN     32   // HKDF-SHA256 -> 32 octets

static char    s_uid[UID_MAX + 1];
static uint8_t s_secret[SECRET_LEN];
static bool    s_provisioned;

// Lit uid + secret depuis un handle ouvert sur le namespace box_creds.
static esp_err_t read_creds(nvs_handle_t nvs)
{
    size_t uid_len = sizeof(s_uid);
    esp_err_t err = nvs_get_str(nvs, KEY_UID, s_uid, &uid_len);
    if (err != ESP_OK) return err;
    size_t secret_len = sizeof(s_secret);
    err = nvs_get_blob(nvs, KEY_SECRET, s_secret, &secret_len);
    if (err != ESP_OK) return err;
    return secret_len == SECRET_LEN ? ESP_OK : ESP_ERR_NVS_INVALID_LENGTH;
}

static esp_err_t read_creds_from(const char *part)
{
    nvs_handle_t nvs;
    esp_err_t err = part
        ? nvs_open_from_partition(part, NVS_NAMESPACE, NVS_READONLY, &nvs)
        : nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) return err;
    err = read_creds(nvs);
    nvs_close(nvs);
    return err;
}

// Copie les identifiants (déjà chargés en RAM) dans box_nvs.
static esp_err_t write_creds_to_box_nvs(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open_from_partition(BOX_NVS_PART, NVS_NAMESPACE,
                                            NVS_READWRITE, &nvs);
    if (err != ESP_OK) return err;
    err = nvs_set_str(nvs, KEY_UID, s_uid);
    if (err == ESP_OK) err = nvs_set_blob(nvs, KEY_SECRET, s_secret, SECRET_LEN);
    if (err == ESP_OK) err = nvs_commit(nvs);
    nvs_close(nvs);
    return err;
}

// Identité lue dans la partition dédiée box_nvs, que rien n'efface (le
// nvs_flash_erase() de config_manager ne touche que la NVS applicative).
// Migration : une box provisionnée avant box_nvs a ses identifiants dans le
// namespace box_creds de la NVS applicative → recopiés une fois dans box_nvs.
// Ancienne table de partitions (box_nvs absente) → lecture legacy + warning.
esp_err_t hal_box_auth_init(void)
{
    s_provisioned = false;

    esp_err_t part_err = nvs_flash_init_partition(BOX_NVS_PART);
    if (part_err == ESP_OK && read_creds_from(BOX_NVS_PART) == ESP_OK) {
        s_provisioned = true;
        ESP_LOGI(TAG, "box provisionnée: %s", s_uid);
        return ESP_OK;
    }

    // Pas (encore) d'identité dans box_nvs : ancien emplacement ?
    esp_err_t err = read_creds_from(NULL);
    if (err != ESP_OK) {
        if (part_err != ESP_OK && part_err != ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "partition %s illisible (%s) — non effacée",
                     BOX_NVS_PART, esp_err_to_name(part_err));
        }
        ESP_LOGW(TAG, "identifiants absents (%s) — lancer tools/provision_box.py",
                 esp_err_to_name(err));
        return ESP_ERR_NVS_NOT_FOUND;
    }
    s_provisioned = true;

    if (part_err == ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "box provisionnée: %s — ⚠ table de partitions sans %s : "
                 "identité exposée à un effacement NVS, reflasher la table",
                 s_uid, BOX_NVS_PART);
        return ESP_OK;
    }
    if (part_err == ESP_ERR_NVS_NO_FREE_PAGES ||
        part_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // box_nvs illisible mais une copie des identifiants existe en RAM :
        // seul cas où l'effacer est sans perte.
        ESP_LOGW(TAG, "%s illisible (%s) — réinitialisée pour migration",
                 BOX_NVS_PART, esp_err_to_name(part_err));
        if (nvs_flash_erase_partition(BOX_NVS_PART) == ESP_OK) {
            part_err = nvs_flash_init_partition(BOX_NVS_PART);
        }
    }
    if (part_err == ESP_OK && (err = write_creds_to_box_nvs()) == ESP_OK) {
        ESP_LOGI(TAG, "box provisionnée: %s (identité migrée vers %s)",
                 s_uid, BOX_NVS_PART);
    } else {
        ESP_LOGW(TAG, "box provisionnée: %s — migration vers %s échouée (%s)",
                 s_uid, BOX_NVS_PART,
                 esp_err_to_name(part_err != ESP_OK ? part_err : err));
    }
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

static const char *purpose_str(hal_box_auth_purpose_t purpose)
{
    switch (purpose) {
    case HAL_BOX_AUTH_PURPOSE_AUTH:     return "auth";
    case HAL_BOX_AUTH_PURPOSE_REGISTER: return "register";
    default:                            return NULL;
    }
}

esp_err_t hal_box_auth_sign(hal_box_auth_purpose_t purpose, const char *challenge,
                            char *out_hex, size_t out_len)
{
    if (!s_provisioned) return ESP_ERR_INVALID_STATE;
    const char *pstr = purpose_str(purpose);
    if (!pstr || !challenge || !out_hex || out_len < HAL_BOX_AUTH_SIG_HEX_LEN + 1) {
        return ESP_ERR_INVALID_ARG;
    }

    // Message signé : "<purpose>:<box_uid>:<challenge>" (identique au serveur).
    char msg[16 + UID_MAX + 1 + 128];
    int n = snprintf(msg, sizeof(msg), "%s:%s:%s", pstr, s_uid, challenge);
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
