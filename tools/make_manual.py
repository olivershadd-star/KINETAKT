#!/usr/bin/env python3
"""Builds docs/MODE-EMPLOI-KINETAKT.pdf from the interface screenshots.

Screenshots come from the Cairo harness (TRIG-UI-Render, Linux build with
-DTRIG_UI_HARNESS=ON, run with the demo kit next to it):
    python3 tools/make_manual.py <screenshots folder> <version>
Needs Pillow and reportlab, and the DejaVu fonts.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image as RLImage, KeepTogether, PageBreak, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

SHOTS = sys.argv[1] if len(sys.argv) > 1 else "."
VERSION = sys.argv[2] if len(sys.argv) > 2 else "0.8.0"
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "MODE-EMPLOI-KINETAKT.pdf")
WORK = os.path.join("/tmp", "kinetakt-manual")
os.makedirs(WORK, exist_ok=True)

FONT_DIR = "/usr/share/fonts/truetype/dejavu/"
pdfmetrics.registerFont(TTFont("Sans", FONT_DIR + "DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("Sans-Bold", FONT_DIR + "DejaVuSans-Bold.ttf"))
pdfmetrics.registerFontFamily("Sans", normal="Sans", bold="Sans-Bold", italic="Sans", boldItalic="Sans-Bold")

RED, BLUE, INK, MUTED = "#EF343E", "#19B9FF", "#10151E", "#5A6270"
title = ParagraphStyle("t", fontName="Sans-Bold", fontSize=26, leading=32, textColor=colors.HexColor(INK))
h1 = ParagraphStyle("h1", fontName="Sans-Bold", fontSize=17, leading=22, spaceBefore=4, spaceAfter=8, textColor=colors.HexColor(INK))
h2 = ParagraphStyle("h2", fontName="Sans-Bold", fontSize=12.5, leading=16, spaceBefore=8, spaceAfter=4, textColor=colors.HexColor("#1C6FA8"))
body = ParagraphStyle("b", fontName="Sans", fontSize=10.5, leading=14.5, spaceAfter=5)
small = ParagraphStyle("s", fontName="Sans", fontSize=9.5, leading=13, textColor=colors.HexColor(MUTED))
cell = ParagraphStyle("c", fontName="Sans", fontSize=9.5, leading=12.5)
cellb = ParagraphStyle("cb", parent=cell, fontName="Sans-Bold")
center = ParagraphStyle("ce", parent=small, alignment=TA_CENTER)

marker_font = ImageFont.truetype(FONT_DIR + "DejaVuSans-Bold.ttf", 26)


def annotate(name, boxes):
    """Numbered red frames on a 1600×900 screenshot. boxes = [(x, y, w, h, number)]"""
    image = Image.open(os.path.join(SHOTS, name)).convert("RGB")
    draw = ImageDraw.Draw(image)
    for x, y, w, h, number in boxes:
        draw.rounded_rectangle([x - 3, y - 3, x + w + 3, y + h + 3], radius=10, outline=RED, width=4)
        cx, cy = x + 2, y + 2
        draw.ellipse([cx - 19, cy - 19, cx + 19, cy + 19], fill=RED, outline="white", width=3)
        text = str(number)
        bounds = draw.textbbox((0, 0), text, font=marker_font)
        draw.text((cx - (bounds[2] - bounds[0]) / 2 - bounds[0], cy - (bounds[3] - bounds[1]) / 2 - bounds[1]), text, fill="white",
                  font=marker_font)
    path = os.path.join(WORK, name)
    image.save(path)
    return path


def picture(name, boxes=(), caption=None, size=176):
    path = annotate(name, boxes)
    width = size * mm
    parts = [RLImage(path, width=width, height=width * 900 / 1600)]
    if caption:
        parts.append(Paragraph(caption, center))
    parts.append(Spacer(1, 6))
    return KeepTogether(parts)


def legend(rows):
    data = [[Paragraph(f"<font color='{RED}'><b>{n}</b></font>", cellb), Paragraph(t, cell)] for n, t in rows]
    table = Table(data, colWidths=[9 * mm, 171 * mm])
    table.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("BOTTOMPADDING", (0, 0), (-1, -1), 2), ("TOPPADDING", (0, 0), (-1, -1), 2)]))
    return table


def grid(rows, widths=(45, 135)):
    data = [[Paragraph(a, cellb), Paragraph(b, cell)] for a, b in rows]
    table = Table(data, colWidths=[w * mm for w in widths])
    table.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("GRID", (0, 0), (-1, -1), .4, colors.HexColor("#C9CED6")),
                               ("BACKGROUND", (0, 0), (0, -1), colors.HexColor("#EEF2F6")), ("TOPPADDING", (0, 0), (-1, -1), 3),
                               ("BOTTOMPADDING", (0, 0), (-1, -1), 3)]))
    return table


def P(text):
    return Paragraph(text, body)


def steps(items):
    return [Paragraph(f"<b>{i}.</b> {t}", body) for i, t in enumerate(items, 1)]


story = []
# ---------------------------------------------------------------- cover
story += [Spacer(1, 30 * mm), Paragraph("KINETAKT", title), Spacer(1, 2 * mm),
          Paragraph("Mode d'emploi · séquenceur à la Elektron, pistes son et MIDI", h1),
          Paragraph(f"Version {VERSION} · ANKS Labs · logiciel libre et gratuit (AGPL-3.0)", small), Spacer(1, 8 * mm),
          picture("ui-1-demo.png"), Spacer(1, 6 * mm),
          P("Ce guide se lit dans l'ordre : chaque partie ajoute une idée. En 10 minutes, tu sais tout faire. "
            "Les numéros rouges sur les images renvoient aux explications juste en dessous."),
          P("<b>La règle d'or :</b> si un potard ne réagit pas, regarde la barre tout en bas de la fenêtre. "
            "Elle dit toujours ce qui se passe et quoi faire, et elle affiche une aide quand tu passes la souris sur un élément."),
          PageBreak()]

# ---------------------------------------------------------------- 1
story += [Paragraph("1. L'écran en un coup d'œil", h1),
          picture("ui-1-demo.png", [(196, 12, 234, 44, 1), (448, 12, 340, 44, 2), (110, 72, 875, 38, 3), (16, 124, 344, 736, 4),
                                    (706, 128, 382, 40, 5), (376, 232, 1208, 96, 6), (376, 392, 892, 36, 7), (376, 440, 1208, 196, 8),
                                    (376, 660, 1208, 196, 9), (16, 872, 1568, 26, 10)]),
          legend([(1, "<b>PLAY / STOP / REC</b> : lecture (barre <b>Espace</b>), arrêt, enregistrement de ce que tu joues."),
                  (2, "<b>TEMPO, SWING, LONG. MAÎTRE</b> : clic = liste de valeurs, ou glisser la souris / molette."),
                  (3, "<b>Patterns A à P</b> : 16 boucles différentes. Clic = changer (en fin de boucle si ça joue). SONG = enchaîner les patterns."),
                  (4, "<b>Les 16 pistes</b> : clic = choisir la piste. <b>M</b> = muet, <b>S</b> = solo, le chiffre en dB = volume (molette dessus), "
                      "la barre sous le nom = vu-mètre."),
                  (5, "<b>Réglages de la piste</b> : SON ou MIDI, LONGUEUR (nombre de pas), VITESSE, sample ou canal MIDI."),
                  (6, "<b>Les pas</b> (step sequencer) : ce qui est allumé joue."),
                  (7, "<b>Les pages de potards</b> : TRIG, SRC, AMP, FX1–FX4, LFO, FX COMMUNS."),
                  (8, "<b>Les 8 potards</b> de la page choisie."),
                  (9, "<b>MACROS</b> (8 potards raccourcis) ou <b>SCÈNES + CROSSFADER</b> : deux onglets."),
                  (10, "<b>Barre d'état</b> : messages et aide.")]),
          PageBreak()]

# ---------------------------------------------------------------- 2
story += [Paragraph("2. Premier son en 2 minutes", h1)]
story += steps(["Lance <b>KINETAKT.exe</b> (ou ouvre le plugin dans Ableton). Un pattern de démonstration est chargé.",
                "Appuie sur <b>Espace</b> : ça joue. Dans Ableton, lance la lecture de Live.",
                "Clique la piste <b>1 · Kick</b> à gauche, puis clique des pas vides : ils s'allument en bleu et jouent.",
                "Clique sur un pas allumé : il devient <b>choisi</b> (contour orange). Re-clique dessus : il s'éteint.",
                "Ouvre l'onglet <b>SCÈNES + CROSSFADER</b> en bas et glisse le crossfader vers la droite : la basse se ferme, "
                "la caisse claire part en réverbe, le clap en delay. Reviens à gauche.",
                "Onglet <b>MACROS</b> : tourne M6 (LOWPASS de la basse) ou M5 (réverbe de la caisse claire).",
                "Pour remplacer un son : glisse un fichier WAV sur une piste, ou clique <b>SONS</b> pour parcourir tes dossiers."])
story += [Spacer(1, 4), Paragraph("Tourner un potard", h2),
          P("Clique sur le potard et <b>glisse vers le haut ou vers la droite</b> pour monter, vers le bas ou la gauche pour descendre. "
            "La <b>molette</b> marche aussi. Avec <b>Maj</b> enfoncé, le réglage est plus fin. <b>Double-clic</b> = valeur par défaut. "
            "Certains potards (CONDITION, FORME, DESTINATION du LFO, TEMPS du delay) ouvrent une <b>liste</b> quand tu cliques sans glisser."),
          PageBreak()]

# ---------------------------------------------------------------- 3
story += [Paragraph("3. Le séquenceur : pas, longueur, vitesse", h1),
          picture("ui-16-trig-chosen.png", [(522, 232, 74, 96, 1), (376, 392, 92, 36, 2), (376, 440, 1208, 196, 3), (472, 180, 286, 30, 4),
                                             (866, 128, 214, 40, 5)], size=158),
          legend([(1, "<b>Pas choisi</b> (contour orange) : c'est lui que règle la page TRIG. Clic sur un pas vide = il joue et il est choisi. "
                      "Clic sur un autre pas allumé = il devient le pas choisi. Re-clic sur le pas choisi = il s'éteint."),
                  (2, "<b>Page TRIG</b> (F1) : les réglages du pas choisi."),
                  (3, "Les 8 réglages du pas (tableau ci-dessous). Si aucun pas n'est choisi, NOTE, VÉLOCITÉ et LONGUEUR règlent les futurs pas."),
                  (4, "<b>Pages de pas</b> : une piste peut faire jusqu'à 64 pas, affichés par pages de 16. Si la piste est plus courte, "
                      "clic sur <b>+ 17–32</b> = la rallonger (pas vides ou recopiés)."),
                  (5, "<b>LONGUEUR</b> de la piste (clic = liste) et <b>VITESSE</b> (2x, 1/2x…). Chaque piste a sa propre longueur : "
                      "une basse de 12 pas contre une batterie de 16 pas, c'est un polyrythme.")]),
          grid([("NOTE", "Pistes MIDI : la note jouée. (Pistes son : la hauteur se règle sur la page SRC.)"),
                ("VÉLOCITÉ", "La force de la frappe (1 à 127)."),
                ("LONGUEUR", "Combien de pas la note dure."),
                ("PROBABILITÉ", "Chance que le pas joue à chaque passage. 100 % = toujours, 50 % = une fois sur deux au hasard."),
                ("CONDITION", "Quand le pas joue (clic = liste) : <b>1:4</b> = au 1er passage sur 4, <b>FILL</b> = seulement quand FILL est "
                              "enfoncé, <b>!FILL</b> = jamais pendant FILL, <b>1ST</b> = seulement au premier passage, "
                              "<b>PRE</b> = si le pas précédent de cette piste a joué, <b>NEI</b> = si la piste du dessus a joué."),
                ("MICRO", "Décale le pas un peu avant ou après (en 24e de pas) : pour le groove."),
                ("RETRIG", "Répète le son 2 à 8 fois dans le pas : roulements, flams."),
                ("SLIDE", "Glisse en douceur vers les valeurs du pas suivant (hauteur, filtre…), comme une 303.")]),
          Spacer(1, 6), Paragraph("Autres outils", h2),
          P("<b>FILL</b> (en haut, ou touche <b>F</b> maintenue) : joue les pas marqués FILL. <b>SWING</b> : retarde un pas sur deux (66 % = ternaire). "
            "<b>LONG. MAÎTRE</b> : longueur après laquelle toutes les pistes repartent ensemble (INF = chaque piste boucle seule). "
            "<b>GÉNÉRER</b> : euclidien, aléatoire, humaniser, décaler, inverser, découper un sample… (<b>Ctrl+Z</b> si ça ne plaît pas). "
            "<b>COPIER PAGE / COLLER PAGE</b> : dupliquer 16 pas."),
          PageBreak()]

# ---------------------------------------------------------------- 4
story += [Paragraph("4. Varier un son pas par pas : les parameter locks", h1),
          P("Un <b>lock</b> (verrou) change un réglage de son <b>pour un seul pas</b> : une caisse claire plus aiguë sur le pas 8, "
            "un filtre fermé sur le pas 12, une réverbe sur le dernier coup… C'est le cœur des machines Elektron."),
          picture("ui-2-locks.png", [(596, 232, 74, 96, 1), (376, 346, 1208, 30, 2), (376, 440, 146, 196, 3)]),
          legend([(1, "<b>Clic droit</b> sur un ou plusieurs pas : ils sont <b>sélectionnés</b> (contour blanc)."),
                  (2, "La ligne rouge le confirme : maintenant, les potards posent des locks au lieu de changer le son de toute la piste. "
                      "COPIER LES PAS / COLLER ICI / EFFACER LOCKS / RELÂCHER (ou <b>Échap</b>)."),
                  (3, "Tourne un potard (sur SRC, AMP, FX…) : il devient rouge, le pas porte un rond rouge avec le nombre de locks. "
                      "Double-clic ou clic droit sur le potard = retirer le lock.")]),
          Paragraph("Le geste rapide", h2),
          P("Touche un potard (pour que KINETAKT sache lequel), puis <b>maintiens le clic sur un pas et tourne la molette</b> : "
            "le réglage est verrouillé sur ce pas. Tu peux aussi poser un lock sur un pas <b>vide</b> : il ne joue pas, "
            "mais change le son à partir de là (trigless lock)."),
          PageBreak()]

# ---------------------------------------------------------------- 5
story += [Paragraph("5. Le son d'une piste", h1),
          Paragraph("SRC : le sample", h2),
          picture("ui-15-waveform.png", [(376, 440, 750, 196, 1), (1130, 440, 449, 196, 2)], size=132),
          legend([(1, "<b>PITCH</b> (hauteur en demi-tons), <b>START</b> / <b>END</b> (début et fin du sample), <b>LEVEL</b> (volume), <b>PAN</b>."),
                  (2, "La forme d'onde : trait vert = START, trait rouge = END, traits orange = tranches d'un sample découpé (GÉNÉRER → Découper).")]),
          Paragraph("AMP : l'enveloppe et les envois", h2),
          P("<b>ATTACK</b> = montée du son. <b>DECAY</b> = durée : « entier » joue tout le sample, une valeur en ms le raccourcit "
            "(idéal pour un charleston fermé ou une grosse caisse sèche). <b>SUSTAIN</b> sous 100 % = niveau tenu après le decay. "
            "<b>RELEASE</b> = extinction à la fin de la note. <b>ENVOI RÉVERBE</b> et <b>ENVOI DELAY</b> = quantité de son envoyée "
            "aux effets communs (partie 6)."),
          Paragraph("FX1 à FX4 : 4 effets par piste", h2),
          picture("ui-17-fx-empty.png", [(376, 440, 1208, 196, 1), (1284, 392, 188, 36, 2), (1480, 392, 104, 36, 3)], size=132),
          legend([(1, "Un emplacement vide : clique dedans pour choisir parmi 31 effets (filtres, saturations, compresseurs, delays, lo-fi…)."),
                  (2, "Changer d'effet à tout moment."),
                  (3, "<b>ACTIF / BYPASS</b> : couper l'effet sans perdre ses réglages.")]),
          P("Un effet a entre 1 et 6 réglages : les cases marquées « — » sont simplement libres. L'effet « Reverb » en insert est lourd "
            "pour le processeur : préfère la réverbe commune."),
          PageBreak()]

# ---------------------------------------------------------------- 6
story += [Paragraph("6. Réverbe et delay communs", h1),
          P("Une seule réverbe et un seul delay pour tout le kit, comme sur une Digitakt. Chaque piste y envoie plus ou moins de son "
            "avec ses potards <b>ENVOI RÉVERBE</b> et <b>ENVOI DELAY</b> (page AMP). Ces envois se verrouillent par pas : "
            "un seul coup de caisse claire dans la réverbe, par exemple."),
          picture("ui-14-shared.png", [(376, 440, 600, 196, 1), (980, 440, 604, 196, 2), (1480, 392, 104, 36, 3)]),
          legend([(1, "<b>Réverbe</b> : DÉCLIN (longueur), TAILLE, TIMBRE (sombre → brillant), RETOUR (volume de la réverbe dans le mix)."),
                  (2, "<b>Delay</b> : TEMPS (clic = liste : 1/16, 1/8 pointée… toujours calé sur le tempo), RÉINJ. (nombre de répétitions), "
                      "TIMBRE, RETOUR."),
                  (3, "<b>PING-PONG</b> : les échos alternent gauche / droite.")]),
          PageBreak()]

# ---------------------------------------------------------------- 7
story += [Paragraph("7. Faire bouger le son : LFO, scènes, macros", h1),
          Paragraph("LFO : un réglage qui oscille tout seul", h2),
          picture("ui-9-lfo.png", [(376, 440, 297, 196, 1), (678, 440, 145, 196, 2), (829, 440, 145, 196, 3), (980, 440, 145, 196, 4),
                                   (1131, 440, 145, 196, 5)], size=124),
          legend([(1, "<b>FORME</b> (sinus, triangle, rampes, carré, aléatoire) et <b>CYCLE</b> (durée d'une oscillation en pas)."),
                  (2, "<b>PROFONDEUR</b> : de combien le réglage bouge (+ ou −). À 0, rien ne bouge."),
                  (3, "<b>DESTINATION</b> : clic = liste de ce que le LFO fait bouger (PAN, filtre, hauteur, envois…)."),
                  (4, "<b>MODE</b> : LIBRE (tourne en continu), TRIG (repart à chaque pas joué), UNE FOIS (comme une enveloppe)."),
                  (5, "<b>SORTIE</b> : la valeur du LFO en direct.")]),
          Paragraph("Scènes et crossfader", h2),
          picture("ui-8-scenes.png", [(552, 660, 250, 28, 1), (376, 702, 1200, 42, 2), (466, 764, 1028, 40, 3)], size=124),
          legend([(1, "Onglet <b>SCÈNES + CROSSFADER</b>."),
                  (2, "16 scènes. Une scène est une photo de réglages. Clic = <b>éditer</b> la scène (en vert) : tous les potards de toutes "
                      "les pistes écrivent alors dans la scène, sans toucher au son de base. Re-clic = fini. "
                      "Clic droit = la mettre en A ou en B, copier, effacer."),
                  (3, "Le <b>crossfader</b> passe en douceur de la scène A (gauche) à la scène B (droite). Dans la démo, B est une version dub. "
                      "Clic droit = l'assigner à un potard de ton contrôleur MIDI (MIDI Learn).")]),
          Paragraph("Macros", h2),
          P("Onglet <b>MACROS</b> : 8 grands potards raccourcis, chacun relié à un réglage de n'importe quelle piste. "
            "Pour en changer : touche le potard voulu, puis <b>clic droit sur la macro → Assigner</b>. "
            "MIDI Learn : clic droit sur la macro, puis tourne un potard de ton LPD8 / MPK."),
          PageBreak()]

# ---------------------------------------------------------------- 8
story += [Paragraph("8. Patterns, morceau, sons", h1),
          grid([("Patterns A–P", "Clic = jouer ce pattern (il démarre à la fin de la boucle en cours). Clic droit = copier / coller / effacer."),
                ("SONG", "Enchaîne des patterns (A ×4, B ×2…) pour construire un morceau."),
                ("KIT", "Chaque pattern utilise un kit de 16 sons. Plusieurs patterns peuvent partager le même kit."),
                ("SONS", "Navigateur de sons : clic = écouter, double-clic = charger sur la piste. "
                         "<b>RÉÉCHANTILLONNER</b> enregistre le pattern dans un WAV sur une piste ; <b>DÉCOUPER</b> le joue en tranches."),
                ("FICHIER", "Nouveau, ouvrir, enregistrer (.kinetakt), importer un projet KICK, kits, "
                            "<b>exporter le pattern en WAV</b> et <b>exporter les pistes séparées</b> (stems pour Ableton)."),
                ("Ctrl+Z / Ctrl+Y", "Annuler / rétablir (64 fois) : pas, locks, scènes, réglages de son.")]),
          Spacer(1, 6), picture("ui-11-browser.png", caption="Le navigateur SONS"),
          PageBreak()]

# ---------------------------------------------------------------- 9
story += [Paragraph("9. Les pistes MIDI : piloter un synthé", h1),
          picture("ui-4-midi.png", [(706, 128, 150, 40, 1), (1088, 128, 96, 40, 2), (1016, 12, 300, 44, 3), (474, 392, 150, 36, 4)]),
          legend([(1, "<b>SON | MIDI</b> : une piste MIDI joue des notes au lieu d'un sample (pas en jaune)."),
                  (2, "<b>CANAL MIDI</b> (1 à 16) du synthé visé."),
                  (3, "<b>SORTIE MIDI</b> : où partent les notes. Pour Ableton en application autonome, installe "
                      "<b>loopMIDI</b> (gratuit), crée un port, choisis-le ici, puis active ce port en entrée « Piste » dans Live."),
                  (4, "<b>CC MIDI</b> : 8 potards qui envoient des Control Change (filtre, résonance…). Molette sur le titre = numéro de CC. "
                      "Ils se verrouillent par pas comme les potards de son.")]),
          P("<b>CLOCK OUT</b> envoie l'horloge MIDI pour synchroniser une machine ou Ableton (Sync sur le port loopMIDI). "
            "Sur la page TRIG d'une piste MIDI, <b>NOTE</b> règle la note du pas choisi."),
          PageBreak()]

# ---------------------------------------------------------------- 10
story += [Paragraph("10. Dans Ableton (plugin VST3)", h1)]
story += steps(["Copie le dossier <b>KINETAKT.vst3</b> du ZIP dans <b>C:\\Program Files\\Common Files\\VST3</b>. "
                "Si tu avais l'ancienne version ELEKTAKT, supprime d'abord <b>ELEKTAKT.vst3</b>.",
                "Dans Live : Préférences → Plug-ins → Dossier système VST3 = On, puis <b>Relancer l'analyse</b>.",
                "Navigateur : Plug-ins → <b>ANKS Labs → KINETAKT</b>, glisse-le sur une piste MIDI.",
                "Lance la lecture de Live : KINETAKT suit le tempo et la position (bouton SYNC LIVE)."])
story += [grid([("Sorties séparées", "Clic droit sur une piste de KINETAKT → Sortie audio de la piste → Sortie 2 à 9. Dans Live, crée une "
                                     "piste audio : Audio From = la piste de KINETAKT → KINETAKT / Sortie 2… Idéal pour enregistrer des stems."),
                ("Pistes MIDI vers un synthé de Live", "SORTIE DES PISTES MIDI = MIDI du plugin, puis sur la piste du synthé : MIDI From = la piste de KINETAKT."),
                ("Jouer depuis un clavier", "Les notes reçues jouent la piste choisie (ou les 16 pistes en mode pads, dans RÉGLAGES). Avec REC armé, elles s'enregistrent."),
                ("Sauvegarde", "Tout est enregistré dans le set Live.")]),
          Spacer(1, 10), Paragraph("Raccourcis clavier", h2),
          grid([("Espace", "Lecture / stop (application autonome)"), ("F (maintenu)", "FILL"), ("Échap", "Relâcher la sélection, finir l'édition de scène"),
                ("Suppr", "Effacer les locks des pas sélectionnés"), ("← →", "Pages de pas"), ("↑ ↓", "Piste précédente / suivante"),
                ("F1 – F8", "Pages de potards"), ("R", "REC"), ("Ctrl+S / Ctrl+O / Ctrl+N", "Enregistrer / ouvrir / nouveau"),
                ("Ctrl+C / Ctrl+V", "Copier / coller les pas sélectionnés ou la piste"), ("Ctrl+Z / Ctrl+Y", "Annuler / rétablir"),
                ("Maj + potard", "Réglage fin")]),
          PageBreak()]

# ---------------------------------------------------------------- lexique
story += [Paragraph("Petit lexique", h1),
          grid([("Pas (step)", "Une case du séquenceur : 16 pas = une mesure en doubles croches."),
                ("Trig", "Un pas allumé : il déclenche le son ou la note."),
                ("Parameter lock", "Un réglage de son valable pour un seul pas."),
                ("Trigless lock", "Un lock sur un pas vide : change le son sans rien jouer."),
                ("Condition", "Règle qui décide si un pas joue (1:4, FILL, 1ST…)."),
                ("Probabilité", "Chance qu'un pas joue."),
                ("Micro-timing", "Petit décalage d'un pas pour le groove."),
                ("Retrig", "Répétition rapide du son dans un pas."),
                ("Slide", "Glissement vers les valeurs du pas suivant."),
                ("Polyrythme", "Pistes de longueurs différentes qui se décalent les unes par rapport aux autres."),
                ("Scène", "Photo de réglages ; le crossfader passe d'une scène à l'autre."),
                ("Macro", "Grand potard raccourci vers un réglage de n'importe quelle piste."),
                ("LFO", "Oscillateur lent qui fait bouger un réglage tout seul."),
                ("Envoi (send)", "Quantité de son envoyée vers la réverbe ou le delay communs."),
                ("Resample", "Enregistrer ce qui joue dans un nouveau sample."),
                ("Chop", "Découper un sample en tranches jouées par les pas."),
                ("Stems", "Un fichier WAV par piste, pour mixer ailleurs.")]),
          Spacer(1, 10),
          Paragraph("KINETAKT s'inspire de la façon de travailler des machines Elektron. Elektron, Digitakt et Ableton sont des marques de "
                    "leurs propriétaires ; ANKS Labs n'est ni affilié à ces fabricants ni approuvé par eux. "
                    "Effets : Airwindows (Chris Johnson, licence MIT).", small)]


def page_decor(canvas, doc):
    canvas.saveState()
    canvas.setFont("Sans", 8)
    canvas.setFillColor(colors.HexColor(MUTED))
    if doc.page > 1:
        canvas.drawString(15 * mm, 10 * mm, f"KINETAKT {VERSION} · mode d'emploi")
        canvas.drawRightString(195 * mm, 10 * mm, str(doc.page))
    canvas.restoreState()


doc = SimpleDocTemplate(OUT, pagesize=A4, leftMargin=15 * mm, rightMargin=15 * mm, topMargin=14 * mm, bottomMargin=16 * mm,
                        title="KINETAKT — mode d'emploi", author="ANKS Labs", subject=f"KINETAKT {VERSION}")
doc.build(story, onFirstPage=page_decor, onLaterPages=page_decor)
print("wrote", OUT)
