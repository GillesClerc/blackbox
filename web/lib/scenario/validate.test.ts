import { readFileSync } from "node:fs";
import path from "node:path";
import { describe, expect, it } from "vitest";
import { validateScenario } from "./validate";

// Mêmes cas que le test C du firmware (firmware/test_host/scenario_cases.json) :
// si le validateur TypeScript dérive du C, ce test (ou celui du C) casse.
const FW = path.join(__dirname, "../../../firmware");
const { cases } = JSON.parse(
  readFileSync(path.join(FW, "test_host/scenario_cases.json"), "utf8")
) as { cases: { name: string; valid: boolean; file?: string; scenario?: unknown }[] };

describe("validateScenario — cas partagés avec le firmware", () => {
  it("le fichier de cas n'est pas vide", () => {
    expect(cases.length).toBeGreaterThan(20);
  });

  for (const c of cases) {
    it(c.name, () => {
      const scenario = c.file
        ? JSON.parse(readFileSync(path.join(FW, c.file), "utf8"))
        : c.scenario;
      const r = validateScenario(scenario);
      expect(r.ok, r.ok ? "" : r.error).toBe(c.valid);
    });
  }
});

describe("validateScenario — fidélité à cJSON", () => {
  it("clés insensibles à la casse (comme cJSON_GetObjectItem)", () => {
    expect(validateScenario({ STEPS: [{ ID: "a", Type: "end" }] }).ok).toBe(true);
  });

  it("une clé présente à null n'est pas une clé absente", () => {
    expect(validateScenario({ steps: [{ id: "a", type: "narrative", do: null }] }).ok).toBe(false);
    expect(validateScenario({ steps: [{ id: "a", type: "narrative", next: null }] }).ok).toBe(false);
  });

  it("racine non objet refusée", () => {
    expect(validateScenario([]).ok).toBe(false);
    expect(validateScenario(null).ok).toBe(false);
  });

  it("message d'erreur lisible", () => {
    const r = validateScenario({ steps: [{ id: "a", type: "narrative", next: "zz" }] });
    expect(r).toEqual({ ok: false, error: "step 'a' : 'next' → step inexistant 'zz'" });
  });
});
