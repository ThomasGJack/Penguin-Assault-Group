# SimpleRP — Guide complet du mod

**Serveur** : GOAF (Groupe Opérationnel Armé Français), milsim RP armée française sur Everon.
**Jeu** : Arma Reforger 1.8.0.13. **Mod** : SimpleRP, 33 scripts Enforce, 3 layouts, 4 fichiers de configuration, 1 override de prefab, 2 projets audio.
**Ce document** : tout ce qui a été construit, comment l'installer depuis un Workbench vide, chaque paramètre, chaque commande, chaque fichier, les leçons apprises, ce qui reste à tester et à faire. Les notices détaillées de chaque phase sont en annexe (`annexes/`).

---

## Sommaire

1. Ce que fait le mod, en une page
2. Architecture
3. Installation depuis zéro (Workbench)
4. Le game mode et ses composants (tous les paramètres)
5. Les prefabs à créer
6. Les fichiers de configuration, layouts, overrides, audio
7. Les fichiers du profil (sauvegardes)
8. Toutes les commandes de chat
9. Les systèmes, un par un
10. Discord
11. Leçons Enforce et Workbench (le cahier des pièges)
12. État, tests à faire, feuille de route

---

## 1. Ce que fait le mod, en une page

Un joueur arrive, accepte la **charte**, spawn à la base en Recrue avec l'équipement de dotation. Un instructeur lui donne sa **FGI** (il passe Soldat de 2e classe), puis des **certifications** (conducteur VL/PL, blindé, pilote, infirmier, médecin, sapeur, logisticien, opérateur radio, MG, LG, AT, tireur de précision, chef de groupe) selon un **arbre de prérequis** ; les **grades** (dix échelons, du Soldat de 2e classe au Capitaine) se gagnent aux heures de jeu et aux missions réussies, sur décision d'officier. Tout ça se fait **au contact**, en regardant le soldat (action « Gestion du soldat »), ou au chat.

La base vit sur quatre **ressources en paquets physiques** d'un kilo (munitions, carburant, pièces, vivres), stockés dans quatre **dépôts** ouvrables ; le respawn coûte 2 vivres, la citerne du carburant, l'atelier des pièces, l'intendance des vivres au fil du temps, l'arsenal des paquets de munitions. Un **coffre** en euros finance les achats au **terminal** (véhicules et caisses du **catalogue**), livrés sur des **points de livraison** parfois tenus par l'ennemi. Le **parc automobile** est persistant : chaque véhicule a un numéro, un état, une position, un contenu.

Seize types de **missions** sont générés en continu (caches, cargaisons, convois, officiers, mines, épaves, colis médical, documents, écoute, village à libérer…), avec prise en charge par un chef, récompenses en euros et en caisses. Le **territoire** découpe la carte en secteurs (les villages, automatiquement) tenus par l'ennemi : garnison à l'approche, capture en tenant la zone, contre-attaques annoncées, production horaire de paquets à aller chercher, niveau de menace. L'**IA ennemie** est réglée par CRX (tir à vue au contact, enquête au bruit, retour en position) ; les **civils** animent villages et routes et réagissent aux combats (fuite, plongeon, cachette).

Le **PC** (un ordinateur) ouvre un écran à onglets : effectifs, fiche, instructeurs, missions, territoire, journal, base, officiers. La **voix** se module (F9 : chuchoté 5 m, normal 30 m, fort 68 m). Le **jour** dure six heures réelles et l'heure est persistante. Un joueur qui se **déconnecte** laisse son corps quinze minutes ; parti trop longtemps loin de la base, il est mort. Le Staff a ses outils (menu, bans persistants, avertissements, wipe), et tout est tracé dans un **journal** quotidien.

---

## 2. Architecture

**Principe** : un game mode à nous (`SRP_GameMode.et`, hérité de `SCR_BaseGameMode`), zéro dépendance au mode Conflict, tout l'état dans des JSON du profil serveur. Chaque système est un **composant** posé sur le game mode ; les objets du monde (dépôts, coffre, terminal, PC, zones, secteurs, points de livraison) sont des **prefabs** porteurs d'un composant ou d'une action. Les joueurs commandent par le **chat** (`/…`), par des **actions au contact** (touche F) et par des **écrans** (menus à liste ou à onglets).

**Réseau** : la logique tourne sur le serveur ; les écrans reçoivent leur contenu par RPC (`SRP_PlayerController`, une chaîne de texte structurée en lignes `M|`, `H|`, `L|`, `I|`, `T|`, `A|`) et renvoient le choix par une commande. Les marqueurs de carte sont des marqueurs statiques vanilla (`SCR_MapMarkerManagerComponent`).

**Persistance** : chaque composant sauvegarde son fichier (sale → 30 s ; filet de sécurité périodique ; à l'arrêt). Règle apprise à la dure : à l'arrêt, on écrit le **dernier état connu**, jamais un recomptage ; et une instance qui n'a **pas chargé** n'écrit jamais (le Workbench crée une instance jetable au lancement).

### Les 33 scripts (`Scripts/Game/SimpleRP/`)

| Fichier | Rôle |
|---|---|
| SRP_Definitions.c | grades, certifications et leur arbre (`SRP_Certifs`, `SRP_CertifTree`, `SRP_GradeRule`), ressources, chemins du profil (`SRP_Paths`), utilitaires (`SRP_Utils`, `SRP_Time`) |
| SRP_PlayerRecord.c | la fiche joueur (JSON par identité) : nom, grade, certifs, habilitations, temps de jeu, missions, infractions, argent porté, position, inventaire, charte, mort en absence |
| SRP_PlayerManagerComponent.c | identité Bohemia, fiches en mémoire, grades et certifs avec règles, instructeurs, bannis à l'enregistrement, avertissements, menu Gestion du soldat, départ/retour |
| SRP_JournalComponent.c | journal horodaté (`journal/journal_AAAA-MM.log`), tampon des 80 dernières lignes pour le PC |
| SRP_SpawnLogic.c | spawn à la base (plusieurs marqueurs), reprise de position et d'inventaire, dotation, coût en munitions, mort en absence |
| SRP_PlayerController.c | RPC : chat vers serveur, ouverture des écrans, téléportations, caméra de mort |
| SRP_DeathCamera.c | caméra de mort |
| SRP_ChatCommands.c | toutes les commandes `/…` (client → serveur) |
| SRP_TerminalMenu.c | mécanique commune des écrans à liste (`SRP_ListMenu`), menu terminal, presets de menus |
| SRP_Admin.c | menu Staff (`/admin`) : téléportations, soin, invincible, heure, états ; `WipeServer` |
| SRP_ArsenalEconomy.c | facturation de l'arsenal Bacon en paquets de munitions (chargeurs, roquettes, grenades ; armes et médical gratuits), crochet d'inventaire (paquets exclus, missions) |
| SRP_BaconCompat.c | masque le dialogue Bacon |
| SRP_Catalogue.c | classes du fichier `SRP_Catalogue.conf` (véhicules, caisses) |
| SRP_Treasury.c | coffre en euros, sacoches de butin, crime de guerre, terminal d'achat, livraisons en attente |
| SRP_Fleet.c | parc automobile persistant (numéros, état, position, cargaison en paquets), signature |
| SRP_Garage.c | citerne (plein, jerricans), atelier (réparation), signature de véhicule |
| SRP_Delivery.c | points de livraison, emprise ennemie, garnisons, lanterne la nuit sur une livraison en cours |
| SRP_Packet.c | le paquet physique (`SRP_PacketComponent`), insertion/comptage/retrait dans les conteneurs, gestionnaire de conteneur |
| SRP_ResourceManagerComponent.c | les quatre ressources en paquets, dépôts, regarnissage, consommation, caisses de livraison, sauvegarde |
| SRP_Depot.c | le dépôt (`SRP_DepotComponent`), consultation, gestionnaire d'inventaire de conteneur |
| SRP_Medical.c | poste de secours (soin complet réservé au Médecin) |
| SRP_Enemy.c | socle IA ennemie : prefabs de groupes, spawn réparti, plafond, présence ambiante, comptages |
| SRP_CRX.c | réglages CRX par rôle (garnison, assaut, patrouille, civil), vitesse de groupe |
| SRP_Missions.c | seize types de missions, générateur, prise en charge, récompenses, tableau, marqueurs |
| SRP_Civilians.c | ambiance civile : zones automatiques et manuelles, promeneurs, postés, voitures, réflexes de danger |
| SRP_Territory.c | secteurs, garnisons, capture, contre-attaques, production, menace, marqueurs clignotants |
| SRP_Charte.c | charte : texte du profil, écran, acceptation, périmètre du spawn |
| SRP_PC.c | terminal du PC : écran à onglets, vues serveur |
| SRP_Soldier.c | action « Gestion du soldat » et son menu (certifs, grades, formations, états de service) |
| SRP_VoiceRange.c | portée de voix modulable (F9) |
| SRP_DayNight.c | cycle jour/nuit accéléré, heure persistante, `IsNight()` |
| SRP_Generateur.c | éclairage de la base : générateur (allumé / éteint / en panne, `generateur.json`), lampes dans le rayon de la base, actions démarrer / couper / réparer, lanternes de nuit (`SpawnLantern`), mode nuit du Game Master, interrupteur des lampes refusé sans courant |
| SRP_Disconnect.c | corps qui reste à la déconnexion, mort en absence |
| SRP_Bans.c | bannissements persistants |

---

## 3. Installation depuis zéro (Workbench)

### 3.1 Le projet et ses dépendances

1. Arma Reforger Tools → Workbench → **Create new project** → nom `SimpleRP`. Le dossier est `Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\`.
2. Dépendances (Project settings → Dependencies), à charger dans cet ordre : **Bacon Loadout Editor** (arsenal), **AMF** (armée française : personnages, véhicules), **RHS AFRF** (faction ennemie et matériel), **ACE** (médical, explosifs, portage), **CRX Enfusion A.I.** (comportement IA). `ModularVoiceRange` était une dépendance de transition : à retirer une fois la voix validée à deux joueurs (§9.16).
3. Copie le contenu du zip dans le dossier du mod en respectant l'arborescence (`Scripts/`, `Configs/`, `UI/`, `Prefabs/`). Les fichiers `.meta` fournis se copient tels quels, jamais recréés dans le Workbench : ceux des `.conf` système portent le GUID du fichier vanilla (c'est ainsi que le jeu fusionne les menus, les entrées clavier), ceux des layouts et du prefab override portent nos GUID.

### 3.2 Le monde

4. **Monde** : `EveronSimpleRP` (copie du monde Everon vanilla, `Worlds/`). Entités indispensables :
   - `SRP_GameMode` (prefab à nous, §4) ;
   - `SCR_AIWorld` réglé comme celui du Conflict Everon (les deux `NavmeshWorldComponent`, piétons et véhicules, pointant vers les navmesh d'Everon), sinon aucune IA ne bouge ;
   - `PerceptionManager` ;
   - `SCR_MapMarkerManagerComponent` sur le game mode (marqueurs de carte) ;
   - `TimeAndWeatherManager` (déjà dans le monde vanilla).
5. **Marqueurs** (des `GenericEntity` nommés dans Object Properties → Name) :
   - `SRP_SpawnBase` : le spawn de la base et le point de référence de toutes les distances ; `SRP_SpawnBase1`, `SRP_SpawnBase2`… (jusqu'à 20) pour d'autres points de spawn, le mod prend un point libre ;
   - `SRP_Livraison` : point de livraison de secours (si aucun `SRP_PointLivraison` posé) ;
   - `SRP_Parc`, `SRP_Parc1`… : emplacements du parc automobile (livraison des véhicules).
6. **Objets de la base** (prefabs du §5) : les quatre dépôts, le coffre, le terminal, le PC, le tableau des missions, le poste de secours, la citerne, l'atelier, les panneaux.

### 3.3 Le prefab de game mode

7. Resource Browser → `SCR_BaseGameMode` (ou le game mode vanilla le plus simple) → Inherit → `SimpleRP/Prefabs/SRP_GameMode.et`. Ajoute les composants du §4 (« + » → catégorie **SimpleRP**), plus `SCR_MapMarkerManagerComponent`, et règle la **logique de spawn** : composant de respawn du game mode → Spawn Logic = `SRP_SpawnLogic`. Pose le prefab dans le monde (une seule instance).
8. Ctrl+S sur tout. Le monde se lance par F5 (play) ; le log s'affiche dans la Log Console (filtre `SRP`).

### 3.4 La routine de mise à jour des scripts

À chaque livraison : **fermer le Workbench**, remplacer les fichiers, relancer (jamais « Compile & Reload »), et vérifier qu'il n'y a **aucune ligne `SCRIPT (E)`** avant le F5. Le module `Game` doit dire `loaded`.

---

## 4. Le game mode et ses composants (tous les paramètres)

Tous sur `SRP_GameMode.et`. Les valeurs indiquées sont les défauts du code ; celles que tu as changées dans le Workbench priment (les tiennes connues : capture 15 min, garnison à 3 000 m, retrait à 3 000 m, perte 15 min, hameaux inclus, 100 secteurs, une journée = 3 h réelles).


### Gestion des joueurs — `SRP_PlayerManagerComponent`

Identités, fiches, grades, certifications, arbre, sauvegarde automatique.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_aStaffIdentities | `` | Identités Bohemia des membres du Staff (copie-les depuis le journal après une première connexion) |
| m_sCertifTreePath | `(à choisir)` | Arbre des certifications (fichier SRP_CertifTree.conf : prérequis et grade minimum par certif ; vide = règles par défaut du code) |
| m_bFgiRequiredForGrade | `1` | La FGI est exigée pour tout grade au-dessus de Recrue (le Staff peut forcer) |
| m_iAutosaveInterval | `300` | Intervalle de sauvegarde automatique, en secondes |

### Menu Staff — `SRP_AdminComponent`

`/admin` : téléportations, soin, invincible, heure, états des systèmes, wipe.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur du point de la base (téléportation « Retour à la base ») |
| m_iPageSize | `14` | Localités par page |

### Économie de l'arsenal — `SRP_ArsenalEconomyComponent`

Facturation des retraits à l'arsenal Bacon en **paquets de munitions** (la soute), remboursement des retours, zone d'arsenal. Depuis la 1.0.26, seuls les chargeurs, roquettes et grenades coûtent : les armes et le médical sont à 0 ; un chargeur vaut 1 paquet quelle que soit sa capacité (boîte de 100 comprise).

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Coûts* | | |
| m_iCostRifle | `0` | Coût d'un fusil, en paquets de munitions (0 = gratuit) |
| m_iCostHandgun | `0` | Coût d'un pistolet, en paquets (0 = gratuit) |
| m_iCostMachineGun | `0` | Coût d'une mitrailleuse, en paquets (0 = gratuit) |
| m_iCostSniper | `0` | Coût d'un fusil de précision, en paquets (0 = gratuit) |
| m_iCostRocketLauncher | `0` | Coût d'un lance-roquettes, en paquets (0 = gratuit) |
| m_iCostGrenadeLauncher | `0` | Coût d'un lance-grenades, en paquets (0 = gratuit) |
| m_iCostMagazine | `1` | Coût d'un chargeur, en paquets, quelle que soit sa capacité (une boîte de 100 = 1) |
| m_iCostRocket | `5` | Coût d'une roquette ou d'une munition unitaire (chargeur d'un coup), en paquets |
| m_iCostGrenade | `2` | Coût d'une grenade, en paquets |
| m_iCostMedical | `0` | Coût d'un consommable médical, en paquets (0 = gratuit) |
| *SimpleRP* | | |
| m_iAlertCooldownSeconds | `60` | Délai minimum entre deux alertes pour le même joueur et la même certification, en secondes |

### Ressources en paquets — `SRP_ResourceManagerComponent`

Les quatre ressources, les prefabs de paquets et de caisse, le regarnissage au démarrage, la consommation de vivres, les caisses posées au sol.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Paquets* | | |
| m_sPacketMunitions | `(à choisir)` | Paquet de munitions (prefab SRP_Paquet_Munitions) |
| m_sPacketCarburant | `(à choisir)` | Paquet de carburant |
| m_sPacketPieces | `(à choisir)` | Paquet de pièces |
| m_sPacketVivres | `(à choisir)` | Paquet de vivres |
| m_sBoxPrefab | `(à choisir)` | Caisse de livraison (prefab SRP_CaisseLivraison : conteneur ouvrable) |
| *SimpleRP* | | |
| m_iInitialPercent | `50` | Stock initial en % de la capacité des dépôts, au tout premier lancement |
| m_fVivresPerPlayerPerHour | `2` | Vivres (paquets) consommés par joueur connecté et par heure |
| m_iVivresPerRespawn | `2` | Vivres (paquets) débités à chaque respawn |
| m_iEmptyBoxMinutes | `10` | Une caisse de livraison vide est retirée après … minutes |

### Trésorerie — `SRP_TreasuryComponent`

Coffre en euros, catalogue, livraisons, butin, crime de guerre.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Trésorerie* | | |
| m_iInitialBalance | `5000` | Solde au premier lancement, en euros |
| m_bOfficersOnly | `1` | Achats réservés aux officiers |
| m_fShopRadius | `0` | Distance maximale au dépôt de pièces pour acheter, en mètres (0 = aucune limite, le terminal suffit) |
| *SimpleRP - Catalogue* | | |
| m_sCataloguePath | `(à choisir)` | Fichier catalogue (SRP_Catalogue.conf) : véhicules et caisses en vente, avec leurs prix |
| m_iDeliveryDelaySeconds | `300` | Délai de livraison des caisses, en secondes |
| m_sLivraisonName | `SRP_Livraison` | Marqueur de livraison de secours, utilisé seulement sans SRP_DeliveryComponent |
| *SimpleRP - Butin* | | |
| m_sMoneyPrefab | `(à choisir)` | Prefab de la sacoche (SRP_Sacoche : prop + SRP_MoneyComponent + action Ramasser l'argent) |
| m_sEnemyFactions | `USSR,FIA` | Clés des factions ennemies, séparées par des virgules |
| m_sCivilianFactions | `CIV` | Clés des factions civiles, séparées par des virgules |
| m_fLootChance | `0.5` | Probabilité qu'un ennemi tué porte une liasse (0 à 1) |
| m_iLootMin | `50` | Somme minimale d'une liasse de butin |
| m_iLootMax | `300` | Somme maximale d'une liasse de butin |
| m_iWarCrimeFine | `1000` | Amende pour la base quand un joueur tue un civil (crime de guerre, 0 = aucune) |

### Parc automobile — `SRP_FleetManagerComponent`

Véhicules persistants, livraison, capture de l'état toutes les 10 s, réparation.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_sParcPrefix | `(liste)` | Flotte de secours si aucun catalogue n'est réglé sur SRP_TreasuryComponent (sinon ignoré) |
| m_sLivraisonName | `SRP_Livraison` | Marqueur de livraison de secours (surplus du premier lancement, et véhicules commandés sans SRP_DeliveryComponent) |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur du centre de la base (pour l'état « à la base ») |
| m_fBaseRadius | `300` | Rayon autour du centre de la base dans lequel un véhicule est « à la base », en mètres |
| m_iTickSeconds | `10` | Période de surveillance (position, carburant, dégâts, destruction), en secondes |
| m_iSaveSeconds | `120` | Période de sauvegarde de flotte.json si quelque chose a changé, en secondes |
| m_fSpawnLift | `0.5` | Hauteur ajoutée au spawn pour ne pas traverser le sol, en mètres |
| m_iAlertCooldownSeconds | `60` | Délai minimum entre deux alertes pour le même joueur et la même catégorie, en secondes |
| m_iOrderDelaySeconds | `600` | Délai de livraison d'un véhicule commandé, en secondes |

### Points de livraison — `SRP_DeliveryComponent`

Choix du point, emprise ennemie, garnisons.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Points* | | |
| m_fClearRadius | `2000` | Un point n'est retenu que si aucun joueur n'est à moins de … mètres (sinon le plus éloigné) |
| m_sFallbackMarker | `SRP_Livraison` | Marqueur de secours si aucun point de livraison n'est posé |
| *SimpleRP - Garnison* | | |
| m_fEnemyChance | `30` | Chance qu'un point de livraison soit sous emprise ennemie, en % |
| m_iGroupsMin | `1` | Groupes minimum par garnison (prefabs et effectif : SRP_EnemyComponent) |
| m_iGroupsMax | `3` | Groupes maximum par garnison |
| m_fGarrisonRadius | `40` | Rayon de dispersion des groupes autour du point, en mètres |
| m_iGarrisonLifetimeMinutes | `180` | Une garnison que personne n'est venu voir est retirée après … minutes |
| m_bAnnounceEnemy | `0` | Dire aux officiers si le point est gardé (sinon ils le découvrent sur place) |

### IA ennemie (socle) — `SRP_EnemyComponent`

Prefabs de groupes, points de passage, plafond, mise à l'échelle, présence ambiante, réglages CRX.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Groupes* | | |
| m_aGroupPrefabs | `(à choisir)` | Prefabs de groupes ennemis (Prefabs/Groups, tirés au hasard) |
| m_sDefendWaypointPrefab | `(à choisir)` | Point de passage « Defend » (garnisons) |
| m_sMoveWaypointPrefab | `{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et` | Point de passage « Move » (patrouilles, vagues) |
| m_sEmptyGroupPrefab | `{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et` | Prefab de groupe IA vide (pour un personnage isolé : officier ennemi) |
| m_sFactionKey | `USSR` | Clé de faction donnée aux personnages ennemis isolés |
| m_iMaxGroups | `14` | Plafond global de groupes ennemis vivants (livraisons + missions + ambiance) |
| m_iPlayersPerExtraGroup | `4` | Un groupe de plus par tranche de … joueurs connectés (effectif adapté) |
| *SimpleRP - Ambiance* | | |
| m_fAmbientChance | `2` | Présence ambiante : chance par minute et par joueur qu'une patrouille apparaisse, en % (rare) |
| m_fMissionChance | `15` | Même chance quand le joueur est à moins de « distance d'une mission » d'une mission active, en % |
| m_fMissionRadius | `1500` | Distance d'une mission active pour la chance renforcée, en mètres |
| m_fSpawnMinDistance | `600` | Distance minimale d'apparition d'une patrouille par rapport aux joueurs, en mètres |
| m_fSpawnMaxDistance | `1200` | Distance maximale d'apparition, en mètres |
| m_fBaseSafeRadius | `1500` | Aucune patrouille à moins de … mètres de la base |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur de la base |
| m_iAmbientMax | `4` | Patrouilles ambiantes vivantes en même temps, maximum |
| m_iAmbientLifetimeMinutes | `45` | Une patrouille est retirée après … minutes |
| m_fDespawnDistance | `2500` | Une patrouille sans joueur à moins de … mètres est retirée |
| *SimpleRP - CRX* | | |
| m_bUseCRX | `1` | Régler nos groupes avec CRX Enfusion A.I. (garnison défensive, assaut offensif, patrouille) au spawn |
| m_fCRXHoldRadius | `40` | CRX : rayon de maintien de position d'un soldat de garnison, en mètres |

### Missions — `SRP_MissionManagerComponent`

Générateur, poids par type, prefabs, durées, vagues, récompenses, prise en charge, marqueurs.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Générateur* | | |
| m_iMinActive | `2` | Missions actives minimum |
| m_iMaxActive | `4` | Missions actives maximum |
| m_fExtraChance | `10` | Au-dessus du minimum : chance par minute qu'une mission de plus apparaisse, en % |
| m_iDurationMinutes | `90` | Durée d'une mission, en minutes (largage et épave : plus court, voir plus bas) |
| *SimpleRP - Sites* | | |
| m_fMinDistanceFromBase | `2000` | Distance minimale d'un site à la base, en mètres |
| m_fMaxDistanceFromBase | `9000` | Distance maximale d'un site à la base, en mètres |
| m_fMinDistanceBetweenMissions | `1500` | Distance minimale entre deux missions actives, en mètres |
| m_fMinDistanceFromPlayers | `1000` | Aucun joueur à moins de … mètres du site au moment de la création |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur de la base |
| m_sRewardMarkerName | `SRP_Livraison` | Marqueur où les caisses de récompense sont livrées (à la base) |
| m_fBaseArrivalRadius | `200` | Rayon d'arrivée à la base (véhicule récupéré, documents ramenés), en mètres |
| *SimpleRP - Poids* | | |
| m_iWeightCache | `3` | Poids : Cache (0 = jamais) |
| m_iWeightCargaison | `3` | Poids : Cargaison abandonnée |
| m_iWeightVehicule | `2` | Poids : Véhicule abandonné |
| m_iWeightConvoi | `2` | Poids : Convoi ennemi (navmesh véhicule requis) |
| m_iWeightPosteObs | `2` | Poids : Poste d'observation |
| m_iWeightSabotage | `2` | Poids : Mortier / AA / radar |
| m_iWeightOfficier | `2` | Poids : Officier ennemi |
| m_iWeightMines | `1` | Poids : Zone minée |
| m_iWeightEpave | `2` | Poids : Matériel abattu (épave) |
| m_iWeightMedical | `2` | Poids : Colis médical |
| m_iWeightCarburant | `2` | Poids : Dépôt de carburant |
| m_iWeightLargage | `2` | Poids : Ravitaillement largué |
| m_iWeightDocuments | `2` | Poids : Documents |
| m_iWeightLiberer | `3` | Poids : Village à libérer (territoire ; 0 si aucun secteur ennemi) |
| m_iWeightEcoute | `1` | Poids : Écoute radio |
| *SimpleRP - Prefabs* | | |
| m_sCachePrefab | `(à choisir)` | Cache ennemie (prop + SRP_SabotageAction) |
| m_aVehiclePrefabs | `(à choisir)` | Véhicules abandonnés (tirés au hasard) |
| m_aConvoyVehiclePrefabs | `(à choisir)` | Véhicules de convoi ennemi (UAZ, Ural… tirés au hasard) |
| m_sObservationPostPrefab | `(à choisir)` | Poste d'observation (prop antenne/radio + SRP_SabotageAction) |
| m_aSabotageTargetPrefabs | `(à choisir)` | Cibles de sabotage : mortier, AA, radar (props + SRP_SabotageAction, tirés au hasard) |
| m_aOfficerPrefabs | `(à choisir)` | Officiers ennemis (prefabs de personnage, tirés au hasard) |
| m_aMinePrefabs | `(à choisir)` | Mines (ACE Explosives ou autre, tirées au hasard) |
| m_sWreckPrefab | `(à choisir)` | Épave (prop d'hélicoptère ou de véhicule détruit) |
| m_iPacketsPerDifficulty | `10` | Paquets par niveau de difficulté dans une cargaison (cargaison, épave, largage, convoi ; carburant : x2) |
| m_sBriefcasePrefab | `(à choisir)` | Mallette de documents (prop + SRP_DocumentsAction) |
| m_sAntennaPrefab | `(à choisir)` | Antenne d'écoute (prop, sans action) |
| *SimpleRP - Vagues* | | |
| m_iCacheHoldMinutes | `8` | Cache : minutes à tenir après le sabotage (0 = réussie au sabotage) |
| m_iWaveMinutes | `4` | Minutes entre deux vagues ennemies |
| m_iFirstWaveMinutes | `2` | Minutes de présence sur le site avant la première vague |
| m_iExtraWaves | `1` | Vagues maximum en plus de la difficulté |
| m_fPresenceRadius | `150` | Rayon de présence sur le site, en mètres |
| *SimpleRP - Spécifique* | | |
| m_iConvoyDepartMinutes | `5` | Convoi : minutes entre l'annonce et le départ |
| m_iDropDurationMinutes | `45` | Largage : durée de la mission, en minutes |
| m_iWreckDurationMinutes | `40` | Épave : durée de la mission, en minutes (l'équipe de récupération ennemie part à mi-temps) |
| m_iListenMinutes | `5` | Écoute : minutes à rester près de l'antenne |
| m_iCalmMinutes | `60` | Poste d'observation : minutes de calme dans le secteur une fois neutralisé |
| m_fMedicalStartHealth | `0.3` | Colis médical : santé du civil au départ (0 à 1) |
| m_fMedicalHealTarget | `0.9` | Colis médical : santé à atteindre pour réussir (0 à 1). À baisser si ACE ne remonte pas la santé au-delà d'un certain point |
| m_fBreakdownChancePerHour | `5` | Panne : chance par heure, par véhicule du parc occupé à plus de 3 km de la base, en % |
| *SimpleRP - Prise en charge* | | |
| m_eClaimMinGrade | `5` | Prise en charge d'une mission : certif CDG, officier, Staff, ou à partir de ce grade (Sergent par défaut) |
| m_iClaimMaxMinutes | `0` | Durée maximale d'une prise en charge, en minutes (0 = illimitée) |
| *SimpleRP - Carte* | | |
| m_bMapMarkers | `1` | Marqueurs de carte (SCR_MapMarkerManagerComponent requis sur le game mode) |
| m_bMarkerIconByType | `1` | Une icône par type de mission (destroy, pick-up, heal, mine-field… voir /marqueurs) ; sinon l'icône unique ci-dessous |
| m_iMarkerIcon | `27` | Icône unique du marqueur (index dans la config vanilla ; 27 = objective-marker) |
| m_iMarkerColor | `1` | Couleur d'une mission libre (index dans la config vanilla ; 1 = orange) |
| m_iMarkerColorClaimed | `8` | Couleur d'une mission prise en charge (8 = bleu clair) |
| *SimpleRP - Récompenses* | | |
| m_bAutoCountMissions | `0` | Compter automatiquement une mission réussie aux joueurs présents sur le site (sinon, un officier crédite après le débriefing) |
| m_iRewardEurosBase | `1500` | Récompense en euros pour une difficulté 1 (x difficulté) |
| m_iRewardPacketsBase | `20` | Paquets de récompense pour une difficulté 1 (x difficulté), livrés en caisse au marqueur de la base |

### Ambiance civile — `SRP_CivilianManagerComponent`

Zones automatiques par localité, gabarits, plafonds, réflexes de danger.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Danger* | | |
| m_bDangerReactions | `1` | Réflexes de danger : un tir entendu fait fuir, une balle qui passe fait plonger, une explosion fait se coucher puis fuir, puis on se cache le temps que ça se calme |
| m_fFleeHearingDistance | `250` | Distance à laquelle un tir entendu fait fuir, en mètres |
| m_fProneDistance | `10` | Distance à laquelle une balle qui passe fait plonger, en mètres |
| m_fFleeDistance | `150` | Distance de fuite, en mètres (à l'opposé du danger, derrière un bâtiment si possible) |
| m_iCalmSeconds | `120` | Secondes de calme avant de reprendre sa vie |
| *SimpleRP - Civils* | | |
| m_aCharacterPrefabs | `(à choisir)` | Prefabs de PNJ civils (tirés au hasard) |
| m_aVehiclePrefabs | `(à choisir)` | Prefabs de voitures civiles (tirées au hasard) |
| m_sGroupPrefab | `{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et` | Prefab de groupe IA vide |
| m_sMoveWaypointPrefab | `{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et` | Prefab de point de passage « Move » |
| m_sFactionKey | `CIV` | Clé de la faction civile |
| *SimpleRP - Zones automatiques* | | |
| m_bAutoZones | `1` | Créer une zone par localité de la carte au démarrage |
| m_sZonePrefab | `(à choisir)` | Prefab de zone (SRP_ZoneCivile.et, porteur de SRP_CivilZoneComponent) |
| m_CityPreset | `(liste)` | Gabarit ville (vide = 300 m, 10 promeneurs, 3 immobiles, 4 garées, 2 en route) |
| m_TownPreset | `(liste)` | Gabarit bourg (vide = 200 m, 7, 2, 3, 1) |
| m_VillagePreset | `(liste)` | Gabarit village (vide = 130 m, 4, 1, 2, 1) |
| m_SettlementPreset | `(liste)` | Gabarit hameau (vide = 80 m, 2, 1, 1, 0) |
| m_fManualOverrideDistance | `300` | Une zone posée à la main à moins de … mètres d'une localité remplace la zone automatique |
| *SimpleRP - Comportement* | | |
| m_fMaxTripDistance | `6000` | Distance maximale entre deux zones pour un trajet de voiture, en mètres |
| *SimpleRP - Distances* | | |
| m_fActivateDistance | `1500` | Une zone s'anime quand un joueur est à moins de … mètres |
| m_fDeactivateDistance | `2500` | Une zone se vide quand tous les joueurs sont à plus de … mètres |
| m_iMaxCivilians | `40` | Nombre maximal de PNJ civils vivants en même temps |
| *SimpleRP - Comportement* | | |
| m_iTickSeconds | `10` | Cadence de la boucle d'ambiance, en secondes (activation des zones, apparitions, entretien) |
| m_iSpawnsPerTick | `4` | Apparitions maximales par cycle, toutes zones confondues (réparties équitablement entre les zones actives) |
| m_bWalk | `1` | Les promeneurs marchent au pas ; ils ne courent qu'en réaction à un danger (tirs, explosion) |
| m_iWanderSeconds | `90` | Délai moyen entre deux points de marche d'un promeneur, en secondes |
| m_iRespawnSeconds | `600` | Délai avant qu'un PNJ tué soit remplacé, en secondes |
| m_fArrivalDistance | `60` | Distance d'arrivée d'une voiture en circulation, en mètres |
| m_iStuckSeconds | `180` | Une voiture immobile depuis … secondes est considérée bloquée et retirée |

### Territoire — `SRP_TerritoryComponent`

Capture, garnisons, contre-attaques, production, secteurs automatiques, marqueurs.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP - Capture* | | |
| m_iCaptureMinutes | `10` | Capture : minutes de présence sans ennemi vivant dans le rayon |
| m_iGarrisonMin | `4` | Garnison : groupes minimum par secteur (répartis à des emplacements différents dans la zone) |
| m_iGarrisonMax | `6` | Garnison : groupes maximum par secteur (la menace et le nombre de joueurs poussent vers ce plafond) |
| m_fGarrisonSpawnDistance | `1200` | Un secteur ennemi reçoit sa garnison quand un joueur approche à … mètres |
| m_fGarrisonDespawnDistance | `2500` | La garnison est retirée si plus aucun joueur n'est à moins de … mètres pendant 10 min |
| *SimpleRP - Contre-attaque* | | |
| m_iAttackMinPlayers | `4` | Contre-attaque : joueurs connectés minimum |
| m_iAttackMinMinutes | `60` | Contre-attaque : délai minimum entre deux, en minutes |
| m_iAttackMaxMinutes | `180` | Contre-attaque : délai maximum, en minutes |
| m_iAttackWarningMinutes | `15` | Contre-attaque : minutes entre l'annonce et l'assaut |
| m_iAttackWaveMinutes | `5` | Contre-attaque : minutes entre deux vagues |
| m_iAttackMaxWaves | `3` | Contre-attaque : vagues maximum (au-delà, l'ennemi renonce s'il n'a pas pris le secteur) |
| m_iLoseMinutes | `10` | Perte : minutes d'ennemi dans le rayon sans joueur |
| *SimpleRP - Production* | | |
| m_iProductionCapHours | `3` | Production : heures de production au plus dans la caisse d'un secteur |
| m_iCaptureReward | `500` | Subvention en euros à la capture d'un secteur |
| *SimpleRP - Secteurs automatiques* | | |
| m_bAutoSectors | `1` | Créer les secteurs automatiquement depuis les lieux de la carte (les zones de l'ambiance civile), en plus des marqueurs SRP_Secteur |
| m_bAutoHamlets | `0` | Inclure les hameaux (secteurs de type poste, munitions) et pas seulement les villages |
| m_fAutoMinDistance | `1500` | Distance minimale à la base pour un secteur automatique, en mètres |
| m_iAutoMax | `12` | Nombre maximum de secteurs automatiques (les plus proches de la base d'abord) |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur de la base (distances) |
| *SimpleRP - Carte* | | |
| m_bMapMarkers | `1` | Marqueurs de carte des secteurs |
| m_iMarkerIcon | `11` | Icône d'un secteur (index config vanilla ; 11 = flag-2, drapeau uni) |
| m_iMarkerColorOurs | `7` | Couleur d'un secteur à nous (7 = bleu) |
| m_iMarkerColorEnemy | `4` | Couleur d'un secteur ennemi (4 = rouge) |
| m_iMarkerIconWarning | `49` | Icône d'une attaque annoncée (49 = attack) |
| m_iMarkerColorWarning | `1` | Couleur d'une attaque annoncée (1 = orange) |
| m_iMarkerIconAttack | `50` | Icône d'une attaque en cours (50 = attack-main) |
| m_bMarkerBlink | `1` | Le drapeau clignote : rouge tant qu'il reste des ennemis dans la zone, orange pendant le compte de capture |
| m_iMarkerColorEnemyDim | `3` | Clignotement rouge : seconde couleur (3 = rouge sombre) |
| m_iMarkerColorCaptureDim | `0` | Clignotement orange : seconde couleur (0 = blanc) |

### Charte — `SRP_CharteComponent`

Texte, ouverture automatique, périmètre du spawn.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_fRadius | `50` | Rayon autour du spawn tant que la charte n'est pas acceptée, en mètres |
| m_sSpawnMarkerName | `SRP_SpawnBase` | Marqueur du spawn de la base |
| m_bAutoOpen | `1` | Ouvrir la charte automatiquement à l'arrivée du personnage tant qu'elle n'est pas acceptée |

### Jour/nuit — `SRP_DayNightComponent`

Durée réelle d'une journée, heure persistante.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_fRealHoursPerDay | `6` | Durée réelle d'une journée en jeu, en heures (24 = temps réel, 6 = quatre fois plus vite) |
| m_bRestoreTime | `1` | Rétablir l'heure sauvegardée au démarrage (sinon, celle du monde) |
| m_fFirstLaunchHour | `8` | Heure de départ au tout premier lancement (0-23) |

### Éclairage de la base — `SRP_GenerateurComponent`

Le générateur de la base (1.0.26, carte #54) : les lampes à moins du rayon suivent son état ; sauvegardé dans `generateur.json`.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur du centre de la base (à défaut : le générateur lui-même) |
| m_fBaseRadius | `300` | Rayon de la base : les lampes à moins de … mètres du centre suivent le générateur |
| m_bFirstLaunchOn | `1` | Générateur allumé au tout premier lancement (sans `generateur.json`) |
| m_fNotifyRadius | `300` | Les joueurs à moins de … mètres du générateur sont prévenus de chaque changement |
| m_iCheckSeconds | `60` | Pendant une coupure, période de vérification des lampes (une lampe apparue ou rallumée est réteinte) |
| m_sLanternPrefab | `LanternMilitary_US_01.et` | Lanterne posée la nuit (points de livraison, contrôle routier) |
| m_sGeneratorPrefab | `SRP_Generateur.et` | Prefab du générateur : à la réparation, une épave est remplacée par un neuf au même endroit |

Composant vanilla à côté : `SCR_NightModeGameModeComponent` avec `m_bAllowGlobalNightMode` = 1 (mode nuit du Game Master, image éclaircie de nuit pour tous ; `SRP_GenerateurComponent.SetGmNightMode` pour le menu Staff).

### Déconnexion — `SRP_DisconnectComponent`

Corps qui reste, mort en absence, rayon de sécurité.

| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_iMinutes | `15` | Minutes pendant lesquelles le corps reste après une déconnexion |
| m_fSafeRadius | `2000` | Rayon de sécurité autour de la base, en mètres : passé le délai, un corps à l'intérieur n'est pas déclaré mort |
| m_sBaseMarkerName | `SRP_SpawnBase` | Marqueur de la base |
| m_bUnconsciousIsDeath | `1` | Se déconnecter inconscient vaut mort immédiate |
| m_bStaffExempt | `1` | Le Staff n'est pas concerné (pas de corps, pas de mort en absence) |

> `SRP_JournalComponent` n'a pas de paramètre. `SRP_SpawnLogic` (la logique de spawn, sur le composant de respawn du game mode) a les siens :


| Paramètre | Défaut | Rôle |
|---|---|---|
| *SimpleRP* | | |
| m_sFactionKey | `FR` | Clé de la faction des joueurs (FR pour AMF) |
| m_sCharacterPrefab | `(à choisir)` | Prefab du personnage joueur (kit de base) |
| m_sSpawnEntityName | `SRP_SpawnBase` | Nom du marqueur de spawn de la base (Object Properties > Name). Les marqueurs de même nom suivis de 1, 2, 3… (SRP_SpawnBase1…) sont des points supplémentaires ; le premier sans joueur à côté est choisi |
| m_bUseSavedPosition | `1` | Reprendre à la dernière position sauvegardée |
| m_aKitDeBase | `(à choisir)` | Kit de base : objets donnés à chaque spawn frais (fusil, chargeurs, pansements...). Un même prefab peut apparaître plusieurs fois. |
| m_bRestoreInventory | `1` | Restaurer l'inventaire sauvegardé à la reconnexion (sinon kit de base à chaque connexion) |
| m_iRespawnDelay | `10` | Délai de respawn après une mort, en secondes |
| m_iRespawnDelayNoVivres | `60` | Délai de respawn quand l'intendance est vide (vivres à zéro), en secondes |


---

## 5. Les prefabs à créer

Recette générale : Resource Browser → un prop vanilla (ou `GenericEntity` pour un marqueur) → clic droit → **Inherit** → enregistrer dans `SimpleRP/Prefabs/<dossier>/`. Ajouter les composants (« + »), régler, Ctrl+S. Un objet qui apparaît en cours de partie (caisse, sacoche) doit porter `RplComponent`. Une action au contact se déclare dans `ActionsManagerComponent` : un **contexte** (nom libre, un point d'ancrage) et, dans `Additional Actions`, l'action avec son contexte, son libellé et sa durée.

| Prefab | Dossier | Base | Composants et actions | Où |
|---|---|---|---|---|
| `SRP_GameMode.et` | Prefabs | `SCR_BaseGameMode` | tous les composants du §4 + `SCR_MapMarkerManagerComponent` ; Spawn Logic = `SRP_SpawnLogic` | une instance dans le monde |
| `SRP_Paquet_Munitions/Carburant/Pieces/Vivres.et` | Prefabs/Paquets | un petit objet d'inventaire (`FieldDressing_US_01`) | `SRP_PacketComponent` (ressource), poids 1 kg, `InventoryItemComponent` ; à renseigner sur `SRP_ResourceManagerComponent` | dans les caisses et dépôts |
| `SRP_CaisseLivraison.et` | Prefabs/Paquets | un prop de caisse | `RplComponent`, `SCR_UniversalInventoryStorageComponent` (capacité ≥ 40 kg), **`SRP_ContainerInventoryStorageManagerComponent`** (le gestionnaire des personnages `SCR_InventoryStorageManagerComponent` est toléré mais signalé), action d'ouverture d'inventaire | posée par le mod |
| `SRP_Depot_Soute/Citerne/Atelier/Intendance.et` | Prefabs/Depots | un prop de conteneur plus grand | même chose + `SRP_DepotComponent` (ressource, capacité en paquets) + action `SRP_DepotConsultAction` (« Consulter le stock ») | la base |
| `SRP_Coffre.et` | Prefabs/Depots | `Safe`/`Locker` | `RplComponent`, actions `SRP_SafeConsultAction`, `SRP_SafeDepositAction` (Duration 2) | la base |
| `SRP_Terminal.et` | Prefabs/Depots | `Laptop`/`Computer` | action `SRP_TerminalAction` (« Utiliser le terminal logistique ») | où tu veux |
| `SRP_PC.et` | Prefabs/Depots | ordinateur ou radio de PC | `RplComponent`, action `SRP_PCAction` (« Consulter le PC », Duration 1) | le PC de la base |
| `SRP_TableauMissions.et` | Prefabs/Depots | `Board`/`Table`/`Sign` | action `SRP_MissionBoardAction` (« Consulter les missions ») | le PC |
| `SRP_PosteSecours.et` | Prefabs/Depots | lit/tente médicale | action `SRP_MedicalHealAction` | infirmerie |
| `SRP_Citerne.et`, `SRP_Atelier.et` | Prefabs/Depots | citerne, atelier | actions `SRP_RefuelAction`, `SRP_FillJerrycansAction` ; `SRP_RepairAction` ; `SRP_FleetSignAction` sur les véhicules du catalogue si besoin | garage |
| `SRP_Sacoche.et` | Prefabs/Objets | petit sac/boîte | `RplComponent`, `SRP_MoneyComponent`, action `SRP_CashPickupAction` (« Ramasser l'argent », Duration 1) ; à renseigner sur la trésorerie | posée par le mod |
| `SRP_PointLivraison.et` | Prefabs/Livraison | `GenericEntity` | `SRP_DeliveryPointComponent` (caisses / véhicules, emprise) | posés loin de la base, nommés |
| `SRP_ZoneCivile.et` | Prefabs/Ambiance | `GenericEntity` | `SRP_CivilZoneComponent` ; enfants facultatifs `Poste_x`, `Parking_x`, `Route_x` | facultatif (les zones sont automatiques) |
| `SRP_Secteur.et` | Prefabs/Territoire | `GenericEntity` | `SRP_SectorComponent` (nom, type, rayon, production) | facultatif (les villages sont automatiques) |
| `SRP_SiteMission.et` | Prefabs/Missions | `GenericEntity` | `SRP_MissionSiteComponent` | facultatif, lieux de mission à toi |
| `SRP_Cache.et`, `SRP_Poste.et`, `SRP_Cible_*.et` | Prefabs/Missions | props (dépôt, antenne, mortier, AA, radar) | `RplComponent`, action `SRP_SabotageAction` (« Saboter », Duration 5) | via le gestionnaire de missions |
| `SRP_Mallette.et` | Prefabs/Missions | `Suitcase`/`Briefcase` | `RplComponent`, action `SRP_DocumentsAction` (« Récupérer les documents », Duration 3) | idem |
| Antenne, épave, mines, véhicules, officiers | — | props et prefabs vanilla/ACE/RHS | aucun composant | à renseigner sur le gestionnaire de missions |
| `SRP_Panneau_*.et` | Prefabs/Base | `Billboard`/`Sign` | matériau remplacé sur la face (§6.5) | la base |
| `SRP_Generateur.et` | Prefabs/Props/Base | `GeneratorPortable_US_01` (jeu) | `SRP_GenerateurEntityComponent` + actions `SRP_GenerateurStartAction` (Duration 1) et `SRP_GenerateurRepairAction` (Duration 30) sur le contexte `powerswitch` du prefab du jeu | le garage, un seul (calque `BaseFR/Eclairage`) |
| `SRP_Plafonnier.et`, `SRP_Neon.et` | Prefabs/Props/Base | `LightCeiling_01`, `LightIndustrial_02` (jeu) | `SCR_BaseInteractiveLightComponent` (Initial Light State = LIT) + action `SCR_SwitchLightUserAction` (« Allumer / Éteindre ») ; verre allumé | les bâtiments de la base (document `Eclairage_base_PAG.md`) |
| `Character_Base.et` (override) | Prefabs/Characters/Core | fourni dans le zip avec son `.meta` | composants de voix + action `SRP_SoldierAction` sur tous les personnages | déjà fait |

---

## 6. Les fichiers de configuration, layouts, overrides, audio

### 6.1 `Configs/System/` (fournis, avec `.meta` vanilla)
- `chimeraMenus.conf` : déclare nos cinq menus (`SRP_TerminalMenu`, `SRP_AdminMenu`, `SRP_SoldierMenu` sur `SRP_Terminal.layout` ; `SRP_PCMenu`, `SRP_CharteMenu` sur `SRP_PC.layout`).
- `chimeraInputCommon.conf` : l'action d'entrée `SRP_VoiceRange` (F9) et son contexte `IngameContext`.
- `keyBindingMenu.conf` : le libellé rebindable dans les options.

### 6.2 `Configs/SimpleRP/`
- `SRP_CertifTree.conf` (fourni) : sept règles de certification (prérequis en cases à cocher, grade minimum) et six règles de grade (heures, missions, certifs exigées). À pointer sur `SRP_PlayerManagerComponent` → Arbre des certifications. Toute certif ou tout grade absent du fichier garde la règle par défaut du code (§9.2).
- `SRP_Catalogue.conf` (à créer : Create Resource → Config File, classe `SRP_Catalogue`) : véhicules (nom, prefab, catégorie, capacité, prix, nombre au premier lancement) et caisses (nom, ressource ou objet, paquets, prix). À pointer sur `SRP_TreasuryComponent`.

### 6.3 `UI/Layouts/SimpleRP/` (fournis, avec `.meta`)
- `SRP_Terminal.layout` : le dialogue à liste (hérite du `ConfigurableDialog` vanilla), largeur réglée à 1 120 px par `WidthOverride`.
- `SRP_PC.layout` : le dialogue à onglets et zone défilante (nœuds `Body`, `Tabs`, `ScrollSize`, `Scroll`, `List`, à ne pas renommer), largeur 1 120 px.
- `SRP_TerminalEntry.layout` : le gabarit d'une ligne, d'un onglet, d'un bouton.

### 6.4 `Prefabs/Characters/Core/Character_Base.et` (override additif)
Ajoute à tous les personnages les trois composants de voix (`SRP_VoNComponentNear/Normal/Loud`, le vanilla désactivé) et l'action « Gestion du soldat » (`SRP_SoldierAction`, contexte `LowerTorso`, 3 m). Les Filename des composants de voix pointent vers les projets audio (§6.6). Le log dit « Unknown class SCR_ChimeraCharacter » au lancement : c'est un rechargement pendant la compilation, sans effet.

### 6.5 Panneaux d'affichage (sans script)
Image PNG standard 2048 × 1024 dans `UI/Textures/Panneaux/` (le Workbench crée le `.edds` ; s'il refuse, l'image est mal formée : ré-exporter en PNG 8 bits non entrelacé ou TGA) → matériau `MatPBRBasic` (BaseColor = la texture, Emissive pour la nuit) → prefab hérité d'un panneau vanilla, matériau remplacé sur la face dans `MeshObject` → posé, échelle 3-4 pour un géant.

### 6.6 Audio (voix)
- `von_5m.acp` (Outer Range 5, Slope 1.2) et `von_30m.acp` (Outer Range 30, Slope 4), copies du `von.acp` vanilla, dans `Sounds/` du mod ; le vanilla (68 m) reste tel quel. Les trois composants de voix pointent chacun vers le sien.

---

## 7. Les fichiers du profil

`Documents\My Games\ArmaReforgerWorkbench\profile\SimpleRP\` dans le Workbench ; sur le serveur dédié, le dossier `profile` du `-profile` du `.bat`.

| Fichier | Contenu | Écrit par |
|---|---|---|
| `players/<identité>.json` | la fiche d'un joueur | gestionnaire de joueurs (connexion, changements, auto 300 s, déconnexion) |
| `journal/journal_AAAA-MM.log` | le journal horodaté du mois | tous les systèmes |
| `base.json` | comptes des quatre ressources (paquets) | ressources (30 s si mouvement, 2 min, arrêt) |
| `flotte.json` | le parc : numéros, prefabs, état, position, cargaison | flotte |
| `tresorerie.json` | solde du coffre, livraisons en attente | trésorerie |
| `missions.json` | compteurs, prochain numéro | missions |
| `territoire.json` | secteurs : propriétaire, date, stock ; menace | territoire (capture, perte, 5 s si sale, arrêt) |
| `temps.json` | l'heure en jeu | jour/nuit (5 min, arrêt) |
| `generateur.json` | état du générateur de la base (0 éteint, 1 allumé, 2 en panne), dernier changement | éclairage (à chaque changement, arrêt) |
| `charte.txt` | la charte, `version: N` en tête | créé au premier lancement, édité à la main, `/charte recharger` |
| `bannis.json` | les bannis | `/ban`, `/unban` |
| `CRX_EAI/` | fichiers de configuration de CRX (difficulté globale de l'IA) | CRX |

Supprimer un fichier remet le système à son premier lancement (dépôts garnis à 50 %, coffre à 5 000 €, flotte du catalogue, territoire tout à l'ennemi). `/wipe confirmer` fait la même chose pour tout sauf les fiches.

---

## 8. Toutes les commandes de chat

Tout le monde :
- `/aide` — la liste selon tes droits
- `/moi` — ta fiche : grade, certifs, états de service, argent porté, certifs à portée, prochain grade
- `/flotte` — le parc ; `/missions [id]`, `/missions claim <id>` (CDG, sous-officiers, officiers), `/missions abandonner <id>`
- `/territoire` — les secteurs ; `/heure` ; `/charte` ; `/mp <pseudo> <message>`

Depuis la 1.0.12, le chat ne garde que `/aide`, `/mp`, `/lier`, `/vote`, `/charte` et `/admin` : toute action se fait en jeu (PC, Gestion du soldat, terminal, tableau, menu Staff F10). Une ancienne commande tapée au chat renvoie vers l'endroit où faire l'action. Détail dans COMMANDES_SIMPLERP.md.

Officiers (Lieutenant, Capitaine) : `/joueurs`, `/info <pseudo>`, `/grade <pseudo> <grade> [force]` (grades : recrue, soldat, 1re, caporal, cch, sergent, sch, adjudant, lieutenant, capitaine), `/service <pseudo> [+1|-1]` (mission créditée après le débriefing), `/warn <pseudo> <motif>`, `/acheter [vehicule <n> | caisse <n>]`, `/euros`, `/charte <pseudo>`, `/deco`.


Staff : `/staff <pseudo> on|off`, `/kick <pseudo> [motif]`, `/ban <pseudo> [jours] [motif]`, `/unban <pseudo>`, `/bans`, `/sauver`, `/stock [ressource] [valeur|+n|-n]`, `/caisse <ressource> [paquets]`, `/flotte reset`, `/euros [valeur|+n|-n]`, `/civils [reset]`, `/admin`, `/missions nouvelle <type>|annuler <id>|reussir <id>|reset`, `/territoire capturer|perdre <secteur>|reset`, `/charte reset|recharger`, `/heure <HH:MM>`, `/heure vitesse <h>`, `/marqueurs`, `/wipe confirmer`.

Menu au contact (touche F sur un joueur) : **Gestion du soldat** : certifs (instructeur habilité), grades et états de service (officiers), formations (Adjudant).

---

## 9. Les systèmes, un par un


### 9.1 Identité, fiche, journal
À la connexion, l'identité Bohemia est lue (repli `local_<pseudo>` après plusieurs essais), la fiche chargée ou créée (`players/`). Elle suit : nom, grade, certifications (masque de bits), habilitations d'instructeur, temps de jeu (secondes), connexions, missions réussies (créditées à la main), infractions, argent porté, dernière position et inventaire (sérialisé par le système de loadout vanilla, exact emplacement par emplacement), acceptation de la charte (version, date), motif de mort en absence. Les identités Staff sont listées sur `SRP_PlayerManagerComponent` (copiées depuis le journal après une première connexion) ou données par `/staff`. Le journal (`[CONNEXION]`, `[GRADE]`, `[CERTIF]`, `[MISSION]`, `[TERRITOIRE]`, `[TRESORERIE]`, `[FLOTTE]`, `[LIVRAISON]`, `[CIVILS]`, `[STAFF]`, `[INFRACTION]`, `[AVERTISSEMENT]`, `[DECONNEXION]`, `[SERVICE]`, `[CHARTE]`, `[SYSTEME]`, `[RESSOURCE]`, `[ENNEMI]`) est la mémoire du serveur ; ses 80 dernières lignes se lisent au PC.

### 9.2 Grades et certifications
**Quinze certifications** (sigle affiché, nom complet) : FGI, OR (Opérateur radio), Infirmier, Médecin, Sapeur, Logi (Logisticien), VL, PL, Blindé, Pilote, MG, LG (Lance-grenades), AT, TP, CDG (Chef de groupe). **Arbre** : FGI conditionne tout et fait passer Recrue → 2e classe ; PL exige VL ; Blindé exige VL et PL (Caporal) ; Pilote exige VL (Caporal) ; Médecin exige Infirmier (Caporal) ; CDG exige OR (Caporal) ; TP : Caporal ; le reste : FGI seule. Retirer une certif emporte celles qui en dépendent. **Qui délivre** : le Staff, et l'**instructeur habilité pour cette certif** (`/instructeur`, par un Adjudant ou le Staff ; une habilitation par certification, il faut détenir la certif elle-même). Les officiers ne délivrent pas de certif sans habilitation.

**Dix grades** : Recrue, Soldat de 2e classe (FGI), Soldat de 1re classe (10 h, 8 missions), Caporal (25 h, 20), Caporal-chef (50 h, 40), Sergent (CDG, 100 h, 70), Sergent-chef (CDG, 200 h, 120), Adjudant (CDG, 400 h, 200), Lieutenant et Capitaine (sur nomination). Officiers = Lieutenant et Capitaine ; sous-officiers = Sergent à Adjudant. La promotion est une décision d'officier (`/grade` ou le menu au contact), refusée avec le détail tant que les conditions manquent ; le Staff force ; jamais au-dessus de son propre grade. Les missions se comptent à la main après le débriefing (`/service +1` ou le menu). Les officiers sont prévenus quand un soldat remplit les conditions du grade suivant. Tout l'arbre et tous les chiffres sont dans `SRP_CertifTree.conf`.

**Effets en jeu** : Médecin = soin complet au poste de secours ; CDG (ou sous-officier, officier) = prise en charge des missions ; VL/PL/Blindé/Pilote = catégories de véhicules au parc ; Sapeur = réparation ; Infirmier = injecteurs à l'arsenal (via l'économie d'arsenal) ; Logi = achats au terminal (avec le grade).

### 9.3 Spawn, dotation, respawn
Spawn sur `SRP_SpawnBase` (ou un marqueur `SRP_SpawnBase1…` libre), reprise de la position et de l'inventaire sauvegardés si la fiche en a (reconnexion), sinon kit de dotation (spawn frais, après une mort) : un HK416F-S et 10 chargeurs de 30 « 30Rnd HK416 BO F5 », gratuits (liste `m_aKitDeBase` du `SRP_SpawnLogic`, exemptée de la facturation d'arsenal ; un même prefab y figure autant de fois qu'il faut d'exemplaires). Un respawn coûte 2 vivres à l'intendance (`m_iVivresPerRespawn`) ; à zéro, on respawn quand même, après un délai plus long. La caméra de mort montre le corps.

### 9.4 Ressources en paquets, dépôts
L'unité de ressource est un **paquet** physique d'un kilo (prefabs `SRP_Paquet_*`, composant `SRP_PacketComponent`). Les **dépôts** (soute = munitions, citerne = carburant, atelier = pièces, intendance = vivres) sont des conteneurs ouvrables ; le stock, c'est le nombre de paquets dedans, lu directement dans les stockages (indépendant du gestionnaire d'inventaire). Au premier lancement (pas de `base.json`), les dépôts sont garnis à 50 % ; ensuite, au démarrage, le compte sauvegardé est reconstitué par lots de 20 (avertissement si un dépôt est trop petit ou sans gestionnaire). Les **vivres** se consomment au fil du temps selon les joueurs connectés. Le respawn prend 2 vivres, la citerne du carburant, l'atelier des pièces, l'arsenal des paquets de munitions. Une **caisse de livraison** (`SRP_CaisseLivraison`) est un conteneur posé au sol rempli de paquets, retiré quand vide depuis 10 min. Tout se fait par glisser-déposer d'inventaire. Sauvegarde : 30 s après un mouvement (y compris un paquet posé à la main), toutes les 2 min, et le dernier compte connu à l'arrêt.

### 9.5 Arsenal (Bacon) et économie
L'arsenal Bacon sert d'armurerie ; chaque retrait est facturé en **paquets de munitions** (la soute), jamais en pièces : les armes de tous types et les consommables médicaux sont gratuits, un chargeur coûte 1 paquet quelle que soit sa capacité (une boîte de 100 de Minimi = 1), une roquette ou une munition unitaire 5, une grenade 2 ; les retours à l'arsenal sont remboursés, les paquets eux-mêmes sont exclus. Le kit de dotation du spawn n'est pas facturé. Une zone d'arsenal (`SRP_ArsenalZoneComponent`) limite où ça s'applique. Les injecteurs et outils sont conditionnés aux certifs.

### 9.6 Trésorerie, butin, terminal, catalogue
Le **coffre** (euros, `tresorerie.json`, 5 000 € au premier lancement) : consultation et dépôt sur le coffre physique, `/euros`. Le **butin** : à la mort d'un ennemi (factions réglables), une **sacoche** tombe une fois sur deux (50-300 €) ; ramassée, la somme est portée (fiche, `/moi`), à déposer au coffre ; mourir avec de l'argent laisse une sacoche ; sur un civil, la sacoche est un **crime de guerre** (infraction, alerte, confiscation, amende). Le **terminal** (officiers, Logi) vend les véhicules et caisses du **catalogue** (`SRP_Catalogue.conf`) ; caisses livrées après 5 min sur un point de livraison, véhicules après le délai de la flotte ; livraisons en attente persistantes. Subventions : captures (500 €), missions.

### 9.7 Parc automobile
Chaque véhicule du parc a un numéro, un prefab, une catégorie (léger, lourd, blindé, aéronef → certifs), un état (en service, en commande, détruit), une position et sa cargaison en paquets, capturés toutes les 10 s et persistants (`flotte.json`). Un véhicule détruit est perdu (journal, alerte) et se rachète au terminal avec un nouveau numéro. Actions : plein à la citerne (carburant de la citerne), jerricans, réparation à l'atelier (pièces, certif Sapeur), signature d'un véhicule trouvé. À l'arrêt, la flotte écrit le dernier état capturé sans relire les coffres.

### 9.8 Points de livraison et emprise ennemie
Des marqueurs `SRP_PointLivraison` (caisses et/ou véhicules) loin de la base. À chaque livraison, un point sans joueur à moins de 2 km est tiré ; l'**emprise ennemie** (30 %, surchargeable par point) y pose une garnison avant la marchandise (1 groupe + 1 par 4 joueurs, 3 max), retirée après 3 h sans visite. Les officiers apprennent où, pas si c'est gardé (réglable).

### 9.9 IA ennemie et CRX
`SRP_EnemyComponent` connaît les prefabs de groupes (RHS/USSR/FIA), les points de passage Defend et Move, tient un plafond global de groupes vivants, adapte l'effectif au nombre de joueurs (groupes minimum, +1 par N joueurs, maximum), et répartit les groupes d'un même spawn dans des parts distinctes du cercle (35-80 % du rayon). La **présence ambiante** pose des patrouilles près des joueurs hors de la base (2 %/min, 15 % près d'une mission), 4 au plus. Les comptages distinguent « agents dans les groupes » et « **en état de combattre** dans une zone » (vivants, conscients, dans le rayon) : c'est ce dernier qui compte pour les captures et les pertes.

**Prorata et budget local (20 sept. 2026).** L'effectif ne suit plus les joueurs connectés mais les joueurs **proches du lieu** (3 km, `m_fScaleRadius` ; ceux restés à la base ne comptent jamais) : un groupe par tranche de 2 joueurs (`m_iPlayersPerGroup`), plus un bonus (difficulté de la mission, menace). Garnison de secteur : 2 groupes pour 1 à 4 joueurs en approche, 3 pour 5-6, 4 pour 7-8, 6 au plus ; elle se **complète** si d'autres joueurs arrivent avant le contact (pas de remplacement des pertes). La **garde d'une mission n'est plus posée à la création** mais à l'approche des joueurs (1 500 m, `m_fGuardSpawnDistance`), à leur mesure. **Pas de doublon** : une mission dont le site est dans un secteur ennemi n'a pas de garde à elle (la garnison fait la garde) ; « Village à libérer » n'ajoute plus rien à la garnison ; et tout ce qui se pose autour d'un même lieu (1 200 m) — garnison, garde, vagues, patrouilles — partage un **budget local** (prorata + 2, plafond 8) : une vague ou une patrouille qui ne rentre pas dans le budget est réduite ou sautée. Les patrouilles ambiantes sont tirées une fois par **groupe de joueurs** (500 m), plus une fois par joueur. **Renforts motorisés** (`m_bMotorized`) : avec au moins 3 joueurs sur place, une vague sur deux voit son premier groupe arriver en camion (Ural ou UAZ, liste réglable) depuis le lieu-dit le plus proche hors de vue (700 à 2 500 m), débarquer à 300 m de la cible (point de passage GetOut) et finir à pied ; le camion abandonné est retiré après 20 min sans joueur à 300 m.

**Discrétion, alerte, jeeps et fusées (20 sept. 2026).** La **perception** de nos ennemis est réglée par soldat selon son état (`SRP_EnemyComponent`, catégorie Perception → `SRP_CRX`) : tranquille 0,45 (CRX : 1,0), vigilant 0,9 (2,5), alerté 1,6 (2,5), menacé 2,2 (3,0) : tant que l'ennemi ne se doute de rien on progresse à couvert, allongé ou accroupi ; une fois le combat engagé il voit presque normalement. Les patrouilles allument leurs lampes la nuit. **Alerte** : toutes les 10 s le socle lit ce que nos groupes ont repéré (`m_Perception.m_aTargets`, catégories IDENTIFIED = vu, DETECTED = entendu, joueurs seulement, moins de 15 s) ; le premier repérage ouvre une alerte à la dernière position connue (fusionnée dans 400 m, éteinte après 6 min sans nouveau repérage, journal ENNEMI). Sur une alerte : après 20 s **une** patrouille à pied du lieu (700 m) vient voir et la jeep (2 km) vient se poster à 150 m, quatre minutes, puis chacun reprend sa ronde ; **la nuit**, une **fusée éclairante blanche** sous parachute (effet de l'obus de 82 mm, 170 m de haut, décalée de 10 à 60 m) toutes les 50 s tant que les joueurs sont repérés, 5 au plus par alerte. La mission Documents donne l'alerte dès que des joueurs sont repérés sur le site. **Jeep armée** (`m_bVehiclePatrols`) : quand une garnison ou une garde de mission est posée pour au moins 3 joueurs, 35 % de chance d'une UAZ-469 PKM (liste réglable, RHS possible) qui fait en boucle le tour du site et des trois lieux-dits voisins (circuit par les routes), pilote et mitrailleur restant à leur poste au contact ; 2 jeeps au plus en même temps, comptées dans le budget local. **Hélicoptère de recherche : prototype** (`SRP_HeliSearch.c`) — l'IA de Reforger ne sait pas piloter, le vol est scripté côté serveur (trajectoire posée à chaque image, moteur tournant, portance des rotors coupée, phares d'atterrissage allumés vers le sol). Menu Staff → Rapide → Essais : il arrive de 2 km, tourne 6 min à 120 m au-dessus de toi, se recentre et resserre ses cercles si une alerte naît près de lui, largue des fusées la nuit, repart. Réglages : catégorie Hélicoptère de `SRP_EnemyComponent`. **Branché sur les alertes** (`m_bHeliOnAlert`) : quand des joueurs sont VUS depuis 30 s à moins de 600 m d'une mission active ou dans un secteur ennemi, avec au moins 3 joueurs dans les 3 km et loin de la base, l'ennemi a 12 % de chance (un tirage par alerte) de faire venir l'appareil ; un seul à la fois, 30 min de repos entre deux sorties. Il fouille la zone au projecteur, se fixe sur un soldat repéré (debout 35 %/s, accroupi 18 %, couché 6 %, ×2 en mouvement), renseigne les patrouilles au sol, est perdu après 6 s hors de vue, et tombe si un moteur, un rotor ou ses pilotes sont touchés. Reste à améliorer : le rendu du projecteur vu du sol.

**CRX** est réglé au spawn, par rôle (`SRP_CRX.c`) : **Garnison** (secteurs, gardes, livraisons) = défensif, tir à vue au contact (mode rouge), réaction au tir à 500 m, enquête 5 min dans 300 m avec fouille des bâtiments, suppression, retour à la position toujours, maintien de position par soldat dans le rayon du secteur (40 m hors territoire), observation au repos ; **Assaut** (contre-attaques, vagues, convoi) = offensif, rouge, 600 m, enquête 400 m, jamais de retour ; **Patrouille** = défaut, rouge, 400 m ; **Civil** = mode blanc (ne tire jamais), ni enquête ni suppression, marche au pas, les postés tiennent leur place. La difficulté globale (perception, précision, grades, exécution des inconscients) se règle dans `profile/CRX_EAI/` et sur `SCR_AIWorld → SCR_AISettingsComponent`.

### 9.10 Missions
Un générateur garde 2 à 4 missions actives (poids par type, sites à 2-9 km de la base, loin des joueurs et des autres missions, jamais deux fois le même type). Chaque mission : difficulté 1-3 (garde, vagues, récompense), timer (90 min ; largage 45, épave 40), prise en charge par un chef (timer gelé), rapport de fin, récompense en euros et en caisses livrées, marqueur de carte (icône par type, orange libre, bleu clair prise en charge). Les seize types et leur mécanique :

| Type | Faire | Particularité |
|---|---|---|
| Cache ennemie | neutraliser la garde, saboter (action), tenir 8 min sous contre-attaque | exfiltration |
| Cargaison abandonnée | ranger les paquets de 2-4 caisses aux dépôts | seuls les paquets rangés comptent, renforts pendant le chargement |
| Véhicule abandonné | ramener le véhicule à la base | il rejoint le parc |
| Convoi ennemi | stopper 2-3 véhicules escortés avant l'arrivée | cargaison au sol, position toutes les 2 min ; arrivé = perdu |
| Poste d'observation | neutraliser garde et radio (action) | patrouilles x2 dans 2 km tant qu'il vit, calme 60 min après |
| Sabotage | saboter un mortier, un AA ou un radar (action) | la garde campe à 300 m, infiltration possible |
| Officier ennemi | éliminer l'officier, qui fuit à 250 m | perdu s'il s'échappe à 1,5 km |
| Zone minée | 5-9 mines déminées ou explosées | renforts pendant le travail |
| Matériel abattu | 2-4 caisses de pièces près d'une épave | équipe de récupération ennemie à mi-temps |
| Colis médical | soigner un civil blessé (> 90 %) | perdu s'il meurt (ACE) |
| Dépôt de carburant | paquets de carburant à ranger à la citerne | long, renforts |
| Ravitaillement largué | 3-5 caisses dans 500 m, 45 min | patrouilles x3 |
| Documents | prendre la mallette (action), la ramener ; tombe avec le porteur | sans alerte = récompense x1,5 |
| Écoute radio | 5 min à moins de 50 m d'une antenne sans alerte | renseignement sur les garnisons |
| Panne en route | événement : un véhicule du parc tombe en panne loin (5 %/h) ; réparer ou ramener | renforts |
| Village à libérer | capturer un secteur ennemi renforcé (règles du territoire) | poids 3, jamais sans secteur ennemi |
| Contrôle routier | jamais tiré au sort : un soldat l'établit où il veut, sur une route ; tenir 20 min, contrôler les voitures civiles (voir ci-dessous) | prime par saisie et arrestation, malus par suspect passé |

Le rapport de fin cite le chef et le nombre de joueurs vus sur le site (le comptage automatique des états de service est décoché : un officier crédite après le débriefing). Chaque succès monte la menace du territoire.

#### Apparition des missions, « Trouver », « Ordonner », captures (refonte du 21/09/2026, à compiler et à essayer)
Chaque mission porte une **origine** (`SRP_EMissionOrigin`) : générateur, trouvée, ordonnée, capture, soldat (contrôle routier), événement (panne).
- **Générateur** : DEUX missions automatiques en permanence, jamais plus (`m_iMinActive` 2 ; l'ancienne « chance d'une mission de plus » est sans effet). Une mission attend **3 heures** d'être acceptée (`m_iAcceptMinutes` 180, toujours en pause quand des joueurs sont dans les environs). « Village à libérer » n'est plus tiré au sort.
- **Radio allégée** : mission automatique ou panne = une ligne courte à tous ; mission trouvée ou ordonnée = les officiers et les intéressés ; une mission jamais prise expire sans bruit (journal seulement). Les fins de mission restent annoncées à tous.
- **« Trouver une mission »** (tableau des missions, première page) : tout soldat formé choisit la distance à la base — proche (2 à 3,5 km), moyenne (3,5 à 6 km), lointaine (6 à 9 km ; réglages `m_fBandNear`, `m_fBandMid`). Type et lieu tirés au sort. La mission va au tableau : il faut ensuite la **demander au commandement** comme les autres. Limites : 4 missions au plus au tableau (`m_iMaxActive`, automatiques + trouvées), 15 min entre deux demandes d'un même soldat (`m_iFindCooldownMinutes`), pas de nouvelle tant que la précédente trouvée n'a pas été prise.
- **« Ordonner une mission »** (Lieutenant, Capitaine, Staff) : au tableau, type → localité (ou « Capture d'un secteur ennemi » → secteur) → chef de mission parmi les soldats en ligne, ou « laisser au tableau ». Créée et accordée d'un coup, le chef reçoit l'ordre à l'écran. Lieu libre : aucune règle de distance aux joueurs ou aux autres missions, seule la base (800 m) est interdite ; si le type ne peut pas se poser là, le menu le dit. **Depuis Discord** : commande `/ordonner type lieu [chef]` (rôles du commandement), avec listes proposées ; le jeu reçoit `ordonner <n° de type | capture> <identité du chef | -> <lieu>` (capacité `ordonner` du pont).
- **Capture de secteur = mission** (type `LIBERER`, renommé « Capture de secteur ») : demandée au tableau et accordée sur Discord comme avant, ou ordonnée par un officier ; à l'accord, une vraie mission est créée au nom du chef (`CreateCapture`). **Aucun délai** tant qu'on se prépare. **Engagée** dès qu'un soldat entre à 3 km du secteur (`m_fCaptureZoneRadius`) : annonce radio, plus d'abandon possible (seul le Staff annule). **Échec** si la zone reste vide 15 minutes (`m_iCaptureAbsentMinutes`, avertissement à 5 min de la fin) : la garnison se reconstitue et la **menace monte d'un point** (`OnCaptureFailed`). **Réussite** à la capture du secteur.
- **Capture hors mission** : toujours possible, mais sans la subvention de 500 € ni crédit de mission ; les officiers en jeu et le salon commandement sont prévenus (événement `commandement`). Le bouton Staff « Capturer » verse toujours la subvention.

#### Contrôle routier (`SRP_Checkpoint.c`, refonte du 22/09/2026, version 1.0.26)
**C'est un soldat qui décide du lieu.** Sur une route, il **regarde un équipier** et choisit **« Établir un point de contrôle ici »** (jauge de 3 s ; action `SRP_CheckpointStartAction` de `Character_Base.et`, visible seulement sur un autre joueur vivant, à pied). Le serveur (`StartControl`) vérifie : soldat formé (pas une recrue : grade, FGI ou Staff) ; **aucun autre contrôle à moins de 1 km** (plusieurs sections peuvent tenir chacune un point) ; à **plus de 1 500 m de la base** ; une route à moins de 40 m (le point est calé sur le sommet de route le plus proche) ; pas dans un secteur ennemi ; 2 soldats à moins de 40 m. La mission est créée (difficulté 1, prise en charge par ce soldat), le marqueur posé, la radio annonce, le chrono de **20 minutes** part, le **salon commandement du Discord est prévenu**. **3 minutes sans personne à moins de 300 m = point abandonné** (échec, mais les primes déjà gagnées sont versées). La nuit, une **lanterne** est posée à 4 m du point. Seul, pour un essai : menu Staff → Rapide → Essais → « Établir un point de contrôle routier ici, seul ».
- **Deux files** : l'axe de la route au point sépare les deux sens d'arrivée. Dans chaque file, une seule voiture est **au point** ; les suivantes s'arrêtent 40 m derrière et avancent quand la place se libère (état `EN_ATTENTE`). Une nouvelle voiture n'est posée que si une file compte moins de 2 voitures (4 au total). La circulation ambiante ne pose plus rien à moins de 1 km d'un point établi.
- **Circulation** : voitures posées sur une route à 450-800 m, hors de vue ; les voitures civiles qui passent à moins de 200 m **en se rapprochant** sont prises aussi. Elles s'arrêtent seules au point (frein à main). Une voiture en approche ou en attente qui **passe le point de 15 m à plus de 8 km/h** est déclarée « force le barrage » : annonce, cible légitime, malus si elle s'échappe à 900 m, reddition si on la stoppe. Non contrôlée au bout de 4 min, une voiture repart.
- **Contrôle** : au contact du conducteur (ou du véhicule), « Contrôler » ouvre le menu « Contrôle du véhicule » : papiers, faire descendre, fouiller (20 s), laisser repartir, arrêter, **désamorcer**. Papiers : un suspect sur deux a des **papiers faux** (ligne rouge : suspect certain, arrestation possible sans fouille) ; les autres suspects et tous les innocents ont des papiers en règle. **Arrêter** exige un motif (fouille positive ou papiers faux).
- **Descendre, braquer, menotter** (ACE Captives) : « descendre » ne provoque plus de fuite. Un conducteur descendu **braqué** (joueur à moins de 15 m qui le vise) **lève les mains** et ne bouge plus ; les **serflex** du joueur fonctionnent (menotté avec motif = arrestation comptée ; sans motif, message « fouillez ou libérez »). « Arrêter » le menotte et le met à genoux. Un suspect qui fuit ne le fait que si personne ne le vise, 6 s après être descendu ; braqué, rattrapé (4 m) ou **blessé**, il s'arrête (état `RENDU`), reste soignable puis attachable.
- **Repartir** : le conducteur **remonte à pied** dans son véhicule (ordre d'embarquement, relancé toutes les 20 s, téléportation de secours après 60 s), puis part **au-delà du point** (plus de demi-tour) et rejoint la circulation ordinaire une fois dégagé.
- **Suspects** : 1 conducteur sur 5 (le 6e d'office s'il n'y en a eu aucun). Cargaison : armes 40 %, documents 25 %, ennemi en civil 20 %, explosifs 15 %. L'ennemi en civil sort une arme (55 %), force (20 %) ou se rend (25 %). Barrage forcé : le stopper (10 s immobile ou moteur hors d'usage) = il se rend.
- **Crime de guerre** : aucune amende pour un **suspect révélé** : barrage forcé (annoncé ou non), arme sortie, fuite à pied, cargaison trouvée à la fouille. Un civil non révélé reste protégé (1 000 € pour la base, infraction).
- **Explosifs** : après la fouille, « Désamorcer » (30 s, à moins de 6 m, certification Génie ou Staff, interrompu si on s'éloigne) rend le véhicule inoffensif et compte la saisie ; sans désamorçage, pas de saisie.
- **Prime** : 1 500 € (difficulté 1) et 10 paquets + 750 € par saisie + 400 € par arrestation − 750 € par suspect passé au travers (jamais négative). En cas d'abandon, les primes des saisies et arrestations sont versées quand même (pas la subvention de base). Documents saisis = le renseignement de l'écoute radio, à la fin.
- **Menace** : 1 mission sur 4, entre la 5e et la 15e minute, une vague ennemie ordinaire vers le point.
- **Nettoyage** : véhicules des conducteurs tués, des hostiles expirés, des fuyards échappés et des explosifs immobilisés sont retirés après 10 min sans joueur à 100 m ; à la fin, un véhicule n'est jamais retiré avec un joueur dedans ou à moins de 5 m, un conducteur arrêté escorté reste.
- **Journal** `CONTROLE` : une ligne à chaque changement d'état d'une voiture (approche, attente, arrêt, repart, force, fuit, hostile, rendue, retirée), avec le n° du véhicule, la file et la distance au point ; papiers, fouilles, désamorçage, mains levées, menottage, fin du point.
- Réglages : catégorie « SimpleRP - Contrôle routier » du gestionnaire de missions : durée, soldats requis, distances (base, contrôle précédent, route), part de suspects, fouille, cadence des voitures, menace, primes, paquets.

### 9.11 Territoire
Les **secteurs** sont créés automatiquement depuis les lieux de la carte (villages, et hameaux si coché → secteurs « poste »), à plus de 1 500 m de la base, jusqu'au maximum réglé ; un marqueur `SRP_Secteur` ajoute un lieu à toi (dépôt de carburant, carrière). Tous à l'ennemi au départ (`territoire.json`). **Garnison** posée quand un joueur approche (distance réglable), 4 groupes minimum répartis dans la zone, retirée après 10 min sans personne à portée. **Capture** : aucun ennemi en état de combattre dans le rayon et un joueur présent pendant N minutes (la garnison doit avoir existé) → radio, journal, drapeau bleu, 500 €, menace +1, mission Village à libérer réussie. **Contre-attaque** : toutes les 1-3 h, avec 4 joueurs connectés minimum, sur un secteur à nous : annonce 15 min avant, jusqu'à 3 vagues (500 m, effectif selon la menace) ; l'ennemi qui tient sans joueur pendant N minutes reprend le secteur (menace -1). **Production** : chaque heure, un secteur à nous ajoute ses paquets (village 10 vivres, dépôt 8 carburant, carrière 6 pièces, poste 6 munitions) dans une caisse posée au centre, plafond 3 h, reposée après un redémarrage. **Menace** 0-10. **Marqueurs** : drapeau uni (11) bleu à nous, rouge ennemi ; attaque annoncée 49 attack orange, en cours 50 attack-main rouge ; le drapeau d'un secteur ennemi **clignote** rouge tant qu'il reste des ennemis en zone (joueur à portée) et orange pendant le compte de capture, avec le compte dans le texte. Onglet Territoire au PC, `/territoire`.

#### Dépôt de secteur (21/09/2026, à compiler et à essayer)
La production d'un secteur n'apparaît plus à 3 m de son centre (souvent dans une maison ou sur une route) : elle va au **dépôt** du secteur, que les joueurs fixent eux-mêmes.
- **Le kit de dépôt** (`Prefabs/Paquets/SRP_KitDepot.et`, objet de 40 kg et 45 litres : trop gros pour un sac, il voyage dans le coffre d'un véhicule ; poids et volume se règlent dans le prefab) s'achète au **terminal logistique**, 500 € (`Configs/SRP_Catalogue.conf`), et est livré comme les autres achats.
- Sur place, on le **pose au sol** à l'endroit voulu, dans le rayon d'un secteur à nous qui n'a pas de dépôt, puis action **« Installer le dépôt ici »** (5 s). Réservé aux **logisticiens (certification Logi), aux officiers et au Staff** ; tout le monde peut le transporter. Aucune autre contrainte de lieu.
- **Sans dépôt, la production attend** : les paquets s'accumulent sans caisse jusqu'au plafond de 3 heures, et la caisse apparaît au dépôt avec tout ce qui attendait dès la pose. Une annonce radio le rappelle à la première production.
- Le dépôt **ne bouge plus**. Secteur perdu : dépôt, caisse et contenu perdus, nouveau kit à la reprise. Après un redémarrage, la caisse est reposée au dépôt.
- Repérage : marqueur « Dépôt de <secteur> » sur la carte du jeu (icône 31, `m_iDepotMarkerIcon`), icône dorée sur la carte web (elle bat quand des paquets attendent), annonce radio avec le carré à chaque production.
- Staff (menu Staff → Territoire → un secteur) : « Fixer le dépôt du secteur ICI » (sans kit, pour un essai ou une correction) et « Retirer le dépôt » (le stock en attente est gardé).
- Sauvegarde : `depot`, `dx`, `dy`, `dz` par secteur dans le fichier du territoire.

### 9.12 Ambiance civile
**Ajouts du 20 sept. 2026.** *Base interdite* : aucun civil dans `m_fBaseExclusionRadius` (300 m) autour du marqueur de la base — ni apparition, ni promenade, ni trajet qui la traverse (à pied ou en voiture, jugé en ligne droite), ni fuite ; une localité dont le centre y tombe ne s'anime pas. *Circulation routière* (`m_bTraffic`) : toutes les 20 s, pour chaque groupe de joueurs hors de la base, jusqu'à 2 voitures dans 1 500 m sont posées **sur une route** (réseau routier du jeu), hors de vue (450 m au moins), tournées vers les joueurs, avec pour destination une localité située au-delà d'eux : on les croise entre deux villes ; ensuite elles vont de localité en localité. Les conducteurs ont leur propre plafond (`m_iMaxDrivers`, 8) : les promeneurs ne leur prennent plus toutes les places. Journal CIVILS : « Circulation : une voiture posée sur la route à … m des joueurs, en route vers … ».

Une zone par localité de la carte (ville 300 m, bourg 200, village 130, hameau 80, avec leurs comptes de promeneurs, postés, voitures garées et en circulation), animée quand un joueur est à moins de 1,5 km, en veille à 2,5 km, plafond global 40, un PNJ tué revient 10 min plus tard. Les promeneurs **marchent** (réglage de vitesse de groupe, celui du Game Master). **Réflexes de danger** : un tir entendu à 250 m (100 m si silencieux) fait fuir en sprint à l'opposé, 150 m, derrière un bâtiment si possible ; une balle qui passe à 10 m ou une explosion à 40 m fait plonger 6-12 s puis fuir ; arrivé, on se cache accroupi ; après 2 min de calme on se relève, au pas, et les postés rentrent. Les dangers arrivent par le même chemin que pour les soldats (crochet sur `SCR_AIConfigComponent.PerformDangerReaction`). Le butin sur un civil est un crime de guerre.

### 9.13 Charte
`charte.txt` (créé avec un texte de départ, `version: N` en tête). L'écran s'ouvre 3 s après le spawn tant que la version courante n'est pas acceptée (« J'accepte » / « Plus tard »). Sans acceptation : Recrue, et ramené au spawn au-delà de 50 m. `/charte`, `/charte <pseudo>`, `/charte reset` (nouvelle version, tout le monde réaccepte), `/charte recharger`. Le Staff n'est pas concerné.

### 9.14 Écrans : terminal, menu Staff, Gestion du soldat, PC
Même mécanique (`SRP_ListMenu`) : le serveur envoie le contenu, le client affiche des lignes (message, section, ligne simple, ligne cliquable), le choix repart en commande. Le **PC** ajoute une rangée d'onglets et une zone défilante : Effectifs (connectés par grade, annuaire, fiche au clic), Ma fiche, Instructeurs, Missions, Territoire, Journal (80 lignes), Base (dépôts, coffre, parc), Officiers (promotions en attente, commandes, livraisons, renseignement, civils). La mise en page se règle dans l'éditeur de layouts (largeur = `WidthOverride`).

### 9.15 Menu Staff (`/admin`)
Téléportations (à un joueur, un joueur à soi, à la base, sur la carte), soin, invincible (persiste au respawn), heure, états de tous les systèmes, lancer/annuler une mission, wipe.

### 9.16 Voix
F9 (rebindable) cycle trois portées : chuchoté 5 m, normal 30 m, fort 68 m (vanilla), message à l'écran. Trois composants de voix sur tous les personnages, chacun avec son projet audio ; le contrôleur vanilla est moddé pour choisir. À valider à deux joueurs, puis retirer ModularVoiceRange.

### 9.17 Jour/nuit
Une journée = 3 h réelles (réglage du Workbench ; le défaut du code reste 6 ; 24 = temps réel ; l'API attend des secondes, le composant convertit), l'heure sauvegardée toutes les 5 min et rétablie au démarrage, 8 h au tout premier lancement. `/heure`, `/heure 22:30`, `/heure vitesse 4`. `SRP_DayNightComponent.IsNight()` (statique) dit s'il fait nuit d'après le lever et le coucher du soleil du monde : l'éclairage et les lanternes s'en servent.

### 9.18 Déconnexion
Copie du corps (même prefab, même équipement, allongée, sans IA) pendant 15 min. Retour à temps : reprise ; copie tuée : mort ; 15 min sans retour : mort sauf à moins de 2 000 m de la base ; parti inconscient : mort immédiate (case) ; Staff exempt (case). Mort en absence = spawn à la base avec la dotation, message au retour. `/deco` pour les officiers. À l'arrêt du serveur, les corps en attente sont jugés sur la distance.

### 9.19 Outils Staff
`/ban` persistant (`bannis.json`, définitif ou en jours, expulsion à l'enregistrement), `/unban`, `/bans`, `/kick`, `/warn` (compté sur la fiche), `/mp`, `/staff`, `/stock`, `/caisse`, `/euros`, `/flotte reset`, `/civils reset`, `/territoire`, `/charte`, `/heure`, `/marqueurs` (index des icônes et couleurs du config vanilla), `/wipe confirmer` (missions, IA, garnisons, flotte, coffre, dépôts, civils, territoire : tout sauf les fiches).

### 9.20 Éclairage de la base
Ajout 1.0.26 (carte #54), script `SRP_Generateur.c`, document de pose `Eclairage_base_PAG.md`.

- **Générateur** : prefab `SRP_Generateur.et` (dérivé de `GeneratorPortable_US_01` du jeu) posé au garage par Jack dans le calque `BaseFR/Eclairage.layer`. Actions au contact : « Démarrer / Couper le générateur » (tout le monde, 1 s ; refusée en panne) et « Réparer le générateur » (30 s, logisticiens Logi, officiers, Staff ; visible seulement en panne). Détruit (sondage de `SCR_DamageManagerComponent` toutes les 5 s, comme le parc) = **en panne** ; à la réparation l'épave est remplacée par un neuf au même endroit.
- **Ce qu'il commande** : tout ce qui éclaire à moins de 300 m de `SRP_SpawnBase` (`QueryEntitiesBySphere`), sur le serveur puis chez tous les clients (RPC broadcast, comme `SCR_NightModeGameModeComponent`) : lampes interactives (`SCR_BaseInteractiveLightComponent.ToggleLight`, dont nos `SRP_Plafonnier` / `SRP_Neon` et le projecteur `GeneratorFloodlight_US_01`), lumières statiques (`LightEntity.SetEnabled` : `LightCeiling_01_on`, `LightIndustrial_02_on_interior`, scialytique), lampadaires et mâts à `StreetLampComponent` (`SetBroken`). Les lampes à pétrole (`SCR_LampComponent`) restent le secours. Pendant une coupure, l'interrupteur des lampes électriques de la base refuse d'allumer (classe moddée `SCR_SwitchLightUserAction`, « Pas de courant ») ; éteindre reste possible ; une lampe rallumée ou apparue est réteinte à la vérification (60 s).
- **Persistance** : `generateur.json` (état 0/1/2, dernier changement). Joueur arrivé en cours : état transmis par `RplSave/RplLoad` du composant, appliqué 2 s après.
- **Calque** : les prefabs interactifs posés dans un calque doivent être en `Initial Light State = LIT` (`LIT_ON_SPAWN` reste éteint pour une entité chargée avec le monde) ; les `SRP_Plafonnier` / `SRP_Neon` le sont déjà.
- **Lanternes** : `SRP_GenerateurComponent.SpawnLantern(vector)` / `RemoveLantern(IEntity)` posent et retirent une `LanternMilitary_US_01` allumée (créée en jeu, `LIT_ON_SPAWN` suffit). Les livraisons s'en servent : une livraison en cours (retenue à `ChoosePoint`) a sa lanterne la nuit, retirée le jour ou quand la livraison est terminée (un joueur venu à moins de 30 m puis tous repartis à plus de 300 m, ou 180 min). Le contrôle routier pourra faire de même.
- **Mode nuit du Game Master** : `SCR_NightModeGameModeComponent` sur le game mode, `m_bAllowGlobalNightMode` = 1 ; dans l'éditeur GM le Staff a le bouton « Mode nuit » ; `SRP_GenerateurComponent.SetGmNightMode(bool, playerId, author)` pour le menu Staff.
- **Journal** : catégorie `BASE` à chaque changement, message aux joueurs à moins de 300 m du générateur.

---

## 10. Discord

Plan et script dans `discord/` : sept rôles calqués sur les grades (Visiteur, Recrue, Soldat, Sous-officier, Officier, Instructeur, Staff), sept catégories (Accueil avec règles et candidatures, Vie du serveur, Opérations avec les flux automatiques missions/territoire/logistique, Formations avec promotions, Commandement avec effectifs, Staff avec journal et alertes, Vocal PC/Alpha/Bravo/Charlie/Formation/Staff). `setup_discord.py` construit tout via l'API, avec le jeton du bot dans une variable d'environnement (`DISCORD_TOKEN`, `DISCORD_GUILD`), et écrit `webhooks.json`. Reste à faire dans le mod : le **pont** (lecture de `discord.json` dans le profil, envoi des événements du journal sur les webhooks par catégorie), à condition que le serveur ait l'accès réseau sortant.


### Demandes de mission (suivi de mission par Discord)

Fichier `SRP_MissionRequests.c`, aucun réglage : tout passe par le bot PAG.

1. Au **tableau des missions**, l'action « Demander une mission » ouvre un assistant : une mission libre du tableau ou la capture d'un secteur ennemi, puis l'effectif, coché parmi les joueurs en ligne. « Envoyer la demande » puis Valider.
2. La demande part au bot, qui la publie dans `⭐┃commandement`. Les officiers en ligne en jeu sont prévenus aussi. Une seule demande ouverte par soldat, une seule demande par cible.
3. Un officier prend l'assignation sur Discord et décide en message privé : accorder (à pied, ou motorisé avec les véhicules garés à la base, plus des consignes), ou refuser avec un motif. Le choix des véhicules est une consigne, le mod ne bloque rien.
4. Accordée : une mission est acceptée au nom du demandeur, même sans le grade ou la certification Chef de groupe, puisqu'un officier l'a décidé. Tout l'effectif reçoit la décision, le transport et les consignes.
5. Le suivi est automatique : mission réussie ou perdue, secteur capturé. Le soldat peut annuler au tableau, l'officier peut clôturer depuis Discord. Une demande en attente tombe si le demandeur reste déconnecté 5 minutes.

Les demandes sont en mémoire, comme les missions : un redémarrage du serveur les efface, et le bot les marque alors comme annulées.


### Votes sur top-serveurs.net (depuis le 22/09/2026)

Un vote n'est compté que s'il est **rattaché à un soldat par son identité**. Deux chemins, et seulement ceux-là :

1. **En jeu**, `/vote` au chat : le mod tire un code de 5 caractères, répond au joueur avec son lien court `penguinassaultgroup.fr/vote/<code>` (valable 2 h) et envoie `code|identité|pseudo` au bot au contact suivant (clé `votereq` de l'état, retirée de la file seulement quand le bot a répondu). Le lien renvoie vers la page de vote top-serveurs avec le pseudo déjà rempli.
2. **Sur Discord**, `/vote` : accepté seulement si le compte Discord est lié au soldat (`/lier`). Bouton direct vers la page de vote, pseudo du soldat déjà rempli.

Une seule demande en cours par soldat, quel que soit le chemin. Un `/vote` retapé en jeu redonne le même lien pendant 2 h. Le bot réclame le vote chaque minute les 20 premières minutes et autour de la fin du lien, toutes les 5 minutes entre les deux, et garde la demande 10 minutes après la fin du lien pour ne pas perdre un vote fait juste avant. L'API top-serveurs (`claim-username`) ne laisse réclamer un vote qu'une fois et ne distingue pas les majuscules.

Vote réclamé : merci en privé sur Discord si le compte est lié, annonce dans le salon choisi, puis commande `vote @identité <prime> <horodatage>` au jeu. Le mod ajoute la prime au coffre (écrit aussitôt), compte le vote sur la fiche (`votes`, affiché sur la fiche du site et dans le panneau Discord), l'écrit au journal `VOTE` et remercie le joueur en ligne. Il garde les horodatages des 10 derniers votes comptés (`voteStamps`) : une commande renvoyée n'est jamais comptée deux fois. Pour un même soldat, le bot n'envoie un vote qu'après la confirmation du précédent. Le mod annonce la capacité `vote2` : sans elle (ancien mod, qui comptait chaque renvoi), le bot garde les votes réclamés en attente et ne les envoie qu'au mod à jour.

Garde-fous : au plus un vote crédité par soldat et par heure (au-delà, le vote est mis de côté dans `vote_manual` de l'état du bot et journalisé en erreur pour le Staff) ; un pseudo porté par plusieurs soldats n'est jamais réclamé (le joueur est prévenu) ; une réclamation dont la réponse s'est perdue est rattrapée au tour suivant. Un vote fait sans `/vote` n'est jamais réclamé seul ; s'il date de moins de 2 h, sous le pseudo exact du soldat, il est pris à la demande suivante de ce soldat. Limite : top-serveurs n'authentifie pas le pseudo ; un vote tapé sous le pseudo exact d'un soldat qui a une demande en cours lui est crédité, la prime allant de toute façon au coffre commun.

Réglages, par le propriétaire du bot sur Discord : `/vote-reglages jeton:<jeton de votes de la fiche> salon:<salon d'annonce> prime:<euros>`. Le jeton n'est écrit que dans `config.json` du bot.

---

## 11. Leçons Enforce et Workbench (le cahier des pièges)

**Enforce**
1. Jamais « Compile & Reload » : fermer et relancer le Workbench.
2. `string.Format` : neuf paramètres maximum (`%1` à `%9`).
3. Pas d'opérateur `%` (modulo) : `n - (n/5)*5`.
4. `map`, `set`, `array`, `base` : à éviter comme noms de variables.
5. Cas empilés (`case A: case B:`) refusés : `if/else if` ou un cas par bloc.
6. `override` sans fonction de base = erreur ; `out` avec valeur par défaut = erreur.
7. Opérateur composé sur un élément de tableau (`tab[i] -= x`) refusé : variable intermédiaire.
8. `string.Replace` modifie en place et retourne un entier : ne pas l'enchaîner dans une expression. `string.Trim` retourne la chaîne.
9. Les noms d'énumération se vérifient dans le mod source (`Ctrl+Shift+F` dans le Script Editor) ; en cas de doute, `typename.EnumToString` et une comparaison de texte.
10. Les valeurs par défaut des attributs sont des chaînes ; un attribut ajouté après coup prend la valeur par défaut du code sur les composants déjà posés.
11. Une action d'entrée = l'action + son contexte (`IngameContext`) + le libellé dans `keyBindingMenu.conf`.
12. `SCR_InventoryStorageManagerComponent` est le gestionnaire des **personnages** : sur un prop il plante (NULL pointer) ; un conteneur prend `SRP_ContainerInventoryStorageManagerComponent` (dérivé du gestionnaire des véhicules), et le moteur n'accepte qu'un gestionnaire par entité (le second est grisé).
13. Un agent IA reste dans son groupe après la mort ou dans l'inconscience : compter les « en état de combattre ».
14. `SetDayDuration` attend des secondes réelles par journée ; le gestionnaire d'heure s'obtient par `ChimeraWorld.GetTimeAndWeatherManager()`.
15. `SCR_AIInfoComponent.SetMovementType` est une coquille vide dans le vanilla : la vitesse d'un groupe se règle par `SCR_AIGroupCharactersMovementSpeedSetting` (origine EDITOR) sur `SCR_AIGroupSettingsComponent`.
16. Les événements de danger des IA ne se lisent pas : on les intercepte en moddant `SCR_AIConfigComponent.PerformDangerReaction`.
17. CRX modde les composants vanilla (`SCR_AIGroupInfoComponent`, `SCR_AIInfoComponent`) : ses réglages se posent par script au spawn.

**Persistance**
18. À l'arrêt du monde, les entités disparaissent dans un ordre indéfini : un composant qui recompte à ce moment lit du vide. Écrire le dernier état connu.
19. Le Workbench crée puis supprime une instance jetable du game mode au lancement : un composant qui sauvegarde à sa suppression doit vérifier qu'il a chargé, sinon il écrase le fichier avec du vide.
20. Un fichier de sauvegarde de l'ancien format se relit à zéro : supprimer le fichier pour repartir, ou forcer les valeurs (`/stock`).

**Interface**
21. Un mot-clé inconnu dans un layout fait sauter tout le bloc qui le contient, enfants compris.
22. Largeur d'un dialogue hérité de `ConfigurableDialog` : `WidthOverride` sur son premier `SizeLayoutWidget`. Hauteur d'un `SizeLayoutWidget` : `AllowMinDesiredHeight 1` / `MinDesiredHeight N`. Offsets d'un `FrameWidgetSlot` : vers l'intérieur, en positif. Un `ButtonWidget` n'a qu'un enfant.
23. La liste vanilla (`SCR_ListBoxComponent`) dépend de layouts disparus en 1.8 : lignes à nous (`SRP_TerminalEntry.layout`).
24. Les composants `.meta` des `.conf` système portent le GUID vanilla : à copier, jamais recréer.

**Prefabs et monde**
25. Un composant hérité ne se supprime pas dans un prefab enfant : on le décoche (Enabled) ou on repart d'un prop nu.
26. Sans `SCR_AIWorld` (navmesh) rien ne bouge ; sans `PerceptionManager` les IA ne s'initialisent pas.
27. Le PNG d'une texture doit être un vrai PNG 8 bits non entrelacé (ou TGA) ; « bad parameters to zlib » = fichier mal formé.
28. Les uploads de fichiers passent dans la conversation ; le texte collé dans le champ « document » arrive vide.

---

## 12. État, tests à faire, feuille de route

**Validé en jeu** : identité, fiches, grades, certifs, menu au contact, spawn multiple, arsenal et facturation, flotte, citerne, trésorerie et terminal, paquets et dépôts (sauvegarde bouclée), voix F9 (seul), civils qui marchent et réagissent, territoire (garnison, capture, sauvegarde, drapeaux), PC, charte, jour/nuit, navmesh en place.

**À tester** : les seize missions une par une (`/missions nouvelle <type>`), surtout convoi, officier, mines, épave, documents, écoute, largage, colis médical (ACE) ; la voix à deux joueurs puis retrait de ModularVoiceRange ; les contre-attaques à 4 joueurs (ou seuil à 1 pour tester) ; la production horaire ; la déconnexion (corps, 15 min, 2 km) ; les bans ; les réflexes civils sous le feu ; les réglages CRX déduits (`SetInvestigateRadius`, `SetInvestigateBuildingSearch`, `SetIdleObserve`, `SetHoldPositionRadius`, énumérations) si la compilation râle.

**Feuille de route** : pont Discord (webhooks) ; mise à jour d'après-ouverture (« DLC ») : raid annoncé sur la base, checkpoint à tenir ; finitions : modèle 3D du paquet (un colis), caisse de livraison refaite sur un prop nu avec le bon gestionnaire, icônes ; à décider : HUD (déjà minimal), messages radio.
