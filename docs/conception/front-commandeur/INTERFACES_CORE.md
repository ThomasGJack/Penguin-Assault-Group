# FRONT — décisions de cohérence (font foi)
## Répartition
1. SOCLE (modules grille et migration) : SRP_Front.c et SRP_FrontGeometry.c, avec la classe SRP_FrontComponent présente sur toutes les machines.
- C'est la seule source de vérité.
- Énumérations uniques : SRP_EFrontOwner, SRP_EFrontLive, SRP_EFrontReason, SRP_EKeyPointRole.
- Il tient :
  • la grille et les zones (SRP_FrontZone) ;
  • les localités, lues dans SRP_CivilZoneComponent ;
  • le registre des points clés (points_cles.json, plus les lignes « point ») ;
  • les propriétaires et les états contestés ;
  • la réplication et les versions (réplication, version croissante pour le pont, compteur de restaurations) ;
  • la sauvegarde unique front.json : pertes F12, date de H2, stock, dépôt et ressource des zones, heure de coupure ;
  • les copies datées, le gel et la menace (SRP_TerritoryComponent.GetThreat et AddThreat ne font plus que relayer) ;
  • la victoire, et E4 par zone ;
  • les primitives du Staff : ForceCellAt, ResetZone, NewCampaign, RestoreDaily ;
  • K1 et K2, avec le code du module migration et IsRestockDone.
- Une seule boucle de 5 s appelle, dans l'ordre : capture, zones, économie, radio, marqueurs.
- Après chaque lot de carrés, appels directs dans l'ordre : capture, zones, économie, pont, radio. Aucun ScriptInvoker.

2. CAPTURE : SRP_FrontCapture.c, classe tenue par le socle.
- Seul recensement des joueurs valides (C2, C3) et des ennemis en état de combattre (C6).
- Capture C4, C5, C7 : elle vérifie CanBlueCapture et zones.IsCellSeizeOnly.
- Reprise F3 : seulement par les groupes marqués m_iAssaultZone, dans la zone visée, sur un carré qui touche du rouge.
- F5 sur le carré du point clé, et les états C8.
- Services : CountValidPlayersNear et InCell, IsPlayerCounted, CountFitEnemiesNear et InCell, CountFitAssaultersInZone.
- Elle ne fait ni E4, ni annonces, ni règle propre pour Erquy.

3. ZONES : SRP_ZoneFall.c.
- Postes de commandement (table), action de saisie D4, mât tricolore, règle D1 et règle de perte.
- Seules portes pour changer une zone de camp : CaptureZone, LoseZone(zone, raison, repeindre), et ForceZone pour le Staff (sans prime ni menace).
- Menace +1 ou −1 (G9).
- Appelle missions.OnZoneCaptured(clé, staff), l'économie, l'ennemi (OnZoneCaptured, OnZoneLost, OnLocalityTaken) et la radio.
- Outil Staff « Point clé ICI », qui écrit dans le registre du socle.

4. ENNEMI : SRP_FrontEnemyComponent, plus la partie localités de SRP_Territory.c et les fichiers SRP_EnemyGarrison, EnemyTrucks, Enemy, HeliSearch, Sirene et EnemyAwareness.
- Garnisons F11, pertes F12 (données gardées par le socle), postes F7 à F9, patrouilles F10.
- Contre-attaques F1, F2, F4, F6, H4 : pose m_iAssaultZone et SetZoneFlag, déplace les groupes, −1 de menace pour une défense réussie.
- H2 passe par zones.LoseZone ; hélicoptère G12.
- Fournit : IsGarrisonPosted, IsGarrisonSettled, OnLocalityTaken, AdoptAssailants, IsInGarrisonedLocality, ForceAttack, CancelAttack, ForceOffensive et les rapports.

5. MISSIONS : SRP_Missions.c, SRP_MissionRequests.c, SRP_FrontEconomy.c, SRP_Civilians.c, SRP_Delivery.c.
- Captures G1, G3, J4 : CreateCapture(…, out refusal) et GetCaptureTargets.
- Seul module qui paie la prime G8 et envoie l'alerte G2 ; filtre G9 dans End.
- G10, G11, G13, G14.
- Production et dépôt G4 à G7 : données sur SRP_FrontZone, ressources lues dans front_retouches.txt.
- Ne fait pas K2.

6. CARTE : SRP_FrontMap.c, SRP_FrontRadio.c, SRP_FrontScreens.c, la partie territoire de SRP_Admin.c, SRP_PC.c, SRP_ChatCommands.c.
- Calque I1 à I3 et icônes des points clés.
- SEUL auteur des textes du front : radio, journal TERRITOIRE et FRONT, PushEvent.
- Onglet PC et menus Staff, dont la page des points clés, les commandes de l'ennemi et le dépôt.

7. WEB : SRP_Bridge.c, SRP_Discord.c, le bot et le site.
- Protocole du pont (#front), liste unique des évènements, routage Discord.
- capture_targets pour /ordonner.
- Archives K1 côté bot, déclenchées par le numéro de campagne.
- I6 à I9 : image chaque soir, photo chaque jour.

8. MIGRATION : organisation de la livraison.
- Ordre de codage : le socle compilé seul d'abord.
- Nettoyage : SRP_Territory.c garde localités et garnisons ; les appels renommés font signaler chaque oubli par le compilateur.
- Prefab : SRP_TerritoryComponent vidé mais gardé ; deux composants neufs seulement, SRP_FrontComponent et SRP_FrontEnemyComponent.
- Contrôles, check-list d'essai et guide.
## Trous tranchés
- 1. [Bloquant] Le cœur porte quatre noms. SRP_FrontComponent (grille, capture, zones, carte côté serveur, web), SRP_FrontGridComponent (ennemi), SRP_FrontGrid.GetLocal() (carte côté joueur), façade IsRedAt/ZoneAt/GetZones/FindZone sur SRP_TerritoryComponent (missions, migration). La classe de zone a deux noms aux champs différents : SRP_FrontZone (grille, web : m_sCode, m_iKind) et SRP_ZoneState (missions, migration : m_sKey, m_sPlaceKind, m_vSite). TRANCHÉ : SRP_FrontComponent + SRP_FrontZone, présents sur toutes les machines. SRP_TerritoryComponent ne garde que les localités et les garnisons.
- 2. [Bloquant] Une même fonction sous cinq noms.
- Carré sous une position : CellIndexAt (grille, carte) / CellAt (zones, ennemi, missions).
- Zone d'un carré : GetCellZone / GetZoneOf (capture) / ZoneOfCell (zones, ennemi) / GetZoneIndexOfCell (web).
- Propriétaire : GetCellOwner / GetOwner (capture, ennemi).
- Carré en jeu : IsCellInPlay / IsLand.
- Taille : GetGridSize / GetCols + GetCellCount / GridSize.
- Version : GetStateVersion (grille) / GetRevision (ennemi) / GetVersion (carte, web).
Fonctions demandées mais absentes de la grille : IsRedAt, IsBlueAt, CellCenter, Neighbour(cell, dir), IsInBaseZone(pos).
TRANCHÉ : la liste de la grille fait foi, on y ajoute ces 5 fonctions, et chaque module reprend ces noms.
- 3. [Bloquant] Le sens de « au front » est flou.
- IsZoneInContact (grille) = zone rouge qui touche du bleu (E1).
- IsZoneOnFront (missions) = la même chose sous un autre nom.
- IsZoneAtFront (grille) = zone bleue qui touche du rouge (F1), mais la carte s'en sert pour le filtre I3 au sens de E1.
La zone protégée a cinq noms : m_bShield, IsProtectedCell, IsLossProtected, IsBaseNeighbour, IsProtected. La base en a quatre : zone 0, IsBaseCell, IsBaseZone, IsZoneBase.
TRANCHÉ : IsZoneInContact (rouge : E1, J4, F11), IsZoneAtFront (bleue : F1), IsZoneProtected (B2 + B4), IsBaseZone, IsBaseCell. Le filtre I3 prend l'union des deux.
- 4. [Bloquant, ne compile pas] Déclarations en double.
- enum SRP_EFrontLive est déclaré dans SRP_Front.c (grille) ET dans SRP_FrontCapture.c (capture).
- SRP_EFrontCause est déclaré par la carte (COMBAT, CONTRE_ATTAQUE, HORS_LIGNE…), alors que la capture utilise CAPTURE, REPRISE, COUPURE et que la grille a SRP_EFrontReason (ZONE_TOMBEE, ISOLEMENT…).
- Rôle d'un point clé : SRP_EKeyPointRole (grille) / SRP_EKeyRole (zones).
- Propriétaire : SRP_EFrontOwner ROUGE/BLEU / SRP_ECellOwner (ennemi) / SRP_ESectorOwner ENNEMI/NOUS (zones, missions).
TRANCHÉ : un seul jeu d'énumérations dans SRP_Front.c : SRP_EFrontOwner, SRP_EFrontLive, SRP_EFrontReason (fusion des trois listes) et SRP_EKeyPointRole. SRP_ESectorOwner reste pour les localités.
- 5. [Bloquant] Six façons d'écrire un carré ou une zone.
- Un carré : SetCellOwner(cell, owner, reason, author) (grille) / ApplyCellChanges(cells, owners, cause) (capture) / SetCellOwner(cell, owner, string, bool announce) (zones) / RetakeCell (ennemi) / SetSquareOwner (migration) / ForceCell (carte).
- Une zone : SetZoneOwner à 5 paramètres (grille), à 3 avec une date (zones), bool SetZoneOwner(z, owner, string) (ennemi, pour H2, qui suppose que dépôt, stock et menace suivent), ForceZoneOwner (migration).
TRANCHÉ : SetCellOwner et SetCellsOwner de la grille sont la seule porte bas niveau. Tout changement de ZONE passe par SRP_ZoneFall : CaptureZone, LoseZone(zone, raison, repeindre) ou ForceZone. Cela vaut aussi pour E4, H2 et le Staff.
- 6. [Élevé] Les notifications de changement ne se branchent pas.
- La grille publie des ScriptInvoker, dont la signature n'est pas vérifiable dans les scripts extraits.
- La capture attend OnOwnersChanged(cells) et OnFrozenChanged(bool).
- Les zones attendent OnCellChanged(cell, ancien, nouveau).
- L'économie attend OnCellChanged(cell, owner).
- Le pont attend NoteFrontCell(cell, owner, version) et NoteFrontReset().
- La radio attend QueueCell.
TRANCHÉ : pas d'invoker. Après chaque lot, SRP_FrontComponent appelle directement, toujours dans cet ordre : capture, zones, économie, pont, radio.
- 7. [Élevé] Personne ne fait tourner l'économie.
La capture a sa boucle de 5 s (départ à 6 s), les zones la leur (départ à 7 s), l'ennemi la sienne. L'économie doit être « appelée par le Tick du cœur », mais la grille n'a qu'un entretien toutes les 60 s. Radio.Flush et Markers.Tick doivent être « appelés par les modules en fin de passage », sans que ce soit précisé.
TRANCHÉ : une seule boucle de 5 s dans SRP_FrontComponent, qui appelle dans l'ordre capture, zones, économie, radio, marqueurs. L'ennemi garde sa boucle.
- 8. [Élevé] E4 est codé trois fois.
- Grille : par zone, avec m_iCutSince et CheckCuts toutes les 60 s.
- Capture : par carré, avec SetCutSince, son propre parcours, CheckCuts, ses annonces et PushEvent secteur_perdu.
- Zones : LoseZone avec repeinte.
Au dégel, la grille décale l'heure de coupure, la capture relance 24 h pleines.
TRANCHÉ : la grille seule, par zone (la réponse E4 parle d'une zone). La perte passe par zones.LoseZone(ISOLEMENT, repeindre). Au dégel, l'heure est décalée de la durée du gel.
- 9. [Élevé] Erquy est traité de deux façons.
- Grille : liaison maritime virtuelle, qui compte pour C1, E1 et E4.
- Capture : débarquement libre tant que l'île n'a aucun carré bleu (m_bIslandLanding), et îles jamais coupées.
TRANCHÉ : la liaison maritime de la grille, qui respecte C1 et E6. La capture appelle CanBlueCapture et perd ses deux réglages. Sous réserve de la question à Jack.
- 10. [Élevé] Soldats et joueurs sont comptés trois fois.
- Ennemis en état de combattre : CountFitEnemiesNear (capture), FitEnemiesInCell/FitEnemiesNear avec son propre recensement par passage (ennemi), CountFitGroundEnemies (zones).
- Joueurs valides : CountValidPlayersNear (capture). Les zones attendent front.PlayerCounts(id), l'ennemi attend capture.HasDefenderInCell et HasDefenderNear, qui n'existent pas.
TRANCHÉ : la capture fait le seul recensement de chaque passage. Elle expose CountFitEnemiesNear, CountFitEnemiesInCell, CountFitAssaultersInZone, CountValidPlayersNear, CountValidPlayersInCell et IsPlayerCounted.
- 11. [Élevé] Reprise F3 et protection F5 en double, marquage absent.
- F3 est codé deux fois : la capture (m_aRetakeMs, groupes marqués par m_iAssaultZone) et l'ennemi (AttackTick avec m_mRetakeMs, puis capture.RetakeCell).
- F5 est codé trois fois : capture (le carré du point clé), zones (IsKeyCellHeld), ennemi (tous les carrés à 100 m du point clé).
- Trou : l'ennemi ne pose jamais m_iAssaultZone, dont la capture dépend, ni sur les débarqués des camions de vague. Il ne pose pas non plus SetZoneFlag (attaque annoncée ou en cours), dont dépendent la carte et le site.
TRANCHÉ : le chronomètre de reprise et F5 vont à la capture, sur le seul carré du point clé. L'ennemi pose m_iAssaultZone et le remet à -1 avant RetireLater ou ConvertToGarrison. Il pose aussi SetZoneFlag et ne fait que déplacer ses groupes.
- 12. [Élevé] La capture et les points clés ne se parlent pas.
- Les zones exigent que la capture appelle IsCellSeizeOnly(cell) avant toute prise par présence : l'algorithme de la capture ne l'appelle nulle part.
- La capture attend IsCellAwaitingGarrison, que personne ne fournit.
TRANCHÉ : la capture appelle zones.IsCellSeizeOnly. IsCellAwaitingGarrison est supprimé : la saisie exige déjà une garnison posée et installée.
- 13. [Élevé] Trois registres de points clés.
- Grille : prefab SRP_KeyPointComponent, lignes « point » du fichier de retouches, m_bTaken sauvegardé.
- Zones : outil Staff « Point clé ICI », points_cles.json, orientation, QG automatique à 150 m.
- Ennemi : m_vKeyPoint2 et m_bHasKeyPoint2 ; les zones appellent le même champ m_vHQ.
Sept façons de le lire : GetKeyPoint(k).m_vPos, GetKeyPoint(zone, k), GetKeyPointPos, ZoneKeyPoint, GetZoneSite, GetMissionPoint, m_vSite.
QG absent : rien selon la grille, QG automatique selon les zones.
TRANCHÉ :
- un seul registre, dans le cœur : points_cles.json écrit par l'outil Staff, lignes « point » acceptées ;
- pas de prefab de repère ;
- un point clé est pris quand son carré est bleu (plus de m_bTaken) ;
- un seul champ, m_vHQ ;
- une seule API : GetKeyPointCount, Pos, Role et Cell(zone, k), plus GetMissionPoint(zone) ;
- pas de QG automatique.
- 14. [Bloquant, bugs silencieux] La chute d'une zone est payée et comptée deux fois.
- La prime G8 est versée par zones.CaptureZone ET par missions.OnZoneCaptured.
- L'alerte G2 « sans mission » part deux fois.
- Le −1 de menace est revendiqué par zones (LoseZone) et par l'ennemi (dépendance « OnZoneLost… c'est lui qui applique la menace −1 »).
- Le filtre G9 est posé à deux endroits : End dans missions, OnMissionSuccess dans zones et migration.
- Les signatures divergent : OnZoneCaptured(SRP_ZoneState, bool) ou OnZoneCaptured(string key) ; CreateCapture avec ou sans « out string refusal ».
TRANCHÉ :
- zones = menace +1 ou −1, hors Staff ;
- missions = prime, alerte G2 et fin de mission, via OnZoneCaptured(string zoneKey, bool staff) ;
- filtre G9 dans End seulement ;
- CreateCapture(zoneText, chiefId, author, out string refusal).
- 15. [Élevé] Les annonces sont faites trois fois.
- Chaque carré est annoncé par la capture (AnnounceCells), par la radio de la carte (QueueCell/Flush) et écrit au journal FRONT par la grille (SetCellOwner).
- Chaque zone est annoncée par zones (CaptureZone/LoseZone), la carte (ZoneFell), la grille (coupure, victoire), la capture (perte par coupure) et l'ennemi (attaque, défense, offensive).
Les types d'évènements envoyés au bot ne concordent pas :
- secteur_pris / secteur_perdu (zones, migration, carte) ;
- zone_prise, zone_perdue, zone_attaque, zone_assaut… (web) ;
- zone_coupee, front_campagne, front_restaure, front_gel (grille) ;
- defense (migration) contre zone_defendue (ennemi, web).
TRANCHÉ : SRP_FrontRadio (carte) est le seul à écrire les textes du front, les lignes de journal et les PushEvent. Liste des évènements : celle du web, plus depot_detruit, offensive et commandement. Le bot garde les anciens types pour rester compatible.
- 16. [Élevé] La sauvegarde est éclatée.
- F12 : front.json selon la grille (m_iLosses), ennemi_front.json selon l'ennemi.
- Date de H2 : un extra de front.json, ennemi_front.json ou la clé lastOffensive.
- Stock et dépôt : champs de SRP_FrontZone (grille) ou SRP_ZoneEconomy.WriteTo (missions).
- Ressource d'une zone : m_sResource, m_iResource ou m_iType SRP_ESectorType.
- Délai d'enregistrement : 60 s (grille) ou 30 s (migration).
- Copies datées : front_jours/ (grille) ou archives/ (migration).
TRANCHÉ : tout va dans front.json, écrit par le cœur, pour qu'une seule copie datée couvre tout (J2). Les champs sont sur SRP_FrontZone et la ressource est un SRP_EResource.
- 17. [Moyen] Deux fichiers pour G5 : ligne « ressource S12 carburant » dans front_retouches.txt (grille) contre zones_ressources.txt « S12 carburant 8 » (missions). TRANCHÉ : un seul fichier, front_retouches.txt, avec la syntaxe « ressource S12 carburant [débit] ».
- 18. [Élevé] K1 et K2 sont codés trois fois (grille MigrateOldSectors, missions PourLegacyStock, migration ImportLegacy + PayLegacyStock), avec des écarts.
- Attente des dépôts : IsCounted (grille, migration) ou IsRestockDone (missions). Vérifié : m_bCounted passe à vrai dans GetStockForSave même en plein regarnissage (SRP_ResourceManagerComponent.c:552) et jamais sans dépôt (l.480-484). IsCounted ne suffit donc pas.
- Surplus : perdu (grille), en caisses qui ne survivent pas au redémarrage (missions), ou réessayé toutes les 10 min (migration).
- Archive : territoire_ancien_DATE.json, territoire_ancien.json jamais effacé, ou archives/territoire_campagne1_DATE.json.
- Campagne : n°1 (grille), n°2 (migration) ou identifiant « C1790000000 » (web).
TRANCHÉ : le code du module migration, dans SRP_FrontComponent.Start. On attend IsRestockDone (à ajouter), avec un délai maximal ; le reste est réessayé. On archive dans archives/territoire_campagne1_<date>.json, puis on efface l'ancien fichier. Campagne n°2, identifiant « C2 » pour le pont.
- 19. [Élevé] Le pont vers le bot n'a pas ses données.
- Le web attend GetVersion (croissante, sauvegardée, +1 par carré), GetRestoreCount, GetCampaignId, et des appels à NoteFrontCell et NoteFrontReset. La grille n'a qu'une version de réplication, non sauvegardée, et aucun compteur de restaurations.
- BuildState a trois formats : web (bloc front + territory{ours, total, threat, blue, land}), migration (territory{…, frozen, campaign} + zones[]), missions (capture_targets[] de noms).
- J4 est servi deux fois : capture_targets du mod, zs f=1 côté bot.
TRANCHÉ : le format du web, avec « campaign » ajouté dans territory. capture_targets = [{c, l}], tiré de missions.GetCaptureTargets, qui écarte les zones déjà visées et le gel. Le cœur fournit GetBridgeVersion, GetRestoreCount et GetCampaignId.
- 20. [Moyen] La carte côté joueur lit des données jamais répliquées : GetCellCaptureSeconds et GetCaptureTotalSeconds (progression « 7/15 min » écrite sur le carré orange), GetZoneAttackMinutesLeft et GetZoneAttackWave. La grille ne réplique que l'état contesté (2 bits) et les drapeaux de zone. TRANCHÉ : pas de chiffres sur la carte en v1, l'orange suffit (I4). Minutes et vagues ne s'affichent qu'au PC et au Staff, dans des textes construits par le serveur.
- 21. [Élevé] Les garnisons dépendent d'un module qui n'existe pas.
- Les zones attendent un « module 5 garnisons » : IsGarrisonPosted, IsGarrisonSettled, WithdrawLocality, OnZoneCaptured(zone), OnGarrisonPosted/Gone.
- L'ennemi fournit l'équivalent sous d'autres noms : territory.OnZoneCaptured, OnKeyPointTaken, AdoptAssailants, IsInsideGarrisonedLocality. Les missions veulent IsInEnemyStronghold ; la migration supprime le test.
- F12 : sous 2 soldats, « rien n'est posé » (ennemi), mais « OnGarrisonPosted quand même, pour poser le poste de commandement » (zones).
- SRP_EnemyGarrison.BuildPlan est modifié par les zones ET par l'ennemi, avec des champs différents.
TRANCHÉ : l'ennemi possède la partie localités de SRP_Territory.c et SRP_EnemyGarrison.c. Il fournit IsGarrisonPosted (vrai même pour une garnison vide), IsGarrisonSettled, OnLocalityTaken (repli, sirène, camions, pertes à zéro), AdoptAssailants et IsInGarrisonedLocality(pos, marge).
- 22. [Moyen] Menus Staff et PC écrits trois fois.
- SRP_Admin.c est réécrit par zones (page territoire:pc), carte (territoire:carre-bleu, zone:, gel, sauvegardes) et migration (territoire:carre:bleu, jours, restaurer), avec des clés incompatibles.
- PC : SRP_FrontScreens (carte) contre GetStatusText gardé (migration).
- ForceAttack a trois signatures : (string, author) sur l'ennemi, (int zone, author) sur le cœur, (string, string) sur SRP_TerritoryComponent.
- Aucun bouton pour CancelAttack, ForceOffensive, GetPostsReport, GetNightReport.
- La carte attend GetZoneEnemiesActive, GetZoneGarrisonGroups et GetZoneDepotText, fournis sous d'autres noms (CountFitEnemiesInZone, StatusText).
TRANCHÉ : la carte possède les pages territoire de SRP_Admin.c et SRP_PC.c. Elle y branche la page des points clés, les commandes de l'ennemi et le dépôt des missions.
- 23. [Moyen] Le prefab est tiré dans deux sens.
- La grille retire SRP_TerritoryComponent ; la migration le garde, et c'est lui qui porte les garnisons.
- Cinq nouveaux composants ont des GUID écrits à la main ({6A5B0C0D0E0FF010}, {6A5F2026C0A70002}, {6A5B0C0D0E0FF004}, {6A60F0A1B2C3D401}) ; celui des zones n'en a pas encore.
- Le prefab porte déjà deux blocs à GUID écrit à la main, désactivés (Enabled 0) : SRP_BridgeComponent {…0F7002} et SRP_DiscordComponent {…0F7001}. Ils ont probablement été remplacés par des GUID du Workbench.
TRANCHÉ : SRP_TerritoryComponent est gardé avec un bloc vide. Seulement deux composants neufs : SRP_FrontComponent (qui porte capture, chute, économie, radio et affichage comme classes internes, avec leurs réglages par catégorie) et SRP_FrontEnemyComponent.
- 24. [Moyen] Les localités viennent de deux sources : la grille lit les noms de la carte (GetByType), zones et ennemi lisent SRP_CivilZoneComponent.GetZones(), comme AddAutoSectors aujourd'hui (dédoublonnage à 300 m). Le nom d'une zone risque de ne pas correspondre à celui de sa garnison. TRANCHÉ : une seule source, SRP_CivilZoneComponent, celle des garnisons et de K2, lue par la grille.
- 25. [Moyen] Les chiffres ne concordent pas entre modules.
- Grille : 66 zones plus la base, après fusion des petits morceaux et coupure des 2 blocs doubles.
- Calcul du matin (blocs.json) : 70 zones.
- Zones, carte, web, migration : 82 (« 54 sur 82 sans localité », « le PC liste 82 lignes », « 3/82 »).
- Réponse I10 : 30.
TRANCHÉ : la référence est front_zones.txt au premier démarrage (attendu : environ 66). Aucun nombre n'est écrit en dur dans les textes.

## mod_capture.json — dépendances déclarées
- Module 1 (grille, zones, sauvegarde, réplication), nom supposé SRP_FrontComponent : static SRP_FrontComponent GetInstance() ; bool IsReady().
- Module 1 : int GetCellCount() (4096) ; int GetCols() (64) ; float GetCellSize() (200) ; int CellIndexAt(vector pos) (-1 si mer ou hors grille) ; int CellCol(int idx) ; int CellRow(int idx), avec idx = ligne × 64 + colonne, ligne = floor(z / 200), colonne = floor(x / 200).
- Module 1 : bool IsLand(int idx) ; int GetOwner(int idx) (SRP_EFrontOwner.ROUGE = 0, BLEU = 1) ; int GetLandComponent(int idx) (0 pour l'île principale, 1 pour Erquy).
- Module 1 : int GetZoneOf(int idx) ; int GetZoneCount() ; string GetZoneLabel(int zone), qui renvoie « Régina (S07) » (A6).
- Module 1 : bool IsBaseCell(int idx) (zone de la base, B2) ; bool IsProtectedCell(int idx) (zone de la base et zones voisines, B4).
- Module 1 : int GetKeyPointCount(int zone) ; vector GetKeyPoint(int zone, int k). Ce sont les points posés à la main (D3), deux par ville (D6).
- Module 1 : bool IsFrozen() (J3, sauvegardé). Au changement, il appelle SRP_FrontCaptureComponent.GetInstance().OnFrozenChanged(bool).
- Module 1 : int GetCutSince(int idx) et void SetCutSince(int idx, int unixTime). Heure de coupure E4, sauvegardée, avec sauvegarde différée.
- Module 1 : void ApplyCellChanges(notnull array<int> cells, notnull array<int> owners, int cause). Il applique les changements, prévoit la sauvegarde et réplique (RPC Broadcast et application locale, car le Broadcast ne tourne pas sur l'hôte). Il prévient ensuite les zones (D1 et perte de zone), la production (G7 : dépôt détruit si son carré est repris ; E4) et les missions (#50). En dernier, il appelle SRP_FrontCaptureComponent.OnOwnersChanged(cells).
- Module 1 : void SetLiveCells(notnull array<int> cells, notnull array<int> states). Il réplique les différences d'état contesté (C8) vers le calque de carte (I1).
- Module 1 : enum SRP_EFrontCause { CAPTURE, REPRISE, COUPURE, ZONE, OFFENSIVE, STAFF, REMISE }, commun à tous les modules.
- Module des garnisons ou des zones : bool IsCellAwaitingGarrison(int idx). Il renvoie vrai si le carré recouvre une localité ennemie dont la garnison n'a pas encore été posée ou n'a pas fini de l'être (équivalent de SRP_Territory.c:994).
- Module des contre-attaques : il pose record.m_iAssaultZone = zone sur chaque groupe de vague, débarqués de camion de vague compris, seulement pendant l'assaut. Il le remet à -1 avant RetireLater ou ConvertToGarrison. Il ne vise pas les zones protégées (B4), car ce serait une attaque sans enjeu. L'offensive fictive (H2) passe par ApplyCellChanges(..., OFFENSIVE) et ne part pas si le front est gelé. Pour F4, il utilise CountFitAssaultersInZone.
- Module des zones (D) : il fait tomber une zone par ApplyCellChanges(..., ZONE) et annonce par zone. Il n'annonce pas les changements COUPURE, que ce module annonce déjà. Il utilise IsCapturableCell (D2), CountFitEnemiesNear (D5) et CountValidPlayersNear (D4). Il fixe la règle de perte d'une zone avec F3 = carré par carré (question à Jack).
- Module de production : il lit IsZoneCut(zone), car une zone coupée ne produit plus (E4). Il détruit le dépôt quand son carré est repris avec la cause REPRISE (G7, nouveau kit).
- Existant : SRP_EnemyComponent.GetInstance(), GetGroups() (SRP_Enemy.c:4200), InGrace(int) (l.851), Journal(string, string) (l.671). SRP_PlayerManagerComponent.GetInstance().IsStaff(int) (SRP_PlayerManagerComponent.c:458). SRP_AdminComponent.GetInstance() (SRP_Admin.c:63) et IsGod(int), méthode à ajouter. SRP_FleetManagerComponent.GridOf(vector) (SRP_Fleet.c:802). SRP_Utils.NotifyAll(string) (SRP_Definitions.c:692). SRP_BridgeComponent.PushEvent(string, string).
- Vanilla vérifié dans scratchpad\vanilla\scripts, partie personnages et IA : ChimeraCharacter.GetCharacterController() et IsInVehicle() (Game\generated\Character\ChimeraCharacter.c:24 et :30). CharacterControllerComponent.GetLifeState() (Game\generated\Components\CharacterControllerComponent.c:265) et ECharacterLifeState.ALIVE / INCAPACITATED / DEAD (ECharacterLifeState.c:12). SCR_TerrainHelper.GetHeightAboveTerrain(vector pos, BaseWorld world = null, bool noUnderwater = false, TraceParam trace = null) (Game\Helpers\SCR_TerrainHelper.c:51). AIGroup.GetAgents(out array<AIAgent>) (AIGroup.c:20). AIAgent.GetControlledEntity() (AIAgent.c).
- Vanilla vérifié, partie joueurs, système et éditeur : PlayerManager.GetPlayers, GetPlayerControlledEntity et GetPlayerIdFromControlledEntity (Game\generated\Network\PlayerManager.c:30, :45, :53). System.GetUnixTime() (Core\generated\System\System.c:53). vector.DistanceXZ (Core\generated\Types\vector.c:94). array.Resize qui met les nouveaux éléments à zéro (Core\proto\Types.c:276). SCR_GameCoreBase.GetInstance(typename) (Game\GameCore\SCR_GameCoreBase.c:110). SCR_EditorManagerCore.GetEditorManager(int playerID) (Game\Editor\Core\SCR_EditorManagerCore.c:238). SCR_EditorManagerEntity.IsOpened() et IsLimited() (Game\Editor\Entities\SCR_EditorManagerEntity.c:99 et :529). SCR_PossessingManagerComponent.GetInstance() et IsPossessing(int) (Game\GameMode\Components\SCR_PossessingManagerComponent.c:22 et :172). SCR_BaseGameModeComponent.OnPlayerDisconnected(int, KickCauseCode, int) (Game\GameMode\SCR_BaseGameModeComponent.c:83).
- Non utilisé mais vérifié, en solution de rechange : TagSystem.GetTagsInRange(out array<IEntity>, vector, float, ETagCategory) (Game\generated\System\TagSystem.c) avec ETagCategory.Character = 128 (Game\Enums\ETagCategory.c). Introuvable dans les scripts extraits : la définition de la classe SCR_ChimeraCharacter. C'est pourquoi j'utilise ChimeraCharacter, dont l'API générée est vérifiée.

## mod_carte.json — dépendances déclarées
- Module grille, zones et réplication. Classe commune serveur et joueurs SRP_FrontGrid :
- static SRP_FrontGrid GetLocal() : l'état qui fait foi sur le serveur, la copie reçue chez le joueur (RplLoad + RPC), null avant réception ;
- bool IsReady() ; int GetVersion(), +1 à chaque changement appliqué, y compris sur l'hôte, où le Broadcast ne s'exécute pas ;
- int GetGridSize() (64) ; float GetCellSize() (200) ;
- int GetCellOwner(int idx) : -1 hors jeu, 0 ennemi, 1 nous (idx = gz*64 + gx) ;
- bool IsCellContested(int idx) ; int GetCellCaptureSeconds(int idx) ; int GetCaptureTotalSeconds(int owner) (900 pour un carré rouge, 60 pour un bleu) ;
- int GetCellZone(int idx) ; int CellIndexAt(vector pos) ; string CellRef(int idx), qui renvoie « 074 042 ».
- Même classe SRP_FrontGrid, partie zones :
- int GetZoneCount() ; string GetZoneName(int zone), GetZoneCode(int zone) (« S07 », sans « : » ni « | »), GetZoneLabel(int zone) (« Régina (S07) ») ;
- vector GetZoneLabelPos(int zone) : centre des carrés de terre ; int FindZone(string codeOuNom) ;
- int GetZoneOwner(int zone) ; int GetZoneBlueCells(int zone) ; int GetZoneLandCells(int zone) ;
- int GetZoneAttackState(int zone) (0, 1 annoncée, 2 en cours) ; int GetZoneAttackMinutesLeft(int zone) ; int GetZoneAttackWave(int zone) ;
- bool IsZoneAtFront(int zone) (E1) ; bool IsZoneBase(int zone) (B2) ; int GetZoneCutMinutes(int zone) (-1 si la zone est reliée, E4) ;
- int GetKeyPointCount(int zone) ; vector GetKeyPointPos(int zone, int k) ; int GetKeyPointKind(int zone, int k) (0 PC ou centre, 1 QG) ; int GetKeyPointOwner(int zone, int k) ;
- bool IsFrozen() ; int CountZonesOurs() ; int CountCellsOurs() ; int CountCellsLand().
- Même module, serveur seulement, SRP_FrontComponent :
- GetInstance() ; int GetThreat() ;
- string ForceCell(int idx, int owner, string author), qui refuse la zone de la base ;
- string ForceZone(int zone, int owner, string author) ; string ResetZone(int zone, string author) ; string NewCampaign(string author) ; string SetFrozen(bool frozen, string author) ;
- int GetDailySaves(notnull array<string> dates, notnull array<string> summaries), du plus récent au plus ancien ; string RestoreDailySave(string date, string author) ;
- string ForceAttack(int zone, string author) ; string GetZoneLastChange(int zone) ; string GetRadioReport(vector from, float radius).
Chaque ForceCell, ForceZone, ResetZone, NewCampaign ou restauration fait monter GetVersion. Ces appels se font avec la cause STAFF, REMISE ou RESTAURATION, sans prime ni menace.
- Modules capture, contre-attaque et coupure : ils appellent SRP_FrontRadio.QueueCell(idx, newOwner, SRP_EFrontCause.COMBAT ou CONTRE_ATTAQUE) à chaque carré qui change par le combat, puis SRP_FrontRadio.Flush() et SRP_FrontMarkers.Tick() en fin de chaque passage de 5 s. Ils appellent aussi ZoneFell, KeyPointSeized, HalfReached, AttackAnnounced, AttackWave, Defended, ZoneCut, CutReminder, Reconnected, Frozen, Restored, NewCampaign, Victory aux moments correspondants. Ils n'appellent plus eux-mêmes NotifyAll pour ces événements.
- Module garnison : int GetZoneGarrisonGroups(int zone) et int GetZoneEnemiesActive(int zone), pour le Staff et pour le PC (zones au front seulement).
- Module production : string GetZoneDepotText(int zone) (vide s'il n'y a rien), string ForceDepot(int zone, vector pos, string author), string RemoveDepot(int zone, string author).
- Module Discord et pont : il choisit les types passés à SRP_BridgeComponent.PushEvent. Par défaut, on garde « secteur_pris », « secteur_perdu » et « attaque », et on ajoute un type de remise à plat pour Restored et NewCampaign, pour que le site n'y voie pas des dizaines de prises (piège noté en J2).

## mod_ennemi.json — dépendances déclarées
- Quand un module est « modifié », il faut prendre les signatures au pied de la lettre, ou les renommer en même temps partout.
- Module GRILLE : static SRP_FrontGridComponent GetInstance().
- Module GRILLE : bool IsReady().
- Module GRILLE : int CellAt(vector pos), qui rend -1 hors de la grille ou en mer.
- Module GRILLE : vector CellCenter(int cell), au sol.
- Module GRILLE : bool IsLand(int cell).
- Module GRILLE : int GetOwner(int cell), avec SRP_ECellOwner.ROUGE = 0 et BLEU = 1.
- Module GRILLE : int Neighbour(int cell, int dir), dir de 0 à 3 (N, E, S, O), -1 hors de la terre.
- Module GRILLE : int GetRevision(), augmenté à chaque changement de propriétaire.
- Module GRILLE : bool IsFrozen() (J3).
- Module GRILLE : string CellLabel(int cell), la référence « 074 042 » (A7).
- Module GRILLE : int GridSize() = 64 et float CellSize() = 200.
- Module CAPTURE : bool RetakeCell(int cell, string reason). Passe le carré au rouge et fait tout le reste : radio I5, journal, dépôt détruit si c'est son carré (G7), recalcul de la zone, réplication, sauvegarde. Rend faux si c'est refusé (front gelé, zone de la base ou zone voisine de la base).
- Module CAPTURE : bool HasDefenderInCell(int cell). Joueur vivant, conscient, au sol ou en véhicule terrestre, hors des 5 min qui suivent une téléportation ou le Game Master (C2, C3), calculé à chaque passage.
- Module CAPTURE : bool HasDefenderNear(vector pos, float radius), avec le même filtre (F5).
- Module CAPTURE, en retour : il utilise SRP_FrontEnemyComponent.FitEnemiesInCell(int cell) (C6) et FitEnemiesNear(vector pos, float radius) (D5, rayon 100 m).
- Module ZONES : int ZoneCount() ; string ZoneName(int z), forme « Régina (S07) » ; int ZoneOwner(int z).
- Module ZONES : void ZoneCells(int z, out array<int> cells) ; int ZoneOfCell(int cell).
- Module ZONES : bool IsBaseZone(int z) (B2) et bool IsBaseNeighbour(int z) (B4).
- Module ZONES : int ZoneCapturedUnix(int z), date de prise sauvegardée (F2).
- Module ZONES : vector ZoneKeyPoint(int z), vector.Zero sans localité ; void ZoneLocalities(int z, out array<string> names).
- Module ZONES : bool SetZoneOwner(int z, int owner, string reason). Pour H2 : perte complète de la zone (dépôt, stock, menace −1, radio, pont).
- Module ZONES, localités : SRP_SectorDef garde m_sName, m_sKind et m_fRadius (150, 250 ou 350). m_vCenter devient le point clé 1 posé par Jack (D3). Nouveaux champs vector m_vKeyPoint2, bool m_bHasKeyPoint2 (QG ennemi des villes, D6) et int m_iZone.
- Module ZONES, localités : SRP_SectorState.m_iOwner = ENNEMI tant que la localité est hostile (zone rouge et point clé non saisi), NOUS sinon. C'est indispensable : camions et sirène testent ce champ (SRP_EnemyTrucks.c:447, 1045, 2130, 2352).
- Module ZONES, localités : bool IsKeyPointTaken(string locality) (D4).
- Module ZONES, appels vers le module 4 : territory.OnZoneCaptured(int z) et front.OnZoneCaptured(int z) quand une zone tombe.
- Module ZONES, appels vers le module 4 : front.OnZoneLost(int z) quand une zone repasse rouge. C'est lui qui applique la menace −1 (G9).
- Module ZONES, appels vers le module 4 : territory.OnKeyPointTaken(string locality), pour faire taire la sirène et rappeler les camions (RecallSector).
- Module ZONES, question non tranchée par le QCM : quand la zone repasse-t-elle rouge ? Proposition : l'inverse de D1.
- Menace (déjà en place, SRP_Territory.c:732 et 1959) : int GetThreat() et void AddThreat(int delta, string reason).
- Module STAFF / PC, appels vers le module 4 : ForceAttack(zoneName, author), CancelAttack(author), ForceOffensive(bool success, author).
- Module STAFF / PC, appels vers le module 4 : GetAttackReport(), GetPostsReport(vector, float), GetNightReport().
- Module STAFF / PC, appels vers le module 4 : ResetAll(reason), pour B3 et K1.
- Module STAFF / PC : GetAttackState(int zone) sert à l'onglet Territoire (I10), et CountFitEnemiesInZone(int zone) aux infos des zones du front (I3).
- Module STAFF / PC : la sauvegarde datée quotidienne (J2) doit copier aussi $profile:SimpleRP/ennemi_front.json.
- Module PONT / BOT : types d'évènement « attaque » (déjà en place), plus « zone_defendue » et « offensive », nouveaux. bot.py doit les traiter (couleurs, flux territoire).
- Module PONT / BOT : les textes gardent les mots « attaque », « défendue », « perdue » pour les couleurs de SRP_Discord.c:396-404.
- Module MISSIONS (hors de ce module) : G11 (gardes toujours présentes) retire le test IsInsideEnemySector de SRP_Missions.c:1946. G10 (contrôle routier seulement en bleu, SRP_Missions.c:1121) passe par grille.GetOwner.
- Module MISSIONS : IsInsideGarrisonedLocality(pos, margin) reste disponible si besoin.
- Vanilla, vérifié : RoadNetworkManager.GetRoadsInAABB(vector, vector, out array<BaseRoad>) et GetClosestRoad(vector, out BaseRoad, out float, bool = false) (Game/generated/RoadNetwork/RoadNetworkManager.c:14-16).
- Vanilla, vérifié : BaseRoad.GetWidth() et GetPoints(out notnull array<vector>) (BaseRoad.c:18-20).
- Vanilla, vérifié : BaseWorld.GetSurfaceY(float, float) (Core/generated/World/BaseWorld.c:24).
- Vanilla, vérifié : System.GetUnixTime(), GetYearMonthDay, GetHourMinuteSecond, GetTickCount (Core/generated/System/System.c:53, 72, 110, 134).
- Vanilla, vérifié : AIWorld.GetLimitOfActiveAIs() et GetCurrentNumOfActiveAIs() (Game/generated/AI/AIWorld.c:19-20).
- Vanilla, vérifié : SCR_BaseGameMode.GetOnPlayerSpawned() (Game/GameMode/SCR_BaseGameMode.c:629).

## mod_grille.json — dépendances déclarées
- Code existant utilisé tel quel : SRP_Paths.EnsureDirectories() (SRP_Definitions.c:562) ; SRP_EnemyComponent.Journal(string category, string text) (SRP_Enemy.c:671) ; SRP_BridgeComponent.PushEvent(string type, string text) (SRP_Bridge.c:254), avec les nouveaux types zone_coupee, zone_perdue, victoire, front_campagne, front_restaure et front_gel, que le module pont et bot devra traiter ; SRP_Utils.NotifyAll(string) pour la radio ; SCR_EntityHelper.DeleteEntityAndChildren.
- SRP_ResourceManagerComponent : GetInstance(), Add(int resource, int amount, string reason, bool log) (l.220) et le nouveau bool IsCounted().
- Module capture (C, D) :
- il appelle CanBlueCapture(cell) avant SetCellOwner(cell, SRP_EFrontOwner.BLEU, SRP_EFrontReason.CAPTURE) ;
- il appelle SetCellLive(cell, SRP_EFrontLive.CAPTURE ou COMBAT) pour l'orange, puis SetKeyPointTaken(k, true) ;
- quand D1 est rempli, il appelle SetZoneOwner(zone, BLEU, ZONE_TOMBEE, true) ;
- il lit IsFrozen(), GetZoneKeyPoints, CountZoneCellsOwnedBy et GetKeyPoint(k).m_iCell (pour D2 avec HasNeighbourOwnedBy) ;
- il s'abonne par GetOnFrontReset().Insert(méthode(int kind, int zone)) pour remettre ses compteurs à zéro.
- Module contre-attaque et front ennemi (F, H2) :
- il lit GetBlueFrontZones(result) et CanRedRetake(cell) ;
- il appelle SetCellOwner(cell, ROUGE, CONTRE_ATTAQUE) et SetZoneOwner(zone, ROUGE, ZONE_PERDUE ou OFFENSIVE, true) selon sa règle (le socle refuse B4 et le gel) ;
- il pose SetZoneFlag(zone, FLAG_ATTACK_ANNOUNCED ou FLAG_ATTACK, on) ;
- il range la date de l'offensive fictive par SetExtra(offensive, date) ;
- il s'abonne à GetOnFrontReset().
- Module garnisons : il lit GetLocality(i) (m_sKind, m_iZone, m_iCentre, m_iQg, m_iLosses, m_iLossesAt) et GetKeyPoint(k).m_vPos ; après avoir écrit la mémoire F12, il appelle MarkDirty(false).
- Module production et dépôt (G4 à G7) :
- il lit et écrit GetZone(z) (m_iStock, m_bDepot, m_vDepot, m_sResource), puis appelle MarkDirty(true) ;
- il lit IsZoneLinked(z) (E4 : plus de production quand la zone est coupée) ;
- il s'abonne à GetOnCellOwnerChanged() (G7 : dépôt détruit quand son carré repasse rouge), GetOnZoneOwnerChanged() et GetOnFrontReset().
- Module missions (G1, G3, G10 à G13, J4) : GetZonesInContact(result), FindZone(codeOrName), GetOwnerAt(position), GetZoneAt(position), GetZoneCentroid(zone).
- Menace (G9) : les modules appellent AddThreat(+1 ou -1, raison) depuis leur abonnement à GetOnZoneOwnerChanged() ; ils lisent GetThreat() à la place de SRP_TerritoryComponent.GetThreat.
- Calque de carte, côté joueur (I1 à I3) :
- GetGeometryVersion, GetStateVersion, GetLiveVersion ;
- GetGridSize, GetCellSize, IsCellInPlay, GetCellOwner, GetCellLive, GetCellZone, IsCellAtFront ;
- GetZoneName, GetZoneCentroid, IsZoneInContact ;
- GetKeyPointCount et GetKeyPoint (position, zone, pris) ; GetSeaLink(cell).
- Menu Staff (J1 à J3) : ForceZone(zone, owner, author), ForceCellAt(position, owner, author), SetFrozen(bool, author), ResetCampaign(reason, author), ResetZone(zone, author), RestoreDaily(day, author), GetDailyCopies(result), GetGeometryReport().
- Pont et bot (I6 à I8, J2, K1) :
- GetCellsString() : N × N caractères « . », « R », « B » ;
- GetZoneMapString() : un caractère de zone par carré ;
- GetZoneTable() : une ligne par zone (code|nom|propriétaire|part bleue|contact|coupée) ;
- GetStateVersion(), GetGeometryVersion(), GetCampaign() ;
- la frise doit repartir de zéro sur front_restaure et front_campagne.

## mod_migration.json — dépendances déclarées
- Module grille et zones : class SRP_ZoneState avec :
- string m_sKey ('S07'), string m_sName, int m_iOwner, int m_iBlue, int m_iSquares, bool m_bFront, int m_iType (SRP_ESectorType) ;
- production et dépôt : int m_iStock, int m_iProductionMs, bool m_bDepot, vector m_vDepot, IEntity m_Box, ref SCR_MapMarkerBase m_DepotMarker ;
- string m_sLastChange, int m_iCut (heure Unix, E4), ref array<string> m_aLocalities ;
- méthodes string GetLabel() (« Régina (S07) »), vector GetMissionPoint() (point clé, sinon centre du bloc), vector GetCenter().
SRP_TerritoryComponent doit fournir :
- string CellsToString() et bool CellsFromString(string cells) (4 096 caractères B/R/point) ;
- void NewCampaignState() (B1/B2) ; void BuildLocalities() ;
- SRP_ZoneState FindZone(string text) (code, nom ou « nom (code) », sans tenir compte des majuscules).
- Module capture de carrés : void SetSquareOwner(int index, int owner, string why). Il écrit les carrés au journal en catégorie FRONT, les annonce à la radio, et marque la sauvegarde sale sans l'écrire aussitôt. Rien de la progression n'est sauvegardé (H3).
- Module chute de zone et points clés : void OnZoneFallen(SRP_ZoneState zone, bool staff). Il appelle :
- missions.HasActiveCapture(zone.m_sKey) et la prime G8 (treasury.Add), sinon PushEvent('commandement') et NotifyOfficiers (G2) ;
- AddThreat(1) ; PushEvent('secteur_pris', texte avec « capturée ») ; missions.OnZoneCaptured(zone.m_sKey).
Il tient m_iOwner des localités (SRP_SectorState) : NOUS une fois le ou les points clés saisis, ENNEMI si l'ennemi les reprend.
- Module front et contre-attaques : string ForceAttack(string zoneText, string author) ; champs d'attaque sur SRP_ZoneState (m_iAttackState 0/1/2, m_iAttackTick, m_iWaves) ; bool IsFrozen() ; minuteries sauvegardées dans front.json sous les clés z<i>_cut (E4) et lastOffensive (H2).
- Module IA : les champs de localité conservés (SRP_SectorState) et la mémoire des pertes dans les clés l<j>_lost et l<j>_lostAt. La pose des garnisons garde les signatures actuelles de SRP_EnemyGarrison et SRP_EnemyTrucks.
- Module missions et économie :
- SRP_MissionManagerComponent : bool HasActiveCapture(string zoneKey) ; SRP_Mission FindCapture(string zoneKey) ; SRP_Mission CreateCapture(string zoneText, int chiefId, string author) (J4 + G3) ; void OnZoneCaptured(string zoneKey) (nouveau nom) ;
- SRP_TerritoryComponent : void OnCaptureFailed(string zoneKey) ; production par zone dans les champs de dépôt de SRP_ZoneState ; InstallDepot(int playerId, IEntity kit) avec la même signature (G6, G7).
- Module affichage, réseau et bot :
- GetStatusText() : ligne 0 de résumé, puis une ligne par zone ;
- JSON du pont : territory{ours, total, threat, blue, land, frozen, campaign}, zones[]{key, name, ours, blue, squares, front, attack, depot?, stock}, boxes[]{sector, count, resource}, front{…}, caps + 'front' ;
- RplSave et RplLoad sur SRP_TerritoryComponent pour l'instantané, RPC des changements, appliqués aussi en local sur l'hôte ;
- le bot lit les deux formats et archive quand territory.campaign change ;
- un retour à une copie datée (J2) est signalé au bot, pour qu'il n'enregistre pas des dizaines de prises.
- SRP_ResourceManagerComponent : bool IsCounted() (nouveau, rend m_bCounted), et int Add(int resource, int amount, string reason, bool log = true) existant (:220), qui rend le nombre de paquets rangés.

## mod_missions.json — dépendances déclarées
- Cœur (grille, zones, front), classe de zone. Noms proposés, à aligner sur le module 1 : class SRP_ZoneState { string m_sKey ; string m_sName ; int m_iOwner ; bool m_bBase ; string m_sPlaceKind ; string m_sPlaceName ; vector m_vSite ; ref array<int> m_aCells ; }. m_sKey est la clé stable « S07 », sans « : » ni « | ». m_sName vaut « Régina (S07) » (A6). m_iOwner vaut SRP_ESectorOwner.ENNEMI ou NOUS. m_bBase marque la zone de la base, imprenable (B2). m_sPlaceKind vaut « hameau », « village » ou « ville » (bourg compté comme ville) pour la plus grosse localité HORS base, et une chaîne vide sans localité. m_vSite est le point clé principal (le centre pour une ville, D6), sinon le centre du carré de terre le plus proche du centre des carrés. m_aCells contient les index des carrés de terre.
- Cœur, lecture : SRP_TerritoryComponent garde son nom et GetInstance(). Méthodes attendues :
- void GetZones(out array<SRP_ZoneState> result), dans un ordre stable ;
- SRP_ZoneState FindZone(string text) : accepte la clé, le nom complet ou la localité seule si elle est unique, sans tenir compte de la casse ni des espaces en double ;
- SRP_ZoneState ZoneAt(vector position) : null en mer ;
- int CellAt(vector position) : -1 en mer ;
- int GetCellOwner(int cell) : -1 en mer ;
- bool IsCellOnFront(int cell) : un des 4 voisins est de l'autre couleur ;
- bool IsBlueAt(vector position) et bool IsRedAt(vector position) : false en mer ;
- bool IsZoneOnFront(SRP_ZoneState zone), au sens de E1 ;
- bool IsZoneCutOff(SRP_ZoneState zone), au sens de E4 : aucun carré bleu de la zone relié à Levie par des carrés bleus, valeur en cache recalculée quand un carré change ;
- bool IsInBaseZone(vector position) ;
- bool IsInEnemyStronghold(vector position, float margin) : localité d'une zone rouge, rayon hameau 150, village 250, ville 350, plus la marge. Remplace IsInsideEnemySector ;
- bool IsFrozen(), pour J3 ;
- int GetThreat() ; void OnCaptureFailed(string zoneKey) ; void OnMissionSuccess(int type) ;
- void MarkDirty() et SRP_FrontEconomy GetEconomy().
- Cœur, crochets à appeler :
- missions.OnZoneCaptured(SRP_ZoneState zone, bool staff), exactement une fois par zone tombée, y compris quand le Staff force la prise. Le cœur ne verse plus aucune subvention lui-même ;
- m_Economy.OnCellChanged(int cell, int owner) pour CHAQUE carré qui change, quelle qu'en soit la cause ;
- m_Economy.OnZoneCaptured(zone), m_Economy.OnZoneLost(zone), m_Economy.Tick(5000), m_Economy.Init(this) après la construction des zones et le Load, m_Economy.ResetAll() ;
- m_Economy.WriteTo(ctx) et m_Economy.ReadFrom(ctx) dans la sauvegarde du cœur ;
- IMPORTANT : la nouvelle sauvegarde ne doit PAS s'appeler territoire.json (proposé : front.json), car K2 lit l'ancien format dans ce fichier.
- Cœur, D1 : quand une zone tombe, tous ses carrés passent bleus. OnZoneLost n'est appelé que si la règle de perte de zone du cœur (F3, E4 après 24 h, H2) fait repasser la zone rouge.
- SRP_ResourceManagerComponent, dans ce module : bool IsRestockDone() (nouveau) ; int Add(int resource, int amount, string reason, bool log = true) et IEntity SpawnBox(int resource, int packets, vector position, string missionId = « ») existent déjà.
- Existant, sans changement :
- SRP_TreasuryComponent.Add(int amount, string reason) ;
- SRP_EnemyComponent : GetActivePlayers(), ScaleGroupsNear(vector, int, int, float), LocalRoom(vector, float), SpawnGroups(...), MaybeSpawnVehiclePatrol ;
- SRP_BridgeComponent.PushEvent(string, string) ;
- SRP_PlayerManagerComponent.NotifyOfficiers(string) ;
- SRP_Resources.FromName et GetName ;
- SRP_FleetManagerComponent.GridOf(vector) ;
- SRP_CivilZoneComponent : GetZones, GetPlaceName, GetPlaceKind.
- Module affichage et pont : garder la clé boxes[] {sector, count, resource} en la nourrissant par GetEconomy().GetBoxes. Le PC et le Staff affichent GetEconomy().StatusText(zone) dans la ligne de chaque zone (I10).
- Module Staff (SRP_Admin l.1455, l.1461) : appeler GetEconomy().ForceDepot(zoneKey, position, author) et GetEconomy().RemoveDepot(zoneKey, author), avec la clé de zone.
- Module IA : SRP_Enemy.c:2778 (hélicoptère, G12), SRP_EnemyTrucks.c (l.442, 1024, 1044, 1095-1097, 2129, 2351), SRP_Sirene.c:455 et SRP_HeliSearch.c:370 utilisent IsInsideEnemySector, GetStates, FindState, NearestEnemySectors ou GetEnemySectors, qui disparaissent ou changent. Ils sont à reprendre par ce module ; le compilateur les signalera.
- API vanilla citées, toutes vérifiées dans scratchpad\vanilla\scripts :
- PlayerManager.GetPlayerCount() (Game/generated/Network/PlayerManager.c l.24) ;
- FileIO.OpenFile, FileExists, CopyFile (Core/generated/System/FileIO.c l.25, 32, 38) ;
- FileHandle.ReadLine(out string) et WriteLine (Core/generated/System/FileHandle.c l.83) ;
- LoadContext.ReadValue(string, out void) renvoie un bool (Game/generated/Plugins/Serialization/LoadContext.c l.21) ;
- SCR_MapMarkerManagerComponent.InsertStaticMarker(SCR_MapMarkerBase, bool, bool) et RemoveStaticMarker(SCR_MapMarkerBase) (Game/Map/Markers/SCR_MapMarkerManagerComponent.c l.169, 272).

## mod_web.json — dépendances déclarées
- Module grille et zones, composant serveur (nom provisoire SRP_FrontComponent) : static SRP_FrontComponent GetInstance() ; bool IsReady() ; int GetGridSize() qui renvoie 64 ; int GetCellMeters() qui renvoie 200.
- SRP_FrontComponent, lecture des carrés : int GetCellOwner(int cell) renvoie 1 (bleu), 0 (rouge) ou -1 (mer, îlot vide) ; index = gz*64+gx, gz compté depuis le sud ; int CountLandCells() ; int CountBlueCells().
- SRP_FrontComponent, suivi : string GetCampaignId(), sans espace (^[A-Za-z0-9_-]+$), stable jusqu'à la campagne suivante et sauvegardé ; int GetVersion(), +1 à chaque carré qui change de camp, sauvegardé, ne redescend jamais ; int GetRestoreCount(), +1 à chaque retour à une sauvegarde datée (J2), sauvegardé ; bool IsFrozen() (J3) ; void GetContestedCells(notnull array<int> cells) (C8).
- SRP_FrontComponent, zones : int GetZoneCount(), toujours sous 255 ; SRP_FrontZone GetZone(int index) ; int GetZoneIndexOfCell(int cell), -1 hors jeu ; int CountZonesOurs() ; et le getter de la menace (int GetThreat(), sur le composant qui la porte).
- SRP_FrontZone, identité : string GetCode() (« S07 », unique, stable, sans espace) ; string GetName() ; string GetLabel() (« Régina (S07) ») ; string GetKind() (hameau, village, ville ou lieu-dit).
- SRP_FrontZone, état : bool IsOurs() ; int GetAttackState() (0, 1 ou 2) ; bool IsAtFront() (E1, F1) ; bool IsCut() ; int GetCutSecondsLeft() (-1 si la zone n'est pas coupée) ; bool IsProtected() (B2, B4).
- SRP_FrontZone, dépôt : bool HasDepot() ; vector GetDepotPosition() ; int GetStock() ; int GetResource() (-1 = aucune ressource).
- Le module du front appelle SRP_BridgeComponent.NoteFrontCell(int cell, int owner, int version) à CHAQUE changement de camp d'un carré : capture, chute de zone, contre-attaque, zone coupée depuis 24 h (E4), offensive de la nuit (H2), Staff.
- Le module du front appelle SRP_BridgeComponent.NoteFrontReset() APRÈS une remise à zéro, une nouvelle campagne ou un retour à une sauvegarde datée.
- Modules qui annoncent (zones, contre-attaques, E4, H2, Staff, victoire) : SRP_BridgeComponent.PushZoneEvent(type, zoneCode, text), avec les types zone_prise, zone_perdue, zone_attaque, zone_assaut, zone_defendue, zone_coupee, zone_reliee, victoire, campagne, front. « commandement » reste pour une capture sans mission.
- Journal : SRP_JournalComponent.Log("TERRITOIRE", …) seulement au niveau zone, avec les mots-clés « prise », « PERDUE », « attaque », « vague », « défendue », « coupée », « reliée », « VICTOIRE », « Nouvelle campagne », « gelé », « ramené ». Jamais « garnison », « groupe(s) » ni « tirage » dans une ligne publique.
- Journal carré par carré : catégorie "FRONT", texte « Carré 074 042 pris — Régina (S07) » ou « Carré 074 042 perdu — Régina (S07) ».
- Missions (#50) : CreateCapture(place, chiefId, author) accepte le code de zone (« S07 ») en plus du nom et du libellé, car /ordonner capture envoie le code. OrderFromBridge (SRP_Missions.c:1463) ne change pas.
- Victoire (B3) : la remise à zéro attend au moins 30 s après l'événement « victoire » (au moins 2 contacts du pont), pour que le bot photographie l'île toute bleue.
- SRP_Bridge.c ne lit plus rien de SRP_TerritoryComponent ni de SRP_SectorState : le module qui retire ces classes n'a rien à garder pour le pont.

## mod_zones.json — dépendances déclarées
- Module 1 (grille et zones), classe appelée ici SRP_FrontComponent : static GetInstance() ; bool IsReady() ; int CellAt(vector pos), qui renvoie -1 en mer ; int GetCellOwner(int cell) (0 ENNEMI, 1 NOUS) ; bool TouchesBlue(int cell), vrai si un des 4 voisins est bleu ; void SetCellOwner(int cell, int owner, string reason, bool announce), qui sauvegarde et réplique (announce = faux : ni radio ni pont).
- Module 1, suite : int ZoneOfCell(int cell) ; void GetZoneCells(int zone, out array<int> cells) ; int CountZoneCells(int zone) ; int CountZoneBlue(int zone) ; int GetZoneOwner(int zone) ; void SetZoneOwner(int zone, int owner, string when), persisté avec la date et un numéro de prise pour F2 ; string GetZoneKey(int zone) (clé stable S07) ; string GetZoneLabel(int zone) (« Régina (S07) ») ; int FindZone(string key) ; vector GetZoneCentroid(int zone).
- Module 1, règles globales : bool IsBaseZone(int zone) (B2) ; bool IsLossProtected(int zone) (B2 et B4 : la base et ses zones voisines) ; bool IsFrozen() (J3).
- Module 1, appels vers ce module : SRP_ZoneFallComponent.GetInstance().OnCellChanged(int cell, int oldOwner, int newOwner) à chaque changement de carré ; void OnZoneOwnerChanged(int zone, int owner) côté module 1, pour la victoire B3, la zone coupée E4, la photo I8 et la production G4.
- Module 2 (capture des carrés) : il appelle IsCellSeizeOnly(int cell) avant toute capture par présence. Il fournit int CountFitGroundEnemies(vector pos, float radius), le filtre C6 (sinon ce module l'implémente), et bool PlayerCounts(int playerId), les filtres C2 et C3 (Staff pendant 5 min après une téléportation ou le Game Master).
- Module 4 (contre-attaques) : il appelle IsKeyCellHeld(int cell) avant de faire passer un carré rouge (F5). Dans OnZoneOwnerChanged(zone, ENNEMI), il convertit les assaillants en garnisons des localités de la zone (ConvertToGarrison autour du point clé centre, SRP_Enemy.c:1841) puis appelle OnGarrisonPosted(place). Pour F2, il lit le numéro de prise de la zone. Les offensives fictives (H2) et la zone coupée (E4) appellent LoseZone(zone, raison, faux, vrai), donc avec repeinte.
- Module 5 (garnisons et postes du front) : l'état de localité (l'actuel SRP_SectorState) garde m_Sector.m_vCenter = point clé centre et reçoit m_vHQ. Il fournit bool IsGarrisonPosted(string place) (vrai même pour une garnison vide par la mémoire F12) et bool IsGarrisonSettled(string place) (SRP_EnemyComponent.IsSettled, SRP_Enemy.c:3418). Il appelle OnGarrisonPosted(place) et OnGarrisonGone(place, why).
- Module 5, suite : void WithdrawLocality(string place, string why), qui reprend SRP_Territory.c:1672-1700. Il ne repose aucune garnison si IsLocalityTaken(place) est vrai. void OnZoneCaptured(int zone) retire les postes du front (F7) de la zone. Le QG d'une ville est posé au point QG (SRP_EnemyGarrison.BuildPlan).
- Missions (#50) : bool HasActiveCapture(string zoneKey) (SRP_Missions.c:1526) ; void OnZoneCaptured(string zoneKey), qui remplace OnSectorCaptured (l.2677) ; CreateCapture utilise GetZoneSite(zone) ; OnCaptureFailed(zoneKey) ; G3 (deux captures à partir de 10 joueurs) reste de leur ressort.
- Territoire (SRP_Territory.c) : void AddThreat(int delta, string reason) (l.1959) ; int GetThreat() (l.732). Trésorerie : SRP_TreasuryComponent.Add(int euros, string reason), comme utilisé l.1714. Joueurs : SRP_PlayerManagerComponent.NotifyOfficiers(string) (l.443). Pont : SRP_BridgeComponent.PushEvent(string type, string text) (SRP_Bridge.c:254), avec les types existants secteur_pris et secteur_perdu.
- Module 6 (affichage, pont, site) : il lit GetKeyPoints(out array<SRP_KeyPoint>) (lieu, rôle, position, carré, saisi) et GetZoneFallText(zone) pour l'onglet Territoire du PC (I10), le menu Staff, la carte du jeu et la carte web. Il garde les messages de zone de ce module et n'annonce pas les carrés repeints en silence.


# COMMANDEUR — décisions de cohérence (font foi, priment sur les sous-modules)
## Répartition
RÉPARTITION TRANCHÉE (Commandeur #76, livré dans le bloc du front)

FORME
- Aucun composant neuf pour le Commandeur. Tout est fait de classes serveur, tenues par SRP_FrontEnemyComponent (composant déjà prévu par le front, GUID {6A5B0C0D0E0FF004}) dans son membre m_Commander.
- Point d'accès unique : SRP_Commander.Get(). Il rend null si le Commandeur est absent ; les accroches statiques ne font alors rien.
- La seule RPC est le son de départ du mortier, plus la relance d'obus en repli. Elle est sur SRP_FrontEnemyComponent, qui existe sur toutes les machines (modèle SRP_Sirene.c:504-515).
- Noms abandonnés : SRP_CommanderComponent, SRP_EnemyCommand, SRP_CmdMeans, SRP_CmdRegionMeans, SRP_CmdStocks, SRP_CmdBudget, SRP_EnemyRegion, SRP_EnemyOfficer(s), SRP_EnemyStocks, SRP_EnemyManeuver, SRP_EnemyMortar, SRP_EnemyHeavy, SRP_EnemyArmor.

DIX FICHIERS NEUFS, UN PROPRIÉTAIRE CHACUN
1. SRP_CmdSettings.c : le SEUL lecteur de $profile:SimpleRP/commandeur_reglages.txt (classe statique SRP_CmdSettings).
   - Format : « clé = valeur   # aide ».
   - Clés en minuscules, avec tiret bas et préfixe de section : cmd_, info_, alerte_, officier_, desorg_, res_, cap_, renfort_, colonne_, repli_, assaut_, harcelement_, reorg_, ca_, nuit_, appui_, vu_.
   - Fonctions : Declare, GetInt, GetFloat, GetBool, GetString, GetList, GetIntClamped, Reload(auteur).
   - Une seule clé par chiffre. Une clé manquante est ajoutée en fin de fichier.
   - Proposé : le même lecteur lit aussi un front_reglages.txt (arbitrage Q6).
2. SRP_Commander.c : le cerveau.
   - Il déclare toutes les énumérations SRP_ECmd* et les classes partagées SRP_CmdOrder et SRP_CmdContact.
   - Il tient : les comptes rendus, la connaissance, les informateurs, la gravité MA9 (seule formule), l'humeur et les cycles rapide et lent.
   - Il choisit TOUTES les cibles d'appui : mortier, artillerie AP2, bombe AP3, blindé AP5, hélico AP7, enquête.
   - Il décide de la taille du renfort demandé par l'officier.
   - Il porte aussi : le gel VU6, ForceOrder (seule entrée du Staff, VU5), CancelOperations, OnMissionSucceeded, OnSabotageUsed, la sauvegarde chaînée, ResetCampaign et OnRestored.
3. SRP_CmdRegions.c : régions (OF1, OF11), alerte CO9 et menace locale, officiers (OF2 à OF10, avec un numéro unique m_iSerial). Textes par SRP_FrontRadio.
4. SRP_CmdResources.c : les stocks (C1, RE2 à RE10).
   - Dépense par tickets : Reserve, Consume, Release. C'est la seule restitution des moyens au redémarrage.
   - Aussi : renflouement, quart de stock retiré par une mission, CA7 (seule implémentation), GetFillRatio(region).
5. SRP_CmdDepot.c : dépôts (RE8, RE9).
6. SRP_CmdCapacity.c : 120 soldats et 160 IA actives, classes, file RE12, partage des localités, TagImportance (seul appel à SetImportance du mod).
   - Toujours active, même Commandeur gelé : c'est la règle Q7 du front.
   - Ni frein C2, ni registre de renforts, ni sauvegarde.
7. SRP_CmdManeuvers.c : toute l'infanterie qui bouge.
   - Camion de contact : il remplace le camion d'alerte #69 tant que le Commandeur n'est pas gelé.
   - Camion d'entrée et colonnes MA4.
   - SEUL détenteur du frein C2 et du registre des soldats envoyés.
   - Aussi : décrochage, doctrine d'assaut, harcèlement, réorganisation, poches et unités sur le papier.
   - Contre-attaque CA1 à CA6 : le soutien CA4 et la feinte MO8 sont décidés ici.
8. SRP_CmdSupport.c : exécute les feux et l'hélico.
   - Mortier et rideau (MO1 à MO9), artillerie 2S1, aviation, hélico.
   - Il applique EQ3, EQ4, MO6, la sécurité et les interdictions de 24 h.
   - Il ne choisit aucune cible et ne tient aucun registre « en route ».
9. SRP_CmdArmor.c : blindés (AP4 à AP6, C3), et modded SCR_GetInUserAction pour verrouiller le 2S1.
10. SRP_CmdScreens.c :
   - SRP_CmdLog : le seul journal des décisions, 200 lignes, catégorie COMMANDEUR ;
   - SRP_CmdIntel : renseignement VU2 par région, révélation des dépôts RE9 ;
   - SRP_CmdScreens : onglet Staff et morceaux du PC.

NOMS FIGÉS (extrait)
- Régions : GetRegionCount, GetRegionOfZone, GetRegionAt, GetRegionCode, GetRegionName, GetRegionLabel (« région de » ou « région d' »), FindRegion(code), GetRegionState, IsRegionDisorganized, GetRegionStartZones, GetRegionAlert, AddRegionAlert, static AddAlertAt, static ThreatAt, static GarrisonFactorAt, GetRegionMood, GetOfficerTrait, GetOfficerName, GetOfficerSerial, GetOfficerHideout, HasLivingOfficer.
  • GetOfficerTrait rend un SRP_ECmdTrait : prudent, audacieux, lent, ou méthodique (neutre). SRP_ECmdTemper est supprimé.
- Connaissance : GetContacts(region, maxAgeS, result), GetBestContact, KnownPlayersNear(pos, radius, maxAgeS), IsContactConfirmed, IsHeavySupportJustified, GetZoneShots, GetZoneLosses, WasArmedVehicleSeen, IsPlayerWatchedNear, GetZoneGravity, IsServedZone.
  • GetDosingPlayers sert à EQ4 seulement. Il vaut capture.CountValidPlayersNear(pos, 3000).
- Stocks : GetStock(kind, region) et GetFull(kind, region), dans cet ordre (kind puis region). Aussi Reserve, Consume, Release, GetFillRatio(region) (−1 = moyens lourds de l'île), IsOffensiveAllowedByStocks, OffensiveChanceBonus.
- Place : SRP_CmdCapacity.Get(), avec Room(cls, owner), Refusal(cls, soldiers, groups, owner), NoteDemand, ClearDemand, SetWantedGarrisons, GarrisonCap.
- Manœuvres : CanReinforceZone(zone), RequestReinforcement(zone, aim, size, reason, author, staff), RequestEntryTruck, crochets de contre-attaque des manœuvres, IsLocalityEmptied, IsKeptFull, IsOnPlayersPath.
- Appuis : RequestMortar(region, aim, shell, shells, reason, staff), RequestSmoke(region, from, threat, reason, staff), RequestArtillery, RequestBomb, RequestFlyby, RequestArmor(region, target, armorReason, zone, variant, staff), RequestHeli, IsBigOperationRunning(attachedZone), EndAssault(zone), IsMortarReady, IsArtilleryReady, et des états lisibles par le Staff.
- Front ennemi : GetLocalityNominal, GetLocalityEffective, AddLosses, AddSent, ReturnSent, ClearSent, AddBonus, GetZoneStrength (simple dérivé), IsLocalityUnderAttack, IsPostPossibleNearPlayers, GetCounterAttackZone, IsInMainRedMass.
- Zones vers ennemi : OnZoneCaptured(int zone, bool staff), OnZoneLost(int zone, int reason, bool staff), OnLocalityTaken(string name).
- Radio : SRP_FrontRadio est le seul auteur. Fonctions : OfficerFell, RegionLiberated, IntelGained, EnemyDepotSabotaged, SupportSilenced, ArmorBroughtBack, AttackBroken, AttackCalledOff.
- Évènements envoyés par PushZoneEvent : officier_tombe, region_liberee, renseignement, depot_ennemi, blinde_ramene. Jamais « commandement ».

OÙ VIT L'ÉTAT
- Seulement dans front.json, par une chaîne unique : SRP_FrontComponent.Save → SRP_FrontEnemyComponent.WriteTo(ctx) → SRP_Commander.WriteTo(ctx).
- Clés, toutes à plat : cmd_gel ; cmd_r* (régions, officiers, alertes, rattachements) ; cmd_res_* (stocks, coupures, tickets ouverts, renflouements, bombe, niveau CA7) ; cmd_dep_* (dépôts) ; cmd_ma_* (dernier harcèlement, dernière contre-attaque, colonnes en route) ; cmd_ap_* (interdictions de 24 h, plafonds EQ3) ; cmd_vu_* (renseignement).
- Les champs F12 étendus sont sur SRP_FrontLocality : lN_sent, sentAt, bonus, bonusAt, emptied, full.
- Aucun extra, aucun fichier à part.
- Chargement : BuildRegions entre la géométrie et Load, puis ReadFrom après les zones et les localités.
- Restitutions au redémarrage : les tickets ouverts (ressources) et les colonnes et envoyés (manœuvres), rien d'autre.
- RestoreDaily : ReadFrom(copie), puis OnRestored.
- Non sauvés (H3) : connaissance, comptes rendus, ordres, frein C2, combats.

FLUX (appels directs, aucun ScriptInvoker)
- SRP_ZoneFall appelle dans l'ordre : missions → économie → SRP_FrontEnemyComponent → pont → radio. L'ennemi transmet au Commandeur (alerte, libération ou rattachement, cachettes), qui prévient les ressources (OnZoneOwnerChanged), les dépôts et les manœuvres (MA11).
  • Zone forcée par le Staff : l'état est mis à jour, sans alerte ni annonce (Q9).
- Renfort :
  1. le rapport d'une unité alimente la connaissance du cerveau ;
  2. réflexe de l'officier : taille calculée sur les joueurs ESTIMÉS, +75 s si l'officier est lent ;
  3. SRP_CmdManeuvers.RequestReinforcement : frein C2, choix de la source, ×2 si la région est désorganisée, arrivée en 3 à 6 min ;
  4. camion ou colonne, puis Refusal(COMBAT) de la capacité.
- Appuis : les cycles du cerveau appellent SRP_CmdSupport.Request*, qui applique EQ3, EQ4, MO6, la sécurité et Reserve. En contre-attaque : SRP_CmdManeuvers.Support appelle les mêmes Request*.
- Faits visibles des joueurs :
  • saisie D4 → SRP_CmdIntel.OnKeyPointSeized, avant le changement de camp ;
  • OfficerDown → SRP_CmdIntel.OnOfficerCaptured ;
  • End(SUCCES) → SRP_Commander.OnMissionSucceeded ;
  • en tête de OnObjectiveUsed : SRP_Commander.OnSabotageUsed.
- Horloge : tout part du Tick de 5 s de SRP_FrontEnemyComponent, en heure Unix. CallLater ne sert qu'à enchaîner les coups de feu. OnPostInit ne fait que créer les objets ; le reste attend le Start différé.

VALEURS TRANCHÉES ENTRE SOUS-MODULES
- Frein C2 : un renfort par zone toutes les 10 min.
- La source garde la moitié de son effectif nominal, les deux tiers pour un officier prudent.
- Désorganisation : ×2 appliqué par les manœuvres seules.
- Officier lent : +75 s, appliqué par le cerveau seul.
- Appui lourd justifié : joueur vu depuis moins de 5 min, ou pertes depuis moins de 10 min.
- Pour viser : contact vu depuis 120 s au plus.
- Hélico : si aucune unité n'est à 800 m.
- Bombe : 10 joueurs valides à 3 km, 24 h glissantes.
- Décision forcée par le Staff : gratuite (cmd_staff_gratuit = 1).
- Journal : 200 lignes.
- VU2 « renforcées / normales / affaiblies » = état des troupes (effectif prévu sur nominal).
- Renseignement par région, déclenché sur un carré rouge.
- Soldat ennemi tué : reconnu par son groupe enregistré (FindRecord), jamais par la clé de faction.
## Trous tranchés
- 1. [Bloquant, ne compile pas] Le cerveau a trois noms.
- SRP_Commander : une classe tenue par SRP_FrontEnemyComponent (sous-module cerveau).
- SRP_CommanderComponent : ressources, manœuvres et appuis. Les appuis le veulent même en composant présent sur toutes les machines, pour leurs RPC.
- SRP_EnemyCommand : sous-module Staff, avec des fichiers qu'aucun autre sous-module n'écrit (SRP_EnemyCommand.c, SRP_EnemyOfficers.c, SRP_EnemyStocks.c, SRP_EnemyManeuver.c, SRP_EnemyMortar.c, SRP_EnemyHeavy.c, SRP_EnemyArmor.c).
TRANCHÉ :
- classe SRP_Commander, accès SRP_Commander.Get(), tenue par SRP_FrontEnemyComponent.m_Commander ;
- aucun composant neuf ;
- 10 fichiers neufs SRP_Cmd* ;
- la RPC de son (MO3) va sur SRP_FrontEnemyComponent.
- 2. [Bloquant] Des classes attendues que personne n'écrit.
- SRP_CmdMeans (Execute, CanExecute…) et SRP_CmdRegionMeans : le cerveau en a besoin pour exécuter ses ordres et sauver la part des moyens.
- SRP_CmdStocks, SRP_CmdBudget.SoldierRoom et SRP_ECmdPriority, attendus par les manœuvres.
- HasRoomFor et GetActiveAttackZone, attendus par les appuis.
TRANCHÉ :
- pas de répartiteur. Le cerveau appelle directement SRP_CmdManeuvers (RENFORT, COLONNE, DECROCHAGE, HARCELEMENT, ENQUETE) et SRP_CmdSupport (MORTIER, FUMEE, FEINTE, ARTILLERIE, BOMBE, AVION, BLINDE, HELICO) ;
- stocks : SRP_CmdResources ;
- place : SRP_CmdCapacity (Room et Refusal) ;
- zone attaquée : SRP_FrontEnemyComponent.GetCounterAttackZone() ;
- la part d'une région est SRP_CmdRegionSupply, tenue par les ressources et non par SRP_CmdRegion ;
- ENQUETE (unité mobile la plus proche, OrderSearch), que personne n'exécutait, va aux manœuvres.
- 3. [Bloquant, ne compile pas] Doubles déclarations.
- class SRP_CmdAssault : écrite par les manœuvres (plan de vague) et par les appuis (appui d'un assaut).
- class SRP_CmdContact : les champs diffèrent. Cerveau : m_iFirstUnix, m_bSeen, m_bInformantOnly, m_bArmed. Appuis : m_iFirstSeenUnix, m_bBySoldier, m_bArmedVehicle.
- Le fichier SRP_CmdSettings.c est revendiqué par deux classes : SRP_CmdSettings (cerveau) et SRP_CmdSettingsFile (ressources).
- Énumérations qui font double emploi : SRP_ECmdOrder / SRP_ECmdSupport, SRP_ECmdTrait / SRP_ECmdTemper, SRP_ECmdCapClass / SRP_ECmdPriority.
TRANCHÉ :
- SRP_CmdAssault des manœuvres. L'état d'appui va dans SRP_CounterAttack : m_bFeint, m_bSupportMortar, m_bSupportHeavy ;
- SRP_CmdContact du cerveau, plus m_iRegion ;
- toutes les énumérations sont déclarées dans SRP_Commander.c ;
- SRP_ECmdOrder fusionné : RENFORT, MORTIER, FUMEE, FEINTE, ARTILLERIE, BOMBE, AVION, BLINDE, HELICO, ENQUETE, HARCELEMENT, COLONNE, DECROCHAGE ;
- SRP_ECmdSupport, SRP_ECmdTemper et SRP_ECmdPriority sont supprimés.
- 4. [Bloquant, bug silencieux] Stocks.
- Ordre des paramètres : GetStock(kind, region) chez les ressources, GetStock(region, kind) chez les appuis. Ce sont deux entiers : ça compile et ça lit le mauvais stock.
- Double remboursement : les appuis dépensent par TakeStock et GiveStock, avec leur propre registre « en route » (m_iOut*), rendu au redémarrage. Les ressources rendent aussi leurs tickets au redémarrage. Chaque redémarrage rembourse donc deux fois.
TRANCHÉ :
- ordre (kind, region) ;
- seuls les tickets des ressources (Reserve, Consume, Release) ;
- les appuis ne gardent que des numéros de ticket ;
- la clé appui_en_route est supprimée.
- 5. [Bloquant pour l'arbitrage C2] Le frein des renforts est codé trois fois.
- Cerveau : CanReinforceZone.
- Capacité : CanReinforce et m_mLastReinforcementAt.
- Manœuvres : m_mLastRenfort et PickSource.
De plus, le cerveau garde le camion d'alerte #69, que les manœuvres remplacent par le camion de contact : deux camions partiraient pour un même contact.
TRANCHÉ :
- les manœuvres sont seules à tenir C2 : 10 min par zone, en heure Unix, non sauvé (H3) ;
- la source garde la moitié de son effectif NOMINAL, les deux tiers pour un officier prudent ;
- le camion de contact remplace le camion d'alerte tant que le Commandeur n'est pas gelé ;
- le camion d'entrée passe par RequestEntryTruck, avec le même frein ;
- le cerveau interroge CanReinforceZone avant d'émettre un ordre.
- 6. [Élevé] Les soldats partis sont comptés deux fois.
- Ressources : un renfort envoyé compte comme une PERTE F12 (AddLocalityLosses), et leur liste SRP_CmdReinforcement est rendue au redémarrage.
- Manœuvres : un compteur séparé m_iSent, indispensable à MA11 (on rappelle les envoyés, pas les tués), et des colonnes rendues elles aussi au redémarrage.
TRANCHÉ :
- les champs des manœuvres, sur SRP_FrontLocality : m_iSent, m_iSentAt, m_iBonus, m_iBonusAt, m_iEmptiedAt, m_iFullUntil ;
- la capacité ne tient pas de registre ;
- RetireSectionFor est supprimé : la source n'est jamais une garnison posée (MA2).
- 7. [Élevé] L'effectif d'une localité a trois noms.
- Effective et Nominal (manœuvres).
- GetLocalityFullStrength et GetLocalityStrength (ressources).
- GetZoneStrength(out planned, out nominal) (Staff).
De plus, GarrisonSoldiers est protected (SRP_Territory.c:1091).
TRANCHÉ :
- GetLocalityNominal = GarrisonSoldiers rendu public : base + joueurs + menace LOCALE, ×1,25 si l'officier vit, plafond 60 ;
- GetLocalityEffective = Nominal − pertes − envoyés + reçus, borné entre 0 et 60 ;
- pose = min(Effective, GarrisonCap) ;
- GetZoneStrength devient un simple dérivé pour le PC.
- 8. [Élevé] Un seul fichier de réglages, quatre lecteurs, deux syntaxes.
- Cerveau : Declare, « clé = valeur ».
- Ressources : lecteur statique, avec des bornes par clé.
- Manœuvres : CmdInt, clés à point (« renfort.frein_minutes »).
- Appuis : leur propre lecteur, lu seulement au démarrage.
- Staff : « clé valeur », sans signe égal.
Chiffres réglés deux fois ou plus : le frein (3 clés), la garde prudente (60 ou 67 centièmes), la distance de l'hélico (800 ou 600 m), la bombe, la taille du journal (40 ou 200), la gratuité des décisions forcées (4 clés).
TRANCHÉ :
- SRP_CmdSettings, format « clé = valeur   # aide », clés à tiret bas avec préfixe de section ;
- une clé par chiffre ;
- Reload relit tout, sauf la structure des régions, prise au redémarrage ;
- chaque sous-module déclare ses clés et les relit dans LoadSettings().
- 9. [Élevé] Le soutien d'une contre-attaque (CA4) et la feinte (MO8) sont codés deux fois.
- Manœuvres, Support : mortier 120 s avant l'assaut ; artillerie si 4 joueurs sont groupés sur 70 m, sinon blindé ; feinte sur l'axe B.
- Appuis, SupportAssault : artillerie ou blindé tiré à parts égales ; faux axe tourné de 90 degrés ; passage d'avion.
TRANCHÉ :
- la décision revient aux manœuvres, qui connaissent l'horaire et les axes ; les appuis exécutent par leurs Request* ;
- la feinte passe par le 2e axe réel ;
- le passage de Su-57 (1 contre-attaque sur 2) est ajouté à Support ;
- SupportAssault, PickMortarTarget, PickStrongpoint et PickBombTarget sont retirés des appuis : c'est le cerveau, qui a la connaissance, qui vise.
- 10. [Élevé] Deux calculs codés deux fois.
- Offensive de nuit CA7. Ressources : niveau noté au départ du dernier joueur, seuils de 95 et 33 centièmes. Manœuvres : NightSnapshotTick, seuils de 1,0 et 0,34.
- Gravité MA9, avec deux barèmes. Cerveau : 5 / 3 / 1 / 10 tirs / 6. Manœuvres : 50 / 8 / 5 / 20 / 3.
TRANCHÉ :
- CA7 aux ressources seules, lu par OffensiveTick ;
- gravité au cerveau seul (GetZoneGravity, IsServedZone), avec en plus le terme « contre-attaque en assaut ».
- 11. [Élevé] L'hélico a deux portes de sortie.
- Le cycle rapide du cerveau : joueurs VUS depuis 120 s, aucune unité à 800 m.
- MaybeCallHeli rebranché sur RequestHeli : personne à 600 m.
TRANCHÉ :
- seule la porte du cerveau, avec 800 m ;
- MaybeCallHeli ne lance plus rien tant que le Commandeur existe ;
- Commandeur gelé : pas d'hélico.
- 12. [Élevé] Des ralentissements appliqués deux fois.
- Région désorganisée : ×2 dans l'ordre du cerveau ET ×2 dans les manœuvres, donc ×4.
- Officier lent : +75 s chez le cerveau ET arrivée ×1,5 chez les manœuvres.
TRANCHÉ :
- le ×2 est appliqué par les manœuvres seules ;
- l'officier lent = +75 s, appliqué par le cerveau seul.
- 13. [Élevé] Journal, textes radio et évènements en plusieurs exemplaires.
- Journal : 5 versions.
- Radio : Announce* (cerveau), MortarSilenced, BatteryDestroyed et ArmorDelivered (appuis), EnemyDepotRevealed, EnemyDepotFell et EnemyStockHit (ressources), plus OfficerFell et les autres (Staff).
- Le type d'évènement « commandement » des appuis irait au salon commandement, réservé aux captures sans mission.
- Salon Staff « commandeur » ou « etat-major ».
TRANCHÉ :
- SRP_CmdLog (200 lignes) est le seul journal ;
- les textes du Staff, plus AttackBroken et AttackCalledOff ;
- 5 types d'évènements, envoyés par PushZoneEvent ;
- flux etat-major, avec repli sur journal-serveur.
- 14. [Élevé] Renseignement et dépôt : trois chemins pour un même fait.
- Saisie D4 : révélation directe du dépôt (ressources) ET OnKeyPointSeized (Staff).
- Officier capturé : trois appels différents.
- Fin de mission : trois ajouts dans End. L'alerte de sabotage est comptée deux fois.
- « Renforcée / normale / affaiblie » a deux sens : les troupes (Staff) et les stocks (ressources).
TRANCHÉ :
- une entrée par fait : ZoneFall → SRP_CmdIntel.OnKeyPointSeized ; OfficerDown → SRP_CmdIntel.OnOfficerCaptured ; End → SRP_Commander.OnMissionSucceeded, qui répartit :
  • renseignement si le carré est rouge ;
  • dépôt révélé pour une mission ÉCOUTE ou DOCUMENTS ;
  • quart de stock retiré pour SABOTAGE ou CACHE ;
  • alerte comptée une seule fois ;
- VU2 parle des troupes, comme la réponse de Jack. L'état des stocks reste réservé au Staff.
- 15. [Élevé] Le Staff a quatre entrées pour forcer une décision.
- ForceOrder (cerveau), ForceSupport (appuis), Force* (manœuvres), ForceMeans(string kind…) (Staff).
- Le gel s'appelle IsFrozen ou IsActive selon le sous-module.
- CancelOperations (appelé par ia-reset et wipe) et GetOfficerSerial n'existent nulle part.
TRANCHÉ :
- SRP_Commander.ForceOrder(int kind, int variant, vector pos, string author) est la seule entrée ; variant désigne par exemple le BTR-70, le BRDM-2 ou le Typhoon ;
- IsFrozen et SetFrozen ;
- ForceOfficerDown et ForceOfficerReplace ;
- CancelOperations et GetOfficerSerial sont ajoutés au cerveau.
- 16. [Élevé] La sauvegarde passe par cinq mécanismes.
- Contexte JSON direct, extras SetExtra et champs de localité.
- Les manœuvres lisent leur état dans l'OnPostInit du composant ennemi, 5 s avant que le socle charge front.json.
- RestoreDaily appelle ResetAll des manœuvres, qui efface l'état qu'on vient de restaurer.
TRANCHÉ :
- une chaîne unique : Save → FrontEnemy.WriteTo(ctx) → Commander.WriteTo(ctx), clés cmd_* ;
- ReadFrom après BuildRegions et après les localités ;
- RestoreDaily : ReadFrom, puis OnRestored ;
- aucun extra pour le Commandeur.
- 17. [Moyen] Violations de CO5 : le Commandeur lit la vraie position des joueurs.
- Taille du camion de contact par LoadFor, sur les joueurs réels à 1 000 m.
- renfort.joueurs_min compté sur les joueurs réels.
- Cible du harcèlement : le carré le plus proche d'un joueur.
TRANCHÉ :
- taille du renfort = formule du cerveau sur les joueurs ESTIMÉS ;
- harcèlement seulement sur un contact connu ;
- le camion d'entrée #69 garde LoadFor (réflexe voté, CO2) ;
- le seul nombre réel est GetDosingPlayers (EQ4), qui passe par la capture.
- 18. [Moyen] Capacité : les classes sont mal devinées.
- ClassOf range l'officier (propriétaire « territoire », tâche « garde officier ») en GARNISON.
- Les servants et les équipages (propriétaire « appui ») aussi.
- L'importance est posée à trois endroits.
TRANCHÉ :
- chaque pose du Commandeur écrit m_iCapClass : GARDE pour l'officier, ses gardes, le dépôt et la batterie ; COMBAT pour les servants, les équipages, les vagues et les renforts ; PATROUILLE pour les raids ;
- TagImportance est le seul appel à SetImportance.
- 19. [Moyen] Moment de la contre-attaque.
- Le cerveau la lance depuis son cycle lent (toutes les 15 à 20 min).
- Or CA1 demande de noter le moment chaque minute, de D − 20 à D + 20.
TRANCHÉ :
- CaWindowDecision est appelé chaque minute par AttackClock ;
- OnCommanderSlowCycle est supprimé.
- 20. [Moyen] Noms du socle mal repris.
- GetZone(i).GetCode() : il faut lire les champs de SRP_FrontZone.
- m_vKeyPoint2 : il faut GetKeyPointPos, avec le rôle QG (relecture n°13, arbitrage Q8).
- GetZoneAt.
- Capture en composant : c'est une classe du socle (relecture n°23).
- Signatures de LoseZone et OnZoneCaptured variables.
TRANCHÉ :
- noms de la grille ;
- accès à la capture : SRP_FrontComponent.GetInstance().GetCapture() ;
- OnZoneCaptured(zone, staff), OnZoneLost(zone, reason, staff), OnLocalityTaken(name).
- 21. [Moyen] Signatures du mod mal citées, vérifiées dans le code.
- HasLineOfSight(vector, vector, IEntity, bool) prend des positions, pas des entités (SRP_Placement.c:66).
- BasePosition(out vector) rend un bool (l.29).
- NearestPlayer rend une distance (l.208).
- TryRadioCall est une méthode d'instance (SRP_Territory.c:1268).
- SpawnHeliPilots, PreventLod et FlankPoint sont protected.
- Cinq exclusions sont ajoutées à la même ligne d'IsFree.
TRANCHÉ : on corrige à l'écriture, et IsFree est retouché une seule fois.
- 22. [Moyen] Pertes ennemies peut-être invisibles.
- Le cerveau reconnaît un soldat ennemi tué par IsEnemyFactionKey sur la liste « USSR,FIA,AFRF » (SRP_GameMode.et:191).
- La comparaison est exacte (SRP_Treasury.c:960-973), alors que RHS déclarerait « RHS_AFRF ».
TRANCHÉ :
- une victime est un soldat ennemi si son groupe est enregistré (SRP_EnemyComponent.FindRecord, SRP_Enemy.c:4207) ;
- aucune clé de faction ;
- IsEnemyFactionKey n'est pas ajouté.
- 23. [Faible] Trop de minuteries.
- Chaque sous-module a son CallLater : ressources 60 s, capacité 5 s, appuis 2 s.
TRANCHÉ :
- tout part du Tick de 5 s de l'ennemi, avec des sous-cadences ;
- rien d'autre qu'une création d'objets dans OnPostInit (instance jetable du Workbench).
## Changements du plan du front
- GRILLE (socle), démarrage :
- avant : la géométrie, puis Load ;
- après : la géométrie, puis SRP_Commander.BuildRegions, puis Load.
- GRILLE, sauvegarde. Avant : zones, localités, points clés et extras. Après :
- une chaîne unique SRP_FrontEnemyComponent.WriteTo/ReadFrom(ctx) pour tout le Commandeur (clés cmd_*, environ 4 Ko) ;
- 6 champs de plus par localité (envoyés, reçus, vidée, tenue au complet) ;
- RestoreDaily fait ReadFrom(copie) puis OnRestored ;
- ResetCampaign passe par l'ennemi, qui prévient le Commandeur ;
- AddThreat prévient les ressources (OnThreatChanged) ;
- front_retouches.txt accepte « region S12 -> R3 » ;
- MarkDirty(bool) devient public.
- GRILLE, réglages :
- avant : attributs de prefab ;
- après (proposé, arbitrage Q6) : les chiffres clés sont lus par SRP_CmdSettings dans le profil.
- CAPTURE :
- avant : seuls les soldats marqués m_iAssaultZone reprennent des carrés ;
- après : les assaillants sur le papier comptent aussi (SetPaperAssault et ClearPaperAssault ; CountFitAssaultersInZone les inclut) ;
- accès GetCapture() pour IsPlayerCounted et CountValidPlayersNear : c'est le seul nombre réel de joueurs lu par le Commandeur (EQ4).
- ZONES (SRP_ZoneFall). Après :
- même ordre d'appels qu'avant. L'ennemi transmet au Commandeur : alerte, libération ou rattachement, cachettes, ressources, dépôts, réorganisation MA11 ;
- signatures OnZoneCaptured(zone, staff) et OnZoneLost(zone, reason, staff) ;
- saisie D4 : SRP_CmdIntel.OnKeyPointSeized, AVANT le changement de camp ;
- zone forcée par le Staff : aucun effet visible du Commandeur (Q9).
- ENNEMI, composant :
- avant : pas de cerveau, boucle de 5 s en GetTickCount ;
- après : il porte tout le Commandeur et le pilote en heure Unix ;
- il surcharge OnControllableDestroyed (vérifié : SCR_BaseGameModeComponent.c:157) ;
- il porte la RPC du son du mortier.
- ENNEMI, capacité.
- Avant : 100 soldats (m_bBudget), 30 groupes, réserve de 12 soldats pour les postes, vagues, gardes et patrouilles hors plafond, GetGlobalSoldierReserve et PostReserveLeft.
- Après : 120 soldats au sol, tout compris :
  • 20 places gardées aux renforts et 12 aux postes ;
  • 36 groupes, dont 6 aux missions, 5 aux renforts et 4 aux postes ;
  • 160 IA actives, par SetLimitOfActiveAIs (vérifié : AIWorld.c:21) ;
  • file de priorité RE12 et partage entre localités.
- Les deux fonctions de réserve disparaissent.
- ENNEMI, garnisons (F11, F12).
- Avant : base + joueurs + 0,5 × menace de l'île, moins les pertes.
- Après :
  • Nominal = base + joueurs + 0,5 × menace LOCALE, × 1,25 si l'officier vit, 60 au plus ;
  • Effective = Nominal − pertes − envoyés + reçus ;
  • pose = la plus petite valeur entre Effective et la part donnée par la capacité ;
  • une localité vidée (MA7) n'est ni posée ni complétée ;
  • la localité suivante est tenue au complet pendant 6 h (MA11).
- ENNEMI, camions du #69.
- Avant : camion d'entrée à 500 m (2 à 4 min) ; camion d'alerte dès 5 joueurs ; départ d'une couronne de sondes.
- Après, Commandeur actif :
  • camion de contact dès le premier contact, à la place du camion d'alerte ;
  • camion d'entrée soumis au même frein C2 ;
  • départ d'une localité SOURCE non posée, débitée des soldats envoyés ;
  • arrivée en 3 à 6 min, deux fois plus lente si la région est désorganisée ;
  • colonne sur le papier si la source est trop loin.
- Commandeur gelé : les règles d'aujourd'hui.
- ENNEMI, contre-attaques (F1 à F6).
- Avant : échéance fixe ; 70 fois sur 100 la dernière zone prise, sinon au hasard ; 3 vagues ; AdvanceAssault ; vague réputée arrivée au bout de 10 min ; rupture à 60 min.
- Après :
  • fenêtre D ± 20 min, notée chaque minute, 45 min au moins entre deux ;
  • cible sur renseignements (CA2) ;
  • nombre de vagues révisé de 2 à 4 ;
  • attaque de face, par le flanc ou sur deux axes ;
  • soutien CA4 et feinte ;
  • plus de vague « réputée arrivée » ; vagues sur le papier ;
  • abandon aux deux tiers de pertes ;
  • horloge en secondes Unix.
- ENNEMI, offensive de nuit (H2) :
- avant : 10 + 5 × menace, 60 au plus ;
- après : + 10 si les stocks étaient pleins au départ du dernier joueur, et pas d'offensive s'ils étaient sous le tiers (CA7, calculé par les ressources).
- ENNEMI, hélico (G12) :
- avant : tirage (10 le jour, 25 la nuit, +15 à chaque raté) ;
- après : décision du cerveau (AP7), sur son stock de sorties ;
- la question G12 du plan devient sans objet.
- ENNEMI, patrouilles (F10) et jeeps :
- avant : bornées seulement par le plafond de groupes ;
- après : elles passent par Refusal(PATROUILLE), cèdent leur place hors de vue quand un renfort attend, et suivent la menace locale.
- ENNEMI, postes (F8) :
- avant : les meilleurs carrés du front ;
- après : les carrés sur le chemin des joueurs passent d'abord (MA11), avec 4 soldats.
- ENNEMI, SRP_Enemy.c.
- Nouveaux ordres : OrderSuppress, OrderAttackPoint, OrderSmokeCover, OrderForcedMove, OrderRetreat, OrderApproach, HoldAwake.
- Nouvelles fonctions : SpawnVehicleCrew, CountPendingSpawn, ReleaseForRoom, SetMaxGroups.
- Accroches : NoteSighting, NoteShot, NoteAirSighting.
- Champs Cmd sur SRP_EnemyGroup.
- IsFree exclut en plus les gardes de l'officier, les servants d'appui, les replis et les rôles donnés par le Commandeur.
- MISSIONS.
- Mission OFFICIER. Avant : un officier isolé, tiré au hasard. Après : l'officier de région, à sa cachette du jour (OF10).
- Gardes de mission en classe GARDE, vagues en COMBAT.
- End(SUCCES) appelle OnMissionSucceeded, une seule fois.
- OnObjectiveUsed teste d'abord le dépôt ennemi et la pièce de mortier.
- Civils : GetInformantCandidates, importance HIGH ; G14 ne change pas.
- Livraisons : passent par Refusal.
- CARTE.
- 8 annonces de plus : OfficerFell, RegionLiberated, IntelGained, EnemyDepotSabotaged, SupportSilenced, ArmorBroughtBack, AttackBroken, AttackCalledOff.
- 8e onglet Staff « Commandeur ». L'en-tête et l'onglet Territoire montrent les deux interrupteurs.
- PC : « région de X » sur chaque zone ennemie, et une section Renseignement.
- Carte : repère du dépôt ennemi une fois révélé.
- ia-reset et wipe appellent CancelOperations.
- WEB.
- Flux etat-major : salon Staff seul, repli sur journal-serveur ; la catégorie COMMANDEUR n'est jamais publique.
- 5 évènements de plus.
- bot.py : styles des nouveaux évènements (ALERT_STYLES).
- setup_discord : le nouveau salon.
- SRP_JournalComponent n'envoie plus les lignes COMMANDEUR dans l'onglet Journal du PC.
- MIGRATION.
- Avant : 10 étapes, socle compilé seul, 3 séances d'essai.
- Après :
  • 14 étapes globales ;
  • contrats du Commandeur figés dès l'étape 1 ;
  • capacité compilée avec le socle ;
  • cerveau d'un coup, puis moyens d'un coup (LI2) ;
  • 4 séances d'essai, dont un essai à deux ;
  • contrôles k à o, plus une recherche des doublons ;
  • prefab SRP_Piece_Mortier.et avec son .meta ;
  • check-list d'essai 12 à 31 ;
  • dossier .claude-flow à retirer.
## Ordre de codage
- 0. Préparation :
- sauvegardes (plan migration) ;
- cartes #75 et #76 « En cours » ;
- retirer .claude-flow de la racine du mod ;
- étendre verif_front.py : contrôles a à o, plus la recherche des doublons (GetStock, Reserve, AddAlert, SetImportance, PushEvent, NotifyAll).
- 1. Contrats figés, corps vides, compilés seuls :
- SRP_Front.c : énumérations et API du socle ;
- SRP_FrontRadio.c : signatures ;
- SRP_Commander.c : toutes les énumérations SRP_ECmd*, SRP_CmdOrder, SRP_CmdContact, et une API qui rend les valeurs votées du front ;
- SRP_CmdRegions, SRP_CmdResources, SRP_CmdDepot, SRP_CmdCapacity, SRP_CmdManeuvers, SRP_CmdSupport, SRP_CmdArmor (sans le modded), SRP_CmdScreens ;
- SRP_CmdSettings.c complet.
La liste des signatures est figée (contrôle m).
- 2. Socle :
- géométrie ;
- SRP_FrontKeyPoint et les 2 prefabs repère (arbitrage Q8) ;
- front.json v2 avec la chaîne WriteTo/ReadFrom, vide pour le Commandeur ;
- migration K1 et K2.
Compiler. Séance courte : front_zones.txt et migration à blanc.
- 3. Capacité complète : 120 soldats, 160 IA, classes, file RE12, TagImportance.
La brancher sur toutes les poses existantes : Enemy, EnemyTrucks, EnemyGarrison, Missions, Civilians, Delivery.
Compiler après chaque fichier. C'est l'arbitrage Q7 du front.
- 4. Régions : tracé OF1, lignes « region » du fichier de retouches, rapport commandeur_regions.txt. Pas encore d'officiers. Compiler.
- 5. Capture, SRP_ZoneFall et économie, avec leurs appels vers les contrats neutres : OnZoneCaptured(zone, staff), OnKeyPointSeized, OnMissionSucceeded. Compiler fichier par fichier.
- 6. Bascule des appelants (étapes 2 à 4 du plan migration) :
- SRP_Territory réduit aux localités, avec GarrisonSoldiers public, Nominal et Effective ;
- puis Enemy, HeliSearch, Sirene, EnemyTrucks, Missions, MissionRequests, Bridge, PC, Admin, ChatCommands ;
- Discord (flux etat-major) ;
- JournalComponent (filtre COMMANDEUR).
Compiler après chaque fichier.
- 7. Ennemi du front, SRP_FrontEnemy.c :
- règles F7 à F12, F1 à F6 et H2 ;
- la boucle de 5 s pilote le Commandeur ;
- surcharge d'OnControllableDestroyed ; RPC de son ;
- chaque point de décision appelle un contrat qui rend la valeur votée.
Compiler.
- 8. Carte et écrans : SRP_FrontMap, SRP_FrontScreens (avec la section Renseignement), SRP_Admin (onglets Territoire et Commandeur, branchés sur les contrats). Compiler.
SÉANCE 1 : le calque de carte d'abord, puis les essais 1 à 11. Le Commandeur neutre donne le front seul.
- 9. Web : SRP_Bridge, bot, site, setup_discord (salon état-major-ennemi). Vérifier avec py_compile.
- 10. Tout le CERVEAU d'un coup (LI2) :
- SRP_Commander : comptes rendus, connaissance, informateurs, cycles, humeur, gravité, portes des appuis ;
- SRP_CmdRegions : officiers, chute, capture ACE, remplacement, libération ;
- SRP_CmdResources et SRP_CmdDepot ;
- SRP_CmdScreens : journal, renseignement, pages.
Les moyens restent neutres : chaque ordre s'écrit « (moyen pas encore branché) ». Compiler.
SÉANCE 2, cerveau à blanc : capture ACE en premier, puis les essais 12 à 14, 17 et 26 à 28.
- 11. Tous les MOYENS d'un coup (LI2), un fichier compilé à la fois :
- SRP_CmdManeuvers, avec les retouches de SRP_Enemy (nouveaux ordres), EnemyTrucks, Territory, Awareness, Garrison et Capture (papier) ;
- SRP_CmdSupport, avec le prefab SRP_Piece_Mortier.et et son .meta ;
- SRP_CmdArmor, avec le modded SCR_GetInUserAction et SpawnVehicleCrew.
- 12. Brancher ForceOrder et CancelOperations, et retirer les mentions « pas encore branché ». Compiler.
SÉANCE 3, moyens avec un client pair : essais 15 à 25 et 29.
- 13. Finition :
- nettoyer le prefab SRP_GameMode.et ;
- générer commandeur_reglages.txt avec les valeurs de Jack ;
- contrôles a à o et doublons ;
- guide, tableau /suivi, mémoire.
- 14. SÉANCE 4 : essai à deux et redémarrage (RE13 : une seule restitution de chaque chose). Puis publication un soir sans opération, Staff présent.

## sous_appuis.json — interfaces fournies
- enum SRP_ECmdSupport { MORTIER = 0, FUMEE = 1, ARTILLERIE = 2, BOMBE = 3, AVION = 4, BLINDE = 5, HELICO = 6 }
- enum SRP_ECmdArmorReason { CONTRE_ATTAQUE = 0, RENFORT = 1, RIPOSTE = 2, STAFF = 3 }
- static SRP_CmdSupport SRP_CmdSupport.GetInstance()
- void SRP_CmdSupport.Start() / void Stop() / void ResetAll(string reason)
- string SRP_CmdSupport.RequestMortar(int region, vector aim, string reason) : "" = accepté, sinon la raison du refus (aussi pour le harcèlement MA10)
- bool SRP_CmdSupport.PickMortarTarget(int region, out vector aim)
- int SRP_CmdSupport.SupportWithdrawal(int region, vector from, vector threat) : secondes avant le premier fumigène, -1 sans mortier
- string SRP_CmdSupport.SupportAssault(int zone, vector target, array<ref SRP_EnemyGroup> groups, bool counterAttack)
- void SRP_CmdSupport.EndAssault(int zone)
- string SRP_CmdSupport.RequestArtillery(int region, vector aim, string reason)
- bool SRP_CmdSupport.PickStrongpoint(int region, out vector aim)
- string SRP_CmdSupport.RequestBomb(vector aim, string reason)
- bool SRP_CmdSupport.PickBombTarget(out vector aim)
- string SRP_CmdSupport.RequestFlyby(vector pos, string reason)
- string SRP_CmdSupport.RequestArmor(int region, vector target, int armorReason, int zone)
- string SRP_CmdSupport.RequestHeli(vector pos, string reason)
- void SRP_CmdSupport.ForceNextHeli()
- bool SRP_CmdSupport.IsMortarReady(int region) / bool IsArtilleryReady(int region) / bool IsArmorActive() / bool IsBigOperationRunning(int attachedZone)
- bool SRP_CmdSupport.OnSabotage(IEntity target, int playerId)
- string SRP_CmdSupport.ForceSupport(int kind, vector pos, string author) (VU5)
- string SRP_CmdSupport.ClearBans(int region, string author)
- void SRP_CmdSupport.GetReport(notnull array<string> lines)
- static bool SRP_CmdSupport.IsArmedVehicle(IEntity vehicle) (sert au renseignement, riposte AP5)
- static bool SRP_CmdSupport.IsBatteryVehicle(IEntity vehicle)
- static void SRP_CmdSupport.PlaySoundLocal(ResourceName acp, string eventName, vector pos) (appelée côté joueur par la RPC du cerveau)
- SRP_EnemyGroup SRP_EnemyComponent.SpawnVehicleCrew(ResourceName crewPrefab, IEntity vehicle, int seats, string owner) (nouvelle, SRP_Enemy.c)
- static void SRP_EnemyTruckComponent.PreventLod(IEntity member) (devenue publique)
- modded class SCR_GetInUserAction : override bool CanBePerformedScript(IEntity user) (verrou du 2S1)
- État sauvé dans front.json par SetExtra/GetExtra : appui_mortier_ban, appui_artillerie_ban, appui_mortier_tirs, appui_artillerie_dernier, appui_bombe_dernier, appui_avion_passages, appui_helico_dernier, appui_frappes, appui_en_route
## sous_appuis.json — interfaces attendues
- CERVEAU (nom proposé SRP_CommanderComponent, composant du GameMode présent sur toutes les machines) : static GetInstance() ; bool IsFrozen() (VU6) ; void LogDecision(string text) (VU4 : journal + salon Discord du Staff).
- CERVEAU, RPC : void BroadcastSound(ResourceName acp, string eventName, vector pos) = [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)] RpcDo_SRPCmdSound, qui appelle SRP_CmdSupport.PlaySoundLocal sur chaque machine, plus un appel local si !System.IsConsoleApp() (modèle SRP_Sirene.c:362-365 et 504-509). Pour le repli : void BroadcastLaunch(RplId shell, vector dir).
- CERVEAU, cycle : appelle SRP_CmdSupport.Start() après le chargement du front (front prêt), Stop() à l'arrêt et ResetAll(raison) à une nouvelle campagne. Il décide du moment : mortier et hélico toutes les 1-2 min, gros moyens toutes les 15-20 min au plus (CO4).
- RENSEIGNEMENT (tranche cerveau, CO5) : class SRP_CmdContact { vector m_vPos; int m_iCell; int m_iRegion; int m_iZone; int m_iPlayers; int m_iFirstSeenUnix; int m_iLastSeenUnix; bool m_bBySoldier; bool m_bArmedVehicle; }.
- RENSEIGNEMENT : int GetContacts(int region, int maxAgeSeconds, notnull array<ref SRP_CmdContact> result), avec region = -1 pour toute l'île.
- RENSEIGNEMENT : bool IsPlayerWatchedNear(vector pos, float radius, out vector playerPos). Vrai si un soldat ennemi vivant et conscient identifie en ce moment (4 s au plus, EAITargetInfoCategory.IDENTIFIED, comme SRP_EnemySenses.NoteSightings l.1238-1270) un joueur à moins de radius.
- OFFICIERS ET RÉGIONS : int RegionCount() ; int RegionAt(vector pos) ; int RegionOfZone(int zone) ; string RegionName(int region) (« région de Saint-Philippe », VU1) ; string RegionKey(int region) et int FindRegion(string key) (clé stable pour front.json) ; bool IsDisorganized(int region) (OF6-OF8) ; bool IsRegionAlive(int region) (OF11).
- STOCKS : enum SRP_ECmdStock { OBUS, BLINDE, HELICO, ARTILLERIE, BOMBE } ; int GetStock(int region, int kind) ; bool TakeStock(int region, int kind, int count, string reason) ; void GiveStock(int region, int kind, int count, string reason). region = -1 pour les moyens du Commandeur (blindés, hélico, artillerie, bombe, RE2) ; les obus sont par région.
- MANŒUVRE ET CONTRE-ATTAQUE (SRP_FrontEnemy.c) : appelle SupportAssault au départ de la 1re vague et EndAssault à la fin ; SupportWithdrawal avant un décrochage #40 ; RequestArmor(RENFORT) pour une localité attaquée ; RequestArmor(RIPOSTE) pour un véhicule armé vu.
- MANŒUVRE : fournit int GetActiveAttackZone() (-1 sans contre-attaque en cours) et bool HasRoomFor(int soldiers, bool combat) pour la limite des 120 (repli : CountBudgetSoldiers() et GetMaxSoldiers()).
- SOCLE DU FRONT, noms tranchés : SRP_FrontComponent.GetInstance(), CellIndexAt(vector), GetCellOwner(int) avec SRP_EFrontOwner, IsCellAtFront(int), IsRedAt(vector), IsBaseCell(int), IsInBaseZone(vector), GetZoneAt(vector), CellRef(int), SetExtra(string, string), GetExtra(string), MarkDirty(bool).
- CAPTURE, recensement unique : int CountValidPlayersNear(vector pos, float radius).
- CARTE : SRP_FrontRadio.MortarSilenced(int zone, string cellRef), BatteryDestroyed(int zone, string cellRef) et ArmorDelivered(string vehicleName, int bounty) ; page Staff « Appuis » branchée sur GetReport, ForceSupport et ClearBans.
- EXISTANT DU MOD, utilisé tel quel : SRP_EnemyComponent (GetInstance, SpawnAt, SpawnGroup, NoteHome, ApplyRole, OrderMove, OrderSearch, GroupPosition, CountGroupsNear, CountBudgetSoldiers, GetMaxSoldiers, RegisterTransport, Delete, Journal, GetFactionKey) ; SRP_Placement (FindSpawnPosition, IsSeenByAnyPlayer, IsVehicleSeenByAnyPlayer, NearestPlayer, BasePosition, IsWater, OnGround, FindRoadPointToward) ; SRP_HeliSearch.Launch et Count ; SRP_TreasuryComponent.Add ; SRP_Utils (GetPlayerCharacters, NotifyPlayer, IsDead, GetPlayerIdFromEntity).

## sous_cerveau.json — interfaces fournies
- Accroches statiques, qui ne font rien si le Commandeur est absent :
- static SRP_Commander SRP_Commander.Get()
- static void SRP_Commander.NoteSighting(SRP_EnemyGroup record, IEntity player, vector perceived, bool identified)
- static void SRP_Commander.NoteShot(SRP_EnemyGroup record)
- static void SRP_Commander.NoteAirSighting(vector position)
- static int SRP_Commander.ThreatAt(vector position, int islandThreat)
- static float SRP_Commander.GarrisonFactorAt(vector position)
- static bool SRP_Commander.HasOfficerTarget()
- static void SRP_Commander.AddAlertAt(vector position, float amount, string reason)
- Vie du Commandeur (appelées par l'hôte et par le socle) :
- SRP_Commander(SRP_FrontEnemyComponent host) ; void BuildRegions() ; void Start() ; bool IsStarted() ; void Tick(int nowUnix)
- void OnControllableDestroyed(notnull SCR_InstigatorContextData data)
- void OnZoneCaptured(int zone) ; void OnZoneLost(int zone) ; void OnLocalityTaken(string locality)
- void ResetCampaign(string reason) ; void OnRestored() ; void WriteTo(JsonSaveContext ctx) ; void ReadFrom(JsonLoadContext ctx)
- Régions et officiers :
- int GetRegionCount() ; int GetRegionOfZone(int zone) ; int GetRegionAt(vector position) ; string GetRegionCode(int region) ; string GetRegionLabel(int region) ; int GetRegionState(int region)
- bool IsRegionDisorganized(int region) ; bool HasLivingOfficer(int region) ; float GetRegionAlert(int region) ; int GetRegionMood(int region)
- int GetOfficerTrait(int region) ; string GetOfficerName(int region) ; int GetOfficerState(int region) ; IEntity GetOfficerEntity(int region) ; bool GetOfficerHideout(int region, out vector position, out string locality)
- Renforts et appuis (pour les moyens et la contre-attaque) :
- float GetReinforceDelayFactor(int zone) ; bool CanReinforceZone(int zone) ; void NoteReinforcementSent(int zone) ; bool CanRegionUseMortar(int region) ; bool CanRegionUseArtillery(int region)
- Connaissance :
- bool IsHeavySupportJustified(int zone) ; bool IsContactConfirmed(int zone, int withinSeconds) ; int GetKnownPlayers(int zone)
- bool GetBestContact(int zone, bool seenOnly, int maxAgeSeconds, out vector position, out int players)
- int CopyContacts(notnull array<ref SRP_CmdContact> result)
- float GetZoneGravity(int zone) ; int GetGravestZones(notnull array<int> zones, int maxCount)
- int GetZoneShots(int zone, int minutes) ; int GetZoneLosses(int zone, int minutes)
- bool WasArmedVehicleSeen(int zone, int withinSeconds, out vector position)
- int GetDosingPlayers(vector position) : dosage EQ4 seulement, jamais pour viser
- Missions (OF10) :
- bool PickOfficerTarget(vector near, out int region, out vector site, out string label) ; int GetOfficerMissionStatus(int region, int sinceUnix) (SRP_ECmdMissionStatus)
- Staff (VU4 à VU6) :
- string SetFrozen(bool frozen, string author) ; bool IsFrozen()
- string ForceOfficerDown(int region, bool captured, string author) ; string ForceOfficerReplace(int region, string author) ; string ForceOrder(int kind, vector position, string author) ; string ReloadSettings(string author)
- string GetStaffReport() ; string GetRegionReport(int region) ; string GetKnowledgeReport(vector from, float radius) ; void GetDecisionLog(notnull array<string> lines)
- Réglages : SRP_CmdSettings GetSettings() ; SRP_CmdSettings.Declare(string key, string defaultValue, string help), GetInt(string key), GetFloat(string key), GetBool(string key), GetString(string key), GetList(string key, notnull array<string> result), GetIntClamped(string key, int min, int max)
- Classes partagées : SRP_CmdOrder (m_iKind, m_iRegion, m_iZone, m_vAim, m_iSize, m_fDelayFactor, m_iNotBefore, m_sReason, m_sAuthor, m_bStaff), SRP_CmdContact, SRP_CmdRegion, SRP_CmdOfficer ; énumérations SRP_ECmdTrait, SRP_ECmdMood, SRP_ECmdOfficerState, SRP_ECmdRegionState, SRP_ECmdReport, SRP_ECmdOrder, SRP_ECmdMissionStatus
- Ajoutées au code existant : SRP_CivilianManagerComponent.GetInformantCandidates(notnull array<IEntity> characters) ; SRP_TreasuryComponent.IsEnemyFactionKey(string key) ; SRP_MissionManagerComponent.GetOfficerPrefabs() et IsOfficerTargeted(string regionCode)
## sous_cerveau.json — interfaces attendues
- Socle (SRP_FrontComponent), lecture : GetInstance(), IsReady(), GetZoneCount(), GetZone(int) donnant un SRP_FrontZone (m_sCode, m_sName, m_iKind, m_iLocality, m_aCells, m_aNeighbours avec la liaison maritime, m_vCentroid, m_iOwner, m_iCapturedAt, m_iLostAt), CellIndexAt(vector), GetCellZone(int), GetCellOwner(int), IsCellInPlay(int), GetGridSize(), CellRef(int), GetZoneName(int) (« Régina (S07) »), IsZoneInContact(int), IsBaseZone(int), GetStateVersion(), GetThreat(), GetLocalityCount() et GetLocality(int) (m_sName, m_sKind, m_iZone), GetKeyPointCount(zone), GetKeyPointPos(zone, k), GetKeyPointRole(zone, k).
- Socle, écriture : MarkDirty(bool immediate). Le socle appelle SRP_Commander.Get().BuildRegions() entre la géométrie et Load, WriteTo et ReadFrom dans Save et Load, OnRestored après RestoreDaily. Il laisse au Commandeur les lignes « region » de front_retouches.txt.
- Capture : un accès à IsPlayerCounted(IEntity) (règles C2 et C3) depuis le socle, pour les informateurs. Le nom d'accès (par exemple SRP_FrontComponent.GetInstance().GetCapture()) reste à fixer par le socle.
- Zones (SRP_ZoneFall) : appelle SRP_FrontEnemyComponent.OnZoneCaptured(zone), OnZoneLost(zone) et OnLocalityTaken(name). C'est déjà dans la répartition ; l'hôte transmet.
- Ennemi (SRP_FrontEnemyComponent), côté hôte : tenir m_Commander, l'appeler dans le Tick de 5 s, surcharger OnControllableDestroyed, appeler ResetCampaign depuis ResetAll, fournir OnCommanderSlowCycle(int now), marquer les vagues de contre-attaque m_sTask = "assaut".
- Ennemi, côté localités : fournir SRP_SectorDef.m_vKeyPoint2 et m_bHasKeyPoint2 (QG des villes) et IsInGarrisonedLocality(vector pos, float margin), prévus par le plan.
- Sous-module B (moyens), noms proposés, à confirmer par B : une classe SRP_CmdMeans tenue par le Commandeur, avec bool CanExecute(SRP_CmdOrder order, out string refusal) ; bool Execute(SRP_CmdOrder order, out string result) ; void OnFastCycle(int now) ; void OnSlowCycle(int now) ; void OnOfficerDown(int region, bool captured) ; void OnLossReported(int zone, vector position) ; float GetHeavyStockRatio() ; float GetRegionShellRatio(int region) ; void WriteTo(JsonSaveContext ctx) ; void ReadFrom(JsonLoadContext ctx) ; void ResetCampaign().
- B fournit aussi la classe SRP_CmdRegionMeans (sa part de chaque région : obus, dépôt, pièce réduite au silence), avec WriteTo(ctx, prefix) et ReadFrom(ctx, prefix). A la crée dans SRP_CmdRegion et la sauvegarde.
- Ce que B applique : EQ3, EQ4 (via GetDosingPlayers), MO1 à MO9, AP1 à AP6, C1, et la moitié de défenseurs gardée par la localité qui envoie (C2), portée à 60 pour cent pour un officier prudent. Il multiplie les délais par GetReinforceDelayFactor.
- B, côté #69 : faire passer le camion d'alerte par CanReinforceZone et NoteReinforcementSent. Remplacer le tirage de MaybeCallHeli par l'ordre HELICO (AP7), et exécuter ENQUETE (unité mobile la plus proche, OrderSearch).
- Sous-module C (manœuvres et contre-attaque) : lit IsRegionDisorganized (OF8 : une vague de moins, ni mortier ni blindé), GetRegionMood et GetOfficerTrait (pour décaler de ±20 min en CA1), GetZoneGravity et GetBestContact (CA2, MA8). Traite le décrochage #40 (MA5 à MA7) sur OnLossReported. Demande lui-même le tir d'ouverture et le blindé d'une contre-attaque (AP2, CA4).
- Carte et radio (SRP_FrontRadio, seul auteur des textes) : AnnounceOfficerDown(string officerName, string regionLabel, bool captured) et AnnounceRegionLiberated(string regionLabel), en radio et sur Discord territoire. Page Staff « Commandeur » : GetStaffReport, GetKnowledgeReport, GetDecisionLog, boutons SetFrozen, ForceOfficerDown, ForceOfficerReplace, ForceOrder et ReloadSettings.
- Web et Discord (SRP_Discord.c, bot) : la catégorie de journal « COMMANDEUR » va vers un nouveau salon Staff « commandeur » (webhook dans discord.json), jamais vers un salon public. Nouveaux types d'évènement « officier_tombe » et « region_liberee » pour 🚩┃territoire.
- Missions : rien d'autre que les modifications listées dans SRP_Missions.c. Le filtre de menace G9 dans End reste celui du plan.

## sous_manoeuvres.json — interfaces fournies
- Classe SRP_CmdManeuvers, serveur seulement, tenue par SRP_FrontEnemyComponent :
- static SRP_CmdManeuvers Get()
- static bool IsOn()
- void Tick(int nowUnix)
- void ReadState()
- void WriteState()
- void ResetAll(string reason)
- void LoadSettings()
- Appelées par le sous-module des officiers :
- void OnContact(int zone, vector pos, int players, bool seen, int nowUnix)
- void NoteSighting(int region, int zone, vector pos, int nowUnix) : alimente les relevés « retranché » et « chemin »
- Appelée par le territoire (camion d'entrée) : bool RequestEntryTruck(SRP_SectorState state, int players, int nowUnix)
- Appelées par le front (zones, ennemi, territoire) :
- void OnZoneCaptured(int zone, bool staff)
- void OnZoneLost(int zone)
- void OnLocalityTaken(string locality)
- void OnGarrisonWithdrawn(SRP_SectorState state)
- Appelées par les camions et l'IA ennemie :
- void OnTruckLaunched(string source, int load)
- void OnColumnDelivered(int columnId, int survivors, bool joined)
- void OnGroupGone(SRP_EnemyGroup record)
- Lues par le territoire et le front :
- bool IsLocalityEmptied(string locality)
- bool IsKeptFull(string locality)
- bool IsOnPlayersPath(int cell)
- Gravité, pour le sous-module des moyens :
- int GravityOf(int zone)
- bool IsServedZone(int zone)
- void GetZonesByGravity(notnull array<int> zones)
- Crochets de contre-attaque, appelés par SRP_FrontEnemyComponent :
- bool CaWindowDecision(int clockSec, int dueSec, int zone, int nowUnix)
- int PickByIntel(notnull array<int> candidates)
- int PlannedWaves(int zone)
- int ReviseWaves(SRP_CounterAttack attack)
- int WaveRoomGroups()
- bool WantPaperWave(vector origin, vector zoneCenter)
- void AddPaperWave(SRP_CounterAttack attack, vector origin, int soldiers, int wave, int axis)
- void OnWavePosted(SRP_CounterAttack attack, int wave, array<ref SRP_EnemyGroup> groups, vector originA, vector originB, bool hasB)
- void Support(SRP_CounterAttack attack, int nowUnix)
- void DriveAssault(SRP_CounterAttack attack, int nowUnix)
- bool WaveArrived(SRP_CounterAttack attack)
- int CheckEnd(SRP_CounterAttack attack, int nowUnix), qui rend SRP_ECmdEnd
- void EndAttack(SRP_CounterAttack attack, int result)
- Offensive de nuit : int NightChance(int chance, out bool cancel)
- Staff :
- string ForceReinforcement(vector around, string author)
- string ForceRetreat(vector around, string author)
- string ForceHarass(vector around, string author)
- string ForceTransfer(string fromLocality, string toLocality, int soldiers, string author)
- string GetReport(vector from, float radius)
- string GetColumnsReport()
- SRP_EnemyComponent, nouveaux ordres :
- void OrderSuppress(SRP_EnemyGroup record, vector target, float holdSeconds, float height)
- void OrderAttackPoint(SRP_EnemyGroup record, vector target)
- void OrderSmokeCover(SRP_EnemyGroup record, vector protect, int grenades)
- void OrderForcedMove(SRP_EnemyGroup record, vector target, float radius)
- void OrderRetreat(SRP_EnemyGroup record, vector smokeAt, int grenades, vector exit, vector destination)
- void OrderApproach(SRP_EnemyGroup record, vector via, vector target)
- static void HoldAwake(SRP_EnemyGroup record)
- static void ReleaseAwake(SRP_EnemyGroup record)
- static int FitOf(SRP_EnemyGroup record)
- SRP_EnemyGroup, champs : int m_iAssaultZone, int m_iCmdRole, string m_sSourceLocality, bool m_bAwake, vector m_vCmdLast, int m_iCmdStillUnix
- SRP_EnemyTruckComponent :
- bool DispatchFrom(SRP_SectorState state, string source, vector sourcePos, int load, int arriveSeconds, string reason)
- bool SendContactTruck(vector contact, string source, vector sourcePos, int load, int arriveSeconds, int zone)
- int LaunchColumn(SRP_CmdColumn column, vector at, array<vector> remaining, int load)
- static void PreventLod(IEntity member) et static void AllowLod(IEntity member), rendues publiques
- int LoadFor(int players), int CapLoad(int load) et int CountLaunched(), rendues publiques
- SRP_FrontEnemyComponent :
- int Effective(string locality)
- int CurrentSent(string locality)
- void AddSent(string locality, int n)
- void ReturnSent(string locality, int n)
- void ClearSent(string locality)
- int CurrentBonus(string locality)
- void AddBonus(string locality, int n)
- bool IsInMainRedMass(int cell)
- bool IsCounterAttackActive()
- int GetCounterAttackZone()
- SRP_EnemyAwareness : static bool FlankPoint(vector helper, vector source, bool hasSource, vector contact, out vector flank), rendue publique
- SRP_EnemyGarrison :
- static SRP_EnemyGroup FindTownHQ(SRP_EnemyComponent enemies, SRP_SectorState state)
- static bool IsCentreGroup(SRP_EnemyGroup record)
- SRP_FrontCapture :
- void SetPaperAssault(int cell, int soldiers, int zone, int unixTime)
- void ClearPaperAssault(int zone)
- SRP_FrontRadio :
- static void AttackBroken(int zone, int retaken)
- static void AttackCalledOff(int zone)
## sous_manoeuvres.json — interfaces attendues
- Sous-module CERVEAU et OFFICIERS (noms à confirmer à la relecture du Commandeur). SRP_CommanderComponent :
- static GetInstance() ; bool IsActive() (interrupteur VU6).
- Régions : int RegionOfZone(int zone), qui tient compte de OF11 (zone reprise rattachée à la région voisine) ; bool IsRegionDisorganized(int region) (OF6/OF7/OF9) ; int RegionTemper(int region), avec SRP_ECmdTemper { PRUDENT, AUDACIEUX, LENT } (CO7) ; string RegionName(int region).
- Connaissance (CO5, jamais la vraie position) : int KnownPlayersNear(vector pos, float radius, int maxAgeSeconds) ; bool LastKnownContact(int zone, int maxAgeSeconds, out vector pos, out int unixTime) ; bool HeavyFire(int zone, int seconds) (CO6) ; int RecentLosses(int zone, int minutes).
- Journal : void Log(string text), vers le menu Staff et le salon Discord du Staff (VU4).
- Réglages : static int CmdInt(string key, int def) et static float CmdFloat(string key, float def), lus dans $profile:SimpleRP/commandeur_reglages.txt.
- Il m'appelle : OnContact(...) à chaque rapport (OF3/OF4) et NoteSighting(...) à chaque repérage.
- OF5 (un quart de soldats en plus tant que l'officier vit) doit entrer dans GarrisonSoldiers, donc dans le Nominal utilisé par Effective.
- Sous-module RESSOURCES et BUDGET (noms à confirmer) :
- SRP_CmdStocks : float GlobalFillRatio() (CA7, 0 à 1) ; float RegionFillRatio(int region) (CA1).
- SRP_CmdBudget : int SoldierRoom(int priority), la place sous les 120 avec les 20 places gardées (RE11/RE12), avec SRP_ECmdPriority { COMBAT, DEFENSE, POSTE, GARDE, PATROUILLE }. Renforts, colonnes vers un combat et vagues : COMBAT. Raids : PATROUILLE.
- C1 respecté : aucun appel de stock pour l'infanterie ni pour les camions.
- Sous-module MOYENS (noms à confirmer). SRP_CmdSupport :
- bool CanMortar(int region, vector target) ;
- bool RequestMortar(int region, vector target, int shellKind, int shells, string reason), avec shellKind EXPLOSIF ou FUMEE et shells = 0 pour le nombre par défaut ;
- bool RequestHeavyArtillery(vector target, string reason) ;
- bool RequestArmour(int region, vector origin, vector target, string reason) ;
- bool IsBigOperationRunning().
- C'est lui qui applique EQ3, EQ4, MO6, OF6/OF8, la gravité (IsServedZone) et les stocks. Il rend faux s'il refuse, sans rien demander d'autre.
- Socle SRP_FrontComponent (liste de la grille, noms tranchés) :
- GetInstance(), CellIndexAt(vector), GetCellOwner(int), GetCellZone(int), CellCenter(int), Neighbour(int cell, int dir), IsRedAt(vector), IsCellAtFront(int) ;
- IsZoneInContact(int), IsZoneAtFront(int), IsZoneProtected(int), IsBaseZone(int) ;
- accès aux SRP_FrontZone (m_aCells, m_aNeighbours, m_vCentroid, m_iOwner, m_bDepot) ;
- GetKeyPointCount(zone), GetKeyPointPos(zone, k), GetKeyPointRole(zone, k), GetKeyPointCell(zone, k) ;
- accès aux SRP_FrontLocality par nom ; IsFrozen() ; GetThreat() ; SetExtra / GetExtra ;
- nouveau : MarkDirty() et les 6 champs de localité sauvegardés.
- Capture SRP_FrontCapture :
- CountValidPlayersNear(vector pos, float radius), CountValidPlayersInCell(int cell), CountFitAssaultersInZone(int zone), GetLiveState(int cell) ;
- nouveau : SetPaperAssault et ClearPaperAssault.
- Zones SRP_ZoneFall :
- appeler SRP_FrontEnemyComponent.OnZoneCaptured(zone), qui transmet OnZoneCaptured(zone, staff) ;
- LoseZone(zone, OFFENSIVE, true) pour H2.
- Module ennemi du front (mod_ennemi) :
- IsGarrisonPosted, CurrentLosses, AddLosses, ClearLosses, GarrisonSoldiers ;
- PickWaveOrigins, qui range les deux premiers départs ;
- SRP_CounterAttack avec les champs listés dans le fichier modifié.
- Carte : les pages Staff (SRP_Admin.c) branchent ForceReinforcement, ForceRetreat, ForceHarass, ForceTransfer, GetReport et GetColumnsReport, plus la commande « relire les réglages ». SRP_FrontRadio porte AttackBroken et AttackCalledOff.

## sous_ressources.json — interfaces fournies
- SRP_CmdSettingsFile (SRP_CmdSettings.c) : static void Reload() ; static int GetInt(string key, int fallback, int lo, int hi, string comment) ; static float GetFloat(string key, float fallback, float lo, float hi, string comment) ; static bool GetBool(string key, bool fallback, string comment) ; static string GetString(string key, string fallback, string comment). Servent aussi aux sous-modules A, C et D : un seul fichier de réglages.
- enum SRP_ECmdStock { OBUS, BLINDE, HELICO, ARTILLERIE, BOMBE }
- SRP_CmdResources : static SRP_CmdResources Get() ; void Start() ; void Stop() ; void Tick()
- SRP_CmdResources lecture : float GetStock(int kind, int region) ; float GetFull(int kind, int region) ; float GetRevenueFactor(int kind, int region) ; int MinutesToNextUnit(int kind, int region) ; int CountOpen(int kind) ; float GetStockLevel() ; int GetRegionSupplyState(int region)
- SRP_CmdResources dépense : bool CanReserve(int kind, int region, int amount, out string refusal) ; int Reserve(int kind, int region, int amount, string what, bool staff) ; void Consume(int ticket, int amount) ; void Release(int ticket, string reason) ; int GetTicketLeft(int ticket)
- SRP_CmdResources évènements : void OnSabotageMission(vector site, int missionType, string prefabName, string author) ; void RequestResupply(int region, string author) ; void CutRegion(int region, string reason) ; void OnZoneOwnerChanged(int zone, int owner) ; void OnThreatChanged()
- SRP_CmdResources offensive de nuit (CA7) : bool IsOffensiveAllowedByStocks() ; int OffensiveChanceBonus()
- SRP_CmdResources sauvegarde : void WriteState(JsonSaveContext ctx) ; void ReadState(JsonLoadContext ctx) ; void ResetForCampaign(string author)
- SRP_CmdResources Staff : void StaffSetStock(int kind, int region, float value, string author) ; void StaffRefillAll(string author) ; string GetStaffReport() ; SRP_CmdDepots GetDepots()
- SRP_CmdDepots (SRP_CmdDepot.c) : bool TryUse(IEntity entity, int playerId, out string message) ; void Reveal(int region, string reason) ; bool IsRevealed(int region) ; vector GetDepotPos(int region) ; void GetRevealedDepots(notnull array<int> regions, notnull array<vector> positions) ; void StaffMove(int region, string author) ; void StaffSabotage(int region, string author) ; void StaffClearCut(int region, string author) ; string GetStaffReport()
- enum SRP_ECmdCapClass { COMBAT, GARNISON, POSTE, GARDE, PATROUILLE, HORS_COMPTE }
- SRP_CmdCapacity : static SRP_CmdCapacity Get() ; void Start() ; void Stop() ; void Tick() ; void ApplyEngineLimit()
- SRP_CmdCapacity classement et comptes : static int ClassOf(SRP_EnemyGroup record) ; static void TagImportance(SRP_EnemyGroup record) ; int CountGround() ; int CountClass(int cls) ; int CountGroundGroups() ; int CountClassGroups(int cls)
- SRP_CmdCapacity place : int Room(int cls, string owner) ; int GroupRoom(int cls, string owner) ; int EngineRoom() ; string Refusal(int cls, int soldiers, int groups, string owner) ; void NoteDemand(string key, int cls, int soldiers, int groups, vector pos) ; void ClearDemand(string key)
- SRP_CmdCapacity localités : void SetWantedGarrisons(notnull array<string> keys, notnull array<int> sizes, notnull array<bool> near, notnull array<bool> attacked) ; int GarrisonCap(string locality, int decided)
- SRP_CmdCapacity frein C2 : bool CanReinforce(int zone, string fromLocality, int wanted, out int allowed, out string refusal) ; int NoteReinforcement(int zone, string fromLocality, int sent) ; void NoteReinforcementArrived(int id)
- SRP_CmdCapacity sauvegarde et Staff : void WriteState(JsonSaveContext ctx) ; void ReadState(JsonLoadContext ctx) ; string GetStaffReport()
- SRP_EnemyComponent (ajouts) : champ SRP_EnemyGroup.m_iCapClass ; int CountPendingSpawn() ; int ReleaseForRoom(int soldiersWanted, float patrolMinDist, float jeepMinDist, int maxGroups) ; void SetMaxGroups(int maxGroups, int missionReserve)
## sous_ressources.json — interfaces attendues
- Sous-module A (Commandeur, officiers, régions), nom attendu SRP_CommanderComponent : Start() différé côté serveur, qui appelle SRP_CmdSettingsFile.Reload(), SRP_CmdCapacity.Get().Start() et SRP_CmdResources.Get().Start() ; Stop() à l'OnDelete ; bool IsActive() (interrupteur VU6) ; WriteState(ctx) et ReadState(ctx), qui chaînent vers les miens.
- Sous-module A, régions : int RegionCount() ; int RegionOfZone(int zone) (-1 pour la base ou aucune région) ; int RegionRefZones(int region) (zones de départ, fichier des régions) ; string RegionName(int region) (VU1) ; bool IsRegionDisorganized(int region) (OF6) ; void NoteRegionEvent(int region, string kind) (alerte de région, CO9).
- Sous-module A, officier capturé (OF9) : SRP_CmdResources.Get().GetDepots().Reveal(region, "officier capturé").
- Sous-module C (moyens) : Reserve avant chaque mortier, blindé, tir lourd, sortie d'hélico ou bombe ; Consume à l'effet réel ; Release en cas d'annulation ou de retour. Avant de poser servants et équipages : Refusal(COMBAT, 2 ou 3, 1, "commandeur") ; les groupes posés reçoivent m_iCapClass = COMBAT. Officier, gardes et garde de la batterie : GARDE. L'hélico remplace son tirage par Reserve(HELICO) (AP7).
- Sous-module Équité (EQ3, EQ4) : ses plafonds et ses seuils s'appliquent avant Reserve ; il lit CountOpen(BLINDE) pour « 1 blindé à la fois ».
- Socle SRP_FrontComponent : GetThreat() ; GetZoneCount() ; GetZoneOwner(int zone) (SRP_EFrontOwner) ; IsZoneInContact(int zone) ; CellIndexAt(vector) ; GetCellZone(int cell) ; GetCellOwner(int cell) ; CellCenter(int cell) ; CellRef(int cell) ; GetKeyPointCount et GetKeyPointPos(zone, k) ; MarkDirty(bool now). Save appelle le WriteState du Commandeur ; Load appelle ReadState APRÈS les zones et les localités (pertes F12). ResetCampaign appelle ResetForCampaign ; RestoreDaily appelle ReadState. Le cœur appelle OnThreatChanged à chaque AddThreat.
- SRP_ZoneFall : après CaptureZone, LoseZone et ForceZone, appel direct de SRP_CmdResources.Get().OnZoneOwnerChanged(zone, owner), placé après l'économie dans l'ordre capture, zones, économie, Commandeur, pont, radio. La saisie du poste de commandement (D4) appelle GetDepots().Reveal(RegionOfZone(zone), "poste de commandement saisi").
- SRP_FrontEnemyComponent : GetLocalityFullStrength, GetLocalityStrength, AddLocalityLosses (valeur négative acceptée), RetireSectionFor, IsLocalityUnderAttack, IsPostPossibleNearPlayers. Il appelle SetWantedGarrisons à chaque passage, pose ses postes et ses vagues avec leur classe, et lit OffensiveChanceBonus et IsOffensiveAllowedByStocks pour H2.
- SRP_EnemyTrucks : chaque camion connaît sa localité d'origine et sa zone visée pour le frein C2.
- SRP_Missions : OnObjectiveUsed teste d'abord le dépôt (TryUse) ; End en SUCCES appelle OnSabotageMission (SABOTAGE, CACHE) ou Reveal (ECOUTE, DOCUMENTS) ; gardes en classe GARDE et vagues en classe COMBAT passent par Refusal et NoteDemand.
- SRP_FrontRadio, seul auteur des textes : EnemyDepotSabotaged(string region, string author, int hours), EnemyDepotRevealed(string region, string cellRef, string reason), EnemyDepotFell(string region, string zoneName, int hours), EnemyStockHit(string region, string what), string DepotSabotagedPlayerText(int hours), CommanderJournal(string text) (catégorie COMMANDEUR). PushEvent « depot_ennemi_sabote » et « depot_ennemi_revele ». Textes proposés : « Dépôt ennemi de la région de %1 détruit par %2 : son ravitaillement est réduit de moitié pendant %3 h. » ; « Renseignement : le dépôt ennemi de la région de %1 est repéré au carré %2. » Jamais les mots IA, automatique, garnison, groupe ou patrouille, jamais de signe pour cent.
- Carte et écrans Staff : page « Ressources ennemies » (GetStaffReport des stocks, des dépôts et de la capacité) ; boutons StaffSetStock, StaffRefillAll, RequestResupply, StaffMove, StaffSabotage, StaffClearCut et Reveal ; commande « Relire les réglages du Commandeur » (SRP_CmdSettingsFile.Reload). Au PC, les dépôts révélés (GetRevealedDepots) et l'état de ravitaillement d'une région après renseignement (VU2).
- Web et bot : catégorie de journal COMMANDEUR envoyée à un salon Discord réservé au Staff (VU4) ; nouveaux types d'évènement depot_ennemi_sabote et depot_ennemi_revele.

## sous_staff.json — interfaces fournies
- enum SRP_ECmdLogKind { DECISION, APPUI, REFUS, OFFICIER, STOCK, RENSEIGNEMENT, STAFF, SYSTEME } ; enum SRP_EIntelSource { AUCUNE, PC, OFFICIER, MISSION } ; class SRP_CmdLogEntry { int m_iUnix; string m_sHour; int m_iRegion; int m_iKind; string m_sText; }
- SRP_EnemyCommandLog.Decision(int region, bool heavy, string what, string reason, string cost, string left) : static void, à appeler par A à D pour CHAQUE décision prise.
- SRP_EnemyCommandLog.Refusal(int region, string what, string why) : static void, pour chaque décision envisagée puis écartée.
- SRP_EnemyCommandLog.Note(int region, int kind, string text) : static void (officier, stock, dépôt, renseignement, système).
- SRP_EnemyCommandLog.StaffAction(string author, string text) : static void.
- SRP_EnemyCommandLog.CountEntries(int region, bool withRefusals) : static int ; GetEntries(int region, bool withRefusals, int first, int count, notnull array<ref SRP_CmdLogEntry> result) : static void ; GetLast() : static SRP_CmdLogEntry ; Clear() : static void.
- SRP_EnemyIntel.OnKeyPointSeized(int zone) : static void, appelée par SRP_ZoneFall avant le changement de camp.
- SRP_EnemyIntel.OnOfficerCaptured(int region) : static void, appelée par B.
- SRP_EnemyIntel.OnMissionSucceeded(int missionType, vector site, string missionId) : static void, appelée par SRP_Missions à la fin d'une mission réussie.
- SRP_EnemyIntel.HasIntel(int region) : static bool ; GetIntelSince(int region) : static int ; GetIntelSourceText(int region) : static string.
- SRP_EnemyCommandScreens.StaffTab(int playerId, vector staffPos, bool inGame, array<string> parts) : static string.
- SRP_EnemyCommandScreens.RunStaff(int playerId, vector staffPos, bool inGame, string action, array<string> parts, string author, out bool wantTeleport, out vector teleportTo) : static string.
- SRP_EnemyCommandScreens.IsInternalPage(string action) : static bool ; NeedsConfirm(string action) : static bool ; PageAfter(string action, array<string> parts) : static string.
- SRP_EnemyCommandScreens.StaffHeaderPart() : static string ; StaffTerritoryLine() : static string ; StaffNearReport(vector from, float radius) : static string.
- SRP_EnemyCommandScreens.PCZoneSuffix(int zone) : static string ; PCIntelSection(int playerId) : static string.
- SRP_EnemyCommandScreens.RegionOf(string regionName) : static string ; OfficerLabel(string surname) : static string ; DurationText(int seconds) : static string ; Esc(string text) : static string.
- SRP_FrontRadio (ajouts) : static void OfficerFell(int region, string surname, bool captured, int zone) ; RegionLiberated(int region, string surname) ; IntelGained(int region, int source, string detail, bool depotRevealed, string depotRef, int zone) ; EnemyDepotSabotaged(int region, int hours, int zone) ; SupportSilenced(int region, bool battery) ; ArmorBroughtBack(string vehicleName, int reward).
- SRP_FrontMarkers.TickEnemyDepots() : static void, appelée par SRP_FrontMarkers.Tick (5 s) ; repères retirés par Clear().
- Évènements du pont (SRP_BridgeComponent.PushZoneEvent(type, zoneCode, text)) : officier_tombe, region_liberee, renseignement, depot_ennemi, blinde_ramene. Aucun autre évènement du Commandeur.
- Catégorie de journal COMMANDEUR : routée vers le flux Discord « etat-major » (secours « journal-serveur »), jamais publique, jamais dans l'onglet Journal du PC.
## sous_staff.json — interfaces attendues
- Front, socle (noms tranchés) : SRP_FrontComponent.GetInstance(), IsReady(), IsFrozen() (J3), GetZoneCount(), GetZone(int) → SRP_FrontZone (GetCode(), GetLabel(), GetKind(), IsOurs()), CellIndexAt(vector), CellRef(int), GetCellOwner(int), GetCellZone(int) (nom de la liste de la grille, à confirmer), IsBaseCell(int), IsZoneInContact(int), SetExtra(string key, string value), GetExtra(string key). Les extras doivent être sauvés dans front.json, effacés à la nouvelle campagne et repris par RestoreDaily.
- Front, carte : SRP_FrontScreens.PCTerritoire appelle PCZoneSuffix et PCIntelSection. SRP_FrontMarkers.Tick appelle TickEnemyDepots. SRP_FrontRadio garde SRP_Utils.NotifyAll, SRP_EnemyComponent.Journal("TERRITOIRE") et PushZoneEvent comme seules sorties.
- Front, zones : SRP_ZoneFall appelle SRP_EnemyIntel.OnKeyPointSeized(zone) à la réussite de D4, AVANT le changement de camp.
- Front, missions : SRP_Missions (End, succès) appelle SRP_EnemyIntel.OnMissionSucceeded(mission.m_iType, mission.m_vSite, mission.m_sId).
- Front, ennemi : SRP_FrontEnemyComponent.GetZoneStrength(int zone, out int planned, out int nominal) → bool. Effectif prévu de la localité de la zone (pertes F12, bonus OF5, départs MA2, regarnissage MA7 compris) ; nominal = effectif de base sans officier ; false pour une zone sans localité.
- Front, web : SRP_BridgeComponent.PushZoneEvent(string type, string zoneCode, string text). Le bot traite tous les évènements d'un envoi par messages de 10 (aujourd'hui seulement events[:10], bot.py:583). La file du mod passe à 40 (m_iMaxEvents).
- Existant : SRP_EnemyComponent.Journal(string, string) (SRP_Enemy.c:671) et IsTestMode() (:680) ; SRP_Utils.NotifyAll (SRP_Definitions.c:692) ; SRP_Time.Now() (:583) ; Teleport et Character de SRP_AdminComponent (l.2236, l.187).
- A, cerveau (nom proposé SRP_EnemyCommand, classe tenue par SRP_FrontEnemyComponent, sans composant neuf) : static GetInstance() ; bool IsReady() ; bool IsFrozen() ; string SetFrozen(bool frozen, string author) (état dans front.json, VU6) ; int GetRegionCount() ; SRP_EnemyRegion GetRegion(int index) ; int FindRegion(string code) ; int RegionIndexOfZone(int zone) (région qui tient ou tenait la zone, -1 pour la base) ; int GetNextFastSeconds() ; int GetNextHeavySeconds() ; int CountEnemySoldiers() ; int GetEnemyLimit() ; string GetSettingsReport() ; string ReloadSettings(string author) ; int GetSettingInt(string key, int defaultValue) ; string GetSettingText(string key, string defaultValue) ; string CancelOperations(string author) ; void OnNewCampaign(), qui appelle SRP_EnemyCommandLog.Clear(). A appelle SRP_EnemyCommandLog.Decision ou Refusal pour CHAQUE décision.
- B, officiers et régions (SRP_EnemyRegion) : string GetCode() (« R3 ») ; string GetName() (plus grande localité) ; int CountZonesRed() ; int CountZonesOurs() ; void GetZones(notnull array<int> zones) ; bool IsLiberated() ; int GetLiberatedAt() ; bool HasOfficer() ; string GetOfficerSurname() ; string GetOfficerTrait() (prudent, audacieux, lent) ; int GetOfficerSerial() (numéro unique, jamais réutilisé) ; int GetOfficerSince() ; string GetHideoutName() ; vector GetHideoutPos() ; void GetHideoutChoices(notnull array<string> names) ; bool IsOfficerPosted() ; int CountGuards() ; bool IsDisorganized() ; int GetDisorganizedUntil() (Unix) ; bool WasCaptured() ; string GetFallenSurname() ; int GetAlertLevel() ; int GetAlertMax() ; int GetAlertDecayAt() ; int GetLastReportAt() ; string GetRadioDelayText().
- B, actions et appels : string ForceOfficerDown(int region, bool captured, string author) ; string ForceNewOfficer(int region, string author) ; string GetNearReport(vector from, float radius). B appelle SRP_FrontRadio.OfficerFell et RegionLiberated, et SRP_EnemyIntel.OnOfficerCaptured(region) à la capture.
- C, stocks : int GetShells(int region) ; int GetShellsMax(int region) ; int GetArmor() ; int GetArmorMax() ; int GetHeavyShots() ; int GetHeavyMax() ; int GetHeliSorties() ; int GetHeliMax() ; bool IsBombAvailable() ; int GetBombUsedAt() ; int GetIncomeCenti() ; int GetIncomeFloorCenti() ; int GetFullStockCenti() (100 + 5 × menace, RE10) ; int CountZonesHeld().
- C, dépôt : vector GetDepotPos(int region) ; bool IsDepotRevealed(int region) ; bool IsDepotSabotaged(int region) ; int GetDepotSabotagedUntil(int region) ; void RevealDepot(int region, string reason). C appelle SRP_FrontRadio.EnemyDepotSabotaged.
- D, moyens : string ForceMeans(string kind, vector target, string targetLabel, string author, bool fromStock). Valeurs de kind : mortier, fumee, feinte, artillerie, bombe, avion, btr, brdm, typhoon, renfort, helico, harcelement, colonne, decrochage. Rend le résultat ou le refus : base, soldats ennemis ou civils trop près, limite des 120, règle de la moitié.
- D, états et appels : string GetEngagedArmorText() ; string GetCapsText() ; int GetMortarState(int region) (0 prête, 1 en batterie, 2 muette) ; int GetMortarSilencedUntil(int region) ; vector GetMortarPos(int region) ; int GetBatteryState(int region) ; int GetBatteryDestroyedUntil(int region) ; vector GetBatteryPos(int region) ; string GetLastReinforcementText(int region) ; int GetNextReinforcementSeconds(int region) ; string GetNearReport(vector from, float radius). D appelle SRP_FrontRadio.SupportSilenced et ArmorBroughtBack.