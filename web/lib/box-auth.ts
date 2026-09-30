import { SignJWT, jwtVerify } from "jose";
import {
  createHmac,
  hkdfSync,
  randomBytes,
  randomUUID,
  timingSafeEqual,
} from "crypto";

// 🔐 Secret par box, jamais embarqué. Chaque box a un secret dérivé de son UID
// eFuse via ce master serveur : box_secret = HKDF(master, "escapebox:<uid>").
// Le firmware ne stocke que son secret dérivé ; le serveur le recalcule à la volée.
// Lecture paresseuse (pas au chargement du module) : sinon `next build` plante
// en collectant la page data sans secrets en environnement.
function getMasterSecret(): string {
  const s = process.env.BOX_MASTER_SECRET;
  if (!s) {
    throw new Error(
      "BOX_MASTER_SECRET manquant (openssl rand -hex 32, jamais embarqué)"
    );
  }
  return s;
}

// Clé de signature des JWT box : propre au serveur (émis ET vérifié côté serveur),
// dérivée du master (HKDF, info "escapebox:jwt") pour ne pas multiplier les
// secrets d'environnement sans jamais signer avec le master lui-même. L'info
// ne peut pas entrer en collision avec un secret de box ("escapebox:<uid>") :
// un box_uid ne vaut jamais "jwt" (format ESP32S3-XXXX-XXXX).
function getJwtSecret(): Uint8Array {
  return new Uint8Array(
    hkdfSync("sha256", getMasterSecret(), "", "escapebox:jwt", 32)
  );
}

// Émetteur / audience : un JWT box n'est valable que sur l'API box.
export const BOX_JWT_ISSUER = "escapebox";
export const BOX_JWT_AUDIENCE = "escapebox:box";

export type BoxJwtPayload = {
  sub: string; // box_uid
  device_id: string;
  owner_id: string;
  jti: string;
};

function boxSecret(boxUid: string): Buffer {
  return Buffer.from(
    hkdfSync("sha256", getMasterSecret(), "", `escapebox:${boxUid}`, 32)
  );
}

export function newChallenge(): string {
  return randomBytes(32).toString("hex");
}

// Usage d'une signature — séparation de domaine : la box signe
// "<purpose>:<box_uid>:<challenge>". Le canal BLE (non authentifié) ne signe
// que des preuves "register" ; une telle preuve ne peut donc jamais servir à
// obtenir un JWT via /api/box/auth ("auth"). Aligné sur hal_box_auth (firmware)
// et tools/box_crypto.py.
export type BoxSigPurpose = "auth" | "register";

export function verifyBoxHmac(
  purpose: BoxSigPurpose,
  boxUid: string,
  challenge: string,
  hmac: string
): boolean {
  const expected = createHmac("sha256", boxSecret(boxUid))
    .update(`${purpose}:${boxUid}:${challenge}`)
    .digest("hex");
  const a = Buffer.from(expected);
  const b = Buffer.from(hmac);
  if (a.length !== b.length) return false;
  return timingSafeEqual(a, b);
}

export async function signBoxJwt(payload: {
  boxUid: string;
  deviceId: string;
  ownerId: string;
}): Promise<string> {
  return new SignJWT({
    device_id: payload.deviceId,
    owner_id: payload.ownerId,
  })
    .setProtectedHeader({ alg: "HS256" })
    .setIssuer(BOX_JWT_ISSUER)
    .setAudience(BOX_JWT_AUDIENCE)
    .setSubject(payload.boxUid)
    .setJti(randomUUID())
    .setIssuedAt()
    .setExpirationTime("2h")
    .sign(getJwtSecret());
}

export async function verifyBoxJwt(token: string): Promise<BoxJwtPayload> {
  const { payload } = await jwtVerify(token, getJwtSecret(), {
    issuer: BOX_JWT_ISSUER,
    audience: BOX_JWT_AUDIENCE,
    algorithms: ["HS256"],
  });
  return payload as BoxJwtPayload;
}

// Extrait le token d'un header "Authorization: Bearer <jwt>", sinon null.
export function bearerToken(authHeader: string | null): string | null {
  if (!authHeader) return null;
  const [scheme, value] = authHeader.split(" ");
  if (scheme !== "Bearer" || !value) return null;
  return value;
}
