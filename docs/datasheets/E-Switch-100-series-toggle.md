# E-Switch série 100 — interrupteurs à levier

> _Synthèse de la datasheet E-Switch 100 Series (fichier local [./E-Switch-100-series-toggle.pdf](./E-Switch-100-series-toggle.pdf))._

**Fabricant** : E-Switch
**Catégorie** : Interrupteur à levier (toggle) miniature
**Référence** : série 100, variante **100SP1** (SPDT, On–None–On) prévue au FSD
**Datasheet source** : https://www.e-switch.com (100 Series)

## Variantes SPDT

| Code | Fonction |
|---|---|
| SP1 | On – None – On |
| SP2 | On – None – (On) (momentané) |
| SP3 | On – Off – On |
| SP4 | (On) – Off – (On) |
| SP5 | On – Off – (On) |

Montages : PCB (M2), panneau, avec étrier de maintien, etc. (voir le PDF pour les cotes).

## Caractéristiques

| Param | Valeur |
|---|---|
| Contacts argent | 5 A @ 120 VAC / 28 VDC (résistif), 2 A @ 250 VAC |
| **Contacts or** | **0,4 VA max @ 20 V max (AC ou DC)** |
| Résistance de contact | 10 mΩ max (initiale) |
| Isolement | 1000 MΩ min |
| Durée de vie mécanique | 40 000 cycles |
| Durée de vie électrique | 6 000 cycles à pleine puissance |
| Température | −40 à +85 °C |
| Couple de serrage | 5 kg·cm max |

## Notes spécifiques projet

- **SW1 / SW2** (face Côté 1 → J8) lus sur GPIO1/GPIO2 avec pull-up interne : courant de
  quelques dizaines de µA → choisir la **finition or** (prévue pour les signaux bas niveau)
  plutôt que l'argent (prévu pour les charges de puissance).
- Candidat possible pour l'interrupteur marche/arrêt (face Côté 2, cf. TODO FSD Phase 2).

## Sources

- Datasheet E-Switch 100 Series (PDF local)
