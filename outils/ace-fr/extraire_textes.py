# -*- coding: utf-8 -*-
# Dresse la liste de tous les textes des mods ACE Dev installés : identifiant, anglais, et « français » actuel du mod.
import io, os, sys, json, glob, struct, zlib
sys.stdout.reconfigure(encoding='utf-8')
ADDONS = r'C:\Users\goule\Documents\My Games\ArmaReforger\addons'
ICI = os.path.dirname(os.path.abspath(__file__))
RETENUS = ['Core', 'Medical Core', 'Medical Hitzones', 'Medical Circulation', 'Medical Breathing', 'Medical AI', 'Overheating', 'Scopes', 'Ballistics',
           'Weather', 'Cook-Off', 'Explosives', 'Trenches', 'Backblast', 'Chopping', 'Tactical Ladder', 'Tactical Periscope', 'Radio', 'Captives',
           'Carrying', 'Finger', 'Facepaint', 'Magazine Repack']


def fichiers_du_pak(pak, suffixes):
    """Rend {chemin: contenu} pour les fichiers du .pak dont le nom finit par un des suffixes."""
    d = open(pak, 'rb').read()
    pos, chunks = 12, {}
    while pos < len(d):
        tag = d[pos:pos + 4]
        size = struct.unpack('>I', d[pos + 4:pos + 8])[0]
        chunks[tag] = (pos + 8, size)
        pos += 8 + size
    p = [chunks[b'FILE'][0]]
    out = {}

    def walk(prefix):
        typ = d[p[0]]
        ln = d[p[0] + 1]
        name = d[p[0] + 2:p[0] + 2 + ln].decode('utf-8', 'replace')
        p[0] += 2 + ln
        if typ == 0:
            count = struct.unpack('<I', d[p[0]:p[0] + 4])[0]
            p[0] += 4
            for _ in range(count):
                walk(prefix + name + '/' if name else prefix)
        else:
            off, csize, usize = struct.unpack('<III', d[p[0]:p[0] + 12])
            comp = d[p[0] + 18]
            p[0] += 24
            path = prefix + name
            if path.endswith(suffixes):
                raw = d[off:off + csize]
                if csize != usize or comp:
                    try:
                        raw = zlib.decompress(raw)
                    except Exception:
                        pass
                out[path] = raw.decode('utf-8', 'replace')
    walk('')
    return out


def table(texte):
    """(identifiants, textes) d'un StringTableRuntime : deux blocs de chaînes entre guillemets."""
    blocs, courant = [], None
    for ligne in texte.split('\n'):
        t = ligne.strip()
        if t.endswith('{') and not t.startswith('"'):
            courant = []
            blocs.append((t, courant))
        elif t.startswith('"') and courant is not None:
            courant.append(t[1:-1] if t.endswith('"') else t[1:])
    ids = [b for n, b in blocs if n.startswith('Ids')]
    autres = [b for n, b in blocs if not n.startswith('Ids') and not n.startswith('StringTable')]
    return (ids[0], autres[0]) if ids and autres else ([], [])


def guid_de(rdb, chemin):
    d = open(rdb, 'rb').read()
    i = d.find(chemin.encode())
    if i < 0:
        return ''
    e = d.find(b'\x00', i)
    return d[e + 7:e + 15][::-1].hex().upper()


resultat = []
for dossier in sorted(glob.glob(os.path.join(ADDONS, 'ACE*Dev_*'))):
    nom = json.load(io.open(os.path.join(dossier, 'ServerData.json'), encoding='utf-8-sig'))['name']
    court = nom.replace('ACE ', '').replace(' Dev', '')
    if court not in RETENUS:
        continue
    f = fichiers_du_pak(os.path.join(dossier, 'data.pak'), ('.en_us.conf', '.fr_fr.conf'))
    for chemin in sorted(f):
        if not chemin.endswith('.fr_fr.conf'):
            continue
        en = f.get(chemin.replace('.fr_fr.conf', '.en_us.conf'), '')
        ids, fr = table(f[chemin])
        ids_en, txt_en = table(en)
        anglais = dict(zip(ids_en, txt_en))
        resultat.append({'mod': nom, 'dossier': os.path.basename(dossier), 'chemin': chemin, 'guid': guid_de(os.path.join(dossier, 'resourceDatabase.rdb'), chemin),
                         'source': f[chemin], 'lignes': [{'id': i, 'en': anglais.get(i, ''), 'fr_mod': t} for i, t in zip(ids, fr)]})
json.dump(resultat, io.open(os.path.join(ICI, 'textes_ace.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
total = sum(len(r['lignes']) for r in resultat)
deja = sum(1 for r in resultat for l in r['lignes'] if l['fr_mod'] != l['en'])
print(len(resultat), 'tables,', total, 'textes, dont', deja, 'déjà différents de l\'anglais')
with io.open(os.path.join(ICI, 'textes_ace.txt'), 'w', encoding='utf-8') as o:
    for r in resultat:
        o.write('\n## %s | %s | {%s}\n' % (r['mod'], r['chemin'], r['guid']))
        for l in r['lignes']:
            marque = '' if l['fr_mod'] == l['en'] else '   [FR du mod : %s]' % l['fr_mod']
            o.write('%s\t%s%s\n' % (l['id'], l['en'], marque))
