#include "as5600.h"
#include "i2c_bus.h"
#include "esp_log.h"

#define TAG "as5600"

// Registres
#define REG_STATUS      0x0B  // [5]=MD, [4]=ML, [3]=MH
#define REG_RAW_ANGLE_H 0x0C
#define REG_ANGLE_H     0x0E

#define STATUS_MD  (1 << 5)  // aimant détecté
#define STATUS_ML  (1 << 4)  // trop faible
#define STATUS_MH  (1 << 3)  // trop fort

static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t as5600_init(i2c_master_bus_handle_t bus) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = AS5600_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_dev));

    uint8_t status;
    esp_err_t ret = i2c_bus_read_reg(s_dev, REG_STATUS, &status);
    if (ret != ESP_OK) return ret;

    if (!(status & STATUS_MD)) {
        ESP_LOGW(TAG, "aimant absent (status=0x%02X)", status);
        return ESP_ERR_NOT_FOUND;
    }
    if (status & STATUS_ML) ESP_LOGW(TAG, "aimant trop faible");
    if (status & STATUS_MH) ESP_LOGW(TAG, "aimant trop fort");

    ESP_LOGI(TAG, "AS5600 initialisé (status=0x%02X)", status);
    return ESP_OK;
}

esp_err_t as5600_read(as5600_data_t *out) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    // Fonction runtime : jamais d'ESP_ERROR_CHECK ici (abort() sur glitch I2C).
    uint8_t status;
    esp_err_t ret = i2c_bus_read_reg(s_dev, REG_STATUS, &status);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "as5600_read: I2C %s", esp_err_to_name(ret)); return ret; }

    uint8_t raw[2], ang[2];
    ret = i2c_bus_read_regs(s_dev, REG_RAW_ANGLE_H, raw, 2);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "as5600_read: I2C %s", esp_err_to_name(ret)); return ret; }
    ret = i2c_bus_read_regs(s_dev, REG_ANGLE_H, ang, 2);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "as5600_read: I2C %s", esp_err_to_name(ret)); return ret; }

    out->raw_angle = ((uint16_t)(raw[0] & 0x0F) << 8) | raw[1];
    out->angle     = ((uint16_t)(ang[0] & 0x0F) << 8) | ang[1];
    out->angle_deg = (float)out->angle * 360.0f / 4096.0f;
    out->magnet_ok = (status & STATUS_MD) && !(status & STATUS_ML) && !(status & STATUS_MH);
    return ESP_OK;
}

float as5600_angle_deg(void) {
    as5600_data_t d;
    if (as5600_read(&d) != ESP_OK) return -1.0f;
    return d.angle_deg;
}
