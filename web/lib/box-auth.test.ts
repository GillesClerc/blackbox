import { beforeAll, describe, expect, it } from "vitest";
import { SignJWT } from "jose";

// Même master et même vecteur que tools/test_box_crypto.py : ce test détecte
// toute dérive entre le serveur, tools/box_crypto.py et le firmware
// (hal_box_auth signe "<purpose>:<box_uid>:<challenge>").
const MASTER = "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff";
const UID = "ESP32S3-8FF7-D684";
const CHALLENGE = "deadbeef".repeat(8);
const EXPECTED_AUTH =
  "590412840de9d81a82914e06be8b24c5fa08c65cd74e5f1d0c68d11bf90fe07d";

let mod: typeof import("./box-auth");

beforeAll(async () => {
  process.env.BOX_MASTER_SECRET = MASTER;
  mod = await import("./box-auth");
});

describe("verifyBoxHmac", () => {
  it("accepte le vecteur de référence (aligné sur box_crypto.py)", () => {
    expect(mod.verifyBoxHmac("auth", UID, CHALLENGE, EXPECTED_AUTH)).toBe(true);
  });

  it("refuse une signature auth présentée comme register (séparation de domaine)", () => {
    expect(mod.verifyBoxHmac("register", UID, CHALLENGE, EXPECTED_AUTH)).toBe(false);
  });

  it("refuse une signature d'une autre box ou d'un autre challenge", () => {
    expect(mod.verifyBoxHmac("auth", "ESP32S3-0000-0000", CHALLENGE, EXPECTED_AUTH)).toBe(false);
    expect(mod.verifyBoxHmac("auth", UID, "00".repeat(32), EXPECTED_AUTH)).toBe(false);
    expect(mod.verifyBoxHmac("auth", UID, CHALLENGE, "abc")).toBe(false);
  });
});

describe("JWT box", () => {
  const claims = { boxUid: UID, deviceId: "dev-1", ownerId: "owner-1" };

  it("signe puis vérifie un jeton", async () => {
    const token = await mod.signBoxJwt(claims);
    const p = await mod.verifyBoxJwt(token);
    expect(p.sub).toBe(UID);
    expect(p.device_id).toBe("dev-1");
    expect(p.owner_id).toBe("owner-1");
    expect(p.jti).toBeTruthy();
  });

  it("refuse un jeton de l'ancien schéma (signé avec le master brut)", async () => {
    const legacy = await new SignJWT({ device_id: "dev-1", owner_id: "owner-1" })
      .setProtectedHeader({ alg: "HS256" })
      .setSubject(UID)
      .setIssuedAt()
      .setExpirationTime("2h")
      .sign(new TextEncoder().encode(MASTER));
    await expect(mod.verifyBoxJwt(legacy)).rejects.toThrow();
  });

  it("refuse un jeton sans la bonne audience", async () => {
    const token = await mod.signBoxJwt(claims);
    const [h, , s] = token.split(".");
    const forged = `${h}.${Buffer.from(
      JSON.stringify({ sub: UID, aud: "autre", iss: mod.BOX_JWT_ISSUER })
    ).toString("base64url")}.${s}`;
    await expect(mod.verifyBoxJwt(forged)).rejects.toThrow();
  });

  it("refuse un jeton altéré", async () => {
    const token = await mod.signBoxJwt(claims);
    const tampered = token.slice(0, -2) + (token.endsWith("AA") ? "BB" : "AA");
    await expect(mod.verifyBoxJwt(tampered)).rejects.toThrow();
  });
});

describe("bearerToken", () => {
  it("extrait le jeton d'un en-tête Bearer", () => {
    expect(mod.bearerToken("Bearer abc.def")).toBe("abc.def");
    expect(mod.bearerToken("Basic abc")).toBeNull();
    expect(mod.bearerToken(null)).toBeNull();
  });
});
