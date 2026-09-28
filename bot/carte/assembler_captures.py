# -*- coding: utf-8 -*-
# Assemble les captures aériennes prises en jeu par SRP_MapCapture (mod PAG) en tuiles pour la carte de situation.
#   1. recadre chaque capture (c<col>_r<rangée>.bmp) sur le centre : le carré de « pas » mètres de sa case ;
#   2. la ramène à 0,25 m par pixel et la range dans <dossier>/cases/ en WebP (la capture .bmp est supprimée avec --purger) ;
#   3. fabrique la pyramide de tuiles (zoom 8 = 0,25 m par pixel, monde de 16 384 m calé en bas à gauche).
# Usage : assembler_captures.py <dossier des captures> <dossier de sortie des tuiles> [--purger] [--echelle 1.0] [--seulement-cases]
# Dépendances : pip install pillow
import os, re, sys
import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

Image.MAX_IMAGE_PIXELS = None
source, sortie = sys.argv[1], sys.argv[2]
purger = '--purger' in sys.argv
seulement_cases = '--seulement-cases' in sys.argv
echelle = 1.0
if '--echelle' in sys.argv:
    echelle = float(sys.argv[sys.argv.index('--echelle') + 1])

PX_PAR_M, TUILE, MONDE, ZMAX = 4, 256, 16384, 8        # 0,25 m par pixel au zoom 8
FOND = (12, 20, 36)

infos = {}
for ligne in open(os.path.join(source, 'capture.txt'), encoding='utf-8', errors='replace'):
    if '=' in ligne:
        cle, valeur = ligne.strip().split('=', 1)
        infos[cle] = valeur
x0, z0, pas = float(infos['x0']), float(infos['z0']), float(infos['pas'])
colonnes, rangees = int(infos['colonnes']), int(infos['rangees'])
vue = float(infos['hauteur_vue_m']) * echelle
cote = int(round(pas * PX_PAR_M))

# Le voile atmosphérique : vue de 1 500 m, l'image est bleutée et plate. Mêmes réglages FIXES pour toutes les cases (pas
# d'automatisme par image, sinon les cases ne se raccordent plus) : point noir et point blanc par couleur, puis un peu
# de contraste et de saturation.
# Les niveaux dépendent de la hauteur de prise de vue (plus on est haut, plus le voile est épais) ; mesurés sur les essais.
if float(infos.get('altitude', 1500)) > 1000:
    NOIR, BLANC, SATURATION, CONTRASTE = (44, 66, 96), (210, 200, 190), 1.08, 1.06
else:
    NOIR, BLANC, SATURATION, CONTRASTE = (16, 24, 30), (228, 218, 204), 1.05, 1.03
TABLE = []
for canal in range(3):
    n, b = NOIR[canal], BLANC[canal]
    TABLE += [max(0, min(255, int(round((v - n) * 255.0 / (b - n))))) for v in range(256)]


def debrumer(image):
    image = image.point(TABLE)
    image = ImageEnhance.Color(image).enhance(SATURATION)
    return ImageEnhance.Contrast(image).enhance(CONTRASTE)


# La mer : repeinte comme sur le fond satellite (fabriquer_tuiles.py), d'après la carte en relief du jeu, pour que la
# photographie aérienne et le fond se raccordent sans marque. Sans relief_everon.png à côté du script : mer laissée telle quelle.
RELIEF = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'relief_everon.png')
TAILLE_ILE = 12800
masque = large = None
if os.path.exists(RELIEF):
    relief = np.asarray(Image.open(RELIEF).convert('RGB')).astype(np.int16)
    mer = ((relief[:, :, 2] - relief[:, :, 0]) > 40).astype(np.uint8) * 255
    masque = Image.fromarray(mer).filter(ImageFilter.GaussianBlur(1.2))
    large = Image.fromarray(mer).filter(ImageFilter.GaussianBlur(40))
COTE_EAU = np.array([38, 78, 92], np.float32)
LARGE_EAU = np.array([12, 20, 36], np.float32)


def peindre_mer(case, colonne, rangee):
    if masque is None:
        return case
    e = masque.size[0] / TAILLE_ILE
    gauche, droite = (x0 + colonne * pas) * e, (x0 + (colonne + 1) * pas) * e
    haut, bas = (TAILLE_ILE - (z0 + (rangee + 1) * pas)) * e, (TAILLE_ILE - (z0 + rangee * pas)) * e
    m = np.asarray(masque.resize(case.size, Image.BILINEAR, box=(gauche, haut, droite, bas))).astype(np.float32)[:, :, None] / 255
    if m.max() <= 0.01:
        return case
    g = np.asarray(large.resize(case.size, Image.BILINEAR, box=(gauche, haut, droite, bas))).astype(np.float32)[:, :, None] / 255
    profondeur = np.clip((g - 0.5) * 2, 0, 1)
    eau = COTE_EAU * (1 - profondeur) + LARGE_EAU * profondeur
    opacite = m * (0.72 + 0.28 * profondeur)
    sol = np.asarray(case).astype(np.float32)
    return Image.fromarray((sol * (1 - opacite) + eau * opacite).clip(0, 255).astype(np.uint8))


# 1 et 2 : les cases
cases = os.path.join(source, 'cases')
os.makedirs(cases, exist_ok=True)
faites = 0
for nom in sorted(os.listdir(source)):
    m = re.fullmatch(r'c(\d+)_r(\d+)\.bmp(?:\.png)?', nom)      # le jeu ajoute « .png » au nom demandé
    if not m:
        continue
    chemin = os.path.join(source, nom)
    cible = os.path.join(cases, 'c%s_r%s.webp' % (m.group(1), m.group(2)))
    if os.path.exists(cible) and os.path.getmtime(cible) >= os.path.getmtime(chemin):
        continue                                    # déjà recadrée : une relance reprend où elle s'était arrêtée
    try:
        image = Image.open(chemin)
        image.load()
    except Exception:
        continue                                    # capture encore en cours d'écriture : au prochain passage
    largeur, hauteur = image.size
    px_par_m = hauteur / vue
    demi = pas * px_par_m / 2
    boite = (largeur / 2 - demi, hauteur / 2 - demi, largeur / 2 + demi, hauteur / 2 + demi)
    case = image.convert('RGB').resize((cote, cote), Image.LANCZOS, box=boite)
    case = peindre_mer(debrumer(case), int(m.group(1)), int(m.group(2)))
    case.save(cible, quality=90, method=4)
    faites += 1
    if purger:
        os.remove(chemin)
print(faites, 'captures recadrées ;', len(os.listdir(cases)), 'cases sur', colonnes * rangees)
if seulement_cases:
    sys.exit(0)

# 3 : les tuiles du zoom 7, bande par bande (une bande = une rangée de cases), puis les zooms inférieurs par fusion
monde_px = MONDE * PX_PAR_M


def ecrire(z, tx, ty, image):
    dossier = os.path.join(sortie, str(z), str(tx))
    os.makedirs(dossier, exist_ok=True)
    image.save(os.path.join(dossier, '%d.webp' % ty), quality=82, method=4)


tuiles = {}                                           # (tx, ty) -> image en cours de remplissage
total = 0
for rangee in range(rangees - 1, -1, -1):             # du nord au sud : les tuiles se complètent dans l'ordre
    for colonne in range(colonnes):
        chemin = os.path.join(cases, 'c%d_r%d.webp' % (colonne, rangee))
        if not os.path.exists(chemin):
            continue
        case = Image.open(chemin).convert('RGB')
        gauche = int(round((x0 + colonne * pas) * PX_PAR_M))
        haut = monde_px - int(round((z0 + (rangee + 1) * pas) * PX_PAR_M))
        for ty in range(haut // TUILE, (haut + cote - 1) // TUILE + 1):
            for tx in range(gauche // TUILE, (gauche + cote - 1) // TUILE + 1):
                if (tx, ty) not in tuiles:
                    existante = os.path.join(sortie, str(ZMAX), str(tx), '%d.webp' % ty)
                    if os.path.exists(existante) and (tx, ty) not in tuiles:
                        tuiles[(tx, ty)] = Image.open(existante).convert('RGBA')
                    else:
                        tuiles[(tx, ty)] = Image.new('RGBA', (TUILE, TUILE), (0, 0, 0, 0))   # transparent : le fond reste visible
                tuiles[(tx, ty)].paste(case, (gauche - tx * TUILE, haut - ty * TUILE))
    # les tuiles entièrement au nord de la prochaine rangée sont finies
    limite = (monde_px - int(round((z0 + rangee * pas) * PX_PAR_M))) // TUILE
    for (tx, ty) in [k for k in tuiles if k[1] < limite]:
        ecrire(ZMAX, tx, ty, tuiles.pop((tx, ty)))
        total += 1
for (tx, ty), image in tuiles.items():
    ecrire(ZMAX, tx, ty, image)
    total += 1
print('zoom', ZMAX, ':', total, 'tuiles')

for z in range(ZMAX - 1, -1, -1):
    parents = set()
    racine = os.path.join(sortie, str(z + 1))
    for tx in os.listdir(racine):
        for nom in os.listdir(os.path.join(racine, tx)):
            parents.add((int(tx) // 2, int(nom.split('.')[0]) // 2))
    for (tx, ty) in parents:
        image = Image.new('RGBA', (TUILE * 2, TUILE * 2), (0, 0, 0, 0))
        for dx in (0, 1):
            for dy in (0, 1):
                enfant = os.path.join(racine, str(tx * 2 + dx), '%d.webp' % (ty * 2 + dy))
                if os.path.exists(enfant):
                    image.paste(Image.open(enfant).convert('RGBA'), (dx * TUILE, dy * TUILE))
        ecrire(z, tx, ty, image.resize((TUILE, TUILE), Image.LANCZOS))
    print('zoom', z, ':', len(parents), 'tuiles')
