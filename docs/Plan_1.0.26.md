# Plan global 1.0.26 (22 septembre 2026)

Ordre par gravité : #57 → #53 → #55 → #58 → #54 → #56 → #52 + #59. Tout dans la même version. Mods tiers figés jusqu'à la publication. Compilation par lot, journal à m'envoyer. Documents (guide, PDF, site, bot) mis à jour en bloc à la publication, une seule annonce. Pas de remise à zéro des inventaires ni de la campagne.

## Points à confirmer avant que je code (contradictions relevées)

1. **Kit gratuit au spawn** (HK416F-S + 10 chargeurs, à chaque mort, gratuit) contre **#59** (les chargeurs consomment la soute) et **A14** (conformité vérifiée par le CDG). Un mort qui réapparaît reçoit 10 chargeurs sans toucher la soute ; seul le rééquipement volontaire coûte. C'est acceptable ? Et un joueur qui préfère le FAMAS repose le HK416 à l'arsenal (remboursé 0 puisque les armes ne coûtent plus rien).
2. **Contrôle routier « un par section »** : aujourd'hui un seul contrôle actif sur tout le serveur. Plusieurs contrôles simultanés, un par groupe de joueurs, c'est bien ce que tu veux ?
3. **Mule « augmenter »** : combien de chargeurs de groupe (16 ? 20 ?) au lieu de 12.
4. **Livraison 30 % ou liée à la menace** : je propose 10 % + 3 % par point de menace (max 40 %).
5. **Journaux du serveur** : tu as dit les récupérer (G28). Envoie-moi script.log, console.log et le journal SimpleRP du 21/09, ils servent à #57 et #55.

## #57 Arme bloquée et plantage (gravité 1)

Diagnostic retenu : l'arme perd son **mode de tir** côté client (pas un enrayage ACE : aucun indice, case vide, arrive aussi en changeant un accessoire et après reconnexion, persiste après avoir lâché l'arme). Le plantage est un défaut moteur au dépôt AU SOL d'un objet venant du stockage de l'arme ; vers le sac, ça ne plante pas.

Décisions : enrayage coupé, Maj + R protégé avec message, message en jeu quand l'arme est hors service, action Staff « recréer l'arme », consigne « chargeur vers le sac, jamais au sol », test à deux sur le serveur, 10 surcharges d'accessoires publiées, carte gardée entière.

Travail :
- `Configs/ACE/Settings.conf` : `m_fJamChanceScale 0` (surchauffe, fumée et dispersion gardées).
- `SRP_AceCompat.c` : surcharge de `ACE_Overheating_TryClearJam` : si le composant est introuvable, message « Arme hors service : changez d'arme puis reprenez-la » et sortie propre (plus d'exception, plus de chargeur jeté).
- `SRP_PlayerController.c` (client) : toutes les 2 s, si l'arme en main n'a plus de mode de tir : encart « Arme hors service. Ne posez pas le chargeur au sol : mettez-le dans le sac. Le Staff peut recréer l'arme. » + une ligne de journal avec prefab, bouche, mode de tir, chargeur, accessoires (la sonde qui manquait).
- `SRP_Admin.c` : bouton Rapide « Recréer l'arme de <joueur> » : supprime l'arme en main, recrée le même prefab avec les mêmes accessoires et un chargeur plein.
- Publier les 10 surcharges `Prefabs/Weapons/Attachments/…` (déjà écrites).
- Protocole de test à deux : HK416F-S + poignée RVG, 100 tirs, changer la poignée, poser un chargeur vers le sac puis au sol ; même chose avec un M16A2 du jeu.

## #53 Soins (gravité 2)

Diagnostic : le poste de secours n'a **jamais** eu de menu d'action sur les lits (tous les joueurs, depuis le début). Le prefab `SRP_PosteDeSecours.et` hérite d'un lit avec le maillage désactivé : sans maillage, pas de collision, donc l'action n'est jamais atteignable. Le `.meta` pointe en plus un mauvais dossier.

Décisions : tout le monde peut se soigner, toujours ; remise à neuf complète ; blessé inconscient déposé sur le lit soigné aussi ; Discord moins bavard ; soin Staff peut ressusciter un mort ; réveil spontané possible ; les IA continuent de se soigner ; déconnecté inconscient = le corps reste 15 min ; état ACE d'un joueur dans le panneau Staff Discord ; parcours : soigné sur place, évacué à la base si trop grave.

Travail :
- Refaire `SRP_PosteDeSecours.et` : lit visible (BedMedical_01), contexte d'action sur le matelas, `m_bRefuseIfMedicOnline 0` ; corriger le `.meta` ; vérifier la pose dans `hopital.layer`.
- `SRP_Medical.c` : seconde action « Soigner le blessé allongé » : soigne le personnage inconscient à moins de 2 m du lit (déposé par ACE Carrying) ; succès journalisé, plus de message Discord public (journal Staff seulement).
- `SRP_Admin.c` + pont + bot : « soigner » relève aussi un inconscient (FullHeal + réveil) et fait réapparaître un mort à l'endroit du corps avec son inventaire ; nouvelle commande « etat <joueur> » (sang, conscience, saignements, pouls) affichée par un bouton du panneau Staff.
- `Missions/SimpleRP_Everon.conf` : `m_fSecondChanceResilienceRegenScale 0.5` (réveil spontané lent).
- `SRP_Disconnect.c` : `m_bUnconsciousIsDeath 0`.
- Guide : le poste est ouvert à tous (le texte « réservé au Médecin » disparaît).

## #55 Contrôle routier (gravité 3)

Constat : point sous les pieds, action facile, voitures annoncées arrivent ; mais voitures ambiantes qui traversent sans conséquence, demi-tours après « repartir », conducteur téléporté puis immobile, « descendre » = fuite immédiate impossible à arrêter sans tuer, pas de mains en l'air, serflex sans effet, 30 voitures qui s'entassent.

Décisions : file d'attente à deux voies ; voitures ambiantes prises dans le contrôle ; demi-tours corrigés ; le conducteur remonte en marchant ; mains en l'air quand on le vise ; serflex fonctionnels ; « Arrêter » = menotté ; le fuyard peut être blessé, attaché et soigné ; pas d'amende pour un suspect révélé ; rayon d'abandon 300 m ; primes payées même en échec ; véhicules retirés après 10 min sans joueur, désamorçage des explosifs, voiture gardée si un joueur est dedans ; droits inchangés ; papiers faux = suspect certain, mais un suspect peut avoir des papiers en règle ; 20 min ; chat local + journal à chaque changement d'état ; un contrôle par section ; tout, journal d'abord.

Travail (`SRP_Checkpoint.c`, `SRP_Civilians.c`, `SRP_Missions.c`) :
- Journal CONTROLE à chaque état (approche, arrêt, repart, force, fuit, hostile, rendue) avec la distance au point.
- File d'attente : deux files (une par sens), une voiture « au point » par file, les suivantes s'arrêtent 40 m derrière et avancent quand la place se libère ; apparition seulement quand une file a moins de 2 voitures ; circulation ambiante coupée à 1 km du point pendant le contrôle.
- Voitures ambiantes déjà en route : adoptées et arrêtées comme les autres ; celle qui ne s'arrête pas passe « force le barrage » (annonce + cible légitime, malus).
- « Repartir » : ordre de conduite vers une destination au-delà du point, jamais derrière ; le conducteur marche jusqu'au véhicule (GetIn) au lieu d'être téléporté ; relance si immobile 20 s.
- « Descendre » : plus de fuite automatique ; la fuite ne se déclenche que si aucun joueur ne le vise à moins de 15 m. Braqué : mains en l'air (ACE Captives, état « rendu »), il s'arrête ; « Arrêter » le menotte (état ACE) ; serflex du joueur acceptés sur un civil rendu ; blessé sans être tué : il s'arrête, peut être soigné puis attaché.
- Crime de guerre : `IsLegitimateTarget` élargi aux suspects révélés (fuyard, cargaison trouvée, force annoncée ou non).
- Abandon : 300 m ; échec : primes des saisies et arrestations versées.
- Nettoyage : véhicules des morts, hostiles et explosifs retirés après 10 min sans joueur à 100 m ; action « Désamorcer » (soldat formé, 30 s) ; véhicule gardé si un joueur est dedans.
- Un contrôle par groupe de joueurs (plus de verrou unique serveur).

## #58 IA ennemie (gravité 4)

Constat : garnisons hors des villages (rayon 2 km), groupes immobiles, trop en campagne et pas assez dans les villages, ennemi trop aveugle.

Décisions : rayon de secteur découplé du rayon civil (hameau 150, village 250, bourg/ville 350) ; 30 secteurs gardés ; Defend_Large avec postes ; rondes + postes fixes ; garde de mission 100 m ou selon la mission ; distance de pose selon le chemin, hors de vue obligatoire, relief ou lisière, sur route ; rayon interdit 1 500 m autour de la base pour tout ; délai de grâce à la reconnexion ; garnisons posées à 2 000 m, retirées à 3 500 ; nouvelle garnison quand plus personne ; assaillants convertis en garnison ; priorité au secteur des joueurs ; contre-attaque sur le secteur le plus proche des joueurs, par la route, sur deux axes échelonnés ; 1,5 soldat par joueur ; groupes au hasard gardés ; patrouille ambiante qui vient vers le joueur ou suit une route voisine ; fréquence du prefab + liée à la menace ; routes puis champs à l'approche ; renfort du voisin + vagues sur alerte ; poursuite courte ; motorisés débarquent sur route, véhicule selon l'effectif ; convoi débarque au contact ; vagues sur présence et alerte ; menace → fréquence, vigilance, renforts, baisse par missions et défense ; gardes de sabotage/écoute gardées avec poste sur la pièce ; survivants restent 15 min ; livraison liée à la menace ; lampes la nuit ; perception moins aveugle ; reddition des ennemis ; vue Staff hors périmètre ; réglages d'abord, puis pose ; patrouilles du jeu pour les garnisons ; alerte, hélico, prorata, jeep, convoi gardés ; mode essai gardé avec marqueur [ESSAI].

Travail en deux temps :
1. Réglages (`SRP_GameMode.et`, `SRP_Territory.c`) : rayon de secteur par type, point Defend_Large avec presets, `m_fCRXHoldRadius` 100 et par mission, garnison 2000/3500, `m_fBaseSafeRadius` 1500 appliqué à tous les chemins, perception relevée, marqueur [ESSAI].
2. Code (`SRP_Enemy.c`, `SRP_Territory.c`, `SRP_Missions.c`) : brique commune de pose (distance par chemin, ligne de vue par joueur, relief/lisière, route via RoadNetworkManager) partagée avec #55 ; garnisons par `SCR_AmbientPatrolSpawnPointComponent` du jeu (rondes Cycle sur les rues + Defend_Large) ; nouvelle garnison quand personne à moins de 1 500 m ; conversion des assaillants ; contre-attaque ciblée, par la route, deux axes ; ambiante avec second trajet sur route ; fréquence × (1 + menace/10) ; renfort du voisin et vagues sur alerte ; poursuite 300 m ; débarquement sur route et véhicule selon l'effectif ; convoi qui débarque ; survivants 15 min ; livraison 10 % + 3 %/menace ; reddition à 1-2 hommes ; menace en baisse : poste d'observation, officier, sabotage réussis (−1), contre-attaque repoussée (−1).

## #54 Lumières (gravité 5)

Décisions : ajout (jamais eu de lumière) ; intérieur des bâtiments, tous ; allumable à la main ; néons, lumière chaude, mâts ; base visible assumée ; décor indestructible + générateur (coupé ou détruit = noir) ; tu poses les lampes au Workbench dans un calque dédié ; autant qu'il faut ; rythme gardé ; lampe et JVN libres, listées dans la dotation ; pas dans le kit ; lumière aussi aux points de livraison et de contrôle ; mode nuit GM pour le Staff.

Travail :
- Je te fournis la liste des prefabs à poser (plafonniers LightCeiling_01_on, néons LightIndustrial_02_on_interior, mâts LightTower_01, projecteurs GeneratorFloodlight_US_01) et la règle : lampes interactives en état LIT. Tu crées `BaseFR/Eclairage.layer`.
- `SRP_Generateur.c` : composant sur un GeneratorPortable posé au garage ; action « Démarrer / Couper » ; allume ou éteint toutes les lampes interactives à moins de 300 m de la base ; détruit = noir jusqu'à réparation (action « Réparer », logisticien) ; état sauvegardé.
- Lanterne posée par script la nuit sur les points de livraison actifs et le point de contrôle.
- `SRP_GameMode.et` : composant mode nuit du Game Master.
- Guide des dotations : lampe et JVN listées.

## #56 FAMAS F1 à rail (gravité 6)

Décisions : rail + optique montée ; F1 classique + rail ; réactiver « FAMAS F1 RIS » ; rails PGM et PGMP aussi en objets ; collimateurs et ×4 ; tous les ayants droit au F1 ; F1 nu et F1 rail gardés ; nom AMF ; arme nue ; tout gratuit.

Travail : `classes.json` (F1 RIS → garder) puis `arsenal_retirer.py` et `arsenal_appliquer.py` ; entrées catalogue pour les deux rails ; fiche FUSIL du guide ; surcharges de réplication des rails publiées avec #57.

## #52 + #59 Dotation et arsenal (gravité 7)

Décisions : 10 chargeurs HK416 / 12 FAMAS, pleins ; tous les rôles à 10 (12 FAMAS) ; mule augmentée (nombre à fixer) ; piège FGI retiré ; CDG et OR à 300 ; munitions libres ; une partie dans le sac, poche chargeur AMF officialisée ; Minimi 5 boîtes, SCAR-H 10, pistolet 3 ; annonce Discord ; conformité par le CDG ; guide fonctionnel corrigé. Arsenal : chargeurs, grenades, roquettes facturés ; 1 paquet par chargeur ; armes et médical à 0 ; stock inchangé. Kit gratuit au spawn : HK416F-S + 10 chargeurs.

Travail : `ajout_armement.py` puis la chaîne guide → PDF → site ; `SRP_ArsenalEconomy.c` (coûts) ; `SRP_GameMode.et` `m_aKitDeBase` ; GUIDE_SIMPLERP.md (kit, munitions, vivres).

## Transversal retenu

Effectif de 2 à 40 joueurs (tout doit tenir). Plafond global d'IA, civils prioritaires. Contrôle routier autorisé partout hors base. Rien de nouveau vers Discord et la carte web. Certifications inchangées. Pas de mesure CPU. Cartes gardées entières. Nested Attachments absent du serveur.
