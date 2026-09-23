# GC9A01A (driver des écrans ronds)

> _Synthèse de la datasheet GalaxyCore GC9A01A V1.0 préliminaire (fichier local [./GC9A01A.pdf](./GC9A01A.pdf), copie GitHub fbiego/dt78). **Couvre la puce driver uniquement** : le module écran rond (dalle + rétroéclairage + éventuel régulateur) a sa propre fiche, toujours absente._

**Fabricant** : GalaxyCore
**Catégorie** : Driver LCD TFT a-Si 240RGB × 240, RAM d'image intégrée
**Référence officielle** : GC9A01A
**Datasheet source** : https://github.com/fbiego/dt78/blob/master/datasheets/GC9A01A.pdf

## Vue d'ensemble

Driver mono-puce pour dalles 240 × 240 (rondes en général), interfaces MCU parallèle
et série (3 ou 4 fils : CSX, SCL, SDA, D/CX), 262k couleurs.

## Paramètres électriques (tableau 44)

| Param | Min | Typ | Max | Unité | Note |
|---|---|---|---|---|---|
| VCI (analogique) | 2,5 | 2,8 | **3,3** | V | |
| IOVCC (logique) | 1,65 | 2,8 | **3,3** | V | IOVCC ≤ VCI |
| V_IH | 0,7 × IOVCC | — | IOVCC | V | |
| V_IL | VSS | — | 0,3 × IOVCC | V | |
| Cycle d'horloge série, écriture (t_scycw) | 10 | — | — | ns | soit ≤ 100 MHz |
| Cycle d'horloge série, lecture (t_scycr) | 150 | — | — | ns | soit ≤ 6,6 MHz |
| Température | −30 | — | 70 | °C | jusqu'à 85 °C sans dommage |

Consommation : _(non donnée pour le rétroéclairage — elle dépend du module)_.

## Interface

- Série 4 fils : données latchées sur le front montant de SCL.
- IM[3:0] et entrées inutilisées : à fixer à IOVCC ou GND (le module s'en charge en général).

## Notes spécifiques projet

- Deux écrans (yeux) sur SPI3, 40 MHz dans le firmware (80 MHz visés au FSD) : **dans la
  limite de 100 MHz en écriture** ✓.
- **VCI/IOVCC max 3,3 V** : alimenter une dalle nue directement sur 3V3_D (AP2112 :
  3,3 V ±1,5 %, jusqu'à 3,35 V) frôle la limite ; beaucoup de modules intègrent leur propre
  régulateur → dépend du module retenu (datasheet à ajouter).
- Rétroéclairage : courant et pilotage (pin BL/BLK) à documenter avec la fiche du module
  (entre dans le budget 3V3_D, cf. `AP2112.md`).

## Sources

- Datasheet GC9A01A V1.0 (copie GitHub) : https://github.com/fbiego/dt78/blob/master/datasheets/GC9A01A.pdf
