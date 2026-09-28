# -*- coding: utf-8 -*-
# Génère guide.html (habillement + armement par certification) à partir de guide.json et des icônes de generer.py.
import json, html, sys
sys.stdout.reconfigure(encoding='utf-8')
G = json.load(open('guide.json', encoding='utf-8'))
ICO = json.load(open('icones.json', encoding='utf-8'))
# Icônes ajoutées à la main (objets absents des captures de l'arsenal) : icones/<nom de la pièce>.png
import os, base64
for _f in sorted(os.listdir('icones')):
    if _f.lower().endswith('.png'):
        ICO[_f[:-4]] = base64.b64encode(open(os.path.join('icones', _f), 'rb').read()).decode('ascii')
E = html.escape
TYPES = {'impose': 'Imposé', 'choix': 'Au choix', 'perso': 'Personnalisation libre', 'interdit': 'Interdit'}
manquantes = []


def piece(p):
    cle, label = (p, p) if isinstance(p, str) else (p[0], p[1])
    if cle not in ICO:
        manquantes.append(cle)
        return ''
    return '<figure><img alt="" loading="lazy" src="data:image/png;base64,%s"><figcaption>%s</figcaption></figure>' % (ICO[cle], E(label))


def bloc(b, ident):
    sigle = ''
    if b.get('sigle'):
        sigle = '<span class="sigle">%s</span>' % E(b['sigle'])
    o = ['<section class="tenue" id="%s"><h2>%s%s</h2><p class="quand">%s</p>' % (ident, E(b['nom']), sigle, E(b['quand']))]
    for l in b['lignes']:
        note = ''
        if l.get('note'):
            note = '<p class="note">%s</p>' % E(l['note'])
        pieces = ''.join(piece(p) for p in l['pieces'])
        if pieces:
            pieces = '<div class="pieces">%s</div>' % pieces
        o.append('<div class="ligne %s"><div class="slot"><strong>%s</strong><span class="tag %s">%s</span></div><div class="contenu">%s%s</div></div>'
                 % (l['type'], E(l['slot']), l['type'], TYPES[l['type']], note, pieces))
    o.append('</section>')
    return '\n'.join(o)


A = G.get('armement')
out = [open('gabarit_guide.html', encoding='utf-8').read()]
out.append('<header><p class="eyebrow">Penguin Assault Group</p><h1>%s</h1><p class="lead">%s</p></header>' % (E(G['titre']), E(G['intro'])))

nav = ['<span class="navt">Habillement</span>']
nav += ['<a href="#t%d">%s</a>' % (i, E(t['nom'])) for i, t in enumerate(G['tenues'])]
nav += ['<a href="#berets">Bérets</a>']
if A:
    nav.append('<span class="navt">Armement</span>')
    nav += ['<a href="#a%d">%s</a>' % (i, E(b['nom'])) for i, b in enumerate(A['blocs'])]
out.append('<nav>' + ''.join(nav) + '</nav>')

out.append('<section class="regles"><h2>Les cinq règles</h2><ol>' + ''.join('<li>%s</li>' % E(r) for r in G['regles']) + '</ol></section>')
out.append('<div class="legende">' + ''.join('<span class="tag %s">%s</span>' % (k, v) for k, v in TYPES.items() if k != 'interdit') + '</div>')

for i, t in enumerate(G['tenues']):
    out.append(bloc(t, 't%d' % i))

out.append('<section class="tenue" id="berets"><h2>Le béret de sa classe</h2><p class="quand">Porté à la base. En cas de cumul, on choisit parmi ceux auxquels on a droit.</p><div class="berets">')
for cle, role in G['berets']:
    out.append('<figure><img alt="" src="data:image/png;base64,%s"><figcaption><strong>%s</strong></figcaption></figure>' % (ICO[cle], E(role)))
out.append('</div></section>')
if G.get('interdits'):
    out.append('<section class="tenue interdits" id="interdits"><h2>Interdit en Centre Europe</h2><ul>' + ''.join('<li>%s</li>' % E(x) for x in G['interdits']) + '</ul></section>')

if A:
    out.append('<header class="partie"><p class="eyebrow">Deuxième partie</p><h1>%s</h1><p class="lead">%s</p></header>' % (E(A['titre']), E(A['intro'])))
    for i, b in enumerate(A['blocs']):
        out.append(bloc(b, 'a%d' % i))
    if A.get('interdits'):
        out.append('<section class="tenue interdits" id="ainterdits"><h2>Interdit à l\'armement</h2><ul>' + ''.join('<li>%s</li>' % E(x) for x in A['interdits']) + '</ul></section>')

out.append('</div></body></html>')
open('guide.html', 'w', encoding='utf-8').write('\n'.join(out))
print('guide.html :', len(G['tenues']), 'tenues,', len(A['blocs']) if A else 0, 'fiches d\'armement')
if manquantes:
    print('PIÈCES INCONNUES :', sorted(set(manquantes)))
