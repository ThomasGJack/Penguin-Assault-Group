# -*- coding: utf-8 -*-
# Produit « Dotations PAG.pdf » à partir de guide.html : page de garde, puis une partie par page.
import re, os, sys, subprocess, tempfile, pathlib
sys.stdout.reconfigure(encoding='utf-8')
CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
ICI = pathlib.Path(__file__).resolve().parent
SORTIE = ICI / 'Dotations PAG.pdf'
import base64
from PIL import Image
import io as _io


def logo_uri(largeur):
    im = Image.open(ICI / 'logo.png')
    im = im.resize((largeur, round(im.height * largeur / im.width)), Image.LANCZOS)
    b = _io.BytesIO()
    im.save(b, 'PNG', optimize=True)
    return 'data:image/png;base64,' + base64.b64encode(b.getvalue()).decode()


LOGO = logo_uri(640)
LOGO_PETIT = logo_uri(120)

s = (ICI / 'guide.html').read_text(encoding='utf-8').replace(' loading="lazy"', '')
tete = s[:s.index('<div class="wrap">')]
regles = re.search(r'<section class="regles">.*?</section>', s, re.S).group(0)
legende = re.search(r'<div class="legende">.*?</div>', s, re.S).group(0)
intros = re.findall(r'<p class="lead">(.*?)</p>', s, re.S)
sections = re.findall(r'(<section class="tenue[^"]*" id="([^"]+)">.*?</section>)', s, re.S)

# Format A4 agrandi (même rapport) : la mise en page de 1000 px tient sans rétrécir, l'impression remet à l'échelle
css = '''
@page{size:280mm 396mm;margin:0}
html,body{background:#12150f!important;-webkit-print-color-adjust:exact;print-color-adjust:exact}
body{padding:0!important}
.wrap{max-width:none;margin:0}
.feuille{padding:16mm 15mm 12mm;break-after:page;min-height:395mm;position:relative}
.feuille:last-child{break-after:auto}
.ligne,figure,.regles,.berets{break-inside:avoid}
.tenue{margin-top:0!important}
.pied{position:absolute;left:15mm;right:15mm;bottom:8mm;display:flex;justify-content:space-between;color:#97a08c;font-size:11px;letter-spacing:.08em;text-transform:uppercase;border-top:1px solid #2f3728;padding-top:6px}
.garde{display:flex;flex-direction:column;justify-content:center}
.garde .logo{display:block;width:74mm;height:auto;margin:0 0 10mm}
.entete{display:flex;justify-content:space-between;align-items:flex-start}
.entete img{width:15mm;height:auto;margin-top:-4mm}
.garde h1{font-size:56px;margin:10px 0 18px}
.garde .lead{font-size:18px;max-width:70ch;margin-bottom:14px}
.garde .regles{margin-top:30px}
.sommaire{columns:2;margin:26px 0 0;padding:0;list-style:none;color:#e9ebe4;font-size:15px}
.sommaire li{padding:5px 0;border-bottom:1px dotted #2f3728;break-inside:avoid}
.sommaire span{color:#c9a227;margin-right:10px;font-variant-numeric:tabular-nums}
.partie-t{font:700 15px/1 "Segoe UI",sans-serif;letter-spacing:.2em;text-transform:uppercase;color:#c9a227;margin:0 0 12px}
@media print{body{background:#12150f!important;color:#e9ebe4!important}}
'''
tete = tete.replace('</style>', css + '</style>').replace("<title>Guide d'habillement PAG</title>", '<title>Dotations PAG</title>')


def titre_de(html):
    return re.sub(r'<.*?>', '', re.search(r'<h2>(.*?)(<span|</h2>)', html, re.S).group(1)).strip()


titres = [titre_de(h) for h, _ in sections]
pages = ['@@GARDE@@']
GARDE = ['<div class="feuille garde"><img class="logo" alt="" src="@@LOGO@@"><p class="eyebrow">Penguin Assault Group · milsim semi-RP armée française</p>'
         '<h1>Dotations</h1><p class="lead">%s</p><p class="lead">%s</p>%s%s<ul class="sommaire">@@SOMMAIRE@@</ul>'
         '<div class="pied"><span>PAG · guide des dotations</span><span>01</span></div></div>' % (intros[0], intros[1], regles, legende)]


def decouper(html, capacite=1250):
    """Coupe une fiche trop haute pour une page en plusieurs morceaux, entre deux lignes."""
    debut = html[:html.index('<div class="ligne')] if '<div class="ligne' in html else html
    lignes = re.findall(r'<div class="ligne.*?</div></div></div>', html, re.S)
    if not lignes:
        return [html]
    morceaux, courant, haut = [], [], 110
    for l in lignes:
        nfig = l.count('<figure>')
        h = 42 + (22 if 'class="note"' in l else 0) + 138 * ((nfig + 5) // 6)  # hauteurs mesurées sur les captures
        if courant and haut + h > capacite:
            morceaux.append(courant)
            courant, haut = [], 110
        courant.append(l)
        haut += h
    morceaux.append(courant)
    res = []
    for k, m in enumerate(morceaux):
        d = debut
        if k:
            d = re.sub(r'(<h2>.*?)(<span|</h2>)', r'\g<1> (suite)\g<2>', re.sub(r'<p class="quand">.*?</p>', '', debut, flags=re.S), count=1, flags=re.S)
        res.append(d + ''.join(m) + '</section>')
    return res


feuilles = []
for html, ident in sections:
    for k, morceau in enumerate(decouper(html)):
        feuilles.append((morceau, ident, titre_de(html) + (' (suite)' if k else '')))
sections = [(h, i) for h, i, _ in feuilles]
titres_pages = [t for _, _, t in feuilles]
for i, (html, ident) in enumerate(sections):
    partie = 'Habillement' if ident.startswith('t') or ident == 'berets' else 'Arsenal par certification'
    leg = legende if 'class="ligne' in html else ''
    pages.append('<div class="feuille"><div class="entete"><p class="partie-t">%s</p><img alt="" src="@@PETIT@@"></div>%s%s<div class="pied"><span>PAG · guide des dotations · %s</span><span>%02d</span></div></div>'
                 % (partie, leg.replace('class="legende"', 'class="legende" style="margin-block:0 14px"'), html, titres_pages[i], i + 2))

sommaire = ''.join('<li><span>%02d</span>%s</li>' % (i + 2, t) for i, t in enumerate(titres_pages) if not t.endswith('(suite)'))
pages[0] = GARDE[0].replace('@@SOMMAIRE@@', sommaire)
tmp = ICI / '_pdf.html'
tmp.write_text((tete + '<div class="wrap">' + ''.join(pages) + '</div></body></html>').replace('@@LOGO@@', LOGO).replace('@@PETIT@@', LOGO_PETIT), encoding='utf-8')
if SORTIE.exists():
    SORTIE.unlink()
subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--no-first-run', '--no-pdf-header-footer',
                '--user-data-dir=' + os.path.join(tempfile.gettempdir(), 'pag-chrome-capture'),
                '--virtual-time-budget=5000', '--print-to-pdf=' + str(SORTIE), tmp.as_uri()],
               capture_output=True, timeout=180)
tmp.unlink()
print('ok' if SORTIE.exists() else 'ÉCHEC', SORTIE.name, SORTIE.stat().st_size // 1024 if SORTIE.exists() else 0, 'Ko')
