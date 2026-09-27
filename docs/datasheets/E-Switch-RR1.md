# E-Switch série RR1 — interrupteur à bascule rond, montage façade

> _Synthèse de la datasheet E-Switch « RR1 Series » (1 page, fichier local
> [./E-Switch-RR1.pdf](./E-Switch-RR1.pdf), fourni par Gilles le 2026-09-27). Le PDF est une
> **image** : valeurs relevées à l'œil sur la page._

**Fabricant** : E-Switch
**Catégorie** : interrupteur à bascule (rocker) rond, unipolaire, encliquetable en façade
**Référence retenue** : **RR111C1921** — SW3, marche/arrêt de la face Côté 2
**LCSC / JLCPCB** : série référencée (ex. RR111C1121 C5870277) mais **stock 0** (27/09/2026) →
DigiKey / Mouser ; pièce câblée à la main, hors assemblage JLCPCB.

## Décodage de RR111C1921 (tableau « Example Part Numbers » et codification)

| Champ | Code | Valeur |
|---|---|---|
| Série | RR1 | Standard Body |
| Terminaison | 1 | Cosse 4,8 mm (petit trou) — faston 4,8 mm |
| Fonction | 1 | Off-On (1 pôle) |
| Calibre | C | 16 A 125 VAC |
| Couleur du corps | 1 | Noir |
| Couleur de la bascule | 9 | Rouge |
| Couleur du marquage | 2 | Blanc |
| Type de marquage | 1 | I/O vertical |
| Lentille / couleur / tension de lampe | — | **aucune : version NON lumineuse** |

La datasheet la liste telle quelle : « RR111C1921 — 1P Off-On, Black Body, Red Actuator, I/O Vert. »

## Caractéristiques

- **Découpe de façade** : Ø **20,0 mm**, avec une encoche de 5,0 mm × 2,1 mm (détrompeur) ;
  collerette Ø 23,0 mm. Épaisseur de paroi admise : **non indiquée** sur la page.
- Hauteur sous la façade (corps + cosses) : 22,8 mm ; au-dessus : 5,5 mm (bascule).
- Calibre C : 16 A 125 VAC, 10 A 250 VAC [UL, cUL] ; 10(4) A 250 V~ [VDE].
- **Contacts : alliage d'argent** ; cosses : cuivre argenté.
- Résistance de contact : 35 mΩ max à 50 VAC ; isolement 100 MΩ min à 500 VAC ;
  rigidité 1 500 VAC 1 min ; durée de vie électrique 10 000 cycles.
- Température : −20 à +105 °C [UL] ; −20 à +125 °C [VDE].
- Matériaux : corps et bascule polyamide 6/6 (UL94 V-2), bascule lumineuse polycarbonate.
- **Versions lumineuses** : suffixe « -lentille couleur tension », tensions de lampe
  **6 V, 12 V, 125 V ou 250 V** uniquement.

## Notes projet

- SW3 ne commute que `EN_SYS` : environ **50 µA** à travers R20 (pull-down 100 kΩ). La
  datasheet ne donne **aucun courant minimal** et les contacts sont en **argent**, conçus
  pour des charges de puissance → risque de mauvais contact à très bas courant (oxydation) :
  voir la décision sur R20 dans le RESTE-A-FAIRE.
- **Pas de voyant fiable avec les versions lumineuses** : lampe de 6 V minimum, alors que VSYS
  varie d'environ 3,4 V (batterie basse) à 5,5 V (USB branché) — voyant éteint ou très faible.
- Raccordement : deux cosses faston 4,8 mm → câble vers J12 (JST-SH 2 broches) de la Main.
