# -*- coding: utf-8 -*-
# Index de l'arsenal FR : pour chaque objet des catalogues AMF, son entrée de catalogue et son nom affiché en jeu.
# Sortie : arsenal_index.json = [{"liste", "entree", "classe", "prefab", "nom", "source"}]
import io, os, re, sys, json, glob, struct, zlib
sys.stdout.reconfigure(encoding='utf-8')
ADDONS = r'C:\Users\goule\Documents\My Games\ArmaReforger\addons'
ICI = os.path.dirname(os.path.abspath(__file__))
MODS = ['AMF-CORE_65D2BD1BD7EA7BF6', 'AMF-FANTASSIN_64CF25A41DCBDBE0', 'AMF-FORCESSPECIALES_6A0AE2F91546B18A', 'AMF-VEHICULES01_65DB713038B8533C']
CATALOGUE = 'Configs/EntityCatalog/FR/InventoryItems_EntityCatalog_FR.conf'


def lire_pak(pak, garder):
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
            if garder(path):
                raw = d[off:off + csize]
                if csize != usize or comp:
                    try:
                        raw = zlib.decompress(raw)
                    except Exception:
                        pass
                out[path] = raw.decode('utf-8', 'replace')
    walk('')
    return out


def chaines_des_blocs(texte):
    """Les chaînes des deux blocs d'une table (identifiants, textes), lues caractère par caractère :
    un texte peut contenir des guillemets échappés et des retours à la ligne."""
    blocs, i, n = [], 0, len(texte)
    while i < n:
        j = texte.find('{', i)
        if j < 0:
            break
        nom = texte[texte.rfind(chr(10), 0, j) + 1:j].strip()
        i = j + 1
        if nom.startswith('StringTable'):
            continue
        chaines, suite = [], False
        while i < n and texte[i] != '}':
            if texte[i] == '"':
                i += 1
                courant = []
                while i < n and texte[i] != '"':
                    if texte[i] == chr(92) and i + 1 < n:
                        courant.append(texte[i + 1])
                        i += 2
                    else:
                        courant.append(texte[i])
                        i += 1
                morceau = ''.join(courant)
                if suite and chaines:
                    chaines[-1] += chr(10) + morceau      # morceau suivant du même texte
                else:
                    chaines.append(morceau)
                suite = i + 1 < n and texte[i + 1] == chr(92)   # guillemet fermant suivi d'un antislash : le texte continue
                if suite:
                    i += 1
            i += 1
        blocs.append((nom, chaines))
        i += 1
    ids = next((c for nm, c in blocs if nm.startswith('Ids')), [])
    autres = next((c for nm, c in blocs if not nm.startswith('Ids')), [])
    return ids, autres


fichiers = {}     # chemin -> texte, le dernier mod l'emporte (même ordre que le chargement)
catalogues = []
textes = {}
for mod in MODS:
    f = lire_pak(os.path.join(ADDONS, mod, 'data.pak'), lambda p: p.endswith('.et') or p == CATALOGUE or p.endswith('.fr_fr.conf') or p.endswith('.en_us.conf'))
    for chemin, texte in f.items():
        if chemin == CATALOGUE:
            catalogues.append((mod, texte))
        elif chemin.endswith('_fr.conf') or chemin.endswith('_us.conf'):
            ids, autres = chaines_des_blocs(texte)
            if ids and autres and len(ids) == len(autres):
                for i, t in zip(ids, autres):
                    if chemin.endswith('fr_fr.conf') or i not in textes:
                        textes[i] = t
        else:
            fichiers[chemin] = texte
print(len(fichiers), 'prefabs AMF,', len(textes), 'textes,', len(catalogues), 'catalogues')


def nom_de(chemin, profondeur=0):
    """Nom affiché d'un prefab : le sien, sinon celui de son parent."""
    texte = fichiers.get(chemin)
    if texte is None or profondeur > 12:
        return None
    m = re.search(r'ItemDisplayName\s+\w+\s+"[^"]*"\s*\{(.*?)\n\s*\}', texte, re.S)
    if m:
        n = re.search(r'\bName\s+"([^"]*)"', m.group(1))
        if n and n.group(1):
            return n.group(1)
    parent = re.match(r'\s*\w+\s*:\s*"\{[0-9A-F]{16}\}([^"]+)"', texte)
    if parent:
        return nom_de(parent.group(1), profondeur + 1)
    return None


index = []
for mod, texte in catalogues:
    liste = ''
    for ligne in texte.split('\n'):
        m = re.match(r'\s*SCR_EntityCatalogMultiListEntry\s+"\{([0-9A-F]{16})\}"', ligne)
        if m:
            liste = m.group(1)
            continue
        m = re.match(r'\s*(SCR_EntityCatalog\w+)\s+"\{([0-9A-F]{16})\}"\s*\{', ligne)
        if m:
            entree = (m.group(1), m.group(2))
            continue
        m = re.match(r'\s*m_sEntityPrefab\s+"(\{[0-9A-F]{16}\})([^"]+)"', ligne)
        if m:
            nom = nom_de(m.group(2))
            if nom and nom.startswith('#'):
                nom = textes.get(nom[1:], nom)
            index.append({'liste': liste, 'entree': entree[1], 'classe': entree[0], 'prefab': m.group(1) + m.group(2), 'nom': nom, 'source': mod.split('_')[0]})
json.dump(index, io.open(os.path.join(ICI, 'arsenal_index.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
sans = [x for x in index if not x['nom']]
print(len(index), 'objets au catalogue,', len(sans), 'sans nom trouvé')
for x in sans[:15]:
    print('   ', x['prefab'].split('}')[1])
