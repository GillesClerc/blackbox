// Port ESP-IDF du pipeline Adafruit "Uncanny Eyes" (MIT, Phil Burgess).
// Adapté du fork GC9A01 de thelastoutpostworkshop / Bodmer (TFT_eSPI).
//
// Différences notables :
//   - SPI/DMA gérés via esp_lcd (driver eyes.c / esp_lcd_gc9a01).
//   - Pas de PROGMEM ni de macros Arduino.
//   - L'œil rendu reste 128×128 (taille des assets defaultEye.h), centré
//     dans la fenêtre 240×240 du GC9A01. Le reste reste noir.
//   - Modulations émotion exposées via eyes_anim_state_t.

#include "eyes_anim.h"
#include "eyes.h"
#include "data/eye_assets.h"
#include "esp_lcd_panel_ops.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

#define TAG "eyes_anim"

#define NUM_EYES        2
#define EYE_RENDER_OFFX ((EYE_WIDTH  - SCREEN_WIDTH)  / 2)  // (240-128)/2 = 56
#define EYE_RENDER_OFFY ((EYE_HEIGHT - SCREEN_HEIGHT) / 2)

// Curve d'ease (3t² - 2t³) — table fournie par Adafruit (256 valeurs).
static const uint8_t s_ease[256] = {
    0,0,0,0,0,0,0,1,1,1,1,1,2,2,2,3,
    3,3,4,4,4,5,5,6,6,7,7,8,9,9,10,10,
    11,12,12,13,14,15,15,16,17,18,18,19,20,21,22,23,
    24,25,26,27,27,28,29,30,31,33,34,35,36,37,38,39,
    40,41,42,44,45,46,47,48,50,51,52,53,54,56,57,58,
    60,61,62,63,65,66,67,69,70,72,73,74,76,77,78,80,
    81,83,84,85,87,88,90,91,93,94,96,97,98,100,101,103,
    104,106,107,109,110,112,113,115,116,118,119,121,122,124,125,127,
    128,130,131,133,134,136,137,139,140,142,143,145,146,148,149,151,
    152,154,155,157,158,159,161,162,164,165,167,168,170,171,172,174,
    175,177,178,179,181,182,183,185,186,188,189,190,192,193,194,195,
    197,198,199,201,202,203,204,205,207,208,209,210,211,213,214,215,
    216,217,218,219,220,221,222,224,225,226,227,228,228,229,230,231,
    232,233,234,235,236,237,237,238,239,240,240,241,242,243,243,244,
    245,245,246,246,247,248,248,249,249,250,250,251,251,251,252,252,
    252,253,253,253,254,254,254,254,254,255,255,255,255,255,255,255
};

// State machine clignement
#define NOBLINK 0
#define ENBLINK 1
#define DEBLINK 2

typedef struct {
    uint8_t  state;
    uint32_t duration_us;
    uint32_t start_us;
} blink_t;

static blink_t  s_blink[NUM_EYES];
static int16_t  s_eye_xpos[NUM_EYES] = { EYE_RENDER_OFFX, EYE_RENDER_OFFX };
static uint16_t s_old_iris;
static uint32_t s_time_of_last_blink_us = 0;
static uint32_t s_time_to_next_blink_us = 0;

// Framebuffers DMA full-frame, un PAR ŒIL (128×128 px × 2 octets = 32 KiB chacun).
//
// On rend la frame complète en RAM, puis un SEUL esp_lcd_panel_draw_bitmap
// pousse toute la fenêtre. Un buffer par œil est nécessaire parce que :
//   - esp_lcd_panel_draw_bitmap est ASYNCHRONE (queue + DMA).
//   - Les deux yeux alternent dans frame(). Si on partageait un seul buffer,
//     le CPU le réécrirait pour l'œil B pendant que le DMA lit encore pour
//     l'œil A → corruption silencieuse, écrans figés/noirs.
// 64 KiB total tient largement dans la SRAM interne (~294 KiB dispo).
static uint16_t *s_framebuf[NUM_EYES] = {NULL, NULL};
#define FRAMEBUF_PX_COUNT  (SCREEN_WIDTH * SCREEN_HEIGHT)
#define FRAMEBUF_BYTES     (FRAMEBUF_PX_COUNT * sizeof(uint16_t))

static inline uint32_t micros(void) { return (uint32_t)esp_timer_get_time(); }
static inline uint32_t millis(void) { return (uint32_t)(esp_timer_get_time() / 1000); }

static inline int32_t rand_range(int32_t lo, int32_t hi)
{
    if (hi <= lo) return lo;
    uint32_t span = (uint32_t)(hi - lo);
    return lo + (int32_t)(esp_random() % span);
}

static inline int16_t map_i(int16_t v, int16_t in_min, int16_t in_max,
                            int16_t out_min, int16_t out_max)
{
    return (int16_t)((int32_t)(v - in_min) * (out_max - out_min)
                     / (in_max - in_min) + out_min);
}

esp_err_t eyes_anim_init(void)
{
    s_old_iris = (IRIS_MIN + IRIS_MAX) / 2;
    for (int e = 0; e < NUM_EYES; e++) {
        s_blink[e].state = NOBLINK;
    }
    for (int i = 0; i < NUM_EYES; i++) {
        s_framebuf[i] = heap_caps_malloc(FRAMEBUF_BYTES, MALLOC_CAP_DMA);
        if (!s_framebuf[i]) {
            ESP_LOGE(TAG, "framebuf[%d] alloc failed (%u bytes)",
                     i, (unsigned)FRAMEBUF_BYTES);
            return ESP_ERR_NO_MEM;
        }
    }
    return ESP_OK;
}

void eyes_anim_state_init(eyes_anim_state_t *s)
{
    memset(s, 0, sizeof(*s));
}

// Rendu d'un œil : un seul appel = une frame complète 128×128.
// Coordonnées dans le repère sclera (200×200) déjà calculées par frame().
static void draw_eye(uint8_t e, uint32_t iScale,
                     uint32_t sclera_x, uint32_t sclera_y,
                     uint32_t uT, uint32_t lT,
                     bool forced_closed)
{
    if (forced_closed) {
        uT = 255;
        lT = 255;
    }

    esp_lcd_panel_handle_t panel = eyes_panel(e == 0 ? EYE_LEFT : EYE_RIGHT);
    if (!panel) return;

    int32_t irisY = (int32_t)sclera_y - (SCLERA_HEIGHT - IRIS_HEIGHT) / 2;

    // Symétrie horizontale des paupières entre les deux yeux (œil R = mirror).
    int  dlidX = (e == 0) ? -1 : 1;

    eye_id_t eye = (e == 0) ? EYE_LEFT : EYE_RIGHT;

    // Rendu en RAM dans le framebuffer dédié à cet œil, puis 1 seul draw_bitmap.
    uint16_t *fb = s_framebuf[e];
    for (int screenY = 0; screenY < SCREEN_HEIGHT;
         screenY++, sclera_y++, irisY++) {

        uint16_t *line = fb + screenY * SCREEN_WIDTH;

        uint32_t scleraX = sclera_x;
        int32_t  irisX   = (int32_t)sclera_x - (SCLERA_WIDTH - IRIS_WIDTH) / 2;
        int      lidX    = (e == 0) ? (SCREEN_WIDTH - 1) : 0;

        for (int screenX = 0; screenX < SCREEN_WIDTH;
             screenX++, scleraX++, irisX++, lidX += dlidX) {

            uint32_t p;

            uint8_t lid_lo = pgm_read_byte(lower + screenY * SCREEN_WIDTH + lidX);
            uint8_t lid_hi = pgm_read_byte(upper + screenY * SCREEN_WIDTH + lidX);

            if (lid_lo <= lT || lid_hi <= uT) {
                p = 0x0000;
            } else if (irisY < 0 || irisY >= IRIS_HEIGHT ||
                       irisX < 0 || irisX >= IRIS_WIDTH) {
                p = pgm_read_word(sclera + sclera_y * SCLERA_WIDTH + scleraX);
            } else {
                uint32_t pol = pgm_read_word(polar + irisY * IRIS_WIDTH + irisX);
                uint32_t d = (iScale * (pol & 0x7F)) / 128;
                if (d < IRIS_MAP_HEIGHT) {
                    uint32_t a = (IRIS_MAP_WIDTH * (pol >> 7)) / 512;
                    p = pgm_read_word(iris + d * IRIS_MAP_WIDTH + a);
                } else {
                    p = pgm_read_word(sclera + sclera_y * SCLERA_WIDTH + scleraX);
                }
            }
            // GC9A01 attend RGB565 big-endian
            line[screenX] = (uint16_t)(((p & 0xFF) << 8) | ((p >> 8) & 0xFF));
        }
    }

    int x0 = s_eye_xpos[e];
    int y0 = EYE_RENDER_OFFY;
    // Lance le transfert DMA full-frame puis attend la fin via le callback ISR.
    // Bloquant côté task, mais le CPU est libre pendant le DMA (xSemaphoreTake)
    // donc le scheduler peut faire tourner d'autres tasks.
    esp_err_t dret = esp_lcd_panel_draw_bitmap(panel, x0, y0,
                                               x0 + SCREEN_WIDTH,
                                               y0 + SCREEN_HEIGHT,
                                               fb);
    if (dret != ESP_OK) {
        ESP_LOGE(TAG, "draw_bitmap e=%d err=%s", (int)e, esp_err_to_name(dret));
        return;
    }
    eyes_wait_done(eye);
}

// Process motion + blinking + iris pour un œil
static void frame(uint16_t iScale, const eyes_anim_state_t *st)
{
    static uint8_t eye_index = 0;
    uint32_t t = micros();

    eye_index = (eye_index + 1) % NUM_EYES;

    // X/Y mouvement — autonome, sauf si state demande un look forcé
    int16_t eyeX, eyeY;
    static bool     in_motion = false;
    static int16_t  oldX = 512, oldY = 512, newX = 512, newY = 512;
    static uint32_t move_start_us = 0;
    static int32_t  move_dur_us = 0;

    if (st->look_forced) {
        // Décalage demandé par l'utilisateur (-512..512 → mapping 0-1023)
        eyeX = 512 + st->look_x;
        eyeY = 512 + st->look_y;
        if (eyeX < 0)    eyeX = 0;
        if (eyeX > 1023) eyeX = 1023;
        if (eyeY < 0)    eyeY = 0;
        if (eyeY > 1023) eyeY = 1023;
        in_motion = false;
        oldX = newX = eyeX;
        oldY = newY = eyeY;
    } else {
        int32_t dt = (int32_t)(t - move_start_us);
        if (in_motion) {
            if (dt >= move_dur_us) {
                in_motion = false;
                move_dur_us = (int32_t)rand_range(0, 3000000);  // 0-3 s d'arrêt
                move_start_us = t;
                eyeX = oldX = newX;
                eyeY = oldY = newY;
            } else {
                int16_t e_idx = s_ease[(255 * dt) / move_dur_us] + 1;
                eyeX = oldX + (((newX - oldX) * e_idx) / 256);
                eyeY = oldY + (((newY - oldY) * e_idx) / 256);
            }
        } else {
            eyeX = oldX;
            eyeY = oldY;
            if (dt > move_dur_us) {
                // Nouvelle cible aléatoire dans le cercle unité
                int16_t dx, dy;
                int32_t r2;
                do {
                    newX = (int16_t)rand_range(0, 1024);
                    newY = (int16_t)rand_range(0, 1024);
                    dx = (newX * 2) - 1023;
                    dy = (newY * 2) - 1023;
                    r2 = dx * dx + dy * dy;
                } while (r2 > 1023 * 1023);
                move_dur_us  = rand_range(72000, 144000);  // ~70-140 ms
                move_start_us = t;
                in_motion = true;
            }
        }
    }

    // AutoBlink
    if (st->forced_blink ||
        ((t - s_time_of_last_blink_us) >= s_time_to_next_blink_us)) {
        s_time_of_last_blink_us = t;
        uint32_t blink_dur = (uint32_t)rand_range(36000, 72000);
        for (int e = 0; e < NUM_EYES; e++) {
            if (s_blink[e].state == NOBLINK) {
                s_blink[e].state = ENBLINK;
                s_blink[e].start_us = t;
                s_blink[e].duration_us = blink_dur;
            }
        }
        s_time_to_next_blink_us = blink_dur * 3 + (uint32_t)rand_range(0, 4000000);
    }

    // Progression du clignement pour l'œil courant
    if (s_blink[eye_index].state) {
        if ((t - s_blink[eye_index].start_us) >= s_blink[eye_index].duration_us) {
            if (++s_blink[eye_index].state > DEBLINK) {
                s_blink[eye_index].state = NOBLINK;
            } else {
                s_blink[eye_index].duration_us *= 2;  // DEBLINK = 1/2 vitesse
                s_blink[eye_index].start_us = t;
            }
        }
    }

    // Scale eye X/Y (0-1023) → unités pixel utilisées par drawEye
    int16_t eyeXpx = map_i(eyeX, 0, 1023, 0, SCLERA_WIDTH  - SCREEN_WIDTH);
    int16_t eyeYpx = map_i(eyeY, 0, 1023, 0, SCLERA_HEIGHT - SCREEN_HEIGHT);

    // Légère convergence inter-yeux
    if (eye_index == 1) eyeXpx += 4;
    else                eyeXpx -= 4;
    if (eyeXpx > SCLERA_WIDTH - SCREEN_WIDTH) eyeXpx = SCLERA_WIDTH - SCREEN_WIDTH;
    if (eyeXpx < 0) eyeXpx = 0;

    // Tracking : paupière haute suit la pupille
    static uint8_t uThreshold = 128;
    uint8_t lThreshold, n;
    int16_t sampleX = SCLERA_WIDTH  / 2 - (eyeXpx / 2);
    int16_t sampleY = SCLERA_HEIGHT / 2 - (eyeYpx + IRIS_HEIGHT / 4);
    if (sampleY < 0) n = 0;
    else {
        int sx1 = sampleX;
        int sx2 = SCREEN_WIDTH - 1 - sampleX;
        if (sx1 < 0) sx1 = 0;
        if (sx1 >= SCREEN_WIDTH) sx1 = SCREEN_WIDTH - 1;
        if (sx2 < 0) sx2 = 0;
        if (sx2 >= SCREEN_WIDTH) sx2 = SCREEN_WIDTH - 1;
        if (sampleY >= SCREEN_HEIGHT) sampleY = SCREEN_HEIGHT - 1;
        uint8_t a = pgm_read_byte(upper + sampleY * SCREEN_WIDTH + sx1);
        uint8_t b = pgm_read_byte(upper + sampleY * SCREEN_WIDTH + sx2);
        n = (uint8_t)((a + b) / 2);
    }
    uThreshold = (uint8_t)(((int)uThreshold * 3 + n) / 4);
    lThreshold = (uint8_t)(254 - uThreshold);

    // Application du clignement
    if (s_blink[eye_index].state) {
        uint32_t s = (t - s_blink[eye_index].start_us);
        if (s >= s_blink[eye_index].duration_us) s = 255;
        else s = 255 * s / s_blink[eye_index].duration_us;
        s = (s_blink[eye_index].state == DEBLINK) ? 1 + s : 256 - s;
        n          = (uint8_t)((uThreshold * s + 254 * (257 - s)) / 256);
        lThreshold = (uint8_t)((lThreshold * s + 254 * (257 - s)) / 256);
    } else {
        n = uThreshold;
    }

    // Application des biais émotion
    int up_t = (int)n          + st->lid_top_bias;
    int lo_t = (int)lThreshold + st->lid_bot_bias;
    if (up_t < 0)   up_t = 0;
    if (up_t > 255) up_t = 255;
    if (lo_t < 0)   lo_t = 0;
    if (lo_t > 255) lo_t = 255;

    draw_eye(eye_index, iScale, eyeXpx, eyeYpx,
             (uint32_t)up_t, (uint32_t)lo_t, st->forced_closed);
}

// Récursion sur la dilatation iris (subdivise un mouvement d'iris en
// sous-mouvements aléatoires, simule réflexe + ajustements continus).
static void split(int16_t startVal, int16_t endVal,
                  uint32_t startTime, int32_t duration, int16_t range,
                  const eyes_anim_state_t *st)
{
    if (range >= 8) {
        range    /= 2;
        duration /= 2;
        int16_t midVal = (startVal + endVal - range) / 2
                         + (int16_t)rand_range(0, range);
        uint32_t midTime = startTime + duration;
        split(startVal, midVal, startTime, duration, range, st);
        split(midVal,   endVal, midTime,   duration, range, st);
    } else {
        int32_t dt;
        int16_t v;
        while ((dt = (int32_t)(micros() - startTime)) < duration) {
            v = startVal + (((endVal - startVal) * dt) / duration);
            if (v < IRIS_MIN)      v = IRIS_MIN;
            else if (v > IRIS_MAX) v = IRIS_MAX;
            // Biais émotion sur la pupille
            int bv = v + st->iris_scale_bias;
            if (bv < 0)    bv = 0;
            if (bv > 1023) bv = 1023;
            frame((uint16_t)bv, st);
        }
    }
}

void eyes_anim_step(const eyes_anim_state_t *st)
{
    uint16_t newIris = (uint16_t)rand_range(IRIS_MIN, IRIS_MAX);
    // 10 s pour aller au prochain target → comme l'original
    split(s_old_iris, newIris, micros(), 10000000L,
          IRIS_MAX - IRIS_MIN, st);
    s_old_iris = newIris;
}
