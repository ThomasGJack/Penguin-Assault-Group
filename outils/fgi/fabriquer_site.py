# -*- coding: utf-8 -*-
# Fabrique la page interactive de l'instructeur FGI à partir de fgi_contenu.json (la même source que le PDF).
#   sortie : E:\Projets\PAG-Bot\site_web\instructeur.html (servie en /instructeur/) + FGI_Guide_instructeur.pdf (servi en /site/)
#   --apercu : copie aussi le tout dans un dossier d'aperçu local (page à la racine, fichiers sous /site/)
import sys, json, shutil, pathlib

ICI = pathlib.Path(__file__).resolve().parent
SITE = pathlib.Path(r"E:\Projets\PAG-Bot\site_web")
contenu = json.loads((ICI / "fgi_contenu.json").read_text(encoding="utf-8"))

# Contrôles simples avant de publier : un contenu incomplet casserait la séance en direct.
erreurs = []
for m in contenu["modules"]:
    for cle in ("n", "titre", "duree", "lieu", "objectif", "groupes", "valider", "pieges"):
        if not m.get(cle) and m.get(cle) != 0:
            erreurs.append("module %s : %s manquant" % (m.get("n"), cle))
    for e in m.get("etapes", []):
        if e.get("type") in ("questions", "controle"):      # repères : l'écran automatique se place ici
            continue
        if e.get("type") not in ("dire", "montrer", "faire", "deplacement"):
            erreurs.append("module %s : type d'étape inconnu %r" % (m["n"], e.get("type")))
        if not e.get("points") and not e.get("mot_pour_mot"):
            erreurs.append("module %s : étape %r vide" % (m["n"], e.get("titre")))
ids = [x["id"] for x in contenu.get("imprevus", [])]
if len(ids) != len(set(ids)):
    erreurs.append("identifiants d'imprévus en double")
if erreurs:
    print("CONTENU INCOMPLET :\n  " + "\n  ".join(erreurs))
    sys.exit(1)

# Les « sources » des étapes (renvois aux points du PDF) servent à la relecture, pas à la page.
for m in contenu["modules"]:
    for e in m.get("etapes", []):
        e.pop("sources", None)

gabarit = (ICI / "site" / "instructeur.template.html").read_text(encoding="utf-8")
donnees = json.dumps(contenu, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
page = gabarit.replace("/*@@CONTENU@@*/null", donnees)
assert page != gabarit, "marqueur de contenu introuvable dans le gabarit"

(SITE / "instructeur.html").write_text(page, encoding="utf-8")
pdf = ICI / "FGI - Guide de l'instructeur.pdf"
shutil.copyfile(pdf, SITE / "FGI_Guide_instructeur.pdf")
print("ok", SITE / "instructeur.html", len(page) // 1024, "Ko ;", "PDF copié")

if "--apercu" in sys.argv:
    dossier = pathlib.Path(sys.argv[sys.argv.index("--apercu") + 1])
    (dossier / "site").mkdir(parents=True, exist_ok=True)
    (dossier / "index.html").write_text(page, encoding="utf-8")
    for f in ("favicon.png", "FGI_Guide_instructeur.pdf"):
        shutil.copyfile(SITE / f, dossier / "site" / f)
    print("aperçu :", dossier)
