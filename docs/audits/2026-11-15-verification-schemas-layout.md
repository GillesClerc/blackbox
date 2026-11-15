# Vérification des schémas KiCad et du guide de layout — 2026-11-15

Périmètre : les six projets de `hardware/` (Main + Devant, Dessus, Côté 1, Côté 2, Côté 3) et
`docs/pcb/04-guide-layout.md`. Seconde passe, indépendante de `2026-11-14-audit.md` : ce rapport
ne reprend que ce qui est **nouveau, corrigé ou levé**. Base : commit `888316c`.

## Méthode et limites

- Netlists reconstruites (`tools/kicad_netlist.py --all` et `--check`) puis relues composant par
  composant contre `docs/datasheets/*.md`, et contre le texte des PDF quand il est extractible.
- Contrôle ajouté : numéros de broches des symboles comparés aux pastilles des 28 empreintes des
  bibliothèques du projet.
- Recherche en ligne par deux agents Sonnet (sources ouvertes citées plus bas). Leurs résultats
  sont marqués **[en ligne]** ; je n'ai recoupé moi-même que l'empreinte KiCad du fader.
- **Non fait** : ERC et DRC (pas de KiCad ici ; les deux `ERC.rpt` datent du 28/09 et sont
  périmés). Pas de rendu PDF dans cet environnement : les brochages donnés en dessin (AP2112 SO-8,
  FS8205, TF-01A, USB-C) n'ont pas été revus à l'œil. `CAP1298.pdf`, `BMP280.pdf` et
  `E-Switch-RR1.pdf` ne livrent aucun texte : pour eux je me suis appuyé sur les synthèses.
  Les empreintes des bibliothèques standard KiCad ne sont pas contrôlables ici.
- Le PCB est toujours vide : il n'y a pas de layout à relire.

**Verdict.** Raccordement sain : 0 fil mal raccordé, 0 net orphelin, liaisons carte à carte
cohérentes, brochages conformes aux synthèses. Cinq points à traiter avant de router (A1 à A5),
dont deux nouveaux et vérifiés (A1, A2). Le point H6 de l'audit du 14/11 (fader) est levé.

---

## A. À traiter avant de router

### A1 — Le bq24075 coupe la sortie au-delà d'environ 2,0 A sur batterie (nouveau)

Les audits précédents ne retenaient que le DW01A comme limite de courant. Le bq24075 en a une
seconde, plus rapide :

- `V_O(SC2)` = **200 / 250 / 300 mV** : si VBAT − VOUT dépasse ce seuil pendant
  `t_DGL(SC2)` = **250 µs**, la sortie est coupée, puis relancée après `t_REC(SC2)` = **60 ms**
  [bq24075 PDF, §8.5 Electrical Characteristics ; §9.3.4.1.3]. Le circuit reste actif sans
  source USB (« During power down mode, the V_OUT(SC2) circuitry is active », §9.3.1).
- Chute BAT → OUT : **50 mV typ, 100 mV max à 1 A** [même tableau, `V_DO(BAT-OUT)`], soit
  50 à 100 mΩ.
- Courant de coupure : 250 mV / 50 mΩ = 5 A en typique, mais **200 mV / 100 mΩ = 2,0 A garantis
  seulement** (calcul).

Pire cas calculé sur batterie à 3,4 V (rendement du boost 0,85, hypothèse) :

| Charge | Rail | Courant | Côté batterie |
|---|---|---|---|
| 20 WS2812B en blanc (12 Main + 8 halo, 12 mA par couleur) | 5 V | 0,72 A | |
| PAM8406 à fond sur 8 Ω (1,8 W, rendement 90 %) | 5 V | 0,40 A | |
| Total 5 V | | 1,12 A | **≈ 2,0 A** |
| ESP32 en émission + SD + écrans | 3V3_D | jusqu'à ~0,6 A | **≈ 0,6 A** |
| **Total** | | | **≈ 2,6 A** |

C'est au-dessus du minimum garanti du bq24075 (2,0 A) et du DW01A (1,6 A, délai 10-20 ms).
Sur une carte typique rien ne se passe ; sur une carte en bord de tolérance, la box redémarre
quand tout s'allume en même temps.

→ **Plafond firmware obligatoire**, à dimensionner sur 1,5 A côté batterie : audio limité à 1 W
(déjà prévu) et LEDs vers 40-50 % donnent ≈ 1,6 A. À mesurer au proto (TP_VSYS1 à l'oscilloscope,
allumage et charge crête).
→ L'allumage est le second cas à risque : sans démarrage progressif sur Q1 (H4 du 14/11), l'appel
de courant de L1 vers 47 µF se cumule à celui des deux LDO. Les deux empreintes proposées en H4
(résistance de grille + condensateur grille-source) deviennent recommandées.
→ Option matérielle : une empreinte pour un second FS8205 en parallèle double le seuil du DW01A,
mais ne change pas celui du bq24075.

**Statut (15/11, décision de Gilles : plafond + empreintes).** Feuille `power` : **R_SS** (0 Ω
monté) en série dans la grille de Q1, entre le nœud Q2.D/R21 et la grille ; **C_SS** (10 nF,
DNP) entre grille et drain. C'est un condensateur **grille-drain** et non grille-source comme
proposé en H4 le 14/11 : pendant la montée, V_GS reste sur le plateau de Miller, et seule la
capacité grille-drain fixe la pente (Q_gd = 2,5 nC, C_rss = 55 pF [AO3401A, Dynamic
parameters]). Départ de réglage au proto : 47 kΩ + 10 nF ≈ 0,2 A d'appel dans les 25 µF du
rail 5 V [calc, `AO3401A.md`]. Plafond firmware reporté dans `RESTE-A-FAIRE.md`.

**Trouvé en traitant A1 — TP_GATE1 n'était relié à rien dans KiCad.** La grille portait un
label **local** `GATE_5V`, TP_GATE1 un label **global** du même nom ; KiCad ne relie pas un
label local à un global homonyme, alors que `kicad_netlist.py` les fusionnait par nom. Corrigé :
la grille porte un label global, et l'outil limite désormais un label local à sa feuille et
signale tout homonyme (`--check`). Aucun autre cas dans les six projets ; les netlists des
satellites sont inchangées avec l'outil corrigé.

### A2 — Empreinte du bq24075 : cinq vias sans net, percés à 0,2 mm (nouveau)

`hardware/main/libs/BQ24075RGTR/QFN50P300X300X100-17N.kicad_mod` contient cinq pastilles
traversantes **numérotées 18 à 22** (Ø 0,4 mm, perçage 0,2 mm) dans le pad thermique 17. Le
symbole s'arrête à la broche 17.

- À la mise à jour du PCB, ces cinq vias n'auront **aucun net** : le DRC signalera cinq
  courts-circuits avec le pad 17, et la zone GND des couches internes les contournera au lieu de
  s'y raccorder. Le pad thermique ne serait alors relié au plan que par la face composant.
- Perçage 0,2 mm avec pastille < 0,45 mm = supplément chez JLCPCB **[en ligne, page capabilities]**.
- Le guide (§2.4) demande déjà « 4 vias 0,3/0,5 » sans savoir que l'empreinte en embarque cinq.

→ Avant F8 : supprimer les pastilles 18-22 de l'empreinte et poser les vias à la main, ou les
renuméroter en 17 et les passer en 0,3/0,5 mm (elles tiennent dans le pad de 1,68 mm).
Le pad 17 a bien sa pâte à braser (polygone sur F.Paste).

**Statut (15/11, décision de Gilles : conforme TI).** Le land pattern TI (RGT0016C, 4222419/E)
place justement 5 vias Ø 0,2 mm au pas de 0,58 mm ; le pochoir TI demande **85 % de pâte**
(ouverture 1,55 mm), l'empreinte n'en avait que 40 % (1,06 mm). Empreinte corrigée : vias
numérotés 17, Ø 0,2 / pastille 0,45 mm, connexion pleine, bouchés côté L4 ; pâte 1,55 mm.
Les rangées de pastilles restent celles de SnapEDA (0,26 × 0,86 mm, IPC nominal), plus longues
vers l'intérieur que celles de TI (0,24 × 0,6) : 0,165 mm entre leur bout et le pad central,
contre 0,26 mm chez TI. Conforme au DRC (≥ 0,15 mm), à surveiller au premier assemblage.

**Découvert en traitant A2 — l'empreinte venait d'une autre copie.** La table globale du poste
pointe `BQ24075RGTR`, `FS8205` et `MT3608` vers `C:/Users/Gilles/dev/Blackbox/blackbox/libs/`,
hors du dépôt (copies identiques au dépôt avant correction). Une correction dans le dépôt
n'aurait eu aucun effet. Les trois bibliothèques sont désormais déclarées dans les tables
**du projet** (`${KIPRJMOD}/libs/…`), qui priment sur la table globale : **B3 soldé**.

### A3 — MT3608 à 1 A sous 5 V : jamais démontré, à valider sur banc avant de commander

- La datasheet ne donne **aucune courbe à 3,3-3,7 V → 5 V**, ni résistance thermique. R_DS(on)
  du switch : 80 mΩ typ, **150 mΩ max** **[en ligne, copie de la fiche V1.0]**.
- Un banc indépendant rapporte une sortie stable jusqu'à ~400 mA depuis une Li-ion, instable
  au-delà de 800 mA d'entrée **[en ligne, finalrewind.org ; une seule source, puces peut-être
  contrefaites, autre inductance]**. Ce n'est pas une donnée fabricant.
- Notre besoin est de 1,1 A en sortie, soit ≈ 1,8-2,0 A d'entrée. La BOM annonce « 2A » pour U7.

→ Monter le boost seul (MT3608 C84817 + SWPA5040S6R8MT + SS34 + 22 µF) et mesurer tension,
ondulation et température à 0,5 / 0,8 / 1,1 A depuis 3,4 V **avant** de lancer la Main. Si la
tenue est insuffisante, c'est un changement de composant et d'empreinte.

**Statut (15/11, décision de Gilles : chercher une alternative, puis TPS63020).** Comparés :
FP6293 (boost asynchrone comme le MT3608 : 140 mΩ, 3,5 A typ sans minimum, V_CC max abs 6 V —
écarté), TPS61022 (pass-through ≈ VIN quand VIN > VOUT, donc jusqu'à ~5,4 V sous USB — écarté),
TPS61088 (surdimensionné, > 15 composants — écarté), **TPS63020** (buck-boost : régule 5 V que VSYS
soit au-dessus ou en dessous, charge déconnectée à l'arrêt, démarrage à 400 mA, limite moyenne
3,5 A min, « 2 A in boost mode » §8.1). Retenu, avec L1 = Sunlord WPN4020H1R5MT (1,5 µH, Isat
7,7 A, DCR 36 mΩ max). Consigne abaissée à **5,0 V** (+3 % du power save, §7.3.1). Feuille
`power` refaite : D1, Q1/Q2, R21, R_SS, C_SS, TP_5VB1, TP_FB1, TP_GATE1 et les PWR_FLAG du 5 V
retirés ; **H4 sans objet, H9 soldé, B6 soldé**. Restent deux contrôles à l'œil (figure 7 pour
VINA, dessin DSJ0014A pour l'empreinte) et le stock de l'inductance (200). Réserve : VSYS jusqu'à
5,6 V avec un chargeur hors norme, au-dessus des 5,5 V recommandés (max abs 7 V).

### A4 — Budget 3V3_D (H1 du 14/11) : des chiffres, et pas de condensateur au slot SD

**[en ligne]** pour les trois premières lignes.

| Poste | Courant | Source |
|---|---|---|
| Carte SD en SPI | 100 mA en moyenne sur 1 s ; **pics de plusieurs centaines de mA pendant ~1 ms** | spécification SD v4.10 ; note Swissbit « SD Memory Card Design-in » |
| Rétroéclairage d'un œil | 27,6 mA (module QDtech) ; dalle 40 mA typ, 60 mA max | lcdwiki MSP1281 ; fiche de la dalle LH128R-IG01 |
| Module œil complet | ~31 à 42 mA | Waveshare (31,2 mA max) ; lcdwiki (0,14 W) |
| ESP32 en émission | 355 mA | `ESP32-S3-WROOM-1-datasheet.md` |

Total : 355 + 100 + 2 × 42 + écran bouche (inconnu) + capteurs ≈ **550-600 mA** pour 600 mA
garantis, sans compter les pics de la SD.

- **J11 n'a aucun condensateur dédié** : les plus proches sur 3V3_D sont C1/C2 (ESP32) et C_D2.
  Swissbit demande de tamponner ces pics par des condensateurs, sous peine de chute de tension
  et de protection en écriture. → Ajouter 10 à 22 µF + 100 nF au pied de J11.
- Le module QDtech (seul schéma trouvé) porte son propre régulateur XC6206 et un transistor sur
  BLK : R1 = 0 Ω vers 3V3_D convient pour ce modèle. **Cinq ordres de broches différents**
  existent selon le vendeur ; celui du schéma (`VCC GND SCL SDA RES DC CS BLK`) n'est aucun des
  cinq tel quel (le QDtech commence par GND, VCC). J4/J5 restent provisoires tant que le module
  n'est pas choisi (question 1 du 27/09).

**Statut (15/11, décisions de Gilles).** C_SD2 100 nF + C_SD1 10 µF ajoutés au pied de J11
(feuille `connector`, netlist : seuls ces deux condensateurs s'ajoutent sur 3V3_D/GND).
Modules des yeux **pas encore choisis** : J4/J5 restent provisoires, bloquant pour le layout
de Devant et pour boucler H1.

### A5 — J2 (batterie) en JST-PH : 2 A par contact

JST donne **2 A avec du fil AWG 24** pour la série PH, 1 A en AWG 28 pour la série SH
**[en ligne, pages série jst-mfg.com]**. J14 (1,07 A), J9 et J10 sont dans la plage. **J2
voit le courant batterie**, soit jusqu'à 2,6 A sans plafond (A1). Avec le plafond à 1,5 A et un
harnais en AWG 24, c'est acceptable ; sinon passer à un connecteur de calibre supérieur. À
décider avant de figer l'empreinte.

---

**Statut (15/11, décision de Gilles).** J2 passe en **JST-PH 6 broches**, BAT+ et BAT− doublés
(1-2 BAT+, 3-4 BAT−, 5 NTC, 6 retour NTC sur GND) : 4 A de capacité théorique pour la même
série. Netlist : seules les broches de J2 changent. Harnais à refaire en 6 fils AWG 24.

## B. Corrections et améliorations

- **B1 — R_ISET = 890 Ω n'existe pas** en série normalisée. Prendre **887 Ω 1 %** (E96) :
  I_CHG = 890 / 887 = 1,003 A. La datasheet exige 1 % [bq24075 §8.3, note 1].
- **B2 — C_PAM_BULK : 22 µF 10 V** sur un rail à 5,1 V. Le PAM8406 demande ≥ 20 µF
  [`PAM8406.md`] et une céramique 10 V perd une grande part de sa valeur sous 5 V. La BOM a
  déjà retenu 16 V pour C_B1/C_B2 pour cette raison : aligner C_PAM_BULK, et choisir les
  références de ces trois condensateurs pour lire leur courbe de polarisation (non chiffrable
  sans référence).
- **B3 — Bibliothèques absentes des tables du projet.** Les symboles et empreintes
  `BQ24075RGTR`, `FS8205` et `MT3608` sont appelés par les schémas, mais `sym-lib-table` et
  `fp-lib-table` ne déclarent que `lcsc_*`. Ils viennent donc des tables globales du poste :
  sur une autre machine, F8 échouera. → Les déclarer dans le projet (`${KIPRJMOD}/libs/…`).
- **B4 — ST25DV : mauvaise variante de datasheet.** Le schéma monte un **ST25DV04KC**
  (datasheet DS13519), le PDF local est celui du ST25DV04K (DS10925). Le schéma est conforme au
  KC : 1 V_EH, 2 AC0, 3 AC1, adresses 0x53 / 0x57, découplage 10 nF + 100 nF **[en ligne,
  DS13519 Rev 2]**. Le KC ne donne C_TUN qu'en typique (28,5 pF) : la plage 26,5-30,5 pF du
  guide vient de l'ancienne fiche. → Télécharger DS13519 et refaire la synthèse.
- **B5 — J7 (yeux) : broches 9 et 10 libres.** Six signaux dont une horloge à 40-80 MHz
  reviennent par une seule masse sur 15-20 cm. Passer J7.9/J7.10 et Devant J2.9/J2.10 à GND
  ne coûte rien tant que rien n'est routé. Remarque de conception, sans source fabricant.
- **B6 — TP_FB1 sur le nœud FB du boost.** Pont 750 k / 100 k, donc ≈ 88 kΩ d'impédance à côté
  d'un nœud qui commute à 1,2 MHz. Une pastille de 1,5 mm y fait antenne. → La coller au
  diviseur, ou la supprimer (TP_5VB1 donne la même information). Remarque de conception.
- **B7 — DW01A : première connexion de la cellule.** Une fiche de la famille (Fortune DW01A+G)
  précise que la décharge peut rester bloquée au premier branchement, jusqu'à un passage sur
  chargeur ; la fiche H&M locale ne le dit pas **[en ligne]**. → Ajouter à la procédure de
  montage : brancher l'USB une fois après avoir connecté la cellule. Les seuils changent d'un
  fabricant à l'autre (150 mV chez H&M, 200 mV chez Fortune +G) : lire la fiche du fabricant
  réellement livré sous C351410.
- **B8 — Pas de superviseur de reset.** Espressif recommande un superviseur vers 3,0 V quand
  l'alimentation monte lentement ou vient d'une batterie **[en ligne, schematic checklist]**. Ici
  la fin de batterie repose sur la coupure firmware par VBAT_SENSE (pas encore écrite) et sur le
  détecteur interne. Option : une empreinte sur EN. Le même guide recommande une pull-up
  externe sur GPIO0 (TP_GPIO0 seul aujourd'hui, point H11 du 27/09).
- **B9 — GPIO45 (BTN1) : à trancher par une lecture d'eFuse.** L'agent rapporte que la
  datasheet du module dit, pour les modules à PSRAM, que VDD_SPI est fixé par eFuse et que
  GPIO45 est sans effet. C'est l'inverse de ce qu'écrit `ESP32-S3-datasheet.md`, et je n'ai
  **pas pu le vérifier** dans le PDF local (texte non décodable). → Sur la DevKitC :
  `espefuse.py --port /dev/ttyACM0 summary`, lire `VDD_SPI_FORCE`. S'il vaut 1, la contrainte
  « jamais d'appui sur BTN1 au démarrage » tombe ; sinon, le forcer au provisioning.
- **B10 — Gain audio en matériel.** Le gain du PAM8406 vaut 2 × R_F / R_I avec R_F = 142 kΩ et
  R_I = 18 kΩ [PAM8406 PDF, Application Information]. Une résistance série entre C_PAM_INx et
  INx le réduit (2 × 142 / (18 + R_s)) : deux empreintes 0402 en 0 Ω suffiraient à garder
  l'option, en complément du plafond numérique à −22 dB.
- **B11 — BOM** : la note de U7 (« Boost 3.7V->5V 2A ») est à corriger après A3.

**Statut de la section B (15/11, décisions de Gilles).** B1 (887 Ω), B2 (16 V), B3 (tables du
projet, avec A2), B4 (DS13519 ajoutée, brochage conforme), B5 (J7.9/10 et Devant J2.9/10 à GND),
B6 (TP_FB1 retiré avec A3), B7 (procédure de première connexion), B8 (R_GPIO0 10 kΩ ; pas de
superviseur), B10 (R_INL/R_INR 0 Ω) et B11 (note BOM de U7 réécrite avec A3) : faits. B9 (lecture
de l'eFuse VDD_SPI_FORCE) reste à faire sur la DevKitC.

## C. Levé ou confirmé

- **H6 (fader PTA6043) : levé.** La datasheet écrit « Terminal 1-2 Output Voltage / Terminal 1-3
  Input Voltage » : 1 et 3 sont les extrémités de piste, 2 le curseur. L'empreinte LCSC
  (1 en −36,75 ; −1,75, 2 en −36,75 ; +1,75, 3 en +36,75 ; −1,75) est identique, pattes de
  fixation comprises, à l'empreinte officielle KiCad `Potentiometer_Bourns_PTA6043_Single_Slide`
  (fichier relu directement, décalage de −36,75 ; −1,75). Le schéma (1 = 3V3_D, 2 = curseur,
  3 = GND) est donc correct. Contrôle de réception : 10 kΩ entre les deux plots opposés du
  même bord.
- **WS2812B V5** : VDD 3,7-5,3 V, V_IH ≥ 2,7 V, 12 mA par couleur, 0,6 mA au repos, timings de
  la synthèse **[en ligne, fiche LCSC C2761795]**. Pilotage en 3,3 V conforme.
- **Module ESP32** : IO35-37 indisponibles (PSRAM octale), IO47/48 en 3,3 V sur N16R8, GPIO3 =
  ADC1_CH2, RC de EN 10 kΩ / 1 µF, résistances USB 22 ou 33 Ω **[en ligne, Espressif]**.
- **Brochages relus dans le texte des PDF** : DW01A (application typique R1 100 Ω, R2 1 kΩ,
  C1 0,1 µF), TMAG5273 (TEST à GND, A1 = 0x35), ADS7830, ICS-43434, BLM21PG221SN1D (2 A à 85 °C).
- L'outil de netlist place les broches aux coordonnées exactes que KiCad imprime dans
  `cote1/ERC.rpt` : sa géométrie est validée sur ce cas.

---

## D. Guide de layout

### Vérifié

- Empilement JLC04161H-7628 : 35 µm / préimprégné 0,2104 mm εr 4,4 / 15,2 µm / âme 1,065 mm
  **[en ligne, jlcpcb.com/impedance]**. Cuivre-bord 0,2 / 0,4 mm, via-via 0,2 mm, trou-trou
  0,45 mm, sérigraphie 0,15 / 1,0 mm, suppléments de perçage : confirmés **[en ligne]**.
- Calculs refaits : largeurs IPC-2221 (0,12 / 0,30 / 0,78 / 1,37 mm, ×5 en interne 0,5 oz),
  bus I2C ≈ 251 pF avec 4,7 kΩ, antennes NFC par la formule de Wheeler modifiée (carrée 35 mm
  9 spires : 4,85 µH ; 40 mm 8 spires : 4,62 µH), fenêtre du VEML7700, quart d'onde des fils du
  haut-parleur.

### À corriger

- **G1 — VSYS classé PWR_1A (0,8 mm) au motif « entrée 1,07 A, charge 1 A ».** Sur batterie,
  VSYS porte le courant de décharge : jusqu'à ≈ 2,6 A entre U8.10/11, C_B1 et L1 (A1). Même
  remarque pour la ligne « IN/OUT/BAT dimensionnés pour 1,07 A / 1 A ». → VSYS en zone ou en
  1,5 mm sur ce tronçon, comme PWR_BAT ; 0,8 mm reste juste pour 5V_USB.
- **G2 — §2.4, vias du bq24075** : signaler les cinq vias de l'empreinte (A2).
- **G3 — §1.2** : le minimum JLCPCB en multicouche est 0,09/0,09 mm, pas 0,10 (sans effet, on
  route à 0,15). « 0,3/0,5 sans surcoût » est une déduction, pas une phrase de JLCPCB. εr de
  l'âme : 4,6.
- **G4 — §2.1, assemblage** **[en ligne]** : JLCPCB exige mires, trous d'outillage (Ø 1,152 mm,
  non métallisés) et rails de 5 mm, et les ajoute s'ils manquent ; en double face, un jeu de
  mires **par face** ; taille minimale 70 × 70 mm pour les rails standard. L'assemblage double
  face en offre « Economic » n'est **pas confirmé** : à vérifier au devis.
- **G5 — À ajouter** : condensateurs du slot SD (A4), TP_FB1 (B6), points de test des horloges
  posés sur la piste et non en dérivation, plage de C_TUN (B4).

**Statut (15/11).** G1 (nouvelle classe PWR_VSYS 1,5 mm, IN/OUT/BAT du bq24075), G2 (vias de
l'empreinte, avec A2), G3 (0,09 mm multicouche, εr de l'âme 4,6, formulation des suppléments),
G4 (exigences d'assemblage JLCPCB) et G5 (SD, TP sur horloges, pas de TP sur FB, C_TUN) : faits
dans le guide.

### Toujours sans valeur vérifiée

Largeur et espacement pour 90 Ω différentiel sur cet empilement : le calculateur JLCPCB n'est pas
consultable sans saisie. Espressif demande 90 Ω ±10 % ; la paire est en Full-Speed et courte.

---

## E. Ordre proposé

1. Empreinte du bq24075 (A2), tables de bibliothèques (B3), R_ISET (B1), C_PAM_BULK (B2).
2. Décider : condensateurs SD (A4), J7.9/10 à GND (B5), empreintes de démarrage progressif de Q1
   (A1), connecteur J2 (A5), TP_FB1 (B6).
3. Banc du boost MT3608 (A3) et lecture de l'eFuse VDD_SPI (B9).
4. Guide de layout : G1 à G5.
5. Firmware : plafond global de courant (A1), en plus du plafond audio.
