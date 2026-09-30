// Test host du validateur de scénario (components/scenario/scenario_validate.c).
// Lancer : firmware/test_host/run.sh — compile avec ASan/UBSan, sans ESP-IDF.
//
// Les cas vivent dans scenario_cases.json, partagé avec le validateur TypeScript
// du back-office (web/lib/scenario/validate.test.ts) : un cas ajouté là-bas est
// testé des deux côtés, et toute divergence entre les deux implémentations casse
// l'un des deux tests.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "scenario_validate.h"

static char *slurp(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) { printf("FAIL lecture %s\n", p); return NULL; }
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *b = malloc(n + 1);
    size_t got = fread(b, 1, n, f); b[got] = 0; fclose(f);
    return b;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int fails = 0, count = 0;

    char *raw = slurp(FW_DIR "/test_host/scenario_cases.json");
    cJSON *doc = raw ? cJSON_Parse(raw) : NULL;
    const cJSON *cases = cJSON_GetObjectItemCaseSensitive(doc, "cases");
    if (!cJSON_IsArray(cases) || cJSON_GetArraySize(cases) == 0) {
        printf("FAIL scenario_cases.json illisible ou vide\n");
        return 1;
    }

    const cJSON *c;
    cJSON_ArrayForEach(c, cases) {
        const char *name = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(c, "name"));
        int want_ok = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(c, "valid"));
        const char *file = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(c, "file"));

        cJSON *owned = NULL;
        const cJSON *scenario = cJSON_GetObjectItemCaseSensitive(c, "scenario");
        if (file) {
            char path[512];
            snprintf(path, sizeof path, "%s/%s", FW_DIR, file);
            char *txt = slurp(path);
            owned = txt ? cJSON_Parse(txt) : NULL;
            free(txt);
            scenario = owned;
        }
        int ok = scenario && scenario_validate(scenario) == ESP_OK;
        printf("%s %s\n", ok == want_ok ? "PASS" : "FAIL", name ? name : "?");
        if (ok != want_ok) fails++;
        count++;
        cJSON_Delete(owned);
    }

    cJSON_Delete(doc);
    free(raw);
    printf("%s (%d cas, %d échec(s))\n", fails ? "ÉCHEC" : "OK", count, fails);
    return fails != 0;
}
