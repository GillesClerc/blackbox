# ST25DV04K

> _Synthèse générée par component-research. Datasheet PDF : [./ST25DV04K.pdf](./ST25DV04K.pdf)_

**Fabricant** : STMicroelectronics
**Catégorie** : Tag NFC/RFID dynamique (mémoire EEPROM dual-interface I2C + RF)
**Référence officielle** : ST25DV04KC-IE6S3 (variante 4-Kbit EEPROM, boîtier SO8, sortie GPO open-drain)
**Datasheet source** : https://www.st.com/resource/en/datasheet/st25dv04k.pdf (DS10925 Rev 9)

## Vue d'ensemble

Tag NFC dynamique (NFC Forum Type 5 / ISO 15693) avec EEPROM 4-Kbit (512 octets utilisateur) accessible à la fois par I2C (côté ESP32) et par RF (côté téléphone/lecteur). Contrairement à un lecteur (PN532, ST25R…), ce composant **ne lit pas** de badges/cartes externes — c'est une mémoire que l'ESP32 écrit (ex: URL NDEF) et qu'un téléphone NFC lit en tapant la box. Fonctionne même sans alimentation VCC (auto-alimenté par le champ RF du téléphone, régulateur interne 1.8V) — la lecture par le téléphone marche box éteinte/débranchée.

## Package & Footprint

- **Package retenu** : SO8N (8 pins), le plus simple à souder/vérifier. Existe aussi en TSSOP8, UFDFPN8 (QFN, pad exposé), WLCSP10, UFDFPN12 (ces 2 derniers seulement pour la variante GPO CMOS avec pin VDCG supplémentaire — non pertinent ici).
- **LCSC** : C3304276 (ST25DV04KC-IE6S3) — en stock, ~0.51$/unité

## Pinout (SO8, variante -IE = GPO open-drain)

| Pin | Nom | Direction | Fonction |
|---|---|---|---|
| 1 | AC0 | RF | Antenne (bobine) — pas d'autre chemin DC/AC dessus |
| 2 | V_EH | Sortie analogique | Energy harvesting — non régulé, non utilisé dans notre design |
| 3 | AC1 | RF | Antenne (bobine) |
| 4 | VSS | PWR | Masse |
| 5 | SDA | I/O I2C | Données I2C (open-drain, pull-up externe requis) |
| 6 | SCL | I2C | Horloge I2C (pull-up externe requis) |
| 7 | GPO | Sortie open-drain | Interrupt (RF field detect / write done / fast-transfer) — pull-up externe >4.7kΩ requis pour être utilisable |
| 8 | VCC | PWR | Alimentation 1.8-5.5V |

## Paramètres électriques

| Param | Min | Typ | Max | Unité | Note |
|---|---|---|---|---|---|
| VCC | 1.8 | — | 5.5 | V | Compatible direct 3V3_D |
| I2C clock | — | — | 1 | MHz | Fast mode+ |
| Capacité tuning interne | — | 28.5 | — | pF | Déjà intégrée, simplifie l'antenne vs PN532 |
| Cycles d'écriture | — | 1M @25°C | — | — | Rétention 40 ans |
| Distance de lecture typique | — | quelques cm | — | — | _(non chiffré précisément dans cette datasheet courte — dépend de l'antenne)_ |

## Interface

- **I2C** : adresse par défaut définie par device select code (voir doc registre — user memory à 0xA6/0x53 selon mode ; à vérifier dans le firmware au moment de coder le driver, pas figé ici)
- **RF** : ISO/IEC 15693, NFC Forum Type 5, lu nativement par les smartphones (Android natif ; iOS lit les tags Type 5 en natif depuis iOS 13 sans app tierce)

## Confirmation importante — sécurité électrique

> "An internal voltage regulator allows the external voltage applied on VCC to supply the ST25DVxxx, while preventing the internal power supply (rectified RF waveforms) to output a DC voltage on the VCC pin."

Donc : même si le téléphone tape la box **VCC non alimenté** (box éteinte), le chip s'auto-alimente en RF **sans jamais renvoyer de tension sur la pin VCC externe** — aucun risque de reverse-feed vers le rail 3V3_D quand la box est éteinte.

## Câblage proposé (main PCB)

- **VCC (8)** → `3V3_D` + découplage 10nF + 100nF au plus près
- **VSS (4)** → GND
- **SDA (5) / SCL (6)** → bus `I2C_SDA`/`I2C_SCL` partagé (pull-up déjà existant R3/R4 4.7k sur ce bus)
- **GPO (7)** → non connecté. Décision : détection du tap téléphone par polling I2C du registre d'état (`ITSTS_DYN`) toutes les ~150ms plutôt que par interruption matérielle — le budget GPIO de l'ESP32-S3 est saturé (GPIO1-21 et 38-48 tous alloués), et le coût du polling est négligeable (~0.6-1ms par lecture à 100kHz, soit ~1% de charge bus à 150ms d'intervalle, latence imperceptible pour l'utilisateur)
- **V_EH (2)** → non connecté (non utilisé)
- **AC0 (1) / AC1 (3)** → bobine antenne (boucle PCB ou fil) — dimensionnement à faire (plus simple que PN532 : pas de C1/C2/Rq/CRx/R1/R2, la capacité de tuning 28.5pF est déjà interne, potentiellement juste un petit condensateur d'ajustement externe selon l'inductance finale de la boucle choisie)

## Drivers existants

| Source | URL | License | Couvre | Maturité |
|---|---|---|---|---|
| STMicroelectronics X-CUBE-NFC6 / st25dv-driver | https://github.com/STMicroelectronics/st25dv-driver | BSD-3 | Driver C complet I2C, portable, pas dépendant d'un HAL ST | ✅ officiel, maintenu |

Pas de driver ESP-IDF natif connu, mais le driver ST officiel est un C portable (juste des callbacks I2C read/write à fournir) — portage direct sur `i2c_master` ESP-IDF simple.

## Notes spécifiques projet

- Remplace le PN532 (abandonné — EOL NXP, plus recommandé pour nouveaux designs). Use-case badge externe abandonné, seul le rôle "tag" (box → téléphone, ouverture d'une page web) est conservé.
- Antenne à dimensionner (boucle PCB, zone de garde sans cuivre dessous) — étape suivante.

## Sources

- Datasheet officiel : https://www.st.com/resource/en/datasheet/st25dv04k.pdf
- Driver officiel : https://github.com/STMicroelectronics/st25dv-driver
- LCSC (part choisi) : https://www.lcsc.com/product-detail/C3304276.html
