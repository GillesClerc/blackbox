#include "audio.h"
#include "i2c_bus.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "minimp3.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>

#define TAG "audio"

#define PCM5122_REG_PAGE    0x00
#define PCM5122_REG_RESET   0x01
#define PCM5122_REG_POWER   0x02
#define PCM5122_REG_MUTE    0x03
#define PCM5122_REG_PLLSEL  0x0D
#define PCM5122_REG_DACSEL  0x0E
#define PCM5122_REG_IGNORE  0x25
#define PCM5122_REG_VOL_L   0x3D
#define PCM5122_REG_VOL_R   0x3E

#define SR              44100
#define CHUNK_SAMPLES   (SR * 20 / 1000)  // 20ms chunks — granularité interruption bg
#define FADE_IN_S       (SR * 5  / 1000)  // 5ms  fade-in
#define FADE_OUT_S      (SR * 12 / 1000)  // 12ms fade-out
#define AMPLITUDE_FG     3000.0f          // foreground (bips touch, équilibré vs MP3 bg)
#define AMPLITUDE_BG     1000.0f          // background tons synthétiques (fallback sans MP3)

static i2s_chan_handle_t        s_tx       = NULL;
static i2c_master_dev_handle_t  s_dac      = NULL;
static SemaphoreHandle_t        s_mutex    = NULL;
static atomic_bool              s_fg_play;   // M3 : accès concurrent bg/fg — atomic_bool C11
static atomic_bool              s_bg_run;    // SMP : lu par la tâche bg (core 0), écrit depuis d'autres cores
static atomic_bool              s_bg_exited; // posé par la tâche bg juste avant vTaskDelete(NULL)
static volatile uint8_t         s_peak_level = 0;
static TaskHandle_t             s_bg_task  = NULL;
static const audio_bg_note_t   *s_bg_notes = NULL;
static int                      s_bg_count = 0;

// Buffer silence statique (BSS, déjà zéro)
static int16_t s_silence[CHUNK_SAMPLES * 2];

// C1 : buffer tone alloué une seule fois dans audio_init(), protégé par s_mutex.
// Évite un malloc/free à chaque bip (fragmentation heap, latence non déterministe).
static int16_t *s_tone_buf = NULL;

// ── I2C helpers ───────────────────────────────────────────────────────────────

static esp_err_t pcm_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dac, buf, 2, 50);
}

// ── Génération waveform interne ───────────────────────────────────────────────

// interruptible=true : sort dès que s_fg_play devient vrai (avec fade-out court).
static void write_tone_internal(uint16_t freq, uint16_t dur_ms,
                                 bool interruptible, float amp)
{
    const size_t total     = (size_t)SR * dur_ms / 1000;
    const size_t fade_in   = FADE_IN_S;
    const size_t fade_out  = FADE_OUT_S;

    // C1 : utilise le buffer statique alloué dans audio_init(), pas de malloc ici.
    int16_t *buf = s_tone_buf;
    if (!buf) return;

    float phase = 0.0f;
    const float delta = 2.0f * M_PI * freq / (float)SR;
    size_t played = 0;
    bool interrupted = false;

    while (played < total) {
        if (interruptible && atomic_load(&s_fg_play)) {
            interrupted = true;
            break;
        }

        size_t n = CHUNK_SAMPLES < total - played ? CHUNK_SAMPLES : total - played;
        for (size_t i = 0; i < n; i++) {
            size_t pos = played + i;
            float env;
            if (total <= fade_in + fade_out) {
                // Ton très court : triangle
                env = pos < total / 2
                    ? (float)pos * 2.0f / total
                    : (float)(total - pos) * 2.0f / total;
            } else if (pos < fade_in) {
                env = (float)pos / fade_in;
            } else if (pos + fade_out >= total) {
                env = (float)(total - pos) / fade_out;
            } else {
                env = 1.0f;
            }
            int16_t s = (int16_t)(sinf(phase) * amp * env);
            buf[i * 2]     = s;
            buf[i * 2 + 1] = s;
            phase += delta;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        size_t w;
        i2s_channel_write(s_tx, buf, n * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(200));
        played += n;
    }

    // Fade-out si interrompu (évite le clic)
    if (interrupted) {
        size_t n = fade_out < (size_t)CHUNK_SAMPLES ? fade_out : CHUNK_SAMPLES;
        for (size_t i = 0; i < n; i++) {
            float env = (float)(n - i) / n;
            int16_t s = (int16_t)(sinf(phase) * amp * env);
            buf[i * 2] = buf[i * 2 + 1] = s;
            phase += delta;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        size_t w;
        i2s_channel_write(s_tx, buf, n * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(100));
    }

    // Silence de queue (évite le crunch DMA)
    size_t w;
    i2s_channel_write(s_tx, s_silence, CHUNK_SAMPLES * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(100));
    // Note : pas de free() — buf = s_tone_buf statique (C1).
}

// ── Tâche musique de fond ─────────────────────────────────────────────────────

static void bg_task_fn(void *arg)
{
    int idx = 0;
    while (atomic_load(&s_bg_run)) {
        if (atomic_load(&s_fg_play)) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        const audio_bg_note_t *n = &s_bg_notes[idx % s_bg_count];

        if (n->freq > 0) {
            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                if (!atomic_load(&s_fg_play))
                    write_tone_internal(n->freq, n->dur_ms, true, AMPLITUDE_BG);
                xSemaphoreGive(s_mutex);
            }
        }

        // Gap : alimenter le DMA avec du silence (mutex obligatoire — même canal I2S)
        uint32_t gap_ms = n->gap_ms;
        while (gap_ms > 0 && atomic_load(&s_bg_run)) {
            if (atomic_load(&s_fg_play)) {
                vTaskDelay(pdMS_TO_TICKS(20));
                gap_ms = gap_ms > 20 ? gap_ms - 20 : 0;
                continue;
            }
            if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(30)) == pdTRUE) {
                if (!atomic_load(&s_fg_play)) {
                    size_t w;
                    i2s_channel_write(s_tx, s_silence,
                                      CHUNK_SAMPLES * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(30));
                }
                xSemaphoreGive(s_mutex);
            }
            gap_ms = gap_ms > 20 ? gap_ms - 20 : 0;
        }

        idx = (idx + 1) % s_bg_count;
    }
    atomic_store(&s_bg_exited, true);
    vTaskDelete(NULL);
}

// ── API publique ──────────────────────────────────────────────────────────────

esp_err_t audio_init(i2c_master_bus_handle_t bus)
{
    // C1 : allouer le buffer tone une seule fois (DMA-capable pour cohérence).
    atomic_init(&s_fg_play, false);
    atomic_init(&s_bg_run, false);
    atomic_init(&s_bg_exited, true);
    s_tone_buf = heap_caps_malloc(CHUNK_SAMPLES * 2 * sizeof(int16_t), MALLOC_CAP_DEFAULT);
    if (!s_tone_buf) {
        ESP_LOGE(TAG, "audio_init: alloc tone_buf échoué");
        return ESP_ERR_NO_MEM;
    }

    // ── 1. I2S : slots 32-bit → BCLK = 44100×64 = 2.82 MHz
    //    Le PCM5122 PLL multiplie ×4 (au lieu de ×8 en 16-bit slots) → moins de jitter
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear    = true;
    chan_cfg.dma_desc_num  = 8;
    chan_cfg.dma_frame_num = 480;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SR),
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
    // Forcer slot 32-bit : données 16-bit left-aligned + 16 zéros de padding
    std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    std_cfg.slot_cfg.ws_width       = 32;

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));

    gpio_set_drive_capability(AUDIO_PIN_BCLK, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_PIN_LRCK, GPIO_DRIVE_CAP_1);
    gpio_set_drive_capability(AUDIO_PIN_DOUT, GPIO_DRIVE_CAP_1);

    vTaskDelay(pdMS_TO_TICKS(100));

    // ── 1b. Probe ciblé PCM5122 + MPR121 (scan complet inutilement lent en v6.1 :
    //        chaque adresse libre timeout à 50 ms → ~5 s pour la plage 0x08-0x77).
    if (i2c_master_probe(bus, PCM5122_I2C_ADDR, 50) == ESP_OK)
        ESP_LOGI(TAG, "PCM5122 présent à 0x%02X", PCM5122_I2C_ADDR);
    else
        ESP_LOGW(TAG, "PCM5122 ABSENT à 0x%02X — audio désactivé probable", PCM5122_I2C_ADDR);

    // ── 2. PCM5122
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCM5122_I2C_ADDR,
        .scl_speed_hz    = I2C_BUS_FREQ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &s_dac));

    // PCM5122 en mode I2C (MODE pins à GND) → config PLL complète obligatoire.
    // BCLK = 44100×64 = 2,822,400 Hz. PLL cible = BCLK×16 = 45,158,400 Hz.
    // m6 : vérifier le premier accès I2C — si KO, le DAC est absent ou non alimenté.
    {
        esp_err_t probe = pcm_write(0x00, 0x00);  // Page 0
        if (probe != ESP_OK) {
            ESP_LOGE(TAG, "PCM5122 inaccessible (0x%02X): %s — init audio abandonnée",
                     PCM5122_I2C_ADDR, esp_err_to_name(probe));
            return probe;
        }
    }
    pcm_write(0x02, 0x10);   // Standby pendant config
    pcm_write(0x25, 0x7B);   // Ignorer erreurs clock (SCK halt, detect, PLL unlock)
    pcm_write(0x0D, 0x10);   // PLL source = BCK
    pcm_write(0x0E, 0x10);   // DAC clock = PLL
    pcm_write(0x04, 0x01);   // PLL enable
    // PLL: 2,822,400 × 16 / 1 = 45,158,400 Hz (1024×fs)
    pcm_write(0x14, 0x01);   // P = 1
    pcm_write(0x15, 0x10);   // J = 16
    pcm_write(0x16, 0x00);   // D[13:8] = 0
    pcm_write(0x17, 0x00);   // D[7:0] = 0
    pcm_write(0x18, 0x01);   // R = 1
    // Dividers : PLL(45.16MHz) → DAC/DSP/NCP
    pcm_write(0x1B, 0x01);   // DSP divider = 1 → 45.16 MHz
    pcm_write(0x1C, 0x04);   // DAC divider = 4 → 11.29 MHz = 256×fs
    pcm_write(0x1D, 0x04);   // NCP divider = 4 → 11.29 MHz
    pcm_write(0x1E, 0x00);   // OSR = auto
    pcm_write(0x2B, 0x07);   // DSP program 7 : ringing-less low latency FIR
    // Analog gain (page 1 reg 0x02) laissé à défaut 0 dB
    pcm_write(0x41, 0x00);   // Auto-mute time = 0
    pcm_write(0x42, 0x00);   // Auto-mute control = disable
    pcm_write(0x3F, 0x74);   // Volume ramp : montée rapide (7), descente moyenne (4)
    pcm_write(0x28, 0x00);   // I2S format
    // Volume digital : 0x00=+24dB, 0x30=0dB, 0xFF=mute, step=0.5dB
    pcm_write(0x3D, 0x30);   // Vol L = 0 dB
    pcm_write(0x3E, 0x30);   // Vol R = 0 dB
    pcm_write(0x03, 0x00);   // Unmute
    pcm_write(0x02, 0x00);   // Exit standby → PLL lock sur BCK
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t reg = 0x05;
    uint8_t status = 0;
    esp_err_t err = i2c_master_transmit_receive(s_dac, &reg, 1, &status, 1, 50);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "PCM5122 status=0x%02X PLL %s", status,
                 (status & 0x10) ? "locked" : "NOT locked");
    }

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    ESP_LOGI(TAG, "I2S démarré (BCLK=%d LRCK=%d DOUT=%d)", AUDIO_PIN_BCLK, AUDIO_PIN_LRCK, AUDIO_PIN_DOUT);
    return ESP_OK;
}

void audio_play_tone(uint16_t freq_hz, uint16_t dur_ms)
{
    if (!s_tx || !freq_hz || !dur_ms) return;
    atomic_store(&s_fg_play, true);
    // Timeout 50ms : un bip cosmétique qui rate parce que le canal est occupé
    // n'est pas un bug, l'appelant peut juste passer à autre chose. Évite que
    // touch_task (prio 5) ou scenario_engine bloquent 500ms si l'audio est busy.
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        write_tone_internal(freq_hz, dur_ms, false, AMPLITUDE_FG);
        atomic_store(&s_fg_play, false);
        xSemaphoreGive(s_mutex);
    } else {
        atomic_store(&s_fg_play, false);
    }
}

void audio_play_sequence(const uint16_t *freqs, const uint16_t *durs, int count, uint16_t gap_ms)
{
    if (!s_tx || !count) return;
    atomic_store(&s_fg_play, true);
    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        for (int i = 0; i < count; i++) {
            if (freqs[i] > 0)
                write_tone_internal(freqs[i], durs[i], false, AMPLITUDE_FG);
            if (gap_ms > 0 && i < count - 1) {
                size_t gap_s = (size_t)SR * gap_ms / 1000;
                size_t w;
                while (gap_s > 0) {
                    size_t n = gap_s < CHUNK_SAMPLES ? gap_s : CHUNK_SAMPLES;
                    i2s_channel_write(s_tx, s_silence, n * 2 * sizeof(int16_t), &w, pdMS_TO_TICKS(100));
                    gap_s -= n;
                }
            }
        }
        atomic_store(&s_fg_play, false);
        xSemaphoreGive(s_mutex);
    } else {
        atomic_store(&s_fg_play, false);
    }
}

esp_err_t audio_play_raw(const int16_t *samples, size_t num_samples, uint32_t sr)
{
    if (!s_tx) return ESP_ERR_INVALID_STATE;

    // C3 : arrêter la tâche bg proprement avant de toucher le canal I2S,
    // puis prendre le mutex pour sérialiser avec tout accès concurrent.
    atomic_store(&s_bg_run, false);
    atomic_store(&s_fg_play, true);

    if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        atomic_store(&s_fg_play, false);
        ESP_LOGE(TAG, "audio_play_raw: timeout mutex");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    if (sr != SR) {
        i2s_channel_disable(s_tx);
        i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(sr);
        ret = i2s_channel_reconfig_std_clock(s_tx, &clk);
        i2s_channel_enable(s_tx);
    }

    if (ret == ESP_OK) {
        size_t w;
        // Timeout dimensionné sur la durée réelle du sample (stéréo) + 1s de marge.
        // portMAX_DELAY gèlerait la tâche appelante (invisible du TWDT) sur un DMA bloqué.
        uint32_t dur_ms = (uint32_t)((uint64_t)num_samples * 1000 / ((sr ? sr : SR) * 2));
        ret = i2s_channel_write(s_tx, samples, num_samples * sizeof(int16_t), &w,
                                pdMS_TO_TICKS(dur_ms + 1000));
        if (ret == ESP_ERR_TIMEOUT)
            ESP_LOGW(TAG, "audio_play_raw: timeout I2S (%u samples)", (unsigned)num_samples);
    }

    atomic_store(&s_fg_play, false);
    xSemaphoreGive(s_mutex);
    return ret;
}

void audio_set_volume(uint8_t vol_pct)
{
    if (!s_dac) return;
    // PCM5122 VOL: 0x00=+24dB, 0x30=0dB, 0xFE=-103dB, 0xFF=mute
    // raw = (dB - 24) * -2.  On mappe 100%→0dB(0x30), 0%→mute(0xFF)
    uint8_t v;
    if (vol_pct == 0)        v = 0xFF;
    else if (vol_pct >= 100) v = 0x30;
    else                     v = (uint8_t)(0x30 + (uint32_t)(100 - vol_pct) * (0xFF - 0x30) / 100);
    pcm_write(PCM5122_REG_VOL_L, v);
    pcm_write(PCM5122_REG_VOL_R, v);
}

void audio_set_dsp_filter(uint8_t program)
{
    if (!s_dac || program < 1 || program > 7) return;
    pcm_write(0x02, 0x10);   // standby (requis pour changer le filtre)
    pcm_write(0x2B, program);
    pcm_write(0x02, 0x00);   // exit standby
}

void audio_set_analog_gain(uint8_t step)
{
    if (!s_dac) return;
    // Analog gain sur Page 1, registre 0x02 : 0x00=0dB, 0x11=-6dB
    pcm_write(0x00, 0x01);   // select page 1
    pcm_write(0x02, step ? 0x11 : 0x00);
    pcm_write(0x00, 0x00);   // retour page 0
}

void audio_stop(void)
{
    audio_bg_stop();
    if (s_dac) {
        pcm_write(PCM5122_REG_MUTE,  0x11);
        pcm_write(PCM5122_REG_POWER, 0x10);
    }
}

// ── Musique de fond MP3 ───────────────────────────────────────────────────────

#define MP3_BG_VOLUME   0.50f   // 50% amplitude : fond sonore sous les bips foreground

static const uint8_t *s_mp3_data = NULL;
static size_t         s_mp3_size = 0;

// Buffers statiques pour éviter 7 KB de stack dans la tâche
static int16_t s_mp3_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
static int16_t s_mp3_stereo[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];
static mp3dec_t s_mp3_dec;

static void bg_mp3_task_fn(void *arg)
{
    mp3dec_init(&s_mp3_dec);
    const uint8_t *ptr       = s_mp3_data;
    int            remaining = (int)s_mp3_size;

    while (atomic_load(&s_bg_run)) {
        if (atomic_load(&s_fg_play)) { vTaskDelay(pdMS_TO_TICKS(20)); continue; }

        if (remaining < 4) {
            mp3dec_init(&s_mp3_dec);
            ptr       = s_mp3_data;
            remaining = (int)s_mp3_size;
            ESP_LOGD(TAG, "MP3 loop");
            taskYIELD();
            continue;
        }

        mp3dec_frame_info_t info;
        int samples = mp3dec_decode_frame(&s_mp3_dec, ptr, remaining,
                                          s_mp3_pcm, &info);

        if (info.frame_bytes > 0) {
            ptr       += info.frame_bytes;
            remaining -= info.frame_bytes;
        } else {
            ptr++; remaining--;
            taskYIELD();
            continue;
        }

        if (samples <= 0) { taskYIELD(); continue; }

        // Mono → stéréo entrelacé + réduction volume
        int out_s = samples * 2;
        if (info.channels == 1) {
            for (int i = 0; i < samples; i++) {
                int16_t v = (int16_t)(s_mp3_pcm[i] * MP3_BG_VOLUME);
                s_mp3_stereo[i * 2]     = v;
                s_mp3_stereo[i * 2 + 1] = v;
            }
        } else {
            for (int i = 0; i < out_s; i++)
                s_mp3_stereo[i] = (int16_t)(s_mp3_pcm[i] * MP3_BG_VOLUME);
        }

        int16_t peak = 0;
        for (int i = 0; i < out_s; i++) {
            int16_t abs_v = s_mp3_stereo[i] < 0 ? -s_mp3_stereo[i] : s_mp3_stereo[i];
            if (abs_v > peak) peak = abs_v;
        }
        s_peak_level = (uint8_t)((uint32_t)peak * 100 / 32768);

        if (atomic_load(&s_fg_play)) continue;

        if (xSemaphoreTake(s_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!atomic_load(&s_fg_play)) {
                size_t w;
                i2s_channel_write(s_tx, s_mp3_stereo,
                                  out_s * sizeof(int16_t), &w, pdMS_TO_TICKS(200));
            }
            xSemaphoreGive(s_mutex);
        }
    }
    s_peak_level = 0;
    atomic_store(&s_bg_exited, true);
    vTaskDelete(NULL);
}

void audio_bg_mp3_start(const uint8_t *data, size_t size)
{
    if (!data || !size) return;
    audio_bg_stop();
    s_mp3_data = data;
    s_mp3_size = size;
    atomic_store(&s_bg_exited, false);
    atomic_store(&s_bg_run, true);
    // C2 : stack 32 KB — mp3dec_scratch_t sur pile ~16 KB + headroom FreeRTOS + débordements potentiels
    // (grbuf[2][576]=4608 + syn[33][64]=8448 + maindata[2815] + reste + overhead)
    // Pinned core 0 prio 3 : éviter de préempter eye_task (prio 4 core 1).
    // Décodage minimp3 = ~5ms CPU par frame de 26ms audio (~20% load), prio 3
    // largement suffisante. Core 0 cohérent avec touch/audio I2C.
    xTaskCreatePinnedToCore(bg_mp3_task_fn, "audio_bg_mp3", 32768, NULL, 3, &s_bg_task, 0);
    ESP_LOGI(TAG, "bg MP3 démarré (%u kB)", (unsigned)(size / 1024));
}

void audio_bg_start(const audio_bg_note_t *notes, int count)
{
    if (!notes || count <= 0) return;
    audio_bg_stop();
    s_bg_notes = notes;
    s_bg_count = count;
    atomic_store(&s_bg_exited, false);
    atomic_store(&s_bg_run, true);
    // Mêmes raisons que bg_mp3 : pinné core 0 prio 3.
    xTaskCreatePinnedToCore(bg_task_fn, "audio_bg", 4096, NULL, 3, &s_bg_task, 0);
    ESP_LOGI(TAG, "bg music démarrée (%d notes)", count);
}

void audio_bg_stop(void)
{
    atomic_store(&s_bg_run, false);
    if (s_bg_task) {
        // M3 : ne jamais déréférencer s_bg_task (TOCTOU — la tâche bg peut se
        // terminer entre le test et l'usage, et eTaskGetState(NULL) est UB).
        // On attend le flag atomique s_bg_exited posé juste avant vTaskDelete(NULL).
        const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(500);
        while (!atomic_load(&s_bg_exited)) {
            if (xTaskGetTickCount() >= deadline) {
                ESP_LOGW(TAG, "audio_bg_stop: tâche bg non terminée dans les temps");
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        s_bg_task = NULL;
    }
}

// ── SPIFFS-based background MP3 playback ─────────────────────────────────────

// Buffer holding the file contents between calls; freed on next call or stop.
static uint8_t *s_file_mp3_buf  = NULL;
static size_t   s_file_mp3_cap  = 0;

// Maximum file size accepted (4 MB in PSRAM).
#define AUDIO_PLAY_BG_MAX_SIZE (4 * 1024 * 1024)

uint8_t audio_get_peak_level(void)
{
    return s_peak_level;
}

esp_err_t audio_play_bg(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;

    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGW(TAG, "audio_play_bg: cannot open %s", path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize <= 0 || (size_t)fsize > AUDIO_PLAY_BG_MAX_SIZE) {
        fclose(f);
        ESP_LOGW(TAG, "audio_play_bg: file too large or empty (%ld bytes)", fsize);
        return ESP_FAIL;
    }

    // Stop current background playback before touching the buffer.
    audio_bg_stop();

    // Reuse or reallocate PSRAM buffer.
    if (s_file_mp3_buf && s_file_mp3_cap < (size_t)fsize) {
        heap_caps_free(s_file_mp3_buf);
        s_file_mp3_buf = NULL;
        s_file_mp3_cap = 0;
    }
    if (!s_file_mp3_buf) {
        s_file_mp3_buf = heap_caps_malloc((size_t)fsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_file_mp3_buf) {
            fclose(f);
            ESP_LOGE(TAG, "audio_play_bg: PSRAM alloc failed (%ld bytes)", fsize);
            return ESP_ERR_NO_MEM;
        }
        s_file_mp3_cap = (size_t)fsize;
    }

    size_t nread = fread(s_file_mp3_buf, 1, (size_t)fsize, f);
    fclose(f);

    if (nread != (size_t)fsize) {
        ESP_LOGW(TAG, "audio_play_bg: short read %u/%ld", (unsigned)nread, fsize);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "audio_play_bg: %s (%u kB)", path, (unsigned)(nread / 1024));
    audio_bg_mp3_start(s_file_mp3_buf, nread);
    return ESP_OK;
}
