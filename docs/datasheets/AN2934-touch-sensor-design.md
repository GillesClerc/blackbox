# AN2934 — Capacitive Touch Sensor Design Guide (Microchip)

> _Synthèse de la note d'application Microchip AN2934, DS00002934B (07/2020) — fichier local
> [./AN2934-touch-sensor-design.pdf](./AN2934-touch-sensor-design.pdf) (PDF chiffré sans mot de
> passe d'ouverture, polices encodées : texte relu après décodage), lue le 2026-09-27._
>
> Guide écrit pour les microcontrôleurs QTouch (annexes A-C), mais les règles de dessin des
> électrodes **self-capacitance** (§1) s'appliquent au CAP1298, qui mesure aussi en self-capacitance.
> Seule la partie §1 (self-capacitance) concerne le projet ; §2 (mutual) ne s'applique pas.

**Source** : https://ww1.microchip.com/downloads/aemDocuments/documents/TXFG/ApplicationNotes/ApplicationNotes/Capacitive-Touch-Sensor-Design-Guide-DS00002934-B.pdf

## Principe (§1.1-1.2.1)

- Le toucher ajoute C_t (doigt ↔ électrode, condensateur plan à travers le couvercle) en série
  avec le corps (C_h 100-200 pF) et le couplage masse ↔ terre (C_g). **Sur un appareil sur
  batterie, C_g est faible (~1 pF) et réduit fortement le delta** : exemple du guide, −33 % avec
  C_g = 2 pF. → c'est le cas de l'EscapeBox (batterie, pas de terre) : prévoir de la marge.
- Delta maximal avec : **grande électrode, couvercle mince, matériau à forte permittivité**.
- Doigt modélisé par un disque de **8 mm** (5-10 mm).

## Boutons (§1.2.2)

- Formes pleines, rondes ou rectangulaires, **coins arrondis** (moins de concentration de champ →
  moins d'ESD). Remplissage maillé (50 %) possible : moins de charge, mais moins de sensibilité.
- Électrode **plus grande que le doigt** pour tolérer un appui décentré (exemple 12 mm).
- **Trop grande → effet d'ombre de la main** (détection à l'approche, exemple 25 mm) et capacité
  de base plus élevée (mesure plus lente).
- **Espacement entre électrodes : 4 mm + épaisseur du couvercle** (recommandé).

| Tableau 1-1 | Min | Typ | Max |
|---|---|---|---|
| Hauteur | 8 mm | 12 mm | 20 mm |
| Largeur | 3 mm | 6 mm | 20 mm |
| Espacement | 3 mm | 6 mm | — |

## Couvercle (§1.3)

- Plus épais = moins sensible ; on compense en agrandissant l'électrode, qui doit **dépasser le
  doigt d'au moins l'épaisseur du couvercle de chaque côté** : couvercle 1 mm → électrode ≥ 10 mm ;
  3 mm → ≥ 14 mm ; 6 mm → ≥ 20 mm (doigt de 8 mm).

## Masse et blindage (§1.4)

- **Masse passive** (plan relié au GND) : tout conducteur référencé à la masse près d'une
  électrode ou de sa piste la charge et **réduit la sensibilité** ; les pistes des autres touches
  se comportent aussi comme de la masse (ne pas faire passer la piste de la touche 1 près de la
  touche 2).
- **Plan de masse arrière** : réduit fortement la sensibilité. Si on en met un : électrodes sur
  la face avant, masse sur la face arrière (distance maximale), **hachuré 50 % ou 25 %**, ou
  **découpé derrière les touches** si aucun toucher par l'arrière n'est à craindre. Relié à la
  masse du circuit en un seul point.
- **Masse coplanaire** (autour des électrodes, même couche) : améliore l'isolation, le bruit et le
  mode commun ; remplissage plein, **à ~2 mm des électrodes** (tableau 1-8 : 1 / 2 / 3 mm). Trop
  près = plus de capacité, **moins de tolérance à l'eau**.
- **Garde active (driven shield)** : même signal que l'électrode mesurée → pas de champ entre
  elles, pas de charge ; protège de l'arrière et améliore la tolérance à l'eau. Écart
  électrode ↔ garde **1 / 2 / 3 mm** (tableau 1-9, garde « deux niveaux »). Trop près : la
  capacité garde ↔ électrode dépasse celle vers la masse (SNR réduit, voire échec de calibration).
- **Anneau de garde : ne pas le fermer complètement** autour de l'électrode (bruit RF) ; l'ouverture
  facilite aussi le routage (figure 1-26).
- Rayonnement de la garde : on peut ajouter ou augmenter une **résistance série** sur l'électrode
  de garde et réduire sa surface.

## Limite du CAP1298 (datasheet DS00001571B, caractéristiques électriques)

- **Capacité de base maximale par entrée : 50 pF** (plage 5-50 pF) ; décalage recommandé au
  toucher 0,1-2 pF, minimum détectable 20 fF. → électrodes et pistes doivent rester sous 50 pF :
  c'est la borne haute de la taille de l'électrode de proximité.

## Application à l'EscapeBox (satellite Côté 2, CAP1298)

- **Épaisseur de la paroi (couvercle) à fixer** : elle dimensionne tout (taille mini des touches,
  espacement). Ex. paroi 3 mm → touches ≥ 14 mm, espacement ≥ 7 mm.
- 6 touches E1-E6 : pleines, coins arrondis, espacement ≥ 4 mm + paroi, pistes courtes, ne
  passant pas sous/près des autres touches.
- Proximité E7 : grande électrode (l'« ombre de la main » est ici recherchée), bornée par les
  50 pF de C_BASE ; anneau de garde E8 à 1-3 mm, **ouvert**, et éventuellement surface de garde
  derrière E7.
- Pas de plan de masse plein derrière les électrodes : hachuré ou découpé ; masse coplanaire à
  ~2 mm si besoin d'isolation.
- La partie USB (J1, USBLC6) et le CAP1298 lui-même restent éloignés des électrodes.
