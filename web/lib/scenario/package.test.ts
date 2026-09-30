import { readFileSync } from "node:fs";
import path from "node:path";
import { zipSync, strToU8 } from "fflate";
import { describe, expect, it } from "vitest";
import { buildPackage, mp3SampleRate, readZip, relPathOk } from "./package";

const PKG = path.join(__dirname, "../../scenario-packages/capitaine_verdier");
const read = (p: string) => new Uint8Array(readFileSync(path.join(PKG, p)));
const SCEN = { steps: [{ id: "a", type: "end" }] };
const scen = (o: unknown = SCEN) => strToU8(JSON.stringify(o));

describe("buildPackage", () => {
  it("reproduit exactement le manifest de package_scenario.py (Capitaine Verdier v4)", () => {
    const ref = JSON.parse(readFileSync(path.join(PKG, "manifest.json"), "utf8"));
    const files = ref.files.map((f: { path: string }) => ({ path: f.path, data: read(f.path) }));
    const r = buildPackage("capitaine_verdier", 4, files);
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.manifest).toEqual(ref);
  });

  it("refuse un package sans scenario.json", () => {
    const r = buildPackage("x", 1, [{ path: "audio/a.mp3", data: new Uint8Array(4) }]);
    expect(r).toMatchObject({ ok: false });
    if (!r.ok) expect(r.errors.join()).toMatch(/scenario.json manquant/);
  });

  it("refuse un scenario.json que la box rejetterait", () => {
    const r = buildPackage("x", 1, [{ path: "scenario.json", data: scen({ steps: [] }) }]);
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.errors.join()).toMatch(/steps/);
  });

  it("refuse chemins dangereux, profondeur excessive, slug et version invalides", () => {
    for (const bad of ["../x", ".cache/a", "a/b/c/d.mp3", "a b.mp3"]) {
      expect(relPathOk(bad)).toBe(false);
    }
    expect(relPathOk("audio/sfx/boom.mp3")).toBe(true);
    const r = buildPackage("Mauvais Slug", 0, [{ path: "scenario.json", data: scen() }]);
    expect(r.ok).toBe(false);
    if (!r.ok) expect(r.errors).toHaveLength(2);
  });

  it("signale un MP3 qui n'est pas en 44,1 kHz sans bloquer", () => {
    // en-tête MPEG1 Layer III à 48 kHz (index de fréquence 1)
    const mp3 = new Uint8Array([0xff, 0xfb, 0x94, 0x00, 0, 0, 0, 0]);
    expect(mp3SampleRate(mp3)).toBe(48000);
    expect(mp3SampleRate(read("ambient.mp3"))).toBe(44100);
    const r = buildPackage("x", 1, [
      { path: "scenario.json", data: scen() },
      { path: "a.mp3", data: mp3 },
    ]);
    expect(r.ok).toBe(true);
    if (r.ok) expect(r.warnings[0]).toMatch(/48000 Hz/);
  });
});

describe("readZip", () => {
  it("retire le dossier racine unique et ignore les artefacts d'archivage", () => {
    const zip = zipSync({
      "capitaine/scenario.json": scen(),
      "capitaine/audio/a.mp3": new Uint8Array([1]),
      "capitaine/manifest.json": strToU8("{}"),
      "__MACOSX/capitaine/._a.mp3": new Uint8Array([0]),
      "capitaine/.DS_Store": new Uint8Array([0]),
    });
    expect(readZip(zip).map((f) => f.path).sort()).toEqual(["audio/a.mp3", "scenario.json"]);
  });

  it("garde les chemins tels quels si scenario.json est à la racine", () => {
    const zip = zipSync({ "scenario.json": scen(), "audio/a.mp3": new Uint8Array([1]) });
    expect(readZip(zip).map((f) => f.path).sort()).toEqual(["audio/a.mp3", "scenario.json"]);
  });
});
