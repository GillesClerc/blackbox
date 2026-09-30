import { createHash } from "node:crypto";
import { unzipSync } from "fflate";
import { validateScenario } from "./validate";

// Construction d'un package d'histoire à partir d'un zip déposé dans le
// back-office. Mêmes règles que tools/package_scenario.py (alignées sur le
// firmware, cloud_client.c), plus une taille maximale totale (100 Mo, E2).

export const SLUG_RE = /^[a-z0-9_-]{1,64}$/;
const SEGMENT_RE = /^[A-Za-z0-9_-][A-Za-z0-9._-]*$/;
export const MAX_FILE_BYTES = 32 * 1024 * 1024;
export const MAX_FILES = 128;
export const MAX_TOTAL_BYTES = 100 * 1024 * 1024;

export type PackageFile = { path: string; data: Uint8Array };
export type ManifestEntry = { path: string; bytes: number; sha256: string };
export type Manifest = {
  slug: string;
  version: number;
  total_bytes: number;
  files: ManifestEntry[];
};

export type BuildResult =
  | { ok: true; manifest: Manifest; files: PackageFile[]; warnings: string[] }
  | { ok: false; errors: string[] };

// Profondeur <= 2 sous-dossiers, segments sûrs (jamais de '.' initial).
export function relPathOk(rel: string): boolean {
  const parts = rel.split("/");
  return parts.length <= 3 && parts.every((p) => SEGMENT_RE.test(p));
}

// Fréquence d'échantillonnage du premier frame MPEG (portage de
// package_scenario.py) ; null si introuvable.
export function mp3SampleRate(data: Uint8Array): number | null {
  const RATES_V1: Record<number, number> = { 0: 44100, 1: 48000, 2: 32000 };
  let pos = 0;
  if (data[0] === 0x49 && data[1] === 0x44 && data[2] === 0x33) {
    // tag ID3v2, taille « syncsafe »
    const size = (data[6] << 21) | (data[7] << 14) | (data[8] << 7) | data[9];
    pos = 10 + size;
  }
  const end = Math.min(data.length - 4, pos + 65536);
  for (let i = pos; i < end; i++) {
    if (data[i] === 0xff && (data[i + 1] & 0xe0) === 0xe0) {
      const version = (data[i + 1] >> 3) & 0x03; // 3 = MPEG1
      const rateIdx = (data[i + 2] >> 2) & 0x03;
      let rate = RATES_V1[rateIdx];
      if (rate === undefined) continue;
      if (version === 2) rate = Math.floor(rate / 2); // MPEG2
      else if (version === 0) rate = Math.floor(rate / 4); // MPEG2.5
      return rate;
    }
  }
  return null;
}

// Ignorés silencieusement : artefacts d'archivage macOS/Windows et manifest
// éventuel (le manifest est TOUJOURS recalculé ici, jamais repris).
function isJunk(p: string): boolean {
  const base = p.split("/").pop() ?? "";
  return (
    p.startsWith("__MACOSX/") ||
    base === ".DS_Store" ||
    base === "Thumbs.db" ||
    base === "desktop.ini" ||
    base === "manifest.json"
  );
}

// Lit un zip. Si tout est rangé dans un unique dossier racine (cas d'un dossier
// compressé tel quel), ce dossier est retiré des chemins.
export function readZip(zip: Uint8Array): PackageFile[] {
  const entries = unzipSync(zip);
  let files = Object.entries(entries)
    .filter(([name]) => !name.endsWith("/")) // dossiers
    .map(([name, data]) => ({ path: name.replace(/\\/g, "/"), data }))
    .filter((f) => !isJunk(f.path));
  const roots = new Set(files.map((f) => f.path.split("/")[0]));
  if (roots.size === 1 && files.every((f) => f.path.includes("/"))) {
    const root = [...roots][0] + "/";
    if (!files.some((f) => f.path === "scenario.json")) {
      files = files.map((f) => ({ ...f, path: f.path.slice(root.length) }));
    }
  }
  return files;
}

const sha256 = (d: Uint8Array) => createHash("sha256").update(d).digest("hex");

export function buildPackage(
  slug: string,
  version: number,
  files: PackageFile[]
): BuildResult {
  const errors: string[] = [];
  const warnings: string[] = [];

  if (!SLUG_RE.test(slug)) errors.push(`slug invalide : ${slug}`);
  if (!Number.isInteger(version) || version < 1) errors.push(`version invalide : ${version}`);
  if (files.length === 0) errors.push("archive vide");
  if (files.length > MAX_FILES) errors.push(`trop de fichiers (${files.length} > ${MAX_FILES})`);

  const seen = new Set<string>();
  let total = 0;
  for (const f of files) {
    if (seen.has(f.path)) errors.push(`fichier en double : ${f.path}`);
    seen.add(f.path);
    if (!relPathOk(f.path)) errors.push(`chemin refusé : ${f.path}`);
    if (f.data.length > MAX_FILE_BYTES) {
      errors.push(`fichier trop gros (${f.data.length} o) : ${f.path}`);
    }
    total += f.data.length;
    if (f.path.toLowerCase().endsWith(".mp3")) {
      const rate = mp3SampleRate(f.data);
      if (rate !== 44100) {
        warnings.push(`${f.path} : ${rate ?? "?"} Hz (44100 attendu par le mixer de la box)`);
      }
    }
  }
  if (total > MAX_TOTAL_BYTES) {
    errors.push(`package trop gros (${total} o > ${MAX_TOTAL_BYTES} o)`);
  }

  const scenarioFile = files.find((f) => f.path === "scenario.json");
  if (!scenarioFile) {
    errors.push("scenario.json manquant à la racine du package");
  } else {
    let parsed: unknown;
    try {
      parsed = JSON.parse(new TextDecoder("utf-8", { fatal: true }).decode(scenarioFile.data));
    } catch {
      errors.push("scenario.json : JSON invalide");
    }
    if (parsed !== undefined) {
      const v = validateScenario(parsed);
      if (!v.ok) errors.push(`scenario.json : ${v.error}`);
    }
  }

  if (errors.length > 0) return { ok: false, errors };

  const sorted = [...files].sort((a, b) => (a.path < b.path ? -1 : a.path > b.path ? 1 : 0));
  const entries = sorted.map((f) => ({ path: f.path, bytes: f.data.length, sha256: sha256(f.data) }));
  return {
    ok: true,
    manifest: { slug, version, total_bytes: total, files: entries },
    files: sorted,
    warnings,
  };
}
