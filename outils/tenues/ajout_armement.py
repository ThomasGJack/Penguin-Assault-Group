# -*- coding: utf-8 -*-
# Écrit la partie « armement par certification » dans guide.json (remplace la précédente si elle existe).
import io, json
p = 'guide.json'
g = json.load(io.open(p, encoding='utf-8'))

FUSIL = ["★ HK416 F-S — recommandé", "FAMAS Valorisé", "FAMAS F1"]
FUSIL = [["HK416 F-S", "★ HK416 F-S — recommandé"], "Famas Valorisé", "FAMAS F1", "FAMAS F1 RIS"]
NOTE_300 = "10 chargeurs de 30 (HK416) ou 12 chargeurs de 25 (FAMAS), pleins : 300 cartouches ; 1 engagé, le reste en poches et dans le sac."
NOTE_300_HK = "10 chargeurs de 30, pleins : 300 cartouches ; 1 engagé, le reste en poches et dans le sac."
POCHES = [["Poche Chargeur Double", "Poche Chargueur Double"], ["Poche Chargeur Simple", "Poche Chargueur Simple"]]
CH_HK = ["30Rnd HK416 BO F5", "30Rnd HK416 BO/BT F5", "30Rnd PMAG BO F5", "30Rnd PMAG BO/BT F5"]
CH_FAMAS = ["BO 25Rnd FMJ SS109", "BO/TR 25Rnd FMJ SS109", "BO 25Rnd FMJ F1", "BO/TR 25Rnd FMJ F1/F1A"]
POING = [["Glock 17 Gen 5", "★ Glock 17 Gen 5 — recommandé"], "PAMAS G1"]
CH_POING = ["Chargeur Glock 9x19 17 cps", "Chargeur 15 balles M9 9 × 19 mm"]
POINT_ROUGE = ["Aimpoint CompM5", "Aimpoint Micro T2", "Eotech 552", "Eotech EXPS3-0", "Scrome J4 F1 RIS"]

g['armement'] = {
 "titre": "Guide d'arsenal — armement par certification",
 "intro": "Chaque soldat part avec la dotation commune, plus la dotation de son rôle du jour. On ne prend une arme spécialisée qu'avec la certification correspondante : le mod enregistre une infraction sinon. Les quantités sont un repère pour une mission standard, le chef de groupe peut les adapter. Les optiques jusqu'à x4 et tous les accessoires d'arme sont libres : poignées, interfaces, rails, organes de visée, cache-flammes, baïonnette. À l'apparition, chacun reçoit déjà son kit de départ : HK416 F-S, 10 chargeurs, gilet et sac ; l'arsenal sert à compléter. À l'arsenal, seuls les chargeurs (1 paquet de munitions chacun), les grenades et les roquettes puisent dans la soute de la base ; armes, tenues, accessoires et matériel médical ne coûtent rien.",
 "blocs": [
  {
   "nom": "Dotation commune", "sigle": "Tous",
   "quand": "Ce que tout soldat a sur lui, quel que soit son rôle. Les fiches suivantes ne listent que ce qui change.",
   "lignes": [
    {"slot": "Radio", "type": "impose", "note": "Une radio individuelle par soldat.", "pieces": ["Thalès ER328"]},
    {"slot": "Caméra de casque", "type": "impose", "note": "Obligatoire pour tout le monde, fixée sur le casque (emplacement « Camera » du MSA TCNVG CCE ou du casque F3). On l'allume au départ en mission depuis le menu radial : le PC de la base suit l'opération en direct. Deux couleurs au choix.", "pieces": ["Caméra de casque", "Caméra de casque (noire)"]},
    {"slot": "Navigation", "type": "impose", "note": "Carte, lampe et la montre GPS Tactix Bravo, obligatoire pour tout le monde : elle sert aussi de boussole.", "pieces": ["Carte (OTAN)", "Tactix Bravo GPS Smartwatch Black", "Lampe-torche"]},
    {"slot": "Soins individuels", "type": "impose", "note": "4 bandages, 1 garrot, 1 morphine, 1 pansement thoracique et 1 solution saline. Ce matériel n'est pas pour toi : il sert à soigner ton binôme, et c'est le sien qui te sauvera. La solution saline se donne à l'infirmier, c'est lui qui perfuse. Un blessé qui saigne meurt en 10 minutes, un cœur arrêté en 15 : on arrête le sang d'abord, on masse ensuite, l'infirmier fait le reste. Tout autre matériel médical est réservé aux soignants.", "pieces": ["Bandage", "Garrot", "Morphine auto-injectable", "Pansement thoracique", "Solution saline"]},
    {"slot": "Grenades", "type": "choix", "note": "2 grenades au maximum, offensive ou défensive, et 1 fumigène blanc.", "pieces": ["GR MA OF F1", "GR MA DEF", "GR MA FUM Blanche"]},
    {"slot": "Poches à chargeurs", "type": "choix", "note": "Sur le gilet, au choix : la poche double ou la poche simple AMF pour chargeurs de 5,56, pour loger ses 300 cartouches.", "pieces": POCHES},
    {"slot": "Arme de poing", "type": "choix", "note": "Facultative, avec 3 chargeurs.", "pieces": POING + CH_POING},
    {"slot": "Arme de poing de l'officier", "type": "choix", "note": "Réservé aux officiers, lieutenant et capitaine, à la place du Glock ou du PAMAS. Avec 3 chargeurs.", "pieces": ["PA MAC 50", "9Rnd PA-MAC 50 FMJ"]},
    {"slot": "Vision nocturne", "type": "choix", "note": "Libres et gratuites à l'arsenal, comme la lampe : à prendre dès qu'une mission peut se prolonger la nuit (une journée de jeu dure 3 h).", "pieces": ["Thalès O-NYX Tan", "Thalès BI-NYX Tan"]},
    {"slot": "Écusson", "type": "impose", "pieces": ["Écusson PAG", "Écusson PAG (veste)"]},
    {"slot": "Matériel facultatif", "type": "perso", "note": "Au choix de chacun, si la place le permet : la crème de camouflage, 2 serflex pour faire un prisonnier, le périscope pour observer sans s'exposer.", "pieces": ["Crème de camouflage", "Serflex", "Périscope"]}
   ]
  },
  {
   "nom": "Fusilier", "sigle": "FGI",
   "quand": "Le soldat sans spécialité, dès la FGI validée. C'est aussi la dotation de la recrue accompagnée.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Le HK416 F-S est le fusil recommandé. Le FAMAS reste autorisé ; le F1 RIS est le F1 à rail, pour monter une optique ; les rails Picatinny PGM et PGMP sont à l'arsenal, gratuits, avec les garde-mains PGM et PGMP du F1, pour monter une optique sur un F1 classique.", "pieces": FUSIL},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300 + " Balles ordinaires, ou mixtes avec traçantes.", "pieces": CH_HK + CH_FAMAS},
    {"slot": "Gilet et sac", "type": "impose", "pieces": ["Gilet de Combat SMB - Grenadier Voltigeur (Fusilier)", "TECPACK 45L"]}
   ]
  },
  {
   "nom": "Mule", "sigle": "Sans certification",
   "quand": "Le pourvoyeur du groupe : il porte le gros sac et ravitaille ses coéquipiers pendant la mission. Aucune certification n'est demandée, la FGI suffit. Une mule par groupe, désignée par le chef de groupe.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Le fusil du fusilier, avec ses propres 300 cartouches : le reste de la place sert aux autres.", "pieces": FUSIL},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300, "pieces": CH_HK + CH_FAMAS},
    {"slot": "Sac", "type": "impose", "note": "Le Tecpack 70 L, le plus gros sac de l'arsenal.", "pieces": ["TECPACK 70L"]},
    {"slot": "Chargeurs du groupe", "type": "impose", "note": "16 chargeurs de fusil, du modèle que porte le groupe.", "pieces": CH_HK + CH_FAMAS},
    {"slot": "Munitions des spécialistes", "type": "choix", "note": "Selon la composition du groupe : 3 boîtes de Minimi s'il y a un mitrailleur, 6 grenades de 40 mm s'il y a un grenadier, 3 chargeurs de SCAR-H s'il y a un tireur de précision. Sans spécialiste, on remplace par 6 chargeurs de fusil de plus.", "pieces": ["100Rnd Minimi BO", "100Rnd Minimi BO/BT", "AMF 40mm HE M406", "AMF HEDP M433", "Chargeur SCAR-H (7,62 × 51 mm OTAN)"]},
    {"slot": "Réserve médicale", "type": "impose", "note": "Pour l'infirmier du groupe, qui vient se servir : 4 solutions salines et 8 bandages.", "pieces": ["Solution saline", "Bandage"]},
    {"slot": "Chargeurs d'arme de poing", "type": "choix", "note": "2 chargeurs pour le groupe.", "pieces": ["Chargeur Glock 9x19 17 cps", "Chargeur 15 balles M9 9 × 19 mm"]},
    {"slot": "Gilet", "type": "impose", "pieces": ["Gilet de Combat SMB - Grenadier Voltigeur (Fusilier)"]}
   ]
  },
  {
   "nom": "Grenadier (lance-grenades)", "sigle": "LG",
   "quand": "Le fusil équipé du lance-grenades HK269 et ses munitions de 40 mm.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "pieces": [["HK416 F-S HK269", "★ HK416 F-S HK269 — recommandé"], "HK416 F-C HK269"]},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300_HK, "pieces": CH_HK},
    {"slot": "Grenades de 40 mm", "type": "impose", "note": "8 explosives au maximum, HE ou HEDP, et 3 fusées éclairantes ou de signalisation.", "pieces": ["AMF 40mm HE M406", "AMF HEDP M433", "Fusée blanche M583A1 40 mm", "Fusée verte M661 40 mm", "Fusée rouge M662 40 mm"]},
    {"slot": "Gilet et sac", "type": "impose", "pieces": ["Gilet de Combat SMB - Grenadier Voltigeur (Fusilier)", "TECPACK 45L Grenadier"]}
   ]
  },
  {
   "nom": "Mitrailleur", "sigle": "MG",
   "quand": "L'appui-feu du groupe. Une seule mitrailleuse par groupe, deux sur ordre.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "La Minimi en 5,56 mm est l'arme du mitrailleur de groupe. La MAG 58 en 7,62 mm se prend sur ordre du chef de groupe, pour un appui lourd.", "pieces": [["FN Minimi MK3", "★ FN Minimi MK3 — recommandé"], "FN Minimi F1", "FN MAG58"]},
    {"slot": "Munitions", "type": "impose", "note": "Minimi : 5 boîtes de 100. MAG 58 : 2 bandes de 200. Un fusilier peut porter une boîte de plus.", "pieces": ["100Rnd Minimi BO", "100Rnd Minimi BO/BT", "200Rnd MAG 58 7.62X51mm 4BO/1TRC"]},
    {"slot": "Optique", "type": "choix", "pieces": ["Scrome J4 F1 Minimi F1", "Elcan Specter", "Aimpoint CompM5"]},
    {"slot": "Gilet et sac", "type": "impose", "pieces": ["Gilet de Combat SMB - Mitrailleur", "TECPACK 45L Minimi"]}
   ]
  },
  {
   "nom": "Tireur de précision", "sigle": "TP",
   "quand": "Le tireur du groupe, et le binôme de tir longue distance avec la PGM.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Le SCAR-H PR est l'arme du tireur de précision de groupe. La PGM Hécate II est une arme anti-matériel : sur ordre uniquement, en binôme avec un observateur.", "pieces": [["FN SCAR-H PR", "★ FN SCAR-H PR — recommandé"], "FR-F2", "PGM Hécate II Polymer RIS", "PGM Hécate II Bois RIS"]},
    {"slot": "Munitions", "type": "impose", "note": "SCAR-H : 10 chargeurs. FR-F2 : 8 chargeurs. PGM : 5 chargeurs.", "pieces": ["Chargeur SCAR-H (7,62 × 51 mm OTAN)", "10Rnd FRF2 BO M62", "10Rnd FRF2 AP", "7Rnd 12.7X99 PGM Hecate II PEI"]},
    {"slot": "Lunette", "type": "choix", "note": "Les optiques au-dessus de x4 sont réservées à cette certification.", "pieces": ["Schmidt & Bender 3-20x50", "Schmidt & Bender 8X24 Mildot CC", "Scrome J8", "Scrome J10"]},
    {"slot": "Observation", "type": "impose", "note": "Un télémètre pour le tireur ou son observateur.", "pieces": ["Leica Vector IV"]},
    {"slot": "Aides au tir", "type": "impose", "note": "Réservées à cette certification. Les tables donnent la correction à afficher sur la lunette, réglable en hausse et en dérive ; l'anémomètre donne le vent et la température.", "pieces": ["Tables balistiques (mrad)", "Anémomètre Kacetrel 4500"]},
    {"slot": "Gilet et sac", "type": "impose", "note": "Le gilet correspond à l'arme emportée.", "pieces": ["Gilet de Combat SMB - Tireur de précision SCAR-H", "Gilet de Combat SMB - Tireur d'élite PGM Hecate", "TECPACK 30L."]}
   ]
  },
  {
   "nom": "Infirmier et médecin", "sigle": "MEDIC",
   "quand": "Le soignant du groupe. Il se bat comme un fusilier, mais sa priorité est de garder les autres en vie. L'infirmier arrête le sang, perfuse, relance le cœur et réveille ; le médecin ajoute les gestes invasifs et les médicaments du cœur. Ce matériel pris à l'arsenal sans la certification est une infraction.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Le fusil du fusilier, avec ses 300 cartouches ; le matériel médical prend le reste de la place.", "pieces": FUSIL},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300, "pieces": CH_HK + CH_FAMAS},
    {"slot": "Infirmier : arrêter le sang", "type": "impose", "note": "Pour une escouade de 8 : 12 bandages, 4 garrots, 4 pansements thoraciques, 8 solutions salines. Une poche rend un quart du sang : il en faut 2 à 3 pour remettre debout un blessé grave. Chaque soldat porte aussi la sienne, et la Mule garde une réserve.", "pieces": ["Bandage", "Garrot", "Pansement thoracique", "Solution saline"]},
    {"slot": "Infirmier : relancer et réveiller", "type": "impose", "note": "4 morphines, 4 épinéphrines pour l'arrêt cardiaque (avec le massage), 3 carbonates d'ammonium pour réveiller un inconscient, 1 naloxone en cas de surdose de morphine, 1 masque à oxygène.", "pieces": ["Morphine auto-injectable", "Épinéphrine", "Carbonate d'ammonium (sels)", "Naloxone", "Masque à oxygène"]},
    {"slot": "Médecin : en plus", "type": "impose", "note": "Réservé à la certification Médecin, en plus de tout le matériel de l'infirmier : 1 kit médical, 2 tubes King LT pour garder les voies aériennes libres, 2 kits d'exsufflation pour le pneumothorax sous tension, 2 métoprolols (cœur trop rapide), 2 phényléphrines (tension trop basse).", "pieces": ["Kit médical", "Tube laryngé King LT", "Kit d'exsufflation (NCD)", "Métoprolol", "Phényléphrine"]},
    {"slot": "Fumigènes", "type": "choix", "note": "2 fumigènes blancs pour couvrir une évacuation.", "pieces": ["GR MA FUM Blanche"]},
    {"slot": "Gilet et sac", "type": "impose", "pieces": ["Gilet de Combat SMB - Auxiliaire sanitaire", "TECPACK 45L AuxSan"]}
   ]
  },
  {
   "nom": "Chef de groupe et officier", "sigle": "CDG",
   "quand": "Celui qui commande. Son arme est sa radio et sa carte.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Le fusil du fusilier, ou la version courte.", "pieces": FUSIL + ["HK416 F-C"]},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300, "pieces": CH_HK + CH_FAMAS},
    {"slot": "Observation", "type": "impose", "pieces": ["Leica Vector IV", "Thales OB 72 Sophie", "APX M241"]},
    {"slot": "Signalisation", "type": "impose", "note": "Les fumigènes de couleur sont réservés au chef de groupe et à l'opérateur radio : vert pour ami, rouge pour ennemi, jaune pour évacuation.", "pieces": ["GR MA FUM Vert", "GR MA FUM Rouge", "GR MA FUM Jaune"]},
    {"slot": "Gilet et sac", "type": "impose", "pieces": ["Gilet de Combat SMB - Chef de groupe", "TECPACK 45L"]}
   ]
  },
  {
   "nom": "Opérateur radio", "sigle": "OR",
   "quand": "Le lien avec la base et les autres groupes. Il reste à portée de voix du chef de groupe.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Version courte recommandée, le sac radio est encombrant.", "pieces": [["HK416 F-C", "★ HK416 F-C — recommandé"], "HK416 F-S"]},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300_HK, "pieces": CH_HK},
    {"slot": "Radio longue portée", "type": "impose", "note": "Le Tecpack avec la radio ER314 est réservé à cette certification.", "pieces": ["Tecpack 45L ER314"]},
    {"slot": "Signalisation", "type": "choix", "pieces": ["GR MA FUM Vert", "GR MA FUM Rouge", "GR MA FUM Jaune"]},
    {"slot": "Gilet", "type": "impose", "pieces": ["Gilet de Combat SMB - Grenadier Voltigeur (Fusilier)"]}
   ]
  },
  {
   "nom": "Logisticien et conducteurs", "sigle": "LOGI · VL · PL",
   "quand": "Ceux qui roulent, ravitaillent et réparent.",
   "lignes": [
    {"slot": "Arme principale", "type": "impose", "note": "La version courte, plus pratique en cabine.", "pieces": ["HK416 F-C"]},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300_HK, "pieces": CH_HK},
    {"slot": "Outils", "type": "impose", "pieces": ["Clé de réparation", "Récipient à carburant"]},
    {"slot": "Gilet et sac", "type": "impose", "note": "Le 70 L pour transporter du matériel.", "pieces": ["Gilet de Combat SMB - Grenadier Voltigeur (Fusilier)", "TECPACK 70L"]}
   ]
  },
  {
   "nom": "Équipage blindé et pilote", "sigle": "BLINDE · PILOTE",
   "quand": "À bord d'un engin, on emporte le minimum.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "La version courte. Le pilote peut partir avec la seule arme de poing de la dotation commune.", "pieces": ["HK416 F-C"]},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300_HK, "pieces": CH_HK},
    {"slot": "Outils", "type": "impose", "note": "Pour l'équipage blindé.", "pieces": ["Clé de réparation"]},
    {"slot": "Casque et gilet", "type": "impose", "note": "Voir « Équipages et pilotes » dans le guide d'habillement.", "pieces": []}
   ]
  },
  {
   "nom": "Sapeur et antichar", "sigle": "GENIE · AT",
   "quand": "Le sapeur ouvre la route et protège la position : il détecte et neutralise les mines, enterre les siennes, creuse et fortifie. La partie antichar reste en attente : l'arsenal ne contient pas encore de lance-roquettes ni de charges explosives.",
   "lignes": [
    {"slot": "Arme principale", "type": "choix", "note": "Le fusil du fusilier, avec ses 300 cartouches ; le matériel du sapeur prend le reste de la place.", "pieces": FUSIL},
    {"slot": "Munitions", "type": "impose", "note": NOTE_300, "pieces": CH_HK + CH_FAMAS},
    {"slot": "Déminage", "type": "impose", "note": "Réservé à la certification Sapeur. Le détecteur repère les mines et tout objet métallique enterré ; l'interrupteur homme mort se relie aux explosifs de l'inventaire.", "pieces": ["Détecteur de mines VMM3", "Interrupteur homme mort"]},
    {"slot": "Terrassement", "type": "impose", "note": "La pelle sert à enterrer et déterrer une mine, à creuser une tranchée individuelle ou un abri pour véhicule, et à couper arbres et buissons. Le conteneur apporte les sacs de sable d'un poste de contrôle.", "pieces": ["Pelle M1967", "Conteneur de sacs de sable"]},
    {"slot": "Franchissement", "type": "choix", "note": "Sur ordre, quand la mission prévoit d'entrer par le haut ou de franchir un mur.", "pieces": ["Échelle télescopique"]},
    {"slot": "Gilet et sac", "type": "impose", "pieces": ["Gilet de Combat SMB - Grenadier Voltigeur (Fusilier)", "TECPACK 45L"]}
   ]
  }
 ],
 "interdits": [
  "PA MAC 50 pour un soldat qui n'est pas officier.",
  "AAN-F1 : pièce de musée, hors cérémonie ou scénario.",
  "Toute arme livrée avec une optique ou en couleur beige : on prend l'arme nue et on monte son optique.",
  "Toute optique de grossissement supérieur à x4 sans la certification Tireur de précision : Schmidt & Bender, Scrome J8 et J10.",
  "Chargeurs PMAG TAN : chargeurs noirs uniquement.",
  "Chargeur M14, bandes M60 et M249 : munitions d'armes absentes de l'arsenal.",
  "Montre classique et boussole : remplacées par la montre GPS Tactix Bravo, obligatoire pour tous.",
  "Munitions de 12,7 mm de véhicule : elles restent dans les engins.",
  "Cumuler deux armes principales, ou deux spécialités sur la même mission."
 ]
}
g['armement']['interdits'] = []  # plus de section Interdit : ce qui n'est pas dans le guide est interdit
json.dump(g, io.open(p, 'w', encoding='utf-8'), ensure_ascii=False, indent=1)
print('armement :', len(g['armement']['blocs']), 'fiches')
