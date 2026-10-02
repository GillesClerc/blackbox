// Port navigateur de firmware/components/ui_manager/eyes_anim.c — « Uncanny Eyes »
// d'Adafruit (MIT, Phil Burgess). Mêmes textures (public/eyes/*.png, générées par
// tools/eye_assets_web.py depuis defaultEye.h), même rendu pixel par pixel, mêmes
// mouvements autonomes, clignements, paupière qui suit la pupille et respiration de
// l'iris. Ajouts propres au site : regard qui suit le pointeur (par saccades, comme
// un vrai œil), clin d'œil, lumière qui contracte la pupille, humeurs lissées.

export const SCLERA = 200;
export const SCREEN = 128;
const IRIS = 80;
const IRIS_MAP_W = 256;
const IRIS_MAP_H = 64;
const IRIS_MIN = 90;
const IRIS_MAX = 130;

export type EyeAssets = {
  sclera: Uint32Array;
  iris: Uint32Array;
  polar: Uint16Array;
  upper: Uint8Array;
  lower: Uint8Array;
};

/** Rendu suréchantillonné (×S) : textures interpolées, LUT polaire recalculée. */
type Scaled = {
  S: number;
  sclera: Uint32Array; // (200·S)²
  upper: Uint8Array; // (128·S)²
  lower: Uint8Array;
  pDist: Float32Array; // (80·S)², distance au bord de l'iris 0..127 (127 = hors iris)
  pAng: Uint16Array; // colonne dans la carte d'iris 0..255
};

function upsample8(src: Uint8Array, w: number, S: number): Uint8Array {
  const W = w * S;
  const out = new Uint8Array(W * W);
  for (let Y = 0; Y < W; Y++) {
    const v = Math.min(w - 1, Math.max(0, (Y + 0.5) / S - 0.5));
    const y0 = v | 0, y1 = Math.min(w - 1, y0 + 1), fy = v - y0;
    for (let X = 0; X < W; X++) {
      const u = Math.min(w - 1, Math.max(0, (X + 0.5) / S - 0.5));
      const x0 = u | 0, x1 = Math.min(w - 1, x0 + 1), fx = u - x0;
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

const scaledCache = new Map<number, Scaled>();

function scaled(a: EyeAssets, S: number): Scaled {
  let sc = scaledCache.get(S);
  if (sc) return sc;
  // LUT polaire analytique : écart moyen à la table du firmware < 0,5 unité
  // (centre 39,5 ; rayon 40 ; angle 0 à gauche, sens horaire, 512 par tour).
  const N = IRIS * S;
  const pDist = new Float32Array(N * N);
  const pAng = new Uint16Array(N * N);
  for (let Y = 0; Y < N; Y++) {
    for (let X = 0; X < N; X++) {
      const dx = (X + 0.5) / S - 0.5 - 39.5;
      const dy = (Y + 0.5) / S - 0.5 - 39.5;
      const r = Math.hypot(dx, dy);
      pDist[Y * N + X] = r >= 40 ? 127 : ((40 - r) / 40) * 127;
      const th = Math.atan2(dy, dx);
      const ang = (((th - Math.PI) % (2 * Math.PI)) + 2 * Math.PI) % (2 * Math.PI);
      pAng[Y * N + X] = Math.min(IRIS_MAP_W - 1, ((ang / (2 * Math.PI)) * IRIS_MAP_W) | 0);
    }
  }
  sc = {
    S,
    sclera: upsampleRGBA(a.sclera, SCLERA, S),
    upper: upsample8(a.upper, SCREEN, S),
    lower: upsample8(a.lower, SCREEN, S),
    pDist,
    pAng,
  };
  scaledCache.set(S, sc);
  return sc;
}

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

let assetsPromise: Promise<EyeAssets> | null = null;

export function loadEyeAssets(base = "/eyes"): Promise<EyeAssets> {
  assetsPromise ??= (async () => {
    const [sclera, iris, polar, lids] = await Promise.all([
      readPng(`${base}/sclera.png`, SCLERA, SCLERA),
      readPng(`${base}/iris.png`, IRIS_MAP_W, IRIS_MAP_H),
      readPng(`${base}/polar.png`, IRIS, IRIS),
      readPng(`${base}/lids.png`, SCREEN, SCREEN),
    ]);
    const pol = new Uint16Array(IRIS * IRIS);
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
      sclera: new Uint32Array(sclera.slice().buffer),
      iris: new Uint32Array(iris.slice().buffer),
      polar: pol,
      upper,
      lower,
    };
  })();
  return assetsPromise;
}

/** Couleur CSS hexadécimale → pixel RGBA little-endian (format d'ImageData). */
export function packColor(hex: string): number {
  const v = parseInt(hex.replace("#", ""), 16);
  return ((0xff << 24) | ((v & 0xff) << 16) | (v & 0xff00) | ((v >> 16) & 0xff)) >>> 0;
}

/**
 * Rendu d'un œil : même algorithme que draw_eye() du firmware, à la résolution
 * 128·S. Coordonnées de fenêtre (scleraX0, scleraY0) en pixels suréchantillonnés.
 */
function drawEye(
  out: Uint32Array,
  a: EyeAssets,
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
  const SW = SCLERA * S;
  const IW = IRIS * S;
  const off = ((SCLERA - IRIS) / 2) * S;
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
        const d = ((iScale * sc.pDist[i]) / 128) | 0;
        p = d < IRIS_MAP_H ? a.iris[d * IRIS_MAP_W + sc.pAng[i]] : sc.sclera[scleraY * SW + scleraX];
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
  private lid: number;
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
  private irisEnd = (IRIS_MIN + IRIS_MAX) / 2;
  private bias = { top: 0, bot: 0, iris: 0 };
  private mood: Mood = "neutre";
  private light = 0;
  private lightNow = 0;
  autoBlink = true;

  private sc: Scaled;
  /** Côté du tampon de rendu, en pixels. */
  readonly size: number;

  constructor(assets: EyeAssets, lidColor = "#000000", S = 2) {
    this.a = assets;
    this.lid = packColor(lidColor);
    this.sc = scaled(assets, S);
    this.size = SCREEN * S;
  }

  setLidColor(hex: string) {
    this.lid = packColor(hex);
  }
  setMood(m: Mood) {
    this.mood = m;
  }
  getMood() {
    return this.mood;
  }
  /** null = le regard redevient autonome (saccades aléatoires du firmware). */
  setGaze(g: Gaze | null) {
    this.gaze = g;
  }
  /** 0 = pénombre, 1 = lampe dans les yeux (la pupille se contracte). */
  setLight(v: number) {
    this.light = clamp(v, 0, 1);
  }
  /** Clignement des deux yeux, ou d'un seul (clin d'œil). */
  blink(now: number, eye?: 0 | 1, speed = 1) {
    const dur = rand(36, 72) * speed;
    for (const e of eye === undefined ? [0, 1] : [eye]) {
      const b = this.blinks[e];
      if (b.state === 0) Object.assign(b, { state: 1, start: now, dur });
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
    while (!this.irisPlan.length || now >= this.irisPlan[this.irisPlan.length - 1].t0 + this.irisPlan[this.irisPlan.length - 1].dur) {
      const start = this.irisPlan.length ? this.irisPlan[this.irisPlan.length - 1].t0 + this.irisPlan[this.irisPlan.length - 1].dur : now;
      const target = Math.floor(rand(IRIS_MIN, IRIS_MAX));
      this.irisPlan = [];
      this.planIris(this.irisEnd, target, Math.max(start, now - 10_000), 10_000, IRIS_MAX - IRIS_MIN);
      this.irisEnd = target;
    }
    const s = this.irisPlan.find((g) => now < g.t0 + g.dur)!;
    return clamp(s.from + ((s.to - s.from) * (now - s.t0)) / s.dur, IRIS_MIN, IRIS_MAX);
  }

  /** Une fois par image : regard, clignements, humeur. */
  step(now: number) {
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
    } else if (!this.moving && now - this.moveStart > this.moveDur) {
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

    // Humeur et lumière : transitions lissées
    const m = MOODS[this.mood];
    this.bias.top += (m.top - this.bias.top) * 0.12;
    this.bias.bot += (m.bot - this.bias.bot) * 0.12;
    this.bias.iris += (m.iris - this.bias.iris) * 0.08;
    this.lightNow += (this.light - this.lightNow) * 0.1;
  }

  /** Rend l'œil `eye` (0 = gauche de l'écran) dans un tampon de `size`² pixels. */
  render(eye: 0 | 1, now: number, out: Uint32Array) {
    const iScale = clamp(this.irisAt(now) + this.bias.iris - this.lightNow * 45, 0, 1023);
    let ex = (this.cur.x * (SCLERA - SCREEN)) / 1023;
    const ey = (this.cur.y * (SCLERA - SCREEN)) / 1023;
    ex = clamp(ex + (eye === 1 ? 4 : -4), 0, SCLERA - SCREEN); // légère convergence

    // La paupière haute suit la pupille
    let n: number;
    const sampleX = SCLERA / 2 - ex / 2;
    let sampleY = SCLERA / 2 - (ey + IRIS / 4);
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
    const S = this.sc.S;
    drawEye(out, this.a, this.sc, eye === 0, iScale, Math.round(ex * S), Math.round(ey * S), uT, lT, this.lid);
  }
}
