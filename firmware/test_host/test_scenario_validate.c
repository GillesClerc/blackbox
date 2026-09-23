// Test host du validateur de scénario (components/scenario/scenario_validate.c).
// Lancer : firmware/test_host/run.sh — compile avec ASan/UBSan, sans ESP-IDF.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "scenario_validate.h"

static char *slurp(const char *p) {
    FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *b = malloc(n + 1); fread(b, 1, n, f); b[n] = 0; fclose(f); return b;
}
static int fails = 0;
static void expect(const char *name, const char *json, int want_ok) {
    cJSON *r = cJSON_Parse(json);
    int ok = r && scenario_validate(r) == ESP_OK;
    printf("%s %s\n", ok == want_ok ? "PASS" : "FAIL", name);
    if (ok != want_ok) fails++;
    cJSON_Delete(r);
}
#define STEP_IN(extra) "{\"steps\":[{\"id\":\"a\",\"type\":\"input\",\"on\":\"keypad_code\"" extra ",\"next\":\"b\"},{\"id\":\"b\",\"type\":\"end\"}]}"
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    char *emb = slurp(FW_DIR "/scenarios/capitaine_verdier.json");
    char *pkg = slurp(FW_DIR "/../web/scenario-packages/capitaine_verdier/scenario.json");
    expect("scénario embarqué valide", emb, 1);
    expect("scénario package valide", pkg, 1);
    expect("input minimal valide", STEP_IN(",\"expect\":{\"code\":\"7394\"}"), 1);
    expect("expect.code nombre refusé", STEP_IN(",\"expect\":{\"code\":7394}"), 0);
    expect("expect.uid nombre refusé", STEP_IN(",\"expect\":{\"uid\":12}"), 0);
    expect("expect non objet refusé", STEP_IN(",\"expect\":\"7394\""), 0);
    expect("action non objet refusée", STEP_IN(",\"do_success\":[\"led\"]"), 0);
    expect("do non tableau refusé", STEP_IN(",\"do_fail\":{\"led\":{}}"), 0);
    expect("timeout texte refusé", STEP_IN(",\"timeout_sec\":\"10\""), 0);
    expect("hint delay texte refusé", STEP_IN(",\"hints\":[{\"delay_sec\":\"5\"}]"), 0);
    expect("next_timeout inexistant refusé", STEP_IN(",\"next_timeout\":\"zz\""), 0);
    expect("on inconnu refusé",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"input\",\"on\":\"laser\"}]}", 0);
    expect("id manquant refusé", "{\"steps\":[{\"type\":\"end\"}]}", 0);
    expect("id nombre refusé", "{\"steps\":[{\"id\":3,\"type\":\"end\"}]}", 0);
    expect("id dupliqué refusé",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"end\"},{\"id\":\"a\",\"type\":\"end\"}]}", 0);
    expect("type inconnu refusé", "{\"steps\":[{\"id\":\"a\",\"type\":\"inptu\"}]}", 0);
    expect("step non objet refusé", "{\"steps\":[\"a\"]}", 0);
    expect("steps vide refusé", "{\"steps\":[]}", 0);
    expect("steps absent refusé", "{\"meta\":{}}", 0);
    expect("next inexistant refusé",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"narrative\",\"next\":\"zz\"}]}", 0);
    expect("next nombre refusé",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"narrative\",\"next\":1}]}", 0);
    expect("next \"end\" sans step end accepté",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"narrative\",\"next\":\"end\"}]}", 1);
    expect("branch valide",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"branch\",\"conditions\":[{\"var\":\"x\",\"op\":\"gte\",\"value\":3,\"next\":\"b\"}],\"default\":\"b\"},{\"id\":\"b\",\"type\":\"end\"}]}", 1);
    expect("branch op inconnu refusé",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"branch\",\"conditions\":[{\"var\":\"x\",\"op\":\"~\",\"value\":3,\"next\":\"a\"}]}]}", 0);
    expect("branch sans conditions refusé",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"branch\",\"default\":\"a\"}]}", 0);
    expect("branch condition sans next refusée",
        "{\"steps\":[{\"id\":\"a\",\"type\":\"branch\",\"conditions\":[{\"var\":\"x\",\"op\":\"eq\",\"value\":1}]}]}", 0);
    free(emb); free(pkg);
    printf("%s (%d échec(s))\n", fails ? "ÉCHEC" : "OK", fails);
    return fails != 0;
}
