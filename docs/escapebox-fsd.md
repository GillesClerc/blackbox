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
- Chaque scénario est **signé cryptographiquement** (protection contre la copie)
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

> **Deux couches à ne pas confondre.** Côté web, le *proxy* Next.js (`proxy.ts`, ex-`middleware.ts`) n'intercepte que les requêtes du **navigateur** (auth, redirections) — il ne parle jamais à la box. Côté matériel, la liaison box↔cloud est une **couche de synchronisation device-to-cloud** : la box est un simple client HTTPS qui appelle l'API REST (§6.1) **à la demande** (sync au boot si WiFi + sync manuelle), jamais en connexion permanente. Aucun framework dédié type ESP RainMaker n'est utilisé — le backend e-commerce (Supabase + Stripe) impose un protocole propre.

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
| 0x10 | VEML7700 | Lumière ambiante | Satellite capteurs |
| 0x28 | MTCH2120 | Capacitif 12 canaux (keypad + zones touch) — cible PCB Phase 2 | Satellite capteurs |
| 0x5A | MPR121 | Capacitif 12 canaux (breakout Phase 1, même rôle que MTCH2120) | Proto breadboard |
| 0x24 | PN532 | Lecteur NFC (I2C mode — 0x24 en 7 bits, parfois noté 0x48 en 8 bits) | Main |
| 0x4C | PCM5122PW | DAC audio stéréo (I2C contrôle) | Main |
| 0x5C | MLX90614 | Température IR (sans contact) — adresse usine 0x5A, reprogrammée en 0x5C via EEPROM (évite collision MPR121) | Satellite capteurs |
| 0x6A | LSM6DSOXTR | Accéléromètre + gyroscope 6 axes + MLC | Satellite capteurs |
| 0x35 | TMAG5273 | Hall linéaire 3D (distance/angle aimant) | Satellite capteurs |
| 0x76 | BMP280 | Pression / détection souffle | Satellite capteurs |
| 0x48 | ADS7830 | ADC 8 canaux 8 bits — 4 faders SL1-SL4 + 4 pots RV1-RV4 (ratiométrique, REFIN=3V3) | Face avant |

> **Composants retirés du Phase 1** : AS5600 (rotation magnétique), servos SG90, laser. Remplacés par potentiomètres rotatifs mécaniques + interactions software.

**Bus SPI2** (e-ink bouche — IOMUX GPIO11/12 pour perf DMA) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO11 | MOSI | SSD1680 e-ink 2.9" |
| GPIO12 | SCLK | SSD1680 e-ink 2.9" |
| GPIO10 | CS | SSD1680 chip select |
| GPIO9  | DC | SSD1680 data/command |
| GPIO8  | RST | SSD1680 reset |
| GPIO13 | BUSY | SSD1680 busy (active low, lecture seule) |

> Écran e-ink 2.9" (296×128, SSD1680) en mode paysage, utilisé comme "bouche" du personnage. Partial refresh 0.3s pour affichage texte mot-à-mot. GPIO14-15 libérés (plus de XPT2046). Driver Espressif officiel : `esp_lcd_ssd1681` (compatible SSD1680).

**Bus SPI3** (yeux — 2× GC9A01 ronds) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO38 | MOSI | GC9A01 ×2 (partagé) |
| GPIO39 | SCLK | GC9A01 ×2 (partagé) |
| GPIO40 | CS_EYE_L | GC9A01 œil gauche |
| GPIO41 | DC | GC9A01 ×2 (partagé) |
| GPIO42 | RST | GC9A01 ×2 (partagé) |
| GPIO14 | CS_EYE_R | GC9A01 œil droit |

> Deux écrans ronds 1.3" 240×240 formant les "yeux" du personnage. MOSI/SCLK/DC/RST partagés, seul le CS distingue les deux écrans. GPIO14 récupéré (ex-CS XPT2046) pour le 2ème CS. SPI 80 MHz, animations ~15-25 FPS par œil en alternance.

**Bus I2S0** (audio sortie → PCM5122) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO4 | BCLK | PCM5122PW |
| GPIO5 | LRCLK | PCM5122PW |
| GPIO6 | DOUT | PCM5122PW |

> PCM5122PW en mode I2C (MODE pins à GND). PLL depuis BCK. I2S : 44100 Hz, 16-bit, 32-bit slots (BCLK = 2.82 MHz), Philips standard. Volume digital 0 dB par défaut (reg 0x3D/0x3E = 0x30). Sortie analogique OUTL/OUTR → filtre RC (100Ω + 22nF, fc≈72 kHz) → PAM8406. Voir `docs/datasheets/pcm5122-registers.md` pour la référence registres complète.
>
> Le **PAM8406** (Class D 5W+5W stéréo) est purement analogique, pas de driver. SHDN tiré haut (toujours actif). Le mute se fait via le registre PCM5122 (0x03). Gain fixe 24 dB — les amplitudes sont contrôlées numériquement côté firmware (MP3_BG_VOLUME, registre volume digital).

**Bus I2S1** (audio entrée — micro MEMS) :

| GPIO | Signal | Composant |
|---|---|---|
| GPIO16 | SCK | ICS-43434 micro MEMS |
| GPIO18 | WS | ICS-43434 |
| GPIO7  | SD | ICS-43434 |

> L'ICS-43434 est un micro MEMS numérique I2S (24-bit, 43 kHz BW). Monté sur le PCB main, trou dans le boîtier. Pin L/R à GND → canal gauche. GPIO16/18/7 choisis pour éviter les conflits avec I2C (GPIO17/21) et XPT2046 (GPIO15).

**GPIO individuels** :

| GPIO | Fonction | Composant | Notes |
|---|---|---|---|
| GPIO1 | Toggle SW1 | E-Switch 100SP SPDT (face avant, via J8) | Input, pull-up interne |
| GPIO2 | Toggle SW2 | E-Switch 100SP SPDT (face avant, via J8) | Input, pull-up interne |
| GPIO3 | Réserve | (libre — analogique migré sur ADS7830 I2C) | ADC1_CH2 dispo si besoin |
| GPIO19 | USB D- | USB-C natif | — |
| GPIO20 | USB D+ | USB-C natif | — |
| GPIO45 | Bouton / reserve | Strapping pin (attention) | Input avec pull-up ext |
| GPIO46 | Bouton / reserve | Input only | — |
| GPIO47 | Réserve | (libre — Hall TMAG5273 migré sur I2C) | — |
| GPIO48 | WS2812 DATA | Chaîne LEDs RGB | RMT driver |

> Les boutons mécaniques peuvent être gérés via les 12 canaux du MTCH2120 (capacitif, fonctionne aussi avec boutons conducteurs) ou directement via GPIO45/46 pour des boutons poussoirs simples.

**Récapitulatif GPIO complet (WROOM-1-N16R8)** :

```
GPIO 0      — (strapping boot, réservé)
GPIO 1-2    — Toggles SW1/SW2 (face avant, via J8)
GPIO 3      — (réserve, ADC1_CH2 dispo — analogique migré sur ADS7830 I2C)
GPIO 4-6    — I2S0 audio out (PCM5122)
GPIO 7      — I2S1 SD (ICS-43434 micro)
GPIO 8-13   — SPI2 e-ink bouche (SSD1680 : CS/DC/RST/MOSI/SCLK/BUSY)
GPIO 14     — CS œil droit (GC9A01 #2, bus SPI3)
GPIO 15     — (réserve, libre — ex-XPT2046 IRQ)
GPIO 16     — I2S1 SCK (ICS-43434 micro)
GPIO 17     — I2C SCL
GPIO 18     — I2S1 WS (ICS-43434 micro)
GPIO 19-20  — USB-C
GPIO 21     — I2C SDA
GPIO 26-37  — ⛔ Flash/PSRAM (non disponible)
GPIO 38-42  — SPI3 yeux (2× GC9A01 : MOSI/SCLK/CS_L/DC/RST partagés)
GPIO 43-44  — UART0 debug
GPIO 45-46  — Boutons / réserve
GPIO 47     — (réserve, libre)
GPIO 48     — WS2812 LEDs
```

#### 2.2.2b Alimentation

```
USB-C (5V, 2A max)
    ↓
bq24075 (TI) — chargeur 1.5A + power path DPPM
    ├── BAT → DW01A + FS8205 → LiPo 3.7V 3000mAh
    └── OUT (VSYS : ~4.4V sur USB, ~VBAT sur batterie)
         ├── AP2112K-3.3 #1 → 3.3V_D (digital : ESP32, écrans, capteurs I2C, micro)
         ├── AP2112K-3.3 #2 → 3.3V_A (audio : PCM5122, zone isolée)
         └── MT3608 boost → 5V (WS2812 LEDs)
```

> **Power path (DPPM)** : le système est alimenté en priorité par l'USB, le surplus charge la batterie. La box peut rester branchée sans user la batterie. **Deux LDOs séparés** pour isoler le bruit digital du chemin audio. GND unique continu (PAS de split). Voir `docs/schematics/06-power-audio.txt` pour le schéma détaillé.

#### 2.2.2c Assignation des faces — Cube 150×150×150mm *(dimension à valider au proto boîtier — la vision mentionne 120mm)*

> Les capteurs et interactions ne sont pas forcément 1:1 avec les faces. Plusieurs capteurs peuvent cohabiter sur une même face, et un même type d'interaction peut traverser plusieurs faces.

| Face | Rôle principal | Composants |
|---|---|---|
| **Devant** | Visage du personnage | 2× GC9A01 1.3" ronds (yeux) + e-ink 2.9" SSD1680 (bouche), WS2812 rétro |
| **Dessus** | Interactions principales | MTCH2120 keypad capacitif, potentiomètres, boutons |
| **Droite** | Capteurs ambiance | VEML7700 lumière, BMP280 souffle |
| **Gauche** | NFC + détection | PN532 (antenne derrière bois ≤3mm), TMAG5273 Hall I2C, WS2812 anneau |
| **Arrière** | Capteurs + réserve | LSM6DSOXTR (orientation), MLX90614 IR temp |
| **Dessous** | Technique | USB-C charge, interrupteur, HP (PAM8406 + haut-parleur) |

> Assignation indicative. Les capteurs sur satellite I2C peuvent être repositionnés librement tant qu'ils restent sur le bus backbone.

**Concept "personnage" :**
```
La face avant est un VISAGE :
  - 2× GC9A01 ronds = yeux expressifs (clignements, regard, émotions)
  - 1× e-ink 2.9" = bouche (texte mot-à-mot, style Animal Crossing)
  - WS2812 = halo lumineux ambiance autour du visage

Le personnage parle : texte e-ink + syllabes audio AC synchronisées.
Les yeux réagissent en temps réel aux capteurs et aux actions du joueur.
Les énigmes sont "données" par le personnage (dialogue + feedback visuel).
```

> **Note NFC téléphone :** L'interaction téléphone → PN532 (téléphone agit comme tag) est **impossible sur iPhone** (limitation hardware iOS, HCE restreint aux paiements).

#### 2.2.3 Architecture PCB — Main + Satellites

**PCB Main** (~100×100mm, 4 couches) :

Composants embarqués :
- ESP32-S3-WROOM-1-N16R8 (soudé)
- Alimentation complète : bq24075 (power path) + DW01A + FS8205, 2× AP2112K-3.3 (digital + audio), MT3608 boost
- PCM5122PW DAC + découplage (zone audio isolée)
- PAM8406 Class D ampli + filtre RC sortie
- ICS-43434 MEMS micro (trou PCB pour son)
- PN532 NFC (antenne PCB intégrée ou FPC externe)
- Connecteur USB-C (charge + USB CDC debug)
- Connecteur JST-SH 6 pins pour e-ink 2.9" SSD1680 (SPI2 : MOSI, SCLK, CS, DC, RST, BUSY)
- Connecteur batterie LiPo JST-PH 2 pins
- 4× connecteurs JST-SH 4 pins (bus I2C : SDA, SCL, 3.3V, GND)
- 1× connecteur JST-SH 10 pins (SPI3 + alim pour 2× GC9A01 : MOSI, SCLK, CS_L, CS_R, DC, RST, 3.3V, GND, +2 rsv)
- 1× connecteur JST-SH 6 pins (ADC pots + 3.3V + GND)
- 1× connecteur JST-SH 4 pins (WS2812 data + 5V + GND + signal Hall)

**PCB Satellite Capteurs** (~30×50mm, 2 couches) :

Composants embarqués :
- MTCH2120 capacitif 12 canaux (pads vers faces via FPC souple)
- LSM6DSOXTR accéléromètre/gyro + MLC
- VEML7700 capteur lumière (avec ouverture)
- BMP280 pression/souffle (avec ouverture)
- MLX90614 IR température (avec fenêtre IR)
- TMAG5273 Hall linéaire 3D I2C
- Connecteur JST-SH 4 pins vers bus I2C main

> Tous les capteurs partagent le même bus I2C. Adresses uniques confirmées (aucun conflit).

**PCB Satellite Interaction** (taille selon la face, 2 couches) :

Composants à câbler (pas forcément sur PCB dédié) :
- 4 faders SL1-SL4 + 4 pots rotatifs RV1-RV4 → ADS7830 (ADC I2C 0x48 sur PCB face avant)
- 2 toggles SW1/SW2 (vers GPIO1/2 main, via J8)
- Boutons mécaniques (via MTCH2120 ou GPIO direct)
- WS2812 LEDs (chaîne série depuis GPIO48)
- 2× GC9A01 1.3" ronds (SPI3 depuis main, CS séparés)
- E-ink 2.9" SSD1680 (SPI2 depuis main)
- Haut-parleur (câbles depuis PAM8406 sur main)

**Connectique backbone** :

JST-SH (1.0mm pitch, verrouillable) pour les connecteurs signaux inter-PCB ; JST-PH (2.0mm, 3A/contact) pour la puissance (J9 LEDs, J10 batterie) :
- 4 pins I2C : VCC(3.3V), GND, SDA, SCL
- 10 pins SPI3 yeux : VCC(3.3V), GND, MOSI, SCLK, CS_L, CS_R, DC, RST, (rsv×2)
- 6 pins SPI2 e-ink : VCC(3.3V), GND, MOSI, SCLK, CS, DC (RST+BUSY sur main)
- 6 pins toggles (J8) : VCC(3.3V), GND, SW1, SW2, GPIO3(rsv), (rsv)
- 4 pins LED (J9, JST-PH — jusqu'à ~1.9A sur 5V) : VCC(5V), GND, WS2812_DATA, (rsv)

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
│  │  display_manager (esp_lcd : 2× GC9A01, SSD1680 e-ink)   │    │
│  │  led_manager     (WS2812 via RMT ESP-IDF)               │    │
│  │  servo_manager   (MCPWM ESP-IDF — Phase 2)               │    │
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

**Composants / librairies :**

| Composant | Usage |
|---|---|
| ESP-IDF v6.1 | Framework de base (FreeRTOS, drivers, HAL) |
| LVGL | Non utilisé actuellement (rendu direct esp_lcd) — dépendance conservée pour usage futur éventuel |
| esp_lcd | Drivers écrans (SPI : GC9A01 ×2, SSD1680 e-ink) |
| led_strip via RMT | WS2812 LEDs (driver natif ESP-IDF) |
| MCPWM | Servos (driver natif ESP-IDF) |
| cJSON | Parsing config / API (inclus ESP-IDF) |
| minimp3 | Décodeur MP3 single-header — musique de fond en boucle, tâche FreeRTOS bg (stack 24 KB). ESP-ADF évalué en Phase 2+ uniquement si un vrai pipeline multi-format/streaming devient nécessaire. |
| i2c_master (PCM5122) | Config DAC : PLL, volume, filtre, mute via registres I2C (addr 0x4C) |
| i2c_master | MTCH2120 keypad, PN532 NFC, LSM6DSOXTR, TMAG5273 |
| NimBLE (ESP-IDF) | Provisioning WiFi via BLE |
| esp_https_ota | OTA HTTPS |
| esp_http_client | Download scénarios HTTPS |
| mbedTLS (intégré) | Vérification ECDSA P-256 (secp256r1 + SHA-256). Clé publique compilée en dur dans le firmware (`const uint8_t[]`), non chargée depuis la SD. |

#### 2.3.2 Format des scénarios (YAML)

Chaque scénario est un fichier YAML décrivant une machine à états finis (state machine). Le YAML est le **format auteur** (source de vérité) ; il est converti en **JSON** par `tools/yaml2json.py` (avec validation de schéma), et c'est ce **JSON** que le firmware embarque et parse (via cJSON) au démarrage — sans aucune logique codée en dur. Le `.json` est un **artefact généré** : ne jamais l'éditer à la main, régénérer depuis le YAML.

> Note pipeline : `yaml2json.py` désactive la résolution YAML 1.1 de `on/off/yes/no` en booléens, car le DSL utilise `on:` (déclencheur d'événement) et `off` (mode LED) comme chaînes.

> ⚠️ L'exemple ci-dessous illustre le DSL **cible**. Certaines constructions (`type: combo` + `require_all`, hints `after_sec`/`after_attempts`, `do_fail_max`, `screen_compass`) ne sont **pas encore** implémentées. Le DSL réellement supporté par `yaml2json.py` et le firmware aujourd'hui : types `narrative/trigger/input/branch/end`, hints `delay_sec`, échec `do_fail`, actions `screen_main/screen_secondary/audio/led/servo/flash/set_var/incr_var`.

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
| Frontend | Next.js 16 (App Router), Tailwind CSS, shadcn/ui |
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

### 3.0 Budget & planning

**Budget Phase 1 (enveloppe 500-2000 CHF) :**

| Poste | Estimation basse | Estimation haute |
|---|---|---|
| Composants électroniques | 300 CHF | 500 CHF |
| Matériaux boîtier (impression 3D, peinture) | 100 CHF | 200 CHF |
| Assets son & illustration | 0 CHF | 700 CHF |
| Infrastructure web (Supabase free, Vercel free) | 0 CHF | 0 CHF |
| Imprévus / itérations | 100 CHF | 300 CHF |
| **Total** | **500 CHF** | **1700 CHF** |

**Jalons Phase 1 (cible : août-septembre 2026) :**

| Jalon | Date cible | Critère de succès | Statut |
|---|---|---|---|
| M0 — Lancement | Mai 2026 | Hardware commandé, firmware validé dev board | ✅ |
| M1 — Hardware | Juin 2026 | ESP32-S3 reçu, drivers principaux validés (display, audio, touch, LEDs) | ✅ (partiel — NFC, capteurs restants en attente) |
| M2 — Scénario + Boîtier | Juillet 2026 | Scénario chargé, prototype boîtier V1 jouable | ☐ |
| M3 — Playtest FFF | Août-Septembre 2026 | 10-15 testeurs, go/no-go Phase 2 décidé | ☐ |

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
- [ ] SSD1680 e-ink 2.9" (bouche, SPI2) → driver à intégrer (`esp_lcd_ssd1681`)
- [x] Câbler PCM5122PW (I2S + I2C 0x4C) + PAM8406 + speakers 8Ω/5W → audio validé (PLL, filtre, MP3 bg)
- [x] Câbler MPR121 breakout → keypad capacitif 12 canaux validé (I2C 0x5A)
- [x] Câbler WS2812 → LEDs validées (RMT, GPIO48)
- [ ] Câbler PN532 breakout → valider NFC
- [ ] Câbler MTCH2120 → valider keypad capacitif (Phase 2 PCB)
- [ ] Câbler servo SG90 → valider compartiment (Phase 2)
- [ ] Câbler LSM6DSOXTR, BMP280, VEML7700, TMAG5273 → valider I2C bus complet
- [ ] Câbler microphone MEMS I2S → valider micro
- [ ] Tester le moteur de scénario YAML sur le hardware assemblé complet

**Firmware :**
- [x] Moteur de scénario JSON (state machine, hints, variables, branches, do_fail)
- [x] Scénario "Capitaine Verdier" — YAML + JSON, 12 steps, 3 énigmes (boussole, code, inclinaison)
- [x] Driver audio PCM5122PW — I2S 32-bit slots + I2C PLL config, filtre ringing-less FIR, volume -6dB hw
- [x] Musique de fond MP3 — minimp3 décodage tâche bg (24 KB stack), MP3 embarqué en flash
- [x] Driver yeux 2× GC9A01 SPI3 partagé (CS_L=40, CS_R=14, MOSI=38, SCLK=39, DC=41, RST=42) — wrapper `components/display/eyes.[ch]` autour de `esp_lcd_gc9a01` v2.0.4, 240×240 RGB565 @ 40 MHz
- [ ] Driver bouche e-ink SSD1680 (`esp_lcd_ssd1681`) — à intégrer
- [x] MPR121 tactile capacitif 12 canaux — validé DevKitC-1 (I2C 0x5A, 100kHz, SDA=21/SCL=17)
- [x] Driver LEDs WS2812B (RMT, GRB, show) — validé
- [x] Drivers I2C (LSM6DSOXTR, TMAG5273, VEML7700, MTCH2120, MPR121) — écrits
- [x] Outil YAML→JSON (tools/yaml2json.py avec validation)
- [x] Driver NFC PN532 (écrit — validation hardware en attente)
- [x] Driver servos SG90 MCPWM (écrit — Phase 2)
- [x] Système de fichiers SD SPI+FAT (écrit — validation hardware en attente)
- [x] App scénario principale (main.c) — JSON embarqué, callbacks audio/led/eye_*, keypad MPR121, hold 2s pour simuler rfid/rotary/tilt
- [x] Flash 8 MB config avec partition custom (7.9 MB app, supporte MP3 4+ MB)
- [x] `ui_manager` v2 — animation yeux (Uncanny Eyes Adafruit MIT porté ESP-IDF) : 2× GC9A01, rendu 128×128 centré, mouvement autonome + clignements aléatoires, émotions HAPPY/SAD/SURPRISED/SLEEPY/ANGRY/CLOSED, regard L/R/U/D pilotable depuis le scénario JSON (`eye_blink`, `eye_emotion`, `eye_look`)
- [ ] Driver bouche e-ink SSD1680 (`esp_lcd_ssd1681`) — à intégrer
- [ ] `ui_manager` bouche : affichage texte mot-à-mot synchro audio

**Web Platform :**
- [ ] Projet Next.js 16 + Supabase initialisé (Route Groups : `(marketing)`, `(auth)`, `(app)` + segment réel `studio/` + `proxy.ts` Supabase — voir `docs/plans/web-implementation.md`)
- [ ] Auth (email + Google)
- [ ] Page catalogue (statique pour commencer)
- [ ] Page bibliothèque (scénarios achetés)
- [ ] Stripe checkout (paiement one-shot) + routes `/checkout/success` et `/checkout/cancel`
- [ ] Route Handler `app/api/webhooks/stripe/route.ts` avec vérification signature Stripe (test via `stripe listen`)
- [ ] API sync basique (liste des scénarios autorisés)
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
- [ ] Palette visuelle : couleurs dominantes pour les yeux GC9A01 (iris, sclérotique) + niveaux de gris pour la bouche e-ink, typographie narrative
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
- [ ] Mapping hardware explicite : rotation (AS5600) → boussole / souffle (BMP280) → détection / NFC → objet physique / keypad → code, etc.
- [ ] Système d'indices gradués : 3 niveaux par énigme (vague → précis → quasi-solution), déclenchés par timer ou tentatives
- [ ] Temps estimé par énigme (ex : 5 / 10 / 15 min) → total cohérent avec la durée cible
- [ ] Revue du flow : est-ce que l'enchaînement est logique ? Y a-t-il des blocages possibles ?

**Étape 5 — Spécification YAML**
- [ ] Rédiger le fichier YAML complet selon le format FSD §2.3.2 (steps, triggers, hints, do_success, fallbacks)
- [ ] Valider avec `tools/yaml2json.py` — zéro erreur
- [ ] Tester le flow sur le simulateur web (Phase 2) ou à la main sur la box

**Étape 6 — Production des assets**
- [ ] Animations yeux (2× GC9A01 1.3" ronds, 240×240) : émotions, regards, clignements — sprites RGB565 ou rendu procédural
- [ ] Texte bouche (SSD1680 e-ink 2.9", 296×128) : font lisible, animation mot-à-mot synchro audio
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
- [ ] Menu on-device tactile LVGL : accueil (sélection scénario) + Réglages (volume, luminosité, langue, reconfig WiFi, diagnostic réseau, infos box, reset usine)
- [ ] Sync auto au boot non bloquante (fallback offline si pas de WiFi)
- [ ] Mode dev (déverrouillage 7× version firmware, flag NVS) + Mode Test diagnostic capteurs/actionneurs

**Web Platform :**
- [ ] Système de synchro complet côté serveur
- [ ] Génération paire de clés ECDSA P-256 par box à l'enregistrement (stockée dans Supabase Vault)
- [ ] Pipeline signing : YAML + assets → archive → signature ECDSA → upload R2 sous `/scenarios/{box_uid}/{slug}.enc`
- [ ] Endpoint GET /api/box/sync retourne R2 Presigned URL (expiration 1h)
- [ ] Gestion des devices (association, liste, dernière synchro, révocation)
- [ ] Webhook Stripe → attribution licence immédiate
- [ ] Dashboard joueur complet (bibliothèque, scores, historique)
- [ ] Pré-commande / liste d'attente
- [ ] Simulateur `/studio/[id]/simulate` — player JSON scénario dans le navigateur (state machine JS identique à la logique firmware, events simulés via boutons)

**Scénario — 2 nouveaux scénarios :**
- [ ] Appliquer le processus complet défini en Phase 1 (étapes 1 à 9) pour chaque scénario
- [ ] Cibler des univers distincts du scénario Phase 1 — diversifier les registres narratifs
- [ ] Exploiter au moins 1 capteur différent par scénario (capteurs enhanced : souffle, température IR, rotation plateau)
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
| WB-02 | Un scénario acheté doit apparaître dans `/api/box/sync` dans les 10 secondes suivant la confirmation de paiement Stripe | MUST | 1 |
| WB-03 | Le paiement Stripe doit déclencher l'attribution de licence immédiatement (webhook) | MUST | 1 |
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
| R-02 | Servos qui tombent en panne après 500 cycles | SAV + mauvaises reviews | Moyen | Utiliser MG90S (métal), tester 2000 cycles, stock pièces de rechange |
| R-03 | Firmware OTA brique une box (brick) | SAV exceptionnel | Faible | Double partition OTA (rollback automatique si boot fail), mode recovery USB |
| R-04 | Scénarios piratés et distribués | Perte de revenus | Moyen | Binding par box : chiffrement AES-128-GCM par box + signature ECDSA sur tuple `(contenu + box_uid)`. Champ `bound_to_box_uid` vérifié par le firmware au chargement. |
| R-05 | Dépendance au cloud (serveur down) | Synchro impossible | Moyen | Scénarios déjà téléchargés jouables offline, mode dégradé local |
| R-06 | Manque de contenu (pas assez de scénarios) | Abandon du produit après 1-2 parties | Fort | Roadmap contenu en parallèle hardware, IA pour accélérer production |
| R-07 | PSRAM incompatibilité avec librairies | Bugs affichage/audio | Faible | Tester dès Phase 1 avec N8R8, documenter les contraintes |
| R-08 | WiFi instable (réseau domestique varié) | Synchro échoue | Moyen | Retry automatique, timeout généreux, feedback clair à l'utilisateur |
| R-09 | Prix de vente trop élevé pour le marché | Ventes insuffisantes | Moyen | Valider la willingness-to-pay avec 50 early adopters avant la série |
| R-10 | Bois / ardoise : variabilité matériau | Qualité inconstante | Faible | Fournisseur local certifié, gabarits précis, contrôle qualité entrée |
| R-11 | iPhone ne peut pas émuler un tag NFC ISO14443A | Interaction téléphone→PN532 impossible sur iOS | Certain | Conception du flux de révélation sans NFC téléphone (voir §2.2.2b). Interaction téléphone→box via code clavier uniquement. NFC téléphone→box réservé Android si implémenté (COULD, Phase 3). |
| R-12 | NVS flash wear (scores écrits à chaque partie) | Corruption données persistantes après 2-3 ans usage intensif | Faible | Batcher les écritures NVS (accumuler N scores avant flush), utiliser un compteur de séquence. Monitorer la santé NVS en Phase 2. |
| R-13 | Replay attack sur HMAC auth (challenge réutilisé ou généré côté client) | Usurpation d'identité box | Faible-Moyen | Challenge généré côté serveur via `GET /box/challenge`, valide 60s, usage unique (invalidé après première réponse valide). Voir §6.1. |

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
| Anthropic API | IA (éditeur B2B) | Fonctionnalité IA dégradée (non bloquant) |

---

## 6. Interface Specifications

### 6.1 Interface Box ↔ Serveur (REST API)

**Base URL :** `https://box.agill.es/api/box` (dev/FFF — Coolify) · `https://escapebox.ch/api/box` (prod, Phase 3). Pas de sous-domaine API dédié ni de préfixe `/v1` : les routes sont des Route Handlers Next.js sous `app/api/box/`. Les chemins ci-dessous sont relatifs à cette base.

**Authentification :** Chaque box s'authentifie par HMAC-SHA256 challenge-response basé sur son UID unique gravé dans les eFuses ESP32.

```
GET /box/challenge?box_uid=ESP32S3-XXXX
Response 200:
  {
    "challenge": "<32 octets aléatoires hex>",   // usage unique, TTL 60s côté serveur
    "expires_in": 60
  }

POST /box/auth
Request:
  {
    "box_uid": "ESP32S3-XXXX-XXXX",
    "challenge": "<challenge reçu>",
    "challenge_response": "hmac_sha256(challenge, secret_key)"
  }
Response 200:
  {
    "token": "eyJ...",        // JWT valide 2h (jti traçable pour révocation)
    "server_time": 1715000000
  }
Response 401:
  { "error": "invalid_credentials" }

POST /box/register                              // appelé depuis la webapp lors du BLE provisioning
  Headers: Authorization: Bearer {supabase_user_jwt}
  Body: { box_uid, pairing_code }
  Returns: { device_id: uuid, ok: true }
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
        "download_url": "https://cdn.escapebox.ch/...(R2 Presigned URL, TTL 1h)",
        "url_expires_at": 1715003600,
        "signature": "base64_ecdsa_p256_signature",   // signature sur (contenu + box_uid)
        "checksum_sha256": "abc123..."
      }
    ],
    "firmware": {
      "version": "1.2.3",
      "url": "https://cdn.escapebox.ch/firmware/v1.2.3.bin",
      "sha256": "def456...",
      "ecdsa_signature": "base64_ecdsa_p256_signature"  // vérifié par le firmware avant OTA
    },
    "revoked_scenarios": [],   // slugs révoqués à supprimer de la SD
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
Service UUID: A1B2C3D4-E5F6-7890-ABCD-EF1234567890   // UUID propre EscapeBox (ne pas réutiliser l'exemple générique ESP-IDF)
Characteristics:
  - WIFI_SSID    (write): UUID A1B2C3D4-E5F6-7890-ABCD-EF1234567891  Max 64 chars
  - WIFI_PASS    (write): UUID A1B2C3D4-E5F6-7890-ABCD-EF1234567892  Max 64 chars
  - BOX_CODE     (read) : UUID A1B2C3D4-E5F6-7890-ABCD-EF1234567893  6-char pairing code
  - STATUS       (notify): UUID A1B2C3D4-E5F6-7890-ABCD-EF1234567894 "connecting|connected|error"
```

> **Sécurité BLE :** La session BLE doit être établie avec **BLE Secure Connections** (NimBLE : `BLE_SM_SC_REQUIRED=1`, `BLE_SM_MITM_REQUIRED=1`, `BLE_SM_IO_CAP_DISP_ONLY`). Le code 6 chars affiché sur l'écran sert de PIN BLE — il chiffre le transport avant l'écriture de `WIFI_PASS`.

**Flux provisioning initial :**
1. Box en mode provisioning → émet BLE avec code 6 chars sur l'écran
2. Webapp scanne les BLE devices (filtre sur Service UUID EscapeBox) → trouve le bon
3. Webapp initie le pairing BLE Secure avec le code 6 chars comme PIN
4. Webapp écrit SSID et mot de passe (chiffrés par la session BLE)
5. Box tente la connexion WiFi → notifie le statut
6. Si succès → `POST /api/box/register` depuis la webapp → provisioning terminé, BLE éteint

**Re-provisioning (changement de réseau WiFi) :**
- Après 3 tentatives de connexion WiFi consécutives en échec au démarrage, la box repasse automatiquement en mode provisioning BLE et affiche un message explicite sur l'écran.
- Accessible aussi manuellement via Menu → Paramètres → Reconfigurer WiFi.

### 6.3 Interface Utilisateur — Webapp

**Navigation principale (App Router Route Groups) :**

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
      session/route.ts              → POST upload scores
    webhooks/stripe/route.ts        → POST webhook Stripe (signature vérifiée)

  marketplace/page.tsx              → Scénarios communautaires (Phase 3)

proxy.ts                            → Next 16 (ex-middleware.ts) : refresh session Supabase +
                                      protection des préfixes réels (/shop, /library, /devices,
                                      /scores, /account, /checkout, /studio)
```

### 6.4 Interface Utilisateur — Box

**Face avant = personnage animé.** Pas de menu tactile on-device (l'ancien ILI9488 + XPT2046 a été retiré). La box affiche un visage piloté par le scénario :

- **Yeux (2× GC9A01)** — `components/ui_manager/eyes_anim.c`, port C ESP-IDF du pipeline « Uncanny Eyes » Adafruit (MIT, Phil Burgess) via le fork GC9A01 de thelastoutpostworkshop. Rendu 128×128 centré dans 240×240, mouvement autonome (drift aléatoire + clignements) en mode IDLE. Actions JSON disponibles dans les scénarios : `eye_blink`, `eye_emotion {type: happy|sad|surprised|sleepy|angry|closed}`, `eye_look {direction: left|right|up|down|center}`. Asset embarqué : `defaultEye.h` (~156 KB flash).
- **Bouche (SSD1680 e-ink)** — à implémenter. Affichage texte mot-à-mot synchro audio.

**Navigation : boutons physiques + visage.** Le joueur navigue avec les touches capacitives (MPR121/MTCH2120) et les boutons GPIO45/46 ; le personnage répond par la bouche e-ink (texte), la voix (audio) et les yeux. La configuration avancée (compte, langue, renommage) est déportée sur la webapp.

**États de la box :**

```
BOOT          → Yeux s'ouvrent + jingle (< 5s)
MENU          → Personnage idle, sélection de scénario (voir 6.4.1)
PLAYING       → Scénario en cours
SYNC          → Texte progression sur la bouche e-ink + LEDs pulsées
SETTINGS      → Réglages on-device minimaux (voir 6.4.2)
TEST          → Mode Test capteurs (dev only, voir 6.4.3)
CHARGING      → Indicateur de charge (LEDs + yeux SLEEPY si batterie faible)
ERROR         → Message e-ink + QR code support
```

#### 6.4.1 Accueil et lancement d'une partie (MENU)

- **Idle** : le personnage vit (clignements, regards). La bouche e-ink affiche le nom du scénario sélectionné.
- **Navigation** : touches capacitives ◀ / ▶ (ou boutons GPIO45/46) pour parcourir les scénarios installés. À chaque changement : titre sur la bouche e-ink + annonce vocale (titre, durée, difficulté), yeux qui réagissent.
- **Lancement** : touche ✓ (appui long 1 s) → confirmation vocale « Commencer [titre] ? » → second appui ✓ pour démarrer.
- **Raccourci NFC** : poser la carte NFC d'un scénario sur la box le lance directement (cohérent avec la vision « poser un objet NFC → elle s'allume »).
- Si aucun scénario installé : le personnage invite vocalement à synchroniser (texte e-ink + QR vers la webapp).

#### 6.4.2 Réglages on-device (minimaux)

Pas de menu de réglages écran : seules les actions indispensables sans réseau sont accessibles sur la box, le reste passe par la webapp.

```
Volume              → potentiomètre rotatif dédié (lu via ADS7830 0x48) — registres I2C PCM5122 (0x4C) + amplitude soft
Synchroniser        → touche ⟳ dédiée (ou combinaison) — état affiché sur la bouche e-ink
Reconfigurer WiFi   → appui long 5 s sur ⟳ au boot → relance le provisioning BLE (§6.2)
Infos box           → appui long 5 s sur ✓ → e-ink affiche version · box_uid · IP/RSSI · espace SD
Reset usine         → appui 10 s sur ⟳ + ✓ simultanés → confirmation vocale + appui ✓ → efface NVS
Langue / renommage  → webapp uniquement (appliqués à la prochaine sync)
```

Les préférences (volume par défaut, langue) sont persistées en **NVS** via `config_manager`.

#### 6.4.3 Mode dev & Mode Test (diagnostic capteurs)

**Déverrouillage (caché aux joueurs)** : depuis l'écran Infos box (§6.4.2), taper **7 fois rapidement** (< 3 s) sur la touche capacitive ✓. Confirmation vocale « Mode dev activé », flag persistant en NVS (`dev_mode=1`). Re-taper 7× le désactive.

**Mode Test** affiche un dashboard temps réel (rafraîchi à la cadence du `sensor_manager`, ~50 ms) de **tous les capteurs/actionneurs**, paginé sur la bouche e-ink (navigation ◀ / ▶) et dupliqué sur UART, pour déboguer le hardware sans recompiler :

| Bloc | Données affichées | Phase (capteur) |
|---|---|---|
| Touch capacitif | état des 12 électrodes (bitmap live) + seuils | 1 (MPR121) |
| Accéléro / gyro | x/y/z (g), tilt détecté, température | 1 (LSM6DSO) |
| Luminosité ambiante | lux | 1 (VEML7700) |
| Rotation plateau | angle brut 0–4095, vitesse | 2 (AS5600) |
| NFC | UID de la dernière carte lue | 2 (PN532) |
| Souffle / pression | pression hPa, seuil détection | 2 (BMP280) |
| Hall / aimant | état booléen | 2 |
| Audio | état I2S, volume courant, flag sous-tension | 1 |
| WiFi | RSSI, IP | 1 |
| Système | heap libre, PSRAM libre, uptime, temp SoC | 1 |

**Actionneurs (boutons de test)** : jouer un ton / MP3 test · cycle LED WS2812 (R/V/B) · servo open/close · flash écran. Permet de valider chaque sortie isolément.

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
│   ├── main/
│   │   ├── main.c               # Point d'entrée app_main()
│   │   └── CMakeLists.txt
│   ├── components/
│   │   ├── scenario/            # Moteur JSON + state machine
│   │   ├── nfc/                 # Driver PN532
│   │   ├── sensors/             # Drivers I2C : MPR121, LSM6DSOXTR, TMAG5273, VEML7700, MTCH2120, AS5600
│   │   ├── display/             # eyes.c — 2× GC9A01 1.3" (SPI3 partagé, esp_lcd_gc9a01)
│   │   ├── ui_manager/           # ui_face + eyes_anim (port Uncanny Eyes, data/defaultEye.h)
│   │   ├── mouth/                # SSD1680 e-ink 2.9" — bouche (SPI2, esp_lcd_ssd1681) [TODO]
│   │   ├── audio/               # I2S DMA + PCM5122PW (DAC) + PAM8406 (amp)
│   │   ├── minimp3/             # Décodeur MP3 single-header
│   │   ├── leds/                # WS2812 via RMT
│   │   ├── servo/               # SG90 via MCPWM (Phase 2)
│   │   ├── storage/             # SD card SPI+FAT + NVS
│   │   ├── config_manager/      # WiFi creds, box ID, préférences (NVS)
│   │   ├── wifi_manager/        # Sync + BLE provisioning (NimBLE) [Phase 2]
│   │   ├── ota_manager/         # esp_https_ota + vérification ECDSA [Phase 2]
│   │   └── crypto/              # Vérification ECDSA P-256 (mbedTLS) [Phase 2]
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults       # Config ESP-IDF par défaut
│   └── partitions.csv           # Partition table custom
├── web/                         # Web platform Next.js (Phase 1+)
├── docs/                        # Vision, FSD
├── tools/                       # yaml2json.py + outils dev
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

**Partition table custom — état actuel (Phase 1)** : flash configurée à 8 MB, partition app unique de 7.9 MB (gros MP3 embarqués, pas d'OTA) :

```csv
# partitions.csv — actuel (flash configurée 8 MB)
# Name,   Type, SubType, Offset,   Size
nvs,      data, nvs,     0x9000,   0x6000
factory,  app,  factory, 0x10000,  0x7F0000   # 7.9 MB app
```

**Cible Phase 2 (OTA, 16 MB exploités)** — à appliquer quand l'OTA arrive (FW-04). Table de référence maintenue dans le skill `esp32-escapebox-expert` (`assets/partitions.csv`) :

```csv
# partitions.csv — cible Phase 2 (N16R8, 16 MB)
# Name,    Type, SubType,  Offset,   Size
nvs,       data, nvs,      0x9000,   0x6000
otadata,   data, ota,      0xF000,   0x2000
nvs_keys,  data, nvs_keys, 0x11000,  0x1000     # clés NVS encryption (prod)
factory,   app,  factory,  0x20000,  0x300000   # 3 MB — image de secours (reflash USB only)
ota_0,     app,  ota_0,    0x320000, 0x300000   # 3 MB — slot OTA actif
ota_1,     app,  ota_1,    0x620000, 0x300000   # 3 MB — slot OTA inactif
storage,   data, littlefs, 0x920000, 0x6E0000   # ~6.9 MB LittleFS (assets MP3 + scénarios)
```
> La migration (FW-04) implique : `CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y`, `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` (rollback validé par self-test boot via `esp_ota_mark_app_valid_cancel_rollback()`), et le déplacement des assets MP3 de la partition app vers LittleFS. La partition `factory` sert d'image de secours : jamais mise à jour par OTA, restaurable si les deux slots OTA sont corrompus.

#### 7.1.4 Premier flash (USB)

```bash
# Configurer la cible (une seule fois)
idf.py set-target esp32s3

# Compiler (depuis firmware/)
idf.py build

# Flasher (esptool direct — `idf.py flash` ne passe pas les bons paramètres flash ici)
python -m esptool --chip esp32s3 -p /dev/ttyACM0 -b 460800 \
  --before default-reset --after hard-reset write_flash \
  --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/blackbox.bin

# Monitor série : depuis un terminal hôte (WSL2), pas depuis le container
```

**Logs attendus au premier démarrage (cible Phase 1 complète) :**

```
[BOOT] EscapeBox Firmware v0.1.0
[BOOT] Box UID: ESP32S3-ABCD-1234
[CONFIG] No WiFi config found - entering provisioning mode
[BLE] Advertising as: EscapeBox-1234
[BLE] Pairing code: X7K2PQ
[EYES] 2x GC9A01 initialized - 240x240 @ 40MHz (CS_L=40, CS_R=14)
[MOUTH] SSD1680 e-ink initialized - 296x128
[STORAGE] SD card: 8.00 GB
[SCENARIO] Found 1 scenario on SD: capitaine_verdier_v1
[AUDIO] PCM5122 PLL locked - I2S 44100 Hz 16-bit
[SENSORS] I2C scan: 0x10 0x24 0x35 0x48 0x4C 0x5A 0x5C 0x6A 0x76 - OK
[BOOT] Ready
```

#### 7.1.5 Configuration initiale WiFi (via webapp)

1. Aller sur `https://box.agill.es/devices/add` (prod Phase 3 : `escapebox.ch/devices/add`)
2. Cliquer "Associer ma box"
3. La webapp utilise la Web Bluetooth API pour scanner les devices BLE
4. Sélectionner "EscapeBox-XXXX" dans la liste
5. Entrer le code affiché sur l'écran de la box pour confirmer
6. Sélectionner le réseau WiFi et entrer le mot de passe
7. La box se connecte → confirmation sur l'écran + dans la webapp

> **Compatibilité Web Bluetooth :** Chrome/Edge desktop + Android Chrome. iOS Safari non supporté (WebBluetooth indisponible). Pour iPhone : utiliser la procédure AP mode ci-dessous. App native iOS prévue en Phase 3 (COULD).

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

> **Toutes les connexions HTTPS** (sync, OTA, upload session) utilisent la vérification TLS complète : le certificat racine (ISRG Root X1 Let's Encrypt) est compilé dans le firmware et passé à `esp_http_client_config_t.cert_pem`. Ne jamais passer `skip_cert_common_name_check = true` en production.

**Comportement en cas d'interruption OTA :**
- Si la connexion WiFi est perdue pendant le download : `esp_ota_abort()` est appelé, la partition inactive est marquée invalide, le firmware actuel continue de fonctionner.
- `esp_https_ota` ne supporte pas le resume HTTP Range — la prochaine synchro retente le download depuis le début.
- La taille cible maximale du binaire firmware est **< 2.5 MB** (à vérifier en CI avant chaque release). Les partitions OTA sont dimensionnées à 0x400000 (4 MB) sur le N16R8 (16 MB flash).

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
2. MENU : le personnage annonce le scénario sélectionné (bouche e-ink + voix)
3. Touches ◀ / ▶ pour naviguer → ✓ pour sélectionner (ou pose de la carte NFC du scénario)
4. Confirmation vocale : "Commencer Le Trésor du Capitaine Verdier ?" → ✓
5. La box charge le JSON depuis la SD → vérifie la signature
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
5. Télécharge les nouveaux scénarios non présents sur la SD :
   - Stratégie "tout-ou-rien" : un scénario est considéré installé UNIQUEMENT après
     vérification SHA256 complète du fichier téléchargé.
   - Si interruption réseau en cours de download : fichier partiel supprimé, retentative
     à la prochaine synchro.
6. Supprime de la SD les scénarios listés dans revoked_scenarios[]
7. Upload les scores des parties récentes (marqués "envoyés" en NVS uniquement après
   réception du { "ok": true } serveur — évite les doublons en cas d'interruption)
8. Vérifie si firmware update disponible (OTA automatique si oui)
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
[ ] Bouche e-ink SSD1680 affiche une chaîne de texte en partial refresh < 0.5s
[ ] Speaker gauche + droite produisent du son stéréo (MP3 lisible, pas de bruit parasite)
[ ] Contrôle volume I2C fonctionnel (fade-in/fade-out propre)
[ ] EQ DSP PCM5122 testée sur un scénario (pas de distorsion)
[ ] Micro détecte un claquement de mains à 1 mètre
[ ] PN532 lit un tag NTAG213 en < 500ms
[ ] MPR121 keypad détecte les 12 touches avec < 1% faux positifs (MTCH2120 : même test en Phase 2 PCB)
[ ] ADS7830 : valeur stable, pleine échelle sur les 8 canaux (SL1-SL4 + RV1-RV4)
[ ] Toggles SW1/SW2 : niveaux propres sur GPIO1/2 (pull-up interne)
[ ] LSM6DSOXTR détecte une inclinaison de 15° minimum
[ ] BMP280 détecte un souffle buccal à 5 cm
[ ] VEML7700 distingue pièce éclairée / pièce sombre
[ ] MLX90614 mesure la température d'une main à 2 cm
[ ] WS2812 : toute la chaîne répond, couleurs correctes
[ ] USB-C : programmation ET charge fonctionnels simultanément
[ ] Carte SD : lecture / écriture à > 1 MB/s
[ ] Batterie : tient 90 minutes sous charge normale
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
| LSM6DSOXTR | LCSC C481766 | https://www.st.com/resource/en/datasheet/lsm6dsox.pdf |
| MLX90614 | LCSC C58661 | https://www.melexis.com/en/documents/documentation/datasheets/datasheet-mlx90614 |
| PCM5122PW | LCSC C14969 | https://www.ti.com/lit/ds/symlink/pcm5122.pdf |
| PAM8406 | LCSC C89689 | https://www.diodes.com/assets/Datasheets/PAM8406.pdf |
| ICS-43434 | LCSC (chercher) | https://invensense.tdk.com/wp-content/uploads/2016/02/DS-000069-ICS-43434-v1.2.pdf |
| bq24075 | LCSC C15464 | https://www.ti.com/lit/ds/symlink/bq24075.pdf |
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
