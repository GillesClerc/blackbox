#include "cloud_client.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

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
#include "esp_vfs_fat.h"
#include "nvs.h"
#include "psa/crypto.h"

#include "hal_box_auth.h"
#include "hal_wifi.h"

#define TAG "cloud_client"

#define BASE_URL_MAX    128
#define URL_MAX         (BASE_URL_MAX + 240)
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
static char         *s_resp;                    // buffer HTTP/hash — PSRAM, alloué au boot
static char          s_token[TOKEN_MAX];
static int64_t       s_token_deadline_us;       // esp_timer ; 0 = pas de token
static char          s_auth_hdr[TOKEN_MAX + 8]; // "Bearer <jwt>" — static : gros + task unique

static esp_err_t do_auth(void);  // (auth plus bas, utilisée par les downloads)

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

// ─── Packages de scénarios sur SD (F3) ───────────────────────────────────────
// Layout aligné sur le chargeur de main.c : /sdcard/scenarios/<slug>/…
// Un package = les fichiers listés par son manifest (sha256 + taille par
// fichier). Le manifest local, écrit en DERNIER, est le marqueur de commit :
// sans lui, le prochain sync reprend l'installation où elle s'était arrêtée.

#define SCENARIO_SD_ROOT "/sdcard/scenarios"
#define LEGACY_MANIFEST  SCENARIO_SD_ROOT "/manifest.json"  // F2, supprimé
#define PKG_MANIFEST_MAX (64 * 1024)
#define PKG_FILE_MAX     (32 * 1024 * 1024)
#define PKG_FILES_MAX    128
#define PKG_PATH_MAX     120  // chemin relatif dans le package
#define SD_DIR_MAX       96   // /sdcard/scenarios/<slug>
#define SD_PATH_MAX      224  // …/<slug>/<rel>

// Le slug devient un nom de dossier : alphanumérique + '-' '_' strictement.
static bool slug_is_safe(const char *slug)
{
    size_t n = strlen(slug);
    if (n == 0 || n > 64) return false;
    for (size_t i = 0; i < n; i++) {
        char c = slug[i];
        if (!isalnum((unsigned char)c) && c != '-' && c != '_') return false;
    }
    return true;
}

// Chemin relatif d'un fichier de package : segments [A-Za-z0-9._-] jamais
// commencés par '.' (exclut ".", "..", cachés), profondeur <= 2 sous-dossiers.
static bool pkg_rel_path_is_safe(const char *p)
{
    size_t len = strlen(p);
    if (len == 0 || len > PKG_PATH_MAX) return false;

    int         depth = 0;
    const char *seg   = p;
    for (size_t i = 0; i <= len; i++) {
        char c = p[i];
        if (c == '/' || c == '\0') {
            if (&p[i] == seg) return false;  // segment vide
            if (seg[0] == '.') return false;
            if (c == '/' && ++depth > 2) return false;
            seg = &p[i] + 1;
        } else if (!isalnum((unsigned char)c) && !strchr("._-", c)) {
            return false;
        }
    }
    return true;
}

// package_path renvoyé par le serveur : absolu court, charset strict sans '.'
// (toute traversée est donc impossible par construction).
static bool pkg_base_path_is_safe(const char *p)
{
    size_t n = strlen(p);
    if (n < 2 || n > 96 || p[0] != '/' || p[n - 1] == '/') return false;
    for (size_t i = 1; i < n; i++) {
        char c = p[i];
        if (!isalnum((unsigned char)c) && !strchr("/_-", c)) return false;
        if (c == '/' && p[i - 1] == '/') return false;
    }
    return true;
}

static void to_hex(const uint8_t *in, size_t n, char *out)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[2 * i]     = digits[in[i] >> 4];
        out[2 * i + 1] = digits[in[i] & 0x0F];
    }
    out[2 * n] = '\0';
}

// sha256 (hex minuscule) d'un fichier SD — sert à la reprise sans manifest.
static esp_err_t sha256_file(const char *path, char out_hex[65])
{
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_FAIL;

    psa_hash_operation_t op = PSA_HASH_OPERATION_INIT;
    if (psa_hash_setup(&op, PSA_ALG_SHA_256) != PSA_SUCCESS) {
        fclose(f);
        return ESP_FAIL;
    }

    esp_err_t err = ESP_OK;
    size_t    n;
    while ((n = fread(s_resp, 1, RESP_BUF_SIZE, f)) > 0) {
        if (psa_hash_update(&op, (const uint8_t *)s_resp, n) != PSA_SUCCESS) {
            err = ESP_FAIL;
            break;
        }
    }
    if (ferror(f)) err = ESP_FAIL;
    fclose(f);

    uint8_t digest[32];
    size_t  dlen = 0;
    if (err == ESP_OK &&
        (psa_hash_finish(&op, digest, sizeof(digest), &dlen) != PSA_SUCCESS ||
         dlen != sizeof(digest))) {
        err = ESP_FAIL;
    }
    if (err != ESP_OK) {
        psa_hash_abort(&op);
        return err;
    }
    to_hex(digest, sizeof(digest), out_hex);
    return ESP_OK;
}

// Télécharge url vers dst_path en streaming via un .tmp renommé à la fin —
// jamais de fichier tronqué visible. expect_sha256 (hex, optionnel) est
// vérifié au fil de l'eau avant le rename. *status_out : code HTTP (0 si
// erreur transport). Réutilise s_resp comme buffer de chunks.
static esp_err_t download_to_file(const char *url, const char *dst_path,
                                  const char *bearer, size_t max_bytes,
                                  const char *expect_sha256, int *status_out)
{
    *status_out = 0;
    char tmp_path[SD_PATH_MAX + 8];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", dst_path);

    esp_http_client_config_t cfg = {
        .url               = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = HTTP_TIMEOUT_MS,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_FAIL;

    if (bearer) {
        snprintf(s_auth_hdr, sizeof(s_auth_hdr), "Bearer %s", bearer);
        esp_http_client_set_header(client, "Authorization", s_auth_hdr);
    }

    psa_hash_operation_t hash    = PSA_HASH_OPERATION_INIT;
    bool                 hashing = false;
    if (expect_sha256) {
        if (psa_hash_setup(&hash, PSA_ALG_SHA_256) != PSA_SUCCESS) {
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        hashing = true;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int status  = esp_http_client_get_status_code(client);
        *status_out = status;
        if (status != 200) {
            ESP_LOGW(TAG, "download %s : status=%d", url, status);
            err = ESP_FAIL;
        } else {
            FILE *f = fopen(tmp_path, "wb");
            if (!f) {
                ESP_LOGW(TAG, "création %s impossible", tmp_path);
                err = ESP_FAIL;
            } else {
                size_t total = 0;
                for (;;) {
                    int n = esp_http_client_read(client, s_resp, RESP_BUF_SIZE);
                    if (n < 0) { err = ESP_FAIL; break; }
                    if (n == 0) break;
                    total += (size_t)n;
                    if (total > max_bytes) {
                        ESP_LOGW(TAG, "download %s : > %u octets, abandonné",
                                 url, (unsigned)max_bytes);
                        err = ESP_FAIL;
                        break;
                    }
                    if (fwrite(s_resp, 1, (size_t)n, f) != (size_t)n) {
                        ESP_LOGW(TAG, "écriture SD échouée (%s)", tmp_path);
                        err = ESP_FAIL;
                        break;
                    }
                    if (hashing && psa_hash_update(&hash, (const uint8_t *)s_resp,
                                                   (size_t)n) != PSA_SUCCESS) {
                        err = ESP_FAIL;
                        break;
                    }
                }
                if (err == ESP_OK &&
                    !esp_http_client_is_complete_data_received(client)) {
                    ESP_LOGW(TAG, "download %s : transfert incomplet", url);
                    err = ESP_FAIL;
                }
                fclose(f);

                if (err == ESP_OK && hashing) {
                    uint8_t digest[32];
                    size_t  dlen = 0;
                    char    hex[65];
                    if (psa_hash_finish(&hash, digest, sizeof(digest), &dlen) !=
                            PSA_SUCCESS || dlen != sizeof(digest)) {
                        err = ESP_FAIL;
                    } else {
                        to_hex(digest, sizeof(digest), hex);
                        if (strcasecmp(hex, expect_sha256) != 0) {
                            ESP_LOGW(TAG, "download %s : sha256 invalide", url);
                            err = ESP_FAIL;
                        }
                    }
                    hashing = false;
                }
                if (err == ESP_OK) {
                    unlink(dst_path);  // FAT : rename vers un existant échoue
                    if (rename(tmp_path, dst_path) != 0) {
                        ESP_LOGW(TAG, "rename %s : %s", tmp_path, strerror(errno));
                        err = ESP_FAIL;
                    } else {
                        ESP_LOGI(TAG, "téléchargé %s (%u octets)", dst_path,
                                 (unsigned)total);
                    }
                }
                if (err != ESP_OK) unlink(tmp_path);
            }
        }
        esp_http_client_close(client);
    }
    if (hashing) psa_hash_abort(&hash);
    esp_http_client_cleanup(client);
    return err;
}

// Download authentifié : sur 401 (JWT expiré en plein gros package),
// une seule ré-auth puis un seul retry.
static esp_err_t download_with_reauth(const char *url, const char *dst_path,
                                      size_t max_bytes, const char *expect_sha256)
{
    int       status = 0;
    esp_err_t err    = download_to_file(url, dst_path, s_token, max_bytes,
                                        expect_sha256, &status);
    if (err != ESP_OK && status == 401) {
        s_token_deadline_us = 0;
        if (do_auth() == ESP_OK) {
            err = download_to_file(url, dst_path, s_token, max_bytes,
                                   expect_sha256, &status);
        }
    }
    return err;
}

// Lit un fichier (≤ max_bytes) dans un buffer PSRAM NUL-terminé, à libérer
// par l'appelant. NULL si absent/trop gros/illisible.
static char *read_file_psram(const char *path, size_t max_bytes)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0 || (size_t)size > max_bytes) {
        fclose(f);
        return NULL;
    }
    char *buf = heap_caps_malloc((size_t)size + 1,
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (n != (size_t)size) {
        heap_caps_free(buf);
        return NULL;
    }
    buf[size] = '\0';
    return buf;
}

// sha256 déclaré pour `rel` dans un manifest ({files:[{path,sha256},…]}).
static const char *manifest_file_sha(const cJSON *manifest, const char *rel)
{
    const cJSON *files =
        manifest ? cJSON_GetObjectItemCaseSensitive(manifest, "files") : NULL;
    const cJSON *f;
    cJSON_ArrayForEach(f, files) {
        const cJSON *p = cJSON_GetObjectItemCaseSensitive(f, "path");
        const cJSON *s = cJSON_GetObjectItemCaseSensitive(f, "sha256");
        if (cJSON_IsString(p) && cJSON_IsString(s) &&
            strcmp(p->valuestring, rel) == 0) {
            return s->valuestring;
        }
    }
    return NULL;
}

// Crée les sous-dossiers de rel (profondeur ≤ 2) sous base.
static esp_err_t mkdirs_for_rel(const char *base, const char *rel)
{
    char path[SD_PATH_MAX];
    for (const char *p = strchr(rel, '/'); p; p = strchr(p + 1, '/')) {
        snprintf(path, sizeof(path), "%s/%.*s", base, (int)(p - rel), rel);
        if (mkdir(path, 0775) != 0 && errno != EEXIST) {
            ESP_LOGW(TAG, "mkdir %s : %s", path, strerror(errno));
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

// Installe (ou met à jour) un package : manifest distant → manifest.new,
// fichiers vérifiés/téléchargés un à un (skip si taille + sha corrects —
// reprise gratuite), puis manifest local commité en dernier.
static esp_err_t install_package(const char *slug, const char *pkg_path,
                                 int server_version)
{
    char dir[SD_DIR_MAX];
    snprintf(dir, sizeof(dir), SCENARIO_SD_ROOT "/%s", slug);
    char man_path[SD_DIR_MAX + 16], man_new[SD_DIR_MAX + 16];
    snprintf(man_path, sizeof(man_path), "%s/manifest.json", dir);
    snprintf(man_new, sizeof(man_new), "%s/manifest.new", dir);

    cJSON *remote = NULL;

    // Déjà à la bonne version ? (aucun accès réseau dans ce cas)
    cJSON *local = NULL;
    char  *buf   = read_file_psram(man_path, PKG_MANIFEST_MAX);
    if (buf) {
        local = cJSON_Parse(buf);
        heap_caps_free(buf);
    }
    const cJSON *lver =
        local ? cJSON_GetObjectItemCaseSensitive(local, "version") : NULL;
    if (cJSON_IsNumber(lver) && (int)lver->valuedouble == server_version) {
        cJSON_Delete(local);
        return ESP_OK;
    }
    ESP_LOGI(TAG, "installation de %s v%d…", slug, server_version);

    if (mkdir(dir, 0775) != 0 && errno != EEXIST) {
        ESP_LOGW(TAG, "mkdir %s : %s", dir, strerror(errno));
        cJSON_Delete(local);
        return ESP_FAIL;
    }

    char url[URL_MAX];
    snprintf(url, sizeof(url), "%s%s/manifest.json", s_base_url, pkg_path);
    if (download_with_reauth(url, man_new, PKG_MANIFEST_MAX, NULL) != ESP_OK) {
        cJSON_Delete(local);
        return ESP_FAIL;
    }

    buf    = read_file_psram(man_new, PKG_MANIFEST_MAX);
    remote = buf ? cJSON_Parse(buf) : NULL;
    if (buf) heap_caps_free(buf);
    const cJSON *files =
        remote ? cJSON_GetObjectItemCaseSensitive(remote, "files") : NULL;
    const cJSON *tot =
        remote ? cJSON_GetObjectItemCaseSensitive(remote, "total_bytes") : NULL;
    if (!cJSON_IsArray(files) || cJSON_GetArraySize(files) > PKG_FILES_MAX) {
        ESP_LOGW(TAG, "%s : manifest de package invalide", slug);
        goto fail;
    }

    // Espace SD : total_bytes du package majore ce qui reste à télécharger.
    uint64_t sd_total = 0, sd_free = 0;
    if (esp_vfs_fat_info("/sdcard", &sd_total, &sd_free) == ESP_OK &&
        cJSON_IsNumber(tot) &&
        sd_free < (uint64_t)tot->valuedouble + (1024 * 1024)) {
        ESP_LOGW(TAG, "%s : espace SD insuffisant (%llu octets libres)", slug,
                 (unsigned long long)sd_free);
        goto fail;
    }

    const cJSON *fentry;
    cJSON_ArrayForEach(fentry, files) {
        const cJSON *rel = cJSON_GetObjectItemCaseSensitive(fentry, "path");
        const cJSON *byt = cJSON_GetObjectItemCaseSensitive(fentry, "bytes");
        const cJSON *sha = cJSON_GetObjectItemCaseSensitive(fentry, "sha256");
        if (!cJSON_IsString(rel) || !pkg_rel_path_is_safe(rel->valuestring) ||
            !cJSON_IsNumber(byt) || byt->valuedouble < 0 ||
            byt->valuedouble > PKG_FILE_MAX ||
            !cJSON_IsString(sha) || strlen(sha->valuestring) != 64) {
            ESP_LOGW(TAG, "%s : entrée de manifest invalide", slug);
            goto fail;
        }
        size_t bytes = (size_t)byt->valuedouble;

        char fpath[SD_PATH_MAX];
        snprintf(fpath, sizeof(fpath), "%s/%s", dir, rel->valuestring);

        // Fichier déjà bon ? Hash connu de l'ancien manifest, sinon re-hash.
        struct stat st;
        if (stat(fpath, &st) == 0 && (size_t)st.st_size == bytes) {
            const char *old_sha = manifest_file_sha(local, rel->valuestring);
            if (old_sha && strcasecmp(old_sha, sha->valuestring) == 0) continue;
            char hex[65];
            if (sha256_file(fpath, hex) == ESP_OK &&
                strcasecmp(hex, sha->valuestring) == 0) {
                continue;
            }
        }

        if (mkdirs_for_rel(dir, rel->valuestring) != ESP_OK) goto fail;
        snprintf(url, sizeof(url), "%s%s/%s", s_base_url, pkg_path,
                 rel->valuestring);
        if (download_with_reauth(url, fpath, bytes, sha->valuestring) != ESP_OK) {
            goto fail;  // les fichiers complets restent → reprise au prochain sync
        }
    }

    unlink(man_path);
    if (rename(man_new, man_path) != 0) {
        ESP_LOGW(TAG, "%s : commit du manifest échoué (%s)", slug,
                 strerror(errno));
        goto fail;
    }
    ESP_LOGI(TAG, "scénario %s v%d installé", slug, server_version);
    cJSON_Delete(local);
    cJSON_Delete(remote);
    return ESP_OK;

fail:
    unlink(man_new);
    cJSON_Delete(local);
    cJSON_Delete(remote);
    return ESP_FAIL;
}

// Installe les scénarios listés par le sync (modèle pull). Les dossiers SD
// absents de la liste sont conservés (pas de GC — juste loggés).
static void install_scenarios(const cJSON *scenarios)
{
    if (!cJSON_IsArray(scenarios) || cJSON_GetArraySize(scenarios) == 0) return;

    // mkdir sonde aussi la présence de la SD : ENOENT = non montée.
    if (mkdir(SCENARIO_SD_ROOT, 0775) != 0 && errno != EEXIST) {
        ESP_LOGW(TAG, "SD indisponible (%s) — installation des scénarios sautée",
                 strerror(errno));
        return;
    }
    unlink(LEGACY_MANIFEST);  // migration F2 : ancien manifest global

    const cJSON *item;
    cJSON_ArrayForEach(item, scenarios) {
        const cJSON *slug = cJSON_GetObjectItemCaseSensitive(item, "slug");
        const cJSON *pkg  = cJSON_GetObjectItemCaseSensitive(item, "package_path");
        const cJSON *ver  = cJSON_GetObjectItemCaseSensitive(item, "version");
        if (!cJSON_IsString(slug) || !slug_is_safe(slug->valuestring) ||
            !cJSON_IsString(pkg) || !pkg_base_path_is_safe(pkg->valuestring) ||
            !cJSON_IsNumber(ver)) {
            ESP_LOGW(TAG, "scénario ignoré (slug/package_path/version invalide)");
            continue;
        }
        install_package(slug->valuestring, pkg->valuestring,
                        (int)ver->valuedouble);
    }

    DIR *d = opendir(SCENARIO_SD_ROOT);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.' || e->d_type != DT_DIR) continue;
        bool known = false;
        cJSON_ArrayForEach(item, scenarios) {
            const cJSON *slug = cJSON_GetObjectItemCaseSensitive(item, "slug");
            if (cJSON_IsString(slug) &&
                strcmp(slug->valuestring, e->d_name) == 0) {
                known = true;
                break;
            }
        }
        if (!known) {
            ESP_LOGI(TAG, "scénario orphelin sur SD (conservé) : %s", e->d_name);
        }
    }
    closedir(d);
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
    ESP_LOGI(TAG, "sync OK — %d scénario(s) licencié(s)", count);
    const cJSON *s;
    cJSON_ArrayForEach(s, scenarios) {
        const cJSON *slug = cJSON_GetObjectItemCaseSensitive(s, "slug");
        const cJSON *ver  = cJSON_GetObjectItemCaseSensitive(s, "version");
        if (cJSON_IsString(slug)) {
            ESP_LOGI(TAG, "  - %s v%d", slug->valuestring,
                     cJSON_IsNumber(ver) ? (int)ver->valuedouble : 0);
        }
    }

    install_scenarios(scenarios);

    // F6 traitera firmware_update ; loggé pour l'instant.
    const cJSON *fw = cJSON_GetObjectItemCaseSensitive(root, "firmware_update");
    if (cJSON_IsObject(fw)) {
        const cJSON *ver = cJSON_GetObjectItemCaseSensitive(fw, "version");
        ESP_LOGI(TAG, "mise à jour firmware disponible : %s (ignorée — F6)",
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

    // sha256 des packages via PSA (déjà utilisé par hal_box_auth ; idempotent).
    if (psa_crypto_init() != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init a échoué");
        return ESP_FAIL;
    }

    // Buffer réponse/chunks : gros et pas DMA → PSRAM, alloué une fois au boot.
    s_resp = heap_caps_malloc(RESP_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_resp) {
        s_resp = heap_caps_malloc(RESP_BUF_SIZE, MALLOC_CAP_8BIT);
    }
    if (!s_resp) return ESP_ERR_NO_MEM;

    s_queue = xQueueCreate(4, sizeof(uint8_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    // Core 0 (réseau), prio 3 sous scenario_engine/touch (5) et wifi (4) :
    // le cloud ne préempte jamais le jeu. Pile : TLS handshake ~7 KB + frames
    // d'installation de package (~2 KB de chemins/URL).
    BaseType_t ok = xTaskCreatePinnedToCore(cloud_task, "cloud_client", 12288,
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
