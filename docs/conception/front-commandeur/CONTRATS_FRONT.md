# CONTRATS DU FRONT (cartes #75 et #76) — figés

Ce document accompagne les 10 squelettes du front écrits dans `code/SimpleRP/` (contrats figés, corps vides qui
compilent). Il fait foi pour tous les modules qui codent autour du front. En cas de doute : arbitrages de Jack
(`plan/arbitrages.txt`) > `INTERFACES_CORE.md` > ce document > consignes `par_fichier/*.md`.

Contrôle statique (`python enforce_check.py SimpleRP`, tout le dossier) : **0 remarque** après la relecture du
26/09 (le doublon `SRP_DepotInstallAction` est retiré de la copie de `SRP_Territory.c`, qui se termine désormais à la
fin de `SRP_TerritoryComponent` ; `SRP_TerritoryComponent.InstallDepot` reste, inutilisé, jusqu'au nettoyage du §8).

**Relecture du 26/09 (deux relecteurs), corrections portées dans le code ET ici** : dépôt de zone détruit par la seule
REPRISE (G7) ; pas de réévaluation de zone après un carré forcé (Q9) ; une seule ligne de journal Staff ; une seule
porte d'AdoptAssailants (EndAttack(PERDUE), appelée par OnZoneLost) ; `IsCommanderActive` seule définition de
« Commandeur actif » (`SRP_CmdManeuvers.IsOn` supprimé) ; `SRP_FrontZone.IsAtFront` renommé `IsNearFront` ;
`SRP_FrontEnemyComponent.GetAttackState` supprimé (seule source : `GetZoneAttackState`) ; `IsZoneLinked` interne ;
sauvegarde finale par l'ennemi (`SaveFinalFromEnemy`) et garde de la chaîne ; `NotifyReset` : une porte par genre de
remise ; Q8 : repère d'abord ; boutons Staff de l'ennemi du front (trou 22) ; `SRP_FrontRadio.CaptureAbandoned` ;
difficulté de capture par la zone et mission ANNULEE pour une zone forcée (Q9, §8).

## 1. Décisions prises ici (en plus des trous tranchés)

1. **Deux composants neufs seulement** sur `SRP_GameMode.et` : `SRP_FrontComponent` et `SRP_FrontEnemyComponent`.
   `SRP_FrontCaptureComponent`, `SRP_ZoneFallComponent`, `SRP_FrontDisplayComponent`, `SRP_FrontGrid` et
   `SRP_FrontMapStyle` n'existent pas : capture (`SRP_FrontCapture`), chute (`SRP_ZoneFall`) et économie
   (`SRP_FrontEconomy`) sont des classes tenues par le socle ; radio et repères sont statiques ; la carte lit
   `SRP_FrontComponent` sur la machine du joueur.
2. **Réglages** : tous les attributs du front sont des champs PUBLICS de `SRP_FrontComponent` (et de
   `SRP_FrontEnemyComponent`), rangés par catégorie ; les classes tenues les lisent par `m_Front.<champ>`. Les
   « chiffres clés » (Q6) sont surchargés au démarrage par un fichier du profil (§7).
3. **Un SEUL lecteur de réglages pour tout le mod : `SRP_CmdSettings`** (fichier `SRP_CmdSettings.c`, classe statique,
   écrite par le Commandeur). Il gère DEUX fichiers : les clés qui commencent par `front_` vont dans
   `$profile:SimpleRP/front_reglages.txt`, toutes les autres dans `$profile:SimpleRP/commandeur_reglages.txt`.
   Contrat attendu (§8, SRP_CmdSettings).
4. **Points clés (arbitrage Q8 > relecture n°13)** : registre UNIQUE au socle, points posés AU WORKBENCH. Sources par
   priorité : CENTRE : REPERE (prefab `SRP_KeyPointComponent`) > FICHIER (ligne « point » de `front_retouches.txt`,
   dépannage signalé au menu Staff « à poser au Workbench ») > NOM_CARTE (repli, listé MANQUANT) ; QG : REPERE >
   FICHIER > aucun (ville à un seul point clé, listée MANQUANT ; jamais le nom de la carte, qui tomberait sur le
   centre). **Pas d'outil Staff « Point clé ICI », pas de `points_cles.json`, pas de QG automatique.** Un point est
   pris quand son carré est bleu (pas de `m_bTaken`).
5. **Noms de zone** : `GetZoneCode` = « S07 », `GetZoneName` = « Régina » (nom SEUL), `GetZoneLabel` = « Régina (S07) ».
   Tout texte affiché utilise `GetZoneLabel`.
6. **`SRP_FrontComponent.SetZoneOwner` est réservée à `SRP_ZoneFall`** (elle ne repeint rien) ; les carrés passent
   par `SetCellOwner` / `SetCellsOwner` (seule porte bas niveau). E4 (socle) et H2 (ennemi) appellent
   `SRP_ZoneFall.LoseZone`.
7. **Contrats neutres du Commandeur sur `SRP_FrontEnemyComponent`** : les FAITS DE JEU passent par `OnZoneCaptured`,
   `OnZoneLost`, `OnLocalityTaken`, `OnKeyPointSeized`, `OnMissionSucceeded`, `OnSabotageUsed`, `OnThreatChanged`,
   `ResetAll`, `OnRestored`, `WriteTo`/`ReadFrom`, qui relaient au Commandeur s'il existe et ne font rien sinon.
   Exceptions admises, et SEULEMENT celles-ci (toujours tester le null) : `SRP_CmdSettings` (lecteur unique) ;
   `SRP_Commander.Get().BuildRegions()` (socle, démarrage) ; `SRP_CmdScreens` (écrans : `PCZoneSuffix`,
   `PCIntelSection`, `StaffHeaderPart`, `StaffTerritoryLine` ; réglages `vu_*` lus par la radio) ; `SRP_Commander.Get()`
   pour la radio (`GetRegionLabel`, `GetBook().GetOfficerName`) et pour `SRP_Admin` (`CancelOperations`) ;
   `SRP_CmdResources.Get().GetDepots()` (repères des dépôts révélés) ; dans l'hôte ennemi lui-même :
   `SRP_CmdIntel.OnKeyPointSeized`, `SRP_CmdCapacity` (Ask, Tag, SetWantedGarrisons, GarrisonCap) et
   `SRP_CmdManeuvers` (crochets de contre-attaque, gardés par `IsCommanderActive()`).
8. **Économie** : ses données sont des champs de `SRP_FrontZone`, sauvegardés par le socle ; `SRP_FrontEconomy`
   n'écrit aucun fichier et ne fait pas K2 (versement des anciens secteurs : socle, `MigrationTick`).
9. **Fin de contre-attaque** : une seule énumération, `SRP_EAttackEnd` (SRP_Front.c), aussi rendue par
   `SRP_CmdManeuvers.CheckEnd` (pas de `SRP_ECmdEnd`).
10. **Copies datées** dans `front_jours/front_AAAA-MM-JJ.json` (30 gardées) ; archives (K1, fin de campagne) dans
    `archives/`. Écriture de front.json au plus 30 s après un changement de carré, aussitôt pour une zone, le Staff,
    le gel, la menace, une remise ou l'arrêt.
11. **Campagne** : 2 après l'import de l'ancien territoire (K1, identifiant « C2 » pour le pont), 1 sur un serveur
    neuf (Workbench).
12. **Journal** : TERRITOIRE = niveau zone (public), FRONT = carré par carré (Staff seulement, `m_bJournalCells` = 1
    par défaut), COMMANDEUR = SRP_CmdLog seul (jamais public). Tous écrits par `SRP_FrontRadio` pour le front.
13. **Staff** : l'onglet Territoire de `SRP_Admin` délègue à `SRP_FrontScreens` (StaffTab, RunStaff, IsInternalPage,
    NeedsConfirm, PageAfter), comme l'onglet Commandeur délègue à `SRP_CmdScreens`. Toutes les commandes de l'ennemi
    du front y ont un bouton (trou 22) : `territoire:attaque` (ForceAttack), `territoire:attaque-annuler`
    (CancelAttack, 2 clics), `territoire:offensive:1` / `:0` (ForceOffensive, 2 clics), page `postes`
    (GetPostsReport), lignes GetAttackReport et GetNightReport. UNE seule ligne de journal [STAFF] par action, écrite
    par `RunStaff` (les primitives appelées n'en écrivent pas).
14. **Une seule porte par fait** (relecture du 26/09) : état d'attaque d'une zone = `SRP_FrontComponent
    .GetZoneAttackState` ; « Commandeur actif » = `SRP_FrontEnemyComponent.IsCommanderActive()` ; « zone coupée » pour
    les autres modules = `IsZoneCut` ; « près du front » (E1 ou F1, filtre I3) = `SRP_FrontZone.IsNearFront()`, à ne
    pas confondre avec `IsZoneAtFront` (F1 seul) ; conversion des assaillants = `EndAttack(PERDUE)` ; texte de capture
    abandonnée = `SRP_FrontRadio.CaptureAbandoned`.

## 2. Classes et méthodes publiques, fichier par fichier

Généré depuis les squelettes (signature exacte + la ligne `//!`). Les méthodes `protected` sont dans le code, avec
leur ligne `//!` ; les attributs des composants sont au §7.

### SRP_Front.c (1725 lignes)

- `enum SRP_EFrontOwner { ROUGE=0, BLEU=1 }`
- `enum SRP_EFrontLive { AUCUN=0, CAPTURE=1, COMBAT=2, REPRISE=3 }`
- `enum SRP_EFrontReason { CAPTURE=0, SAISIE=1, REPRISE=2, ZONE_TOMBEE=3, ZONE_PERDUE=4, ISOLEMENT=5, OFFENSIVE=6, STAFF=7, REMISE=8, RESTAURATION=9 }`
- `enum SRP_EKeyPointRole { CENTRE=0, QG=1 }`
- `enum SRP_EKeyPointSource { NOM_CARTE=0, REPERE=1, FICHIER=2 }`
- `enum SRP_EZoneKind { AUCUNE=0, HAMEAU=1, VILLAGE=2, VILLE=3 }`
- `enum SRP_EFrontAttack { AUCUNE=0, ANNONCEE=1, ASSAUT=2 }`
- `enum SRP_EAttackEnd { AUCUNE=0, PERDUE=1, DEFENDUE=2, ROMPUE=3, ANNULEE=4 }`
- `enum SRP_ESeizeBlock { LIBRE=0, ENNEMIS=1, PAS_AU_CONTACT=2, GELE=3, AUTRE_SOLDAT=4, TOUCHE=5, GARNISON_EN_ROUTE=6 }`
- `enum SRP_EFrontReset { CAMPAGNE=0, RESTAURATION=1, ZONE=2 }`

**class SRP_FrontComponentClass : SCR_BaseGameModeComponentClass**

**class SRP_FrontComponent : SCR_BaseGameModeComponent**
- `static SRP_FrontComponent GetInstance()` — L'instance du game mode (serveur et joueurs), null avant OnPostInit — appelé par tout le mod
- `override void OnPostInit(IEntity owner)` — Toutes machines : s_Instance. Serveur : EnsureDirectories, dossiers front_jours et archives, crée m_Capture, m_ZoneFall, m_Economy, puis CallLater(Start, 5000) (jamais de travail ici : instance jetable du Workbench)
- `override void OnDelete(IEntity owner)` — Serveur : retire Start, Tick et FlushChanges de la file, puis Save() si m_bLoaded (SRP_Territory.c:534-544) ; si l'ennemi est déjà parti, Save refuse d'écrire un fichier amputé (voir Save) — le moteur
- `override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)` — Serveur : un joueur part, la capture oublie sa position et son exclusion (C3) — appelé par le jeu
- `bool IsReady()` — Le front est construit (serveur) ou reçu (joueur) — appelé par tout le mod avant toute lecture
- `SRP_FrontCapture GetCapture()` — Serveur : la capture (C, F3, F5, recensement unique) — appelé par la chute, l'ennemi, le Commandeur (EQ4)
- `SRP_ZoneFall GetZoneFall()` — Serveur : la seule porte des changements de zone (CaptureZone, LoseZone, ForceZone) — appelé par l'ennemi (H2), les missions, le Staff
- `SRP_FrontEconomy GetEconomy()` — Serveur : production et dépôts de zone (G4 à G7) — appelé par SRP_Bridge (boxes), SRP_Admin, SRP_DepotInstallAction
- `string ReloadSettings(string author)` — Serveur, bouton Staff « Relire les réglages » : SRP_CmdSettings.Reload(author), puis LoadSettings ici, SRP_FrontEnemyComponent.LoadSettings (qui relaie au Commandeur) et SRP_MissionManagerComponent.LoadFrontSettings — rend le compte rendu (SRP_FrontScreens, SRP_Admin, SRP_CmdScreens)
- `int GetGridSize()` — Côté de la grille en carrés (Everon : 64) — carte, pont
- `float GetCellSize()` — Taille d'un carré en mètres (200, A1) — carte, pont
- `int GetCellCount()` — Nombre de carrés de la grille (N x N = 4096), en jeu ou non — capture (dimension des tableaux)
- `int CellIndexAt(vector position)` — Carré sous une position : gz x N + gx, gz compté depuis le sud ; -1 hors grille ou en mer — tout le mod
- `int CellCol(int cell)` — Colonne gx d'un carré (sans opérateur modulo) — géométrie, carte
- `int CellRow(int cell)` — Rangée gz d'un carré, depuis le sud — géométrie, carte
- `vector CellCenter(int cell)` — Centre d'un carré, posé au sol (GetSurfaceY) ; vector.Zero hors grille — ennemi (vagues), Commandeur
- `string CellRef(int cell)` — Référence de carte du coin sud-ouest, « 074 042 » (A7, même format que SRP_Fleet.c:802) — radio, Staff, pont
- `int CellFromRef(string reference)` — Inverse de CellRef (« 074 042 » ou « 75 43 » : gx = a / 2) ; -1 si illisible — retouches, Staff
- `int Neighbour(int cell, int dir)` — Voisin par un côté, dir 0 N, 1 E, 2 S, 3 O ; -1 hors grille ou hors jeu (sans la liaison maritime) — ennemi, capture
- `int GetSeaLink(int cell)` — Carré relié par la mer (Q5 : Erquy), -1 sinon — capture (C1), calque, parcours E4
- `bool IsCellInPlay(int cell)` — Le carré est-il en jeu (terre gardée, A2) ? — tout le mod
- `int GetCellOwner(int cell)` — SRP_EFrontOwner du carré, -1 hors jeu — tout le mod
- `int GetCellZone(int cell)` — Zone du carré (0 = base), -1 hors jeu — tout le mod
- `int GetCellLive(int cell)` — SRP_EFrontLive du carré (C8) — carte (orange), PC, Staff
- `bool IsCellContested(int cell)` — Le carré est-il contesté (état différent d'AUCUN) ? — carte (orange, I1), pont (cs)
- `bool IsCellAtFront(int cell)` — Un des 4 côtés en jeu a l'autre propriétaire (sans la liaison maritime) — missions (G6), ennemi, calque (tracé)
- `bool HasNeighbourOwnedBy(int cell, int owner, bool withSeaLink)` — Un des 4 côtés (et la liaison maritime si withSeaLink) appartient à owner — chute (D2), capture, économie
- `bool CanBlueCapture(int cell)` — C1 + Q5 : carré rouge, hors base, voisin bleu par un côté ou par la mer, front non gelé — capture (seule à capturer)
- `bool CanRedRetake(int cell)` — F3 + B4 : carré bleu, hors base, hors zone protégée bleue, front non gelé — capture (reprise)
- `bool IsBaseCell(int cell)` — Carré de la zone de la base (B2) — capture, Commandeur (sécurité des appuis)
- `bool IsCellProtected(int cell)` — Carré d'une zone protégée (B2 + B4) — capture (F3), ennemi
- `int GetCellLand(int cell)` — Numéro de terre du carré : 0 grande île, 1 et suivants les îles gardées, -1 mer — capture, calque
- `bool IsRedAt(vector position)` — Carré rouge sous la position — missions (G10, G13), ennemi (G12, F10), civils (G14), Commandeur
- `bool IsBlueAt(vector position)` — Carré bleu sous la position — missions (G10), civils (G14), livraisons
- `int GetOwnerAt(vector position)` — Propriétaire sous la position, -1 en mer — missions, ennemi
- `int GetZoneAt(vector position)` — Zone sous la position, -1 en mer — missions, Commandeur, Staff
- `bool IsInBaseZone(vector position)` — La position est-elle dans la zone de la base (B2) ? — missions (m_bCaptureIgnoreBase), Commandeur
- `string RedDirection(vector center, out float angle)` — Direction moyenne (plan XZ) des carrés rouges entre 500 et 3000 m ; angle en degrés dans angle ; rend le nom de la zone rouge la plus proche, ou "" s'il n'y a pas de rouge — SRP_HeliSearch (remplace EnemySectorDirection)
- `int GetZoneCount()` — Nombre d'entrées de zone, base comprise (index 0) ; toujours sous 127 — tout le mod
- `int CountZonesTotal()` — Nombre de zones jouables (sans la base), la référence des textes « 12 zones sur 66 » — PC, Staff, pont
- `int CountZonesOurs()` — Zones jouables à nous (sans la base) — PC, Staff, pont (territory.ours)
- `SRP_FrontZone GetZone(int zone)` — La zone d'index donné, null hors bornes (lecture de ses champs publics) — tout le mod
- `int FindZone(string text)` — Zone par code (« S07 »), nom (« Régina »), libellé (« Régina (S07) ») ou localité unique, sans tenir compte de la casse ni des espaces en double ; -1 sinon — missions (CreateCapture), pont (/ordonner), Staff
- `int GetZoneOwner(int zone)` — SRP_EFrontOwner de la zone (la base est BLEU) — tout le mod
- `string GetZoneCode(int zone)` — Code stable de la zone, « S07 » (clé technique, sans « : » ni « | ») — pont, missions, Staff
- `string GetZoneName(int zone)` — Nom seul de la zone, « Régina » (localité ou lieu-dit ; « Secteur » à défaut) — carte, pont
- `string GetZoneLabel(int zone)` — Libellé affiché partout, « Régina (S07) » (A6) — radio, PC, Staff, missions, Commandeur
- `vector GetZoneCentroid(int zone)` — Centre de gravité des carrés de terre de la zone (y au sol) — Commandeur, ennemi
- `vector GetZoneLabelPos(int zone)` — Où écrire le nom de la zone sur la carte (centre de ses carrés de terre, pas le point clé) — calque
- `vector GetMissionPoint(int zone)` — Point de mission de la zone : point clé CENTRE de la localité principale, sinon le carré de terre le plus proche du centre de gravité — missions (CreateCapture, rayon 3 km), Staff (téléportation)
- `int GetZoneCells(int zone, notnull array<int> cells)` — Remplit cells avec les carrés de terre de la zone, rend leur nombre — chute, économie, ennemi
- `int CountZoneCells(int zone)` — Nombre de carrés de terre de la zone — chute (D1, Q2), PC, Staff
- `int CountZoneBlue(int zone)` — Nombre de carrés bleus de la zone (tenu à jour à chaque lot) — chute, PC, Staff, pont
- `bool IsZoneInContact(int zone)` — E1 : zone ROUGE dont un carré touche du bleu (par un côté ou la mer) — missions (J4), ennemi (F11), carte (I3)
- `bool IsZoneAtFront(int zone)` — F1 : zone BLEUE dont un carré touche du rouge — ennemi (F1, cibles de contre-attaque), carte (I3)
- `bool IsZoneProtected(int zone)` — B2 + B4 : la base, ou une zone qui touche la base (m_bShield) — chute (Q2), ennemi (F1, H2), pont (p)
- `bool IsBaseZone(int zone)` — La zone de la base (index 0, B2) — tout le mod
- `bool IsZoneCut(int zone)` — E4, SEUL prédicat public de « zone coupée » : zone bleue non protégée, coupée de la base depuis m_iCutSince — économie (plus de production), PC, pont (k)
- `int GetZoneCutSecondsLeft(int zone)` — E4 : secondes avant la perte d'une zone coupée, -1 si elle est reliée (gel : décompte arrêté) — PC, Staff, pont (kr)
- `int GetZoneFlags(int zone)` — Drapeaux d'affichage de la zone (FLAG_ATTACK_ANNOUNCED, FLAG_ATTACK, FLAG_CUT) — carte, PC, pont
- `int GetZoneAttackState(int zone)` — SRP_EFrontAttack de la zone, déduit des drapeaux (posés par SRP_FrontEnemyComponent via SetZoneFlag) : SEULE source de l'état d'attaque d'une zone — carte, repères, PC, Staff, pont (a), ennemi, Commandeur
- `int GetZoneCapturedAt(int zone)` — Heure Unix de la dernière prise de la zone (F2 : dernière zone prise) — ennemi (PickTargetZone)
- `string GetZoneLastChange(int zone)` — Dernier changement de camp, en clair (« 25/09 21:14 ») — Staff (page de zone)
- `int GetZonesInContact(notnull array<int> zones)` — Zones rouges au contact (E1), sans la base — missions (J4 : GetCaptureTargets), Commandeur ; rend le nombre
- `int GetBlueFrontZones(notnull array<int> zones)` — Zones bleues au front (F1), non protégées (B4) — ennemi (PickTargetZone, H2) ; rend le nombre
- `int GetZoneLocalities(int zone, notnull array<int> localities)` — Index des localités de la zone (principale d'abord), rend le nombre — chute, ennemi, Staff
- `int GetKeyPointCount(int zone)` — Nombre de points clés de la zone (0 sans localité, 1 par hameau ou village, 2 par ville : D6) — chute, ennemi, carte
- `vector GetKeyPointPos(int zone, int k)` — Position du k-ième point clé de la zone — chute (poste), ennemi (garnison, F5), Commandeur (QG = m_vHQ)
- `int GetKeyPointRole(int zone, int k)` — SRP_EKeyPointRole du k-ième point clé de la zone — chute, ennemi, repères
- `int GetKeyPointCell(int zone, int k)` — Carré du k-ième point clé de la zone — capture (F5), chute (D2), ennemi
- `int GetKeyPointIndex(int zone, int k)` — Index du k-ième point clé de la zone dans le registre global (GetKeyPoint) — chute (postes)
- `bool IsKeyPointTaken(int zone, int k)` — Le point clé est pris : son carré est bleu — chute (D1, Q2), repères, PC
- `int GetKeyPointTotal()` — Nombre total de points clés du registre — chute (postes), Staff
- `SRP_FrontKeyPoint GetKeyPoint(int index)` — Le point clé d'index global donné, null hors bornes — chute, Staff, repères
- `int GetKeyPointReport(notnull array<string> lines)` — Page Staff « Points clés » (Q8) : une ligne par localité, source de chaque point (REPERE ; FICHIER = dépannage, « à poser au Workbench » ; NOM_CARTE = centre MANQUANT ; QG absent d'une ville = MANQUANT) ; rend le nombre de points manquants — SRP_FrontScreens
- `int GetLocalityCount()` — Nombre de localités de zone (sans celles de la base) — ennemi, Commandeur (régions), SRP_Territory (BuildLocalities)
- `SRP_FrontLocality GetLocality(int index)` — La localité d'index donné, null hors bornes (champs publics : pertes F12, envoyés, bonus…) — ennemi, Commandeur
- `int FindLocality(string name)` — Index de la localité par son nom exact (nom traduit de la carte), -1 sinon — ennemi, territoire, chute
- `bool IsFrozen()` — J3 : front gelé, ni capture, ni perte, ni contre-attaque — tout le mod
- `bool IsVictory()` — B3 : toutes les zones jouables sont bleues — Staff, PC
- `int GetThreat()` — Menace de l'île, 0 à 10 (G9) ; SRP_TerritoryComponent.GetThreat ne fait plus que relayer — serveur, tout le mod
- `void AddThreat(int delta, string reason)` — Serveur : menace + delta, bornée de 0 à 10, sauvegarde immédiate, puis SRP_FrontEnemyComponent.OnThreatChanged (ressources du Commandeur) — appelé par SRP_ZoneFall (G9), l'ennemi (F4, C8), SRP_TerritoryComponent (relais)
- `int GetStateVersion()` — Version d'état : +1 à chaque lot appliqué, y compris sur l'hôte (le Broadcast ne s'y exécute pas) — calque, Commandeur
- `int GetGeometryVersion()` — Version de géométrie, +1 à chaque géométrie appliquée — calque
- `int GetLiveVersion()` — Version des états contestés — calque (orange)
- `int GetBridgeVersion()` — Serveur : version pour le pont, +1 par carré qui change de camp, sauvegardée, ne redescend jamais (trou 19) — SRP_Bridge (v, NoteFrontCell)
- `int GetRestoreCount()` — Serveur : nombre de retours à une copie datée (J2), sauvegardé — SRP_Bridge (rs)
- `int GetCampaign()` — Numéro de campagne (première campagne du front = 2, trou 18) — Staff, Commandeur
- `string GetCampaignId()` — Identifiant de campagne pour le pont, « C2 » (^[A-Za-z0-9_-]+$, stable jusqu'à la suivante) — SRP_Bridge
- `int CountLandCells()` — Carrés de terre en jeu — PC, Staff, pont (land)
- `int CountBlueCells()` — Carrés bleus (base comprise) — PC, Staff, pont (blue)
- `int GetContestedCells(notnull array<int> cells)` — Carrés contestés (C8), rend le nombre — SRP_Bridge (cs)
- `string GetCellsString()` — N x N caractères, « . » hors jeu, « R », « B », index gz x N + gx — SRP_Bridge (g), sauvegarde (cells)
- `string GetZoneMapString()` — Deux caractères hexadécimaux de zone par carré, « FF » hors jeu — SRP_Bridge (zm, en cache côté pont)
- `string GetGeometryReport()` — Rapport de géométrie (zones, carrés, localités, points clés manquants, retouches refusées) — Staff
- `void SetExtra(string key, string value)` — Serveur : range une valeur (clé sans « | »), puis MarkDirty(false) — missions, économie
- `string GetExtra(string key)` — Serveur : la valeur rangée, "" si absente — missions, économie
- `bool SetCellOwner(int cell, int owner, int reason, string author)` — Un carré change de camp : refus (false + journal) hors jeu, base, déjà en place, gel (sauf STAFF, REMISE, RESTAURATION), passage au rouge d'une zone protégée bleue (sauf STAFF) ; sinon comme SetCellsOwner avec un seul carré — capture (via SetCellsOwner), chute (SAISIE)
- `int SetCellsOwner(notnull array<int> cells, int owner, int reason, string author)` — Un lot de carrés passe à owner, mêmes refus carré par carré ; rend le nombre appliqué. Puis, dans l'ordre et en appels directs (trou 6) : m_Capture.OnOwnersChanged -> m_ZoneFall.OnCellsChanged -> m_Economy.OnCellsChanged -> SRP_BridgeComponent.NoteFrontCell (par carré) -> SRP_FrontRadio.QueueCell (par carré) ; m_iBridgeVersion++ par carré, compteurs de zone, topologie à refaire, QueueFlush, MarkDirty(false) — capture, chute, Staff
- `bool SetZoneOwner(int zone, int owner, int reason)` — RÉSERVÉ À SRP_ZoneFall : note le propriétaire de la zone, m_iCapturedAt/m_iLostAt, numéro de prise (F2), m_iCutSince = 0, drapeaux d'attaque effacés, m_bStatePending, sauvegarde immédiate, CheckVictory. Ne repeint AUCUN carré (la chute appelle SetCellsOwner elle-même) — SRP_ZoneFall.CaptureZone/LoseZone/ForceZone
- `void SetZoneFlag(int zone, int flag, bool on)` — Drapeau d'affichage d'une zone (FLAG_ATTACK_ANNOUNCED, FLAG_ATTACK) posé ou retiré, m_bStatePending (trou 11) — SRP_FrontEnemyComponent (contre-attaques)
- `void SetLiveCells(notnull array<int> cells, notnull array<int> states)` — C8 : différences d'état contesté du passage (carrés, SRP_EFrontLive) ; envoi au plus toutes les m_iLiveSendMinMs, le dernier état part toujours — SRP_FrontCapture.PushLive
- `void ClearLive()` — Tous les états contestés à AUCUN et envoi (gel, restauration, remise) — socle, capture
- `void MarkDirty(bool immediate)` — Serveur : front.json à écrire ; immediate = au prochain passage, sinon au plus m_iSaveDelaySeconds plus tard — ennemi (F12), Commandeur, économie, missions
- `bool Save()` — Serveur : écrit front.json (refusé si !m_bLoaded), puis la copie datée du jour si le jour a changé ; rend vrai si le fichier est écrit. Garde de la chaîne unique : si m_bEnemyChained et SRP_FrontEnemyComponent.GetInstance() est null, RIEN n'est écrit (un front.json sans fe_* ni cmd_* ferait tout repartir à neuf : stocks pleins, officiers neufs, interdictions effacées) ; journal ERREUR sauf si m_bEnemyFinalSaved (sauvegarde finale déjà faite par l'ennemi) — Housekeeping, OnDelete, Staff, SaveFinalFromEnemy
- `bool SaveFinalFromEnemy()` — Serveur, OnDelete de SRP_FrontEnemyComponent (AVANT son m_Commander.Stop et son s_Instance = null) : Save() tant que la chaîne complète existe, puis m_bEnemyFinalSaved = vrai ; rend vrai si le fichier est écrit — SRP_FrontEnemyComponent.OnDelete
- `string ForceCellAt(vector position, int owner, string author)` — J1 : le carré où se tient le Staff passe à owner (cause STAFF : ni prime, ni menace, ni radio ; correction Q9 : la zone n'est PAS réévaluée, SRP_ZoneFall.OnCellsChanged ignore STAFF) — SRP_FrontScreens
- `string ForceCell(int cell, int owner, string author)` — J1 : un carré passe à owner (cause STAFF), refusé dans la base ; ni chute ni perte de zone ne suivent (Q9 : correction ; pour une vraie chute, ForceSeize ; pour corriger toute la zone, ForceZone) — SRP_FrontScreens, ForceCellAt
- `string ForceZone(int zone, int owner, string author)` — J1 : relais de SRP_ZoneFall.ForceZone (zone entière repeinte, Q9 : ni prime, ni menace, ni radio) — SRP_FrontScreens
- `string ResetZone(int zone, string author)` — J2 : la zone revient à son état de départ (rouge, stock et dépôt effacés), relais REMISE à la capture, à l'économie et à l'ennemi (OnFrontReset ZONE) — SRP_FrontScreens
- `string NewCampaign(string author)` — B3, J2 : nouvelle campagne à la demande du Staff (archive, tout rouge sauf la base, menace 0) — SRP_FrontScreens, SRP_Admin (danger, wipe)
- `string SetFrozen(bool frozen, string author)` — J3 : gel ou dégel ; au dégel chaque m_iCutSince est décalé de la durée du gel ; ClearLive ; capture prévenue ; radio SRP_FrontRadio.Frozen — SRP_FrontScreens
- `int GetDailyCopies(notnull array<string> days, notnull array<string> summaries)` — J2 : copies datées, de la plus récente à la plus ancienne (au plus m_iSavesShown), avec un résumé « 14 zones, 231 carrés à nous, menace 4 » ; rend le nombre — SRP_FrontScreens
- `string RestoreDaily(string day, string author)` — J2 : revient à la copie du jour donné (AAAA-MM-JJ) : lecture dans des tableaux neufs, application d'un coup, m_iRestoreCount++, ClearLive, OnFrontReset(RESTAURATION) aux modules, FrontEnemy.ReadFrom puis OnRestored, SRP_BridgeComponent.NoteFrontReset, radio Restored, sauvegarde — SRP_FrontScreens
- `override bool RplSave(ScriptBitWriter writer)` — Serveur, arrivant : WriteBool(m_bReady) puis, si prêt, les tableaux de géométrie et d'état (5 à 6 Ko, WriteIntRange 0..268435455, WriteString) — le moteur (EnNetwork.c:126-196)
- `override bool RplLoad(ScriptBitReader reader)` — Joueur arrivant : lecture miroir avec gardes (N <= 256, zones <= 126, points <= 256, sinon false), puis ApplyGeometry et ApplyState — le moteur (EnNetwork.c:198-240)

### SRP_FrontGeometry.c (368 lignes)


**class SRP_FrontZone**
  Une zone du front. Champs publics en lecture pour tout le mod ; seul le socle (et SRP_ZoneFall par le socle) les écrit. Les accesseurs Get*/Is* ne font que lire les champs (contrat du pont, mod_web).
  - champ `int m_iIndex` — 0 = la base, puis 1..Z (numéro du code)
  - champ `string m_sCode` — « S07 », clé stable
  - champ `string m_sName` — « Régina », localité ou lieu-dit, « Secteur » à défaut
  - champ `int m_iKind = SRP_EZoneKind.AUCUNE` — genre de la plus grosse localité (SRP_EZoneKind)
  - champ `int m_iLocality = -1` — localité principale (index socle), -1 sans localité
  - champ `ref array<int> m_aLocalities = {}` — toutes ses localités, principale d'abord (serveur)
  - champ `ref array<int> m_aCells = {}` — carrés de terre
  - champ `ref array<int> m_aNeighbours = {}` — zones voisines par un côté ou par la liaison maritime (Q5)
  - champ `ref array<int> m_aKeyPoints = {}` — index des points clés dans le registre du socle
  - champ `vector m_vCentroid` — centre de gravité des carrés (y au sol)
  - champ `vector m_vAnchor` — centre du bloc ou centre de gravité (numérotation A6)
  - champ `vector m_vLabelPos` — où écrire le nom sur la carte
  - champ `bool m_bBase` — B2
  - champ `bool m_bShield` — B4 : touche la base par un côté
  - champ `bool m_bIsland` — île secondaire (Erquy)
  - champ `int m_iOwner = SRP_EFrontOwner.ROUGE`
  - champ `int m_iFlags` — FLAG_ATTACK_ANNOUNCED | FLAG_ATTACK | FLAG_CUT
  - champ `int m_iBlueCells` — carrés bleus, tenu à jour par le socle
  - champ `bool m_bInContact` — E1 (zone rouge au contact), tenu par UpdateTopology
  - champ `bool m_bAtFront` — F1 (zone bleue au front), tenu par UpdateTopology
  - champ `bool m_bLinked = true` — E4 : reliée à la base
  - champ `int m_iCapturedAt`
  - champ `int m_iLostAt`
  - champ `int m_iTakenCount` — numéro de prise, F2
  - champ `int m_iCutSince` — E4 : début de la coupure, 0 = reliée
  - champ `string m_sLastChange` — « 25/09 21:14 » pour le Staff
  - champ `int m_iResource = -1` — SRP_EResource produite, -1 = rien (G4, G5)
  - champ `int m_iRate = -1` — paquets par heure, -1 = débit par défaut de la ressource
  - champ `int m_iStock` — paquets en attente (sauvegardé)
  - champ `bool m_bDepot` — dépôt installé (sauvegardé)
  - champ `vector m_vDepot` — position du dépôt (sauvegardée)
  - champ `int m_iDepotCell = -1` — carré du dépôt, recalculé
  - champ `int m_iProductionSec` — secondes de production accumulées (non sauvegardé)
  - champ `IEntity m_Box` — caisse de production (vivante)
  - champ `ref SCR_MapMarkerBase m_DepotMarker` — repère du dépôt (vivant)
- `string GetCode()` — Code stable « S07 » — pont (mod_web)
- `string GetName()` — Nom seul « Régina » — pont
- `string GetLabel()` — Libellé « Régina (S07) » (A6), la base garde son nom seul — pont, radio, écrans
- `string GetKind()` — Genre en clair : « hameau », « village », « ville » ou « lieu-dit » — pont (genre)
- `bool IsOurs()` — Zone à nous (BLEU) — pont (o)
- `int GetAttackState()` — SRP_EFrontAttack d'après m_iFlags — pont (a)
- `bool IsNearFront()` — PRÈS du front au sens large (E1 zone rouge au contact OU F1 zone bleue au front), le filtre I3 — pont (clé f), PC. Ne PAS confondre avec SRP_FrontComponent.IsZoneAtFront (F1 seul, zone bleue qui touche du rouge)
- `bool IsCut()` — Coupée de la base (E4) — pont (k)
- `bool IsProtected()` — Imprenable par l'ennemi (B2, B4) — pont (p)
- `bool HasDepot()` — Dépôt installé (#49) — pont (dp)
- `vector GetDepotPosition()` — Position du dépôt — pont (dp)
- `int GetStock()` — Paquets en attente — pont (st), boxes
- `int GetResource()` — SRP_EResource produite, -1 = rien — pont (boxes)

**class SRP_FrontLocality**
  Une localité de zone (serveur). Les champs « sauvegardés » sont écrits dans front.json par le socle (clés lN_*).
  - champ `string m_sName` — nom traduit de la carte (clé)
  - champ `string m_sKind` — « hameau », « village » ou « ville » (un bourg compte comme une ville)
  - champ `int m_iKind = SRP_EZoneKind.VILLAGE` — même chose en SRP_EZoneKind
  - champ `vector m_vLabel` — centre de la zone civile (SRP_CivilZoneComponent.GetCenter)
  - champ `bool m_bHome` — localité de la base (ignorée par le front)
  - champ `int m_iZone = -1` — zone de son CENTRE
  - champ `int m_iCentre = -1` — point clé CENTRE (index du registre)
  - champ `int m_iQg = -1` — point clé QG (index du registre), -1 hors ville
  - champ `int m_iLosses`
  - champ `int m_iLossesAt`
  - champ `int m_iSent` — soldats envoyés en renfort (MA2)
  - champ `int m_iSentAt`
  - champ `int m_iBonus` — renforts reçus
  - champ `int m_iBonusAt`
  - champ `int m_iEmptiedAt` — localité vidée par un décrochage (MA7)
  - champ `int m_iFullUntil` — tenue au complet jusqu'à (MA11)

**class SRP_FrontKeyPoint**
  Un point clé du registre UNIQUE (trou 13). Pas de m_bTaken : un point est pris quand son carré est bleu.
  - champ `int m_iIndex` — place dans le registre
  - champ `int m_iLocality = -1` — localité (index socle)
  - champ `int m_iRole = SRP_EKeyPointRole.CENTRE`
  - champ `vector m_vPos` — position au sol
  - champ `float m_fYaw` — orientation du repère (poste de commandement)
  - champ `int m_iCell = -1`
  - champ `int m_iZone = -1`
  - champ `int m_iSource = SRP_EKeyPointSource.NOM_CARTE`
  - champ `string m_sSourceText` — « repère SRP_PointCle_12 », « front_retouches.txt ligne 8 », « nom de la carte »

**class SRP_FrontPack**
  Empaquetage des petits entiers pour le réseau : 28 bits utiles par entier, jamais le bit de signe ; écriture word = word | ((v & mask) << shift) (jamais |=) — appelé par le socle (réseau, RplSave)
- `static void Pack(notnull array<int> values, int bits, notnull array<int> packed)` — values (0 à 2^bits - 1) vers packed, 28 / bits valeurs par entier — BuildGeometryArrays, BuildStateArrays
- `static void Unpack(notnull array<int> packed, int bits, int count, notnull array<int> values)` — packed vers count valeurs de bits bits — ApplyGeometry, ApplyState

**class SRP_FrontGeometryBuilder**
  Construction de la géométrie (serveur seulement), déterministe : mêmes entrées, mêmes zones, mêmes codes. Le socle crée le bâtisseur, appelle Build, puis adopte ses tableaux publics de sortie.
  - champ `int m_iN` — côté de la grille (64)
  - champ `int m_iHash` — somme de contrôle (h x 131 + zone + 2) modulo 8388593
  - champ `ref array<int> m_aCellZone = {}` — -1 hors jeu
  - champ `ref array<int> m_aCellLand = {}` — numéro de terre, -1 mer
  - champ `ref array<int> m_aSeaLink = {}` — -1 sinon
  - champ `ref array<ref SRP_FrontZone> m_aZones = {}`
  - champ `ref array<ref SRP_FrontLocality> m_aLocalities = {}`
  - champ `ref array<ref SRP_FrontKeyPoint> m_aKeyPoints = {}`
  - champ `ref array<string> m_aReport = {}` — lignes du rapport front_zones.txt
  - champ `ref array<string> m_aWarnings = {}` — avertissements (aussi au journal TERRITOIRE)
  - champ `int m_iMissingKeyPoints` — points clés sans repère ni ligne : centre sur le nom de la carte, QG absent (Q8)
- `void SRP_FrontGeometryBuilder(SRP_FrontComponent front)` — Le bâtisseur lit les réglages du socle — SRP_FrontComponent.Start
- `bool Build()` — Toutes les étapes dans l'ordre (1 à 16 de la consigne) ; faux sur erreur fatale (carte absente, aucune terre) — SRP_FrontComponent.Start
- `void WriteReport()` — 16. Rapport REPORT_PATH réécrit : une ligne par zone, points clés et sources, avertissements, retouches refusées, villages coupés (m_fVillageCheckRadius) — SRP_FrontComponent.Start après Build

### SRP_FrontKeyPoint.c (68 lignes)


**class SRP_KeyPointDef**
  Définition brute d'un repère posé (serveur), lue par le bâtisseur de géométrie
  - champ `string m_sLocality` — nom exact de la localité, vide = la plus proche à 600 m au plus
  - champ `int m_iRole = SRP_EKeyPointRole.CENTRE`
  - champ `vector m_vPos` — origine du repère
  - champ `float m_fYaw` — orientation du repère (le poste de commandement la reprend)
  - champ `string m_sSource` — « repère <nom de l'entité> »

**class SRP_KeyPointComponentClass : ScriptComponentClass**

**class SRP_KeyPointComponent : ScriptComponent**
  - champ `string m_sLocality`
  - champ `SRP_EKeyPointRole m_eRole`
- `static array<ref SRP_KeyPointDef> GetDefs()` — Les repères posés dans le monde (serveur), dans l'ordre d'initialisation — SRP_FrontGeometryBuilder.PlaceKeyPoints
- `override void OnPostInit(IEntity owner)` — super ; rien au World Editor (SCR_Global.IsEditMode(owner)) ; serveur : un SRP_KeyPointDef (m_sLocality.Trim(), m_eRole, origine, GetYawPitchRoll()[0], nom de l'entité) dans s_aDefs ; toutes machines : CallLater(RemoveMarker, 0) — le moteur

### SRP_FrontCapture.c (304 lignes)


**class SRP_FrontCapture**
- `void SRP_FrontCapture(SRP_FrontComponent front)` — Créée par le socle dans OnPostInit (serveur) — SRP_FrontComponent
- `void Start()` — Front prêt : Resize de tous les tableaux par carré à GetCellCount(), états contestés à AUCUN — SRP_FrontComponent.Start
- `void Tick(int nowUnix)` — Le passage de 5 s (1er de la boucle unique) : rien si gelé (ClearLive une fois) ; CollectPlayers, CollectEnemies, EvaluateCells, ResetTouched, ApplyFlips (captures puis reprises), PushLive ; diagnostic si m_bTimingDiag — SRP_FrontComponent.Tick
- `void OnOwnersChanged(notnull array<int> cells)` — Des carrés ont changé de camp (toute cause) : leurs deux compteurs à 0, retrait de m_aActive — socle, 1er appel direct après chaque lot (trou 6)
- `void OnFrozenChanged(bool frozen)` — J3 : gel -> compteurs vidés et états contestés à AUCUN (le socle envoie la différence) — SRP_FrontComponent.SetFrozen
- `void OnFrontReset(int kind, int zone)` — Remise à plat (SRP_EFrontReset) : tous les compteurs à 0 (ZONE : seulement les carrés de la zone), papier effacé — SRP_FrontComponent.NotifyReset
- `static void NoteTeleport(int playerId)` — C3 : un joueur téléporté par le menu Staff ne compte pas pendant m_iExcludeMinutes (tout joueur si m_bExcludeAllTeleported, sinon le Staff seul) ; sans effet si le front est absent — SRP_AdminComponent.Teleport
- `void OnPlayerDisconnected(int playerId)` — Un joueur part : on oublie sa dernière position et son exclusion — SRP_FrontComponent.OnPlayerDisconnected
- `int CountValidPlayersNear(vector position, float radius)` — Joueurs valides (C2, C3) à moins de radius (XZ) — chute (D4, F5), ennemi (F4, postes), Commandeur (EQ4 : GetDosingPlayers, seul nombre réel qu'il lit)
- `int CountValidPlayersInCell(int cell)` — Joueurs valides dans le carré — ennemi, PC
- `bool IsPlayerCounted(int playerId)` — Ce joueur compte-t-il (recensé valide à ce passage) ? — chute (TrySeize, C3), Commandeur
- `bool IsCharacterCounted(IEntity character)` — Ce personnage est-il un joueur qui compte (C2, C3) ? — Commandeur (informateurs, CO5)
- `int CountFitEnemiesNear(vector position, float radius)` — Ennemis au sol en état de combattre à moins de radius (C6) — chute (D5 : poste, saisie), ennemi
- `int CountFitEnemiesInCell(int cell)` — Ennemis en état de combattre dans le carré (C6) — ennemi, Staff
- `int CountFitEnemiesInZone(int zone)` — Ennemis en état de combattre dans la zone (I3 : n'est affiché que pour les zones au front) — PC, Staff
- `int CountFitAssaultersInZone(int zone)` — Soldats de l'attaque (m_iAssaultZone == zone) en état de combattre dans la zone, plus ceux sur le papier (CA6) — ennemi (F4 : zone nettoyée), SRP_CmdManeuvers
- `bool IsKeyCellGuarded(int cell)` — F5 : un joueur valide à moins de m_fKeyPointGuardRadius d'un point clé de ce carré — ennemi, chute
- `int GetLiveState(int cell)` — SRP_EFrontLive du carré au dernier passage — PC, Staff, Commandeur
- `int GetCaptureSeconds(int cell)` — Secondes de capture accumulées sur le carré (jamais affichées sur la carte, trou 20) — PC, Staff
- `int GetCaptureTotalSeconds()` — Secondes nécessaires pour prendre un carré (m_iSquareCaptureMinutes x 60) — PC, Staff
- `int GetRetakeSeconds(int cell)` — Secondes de reprise accumulées sur le carré — Staff
- `int GetRetakeTotalSeconds()` — Secondes nécessaires pour reprendre un carré (m_iRetakeSeconds) — Staff
- `void SetPaperAssault(int cell, int soldiers, int zone, int unixTime)` — CA6 : des assaillants sur le papier occupent ce carré pour la zone visée (entrée valable m_iPaperFreshSeconds) — SRP_CmdManeuvers (PaperTick)
- `void ClearPaperAssault(int zone)` — CA6 : efface les assaillants sur le papier de la zone (fin d'attaque) — SRP_CmdManeuvers, ennemi (EndAttack)

### SRP_ZoneFall.c (428 lignes)


**class SRP_KeyPointPost**
  État vivant d'un point clé côté chute (serveur) : poste, mât, saisie. Rien n'est sauvegardé (H3) : la saisie est la couleur du carré, sauvegardée par le socle.
  - champ `int m_iKeyPoint = -1` — index dans le registre du socle
  - champ `string m_sLocality` — nom de la localité
  - champ `IEntity m_CP` — poste de commandement posé
  - champ `int m_iNextCPTry` — heure Unix du prochain essai de pose
  - champ `bool m_bCPLogged` — échec de pose déjà écrit pour cet épisode
  - champ `IEntity m_Flag` — mât de nos couleurs
  - champ `int m_iNextFlagTry` — heure Unix
  - champ `bool m_bFlagNow` — hisser tout de suite, même vu (juste après la saisie)
  - champ `int m_iSeizerId` — joueur qui saisit, 0 = personne
  - champ `int m_iSeizeStartMs` — début de la saisie (GetTickCount, non sauvé)
  - champ `int m_iHitLockUntilMs` — saisie refusée jusqu'à (GetTickCount, non sauvé)
  - champ `int m_iBlock = SRP_ESeizeBlock.LIBRE`

**class SRP_ZoneFall**
- `void SRP_ZoneFall(SRP_FrontComponent front)` — Créée par le socle dans OnPostInit (serveur) — SRP_FrontComponent
- `void Start()` — Front prêt : un SRP_KeyPointPost par point clé du registre, RespawnFlags ; journal TERRITOIRE « N points clés sur M localités, K posés au Workbench » — SRP_FrontComponent.Start
- `void Tick(int nowUnix)` — Passage de 5 s (2e de la boucle unique) : UpdatePost de chaque point, puis EvaluateZone des seules zones de m_aZonesToCheck (jamais toute l'île) — SRP_FrontComponent.Tick
- `void OnCellsChanged(notnull array<int> cells, int owner, int reason)` — Un lot de carrés a changé (2e appel direct après chaque lot, trou 6) : leurs zones vont dans m_aZonesToCheck, sauf pendant une repeinte et sauf pour la cause STAFF (ForceCell = correction Q9 : ni chute ni perte qui suivent ; la zone sera réévaluée au prochain changement réel) — SRP_FrontComponent.SetCellsOwner
- `void OnFrontReset(int kind, int zone)` — Remise à plat : saisies arrêtées, postes et mâts recalculés (ZONE : seulement ceux de la zone) — SRP_FrontComponent.NotifyReset
- `bool CaptureZone(int zone)` — D1 / Q4 : la zone tombe. Rien si elle est déjà bleue, de base, ou front gelé. m_Front.SetZoneOwner(BLEU, ZONE_TOMBEE) ; repeinte en silence des carrés rouges (SetCellsOwner, ZONE_TOMBEE) ; localités pas encore prises -> ennemi.OnLocalityTaken ; menace + m_iThreatPerZoneTaken (G9) ; puis missions.OnZoneCaptured(code, false) -> économie.OnZoneCaptured -> ennemi.OnZoneCaptured(zone, false) -> SRP_FrontRadio.ZoneFell — EvaluateZone
- `bool LoseZone(int zone, int reason, bool repaint)` — Q2, E4, H2 : la zone repasse rouge. Rien si déjà rouge, protégée (B2, B4) ou gel. m_Front.SetZoneOwner(ROUGE, reason) ; si repaint, ses carrés bleus passent rouges en silence (ISOLEMENT, OFFENSIVE) ; menace - m_iThreatPerZoneLost (G9) ; économie.OnZoneLost -> ennemi.OnZoneLost(zone, reason, false) -> radio ZoneFell — EvaluateZone (ZONE_PERDUE, sans repeinte), socle CheckCuts (ISOLEMENT), ennemi RunOffensive (OFFENSIVE)
- `string ForceZone(int zone, int owner, string author)` — J1 / Q9 : correction du Staff. Zone entière repeinte à owner (STAFF), ni prime, ni menace, ni radio ; missions.OnZoneCaptured(code, true) si elle passe bleue (mission de la zone ANNULEE, sans récompense) ; économie ; ennemi.OnZoneCaptured(zone, true) ou OnZoneLost(zone, STAFF, true) (le Commandeur ne réagit pas, Q9) ; AUCUNE ligne de journal ici : la seule ligne [STAFF] est écrite par SRP_FrontScreens.RunStaff ; rend le compte rendu — SRP_FrontComponent.ForceZone
- `bool IsCellSeizeOnly(int cell)` — Carré d'un point clé d'une localité encore hostile : il ne se prend QUE par la saisie du poste (D4), jamais par présence — SRP_FrontCapture.EvaluateCells (trou 12)
- `bool IsLocalityTaken(string locality)` — Tous les points clés de la localité sont bleus (localité prise) — SRP_TerritoryComponent (aucune garnison), ennemi, missions
- `string GetZoneFallText(int zone)` — Texte de chute de la zone : « points clés 1/2 saisis · 11 carrés bleus sur 25, 13 requis » — PC, Staff, missions
- `int GetSeizeBlock(int keyPoint)` — Raison qui bloque la saisie du point clé (SRP_ESeizeBlock) — Staff (page des points clés)
- `int GetPostReport(notnull array<string> lines)` — État de chaque poste pour la page Staff « Points clés » (en place, pas de garnison, saisi, bloqué…) ; rend le nombre de lignes — SRP_FrontScreens
- `void OnGarrisonPosted(string locality)` — La garnison de la localité est posée : réveille la pose de son poste — SRP_TerritoryComponent (pose, même vide F12)
- `void OnGarrisonGone(string locality)` — La garnison de la localité est partie : le poste sera retiré hors de vue — SRP_TerritoryComponent
- `void OnSeizeStart(IEntity cp, IEntity user)` — Début de la barre d'action : si personne ne saisit et LIBRE, m_iSeizerId = joueur, s_iActiveSeizures++, SetState, CallLater(SeizeWatch, m_iSeizeCheckMs, true) à la première saisie — SRP_SaisiePCAction.OnActionStart
- `void OnSeizeCancel(IEntity cp, IEntity user)` — Action annulée : si c'est le saisisseur, StopSeize(LIBRE) — SRP_SaisiePCAction.OnActionCanceled
- `string TrySeize(IEntity cp, IEntity user)` — Fin de la barre : tout revérifié au serveur (saisisseur, durée x m_fSeizeMinRatio, zone rouge, carré rouge au contact, non gelé, garnison posée et installée, aucun ennemi à m_fKeyPointClearRadius, pas touché, capture.IsPlayerCounted) puis Seize ; rend "" ou la raison du refus — SRP_SaisiePCAction.PerformAction
- `static void NoteHit(IEntity character, EDamageType type, float value)` — Coût nul hors saisie (s_iActiveSeizures == 0) ; dégât MELEE, KINETIC, FRAGMENTATION, EXPLOSIVE, INCENDIARY ou PROCESSED_FRAGMENTATION sur le saisisseur -> verrou m_iHitLockSeconds, StopSeize(TOUCHE), message — dégâts moddés
- `string ForceSeize(string locality, int role, string author)` — Saisie forcée du point clé (rôle SRP_EKeyPointRole) de la localité, comme une vraie (Q9 : la chute de zone qui suit reste une vraie chute) — SRP_FrontScreens
- `string RespawnCP(string locality)` — Retire et repose le poste de la localité (hors de vue) — SRP_FrontScreens

**class SRP_PosteCommandementComponentClass : ScriptComponentClass**
  Composant du prefab Prefabs/Props/SRP_PosteCommandement.et (RplComponent obligatoire) : état de saisie répliqué pour que l'action affiche la bonne raison chez le joueur (modèle SCR_CacheNoteComponent.c:4-25)

**class SRP_PosteCommandementComponent : ScriptComponent**
- `void SetState(int block, int seizer)` — Serveur : affecte, puis Replication.BumpMe() seulement si quelque chose change — SRP_ZoneFall
- `void SetLabel(string label)` — Serveur : nom affiché du poste, puis BumpMe — SRP_ZoneFall.UpdateCP
- `int GetBlock()` — SRP_ESeizeBlock répliqué — SRP_SaisiePCAction.CanBePerformedScript
- `int GetSeizer()` — Joueur qui saisit, 0 = personne — SRP_SaisiePCAction.CanBePerformedScript
- `string GetLabel()` — Nom affiché du poste — SRP_SaisiePCAction

**class SRP_SaisiePCAction : ScriptedUserAction**
  Action « Saisir le poste de commandement » du prefab (Duration 60 réglée dans le .et). Modèle SRP_DepotInstallAction. Le gestionnaire d'interaction annule dès que CanBePerformed rend faux.
- `override void OnActionStart(IEntity pUserEntity)` — Serveur : SRP_ZoneFall.OnSeizeStart(GetOwner(), user) — le moteur, chez l'autorité
- `override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)` — Serveur : SRP_ZoneFall.OnSeizeCancel — le moteur
- `override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)` — Serveur : SRP_ZoneFall.TrySeize, puis SRP_Utils.NotifyPlayer du refus — le moteur
- `override bool GetActionNameScript(out string outName)` — « Saisir le poste de commandement »
- `override bool CanBeShownScript(IEntity user)` — Composant présent et utilisateur à pied
- `override bool CanBePerformedScript(IEntity user)` — Client et serveur : autre saisisseur ou raison différente de LIBRE -> SetCannotPerformReason (texte lisible de SRP_ESeizeBlock) et faux
- `override bool HasLocalEffectOnlyScript()` — 
- `override bool CanBroadcastScript()` — 

**modded class SCR_CharacterDamageManagerComponent**
  Interruption de la saisie par un coup reçu (aucun champ ajouté). OnDamage existe : Game/Components/Damage/SCR_CharacterDamageManagerComponent.c:2033

### SRP_FrontEnemy.c (871 lignes)


**class SRP_FrontCellInfo**
  Cache par carré (la géométrie ne change pas) : note de poste (F8)
  - champ `bool m_bDone` — calculé
  - champ `int m_iScore` — carrefour 3, route principale 2, hauteur 2 (+1 si route), piste 1, 0 rien
  - champ `string m_sKind = ""` — « carrefour », « route », « hauteur » ou « piste »
  - champ `vector m_vPost` — point du poste
  - champ `float m_fTop` — altitude maximale

**class SRP_FrontPost**
  Un poste du front vivant (F7 à F9)
  - champ `int m_iCell = -1`
  - champ `vector m_vPos`
  - champ `string m_sKind`
  - champ `ref array<ref SRP_EnemyGroup> m_aGroups = {}` — toujours des ref (SRP_EnemyTrucks.c:45-47)
  - champ `int m_iNoPlayerSince` — heure Unix
  - champ `int m_iDeferUntil` — heure Unix
  - champ `IEntity m_Fort` — décor retranché

**class SRP_CounterAttack**
  La contre-attaque en cours (une à la fois). Lue et pilotée aussi par SRP_CmdManeuvers (crochets CA1 à CA7). Rien n'est sauvegardé (H3) : une attaque en cours s'annule au redémarrage.
  - champ `int m_iZone = -1`
  - champ `string m_sZone` — libellé « Régina (S07) »
  - champ `int m_iState = SRP_EFrontAttack.AUCUNE` — ANNONCEE puis ASSAUT
  - champ `int m_iWarnEndUnix` — fin de l'annonce (m_iAttackWarningMinutes)
  - champ `int m_iAssaultStartUnix`
  - champ `int m_iLastWaveUnix`
  - champ `int m_iNextWaveUnix`
  - champ `int m_iWaves` — vagues parties
  - champ `int m_iPlannedWaves` — 2 à 4, revu à la 2e vague (CA3)
  - champ `bool m_bRevised`
  - champ `int m_iPostedSoldiers`
  - champ `int m_iWave1Posted`
  - champ `int m_iWave1Fit`
  - champ `int m_iRetakenAtWave2`
  - champ `int m_iRegion = -1` — région du Commandeur
  - champ `bool m_bDisorganized` — OF8 : une vague de moins, ni mortier ni blindé
  - champ `bool m_bFeint` — MO8
  - champ `bool m_bSupportMortar` — CA4
  - champ `bool m_bSupportHeavy` — CA4
  - champ `bool m_bForced` — forcée par le Staff
  - champ `vector m_vTarget` — point clé, sinon centre des carrés bleus
  - champ `vector m_vOriginA` — axe 1
  - champ `vector m_vOriginB` — axe 2
  - champ `bool m_bHasB`
  - champ `ref array<ref SRP_EnemyGroup> m_aGroups = {}` — tous les groupes d'assaut (m_iAssaultZone = m_iZone)
  - champ `ref array<ref SRP_EnemyGroup> m_aLastWave = {}`
  - champ `ref array<ref SRP_EnemyGroup> m_aArrived = {}`
  - champ `ref array<ref SRP_EnemyGroup> m_aObjGroups = {}` — AdvanceAssault d'origine (Commandeur gelé)
  - champ `ref array<int> m_aObjCells = {}`
  - champ `ref array<int> m_aObjSince = {}`
  - champ `int m_iRetaken` — carrés repris par l'ennemi pendant l'attaque
  - champ `int m_iEnd = SRP_EAttackEnd.AUCUNE`

**class SRP_FrontEnemyComponentClass : SCR_BaseGameModeComponentClass**

**class SRP_FrontEnemyComponent : SCR_BaseGameModeComponent**
- `static SRP_FrontEnemyComponent GetInstance()` — L'instance (toutes machines) — socle, chute, territoire, missions, Commandeur
- `override void OnPostInit(IEntity owner)` — Toutes machines : s_Instance. Serveur : m_Commander = new SRP_Commander(this) (il DÉCLARE ses réglages, ne trace rien), DeclareSettings, GetOnPlayerSpawned().Insert(OnPlayerSpawnedNews), CallLater(Tick, 5000, true) — le moteur
- `override void OnDelete(IEntity owner)` — Serveur : d'abord SRP_FrontComponent front = SRP_FrontComponent.GetInstance(); if (front) front.SaveFinalFromEnemy(); (écriture complète de front.json, chaîne WriteTo, tant que ce composant et le Commandeur existent : l'ordre des OnDelete n'est pas garanti) ; puis Remove(Tick), désabonnement, if (m_Commander) m_Commander.Stop() ; enfin s_Instance = null — le moteur
- `override void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)` — Serveur : super, puis m_Commander.OnControllableDestroyed (pertes, officiers) — le moteur (SCR_BaseGameModeComponent.c:157)
- `SRP_Commander GetCommander()` — Le Commandeur (null s'il est absent) — Staff, SRP_Commander.Get
- `bool IsCommanderActive()` — SEULE définition de « Commandeur actif » : m_Commander && m_Commander.IsStarted() && !m_Commander.IsFrozen() (VU6) ; faux : les règles votées du front seules (camion d'alerte #69, AdvanceAssault, vague réputée arrivée…). Squelette : faux (règles votées) — ce composant (garde de TOUS les crochets de contre-attaque), territoire (GarrisonTrucksTick : camion d'entrée par RequestEntryTruck, pas de camion d'alerte), camions, SRP_CmdManeuvers.Tick. (L'hélico par tirage, lui, s'arrête dès que le Commandeur EXISTE : SRP_Commander.Get().)
- `void DeclareSettings()` — Serveur : Declare de ses clés front_* (contre-attaques, postes, offensive, regarnissage, infiltration) — OnPostInit ; puis SRP_FrontComponent.Start fait le Reload
- `void LoadSettings()` — Serveur : relit ses clés front_*, puis m_Commander.LoadSettings() — SRP_FrontComponent (Start, ReloadSettings)
- `void OnZoneCaptured(int zone, bool staff)` — Une zone est tombée (D1) ou forcée bleue (staff) : ClearLosses/ClearSent/bonus/vidée de ses localités, postes de la zone retirés, attaque visant la zone close, puis m_Commander.OnZoneCaptured(zone, staff) (Q9 : rien de visible si staff) — SRP_ZoneFall (3e appel)
- `void OnZoneLost(int zone, int reason, bool staff)` — Une zone est repassée rouge (Q2, E4, H2, Staff) : ses localités redeviennent hostiles (SRP_SectorState.m_iOwner = ENNEMI) ; si m_Attack vise cette zone : EndAttack(SRP_EAttackEnd.PERDUE) (SEUL endroit qui fait AdoptAssailants et remet m_iAssaultZone à -1) ; puis m_Commander.OnZoneLost(zone, reason, staff) — SRP_ZoneFall
- `void OnLocalityTaken(string locality)` — Tous les points clés de la localité sont pris : m_iOwner = NOUS, WithdrawLocality (repli, sirène muette, camions rappelés), pertes à zéro, puis m_Commander.OnLocalityTaken(locality) — SRP_ZoneFall
- `void OnKeyPointSeized(int zone, int keyPoint, int playerId)` — D4 réussie, AVANT le changement de camp : SRP_CmdIntel.OnKeyPointSeized(zone) (renseignement, dépôt révélé ; un 2e appel rafraîchit l'heure) — SRP_ZoneFall.Seize
- `void OnMissionSucceeded(int missionType, vector site, string missionId, string targetPrefab, string author)` — Mission réussie, UNE entrée par fait (trou 14) : SRP_Commander.OnMissionSucceeded répartit (renseignement, dépôt révélé ECOUTE/DOCUMENTS, quart de stock SABOTAGE/CACHE, alerte) — SRP_MissionManagerComponent.End(SUCCES)
- `bool OnSabotageUsed(IEntity target, int playerId, out string reply)` — En tête de OnObjectiveUsed : l'objet utilisé est-il le dépôt ennemi ou une pièce d'appui du Commandeur ? Vrai = consommé (message dans reply) — SRP_MissionManagerComponent.OnObjectiveUsed SEUL (SRP_SabotageAction passe déjà par OnObjectiveUsed, SRP_Missions.c:3143 : une seule entrée par « Saboter »)
- `void OnThreatChanged(int threat)` — La menace de l'île a changé : relais aux ressources du Commandeur (OnThreatChanged) — SRP_FrontComponent.AddThreat
- `void OnFrontReset(int kind, int zone)` — Remise à plat d'une zone (SRP_EFrontReset.ZONE SEULEMENT) : attaque close si elle la vise, localités de la zone sans pertes — SRP_FrontComponent.NotifyReset (CAMPAGNE -> ResetAll, RESTAURATION -> ReadFrom puis OnRestored)
- `void ResetAll(string reason)` — Nouvelle campagne (B3, K1, Staff) : attaque, postes et horloges à zéro, offensive oubliée, puis m_Commander.ResetCampaign(reason) — SRP_FrontComponent.ResetCampaign
- `void OnRestored()` — Après RestoreDaily (ReadFrom déjà fait) : attaque close, postes recalculés, puis m_Commander.OnRestored() — SRP_FrontComponent.RestoreDaily
- `void WriteTo(JsonSaveContext ctx)` — Clés fe_offDay, fe_offText, fe_offUnix, fe_offZone, puis m_Commander.WriteTo(ctx) (cmd_*) — SRP_FrontComponent.WriteTo
- `void ReadFrom(JsonLoadContext ctx)` — Relit les clés fe_*, puis m_Commander.ReadFrom(ctx) — SRP_FrontComponent.ReadFrom (après zones et localités)
- `int CurrentLosses(string locality)` — Pertes courantes : pertes x max(0, 1 - (maintenant - date) / (m_fLossRefillHours x 3600)) — territoire, Commandeur
- `void AddLosses(string locality, int n)` — Pertes = CurrentLosses + n, date = maintenant (n négatif accepté, RE13), MarkDirty(false) — territoire, Commandeur
- `void ClearLosses(string locality)` — Pertes à zéro (localité prise, nouvelle campagne) — OnLocalityTaken, OnZoneCaptured
- `int LiveLosses(SRP_SectorState state)` — Soldats manquants d'une garnison posée : prévus - survivants (SRP_EnemyGarrison.CountPlanSurvivors) — territoire (avant chaque DeleteAll : AddLosses(nom, LiveLosses))
- `int CurrentSent(string locality)` — Envoyés courants (MA2), même formule de regarnissage que les pertes — SRP_CmdManeuvers
- `void AddSent(string locality, int n)` — Envoyés + n, date = maintenant (débit AU DÉPART, une seule fois) — SRP_CmdManeuvers (OnTruckLaunched pour un camion hors colonne, NewColumn pour une colonne)
- `void ReturnSent(string locality, int n)` — Envoyés - n, plancher 0 (soldats rendus vivants) — SRP_Enemy (RetireTick : SEULE porte de retour des soldats retirés vivants), SRP_CmdManeuvers (colonne annulée ; RE13 : colonnes et camions relus par ReadFrom)
- `void ClearSent(string locality)` — Envoyés à zéro — OnZoneCaptured, SRP_CmdManeuvers
- `int CurrentBonus(string locality)` — Renforts reçus courants (même formule) — SRP_CmdManeuvers
- `void AddBonus(string locality, int n)` — Reçus + n, plafonné pour que Effective reste <= 60 — SRP_CmdManeuvers
- `int GetLocalityNominal(string locality)` — Nominal = GarrisonSoldiers rendu public (base + joueurs + menace LOCALE, x1,25 si l'officier vit, 60 au plus) — Commandeur, capacité
- `int GetLocalityEffective(string locality)` — Effective = Nominal - pertes - envoyés + reçus, borné de 0 à 60 ; SEUL effectif « décidé » d'une pose : la pose vaut min(Effective, SRP_CmdCapacity.Get().GarrisonCap(nom, Effective)), appliqué par SRP_EnemyGarrison seul ; rien si SRP_CmdManeuvers.IsLocalityEmptied — territoire (pose F11), Commandeur, capacité
- `bool GetZoneStrength(int zone, out int planned, out int nominal)` — Dérivé pour le PC : effectif prévu et nominal de la localité principale de la zone ; faux sans localité — SRP_CmdScreens (VU2), SRP_FrontScreens
- `int GetZoneGarrisonGroups(int zone)` — Groupes de garnison posés dans la zone (Staff) — SRP_FrontScreens
- `bool IsLocalityUnderAttack(string locality)` — Joueur valide dans le rayon + 150 m du point clé, ou groupe de la localité au contact depuis moins de 60 s — capacité (C6), garnison (classe COMBAT)
- `bool IsGarrisonPosted(string locality)` — Garnison posée (vrai même vide par F12, trou 21) — SRP_ZoneFall (poste de commandement), Commandeur
- `bool IsGarrisonSettled(string locality)` — Garnison posée ET installée (SRP_EnemyComponent.IsSettled) — SRP_ZoneFall (saisie D4)
- `void WithdrawLocality(string locality, string why)` — Retrait des défenseurs d'une localité prise (SRP_Territory.c:1672-1700 : loin ou vu -> survivant RetireLater, sinon DeleteAll ; OnGarrisonGone ; sirène muette) — OnLocalityTaken
- `void AdoptAssailants(string locality, array<ref SRP_EnemyGroup> groups)` — Zone repassée rouge : les assaillants présents deviennent la garnison de la localité (ConvertToGarrison, m_iAssaultZone = -1, m_iPlannedSoldiers, sirène) — EndAttack(PERDUE) SEUL (appelée par OnZoneLost)
- `bool IsInGarrisonedLocality(vector position, float margin)` — Position dans une localité hostile dont la garnison est posée (rayon + margin) ; remplace IsInsideEnemySector — missions (G11 : IsInEnemyStronghold), Commandeur
- `bool IsLocalityOnFront(SRP_SectorState state)` — F11 : la localité est dans une zone au contact (E1) — SRP_TerritoryComponent (choix de garnison)
- `float GarrisonScore(SRP_SectorState state, float reach)` — F11 : score de garnison = reach - bonus du front ; plus petit = prioritaire — SRP_TerritoryComponent
- `bool AllowAmbientFor(IEntity player)` — F10 : une patrouille peut naître autour de ce joueur (carré rouge ; carré bleu près du rouge selon m_iInfiltrationChance, résultat gardé pour AcceptAmbientSpawn) — SRP_Enemy (Tick ambiant)
- `bool AcceptAmbientSpawn(vector position, bool infiltration)` — F10 : point de pose acceptable (rouge, ou bleu près du rouge en infiltration) — SRP_Enemy (SpawnPatrolNear, SpawnRoadPatrol)
- `bool IsPostPossibleNearPlayers()` — Un carré de poste est à portée d'un joueur actif (m_fPostSpawnDistance + 500) : la réserve des postes joue — SRP_CmdCapacity (IsPostReserveActive)
- `bool IsInMainRedMass(int cell)` — Le carré appartient à la masse rouge principale (MA12 : poches) — SRP_CmdManeuvers
- `bool IsCounterAttackActive()` — Une contre-attaque est en cours (annoncée ou en assaut) — Commandeur, capacité
- `int GetCounterAttackZone()` — Zone visée par la contre-attaque en cours, -1 sinon — Commandeur (gravité, appuis), capacité
- `SRP_CounterAttack GetCounterAttack()` — La contre-attaque en cours, null sinon — SRP_CmdManeuvers (crochets CA)
- `string GetAttackStatusText(int zone)` — Texte d'état construit par le serveur (trou 20) : « ATTAQUE annoncée, assaut dans 9 min », « vague 2 » ; "" sans attaque — SRP_FrontScreens (PC, Staff). L'ÉTAT d'attaque d'une zone se lit UNIQUEMENT sur le socle (SRP_FrontComponent.GetZoneAttackState, drapeaux posés par SetZoneFlag) : pas de seconde source ici
- `string ForceAttack(int zone, string author)` — Contre-attaque forcée maintenant sur une zone à nous au front, non protégée (m_bForced) ; rend le compte rendu — SRP_FrontScreens (action territoire:attaque)
- `string CancelAttack(string author)` — Annule la contre-attaque en cours (EndAttack ANNULEE, radio AttackCalledOff si annoncée) — SRP_FrontScreens (action territoire:attaque-annuler, 2 clics)
- `string ForceOffensive(bool success, string author)` — Lance l'offensive de la nuit maintenant, réussite imposée ou échec (H2) — SRP_FrontScreens (actions territoire:offensive:1 et territoire:offensive:0, 2 clics)
- `string GetAttackReport()` — Rapport de l'attaque en cours (zone, état, vagues, groupes, carrés repris, fin prévue) — SRP_FrontScreens (bloc « Ennemi du front » de la page Territoire)
- `string GetPostsReport(vector from, float radius)` — Postes du front autour d'un point (carré, genre, effectif, retour après anéantissement) — SRP_FrontScreens (page « postes », rayon 3000 m autour du Staff)
- `string GetNightReport()` — Dernière offensive de la nuit (jour, zone, chance, résultat) et prochaine fenêtre — SRP_FrontScreens (bloc « Ennemi du front »)
- `void BroadcastSound(string acp, string eventName, vector pos)` — Serveur : Rpc(RpcDo_SRPCmdSound, …) puis appel local si !System.IsConsoleApp() — SRP_CmdSupport (départ du mortier, MO3)
- `void BroadcastLaunch(RplId shell, vector dir)` — Serveur : Rpc(RpcDo_SRPCmdLaunch, …) (relance d'obus en repli) — SRP_CmdSupport

### SRP_FrontEconomy.c (242 lignes)


**class SRP_FrontEconomy**
- `void SRP_FrontEconomy(SRP_FrontComponent front)` — Créée par le socle dans OnPostInit (serveur) — SRP_FrontComponent
- `void Start()` — Après Load : dépôt chargé dont le carré n'est plus bleu ou plus dans la zone -> effacé (stock gardé, journal) ; caisses reposées (RespawnBoxes) et repères des dépôts — SRP_FrontComponent.Start
- `void Tick(int nowUnix)` — Passage de 5 s (3e de la boucle unique) : pour chaque zone à nous qui produit et n'est pas coupée (E4, m_bProductionStopsWhenCut), m_iProductionSec += 5 ; à 3600, Produce — SRP_FrontComponent.Tick
- `void OnCellsChanged(notnull array<int> cells, int owner, int reason)` — G7 selon la CAUSE (SRP_EFrontReason) d'un carré du dépôt qui n'est plus bleu (m_bDepotLostWithCell) : REPRISE seule -> DestroyDepot (radio DepotDestroyed + évènement depot_detruit) ; STAFF (ForceCell, correction Q9) -> ClearDepot en silence, stock gardé ; ZONE_PERDUE, ISOLEMENT, OFFENSIVE -> rien ici (le dépôt est effacé en silence par OnZoneLost) ; REMISE, RESTAURATION -> rien ici (OnFrontReset) ; carrés passés bleus -> rien — socle, 3e appel direct après chaque lot (trou 6)
- `void OnZoneCaptured(int zone)` — Zone prise : production et stock à zéro — SRP_ZoneFall (2e appel après la chute)
- `void OnZoneLost(int zone)` — Zone perdue : stock à zéro, dépôt effacé et caisse supprimée (comme Lose, SRP_Territory.c:1744-1747) — SRP_ZoneFall
- `void OnFrontReset(int kind, int zone)` — Remise à plat (SRP_EFrontReset) : caisses supprimées, stocks à zéro, dépôts effacés (ZONE : cette zone seule ; RESTAURATION : caisses et repères refaits d'après les champs restaurés) — SRP_FrontComponent.NotifyReset
- `string InstallDepot(int playerId, IEntity kit)` — G6 : kit posé au sol (droits et test du kit repris de SRP_Territory.c:2193-2203) dans une zone à nous hors base, sur un carré bleu, pas au front (m_bDepotNotOnFront : « Trop près de l'ennemi… %2 carré(s) possible(s) »), un seul dépôt par zone ; SetDepot, kit supprimé, radio DepotInstalled ; rend "" ou le refus — SRP_DepotInstallAction
- `string ForceDepot(int zone, vector position, string author)` — Staff : dépôt fixé ICI sans kit (position dans la zone, carré bleu ; un carré du front est accepté, correction) — SRP_FrontScreens
- `string RemoveDepot(int zone, string author)` — Staff : retire le dépôt, garde le stock (comme SRP_Territory.c:2242-2260) — SRP_FrontScreens
- `int PerHour(int zone)` — Paquets par heure de la zone (m_iRate, sinon le défaut de sa ressource), 0 si elle ne produit rien — StatusText
- `string StatusText(int zone)` — Texte du PC et du Staff : « produit 10 vivres/h, 12 en attente, dépôt carré 072 023 », « coupée de la base : production arrêtée » ou « ne produit rien » — SRP_FrontScreens
- `int GetBoxes(notnull array<string> labels, notnull array<int> counts, notnull array<int> resources)` — Zones à nous avec un stock > 0 et une ressource : libellé « Régina (S07) », paquets, SRP_EResource ; rend le nombre — SRP_Bridge (boxes[], clés sector, count, resource inchangées)

**class SRP_DepotInstallAction : ScriptedUserAction**
  Action « Installer le dépôt ici » du kit (Prefabs/Paquets/SRP_KitDepot.et la trouve par son NOM de classe). DÉPLACÉE depuis SRP_Territory.c (ancienne copie retirée) ; elle appelle maintenant l'économie du front.
- `override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)` — Serveur : SRP_FrontComponent.GetInstance().GetEconomy().InstallDepot(joueur, kit), puis NotifyPlayer du refus
- `override bool GetActionNameScript(out string outName)` — « Installer le dépôt ici »
- `override bool CanBeShownScript(IEntity user)` — Seulement quand le kit est posé au sol (ni dans un sac, ni dans un coffre)
- `override bool CanBePerformedScript(IEntity user)` — 
- `override bool HasLocalEffectOnlyScript()` — 

### SRP_FrontMap.c (175 lignes)


**modded class SCR_MapEntity**
  Accrochage du calque. Tout champ ou méthode ajouté porte SRP_ (classe moddée).
- `override protected void OnMapOpen(MapConfiguration config)` — Crée le calque au besoin et l'attache AVANT super (les repères vanilla, créés ensuite, passent au-dessus) — le moteur
- `override protected void OnMapClose()` — Détache le calque (la géométrie est gardée pour la prochaine ouverture) — le moteur
- `override protected void UpdateMap(float timeSlice)` — super (pan et zoom de l'image déjà appliqués), puis m_SRP_FrontLayer.Update(this) — le moteur, chaque image
- `void SRP_GetScreenTransform(out float scale, out float offsetX, out float offsetY)` — Monde vers pixels du canevas, sans arrondi, même formule que WorldToScreen(…, withPan = true) : x écran = x x scale + offsetX ; y écran = offsetY - z x scale, avec scale = m_fZoomPPU, offsetX = m_Workspace.DPIScale(m_iPanX) - m_iMapOffsetX x m_fZoomPPU, offsetY = m_Workspace.DPIScale(m_iPanY) + (m_iMapSizeY + m_iMapOffsetY) x m_fZoomPPU — SRP_FrontMapLayer.Update

**class SRP_FrontMapLayer**
  Le calque du front d'une carte ouverte (joueur). Tous les tableaux de commandes sont des ref : SetDrawCommands ne garde qu'un pointeur (CanvasWidget.c:14-24).
- `bool Attach(SCR_MapEntity mapEntity, MapConfiguration config)` — Rien si m_bMapLayer est faux ou si le mode n'est pas autorisé (FULLSCREEN, SPAWNSCREEN, PLAIN, EDITOR selon les réglages) ; canevas VISIBLE | BLEND | IGNORE_CURSOR | NOFOCUS dans MapFrame (ou m_sMapLayerParent), plein cadre, rang holder + m_iMapZOrderOffset ; diagnostic m_bMapDiag ; rend vrai si attaché — SCR_MapEntity.OnMapOpen
- `void Detach()` — RemoveFromHierarchy du canevas (et des TextWidget de repli), m_fScale = -1 — SCR_MapEntity.OnMapClose
- `void Update(SCR_MapEntity mapEntity)` — Chaque image carte ouverte : front prêt ? version changée -> BuildWorld ; transformation ou taille changée -> retracé (BuildScreen puis SetDrawCommands) au plus toutes les m_iMapRedrawMinMs — SCR_MapEntity.UpdateMap

### SRP_FrontRadio.c (353 lignes)


**class SRP_FrontRadio**
- `static void QueueCell(int cell, int owner, int reason)` — Note un carré qui a changé de camp avec sa cause (SRP_EFrontReason) — socle, 5e appel direct après chaque lot
- `static void Flush()` — Fin de chaque boucle de 5 s (4e) : journal FRONT par carré (m_bJournalCells) ; radio CAPTURE / REPRISE par (zone, sens) : « Radio : carré %1 pris — %2, %3/%4 carrés à nous », « Radio : carré %1 perdu, repris par l'ennemi — %2, %3/%4 carrés à nous », ou « Radio : %1 carrés pris — %2 : %3 » (au plus m_iRadioMaxRefs références) ; puis vide la file — SRP_FrontComponent.Tick
- `static void ClearQueue()` — Vide la file sans rien dire (remise à plat, restauration) — socle
- `static void ZoneFell(int zone, int owner, int threat, int reason)` — Chute ou perte d'une zone, selon la cause : ZONE_TOMBEE « Zone %1 capturée — menace %2/10 » (zone_prise) ; ZONE_PERDUE « Zone %1 PERDUE, reprise par l'ennemi — menace %2/10 » (zone_perdue) ; ISOLEMENT « Zone %1 PERDUE : restée coupée de la base %2 h — menace %3/10 » (zone_perdue) ; OFFENSIVE « Pendant l'absence de tous, l'ennemi a pris %1 — menace %2/10 » (offensive) ; STAFF : rien (Q9) — SRP_ZoneFall
- `static void KeyPointSeized(int zone, int role, string author, int blue, int need)` — D4 : « Radio : poste de commandement ennemi de %1 capturé par %2 — la zone tombera à %3/%4 carrés à nous » ; « QG ennemi » pour le rôle QG (D6) ; journal TERRITOIRE — SRP_ZoneFall.Seize
- `static void HalfReached(int zone)` — D1, radio seule, une fois par zone (m_bRadioHalf) : « Radio : %1 — la moitié des carrés est à nous, reste le PC ennemi » — SRP_ZoneFall.EvaluateZone
- `static void AttackAnnounced(int zone, int minutes)` — F6 (G15 : alerte seule) : « Renseignement : l'ennemi prépare une attaque sur %1, assaut dans %2 minutes. Défendez la zone. » ; évènement zone_attaque — SRP_FrontEnemyComponent.LaunchAttack
- `static void AttackWave(int zone, int wave, string axes)` — « Radio : %1 — l'ennemi attaque, vague %2, mouvement signalé depuis %3 » ; évènement zone_assaut à la 1re vague seulement — SRP_FrontEnemyComponent.SendWave
- `static void Defended(int zone, int waves, int threat)` — F4, C8 : « Zone %1 défendue, %2 vague(s) repoussée(s) — menace %3/10 » ; évènement zone_defendue — SRP_FrontEnemyComponent.EndAttack
- `static void AttackBroken(int zone, int retaken)` — C8 (60 min) : « Radio : %1 — l'ennemi rompt le contact et se replie ; %2 carré(s) repris par lui restent rouges. » ; journal ; évènement zone_assaut — SRP_FrontEnemyComponent.EndAttack
- `static void AttackCalledOff(int zone)` — « Radio : l'attaque ennemie annoncée sur %1 n'aura pas lieu. » ; journal, pas d'évènement — SRP_FrontEnemyComponent.EndAttack (ANNULEE pendant l'annonce)
- `static void ZoneCut(int zone, int hours)` — E4 : « Radio : %1 est coupée de la base : plus de production, perdue dans %2 h si le passage n'est pas rouvert » ; évènement zone_coupee — SRP_FrontComponent.CheckCuts
- `static void CutReminder(int zone, int minutesLeft)` — E4, radio seule, m_iCutReminderMinutes avant la perte : « Radio : %1 coupée de la base, perdue dans %2 min » — SRP_FrontComponent.CheckCuts
- `static void Reconnected(int zone)` — E4 : « Radio : %1 de nouveau reliée à la base » ; évènement zone_reliee — SRP_FrontComponent.CheckCuts
- `static void Frozen(bool on, string author)` — J3 : « Front gelé par l'état-major : ni capture ni perte » / « Front dégelé » ; évènement front — SRP_FrontComponent.SetFrozen
- `static void Restored(string day, string author)` — J2 : UNE ligne « Front ramené à l'état du %1 » ; évènement front (remise à plat, jamais des dizaines de prises) — SRP_FrontComponent.RestoreDaily
- `static void NewCampaign(int campaign)` — B3, K1 : « Nouvelle campagne : tout est à reprendre depuis Levie » ; évènement campagne — SRP_FrontComponent.ResetCampaign
- `static void Victory()` — B3 : « VICTOIRE : toute l'île est à nous » ; évènement victoire — SRP_FrontComponent.CheckVictory
- `static void CaptureWithoutMission(int zone)` — G2 : « Zone %1 capturée SANS mission activée au tableau : ni prime, ni crédit de mission » ; journal TERRITOIRE, évènement commandement, NotifyOfficiers — SRP_MissionManagerComponent.OnZoneCaptured (seul payeur)
- `static void CaptureAbandoned(int zone, int threat)` — Mission de capture abandonnée : « Radio : capture de %1 abandonnée : l'ennemi reprend position — menace %2/10 » (libellé de zone ; zone -1 : « la zone visée ») ; journal TERRITOIRE, sans évènement ni mot confidentiel — SRP_TerritoryComponent.OnCaptureFailed (après AddThreat(+1))
- `static string NightOffensive(int zone, bool success, int threat)` — H2 : texte du résultat pour les nouvelles personnelles ; en cas d'ÉCHEC, annonce aussi (journal, évènement offensive) ; en cas de réussite, l'annonce est faite par ZoneFell(OFFENSIVE) — SRP_FrontEnemyComponent.RunOffensive
- `static void NightNews(int playerId, string text)` — H2 : « Radio : » + texte au joueur, à sa première apparition — SRP_FrontEnemyComponent.OnPlayerSpawnedNews
- `static void DepotProduced(int zone, int packets, int resource, string cellRef)` — « Radio : %1 — %2 paquet(s) de %3 produits à ramasser au dépôt, carré %4 » — SRP_FrontEconomy.Produce
- `static void DepotNoDepot(int zone, int packets, int resource)` — « Radio : %1 a produit %2 paquet(s) de %3, mais n'a pas de dépôt : posez un kit » — SRP_FrontEconomy.Produce
- `static void DepotInstalled(int zone, string author, string cellRef)` — « Dépôt de %1 installé par %2, carré %3 » (radio, journal) — SRP_FrontEconomy.InstallDepot
- `static void DepotDestroyed(int zone, string cellRef)` — G7 : « Dépôt de %1 perdu : détruit par l'ennemi, carré %2. Il faudra un nouveau kit » ; évènement depot_detruit — SRP_FrontEconomy.DestroyDepot
- `static void LegacyImported(string info)` — K1 : journal TERRITOIRE public « Nouvelle campagne… » avec le résumé de l'ancien territoire archivé — SRP_FrontComponent.ImportLegacy
- `static void LegacyPoured(int vivres, int munitions)` — K2 : « Versement des anciens secteurs : %1 vivres et %2 munitions rangés à la base » (journal RESSOURCE, NotifyOfficiers) — SRP_FrontComponent.MigrationTick
- `static void OfficerFell(int region, string surname, bool captured, int zone)` — « Radio : l'officier ennemi %1 est tombé, région de %2. » ou « … a été capturé … » ; journal (+ « — zone » si zone >= 0) et évènement officier_tombe (radio seule si vu_officier_discord = 0) — SRP_CmdRegions
- `static void RegionLiberated(int region, string surname)` — OF11 : « Radio : toute la région de %1 est à nous : l'officier ennemi %2 a quitté l'île. » ; évènement region_liberee — SRP_CmdRegionBook.CheckLiberation (jamais pour une zone forcée par le Staff, Q9)
- `static void IntelGained(int region, int source, string detail, bool depotRevealed, string depotRef, int zone)` — VU2, RE9 : « Radio : renseignement — … : la région de %1 est renseignée (PC, onglet Territoire). » selon la source (SRP_CmdIntel : PC, OFFICIER, MISSION), + « Dépôt ennemi repéré au carré %2. » ; évènement renseignement — SRP_CmdIntel
- `static void EnemyDepotSabotaged(int region, int hours, int zone)` — RE8 : « Radio : dépôt ennemi saboté, région de %1 : son ravitaillement est réduit de moitié pendant %2 h. » ; évènement depot_ennemi — SRP_CmdDepot
- `static void SupportSilenced(int region, bool battery)` — MO9, AP1, radio SEULE si vu_radio_silence = 1 : « Radio : pièce de mortier ennemie réduite au silence, région de %1. » ou « Radio : batterie d'artillerie ennemie détruite, région de %1. » — SRP_CmdSupport
- `static void ArmorBroughtBack(string vehicleName, int reward)` — C3 : « Radio : blindé ennemi %1 ramené à la base — prime de %2 € versée au coffre. » ; évènement blinde_ramene (le versement reste à SRP_CmdArmor) — SRP_CmdArmor

**class SRP_FrontMarkers**
  Repères de carte des points clés (I3) et des dépôts ennemis révélés (RE9), serveur. Remplace PlaceMarker et UpdateBlink de SRP_Territory.c:2072-2163. Les repères des dépôts de ZONE restent à SRP_FrontEconomy.
- `static void Tick()` — Fin de chaque boucle de 5 s (5e) : pour chaque point clé, visible = m_bKeyPointMarkers et zone hors base et (à nous, au front, attaquée, ou !m_bHideRearKeyPoints) ; signature propriétaire + 2 x front + 4 x attaque + 16 x point pris + 32 x visible ; si elle change : retrait puis repose (« PC ennemi S08 », « QG ennemi S08 », « Point clé S07 », « ATTAQUE ANNONCÉE S07 », « ATTAQUE S07 ») ; puis TickEnemyDepots — SRP_FrontComponent.Tick
- `static void TickEnemyDepots()` — RE9 : un repère par dépôt ennemi révélé (SRP_CmdResources, dépôts révélés), retiré quand il ne l'est plus (vu_depot_icone, vu_depot_couleur) — Tick
- `static void Clear()` — Retire tous les repères (remise à zéro, restauration) ; ils reviennent au passage suivant — socle (NotifyReset)

### SRP_FrontScreens.c (205 lignes)


**class SRP_FrontScreens**
- `static string PCTerritoire(int playerId)` — Remplace SRP_PC.c:528-544 : « M|Territoire : 12 zones sur 66 à nous · 214/1395 carrés · menace 3/10 », gel, Attaques, Au front — zones ennemies (+ PCZoneSuffix), PCIntelSection, Au front — nos zones, Nos zones à l'arrière (coupées : E4), Zones ennemies à l'arrière (m_bPCRearEnemies, gris, « · région de X ») ; chaque groupe trié par distance au joueur — SRP_PCScreens.Territoire(playerId)
- `static string StatusPlain()` — Même contenu sans préfixes, limité aux attaques et aux zones au front — SRP_ChatCommands (/territoire)
- `static string StaffHeaderPart()` — En-tête Staff : « 3/10, 12/66 zones » (+ « , FRONT GELÉ ») ; SRP_Admin y colle SRP_CmdScreens.StaffHeaderPart par concaténation — SRP_AdminComponent.Header
- `static string CellHere(vector position)` — « Carré 074 042 · Perelle (S08) · ennemi · au front · capture 7/15 min » — SRP_Admin (page Rapide), StaffTerritoire
- `static string ZoneOwnerWord(int zone)` — « à nous », « ennemie » ou « base » — lignes de zone
- `static string ShareText(int zone)` — « 6/25 carrés » — lignes de zone
- `static string StaffTab(int playerId, vector staffPos, bool inGame, array<string> parts)` — Page demandée selon parts (« territoire », « zone:S08 », « liste:bleues:0 », « sauvegardes:0 », « points:0 », « postes ») — SRP_AdminComponent.BuildMenu
- `static string StaffTerritoire(int playerId, vector staffPos, bool inGame)` — Page principale : ici (CellHere, carre-bleu, carre-rouge), gel [x], menace et comptes, zones au front (liens zone:<code>), listes, sauvegardes, points clés (« N manquants »), nouvelle campagne (2 clics) ; le Staff voit tout (pas de filtre I3) + ligne SRP_CmdScreens.StaffTerritoryLine sous le gel ; bloc « H|Ennemi du front » (trou 22) : lignes I de SRP_FrontEnemyComponent.GetAttackReport() et GetNightReport(), K|r|territoire:attaque-annuler (CancelAttack, 2 clics, seulement si une attaque est en cours), K|r|territoire:offensive:1 et K|r|territoire:offensive:0 (ForceOffensive réussite / échec, 2 clics), lien « postes » — StaffTab
- `static string StaffZone(int zone, vector staffPos)` — Page d'une zone : part bleue, contestés, contact, coupure, points clés et chute (GetZoneFallText), garnison (groupes, ennemis en zone), attaque (GetAttackStatusText), production (StatusText), dernier changement, distance ; actions tp, tp-centre, zone-bleue, zone-rouge, zone-reset, attaque, depot-ici, depot-retirer — StaffTab
- `static string StaffZoneList(string which, int page, vector staffPos)` — Liste paginée « bleues », « rouges » ou « coupees » (une ligne K par zone, suivant / précédent / retour) — StaffTab
- `static string StaffSaves(int page)` — Copies datées (J2) : avertissement, puis « K|r|territoire:restaurer:2026-09-25|!! 25/09/2026 — 14 zones… (2 clics) », les plus récentes d'abord — StaffTab
- `static string StaffKeyPoints(int page, vector staffPos)` — Points clés (Q8) : une ligne par localité (genre, zone, source de chaque point : repère, fichier ou MANQUANT — nom de la carte), état du poste (GetPostReport), distance ; actions pc-tp, pc-saisir, pc-reposer — StaffTab
- `static string StaffPosts(vector staffPos, bool inGame)` — Page « postes » : SRP_FrontEnemyComponent.GetPostsReport(staffPos, 3000) en lignes I (Staff hors jeu : tous les postes), puis retour vers « territoire » — StaffTab
- `static string RunStaff(int playerId, vector staffPos, bool inGame, string action, array<string> parts, string author, out bool wantTeleport, out vector teleportTo)` — Exécute une action de l'onglet : reset -> NewCampaign ; gel -> SetFrozen ; carre-bleu / carre-rouge -> ForceCellAt ; tp, tp-centre (wantTeleport) ; zone-bleue / zone-rouge -> ForceZone ; zone-reset -> ResetZone ; attaque -> SRP_FrontEnemyComponent.ForceAttack ; attaque-annuler -> CancelAttack ; offensive:1 / offensive:0 -> ForceOffensive(true / false) ; depot-ici / depot-retirer -> économie ; restaurer -> RestoreDaily ; pc-tp, pc-saisir (ForceSeize), pc-reposer (RespawnCP) ; puis UNE ligne de journal [STAFF] (la seule pour l'action : les primitives appelées n'en écrivent pas) ; rend le compte rendu — SRP_AdminComponent.Run (qui téléporte si wantTeleport)
- `static bool IsInternalPage(string action)` — Actions qui ouvrent une page sans rien exécuter (zone, liste, sauvegardes, points, postes) — SRP_AdminComponent.Execute
- `static bool NeedsConfirm(string action)` — Actions à deux clics (reset, restaurer, zone-reset, attaque-annuler, offensive) — SRP_AdminComponent.NeedsConfirm
- `static string PageAfter(string action, array<string> parts)` — Page à rouvrir après une action (zone:<code>, sauvegardes:0, territoire ; attaque-annuler et offensive -> territoire) — SRP_AdminComponent.PageAfter

## 3. Ordre de démarrage (serveur)

À t = 0, `OnPostInit` de chaque composant (ordre non garanti entre eux ; rien de lourd ici, instance jetable du
Workbench) :
- `SRP_FrontComponent` : `s_Instance` (toutes machines) ; serveur : `SRP_Paths.EnsureDirectories()`,
  `FileIO.MakeDirectory(DAILY_DIR)` et `(ARCHIVE_DIR)`, création de `m_Capture`, `m_ZoneFall`, `m_Economy`
  (constructeurs seuls), `CallLater(Start, 5000)`.
- `SRP_FrontEnemyComponent` : `s_Instance` (toutes machines) ; serveur : `m_Commander = new SRP_Commander(this)`
  (le constructeur ne fait que DÉCLARER ses clés), `DeclareSettings()`, abonnement `GetOnPlayerSpawned`,
  `CallLater(Tick, 5000, true)` (le Tick ne fait rien tant que le front n'est pas prêt).
- `SRP_KeyPointComponent` (repères) : définition notée côté serveur, repère supprimé sur chaque machine.
- `SRP_TerritoryComponent` : son Start à 5 s attend `SRP_FrontComponent.IsReady()` (nouvel essai toutes les 2 s)
  avant `BuildLocalities` (voir §8).

À t = 5 s, `SRP_FrontComponent.Start` :
1. `SCR_MapEntity.GetMapInstance()` prête ? sinon nouvel essai toutes les `m_iMapRetrySeconds`, `m_iMapRetryMax` fois,
   puis ERREUR au journal.
2. Réglages : `DeclareSettings()` (socle, capture, chute, économie, radio) ;
   `SRP_MissionManagerComponent.DeclareFrontSettings()` ; `SRP_CmdSettings.Reload("démarrage")` (lit les DEUX
   fichiers, ajoute les clés manquantes) ; puis `LoadSettings()` ici, `SRP_FrontEnemyComponent.LoadSettings()`
   (qui appelle `SRP_Commander.LoadSettings()`), `SRP_MissionManagerComponent.LoadFrontSettings()`.
3. Géométrie : `new SRP_FrontGeometryBuilder(this)`, `Build()`, adoption de ses tableaux, `WriteReport()`.
4. `SRP_Commander.Get().BuildRegions()` (si le Commandeur existe) : régions tracées AVANT la lecture de leur état ;
   il lit lui-même les lignes « region » de `front_retouches.txt`.
5. `Load()` : `front.json` -> `ReadFrom(ctx)` (carrés, zones, localités, extras, puis
   `SRP_FrontEnemyComponent.ReadFrom(ctx)` qui chaîne `SRP_Commander.ReadFrom(ctx)`).
   Pas de fichier : `NewCampaignState()` ; si `territoire.json` existe et `m_bLegacyImport` : `ImportLegacy()` (K1).
   Fichier illisible : ERREUR, `m_bLoaded` reste faux (rien n'est jamais écrit), officiers prévenus.
6. `m_bEnemyChained = (SRP_FrontEnemyComponent.GetInstance() != null)` (garde de la chaîne de sauvegarde) ;
   `m_bLoaded`, `m_bReady` ; `m_Capture.Start()`, `m_ZoneFall.Start()`, `m_Economy.Start()` ;
   `BroadcastGeometry()` puis `BroadcastState()` (joueurs arrivés avant) ; versions + 1.
7. `UpdateTopology()` puis `CheckCuts(maintenant)` : le temps serveur éteint compte pour E4 (Q1).
8. Victoire enregistrée et `m_iVictoryResetMinutes == 0` : `ResetCampaign("victoire")` (B3 : nouvelle campagne au
   redémarrage).
9. `CallLater(Tick, 5000, true)` — LA boucle unique.
10. Journal TERRITOIRE : « Front : 1395 carrés, 66 zones, base 161 carrés, 30 localités, N points clés posés, M manquants ».
11. Fichier neuf : `Save()`.

Ensuite, au premier Tick de l'ennemi qui voit le front prêt : `m_Commander.Start()` (officiers, capacité
`SetLimitOfActiveAIs(160)`, stocks). `SRP_TerritoryComponent` construit ses localités depuis le socle
(`GetLocalityCount`, `GetLocality`, `GetKeyPointPos`).

**Arrêt** (ordre des `OnDelete` NON garanti) :
- `SRP_FrontEnemyComponent.OnDelete` : `SRP_FrontComponent front = SRP_FrontComponent.GetInstance(); if (front)
  front.SaveFinalFromEnemy();` (écriture complète, chaîne fe_* et cmd_* comprise), PUIS `Remove(Tick)`,
  désabonnement, `if (m_Commander) m_Commander.Stop();`, `s_Instance = null`.
- `SRP_FrontComponent.OnDelete` : retire Start, Tick, FlushChanges ; `Save()` si `m_bLoaded`. `Save` n'écrit RIEN si
  `m_bEnemyChained` et que l'ennemi n'existe plus (un front.json sans fe_* ni cmd_* ferait repartir le Commandeur à
  neuf : stocks pleins, officiers neufs, interdictions de 24 h effacées, RE13 violé) ; journal ERREUR sauf si
  `m_bEnemyFinalSaved` (l'ennemi a déjà fait la sauvegarde finale).

## 4. Ordres d'appel (appels directs, AUCUN ScriptInvoker)

**Boucle unique de 5 s, `SRP_FrontComponent.Tick`** (serveur, `nowUnix = System.GetUnixTime()`) :
1. `m_Capture.Tick(nowUnix)` — recensement, captures, reprises, états contestés ;
2. `m_ZoneFall.Tick(nowUnix)` — postes, mâts, blocages, puis `EvaluateZone` des zones notées ;
3. `m_Economy.Tick(nowUnix)` — production ;
4. `SRP_FrontRadio.Flush()` — journal FRONT et radio des carrés du passage ;
5. `SRP_FrontMarkers.Tick()` — icônes des points clés, puis `TickEnemyDepots()` ;
6. une fois sur 12 (60 s) : `Housekeeping(nowUnix)` — sauvegarde en retard, copie du jour, `CheckCuts` (E4) et
   rappels, `MigrationTick` (K2), victoire différée.
L'ennemi garde SA boucle de 5 s (`SRP_FrontEnemyComponent.Tick`) : RebuildFront si la version d'état a changé,
PostsTick, AttackClock, AttackTick, `m_Commander.Tick(nowUnix)`, OffensiveTick.

**Après CHAQUE lot de carrés** (`SetCellsOwner`, dans l'appel même) :
`m_Capture.OnOwnersChanged(cells)` -> `m_ZoneFall.OnCellsChanged(cells, owner, reason)` (zones à réévaluer, sauf
repeinte et sauf STAFF) -> `m_Economy.OnCellsChanged(cells, owner, reason)` (dépôt : REPRISE = détruit avec radio ;
STAFF = effacé en silence ; autres causes : rien ici) -> `SRP_BridgeComponent.NoteFrontCell(cell, owner, version)` par
carré -> `SRP_FrontRadio.QueueCell(cell, owner, reason)` par carré ; puis `QueueFlush()` (réseau à l'image suivante),
`MarkDirty(false)`.

**Après un changement de ZONE** (`SRP_ZoneFall.CaptureZone`, `LoseZone`, `ForceZone`) :
`m_Front.SetZoneOwner` -> repeinte éventuelle par `SetCellsOwner` (en silence, `m_bRepainting`) -> localités prises
(`SRP_FrontEnemyComponent.OnLocalityTaken`, prise seulement) -> menace (`AddThreat`, hors Staff) ->
`SRP_MissionManagerComponent.OnZoneCaptured(code, staff)` (prise seulement ; staff : mission ANNULEE, ni prime ni G2)
-> `m_Economy.OnZoneCaptured / OnZoneLost` (dépôt effacé en silence à la perte) ->
`SRP_FrontEnemyComponent.OnZoneCaptured(zone, staff) / OnZoneLost(zone, reason, staff)` (perte : `EndAttack(PERDUE)`
si l'attaque visait la zone ; puis relais Commandeur, qui ne montre rien si staff) -> pont (déjà fait par les
`NoteFrontCell` de la repeinte) -> `SRP_FrontRadio.ZoneFell` (sauf Staff, Q9). ForceZone n'écrit aucun journal : la
seule ligne [STAFF] vient de `SRP_FrontScreens.RunStaff`.

**Saisie D4 réussie** : `SRP_FrontEnemyComponent.OnKeyPointSeized(zone, point, joueur)` AVANT
`m_Front.SetCellOwner(carré, BLEU, SAISIE, nom)`, puis `SRP_FrontRadio.KeyPointSeized`, puis zone à réévaluer.

**Remises à plat** (`ResetCampaign`, `ResetZone`, `RestoreDaily`) : état appliqué d'un coup, puis
`NotifyReset(kind, zone)` -> `m_Capture.OnFrontReset`, `m_ZoneFall.OnFrontReset`, `m_Economy.OnFrontReset`,
`SRP_FrontMarkers.Clear`, `SRP_FrontRadio.ClearQueue`. L'ENNEMI a UNE porte par genre de remise (jamais deux) :
`SRP_EFrontReset.ZONE` -> `SRP_FrontEnemyComponent.OnFrontReset(kind, zone)` (par NotifyReset) ; `CAMPAGNE` ->
`SRP_FrontEnemyComponent.ResetAll(reason)` (par ResetCampaign, NotifyReset ne l'appelle pas) ; `RESTAURATION` ->
`SRP_FrontEnemyComponent.ReadFrom(copie)` puis `OnRestored()` (par RestoreDaily, NotifyReset ne l'appelle pas).
Puis `SRP_BridgeComponent.NoteFrontReset()` ; puis la radio (NewCampaign / Restored) ; puis envoi de l'état et
sauvegarde.

## 5. Schéma de front.json (JsonSaveContext, clés à plat)

Écrit par `SRP_FrontComponent.Save` SEUL (une seule copie datée couvre tout, J2). `N` = index.

| Clés | Contenu |
|---|---|
| `format` | 2 |
| `savedAt`, `campaign`, `campaignStart` | date d'écriture, numéro de campagne, date de début |
| `threat`, `frozen`, `frozenSince` | menace 0-10, gel J3, heure Unix du gel |
| `victory`, `victoryAt` | B3 |
| `bridgeVersion`, `restores` | version du pont (ne redescend jamais), nombre de restaurations |
| `grid`, `cell`, `geom` | côté N, taille du carré, somme de contrôle de la géométrie |
| `cells` | N x N caractères `.` `R` `B`, index gz x N + gx (carrés de base relus toujours bleus) |
| `zones`, puis `zN_code`, `zN_owner`, `zN_captured`, `zN_lost`, `zN_taken`, `zN_cut`, `zN_last` | zones (relues PAR CODE ; code inconnu gardé de côté et signalé) |
| `zN_stock`, `zN_depot`, `zN_dx`, `zN_dy`, `zN_dz` | économie de la zone (G4 à G7) |
| `loc`, puis `lN_name`, `lN_losses`, `lN_lossesAt` | localités (relues PAR NOM), mémoire F12 |
| `lN_sent`, `lN_sentAt`, `lN_bonus`, `lN_bonusAt`, `lN_emptied`, `lN_full` | champs du Commandeur (MA2, MA6, MA7, MA11) |
| `daily`, `dN`, `lastDaily` | copies datées gardées, dernier jour copié |
| `migrated`, `legacyVivres`, `legacyMunitions`, `legacyNext`, `legacyInfo` | K1 fait, restes K2 à verser, prochain essai, résumé |
| `extra`, puis `xN_k`, `xN_v` | extras des autres modules (effacés à la nouvelle campagne) |
| `fe_offDay`, `fe_offText`, `fe_offUnix`, `fe_offZone` | offensive de la nuit (H2), écrits par `SRP_FrontEnemyComponent.WriteTo` |
| `cmd_*` | tout le Commandeur, par la chaîne `SRP_FrontEnemyComponent.WriteTo` -> `SRP_Commander.WriteTo` |

Jamais sauvegardé (H3) : états contestés, progressions de capture et de reprise, drapeaux d'attaque, contre-attaque en
cours, postes du front, postes de commandement et mâts (reposés au démarrage), saisies. Heures en Unix partout.
`ennemi_front.json`, `points_cles.json` et `zones_ressources.txt` n'existent pas.

## 6. Évènements du pont (liste unique)

Tous envoyés par `SRP_FrontRadio` via `SRP_BridgeComponent.PushZoneEvent(type, codeDeZone ou "", texte)` ; le texte
part aussi à la radio et au journal TERRITOIRE (sauf mention). Le bot garde les anciens types (`secteur_pris`,
`secteur_perdu`, `attaque`, `defense`) pour rester compatible, le mod ne les émet plus.

| Type | Émis par | Quand |
|---|---|---|
| `zone_prise` | `ZoneFell` (ZONE_TOMBEE) | D1, zone tombée |
| `zone_perdue` | `ZoneFell` (ZONE_PERDUE, ISOLEMENT) | Q2, E4 |
| `offensive` | `ZoneFell` (OFFENSIVE) si réussite, `NightOffensive` si échec | H2 (bot : style à ajouter à ALERT_STYLES, `"offensive": ("🏳️", RED, None)`, sinon il tombe sur « info ») |
| `zone_attaque` | `AttackAnnounced` | F6, annonce 15 min avant |
| `zone_assaut` | `AttackWave` (1re vague), `AttackBroken` | assaut, rupture à 60 min (C8) |
| `zone_defendue` | `Defended` | F4, deux tiers de pertes (C8) |
| `zone_coupee` | `ZoneCut` | E4 |
| `zone_reliee` | `Reconnected` | E4 |
| `victoire` | `Victory` | B3 |
| `campagne` | `NewCampaign` | B3, K1, Staff |
| `front` | `Frozen`, `Restored` | J3, J2 (remise à plat : le site ne compte pas de prises) |
| `commandement` | `CaptureWithoutMission` (appelé par les missions) | G2 |
| `depot_detruit` | `DepotDestroyed` | G7 |
| `officier_tombe` | `OfficerFell` | VU1, VU3 |
| `region_liberee` | `RegionLiberated` | OF11 |
| `renseignement` | `IntelGained` | VU2, RE9 |
| `depot_ennemi` | `EnemyDepotSabotaged` | RE8 |
| `blinde_ramene` | `ArmorBroughtBack` | C3 |

Radio seule (ni journal public ni évènement) : lignes par carré (`Flush`), `HalfReached`, `CutReminder`,
`AttackWave` après la 1re vague, `SupportSilenced`, `NightNews`. Journal sans évènement : `AttackCalledOff`,
`KeyPointSeized`, `DepotInstalled`, `CaptureAbandoned` (capture abandonnée, menace +1), `LegacyImported`,
`LegacyPoured` (RESSOURCE).
Carrés : `SRP_BridgeComponent.NoteFrontCell(cell, owner, version)` à CHAQUE carré qui change (toute cause) ;
`SRP_BridgeComponent.NoteFrontReset()` APRÈS une nouvelle campagne, une remise de zone ou une restauration.

## 7. Clés de réglages

**Lecteur unique : `SRP_CmdSettings`** (statique, `SRP_CmdSettings.c`). Les clés `front_*` sont dans
`$profile:SimpleRP/front_reglages.txt`, format « clé = valeur   # aide », écrit au premier démarrage avec les valeurs
ci-dessous et complété de lui-même par les clés nouvelles. Défaut = attribut du composant = choix de Jack. Relu au
démarrage et par le bouton Staff « Relire les réglages » (`SRP_FrontComponent.ReloadSettings`). Les réglages de
STRUCTURE (grille, zones, prefabs) et de la carte du joueur restent des attributs seuls.

| Clé | Défaut | Champ | Règle | Relue par (LoadSettings), fichier |
|---|---|---|---|---|
| `front_capture_minutes` | 15 | `SRP_FrontComponent.m_iSquareCaptureMinutes` | C4 (1 pour les essais) | SRP_FrontComponent (SRP_Front.c) |
| `front_capture_redescente` | 1 | `m_fCaptureDecayRatio` | C7 | SRP_FrontComponent (SRP_Front.c) |
| `front_exclusion_minutes` | 5 | `m_iExcludeMinutes` | C3 | SRP_FrontComponent (SRP_Front.c) |
| `front_reprise_secondes` | 60 | `m_iRetakeSeconds` | F3, Q2 | SRP_FrontComponent (SRP_Front.c) |
| `front_garde_point_cle_m` | 100 | `m_fKeyPointGuardRadius` | F5, Q2 | SRP_FrontComponent (SRP_Front.c) |
| `front_saisie_rayon_m` | 100 | `m_fKeyPointClearRadius` | D5 | SRP_FrontComponent (SRP_Front.c) |
| `front_chute_part` | 50 | `m_iZoneFallPercent` | D1 (sur 100) | SRP_FrontComponent (SRP_Front.c) |
| `front_chute_sans_village_part` | 50 | `m_iNoLocalityFallPercent` | Q4 | SRP_FrontComponent (SRP_Front.c) |
| `front_perte_part` | 50 | `m_iZoneLossPercent` | Q2 | SRP_FrontComponent (SRP_Front.c) |
| `front_menace_prise` | 1 | `m_iThreatPerZoneTaken` | G9, Q6 | SRP_FrontComponent (SRP_Front.c) |
| `front_menace_perte` | 1 | `m_iThreatPerZoneLost` | G9, Q6 | SRP_FrontComponent (SRP_Front.c) |
| `front_coupure_minutes` | 1440 | `m_iCutLossMinutes` | E4, Q1 (2 pour les essais) | SRP_FrontComponent (SRP_Front.c) |
| `front_coupure_rappel_minutes` | 60 | `m_iCutReminderMinutes` | E4 | SRP_FrontComponent (SRP_Front.c) |
| `front_sauvegarde_secondes` | 30 | `m_iSaveDelaySeconds` | — | SRP_FrontComponent (SRP_Front.c) |
| `front_copies_jours` | 30 | `m_iDailyKeep` | J2 | SRP_FrontComponent (SRP_Front.c) |
| `front_victoire_minutes` | 0 | `m_iVictoryResetMinutes` | B3 (0 = au redémarrage) | SRP_FrontComponent (SRP_Front.c) |
| `front_prod_vivres` | 10 | `m_iProdVivresPerHour` | G4 | SRP_FrontComponent (SRP_Front.c) |
| `front_prod_munitions` | 6 | `m_iProdMunitionsPerHour` | G4 | SRP_FrontComponent (SRP_Front.c) |
| `front_prod_carburant` | 8 | `m_iProdCarburantPerHour` | G5 | SRP_FrontComponent (SRP_Front.c) |
| `front_prod_pieces` | 6 | `m_iProdPiecesPerHour` | G5 | SRP_FrontComponent (SRP_Front.c) |
| `front_prod_plafond_heures` | 3 | `m_iProductionCapHours` | G4 | SRP_FrontComponent (SRP_Front.c) |
| `front_radio_carres` | 1 | `m_bRadioCells` | I5 | SRP_FrontComponent (SRP_Front.c) |
| `front_radio_regroupe` | 3 | `m_iRadioMergeAbove` | I5 | SRP_FrontComponent (SRP_Front.c) |
| `front_attaque_minutes_menace0` | 180 | `SRP_FrontEnemyComponent.m_iAttackMinutesThreat0` | F6 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_minutes_menace10` | 60 | `m_iAttackMinutesThreat10` | F6 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_alea` | 20 | `m_iAttackJitterPercent` | F6 (sur 100) | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_joueurs` | 4 | `m_iAttackMinPlayers` | H4 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_annonce_minutes` | 15 | `m_iAttackWarningMinutes` | F6 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_vague_minutes` | 5 | `m_iAttackWaveMinutes` | F6 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_vagues` | 3 | `m_iAttackMaxWaves` | vagues prévues, Commandeur gelé OU actif (CA3 : une de moins si la région est désorganisée, puis révision de `ca_vagues_min` à `ca_vagues_max` ; `ca_vagues` supprimée, une clé par chiffre) | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_duree_max_minutes` | 60 | `m_iAttackMaxMinutes` | C8 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_attaque_derniere_prise` | 70 | `m_iAttackLastTakenPercent` | F2 (sur 100) | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_postes_max` | 4 | `m_iPostMaxActive` | Q7 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_postes_part` | 25 | `m_iPostCellPercent` | F8 (sur 100) | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_poste_retour_heures` | 6 | `m_iPostRespawnHours` | F9 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive` | 1 | `m_bFictiveOffensive` | H2 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive_base` | 10 | `m_iOffensiveChanceBase` | Q1 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive_par_menace` | 5 | `m_iOffensiveChancePerThreat` | Q1 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive_max` | 60 | `m_iOffensiveChanceMax` | Q1 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive_heure_debut` | 3 | `m_iOffensiveHourStart` | Q1 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive_heure_fin` | 7 | `m_iOffensiveHourEnd` | Q1 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_offensive_vide_minutes` | 30 | `m_iOffensiveEmptyMinutes` | Q1 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_regarnissage_heures` | 4 | `m_fLossRefillHours` | F12 | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_infiltration_chance` | 15 | `m_iInfiltrationChance` | F10 (sur 100) | SRP_FrontEnemyComponent (SRP_FrontEnemy.c) |
| `front_prime_hameau` | 300 | `SRP_MissionManagerComponent.m_iCapturePrimeHamlet` | G8, Q9 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_prime_village` | 500 | `m_iCapturePrimeVillage` | G8, Q9 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_prime_ville` | 800 | `m_iCapturePrimeTown` | G8, Q9 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_prime_sans_village` | 200 | `m_iCapturePrimeNoPlace` | Q4, Q9 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_captures_max` | 1 | `m_iCaptureMaxLow` | G3 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_captures_max_monde` | 2 | `m_iCaptureMaxHigh` | G3 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_captures_seuil_joueurs` | 10 | `m_iCaptureHighPlayers` | G3 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_capture_rayon_m` | 3000 | `m_fCaptureZoneRadius` | G1 (#50) | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_capture_absence_minutes` | 15 | `m_iCaptureAbsentMinutes` | G1 (#50) | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |
| `front_controle_distance_base_m` | 1000 | `m_fControlMinBaseDistance` | Q10 | SRP_MissionManagerComponent.LoadFrontSettings (SRP_Missions.c) |

Les chiffres du Commandeur (dont `cap_soldats_max` 120, 12 places gardées aux postes Q7, `cap_ia_actives` 160) sont
dans `commandeur_reglages.txt`, déclarés et relus par les classes `SRP_Cmd*`, par le même lecteur.

## 8. Méthodes attendues dans les fichiers EXISTANTS (codées plus tard par leurs propriétaires)

Signatures exactes. « (neuf) » = à ajouter ; « (retouché) » = existe, change de sens ou de signature.

**SRP_CmdSettings.c — `class SRP_CmdSettings` (Commandeur ; le SEUL lecteur de réglages du mod)**
- `static void Declare(string key, string defaultValue, string help)` — clé en minuscules ; `front_*` -> front_reglages.txt, sinon commandeur_reglages.txt ; une clé déjà déclarée est ignorée.
- `static string Reload(string author)` — relit les deux fichiers, ajoute en fin de fichier les clés déclarées absentes (« clé = défaut   # aide »), journalise les clés inconnues et les valeurs illisibles ; rend le compte rendu.
- `static int GetInt(string key)` ; `static float GetFloat(string key)` (virgule acceptée) ; `static bool GetBool(string key)` (1, oui, vrai) ; `static string GetString(string key)` ; `static void GetList(string key, notnull array<string> result)` ; `static int GetIntClamped(string key, int min, int max)` — valeur lue, sinon le défaut déclaré.

**SRP_Commander.c — `class SRP_Commander` (Commandeur ; seules méthodes que le front appelle, via SRP_FrontEnemyComponent)**
- `void SRP_Commander(SRP_FrontEnemyComponent host)` ; `static SRP_Commander Get()` ; `void BuildRegions()` ; `void Start()` ; `bool IsStarted()` ; `void Tick(int nowUnix)` ; `void LoadSettings()` ; `bool IsFrozen()`.
- `void OnControllableDestroyed(notnull SCR_InstigatorContextData data)` ; `void OnZoneCaptured(int zone, bool staff)` ; `void OnZoneLost(int zone, int reason, bool staff)` ; `void OnLocalityTaken(string locality)`.
- `void OnMissionSucceeded(int missionType, vector site, string missionId, string targetPrefab, string author)` ; `bool OnSabotageUsed(IEntity target, int playerId, out string reply)` ; `void OnThreatChanged(int threat)`.
- `void ResetCampaign(string reason)` ; `void OnRestored()` ; `void WriteTo(JsonSaveContext ctx)` ; `void ReadFrom(JsonLoadContext ctx)` ; `string CancelOperations(string author)`.
- Régions pour les écrans : `int GetRegionOfZone(int zone)` ; `string GetRegionLabel(int region)`.
- Statiques utilisées par le territoire : `static int ThreatAt(vector position, int islandThreat)` ; `static float GarrisonFactorAt(vector position)`.

**SRP_CmdScreens.c — `class SRP_CmdIntel` et `class SRP_CmdScreens` (Commandeur)**
- `static void SRP_CmdIntel.OnKeyPointSeized(int zone)` — appelé par `SRP_FrontEnemyComponent.OnKeyPointSeized`.
- `static string SRP_CmdScreens.PCZoneSuffix(int zone)` ; `static string SRP_CmdScreens.PCIntelSection(int playerId)` ; `static string SRP_CmdScreens.StaffHeaderPart()` ; `static string SRP_CmdScreens.StaffTerritoryLine()` — appelés par `SRP_FrontScreens` et `SRP_Admin`.

**SRP_CmdManeuvers.c — `class SRP_CmdManeuvers` (Commandeur ; crochets appelés par SRP_FrontEnemyComponent si `IsCommanderActive()`)**
- `static SRP_CmdManeuvers Get()`. (`IsOn()` est SUPPRIMÉ : la seule garde est `SRP_FrontEnemyComponent.IsCommanderActive()`.)
- `bool CaWindowDecision(int clockSec, int dueSec, int zone, int nowUnix)` ; `int PickByIntel(notnull array<int> candidates)` ; `int PlannedWaves(int zone)` ; `int ReviseWaves(SRP_CounterAttack attack)` ; `int WaveRoomGroups()` ; `bool WantPaperWave(vector origin, vector zoneCenter)` ; `void AddPaperWave(SRP_CounterAttack attack, vector origin, int soldiers, int wave, int axis)` ; `void OnWavePosted(SRP_CounterAttack attack, int wave, array<ref SRP_EnemyGroup> groups, vector originA, vector originB, bool hasB)` ; `void Support(SRP_CounterAttack attack, int nowUnix)` ; `void DriveAssault(SRP_CounterAttack attack, int nowUnix)` ; `bool WaveArrived(SRP_CounterAttack attack)` ; `int CheckEnd(SRP_CounterAttack attack, int nowUnix)` (rend un `SRP_EAttackEnd`) ; `void EndAttack(SRP_CounterAttack attack, int result)` ; `int NightChance(int chance, out bool cancel)`.
- `bool IsLocalityEmptied(string locality)` ; `bool IsKeptFull(string locality)` ; `bool IsOnPlayersPath(int cell)`.
- `int OnTruckLaunched(string source, int load)` (camion HORS colonne : débit AddSent au départ, rend le numéro
  `m_iSentId`) ; `void OnTruckUnloaded(int sentId)` ; `void OnColumnDelivered(int columnId, int survivors, bool joined)`.
- Il appelle `SRP_FrontCapture.SetPaperAssault` / `ClearPaperAssault` et les méthodes F12 de `SRP_FrontEnemyComponent`.

**SRP_CmdSupport.c — `class SRP_CmdSupport` (Commandeur)**
- `static void PlaySoundLocal(string acp, string eventName, vector pos)` (la RPC passe une `string`, pas un `ResourceName`) ; `static void LaunchLocal(RplId shell, vector dir)` — appelées par les RPC de `SRP_FrontEnemyComponent`.

**SRP_CmdCapacity.c — `class SRP_CmdCapacity` (Commandeur)**
- `static SRP_CmdCapacity Get()` ; `string Ask(string key, int cls, int soldiers, int groups, vector position, string owner)` (porte de pose qui note la demande, RE12 : postes `Ask("poste-" + carré, SRP_ECmdCapClass.POSTE, …, "territoire")`, vagues `Ask("vague-" + code, SRP_ECmdCapClass.COMBAT, …)`) puis `static void Tag(SRP_EnemyGroup record, int cls)` ; `void SetWantedGarrisons(notnull array<string> keys, notnull array<int> sizes, notnull array<bool> near, notnull array<bool> attacked)` ; `int GarrisonCap(string locality, int decided)`. (`Refusal` seul ne fait pas la queue : pas pour les poses du front.)

**SRP_Territory.c — `SRP_TerritoryComponent` réduit aux LOCALITÉS et GARNISONS (propriétaire : ennemi)**
- (neuf) `void GetLocalities(notnull array<SRP_SectorState> result)` — remplace `GetStates`.
- (neuf) `void GetEnemyLocalities(notnull array<SRP_SectorState> result)` — remplace `GetEnemySectors` (localités hostiles).
- (gardé) `SRP_SectorState FindState(string name)` ; `bool TryRadioCall(vector position, int now)` ; `string GetRadioReport(vector from, float radius)` ; `void NearestEnemySectors(vector position, int count, SRP_SectorState exclude, out array<SRP_SectorState> result)` ; `void OnMissionSuccess(int type = -1)`.
- (retouché) `int GarrisonSoldiers(SRP_EnemyComponent enemies, SRP_SectorDef sector)` — devient PUBLIC (Nominal : menace LOCALE `SRP_Commander.ThreatAt`, x `SRP_Commander.GarrisonFactorAt`, 60 au plus).
- (neuf) `bool IsGarrisonPosted(string locality)` (vrai même vide) ; `bool IsGarrisonSettled(string locality)` ; `void WithdrawLocality(string locality, string why)` ; `void AdoptAssailants(SRP_SectorState state, array<ref SRP_EnemyGroup> groups)` ; `bool IsInGarrisonedLocality(vector position, float margin)` ; `void SetLocalityOwner(string locality, int owner)` (SRP_ESectorOwner) ; `int CountGarrisonGroupsInZone(int zone)` — utilisés par la façade `SRP_FrontEnemyComponent` (les autres modules passent par la façade).
- (retouché) `void OnCaptureFailed(string zoneCode)` — garnisons des localités de la zone, menace +1 par
  `SRP_FrontComponent.AddThreat`, puis `SRP_FrontRadio.CaptureAbandoned(zone, menace)` SEUL texte (plus de
  Journal/NotifyAll en dur ni du mot « garnison » : déjà fait dans la copie, SRP_Territory.c:2348-2353).
- (retouché) `int GetThreat()` et `void AddThreat(int delta, string reason)` — ne font plus que relayer `SRP_FrontComponent`.
- (retouché) `void ResetAll(string reason)` — garnisons, poteaux et camions seulement (appelé par `SRP_FrontEnemyComponent.ResetAll`).
- Start : attend `SRP_FrontComponent.IsReady()`, puis `BuildLocalities` d'après `GetLocality(i)` et les points clés (`m_vCenter` = CENTRE, `m_vHQ` = QG) ; `m_iOwner` = ENNEMI tant que la localité n'est pas prise (`SRP_ZoneFall.IsLocalityTaken`).
- Pose d'une garnison (même vide par F12) : `SRP_FrontComponent.GetInstance().GetZoneFall().OnGarrisonPosted(nom)` ; retrait : `OnGarrisonGone(nom)` ; avant chaque DeleteAll : `SRP_FrontEnemyComponent.AddLosses(nom, LiveLosses(state))`.
- **Pose et complément (l.886-891, l.926) : UNE formule.** Décidé = `SRP_FrontEnemyComponent.GetInstance()
  .GetLocalityEffective(nom)` (Nominal − pertes − envoyés + reçus : F12, MA2, MA11 compris), passé à
  `SRP_EnemyGarrison`, qui SEUL applique `Math.Min(décidé, SRP_CmdCapacity.Get().GarrisonCap(nom, décidé))` ; rien si
  `SRP_CmdManeuvers.IsLocalityEmptied(nom)` (MA7). La formule `GarrisonSoldiers − CurrentLosses` de la consigne
  `par_fichier/SRP_Territory.c.md` (l.30-32) est CADUQUE.
- `GarrisonTrucksTick` : `SRP_FrontEnemyComponent fe = SRP_FrontEnemyComponent.GetInstance(); if (fe &&
  fe.IsCommanderActive())` -> camion d'entrée par `SRP_CmdManeuvers.Get().RequestEntryTruck`, pas de camion d'alerte ;
  sinon, règles d'aujourd'hui.
- `SRP_SectorDef` (neuf) : `vector m_vHQ;` `bool m_bHasHQ;` `int m_iZone = -1;` — `SRP_SectorState` (neuf) : `string m_sZoneCode;` `bool m_bRetreatDone;`.
- À RETIRER pour compiler : `SRP_SectorComponent` et sa classe, `SRP_DepotInstallAction` (FAIT dans la copie le 26/09 : la classe vit dans SRP_FrontEconomy.c), `IsInsideEnemySector`, `GetStates`, `GetEnemySectors`, `CountOurs`, `ForceOwner`, `ForceAttack`, `InstallDepot`, `ForceDepot`, `RemoveDepot`, `GetStatusText`, `Save` / `Load` v1, `Capture`, `Lose`, `UpdateBlink`, `PlaceMarker`, `OurSectorAt`, contre-attaques par secteur, production ; attributs m_iCaptureMinutes, m_iLoseMinutes, m_iCaptureReward, marqueurs.

**SRP_Enemy.c**
- `SRP_EnemyGroup` (neuf) : `int m_iAssaultZone = -1;` (posé par l'ennemi du front sur chaque groupe d'assaut, remis à -1 avant RetireLater / ConvertToGarrison).
- MaybeCallHeli (G12) : `SRP_FrontComponent.GetInstance().IsRedAt(alert.m_vPosition)` remplace `IsInsideEnemySector`.
- Tick ambiant et poses (F10) : `SRP_FrontEnemyComponent.GetInstance().AllowAmbientFor(player)` et `AcceptAmbientSpawn(position, infiltration)`.
- RetireTick : SEULE porte de retour des soldats envoyés retirés vivants : groupe dont `m_sSourceLocality` n'est pas
  vide et `SRP_CmdManeuvers.Get().m_bReturnSurvivors` (clé renfort_rendre_survivants ; sans Commandeur : vrai) ->
  `SRP_FrontEnemyComponent.ReturnSent(source, vivants)`. Ni `ContactGroupsTick` ni `OnColumnDelivered` ne rendent quoi
  que ce soit.
- Utilisés tels quels (vérifiés publics) : `GetGroups()`, `InGrace(int)`, `GetActivePlayers()`, `ScaleGroupsNear`, `LocalRoom`, `SpawnWaveFrom`, `SpawnGroup`, `ApplyRole`, `NoteHome`, `RetireLater`, `DeleteAll`, `ConvertToGarrison`, `OrderMove`, `OrderSearch`, `static GetMembers`, `static ActiveAgentsNear`, `static IsSettled`, `PickPrefabOfSize`, `GetSectionPrefabs`, `GetSentryPrefabs`, `static Journal`, `FindRecord`.

**SRP_EnemyGarrison.c**
- (neuf) `static int CountPlanSurvivors(SRP_SectorState state)`.
- (retouché) `protected static void BuildPlan(SRP_EnemyComponent enemies, string kind, int wanted, bool topUp, bool hasCommand, bool twoKeys, array<ref SRP_GarrisonSlot> plan)` — ville : QG posé au point QG (`m_vHQ`), la plus grande section au centre.
- Aucune garnison pour une localité prise (`SRP_ZoneFall.IsLocalityTaken`).

**SRP_EnemyTrucks.c** — départs au rouge (`m_bOriginRedOnly`, `SRP_FrontComponent.IsRedAt`) ; `territory.GetLocalities` au lieu de `GetStates`. (Réserves globales supprimées : la capacité du Commandeur les remplace.)
**SRP_HeliSearch.c** — `EnemySectorDirection` remplacée par `SRP_FrontComponent.GetInstance().RedDirection(center, direction)` ; `GetEnemyLocalities`.
**SRP_Sirene.c** — `territory.GetLocalities(states)` dans GetAlertReport.
**SRP_EnemyAwareness.c** — `IsFree` : `if (record.m_sHomeTask == "poste front") return false;` ; TaskLabel « poste front ».

**SRP_Missions.c — `SRP_MissionManagerComponent` (propriétaire : missions ; SEUL payeur de la prime G8 et de l'alerte G2)**
- (retouché) `bool HasActiveCapture(string zoneCode)` ; `SRP_Mission FindCapture(string zoneCode)` — clé = code de zone.
- (retouché) `SRP_Mission CreateCapture(string zoneText, int chiefId, string author, out string refusal)` — `SRP_FrontComponent.FindZone`, refus : à nous, base, gelé, pas au contact (`IsZoneInContact`, J4), plafond G3 ; site = `GetMissionPoint(zone)` ; Q9 : après `NewMission` (qui tire la difficulté au hasard, SRP_Missions.c:849), `mission.m_iDifficulty = CaptureDifficulty(zone)` puis `m_iRewardEuros = m_iRewardEurosBase x difficulté` et `m_iRewardPackets = m_iRewardPacketsBase x difficulté` recalculés (l.857-858) ; les gardes suivent cette difficulté.
- (neuf) `int CaptureDifficulty(int zone)` — Q9 : `SRP_FrontZone.m_iKind` AUCUNE ou HAMEAU -> 1, VILLAGE -> 2, VILLE -> 3 (if/else).
- (neuf) `int GetCaptureTargets(notnull array<int> zones)` — zones au contact, non visées, rien si gelé (J4 ; source du tableau, de « Ordonner » et du pont).
- (neuf, remplace OnSectorCaptured) `void OnZoneCaptured(string zoneCode, bool staff)` — **staff (Q9, correction)** :
  ni prime, ni `CaptureWithoutMission` ; une mission active sur la zone -> `End(mission, SRP_EMissionState.ANNULEE,
  "zone forcée par le Staff")` (End ne paie qu'en SUCCES et ne pousse d'évènement qu'en SUCCES ou ECHEC : aucune
  récompense, aucun PushEvent, aucun OnMissionSucceeded ; seule reste la ligne « Mission … annulée » de fin de mission
  du tableau, pas une annonce de zone). **Hors staff** : mission active -> prime `CapturePrime(zone)` puis
  `End(mission, SUCCES, …)` ; sans mission -> `SRP_FrontRadio.CaptureWithoutMission(zone)` (G2). La consigne
  `par_fichier/SRP_Missions.c.md` l.82-84 (`authorised = staff || mission`, prime payée même au Staff, PushEvent
  direct) est CADUQUE.
- (neuf) `int CapturePrime(int zone)` ; `int CountActiveOfType(int type)` ; `int CaptureLimit()` ; `int GetCaptureMaxHigh()` ; `int GetCaptureHighPlayers()` ; `void DeclareFrontSettings()` ; `void LoadFrontSettings()`.
- End(SUCCES) : `SRP_FrontEnemyComponent.GetInstance().OnMissionSucceeded(mission.m_iType, mission.m_vSite, mission.m_sId, mission.m_sTargetPrefab, author)` ; G9 : `OnMissionSuccess` seulement si le type n'est pas LIBERER.
- En tête de `static string OnObjectiveUsed(IEntity entity, int playerId)` : `string reply; if (fe && fe.OnSabotageUsed(entity, playerId, reply)) return reply;` — SEULE entrée du sabotage vers le Commandeur ; `SRP_SabotageAction.PerformAction` (l.3136-3144) ne change PAS (il appelle déjà OnObjectiveUsed) : la consigne `par_fichier/SRP_Missions.c.md` l.101-103 (appel direct `SRP_CmdSupport.GetInstance().OnSabotage` avant OnObjectiveUsed) est CADUQUE.
- CheckControl (G10, Q10) : `IsBlueAt` ; `m_fControlMinBaseDistance` = 1000 ; SpawnPendingGuards (G11) : `SRP_FrontEnemyComponent.IsInGarrisonedLocality(site, marge)`.
**SRP_MissionRequests.c** — zones par code (`SRP_FrontComponent.FindZone`, `GetZoneLabel`) ; listes par `GetCaptureTargets` ; `CreateCapture(…, refusal)`.

**SRP_Bridge.c — `SRP_BridgeComponent` (propriétaire : web)**
- (neuf) `static void NoteFrontCell(int cell, int owner, int version)` ; `static void NoteFrontReset()` ; `static void PushZoneEvent(string type, string zoneCode, string text)` — `PushEvent(type, text)` devient `PushZoneEvent(type, "", text)`.
- BuildState : `territory{ours, total, threat, blue, land, frozen, campaign}` (`CountZonesOurs`, `CountZonesTotal`, `GetThreat`, `CountBlueCells`, `CountLandCells`, `IsFrozen`, `GetCampaignId`) ; bloc `front` (`GetBridgeVersion`, `GetRestoreCount`, `GetCellsString`, `GetZoneMapString`, `GetZone(i).Get*` — clé `f` = `IsNearFront()`, ex « IsAtFront » de la consigne `par_fichier/SRP_Bridge.c.md` l.72, `a` = `GetAttackState()` de la zone, `k` = `IsCut()` —, `GetContestedCells`, `CellRef`) ; `boxes` par `GetEconomy().GetBoxes` ; `capture_targets` = [{c, l}] par `GetCaptureTargets`. Ne lit plus rien de SRP_TerritoryComponent.

**SRP_Admin.c — `SRP_AdminComponent` (propriétaire : carte)**
- (neuf) `bool IsGod(int playerId)` — `m_GodPlayers.Contains(playerId)`.
- `Teleport` : première ligne `SRP_FrontCapture.NoteTeleport(playerId);` (C3).
- Onglet territoire délégué : `SRP_FrontScreens.StaffTab`, `RunStaff` (téléporte si `wantTeleport`), `IsInternalPage`, `NeedsConfirm`, `PageAfter` ; Header : `SRP_FrontScreens.StaffHeaderPart()` + `SRP_CmdScreens.StaffHeaderPart()` ; page Rapide : `SRP_FrontScreens.CellHere`.
- Danger « territoire » et wipe : `SRP_FrontComponent.GetInstance().NewCampaign(author)` ; ia-reset et wipe : `SRP_Commander.Get().CancelOperations(author)` ; bouton « Relire les réglages » : `SRP_FrontComponent.GetInstance().ReloadSettings(author)`.
**SRP_PC.c** — `protected static string Territoire(int playerId)` rend `SRP_FrontScreens.PCTerritoire(playerId)` (appel l.357 : `Territoire(playerId)`).
**SRP_ChatCommands.c** — bloc mort `/territoire` retiré (ou `SRP_FrontScreens.StatusPlain()`), WhereInGame : « ordinateur (PC), onglet Territoire, et la carte (M) ».
**SRP_Discord.c** — `IsChatty` : FRONT ; route FRONT (`m_bFrontSquaresPublic` 0, `m_bFrontSquaresStaff` 1) ; COMMANDEUR -> flux etat-major ; couleurs (« reliée », « coupée », « victoire », « nouvelle campagne », « gelé », « ramené », « officier ennemi », « renseignement », « dépôt ennemi ») ; mots confidentiels + « aucune zone », « tirage ».
**SRP_JournalComponent.c** — la catégorie COMMANDEUR n'entre pas dans `s_aRecent`.
**SRP_ResourceManagerComponent.c** — (neuf) `bool IsRestockDone()` + `protected bool m_bRestockDone` (vrai en fin de RestockBatch ou sans dépôt) ; `int Add(int resource, int amount, string reason, bool log = true)` inchangé (K2).
**SRP_Civilians.c** — G14 : `SRP_FrontComponent.IsBlueAt` / `IsRedAt` ; (Commandeur) `void GetInformantCandidates(notnull array<IEntity> characters)`.
**SRP_Delivery.c** — `m_bPreferBlue` (`IsBlueAt`) ; menace lue sur `SRP_FrontComponent.GetThreat()`.

## 9. Prefabs

- `Prefabs/MP/Modes/Plain/SRP_GameMode.et` : ajouter `SRP_FrontComponent "{6A5B0C0D0E0FF010}" { m_sCPPrefab "<GUID>Prefabs/Props/SRP_PosteCommandement.et" m_sColoursPrefab "<GUID>Prefabs/Props/SRP_MatCouleurs.et" }` et `SRP_FrontEnemyComponent "{6A5B0C0D0E0FF004}" { }` (GUID à vérifier uniques) ; vider le bloc `SRP_TerritoryComponent '{6A5B068C2E0AB2B2}' { }` (m_iCaptureMinutes 2 et m_iLoseMinutes 15 surtout). Aucun autre composant neuf.
- `Prefabs/Front/SRP_PointCle_Centre.et` et `SRP_PointCle_QG.et` : `SRP_KeyPointComponent { m_sLocality "…" m_eRole 0 ou 1 }`, maillage de drapeau visible, 30 centres + un QG par ville ou bourg.
- `Prefabs/Props/SRP_PosteCommandement.et` : RplComponent, `SRP_PosteCommandementComponent`, ActionsManager avec `SRP_SaisiePCAction` (contexte « pc », Duration 60), destruction désactivée.
- `Prefabs/Props/SRP_MatCouleurs.et` : mât tricolore, RplComponent, aucune action.
- `Prefabs/Paquets/SRP_KitDepot.et` : inchangé (nom de classe `SRP_DepotInstallAction` gardé).

## 10. Points délicats

1. **Compilation d'ensemble** : `SRP_DepotInstallAction` n'est plus déclarée qu'une fois (SRP_FrontEconomy.c, corps
   écrit : il appelle `GetEconomy().InstallDepot`) ; la copie de SRP_Territory.c ne la contient plus. Le contrôleur
   statique ne relève plus rien sur tout le dossier.
2. **Deux fichiers pour un lecteur** : `SRP_CmdSettings.Declare` doit router les clés `front_*` vers
   `front_reglages.txt`. S'il ne le fait pas, les clés du front finissent dans `commandeur_reglages.txt` : rien ne
   casse, mais Jack les cherchera au mauvais endroit.
3. **Q8 l'emporte sur la relecture n°13** : aucun outil « Point clé ICI » ni `points_cles.json` ; les consignes
   SRP_Admin (page `territoire:pc` avec `pc-centre` / `pc-qg`) et SRP_ZoneFall (SetKeyPointHere, SaveKeyPoints) sont
   caduques. La page Staff « Points clés » ne fait que lister (repère, fichier, MANQUANT) et agir sur les postes.
4. **RPC du son** : paramètre `string` au lieu de `ResourceName` ; `SRP_CmdSupport.PlaySoundLocal` doit l'accepter.
5. **Attributs publics** sur les deux composants : les classes tenues les lisent directement ; ne jamais les écrire
   ailleurs que dans `LoadSettings`.
6. **`GetZoneName` rend le nom seul** (« Régina ») ; plusieurs consignes du Commandeur l'attendaient au sens de
   « Régina (S07) » : utiliser `GetZoneLabel`.
7. **Réseau** : géométrie + état d'environ 5 à 6 Ko par arrivant (RplSave) ; `RplLoad` doit refuser les tailles
   aberrantes (N <= 256, zones <= 126, points <= 256) ; le Broadcast ne s'exécute pas sur l'hôte, d'où
   `m_iStateVersion++` dans FlushChanges.
8. **Ordres figés** (boucle, lot, zone, remise) : tout module qui ajoute un appel le place dans ces chaînes, jamais
   par un ScriptInvoker.
9. **Heures Unix** pour tout ce qui est sauvegardé ou dure plus qu'une session (E4, F2, F9, F12, H2, Commandeur) ;
   GetTickCount seulement pour les mesures internes non sauvegardées (saisie D4, verrou de coup, envoi C8).
10. **Classes moddées** : `SCR_MapEntity` (OnMapOpen, OnMapClose, UpdateMap, vérifiées) et
    `SCR_CharacterDamageManagerComponent.OnDamage` (vérifiée, l.2033) ; toujours appeler `super`.
