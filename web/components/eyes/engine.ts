// Port navigateur de « Uncanny Eyes » d'Adafruit (MIT, Phil Burgess).
// - L'œil « default » est celui de la box : même texture et même formule de
//   pupille que firmware/components/ui_manager/eyes_anim.c.
// - Les autres personnages (dragon, faune, triton…) viennent du dépôt Adafruit
//   et suivent la formule de son croquis actuel (seuil d'iris).
// Textures : public/eyes/<id>/*.png, générées par tools/eye_assets_web.py.
// Mêmes mouvements autonomes, clignements, paupière qui suit la pupille et
// respiration de l'iris que le firmware. Ajouts propres au site : regard qui suit
// le pointeur (par saccades), clin d'œil, lumière qui contracte la pupille,
// humeurs lissées, teinte d'iris, changement de personnage pendant un clignement.

import { EYE_CATALOG, type EyeId } from "./catalog.gen";

export type { EyeId };
export const SCREEN = 128;

type Meta = (typeof EYE_CATALOG)[EyeId];

export type EyeAssets = {
  id: EyeId;
  meta: Meta;
  sclera: Uint32Array; // sclera² (RGBA little-endian, format d'ImageData)
  iris: Uint32Array; // mapW × mapH
  polar: Uint16Array; // iris², distance (7 bits) | angle (9 bits) << 7
  upper: Uint8Array; // 128²
  lower: Uint8Array;
};

async function readPng(src: string, w: number, h: number): Promise<Uint8ClampedArray> {
  const img = new Image();
  img.src = src;
  await img.decode();
  const c = document.createElement("canvas");
  c.width = w;
  c.height = h;
  const ctx = c.getContext("2d", { willReadFrequently: true })!;
  ctx.drawImage(img, 0, 0);
  return ctx.getImageData(0, 0, w, h).data;
}

const assetsCache = new Map<EyeId, Promise<EyeAssets>>();

export function loadEyeAssets(id: EyeId = "default", base = "/eyes"): Promise<EyeAssets> {
  let p = assetsCache.get(id);
  if (p) return p;
  const meta = EYE_CATALOG[id];
  p = (async () => {
    const [sclera, iris, polar, lids] = await Promise.all([
      readPng(`${base}/${id}/sclera.png`, meta.sclera, meta.sclera),
      readPng(`${base}/${id}/iris.png`, meta.mapW, meta.mapH),
      readPng(`${base}/${id}/polar.png`, meta.iris, meta.iris),
      readPng(`${base}/${id}/lids.png`, SCREEN, SCREEN),
    ]);
    const pol = new Uint16Array(meta.iris * meta.iris);
    for (let i = 0; i < pol.length; i++) {
      pol[i] = polar[i * 4] | (((polar[i * 4 + 1] << 1) | polar[i * 4 + 2]) << 7);
    }
    const upper = new Uint8Array(SCREEN * SCREEN);
    const lower = new Uint8Array(SCREEN * SCREEN);
    for (let i = 0; i < upper.length; i++) {
      upper[i] = lids[i * 4];
      lower[i] = lids[i * 4 + 1];
    }
    return {
      id,
      meta,
      sclera: new Uint32Array(sclera.slice().buffer),
      iris: new Uint32Array(iris.slice().buffer),
      polar: pol,
      upper,
      lower,
    };
  })();
  assetsCache.set(id, p);
  return p;
}

/** Couleur CSS hexadécimale → pixel RGBA little-endian (format d'ImageData). */
export function packColor(hex: string): number {
  const v = parseInt(hex.replace("#", ""), 16);
  return ((0xff << 24) | ((v & 0xff) << 16) | (v & 0xff00) | ((v >> 16) & 0xff)) >>> 0;
}

// ── Suréchantillonnage (×S) : paupières lisses à la taille où le site les montre ──

type Scaled = {
  S: number;
  sclera: Uint32Array; // (sclera·S)²
  upper: Uint8Array; // (128·S)²
  lower: Uint8Array;
  pDist: Uint8Array; // (iris·S)², distance au bord de l'iris 0..127
  pAng: Uint16Array; // colonne dans la carte d'iris
};

function upsample8(src: Uint8Array, w: number, S: number): Uint8Array {
  const W = w * S;
  const out = new Uint8Array(W * W);
  for (let Y = 0; Y < W; Y++) {
    const v = Math.min(w - 1, Math.max(0, (Y + 0.5) / S - 0.5));
    const y0 = v | 0,
      y1 = Math.min(w - 1, y0 + 1),
      fy = v - y0;
    for (let X = 0; X < W; X++) {
      const u = Math.min(w - 1, Math.max(0, (X + 0.5) / S - 0.5));
      const x0 = u | 0,
        x1 = Math.min(w - 1, x0 + 1),
        fx = u - x0;
      const a = src[y0 * w + x0] * (1 - fx) + src[y0 * w + x1] * fx;
      const b = src[y1 * w + x0] * (1 - fx) + src[y1 * w + x1] * fx;
      out[Y * W + X] = Math.round(a * (1 - fy) + b * fy);
    }
  }
  return out;
}

function upsampleRGBA(src: Uint32Array, w: number, S: number): Uint32Array {
  const ch = [0, 8, 16].map((sh) => {
    const c = new Uint8Array(w * w);
    for (let i = 0; i < c.length; i++) c[i] = (src[i] >>> sh) & 0xff;
    return upsample8(c, w, S);
  });
  const out = new Uint32Array(ch[0].length);
  for (let i = 0; i < out.length; i++) out[i] = ((0xff << 24) | (ch[2][i] << 16) | (ch[1][i] << 8) | ch[0][i]) >>> 0;
  return out;
}

/** LUT polaire agrandie : distance interpolée (sauf au bord de l'iris), angle au plus proche. */
function upsamplePolar(a: EyeAssets, S: number) {
  const n = a.meta.iris;
  const N = n * S;
  const pDist = new Uint8Array(N * N);
  const pAng = new Uint16Array(N * N);
  const dist = (x: number, y: number) => a.polar[y * n + x] & 0x7f;
  for (let Y = 0; Y < N; Y++) {
    const v = Math.min(n - 1, Math.max(0, (Y + 0.5) / S - 0.5));
    const y0 = v | 0,
      y1 = Math.min(n - 1, y0 + 1),
      fy = v - y0;
    for (let X = 0; X < N; X++) {
      const u = Math.min(n - 1, Math.max(0, (X + 0.5) / S - 0.5));
      const x0 = u | 0,
        x1 = Math.min(n - 1, x0 + 1),
        fx = u - x0;
      const nx = Math.min(n - 1, Math.round(u)),
        ny = Math.min(n - 1, Math.round(v));
      const d00 = dist(x0, y0),
        d10 = dist(x1, y0),
        d01 = dist(x0, y1),
        d11 = dist(x1, y1);
      pDist[Y * N + X] =
        d00 === 127 || d10 === 127 || d01 === 127 || d11 === 127
          ? dist(nx, ny)
          : Math.round((d00 * (1 - fx) + d10 * fx) * (1 - fy) + (d01 * (1 - fx) + d11 * fx) * fy);
      pAng[Y * N + X] = Math.min(a.meta.mapW - 1, ((a.meta.mapW * (a.polar[ny * n + nx] >> 7)) / 512) | 0);
    }
  }
  return { pDist, pAng };
}

const scaledCache = new Map<string, Scaled>();
function scaled(a: EyeAssets, S: number): Scaled {
  const key = `${a.id}@${S}`;
  let sc = scaledCache.get(key);
  if (!sc) {
    sc = {
      S,
      sclera: upsampleRGBA(a.sclera, a.meta.sclera, S),
      upper: upsample8(a.upper, SCREEN, S),
      lower: upsample8(a.lower, SCREEN, S),
      ...upsamplePolar(a, S),
    };
    scaledCache.set(key, sc);
  }
  return sc;
}

/** Iris recoloré : rotation de teinte (comme le filtre CSS hue-rotate). */
const tintCache = new Map<string, Uint32Array>();
function tinted(a: EyeAssets, hue: number): Uint32Array {
  if (!hue) return a.iris;
  const key = `${a.id}:${hue}`;
  let out = tintCache.get(key);
  if (out) return out;
  const r = (hue * Math.PI) / 180,
    c = Math.cos(r),
    s = Math.sin(r);
  const m = [
    0.213 + c * 0.787 - s * 0.213, 0.715 - c * 0.715 - s * 0.715, 0.072 - c * 0.072 + s * 0.928,
    0.213 - c * 0.213 + s * 0.143, 0.715 + c * 0.285 + s * 0.14, 0.072 - c * 0.072 - s * 0.283,
    0.213 - c * 0.213 - s * 0.787, 0.715 - c * 0.715 + s * 0.715, 0.072 + c * 0.928 + s * 0.072,
  ];
  const cl = (v: number) => (v < 0 ? 0 : v > 255 ? 255 : v | 0);
  out = new Uint32Array(a.iris.length);
  for (let i = 0; i < out.length; i++) {
    const p = a.iris[i];
    const R = p & 0xff,
      G = (p >>> 8) & 0xff,
      B = (p >>> 16) & 0xff;
    out[i] =
      ((0xff << 24) |
        (cl(m[6] * R + m[7] * G + m[8] * B) << 16) |
        (cl(m[3] * R + m[4] * G + m[5] * B) << 8) |
        cl(m[0] * R + m[1] * G + m[2] * B)) >>>
      0;
  }
  tintCache.set(key, out);
  return out;
}

/**
 * Rendu d'un œil à la résolution 128·S (même boucle que draw_eye()).
 * Coordonnées de fenêtre (scleraX0, scleraY0) en pixels suréchantillonnés.
 */
function drawEye(
  out: Uint32Array,
  a: EyeAssets,
  iris: Uint32Array,
  sc: Scaled,
  mirror: boolean,
  iScale: number,
  scleraX0: number,
  scleraY0: number,
  uT: number,
  lT: number,
  lid: number
) {
  const S = sc.S;
  const W = SCREEN * S;
  const SW = a.meta.sclera * S;
  const IW = a.meta.iris * S;
  const mapW = a.meta.mapW;
  const mapH = a.meta.mapH;
  const off = ((a.meta.sclera - a.meta.iris) / 2) * S;
  // Formule historique (firmware) : rangée = iScale·d/128, iris si < mapH.
  // Formule actuelle (Adafruit) : iris si d < seuil, rangée = d·mapH/seuil.
  const legacy = a.meta.legacy;
  const threshold = legacy ? 0 : ((128 * (1023 - iScale) + 512) / 1024) | 0;
  let scleraY = scleraY0;
  let irisY = scleraY0 - off;
  const dlidX = mirror ? -1 : 1;
  for (let y = 0; y < W; y++, scleraY++, irisY++) {
    let scleraX = scleraX0;
    let irisX = scleraX0 - off;
    let lidX = mirror ? W - 1 : 0;
    const row = y * W;
    for (let x = 0; x < W; x++, scleraX++, irisX++, lidX += dlidX) {
      let p: number;
      if (sc.lower[row + lidX] <= lT || sc.upper[row + lidX] <= uT) {
        p = lid;
      } else if (irisY < 0 || irisY >= IW || irisX < 0 || irisX >= IW) {
        p = sc.sclera[scleraY * SW + scleraX];
      } else {
        const i = irisY * IW + irisX;
        const d = sc.pDist[i];
        let r = -1;
        if (legacy) {
          const t = ((iScale * d) / 128) | 0;
          if (t < mapH) r = t;
        } else if (d < threshold) {
          r = ((d * mapH) / threshold) | 0;
        }
        p = r >= 0 ? iris[r * mapW + sc.pAng[i]] : sc.sclera[scleraY * SW + scleraX];
      }
      out[row + x] = p;
    }
  }
}

// Courbe 3t² − 2t³ (la table s_ease du firmware), t ∈ [0, 1].
const ease = (t: number) => t * t * (3 - 2 * t);
const rand = (lo: number, hi: number) => lo + Math.random() * (hi - lo);
const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));

/** Humeurs : biais de paupières et de pupille, repris de ui_face.c (+ « méfiante »). */
export const MOODS = {
  neutre: { top: 0, bot: 0, iris: 0 },
  contente: { top: 0, bot: 40, iris: 0 },
  triste: { top: 30, bot: 0, iris: 0 },
  surprise: { top: -20, bot: -20, iris: -40 },
  endormie: { top: 70, bot: 20, iris: 0 },
  fachee: { top: 60, bot: 0, iris: 60 },
  mefiante: { top: 48, bot: 30, iris: 10 },
} as const;
export type Mood = keyof typeof MOODS;

type Blink = { state: 0 | 1 | 2; start: number; dur: number };
type IrisSeg = { from: number; to: number; t0: number; dur: number };

export type Gaze = { x: number; y: number }; // −1..1, x vers la droite de l'écran, y vers le bas

export class EyesEngine {
  private a: EyeAssets;
  private sc: Scaled;
  private iris: Uint32Array;
  private readonly S: number;
  private lid: number;
  private pending: { a: EyeAssets; hue: number } | null = null;
  // Regard, en unités firmware 0..1023 (512 = centre).
  private cur = { x: 512, y: 512 };
  private from = { x: 512, y: 512 };
  private to = { x: 512, y: 512 };
  private moving = false;
  private moveStart = 0;
  private moveDur = 0;
  private gaze: Gaze | null = null;
  private blinks: [Blink, Blink] = [
    { state: 0, start: 0, dur: 0 },
    { state: 0, start: 0, dur: 0 },
  ];
  private lastBlink = 0;
  private nextBlink = 1500;
  private uThreshold = [128, 128];
  private irisPlan: IrisSeg[] = [];
  private irisEnd: number;
  private bias = { top: 0, bot: 0, iris: 0 };
  private mood: Mood = "neutre";
  private light = 0;
  private lightNow = 0;
  autoBlink = true;
  /** Côté du tampon de rendu, en pixels. */
  readonly size: number;

  constructor(assets: EyeAssets, lidColor = "#000000", S = 2, hue = 0) {
    this.S = S;
    this.a = assets;
    this.sc = scaled(assets, S);
    this.iris = tinted(assets, hue);
    this.irisEnd = (assets.meta.irisMin + assets.meta.irisMax) / 2;
    this.lid = packColor(lidColor);
    this.size = SCREEN * S;
  }

  setLidColor(hex: string) {
    this.lid = packColor(hex);
  }
  setMood(m: Mood) {
    this.mood = m;
  }
  /** null = le regard redevient autonome (saccades aléatoires du firmware). */
  setGaze(g: Gaze | null) {
    this.gaze = g;
  }
  /** 0 = pénombre, 1 = lampe dans les yeux (la pupille se contracte). */
  setLight(v: number) {
    this.light = clamp(v, 0, 1);
  }
  /** Change de personnage pendant un clignement lent (yeux fermés au moment du changement). */
  transitionTo(assets: EyeAssets, hue: number, now: number) {
    this.pending = { a: assets, hue };
    scaled(assets, this.S); // prépare les textures avant que les yeux se ferment
    tinted(assets, hue);
    this.blink(now, undefined, 3.2, true);
  }
  /** Clignement des deux yeux, ou d'un seul (clin d'œil). */
  blink(now: number, eye?: 0 | 1, speed = 1, force = false) {
    const dur = rand(36, 72) * speed;
    for (const e of eye === undefined ? [0, 1] : [eye]) {
      const b = this.blinks[e];
      if (b.state === 0 || force) Object.assign(b, { state: 1, start: now, dur });
    }
  }

  private planIris(from: number, to: number, t0: number, dur: number, range: number) {
    if (range >= 8) {
      range = Math.floor(range / 2);
      dur /= 2;
      const mid = Math.floor((from + to - range) / 2) + Math.floor(rand(0, range));
      this.planIris(from, mid, t0, dur, range);
      this.planIris(mid, to, t0 + dur, dur, range);
    } else {
      this.irisPlan.push({ from, to, t0, dur });
    }
  }

  private irisAt(now: number): number {
    const { irisMin, irisMax } = this.a.meta;
    const last = () => this.irisPlan[this.irisPlan.length - 1];
    while (!this.irisPlan.length || now >= last().t0 + last().dur) {
      const end = this.irisPlan.length ? last().t0 + last().dur : now;
      const target = Math.floor(rand(irisMin, irisMax));
      this.irisPlan = [];
      this.planIris(this.irisEnd, target, end < now - 10_000 ? now : end, 10_000, irisMax - irisMin);
      this.irisEnd = target;
    }
    const s = this.irisPlan.find((g) => now < g.t0 + g.dur)!;
    return clamp(s.from + ((s.to - s.from) * (now - s.t0)) / s.dur, irisMin, irisMax);
  }

  /** Une fois par image : regard, clignements, humeur. */
  step(now: number) {
    const span = this.a.meta.sclera - SCREEN;
    // Regard
    if (this.gaze) {
      const r = Math.hypot(this.gaze.x, this.gaze.y);
      const k = r > 1 ? 1 / r : 1;
      const tx = 512 - this.gaze.x * k * 511;
      const ty = 512 - this.gaze.y * k * 511;
      if (!this.moving) {
        if (Math.hypot(tx - this.cur.x, ty - this.cur.y) > 70) {
          // grand écart : saccade rapide, comme le firmware
          this.from = { ...this.cur };
          this.to = { x: tx, y: ty };
          this.moveStart = now;
          this.moveDur = rand(60, 110);
          this.moving = true;
        } else {
          // petit écart : poursuite douce
          this.cur.x += (tx - this.cur.x) * 0.2;
          this.cur.y += (ty - this.cur.y) * 0.2;
        }
      }
    } else if (!this.moving && now - this.moveStart > this.moveDur && span > 0) {
      let nx: number, ny: number;
      do {
        nx = rand(0, 1024);
        ny = rand(0, 1024);
      } while ((nx * 2 - 1023) ** 2 + (ny * 2 - 1023) ** 2 > 1023 * 1023);
      this.from = { ...this.cur };
      this.to = { x: nx, y: ny };
      this.moveStart = now;
      this.moveDur = rand(72, 144);
      this.moving = true;
    }
    if (this.moving) {
      const t = (now - this.moveStart) / this.moveDur;
      if (t >= 1) {
        this.moving = false;
        this.cur = { ...this.to };
        this.moveStart = now;
        this.moveDur = rand(0, 3000); // 0–3 s d'arrêt avant la prochaine saccade autonome
      } else {
        const e = ease(t);
        this.cur.x = this.from.x + (this.to.x - this.from.x) * e;
        this.cur.y = this.from.y + (this.to.y - this.from.y) * e;
      }
    }

    // Clignement automatique
    if (this.autoBlink && now - this.lastBlink >= this.nextBlink) {
      this.lastBlink = now;
      this.blink(now);
      this.nextBlink = this.blinks[0].dur * 3 + rand(0, 4000);
      if (Math.random() < 0.12) setTimeout(() => this.blink(performance.now()), 260); // double clignement
    }
    for (const b of this.blinks) {
      if (b.state && now - b.start >= b.dur) {
        if (b.state === 2) b.state = 0;
        else Object.assign(b, { state: 2, start: now, dur: b.dur * 2 }); // réouverture deux fois plus lente
      }
    }
    // Changement de personnage : au moment où les yeux sont fermés
    if (this.pending && this.blinks[0].state === 2) {
      this.a = this.pending.a;
      this.sc = scaled(this.a, this.S);
      this.iris = tinted(this.a, this.pending.hue);
      this.irisPlan = [];
      this.irisEnd = (this.a.meta.irisMin + this.a.meta.irisMax) / 2;
      this.pending = null;
    }

    // Humeur et lumière : transitions lissées
    const m = MOODS[this.mood];
    this.bias.top += (m.top - this.bias.top) * 0.12;
    this.bias.bot += (m.bot - this.bias.bot) * 0.12;
    this.bias.iris += (m.iris - this.bias.iris) * 0.08;
    this.lightNow += (this.light - this.lightNow) * 0.1;
  }

  /** Rend l'œil `eye` (0 = gauche de l'écran) dans un tampon de `size`² pixels. */
  render(eye: 0 | 1, now: number, out: Uint32Array) {
    const { sclera: SW, iris: IW, irisMin, irisMax, legacy } = this.a.meta;
    const range = irisMax - irisMin;
    // Biais d'humeur et lumière exprimés dans l'échelle de pupille de l'œil courant.
    const k = legacy ? 1 : range / 40;
    const iScale = clamp(this.irisAt(now) + this.bias.iris * k - this.lightNow * (legacy ? 45 : range * 0.9), 0, 1023);
    const span = SW - SCREEN;
    let ex = (this.cur.x * span) / 1023;
    const ey = (this.cur.y * span) / 1023;
    if (span > 0) ex = clamp(ex + (eye === 1 ? 4 : -4), 0, span); // légère convergence

    // La paupière haute suit la pupille
    let n: number;
    const sampleX = SW / 2 - ex / 2;
    let sampleY = SW / 2 - (ey + IW / 4);
    if (sampleY < 0) n = 0;
    else {
      const sx1 = clamp(sampleX, 0, SCREEN - 1) | 0;
      const sx2 = clamp(SCREEN - 1 - sampleX, 0, SCREEN - 1) | 0;
      sampleY = Math.min(sampleY, SCREEN - 1) | 0;
      n = ((this.a.upper[sampleY * SCREEN + sx1] + this.a.upper[sampleY * SCREEN + sx2]) / 2) | 0;
    }
    this.uThreshold[eye] = ((this.uThreshold[eye] * 3 + n) / 4) | 0;
    let lT = 254 - this.uThreshold[eye];

    const b = this.blinks[eye];
    if (b.state) {
      let s = now - b.start >= b.dur ? 255 : (255 * (now - b.start)) / b.dur;
      s = b.state === 2 ? 1 + s : 256 - s;
      n = (this.uThreshold[eye] * s + 254 * (257 - s)) / 256;
      lT = (lT * s + 254 * (257 - s)) / 256;
    } else {
      n = this.uThreshold[eye];
    }
    const uT = clamp(n + this.bias.top, 0, 255);
    lT = clamp(lT + this.bias.bot, 0, 255);
    const S = this.S;
    drawEye(out, this.a, this.iris, this.sc, eye === 0, iScale, Math.round(ex * S), Math.round(ey * S), uT, lT, this.lid);
  }
}
