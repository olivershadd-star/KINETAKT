<div align="center">

# KINETAKT
### ELEKTRON-STYLE SEQUENCER · SOUND + MIDI TRACKS · FREE

![Status](https://img.shields.io/badge/VERSION-0.7.1-3DDC84?style=for-the-badge&labelColor=10151E)
![Windows](https://img.shields.io/badge/WINDOWS-10%20%2F%2011_X64-19B9FF?style=for-the-badge&labelColor=10151E)
![VST3](https://img.shields.io/badge/VST3-ABLETON_LIVE-19B9FF?style=for-the-badge&labelColor=10151E)
![Licence](https://img.shields.io/badge/LICENCE-AGPL--3.0--or--later-F5C343?style=for-the-badge&labelColor=10151E)

**ANKS LABS // BUILD. BREAK. REWIRE. LISTEN.**

[![TÉLÉCHARGER](https://img.shields.io/badge/T%C3%89L%C3%89CHARGER-DERNI%C3%88RE_VERSION-EF343E?style=for-the-badge&labelColor=10151E)](../../releases/latest)
[![TOUTES LES VERSIONS](https://img.shields.io/badge/TOUTES_LES-VERSIONS-F5C343?style=for-the-badge&labelColor=10151E)](../../releases)

</div>

![KINETAKT — sélection d'un pas et parameter lock de hauteur](docs/apercu.png)

> **FR —** KINETAKT (anciennement ELEKTAKT) est un séquenceur gratuit pour Windows, en application autonome et en **plugin VST3 pour Ableton**, inspiré de la façon de travailler des machines Elektron : des **parameter locks** par pas, des **trig conditions**, une **longueur et une vitesse par piste**, du micro-timing et des retrigs. Chaque piste joue soit un **sample** (avec 4 effets Airwindows), soit des **notes MIDI** envoyées vers Ableton ou un autre logiciel. Il reprend le moteur audio du projet KICK d'ANKS Labs.
>
> **EN —** KINETAKT (formerly ELEKTAKT) is a free Windows sequencer, standalone and as a **VST3 plugin for Ableton**, inspired by the Elektron workflow: per-step **parameter locks**, **trig conditions**, **per-track length and speed**, micro-timing and retrigs. Each track plays either a **sample** (with four Airwindows effects) or **MIDI notes** sent to Ableton or any other software. It reuses the audio engine of the ANKS Labs KICK project.

## 01 // TÉLÉCHARGER ET LANCER — DOWNLOAD & RUN

1. Ouvre **[la dernière version](../../releases/latest)** et télécharge `KINETAKT-Windows-x64-0.7.1.zip` (le numéro est dans le nom : prends toujours le plus grand).
2. Clic droit sur le ZIP → **Extraire tout**. Garde le dossier `demo-kit` à côté de `KINETAKT.exe`.
3. Lance **KINETAKT.exe**. Aucune installation. Un pattern de démonstration est chargé : appuie sur **Espace**.

![KINETAKT — scènes et crossfader](docs/apercu-scenes.png)

*Windows peut afficher « Windows a protégé votre ordinateur » : clique sur **Informations complémentaires → Exécuter quand même** (le programme n'est pas signé, c'est normal pour un projet gratuit).*

**EN —** Download the latest ZIP from Releases, extract everything, keep `demo-kit` next to `KINETAKT.exe` and run it. No installation.

## 02 // DANS ABLETON — THE VST3 PLUGIN

Le ZIP contient aussi **`VST3/KINETAKT.vst3`** : le même séquenceur, les mêmes écrans, dans Ableton Live (Live Lite compris).

1. Copie le dossier **`KINETAKT.vst3`** dans **`C:\Program Files\Common Files\VST3`**.
2. Dans Live : **Préférences → Plug-ins → Dossier système VST3 = On**, puis **Relancer l'analyse**.
3. Dans le navigateur : **Plug-ins → ANKS Labs → KINETAKT**, glisse-le sur une **piste MIDI**.
*Tu avais la version **ELEKTAKT** (0.7.0 ou avant) ? C'est le même projet, renommé. Supprime l'ancien dossier `ELEKTAKT.vst3` du dossier VST3 avant de copier `KINETAKT.vst3` : tes sets Live retrouvent le plugin, et tes projets `.elektakt` s'ouvrent toujours.*

4. Lance la lecture dans Live : KINETAKT suit le **tempo**, la **position** et les **boucles** de Live (bouton **SYNC LIVE**).

| Dans Live | Comment faire |
|:--|:--|
| **Sorties séparées** | Clic droit sur une piste de KINETAKT → **Sortie audio de la piste** → Sortie 2 à 9. Dans Live, crée une piste audio : **Audio From** = la piste de KINETAKT, puis **KINETAKT / Sortie 2…**. Idéal pour enregistrer des stems. |
| **Pistes MIDI → un synthé de Live** | **SORTIE DES PISTES MIDI = MIDI du plugin**, puis sur la piste du synthé : **MIDI From** = la piste de KINETAKT → **KINETAKT**. Live ramène tout ce MIDI sur **un seul canal** : un seul synthé par instance. |
| **Plusieurs synthés** | Choisis un port **loopMIDI** comme sortie : chaque piste MIDI garde son canal (même réglage que l'application). |
| **Jouer depuis Live** | Les notes reçues jouent la piste sélectionnée, ou les 16 pistes en **mode pads** (RÉGLAGES). Avec REC armé, elles s'enregistrent. |
| **Sauvegarde** | Patterns, locks, scènes et réglages sont gardés **dans le set Live**. Les samples restent là où ils sont (garde-les en place). Le kit démo, `samples` et `projets` sont dans **Documents\KINETAKT**. |

## 02b // LE SÉQUENCEUR — THE SEQUENCER

| Fonction | Comment faire |
|:--|:--|
| **Trig** | Clic sur un pas vide = trig. Clic sur un trig = le retirer (au relâchement). Glisser la souris peint plusieurs pas. |
| **Parameter lock** | **Clic droit** sur un ou plusieurs pas (ils s'entourent de blanc), puis tourne un potard : la valeur ne s'applique qu'à ces pas. Le potard devient rouge, le pas reçoit un point rouge. Clic droit ou **double-clic** sur le potard = retirer le lock. **Plus rapide** : maintiens le clic sur un pas et tourne la **molette** : le dernier potard touché est verrouillé sur ce pas. |
| **Trigless lock** | Même geste sur un pas **vide** : il change le son en cours (filtre, effets, pan, CC MIDI…) sans rejouer de note. Cadre rouge. |
| **Trig conditions** | Page **TRIG**, potard **CONDITION** : `1:2`… `8:8` (joue au passage A sur B), `FILL` / `!FILL` (bouton FILL ou touche F maintenue), `PRE` / `!PRE` (selon le dernier trig conditionnel de la piste), `NEI` / `!NEI` (selon la piste au-dessus), `1ST` / `!1ST` (premier passage). |
| **Probabilité** | Page TRIG, 0 à 100 % par pas. |
| **Micro-timing** | Page TRIG, −23 à +23 vingt-quatrièmes de pas (en avance / en retard). |
| **Retrig** | Page TRIG, 1 à 8 coups dans le pas. |
| **Parameter slide** | Page TRIG, potard **SLIDE** sur un pas sélectionné : ses valeurs verrouillées glissent en douceur jusqu'au prochain lock du même paramètre (volume, pan, enveloppe, effets, CC MIDI). Le pas affiche « SLIDE ». |
| **Scènes + crossfader** | En bas, onglet **SCÈNES** : 16 scènes par pattern. Clic sur une scène = édition : les potards de **toutes les pistes** écrivent dans la scène (potard vert). Clic droit = la mettre en **A** ou en **B**. Le **crossfader** passe en douceur de A à B, à la souris, à la molette ou avec un potard MIDI (clic droit = MIDI Learn). |
| **LFO** | Page **LFO** de chaque piste : FORME (sinus, triangle, rampes, carré, aléatoire), CYCLE (½ à 256 pas, calé sur la piste), PROFONDEUR (±), DESTINATION (enveloppe, volume, pan, paramètres d'effet, ou un CC pour les pistes MIDI) et MODE (LIBRE, TRIG = repart à chaque trig, UNE FOIS = enveloppe). Le potard SORTIE montre l'LFO en mouvement. |
| **GÉNÉRER ▾** | Au-dessus des pas, pour la piste sélectionnée : **euclidien** (3 à 11 coups répartis), **aléatoire** (25 / 50 / 75 %), **muter** (20 % des pas changent), **vélocités au hasard**, **accidents** (probabilités, cycles A:B, retrigs, micro-décalages semés au hasard), **locks au hasard** sur le dernier potard touché, **hauteurs au hasard** (gamme mineure), **décaler** d'un pas, **inverser**. Ça ne te plaît pas : **Ctrl+Z**. |
| **SONS** | Bouton **SONS** (ligne des patterns) : navigateur de WAV (kit démo, dossier `samples`, ou n'importe quel dossier de ton PC). Clic = écouter, double-clic ou **CHARGER** = mettre sur la piste sélectionnée. |
| **Rééchantillonner** | Dans SONS ou clic droit sur une piste : le pattern (1 à 8 cycles) est enregistré dans `samples` et chargé sur la piste. Comme sur une Digitakt, tu peux ensuite le découper et le rejouer. |
| **Découper (chop)** | Dans SONS, GÉNÉRER ou clic droit sur une piste : le sample est coupé en 8, 16 ou 32 tranches, une par pas (locks START / END), dans l'ordre ou au hasard. |
| **Copier / coller** | Pas sélectionnés : **COPIER LES PAS** puis sélectionne le pas de départ et **COLLER ICI** (ou Ctrl+C / Ctrl+V). **COPIER PAGE / COLLER PAGE** pour 16 pas d'un coup. Sans sélection, Ctrl+C / Ctrl+V copie la piste entière. |
| **Effets partagés** | Page **AMP** : **ENVOI RÉVERBE** et **ENVOI DELAY** de la piste (verrouillables par pas, comme sur une Digitakt). Page **FX COMMUNS** : la réverbe (déclin, taille, timbre, retour) et le delay calé sur le tempo (temps 1/32 à 1/2, réinjection, timbre, retour, ping-pong), partagés par toutes les pistes. Bien plus léger qu'une réverbe en insert sur chaque piste. |
| **Annuler** | **Ctrl+Z** annule, **Ctrl+Y** rétablit (64 niveaux) : pas, locks, réglages de piste, scènes et réglages de son (enveloppe, effets, niveau…). Le choix du sample n'est pas concerné. |
| **Longueur / vitesse par piste** | En haut : **LONGUEUR** (1 à 64 pas, molette ou glisser, clic droit = valeurs rapides) et **VITESSE** (2x, 1x, 1/2x, 1/4x, 1/8x). Des pistes de longueurs différentes créent des polyrythmies. |
| **Longueur maître** | `INF` : les pistes tournent librement et un pattern en attente démarre toutes les 16 pas. Une valeur (1–64) relance toutes les pistes à ce rythme. |
| **Swing** | 50 à 80 %, appliqué aux pas pairs. |
| **Patterns A–P** | 16 patterns. Pendant la lecture, le pattern choisi démarre à la fin du cycle. Clic droit : copier, coller, effacer. |
| **Song** | Bouton SONG : enchaîne des patterns (×1, ×2, ×4), puis « Jouer le song ». |
| **Macros M1–M8** | Touche un potard, puis clic droit sur une macro → Assigner. **MIDI Learn** pour la piloter avec un LPD8, un MPK mini… |
| **REC** | Arme REC, lance la lecture et joue sur ton clavier MIDI : les notes s'écrivent sur la piste sélectionnée. |
| **Niveau des pistes** | Chaque piste son affiche un vu-mètre et son niveau en dB : **molette** sur le niveau pour le régler. |
| **Double-clic** | Sur un potard : retire son lock ou sa valeur de scène, sinon remet la valeur par défaut. |
| **Mode pads** | **RÉGLAGES → Entrée MIDI : pads** : les notes 36 à 51 (les pads d'un MPK mini, d'un LPD8…) jouent directement les pistes 1 à 16, et s'enregistrent sur la bonne piste avec REC. |

![KINETAKT — réverbe et delay partagés (page FX COMMUNS)](docs/apercu-fx.png)

![KINETAKT — forme d'onde et tranches sur la page SRC](docs/apercu-onde.png)

**Pistes SON** : pages **SRC** (hauteur, début, fin, volume, pan, avec la forme d'onde), **AMP** (enveloppe ADSR et envois vers les effets partagés), **FX1–FX4** (31 effets Airwindows : filtres, saturations, compresseurs, delays, réverbes, lo-fi…), **LFO** et **FX COMMUNS**. Tout est verrouillable par pas (sauf les réglages du LFO).
Charge un son par **glisser-déposer** d'un WAV sur la piste, ou clic droit → *Charger un sample*.

**Pistes MIDI** : page **TRIG** (note, vélocité, longueur) et page **CC MIDI** (8 contrôleurs, numéro réglable à la molette sur le titre) et page **LFO** (vers un des 8 CC). Les notes, les CC verrouillés et l'horloge partent vers la sortie MIDI choisie.

## 03 // RELIER À ABLETON — CONNECT TO ABLETON

1. Installe **[loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html)** (gratuit) et crée un port avec le bouton **+**.
2. Dans KINETAKT, clique sur **SORTIE MIDI** et choisis le port loopMIDI. Passe une piste en **MIDI** (bouton SON | MIDI) et choisis son canal (clic droit sur la piste).
3. Dans Ableton : **Préférences → Link, Tempo & MIDI**, ligne d'entrée loopMIDI : **Piste = On** (notes) et **Sync = On** (horloge). Dans Live, clique sur **EXT** à côté du tempo pour suivre KINETAKT.
4. Sur une piste MIDI d'Ableton : entrée = loopMIDI, canal = celui de la piste KINETAKT, **Monitor = In**.

Le bouton **CLOCK OUT** coupe ou rétablit l'envoi de l'horloge. Les notes MIDI sont envoyées au moment où le son de KINETAKT sort des haut-parleurs, pour rester calées.

## 04 // RACCOURCIS — SHORTCUTS

`Espace` lecture / stop · `F` maintenu = FILL · `Échap` relâcher la sélection · `Suppr` effacer les locks sélectionnés · `←` `→` pages de pas · `↑` `↓` pistes · `F1`–`F8` pages de potards (TRIG, SRC, AMP, FX1–FX4, LFO ; FX COMMUNS par l'onglet) · `R` REC · `Ctrl+S` enregistrer · `Ctrl+O` ouvrir · `Ctrl+N` nouveau · `Ctrl+C` / `Ctrl+V` copier / coller (pas sélectionnés ou piste) · `Ctrl+Z` / `Ctrl+Y` annuler / rétablir · `Maj` + potard = réglage fin.

## 05 // FICHIERS — FILES

| Fichier | Contenu |
|:--|:--|
| `.kinetakt` | Projet complet : patterns, pistes, locks, song, kits. |
| `.kicklab` | Projet KICK : **FICHIER → Importer un projet KICK** convertit ses pas en trigs. |
| `.kickkit` | Kit de 16 sons et leurs réglages (compatible KICK). |
| `.wav` | **FICHIER → Exporter le pattern en WAV** : 1, 2, 4 ou 8 cycles du pattern, 48 kHz / 24 bits. Choisis 4 cycles si tu utilises des conditions (1:4, !1ST…). |
| `…-T03-….wav` | **FICHIER → Exporter les pistes séparées (stems)** : le mix plus un WAV par piste son, même durée, prêts à glisser dans Ableton ou dans ta banque de loops. |

L'historique des versions est dans [CHANGELOG.md](CHANGELOG.md). Les réglages des macros, du crossfader et des appareils MIDI sont gardés à côté de `KINETAKT.exe` (`KINETAKT-settings.txt`, `KINETAKT-devices.txt`).

## 06 // FEUILLE DE ROUTE — ROADMAP

- [x] Séquenceur 16 pistes × 64 pas, longueur et vitesse par piste
- [x] Parameter locks, trigless locks, trig conditions, probabilité, micro-timing, retrig
- [x] Pistes son (samples + 4 effets) et pistes MIDI, horloge MIDI
- [x] Macros M1–M8 avec MIDI Learn, import des projets KICK
- [x] Scènes avec crossfader A/B, parameter slide (0.2.0)
- [x] Copier / coller de pas et de pages, annuler / rétablir (0.2.0)
- [x] LFO par piste, mode pads pour l'entrée MIDI (0.3.0)
- [x] Générateur (euclidien, aléatoire, mutation, accidents), export des stems (0.4.0)
- [x] Navigateur de sons, rééchantillonnage, découpage en tranches (0.5.0)
- [x] Version VST3 pour Ableton : synchro Live, sorties séparées, état dans le set (0.6.0)
- [x] Réverbe et delay partagés avec envois verrouillables, timing à l'échantillon près, ergonomie (0.7.0)
- [x] Nouveau nom KINETAKT, première version stable (0.7.1)

## 07 // COMPILER — BUILD FROM SOURCE

Windows, Visual Studio 2022 + CMake (le VST3 télécharge JUCE automatiquement) :

```powershell
cmake -S . -B build -A x64 -DKINETAKT_PLUGIN=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Linux (tests du moteur et rendu de l'interface en PNG) : `cmake -S . -B build -DTRIG_UI_HARNESS=ON && cmake --build build && ./build/TRIG-Test`.
Compilation croisée MinGW : `-DCMAKE_TOOLCHAIN_FILE=tools/mingw.cmake`.

## 08 // LICENCE & SOURCES

KINETAKT est libre et gratuit, sous **AGPL-3.0-or-later**. Il est construit sur le moteur **KICK** d'ANKS Labs, basé sur le code ouvert **AudIO / KICK de Music Tribe** ([0700-AAM](https://github.com/MusicTribeOpenSource/0700-AAM), AGPL-3.0), le plugin utilise **[JUCE](https://juce.com)** (AGPLv3), avec les effets **[Airwindows](https://www.airwindows.com/)** de Chris Johnson (MIT) et **AudioFile** d'Adam Stark (MIT). Détails : [NOTICES.txt](NOTICES.txt).

L'ergonomie s'inspire des machines **Elektron**. Elektron, Ableton, Behringer, Music Tribe et KICK sont des marques de leurs propriétaires ; ANKS Labs n'est ni affilié à ces fabricants ni approuvé par eux.

---

<div align="center">

**ANKS LABS // EXPERIMENTAL AUDIO DEVELOPMENT**

</div>
