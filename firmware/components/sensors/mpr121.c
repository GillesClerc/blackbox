#include "mpr121.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "mpr121"

// Registres
#define REG_TOUCH_STATUS_0  0x00    // canaux 7-0
#define REG_TOUCH_STATUS_1  0x01    // canaux 11-8 (bits [3:0])
#define REG_MHD_RISING      0x2B
#define REG_NHD_RISING      0x2C
#define REG_NCL_RISING      0x2D
#define REG_FDL_RISING      0x2E
#define REG_MHD_FALLING     0x2F
#define REG_NHD_FALLING     0x30
#define REG_NCL_FALLING     0x31
#define REG_FDL_FALLING     0x32
#define REG_ELE0_TOUCH_TH   0x41    // pattern : 0x41 + 2*n pour ELEn touch
#define REG_ELE0_RELEASE_TH 0x42    // pattern : 0x42 + 2*n pour ELEn release
#define REG_ECR             0x5E    // Electrode Configuration Register
#define REG_SRST            0x80    // Soft Reset (écrire 0x63)

static i2c_master_dev_handle_t s_dev = NULL;

static esp_err_t write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, 2, 100);
}

esp_err_t mpr121_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MPR121_ADDR,
        .scl_speed_hz    = I2C_BUS_FREQ,
    };
    // C4 : check explicite — ESP_ERROR_CHECK interdit hors _init(), et mpr121_init
    // est appelé depuis touch_task qui gère l'absence du périphérique (ESP_LOGW + delete).
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_master_bus_add_device: %s", esp_err_to_name(ret));
        return ret;
    }

#define WR(r, v) do { esp_err_t e = write_reg((r),(v)); if (e != ESP_OK) return e; } while(0)

    WR(REG_SRST, 0x63);
    vTaskDelay(pdMS_TO_TICKS(10));

    WR(REG_ECR, 0x00);
    WR(REG_MHD_RISING,  0x01);
    WR(REG_NHD_RISING,  0x01);
    WR(REG_NCL_RISING,  0x0E);
    WR(REG_FDL_RISING,  0x00);
    WR(REG_MHD_FALLING, 0x01);
    WR(REG_NHD_FALLING, 0x05);
    WR(REG_NCL_FALLING, 0x01);
    WR(REG_FDL_FALLING, 0x00);

    for (int i = 0; i < MPR121_NUM_CH; i++) {
        WR(REG_ELE0_TOUCH_TH   + 2 * i, MPR121_TOUCH_TH);
        WR(REG_ELE0_RELEASE_TH + 2 * i, MPR121_RELEASE_TH);
    }

    WR(REG_ECR, 0x8C);
#undef WR

    ESP_LOGI(TAG, "MPR121 initialisé (%d canaux, seuils touch=%d release=%d)",
             MPR121_NUM_CH, MPR121_TOUCH_TH, MPR121_RELEASE_TH);
    return ESP_OK;
}

esp_err_t mpr121_read(mpr121_data_t *out)
{
    if (!s_dev) return ESP_ERR_INVALID_STATE;

    uint8_t buf[2];
    esp_err_t ret = i2c_bus_read_regs(s_dev, REG_TOUCH_STATUS_0, buf, 2);
    if (ret != ESP_OK) return ret;

    out->touched = (uint16_t)buf[0] | ((uint16_t)(buf[1] & 0x0F) << 8);
    for (int i = 0; i < MPR121_NUM_CH; i++) {
        out->ch[i] = (out->touched >> i) & 1;
    }
    return ESP_OK;
}

bool mpr121_is_touched(uint8_t channel)
{
    if (channel >= MPR121_NUM_CH) return false;
    mpr121_data_t d;
    if (mpr121_read(&d) != ESP_OK) return false;
    return d.ch[channel];
}

int mpr121_first_touched(void)
{
    mpr121_data_t d;
    if (mpr121_read(&d) != ESP_OK) return -1;
    for (int i = 0; i < MPR121_NUM_CH; i++) {
        if (d.ch[i]) return i;
    }
    return -1;
}
