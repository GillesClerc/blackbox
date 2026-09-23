# VEML7700 — note d'application « Designing the VEML7700 Into an Application »

> _Synthèse de la note d'application Vishay (fichier local [./VEML7700-AN-designing.pdf](./VEML7700-AN-designing.pdf)). Datasheet du composant : [./VEML7700.md](./VEML7700.md)._

**Fabricant** : Vishay Semiconductors
**Objet** : intégration du capteur de lumière ambiante VEML7700 (boîtier transparent 6,8 × 3 mm, I2C)

## Points clés

- Photodiode très sensible : le capteur peut être placé derrière un verre de protection
  **très sombre** ; il fonctionne aussi derrière un verre clair, car même le plein soleil
  ne le sature pas (réglage de gain/intégration adapté).
- Six registres de commande 16 bits (00h–06h, 03h non défini/réservé selon la note) :
  ALS_CONF (gain, temps d'intégration, interruption, shutdown), seuils haut/bas (ALS_WH /
  ALS_WL), registres de sortie — détail dans `VEML7700.md`.
- **Au démarrage, ALS_CONF vaut 01 (shutdown)** : écrire le bit 0 à 0 puis attendre
  **2,5 ms** avant la première mesure.
- Gain et résolution : gain élevé pour ~0–230 lx ; gain 1/8 jusqu'à ~140 000 lx (résolution
  0,0672 lx/count à 800 ms d'intégration). Choisir gain et intégration selon la plage visée.
- Circuit d'application : figure 2 du PDF (« VEML7700 Application Circuit »).

## Notes spécifiques projet

- VEML7700 (0x10) sur le satellite face Devant : « éclairer les yeux » déclenche une réaction.
  Prévoir une ouverture ou une fenêtre ; le capteur tolère une vitre sombre (pratique pour
  le dissimuler dans le visage).

## Sources

- Note d'application Vishay « Designing the VEML7700 Into an Application » (PDF local)
