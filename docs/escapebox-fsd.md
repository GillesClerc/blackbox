# EscapeBox — Functional Specification Document (FSD)

**Version** : 0.2-draft  
**Date** : Mai 2026  
**Auteur** : Gilles  
**Statut** : Draft — en cours de définition  

---

## Table des matières

1. [System Overview](#1-system-overview)
2. [System Architecture](#2-system-architecture)
   - 2.1 [Logical Architecture](#21-logical-architecture)
   - 2.2 [Hardware / Platform Architecture](#22-hardware--platform-architecture)
   - 2.3 [Software Architecture](#23-software-architecture)
3. [Implementation Phases](#3-implementation-phases)
   - 3.1 [Phase 1 — Proof of Concept](#31-phase-1--proof-of-concept)
   - 3.2 [Phase 2 — Proto PCB + Boîtier](#32-phase-2--proto-pcb--boîtier)
   - 3.3 [Phase 3 — Lancement commercial](#33-phase-3--lancement-commercial)
4. [Functional Requirements](#4-functional-requirements)
5. [Risks, Assumptions & Dependencies](#5-risks-assumptions--dependencies)
6. [Interface Specifications](#6-interface-specifications)
7. [Operational Procedures](#7-operational-procedures)
   - 7.1 [First-Time Setup / Flashing](#71-first-time-setup--flashing)
   - 7.2 [OTA Firmware Update](#72-ota-firmware-update)
   - 7.3 [Normal Operation](#73-normal-operation)
8. [Verification & Validation](#8-verification--validation)
9. [Troubleshooting Guide](#9-troubleshooting-guide)
10. [Appendix](#10-appendix)

---

## 1. System Overview

EscapeBox est un système de jeu d'escape interactif composé de trois sous-systèmes interdépendants :

| Sous-système | Description |
|---|---|
| **Hardware** | Box physique avec capteurs, actionneurs, écrans, ESP32-S3 |
| **Firmware** | Code embarqué sur l'ESP32-S3, moteur de scénario, drivers capteurs |
| **Web Platform** | Webapp Next.js (catalogue, bibliothèque, éditeur B2B, compte joueur) |

Le principe opérationnel est le suivant :

```
Joueur achète scénario (webapp)
        ↓
Scénario ajouté à la bibliothèque (Supabase)
        ↓
Box synchro WiFi (à la demande du joueur)
        ↓
Scénario téléchargé + vérifié sur carte SD
        ↓
Box fonctionne 100% offline pendant le jeu
        ↓
Scores et stats remontés à la prochaine synchro
```

**Contraintes non-négociables :**
- La box doit fonctionner **sans Internet pendant le jeu**
- Chaque scénario est **signé cryptographiquement** (protection contre la copie)
- L'expérience du premier déballage doit être possible **sans aucun setup** (scénario pré-chargé en usine)

---

## 2. System Architecture

### 2.1 Logical Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                         JOUEUR / CLIENT                             │
└──────────┬──────────────────────────────────┬───────────────────────┘
           │  Webapp (navigateur)             │  Box physique
           ▼                                  ▼
┌──────────────────────┐          ┌───────────────────────────────────┐
│   WEB PLATFORM        │          │           HARDWARE BOX            │
│                      │          │                                   │
│  ┌────────────────┐  │          │  ┌─────────────────────────────┐  │
│  │  Catalogue /   │  │          │  │     Firmware ESP32-S3       │  │
│  │  Boutique      │  │  HTTPS   │  │                             │  │
│  │  Bibliothèque  │◄─┼──────────┼─►│  Moteur scénario (YAML)    │  │
│  │  Éditeur B2B   │  │  WiFi    │  │  Drivers capteurs           │  │
│  │  Compte joueur │  │  (sync   │  │  Gestion audio + vidéo      │  │
│  └────────────────┘  │  only)   │  │  Gestion OTA                │  │
│                      │          │  └─────────────────────────────┘  │
│  ┌────────────────┐  │          │                                   │
│  │  Supabase      │  │          │  ┌──────────────┐  ┌───────────┐  │
│  │  (DB + Auth)   │  │          │  │  Carte SD    │  │  Batterie │  │
│  └────────────────┘  │          │  │  (scénarios) │  │  LiPo     │  │
│                      │          │  └──────────────┘  └───────────┘  │
│  ┌────────────────┐  │          └───────────────────────────────────┘
│  │  Cloudflare R2 │  │
│  │  (CDN assets)  │  │
│  └────────────────┘  │
│                      │
│  ┌────────────────┐  │
│  │  Stripe        │  │
│  │  (paiements)   │  │
│  └────────────────┘  │
└──────────────────────┘
```

### 2.2 Hardware / Platform Architecture

#### 2.2.1 SoC principal

**ESP32-S3-DevKitC-1-N8R8** (proto) → **ESP32-S3-WROOM-1-N16R8** (série)

#### 2.2.2 Capteurs et actionneurs

**Bus backbone interne** (JST, court le long de la structure de la box) :

Chaque PCB satellite ou composant se plug/déplugg via connecteur JST sur ce bus backbone. Il transporte : alimentation 3.3V + 5V, I2C, et signaux GPIO nécessaires. Permet un assemblage/démontage propre sans soudure volante.

**Bus I2C** (SDA=GPIO8, SCL=GPIO9, pull-up 4.7kΩ) — distribué via backbone JST :

| Adresse | Composant | Fonction |
|---|---|---|
| 0x10 | VEML7700 | Lumière ambiante |
| 0x24 | PN532 | Lecteur NFC |
| 0x36 | AS5600 | Rotation magnétique (plateau) |
| 0x5A | MPR121 (Phase 1 proto) | Capacitif 12 canaux — breakout AliExpress |
| 0x28 | MTCH2120 (Phase 2 PCB) | Capacitif 12 canaux — directement sur PCB custom |
| 0x5C | MLX90614 | Température IR |
| 0x6A | LSM6DSOTR | Accéléromètre + gyroscope 6 axes (STMicroelectronics) |
| 0x76 | BMP280 | Pression / détection souffle |

**Bus SPI2** (écran principal, GPIO35-40) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO35 | MOSI | TFT 4" ILI9488 |
| GPIO36 | CLK | TFT 4" ILI9488 |
| GPIO37 | CS | TFT 4" ILI9488 |
| GPIO38 | DC | TFT 4" ILI9488 |
| GPIO39 | RST | TFT 4" ILI9488 |
| GPIO40 | BL | TFT 4" backlight |

**Bus SPI3** (écran boussole, GPIO41-45) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO41 | MOSI | GC9A01 1.3" rond |
| GPIO42 | CLK | GC9A01 1.3" rond |
| GPIO43 | CS | GC9A01 1.3" rond |
| GPIO44 | DC | GC9A01 1.3" rond |
| GPIO45 | RST | GC9A01 1.3" rond |

**Bus I2S0** (audio sortie, GPIO5-7) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO5 | BCLK | PCM5122PW (I2S DAC stéréo) |
| GPIO6 | LRCLK | PCM5122PW |
| GPIO7 | DIN | PCM5122PW |

> PCM5122PW génère son propre clock maître (pas de MCLK externe nécessaire). Il est aussi contrôlé via I2C à l'adresse **0x4C** (volume, EQ DSP, mute). Sa sortie analogique LOUT/ROUT alimente le PAM8406 via condensateurs de couplage 100nF. Le **PAM8406** (Class D 5W+5W stéréo) ne nécessite aucun driver logiciel — purement analogique, piloté par SHDN pin.

**Bus I2S1** (audio entrée, GPIO15-17) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO15 | SCK | ICS-43434 micro MEMS |
| GPIO16 | WS | ICS-43434 |
| GPIO17 | SD | ICS-43434 |

**GPIO individuels** :

| GPIO | Fonction | Composant |
|---|---|---|
| GPIO1 | PWM Servo 1 | SG90 — compartiment principal |
| GPIO2 | PWM Servo 2 | SG90 — compartiment secondaire |
| GPIO3 | Laser ON/OFF | 2N7002 MOSFET |
| GPIO4 | WS2812 DATA | Chaîne LEDs RGB (arêtes + anneau NFC) |
| GPIO10 | Hall sensor | A3144E (détection aimant) |
| GPIO11-13 | Touch natif ESP32-S3 | Réservé — à confirmer selon besoins MTCH2120 |
| GPIO14 | Encoder A | Rotary encoder EC11 |
| GPIO18 | Encoder B | Rotary encoder EC11 |
| GPIO21 | Encoder BTN | Bouton rotary encoder |
| GPIO19 | USB D- | USB-C natif |
| GPIO20 | USB D+ | USB-C natif |
| GPIO48 | LED status | WS2812 onboard DevKit |

**Alimentation** :

```
USB-C (5V in)
    ↓
TP4056 + DW01A + FS8205 (charge LiPo + protection)
    ↓
LiPo 3.7V 3000mAh
    ↓
    ├── AP2112K-3.3 → 3.3V (ESP32 + capteurs I2C + écrans + micro)
    └── MT3608 boost → 5V (WS2812 + servos + laser)
```

#### 2.2.2b Assignation des faces — Cube Phase 1 (120×120×120mm)

| Face | Rôle | Composants |
|---|---|---|
| **Avant** | Écran narratif principal | ILI9488 4" SPI (GPIO35-40), WS2812 border |
| **Droite** | Keypad capacitif | MPR121 0x5A (proto) / MTCH2120 0x28 (série), WS2812 rétro |
| **Gauche** | Zone NFC | PN532 0x24 (antenne derrière bois ≤3mm), WS2812 anneau |
| **Haut** | Capteurs ambiance | VEML7700 0x10, BMP280 0x76 (souffle), LSM6DSOTR 0x6A, Hall A3144E |
| **Dos** | Face révélation victoire | GC9A01 1.3" round (GPIO41-45), WS2812 celebration border |
| **Dessous** | Technique | USB-C, micro SD, interrupteur, AS5600 plateau rotatif (Phase 2 Pro) |

**Mécanique de révélation Phase 1 (sans servo) :**
La face dos reste neutre pendant la partie. À la victoire :
1. WS2812 border (face dos) s'allume en animation celebration
2. GC9A01 affiche le QR code → `escapebox.ch/score/{session_token}`
3. WS2812 toute la box en animation globale
4. Audio piste victoire

**Servo Phase 2 :** réintroduit optionnellement pour les scénarios avec pack physique.
Déclaré via `hardware_required: [servo_main]` dans le YAML du scénario. Driver MCPWM déjà écrit.

**Flux de révélation complet (tous appareils) :**
```
Joueur résout l'énigme → entre le code sur le keypad
        ↓
Servo (Phase 2) OU face dos s'active (Phase 1)
        ↓
GC9A01 affiche QR → escapebox.ch/obj/{scenario}/{session}
        ↓
Téléphone reçoit l'objet digital (image, fragment, symbole)
        ↓
L'objet affiche un CODE → joueur le tape sur le keypad
        ↓
Prochaine énigme débloquée
```

> **Note NFC téléphone :** L'interaction téléphone → PN532 (téléphone agit comme tag) est **impossible sur iPhone** (limitation hardware iOS 18.1 inclus, HCE restreint aux paiements). Le flux ci-dessus (QR → objet → code clavier) fonctionne sur tous les appareils. Une variante Android-only (HCE → tap PN532) pourrait être explorée en Phase 3 (COULD).

#### 2.2.3 PCB

**Proto Phase 1** : ESP32-S3-DevKitC-1 sur headers femelles + PCB capteurs.

**Proto Phase 2** : Module WROOM-1-N16R8 soudé directement sur PCB custom tout-en-un, PCBA par JLCPCB.

**Architecture PCB interne** :

Un bus backbone JST court le long de la structure interne de la box. Chaque PCB satellite (face keypad, face NFC, face boussole, etc.) et chaque composant se plug/déplugg proprement sur ce bus via connecteur JST. Le backbone distribue :
- Alimentation 3.3V et 5V
- Bus I2C partagé
- Signaux GPIO nécessaires par face

Avantages : assemblage sans soudure volante, remplacement/upgrade d'une face sans toucher au reste, câblage propre et maintenable en production.

**Boîtier** :
- Phase 1 : MDF découpé laser + peinture noire + détails laiton minimal (Lite)
- Phase 2 : Bois massif (noyer/chêne) + ardoise composite + laiton brossé + plateau rotatif avec roulement à billes 100-150mm + AS5600 + aimant néodyme diamétral(Pro)

### 2.3 Software Architecture

#### 2.3.1 Firmware (ESP32-S3, ESP-IDF v6.1)

```
┌──────────────────────────────────────────────────────────────────┐
│                    FIRMWARE ESP32-S3                              │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Core 0 (réseau + audio)               │    │
│  │                                                          │    │
│  │  wifi_manager  (BLE provisioning + sync)                 │    │
│  │  ota_manager   (esp_https_ota)                           │    │
│  │  audio_player  (I2S DMA, MP3 via ESP-ADF)               │    │
│  │  audio_capture (I2S micro, analyse niveau/rythme)        │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Core 1 (UI + jeu)                     │    │
│  │                                                          │    │
│  │  scenario_engine (state machine, parse YAML)             │    │
│  │  sensor_manager  (polling I2C 50ms, event queue)         │    │
│  │  display_manager (LVGL, TFT 4", GC9A01 1.3")            │    │
│  │  led_manager     (WS2812 via RMT ESP-IDF)               │    │
│  │  servo_manager   (MCPWM ESP-IDF)                         │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Shared (FreeRTOS)                     │    │
│  │                                                          │    │
│  │  event_queue    (xQueue entre cores)                     │    │
│  │  storage_manager (SD card SDMMC, NVS flash)              │    │
│  │  crypto_manager  (ECDSA vérif via mbedTLS intégré)       │    │
│  │  config_manager  (WiFi creds, box ID, préférences)       │    │
│  └─────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────┘
```

**Composants / librairies :**

| Composant | Usage |
|---|---|
| ESP-IDF v6.1 | Framework de base (FreeRTOS, drivers, HAL) |
| LVGL | Interface graphique écrans (porté ESP-IDF natif) |
| esp_lcd | Drivers TFT bas niveau (SPI, ILI9488, GC9A01) |
| led_strip via RMT | WS2812 LEDs (driver natif ESP-IDF) |
| MCPWM | Servos (driver natif ESP-IDF) |
| cJSON | Parsing config / API (inclus ESP-IDF) |
| ESP-ADF | Playback MP3/WAV via I2S DMA |
| i2c_master (PCM5122) | Config DAC : volume, EQ DSP, mute via registres I2C (addr 0x4C) |
| i2c_master | MTCH2120 keypad, PN532 NFC, LSM6DSOTR, AS5600 |
| NimBLE (ESP-IDF) | Provisioning WiFi via BLE |
| esp_https_ota | OTA HTTPS |
| esp_http_client | Download scénarios HTTPS |
| mbedTLS (intégré) | Vérification ECDSA |

#### 2.3.2 Format des scénarios (YAML)

Chaque scénario est un fichier YAML décrivant une machine à états finis (state machine). Le firmware parse ce fichier au démarrage de la partie et l'exécute sans aucune logique codée en dur.

**Structure de base :**

Exemple:

```yaml
meta:
  id: capitaine_verdier_v1
  title: "Le Trésor du Capitaine Verdier"
  author: "EscapeBox Studio"
  version: 1.0
  language: fr
  duration_min: 60
  difficulty: 3              # 1-5
  players: { min: 2, max: 5 }
  hardware_required:
    - screen_main
    - screen_compass
    - rfid
    - keypad
    - servo_main
    - audio
  hardware_enhanced:
    - rotation_base
    - breath
    - accelerometer

assets:
  audio:
    - id: intro
      file: verdier_intro.mp3
      duration_sec: 45
    - id: victoire
      file: verdier_victoire.mp3
  images:
    - id: carte_tresor
      file: carte.png
  videos:
    - id: video_test.mp4
      file: video_test.mp4

variables:
  attempts_keypad: 0
  hall_active: false

steps:
  - id: intro
    type: trigger
    on: rfid_read
    expect: { uid: "04:AB:CD:EF" }
    timeout_sec: 0               # 0 = pas de timeout
    do:
      - audio: { play: intro }
      - screen_main: { image: carte_tresor, text: "ÉPREUVE 1 / 3" }
      - screen_compass: { mode: compass, animate: true }
      - led: { target: edges, color: "#0000FF", mode: pulse }
    next: epreuve_orientation

  - id: epreuve_orientation
    type: input
    on: rotary_value
    expect: { value: 250, tolerance: 5 }
    timeout_sec: 600
    hints:
      - after_sec: 120
        do:
          - audio: { play: hint_azimut }
      - after_sec: 300
        do:
          - screen_main: { text: "Cap vers les Caraïbes... 250°" }
    do_success:
      - servo: { id: main, action: open }
      - audio: { play: compartiment_ouvre }
      - led: { target: all, color: "#00FF00", mode: flash, duration_ms: 2000 }
    do_fail_max:
      count: 3
      do:
        - screen_main: { text: "Indice: regardez l'azimut..." }
    next: epreuve_lumiere

  - id: epreuve_lumiere
    type: combo
    require_all:
      - { on: light_pattern, expect: [3, 1, 2, 4], tolerance_ms: 300 }
      - { on: hall_detected, expect: true }
    timeout_sec: 900
    do_success:
      - servo: { id: secondary, action: open }
      - audio: { play: etoiles }
    next: final

  - id: final
    type: input
    on: keypad_code
    expect: { code: "1743", max_attempts: 5 }
    hints:
      - after_attempts: 3
        do:
          - screen_main: { text: "L'année de sa disparition..." }
    do_success:
      - audio: { play: victoire }
      - led: { target: all, color: rainbow, mode: cycle, duration_ms: 5000 }
      - screen_main: { mode: victory, text: "Bravo !" }
      - servo: { id: main, action: open }
    next: epilogue

  - id: epilogue
    type: narrative
    do:
      - audio: { play: epilogue }
      - screen_main: { mode: qr, url: "https://escapebox.ch/v/VERDIER/{BOX_ID}/{TOKEN}" }
    next: end

  - id: end
    type: end
    do:
      - led: { target: all, off: true }
      - screen_main: { mode: menu }
```

**Types de steps supportés :**

| Type | Description |
|---|---|
| `trigger` | Attend un événement (RFID, timer, démarrage) |
| `input` | Attend une valeur précise sur un capteur |
| `combo` | Attend plusieurs capteurs simultanément |
| `narrative` | Joue des médias sans attente d'input |
| `branch` | Branchement conditionnel selon une variable |
| `end` | Fin du scénario |

**Fallback pour hardware manquant :**

```yaml
  - id: epreuve_souffle
    type: input
    on: breath_detected
    fallback:                    # Si BMP280 absent (Lite sans ce capteur)
      type: input
      on: keypad_code
      expect: { code: "1234" }
```

#### 2.3.3 Web Platform (Next.js + Supabase)

**Stack technique :**

| Composant | Technologie |
|---|---|
| Frontend | Next.js 14+ (App Router), Tailwind CSS, shadcn/ui |
| Auth | Supabase Auth (email, OAuth Google/Apple) |
| Database | Supabase PostgreSQL |
| Storage | Supabase Storage (assets légers) + Cloudflare R2 (audio, images scénarios) |
| Paiements | Stripe (one-shot + abonnements) |
| Emails | Resend |
| Éditeur visuel | React Flow (mode Pro) |
| Éditeur code | Monaco Editor (mode Expert YAML) |
| IA | Anthropic API (Claude) — assistance génération scénarios |
| Hébergement | Coolify sur VPS (ou Vercel) |

**Schéma base de données (tables principales) :**

```sql
-- Utilisateurs (géré par Supabase Auth)
users (id, email, created_at)

-- Box associées à un compte
devices (
  id uuid PRIMARY KEY,
  user_id uuid REFERENCES users,
  box_uid text UNIQUE,          -- ID gravé dans eFuse ESP32
  name text,                    -- Nom personnalisé (ex: "Box salon")
  firmware_version text,
  last_sync_at timestamptz,
  created_at timestamptz
)

-- Catalogue scénarios
scenarios (
  id uuid PRIMARY KEY,
  slug text UNIQUE,             -- ex: "capitaine_verdier_v1"
  title text,
  description text,
  difficulty int,               -- 1-5
  duration_min int,
  min_players int,
  max_players int,
  language text,                -- fr, en, de
  hardware_required text[],     -- ["rfid","keypad","servo_main"]
  price_chf numeric,
  published boolean,
  created_by uuid REFERENCES users,
  created_at timestamptz
)

-- Licences (achat d'un scénario par un user)
licenses (
  id uuid PRIMARY KEY,
  user_id uuid REFERENCES users,
  scenario_id uuid REFERENCES scenarios,
  stripe_payment_intent text,
  purchased_at timestamptz
)

-- Mapping licence <-> device (installé sur quelle(s) box)
device_scenarios (
  device_id uuid REFERENCES devices,
  scenario_id uuid REFERENCES scenarios,
  installed_at timestamptz,
  PRIMARY KEY (device_id, scenario_id)
)

-- Scores des parties
game_sessions (
  id uuid PRIMARY KEY,
  device_id uuid REFERENCES devices,
  scenario_id uuid REFERENCES scenarios,
  completed boolean,
  duration_sec int,
  hints_used int,
  score int,
  played_at timestamptz
)

-- Abonnements B2B
subscriptions (
  id uuid PRIMARY KEY,
  user_id uuid REFERENCES users,
  stripe_subscription_id text,
  plan text,                    -- maker, pro, studio
  status text,
  current_period_end timestamptz
)
```

**API endpoints firmware → serveur :**

```
POST /api/box/auth
  Body: { box_uid, hmac_challenge, hmac_response }
  Returns: { token: jwt }

GET /api/box/sync
  Headers: Authorization: Bearer {token}
  Returns: {
    scenarios_available: [{ id, slug, version, download_url, signature }],
    firmware_latest: { version, url, sha256 },
    timestamp: unix
  }

POST /api/box/session
  Headers: Authorization: Bearer {token}
  Body: { scenario_id, duration_sec, hints_used, completed, score }
  Returns: { ok: true }
```

**Flux d'activation d'un QR code de scénario (achat physique) :**

```
1. QR code imprimé sur carte physique (pack scénario)
2. Joueur scanne → https://escapebox.ch/activate/{TOKEN}
3. Si connecté : scénario ajouté à la bibliothèque immédiatement
4. Si pas connecté : invite à créer un compte, puis ajout
5. Prochaine synchro box : scénario disponible
```

---

## 3. Implementation Phases

### 3.1 Phase 1 — Proof of Concept

**Durée estimée :** 1 mois 
**Objectif :** Valider que le hardware fonctionne et que les gens veulent jouer

**Hardware :**
- [ ] ESP32-S3-DevKitC-1 + Joy-iT TFT 1.8" → valider affichage
- [ ] Cabler microphone MEMS i2s -> valider le micro
- [ ] Câbler PN532 breakout → valider NFC
- [ ] Câbler MTCH2120 → valider keypad capacitif
- [ ] Câbler servo SG90 → valider compartiment
- [ ] Câbler PCM5122PW (I2S + I2C 0x4C) + PAM8406 + 2× speakers 3W 4Ω → valider audio MP3 stéréo
- [ ] Valider contrôle volume logiciel via I2C (fade-in/out propre)
- [ ] Tester EQ DSP PCM5122 sur un scénario (pas de distorsion)
- [ ] Câbler LSM6DSOTR, BMP280, VEML7700, AS5600 → valider I2C bus complet
- [ ] Câbler WS2812 → valider LEDs
- [ ] Tester le moteur de scénario YAML sur le hardware assemblé

**Firmware :**
- [x] Moteur de scénario JSON (state machine, hints, variables, branches, do_fail)
- [x] Scénario "Capitaine Verdier" — YAML + JSON, 3 énigmes (boussole, code, inclinaison)
- [x] Driver audio PCM5122PW (I2S + I2C config, tone generator)
- [x] Driver LEDs WS2812B (RMT, GRB, show)
- [x] Drivers I2C (LSM6DSOTR, AS5600, VEML7700, MTCH2120, MPR121)
- [x] Outil YAML→JSON (tools/yaml2json.py avec validation)
- [ ] Driver LVGL sur TFT 1.8" (puis 4")
- [ ] Driver audio MP3 playback (ESP-ADF ou SPIFFS + raw PCM)
- [ ] Driver NFC PN532
- [ ] Driver servos SG90 MCPWM
- [ ] Système de fichiers SD (LittleFS ou SD FAT)

**Web Platform :**
- [ ] Projet Next.js + Supabase initialisé
- [ ] Auth (email + Google)
- [ ] Page catalogue (statique pour commencer)
- [ ] Page bibliothèque (scénarios achetés)
- [ ] Stripe checkout (paiement one-shot)
- [ ] API sync basique (liste des scénarios autorisés)

**Scénario :**
- [ ] Trouver un scénariste
- [ ] Lui fournir les capteurs souhaités 
- [ ] Implémenter l'histoire
- [ ] Faire des illustrations et vidéos
- [ ] Faire les leds d'ambiance

**Box**
- [ ] Faire un prototype et plusieurs itérations
- [ ] Faire des design mockup



**Validation Phase 1 :**
- 10-15 testeurs jouent le scénario de base de bout en bout
- Taux de complétion > 80%
- Feedback qualitatif positif ("je l'achèterais ?")
- Aucun bug bloquant en cours de partie



---

### 3.2 Phase 2 — Proto PCB + Boîtier

**Durée estimée :** 6-12 mois  
**Objectif :** Avoir un produit physique propre prêt pour pré-vente

**Hardware :**
- [ ] Schématique EasyEDA complète (DevKit + tous capteurs)
- [ ] PCB custom V1 commandé chez JLCPCB (5 exemplaires)
- [ ] Boîtier Lite : Fusion 360 → impression 3D → découpe laser MDF
- [ ] Assemblage 30-50 boxes proto
- [ ] Certification CE-RED initiée (3-4 mois, ~10k CHF)
- [ ] Test autonomie batterie
- [ ] Test fiabilité mécanique servos (1000 cycles)

**Firmware :**
- [ ] WiFi provisioning via BLE (app-less, page web)
- [ ] Système de synchro complet (download depuis CDN, vérif signature ECDSA)
- [ ] OTA firmware update (HTTPs)
- [ ] USB Mass Storage mode (backup offline)
- [ ] Mode AP de récupération (si WiFi perdu)
- [ ] Gestion multi-scénarios sur SD
- [ ] 3 scénarios YAML complets avec audio

**Web Platform :**
- [ ] Système de synchro complet côté serveur
- [ ] Génération des fichiers scénario signés (ECDSA) par box
- [ ] Gestion des devices (association, liste, dernière synchro)
- [ ] Webhook Stripe → attribution licence immédiate
- [ ] Dashboard joueur complet (bibliothèque, scores, historique)
- [ ] Pré-commande / liste d'attente

**Scénario :**
- [ ] Faire 2 autres scénarios


**Box**
- [ ] Lancer une série 0 de 50 box

**Validation Phase 2 :**
- 30-50 boxes pré-vendues à des early adopters
- Synchro WiFi fonctionnelle sur réseau domestique standard
- OTA firmware sans intervention physique
- Scénario téléchargé et jouable en < 5 min depuis l'achat



---

### 3.3 Phase 3 — Lancement commercial

**Durée estimée :** 12-24 mois  
**Objectif :** Vente en ligne, boutiques, montée en charge

**Hardware :**
- [ ] PCB V2 : module WROOM-1-N16R8 soudé directement (plus de DevKit)
- [ ] PCBA complet chez JLCPCB (assemblage usine)
- [ ] Boîtier injection plastique ou bois série (sous-traitance)
- [ ] Certification CE-RED finalisée
- [ ] Box Pro (cube rotatif) : conception et certification
- [ ] Packaging premium (boîte carton rigide, velours, QR d'activation)

**Firmware :**
- [ ] LVGL animations avancées
- [ ] Mode mini-jeu web (BLE + browser game)
- [ ] Support multi-langues (FR/DE/EN)
- [ ] Diagnostic en ligne (logs remontés à la synchro)
- [ ] Mode kiosque B2B (sans compte, scénario pré-chargé, reset automatique)

**Web Platform :**
- [ ] Éditeur B2B mode Simple (templates)
- [ ] Éditeur B2B mode Pro (React Flow drag-and-drop)
- [ ] Éditeur B2B mode Expert (Monaco Editor YAML)
- [ ] Simulateur scénario dans le navigateur
- [ ] Marketplace (publication + vente scénarios tiers)
- [ ] App mobile native iOS/Android (si demande)
- [ ] Multi-langues webapp (FR/DE/EN)
- [ ] Dashboard B2B (gestion flotte de boxes, stats)

**Validation Phase 3 :**
- 200-500 boxes/an vendues
- 5-10 scénarios au catalogue
- 10+ clients B2B (escape rooms, profs, animateurs)
- Marge brute > 50% sur l'ensemble produit + contenu

**Budget Phase 3 :** 60-130k CHF

---

## 4. Functional Requirements

### FR-HW — Hardware

| ID | Exigence | Priorité | Phase |
|---|---|---|---|
| HW-01 | La box doit fonctionner offline pendant une partie complète | MUST | 1 |
| HW-02 | La batterie doit durer au minimum une partie de 90 min sans recharge | MUST | 2 |
| HW-03 | La recharge via USB-C doit fonctionner sans éteindre la box | SHOULD | 2 |
| HW-04 | Le compartiment servo doit s'ouvrir en < 2 secondes | SHOULD | 1 |
| HW-05 | Les deux écrans doivent être rafraîchis à > 20 FPS simultanément | MUST | 1 |
| HW-06 | Le son doit être audible à 2 mètres dans un environnement normal | MUST | 1 |
| HW-07 | Le NFC doit détecter un tag à < 3 cm | MUST | 1 |
| HW-08 | Le clavier capacitif doit fonctionner à travers 3 mm de bois/ardoise | SHOULD | 2 |
| HW-09 | La rotation du plateau doit être mesurée avec précision ≤ 5° | SHOULD | 2 |
| HW-10 | La box doit démarrer (boot complet) en < 5 secondes | SHOULD | 2 |
| HW-11 | La box doit résister à une utilisation de 1000 parties (fiabilité) | MUST | 3 |

### FR-FW — Firmware

| ID | Exigence | Priorité | Phase |
|---|---|---|---|
| FW-01 | Le moteur de scénario doit parser et exécuter un fichier YAML sans recompilation | MUST | 1 |
| FW-02 | La signature ECDSA de chaque scénario doit être vérifiée avant exécution | MUST | 1 |
| FW-03 | Le fallback matériel (capteur absent) doit être géré gracieusement | MUST | 1 |
| FW-04 | Le firmware doit supporter la mise à jour OTA via WiFi | MUST | 2 |
| FW-05 | La synchro WiFi ne doit pas durer plus de 3 minutes (scénario de 50 MB) | SHOULD | 2 |
| FW-06 | Le provisioning WiFi doit être possible via BLE sans app native | MUST | 2 |
| FW-07 | La box doit pouvoir stocker ≥ 5 scénarios simultanément sur la SD | SHOULD | 2 |
| FW-08 | Un mode USB Mass Storage doit permettre le chargement offline de scénarios | COULD | 2 |
| FW-09 | Les scores et stats de partie doivent être sauvegardés localement et remontés à la synchro | SHOULD | 2 |
| FW-10 | Le firmware doit détecter et reporter les erreurs capteur sans crasher | MUST | 2 |
| FW-11 | Le mode deep sleep doit être activé après 30 min d'inactivité | SHOULD | 3 |

### FR-WEB — Web Platform

| ID | Exigence | Priorité | Phase |
|---|---|---|---|
| WB-01 | Un utilisateur doit pouvoir créer un compte et associer une box en < 10 min | MUST | 1 |
| WB-02 | Un scénario acheté doit être disponible à la synchro suivante | MUST | 1 |
| WB-03 | Le paiement Stripe doit déclencher l'attribution de licence immédiatement (webhook) | MUST | 1 |
| WB-04 | Un compte peut gérer jusqu'à 3 box simultanément | MUST | 2 |
| WB-05 | Les fichiers scénario servis aux box doivent être signés ECDSA côté serveur | MUST | 2 |
| WB-06 | L'éditeur B2B doit permettre de créer un scénario simple en < 1 heure | SHOULD | 3 |
| WB-07 | Le simulateur doit reproduire fidèlement le comportement du firmware | SHOULD | 3 |
| WB-08 | La marketplace doit gérer les reversements aux créateurs (Stripe Connect) | COULD | 3 |
| WB-09 | La webapp doit être disponible en FR, DE et EN | SHOULD | 3 |
| WB-10 | Le dashboard B2B doit afficher l'état en temps réel des boxes d'une flotte | COULD | 3 |

---

## 5. Risks, Assumptions & Dependencies

### Risks

| ID | Risque | Impact | Probabilité | Mitigation |
|---|---|---|---|---|
| R-01 | Certification CE-RED refusée ou retardée | Bloquant pour vente EU | Moyen | Anticiper 6 mois, budget 15k CHF, pré-compliance dès la phase 2 |
| R-02 | Servos qui tombent en panne après 500 cycles | SAV + mauvaises reviews | Moyen | Utiliser MG90S (métal), tester 2000 cycles, stock pièces de rechange |
| R-03 | Firmware OTA brique une box (brick) | SAV exceptionnel | Faible | Double partition OTA (rollback automatique si boot fail), mode recovery USB |
| R-04 | Scénarios piratés et distribués | Perte de revenus | Moyen | ECDSA signature, accepter 1-5% de piratage comme coût marketing |
| R-05 | Dépendance au cloud (serveur down) | Synchro impossible | Moyen | Scénarios déjà téléchargés jouables offline, mode dégradé local |
| R-06 | Manque de contenu (pas assez de scénarios) | Abandon du produit après 1-2 parties | Fort | Roadmap contenu en parallèle hardware, IA pour accélérer production |
| R-07 | PSRAM incompatibilité avec librairies | Bugs affichage/audio | Faible | Tester dès Phase 1 avec N8R8, documenter les contraintes |
| R-08 | WiFi instable (réseau domestique varié) | Synchro échoue | Moyen | Retry automatique, timeout généreux, feedback clair à l'utilisateur |
| R-09 | Prix de vente trop élevé pour le marché | Ventes insuffisantes | Moyen | Valider la willingness-to-pay avec 50 early adopters avant la série |
| R-10 | Bois / ardoise : variabilité matériau | Qualité inconstante | Faible | Fournisseur local certifié, gabarits précis, contrôle qualité entrée |
| R-11 | iPhone ne peut pas émuler un tag NFC ISO14443A | Interaction téléphone→PN532 impossible sur iOS | Certain | Conception du flux de révélation sans NFC téléphone (voir §2.2.2b). Interaction téléphone→box via code clavier uniquement. NFC téléphone→box réservé Android si implémenté (COULD, Phase 3). |

### Assumptions

- L'ESP32-S3-WROOM-1-N16R8 reste disponible et au même prix (ou moins cher) sur 3 ans
- JLCPCB continue à supporter l'assemblage PCBA pour de petites séries (50-500 pièces)
- Supabase reste dans son modèle de pricing actuel pour les petits projets
- Les joueurs ont accès à un réseau WiFi domestique pour la synchro (pas de 4G requise)
- Le format YAML choisi est suffisamment expressif pour couvrir 90% des énigmes imaginables
- Les testeurs Phase 1 sont représentatifs du marché cible final
- **iPhone ne peut pas émuler un tag NFC** lisible par le PN532 (limitation hardware/OS confirmée, iOS 18.1 inclus — HCE restreint aux paiements/badges). Le flux compartiment → QR → code clavier est conçu pour fonctionner sur tous les appareils.

### Dependencies

| Dépendance | Type | Impact si défaillante |
|---|---|---|
| Supabase | Cloud (DB + Auth) | Authentification impossible, données perdues |
| Cloudflare R2 | Cloud (CDN) | Téléchargement scénarios impossible |
| Stripe | Cloud (paiements) | Ventes impossibles |
| JLCPCB / LCSC | Fabrication | Production bloquée |
| Espressif ESP32-S3 | Composant | Redesign MCU nécessaire |
| ESP-IDF | Framework | Migration vers autre SDK |
| LVGL | Librairie | Redesign affichage |
| Anthropic API | IA (éditeur B2B) | Fonctionnalité IA dégradée (non bloquant) |

---

## 6. Interface Specifications

### 6.1 Interface Box ↔ Serveur (REST API)

**Base URL :** `https://api.escapebox.ch/v1`

**Authentification :** Chaque box s'authentifie par HMAC-SHA256 challenge-response basé sur son UID unique gravé dans les eFuses ESP32.

```
POST /box/auth
Request:
  {
    "box_uid": "ESP32S3-XXXX-XXXX",
    "challenge_response": "hmac_sha256(challenge, secret_key)"
  }
Response 200:
  {
    "token": "eyJ...",        // JWT valide 24h
    "server_time": 1715000000
  }
Response 401:
  { "error": "invalid_credentials" }
```

```
GET /box/sync
Headers:
  Authorization: Bearer {token}
Response 200:
  {
    "scenarios": [
      {
        "id": "uuid",
        "slug": "capitaine_verdier_v1",
        "version": "1.0",
        "size_bytes": 47200000,
        "download_url": "https://cdn.escapebox.ch/scenarios/verdier_v1_{box_uid}.enc",
        "signature": "base64_ecdsa_signature",
        "checksum_sha256": "abc123..."
      }
    ],
    "firmware": {
      "version": "1.2.3",
      "url": "https://cdn.escapebox.ch/firmware/v1.2.3.bin",
      "sha256": "def456..."
    },
    "timestamp": 1715000000
  }
```

```
POST /box/session
Headers:
  Authorization: Bearer {token}
Request:
  {
    "scenario_id": "uuid",
    "started_at": 1715000000,
    "duration_sec": 3547,
    "completed": true,
    "hints_used": 2,
    "score": 850
  }
Response 200:
  { "ok": true, "session_id": "uuid" }
```

### 6.2 Interface Box ↔ App/Webapp (BLE Provisioning)

Utilisé uniquement lors de la première configuration WiFi.

```
Service UUID: 4FAFC201-1FB5-459E-8FCC-C5C9C331914B
Characteristics:
  - WIFI_SSID    (write): UUID 6E400002-...  Max 64 chars
  - WIFI_PASS    (write): UUID 6E400003-...  Max 64 chars
  - BOX_CODE     (read) : UUID 6E400004-...  6-char pairing code
  - STATUS       (notify): UUID 6E400005-... "connecting|connected|error"
```

**Flux :**
1. Box en mode provisioning → émet BLE avec code 6 chars sur l'écran
2. Webapp scanne les BLE devices → trouve le bon (nom = "EscapeBox-XXXX")
3. Webapp écrit SSID et mot de passe
4. Box tente la connexion WiFi → notifie le statut
5. Si succès → provisioning terminé, BLE éteint

### 6.3 Interface Utilisateur — Webapp

**Navigation principale :**

```
/                       → Landing page / marketing
/auth/login             → Connexion
/auth/register          → Inscription
/app/library            → Ma bibliothèque (scénarios achetés)
/app/shop               → Catalogue scénarios
/app/shop/{slug}        → Détail scénario
/app/devices            → Mes box (liste, synchro, stats)
/app/devices/add        → Associer une nouvelle box
/app/scores             → Historique des parties
/app/account            → Mon compte, abonnement
/studio                 → Éditeur B2B (plan Pro+)
/studio/new             → Nouveau scénario
/studio/{id}/edit       → Éditer un scénario existant
/studio/{id}/simulate   → Simulateur
/marketplace            → Scénarios communautaires (Phase 3)
/activate/{token}       → Activation QR code physique
```

### 6.4 Interface Utilisateur — Box (Menu on-device)

**Écran principal — États :**

```
BOOT          → Logo EscapeBox + animation (< 5s)
MENU          → Liste des scénarios installés (scroll rotary)
PLAYING       → Affichage scénario en cours
SYNC          → Barre de progression download
SETTINGS      → WiFi, langue, version firmware
CHARGING      → Indicateur de charge (si batterie faible)
ERROR         → Message d'erreur + code QR support
```

**Navigation menu :**
- Rotary encoder : scroll dans les listes
- Bouton rotary : sélection / confirmation
- Appui long bouton : retour / annuler
- Double appui : raccourci synchro

---

## 7. Operational Procedures

### 7.1 First-Time Setup / Flashing

#### 7.1.1 Prérequis développement

```bash
# Outils requis
- ESP-IDF v6.1 (installé via install.sh ou Docker espressif/idf)
- Python 3.8+
- Git
- Driver CP2104 ou CH340 (selon le board)
- VS Code + extension Espressif IDF (optionnel)
```

#### 7.1.2 Cloner le repository

```bash
git clone git@github.com:GillesClerc/blackbox.git
cd blackbox
source /opt/esp/idf/export.sh   # ou via le container Docker
```

**Structure du repository :**

```
blackbox/
├── firmware/
│   └── main/
│       ├── main.c               # Point d'entrée app_main()
│       └── CMakeLists.txt
├── components/
│   ├── scenario/                # Moteur YAML + state machine
│   ├── nfc/                     # Driver PN532
│   ├── keypad/                  # Driver MTCH2120
│   ├── sensors/                 # Drivers I2C : LSM6DSOTR, AS5600, VEML7700, MTCH2120
│   ├── display/                 # LVGL + ILI9488 + GC9A01
│   ├── audio/                   # I2S DMA + PCM5122PW (DAC) + PAM8406 (amp)
│   ├── led/                     # WS2812 via RMT
│   ├── servo/                   # SG90 via MCPWM
│   ├── storage/                 # SD card SDMMC + NVS
│   ├── wifi_manager/            # Sync + BLE provisioning
│   ├── ota_manager/             # esp_https_ota
│   └── crypto/                  # Vérification ECDSA
├── docs/                        # Vision, FSD
├── CMakeLists.txt               # Root CMake
├── sdkconfig.defaults           # Config ESP-IDF par défaut
├── partitions.csv               # Partition table custom
└── CLAUDE.md
```

#### 7.1.3 Configuration ESP-IDF

```cmake
# CMakeLists.txt (racine)
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(blackbox)
```

```
# sdkconfig.defaults
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_LOG_DEFAULT_LEVEL_INFO=y
```

**Partition table custom :**

```csv
# partitions.csv
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,   0x5000
otadata,  data, ota,     0xe000,   0x2000
app0,     app,  ota_0,   0x10000,  0x300000
app1,     app,  ota_1,   0x310000, 0x300000
storage,  data, fat,     0x610000, 0x1F0000
```

#### 7.1.4 Premier flash (USB)

```bash
# Configurer la cible (une seule fois)
idf.py set-target esp32s3

# Compiler
idf.py build

# Flasher
idf.py -p /dev/ttyUSB0 flash

# Monitor série
idf.py -p /dev/ttyUSB0 monitor

# Flash + monitor en une commande
idf.py -p /dev/ttyUSB0 flash monitor
```

**Logs attendus au premier démarrage :**

```
[BOOT] EscapeBox Firmware v0.1.0
[BOOT] Box UID: ESP32S3-ABCD-1234
[CONFIG] No WiFi config found - entering provisioning mode
[BLE] Advertising as: EscapeBox-1234
[BLE] Pairing code: X7K2PQ
[DISPLAY] LVGL initialized - 800x480
[DISPLAY] Compass display initialized - 240x240
[STORAGE] SD card: 8.00 GB
[STORAGE] LittleFS: 1.75 MB free
[SCENARIO] Found 1 scenario on SD: capitaine_verdier_v1
[AUDIO] I2S initialized - Speaker + Mic
[SENSORS] I2C scan: 0x10 0x24 0x36 0x5A 0x5B 0x5C 0x68 0x76 - OK
[BOOT] Ready
```

#### 7.1.5 Configuration initiale WiFi (via webapp)

1. Aller sur `https://escapebox.ch/app/devices/add`
2. Cliquer "Associer ma box"
3. La webapp utilise la Web Bluetooth API pour scanner les devices BLE
4. Sélectionner "EscapeBox-XXXX" dans la liste
5. Entrer le code affiché sur l'écran de la box pour confirmer
6. Sélectionner le réseau WiFi et entrer le mot de passe
7. La box se connecte → confirmation sur l'écran + dans la webapp

> **Note :** La Web Bluetooth API nécessite Chrome ou Edge sur desktop. Sur mobile, utiliser l'app native (Phase 3) ou la procédure manuelle (AP mode).

**Procédure manuelle AP mode (fallback) :**
```
1. Box en mode provisioning → crée un réseau WiFi "EscapeBox-Setup"
2. Se connecter à ce réseau depuis le téléphone
3. Ouvrir http://192.168.4.1 dans le navigateur
4. Remplir SSID + mot de passe → Valider
5. Box redémarre et se connecte
```

---

### 7.2 OTA Firmware Update

#### 7.2.1 OTA automatique (recommandé en production)

Au démarrage de chaque synchro WiFi, la box interroge le serveur pour connaître la dernière version du firmware. Si une mise à jour est disponible :

```
Flux OTA automatique :
1. GET /api/box/sync → { firmware: { version: "1.2.3", url: "...", sha256: "..." } }
2. Box compare avec sa version actuelle
3. Si version_serveur > version_locale :
   a. Afficher sur écran : "Mise à jour disponible (v1.2.3) - Installation..."
   b. Download depuis CDN vers la partition OTA inactive (app1 si app0 active)
   c. Vérifier SHA256 du fichier téléchargé
   d. esp_ota_set_boot_partition() → pointer vers la nouvelle partition
   e. Redémarrer
   f. Au boot : vérifier que le nouveau firmware démarre (watchdog 30s)
   g. Si OK → valider (esp_ota_mark_app_valid_cancel_rollback())
   h. Si KO → rollback automatique vers la partition précédente
```

#### 7.2.2 OTA forcée via WiFi local (développement)

```bash
# Générer le binaire
idf.py build

# Flasher via réseau (la box doit être en mode OTA local)
espota.py -i 192.168.1.xxx -p 3232 -f build/blackbox.bin
```

La box doit être en mode OTA local (menu Settings → OTA local → ON).

#### 7.2.3 Rollback manuel

Si la box ne démarre plus après une OTA :
```
1. Brancher USB-C sur PC
2. Ouvrir le Serial Monitor
3. Observer les logs de boot → identifier l'erreur
4. Si brick complet : reflash manuel via USB (section 7.1.4)
```

---

### 7.3 Normal Operation

#### 7.3.1 Démarrage d'une partie

```
1. Allumer la box (bouton ON/OFF ou sortir du deep sleep)
2. Menu principal → liste des scénarios
3. Rotary encoder pour naviguer → appui pour sélectionner
4. Confirmation : "Commencer Le Trésor du Capitaine Verdier ?"
5. La box charge le YAML depuis la SD → vérifie la signature
6. Écrans : animation de démarrage thématique
7. Audio : narration d'introduction
8. Jeu commence → moteur de scénario prend le contrôle
```

#### 7.3.2 Synchronisation manuelle

```
1. Menu principal → "Synchroniser"
2. Box se connecte au WiFi (credentials stockés dans NVS)
3. Auth avec le serveur (HMAC)
4. Récupère la liste des scénarios autorisés
5. Télécharge les nouveaux scénarios non présents sur la SD
6. Upload les scores des parties récentes
7. Vérifie si firmware update disponible (OTA automatique si oui)
8. Déconnexion WiFi
9. Retour au menu
```

#### 7.3.3 Gestion des indices (hints)

Le moteur de scénario peut déclencher des indices automatiquement :
- Après un délai configurable (ex: 120s sans action)
- Après N tentatives échouées
- Sur demande explicite (bouton secret ou séquence sur le keypad)

Les indices sont gradués : d'abord vague, puis de plus en plus précis.

#### 7.3.4 Fin de partie et scores

```
1. Scénario terminé → animation de victoire (LEDs + audio)
2. Écran affiche : score, durée, nombre d'indices utilisés
3. QR code vers la page de résultat en ligne
4. Score sauvegardé localement (SD + NVS)
5. Sera remonté au serveur à la prochaine synchro
6. Retour au menu après 30 secondes
```

---

## 8. Verification & Validation

### 8.1 Phase 1 Verification — Infrastructure

#### 8.1.1 Tests hardware (checklist)

```
[ ] TFT s'affiche correctement (couleurs, orientation, pas d'artefact)
[ ] Écran rond GC9A01 s'affiche (animation boussole)
[ ] Speaker gauche + droite produisent du son stéréo (MP3 lisible, pas de bruit parasite)
[ ] Contrôle volume I2C fonctionnel (fade-in/fade-out propre)
[ ] EQ DSP PCM5122 testée sur un scénario (pas de distorsion)
[ ] Micro détecte un claquement de mains à 1 mètre
[ ] PN532 lit un tag NTAG213 en < 500ms
[ ] MTCH2120 keypad détecte les 12 touches avec < 1% faux positifs
[ ] AS5600 mesure la rotation avec précision ≤ 2° sur 360°
[ ] LSM6DSOTR détecte une inclinaison de 15° minimum
[ ] BMP280 détecte un souffle buccal à 5 cm
[ ] VEML7700 distingue pièce éclairée / pièce sombre
[ ] MLX90614 mesure la température d'une main à 2 cm
[ ] Servo 1 ouvre / ferme le compartiment en < 2s sans calage
[ ] WS2812 : toute la chaîne répond, couleurs correctes
[ ] Laser : s'allume / s'éteint proprement via MOSFET
[ ] USB-C : programmation ET charge fonctionnels simultanément
[ ] Carte SD : lecture / écriture à > 1 MB/s
[ ] Batterie : tient 90 minutes sous charge normale
[ ] OTA WiFi : mise à jour firmware complète sans intervention physique
```

#### 8.1.2 Tests firmware

```
[ ] Le moteur YAML parse un scénario complet (Verdier) sans erreur
[ ] Toutes les transitions de states s'enchaînent correctement
[ ] Les timeouts déclenchent les hints au bon moment
[ ] Le fallback hardware fonctionne (débrancher un capteur → mode dégradé)
[ ] La signature ECDSA invalide est rejetée (tester avec un fichier modifié)
[ ] La synchro WiFi télécharge et installe un scénario complet
[ ] L'OTA installe une nouvelle version et redémarre sans brick
[ ] Le rollback OTA fonctionne (simuler un boot fail)
```

#### 8.1.3 Tests webapp

```
[ ] Création de compte et connexion (email + Google)
[ ] Association d'une box (BLE provisioning)
[ ] Achat d'un scénario via Stripe (test card 4242...)
[ ] Webhook Stripe → licence créée dans Supabase < 5s
[ ] API sync renvoie la liste correcte des scénarios autorisés
[ ] Fichier scénario généré avec signature ECDSA valide
[ ] Download depuis CDN (Cloudflare R2) < 60s pour 50 MB
[ ] Dashboard affiche la bonne dernière synchro et les bons scénarios
```

#### 8.1.4 Tests intégration end-to-end

```
[ ] Scénario de base joué de bout en bout par 3 personnes
[ ] Taux de complétion mesuré sur 10 groupes testeurs
[ ] Aucun bug bloquant en cours de partie sur 10 parties complètes
[ ] Synchro WiFi après achat : scénario jouable en < 5 minutes
[ ] OTA en condition réelle (réseau WiFi domestique)
```

### 8.2 Phase 2 Verification — Produit

```
[ ] PCB assemblé par JLCPCB : 5 exemplaires testés, < 1 DOA
[ ] Boîtier : fermeture correcte, accès aux capteurs conforme
[ ] Plateau rotatif : rotation fluide, AS5600 opérationnel
[ ] Test vibration : 1000 cycles servo sans défaillance
[ ] Test thermique : 4h de fonctionnement continu sans surchauffe
[ ] Test batterie : 5 cycles de charge/décharge complets
[ ] Certification CE-RED : rapport de pré-conformité validé
```

---

## 9. Troubleshooting Guide

### 9.1 Hardware

| Symptôme | Causes possibles | Solution |
|---|---|---|
| Box ne démarre pas | Batterie déchargée / câble USB charge-only | Charger 30 min / changer câble |
| Écran blanc | SPI mal câblé / CS/DC inversés | Vérifier GPIO 37/38, tester avec sketch minimal |
| Pas de son | PCM5122 XSMT pin flottant / PAM8406 SHDN actif | Vérifier XSMT → 3.3V, SHDN → VDD |
| Son mono uniquement | ROUT non connecté au PAM8406 INR | Vérifier condensateurs de couplage LOUT/ROUT |
| Bruit de fond / hiss | Masse analogique mal séparée | Séparer AGND (PCM5122) du PGND (PAM8406) sur le PCB |
| Volume ne change pas | I2C addr incorrecte / registres mal configurés | Vérifier 0x4C sur bus I2C, relire registres 61 et 62 |
| NFC ne lit pas | Mode I2C non sélectionné sur PN532 (jumper) | Souder le jumper I2C sur le module PN532 |
| Touch erratique | Paroi trop épaisse / mauvais calibrage | Augmenter le pad cuivre / ajuster threshold firmware |
| Servo bloqué | Courant insuffisant (5V rail) / mécanisme coincé | Vérifier MT3608 boost 5V, libérer mécaniquement |
| LEDs éteintes | DATA sans résistance 330Ω / alimentation 5V | Ajouter résistance / vérifier rail 5V |

### 9.2 Firmware

| Symptôme | Causes possibles | Solution |
|---|---|---|
| Boot loop | OTA corrompue / stack overflow | Rollback OTA ou reflash USB |
| YAML parse error | Syntaxe invalide / encodage non-UTF8 | Valider le YAML avec un linter, vérifier encodage |
| Signature rejetée | Fichier modifié / mauvaise clé publique | Régénérer le fichier signé côté serveur |
| WiFi ne connecte pas | Mauvais credentials / réseau 5GHz | Vérifier SSID/pass dans NVS, ESP32 ne supporte que 2.4GHz |
| SD non détectée | Format incorrect / contact SD défaillant | Formater en FAT32, nettoyer les contacts |
| Audio crachotement | Buffer underrun / fréquence incorrecte | Vérifier sample rate (44100 Hz), augmenter buffer |

### 9.3 Web Platform

| Symptôme | Causes possibles | Solution |
|---|---|---|
| Synchro échoue (401) | Token expiré / box_uid non reconnu | Ré-associer la box dans la webapp |
| Scénario non disponible après achat | Webhook Stripe non reçu | Vérifier les logs Stripe, déclencher manuellement |
| Download lent | CDN Cloudflare / bande passante | Normal à 50 MB/50 Mbps = ~8s. Vérifier si R2 répond |
| BLE provisioning échoue | Navigateur non compatible | Utiliser Chrome/Edge desktop, ou procédure AP mode |

---

## 10. Appendix

### 10.1 Références matérielles

| Composant | Référence | Datasheet |
|---|---|---|
| ESP32-S3-WROOM-1-N16R8 | LCSC C2913202 | https://datasheet.lcsc.com/lcsc/2207151200_Espressif-Systems-ESP32-S3-WROOM-1-N16R8_C2913202.pdf |
| PN532 | LCSC C132449 | https://www.nxp.com/docs/en/user-guide/141520.pdf |
| MTCH2120 | LCSC (chercher) | https://ww1.microchip.com/downloads/en/DeviceDoc/MTCH2120-Touch-Sensor-Controller-DS60001337B.pdf |
| AS5600 | LCSC C79815 | https://ams.com/documents/20143/36005/AS5600_DS000365_5-00.pdf |
| VEML7700 | LCSC C1850416 | https://www.vishay.com/docs/84286/veml7700.pdf |
| BMP280 | LCSC C83291 | https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf |
| LSM6DSOTR | LCSC C2678328 | https://www.st.com/resource/en/datasheet/lsm6dso.pdf |
| MLX90614 | LCSC C58661 | https://www.melexis.com/en/documents/documentation/datasheets/datasheet-mlx90614 |
| PCM5122PW | LCSC C14969 | https://www.ti.com/lit/ds/symlink/pcm5122.pdf |
| PAM8406 | LCSC C89689 | https://www.diodes.com/assets/Datasheets/PAM8406.pdf |
| ICS-43434 | LCSC (chercher) | https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf |
| TP4056 | LCSC C382139 | https://datasheet.lcsc.com/lcsc/TOPPOWER-Nanjing-Top-Power-ATEC-TP4056_C382139.pdf |
| MT3608 | LCSC C84817 | https://datasheet.lcsc.com/lcsc/XI-AN-Aerosemi-Tech-MT3608_C84817.pdf |
| AP2112K-3.3 | LCSC C51353 | https://datasheet.lcsc.com/lcsc/DIODES-AP2112K-3.3TRG1_C51353.pdf |
| WS2812B | LCSC C114586 | https://datasheet.lcsc.com/lcsc/Worldsemi-WS2812B_C114586.pdf |
| 2N7002 | LCSC C8545 | https://datasheet.lcsc.com/lcsc/Nexperia-2N7002_C8545.pdf |
| TTP223-BA6 | LCSC C80757 | https://datasheet.lcsc.com/lcsc/1809301523_TONTEK-TTP223-BA6_C80757.pdf |

### 10.2 Liens utiles

| Ressource | URL |
|---|---|
| ESP32-S3 Datasheet | https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf |
| ESP32-S3 DevKitC-1 User Guide | https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html |
| ESP32-S3 DevKitC-1 Schéma (open source) | https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-devkitc-1 |
| LVGL Documentation | https://docs.lvgl.io |
| TFT_eSPI Library | https://github.com/Bodmer/TFT_eSPI |
| FastLED Library | https://fastled.io |
| PlatformIO Documentation | https://docs.platformio.org |
| JLCPCB PCB + PCBA | https://jlcpcb.com |
| EasyEDA Schematic Tool | https://easyeda.com |
| LCSC Components | https://www.lcsc.com |
| ESP Touch Sensor Design Guide | https://github.com/espressif/esp-iot-solution/blob/master/documents/touch_pad_solution/touch_sensor_design_en.md |
| Supabase Documentation | https://supabase.com/docs |
| Stripe Documentation | https://stripe.com/docs |
| Cloudflare R2 Documentation | https://developers.cloudflare.com/r2 |

### 10.3 Glossaire

| Terme | Définition |
|---|---|
| BOM | Bill of Materials — liste de tous les composants avec références et prix |
| CDN | Content Delivery Network — serveur de distribution de fichiers statiques |
| ECDSA | Elliptic Curve Digital Signature Algorithm — signature cryptographique |
| FSD | Functional Specification Document — ce document |
| HMAC | Hash-based Message Authentication Code — authentification par hash |
| I2C | Inter-Integrated Circuit — bus de communication série 2 fils |
| I2S | Inter-IC Sound — interface audio numérique |
| LVGL | Light and Versatile Graphics Library — bibliothèque graphique pour embedded |
| NFC | Near Field Communication — communication sans fil courte portée |
| NVS | Non-Volatile Storage — zone de flash pour stocker des données persistantes |
| OTA | Over-The-Air — mise à jour firmware sans câble |
| PCBA | Printed Circuit Board Assembly — PCB avec composants soudés |
| PWM | Pulse Width Modulation — modulation de largeur d'impulsion (servos) |
| PSRAM | Pseudo-Static RAM — mémoire RAM externe rapide |
| RFID | Radio Frequency Identification — identification par radiofréquence |
| SoC | System on Chip — puce intégrant CPU, mémoire, périphériques |
| SPI | Serial Peripheral Interface — bus de communication série 4 fils |
| YAML | YAML Ain't Markup Language — format de données lisible par l'humain |

---

*FSD v0.2 — Document vivant, mis à jour à chaque fin de phase.*
