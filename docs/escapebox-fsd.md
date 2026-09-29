# EscapeBox — Functional Specification Document (FSD)

**Version** : 0.3-draft  
**Date** : 27 septembre 2026 (création : mai 2026)  
**Auteur** : Gilles  
**Statut** : Draft — en cours de définition  

> **Journal de révision**
> - **0.3 (2026-09-27)** — nettoyage après l'audit complet (`docs/audits/2026-09-27-audit.md`) :
>   alimentation et connectique alignées sur le schéma KiCad (AP2112M, SS34, J2 3 broches,
>   marche/arrêt, J10/J11/J12), objectif J9 à 1,9 A retiré, drivers annoncés « écrits » à tort
>   (TMAG5273, servo) décochés, API §6.1 et BLE §6.2 réécrites d'après le code, commande de
>   flash et structure du dépôt corrigées, servos/AS5600 retirés des exigences, contradiction
>   AGND/PGND levée. **Ce qui reste à faire vit dans `docs/audits/RESTE-A-FAIRE.md`**, pas ici.
> - **0.3 (2026-09-27, suite)** — **MLX90614 retiré du produit** (variante 3 V à 6,6-8,9 $ pièce chez
>   JLCPCB, pilote à écrire, bus I2C bridé à 100 kHz, adresse 0x5A). Côté 3 = TMAG5273
>   + électrode de proximité capacitive (« approche ta main »).
> - **0.3 (2026-09-27, suite)** — **contrôleur tactile de Côté 2 = CAP1298** (Microchip, 8 canaux,
>   SOIC-14, 1,61 $, en stock JLCPCB) à la place du MTCH2120 (aucun stock JLCPCB). Écartés : MPR121
>   (fin de vie), CAP1214 (introuvable), SC12B, deux CAP12xx (adresse fixe 0x28 commune), MCU
>   dédié (ATtiny1616 : second firmware). Répartition : **6 touches + proximité « approche ta
>   main » gardée par SG**, électrodes en cuivre sur le PCB du satellite. « Approche ta main »
>   passe donc sur **Côté 2** ; **Côté 3 ne garde que le TMAG5273** (pose d'un aimant).
> - 0.2 (2026-09-23) — corrections issues des datasheets (audit hardware + DB).
> - 0.1 (mai 2026) — première version.
>
> Les sections marquées **(cible)** décrivent l'intention produit, pas l'implémentation
> actuelle.

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
| **Web Platform** | Webapp Next.js (catalogue, bibliothèque, éditeur B2B, compte joueur, backend) |

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
- Chaque scénario est **signé cryptographiquement** (protection contre la copie) — *(cible : aujourd'hui les packages sont authentifiés au téléchargement et vérifiés par sha256, mais pas encore signés)*
- L'expérience du premier déballage doit être possible **sans aucun setup** (scénario pré-chargé en usine)

### 1.1 Utilisateurs & proposition de valeur

**Personas cibles :**

**Persona A — Famille "Saturday Night"**
- Profil : parents 30-45 ans, enfants 8-14 ans, habitués aux jeux de société
- Contexte d'achat : cadeau Noël/anniversaire, recherche d'activité commune hors écran
- Contexte de jeu : salon, 60-90 min, 3-5 joueurs
- Attentes : facile à démarrer (< 5 min setup), rejouable, pas de compte obligatoire pour la première partie

**Persona B — Couple/groupe "Escape Room fans"**
- Profil : 25-40 ans, déjà clients d'escape rooms, budget loisirs moyen-haut
- Contexte d'achat : alternative domicile, offrir à quelqu'un qui "aime les énigmes"
- Contexte de jeu : soirée entre amis, 2-6 joueurs, attachés à la qualité narrative
- Attentes : histoire immersive, difficulté réelle, résultat partageable

**Proposition de valeur :**
> "L'escape room à domicile, rechargeable en contenu — une expérience physique et narrative qui évolue avec de nouveaux scénarios."

**Positionnement vs. alternatives :**

| Alternative | Limite vs. EscapeBox |
|---|---|
| Escape room réelle | 80-120 CHF/session, hors domicile, non rejouable |
| Jeux de société escape | Pas d'électronique, expérience figée, non rechargeable |
| Jackbox / jeux numériques | Zéro physicalité, pas de manipulation d'objets |
| Kits DIY (Arduino) | Trop technique, pas clé en main |

**Segmentation :** B2C uniquement en Phase 1 et Phase 2. B2B (escape rooms pro, profs, animateurs) : roadmap Phase 3, non prioritaire avant.

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
│  └────────────────┘  │          │  │  (scénarios) │  │  Li-ion   │  │
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

> **Deux couches à ne pas confondre.** Côté web, le *proxy* Next.js (`proxy.ts`, ex-`middleware.ts`) n'intercepte que les requêtes du **navigateur** (auth, redirections) — il ne parle jamais à la box. Côté matériel, la liaison box↔cloud est une **couche de synchronisation device-to-cloud** : la box est un simple client HTTPS qui appelle l'API REST (§6.1) **à la demande** (sync au boot si WiFi + sync manuelle), jamais en connexion permanente. Aucun framework dédié type ESP RainMaker n'est utilisé — le backend e-commerce (Supabase + Stripe) impose un protocole propre.
>
> **État actuel** : les packages de scénario sont servis par l'API Next.js elle-même (`GET /api/box/pkg/<slug>/…`, fichiers dans `web/scenario-packages/`). Cloudflare R2 et Stripe figurent dans le schéma comme **cible**, ils ne sont pas encore branchés.

### 2.2 Hardware / Platform Architecture

#### 2.2.1 SoC principal

**Proto** : ESP32-S3-DevKitC-1 (headers femelles, breadboard)
**PCB custom** : ESP32-S3-WROOM-1-N16R8 soudé directement

> **Contrainte GPIO WROOM-1-N16R8** : GPIO26-37 sont monopolisés par le flash/PSRAM octal SPI et ne sont **pas disponibles**. GPIO19-20 = USB. GPIO43-44 = UART0 debug. GPIO disponibles : 0-18, 21, 38-42, 45-48.

#### 2.2.2 Capteurs et actionneurs — liste confirmée

**Bus I2C** (partagé via backbone JST) :

> Proto DevKitC-1 : SDA=GPIO21, SCL=GPIO17 | PCB cible : même assignation (GPIO21/17 libres sur WROOM-1-N16R8). Pull-up 4.7kΩ sur chaque ligne. Défini dans `i2c_bus.h`.

| Adresse | Composant | Fonction | PCB |
|---|---|---|---|
| 0x10 | VEML7700 | Lumière ambiante | Satellite Devant |
| 0x28 | CAP1298 | Capacitif 8 canaux : 6 touches (CS1-CS4, CS6, CS7) + proximité « approche ta main » (CS8) gardée par SG (CS5) — cible PCB Phase 2. **Adresse fixe 0x28** [CAP1298 §3.2.2] : un seul CAP12xx sur le bus. ALERT# non remontée (Qwiic 4 fils) → scrutation | Satellite Côté 2 |
| 0x5A | MPR121 | Capacitif 12 canaux (breakout Phase 1, même rôle que le CAP1298 ; fin de vie depuis 2019) | Proto breadboard |
| 0x53 / 0x57 | ST25DV04KC-IE6S3 | Tag NFC dynamique — 0x53 mémoire utilisateur/registres dynamiques, 0x57 configuration système (vérifié datasheet) | Satellite Dessus |
| 0x4C | PCM5122PW | DAC audio stéréo (I2C contrôle) | Main |
| 0x6A | LSM6DSOXTR | Accéléromètre + gyroscope 6 axes + MLC | **Main** (soudé, feuille `imu`) |
| 0x35 | TMAG5273 | Hall linéaire 3D (distance/angle aimant) — **variante A1 à commander** (0x35, ±40/80 mT ; B/C/D = 0x22/0x78/0x44) | Satellite Côté 3 |
| 0x76 | BMP280 | Pression / détection souffle — SDO = GND (0x76), **CSB relié directement à VDDIO** (sinon verrouillage en SPI) | Satellite Devant |
| 0x48 | ADS7830 | ADC 8 canaux 8 bits — 4 faders SL1-SL4 + 4 pots RV1-RV4 (ratiométrique, REFIN=3V3) | Face avant |

> **Composants retirés du Phase 1** : AS5600 (rotation magnétique), servos SG90, laser. Remplacés par potentiomètres rotatifs mécaniques + interactions software.

**Bus SPI2** (display bouche, type à définir ultérieurement — pas e-ink — **+ carte microSD**, IOMUX GPIO11/12 pour perf DMA) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO11 | MOSI | Display bouche (TBD) + microSD (CMD) |
| GPIO12 | SCLK | Display bouche (TBD) + microSD — via R13 22 Ω série |
| GPIO15 | MISO | microSD (DAT0) seule |
| GPIO10 | CS | Display bouche (TBD) chip select |
| GPIO47 | CS_SD | microSD chip select (CD/DAT3) |
| GPIO9  | DC | Display bouche (TBD) data/command |
| GPIO8  | RST | Display bouche (TBD) reset |
| GPIO13 | BUSY | Display bouche (TBD) busy (active low, lecture seule) |

> Écran bouche : type à définir ultérieurement (pas e-ink). Utilisé comme "bouche" du personnage, affichage texte mot-à-mot. Driver Espressif à choisir selon le composant retenu.
>
> **microSD** (slot TF-01A, J11, sur la Main) : pull-ups 10 kΩ vers 3V3_D sur CS/DAT3, CMD, DAT0, DAT1 et DAT2 (R15-R19, obligatoires en mode SPI — ESP-IDF `sd_pullup_requirements`). Bus partagé : monter la SD avant tout échange avec l'écran bouche, CS de l'écran maintenu haut (ESP-IDF `sdspi_share`).

**Bus SPI3** (yeux — 2× GC9A01 ronds) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO38 | MOSI | GC9A01 ×2 (partagé) |
| GPIO39 | SCLK | GC9A01 ×2 (partagé) |
| GPIO40 | CS_EYE_L | GC9A01 œil gauche |
| GPIO41 | DC | GC9A01 ×2 (partagé) |
| GPIO42 | RST | GC9A01 ×2 (partagé) |
| GPIO14 | CS_EYE_R | GC9A01 œil droit |

> Deux écrans ronds 1.3" 240×240 formant les "yeux" du personnage. MOSI/SCLK/DC/RST partagés, seul le CS distingue les deux écrans. SCLK via R14 22 Ω série. SPI à 40 MHz aujourd'hui (80 MHz visés ; la puce accepte jusqu'à 100 MHz en écriture, GC9A01A tableau 44), ~16 FPS par œil mesurés.

**Bus I2S0** (audio sortie → PCM5122) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO4 | BCLK | PCM5122PW |
| GPIO5 | LRCLK | PCM5122PW |
| GPIO6 | DOUT | PCM5122PW |

> PCM5122PW en mode I2C (MODE1→GND, MODE2→3V3_A — vérifié datasheet TI SLAS763C §8.4.1.1, "MODE pins à GND" était imprécis, MODE1/MODE2 aux deux GND = mode Hardwired, pas I2C). ADR1/ADR2→GND → adresse 0x4C (confirmé `PCM5122_I2C_ADDR` dans `hal_audio.c`). SCK→GND (mode PLL 3-wire depuis BCK, §8.3.6.3). PLL depuis BCK. I2S : 44100 Hz, 16-bit, 32-bit slots (BCLK = 2.82 MHz), Philips standard. Volume digital : 100 % = 0 dB (reg 0x3D/0x3E = 0x30) dans le firmware actuel, plafond à poser (voir ci-dessous). Sortie analogique OUTL/OUTR → filtre RC (470Ω + 2.2nF NP0/C0G, fc≈153 kHz — valeurs recommandées datasheet TI §8.3.5.2) → PAM8406. Voir `docs/datasheets/pcm5122-registers.md` pour la référence registres complète.
>
> Le **PAM8406** (classe D, stéréo) est purement analogique, pas de driver. Puissance réelle sous 5 V (datasheet DS43342) : 3,14 W/canal sur 4 Ω (THD 10 %), 1,8 W sur 8 Ω — les « 5 W » ne valent que sur 2 Ω. SHDN et MUTE tirés haut (toujours actif), MODE haut = classe D (R_MODE_D montée, R_MODE_AB en DNP — jamais les deux). Le mute se fait via le registre PCM5122 (0x03). **Gain fixe 24 dB** : le DAC sort 2,1 Vrms à 0 dB, l'ampli écrête au-delà d'environ −20 dB de volume numérique (la datasheet avertit qu'un écrêtage peut l'endommager).
>
> **Haut-parleur retenu : PUI AS04008PO-2-R, 8 Ω, 1 W nominal / 2 W max**, un seul, sur le canal **L** (face Dessus). Sortie : **J10** (JST-PH 4 broches : L+, L−, R+, R−) à travers les ferrites **FB3-FB6** (BLM21PG221SN1D). Sorties en pont : aucune masse commune, ne jamais relier les `−` entre elles ni à GND. Le canal R reste disponible pour un second haut-parleur. → Firmware : **plafond de volume vers −22 dB** (1 W sur 8 Ω, calcul dans `docs/datasheets/PAM8406.md`) et mixage mono sur L.

**Bus I2S1** (audio entrée — micro MEMS) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO16 | SCK | ICS-43434 micro MEMS |
| GPIO18 | WS | ICS-43434 |
| GPIO7  | SD | ICS-43434 |

> L'ICS-43434 est un micro MEMS numérique I2S (24-bit, 43 kHz BW), bottom-port, monté sur le PCB main : son trou traverse la carte (face Dessous) — conduit acoustique à prévoir avec le boîtier. Pin L/R à GND → canal gauche. Pas encore de driver de capture.

**GPIO individuels** :

| GPIO | Fonction | Composant | Notes |
|---|---|---|---|
| GPIO1 | Toggle SW1 | E-Switch 100SP SPDT (face avant, via J8) | Input, pull-up interne |
| GPIO2 | Toggle SW2 | E-Switch 100SP SPDT (face avant, via J8) | Input, pull-up interne |
| GPIO3 | VBAT_SENSE | Pont diviseur 1 MΩ / 1 MΩ + 100 nF sur la Main | ADC1_CH2, interne à la Main (retiré de J8.5 le 2026-09-27) |
| GPIO15 | SPI2_MISO | Carte microSD | Bus partagé avec le display bouche |
| GPIO19 | USB D- | USB-C natif | — |
| GPIO20 | USB D+ | USB-C natif | — |
| GPIO45 | BTN1 | Bouton poussoir Côté 1 (via J8.5) | Strapping VDD_SPI : **jamais de pull-up externe** (1 au reset = flash 1,8 V → ne boote plus). Pull-down externe **R23 10 kΩ** ; bouton **actif haut** (vers 3V3_D), à ne pas tenir enfoncé au démarrage. Cf. `docs/datasheets/ESP32-S3-datasheet.md` |
| GPIO46 | BTN2 | Bouton poussoir Côté 1 (via J8.6) | Strapping boot : pas de pull-up externe ; pull-down **R24 10 kΩ**, bouton actif haut |
| GPIO47 | SPI2_CS_SD | Carte microSD (chip select) | Distinct du CS display bouche (GPIO10) |
| GPIO48 | WS2812 DATA | Chaîne LEDs RGB | RMT driver |

> Deux boutons poussoirs mécaniques (Côté 1) sont câblés sur GPIO45/46 via J8 ; d'autres boutons peuvent passer par les 6 touches du CAP1298 (capacitif, fonctionne aussi avec boutons conducteurs).
>
> **Budget GPIO saturé** : à ce stade, GPIO1-21 et GPIO38-48 sont tous alloués (plus aucune pin libre). GPIO0 réservé strapping/boot, GPIO26-37 indisponibles (flash/PSRAM octal), GPIO43-44 réservées UART0 debug.

**Récapitulatif GPIO complet (WROOM-1-N16R8)** :

```
GPIO 0      — (strapping boot, réservé)
GPIO 1-2    — Toggles SW1/SW2 (face avant, via J8)
GPIO 3      — VBAT_SENSE (ADC1_CH2, pont diviseur lecture batterie)
GPIO 4-6    — I2S0 audio out (PCM5122)
GPIO 7      — I2S1 SD (ICS-43434 micro)
GPIO 8-13   — SPI2 display bouche, TBD (CS/DC/RST/MOSI/SCLK/BUSY)
GPIO 14     — CS œil droit (GC9A01 #2, bus SPI3)
GPIO 15     — SPI2_MISO (carte microSD, bus partagé display bouche)
GPIO 16     — I2S1 SCK (ICS-43434 micro)
GPIO 17     — I2C SCL
GPIO 18     — I2S1 WS (ICS-43434 micro)
GPIO 19-20  — USB-C
GPIO 21     — I2C SDA
GPIO 26-37  — ⛔ Flash/PSRAM (non disponible)
GPIO 38-42  — SPI3 yeux (2× GC9A01 : MOSI/SCLK/CS_L/DC/RST partagés)
GPIO 43-44  — UART0 debug
GPIO 45-46  — BTN1/BTN2, boutons Côté 1 via J8.5/J8.6 (pull-down 10 kΩ, actifs hauts, JAMAIS de pull-up sur 45)
GPIO 47     — SPI2_CS_SD (carte microSD)
GPIO 48     — WS2812 LEDs

⚠ Budget saturé : GPIO1-21 et 38-48 tous alloués, plus aucune pin libre.
```

#### 2.2.2b Alimentation

```
USB-C J1 (satellite Côté 2 : CC 5,1 kΩ = sink, USBLC6) ── câble ── J14 (Main)
    ↓ 5V_USB
bq24075 (U8) — chargeur 1 A + power path DPPM, entrée limitée à 1,07 A (R_ILIM)
    ├── BAT → cellule Li-ion 18650 3400-3500 mAh (J2 : BAT+, BAT−, NTC, retour NTC sur GND)
    │         protection DW01A (U9) + FS8205 (U10) sur le négatif (VBAT−)
    └── OUT = VSYS (≈ VIN USB − dropout sur secteur, 5,5 V max régulés ; ≈ VBAT sur batterie)
         │   interrupteur marche/arrêt SW3 (Côté 2, via J12) → EN_SYS (R20 2,2 k pull-down : courant de contact pour SW3, 27/09)
         ├── AP2112M-3.3 SO-8 (U5)  [EN_SYS] → 3V3_D (ESP32, SD, écrans, IMU, satellites)
         ├── AP2112K-3.3 SOT-25 (U6)[EN_SYS] → 3V3_A (PCM5122, ICS-43434)
         └── MT3608 boost (U7)      [EN_SYS] → 5V_BOOST (5,1 V) → D1 SS34
                  └── load switch Q1 AO3401A (commandé par Q2 2N7002 depuis EN_SYS)
                       → 5V (12 WS2812 du halo, PAM8406, J9 halo visage)
```

> **Power path (DPPM)** : le système est alimenté en priorité par l'USB, le surplus charge la batterie. La box peut rester branchée sans user la batterie. **Deux LDOs séparés** pour isoler le bruit digital du chemin audio. GND unique continu (PAS de split). Source de vérité : le schéma KiCad `hardware/main/power.kicad_sch` (les `docs/schematics/*.txt` sont antérieurs à KiCad).
>
> **Marche/arrêt (option B, câblée le 2026-09-27)** : SW3 ne commute que `EN_SYS`, qui coupe les trois régulateurs. Le MT3608 étant un boost asynchrone, EN bas laisse passer VSYS − V_F vers sa sortie : le load switch Q1 coupe donc réellement le rail 5 V (LEDs et ampli). Le bq24075 reste actif : **la charge USB fonctionne box éteinte**. Consommation éteinte estimée ≈ 15-20 µA (à mesurer).
>
> **Paramètres vérifiés datasheets (audits 2026-09-23 et 2026-09-27)** :
> - Charge : 1 A (R_ISET 890 Ω), entrée limitée à 1,07 A (R_ILIM 1,5 kΩ, mode EN2 = H / EN1 = L).
>   ✅ 2026-09-27 : timers de sécurité réactivés (**R_TMR 56 kΩ** → 5,6-9,3 h) et **NTC 10 kΩ
>   de la cellule sur TS** via J2.3 (fenêtre 0-50 °C) ; `R_TS` conservée en **DNP** — monter
>   l'une **ou** l'autre, jamais les deux (mesure faussée, charge suspendue sans symptôme).
> - **Batterie : cellule 18650 de 3400-3500 mAh** (décision 2026-09-27), et non plus une LiPo
>   pouch. Même chimie lithium-ion, mais enveloppe acier rigide avec évent de surpression :
>   bien plus robuste dans un objet qu'on secoue et retourne. ~5 h d'autonomie (~690 mA
>   moyens), charge en ~4 h — compatible avec R_ISET et R_TMR tels quels. Protection assurée
>   par le DW01A + FS8205 de la carte, donc **cellule nue** possible. NTC 10 kΩ à coller sur
>   la cellule. Jamais de soudure au fer sur une cellule : languettes ou support.
> - **NTC** : J2 en **4 broches** (2026-09-27) — le retour de la NTC va sur **GND** (J2.4) et non sur BAT− : la TS mesure par rapport à VSS, et BAT− en est séparé par les MOSFET du FS8205 (56-74 mV à 1 A, soit ≈ +7 °C d'erreur sur la coupure en surchauffe). Harnais de cellule à 4 fils.
> - Protection DW01A + FS8205 : surcharge 4,30 V, décharge profonde 2,40 V, **surintensité ≈ 1,6-3,2 A** → plafond du courant crête total (plafonds firmware de luminosité et de volume, mesure au proto).
> - 3V3_D : **U5 = AP2112M-3.3 en SO-8** (θJA 114 °C/W au lieu de 184 °C/W en SOT-25), 600 mA garantis, l'ESP32 en exige ≥ 0,5 A ; plan de cuivre sous U5 au layout. Charges du rail : ESP32 ≥ 3,0 V, CAP1298 ≥ 3,0 V, GC9A01A ≤ 3,3 V → **extinction firmware sur VBAT basse** (~3,4-3,5 V).
> - Rail 5 V (MT3608, 5,0-5,2 V) : 12 WS2812B = 432 mA en blanc (+ 7,2 mA au repos), PAM8406 ≈ 0,4 A par canal au maximum sur le haut-parleur 8 Ω, plus le halo externe sur J9 (pas de budget fixé : c'est le plafond firmware de luminosité qui borne l'ensemble). **D1 = SS34** (3 A). **L1 = Sunlord SWPA5040S6R8MT** (6,8 µH ±20 %, Isat 2,9 A, 5 × 5 mm ; crête calculée ≈ 1,9 A — `docs/datasheets/SWPA5040S.md`).

#### 2.2.2c Assignation des faces — Cube 150×150×150mm *(dimension à valider au proto boîtier — la vision mentionne 120mm)*

> Les capteurs et interactions ne sont pas forcément 1:1 avec les faces. Plusieurs capteurs peuvent cohabiter sur une même face, et un même type d'interaction peut traverser plusieurs faces.

| Face | Rôle principal | Composants |
|---|---|---|
| **Devant** | Visage du personnage | 2× GC9A01 1.3" ronds (yeux) + display bouche (TBD, pas e-ink) + WS2812 rétro + BMP280 souffle (souffler sur la bouche) + VEML7700 lumière (éclairer les yeux → réaction du personnage) |
| **Dessus** | Voix + NFC | Haut-parleur (câblé depuis PAM8406 sur Main) + ST25DV04KC-IE6S3 NFC (tap téléphone) |
| **Côté 1** | Panneau de contrôle | ADS7830 + 4 faders + 4 potentiomètres rotatifs, toggles SW1/SW2, boutons poussoir |
| **Côté 2** | Accès technique + énigme | Port USB-C réel (charge + énigme "bonne clé USB avec le bon contenu"), interrupteur, clavier tactile CAP1298 (6 touches) + proximité "approche ta main" |
| **Côté 3** | Zone magique | TMAG5273 Hall linéaire seul ("pose un objet" aimanté) — MLX90614 retiré, proximité déplacée sur Côté 2 (2026-09-27) |
| **Dessous** | Main (technique + lest) | ESP32-S3, alimentation, PCM5122+PAM8406, LSM6DSOXTR (IMU, soudé directement sur Main — aucune contrainte de position), batterie (lest, stabilise l'orientation de repos), WS2812 halo table |

> Répartition figée par face (2026-09-21). Le cube prend de fait une orientation de repos stable une fois assemblé (batterie + Main lestent le Dessous), même si géométriquement un cube n'a ni haut ni bas.

**Concept "personnage" :**
```
La face avant est un VISAGE :
  - 2× GC9A01 ronds = yeux expressifs (clignements, regard, émotions)
  - 1× display bouche (TBD, pas e-ink) = bouche (texte mot-à-mot, style Animal Crossing)
  - WS2812 = halo lumineux ambiance autour du visage
  - BMP280 = souffle sur la bouche (interaction directe avec le personnage)
  - VEML7700 = éclairer les yeux déclenche une réaction (le personnage râle/réagit à la lumière)

Le personnage parle : texte à l'écran + syllabes audio AC synchronisées, la voix
sort du haut-parleur sur le Dessus (séparé physiquement de la bouche visuelle).
Les yeux réagissent en temps réel aux capteurs et aux actions du joueur.
Les énigmes sont "données" par le personnage (dialogue + feedback visuel).
```

> **Énigme clé USB (Côté 2) :** le port USB-C réel du produit sert aussi de mécanique d'énigme — le joueur doit trouver/brancher une clé USB contenant un fichier précis (ex. texte avec une information donnée). Nécessite le mode USB host/OTG sur l'ESP32-S3 (distinct du mode device/CDC actuel utilisé pour le debug) — à instruire en Phase 2 firmware. Volontairement pas de mécanisme d'insertion factice à côté : réutiliser le port réel évite le risque d'un joueur qui force un objet non-USB dedans.
>
> **Décision du 2026-09-27** : le port USB-C **J1 est déporté sur le satellite Côté 2**, avec sa protection USBLC6 et ses résistances CC (5,1 kΩ). La Main reçoit **J14** (JST-PH 6 broches : VBUS, GND, D−, D+, GND, 5V_HOST) ; les résistances série 22 Ω (R11/R12) restent au pied de l'ESP32. USB Full-Speed (12 Mbit/s) : 15-20 cm de câble interne conviennent. **Mode hôte (énigme de la clé) : place réservée, non câblée** — J14.6 `5V_HOST` peut recevoir le rail 5 V par R22 (0 Ω, DNP) ; resteront à concevoir sur le satellite la commutation et la limitation du VBUS vers la clé, la protection contre la réinjection dans le chargeur et la gestion des CC (source), plus une commande (pas de GPIO libre : expander I2C sur le satellite). En mode hôte, la console USB est perdue : console de secours sur les pastilles `TP_TXD0` / `TP_RXD0` (UART0, posées le 2026-09-27).

> **Note NFC téléphone :** le ST25DV est un tag pur (pas de mode lecteur) — la box ne peut lire ni un téléphone en émulation de tag, ni un badge externe. L'unique interaction NFC possible est **téléphone → lit la box** (URL/contenu NDEF stocké côté box), qui fonctionne nativement sur tout téléphone NFC y compris iPhone. L'ancienne limitation iOS (HCE restreint aux paiements, bloquait la lecture d'un téléphone par un lecteur PN532) ne s'applique plus : il n'y a plus de fonction lecteur du tout, dans aucune direction.

#### 2.2.3 Architecture PCB — Main + Satellites par face

> **Statut (2026-09-27) :** **6 PCB — Main + un satellite par face**, chacun son projet KiCad : `hardware/main/`, `hardware/devant/`, `hardware/dessus/`, `hardware/cote1/`, `hardware/cote2/`, `hardware/cote3/`. Les satellites partagent les bibliothèques de la Main (`hardware/main/libs/`). Chaque feuille porte un cartouche de spécification (composants, liaison vers la Main, contraintes datasheet). Les **cinq schémas de satellites sont dessinés** (Côté 2 : USB + CAP1298 ; Dessus, Devant, Côté 1, Côté 3 le 2026-09-27, références préfixées par face dans la BOM : `DESSUS-U1`, `COTE1-SL1`…). Points à trancher : `docs/audits/2026-09-27-questions-satellites.md`.

**PCB Main — face Dessous** (~100×100mm, 4 couches) :

Composants embarqués :
- ESP32-S3-WROOM-1-N16R8 (soudé)
- Alimentation complète : bq24075 (power path) + DW01A + FS8205, AP2112M-3.3 (3V3_D, SO-8) + AP2112K-3.3 (3V3_A), MT3608 boost + load switch 5 V, marche/arrêt par EN_SYS
- PCM5122PW DAC + découplage (zone audio isolée)
- PAM8406 Class D ampli + ferrites de sortie FB3-FB6 → J10 (le haut-parleur lui-même est déporté sur la face Dessus, câblé depuis Main)
- ICS-43434 MEMS micro (bottom-port, trou PCB pour le son)
- LSM6DSOXTR accéléromètre/gyro + MLC (IMU — soudé directement sur Main, aucune contrainte de position donc pas besoin de satellite dédié)
- 12 × WS2812B-B (halo face Dessous)
- Slot microSD (J11)
- J14 (JST-PH 6) : liaison USB vers le port USB-C du satellite Côté 2 (charge + USB CDC debug) ; R22 (DNP) = réserve du mode hôte
- Connecteur batterie J2 JST-PH 4 broches (BAT+, BAT−, NTC, retour NTC sur GND)
- Connecteurs JST vers les faces satellites (voir « Connectique backbone » ci-dessous)
- 28 points de test, dont la console UART0 `TP_TXD0` / `TP_RXD0` (liste et valeurs attendues : `docs/pcb/03-validation-qualite.md`)

> **ST25DV04KC-IE6S3 (NFC) retiré du Main** — déplacé sur le satellite face Dessus (voir plus bas), car le tap téléphone doit être accessible sur la face Dessus alors que Main vit sur le Dessous. Le sheet KiCad `nfc.kicad_sch` reste réutilisable tel quel comme point de départ du satellite Dessus.

**Satellite face Devant** — visage :
- 2× GC9A01 1.3" ronds (yeux, SPI3 depuis Main)
- Display bouche, type à définir ultérieurement — pas e-ink (SPI2 depuis Main)
- WS2812 rétro (halo visage)
- BMP280 pression/souffle (avec ouverture, près de la bouche)
- VEML7700 capteur lumière (avec ouverture, déclenche une réaction du personnage)

**Satellite face Dessus** — voix + NFC :
- Haut-parleur (câblé depuis PAM8406 sur Main, pas d'électronique dédiée à part le driver acoustique)
- ST25DV04KC-IE6S3 NFC (antenne PCB, boucle à dessiner — cf. `docs/datasheets/ST25DV04K.md`)

**Satellite face Côté 1** — panneau de contrôle :
- ADS7830 (ADC I2C 0x48) + 4 faders SL1-SL4 + 4 pots rotatifs RV1-RV4
- Toggles SW1/SW2
- Boutons poussoir

**Satellite face Côté 2** — accès technique + énigme :
- Port USB-C réel J1 + USBLC6 + CC 5,1 kΩ (déplacés depuis la Main le 2026-09-27), relié à J14 ; lecture de clé USB (énigme) : place réservée, circuit hôte à concevoir
- Interrupteur
- CAP1298 capacitif (SOIC-14, LCSC C2652072 ; remplace le MTCH2120 le 2026-09-27) : 6 touches + grande électrode de proximité « approche ta main » entourée de l'anneau de garde SG ; électrodes en cuivre sur le PCB du satellite (schéma dessiné le 2026-09-27)

**Satellite face Côté 3** — zone magique :
- TMAG5273 Hall linéaire 3D I2C
- (« Approche ta main » déplacé sur Côté 2 le 2026-09-27 : un seul CAP12xx possible sur le bus)
- ~~MLX90614~~ (retiré le 2026-09-27 : coût, et bus I2C bridé à 100 kHz)

> Tous les capteurs I2C (satellites + Main) partagent le même bus backbone. Adresses vérifiées datasheets (2026-09-23), aucun conflit en Phase 2 : VEML7700 0x10, CAP1298 0x28 (fixe : un seul CAP12xx), TMAG5273A1 0x35, ADS7830 0x48, PCM5122 0x4C, ST25DV 0x53 + 0x57, LSM6DSOX 0x6A, BMP280 0x76 (MPR121 0x5A en Phase 1). Bus à 100 kHz aujourd'hui ; le MLX90614, qui imposait cette limite, est retiré — monter à 400 kHz demande de vérifier chaque datasheet et la capacité du bus avec les câbles.

**Connectique backbone (état du schéma Main au 2026-09-27)** :

JST-SH (1,0 mm, verrouillable) pour les signaux inter-PCB ; JST-PH (2,0 mm) pour la puissance :

| Réf. | Type | Brochage | Vers |
|---|---|---|---|
| J3-J6, J13 | JST-SH 4 | **Qwiic/STEMMA QT** : 1 = GND, 2 = 3V3_D, 3 = SDA, 4 = SCL | satellites I2C — **5 connecteurs, un par face I2C** (Devant, Dessus, Côté 1, Côté 2, Côté 3 ; J13 ajouté le 2026-09-27) |
| J7 | JST-SH 10 | 3V3_D, GND, MOSI, SCLK, CS_L, CS_R, DC, RST, NC, NC | yeux (SPI3) |
| J7b | JST-SH 8 | 3V3_D, GND, MOSI, SCLK, CS, DC, RST, BUSY | bouche (SPI2) |
| J8 | JST-SH 6 | 3V3_D, GND, SW1, SW2, BTN1, BTN2 | Côté 1 (toggles + 2 boutons poussoirs actifs hauts, pull-down 10 kΩ sur la Main) |
| J9 | JST-PH 4 | 5V, GND, DATA (sortie de LED13), NC | halo visage (Devant), suite de la chaîne WS2812 — pas de budget de courant fixé : le plafond firmware de luminosité borne l'ensemble |
| J10 | JST-PH 4 | L+, L−, R+, R− (sorties en pont, via FB3-FB6) | haut-parleur (Dessus) |
| J11 | slot microSD TF-01A | — | carte SD |
| J12 | JST-SH 2 | VSYS, EN_SYS | interrupteur marche/arrêt SW3 = E-Switch RR111C1921 (bascule ronde de façade Côté 2, câble direct, hors satellite) |
| J2 | JST-PH 4 | BAT+, BAT−, NTC, retour NTC (GND) | cellule 18650 (harnais 4 fils, voir §2.2.2b) |
| J14 | JST-PH 6 | VBUS (5V_USB), GND, D−, D+, GND, 5V_HOST | satellite Côté 2 (port USB-C J1 + USBLC6 + CC) |

**Boîtier** :
- Phase proto : Imprimé en 3D (PLA/PETG)
- Phase 1 Lite : MDF découpé laser + peinture noire mate
- Phase 2 Pro : Bois massif (noyer/chêne) + laiton brossé

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
│  │  audio_player  (I2S DMA, MP3 via minimp3)               │    │
│  │  audio_capture (I2S micro, analyse niveau/rythme)        │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Core 1 (UI + jeu)                     │    │
│  │                                                          │    │
│  │  scenario_engine (state machine, parse JSON)             │    │
│  │  sensor_manager  (polling I2C 50ms, event queue)         │    │
│  │  display_manager (esp_lcd : 2× GC9A01, display bouche TBD)│    │
│  │  led_manager     (WS2812 via RMT ESP-IDF)               │    │
│  └─────────────────────────────────────────────────────────┘    │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                    Shared (FreeRTOS)                     │    │
│  │                                                          │    │
│  │  event_queue    (xQueue entre cores)                     │    │
│  │  storage_manager (SD card SPI+FAT, NVS flash)            │    │
│  │  crypto_manager  (ECDSA vérif via mbedTLS intégré)       │    │
│  │  config_manager  (WiFi creds, box ID, préférences)       │    │
│  └─────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────┘
```

> Le schéma ci-dessus est l'architecture **cible**. Correspondance avec les composants réels de `firmware/components/` : réseau = `hal_wifi` + `cloud_client` (auth, sync, packages ; OTA à venir) + `ble_prov` (provisioning) ; audio = `hal_audio` (mixer 4 voix, task `audio_mixer` seule écrivaine I2S) ; écrans = `hal_display` + `ui_manager` (yeux) ; scénario = `scenario` (moteur + validateur) ; stockage = `hal_storage` (SD) + `config_manager` (NVS) ; identité = `hal_box_auth` ; capteurs = `hal_i2c_bus`, `hal_touch` (MPR121 ; CAP1298 à écrire, pilote MTCH2120 obsolète), `hal_leds`, `hal_imu`, `hal_light`, `hal_nfc` (ancien PN532, à réécrire pour le ST25DV). Pas de `sensor_manager` ni de `crypto_manager` pour l'instant ; les servos ont été retirés du produit.

**Composants / librairies :**

| Composant | Usage |
|---|---|
| ESP-IDF v6.1 | Framework de base (FreeRTOS, drivers, HAL) |
| LVGL | Non utilisé actuellement (rendu direct esp_lcd) — dépendance conservée pour usage futur éventuel |
| esp_lcd + esp_lcd_gc9a01 | Drivers écrans (SPI : GC9A01 ×2, display bouche TBD) |
| RMT (encodeur d'octets) | WS2812 LEDs (`hal_leds`) |
| cJSON | Parsing scénario / API — copie embarquée dans `components/scenario/` (MIT) |
| minimp3 | Décodeur MP3 single-header (voix du mixer `hal_audio`, buffers PSRAM). ESP-ADF évalué en Phase 2+ uniquement si un vrai pipeline multi-format/streaming devient nécessaire. |
| i2c_master (PCM5122) | Config DAC : PLL, volume, filtre, mute via registres I2C (addr 0x4C) |
| i2c_master | Bus unique à **100 kHz** : MPR121/CAP1298, LSM6DSOXTR, VEML7700 ; ST25DV, TMAG5273, BMP280, ADS7830 **à écrire** |
| NimBLE (ESP-IDF) | Provisioning WiFi + preuve de possession via BLE (`ble_prov`) |
| esp_http_client + bundle CA | Auth, sync et download des packages en HTTPS (`cloud_client`) |
| PSA crypto (mbedTLS 4) | HMAC-SHA256 de l'identité box (`hal_box_auth`) |
| esp_https_ota | OTA HTTPS — **(cible, F6)** |
| mbedTLS ECDSA P-256 | Vérification de signature des scénarios / firmware, clé publique compilée — **(cible)** |

#### 2.3.2 Format des scénarios (YAML)

Chaque scénario est un fichier YAML décrivant une machine à états finis (state machine). Le YAML est le **format auteur** (source de vérité) ; il est converti en **JSON** par `tools/yaml2json.py` (avec validation de schéma), et c'est ce **JSON** que le firmware embarque et parse (via cJSON) au démarrage — sans aucune logique codée en dur. Le `.json` est un **artefact généré** : ne jamais l'éditer à la main, régénérer depuis le YAML.

> Note pipeline : `yaml2json.py` désactive la résolution YAML 1.1 de `on/off/yes/no` en booléens, car le DSL utilise `on:` (déclencheur d'événement) et `off` (mode LED) comme chaînes.

> ⚠️ L'exemple ci-dessous illustre le DSL **cible**. Certaines constructions (`type: combo` + `require_all`, hints `after_sec`/`after_attempts`, `do_fail_max`, `screen_compass`) ne sont **pas encore** implémentées. Le DSL réellement supporté par `yaml2json.py` et le firmware aujourd'hui : types `narrative/trigger/input/branch/end`, hints `delay_sec`, échec `do_fail`, actions `screen_main/screen_secondary/audio/led/flash/set_var/incr_var/eye_blink/eye_emotion/eye_look` (`screen_*` ne font que logger en attendant l'écran bouche ; `servo` est accepté mais sans effet, les servos ayant été retirés ; `fallback` n'est pas implémenté). Toute nouvelle clé doit être ajoutée à `scenario_validate.c`, aux tests de `firmware/test_host/` et à `tools/yaml2json.py`.

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
| `combo` | Attend plusieurs capteurs simultanément — **(cible, non implémenté)** |
| `narrative` | Joue des médias sans attente d'input |
| `branch` | Branchement conditionnel selon une variable |
| `end` | Fin du scénario |

**Fallback pour hardware manquant (cible, non implémenté) :**

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
| Frontend | Next.js 16 (App Router), Tailwind CSS, shadcn/ui |
| Auth | Supabase Auth (email, OAuth Google/Apple) |
| Database | Supabase PostgreSQL |
| Storage | Aujourd'hui : packages de scénario dans `web/scenario-packages/`, servis par `/api/box/pkg` (JWT box + droit). Cible : Supabase Storage (assets légers) + Cloudflare R2 (audio, images scénarios) |
| Paiements | Stripe (one-shot + abonnements) |
| Emails | Resend |
| Éditeur visuel | React Flow (mode Pro) |
| Éditeur code | Monaco Editor (mode Expert YAML) |
| IA | Anthropic API (Claude) — assistance génération scénarios |
| Hébergement | Coolify self-hosted (Nixpacks, base dir `web/`, domaine dev box.agill.es) + Supabase self-hosted — en production depuis 2026-06 |

> Stack : **en service** = Next.js 16 + Tailwind/shadcn, Supabase Auth (email + Google) et PostgreSQL, Coolify ; **cible** = Stripe, Resend, R2, React Flow, Monaco, Anthropic API.

**Schéma base de données — état réel (Supabase, relevé lors de l'audit 2026-09-23) :**
`profiles` (→ `auth.users`), `devices` (`box_uid` UNIQUE, `owner_id`, `name`, `firmware_version`, `last_sync_at`), `scenarios` (`slug` UNIQUE, `version`, `package_path`, `active`, `price_chf`…), `device_scenarios` (PK `device_id, scenario_id` — **seul droit d'accès aujourd'hui, lié à la box**), `box_challenges` (nonces à usage unique), `firmware_releases` (`version`, `channel`, `url`, `sha256`, `active`), `waitlist`. Pas encore de migrations versionnées dans le dépôt. **Décision ouverte avant Stripe** : lier les licences à l'utilisateur (table `licenses`/`purchases`) plutôt qu'à la box.

**Schéma base de données — cible (tables principales) :**

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

**API endpoints firmware → serveur :** voir §6.1 (source unique).

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

### 3.0 Budget & planning

**Budget Phase 1 (enveloppe 500-2000 CHF) :**

| Poste | Estimation basse | Estimation haute |
|---|---|---|
| Composants électroniques | 300 CHF | 500 CHF |
| Matériaux boîtier (impression 3D, peinture) | 100 CHF | 200 CHF |
| Assets son & illustration | 0 CHF | 700 CHF |
| Infrastructure web (Coolify + Supabase self-hosted sur VPS existant) | 0 CHF | 0 CHF |
| Imprévus / itérations | 100 CHF | 300 CHF |
| **Total** | **500 CHF** | **1700 CHF** |

**Jalons Phase 1 (cible : août-septembre 2026) :**

| Jalon | Date cible | Critère de succès | Statut |
|---|---|---|---|
| M0 — Lancement | Mai 2026 | Hardware commandé, firmware validé dev board | ✅ |
| M1 — Hardware | Juin 2026 | ESP32-S3 reçu, drivers principaux validés (display, audio, touch, LEDs) | ✅ (partiel — NFC, capteurs restants en attente) |
| M2 — Scénario + Boîtier | Juillet 2026 | Scénario chargé, prototype boîtier V1 jouable | ☐ — date dépassée, à replanifier |
| M3 — Playtest FFF | Août-Septembre 2026 | 10-15 testeurs, go/no-go Phase 2 décidé | ☐ — date dépassée, à replanifier |

**Séquencement des workstreams Phase 1 :**
```
Maintenant → M1 : firmware + hardware (bloquant pour la suite)
M1 → M2   : scénario + boîtier (en parallèle, non interdépendants)
En continu : web MVP peut démarrer dès maintenant (non bloquant)
M2 → M3   : intégration complète + playtests
```

**Budget Phase 2 :** à affiner après go/no-go Phase 1. Poste principal identifié : certification CE-RED (~10k CHF, chemin critique — à initier dès le début de Phase 2).

---

### 3.1 Phase 1 — Proof of Concept

**Durée estimée :** 3-4 mois (voir jalons §3.0)
**Objectif :** Valider que le hardware fonctionne et que les gens veulent jouer

**Hardware :**
- [x] ESP32-S3-WROOM-1-N16R8 → DevKitC-1 validé sur breadboard
- [x] 2× GC9A01 1.3" ronds (yeux, SPI3 partagé) → driver Espressif `esp_lcd_gc9a01` intégré, validé hardware (animation Uncanny Eyes fluide ~16 fps/œil, framebuf DMA full-frame, ISR `on_color_trans_done` + sémaphore par œil)
- [ ] Display bouche (SPI2), type à définir ultérieurement (pas e-ink) → composant à choisir puis driver à intégrer
- [x] Câbler PCM5122PW (I2S + I2C 0x4C) + PAM8406 + speakers 8Ω/5W → audio validé (PLL, filtre, MP3 bg)
- [x] Câbler MPR121 breakout → keypad capacitif 12 canaux validé (I2C 0x5A)
- [x] Câbler WS2812 → LEDs validées (RMT, GPIO48)
- [ ] Câbler ST25DV04KC-IE6S3 → valider NFC (tag, écriture URL NDEF depuis l'ESP32)
- [ ] Câbler CAP1298 → valider le clavier capacitif (Phase 2 PCB)
- [ ] Câbler LSM6DSOXTR, BMP280, VEML7700, TMAG5273 → valider I2C bus complet
- [ ] Câbler microphone MEMS I2S → valider micro
- [ ] Tester le moteur de scénario YAML sur le hardware assemblé complet

**Firmware :**
- [x] Moteur de scénario JSON (state machine, hints, variables, branches, do_fail)
- [x] Scénario "Capitaine Verdier" — YAML + JSON, 12 steps, 3 énigmes (boussole, code, inclinaison)
- [x] Driver audio PCM5122PW — I2S 32-bit slots + I2C PLL config, filtre ringing-less FIR, volume -6dB hw
- [x] Musique de fond MP3 — minimp3 décodage tâche bg (24 KB stack), MP3 embarqué en flash
- [x] Driver yeux 2× GC9A01 SPI3 partagé (CS_L=40, CS_R=14, MOSI=38, SCLK=39, DC=41, RST=42) — `components/hal_display` autour de `esp_lcd_gc9a01` v2.0.4, 240×240 RGB565 @ 40 MHz
- [x] MPR121 tactile capacitif 12 canaux — validé DevKitC-1 (I2C 0x5A, 100kHz, SDA=21/SCL=17)
- [x] Driver LEDs WS2812B (RMT, GRB, show) — validé
- [x] Drivers I2C écrits : MPR121 (validé), `hal_imu` (LSM6DSO/DSOX, non branché), `hal_light` (VEML7700, non branché), MTCH2120 (abandonné le 2026-09-27 ; pilote CAP1298 à écrire)
- [ ] Drivers I2C à écrire : TMAG5273, BMP280, ADS7830, ST25DV
- [x] Outil YAML→JSON (tools/yaml2json.py avec validation)
- [ ] Driver NFC ST25DV — composant changé (ex-PN532, protocole totalement différent : registres I2C/EEPROM au lieu du protocole de commande PN532), ancien driver `hal_nfc` obsolète, réécriture à faire
- [x] Système de fichiers SD SPI+FAT — validé sur cible (module 5V, SPI2 CS=47, monté sur /sdcard)
- [x] App scénario principale (main.c) — scénario + ambient.mp3 chargés depuis SD (`/sdcard/scenarios/<dir>/`), fallback embarqué ; callbacks audio/led/eye_*, keypad MPR121, hold 2s pour simuler rfid/rotary/tilt
- [x] `hal_box_auth` — creds box dans la partition dédiée `box_nvs` (namespace `box_creds`, migration auto) + signature HMAC-SHA256 `"<purpose>:<uid>:<challenge>"` via PSA crypto (mbedTLS 4). Provisionné par `tools/provision_box.py`
- [x] `hal_wifi` — STA, creds WiFi en NVS (`wifi_creds`, provisionnés par `provision_box.py --wifi-ssid/--wifi-pass`), connexion validée sur cible + smoke-test HTTPS OK (bundle CA Mozilla, GET box.agill.es status 200). Réseau dans une tâche dédiée core 0 (pile TLS 8 KB)
- [x] `cloud_client` — flux challenge→auth→sync + packages de scénarios sur SD, validé sur cible (voir `docs/plans/firmware-cloud-client.md`)
- [ ] **Corrections issues des datasheets et du hardware actuel** (pilote CAP1298, WS2812 V5 + 12 LEDs, plafond de volume −22 dB + mono, batterie basse, SPI2 partagé, bus I2C à 100 kHz…) : liste tenue dans `docs/audits/RESTE-A-FAIRE.md` §4
- [x] Partitions OTA 16 MB (factory + ota_0 + ota_1 de 3 MB, `box_nvs`, storage LittleFS 6.9 MB, rollback activé) — validées sur cible
- [x] PSRAM octal 8 MB activée (SPIRAM_MODE_OCT 80 MHz) — buffers scénario et MP3 en MALLOC_CAP_SPIRAM
- [x] `ui_manager` v2 — animation yeux (Uncanny Eyes Adafruit MIT porté ESP-IDF) : 2× GC9A01, rendu 128×128 centré, mouvement autonome + clignements aléatoires, émotions HAPPY/SAD/SURPRISED/SLEEPY/ANGRY/CLOSED, regard L/R/U/D pilotable depuis le scénario JSON (`eye_blink`, `eye_emotion`, `eye_look`)
- [ ] Driver bouche — display à définir ultérieurement (pas e-ink) — à intégrer
- [ ] `ui_manager` bouche : affichage texte mot-à-mot synchro audio

**Web Platform :**
- [x] Projet Next.js 16.2.6 + shadcn/ui initialisé dans `web/` (monorepo, voir `docs/plans/web-implementation.md`)
- [x] Landing « Ouvrez l'œil » + waitlist (server action zod + table `waitlist` RLS insert-only) — déployée sur Coolify (Nixpacks, base dir `web/`, box.agill.es), Supabase self-hosted opérationnel, circuit validé de bout en bout
- [x] Auth (email + Google) + pages login/register/account — déployée et validée en prod (box.agill.es)
- [x] API box : challenge/auth/sync/register/pkg (HMAC par box via HKDF, JWT 2h) — déployée et validée bout-en-bout. Register en **option B** (preuve de possession signée par la box via BLE, 2026-09) ; signatures séparées par domaine (`auth` / `register`)
- [x] Provisioning box : `tools/provision_box.py` (dérive le secret du `BOX_MASTER_SECRET`, écrit `box_creds` dans la partition `box_nvs`) + `tools/box_crypto.py` (source crypto HKDF/HMAC partagée host↔serveur)
- [x] Firmware `hal_box_auth` : lit `box_uid`/`box_secret` dans `box_nvs`, signe `"<purpose>:<uid>:<challenge>"` en HMAC-SHA256 (PSA crypto). Flux challenge→auth→sync validé sur cible
- [x] Appairage `/devices/add` (Web Bluetooth) + provisioning BLE `ble_prov` — reste le parcours E2E navigateur à valider
- [ ] Page catalogue (statique pour commencer) — *reportée après l'API box*
- [ ] Page bibliothèque (scénarios achetés) — *reportée*
- [ ] Stripe checkout (paiement one-shot) + routes `/checkout/success` et `/checkout/cancel` — *reporté*
- [ ] Route Handler `app/api/webhooks/stripe/route.ts` avec vérification signature Stripe (test via `stripe listen`) — *reporté*
- [ ] Route publique `/v/[scenario]/[session]` (leaderboard / résultat QR code)

**Scénario — Processus de création (1 scénario complet) :**

*Contrainte clé EscapeBox : chaque énigme doit être ancrée dans un capteur physique disponible sur la box. Le game design et le hardware design doivent avancer ensemble.*

**Étape 1 — Vision & cadrage**
- [ ] Définir les paramètres fixes : public cible, âge min, nombre de joueurs (min/max), durée (45/60/90 min), difficulté (1-5)
- [ ] Choisir le registre narratif : aventure, mystère, horreur douce, SF, historique…
- [ ] Confirmer la liste de capteurs disponibles pour ce scénario (`hardware_required` + `hardware_enhanced`)
- [ ] Valider le cadrage en 1 page (titre provisoire, pitch 3 phrases, contraintes hardware)

**Étape 2 — Univers & direction artistique**
- [ ] Thème principal, époque, lieux — assez précis pour guider les assets
- [ ] Palette visuelle : couleurs dominantes pour les yeux GC9A01 (iris, sclérotique) + palette pour la bouche (display TBD), typographie narrative
- [ ] Charte LED par état : couleur repos, tension, danger, victoire, indice
- [ ] Ambiance sonore générale : style musical, effets attendus, voix narratrice (oui/non)
- [ ] Moodboard ou références visuelles transmis au scénariste et à l'illustrateur

**Étape 3 — Cahier des charges narratif**
- [ ] Synopsis complet (500-800 mots) : mise en situation, nœud dramatique, résolution
- [ ] Personnages et leur rôle dans la fiction (le joueur est qui ? quel est l'enjeu ?)
- [ ] Arc narratif structuré : intro → énigme 1 → transition → énigme 2 → transition → énigme 3 → révélation finale
- [ ] Logique interne cohérente : chaque énigme doit avoir une justification narrative ("pourquoi je fais ça dans l'histoire ?")

**Étape 4 — Design des énigmes**
- [ ] Pour chaque énigme : nom, description narrative, mécanique de résolution, capteur(s) utilisé(s)
- [ ] Mapping hardware explicite : potentiomètres/faders (ADS7830) → boussole, réglage / souffle (BMP280) → détection / aimant (TMAG5273) → objet posé / keypad → code / téléphone qui lit le tag NFC → révélation, etc.
- [ ] Système d'indices gradués : 3 niveaux par énigme (vague → précis → quasi-solution), déclenchés par timer ou tentatives
- [ ] Temps estimé par énigme (ex : 5 / 10 / 15 min) → total cohérent avec la durée cible
- [ ] Revue du flow : est-ce que l'enchaînement est logique ? Y a-t-il des blocages possibles ?

**Étape 5 — Spécification YAML**
- [ ] Rédiger le fichier YAML complet selon le format FSD §2.3.2 (steps, triggers, hints, do_success, fallbacks)
- [ ] Valider avec `tools/yaml2json.py` — zéro erreur
- [ ] Tester le flow sur le simulateur web (Phase 2) ou à la main sur la box

**Étape 6 — Production des assets**
- [ ] Animations yeux (2× GC9A01 1.3" ronds, 240×240) : émotions, regards, clignements — sprites RGB565 ou rendu procédural
- [ ] Texte bouche (display TBD, pas e-ink) : font lisible, animation mot-à-mot synchro audio
- [ ] Audio : narration intro/transitions/victoire (voix ou synthèse), musique d'ambiance, effets sonores
- [ ] Programmation LED : séquences par état (repos, tension, indice, victoire) — testées sur la box
- [ ] QR code de révélation : URL `escapebox.ch/v/{scenario}/{session}` + page web correspondante

**Étape 7 — Intégration & test technique**
- [ ] Charger assets + YAML sur la SD, vérifier la signature
- [ ] Tester chaque capteur dans le contexte réel du scénario (pas juste en démo)
- [ ] Valider le flow complet bout en bout sans intervention extérieure
- [ ] Vérifier les cas limites : timeout énigme, mauvaise réponse répétée, fallback capteur absent

**Étape 8 — Playtest interne**
- [ ] 3-5 personnes jouent sans explication préalable (cold start)
- [ ] Observer sans intervenir — noter les blocages, les confusions, les moments forts
- [ ] Mesurer : durée réelle, nombre d'indices utilisés, où les gens abandonnent
- [ ] Feedback structuré post-jeu : qu'est-ce qui était flou ? trop facile ? trop dur ?

**Étape 9 — Itération**
- [ ] Ajuster les énigmes trop longues ou trop courtes
- [ ] Revoir les indices si les joueurs ont bloqué > 5 min sans progresser
- [ ] Corriger les incohérences narratives signalées
- [ ] Retester si des changements majeurs ont été faits (retour étape 8)

**Roadmap contenu minimale :**
- 1 scénario pré-chargé en usine (inclus à l'achat)
- Objectif : 2 scénarios disponibles au premier lancement public
- Cadence cible : 1 nouveau scénario tous les 2-3 mois
- Coût de production estimé : 4-6 semaines solo / 2-3 semaines avec illustrateur + compositeur
- Décision scénariste externe : si NPS Phase 1 ≥ 8 et WTP validée

**Box**
- [ ] Faire un prototype et plusieurs itérations
- [ ] Faire des design mockup



**Validation Phase 1 :**

Critères techniques :
- Taux de complétion > 80% sur 10 parties complètes
- Aucun bug bloquant en cours de partie
- Synchro WiFi : scénario téléchargeable en < 5 min

Critères produit — go/no-go Phase 2 :
- WTP mesurée : au moins 5 testeurs annoncent un prix acceptable ≥ 100 CHF
- NPS ≥ 7/10 sur les 15 testeurs
- Au moins 3 personnes prêtes à précommander à prix réel (non amical)
- Feedback spontané de recommandation (sans être sollicité)

> **Décision :** si les critères produit ne sont pas atteints → retravail scénario/boîtier avant de lancer Phase 2. Les critères techniques seuls ne suffisent pas à justifier l'investissement Phase 2.

---

### 3.2 Phase 2 — Proto PCB + Boîtier

**Durée estimée :** 6-12 mois  
**Objectif :** Avoir un produit physique propre prêt pour pré-vente

**Hardware — PCB custom (outil : KiCad 10, pas EasyEDA — bascule décidée en cours de Phase 2) :**

*PCB Main — face Dessous (~100×100mm, 4 couches) :*
- [x] Sheet Power — bq24075 + DW01A/FS8205 + LDOs + boost + USB-C + VBAT_SENSE, câblé et audité
- [x] Sheet ESP32 — module + découplage + reset + strapping + résistances série 22 Ω sur SPI2/SPI3_SCLK (10 pF en DNP), câblé et audité
- [x] Sheet Audio — PCM5122 + PAM8406 + ICS-43434, câblé et audité
- [x] Empreintes des passifs assignées selon BOM sur les 5 sheets (plusieurs erreurs de copié-collé d'empreinte détectées et corrigées en cours de route — footprint d'un CI collée sur un passif voisin)
- [x] Résoudre l'écart U12 → `USBLC6-2SC6` / `Package_TO_SOT_SMD:SOT-23-6` (LCSC C7519), schéma+BOM alignés
- [x] 28 points de test posés (dont UART0, 2026-09-27) (`TestPoint_Pad_D1.5mm`) — liste dans `docs/pcb/03-validation-qualite.md`
- [x] Sortir le sheet NFC (ST25DV04KC-IE6S3) du projet Main — `nfc.kicad_sch` conservé tel quel comme base du futur projet satellite face Dessus
- [x] Ajouter LSM6DSOXTR (IMU) sur Main — `imu.kicad_sch`, pinout vérifié datasheet ST DS12814, mode I2C adresse 0x6A, câblé et audité
- [x] Ajouter 12× WS2812B-B/T (halo face Dessous) sur le sheet Connecteurs — chaîne R8→LED2→...→LED13→J9, un bug de câblage trouvé et corrigé au passage (fil direct R8→J9 en reliquat, label WS2812_DATA mal placé)
- [x] BOM + netlist mis à jour (retrait NFC/ajout IMU/ajout LEDs) — U11 réutilisé pour le LSM6DSOXTR, C4/C5 réutilisés pour son découplage, ST25DV/C4/C5 basculés en ligne "Satellite Dessus"
- [x] **Bouton marche/arrêt** (option B) : EN_SYS sur les EN de U5/U6/U7 + load switch Q1/Q2 sur le rail 5 V, charge USB préservée quand éteint (2026-09-27)
- [x] Corrections de l'audit hardware 2026-09-23 : C20 PCM5122, J10 + ferrites FB3-FB6, pull-ups SD, J3-J6 Qwiic, D1 SS34, R_TMR + NTC, U5 en SO-8 (2026-09-27, vérifiées sur la netlist)
- [x] ERC natif KiCad lancé, 53 erreurs traitées (2026-09-27) — à relancer après chaque modification
- [ ] **Validation schéma main → étape de passage au PCB** : restent les points de `docs/audits/RESTE-A-FAIRE.md` §0 (aucun point bloquant ; placement de l'antenne)
- [ ] Placement des composants sur le PCB (contrainte : zone audio isolée dans un coin, pas de trace digitale dessous) — **guide composant par composant + règles KiCad/JLCPCB : `docs/pcb/04-guide-layout.md`**
- [ ] Routage (plan de masse continu layer 2, I2S court/groupé/blindé, alimentation en priorité)
- [ ] DRC KiCad (clearance 0.2mm, track 0.2-0.5mm, via 0.3/0.6mm — règles JLCPCB)
- [ ] Export Gerbers + BOM + CPL
- [ ] **Validation finale avant envoi JLCPCB** (voir checklist dédiée ci-dessous)

*Satellites par face — un projet KiCad par face créé le 2026-09-27 (`hardware/<face>/`), à dessiner :*
- [ ] **Devant** (visage) : 2×GC9A01 + display bouche + WS2812 + BMP280 + VEML7700 — schéma (dessiné le 2026-09-27 ; embases des modules et nombre de LED provisoires) + layout + Gerbers
- [ ] **Dessus** (voix + NFC) : ST25DV04KC-IE6S3 + antenne boucle PCB (haut-parleur câblé en direct sur J10, hors PCB) — schéma (dessiné le 2026-09-27) + dessin antenne NFC (~4,8 µH, zone de garde) + layout + Gerbers
- [ ] **Côté 1** (panneau de contrôle) : ADS7830 + faders/pots + toggles + boutons — schéma (dessiné le 2026-09-27 ; toggles et boutons en façade, câblés sur JST-PH) + layout + Gerbers
- [ ] **Côté 2** (technique + énigme) : port USB-C + interrupteur + CAP1298 — schéma (USB + tactile dessinés le 2026-09-27 ; SW3 câblé en direct sur J12, hors satellite) + layout + Gerbers
- [ ] **Côté 3** (zone magique) : TMAG5273 seul — schéma (dessiné le 2026-09-27) + layout + Gerbers
- [x] 5 connecteurs I2C sur Main (J3-J6 + J13), un par face satellite (2026-09-27)

*Commun à tous les PCB :*
- [ ] Boîtier Lite : Fusion 360 → impression 3D → découpe laser MDF
- [ ] Assemblage 30-50 boxes proto
- [ ] Certification CE-RED initiée (3-4 mois, ~10k CHF)
- [ ] Test autonomie batterie

**Checklist de validation schéma → PCB (par PCB, avant de passer au layout) :**
- [ ] Tous les sheets du PCB concerné audités (connectivité script + ERC KiCad natif), zéro warning non justifié
- [ ] Toutes les empreintes assignées et vérifiées contre la BOM (aucun champ vide, aucune empreinte d'un autre composant par erreur)
- [ ] BOM à jour : chaque référence du schéma a une ligne BOM correspondante et vice-versa (pas de désynchro de nom comme J2/J10 rencontré cette session)
- [ ] Toutes les décisions de composant ouvertes tranchées (ex : variante U12, adresse I2C ST25DV si bloquant pour le hardware)

**Checklist de validation avant envoi fabrication JLCPCB (par PCB) :**
- [ ] DRC KiCad sans erreur (0 seulement, warnings documentés/justifiés)
- [ ] Gerbers + drill files générés et visuellement inspectés (Gerber viewer) — couches, silkscreen lisible, pas de chevauchement
- [ ] BOM export JLCPCB (`Comment, Designator, Footprint, LCSC Part#`) — toutes les lignes ont un LCSC Part# valide ou sont explicitement marquées DNP/hors-catalogue
- [ ] CPL (Component Placement List) généré et cohérent avec le placement réel
- [ ] Dimensions PCB confirmées (contour, épaisseur, nombre de couches) vs devis JLCPCB
- [ ] Revue humaine finale du footprint 3D (KiCad 3D viewer) pour détecter les collisions mécaniques évidentes
- [ ] Go/no-go explicite de l'utilisateur avant upload sur jlcpcb.com (poste irréversible/payant)

**Firmware :**
- [x] WiFi provisioning via BLE (app-less, page web `/devices/add`) — `ble_prov` ; reste LE Secure Connections et le parcours E2E navigateur
- [x] Synchro : auth + sync + packages versionnés sur SD (manifest sha256, reprise) — `cloud_client`
- [ ] Signature des scénarios (ECDSA) vérifiée avant installation
- [ ] OTA firmware update (HTTPs)
- [ ] USB Mass Storage mode (backup offline)
- [ ] Mode AP de récupération (si WiFi perdu)
- [x] Gestion multi-scénarios sur SD (choix au menu de boot, persisté en NVS)
- [ ] 3 scénarios YAML complets avec audio
- [x] Menu de boot « boutons + visage » (JOUER / SCÉNARIO / APPAIRAGE, keypad) — pas de menu tactile LVGL (§6.4)
- [ ] Réglages on-device minimaux (§6.4.2 : volume, synchro, infos box, reset usine)
- [x] Sync auto au boot non bloquante (fallback offline si pas de WiFi)
- [ ] Sync périodique + reconnexion WiFi après échec
- [ ] Mode dev (déverrouillage 7× version firmware, flag NVS) + Mode Test diagnostic capteurs/actionneurs

**Web Platform :**
- [ ] Système de synchro complet côté serveur
- [ ] Génération paire de clés ECDSA P-256 par box à l'enregistrement (stockée dans Supabase Vault)
- [ ] Pipeline signing : YAML + assets → archive → signature ECDSA → upload R2 sous `/scenarios/{box_uid}/{slug}.enc`
- [ ] Endpoint GET /api/box/sync retourne R2 Presigned URL (expiration 1h)
- [x] Association d'une box (preuve de possession BLE) + liste `/devices`
- [ ] Révocation d'une box (WB-12) et dernière synchro affichée
- [ ] Webhook Stripe → attribution licence immédiate
- [ ] Dashboard joueur complet (bibliothèque, scores, historique)
- [x] Liste d'attente (landing « Ouvrez l'œil »)
- [ ] Pré-commande
- [ ] Simulateur `/studio/[id]/simulate` — player JSON scénario dans le navigateur (state machine JS identique à la logique firmware, events simulés via boutons)

**Scénario — 2 nouveaux scénarios :**
- [ ] Appliquer le processus complet défini en Phase 1 (étapes 1 à 9) pour chaque scénario
- [ ] Cibler des univers distincts du scénario Phase 1 — diversifier les registres narratifs
- [ ] Exploiter au moins 1 capteur différent par scénario (capteurs enhanced : souffle, température IR, aimant, lumière)
- [ ] S'assurer que les 3 scénarios couvrent des niveaux de difficulté variés (ex : 2 / 3 / 4 sur 5)
- [ ] Faire appel à un scénariste externe si le retour Phase 1 confirme la traction produit


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
- [ ] PCB V2 : révision industrialisée de la carte Main (le module WROOM-1-N16R8 est déjà soudé directement dès la Phase 2)
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
- [ ] Simulateur avancé (améliorations Phase 3 — base posée en Phase 2)
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
| ~~HW-04~~ | ~~Le compartiment servo doit s'ouvrir en < 2 secondes~~ — retirée : servos retirés du produit | — | — |
| HW-05 | Les deux écrans doivent être rafraîchis à > 20 FPS simultanément | MUST | 1 |
| HW-06 | Le son doit être audible à 2 mètres dans un environnement normal | MUST | 1 |
| HW-07 | Un téléphone NFC doit lire le tag de la box (ST25DV) à < 3 cm | MUST | 1 |
| HW-08 | Le clavier capacitif doit fonctionner à travers 3 mm de bois/ardoise | SHOULD | 2 |
| ~~HW-09~~ | ~~La rotation du plateau doit être mesurée avec précision ≤ 5°~~ — retirée : AS5600 et plateau rotatif retirés | — | — |
| HW-10 | La box doit démarrer (boot complet) en < 5 secondes | SHOULD | 2 |
| HW-11 | La box doit résister à une utilisation de 1000 parties (fiabilité) | MUST | 3 |

### FR-FW — Firmware

| ID | Exigence | Priorité | Phase |
|---|---|---|---|
| FW-01 | Le moteur de scénario doit exécuter un scénario (JSON généré depuis le YAML) sans recompilation | MUST | 1 |
| FW-02 | La signature ECDSA de chaque scénario doit être vérifiée avant exécution (non réalisé : intégrité par sha256 seulement) | MUST | 1 |
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
| WB-02 | Un scénario acheté doit apparaître dans `/api/box/sync` dans les 10 secondes suivant la confirmation de paiement Stripe (Stripe reporté) | MUST | 1 |
| WB-03 | Le paiement Stripe doit déclencher l'attribution de licence immédiatement (webhook) (Stripe reporté) | MUST | 1 |
| WB-04 | Un compte peut gérer jusqu'à 3 box simultanément | MUST | 2 |
| WB-05 | Les fichiers scénario servis aux box doivent être signés ECDSA côté serveur | MUST | 2 |
| WB-06 | L'éditeur B2B doit permettre de créer un scénario simple en < 1 heure | SHOULD | 3 |
| WB-07 | Le simulateur doit reproduire fidèlement le comportement du firmware | SHOULD | 3 |
| WB-08 | La marketplace doit gérer les reversements aux créateurs (Stripe Connect) | COULD | 3 |
| WB-09 | La webapp doit être disponible en FR, DE et EN | SHOULD | 3 |
| WB-10 | Le dashboard B2B doit afficher l'état en temps réel des boxes d'une flotte | COULD | 3 |
| WB-11 | Les endpoints `/api/box/*` doivent être protégés par rate limiting (max 10 req/min par box_uid) | MUST | 2 |
| WB-12 | Une box peut être révoquée depuis le dashboard (token invalidé immédiatement) | SHOULD | 2 |

---

## 5. Risks, Assumptions & Dependencies

### Risks

| ID | Risque | Impact | Probabilité | Mitigation |
|---|---|---|---|---|
| R-01 | Certification CE-RED refusée ou retardée | Bloquant pour vente EU | Moyen | Anticiper 6 mois, budget 15k CHF, pré-compliance dès la phase 2 |
| ~~R-02~~ | ~~Servos qui tombent en panne après 500 cycles~~ | _(obsolète : servos retirés du produit)_ | — | — |
| R-03 | Firmware OTA brique une box (brick) | SAV exceptionnel | Faible | Double partition OTA (rollback automatique si boot fail), mode recovery USB |
| R-04 | Scénarios piratés et distribués | Perte de revenus | Moyen | Binding par box : chiffrement AES-128-GCM par box + signature ECDSA sur tuple `(contenu + box_uid)`. Champ `bound_to_box_uid` vérifié par le firmware au chargement. |
| R-05 | Dépendance au cloud (serveur down) | Synchro impossible | Moyen | Scénarios déjà téléchargés jouables offline, mode dégradé local |
| R-06 | Manque de contenu (pas assez de scénarios) | Abandon du produit après 1-2 parties | Fort | Roadmap contenu en parallèle hardware, IA pour accélérer production |
| R-07 | PSRAM incompatibilité avec librairies | Bugs affichage/audio | Faible | PSRAM octale 8 Mo (N16R8) validée en Phase 1 (buffers scénario, MP3, framebuffers) ; GPIO35-37 réservées |
| R-08 | WiFi instable (réseau domestique varié) | Synchro échoue | Moyen | Retry automatique, timeout généreux, feedback clair à l'utilisateur |
| R-09 | Prix de vente trop élevé pour le marché | Ventes insuffisantes | Moyen | Valider la willingness-to-pay avec 50 early adopters avant la série |
| R-10 | Bois / ardoise : variabilité matériau | Qualité inconstante | Faible | Fournisseur local certifié, gabarits précis, contrôle qualité entrée |
| R-11 | _(Résolu par changement de composant)_ Ex-limitation iOS (HCE) sur la lecture d'un téléphone par un lecteur NFC | _(obsolète)_ | _(obsolète)_ | Le composant NFC (ST25DV) n'a plus de fonction lecteur du tout — la box ne lit ni téléphone ni badge, dans aucune direction. Seule interaction possible : téléphone → lit la box. Le flux de révélation reste conçu sans dépendance NFC (voir §2.2.2c), interaction téléphone→box via code clavier uniquement. |
| R-12 | NVS flash wear (scores écrits à chaque partie) | Corruption données persistantes après 2-3 ans usage intensif | Faible | Batcher les écritures NVS (accumuler N scores avant flush), utiliser un compteur de séquence. Monitorer la santé NVS en Phase 2. |
| R-13 | Replay attack sur HMAC auth (challenge réutilisé ou généré côté client) | Usurpation d'identité box | Faible-Moyen | Challenge généré côté serveur via `GET /box/challenge`, valide 60s, usage unique (invalidé après première réponse valide), signatures séparées par domaine (`auth` / `register`). Voir §6.1. |

### Assumptions

- L'ESP32-S3-WROOM-1-N16R8 reste disponible et au même prix (ou moins cher) sur 3 ans
- JLCPCB continue à supporter l'assemblage PCBA pour de petites séries (50-500 pièces)
- Supabase reste dans son modèle de pricing actuel pour les petits projets
- Les joueurs ont accès à un réseau WiFi domestique pour la synchro (pas de 4G requise)
- Le format YAML choisi est suffisamment expressif pour couvrir 90% des énigmes imaginables
- Les testeurs Phase 1 sont représentatifs du marché cible final
- _(obsolète, cf. R-11)_ Le composant NFC (ST25DV) n'a plus de fonction lecteur — la box ne peut lire ni téléphone ni badge. Le flux compartiment → QR → code clavier reste conçu pour fonctionner sur tous les appareils, indépendamment du NFC.

### Dependencies

| Dépendance | Type | Impact si défaillante |
|---|---|---|
| Supabase | Cloud (DB + Auth) | Authentification impossible, données perdues |
| Cloudflare R2 | Cloud (CDN) | Téléchargement scénarios impossible |
| Stripe | Cloud (paiements) | Ventes impossibles |
| JLCPCB / LCSC | Fabrication | Production bloquée |
| Espressif ESP32-S3 | Composant | Redesign MCU nécessaire |
| ESP-IDF | Framework | Migration vers autre SDK |
| Anthropic API | IA (éditeur B2B) | Fonctionnalité IA dégradée (non bloquant) |

---

## 6. Interface Specifications

### 6.1 Interface Box ↔ Serveur (REST API)

**Base URL :** `https://box.agill.es/api/box` (dev/FFF — Coolify) · `https://escapebox.ch/api/box` (prod, Phase 3). Pas de sous-domaine API dédié ni de préfixe `/v1` : les routes sont des Route Handlers Next.js sous `app/api/box/`. Les chemins ci-dessous sont relatifs à cette base.

**Authentification :** chaque box s'authentifie par HMAC-SHA256 challenge-response. Son secret est dérivé côté serveur (`box_secret = HKDF-SHA256(BOX_MASTER_SECRET, info="escapebox:<box_uid>", 32)`), le `box_uid` venant de la MAC eFuse (`ESP32S3-XXXX-XXXX`). La box signe `"<purpose>:<box_uid>:<challenge>"` avec `purpose` = `auth` (obtention d'un JWT) ou `register` (preuve de possession à l'appairage) : une signature `register` n'est jamais acceptée par `/auth`. Implémentations alignées : firmware `hal_box_auth`, serveur `web/lib/box-auth.ts`, outils `tools/box_crypto.py`.

**Routes en service** (Route Handlers `web/app/api/box/`) :

```
GET /box/challenge?box_uid=ESP32S3-XXXX-XXXX
Response 200: { "challenge": "<hex>", "expires_in": 60 }     // usage unique, TTL 60 s

POST /box/auth
Request:  { "box_uid", "challenge", "challenge_response": "<HMAC hex de 'auth:<uid>:<challenge>'>" }
Response 200: { "token": "eyJ...", "server_time": 1715000000 }   // JWT box, 2 h
Response 401: challenge invalide/expiré/consommé ou signature invalide
Response 403: box non enregistrée sur un compte

POST /box/register                     // appelé par la webapp (/devices/add), session utilisateur Supabase
Request:  { "box_uid", "name"?, "challenge", "challenge_response": "<HMAC de 'register:<uid>:<challenge>'>" }
          // le challenge vient de /box/challenge, la box le signe via BLE (§6.2)
Response 200: { "device_id": "uuid" }  (ou { device_id, already_owned: true })
Response 401: non authentifié, preuve invalide ; 409 : box d'un autre compte ou limite de 3 box

GET /box/sync?firmware_version=1.2.3
Headers: Authorization: Bearer {token}
Response 200:
  {
    "scenarios": [
      { "id": "uuid", "slug": "capitaine_verdier", "title": "...",
        "package_path": "/api/box/pkg/capitaine_verdier", "version": 4, "installed_at": "..." }
    ],
    "firmware_update": { "version", "url", "sha256", "notes" } | null,   // release active la plus récente
    "server_time": 1715000000
  }

GET /box/pkg/<slug>/<fichier>          // manifest.json puis chaque fichier listé (sha256 par fichier)
Headers: Authorization: Bearer {token} // + droit device_scenarios sur ce slug ; chemin assaini
```

Détail du format des packages et de l'installation incrémentale : `docs/plans/firmware-cloud-client.md`. Publier un scénario : `tools/package_scenario.py`, puis bump de `scenarios.version` en DB.

**Cible (non implémenté)** :

```
POST /box/session                     // remontée des scores (FW-09)
Headers: Authorization: Bearer {token}
Request:  { "scenario_id", "started_at", "duration_sec", "completed", "hints_used", "score" }
Response 200: { "ok": true, "session_id": "uuid" }
```

- Scénarios : signature ECDSA P-256 sur `(contenu + box_uid)`, chiffrement par box et `revoked_scenarios` (R-04, WB-05) ; distribution par URL présignée (R2) au lieu de `/box/pkg`.
- Firmware : `firmware_update` signé (ECDSA) en plus du sha256, vérifié avant OTA.
- Rate limiting `/box/*` (WB-11) et révocation des JWT (WB-12).

### 6.2 Interface Box ↔ App/Webapp (BLE Provisioning)

Utilisé pour la configuration WiFi et la preuve de possession à l'appairage. Composant firmware `ble_prov`, client de référence : page `/devices/add` (Web Bluetooth).

```
Service UUID : e5c40001-5c25-4b10-8f46-6b9c30ac7a11   (UUID propre EscapeBox ; caractéristiques = même base, octets 4-5 = numéro)
  0002 box_uid        READ          "ESP32S3-XXXX-XXXX" (ou "UNPROVISIONED")
  0003 wifi_ssid      WRITE         à écrire AVANT wifi_pass
  0004 wifi_pass      WRITE         déclenche la connexion ("" = réseau ouvert) — write-only, jamais loggé
  0005 status         READ+NOTIFY   "idle" / "connecting" / "wifi_ok" / "wifi_fail"
  0006 auth_challenge WRITE         nonce hex émis par GET /api/box/challenge
  0007 auth_response  READ+NOTIFY   HMAC hex de "register:<uid>:<challenge>" (preuve de possession)
Advertising : "EscapeBox-XXXX", UUID du service en scan response.
```

**Déclencheurs** : boot sans identifiants WiFi en NVS, ou item « APPAIRAGE » du menu de boot. Fenêtre de **5 min**, puis BLE arrêté (RAM du contrôleur rendue).

**Flux d'appairage (`/devices/add`)** :
1. La webapp se connecte en BLE (filtre sur le service), lit `box_uid`.
2. Elle écrit `wifi_ssid` puis `wifi_pass` ; la box tente la connexion et notifie `status`.
3. Sur `wifi_ok` : la webapp demande un challenge au serveur, l'écrit dans `auth_challenge`, lit la signature dans `auth_response` — **juste avant** `/register` (le challenge expire en 60 s).
4. `POST /api/box/register` avec la preuve → la box est rattachée au compte ; la box lance un sync.

> **Sécurité BLE — état actuel** : pas de LE Secure Connections (`sm_sc = 0`, `NO_IO`) : le mot de passe WiFi passe **en clair** sur la liaison radio pendant la fenêtre. La possession de la box est prouvée par le HMAC `register`, pas par l'appairage BLE — et `ble_prov` ne signe jamais `auth`, donc une preuve captée ne donne jamais de JWT. **Cible** : LE Secure Connections, et fermeture de la fenêtre dès `wifi_ok`.

**Re-provisioning (changement de réseau WiFi)** : relancer l'item « APPAIRAGE » du menu de boot. **Cible** : repasser automatiquement en fenêtre BLE après plusieurs échecs WiFi au démarrage, avec un message du personnage.

### 6.3 Interface Utilisateur — Webapp

**Navigation principale (App Router Route Groups) — arborescence cible.** Existent aujourd'hui : `page.tsx` (landing + waitlist), `(auth)/login`, `(auth)/register`, `(app)/account`, `(app)/devices`, `(app)/devices/add`, et les routes `api/box/{challenge,auth,register,sync,pkg}`. Le reste est à construire.

```
app/
  (marketing)/                      → layout public, pas d'auth requis
    page.tsx                        → Landing page / marketing
    activate/[token]/page.tsx       → Activation QR code physique
    v/[scenario]/[session]/page.tsx → Leaderboard / résultat de partie (lien QR box)

  (auth)/                           → layout login/register
    login/page.tsx
    register/page.tsx

  (app)/                            → route group (PAS de segment URL) — guard session via proxy.ts
    library/page.tsx                → /library — Ma bibliothèque (scénarios achetés)
    shop/page.tsx                   → /shop — Catalogue scénarios
    shop/[slug]/page.tsx            → /shop/[slug] — Détail scénario + bouton achat
    checkout/success/page.tsx       → Retour Stripe OK
    checkout/cancel/page.tsx        → Retour Stripe annulé
    devices/page.tsx                → /devices — Mes box (liste, synchro, stats)
    devices/add/page.tsx            → Associer une nouvelle box (BLE provisioning)
    scores/page.tsx                 → /scores — Historique des parties
    account/page.tsx                → /account — Mon compte, abonnement

  studio/                           → vrai segment /studio (pas un route group — guard plan Pro+)
    page.tsx                        → Éditeur B2B
    new/page.tsx
    [id]/edit/page.tsx
    [id]/simulate/page.tsx          → Simulateur (Phase 2)

  api/
    box/
      challenge/route.ts            → GET challenge HMAC
      auth/route.ts                 → POST auth box
      register/route.ts             → POST enregistrement box
      sync/route.ts                 → GET sync scénarios + firmware
      pkg/[slug]/[...path]/route.ts → GET fichiers de package (JWT + droit)
      session/route.ts              → POST upload scores (cible)
    webhooks/stripe/route.ts        → POST webhook Stripe (signature vérifiée)

  marketplace/page.tsx              → Scénarios communautaires (Phase 3)

proxy.ts                            → Next 16 (ex-middleware.ts) : refresh session Supabase +
                                      protection des préfixes réels (/shop, /library, /devices,
                                      /scores, /account, /checkout, /studio)
```

### 6.4 Interface Utilisateur — Box

**Face avant = personnage animé.** Pas de menu tactile on-device (l'ancien ILI9488 + XPT2046 a été retiré). La box affiche un visage piloté par le scénario :

- **Yeux (2× GC9A01)** — `components/ui_manager/eyes_anim.c`, port C ESP-IDF du pipeline « Uncanny Eyes » Adafruit (MIT, Phil Burgess) via le fork GC9A01 de thelastoutpostworkshop. Rendu 128×128 centré dans 240×240, mouvement autonome (drift aléatoire + clignements) en mode IDLE. Actions JSON disponibles dans les scénarios : `eye_blink`, `eye_emotion {type: happy|sad|surprised|sleepy|angry|closed}`, `eye_look {direction: left|right|up|down|center}`. Asset embarqué : `defaultEye.h` (~156 KB flash).
- **Bouche (display TBD, pas e-ink)** — à implémenter. Affichage texte mot-à-mot synchro audio.

**Navigation : boutons physiques + visage.** Le joueur navigue avec les touches capacitives (MPR121 puis CAP1298) — et les deux boutons poussoirs GPIO45/46 du Côté 1 ; le personnage répond par la bouche (texte), la voix (audio) et les yeux. La configuration avancée (compte, langue, renommage) est déportée sur la webapp.

> **Implémenté aujourd'hui** (`firmware/main/boot_menu.c`) : au boot, invite de 4 s (double bip + LEDs douces) ; sans appui → jeu direct (premier déballage sans setup). Un appui → menu au keypad : ◀ = touche 4, ▶ = touche 6, ✓ = touche 11 ; items JOUER, SCÉNARIO (✓ = suivant, persisté en NVS) et APPAIRAGE (fenêtre BLE de 5 min) ; sortie par ✓ sur JOUER ou après 30 s. Le reste de cette section est la **cible**.

**États de la box :**

```
BOOT          → Yeux s'ouvrent + jingle (< 5s)
MENU          → Personnage idle, sélection de scénario (voir 6.4.1)
PLAYING       → Scénario en cours
SYNC          → Texte progression sur la bouche + LEDs pulsées
SETTINGS      → Réglages on-device minimaux (voir 6.4.2)
TEST          → Mode Test capteurs (dev only, voir 6.4.3)
CHARGING      → Indicateur de charge (LEDs + yeux SLEEPY si batterie faible) — ⚠ CHG/PGOOD du
                chargeur ne sont pas reliés à l'ESP32 (budget GPIO) : seule VBAT_SENSE est lisible
ERROR         → Message bouche + QR code support
```

#### 6.4.1 Accueil et lancement d'une partie (MENU)

- **Idle** : le personnage vit (clignements, regards). La bouche affiche le nom du scénario sélectionné.
- **Navigation** : touches capacitives ◀ / ▶ (ou boutons GPIO45/46) pour parcourir les scénarios installés. À chaque changement : titre sur la bouche + annonce vocale (titre, durée, difficulté), yeux qui réagissent.
- **Lancement** : touche ✓ (appui long 1 s) → confirmation vocale « Commencer [titre] ? » → second appui ✓ pour démarrer.
- ~~**Raccourci NFC**~~ : impossible avec le ST25DV, qui est un tag et ne lit ni carte ni téléphone (R-11). Seul sens possible : un téléphone lit la box.
- Si aucun scénario installé : le personnage invite vocalement à synchroniser (texte à l'écran + QR vers la webapp).

#### 6.4.2 Réglages on-device (minimaux)

Pas de menu de réglages écran : seules les actions indispensables sans réseau sont accessibles sur la box, le reste passe par la webapp.

```
Volume              → potentiomètre rotatif dédié (lu via ADS7830 0x48) — registres I2C PCM5122 (0x4C) + amplitude soft
Synchroniser        → touche ⟳ dédiée (ou combinaison) — état affiché sur la bouche
Reconfigurer WiFi   → appui long 5 s sur ⟳ au boot → relance le provisioning BLE (§6.2)
Infos box           → appui long 5 s sur ✓ → bouche affiche version · box_uid · IP/RSSI · espace SD
Reset usine         → appui 10 s sur ⟳ + ✓ simultanés → confirmation vocale + appui ✓ → efface la NVS applicative (jamais `box_nvs`, l'identité de la box)
Langue / renommage  → webapp uniquement (appliqués à la prochaine sync)
```

Les préférences (volume par défaut, langue) sont persistées en **NVS** via `config_manager`.

#### 6.4.3 Mode dev & Mode Test (diagnostic capteurs)

**Déverrouillage (caché aux joueurs)** : depuis l'écran Infos box (§6.4.2), taper **7 fois rapidement** (< 3 s) sur la touche capacitive ✓. Confirmation vocale « Mode dev activé », flag persistant en NVS (`dev_mode=1`). Re-taper 7× le désactive.

**Mode Test** affiche un dashboard temps réel (rafraîchi à la cadence du `sensor_manager`, ~50 ms) de **tous les capteurs/actionneurs**, paginé sur la bouche (navigation ◀ / ▶) et dupliqué sur UART, pour déboguer le hardware sans recompiler :

| Bloc | Données affichées | Phase (capteur) |
|---|---|---|
| Touch capacitif | état des électrodes (bitmap live) + seuils : 12 en Phase 1, 6 touches + proximité en Phase 2 | 1 (MPR121) → 2 (CAP1298) |
| Accéléro / gyro | x/y/z (g), tilt détecté, température | 2 (LSM6DSOX, sur la Main) |
| Luminosité ambiante | lux | 1 (VEML7700) |
| Panneau de contrôle | 4 faders + 4 potentiomètres (0-255), toggles SW1/SW2 | 2 (ADS7830, GPIO1/2) |
| NFC | état RF (champ détecté via poll I2C du registre `ITSTS_DYN`) | 2 (ST25DV) |
| Souffle / pression | pression hPa, seuil détection | 2 (BMP280) |
| Hall / aimant | champ x/y/z, aimant détecté | 2 (TMAG5273) |
| Batterie | VBAT (mV), seuil d'extinction | 2 (VBAT_SENSE) |
| Audio | état I2S, volume courant, flag sous-tension | 1 |
| WiFi | RSSI, IP | 1 |
| Système | heap libre, PSRAM libre, uptime, temp SoC | 1 |

**Actionneurs (boutons de test)** : jouer un ton / MP3 test · cycle LED WS2812 (R/V/B) · flash écran. Permet de valider chaque sortie isolément.

> Le Mode Test ne lance jamais de scénario et n'altère pas la config — c'est un écran de pur diagnostic, invisible pour un joueur final tant que `dev_mode` n'est pas déverrouillé.

#### 6.4.4 Comportement de synchronisation

- **Auto au boot** : si le WiFi est connecté, la box lance une sync **silencieuse et non bloquante** en tâche de fond (Core 0). Sans réseau → sync sautée, la box reste pleinement jouable **offline** avec les scénarios déjà sur SD (R-05). Aucun écran de blocage.
- **Manuelle** : bouton ⟳ du header d'accueil → écran SYNC avec progression.
- **Flux** : auth HMAC challenge-response (§6.1) → JWT 2h → `GET /box/sync` → download des deltas (scénarios manquants / mis à jour) → **vérification signature ECDSA P-256** sur `(contenu + box_uid)` → écriture SD → purge des `revoked_scenarios`.
- **Achat → box (modèle pull)** : un achat sur le site ajoute un droit côté serveur (`device_scenarios`, lié au `device_id` de la box) ; la box le découvre au **prochain `GET /box/sync`**, sans push temps réel. Pour une mise à dispo immédiate, le joueur déclenche une sync manuelle (ou scanne le QR d'activation, §6.3).

---

## 7. Operational Procedures

### 7.1 First-Time Setup / Flashing

#### 7.1.1 Prérequis développement

```bash
# Outils requis
- ESP-IDF v6.1 — fourni par le container Docker du projet (`./start.sh` ou « Reopen in Container »)
- Python 3 (esptool, pyserial — chargés par l'environnement IDF)
- Git
- DevKitC-1 : pont USB CH343 (1a86:55d3) ; carte Main : USB natif de l'ESP32-S3
- Sous WSL2 : usbipd pour exposer /dev/ttyACM0 au container (voir CLAUDE.md)
```

#### 7.1.2 Cloner le repository

```bash
git clone git@github.com:GillesClerc/blackbox.git
cd blackbox
./start.sh        # ou VS Code « Reopen in Container » — l'environnement IDF est chargé par ~/.bashrc
```

**Structure du repository :**

```
blackbox/
├── firmware/
│   ├── main/                    # app_main (main.c), menu de boot (boot_menu.c)
│   ├── components/
│   │   ├── scenario/            # moteur JSON + validateur + cJSON embarqué
│   │   ├── hal_audio/           # PCM5122 (I2C) + I2S + mixer 4 voix (minimp3)
│   │   ├── minimp3/             # décodeur MP3 single-header
│   │   ├── hal_display/         # 2× GC9A01 (SPI3, esp_lcd_gc9a01)
│   │   ├── ui_manager/          # visage : eyes_anim (Uncanny Eyes), ui_face
│   │   ├── hal_leds/            # WS2812 via RMT
│   │   ├── hal_i2c_bus/         # bus I2C partagé (SDA 21 / SCL 17)
│   │   ├── hal_touch/           # MPR121 (Phase 1) / CAP1298 (pilote à écrire)
│   │   ├── hal_imu/             # LSM6DSO(X) — non branché
│   │   ├── hal_light/           # VEML7700 — non branché
│   │   ├── hal_nfc/             # ancien PN532 — obsolète, à réécrire pour le ST25DV
│   │   ├── hal_storage/         # carte SD (SPI2, FAT sur /sdcard)
│   │   ├── config_manager/      # préférences en NVS
│   │   ├── hal_box_auth/        # identité box (box_nvs) + HMAC
│   │   ├── hal_wifi/            # WiFi STA
│   │   ├── cloud_client/        # challenge → auth → sync → packages (OTA à venir)
│   │   └── ble_prov/            # provisioning BLE + preuve de possession
│   ├── scenarios/               # capitaine_verdier.yaml (source) + .json (généré, embarqué)
│   ├── assets/audio/            # ambient.mp3 embarqué (fallback, à retirer pour l'OTA)
│   ├── test_host/               # tests host du validateur (ASan/UBSan)
│   ├── partitions.csv
│   └── sdkconfig                # configuration versionnée (pas de sdkconfig.defaults)
├── hardware/main/               # projet KiCad de la carte Main + BOM de référence (BOM/)
├── web/                         # plateforme Next.js 16 + Supabase
├── docs/                        # FSD, vision, audits, datasheets, plans, pcb
├── tools/                       # yaml2json, package_scenario, provision_box, box_crypto,
│                                #   kicad_netlist, check_bom, pdf_text…
└── CLAUDE.md
```

#### 7.1.3 Configuration ESP-IDF

La configuration de référence est le `firmware/sdkconfig` versionné. Points clés : cible `esp32s3`, CPU 240 MHz, **flash 16 Mo** (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`), **PSRAM octale** (`CONFIG_SPIRAM_MODE_OCT`), table de partitions custom `partitions.csv`, **rollback OTA activé** (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`), NimBLE activé, brownout niveau 7.

**Partition table — état actuel (validé sur cible)** — source de vérité : `firmware/partitions.csv` :

```csv
# Name,    Type, SubType,  Offset,   Size
nvs,       data, nvs,      0x9000,   0x6000     # NVS applicative (config, WiFi, scénario actif)
otadata,   data, ota,      0xF000,   0x2000
nvs_keys,  data, nvs_keys, 0x11000,  0x1000     # clés NVS encryption (prod)
box_nvs,   data, nvs,      0x12000,  0x6000     # identité box (box_creds) — jamais effacée
factory,   app,  factory,  0x20000,  0x300000   # 3 MB — image de secours (reflash USB only)
ota_0,     app,  ota_0,    0x320000, 0x300000   # 3 MB — slot OTA
ota_1,     app,  ota_1,    0x620000, 0x300000   # 3 MB — slot OTA
storage,   data, littlefs, 0x920000, 0x6E0000   # ~6.9 MB LittleFS
```
> Reste pour l'OTA (FW-04) : valider l'image au boot (`esp_ota_mark_app_valid_cancel_rollback()` après un auto-test), et sortir le MP3 embarqué de l'image applicative (binaire à 2,63 Mo pour 3 Mo de partition). La partition `factory` sert d'image de secours : jamais mise à jour par OTA.

#### 7.1.4 Premier flash (USB)

```bash
cd firmware
idf.py set-target esp32s3        # une seule fois
idf.py build

# Flash complet (première fois ou si la table de partitions change) — esptool direct,
# jamais `idf.py flash` (paramètres flash incorrects ici). N'écrit ni nvs ni box_nvs.
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before default-reset --after hard-reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0xf000 build/ota_data_initial.bin \
  0x20000 build/blackbox.bin

# Cas courant (app seule) : même commande avec uniquement `0x20000 build/blackbox.bin`.
# Identité de la box : BOX_MASTER_SECRET=<hex> python3 tools/provision_box.py --port /dev/ttyACM0 --flash
# Monitor série : depuis un terminal hôte (WSL2), pas depuis le container
```

**Logs attendus au démarrage (extrait, état actuel) :**

```
box_auth: box provisionnée: ESP32S3-XXXX-XXXX
hal_audio: PCM5122 … PLL locked
hal_storage: SD montée sur /sdcard
main: scénario SD: /sdcard/scenarios/<slug>/scenario.json
main: EscapeBox ready — yeux OK, audio OK, scenario engine actif
cloud_client: authentifiée auprès de https://box.agill.es
```

#### 7.1.5 Configuration initiale WiFi (via webapp)

1. Box neuve (sans identifiants WiFi) : la fenêtre d'appairage BLE s'ouvre au boot. Box déjà configurée : menu de boot → APPAIRAGE.
2. Aller sur `https://box.agill.es/devices/add` (prod Phase 3 : `escapebox.ch/devices/add`), connecté à son compte.
3. La webapp scanne en Web Bluetooth → sélectionner « EscapeBox-XXXX ».
4. Saisir le réseau WiFi et le mot de passe → la box se connecte (`status` = `wifi_ok`).
5. La webapp fait signer un challenge par la box (preuve de possession) puis l'enregistre sur le compte ; la box lance un sync.

Détail du protocole : §6.2.

> **Compatibilité Web Bluetooth :** Chrome/Edge desktop + Android Chrome. iOS Safari non supporté (WebBluetooth indisponible). Pas encore de solution pour iPhone : mode AP (ci-dessous) ou app native (Phase 3, COULD).

**Procédure manuelle AP mode (fallback) — (cible, non implémenté) :**
```
1. Box en mode provisioning → crée un réseau WiFi "EscapeBox-Setup"
2. Se connecter à ce réseau depuis le téléphone
3. Ouvrir http://192.168.4.1 dans le navigateur
4. Remplir SSID + mot de passe → Valider
5. Box redémarre et se connecte
```

---

### 7.2 OTA Firmware Update

#### 7.2.1 OTA automatique (recommandé en production) — (cible, F6)

Au démarrage de chaque synchro WiFi, la box interroge le serveur pour connaître la dernière version du firmware. Si une mise à jour est disponible :

```
Flux OTA automatique :
1. GET /api/box/sync → { firmware: { version: "1.2.3", url: "...", sha256: "...", ecdsa_signature: "..." } }
2. Box compare avec sa version actuelle
3. Si version_serveur > version_locale :
   a. Afficher sur écran : "Mise à jour disponible (v1.2.3) - Installation..."
   b. Download depuis CDN vers la partition OTA inactive (ota_1 si ota_0 active)
   c. Vérifier SHA256 du fichier téléchargé
   d. Vérifier signature ECDSA P-256 du binaire (clé publique compilée dans le firmware)
   e. Si signature invalide → esp_ota_abort(), annuler, log erreur, continuer sur partition actuelle
   f. esp_ota_set_boot_partition() → pointer vers la nouvelle partition
   g. Redémarrer
   h. Au boot : vérifier que le nouveau firmware démarre (watchdog 30s)
   i. Si OK → valider (esp_ota_mark_app_valid_cancel_rollback())
   j. Si KO → rollback automatique vers la partition précédente
```

> **Toutes les connexions HTTPS** (sync, packages, OTA) utilisent la vérification TLS complète via le **bundle de certificats racine d'ESP-IDF** (`esp_crt_bundle`, magasin Mozilla). Ne jamais passer `skip_cert_common_name_check = true` en production.

**Comportement en cas d'interruption OTA :**
- Si la connexion WiFi est perdue pendant le download : `esp_ota_abort()` est appelé, la partition inactive est marquée invalide, le firmware actuel continue de fonctionner.
- `esp_https_ota` ne supporte pas le resume HTTP Range — la prochaine synchro retente le download depuis le début.
- Les partitions OTA font **3 Mo** (0x300000). Le binaire actuel pèse 2,63 Mo, dont ~1 Mo de MP3 embarqué à sortir de l'image avant F6 ; taille à surveiller en CI avant chaque release.

#### 7.2.2 OTA locale (développement)

`espota.py` est un outil Arduino, indisponible en ESP-IDF. Pour tester l'OTA en local : servir le binaire depuis un serveur HTTP sur le LAN et pointer `esp_https_ota` dessus :

```bash
idf.py build
python3 -m http.server 8070 -d build/    # sur la machine de dev
# La box (mode dev, §6.4.3) télécharge http://192.168.1.xxx:8070/blackbox.bin
```

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
2. MENU : le personnage annonce le scénario sélectionné (bouche + voix)
3. Touches ◀ / ▶ pour naviguer → ✓ pour sélectionner (ou pose de la carte NFC du scénario)
4. Confirmation vocale : "Commencer Le Trésor du Capitaine Verdier ?" → ✓
5. La box charge le JSON depuis la SD → le valide (`scenario_validate`) ; vérification de signature = cible
6. Écrans : animation de démarrage thématique
7. Audio : narration d'introduction
8. Jeu commence → moteur de scénario prend le contrôle
```

#### 7.3.2 Synchronisation manuelle

```
1. Menu principal → "Synchroniser"
2. Box se connecte au WiFi (credentials stockés dans NVS)
3. Auth avec le serveur (HMAC) — toujours faire un GET /box/challenge + POST /box/auth
   (le JWT NVS n'est pas réutilisé — durée de vie 2h, synchros espacées de jours/semaines)
4. Récupère la liste des scénarios autorisés
5. Installe ou met à jour les packages (état actuel, `cloud_client`) :
   - manifest distant (sha256 par fichier) ; chaque fichier est téléchargé dans un
     `.tmp`, vérifié au fil de l'eau, puis renommé ;
   - le manifest local est écrit EN DERNIER (marqueur de commit) : une interruption
     laisse les fichiers complets en place et la synchro suivante reprend là où elle
     s'était arrêtée.
6. (cible) Supprime de la SD les scénarios révoqués
7. (cible) Upload les scores des parties récentes (marqués "envoyés" en NVS uniquement
   après réception du { "ok": true } serveur — évite les doublons en cas d'interruption)
8. (cible, F6) Si `firmware_update` est présent : OTA
9. Déconnexion WiFi
10. Retour au menu
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
3. QR code vers la page de résultat en ligne (escapebox.ch/v/{scenario}/{session})
4. Score sauvegardé localement (SD + NVS)
5. Sera remonté au serveur à la prochaine synchro
6. Retour au menu après 30 secondes
```

**Page de résultat (QR) — micro-enquête post-partie :**
```
- "C'était comment ?"      → 3 boutons emoji  😕 / 😊 / 🤩
- "Difficulté ?"           → Trop facile / Bien dosé / Trop difficile
- "Tu recommanderais ?"    → Oui / Non
```
Ces 3 réponses sont liées à la session (`hints_used`, `duration_sec`, `score`) et remontées au serveur. Elles permettent d'itérer sur les scénarios sans organiser de playtest formel.

---

## 8. Verification & Validation

### 8.1 Phase 1 Verification — Infrastructure

#### 8.1.1 Tests hardware (checklist)

```
[ ] Œil gauche GC9A01 (CS=40) s'affiche : mire couleur, pas d'artefact
[ ] Œil droit GC9A01 (CS=14) s'affiche : mire couleur, pas d'artefact
[ ] Bouche (display TBD) affiche une chaîne de texte correctement
[ ] Haut-parleur (canal L, J10) : MP3 lisible, pas de bruit parasite ; sources stéréo mixées en mono
[ ] Contrôle volume I2C fonctionnel (fade-in/fade-out propre)
[ ] EQ DSP PCM5122 testée sur un scénario (pas de distorsion)
[ ] Micro détecte un claquement de mains à 1 mètre
[ ] ESP32 écrit une URL NDEF sur le ST25DV via I2C, et un smartphone la lit correctement en tapant la box
[ ] MPR121 keypad détecte les 12 touches avec < 1% faux positifs (CAP1298 : même test sur ses 6 touches en Phase 2 PCB)
[ ] ADS7830 : valeur stable, pleine échelle sur les 8 canaux (SL1-SL4 + RV1-RV4)
[ ] Toggles SW1/SW2 : niveaux propres sur GPIO1/2 (pull-up interne)
[ ] LSM6DSOXTR détecte une inclinaison de 15° minimum
[ ] BMP280 détecte un souffle buccal à 5 cm
[ ] VEML7700 distingue pièce éclairée / pièce sombre
[ ] Proximité « approche ta main » (Côté 2, CAP1298 CS8) détecte une main approchée
[ ] WS2812 : toute la chaîne répond, couleurs correctes
[ ] USB-C : programmation ET charge fonctionnels simultanément
[ ] Carte SD : lecture / écriture à > 1 MB/s
[ ] Batterie : tient 90 minutes sous charge normale
[ ] I2C : temps de montée SDA/SCL ≤ 1000 ns à 100 kHz avec tous les satellites branchés (oscilloscope)
[ ] Audio : pas d'écrêtage au volume max plafonné (−22 dB numérique), haut-parleur branché
[ ] Charge crête : LEDs blanc 100 % + audio max + WiFi TX, batterie basse, sans coupure DW01A ni chute du rail 5 V
[ ] Rail 5 V ≤ 5,3 V (max absolu WS2812) sur USB, avec plusieurs chargeurs
[ ] Charge sur un port PC 500 mA : comportement acceptable (limite d'entrée à 1,07 A)
[ ] NTC : charge suspendue au-delà du seuil chaud (J2 en 4 broches, retour NTC sur GND)
[ ] Thermique : AP2112 3V3_D et bq24075 sous USB + charge, boîtier fermé
[ ] Marche/arrêt : consommation éteinte ≤ 20 µA, charge USB fonctionnelle éteinte
[ ] OTA WiFi : mise à jour firmware complète sans intervention physique
```

#### 8.1.2 Tests firmware

```
[ ] Le moteur de scénario parse le JSON généré complet (Verdier) sans erreur
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
[ ] Test mécanique : chutes et secousses du cube (cellule, connecteurs JST, haut-parleur)
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
| Écran blanc | SPI mal câblé / CS/DC inversés | Vérifier SPI3 : MOSI 38, SCLK 39, CS_L 40, CS_R 14, DC 41, RST 42 (GPIO35-37 = PSRAM, jamais utilisables) ; tester avec sketch minimal |
| Pas de son | PCM5122 XSMT flottante / PAM8406 SHDN actif / haut-parleur sur R | Vérifier XSMT → 3V3_A (R5), SHDN → 3V3_A (R_SHDN), haut-parleur sur J10.1-2 (canal L) |
| Une partie du son manque | Source stéréo, un seul haut-parleur (L) | Mixage mono dans le firmware ; vérifier C_PAM_INL/INR |
| Bruit de fond / hiss | Retours de courant du numérique ou du boost sous la zone audio | GND unique et continu (pas de split) ; aucune piste numérique ni boucle du boost sous PCM5122/PAM8406 ; vérifier le découplage |
| Volume ne change pas | I2C addr incorrecte / registres mal configurés | Vérifier 0x4C sur bus I2C, relire registres 61 et 62 |
| Téléphone ne lit pas le tag | Antenne mal accordée / boucle PCB mal dimensionnée | Vérifier la géométrie de la boucle antenne AC0/AC1, mesurer la résonance ~13.56MHz |
| Touch erratique | Paroi trop épaisse / mauvais calibrage | Augmenter le pad cuivre / ajuster threshold firmware |
| Box qui redémarre sur un pic (LEDs, son fort) | Surintensité DW01A (1,6-3,2 A) ou rail 5 V effondré | Baisser les plafonds de luminosité et de volume ; mesurer TP_VBAT1 / TP_5V1 |
| Box qui ne s'allume pas sur batterie | Interrupteur / EN_SYS, ou cellule en décharge profonde (DW01A < 2,4 V) | Vérifier TP_ENSYS1, TP_GATE1 et TP_VBAT1 ; recharger par USB (relâchement du DW01A vers 3,0 V) |
| LEDs éteintes ou couleurs fausses | Rail 5 V coupé (Q1) / timings hors spec V5 | Vérifier TP_5V1 et TP_WS2812 ; timings RMT conformes à `WS2812B-B.md` |

### 9.2 Firmware

| Symptôme | Causes possibles | Solution |
|---|---|---|
| Boot loop | OTA corrompue / stack overflow | Rollback OTA ou reflash USB |
| Scénario refusé au chargement | JSON invalide ou structure rejetée par `scenario_validate` | Régénérer avec `tools/yaml2json.py` (zéro erreur) ; la box retombe sur le scénario embarqué |
| Package refusé (sha256) | Fichier corrompu ou modifié sur le serveur / la SD | Republier avec `tools/package_scenario.py` ; la synchro suivante reprend l'installation |
| WiFi ne connecte pas | Mauvais credentials / réseau 5GHz | Vérifier SSID/pass dans NVS, ESP32 ne supporte que 2.4GHz |
| SD non détectée | Format incorrect / contact SD défaillant | Formater en FAT32, nettoyer les contacts |
| Audio crachotement | Buffer underrun / fréquence incorrecte | Vérifier sample rate (44100 Hz), augmenter buffer |

### 9.3 Web Platform

| Symptôme | Causes possibles | Solution |
|---|---|---|
| Synchro échoue (401 / 403) | Signature invalide (box mal provisionnée, secret serveur différent) / box non enregistrée (403) | Reprovisionner (`tools/provision_box.py`) ; ré-associer la box depuis `/devices/add` |
| Scénario non disponible après achat | Webhook Stripe non reçu | Vérifier les logs Stripe, déclencher manuellement |
| Download lent | CDN Cloudflare / bande passante | Normal à 50 MB/50 Mbps = ~8s. Vérifier si R2 répond |
| BLE provisioning échoue | Navigateur non compatible / fenêtre de 5 min expirée | Utiliser Chrome/Edge desktop ou Android ; rouvrir la fenêtre (menu de boot → APPAIRAGE) |

---

## 10. Appendix

### 10.1 Références matérielles

> **Source de vérité : `docs/datasheets/`** — un PDF officiel + une synthèse `.md` par composant, à lire avant tout travail sur le composant (règle CLAUDE.md). Liens ci-dessous = sources d'origine.

| Composant | Référence | Datasheet |
|---|---|---|
| ESP32-S3-WROOM-1-N16R8 | LCSC C2913202 | https://datasheet.lcsc.com/lcsc/2207151200_Espressif-Systems-ESP32-S3-WROOM-1-N16R8_C2913202.pdf |
| ST25DV04KC-IE6S3 | LCSC C3304276 | https://www.st.com/resource/en/datasheet/st25dv04k.pdf |
| CAP1298 | LCSC C2652072 (SOIC-14) | https://ww1.microchip.com/downloads/en/DeviceDoc/00001571B.pdf (DS00001571B, adresse fixe 0x28) |
| ~~MTCH2120~~ (abandonné 2026-09-27) | — | DS40002613E (aucun stock JLCPCB) |
| ~~AS5600~~ (retiré) | LCSC C79815 | https://ams.com/documents/20143/36005/AS5600_DS000365_5-00.pdf |
| VEML7700 | LCSC C1850416 | https://www.vishay.com/docs/84286/veml7700.pdf |
| BMP280 | LCSC C83291 | https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmp280-ds001.pdf |
| LSM6DSOXTR | LCSC C481766 | https://www.st.com/resource/en/datasheet/lsm6dsox.pdf |
| ~~MLX90614~~ (retiré 2026-09-27) | — | https://www.melexis.com/en/documents/documentation/datasheets/datasheet-mlx90614 |
| PCM5122PW | LCSC C1540085 (BOM) | https://www.ti.com/lit/ds/symlink/pcm5122.pdf |
| PAM8406DR | LCSC C86270 (BOM) | https://www.diodes.com/assets/Datasheets/PAM8406.pdf |
| ICS-43434 | LCSC C5656610 (BOM) | https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf |
| bq24075 | LCSC C15464 | https://www.ti.com/lit/ds/symlink/bq24075.pdf |
| DW01A | — | https://hmsemi.com/downfile/DW01A.PDF |
| FS8205(A) | LCSC C32254 (SOT-23-6 — brochage à confirmer sur le PDF) | https://wmsc.lcsc.com/wmsc/upload/file/pdf/v2/lcsc/1811081616_Fortune-Semicon-FS8205A_C32254.pdf |
| USBLC6-2SC6 | LCSC C7519 | https://www.st.com/resource/en/datasheet/usblc6-2.pdf |
| SS34 (D1, 3 A) | LCSC C8678 | `docs/datasheets/SS34.md` (PDF local Vishay SS32-SS36 ; fiche MDD sur la page LCSC) |
| TMAG5273 (**variante A1**, 0x35) | C3716049 (TMAG5273A1QDBVR ; empreinte SOT-23-6 commune à la famille, fichier nommé C3715882) | https://www.ti.com/lit/ds/symlink/tmag5273.pdf |
| MPR121 (Phase 1) | — | https://cdn-shop.adafruit.com/datasheets/MPR121.pdf |
| GC9A01A (puce driver des yeux) | — | https://github.com/fbiego/dt78/blob/master/datasheets/GC9A01A.pdf (fiche du **module** écran encore absente) |
| TF-01A (slot microSD) | LCSC C91145 | plan mécanique LCSC (pull-ups 10 kΩ exigées côté hôte, cf. ESP-IDF) |
| MT3608 | LCSC C84817 | https://datasheet.lcsc.com/lcsc/XI-AN-Aerosemi-Tech-MT3608_C84817.pdf |
| AP2112M-3.3 (U5, 3V3_D, SO-8) | LCSC C5290219 | `docs/datasheets/AP2112.md` (brochage SO-8 ≠ SOT-25) |
| AP2112K-3.3 (U6, 3V3_A, SOT-25) | LCSC C51118 | https://www.diodes.com/assets/Datasheets/AP2112.pdf |
| WS2812B-B/T | LCSC C2761795 | Datasheet **WS2812B V5** propre à C2761795 : `docs/datasheets/WS2812B-B.pdf` / `.md` (⚠ ≠ ancienne WS2812B C114586 : V_IH 2,7 V et timings T0H/T1L différents) |
| 2N7002 | LCSC C8545 | https://datasheet.lcsc.com/lcsc/Nexperia-2N7002_C8545.pdf |
| AO3401A (Q1, load switch 5 V) | LCSC C15127 | `docs/datasheets/AO3401A.md` |
| BLM21PG221SN1D (FB3-FB6, ferrites HP) | LCSC C85840 | `docs/datasheets/BLM21PG.md` |
| PUI AS04008PO-2-R (haut-parleur 8 Ω, 1 W) | — | `docs/datasheets/AS04008PO.md` |
| ADS7830 (face Côté 1) | LCSC C161747 | `docs/datasheets/ADS7830.md` |
| E-Switch série 100 (SW1-SW2, finition or) | — | `docs/datasheets/E-Switch-100-series-toggle.md` |
| E-Switch RR111C1921 (SW3, marche/arrêt) | DigiKey / Mouser | `docs/datasheets/E-Switch-RR1.md` |
| Bourns PTA6043 / PDB181 (faders, pots) | LCSC C17203852 / C6251250 | `docs/datasheets/Bourns-PTA6043.md`, `Bourns-PDB181.md` |
| Sunlord SWPA5040S6R8MT (L1, boost, 6,8 µH) | LCSC C36411 | `docs/datasheets/SWPA5040S.md` |
| Cellule 18650 + NTC | **à choisir** | — |

### 10.2 Liens utiles

| Ressource | URL |
|---|---|
| ESP32-S3 Datasheet | https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf |
| ESP32-S3 DevKitC-1 User Guide | https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide.html |
| ESP32-S3 DevKitC-1 Schéma (open source) | https://github.com/espressif/esp-dev-kits/tree/master/esp32-s3-devkitc-1 |
| ESP-IDF (programmation) | https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/ |
| KiCad | https://docs.kicad.org |
| JLCPCB PCB + PCBA | https://jlcpcb.com |
| LCSC Components | https://www.lcsc.com |
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
| PWM | Pulse Width Modulation — modulation de largeur d'impulsion |
| PSRAM | Pseudo-Static RAM — mémoire RAM externe rapide |
| RFID | Radio Frequency Identification — identification par radiofréquence |
| SoC | System on Chip — puce intégrant CPU, mémoire, périphériques |
| SPI | Serial Peripheral Interface — bus de communication série 4 fils |
| YAML | YAML Ain't Markup Language — format de données lisible par l'humain |

---

*FSD v0.3 — Document vivant. Ce qui reste à faire : `docs/audits/RESTE-A-FAIRE.md`.*
