# -*- coding: utf-8 -*-
"""PAG-Bot — la mémoire de la campagne, pour le site web (module sans Discord).

À chaque envoi du jeu (toutes les 5 s) `Campagne.ingest()` retient, sans jamais rien demander au jeu en plus :
  - les SOLDATS vus : nom, grade, certifications, heures, missions, première et dernière fois vus (fiche et annuaire) ;
  - les SECTEURS (ancien mod seulement) : chaque changement de propriétaire, avec les soldats présents autour. Au premier
    front reçu, cet historique part une fois pour toutes dans archives/ (K1) et n'est plus nourri, sauf si un ancien mod
    est remis ensuite (retour arrière : ses secteurs sont de nouveau notés) ;
  - les TRAJETS : le carré de 100 m de chaque soldat, noté seulement quand il en CHANGE. Un fichier par jour, 30 jours
    gardés : quelques dizaines de Ko par soirée. Rien n'est écrit quand le serveur est vide ;
  - les dernières MISSIONS terminées (page Campagne).

Le FRONT (#75) est tenu par la classe `Front` : grille des carrés de 200 m et zones (front.json), photos de la frise
(frise.json), mouvements, archives des campagnes finies. Il est nourri par `Campagne.front_ingest()` (bloc « front » du
pont), lu par le site même serveur éteint, et ne dit rien des forces ennemies (I3).

Ce qui sort d'ici est public (le site n'a pas de connexion) : jamais d'identifiant du jeu, d'infraction, de sanction ni
de statut Staff. Un soldat est désigné par un CODE court, dérivé de son identité par HMAC avec le secret du pont : on ne
peut pas remonter du code à l'identité.
"""
import hashlib
import hmac
import json
import os
import re
import time

try:
    from zoneinfo import ZoneInfo
    PARIS = ZoneInfo("Europe/Paris")
except Exception:                       # noqa: BLE001
    PARIS = None

GARDER_JOURS = 30           # trajets
POINTS_MAX_PAR_JOUR = 6000  # par soldat : garde-fou (une soirée normale en fait quelques centaines)
HISTORIQUE_MAX = 2000
MISSIONS_MAX = 20
AUTOUR = 3                  # « présents » à la prise d'un secteur : à 3 carrés (300 m) ou moins du carré du secteur

# Front (#75) : valeurs par défaut, écrasées par les clés front_* de config.json (bot.py passe ses réglages)
FRISE_JOURS = 30            # photos du jour gardées dans la frise (jours)
FRISE_ZONES_JOURS = 0       # photos de zone prise ou perdue gardées (jours) ; 0 = toute la campagne
MOUVEMENTS_MAX = 50         # mouvements du front gardés (la page Campagne montre les 8 derniers)
PHOTO_DELAI = 15            # secondes entre la bascule d'une zone et sa photo : le temps que tous ses carrés arrivent
DELTA_MAX = 2000            # garde-fou : au-delà, les changements sont refusés et la grille entière redemandée
TAILLE_MAX = 128            # côté de grille le plus grand accepté (Everon : 64)
PHOTO_SI_INCHANGEE = False  # prendre la photo du jour même identique à la précédente
EVENEMENTS_MAX = 60         # évènements lus par envoi
PHOTOS_MAX = 3000           # garde-fou mémoire de la frise (environ 0,4 Ko par photo)
ARCHIVE_RE = re.compile(r"^[A-Za-z0-9-]{1,60}$")
CAMPAGNE_RE = re.compile(r"^[A-Za-z0-9_-]{1,40}$")
REF_RE = re.compile(r"^\d{1,3} \d{1,3}$")            # référence d'un carré de 100 m, « 074 042 » (A7)
GENRES = {0: "lieu-dit", 1: "hameau", 2: "village", 3: "ville"}


def _jour(t):
    import datetime
    if PARIS:
        return datetime.datetime.fromtimestamp(t, PARIS).strftime("%Y-%m-%d")
    return time.strftime("%Y-%m-%d", time.localtime(t))


def _carre(texte):
    """« 074 043 » -> (74, 43), ou None."""
    try:
        a, b = str(texte).split()
        return int(a), int(b)
    except (ValueError, AttributeError):
        return None


# ================================================================================================== front (#75)
def _entier(valeur, defaut=0):
    """Un entier lu dans le JSON du pont, ou le défaut s'il est illisible."""
    try:
        return int(valeur)
    except (TypeError, ValueError):
        return defaut


def _vrai(valeur):
    """Un booléen lu dans le JSON du pont : true, 1, "1", "true"."""
    if isinstance(valeur, str):
        return valeur.strip().lower() in ("1", "true", "vrai", "oui")
    return bool(valeur)


def _lire_json(chemin, defaut):
    try:
        with open(chemin, encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return defaut


def _ecrire_json(chemin, data, compact=False):
    """Écriture atomique : un fichier à moitié écrit ne remplace jamais le bon."""
    tmp = chemin + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        if compact:
            json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
        else:
            json.dump(data, f, ensure_ascii=False, indent=1)
    os.replace(tmp, chemin)


def bits(g):
    """Les carrés de terre de la grille, dans l'ordre des index : 1 = bleu. 4 carrés par caractère hexadécimal, bit de
    poids fort = premier carré, complété par des 0 (1 395 carrés : 349 caractères)."""
    sortie = []
    acc = 0
    nb = 0
    for c in g:
        if c == ".":
            continue
        acc = (acc << 1) | (1 if c == "B" else 0)
        nb += 1
        if nb == 4:
            sortie.append("%x" % acc)
            acc = 0
            nb = 0
    if nb:
        sortie.append("%x" % (acc << (4 - nb)))
    return "".join(sortie)


def masque(g):
    """Le masque de la frise : « . » mer, « x » terre (les bits des photos se lisent dans cet ordre)."""
    return "".join("." if c == "." else "x" for c in g)


def grille_de_photo(masque_txt, b):
    """Inverse de bits() : la grille « . R B » d'une photo, d'après le masque (la page fait le même calcul en JS)."""
    sortie = []
    rang = 0
    for c in masque_txt:
        if c == ".":
            sortie.append(".")
            continue
        try:
            valeur = int(b[rang >> 2], 16)
        except (IndexError, ValueError):
            valeur = 0
        sortie.append("B" if (valeur >> (3 - (rang & 3))) & 1 else "R")
        rang += 1
    return "".join(sortie)


def segments(g, n, k, differe):
    """Les arêtes entre deux carrés de terre voisins par un côté quand differe(i, j) est vrai, fusionnées en plages
    contiguës : une liste de segments ((x1, z1), (x2, z2)) en mètres. Front : g[i] != g[j] ; contours de zones : zm
    différent. La carte web fait le même calcul en JS (segmentsFront)."""
    horizontales = {}      # rangée de la limite (z = r x k) -> colonnes gx des arêtes, dans l'ordre
    verticales = {}        # colonne de la limite (x = c x k) -> rangées gz des arêtes, dans l'ordre
    for gz in range(n):
        debut_rang = gz * n
        for gx in range(n):
            i = debut_rang + gx
            if g[i] == ".":
                continue
            if gx + 1 < n and g[i + 1] != "." and differe(i, i + 1):
                verticales.setdefault(gx + 1, []).append(gz)
            if gz + 1 < n and g[i + n] != "." and differe(i, i + n):
                horizontales.setdefault(gz + 1, []).append(gx)
    traits = []
    for rangee, colonnes in horizontales.items():
        premier = precedent = colonnes[0]
        for gx in colonnes[1:] + [None]:
            if gx is not None and gx == precedent + 1:
                precedent = gx
                continue
            traits.append(((premier * k, rangee * k), ((precedent + 1) * k, rangee * k)))
            if gx is not None:
                premier = precedent = gx
    for colonne, rangees in verticales.items():
        premier = precedent = rangees[0]
        for gz in rangees[1:] + [None]:
            if gz is not None and gz == precedent + 1:
                precedent = gz
                continue
            traits.append(((colonne * k, premier * k), (colonne * k, (precedent + 1) * k)))
            if gz is not None:
                premier = precedent = gz
    return traits


class Front:
    """La mémoire du front côté bot : front.json (grille, zones, mouvements) et frise.json (photos), plus les archives des
    campagnes finies. Nourrie par le bloc « front » du pont (instantané complet une fois par minute, changements entre les
    deux), elle survit au serveur éteint : la carte web, l'accueil et la page Campagne la lisent. Rien de ce qui est gardé
    ici ne dit quoi que ce soit des forces ennemies (I3) : couleurs, zones, front, dépôts de la PAG."""

    def __init__(self, dossier, reglages=None):
        self.dossier = dossier
        os.makedirs(os.path.join(dossier, "archives"), exist_ok=True)
        self.regler(reglages or {})
        self.etat = _lire_json(self._chemin("front.json"), {})
        if not isinstance(self.etat, dict):
            self.etat = {}
        self.frise = _lire_json(self._chemin("frise.json"), {})
        if not isinstance(self.frise, dict) or not isinstance(self.frise.get("photos"), list):
            self.frise = {"campagne": str(self.etat.get("campagne") or ""), "masque": "", "photos": []}
        self.contestes = []          # carrés contestés du dernier envoi (C8) : jamais sauvegardés ni photographiés
        self.attente = []            # photos de zone programmées : {"due", "type", "zone", "texte"}
        self.victoire = False        # B3 : l'image de la victoire est à poster (bot.py la prend par prendre_victoire)
        self.sale = set()            # "front", "frise" : fichiers à écrire
        self.cache = None            # calculs d'affichage de la révision courante (derive)
        self.quand_archive = None    # rappel posé par la campagne : la liste des archives a changé

    def regler(self, reglages):
        """Les réglages front_* de config.json (bot.py), sinon les valeurs par défaut du module."""
        self.frise_jours = max(1, _entier(reglages.get("front_photo_days", FRISE_JOURS), FRISE_JOURS))
        self.frise_zones_jours = max(0, _entier(reglages.get("front_zone_photo_days", FRISE_ZONES_JOURS), FRISE_ZONES_JOURS))
        self.photo_si_inchangee = _vrai(reglages.get("front_photo_if_unchanged", PHOTO_SI_INCHANGEE))
        self.photo_delai = max(0, _entier(reglages.get("front_photo_delay_seconds", PHOTO_DELAI), PHOTO_DELAI))
        self.mouvements_max = max(8, _entier(reglages.get("front_movements_max", MOUVEMENTS_MAX), MOUVEMENTS_MAX))
        self.delta_max = max(1, _entier(reglages.get("front_delta_max", DELTA_MAX), DELTA_MAX))
        self.evenements_max = max(1, _entier(reglages.get("events_per_sync_max", EVENEMENTS_MAX), EVENEMENTS_MAX))

    def _chemin(self, nom):
        return os.path.join(self.dossier, nom)

    def a_front(self):
        """Un front a-t-il déjà été reçu (grille et campagne connues) ?"""
        return bool(self.etat.get("g")) and bool(self.etat.get("campagne"))

    def _label(self, code):
        zone = (self.etat.get("zones") or {}).get(code) or {}
        return str(zone.get("l") or code)

    # ------------------------------------------------------------------------------------------ réception
    def ingest(self, bloc, evenements, now):
        """Le bloc « front » d'un envoi du pont. Rend la ligne de réponse pour le mod : « #front <id> <v> » (le bot tient
        cette campagne à cette version), « #front plein » (il redemande la grille entière), ou "" (pas de front)."""
        if not isinstance(bloc, dict):
            return ""
        cid = str(bloc.get("id", ""))[:40]
        if not CAMPAGNE_RE.match(cid):
            return ""            # identifiant vide ou avec une espace : l'accusé serait illisible, on n'accuse rien
        v = _entier(bloc.get("v"), -1)
        rs = _entier(bloc.get("rs"), 0)
        e = self.etat
        connue = str(e.get("campagne") or "")
        ligne = "#front plein"
        debut = False
        if _vrai(bloc.get("plein")):
            grille = self._grille_valide(bloc)
            if grille:
                n, k, g, zm = grille
                debut = connue != cid
                if connue and connue != cid:
                    self.archiver(now)          # B3 : la campagne finie part aux archives, la frise repart à zéro
                self._poser_grille(cid, v, n, k, g, zm, bloc.get("zones"), debut, now)
                ligne = "#front %s %d" % (cid, v)
        elif connue == cid and e.get("g") and _entier(bloc.get("base"), -2) == _entier(e.get("v"), -1):
            changements = bloc.get("d")
            if isinstance(changements, list) and len(changements) <= self.delta_max:
                self._appliquer_changements(changements)
                e["v"] = v
                ligne = "#front %s %d" % (cid, v)
        if str(e.get("campagne") or "") != cid:
            return ligne         # campagne pas encore reçue en entier : rien d'autre à lire dans ce bloc
        restauration = not debut and rs != _entier(e.get("rs"), 0)
        if rs != _entier(e.get("rs"), 0):
            e["rs"] = rs
            self.sale.add("front")
        # stf : les zones forcées par le Staff depuis l'envoi précédent (correction, Q9) : ni mouvement ni photo
        staff = set()
        if isinstance(bloc.get("stf"), list):
            staff = {str(code)[:12] for code in bloc["stf"][:256] if code}
        # J2 et nouvelle campagne : l'état des zones est remplacé d'un coup, sans une prise par zone
        self._appliquer_zs(bloc.get("zs"), now, debut or restauration, staff)
        self.contestes = self._refs(bloc.get("cs"))
        gele = _vrai(bloc.get("gel"))
        if gele != bool(e.get("gele")):
            e["gele"] = gele
            self.sale.add("front")
        e["recu"] = int(now)
        if debut:
            e["debut"] = int(now)
            self.mouvement("debut", "", "Début de la campagne", now)
            self.photo("debut", "", "Début de la campagne", now)
        elif restauration:
            self.attente = []    # les photos de zone d'avant le retour en arrière n'ont plus de sens
            self.mouvement("restauration", "", "Front ramené à un état antérieur", now)
            self.photo("restauration", "", "Front ramené à un état antérieur", now)
        self._evenements(evenements, now)
        self._reviser(now)
        return ligne

    def _grille_valide(self, bloc):
        """(n, k, g, zm) d'un instantané complet, ou None s'il est incohérent (tailles, caractères)."""
        n = _entier(bloc.get("n"), 0)
        k = _entier(bloc.get("k"), 0)
        g = bloc.get("g")
        zm = bloc.get("zm")
        if not isinstance(g, str) or not isinstance(zm, str):
            return None
        if n <= 0 or n > TAILLE_MAX or k <= 0 or k > 100000:
            return None
        if len(g) != n * n or len(zm) != 2 * n * n:
            return None
        if set(g) - set(".RB"):
            return None
        zm = zm.upper()          # int.ToString(2, true) écrit en majuscules, mais on ne suppose rien
        if set(zm) - set("0123456789ABCDEF"):
            return None
        return n, k, g, zm

    def _poser_grille(self, cid, v, n, k, g, zm, metas, neuve, now):
        """Instantané complet : grille, découpage et identité des zones (l'état des zones vient de zs, juste après)."""
        e = self.etat
        anciennes = {} if neuve else (e.get("zones") or {})
        zones = {}
        for meta in metas if isinstance(metas, list) else []:
            if not isinstance(meta, dict):
                continue
            code = str(meta.get("c", ""))[:12]
            index = _entier(meta.get("i"), -1)
            if not code or index < 0 or index > 254:
                continue
            genre = meta.get("genre", "")
            if isinstance(genre, int) and not isinstance(genre, bool):
                genre = GENRES.get(genre, "")
            info = dict(anciennes.get(code) or {"o": 0, "a": 0, "f": 0, "k": 0, "kr": -1, "p": 0})
            info.update({"i": index, "n": str(meta.get("n", "") or "")[:40], "l": str(meta.get("l", "") or meta.get("n", "") or code)[:60],
                         "genre": str(genre or "")[:12]})
            zones[code] = info
        if not zones and anciennes:
            zones = anciennes    # instantané sans la liste des zones : on garde celle qu'on a
        if neuve:
            e["mouvements"] = []
            e["debut"] = int(now)
        e.update({"format": 1, "campagne": cid, "v": v, "n": n, "k": k, "g": g, "zm": zm, "zones": zones})
        self.sale.add("front")
        self.cache = None

    def _appliquer_changements(self, changements):
        """Changements « gx gz S » (coin sud-ouest en carrés de 100 m, S = B ou R) depuis la version que le bot tenait."""
        e = self.etat
        n = _entier(e.get("n"), 0)
        g = list(e.get("g") or "")
        if n <= 0 or len(g) != n * n:
            return
        # pas = carrés de 100 m par carré du front, d'après la taille « k » envoyée par le pont (200 m par défaut : pas 2).
        # Arrondi au plus proche, moitié vers le haut, comme Math.round de la carte (round() de Python arrondirait 2,5 à 2).
        pas = max(1, ((_entier(e.get("k"), 0) or 200) + 50) // 100)
        for item in changements:
            morceaux = str(item).split()
            if len(morceaux) != 3:
                continue
            gx2 = _entier(morceaux[0], -1)
            gz2 = _entier(morceaux[1], -1)
            camp = morceaux[2].upper()
            if camp not in ("B", "R") or gx2 < 0 or gz2 < 0:
                continue
            gx = gx2 // pas
            gz = gz2 // pas
            if gx >= n or gz >= n:
                continue
            index = gz * n + gx
            if g[index] == ".":
                continue
            g[index] = camp
        e["g"] = "".join(g)
        self.sale.add("front")

    def _refs(self, cs):
        if not isinstance(cs, list):
            return []
        return [str(r) for r in cs[:4096] if REF_RE.match(str(r))]

    def _appliquer_zs(self, zs, now, silencieux, staff=()):
        """zs est ABSOLU : une zone absente est rouge, calme et sans dépôt. Chaque zone qui change de camp donne un
        mouvement et une photo 15 s plus tard (sauf silencieux : début de campagne, retour à une sauvegarde ; et sauf les
        zones de staff, forcées par le Staff : une correction n'est pas une prise). Leur état, lui, est toujours tenu."""
        zones = self.etat.get("zones") or {}
        recues = {}
        if isinstance(zs, list):
            for z in zs:
                if isinstance(z, dict) and z.get("c"):
                    recues[str(z.get("c"))[:12]] = z
        for code, info in zones.items():
            z = recues.get(code) or {}
            avant_o = _entier(info.get("o"), 0)
            avant_k = _entier(info.get("k"), 0)
            o = 1 if _vrai(z.get("o", 0)) else 0
            coupee = 1 if _vrai(z.get("k", 0)) else 0
            nouveau = {"o": o, "a": max(0, min(2, _entier(z.get("a"), 0))), "f": 1 if _vrai(z.get("f", 0)) else 0, "k": coupee,
                       "p": 1 if _vrai(z.get("p", 0)) else 0}
            depot = str(z.get("dp", "") or "")[:7]
            if depot and REF_RE.match(depot):
                nouveau["dp"] = depot
                nouveau["st"] = max(0, _entier(z.get("st"), 0))
            # le décompte kr bouge à chaque envoi : il est tenu à jour sans rendre le fichier « sale »
            info["kr"] = _entier(z.get("kr"), -1) if coupee else -1
            if any(info.get(cle) != valeur for cle, valeur in nouveau.items()) or ("dp" in info and "dp" not in nouveau):
                info.update(nouveau)
                if "dp" not in nouveau:
                    info.pop("dp", None)
                    info.pop("st", None)
                self.sale.add("front")
            if o != avant_o:
                info["depuis"] = int(now)
                if not silencieux and code not in staff:
                    self._bascule(code, o, now)
            elif o == 1 and coupee != avant_k and not silencieux:
                texte = "Zone %s %s" % (self._label(code), "coupée de la base" if coupee else "reliée à la base")
                self.mouvement("coupee" if coupee else "reliee", code, texte, now)

    def _bascule(self, code, o, now):
        genre = "prise" if o else "perte"
        texte = "Zone %s %s" % (self._label(code), "prise" if o else "perdue")
        self.mouvement(genre, code, texte, now)
        self.attente.append({"due": now + self.photo_delai, "type": genre, "zone": code, "texte": texte})

    def _evenements(self, evenements, now):
        """Évènements du même envoi qui laissent une trace au front : zone défendue (mouvement), victoire (photo)."""
        if not isinstance(evenements, list):
            return
        for ev in evenements[:self.evenements_max]:
            if not isinstance(ev, dict):
                continue
            genre = str(ev.get("type", ""))
            if genre == "zone_defendue":
                code = str(ev.get("zone", "") or "")[:12]
                if code in (self.etat.get("zones") or {}):
                    texte = "Zone %s défendue" % self._label(code)
                else:
                    texte = str(ev.get("text", "") or "Zone défendue")[:200]
                self.mouvement("defendue", code, texte, now)
            elif genre == "victoire":
                self.mouvement("victoire", "", "Everon libérée", now)
                self.photo("victoire", "", "Everon libérée", now)
                self.victoire = True

    def _reviser(self, now):
        """Révision d'affichage : +1 quand la grille, le découpage ou les zones changent (hors attaques et décompte de
        coupure, qui passent par /carte/api/carte). La carte ne recharge le front que sur une nouvelle révision."""
        e = self.etat
        if not e.get("g"):
            return
        zones = {code: {cle: val for cle, val in z.items() if cle not in ("a", "kr")} for code, z in (e.get("zones") or {}).items()}
        texte = "|".join((str(e.get("campagne") or ""), e.get("g") or "", e.get("zm") or "", json.dumps(zones, sort_keys=True, ensure_ascii=False)))
        signature = hashlib.sha1(texte.encode("utf-8")).hexdigest()
        if signature != e.get("sig"):
            e["sig"] = signature
            e["rev"] = _entier(e.get("rev"), 0) + 1
            e["maj"] = int(now)
            self.cache = None
            self.sale.add("front")

    # ------------------------------------------------------------------------------------------ mouvements et frise
    def mouvement(self, genre, zone, texte, now):
        """Un mouvement du front (page Campagne) : prise, perte, defendue, coupee, reliee, restauration, victoire, debut."""
        liste = self.etat.setdefault("mouvements", [])
        liste.append({"t": int(now), "type": genre, "zone": str(zone or "")[:12], "texte": str(texte)[:200]})
        del liste[:-self.mouvements_max]
        self.sale.add("front")

    def traiter_attente(self, now):
        """Prend les photos de zone arrivées à échéance (15 s après la bascule : tous ses carrés sont arrivés)."""
        if not self.attente:
            return
        restent = []
        for photo in self.attente:
            if photo["due"] <= now:
                self.photo(photo["type"], photo["zone"], photo["texte"], now)
            else:
                restent.append(photo)
        self.attente = restent

    def photo(self, genre, zone, texte, now):
        """Une photo de la frise (I8) : les carrés de terre en bits (1 = bleu) et les zones tenues. Jamais de contestés."""
        g = self.etat.get("g")
        if not g:
            return
        b = bits(g)
        tenues = sorted(code for code, z in (self.etat.get("zones") or {}).items() if _entier(z.get("o"), 0))
        photos = self.frise.setdefault("photos", [])
        if genre == "jour" and not self.photo_si_inchangee and photos and photos[-1].get("b") == b and photos[-1].get("zt") == tenues:
            return               # le front n'a pas bougé depuis la dernière photo
        self._poser_masque(masque(g))
        self.frise["campagne"] = str(self.etat.get("campagne") or "")
        photos.append({"t": int(now), "type": genre, "zone": str(zone or "")[:12], "texte": str(texte or "")[:200], "b": b, "zt": tenues})
        limite_jour = now - self.frise_jours * 86400
        limite_zone = now - self.frise_zones_jours * 86400 if self.frise_zones_jours > 0 else None
        gardees = []
        for rang, p in enumerate(photos):
            if rang < len(photos) - 1:           # la dernière est toujours gardée
                if p.get("type") == "jour" and p.get("t", 0) < limite_jour:
                    continue
                if limite_zone is not None and p.get("type") in ("prise", "perte") and p.get("t", 0) < limite_zone:
                    continue
            gardees.append(p)
        self.frise["photos"] = gardees[-PHOTOS_MAX:]
        self.sale.add("frise")

    def _poser_masque(self, nouveau):
        """Le masque de terre change (géométrie retouchée) : les photos déjà prises sont relues puis réécrites avec le
        nouveau, les carrés disparus sont oubliés et les nouveaux comptent rouges. Taille différente : frise effacée."""
        ancien = self.frise.get("masque") or ""
        if ancien == nouveau:
            return
        photos = self.frise.get("photos") or []
        if ancien and len(ancien) == len(nouveau):
            for p in photos:
                vieille = grille_de_photo(ancien, p.get("b", ""))
                p["b"] = bits("".join("." if m == "." else ("B" if vieille[i] == "B" else "R") for i, m in enumerate(nouveau)))
        elif photos:
            self.frise["photos"] = []
        self.frise["masque"] = nouveau
        self.sale.add("frise")

    # ------------------------------------------------------------------------------------------ calculs d'affichage
    def derive(self):
        """Chiffres de la révision courante (en cache) : terre, bleus, part de l'île, et par zone t, b et la position de
        son nom (le carré de la zone le plus proche du barycentre de ses carrés)."""
        e = self.etat
        rev = e.get("rev")
        if self.cache is not None and self.cache.get("rev") == rev:
            return self.cache
        g = e.get("g") or ""
        zm = e.get("zm") or ""
        n = _entier(e.get("n"), 0)
        k = _entier(e.get("k"), 0)
        terre = sum(1 for c in g if c != ".")
        bleus = g.count("B")
        par_index = {}
        for code, z in (e.get("zones") or {}).items():
            par_index[_entier(z.get("i"), -1)] = code
        cellules = {}
        if n > 0 and len(zm) == 2 * len(g):
            for i, c in enumerate(g):
                if c == ".":
                    continue
                try:
                    index = int(zm[2 * i:2 * i + 2], 16)
                except ValueError:
                    continue
                code = par_index.get(index)
                if code is not None:
                    cellules.setdefault(code, []).append((i % n, i // n, c == "B"))
        zones = {}
        for code, liste in cellules.items():
            mx = sum(c[0] for c in liste) / len(liste)
            mz = sum(c[1] for c in liste) / len(liste)
            meilleure = min(liste, key=lambda c, mx=mx, mz=mz: (c[0] - mx) ** 2 + (c[1] - mz) ** 2)
            zones[code] = {"t": len(liste), "b": sum(1 for c in liste if c[2]),
                           "lx": int((meilleure[0] + 0.5) * k), "lz": int((meilleure[1] + 0.5) * k)}
        self.cache = {"rev": rev, "terre": terre, "bleus": bleus, "part": round(100.0 * bleus / terre, 1) if terre else 0.0,
                      "zones": zones, "front": None, "contours": None}
        return self.cache

    def _traits(self):
        """Segments du front et des contours de zones de la révision courante (calculés une fois par révision)."""
        d = self.derive()
        if d["front"] is None:
            e = self.etat
            g = e.get("g") or ""
            zm = e.get("zm") or ""
            n = _entier(e.get("n"), 0)
            k = _entier(e.get("k"), 0)
            d["front"] = segments(g, n, k, lambda i, j: g[i] != g[j])
            d["contours"] = segments(g, n, k, lambda i, j: zm[2 * i:2 * i + 2] != zm[2 * j:2 * j + 2]) if len(zm) == 2 * len(g) else []
        return d["front"], d["contours"]

    def _zones_affichees(self, en_ligne, now):
        """Les zones pour le site, dans l'ordre des index. Attaques seulement en ligne ; décompte de coupure ramené à
        maintenant (le temps serveur éteint compte pour E4, sauf front gelé)."""
        e = self.etat
        d = self.derive()
        recu = _entier(e.get("recu"), int(now))
        sortie = []
        for code, z in sorted((e.get("zones") or {}).items(), key=lambda kv: _entier(kv[1].get("i"), 0)):
            calcul = d["zones"].get(code, {})
            o = _entier(z.get("o"), 0)
            reste = _entier(z.get("kr"), -1)
            if reste >= 0 and not e.get("gele"):
                reste = max(0, reste - max(0, int(now) - recu))
            item = {"c": code, "i": _entier(z.get("i"), -1), "l": z.get("l", code), "n": z.get("n", ""), "genre": z.get("genre", ""), "o": o,
                    "a": _entier(z.get("a"), 0) if en_ligne else 0, "f": _entier(z.get("f"), 0), "k": _entier(z.get("k"), 0),
                    "kr": reste, "p": _entier(z.get("p"), 0), "b": calcul.get("b", 0), "t": calcul.get("t", 0),
                    "lx": calcul.get("lx", 0), "lz": calcul.get("lz", 0)}
            if _entier(z.get("i"), -1) == 0:
                item["base"] = 1
            if o and z.get("dp"):
                item["dp"] = z["dp"]
                item["st"] = _entier(z.get("st"), 0)
            sortie.append(item)
        return sortie

    def public_front(self, en_ligne, now=None):
        """/carte/api/front : la grille complète et les zones, sans rien des forces ennemies (I3). None sans front."""
        if not self.a_front():
            return None
        now = now or time.time()
        e = self.etat
        d = self.derive()
        return {"campagne": e.get("campagne"), "rev": e.get("rev", 0), "maj": e.get("maj", 0), "recu": e.get("recu", 0),
                "en_ligne": bool(en_ligne), "gele": bool(e.get("gele")), "n": e.get("n"), "k": e.get("k"), "g": e.get("g"),
                "zm": e.get("zm"), "terre": d["terre"], "bleus": d["bleus"], "part": d["part"],
                "zones": self._zones_affichees(en_ligne, now)}

    def public_frise(self):
        """/carte/api/frise : le masque de terre et les photos de la campagne en cours."""
        return {"campagne": str(self.frise.get("campagne") or self.etat.get("campagne") or ""), "masque": self.frise.get("masque", ""),
                "photos": self.frise.get("photos", [])}

    def zones_campagne(self, en_ligne, now=None):
        """Les zones jouables (sans la base) pour la page Campagne."""
        if not self.a_front():
            return []
        cles = ("c", "l", "o", "a", "f", "k", "kr", "p", "b", "t")
        return [{cle: z[cle] for cle in cles} for z in self._zones_affichees(en_ligne, now or time.time()) if not z.get("base")]

    def resume(self):
        """Les chiffres d'accueil : part de l'île tenue, carrés, zones tenues sur zones jouables (sans la base)."""
        if not self.a_front():
            return None
        e = self.etat
        d = self.derive()
        jouables = [z for z in (e.get("zones") or {}).values() if _entier(z.get("i"), -1) != 0]
        return {"part": d["part"], "bleus": d["bleus"], "terre": d["terre"], "zones_nous": sum(1 for z in jouables if _entier(z.get("o"), 0)),
                "zones_total": len(jouables), "maj": e.get("maj", 0), "recu": e.get("recu", 0), "gele": bool(e.get("gele"))}

    def zones_contact(self):
        """J4 : zones ennemies au contact du front (o 0, f 1), pour /ordonner capture : [{"c", "l", "a"}]."""
        sortie = []
        for code, z in (self.etat.get("zones") or {}).items():
            if _entier(z.get("i"), -1) != 0 and not _entier(z.get("o"), 0) and _entier(z.get("f"), 0):
                sortie.append({"c": code, "l": str(z.get("l") or code), "a": _entier(z.get("a"), 0)})
        return sorted(sortie, key=lambda z: z["l"].lower())

    def attaques(self):
        """Zones attaquées (1 annoncée, 2 assaut) : [{"c", "a"}]."""
        return [{"c": code, "a": _entier(z.get("a"), 0)} for code, z in (self.etat.get("zones") or {}).items() if _entier(z.get("a"), 0) > 0]

    def pour_image(self):
        """Ce qu'il faut à trace_image.fabriquer_front : grille, noms des zones tenues ou au contact, segments déjà calculés."""
        if not self.a_front():
            return None
        e = self.etat
        d = self.derive()
        front, contours = self._traits()
        zones = []
        for code, z in (e.get("zones") or {}).items():
            calcul = d["zones"].get(code)
            if calcul:
                zones.append({"l": str(z.get("l") or code), "lx": calcul["lx"], "lz": calcul["lz"], "o": _entier(z.get("o"), 0),
                              "f": _entier(z.get("f"), 0)})
        return {"n": _entier(e.get("n"), 0), "k": _entier(e.get("k"), 0), "g": e.get("g") or "", "zones": zones,
                "front": front, "contours": contours, "rev": e.get("rev", 0)}

    def version_web(self):
        """« campagne:révision » (front_v de /carte/api/carte : la carte recharge le front quand il change), ou "" tant
        qu'aucun front n'est reçu."""
        if not self.a_front():
            return ""
        return "%s:%d" % (self.etat.get("campagne"), _entier(self.etat.get("rev"), 0))

    def signature_image(self):
        """Empreinte de ce que dessine l'image du front (trace_image.fabriquer_front) : grille, découpage, et par zone
        index, camp, contact et nom. Les paquets des dépôts, la coupure ou « depuis » font bouger rev, pas l'image : l'image
        du soir ne repart que si cette empreinte a changé."""
        e = self.etat
        zones = sorted((str(code), _entier(z.get("i"), -1), _entier(z.get("o"), 0), _entier(z.get("f"), 0), str(z.get("l") or code))
                       for code, z in (e.get("zones") or {}).items())
        texte = json.dumps([_entier(e.get("n"), 0), _entier(e.get("k"), 0), e.get("g") or "", e.get("zm") or "", zones], ensure_ascii=False)
        return hashlib.sha1(texte.encode("utf-8")).hexdigest()

    def prendre_victoire(self):
        """Vrai une seule fois après l'évènement victoire : l'image finale est à poster."""
        if self.victoire:
            self.victoire = False
            return True
        return False

    # ------------------------------------------------------------------------------------------ archives et fichiers
    def nom_libre(self, base):
        """Un nom d'archive libre (^[A-Za-z0-9-]{1,60}$), suffixé -2, -3… s'il est déjà pris."""
        base = re.sub(r"[^A-Za-z0-9-]", "-", str(base))[:54] or "archive"
        nom = base
        rang = 2
        while os.path.exists(self._chemin(os.path.join("archives", nom + ".json"))):
            nom = "%s-%d" % (base, rang)
            rang += 1
        return nom

    def archiver(self, now):
        """B3 : la campagne en cours part dans archives/front-<campagne>.json (frise, mouvements, zones et leur tracé pour
        rejouer la frise sur la carte), puis la frise et les mouvements repartent à zéro."""
        e = self.etat
        cid = str(e.get("campagne") or "")
        if not cid or not e.get("g"):
            return
        d = self.derive()
        nom = self.nom_libre("front-" + cid)
        numero = re.match(r"^C(\d+)$", cid)
        photos = self.frise.get("photos") or []
        debut = _entier(e.get("debut"), 0) or (photos[0].get("t", 0) if photos else int(now))
        zones = e.get("zones") or {}
        data = {"type": "front", "nom": nom, "titre": "Campagne %s" % (numero.group(1) if numero else cid), "debut": int(debut), "fin": int(now),
                "masque": self.frise.get("masque") or masque(e.get("g") or ""), "n": e.get("n"), "k": e.get("k"), "zm": e.get("zm", ""),
                "zones": {code: str(z.get("l") or code) for code, z in zones.items()},
                "idx": {code: _entier(z.get("i"), -1) for code, z in zones.items()},
                "pos": {code: [c["lx"], c["lz"]] for code, c in d["zones"].items()},
                "photos": photos, "mouvements": e.get("mouvements") or []}
        _ecrire_json(self._chemin(os.path.join("archives", nom + ".json")), data, compact=True)
        self.frise = {"campagne": "", "masque": "", "photos": []}
        e["mouvements"] = []
        self.attente = []
        self.sale.add("front")
        self.sale.add("frise")
        if self.quand_archive:
            self.quand_archive()

    def sauver(self):
        """Écrit front.json (compact) et frise.json quand ils ont changé."""
        if "front" in self.sale and self.etat:
            _ecrire_json(self._chemin("front.json"), self.etat, compact=True)
        if "frise" in self.sale:
            _ecrire_json(self._chemin("frise.json"), self.frise, compact=True)
        self.sale.clear()


class Campagne:
    def __init__(self, dossier, secret, reglages=None):
        self.dossier = dossier
        self.cle = str(secret or "pag").encode("utf-8")
        os.makedirs(os.path.join(dossier, "trajets"), exist_ok=True)
        os.makedirs(os.path.join(dossier, "archives"), exist_ok=True)
        self.soldats = self._lire("soldats.json", {})            # code -> fiche
        self.secteurs = self._lire("secteurs.json", {"etat": {}, "historique": []})
        self.missions = self._lire("missions.json", [])
        self.jour = ""
        self.trajets = {}                                          # code -> [[secondes depuis minuit, gx, gz], ...]
        self.dernier_carre = {}                                    # code -> (gx, gz)
        self.sale = set()
        self.prochaine_ecriture = 0
        self.dernier_menage = 0
        self.front = Front(dossier, reglages)                      # front.json, frise.json, archives du front
        self.front.quand_archive = self._oublier_archives
        self._archives = None                                      # liste des archives (en cache, refaite après une archive)
        self._histo_archive = None                                 # historique des secteurs archivé (K1), lu une fois

    # ------------------------------------------------------------------------------------------ fichiers
    def _chemin(self, nom):
        return os.path.join(self.dossier, nom)

    def _lire(self, nom, defaut):
        try:
            with open(self._chemin(nom), encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            return defaut

    def _ecrire(self, nom, data, compact=False):
        tmp = self._chemin(nom) + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            if compact:
                json.dump(data, f, ensure_ascii=False, separators=(",", ":"))
            else:
                json.dump(data, f, ensure_ascii=False, indent=1)
        os.replace(tmp, self._chemin(nom))

    def code(self, identite):
        return hmac.new(self.cle, str(identite).encode("utf-8"), hashlib.sha256).hexdigest()[:10]

    # ------------------------------------------------------------------------------------------ collecte
    def front_ingest(self, state, now=None):
        """Le bloc « front » de l'envoi (mod récent) : rend la ligne de réponse « #front … » pour le mod, ou "" (ancien
        mod, sans bloc). K1 : au premier front reçu, l'historique des secteurs est archivé une fois pour toutes."""
        now = now or time.time()
        ligne = self.front.ingest(state.get("front"), state.get("events") or [], now)
        if self.front.a_front() and not self.front.etat.get("secteurs_archives"):
            self.archiver_secteurs(now)
        return ligne

    def ingest(self, state, now=None):
        now = now or time.time()
        joueurs = [p for p in state.get("players", []) if p.get("identity")]
        presents = []                                              # (code, nom, carré)

        for p in joueurs:
            code = self.code(p["identity"])
            fiche = self.soldats.get(code)
            if not fiche:
                fiche = self.soldats[code] = {"premier": int(now)}
            nouveau = {"nom": str(p.get("name", "?"))[:32], "grade": int(p.get("grade", 0)), "grade_nom": str(p.get("gradeName", ""))[:24],
                       "certifs": str(p.get("certifs", ""))[:200], "instructeur": str(p.get("instructeur", ""))[:200],
                       "minutes": int(p.get("minutes", 0)), "missions": int(p.get("missions", 0)), "votes": int(p.get("votes", 0))}
            if any(fiche.get(k) != v for k, v in nouveau.items()):
                fiche.update(nouveau)
                self.sale.add("soldats")
            if now - fiche.get("dernier", 0) > 60:
                fiche["dernier"] = int(now)
                self.sale.add("soldats")
            carre = _carre(p.get("grid"))
            if carre:
                presents.append((code, nouveau["nom"], carre))

        # ancien mod : les secteurs, jusqu'à l'archive K1 ; après elle, seulement si un ancien mod est remis (retour arrière)
        if state.get("sectors") or not self.front.etat.get("secteurs_archives"):
            self._secteurs(state.get("sectors") or [], presents, now)
        self._trajets(presents, now)
        self._missions(state.get("events") or [], now)

        if self.sale and now >= self.prochaine_ecriture:
            self.sauver(now)

    def _secteurs(self, secteurs, presents, now):
        etat = self.secteurs["etat"]
        for s in secteurs:
            nom = str(s.get("name", ""))[:40]
            if not nom:
                continue
            nous = bool(s.get("ours"))
            connu = etat.get(nom)
            if connu is None:                                     # première fois qu'on le voit : l'état de départ, sans évènement
                etat[nom] = {"nous": nous, "carre": str(s.get("grid", ""))[:7], "depuis": int(now)}
                self.sale.add("secteurs")
                continue
            if connu["nous"] == nous:
                continue
            centre = _carre(s.get("grid"))
            autour = []
            if centre:
                autour = [{"code": c, "nom": n} for c, n, (gx, gz) in presents if abs(gx - centre[0]) <= AUTOUR and abs(gz - centre[1]) <= AUTOUR]
            self.secteurs["historique"].append({"t": int(now), "secteur": nom, "carre": str(s.get("grid", ""))[:7], "nous": nous, "soldats": autour})
            del self.secteurs["historique"][:-HISTORIQUE_MAX]
            etat[nom] = {"nous": nous, "carre": str(s.get("grid", ""))[:7], "depuis": int(now)}
            self.sale.add("secteurs")

    def _trajets(self, presents, now):
        if not presents:
            return                                                # serveur vide : rien à noter
        jour = _jour(now)
        if jour != self.jour:
            if self.jour and "trajets" in self.sale:
                self._ecrire_trajets()
            self.jour = jour
            self.trajets = self._lire(os.path.join("trajets", jour + ".json"), {})
            self.dernier_carre = {}
        import datetime
        if PARIS:
            d = datetime.datetime.fromtimestamp(now, PARIS)
        else:
            d = datetime.datetime.fromtimestamp(now)
        secondes = d.hour * 3600 + d.minute * 60 + d.second
        for code, _nom, carre in presents:
            if self.dernier_carre.get(code) == carre:
                continue
            self.dernier_carre[code] = carre
            points = self.trajets.setdefault(code, [])
            if len(points) < POINTS_MAX_PAR_JOUR:
                points.append([secondes, carre[0], carre[1]])
                self.sale.add("trajets")

    def _missions(self, events, now):
        for ev in events[:self.front.evenements_max]:            # tous les évènements de l'envoi (60 au plus)
            if not isinstance(ev, dict) or str(ev.get("type", "")) != "mission":
                continue
            self.missions.append({"t": int(now), "texte": str(ev.get("text", ""))[:400]})
            del self.missions[:-MISSIONS_MAX]
            self.sale.add("missions")

    def _ecrire_trajets(self):
        if self.jour:
            self._ecrire(os.path.join("trajets", self.jour + ".json"), self.trajets, compact=True)

    def sauver(self, now=None):
        now = now or time.time()
        if "soldats" in self.sale:
            self._ecrire("soldats.json", self.soldats)
        if "secteurs" in self.sale:
            self._ecrire("secteurs.json", self.secteurs)
        if "missions" in self.sale:
            self._ecrire("missions.json", self.missions)
        if "trajets" in self.sale:
            self._ecrire_trajets()
        self.sale.clear()
        self.front.traiter_attente(now)
        self.front.sauver()
        self.prochaine_ecriture = now + 60
        if now - self.dernier_menage > 6 * 3600:
            self.dernier_menage = now
            self._menage(now)

    def _menage(self, now):
        limite = _jour(now - GARDER_JOURS * 86400)
        dossier = os.path.join(self.dossier, "trajets")
        for nom in os.listdir(dossier):
            if nom.endswith(".json") and nom[:-5] < limite:
                try:
                    os.remove(os.path.join(dossier, nom))
                except OSError:
                    pass

    # ------------------------------------------------------------------------------------------ archives (K1, B3)
    def archiver_secteurs(self, now):
        """K1, une seule fois : l'historique des secteurs part dans archives/secteurs-<AAAA-MM-JJ>.json, puis repart vide.
        Rien n'est écrit pour un historique vide (serveur neuf) ; le drapeau secteurs_archives est posé dans les deux cas."""
        e = self.front.etat
        if e.get("secteurs_archives"):
            return
        etat = self.secteurs.get("etat") or {}
        historique = self.secteurs.get("historique") or []
        if etat or historique:
            temps = [h.get("t", 0) for h in historique if isinstance(h, dict) and h.get("t")]
            temps += [s.get("depuis", 0) for s in etat.values() if isinstance(s, dict) and s.get("depuis")]
            nom = self.front.nom_libre("secteurs-" + _jour(now))
            data = {"type": "secteurs", "nom": nom, "titre": "Campagne des secteurs", "debut": int(min(temps) if temps else now),
                    "fin": int(now), "etat": etat, "historique": historique}
            self._ecrire(os.path.join("archives", nom + ".json"), data, compact=True)
            e["secteurs_archive_nom"] = nom
            self._oublier_archives()
        self.secteurs = {"etat": {}, "historique": []}
        self.sale.add("secteurs")
        e["secteurs_archives"] = True
        self.front.sale.add("front")

    def _oublier_archives(self):
        self._archives = None

    def archives_liste(self):
        """Campagnes précédentes : [{"nom", "type", "titre", "debut", "fin", "n"}], la plus récente d'abord."""
        if self._archives is None:
            liste = []
            dossier = os.path.join(self.dossier, "archives")
            try:
                noms = os.listdir(dossier)
            except OSError:
                noms = []
            for fichier in noms:
                nom = fichier[:-5]
                if not fichier.endswith(".json") or not ARCHIVE_RE.match(nom):
                    continue
                data = self._lire(os.path.join("archives", fichier), None)
                if not isinstance(data, dict):
                    continue
                liste.append({"nom": nom, "type": str(data.get("type", "")), "titre": str(data.get("titre", nom))[:80],
                              "debut": _entier(data.get("debut"), 0), "fin": _entier(data.get("fin"), 0),
                              "n": len(data.get("mouvements") or data.get("historique") or [])})
            liste.sort(key=lambda a: (a["fin"], a["nom"]), reverse=True)
            self._archives = liste
        return self._archives

    def archive_chemin(self, nom):
        """Le fichier d'une archive, ou None si le nom est refusé ou inconnu."""
        nom = str(nom or "")
        if not ARCHIVE_RE.match(nom):
            return None
        chemin = self._chemin(os.path.join("archives", nom + ".json"))
        return chemin if os.path.isfile(chemin) else None

    def archive(self, nom):
        """Le contenu d'une archive, ou None si le nom ne respecte pas ^[A-Za-z0-9-]{1,60}$ ou n'existe pas."""
        chemin = self.archive_chemin(nom)
        if not chemin:
            return None
        data = _lire_json(chemin, None)
        return data if isinstance(data, dict) else None

    # ------------------------------------------------------------------------------------------ pour le site
    def historique_public(self):
        """/carte/api/historique (pages restées en cache) et page Campagne d'un ancien mod : l'historique des secteurs,
        lu dans son archive après K1 ; celui du moment s'il s'est rempli depuis (ancien mod remis après le front)."""
        nom = self.front.etat.get("secteurs_archive_nom")
        if self.front.etat.get("secteurs_archives") and nom and not self.secteurs.get("historique"):
            if self._histo_archive is None:
                data = self.archive(nom) or {}
                self._histo_archive = {"etat": data.get("etat") or {}, "historique": (data.get("historique") or [])[-500:], "archive": nom}
            return self._histo_archive
        return {"etat": self.secteurs["etat"], "historique": self.secteurs["historique"][-500:]}

    def trajets_entre(self, debut, fin):
        """Les trajets entre deux instants (secondes Unix) : {code: {"nom":…, "points": [[t Unix, gx, gz], …]}}. Trois jours au plus."""
        import datetime
        resultat = {}
        fin = min(fin, debut + 3 * 86400)
        t = debut
        jours = []
        while _jour(t) <= _jour(fin):
            jours.append(_jour(t))
            t += 86400
            if len(jours) > 4:
                break
        for jour in jours:
            data = self.trajets if jour == self.jour else self._lire(os.path.join("trajets", jour + ".json"), {})
            a, m, j = (int(x) for x in jour.split("-"))
            if PARIS:
                minuit = datetime.datetime(a, m, j, tzinfo=PARIS).timestamp()
            else:
                minuit = datetime.datetime(a, m, j).timestamp()
            for code, points in data.items():
                gardes = [[int(minuit + s), gx, gz] for s, gx, gz in points if debut <= minuit + s <= fin]
                if not gardes:
                    continue
                entree = resultat.setdefault(code, {"nom": self.soldats.get(code, {}).get("nom", "?"), "points": []})
                entree["points"].extend(gardes)
        return resultat

    def effectifs_public(self):
        lignes = [{"code": code, "nom": f.get("nom", "?"), "grade": f.get("grade", 0), "grade_nom": f.get("grade_nom", ""),
                   "minutes": f.get("minutes", 0), "missions": f.get("missions", 0), "votes": f.get("votes", 0), "dernier": f.get("dernier", 0)}
                  for code, f in self.soldats.items()]
        lignes.sort(key=lambda l: (-l["grade"], -l["minutes"], l["nom"].lower()))
        return lignes

    def fiche_publique(self, code):
        """La fiche publique d'un soldat. I9 : plus de « secteurs pris », la fiche ne dit plus rien du territoire."""
        f = self.soldats.get(str(code))
        if not f:
            return None
        return {"code": code, "nom": f.get("nom", "?"), "grade": f.get("grade", 0), "grade_nom": f.get("grade_nom", ""),
                "certifs": f.get("certifs", ""), "instructeur": f.get("instructeur", ""), "minutes": f.get("minutes", 0),
                "missions": f.get("missions", 0), "votes": f.get("votes", 0), "premier": f.get("premier", 0), "dernier": f.get("dernier", 0)}

    def missions_recentes(self):
        return list(reversed(self.missions[-10:]))
