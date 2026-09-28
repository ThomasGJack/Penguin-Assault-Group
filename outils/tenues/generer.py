# -*- coding: utf-8 -*-
# Découpe les icônes des captures (captures/) selon items.json et génère tenues.html (autonome, images intégrées).
import json, io, base64, html, sys
from PIL import Image
import numpy as np
sys.stdout.reconfigure(encoding='utf-8')
data = json.load(open('items.json', encoding='utf-8'))
CLASSES = json.load(open('classes.json', encoding='utf-8'))  # attributions décidées : statut et classe par défaut


def rows_of(im):
    a = np.asarray(im).astype(int)
    bg = a[5, 5]
    d = (abs(a[:, 10:118] - bg).sum(axis=2) > 40).sum(axis=1)
    rows, start = [], None
    for y, v in enumerate(d):
        if v > 3 and start is None:
            start = y
        if v <= 3 and start is not None:
            if y - start > 25:
                rows.append((start, y))
            start = None
    if start is not None and len(a) - start > 25:
        rows.append((start, len(a)))
    return rows


sections, total, num, vus = [], 0, 0, {}
for bloc in data:
    im = Image.open('captures/' + bloc['capture']).convert('RGB')
    boites = bloc.get('boites')
    if bloc.get('grille'):  # [y du premier nom, pas, nombre] : icônes fines (armes) que la détection découpe mal
        y0, pas, nb = bloc['grille']
        cols = bloc.get('colonnes', [0])  # captures en plusieurs colonnes : lecture ligne par ligne
        boites = [[x + 8, round(y0 + k * pas) - 43, x + 120, round(y0 + k * pas) + 43] for k in range(nb) for x in cols]
        boites = boites[:len(bloc['noms'])]
    rows = boites or rows_of(im)
    if len(rows) != len(bloc['noms']):
        print('ATTENTION', bloc['capture'], len(rows), 'icônes pour', len(bloc['noms']), 'noms')
    lignes = []
    for k, (row, nom) in enumerate(zip(rows, bloc['noms'])):
        if not nom:
            continue  # case déjà recensée ailleurs
        if boites:
            box = tuple(row)
            crop = Image.new('RGB', (box[2] - box[0], box[3] - box[1]), im.getpixel((5, 5)))
            haut = max(0, box[1])
            gauche = max(0, box[0])
            crop.paste(im.crop((gauche, haut, box[2], min(im.height, box[3]))), (gauche - box[0], haut - box[1]))
        else:
            c = (row[0] + row[1]) // 2
            box = (8, c - 46, 120, c + 46)
            crop = im.crop(box)
            # on masque ce qui dépasse sur les icônes voisines (fond de la capture)
            fond = im.getpixel((5, 5))
            haut = 0
            bas = im.height
            if k > 0:
                haut = (rows[k - 1][1] + row[0]) // 2
            if k + 1 < len(rows):
                bas = (row[1] + rows[k + 1][0]) // 2
            if haut > box[1]:
                crop.paste(fond, (0, 0, 112, haut - box[1]))
            if bas < box[3]:
                crop.paste(fond, (0, bas - box[1], 112, 92))
            if box[1] < 0:
                crop.paste(fond, (0, 0, 112, -box[1]))
            if box[3] > im.height:
                crop.paste(fond, (0, im.height - box[1], 112, 92))
        crop = crop.resize((168, 138), Image.LANCZOS)
        buf = io.BytesIO()
        crop.save(buf, 'PNG')
        num += 1
        fam, sep, reste = nom.partition('@@')
        if sep:
            nom = reste
        else:
            fam = bloc['famille']
        nom, _, note = nom.partition('||')
        vus[nom] = vus.get(nom, 0) + 1
        cle = nom
        if vus[nom] > 1:
            cle = '%s#%d' % (nom, vus[nom])
        lignes.append((fam, (num, base64.b64encode(buf.getvalue()).decode(), nom, note, cle)))
    total += len(lignes)
    for fam, ligne in lignes:  # une famille peut venir de plusieurs captures
        for sec in sections:
            if sec[0] == fam:
                sec[1].append(ligne)
                break
        else:
            sections.append((fam, [ligne]))

HEAD = open('gabarit_tete.html', encoding='utf-8').read()
FOOT = open('gabarit_pied.html', encoding='utf-8').read()
out = [HEAD]
for fam, lignes in sections:
    out.append('<section><h2>%s <span>%d pièces</span></h2><div class="tw"><table><tr><th>#</th><th>Image</th><th>Nom</th><th>Statut</th><th class="c">Classe / usage</th></tr>' % (html.escape(fam), len(lignes)))
    for n, b64, nom, note, cle in lignes:
        extra = ''
        if note:
            extra = '<small>⚠ %s</small>' % html.escape(note)
        dec = CLASSES.get(cle, {})
        out.append('<tr data-id="%d" data-k="%s" data-ds="%s" data-dc="%s" data-s="voir"><td class="n">%d</td><td class="i"><img alt="" src="data:image/png;base64,%s"></td><td class="nom"><span>%s</span>%s</td><td class="s"><select><option value="voir">À voir</option><option value="garder">Garder</option><option value="retirer">Retirer</option></select></td><td class="c"><input placeholder="ex. Logistique"></td></tr>' % (n, html.escape(cle, quote=True), dec.get('s', 'voir'), html.escape(dec.get('c', ''), quote=True), n, b64, html.escape(nom), extra))
    out.append('</table></div></section>')
out.append(FOOT)
json.dump({l[4]: l[1] for _, ls in sections for l in ls}, open('icones.json', 'w', encoding='utf-8'))  # pour le guide
open('tenues.html', 'w', encoding='utf-8').write('\n'.join(out))
print('tenues.html :', total, 'pièces,', len(sections), 'familles')
