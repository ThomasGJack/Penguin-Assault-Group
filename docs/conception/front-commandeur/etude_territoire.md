# Système de territoire actuel de SimpleRP : état des lieux

J'ai tout lu, sans rien modifier. Toutes les lignes citées sont celles de `SRP_Territory.c`, sauf quand un autre fichier est nommé.

**Deux écarts avec le contexte donné :**
- **Il y a 30 secteurs, pas ~100.** `m_iAutoMax` vaut bien 100 dans le prefab, mais c'est un plafond. Everon ne donne que 30 candidats : le journal affiche « 30 secteur(s) automatique(s) depuis la carte (30 candidats) » le 14/09 et le 25/09. Les deux fichiers `territoire.json` contiennent aussi 30 secteurs.
- **Le guide est dépassé.** Sa section 9.11 et sa table de réglages parlent de 4 groupes minimum, de vagues à 500 m, d'une caisse au centre et d'une garnison à 1200 m. Ses « valeurs connues » (capture 15 min, 3 000 m / 3 000 m) ne correspondent plus au prefab.

Fichiers utilisés :
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Scripts\Game\SimpleRP\SRP_Territory.c` (2478 lignes)
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\addons\SimpleRP\Prefabs\MP\Modes\Plain\SRP_GameMode.et` (l.178-185). La couche du monde `Worlds\EveronSimpleRP\EveronSimpleRP_Layers\GameMode.layer` ne surcharge rien.
- `%USERPROFILE%\Downloads\GUIDE_SIMPLERP.md` (§4, §7, §8, §9.11). C'est le seul exemplaire trouvé, il n'y en a aucun sous le dossier du mod ni sous Documents.
- `%USERPROFILE%\Documents\My Games\ArmaReforgerWorkbench\profile\SimpleRP\territoire.json` et `%USERPROFILE%\Documents\My Games\ArmaReforger\profile\SimpleRP\territoire.json`
- Aussi : `SRP_EnemyGarrison.c`, `SRP_Enemy.c`, `SRP_EnemyTrucks.c`, `SRP_Missions.c`, `SRP_Admin.c`, `SRP_PC.c`, `SRP_ChatCommands.c`, `SRP_Bridge.c`, `SRP_Civilians.c`, `SRP_Definitions.c`, `E:\Projets\PAG-Bot\carte\index.html`, `E:\Projets\PAG-Bot\bot.py` (l.121-127 seulement).

---

## 0. Valeurs réglées dans le prefab (SRP_GameMode.et, l.178-185)

Seuls 6 attributs de `SRP_TerritoryComponent` sont surchargés. Tous les autres gardent la valeur du code.

| Attribut | Défaut du code | Réglé |
|---|---|---|
| m_iCaptureMinutes (l.284) | 10 | **2** |
| m_fGarrisonSpawnDistance (l.296) | 2000 | 2000 |
| m_fGarrisonDespawnDistance (l.299) | 3500 | 3500 |
| m_iLoseMinutes (l.408) | 10 | **15** |
| m_bAutoHamlets (l.429) | 0 | **1** |
| m_iAutoMax (l.444) | 12 | **100** |

Réglages liés sur d'autres composants du prefab :
- `SRP_EnemyComponent` : m_iMaxGroups 36 (défaut 14), faction « AFRF » avec les groupes RHS MSV VKPO. m_iMaxSoldiers reste à 100 et m_iMissionGroupReserve à 6, donc le plafond de groupes du territoire est 36 − 6 = 30 (SRP_Enemy.c l.4228).
- `SRP_DayNightComponent` : m_fRealHoursPerDay 3.

---

## 1. Ce qu'est un secteur

**Classes**
- `SRP_SectorComponent` (l.61-156) : composant qu'on pose à la main. Attributs : m_sLabel, m_eType, m_fRadius (150), m_iProductionOverride (-1), m_iGarrisonBase (1). Il s'enregistre côté serveur seulement (l.93).
- `SRP_SectorDef` (l.160-213) : définition du secteur (nom, centre, rayon, type, base de garnison, m_bAuto, m_sKind « hameau », « village » ou « ville »).
- `SRP_SectorState` (l.217-274) : l'état du secteur, persistant et vivant.

**Types** (`SRP_ESectorType`, l.45-52) : VILLAGE (vivres), CARREFOUR (rien), DEPOT (carburant), CARRIERE (pièces), POSTE (munitions).

**Propriétaires** (`SRP_ESectorOwner`, l.54-58) : ENNEMI = 0, NOUS = 1. Il n'y a que deux valeurs.

**Création** (Start, l.547-586, lancé 5 s après OnPostInit, l.508)
- Secteurs manuels : on prend les `SRP_SectorComponent` du monde, et le genre se déduit du rayon (KindFromRadius, l.681-688).
  - Aucun n'est posé dans le monde.
  - Le prefab `SRP_Secteur.et` cité par le guide n'existe pas dans l'addon.
- Secteurs automatiques (AddAutoSectors, l.591-653) : ils viennent des zones civiles (`SRP_CivilZoneComponent`). Ces zones sont elles-mêmes créées depuis les noms de lieux de la carte (SRP_Civilians.c l.424-427) :
  - MDT_NAME_CITY donne « ville » ;
  - MDT_NAME_TOWN donne « bourg » ;
  - MDT_NAME_VILLAGE donne « village » ;
  - MDT_NAME_SETTLEMENT donne « hameau ».
- Filtres appliqués :
  - hameaux seulement si m_bAutoHamlets est coché (il l'est) ;
  - plus de 1500 m du marqueur `SRP_SpawnBase` (m_fAutoMinDistance, l.441), ce qui exclut Levie ;
  - pas de doublon : même nom ou moins de 300 m d'un secteur existant (l.611) ;
  - tri du plus proche au plus loin de la base, puis limite à m_iAutoMax.

**Caractéristiques d'un secteur automatique**
- Centre : l'origine de l'entité du nom de lieu, c'est-à-dire le label de la carte, pas le barycentre des bâtiments.
- Nom : le nom affiché, traduit.
- Type : hameau → POSTE, tout le reste → VILLAGE (l.624-627).
- Genre : KindOf (l.669-676), où le bourg compte comme une ville.
- Rayon (SectorRadiusFor, l.657-665) : hameau 150 m, village 250 m, bourg et ville 350 m, jamais moins de 60 m. Ce rayon est indépendant du rayon civil.

**Les 30 secteurs réels**, du plus proche au plus loin de la base : Chotain, Régina, Perelle, Durras, Morton, Figari, Provins, Bénac, Vernon, Le Bosc, Entre-Deux, Saint-Pierre, Montignac, Richemont, Lacourt, Gorey, Lancre, Gravette, Kervel, Le Moule, Villeneuve, Tyrone, Kermovan, Meaux, Étoupe, Redon, Lamentin, Saint-Philippe, Les Creux, Erquy.
- Rayons vus au journal : Durras 250, Le Bosc 150, Régina 250, Saint-Pierre 350.
- Surface couverte : entre 2,1 km² (tout à 150 m) et 11,5 km² (tout à 350 m) sur 163,8 km², soit 1,3 à 7 % de l'île.

**Un secteur = un point + un cercle.** Il n'existe ni polygone ni notion de voisinage.

---

## 2. Règles de capture exactes

**Boucle** : Tick toutes les 5 s (TICK_MS, l.481 et l.584). Chaque secteur est traité aux l.871-1070.

**Condition de capture** (l.994) :
```
if (playerInside && activeInZone == 0 && !state.m_aGroups.IsEmpty() && SRP_EnemyComponent.IsSettled(state.m_aGroups, now))
```

Ce que recouvre chaque terme :
- **playerInside** : le joueur le plus proche est à une distance 3D inférieure ou égale au rayon (l.877 et l.880). La liste vient de `SRP_Utils.GetPlayerCharacters` (SRP_Definitions.c l.662-674). Elle n'est filtrée ni sur la mort ni sur le délai de grâce, alors que la pose des garnisons, elle, filtre ces deux cas.
- **activeInZone** : `ActiveAgentsNear(state.m_aGroups, centre, rayon)` (SRP_Enemy.c l.3450). On compte les soldats vivants et conscients, dans le rayon, et appartenant aux groupes de la garnison uniquement. Cela inclut la jeep (l.898-900) et les débarqués de camion qui rejoignent la garnison (SRP_EnemyTrucks.c l.2139-2140). Cela exclut :
  - les patrouilles ambiantes ;
  - les gardes de mission ;
  - les ennemis hors du rayon ;
  - les inconscients ;
  - les soldats rendus, qui sortent de leur groupe.
- **Une garnison doit avoir été posée.** Un secteur sans garnison n'est pas capturable (par exemple si elle a été retirée, ou si le plafond de 3 garnisons est atteint).
- **Compteur m_iHoldMs** : +5000 ms à chaque passage. Il repart à 0 au premier passage où la condition tombe (l.1003-1004) : pas de pause, pas de décroissance progressive. La capture a lieu à m_iCaptureMinutes × 60 000, soit **2 min** avec le prefab.
- Un seul joueur suffit. Le nombre de joueurs n'accélère rien, et il n'y a aucun rapport de force.

**États**
- L'état persistant n'a que deux valeurs : ENNEMI ou NOUS.
- Les états « vivants », non sauvegardés, sont :
  - m_iAttackState : 0 rien, 1 annoncée, 2 en cours (l.234) ;
  - m_iMarkerMode (l.240) : 0 fixe, 1 « combat » (clignote rouge), 2 « capture » (clignote orange).
- Aucun état « contesté » ni « neutre » n'existe.

**À la capture** (Capture, l.1663-1734) :
- le propriétaire passe à NOUS, la date est notée, les compteurs sont remis à zéro ;
- les groupes de garnison vus par un joueur ou à plus de rayon + 300 m deviennent des « survivants » retirés plus tard (RetireLater) ; les autres sont supprimés aussitôt ;
- OnGarrisonGone("capture") remet les drapeaux à zéro et rappelle les camions (l.1163-1172) ;
- la sirène se tait, le poteau reste ;
- la menace prend +1 (l.1701) et le drapeau passe au bleu ;
- 500 € (m_iCaptureReward, l.423) sont versés seulement si `HasActiveCapture` est vrai (SRP_Missions.c l.1526) ou si le Staff force la capture. Sinon :
  - une ligne de journal est écrite ;
  - l'évènement pont « commandement » est envoyé ;
  - les officiers en jeu sont prévenus (l.1715-1723) ;
- sauvegarde, journal, évènement pont « secteur_pris », puis `NotifyAll("Radio : Secteur X capturé — menace N/10")` ;
- missions.OnSectorCaptured termine la mission LIBERER en SUCCÈS (SRP_Missions.c l.2677-2682), et OnMissionSuccess ajoute encore +1 de menace (SRP_Missions.c l.2904-2906, Territory l.1971-1977). **Une capture sous mission donne donc +2 de menace.**

**Mission de capture** (type LIBERER, « Capture de secteur », SRP_Missions.c) :
- elle est engagée dès qu'un soldat entre à 3 km (m_fCaptureZoneRadius, l.222) ;
- elle échoue si la zone reste vide 15 min (m_iCaptureAbsentMinutes, l.225 ; TrackCapture l.1574-1615). Dans ce cas, `OnCaptureFailed` (l.2333-2351) supprime la garnison et ajoute +1 de menace ;
- la mission ne pose aucune garde en plus (l.1886-1890).

---

## 3. Contre-attaques ennemies

**Déclenchement** (l.1072-1079, ScheduleAttack l.1654-1658)
- Minuterie aléatoire de 60 à 180 min (m_iAttackMinMinutes / m_iAttackMaxMinutes, l.393-397). Elle est replanifiée à chaque échéance.
- L'attaque part seulement si au moins 4 joueurs sont **connectés** (m_iAttackMinPlayers, l.390), où qu'ils soient. Sinon l'échéance est simplement perdue.

**Cible** (LaunchAttack, l.1793-1831)
- Parmi nos secteurs sans attaque en cours, celui qui est le plus proche de **n'importe quel** personnage joueur, y compris ceux restés à la base. Si aucun ne ressort, un secteur au hasard.
- Une seule cible par échéance. Le Staff peut en forcer d'autres (ForceAttack, l.2005-2031).

**Déroulé**
- **Annonce** : état 1. Marqueur icône 49 orange « ATTAQUE ANNONCÉE », radio « Renseignement : l'ennemi prépare une attaque sur X, assaut dans 15 minutes », évènement pont « attaque ». L'assaut suit 15 min plus tard (m_iAttackWarningMinutes, l.399).
- **Assaut** : état 2 (l.1063-1069). Une vague part tout de suite, puis toutes les 5 min (m_iAttackWaveMinutes, l.402), 3 vagues au plus (m_iAttackMaxWaves, l.405).

**Taille d'une vague** (SendWave, l.1834-1882) : `min(ScaleGroupsNear(centre, menace/4, 4), LocalRoom(centre))` groupes.
- ScaleGroupsNear (SRP_Enemy.c l.1015) donne un groupe par tranche de 2 joueurs proches (à moins de 3 km, sans compter la base), plus menace/4 en division entière, borné entre 1 et 4.
- LocalRoom (l.1049) vaut ScaleGroupsNear(centre, 2, 8) moins les groupes déjà présents à moins de 1200 m.

**Origine des vagues**
- Au plus 2 secteurs ennemis parmi les plus proches, à vol d'oiseau, à moins de 4 000 m (NearestEnemySectors, l.1609-1636).
- Les groupes alternent entre les axes. Le premier part tout de suite, les suivants à 60-90 s d'écart (l.411-415). Ils apparaissent hors de vue, à au moins 500 m des joueurs, et rejoignent la cible par la route (SpawnWaveFrom, SRP_Enemy.c l.2100).
- Sans secteur ennemi voisin, l'ancienne vague part à 500 m (SpawnWave), éventuellement en camion (50 % × facteur de menace).

**Perte du secteur** (l.1030-1040)
- Il faut au moins un assaillant actif dans le rayon **et** aucun joueur dans le rayon, pendant m_iLoseMinutes d'affilée (**15 min** avec le prefab). Le compteur repart à 0 sinon.
- Lose (l.1737-1788) :
  - le propriétaire repasse à ENNEMI ;
  - le stock tombe à 0 ;
  - le dépôt est effacé : caisse, contenu et emplacement (ClearDepot) ;
  - la menace perd 1 ;
  - les assaillants deviennent la garnison (ConvertToGarrison) ;
  - radio « Secteur X PERDU », évènement pont « secteur_perdu ».

**Défense réussie** (EndAttack, l.1900-1917)
- Il faut que tous les assaillants soient hors de combat, **n'importe où** (rayon 0), et que les 3 vagues soient passées ou le délai de la vague écoulé. La menace perd alors 1 et la radio annonce « Secteur X défendu ».
- L'évènement pont envoyé est de type « secteur_pris » (l.1914).
- Tant qu'un seul assaillant reste vivant quelque part, l'attaque ne se termine pas.

**Rien de l'attaque n'est sauvegardé** : un redémarrage l'annule.

---

## 4. Économie

**Production** (l.1017-1027, Produce l.1922-1953)
- Chaque heure de possession, comptée par tranches de 5 s pendant que le serveur tourne, on ajoute GetProductionPerHour paquets (l.130-142) :

  | Type | Paquets par heure |
  |---|---|
  | VILLAGE | 10 vivres |
  | POSTE | 6 munitions |
  | DEPOT | 8 carburant |
  | CARRIERE | 6 pièces |
  | CARREFOUR | 0 |

- Comme il n'existe que des secteurs automatiques, **la production réelle se limite aux vivres (villes, bourgs, villages) et aux munitions (hameaux)**.
- Plafond : production horaire × m_iProductionCapHours (3, l.417), soit 30 vivres ou 18 munitions.
- m_iProductionMs n'est pas sauvegardé : l'heure en cours repart de zéro à chaque redémarrage.

**Dépôt de secteur** (#49, l.2166-2329)
- Kit « Kit de dépôt de secteur », 500 € (Configs\SRP_Catalogue.conf l.157-163), prefab `Prefabs/Paquets/SRP_KitDepot.et`.
- L'action `SRP_DepotInstallAction` « Installer le dépôt ici » (l.2443-2478) demande que le kit soit posé au sol (sans parent), dans le rayon d'un de nos secteurs qui n'a pas encore de dépôt.
- Réservé à la certification Logi, aux officiers et au Staff (InstallDepot, l.2191-2221).
- Le dépôt ne bouge plus ensuite. La caisse (SpawnBox) reçoit les paquets. Sans dépôt, le stock attend sans caisse.
- Messages radio :
  - avec dépôt, à chaque production : « X — N paquet(s) de R à ramasser au dépôt, carré G » ;
  - sans dépôt, à la première production seulement.
- Au redémarrage, la caisse est reposée 3 s après le démarrage (RespawnBoxes, l.692-704).
- Staff : ForceDepot (l.2225) et RemoveDepot (l.2242), qui garde le stock.

**Trésorerie** : 500 € par capture autorisée. La mission de capture ajoute sa propre récompense (Reward, non détaillée ici). Les secteurs ne produisent aucun revenu en euros.

**À la perte** : stock, caisse et contenu sont perdus, et le dépôt est à reposer avec un nouveau kit.

---

## 5. Garnison

**Pose** (l.785-869 et l.886-905)
- Au plus une garnison posée par passage de 5 s : le secteur ennemi le plus proche d'un joueur **actif** (ni à la base, ni en délai de grâce), si ce joueur est à moins de 2000 m.
- Au plus 3 garnisons vivantes en même temps (m_iGarrisonMaxActive, l.293 ; commentaire : limite de 128 IA du jeu).
- Plafond atteint ou plus de place : la garnison la plus lointaine laisse sa place (éviction, l.844-868) si toutes ces conditions sont remplies :
  - le nouveau secteur est plus proche des joueurs d'au moins 300 m ;
  - personne n'est à moins de 1200 m d'elle (m_fGarrisonEvictDistance, l.305) ;
  - elle n'est pas en alerte ;
  - elle n'est pas vue ;
  - elle ne subit pas d'attaque.

**Effectif** (GarrisonSoldiers, l.1091-1123)
- Base : hameau 10, village 20, ville ou bourg 30 (l.315-322).
- +3 par joueur proche au-delà de 2 (l.324-331, bonus plafonné à +18). « Proche » veut dire à moins de 3 km : CountPlayersNear prend le plus grand entre la distance demandée et m_fScaleRadius 3000 (SRP_Enemy.c l.986).
- +arrondi supérieur de (menace × 0,5), soit 0 à 5 (l.333).
- +(m_iGarrisonBase − 1) × 4 pour un secteur manuel.
- Borné entre 2 et 60 soldats (l.336). Même effectif le jour et la nuit.

**Composition** (SRP_EnemyGarrison.BuildPlan, SRP_EnemyGarrison.c l.127-189)
- QG (PlatoonHQ) seulement en ville et à partir de 13 soldats (l.339).
- Postes d'entrée en binômes sur les routes : hameau 1, village 1, ville 2 (l.342-349).
- Le reste en sections de 6, puis une de 4, puis un binôme.
- Sans QG, la plus grande section tient le centre.

**Placement**
- Centre à moins de min(0,3 × rayon, 40 m) du point central (l.366-370).
- Rondes : 50 % des sections le jour, 20 % la nuit (l.354-358). Sinon 40 % dans un bâtiment (l.360), le reste en poste.
- Toujours hors de vue, à au moins max(rayon, 300 m) des joueurs. Si c'est impossible parce qu'un joueur est dans le secteur, la pose est différée et retentée toutes les 30 s (l.906-919, NoteDeferral l.1474-1487).
- Complément de garnison quand les joueurs sont à plus de rayon + 400 m et qu'il manque au moins 2 soldats (l.920-940). Les pertes ne sont jamais remplacées : les soldats prévus comptent les morts.

**Plafonds**
- Soldats des garnisons et des camions : m_iMaxSoldiers 100, moins la réserve gardée pour les camions (SoldierCap, SRP_EnemyGarrison.c l.477).
- Groupes : 30 (GroupCap).
- HasGarrisonRoom (l.1434-1441).

**Retrait et anéantissement**
- Retrait si aucun joueur à moins de 3500 m pendant 10 min. Ces 10 min sont écrites en dur (l.954-966).
- Garnison anéantie sans capture : elle est oubliée dès que tous les joueurs sont à plus de 1500 m (m_fGarrisonWipeClearDistance, l.302 ; l.970-986). Une garnison **complète** revient à la prochaine approche.

**Extras** (carte #69)
- Jeep possible (MaybeSpawnVehiclePatrol).
- Sirène au centre (l.1494-1583).
- Opérateur radio : s'il est tué, l'appel suivant est retardé de 5 min (l.387, l.1218-1289).
- Camion d'entrée à 500 m quand au moins 2 joueurs sont à moins de 1000 m : 2 à 6 soldats, plein (10) à partir de 5 joueurs.
- Camion d'alerte 30 s après que la garnison a vu les joueurs, à partir de 5 joueurs (l.1180-1208, l.1591-1604 ; SRP_EnemyTrucks.c l.102-219).
- Posture de nuit (l.944-949).
- Les missions posées dans un secteur ennemi n'ajoutent pas de garde (IsInsideEnemySector, l.1641-1651 ; SRP_Missions.c l.1122 et l.1946).

---

## 6. Persistance, réplication, carte, menus, annonces

**Sauvegarde** : `$profile:SimpleRP/territoire.json` (PATH, l.480), Save l.2356-2386, Load l.2402-2437. Le fichier est un JSON à plat (JsonSaveContext) :
- `threat`, `savedAt`, `count` ;
- puis, par secteur : `sN_name`, `sN_owner`, `sN_changed`, `sN_stock`, `sN_depot`, `sN_dx`, `sN_dy`, `sN_dz`.

Fonctionnement :
- La clé est le nom du secteur. Un état orphelin (sans secteur dans le monde) est gardé et produit un avertissement (l.576-580).
- Écriture : à chaque passage si quelque chose a changé, et à la capture, à la perte, à AddThreat, au dépôt, à la remise à zéro et à l'arrêt.
- **Ne sont pas sauvegardés** : les garnisons et leurs pertes, les compteurs de capture et de perte, les attaques, le temps de production.
- Contenu actuel :
  - profil Workbench (25/09) : 30 secteurs, Vernon à nous, menace 1 ;
  - profil jeu (14/09) : 30 secteurs, aucun à nous.

**Réplication**
- Tout tourne côté serveur. Aucune propriété répliquée.
- Les clients voient :
  - les marqueurs statiques de la carte, répliqués par le gestionnaire de marqueurs du jeu ;
  - les messages `SRP_Notify` (NotifyAll, préfixe « Radio : ») ;
  - le texte que le serveur envoie aux menus.

**Marqueurs sur la carte du jeu** (PlaceMarker l.2100-2163, UpdateBlink l.2072-2097)
- Un seul point par secteur, au centre, de type `PLACED_CUSTOM` (InsertStaticMarker).
  - Icône 11 (drapeau), bleu (7) à nous, rouge (4) à l'ennemi ; texte « Nom (à nous) » ou « Nom (ennemi) ».
  - Attaque annoncée : icône 49, orange. Attaque en cours : icône 50, rouge.
- **Aucun cercle ni aucune zone n'est dessiné.**
- Le clignotement est fait en retirant puis en reposant le marqueur à chaque passage de 5 s :
  - mode 1 (rouge / rouge sombre) quand il reste des ennemis dans la zone et qu'un joueur est à moins de max(3 × rayon, 800 m) ;
  - mode 2 (orange / blanc) avec le texte « capture x / N min ».
- Dépôt : icône 31, bleu, « Dépôt de X » (l.2307-2329).
- Tous les secteurs sont visibles par tout le monde.

**Menus**
- **PC, onglet Territoire** (SRP_PC.c l.528-544) : affiche GetStatusText (l.749-774), public. On y lit le propriétaire, les paquets, la capture « x / N min », le nombre d'ennemis en état de combattre dans la zone et l'état des attaques.
- **Menu Staff (F10), Territoire** (SRP_Admin.c l.698-785 et RunTerritoire l.1425-1464) :
  - la liste montre, en couleur : garnison (nombre de groupes), ennemis en zone, attaque, capture, stock, distance ;
  - par secteur : Aller au centre, Capturer, Perdre, Fixer le dépôt ici, Retirer le dépôt, Forcer une contre-attaque ;
  - une remise à zéro globale ;
  - la page « État des groupes » donne la radio et les camions dans un rayon de 3 km (GetRadioReport l.1393, SRP_Admin.c l.435-437).

**Commande de chat** : `/territoire` est retiré du chat. Il renvoie vers « ordinateur (PC), onglets Base et Territoire » (SRP_ChatCommands.c l.151-157 et l.182-183). Le code qui la traite (l.400-426 : statut, et pour le Staff capturer / perdre / reset) n'est plus atteignable depuis le chat.

**Pont, Discord et carte web**
- `SRP_BridgeComponent.PushEvent` envoie « secteur_pris », « secteur_perdu », « attaque » et « commandement ». Dans bot.py (l.121-127) :
  - « attaque » s'affiche en rouge ;
  - « secteur_perdu » en rouge ;
  - « secteur_pris » en vert ;
  - « commandement » part au salon commandement.
- L'instantané d'état (SRP_Bridge.c l.354-395 et l.455-475) contient :
  - un résumé territoire : ours, total, threat ;
  - pour chaque secteur : name, grid (le carré de 100 m qui contient le centre, via `GridOf`, SRP_Fleet.c l.802-807), ours, attack, depot, stock ;
  - les caisses (boxes).
- Carte web (`E:\Projets\PAG-Bot\carte\index.html` l.203-253) : un drapeau par secteur dans son carré de 100 m, l'icône du dépôt, le compteur « n/N » et une frise d'historique.

---

## 7. Limites qui expliquent le côté « basique » (constats factuels)

1. **Tout ou rien.** La possession est binaire, ENNEMI ou NOUS. Aucun contrôle partiel, aucun état contesté ou neutre n'est enregistré.
2. **Des îlots.** Trente cercles de 150 à 350 m, collés aux noms de lieux, couvrent 1,3 à 7 % de l'île. Tout le reste n'appartient à personne.
3. **Pas de voisinage ni de ligne de front.** N'importe quel secteur ennemi peut être pris dans n'importe quel ordre, même loin derrière. Une mission LIBERER tirée au hasard choisit un secteur ennemi aléatoire (SRP_Missions.c l.798-818). Le « voisin » n'est que le plus proche à vol d'oiseau à moins de 4 km, et ne sert qu'à choisir l'origine des vagues et des camions.
4. **Capturer, c'est vider un cercle puis attendre.** Il faut neutraliser les groupes de garnison **dans le rayon**, puis qu'un seul joueur reste 2 min dans le cercle. Ne bloquent pas la capture :
   - un ennemi juste hors du rayon ;
   - une patrouille ambiante ;
   - un blessé inconscient.

   Le compteur repart à zéro au moindre ennemi actif dans le rayon.
5. **Un ennemi sans mémoire, qui n'existe qu'autour des joueurs.** Au plus 3 garnisons, et seulement dans un rayon de 2 km. Les pertes sont oubliées au retrait ou après un anéantissement. Rien n'occupe l'espace entre les localités.
6. **Pas d'initiative adverse hors de la minuterie.** Une seule contre-attaque toutes les 1 à 3 h, à condition d'avoir au moins 4 joueurs connectés. La cible est choisie par rapport au joueur le plus proche, même s'il est à la base. Conséquences :
   - aucune perte possible hors ligne ou à moins de 4 joueurs ;
   - un seul joueur dans le cercle empêche la perte indéfiniment ;
   - un redémarrage annule l'attaque.
7. **Une possession qui n'apporte presque rien.** De petites quantités de vivres ou de munitions, à aller chercher, et seulement avec un dépôt payant. Ni point d'apparition, ni lignes de ravitaillement, ni bonus lié à la contiguïté. Hors de ces systèmes, la possession ne sert qu'à définir la cible des missions, l'origine des vagues, des camions et de l'hélicoptère (SRP_HeliSearch.c l.364-387) et la règle « pas de garde de mission en secteur ennemi ».
8. **Une menace globale et unique, de 0 à 10.** Chaque capture la fait monter (+2 sous mission), ce qui renforce l'ennemi partout et pas sur place.
9. **Une représentation pauvre.** Un drapeau-point par secteur, aucune zone dessinée, et le clignotement se fait en retirant et reposant le marqueur toutes les 5 s. Les onglets et la carte web ne sont que des listes.
10. **Dette technique.**
    - Attributs obsolètes toujours présents : m_iGarrisonMin, m_iGarrisonMax, m_iGarrisonReinforceMinutes, m_iNightExtraPosts.
    - Le prefab `SRP_Secteur` est absent, donc les types DEPOT, CARRIERE et CARREFOUR ne sont jamais utilisés.
    - La commande `/territoire` n'est plus atteignable.
    - Une défense réussie est envoyée sous l'évènement « secteur_pris ».
    - Les distances des joueurs sont en 3D, et la liste des joueurs n'est filtrée ni sur la mort ni sur le délai de grâce.
    - Le guide n'est plus à jour sur ce système.