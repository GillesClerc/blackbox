#include "mtch2120.h"
#include "i2c_bus.h"
#include "esp_log.h"

#define TAG "mtch2120"

// Registres MTCH2120
#define REG_DET_STATUS_0  0x00  // canaux 7-0 (bit=1 : touché)
#define REG_DET_STATUS_1  0x01  // canaux 11-8 (bits [3:0])

static i2c_master_dev_handle_t s_dev   = NULL;
static mtch2120_data_t         s_last  = { 0 };

esp_err_t mtch2120_init(void) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MTCH2120_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &s_dev));

    // Vérifier que le chip répond (lecture du status initial)
    uint8_t buf[2];
    esp_err_t ret = i2c_bus_read_regs(s_dev, REG_DET_STATUS_0, buf, 2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "MTCH2120 non détecté");
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "MTCH2120 initialisé (%d canaux)", MTCH2120_NUM_CH);
    return ESP_OK;
}

esp_err_t mtch2120_read(mtch2120_data_t *out) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    // Fonction runtime : jamais d'ESP_ERROR_CHECK ici (abort() sur glitch I2C).
    uint8_t buf[2];
    esp_err_t ret = i2c_bus_read_regs(s_dev, REG_DET_STATUS_0, buf, 2);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "mtch2120_read: I2C %s", esp_err_to_name(ret)); return ret; }

    out->touched = (uint16_t)buf[0] | ((uint16_t)(buf[1] & 0x0F) << 8);
    for (int i = 0; i < MTCH2120_NUM_CH; i++) {
        out->ch[i] = (out->touched >> i) & 1;
    }
    s_last = *out;
    return ESP_OK;
}

bool mtch2120_is_touched(uint8_t channel) {
    if (channel >= MTCH2120_NUM_CH) return false;
    mtch2120_data_t d;
    if (mtch2120_read(&d) != ESP_OK) return false;
    return d.ch[channel];
}

int mtch2120_first_touched(void) {
    mtch2120_data_t d;
    if (mtch2120_read(&d) != ESP_OK) return -1;
    for (int i = 0; i < MTCH2120_NUM_CH; i++) {
        if (d.ch[i]) return i;
    }
    return -1;
}
