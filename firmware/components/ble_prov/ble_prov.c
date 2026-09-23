#include "ble_prov.h"

#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include "hal_box_auth.h"
#include "hal_wifi.h"

#define TAG "ble_prov"

#define SSID_MAX      32
#define PASS_MAX      64
#define CHALLENGE_MAX 96
#define STATUS_MAX    24
#define WIFI_TIMEOUT_MS 15000

// Base UUID e5c4000X-5c25-4b10-8f46-6b9c30ac7a11, octets LSB→MSB pour NimBLE.
#define PROV_UUID(n) BLE_UUID128_INIT(                                  \
    0x11, 0x7a, 0xac, 0x30, 0x9c, 0x6b, 0x46, 0x8f,                     \
    0x10, 0x4b, 0x25, 0x5c, (n), 0x00, 0xc4, 0xe5)

static const ble_uuid128_t UUID_SVC       = PROV_UUID(0x01);
static const ble_uuid128_t UUID_BOX_UID   = PROV_UUID(0x02);
static const ble_uuid128_t UUID_SSID      = PROV_UUID(0x03);
static const ble_uuid128_t UUID_PASS      = PROV_UUID(0x04);
static const ble_uuid128_t UUID_STATUS    = PROV_UUID(0x05);
static const ble_uuid128_t UUID_CHALLENGE = PROV_UUID(0x06);
static const ble_uuid128_t UUID_RESPONSE  = PROV_UUID(0x07);

typedef enum {
    PROV_EVT_APPLY_WIFI = 1,
    PROV_EVT_STOP,
} prov_evt_t;

static atomic_bool           s_active;
static bool                  s_infra_ready;  // worker + timer créés (une fois)
static uint8_t               s_own_addr_type;
static uint16_t              s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t              s_status_handle, s_response_handle;
static char                  s_ssid[SSID_MAX + 1];
static char                  s_pass[PASS_MAX + 1];   // write-only, jamais loggé
static bool                  s_have_ssid;
static char                  s_status[STATUS_MAX] = "idle";
static char                  s_response[HAL_BOX_AUTH_SIG_HEX_LEN + 1];
static QueueHandle_t         s_queue;
static esp_timer_handle_t    s_window_timer;
static ble_prov_wifi_ok_cb_t s_wifi_ok_cb;

static void advertise(void);

// ── Statut (READ + NOTIFY) ────────────────────────────────────────────────────

static void set_status(const char *st)
{
    strlcpy(s_status, st, sizeof(s_status));
    ESP_LOGI(TAG, "statut : %s", s_status);
    if (s_status_handle) {
        ble_gatts_chr_updated(s_status_handle);  // notifie les abonnés
    }
}

// ── Accès GATT ────────────────────────────────────────────────────────────────

static int write_flat(struct os_mbuf *om, char *dst, uint16_t max_len)
{
    uint16_t len = 0;
    if (OS_MBUF_PKTLEN(om) > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    if (ble_hs_mbuf_to_flat(om, dst, max_len, &len) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    dst[len] = '\0';
    return 0;
}

static int gatt_access(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR ||
                             ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR
                                 ? ctxt->chr->uuid
                                 : NULL;
    if (!uuid) return BLE_ATT_ERR_UNLIKELY;

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        const char *out = NULL;
        if (ble_uuid_cmp(uuid, &UUID_BOX_UID.u) == 0) {
            out = hal_box_auth_uid() ? hal_box_auth_uid() : "UNPROVISIONED";
        } else if (ble_uuid_cmp(uuid, &UUID_STATUS.u) == 0) {
            out = s_status;
        } else if (ble_uuid_cmp(uuid, &UUID_RESPONSE.u) == 0) {
            out = s_response;
        }
        if (!out) return BLE_ATT_ERR_READ_NOT_PERMITTED;
        return os_mbuf_append(ctxt->om, out, strlen(out)) == 0
                   ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ble_uuid_cmp(uuid, &UUID_SSID.u) == 0) {
            int rc = write_flat(ctxt->om, s_ssid, SSID_MAX);
            if (rc == 0 && s_ssid[0]) s_have_ssid = true;
            return rc;
        }
        if (ble_uuid_cmp(uuid, &UUID_PASS.u) == 0) {
            int rc = write_flat(ctxt->om, s_pass, PASS_MAX);
            if (rc != 0) return rc;
            if (!s_have_ssid) {
                ESP_LOGW(TAG, "wifi_pass reçu sans wifi_ssid — ignoré");
                return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
            }
            // La connexion WiFi bloque ~15 s → déléguée au worker, jamais
            // dans le host task NimBLE.
            prov_evt_t evt = PROV_EVT_APPLY_WIFI;
            if (xQueueSend(s_queue, &evt, 0) != pdTRUE) {
                return BLE_ATT_ERR_INSUFFICIENT_RES;
            }
            return 0;
        }
        if (ble_uuid_cmp(uuid, &UUID_CHALLENGE.u) == 0) {
            char challenge[CHALLENGE_MAX + 1];
            int  rc = write_flat(ctxt->om, challenge, CHALLENGE_MAX);
            if (rc != 0) return rc;
            // HMAC PSA : rapide, peut rester dans le host task. Ce canal n'est
            // pas authentifié : il ne signe QUE des preuves d'appairage
            // (purpose "register"), jamais des challenges d'auth cloud — sinon
            // n'importe qui à portée BLE obtiendrait un JWT au nom de la box.
            if (hal_box_auth_sign(HAL_BOX_AUTH_PURPOSE_REGISTER, challenge,
                                  s_response, sizeof(s_response)) != ESP_OK) {
                ESP_LOGW(TAG, "signature challenge impossible (box non provisionnée ?)");
                return BLE_ATT_ERR_UNLIKELY;
            }
            ESP_LOGI(TAG, "challenge signé (preuve de possession)");
            if (s_response_handle) ble_gatts_chr_updated(s_response_handle);
            return 0;
        }
        return BLE_ATT_ERR_WRITE_NOT_PERMITTED;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static const struct ble_gatt_svc_def s_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &UUID_SVC.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            { .uuid = &UUID_BOX_UID.u,   .access_cb = gatt_access,
              .flags = BLE_GATT_CHR_F_READ },
            { .uuid = &UUID_SSID.u,      .access_cb = gatt_access,
              .flags = BLE_GATT_CHR_F_WRITE },
            { .uuid = &UUID_PASS.u,      .access_cb = gatt_access,
              .flags = BLE_GATT_CHR_F_WRITE },
            { .uuid = &UUID_STATUS.u,    .access_cb = gatt_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &s_status_handle },
            { .uuid = &UUID_CHALLENGE.u, .access_cb = gatt_access,
              .flags = BLE_GATT_CHR_F_WRITE },
            { .uuid = &UUID_RESPONSE.u,  .access_cb = gatt_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &s_response_handle },
            { 0 },
        },
    },
    { 0 },
};

// ── GAP ───────────────────────────────────────────────────────────────────────

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "client connecté");
        } else if (atomic_load(&s_active)) {
            advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGI(TAG, "client déconnecté (raison %d)", event->disconnect.reason);
        if (atomic_load(&s_active)) advertise();
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        if (atomic_load(&s_active)) advertise();
        return 0;
    default:
        return 0;
    }
}

static void advertise(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)ble_svc_gap_device_name();
    fields.name_len = strlen(ble_svc_gap_device_name());
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGW(TAG, "adv_set_fields rc=%d", rc);
        return;
    }

    // UUID du service en scan response (l'advertising est plein avec le nom).
    struct ble_hs_adv_fields rsp = {0};
    rsp.uuids128 = (ble_uuid128_t *)&UUID_SVC;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params params = {0};
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params,
                           gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGW(TAG, "adv_start rc=%d", rc);
    }
}

static void on_sync(void)
{
    if (ble_hs_util_ensure_addr(0) != 0 ||
        ble_hs_id_infer_auto(0, &s_own_addr_type) != 0) {
        ESP_LOGE(TAG, "adresse BLE indisponible");
        return;
    }
    ESP_LOGI(TAG, "advertising « %s » (fenêtre d'appairage ouverte)",
             ble_svc_gap_device_name());
    advertise();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "reset host NimBLE (raison %d)", reason);
}

static void host_task(void *arg)
{
    nimble_port_run();  // revient à nimble_port_stop()
    nimble_port_freertos_deinit();
}

// ── Worker (connexion WiFi + arrêt de fenêtre) ───────────────────────────────

static void apply_wifi(void)
{
    set_status("connecting");
    esp_err_t err = hal_wifi_set_credentials(s_ssid, s_pass);
    memset(s_pass, 0, sizeof(s_pass));  // le secret ne traîne pas en RAM
    if (err == ESP_OK) {
        err = hal_wifi_connect(WIFI_TIMEOUT_MS);
    }
    if (err == ESP_OK) {
        set_status("wifi_ok");
        if (s_wifi_ok_cb) s_wifi_ok_cb();
    } else {
        set_status("wifi_fail");
    }
}

static void stop_ble(void)
{
    if (!atomic_load(&s_active)) return;
    atomic_store(&s_active, false);
    esp_timer_stop(s_window_timer);
    ble_gap_adv_stop();
    if (s_conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ble_gap_terminate(s_conn_handle, BLE_ERR_REM_USER_CONN_TERM);
        vTaskDelay(pdMS_TO_TICKS(200));  // laisse partir la déconnexion
    }
    nimble_port_stop();    // bloque jusqu'à l'arrêt du host task
    nimble_port_deinit();  // rend la RAM du contrôleur
    s_status_handle = s_response_handle = 0;
    ESP_LOGI(TAG, "fenêtre fermée — BLE arrêté (%u octets internes libres)",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

static void worker_task(void *arg)
{
    for (;;) {
        prov_evt_t evt;
        if (xQueueReceive(s_queue, &evt, pdMS_TO_TICKS(500)) != pdTRUE) {
            continue;
        }
        switch (evt) {
        case PROV_EVT_APPLY_WIFI: apply_wifi(); break;
        case PROV_EVT_STOP:       stop_ble();   break;
        }
    }
}

static void window_timeout(void *arg)
{
    // Contexte esp_timer : ne pas bloquer ici, déléguer au worker.
    prov_evt_t evt = PROV_EVT_STOP;
    xQueueSend(s_queue, &evt, 0);
}

// ── API publique ──────────────────────────────────────────────────────────────

esp_err_t ble_prov_start(uint32_t window_s, ble_prov_wifi_ok_cb_t on_wifi_ok)
{
    if (atomic_load(&s_active)) return ESP_OK;
    s_wifi_ok_cb = on_wifi_ok;
    s_have_ssid  = false;
    s_response[0] = '\0';
    strlcpy(s_status, "idle", sizeof(s_status));

    if (!s_infra_ready) {
        s_queue = xQueueCreate(4, sizeof(prov_evt_t));
        if (!s_queue) return ESP_ERR_NO_MEM;
        const esp_timer_create_args_t targs = {
            .callback = window_timeout,
            .name     = "ble_prov_win",
        };
        esp_err_t terr = esp_timer_create(&targs, &s_window_timer);
        if (terr != ESP_OK) return terr;
        // Core 0 prio 3 (réseau) : la connexion WiFi bloque ≤ 15 s ici.
        if (xTaskCreatePinnedToCore(worker_task, "ble_prov", 4096, NULL, 3,
                                    NULL, 0) != pdPASS) {
            return ESP_FAIL;
        }
        s_infra_ready = true;
    }

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.sync_cb   = on_sync;
    ble_hs_cfg.reset_cb  = on_reset;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;  // pas de bonding : la preuve
    ble_hs_cfg.sm_sc     = 0;                    // de possession est le HMAC

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_gatts_count_cfg(s_gatt_svcs);
    if (rc == 0) rc = ble_gatts_add_svcs(s_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "enregistrement GATT rc=%d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }

    // Nom : EscapeBox-XXXX (fin du box_uid), reconnaissable dans le sélecteur
    // Web Bluetooth.
    char name[24] = "EscapeBox";
    const char *uid = hal_box_auth_uid();
    if (uid) {
        size_t n = strlen(uid);
        snprintf(name, sizeof(name), "EscapeBox-%s", n >= 4 ? uid + n - 4 : uid);
    }
    ble_svc_gap_device_name_set(name);

    nimble_port_freertos_init(host_task);

    atomic_store(&s_active, true);
    esp_timer_start_once(s_window_timer, (uint64_t)window_s * 1000000ULL);
    ESP_LOGI(TAG, "fenêtre d'appairage BLE ouverte (%lu s)", (unsigned long)window_s);
    return ESP_OK;
}

bool ble_prov_is_active(void)
{
    return atomic_load(&s_active);
}
