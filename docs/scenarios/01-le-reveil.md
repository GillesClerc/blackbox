# Scenario 1 — Le Reveil

## Concept
Le cube est un personnage endormi. Il ne se reveille que dans le noir.
Style narratif : personnage grognon/attachant, voix "Animal Crossing"
(syllabes synthetisees, intonation expressive, texte affiche sur e-ink).

## Materiel utilise
- 2x GC9A01 (yeux) — expressions, clignements, regard
- 1x e-ink 2.9" SSD1680 (bouche) — bulles de dialogue, texte mot-a-mot
- VEML7700 — detection lumiere ambiante
- WS2812 LEDs — ambiance, feedback visuel
- PCM5122 + HP — voix synthetisee, sons d'ambiance
- ICS-43434 micro — detection souffle, niveau sonore, "ecoute" du joueur
- BMP280 — confirmation souffle (variation pression)
- PN532 — lecture badge NFC
- Potentiometres — interaction joueur (reglage notes)

## Deroulement

### Phase 0 — Sommeil (lumiere allumee)
- Les yeux sont eteints, le e-ink est vide
- Le cube semble inerte, un simple objet
- VEML7700 mesure la lumiere en continu (seuil ~50 lux)
- Au bout de 2-3 minutes, **indice sonore** :
  un grognement/ronchonnement discret sort des HP
  ("mmrrghh... trop de lumiere...")
- Les LEDs clignotent tres faiblement, comme une respiration agacee
- Si le joueur ne reagit pas : grognements de plus en plus forts,
  un oeil s'entrouvre 1 seconde puis se referme ("aaargh!")

### Phase 1 — Reveil (lumiere eteinte)
- VEML7700 detecte obscurite (< 10 lux)
- Pause de 2 secondes (suspense)
- Les LEDs s'allument doucement en bleu nuit
- Un oeil s'ouvre lentement (animation pupille qui se dilate)
- Puis le deuxieme oeil
- Clignement surpris (les deux yeux se ferment/ouvrent vite)
- Le e-ink affiche mot par mot : "... il fait noir ?"
- Son : babillage Animal Crossing (syllabes joyeuses)
- Les yeux regardent a gauche, a droite (curiosite)
- e-ink : "Ah ! Enfin ! Je dormais depuis si longtemps..."

### Phase 2 — Presentation du personnage
- Le personnage se presente (texte e-ink + voix AC)
- Il explique qu'il est piege dans cette boite
- Les yeux changent d'expression selon l'emotion du dialogue :
  - Triste (pupilles basses, paupieres tombantes)
  - Excite (pupilles larges, brillantes)
  - Pensif (yeux qui regardent en haut)
- e-ink : "Aide-moi a sortir ! Mais d'abord..."
- Les yeux deviennent malicieux (pupilles retrecies, sourire)

### Phase 2b — "Comment tu t'appelles ?"
- e-ink : "Dis... comment tu t'appelles ?"
- Les yeux regardent le joueur avec curiosite (pupilles larges)
- ICS-43434 detecte le niveau sonore ambiant
  - Si silence > 3 secondes : yeux impatients, e-ink "Allez, dis-moi !"
  - Si le joueur parle (niveau sonore > seuil) :
    le personnage ecoute (yeux attentifs, oreilles tendues)
  - Mais le micro ne fait PAS de reconnaissance vocale
- Quel que soit ce que le joueur dit, le personnage repond :
  e-ink : "Ah bonjour... Machin !"
  Voix AC enthousiaste
- Les yeux sont ravis
- **A partir de maintenant, il appelle le joueur "Machin" dans tous
  les dialogues** ("Allez Machin !", "Bravo Machin !", "Machin, ecoute...")

#### Mecanisme "Plus fort !"
- Quand le joueur donne son nom, le personnage fait semblant
  de ne pas entendre :
  - 1ere tentative : e-ink "Quoi ? J'entends rien..."
    yeux plisses, main en cornet (expression confuse)
  - Le joueur repete plus fort
  - 2eme tentative : e-ink "PLUS FORT !"
    yeux grands ouverts, LEDs qui pulsent en rythme
  - Le joueur crie son nom
  - 3eme tentative (niveau sonore > seuil eleve) :
    e-ink "WOAAH PAS SI FORT !!"
    yeux effrayes, reculent, LEDs flash rouge
    puis : e-ink "... bon, je t'appellerai Machin."
    yeux malicieux, petit rire AC
- Le comedique vient du fait qu'il n'a jamais ecoute et appelle
  tout le monde "Machin" quoi qu'il arrive

### Phase 3 — Enigmes (donnees par le personnage)

#### Enigme 3.1 — "Ecoute bien, Machin"
- e-ink : "Machin, tu as l'oreille musicale ?"
- Le personnage joue une sequence de sons/notes
- Le joueur doit reproduire la sequence avec les potentiometres
  (chaque pot controle la hauteur d'une note)
- Yeux : se ferment pendant l'ecoute, s'ouvrent avec anticipation
- Feedback : LEDs vertes si correct, yeux contents
  LEDs rouges si faux, yeux decus, e-ink "Non non non Machin..."

#### Enigme 3.2 — "Souffle sur moi, Machin"
- e-ink : "Machin, j'ai froid... rechauffe-moi !"
- Le joueur doit souffler sur le micro (ICS-43434 detection de souffle)
- BMP280 detecte la variation de pression (confirmation)
- Les yeux deviennent joyeux, LEDs passent du bleu au orange/rouge
- Si le joueur souffle trop fort : yeux effrayes,
  e-ink "PAS SI FORT Machin !!"

#### Enigme 3.3 — "Le badge secret"
- PN532 attend un tag NFC specifique
- e-ink : "Machin, j'ai oublie le mot de passe..."
- Les joueurs doivent trouver le tag NFC cache dans la piece
- Quand scanne : les yeux s'illuminent, LEDs arc-en-ciel
- e-ink : "OUI ! Ca me revient !"
- Le personnage "se souvient" et debloque la suite

### Phase 4 — Liberation
- Toutes les enigmes resolues
- e-ink : "Machin... merci. Vraiment."
- Les yeux se remplissent de larmes de joie (animation)
- LEDs celebration (arc-en-ciel, pulsation)
- Voix AC emotionnelle (syllabes lentes, pitch bas)
- e-ink : "Je suis libre... grace a toi, Machin."
- Les yeux se ferment doucement (sourire paisible)
- e-ink final : temps total + score
- Les yeux se rouvrent une derniere fois, clin d'oeil,
  e-ink : "A bientot, Machin !" puis s'eteint

## Expressions des yeux (sprites/animations)
- Neutre : pupille centree, iris rond
- Content : pupille large, "sparkle" dans le coin
- Triste : pupille basse, paupiere superieure tombante
- Surpris : pupille tres large, yeux grands ouverts
- Enerve : pupilles petites, sourcils fronces (paupiere en V)
- Pensif : pupille en haut a droite
- Endormi : yeux mi-clos, paupiere lourde, ZZZ
- Clin d'oeil : un oeil ferme, l'autre ouvert
- Malicieux : pupilles retrecies, regard en coin
- Confus : yeux plisses, un plus ferme que l'autre
- Effraye : pupilles minuscules, yeux grands ouverts, tremblement
- Attentif : pupilles dilatees, fixes, paupieres relevees
- Ravi : pupilles en etoile, sparkles autour

## Notes techniques
- La voix "Animal Crossing" = lecture rapide de phonemes pre-enregistres
  en MP3 (stock de ~50 syllabes), pitch-shift aleatoire pour variete.
  Chaque mot du e-ink declenche 1-3 syllabes audio.
- Le e-ink utilise le partial refresh (0.3s) pour le texte mot-a-mot.
  Full refresh entre chaque phase pour eviter le ghosting.
- Les animations d'yeux tournent a ~15-25 FPS sur les GC9A01 (SPI 80MHz).
- La detection lumiere/noir doit avoir une hysteresis pour eviter
  les faux declenchements (seuil ON=10 lux, seuil OFF=50 lux).
- Le mecanisme "Plus fort" utilise 3 seuils de detection niveau sonore
  sur ICS-43434 (ex: 60 dB, 75 dB, 85 dB). Le 3eme seuil declenche
  la reaction "trop fort" et la revelation du nom "Machin".
- "Machin" est un placeholder dans les textes du scenario JSON.
  Le firmware le remplace a l'affichage (toujours par "Machin" — c'est
  le gag, le personnage ne retient jamais le vrai nom).
