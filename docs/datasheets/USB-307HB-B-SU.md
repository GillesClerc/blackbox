# USB-307HB-B-SU (connecteur USB-C J1)

> _Fichier local [./USB-307HB-B-SU.pdf](./USB-307HB-B-SU.pdf) : plan mécanique HOOYA en image (1 page, texte non extractible). Caractéristiques ci-dessous tirées de la fiche produit LCSC/JLCPCB et du brochage standard USB Type-C (symbole LCSC du schéma) — à confirmer sur le plan avant routage._

**Fabricant** : HOOYA (Dongguan Haoyu Electronics)
**Catégorie** : Connecteur USB Type-C femelle 24 broches CMS, vertical
**Référence officielle** : USB-307HB-B-SU (LCSC C309361)
**Datasheet source** : https://lcsc.com/datasheet/lcsc_datasheet_1912111437_HOOYA-USB-307HB-B-SU_C309361.pdf

## Caractéristiques (fiche produit LCSC/JLCPCB)

| Param | Valeur |
|---|---|
| Broches | 24 (USB 3.x complet) + blindage |
| Tension / courant nominal | 30 V / 3 A |
| Hauteur | 9,3 mm |
| Température | −30 à +80 °C |

## Brochage utile (USB 2.0 seulement, symbole du schéma)

| Broches | Signal | Connexion projet |
|---|---|---|
| A4, A9, B4, B9 | VBUS | 5V_USB → bq24075 IN, USBLC6 VBUS |
| A1, A12, B1, B12 | GND | GND |
| A5 / B5 | CC1 / CC2 | 5,1 kΩ vers GND chacune (R9/R10) → annonce « device », 5 V par défaut |
| A6, B6 | D+ | court-circuitées → R12 22 Ω → USB_DP |
| A7, B7 | D− | court-circuitées → R11 22 Ω → USB_DM |
| A2/A3/B2/B3, A10/A11/B10/B11 | paires SuperSpeed | non connectées |
| A8, B8 | SBU1/SBU2 | non connectées |
| 25–28 | blindage | GND |

## Notes spécifiques projet

- Côté 2 : port de charge et de debug, et futur mécanisme d'énigme « clé USB » (mode
  host/OTG, FSD §2.2.2c). ⚠ En mode host, la box devra fournir le VBUS de la clé : les
  5,1 kΩ sur CC (mode device) et l'alimentation actuelle ne le permettent pas → à concevoir
  si l'énigme est retenue.

## Sources

- Plan HOOYA (LCSC) : https://lcsc.com/datasheet/lcsc_datasheet_1912111437_HOOYA-USB-307HB-B-SU_C309361.pdf
- Fiche produit JLCPCB : https://jlcpcb.com/partdetail/HOOYA-USB_307HB_BSU/C309361
