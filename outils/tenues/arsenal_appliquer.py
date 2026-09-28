# -*- coding: utf-8 -*-
# Retire de l'arsenal FR les objets marqués « retirer » : désactive leurs entrées dans la surcharge du catalogue du mod PAG.
# À relancer après tout changement du tri (classes.json) : arsenal_retirer.py puis ce script.
# La surcharge est reconstruite à partir d'une copie « de base » (sans retrait), gardée ici : catalogue_FR_base.conf.
import io, os, sys, json, shutil, collections
sys.stdout.reconfigure(encoding='utf-8')
ICI = os.path.dirname(os.path.abspath(__file__))
CONF = r'C:\Users\goule\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Configs\EntityCatalog\FR\InventoryItems_EntityCatalog_FR.conf'
BASE = os.path.join(ICI, 'catalogue_FR_base.conf')
MARQUE = '6532BE0C683CE002'     # la liste « Equipment », déjà présente dans notre surcharge

if not os.path.exists(BASE):
    if 'm_bEnabled 0' in io.open(CONF, encoding='utf-8').read().split('SCR_EntityCatalogInventoryItem', 2)[-1]:
        raise SystemExit('la surcharge contient déjà des retraits et aucune copie de base n\'existe')
    shutil.copy2(CONF, BASE)
base = io.open(BASE, encoding='utf-8').read()
donnees = json.load(io.open(os.path.join(ICI, 'arsenal_retirer.json'), encoding='utf-8'))
par_liste = collections.OrderedDict()
for e in donnees['entrees']:
    par_liste.setdefault(e['liste'], []).append(e)


def lignes(entrees):
    return ''.join('    %s "{%s}" {\n     m_bEnabled 0\n    }\n' % (e['classe'], e['entree']) for e in entrees)


# 1. la liste déjà présente : les retraits passent en tête de ses entrées
ancre = '  SCR_EntityCatalogMultiListEntry "{%s}" {\n   m_aEntities {\n' % MARQUE
if base.count(ancre) != 1:
    raise SystemExit('structure inattendue de la surcharge : liste Equipment introuvable')
sortie = base.replace(ancre, ancre + lignes(par_liste.pop(MARQUE, [])))

# 2. les autres listes : un bloc chacune, ajouté à la fin de m_aMultiLists
fin = '\n }\n}'
if not sortie.rstrip().endswith(fin):
    raise SystemExit('structure inattendue de la surcharge : fin de fichier')
blocs = ''.join('  SCR_EntityCatalogMultiListEntry "{%s}" {\n   m_aEntities {\n%s   }\n  }\n' % (liste, lignes(entrees)) for liste, entrees in par_liste.items())
corps = sortie.rstrip()[:-len(fin)]
sortie = corps + '\n' + blocs.rstrip('\n') + fin + '\n'
if sortie.count('{') != sortie.count('}'):
    raise SystemExit('accolades déséquilibrées')
io.open(CONF, 'w', encoding='utf-8', newline='\n').write(sortie)
print('%d objets retirés de l\'arsenal, dans %d listes ; laissés : %s' % (len(donnees['entrees']), len(par_liste) + 1, ', '.join(donnees['introuvables']) or 'aucun'))
