# Audit hardware (KiCad Main) + schéma DB — 2026-09-23

## Méthode

- Schémas reçus : `blackbox` (racine), `power`, `esp32`, `imu`, `connector`
  (KiCad 10). **`audio.kicad_sch` absent** (feuille référencée par la racine)
  → PCM5122, PAM8406, ICS-43434 et `3V3_A` côté audio **non audités**.
- Netlist extraite par script (pins + fils + labels + jonctions, transformations
  de symboles, fusion des global labels entre feuilles) : 83 composants.
- Recoupements : firmware (`#define` de pins), `docs/datasheets/*.md`,
  `docs/pcb/01-netlist.txt`, `docs/pcb/02-bom-lcsc.csv`, sdkconfig.
- Pas d'ERC/DRC KiCad (pas de kicad-cli dans le container), pas de PCB layout
  (antenne, plans de masse, pistes de puissance non vérifiables).

**Pinout firmware ↔ schéma : cohérent** — I2S0 BCLK/LRCK/DOUT = 4/5/6,
SPI3 yeux MOSI/SCLK/CS_L/CS_R/DC/RST = 38/39/40/14/41/42, SD
MOSI/MISO/CLK/CS = 11/15/12/47, I2C SDA/SCL = 21/17, WS2812 = 48,
LSM6DSOX SA0=GND → 0x6A (`LSM6_ADDR_DEFAULT`). GPIO35-37 (PSRAM octale)
laissés libres ✓.

---

## 🔴 Critique

### H1 — BTN1/BTN2 sur GPIO45/GPIO46 (strapping) avec pull-up prévue
`docs/pcb/01-netlist.txt` prévoit « pull-up externe vers 3V3_D » sur BTN1
(GPIO45) et BTN2 (GPIO46). Aujourd'hui les labels sont **orphelins** (reliés au
module seulement), mais si on câble comme documenté :
- **GPIO45 haut au reset = VDD_SPI 1,8 V** → la flash 3,3 V du N16R8 n'est plus
  lue → **la box ne boote plus**.
- GPIO46 haut + GPIO0 bas = combinaison de boot invalide (mode download).

→ Ne rien câbler sur 45/46 qui puisse être haut au reset. Seuls GPIO libres
restants : **GPIO43/44 (U0TXD/U0RXD)**, non routés. Les utiliser pour les
boutons impose de passer la console en USB-Serial-JTAG seule
(`CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG`, aujourd'hui UART0 primaire + USB
secondaire — les logs passent déjà par l'USB). Sinon : boutons sur un
satellite I2C.

### H2 — Connecteurs I2C J3-J6 : brochage inverse du standard Qwiic/STEMMA QT
J3-J6 (JST-SH 4p) : pin 1 = 3V3, pin 2 = GND, 3 = SDA, 4 = SCL. Le standard
Qwiic/STEMMA QT est **1 = GND, 2 = 3V3**, 3 = SDA, 4 = SCL. Le breakout
Adafruit MPR121 STEMMA QT (Phase 1) branché avec un câble standard reçoit une
**alimentation inversée**. Les satellites maison (`J_SAT`) suivent la
convention interne, donc cohérents entre eux.
→ Adopter le brochage Qwiic (1 = GND, 2 = 3V3) sur J3-J6 **et** `J_SAT` : on
garde la compatibilité avec tout l'écosystème de breakouts pour le proto.

### H3 — Feuille audio manquante
Non auditable : PCM5122 (découplage, MODE, charge pump), PAM8406 (5 W + 5 W :
**d'où vient son alimentation ?** Si c'est le rail 5 V du MT3608 avec D1 SS14
1 A, crêtes audio + 12 LEDs = surcharge), ICS-43434 (I2S1), filtrage 3V3_A.
→ Envoyer `audio.kicad_sch`.

---

## 🟠 Important

### H4 — Aucun moyen d'éteindre la box
SYSOFF (U8.15) → GND, EN du MT3608 et des deux AP2112K → VSYS : tout est
alimenté en permanence dès qu'une batterie est branchée. Consommation
résiduelle même ESP32 en deep sleep : 12 × WS2812B au repos (~0,5-1 mA
chacune), I_q du MT3608 (boost actif à vide), 2 LDO, rétroéclairage des
GC9A01 (non pilotable, cf. H13). La batterie se vide en quelques jours
(stock, expédition).
→ SYSOFF sur un interrupteur à glissière (le bq24075 est prévu pour : il
déconnecte la batterie de OUT) **et** EN du MT3608 piloté par un GPIO (rail
5 V coupé quand les LEDs sont éteintes).

### H5 — Régulateur 3V3_D sous-dimensionné (courant et thermique)
U5 AP2112K (600 mA, SOT-23-5) alimente : ESP32-S3 (pics WiFi ~350-500 mA),
carte SD (pics d'écriture 100-200 mA), 2 × GC9A01 + rétroéclairage (J7),
écran bouche (J7b), 4 satellites I2C, IMU, pull-ups.
- Budget de crête proche ou au-delà de 600 mA → brownout sous WiFi + SD + écrans.
- Thermique : sous USB, VSYS ≈ 4,9-5 V → (5 - 3,3) × 0,3 A ≈ 0,5 W dans un
  SOT-23-5 (θJA ~200-250 °C/W) → risque de coupure thermique.
- Fin de batterie : dropout 250 mV → 3V3 < 3,3 V sous ~3,55 V de VBAT (le
  S3 tient jusqu'à 3,0 V, mais la SD et les écrans sont moins tolérants).

→ Buck-boost 1-2 A (ex. TPS63802) ou buck synchrone, plus un budget de
puissance chiffré par rail.

### H6 — WS2812B pilotées en 3,3 V sous 5 V
V_IH = 0,7 × VDD = 3,5 V > 3,3 V du GPIO48 : hors spec. Ça marche souvent à
température ambiante, mais ce n'est pas fiable (premier pixel instable). Pas
non plus de 100 nF par LED dans le schéma : vérifier dans la datasheet si le
WS2812B-B intègre son condensateur, sinon en ajouter.
→ Buffer 74AHCT1G125 alimenté en 5 V (SOT-23-5, quelques centimes) entre R8
et LED2.

### H7 — Carte SD : ni pull-ups ni découplage local
- Aucune pull-up sur CS, CMD/MOSI, DAT0/MISO, DAT1, DAT2. La spec SD et
  Espressif recommandent 10 kΩ sur toutes les lignes, y compris en mode SPI
  (DAT1/DAT2 flottantes, CS flottant au boot = carte sélectionnée).
- Pas de condensateur près du slot : l'appel de courant à l'insertion fait
  plonger 3V3_D (seuls C1/C2 près du module) → reset brownout.

→ 5 × 10 kΩ vers 3V3_D + 10 µF et 100 nF au pied de J11.

### H8 — Ferrite + 10 pF en série sur les horloges SPI
FB1 (IO12 → SPI2_SCLK) et FB2 (IO39 → SPI3_SCLK), 120 Ω @ 100 MHz, plus
CC_SPI2_CLK / CC_SPI3_CLK 10 pF à la masse. Avec les yeux à 40 MHz, ce
filtre LC arrondit ou fait sonner l'horloge (glitches d'affichage). Une
ferrite n'est pas une terminaison série.
→ 22-33 Ω série côté ESP32 (même empreinte 0603 possible) et 10 pF en DNP
jusqu'au test CEM.

### H9 — Sécurité de charge LiPo désactivée
TMR → GND (timers de sécurité coupés, décision documentée dans
`bq24075.md`) + TS sur une résistance fixe de 10 kΩ (pas de NTC) : la LiPo
est chargée à 1 A, dans une boîte fermée, sans protection thermique ni
timeout. Acceptable en proto, à reconsidérer pour un produit (sécurité
batterie, CE).
→ TMR flottant (valeurs par défaut : 30 min de précharge, 5 h de charge) et
batterie à 3 fils avec NTC 10 k (J2 en 3 pins, TS sur la NTC).

### H10 — ~CHG / ~PGOOD non routés
R_CHG et R_PG (100 kΩ vers 3V3_D) tirent des nets qui ne vont nulle part : le
firmware ne peut savoir ni si la box charge ni si l'USB est présent.
→ Les router vers des GPIO (voir H1 pour les broches libres), ou au minimum
vers une LED de charge.

### H11 — D1 SS14 / MT3608 limites avec 12 LEDs + J9
12 LEDs blanc plein = ~720 mA à 5 V (> 1 A côté batterie), plus la chaîne
externe J9 : la diode 1 A est limite.
→ SS34 (3 A, même empreinte SMA) et plafond de luminosité côté firmware.

---

## 🟡 Faible / à noter

- **H12 — IMU INT1/INT2 non routés** : pas de réveil sur mouvement ni de tap
  sans polling (le « secouer la box » serait idéal en interruption). ST
  recommande aussi 10 µF sur VDD en plus des 100 nF (seuls C4/C5 100 nF).
- **H13 — Rétroéclairage GC9A01 non pilotable** (J7 pins 9/10 NC) : le
  réglage `bright` de `config_manager` est inopérant et le rétroéclairage
  reste allumé en veille. Si un GPIO se libère : MOSFET + PWM sur J7.9.
- **H14 — VBAT_SENSE sorti sur J8.5** : nœud ADC haute impédance
  (1 MΩ / 1 MΩ) envoyé dans un câble vers la face avant → bruit et ESD
  directement sur GPIO3. Le retirer du connecteur si ce n'est pas
  indispensable.
- Pas de boutons BOOT/RESET (seulement TP_GPIO0 et TP_EN) : OK en dev, à
  prévoir accessibles pour le SAV.
- USBLC6 placé après les 22 Ω (côté ESP32) : idéalement au plus près du
  connecteur. Mineur.
- Champ LCSC absent sur la plupart des passifs et de plusieurs CI (U1, U5-U10,
  U12) : à compléter pour l'assemblage JLCPCB.
- **Firmware** : `hal_leds_init(48, 1)` → 12 LEDs sur le Main (+ chaîne J9).
  À mettre à jour au bring-up.

## ✅ Conforme

EN : RC 10 kΩ / 1 µF (Espressif). USB-C : CC1/CC2 5,1 kΩ, paires A/B court-
circuitées, USBLC6 correctement câblé (flow-through 1-6 / 3-4). bq24075 :
mode ILIM (EN2 = H, EN1 = L), R_ILIM 1,5 kΩ → 1,07 A, R_ISET 890 Ω → 1 A,
CE → GND, SYSOFF non flottant, C_IN/C_OUT/C_BAT 4,7 µF. DW01A + FS8205 :
montage standard (OD → FET côté batterie, OC → FET côté charge, VCC par
100 Ω + 100 nF, CS par 1 kΩ). MT3608 : FB 750 k / 100 k → 5,1 V, L 22 µH.
LSM6DSOX : CS → VDDIO (I2C), SDx/SCx → GND, OCS/SDO_AUX flottants (conforme).
Pull-ups I2C 4,7 kΩ. Diviseur VBAT sur ADC1 (GPIO3) avec 100 nF.

---

## Schéma de base de données (diagramme Supabase)

### ✅ Conforme
`devices.box_uid` UNIQUE, `scenarios.slug` UNIQUE, PK composite
`device_scenarios(device_id, scenario_id)` (couvre la requête de `/pkg`),
PK `box_challenges.challenge`, `profiles.id` → `auth.users.id`.

### 🟠 Important
- **D1 — Licences liées à la box, pas à l'utilisateur** : `device_scenarios`
  est le seul droit. Une box remplacée (SAV) ou une deuxième box ne récupère
  pas les achats ; revendre la box transfère les achats. À trancher **avant
  Stripe** : table `purchases` / `user_scenarios` (droit utilisateur), puis
  droits par box dérivés du propriétaire.
- **D2 — `firmware_releases.sha256` nullable**, pas d'unicité
  `(version, channel)` : une OTA sans intégrité serait possible. NOT NULL +
  UNIQUE avant F6, puis signature (point #12 de l'audit logiciel).

### 🟡 Faible
- `box_challenges.used` nullable : le code filtre `used = false`, donc une
  ligne à NULL est inutilisable → `NOT NULL DEFAULT false`. Index sur
  `expires_at` (purge à chaque `/challenge`).
- `scenarios.active` nullable (le code traite NULL comme actif) →
  `NOT NULL DEFAULT true`. `price_chf numeric` sans précision ni
  `CHECK (>= 0)` ; `difficulty` / `duration_min` sans CHECK.
- `devices.owner_id` : index (RLS « own devices » + comptage de
  `/register`) ; comportement de la FK à la suppression d'un profil inconnu
  → `ON DELETE SET NULL` recommandé (la box redevient libre).
- `/api/box/register` : grâce à UNIQUE(box_uid), un double enregistrement
  concurrent échoue en base → la route renvoie 500 au lieu de 409 (gérer le
  code 23505).
- RLS et policies non visibles sur le diagramme : requêtes d'extraction
  ci-dessous, pour écrire la migration de référence (point #6 de l'audit
  logiciel).

```sql
select table_name, column_name, data_type, is_nullable, column_default
  from information_schema.columns where table_schema = 'public'
  order by table_name, ordinal_position;
select conrelid::regclass as tbl, conname, pg_get_constraintdef(oid)
  from pg_constraint where connamespace = 'public'::regnamespace;
select relname, relrowsecurity from pg_class
  where relnamespace = 'public'::regnamespace and relkind = 'r';
select tablename, policyname, cmd, roles, qual, with_check
  from pg_policies where schemaname = 'public';
select indexdef from pg_indexes where schemaname = 'public';
select tgname, tgrelid::regclass, pg_get_triggerdef(oid)
  from pg_trigger where not tgisinternal;
```
