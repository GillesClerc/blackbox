# Plan — Suite EscapeBox : Design cube, servo reveal, web platform, business

## Context

Le hardware AliExpress arrive dans 2-4 semaines. En attendant, on peut avancer sur trois fronts en parallèle :
1. **Design du boîtier** — cube 6 faces actives (décision validée)
2. **Web platform** — construire la fondation Next.js + Supabase + landing + API + simulateur
3. **Business** — stratégie FFF → Kickstarter

Réponses utilisateur structurant ce plan :
- Boîtier : **cube 6 faces actives**
- Servo reveal : **compartiment qui s'ouvre sur un écran (GC9A01 round) affichant QR code → leaderboard + son**
- Web priorités : **toutes les 4** (landing, auth/catalogue, API sync, simulateur)
- Business : **FFF d'abord, puis Kickstarter si traction**

---

## 1. Design boîtier — Cube 6 faces actives

### Assignation des faces (à valider avec Gilles)

| Face | Contenu | Composants |
|------|---------|-----------|
| **Avant** (face principale) | Écran narratif 4" | ILI9488 SPI, WS2812 border LEDs |
| **Droite** | Keypad capacitif | MPR121/MTCH2120 12 pads, WS2812 rétroéclairage |
| **Gauche** | Zone NFC | PN532 (antenne cachée derrière bois fin), WS2812 anneau |
| **Haut** | Capteurs ambiance | VEML7700 (lumière), BMP280 (souffle), LSM6DSOTR (IMU), Hall A3144E |
| **Dos** | Face révélation (victoire) | GC9A01 1.3" round (QR leaderboard), WS2812 border celebration |
| **Dessous** | Technique | USB-C charge/prog, slot SD card, interrupteur ON/OFF, face boussole GC9A01 (optionnel) |

### Face "révélation" — reveal sans servo (Phase 1)

**Décision** : pas de servo en Phase 1 pour les scénarios digitaux. Une face du cube reste "inerte" pendant la partie et s'active à la victoire — plus fiable, zéro mécanique.

Quand le scénario se termine (victoire) :
1. La face "révélation" s'allume — WS2812 border + GC9A01 rond au centre
2. GC9A01 affiche : QR code → `escapebox.ch/score/{session_token}` + score/chrono
3. WS2812 tout autour du cube font une animation celebration
4. Audio joue la piste de victoire
5. ILI9488 principal affiche aussi l'écran de fin

**Avantage** : même effet "wahou" sans pièce mobile. Le GC9A01 commandé en 3 exemplaires suffit (face révélation + face boussole + spare).

**Servo en Phase 2** : réintroduit optionnellement pour les scénarios avec pack physique (objets inclus dans la boîte). Déclaré dans le YAML via `hardware_required: [servo_main]`. Le driver est déjà écrit et attend.

### Dimensions suggérées
- ILI9488 4" module : ~95×65mm → face min. **120×120mm**
- Cube : **120×120×120mm** extérieur (bois/MDF 6mm) = intérieur ~108mm
- Compartiment arrière : ~40mm de profondeur, largeur complète de la face

### Composants face "haut" (à ajouter à la commande si pas déjà fait)
- BMP280 breakout (souffle) — pas dans le panier AliExpress actuel
- MLX90614 breakout (IR temp) — optionnel Phase 1
- Hall sensor A3144E (magnet detection) — à vérifier

### JST backbone interne
Un câble plat JST court le long de l'arête interne du cube. Chaque face satellite se branche :
- 3.3V + GND
- I2C (SDA/SCL)
- Les GPIOs spécifiques à la face (WS2812 data, servo PWM, etc.)

---

## 2. Web Platform — Sprint 4 semaines

Créer `web/` dans le repo (monorepo décidé). Stack : Next.js 14 App Router + Supabase + Stripe + shadcn/ui.

### Semaine 1 — Fondation technique
- Init `web/` : `npx create-next-app@latest web --typescript --tailwind --app`
- Supabase project (prod + staging)
- Auth : email + Google OAuth via Supabase Auth
- Layout global, shadcn/ui, thème dark escape game
- Variables d'env, deploy sur Coolify (domaine escapebox.ch)

### Semaine 2 — Pages core
- `/` Landing page : hero, features, capture email waitlist
- `/app/shop` Catalogue (statique Phase 1 — juste le Capitaine Verdier)
- `/app/library` Bibliothèque (scénarios achetés par l'user)
- `/app/account` Compte, abonnement (vide pour Phase 1)
- Schema Supabase : `users`, `devices`, `scenarios`, `licenses`

### Semaine 3 — API sync box + Stripe
- `POST /api/box/auth` — HMAC challenge/response, retourne JWT
- `GET /api/box/sync` — liste scénarios autorisés pour cette box
- `POST /api/box/session` — upload score de partie
- Stripe checkout (one-shot, test mode)
- Webhook Stripe → création licence Supabase
- `/app/devices` association box (UI simple, pas encore BLE)

### Semaine 4 — Simulateur scénario
- `/studio/simulate` — player YAML dans le navigateur
- Parse le fichier YAML (même format que le firmware)
- State machine JS qui reproduit le moteur de scénario
- Interface : affiche l'état courant, les actions déclenchées, permet d'injecter des événements (click "NFC détecté", "code entré", etc.)
- Utile pour valider les scénarios sans hardware

### Fichiers critiques à créer
```
web/
  app/
    (auth)/login/page.tsx
    (auth)/register/page.tsx
    app/shop/page.tsx
    app/library/page.tsx
    app/devices/page.tsx
    app/account/page.tsx
    studio/simulate/page.tsx
  api/
    box/auth/route.ts
    box/sync/route.ts
    box/session/route.ts
    webhooks/stripe/route.ts
  components/
    ScenarioPlayer.tsx    ← simulateur
    ScenarioCard.tsx
  lib/
    supabase.ts
    stripe.ts
    scenario-engine.ts   ← port JS du moteur YAML
```

---

## 3. Business — FFF → Kickstarter

### Phase FFF (maintenant → Phase 1 hardware validé)
- Landing page avec email capture dès semaine 1
- Tester le scénario Capitaine Verdier avec famille/amis
- Collecter retours qualitatifs structurés (formulaire post-partie)
- Objectif : 10-15 testeurs, taux complétion > 80%

### Phase Kickstarter (si traction confirmée)
- Lancer uniquement après validation Phase 1 (hardware + scénario + web)
- Objectif : 50-100 boxes early adopters (~150 CHF/box incluant 1 scénario)
- Financement : PCB custom Phase 2 + certification CE-RED initiale
- Rewards : box Lite early adopter, accès à 2 scénarios, nom dans les crédits
- Plateau : Suisse + France (FR), potentiellement Belgique
- Pas de Kickstarter US tant que la CE-RED n'est pas faite

### Validations business à faire pendant les 4 semaines web
- Définir le prix final box Lite (99-129 CHF → tester avec proches)
- Définir le prix scénario (19-29 CHF → tester willingness-to-pay)
- Trouver le scénariste (réseau, freelance)
- Créer la charte graphique et le nom de marque (EscapeBox ? autre ?)
- Réserver escapebox.ch si pas déjà fait

---

## 4. Mises à jour docs et mémoire à faire après validation du plan

### CLAUDE.md
- Ajouter section `## Boîtier Phase 1` avec tableau faces et composants
- Mettre à jour contexte projet (ESP32-S3 N16R8 kit commandé)

### docs/escapebox-fsd.md
- Section 2.2.2 : préciser l'assignation face par face du cube
- Ajouter la mécanique servo : GC9A01 dans le compartiment = reveal QR leaderboard
- Mettre à jour Phase 1 hardware checklist (BMP280 à commander)

### memory/
- `project_box_design.md` : cube 120mm, 6 faces, compartiment GC9A01
- `project_web_plan.md` : sprint 4 semaines, structure web/
- `project_business.md` : FFF → Kickstarter, prix à valider

---

## 5. Questions ouvertes (à traiter en session suivante)

1. **BMP280** (souffle) : à commander sur AliExpress (absent du panier actuel).
2. **Face boussole** : la boussole AS5600 est affichée sur l'ILI9488 principal ou sur un 2ème GC9A01 sur la face dessous/keypad ? À décider quand le proto sera assemblé.
3. **Nom de marque** : "EscapeBox" est générique — à travailler séparément avant Kickstarter.
4. **Scénariste** : qui et comment le trouver pour écrire 2-3 scénarios Phase 2 ?
5. **Servo Phase 2** : MG90S recommandé à la place du SG90 (engrenages métal, même format, R-02 dans FSD).

---

## Ordre d'exécution

1. **Maintenant** : mettre à jour CLAUDE.md, FSD, mémoire avec les décisions de ce plan
2. **Semaine 1** : init projet `web/` + fondation technique
3. **Semaine 1-2** : landing page (capture emails = validation marché immédiate)
4. **Semaine 2-3** : auth + catalogue + API sync
5. **Semaine 4** : simulateur scénario
6. **À réception hardware** : valider tous les drivers, assembler proto cube
