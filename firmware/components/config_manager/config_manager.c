#include "config_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

#define TAG "config"
#define NVS_NAMESPACE "escapebox"

#define DEFAULT_VOLUME     70
#define DEFAULT_BRIGHTNESS 80

static nvs_handle_t s_nvs;
static uint8_t s_volume;
static uint8_t s_brightness;
static bool    s_dev_mode;

static uint8_t nvs_get_u8_or(const char *key, uint8_t def)
{
    uint8_t val = def;
    nvs_get_u8(s_nvs, key, &val);
    return val;
}

esp_err_t config_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition erased (corrupt/version mismatch)");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs);
    if (ret != ESP_OK) return ret;

    s_volume     = nvs_get_u8_or("volume", DEFAULT_VOLUME);
    s_brightness = nvs_get_u8_or("bright", DEFAULT_BRIGHTNESS);
    s_dev_mode   = nvs_get_u8_or("devmode", 0) != 0;

    ESP_LOGI(TAG, "vol=%u%% bright=%u%% dev=%d", s_volume, s_brightness, s_dev_mode);
    return ESP_OK;
}

uint8_t config_get_volume(void)     { return s_volume; }
uint8_t config_get_brightness(void) { return s_brightness; }
bool    config_get_dev_mode(void)   { return s_dev_mode; }

void config_set_volume(uint8_t percent)
{
    if (percent > 100) percent = 100;
    s_volume = percent;
    nvs_set_u8(s_nvs, "volume", percent);
    nvs_commit(s_nvs);
}

void config_set_brightness(uint8_t percent)
{
    if (percent < 10) percent = 10;
    if (percent > 100) percent = 100;
    s_brightness = percent;
    nvs_set_u8(s_nvs, "bright", percent);
    nvs_commit(s_nvs);
}

void config_set_dev_mode(bool enabled)
{
    s_dev_mode = enabled;
    nvs_set_u8(s_nvs, "devmode", enabled ? 1 : 0);
    nvs_commit(s_nvs);
}
