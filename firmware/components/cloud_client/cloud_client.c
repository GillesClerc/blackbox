#include "cloud_client.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "cJSON.h"
#include "esp_app_desc.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"

#include "hal_box_auth.h"
#include "hal_wifi.h"

#define TAG "cloud_client"

#define BASE_URL_MAX    128
#define URL_MAX         (BASE_URL_MAX + 160)
#define CHALLENGE_MAX   96    // nonce hex 64 chars côté serveur
#define TOKEN_MAX       1024  // JWT HS256 ~350 chars, large marge
#define BODY_MAX        512
#define RESP_BUF_SIZE   8192
#define HTTP_TIMEOUT_MS 10000

// Le JWT expire à 2 h côté serveur — ré-auth à 110 min pour garder de la marge.
#define TOKEN_LIFETIME_US (110LL * 60 * 1000000)

// Sanity check avant settimeofday : refuse un server_time aberrant (< mi-2025).
#define MIN_VALID_EPOCH 1750000000LL

typedef enum {
    CLOUD_EVT_SYNC = 1,
} cloud_evt_t;

static QueueHandle_t s_queue;
static char          s_base_url[BASE_URL_MAX];
static char          s_fw_version[24];          // semver validé pour l'URL sync
static char         *s_resp;                    // réponse HTTP — PSRAM, alloué au boot
static char          s_token[TOKEN_MAX];
static int64_t       s_token_deadline_us;       // esp_timer ; 0 = pas de token
static char          s_auth_hdr[TOKEN_MAX + 8]; // "Bearer <jwt>" — static : gros + task unique

// ─── HTTP ────────────────────────────────────────────────────────────────────

// Requête HTTPS générique : body == NULL → GET, sinon POST JSON.
// La réponse (NUL-terminée, tronquée à RESP_BUF_SIZE-1) est écrite dans s_resp.
// ESP_OK = échange HTTP abouti, quel que soit le status (renvoyé via *status).
static esp_err_t http_request(const char *url, const char *body,
                              const char *bearer, int *status)
{
    esp_http_client_config_t cfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    if (body) {
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", "application/json");
    }
    if (bearer) {
        snprintf(s_auth_hdr, sizeof(s_auth_hdr), "Bearer %s", bearer);
        esp_http_client_set_header(client, "Authorization", s_auth_hdr);
    }

    size_t    body_len = body ? strlen(body) : 0;
    esp_err_t err      = esp_http_client_open(client, body_len);
    if (err != ESP_OK) goto cleanup;

    if (body_len &&
        esp_http_client_write(client, body, body_len) != (int)body_len) {
        err = ESP_FAIL;
        goto close;
    }
    if (esp_http_client_fetch_headers(client) < 0) {
        err = ESP_FAIL;
        goto close;
    }

    int n = esp_http_client_read_response(client, s_resp, RESP_BUF_SIZE - 1);
    if (n < 0) {
        err = ESP_FAIL;
        goto close;
    }
    if (n == RESP_BUF_SIZE - 1) {
        ESP_LOGW(TAG, "réponse tronquée à %d octets", n);
    }
    s_resp[n] = '\0';
    *status   = esp_http_client_get_status_code(client);

close:
    esp_http_client_close(client);
cleanup:
    esp_http_client_cleanup(client);
    return err;
}

// ─── Auth (challenge → HMAC → JWT) ───────────────────────────────────────────

static esp_err_t do_auth(void)
{
    const char *uid = hal_box_auth_uid();
    char        url[URL_MAX];
    int         status = 0;

    snprintf(url, sizeof(url), "%s/api/box/challenge?box_uid=%s", s_base_url, uid);
    esp_err_t err = http_request(url, NULL, NULL, &status);
    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "challenge KO (%s, status=%d)", esp_err_to_name(err), status);
        return ESP_FAIL;
    }

    char   challenge[CHALLENGE_MAX];
    cJSON *root = cJSON_Parse(s_resp);
    const cJSON *item =
        root ? cJSON_GetObjectItemCaseSensitive(root, "challenge") : NULL;
    if (!cJSON_IsString(item) ||
        strlen(item->valuestring) >= sizeof(challenge)) {
        ESP_LOGW(TAG, "challenge : réponse JSON invalide");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    strlcpy(challenge, item->valuestring, sizeof(challenge));
    cJSON_Delete(root);

    char sig[HAL_BOX_AUTH_SIG_HEX_LEN + 1];
    err = hal_box_auth_sign(challenge, sig, sizeof(sig));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "signature du challenge : %s", esp_err_to_name(err));
        return err;
    }

    char body[BODY_MAX];
    snprintf(body, sizeof(body),
             "{\"box_uid\":\"%s\",\"challenge\":\"%s\",\"challenge_response\":\"%s\"}",
             uid, challenge, sig);

    snprintf(url, sizeof(url), "%s/api/box/auth", s_base_url);
    err = http_request(url, body, NULL, &status);
    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "auth KO (%s, status=%d) : %.128s", esp_err_to_name(err),
                 status, s_resp);
        return ESP_FAIL;
    }

    root = cJSON_Parse(s_resp);
    const cJSON *token =
        root ? cJSON_GetObjectItemCaseSensitive(root, "token") : NULL;
    const cJSON *stime =
        root ? cJSON_GetObjectItemCaseSensitive(root, "server_time") : NULL;
    if (!cJSON_IsString(token) ||
        strlen(token->valuestring) >= sizeof(s_token)) {
        ESP_LOGW(TAG, "auth : token absent ou trop long");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    strlcpy(s_token, token->valuestring, sizeof(s_token));
    s_token_deadline_us = esp_timer_get_time() + TOKEN_LIFETIME_US;

    // Pas de RTC ni SNTP pour l'instant : l'horloge système est réglée depuis
    // server_time (timestamps de logs, validation des dates de certificats).
    // N'affecte pas les timers FreeRTOS/esp_timer (monotones).
    if (cJSON_IsNumber(stime) && (int64_t)stime->valuedouble > MIN_VALID_EPOCH) {
        struct timeval tv = { .tv_sec = (time_t)stime->valuedouble };
        settimeofday(&tv, NULL);
    }
    cJSON_Delete(root);

    ESP_LOGI(TAG, "authentifiée auprès de %s — JWT valide ~2 h", s_base_url);
    return ESP_OK;
}

// ─── Sync ────────────────────────────────────────────────────────────────────

static esp_err_t do_sync(int *status)
{
    char url[URL_MAX];
    if (s_fw_version[0]) {
        snprintf(url, sizeof(url), "%s/api/box/sync?firmware_version=%s",
                 s_base_url, s_fw_version);
    } else {
        snprintf(url, sizeof(url), "%s/api/box/sync", s_base_url);
    }

    *status       = 0;
    esp_err_t err = http_request(url, NULL, s_token, status);
    if (err != ESP_OK || *status != 200) {
        ESP_LOGW(TAG, "sync KO (%s, status=%d)", esp_err_to_name(err), *status);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(s_resp);
    if (!root) {
        ESP_LOGW(TAG, "sync : réponse JSON invalide");
        return ESP_FAIL;
    }

    const cJSON *scenarios = cJSON_GetObjectItemCaseSensitive(root, "scenarios");
    int count = cJSON_IsArray(scenarios) ? cJSON_GetArraySize(scenarios) : 0;
    ESP_LOGI(TAG, "sync OK — %d scénario(s) installé(s)", count);
    const cJSON *s;
    cJSON_ArrayForEach(s, scenarios) {
        const cJSON *slug = cJSON_GetObjectItemCaseSensitive(s, "slug");
        if (cJSON_IsString(slug)) {
            ESP_LOGI(TAG, "  - %s", slug->valuestring);
        }
    }

    // F2 téléchargera les scénarios sur SD ; F5 traitera firmware_update.
    const cJSON *fw = cJSON_GetObjectItemCaseSensitive(root, "firmware_update");
    if (cJSON_IsObject(fw)) {
        const cJSON *ver = cJSON_GetObjectItemCaseSensitive(fw, "version");
        ESP_LOGI(TAG, "mise à jour firmware disponible : %s (ignorée — F5)",
                 cJSON_IsString(ver) ? ver->valuestring : "?");
    }

    cJSON_Delete(root);
    return ESP_OK;
}

// ─── Task ────────────────────────────────────────────────────────────────────

static void handle_sync(void)
{
    if (!hal_box_auth_is_provisioned()) {
        ESP_LOGW(TAG, "sync ignorée — box non provisionnée (tools/provision_box.py)");
        return;
    }
    if (!hal_wifi_is_connected()) {
        ESP_LOGW(TAG, "sync ignorée — WiFi non connecté");
        return;
    }

    bool token_valid =
        s_token_deadline_us && esp_timer_get_time() < s_token_deadline_us;
    if (!token_valid && do_auth() != ESP_OK) return;

    int status = 0;
    if (do_sync(&status) != ESP_OK && status == 401) {
        // JWT rejeté avant sa deadline locale (rotation de secret, horloge) :
        // une seule ré-auth puis un seul retry, pas de boucle.
        ESP_LOGW(TAG, "JWT rejeté — ré-authentification");
        s_token_deadline_us = 0;
        if (do_auth() == ESP_OK) {
            do_sync(&status);
        }
    }
}

static void cloud_task(void *arg)
{
    for (;;) {
        uint8_t evt;
        if (xQueueReceive(s_queue, &evt, pdMS_TO_TICKS(500)) != pdTRUE) {
            continue;  // idle — pas de TWDT : les requêtes bloquent en select(), pas en spin
        }
        if (evt == CLOUD_EVT_SYNC) {
            handle_sync();
        }
    }
}

// ─── API publique ────────────────────────────────────────────────────────────

esp_err_t cloud_client_init(void)
{
    if (s_queue) return ESP_OK;

    strlcpy(s_base_url, CONFIG_ESCAPEBOX_API_URL, sizeof(s_base_url));

    // Surcharge optionnelle en NVS (tests contre un serveur local).
    nvs_handle_t nvs;
    if (nvs_open("cloud", NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = sizeof(s_base_url);
        if (nvs_get_str(nvs, "api_url", s_base_url, &len) == ESP_OK) {
            ESP_LOGI(TAG, "API URL surchargée par NVS : %s", s_base_url);
        }
        nvs_close(nvs);
    }
    size_t bl = strlen(s_base_url);
    if (bl && s_base_url[bl - 1] == '/') s_base_url[bl - 1] = '\0';

    // firmware_version transmise au sync : uniquement si semver x.y.z propre
    // (PROJECT_VER ; le serveur rejette tout autre format).
    const esp_app_desc_t *app = esp_app_get_description();
    int maj, min, pat;
    if (sscanf(app->version, "%d.%d.%d", &maj, &min, &pat) == 3) {
        snprintf(s_fw_version, sizeof(s_fw_version), "%d.%d.%d", maj, min, pat);
    }

    // Buffer réponse : gros et pas DMA → PSRAM, alloué une fois au boot.
    s_resp = heap_caps_malloc(RESP_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_resp) {
        s_resp = heap_caps_malloc(RESP_BUF_SIZE, MALLOC_CAP_8BIT);
    }
    if (!s_resp) return ESP_ERR_NO_MEM;

    s_queue = xQueueCreate(4, sizeof(uint8_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    // Core 0 (réseau), prio 3 sous scenario_engine/touch (5) et wifi (4) :
    // le cloud ne préempte jamais le jeu. Pile : TLS handshake ~7 KB + locaux.
    BaseType_t ok = xTaskCreatePinnedToCore(cloud_task, "cloud_client", 10240,
                                            NULL, 3, NULL, 0);
    if (ok != pdPASS) {
        vQueueDelete(s_queue);
        s_queue = NULL;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "prêt — API %s, version firmware %s", s_base_url,
             s_fw_version[0] ? s_fw_version : "(non semver, non transmise)");
    return ESP_OK;
}

esp_err_t cloud_client_request_sync(void)
{
    if (!s_queue) return ESP_ERR_INVALID_STATE;
    uint8_t evt = CLOUD_EVT_SYNC;
    if (xQueueSend(s_queue, &evt, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "queue pleine — demande de sync ignorée");
        return ESP_FAIL;
    }
    return ESP_OK;
}
