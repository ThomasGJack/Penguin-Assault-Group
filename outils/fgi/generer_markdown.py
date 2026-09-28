# -*- coding: utf-8 -*-
# Plan de la FGI en Markdown, tiré de fgi_contenu.json (même source que le PDF et le site) :
#   C:\Users\goule\Downloads\Plan_FGI_v2.md
import re, json, pathlib

ICI = pathlib.Path(__file__).resolve().parent
c = json.loads((ICI / "fgi_contenu.json").read_text(encoding="utf-8"))
SORTIE = pathlib.Path(r"C:\Users\goule\Downloads\Plan_FGI_v2.md")
nu = lambda s: re.sub(r"<[^>]+>", "", s)
total = sum(m["duree"] for m in c["modules"]) + 2

L = ["# FGI — Formation Générale Initiale de la PAG", "",
     "Plan de référence, généré depuis `fgi_contenu.json` (version du %s). Le PDF de l'instructeur et le site "
     "https://penguinassaultgroup.fr/instructeur/ sortent de la même source : on corrige le JSON, pas ce fichier." % c.get("version", "—"), "",
     "**%d minutes · 2 à 4 recrues · un instructeur habilité FGI, seul · tout à la base de Levie**" % total, "",
     "| Module | Titre | Durée | Lieu |", "|---|---|---|---|"]
for m in c["modules"]:
    L.append("| %s | %s | %d min | %s |" % (m["n"], m["titre"], m["duree"], m["lieu"]))
L.append("| V | Validation en jeu et suite du parcours | 2 min | Là où finit la patrouille |")
L += ["", "## Avant la séance", ""] + ["- [ ] " + nu(x["html"]) for x in c["checklist"]]
L += ["", "## Fautes éliminatoires", ""] + ["- " + x for x in c["eliminatoires"]]
for m in c["modules"]:
    L += ["", "## Module %s — %s (%d min, %s)" % (m["n"], m["titre"], m["duree"], m["lieu"]), "", "**Objectif** : " + m["objectif"], ""]
    L.append("### Déroulé")
    k = 0
    for e in m.get("etapes", []):
        if not e.get("points"):
            continue
        k += 1
        L.append("%d. **%s** — %s" % (k, {"dire": "À aborder", "montrer": "À montrer", "faire": "À faire faire", "deplacement": "Déplacement"}[e["type"]], e["titre"]))
        L += ["   - " + p for p in e["points"]]
        if e.get("mot_pour_mot"):
            L.append("   - *Mot pour mot : « %s »*" % e["mot_pour_mot"])
    if m["questions"]:
        L += ["", "### Questions (une par recrue)"] + ["- « %s » → %s" % (q["q"], q["r"]) for q in m["questions"]]
    L += ["", "### À valider"] + ["- [ ] " + v for v in m["valider"]]
    L += ["", "### Pièges"] + ["- " + p for p in m["pieges"]]
v = c["validation"]
L += ["", "## V — Validation en jeu"]
for e in v["ecrans"]:
    if e.get("annonce"):
        L += ["", "### Annonce des promotions (Discord)", "", "> " + c["annonce_promotions"]]
    else:
        L += ["", "### " + e["titre"]] + ["- " + nu(x) for x in e["html"]]
L += ["", "## Imprévus : que faire si…"]
cats = []
for x in c["imprevus"]:
    if x["categorie"] not in cats:
        cats.append(x["categorie"])
for cat in cats:
    L += ["", "### " + cat]
    for x in c["imprevus"]:
        if x["categorie"] == cat:
            L.append("- **%s** : %s" % (x["situation"], " ".join(x["conduite"])))
SORTIE.write_text("\n".join(L) + "\n", encoding="utf-8")
print("ok", SORTIE, len(L), "lignes")
