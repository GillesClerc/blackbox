# Validation et contrôle qualité des cartes — EscapeBox

> **Statut : points de test POSÉS au schéma le 2026-09-27** (22 nouveaux + 4 existants,
> vérifiés un par un sur leur net). La check-list, elle, reste un squelette.
>
> **Statut initial : squelette** (2026-09-27). Objectif : figer *maintenant* la liste des points de
> test, parce qu'ils doivent être posés sur le PCB **avant le routage** — on ne les ajoute
> pas après coup. La check-list, elle, s'étoffera au fil des premiers prototypes : les
> valeurs mesurées sur la première carte saine deviennent la référence des suivantes.

## 1. À quoi sert quoi

Quatre usages, qui n'ont pas les mêmes exigences :

| Usage | Quand | Besoin |
|---|---|---|
| **Bring-up** d'un prototype | première carte | mesurer chaque rail avant de risquer les composants |
| **Réception de série** | à chaque lot JLCPCB | test rapide, répétable, sans réflexion |
| **Diagnostic de panne** | SAV, carte morte | isoler le sous-système fautif |
| **Traçabilité** | vente | savoir ce qui a été testé, quand, avec quel résultat |

Le principe directeur : **ne jamais alimenter une carte neuve sans avoir vérifié qu'aucun
rail n'est en court-circuit**. C'est la seule erreur qui détruit des composants en une
seconde, et elle se détecte à l'ohmmètre en trente secondes.

## 2. Points de test proposés

### 2.1 Obligatoires — alimentation

Sans eux, aucun bring-up sérieux n'est possible.

| Repère | Net | Pourquoi | Valeur attendue |
|---|---|---|---|
| `TP_VBUS` | `5V_USB` | présence de l'alimentation USB | 5,0 V ±5 % |
| `TP_VSYS` | `VSYS` | sortie du power path bq24075 | ≈ 4,7-5,0 V sur USB ; ≈ V_BAT sur batterie |
| `TP_VBAT` | `VBAT` | tension cellule, avant protection | 3,0-4,2 V |
| `TP_3V3D` | `3V3_D` | rail numérique | 3,3 V ±1,5 % (précision AP2112) |
| `TP_3V3A` | `3V3_A` | rail audio | 3,3 V ±1,5 % |
| `TP_5VB` | `5V_BOOST` | sortie du boost, **avant** le load switch | 5,0-5,2 V (0,6 × (1+750k/100k), MT3608) |
| `TP_5V` | `5V` | rail 5 V, **après** le load switch | idem − ~50 mV de chute dans Q1 |
| `TP_GND1..3` | `GND` | retours de mesure | — |

**Le couple `TP_5VB` / `TP_5V` est le plus rentable de la liste** : il isole en une mesure
le boost du load switch. Tension présente en amont et absente en aval = Q1 ou sa commande ;
absente des deux côtés = MT3608, L1 ou D1.

Pour les masses : **trois points répartis**, dont un dans la zone audio et un près du
connecteur USB. Au moins un doit accepter une **pince** (trou métallisé ou boucle) : une
masse longue en fil volant rend inexploitable toute mesure de signal rapide.

### 2.2 Fortement recommandés — diagnostic

| Repère | Net (schéma) | Ce qu'il permet |
|---|---|---|
| `TP_ISET` | `U8.ISET` / `R_ISET` | **mesurer le courant de charge sans couper une piste** — la tension sur ISET reflète le courant réel (bq24075, `bq24075.md`) |
| `TP_TS` | `BAT_TS` | vérifier la NTC : ≈ **0,75 V à 25 °C** (10 kΩ × la source interne de 75 µA) ; hors plage = charge suspendue sans symptôme |
| `TP_ENSYS` | `EN_SYS` | état de l'interrupteur marche/arrêt vu par la carte |
| `TP_GATE` | `GATE_5V` | grille de Q1 : ≈ 0 V box allumée, ≈ V_5V_BOOST éteinte |
| `TP_FB` | `U7.FB` | boucle de régulation du boost : **0,6 V** attendu ; toute autre valeur = R_FB_H/R_FB_L ou MT3608 |
| `TP_CHG` | `U8.CHG` | état de charge (open-drain, pull-up 100 kΩ déjà présent). Clignotement 2 Hz = **défaut de timer** |

### 2.3 Utiles — signaux

| Repère | Net | Usage |
|---|---|---|
| `TP_I2S_BCK` | `I2S0_BCLK` | diagnostiquer l'audio numérique sans sonder le DAC |
| `TP_I2S_LRCK` | `I2S0_LRCK` | idem |
| `TP_SCLK2` | `SPI2_SCLK` | **vérifier les fronts d'horloge à l'oscilloscope** — demandé par l'audit (H8) après le passage des ferrites aux 22 Ω |
| `TP_SCLK3` | `SPI3_SCLK` | idem, côté yeux (40 MHz, 80 visés) |
| `TP_WS2812` | `WS2812_DATA` | vérifier le signal LED après R8 |
| `TP_AUDIO_L` | `AUDIO_L` | sortie du DAC **après** le filtre RC, avant l'ampli : isole le PCM5122 du PAM8406 |

### 2.4 Déjà au schéma

`TP_EN`, `TP_GPIO0`, `TP_I2CA`, `TP_I2CL` — empreinte `TestPoint:TestPoint_Pad_D1.5mm`.

### 2.5 État : posés le 2026-09-27

Les 22 points de test ci-dessus **sont au schéma**, chacun relié par un fil court à un
global label portant son net (le routeur est donc libre de les placer là où ils sont
accessibles, sans contrainte de dessin). Vérification : 22/22 sur le net attendu.

Trois nets utiles n'avaient aucun nom et ont été nommés à cette occasion — ce qui sert
aussi la lisibilité du PCB : **`BQ_ISET`** (courant de charge), **`BQ_CHG`** (état du
chargeur), **`BOOST_FB`** (boucle de régulation du MT3608).

Avec les 4 existants, la carte compte **26 points de test**. Total en BOM : 3 lignes
groupées, empreinte `TestPoint:TestPoint_Pad_D1.5mm`.

### 2.6 Règles de pose (pour le routage)

- **Sérigraphier le nom à côté de chaque pastille.** Une pastille non identifiée ne sert à
  personne six mois plus tard.
- Pastilles **au plus près du point à mesurer** : un TP de rail placé loin mesure la piste,
  pas le régulateur.
- Regrouper les TP d'alimentation dans une zone accessible **face Dessous**, atteignable
  sans démonter le cube.
- Ne pas placer de TP sous un composant, un connecteur ou la batterie.
- Diamètre 1,5 mm : compatible pointes de touche standard. Pour une série plus grande, on
  pourra ajouter un connecteur de test groupé — décision à prendre plus tard, les pastilles
  n'empêchent rien.

## 3. Check-list de contrôle qualité

> Squelette. Les cases `____` sont à remplir avec les valeurs relevées sur la **première
> carte validée**, qui devient la référence.

### Phase 0 — Réception (avant toute alimentation)

- [ ] Nombre de cartes conforme, aspect général (délaminage, rayures, oxydation)
- [ ] **Inspection visuelle** : composants manquants, décalés, dressés (tombstoning), ponts
- [ ] Orientation des polarisés : **D1** (cathode), **U1**, **U8**, **Q1**, **Q2**, **U5**
- [ ] Photo recto/verso archivée (numéro de lot)

### Phase 1 — Hors tension, à l'ohmmètre

- [ ] Continuité de masse entre les trois `TP_GND`
- [ ] Résistance rail → GND, **aucune ne doit être proche de 0 Ω** :
      `3V3_D` ____ Ω · `3V3_A` ____ Ω · `5V` ____ Ω · `5V_BOOST` ____ Ω · `VSYS` ____ Ω · `VBAT` ____ Ω
- [ ] Pas de continuité entre rails différents (3V3_D ↔ 3V3_A ↔ 5V)
- [ ] Interrupteur : `EN_SYS` bascule bien entre VSYS et ~0 V (pull-down R20)

### Phase 2 — Première mise sous tension (alimentation de laboratoire)

> Alimenter **par USB uniquement**, batterie débranchée, courant limité à **100 mA** d'abord.

- [ ] Courant à vide < ____ mA — **au-delà, couper immédiatement**
- [ ] `TP_VSYS` = ____ V
- [ ] `TP_3V3D` = 3,3 V ±1,5 % → ____ V
- [ ] `TP_3V3A` = 3,3 V ±1,5 % → ____ V
- [ ] `TP_FB` = 0,6 V → ____ V
- [ ] `TP_5VB` = 5,0-5,2 V → ____ V
- [ ] `TP_5V` ≈ `TP_5VB` − 50 mV → ____ V
- [ ] Interrupteur sur arrêt : **tous les rails à 0 V** sauf `VSYS` (la charge doit rester possible)
- [ ] Aucun composant anormalement chaud (contrôle au doigt puis caméra thermique si dispo)

### Phase 3 — Charge de la batterie

- [ ] Cellule branchée (ou alimentation réglée à 3,7 V, limitée à 1,5 A)
- [ ] `TP_CHG` bas = charge en cours ; **clignotement 2 Hz = défaut de timer**
- [ ] `TP_ISET` = ____ V → courant de charge déduit = ____ A (cible ≈ 1 A)
- [ ] `TP_TS` ≈ 0,75 V à 25 °C → ____ V (hors plage : la charge est suspendue silencieusement)
- [ ] Basculer sur batterie seule : tous les rails tiennent, `TP_VSYS` ≈ `TP_VBAT`

### Phase 4 — Firmware et auto-test

- [ ] Détection USB série, flash complet réussi
- [ ] Boot sans redémarrage en boucle, log série propre
- [ ] `box_uid` lu et conforme (`ESP32S3-XXXX-XXXX`)
- [ ] **Auto-test** (voir §4) : PSRAM, flash, scan I2C, carte SD, écrans, audio, LEDs, IMU
- [ ] Wi-Fi : association et synchronisation cloud
- [ ] BLE : appairage depuis `/devices/add`

### Phase 5 — Fonctionnel et endurance

- [ ] Audio : écoute sans distorsion au volume plafonné (−22 dB), pas de souffle au repos
- [ ] Les 12 LEDs s'allument toutes, couleurs correctes, pas de LED figée
- [ ] Consommation moyenne en jeu = ____ mA ; autonomie mesurée = ____ h
- [ ] Élévation de température de U5 après 30 min sous USB = ____ °C
- [ ] Extinction/rallumage 10 fois sans incident

### Phase 6 — Traçabilité

- [ ] Numéro de série attribué et inscrit
- [ ] `box_uid` associé au numéro de série (base de données)
- [ ] Date, opérateur, version de firmware, résultats archivés

## 4. Auto-test firmware — la piste la plus rentable

La majorité de la phase 4 est automatisable : un mode « auto‑test » dans le firmware
vérifierait sans intervention humaine, et rendrait un verdict sur le port série :

- PSRAM détectée et testée, taille de flash, partitions, `box_uid`
- **Scan du bus I2C** et comparaison à la liste attendue (0x10, 0x20, 0x35, 0x48, 0x4C,
  0x53/0x57, 0x5A, 0x6A, 0x76 — voir FSD §2.2.3)
- Carte SD : montage, écriture/lecture d'un fichier témoin
- Écrans : mire de test sur chaque GC9A01, puis l'écran bouche
- Audio : tonalité de test, retour par le micro ICS‑43434 (**boucle acoustique** : le seul
  test qui valide toute la chaîne DAC → ampli → haut‑parleur → micro sans instrument)
- LEDs : balayage des 12 WS2812
- IMU : lecture, vérification du vecteur gravité (≈ 1 g sur un axe au repos)
- ADC : lecture de `VBAT_SENSE`, cohérence avec la tension réelle

Le firmware a déjà un menu de boot : l'auto‑test y prendrait une entrée. C'est à chiffrer,
mais un test qui prend 30 secondes et ne demande aucune compétence remplace une heure de
mesures à chaque carte.

## 5. À étoffer

- Valeurs de référence à relever sur la première carte saine (toutes les cases `____`)
- Seuils de rejet : à partir de quel écart une carte est-elle refusée ?
- Procédure pour les **satellites** (à écrire quand leurs schémas existeront)
- Outillage : gabarit de test, alimentation de laboratoire, charge électronique ?
- Décision « connecteur de test groupé » si la série dépasse quelques dizaines d'unités
- Que faire d'une carte refusée : reprise, rebut, retour JLCPCB
