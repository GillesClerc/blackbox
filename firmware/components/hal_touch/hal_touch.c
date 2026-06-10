#include "hal_touch.h"
#include "hal_i2c_bus_priv.h"
#include "mpr121.h"
#include "mtch2120.h"
#include "esp_log.h"

#define TAG "hal_touch"

typedef enum { BACKEND_NONE = 0, BACKEND_MTCH2120, BACKEND_MPR121 } backend_t;

static backend_t s_backend = BACKEND_NONE;

esp_err_t hal_touch_init(void)
{
    if (s_backend != BACKEND_NONE) return ESP_OK;

    if (i2c_master_probe(hal_i2c_bus_handle(), MTCH2120_ADDR, 50) == ESP_OK &&
        mtch2120_init() == ESP_OK) {
        s_backend = BACKEND_MTCH2120;
        ESP_LOGI(TAG, "backend MTCH2120 (0x%02X)", MTCH2120_ADDR);
        return ESP_OK;
    }

    if (i2c_master_probe(hal_i2c_bus_handle(), MPR121_ADDR, 50) == ESP_OK) {
        esp_err_t ret = mpr121_init();
        if (ret != ESP_OK) return ret;
        s_backend = BACKEND_MPR121;
        ESP_LOGI(TAG, "backend MPR121 (0x%02X)", MPR121_ADDR);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "aucun contrôleur tactile détecté (0x%02X/0x%02X)",
             MTCH2120_ADDR, MPR121_ADDR);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t hal_touch_read(hal_touch_data_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    if (s_backend == BACKEND_MTCH2120) {
        mtch2120_data_t d;
        esp_err_t ret = mtch2120_read(&d);
        if (ret != ESP_OK) return ret;
        out->touched = d.touched;
        for (int i = 0; i < HAL_TOUCH_NUM_CH; i++) out->ch[i] = d.ch[i];
        return ESP_OK;
    }
    if (s_backend == BACKEND_MPR121) {
        mpr121_data_t d;
        esp_err_t ret = mpr121_read(&d);
        if (ret != ESP_OK) return ret;
        out->touched = d.touched;
        for (int i = 0; i < HAL_TOUCH_NUM_CH; i++) out->ch[i] = d.ch[i];
        return ESP_OK;
    }
    return ESP_ERR_INVALID_STATE;
}

bool hal_touch_is_touched(uint8_t channel)
{
    if (s_backend == BACKEND_MTCH2120) return mtch2120_is_touched(channel);
    if (s_backend == BACKEND_MPR121)   return mpr121_is_touched(channel);
    return false;
}

int hal_touch_first_touched(void)
{
    if (s_backend == BACKEND_MTCH2120) return mtch2120_first_touched();
    if (s_backend == BACKEND_MPR121)   return mpr121_first_touched();
    return -1;
}
