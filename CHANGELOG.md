# ELEKTAKT — versions

## 0.6.0
- **Plugin VST3 pour Ableton Live** (éditeur ANKS Labs), avec la même interface que l'application :
  - synchronisé sur Live : lecture, tempo, position et boucles ;
  - 44,1 / 48 / 96 kHz (conversion interne) ;
  - **8 sorties stéréo séparées** en plus de la sortie principale, au choix par piste ;
  - pistes MIDI vers Live (un canal) ou vers un port loopMIDI (multicanal) ;
  - notes reçues de Live : piste sélectionnée ou mode pads, enregistrement REC ;
  - tout le projet est sauvegardé dans le set Live ; kit démo et samples dans Documents\ELEKTAKT.
- Vérifié automatiquement à chaque version : hôte de test (lecture synchronisée, arrêt, sauvegarde / rechargement, éditeur) et **pluginval**.
- Moteur : l'interface n'utilise plus de grosses copies sur la pile (nécessaire dans un plugin).

## 0.5.0
- **Navigateur SONS** : kit démo, dossier `samples` ou un dossier de ton choix (mémorisé), écoute au clic, chargement au double-clic.
- **Rééchantillonnage** : le pattern (1 à 8 cycles) devient un WAV dans `samples` et se charge sur la piste choisie.
- **Découpage (chop)** : 8, 16 ou 32 tranches d'un sample posées sur les pas avec des locks START / END, dans l'ordre ou au hasard.

## 0.4.0
- **GÉNÉRER ▾** par piste : euclidien (3–11 coups), aléatoire 25/50/75 %, mutation de 20 % des pas, vélocités au hasard, « accidents » (probabilités, cycles, retrigs, micro-décalages), locks au hasard sur le dernier potard touché, hauteurs au hasard en gamme mineure, décalage d'un pas, inversion. Tout s'annule avec Ctrl+Z.
- **Export des stems** : le mix et un WAV par piste son, de même durée.
- Les exports WAV durent 1, 2, 4 ou 8 cycles, pour entendre les conditions (1:4, !1ST…).

## 0.3.0
- **LFO par piste** (page LFO) : 6 formes, cycle de ½ à 256 pas calé sur la vitesse de la piste, profondeur ±, destination (enveloppe, volume, pan, paramètres d'effet, ou CC MIDI), modes LIBRE / TRIG / UNE FOIS, sortie visible en direct.
- **Mode pads** pour l'entrée MIDI : notes 36–51 → pistes 1–16 (MPK mini, LPD8…), avec enregistrement REC sur la bonne piste.
- Format de projet v3 : les projets 0.1.0 et 0.2.0 s'ouvrent toujours.

## 0.2.0
- **Scènes** : 16 par pattern, édition en direct sur toutes les pistes, scènes A / B et **crossfader** (souris, molette, MIDI Learn).
- **Parameter slide** : les valeurs verrouillées d'un pas glissent jusqu'au lock suivant (volume, pan, enveloppe, effets, CC MIDI).
- **Copier / coller** des pas sélectionnés (avec leurs locks) et des pages de 16 pas.
- **Annuler / rétablir** (Ctrl+Z / Ctrl+Y, 64 niveaux) pour les pas, locks, pistes et scènes.
- Les pas affichent le **nombre de locks** (8 maximum).
- Corrigé : la tête de lecture restait sur le pas 1, le bouton FILL et le clignotement des pistes ne s'allumaient pas, et le bouton CLOCK OUT ne pouvait plus être réactivé après l'avoir coupé.
- Format de projet : les fichiers 0.1.0 s'ouvrent toujours.

## 0.1.0
- Première version : séquenceur 16 pistes × 64 pas à la Elektron (parameter locks, trigless locks, trig conditions, probabilité, micro-timing, retrig, longueur et vitesse par piste), pistes son et pistes MIDI, sortie MIDI et horloge vers Ableton, macros, patterns, song, import KICK, export WAV.
