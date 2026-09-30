// Portage TypeScript de firmware/components/scenario/scenario_validate.c : même
// validation structurelle qu'effectue la box AVANT de confier un scénario à son
// moteur. Le back-office refuse ainsi tout package que la box rejetterait.
//
// Fidélité à cJSON (le C fait foi) :
//   - cJSON_GetObjectItem ignore la CASSE des clés (première clé qui correspond) ;
//   - une clé présente avec la valeur null n'est PAS une clé absente ;
//   - « chaîne » = chaîne non vide (str_of), nombre = number JSON.
// Cas de test partagés avec le C : firmware/test_host/scenario_cases.json.

type Json = unknown;

const STEP_TYPES = ["narrative", "trigger", "input", "branch", "end"];
const EVENT_TYPES = [
  "rfid_read", "keypad_code", "touch", "rotary_value",
  "hall_detected", "breath_detected", "accel_tilt",
];
const BRANCH_OPS = ["eq", "neq", "gt", "gte", "lt", "lte"];

const MISSING = Symbol("absent");
type Item = Json | typeof MISSING;

const isObject = (v: Item): v is Record<string, Json> =>
  typeof v === "object" && v !== null && !Array.isArray(v);
const isArray = (v: Item): v is Json[] => Array.isArray(v);
const isNumber = (v: Item) => typeof v === "number";
const isString = (v: Item) => typeof v === "string";

// cJSON_GetObjectItem : insensible à la casse, MISSING si absent ou si owner
// n'est pas un objet.
function item(owner: Item, key: string): Item {
  if (!isObject(owner)) return MISSING;
  const k = key.toLowerCase();
  for (const name of Object.keys(owner)) {
    if (name.toLowerCase() === k) return owner[name];
  }
  return MISSING;
}

// str_of : chaîne non vide, sinon null.
function strOf(owner: Item, key: string): string | null {
  const v = item(owner, key);
  return typeof v === "string" && v.length > 0 ? v : null;
}

export type ValidationResult = { ok: true } | { ok: false; error: string };

class Invalid extends Error {}
const fail = (msg: string): never => {
  throw new Invalid(msg);
};

function stepExists(steps: Json[], id: string): boolean {
  return steps.some((s) => strOf(s, "id") === id);
}

// Liste d'actions optionnelle : absente, ou tableau d'objets.
function actionsOk(owner: Item, key: string, sid: string): void {
  const list = item(owner, key);
  if (list === MISSING) return;
  if (!isArray(list)) fail(`step '${sid}' : '${key}' doit être un tableau`);
  for (const a of list as Json[]) {
    if (!isObject(a)) fail(`step '${sid}' : action de '${key}' non objet`);
  }
}

// Référence optionnelle vers un step : absente, ou chaîne d'un id existant.
function refOk(steps: Json[], owner: Item, key: string, sid: string): void {
  const ref = item(owner, key);
  if (ref === MISSING) return;
  if (typeof ref !== "string" || ref.length === 0) {
    fail(`step '${sid}' : '${key}' doit être une chaîne`);
  }
  const id = ref as string;
  if (id !== "end" && !stepExists(steps, id)) {
    fail(`step '${sid}' : '${key}' → step inexistant '${id}'`);
  }
}

function waitStepOk(step: Item, sid: string, type: string): void {
  const on = strOf(step, "on");
  if (!on || !EVENT_TYPES.includes(on)) fail(`step '${sid}' : 'on' manquant ou inconnu`);

  const expect = item(step, "expect");
  if (expect !== MISSING) {
    if (!isObject(expect)) fail(`step '${sid}' : 'expect' doit être un objet`);
    for (const k of ["uid", "code"]) {
      const v = item(expect, k);
      if (v !== MISSING && !isString(v)) {
        fail(`step '${sid}' : expect.${k} doit être une chaîne (guillemets en YAML)`);
      }
    }
  }

  const to = item(step, "timeout_sec");
  if (to !== MISSING && !isNumber(to)) fail(`step '${sid}' : timeout_sec doit être un nombre`);

  const hints = item(step, "hints");
  if (hints !== MISSING) {
    if (!isArray(hints)) fail(`step '${sid}' : 'hints' doit être un tableau`);
    for (const h of hints as Json[]) {
      const d = item(h, "delay_sec");
      let bad = !isObject(h) || (d !== MISSING && !isNumber(d));
      if (!bad) {
        try {
          actionsOk(h, "do", sid);
        } catch {
          bad = true;
        }
      }
      if (bad) fail(`step '${sid}' : hint invalide`);
    }
  }

  if (type === "trigger") {
    actionsOk(step, "do", sid);
  } else {
    actionsOk(step, "do_success", sid);
    actionsOk(step, "do_fail", sid);
    actionsOk(step, "do_timeout", sid);
  }
}

function branchStepOk(steps: Json[], step: Item, sid: string): void {
  const conds = item(step, "conditions");
  if (!isArray(conds) || conds.length === 0) {
    fail(`step '${sid}' : 'conditions' manquante ou vide`);
  }
  for (const c of conds as Json[]) {
    const op = strOf(c, "op");
    const val = item(c, "value");
    let bad =
      !isObject(c) ||
      !strOf(c, "var") ||
      !op ||
      !BRANCH_OPS.includes(op) ||
      !(isNumber(val) || isString(val)) ||
      item(c, "next") === MISSING;
    if (!bad) {
      try {
        refOk(steps, c, "next", sid);
      } catch {
        bad = true;
      }
    }
    if (bad) fail(`step '${sid}' : condition invalide`);
  }
}

export function validateScenario(root: Json): ValidationResult {
  try {
    const steps = item(root, "steps");
    if (!isArray(steps) || steps.length === 0) fail("'steps' manquant, vide ou non tableau");
    const list = steps as Json[];

    list.forEach((step, idx) => {
      if (!isObject(step)) fail(`steps[${idx}] n'est pas un objet`);
      const sid = strOf(step, "id");
      if (!sid) fail(`steps[${idx}] : 'id' manquant ou non chaîne`);
      // Unicité : aucun step précédent ne porte le même id.
      for (let j = 0; j < idx; j++) {
        if (strOf(list[j], "id") === sid) fail(`id dupliqué : '${sid}'`);
      }
      const type = strOf(step, "type");
      if (!type || !STEP_TYPES.includes(type)) fail(`step '${sid}' : 'type' manquant ou inconnu`);

      refOk(list, step, "next", sid!);
      refOk(list, step, "next_timeout", sid!);
      refOk(list, step, "default", sid!);
      if (type === "narrative" || type === "end") actionsOk(step, "do", sid!);
      else if (type === "branch") branchStepOk(list, step, sid!);
      else waitStepOk(step, sid!, type!);
    });
    return { ok: true };
  } catch (e) {
    if (e instanceof Invalid) return { ok: false, error: e.message };
    throw e;
  }
}
