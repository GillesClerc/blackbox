#include "veml7700.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "veml7700"

// Registres (little-endian 16-bit)
#define REG_ALS_CONF  0x00  // config : gain + intégration + shutdown
#define REG_ALS       0x04  // valeur ALS brute
#define REG_WHITE     0x05  // valeur white brute

// ALS_CONF : gain=1x (bits[12:11]=00), IT=100ms (bits[9:6]=0000), no shutdown (bit0=0)
#define ALS_CONF_DEFAULT  0x0000

// Résolution lux pour gain=1x, IT=100ms : 0.0576 lux/count
#define LUX_RESOLUTION  0.0576f

static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t veml_write16(uint8_t reg, uint16_t val) {
    uint8_t buf[3] = { reg, val & 0xFF, (val >> 8) & 0xFF };
    return i2c_master_transmit(s_dev, buf, 3, pdMS_TO_TICKS(50));
}

static esp_err_t veml_read16(uint8_t reg, uint16_t *out) {
    uint8_t buf[2];
    esp_err_t ret = i2c_master_transmit_receive(s_dev, &reg, 1, buf, 2, pdMS_TO_TICKS(50));
    if (ret == ESP_OK) *out = (uint16_t)(buf[0] | (buf[1] << 8));
    return ret;
}

esp_err_t veml7700_init(i2c_master_bus_handle_t bus) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = VEML7700_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev));

    // Wake up + config gain=1x, IT=100ms
    ESP_ERROR_CHECK(veml_write16(REG_ALS_CONF, ALS_CONF_DEFAULT));
    vTaskDelay(pdMS_TO_TICKS(110));  // attend 1 intégration avant lecture

    ESP_LOGI(TAG, "VEML7700 initialisé (gain=1x, IT=100ms)");
    return ESP_OK;
}

esp_err_t veml7700_read(veml7700_data_t *out) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    // Fonction runtime : jamais d'ESP_ERROR_CHECK ici (abort() sur glitch I2C).
    esp_err_t ret = veml_read16(REG_ALS, &out->als_raw);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "veml7700_read: I2C %s", esp_err_to_name(ret)); return ret; }
    ret = veml_read16(REG_WHITE, &out->white);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "veml7700_read: I2C %s", esp_err_to_name(ret)); return ret; }
    out->lux = (float)out->als_raw * LUX_RESOLUTION;
    return ESP_OK;
}

float veml7700_lux(void) {
    veml7700_data_t d;
    if (veml7700_read(&d) != ESP_OK) return -1.0f;
    return d.lux;
}
