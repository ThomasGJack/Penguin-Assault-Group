# -*- coding: utf-8 -*-
"""PAG-Bot — le tableau de suivi en ligne de commande (sur le VPS, sous l'utilisateur du bot).

C'est la main de Claude sur le tableau : quand Jack lui demande d'ajouter, de détailler ou de déplacer des cartes, il
passe par ici. Le bot relit le fichier dès qu'il change : rien à redémarrer. Publier une mise à jour reste une action de
Jack sur Discord (/suivi publier), parce qu'elle poste une annonce.

  python suivi_cli.py liste
  python suivi_cli.py lot < operations.json

operations.json est une liste d'opérations, appliquées dans l'ordre :
  {"op": "ajouter",   "titre": "…", "colonne": "idee", "detail": "…"}
  {"op": "deplacer",  "id": 3, "colonne": "en_cours"}
  {"op": "modifier",  "id": 3, "titre": "…", "detail": "…"}        (titre et detail facultatifs)
  {"op": "supprimer", "id": 3}
Colonnes : idee, a_faire, a_developper, en_cours, a_tester, termine.
"""
import json
import os
import sys

import suivi

ICI = os.path.dirname(os.path.abspath(__file__))


def main():
    tableau = suivi.Tableau(os.path.join(ICI, "suivi.json"))
    commande = sys.argv[1] if len(sys.argv) > 1 else "liste"

    if commande == "lot":
        operations = json.loads(sys.stdin.buffer.read().decode("utf-8"))
        for op in operations:
            nom = op.get("op")
            if nom == "ajouter":
                carte = tableau.ajouter(op["titre"], op.get("colonne", "idee"), op.get("detail", ""))
            elif nom == "deplacer":
                carte = tableau.deplacer(op["id"], op["colonne"])
            elif nom == "modifier":
                carte = tableau.modifier(op["id"], op.get("titre"), op.get("detail"))
            elif nom == "supprimer":
                carte = tableau.supprimer(op["id"])
            else:
                print("opération inconnue :", nom)
                continue
            if carte:
                print("%s #%d · %s · %s" % (nom, carte["id"], suivi.NOMS.get(carte["colonne"], "?"), carte["titre"]))
            else:
                print("%s : carte ou colonne introuvable (%s)" % (nom, json.dumps(op, ensure_ascii=False)))

    for cle, nom in suivi.COLONNES:
        cartes = tableau.cartes(cle)
        print("%s (%d)" % (nom, len(cartes)))
        for c in cartes:
            print("   #%d  %s%s" % (c["id"], c["titre"], "  [détaillée]" if c.get("detail") else ""))
    print("Historique : %d mise(s) à jour" % len(tableau.data["versions"]))


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8")
    main()
