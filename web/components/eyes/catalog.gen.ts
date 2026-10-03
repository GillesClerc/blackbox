// Généré par tools/eye_assets_web.py — ne pas modifier à la main.
export const EYE_CATALOG = {
  default: { sclera: 200, iris: 80, mapW: 256, mapH: 64, irisMin: 90, irisMax: 130, legacy: true },
  dragon: { sclera: 160, iris: 160, mapW: 512, mapH: 80, irisMin: 80, irisMax: 400, legacy: false },
  goat: { sclera: 128, iris: 128, mapW: 402, mapH: 64, irisMin: 120, irisMax: 720, legacy: false },
  newt: { sclera: 200, iris: 80, mapW: 256, mapH: 64, irisMin: 180, irisMax: 750, legacy: false },
  nosclera: { sclera: 160, iris: 160, mapW: 512, mapH: 80, irisMin: 120, irisMax: 550, legacy: false },
  terminator: { sclera: 200, iris: 80, mapW: 256, mapH: 64, irisMin: 120, irisMax: 720, legacy: false },
  cat: { sclera: 180, iris: 128, mapW: 1, mapH: 1, irisMin: 100, irisMax: 600, legacy: false },
} as const;
export type EyeId = keyof typeof EYE_CATALOG;
