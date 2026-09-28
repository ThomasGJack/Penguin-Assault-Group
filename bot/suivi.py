# -*- coding: utf-8 -*-
"""PAG-Bot — tableau de suivi du développement (une sorte de Trello maison).

Module sans Discord : les données et leurs règles. Le bot s'en sert pour ses commandes /suivi, et la page web /suivi/
affiche ce que rend `public()`.

Six colonnes, dans l'ordre du travail : Idée, À faire, À développer, En cours, À tester, Terminé. Quand une mise à jour
est publiée, toutes les cartes « Terminé » quittent le tableau et entrent dans l'historique, sous un numéro de version
et une date : l'historique est le journal des mises à jour.
"""
import json
import os
import time

COLONNES = [
    ("idee", "💡 Idée"),
    ("a_faire", "📋 À faire"),
    ("a_developper", "🛠️ À développer"),
    ("en_cours", "⚙️ En cours"),
    ("a_tester", "🧪 À tester"),
    ("termine", "✅ Terminé"),
]
CLES = [cle for cle, _ in COLONNES]
NOMS = dict(COLONNES)
TITRE_MAX, DETAIL_MAX, NOTES_MAX, VERSION_MAX = 120, 4000, 1500, 30


try:                                    # l'heure de Paris, quel que soit le fuseau de la machine (le VPS est en UTC)
    from zoneinfo import ZoneInfo
    PARIS = ZoneInfo("Europe/Paris")
except Exception:                       # noqa: BLE001 — pas de base de fuseaux (Windows sans tzdata) : heure locale
    PARIS = None


def maintenant():
    if PARIS:
        import datetime
        return datetime.datetime.now(PARIS).strftime("%Y-%m-%d %H:%M")
    return time.strftime("%Y-%m-%d %H:%M", time.localtime())


class Tableau:
    def __init__(self, chemin):
        self.chemin = chemin
        self.data = {"suivant": 1, "cartes": [], "versions": []}
        self.lu = None          # date du fichier au dernier chargement
        self.relire()

    def relire(self):
        """Recharge le fichier s'il a changé sur le disque. Le tableau a deux mains : le bot Discord, et l'outil en ligne
        de commande suivi_cli.py (utilisé par Claude à la demande de Jack). Chacun relit avant d'agir et écrit d'un
        seul geste (os.replace), donc l'un ne défait pas le travail de l'autre."""
        try:
            date = os.path.getmtime(self.chemin)
        except OSError:
            return
        if date == self.lu:
            return
        try:
            with open(self.chemin, encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            return          # fichier illisible ou en cours d'écriture : on garde ce qu'on a en mémoire
        self.data = {"suivant": 1, "cartes": [], "versions": []}
        self.data.update(data)
        self.lu = date

    # -------------------------------------------------------------------------------------------- écriture
    def sauver(self):
        tmp = self.chemin + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(self.data, f, indent=1, ensure_ascii=False)
        os.replace(tmp, self.chemin)
        self.lu = os.path.getmtime(self.chemin)

    # -------------------------------------------------------------------------------------------- lecture
    def cartes(self, colonne=None):
        self.relire()
        cartes = self.data["cartes"]
        if colonne:
            cartes = [c for c in cartes if c["colonne"] == colonne]
        return sorted(cartes, key=lambda c: (CLES.index(c["colonne"]), c["id"]))

    def trouver(self, numero):
        self.relire()
        for carte in self.data["cartes"]:
            if carte["id"] == int(numero):
                return carte
        return None

    # -------------------------------------------------------------------------------------------- actions
    def ajouter(self, titre, colonne="idee", detail=""):
        self.relire()
        titre = " ".join(str(titre).split())[:TITRE_MAX]
        if not titre:
            raise ValueError("Il faut un titre")
        if colonne not in CLES:
            colonne = "idee"
        carte = {"id": self.data["suivant"], "titre": titre, "detail": str(detail).strip()[:DETAIL_MAX], "colonne": colonne,
                 "cree": maintenant(), "modifie": maintenant()}
        self.data["suivant"] += 1
        self.data["cartes"].append(carte)
        self.sauver()
        return carte

    def deplacer(self, numero, colonne):
        carte = self.trouver(numero)
        if not carte or colonne not in CLES:
            return None
        carte["colonne"] = colonne
        carte["modifie"] = maintenant()
        if colonne == "termine":
            carte["termine"] = maintenant()
        self.sauver()
        return carte

    def decaler(self, numero, pas):
        """Avance (+1) ou recule (-1) d'une colonne."""
        carte = self.trouver(numero)
        if not carte:
            return None
        index = max(0, min(len(CLES) - 1, CLES.index(carte["colonne"]) + pas))
        return self.deplacer(numero, CLES[index])

    def modifier(self, numero, titre=None, detail=None):
        carte = self.trouver(numero)
        if not carte:
            return None
        if titre is not None and " ".join(str(titre).split()):
            carte["titre"] = " ".join(str(titre).split())[:TITRE_MAX]
        if detail is not None:
            carte["detail"] = str(detail).strip()[:DETAIL_MAX]
        carte["modifie"] = maintenant()
        self.sauver()
        return carte

    def supprimer(self, numero):
        carte = self.trouver(numero)
        if not carte:
            return None
        self.data["cartes"].remove(carte)
        self.sauver()
        return carte

    def publier(self, version, notes=""):
        """Les cartes « Terminé » entrent dans l'historique sous ce numéro de version. None s'il n'y en a aucune."""
        terminees = self.cartes("termine")
        if not terminees:
            return None
        version = " ".join(str(version).split())[:VERSION_MAX] or maintenant()
        entree = {"version": version, "date": maintenant(), "notes": str(notes).strip()[:NOTES_MAX],
                  "cartes": [{"id": c["id"], "titre": c["titre"], "detail": c.get("detail", "")} for c in terminees]}
        self.data["versions"].insert(0, entree)
        self.data["cartes"] = [c for c in self.data["cartes"] if c["colonne"] != "termine"]
        self.sauver()
        return entree

    # -------------------------------------------------------------------------------------------- pour la page web
    def public(self):
        self.relire()
        return {"colonnes": [{"cle": cle, "nom": nom, "cartes": [{"id": c["id"], "titre": c["titre"], "detail": c.get("detail", ""),
                                                                   "cree": c.get("cree", ""), "modifie": c.get("modifie", "")} for c in self.cartes(cle)]}
                             for cle, nom in COLONNES],
                "versions": self.data["versions"][:100]}
