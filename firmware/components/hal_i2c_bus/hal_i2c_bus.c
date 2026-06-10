#include "hal_i2c_bus_priv.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

#define TAG "hal_i2c_bus"

static i2c_master_bus_handle_t s_bus = NULL;

esp_err_t hal_i2c_bus_init(void) {
    if (s_bus) return ESP_OK;  // déjà initialisé

    i2c_master_bus_config_t cfg = {
        .i2c_port            = I2C_NUM_0,
        .sda_io_num          = I2C_BUS_SDA,
        .scl_io_num          = I2C_BUS_SCL,
        .clk_source          = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt   = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&cfg, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "bus I2C initialisé (SDA=%d, SCL=%d, %dkHz)",
             I2C_BUS_SDA, I2C_BUS_SCL, I2C_BUS_FREQ / 1000);
    return ESP_OK;
}

i2c_master_bus_handle_t hal_i2c_bus_handle(void) {
    return s_bus;
}

esp_err_t hal_i2c_bus_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(dev, buf, 2, 100);
}

esp_err_t hal_i2c_bus_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out) {
    return i2c_master_transmit_receive(dev, &reg, 1, out, 1, 100);
}

esp_err_t hal_i2c_bus_read_regs(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, 100);
}
