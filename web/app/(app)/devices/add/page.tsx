"use client";

import { useCallback, useRef, useState } from "react";

// Appairage d'une box via Web Bluetooth (Chrome desktop/Android — pas iOS).
// Flux : connexion BLE → lecture box_uid → challenge serveur signé par la box
// (preuve de possession, option B) → envoi du WiFi → enregistrement.
// UUIDs alignés sur firmware/components/ble_prov (base e5c4000X-…).
const SVC_UUID = "e5c40001-5c25-4b10-8f46-6b9c30ac7a11";
const CHR = {
  boxUid: "e5c40002-5c25-4b10-8f46-6b9c30ac7a11",
  ssid: "e5c40003-5c25-4b10-8f46-6b9c30ac7a11",
  pass: "e5c40004-5c25-4b10-8f46-6b9c30ac7a11",
  status: "e5c40005-5c25-4b10-8f46-6b9c30ac7a11",
  challenge: "e5c40006-5c25-4b10-8f46-6b9c30ac7a11",
  response: "e5c40007-5c25-4b10-8f46-6b9c30ac7a11",
};

type Step = "idle" | "connecting" | "wifi" | "joining" | "registering" | "done";

const enc = new TextEncoder();
const dec = new TextDecoder();

export default function AddDevicePage() {
  const [step, setStep] = useState<Step>("idle");
  const [error, setError] = useState<string | null>(null);
  const [boxUid, setBoxUid] = useState<string>("");
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const [boxName, setBoxName] = useState("EscapeBox");

  // Références BLE + preuve, conservées entre les étapes.
  const svcRef = useRef<BluetoothRemoteGATTService | null>(null);
  const proofRef = useRef<{ challenge: string; response: string } | null>(null);

  const supported =
    typeof navigator !== "undefined" && "bluetooth" in navigator;

  const connect = useCallback(async () => {
    setError(null);
    setStep("connecting");
    try {
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: "EscapeBox" }],
        optionalServices: [SVC_UUID],
      });
      const gatt = await device.gatt!.connect();
      const svc = await gatt.getPrimaryService(SVC_UUID);
      svcRef.current = svc;

      const uidChr = await svc.getCharacteristic(CHR.boxUid);
      const uid = dec.decode(await uidChr.readValue());
      if (!uid || uid === "UNPROVISIONED") {
        throw new Error(
          "Box non provisionnée en usine (box_creds absente) — lancer tools/provision_box.py d'abord."
        );
      }
      setBoxUid(uid);

      // Preuve de possession : la box signe un nonce fraîchement émis.
      const chal = await fetch(
        `/api/box/challenge?box_uid=${encodeURIComponent(uid)}`
      ).then((r) => r.json());
      if (!chal.challenge) throw new Error("challenge serveur indisponible");

      const chalChr = await svc.getCharacteristic(CHR.challenge);
      await chalChr.writeValue(enc.encode(chal.challenge));
      const respChr = await svc.getCharacteristic(CHR.response);
      const response = dec.decode(await respChr.readValue());
      if (!/^[0-9a-f]{64}$/.test(response)) {
        throw new Error("la box n'a pas signé le challenge");
      }
      proofRef.current = { challenge: chal.challenge, response };

      setStep("wifi");
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setStep("idle");
    }
  }, []);

  const sendWifi = useCallback(async () => {
    const svc = svcRef.current;
    if (!svc || !ssid) return;
    setError(null);
    setStep("joining");
    try {
      // ssid PUIS pass — l'écriture du pass déclenche la connexion côté box.
      await (await svc.getCharacteristic(CHR.ssid)).writeValue(enc.encode(ssid));
      await (await svc.getCharacteristic(CHR.pass)).writeValue(
        enc.encode(password)
      );

      // La box notifie status ; fallback en polling (20 s max).
      const statusChr = await svc.getCharacteristic(CHR.status);
      const result = await new Promise<string>((resolve, reject) => {
        const deadline = setTimeout(
          () => reject(new Error("délai dépassé (20 s)")),
          20000
        );
        const check = (v: string) => {
          if (v === "wifi_ok") {
            clearTimeout(deadline);
            resolve(v);
          } else if (v === "wifi_fail") {
            clearTimeout(deadline);
            reject(new Error("la box n'a pas réussi à joindre ce réseau"));
          }
        };
        statusChr
          .startNotifications()
          .then(() => {
            statusChr.addEventListener("characteristicvaluechanged", (ev) => {
              const t = ev.target as BluetoothRemoteGATTCharacteristic;
              if (t.value) check(dec.decode(t.value));
            });
          })
          .catch(() => {
            const poll = setInterval(async () => {
              try {
                check(dec.decode(await statusChr.readValue()));
              } catch {
                /* lecture ratée : on repollera */
              }
            }, 1500);
            setTimeout(() => clearInterval(poll), 21000);
          });
      });
      void result;

      // WiFi OK → enregistrement avec la preuve signée plus tôt.
      setStep("registering");
      const proof = proofRef.current!;
      const reg = await fetch("/api/box/register", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          box_uid: boxUid,
          name: boxName,
          challenge: proof.challenge,
          challenge_response: proof.response,
        }),
      });
      const regBody = await reg.json();
      if (!reg.ok) throw new Error(regBody.error ?? "enregistrement refusé");

      setStep("done");
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
      setStep("wifi");
    }
  }, [ssid, password, boxUid, boxName]);

  const label = "font-mono text-xs tracking-[0.2em] text-muted-foreground";
  const input =
    "mt-1 w-full rounded-lg border border-border bg-background px-3 py-2 text-foreground outline-none focus:border-foreground/40";
  const button =
    "rounded-lg bg-foreground px-5 py-2.5 font-medium text-background transition-opacity hover:opacity-85 disabled:opacity-40";

  return (
    <main className="mx-auto w-full max-w-5xl flex-1 px-6 py-12">
      <p className="font-mono text-xs tracking-[0.25em] text-glow">
        APPAIRAGE
      </p>
      <h1 className="mt-3 font-display text-2xl font-medium">
        Ajouter une box
      </h1>
      <p className="mt-2 max-w-md text-sm text-muted-foreground">
        Maintenez une touche du clavier de la box pendant son démarrage pour
        ouvrir la fenêtre d&apos;appairage (5 minutes), puis connectez-vous en
        Bluetooth ci-dessous.
      </p>

      {!supported && (
        <div className="mt-6 max-w-md rounded-2xl border border-border bg-card p-4 text-sm text-muted-foreground">
          Ce navigateur ne supporte pas le Web Bluetooth. Utilisez Chrome ou
          Edge sur ordinateur/Android (Safari iOS n&apos;est pas compatible).
        </div>
      )}

      {error && (
        <div className="mt-6 max-w-md rounded-2xl border border-red-900/60 bg-red-950/30 p-4 text-sm text-red-300">
          {error}
        </div>
      )}

      <div className="mt-8 max-w-md rounded-2xl border border-border bg-card p-6">
        {step === "idle" && (
          <button className={button} onClick={connect} disabled={!supported}>
            Rechercher ma box
          </button>
        )}

        {step === "connecting" && (
          <p className="text-sm text-muted-foreground">
            Connexion à la box et vérification de possession…
          </p>
        )}

        {(step === "wifi" || step === "joining" || step === "registering") && (
          <div className="flex flex-col gap-4">
            <div>
              <p className={label}>BOX DÉTECTÉE</p>
              <p className="mt-1 font-mono text-sm">{boxUid}</p>
            </div>
            <div>
              <label className={label} htmlFor="ssid">
                RÉSEAU WIFI (SSID)
              </label>
              <input
                id="ssid"
                className={input}
                value={ssid}
                onChange={(e) => setSsid(e.target.value)}
                maxLength={32}
                autoComplete="off"
              />
            </div>
            <div>
              <label className={label} htmlFor="pass">
                MOT DE PASSE
              </label>
              <input
                id="pass"
                className={input}
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                maxLength={63}
                autoComplete="off"
              />
            </div>
            <div>
              <label className={label} htmlFor="name">
                NOM DE LA BOX
              </label>
              <input
                id="name"
                className={input}
                value={boxName}
                onChange={(e) => setBoxName(e.target.value)}
                maxLength={64}
              />
            </div>
            <button
              className={button}
              onClick={sendWifi}
              disabled={!ssid || step !== "wifi"}
            >
              {step === "joining"
                ? "Connexion de la box au WiFi…"
                : step === "registering"
                  ? "Enregistrement…"
                  : "Configurer et enregistrer"}
            </button>
          </div>
        )}

        {step === "done" && (
          <div>
            <p className="text-foreground">
              ✓ <span className="font-mono">{boxUid}</span> est en ligne et
              rattachée à votre compte.
            </p>
            <p className="mt-2 text-sm text-muted-foreground">
              La box synchronise ses scénarios — vous pouvez fermer cette page.
            </p>
          </div>
        )}
      </div>
    </main>
  );
}
