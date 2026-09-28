# Carte des dépendances du système de territoire (SimpleRP), en vue d'une refonte « carrés + zones + front »

Dossier analysé : `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\`. Analyse en lecture seule, aucun fichier modifié.

## 0. Ce qu'il faut retenir

Aujourd'hui, un secteur est un cercle : un nom qui sert de clé partout, un centre, un rayon, un genre (hameau, village ou ville) et un propriétaire (ENNEMI ou NOUS).

- **Une seule boucle fait tout.** `SRP_TerritoryComponent.Tick()` tourne toutes les 5 s (`SRP_Territory.c:779-1083`) et gère la garnison, la capture, la perte, la production, les marqueurs et la contre-attaque.
- **13 fichiers utilisent directement `SRP_TerritoryComponent` ou `SRP_SectorState`.** Il y a aussi environ 8 dépendances indirectes et 4 lecteurs hors du mod (bot, carte web, site Campagne, campagne.py).
- **La notion de voisinage n'existe pas.** Aucune adjacence ni ligne de front dans le code : le seul « voisin » est `NearestEnemySectors`, c'est-à-dire les secteurs ennemis les plus proches à vol d'oiseau, à moins de 4 km (`SRP_Territory.c:1609-1636`).
- **Aucune surface n'est dessinée.** La carte en jeu n'affiche qu'une icône par secteur (`PlaceMarker`, `SRP_Territory.c:2100-2163`).

## 1. Réglages réellement en service

Ils viennent du prefab `Prefabs\MP\Modes\Plain\SRP_GameMode.et:178-185` et remplacent les valeurs par défaut du code.

- **Capture :** `m_iCaptureMinutes 2` (le code dit 10 par défaut).
- **Perte :** `m_iLoseMinutes 15` (le code dit 10).
- **Garnison :** `m_fGarrisonSpawnDistance 2000`, `m_fGarrisonDespawnDistance 3500`.
- **Secteurs automatiques :** `m_bAutoHamlets 1` et `m_iAutoMax 100`. Ce sont eux qui donnent la centaine de secteurs.
- **Sauvegarde locale du Workbench :** `profile\SimpleRP\territoire.json` contient 30 secteurs, 1 à nous (Vernon), menace 1, enregistré le 25/09 à 17:28.
- **Aucun marqueur `SRP_Secteur` posé à la main** n'a été trouvé dans les fichiers de l'addon : tous les secteurs sont automatiques.

## 2. Ce que propose SRP_Territory.c

### Énumérations et classes
- **`SRP_ESectorType`** (`:45`) : VILLAGE (vivres), CARREFOUR, DEPOT (carburant), CARRIERE (pièces), POSTE (munitions).
- **`SRP_ESectorOwner`** (`:54`) : ENNEMI = 0, NOUS = 1.
- **`SRP_SectorComponent`** (`:66-156`), c'est-à-dire le marqueur posé à la main :
  - statique `GetSectors()` ;
  - `GetLabel`, `GetCenter`, `GetResource`, `GetProductionPerHour` ;
  - statique `TypeName(int)`.
- **`SRP_SectorDef`** (`:160-213`) :
  - champs `m_sName`, `m_vCenter`, `m_fRadius`, `m_iType`, `m_iGarrisonBase`, `m_iProductionOverride`, `m_bAuto`, `m_sKind` (« hameau », « village » ou « ville ») ;
  - `GetCenter`, `GetResource`, `GetProductionPerHour`, statique `FromComponent`.
- **`SRP_SectorState`** (`:217-274`) :
  - **Sauvegardé :** `m_sName`, `m_iOwner`, `m_sLastChange`, `m_iStock`, `m_iProductionMs`, `m_bDepot`, `m_vDepot`.
  - **En mémoire seulement :** `m_Sector`, `m_aGroups` (la garnison), `m_iHoldMs`, `m_iEnemyHoldMs`, `m_iNoPlayerSince`, `m_iAttackState` (0, 1 ou 2), `m_iAttackTick`, `m_iWaves`, `m_Box`, `m_Marker`, `m_DepotMarker`, `m_iMarkerMode`, `m_iActiveInZone`, les champs de pose différée, `m_iPlannedSoldiers`, `m_bNightPosture`, la radio (`m_RadioOperator`, `m_bRadioLost`, `m_iRadioBlockedUntil`), les camions (`m_iEntryTick`, `m_bEntryTruckDone`, `m_iAlertTruckDue`, `m_bAlertTruckDone`) et la sirène (`m_Siren`, `m_bPoleWanted`…).

### Méthodes publiques de `SRP_TerritoryComponent`

| Méthode | Ligne | Appelée depuis |
|---|---|---|
| statique `GetInstance` | 494 | partout |
| `FindState(nom)` | 709 | Admin 751, 1438 · Missions 1545 · EnemyTrucks 1024, 1044, 1095, 2129, 2351 |
| `CountOurs` | 720 | Admin 264 |
| `GetThreat` | 732 | Admin 264, 742 · Bridge 369 · Delivery 227 · Enemy 969 |
| `GetEnemySectors` | 739 | HeliSearch 370 · Missions 803 (chemin jamais atteint, voir 4.6) |
| `GetStatusText` | 749 | ChatCommands 406 · PC 534 |
| `ReducePlanned(nom, n)` | 1148 | Enemy 3652 |
| `TryRadioCall(pos, now)` | 1268 | Enemy 2805, et interne en 1202 |
| `GetRadioReport` | 1393 | Admin 437 |
| `OnGarrisonAlert(nom, …)` | 1569 | Enemy 2622 |
| `NearestEnemySectors` | 1609 | EnemyTrucks 1097, et interne en 1848 |
| `IsInsideEnemySector(pos, marge)` | 1641 | Missions 1122, 1946 · Enemy 2778 |
| `AddThreat` | 1959 | interne seulement |
| `OnMissionSuccess(type)` | 1971 | Missions 2906 |
| `ForceOwner(nom, owner, auteur)` | 1981 | Admin 1449, 1451 · ChatCommands 422, 424 |
| `GetStates` | 1997 | Admin 708 · Bridge 362, 378, 461 · Sirene 455 · EnemyTrucks 442 · MissionRequests 209, 375, 429, 481, 630 |
| `ForceAttack` | 2005 | Admin 1453 |
| `ResetAll` | 2034 | Admin 1433, 1676, 2180 · ChatCommands 413 |
| `InstallDepot` | 2191 | `SRP_DepotInstallAction` (`:2443-2478`) |
| `ForceDepot`, `RemoveDepot` | 2225, 2242 | Admin 1461, 1455 |
| `OnCaptureFailed(nom)` | 2333 | Missions 1614 |
| `Save` | 2356 | interne |

### Méthodes protégées qui portent la logique
`Start` (547), `AddAutoSectors` (591), `SectorRadiusFor` (657), `KindOf` (669), `KindFromRadius` (681), `Tick` (779), `GarrisonSoldiers` (1091), `OnGarrisonGone` (1163), `GarrisonTrucksTick` (1180), `UpdateRadio` (1218), `GarrisonAt` (1293), `HasGarrisonRoom` (1434), `MarkZone`, `WantPole`, `UpdatePole`, `RetirePole` (1494-1562), `OnGarrisonSpotted` (1591), `Capture` (1663), `Lose` (1737), `LaunchAttack` (1793), `SendWave` (1834), `SendWaveGroup` (1887), `EndAttack` (1900), `Produce` (1922), `UpdateBlink` (2072), `PlaceMarker` (2100), `OurSectorAt` (2171), `SetDepot`, `ClearDepot`, `DeliverStock`, `PlaceDepotMarker` (2263-2329), `Load` (2402).

## 3. Les règles actuelles, lues dans le code (SRP_Territory.c)

1. **Création des secteurs** (`:547-653`) :
   - chaque localité de la carte (`SRP_CivilZoneComponent`, genre ville, bourg, village ou hameau) devient un secteur ;
   - à plus de 1 500 m de la base, sans doublon à moins de 300 m, les plus proches de la base d'abord, 100 au plus ;
   - rayon selon le genre : hameau 150, village 250, bourg et ville 350 (`:657-665`) ;
   - un bourg compte comme une ville pour la garnison (`:669-676`) ;
   - type : un hameau est un POSTE (munitions 6/h), tout le reste est un VILLAGE (vivres 10/h) (`:624-627`).
   - **Conséquence :** carburant et pièces ne sont jamais produits par le territoire. DEPOT et CARRIERE n'existent qu'avec un marqueur posé à la main, et il n'y en a aucun.
2. **Pose de la garnison** (`:801-905`) :
   - une seule garnison posée par passage : celle du secteur ennemi le plus proche d'un joueur actif, à moins de 2 000 m ;
   - 3 garnisons vivantes au plus (`m_iGarrisonMaxActive`, limite de 128 IA du jeu) ;
   - quand le plafond est atteint, la garnison la plus lointaine est retirée si elle est à plus de 1 200 m de tout joueur, non vue et pas en alerte (`:851-867`).
3. **Effectif** (`:1091-1123`) :
   - base de 10, 20 ou 30 soldats (hameau, village, ville) ;
   - +3 par joueur proche au-delà du 2e, 18 au plus ;
   - +0,5 par point de menace, arrondi au-dessus ;
   - +4 par groupe de base en plus sur un secteur posé à la main ;
   - plafond de 60.
4. **Complément et pose différée** : complément quand plus personne n'est à moins du rayon + 400 m (`:920-940`) ; groupes différés réessayés toutes les 30 s (`:906-919`).
5. **Retrait de la garnison** :
   - personne à moins de 3 500 m pendant 10 min (`:954-968`) ;
   - garnison anéantie et personne à moins de 1 500 m : elle est oubliée, et une garnison neuve sera posée à la prochaine approche (`:972-986`).
6. **Capture** (`:994-1004`) :
   - il faut un joueur dans le rayon, aucun soldat de la garnison de ce secteur en état de combattre dans le rayon, et une garnison qui a existé et dont la pose est terminée ;
   - la capture tombe après `m_iCaptureMinutes` (2 min en service).
   - Les patrouilles ambiantes et les gardes de mission ne comptent pas.
   - Sans garnison posée (plafond des 3 atteint), la capture est impossible.
7. **Effets de la capture** (`:1663-1734`) :
   - la garnison est retirée, sauf les groupes loin du centre ou vus, retirés plus tard ;
   - la sirène se tait et le poteau reste ;
   - menace +1, drapeau bleu ;
   - 500 € seulement si une mission de capture est active (`HasActiveCapture`) ; sinon le commandement est prévenu ;
   - événement de passerelle `secteur_pris` et appel `missions.OnSectorCaptured(nom)`.
8. **Contre-attaque** (`:1072-1079`, `:1793-1917`) :
   - toutes les 60 à 180 min, avec au moins 4 joueurs connectés ;
   - cible : le secteur à nous le plus proche d'un joueur ; annonce 15 min avant ;
   - vagues toutes les 5 min, 3 au plus, de `min(ScaleGroupsNear(centre, menace/4, 4), LocalRoom)` groupes ;
   - origines : les 2 secteurs ennemis les plus proches à moins de 4 km, groupes partis par la route (`SpawnWaveFrom`) ; sans voisin, une vague à 500 m.
9. **Perte** (`:1030-1042`, `:1737-1788`) :
   - l'ennemi tient le rayon sans joueur pendant 15 min ;
   - le dépôt, la caisse et le stock sont perdus, menace -1 ;
   - les assaillants deviennent la garnison (`ConvertToGarrison`) ; événement `secteur_perdu`.
10. **Production** (`:1017-1027`, `:1922-1953`) :
    - chaque heure, dans la caisse du dépôt ; plafond de 3 heures ;
    - sans dépôt, rien n'apparaît ;
    - le dépôt se pose avec un kit dans le rayon d'un secteur à nous (`OurSectorAt`, `:2171-2187` ; `InstallDepot`, `:2191-2221`).
11. **Menace globale de 0 à 10** (`:1959-1977`) :
    - +1 par capture, par mission réussie, par capture abandonnée ;
    - -1 par perte, par contre-attaque repoussée, par poste d'observation, officier ou sabotage réussi.
12. **Sauvegarde** (`:2356-2437`) : `$profile:SimpleRP/territoire.json`, clé par NOM (`s<i>_name`, `owner`, `changed`, `stock`, `depot`, `dx`, `dy`, `dz`) plus `threat`. Un état sans secteur du même nom dans le monde est ignoré (`:576-580`).

## 4. Les dépendances, fichier par fichier

### 4.1 SRP_EnemyGarrison.c (composition et pose de la garnison) — critique
- **`:80-105` `Spawn`** : `state.m_Sector.m_sKind` pour `BuildPlan`, puis `state.m_aGroups` et `m_aDeferredSlots`.
- **`:122-131`** : QG (groupe de commandement : opérateur radio, tireur d'élite) seulement si le genre est « ville » et la garnison d'au moins 13 soldats.
- **`:247-252`** : postes d'entrée selon le genre (hameau 1, village 1, ville 2).
- **`:276-283` `PlacePlan`** : centre et rayon du secteur. Distance minimale aux joueurs = max(300, rayon) ; pose différée si un joueur est dans le rayon.
- **`:296`** : `SRP_Placement.FindTownEntrances(centre, rayon)`, les entrées de la localité sur ses routes.
- **`:320`** : parts du cercle des sections (rayon × 0,8).
- **`:410`** : `record.m_sZone = state.m_sName`. Chaque soldat sait à quelle localité il appartient (sirène, camions).
- **`:477-484` `SoldierCap`** : place gardée aux camions (`GetSoldierReserve`), avec `CountPlayersNear(centre)`.
- **`:634-641` `SwitchPosture`** : centre et rayon, pour poster la nuit les rondes en trop aux entrées.
- **En jeu :** toute la garnison est pensée pour une localité (QG au centre, binômes aux entrées, rondes dans les rues, bâtiments).

### 4.2 SRP_Enemy.c (socle de l'IA) — important
- **`:197-201`** : `SRP_EnemyGroup.m_sZone`, le nom du secteur de la garnison.
- **`:966-969` `ThreatFactor`** : `GetThreat()` multiplie la chance de jeep et de renforts motorisés (×1 à ×2).
- **`:1072-1095` `SpawnGroups`** :
  - pour `owner == "territoire"`, le rayon tenu (`holdRadius`) est le rayon du secteur ;
  - distance minimale aux joueurs = rayon du secteur ;
  - pose différée si un joueur est dans le rayon ;
  - un seul groupe de commandement.
- **`:1526-1541`** : rôle CRX GARNISON (`hold` = rayon du secteur), tâche « poste ».
- **`:1841-1865` `ConvertToGarrison`** : les assaillants d'un secteur pris deviennent sa garnison (point Defend dans le rayon).
- **`:2097-2114` `SpawnWaveFrom`** : vague qui part d'un secteur (contre-attaque).
- **`:2400-2414`** : jeep d'une garnison (`owner == "territoire"`), comptée dans le plafond.
- **`:2590-2622` `AlertTick`** : un soldat avec un `m_sZone` qui VOIT un joueur appelle `territory.OnGarrisonAlert`, donc sirène et camion d'alerte.
- **`:2776-2809` `MaybeCallHeli`** :
  - l'alerte doit être dans un secteur ennemi (rayon + `m_fHeliSiteDistance`, 900 m par défaut) ou près d'une mission ;
  - puis `TryRadioCall` : si l'opérateur radio est mort, l'appel est retardé.
- **`:3627-3652` `CloseStalledSpawns`** : soldats jamais livrés par la file d'apparition, retirés de l'effectif prévu (`ReducePlanned(m_sZone)`).
- **`:4228` `GetTerritoryGroupCap`** : plafond de groupes hors missions (garnisons, camions, jeeps, ambiance).
- **Aucune dépendance :** la présence ambiante ignore le propriétaire des secteurs. Des patrouilles peuvent apparaître en zone « bleue » ; seule la base est exclue.

### 4.3 SRP_EnemyTrucks.c (camions de renfort) — important
- **`:5-9`** (en-tête) : camion d'entrée quand un joueur arrive à 500 m (`m_fTruckEntryDistance`, `:138-139`) du centre d'une localité gardée ; camion d'alerte par radio.
- **`:322-340` `GetSoldierReserve`** : drapeaux camion d'entrée et camion d'alerte du secteur.
- **`:349-378` `Dispatch`** : `m_vTown` = centre, `m_fTownRadius` = rayon, `state.m_iEntryTick`.
- **`:433-487` `ForceTruck`** (Staff) : `GetStates`, localité ennemie garnie la plus proche, à moins de 3 km.
- **`:493` `RecallSector(nom)`** : garnison partie, camion rappelé.
- **`:635`, `:2346-2355` `IsSectorHeld`** : la localité est-elle toujours ennemie et garnie ? Sinon le camion est annulé.
- **`:984-992`, `:1037-1050` `HeldStateNear`** (rayon + 300 m) : le chauffeur rejoint la garnison (`m_sZone`, `m_aGroups`).
- **`:1019-1026` `ReleaseDriver`** : `FindState`.
- **`:1088-1103` `CollectOrigins`** : départs depuis les 6 secteurs ennemis voisins (`NearestEnemySectors`), puis les lieux-dits, puis une couronne de sondes.
- **`:1301-1311`** : stationnement dans la localité (0,5 × rayon, `m_fParkFactor`, `:165`).
- **`:987`, `:1960`** : la moitié des débarqués défend 0,5 × rayon.
- **`:1996`** : dernière alerte à moins du rayon + 600 m.
- **`:2126-2141`** : les débarqués rejoignent la garnison de la localité.

### 4.4 SRP_Sirene.c — moyen
- **Clé par nom de localité (« town ») :** `SpawnPole(nom, centre)` (`:152`), `RemovePole` (`:237`), `Request` (`:302`), `Silence` (`:436`).
- **Le poteau est au centre du secteur**, il sonne si le joueur vu est à moins du rayon + 150 m (`SRP_Territory.c:1569-1583`, `m_fSirenZoneMargin` `:311`).
- **`:447-478` `GetAlertReport`** (page Staff) : `GetStates`, propriétaire, `m_Siren`, centre.

### 4.5 SRP_HeliSearch.c — faible
- **`:317-348`, `:364-387` `EnemySectorDirection`** : l'hélicoptère part du côté du secteur ennemi le plus proche situé à plus de 500 m (`GetEnemySectors`), pour qu'il « vienne de chez eux ».
- Le déclenchement lui-même est dans `SRP_Enemy.c:2778` (voir 4.2).

### 4.6 SRP_Missions.c (missions de capture #50) — critique
- **Déclarations :** `:53` et `:63` (type LIBERER, origine CAPTURE), `:139` (`m_bEngaged`), `:222-226` (zone de mission de 3 000 m, échec après 15 min de zone vide).
- **Textes :** `:723-728` (état) et `:760` (briefing) parlent de « secteur » et de « 3 km ».
- **`:792-819`** : le tirage au sort d'un « Village à libérer » via `GetEnemySectors` n'est **jamais atteint**, car `:792-793` renvoie `false` avant.
- **`:1539-1569` `CreateCapture(nom)`** :
  - `FindState` ; refusée si le secteur est à nous ;
  - site = centre du secteur, `m_sDestinationLabel` = nom.
- **`:1574-1615` `TrackCapture`** :
  - mission ENGAGÉE dès qu'un joueur est à moins de 3 km du centre ;
  - 15 min sans personne : échec, puis `OnCaptureFailed(nom)` (garnison reconstituée, menace +1).
- **`:1886-1891`** : aucune garde ajoutée, la garnison du secteur est l'objectif.
- **`:1936-1950` `SpawnPendingGuards`** : dans un secteur ennemi (rayon + 200 m), pas de garde de mission, pour ne pas faire doublon avec la garnison.
- **`:1121-1123`** (dans `CheckControl`) : contrôle routier interdit dans un secteur ennemi (marge 0).
- **`:2665-2682` `FindActiveLiberer`, `OnSectorCaptured(nom)`** : succès de la mission par nom.
- **`:2870`** : une capture engagée ne s'annule que par le Staff.
- **`:2904-2906`** : `OnMissionSuccess(type)` fait bouger la menace.
- **Aucune dépendance :** `PickSite` (`:961-1000`) choisit les sites parmi les localités, les points de livraison et les marqueurs, **sans regarder le propriétaire**. Une mission peut tomber en territoire bleu comme rouge.

### 4.7 SRP_MissionRequests.c (demande de mission au tableau, suivi Discord) — important
- **`:40-41`, `:68`** : `m_bTerritory`, `m_sTarget` = nom du secteur.
- **`:203-215` `FindSector(nom)`** : via `GetStates`.
- **`:371-382`, `:426-440`** (ordonner une capture) et **`:475-498`, `:621-634`** (choisir une cible) : menus construits sur **l'indice dans `GetStates`** (`olieu:%1`, `cible:t:%1`). C'est fragile si la liste change.
- **`:714-722`** : titre = nom, carré = `GridOf(centre)`.
- **`:860-876`** : demande accordée, donc `CreateCapture`.
- **`:967-980`** : secteur passé à nous, donc demande close (réussie).
- **`:1043-1044`** : `"kind":"territoire"` envoyé au bot.

### 4.8 Contrôle routier (droit d'action) — moyen
- `SRP_Rights.c:61` appelle `missions.CanStartControl`, qui appelle `CheckControl` (`SRP_Missions.c:1137-1142`), qui appelle `IsInsideEnemySector` (`:1121-1123`).
- L'action « Établir un point de contrôle ici » disparaît dans un secteur ennemi (`SRP_Checkpoint.c:2062-2064`).

### 4.9 SRP_Delivery.c — faible
- **`:224-229`** : chance d'une garnison au point de livraison = min(40, 10 + 3 × menace).

### 4.10 SRP_Bridge.c (passerelle vers le bot) — important pour le site et Discord
- **`:252-253`** : types d'alerte `attaque`, `secteur_pris`, `secteur_perdu`. Une contre-attaque repoussée envoie aussi `secteur_pris` (`SRP_Territory.c:1914`).
- **`:354-371`** : `territory{ours,total,threat}`. Ici `ours` compte `m_iOwner != ENNEMI` et `total` compte tous les états, y compris ceux sans secteur dans le monde, contrairement à `CountOurs`.
- **`:373-395`** : `sectors[]` avec `{name, grid (carré de 100 m du centre), ours, attack, depot, stock}`.
- **`:455-475`** : `boxes[]`, production à ramasser par secteur.
- **`:783-789`** : `ordonner capture <chef> <secteur>` depuis Discord, vers `OrderFromBridge` (`SRP_Missions.c:1463-1503`).

**Lecteurs hors du mod (lus pour information) :**
- `E:\Projets\PAG-Bot\bot.py` :
  - `:122-123` : alertes ;
  - `:423-424` : statut « x/y secteurs » ;
  - `:819-837` : embed Territoire et production ;
  - `:1840-1842` : carte, **tronquée à 200 secteurs** ;
  - `:1958-1968` : site Campagne ;
  - `:2515` : autocomplétion des captures = secteurs non à nous.
- `campagne.py:112-139` : historique pris/perdu **par nom** et par carré de 100 m.
- `carte/index.html:203-216` : un drapeau par secteur. La carte sait déjà dessiner des carrés (`L.rectangle(coins(g))`, `:187`).
- `site_web/campagne.html:99-120` : le « % de l'île » y est en réalité le nombre de secteurs à nous sur le total.

### 4.11 SRP_Discord.c — faible, indirect
- **`:325-326`** : la catégorie de journal TERRITOIRE part dans le flux public « territoire ».
- **`:396-404`** : la couleur dépend de mots du texte (« captur », « défendu », « à nous », « perdu », « attaque », « vague », « produit »).
- **`:246-248`** : mots confidentiels (« garnison », « aucun secteur », « à nous, menace »).
- Il faudra garder ces mots dans les nouveaux messages.

### 4.12 Outils du Staff et des joueurs — affichage
- **`SRP_Admin.c`** :
  - `:264` : page rapide (menace, secteurs à nous) ;
  - `:433-437` : rapports sirène et radio ;
  - `:698-745` : onglet Territoire, liste de **tous** les secteurs ;
  - `:749-786` : page d'un secteur (téléport au centre, capturer, perdre, dépôt ici, retirer le dépôt, forcer une contre-attaque) ;
  - `:1425-1464` : `RunTerritoire`, par nom ;
  - `:1671-1677`, `:2177-2181` : remises à zéro.
- **`SRP_ChatCommands.c:400-425`** : `/territoire` (liste publique), et pour le Staff `capturer`, `perdre` `<secteur>` et `reset`.
- **`SRP_PC.c:317`, `:357`, `:528-544`** : onglet Territoire du PC = une ligne par secteur (`GetStatusText`).

### 4.13 Dépendances indirectes (source des lieux, géométrie)
- **`SRP_Civilians.c`** : il fournit les lieux.
  - `:424-427` : lieux de la carte (ville, bourg, village, hameau) ;
  - `:478-479` : `SetPlace` ;
  - `:183-201` : `GetPlaceName`, `GetPlaceKind`.
  - `SRP_Territory.c:599-628` s'en sert pour créer les secteurs.
- **`SRP_Fleet.c:801-807` `GridOf`** : le carré de 100 m (« 087 028 ») déjà affiché partout (dépôt, carte web, demandes). Attention : risque de confusion avec les futurs carrés de territoire.
- **`SRP_Placement.c:425-431` `FindTownEntrances(centre, rayon)`** : entrées de la localité pour les postes.
- **`SRP_CRX.c:6`, `:37-41`** et **`SRP_EnemySenses.c:798-817`** : un soldat de garnison tient 30 m autour de son poste ; le rayon du secteur ne sert qu'au point Defend.
- **`SRP_EnemyAwareness.c:9`, `:282`** : un poste d'entrée de garnison ne part pas aider (selon la tâche du groupe).
- **Appelés par le territoire seulement :** `SRP_Treasury` (`treasury.Add` à la capture, `SRP_Territory.c:1714`) et `SRP_PlayerManagerComponent` (`NotifyOfficiers`, `:1722`).

### 4.14 Aucune dépendance au territoire
- `SRP_Depot.c` : ce sont les conteneurs de la base, rien à voir avec le « dépôt de secteur ».
- `SRP_VoiceRange.c`, `SRP_Generateur.c`, `SRP_CRX.c` (hors du réglage de maintien), `SRP_MapCapture.c`, `SRP_Breche.c`, `SRP_Fleet.c` (hors `GridOf`).
- `SRP_SpawnLogic.c` : **aucune réapparition possible dans un secteur pris.**

## 5. Si les cercles deviennent des carrés et des zones : ce qui casse ou doit être adapté

| # | Priorité | Élément | Où | Ce qui casse, ce qu'il faut décider |
|---|---|---|---|---|
| 1 | CRITIQUE | Modèle de données (un secteur = un cercle, un nom) | Territory 160-274, 547-653 | Tout repose sur centre et rayon. Il faut un modèle à deux niveaux : carré (propriétaire), zone (groupe de carrés adjacents, nom), et garder le lieu. Choisir la taille du carré (voir 7). |
| 2 | CRITIQUE | Règle de capture | Territory 994-1004 | Aujourd'hui : un joueur dans le rayon, la garnison de CE secteur neutralisée, 2 min. À redéfinir : capture par carré ou par zone, seulement les carrés au contact du front, ce qui compte comme ennemi (garnison seule, ou aussi ambiance et gardes), carré sans localité donc sans garnison. |
| 3 | CRITIQUE | Garnison liée à une localité | Territory 801-1123 · EnemyGarrison (tout) · Enemy 1072-1095, 1526-1541 | La composition et la pose supposent un centre, un rayon, des entrées et un genre. Plafond de 3 garnisons et limite de 128 IA : impossible de garnir tous les carrés. Qui défend un carré rouge sans village ? |
| 4 | CRITIQUE | Contre-attaque et perte | Territory 1030-1079, 1737-1917 · Enemy 1841-1865, 2097-2114 | La cible est aujourd'hui le secteur à nous le plus proche d'un joueur, et les origines les 2 secteurs ennemis les plus proches à moins de 4 km. Avec un front : attaques depuis les carrés rouges adjacents vers les carrés bleus du front. Perte après 15 min d'ennemi sans joueur : par carré ou par zone ? |
| 5 | CRITIQUE | Missions de capture #50 | Missions 222-226, 1539-1615, 2665-2682, 2870 · MissionRequests (tout le chemin « territoire ») · Bridge 783-789 · bot.py 2515 | La cible est un nom de secteur, la zone d'engagement fait 3 km autour du centre, le succès passe par `OnSectorCaptured(nom)`. Il faudra viser une zone (ou un carré, ou un lieu) et fixer le déclencheur d'engagement. |
| 6 | CRITIQUE | Sauvegarde | Territory 2356-2437 | Clé par nom, 8 valeurs par secteur. Nouveau format (propriétaire par carré, zones), reprise des 30 à 100 états actuels (Vernon à nous en local). Le fichier est réécrit en entier à chaque changement. |
| 7 | IMPORTANT | `IsInsideEnemySector` | Territory 1641 · Missions 1122, 1946 · Enemy 2778 | Aujourd'hui, hors des cercles, tout est « libre ». Si la moitié de l'île devient rouge : contrôle routier interdit partout côté rouge, hélicoptère possible partout, plus aucune garde de mission côté rouge. Il faudra séparer « carré rouge » et « localité ennemie garnie ». |
| 8 | IMPORTANT | Camions de renfort | EnemyTrucks 322-493, 635, 984-1103, 1301-1311, 1960-1996, 2126-2141, 2346 | Localité garnie (centre, rayon), départs depuis les secteurs ennemis voisins. Ce pourrait devenir « depuis l'arrière du front rouge ». |
| 9 | IMPORTANT | Production et dépôt | Territory 1017-1027, 1922-1953, 2171-2329 · Bridge 455-475 | Production par secteur à nous (type = ressource) ; le kit se pose dans le rayon (`OurSectorAt`). Par carré, par zone ou par lieu ? Carburant et pièces ne sont jamais produits aujourd'hui. |
| 10 | IMPORTANT | Carte en jeu | Territory 2072-2163 | Une icône colorée par secteur, qui clignote en étant reposée toutes les 5 s. Le code n'a aucun moyen de dessiner une surface. Les carrés demandent un calque de carte à écrire, ou un drapeau par zone. |
| 11 | IMPORTANT | Menace | Territory 1701, 1748, 1911, 1959-1977, 2347 | +1 par capture, -1 par perte. Avec des captures par carré, la menace grimperait vite : à rattacher aux zones. |
| 12 | IMPORTANT | Radio et sirène | Territory 1218-1308, 1494-1604 · Sirene 152-478 · Enemy 2805 | Clé par nom de localité (rayon + 300 m, rayon + 150 m). Inchangé si la notion de lieu est gardée. |
| 13 | MOYEN | Hélicoptère | HeliSearch 317-387 · Enemy 2776-2800 | Direction « de chez eux » = secteur ennemi le plus proche ; ce pourrait devenir la profondeur rouge. |
| 14 | MOYEN | Subvention de capture | Territory 1704-1723 | 500 € par capture couverte par une mission : par zone, sinon inflation. |
| 15 | AFFICHAGE | Staff, PC, /territoire | Admin 264, 698-786, 1425-1464 · ChatCommands 400-425 · PC 528-544 | Listes de TOUS les secteurs : inutilisables avec des centaines de carrés. Passer à une liste de zones et du front. |
| 16 | AFFICHAGE | Menus des demandes par indice | MissionRequests 380, 432, 492, 633 | Clés = indice dans `GetStates`, à refaire. |
| 17 | AFFICHAGE | Passerelle et site | Bridge 354-395 · bot.py 1840 (200 au plus), 423, 819, 1958 · campagne.py 112-139 · carte/index.html 203-216 · campagne.html 99-120 | `sectors[]` par nom et carré de 100 m ; « x/y secteurs » ; frise par nom. La carte web sait déjà tracer des rectangles et peut montrer le front. |
| 18 | AFFICHAGE | Discord | Discord 246-248, 325-326, 396-404 | Couleurs et filtre selon des mots : les garder dans les nouveaux messages. |
| 19 | INCHANGÉ | Menace globale ailleurs | Enemy 966-979 · Delivery 224-229 | Rien à changer si la menace reste globale. |

## 6. Notions à garder, quelle que soit la refonte

- **Le lieu (localité)** : nom, centre, genre (hameau, village, ville ; bourg compté comme ville), entrées sur ses routes, bâtiments. Il porte :
  - la garnison de 10, 20 ou 30 soldats (+ joueurs + menace), le QG seulement en ville, les postes d'entrée 1, 1 ou 2 ;
  - le poteau de sirène au centre, l'opérateur radio ;
  - le stationnement des camions, la capture des objectifs.
  Même si le contrôle passe au carré, la localité reste le point fort qu'on attaque et défend.
- **Une clé de nom stable et unique.** Elle sert à la sauvegarde, à la sirène (`town`), aux camions (`m_sSector`), aux groupes (`m_sZone`), aux missions (`m_sDestinationLabel`), aux demandes (`m_sTarget`), à l'historique de campagne.py et à l'autocomplétion du bot. Les zones et les lieux doivent garder des noms fixes.
- **Le propriétaire ENNEMI = 0 / NOUS = 1**, éventuellement complété par « contesté » ou « neutre ».
- **Les compteurs d'état** : attaque 0 (rien), 1 (annoncée), 2 (en cours) ; vagues ; temps de capture et de perte.
- **Les garde-fous de l'IA** :
  - garnison posée à l'approche (2 km), retirée à 3,5 km après 10 min ;
  - 3 garnisons au plus (128 IA) ;
  - pose hors de vue, pose différée ;
  - jamais de disparition sous les yeux d'un joueur (`RetireLater`, survivants).
- **La menace globale de 0 à 10** et ses effets (garnisons, vagues, jeeps, livraisons).
- **Le dépôt par kit, la production et la caisse**, ainsi que la mission de capture activée au tableau (chef, engagement, échec si la zone reste vide, subvention seulement dans ce cadre).
- **Les messages radio et journal TERRITOIRE**, avec leurs mots-clés pour les couleurs Discord.

## 7. Chiffres utiles pour le QCM

- **Everon fait 12 800 m de côté** (une grande part est de la mer). Nombre de carrés selon la taille :

  | Taille du carré | Grille | Carrés |
  |---|---|---|
  | 100 m (taille de `GridOf`, déjà affichée partout) | 128 × 128 | 16 384 |
  | 250 m | 52 × 52 | 2 704 |
  | 500 m | 26 × 26 | 676 |
  | 1 km | 13 × 13 | 169 |

  Le bot coupe à 200 éléments (`bot.py:1840`), et la boucle de 5 s parcourt tous les états avec un calcul de distance au joueur le plus proche pour chacun (`Tick`, `:871-877`).
- **Les rayons d'aujourd'hui** (150, 250, 350 m) montrent la taille d'une localité : un carré de 500 m couvre à peu près un village.
- **Limites fixes** : 3 garnisons vivantes, 36 groupes (`m_iMaxGroups`, `SRP_GameMode.et:91`), 128 IA actives dans le jeu.
- **La présence ambiante ignore déjà le propriétaire.** Un front « vivant », avec patrouilles côté rouge et calme côté bleu, serait une nouveauté à coder dans `SRP_Enemy.c`.

## Fichiers

- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Territory.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_EnemyGarrison.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Enemy.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_EnemyTrucks.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Missions.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_MissionRequests.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Sirene.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_HeliSearch.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Bridge.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Admin.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_ChatCommands.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_PC.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Delivery.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Discord.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Civilians.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Rights.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Placement.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Fleet.c`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Prefabs\MP\Modes\Plain\SRP_GameMode.et`
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\profile\SimpleRP\territoire.json`
- `E:\Projets\PAG-Bot\bot.py`
- `E:\Projets\PAG-Bot\campagne.py`
- `E:\Projets\PAG-Bot\carte\index.html`
- `E:\Projets\PAG-Bot\site_web\campagne.html`