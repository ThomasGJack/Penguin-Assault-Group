# -*- coding: utf-8 -*-
"""Met en ligne le guide des dotations : prend le guide fabriqué dans E:\\Projets\\PAG-Tenues (guide.html, qui contient ses
images) et lui ajoute la barre du site. À relancer après chaque régénération du guide, puis renvoyer site_web/ au VPS.

  python fabriquer_dotations.py [dossier PAG-Tenues]
"""
import io
import os
import shutil
import sys

ICI = os.path.dirname(os.path.abspath(__file__))
SOURCE = sys.argv[1] if len(sys.argv) > 1 else r"E:\Projets\PAG-Tenues"

BARRE_CSS = """
.pag-barre{position:sticky;top:0;z-index:50;display:flex;flex-wrap:wrap;align-items:center;gap:8px 12px;padding:10px 22px;background:#0c1116f2;border-bottom:1px solid #24313d;backdrop-filter:blur(6px);font:14px/1.4 "Segoe UI",system-ui,sans-serif}
.pag-barre img{width:32px;height:32px}
.pag-barre b{letter-spacing:.08em;text-transform:uppercase;font-family:Bahnschrift,"Segoe UI",sans-serif;color:#d7e0e8}
.pag-barre span{margin-left:auto;display:flex;flex-wrap:wrap;gap:8px}
.pag-barre a{color:#d7e0e8;text-decoration:none;border:1px solid #24313d;border-radius:999px;padding:4px 13px;font-size:13px;background:none}
.pag-barre a:hover{border-color:#c9a227;color:#c9a227}
.pag-barre a.fort{border-color:#c9a227;color:#c9a227}
[id]{scroll-margin-top:64px}
@media print{.pag-barre{display:none}}
"""
BARRE = ('<div class="pag-barre"><img src="/site/favicon.png" alt=""><b>PAG · Dotations</b><span>'
         '<a href="/">← Accueil</a><a href="/certifications/">Certifications</a><a href="/effectifs/">Effectifs</a>'
         '<a class="fort" href="/site/Dotations_PAG.pdf" target="_blank" rel="noopener">Télécharger le PDF</a></span></div>')


def main():
    page = io.open(os.path.join(SOURCE, "guide.html"), encoding="utf-8").read()
    assert "</style></head><body>" in page, "gabarit du guide changé : adapter fabriquer_dotations.py"
    page = page.replace("</style></head><body>", BARRE_CSS + '</style><link rel="icon" type="image/png" href="/site/favicon.png"></head><body>' + BARRE, 1)
    page = page.replace("<title>Guide d'habillement PAG</title>", "<title>PAG · Dotations et habillement</title>", 1)
    # Les liens venus de la page des certifications visent une fiche précise (#a4…) : toutes les images sont chargées d'emblée,
    # puis on se replace sur la fiche une fois la page complète (sinon elles décalent tout en arrivant)
    page = page.replace(' loading="lazy"', "")
    page = page.replace("</body>", "<script>addEventListener('load',()=>{if(location.hash){const e=document.getElementById(location.hash.slice(1));if(e)e.scrollIntoView();}});</script></body>", 1)
    io.open(os.path.join(ICI, "dotations.html"), "w", encoding="utf-8", newline="\n").write(page)
    pdf = os.path.join(SOURCE, "Dotations PAG.pdf")
    if os.path.isfile(pdf):
        shutil.copyfile(pdf, os.path.join(ICI, "Dotations_PAG.pdf"))
    print("dotations.html :", os.path.getsize(os.path.join(ICI, "dotations.html")) // 1024, "Ko ; PDF copié :", os.path.isfile(pdf))


if __name__ == "__main__":
    main()
