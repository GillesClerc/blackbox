#include "scenario_validate.h"
#include "esp_log.h"
#include <stdbool.h>
#include <string.h>

#define TAG "scenario_val"

static const char *const STEP_TYPES[] = {
    "narrative", "trigger", "input", "branch", "end", NULL,
};
static const char *const EVENT_TYPES[] = {
    "rfid_read", "keypad_code", "touch", "rotary_value",
    "hall_detected", "breath_detected", "accel_tilt", NULL,
};
static const char *const BRANCH_OPS[] = {
    "eq", "neq", "gt", "gte", "lt", "lte", NULL,
};

static bool in_set(const char *s, const char *const *set)
{
    for (int i = 0; set[i]; i++) {
        if (strcmp(s, set[i]) == 0) return true;
    }
    return false;
}

// Chaîne non vide, ou NULL.
static const char *str_of(const cJSON *obj, const char *key)
{
    const char *s = cJSON_GetStringValue(cJSON_GetObjectItem(obj, key));
    return (s && s[0]) ? s : NULL;
}

static bool step_exists(const cJSON *steps, const char *id)
{
    const cJSON *step;
    cJSON_ArrayForEach(step, steps) {
        const char *sid = str_of(step, "id");
        if (sid && strcmp(sid, id) == 0) return true;
    }
    return false;
}

// Liste d'actions optionnelle : absente, ou tableau d'objets.
static bool actions_ok(const cJSON *owner, const char *key, const char *sid)
{
    const cJSON *list = cJSON_GetObjectItem(owner, key);
    if (!list) return true;
    if (!cJSON_IsArray(list)) {
        ESP_LOGE(TAG, "step '%s' : '%s' doit être un tableau", sid, key);
        return false;
    }
    const cJSON *a;
    cJSON_ArrayForEach(a, list) {
        if (!cJSON_IsObject(a)) {
            ESP_LOGE(TAG, "step '%s' : action de '%s' non objet", sid, key);
            return false;
        }
    }
    return true;
}

// Référence optionnelle vers un step : absente, ou chaîne d'un id existant.
static bool ref_ok(const cJSON *steps, const cJSON *owner, const char *key,
                   const char *sid)
{
    const cJSON *ref = cJSON_GetObjectItem(owner, key);
    if (!ref) return true;
    const char *id = cJSON_GetStringValue(ref);
    if (!id || !id[0]) {
        ESP_LOGE(TAG, "step '%s' : '%s' doit être une chaîne", sid, key);
        return false;
    }
    if (strcmp(id, "end") != 0 && !step_exists(steps, id)) {
        ESP_LOGE(TAG, "step '%s' : '%s' → step inexistant '%s'", sid, key, id);
        return false;
    }
    return true;
}

static bool wait_step_ok(const cJSON *step, const char *sid, const char *type)
{
    const char *on = str_of(step, "on");
    if (!on || !in_set(on, EVENT_TYPES)) {
        ESP_LOGE(TAG, "step '%s' : 'on' manquant ou inconnu", sid);
        return false;
    }

    const cJSON *expect = cJSON_GetObjectItem(step, "expect");
    if (expect) {
        if (!cJSON_IsObject(expect)) {
            ESP_LOGE(TAG, "step '%s' : 'expect' doit être un objet", sid);
            return false;
        }
        static const char *const STR_KEYS[] = {"uid", "code", NULL};
        for (int i = 0; STR_KEYS[i]; i++) {
            const cJSON *v = cJSON_GetObjectItem(expect, STR_KEYS[i]);
            if (v && !cJSON_IsString(v)) {
                ESP_LOGE(TAG, "step '%s' : expect.%s doit être une chaîne "
                         "(guillemets en YAML)", sid, STR_KEYS[i]);
                return false;
            }
        }
    }

    const cJSON *to = cJSON_GetObjectItem(step, "timeout_sec");
    if (to && !cJSON_IsNumber(to)) {
        ESP_LOGE(TAG, "step '%s' : timeout_sec doit être un nombre", sid);
        return false;
    }

    const cJSON *hints = cJSON_GetObjectItem(step, "hints");
    if (hints) {
        if (!cJSON_IsArray(hints)) {
            ESP_LOGE(TAG, "step '%s' : 'hints' doit être un tableau", sid);
            return false;
        }
        const cJSON *h;
        cJSON_ArrayForEach(h, hints) {
            const cJSON *d = cJSON_GetObjectItem(h, "delay_sec");
            if (!cJSON_IsObject(h) || (d && !cJSON_IsNumber(d)) ||
                !actions_ok(h, "do", sid)) {
                ESP_LOGE(TAG, "step '%s' : hint invalide", sid);
                return false;
            }
        }
    }

    if (strcmp(type, "trigger") == 0) return actions_ok(step, "do", sid);
    return actions_ok(step, "do_success", sid) &&
           actions_ok(step, "do_fail", sid) &&
           actions_ok(step, "do_timeout", sid);
}

static bool branch_step_ok(const cJSON *steps, const cJSON *step, const char *sid)
{
    const cJSON *conds = cJSON_GetObjectItem(step, "conditions");
    if (!cJSON_IsArray(conds) || cJSON_GetArraySize(conds) == 0) {
        ESP_LOGE(TAG, "step '%s' : 'conditions' manquante ou vide", sid);
        return false;
    }
    const cJSON *c;
    cJSON_ArrayForEach(c, conds) {
        const char *op = str_of(c, "op");
        const cJSON *val = cJSON_GetObjectItem(c, "value");
        if (!cJSON_IsObject(c) || !str_of(c, "var") || !op ||
            !in_set(op, BRANCH_OPS) ||
            !(cJSON_IsNumber(val) || cJSON_IsString(val)) ||
            !cJSON_GetObjectItem(c, "next") ||
            !ref_ok(steps, c, "next", sid)) {
            ESP_LOGE(TAG, "step '%s' : condition invalide", sid);
            return false;
        }
    }
    return true;
}

esp_err_t scenario_validate(const cJSON *root)
{
    const cJSON *steps = cJSON_GetObjectItem(root, "steps");
    if (!cJSON_IsArray(steps) || cJSON_GetArraySize(steps) == 0) {
        ESP_LOGE(TAG, "'steps' manquant, vide ou non tableau");
        return ESP_ERR_INVALID_ARG;
    }

    int idx = 0;
    const cJSON *step;
    cJSON_ArrayForEach(step, steps) {
        if (!cJSON_IsObject(step)) {
            ESP_LOGE(TAG, "steps[%d] n'est pas un objet", idx);
            return ESP_ERR_INVALID_ARG;
        }
        const char *sid = str_of(step, "id");
        if (!sid) {
            ESP_LOGE(TAG, "steps[%d] : 'id' manquant ou non chaîne", idx);
            return ESP_ERR_INVALID_ARG;
        }
        // Unicité : aucun step précédent ne porte le même id.
        int j = 0;
        const cJSON *prev;
        cJSON_ArrayForEach(prev, steps) {
            if (j++ >= idx) break;
            const char *pid = str_of(prev, "id");
            if (pid && strcmp(pid, sid) == 0) {
                ESP_LOGE(TAG, "id dupliqué : '%s'", sid);
                return ESP_ERR_INVALID_ARG;
            }
        }

        const char *type = str_of(step, "type");
        if (!type || !in_set(type, STEP_TYPES)) {
            ESP_LOGE(TAG, "step '%s' : 'type' manquant ou inconnu", sid);
            return ESP_ERR_INVALID_ARG;
        }

        bool ok = ref_ok(steps, step, "next", sid) &&
                  ref_ok(steps, step, "next_timeout", sid) &&
                  ref_ok(steps, step, "default", sid);
        if (ok) {
            if (strcmp(type, "narrative") == 0 || strcmp(type, "end") == 0) {
                ok = actions_ok(step, "do", sid);
            } else if (strcmp(type, "branch") == 0) {
                ok = branch_step_ok(steps, step, sid);
            } else {
                ok = wait_step_ok(step, sid, type);
            }
        }
        if (!ok) return ESP_ERR_INVALID_ARG;
        idx++;
    }
    return ESP_OK;
}
