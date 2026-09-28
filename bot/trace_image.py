# -*- coding: utf-8 -*-
"""PAG-Bot — les images fabriquées à partir des tuiles de la carte web (Pillow requis) :
  - le tracé d'une opération (fabriquer), posté dans le salon des opérations et servi par /carte/api/trace.jpg ;
  - le front (fabriquer_front, #75) : carrés bleus et rouges, contours de zones, ligne de front et noms, posté chaque soir
    à 23 h et à la victoire dans 🚩┃territoire, et servi par /carte/api/front.jpg. Juste la carte : aucun chiffre (I7).

Le monde fait 16 384 m de côté ; au zoom z une tuile de 256 px couvre 16384 / 2^z mètres, l'origine est en bas à gauche
(même repère que la page : pixel x = X / 64 * 2^z, pixel y = (256 - Z / 64) * 2^z). Les trajets sont des suites de carrés
de 100 m : on relie leurs centres. Un trou de plus de 10 minutes ou un saut de plus de 1,5 km (réapparition) coupe le trait.
"""
import io
import os
import time

COULEURS = [(77, 163, 255), (255, 176, 32), (61, 220, 132), (255, 122, 89), (155, 140, 255), (55, 194, 194), (255, 93, 93),
            (240, 220, 90), (230, 130, 220), (150, 200, 90), (255, 255, 255), (120, 160, 200)]
POLICES = ["/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", "C:/Windows/Fonts/segoeuib.ttf", "C:/Windows/Fonts/arialbd.ttf"]
FRONT_ALPHA_BLEU = 85       # transparence des carrés bleus sur l'image du front (sur 255)
FRONT_ALPHA_ROUGE = 70      # transparence des carrés rouges
FRONT_BLEU = (77, 163, 255)
FRONT_ROUGE = (255, 77, 77)
FRONT_TRAIT = (255, 214, 64)
FRONT_HALO = (10, 14, 20, 230)


def _police(taille):
    from PIL import ImageFont
    for chemin in POLICES:
        if os.path.isfile(chemin):
            try:
                return ImageFont.truetype(chemin, taille)
            except OSError:
                pass
    return ImageFont.load_default()


def _taille(police):
    return getattr(police, "size", 12)


def segments(points):
    """Coupe un trajet en traits continus."""
    traits, courant = [], []
    for t, gx, gz in points:
        if courant:
            pt, px, pz = courant[-1]
            if t - pt > 600 or abs(gx - px) > 15 or abs(gz - pz) > 15:
                traits.append(courant)
                courant = []
        courant.append((t, gx, gz))
    if courant:
        traits.append(courant)
    return traits


def _fond(dossier_carte, x0, x1, z0, z1, largeur_min=1200):
    """Le fond de carte d'un cadre carré (mètres du jeu) : (image RGBA voilée, pixel(x, z) -> coordonnées dans l'image,
    largeur). Zoom choisi pour 1 800 px au plus, tuiles « tuiles » puis « tuiles_aerien », agrandissement jusqu'à
    largeur_min, voile sombre pour que les tracés ressortent."""
    from PIL import Image

    cote = max(x1 - x0, z1 - z0)
    zoom = 6
    while zoom > 2 and cote / (64 / 2 ** zoom) > 1800:
        zoom -= 1
    echelle = 2 ** zoom / 64.0                               # pixels par mètre

    def absolu(x, z):
        return x * echelle, (16384 - z) * echelle

    gauche, haut = absolu(x0, z1)
    droite, bas = absolu(x1, z0)
    largeur, hauteur = int(droite - gauche), int(bas - haut)
    image = Image.new("RGB", (largeur, hauteur), (12, 20, 36))

    for tx in range(int(gauche // 256), int(droite // 256) + 1):
        for ty in range(int(haut // 256), int(bas // 256) + 1):
            for couche in ("tuiles", "tuiles_aerien"):
                chemin = os.path.join(dossier_carte, couche, str(zoom), str(tx), "%d.webp" % ty)
                if not os.path.isfile(chemin):
                    continue
                try:
                    tuile = Image.open(chemin).convert("RGBA")
                except OSError:
                    continue
                image.paste(tuile, (int(tx * 256 - gauche), int(ty * 256 - haut)), tuile)

    # Petit agrandissement si le cadre est étroit : l'image finale fait au moins largeur_min px
    if largeur < largeur_min:
        facteur = float(largeur_min) / largeur
        image = image.resize((largeur_min, int(hauteur * facteur)), Image.LANCZOS)
        echelle *= facteur
        gauche *= facteur
        haut *= facteur
        largeur, hauteur = image.size

    voile = Image.new("RGBA", image.size, (8, 12, 18, 70))
    image = Image.alpha_composite(image.convert("RGBA"), voile)

    def pixel(x, z):
        return x * echelle - gauche, (16384 - z) * echelle - haut

    return image, pixel, largeur


def fabriquer(dossier_carte, trajets, titre, sous_titre=""):
    """JPEG (octets) du tracé, ou None s'il n'y a rien à dessiner."""
    from PIL import ImageDraw

    tous = [(gx, gz) for t in trajets.values() for _, gx, gz in t["points"]]
    if not tous:
        return None
    x0 = min(g[0] for g in tous) * 100 - 400
    x1 = max(g[0] for g in tous) * 100 + 500
    z0 = min(g[1] for g in tous) * 100 - 400
    z1 = max(g[1] for g in tous) * 100 + 500
    cote = max(x1 - x0, z1 - z0, 1600)                       # cadre carré, 1,6 km au moins
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    x0, x1, z0, z1 = cx - cote / 2, cx + cote / 2, cz - cote / 2, cz + cote / 2

    image, pixel, largeur = _fond(dossier_carte, x0, x1, z0, z1)
    hauteur = image.size[1]
    dessin = ImageDraw.Draw(image)
    epaisseur = max(3, int(largeur / 320))

    legende = []
    for index, (code, trajet) in enumerate(sorted(trajets.items(), key=lambda kv: kv[1]["nom"].lower())):
        couleur = COULEURS[index % len(COULEURS)]
        legende.append((trajet["nom"], couleur))
        for trait in segments(trajet["points"]):
            pts = []
            for _, gx, gz in trait:
                pts.append(pixel(gx * 100 + 50, gz * 100 + 50))
            if len(pts) >= 2:
                dessin.line(pts, fill=(10, 14, 20, 255), width=epaisseur + 3, joint="curve")
                dessin.line(pts, fill=couleur + (255,), width=epaisseur, joint="curve")
            r = epaisseur + 2
            x, y = pts[-1]
            dessin.ellipse((x - r, y - r, x + r, y + r), fill=couleur + (255,), outline=(10, 14, 20, 255), width=2)

    # Bandeau du haut et légende
    grand, petit = _police(max(22, largeur // 45)), _police(max(14, largeur // 80))
    dessin.rectangle((0, 0, largeur, _taille(grand) + _taille(petit) + 30), fill=(12, 17, 22, 215))
    dessin.text((18, 10), titre, font=grand, fill=(201, 162, 39, 255))
    if sous_titre:
        dessin.text((18, 16 + _taille(grand)), sous_titre, font=petit, fill=(215, 224, 232, 255))

    ligne = _taille(petit) + 8
    visibles = legende[:16]
    haut_legende = hauteur - ligne * len(visibles) - 16
    largeur_legende = max(dessin.textlength(nom, font=petit) for nom, _ in visibles) + 50
    dessin.rectangle((10, haut_legende - 8, 10 + largeur_legende, hauteur - 10), fill=(12, 17, 22, 215))
    for i, (nom, couleur) in enumerate(visibles):
        y = haut_legende + i * ligne
        dessin.rectangle((20, y + 3, 40, y + _taille(petit) - 1), fill=couleur + (255,))
        dessin.text((48, y), nom, font=petit, fill=(215, 224, 232, 255))

    sortie = io.BytesIO()
    image.convert("RGB").save(sortie, "JPEG", quality=88, optimize=True)
    return sortie.getvalue()


def sous_titre_periode(debut, fin, paris=None):
    import datetime
    a = datetime.datetime.fromtimestamp(debut, paris) if paris else datetime.datetime.fromtimestamp(debut)
    b = datetime.datetime.fromtimestamp(fin, paris) if paris else datetime.datetime.fromtimestamp(fin)
    return "%s, de %s à %s — Everon, carrés de 100 m" % (a.strftime("%d/%m/%Y"), a.strftime("%H h %M"), b.strftime("%H h %M"))


# ================================================================================================== front (#75)
def fabriquer_front(dossier_carte, front, titre, sous_titre=""):
    """JPEG (octets) du front, ou None sans grille. front = Front.pour_image() de campagne.py : n, k, g, zones (l, lx, lz,
    o, f) et les segments du front et des contours, déjà calculés. Juste la carte : aucun chiffre, aucun bilan (I7)."""
    from PIL import Image, ImageDraw

    if not front or not front.get("g"):
        return None
    n, k, g = int(front["n"]), int(front["k"]), front["g"]
    terre = [(i % n, i // n, c == "B") for i, c in enumerate(g) if c != "."]
    if not terre or n <= 0 or k <= 0:
        return None
    # Cadre : boîte des carrés de terre + 400 m (et 800 m de plus en haut, sous le bandeau du titre), rendue carrée
    # (l'île entière sort au zoom 3, 8 m par pixel)
    x0 = min(c[0] for c in terre) * k - 400
    x1 = (max(c[0] for c in terre) + 1) * k + 400
    z0 = min(c[1] for c in terre) * k - 400
    z1 = (max(c[1] for c in terre) + 1) * k + 1200
    cote = max(x1 - x0, z1 - z0, 1600)
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    x0, x1, z0, z1 = cx - cote / 2, cx + cote / 2, cz - cote / 2, cz + cote / 2
    image, pixel, largeur = _fond(dossier_carte, x0, x1, z0, z1)
    hauteur = image.size[1]

    # Carrés : un rectangle par carré de terre, sur un calque transparent (jamais deux fois le même pixel)
    calque = Image.new("RGBA", image.size, (0, 0, 0, 0))
    dessin = ImageDraw.Draw(calque)
    bleu = FRONT_BLEU + (FRONT_ALPHA_BLEU,)
    rouge = FRONT_ROUGE + (FRONT_ALPHA_ROUGE,)
    for gx, gz, est_bleu in terre:
        ax, ay = pixel(gx * k, (gz + 1) * k)
        bx, by = pixel((gx + 1) * k, gz * k)
        dessin.rectangle((int(round(ax)), int(round(ay)), int(round(bx)) - 1, int(round(by)) - 1), fill=bleu if est_bleu else rouge)
    image = Image.alpha_composite(image, calque)

    def points(segment):
        (xa, za), (xb, zb) = segment
        return [pixel(xa, za), pixel(xb, zb)]

    # Contours de zones, fins et discrets
    calque = Image.new("RGBA", image.size, (0, 0, 0, 0))
    dessin = ImageDraw.Draw(calque)
    for segment in front.get("contours") or []:
        dessin.line(points(segment), fill=(255, 255, 255, 70), width=1)
    image = Image.alpha_composite(image, calque)

    # Ligne de front : halo sombre puis trait jaune ; un petit carré plein à chaque bout bouche les angles
    epaisseur = max(3, largeur // 350)
    calque = Image.new("RGBA", image.size, (0, 0, 0, 0))
    dessin = ImageDraw.Draw(calque)
    traits = [points(s) for s in front.get("front") or []]
    for couleur, largeur_trait in ((FRONT_HALO, epaisseur + 4), (FRONT_TRAIT + (255,), epaisseur)):
        demi = largeur_trait / 2.0
        for pts in traits:
            dessin.line(pts, fill=couleur, width=largeur_trait)
            for px, py in pts:
                dessin.rectangle((int(px - demi), int(py - demi), int(px + demi), int(py + demi)), fill=couleur)
    image = Image.alpha_composite(image, calque)

    # Noms des zones tenues ou au contact, petite police, ombre noire
    dessin = ImageDraw.Draw(image)
    petite = _police(max(12, largeur // 115))
    for zone in front.get("zones") or []:
        if not zone.get("o") and not zone.get("f"):
            continue
        px, py = pixel(zone["lx"], zone["lz"])
        texte = str(zone.get("l", ""))
        large = dessin.textlength(texte, font=petite)
        tx, ty = px - large / 2, py - _taille(petite) / 2
        couleur = (228, 240, 255, 255) if zone.get("o") else (255, 214, 214, 255)
        for dx, dy in ((1, 1), (-1, 1), (1, -1), (-1, -1), (0, 2)):
            dessin.text((tx + dx, ty + dy), texte, font=petite, fill=(0, 0, 0, 255))
        dessin.text((tx, ty), texte, font=petite, fill=couleur)

    # Bandeau du haut : titre en or et sous-titre
    grand, moyen = _police(max(22, largeur // 45)), _police(max(14, largeur // 80))
    dessin.rectangle((0, 0, largeur, _taille(grand) + _taille(moyen) + 30), fill=(12, 17, 22, 215))
    dessin.text((18, 10), titre, font=grand, fill=(201, 162, 39, 255))
    if sous_titre:
        dessin.text((18, 16 + _taille(grand)), sous_titre, font=moyen, fill=(215, 224, 232, 255))

    # Légende : carré bleu « PAG », carré rouge « Ennemi », trait jaune « Front »
    ligne = _taille(moyen) + 8
    entrees = [("PAG", "carre", FRONT_BLEU), ("Ennemi", "carre", FRONT_ROUGE), ("Front", "trait", FRONT_TRAIT)]
    haut_legende = hauteur - ligne * len(entrees) - 16
    largeur_legende = max(dessin.textlength(nom, font=moyen) for nom, _, _ in entrees) + 60
    dessin.rectangle((10, haut_legende - 8, 10 + largeur_legende, hauteur - 10), fill=(12, 17, 22, 215))
    for rang, (nom, forme, couleur) in enumerate(entrees):
        y = haut_legende + rang * ligne
        if forme == "carre":
            dessin.rectangle((20, y + 2, 42, y + _taille(moyen)), fill=couleur + (255,), outline=(10, 14, 20, 255))
        else:
            milieu = y + _taille(moyen) // 2 + 1
            dessin.rectangle((18, milieu - epaisseur // 2 - 2, 44, milieu + epaisseur // 2 + 2), fill=FRONT_HALO)
            dessin.rectangle((20, milieu - epaisseur // 2, 42, milieu + epaisseur // 2), fill=FRONT_TRAIT + (255,))
        dessin.text((52, y), nom, font=moyen, fill=(215, 224, 232, 255))

    sortie = io.BytesIO()
    image.convert("RGB").save(sortie, "JPEG", quality=88, optimize=True)
    return sortie.getvalue()


def sous_titre_front(t, paris=None, prefixe="Au soir du", heure=False):
    """« Au soir du 26/09/2026 · Everon, carrés de 200 m » ; avec heure : « Situation au 26/09/2026 à 23 h 10 · … »."""
    import datetime
    d = datetime.datetime.fromtimestamp(t, paris) if paris else datetime.datetime.fromtimestamp(t)
    quand = d.strftime("%d/%m/%Y") + (d.strftime(" à %H h %M") if heure else "")
    return "%s %s · Everon, carrés de 200 m" % (prefixe, quand)
