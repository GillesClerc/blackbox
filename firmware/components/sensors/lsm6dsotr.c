#include "lsm6dsotr.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#define TAG "lsm6"

// Registres
#define REG_WHO_AM_I  0x0F  // valeur attendue : 0x6C
#define REG_CTRL1_XL  0x10  // accéléromètre : ODR + FS
#define REG_CTRL2_G   0x11  // gyroscope : ODR + FS
#define REG_CTRL3_C   0x12  // général (BDU, IF_INC, SW_RESET)
#define REG_STATUS    0x1E  // [1]=GDA, [0]=XLDA
#define REG_TEMP_L    0x20
#define REG_OUTX_L_G  0x22  // 6 octets gyro (X,Y,Z)
#define REG_OUTX_L_A  0x28  // 6 octets accel (X,Y,Z)

// CTRL1_XL : ODR=104Hz (0100), FS=±2g (00)
#define CTRL1_XL_104HZ_2G   0x40
// CTRL2_G  : ODR=104Hz (0100), FS=±250dps (000)
#define CTRL2_G_104HZ_250   0x40
// CTRL3_C  : BDU=1, IF_INC=1
#define CTRL3_C_INIT        0x44

// Sensibilité
#define ACCEL_SENS  0.000598f   // m/s²/LSB pour ±2g (0.061 mg/LSB)
#define GYRO_SENS   0.00875f    // °/s/LSB pour ±250dps

static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t lsm6_init(uint8_t addr) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = addr,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle(), &dev_cfg, &s_dev));

    // WHO_AM_I
    uint8_t who;
    ESP_ERROR_CHECK(i2c_bus_read_reg(s_dev, REG_WHO_AM_I, &who));
    if (who != 0x6C) {
        ESP_LOGE(TAG, "WHO_AM_I=0x%02X (attendu 0x6C)", who);
        return ESP_ERR_NOT_FOUND;
    }

    // Reset software
    ESP_ERROR_CHECK(i2c_bus_write_reg(s_dev, REG_CTRL3_C, 0x01));
    vTaskDelay(pdMS_TO_TICKS(10));

    // Config
    ESP_ERROR_CHECK(i2c_bus_write_reg(s_dev, REG_CTRL3_C, CTRL3_C_INIT));
    ESP_ERROR_CHECK(i2c_bus_write_reg(s_dev, REG_CTRL1_XL, CTRL1_XL_104HZ_2G));
    ESP_ERROR_CHECK(i2c_bus_write_reg(s_dev, REG_CTRL2_G,  CTRL2_G_104HZ_250));

    ESP_LOGI(TAG, "LSM6DSOTR initialisé @ 0x%02X", addr);
    return ESP_OK;
}

esp_err_t lsm6_read(lsm6_data_t *out) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    uint8_t raw[14];  // TEMP(2) + GYRO(6) + ACCEL(6) — reg TEMP_L=0x20 à OUTZ_H_A=0x2D
    // Lire 14 octets depuis REG_TEMP_L (auto-incrément activé via CTRL3_C).
    // Fonction runtime : jamais d'ESP_ERROR_CHECK ici (abort() sur glitch I2C).
    esp_err_t ret = i2c_bus_read_regs(s_dev, REG_TEMP_L, raw, 14);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "lsm6_read: I2C %s", esp_err_to_name(ret));
        return ret;
    }

    int16_t temp_raw = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t gx_raw   = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t gy_raw   = (int16_t)((raw[5] << 8) | raw[4]);
    int16_t gz_raw   = (int16_t)((raw[7] << 8) | raw[6]);
    int16_t ax_raw   = (int16_t)((raw[9]  << 8) | raw[8]);
    int16_t ay_raw   = (int16_t)((raw[11] << 8) | raw[10]);
    int16_t az_raw   = (int16_t)((raw[13] << 8) | raw[12]);

    out->temp_c = 25.0f + (float)temp_raw / 256.0f;
    out->gx = (float)gx_raw * GYRO_SENS;
    out->gy = (float)gy_raw * GYRO_SENS;
    out->gz = (float)gz_raw * GYRO_SENS;
    out->ax = (float)ax_raw * ACCEL_SENS;
    out->ay = (float)ay_raw * ACCEL_SENS;
    out->az = (float)az_raw * ACCEL_SENS;
    return ESP_OK;
}

float lsm6_pitch_deg(const lsm6_data_t *d) {
    // Inclinaison avant (pitch) à partir de ax et az
    return atan2f(d->ax, d->az) * (180.0f / M_PI);
}

esp_err_t lsm6_sleep(void) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    // ODR=0 sur les deux capteurs = power down
    esp_err_t ret = i2c_bus_write_reg(s_dev, REG_CTRL1_XL, 0x00);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "lsm6_sleep: I2C %s", esp_err_to_name(ret)); return ret; }
    ret = i2c_bus_write_reg(s_dev, REG_CTRL2_G, 0x00);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "lsm6_sleep: I2C %s", esp_err_to_name(ret)); return ret; }
    return ESP_OK;
}

esp_err_t lsm6_wake(void) {
    if (!s_dev) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = i2c_bus_write_reg(s_dev, REG_CTRL1_XL, CTRL1_XL_104HZ_2G);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "lsm6_wake: I2C %s", esp_err_to_name(ret)); return ret; }
    ret = i2c_bus_write_reg(s_dev, REG_CTRL2_G, CTRL2_G_104HZ_250);
    if (ret != ESP_OK) { ESP_LOGW(TAG, "lsm6_wake: I2C %s", esp_err_to_name(ret)); return ret; }
    return ESP_OK;
}
