# -*- coding: utf-8 -*-
# Une image PNG par partie du guide (tenue, bérets, chaque dotation), via Chrome sans interface.
import re, os, sys, subprocess, tempfile, pathlib
sys.stdout.reconfigure(encoding='utf-8')
CHROME = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
ICI = pathlib.Path(__file__).resolve().parent
SORTIE = ICI / 'captures_guide'
SORTIE.mkdir(exist_ok=True)
for f in SORTIE.glob('*.png'):
    f.unlink()
LARGEUR = 1000

s = (ICI / 'guide.html').read_text(encoding='utf-8')
tete = s[:s.index('<div class="wrap">')]
tete = tete.replace('padding-block:36px 80px', 'padding-block:26px 30px')
regles = re.search(r'<section class="regles">.*?</section>', s, re.S).group(0)
legende = re.search(r'<div class="legende">.*?</div>', s, re.S).group(0)
sections = re.findall(r'(<section class="tenue[^"]*" id="([^"]+)">.*?</section>)', s, re.S)


def nom_fichier(i, html):
    titre = re.search(r'<h2>(.*?)(<span|</h2>)', html, re.S).group(1)
    titre = re.sub(r'[^0-9A-Za-zÀ-ÿ]+', '-', titre).strip('-')
    return '%02d-%s.png' % (i, titre)


def chrome(args):
    return subprocess.run([CHROME, '--headless=new', '--disable-gpu', '--hide-scrollbars', '--no-first-run',
                           '--user-data-dir=' + os.path.join(tempfile.gettempdir(), 'pag-chrome-capture')] + args,
                          capture_output=True, text=True, encoding='utf-8', errors='replace', timeout=120)


pages = [('00-Regles.png', '<p class="eyebrow">Penguin Assault Group</p><h1>Guide des dotations</h1>' + regles + legende)]
for i, (html, ident) in enumerate(sections, 1):
    marque = '<p class="eyebrow" style="margin-bottom:10px">Penguin Assault Group · guide des dotations</p>'
    leg = legende if 'class="ligne' in html else ''  # pas de légende sur la planche des bérets
    pages.append((nom_fichier(i, html), marque + leg.replace('class="legende"', 'class="legende" style="margin-block:0 6px"') + html.replace('class="tenue', 'style="margin-top:14px" class="tenue', 1)))

for nom, corps in pages:
    corps = corps.replace(' loading="lazy"', '')
    page = tete + '<div class="wrap">' + corps + '</div><script>addEventListener("load",()=>{document.title="H="+Math.ceil(document.documentElement.getBoundingClientRect().height)})</script></body></html>'
    tmp = SORTIE / '_page.html'
    tmp.write_text(page, encoding='utf-8')
    url = tmp.as_uri()
    dom = chrome(['--window-size=%d,2000' % LARGEUR, '--virtual-time-budget=3000', '--dump-dom', url]).stdout
    m = re.search(r'<title>H=(\d+)</title>', dom)
    hauteur = int(m.group(1)) if m else 2000
    chrome(['--window-size=%d,%d' % (LARGEUR, hauteur), '--force-device-scale-factor=1.5', '--virtual-time-budget=3000',
            '--screenshot=' + str(SORTIE / nom), url])
    ok = (SORTIE / nom).exists()
    print(('ok  ' if ok else 'ÉCHEC ') + nom, hauteur, 'px')
(SORTIE / '_page.html').unlink()
