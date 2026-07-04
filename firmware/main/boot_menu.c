#include "boot_menu.h"

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "nvs.h"

#include "ble_prov.h"
#include "hal_audio.h"
#include "hal_leds.h"
#include "hal_touch.h"
#include "ui_face.h"

#define TAG "boot_menu"

#define KEY_LEFT   4
#define KEY_RIGHT  6
#define KEY_OK     11

#define INVITE_MS       4000
#define IDLE_EXIT_MS    30000
#define POLL_MS         30
#define SCENARIO_SD_ROOT "/sdcard/scenarios"
#define MAX_SCENARIOS   16

typedef enum {
    ITEM_PLAY = 0,   // vert  — regard centre
    ITEM_SCENARIO,   // ambre — regard gauche
    ITEM_PAIR,       // bleu  — regard droite
    ITEM_COUNT,
} menu_item_t;

static char s_slugs[MAX_SCENARIOS][64];
static int  s_slug_count;
static int  s_slug_sel;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void beeps(int n, uint16_t base_freq)
{
    uint16_t f[8], d[8];
    if (n > 8) n = 8;
    for (int i = 0; i < n; i++) { f[i] = base_freq; d[i] = 70; }
    hal_audio_play_sequence(f, d, n, 70);
}

static int scan_scenarios(void)
{
    s_slug_count = 0;
    DIR *root = opendir(SCENARIO_SD_ROOT);
    if (!root) return 0;
    struct dirent *e;
    while (s_slug_count < MAX_SCENARIOS && (e = readdir(root)) != NULL) {
        if (e->d_name[0] == '.') continue;
        char path[320];
        struct stat st;
        snprintf(path, sizeof(path), SCENARIO_SD_ROOT "/%s/scenario.json",
                 e->d_name);
        if (stat(path, &st) == 0) {
            strlcpy(s_slugs[s_slug_count++], e->d_name, sizeof(s_slugs[0]));
        }
    }
    closedir(root);
    return s_slug_count;
}

static void save_active_scenario(const char *slug)
{
    nvs_handle_t nvs;
    if (nvs_open("cloud", NVS_READWRITE, &nvs) != ESP_OK) return;
    if (nvs_set_str(nvs, "active_scenario", slug) == ESP_OK) nvs_commit(nvs);
    nvs_close(nvs);
    ESP_LOGI(TAG, "scénario actif : %s", slug);
}

static int load_active_index(void)
{
    char slug[64] = {0};
    size_t len = sizeof(slug);
    nvs_handle_t nvs;
    if (nvs_open("cloud", NVS_READONLY, &nvs) == ESP_OK) {
        nvs_get_str(nvs, "active_scenario", slug, &len);
        nvs_close(nvs);
    }
    for (int i = 0; i < s_slug_count; i++) {
        if (strcmp(s_slugs[i], slug) == 0) return i;
    }
    return 0;
}

static void announce_item(menu_item_t it)
{
    switch (it) {
    case ITEM_PLAY:
        ui_face_look(UI_FACE_LOOK_CENTER);
        hal_leds_fill_hex("#1FA84A", 60);
        beeps(1, 880);
        break;
    case ITEM_SCENARIO:
        ui_face_look(UI_FACE_LOOK_LEFT);
        hal_leds_fill_hex("#E8A33D", 60);
        beeps(2, 988);
        break;
    case ITEM_PAIR:
        ui_face_look(UI_FACE_LOOK_RIGHT);
        hal_leds_fill_hex("#2A6BE8", 60);
        beeps(3, 1175);
        break;
    default:
        break;
    }
    hal_leds_show();
}

// Fronts montants depuis le dernier poll. Retourne un bitmask des touches.
static uint16_t rising_edges(hal_touch_data_t *prev)
{
    hal_touch_data_t cur;
    if (hal_touch_read(&cur) != ESP_OK) return 0;
    uint16_t rose = cur.touched & (uint16_t)~prev->touched;
    *prev = cur;
    return rose;
}

// ── Menu ──────────────────────────────────────────────────────────────────────

static void menu_loop(void (*on_ble_wifi_ok)(void))
{
    scan_scenarios();
    s_slug_sel = load_active_index();

    menu_item_t it = ITEM_PLAY;
    ui_face_set_emotion(UI_FACE_SURPRISED);
    announce_item(it);

    hal_touch_data_t prev = {0};
    hal_touch_read(&prev);  // état initial (le doigt d'entrée peut être posé)
    uint32_t idle_ms = 0;

    while (idle_ms < IDLE_EXIT_MS) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        uint16_t rose = rising_edges(&prev);
        if (!rose) {
            idle_ms += POLL_MS;
            continue;
        }
        idle_ms = 0;

        if (rose & (1u << KEY_LEFT)) {
            it = (it + ITEM_COUNT - 1) % ITEM_COUNT;
            announce_item(it);
        } else if (rose & (1u << KEY_RIGHT)) {
            it = (menu_item_t)((it + 1) % ITEM_COUNT);
            announce_item(it);
        } else if (rose & (1u << KEY_OK)) {
            switch (it) {
            case ITEM_PLAY:
                goto out;
            case ITEM_SCENARIO:
                if (s_slug_count == 0) {
                    beeps(1, 220);  // pas de SD / pas de scénario
                    break;
                }
                s_slug_sel = (s_slug_sel + 1) % s_slug_count;
                save_active_scenario(s_slugs[s_slug_sel]);
                // Annonce : position dans la liste (1 bip = premier, etc.)
                beeps(s_slug_sel + 1, 1319);
                break;
            case ITEM_PAIR:
                if (ble_prov_start(300, on_ble_wifi_ok) == ESP_OK) {
                    hal_leds_fill_hex("#2A6BE8", 90);
                    hal_leds_show();
                    beeps(2, 1568);
                } else {
                    beeps(1, 220);
                }
                break;
            default:
                break;
            }
        }
    }

out:
    hal_leds_clear();
    hal_leds_show();
    ui_face_set_emotion(UI_FACE_HAPPY);
    ui_face_look(UI_FACE_LOOK_CENTER);
    beeps(1, 1568);
    ESP_LOGI(TAG, "sortie du menu — scénario actif : %s",
             s_slug_count ? s_slugs[s_slug_sel] : "(embarqué)");
}

esp_err_t boot_menu_maybe_run(void (*on_ble_wifi_ok)(void))
{
    if (hal_touch_init() != ESP_OK) {
        ESP_LOGW(TAG, "keypad absent — pas de menu");
        return ESP_OK;
    }

    // Invite : double bip + LEDs douces, 4 s pour poser un doigt. Le MPR121
    // s'est calibré à l'init (doigt posé PENDANT le boot = invisible — c'est
    // précisément pourquoi le menu attend le doigt APRÈS).
    ESP_LOGI(TAG, "invite menu (%d ms) — ◀=4 ▶=6 ✓=#", INVITE_MS);
    beeps(2, 1047);
    hal_leds_fill_hex("#404040", 30);
    hal_leds_show();

    hal_touch_data_t prev = {0};
    hal_touch_read(&prev);
    bool enter = false;
    for (uint32_t t = 0; t < INVITE_MS && !enter; t += POLL_MS) {
        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
        if (rising_edges(&prev)) enter = true;
    }

    if (!enter) {
        hal_leds_clear();
        hal_leds_show();
        return ESP_OK;
    }

    ESP_LOGI(TAG, "menu ouvert");
    menu_loop(on_ble_wifi_ok);
    return ESP_OK;
}
