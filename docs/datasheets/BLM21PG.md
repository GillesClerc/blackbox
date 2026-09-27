# Murata BLM21PG (BLM21PG221SN1D / BLM21PG121SN1D)

> _Synthèse de la datasheet Murata « Chip Ferrite Bead BLM21□□□□□□N1 », Spec No.
> JENF243A_0005AE-01 (fichier local [./BLM21PG.pdf](./BLM21PG.pdf))._

**Fabricant** : Murata
**Catégorie** : perle de ferrite CMS (EMIFIL), série **PG = « for DC power line »**
**Référence retenue projet** : **BLM21PG221SN1D** (LCSC C85840)
**Datasheet source** : https://www.murata.com/products/productdata/8796740845598/ENFA0005.pdf

## Vue d'ensemble

Perle de ferrite multicouche qui se comporte quasiment comme une résistance aux
fréquences de bruit (pas de résonance marquée, forme d'onde du signal non déformée) et ne
demande aucune connexion de masse — utilisable sur une ligne sans masse stable
[§1, Caractéristiques générales]. La série **PG** est la déclinaison forte intensité :
c'est celle qu'il faut sur une sortie d'ampli ou un rail d'alimentation.

⚠ **Piège de sélection** : les séries BLM21**BB/BD/RK/AG** existent aux mêmes valeurs
d'impédance (120 Ω, 220 Ω…) mais ne tiennent que **200 à 1000 mA** (voir tableau ci-dessous).
Sur une sortie de haut-parleur, elles seraient détruites. Vérifier les 2 lettres après
« BLM21 ».

## Package & Footprint

- **Package** : 0805 (2012 metric) — empreinte KiCad `Inductor_SMD:L_0805_2012Metric`
- Masse unitaire typique : **0,010 g** [§5]
- Dimensions détaillées : dessin §5 du PDF (graphique, non extractible en texte)
- Land recommandé pour la série PG [§12.1] : flow a = 1,1 / b = 3,5 / c = 0,95 mm ;
  reflow a = 1,2 / b = 2,4 / c = 1,25 mm
- Largeur de piste minimale conseillée selon le courant nominal et l'épaisseur de cuivre
  [§12.1] : PG jusqu'à 2 A → d = 1,25 mm (18/35/70 µm) ; PG 3 à 4 A → 2,4 mm en 18 µm,
  1,25 mm en 35 µm

## Table de la série PG (impédance à 100 MHz, courant nominal, R_DC)

| Référence | Z @ 100 MHz | I nominal 85 °C | I nominal 125 °C | R_DC init. | R_DC max après essai |
|---|---|---|---|---|---|
| BLM21PG220SN1D | 22 Ω ±25 % | 6000 mA | 3300 mA | 9 mΩ | 18 mΩ |
| BLM21PG300SN1D | 30 Ω (20 min.) | 4000 mA | 2300 mA | 14 mΩ | 28 mΩ |
| BLM21PG600SN1D | 60 Ω ±25 % | 3500 mA | 1900 mA | 20 mΩ | 40 mΩ |
| **BLM21PG121SN1D** | **120 Ω ±25 %** | **3000 mA** | **1550 mA** | **30 mΩ** | 60 mΩ |
| **BLM21PG221SN1D** | **220 Ω ±25 %** | **2000 mA** | **1250 mA** | **45 mΩ** | 90 mΩ |
| BLM21PG331SN1D | 330 Ω ±25 % | 1500 mA | 1000 mA | 70 mΩ | 140 mΩ |
| BLM21PG601SN1D | 600 Ω ±25 % | 1400 mA | 900 mA | 140 mΩ | 200 mΩ |
| BLM21PG102SN1D | 1000 Ω ±25 % | 1150 mA | 700 mA | 200 mΩ | 300 mΩ |

Source : [§3 Part Number and Rating]. Le courant nominal subit un **derating** linéaire avec
la température ambiante entre ces deux points [§3, note *1 et diagramme].

## Caractéristiques générales

| Param | Valeur | Note |
|---|---|---|
| Température de fonctionnement | −55 °C à +125 °C | [§3] |
| Température de stockage | −55 °C à +125 °C | [§3] |
| Mesure d'impédance | 100 MHz ±1 MHz | Keysight 4291A + fixture 16192A [§7.1] |
| Soudure | Sn-3,0Ag-0,5Cu, flux à base de rosin | pas de flux soluble à l'eau ni halide > 0,2 % [§12.2] |
| Conditionnement | 4000 pcs/reel, tape papier 8 mm | [§10] |

## Précautions (datasheet §11)

- **§11.2** : ne jamais dépasser le courant nominal → rupture du conducteur interne,
  combustion possible.
- **§11.3** : un **courant d'appel** (inrush) très supérieur au nominal provoque une
  surchauffe, même bref. À considérer sur un rail d'alimentation commuté.
- **§11.4** : éviter les atmosphères corrosives (soufre, chlore, ammoniac) et les huiles —
  corrosion des électrodes, circuit ouvert. Pas de garantie dans ces environnements.
- **§11.1** : la série n'est pas garantie pour les applications critiques listées
  (aéronautique, médical, transport…).

## Notes spécifiques projet

- **FB3-FB6** (feuille `audio`) : ferrites de sortie du PAM8406 vers le connecteur
  haut-parleur J10, une par fil. La datasheet PAM8406 les exige en classe D sans filtre
  (`PAM8406.md`, Application Note 2).
- **Courant réel** : PAM8406 au maximum 1,8 W sur 8 Ω (`PAM8406.md`) → **0,47 A rms,
  0,67 A crête** par fil. La BLM21PG221SN1D (2 A à 85 °C) offre donc un facteur 3 de marge,
  pour 45 mΩ soit ~21 mV de chute. La BLM21PG121SN1D (3 A, 30 mΩ) est le repli si la
  220 Ω manque.
- **Pourquoi 220 Ω plutôt que 120 Ω** : le courant est faible, donc autant prendre
  l'impédance la plus élevée que la marge autorise — l'atténuation du bruit de découpage
  (250 kHz et ses harmoniques, `PAM8406.md`) est doublée, sans effet dans la bande audio
  où la ferrite est quasi transparente.
- **FB1/FB2** (feuille `esp32`, horloges SPI) sont d'une autre série (0603, valeur
  `120R@100MHz`) et l'audit 2026-09-23 (H8) recommande de les **remplacer par des
  résistances 22-33 Ω** : une ferrite sur une horloge rapide arrondit les fronts. Ne pas
  confondre les deux usages.

## Sources

- Datasheet Murata JENF243A_0005AE-01 : https://www.murata.com/products/productdata/8796740845598/ENFA0005.pdf
- Fiche LCSC C85840 (BLM21PG221SN1D), C79382 (BLM21PG121SN1D)
