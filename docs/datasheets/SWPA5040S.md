# Sunlord SWPA5040S (inductance de puissance, L1 du boost MT3608)

> _Synthèse du catalogue Sunlord « Wire Wound SMD Power Inductors – SWPA Series », révision
> 2023/06/01 (fichier local [./SWPA5040S.pdf](./SWPA5040S.pdf), téléchargé depuis le miroir
> LCSC de C48496). Le PDF est un **scan** (images, pas de texte) : valeurs relevées à l'œil
> sur les pages « Specifications » (tableaux SWPA5040S), « Shape and dimensions » et notes
> *1 à *4 — relues le 2026-09-27._

**Fabricant** : Shenzhen Sunlord Electronics
**Catégorie** : inductance de puissance CMS bobinée, blindée (résine magnétique), 5 × 5 × 4 mm
**Référence retenue** : **SWPA5040S6R8MT** — LCSC / JLCPCB **C36411** (Extended)
**Datasheet source** : https://www.lcsc.com/datasheet/lcsc_datasheet_2310251551_Sunlord-SWPA5040S6R8MT_C36411.pdf

## Définitions (notes du catalogue)

- *1 : mesures référencées à **20 °C** ambiant.
- *2 : courant nominal = **le plus petit de Isat et Irms**.
- *3 : **Isat** = courant continu pour lequel l'inductance **chute d'environ 30 %**.
- *4 : **Irms** = courant continu qui provoque un **échauffement de 40 °C** (depuis 20 °C).
- L mesurée à 100 kHz, 1 V. Température de fonctionnement −40 à +125 °C, auto-échauffement compris.
- Les colonnes Isat/Irms portent deux valeurs, « Max. » et « Typ. », la « Max. » étant la plus
  basse : on dimensionne sur la colonne « Max. » (la valeur garantie). JLCPCB affiche la
  « Typ. » dans ses descriptions (ex. « 3.9A » pour le 4R7NT).

## Tableau SWPA5040S (extrait 4,7-10 µH)

| Référence | L | DCR max / typ (Ω) | SRF min (MHz) | Isat « Max » / typ (A) | Irms « Max » / typ (A) | LCSC | Stock JLCPCB (27/09/2026) |
|---|---|---|---|---|---|---|---|
| SWPA5040S4R7NT | 4,7 µH **±30 %** | 0,039 / 0,030 | 28 | 3,50 / 3,90 | 3,00 / 3,30 | C305174 | 36 460 |
| SWPA5040S5R6MT | 5,6 µH ±20 % | 0,046 / 0,035 | 27 | 3,00 / 4,10 | 2,80 / 3,10 | C96924 | 1 501 |
| **SWPA5040S6R8MT** | **6,8 µH ±20 %** | **0,056 / 0,043** | 21 | **2,90 / 3,50** | **2,50 / 2,80** | **C36411** | 11 971 |
| SWPA5040S8R2MT | 8,2 µH ±20 % | 0,062 / 0,048 | 20 | 2,70 / 3,00 | 2,30 / 2,60 | non cherché | — |
| SWPA5040S100MT | 10 µH ±20 % | 0,083 / 0,064 | 18 | 2,35 / 2,90 | 2,10 / 2,40 | C84608 | 64 703 |

⚠ **SWPA5040S4R7MT (±20 %, C48496) n'existe pas dans le catalogue 2023** : le 4,7 µH n'y
figure qu'en « NT » (±30 %). La référence MT vendue par LCSC n'a donc pas de ligne de
spécification dans la datasheet actuelle — à éviter.

## Package & empreinte (catalogue, « Shape and dimensions », Fig. 3)

- A × B = 5,0 ± 0,2 × 5,0 ± 0,2 mm, hauteur C = **4,0 mm max**, D = 4,0 ± 0,2, E = 1,25 ± 0,2.
- **Empreinte recommandée** : 2 pastilles b × c = **1,4 × 4,2 mm**, écartement intérieur
  a = **2,3 mm** (typ.).
- Marquage imprimé sur le dessus. Pas de polarité électrique (repère de début de bobinage
  seulement).

## Dimensionnement pour l'EscapeBox (U7 MT3608, rail 5 V)

Exigences MT3608 : L de **4,7 à 22 µH**, faibles pertes à 1,2 MHz, DCR faible, courant de
saturation à prendre en compte ; switch interne limité à **4 A** (`MT3608.md`).

Hypothèses : I_OUT = 1 A sur le 5 V (12 WS2812 à 432 mA + audio + halo J9), V_OUT + V_F(D1)
≈ 5,5 V, V_IN = 3,4 V (seuil d'extinction firmware), η ≈ 0,85 (à mesurer au proto).
- Courant moyen dans L : I_OUT × V_OUT / (V_IN × η) = 1 × 5,1 / (3,4 × 0,85) ≈ **1,8 A**.
- Rapport cyclique D ≈ 1 − 3,4 / 5,5 ≈ 0,38.
- Ondulation avec la **L minimale** (6,8 µH − 20 % = 5,44 µH) : ΔI = V_IN × D / (L × f)
  = 3,4 × 0,38 / (5,44 µ × 1,2 M) ≈ 0,20 A → **crête ≈ 1,9 A**.
- Isat 2,90 A (colonne « Max. ») → **marge ×1,5** à la définition −30 % ; à 1,9 A la baisse
  réelle d'inductance est bien moindre.
- Irms 2,50 A pour +40 °C → à 1,8 A, ≈ (1,8 / 2,5)² × 40 ≈ **+21 °C** estimés (pertes cuivre
  seules).
- Pertes DCR : 1,8² × 0,056 ≈ **0,18 W** au pire.
- En surcharge, l'inductance sature (≈ 2,9-3,5 A) avant la limite de switch du MT3608
  (4 A) : le courant monte plus vite, mais la limite cycle à cycle du MT3608 protège.
  C'est un cas de défaut, pas de fonctionnement normal.

**Pourquoi 6,8 µH plutôt que 4,7 µH** : le seul 4,7 µH documenté est à ±30 %, soit 3,3 µH
au minimum, sous les 4,7 µH recommandés par le MT3608. Le 6,8 µH ±20 % reste dans la plage
sur toute sa tolérance (5,44-8,16 µH), avec encore 2,9 A de saturation. Le 10 µH tombe à
2,35 A de saturation, sous la marge visée.

## Notes spécifiques projet

- **L1 = SWPA5040S6R8MT (C36411)**, empreinte 5 × 5 mm à la place du 1210 (audit
  2026-09-27, H1).
- Placement : boucle SW → L1 → D1 → C_B2 → GND la plus courte possible.
- À mesurer au proto : rendement réel du boost, échauffement de L1 à pleine charge.

## Sources

- Catalogue Sunlord SWPA (rév. 2023/06/01), fichier local `SWPA5040S.pdf`.
- JLCPCB (stock, catégorie, prix au 27/09/2026) : https://jlcpcb.com/partdetail/Sunlord-SWPA5040S6R8MT/C36411
- LCSC : https://www.lcsc.com/product-detail/power-inductors_sunlord-swpa5040s6r8mt_C36411.html
