# -*- coding: utf-8 -*-
# Produit « FGI - Guide de l'instructeur.pdf » : le document que l'instructeur FGI garde à côté de lui.
# Le contenu est dans CONTENU ci-dessous ; la mise en page reprend celle du guide des dotations (E:\Projets\PAG-Tenues).
import os, sys, io, html, json, base64, pathlib, subprocess, tempfile
from PIL import Image
sys.stdout.reconfigure(encoding='utf-8')
CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
ICI = pathlib.Path(__file__).resolve().parent
LOGO_SOURCE = pathlib.Path(r'E:\Projets\PAG-Tenues\logo.png')
SORTIE = ICI / "FGI - Guide de l'instructeur.pdf"
E = html.escape


def logo_uri(largeur):
    im = Image.open(LOGO_SOURCE)
    im = im.resize((largeur, round(im.height * largeur / im.width)), Image.LANCZOS)
    b = io.BytesIO()
    im.save(b, 'PNG', optimize=True)
    return 'data:image/png;base64,' + base64.b64encode(b.getvalue()).decode()


# Tout le contenu vient de fgi_contenu.json : c'est lui qu'on corrige, pour le PDF comme pour le site.
CONTENU = json.loads((ICI / "fgi_contenu.json").read_text(encoding="utf-8"))
MODULES = [dict(m, groupes=[(g["titre"], [(p["texte"], p["mot_pour_mot"]) for p in g["points"]]) for g in m["groupes"]],
                questions=[(x["q"], x["r"]) for x in m["questions"]]) for m in CONTENU["modules"]]
ELIMINATOIRES = CONTENU["eliminatoires"]


def liste_points(groupes):
    out, numero = [], 1
    for titre, points in groupes:
        out.append('<div class="groupe"><h4>%s</h4><ol class="points" start="%d">' % (E(titre), numero))
        for texte, exact in points:
            classe = ' class="exact"' if exact else ''
            texte = '« %s »' % texte if exact else texte
            out.append('<li%s>%s</li>' % (classe, E(texte)))
            numero += 1
        out.append('</ol></div>')
    return ''.join(out)


def module(m):
    out = ['<section class="module"><header><div class="pastille">%s</div><div><p class="eyebrow">Module %s · %d minutes · %s</p><h2>%s</h2></div></header>'
           % (E(m["n"]), E(m["n"]), m["duree"], E(m["lieu"]), E(m["titre"]))]
    out.append('<div class="objectif"><strong>Objectif</strong> %s</div>' % E(m["objectif"]))
    out.append('<div class="bloc-points"><h3>Points à aborder</h3><div class="points-cols">%s</div></div>' % liste_points(m["groupes"]))
    cases = []
    if m.get("montrer"):
        cases.append('<div class="bloc-montrer"><h3>À montrer</h3><ul>%s</ul></div>' % ''.join('<li>%s</li>' % E(x) for x in m["montrer"]))
    if m.get("faire"):
        cases.append('<div class="bloc-faire"><h3>À faire faire</h3><ul>%s</ul></div>' % ''.join('<li>%s</li>' % E(x) for x in m["faire"]))
    if m.get("questions"):
        cases.append('<div class="qr"><h3>Questions au choix · une par recrue</h3><ul>'
                     + ''.join('<li><p class="q">« %s »</p><p class="r">→ %s</p></li>' % (E(q), E(r)) for q, r in m["questions"])
                     + '</ul></div>')
    cases.append('<div class="pile"><div class="valider"><h3>À valider</h3><ul>%s</ul></div>'
                 '<div class="pieges"><h3>Pièges fréquents</h3><ul>%s</ul></div></div>'
                 % (''.join('<li><span class="case"></span>%s</li>' % E(v) for v in m["valider"]),
                    ''.join('<li>%s</li>' % E(p) for p in m["pieges"])))
    out.append('<div class="bandeau bandeau-%d">%s</div></section>' % (len(cases), ''.join(cases)))
    return ''.join(out)


total = sum(m["duree"] for m in MODULES) + 2
sommaire = ''.join('<li><span>%s</span>%s<em>%d min</em></li>' % (E(m["n"]), E(m["titre"]), m["duree"]) for m in MODULES) + '<li><span>V</span>Validation en jeu et suite du parcours<em>2 min</em></li>'

grille_lignes = ''.join('<tr><th>%s · %s</th>%s</tr>' % (E(m["n"]), E(m["titre"]), '<td></td>' * 4) for m in MODULES if m["n"] != "0")

CSS = '''
@page{size:338.667mm 190.5mm;margin:0}
:root{color-scheme:dark;--bg:#12150f;--panel:#1a1f17;--panel2:#20261c;--line:#2f3728;--ink:#e9ebe4;--mute:#97a08c;--acc:#c9a227;--dire:#c9a227;--montrer:#6fa3c7;--faire:#6fb27f;--rouge:#c0564b}
*{box-sizing:border-box}
html,body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 "Segoe UI",system-ui,sans-serif;-webkit-print-color-adjust:exact;print-color-adjust:exact}
.page{padding:7mm 10mm 5mm;break-after:page;height:190.5mm;position:relative;overflow:hidden}
.page:last-child{break-after:auto}
.pied{position:absolute;left:10mm;right:10mm;bottom:2mm;display:flex;justify-content:space-between;color:var(--mute);font-size:9px;letter-spacing:.08em;text-transform:uppercase;border-top:1px solid var(--line);padding-top:5px}
.eyebrow{margin:0;color:var(--acc);text-transform:uppercase;letter-spacing:.2em;font-size:12px;font-weight:600}
h1{font:700 44px/1.08 Georgia,"Times New Roman",serif;margin:6px 0 10px}
h2{font:700 24px/1.15 Georgia,"Times New Roman",serif;margin:1px 0 0}
h3{font:700 11px/1 "Segoe UI",sans-serif;letter-spacing:.12em;text-transform:uppercase;margin:0 0 6px;color:var(--acc)}
.lead{color:var(--mute);font-size:15px;max-width:70ch;margin:0 0 8px}
.garde{display:flex;flex-direction:column;justify-content:center}
.garde .logo{width:44mm;height:auto;margin:0 0 4mm}
.bref{display:grid;grid-template-columns:repeat(4,1fr);gap:8px;margin:10px 0 6px}
.bref div{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:13px 13px}
.bref strong{display:block;font:700 19px/1.15 Georgia,serif;color:var(--acc);white-space:nowrap}
.bref span{color:var(--mute);font-size:13px}
.garde-bas{display:grid;grid-template-columns:1fr 1.05fr;gap:13mm;align-items:start}
.sommaire{list-style:none;margin:0;padding:0;font-size:14.5px;column-count:2;column-gap:10mm}
.sommaire li{display:flex;gap:10px;padding:4px 0;border-bottom:1px dotted var(--line);break-inside:avoid}
.sommaire span{color:var(--acc);font-weight:700;width:22px}
.sommaire em{margin-left:auto;color:var(--mute);font-style:normal;font-variant-numeric:tabular-nums;white-space:nowrap}
.bloc{background:var(--panel);border:1px solid var(--line);border-left:3px solid var(--acc);border-radius:8px;padding:13px 17px;margin:0 0 11px;font-size:15.5px;line-height:1.35;break-inside:avoid}
.bloc.rouge{border-left-color:var(--rouge)}.bloc.rouge h3{color:var(--rouge)}
.bloc ul,.bloc ol{margin:0;padding-left:19px}.bloc li{margin:5px 0}
.deux{display:grid;grid-template-columns:1fr 1fr;gap:12px}
.colonnes-2{column-count:2;column-gap:9mm}
.colonnes-3{column-count:3;column-gap:7mm}
.colonnes-2 .bloc,.colonnes-3 .bloc{font-size:12.3px;padding:9px 13px;margin:0 0 7px;line-height:1.4}
.colonnes-2 .bloc li,.colonnes-3 .bloc li{margin:2px 0}
.aere .bloc{font-size:15px;padding:12px 16px;margin:0 0 10px;line-height:1.38}
.aere .bloc li{margin:4px 0}
.colonnes-3 .legende{font-size:12px;margin:5px 0 0}
.colonnes-2 .bloc,.colonnes-3 .bloc,.colonnes-2 .lead{break-inside:avoid}
.legende{display:flex;gap:10px;flex-wrap:wrap;margin:8px 0 0}
.module header{display:flex;gap:11px;align-items:center;margin-bottom:6px}
.pastille{width:40px;height:40px;border-radius:50%;background:var(--acc);color:var(--bg);font:700 20px/40px Georgia,serif;text-align:center;flex:none}
.fiche{display:block;margin-bottom:8px}
.fiche div{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:8px 13px}
.fiche strong{display:block;font-size:11px;letter-spacing:.14em;text-transform:uppercase;color:var(--acc);margin-bottom:3px}
.fiche span{font-size:13.5px}
.objectif{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:7px 14px;margin-bottom:7px;font-size:14.5px}
.objectif strong{color:var(--acc);font-size:10.5px;letter-spacing:.12em;text-transform:uppercase;margin-right:8px}
.points-cols{column-count:3;column-gap:6mm}
.groupe{break-inside:avoid-column}
.groupe h4{break-after:avoid}
.bandeau{display:grid;gap:3.5mm;align-items:start;margin-top:5px}
.bandeau-4{grid-template-columns:repeat(4,1fr)}
.bandeau-3{grid-template-columns:repeat(3,1fr)}
.bandeau-2{grid-template-columns:repeat(2,1fr)}
.bandeau-1{grid-template-columns:1fr}
.pile{min-width:0}
.bloc-points{background:#241d0c;border:1px solid var(--line);border-left:6px solid var(--dire);border-radius:8px;padding:10px 16px 9px;margin-bottom:0}
.groupe{margin:0}
.groupe h4{font:700 11px/1.2 "Segoe UI",sans-serif;letter-spacing:.09em;text-transform:uppercase;color:#dcc06a;margin:7px 0 3px;padding-top:5px;border-top:1px solid rgba(201,162,39,.28)}
.groupe:first-child h4{margin-top:0;padding-top:0;border-top:0}
.points{margin:0;padding-left:17px;font-size:14.5px;line-height:1.31}
.points li{margin:3px 0;padding-left:2px}
.points li::marker{color:var(--dire);font-weight:700}
.points li.exact{list-style:none;margin:4px 0 4px -15px;padding:3px 9px;background:rgba(201,162,39,.16);border-left:3px solid var(--dire);border-radius:4px;font-style:italic}
.bloc-montrer,.bloc-faire{border:1px solid var(--line);border-radius:8px;padding:10px 14px 9px;margin:0}
.bloc-montrer{background:#111e26;border-left:7px solid var(--montrer)}
.bloc-montrer h3{color:var(--montrer)}
.bloc-faire{background:#0f2118;border-left:7px solid var(--faire)}
.bloc-faire h3{color:var(--faire)}
.bloc-montrer ul,.bloc-faire ul{margin:0;padding-left:17px;font-size:14px;line-height:1.3}
.bloc-montrer li,.bloc-faire li{margin:3px 0}
.etapes{list-style:none;margin:0;padding:0}
.etape{display:grid;grid-template-columns:30px 132px 1fr;gap:12px;align-items:start;background:var(--panel);border:1px solid var(--line);border-left:7px solid var(--line);border-radius:8px;padding:9px 16px;margin-bottom:7px;break-inside:avoid}
.etape p{margin:0;font-size:16px}
.etape .num{font:700 20px/1.3 Georgia,serif;color:var(--mute)}
.tag{display:block;font:800 11.5px/1 "Segoe UI",sans-serif;letter-spacing:.12em;text-transform:uppercase;border:0;border-radius:4px;padding:7px 8px;text-align:center;margin-top:1px;color:#12150f}
.etape.dire{background:#241d0c;border-left-color:var(--dire)}.dire .tag,.tag.dire{background:var(--dire)}
.etape.dire p{font-style:italic;font-size:16.5px}
.etape.montrer{background:#111e26;border-left-color:var(--montrer)}.montrer .tag,.tag.montrer{background:var(--montrer)}
.etape.faire{background:#0f2118;border-left-color:var(--faire)}.faire .tag,.tag.faire{background:var(--faire)}
.qr{background:var(--panel2);border:1px solid var(--line);border-left:5px solid var(--montrer);border-radius:8px;padding:10px 14px;margin:0}
.qr ul{list-style:none;margin:0;padding:0}
.qr li{margin:3px 0}
.qr .q{margin:0;font-size:13.4px;font-weight:600;line-height:1.24}
.qr .r{margin:2px 0 0;font-size:13.4px;color:var(--faire);line-height:1.24}
.valider,.pieges{background:var(--panel2);border:1px solid var(--line);border-radius:8px;padding:9px 13px;margin:0 0 3mm}
.pile .pieges{margin-bottom:0}
.pieges h3{color:var(--rouge)}
.valider ul{list-style:none;margin:0;padding:0}.valider li{display:flex;gap:8px;margin:3px 0;font-size:14px;line-height:1.28}
.case{width:15px;height:15px;border:2px solid var(--acc);border-radius:3px;flex:none;margin-top:2px}
.pieges ul{margin:0;padding-left:16px;font-size:14px;color:var(--ink);line-height:1.28}.pieges li{margin:3px 0}
table{width:100%;border-collapse:collapse;font-size:15px}
th,td{border:1px solid var(--line);padding:11px 12px;text-align:left}
thead th{background:var(--panel2);color:var(--acc);font-size:12px;letter-spacing:.1em;text-transform:uppercase}
tbody th{font-weight:500;width:46%}td{width:13.5%}
.ecrire{border-bottom:1px solid var(--mute);display:inline-block;min-width:60mm;height:20px}
kbd{background:var(--panel2);border:1px solid var(--line);border-bottom-width:3px;border-radius:5px;padding:2px 8px;font:600 14px "Segoe UI",sans-serif;color:var(--acc)}
.imprevus{column-count:5;column-gap:5mm;margin-top:4px}
.imp-cat h3{font:700 11px/1.2 "Segoe UI",sans-serif;letter-spacing:.12em;text-transform:uppercase;color:var(--acc);margin:0 0 4px;break-after:avoid}
.imp-cat{margin-bottom:6px}
.imp{break-inside:avoid;background:var(--panel);border:1px solid var(--line);border-left:3px solid var(--rouge);border-radius:6px;padding:4px 8px;margin:0 0 4px}
.imp b{display:block;font-size:12.2px;line-height:1.22;margin-bottom:2px}
.imp ul{margin:0;padding-left:13px;font-size:11px;line-height:1.23;color:var(--ink)}
.imp li{margin:1px 0}
.memo{display:grid;grid-template-columns:repeat(4,1fr);gap:11px}
.memo .bloc{margin:0;font-size:14.6px}
.memo .bloc li{margin:4px 0}
.memo .bloc{margin:0}
'''


def pied(n):
    return '<div class="pied"><span>PAG · FGI · Guide de l\'instructeur · version interactive : penguinassaultgroup.fr/instructeur</span><span>%02d</span></div>' % n


pages = []
pages.append('<div class="page garde"><img class="logo" alt="" src="@@LOGO@@"><p class="eyebrow">Penguin Assault Group · milsim semi-RP armée française</p>'
             '<h1>Formation Générale Initiale<br>Guide de l\'instructeur</h1>'
             '<div class="garde-bas"><div><p class="lead">Le document à garder à côté de toi pendant la séance : les points à aborder, ce que tu montres, ce que tu fais faire, et ce que tu observes avant de passer à la suite. Tu parles avec tes mots ; seules les formules en italique se disent mot pour mot.</p>'
             '<div class="bref"><div><strong>%d min</strong><span>durée de la séance</span></div><div><strong>2 à 4</strong><span>recrues, chacune passe</span></div>'
             '<div><strong>7 modules</strong><span>plus la patrouille</span></div><div><strong>Levie</strong><span>tout se fait à la base</span></div></div>'
             '</div><ul class="sommaire">%s</ul></div>%s</div>' % (total, sommaire, pied(1)))

pages.append('<div class="page"><p class="eyebrow">Avant la séance</p><h2>Préparer ta FGI</h2><p class="lead" style="margin-top:10px">Dix minutes de préparation t\'évitent une heure de flottement.</p>'
             '<div class="colonnes-3"><div class="bloc"><h3>Dix minutes avant</h3><ul>' + ''.join('<li>%s</li>' % x["html"] for x in CONTENU["checklist"]) + '</ul></div>'
             '<div class="bloc"><h3>Comment lire ce guide</h3><p style="margin:0 0 10px">Chaque module tient sur une page. Les étapes se suivent dans l\'ordre, et chacune porte une étiquette :</p>'
             '<div class="legende"><span class="tag dire">Points à aborder</span> encadré doré : les points que tu traites, dans l\'ordre, avec TES mots. Ce ne sont pas des phrases à lire.</div>'
             '<div class="legende"><span class="tag montrer">À montrer</span> encadré bleu : les démonstrations que tu fais toi-même, devant eux.</div>'
             '<div class="legende"><span class="tag faire">À faire faire</span> encadré vert : ce que les recrues exécutent pendant que tu observes et corriges.</div>'
             '<p style="margin:10px 0 0">Une formule en italique sur fond doré se dit <strong>mot pour mot</strong> : il n\'y en a qu\'une ou deux par module.</p>'
             '<p style="margin:12px 0 0">En bas de page : ce que tu <strong>observes</strong> avant de passer au module suivant, et les <strong>pièges fréquents</strong>. Une case non cochée ne recale personne : elle se dit au débriefing et se transmet au chef de groupe.</p></div>'
             '<div class="bloc rouge"><h3>Fautes éliminatoires : la séance s\'arrête pour la recrue</h3><ul>' + ''.join('<li>%s</li>' % E(x) for x in ELIMINATOIRES) + '</ul>'
             '<p style="margin:8px 0 0;color:var(--mute)">C\'est le seul cas où la FGI n\'est pas accordée : tu préviens un officier ou le Staff, et la recrue pourra se représenter plus tard.</p></div>'
             '<div class="bloc"><h3>Ta posture d\'instructeur</h3><ul>' + ''.join('<li>%s</li>' % x for x in CONTENU["preparation"]["posture"]) + '</ul></div></div>' + pied(2) + '</div>')

for i, m in enumerate(MODULES):
    pages.append('<div class="page">%s%s</div>' % (module(m), pied(3 + i)))

n = 3 + len(MODULES)
pages.append('<div class="page"><section class="module"><header><div class="pastille">V</div><div><p class="eyebrow">Validation · 2 minutes</p><h2>Accorder la FGI en jeu, et après</h2></div></header>'
             '<div class="colonnes-2 aere"><div class="bloc"><h3>Accorder la certification</h3><ol>' + ''.join('<li>%s</li>' % x for x in CONTENU["validation"]["accorder"]) + '</ol></div>'
             '<div class="bloc"><h3>Ce qui reste fragile</h3><ul>' + ''.join('<li>%s</li>' % x for x in CONTENU["validation"]["fragile"]) + '</ul></div>'
             '<div class="bloc"><h3>Ce que tu dis au nouveau soldat</h3><ul>' + ''.join('<li>%s</li>' % x for x in CONTENU["validation"]["dire_au_soldat"]) + '</ul></div>'
             '<div class="bloc"><h3>Après la séance</h3><ul>' + ''.join('<li>%s</li>' % x for x in CONTENU["validation"]["apres"]) + '</ul></div></div></section>' + pied(n))

def page_imprevus():
    cats = []
    for x in CONTENU['imprevus']:
        if x['categorie'] not in cats:
            cats.append(x['categorie'])
    out = []
    for c in cats:
        out.append('<div class="imp-cat"><h3>' + E(c) + '</h3>')
        for x in CONTENU['imprevus']:
            if x['categorie'] == c:
                out.append('<div class="imp"><b>' + E(x['situation']) + '</b><ul>' + ''.join('<li>' + E(l) + '</li>' for l in x['conduite']) + '</ul></div>')
        out.append('</div>')
    return ('<div class="page"><p class="eyebrow">Imprévus</p><h2>Que faire si…</h2><div class="imprevus">' + ''.join(out) + '</div>'
            + pied(n + 1) + '</div>')


pages.append(page_imprevus())
n += 1


def bloc_memo(b):
    balise = 'ol' if b['ordonnee'] else 'ul'
    return '<div class="bloc"><h3>' + E(b['titre']) + '</h3><' + balise + '>' + ''.join('<li>' + x + '</li>' for x in b['items']) + '</' + balise + '></div>'


pages.append('<div class="page"><p class="eyebrow">Aide-mémoire</p><h2>Tout ce qu\'on te demandera, sur une page</h2><div style="height:8px"></div><div class="memo">'
             + ''.join(bloc_memo(b) for b in CONTENU['aide_memoire']) + '</div>' + pied(n + 1) + '</div>')

doc = '<!doctype html><html lang="fr"><head><meta charset="utf-8"><title>FGI - Guide de l\'instructeur</title><style>' + CSS + '</style></head><body>' + ''.join(pages) + '</body></html>'
doc = doc.replace('@@LOGO@@', logo_uri(640))
(ICI / 'fgi_instructeur.html').write_text(doc, encoding='utf-8')
tmp = ICI / '_pdf.html'
tmp.write_text(doc, encoding='utf-8')
if SORTIE.exists():
    SORTIE.unlink()
subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--no-first-run', '--no-pdf-header-footer',
                '--user-data-dir=' + os.path.join(tempfile.gettempdir(), 'pag-chrome-capture'),
                '--virtual-time-budget=5000', '--print-to-pdf=' + str(SORTIE), tmp.as_uri()], capture_output=True, timeout=180)
tmp.unlink()
print('ok' if SORTIE.exists() else 'ÉCHEC', SORTIE.name, (SORTIE.stat().st_size // 1024 if SORTIE.exists() else 0), 'Ko,', total, 'min,', len(pages), 'pages prévues')
