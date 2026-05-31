# EscapeBox — Workflow PCB avec Fusion 360 Electronics

## Vue d'ensemble

- **Outil EDA** : Autodesk Fusion 360 Electronics (basé sur Eagle)
- **Fabrication main board** : JLCPCB PCBA (composants CMS soudés en usine)
- **Fabrication satellites** : JLCPCB PCB nu + soudure manuelle
- **Librairies composants** : SnapEDA + Ultra Librarian (format Eagle .lbr)

## Etape 1 — Installer les librairies de composants

Fusion 360 Electronics n'a pas tous nos composants dans ses librairies intégrées.
Pour chaque composant, il faut télécharger le symbole + footprint + modèle 3D.

### Sources de librairies (gratuites)

| Source | URL | Format | Notes |
|---|---|---|---|
| SnapEDA | snapeda.com | Eagle .lbr | Le plus complet, inscription gratuite |
| Ultra Librarian | ultralibrarian.com | Eagle .lbr | Bon pour les TI/NXP |
| Component Search Engine | componentsearchengine.com | Eagle .lbr | Alternative |
| Espressif GitHub | github.com/espressif | Eagle .lbr | ESP32-S3 officiel |

### Composants à télécharger

Pour chaque composant ci-dessous, aller sur SnapEDA, chercher le part number exact,
télécharger en format "Eagle", et importer le .lbr dans Fusion 360 :

**Menu Fusion** : `Electronics` → `Library` → `Import Library` → sélectionner le .lbr

| Composant | Part number exact | Chercher sur SnapEDA |
|---|---|---|
| ESP32-S3 module | ESP32-S3-WROOM-1-N16R8 | "ESP32-S3-WROOM-1" |
| DAC audio | PCM5122PW | "PCM5122" |
| Ampli Class D | PAM8406DR | "PAM8406" |
| Micro MEMS | ICS-43434 | "ICS-43434" |
| LDO 3.3V | AP2112K-3.3TRG1 | "AP2112K" |
| Boost converter | MT3608 | "MT3608" |
| Chargeur LiPo + PP | bq24075RGTR | "bq24075" |
| Protection batterie | DW01A | "DW01A" |
| Dual MOSFET | FS8205 | "FS8205" |
| Touch capacitif | MTCH2120-I/MX | "MTCH2120" |
| IMU 6 axes | LSM6DSOXTR | "LSM6DSOX" |
| Lumière ambiante | VEML7700 | "VEML7700" |
| Pression | BMP280 | "BMP280" |
| NFC reader | PN532 | "PN532" |
| Température IR | MLX90614ESF-BAA | "MLX90614" |
| Hall linéaire 3D | TMAG5273A2QDBVR | "TMAG5273" |
| ESD USB | USBLC6-2SC6 | "USBLC6-2" |
| USB-C connector | — | "USB Type-C 16 pin" |

## Etape 2 — Créer le schéma dans Fusion

1. `File` → `New Electronics Design`
2. Créer un nouveau schematic
3. Placer les composants depuis les librairies importées (`Add Part`)
4. Câbler les nets en suivant la netlist fournie (`docs/pcb/01-netlist.txt`)
5. Utiliser les schémas ASCII (`docs/schematics/`) comme référence visuelle

### Organisation des feuilles (sheets)

Créer plusieurs feuilles dans le schéma pour la lisibilité :

| Sheet | Contenu |
|---|---|
| 1 - Power | Alimentation : USB-C, bq24075, LiPo, LDOs, boost |
| 2 - ESP32 | Module ESP32-S3 + découplage + reset + USB |
| 3 - Audio | PCM5122 + filtre RC + PAM8406 + ICS-43434 |
| 4 - Display | Connecteurs JST yeux (2× GC9A01) + e-ink bouche (SSD1680) |
| 5 - Sensors | Bus I2C + connecteurs JST satellites |
| 6 - IO | WS2812, potentiomètres, boutons, Hall |

## Etape 3 — PCB Layout

1. Depuis le schematic, `Switch to Board` (génère le netlist automatiquement)
2. Définir le contour du PCB : 100×100mm (main) ou 30×50mm (satellite)
3. Configurer les règles DRC :
   - Clearance : 0.2mm min (JLCPCB)
   - Track width : 0.2mm min, 0.3mm signal, 0.5mm power
   - Via : 0.3mm drill, 0.6mm pad
4. Placer les composants en suivant le placement recommandé dans les schémas
5. Router : GND plane sur layer 2, signaux sur layer 1 et 4

### Règles spécifiques EscapeBox

- **Zone audio isolée** : regrouper PCM5122 + LDO audio + filtre RC dans un coin
- **Pas de traces digitales sous la zone audio**
- **I2S traces courtes** (< 3cm), groupées, blindées par GND
- **GND continu** : pas de split, pas de fente dans le plan de masse
- **Découplage** : via GND au plus près de chaque capa

## Etape 4 — Export pour JLCPCB

1. `File` → `CAM Processor` → charger le job JLCPCB (ou utiliser les Gerber defaults)
2. Exporter les Gerbers : Top, Bottom, GND, Power, Drill, Silkscreen, Soldermask
3. Exporter la BOM en CSV (voir `docs/pcb/02-bom-lcsc.csv`)
4. Exporter le CPL (Component Placement List) en CSV
5. Uploader sur jlcpcb.com : Gerbers + BOM + CPL

### Fichiers requis par JLCPCB pour PCBA

| Fichier | Description |
|---|---|
| Gerber.zip | Toutes les couches du PCB |
| BOM.csv | `Comment, Designator, Footprint, LCSC Part#` |
| CPL.csv | `Designator, Mid X, Mid Y, Rotation, Layer` |
