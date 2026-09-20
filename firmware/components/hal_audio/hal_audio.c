#include "hal_audio.h"
#include "hal_i2c_bus_priv.h"
#include "esp_log.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "minimp3.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>

#define TAG "hal_audio"

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
#define CHUNK_FRAMES    (SR * 20 / 1000)  // 20 ms — granularité du mixer
#define FADE_IN_S       (SR * 5  / 1000)  // 5 ms  fade-in des tons
#define FADE_OUT_S      (SR * 12 / 1000)  // 12 ms fade-out des tons
#define AMPLITUDE_FG     3000.0f          // bips (équilibré vs MP3)
#define AMPLITUDE_BG     1000.0f          // ambiance tons fallback

#define MP3_BG_VOLUME   0.50f   // musique de fond sous les voix/bips
#define DUCK_GAIN       0.35f   // atténuation du fond pendant un one-shot
#define TONE_QUEUE_LEN  32

// Fichiers MP3 acceptés (bg comme one-shot) : 4 MB en PSRAM.
#define AUDIO_FILE_MAX_SIZE (4 * 1024 * 1024)

static i2s_chan_handle_t       s_tx  = NULL;
static i2c_master_dev_handle_t s_dac = NULL;

// ─── Mixer 4 voix ─────────────────────────────────────────────────────────────
// Une seule task (audio_mixer) écrit sur l'I2S. Les voix sont sommées en 32
// bits avec saturation par blocs de 20 ms :
//   bg MP3 (boucle, ducké) + one-shot MP3 + bips fg (queue) + tons bg fallback.
// Contrat de concurrence : les champs d'une voix MP3 (data/size/loop) ne sont
// écrits par l'API que voix inactive (handshake req_active/is_active), la
// task mixer est la seule à toucher l'état de décodage.

// Voix MP3 : source en mémoire (flash embarquée ou PSRAM), décodée frame par
// frame dans un FIFO PCM stéréo. mp3dec_t reste en .bss interne (état chaud).
typedef struct {
    atomic_bool    req_active;  // demandé par l'API
    atomic_bool    is_active;   // état réel, écrit par le mixer
    const uint8_t *data;        // valides uniquement voix inactive (handshake)
    size_t         size;
    bool           loop;
} mp3_ctrl_t;

typedef struct {
    bool           on;         // état mixer
    const uint8_t *data;
    size_t         size;
    const uint8_t *ptr;
    int            remaining;
    bool           loop;
    int16_t       *pcm;        // PSRAM : MINIMP3_MAX_SAMPLES_PER_FRAME*2 échantillons
    int            pcm_len, pcm_pos;  // en échantillons int16 (stéréo entrelacé)
    mp3dec_t       dec;
} mp3_voice_t;

static mp3_ctrl_t  s_bg_ctrl, s_one_ctrl;
static mp3_voice_t s_bg_voice, s_one_voice;

// Voix tons : une note en cours (sinus + enveloppe), puis un gap de silence.
typedef struct {
    bool     active;
    uint16_t freq;      // 0 = silence (le gap et la durée comptent quand même)
    float    amp;
    size_t   total, pos;  // frames de la note
    size_t   gap;         // frames de silence après
    float    phase;
} tone_state_t;

typedef struct {
    uint16_t freq, dur_ms, gap_ms;
} tone_evt_t;

static QueueHandle_t s_tone_queue;   // bips fg → mixer
static tone_state_t  s_tonefg_state;

// Ambiance tons fallback : config posée par l'API (sous mutex) avant req=true.
static atomic_bool                s_tonebg_req;
static const hal_audio_bg_note_t *s_tonebg_cfg_notes;
static int                        s_tonebg_cfg_count;
static bool                       s_tonebg_on;    // état mixer
static const hal_audio_bg_note_t *s_tonebg_notes;
static int                        s_tonebg_count, s_tonebg_idx;
static tone_state_t               s_tonebg_state;

// Buffers du mixer (PSRAM, alloués une fois au boot).
static int32_t *s_acc;   // accumulation 32 bits, CHUNK_FRAMES*2
static int16_t *s_out;   // sortie s16,            CHUNK_FRAMES*2
// Frame de décodage partagée (une seule voix décodée à la fois, task mixer).
static int16_t s_frame_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];

static float            s_duck = 1.0f;   // rampe de ducking (task mixer)
static volatile uint8_t s_peak_level;

// Fichiers MP3 chargés depuis SD/FS (bg et one-shot), réutilisés entre appels.
static uint8_t *s_bgfile_buf;
static size_t   s_bgfile_cap;
static uint8_t *s_onefile_buf;
static size_t   s_onefile_cap;

static SemaphoreHandle_t s_api_mutex;  // sérialise les appels API (pas la task mixer)
static bool              s_mixer_ok;

// ── I2C helpers ───────────────────────────────────────────────────────────────

static esp_err_t pcm_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dac, buf, 2, 50);
}

// ── Voix MP3 (task mixer) ─────────────────────────────────────────────────────

// Mixe jusqu'à `frames` frames de la voix dans acc (gain en Q8).
static void mp3_voice_mix(mp3_voice_t *v, mp3_ctrl_t *c, int32_t *acc,
                          int frames, float gain)
{
    if (!atomic_load(&c->req_active)) {
        if (v->on) {
            v->on = false;
            atomic_store(&c->is_active, false);
        }
        return;
    }
    if (!v->on) {  // activation : l'API a posé data/size/loop avant req=true
        v->data      = c->data;
        v->size      = c->size;
        v->loop      = c->loop;
        v->ptr       = v->data;
        v->remaining = (int)v->size;
        v->pcm_len   = 0;
        v->pcm_pos   = 0;
        mp3dec_init(&v->dec);
        v->on = true;
        atomic_store(&c->is_active, true);
    }

    const int gq     = (int)(gain * 256.0f);
    const int needed = frames * 2;
    int       filled = 0;
    while (filled < needed) {
        if (v->pcm_pos >= v->pcm_len) {
            if (v->remaining < 4) {
                if (!v->loop || v->size < 4) break;  // fini (ou source vide)
                v->ptr       = v->data;
                v->remaining = (int)v->size;
                mp3dec_init(&v->dec);
            }
            mp3dec_frame_info_t info;
            int samples = mp3dec_decode_frame(&v->dec, v->ptr, v->remaining,
                                              s_frame_pcm, &info);
            if (info.frame_bytes > 0) {
                v->ptr       += info.frame_bytes;
                v->remaining -= info.frame_bytes;
            } else {
                v->ptr++;
                v->remaining--;
                continue;
            }
            if (samples <= 0) continue;

            if (info.channels == 1) {
                for (int i = 0; i < samples; i++) {
                    v->pcm[2 * i]     = s_frame_pcm[i];
                    v->pcm[2 * i + 1] = s_frame_pcm[i];
                }
            } else {
                memcpy(v->pcm, s_frame_pcm, (size_t)samples * 2 * sizeof(int16_t));
            }
            v->pcm_len = samples * 2;
            v->pcm_pos = 0;
        }

        int take = needed - filled;
        if (take > v->pcm_len - v->pcm_pos) take = v->pcm_len - v->pcm_pos;
        for (int i = 0; i < take; i++) {
            acc[filled + i] += ((int32_t)v->pcm[v->pcm_pos + i] * gq) >> 8;
        }
        filled += take;
        v->pcm_pos += take;
    }

    if (filled < needed && !v->loop) {  // fin du one-shot
        v->on = false;
        atomic_store(&c->req_active, false);
        atomic_store(&c->is_active, false);
    }
}

// ── Voix tons (task mixer) ────────────────────────────────────────────────────

static void tone_start(tone_state_t *t, uint16_t freq, uint16_t dur_ms,
                       uint16_t gap_ms, float amp)
{
    t->freq   = freq;
    t->amp    = amp;
    t->total  = (size_t)SR * dur_ms / 1000;
    t->pos    = 0;
    t->gap    = (size_t)SR * gap_ms / 1000;
    t->phase  = 0.0f;
    t->active = (t->total > 0 || t->gap > 0);
}

// Mixe la note en cours (sinus + enveloppe anti-crissement) dans acc.
static void tone_mix(tone_state_t *t, int32_t *acc, int frames)
{
    if (!t->active) return;
    const float delta = 2.0f * (float)M_PI * t->freq / (float)SR;
    for (int i = 0; i < frames; i++) {
        if (t->pos < t->total) {
            if (t->freq) {
                float env;
                if (t->total <= (size_t)(FADE_IN_S + FADE_OUT_S)) {
                    env = t->pos < t->total / 2
                        ? (float)t->pos * 2.0f / t->total
                        : (float)(t->total - t->pos) * 2.0f / t->total;
                } else if (t->pos < (size_t)FADE_IN_S) {
                    env = (float)t->pos / FADE_IN_S;
                } else if (t->pos + FADE_OUT_S >= t->total) {
                    env = (float)(t->total - t->pos) / FADE_OUT_S;
                } else {
                    env = 1.0f;
                }
                int32_t s = (int32_t)(sinf(t->phase) * t->amp * env);
                acc[2 * i]     += s;
                acc[2 * i + 1] += s;
                t->phase += delta;
                if (t->phase > 2.0f * (float)M_PI) t->phase -= 2.0f * (float)M_PI;
            }
            t->pos++;
        } else if (t->gap > 0) {
            t->gap--;
        } else {
            t->active = false;
            return;
        }
    }
}

// Ambiance tons fallback : enchaîne les notes en boucle.
static void tonebg_step(void)
{
    if (!atomic_load(&s_tonebg_req)) {
        s_tonebg_on           = false;
        s_tonebg_state.active = false;
        return;
    }
    if (!s_tonebg_on) {  // activation : copie de la config posée par l'API
        s_tonebg_notes        = s_tonebg_cfg_notes;
        s_tonebg_count        = s_tonebg_cfg_count;
        s_tonebg_idx          = 0;
        s_tonebg_state.active = false;
        s_tonebg_on           = true;
    }
    if (!s_tonebg_state.active && s_tonebg_count > 0) {
        const hal_audio_bg_note_t *n = &s_tonebg_notes[s_tonebg_idx];
        s_tonebg_idx = (s_tonebg_idx + 1) % s_tonebg_count;
        tone_start(&s_tonebg_state, n->freq, n->dur_ms, n->gap_ms, AMPLITUDE_BG);
    }
}

// ── Task mixer ────────────────────────────────────────────────────────────────

static void mixer_task_fn(void *arg)
{
    // Instrumentation : production vs temps réel. 250 chunks = 5 s d'audio ;
    // si le temps réel écoulé dépasse nettement 5 s → le mixer ne suit pas.
    int64_t  stat_t0    = esp_timer_get_time();
    int64_t  busy_us    = 0;
    int64_t  worst_us   = 0;
    uint32_t chunks     = 0;

    for (;;) {
        int64_t t_prod = esp_timer_get_time();
        memset(s_acc, 0, (size_t)CHUNK_FRAMES * 2 * sizeof(int32_t));

        tonebg_step();
        tone_mix(&s_tonebg_state, s_acc, CHUNK_FRAMES);

        if (!s_tonefg_state.active) {
            tone_evt_t evt;
            if (xQueueReceive(s_tone_queue, &evt, 0) == pdTRUE) {
                tone_start(&s_tonefg_state, evt.freq, evt.dur_ms, evt.gap_ms,
                           AMPLITUDE_FG);
            }
        }
        tone_mix(&s_tonefg_state, s_acc, CHUNK_FRAMES);

        // Ducking du fond pendant un one-shot — rampe ~80 ms, pas de step audible.
        float target = atomic_load(&s_one_ctrl.is_active) ? DUCK_GAIN : 1.0f;
        s_duck += (target - s_duck) * 0.25f;

        mp3_voice_mix(&s_bg_voice, &s_bg_ctrl, s_acc, CHUNK_FRAMES,
                      MP3_BG_VOLUME * s_duck);
        mp3_voice_mix(&s_one_voice, &s_one_ctrl, s_acc, CHUNK_FRAMES, 1.0f);

        // Saturation 32→16 bits + niveau de crête.
        int32_t peak = 0;
        for (int i = 0; i < CHUNK_FRAMES * 2; i++) {
            int32_t v = s_acc[i];
            if (v > 32767)  v = 32767;
            if (v < -32768) v = -32768;
            s_out[i] = (int16_t)v;
            int32_t a = v < 0 ? -v : v;
            if (a > peak) peak = a;
        }
        s_peak_level = (uint8_t)(peak * 100 / 32768);

        int64_t prod = esp_timer_get_time() - t_prod;
        busy_us += prod;
        if (prod > worst_us) worst_us = prod;

        // Le blocage sur le DMA (~20 ms de données) cadence la boucle. Timeout
        // dimensionné sur le chunk + marge — jamais portMAX_DELAY.
        size_t    w;
        esp_err_t e = i2s_channel_write(s_tx, s_out,
                                        (size_t)CHUNK_FRAMES * 2 * sizeof(int16_t),
                                        &w, pdMS_TO_TICKS(20 + 1000));
        if (e == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "mixer : timeout I2S");
        }
        if (w != (size_t)CHUNK_FRAMES * 2 * sizeof(int16_t)) {
            ESP_LOGW(TAG, "mixer : écriture partielle %u", (unsigned)w);
        }

        // Sentinelle temps réel : n'alerte que si la production décroche
        // (5 s d'audio en > 5,3 s réel, ou un chunk > 100 ms — tampon 130 ms).
        // Diag 2026-07 : production mesurée en temps réel exact, sortie
        // vérifiée propre sur banc host — les craquements du proto viennent du
        // câblage breadboard (A/B identique avec l'ancien pipeline).
        if (++chunks == 250) {
            int64_t wall = esp_timer_get_time() - stat_t0;
            if (wall > 5300000 || worst_us > 100000) {
                ESP_LOGW(TAG, "mixer en retard : 5000 ms audio en %lld ms (pire chunk %lld µs)",
                         wall / 1000, worst_us);
            }
            stat_t0  = esp_timer_get_time();
            busy_us  = 0;
            worst_us = 0;
            chunks   = 0;
        }
        (void)busy_us;
    }
}

// ── Handshake API ↔ mixer ─────────────────────────────────────────────────────

// Désactive une voix MP3 et attend que le mixer l'ait relâchée (≤ 500 ms) —
// après quoi son buffer source peut être libéré/réécrit sans danger.
static void voice_stop_wait(mp3_ctrl_t *c)
{
    atomic_store(&c->req_active, false);
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(500);
    while (atomic_load(&c->is_active)) {
        if (xTaskGetTickCount() >= deadline) {
            ESP_LOGW(TAG, "voix MP3 non relâchée dans les temps");
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Charge un fichier dans un buffer PSRAM réutilisable (realloc si trop petit).
// À n'appeler que voix arrêtée (voice_stop_wait) — le buffer peut être rejoué.
static esp_err_t load_file_psram(const char *path, uint8_t **buf, size_t *cap,
                                 size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return ESP_ERR_NOT_FOUND;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0 || (size_t)fsize > AUDIO_FILE_MAX_SIZE) {
        fclose(f);
        ESP_LOGW(TAG, "%s : taille invalide (%ld octets)", path, fsize);
        return ESP_FAIL;
    }

    if (*buf && *cap < (size_t)fsize) {
        heap_caps_free(*buf);
        *buf = NULL;
        *cap = 0;
    }
    if (!*buf) {
        *buf = heap_caps_malloc((size_t)fsize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!*buf) {
            fclose(f);
            ESP_LOGE(TAG, "%s : alloc PSRAM échouée (%ld octets)", path, fsize);
            return ESP_ERR_NO_MEM;
        }
        *cap = (size_t)fsize;
    }

    size_t nread = fread(*buf, 1, (size_t)fsize, f);
    fclose(f);
    if (nread != (size_t)fsize) {
        ESP_LOGW(TAG, "%s : lecture incomplète %u/%ld", path, (unsigned)nread, fsize);
        return ESP_FAIL;
    }
    *out_size = (size_t)fsize;
    return ESP_OK;
}

// ── API publique ──────────────────────────────────────────────────────────────

esp_err_t hal_audio_init(void)
{
    // ── 1. I2S : slots 32-bit → BCLK = 44100×64 = 2.82 MHz
    //    Le PCM5122 PLL multiplie ×4 (au lieu de ×8 en 16-bit slots) → moins de jitter
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear    = true;
    // 12 × 480 frames ≈ 130 ms de tampon DMA : marge contre les pics CPU
    // (TLS/sha256 de cloud_client au boot) sans latence perceptible.
    chan_cfg.dma_desc_num  = 12;
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

    // ── 1b. Probe ciblé PCM5122 (scan complet inutilement lent en v6.1 :
    //        chaque adresse libre timeout à 50 ms → ~5 s pour la plage 0x08-0x77).
    if (i2c_master_probe(hal_i2c_bus_handle(), PCM5122_I2C_ADDR, 50) == ESP_OK)
        ESP_LOGI(TAG, "PCM5122 présent à 0x%02X", PCM5122_I2C_ADDR);
    else
        ESP_LOGW(TAG, "PCM5122 ABSENT à 0x%02X — audio désactivé probable", PCM5122_I2C_ADDR);

    // ── 2. PCM5122
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = PCM5122_I2C_ADDR,
        .scl_speed_hz    = I2C_BUS_FREQ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(hal_i2c_bus_handle(), &dev_cfg, &s_dac));

    // PCM5122 en mode I2C (MODE1→GND, MODE2→3V3_A) → config PLL complète obligatoire.
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

    // ── 3. Mixer : buffers PSRAM (pas de malloc au runtime) + task unique.
    atomic_init(&s_bg_ctrl.req_active, false);
    atomic_init(&s_bg_ctrl.is_active, false);
    atomic_init(&s_one_ctrl.req_active, false);
    atomic_init(&s_one_ctrl.is_active, false);
    atomic_init(&s_tonebg_req, false);

    // Buffers du mixer en PSRAM : la RAM interne est réservée aux stacks
    // WiFi/BLE (le banc host + la sentinelle prouvent que le débit PSRAM
    // suffit largement au mix temps réel).
    s_acc = heap_caps_malloc((size_t)CHUNK_FRAMES * 2 * sizeof(int32_t),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_acc) s_acc = heap_caps_malloc((size_t)CHUNK_FRAMES * 2 * sizeof(int32_t),
                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_out = heap_caps_malloc((size_t)CHUNK_FRAMES * 2 * sizeof(int16_t),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_out) s_out = heap_caps_malloc((size_t)CHUNK_FRAMES * 2 * sizeof(int16_t),
                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_bg_voice.pcm  = heap_caps_malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * 2 * sizeof(int16_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_one_voice.pcm = heap_caps_malloc(MINIMP3_MAX_SAMPLES_PER_FRAME * 2 * sizeof(int16_t),
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_acc || !s_out || !s_bg_voice.pcm || !s_one_voice.pcm) {
        ESP_LOGE(TAG, "alloc buffers mixer échouée");
        return ESP_ERR_NO_MEM;
    }

    s_tone_queue = xQueueCreate(TONE_QUEUE_LEN, sizeof(tone_evt_t));
    s_api_mutex  = xSemaphoreCreateMutex();
    if (!s_tone_queue || !s_api_mutex) return ESP_ERR_NO_MEM;

    // Core 0 prio 4 : temps réel — doit préempter cloud_client (prio 3, TLS +
    // sha256 au boot), sinon underruns DMA (craquements, musique « ralentie »).
    // Sous scenario_engine/touch (5) : l'input garde la main. Stack 32 KB (le
    // scratch minimp3 vit sur la pile) en PSRAM : la RAM interne est réservée
    // à WiFi+BLE. Licite car ce task ne touche JAMAIS la flash SPI ni la NVS
    // (décodage + I2S uniquement — contrainte des stacks en PSRAM).
    BaseType_t ok = xTaskCreatePinnedToCoreWithCaps(mixer_task_fn, "audio_mixer",
                                                    32768, NULL, 4, NULL, 0,
                                                    MALLOC_CAP_SPIRAM);
    if (ok != pdPASS) return ESP_FAIL;
    s_mixer_ok = true;

    ESP_LOGI(TAG, "I2S + mixer démarrés (BCLK=%d LRCK=%d DOUT=%d)",
             AUDIO_PIN_BCLK, AUDIO_PIN_LRCK, AUDIO_PIN_DOUT);
    return ESP_OK;
}

void hal_audio_play_tone(uint16_t freq_hz, uint16_t dur_ms)
{
    if (!s_tone_queue || !freq_hz || !dur_ms) return;
    tone_evt_t evt = { .freq = freq_hz, .dur_ms = dur_ms, .gap_ms = 0 };
    if (xQueueSend(s_tone_queue, &evt, 0) != pdTRUE) {
        ESP_LOGD(TAG, "queue tons pleine — bip ignoré");
    }
}

void hal_audio_play_sequence(const uint16_t *freqs, const uint16_t *durs,
                             int count, uint16_t gap_ms)
{
    if (!s_tone_queue || !freqs || !durs || count <= 0) return;
    for (int i = 0; i < count; i++) {
        tone_evt_t evt = {
            .freq   = freqs[i],
            .dur_ms = durs[i],
            .gap_ms = (i < count - 1) ? gap_ms : 0,
        };
        if (xQueueSend(s_tone_queue, &evt, pdMS_TO_TICKS(10)) != pdTRUE) {
            ESP_LOGW(TAG, "queue tons pleine — séquence tronquée (%d/%d)", i, count);
            return;
        }
    }
}

esp_err_t hal_audio_play_oneshot(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;
    if (!s_mixer_ok) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_api_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    voice_stop_wait(&s_one_ctrl);

    size_t    size = 0;
    esp_err_t err  = load_file_psram(path, &s_onefile_buf, &s_onefile_cap, &size);
    if (err == ESP_OK) {
        s_one_ctrl.data = s_onefile_buf;
        s_one_ctrl.size = size;
        s_one_ctrl.loop = false;
        atomic_store(&s_one_ctrl.req_active, true);
        ESP_LOGI(TAG, "one-shot %s (%u kB)", path, (unsigned)(size / 1024));
    }
    xSemaphoreGive(s_api_mutex);
    return err;
}

void hal_audio_oneshot_stop(void)
{
    if (!s_api_mutex) return;
    if (xSemaphoreTake(s_api_mutex, pdMS_TO_TICKS(500)) != pdTRUE) return;
    voice_stop_wait(&s_one_ctrl);
    xSemaphoreGive(s_api_mutex);
}

bool hal_audio_oneshot_active(void)
{
    return atomic_load(&s_one_ctrl.is_active);
}

void hal_audio_bg_mp3_start(const uint8_t *data, size_t size)
{
    if (!data || !size || !s_mixer_ok) return;
    if (xSemaphoreTake(s_api_mutex, pdMS_TO_TICKS(500)) != pdTRUE) return;

    voice_stop_wait(&s_bg_ctrl);
    atomic_store(&s_tonebg_req, false);  // une seule musique de fond à la fois

    s_bg_ctrl.data = data;
    s_bg_ctrl.size = size;
    s_bg_ctrl.loop = true;
    atomic_store(&s_bg_ctrl.req_active, true);

    xSemaphoreGive(s_api_mutex);
    ESP_LOGI(TAG, "bg MP3 démarré (%u kB)", (unsigned)(size / 1024));
}

void hal_audio_bg_start(const hal_audio_bg_note_t *notes, int count)
{
    if (!notes || count <= 0 || !s_mixer_ok) return;
    if (xSemaphoreTake(s_api_mutex, pdMS_TO_TICKS(500)) != pdTRUE) return;

    voice_stop_wait(&s_bg_ctrl);
    s_tonebg_cfg_notes = notes;
    s_tonebg_cfg_count = count;
    atomic_store(&s_tonebg_req, true);

    xSemaphoreGive(s_api_mutex);
    ESP_LOGI(TAG, "bg tons démarré (%d notes)", count);
}

void hal_audio_bg_stop(void)
{
    if (!s_api_mutex) return;
    if (xSemaphoreTake(s_api_mutex, pdMS_TO_TICKS(500)) != pdTRUE) return;
    voice_stop_wait(&s_bg_ctrl);
    atomic_store(&s_tonebg_req, false);
    xSemaphoreGive(s_api_mutex);
}

esp_err_t hal_audio_play_bg(const char *path)
{
    if (!path) return ESP_ERR_INVALID_ARG;
    if (!s_mixer_ok) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_api_mutex, pdMS_TO_TICKS(500)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Arrêter la voix bg AVANT de toucher le buffer (elle peut le lire).
    voice_stop_wait(&s_bg_ctrl);
    atomic_store(&s_tonebg_req, false);

    size_t    size = 0;
    esp_err_t err  = load_file_psram(path, &s_bgfile_buf, &s_bgfile_cap, &size);
    if (err == ESP_OK) {
        s_bg_ctrl.data = s_bgfile_buf;
        s_bg_ctrl.size = size;
        s_bg_ctrl.loop = true;
        atomic_store(&s_bg_ctrl.req_active, true);
        ESP_LOGI(TAG, "bg MP3 fichier %s (%u kB)", path, (unsigned)(size / 1024));
    }
    xSemaphoreGive(s_api_mutex);
    return err;
}

uint8_t hal_audio_get_peak_level(void)
{
    return s_peak_level;
}

void hal_audio_set_volume(uint8_t vol_pct)
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

void hal_audio_set_dsp_filter(uint8_t program)
{
    if (!s_dac || program < 1 || program > 7) return;
    pcm_write(0x02, 0x10);   // standby (requis pour changer le filtre)
    pcm_write(0x2B, program);
    pcm_write(0x02, 0x00);   // exit standby
}

void hal_audio_set_analog_gain(uint8_t step)
{
    if (!s_dac) return;
    // Analog gain sur Page 1, registre 0x02 : 0x00=0dB, 0x11=-6dB
    pcm_write(0x00, 0x01);   // select page 1
    pcm_write(0x02, step ? 0x11 : 0x00);
    pcm_write(0x00, 0x00);   // retour page 0
}

void hal_audio_stop(void)
{
    hal_audio_bg_stop();
    hal_audio_oneshot_stop();
    if (s_dac) {
        pcm_write(PCM5122_REG_MUTE,  0x11);
        pcm_write(PCM5122_REG_POWER, 0x10);
    }
}
