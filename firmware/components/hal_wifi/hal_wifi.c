#include "hal_wifi.h"
#include <stdatomic.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"

#define TAG "hal_wifi"

#define NVS_NAMESPACE  "wifi_creds"
#define KEY_SSID       "ssid"
#define KEY_PASS       "pass"

#define SSID_MAX       32   // 802.11 : 32 octets max
#define PASS_MAX       64   // WPA2 PSK : 63 + NUL
#define MAX_RETRY      5

#define BIT_CONNECTED  BIT0
#define BIT_FAIL       BIT1

static EventGroupHandle_t s_events;
static int                s_retry;
static bool               s_inited;
static atomic_bool        s_connected;  // partagé task WiFi ↔ appelants (SMP)

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        atomic_store(&s_connected, false);
        if (s_retry < MAX_RETRY) {
            s_retry++;
            ESP_LOGW(TAG, "déconnecté, tentative %d/%d", s_retry, MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_events, BIT_FAIL);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "connectée, IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_retry = 0;
        atomic_store(&s_connected, true);
        xEventGroupSetBits(s_events, BIT_CONNECTED);
    }
}

esp_err_t hal_wifi_init(void)
{
    if (s_inited) return ESP_OK;
    atomic_store(&s_connected, false);

    char   ssid[SSID_MAX + 1] = {0};
    char   pass[PASS_MAX + 1] = {0};
    size_t ssid_len = sizeof(ssid);
    size_t pass_len = sizeof(pass);

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "namespace '%s' absent: %s", NVS_NAMESPACE,
                 esp_err_to_name(err));
        return ESP_ERR_NVS_NOT_FOUND;
    }
    err = nvs_get_str(nvs, KEY_SSID, ssid, &ssid_len);
    if (err == ESP_OK) {
        // Mot de passe optionnel (réseau ouvert) : on tolère son absence.
        esp_err_t perr = nvs_get_str(nvs, KEY_PASS, pass, &pass_len);
        if (perr != ESP_OK && perr != ESP_ERR_NVS_NOT_FOUND) {
            err = perr;
        }
    }
    nvs_close(nvs);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "identifiants WiFi absents: %s", esp_err_to_name(err));
        return ESP_ERR_NVS_NOT_FOUND;
    }

    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "esp_netif_init");
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {  // peut déjà exister
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        return err;
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init");

    s_events = xEventGroupCreate();
    if (!s_events) return ESP_ERR_NO_MEM;

    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                            on_event, NULL, NULL),
        TAG, "reg WIFI_EVENT");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            on_event, NULL, NULL),
        TAG, "reg IP_EVENT");

    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, pass, sizeof(wc.sta.password));
    wc.sta.threshold.authmode = pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set_mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wc), TAG, "set_config");

    s_inited = true;
    ESP_LOGI(TAG, "initialisée (SSID '%s')", ssid);
    return ESP_OK;
}

esp_err_t hal_wifi_connect(uint32_t timeout_ms)
{
    if (!s_inited) return ESP_ERR_INVALID_STATE;

    s_retry = 0;
    xEventGroupClearBits(s_events, BIT_CONNECTED | BIT_FAIL);
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "esp_wifi_start");

    EventBits_t bits = xEventGroupWaitBits(s_events, BIT_CONNECTED | BIT_FAIL,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(timeout_ms));
    if (bits & BIT_CONNECTED) return ESP_OK;
    ESP_LOGW(TAG, "connexion échouée (timeout ou %d échecs)", MAX_RETRY);
    return ESP_FAIL;
}

bool hal_wifi_is_connected(void)
{
    return atomic_load(&s_connected);
}
