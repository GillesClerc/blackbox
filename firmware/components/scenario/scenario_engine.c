#include "scenario_engine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#define TAG "scenario"
#define EVENT_QUEUE_LEN     16
#define MAX_ACTION_HANDLERS 20
#define ENGINE_STACK_SIZE   8192
#define ENGINE_PRIORITY     5

// --- Internals ---

typedef struct {
    char                 name[32];
    scenario_action_cb_t cb;
} action_handler_t;

static cJSON          *s_root         = NULL;
static cJSON          *s_steps        = NULL;
static cJSON          *s_current      = NULL;
static QueueHandle_t   s_queue        = NULL;
static action_handler_t s_handlers[MAX_ACTION_HANDLERS];
static int             s_handler_count = 0;
static TickType_t      s_step_entered  = 0;

// --- Helpers ---

static cJSON *step_by_id(const char *id) {
    cJSON *step;
    cJSON_ArrayForEach(step, s_steps) {
        cJSON *sid = cJSON_GetObjectItem(step, "id");
        if (sid && strcmp(sid->valuestring, id) == 0) return step;
    }
    return NULL;
}

static void run_actions(const cJSON *actions) {
    if (!cJSON_IsArray(actions)) return;
    const cJSON *action;
    cJSON_ArrayForEach(action, actions) {
        // Chaque action est un objet à une seule clé : {"audio": {...}}
        const cJSON *param = action->child;
        if (!param) continue;
        bool handled = false;
        for (int i = 0; i < s_handler_count; i++) {
            if (strcmp(s_handlers[i].name, param->string) == 0) {
                s_handlers[i].cb(param->string, param);
                handled = true;
                break;
            }
        }
        if (!handled) {
            ESP_LOGW(TAG, "action '%s' sans handler", param->string);
        }
    }
}

// Vérifie si l'event correspond au step courant.
// Retourne true si match, false sinon.
static bool event_matches(const cJSON *step, const scenario_event_t *evt) {
    const cJSON *on = cJSON_GetObjectItem(step, "on");
    if (!on || !on->valuestring) return false;

    const cJSON *expect = cJSON_GetObjectItem(step, "expect");

    switch (evt->type) {
        case EVT_RFID_READ:
            if (strcmp(on->valuestring, "rfid_read") != 0) return false;
            if (expect) {
                const cJSON *uid = cJSON_GetObjectItem(expect, "uid");
                if (uid && strcmp(uid->valuestring, evt->str) != 0) return false;
            }
            return true;

        case EVT_KEYPAD_CODE:
            if (strcmp(on->valuestring, "keypad_code") != 0) return false;
            if (expect) {
                const cJSON *code = cJSON_GetObjectItem(expect, "code");
                if (code && strcmp(code->valuestring, evt->str) != 0) return false;
            }
            return true;

        case EVT_ROTARY_VALUE:
            if (strcmp(on->valuestring, "rotary_value") != 0) return false;
            if (expect) {
                const cJSON *val = cJSON_GetObjectItem(expect, "value");
                const cJSON *tol = cJSON_GetObjectItem(expect, "tolerance");
                if (val) {
                    int tolerance = tol ? tol->valueint : 0;
                    if (abs(evt->int_val - val->valueint) > tolerance) return false;
                }
            }
            return true;

        case EVT_HALL_DETECTED:
            if (strcmp(on->valuestring, "hall_detected") != 0) return false;
            if (expect) {
                const cJSON *v = cJSON_GetObjectItem(expect, "value");
                if (v && cJSON_IsBool(v) && (cJSON_IsTrue(v) != evt->bool_val)) return false;
            }
            return true;

        case EVT_BREATH_DETECTED:
            return strcmp(on->valuestring, "breath_detected") == 0;

        case EVT_ACCEL_TILT:
            if (strcmp(on->valuestring, "accel_tilt") != 0) return false;
            if (expect) {
                const cJSON *val = cJSON_GetObjectItem(expect, "angle");
                const cJSON *tol = cJSON_GetObjectItem(expect, "tolerance");
                if (val) {
                    int tolerance = tol ? tol->valueint : 5;
                    if (abs(evt->int_val - val->valueint) > tolerance) return false;
                }
            }
            return true;

        default:
            return false;
    }
}

static void enter_step(const char *id) {
    cJSON *step = step_by_id(id);
    if (!step) {
        ESP_LOGE(TAG, "step '%s' introuvable", id);
        return;
    }
    s_current      = step;
    s_step_entered = xTaskGetTickCount();

    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(step, "type"));
    ESP_LOGI(TAG, "→ [%s] (%s)", id, type ? type : "?");
}

// --- Tâche principale ---

static void engine_task(void *arg) {
    // Démarrer au premier step
    cJSON *first = cJSON_GetArrayItem(s_steps, 0);
    if (!first) {
        ESP_LOGE(TAG, "scénario vide");
        vTaskDelete(NULL);
        return;
    }
    enter_step(cJSON_GetStringValue(cJSON_GetObjectItem(first, "id")));

    while (1) {
        if (!s_current) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "type"));
        if (!type) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        // --- narrative : exécute do[] et avance automatiquement ---
        if (strcmp(type, "narrative") == 0) {
            run_actions(cJSON_GetObjectItem(s_current, "do"));
            vTaskDelay(pdMS_TO_TICKS(200));
            const char *next = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "next"));
            if (next) enter_step(next);
            else s_current = NULL;
            continue;
        }

        // --- end : exécute do[] et arrête ---
        if (strcmp(type, "end") == 0) {
            run_actions(cJSON_GetObjectItem(s_current, "do"));
            ESP_LOGI(TAG, "=== SCÉNARIO TERMINÉ ===");
            s_current = NULL;
            continue;
        }

        // --- trigger / input / combo : attente d'événement ---

        // Vérifier timeout (0 = pas de timeout)
        const cJSON *timeout_obj = cJSON_GetObjectItem(s_current, "timeout_sec");
        int timeout_sec = timeout_obj ? timeout_obj->valueint : 0;
        if (timeout_sec > 0) {
            int elapsed_sec = (xTaskGetTickCount() - s_step_entered) / configTICK_RATE_HZ;
            if (elapsed_sec >= timeout_sec) {
                ESP_LOGW(TAG, "timeout [%s] (%ds)",
                         cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "id")),
                         timeout_sec);
                run_actions(cJSON_GetObjectItem(s_current, "do_timeout"));
                s_step_entered = xTaskGetTickCount(); // reset pour ne pas spammer
            }
        }

        // Attendre un événement (100ms max pour pouvoir checker le timeout)
        scenario_event_t evt;
        if (xQueueReceive(s_queue, &evt, pdMS_TO_TICKS(100)) != pdTRUE) continue;

        if (!event_matches(s_current, &evt)) {
            ESP_LOGD(TAG, "event ignoré (no match)");
            continue;
        }

        // Match : exécuter les actions de succès
        // "trigger" utilise "do", "input"/"combo" utilisent "do_success"
        if (strcmp(type, "trigger") == 0) {
            run_actions(cJSON_GetObjectItem(s_current, "do"));
        } else {
            run_actions(cJSON_GetObjectItem(s_current, "do_success"));
        }

        // Avancer au step suivant
        const char *next = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "next"));
        if (next) {
            enter_step(next);
        } else {
            s_current = NULL;
        }
    }
}

// --- API publique ---

esp_err_t scenario_engine_init(const char *json) {
    s_root = cJSON_Parse(json);
    if (!s_root) {
        ESP_LOGE(TAG, "JSON invalide près de: %.40s", cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "?");
        return ESP_ERR_INVALID_ARG;
    }

    s_steps = cJSON_GetObjectItem(s_root, "steps");
    if (!cJSON_IsArray(s_steps)) {
        ESP_LOGE(TAG, "'steps' manquant ou invalide");
        cJSON_Delete(s_root);
        s_root = NULL;
        return ESP_ERR_INVALID_ARG;
    }

    s_queue = xQueueCreate(EVENT_QUEUE_LEN, sizeof(scenario_event_t));
    if (!s_queue) return ESP_ERR_NO_MEM;

    const cJSON *meta  = cJSON_GetObjectItem(s_root, "meta");
    const char  *title = cJSON_GetStringValue(cJSON_GetObjectItem(meta, "title"));
    ESP_LOGI(TAG, "chargé: \"%s\" — %d steps", title ? title : "?", cJSON_GetArraySize(s_steps));

    return ESP_OK;
}

void scenario_engine_register_action(const char *name, scenario_action_cb_t cb) {
    if (s_handler_count >= MAX_ACTION_HANDLERS) {
        ESP_LOGW(TAG, "MAX_ACTION_HANDLERS atteint");
        return;
    }
    strncpy(s_handlers[s_handler_count].name, name, sizeof(s_handlers[0].name) - 1);
    s_handlers[s_handler_count].cb = cb;
    s_handler_count++;
    ESP_LOGD(TAG, "handler '%s' enregistré", name);
}

esp_err_t scenario_engine_post_event(const scenario_event_t *evt) {
    if (!s_queue) return ESP_ERR_INVALID_STATE;
    if (xQueueSend(s_queue, evt, pdMS_TO_TICKS(10)) != pdTRUE) {
        ESP_LOGW(TAG, "event queue pleine, event perdu");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t scenario_engine_start(void) {
    if (!s_steps || !s_queue) return ESP_ERR_INVALID_STATE;
    BaseType_t r = xTaskCreate(engine_task, "scenario_engine",
                               ENGINE_STACK_SIZE, NULL, ENGINE_PRIORITY, NULL);
    if (r != pdPASS) {
        ESP_LOGE(TAG, "échec création tâche");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "moteur démarré");
    return ESP_OK;
}

const char *scenario_engine_current_step(void) {
    if (!s_current) return "none";
    const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "id"));
    return id ? id : "?";
}
