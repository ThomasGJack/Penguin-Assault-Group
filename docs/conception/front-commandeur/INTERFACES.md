# 1. FRONT : décisions de cohérence (font foi)

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

## Réponses mal couvertes (retenu)
- A3 (blocs réguliers d'1 km) : la grille coupe les 2 blocs à deux villages et fusionne les morceaux de moins de 5 carrés. On passe à environ 66 zones, dont certaines irrégulières, alors que 4 modules comptent encore 82 blocs bruts. À dire à Jack (question 3) et à corriger dans les chiffres.
- A6 (« Régina (S07) ») : la note du choix disait « le nom reste la clé ». Tous les modules prennent le code S07 comme clé technique, parce que le nom d'une zone peut changer avec une retouche. C'est plus sûr, mais à dire à Jack en une ligne, sans en faire une question.
- B5 (2 à 3 mois) : aucun module n'a vérifié le rythme. Le calcul de l'aide (30 zones, 3 par semaine) ne vaut plus avec environ 66 zones à 15 min le carré. Question 6.
- D1 : la réponse ne dit rien des zones sans village, environ 36 sur 66. Tous les modules proposent « la moitié des carrés », mais avec des primes différentes (200 € ou 300 €). Question 4.
- D3 (« posé à la main par toi ») : le module zones place un QG automatique à 150 m tant que Jack ne l'a pas posé. La grille retombe sur l'endroit où le nom est écrit sur la carte. Ces replis ne doivent servir qu'en attendant, jamais en ligne. Question 8.
- E4 : la capture l'applique carré par carré, y compris à un petit groupe de carrés isolés dans une zone encore reliée. C'est plus que la réponse, qui parle d'une zone, et encore plus contraire à E5. Retenu : par zone seulement.
- E5 (« jamais sans combat ») : contredite par E4 et H2, ce que les notes du QCM signalaient déjà. Question 1.
- F3 : le détail du choix dit « une zone ne retombe jamais d'un bloc ». La grille propose au contraire qu'une zone perdue repasse entièrement rouge (sa question 12, et SetZoneOwner avec paintCells). À écarter pour une perte en combat ; la repeinte ne sert qu'à E4, H2 et au Staff. La règle qui fait perdre une zone n'existe nulle part. Question 2.
- F5 : l'ennemi l'étend à tous les carrés à 100 m du point clé, la capture et les zones au seul carré du point clé. Retenu : le carré du point clé, puisque la réponse parle du point clé.
- G2, G8 et G9 : chacune est appliquée deux fois (double prime, double alerte au commandement, double −1 possible, filtre G9 à deux endroits). Voir les trous 14.
- G8 : trois désaccords. Zone sans localité : 200 € (zones) ou 300 € (missions). Zone à deux localités : somme (zones) ou plus grosse (missions), sans objet si les blocs sont coupés. Zone forcée par le Staff : sans prime (zones, carte) ou avec prime (missions). Questions 4 et 9.
- G11 (« toujours des gardes, même dans un village gardé ») : les missions limitent la garde à 2 groupes dans un village gardé ; la migration retire tout test. La limite de 2 est une entorse, liée à la question 7 sur la capacité d'IA.
- I5 (« chaque carré pris ou perdu ») : au-delà de 3 carrés dans un même passage, la carte les regroupe en une ligne qui cite au plus 6 références. Les carrés repeints par une chute de zone, E4, H2 ou le Staff ne sont pas annoncés un par un, mais en une ligne par zone. Entorse raisonnable, à dire à Jack.
- I7 (« une image du front chaque soir ») : par défaut, le web ne poste rien si le front n'a pas bougé (front_image_if_unchanged = false). C'est contraire à la lettre de la réponse. Retenu : poster chaque soir ; le réglage reste disponible.
- I8 (« une photo par jour ») : le web saute la photo du jour si elle est identique à la précédente (PHOTO_SI_INCHANGEE = False). Retenu : une photo chaque jour (environ 0,4 Ko).
- I10 (« liste des 30 zones ») : il y en a environ 66. La carte les liste groupées (attaques, front ennemi, nos zones au front, arrière). D'accord, à dire à Jack.
- C3 : la capture écarte aussi tout joueur téléporté par le menu Staff et le Staff invincible, alors que la question ne visait que le Staff. C'est logique ; à dire à Jack.
- E2 (« rien » pour une poche) : l'ennemi ajoute que les poches de moins de 10 carrés ne rendent pas une zone bleue attaquable (F1). C'est un ajout ; à garder réglable.
- K1 : campagne n°1 (grille) ou n°2 (migration), trois noms d'archive différents. Retenu : n°2 et archives/territoire_campagne1_<date>.json.
- K2 : codée trois fois, et IsCounted ne suffit pas (vérifié dans le code). Retenu : IsRestockDone et un nouvel essai tant que les dépôts sont pleins, sans caisse posée ni paquet perdu.
- H2 : bien appliquée par l'ennemi, mais la réussite passe par zones.SetZoneOwner, une fonction qui n'existe pas. Elle doit passer par zones.LoseZone(OFFENSIVE, repeindre) pour que la menace, le dépôt et le stock suivent.

## Module mod_capture.json
### Résumé
Ce module fait vivre le front, carré par carré. Toutes les 5 secondes, le serveur regarde dans quel carré se trouve chaque joueur valide et chaque soldat ennemi. Un joueur est valide s'il est vivant, conscient, sorti de son délai de grâce, et à pied ou dans un véhicule au sol. Le Staff ne compte pas pendant 5 minutes après une téléportation ou un passage en Game Master.
Un carré rouge qui touche du bleu par un côté est pris après 15 minutes de présence sans ennemi dedans. Pendant un combat, la progression se fige. Quand plus aucun joueur n'est dans le carré, elle redescend aussi vite qu'elle était montée.
Pendant une contre-attaque, les assaillants reprennent un carré bleu qu'ils occupent 1 minute sans joueur. Un joueur à 100 m d'un point clé protège le carré de ce point clé. La zone de la base et les zones qui la touchent ne se perdent jamais.
Un carré bleu qui n'a plus de chemin de carrés bleus jusqu'à la base repasse rouge après 24 heures réelles (E4).
Chaque carré pris ou perdu est annoncé à la radio, avec une ligne par zone et par passage. Ces lignes ne partent pas sur Discord, où seules les annonces par zone sont publiées. Le front gelé bloque tout. Un redémarrage efface ce qui est en cours sans rien faire perdre.
### Règles
- A1 : un carré fait 200 m (grille de 64 × 64). Seuls les carrés de terre gardés sont évalués (A2 : 1 395 carrés, île principale et Erquy). Un point en mer ou hors de la grille est ignoré.
- C2 : un joueur compte s'il est vivant et conscient (GetLifeState() == ALIVE), à pied, ou dans un véhicule à 7 m au plus au-dessus du sol ou de la mer. Hélico en vol : jamais. Hélico posé, voiture, camion, blindé : oui.
- Consigne du module (et aide de C2) : un joueur en délai de grâce (3 min après une connexion ou une réapparition, SRP_Enemy.c:851 InGrace) ne compte pas.
- C3 : un joueur ne compte pas pendant 5 min après une téléportation faite par le menu Staff (SRP_Admin.c:2236 Teleport, qui sert à toutes les téléportations du menu). Même règle pendant qu'il a le Game Master ouvert (éditeur non limité) ou qu'il possède une IA, puis 5 min après. Filet de sécurité : un membre du Staff à pied qui saute de plus de 300 m en un passage est traité comme téléporté. Un joueur écarté ne prend rien et n'empêche aucune reprise (« ni prendre ni tenir »).
- C6 : un ennemi compte s'il appartient à un de nos groupes d'IA (SRP_EnemyComponent.GetGroups, SRP_Enemy.c:4200) : garnison, poste, patrouille, garde de mission, jeep, camion, survivants. Il doit être vivant, conscient et au sol (même filtre de véhicule à 7 m). L'équipage de l'hélico (m_sOwner « helico ») ne compte jamais. Les civils non plus. Le comptage se fait par soldat, jamais par une recherche par carré.
- C1 : un carré rouge ne se prend que s'il touche un carré bleu par un côté (ses 4 voisins, jamais en diagonale).
- C4 (réponse « autre » de Jack) : il faut 15 min cumulées avec au moins un joueur valide dans le carré et aucun ennemi valide.
- C5 : la durée est la même avec 1 ou 10 joueurs.
- C7 : joueur et ennemi dans le même carré : la progression se fige. Plus aucun joueur dans le carré, ou carré qui ne touche plus de bleu : la progression redescend à la même vitesse qu'elle monte, jusqu'à 0. C'est le principe du mode Conflict, vérifié dans SCR_SeizingComponent.c HandleGradualReset : on perd autant de temps qu'on en a passé dehors.
- C8 : un carré est « contesté » (orange sur la carte) pendant une capture qui avance, un combat (joueur et ennemi dans le carré) ou une reprise par l'ennemi. Cet état n'est jamais sauvegardé : il est envoyé à l'affichage à chaque changement.
- F3 : pendant une contre-attaque, un carré bleu occupé 1 min par au moins un assaillant valide, sans aucun joueur valide, repasse rouge. La reprise se fait carré par carré, jamais par zone entière.
- F3 (aide) et E5 : seuls les groupes de la contre-attaque en cours reprennent du terrain (champ m_iAssaultZone ≥ 0 posé par le module des contre-attaques). Patrouilles, postes, camions de renfort et gardes de mission ne repeignent rien.
- Proposition (miroir de C1 et de E6, un seul front) : l'ennemi ne reprend qu'un carré qui touche du rouge par un côté, et seulement dans la zone qu'il attaque (F1). À valider par Jack.
- C7 en miroir : un joueur arrive pendant une reprise, elle se fige. Plus d'assaillant dans le carré, elle redescend.
- F5 : le carré qui contient un point clé ne peut pas être repris tant qu'un joueur valide est à 100 m au plus de ce point clé (rayon repris de D5). En ville, les deux points clés sont gardés chacun (D6).
- B1 et B2 : les carrés de la zone de la base ne sont jamais repris, jamais coupés, jamais perdus.
- B4 : un carré bleu d'une zone voisine de la base ne repasse jamais rouge, quelle que soit la cause (reprise, coupure). Ces zones démarrent rouges (B1) et se prennent normalement.
- E4 : un carré bleu sans chemin de carrés bleus (par les côtés) jusqu'à la base est « coupé » à partir de ce moment. L'heure est prise sur l'horloge réelle et sauvegardée. Après 24 h de coupure, il repasse rouge sans combat, même serveur éteint. Une zone dont tous les carrés bleus sont coupés ne produit plus (information lue par le module de production). Une annonce radio signale une zone qui devient coupée ou de nouveau reliée.
- E5 : aucune autre perte sans combat. Les seules exceptions sont E4 (ici) et H2 (offensive fictive quotidienne, dans un autre module, qui passe par la même porte d'entrée). Les corrections du Staff (J1, J2) restent possibles.
- J3 : front gelé : ni capture, ni reprise, ni perte par coupure. Les compteurs en cours sont vidés au gel et les états contestés effacés. Proposition : au dégel, les carrés encore coupés repartent pour 24 h complètes.
- H3 : les compteurs de capture et de reprise et les états contestés ne vivent qu'en mémoire. Un redémarrage les efface sans rien faire perdre. Seule l'heure de coupure (E4) est sauvegardée, car Jack l'a voulue « même en ton absence ».
- I5 : chaque carré pris ou perdu en combat donne un message radio à tous, par exemple « Radio : carré 074 042 pris — zone Régina (S07) ». Plusieurs carrés de la même zone qui basculent au même passage sont cités sur une seule ligne. Les carrés qui basculent avec une zone entière (D1), par le Staff ou par H2 sont annoncés par zone, par leur propre module.
- A7 et A8 : un carré se désigne par la référence du carré de 100 m de son coin en bas à gauche (nombres pairs, SRP_Fleet.c:802 GridOf), avec le mot « carré ». A6 : la zone s'écrit « Régina (S07) ».
- I7 : aucune annonce Discord et aucun évènement du pont par carré. La ligne de journal de chaque carré part dans une catégorie « FRONT », qui n'a pas de flux public. Les lignes de zone du module (coupée, reliée, perdue par coupure) restent en TERRITOIRE, avec les mots-clés de couleur.
- A2 et B3 (proposition, conflit à trancher) : Erquy est séparée de l'île par la mer, donc C1 l'empêche d'être prise et E4 la couperait toujours. Tant que l'île n'a aucun carré bleu, un de ses carrés de côte peut se prendre (débarquement). Les carrés bleus d'une île ne sont jamais « coupés ».
- Proposition reprise du code actuel (SRP_Territory.c:994, « la garnison ayant existé ») : un carré qui recouvre une localité dont la garnison n'a pas encore été posée ne se prend pas. Sans cette règle, un village laissé sans garnison par le plafond de 3 se prendrait sans combat.
- Tout changement de propriétaire venu d'ailleurs remet à zéro les compteurs du carré et fait recalculer les coupures. Cela vaut pour une zone tombée (D1), le Staff (J1, J2), l'offensive fictive (H2) et la remise à zéro.
### Réglages
- m_iTickSeconds = 5 : période de la boucle de présence, en secondes.
- m_iCaptureMinutes = 15 : minutes de présence sans ennemi pour prendre un carré (C4).
- m_fCaptureDecayRatio = 1 : vitesse à laquelle la progression redescend quand plus personne n'est dans le carré, comparée à la montée. 1 = même vitesse (principe de Conflict), 0,5 = deux fois plus lent (C7).
- m_fMaxVehicleAltitude = 7 : au-delà de cette hauteur en mètres dans un véhicule, un joueur ou un ennemi ne compte pas (C2, même valeur que Conflict).
- m_iExcludeMinutes = 5 : minutes pendant lesquelles un joueur ne compte pas après une téléportation du menu Staff, le Game Master ou une possession (C3).
- m_bExcludeAllTeleported = 1 : tout joueur téléporté par le menu Staff est écarté, pas seulement le Staff (C3, à confirmer).
- m_bExcludeGameMaster = 1 : écarte un joueur qui a le Game Master ouvert (éditeur non limité) ou qui possède une IA (C3).
- m_fStaffJumpMeters = 300 : filet de sécurité. Un membre du Staff à pied qui saute de plus de ce nombre de mètres en un passage est traité comme téléporté (0 = arrêt).
- m_bExcludeGodMode = 1 : un membre du Staff en mode invincible ne compte pas (proposition).
- m_iRetakeSeconds = 60 : secondes d'occupation par un assaillant, sans joueur, pour que l'ennemi reprenne un carré (F3).
- m_fRetakeDecayRatio = 1 : vitesse à laquelle la reprise redescend quand plus aucun assaillant n'est dans le carré.
- m_bRetakeNeedsRed = 1 : l'ennemi ne reprend qu'un carré qui touche du rouge par un côté (miroir de C1).
- m_bRetakeTargetZoneOnly = 1 : l'ennemi ne reprend que les carrés de la zone qu'il attaque.
- m_fKeyPointGuardRadius = 100 : un joueur à moins de ce nombre de mètres d'un point clé empêche la reprise du carré de ce point clé (F5, rayon repris de D5).
- m_bKeyCellNeedsGarrison = 1 : un carré qui recouvre une localité dont la garnison n'a jamais été posée ne se prend pas (reprend la règle de SRP_Territory.c:994).
- m_iCutMinutes = 1440 : minutes d'heure réelle avant qu'un carré bleu coupé de la base repasse rouge (E4, 24 h ; 2 pour les essais).
- m_iCutCheckSeconds = 60 : intervalle entre deux vérifications des coupures, en secondes.
- m_bAnnounceCut = 1 : annonce radio quand une zone est coupée de la base ou de nouveau reliée.
- m_bIslandLanding = 1 : une île sans aucun carré bleu (Erquy) peut se prendre par un carré de côte.
- m_bIslandsNeverCut = 1 : les carrés bleus d'une île sont reliés par la mer et ne sont jamais « coupés ».
- m_bAnnounceCells = 1 : annonce radio de chaque carré pris ou perdu en combat (I5).
- m_bGroupAnnounce = 1 : les carrés d'une même zone qui basculent au même passage sont cités sur une seule ligne.
- m_bTimingDiag = 0 : écrit au journal FRONT la durée d'un passage quand elle dépasse 5 ms.
### Dépendances attendues
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

## Module mod_carte.json
### Résumé
Ce module montre le front aux joueurs, sans rien changer aux règles.
1) La carte du jeu (touche M), la carte de réapparition et celle du Game Master reçoivent un calque dessiné. On y voit les carrés bleus ou rouges, très pâles et toujours affichés, et les carrés disputés en orange. Le front est un trait noir épais, les limites des zones un trait fin, et chaque zone porte son nom, « Régina (S07) ». La zone attaquée est entourée d'orange ou de rouge. Rien ne se règle au zoom et aucune touche n'est ajoutée : tout marche à la manette.
2) Chaque point clé garde une seule icône fixe, qui ne clignote plus. Elle n'apparaît que pour nos zones, les zones au front et les zones attaquées : le renseignement sur l'ennemi reste limité au front.
3) Rien ne s'affiche à l'écran pendant l'avance. La radio annonce chaque carré pris ou perdu, en regroupant quand il y en a beaucoup d'un coup, ainsi que les zones prises ou perdues, les attaques, les zones coupées et le gel du front.
4) L'onglet Territoire du PC liste toutes les zones avec leur propriétaire, la part de carrés à nous et l'attaque en cours.
5) Le menu Staff (F10) permet de passer le carré où l'on se tient ou une zone entière en bleu ou en rouge, de remettre à zéro une zone ou toute la campagne, de revenir à la sauvegarde d'un jour donné et de geler le front.
### Règles
- I1 : la carte montre les carrés bleus (à nous), rouges (ennemis) et orange (contestés), le front en trait épais et le nom de chaque zone.
- I2 : couleurs toujours affichées et très transparentes (alpha 0,20 ; 0,40 pour l'orange), identiques à tous les zooms, sans touche pour masquer le calque.
- I3 : couleurs et noms partout. L'icône du point clé (PC ou QG ennemi) n'est montrée que pour nos zones, les zones au contact du front (définition E1) et les zones attaquées. Au PC, la présence ennemie (« N ennemis repérés en zone ») n'est écrite que pour les zones au front ; les zones ennemies de l'arrière sont marquées « pas de renseignement ».
- C8 : l'orange est un état d'affichage seulement. Le front se calcule sur le propriétaire réel du carré, jamais sur l'orange.
- E2 : une poche rouge entourée de bleu garde sa couleur, et son front se dessine en boucle fermée.
- A6 : étiquette de zone « Régina (S07) ». Dans les menus, la clé est le code (S07).
- A7 et A8 : un carré se désigne par la référence de carte de son coin sud-ouest (« carré 074 042 », toujours pair, via SRP_FleetManagerComponent.GridOf). Le mot employé est « carré ».
- D3, D4 et D6 : une icône par point clé, posée à sa position réelle ; deux en ville (centre, QG ennemi). Texte : « PC ennemi S08 » ou « QG ennemi S08 », puis « Point clé S07 » une fois à nous.
- I4 : rien à l'écran pendant l'avance (ni barre, ni message d'entrée de zone). L'ancien texte « capture x / N min » du drapeau disparaît. La progression se lit sur la carte, sur le carré orange.
- I5 : la radio annonce chaque carré pris par les joueurs ou repris par l'ennemi (F3), plus les zones prises ou perdues, les attaques (annonce, vagues, défense), les coupures (E4), le gel (J3), la victoire (B3) et l'offensive hors ligne (H2).
- I7 : les messages par carré restent en jeu (radio seulement). Ils ne passent ni par le journal TERRITOIRE ni par Discord ; seuls les événements de zone y vont.
- D1 : la radio annonce la saisie du point clé et la moitié des carrés atteinte (« reste le PC ennemi »).
- G15 : une contre-attaque reste une alerte radio (« Renseignement : … assaut dans 15 minutes »), pas une mission.
- E4 : une zone coupée est signalée à la radio, au PC et au Staff (« coupée depuis 3 h, perdue dans 21 h »), avec un rappel une heure avant sa perte.
- I10 : l'onglet Territoire du PC liste les zones avec leur propriétaire, la part bleue (« 6/25 carrés ») et l'attaque en cours ou annoncée.
- J1 : le Staff passe en bleu ou en rouge soit le carré où se tient son personnage, soit une zone entière.
- J2 : remise à zéro d'une zone ou de toute la campagne, et retour à une sauvegarde datée (une par jour), chaque fois avec deux validations.
- J3 : un interrupteur « front gelé » dans l'onglet Territoire du Staff (ni capture, ni perte, ni contre-attaque), annoncé à la radio et affiché en tête du PC.
- B2 : la zone de la base se dessine comme les autres (bleue). Le Staff ne peut pas la passer en rouge : refus expliqué.
- H3 : rien à afficher après un redémarrage. Les carrés orange et les attaques non sauvegardés disparaissent d'eux-mêmes, car la version de la grille change.
### Réglages
- m_bMapLayer = 1 : dessiner le calque du front.
- m_bOnFullscreen = 1, m_bOnSpawn = 1 (modes SPAWNSCREEN et PLAIN), m_bOnEditor = 1 (Game Master) : cartes qui reçoivent le calque.
- m_BlueFill = "0.16 0.42 0.85 0.20" : remplissage bleu, très transparent (I2).
- m_RedFill = "0.85 0.18 0.18 0.20" : remplissage rouge (I2).
- m_ContestedFill = "1 0.55 0.1 0.40" : orange des carrés contestés (C8).
- m_FrontColor = "0.08 0.08 0.08 0.90", m_fFrontWidth = 4 px, m_FrontOutlineColor = "1 1 1 0.55", m_fFrontOutlineWidth = 7 px : le front épais (I1).
- m_bZoneBorders = 1, m_ZoneBorderColor = "0.1 0.1 0.1 0.35", m_fZoneBorderWidth = 1.5 px : limites de zones en trait fin.
- m_bAttackOutline = 1, m_AttackWarnColor = "1 0.55 0.1 0.9", m_AttackColor = "0.9 0.1 0.1 0.9", m_fAttackOutlineWidth = 3 px : contour de la zone attaquée.
- m_bZoneLabels = 1, m_LabelColor = "0.08 0.08 0.08 0.85", m_fLabelSize = 15 px, m_fLabelCharWidth = 0.55 : noms des zones.
- m_fLabelFullMinPxPerKm = 150 : sous ce zoom, le code seul (« S07 »), au-dessus « Régina (S07) ». Ne touche pas aux couleurs.
- m_bCaptureText = 1, m_fCaptureTextMinPx = 40, m_fCaptureTextSize = 12 : progression écrite sur le carré orange, quand un carré fait au moins 40 px à l'écran.
- m_iMaxPointsPerLine = 190 : points par tronçon de ligne (limite moteur de 400).
- m_iRedrawMinMs = 0 : délai minimal entre deux retracés pendant un glissé (0 = à chaque image).
- m_iZOrderOffset = 0 et m_sLayerParent = "" : rang et parent du canevas (vide = MapFrame, sinon le parent de MapWidget), à ajuster d'après le test 2.
- m_bTextWidgetFallback = 0 : 1 pour écrire les noms en TextWidget si TextDrawCommand ne s'affiche pas.
- m_bDiag = 0 (liste les widgets voisins et leurs rangs) et m_bCalibration = 0 (motif de calage) : outils des deux essais au Workbench.
- m_bKeyPointMarkers = 1, m_iKeyPointIcon = 11, m_iColorEnemy = 4, m_iColorOurs = 7, m_iIconWarning = 49, m_iColorWarning = 1, m_iIconAttack = 50, m_iColorAttack = 4 : icônes des points clés (valeurs actuelles).
- m_bHideRearKeyPoints = 1 : pas d'icône pour les zones ennemies loin du front (I3).
- m_bRadioCells = 1 : une ligne radio par carré pris ou perdu (I5).
- m_iRadioMergeAbove = 3 et m_iRadioMaxRefs = 6 : au-delà de 3 carrés dans le même passage de 5 s, une seule ligne, avec au plus 6 références.
- m_bJournalCells = 0 : les carrés ne vont ni au journal ni à Discord (I7).
- m_bRadioHalf = 1 : annonce « la moitié des carrés est à nous, reste le PC ennemi » (D1).
- m_bRadioStaff = 0 : pas de radio pour les corrections du Staff.
- m_iCutReminderMinutes = 60 : rappel radio avant la perte d'une zone coupée (E4).
- m_bPCRearEnemies = 1 : le PC liste aussi les zones ennemies de l'arrière, en gris et sans renseignement.
- m_iSavesShown = 30 : nombre de sauvegardes datées proposées au Staff.
### Dépendances attendues
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

## Module mod_ennemi.json
### Résumé
Le module 4 décide où et quand l'ennemi se montre. Il a deux rôles : défendre le rouge et reprendre du bleu.
- **Défense du rouge.** Les garnisons de village vont d'abord aux zones au contact du front. Un village de l'arrière reçoit quand même la sienne si un joueur s'en approche (raid). Chaque garnison se pose au point clé que tu as placé à la main, et en ville le QG se pose à ton 2e point clé.
- **Mémoire des pertes.** Chaque village garde en mémoire ses morts (sauvegardés). Il se regarnit d'un quart par heure réelle et redevient plein en 4 h, même après un redémarrage.
- **Postes du front.** Des postes de 2 à 4 soldats tiennent environ 1 carré rouge du front sur 4 : les carrefours, les routes, puis les hauteurs. Ils se posent à l'approche d'un joueur, 4 au plus à la fois. Ils ont une réserve dans la limite des 100 soldats, et un poste anéanti ne revient qu'après 6 h de serveur ou au redémarrage.
- **Patrouilles.** Elles n'apparaissent plus que côté rouge, avec de rares infiltrations côté bleu à moins de 400 m du front.
- **Contre-attaques.** Elles visent seulement les zones bleues au contact du front, jamais les zones voisines de la base, et le plus souvent la dernière prise. Il en part une toutes les 3 h à menace basse, toutes les 1 h à menace haute, à partir de 4 joueurs connectés, annoncée 15 min avant, en 3 vagues. Les vagues partent des carrés rouges d'en face et reprennent le terrain carré par carré (1 min sans joueur). La défense est gagnée quand la zone est nettoyée après l'arrivée de la dernière vague.
- **Offensive fictive.** Une fois par jour, la nuit, serveur vide, un tirage selon la menace peut faire tomber une zone du front. C'est annoncé sur Discord puis par radio à chacun à sa première apparition.
- **Hélico, sirène, camions.** L'hélico peut venir partout en territoire rouge. La sirène et les camions restent attachés aux villages et à leur point clé. Les zones sans village n'ont que des postes et des patrouilles.
### Règles
- F11 : garnison posée seulement dans une localité « hostile » (zone rouge et point clé pas encore saisi, D4). Choix du prochain village : on prend le plus petit score. Score = distance au joueur actif le plus proche, moins 1 000 m si la zone du village est au contact du front (E1 : un de ses carrés touche du bleu par un côté).
- F11 : un village du front se garnit si un joueur actif (hors base, hors délai de grâce) est à 2 000 m ou moins. Un village de l'arrière se garnit à la même distance, mais passe après ceux du front (raid possible, sans capture possible, D2).
- F11 (règle d'aujourd'hui gardée) : 3 garnisons vivantes au plus. Plafond atteint : la garnison au plus mauvais score cède sa place si le nouveau village a un score meilleur d'au moins 300. Seulement si aucun joueur n'est à 1 200 m d'elle, qu'elle n'est pas vue, pas en alerte et pas sous attaque.
- F11 + D3 : le centre de la garnison, les entrées (FindTownEntrances), le poteau de sirène et le stationnement des camions se calent sur le point clé posé par Jack, et non plus sur le nom de la carte.
- D6 : en ville (bourg compris), le QG (PlatoonHQ avec son opérateur radio) tient le 2e point clé (QG ennemi) et la plus grande section tient le point clé du centre. Les deux sont des groupes de commandement : ils ne partent pas aider. Les sections suivantes alternent entre les deux points clés.
- F12 : les pertes sont comptées par localité : soldats prévus moins survivants en état de combattre (les inconscients comptent comme pertes). Elles sont relevées à chaque disparition de garnison (retrait, éviction, anéantissement, capture abandonnée) et à chaque sauvegarde pour une garnison vivante.
- F12 : pertes courantes = pertes × max(0 ; 1 − heures réelles écoulées / 4). On regarnit donc un quart des pertes par heure, et le village est plein 4 h après sa dernière perte, même serveur redémarré (heure Unix sauvegardée). Effectif posé = effectif décidé (10/20/30, +3 par joueur au-delà du 2e, +0 à 5 selon la menace) moins les pertes courantes. En dessous de 2 soldats, rien n'est posé : le village est vide et prenable.
- F12 : capture de la zone : les pertes du village sont remises à zéro. Si l'ennemi reprend la zone, sa garnison est faite des assaillants convertis, comme aujourd'hui.
- F7 : un carré rouge sans village n'est défendu que par un poste du front (2 à 4 soldats retranchés) ou par les patrouilles de fond.
- F8 : un poste est possible seulement sur un carré rouge du front (C1/E1 : il touche un carré bleu par un côté), hors du cercle d'une localité hostile et hors de la zone en contre-attaque. Note des carrés : carrefour 3, route principale 2, hauteur 2 (+1 si elle s'ajoute à une route), petite route 1, rien 0. On garde les mieux notés, avec une note d'au moins 1, jusqu'à 25 % des carrés du front, jamais deux postes dans des carrés voisins.
- F8 : pose quand un joueur actif est à 1 000 m ou moins du carré : hors de vue, à 300 m au moins de tout joueur (sinon nouvel essai toutes les 30 s, jamais de repli sur un joueur). 4 postes vivants au plus, une pose par passage. Taille : 4 soldats au carrefour et sur une route principale, 2 sur une hauteur et une petite route. Ils comptent dans les 100 soldats et les 30 groupes, dans une réserve de 12 soldats et 4 groupes.
- F9 : un poste est anéanti quand plus aucun de ses soldats n'est en état de combattre. Aucun poste sur ce carré pendant 6 h de fonctionnement du serveur. Ce souvenir n'est pas sauvegardé : un redémarrage le rend. Un poste seulement retiré (plus aucun joueur à 1 500 m pendant 5 min, et pas vu) revient au complet à la prochaine approche.
- F10 : une patrouille de fond n'apparaît que sur un carré rouge. Côté bleu, seulement à 400 m ou moins (2 carrés) d'un carré rouge et sur un tirage de 15 % (infiltration). Un joueur à plus de 400 m du rouge côté bleu n'attire aucune patrouille. Une patrouille sur route n'est acceptée que si ses deux extrémités respectent la même règle. Les patrouilles ne reprennent jamais de carré (F3, aide).
- F1 + B4 : peut être visée par une contre-attaque une zone bleue dont un carré bleu touche par un côté un carré rouge de la masse rouge principale. Jamais la zone de la base (B2) ni une zone voisine de la base (imprenables). Les poches rouges de moins de 10 carrés ne comptent pas.
- F2 : 70 % du temps, la cible est, parmi les zones possibles, celle prise le plus récemment (date de prise sauvegardée). Sinon, une autre au hasard.
- F6 : délai entre deux contre-attaques = 180 − 12 × menace minutes (3 h à menace 0, 1 h à menace 10), plus ou moins 20 %. L'horloge n'avance que si au moins 4 joueurs sont connectés (H4) et que le front n'est pas gelé (J3).
- H4 : il faut 4 joueurs connectés au moment du départ. Annonce 15 min avant : radio en jeu et Discord, pas de mission (G15).
- Vagues : une vague toutes les 5 min, 3 au plus. Taille = min(1 groupe par tranche de 2 joueurs à 3 km + menace/4, 4 au plus ; place du budget local à 1 200 m), comme aujourd'hui. Si la 1re vague ne peut poser personne, l'attaque est annulée sans effet.
- Origine des vagues (remplace NearestEnemySectors à 4 km) : les carrés rouges qui touchent la zone visée. Point de départ reculé de 3, puis 2, puis 1 carré dans le rouge, le premier à 500 m au moins de tout joueur et hors de vue. Deux axes : l'accès par la meilleure route, puis le plus écarté en angle (60 ° au moins). Le premier groupe part tout de suite, les suivants à 60-90 s d'écart.
- F3 : seuls les assaillants de la contre-attaque reprennent du terrain, et seulement dans la zone visée. Un carré bleu qui touche du rouge par un côté repasse rouge après 60 s cumulées avec au moins un assaillant en état de combattre et aucun joueur vivant et conscient au sol dans le carré. Le cumul se fige si un joueur est là et redescend s'il n'y a plus d'assaillant. Chaque carré repris est annoncé à la radio (I5).
- F5 : les carrés à moins de 100 m d'un point clé de la zone ne peuvent pas être repris tant qu'un joueur vivant et conscient est à 100 m ou moins de ce point clé.
- F4 : la dernière vague est « arrivée » quand chacun de ses groupes est entré dans la zone ou est hors de combat, ou 10 min après sa pose. Ensuite, dès qu'aucun assaillant en état de combattre ne reste dans les carrés de la zone : défense réussie, menace −1, radio « Zone X défendue ». Les traînards se retirent hors de vue (RetireLater).
- Zone repassée rouge pendant l'attaque (règle du module zones) : les assaillants dans la zone deviennent la garnison de sa localité (conversion d'aujourd'hui). Zone sans localité : ils se retirent hors de vue. La menace −1 de la perte est comptée par le module zones, pas ici (G9, sans doublon).
- Attaque enlisée (réglage proposé) : 60 min après le début de l'assaut, l'ennemi rompt le contact. Les assaillants se retirent hors de vue, les carrés repris restent rouges (E5), la menace ne change pas.
- H3 : un redémarrage annule l'attaque en cours sans perte, et oublie les postes détruits et l'horloge des attaques. Seules les pertes des villages (F12) et la date de l'offensive fictive sont gardées.
- J3 : front gelé : pas de contre-attaque (horloge arrêtée, attaque en cours annulée sans effet), pas de reprise de carré, pas d'offensive fictive. Garnisons, postes et patrouilles continuent.
- H2 : une seule offensive fictive par jour calendaire (date sauvegardée). Elle a lieu entre 3 h et 7 h, heure du serveur, quand aucun joueur n'est connecté depuis 30 min. Elle vise au plus une zone du front, choisie comme en F1 et F2, jamais une zone voisine de la base (B4).
- H2 : chance de réussite = 10 + 5 × menace pour cent, 60 au plus. Réussite : toute la zone repasse rouge, comme une perte de zone (dépôt et stock perdus, menace −1). Échec : rien ne change. Dans les deux cas : ligne au journal TERRITOIRE, évènement Discord aussitôt, et message radio à chaque joueur à sa première apparition dans les 24 h.
- G12 : l'hélicoptère de recherche peut être appelé pour toute alerte avec des joueurs VUS dont le point est dans un carré rouge (ou à 900 m d'une mission, comme aujourd'hui), jamais à moins de 1 500 m de la base. Les tirages, le repos et la radio de garnison du #11 ne changent pas. Il arrive du côté du rouge.
- Sirène : un poteau par localité hostile garnie, au point clé 1. Elle sonne quand un soldat de la garnison voit un joueur à moins du rayon de la localité + 150 m du point clé. Les postes et les patrouilles n'ont pas de sirène.
- Camions : un camion d'entrée part quand un joueur arrive à 500 m du point clé. Le camion d'alerte suit les règles du #69. Les départs se font seulement depuis un carré rouge (sinon repli sur n'importe quel départ), depuis les points clés des localités rouges voisines puis la couronne de sondes.
- Zones sans localité : ni garnison, ni sirène, ni camion. Seulement les postes du front et les patrouilles. Elles peuvent être contre-attaquées (cible = centre de leurs carrés bleus) et visées par l'offensive fictive.
- C6 (service rendu au module capture) : décompte à chaque passage, par carré, des ennemis au sol en état de combattre (vivants, conscients, hors hélicoptère), tous groupes confondus : garnison, poste, patrouille, garde de mission, assaillant, équipage.
### Réglages
- Garnisons, SRP_TerritoryComponent :
- m_iGarrisonMaxActive = 3 (déjà en place) : garnisons vivantes en même temps.
- m_fGarrisonSpawnDistance = 2000 (déjà en place) : un village du FRONT se garnit quand un joueur actif est à … m de son point clé.
- m_fRearGarrisonSpawnDistance = 2000 : même seuil pour un village de l'ARRIÈRE (raid).
- m_fFrontPriorityBonus = 1000 : avance, en mètres, donnée aux villages du front pour le choix et l'éviction (« front d'abord »).
- m_iGarrisonMinSoldiers = 2 : sous cet effectif, une fois les pertes retirées, rien n'est posé (village vide).
- Mémoire des pertes et offensive fictive, SRP_FrontEnemyComponent :
- m_bLossMemory = 1 : mémoire des pertes activée.
- m_fLossRefillHours = 4.0 : heures réelles pour que le village soit de nouveau plein (un quart par heure).
- m_bFictiveOffensive = 1 : offensive fictive quotidienne.
- m_iOffensiveHourStart = 3 et m_iOffensiveHourEnd = 7 : fenêtre horaire, heure du serveur.
- m_iOffensiveEmptyMinutes = 30 : serveur vide depuis … minutes.
- m_iOffensiveChanceBase = 10, m_iOffensiveChancePerThreat = 5, m_iOffensiveChanceMax = 60 : chance de réussite, en pour cent.
- m_iOffensiveNewsHours = 24 : durée pendant laquelle chaque joueur reçoit le message radio à sa première apparition.
- Postes du front, SRP_FrontEnemyComponent :
- m_bFrontPosts = 1 : postes du front activés.
- m_iPostCellPercent = 25 : part des carrés du front tenue par un poste.
- m_iPostMaxActive = 4 : postes vivants en même temps.
- m_fPostSpawnDistance = 1000 : un poste se pose quand un joueur actif est à … m de son carré.
- m_fPostPlayerMinDistance = 300 : distance minimale aux joueurs à la pose (toujours hors de vue).
- m_fPostDespawnDistance = 1500 et m_iPostDespawnMinutes = 5 : retrait discret quand plus aucun joueur n'est à cette distance pendant ce temps.
- m_iPostRespawnHours = 6 : un poste anéanti ne revient qu'après … heures de fonctionnement du serveur, ou au redémarrage.
- m_iPostSoldiersKey = 4 : taille au carrefour et sur une route principale.
- m_iPostSoldiersMinor = 2 : taille sur une hauteur et une petite route.
- m_aPostKeyPrefabs = vide : prefabs des postes de 4 (proposé : Team_AT, Team_LAT, Team_Suppress RHS). Vide = tirage par taille.
- m_aPostMinorPrefabs = vide : prefabs des postes de 2 (proposé : SentryTeam, MachineGunTeam). Vide = sentinelles réglées.
- m_aPostFortPrefabs = vide : décor retranché (sacs de sable). Vide = aucun décor.
- m_fPostDefendRadius = 15 et m_fPostHoldRadius = 25 : point Defend et maintien de position du poste.
- m_fPostRoadMinWidth = 3.0 : largeur de route minimale prise en compte (à mesurer au Workbench).
- m_fMainRoadWidth = 6.0 : largeur à partir de laquelle une route est « principale » (à mesurer au Workbench).
- m_fHeightProminence = 15 : mètres au-dessus des carrés voisins pour qu'un carré compte comme une hauteur.
- Capacité, SRP_FrontEnemyComponent :
- m_iPostSoldierReserve = 12 : soldats gardés pour les postes dans le plafond des 100, seulement quand un poste est possible près d'un joueur.
- m_iPostGroupReserve = 4 : groupes gardés pour les postes dans le plafond des 30.
- m_iAIHeadroom = 10 : IA actives laissées libres sous la limite du jeu avant de poser un poste.
- Patrouilles de fond (F10), SRP_FrontEnemyComponent :
- m_iInfiltrationChance = 15 : chance, en pour cent, d'une patrouille côté bleu.
- m_iInfiltrationDepthCells = 2 : carrés, soit 400 m, entre une infiltration et le rouge, au plus.
- Contre-attaques, SRP_FrontEnemyComponent :
- m_iAttackMinPlayers = 4 (déplacé) : joueurs connectés minimum.
- m_iAttackMinutesThreat0 = 180 et m_iAttackMinutesThreat10 = 60 : délai entre deux contre-attaques à menace 0 et 10, linéaire entre les deux.
- m_iAttackJitterPercent = 20 : aléa du délai, en plus ou en moins.
- m_bAttackClockNeedsQuorum = 1 : l'horloge n'avance qu'à 4 connectés ou plus.
- m_iAttackWarningMinutes = 15, m_iAttackWaveMinutes = 5, m_iAttackMaxWaves = 3, m_iAttackStaggerMinSeconds = 60, m_iAttackStaggerMaxSeconds = 90 (déplacés) : annonce, vagues, échelonnement.
- m_iAttackLastTakenPercent = 70 : chance de viser la dernière zone prise encore au contact.
- m_iAttackAxes = 2 : axes d'arrivée des vagues.
- m_iRetakeSeconds = 60 : occupation d'un carré sans joueur nécessaire pour le reprendre.
- m_bRetakeTargetZoneOnly = 1 : l'ennemi ne reprend que les carrés de la zone visée.
- m_fKeyPointGuardRadius = 100 : rayon autour d'un point clé où un joueur vivant et conscient empêche la reprise.
- m_iWaveArrivalMaxMinutes = 10 : au-delà, la dernière vague est réputée arrivée.
- m_iAttackMaxMinutes = 60 : attaque enlisée, l'ennemi rompt le contact après … minutes d'assaut.
- m_iPocketMinCells = 10 : une poche rouge plus petite ne lance pas de contre-attaque.
- Hélicoptère et camions :
- m_bHeliRedTerritory = 1 (SRP_EnemyComponent) : hélicoptère possible pour toute alerte en carré rouge.
- m_bOriginRedOnly = 1 (SRP_EnemyTruckComponent) : les camions partent d'un carré rouge (repli sinon).
### Dépendances attendues
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

## Module mod_grille.json
### Résumé
Le socle remplace les 30 cercles par une grille de carrés de 200 m (64 × 64) posée sur toute la terre d'Everon : 1 395 carrés en jeu, île d'Erquy comprise, et les îlots vides retirés. Au démarrage, le serveur calcule tout seul trois choses. D'abord la zone de la base : 162 carrés à moins de 1,5 km, toujours bleus. Ensuite les zones : ce sont les carrés de 1 km de la carte, coupés là où la mer les sépare et fusionnés quand il en reste un petit bout. On obtient 66 zones, de S01 à S66, dont 30 contiennent un village. Enfin leur nom, sur le modèle « Régina (S07) ».
Tu poses toi-même les points clés : un centre par localité, plus un QG pour chaque ville ou bourg. Tant qu'un point clé n'est pas posé, le socle se rabat sur l'endroit où le nom est écrit sur la carte. Ton fichier de retouches permet de déplacer un carré d'une zone à l'autre, et un rapport te liste les villages coupés par une limite de zone.
Le socle enregistre la couleur de chaque carré et de chaque zone, les points clés pris, la menace, les dépôts et la mémoire de l'ennemi, avec une copie datée par jour pour revenir en arrière. Il envoie la carte aux joueurs à leur connexion, puis seulement les carrés qui changent.
Il applique lui-même trois règles de fond : base et zones voisines imprenables, perte d'une zone coupée de la base depuis 24 h, victoire quand tout est bleu. Pour les autres modules, il répond à des questions simples : à qui est ce carré, quelle zone ici, quelles zones touchent le front, cette zone est-elle reliée à la base.
### Règles
- A1 : un carré fait 200 m. La grille de 64 × 64 part de l'origine de la carte : index = gz × 64 + gx, avec gx = floor(x / 200) et gz = floor(z / 200).
- A2 : un carré est en jeu s'il a au moins 10 % de terre. Sur 16 points testés (4 × 4), il en faut au moins 2 plus hauts que le niveau de la mer. Parmi les blocs de terre d'un seul tenant, on garde la grande île et ceux qui portent le nom d'une localité (Erquy, 34 carrés). Les autres îlots (20, 16, 5, 1, 1 et 1 carrés sur le relief du site) comptent comme de la mer.
- A3 : une zone est un bloc de 1 km, soit 5 × 5 carrés calés sur le quadrillage kilométrique de la carte. Une zone est toujours d'un seul tenant : deux morceaux d'un même bloc séparés par la mer ou par la base font deux zones.
- A3 (lu avec G4 et le garnisonnement par localité) : un bloc qui contient deux localités est coupé en deux zones. Chaque carré va à la localité dont le nom est le plus proche : Montignac 12 carrés et Gravette 13, Meaux 21 et Tyrone 3. Choix à confirmer.
- A2 et A3 : une île autre que la grande île forme une seule zone, qui n'est jamais fusionnée. C'est le cas d'Erquy.
- A4 : un morceau de moins de 5 carrés sans localité est versé dans le morceau voisin qui partage le plus de côtés avec lui. En cas d'égalité, on prend le plus grand, puis le premier. Un morceau qui ne touche que la base est versé dans la base. Une zone avec localité n'est jamais absorbée. Résultat sur le relief du site : 66 zones de 3 à 34 carrés.
- A4 : le fichier front_retouches.txt est lu au démarrage, avec une ligne par carré (« 074 042 -> S07 » ou « -> Régina »). Un déplacement est refusé et noté au journal si le carré ne touche pas la zone d'arrivée par un côté, s'il vide ou coupe en deux sa zone de départ, ou s'il touche à la base. Les lignes sont appliquées dans l'ordre.
- A6 : le nom d'une zone est « Nom (S07) ». Le nom est celui de la localité de la zone ; sinon, c'est le lieu-dit le plus proche (colline, lieu, crête, vallée, île, ou une localité de la base). Chaque nom ne sert qu'une fois : un doublon passe au nom libre suivant. Sans nom à moins de 2,5 km, la zone s'appelle « Secteur (S41) ». La base s'appelle « Base de Levie ».
- A6 : les zones sont numérotées de S01 à S66, de la plus proche à la plus éloignée de la base, en mesurant jusqu'au centre de leur bloc de 1 km. La numérotation est faite avant les retouches, donc les retouches ne changent pas les codes. Le code est la clé de sauvegarde.
- A7 : un carré se désigne par la référence de carte de son coin sud-ouest, en carrés de 100 m. Les deux nombres sont pairs : « 074 042 ». Le fichier de retouches accepte aussi un carré de 100 m situé à l'intérieur (075 043 donne le même carré).
- A8 : tous les textes disent « carré ».
- B1 : une nouvelle campagne commence tout en rouge sauf la base. Les points clés, les dates, les stocks, les dépôts et la mémoire de l'ennemi sont remis à zéro, et la menace revient à 0.
- B2 : la base comprend les carrés en jeu dont le centre est à 1 500 m au plus du marqueur SRP_SpawnBase (7415 / 4334), soit 161 carrés, plus 1 carré isolé qu'on y verse. Ces carrés sont toujours bleus : personne ne peut les changer, pas même le Staff. Les localités à moins de 1,5 km (Levie, Laruns, La Valette, Camurac) restent des localités de la base, comme aujourd'hui : jamais un objectif ni un point clé.
- B3 : quand toutes les zones hors base sont bleues, c'est la victoire. Elle est annoncée à la radio et sur Discord, et le front se gèle tout seul. La nouvelle campagne démarre au prochain démarrage du serveur (réglable en minutes).
- B4 : une zone qui touche la base par un côté (environ 9 zones, dont Morton, Régina et Durras) ne peut plus repasser rouge une fois prise. Ses carrés non plus : ni contre-attaque, ni offensive fictive, ni isolement. Seul le Staff peut les changer.
- C1 (support) : CanBlueCapture accepte seulement un carré rouge, hors base, qui touche un carré bleu par un côté (ou par la liaison maritime d'Erquy), et seulement si le front n'est pas gelé.
- C8 : chaque carré a un propriétaire enregistré, bleu ou rouge. Il a aussi un état vivant, jamais enregistré : aucun, capture, combat ou reprise ennemie. Les trois derniers s'affichent en orange (contesté). Cet état est envoyé aux joueurs au plus une fois par seconde.
- D3 : les points clés sont posés à la main. Pour le centre d'une localité, le socle prend dans l'ordre la ligne « point » du fichier, puis le prefab posé au Workbench, puis l'endroit où le nom est écrit sur la carte (avec un avertissement au rapport).
- D6 : une ville ou un bourg a deux points clés, le centre et le QG. Un QG pas encore posé n'est pas remplacé : la ville n'a alors que son centre, et le rapport le signale. Une localité appartient à la zone du carré de son centre ; son QG compte pour la même zone.
- D1 (support) : le socle garde les points clés pris (enregistrés) et compte les carrés bleus de chaque zone. Quand une zone tombe, tous ses carrés passent bleus et ses points clés sont marqués pris. Une zone perdue repasse rouge avec ses points clés.
- E1 : une zone rouge est au contact du front dès qu'un de ses carrés est bleu ou touche un carré bleu par un côté (ou par la liaison maritime d'Erquy).
- E2 : aucune règle de poche. Le socle ne calcule pas d'encerclement.
- E4 : la liaison à la base se calcule par un parcours en largeur qui part des carrés de base et passe par les carrés bleus. Une zone bleue sans aucun carré atteint est coupée : on note l'heure réelle, qui est enregistrée et continue de compter serveur éteint. Au bout de 24 h, la zone et ses carrés repassent rouges (radio et Discord), sauf pour une zone B4. La minuterie ne court pas pendant un gel.
- E5 : en dehors d'E4 (isolement) et de H2 (offensive fictive), le socle ne fait jamais changer un carré sans combat : pas d'usure.
- E6 : un seul front qui part de la base. La liaison maritime vers Erquy (voir les questions) ne crée pas de second front : elle relie seulement l'île à la côte la plus proche.
- F1 et F3 (support) : GetBlueFrontZones donne les zones bleues qui touchent du rouge. CanRedRetake refuse un carré de base, un carré d'une zone B4 déjà prise, et tout carré pendant un gel.
- F12 (support) : les pertes de garnison et leur date sont enregistrées pour chaque localité.
- G6 et G7 (support) : le stock, le dépôt (présence et position) et la ressource choisie (ligne « ressource » du fichier, G5) sont enregistrés pour chaque zone. Les dates de prise et de perte le sont aussi (F2 : dernière zone prise).
- G9 (support) : le socle garde la menace, de 0 à 10. Ce sont les modules qui décident des +1 et des –1.
- H3 : rien de ce qui est en cours n'est enregistré (états contestés, attaques, captures). On garde les propriétaires, les points clés pris, la mémoire, les dépôts, la minuterie d'isolement, le gel et la menace.
- I6, I7 et I8 (support) : la grille (un caractère par carré), les zones et un numéro de version sont exportés pour le pont vers le bot.
- J1 : le Staff peut forcer une zone entière, ou le carré où il se trouve, en bleu ou en rouge, sans condition de voisinage. La base reste intouchable.
- J2 : trois outils pour le Staff. Remise à zéro complète (nouvelle campagne), remise à zéro d'une zone, retour à la copie datée d'un jour choisi. Une copie est faite par jour, au premier enregistrement du jour, et on garde les 30 dernières.
- J3 : l'interrupteur « front gelé » est enregistré. Il refuse tout changement de carré ou de zone qui ne vient pas du Staff, efface les états contestés et suspend la minuterie E4.
- K1 : au premier démarrage de la nouvelle version, campagne n° 1 et menace à 0. L'ancien territoire.json est archivé sous territoire_ancien_AAAA-MM-JJ.json.
- K2 : une seule fois, dès que les dépôts de la base sont comptés, le stock des anciens secteurs à nous est versé à la base : munitions pour un hameau, vivres pour le reste (150 vivres et 36 munitions sur le serveur en ligne). Le versement est marqué dans front.json pour ne jamais se répéter.
### Réglages
- m_fCellSize = 200 : taille d'un carré en mètres (A1).
- m_fWorldSize = 12800 : taille de la carte utilisée si le jeu ne la fournit pas.
- m_iLandSamples = 4 : points testés par côté d'un carré (4 × 4 = 16).
- m_fLandMinShare = 0.10 : part de terre minimale pour qu'un carré soit en jeu (A2).
- m_fLandMinHeight = 0 : un point est terre s'il dépasse le niveau de la mer de ce nombre de mètres.
- m_bKeepIslandsWithLocality = 1 : garder les îles qui portent une localité, comme Erquy (A2).
- m_bIslandSingleZone = 1 : une île secondaire forme une seule zone.
- m_bSeaLinks = 1 : liaison maritime virtuelle entre une île et la côte la plus proche, pour qu'Erquy soit prenable (proposition).
- m_iBlockCells = 5 : côté d'un bloc de zone, en carrés (5 × 200 m = 1 km) (A3).
- m_iZoneMinCells = 5 : un morceau sans localité plus petit que ce nombre de carrés est fusionné dans un voisin (A4).
- m_bSplitMultiLocality = 1 : un bloc qui contient deux localités est coupé en deux zones.
- m_fNameMaxDistance = 2500 : un lieu-dit plus loin que cette distance (en mètres) ne donne pas son nom à la zone (A6).
- m_sCodePrefix = S : préfixe des codes de zone, S07 (A6).
- m_sBaseZoneName = Base de Levie : nom de la zone de la base.
- m_sBaseMarkerName = SRP_SpawnBase : entité qui marque la base.
- m_fBaseRadius = 1500 : rayon de la base en mètres, mesuré jusqu'au centre des carrés (B2) ; les localités plus proches sont celles de la base.
- m_bShieldBaseNeighbours = 1 : une zone qui touche la base ne peut plus être perdue une fois prise (B4).
- m_fKeyPointMaxDistance = 600 : un point clé posé sans nom est rattaché à la localité la plus proche dans ce rayon (mètres).
- m_fVillageCheckRadius = 200 : rayon, en mètres, du relevé des villages coupés par une limite de zone (rapport).
- m_iCutLossHours = 24 : heures réelles au bout desquelles une zone bleue coupée de la base repasse rouge (E4).
- m_iSaveDelaySeconds = 60 : délai maximal en secondes avant d'enregistrer après un changement de carré.
- m_iDailyKeep = 30 : nombre de copies datées gardées (J2).
- m_iVictoryResetMinutes = 0 : minutes entre la victoire et la nouvelle campagne ; 0 = au prochain démarrage du serveur (B3).
- m_iLiveSendMinMs = 1000 : intervalle minimal d'envoi des carrés contestés, en millisecondes (C8).
- m_iMigrationDelaySeconds = 20 : délai avant de verser l'ancien stock à la base (K2).
- m_iMapRetrySeconds = 2 et m_iMapRetryMax = 30 : attente de la carte au démarrage (intervalle entre essais, nombre d'essais).
### Dépendances attendues
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

## Module mod_migration.json
### Résumé
Ce module fait la bascule d'un seul bloc et sans retour (K3, K4) : il retire la capture par cercles de SRP_Territory.c et branche le reste du mod sur la nouvelle carte (carrés de 200 m, zones de 1 km, front).
Le composant garde son nom et sa ligne dans le prefab. Ce qui sert aux 30 localités est conservé : garnison, radio, sirène, camions et dépôt. Les cercles, les drapeaux qui clignotent, le marqueur posé à la main et l'ancienne sauvegarde disparaissent.
Au premier démarrage, l'ancien territoire.json est archivé et une nouvelle campagne commence : tout est rouge sauf la base, menace à 0 (K1). Les paquets en attente sont versés une seule fois aux dépôts de la base : 150 vivres et 36 munitions sur le serveur en ligne (K2).
Le module donne aussi :
- la liste des quelque soixante appels à reprendre, avec leur fichier:ligne vérifié ;
- l'ordre de codage, en étapes qui compilent chacune ;
- le nettoyage du prefab : 6 lignes à retirer, dont une capture à 2 min qui écraserait tes 15 min ;
- le plan de vérification, la liste des essais et la mise à jour du guide.
Volume total de la refonte : environ 7 000 lignes touchées dans une vingtaine de fichiers du mod, plus le bot et le site.
### Règles
- K1 : au premier démarrage de la nouvelle version, une nouvelle campagne (n° 2) commence. Tous les carrés sont rouges sauf ceux de la zone de la base (B1 ; B2 : 1,5 km autour de SRP_SpawnBase). Menace 0, aucune zone à nous, aucun dépôt. L'ancien territoire.json est copié dans profile/SimpleRP/archives/territoire_campagne1_<date>.json, puis effacé. L'historique du site est archivé sous le nom « campagne 1 » et reste consultable.
- K2 : les paquets en attente des anciens secteurs à nous (clés sN_owner = 1 et sN_stock > 0) sont versés UNE seule fois aux dépôts de la base. Un hameau (ancien type POSTE) donne des munitions, les autres localités (ancien type VILLAGE) des vivres. En ligne, cela fait 150 vivres et 36 munitions : 5 villages et 2 hameaux au plafond de 3 h. Le versement attend la fin du regarnissage des dépôts. Si les dépôts sont pleins, le reste attend et est réessayé plus tard. Jamais deux fois : la nouvelle sauvegarde est écrite avant le versement, et l'ancien fichier est effacé après son archivage.
- K3 : une seule publication le même soir : mod, prefab, bot, site et guide. Le bot est déployé en premier et sait lire l'ancien format comme le nouveau.
- K4 : aucun interrupteur « ancien système ». Sont supprimés du code : SRP_SectorComponent, les drapeaux de secteur, la capture par cercle, la lecture de l'ancien format (sauf pour la migration) et le bloc /territoire mort.
- B3 : une victoire (toute l'île bleue) ou une remise à zéro par le Staff archive d'abord l'état final (archives/front_campagne<N>_fin_<date>.json), puis lance la campagne N+1 avec la même carte de départ que K1.
- J2 : une copie datée de front.json est faite chaque jour (archives/front_AAAA-MM-JJ.json) et 30 sont gardées. Revenir à une copie annule tout ce qui est en cours, comme un redémarrage (H3).
- H3 : rien de ce qui est en cours n'est sauvegardé : progression des carrés, contre-attaques, missions, demandes. La migration n'en reprend donc rien.
- A6 : la clé de sauvegarde et des menus est le CODE de zone (« S07 », sans espace, parce qu'une commande de menu ne garde que le premier mot). À l'écran, la zone s'affiche « Régina (S07) ». FindZone accepte le code, le nom ou les deux.
- A8 : le mot « secteur » disparaît des textes vus par les joueurs : « zone » pour le kilomètre carré, « carré » pour les 200 m.
- I5 + I7 : chaque carré pris ou perdu est annoncé à la radio (NotifyAll). Au journal, il est écrit dans la catégorie FRONT, ni publique ni envoyée à Discord (ajoutée à SRP_Discord.IsChatty). Seules les zones (catégorie TERRITOIRE) partent sur le fil Discord « territoire ». Leurs textes utilisent « capturée », « défendue », « perdue », « attaque », « vague » (couleurs, SRP_Discord.c:396-404) et jamais « garnison » ni « groupe(s) », qui rendent une ligne confidentielle (:246-248).
- I9 : la ligne « Secteurs pris » disparaît de la fiche soldat du site. Les données restent dans l'archive de la campagne 1.
- I10 : l'onglet Territoire du PC reste GetStatusText : une ligne 0 de résumé, puis une ligne par zone.
- G2 : une zone prise sans mission ne donne pas de prime, et le commandement est prévenu (événement « commandement » et NotifyOfficiers), comme avant mais au niveau de la zone.
- G9 : OnMissionSuccess n'ajoute plus +1 pour une mission de capture (LIBERER). La menace monte seulement de +1 par zone prise.
- G10 : le contrôle routier n'est permis que sur un carré bleu (SRP_Missions.CheckControl passe par IsRedAt).
- G11 : la garde d'une mission est toujours posée, même en territoire rouge (le test IsInsideEnemySector de SpawnPendingGuards est supprimé). La mission de capture elle-même garde 0 garde.
- G12 : l'hélicoptère peut être appelé pour toute alerte sur un carré rouge (IsRedAt). La marge m_fHeliSiteDistance ne vaut plus que pour les missions.
- J1 et J3 : dans le menu Staff, on peut forcer une zone ou le carré où se tient le Staff, et basculer l'interrupteur « front gelé ». Pendant le gel, TrackCapture ne compte plus l'absence, donc une capture ne peut pas échouer.
- J4 : le tableau des missions et /ordonner capture ne proposent que les zones au contact du front (GetFrontZones).
### Réglages
- m_bLegacyImport, 1 (coché) : au premier démarrage sans front.json, lit l'ancien territoire.json, l'archive dans archives/ et lance la nouvelle campagne (K1).
- m_bLegacyStockToBase, 1 (coché) : verse une seule fois aux dépôts de la base les paquets en attente de l'ancien fichier (K2).
- m_iLegacyPayTimeoutMinutes, 10 : si les dépôts n'ont pas fini leur regarnissage au bout de ce délai, le versement part quand même.
- m_iLegacyRetryMinutes, 10 : dépôts pleins : ce qui n'a pas pu être rangé est réessayé toutes les … minutes.
- m_iDailyBackupsKept, 30 : nombre de copies datées de front.json gardées (J2). Jack n'a pas donné de chiffre.
- m_bArchiveOnNewCampaign, 1 (coché) : une victoire (B3) ou une remise à zéro archive d'abord l'état final de la campagne.
- m_iSaveThrottleSeconds, 30 : front.json est écrit au plus toutes les … secondes pour les changements de carrés, et aussitôt pour une zone, un dépôt, la menace ou l'arrêt.
- m_fAutoMinDistance, 1500 m, gardé (renommable en m_fBaseZoneRadius, il n'est pas dans le prefab) : rayon de la zone de la base, toujours bleue (B2), et distance minimale d'une localité à la base.
- m_iDepotMarkerColor, 7 (bleu), ex-m_iMarkerColorOurs ; m_iDepotMarkerIcon, 31, inchangé : marqueur du dépôt de zone.
- Nouveaux NOMS imposés par le nettoyage du prefab, valeurs tenues par les autres modules : m_iSquareCaptureMinutes 15 (C4), m_iSquareLoseMinutes 1 (F3), m_iRewardHamlet 300, m_iRewardVillage 500, m_iRewardTown 800 (G8). Ils remplacent m_iCaptureMinutes, m_iLoseMinutes et m_iCaptureReward.
- Prefab SRP_GameMode.et : bloc SRP_TerritoryComponent vide. Toutes les valeurs sont celles du code, c'est-à-dire les choix de Jack. Seule exception tolérée : la valeur d'essai de 1 min pendant la séance au Workbench.
### Dépendances attendues
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

## Module mod_missions.json
### Résumé
Ce module branche les missions, l'argent, la production et les civils sur la nouvelle carte de carrés et de zones.
- La mission de capture vise une ZONE ennemie au contact du front. Elle s'engage quand un soldat arrive à 3 km de son point clé, elle échoue après 15 min sans personne et elle réussit quand la zone tombe. Une seule capture à la fois, deux à partir de 10 joueurs connectés. Prime seulement sous mission : hameau 300 €, village 500 €, ville 800 €.
- Le tableau, « Ordonner » en jeu et /ordonner capture sur Discord ne proposent que les zones ennemies au contact du front.
- Les autres missions tombent en rouge si elles sont offensives et en bleu si ce sont des missions de sécurité. Elles ont toujours leurs gardes, même dans un village gardé. Le contrôle routier ne s'établit que sur un carré bleu, donc jamais le premier soir.
- Chaque zone à nous produit selon son village (vivres ou munitions) ou selon ton fichier (carburant, pièces). Une zone coupée de la base ne produit plus. Le dépôt se pose dans la zone, mais jamais sur un carré qui touche le rouge. Si l'ennemi reprend son carré, le dépôt est détruit.
- Il y a plus de civils dans les zones libérées et très peu côté rouge, toujours 25 au plus.
- À la mise à jour, les 186 paquets en attente sont versés une seule fois aux stocks de la base. Ce qui ne rentre pas arrive en caisses au point de livraison.
### Règles
- G1 : la mission de capture vise une zone entière. Elle est réussie dès que la zone passe bleue (point clé et moitié des carrés, D1), quel que soit le chemin suivi. Son site et son marqueur sont au point clé ; pour une ville, c'est le point clé du centre (D6). Pour une zone sans localité, c'est le centre du carré de terre le plus proche du centre de ses carrés.
- G1, règle #50 gardée : la mission est ENGAGÉE quand un soldat arrive à 3 km du point clé. Elle ÉCHOUE si plus personne n'est à 3 km pendant 15 min (avertissement 5 min avant), et la menace prend +1 comme aujourd'hui. Une fois engagée, elle ne s'abandonne plus, sauf par le Staff. Je mesure depuis le point clé et non depuis la zone : c'est là qu'est le marqueur, et une zone fait 1 km, donc l'écart reste sous 0,7 km. Proposition : ne comptent pas les soldats dans la zone de la base (1,5 km, B2), les morts, ni ceux en délai de grâce.
- G2 : une zone peut tomber sans mission. La prime n'est versée que si une mission de capture de CETTE zone est active, ou si le Staff force la prise, comme aujourd'hui. Sinon, message aux officiers en jeu et évènement « commandement » vers Discord : « Zone X capturée SANS mission activée au tableau : ni prime, ni crédit de mission ».
- G3 : une seule mission de capture active à la fois, deux si au moins 10 joueurs sont connectés au moment où on la crée (PlayerManager.GetPlayerCount). Une capture en cours n'est jamais annulée si le nombre de joueurs baisse. Au-delà de la limite, une demande Discord reste en attente avec un refus expliqué.
- J4 (avec la définition E1) : la demande au tableau, « Ordonner » et /ordonner capture ne listent que les zones ennemies dont au moins un carré touche un carré bleu par un côté, hors zones déjà visées par une capture active. Le serveur revérifie à la création et donne un refus expliqué : zone plus au contact, front gelé, limite G3 atteinte.
- G8 : prime pour une zone prise sous mission, selon la plus grosse localité de la zone : hameau 300 €, village 500 €, ville ou bourg 800 €. Zone sans localité : 300 €, c'est ma proposition. L'ancienne subvention de 500 € (m_iCaptureReward) disparaît.
- G9 : la réussite d'une mission de capture n'ajoute plus +1 de menace. Seule la prise de zone compte (+1, dans le cœur). L'échec d'une capture engagée garde son +1, et les autres missions gardent leurs effets (-1 pour poste d'observation, officier et sabotage, +1 pour les autres).
- G4 : chaque zone à nous produit selon sa localité principale, comme aujourd'hui : ville, bourg ou village donnent 10 vivres par heure, un hameau 6 munitions par heure. La caisse s'arrête à 3 heures de production. Une localité située dans la base (Levie) ne produit rien, comme aujourd'hui où elle n'est pas un secteur.
- G5 : le fichier $profile:SimpleRP/zones_ressources.txt, lu au démarrage, désigne des zones par leur code, par exemple « S12 carburant » ou « S31 pieces 6 ». Par défaut, le carburant donne 8 par heure et les pièces 6 par heure. On peut aussi y écrire « vivres », « munitions » ou « rien », avec un débit facultatif. Proposition : une zone sans localité ne produit rien, sauf si elle figure dans le fichier.
- E4 : une zone coupée de la base (plus aucun chemin de carrés bleus jusqu'à Levie) ne produit plus. Son compteur d'heure se fige, son stock et son dépôt restent. Le retour au rouge après 24 h coupées est géré par le cœur.
- G6 (#49) : le kit s'installe n'importe où dans une zone à nous, sur un carré bleu qui ne touche aucun carré rouge par un côté. Le refus est expliqué avec le nombre de carrés possibles ; s'il n'y en a aucun, c'est dit clairement. Un seul dépôt par zone, installé par un Logi, un officier ou le Staff, jamais dans la zone de la base.
- G7 : si le carré du dépôt repasse rouge, le dépôt est DÉTRUIT : caisse, contenu, stock et emplacement. La cause peut être une contre-attaque carré par carré (F3), le Staff (J1) ou l'offensive fictive (H2). Il faut alors un nouveau kit à 500 €. La radio annonce « Dépôt de X perdu, détruit par l'ennemi (carré G) », avec le mot « perdu » pour la couleur Discord.
- Comme aujourd'hui (Lose, SRP_Territory.c l.1744-1747) : une zone perdue perd son stock et son dépôt, et une zone prise repart de zéro.
- G10 : « Établir un point de contrôle ici » n'apparaît que si le soldat ET le point de route sont sur un carré bleu, en plus des règles de #14 (1 500 m de la base, 1 000 m du contrôle précédent, 2 soldats, route à 40 m). Au lancement, tout est rouge sauf la base de 1,5 km (B1, B2) : aucun contrôle n'est donc possible avant la première zone prise. C'est signalé dans les questions.
- G11 : toute mission posée en territoire rouge reçoit sa garde à l'approche, même dans une localité ennemie gardée. Là, 2 groupes au plus s'ajoutent à la garnison sans être bridés par le budget local (LocalRoom) ; le plafond global de groupes (m_iMaxGroups et la réserve des missions) reste respecté. Les missions sans garde par conception (capture, convoi, colis médical, largage, contrôle) ne changent pas.
- G13 : au tirage du générateur et de « Trouver une mission », les types offensifs tombent sur un carré rouge : cache, convoi, poste d'observation, sabotage, officier, dépôt de carburant, documents, écoute. Les types de sécurité tombent sur un carré bleu : cargaison, véhicule abandonné, zone minée, colis médical, largage. L'épave tombe partout. Le convoi va d'un lieu rouge à un lieu rouge. Sans lieu bleu (début de campagne), repli sur les zones rouges au contact du front. Proposition : une mission ORDONNÉE par un officier garde un lieu libre.
- G14 : le plafond de 25 civils à pied reste. Une localité dont le carré est bleu reçoit 150 pour cent de son gabarit (promeneurs, immobiles, voitures au départ) et elle est servie en premier. Une localité rouge en reçoit 25 pour cent. La circulation routière passe à 1 voiture par groupe de joueurs en rouge au lieu de 2. Les chiffres sont une proposition.
- G15 : pas de mission de défense. La contre-attaque reste une alerte radio et Discord, il n'y a rien à coder ici.
- K2 : au premier démarrage de la nouvelle version, le stock des anciens secteurs « à nous » de territoire.json est versé une seule fois : 150 vivres à l'Intendance et 36 munitions à la Soute sur le serveur en ligne. Ce qui ne rentre pas (dépôts pleins) part en caisses de 50 au marqueur SRP_Livraison. L'ancien fichier est d'abord copié en territoire_ancien.json ; cette copie marque le versement comme fait, et l'archive reste consultable (K1).
- H3 : un redémarrage annule sans perte les missions de capture et les demandes en cours, comme aujourd'hui (les missions ne sont pas sauvegardées).
- J3, piège noté au QCM : front gelé = aucune nouvelle mission de capture, et ni engagement ni compte d'absence pour celles en cours. Proposition : la production continue.
- SRP_Delivery, lecture de G13 « les livraisons chez nous », c'est une proposition : pour une livraison d'achat, un point libre sur un carré bleu est préféré. Sinon le choix se fait comme aujourd'hui. La chance d'emprise ennemie ne change pas : 10 + 3 × menace, 40 pour cent au plus.
### Réglages
- SRP_MissionManagerComponent.m_fCaptureZoneRadius, 3000 : rayon autour du point clé qui engage une capture, et où il faut rester pour éviter l'échec (G1, #50)
- SRP_MissionManagerComponent.m_iCaptureAbsentMinutes, 15 : minutes sans personne dans ce rayon avant l'échec d'une capture engagée (G1, #50)
- SRP_MissionManagerComponent.m_bCaptureIgnoreBase, 1 : les soldats dans la zone de la base (1,5 km) ne comptent pas pour engager ni pour tenir une capture (proposition)
- SRP_MissionManagerComponent.m_iCaptureMaxLow, 1 : captures sous mission en même temps en petit comité (G3)
- SRP_MissionManagerComponent.m_iCaptureMaxHigh, 2 : captures en même temps à partir du seuil de joueurs (G3)
- SRP_MissionManagerComponent.m_iCaptureHighPlayers, 10 : joueurs connectés à partir desquels on passe à m_iCaptureMaxHigh (G3)
- SRP_MissionManagerComponent.m_iCapturePrimeHamlet, 300 : prime en euros d'une zone de hameau prise sous mission (G8)
- SRP_MissionManagerComponent.m_iCapturePrimeVillage, 500 : prime d'une zone de village (G8)
- SRP_MissionManagerComponent.m_iCapturePrimeTown, 800 : prime d'une zone de ville ou de bourg (G8)
- SRP_MissionManagerComponent.m_iCapturePrimeNoPlace, 300 : prime d'une zone sans localité (proposition)
- SRP_MissionManagerComponent.m_sOffensiveTypes, cache,convoi,poste,sabotage,officier,carburant,documents,ecoute : types tirés sur un carré rouge (G13)
- SRP_MissionManagerComponent.m_sSecurityTypes, cargaison,vehicule,mines,medical,largage : types tirés sur un carré bleu ; les types absents des deux listes tombent partout (G13)
- SRP_MissionManagerComponent.m_bSecurityFallbackFront, 1 : sans lieu bleu, une mission de sécurité tombe dans une zone rouge au contact du front ; 0 = pas de mission de ce type (G13)
- SRP_MissionManagerComponent.m_bGuardsInStronghold, 1 : une mission dans une localité ennemie gardée reçoit quand même sa garde ; 0 = ancienne règle (G11)
- SRP_MissionManagerComponent.m_iGuardStrongholdMax, 2 : groupes de garde au plus ajoutés à une garnison, sans budget local (G11)
- SRP_MissionManagerComponent.m_fGuardStrongholdMargin, 200 : marge autour de la localité ennemie pour ce cas, en mètres (reprend le 200 de l.1946)
- SRP_MissionManagerComponent.m_bControlBlueOnly, 1 : contrôle routier seulement sur un carré bleu (G10)
- SRP_MissionManagerComponent.m_fControlMinBaseDistance, 1500 (existant) : distance minimale d'un contrôle à la base (#14) ; le baisser à 1000 permettrait des contrôles dès le premier soir
- SRP_TerritoryComponent.m_iProdVivresPerHour, 10 : vivres par heure d'une zone de ville, de bourg ou de village (G4)
- SRP_TerritoryComponent.m_iProdMunitionsPerHour, 6 : munitions par heure d'une zone de hameau (G4)
- SRP_TerritoryComponent.m_iProdCarburantPerHour, 8 : débit par défaut d'une zone désignée carburant dans le fichier (G5)
- SRP_TerritoryComponent.m_iProdPiecesPerHour, 6 : débit par défaut d'une zone désignée pièces (G5)
- SRP_TerritoryComponent.m_iProductionCapHours, 3 (existant) : heures de production au plus dans la caisse d'une zone
- SRP_TerritoryComponent.m_bProductionStopsWhenCut, 1 : une zone coupée de la base ne produit plus (E4)
- SRP_TerritoryComponent.m_bDepotNotOnFront, 1 : refus d'installer le dépôt sur un carré qui touche le rouge (G6)
- SRP_TerritoryComponent.m_bDepotLostWithCell, 1 : dépôt détruit quand son carré repasse rouge (G7)
- SRP_TerritoryComponent.m_iDepotMarkerIcon, 31 (existant) : icône du dépôt sur la carte
- SRP_TerritoryComponent.m_bPourLegacyStock, 1 : versement unique des paquets des anciens secteurs aux stocks de la base (K2)
- SRP_TerritoryComponent.m_sLegacyPourMarker, SRP_Livraison : marqueur où partent en caisses les paquets qui ne rentrent pas (K2)
- SRP_TerritoryComponent.m_iLegacyBoxPackets, 50 : paquets par caisse pour ce surplus (K2)
- Fichier $profile:SimpleRP/zones_ressources.txt : une ligne par zone désignée, par exemple « S12 carburant » ou « S31 pieces 6 » ; relu à chaque démarrage (G5)
- SRP_CivilianManagerComponent.m_bFollowTerritory, 1 : les civils suivent le territoire (G14)
- SRP_CivilianManagerComponent.m_iBluePercent, 150 : gabarit d'une localité sur carré bleu, en pour cent (G14, proposition)
- SRP_CivilianManagerComponent.m_iRedPercent, 25 : gabarit d'une localité sur carré rouge, en pour cent (G14, proposition)
- SRP_CivilianManagerComponent.m_iTrafficPerGroupRed, 1 : voitures de circulation autour d'un groupe de joueurs en territoire rouge (G14, proposition)
- SRP_DeliveryComponent.m_bPreferBlue, 1 : livraisons d'achats de préférence sur un point en territoire bleu (proposition)
### Dépendances attendues
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

## Module mod_web.json
### Résumé
Ce module sort le front du jeu pour le rendre visible hors du jeu.
- Le serveur envoie au bot la carte des 1 395 carrés : en entier une fois par minute, puis seulement les carrés qui changent. Le bot accuse réception de chaque envoi.
- Le bot garde cette carte dans un fichier (front.json). Le site la montre donc même quand le serveur est éteint.
- La carte web colorie les carrés en bleu, rouge ou orange (contesté), trace la ligne de front et affiche le nom des zones. Le calque n'est redessiné que quand la carte change, et on ne dessine jamais 1 395 rectangles.
- Discord : chaque zone prise, perdue ou attaquée est annoncée dans 🚩┃territoire. Les carrés, eux, restent côté Staff. Chaque soir à 23 h, le bot poste l'image de la carte, sans chiffres.
- La frise du site garde une photo à chaque zone prise ou perdue, plus une par jour.
- L'accueil et la page Campagne montrent la part de l'île tenue et les zones tenues. La fiche soldat perd « Secteurs pris ».
- /ordonner capture ne propose que les zones au contact du front.
- L'ancien historique des secteurs est archivé et reste consultable.
### Règles
- A1 : grille de 64 × 64 carrés de 200 m. Le pont envoie n = 64 et k = 200, et le bot ne suppose rien d'autre.
- A2 : seuls les carrés de terre (au moins 10 % de terre, sans les îlots vides) comptent, codés 'R' ou 'B'. La mer et les îlots vides valent '.'. La part de l'île = carrés bleus / carrés de terre.
- A3 + A4 : les zones sont des blocs de 1 km que Jack peut retoucher. Le pont envoie la zone de chaque carré (zm) ; le bot ne recalcule jamais le découpage.
- A6 : chaque zone s'affiche « Régina (S07) » partout : carte web, page Campagne, Discord, autocomplétion.
- A7 : un carré se désigne par la référence de son coin bas-gauche en carrés de 100 m (« 074 042 », nombres pairs). Ce format sert pour les changements, les carrés contestés et les infobulles.
- C8 : un carré contesté est orange sur la carte web, en direct seulement. Il n'est jamais écrit dans front.json ni photographié pour la frise.
- I5 + I7 : la radio en jeu annonce chaque carré, mais Discord ne publie que les annonces de zone. Les lignes carré par carré (catégorie FRONT) ne vont pas dans le fil public 🚩┃territoire.
- I6 : la carte web publique montre les carrés colorés, la ligne de front et le nom des zones, même serveur éteint (front.json gardé par le bot). Le calque n'est redessiné que si la révision change.
- I7 : les annonces de zone (prise, perdue, attaque, plus défendue, coupée, victoire) partent dans 🚩┃territoire par le fil du mod, avec une copie Staff dans 🚨┃alertes. Chaque soir à 23 h (heure de Paris), une image du front part dans 🚩┃territoire : juste la carte, sans chiffre ni bilan.
- I8 : une photo du front à chaque zone prise ou perdue, prise 15 s après pour avoir tous ses carrés, plus une photo par jour à 23 h. Le curseur de la frise passe d'une photo à l'autre. Les photos du jour sont gardées 30 jours.
- I9 : le chiffre et la liste « Secteurs pris » disparaissent de la fiche soldat. Le texte d'accueil ne parle plus de secteurs pris.
- I3 : le site public ne montre rien des forces ennemies (ni garnison ni poste), seulement les couleurs, le front, les noms et les zones au contact.
- J2 : un retour à une sauvegarde datée donne une seule photo « Front ramené à un état antérieur » et un seul mouvement. Il ne produit jamais une prise par zone.
- J3 : le front gelé s'affiche sur la carte web, sur la page Campagne et dans le tableau de bord Discord.
- J4 + E1 : /ordonner capture ne propose que les zones rouges au contact du front, telles que le mod les signale (f = 1).
- K1 : l'ancien historique des secteurs est archivé une seule fois, au premier envoi du front. Il reste consultable sur la page Campagne (bloc « Campagnes précédentes »).
- B3 : à la victoire, le bot poste l'image de l'île toute bleue et prend une photo « victoire ». À la nouvelle campagne, la frise est archivée et une nouvelle commence.
- E4 : une zone coupée de la base s'affiche en ambre sur le site, avec le temps qui reste avant sa perte.
- G6 + G7 : le dépôt et son stock s'affichent par zone sur la carte web, comme aujourd'hui par secteur.
- G15 : une attaque annoncée reste une alerte, sans mission ni mention de rôle.
- Accueil et Campagne : part de l'île tenue (en % des carrés de terre) et nombre de zones tenues, lues dans front.json, donc visibles serveur éteint.
- Limite de 10 : le bot traite tous les événements d'un envoi, par messages de 10 encadrés (limite Discord). Le mod ne perd plus une alerte poussée pendant un envoi.
### Réglages
- SRP_BridgeComponent.m_iFrontFullEvery = 12 : instantané complet de la grille tous les 12 envois (1 min), en plus des changements.
- SRP_BridgeComponent.m_iFrontMaxDelta = 300 : au-delà de 300 changements en attente, on renvoie la grille entière.
- SRP_BridgeComponent.m_iFrontLogMax = 2000 : changements gardés en mémoire en attendant l'accusé du bot.
- SRP_BridgeComponent.m_iMaxEvents = 40 : alertes en attente d'envoi au bot (c'était 20, écrit en dur).
- SRP_DiscordComponent.m_bFrontSquaresPublic = non : envoyer aussi les lignes carré par carré dans 🚩┃territoire.
- SRP_DiscordComponent.m_bFrontSquaresStaff = oui : envoyer les lignes carré par carré dans journal-serveur.
- SRP_DiscordComponent.m_aConfidential = vide (liste du code) : la liste par défaut gagne « aucune zone » et « tirage ».
- config.json front_channel = « 🚩┃territoire » : salon de l'image du soir et de l'image de victoire.
- config.json front_image_hour = 23 et front_image_minute = 0 : heure de Paris de l'image du soir et de la photo du jour.
- config.json front_image_if_unchanged = false : poster l'image même si le front n'a pas bougé depuis la précédente.
- config.json front_photo_days = 30 : durée de conservation des photos du jour dans la frise.
- config.json front_zone_photo_days = 0 : durée de conservation des photos de zone prise ou perdue (0 = toute la campagne).
- config.json front_photo_if_unchanged = false : prendre la photo du jour même si elle est identique à la précédente.
- config.json front_photo_delay_seconds = 15 : délai entre la bascule d'une zone et sa photo, le temps que ses carrés arrivent.
- config.json front_movements_max = 50 : mouvements du front gardés (la page Campagne montre les 8 derniers).
- config.json front_delta_max = 2000 : garde-fou. Au-delà, le bot refuse les changements et redemande la grille entière.
- config.json alerts_per_message = 10 : encadrés par message Discord (limite de Discord).
- config.json events_per_sync_max = 60 : garde-fou du nombre d'événements traités par envoi.
- carte/index.html FRONT_PX = 8 : pixels par carré sur le calque.
- carte/index.html couleurs : --front-bleu rgba(77,163,255,.32), --front-rouge rgba(255,77,77,.26), --front-conteste rgba(255,122,26,.55).
- carte/index.html ZOOM_NOMS_ZONES = 2.75 (tous les noms de zones) et ZOOM_LIEUX = 4 (noms des localités).
- carte/index.html : état relu toutes les 5 s, frise toutes les 60 s.
- trace_image.py FRONT_ALPHA_BLEU = 85 et FRONT_ALPHA_ROUGE = 70 : transparence des carrés sur l'image du soir.
- trace_image.py : épaisseur du front = max(3, largeur / 350) pixels.
### Dépendances attendues
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

## Module mod_zones.json
### Résumé
Ce module décide quand une zone d'1 km change de camp.
Zone avec village : il faut saisir le poste de commandement ennemi, c'est-à-dire une table avec radio et cartes posée au point clé que tu places toi-même. La saisie est une action de 60 s. Il faut aussi qu'au moins la moitié des carrés de la zone soient bleus. Toute la zone passe alors bleue d'un coup.
Ville ou bourg : deux postes à saisir, le centre et le QG ennemi.
Conditions de la saisie : le carré du point clé touche du bleu, aucun ennemi en état de combattre n'est à moins de 100 m, et la garnison a fini d'arriver. La saisie s'arrête si le soldat est touché ou si un ennemi revient à moins de 100 m.
Zone sans village : elle tombe dès que la moitié de ses carrés est bleue.
Retour à l'ennemi : une zone repasse rouge quand plus de la moitié de ses carrés sont rouges et que ses points clés sont repris. Un joueur vivant à moins de 100 m d'un point clé le protège. Les carrés ne sont pas repeints.
Le module gère aussi le reste de la chute : prime sous mission seulement (300, 500 ou 800 €), menace (+1 par zone prise, -1 par zone perdue, sans le doublon actuel), réussite de la mission de capture, repli de la garnison, et nos couleurs hissées à la place du poste saisi.
### Règles
- D1 : une zone AVEC localité tombe quand tous ses points clés sont saisis ET qu'au moins la moitié de ses carrés de terre sont bleus (test entier : bleus x 100 >= total x 50) ; alors tous ses carrés passent bleus d'un coup (repeinte groupée, une seule annonce).
- D1 (proposition) : une zone SANS localité (54 sur 82) n'a pas de point clé : elle tombe dès que la moitié de ses carrés est bleue.
- D1 (proposition) : zone à deux localités (Montignac + Gravette, Tyrone + Meaux) : les points clés des DEUX localités sont à saisir.
- D3 : chaque localité a un point clé posé à la main par Jack avec l'outil Staff « Point clé ICI » (position exacte, hauteur comprise, et orientation). Tant qu'il n'est pas posé, le point par défaut est le nom sur la carte, et pour une ville le QG est placé automatiquement à 150 m, du côté opposé à la base. La zone d'une localité est celle du carré qui contient son point clé centre.
- D6 : ville et bourg ont DEUX points clés, le centre et le QG ennemi. Chacun a son poste de commandement, les deux sont à saisir, dans n'importe quel ordre. Le groupe QG ennemi (PlatoonHQ) est posé au point QG, la plus grande section au centre.
- D4 : prendre un point clé = action « Saisir le poste de commandement » de 60 s sur la table (radio, cartes), par UN soldat (C5 : même durée à 1 ou 10).
- D2 : la saisie n'est possible que si le carré du point clé touche un carré bleu par un côté (C1, 4 voisins).
- D5 + C6 : la saisie n'est possible que si aucun ennemi AU SOL en état de combattre n'est à moins de 100 m du point clé, quel qu'il soit (garnison, poste du front, patrouille, garde de mission, équipage débarqué). Il faut aussi que la garnison de la localité ait fini d'arriver (file d'apparition de la 1.8).
- D4 : la saisie s'interrompt et est à refaire depuis zéro si le soldat est touché (balle, éclat, explosion, incendiaire, corps à corps), tombe inconscient, meurt, se déconnecte, s'éloigne de plus de 4 m de la table, ou si un ennemi revient à moins de 100 m.
- C3 : un membre du Staff ne peut pas saisir dans les 5 min qui suivent une téléportation ou le Game Master.
- Déduit de D4 : tant que sa zone est rouge, le carré d'un point clé ne se prend PAS par présence (C4), seulement par la saisie. Dans une zone BLEUE, il se reprend comme un carré ordinaire (15 min), puisqu'il n'y a plus rien à saisir.
- Saisie réussie : le carré du point clé passe bleu et le poste disparaît, remplacé par nos couleurs (mât tricolore). Quand tous les points clés d'une localité sont saisis, sa garnison se replie (survivants retirés hors de vue), la sirène se tait, les camions sont rappelés et aucune garnison n'y est plus reposée.
- Le poste de commandement n'existe que si la garnison de la localité est posée (même vide par la mémoire des pertes F12) : sans garnison (plafond de 3), pas de saisie possible.
- F3 (retour inverse, proposition) : une zone bleue repasse rouge quand PLUS de la moitié de ses carrés sont rouges ET que tous ses points clés ont été repris (leur carré est rouge). Ses carrés ne sont PAS repeints : une zone ne retombe jamais d'un bloc. Zone sans localité : plus de la moitié de carrés rouges suffit.
- F5 : l'ennemi ne peut pas reprendre le carré d'un point clé tant qu'un joueur vivant et conscient est à moins de 100 m de ce point clé.
- E5 : le changement de camp d'une zone par contre-attaque ne repeint aucun carré ; seuls le Staff (J1), la zone coupée (E4) et l'offensive fictive (H2) repeignent toute la zone en rouge (sinon la zone se reprendrait aussitôt).
- B2/B4 : la zone de la base n'a pas de point clé et les zones voisines de la base ne repassent jamais rouges.
- J3 : front gelé : pas de saisie, aucune zone ne change de camp.
- H3 : un redémarrage annule une saisie en cours. Un point clé déjà saisi le reste, puisque c'est un carré bleu sauvegardé. Postes et drapeaux sont reposés au démarrage.
- G1 : la mission de capture d'une zone est réussie quand la ZONE tombe (plus quand un secteur est pris) ; le site de la mission (engagement à 3 km) est le point clé centre de la plus grosse localité, sinon le centre de la zone.
- G2 : une zone prise sans mission compte, sans prime ; les officiers sont prévenus en jeu et au salon commandement.
- G8 : prime sous mission : hameau 300 €, village 500 €, ville ou bourg 800 €. Propositions : zone sans localité 200 €, zone à deux localités = somme des deux.
- G9 : menace +1 par zone prise (avec ou sans mission, avec ou sans localité) et -1 par zone perdue, une seule fois. La réussite de la mission de capture ne compte plus : OnMissionSuccess ignore LIBERER. Rien ne bouge à la saisie d'un point clé.
- I5 : la repeinte d'une zone prise ne produit pas une annonce par carré : un seul message « Zone X capturée — menace N/10 » (radio, journal, pont).
- J1 (proposition) : une zone forcée par le Staff est repeinte en entier (bleue ou rouge), sans prime ni menace : c'est une correction.
### Réglages
- m_iZoneFallPercent = 50 : part minimale (en pour cent) de carrés de terre bleus pour qu'une zone AVEC localité tombe, une fois ses points clés saisis (D1 « au moins la moitié »)
- m_iNoLocalityFallPercent = 50 : même seuil pour une zone SANS localité (proposition)
- m_iZoneLossPercent = 50 : une zone bleue repasse rouge quand PLUS de cette part de ses carrés est rouge (et ses points clés repris)
- m_bLossNeedsAllKeyPoints = 1 : l'ennemi doit reprendre TOUS les points clés de la zone (0 : un seul suffit)
- m_bAllLocalitiesRequired = 1 : zone à deux localités, il faut saisir les points clés des deux (0 : la plus grosse seulement)
- m_fKeyPointClearRadius = 100 : D5, aucun ennemi au sol en état de combattre à moins de … m du point clé pour saisir ; même rayon pour interrompre une saisie
- m_fKeyPointHoldRadius = 100 : F5, un joueur vivant et conscient à moins de … m protège le carré du point clé
- Duration de SRP_SaisiePCAction dans SRP_PosteCommandement.et = 60 : durée de la saisie en secondes (D4)
- m_fSeizeMinRatio = 0.9 : garde-fou serveur, la saisie n'est validée que si au moins 90 pour cent de la durée s'est écoulée côté serveur
- m_fSeizeMaxDistance = 4 : la saisie s'interrompt si le soldat s'éloigne de plus de … m de la table
- m_iSeizeCheckMs = 1000 : contrôle pendant une saisie (ennemi revenu, soldat hors de combat)
- m_iHitLockSeconds = 3 : après un tir reçu, la saisie est refusée pendant … s
- m_fAirborneHeight = 7 : un ennemi en véhicule à plus de … m au-dessus du sol (hélicoptère) ne bloque pas (C6)
- m_fCPSpawnPlayerMin = 50 : le poste n'est posé que si aucun joueur n'est à moins de … m et que personne ne le voit
- m_fCPDeleteMinPlayerDistance = 300 : un poste qui n'a plus lieu d'être n'est retiré que hors de vue et sans joueur à moins de … m
- m_iCPRetrySeconds = 30 : délai entre deux essais de pose du poste
- m_fAutoHQDistance = 150 : QG par défaut d'une ville tant que Jack ne l'a pas posé, à … m du centre, du côté opposé à la base
- m_sCPPrefab = {GUID}Prefabs/Props/SRP_PosteCommandement.et : le poste de commandement (table, radio, cartes)
- m_sHQDecorPrefab = {1D2887BB9A7D4670}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et : décor du QG des villes (vide = aucun)
- m_bRaiseColours = 1 : un mât tricolore remplace le poste saisi et reste tant que le carré du point clé est bleu
- m_sColoursPrefab = {GUID}Prefabs/Props/SRP_MatCouleurs.et : le mât de nos couleurs
- m_iRewardHamlet = 300 : prime d'une zone à hameau prise sous mission (G8)
- m_iRewardVillage = 500 : prime d'une zone à village (G8)
- m_iRewardTown = 800 : prime d'une zone à ville ou bourg (G8)
- m_iRewardNoLocality = 200 : prime d'une zone sans localité prise sous mission (proposition)
- m_bRewardSum = 1 : zone à deux localités, prime = somme des deux (0 : la plus grosse)
- m_iThreatPerZoneTaken = 1 : menace ajoutée par zone prise (G9)
- m_iThreatPerZoneLost = 1 : menace retirée par zone perdue (G9)
- m_iThreatMinCells = 1 : une zone de moins de … carrés ne fait pas bouger la menace (1 = toutes, règle de Jack telle quelle)
- m_bStaffForceCountsThreat = 0 : une zone forcée par le Staff fait bouger la menace
- m_bStaffForcePaysReward = 0 : une zone forcée par le Staff verse la prime
### Dépendances attendues
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


# 2. COMMANDEUR : décisions de cohérence (font foi, priment sur les sous-modules)

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

## Réponses mal couvertes (retenu)
- CO2 (« camions de vague, jeeps et hélico décidés par lui ») :
- l'hélico et les nouveaux moyens passent bien sous le Commandeur ;
- les jeeps (MaybeSpawnVehiclePatrol) et les camions de vague restent au tirage d'aujourd'hui, bornés seulement par la capacité.
Avec l'arbitrage C1 (infanterie gratuite), c'est acceptable. À dire à Jack en une ligne.
- CO4 : la contre-attaque était rangée dans le cycle lent de 15 à 20 min, alors que CA1 demande un choix à la minute. Corrigé (trou 19).
- CO5 : trois lectures de la vraie position des joueurs chez les manœuvres. Corrigé (trou 17).
- CO6 : trois durées différentes pour un contact « vu » : 90 s (appuis), 120 s (cerveau) et 5 min (justification).
Tranché : 5 min pour justifier un appui lourd, 120 s pour viser.
- CO7 :
- effets appliqués en double pour l'officier lent (trou 12) ;
- désaccord sur la part gardée par une source quand l'officier est prudent : 60 ou 67 centièmes. Retenu : les deux tiers ;
- « audacieux : mortier dès le premier contact confirmé » est écrit dans une question du cerveau, mais codé nulle part : il faut retirer cette phrase. Effets réels de l'audacieux : renforts ×1,25, annonce avancée au seuil de 60, 4e vague plus facile ;
- « méthodique » est un 4e caractère inventé, sans effet : gardé comme caractère neutre.
- CO9 :
- l'alerte de sabotage est comptée deux fois (trou 14) ;
- l'alerte « zone perdue » (+15) et l'annonce de libération doivent ignorer une zone forcée par le Staff (arbitrage Q9 : une correction).
- OF2 : en ville, la cachette doit se lire au point clé QG du socle (prefab, arbitrage Q8), pas dans m_vKeyPoint2.
- OF6 : le ralentissement ×2 des renforts est appliqué deux fois, ce qui fait ×4 (trou 12).
- OF9 : la capture (menottes ACE sur un officier IA inconscient) n'a jamais été essayée, et aucun repli n'est codé. Question 4.
- VU2 :
- deux sens pour « renforcée, normale, affaiblie » (trou 14) ;
- la note du QCM parlait d'un drapeau par ZONE. Retenu : par RÉGION, car saisir le poste de commandement d'une zone la fait tomber : un drapeau sur cette zone ne servirait plus à rien.
- VU4 :
- l'onglet Journal du PC est ouvert à tous (vérifié : SRP_PC.c:317 et 547-553, 80 lignes, toutes catégories), d'où le filtre COMMANDEUR ;
- retenu : le salon « etat-major », avec repli sur journal-serveur.
- VU5 :
- quatre entrées pour forcer une décision (trou 15) ;
- une chute d'officier forcée par le Staff est annoncée comme une vraie (« animer une soirée ») ;
- un remplaçant forcé reste silencieux (VU2).
- VU6 : Commandeur gelé, les règles du front s'appliquent seules. Le camion d'alerte #69 revient donc, et le frein C2 ne joue plus pendant le gel. À dire à Jack.
- MA1 et arbitrage C2 : frein codé trois fois, et deux camions pour un même contact (trou 5).
- MA2 (« la source s'affaiblit d'autant ») : les soldats envoyés doivent être comptés à part des pertes au combat (trou 6).
- MA9 : deux barèmes de gravité (trou 10). MA10 : la cible du harcèlement doit être un contact connu (trou 17).
- CA4 et MO8 : codés deux fois, avec deux définitions du faux axe (trou 9). CA7 : codé deux fois (trou 10).
- AP3 :
- « dès 10 joueurs » : 10 connectés (ressources) ou 10 à 3 km (appuis) ;
- « 1 par jour » : jour du calendrier ou 24 h glissantes.
Tranché : 10 joueurs valides à 3 km, comme les autres seuils EQ4, et 24 h glissantes.
- AP7 : l'hélico a deux déclencheurs, avec 600 ou 800 m (trou 11).
- RE9 :
- trois chemins pour révéler un dépôt (trou 14) ;
- une mission ne révèle le dépôt que si son site est sur un carré rouge, comme pour VU2.
- RE13 : les moyens en route sont rendus deux fois (trou 4), et les renforts en route aussi (trou 6).
- MO7 et C1 : aucun sous-module ne tranche de la même façon si les fumigènes coûtent des obus et comptent dans les 2 tirs par heure. Question 2.
- Arbitrage Q6 (chiffres réglables dans le fichier du profil) : les réglages du front ennemi (F6, vagues, postes, H2) restent des attributs de prefab dans mod_ennemi. Proposé : les lire avec le même lecteur SRP_CmdSettings.
- LI2 : conforme. Officiers, stocks et décisions arrivent ensemble, puis tous les moyens. Seule la capacité (120 soldats et 160 IA) passe avant, avec le socle, parce que l'arbitrage Q7 vaut aussi pour le front seul.

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

## Sous-module sous_appuis.json
### Résumé
Le sous-module Appuis donne au Commandeur ses gros moyens. Le cerveau décide quand frapper. Les Appuis choisissent où et comment, et refusent tout ce que tes règles interdisent.
- Mortier : une vraie pièce par région, avec 2 servants, posée à environ 1 km côté rouge. On la repère au bruit du départ puis au sifflement. Le tir se resserre de 40 m à chaque coup tant qu'un soldat ennemi te voit. Pièce détruite, sabotée ou sans servant : 24 h sans mortier dans la région.
- Artillerie lourde : une batterie 2S1 verrouillée, à 2-3 km, à faire sauter. 1 ou 2 coups de réglage, puis une salve de 25 à 30 obus.
- Aviation : passages de Su-57 sans dégât, et au plus 1 bombe FAB-500 par 24 h, annoncée par le bruit de l'avion.
- Blindés : BTR-70, BRDM-2 et un Typhoon rare. On peut les prendre : ramenés à la base, ils rapportent 2 000 € (5 000 € pour le Typhoon).
- Hélico : il sort sur le stock du Commandeur, plus au tirage.
Tes plafonds sont appliqués : 2 mortiers par heure, 1 artillerie toutes les 2 h, 1 blindé à la fois, jamais deux frappes au même endroit en 20 min. Les seuils sont comptés à 3 km : mortier dès 3 joueurs, blindé dès 6, artillerie dès 8. Jamais de tir sur la base, les civils ou les soldats ennemis. Tous les chiffres sont dans le fichier du profil, et l'état est sauvé dans front.json. Restent à essayer au Workbench : le son du départ, la bombe RHS lancée par script et un blindé conduit par l'ennemi.
### Règles
- MO1 : vraie pièce = décor SRP_Piece_Mortier (enfant de SRP_Cible_Mortier, modèle 2B14, destructible + action « Saboter »), 2 servants ; obus tirés par script tant qu'au moins un servant est « en service » (vivant, conscient, à 25 m de la pièce) et que la pièce est intacte ; aucune caisse d'obus.
- MO2 : une équipe par région au plus ; posée au moment du tir à 800-1 200 m de la cible, hors de vue de tout joueur, sur un carré rouge, hors zone de base, à plus de 600 m de tout joueur ; reste tant qu'un joueur est à 2 km ; retirée après 5 min sans joueur à 2 km et non vue.
- MO3 : bruit de départ du 2B14 joué depuis la pièce chez tous les joueurs (RPC), puis l'obus tombe 15 s plus tard, lâché à 200 m pour siffler (SCR_ShellSoundComponent de l'obus).
- MO4 : 1er coup à 150 m du point visé ; à chaque coup suivant, 40 m plus près si un soldat ennemi voit un joueur à 100 m du point (le point suit ce joueur) ; jamais sous 30 m ; sans observateur, l'écart reste.
- MO5 : 1er obus 60 à 120 s après la décision, 4 à 6 obus, un toutes les 10 à 15 s.
- MO6 : cible = joueurs VUS par un soldat depuis 90 s au plus, sur un carré rouge ou sur un carré bleu du front (IsCellAtFront) ; jamais à moins de 1 500 m de la base ni sur un carré de base ; aucun point d'impact à moins de 150 m d'un soldat ennemi ou d'un civil, testé à chaque obus (obus sauté, 2 sautés de suite = fin du tir).
- MO7 : fumigènes de 82 mm en rideau (3 obus sur 100 m, à 80 m devant les joueurs vers l'axe d'arrivée) pour couvrir un assaut (déclenché quand la vague la plus proche est à 400 m) et un décrochage #40.
- MO8 : feinte sur 1 contre-attaque sur 4 (chance 25) : fumée + 2 obus explosifs sur un faux axe (axe réel tourné de 90 degrés, à 300 m) ; l'axe réel ne reçoit rien.
- MO9 : pièce détruite, sabotée, ou sans servant en service pendant 10 s = région sans mortier 24 h réelles, heure Unix sauvée dans front.json, redémarrage compris.
- AP1 : artillerie lourde = batterie 2S1 sans équipage posée à 2-3 km dans le rouge, hors de vue, verrouillée (entrée refusée à tous les joueurs), gardée par une section qui apparaît quand un joueur arrive à 1 000 m ; tir scripté de 25 à 30 obus de 82 mm dans 70 m ; batterie détruite (ou occupée par un joueur) = 24 h sans artillerie lourde dans la région, comme pour le mortier.
- AP2 : l'artillerie lourde tire sur un point d'appui (4 joueurs vus à moins de 100 m du même point depuis 10 min) et pour préparer une contre-attaque, toujours dans les plafonds.
- EQ1 : 1 ou 2 coups de réglage à environ 100 m de la cible, 30 à 60 s avant la salve ; avant la bombe, passage de Su-57 10 s avant l'apparition de la bombe (bruit de l'avion, charge vide).
- AP3 : bombe FAB-500 UMPK au plus 1 par 24 h, dès 10 joueurs à 3 km, sur au moins 3 joueurs vus ensemble par un soldat, aucun soldat ni civil à 250 m ; recette RHS vérifiée (SpawnEntityPrefab puis EOnEditorPlace(parent, null, 0, false, -1)) ; la charge part 26 s après ; passages de Su-57 sans dégât : avant chaque bombe, 1 contre-attaque sur 2, 3 par 24 h au plus.
- AP4 : blindés BTR-70 (poids 45), BRDM-2 (45), Typhoon armé de 30 mm (10) ; T-14, K-17, Kurganets et T-90 refusés par le code même s'ils sont écrits dans les réglages.
- AP5 : blindé seulement en contre-attaque, en renfort d'une localité attaquée, ou en riposte à un véhicule armé vu ; événement de 25 min au plus, puis repli ; rentré entier et non vu = rendu au stock.
- AP6 + C3 : équipage hors de combat = blindé prenable (le jeu vide lui-même sa faction) ; ramené à 300 m du repère de base avec un joueur à bord = 2 000 € (BTR-70, BRDM-2) ou 5 000 € (Typhoon) au coffre, une seule fois, puis retiré dès qu'il est vide ; non ramené : rien n'est sauvé, il disparaît au redémarrage.
- AP6, clef de faction : équipage RHS (Character_RHS_RF_MSV_VKPO_DS_Crew) ; le mod écrit « AFRF » (SRP_GameMode.et:90), RHS déclare « RHS_AFRF » (Configs/Factions/RHS_RF_MSV.conf:2) ; réglage appui.blinde.faction = RHS_AFRF, les deux clefs sont testées et notées au démarrage.
- AP7 : hélico appelé sur décision : stock de sorties du Commandeur, contact vu en rouge, personne sur place (aucun soldat ennemi à 600 m), une sortie à la fois, 30 min de repos ; le tirage et le bonus de nuit disparaissent (CO8).
- AP8 : l'hélico n'est jamais armé : aucun code de roquettes ni de tir.
- EQ3 : sur toute l'île, 2 tirs de mortier par heure glissante, 1 tir d'artillerie lourde toutes les 2 h, 1 blindé à la fois, une seule grosse opération à la fois (contre-attaque, blindé, artillerie lourde, bombe ; les appuis attachés à la contre-attaque en cours comptent avec elle, un seul appui lourd par contre-attaque), jamais deux frappes (explosif, artillerie, bombe) à moins de 400 m en 20 min.
- EQ4 : joueurs valides à 3 km de la cible (le seul emploi du nombre réel, CO5) : mortier dès 3, blindé dès 6, artillerie lourde dès 8, bombe dès 10.
- CA4 : contre-attaque = obus explosifs de préparation + fumée, plus blindé OU artillerie lourde si stock, seuils et plafonds le permettent (tirage à parts égales si les deux) ; l'artillerie choisie remplace les obus explosifs du mortier (règle des 20 min).
- OF6 / OF8 : région désorganisée = aucun appui (le Commandeur n'a plus de rapports) ; sa contre-attaque part sans mortier ni blindé.
- OF4 : les appuis lourds passent tous par ces fonctions ; l'officier demande, le Commandeur accorde.
- MA10 : le harcèlement du front est un tir de mortier ordinaire, avec les mêmes règles ; le cerveau limite à 1 par heure.
- C1 / RE2 : stocks = obus par région ; blindés, sorties d'hélico, tirs d'artillerie lourde et bombe au Commandeur ; servants, équipages et gardes sont de l'infanterie gratuite mais comptée dans les 120 soldats.
- RE13 : registre « en route » (blindé sorti, hélico en vol, tir d'artillerie ou bombe décidés mais pas encore partis) rendu au stock au redémarrage ; interdictions 24 h et plafonds sauvés dans front.json.
- CO6 : mortier et artillerie exigent un contact confirmé (joueurs vus), jamais du simple bruit.
- CO8 : rien ne change la nuit.
- EQ2 : aucune annonce avant un appui ; seules annonces radio : après coup (pièce réduite au silence, batterie détruite, blindé livré).
- VU4 : chaque décision et chaque refus (une fois par 10 min et par raison) vont au journal du Commandeur avec raison, coût et stock restant.
- VU5 : le Staff force chaque moyen à l'endroit voulu, sans stock, plafond, seuil ni test de carré ; la sécurité (base, soldats, civils, mer) reste.
- VU6 : Commandeur gelé = aucun nouvel appui ; tirs en attente annulés, stock rendu ; un blindé déjà engagé continue sur place.
- Sécurité (consigne) : jamais sur la base (1 500 m + carrés de base), jamais sur des civils, jamais de tir ami : 150 m pour l'explosif, 220 m à la décision d'une salve puis 150 m par obus, 250 m pour la bombe, 40 m pour la fumée.
- Textes : jamais « IA », « automatique », « garnison », « groupe », « patrouille » pour parler du Commandeur, ni de signe pour cent (radio, Discord, journal Staff).
### Réglages
- appui.actif = 1 (interrupteur des appuis ; le Commandeur a le sien, VU6)
- appui.surveillance_s = 2 (passage de surveillance)
- appui.distance_base_m = 1500 (MO6)
- appui.securite_explosif_m = 150 (MO6)
- appui.securite_fumee_m = 40 (proposé, question 2)
- appui.meme_endroit_m = 400 et appui.meme_endroit_min = 20 (EQ3)
- appui.rayon_joueurs_m = 3000 (EQ4)
- appui.contact_vu_s = 90 (âge maximal d'un contact « vu »)
- appui.informateurs = 0 (question 3)
- appui.refus_journal_min = 10 (VU4)
- appui.obus_hauteur_m = 200 ; appui.obus_coef_vitesse = 1.0 ; appui.obus_relance_clients = 0 (recette SCR_EffectModule ; repli sur le modèle Airstrike)
- appui.obus_explosif = {98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et
- appui.obus_fumee = {A544A2C131DE2C64}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_Smoke_D832DU.et
- appui.mortier.seuil_joueurs = 3 (EQ4)
- appui.mortier.tirs_heure = 2 (EQ3) ; appui.mortier.fumee_compte = 1 (question 2)
- appui.mortier.delai_min_s = 60 ; delai_max_s = 120 (MO5)
- appui.mortier.obus_min = 4 ; obus_max = 6 (MO5)
- appui.mortier.cadence_min_s = 10 ; cadence_max_s = 15 (MO5)
- appui.mortier.erreur_depart_m = 150 ; erreur_pas_m = 40 ; erreur_min_m = 30 (MO4)
- appui.mortier.observateur_m = 100 (MO4 : rayon du « vous voit »)
- appui.mortier.distance_min_m = 800 ; distance_max_m = 1200 (MO2)
- appui.mortier.portee_max_m = 3500 ; appui.mortier.joueur_min_m = 600 (pose et réutilisation de la pièce)
- appui.mortier.presence_m = 2000 ; appui.mortier.retrait_min = 5 (MO2)
- appui.mortier.servants = 2 (MO1) ; servant_rayon_m = 25 ; silence_confirm_s = 10
- appui.mortier.silence_heures = 24 (MO9)
- appui.mortier.temps_vol_s = 15 (MO3 : délai entre le départ et l'arrivée)
- appui.mortier.piece = {6A5B0C0D0E0FC001}Prefabs/Missions/SRP_Piece_Mortier.et
- appui.mortier.servants_groupe = {56FD583BBC989204}Prefabs/Groups/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Group_RHS_RF_MSV_VKPO_DS_SentryTeam.et
- appui.mortier.son = {3A984BD46A47EEC8}Sounds/Weapons/Mortars/2B14/Weapons_Mortars_2B14_Shot.acp ; appui.mortier.son_evenement = SOUND_SHOT (nom à vérifier au Workbench)
- appui.fumee.obus = 3 ; devant_joueurs_m = 80 ; largeur_m = 100 ; dispersion_m = 20 ; declenchement_m = 400 (MO7)
- appui.feinte.chance = 25 (MO8, 1 grosse attaque sur 4) ; decalage_m = 300 ; obus_explosifs = 2 ; obus_fumee = 3
- appui.artillerie.seuil_joueurs = 8 (EQ4) ; appui.artillerie.intervalle_h = 2 (EQ3)
- appui.artillerie.reglage_min = 1 ; reglage_max = 2 ; reglage_ecart_m = 100 ; avance_min_s = 30 ; avance_max_s = 60 (EQ1)
- appui.artillerie.premier_coup_min_s = 20 ; premier_coup_max_s = 40 (entre la décision et le réglage)
- appui.artillerie.salve_min = 25 ; salve_max = 30 ; rayon_m = 70 (AP1, gros barrage) ; cadence_min_s = 2 ; cadence_max_s = 3
- appui.artillerie.appui_joueurs = 4 ; appui_minutes = 10 ; appui_rayon_m = 100 (AP2)
- appui.artillerie.batterie_min_m = 2000 ; batterie_max_m = 3000 (AP1) ; garde_m = 1000 ; presence_m = 3000 ; retrait_min = 10
- appui.artillerie.silence_heures = 24 (AP1 : « comme la pièce de mortier »)
- appui.artillerie.batterie = {D8136D90BE12445F}Prefabs/Vehicles/Tracked/2S1/Tank_2S1.et
- appui.artillerie.son = (vide) ; son_evenement = (vide) (question 4)
- appui.bombe.seuil_joueurs = 10 ; appui.bombe.intervalle_h = 24 (AP3, question 1)
- appui.bombe.groupe_joueurs = 3 ; appui.bombe.securite_m = 250 ; appui.bombe.dispersion_m = 3 ; appui.bombe.avion_avance_s = 10 (AP3, EQ1)
- appui.bombe.prefab = {B881CA7B63D4EDF5}Prefabs/Vehicles/Bombs/UMPK500/UMPK500_CAS.et
- appui.avion.prefab = {FDD01BF3CEAB37E3}Prefabs/Vehicles/Airplanes/SU57/SU57_Flyby.et ; appui.avion.passages_24h = 3 ; appui.avion.chance_contre_attaque = 50
- appui.blinde.seuil_joueurs = 6 (EQ4) ; appui.blinde.simultanes = 1 (EQ3)
- appui.blinde.1 : nom = BTR-70 ; prefab = {447CFE8D73F95C3E}Prefabs/Vehicles/Wheeled/BTR70/BTR70_AFRF.et ; poids = 45 ; prime = 2000 (AP4, C3)
- appui.blinde.2 : nom = BRDM-2 ; prefab = {9AEDF323A812F9ED}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2_AFRF.et ; poids = 45 ; prime = 2000 (AP4, C3)
- appui.blinde.3 : nom = Typhoon ; prefab = {AB5DE68E3E654FCA}Prefabs/Vehicles/Wheeled/K4386/K4386_Armed.et ; poids = 10 ; prime = 5000 (AP4 « très rare », C3)
- appui.blinde.equipage = 2 ; appui.blinde.equipage_prefab = {0848BAB1B94FCBBA}Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Character_RHS_RF_MSV_VKPO_DS_Crew.et ; appui.blinde.faction = RHS_AFRF
- appui.blinde.depart_min_m = 1500 ; depart_max_m = 2500 ; halte_m = 350 ; duree_max_min = 25 ; fin_sans_joueur_m = 800 ; fin_sans_joueur_min = 5 ; bloque_s = 180 ; retrait_m = 1200 (AP5)
- appui.blinde.base_rayon_m = 300 ; appui.blinde.vide_s = 10 (C3)
- appui.helico.personne_m = 600 ; appui.helico.repos_min = 30 (réglage validé en #11) ; appui.helico.contact_s = 120 (AP7)
### Interfaces fournies
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
### Interfaces attendues
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
### Changements du plan du front
- ENNEMI (SRP_Enemy.c, MaybeCallHeli, G12) : avant, tirage à 10 / 25 pour cent, +15 par raté, repos 30 min -> après, plus de tirage : SRP_CmdSupport.RequestHeli (stock de sorties, contact vu, personne à 600 m, une sortie à la fois, repos 30 min). La question G12 du plan (garder le réglage #11) devient sans objet ; rien de plus la nuit (CO8).
- ENNEMI (SRP_Enemy.c, ForceNextHeliRoll) : avant, le tirage suivant est gagné -> après, SRP_CmdSupport.ForceNextHeli() (stock, repos et « personne sur place » ignorés une fois).
- ENNEMI (SRP_Enemy.c, SRP_EnemyTrucks.c) : avant, aucun équipage de blindé -> après, nouvelle méthode publique SpawnVehicleCrew ; PreventLod devient public.
- ENNEMI (SRP_FrontEnemy.c, contre-attaques F1-F6) : avant, vagues d'infanterie seules -> après, SRP_CmdSupport.SupportAssault(zone, cible, groupes de la vague 1, true) au départ de la 1re vague (mortier, fumée, feinte 1 fois sur 4, blindé ou artillerie lourde, passage d'avion), et EndAssault(zone) à la fin (défendue, abandon CA5, zone perdue). Le résumé rendu va au journal de l'attaque.
- ENNEMI (SRP_FrontEnemy.c) : nouveau int GetActiveAttackZone(), lu pour « une seule grosse opération à la fois » (EQ3).
- ENNEMI (SRP_EnemyAwareness.IsFree, l.297) : avant, seuls « poste front », officier, jeep et hélico sont exclus -> après, aussi m_sHomeTask « appui » (servants du mortier, garde de la batterie).
- ENNEMI / MANŒUVRE (décrochage #40, MA6) : avant, repli sous fumigènes à main -> après, SupportWithdrawal(région, localité, joueurs) d'abord. Le repli attend le délai rendu ; avec -1, il part tout de suite, fumigènes à main.
- ENNEMI (limite des 120, Q7, RE11-RE12) : avant, aucun soldat d'appui -> après, servants (2), équipage (2) et garde de batterie sont comptés (m_bBudget), avec la priorité du combat ; HasRoomFor attendu.
- SOCLE (SRP_Front.c, front.json) : avant, extras H2 seulement -> après, clés appui_* écrites par SetExtra/GetExtra, puis MarkDirty(true). La copie datée J2 et la restauration les couvrent ; ResetCampaign fait appeler SRP_CmdSupport.ResetAll par le Commandeur.
- CARTE (SRP_FrontRadio.c) : avant, annonces du front seulement -> après, + MortarSilenced, BatteryDestroyed, ArmorDelivered (radio, journal TERRITOIRE, PushEvent « commandement »). Aucune annonce avant un appui (EQ2).
- CARTE (SRP_Admin.c, Staff) : avant, le bouton hélico lance SRP_HeliSearch.Launch directement (l.1109) -> après, page « Appuis » : état (GetReport) ; Forcer mortier, fumée, artillerie, bombe, passage d'avion, blindé ou hélico sur un joueur visé ou un carré (ForceSupport) ; Lever les interdictions (ClearBans).
- MISSIONS (SRP_Missions.c, SRP_SabotageAction.PerformAction l.3136) : avant, toujours OnObjectiveUsed -> après, SRP_CmdSupport.OnSabotage d'abord (pièce du mortier), sinon la mission.
- WEB (bot) : avant, les types du front -> après, « commandement » (déjà dans la liste tranchée) porte aussi les 3 annonces des appuis ; le salon Staff reçoit le journal des décisions (VU4, tranche cerveau).
- MIGRATION : avant, 2 prefabs neufs -> après, + SRP_Piece_Mortier.et et son .meta. Ajouts à la check-list : obus qui siffle chez un client, départ entendu à 1 km, bombe et Su-57 RHS par script, BTR-70 conduit avec tireur actif, verrou du 2S1, pièce détruite à l'Hexomax, prime de 2 000 € payée une seule fois, redémarrage avec un blindé sorti (retour au stock).

## Sous-module sous_cerveau.json
### Résumé
Le Commandeur est un état-major ennemi sans corps : il n'existe que dans le calcul du serveur, on ne peut pas le tuer (CO3). Il ne sait que ce que ses soldats lui disent. Chaque unité ennemie envoie un compte rendu toutes les 30 à 60 s : joueurs vus ou entendus (position à 50-150 m près), coups de feu entendus, pertes, carrés perdus. Il oublie un contact au bout de 20 min.
En zone rouge, un civil qui vous voit à moins de 120 m peut aussi vous signaler : 1 chance sur 4, message 2 à 5 min plus tard, position à 150-300 m près. Menotter ce civil empêche le signalement.
Il décide à deux vitesses. Renforts et mortier : toutes les 1 à 2 min. Blindé, artillerie lourde et contre-attaque : toutes les 15 à 20 min. Un appui lourd n'est jamais lancé sur du simple bruit. Rien ne change la nuit.
L'île est découpée en 6 régions de 8 à 13 zones. Chaque région a une alerte qui monte avec les combats et retombe en 3 h, et un « officier ennemi Volkov » avec son propre caractère. L'officier se cache chaque jour dans une localité en retrait avec 2 gardes. Tant qu'il vit, les garnisons de sa région ont un quart de soldats en plus.
Officier tué : sa région est désorganisée pendant 3 h (6 h s'il est capturé menotté). Il n'y a alors plus de comptes rendus, les renforts sont deux fois plus lents, il n'y a ni mortier ni artillerie, et la contre-attaque est réduite. Ensuite un remplaçant arrive.
La mission « officier » vise directement l'officier de région. Une région toute bleue perd son officier pour de bon.
Tout l'état est sauvé dans front.json. Tous les chiffres sont dans commandeur_reglages.txt, avec tes valeurs par défaut.
### Règles
- CO1 : le Commandeur se pose par-dessus les règles du front. Il ne change aucun seuil voté (F6, H4, C2, EQ3, EQ4, MO6…). Il choisit seulement la cible, le moment, la taille et le soutien, dans ces limites.
- CO3 : il n'y a ni QG ni entité. L'état-major est un calcul serveur (classe SRP_Commander). Ses seules cibles physiques sont les 6 officiers et la logistique (sous-module moyens).
- CO4 : le cycle rapide est tiré entre 60 et 120 s à chaque passage (réflexes de renfort, mortier, hélico, enquêtes). Le cycle lent est tiré entre 15 et 20 min (blindé, artillerie lourde, bombe, contre-attaque, renflouement). Les gros moyens ne peuvent donc jamais s'enchaîner plus vite que le cycle lent.
- CO5 (réponse « autre » = choix 1 + choix 3), partie soldats : le Commandeur ne connaît que les comptes rendus de ses unités. Un joueur IDENTIFIED (vu) est placé à 50-100 m près, un joueur DETECTED (entendu) à 100-150 m près. L'estimation du nombre = les joueurs distincts perçus par l'unité. Deux contacts à moins de 200 m l'un de l'autre sont fusionnés. Un contact est oublié 20 min après le dernier compte rendu. La vraie position d'un joueur n'entre jamais dans sa connaissance.
- CO5 : le nombre réel de joueurs à 3 km (CountPlayersNear) ne sert qu'au dosage des seuils EQ4, par GetDosingPlayers. Jamais pour viser, jamais pour choisir un lieu.
- CO5, partie civils informateurs (proposition chiffrée). Toutes les 30 s, un civil posé sur un carré ROUGE peut signaler un joueur valide. Conditions : le joueur est lui aussi sur un carré rouge, à 120 m au plus, et en vue (ligne de vue). La région doit avoir un officier vivant et ne pas être désorganisée. Chaque civil ne tire qu'une fois par 15 min, chaque zone ne donne qu'un signalement par 10 min, et la chance est de 25 pour cent. Le message part 2 à 5 min plus tard, avec une position à 150-300 m près. Il est perdu si le civil est mort, inconscient ou menotté entre-temps. Le signalement donne seulement une alerte et une enquête : jamais de mortier, d'artillerie, de blindé ni de bombe. Lien avec G14 : les civils sont rares côté rouge, donc les informateurs aussi. Il n'y en a jamais en zone libérée.
- CO6 : chaque unité compte les tirs de joueurs qu'elle entend (au plus 60 par compte rendu). On les range par zone, en paniers d'une minute. L'intensité = les tirs des 3 dernières minutes. Les tirs font monter l'alerte de la région, la gravité de la zone et la taille du renfort. Mortier, artillerie, blindé et bombe exigent un contact VU par un soldat depuis moins de 5 min, ou des pertes dans la zone depuis moins de 10 min.
- CO7 : chaque officier tire au hasard un caractère parmi quatre : prudent, audacieux, lent, méthodique. Jamais celui de son prédécesseur. L'humeur (prudente, normale, agressive) est recalculée par région à chaque cycle rapide, à partir des stocks lourds, des pertes, des carrés repris et du caractère. Ses effets sont bornés et ne franchissent jamais une règle votée.
- CO8 : le cerveau ne lit jamais IsNight. Les fusées éclairantes et l'hélico plus fréquent la nuit (#69, #11) restent tels quels.
- CO9 : chaque région a une alerte de 0 à 100. Elle monte avec : joueur vu +2, entendu +1, 25 tirs +1, soldat tué +2, carré perdu +4, zone perdue +15, officier tombé +30, sabotage +15, informateur +2, véhicule armé vu +3. Elle retombe de 100 à 0 en 3 h, de façon linéaire, serveur éteint compris. Elle est sauvée. Menace locale = menace de l'île + 1 par tranche de 20 points d'alerte, 5 au plus, plafonnée à 10. Elle sert à l'effectif des garnisons et à la chance des patrouilles de fond de la région.
- OF1 : 6 régions de 8 à 13 zones. Le découpage est déterministe : graines éloignées, croissance équilibrée, puis équilibrage. Numérotation de R1 (la plus proche de la base) à R6. Retouches par des lignes « region S12 -> R3 » dans front_retouches.txt. Rapport dans commandeur_regions.txt. La zone de la base n'appartient à aucune région.
- VU1 : l'officier s'appelle « officier ennemi <Nom> ». Le nom russe est tiré dans une liste du fichier de réglages : jamais un nom déjà en poste, jamais l'un des 12 derniers. La région porte le nom de sa plus grande localité (ville, puis village, puis hameau ; à égalité, ordre alphabétique) : « région de Saint-Philippe », ou « région d'… » devant une voyelle.
- OF2 : la cachette change chaque jour à 6 h, heure du serveur. Les candidates sont les 3 localités hostiles de la région les plus en retrait (distance en zones jusqu'au bleu), hors contact du front s'il en reste. Tirage au hasard, sans reprendre celle de la veille. La cachette ne bouge ni pendant que l'officier est posé, ni tant qu'une mission le vise. En ville, l'officier se tient au point QG ; ailleurs dans un bâtiment près du point clé.
- OF2 : l'officier et ses 2 gardes ne sont posés que si un joueur actif est à 1 500 m ou moins, hors de vue, s'il reste de la place dans les 120 soldats et s'il y a une marge d'IA actives. Ils comptent dans les 120 (RE11), avec une importance HIGH. Ils sont retirés hors de vue après 5 min sans joueur à 2 500 m. L'officier reste alors vivant « sur le papier ».
- OF2 et OF10 (modèle de l'officier de mission actuel) : un joueur à 250 m ou moins fait fuir l'officier à pied vers une autre localité de sa région. Ses gardes restent pour couvrir. S'il se retrouve à plus de 1 500 m de sa cachette, hors de vue et sans joueur à 800 m, il s'est échappé : sa cachette du jour devient sa destination.
- OF3 : chaque unité ennemie (localité, poste, patrouille, camion, garde, officier) fait un compte rendu toutes les 30 à 60 s, retiré au sort à chaque fois, si elle a vu, entendu ou essuyé des tirs. Pertes et carrés perdus arrivent avec le même retard de 30 à 60 s. L'hélico et les vagues de contre-attaque rendent compte directement au Commandeur.
- OF3 : les comptes rendus des unités d'une localité passent par sa radio (TryRadioCall, SRP_Territory.c:1268). Opérateur radio tué : le premier compte rendu qui suit fixe un retard de 5 min, et tout reste en attente jusqu'à l'échéance. Ce retard est partagé avec le camion d'alerte et l'hélico, comme aujourd'hui.
- OF4 et MA1 : l'officier demande seul un renfort dès le premier contact (vu ou entendu) dans une zone ROUGE. Mortier, blindé, artillerie, hélico et bombe passent uniquement par le Commandeur. Le décrochage (#40) est transmis au sous-module manœuvres.
- C2 (arbitrage) : le cerveau tient le frein. Au plus un renfort par zone toutes les 10 min, camion d'alerte #69 compris (CanReinforceZone et NoteReinforcementSent). La règle « la localité qui envoie garde au moins la moitié » est appliquée par le sous-module moyens. C1 : aucun stock d'infanterie.
- Taille du renfort voulu (MA1) : 4 soldats + 2 par joueur estimé au-delà du 1er. On passe à 10 si les tirs des 3 dernières minutes atteignent 30, si 3 soldats sont tombés en 10 min, ou si l'alerte atteint 60. Puis on multiplie : ×0,75 pour un prudent, ×1,25 pour un audacieux, et de même selon l'humeur. Le résultat est borné entre 4 et 10. Les moyens fixent l'effectif réel.
- OF5 : tant que l'officier vit et que sa région est active, l'effectif voulu des garnisons de sa région est multiplié par 1,25 (arrondi), avec un plafond de 60. Le bonus joue à la pose et au complément suivants, jamais par un retrait sous les yeux des joueurs. Il disparaît dès la chute de l'officier.
- OF6 : une région désorganisée perd tous ses comptes rendus : le Commandeur ne sait plus rien, et ses contacts vieillissent puis s'oublient. Seul reste un réflexe local de renfort, sans caractère ni humeur, deux fois plus lent. Il n'y a ni mortier, ni artillerie, ni informateur, et le bonus OF5 est perdu.
- OF7 : la désorganisation dure 3 h réelles, calculées en heure Unix et sauvées, redémarrages compris. Ensuite arrive un nouvel officier : autre nom, autre caractère, nouvelle cachette. Son arrivée n'est pas annoncée aux joueurs (VU2) ; seul le journal Staff la note.
- OF9 : capture = officier inconscient puis menotté avec ACE (ACE_Captives_IsCaptive, comme SRP_Civilians.c:1993-2000). La désorganisation est alors doublée : 6 h. Tué, ou inconscient laissé 10 min sans joueur à 300 m : 3 h. Une entité supprimée sans mort (éviction, retrait) ne compte jamais comme une chute.
- OF8 : si la zone visée par une contre-attaque appartient à une région désorganisée, IsRegionDisorganized est vrai. La contre-attaque part quand même, avec une vague de moins, sans mortier ni blindé. C'est appliqué par le sous-module contre-attaque ; la règle F2 reste entière.
- OF10 : la mission OFFICIER désigne l'officier de région à sa cachette du jour, et n'est proposée que s'il en existe un vivant. Réussie à la mort ou à la capture de l'officier survenue après son lancement. Échouée s'il s'échappe. Annulée si sa région est libérée. Le nom de l'officier reste caché jusqu'à sa chute.
- OF11 : une région dont toutes les zones sont bleues est libérée. Son officier disparaît, sans remplacement, et la libération est annoncée. Une zone reprise par l'ennemi dans une région libérée est rattachée à la région voisine qui a encore un officier : celle qui partage le plus de côtés, sinon la plus proche en zones. Ce rattachement est sauvé.
- VU3 : la chute d'un officier et la libération d'une région sont annoncées à la radio en jeu et sur le salon Discord du territoire, par SRP_FrontRadio, seul auteur des textes du front.
- VU4 : chaque décision (ordre, refus des moyens, chute, remplacement, cachette) est écrite au journal COMMANDEUR, routé vers un salon Discord réservé au Staff. Les 40 dernières lignes restent en mémoire pour le menu Staff. Format : qui, quoi, pourquoi, coût, reste.
- VU5 : outils Staff : voir (état, connaissance, journal) ; geler ; forcer la chute ou le remplacement d'un officier ; forcer un ordre (mortier, renfort, blindé…) à un endroit, sans passer par la porte du cerveau.
- VU6 : l'interrupteur du Commandeur est séparé du gel du front (J3) et sauvé dans front.json. Commandeur gelé : aucun ordre, réflexes d'officier compris. Les comptes rendus, les officiers, les alertes et les réflexes du #69 continuent.
- RE13 : tout l'état du Commandeur va dans front.json (clés cmd*) : régions, alertes, officiers (vivants ou non, heure de remplacement, cachette du jour, noms récents), rattachements, gel, et la part du sous-module moyens. Ne sont pas sauvés : la connaissance, les comptes rendus en route et les ordres en file (H3). Une restauration datée remet le front et le Commandeur au même jour.
- MA9 : gravité d'une zone = 5 si un contact connu est à 150 m ou moins d'un point clé, + 3 par carré perdu en 15 min, + 1 par soldat tombé en 15 min, + 1 par tranche de 10 tirs en 3 min, + les joueurs estimés (6 au plus). Seules les zones qui ont fait l'objet d'un compte rendu depuis 15 min comptent. Région désorganisée : gravité 0.
- AP7 : le Commandeur appelle l'hélico quand des joueurs ont été VUS dans une zone rouge depuis moins de 2 min et qu'aucune de ses unités n'est à 800 m du contact (il connaît ses propres troupes). Un contact signalé par un civil déclenche seulement une enquête : l'unité mobile la plus proche va fouiller le point, ou l'hélico si personne n'est à 1 km.
- AP5, AP2, AP3 (cycle lent) : blindé contre un véhicule armé vu depuis moins de 20 min, ou pour une localité attaquée (gravité 8 et point clé menacé). Artillerie lourde ou bombe contre au moins 4 joueurs vus, restés à 100 m de leur première position depuis 10 min. Les plafonds et les seuils de joueurs (EQ3, EQ4, 1 bombe par jour) restent au sous-module moyens.
- Textes : jamais « IA », « automatique », « garnison », « groupe » ni « patrouille » dans ce qui parle du Commandeur, même au salon Staff (on écrit « unité », « défenseurs », « localité »). Pas de signe pour cent. Au plus 9 paramètres par string.Format.
- Délais : tous en heure Unix (System.GetUnixTime). Seule exception : le retard radio existant, compté en GetTickCount dans TryRadioCall, qui n'est pas sauvé et que l'on garde tel quel.
- Enforce : pas de variable nommée map, set, array ni base ; une seule déclaration par nom de variable dans une fonction ; jamais « vector + float » (Vector(dx, 0, dz)) ; jamais « |= » ; pas de case empilés ; pas de paramètre out avec valeur par défaut. Les tableaux d'unités sont toujours en ref. Le seul override ajouté (OnControllableDestroyed) existe dans SCR_BaseGameModeComponent.c:157.
### Réglages
- Fichier $profile:SimpleRP/commandeur_reglages.txt, format « clé = valeur ». Toutes les valeurs par défaut ci-dessous sont les choix de Jack.
- cycle_rapide_min_s = 60 ; cycle_rapide_max_s = 120 : cycle rapide (renforts, mortier, hélico, enquêtes) (CO4).
- cycle_lent_min_s = 900 ; cycle_lent_max_s = 1200 : cycle lent (blindé, artillerie lourde, bombe, contre-attaque) (CO4).
- rapport_min_s = 30 ; rapport_max_s = 60 : cadence des comptes rendus de chaque unité (OF3).
- rapport_tirs_max = 60 : tirs retenus au plus par unité et par compte rendu (CO6).
- radio_retard_min = 5 : retard des nouvelles d'une localité dont l'opérateur radio est tombé (OF3 ; remplace la lecture de m_iRadioDelayMinutes).
- flou_vu_min_m = 50 ; flou_vu_max_m = 100 ; flou_entendu_min_m = 100 ; flou_entendu_max_m = 150 : précision de la position connue (CO5).
- oubli_s = 1200 : un contact est oublié 20 min après le dernier compte rendu (CO5).
- fusion_contacts_m = 200 : deux contacts plus proches que cette distance n'en font qu'un.
- contact_confirme_s = 300 ; pertes_appui_s = 600 : un appui lourd demande un joueur vu depuis moins de 5 min, ou des pertes depuis moins de 10 min (CO6).
- tirs_intenses_3min = 30 : au-delà, le combat est jugé intense (renfort plein) (CO6).
- informateurs = 1 ; info_periode_s = 30 ; info_portee_m = 120 ; info_chance = 25 ; info_civil_repos_s = 900 ; info_zone_repos_s = 600 ; info_delai_min_s = 120 ; info_delai_max_s = 300 ; info_flou_min_m = 150 ; info_flou_max_m = 300 ; info_compte_m = 50 : civils informateurs (CO5 « autre » ; proposition à valider).
- alerte_max = 100 ; alerte_retombee_min = 180 : l'alerte de région retombe de 100 à 0 en 3 h (CO9).
- alerte_vu = 2 ; alerte_entendu = 1 ; alerte_tirs_par = 25 ; alerte_perte = 2 ; alerte_carre = 4 ; alerte_zone = 15 ; alerte_officier = 30 ; alerte_sabotage = 15 ; alerte_informateur = 2 ; alerte_vehicule = 3 : ce qui fait monter l'alerte (CO9).
- alerte_menace_par = 20 ; alerte_menace_max = 5 : +1 de menace locale par tranche de 20 points d'alerte, 5 au plus (CO9).
- alerte_renfort_plein = 60 : à partir de cette alerte, les renforts partent pleins.
- regions = 6 ; region_min_zones = 8 ; region_max_zones = 13 : découpage en régions, pris en compte au redémarrage (OF1).
- officier_gardes = 2 ; officier_cachettes = 3 ; officier_changement_heure = 6 : gardes, nombre de cachettes possibles, heure du changement de cachette (OF2).
- officier_apparition_m = 1500 ; officier_retrait_m = 2500 ; officier_retrait_min = 5 ; officier_marge_ia = 10 : pose et retrait de l'officier (OF2).
- officier_fuite_m = 250 ; officier_echappe_m = 1500 : fuite et évasion de l'officier (OF2, OF10 ; mêmes chiffres que la mission actuelle).
- officier_abandon_m = 300 ; officier_inconscient_min = 10 ; officier_captif_retrait_min = 10 : officier inconscient laissé sur place, officier capturé abandonné (OF9).
- officier_prefab = (vide) : prefab de l'officier ; vide = la liste des officiers de mission.
- officier_bonus_garnison = 25 ; officier_plafond_garnison = 60 : garnisons de la région plus fournies d'un quart tant que l'officier vit, 60 au plus (OF5).
- desorg_min = 180 ; desorg_capture_facteur = 2 ; desorg_renfort_facteur = 2 : 3 h de désorganisation, 6 h en cas de capture, renforts deux fois plus lents (OF6, OF7, OF9).
- noms = Volkov, Orlov, Sokolov, Morozov, Lebedev, Kozlov, Novikov, Pavlov, Smirnov, Fedorov, Belov, Zaitsev, Karpov, Gromov, Titov, Zhukov, Tarasov, Kuznetsov, Popov, Vasiliev, Mikhailov, Frolov, Yegorov, Nikitin, Makarov, Andreev, Kovalev, Ilyin, Gusev, Baranov ; noms_memoire = 12 : noms des officiers ennemis (VU1).
- renfort_frein_zone_min = 10 : au plus un renfort par zone toutes les 10 min, camion d'alerte compris (C2).
- renfort_base = 4 ; renfort_par_joueur = 2 ; renfort_plein = 10 : taille du renfort demandé (MA1).
- trait_poids = 1,1,1,1 : poids du tirage des caractères prudent, audacieux, lent, méthodique (CO7).
- trait_lent_retard_s = 75 ; trait_prudent_taille = 75 ; trait_audacieux_taille = 125 ; trait_prudent_reserve_obus = 50 ; trait_prudent_garde_source = 60 : effets des caractères (CO7 ; le dernier est appliqué par le sous-module moyens).
- humeur_stock_bas = 33 ; humeur_stock_haut = 66 ; humeur_pertes_region_60min = 15 ; humeur_reprises_60min = 2 ; humeur_prudente_taille = 75 ; humeur_agressive_taille = 125 : humeur adaptative (CO7).
- grav_point_cle = 5 ; grav_point_cle_m = 150 ; grav_carre_perdu = 3 ; grav_perte = 1 ; grav_tirs_par = 10 ; grav_joueurs_max = 6 ; grav_fenetre_min = 15 : calcul de la gravité d'une zone (MA9).
- mortier_contact_s = 120 ; mortier_repos_zone_s = 600 ; mortier_zones_par_cycle = 3 : déclenchement du mortier par le cerveau (les plafonds EQ3 restent au sous-module moyens).
- helico_sans_troupe_m = 800 : l'hélico vient quand aucune unité ennemie n'est à moins de cette distance du contact (AP7).
- blinde_vehicule_s = 1200 ; blinde_gravite = 8 : déclenchement du blindé (AP5).
- artillerie_joueurs = 4 ; artillerie_immobile_s = 600 ; artillerie_immobile_m = 100 : artillerie lourde sur des joueurs restés groupés (AP2).
- bombe_joueurs = 4 : bombe (AP3 ; 1 par jour dès 10 joueurs, contrôlé par les moyens).
- gel_au_depart = 0 : état de l'interrupteur du Commandeur au début d'une nouvelle campagne (VU6 ; ensuite l'état sauvé dans front.json fait foi).
- journal_staff_lignes = 40 : décisions gardées pour le menu Staff (VU4).
- Dans front_retouches.txt (socle) : lignes « region S12 -> R3 » (OF1).
### Interfaces fournies
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
### Interfaces attendues
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
### Changements du plan du front
- mod_ennemi, SRP_FrontEnemyComponent : avant = composant sans cerveau → après = il porte m_Commander (Tick, OnControllableDestroyed surchargé, OnZoneCaptured, OnZoneLost et OnLocalityTaken transmis, ResetAll qui prévient le Commandeur, nouveau OnCommanderSlowCycle pour la contre-attaque).
- mod_ennemi, SRP_Territory.GarrisonSoldiers : avant = base + joueurs + 0,5 × menace de l'île, puis − pertes F12 → après = base + joueurs + 0,5 × menace LOCALE (ThreatAt), × 1,25 si l'officier de la région vit (plafond 60), puis − pertes F12.
- mod_ennemi, patrouilles de fond (SRP_Enemy.c:3950) : avant = facteur de menace de l'île → après = facteur de menace locale du joueur tiré (CO9).
- mod_ennemi, hélico G12 : avant = un tirage à chaque alerte → après = un ordre HELICO du Commandeur (AP7), exécuté par les moyens. À confirmer avec le sous-module B.
- mod_ennemi, camion d'alerte #69 : avant = libre → après = soumis au frein C2 commun (CanReinforceZone, NoteReinforcementSent) et ralenti ×2 en région désorganisée.
- mod_ennemi, SRP_EnemyAwareness.IsFree : + exclusion de la tâche « garde officier ».
- mod_ennemi, contre-attaque : avant = cible, moment et vagues fixés par le seul plan → après = mêmes règles, avec les lectures du Commandeur (OF8, CA1 à CA3) faites par le sous-module C dans OnCommanderSlowCycle.
- mod_grille, SRP_FrontComponent.Start : + BuildRegions avant Load. Save et Load : + clés cmd* (environ 3 Ko de plus dans front.json). RestoreDaily : + OnRestored. front_retouches.txt : + lignes « region S12 -> R3 », ignorées par le socle.
- mod_missions, mission OFFICIER : avant = officier isolé tiré au hasard, réussie à sa mort → après = elle vise l'officier de région à sa cachette du jour (OF10). Réussie s'il tombe ou s'il est capturé, échouée s'il s'échappe, annulée si sa région est libérée.
- mod_missions, SRP_Civilians : + GetInformantCandidates (CO5). G14 inchangé.
- mod_missions, fin d'un sabotage réussi : + alerte de la région (CO9).
- mod_carte, SRP_FrontRadio : + AnnounceOfficerDown et AnnounceRegionLiberated. Menu Staff : + page Commandeur.
- mod_web, SRP_Discord : + catégorie COMMANDEUR routée vers un salon Staff ; + types officier_tombe et region_liberee pour le salon territoire.
- mod_migration, ordre de codage : SRP_CmdSettings.c puis SRP_CmdRegions.c compilés juste après le socle, puis SRP_Commander.c avec un SRP_CmdMeans vide (Execute rend faux), pour compiler le cerveau seul avant les moyens (LI2).

## Sous-module sous_manoeuvres.json
### Résumé
Ce morceau fait bouger l'ennemi, dans les limites de tes règles du front.
- Renforts : dès le premier accrochage, un camion part de la localité rouge voisine. Il arrive en 3 à 6 min (deux fois plus lentement si l'officier de la région est tombé). Frein : au plus un renfort par zone toutes les 10 min, et la voisine garde toujours la moitié de ses soldats. Elle devient donc plus facile à prendre.
- Les déplacements lointains se font « sur le papier ». Ils deviennent de vrais camions à 1,5 km de vous, et on peut leur tendre une embuscade, même quand ils traversent votre territoire pour ravitailler une poche.
- Décrochage : une section réduite au tiers se replie au centre. Aux deux tiers de pertes, les défenseurs décrochent sous fumigènes vers la voisine, sauf le QG d'une ville. Un binôme les couvre. La localité quittée reste vide tant que vous êtes à 2 km.
- Assauts : de face si vous êtes peu, par le flanc si vous êtes retranchés, sur deux axes s'il a du monde. Entre deux contre-attaques, au plus un harcèlement par heure, sans jamais reprendre de carré.
- Contre-attaques : ta minuterie, avancée ou retardée de 20 min au plus, 45 min au moins entre deux. De 2 à 4 vagues, un soutien selon ses stocks, un abandon aux deux tiers de pertes ou au bout de 60 min.
- Une zone laissée sans défenseur est perdue : plus jamais de défense réussie sans combat.
### Règles
- Interrupteur (VU6, CO1) : tout ce sous-module ne joue que si SRP_CommanderComponent.IsActive() est vrai et que manoeuvre.actif vaut 1. Commandeur coupé : les règles votées du front s'appliquent seules, c'est-à-dire mod_ennemi.json tel quel (camion d'entrée et camion d'alerte du #69, 3 vagues, F2 au hasard, H2 sans correction). Le front gelé (J3) arrête les contre-attaques et les reprises, mais pas les renforts, les colonnes, les décrochages ni les harcèlements. CO8 : aucune règle de ce sous-module ne dépend du jour ou de la nuit.
- MA1 et CO2 : le camion d'alerte du #69 devient le camion de CONTACT. Chaque contact signalé (rapport d'officier ou radio de localité, OF3/OF4) sur un carré rouge ou dans le rayon + 150 m d'une localité hostile demande un renfort dès 1 joueur, et non plus à partir de 5. Rapport retardé de 5 min si l'opérateur radio est tué (TryRadioCall, inchangé). Un contact en territoire bleu (raid, contre-attaque) n'appelle rien.
- C2 (arbitrage) : au plus un renfort par zone toutes les 10 min, en heure Unix. Le camion d'entrée du #69 (joueur à 500 m) compte dans ce frein. La localité qui envoie garde toujours au moins la moitié de sa garnison nominale : envoi possible = Effectif − arrondi supérieur de (Nominal × 0,5). Une localité vidée (MA7) ne cède rien. Réglable sans republier.
- MA2 : la source est la localité hostile la plus proche (4 km au plus) dont la garnison n'est pas posée et qui peut céder au moins 2 soldats. Les soldats envoyés sont débités comme ENVOYÉS à la pose du camion (F12) : un camion annulé ne coûte rien. La source se regarnit d'un quart par heure. Arrivée visée entre 3 et 6 min après la demande, ×2 si la région est désorganisée (OF6), ×1,5 si l'officier est lent (CO7), les deux se cumulant. Source trop loin pour tenir ce délai : départ en colonne sur le papier (MA4), qui arrive plus tard (ligne au journal).
- MA3, CO6 et C1 : camions Ural seulement (SRP_EnemyTrucks), l'infanterie est gratuite. Chargement = LoadFor d'aujourd'hui selon les joueurs actifs à 1 000 m (2 à 6, 10 dès 5 joueurs), + 2 si le feu est nourri dans la zone depuis 2 min (CO6), borné par ce que la source peut céder et par les 14 places. Zone avec garnison posée : camion vers la localité (moitié à l'assaut, moitié en défense, JoinTown). Sinon : camion d'assaut garé à 150-250 m du contact. Sans contact depuis 20 min, il est retiré hors de vue. Survivants retirés : rendus à leur source (ses envois diminuent d'autant).
- MA4 : tout déplacement entre localités est une colonne sur le papier : repli MA6, renfort lointain MA2, ordre du Staff. Elle a un effectif, une route (SRP_Placement.BuildRoadRoute, un point tous les 300 m), une heure de départ Unix et une vitesse de 7 m/s. Sa position = le point de la route à vitesse × temps écoulé.
- Elle devient réelle dès qu'un joueur, délai de grâce compris, est à 1 500 m ou moins de la route qui lui reste.
- Pose : 1 camion par 10 soldats, 2 au plus, à sa position si ce point est hors de vue et à 700 m au moins des joueurs. Sinon on la recule par pas de 100 m le long de la route déjà faite (1 000 m au plus), jamais en avant. Sinon nouvel essai toutes les 10 s.
- Réelle, elle roule jusqu'au bout (chauffeur jamais endormi) et peut être prise en embuscade (règle du #69).
- Arrivée sur le papier, ou survivants débarqués sans garnison posée : ils s'ajoutent aux renforts reçus de la destination.
- MA5 : la règle vaut pour une garnison posée, installée et au contact (contact depuis moins de 5 min).
- Section : un groupe du plan (ni le QG de ville ni le centre) réduit au tiers se replie au centre. Il est réduit au tiers quand ses soldats en état de combattre sont ≤ 0,34 × son effectif attendu, les inconscients comptant comme pertes. Ordre : ForcedMove vers le point clé 1, puis défense du centre sur 40 m.
- Garnison : quand ses soldats en état de combattre du plan sont ≤ 0,34 × ses soldats prévus (les renforts débarqués ne comptent pas), elle décroche en entier.
- Exception : le QG d'une ville (groupe de commandement au prefab QG, point clé 2) tient jusqu'au bout.
- Un seul décrochage par pose.
- MA6 : déroulé du décrochage.
- Couverture : le groupe qui a le moins de soldats en état de combattre (1 au moins, hors QG) tire en Suppress sur la dernière position connue des joueurs pendant 75 s, puis part à son tour.
- Les autres : DeploySmokeCover (2 grenades par soldat au plus, protection de leur position), puis ForcedMove vers un point de sortie à rayon + 250 m, du côté de la voisine, puis Move vers elle.
- Mortier : obus fumigènes demandés au sous-module des moyens si la région a son mortier (MO7).
- Voisine : la localité hostile la plus proche à 3 km au plus, de préférence à plus de 90° de la direction du contact. Aucune : repli sur le centre, sans sortie.
- Un groupe sorti, hors de vue et à 300 m au moins de tout joueur, passe sur le papier dans une colonne vers la voisine (MA4). Il la renforce jusqu'à 60 soldats ; au-delà, le reste part à l'arrière et n'est plus compté.
- MA7 : la localité quittée est « vidée ». Ses pertes au combat et ses soldats partis sont notés (F12). Tant qu'un joueur est à 2 000 m de son point clé, rien n'y est posé ni complété, et l'heure de ses pertes est tenue à « maintenant » : le regarnissage d'un quart par heure ne commence qu'après votre départ. Les renforts reçus par la voisine fondent au même rythme (4 h) : ils rentrent chez eux pendant que la localité quittée se regarnit.
- MA8 : doctrine des vagues de contre-attaque et des raids, fondée sur ce que l'ennemi sait (CO5 : joueurs connus à 300 m depuis 10 min, position du contact).
- « Retranchés » : le contact est resté à 40 m près pendant 5 min, ou un joueur valide est à 100 m d'un point clé de la zone.
- DEUX AXES si la vague a au moins 3 groupes, au moins max(12, 2 × les joueurs connus) soldats, et un 2e départ à 60° au moins. Sinon FLANC si les joueurs sont retranchés et qu'il y a 2 groupes. Sinon FACE.
- FACE : SearchAndDestroy sur l'objectif. Si 3 joueurs ou plus sont connus, le 1er groupe ouvre par 45 s de Suppress.
- FLANC : le plus gros groupe fait base de feu : Move à 250-350 m avec vue sur l'objectif, puis Suppress 90 s (hauteur 1,5 m). Les autres passent par le point de contournement caché du #69 (FlankPoint), puis SearchAndDestroy, lancés 20 s après l'ouverture du feu (4 min d'attente au plus). Fumée (DeploySmokeCover) à 300 m de l'objectif.
- DEUX AXES : chaque départ garde ses groupes. Assaut lancé quand les deux axes sont à 400 m (4 min d'attente au plus), chaque axe ouvrant par 45 s de Suppress.
- Dernier carré contenant un point clé : Attack sur sa position, sans entité visée ; repli SearchAndDestroy.
- Retraits : ForcedMove, priorité 2000.
- MA9 : gravité de chaque zone, toutes les 60 s, avec ce que l'ennemi sait ou voit.
- Barème : 50 si un point clé est menacé (joueur connu à 150 m ou carré du point clé orange), 8 par carré de la zone en cours de capture, 5 par soldat ennemi perdu dans la zone en 10 min (40 au plus), 20 pour une contre-attaque en assaut, 3 par joueur connu (30 au plus).
- Mortier, blindé et artillerie (sous-module des moyens) ne vont qu'à la zone la plus grave, ou aux 2 plus graves dès 10 joueurs connectés. Une contre-attaque est toujours servie.
- Place qui manque sous les 120 : les renforts demandés passent par gravité décroissante.
- Ailleurs, seuls les réflexes des officiers répondent (renfort MA1, décrochage MA5).
- MA10 : harcèlement du front, au plus 1 fois par heure (heure Unix sauvegardée).
- Jamais pendant une contre-attaque annoncée ou en cours, ni dans les 20 min qui suivent.
- Essai toutes les 5 min, avec 25 chances sur 100, une fois l'heure passée. Il faut au moins 2 connectés et un joueur à 1 500 m d'un carré bleu du front.
- Cible : le carré bleu du front où des joueurs sont connus depuis 20 min, sinon le plus proche d'un joueur.
- Une fois sur deux, si le mortier est permis et qu'un joueur a été VU sur un carré bleu du front (MO6) : 3 obus via le sous-module des moyens.
- Sinon, raid : 1 groupe (2 dès 6 joueurs à 3 km), posé hors de vue sur un carré rouge à 400-800 m, doctrine MA8. Au bout de 10 min de contact ou à la moitié de pertes : ForcedMove vers le rouge, puis retrait hors de vue.
- Les raiders gardent m_iAssaultZone = -1 : aucun carré repris (E5).
- MA11, localité suivante : après chaque zone prise (ni forcée par le Staff, ni dans une région désorganisée), la localité hostile la plus proche du centre de la zone prise, parmi les zones voisines au contact, est « tenue au complet » pendant 6 h. Ses pertes, ses envois et son état de localité vidée sont effacés, et elle passe devant les autres pour la pose (-500 m au score F11).
- MA11, chemin des joueurs : deux repérages d'une même région, distants d'au moins 150 m en moins de 10 min, donnent une direction.
- Les localités hostiles à 2 500 m au plus, dans un cône de ±45°, rappellent leurs soldats envoyés (pas les pertes au combat, pour garder F12), si elles n'ont pas été au contact depuis 30 min.
- Les carrés de poste du front dans ce cône, à 1 500 m, passent en tête de pose, en taille 4, dans les 4 postes et la réserve de 12.
- Une fois par région toutes les 20 min. Rien dans une région désorganisée.
- MA12 : une poche est une localité hostile hors de la masse rouge principale. Elle est traitée comme les autres : ses renforts viennent de la localité hostile la plus proche qui peut céder, jusqu'à 6 km. Les camions et les colonnes roulent par la route et traversent le bleu ; seul leur point de départ doit être rouge (m_bOriginRedOnly). On peut les prendre en embuscade. Le regarnissage F12 d'un quart par heure continue.
- CA1 : la minuterie F6 reste la règle (180 − 12 × menace min, ± 20, horloge seulement à 4 connectés et front non gelé).
- De D − 20 à D + 20 min d'horloge, chaque minute, le Commandeur note le moment. Stocks : jusqu'à 50, selon la part pleine des stocks lourds de la région qui attaque. Présence : 50 si aucun joueur connu à 1 km de la zone visée depuis 10 min, 25 pour 1 ou 2, 0 au-delà.
- Annonce avant D si la note atteint 70 (60 pour un officier audacieux ; jamais avant D pour un officier lent). À partir de D si elle atteint 40. Toujours à D + 20.
- Jamais moins de 45 min (heure Unix sauvegardée) entre deux annonces, jamais sans 4 connectés (H4).
- Une grosse opération en cours (EQ3) retarde l'annonce, jusqu'à D + 20 au plus. L'attaque part alors sans appui lourd.
- CA2 : 70 fois sur 100, la dernière zone prise encore attaquable (F2, inchangé). Sinon, cible sur renseignements parmi les zones attaquables (F1) : +40 si un dépôt de secteur #49 y est posé, +30 si aucun joueur n'y est connu à 1 km depuis 20 min, +2 par carré bleu qui touche le rouge. La meilleure note l'emporte, au hasard à égalité. Hasard pur seulement si aucune note n'est positive.
- CA3, lu avec C1 : aucun stock n'entre en compte.
- 3 vagues prévues ; une de moins si la région est désorganisée (OF8).
- Au moment de la 2e vague, la 1re est jugée. Bonne (un carré repris, ou deux tiers de ses soldats encore debout dont un au moins dans la zone) : +1 vague, 4 au plus (3 si désorganisée), s'il reste sous la limite des 120 la place d'une vague + 6. Brisée (un tiers ou moins debout, aucun carré repris) : −1, 2 au moins (1 si désorganisée).
- Officier prudent : jamais d'ajout. Audacieux : ajout aussi quand la 1re vague n'est ni bonne ni brisée.
- Taille d'une vague = min(règle F, LocalRoom, place « combat » sous les 120 divisée par 6). Une vague sans place est comptée sans groupe.
- CA4 : chaque soutien exige assez de joueurs (EQ4 : mortier dès 3 à 3 km, blindé dès 6, artillerie dès 8), le respect des plafonds (EQ3) et du stock. Le sous-module des moyens décide et peut refuser.
- Mortier explosif 2 min avant l'assaut, sur les défenseurs connus.
- Fumée : DeploySmokeCover des groupes à 300 m de l'objectif, et obus fumigènes si le mortier est permis (MO7).
- Un seul gros moyen : l'artillerie lourde 1 min avant l'assaut s'il existe un amas d'au moins 4 joueurs connus sur 70 m. Sinon, un blindé qui suit la 2e vague par l'axe principal.
- Région désorganisée (OF6, OF8) : ni mortier, ni blindé, ni artillerie.
- Feinte (MO8) : 25 fois sur 100, s'il y a deux axes et que le mortier est permis. Fumée et 2 obus sur le faux axe 1 min avant ; toutes les vagues arrivent par l'autre.
- CA5 : abandon quand les deux tiers des soldats engagés sont hors de combat, c'est-à-dire quand ceux en état de combattre, papier compris, sont ≤ un tiers des soldats posés. Le test se fait une fois 2 vagues posées, ou toutes celles prévues.
- Les vagues restantes sont annulées, les soldats se replient en ForcedMove sous fumée vers leurs départs rouges, puis sont retirés hors de vue.
- La zone est DÉFENDUE, puisque l'assaut a été brisé au combat : radio Defended, menace −1 (voir question).
- Au bout de 60 min d'assaut : rupture, menace inchangée, radio AttackBroken.
- Dans les deux cas, les carrés déjà repris restent rouges.
- CA6 : les vagues avancent même sans défenseur.
- Vague sur le papier si aucun joueur n'est à 1 500 m de son origine ni de la zone. Elle avance à 1,3 m/s en ligne droite vers le carré reprenable le plus proche. Ses soldats sont déclarés à la capture comme assaillants de leur carré, seulement si aucun joueur n'est à 1 000 m. La capture reprend en 60 s (F3, F5 inchangés).
- Vague réelle loin des joueurs : tenue éveillée (PreventMaxLOD).
- Le délai « dernière vague réputée arrivée au bout de 10 min » est SUPPRIMÉ. Un groupe bloqué 3 min reçoit un nouvel ordre ; bloqué 10 min et hors de vue, il passe sur le papier.
- Une défense n'est donc réussie que si les assaillants ont été mis hors de combat. Sinon la zone tombe (règle Q2), ou l'assaut rompt à 60 min.
- CA7 : au départ du dernier joueur, la part pleine des stocks du Commandeur (0 à 1) est notée dans front.json. Offensive de nuit (H2) : stocks pleins (part 1,0) → chance + 10, 60 au plus ; moins d'un tiers → pas d'offensive cette nuit (jour marqué fait, une ligne au journal Staff, aucune annonce publique) ; entre les deux, formule votée.
- IA loin des joueurs : au-delà d'environ 1 000 m, le jeu coupe l'IA au niveau de détail maximal (commentaire vanilla SCR_AIGroup.c:118-123).
- Sur le papier : tout ce qui voyage loin (colonnes, vagues sans témoin, replis sortis de vue). Ces unités sont matérialisées hors de vue à 1 500 m d'un joueur.
- PreventMaxLOD, comme pour le chauffeur des camions (SRP_EnemyTrucks.c:1839-1861) : seulement les groupes réels déjà engagés dans une contre-attaque, au-delà de 900 m du joueur le plus proche.
- Groupe réel sans joueur à 2 000 m pendant 3 min et hors de vue : il repasse sur le papier.
- Les groupes « dormants » de la 1.8 ne sont pas utilisés.
- RE13 et H3 : au redémarrage, contre-attaque, vagues sur le papier, raids et replis en cours sont annulés sans perte. Une colonne en route rend ses soldats à sa localité de départ. Sont gardés dans front.json : envois, renforts reçus, localités vidées et tenues au complet, heures du dernier harcèlement et de la dernière annonce, compteur des grosses attaques, part des stocks au départ du dernier joueur.
- VU4 et VU5 : chaque décision écrit une ligne au journal Staff du Commandeur (menu Staff et salon Discord réservé) : renfort, colonne, décrochage, manœuvre d'une vague, harcèlement, moment et cible d'une contre-attaque, révision des vagues, abandon. Le Staff peut forcer un renfort, un décrochage, un harcèlement ou une colonne à l'endroit voulu. Les règles de pose hors de vue restent.
- CO7 : le caractère de l'officier de la région joue ici.
- Prudent : la source garde les deux tiers au lieu de la moitié, jamais de 4e vague.
- Audacieux : seuil d'annonce avancée à 60, 4e vague plus facile.
- Lent : renforts ×1,5, jamais d'annonce avant D.
- Textes : radio et Discord public seulement par SRP_FrontRadio ; les lignes Staff passent par le journal du Commandeur. Jamais « IA », « automatique », « garnison », « groupe » ni « patrouille » : on écrit « défenseurs », « troupes », « soldats », « section », « détachement », « colonne », « localité ». Aucun signe pour cent : on écrit « 25 sur 100 ».
- Enforce :
- string.Format à 6 paramètres au plus ici, jamais de % littéral ;
- aucune variable nommée map, set, array ou base (« fireBase » est permis) ;
- choix sur les énumérations en if/else, sans case empilés ;
- pas de |= ;
- pas de paramètre out avec valeur par défaut ;
- une seule déclaration par nom dans une fonction ;
- jamais vector + float : on écrit Vector(dx, 0, dz) ;
- groupes toujours tenus par ref (SRP_EnemyTrucks.c:45-47) ;
- aucun override (SRP_CmdManeuvers n'hérite de rien) ;
- aucune classe modded touchée, donc pas de préfixe SRP_ imposé sur les champs ;
- délais stockés en heure Unix (System.GetUnixTime, vérifié Core/generated/System/System.c:53).
### Réglages
- Fichier $profile:SimpleRP/commandeur_reglages.txt, section « manœuvres ». Il est lu au démarrage et par la commande Staff « relire les réglages », sans republier. Les valeurs ci-dessous sont les choix de Jack ou, à défaut, la proposition par défaut. Syntaxe : clé = valeur, # pour un commentaire.
- manoeuvre.actif = 1 : renforts, colonnes, décrochages, harcèlement et réglage des contre-attaques par le Commandeur. 0 : règles votées seules.
- Renforts (MA1-MA3, C2, CO6, CO7, OF6) :
- renfort.actif = 1
- renfort.joueurs_min = 1
- renfort.contact_secondes = 90 : âge maximal d'un contact
- renfort.frein_minutes = 10 (C2)
- renfort.source_garde = 0.5 (C2 : part de la garnison nominale toujours gardée)
- renfort.source_garde_prudent = 0.67 (CO7)
- renfort.source_distance = 4000
- renfort.source_distance_poche = 6000 (MA12)
- renfort.arrivee_min = 180 et renfort.arrivee_max = 360 (secondes, MA2)
- renfort.facteur_desorganisee = 2.0 (OF6)
- renfort.facteur_lent = 1.5 (CO7)
- renfort.bonus_feu = 2 et renfort.feu_secondes = 120 (CO6)
- renfort.retrait_minutes = 20 : camion d'assaut sans contact, retrait hors de vue
- renfort.rendre_survivants = 1
- Colonnes (MA4) :
- colonne.distance = 1500
- colonne.vitesse = 7.0 (m/s sur le papier)
- colonne.joueur_min = 700
- colonne.recul_pas = 100 et colonne.recul_max = 1000
- colonne.soldats_camion = 10 et colonne.camions_max = 2
- colonne.essai_secondes = 10
- colonne.pas_route = 300
- Décrochage (MA5-MA7) :
- repli.actif = 1
- repli.section_tiers = 0.34 et repli.garnison_tiers = 0.34
- repli.contact_minutes = 5
- repli.couverture_secondes = 75
- repli.grenades = 2
- repli.sortie = 250
- repli.voisine_max = 3000
- repli.angle_min = 90
- repli.effacement = 300
- repli.plafond = 60
- repli.vide_distance = 2000
- repli.fonte_heures = 4.0
- repli.centre_rayon = 40
- Assaut (MA8) :
- assaut.deux_axes_groupes = 3, assaut.deux_axes_soldats = 12, assaut.deux_axes_rapport = 2.0
- assaut.angle_axes = 60
- assaut.retranche_metres = 40 et assaut.retranche_minutes = 5
- assaut.connus_rayon = 300 et assaut.connus_minutes = 10
- assaut.feu_min = 250 et assaut.feu_max = 350
- assaut.suppression_secondes = 90, assaut.suppression_ouverture = 45, assaut.suppression_hauteur = 1.5
- assaut.fumee_distance = 300 et assaut.fumee_grenades = 1
- assaut.synchro_distance = 400 et assaut.synchro_max = 240
- assaut.attaque_finale = 1
- Gravité (MA9) :
- gravite.point_cle = 50 et gravite.point_cle_rayon = 150
- gravite.carre = 8
- gravite.perte = 5 et gravite.pertes_max = 40
- gravite.contre_attaque = 20
- gravite.joueur = 3 et gravite.joueurs_max = 30
- gravite.zones = 1, gravite.zones_nombreux = 2 et gravite.nombreux = 10
- Harcèlement (MA10) :
- harcelement.actif = 1
- harcelement.intervalle = 60 (minutes au moins entre deux)
- harcelement.chance = 25 (sur 100, à chaque essai de 5 min)
- harcelement.joueurs_min = 2
- harcelement.portee = 1500
- harcelement.connus_minutes = 20
- harcelement.obus = 3 et harcelement.part_obus = 50
- harcelement.groupes_max = 2 et harcelement.deux_groupes_des = 6
- harcelement.depart_min = 400 et harcelement.depart_max = 800
- harcelement.combat_minutes = 10
- harcelement.pertes_retrait = 0.5
- harcelement.apres_ca = 20
- Réorganisation (MA11) :
- reorg.complet_heures = 6
- reorg.bonus_score = 500
- reorg.chemin_distance = 2500 et reorg.chemin_angle = 45
- reorg.chemin_intervalle = 20
- reorg.chemin_calme = 30
- reorg.postes_distance = 1500
- reorg.releve_metres = 150 et reorg.releve_minutes = 10
- Contre-attaques (CA1-CA6) :
- ca.fenetre = 20
- ca.ecart_min = 45
- ca.seuil_avance = 70, ca.seuil_audacieux = 60, ca.seuil_heure = 40
- ca.presence_rayon = 1000 et ca.presence_minutes = 10
- ca.cible_depot = 40, ca.cible_vide = 30, ca.cible_carre = 2, ca.cible_minutes = 20
- ca.vagues = 3, ca.vagues_min = 2, ca.vagues_max = 4
- ca.vague_bonne = 0.67, ca.vague_brisee = 0.34, ca.marge_place = 6
- ca.mortier_avance = 120 et ca.lourd_avance = 60
- ca.amas_joueurs = 4 et ca.amas_rayon = 70
- ca.feinte = 25
- ca.abandon = 0.67, ca.abandon_vagues = 2, ca.abandon_minutes = 60
- ca.papier_distance = 1500
- ca.eveil_distance = 900
- ca.effacement_distance = 2000 et ca.effacement_minutes = 3
- ca.pied_vitesse = 1.3
- ca.bloque_minutes = 3 et ca.bloque_papier_minutes = 10
- ca.papier_joueur_min = 1000
- Offensive de nuit (CA7) :
- nuit.pleins = 1.0 (part des stocks, 1 = pleins)
- nuit.bonus = 10
- nuit.bas = 0.34 (moins d'un tiers : pas d'offensive)
- Le plafond de 60 reste m_iOffensiveChanceMax du front.
- Ce qui ne change pas : la rythmique F6 (180/60 min, aléa de 20), la reprise de 60 s par carré, le rayon de 100 m des points clés et l'annonce 15 min avant restent des réglages du front. La relecture propose de les passer aussi dans le fichier du profil.
### Interfaces fournies
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
### Interfaces attendues
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
### Changements du plan du front
- mod_ennemi, F6 AttackClock : avant, échéance atteinte → LaunchAttack. Après : fenêtre D ± 20 min, décision du Commandeur (stocks, présence), 45 min au moins entre deux annonces, départ forcé à D + 20. Horloge comptée en secondes Unix (m_iAttackClockSec).
- mod_ennemi, F2 PickTargetZone : avant, 70 sur 100 la dernière prise, sinon au hasard. Après : 70 sur 100 la dernière prise, sinon une cible sur renseignements (dépôt #49, zone sans joueur connu, carrés exposés) ; hasard seulement sans renseignement.
- mod_ennemi, vagues (SendWave) : avant, 3 vagues fixes toutes les 5 min. Après : 3 prévues (−1 si région désorganisée), révisées de ±1 après la 1re (bornes 2-4, ou 1-3), taille bornée aussi par la place « combat » sous les 120. Vague sur le papier si aucun joueur n'est à 1 500 m, doctrine d'assaut MA8 pour chaque vague.
- mod_ennemi, AttackTick c) AdvanceAssault : avant, OrderMove et OrderSearch vers le carré reprenable. Après : SRP_CmdManeuvers.DriveAssault. Même choix d'objectif, avec Suppress, SearchAndDestroy, Attack sur le point clé, fumée, IA tenue éveillée, unités sur le papier.
- mod_ennemi, AttackTick e) « dernière vague arrivée » : avant, le groupe est entré ou hors de combat, OU 10 min après la pose (m_iWaveArrivalMaxMinutes). Après : délai supprimé quand le Commandeur est actif ; un groupe bloqué reçoit un nouvel ordre, puis passe sur le papier. Il n'y a plus de défense réussie sans combat (CA6).
- mod_ennemi, fins de contre-attaque : avant, rupture seulement à 60 min. Après : + rupture aux deux tiers de pertes (CA5), comptée « défendue » (menace −1). À 60 min, « repli » avec le nouveau texte AttackBroken, menace inchangée.
- mod_ennemi, soutien : avant, aucun (infanterie seule). Après : mortier 2 min avant l'assaut, fumée à l'approche, un gros moyen (artillerie lourde si un amas de joueurs est connu, sinon blindé avec la 2e vague), feinte une fois sur 4. Rien si la région est désorganisée (CA4, OF8).
- mod_ennemi, H2 OffensiveTick : avant, chance 10 + 5 × menace, 60 au plus. Après : la même chose, + 10 si les stocks étaient pleins au départ du dernier joueur ; pas d'offensive s'ils étaient sous le tiers (CA7). La réussite passe par SRP_ZoneFall.LoseZone.
- mod_ennemi, F12 pertes : avant, un seul compteur (pertes au combat). Après : pertes au combat + soldats envoyés (MA2, MA6) + renforts reçus (MA4, MA6), chacun fondant d'un quart par heure. Effective = Nominal − pertes − envois + reçus, 60 au plus.
- mod_ennemi, F11 pose : avant, wanted = GarrisonSoldiers − pertes. Après : wanted = Effective. Localité vidée (MA7) : ni pose ni complément tant qu'un joueur est à 2 km. Localité tenue au complet (MA11) : −500 m au score pendant 6 h.
- mod_ennemi, F8 postes : avant, les meilleurs carrés du front, taille selon le genre. Après : les carrés sur le chemin des joueurs (MA11) passent en tête et prennent la taille 4, dans les mêmes plafonds (4 postes, réserve de 12).
- mod_ennemi, camions : avant, départ d'une couronne de sondes autour de la localité visée, camion d'alerte dès 5 joueurs, arrivée du camion d'entrée en 2 à 4 min. Après, le Commandeur actif :
- départ d'une localité SOURCE débitée (MA2) ;
- camion de contact dès le 1er accrochage, à la place du camion d'alerte ;
- arrivée en 3 à 6 min ;
- frein C2 de 10 min par zone, camion d'entrée compris.
- mod_ennemi, H3 redémarrage : avant, les attaques et l'horloge sont oubliées. Après : pareil, et en plus les colonnes en route rendent leurs soldats à leur source (RE13). Envois, reçus, localités vidées ou tenues au complet, dernière annonce et dernier harcèlement sont gardés dans front.json.
- mod_capture, reprise F3 : avant, seuls les soldats marqués m_iAssaultZone. Après : + les soldats sur le papier déclarés par SetPaperAssault, avec les mêmes conditions (carré qui touche du rouge, aucun joueur valide, F5).
- socle SRP_FrontLocality : avant, m_iLosses et m_iLossesAt. Après : + m_iSent, m_iSentAt, m_iBonus, m_iBonusAt, m_iEmptiedAt, m_iFullUntil, sauvegardés, + extras cmd_*.
- carte SRP_FrontRadio : avant, AttackAnnounced, AttackWave, Defended, ZoneFell. Après : + AttackBroken et AttackCalledOff. Menus Staff : + forcer un renfort, un décrochage, un harcèlement, une colonne, + rapport des manœuvres.
- SRP_EnemyAwareness : avant, l'entraide peut prendre tout groupe libre. Après : les groupes menés par le Commandeur (m_iCmdRole ≠ 0, repli) sont exclus, et FlankPoint devient public.

## Sous-module sous_ressources.json
### Résumé
Ce morceau donne à l'ennemi ses réserves et sa limite de soldats.
- Réserves (C1) : seuls les obus (par région), les blindés, les sorties d'hélico, les tirs d'artillerie lourde et la bombe sont comptés. L'infanterie et les camions restent gratuits.
- Pleins : 48 obus pour l'île (6 à 9 par région selon sa taille), 2 blindés, 3 sorties d'hélico, 1 tir lourd, 1 bombe par jour dès 10 joueurs. Ils grossissent de 5 centièmes par point de menace et se refont en 4 h au revenu plein, en temps réel, même serveur éteint.
- Chaque zone prise appauvrit l'ennemi (jamais sous la moitié). Chaque région cache un dépôt, révélé par le renseignement. Saboté, il coupe la moitié du revenu de sa région pendant 24 h, puis change de place. Une région à sec est renflouée en 1 h.
- Soldats : 120 au sol au plus, tout compris, dont 20 places gardées aux renforts et 12 aux postes du front. La limite du jeu est portée à 160 au démarrage. Le combat passe d'abord ; les patrouilles loin de vous cèdent leur place hors de vue.
- Renforts freinés : un par zone toutes les 10 min, et la localité qui les envoie garde la moitié de ses soldats.
- Au redémarrage, tout l'état est gardé, et ce qui était en route revient au stock.
- Budget : à 5 joueurs, tout tient (environ 119 sur 120). À 12 joueurs sur une ville : environ 49 en ville, 38 dans les deux localités voisines et 22 en renfort. À 20 joueurs sur 3 villages : environ 30 par village, 12 aux postes et 20 en renfort, puis de nouveaux renforts au fil des pertes.
### Règles
- C1 : seuls les obus de mortier, les blindés, les sorties d'hélico, les tirs d'artillerie lourde et la bombe sont des stocks. Soldats et camions sont gratuits. Ils ne sont limités que par les 120 soldats, par la localité voisine qui perd ceux qu'elle envoie (MA2, pertes F12 regarnies d'un quart par heure) et par le frein C2. Les chiffres « 10 sections, 4 camions » et la règle « stock vide, pas de camion » disparaissent (RE1, RE3, CO2 et CA3 relus ainsi).
- RE2 : les obus sont tenus par région. Blindés, sorties d'hélico, artillerie lourde et bombe sont tenus par le Commandeur.
- RE3 (moyen) + AP3 : pleins à menace 0. 48 obus pour l'île, partagés au prorata des zones de départ de chaque région (plus forts restes) : régions de 8, 10, 13, 12, 11 et 12 zones → 6, 7, 9, 9, 8 et 9 obus. 2 blindés, 3 sorties d'hélico, 1 tir d'artillerie lourde, 1 bombe.
- RE10 : chaque plein est multiplié par (1 + 5/100 × menace), sauf la bombe. Les stocks sont des nombres à virgule : à menace 10, l'île a 72 obus, 3 blindés, 4,5 sorties et 1,5 tir lourd. Seule une unité entière se dépense. Quand la menace baisse, le stock est ramené au nouveau plein.
- RE5 : au revenu plein, un stock vide redevient plein en 4 h (taux = plein ÷ 4 par heure). La bombe se refait en 24 h (AP3 : 1 par jour).
- RE4 : le revenu en obus d'une région vaut le taux × max(0,5 ; zones rouges de la région ÷ ses zones de départ, au plus 1). Le revenu du Commandeur vaut le taux × max(0,5 ; zones rouges de l'île ÷ 66). Une région sans zone rouge (OF11) n'a ni revenu ni obus.
- RE6 : le revenu se calcule en heure Unix (System.GetUnixTime), serveur vide ou éteint compris. Au chargement, le temps manqué est rattrapé en tranches coupées aux heures de fin des coupures de dépôt. Si l'horloge recule, aucun revenu n'est versé.
- RE8 : un dépôt saboté coupe pendant 24 h réelles la moitié du revenu en obus de sa région, ainsi que la moitié de la part de sa région dans le revenu du Commandeur (part = zones rouges de la région ÷ zones rouges de l'île). Un second sabotage pendant la coupure relance les 24 h, sans jamais descendre sous la moitié.
- RE8 « pareil » : une mission de sabotage ou de cache réussie retire tout de suite un quart du plein du stock visé par la pièce. Mortier ou cache → obus de la région ; antichar → blindés ; AA ou radar → hélico ; transmissions ou générateur → artillerie lourde (question 2).
- RE9 : chaque région vivante a un dépôt caché, posé dans une de ses zones rouges, hors des zones au contact si possible. Il est à 1 000 m au moins de tout carré bleu, à 400 m au moins de tout point clé, près d'une route (150 m) et jamais dans la zone du dépôt précédent. Il n'est posé que hors de vue, à plus de 1 500 m de tout joueur. Le décor existe sans joueur. Ses gardes (1 ou 2 sections, comme les missions) n'apparaissent qu'à 1 500 m d'un joueur et comptent dans les 120.
- RE9 : le dépôt d'une région est révélé par la capture de son officier, par la saisie d'un poste de commandement d'une de ses zones, ou par la réussite d'une mission « écoute » ou « documents » sur son territoire. Un dépôt révélé apparaît sur la carte et au PC, avec un message radio. Après chaque sabotage, il change de place et redevient caché ; un renseignement reçu pendant le déplacement révèle le nouveau dépôt dès sa pose.
- Ajout logique (question 4) : si la zone du dépôt passe bleue, le dépôt compte comme saboté (24 h de coupure) et change de place.
- RE2 : une région vivante, organisée (OF6) et sans renflouement en route, qui a moins de 4 obus (moins d'un tir, MO5), est renflouée. La région organisée la plus riche au-dessus de la moitié de son plein lui cède jusqu'à la moitié du plein de la région à sec, sans descendre elle-même sous la moitié du sien. Les obus arrivent 1 h plus tard : c'est un compte à rebours, aucune colonne ne roule. Pas de renflouement quand le Commandeur est coupé (VU6).
- RE7 et C1 (tickets) : un moyen est réservé au moment de la décision, et le stock baisse aussitôt. Il est consommé quand il part pour de bon (obus tiré, blindé détruit ou pris, sortie d'hélico finie ou abattue, tir lourd ou bombe tombés). Il est rendu quand il est annulé ou qu'il revient vivant (obus non tirés, blindé qui se retire : question 8).
- AP3 : au plus une bombe par jour du calendrier (heure du serveur), et seulement si 10 joueurs ou plus sont connectés (question 3).
- CA7 : le niveau des stocks est noté au départ du dernier joueur (moyenne du remplissage des obus de l'île, des blindés, de l'hélico et de l'artillerie ; bombe exclue). À 95 centièmes ou plus, l'offensive de nuit gagne 10 chances (60 au plus). Sous un tiers, il n'y a pas d'offensive cette nuit.
- VU2 : état de ravitaillement d'une région, montré seulement après un renseignement. « Affaiblie » : obus sous 34 centièmes du plein, ou dépôt coupé. « Renforcée » : 90 centièmes ou plus, sans coupure. Sinon « normale ».
- VU5 : une décision forcée par le Staff ne coûte rien (réglable).
- VU6 : Commandeur coupé ou front gelé : le revenu, les dépôts et la capacité continuent ; seules les dépenses et les renflouements s'arrêtent.
- RE13 : tout l'état va dans front.json (stocks, coupures, bombe du jour, dépôts avec leur place et leur état révélé, niveau CA7). Au redémarrage, les tickets ouverts sont rendus au stock, les renflouements en route rendus au donneur, et les soldats d'un renfort parti mais pas arrivé sont rendus à la localité qui les avait envoyés (ses pertes F12 baissent d'autant).
- Arbitrage (limite du jeu) : 160 IA actives, posées au démarrage par AIWorld.SetLimitOfActiveAIs (vérifié : Game/generated/AI/AIWorld.c:21), relues 2 s après puis chaque minute. Budget : 120 soldats + 25 civils + 2 pilotes = 147, soit 13 places de marge, dont 10 toujours gardées sous la limite.
- Q7 et RE11 : 120 soldats ennemis au sol au plus, tout compris : garnisons, postes, camions et chauffeurs, vagues de contre-attaque et de mission, patrouilles, jeeps, gardes, officiers et leurs gardes, servants de mortier, équipages de blindé, gardes du dépôt et de la batterie 2S1. Seul l'équipage de l'hélico en vol est hors des 120, mais dans la marge des 160.
- RE11 : 20 places sont gardées aux renforts (classe combat). Q7 : 12 places sont gardées aux postes du front, seulement quand un poste est possible près d'un joueur (4 postes au plus). Il reste 88 places pour tout le reste. Une réserve n'est gardée que pour sa part encore inutilisée.
- RE12 : l'ordre de passage est combat (renforts, vagues, localité attaquée, servants et équipages d'appui), puis défense des localités au calme, puis postes et gardes, puis patrouilles. Tant qu'une demande d'une classe plus haute attend, aucune classe plus basse n'est posée. Si une demande de combat est bloquée, on retire hors de vue les patrouilles à plus de 800 m des joueurs et les jeeps à plus de 1 500 m, non vues et sans contact depuis 60 s, les plus lointaines d'abord, 2 par passage de 5 s. On ne retire jamais un soldat vu ou au combat.
- Groupes (prefab) : 36 au plus, dont 6 gardés aux missions (existant), 5 aux renforts et 4 aux postes.
- Partage des localités : quand les garnisons voulues dépassent la place, une localité attaquée est servie d'abord, à sa taille. Les autres reçoivent une part proportionnelle à leur taille décidée, doublée si un joueur est à moins de 1 000 m. Chaque part vaut 8 au moins et jamais plus que la taille. Une garnison déjà posée au-dessus de sa part n'est jamais réduite ; seul son complément s'arrête (question 6).
- C2 : au plus un renfort par zone visée toutes les 10 min (heure Unix), contrôlé à la demande et au départ. La localité qui envoie garde au moins la moitié de son effectif plein : elle n'envoie que ce qui dépasse, 2 soldats au moins, sinon il n'y a pas de camion (question 5). Les soldats envoyés comptent comme pertes F12. Si la localité est posée, une section d'effectif voisin la quitte hors de vue ; si elle est vue, les soldats sont seulement comptés en pertes. Le camion forcé par le Staff et les vagues de contre-attaque ne sont pas freinés.
- Camions : un camion qui ne tient pas dans la place part quand même avec moins de monde s'il reste au moins 5 places (4 soldats et le chauffeur). Sinon il attend 10 min au plus (existant).
- Enforce : string.Format prend 9 paramètres au plus (rapports Staff en plusieurs appels) ; aucun signe pour cent (on écrit « centièmes ») ; aucune variable nommée map, set, array ou base ; un seul case par branche, ou des if/else ; une seule déclaration par nom de variable dans une fonction (index de boucle déclarés une fois en tête, i, j, k distincts) ; pas de vector + float (Vector(dx, 0, dz)) ; pas de |= ; aucun paramètre out avec valeur par défaut (CanReserve, CanReinforce, TryUse, PickSite) ; aucun override (aucune méthode vanilla surchargée) ; SRP_EnemyGroup est une classe du mod, donc pas de préfixe SRP_ exigé.
- Textes : radio, Discord et journal Staff passent par SRP_FrontRadio. Ils n'emploient jamais « IA », « automatique », « garnison », « groupe » ni « patrouille » pour parler du Commandeur (on écrit « localité », « éléments », « ronde », « soldats »), et jamais de signe pour cent.
### Réglages
- Fichier $profile:SimpleRP/commandeur_reglages.txt, créé avec ces valeurs, relu au démarrage et par Staff > Relire les réglages du Commandeur, sans republier. Une clé absente reprend sa valeur par défaut ; une valeur hors bornes est bornée, avec une ligne de journal.
- res_obus_plein = 48 : obus pour toute l'île à menace 0, partagés au prorata des zones de départ (6, 7, 9, 9, 8, 9) (RE3 moyen, RE2)
- res_blindes_plein = 2 (RE3)
- res_helico_plein = 3 : sorties de l'hélico de recherche (RE3, AP7)
- res_artillerie_plein = 1 : tirs d'artillerie lourde (RE3)
- res_bombe_plein = 1 (AP3)
- res_bonus_menace_centiemes = 5 : plein grossi de 5 centièmes par point de menace (RE10)
- res_bombe_bonus_menace = 0 : la bombe ne grossit pas avec la menace (AP3, au plus une par jour)
- res_heures_plein = 4 : heures pour passer de vide à plein au revenu plein (RE5)
- res_heures_bombe = 24 : même chose pour la bombe (AP3, 1 par jour)
- res_bombes_par_jour = 1 (AP3)
- res_bombe_joueurs_min = 10 : joueurs connectés (AP3, question 3)
- res_plancher_centiemes = 50 : revenu jamais sous la moitié (RE4)
- res_zones_depart = 0 : 0 = somme des zones du fichier des régions (66)
- res_depot_coupure_heures = 24 (RE8)
- res_depot_coupure_centiemes = 50 : part du revenu coupée (RE8)
- res_depot_distance_bleu_m = 1000 : distance minimale à un carré bleu
- res_depot_distance_point_cle_m = 400 : distance minimale à un point clé
- res_depot_route_m = 150 : route recherchée à cette distance
- res_depot_distance_pose_m = 1500 : jamais posé plus près d'un joueur, toujours hors de vue
- res_depot_gardes_distance_m = 1500 : gardes posées à l'approche (RE9, comme les missions)
- res_depot_gardes_max = 2 : sections de garde au plus
- res_depot_gardes_retrait_m = 2500 et res_depot_gardes_retrait_minutes = 10
- res_depot_prefab = {361B78F0EC88F339}Prefabs/Props/Military/SupplyBox/SupplyStack/SupplyStack_Large_01/SRP_Cache.et (action « Saboter » déjà présente)
- res_renflouement_minutes = 60 (RE2)
- res_renflouement_seuil_obus = 4 : « à sec » = moins d'un tir (MO5)
- res_renflouement_centiemes = 50 : jusqu'à la moitié du plein de la région à sec
- res_renflouement_donneur_centiemes = 50 : le donneur garde la moitié de son plein
- res_sabotage_centiemes = 25 : retrait après une mission de sabotage ou de cache réussie (RE8, question 2)
- res_offensive_bonus = 10 ; res_offensive_plein_centiemes = 95 ; res_offensive_bas_centiemes = 33 (CA7)
- res_etat_affaiblie_centiemes = 34 ; res_etat_renforcee_centiemes = 90 (VU2)
- res_staff_gratuit = 1 : une décision forcée par le Staff ne coûte rien (VU5, question 7)
- cap_limite_jeu = 160 : limite d'IA actives, posée au démarrage (arbitrage)
- cap_soldats_max = 120 : soldats ennemis au sol, tout compris (Q7, RE11)
- cap_reserve_renforts = 20 (RE11)
- cap_reserve_postes = 12 (Q7) ; les 4 postes au plus restent réglés dans le module ennemi
- cap_groupes_max = 36 (prefab) ; cap_groupes_reserve_missions = 6 ; cap_groupes_reserve_renforts = 5 ; cap_groupes_reserve_postes = 4
- cap_marge_jeu = 10 : places gardées sous la limite du jeu (existant, camions)
- cap_civils_prevus = 25 ; cap_pilotes_prevus = 2 : contrôle de cohérence (120 + 25 + 2 + 10 ≤ 160)
- cap_retrait_rondes_m = 800 ; cap_retrait_jeeps_m = 1500 ; cap_retrait_par_passe = 2 (RE12)
- cap_demande_secondes = 45 : durée de validité d'une demande en attente
- cap_garnison_min = 8 ; cap_garnison_max = 60 ; cap_garnison_proche_m = 1000 ; cap_garnison_poids_proche = 2 (partage des localités, question 6)
- cap_importance_ennemis = 3 (CRITICAL) ; cap_importance_rondes = 2 (HIGH) ; cap_importance_civils = 2 (HIGH)
- frein_minutes = 10 : un renfort par zone au plus toutes les 10 min (C2)
- frein_garde_centiemes = 50 : la localité qui envoie garde la moitié de son effectif plein (C2, question 5)
- frein_chargement_min = 2 : sous ce nombre, pas de camion
- frein_section_suit = 1 : si la localité qui envoie est posée, une section la quitte hors de vue avec le camion
### Interfaces fournies
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
### Interfaces attendues
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
### Changements du plan du front
- mod_ennemi, capacité : « 100 soldats (m_iMaxSoldiers, groupes m_bBudget) et 30 groupes ; réserve de 12 soldats et 4 groupes pour les postes dans les 100 ; vagues, gardes et patrouilles hors plafond » → « 120 soldats au sol, tout compris (RE11), tenus par SRP_CmdCapacity ; réserves de 20 aux renforts et de 12 aux postes ; groupes 36, dont 6 aux missions, 5 aux renforts et 4 aux postes ; chiffres dans commandeur_reglages.txt ».
- mod_ennemi, garnisons (SoldierCap) : « plafond des 100 moins la réserve des camions (GetSoldierReserve) » → « GarrisonCap : partage au prorata des localités voulues, 8 au moins, localité attaquée servie d'abord ; GetSoldierReserve supprimé ».
- mod_ennemi, réglages m_iPostSoldierReserve, m_iPostGroupReserve et m_iAIHeadroom (attributs du prefab) → cap_reserve_postes, cap_groupes_reserve_postes et cap_marge_jeu dans le fichier du Commandeur.
- mod_ennemi, camions : « départ des localités rouges voisines ; limites de 100 soldats et marge de 10 IA » → « classe COMBAT ; frein C2 (un par zone toutes les 10 min, la localité garde la moitié, soldats envoyés = pertes F12) ; chargement réduit si la place manque ».
- mod_ennemi, vagues de contre-attaque : « hors des 100 ; 3 vagues ; taille = min(formule, budget local) » → « comptées en COMBAT ; taille bornée par Room(COMBAT) ; 2 à 4 vagues selon la 1re (CA3), sans stock (C1) ».
- mod_ennemi, patrouilles F10 et jeeps : « plafond de groupes seulement » → « Refusal(PATROUILLE) ; retirées hors de vue quand un renfort attend (RE12) ».
- mod_ennemi, offensive de nuit H2 : « 10 + 5 × menace, 60 au plus » → « + OffensiveChanceBonus (10 de plus si les stocks étaient pleins au départ du dernier joueur) ; pas d'offensive sous un tiers (CA7) ».
- mod_ennemi, hélico G12 : « tirage de 10 pour cent le jour et 25 la nuit, 15 de plus à chaque échec » → « décision du Commandeur sur son stock (AP7) : Reserve(HELICO) à la sortie, Consume à la fin, rendu au redémarrage ».
- mod_ennemi, services : ajout de GetLocalityFullStrength, GetLocalityStrength, AddLocalityLosses, RetireSectionFor, IsLocalityUnderAttack et IsPostPossibleNearPlayers ; appel de SetWantedGarrisons à chaque passage de 5 s.
- mod_missions (SRP_Missions.c) : « gardes et vagues hors plafond ; OnObjectiveUsed ne cherche que les missions » → « gardes comptées en GARDE, vagues en COMBAT ; dépôt ennemi testé en premier ; une mission SABOTAGE ou CACHE réussie retire un quart d'un stock ; une mission ECOUTE ou DOCUMENTS révèle le dépôt de la région ».
- mod_zones (SRP_ZoneFall) : « appelle missions, économie, ennemi et radio » → « appelle aussi SRP_CmdResources.OnZoneOwnerChanged après l'économie ; la saisie du poste de commandement (D4) révèle le dépôt de la région ».
- mod_grille (socle, front.json) : « zones, localités, points clés, extras » → « plus les clés cmd_res_*, cmd_dep_* et cmd_cap_*, écrites par le Commandeur (WriteState/ReadState lus après les localités) ; ResetCampaign → ResetForCampaign ; RestoreDaily → ReadState ; AddThreat → OnThreatChanged ».
- mod_carte (SRP_FrontRadio, écrans Staff, PC) : « textes du front » → « plus EnemyDepotSabotaged, EnemyDepotRevealed, EnemyDepotFell, EnemyStockHit, DepotSabotagedPlayerText et CommanderJournal ; page Staff « Ressources ennemies » et ses boutons ; dépôts révélés sur la carte et au PC ; état de ravitaillement d'une région (VU2) ».
- mod_web : « liste des évènements du front » → « plus depot_ennemi_sabote et depot_ennemi_revele ; catégorie COMMANDEUR vers un salon réservé au Staff (VU4) ».
- mod_migration : « socle compilé seul d'abord » → « SRP_CmdSettings.c et SRP_CmdCapacity.c compilés avec le socle, car les poses d'IA en dépendent ; un front.json sans clés cmd_ donne des stocks pleins et des dépôts posés au premier démarrage ; le contrôle des appels orphelins cherche aussi les SpawnGroup, SpawnGroups et SpawnWave sans Refusal ».
- coherence (question 7, proposition des 100 soldats avec 2 groupes de garde au plus dans un village gardé) → remplacée par l'arbitrage Q7 (120 soldats, dont 12 aux postes) et la file par priorité de RE12, sans limite propre aux gardes.
- Hors plan du front : SRP_Civilians.c (SetImportance HIGH, texte « 160 ») et SRP_Delivery.c (garnison de livraison en classe GARDE).

## Sous-module sous_staff.json
### Résumé
Ce sous-module montre le Commandeur aux joueurs et au Staff, puis organise la livraison du bloc front + Commandeur.
Les joueurs voient trois choses. D'abord la chute d'un officier, à la radio et dans le salon territoire (« l'officier ennemi Volkov est tombé, région de Saint-Philippe »). Ensuite une section « Renseignement » au PC (onglet Territoire), qui ne se remplit qu'après un PC ennemi saisi, un officier capturé ou une mission réussie. Enfin le dépôt ennemi sur la carte, une fois repéré. Rien n'annonce les tirs ni les blindés de l'ennemi (EQ2).
Le Staff a un nouvel onglet « Commandeur » (F10). On y trouve l'interrupteur « Commandeur gelé », séparé du front gelé, les stocks, les 6 régions et le journal des décisions (raison, coût, reste). On peut aussi y forcer un mortier, un renfort, un blindé, l'artillerie, la bombe, les passages d'avion, une colonne, un décrochage ou la chute d'un officier, à sa position ou sur un joueur.
Chaque décision part dans un salon Discord réservé au Staff (état-major-ennemi, sinon journal-serveur). Elle ne va jamais dans l'onglet Journal du PC, qui est visible de tous : vérifié, c'est une vraie fuite à boucher.
Livraison en un seul bloc : on écrit d'abord tous les contrats, avec des corps vides. Le mod compile à chaque étape et tourne comme « Commandeur gelé » tant que le cerveau n'existe pas. On ajoute ensuite tout le cerveau d'un coup, puis tous les moyens.
Volume : environ 1 350 lignes pour ce sous-module, environ 15 500 lignes au total (front + Commandeur) et une quarantaine de fichiers du mod.
### Règles
- VU1 : à l'écran, « officier ennemi Volkov » (nom de famille seul, jamais de grade) et « région de Saint-Philippe », du nom de la plus grande localité de la région. Devant une voyelle ou un h, on élide : « région d'Entre-Deux ». Dans les menus, la clé technique est le code R1 à R6, comme S07 pour les zones.
- VU2 + I3 : au PC, onglet Territoire, chaque zone ennemie au front affiche toujours sa région, qui est de la géographie. Si la région est renseignée, la ligne ajoute aussi « officier ennemi Volkov » et « troupes renforcées / normales / affaiblies ». La section « Renseignement sur l'ennemi » liste les 6 régions : renseignée, désorganisée, pas de renseignement ou libérée. Les joueurs ne voient jamais le caractère de l'officier, sa cachette du jour, les stocks, l'alerte ni les décisions.
- VU2 (déclencheurs) : trois faits renseignent une région. Saisir un PC ennemi (D4) renseigne la région qui tenait la zone. Capturer un officier renseigne sa région. Réussir une mission dont le site est sur un carré rouge renseigne la région de ce carré. Le renseignement tombe tout seul quand un nouvel officier arrive, puisqu'il faut connaître le nouveau chef. Il est gardé dans front.json (extras), effacé à la nouvelle campagne et repris par le retour à une copie datée.
- RE9 : un PC saisi, un officier capturé ou une mission ÉCOUTE ou DOCUMENTS révèlent aussi le dépôt de la région. Il apparaît sur la carte et au PC (« carré 074 042 ») jusqu'à son sabotage : le dépôt change alors de place et redevient inconnu.
- VU3 : la chute d'un officier (tué ou capturé) passe à la radio à tous les joueurs connectés, puis dans le journal TERRITOIRE. Celui-ci l'envoie dans le salon territoire et dans journal-serveur, et le pont en envoie une copie dans alertes. Texte repris de l'exemple : « l'officier ennemi Volkov est tombé, région de Saint-Philippe » (« a été capturé » pour une capture).
- VU4 : chaque décision est écrite avec sa raison, son coût et le stock restant. Elle arrive dans le menu Staff (200 décisions gardées en mémoire, 15 par page) et dans le salon Staff état-major-ennemi, ou dans journal-serveur s'il n'a pas de webhook. Elle va aussi dans le fichier journal du mois. Les décisions écartées (seuil, plafond, stock, frein) restent dans le menu, en gris.
- VU4 (confidentialité, vérifiée) : l'onglet Journal du PC montre à tous les joueurs les 80 dernières lignes, toutes catégories (SRP_PC.c:547-553, SRP_JournalComponent.c:39 et 58). La catégorie COMMANDEUR n'entre donc plus dans ce tampon. Les actions du Staff sur le Commandeur s'écrivent en COMMANDEUR et jamais en STAFF.
- VU5 : le Staff voit tout (stocks, officiers, journal), gèle le Commandeur et force une décision à sa position ou sur un joueur connecté. Moyens forçables : mortier explosif, fumigène, feinte, artillerie lourde, bombe, passages d'avion, BTR-70, BRDM-2, Typhoon, renfort, hélico, harcèlement, colonne, décrochage. Il peut aussi faire tomber un officier (tué ou capturé). Une décision forcée passe outre les seuils (EQ4), les plafonds (EQ3) et les délais. Elle garde la sécurité : base, soldats ennemis ou civils trop près, limite des 120, règle de la moitié du frein C2. Elle marche même Commandeur gelé. L'artillerie, la bombe et la chute d'un officier demandent 2 clics.
- VU6 : deux interrupteurs séparés. Le front gelé (J3) est dans l'onglet Territoire, le Commandeur gelé dans l'onglet Commandeur. Chacun affiche l'état de l'autre et l'en-tête Staff montre les deux. Commandeur gelé : plus aucune décision (renforts, colonnes, décrochages ordonnés, harcèlement, appuis) ; les tirs pas encore partis sont annulés ; les soldats posés réagissent sur place. Les contre-attaques suivent alors les seules règles du front, sans appui, et H2 garde la formule du front sans la correction CA7. Les stocks continuent de se refaire, bornés par les plafonds EQ3. Aucune radio pour ce gel.
- CO3 + EQ2 : le mot « Commandeur » n'apparaît que chez le Staff. La radio publique ne parle que de faits accomplis (officier tombé, région libérée, renseignement, dépôt saboté, blindé ramené). Rien avant ni pendant un tir de mortier, d'artillerie, une bombe, un blindé, un renfort ou une colonne.
- OF7 + OF9 : les durées affichées (désorganisée encore 2 h 10, muette encore 18 h, sabotée encore 20 h) sont calculées depuis les heures Unix sauvegardées par B, C et D : 3 h si tué, 6 h si capturé, redémarrages compris. Jamais GetTickCount.
- OF11 : une région entièrement bleue est annoncée à la radio et dans le salon territoire (« toute la région de Saint-Philippe est à nous : l'officier ennemi Volkov a quitté l'île »). Elle passe en gris « libérée » au PC et au Staff.
- C3 : un blindé ramené à la base est annoncé à la radio et dans le salon territoire (« blindé ennemi BTR-70 ramené à la base — prime de 2000 € versée au coffre »). Le versement et sa ligne TRESORERIE restent à D.
- Coherence trou 15 : SRP_FrontRadio reste le SEUL auteur des textes publics, du Commandeur compris. Les textes Staff (journal des décisions) passent tous par SRP_EnemyCommandLog.
- Coherence trou 16 : aucun fichier de sauvegarde à part. Le renseignement vit dans les extras de front.json (clés vu_intel_R1 à R6). Le journal des décisions n'est pas un état : il vit en mémoire, dans le salon Staff et dans le fichier journal du mois.
- Mots : aucun texte du Commandeur envoyé à la radio ou à Discord (salon Staff compris) ne contient « IA », « automatique », « garnison », « groupe » ou « patrouille », ni de signe pour cent (« 0,82 du plein », « 110 centièmes »). Les textes publics évitent aussi la liste confidentielle de SRP_Discord.c:246-248 et les ajouts du plan web (« aucune zone », « tirage »). En mode essai, SRP_EnemyCommandLog signale au journal tout mot interdit reçu d'A à D.
- Idées refusées respectées : pas de bilan de soirée (aucun résumé dans le salon Staff), pas de réglage de difficulté au menu Staff (ni bouton de stock, ni seuil modifiable, seulement « relire le fichier de réglages »), pas de médaille pour une capture d'officier, pas de reddition.
- LI1 + LI2 : un seul bloc publié, mais codé en étages. D'abord les contrats vides (le mod tourne comme le front seul), puis tout le cerveau d'un coup (A, B, C, dont les moyens ne font que s'écrire au journal), puis tous les moyens d'un coup (D). Chaque étape compile.
- Enforce : concaténation pour toute ligne à plus de 9 valeurs, aucun signe pour cent, aucune variable nommée map, set, array ou base, des if à la place des case empilés, aucun override (classes statiques sans parent), paramètres out sans défaut, pas de |=, une déclaration par nom et par fonction. Pas de vecteur plus nombre : on écrit position + Vector(3, 0, 3).
### Réglages
- vu_journal_memoire = 200 : décisions gardées en mémoire pour le menu Staff (VU4). Le salon Staff et le fichier du mois gardent tout.
- vu_journal_par_page = 15 : lignes par page du journal dans le menu Staff.
- vu_refus_menu = 1 : les décisions écartées (seuil, plafond, stock, frein) se lisent dans le menu Staff, en gris.
- vu_refus_discord = 0 : les décisions écartées ne partent pas au salon Staff (évite le bruit).
- vu_forcer_puise = 0 : une décision forcée par le Staff ne puise pas dans les stocks de l'ennemi (question à Jack).
- vu_renseignement_heures = 0 : le renseignement dure jusqu'à l'arrivée d'un nouvel officier dans la région. Avec un nombre d'heures, il s'arrête aussi au bout de ce délai (question à Jack).
- vu_renseignement_missions = toutes : toute mission réussie sur un carré rouge renseigne sa région (VU2). On peut aussi donner une liste de types, par exemple DOCUMENTS ECOUTE OFFICIER.
- vu_depot_missions = ECOUTE DOCUMENTS : missions qui révèlent aussi le dépôt (RE9). Un PC saisi ou un officier capturé le révèlent toujours.
- vu_troupes_renforcees = 110 : à partir de 110 centièmes de l'effectif normal, les troupes d'une zone sont « renforcées ». Avec un officier vivant (+25 pour cent, OF5), c'est le cas dès le départ.
- vu_troupes_affaiblies = 70 : à 70 centièmes ou moins, les troupes sont « affaiblies ».
- vu_radio_silence = 1 : annonce radio quand une pièce de mortier ou une batterie est réduite au silence (question à Jack).
- vu_officier_discord = 1 : la chute d'un officier part aussi dans le salon territoire (VU3 : oui).
- vu_depot_icone = 31 et vu_depot_couleur = 4 : repère du dépôt ennemi révélé (même icône que nos dépôts, couleur ennemie).
- vu_proximite_m = 3000 : rayon du rapport Staff « autour de moi » (officier, mortier, 2S1, blindés, colonnes).
- discord.json du profil : nouvelle clé « etat-major » = webhook du salon état-major-ennemi (Staff seul). Sans elle, secours automatique vers journal-serveur.
- setup_discord.py : nouveau salon état-major-ennemi (préfixe médaille), dans la catégorie STAFF, lu et écrit par le Staff seul.
### Interfaces fournies
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
### Interfaces attendues
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
### Changements du plan du front
- Carte, SRP_Admin.c, onglets : Rapide, Joueurs, Missions, Territoire, Logistique, Serveur, Danger → ajout de « Commandeur » après Territoire. Touche IsTab, BuildMenu, les pages internes d'Execute (region, journal, forcer, cible), NeedsConfirm (officier-tue, officier-capture, frappe), PageAfter, et Run → RunCommandeur.
- Carte, en-tête Staff (StaffHeaderPart) : « menace 3/10, 12/66 zones[, FRONT GELÉ] » → + « , COMMANDEUR GELÉ » quand il est gelé.
- Carte, onglet Staff Territoire, bloc Front : l'interrupteur « front gelé » seul → + une ligne d'état du Commandeur (actif ou GELÉ, interrupteur séparé, onglet Commandeur) (VU6).
- Carte, Rapide « ia-groupes » (EnemyGroupsPage l.407-450) : sirènes, radio, camions → + rapport de proximité du Commandeur (officier et cachette, mortier, 2S1, blindés, colonnes) en première page.
- Carte, SRP_FrontScreens.PCTerritoire : zones ennemies au front avec I3 seul → + « région de X », et si la région est renseignée « officier ennemi Volkov · troupes renforcées/normales/affaiblies ». Zones de l'arrière → + « région de X ». Nouvelle section « Renseignement sur l'ennemi » (6 régions).
- Carte, SRP_FrontRadio : textes de zone seulement → + OfficerFell, RegionLiberated, IntelGained, EnemyDepotSabotaged, SupportSilenced, ArmorBroughtBack. Aucun texte pour les moyens du Commandeur ni pour son gel (EQ2).
- Carte, SRP_FrontMarkers : icônes des points clés et des attaques → + repère « Dépôt ennemi — région de X » tant que le dépôt est révélé (RE9).
- Web, SRP_Discord.c : 7 flux → + « etat-major ». La catégorie COMMANDEUR va vers etat-major seul (secours journal-serveur), jamais publique. TitleFor et ColorFor pour COMMANDEUR. Pour TERRITOIRE, les tests « officier ennemi » et « blindé ennemi » (vert), « renseignement » (marine) et « dépôt ennemi » (sarcelle) passent AVANT ceux du plan web.
- Web, liste des évènements du pont : types du web + depot_detruit, offensive, commandement → + officier_tombe, region_liberee, renseignement, depot_ennemi, blinde_ramene. Aucune décision du Commandeur ne passe par le pont.
- Web, bot.py ALERT_STYLES : + 5 styles. handle_events : rien de plus que le plan web (tous les évènements, par messages de 10).
- Web, setup_discord.py, catégorie STAFF : staff, journal-serveur, alertes → + état-major-ennemi (Staff seul, avec webhook).
- Migration, SRP_JournalComponent.c : toutes les catégories vont dans le tampon des 80 dernières lignes (onglet Journal du PC, visible de tous) → la catégorie COMMANDEUR n'y entre plus.
- Migration, ordre de codage : 10 étapes du front → 14 étapes globales. Les contrats du Commandeur sont écrits dès l'étape 1, avec des réponses neutres (le mod reste « front seul » jusqu'au cerveau). Le cerveau est à l'étape 9, les moyens aux étapes 10 et 11. Quatre séances d'essai au lieu de trois.
- Migration, check-list Workbench : essais 1 à 11 → + 12 à 31 (Commandeur). Check-list serveur : + 8 à 10 (salon état-major, client réel, gel en soirée). Contrôles verif_front.py : a à j → + k à o.
- Migration, guide : + sections Commandeur (§1, §4, §7, §9.14, §9.15, §10, §11).
- Migration, SRP_Admin RunServeur « ia-reset » et RunDanger « ia » et « wipe » → + SRP_EnemyCommand.CancelOperations(author).
- Zones, SRP_ZoneFall, saisie D4 → + SRP_EnemyIntel.OnKeyPointSeized(zone) avant le changement de camp.
- Missions, SRP_Missions.End, succès → + SRP_EnemyIntel.OnMissionSucceeded(type, site, id).
- Grille, extras de front.json : petites valeurs des modules (date H2…) → + vu_intel_R1 à R6 (renseignement des joueurs).
