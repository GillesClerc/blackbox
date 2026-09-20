# ESP32-S3 — Hardware Design Guidelines

> _Synthèse du PDF fourni par l'utilisateur (`esp-hardware-design-guidelines-en-master-esp32s3.pdf`, rev. Sep 2026, "Releasemaster"). Couvre le SoC ESP32-S3 nu — beaucoup de sections (cristal, matching RF, flash/PSRAM) ne s'appliquent **pas** à nous car on utilise le **module** ESP32-S3-WROOM-1-N16R8, qui intègre déjà tout ça en interne. Cette synthèse ne garde que ce qui est pertinent pour l'intégration du module sur notre PCB._

**Fabricant** : Espressif Systems
**Portée retenue ici** : alimentation/découplage, reset (EN), strapping pins, ADC, USB, placement du module (antenne)
**Portée écartée (gérée en interne par le module)** : cristal 40MHz, matching RF, pinout flash/PSRAM interne, touch sensor natif (on utilise MTCH2120 externe)

## Alimentation / découplage

- Alim recommandée : **3.3V, courant de sortie ≥ 500mA** en simple alimentation — cohérent avec nos LDO AP2112K-3.3 (600mA)
- **0.1µF au plus près de chaque pin VDD** — la logique interne du module a ses propres broches VDD3P3/VDDA, mais côté module WROOM-1 elles sont regroupées sous une seule pin `3V3` exposée : nos `C1` (10µF bulk) + `C2` (100nF) sur cette pin couvrent la recommandation
- Diode ESD + ≥10µF conseillés à l'entrée d'alimentation principale de la carte — déjà couvert côté USB-C par `U12` (USBLC6-2SC6) + `C_IN` (4.7µF) sur la feuille Power

## Reset / EN (CHIP_PU)

- **CHIP_PU (= EN) ne doit jamais être laissé flottant**
- Circuit RC recommandé : **R = 10kΩ, C = 1µF** — confirmé et corrigé le 2026-09-20 (`R6` = 10k, `C_EN` = 1µF, anciennement 100nF par erreur)
- Timing mini : `tSTBL` = 50µs (stabilisation alim avant activation), `tRST` = 50µs (durée mini bas pour reset)
- Trace EN la plus courte possible (sensible aux interférences)
- Si alimentation lente à monter/instable (ex. charge batterie), le simple RC peut ne pas suffire — pas notre cas ici (LDO classique), pas d'action requise

## Strapping pins (GPIO0, GPIO3, GPIO45, GPIO46)

Lus au reset pour déterminer le mode de boot, redeviennent GPIO normaux ensuite.

| Pin | Rôle boot | État par défaut (interne) | Chez nous |
|---|---|---|---|
| GPIO0 | Boot mode (avec GPIO46) | Pull-up interne activé | **Pas de pull-up externe dans notre design actuel** — à vérifier/ajouter (voir Action ci-dessous) |
| GPIO46 | Boot mode | Pull-down interne activé | `BTN2` — input only, cohérent avec strapping |
| GPIO45 | VDD_SPI voltage select | Pull-down interne activé | `BTN1` — attention, ce pin sélectionne aussi la tension VDD_SPI (flash), vérifier que l'état au boot (LOW via pull-down interne = 3.3V, notre config) n'est pas perturbé par le bouton |
| GPIO3 | (ADC1_CH2, pas un strap boot standard) | — | `VBAT_SENSE` (déjà câblé) |

**⚠️ Action à vérifier** : la doc recommande explicitement *"place a pull-up resistor at the GPIO0 pin"* et *"do not add high-value capacitors at GPIO0"*. Le module a un pull-up interne (suffisant dans la plupart des designs sans bouton BOOT dédié), mais rien dans notre netlist actuel ne mentionne GPIO0 — à statuer : laisser sur pull-up interne seul, ou ajouter un pull-up externe + éventuellement un bouton BOOT pour le confort de dev.

## ADC

- `GPIO3 = ADC1_CH2` — confirme exactement notre choix pour `VBAT_SENSE`
- **"Add a 0.1µF filter capacitor between ESP pins and ground when using the ADC function to improve accuracy"** — déjà fait (`C_VBAT_SENSE`, 100nF)
- ADC1 recommandé plutôt qu'ADC2 (moins sujet aux interférences WiFi) — cohérent, GPIO3 est bien sur ADC1
- Précision typique après calibration : ±5 à ±50mV selon l'atténuation utilisée — à garder en tête côté firmware pour les seuils de tension batterie

## USB natif (GPIO19/20)

- Résistances série 22/33Ω + capas vers GND (initialement non peuplées) recommandées, au plus près de la puce — cohérent avec nos `R11`/`R12` (22Ω)
- Au power-up, `USB_D+` fluctue avant stabilisation — normal, pas d'action needed
- Confirme ce qu'on avait déjà déduit : PHY USB intégré, pas besoin de transceiver externe

## Placement du module / antenne (si applicable à notre boîtier)

- Antenne PCB du module à positionner **en bordure de carte**, point d'alimentation de l'antenne proche du bord
- Si impossible de dépasser le bord : découper le PCB de part et d'autre de l'antenne pour dégager une zone de clearance
- **Ne pas** placer le module au centre avec un évidement sur les 4 côtés
- Une fois en boîtier : **prévoir au moins 15mm de dégagement autour de l'antenne dans toutes les directions** — à vérifier lors du placement du module sur le PCB main et lors de la conception du boîtier (peut affecter la portée WiFi si le boîtier/composants sont trop proches)

## Notes diverses utiles

- Glitches bas niveau (~60µs) au power-up sur GPIO4-14, GPIO17-20 — normal, à ne pas confondre avec un bug si observé au scope pendant le bring-up
- GPIO19/GPIO20 ont chacun 2 glitches haut niveau (~60µs) au power-up, durée totale glitch+délai 3.2ms/2ms — spécifique USB, sans impact fonctionnel connu
