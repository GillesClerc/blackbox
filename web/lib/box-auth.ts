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
// dérivée du master pour ne pas multiplier les secrets d'environnement.
function getJwtSecret(): Uint8Array {
  return new TextEncoder().encode(getMasterSecret());
}

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

export function verifyBoxHmac(
  boxUid: string,
  challenge: string,
  hmac: string
): boolean {
  const expected = createHmac("sha256", boxSecret(boxUid))
    .update(`${boxUid}:${challenge}`)
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
    .setSubject(payload.boxUid)
    .setJti(randomUUID())
    .setIssuedAt()
    .setExpirationTime("2h")
    .sign(getJwtSecret());
}

export async function verifyBoxJwt(token: string): Promise<BoxJwtPayload> {
  const { payload } = await jwtVerify(token, getJwtSecret());
  return payload as BoxJwtPayload;
}

// Extrait le token d'un header "Authorization: Bearer <jwt>", sinon null.
export function bearerToken(authHeader: string | null): string | null {
  if (!authHeader) return null;
  const [scheme, value] = authHeader.split(" ");
  if (scheme !== "Bearer" || !value) return null;
  return value;
}
