# Questions ouvertes — schémas des satellites Dessus, Devant, Côté 1, Côté 3 (2026-09-27)

> Relevées pendant le dessin des quatre schémas. Pour chacune, **le choix fait par défaut** au
> schéma est indiqué : rien n'est bloquant pour l'ERC, mais certaines réponses changeront des
> empreintes ou des brochages avant le layout. Cocher / répondre au fil de l'eau.

## Devant (visage)

1. **Module des yeux GC9A01** : quelle référence exacte (vendeur, lien) ? Sans sa fiche, l'embase
   J4/J5 est **provisoire** : barrette femelle 1×8 au pas de 2,54 mm, ordre supposé
   `VCC GND SCL SDA RES DC CS BLK`. À confirmer : ordre des broches, présence d'un régulateur
   (VCI/IOVCC max 3,3 V selon `GC9A01A.md`), courant du rétroéclairage, montage (embase,
   soudé, nappe FPC ?).
2. **Rétroéclairage des yeux** : pas de GPIO libre, donc BLK est relié à 3V3_D par **R1 = 0 Ω**
   (toujours allumé quand la box est allumée). Acceptable ? Ou faut-il une résistance de
   limitation (valeur selon le module), voire une commande par PWM (il faudrait alors libérer
   une broche) ?
3. **Écran bouche** : type toujours non choisi (pas e-ink). J6 est une embase provisoire dans
   l'ordre de J7b (`3V3_D GND MOSI SCLK CS DC RST BUSY`).
4. **Halo WS2812B** : **8 LED** placées par défaut (la BOM parle de ~8-10 externes au budget
   batterie). Combien, et quelle disposition (anneau autour du visage, arc au-dessus des yeux…) ?
5. **Position des capteurs** : ouverture de la bouche pour le BMP280 (trou d'évent orienté
   vers elle), fenêtre du VEML7700 face aux yeux — cotes du visage à fixer pour le layout.
6. **VEML7700, code LCSC** : la BOM et l'empreinte utilisent **C1850416**, la synthèse
   `VEML7700.md` cite **C137509**. Lequel est le bon (stock, variante -TT) ?

## Dessus (voix + NFC)

7. **Antenne NFC** : la boucle ANT1 est un symbole **sans empreinte** (hors BOM). Il faut une
   empreinte de bobine dimensionnée pour **≈ 4,8 µH** (résonance avec C_TUN = 28,5 pF,
   26,5-30,5 pF). Quelle surface disponible sur le PCB Dessus ? Je la génère par script, ou tu
   passes par l'outil ST (eDesignSuite / « NFC inductance ») ?
8. **Couvercle Dessus** : matériau et épaisseur ? Nécessaire pour dimensionner la boucle et la
   zone de garde sans cuivre prévue au cartouche (point à vérifier dans la note d'application
   d'antenne ST, pas encore dans `docs/datasheets/`).
9. **C3 (accord d'antenne, DNP)** : garder cette place de condensateur en parallèle de C_TUN,
   ou l'enlever si la bobine est bien dimensionnée ?
10. **Haut-parleur** : câblé en **fils directs** depuis J10 de la Main (rien sur le PCB Dessus,
    comme le prévoyait le cartouche). Préfères-tu un connecteur relais sur le satellite ?

## Côté 1 (panneau de contrôle)

11. **Toggles SW1/SW2 et boutons BTN1/BTN2** : ils sont montés **en panneau** et raccordés par
    fils sur **J4-J7 (JST-PH 2 broches)** — la BOM décrit les toggles « panel 12 mm ». OK, ou
    les veux-tu soudés sur le PCB (il faudra alors leurs empreintes) ?
12. **Boutons poussoirs BTN1/BTN2** : quelle référence (style assorti au RR111C1921 ? à LED ?)
    Contrainte : contact vers 3V3_D, actif haut.
13. **Faders PTA6043** : le symbole est le `…2015CPB103` (levier métal « CP »), l'empreinte
    LCSC le `…2015DPB103` (levier « DP ») — même empreinte, mais quel style et quelle
    longueur de levier veux-tu ? Sens : 3V3_D sur la broche 1, GND sur la 3 (inversable par
    firmware).
14. **Cadre métallique des faders** : pattes 4-7 reliées à **GND** par défaut. OK ?
15. **Pots PDB181** : l'empreinte n'a que 3 pastilles (fixation par l'écrou du canon M7).
    Suffisant mécaniquement, ou faut-il des pattes de maintien / un support ?

## Côté 3 (zone magique)

16. **Aimant et distance** : A1 (±40/80 mT) est retenu. Quel aimant (taille, grade) et à quelle
    distance du capteur à travers la paroi ? À fixer pour valider la plage et la position de U1.

## Toutes les faces

17. **Trous de fixation et contour** : aucun trou de montage ni contour n'est au schéma. Taille
    de chaque PCB et mode de fixation (vis, entretoises, clips imprimés) ?
18. **Références dans la BOM** : préfixées par face (`DEVANT-U1`, `COTE1-SL1`…), dans l'unique
    `hardware/main/BOM/02-bom-lcsc.csv`. Pour la commande JLCPCB il faudra une BOM par carte :
    on les découpe au moment de commander ?
19. **Bibliothèque `lcsc_imported.kicad_sym`** : elle contient des **symboles en double**
    (TF-01A ×4, LSM6DSOXTR ×4, ESP32-S3, PCM5122… ×3, VEML7700, BMP280, TMAG5273, ADS7830,
    PTA6043, PDB181 ×2). KiCad ne lit que le premier ; je peux la dédoublonner (une seule copie
    par symbole, fichier CRLF conservé) — d'accord ?
20. **ESD en façade** : aucune protection sur les lignes qui sortent vers des organes touchés
    (SW1/SW2/BTN1/BTN2, faders, pots). À prévoir si la façade est métallique ?
