## LSM6DSOXTR

> _Synthèse générée par component-research. Datasheet PDF : [./LSM6DSOXTR.pdf](./LSM6DSOXTR.pdf) (STMicroelectronics, DM00557899). Guide applicatif Adafruit : [./LSM6DSOXTR-adafruit-guide.pdf](./LSM6DSOXTR-adafruit-guide.pdf)._

**Fabricant** : STMicroelectronics
**Catégorie** : IMU 6 axes (accéléromètre 3D + gyroscope 3D) avec Machine Learning Core, FSM, Sensor Hub
**Référence officielle** : LSM6DSOXTR (suffixe `TR` = tape & reel ; puce identique au LSM6DSOX cut-tape)
**Datasheet source** : https://www.st.com/resource/en/datasheet/lsm6dsox.pdf

## Vue d'ensemble

IMU iNEMO 6 axes à très basse consommation (0,55 mA en mode combo haute performance). Combine accéléromètre 16 bits (±2/4/8/16 g) et gyroscope 16 bits (±125/250/500/1000/2000 dps). Embarque un **Machine Learning Core (MLC)** programmable (8 arbres de décision, 16 inputs), un **Finite State Machine (FSM)** (16 machines pour reconnaissance de mouvements custom), un **Sensor Hub** I²C maître pour 4 capteurs externes, une **FIFO de 9 KB** avec compression, pedometer/tap/free-fall/wakeup hardware, et interfaces I²C, SPI, MIPI I3C. Production massive : automobile, wearables, drones, gaming, AR/VR.

## Package & Footprint

- **Package** : LGA-14L (Land Grid Array, 14 pads)
- **Dimensions** : 2,5 × 3,0 × 0,83 mm (L × W × H typ.)
- **Pitch** : 0,5 mm (pads périphériques)
- **Empreinte LCSC / DigiKey** : LCSC C485229 (LSM6DSOXTR) ; DigiKey 497-LSM6DSOXTRCT-ND

## Pinout

LGA-14, vue de dessous (cf. section "Pin description" du PDF datasheet pour la disposition exacte) :

| Pin | Nom | Direction | Fonction |
|---|---|---|---|
| 1 | SDO / SA0 | I/O | SPI MISO en mode SPI 4-fils ; sélection adresse I²C (0 = `0x6A`, 1 = `0x6B`) |
| 2 | SDx | I/O | I²C data (sensor hub maître, vers slaves externes) |
| 3 | SCx | OUT | I²C clock (sensor hub maître) |
| 4 | NC / VDDIO | PWR | Selon variante, voir PDF — typ. VDDIO (1,62–3,6 V) |
| 5 | GND | PWR | Masse |
| 6 | OCS_AUX | IN | Chip-select auxiliaire (interface OIS) |
| 7 | SDO_AUX | OUT | Données OIS (Optical Image Stabilization) |
| 8 | SDA / SDI / SDO | I/O | I²C SDA, SPI MOSI/SDI selon mode |
| 9 | SCL / SPC | IN | I²C SCL ou SPI clock |
| 10 | CS | IN | Sélection bus : 1 = I²C/MIPI I3C, 0 = SPI |
| 11 | INT1 | OUT | Interruption programmable 1 (data ready, MLC, FSM, FIFO, wake-up…) |
| 12 | VDD | PWR | Alimentation principale 1,71–3,6 V |
| 13 | INT2 | OUT | Interruption programmable 2 |
| 14 | RES / OCS_AUX2 | — | Réservé / signaux aux |

> ⚠️ La numérotation et le nom exact des pads dépendent de la marque LGA — vérifier la section "Pin description" et "Package information" du PDF datasheet local avant routage. Toutes les broches d'alimentation et de masse doivent être découplées (≥100 nF + 10 µF VDD, 100 nF VDDIO).

## Paramètres électriques

| Param | Min | Typ | Max | Unité | Note |
|---|---|---|---|---|---|
| VDD | 1,71 | — | 3,6 | V | Alimentation analogique/numérique principale |
| VDD_IO | 1,62 | — | 3,6 | V | Alimentation I/O indépendante (logique I²C/SPI) |
| Iop combo HP | — | 0,55 | — | mA | Accel + gyro tous deux en haute performance |
| Iop combo LP | — | 0,38 | — | mA | Mode combo low-power _(typique datasheet)_ |
| Iop accel LP | — | 12 | — | µA | Accel seul, low-power ODR 12,5 Hz _(voir PDF)_ |
| Ipd (power-down) | — | 3 | — | µA | Mode arrêt complet |
| Vih / Vil | 0,7·VDDIO | — | 0,3·VDDIO | V | Niveaux logiques I/O |
| Fmax SCL (I²C) | — | — | 400 | kHz | Fast-mode (Fast-mode-plus non supporté) |
| Fmax SPI clock | — | — | 10 | MHz | Modes SPI 0 et 3 |
| Fmax I3C | — | — | 12,5 | MHz | MIPI I3C SDR |
| Température op | -40 | — | +85 | °C | Plage industrielle |
| Résolution ADC | — | 16 | — | bits | Accel et gyro |
| FIFO | — | 9 | — | KB | Avec compression dynamique |

_Les valeurs marquées "(voir PDF)" doivent être vérifiées directement dans `LSM6DSOXTR.pdf` (sections 1.2 "Mechanical characteristics" et 1.3 "Electrical characteristics") avant design final. Le mode combo low-power et l'accel-only low-power varient fortement avec l'ODR sélectionné — la table 7 du datasheet donne le courant pour chaque ODR._

## Interface

Trois interfaces séries mutuellement exclusives, sélectionnées par la pin CS et le bit `I3C_disable` du registre `CTRL9_XL` (0x18).

### I²C (CS=1, défaut)

- **Adresse** : `0x6A` (SA0/SDO = GND) ou `0x6B` (SA0/SDO = VDDIO)
- **Vitesse** : Standard 100 kHz, Fast-mode 400 kHz
- **Sensor Hub** : la puce peut piloter elle-même 4 slaves I²C externes via SDx/SCx (master mode), avec lecture cyclique en FIFO

### SPI (CS=0)

- **Modes** : SPI mode 0 (CPOL=0, CPHA=0) et SPI mode 3 (CPOL=1, CPHA=1)
- **Vitesse max** : 10 MHz
- **3-fils ou 4-fils** : sélection via `SIM` bit du `CTRL3_C` (0x12)
- **Adressage registre** : 7 bits + bit R/W ; bit 7 = R/W (1 = read), bits 6:0 = adresse

### MIPI I3C

- **Activé par défaut** au reset si I3C master présent (sinon dégradé en I²C)
- **Désactiver** : écrire `I3C_disable=1` dans `CTRL9_XL` pour forcer I²C/SPI
- Vitesse jusqu'à 12,5 MHz SDR

- **Endianness** : data registres OUT_X/Y/Z en paires Low/High (LSB en bas), little-endian
- **Auto-increment** : activé par bit `IF_INC=1` dans `CTRL3_C` (0x12) — quasi indispensable pour burst-read

## Registres clés

> Reset par défaut (sauf reset register lui-même) : `0x00`. WHO_AM_I est constant.

| Adresse | Nom | Reset | Description | Bits remarquables |
|---|---|---|---|---|
| `0x01` | FUNC_CFG_ACCESS | `0x00` | Accès banque embedded (MLC/FSM) | `[7]` FUNC_CFG_ACCESS, `[6]` SHUB_REG_ACCESS |
| `0x02` | PIN_CTRL | `0x3F` | Configuration pull-up SDO/INT | `[7]` OIS_PU_DIS, `[6]` SDO_PU_EN |
| `0x0D` | INT1_CTRL | `0x00` | Routage IRQ vers INT1 | `[0]` DRDY_XL, `[1]` DRDY_G, `[2]` BOOT, `[3]` FIFO_TH, `[4]` FIFO_OVR, `[5]` FIFO_FULL, `[6]` CNT_BDR, `[7]` DEN_DRDY |
| `0x0E` | INT2_CTRL | `0x00` | Routage IRQ vers INT2 | Idem INT1_CTRL avec sources alternatives |
| `0x0F` | WHO_AM_I | `0x6C` | Identification chip | Lecture seule, **vérifier au boot** |
| `0x10` | CTRL1_XL | `0x00` | Accel : ODR + FS + filtre | `[7:4]` ODR_XL, `[3:2]` FS_XL, `[1]` LPF2_XL_EN |
| `0x11` | CTRL2_G | `0x00` | Gyro : ODR + FS | `[7:4]` ODR_G, `[3:2]` FS_G, `[1]` FS_125 |
| `0x12` | CTRL3_C | `0x04` | Config globale | `[0]` SW_RESET, `[1]` _reserved_, `[2]` IF_INC, `[3]` SIM, `[4]` PP_OD, `[5]` H_LACTIVE, `[6]` BDU, `[7]` BOOT |
| `0x14` | MASTER_CONFIG | `0x00` | Sensor hub (accès via FUNC_CFG) | Configure master I²C interne |
| `0x17` | CTRL8_XL | `0x00` | Filtres HP/LP accel | `[7:5]` HPCF_XL, `[2]` HP_REF_MODE_XL |
| `0x18` | CTRL9_XL | `0xE0` | Mode I3C + DEN | `[1]` I3C_disable (mettre à 1 si pas d'I3C) |
| `0x19` | CTRL10_C | `0x00` | Timestamp enable | `[5]` TIMESTAMP_EN |
| `0x1E` | STATUS_REG | RO | Data ready | `[0]` XLDA, `[1]` GDA, `[2]` TDA |
| `0x20–21` | OUT_TEMP_L/H | RO | Température (16 bits, signé) | 256 LSB/°C, offset 25 °C |
| `0x22–27` | OUTX/Y/Z_L/H_G | RO | Gyro X/Y/Z (16 bits signés) | 6 octets en burst |
| `0x28–2D` | OUTX/Y/Z_L/H_A | RO | Accel X/Y/Z (16 bits signés) | 6 octets en burst |
| `0x4B` | STEPCOUNTER (L/H) | RO | Compteur de pas (16 bits) | Si pedometer activé |
| `0x58` | TAP_CFG0 | `0x00` | Tap/wakeup config | `[0]` LIR (latched IRQ), `[1]` TAP_X_EN, etc. |
| `0x5B` | WAKEUP_THS | `0x00` | Seuil wake-up | `[5:0]` threshold |

### ODR_XL / ODR_G — valeurs (4 bits)

| Code | ODR |
|---|---|
| `0000` | power-down |
| `0001` | 12,5 Hz |
| `0010` | 26 Hz |
| `0011` | 52 Hz |
| `0100` | 104 Hz |
| `0101` | 208 Hz |
| `0110` | 416 Hz |
| `0111` | 833 Hz |
| `1000` | 1,66 kHz |
| `1001` | 3,33 kHz |
| `1010` | 6,66 kHz |
| `1011` | 1,6 Hz (accel low-power uniquement) |

### FS_XL (accel) et FS_G (gyro)

| Code | FS_XL | Sensibilité accel | Code | FS_G | Sensibilité gyro |
|---|---|---|---|---|---|
| `00` | ±2 g | 0,061 mg/LSB | `00` | ±250 dps | 8,75 mdps/LSB |
| `01` | ±16 g | 0,488 mg/LSB | `01` | ±500 dps | 17,50 mdps/LSB |
| `10` | ±4 g | 0,122 mg/LSB | `10` | ±1000 dps | 35,00 mdps/LSB |
| `11` | ±8 g | 0,244 mg/LSB | `11` | ±2000 dps | 70,00 mdps/LSB |
| | | | + FS_125=1 | ±125 dps | 4,375 mdps/LSB |

> ⚠️ L'ordre FS_XL n'est **pas** ±2/±4/±8/±16 mais ±2/±16/±4/±8 — c'est une particularité ST, attention en mappant un enum logiciel.

## Séquence d'initialisation

```
1. Power-on : VDD puis VDDIO (peu importe l'ordre selon datasheet), attendre boot ≥ 35 ms
2. Lire WHO_AM_I (0x0F), vérifier = 0x6C
3. Soft reset : écrire CTRL3_C (0x12) = 0x01 (SW_RESET), poller jusqu'à SW_RESET=0
4. BDU + IF_INC : écrire CTRL3_C = 0x44 (BDU=1, IF_INC=1)
5. Désactiver I3C si non utilisé : CTRL9_XL (0x18) bit I3C_disable=1
6. Config accel : CTRL1_XL (0x10) = (ODR_XL<<4) | (FS_XL<<2) | LPF2_EN
7. Config gyro : CTRL2_G (0x11) = (ODR_G<<4) | (FS_G<<2)
8. Optionnel : router data-ready sur INT1 : INT1_CTRL (0x0D) = 0x03 (DRDY_XL | DRDY_G)
9. Attendre 1ère mesure : t_wait ≈ max(IT_accel, IT_gyro) selon ODR (≈10 ms typique à 104 Hz)
10. Lire OUT_TEMP_L (0x20) en burst 14 octets : temp(2) + gyro(6) + accel(6)
```

**Boot time** : datasheet indique typiquement ≤ 35 ms après VDD stable avant que le chip ne soit accessible. Toujours laisser cette marge.

## Exemple ESP-IDF

```c
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LSM6DSOX_ADDR         0x6A   // SA0=GND
#define LSM6DSOX_WHO_AM_I     0x0F
#define LSM6DSOX_CTRL1_XL     0x10
#define LSM6DSOX_CTRL2_G      0x11
#define LSM6DSOX_CTRL3_C      0x12
#define LSM6DSOX_CTRL9_XL     0x18
#define LSM6DSOX_OUT_TEMP_L   0x20

#define ODR_104HZ_FS_4G       ((0x4 << 4) | (0x2 << 2))   // accel
#define ODR_104HZ_FS_2000DPS  ((0x4 << 4) | (0x3 << 2))   // gyro
#define WHO_AM_I_EXPECTED     0x6C

static const char *TAG = "lsm6dsox";

static esp_err_t reg_write(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t v) {
    uint8_t buf[2] = { reg, v };
    return i2c_master_transmit(dev, buf, 2, 100);
}

static esp_err_t reg_read(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t n) {
    return i2c_master_transmit_receive(dev, &reg, 1, buf, n, 100);
}

esp_err_t lsm6dsox_init(i2c_master_bus_handle_t bus, i2c_master_dev_handle_t *out)
{
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = LSM6DSOX_ADDR,
        .scl_speed_hz    = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(bus, &cfg, out), TAG, "add_device");
    vTaskDelay(pdMS_TO_TICKS(40)); // boot time

    uint8_t who = 0;
    ESP_RETURN_ON_ERROR(reg_read(*out, LSM6DSOX_WHO_AM_I, &who, 1), TAG, "whoami");
    if (who != WHO_AM_I_EXPECTED) {
        ESP_LOGE(TAG, "WHO_AM_I=0x%02X expected 0x%02X", who, WHO_AM_I_EXPECTED);
        return ESP_ERR_NOT_FOUND;
    }
    // Soft reset + reload defaults
    ESP_RETURN_ON_ERROR(reg_write(*out, LSM6DSOX_CTRL3_C, 0x01), TAG, "sw_reset");
    vTaskDelay(pdMS_TO_TICKS(15));
    // BDU=1, IF_INC=1
    ESP_RETURN_ON_ERROR(reg_write(*out, LSM6DSOX_CTRL3_C, 0x44), TAG, "ctrl3");
    // I3C off (CTRL9_XL bit1)
    ESP_RETURN_ON_ERROR(reg_write(*out, LSM6DSOX_CTRL9_XL, 0xE2), TAG, "ctrl9");
    // Accel 104Hz / ±4g, Gyro 104Hz / ±2000dps
    ESP_RETURN_ON_ERROR(reg_write(*out, LSM6DSOX_CTRL1_XL, ODR_104HZ_FS_4G), TAG, "ctrl1");
    ESP_RETURN_ON_ERROR(reg_write(*out, LSM6DSOX_CTRL2_G,  ODR_104HZ_FS_2000DPS), TAG, "ctrl2");
    return ESP_OK;
}

esp_err_t lsm6dsox_read_all(i2c_master_dev_handle_t dev,
                             int16_t *temp, int16_t gyro[3], int16_t accel[3])
{
    uint8_t buf[14];
    ESP_RETURN_ON_ERROR(reg_read(dev, LSM6DSOX_OUT_TEMP_L, buf, 14), TAG, "burst");
    *temp     = (int16_t)((buf[1] << 8) | buf[0]);
    gyro[0]   = (int16_t)((buf[3] << 8) | buf[2]);
    gyro[1]   = (int16_t)((buf[5] << 8) | buf[4]);
    gyro[2]   = (int16_t)((buf[7] << 8) | buf[6]);
    accel[0]  = (int16_t)((buf[9] << 8) | buf[8]);
    accel[1]  = (int16_t)((buf[11] << 8) | buf[10]);
    accel[2]  = (int16_t)((buf[13] << 8) | buf[12]);
    return ESP_OK;
}
```

## Drivers existants

| Source | URL | Licence | Couvre | Maturité |
|---|---|---|---|---|
| ESP Component Registry — `espp/lsm6dso` | https://components.espressif.com/components/espp/lsm6dso | MIT | I²C+SPI, FIFO, IRQ, tap, filtres | ✅ très mature (v1.0.35+) |
| ESP Component Registry — `kodediy/kode_lsm6dsox` | https://components.espressif.com/components/kodediy/kode_lsm6dsox | MIT | Wrapper du driver ST officiel | bonne pour MLC/FSM (utilise lsm6dsox_reg.c de ST) |
| GitHub ST officiel C-driver | https://github.com/STMicroelectronics/lsm6dsox-pid | BSD-3 | Tous registres, accès brut | ✅ source faisant foi |
| Adafruit C++ (Arduino) | https://github.com/adafruit/Adafruit_LSM6DS | BSD | Accel + gyro + tap + pedometer | ✅ mature, lisible |
| CircuitPython | https://github.com/adafruit/Adafruit_CircuitPython_LSM6DSOX | MIT | API haut niveau | ✅ |

> Pour ESP-IDF v6.1 du projet Blackbox : `kodediy/kode_lsm6dsox` est le plus pertinent si tu veux exploiter le MLC/FSM (il intègre le `lsm6dsox_reg.c` officiel de ST en sous-couche). Pour une intégration basique accel+gyro, `espp/lsm6dso` ou le code from-scratch ci-dessus suffit.

## Breakouts disponibles

- **Adafruit #4438** (LSM6DSOX seul, STEMMA QT) : breakout 3,3 V régulé + level-shift, ~10 USD — https://www.adafruit.com/product/4438
- **Adafruit #4517** (LSM6DSOX + LIS3MDL 9-DoF, STEMMA QT) : combo avec magnétomètre — https://www.adafruit.com/product/4517
- **Adafruit #4565** : FeatherWing combo 9-DoF
- **SparkFun Qwiic 6DoF** (basé LSM6DSO, pas DSOX) : utile mais MLC absent — https://www.sparkfun.com/products/18020
- Modules génériques LCSC/AliExpress : LSM6DSOX nu sans régulateur, alimentation 3,3 V uniquement

## Notes spécifiques projet

_(section vide — à remplir si le LSM6DSOXTR est intégré dans la Blackbox : bus I²C utilisé, adresse choisie via SA0, INT1/INT2 vers quels GPIO de l'ESP32-S3, usage prévu — détection de manipulation du boîtier, accéléromètre pour réveiller la box, gestures pour interaction…)_

## Sources

- Datasheet officiel ST (DM00557899) : https://www.st.com/resource/en/datasheet/lsm6dsox.pdf
- Application note AN5272 (always-on operation) : https://www.st.com/resource/en/application_note/an5272-lsm6dsox-alwayson-3axis-accelerometer-and-3axis-gyroscope-stmicroelectronics.pdf
- Application note AN5259 (Machine Learning Core) : https://www.st.com/resource/en/application_note/an5259-lsm6dsox-machine-learning-core-stmicroelectronics.pdf
- Page produit ST : https://www.st.com/en/mems-and-sensors/lsm6dsox.html
- Guide Adafruit (overview + pinouts + code) : https://learn.adafruit.com/lsm6dsox-and-ism330dhc-6-dof-imu
- Driver ST officiel C : https://github.com/STMicroelectronics/lsm6dsox-pid
- Driver Adafruit (headers utilisés pour validation registres) : https://github.com/adafruit/Adafruit_LSM6DS
- Composant ESP-IDF `espp/lsm6dso` : https://components.espressif.com/components/espp/lsm6dso
- Composant ESP-IDF `kodediy/kode_lsm6dsox` : https://components.espressif.com/components/kodediy/kode_lsm6dsox
