#!/usr/bin/env python3
"""
yaml2json.py — Convertisseur de scénarios EscapeBox YAML → JSON firmware.

Usage:
    python3 tools/yaml2json.py firmware/scenarios/mon_scenario.yaml
    python3 tools/yaml2json.py firmware/scenarios/mon_scenario.yaml -o firmware/scenarios/mon_scenario.json

Le JSON produit est validé contre le schéma du moteur de scénario avant écriture.
"""

import sys
import json
import argparse
import re
from pathlib import Path

try:
    import yaml
except ImportError:
    sys.exit("Erreur : pyyaml non installé — pip install pyyaml")

# Le DSL scénario utilise 'on:' (déclencheur) et 'off' (mode LED) comme chaînes.
# Or PyYAML (spec YAML 1.1) résout on/off/yes/no en booléens → 'on:' deviendrait
# la clé True. On retire ces résolveurs pour ce loader (true/false restent booléens).
class ScenarioLoader(yaml.SafeLoader):
    pass

ScenarioLoader.yaml_implicit_resolvers = {
    k: list(v) for k, v in ScenarioLoader.yaml_implicit_resolvers.items()
}
for _ch in "oOyYnN":
    if _ch in ScenarioLoader.yaml_implicit_resolvers:
        ScenarioLoader.yaml_implicit_resolvers[_ch] = [
            (tag, rx) for (tag, rx) in ScenarioLoader.yaml_implicit_resolvers[_ch]
            if tag != "tag:yaml.org,2002:bool"
        ]

# --- Schéma de validation ---

VALID_STEP_TYPES = {"narrative", "trigger", "input", "branch", "end"}
VALID_EVENT_TYPES = {
    "rfid_read", "keypad_code", "touch", "rotary_value",
    "hall_detected", "breath_detected", "accel_tilt",
}
VALID_ACTIONS = {
    "screen_main", "screen_secondary", "audio", "led", "servo", "flash",
    "set_var", "incr_var",
}
VALID_LED_MODES = {"solid", "pulse", "flash", "cycle", "off"}
VALID_SERVO_ACTIONS = {"open", "close", "toggle"}
VALID_BRANCH_OPS = {"eq", "neq", "gt", "gte", "lt", "lte"}

COLOR_RE = re.compile(r'^#[0-9A-Fa-f]{6}$')

errors: list[str] = []
warnings: list[str] = []

def err(msg: str):
    errors.append(f"  ERREUR : {msg}")

def warn(msg: str):
    warnings.append(f"  AVERTISSEMENT : {msg}")

# --- Validation ---

def validate_action(action: dict, ctx: str):
    if not isinstance(action, dict) or len(action) != 1:
        err(f"{ctx} : action doit être un dict à une seule clé, got {action!r}")
        return
    name, params = next(iter(action.items()))

    if name not in VALID_ACTIONS:
        warn(f"{ctx} : action inconnue '{name}' (sera ignorée si pas de handler)")

    if name == "led":
        if not isinstance(params, dict):
            err(f"{ctx}/led : params doit être un dict")
            return
        color = params.get("color")
        if color and not COLOR_RE.match(str(color)):
            err(f"{ctx}/led : color '{color}' invalide (format #RRGGBB)")
        mode = params.get("mode")
        if mode and mode not in VALID_LED_MODES:
            err(f"{ctx}/led : mode '{mode}' inconnu (valides: {VALID_LED_MODES})")

    elif name == "servo":
        action_val = params.get("action") if isinstance(params, dict) else None
        if action_val and action_val not in VALID_SERVO_ACTIONS:
            err(f"{ctx}/servo : action '{action_val}' inconnue")

def validate_action_list(actions, ctx: str):
    if actions is None:
        return
    if not isinstance(actions, list):
        err(f"{ctx} : doit être une liste d'actions")
        return
    for a in actions:
        validate_action(a, ctx)

def validate_hints(hints, ctx: str):
    if hints is None:
        return
    if not isinstance(hints, list):
        err(f"{ctx}/hints : doit être une liste")
        return
    last_delay = -1
    for i, h in enumerate(hints):
        delay = h.get("delay_sec", 0)
        if not isinstance(delay, (int, float)) or delay < 0:
            err(f"{ctx}/hints[{i}] : delay_sec invalide")
        if delay < last_delay:
            warn(f"{ctx}/hints[{i}] : hints non triés par delay_sec (attendus croissants)")
        last_delay = delay
        validate_action_list(h.get("do"), f"{ctx}/hints[{i}]/do")

def validate_step(step: dict, step_ids: set, idx: int):
    sid = step.get("id", f"[{idx}]")
    ctx = f"step '{sid}'"

    if not isinstance(step.get("id"), str):
        err(f"{ctx} : 'id' manquant ou invalide")

    stype = step.get("type")
    if stype not in VALID_STEP_TYPES:
        err(f"{ctx} : type '{stype}' invalide (valides: {VALID_STEP_TYPES})")
        return

    if stype == "narrative":
        validate_action_list(step.get("do"), f"{ctx}/do")
        if "next" not in step:
            err(f"{ctx} : 'next' manquant pour un step narrative")

    elif stype in ("trigger", "input"):
        on = step.get("on")
        if on not in VALID_EVENT_TYPES:
            err(f"{ctx} : 'on' '{on}' invalide (valides: {VALID_EVENT_TYPES})")
        timeout = step.get("timeout_sec")
        if timeout is not None and (not isinstance(timeout, (int, float)) or timeout <= 0):
            err(f"{ctx} : timeout_sec invalide")
        validate_hints(step.get("hints"), ctx)
        if stype == "trigger":
            validate_action_list(step.get("do"), f"{ctx}/do")
        else:
            validate_action_list(step.get("do_success"), f"{ctx}/do_success")
            validate_action_list(step.get("do_fail"),    f"{ctx}/do_fail")
            validate_action_list(step.get("do_timeout"), f"{ctx}/do_timeout")
        if "next" not in step:
            err(f"{ctx} : 'next' manquant")

    elif stype == "branch":
        conds = step.get("conditions")
        if not isinstance(conds, list) or not conds:
            err(f"{ctx} : 'conditions' manquante ou vide")
        else:
            for i, c in enumerate(conds):
                if c.get("op") not in VALID_BRANCH_OPS:
                    err(f"{ctx}/conditions[{i}] : op '{c.get('op')}' invalide")
                if "var" not in c:
                    err(f"{ctx}/conditions[{i}] : 'var' manquant")
                if "next" not in c:
                    err(f"{ctx}/conditions[{i}] : 'next' manquant")
        if "default" not in step:
            warn(f"{ctx} : 'default' manquant — branch sans issue si aucune condition ne match")

    elif stype == "end":
        validate_action_list(step.get("do"), f"{ctx}/do")

def validate_scenario(data: dict) -> bool:
    meta = data.get("meta")
    if not isinstance(meta, dict):
        err("'meta' manquant ou invalide")
    else:
        for field in ("id", "title", "version"):
            if field not in meta:
                warn(f"meta.{field} manquant")

    steps = data.get("steps")
    if not isinstance(steps, list) or not steps:
        err("'steps' manquant ou vide")
        return False

    step_ids = set()
    for i, step in enumerate(steps):
        sid = step.get("id")
        if sid in step_ids:
            err(f"ID dupliqué : '{sid}'")
        step_ids.add(sid)

    for i, step in enumerate(steps):
        validate_step(step, step_ids, i)

    # Vérifie que tous les 'next' pointent vers des IDs existants
    def check_next(val, ctx):
        if val and val != "end" and val not in step_ids:
            err(f"{ctx} : 'next' pointe vers un step inexistant '{val}'")

    for step in steps:
        check_next(step.get("next"), f"step '{step.get('id')}'")
        check_next(step.get("next_timeout"), f"step '{step.get('id')}'/next_timeout")
        conds = step.get("conditions", [])
        for c in (conds if isinstance(conds, list) else []):
            check_next(c.get("next"), f"step '{step.get('id')}'/branch condition")
        check_next(step.get("default"), f"step '{step.get('id')}'/default")

    return len(errors) == 0

# --- Conversion ---

def yaml_to_json(yaml_path: Path, json_path: Path):
    print(f"Lecture : {yaml_path}")
    with open(yaml_path, encoding="utf-8") as f:
        data = yaml.load(f, Loader=ScenarioLoader)

    print("Validation...")
    ok = validate_scenario(data)

    for w in warnings:
        print(w)
    for e in errors:
        print(e)

    if not ok:
        print(f"\n  {len(errors)} erreur(s) — JSON non généré.")
        sys.exit(1)

    if warnings:
        print(f"  {len(warnings)} avertissement(s).")

    out = json.dumps(data, ensure_ascii=False, indent=2)
    json_path.write_text(out, encoding="utf-8")
    print(f"OK : {json_path} ({len(out)} octets, {len(data['steps'])} steps)")

# --- CLI ---

def main():
    parser = argparse.ArgumentParser(description="Convertisseur YAML → JSON EscapeBox")
    parser.add_argument("input",  type=Path, help="Fichier YAML source")
    parser.add_argument("-o", "--output", type=Path, default=None,
                        help="Fichier JSON de sortie (défaut : même nom, extension .json)")
    args = parser.parse_args()

    if not args.input.exists():
        sys.exit(f"Fichier introuvable : {args.input}")

    out_path = args.output or args.input.with_suffix(".json")
    yaml_to_json(args.input, out_path)

if __name__ == "__main__":
    main()
