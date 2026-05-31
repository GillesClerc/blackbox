#include "xpt2046.h"
#include "ili9488.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "lvgl.h"

#define TAG "xpt2046"

// XPT2046 command bytes (12-bit, differential, PD=01 keeps ADC on)
#define CMD_X_ADC_ON  0xD1
#define CMD_Y_ADC_ON  0x91
#define CMD_Z1_ADC_ON 0xB1
#define CMD_Z2_ADC_ON 0xC1
#define CMD_PWRDOWN   0xD0  // power down + enable PENIRQ

#define CAL_X_MIN  200
#define CAL_X_MAX  3850
#define CAL_Y_MIN  200
#define CAL_Y_MAX  3850

#define Z_THRESHOLD 300

static spi_device_handle_t s_touch_spi;
static uint16_t xpt_cmd(uint8_t cmd);

esp_err_t xpt2046_init(void)
{
    gpio_config_t irq_cfg = {
        .pin_bit_mask = 1ULL << XPT2046_PIN_IRQ,
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&irq_cfg);

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 1 * 1000 * 1000,
        .mode           = 0,
        .spics_io_num   = XPT2046_PIN_CS,
        .queue_size     = 1,
    };
    esp_err_t ret = spi_bus_add_device(SPI2_HOST, &dev, &s_touch_spi);
    if (ret != ESP_OK) return ret;

    // Enable PENIRQ output by sending power-down command
    xpt_cmd(CMD_PWRDOWN);
    xpt_cmd(CMD_PWRDOWN);

    ESP_LOGI(TAG, "XPT2046 init OK (CS=%d IRQ=%d)", XPT2046_PIN_CS, XPT2046_PIN_IRQ);
    return ESP_OK;
}

// Send command and read the result of the PREVIOUS command (pipelined)
static uint16_t xpt_cmd(uint8_t cmd)
{
    uint8_t tx[3] = { cmd, 0x00, 0x00 };
    uint8_t rx[3] = {0};
    spi_transaction_t t = {
        .length    = 24,
        .tx_buffer = tx,
        .rx_buffer = rx,
        .flags     = 0,
    };
    spi_device_polling_transmit(s_touch_spi, &t);
    return ((uint16_t)rx[1] << 8 | rx[2]) >> 3;
}

bool xpt2046_read(uint16_t *x, uint16_t *y)
{
    // PENIRQ is LOW when touched (re-enabled by CMD_PWRDOWN at end of previous read)
    if (gpio_get_level(XPT2046_PIN_IRQ) != 0)
        return false;

    // Read Z1/Z2 for pressure detection
    xpt_cmd(CMD_Z1_ADC_ON);          // send Z1 cmd, discard previous
    uint16_t z1 = xpt_cmd(CMD_Z2_ADC_ON);  // send Z2 cmd, read Z1
    uint16_t z2 = xpt_cmd(CMD_X_ADC_ON);   // send X cmd, read Z2

    int z = (int)z1 + 4095 - (int)z2;
    if (z < Z_THRESHOLD) {
        xpt_cmd(CMD_PWRDOWN);         // power down, re-enable PENIRQ
        xpt_cmd(CMD_PWRDOWN);
        return false;
    }

    // Discard first X (always noisy after channel switch)
    xpt_cmd(CMD_Y_ADC_ON);           // send Y cmd, discard noisy X

    // Read 3 pairs X/Y with pipelining
    uint32_t sum_x = 0, sum_y = 0;
    int valid = 0;
    for (int i = 0; i < 3; i++) {
        uint16_t ry = xpt_cmd(CMD_X_ADC_ON);  // send X cmd, read Y
        uint16_t rx = xpt_cmd(CMD_Y_ADC_ON);  // send Y cmd, read X
        if (rx > 100 && rx < 4000 && ry > 100 && ry < 4000) {
            sum_x += rx;
            sum_y += ry;
            valid++;
        }
    }

    // Power down + re-enable PENIRQ
    xpt_cmd(CMD_PWRDOWN);
    xpt_cmd(CMD_PWRDOWN);

    if (valid == 0)
        return false;

    uint16_t raw_x = sum_x / valid;
    uint16_t raw_y = sum_y / valid;

    if (raw_x < CAL_X_MIN) raw_x = CAL_X_MIN;
    if (raw_x > CAL_X_MAX) raw_x = CAL_X_MAX;
    if (raw_y < CAL_Y_MIN) raw_y = CAL_Y_MIN;
    if (raw_y > CAL_Y_MAX) raw_y = CAL_Y_MAX;

    // Landscape: touch X → screen X, touch Y → screen Y (invert both for MADCTL 0x28)
    *x = (ILI9488_WIDTH  - 1) - (uint16_t)(((uint32_t)(raw_x - CAL_X_MIN) * (ILI9488_WIDTH  - 1)) / (CAL_X_MAX - CAL_X_MIN));
    *y = (ILI9488_HEIGHT - 1) - (uint16_t)(((uint32_t)(raw_y - CAL_Y_MIN) * (ILI9488_HEIGHT - 1)) / (CAL_Y_MAX - CAL_Y_MIN));

    return true;
}

static uint32_t s_press_cnt = 0;

void xpt2046_lvgl_read(void *indev_ptr, void *data_ptr)
{
    lv_indev_t *indev = indev_ptr;
    lv_indev_data_t *data = data_ptr;
    (void)indev;

    uint16_t x, y;
    if (xpt2046_read(&x, &y)) {
        data->point.x = x;
        data->point.y = y;
        data->state   = LV_INDEV_STATE_PRESSED;
        if ((s_press_cnt++ % 10) == 0)
            ESP_LOGI(TAG, "PRESS x=%u y=%u", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
