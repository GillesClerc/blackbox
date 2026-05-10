# EscapeBox — Project Vision

*Document de vision — Mai 2026*

---

## L'idée en une phrase

Une **box physique électronique**, posée au centre d'une table, sur laquelle des joueurs résolvent des énigmes immersives ensemble. Elle se transforme à chaque scénario téléchargé, comme une console de jeu change de jeu.

---

## L'envie de départ

Je veux créer un produit qui **réunit des gens autour d'une table** — famille, amis, collègues — pour vivre ensemble une expérience d'escape game à la maison. Pas du papier à découper, pas une application sur smartphone, pas un jeu de cartes. Un **objet physique qui s'illumine, parle, s'ouvre, réagit**, et qui raconte une vraie histoire.

Ce produit doit être **rejouable** : on achète la box une fois, on achète des scénarios comme on achète des jeux pour une console. Chaque nouveau scénario change l'identité de la box — le thème, la voix narrative, les énigmes, les objets physiques.

---

## La box

Un **cube ou parallélépipède en bois et ardoise**, monté sur un plateau rotatif. Les joueurs assis autour de la table font tourner la box vers eux pour interagir avec les différentes faces. Chaque face accueille des capteurs, des écrans, des zones tactiles, des capteurs cachées. L'ensemble ressemble à un **objet artisanal de collection** — pas un gadget plastique, un vrai objet qu'on garde sur une étagère.

À l'intérieur : un ESP32-S3, une batterie rechargeable, tous les capteurs nécessaires pour créer des énigmes variées : NFC, clavier capacitif (MTCH2120), boussole, accéléromètre, micro, capteur de souffle, capteur d'angle magnétique, température, lumière, capteur Hall analogique. Un compartiment s'ouvre mécaniquement via un servo — c'est le moment de révélation physique qui crée le wahou.

Le cube contient également un haut-parleur, un micro et deux écrans : un écran 3-4" de haute résolution, et un petit écran rond sur une autre face.

Un bus backbone interne (connecteurs JST) court le long de la structure. Chaque PCB satellite — face keypad, face NFC, face boussole — se plug/déplugg proprement sur ce bus. L'assemblage est propre, maintenable, et permet d'upgrader une face sans tout démonter.

---

## L'expérience de jeu

Le joueur déballe la box. Il pose un médaillon NFC sur la surface — la box s'allume, une voix raconte le début de l'histoire. L'écran affiche une carte. Les énigmes s'enchaînent : des codes à trouver, des choses à dire, des un aimant à utiliser, un souffle à produire, un angle précis à atteindre. 

Chaque joueur a un rôle naturel : l'un tourne la boussole, l'autre lit les indices sur l'écran, le troisième tient l'objet magnétique sur la bonne zone. Certaines énigmes nécessitent deux personnes simultanément — la coopération est dans le design, pas dans les règles.

---

## Le modèle — Lunii pour les adultes

Je m'inspire du modèle de **Lunii** (la conteuse pour enfants) : une box vendue une fois, des histoires achetées sur une plateforme web et synchronisées sur l'appareil. La box fonctionne **100% offline** pendant le jeu — elle ne se connecte au WiFi qu'au moment de la synchronisation des nouveaux scénarios.

Un compte en ligne gère la bibliothèque du joueur. L'achat d'un scénario (19-29 CHF) le rend disponible sur la webapp, et la box le télécharge à la prochaine synchronisation.

---

## La plateforme

En parallèle de la box grand public, je veux offrir aux **professionnels** (escape rooms, animateurs, enseignants, musées) la possibilité de **créer leurs propres scénarios** via une webapp intuitive. Un éditeur visuel type drag-and-drop permet de définir les énigmes, les actions, les audios, sans coder. Les scénarios sont décrits dans un format YAML standard qui alimente directement le firmware de la box.
À terme, une marketplace permet aux créateurs de vendre leurs scénarios, avec une commission.

---

## Les deux gammes envisagées

**Escape Box Lite** (99-129 CHF) : format rectangle, boîtier MDF + finition noire, tous les capteurs essentiels, 1 compartiment servo, destinée au grand public, à l'éducation, aux cadeaux. 

**Escape Box Pro** (199-249 CHF) : format cube ou polyèdre rotatif sur plateau bois tourné, boîtier bois massif + ardoise + laiton brossé, 2 compartiments, capteurs supplémentaires. Destinée aux passionnés, aux escape rooms, aux collectionneurs.

---

## Ce qui me tient à cœur

- Que la box soit **belle** — un objet qu'on est fier d'avoir sur la table
- Que les scénarios soient **vraiment bien écrits** — narration, cohérence, surprise
- Que l'expérience soit **accessible à tous** — du gamin de 10 ans à la grand-mère
- Que le système soit **fiable** — rien de plus frustrant qu'une box qui plante au milieu d'une partie
- Que la fabrication soit **artisanale et locale** dans la mesure du possible — bois du Jura, assemblage en Suisse

---

## La vision à 3 ans

Un catalogue de 15-20 scénarios, une communauté de créateurs actifs sur la marketplace, des partenariats avec des escape rooms et des établissements scolaires en Suisse et en France. Un produit reconnu dans le monde du jeu de société premium comme **la** escape box électronique rejouable de référence en français.

---

*Ce document est la vision du projet, pas une spec technique. Il peut évoluer.*
