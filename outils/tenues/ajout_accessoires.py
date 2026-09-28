# -*- coding: utf-8 -*-
# Ajoute à items.json les captures en trois colonnes (optiques, accessoires, équipement, médical, munitions).
# Un nom « Famille@@Nom » range la pièce dans cette famille ; "" = case ignorée (déjà recensée ailleurs).
import io, json
p = 'items.json'
d = json.load(io.open(p, encoding='utf-8'))
if any(b['capture'] == 'cap_28.png' for b in d):
    raise SystemExit('déjà ajouté')
P = 85.4
D = "||doublon de nom dans l'arsenal"
O, V, G, B = "Optiques@@", "Organes de visée de secours@@", "Poignées et interfaces@@", "Bouche et baïonnette@@"

d += [
 {"capture": "cap_28.png", "famille": "Optiques", "colonnes": [-4, 551, 1106], "grille": [54, P, 12], "noms": [
  O + "Aimpoint CompM5", O + "Aimpoint CompM PGMP", O + "Aimpoint Pro Patrol",
  O + "Aimpoint Micro T1", O + "Aimpoint Micro T2", O + "Eotech 512",
  O + "Eotech 552", O + "Eotech 553", O + "Eotech EXPS3-0",
  O + "Elcan Specter", O + "Scrome J4 F1 PGMP", O + "Scrome J4 F1 PGMP sans bonette",
  O + "Scrome J4 F1 RIS", O + "Scrome J4 F1 RIS sans bonette", O + "Scrome J4 F1 Minimi F1",
  O + "Scrome J4 F1 Minimi F1" + D + " : sans doute la version sans bonnette", V + "Guidon HK-416F", V + "Guidon Magpul MBUS Noir",
  V + "Guidon Magpul MBUS Dark Earth", V + "Guidon Magpul MBUS Olive", V + "Oeilleton HK-416F",
  V + "Oeilleton Magpul MBUS Noir", V + "Oeilleton Magpul MBUS Dark Earth", V + "Oeilleton Magpul MBUS Olive",
  B + "Baïonnette Famas", G + "Interface poignée Famas", G + "Interface poignée HK Famas",
  G + "Interface poignée Famas Magpul RVG Noir", G + "Interface poignée Famas Magpul RVG Dark Erath||faute de frappe : « Earth »", G + "Interface poignée Famas Magpul RVG Olive Drab",
  G + "Interface poignée bipied Famas Noir", G + "Interface poignée bipied Famas Dark Earth", G + "Interface poignée bipied Famas Olive Drab",
  G + "Interface poignée Minimi F1", G + "Interface poignée HK Minimi F1", G + "Interface poignée Minimi F1 Magpul RVG Noir"]},
 {"capture": "cap_27.png", "famille": "Poignées et interfaces", "colonnes": [0, 555, 1110], "grille": [42, P, 10], "noms": [
  G + "Interface poignée Minimi F1 Magpul RVG Dark Earth", G + "Interface poignée Minimi F1 Magpul RVG Olive Drab", G + "Interface poignée Minimi F1 poignée bipied Dark Earth",
  G + "Interface poignée Minimi F1 poignée bipied Dark Earth" + D + " : l'une des deux est sans doute la version Noir", G + "Interface poignée Minimi F1 poignée bipied Olive Drab", G + "Poignée Vertical",
  G + "Poignée Vertical HK", G + "Poignée Bipied Noir", G + "Poignée Bipied Dark Earth",
  G + "Poignée Bipied Olive Drab", G + "Magpul RVG Noir", G + "Magpul RVG Dark Earth",
  G + "Magpul RVG Olive Drab", G + "Magpul AFG 2 Noir", G + "Magpul AFG 2 Dark Earth",
  G + "Magpul AFG 2 Olive Drab", G + "Magpul AFG Noir", G + "Magpul AFG Dark Earth",
  G + "Magpul AFG Olive Drab", B + "Cache Flame Famas F1", B + "Cache Flame Famas G2",
  B + "Vortex CF-22", B + "RDS Zastava Famas", O + "Schmidt & Bender 8X24 Mildot CC",
  O + "Schmidt & Bender 3-20x50", O + "Scrome J8", O + "Scrome J8 NoCover",
  O + "Scrome J10", O + "Scrome J10 NoCover"]},
 {"capture": "cap_30.png", "famille": "Équipement", "colonnes": [-3, 552, 1107], "grille": [55, P, 10], "noms": [
  "Navigation et éclairage@@Carte (OTAN)", "Navigation et éclairage@@Boussole", "Vision nocturne@@Thalès BI-NYX Tan",
  "Vision nocturne@@Thalès BI-NYX Tan sans bonnettes", "Vision nocturne@@Thalès BI-NYX Tan" + D, "Vision nocturne@@Thalès BI-NYX Tan sans bonnettes" + D,
  "Vision nocturne@@Thalès BI-NYX Tan sans string", "Vision nocturne@@Thalès BI-NYX Tan sans string" + D + " : l'image est noire, sans doute la version Black", "Vision nocturne@@Thalès BI-NYX Tan sans bonettes et sans string",
  "Vision nocturne@@Thalès O-NYX Tan", "Vision nocturne@@Thalès O-NYX Tan sans bonnettes", "Vision nocturne@@Thalès O-NYX Tan sans bonnettes et sans string",
  "Vision nocturne@@Thalès O-NYX Tan sans string", "Navigation et éclairage@@Lampe-torche", "",
  "", "", "",
  "Radio@@Thalès ER328", "Outils@@Pelle M1967", "Outils@@Récipient à carburant",
  "Outils@@Clé de réparation", "Écussons et poches@@Écusson PAG", "Écussons et poches@@Écusson PAG (veste)",
  "", "Écussons et poches@@Poche Chargeur Double", "Écussons et poches@@Poche Chargeur Double FAMAS",
  "Écussons et poches@@Poche Chargeur Simple", "Écussons et poches@@Affaires personnelles"]},
 {"capture": "cap_29.png", "famille": "Matériel médical", "colonnes": [-10, 545, 1100], "grille": [52, P, 2], "noms": [
  "Bandage", "Garrot", "Morphine auto-injectable", "Solution saline", "Épinéphrine", "Kit médical"]},
 {"capture": "cap_31.png", "famille": "Munitions de mitrailleuse", "colonnes": [-2, 553, 1108], "grille": [41, P, 3], "noms": [
  "200Rnd AAN-F1 7.62X51mm 4BO/1TRC", "200Rnd MAG 58 7.62X51mm 4BO/1TRC", "105Rnd 12.7X99 M2HB 2BO/1PEI/1AP/1TRC||munition de 12,7 mm de véhicule",
  "Munitions de 40 mm@@AMF 40mm HE M406", "Munitions de 40 mm@@AMF HEDP M433", "Munitions de 40 mm@@Fusée blanche M583A1 40 mm",
  "Munitions de 40 mm@@Fusée verte M661 40 mm", "Munitions de 40 mm@@Fusée rouge M662 40 mm"]},
 {"capture": "cap_32.png", "famille": "Chargeurs", "colonnes": [-6, 549, 1104], "grille": [57, P, 12], "noms": [
  "Chargeur SCAR-H (7,62 × 51 mm OTAN)", "Munitions de mitrailleuse@@100Rnd Minimi BO", "Munitions de mitrailleuse@@100Rnd Minimi BO/BT",
  "Munitions de mitrailleuse@@200rnd M249 Belt (M855A1)||aucune M249 dans l'arsenal : sert à la Minimi ?", "30Rnd HK416 BO F5", "30Rnd HK416 BO/BT F5",
  "30Rnd PMAG BO F5", "30Rnd PMAG BO/BT F5", "30Rnd PMAG BT F5",
  "30Rnd PMAG TAN BO F5", "30Rnd PMAG TAN BO/BT F5", "30Rnd PMAG BT F5 TAN",
  "BO 25Rnd FMJ SS109||chargeur FAMAS", "BO/TR 25Rnd FMJ SS109||chargeur FAMAS", "25Rnd BT SS109||chargeur FAMAS",
  "BO 25Rnd FMJ F1||chargeur FAMAS", "BO/TR 25Rnd FMJ F1/F1A||chargeur FAMAS", "25Rnd BT F1A||chargeur FAMAS",
  "Chargeur Glock 9x19 17 cps", "Chargeur 15 balles M9 9 × 19 mm||chargeur du PAMAS G1", "9Rnd PA-MAC 50 FMJ",
  "10Rnd FRF2 BO M62", "10Rnd FRF2 AP", "Chargeur 20 balles M14 7,62 × 51 mm||aucun M14 dans l'arsenal : chargeur orphelin ?",
  "7Rnd 12.7X99 PGM Hecate II PEI", "", "",
  "Munitions de mitrailleuse@@Bande 100 balles M60 antiblindage 7,62 × 51 mm||aucune M60 dans l'arsenal : sert à la MAG 58 ?", "Munitions de mitrailleuse@@Bande 100 balles M60 7,62 × 51 mm", "",
  "Munitions de mitrailleuse@@50Rnd AAN-F1 7.62X51 BO/BT", "Munitions de mitrailleuse@@50Rnd AAN-F1 7.62X51 BO", "Munitions de mitrailleuse@@75Rnd AAN-F1 7.62X51 BO/BT",
  "", "", "Munitions de mitrailleuse@@100Rnd AAN-F1 7.62X51 BO"]}
]
json.dump(d, io.open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print('ok', len(d), 'blocs')
