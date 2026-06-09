#include "scenario_engine.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_task_wdt.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

#define TAG "scenario"
#define EVENT_QUEUE_LEN     16
#define MAX_ACTION_HANDLERS 20
#define ENGINE_STACK_SIZE   8192
#define ENGINE_PRIORITY     5
#define MAX_VARS            32
#define VAR_NAME_LEN        32
#define VAR_VAL_LEN         64

// --- Types internes ---

typedef struct {
    char                 name[32];
    scenario_action_cb_t cb;
} action_handler_t;

typedef struct {
    char name[VAR_NAME_LEN];
    char value[VAR_VAL_LEN];
} var_t;

// --- État global ---

static cJSON           *s_root          = NULL;
static cJSON           *s_steps         = NULL;
static cJSON           *s_current       = NULL;
static QueueHandle_t    s_queue         = NULL;
static action_handler_t s_handlers[MAX_ACTION_HANDLERS];
static int              s_handler_count = 0;
static TickType_t       s_step_entered  = 0;
static var_t            s_vars[MAX_VARS];
static int              s_var_count     = 0;
static int              s_hint_idx      = 0;  // prochain hint à livrer dans le step courant

// --- Variables runtime ---

void scenario_var_set(const char *name, const char *value) {
    for (int i = 0; i < s_var_count; i++) {
        if (strcmp(s_vars[i].name, name) == 0) {
            strncpy(s_vars[i].value, value, VAR_VAL_LEN - 1);
            s_vars[i].value[VAR_VAL_LEN - 1] = '\0';
            ESP_LOGD(TAG, "var %s = '%s'", name, value);
            return;
        }
    }
    if (s_var_count >= MAX_VARS) {
        ESP_LOGW(TAG, "MAX_VARS atteint, var '%s' ignorée", name);
        return;
    }
    strncpy(s_vars[s_var_count].name,  name,  VAR_NAME_LEN - 1);
    strncpy(s_vars[s_var_count].value, value, VAR_VAL_LEN  - 1);
    s_vars[s_var_count].name[VAR_NAME_LEN - 1]  = '\0';
    s_vars[s_var_count].value[VAR_VAL_LEN - 1] = '\0';
    s_var_count++;
    ESP_LOGD(TAG, "var %s = '%s' (new)", name, value);
}

void scenario_var_set_int(const char *name, int val) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", val);
    scenario_var_set(name, buf);
}

const char *scenario_var_get(const char *name) {
    for (int i = 0; i < s_var_count; i++) {
        if (strcmp(s_vars[i].name, name) == 0) return s_vars[i].value;
    }
    return "";
}

int scenario_var_get_int(const char *name, int default_val) {
    const char *v = scenario_var_get(name);
    if (!v || *v == '\0') return default_val;
    return atoi(v);
}

// --- Helpers ---

static cJSON *step_by_id(const char *id) {
    cJSON *step;
    cJSON_ArrayForEach(step, s_steps) {
        cJSON *sid = cJSON_GetObjectItem(step, "id");
        if (sid && strcmp(sid->valuestring, id) == 0) return step;
    }
    return NULL;
}

// Évalue une condition {"var":"x","op":"gte","value":3}
static bool eval_condition(const cJSON *cond) {
    const char  *var_name = cJSON_GetStringValue(cJSON_GetObjectItem(cond, "var"));
    const char  *op       = cJSON_GetStringValue(cJSON_GetObjectItem(cond, "op"));
    const cJSON *val_obj  = cJSON_GetObjectItem(cond, "value");
    if (!var_name || !op || !val_obj) return false;

    const char *var_str = scenario_var_get(var_name);
    int         var_int = atoi(var_str);

    if (cJSON_IsNumber(val_obj)) {
        int cmp = val_obj->valueint;
        if (strcmp(op, "eq")  == 0) return var_int == cmp;
        if (strcmp(op, "neq") == 0) return var_int != cmp;
        if (strcmp(op, "gt")  == 0) return var_int >  cmp;
        if (strcmp(op, "gte") == 0) return var_int >= cmp;
        if (strcmp(op, "lt")  == 0) return var_int <  cmp;
        if (strcmp(op, "lte") == 0) return var_int <= cmp;
    } else if (cJSON_IsString(val_obj)) {
        if (strcmp(op, "eq")  == 0) return strcmp(var_str, val_obj->valuestring) == 0;
        if (strcmp(op, "neq") == 0) return strcmp(var_str, val_obj->valuestring) != 0;
    }
    return false;
}

// Action built-in : set_var
// Formats : {"set_var":{"name":"score","value":42}} ou shorthand {"set_var":{"score":42}}
static void builtin_set_var(const cJSON *params) {
    const cJSON *name_item = cJSON_GetObjectItem(params, "name");
    if (name_item && cJSON_IsString(name_item)) {
        const cJSON *val = cJSON_GetObjectItem(params, "value");
        if      (cJSON_IsString(val))  scenario_var_set(name_item->valuestring, val->valuestring);
        else if (cJSON_IsNumber(val))  scenario_var_set_int(name_item->valuestring, (int)val->valuedouble);
    } else {
        const cJSON *child = params->child;
        while (child) {
            if      (cJSON_IsString(child))  scenario_var_set(child->string, child->valuestring);
            else if (cJSON_IsNumber(child))  scenario_var_set_int(child->string, (int)child->valuedouble);
            child = child->next;
        }
    }
}

// Action built-in : incr_var
// Format : {"incr_var":{"attempts":1}} — delta positif ou négatif
static void builtin_incr_var(const cJSON *params) {
    const cJSON *child = params->child;
    while (child) {
        int delta   = cJSON_IsNumber(child) ? (int)child->valuedouble : 1;
        int current = scenario_var_get_int(child->string, 0);
        scenario_var_set_int(child->string, current + delta);
        child = child->next;
    }
}

static void run_actions(const cJSON *actions) {
    if (!cJSON_IsArray(actions)) return;
    const cJSON *action;
    cJSON_ArrayForEach(action, actions) {
        const cJSON *param = action->child;
        if (!param) continue;

        // Built-ins (pas de handler externe nécessaire)
        if (strcmp(param->string, "set_var") == 0)  { builtin_set_var(param);  continue; }
        if (strcmp(param->string, "incr_var") == 0) { builtin_incr_var(param); continue; }

        // Handlers enregistrés
        bool handled = false;
        for (int i = 0; i < s_handler_count; i++) {
            if (strcmp(s_handlers[i].name, param->string) == 0) {
                s_handlers[i].cb(param->string, param);
                handled = true;
                break;
            }
        }
        if (!handled) ESP_LOGW(TAG, "action '%s' sans handler", param->string);
    }
}

// true si le type d'event correspond au "on:" du step
static bool event_type_matches(const cJSON *step, const scenario_event_t *evt) {
    const cJSON *on = cJSON_GetObjectItem(step, "on");
    if (!on || !on->valuestring) return false;
    switch (evt->type) {
        case EVT_RFID_READ:       return strcmp(on->valuestring, "rfid_read")       == 0;
        case EVT_KEYPAD_CODE:     return strcmp(on->valuestring, "keypad_code")     == 0;
        case EVT_TOUCH:           return strcmp(on->valuestring, "touch")           == 0;
        case EVT_ROTARY_VALUE:    return strcmp(on->valuestring, "rotary_value")    == 0;
        case EVT_HALL_DETECTED:   return strcmp(on->valuestring, "hall_detected")   == 0;
        case EVT_BREATH_DETECTED: return strcmp(on->valuestring, "breath_detected") == 0;
        case EVT_ACCEL_TILT:      return strcmp(on->valuestring, "accel_tilt")      == 0;
        default:                  return false;
    }
}

// true si la valeur de l'event correspond à "expect:"
static bool event_value_matches(const cJSON *step, const scenario_event_t *evt) {
    const cJSON *expect = cJSON_GetObjectItem(step, "expect");
    if (!expect) return true;  // pas de contrainte = toujours ok

    switch (evt->type) {
        case EVT_RFID_READ: {
            const cJSON *uid = cJSON_GetObjectItem(expect, "uid");
            return !uid || strcmp(uid->valuestring, evt->str) == 0;
        }
        case EVT_KEYPAD_CODE: {
            const cJSON *code = cJSON_GetObjectItem(expect, "code");
            return !code || strcmp(code->valuestring, evt->str) == 0;
        }
        case EVT_TOUCH: {
            const cJSON *ch = cJSON_GetObjectItem(expect, "channel");
            return !ch || ch->valueint == evt->int_val;
        }
        case EVT_ROTARY_VALUE: {
            const cJSON *val = cJSON_GetObjectItem(expect, "value");
            const cJSON *tol = cJSON_GetObjectItem(expect, "tolerance");
            if (!val) return true;
            return abs(evt->int_val - val->valueint) <= (tol ? tol->valueint : 0);
        }
        case EVT_HALL_DETECTED: {
            const cJSON *v = cJSON_GetObjectItem(expect, "value");
            return !v || !cJSON_IsBool(v) || (cJSON_IsTrue(v) == evt->bool_val);
        }
        case EVT_ACCEL_TILT: {
            const cJSON *val = cJSON_GetObjectItem(expect, "angle");
            const cJSON *tol = cJSON_GetObjectItem(expect, "tolerance");
            if (!val) return true;
            return abs(evt->int_val - val->valueint) <= (tol ? tol->valueint : 5);
        }
        case EVT_BREATH_DETECTED:
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
    s_hint_idx     = 0;

    // M5 : xQueueReset supprimé — les events en attente restent valides (un RFID
    // scanné pendant la transition doit être traité dans le nouveau step).
    // L'engine filtre déjà à la réception via event_type_matches / event_value_matches.

    const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(step, "type"));
    ESP_LOGI(TAG, "→ [%s] (%s)", id, type ? type : "?");
}

// --- Tâche principale ---

static void engine_task(void *arg) {
    cJSON *first = cJSON_GetArrayItem(s_steps, 0);
    if (!first) {
        ESP_LOGE(TAG, "scénario vide");
        vTaskDelete(NULL);
        return;
    }
    enter_step(cJSON_GetStringValue(cJSON_GetObjectItem(first, "id")));

    // TWDT : signale un blocage du moteur. Pas de panic par défaut (warning log),
    // donc une action longue (audio bloquant >5s) ne crashe pas, elle se voit juste.
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret != ESP_OK) ESP_LOGW(TAG, "esp_task_wdt_add: %s", esp_err_to_name(wdt_ret));

    while (1) {
        if (wdt_ret == ESP_OK) esp_task_wdt_reset();
        if (!s_current) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        const char *type = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "type"));
        if (!type) { vTaskDelay(pdMS_TO_TICKS(100)); continue; }

        // --- narrative : do[] immédiat, avance automatiquement ---
        if (strcmp(type, "narrative") == 0) {
            run_actions(cJSON_GetObjectItem(s_current, "do"));
            vTaskDelay(pdMS_TO_TICKS(500));
            const char *next = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "next"));
            if (next) enter_step(next);
            else s_current = NULL;
            continue;
        }

        // --- branch : évalue les conditions et saute ---
        if (strcmp(type, "branch") == 0) {
            const char  *next       = NULL;
            const cJSON *conditions = cJSON_GetObjectItem(s_current, "conditions");
            if (cJSON_IsArray(conditions)) {
                const cJSON *cond;
                cJSON_ArrayForEach(cond, conditions) {
                    if (eval_condition(cond)) {
                        next = cJSON_GetStringValue(cJSON_GetObjectItem(cond, "next"));
                        break;
                    }
                }
            }
            if (!next) next = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "default"));
            if (next) enter_step(next);
            else {
                ESP_LOGW(TAG, "branch [%s] sans destination",
                         cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "id")));
                s_current = NULL;
            }
            continue;
        }

        // --- end ---
        if (strcmp(type, "end") == 0) {
            run_actions(cJSON_GetObjectItem(s_current, "do"));
            ESP_LOGI(TAG, "=== SCÉNARIO TERMINÉ ===");
            s_current = NULL;
            continue;
        }

        // --- trigger / input : attente événement ---

        // Timeout
        const cJSON *timeout_obj = cJSON_GetObjectItem(s_current, "timeout_sec");
        int timeout_sec = timeout_obj ? timeout_obj->valueint : 0;
        if (timeout_sec > 0) {
            // m7 : la soustraction (TickType_t - TickType_t) est overflow-safe en unsigned
            // (arithmétique modulo 2^32) — valide même après débordement du compteur de ticks.
            int elapsed = (int)((xTaskGetTickCount() - s_step_entered) / configTICK_RATE_HZ);
            if (elapsed >= timeout_sec) {
                const char *id = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "id"));
                ESP_LOGW(TAG, "timeout [%s] (%ds)", id ? id : "?", timeout_sec);
                run_actions(cJSON_GetObjectItem(s_current, "do_timeout"));
                const char *next_to = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "next_timeout"));
                if (next_to) enter_step(next_to);
                else s_step_entered = xTaskGetTickCount();  // reset pour éviter le spam
                continue;
            }
        }

        // Hints temporisés (ordonnés par delay_sec croissant)
        const cJSON *hints = cJSON_GetObjectItem(s_current, "hints");
        if (cJSON_IsArray(hints)) {
            int hint_count = cJSON_GetArraySize(hints);
            while (s_hint_idx < hint_count) {
                const cJSON *hint      = cJSON_GetArrayItem(hints, s_hint_idx);
                const cJSON *delay_obj = cJSON_GetObjectItem(hint, "delay_sec");
                int delay_sec = delay_obj ? delay_obj->valueint : 0;
                int elapsed   = (int)((xTaskGetTickCount() - s_step_entered) / configTICK_RATE_HZ);
                if (elapsed >= delay_sec) {
                    ESP_LOGI(TAG, "hint %d/%d [%s]", s_hint_idx + 1, hint_count,
                             cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "id")));
                    run_actions(cJSON_GetObjectItem(hint, "do"));
                    s_hint_idx++;
                } else {
                    break;  // hints triés → inutile de continuer
                }
            }
        }

        // Attente événement (100ms max pour relire les timers)
        scenario_event_t evt;
        if (xQueueReceive(s_queue, &evt, pdMS_TO_TICKS(100)) != pdTRUE) continue;

        if (!event_type_matches(s_current, &evt)) {
            ESP_LOGD(TAG, "event ignoré (type mismatch)");
            continue;
        }

        if (!event_value_matches(s_current, &evt)) {
            // Mauvaise réponse
            ESP_LOGI(TAG, "mauvaise réponse sur [%s]",
                     cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "id")));
            run_actions(cJSON_GetObjectItem(s_current, "do_fail"));
            continue;  // rester dans le même step
        }

        // Bonne réponse
        if (strcmp(type, "trigger") == 0) {
            run_actions(cJSON_GetObjectItem(s_current, "do"));
        } else {
            run_actions(cJSON_GetObjectItem(s_current, "do_success"));
        }

        const char *next = cJSON_GetStringValue(cJSON_GetObjectItem(s_current, "next"));
        if (next) enter_step(next);
        else s_current = NULL;
    }
}

// --- API publique ---

esp_err_t scenario_engine_init(const char *json) {
    // Réinit possible (changement de scénario) : libérer l'état précédent.
    if (s_root) {
        cJSON_Delete(s_root);
        s_root  = NULL;
        s_steps = NULL;
        s_current = NULL;
    }
    if (s_queue) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }

    s_root = cJSON_Parse(json);
    if (!s_root) {
        const char *err = cJSON_GetErrorPtr();
        ESP_LOGE(TAG, "JSON invalide près de: %.40s", err ? err : "?");
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

    s_var_count     = 0;
    s_handler_count = 0;
    s_hint_idx      = 0;

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
        ESP_LOGW(TAG, "queue pleine, event perdu");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t scenario_engine_start(void) {
    if (!s_steps || !s_queue) return ESP_ERR_INVALID_STATE;
    // Core 0 : laisse eye_task (core 1) tranquille — voir flash_task dans main.c.
    BaseType_t r = xTaskCreatePinnedToCore(engine_task, "scenario_engine",
                                           ENGINE_STACK_SIZE, NULL, ENGINE_PRIORITY,
                                           NULL, 0);
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
