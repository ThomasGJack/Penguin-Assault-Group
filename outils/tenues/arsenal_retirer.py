# -*- coding: utf-8 -*-
# Rapproche les objets marqués « retirer » (classes.json) des entrées des catalogues d'arsenal AMF.
# Sortie : arsenal_retirer.json = {"entrees": [{liste, entree, classe, prefab, nom}], "introuvables": [...]}
import io, os, re, sys, json, unicodedata, collections
sys.stdout.reconfigure(encoding='utf-8')
ICI = os.path.dirname(os.path.abspath(__file__))
src = io.open(os.path.join(ICI, 'arsenal_index.py'), encoding='utf-8').read().split('index = []')[0]
exec(src)          # fichiers, textes, catalogues, lire_pak…


def norm(s):
    s = unicodedata.normalize('NFKD', s or '').encode('ascii', 'ignore').decode().lower()
    return re.sub(r'[^a-z0-9]+', ' ', s).strip()


def noms_de(chemin, profondeur=0):
    """Tous les noms d'objet du premier fichier de la chaîne (le prefab, sinon son parent…) qui en définit."""
    texte = fichiers.get(chemin)
    if texte is None or profondeur > 12:
        return []
    noms = []
    for m in re.finditer(r'ItemDisplayName\s+\w+\s+"[^"]*"\s*\{(.*?)\n\s*\}', texte, re.S):
        n = re.search(r'\bName\s+"([^"]*)"', m.group(1))
        if n and n.group(1):
            v = n.group(1)
            noms.append(textes.get(v[1:], v) if v.startswith('#') else v)
    if noms:
        return noms
    parent = re.match(r'\s*\w+\s*:\s*"\{[0-9A-F]{16}\}([^"]+)"', texte)
    return noms_de(parent.group(1), profondeur + 1) if parent else []


# Objets du jeu de base (prefabs binaires, illisibles) et noms que la capture a simplifiés : correspondance à la main
A_LA_MAIN = {
    "Montre": ["Watch_SandY184A_Map.et"], "Boussole": ["Compass_SY183.et"], "Affaires personnelles": ["PersonalBelongings_US.et"],
    "200rnd M249 Belt (M855A1)": ["Box_556x45_M249_200rnd_Ball.et"], "Bande 100 balles M60 antiblindage 7,62 × 51 mm": ["Box_762x51_M60_100rnd_AP.et"],
    "Bande 100 balles M60 7,62 × 51 mm": ["Box_762x51_M60_100rnd_Ball.et", "Box_762x51_M60_100rnd_4Ball_1Tracer.et"],
    "Tenue de protection de base (blocked)": ["PTB_base.et"],
    "Thalès BI-NYX Tan sans bonnettes": ["NVG_BI-NYX_Black_Without_WindShield.et", "NVG_BI-NYX_Tan_Without_WindShield.et"],
    "Thalès O-NYX Tan sans bonnettes": ["NVG_O-NYX_Tan_Without_WindShield.et"],
    "Poche Chargeur Simple": ["Pouch_Chargeur_SCARH_Simple.et"],
    # prefab de base du chargeur PGM, listé par erreur par AMF sous le nom du chargeur M14 du jeu de base
    "Chargeur 20 balles M14 7,62 × 51 mm": ["Magazine_127x99_PGM_7rnd_Base.et"],
    # « Poche Chargeur Double » et « Double FAMAS » : trois prefabs portent ce nom, on ne tranche pas (laissées à l'arsenal)
}

entrees = []
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
            entrees.append({'liste': liste, 'entree': entree[1], 'classe': entree[0], 'prefab': m.group(1) + m.group(2), 'noms': noms_de(m.group(2))})

classes = json.load(io.open(os.path.join(ICI, 'classes.json'), encoding='utf-8'))
garder = collections.Counter(norm(k.split('#')[0]) for k, v in classes.items() if v.get('s') == 'garder')
retirer = collections.Counter(norm(k.split('#')[0]) for k, v in classes.items() if v.get('s') == 'retirer')
libelle = {norm(k.split('#')[0]): k.split('#')[0] for k in classes}

choisies, introuvables, a_verifier = [], [], []
for n, combien in retirer.items():
    trouvees = [e for e in entrees if any(norm(x) == n for x in e['noms'])]
    manuel = A_LA_MAIN.get(libelle[n])
    if manuel:
        trouvees = [e for e in entrees if e['prefab'].split('/')[-1] in manuel]
    if not trouvees:
        introuvables.append(libelle[n])
        continue
    if garder.get(n):
        # Homonymes : un même nom pour l'arme nue (gardée) et ses variantes équipées (retirées).
        # Vérifié sur les icônes des captures : l'objet gardé est le premier dans l'ordre du catalogue
        # (sac camouflé avant le sac beige, arme nue avant ses variantes à lunette). On retire les suivants.
        gardees, trouvees = trouvees[:garder[n]], trouvees[garder[n]:]
        a_verifier.append((libelle[n], [e['prefab'].split('/')[-1] for e in gardees], [e['prefab'].split('/')[-1] for e in trouvees]))
    for e in trouvees:
        choisies.append(dict(e, nom=libelle[n]))

vus, uniques = set(), []
for e in choisies:
    if e['entree'] not in vus:
        vus.add(e['entree'])
        uniques.append(e)
json.dump({'entrees': uniques, 'introuvables': introuvables}, io.open(os.path.join(ICI, 'arsenal_retirer.json'), 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print('%d noms à retirer -> %d entrées de catalogue ; %d introuvables' % (len(retirer), len(uniques), len(introuvables)))
print('--- homonymes (gardé | retiré) :')
for nom, g, r in a_verifier:
    print('   %s : garde %s | retire %s' % (nom, g, r))
print('--- introuvables :')
for x in introuvables:
    print('  ', x)
