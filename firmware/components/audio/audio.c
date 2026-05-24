#include "audio.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define TAG "audio"

// Registres PCM5122PW (page 0)
#define PCM5122_REG_PAGE    0x00
#define PCM5122_REG_RESET   0x01
#define PCM5122_REG_POWER   0x02
#define PCM5122_REG_MUTE    0x03
#define PCM5122_REG_PLLSEL  0x0D  // PLL clock source: 0x01 = BCK
#define PCM5122_REG_DACSEL  0x0E  // DAC clock source: 0x10 = PLL
#define PCM5122_REG_IGNORE  0x25  // ignore clock detection errors
#define PCM5122_REG_VOL_L   0x3D  // DACL volume: 0x00=mute, 0x78=0dBFS
#define PCM5122_REG_VOL_R   0x3E  // DACR volume: idem

#define I2S_SAMPLE_RATE  44100
#define TONE_BUF_MS      50      // buffer pour la génération de tons

static i2s_chan_handle_t    s_tx       = NULL;
static i2c_master_dev_handle_t s_dac   = NULL;
static uint8_t              s_vol      = 100;

// --- I2C helpers ---

static esp_err_t pcm_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dac, buf, 2, pdMS_TO_TICKS(50));
}

// --- Init ---

esp_err_t audio_init(i2c_master_bus_handle_t bus) {
    // Ajout du DAC sur le bus I2C
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCM5122_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_dac));

    // Init PCM5122PW
    pcm_write(PCM5122_REG_PAGE, 0x00);  // page 0
    pcm_write(PCM5122_REG_RESET, 0x11); // full reset
    vTaskDelay(pdMS_TO_TICKS(15));
    pcm_write(PCM5122_REG_RESET, 0x00); // normal
    pcm_write(PCM5122_REG_POWER, 0x00); // power up
    pcm_write(PCM5122_REG_PLLSEL, 0x01); // PLL ref = BCK
    pcm_write(PCM5122_REG_DACSEL, 0x10); // DAC clk = PLL
    pcm_write(PCM5122_REG_IGNORE, 0x01); // ignore auto-detect errors au boot
    pcm_write(PCM5122_REG_VOL_L, 0x78);  // DACL : 0dBFS
    pcm_write(PCM5122_REG_VOL_R, 0x78);  // DACR : 0dBFS
    pcm_write(PCM5122_REG_MUTE, 0x00);   // unmute
    ESP_LOGI(TAG, "PCM5122PW initialisé");

    // Init I2S
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = AUDIO_PIN_BCLK,
            .ws   = AUDIO_PIN_LRCK,
            .dout = AUDIO_PIN_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    vTaskDelay(pdMS_TO_TICKS(200));  // PLL lock PCM5122
    ESP_LOGI(TAG, "I2S démarré (BCLK=%d, LRCK=%d, DOUT=%d)",
             AUDIO_PIN_BCLK, AUDIO_PIN_LRCK, AUDIO_PIN_DOUT);
    return ESP_OK;
}

// --- Playback ---

esp_err_t audio_play_raw(const int16_t *samples, size_t num_samples, uint32_t sample_rate_hz) {
    if (!s_tx) return ESP_ERR_INVALID_STATE;

    // Reconfigurer le sample rate si nécessaire
    if (sample_rate_hz != I2S_SAMPLE_RATE) {
        i2s_channel_disable(s_tx);
        i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate_hz);
        i2s_channel_reconfig_std_clock(s_tx, &clk);
        i2s_channel_enable(s_tx);
    }

    size_t written;
    return i2s_channel_write(s_tx, samples, num_samples * sizeof(int16_t),
                             &written, portMAX_DELAY);
}

void audio_play_tone(uint16_t freq_hz, uint16_t duration_ms) {
    if (!s_tx || freq_hz == 0 || duration_ms == 0) return;

    const uint32_t sr     = I2S_SAMPLE_RATE;
    const size_t total    = (uint32_t)sr * duration_ms / 1000;
    const size_t chunk    = sr * TONE_BUF_MS / 1000;
    int16_t *buf          = malloc(chunk * 2 * sizeof(int16_t));
    if (!buf) return;

    float phase = 0.0f;
    const float delta = 2.0f * M_PI * freq_hz / (float)sr;
    size_t remaining = total;

    while (remaining > 0) {
        size_t n = remaining < chunk ? remaining : chunk;
        for (size_t i = 0; i < n; i++) {
            int16_t s = (int16_t)(sinf(phase) * 16383.0f);
            buf[i * 2]     = s;
            buf[i * 2 + 1] = s;
            phase += delta;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        size_t written;
        i2s_channel_write(s_tx, buf, n * 2 * sizeof(int16_t), &written, portMAX_DELAY);
        vTaskDelay(1);  // yield watchdog entre chunks
        remaining -= n;
    }
    free(buf);
}

void audio_set_volume(uint8_t vol_percent) {
    if (!s_dac) return;
    s_vol = vol_percent > 100 ? 100 : vol_percent;
    // 0% → 0x00 (mute), 100% → 0x78 (0dBFS)
    uint8_t reg_val = (uint8_t)((uint32_t)s_vol * 0x78 / 100);
    pcm_write(PCM5122_REG_VOL_L, reg_val);
    pcm_write(PCM5122_REG_VOL_R, reg_val);
}

void audio_stop(void) {
    if (!s_dac) return;
    pcm_write(PCM5122_REG_MUTE, 0x11);   // mute L + R
    pcm_write(PCM5122_REG_POWER, 0x10);  // standby
}
