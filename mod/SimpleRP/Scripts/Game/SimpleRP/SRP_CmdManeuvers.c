//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : LES MANŒUVRES, toute l'infanterie qui bouge.
//
// RÔLE (serveur ; tenu par SRP_Commander.m_Maneuvers, accès SRP_CmdManeuvers.Get()) : le cerveau SAIT et DÉCIDE,
// ce module EXÉCUTE l'infanterie (arbitrage C1 : soldats et camions gratuits, bornés par la place des 120, par la
// garnison source qui se vide et par le frein C2) :
// - renforts (MA1 à MA3) : SEUL détenteur du frein C2 (renfort_frein_min par zone, heure Unix, non sauvé) et du
//   registre des soldats envoyés (champs de SRP_FrontLocality par SRP_FrontEnemyComponent.AddSent/ReturnSent) ;
//   source = localité rouge voisine NON posée qui garde renfort_source_garde_centiemes de son NOMINAL (67 pour un
//   officier prudent), camions déjà demandés et pas encore posés déduits (NotePending) ; arrivée en
//   renfort_arrivee_min_s à renfort_arrivee_max_s, x renfort_facteur_desorg si la
//   région est désorganisée (SEUL endroit du x2, trou 12) ; camion de contact (remplace le camion d'alerte #69 tant
//   que le Commandeur n'est pas gelé), camion d'entrée #69 (RequestEntryTruck, même frein) ; colonne sur le papier
//   si la source est loin (MA4), réelle à colonne_distance_m d'un joueur ;
// - enquête (ENQUETE : unité mobile la plus proche, fouille puis retour), décrochage #40 (MA5 à MA7), doctrine
//   d'assaut (MA8), harcèlement (MA10), réorganisation (MA11), poches (MA12), unités sur le papier (CA6) ;
// - contre-attaques CA1 à CA6 : crochets appelés par SRP_FrontEnemyComponent (garde : IsCommanderActive(), SEULE
//   définition de « Commandeur actif » : présent, démarré, non gelé), soutien CA4 et
//   feinte MO8 DÉCIDÉS ici (2e axe réel), exécutés par SRP_CmdSupport.Request* ; passage de Su-57 une contre-
//   attaque sur ca_avion_chance ; fin CA5/C8 rendue en SRP_EAttackEnd (DEFENDUE aux deux tiers de pertes).
// La gravité MA9 est calculée par le cerveau (SEULE formule) ; l'offensive de nuit CA7 relaie SRP_CmdResources.
// CO5 : jamais la vraie position des joueurs pour décider (connaissance du cerveau) ; seules exceptions : la vue et
// la distance pour poser ou effacer hors de vue (SRP_Placement), le camion d'entrée #69 (LoadFor, réflexe voté) et
// le carré du point clé occupé (« retranchés », information visible : carré orange).
// Groupes toujours tenus par ref. Sauvegarde : clés cmd_ma_* (WriteTo / ReadFrom) ; RE13 : les colonnes en route
// (papier ou réelles, cmd_ma_c*) et les camions de renfort lancés et pas encore déchargés (cmd_ma_t*) sont rendus à
// leur source à la relecture, une seule fois ; contre-attaque, papier, raids, replis et débarqués : rien (H3).
// Débit des envoyés (MA2) AU DÉPART, une seule fois : camion hors colonne -> OnTruckLaunched ; colonne -> NewColumn
// (papier ou réelle ; les camions d'une colonne n'appellent jamais OnTruckLaunched) ; décrochage -> StartRetreat
// (la colonne de repli s'ouvre à 0 soldat et ne débite rien). Retour des soldats vivants : UNE seule porte,
// SRP_EnemyComponent.RetireTick (m_sSourceLocality du groupe, si renfort_rendre_survivants) ; ici, seulement
// ReturnSent d'une colonne annulée ou arrivée vers un contact, et la restitution RE13 de ReadFrom. Les soldats demandés
// pour un camion de colonne qui ne partent pas sont rendus par le camion à son départ (SettleDeparture) : ils sortent
// alors de la colonne (OnColumnTruckDeparted), pour que ColumnPending ne les rende jamais une seconde fois.
// Textes : journal du Commandeur seulement (SRP_CmdLog, salon Staff) ; jamais les mots IA, automatique, garnison,
// groupe, patrouille : « défenseurs », « troupes », « section », « détachement », « colonne », « localité ».
// APPELÉ PAR : SRP_Commander (vie, ordres, relais), SRP_FrontEnemyComponent (crochets CA, IsLocalityEmptied,
// IsKeptFull, IsOnPlayersPath), SRP_TerritoryComponent (RequestEntryTruck), SRP_EnemyTruckComponent (OnTruckLaunched, OnTruckUnloaded,
// OnColumnTruckDeparted, OnColumnDelivered), SRP_EnemyComponent (OnGroupGone), SRP_CmdScreens (Staff).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Une colonne de soldats entre deux localités (MA4, MA6, Staff), sur le papier puis réelle. En route : sauvée
//! (clés cmd_ma_cN_*) et rendue à sa source à la relecture (RE13).
class SRP_CmdColumn
{
	int m_iId;
	int m_iKind = SRP_ECmdColumnKind.RENFORT;
	int m_iState = SRP_ECmdColumnState.PAPIER;
	string m_sFrom;								// localité source
	string m_sTo;								// localité visée ("" = vers un contact)
	int m_iSoldiers;							// soldats de la colonne, débités à sa source (moins ceux qu'un camion n'a pas emmenés, rendus)
	int m_iZone = -1;							// zone du contact (renfort)
	vector m_vContact;
	ref array<vector> m_aRoute = {};			// points de route (SRP_Placement.BuildRoadRoute) plus les extrémités
	float m_fLength;							// longueur à plat
	int m_iDepartUnix;							// 0 = pas encore partie (colonne de repli qui attend ses soldats)
	float m_fSpeed;								// colonne_vitesse
	int m_iNextTryUnix;							// prochaine tentative de passage au réel
	int m_iTrucks;								// camions posés
	int m_iAboard;								// soldats confiés aux camions posés : demandés, puis assis au départ de chaque camion (OnColumnTruckDeparted) ; le reste roule sur le papier
	int m_iTrucksDone;							// camions revenus (OnColumnDelivered)
	int m_iAboardDone;						// part de m_iAboard des camions revenus (OnColumnTruckDone) : plus rien à rendre
	int m_iDelivered;							// soldats débarqués en état de combattre
	int m_iRealUnix;							// passage au réel (heure Unix)
	bool m_bStaff;								// forcée par le Staff (VU5 : continue même Commandeur gelé)
}

//------------------------------------------------------------------------------------------------
//! Un camion de renfort HORS colonne lancé et pas encore déchargé (RE13). Sauvé (clés cmd_ma_tN_*) ; relu = ses
//! soldats sont rendus à leur source (ReturnSent), une seule fois.
class SRP_CmdSentTruck
{
	int m_iId;
	string m_sFrom;								// localité source, débitée au départ (AddSent)
	int m_iLoad;								// soldats à bord
}

//------------------------------------------------------------------------------------------------
//! Une unité de contre-attaque « sur le papier », loin des joueurs (CA6). Non sauvée (H3).
class SRP_CmdPaperUnit
{
	int m_iZone = -1;
	int m_iWave;
	int m_iAxis;
	int m_iSoldiers;
	vector m_vPos;
	vector m_vOrigin;
	int m_iObjCell = -1;
	int m_iLastUnix;
	int m_iNextTryUnix;							// prochain essai de passage au réel (pose refusée, point introuvable)
}

//------------------------------------------------------------------------------------------------
//! Un décrochage en cours (#40, MA5, MA6). Non sauvé (H3).
class SRP_CmdRetreat
{
	string m_sLocality;
	string m_sTo;
	vector m_vExit;
	vector m_vContact;
	ref array<ref SRP_EnemyGroup> m_aGroups = {};
	ref SRP_EnemyGroup m_Cover;					// binôme qui couvre
	int m_iStartUnix;
	int m_iColumnId;
	bool m_bMoved;
	bool m_bStaff;								// forcé par le Staff (VU5 : continue même Commandeur gelé)
}

//------------------------------------------------------------------------------------------------
//! Plan d'assaut d'une vague ou d'un raid (MA8) ; la SEULE classe SRP_CmdAssault (trou 3). L'état d'appui d'une
//! contre-attaque est sur SRP_CounterAttack (m_bFeint, m_bSupportMortar, m_bSupportHeavy).
class SRP_CmdAssault
{
	int m_iZone = -1;							// -1 pour un raid
	int m_iMode = SRP_ECmdAssaultMode.FACE;
	vector m_vObjective;
	vector m_vFirePos;
	vector m_vOriginA;
	vector m_vOriginB;
	bool m_bHasB;
	ref array<ref SRP_EnemyGroup> m_aFire = {};
	ref array<ref SRP_EnemyGroup> m_aMove = {};
	ref array<ref SRP_EnemyGroup> m_aAxisB = {};
	int m_iStep;								// 0 mise en place, 1 ouverture du feu, 2 assaut
	int m_iStepUnix;
	bool m_bSmoked;
	int m_iWave;								// vague de la contre-attaque (0 pour un raid)
	int m_iObjCell = -1;						// carré visé (reprenable)
	int m_iObjSince;							// objectif tenu depuis (heure Unix, nouvel objectif après 180 s)
	vector m_vWaitA;							// deux axes : points d'attente à assaut_synchro_distance_m
	vector m_vWaitB;
	bool m_bFireJoined;							// la base de feu a rejoint l'assaut après sa suppression
	bool m_bFinal;								// attaque directe du point clé en cours
	int m_iFinalUnix;
	vector m_vFinalPos;
}

//------------------------------------------------------------------------------------------------
//! Un raid de harcèlement (MA10). Non sauvé (H3).
class SRP_CmdRaid
{
	int m_iCell = -1;
	vector m_vTarget;
	vector m_vOrigin;
	ref array<ref SRP_EnemyGroup> m_aGroups = {};
	ref SRP_CmdAssault m_Plan;
	int m_iStartUnix;
	int m_iContactUnix;
	int m_iPosted;
	int m_iNoPlayerSince;						// plus aucun joueur à 1 500 m depuis (heure Unix, 0 = il y en a)
	bool m_bStaff;								// forcé par le Staff (VU5 : continue même Commandeur gelé)
}

//------------------------------------------------------------------------------------------------
//! Les deux derniers relevés de contact d'une zone ou d'une région (joueurs « retranchés » MA8, « chemin » MA11)
class SRP_CmdTrack
{
	vector m_vA;
	int m_iA;
	vector m_vB;
	int m_iB;
}

//------------------------------------------------------------------------------------------------
//! Les manœuvres (serveur).
class SRP_CmdManeuvers
{
	// --- Réglages (clés renfort_* sauf renfort_taille_*, colonne_*, repli_*, assaut_*, harcelement_*, reorg_*, ca_*)
	bool m_bReinforceOn = true;		// renfort_actif
	int m_iBrakeMin = 10;		// renfort_frein_min
	int m_iSourceKeepCent = 50;		// renfort_source_garde_centiemes
	int m_iSourceKeepPrudentCent = 67;		// renfort_source_garde_prudent_centiemes
	int m_iSourceM = 4000;		// renfort_source_distance_m
	int m_iSourcePocketM = 6000;		// renfort_source_distance_poche_m
	int m_iArriveMinS = 180;		// renfort_arrivee_min_s
	int m_iArriveMaxS = 360;		// renfort_arrivee_max_s
	float m_fDesorgFactor = 2.0;		// renfort_facteur_desorg
	int m_iLoadMin = 2;		// renfort_chargement_min
	int m_iLoadMax = 14;		// renfort_chargement_max
	int m_iContactTruckRetireMin = 20;		// renfort_retrait_min
	bool m_bReturnSurvivors = true;		// renfort_rendre_survivants
	int m_iColumnRealM = 1500;		// colonne_distance_m
	float m_fColumnSpeed = 7.0;		// colonne_vitesse
	int m_iColumnPlayerMinM = 700;		// colonne_joueur_min_m
	int m_iColumnBackStepM = 100;		// colonne_recul_pas_m
	int m_iColumnBackMaxM = 1000;		// colonne_recul_max_m
	int m_iColumnSoldiersPerTruck = 10;		// colonne_soldats_camion
	int m_iColumnTrucksMax = 2;		// colonne_camions_max
	int m_iColumnRetryS = 10;		// colonne_essai_s
	int m_iColumnRoadStepM = 300;		// colonne_pas_route_m
	bool m_bRetreatOn = true;		// repli_actif
	int m_iRetreatSectionCent = 34;		// repli_section_centiemes
	int m_iRetreatGarrisonCent = 34;		// repli_garnison_centiemes
	int m_iRetreatContactMin = 5;		// repli_contact_min
	int m_iRetreatCoverS = 75;		// repli_couverture_s
	int m_iRetreatGrenades = 2;		// repli_grenades
	int m_iRetreatExitM = 250;		// repli_sortie_m
	int m_iRetreatNeighbourM = 3000;		// repli_voisine_max_m
	int m_iRetreatAngleMin = 90;		// repli_angle_min
	int m_iRetreatEraseM = 300;		// repli_effacement_m
	int m_iEmptiedM = 2000;		// repli_vide_distance_m
	int m_iRetreatCentreM = 40;		// repli_centre_rayon_m
	int m_iTwoAxesGroups = 3;		// assaut_deux_axes_groupes
	int m_iTwoAxesSoldiers = 12;		// assaut_deux_axes_soldats
	float m_fTwoAxesRatio = 2.0;		// assaut_deux_axes_rapport
	int m_iAxesAngle = 60;		// assaut_angle_axes
	int m_iEntrenchedM = 40;		// assaut_retranche_m
	int m_iEntrenchedMin = 5;		// assaut_retranche_min
	int m_iKnownRadiusM = 300;		// assaut_connus_rayon_m
	int m_iKnownMin = 10;		// assaut_connus_min
	int m_iFireMinM = 250;		// assaut_feu_min_m
	int m_iFireMaxM = 350;		// assaut_feu_max_m
	int m_iSuppressS = 90;		// assaut_suppression_s
	int m_iSuppressOpenS = 45;		// assaut_suppression_ouverture_s
	float m_fSuppressHeight = 1.5;		// assaut_suppression_hauteur
	int m_iSmokeDistanceM = 300;		// assaut_fumee_distance_m
	int m_iSmokeGrenades = 1;		// assaut_fumee_grenades
	int m_iSyncDistanceM = 400;		// assaut_synchro_distance_m
	int m_iSyncMaxS = 240;		// assaut_synchro_max_s
	bool m_bFinalAttack = true;		// assaut_attaque_finale
	bool m_bHarassOn = true;		// harcelement_actif
	int m_iHarassIntervalMin = 60;		// harcelement_intervalle_min
	int m_iHarassChance = 25;		// harcelement_chance
	int m_iHarassPlayersMin = 2;		// harcelement_joueurs_min
	int m_iHarassRangeM = 1500;		// harcelement_portee_m
	int m_iHarassKnownMin = 20;		// harcelement_connus_min
	int m_iHarassShells = 3;		// harcelement_obus
	int m_iHarassShellChance = 50;		// harcelement_part_obus
	int m_iHarassGroupsMax = 2;		// harcelement_groupes_max
	int m_iHarassTwoGroupsFrom = 6;		// harcelement_deux_groupes_des
	int m_iHarassFromMinM = 400;		// harcelement_depart_min_m
	int m_iHarassFromMaxM = 800;		// harcelement_depart_max_m
	int m_iHarassFightMin = 10;		// harcelement_combat_min
	int m_iHarassLossCent = 50;		// harcelement_pertes_retrait_centiemes
	int m_iHarassAfterCaMin = 20;		// harcelement_apres_ca_min
	int m_iFullHours = 6;		// reorg_complet_heures
	int m_iFullScoreBonus = 500;		// reorg_bonus_score
	int m_iPathM = 2500;		// reorg_chemin_distance_m
	int m_iPathAngle = 45;		// reorg_chemin_angle
	int m_iPathIntervalMin = 20;		// reorg_chemin_intervalle_min
	int m_iPathQuietMin = 30;		// reorg_chemin_calme_min
	int m_iPathPostsM = 1500;		// reorg_postes_distance_m
	int m_iTrackM = 150;		// reorg_releve_m
	int m_iTrackMin = 10;		// reorg_releve_min
	int m_iWindowMin = 20;		// ca_fenetre_min
	int m_iGapMin = 45;		// ca_ecart_min
	int m_iEarlyScore = 70;		// ca_seuil_avance
	int m_iEarlyScoreBold = 60;		// ca_seuil_audacieux
	int m_iOnTimeScore = 40;		// ca_seuil_heure
	int m_iPresenceM = 1000;		// ca_presence_rayon_m
	int m_iPresenceMin = 10;		// ca_presence_min
	int m_iTargetDepot = 40;		// ca_cible_depot
	int m_iTargetEmpty = 30;		// ca_cible_vide
	int m_iTargetCell = 2;		// ca_cible_carre
	int m_iTargetMin = 20;		// ca_cible_min
	int m_iWavesMin = 2;		// ca_vagues_min
	int m_iWavesMax = 4;		// ca_vagues_max
	int m_iWaveGoodCent = 67;		// ca_vague_bonne_centiemes
	int m_iWaveBrokenCent = 34;		// ca_vague_brisee_centiemes
	int m_iRoomMargin = 6;		// ca_marge_place
	int m_iMortarLeadS = 120;		// ca_mortier_avance_s
	int m_iHeavyLeadS = 60;		// ca_lourd_avance_s
	int m_iClusterPlayers = 4;		// ca_amas_joueurs
	int m_iClusterM = 70;		// ca_amas_rayon_m
	int m_iFeintChance = 25;		// ca_feinte
	int m_iFlybyChance = 50;		// ca_avion_chance
	int m_iAbandonCent = 67;		// ca_abandon_centiemes
	int m_iAbandonWaves = 2;		// ca_abandon_vagues
	int m_iPaperM = 1500;		// ca_papier_distance_m
	int m_iAwakeM = 900;		// ca_eveil_distance_m
	int m_iEraseM = 2000;		// ca_effacement_distance_m
	int m_iEraseMin = 3;		// ca_effacement_min
	float m_fFootSpeed = 1.3;		// ca_pied_vitesse
	int m_iStuckMin = 3;		// ca_bloque_min
	int m_iStuckPaperMin = 10;		// ca_bloque_papier_min
	int m_iPaperPlayerMinM = 1000;		// ca_papier_joueur_min_m

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected static const string FREE = "gratuit";							// coût d'une manœuvre (C1 : infanterie gratuite)
	protected static const int PENDING_MARGIN_S = 1200;						// camion demandé jamais posé : oublié après max(délai, 10 min) + 20 min
	protected SRP_Commander m_Commander;
	protected ref array<ref SRP_CmdColumn> m_aColumns = {};
	protected ref array<ref SRP_CmdSentTruck> m_aSentTrucks = {};			// camions hors colonne pas encore déchargés (RE13)
	protected ref array<string> m_aPendingFrom = {};						// camions hors colonne demandés, pas encore posés : source (C2, non sauvé)
	protected ref array<int> m_aPendingLoad = {};							// soldats prévus, même index
	protected ref array<int> m_aPendingUntil = {};							// oubliés au plus tard à (heure Unix), même index
	protected ref array<string> m_aGathered = {};							// localités regroupées au centre sans décrochage, une fois par pose (MA6)
	protected ref array<ref SRP_CmdPaperUnit> m_aPaper = {};
	protected ref array<ref SRP_CmdRetreat> m_aRetreats = {};
	protected ref array<ref SRP_CmdRaid> m_aRaids = {};
	protected ref array<ref SRP_CmdAssault> m_aAssaults = {};				// plans des vagues de la contre-attaque
	protected ref array<ref SRP_EnemyGroup> m_aContactGroups = {};			// débarqués des camions de contact
	protected ref array<int> m_aContactZones = {};
	protected ref array<int> m_aContactQuiet = {};							// sans contact depuis (heure Unix), même index
	protected ref array<ref SRP_EnemyGroup> m_aFarGroups = {};				// assaillants loin de tout joueur (CA6)
	protected ref array<int> m_aFarSince = {};								// depuis (heure Unix), même index
	protected ref array<ref SRP_EnemyGroup> m_aReordered = {};				// assaillants bloqués déjà relancés (CA6)
	protected ref array<IEntity> m_aPlayers = {};							// personnages des joueurs, relus à chaque passage
	protected ref map<int, int> m_mLastReinforce = new map<int, int>();		// zone -> heure Unix (C2, non sauvé)
	protected ref map<int, int> m_mRegionLastUnix = new map<int, int>();	// région -> dernier renfort (Staff)
	protected ref map<int, int> m_mRegionLastZone = new map<int, int>();	// région -> zone du dernier renfort (Staff)
	protected ref map<int, int> m_mPathUnix = new map<int, int>();			// région -> heure Unix (MA11)
	protected ref map<int, int> m_mPathCell = new map<int, int>();			// carré de poste -> heure Unix (MA11)
	protected ref map<int, ref SRP_CmdTrack> m_mZoneTrack = new map<int, ref SRP_CmdTrack>();
	protected ref map<int, ref SRP_CmdTrack> m_mRegionTrack = new map<int, ref SRP_CmdTrack>();
	protected int m_iLast10;												// sous-cadences (heure Unix)
	protected int m_iLast60;
	protected int m_iLast300;
	protected int m_iLastRetreatCheck;
	protected bool m_bCheckRetreatSoon;									// perte rapportée : contrôle du décrochage avancé
	protected int m_iLastHarassUnix;										// sauvé
	protected int m_iLastCaUnix;											// sauvé (fin de la dernière contre-attaque)
	protected int m_iCaCount;												// sauvé
	protected int m_iNextColumnId = 1;
	protected int m_iNextSentId = 1;
	protected bool m_bDirty;
	// Contre-attaque en cours (remis à zéro à chaque annonce : PlannedWaves, ou nouvelle échéance vue par Support)
	protected int m_iCaKey;													// fin d'annonce de l'attaque suivie
	protected int m_iCaWave1Posted;										// soldats de la 1re vague (réels et papier)
	protected int m_iCaPlannedWave;										// dernière vague remise par OnWavePosted
	protected int m_iCaSeenWave;											// dernière vague vue partir (DriveAssault)
	protected int m_iCaSeenWaveUnix;
	protected bool m_bFeintRolled;
	protected bool m_bFeintFired;
	protected bool m_bFlybyRolled;
	protected bool m_bCaSmokeDone;
	protected bool m_bCaEndLogged;

	//================================================================================================
	// Vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Constructeur seul — SRP_Commander (constructeur)
	void SRP_CmdManeuvers(SRP_Commander commander)
	{
		m_Commander = commander;
	}

	//------------------------------------------------------------------------------------------------
	//! Les manœuvres, null sans Commandeur — SRP_FrontEnemyComponent, SRP_TerritoryComponent, camions
	static SRP_CmdManeuvers Get()
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return null;
		return commander.GetManeuvers();
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés — SRP_Commander.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareBool("renfort_actif", m_bReinforceOn, "Renforts du Commandeur (MA1)");
		SRP_CmdSettings.DeclareInt("renfort_frein_min", m_iBrakeMin, "Un renfort par zone au plus toutes les ce nombre de minutes, camion d'entrée compris (C2)");
		SRP_CmdSettings.DeclareInt("renfort_source_garde_centiemes", m_iSourceKeepCent, "La localité qui envoie garde toujours ce nombre de centièmes de son effectif nominal (C2)");
		SRP_CmdSettings.DeclareInt("renfort_source_garde_prudent_centiemes", m_iSourceKeepPrudentCent, "Avec un officier prudent (CO7)");
		SRP_CmdSettings.DeclareInt("renfort_source_distance_m", m_iSourceM, "Localité source à cette distance au plus, en mètres (MA2)");
		SRP_CmdSettings.DeclareInt("renfort_source_distance_poche_m", m_iSourcePocketM, "Pour une poche, en mètres (MA12)");
		SRP_CmdSettings.DeclareInt("renfort_arrivee_min_s", m_iArriveMinS, "Arrivée d'un renfort au plus tôt, en secondes (MA2)");
		SRP_CmdSettings.DeclareInt("renfort_arrivee_max_s", m_iArriveMaxS, "Arrivée d'un renfort au plus tard, en secondes (MA2)");
		SRP_CmdSettings.DeclareFloat("renfort_facteur_desorg", m_fDesorgFactor, "Région désorganisée : délais des renforts multipliés par ce nombre (OF6)");
		SRP_CmdSettings.DeclareInt("renfort_chargement_min", m_iLoadMin, "Sous ce nombre de soldats, pas de camion (C2)");
		SRP_CmdSettings.DeclareInt("renfort_chargement_max", m_iLoadMax, "Soldats au plus par camion de renfort (MA3)");
		SRP_CmdSettings.DeclareInt("renfort_retrait_min", m_iContactTruckRetireMin, "Débarqués d'un camion de contact sans contact : retrait hors de vue après ce nombre de minutes (MA1)");
		SRP_CmdSettings.DeclareBool("renfort_rendre_survivants", m_bReturnSurvivors, "Les soldats d'un renfort qui rentrent vivants sont rendus à leur localité (MA2, lu par le retrait des groupes)");
		SRP_CmdSettings.DeclareInt("colonne_distance_m", m_iColumnRealM, "Une colonne devient réelle à cette distance d'un joueur, en mètres (MA4)");
		SRP_CmdSettings.DeclareFloat("colonne_vitesse", m_fColumnSpeed, "Vitesse d'une colonne sur le papier, en mètres par seconde (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_joueur_min_m", m_iColumnPlayerMinM, "Jamais posée plus près d'un joueur, en mètres (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_recul_pas_m", m_iColumnBackStepM, "Recul sur la route pour la poser hors de vue, par pas de ce nombre de mètres (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_recul_max_m", m_iColumnBackMaxM, "Recul au plus, en mètres (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_soldats_camion", m_iColumnSoldiersPerTruck, "Soldats par camion d'une colonne (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_camions_max", m_iColumnTrucksMax, "Camions au plus par colonne (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_essai_s", m_iColumnRetryS, "Nouvel essai de pose après ce nombre de secondes (MA4)");
		SRP_CmdSettings.DeclareInt("colonne_pas_route_m", m_iColumnRoadStepM, "Pas de la route d'une colonne, en mètres (MA4)");
		SRP_CmdSettings.DeclareBool("repli_actif", m_bRetreatOn, "Décrochage des garnisons (#40) (MA5)");
		SRP_CmdSettings.DeclareInt("repli_section_centiemes", m_iRetreatSectionCent, "Une section se replie au centre à ce nombre de centièmes de son effectif (MA5)");
		SRP_CmdSettings.DeclareInt("repli_garnison_centiemes", m_iRetreatGarrisonCent, "La garnison décroche à ce nombre de centièmes de son effectif (MA5)");
		SRP_CmdSettings.DeclareInt("repli_contact_min", m_iRetreatContactMin, "Seulement au contact depuis moins de ce nombre de minutes (MA5)");
		SRP_CmdSettings.DeclareInt("repli_couverture_s", m_iRetreatCoverS, "Le binôme couvre pendant ce nombre de secondes (MA6)");
		SRP_CmdSettings.DeclareInt("repli_grenades", m_iRetreatGrenades, "Fumigènes lancés par les groupes qui décrochent (MA6)");
		SRP_CmdSettings.DeclareInt("repli_sortie_m", m_iRetreatExitM, "Point de sortie au-delà du rayon de la localité, en mètres (MA6)");
		SRP_CmdSettings.DeclareInt("repli_voisine_max_m", m_iRetreatNeighbourM, "Localité de repli à cette distance au plus, en mètres (MA6)");
		SRP_CmdSettings.DeclareInt("repli_angle_min", m_iRetreatAngleMin, "Localité de repli à cet angle au moins de la direction du contact, en degrés (MA6)");
		SRP_CmdSettings.DeclareInt("repli_effacement_m", m_iRetreatEraseM, "Groupes en repli effacés hors de vue à cette distance des joueurs, en mètres (MA6)");
		SRP_CmdSettings.DeclareInt("repli_vide_distance_m", m_iEmptiedM, "Localité quittée vide tant qu'un joueur est à cette distance, en mètres (MA7)");
		SRP_CmdSettings.DeclareInt("repli_centre_rayon_m", m_iRetreatCentreM, "Rayon de défense au centre après un repli, en mètres (MA5)");
		SRP_CmdSettings.DeclareInt("assaut_deux_axes_groupes", m_iTwoAxesGroups, "Assaut sur deux axes à partir de ce nombre d'éléments (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_deux_axes_soldats", m_iTwoAxesSoldiers, "Et de ce nombre de soldats (MA8)");
		SRP_CmdSettings.DeclareFloat("assaut_deux_axes_rapport", m_fTwoAxesRatio, "Et de ce rapport aux joueurs connus (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_angle_axes", m_iAxesAngle, "Écart minimal entre les deux axes, en degrés (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_retranche_m", m_iEntrenchedM, "Joueurs retranchés : restés à cette distance, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_retranche_min", m_iEntrenchedMin, "Pendant ce nombre de minutes (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_connus_rayon_m", m_iKnownRadiusM, "Joueurs connus comptés à cette distance de l'objectif, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_connus_min", m_iKnownMin, "Vus depuis ce nombre de minutes au plus (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_feu_min_m", m_iFireMinM, "Base de feu à cette distance au moins de l'objectif, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_feu_max_m", m_iFireMaxM, "Base de feu à cette distance au plus, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_suppression_s", m_iSuppressS, "Tir de suppression de la base de feu, en secondes (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_suppression_ouverture_s", m_iSuppressOpenS, "Tir d'ouverture, en secondes (MA8)");
		SRP_CmdSettings.DeclareFloat("assaut_suppression_hauteur", m_fSuppressHeight, "Hauteur visée par la suppression, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_fumee_distance_m", m_iSmokeDistanceM, "Fumigène lancé par la manœuvre à cette distance de l'objectif, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_fumee_grenades", m_iSmokeGrenades, "Fumigènes de la manœuvre (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_synchro_distance_m", m_iSyncDistanceM, "Deux axes : point d'attente à cette distance de l'objectif, en mètres (MA8)");
		SRP_CmdSettings.DeclareInt("assaut_synchro_max_s", m_iSyncMaxS, "Deux axes : attente au plus, en secondes (MA8)");
		SRP_CmdSettings.DeclareBool("assaut_attaque_finale", m_bFinalAttack, "Attaque directe du dernier point clé (MA8)");
		SRP_CmdSettings.DeclareBool("harcelement_actif", m_bHarassOn, "Harcèlement du front entre deux contre-attaques (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_intervalle_min", m_iHarassIntervalMin, "Au plus un harcèlement toutes les ce nombre de minutes (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_chance", m_iHarassChance, "Chances sur 100 à chaque essai de 5 min (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_joueurs_min", m_iHarassPlayersMin, "Joueurs connectés au moins (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_portee_m", m_iHarassRangeM, "Cible : carré bleu du front à cette distance d'un contact connu, en mètres (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_connus_min", m_iHarassKnownMin, "Contact connu depuis ce nombre de minutes au plus (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_obus", m_iHarassShells, "Obus d'un harcèlement au mortier (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_part_obus", m_iHarassShellChance, "Chances sur 100 d'un harcèlement au mortier plutôt qu'un raid (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_groupes_max", m_iHarassGroupsMax, "Éléments d'un raid au plus (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_deux_groupes_des", m_iHarassTwoGroupsFrom, "Deux éléments à partir de ce nombre de joueurs connus à 3 km (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_depart_min_m", m_iHarassFromMinM, "Départ du raid à cette distance au moins de la cible, en mètres (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_depart_max_m", m_iHarassFromMaxM, "Départ du raid à cette distance au plus, en mètres (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_combat_min", m_iHarassFightMin, "Le raid rentre après ce nombre de minutes de combat (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_pertes_retrait_centiemes", m_iHarassLossCent, "Ou à ce nombre de centièmes de pertes (MA10)");
		SRP_CmdSettings.DeclareInt("harcelement_apres_ca_min", m_iHarassAfterCaMin, "Pas de harcèlement avant ce nombre de minutes après une contre-attaque (MA10)");
		SRP_CmdSettings.DeclareInt("reorg_complet_heures", m_iFullHours, "Localité suivante tenue au complet pendant ce nombre d'heures (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_bonus_score", m_iFullScoreBonus, "Priorité de garnison d'une localité tenue au complet (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_chemin_distance_m", m_iPathM, "Chemin des joueurs : localités à cette distance, en mètres (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_chemin_angle", m_iPathAngle, "Dans ce cône, en degrés de part et d'autre (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_chemin_intervalle_min", m_iPathIntervalMin, "Une fois par région toutes les ce nombre de minutes (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_chemin_calme_min", m_iPathQuietMin, "Localités sans contact depuis ce nombre de minutes (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_postes_distance_m", m_iPathPostsM, "Postes du front sur le chemin à cette distance, en mètres (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_releve_m", m_iTrackM, "Deux relevés de contact distants d'au moins ce nombre de mètres (MA11)");
		SRP_CmdSettings.DeclareInt("reorg_releve_min", m_iTrackMin, "À ce nombre de minutes près (MA11)");
		SRP_CmdSettings.DeclareInt("ca_fenetre_min", m_iWindowMin, "La contre-attaque part jusqu'à ce nombre de minutes avant ou après l'échéance (CA1)");
		SRP_CmdSettings.DeclareInt("ca_ecart_min", m_iGapMin, "Jamais moins de ce nombre de minutes entre deux contre-attaques (CA1)");
		SRP_CmdSettings.DeclareInt("ca_seuil_avance", m_iEarlyScore, "Note pour partir en avance (CA1)");
		SRP_CmdSettings.DeclareInt("ca_seuil_audacieux", m_iEarlyScoreBold, "Note pour partir en avance avec un officier audacieux (CA1)");
		SRP_CmdSettings.DeclareInt("ca_seuil_heure", m_iOnTimeScore, "Note pour partir après l'échéance (CA1)");
		SRP_CmdSettings.DeclareInt("ca_presence_rayon_m", m_iPresenceM, "Joueurs connus comptés à cette distance de la zone, en mètres (CA1)");
		SRP_CmdSettings.DeclareInt("ca_presence_min", m_iPresenceMin, "Vus depuis ce nombre de minutes au plus (CA1)");
		SRP_CmdSettings.DeclareInt("ca_cible_depot", m_iTargetDepot, "Cible : points pour une zone avec un dépôt (CA2)");
		SRP_CmdSettings.DeclareInt("ca_cible_vide", m_iTargetEmpty, "Cible : points pour une zone sans joueur connu (CA2)");
		SRP_CmdSettings.DeclareInt("ca_cible_carre", m_iTargetCell, "Cible : points par carré bleu au contact (CA2)");
		SRP_CmdSettings.DeclareInt("ca_cible_min", m_iTargetMin, "Cible : joueurs connus depuis ce nombre de minutes (CA2)");
		SRP_CmdSettings.DeclareInt("ca_vagues_min", m_iWavesMin, "Vagues au moins (CA3)");
		SRP_CmdSettings.DeclareInt("ca_vagues_max", m_iWavesMax, "Vagues au plus (CA3)");
		SRP_CmdSettings.DeclareInt("ca_vague_bonne_centiemes", m_iWaveGoodCent, "1re vague réussie à ce nombre de centièmes d'effectif encore en état : une vague de plus (CA3)");
		SRP_CmdSettings.DeclareInt("ca_vague_brisee_centiemes", m_iWaveBrokenCent, "1re vague brisée sous ce nombre de centièmes : une vague de moins (CA3)");
		SRP_CmdSettings.DeclareInt("ca_marge_place", m_iRoomMargin, "Places gardées sous la place de combat pour une vague (CA3)");
		SRP_CmdSettings.DeclareInt("ca_mortier_avance_s", m_iMortarLeadS, "Mortier ce nombre de secondes avant l'assaut (CA4)");
		SRP_CmdSettings.DeclareInt("ca_lourd_avance_s", m_iHeavyLeadS, "Artillerie ou blindé ce nombre de secondes avant l'assaut (CA4)");
		SRP_CmdSettings.DeclareInt("ca_amas_joueurs", m_iClusterPlayers, "Artillerie plutôt que blindé sur un amas d'au moins ce nombre de joueurs connus (CA4)");
		SRP_CmdSettings.DeclareInt("ca_amas_rayon_m", m_iClusterM, "Regroupés dans ce rayon, en mètres (CA4)");
		SRP_CmdSettings.DeclareInt("ca_feinte", m_iFeintChance, "Chances sur 100 d'une feinte sur un faux axe (contre-attaque à deux axes) (MO8)");
		SRP_CmdSettings.DeclareInt("ca_avion_chance", m_iFlybyChance, "Chances sur 100 d'un passage de Su-57 pendant une contre-attaque (CA4)");
		SRP_CmdSettings.DeclareInt("ca_abandon_centiemes", m_iAbandonCent, "Attaque brisée (défense réussie) à ce nombre de centièmes de pertes (C8)");
		SRP_CmdSettings.DeclareInt("ca_abandon_vagues", m_iAbandonWaves, "Après ce nombre de vagues posées au moins (ou toutes) (CA5)");
		SRP_CmdSettings.DeclareInt("ca_papier_distance_m", m_iPaperM, "Vague sur le papier si aucun joueur n'est à cette distance, en mètres (CA6)");
		SRP_CmdSettings.DeclareInt("ca_eveil_distance_m", m_iAwakeM, "Groupes d'assaut tenus éveillés au-delà de cette distance des joueurs, en mètres (CA6)");
		SRP_CmdSettings.DeclareInt("ca_effacement_distance_m", m_iEraseM, "Groupe d'assaut passé sur le papier au-delà de cette distance, en mètres (CA6)");
		SRP_CmdSettings.DeclareInt("ca_effacement_min", m_iEraseMin, "Depuis ce nombre de minutes (CA6)");
		SRP_CmdSettings.DeclareFloat("ca_pied_vitesse", m_fFootSpeed, "Vitesse d'une unité sur le papier, en mètres par seconde (CA6)");
		SRP_CmdSettings.DeclareInt("ca_bloque_min", m_iStuckMin, "Groupe bloqué après ce nombre de minutes sans avancer (CA6)");
		SRP_CmdSettings.DeclareInt("ca_bloque_papier_min", m_iStuckPaperMin, "Groupe bloqué passé sur le papier après ce nombre de minutes, hors de vue (CA6)");
		SRP_CmdSettings.DeclareInt("ca_papier_joueur_min_m", m_iPaperPlayerMinM, "Une unité sur le papier ne reprend un carré que sans joueur à cette distance, en mètres (CA6)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés — SRP_Commander.LoadSettings
	void LoadSettings()
	{
		m_bReinforceOn = SRP_CmdSettings.GetBool("renfort_actif");
		m_iBrakeMin = SRP_CmdSettings.GetIntClamped("renfort_frein_min", 0, 120);
		m_iSourceKeepCent = SRP_CmdSettings.GetIntClamped("renfort_source_garde_centiemes", 0, 100);
		m_iSourceKeepPrudentCent = SRP_CmdSettings.GetIntClamped("renfort_source_garde_prudent_centiemes", 0, 100);
		m_iSourceM = SRP_CmdSettings.GetIntClamped("renfort_source_distance_m", 500, 20000);
		m_iSourcePocketM = SRP_CmdSettings.GetIntClamped("renfort_source_distance_poche_m", 500, 20000);
		m_iArriveMinS = SRP_CmdSettings.GetIntClamped("renfort_arrivee_min_s", 10, 3600);
		m_iArriveMaxS = SRP_CmdSettings.GetIntClamped("renfort_arrivee_max_s", 10, 3600);
		m_fDesorgFactor = SRP_CmdSettings.GetFloatClamped("renfort_facteur_desorg", 1.0, 10.0);
		m_iLoadMin = SRP_CmdSettings.GetIntClamped("renfort_chargement_min", 1, 20);
		m_iLoadMax = SRP_CmdSettings.GetIntClamped("renfort_chargement_max", 1, 30);
		m_iContactTruckRetireMin = SRP_CmdSettings.GetIntClamped("renfort_retrait_min", 1, 240);
		m_bReturnSurvivors = SRP_CmdSettings.GetBool("renfort_rendre_survivants");
		m_iColumnRealM = SRP_CmdSettings.GetIntClamped("colonne_distance_m", 100, 5000);
		m_fColumnSpeed = SRP_CmdSettings.GetFloatClamped("colonne_vitesse", 0.5, 30.0);
		m_iColumnPlayerMinM = SRP_CmdSettings.GetIntClamped("colonne_joueur_min_m", 0, 5000);
		m_iColumnBackStepM = SRP_CmdSettings.GetIntClamped("colonne_recul_pas_m", 10, 1000);
		m_iColumnBackMaxM = SRP_CmdSettings.GetIntClamped("colonne_recul_max_m", 0, 5000);
		m_iColumnSoldiersPerTruck = SRP_CmdSettings.GetIntClamped("colonne_soldats_camion", 2, 20);
		m_iColumnTrucksMax = SRP_CmdSettings.GetIntClamped("colonne_camions_max", 1, 6);
		m_iColumnRetryS = SRP_CmdSettings.GetIntClamped("colonne_essai_s", 5, 600);
		m_iColumnRoadStepM = SRP_CmdSettings.GetIntClamped("colonne_pas_route_m", 50, 2000);
		m_bRetreatOn = SRP_CmdSettings.GetBool("repli_actif");
		m_iRetreatSectionCent = SRP_CmdSettings.GetIntClamped("repli_section_centiemes", 0, 100);
		m_iRetreatGarrisonCent = SRP_CmdSettings.GetIntClamped("repli_garnison_centiemes", 0, 100);
		m_iRetreatContactMin = SRP_CmdSettings.GetIntClamped("repli_contact_min", 1, 60);
		m_iRetreatCoverS = SRP_CmdSettings.GetIntClamped("repli_couverture_s", 0, 600);
		m_iRetreatGrenades = SRP_CmdSettings.GetIntClamped("repli_grenades", 0, 6);
		m_iRetreatExitM = SRP_CmdSettings.GetIntClamped("repli_sortie_m", 50, 2000);
		m_iRetreatNeighbourM = SRP_CmdSettings.GetIntClamped("repli_voisine_max_m", 500, 20000);
		m_iRetreatAngleMin = SRP_CmdSettings.GetIntClamped("repli_angle_min", 0, 180);
		m_iRetreatEraseM = SRP_CmdSettings.GetIntClamped("repli_effacement_m", 50, 5000);
		m_iEmptiedM = SRP_CmdSettings.GetIntClamped("repli_vide_distance_m", 100, 10000);
		m_iRetreatCentreM = SRP_CmdSettings.GetIntClamped("repli_centre_rayon_m", 5, 300);
		m_iTwoAxesGroups = SRP_CmdSettings.GetIntClamped("assaut_deux_axes_groupes", 1, 20);
		m_iTwoAxesSoldiers = SRP_CmdSettings.GetIntClamped("assaut_deux_axes_soldats", 1, 200);
		m_fTwoAxesRatio = SRP_CmdSettings.GetFloatClamped("assaut_deux_axes_rapport", 0.5, 10.0);
		m_iAxesAngle = SRP_CmdSettings.GetIntClamped("assaut_angle_axes", 0, 180);
		m_iEntrenchedM = SRP_CmdSettings.GetIntClamped("assaut_retranche_m", 5, 500);
		m_iEntrenchedMin = SRP_CmdSettings.GetIntClamped("assaut_retranche_min", 1, 60);
		m_iKnownRadiusM = SRP_CmdSettings.GetIntClamped("assaut_connus_rayon_m", 50, 3000);
		m_iKnownMin = SRP_CmdSettings.GetIntClamped("assaut_connus_min", 1, 60);
		m_iFireMinM = SRP_CmdSettings.GetIntClamped("assaut_feu_min_m", 50, 2000);
		m_iFireMaxM = SRP_CmdSettings.GetIntClamped("assaut_feu_max_m", 50, 2000);
		m_iSuppressS = SRP_CmdSettings.GetIntClamped("assaut_suppression_s", 5, 600);
		m_iSuppressOpenS = SRP_CmdSettings.GetIntClamped("assaut_suppression_ouverture_s", 5, 600);
		m_fSuppressHeight = SRP_CmdSettings.GetFloatClamped("assaut_suppression_hauteur", 0.0, 5.0);
		m_iSmokeDistanceM = SRP_CmdSettings.GetIntClamped("assaut_fumee_distance_m", 50, 2000);
		m_iSmokeGrenades = SRP_CmdSettings.GetIntClamped("assaut_fumee_grenades", 0, 6);
		m_iSyncDistanceM = SRP_CmdSettings.GetIntClamped("assaut_synchro_distance_m", 50, 3000);
		m_iSyncMaxS = SRP_CmdSettings.GetIntClamped("assaut_synchro_max_s", 10, 1800);
		m_bFinalAttack = SRP_CmdSettings.GetBool("assaut_attaque_finale");
		m_bHarassOn = SRP_CmdSettings.GetBool("harcelement_actif");
		m_iHarassIntervalMin = SRP_CmdSettings.GetIntClamped("harcelement_intervalle_min", 5, 1440);
		m_iHarassChance = SRP_CmdSettings.GetIntClamped("harcelement_chance", 0, 100);
		m_iHarassPlayersMin = SRP_CmdSettings.GetIntClamped("harcelement_joueurs_min", 1, 50);
		m_iHarassRangeM = SRP_CmdSettings.GetIntClamped("harcelement_portee_m", 100, 10000);
		m_iHarassKnownMin = SRP_CmdSettings.GetIntClamped("harcelement_connus_min", 1, 120);
		m_iHarassShells = SRP_CmdSettings.GetIntClamped("harcelement_obus", 1, 20);
		m_iHarassShellChance = SRP_CmdSettings.GetIntClamped("harcelement_part_obus", 0, 100);
		m_iHarassGroupsMax = SRP_CmdSettings.GetIntClamped("harcelement_groupes_max", 1, 6);
		m_iHarassTwoGroupsFrom = SRP_CmdSettings.GetIntClamped("harcelement_deux_groupes_des", 1, 50);
		m_iHarassFromMinM = SRP_CmdSettings.GetIntClamped("harcelement_depart_min_m", 100, 5000);
		m_iHarassFromMaxM = SRP_CmdSettings.GetIntClamped("harcelement_depart_max_m", 100, 5000);
		m_iHarassFightMin = SRP_CmdSettings.GetIntClamped("harcelement_combat_min", 1, 120);
		m_iHarassLossCent = SRP_CmdSettings.GetIntClamped("harcelement_pertes_retrait_centiemes", 0, 100);
		m_iHarassAfterCaMin = SRP_CmdSettings.GetIntClamped("harcelement_apres_ca_min", 0, 240);
		m_iFullHours = SRP_CmdSettings.GetIntClamped("reorg_complet_heures", 0, 72);
		m_iFullScoreBonus = SRP_CmdSettings.GetIntClamped("reorg_bonus_score", 0, 10000);
		m_iPathM = SRP_CmdSettings.GetIntClamped("reorg_chemin_distance_m", 100, 20000);
		m_iPathAngle = SRP_CmdSettings.GetIntClamped("reorg_chemin_angle", 0, 180);
		m_iPathIntervalMin = SRP_CmdSettings.GetIntClamped("reorg_chemin_intervalle_min", 1, 240);
		m_iPathQuietMin = SRP_CmdSettings.GetIntClamped("reorg_chemin_calme_min", 0, 240);
		m_iPathPostsM = SRP_CmdSettings.GetIntClamped("reorg_postes_distance_m", 100, 10000);
		m_iTrackM = SRP_CmdSettings.GetIntClamped("reorg_releve_m", 10, 5000);
		m_iTrackMin = SRP_CmdSettings.GetIntClamped("reorg_releve_min", 1, 120);
		m_iWindowMin = SRP_CmdSettings.GetIntClamped("ca_fenetre_min", 0, 120);
		m_iGapMin = SRP_CmdSettings.GetIntClamped("ca_ecart_min", 0, 600);
		m_iEarlyScore = SRP_CmdSettings.GetIntClamped("ca_seuil_avance", 0, 100);
		m_iEarlyScoreBold = SRP_CmdSettings.GetIntClamped("ca_seuil_audacieux", 0, 100);
		m_iOnTimeScore = SRP_CmdSettings.GetIntClamped("ca_seuil_heure", 0, 100);
		m_iPresenceM = SRP_CmdSettings.GetIntClamped("ca_presence_rayon_m", 100, 5000);
		m_iPresenceMin = SRP_CmdSettings.GetIntClamped("ca_presence_min", 1, 120);
		m_iTargetDepot = SRP_CmdSettings.GetIntClamped("ca_cible_depot", 0, 100);
		m_iTargetEmpty = SRP_CmdSettings.GetIntClamped("ca_cible_vide", 0, 100);
		m_iTargetCell = SRP_CmdSettings.GetIntClamped("ca_cible_carre", 0, 100);
		m_iTargetMin = SRP_CmdSettings.GetIntClamped("ca_cible_min", 1, 240);
		m_iWavesMin = SRP_CmdSettings.GetIntClamped("ca_vagues_min", 1, 10);
		m_iWavesMax = SRP_CmdSettings.GetIntClamped("ca_vagues_max", 1, 10);
		m_iWaveGoodCent = SRP_CmdSettings.GetIntClamped("ca_vague_bonne_centiemes", 0, 100);
		m_iWaveBrokenCent = SRP_CmdSettings.GetIntClamped("ca_vague_brisee_centiemes", 0, 100);
		m_iRoomMargin = SRP_CmdSettings.GetIntClamped("ca_marge_place", 0, 60);
		m_iMortarLeadS = SRP_CmdSettings.GetIntClamped("ca_mortier_avance_s", 0, 900);
		m_iHeavyLeadS = SRP_CmdSettings.GetIntClamped("ca_lourd_avance_s", 0, 900);
		m_iClusterPlayers = SRP_CmdSettings.GetIntClamped("ca_amas_joueurs", 1, 50);
		m_iClusterM = SRP_CmdSettings.GetIntClamped("ca_amas_rayon_m", 10, 500);
		m_iFeintChance = SRP_CmdSettings.GetIntClamped("ca_feinte", 0, 100);
		m_iFlybyChance = SRP_CmdSettings.GetIntClamped("ca_avion_chance", 0, 100);
		m_iAbandonCent = SRP_CmdSettings.GetIntClamped("ca_abandon_centiemes", 0, 100);
		m_iAbandonWaves = SRP_CmdSettings.GetIntClamped("ca_abandon_vagues", 1, 10);
		m_iPaperM = SRP_CmdSettings.GetIntClamped("ca_papier_distance_m", 100, 5000);
		m_iAwakeM = SRP_CmdSettings.GetIntClamped("ca_eveil_distance_m", 100, 5000);
		m_iEraseM = SRP_CmdSettings.GetIntClamped("ca_effacement_distance_m", 100, 10000);
		m_iEraseMin = SRP_CmdSettings.GetIntClamped("ca_effacement_min", 1, 60);
		m_fFootSpeed = SRP_CmdSettings.GetFloatClamped("ca_pied_vitesse", 0.1, 10.0);
		m_iStuckMin = SRP_CmdSettings.GetIntClamped("ca_bloque_min", 1, 60);
		m_iStuckPaperMin = SRP_CmdSettings.GetIntClamped("ca_bloque_papier_min", 1, 120);
		m_iPaperPlayerMinM = SRP_CmdSettings.GetIntClamped("ca_papier_joueur_min_m", 0, 5000);
	}

	//------------------------------------------------------------------------------------------------
	//! Front prêt : sous-cadences initialisées — SRP_Commander.Start
	void Start(int nowUnix)
	{
		m_iLast10 = nowUnix;
		m_iLast60 = nowUnix;
		m_iLast300 = nowUnix;
		m_iLastRetreatCheck = nowUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s : Commandeur inactif (!m_Commander.GetHost().IsCommanderActive()) -> seulement finir proprement
	//! (FinishOnly : replis, raids qui rentrent, colonnes réelles ; colonnes papier annulées, ReturnSent à leur source ;
	//! ce que le Staff a forcé continue, VU5) ;
	//! sinon ColumnsTick, PaperTick, RetreatsTick, RaidsTick, AwakeTick (10 s) ;
	//! 60 s : RetreatCheckTick, EmptiedTick, PathTick, ContactGroupsTick ; 300 s : HarassTick ; m_bDirty ->
	//! SRP_FrontComponent.MarkDirty(false) — SRP_Commander.Tick
	void Tick(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!m_Commander || !front || !front.IsReady())
			return;
		// Tous les joueurs, délai de grâce compris : rien ne doit apparaître ni disparaître sous leurs yeux
		FreshPlayers();
		SRP_FrontEnemyComponent host = m_Commander.GetHost();
		if (!host || !host.IsCommanderActive())
		{
			FinishOnly(nowUnix);
		}
		else
		{
			ColumnsTick(nowUnix);
			PaperTick(nowUnix);
			RetreatsTick(nowUnix);
			RaidsTick(nowUnix);
			if (nowUnix - m_iLast10 >= 10)
			{
				m_iLast10 = nowUnix;
				AwakeTick(nowUnix);
			}
			bool minute = nowUnix - m_iLast60 >= 60;
			// Une perte rapportée (MA5) avance le contrôle du décrochage, dix secondes au plus tôt
			if (minute || (m_bCheckRetreatSoon && nowUnix - m_iLastRetreatCheck >= 10))
			{
				m_bCheckRetreatSoon = false;
				m_iLastRetreatCheck = nowUnix;
				RetreatCheckTick(nowUnix);
			}
			if (minute)
			{
				m_iLast60 = nowUnix;
				EmptiedTick(nowUnix);
				PathTick(nowUnix);
				ContactGroupsTick(nowUnix);
			}
			if (nowUnix - m_iLast300 >= 300)
			{
				m_iLast300 = nowUnix;
				HarassTick(nowUnix);
			}
		}
		if (m_bDirty)
		{
			m_bDirty = false;
			front.MarkDirty(false);
		}
	}

	//================================================================================================
	// Renforts (MA1 à MA4, C2) et ordres du cerveau
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Frein C2 : aucun renfort vers la zone depuis renfort_frein_min minutes (camion d'entrée compris) — cerveau
	//! (avant d'émettre un ordre RENFORT), RequestEntryTruck
	bool CanReinforceZone(int zone)
	{
		return GetReinforceWaitSeconds(zone) <= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes avant qu'un renfort soit de nouveau possible vers la zone (0 = possible) — Staff
	int GetReinforceWaitSeconds(int zone)
	{
		int last;
		if (zone < 0 || !m_mLastReinforce.Find(zone, last))
			return 0;
		int wait = last + m_iBrakeMin * 60 - System.GetUnixTime();
		if (wait < 0)
			return 0;
		return wait;
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre RENFORT : frein C2 (sauf staff), source (PickSource), délai (x renfort_facteur_desorg si désorganisée),
	//! camion localité (DispatchFrom) ou camion de contact (SendContactTruck) si la route tient dans le délai, sinon
	//! colonne ; place par SRP_CmdCapacity (COMBAT) au moment de la pose des camions ; frein noté dès l'envoi ; rend ""
	//! ou la raison — SRP_Commander.IssueOrder
	string RequestReinforcement(int zone, vector aim, int size, string reason, string author, bool staff)
	{
		if (!m_bReinforceOn && !staff)
			return "renforts coupés (renfort_actif = 0)";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (!front || !front.IsReady() || !host || !trucks || !m_Commander)
			return "front ou camions pas encore prêts";
		int nowUnix = System.GetUnixTime();
		if (zone < 0)
			zone = front.GetZoneAt(aim);
		int region = RegionAt(aim);
		string where = ZoneText(zone);
		// VU5 : jamais sans la sécurité, Staff compris : rien vers la base (zone de la base, rayon interdit aux poses)
		string baseRefusal = BaseRefusal(aim, zone, staff);
		if (!baseRefusal.IsEmpty())
			return baseRefusal;

		// Le cerveau (IssueOrder) écrit la décision ou le refus rendu ici ; on n'ajoute que la ligne d'exécution
		// MA1 : un contact en territoire bleu (raid, contre-attaque) n'appelle rien ; le Staff passe outre
		if (!staff && !front.IsRedAt(aim) && !HostileLocalityNear(aim, 150))
			return "contact hors du territoire ennemi";
		// C2 : un renfort par zone toutes les renfort_frein_min minutes, camion d'entrée compris (le Staff passe outre)
		if (!staff && !CanReinforceZone(zone))
			return "frein : prochain renfort possible dans " + SRP_CmdScreens.DurationText(GetReinforceWaitSeconds(zone));

		// Cible : la localité tenue de la zone près du contact (camion « localité »), sinon le contact (camion d'assaut)
		SRP_SectorState target = PostedLocalityAt(aim, zone, 300);
		vector targetPos = aim;
		string targetName = "";
		if (target)
		{
			targetPos = target.m_Sector.m_vCenter;
			targetName = target.m_sName;
		}

		// Taille voulue par le cerveau (joueurs ESTIMÉS, CO5) ; 0 = au choix : base + par joueur connu à 1 km
		int wanted = size;
		if (wanted <= 0)
		{
			int known = m_Commander.KnownPlayersNear(aim, 1000, 600);
			if (known < 1)
				known = 1;
			wanted = m_Commander.m_iReinforceBase + m_Commander.m_iReinforcePerPlayer * known;
		}
		int maxLoad = m_iLoadMax;
		if (maxLoad < m_iLoadMin)
			maxLoad = m_iLoadMin;
		wanted = Math.ClampInt(wanted, m_iLoadMin, maxLoad);

		// MA2, MA12 : la localité voisine qui peut céder (la moitié de son nominal reste, C2)
		string source;
		vector sourcePos;
		int load;
		if (!PickSource(targetPos, zone, wanted, source, sourcePos, load))
			return "aucune localité voisine ne peut envoyer de soldats";

		// Délai visé (x2 en région désorganisée) ; route trop longue pour le tenir à 8 m/s : colonne (MA4)
		int delay = ReinforceDelay(region);
		array<vector> route = {};
		float length = RouteLength(sourcePos, targetPos, route);
		string how = "colonne sur le papier";
		if (length <= 8.0 * delay)
		{
			bool sent;
			if (target)
				sent = trucks.DispatchFrom(target, source, sourcePos, load, delay, "renfort");
			else
				sent = trucks.SendContactTruck(aim, source, sourcePos, load, delay, zone);
			if (!sent)
				return "camion refusé (place ou camions déjà en route)";
			// C2 : la source n'est débitée qu'à la pose ; d'ici là, ces soldats sont retenus (CanGive)
			NotePending(source, load, delay, nowUnix);
			how = "camion";
		}
		else
		{
			SRP_CmdColumn reinforceColumn = FindColumn(NewColumn(source, targetName, load, SRP_ECmdColumnKind.RENFORT, zone, aim, nowUnix));
			if (reinforceColumn)
				reinforceColumn.m_bStaff = staff;
		}

		// Frein noté dès l'envoi ; le débit de la source est fait au départ (OnTruckLaunched ou NewColumn)
		NoteReinforcement(zone, region, nowUnix);
		string destination = where;
		if (!targetName.IsEmpty())
			destination = targetName;
		int minutes = Math.Round(delay / 60.0);
		string text = string.Format("renfort de %1 soldats de %2 vers %3 (%4), arrivée prévue dans %5 min", load, source, destination, how, minutes);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, text + " — " + EffectiveText(source));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Camion d'entrée #69 (joueur à 500 m d'une localité), Commandeur actif : même frein, même source, même délai ;
	//! chargement d'aujourd'hui (LoadFor) ; vrai si un camion part (sinon la règle votée ne pose rien de plus) —
	//! SRP_TerritoryComponent
	bool RequestEntryTruck(SRP_SectorState state, int players, int nowUnix)
	{
		if (!m_bReinforceOn || !state || !state.m_Sector)
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (!front || !front.IsReady() || !trucks)
			return false;
		vector center = state.m_Sector.m_vCenter;
		int zone = front.GetZoneAt(center);
		int region = RegionAt(center);
		string what = "camion d'entrée vers " + state.m_sName;
		if (!CanReinforceZone(zone))
		{
			SRP_CmdLog.Refusal(region, what, string.Format("frein : un renfort par zone toutes les %1 min", m_iBrakeMin));
			return false;
		}
		int maxLoad = m_iLoadMax;
		if (maxLoad < m_iLoadMin)
			maxLoad = m_iLoadMin;
		int wanted = Math.ClampInt(trucks.LoadFor(players), m_iLoadMin, maxLoad);
		string source;
		vector sourcePos;
		int load;
		if (!PickSource(center, zone, wanted, source, sourcePos, load))
		{
			SRP_CmdLog.Refusal(region, what, "aucune localité voisine ne peut envoyer de soldats");
			return false;
		}
		int delay = ReinforceDelay(region);
		array<vector> route = {};
		float length = RouteLength(sourcePos, center, route);
		string how = "colonne sur le papier";
		if (length <= 8.0 * delay)
		{
			if (!trucks.DispatchFrom(state, source, sourcePos, load, delay, "entrée"))
			{
				SRP_CmdLog.Refusal(region, what, "camion refusé (place ou camions déjà en route)");
				return false;
			}
			NotePending(source, load, delay, nowUnix);
			how = "camion";
		}
		else
		{
			NewColumn(source, state.m_sName, load, SRP_ECmdColumnKind.RENFORT, zone, center, nowUnix);
		}
		NoteReinforcement(zone, region, nowUnix);
		int minutes = Math.Round(delay / 60.0);
		string text = string.Format("entrée d'un joueur vers %1 : %2 soldats de %3 (%4), arrivée prévue dans %5 min", state.m_sName, load, source, how, minutes);
		SRP_CmdLog.Decision(region, false, text, "camion d'entrée (#69)", FREE, EffectiveText(source));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre ENQUETE (contact « informateur seul », C4) : l'unité mobile la plus proche va fouiller (fouille de
	//! l'entraide #69, SearchAndDestroy puis retour à son poste) ; jamais d'appui — SRP_Commander.IssueOrder
	string RequestSearch(vector aim, int zone, string reason, bool staff)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return "ennemis absents";
		// VU5 : jamais sans la sécurité, Staff compris : aucune fouille dans la base
		int searchZone = zone;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (searchZone < 0 && front && front.IsReady())
			searchZone = front.GetZoneAt(aim);
		string baseRefusal = BaseRefusal(aim, searchZone, staff);
		if (!baseRefusal.IsEmpty())
			return baseRefusal;
		int tick = System.GetTickCount();
		SRP_EnemyGroup best = null;
		float bestDistance = 2000;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!IsMobile(record, tick))
				continue;
			float distance = vector.DistanceXZ(enemies.GroupPosition(record), aim);
			if (distance < bestDistance)
			{
				best = record;
				bestDistance = distance;
			}
		}
		if (!best)
			return "aucune unité mobile à 2 km";
		// Fouille de 10 min au plus (OrderSweep : SearchAndDestroy sur le point, puis retour à son poste) ; la décision
		// est écrite par le cerveau (IssueOrder)
		enemies.OrderSweep(best, aim, tick + 10 * 60 * 1000);
		int meters = Math.Round(bestDistance);
		SRP_CmdLog.Note(RegionAt(aim), SRP_ECmdLogKind.DECISION, string.Format("fouille vers %1 : unité à %2 m envoyée", SRP_CmdScreens.CellText(aim), meters));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre COLONNE (Staff, par ForceOrder seul, trou 15) : colonne de la localité ennemie la plus proche d'aim vers la
	//! suivante (papier, réelle à colonne_distance_m) — SRP_Commander.IssueOrder
	string RequestColumn(vector aim, string reason, string author, bool staff)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host)
			return "front absent";
		SRP_SectorState fromState = NearestLocality(aim, 5000, null, false);
		if (!fromState)
			return "aucune localité ennemie à 5 km";
		string fromName = fromState.m_sName;
		vector fromPos = fromState.m_Sector.m_vCenter;
		// Une localité tenue sous les yeux des joueurs ne se vide pas en silence : c'est le décrochage
		if (host.IsGarrisonPosted(fromName))
			return fromName + " : défenseurs en place, utiliser le décrochage";
		SRP_SectorState toState = NearestLocality(fromPos, 8000, fromState, false);
		if (!toState)
			return "aucune autre localité ennemie à 8 km de " + fromName;
		int give = CanGive(fromName, fromPos);
		if (give < m_iLoadMin)
			return string.Format("%1 ne peut rien céder (%2 soldats possibles)", fromName, give);
		SRP_CmdColumn staffColumn = FindColumn(NewColumn(fromName, toState.m_sName, give, SRP_ECmdColumnKind.STAFF, -1, toState.m_Sector.m_vCenter, System.GetUnixTime()));
		if (staffColumn)
			staffColumn.m_bStaff = staff;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre DECROCHAGE : StartRetreat de la garnison posée la plus proche d'aim à 3 km — SRP_Commander.IssueOrder
	string RequestRetreat(vector aim, string reason, string author, bool staff)
	{
		if (!m_bRetreatOn && !staff)
			return "décrochages coupés (repli_actif = 0)";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady() || !m_Commander)
			return "front pas encore prêt";
		SRP_SectorState state = NearestLocality(aim, 3000, null, true);
		if (!state)
			return "aucune localité tenue à 3 km";
		if (state.m_bRetreatDone)
			return state.m_sName + " : un décrochage a déjà eu lieu depuis la pose";
		vector center = state.m_Sector.m_vCenter;
		vector contact;
		int contactUnix;
		if (!m_Commander.LastKnownContact(front.GetZoneAt(center), 1800, contact, contactUnix))
			contact = aim;
		if (StartRetreat(state, contact, System.GetUnixTime()))
		{
			// VU5 : un décrochage forcé continue même Commandeur gelé (FinishOnly), sa colonne aussi
			SRP_CmdRetreat started = m_aRetreats[m_aRetreats.Count() - 1];
			if (started)
			{
				started.m_bStaff = staff;
				SRP_CmdColumn retreatColumn = FindColumn(started.m_iColumnId);
				if (retreatColumn)
					retreatColumn.m_bStaff = staff;
			}
			return "";
		}
		return state.m_sName + " : décrochage impossible (aucune localité de repli à portée, ou personne en état de partir)";
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre HARCELEMENT (MA10) sur le carré bleu du front le plus proche d'aim : quelques obus (SRP_CmdSupport) ou raid
	//! de 1 à 2 groupes (PATROUILLE) — SRP_Commander.IssueOrder, HarassTick
	string RequestHarass(vector aim, string reason, string author, bool staff)
	{
		if (!m_bHarassOn && !staff)
			return "harcèlement coupé (harcelement_actif = 0)";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !front.IsReady() || !enemies || !m_Commander)
			return "front ou ennemis pas encore prêts";
		array<IEntity> players = FreshPlayers();
		float range = m_iHarassRangeM;
		if (staff)
			range = 3000;
		int cell = NearestBlueFrontCell(aim, range);
		if (cell < 0)
			return "aucun carré à nous au front à portée";
		vector target = front.CellCenter(cell);
		int zone = front.GetCellZone(cell);
		int region = RegionAt(target);
		string cellText = front.CellRef(cell);
		string where = ZoneText(zone);
		string why = ReasonText(reason, author, staff);
		int nowUnix = System.GetUnixTime();

		// Quelques obus, une fois sur deux, si un joueur y a été VU (MO6) et que le mortier est permis
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		vector seen;
		if (support && region >= 0 && Math.RandomInt(0, 100) < m_iHarassShellChance && SeenContactNear(target, 300, m_iHarassKnownMin * 60, seen))
		{
			if (support.MortarRefusal(region, seen, SRP_ECmdShell.EXPLOSIF).IsEmpty())
			{
				string fired = support.RequestMortar(region, seen, SRP_ECmdShell.EXPLOSIF, m_iHarassShells, "harcèlement", staff);
				if (fired.IsEmpty())
				{
					m_iLastHarassUnix = nowUnix;
					m_bDirty = true;
					SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, string.Format("harcèlement sur le carré %1 (%2) : %3 obus de mortier (%4)", cellText, where, m_iHarassShells, why));
					return "";
				}
			}
		}

		// Sinon raid : départ hors de vue, sur un carré rouge, à harcelement_depart_min_m..max_m de la cible
		vector origin;
		bool found = false;
		float farthest = m_iHarassFromMaxM;
		if (farthest < m_iHarassFromMinM)
			farthest = m_iHarassFromMinM;
		for (int attempt = 0; attempt < 4 && !found; attempt++)
		{
			vector candidate;
			if (!SRP_Placement.FindSpawnPosition(target, m_iHarassFromMinM, farthest, players, true, true, candidate, 500))
				continue;
			if (!front.IsRedAt(candidate))
				continue;
			origin = candidate;
			found = true;
		}
		if (!found)
			return "aucun départ hors de vue sur le territoire ennemi";
		int wanted = 1;
		if (m_Commander.KnownPlayersNear(target, 3000, 1200) >= m_iHarassTwoGroupsFrom)
			wanted = 2;
		if (wanted > m_iHarassGroupsMax)
			wanted = m_iHarassGroupsMax;
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (capacity)
		{
			string refusal = capacity.Ask("raid-" + cell.ToString(), SRP_ECmdCapClass.PATROUILLE, wanted * 6, wanted, origin, "territoire");
			if (!refusal.IsEmpty())
				return refusal;
		}
		array<ref SRP_EnemyGroup> posted = {};
		enemies.SpawnWaveFrom(origin, 60, target, wanted, "territoire", posted);
		if (posted.IsEmpty())
			return "pose du détachement refusée";

		SRP_CmdRaid raid = new SRP_CmdRaid();
		raid.m_iCell = cell;
		raid.m_vTarget = target;
		raid.m_vOrigin = origin;
		raid.m_iStartUnix = nowUnix;
		raid.m_bStaff = staff;
		foreach (SRP_EnemyGroup raider : posted)
		{
			if (!raider)
				continue;
			SRP_CmdCapacity.Tag(raider, SRP_ECmdCapClass.PATROUILLE);
			raider.m_iCmdRole = SRP_ECmdRole.RAID;
			raider.m_iAssaultZone = -1;			// E5 : un raid ne reprend aucun carré
			raider.m_vCmdLast = origin;
			raider.m_iCmdStillUnix = nowUnix;
			raid.m_aGroups.Insert(raider);
			raid.m_iPosted += raider.m_iExpected;
		}
		raid.m_Plan = BuildAssault(raid.m_aGroups, target, origin, origin, false, -1);
		m_aRaids.Insert(raid);
		m_iLastHarassUnix = nowUnix;
		m_bDirty = true;
		string text = string.Format("harcèlement sur le carré %1 (%2) : %3 détachement(s), %4 soldats, %5 (%6)", cellText, where, raid.m_aGroups.Count(), raid.m_iPosted, ModeWord(raid.m_Plan), why);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, text);
		return "";
	}

	//================================================================================================
	// Localités (lues par le territoire et le front)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! MA7 : localité vidée par un décrochage tant qu'un joueur est à repli_vide_distance_m : ni posée ni complétée —
	//! SRP_TerritoryComponent, SRP_FrontEnemyComponent
	bool IsLocalityEmptied(string locality)
	{
		SRP_FrontLocality loc = FrontLocality(locality);
		return loc && loc.m_iEmptiedAt != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! MA11 : localité tenue au complet (m_iFullUntil de SRP_FrontLocality), Commandeur actif seulement (VU6 : gelé, le
	//! front garde ses seules règles) — SRP_FrontEnemyComponent (GarrisonScore), SRP_EnemyGarrison (AllowedSoldiers)
	bool IsKeptFull(string locality)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host || !host.IsCommanderActive())
			return false;
		SRP_FrontLocality loc = FrontLocality(locality);
		return loc && loc.m_iFullUntil > System.GetUnixTime();
	}

	//------------------------------------------------------------------------------------------------
	//! MA11 : carré de poste sur le chemin estimé des joueurs (20 min) — SRP_FrontEnemyComponent.PostsTick
	bool IsOnPlayersPath(int cell)
	{
		int noted;
		if (!m_mPathCell.Find(cell, noted))
			return false;
		return System.GetUnixTime() - noted < 1200;
	}

	//================================================================================================
	// Évènements
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! MA11 (hors staff, région organisée, Commandeur actif : c'est une décision, VU6) : localité hostile suivante
	//! tenue au complet reorg_complet_heures, pertes et envoyés remis à zéro — SRP_Commander.OnZoneCaptured
	void OnZoneCaptured(int zone, bool staff)
	{
		if (staff || !m_Commander)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!front || !host || !territory)
			return;
		if (!host.IsCommanderActive())
			return;
		int region = m_Commander.GetRegionOfZone(zone);
		SRP_CmdRegionBook book = Book();
		if (region >= 0 && book && book.IsRegionDisorganized(region))
			return;
		SRP_FrontZone taken = front.GetZone(zone);
		if (!taken)
			return;

		// La localité hostile la plus proche du centre de la zone prise, parmi les zones voisines au contact
		vector centroid = front.GetZoneCentroid(zone);
		SRP_SectorState best = null;
		float bestDistance = 1000000;
		foreach (int neighbour : taken.m_aNeighbours)
		{
			if (!front.IsZoneInContact(neighbour))
				continue;
			array<int> indices = {};
			front.GetZoneLocalities(neighbour, indices);
			foreach (int index : indices)
			{
				SRP_FrontLocality candidate = front.GetLocality(index);
				if (!candidate)
					continue;
				SRP_SectorState state = territory.FindState(candidate.m_sName);
				if (!state || !state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI)
					continue;
				float distance = vector.DistanceXZ(state.m_Sector.m_vCenter, centroid);
				if (distance < bestDistance)
				{
					best = state;
					bestDistance = distance;
				}
			}
		}
		if (!best)
			return;

		string nextName = best.m_sName;
		int nowUnix = System.GetUnixTime();
		host.ClearLosses(nextName);
		host.ClearSent(nextName);
		SRP_FrontLocality kept = FrontLocality(nextName);
		if (kept)
		{
			kept.m_iEmptiedAt = 0;
			kept.m_iFullUntil = nowUnix + m_iFullHours * 3600;
		}
		m_bDirty = true;
		string text = string.Format("%1 tenue au complet pendant %2 h après la chute de %3", nextName, m_iFullHours, ZoneText(zone));
		SRP_CmdLog.Decision(RegionAt(best.m_Sector.m_vCenter), false, text, "réorganisation après une zone perdue", FREE, "");
	}

	//------------------------------------------------------------------------------------------------
	//! Zone repassée rouge : replis et raids qui la visaient arrêtés — SRP_Commander.OnZoneLost
	void OnZoneLost(int zone, int reason, bool staff)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		// Raids : leur carré visé n'est plus à nous, ils rentrent. (Un décrochage part toujours d'une zone rouge
		// vers une localité hostile : aucun ne vise une zone qui était bleue.)
		for (int i = m_aRaids.Count() - 1; i >= 0; i--)
		{
			SRP_CmdRaid raid = m_aRaids[i];
			if (!raid)
			{
				m_aRaids.Remove(i);
				continue;
			}
			if (front.GetCellZone(raid.m_iCell) != zone)
				continue;
			EndRaid(raid, "zone reprise");
			m_aRaids.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Localité prise : ses replis arrêtés (RetireLater), colonnes réorientées vers la localité hostile la plus proche
	//! à 3 km, drapeaux vidée et tenue au complet effacés — SRP_Commander.OnLocalityTaken
	void OnLocalityTaken(string locality)
	{
		int nowUnix = System.GetUnixTime();
		vector takenPos = LocalityPoint(locality);
		SRP_SectorState next = NearestLocality(takenPos, 3000, null, false);
		if (next && next.m_sName == locality)
			next = null;

		// Replis de cette localité arrêtés : les groupes en marche se retirent hors de vue
		for (int i = m_aRetreats.Count() - 1; i >= 0; i--)
		{
			SRP_CmdRetreat retreat = m_aRetreats[i];
			if (!retreat)
			{
				m_aRetreats.Remove(i);
				continue;
			}
			if (retreat.m_sLocality == locality)
			{
				EndRetreat(retreat, "localité prise");
				m_aRetreats.Remove(i);
				continue;
			}
			// Repli qui allait vers la localité prise : nouvelle destination, sinon retrait
			if (retreat.m_sTo != locality)
				continue;
			if (!next)
			{
				EndRetreat(retreat, "destination prise");
				m_aRetreats.Remove(i);
				continue;
			}
			retreat.m_sTo = next.m_sName;
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			if (!enemies)
				continue;
			foreach (SRP_EnemyGroup mover : retreat.m_aGroups)
			{
				if (mover)
					enemies.OrderForcedMove(mover, next.m_Sector.m_vCenter, 30);
			}
		}

		// Colonnes qui y allaient
		for (int k = m_aColumns.Count() - 1; k >= 0; k--)
		{
			SRP_CmdColumn column = m_aColumns[k];
			if (!column || column.m_sTo != locality)
				continue;
			if (column.m_iState == SRP_ECmdColumnState.REELLE)
			{
				// Les camions roulent déjà : leurs soldats ne sont plus crédités nulle part (retour par le retrait)
				column.m_sTo = "";
				SRP_CmdLog.Note(RegionAt(takenPos), SRP_ECmdLogKind.DECISION, string.Format("colonne n°%1 : %2 prise, ses camions poursuivent sans destination", column.m_iId, locality));
				continue;
			}
			if (!next)
			{
				// Les soldats partent à l'arrière : ils restent comptés comme envoyés et se regarnissent
				SRP_CmdLog.Note(RegionAt(takenPos), SRP_ECmdLogKind.DECISION, string.Format("colonne n°%1 : %2 prise, aucune localité à 3 km, les %3 soldats partent à l'arrière", column.m_iId, locality, column.m_iSoldiers));
				m_aColumns.RemoveOrdered(k);
				m_bDirty = true;
				continue;
			}
			vector from = column.m_aRoute[0];
			if (column.m_iDepartUnix > 0)
				from = PositionAt(column, nowUnix);
			column.m_sTo = next.m_sName;
			BuildColumnRoute(column, from, next.m_Sector.m_vCenter);
			if (column.m_iDepartUnix > 0)
				column.m_iDepartUnix = nowUnix;
			m_bDirty = true;
			SRP_CmdLog.Note(RegionAt(takenPos), SRP_ECmdLogKind.DECISION, string.Format("colonne n°%1 : %2 prise, nouvelle destination %3", column.m_iId, locality, next.m_sName));
		}

		// Drapeaux vidée et tenue au complet effacés, regroupement au centre oublié
		SRP_FrontLocality loc = FrontLocality(locality);
		if (loc && (loc.m_iEmptiedAt != 0 || loc.m_iFullUntil != 0))
		{
			loc.m_iEmptiedAt = 0;
			loc.m_iFullUntil = 0;
			m_bDirty = true;
		}
		m_aGathered.RemoveItem(locality);
	}

	//------------------------------------------------------------------------------------------------
	//! MA5 : une perte rapportée dans la zone, point de départ du contrôle de décrochage #40 — SRP_Commander.Integrate
	void OnLossReported(int zone, vector position)
	{
		// Le contrôle (sections usées, décrochage de la garnison) passe au prochain passage au lieu d'attendre la minute
		m_bCheckRetreatSoon = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Relevé de contact connu (position floutée) pour les pistes « retranché » (MA8) et « chemin » (MA11) —
	//! SRP_Commander.Integrate
	void NoteContact(int region, int zone, vector position, int nowUnix)
	{
		// Zone : ancre A = premier relevé de l'épisode immobile, B = dernier relevé resté à assaut_retranche_m de A
		if (zone >= 0)
		{
			SRP_CmdTrack zoneTrack = m_mZoneTrack.Get(zone);
			if (!zoneTrack)
			{
				zoneTrack = new SRP_CmdTrack();
				m_mZoneTrack.Set(zone, zoneTrack);
				zoneTrack.m_vA = position;
				zoneTrack.m_iA = nowUnix;
			}
			else if (vector.DistanceXZ(position, zoneTrack.m_vA) > m_iEntrenchedM || nowUnix - zoneTrack.m_iB > 600)
			{
				zoneTrack.m_vA = position;
				zoneTrack.m_iA = nowUnix;
			}
			zoneTrack.m_vB = position;
			zoneTrack.m_iB = nowUnix;
		}
		// Région : les deux derniers relevés distants d'au moins reorg_releve_m (direction du chemin)
		if (region >= 0)
		{
			SRP_CmdTrack regionTrack = m_mRegionTrack.Get(region);
			if (!regionTrack)
			{
				regionTrack = new SRP_CmdTrack();
				m_mRegionTrack.Set(region, regionTrack);
				regionTrack.m_vA = position;
				regionTrack.m_iA = nowUnix;
				regionTrack.m_vB = position;
				regionTrack.m_iB = nowUnix;
			}
			else if (vector.DistanceXZ(position, regionTrack.m_vB) >= m_iTrackM)
			{
				regionTrack.m_vA = regionTrack.m_vB;
				regionTrack.m_iA = regionTrack.m_iB;
				regionTrack.m_vB = position;
				regionTrack.m_iB = nowUnix;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un camion de renfort HORS colonne part (contact, localité, entrée #69) : AddSent(source, load) (MA2, débit au
	//! départ, une seule fois) et inscription au registre des camions en route (RE13) ; le camion sort des demandes en
	//! attente de sa source (C2, NotePending) ; rend le numéro à ranger dans
	//! SRP_EnemyTruck.m_iSentId (0 si rien n'est inscrit : source vide ou load <= 0). Jamais appelé pour un camion de
	//! colonne (m_iColumnId != 0 : débitée par NewColumn) — SRP_EnemyTruckComponent
	int OnTruckLaunched(string source, int load)
	{
		if (source.IsEmpty())
			return 0;
		// C2 : le camion demandé le plus ancien de cette source n'est plus « en attente » (débité ci-dessous)
		int pendingIndex = m_aPendingFrom.Find(source);
		if (pendingIndex >= 0)
			RemovePendingAt(pendingIndex);
		if (load <= 0)
			return 0;
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (host)
			host.AddSent(source, load);
		SRP_CmdSentTruck entry = new SRP_CmdSentTruck();
		entry.m_iId = m_iNextSentId;
		m_iNextSentId++;
		entry.m_sFrom = source;
		entry.m_iLoad = load;
		m_aSentTrucks.Insert(entry);
		m_bDirty = true;
		return entry.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Le camion sentId est déchargé (passagers débarqués) ou perdu (détruit, retiré) : il sort du registre RE13 ;
	//! RIEN n'est rendu ici (les débarqués vivants rentrent par SRP_EnemyComponent.RetireTick, les morts restent des
	//! envoyés qui se regarnissent) ; sentId 0 : rien — SRP_EnemyTruckComponent
	void OnTruckUnloaded(int sentId)
	{
		if (sentId <= 0)
			return;
		for (int i = m_aSentTrucks.Count() - 1; i >= 0; i--)
		{
			SRP_CmdSentTruck entry = m_aSentTrucks[i];
			if (!entry || entry.m_iId == sentId)
			{
				m_aSentTrucks.RemoveOrdered(i);
				m_bDirty = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Départ réglé d'un camion de colonne (fin de l'embarquement, ou camion annulé ou rappelé avant de partir) :
	//! « returned » soldats demandés pour lui (m_iColumnLoad) ne sont pas partis et viennent d'être rendus à la source par
	//! le camion (ReturnSent). Ils sortent de la colonne (m_iSoldiers et m_iAboard, le papier ne change pas) : la
	//! relecture après redémarrage et CancelAll (ColumnPending) ne comptent plus, pour ce camion, que ses soldats assis.
	//! Colonne inconnue (déjà close, annulée, relue) : rien — SRP_EnemyTruckComponent.SettleDeparture
	void OnColumnTruckDeparted(int columnId, int returned)
	{
		if (returned <= 0)
			return;
		SRP_CmdColumn column = FindColumn(columnId);
		if (!column)
			return;
		int gone = Math.ClampInt(returned, 0, column.m_iAboard);
		if (gone <= 0)
			return;
		column.m_iAboard -= gone;
		column.m_iSoldiers = Math.MaxInt(column.m_iSoldiers - gone, 0);
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Camion de colonne revenu (livré ou perdu) : « share » = sa part encore comptée dans m_iAboard (ses soldats partis,
	//! ou sa charge si son départ n'a pas été réglé). Elle sort du compte de ColumnPending (jamais rendue une seconde
	//! fois) sans toucher m_iAboard ni m_iSoldiers (textes et papier inchangés) — SRP_EnemyTruckComponent.DeliverColumn,
	//! juste avant OnColumnDelivered
	void OnColumnTruckDone(int columnId, int share)
	{
		SRP_CmdColumn column = FindColumn(columnId);
		if (!column || share <= 0)
			return;
		column.m_iAboardDone += Math.ClampInt(share, 0, Math.MaxInt(column.m_iAboard - column.m_iAboardDone, 0));
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Colonne réelle livrée (la source a été débitée par NewColumn au départ) : joined = soldats entrés dans la
	//! garnison posée de la destination : leurs groupes gardent m_sSourceLocality = source (ils rentreront à leur
	//! source par SRP_EnemyComponent.RetireTick s'ils sont retirés vivants) et rien n'est rendu ni crédité ici ; sinon
	//! AddBonus(destination, survivants), groupes absorbés (m_sSourceLocality vidé avant leur retrait) ; aucun
	//! survivant : PERDUE — SRP_EnemyTruckComponent
	void OnColumnDelivered(int columnId, int survivors, bool joined)
	{
		SRP_CmdColumn column = FindColumn(columnId);
		if (!column)
			return;
		column.m_iTrucksDone++;
		if (survivors > 0)
		{
			column.m_iDelivered += survivors;
			// Vers un contact (destination vide) : rien à créditer, les débarqués rentreront par le retrait
			SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
			if (!joined && host && !column.m_sTo.IsEmpty())
				host.AddBonus(column.m_sTo, survivors);
		}
		m_bDirty = true;
		CheckColumnDone(column);
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe disparaît (supprimé, anéanti) : retiré des replis, raids, plans et débarqués — SRP_EnemyComponent
	void OnGroupGone(SRP_EnemyGroup record)
	{
		if (!record)
			return;
		foreach (SRP_CmdRetreat retreat : m_aRetreats)
		{
			if (!retreat)
				continue;
			retreat.m_aGroups.RemoveItem(record);
			if (retreat.m_Cover == record)
				retreat.m_Cover = null;
		}
		foreach (SRP_CmdRaid raid : m_aRaids)
		{
			if (!raid)
				continue;
			raid.m_aGroups.RemoveItem(record);
			RemoveFromPlan(raid.m_Plan, record);
		}
		foreach (SRP_CmdAssault plan : m_aAssaults)
			RemoveFromPlan(plan, record);
		int contactIndex = m_aContactGroups.Find(record);
		if (contactIndex >= 0)
			RemoveContactAt(contactIndex);
		int farIndex = m_aFarGroups.Find(record);
		if (farIndex >= 0)
		{
			m_aFarGroups.Remove(farIndex);
			m_aFarSince.Remove(farIndex);
		}
		m_aReordered.RemoveItem(record);
	}

	//================================================================================================
	// Contre-attaques : crochets appelés par SRP_FrontEnemyComponent quand IsCommanderActive() (sinon règle votée)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! CA1, chaque minute de la fenêtre D ± ca_fenetre_min (horloge F6 en secondes) : vrai = lancer maintenant ;
	//! jamais à moins de ca_ecart_min d'une précédente ; note = 50 x remplissage + présence connue ; toujours à
	//! D + ca_fenetre_min (écart compris) — SRP_FrontEnemyComponent.AttackClock
	bool CaWindowDecision(int clockSec, int dueSec, int zone, int nowUnix)
	{
		if (zone < 0 || !m_Commander)
			return clockSec >= dueSec;
		int window = m_iWindowMin * 60;
		if (clockSec < dueSec - window)
			return false;
		if (m_iLastCaUnix > 0 && nowUnix - m_iLastCaUnix < m_iGapMin * 60)
			return false;
		int region = m_Commander.GetRegionOfZone(zone);
		string where = ZoneText(zone);
		if (clockSec >= dueSec + window)
		{
			SRP_CmdLog.Decision(region, false, "contre-attaque annoncée sur " + where, "fin de la fenêtre d'annonce", FREE, "");
			return true;
		}

		// Note : stocks de la région (50 au plus) + présence connue autour de la zone visée (50, 25 ou 0)
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdResources resources = SRP_CmdResources.Get();
		float fill = 0;
		if (resources)
			fill = Math.Clamp(resources.GetFillRatio(region), 0, 1);
		int stockScore = Math.Round(50 * fill);
		int known = 0;
		if (front)
			known = m_Commander.KnownPlayersNear(front.GetZoneCentroid(zone), m_iPresenceM, m_iPresenceMin * 60);
		int presenceScore = 0;
		if (known == 0)
			presenceScore = 50;
		else if (known <= 2)
			presenceScore = 25;
		int score = stockScore + presenceScore;

		int trait = SRP_ECmdTrait.METHODIQUE;
		SRP_CmdRegionBook book = Book();
		if (book && region >= 0)
			trait = book.GetOfficerTrait(region);
		bool go;
		string rule;
		if (clockSec < dueSec)
		{
			// Avant l'échéance : jamais pour un officier lent ; seuil abaissé pour un audacieux (CO7)
			int threshold = m_iEarlyScore;
			if (trait == SRP_ECmdTrait.AUDACIEUX)
				threshold = m_iEarlyScoreBold;
			go = trait != SRP_ECmdTrait.LENT && score >= threshold;
			rule = string.Format("en avance, seuil %1", threshold);
		}
		else
		{
			// Après l'échéance : une grosse opération en cours (EQ3) retarde l'annonce jusqu'à la fin de la fenêtre
			SRP_CmdSupport support = SRP_CmdSupport.Get();
			go = score >= m_iOnTimeScore && !(support && support.IsBigOperationRunning(zone));
			rule = string.Format("après l'échéance, seuil %1", m_iOnTimeScore);
		}
		if (go)
		{
			string reason = string.Format("note %1 (stocks %2, présence %3), %4", score, stockScore, presenceScore, rule);
			SRP_CmdLog.Decision(region, false, "contre-attaque annoncée sur " + where, reason, FREE, "");
		}
		return go;
	}

	//------------------------------------------------------------------------------------------------
	//! CA2 (hors les 70 sur 100 de la dernière prise) : cible choisie sur renseignement (dépôt #49, zone peu
	//! défendue, carrés) ; -1 = aucune note positive (le front tire au hasard) — SRP_FrontEnemyComponent.PickTargetZone
	int PickByIntel(notnull array<int> candidates)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !m_Commander || candidates.IsEmpty())
			return -1;
		int bestScore = 0;
		array<int> best = {};
		foreach (int zone : candidates)
		{
			int score = IntelScore(front, zone);
			if (score <= 0 || score < bestScore)
				continue;
			if (score > bestScore)
			{
				bestScore = score;
				best.Clear();
			}
			best.Insert(zone);
		}
		if (best.IsEmpty())
			return -1;
		int chosen = best.GetRandomElement();
		string text = string.Format("cible choisie sur renseignement : %1 (note %2)", ZoneText(chosen), bestScore);
		SRP_CmdLog.Decision(m_Commander.GetRegionOfZone(chosen), false, text, "dépôt, défense connue, carrés au contact", FREE, "");
		return chosen;
	}

	//------------------------------------------------------------------------------------------------
	//! CA3 : vagues prévues = SRP_FrontEnemyComponent.m_iAttackMaxWaves (clé front_attaque_vagues, UNE clé par chiffre,
	//! la même que Commandeur gelé), une de moins si la région est désorganisée (OF8), jamais sous 1 ; la révision de
	//! la 2e vague reste bornée par ca_vagues_min / ca_vagues_max (ReviseWaves). Appelé à l'annonce : l'état de la
	//! contre-attaque précédente (plans, papier, appuis tirés) est oublié ici — SRP_FrontEnemyComponent.LaunchAttack
	int PlannedWaves(int zone)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		int waves = 3;
		if (host)
			waves = host.m_iAttackMaxWaves;
		int region = -1;
		if (m_Commander)
			region = m_Commander.GetRegionOfZone(zone);
		SRP_CmdRegionBook book = Book();
		bool disorganized = region >= 0 && book && book.IsRegionDisorganized(region);
		if (disorganized)
			waves--;
		if (waves < 1)
			waves = 1;
		ResetAttackState(0);
		string reason = "vagues du front";
		if (disorganized)
			reason = "région désorganisée : une vague de moins";
		SRP_CmdLog.Decision(region, false, string.Format("contre-attaque sur %1 : %2 vague(s) prévue(s)", ZoneText(zone), waves), reason, FREE, "");
		return waves;
	}

	//------------------------------------------------------------------------------------------------
	//! CA3 : à la 2e vague, total revu d'après la 1re (ca_vague_bonne_centiemes, ca_vague_brisee_centiemes), borné
	//! de ca_vagues_min à ca_vagues_max — SRP_FrontEnemyComponent.SendWave
	int ReviseWaves(SRP_CounterAttack attack)
	{
		if (!attack)
			return 0;
		int planned = attack.m_iPlannedWaves;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		int tick = System.GetTickCount();

		// La 1re vague : soldats posés (réels et papier), encore debout, et au moins un dans la zone ; carrés repris
		// relevés par l'hôte juste avant cet appel (m_iRetakenAtWave2)
		int retaken = attack.m_iRetakenAtWave2;
		int posted = m_iCaWave1Posted;
		if (posted <= 0)
			posted = attack.m_iWave1Posted;
		int fit = 0;
		bool inZone = false;
		array<ref SRP_EnemyGroup> firstWave = {};
		foreach (SRP_CmdAssault plan : m_aAssaults)
		{
			if (plan && plan.m_iWave == 1)
				PlanGroups(plan, firstWave);
		}
		foreach (SRP_EnemyGroup record : firstWave)
		{
			int standing = FitNow(record, tick);
			fit += standing;
			if (standing > 0 && front && enemies && front.GetZoneAt(enemies.GroupPosition(record)) == attack.m_iZone)
				inZone = true;
		}
		foreach (SRP_CmdPaperUnit unit : m_aPaper)
		{
			if (!unit || unit.m_iWave != 1 || unit.m_iZone != attack.m_iZone)
				continue;
			fit += unit.m_iSoldiers;
			if (front && front.GetZoneAt(unit.m_vPos) == attack.m_iZone)
				inZone = true;
		}
		// 1re vague posée sans nous (Commandeur gelé à ce moment) : les chiffres de l'hôte (m_iWave1Fit, m_iWave1Posted,
		// groupes m_aWave1)
		if (m_iCaWave1Posted <= 0 && fit <= 0)
		{
			fit = attack.m_iWave1Fit;
			foreach (SRP_EnemyGroup hostRecord : attack.m_aWave1)
			{
				if (FitNow(hostRecord, tick) > 0 && front && enemies && front.GetZoneAt(enemies.GroupPosition(hostRecord)) == attack.m_iZone)
					inZone = true;
			}
		}
		bool good = retaken > 0 || (posted > 0 && fit * 100 >= m_iWaveGoodCent * posted && inZone);
		bool broken = retaken == 0 && posted > 0 && fit * 100 <= m_iWaveBrokenCent * posted;

		// Caractère (CO7) : prudent, jamais d'ajout ; audacieux, ajout aussi quand la vague n'est pas brisée
		int region = attack.m_iRegion;
		if (region < 0 && m_Commander)
			region = m_Commander.GetRegionOfZone(attack.m_iZone);
		int trait = SRP_ECmdTrait.METHODIQUE;
		SRP_CmdRegionBook book = Book();
		if (book && region >= 0)
			trait = book.GetOfficerTrait(region);
		bool add = good;
		if (trait == SRP_ECmdTrait.AUDACIEUX && !broken)
			add = true;
		if (trait == SRP_ECmdTrait.PRUDENT)
			add = false;
		// Ajout seulement s'il reste sous les 120 la place d'une vague + ca_marge_place
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (add && capacity && capacity.Room(SRP_ECmdCapClass.COMBAT, "territoire") < posted + m_iRoomMargin)
			add = false;

		int total = planned;
		if (add)
			total++;
		else if (broken)
			total--;
		int low = m_iWavesMin;
		int high = m_iWavesMax;
		if (high < low)
			high = low;
		bool disorganized = attack.m_bDisorganized || (region >= 0 && book && book.IsRegionDisorganized(region));
		if (disorganized)
		{
			low--;
			if (low < 1)
				low = 1;
			high--;
			if (high < low)
				high = low;
		}
		total = Math.ClampInt(total, low, high);
		string verdict = "ni bonne ni brisée";
		if (good)
			verdict = "bonne";
		else if (broken)
			verdict = "brisée";
		string text = string.Format("révision des vagues sur %1 : %2 au lieu de %3", attack.m_sZone, total, planned);
		string reason = string.Format("1re vague %1 : %2 debout sur %3, %4 carré(s) repris", verdict, fit, posted, retaken);
		SRP_CmdLog.Decision(region, false, text, reason, FREE, "");
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes d'une vague permis par la place COMBAT ((Room(COMBAT) - ca_marge_place) / 6) —
	//! SRP_FrontEnemyComponent.SendWave
	int WaveRoomGroups()
	{
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (!capacity)
			return 1000;
		int room = capacity.Room(SRP_ECmdCapClass.COMBAT, "territoire") - m_iRoomMargin;
		if (room <= 0)
			return 0;
		return room / 6;
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : aucun joueur à ca_papier_distance_m de l'origine ni du centre de la zone -> vague sur le papier —
	//! SRP_FrontEnemyComponent.SendWave
	bool WantPaperWave(vector origin, vector zoneCenter)
	{
		array<IEntity> players = FreshPlayers();
		return SRP_Placement.NearestPlayer(origin, players) > m_iPaperM && SRP_Placement.NearestPlayer(zoneCenter, players) > m_iPaperM;
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : ajoute une unité sur le papier (soldats, vague, axe) ; ses soldats comptent dans les posés de
	//! l'attaque (CA5) — SRP_FrontEnemyComponent.SendWave
	void AddPaperWave(SRP_CounterAttack attack, vector origin, int soldiers, int wave, int axis)
	{
		if (!attack || soldiers <= 0)
			return;
		int nowUnix = System.GetUnixTime();
		SyncAttack(attack);
		SRP_CmdPaperUnit unit = new SRP_CmdPaperUnit();
		unit.m_iZone = attack.m_iZone;
		unit.m_iWave = wave;
		unit.m_iAxis = axis;
		unit.m_iSoldiers = soldiers;
		unit.m_vPos = origin;
		unit.m_vOrigin = origin;
		unit.m_iLastUnix = nowUnix;
		m_aPaper.Insert(unit);
		attack.m_iPostedSoldiers += soldiers;
		if (wave == 1)
			m_iCaWave1Posted += soldiers;
		SRP_CmdLog.Note(attack.m_iRegion, SRP_ECmdLogKind.DECISION, string.Format("vague %1 sur %2 : %3 soldats en marche loin des joueurs (axe %4)", wave, attack.m_sZone, soldiers, axis + 1));
	}

	//------------------------------------------------------------------------------------------------
	//! MA8 : plan d'assaut de la vague posée (face, flanc, deux axes), m_iCmdRole ASSAUT, attack.m_iPostedSoldiers —
	//! SRP_FrontEnemyComponent.SendWave (une vague posée en plusieurs fois rejoint le plan de sa vague)
	void OnWavePosted(SRP_CounterAttack attack, int wave, array<ref SRP_EnemyGroup> groups, vector originA, vector originB, bool hasB)
	{
		if (!attack || !groups || groups.IsEmpty())
			return;
		SyncAttack(attack);
		array<ref SRP_EnemyGroup> counted = {};
		array<ref SRP_EnemyGroup> fresh = {};
		int soldiers = 0;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || counted.Find(record) >= 0)
				continue;
			counted.Insert(record);
			soldiers += record.m_iExpected;
			// Anéanti pendant l'échelonnement (le jeu a supprimé son groupe) : compté dans les posés, hors du plan
			if (!IsPresent(record))
				continue;
			fresh.Insert(record);
			record.m_iCmdRole = SRP_ECmdRole.ASSAUT;
			record.m_iAssaultZone = attack.m_iZone;
		}
		if (wave > m_iCaPlannedWave)
			m_iCaPlannedWave = wave;
		attack.m_iPostedSoldiers += soldiers;
		if (wave == 1)
			m_iCaWave1Posted += soldiers;
		if (fresh.IsEmpty())
			return;

		SRP_CmdAssault plan = PlanOfWave(wave, attack.m_iZone);
		if (plan)
		{
			AddToPlan(plan, fresh, System.GetUnixTime());
		}
		else
		{
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			vector from = originA;
			if (enemies)
				from = enemies.GroupPosition(fresh[0]);
			int cell;
			vector objective = AttackObjective(attack, from, cell);
			plan = BuildAssault(fresh, objective, originA, originB, hasB, attack.m_iZone);
			plan.m_iWave = wave;
			plan.m_iObjCell = cell;
			m_aAssaults.Insert(plan);
		}
		string text = string.Format("vague %1 sur %2, %3 soldats, %4", wave, attack.m_sZone, soldiers, ModeWord(plan));
		SRP_CmdLog.Decision(attack.m_iRegion, false, text, "contre-attaque", FREE, "");
	}

	//------------------------------------------------------------------------------------------------
	//! CA4 et MO8, à chaque passage pendant l'annonce : mortier ca_mortier_avance_s avant l'assaut, artillerie (amas de
	//! ca_amas_joueurs sur ca_amas_rayon_m) sinon blindé ca_lourd_avance_s avant, feinte (ca_feinte sur 100, 2e axe
	//! réel), Su-57 (ca_avion_chance) ; région désorganisée : rien (OF8) ; tout par SRP_CmdSupport.Request* —
	//! SRP_FrontEnemyComponent.AttackTick
	void Support(SRP_CounterAttack attack, int nowUnix)
	{
		if (!attack || attack.m_iZone < 0 || !m_Commander)
			return;
		SyncAttack(attack);
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		SRP_CmdRegionBook book = Book();
		if (!support)
			return;
		int region = attack.m_iRegion;
		if (region < 0)
			region = m_Commander.GetRegionOfZone(attack.m_iZone);
		if (region < 0 || attack.m_bDisorganized || (book && book.IsRegionDisorganized(region)))
			return;
		int assaultIn = attack.m_iWarnEndUnix - nowUnix;
		string reason = "contre-attaque sur " + attack.m_sZone;

		// Feinte (MO8) : tirée une fois, deux axes et mortier permis ; les vagues ne viennent alors que par l'axe réel
		if (!m_bFeintRolled)
		{
			m_bFeintRolled = true;
			if (attack.m_bHasB && Math.RandomInt(0, 100) < m_iFeintChance && support.MortarRefusal(region, attack.m_vOriginB, SRP_ECmdShell.EXPLOSIF).IsEmpty())
			{
				attack.m_bFeint = true;
				SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, string.Format("contre-attaque sur %1 : feinte prévue sur le 2e axe, toutes les vagues par le 1er (tirage de %2 sur 100)", attack.m_sZone, m_iFeintChance));
			}
		}
		if (attack.m_bFeint && !m_bFeintFired && assaultIn <= 60)
		{
			m_bFeintFired = true;
			vector falseEntry = WaitPoint(attack.m_vTarget, attack.m_vOriginB);
			string feint = support.RequestFeint(region, falseEntry, "feinte, " + reason, false);
			LogSupport(region, false, "feinte sur le 2e axe de " + attack.m_sZone, reason, feint, "obus de mortier", SRP_ECmdStock.OBUS);
		}

		// Mortier explosif sur les défenseurs connus, ca_mortier_avance_s avant l'assaut
		if (!attack.m_bSupportMortar && assaultIn <= m_iMortarLeadS)
		{
			attack.m_bSupportMortar = true;
			vector aim;
			string mortar = "aucun défenseur vu";
			if (SeenContactNear(attack.m_vTarget, 1500, 600, aim))
				mortar = support.RequestMortar(region, aim, SRP_ECmdShell.EXPLOSIF, 0, reason, false);
			LogSupport(region, false, "mortier avant l'assaut sur " + attack.m_sZone, reason, mortar, "obus de mortier", SRP_ECmdStock.OBUS);
		}

		// Un seul gros moyen : artillerie lourde sur un amas connu, sinon un blindé qui suit la 2e vague
		if (!attack.m_bSupportHeavy && assaultIn <= m_iHeavyLeadS)
		{
			attack.m_bSupportHeavy = true;
			vector cluster;
			if (FindCluster(attack.m_iZone, attack.m_vTarget, cluster))
			{
				string artillery = support.RequestArtillery(region, cluster, attack.m_iZone, reason, false);
				LogSupport(region, true, "artillerie lourde avant l'assaut sur " + attack.m_sZone, reason + ", amas de joueurs connus", artillery, "1 tir d'artillerie lourde", SRP_ECmdStock.ARTILLERIE);
			}
			else
			{
				string armor = support.RequestArmor(region, attack.m_vTarget, SRP_ECmdArmorReason.CONTRE_ATTAQUE, attack.m_iZone, -1, false);
				LogSupport(region, true, "blindé avec la contre-attaque sur " + attack.m_sZone, reason, armor, "1 blindé", SRP_ECmdStock.BLINDE);
			}
		}

		// Passage de Su-57, une fois par contre-attaque, sur ca_avion_chance
		if (!m_bFlybyRolled && assaultIn <= m_iHeavyLeadS)
		{
			m_bFlybyRolled = true;
			if (Math.RandomInt(0, 100) < m_iFlybyChance)
			{
				string flyby = support.RequestFlyby(attack.m_vTarget, reason, false);
				LogSupport(region, false, "passage d'avion sur " + attack.m_sZone, reason, flyby, "aucun (passage sans frappe)", -1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un appui demandé par les manœuvres (hors IssueOrder) : décision (coût, reste du stock) ou refus au journal
	protected void LogSupport(int region, bool heavy, string what, string reason, string result, string cost, int stockKind)
	{
		if (!result.IsEmpty())
		{
			SRP_CmdLog.Refusal(region, what, result);
			return;
		}
		string left = "";
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources && stockKind >= 0)
		{
			int stockRegion = -1;
			if (stockKind == SRP_ECmdStock.OBUS)
				stockRegion = region;
			left = resources.StockText(stockKind, stockRegion);
		}
		SRP_CmdLog.Decision(region, heavy, what, reason, cost, left);
	}

	//------------------------------------------------------------------------------------------------
	//! MA8, CA6 : conduit l'assaut (plans, objectifs reprenables, groupes bloqués passés sur le papier) ; remplace
	//! AdvanceAssault quand le Commandeur est actif (IsCommanderActive) — SRP_FrontEnemyComponent.AttackTick
	void DriveAssault(SRP_CounterAttack attack, int nowUnix)
	{
		if (!attack)
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!enemies || !front)
			return;
		SyncAttack(attack);
		int tick = System.GetTickCount();
		if (attack.m_iWaves != m_iCaSeenWave)
		{
			m_iCaSeenWave = attack.m_iWaves;
			m_iCaSeenWaveUnix = nowUnix;
		}

		// Tout groupe d'assaut appartient à un plan (sinon il rejoint celui de sa vague) ; une vague encore en cours de
		// pose (échelonnement) garde les ordres du front jusqu'à OnWavePosted. Un groupe anéanti reste dans m_aGroups
		// (le jeu supprime son SCR_AIGroup, OnGroupGone le sort des plans) : jamais repris ni lu (GroupPosition)
		foreach (SRP_EnemyGroup orphan : attack.m_aGroups)
		{
			if (!IsPresent(orphan) || FitNow(orphan, tick) <= 0 || PlanOf(orphan) || IsPosting(attack, orphan, nowUnix))
				continue;
			SRP_CmdAssault home = PlanOfWave(attack.m_iWaves, attack.m_iZone);
			if (!home)
			{
				array<ref SRP_EnemyGroup> single = {};
				single.Insert(orphan);
				int orphanCell;
				vector orphanGoal = AttackObjective(attack, enemies.GroupPosition(orphan), orphanCell);
				home = BuildAssault(single, orphanGoal, attack.m_vOriginA, attack.m_vOriginB, false, attack.m_iZone);
				home.m_iWave = attack.m_iWaves;
				home.m_iObjCell = orphanCell;
				m_aAssaults.Insert(home);
				continue;
			}
			array<ref SRP_EnemyGroup> joining = {};
			joining.Insert(orphan);
			AddToPlan(home, joining, nowUnix);
		}

		// Plans : objectif reprenable (repris ou tenu 180 s -> nouvel objectif), puis un pas de la doctrine
		for (int p = m_aAssaults.Count() - 1; p >= 0; p--)
		{
			SRP_CmdAssault plan = m_aAssaults[p];
			if (!plan || plan.m_iZone != attack.m_iZone)
				continue;
			UpdateObjective(front, enemies, attack, plan, nowUnix);
			DriveAssaultPlan(plan, nowUnix);
		}

		// MO7 : rideau d'obus fumigènes quand l'élément le plus proche arrive à appui_fumee_declenchement_m de la cible,
		// une fois par contre-attaque ; avec une feinte, l'axe réel ne reçoit rien (le rideau est sur le faux axe)
		if (!m_bCaSmokeDone && !attack.m_bFeint)
			CurtainTick(enemies, attack, tick);

		// Groupes bloqués : nouvel ordre à ca_bloque_min ; à ca_bloque_papier_min et hors de vue, passage sur le papier
		for (int i = attack.m_aGroups.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup record = attack.m_aGroups[i];
			if (!IsPresent(record) || SRP_EnemyComponent.FitOf(record) <= 0 || IsPosting(attack, record, nowUnix))
				continue;
			vector position = enemies.GroupPosition(record);
			if (InContact(record, tick) || record.m_iCmdStillUnix == 0 || vector.DistanceXZ(position, record.m_vCmdLast) > 30)
			{
				record.m_vCmdLast = position;
				record.m_iCmdStillUnix = nowUnix;
				m_aReordered.RemoveItem(record);
				continue;
			}
			int still = nowUnix - record.m_iCmdStillUnix;
			if (still >= m_iStuckPaperMin * 60 && SRP_Placement.NearestPlayer(position, m_aPlayers) > 300 && !SRP_Placement.IsSeenByAnyPlayer(position, m_aPlayers))
			{
				GroupToPaper(enemies, attack, record, nowUnix);
				continue;
			}
			if (still >= m_iStuckMin * 60 && m_aReordered.Find(record) < 0)
			{
				SRP_CmdAssault owner = PlanOf(record);
				vector goal = attack.m_vTarget;
				if (owner)
					goal = owner.m_vObjective;
				enemies.OrderSearch(record, goal);
				m_aReordered.Insert(record);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La dernière vague est arrivée (chaque groupe entré dans la zone ou hors de combat, chaque unité papier entrée),
	//! sans délai réputé — SRP_FrontEnemyComponent.AttackTick
	bool WaveArrived(SRP_CounterAttack attack)
	{
		if (!attack)
			return false;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!enemies || !front)
			return false;
		int tick = System.GetTickCount();
		foreach (SRP_EnemyGroup record : attack.m_aLastWave)
		{
			if (!record)
				continue;
			if (SRP_EnemyComponent.InGraceOf(record, tick))
				return false;
			if (SRP_EnemyComponent.FitOf(record) <= 0)
				continue;
			if (front.GetZoneAt(enemies.GroupPosition(record)) != attack.m_iZone)
				return false;
		}
		foreach (SRP_CmdPaperUnit unit : m_aPaper)
		{
			if (!unit || unit.m_iZone != attack.m_iZone || unit.m_iWave != attack.m_iWaves || unit.m_iSoldiers <= 0)
				continue;
			if (front.GetZoneAt(unit.m_vPos) != attack.m_iZone)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CA5, C8 : SRP_EAttackEnd.DEFENDUE si les soldats en état de combattre (papier compris) tombent à
	//! (100 - ca_abandon_centiemes) centièmes des posés après ca_abandon_vagues vagues (ou toutes posées) ; AUCUNE
	//! sinon (la rupture ROMPUE à 60 min reste au front, m_iAttackMaxMinutes) — SRP_FrontEnemyComponent.AttackTick
	int CheckEnd(SRP_CounterAttack attack, int nowUnix)
	{
		if (!attack || attack.m_iState != SRP_EFrontAttack.ASSAUT)
			return SRP_EAttackEnd.AUCUNE;
		int posted = attack.m_iPostedSoldiers;
		if (posted <= 0)
			return SRP_EAttackEnd.AUCUNE;
		if (attack.m_iWaves < m_iAbandonWaves && attack.m_iWaves < attack.m_iPlannedWaves)
			return SRP_EAttackEnd.AUCUNE;
		int tick = System.GetTickCount();
		int fit = 0;
		foreach (SRP_EnemyGroup record : attack.m_aGroups)
			fit += FitNow(record, tick);
		foreach (SRP_CmdPaperUnit unit : m_aPaper)
		{
			if (unit && unit.m_iZone == attack.m_iZone)
				fit += unit.m_iSoldiers;
		}
		if (fit * 100 > (100 - m_iAbandonCent) * posted)
			return SRP_EAttackEnd.AUCUNE;
		if (!m_bCaEndLogged)
		{
			m_bCaEndLogged = true;
			string text = string.Format("contre-attaque sur %1 brisée : %2 soldats en état sur %3 engagés", attack.m_sZone, fit, posted);
			SRP_CmdLog.Decision(attack.m_iRegion, false, text, "deux tiers de pertes (C8)", FREE, "");
		}
		return SRP_EAttackEnd.DEFENDUE;
	}

	//------------------------------------------------------------------------------------------------
	//! Fin d'attaque (SRP_EAttackEnd) : groupes réels en repli puis RetireLater, ReleaseAwake, papier effacé,
	//! capture.ClearPaperAssault(zone), SRP_CmdSupport.EndAssault(zone), m_iLastCaUnix, m_iCaCount — SRP_FrontEnemyComponent.EndAttack
	void EndAttack(SRP_CounterAttack attack, int result)
	{
		int nowUnix = System.GetUnixTime();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int zone = -1;
		if (attack)
			zone = attack.m_iZone;

		// Tous les groupes suivis : ceux des plans et ceux qui restent à l'attaque
		array<ref SRP_EnemyGroup> tracked = {};
		foreach (SRP_CmdAssault plan : m_aAssaults)
			PlanGroups(plan, tracked);
		if (attack)
		{
			foreach (SRP_EnemyGroup extra : attack.m_aGroups)
			{
				if (extra && tracked.Find(extra) < 0)
					tracked.Insert(extra);
			}
		}
		array<ref SRP_EnemyGroup> leaving = {};
		foreach (SRP_EnemyGroup record : tracked)
		{
			if (!record)
				continue;
			SRP_EnemyComponent.ReleaseAwake(record);
			if (!record.m_Group || record.m_Group.IsDeleted())
				continue;
			// Zone perdue par nous : les assaillants adoptés par la localité (AdoptAssailants, sortis de l'attaque par le
			// front avant cet appel) restent sur place
			if (result == SRP_EAttackEnd.PERDUE && attack && attack.m_aGroups.Find(record) < 0)
			{
				record.m_iCmdRole = SRP_ECmdRole.AUCUN;
				continue;
			}
			// Repli sous un fumigène vers le départ rouge de leur axe, puis retrait hors de vue
			if (enemies && SRP_EnemyComponent.AliveAgents(record) > 0)
			{
				vector home = OriginOf(record, attack);
				enemies.OrderRetreat(record, enemies.GroupPosition(record), 1, home, home);
			}
			record.m_iCmdRole = SRP_ECmdRole.REPLI;
			record.m_iAssaultZone = -1;
			leaving.Insert(record);
		}

		// Papier effacé, déclarations à la capture retirées
		for (int i = m_aPaper.Count() - 1; i >= 0; i--)
		{
			SRP_CmdPaperUnit unit = m_aPaper[i];
			if (!unit || zone < 0 || unit.m_iZone == zone)
				m_aPaper.Remove(i);
		}
		m_aAssaults.Clear();
		m_aFarGroups.Clear();
		m_aFarSince.Clear();
		m_aReordered.Clear();
		if (enemies && !leaving.IsEmpty())
			enemies.RetireLater(leaving);
		if (front && zone >= 0)
		{
			SRP_FrontCapture capture = front.GetCapture();
			if (capture)
				capture.ClearPaperAssault(zone);
		}
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		if (support && zone >= 0)
			support.EndAssault(zone);
		m_iLastCaUnix = nowUnix;
		m_iCaCount++;
		m_bDirty = true;
		int region = -1;
		string where = "";
		if (attack)
		{
			region = attack.m_iRegion;
			where = attack.m_sZone;
		}
		string text = string.Format("fin de la contre-attaque sur %1 : %2 (%3 en tout)", where, EndWord(result), m_iCaCount);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, text);
	}

	//------------------------------------------------------------------------------------------------
	//! CA7 (relais de SRP_CmdResources, seule implémentation) : cancel = !IsOffensiveAllowedByStocks() ; rend chance +
	//! OffensiveChanceBonus() (le plafond de 60 reste appliqué par le front) — SRP_FrontEnemyComponent.RunOffensive
	int NightChance(int chance, out bool cancel)
	{
		cancel = false;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (!resources)
			return chance;
		cancel = !resources.IsOffensiveAllowedByStocks();
		return chance + resources.OffensiveChanceBonus();
	}

	//================================================================================================
	// Staff, remises, sauvegarde
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! ia-reset et wipe : colonnes annulées et rendues à leur source (ReturnSent), camions demandés oubliés (C2), papier,
	//! raids et replis retirés hors de vue — SRP_Commander
	string CancelAll(string author)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		int columns = 0;
		int returned = 0;
		foreach (SRP_CmdColumn column : m_aColumns)
		{
			if (!column)
				continue;
			columns++;
			int pending = ColumnPending(column);
			if (host && !column.m_sFrom.IsEmpty() && pending > 0)
			{
				host.ReturnSent(column.m_sFrom, pending);
				returned += pending;
			}
		}
		m_aColumns.Clear();
		// C2 : les camions demandés et pas encore posés sont supprimés par la remise à zéro de l'IA qui suit toujours
		// (SRP_EnemyTruckComponent.ResetAll) sans jamais appeler OnTruckLaunched : leurs soldats ne sont plus retenus
		ClearPending();
		int raids = m_aRaids.Count();
		int retreats = m_aRetreats.Count();
		EndAllRaids("remise à zéro");
		EndAllRetreats("remise à zéro");
		DropAttackPlans();
		m_bDirty = true;
		string text = string.Format("Manœuvres : %1 colonne(s) annulée(s), %2 soldats rendus, %3 harcèlement(s) et %4 décrochage(s) arrêtés", columns, returned, raids, retreats);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STAFF, text);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Colonnes, replis, raids, freins et dernier renfort autour d'un point — SRP_CmdScreens.StaffNearReport
	string GetReport(vector from, float radius)
	{
		int nowUnix = System.GetUnixTime();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		array<string> lines = {};
		foreach (SRP_CmdColumn column : m_aColumns)
		{
			if (!column)
				continue;
			int columnMeters = Math.Round(vector.DistanceXZ(from, PositionAt(column, nowUnix)));
			if (columnMeters > radius)
				continue;
			lines.Insert(ColumnLine(column, nowUnix) + string.Format(", à %1 m", columnMeters));
		}
		foreach (SRP_CmdRetreat retreat : m_aRetreats)
		{
			if (!retreat)
				continue;
			int retreatMeters = Math.Round(vector.DistanceXZ(from, retreat.m_vExit));
			if (retreatMeters > radius)
				continue;
			int marching = retreat.m_aGroups.Count();
			if (retreat.m_Cover)
				marching++;
			lines.Insert(string.Format("Décrochage de %1 vers %2 depuis %3 : %4 section(s) en marche, sortie à %5 m", retreat.m_sLocality, retreat.m_sTo, SRP_CmdScreens.DurationText(nowUnix - retreat.m_iStartUnix), marching, retreatMeters));
		}
		foreach (SRP_CmdRaid raid : m_aRaids)
		{
			if (!raid)
				continue;
			int raidMeters = Math.Round(vector.DistanceXZ(from, raid.m_vTarget));
			if (raidMeters > radius)
				continue;
			string cellText = SRP_CmdScreens.CellText(raid.m_vTarget);
			int standing = 0;
			int tick = System.GetTickCount();
			foreach (SRP_EnemyGroup raider : raid.m_aGroups)
				standing += FitNow(raider, tick);
			lines.Insert(string.Format("Harcèlement sur le carré %1 depuis %2 : %3 soldats debout sur %4, à %5 m", cellText, SRP_CmdScreens.DurationText(nowUnix - raid.m_iStartUnix), standing, raid.m_iPosted, raidMeters));
		}
		foreach (SRP_CmdPaperUnit unit : m_aPaper)
		{
			if (!unit)
				continue;
			int paperMeters = Math.Round(vector.DistanceXZ(from, unit.m_vPos));
			if (paperMeters > radius)
				continue;
			lines.Insert(string.Format("Vague %1 sur le papier : %2 soldats vers le carré %3, à %4 m", unit.m_iWave, unit.m_iSoldiers, SRP_CmdScreens.CellText(unit.m_vPos), paperMeters));
		}
		if (front)
		{
			foreach (int zone, int last : m_mLastReinforce)
			{
				int zoneMeters = Math.Round(vector.DistanceXZ(from, front.GetZoneCentroid(zone)));
				if (zoneMeters > radius)
					continue;
				int wait = GetReinforceWaitSeconds(zone);
				string next = "renfort de nouveau possible";
				if (wait > 0)
					next = "frein encore " + SRP_CmdScreens.DurationText(wait);
				lines.Insert(string.Format("Dernier renfort vers %1 il y a %2 : %3", ZoneText(zone), SRP_CmdScreens.DurationText(nowUnix - last), next));
			}
		}
		if (enemies)
		{
			foreach (SRP_EnemyGroup landed : m_aContactGroups)
			{
				// Anéanti depuis moins d'une minute (pas encore sorti par OnGroupGone) : son groupe n'existe plus
				if (!landed || SRP_EnemyComponent.AliveAgents(landed) <= 0)
					continue;
				int landedMeters = Math.Round(vector.DistanceXZ(from, enemies.GroupPosition(landed)));
				if (landedMeters <= radius)
					lines.Insert(string.Format("Débarqués d'un camion de renfort : %1 soldats, à %2 m", SRP_EnemyComponent.FitOf(landed), landedMeters));
			}
		}
		// Rien : "" (l'écran écrit alors « Rien du Commandeur dans ce rayon » s'il n'a rien d'autre)
		if (lines.IsEmpty())
			return "";
		return JoinLines(lines);
	}

	//------------------------------------------------------------------------------------------------
	//! Colonnes en route (source, destination, soldats, papier ou réelle, distance restante) ; "" si aucune (l'écran
	//! écrit « Aucune colonne en route ») — SRP_CmdScreens
	string GetColumnsReport()
	{
		int nowUnix = System.GetUnixTime();
		array<string> lines = {};
		foreach (SRP_CmdColumn column : m_aColumns)
		{
			if (column)
				lines.Insert(ColumnLine(column, nowUnix));
		}
		if (lines.IsEmpty())
			return "";
		return JoinLines(lines);
	}

	//------------------------------------------------------------------------------------------------
	//! « dernier à 21:10 vers Régina (S07) · prochain possible dans 4 min » pour une région ; "" si aucun renfort depuis
	//! le démarrage (l'écran écrit « aucun depuis le démarrage ») — SRP_CmdScreens (page région)
	string GetReinforcementText(int region)
	{
		int last;
		int zone;
		if (!m_mRegionLastUnix.Find(region, last) || !m_mRegionLastZone.Find(region, zone))
			return "";
		int wait = GetReinforceWaitSeconds(zone);
		string next = "prochain possible maintenant";
		if (wait > 0)
			next = "prochain possible dans " + SRP_CmdScreens.DurationText(wait);
		return string.Format("dernier à %1 vers %2 · %3", ClockText(last), ZoneText(zone), next);
	}

	//------------------------------------------------------------------------------------------------
	//! cmd_ma_v, cmd_ma_harass, cmd_ma_ca_last, cmd_ma_ca_count, cmd_ma_cols puis cmd_ma_cN_from, _to, _soldiers
	//! (colonnes en route, papier ou réelles), cmd_ma_tn puis cmd_ma_tN_from, _n (camions hors colonne lancés et pas
	//! encore déchargés, m_aSentTrucks) — SRP_Commander.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		int version = 1;
		ctx.WriteValue("cmd_ma_v", version);
		ctx.WriteValue("cmd_ma_harass", m_iLastHarassUnix);
		ctx.WriteValue("cmd_ma_ca_last", m_iLastCaUnix);
		ctx.WriteValue("cmd_ma_ca_count", m_iCaCount);
		int columns = 0;
		foreach (SRP_CmdColumn column : m_aColumns)
		{
			if (!column || column.m_sFrom.IsEmpty())
				continue;
			int pending = ColumnPending(column);
			if (pending <= 0)
				continue;
			string columnPrefix = "cmd_ma_c" + columns.ToString() + "_";
			ctx.WriteValue(columnPrefix + "from", column.m_sFrom);
			ctx.WriteValue(columnPrefix + "to", column.m_sTo);
			ctx.WriteValue(columnPrefix + "soldiers", pending);
			columns++;
		}
		ctx.WriteValue("cmd_ma_cols", columns);
		int trucks = 0;
		foreach (SRP_CmdSentTruck entry : m_aSentTrucks)
		{
			if (!entry || entry.m_sFrom.IsEmpty() || entry.m_iLoad <= 0)
				continue;
			string truckPrefix = "cmd_ma_t" + trucks.ToString() + "_";
			ctx.WriteValue(truckPrefix + "from", entry.m_sFrom);
			ctx.WriteValue(truckPrefix + "n", entry.m_iLoad);
			trucks++;
		}
		ctx.WriteValue("cmd_ma_tn", trucks);
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture ; RE13 : chaque colonne lue et chaque camion lu (cmd_ma_t*) rendent leurs soldats à leur source
	//! (ReturnSent), une seule fois ; colonnes et registre des camions vidés ensuite — SRP_Commander.ReadFrom
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		m_aColumns.Clear();
		m_aSentTrucks.Clear();
		int version = 0;
		ctx.ReadValue("cmd_ma_v", version);
		int harass = 0;
		if (ctx.ReadValue("cmd_ma_harass", harass))
			m_iLastHarassUnix = harass;
		int caLast = 0;
		if (ctx.ReadValue("cmd_ma_ca_last", caLast))
			m_iLastCaUnix = caLast;
		int caCount = 0;
		if (ctx.ReadValue("cmd_ma_ca_count", caCount))
			m_iCaCount = caCount;

		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		int returned = 0;
		int columns = 0;
		ctx.ReadValue("cmd_ma_cols", columns);
		for (int i = 0; i < columns; i++)
		{
			string columnPrefix = "cmd_ma_c" + i.ToString() + "_";
			string columnFrom = "";
			int columnSoldiers = 0;
			ctx.ReadValue(columnPrefix + "from", columnFrom);
			ctx.ReadValue(columnPrefix + "soldiers", columnSoldiers);
			if (host && !columnFrom.IsEmpty() && columnSoldiers > 0)
			{
				host.ReturnSent(columnFrom, columnSoldiers);
				returned += columnSoldiers;
			}
		}
		int trucks = 0;
		ctx.ReadValue("cmd_ma_tn", trucks);
		for (int k = 0; k < trucks; k++)
		{
			string truckPrefix = "cmd_ma_t" + k.ToString() + "_";
			string truckFrom = "";
			int truckLoad = 0;
			ctx.ReadValue(truckPrefix + "from", truckFrom);
			ctx.ReadValue(truckPrefix + "n", truckLoad);
			if (host && !truckFrom.IsEmpty() && truckLoad > 0)
			{
				host.ReturnSent(truckFrom, truckLoad);
				returned += truckLoad;
			}
		}
		m_bDirty = true;
		if (columns > 0 || trucks > 0)
		{
			string text = string.Format("Redémarrage : %1 colonne(s) et %2 camion(s) de renfort en route rendus à leur localité (%3 soldats)", columns, trucks, returned);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, text);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne : tout vidé (colonnes, papier, replis, raids, freins, pistes, horloges) —
	//! SRP_Commander.ResetCampaign
	void ResetCampaign(string reason)
	{
		EndAllRaids("nouvelle campagne");
		EndAllRetreats("nouvelle campagne");
		DropAttackPlans();
		ResetAttackState(0);
		m_aColumns.Clear();
		m_aSentTrucks.Clear();
		ClearPending();
		m_aGathered.Clear();
		m_aContactGroups.Clear();
		m_aContactZones.Clear();
		m_aContactQuiet.Clear();
		m_mLastReinforce.Clear();
		m_mRegionLastUnix.Clear();
		m_mRegionLastZone.Clear();
		m_mPathUnix.Clear();
		m_mPathCell.Clear();
		m_mZoneTrack.Clear();
		m_mRegionTrack.Clear();
		m_iLastHarassUnix = 0;
		m_iLastCaUnix = 0;
		m_iCaCount = 0;
		m_bCheckRetreatSoon = false;
		m_bDirty = true;
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Manœuvres remises à zéro : " + reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Après une restauration : papier, raids et replis oubliés ; colonnes et camions relus déjà rendus —
	//! SRP_Commander.OnRestored
	void OnRestored()
	{
		EndAllRaids("restauration");
		EndAllRetreats("restauration");
		DropAttackPlans();
		ResetAttackState(0);
		m_aGathered.Clear();
		m_aContactGroups.Clear();
		m_aContactZones.Clear();
		m_aContactQuiet.Clear();
		m_mPathCell.Clear();
		m_mZoneTrack.Clear();
		m_mRegionTrack.Clear();
		m_bDirty = true;
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! MA2, MA12, C2 : localité hostile non posée, non vidée, autre que celle du contact, à renfort_source_distance_m
	//! (renfort_source_distance_poche_m pour une poche), qui peut céder Effective - arrondi sup(Nominal x garde) >=
	//! renfort_chargement_min ; la plus proche ; load = min(wanted, céder) — RequestReinforcement
	protected bool PickSource(vector position, int zone, int wanted, out string source, out vector sourcePos, out int load)
	{
		source = "";
		sourcePos = vector.Zero;
		load = 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!front || !host || !territory)
			return false;
		// MA12 : contact ou localité visée hors de la masse rouge principale -> poche, source cherchée plus loin
		float reach = m_iSourceM;
		if (!host.IsInMainRedMass(front.CellIndexAt(position)))
			reach = m_iSourcePocketM;
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		float best = reach;
		foreach (SRP_SectorState state : localities)
		{
			if (!state || !state.m_Sector)
				continue;
			string name = state.m_sName;
			vector center = state.m_Sector.m_vCenter;
			float distance = vector.DistanceXZ(center, position);
			if (distance > best)
				continue;
			// La localité du contact elle-même ne s'envoie pas de renfort
			if (distance <= state.m_Sector.m_fRadius + 300)
				continue;
			// Jamais une localité tenue sous les yeux des joueurs, ni une localité vidée (MA7)
			if (host.IsGarrisonPosted(name) || IsLocalityEmptied(name))
				continue;
			if (front.GetZoneOwner(front.GetZoneAt(center)) != SRP_EFrontOwner.ROUGE)
				continue;
			int give = CanGive(name, center);
			if (give < m_iLoadMin)
				continue;
			best = distance;
			source = name;
			sourcePos = center;
			load = wanted;
			if (load > give)
				load = give;
		}
		return !source.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! C2 : soldats qu'une localité peut céder = Effective - arrondi supérieur de (Nominal x garde) - soldats des camions
	//! déjà demandés à cette source et pas encore partis (débités seulement au départ du camion, soldats assis) ; garde des deux tiers avec un
	//! officier prudent (CO7) — PickSource, RequestColumn
	protected int CanGive(string locality, vector position)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host)
			return 0;
		int nominal = host.GetLocalityNominal(locality);
		int effective = host.GetLocalityEffective(locality);
		int keep = m_iSourceKeepCent;
		int region = RegionAt(position);
		SRP_CmdRegionBook book = Book();
		if (book && region >= 0 && book.GetOfficerTrait(region) == SRP_ECmdTrait.PRUDENT)
			keep = m_iSourceKeepPrudentCent;
		int kept = (nominal * keep + 99) / 100;
		return effective - kept - PendingFrom(locality, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! C2 : un camion hors colonne vient d'être demandé (DispatchFrom, SendContactTruck) ; sa source n'est débitée qu'au
	//! départ du camion (OnTruckLaunched, soldats assis), parfois plusieurs minutes plus tard : ses soldats sont retenus d'ici là (CanGive).
	//! Un camion jamais posé (abandonné, annulé) est oublié après max(délai, 10 min) + 20 min
	protected void NotePending(string source, int load, int delay, int nowUnix)
	{
		if (source.IsEmpty() || load <= 0)
			return;
		m_aPendingFrom.Insert(source);
		m_aPendingLoad.Insert(load);
		m_aPendingUntil.Insert(nowUnix + Math.MaxInt(delay, 600) + PENDING_MARGIN_S);
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats des camions demandés à cette source et pas encore posés (demandes trop vieilles oubliées au passage)
	protected int PendingFrom(string source, int nowUnix)
	{
		int total = 0;
		for (int i = m_aPendingFrom.Count() - 1; i >= 0; i--)
		{
			if (m_aPendingUntil[i] <= nowUnix)
			{
				RemovePendingAt(i);
				continue;
			}
			if (m_aPendingFrom[i] == source)
				total += m_aPendingLoad[i];
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemovePendingAt(int index)
	{
		if (index < 0 || index >= m_aPendingFrom.Count())
			return;
		m_aPendingFrom.RemoveOrdered(index);
		if (index < m_aPendingLoad.Count())
			m_aPendingLoad.RemoveOrdered(index);
		if (index < m_aPendingUntil.Count())
			m_aPendingUntil.RemoveOrdered(index);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPending()
	{
		m_aPendingFrom.Clear();
		m_aPendingLoad.Clear();
		m_aPendingUntil.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! MA4 : nouvelle colonne sur la route source -> destination (ou contact) ; débite AddSent(fromLocality, soldiers)
	//! AU DÉPART (papier ou réelle, seul débit d'une colonne) ; rend son numéro — renforts, replis, Staff (COLONNE)
	protected int NewColumn(string fromLocality, string toLocality, int soldiers, int kind, int zone, vector contact, int nowUnix)
	{
		SRP_CmdColumn column = new SRP_CmdColumn();
		column.m_iId = m_iNextColumnId;
		m_iNextColumnId++;
		column.m_iKind = kind;
		column.m_iState = SRP_ECmdColumnState.PAPIER;
		column.m_sFrom = fromLocality;
		column.m_sTo = toLocality;
		column.m_iSoldiers = soldiers;
		column.m_iZone = zone;
		column.m_vContact = contact;
		column.m_fSpeed = m_fColumnSpeed;
		column.m_iDepartUnix = nowUnix;
		vector start = LocalityPoint(fromLocality);
		vector end = contact;
		if (!toLocality.IsEmpty())
			end = LocalityPoint(toLocality);
		if (start == vector.Zero)
			start = end;
		BuildColumnRoute(column, start, end);
		m_aColumns.Insert(column);
		if (soldiers > 0 && !fromLocality.IsEmpty())
		{
			SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
			if (host)
				host.AddSent(fromLocality, soldiers);
		}
		m_bDirty = true;
		if (soldiers > 0)
		{
			string destination = toLocality;
			if (destination.IsEmpty())
				destination = "le contact " + SRP_CmdScreens.CellText(contact);
			// La décision (renfort, colonne du Staff) est écrite par son auteur ; ici, la ligne d'exécution
			int meters = Math.Round(column.m_fLength);
			string text = string.Format("colonne n°%1 (%2) : %3 soldats de %4 vers %5, %6 m", column.m_iId, ColumnKindWord(kind), soldiers, fromLocality, destination, meters);
			SRP_CmdLog.Note(RegionAt(start), SRP_ECmdLogKind.DECISION, text);
		}
		return column.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Route d'une colonne : départ, points de route (colonne_pas_route_m), arrivée ; longueur à plat — NewColumn,
	//! réorientation, décrochage
	protected void BuildColumnRoute(SRP_CmdColumn column, vector start, vector end)
	{
		column.m_aRoute.Clear();
		array<vector> middle = {};
		column.m_fLength = RouteLength(start, end, middle);
		column.m_aRoute.Insert(start);
		foreach (vector point : middle)
			column.m_aRoute.Insert(point);
		column.m_aRoute.Insert(end);
	}

	//------------------------------------------------------------------------------------------------
	//! Longueur à plat de la route de « from » à « to » (points de route dans « route », extrémités non comprises)
	protected float RouteLength(vector from, vector to, notnull array<vector> route)
	{
		route.Clear();
		SRP_Placement.BuildRoadRoute(from, to, m_iColumnRoadStepM, 0, route);
		float length = 0;
		vector previous = from;
		foreach (vector point : route)
		{
			length += vector.DistanceXZ(previous, point);
			previous = point;
		}
		length += vector.DistanceXZ(previous, to);
		return length;
	}

	//------------------------------------------------------------------------------------------------
	//! Position d'une colonne sur sa route (interpolation a + (b - a) x t, jamais vector + float) — ColumnsTick
	protected vector PositionAt(SRP_CmdColumn column, int nowUnix)
	{
		if (!column || column.m_aRoute.IsEmpty())
			return vector.Zero;
		if (column.m_iDepartUnix <= 0)
			return column.m_aRoute[0];
		return PointAtDistance(column, TravelledAt(column, nowUnix));
	}

	//------------------------------------------------------------------------------------------------
	//! Distance parcourue sur la route, bornée à sa longueur
	protected float TravelledAt(SRP_CmdColumn column, int nowUnix)
	{
		if (column.m_iDepartUnix <= 0)
			return 0;
		float travelled = column.m_fSpeed * (nowUnix - column.m_iDepartUnix);
		if (travelled > column.m_fLength)
			travelled = column.m_fLength;
		if (travelled < 0)
			travelled = 0;
		return travelled;
	}

	//------------------------------------------------------------------------------------------------
	//! Le point de la route à « distance » mètres du départ (au sol)
	protected vector PointAtDistance(SRP_CmdColumn column, float distance)
	{
		int count = column.m_aRoute.Count();
		if (count == 0)
			return vector.Zero;
		if (distance <= 0 || count == 1)
			return column.m_aRoute[0];
		float walked = 0;
		for (int i = 0; i < count - 1; i++)
		{
			vector a = column.m_aRoute[i];
			vector b = column.m_aRoute[i + 1];
			float segment = vector.DistanceXZ(a, b);
			if (segment < 0.01)
				continue;
			if (walked + segment >= distance)
			{
				float t = (distance - walked) / segment;
				return SRP_Placement.OnGround(a + (b - a) * t);
			}
			walked += segment;
		}
		return column.m_aRoute[count - 1];
	}

	//------------------------------------------------------------------------------------------------
	//! Les points de la route au-delà de « distance » mètres (route qui reste), arrivée comprise
	protected void RemainingRoute(SRP_CmdColumn column, float distance, notnull array<vector> remaining)
	{
		remaining.Clear();
		float walked = 0;
		int count = column.m_aRoute.Count();
		for (int i = 1; i < count; i++)
		{
			walked += vector.DistanceXZ(column.m_aRoute[i - 1], column.m_aRoute[i]);
			if (walked > distance)
				remaining.Insert(column.m_aRoute[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Colonnes : arrivée sur le papier -> AddBonus(destination, soldats), rien n'est rendu à la source ; colonne vers un
	//! contact arrivée sur le papier ou annulée (destination prise, Commandeur gelé) -> ReturnSent(source, soldats) ;
	//! passage au réel à colonne_distance_m d'un joueur (Materialize) — Tick
	protected void ColumnsTick(int nowUnix)
	{
		for (int i = m_aColumns.Count() - 1; i >= 0; i--)
		{
			SRP_CmdColumn column = m_aColumns[i];
			if (!column)
			{
				m_aColumns.RemoveOrdered(i);
				continue;
			}
			// Colonne de repli qui attend ses premiers soldats (StartRetreat) : rien à faire ici
			if (column.m_iDepartUnix <= 0)
				continue;
			float travelled = TravelledAt(column, nowUnix);
			int paperLeft = column.m_iSoldiers - column.m_iAboard;
			if (column.m_iState == SRP_ECmdColumnState.PAPIER)
			{
				if (travelled >= column.m_fLength)
				{
					ArriveOnPaper(column, column.m_iSoldiers);
					m_aColumns.RemoveOrdered(i);
					continue;
				}
				if (column.m_iSoldiers > 0 && nowUnix >= column.m_iNextTryUnix && IsRouteNearPlayers(column, travelled))
					Materialize(column, nowUnix);
				continue;
			}
			// Réelle : les soldats sans camion finissent la route sur le papier
			if (paperLeft > 0 && travelled >= column.m_fLength)
			{
				ArriveOnPaper(column, paperLeft);
				column.m_iSoldiers -= paperLeft;
				m_bDirty = true;
			}
			if (CheckColumnDone(column))
				continue;
			// Camions sans nouvelles (supprimés hors de notre suivi) : la colonne est oubliée au bout de 90 min
			if (column.m_iRealUnix > 0 && nowUnix - column.m_iRealUnix >= 5400)
			{
				SRP_CmdLog.Note(RegionAt(PositionAt(column, nowUnix)), SRP_ECmdLogKind.DECISION, string.Format("colonne n°%1 de %2 : camions sans nouvelles, colonne close", column.m_iId, column.m_sFrom));
				m_aColumns.RemoveOrdered(i);
				m_bDirty = true;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur (délai de grâce compris) est-il à colonne_distance_m de la position ou de la route qui reste ?
	protected bool IsRouteNearPlayers(SRP_CmdColumn column, float travelled)
	{
		if (m_aPlayers.IsEmpty())
			return false;
		if (SRP_Placement.NearestPlayer(PointAtDistance(column, travelled), m_aPlayers) <= m_iColumnRealM)
			return true;
		array<vector> remaining = {};
		RemainingRoute(column, travelled, remaining);
		foreach (vector point : remaining)
		{
			if (SRP_Placement.NearestPlayer(point, m_aPlayers) <= m_iColumnRealM)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Arrivée sur le papier : localité visée -> AddBonus (plafonné à 60 par la façade) ; vers un contact -> ReturnSent
	//! (les soldats rentrent chez eux)
	protected void ArriveOnPaper(SRP_CmdColumn column, int soldiers)
	{
		if (!column || soldiers <= 0)
			return;
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host)
			return;
		string text;
		if (!column.m_sTo.IsEmpty())
		{
			host.AddBonus(column.m_sTo, soldiers);
			text = string.Format("colonne n°%1 arrivée : %2 soldats de %3 renforcent %4", column.m_iId, soldiers, column.m_sFrom, column.m_sTo);
		}
		else
		{
			if (!column.m_sFrom.IsEmpty())
				host.ReturnSent(column.m_sFrom, soldiers);
			text = string.Format("colonne n°%1 : contact perdu, %2 soldats rentrent à %3", column.m_iId, soldiers, column.m_sFrom);
		}
		m_bDirty = true;
		SRP_CmdLog.Note(RegionAt(column.m_vContact), SRP_ECmdLogKind.DECISION, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Colonne réelle finie (tous ses camions revenus, plus personne sur le papier) : ARRIVEE ou PERDUE, retirée ;
	//! rend vrai si elle est retirée
	protected bool CheckColumnDone(SRP_CmdColumn column)
	{
		if (!column || column.m_iState != SRP_ECmdColumnState.REELLE)
			return false;
		if (column.m_iTrucksDone < column.m_iTrucks || column.m_iSoldiers - column.m_iAboard > 0)
			return false;
		if (column.m_iDelivered > 0)
			column.m_iState = SRP_ECmdColumnState.ARRIVEE;
		else
			column.m_iState = SRP_ECmdColumnState.PERDUE;
		string destination = column.m_sTo;
		if (destination.IsEmpty())
			destination = "le contact";
		string text;
		if (column.m_iDelivered > 0)
			text = string.Format("colonne n°%1 livrée : %2 soldats débarqués sur %3 vers %4", column.m_iId, column.m_iDelivered, column.m_iAboard, destination);
		else
			text = string.Format("colonne n°%1 perdue en route vers %2 : aucun soldat en état de combattre", column.m_iId, destination);
		SRP_CmdLog.Note(RegionAt(column.m_vContact), SRP_ECmdLogKind.DECISION, text);
		m_aColumns.RemoveItemOrdered(column);
		m_bDirty = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Passage au réel hors de vue (recul par colonne_recul_pas_m) : SRP_EnemyTruckComponent.LaunchColumn — ColumnsTick
	protected bool Materialize(SRP_CmdColumn column, int nowUnix)
	{
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (!column || !trucks || column.m_iSoldiers <= 0)
			return false;
		float travelled = TravelledAt(column, nowUnix);
		// Point de pose : la position, sinon en reculant sur la route déjà faite (jamais en avant)
		float back = 0;
		bool found = false;
		vector point;
		while (back <= m_iColumnBackMaxM)
		{
			float along = travelled - back;
			if (along < 0)
				along = 0;
			point = PointAtDistance(column, along);
			if (!SRP_Placement.IsWater(point) && SRP_Placement.NearestPlayer(point, m_aPlayers) >= m_iColumnPlayerMinM && !SRP_Placement.IsSeenByAnyPlayer(point, m_aPlayers))
			{
				found = true;
				break;
			}
			if (along <= 0)
				break;
			back += m_iColumnBackStepM;
		}
		if (!found)
		{
			column.m_iNextTryUnix = nowUnix + m_iColumnRetryS;
			return false;
		}
		float start = travelled - back;
		if (start < 0)
			start = 0;
		array<vector> remaining = {};
		RemainingRoute(column, start, remaining);

		// Un camion par colonne_soldats_camion soldats, colonne_camions_max au plus ; le reste finit sur le papier
		int perTruck = m_iColumnSoldiersPerTruck;
		int wantedTrucks = (column.m_iSoldiers + perTruck - 1) / perTruck;
		if (wantedTrucks > m_iColumnTrucksMax)
			wantedTrucks = m_iColumnTrucksMax;
		int left = column.m_iSoldiers - column.m_iAboard;
		int launched = 0;
		int aboard = 0;
		for (int k = 0; k < wantedTrucks && left > 0; k++)
		{
			int load = perTruck;
			if (load > left)
				load = left;
			// Le 2e camion se pose 40 m derrière le premier, sur la route déjà faite
			float spot = start - 40 * k;
			if (spot < 0)
				spot = 0;
			vector at = point;
			if (k > 0)
				at = PointAtDistance(column, spot);
			int posted = trucks.LaunchColumn(column, at, remaining, load);
			if (posted <= 0)
				break;
			launched += posted;
			aboard += load;
			left -= load;
		}
		if (launched <= 0)
		{
			// Refus des camions (place, 4 camions posés) : la colonne reste sur le papier, nouvel essai
			column.m_iNextTryUnix = nowUnix + m_iColumnRetryS;
			return false;
		}
		column.m_iState = SRP_ECmdColumnState.REELLE;
		column.m_iTrucks += launched;
		column.m_iAboard += aboard;
		column.m_iRealUnix = nowUnix;
		m_bDirty = true;
		string destination = column.m_sTo;
		if (destination.IsEmpty())
			destination = "le contact";
		string text = string.Format("colonne n°%1 vers %2 : %3 camion(s), %4 soldats à bord, %5 restent sur le papier", column.m_iId, destination, launched, aboard, left);
		SRP_CmdLog.Note(RegionAt(point), SRP_ECmdLogKind.DECISION, text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats d'une colonne encore débités à sa source et jamais rendus — RE13 (WriteTo), CancelAll : le papier, plus
	//! ceux des camions pas encore revenus. Camion à l'embarquement : ses soldats demandés (rien n'est encore rendu) ;
	//! camion parti : ses seuls soldats assis, car ceux qui ne sont pas partis ont déjà été rendus par le camion
	//! (SettleDeparture) et retirés de m_iAboard (OnColumnTruckDeparted) ; camion revenu (livré ou perdu) : plus rien,
	//! sa part exacte est dans m_iAboardDone (OnColumnTruckDone). Aucun soldat n'est rendu deux fois.
	protected int ColumnPending(SRP_CmdColumn column)
	{
		if (!column)
			return 0;
		int trucksAboard = Math.MaxInt(column.m_iAboard, 0);
		int paper = column.m_iSoldiers - trucksAboard;
		if (paper < 0)
			paper = 0;
		int aboard = Math.MaxInt(0, trucksAboard - column.m_iAboardDone);
		return paper + aboard;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdColumn FindColumn(int columnId)
	{
		if (columnId <= 0)
			return null;
		foreach (SRP_CmdColumn column : m_aColumns)
		{
			if (column && column.m_iId == columnId)
				return column;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! « Colonne n°3 de Chotain vers Régina : 12 soldats, sur le papier, 2400 m restants »
	protected string ColumnLine(SRP_CmdColumn column, int nowUnix)
	{
		string destination = column.m_sTo;
		if (destination.IsEmpty())
			destination = "le contact";
		string stateText = "sur le papier";
		if (column.m_iDepartUnix <= 0)
			stateText = "attend ses soldats";
		else if (column.m_iState == SRP_ECmdColumnState.REELLE)
			stateText = string.Format("en camions (%1 sur %2 revenus)", column.m_iTrucksDone, column.m_iTrucks);
		int remainingMeters = Math.Round(column.m_fLength - TravelledAt(column, nowUnix));
		return string.Format("Colonne n°%1 de %2 vers %3 : %4 soldats, %5, %6 m restants", column.m_iId, column.m_sFrom, destination, column.m_iSoldiers, stateText, remainingMeters);
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : unités sur le papier (avance, passage au réel, SRP_FrontCapture.SetPaperAssault) — Tick
	protected void PaperTick(int nowUnix)
	{
		if (m_aPaper.IsEmpty())
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!front || !host)
			return;
		SRP_CounterAttack attack = host.GetCounterAttack();
		SRP_FrontCapture capture = front.GetCapture();
		for (int i = m_aPaper.Count() - 1; i >= 0; i--)
		{
			SRP_CmdPaperUnit unit = m_aPaper[i];
			if (!unit || unit.m_iSoldiers <= 0 || !attack || attack.m_iZone != unit.m_iZone)
			{
				m_aPaper.Remove(i);
				continue;
			}
			// a) Un joueur à ca_papier_distance_m : l'unité devient réelle, hors de vue
			float nearest = SRP_Placement.NearestPlayer(unit.m_vPos, m_aPlayers);
			if (nearest <= m_iPaperM)
			{
				if (nowUnix >= unit.m_iNextTryUnix && MaterializePaper(unit, attack, nowUnix))
					m_aPaper.Remove(i);
				continue;
			}
			// b) Avance vers le carré reprenable, même règle que les groupes réels (vector * float, jamais + float)
			int cell = PickRetakeCell(unit.m_iZone, unit.m_vPos, attack.m_vTarget);
			unit.m_iObjCell = cell;
			vector goal = attack.m_vTarget;
			if (cell >= 0)
				goal = front.CellCenter(cell);
			vector step = goal - unit.m_vPos;
			step[1] = 0;
			float length = step.Length();
			int elapsed = nowUnix - unit.m_iLastUnix;
			unit.m_iLastUnix = nowUnix;
			if (length > 1 && elapsed > 0)
			{
				float move = m_fFootSpeed * elapsed;
				if (move > length)
					move = length;
				unit.m_vPos = SRP_Placement.OnGround(unit.m_vPos + step * (move / length));
			}
			// c) Dans un carré bleu reprenable de la zone, sans joueur à ca_papier_joueur_min_m : assaillants déclarés
			int here = front.CellIndexAt(unit.m_vPos);
			if (capture && here >= 0 && nearest > m_iPaperPlayerMinM && IsRetakeCell(front, here, unit.m_iZone))
				capture.SetPaperAssault(here, unit.m_iSoldiers, unit.m_iZone, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : une unité sur le papier devient réelle (point hors de vue, sinon recul vers son départ par 100 m, 600 m au
	//! plus ; sections de 6 puis de 4, reste en binôme ; place COMBAT) et rejoint le plan de sa vague — PaperTick
	protected bool MaterializePaper(SRP_CmdPaperUnit unit, SRP_CounterAttack attack, int nowUnix)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!enemies || !front || !unit || !attack)
			return false;
		vector spawn;
		bool found = SRP_Placement.FindSpawnPosition(unit.m_vPos, 0, 150, m_aPlayers, false, true, spawn, 300);
		if (!found)
		{
			vector back = unit.m_vOrigin - unit.m_vPos;
			back[1] = 0;
			float backLength = back.Length();
			if (backLength > 1)
			{
				vector backDir = back * (1 / backLength);
				for (int k = 1; k <= 6 && !found; k++)
				{
					float reach = 100.0 * k;
					if (reach > backLength)
						reach = backLength;
					vector probe = SRP_Placement.OnGround(unit.m_vPos + backDir * reach);
					found = SRP_Placement.FindSpawnPosition(probe, 0, 100, m_aPlayers, false, true, spawn, 300);
				}
			}
		}
		if (!found)
		{
			unit.m_iNextTryUnix = nowUnix + 10;
			return false;
		}
		array<ResourceName> prefabs = {};
		PlanPaperGroups(enemies, unit.m_iSoldiers, prefabs);
		if (prefabs.IsEmpty())
		{
			unit.m_iNextTryUnix = nowUnix + 10;
			return false;
		}
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string key = "papier-" + front.GetZoneCode(unit.m_iZone);
		if (capacity)
		{
			string refusal = capacity.Ask(key, SRP_ECmdCapClass.COMBAT, unit.m_iSoldiers, prefabs.Count(), spawn, "territoire");
			if (!refusal.IsEmpty())
			{
				unit.m_iNextTryUnix = nowUnix + 10;
				return false;
			}
		}
		int cell;
		vector goal = AttackObjective(attack, spawn, cell);
		array<ref SRP_EnemyGroup> posted = {};
		foreach (ResourceName prefab : prefabs)
		{
			SRP_EnemyGroup record = enemies.SpawnGroup(spawn, false, goal, "territoire", -1, vector.Zero, -1, false, prefab);
			if (!record)
				continue;
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.COMBAT);
			enemies.NoteHome(record, "assaut", false);
			record.m_iAssaultZone = attack.m_iZone;
			record.m_iCmdRole = SRP_ECmdRole.ASSAUT;
			record.m_vCmdLast = spawn;
			record.m_iCmdStillUnix = nowUnix;
			attack.m_aGroups.Insert(record);
			if (unit.m_iWave == attack.m_iWaves)
				attack.m_aLastWave.Insert(record);
			posted.Insert(record);
		}
		if (posted.IsEmpty())
		{
			unit.m_iNextTryUnix = nowUnix + 10;
			return false;
		}
		// Doctrine : le plan de sa vague, sinon un plan neuf
		SRP_CmdAssault plan = PlanOfWave(unit.m_iWave, attack.m_iZone);
		if (plan)
		{
			AddToPlan(plan, posted, nowUnix);
		}
		else
		{
			plan = BuildAssault(posted, goal, unit.m_vOrigin, attack.m_vOriginB, false, attack.m_iZone);
			plan.m_iWave = unit.m_iWave;
			plan.m_iObjCell = cell;
			m_aAssaults.Insert(plan);
		}
		string text = string.Format("vague %1 sur %2 : %3 soldats sortent du papier près du carré %4 (%5 section(s))", unit.m_iWave, attack.m_sZone, unit.m_iSoldiers, SRP_CmdScreens.CellText(spawn), posted.Count());
		SRP_CmdLog.Note(attack.m_iRegion, SRP_ECmdLogKind.DECISION, text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sections de 6 puis de 4 (prefabs de section), reste en binôme (sentinelles), pour « soldiers » soldats
	protected void PlanPaperGroups(SRP_EnemyComponent enemies, int soldiers, notnull array<ResourceName> prefabs)
	{
		prefabs.Clear();
		array<ResourceName> sections = enemies.GetSectionPrefabs();
		int left = soldiers;
		int guard = 0;
		while (left >= 4 && guard < 12)
		{
			guard++;
			int biggest = left;
			if (biggest > 6)
				biggest = 6;
			ResourceName pick = "";
			if (biggest >= 6)
				pick = enemies.PickPrefabOfSize(sections, 6, 6);
			if (pick.IsEmpty())
				pick = enemies.PickPrefabOfSize(sections, 4, biggest);
			if (pick.IsEmpty())
				pick = enemies.PickSectionPrefab();
			if (pick.IsEmpty())
				break;
			int size = enemies.GroupSize(pick);
			if (size <= 0)
				size = 4;
			prefabs.Insert(pick);
			left -= size;
		}
		if (left > 0 && guard < 12)
		{
			ResourceName pair = enemies.PickPrefabOfSize(enemies.GetSentryPrefabs(), 1, 3);
			if (pair.IsEmpty())
				pair = enemies.PickLightPrefab();
			if (!pair.IsEmpty())
				prefabs.Insert(pair);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : un groupe d'assaut passe sur le papier (soldats en état de combattre, position du groupe), puis il est
	//! retiré (OnGroupGone le sort de nos listes) — AwakeTick, DriveAssault
	protected void GroupToPaper(SRP_EnemyComponent enemies, SRP_CounterAttack attack, SRP_EnemyGroup record, int nowUnix)
	{
		if (!enemies || !attack || !record)
			return;
		int fit = SRP_EnemyComponent.FitOf(record);
		SRP_CmdAssault plan = PlanOf(record);
		SRP_CmdPaperUnit unit = new SRP_CmdPaperUnit();
		unit.m_iZone = attack.m_iZone;
		unit.m_iWave = attack.m_iWaves;
		if (plan && plan.m_iWave > 0)
			unit.m_iWave = plan.m_iWave;
		unit.m_iSoldiers = fit;
		unit.m_vPos = enemies.GroupPosition(record);
		unit.m_vOrigin = OriginOf(record, attack);
		unit.m_iLastUnix = nowUnix;
		unit.m_iNextTryUnix = nowUnix + 30;
		if (fit > 0)
			m_aPaper.Insert(unit);
		attack.m_aGroups.RemoveItem(record);
		attack.m_aLastWave.RemoveItem(record);
		attack.m_aArrived.RemoveItem(record);
		SRP_EnemyComponent.ReleaseAwake(record);
		record.m_bAwake = false;
		record.m_iAssaultZone = -1;
		enemies.Delete(record);
		string text = string.Format("vague %1 sur %2 : %3 soldats loin des joueurs repassent sur le papier (%4)", unit.m_iWave, attack.m_sZone, fit, SRP_CmdScreens.CellText(unit.m_vPos));
		SRP_CmdLog.Note(attack.m_iRegion, SRP_ECmdLogKind.DECISION, text);
	}

	//------------------------------------------------------------------------------------------------
	//! MA5 : sections usées repliées au centre, garnison au tiers -> StartRetreat — Tick (60 s)
	protected void RetreatCheckTick(int nowUnix)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!territory || !host || !enemies || !front || !m_Commander)
			return;
		PruneGathered(host);
		int tick = System.GetTickCount();
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		foreach (SRP_SectorState state : localities)
		{
			if (!state || !state.m_Sector)
				continue;
			string name = state.m_sName;
			if (!host.IsGarrisonPosted(name))
				continue;
			vector center = state.m_Sector.m_vCenter;

			// Sections repliées arrivées au centre : elles le défendent sur repli_centre_rayon_m
			foreach (SRP_EnemyGroup arriving : state.m_aGroups)
			{
				if (!arriving || arriving.m_iCmdRole != SRP_ECmdRole.REPLI || arriving.m_sTask != "repli")
					continue;
				if (SRP_EnemyComponent.FitOf(arriving) <= 0)
					continue;
				vector here = enemies.GroupPosition(arriving);
				if (vector.DistanceXZ(here, center) > m_iRetreatCentreM + 20)
				{
					if (nowUnix - arriving.m_iCmdStillUnix >= 300)
					{
						enemies.OrderForcedMove(arriving, center, m_iRetreatCentreM);
						arriving.m_iCmdStillUnix = nowUnix;
					}
					continue;
				}
				enemies.ConvertToGarrison(arriving, center, m_iRetreatCentreM);
				enemies.NoteHome(arriving, "centre", true);
				arriving.m_iCmdRole = SRP_ECmdRole.AUCUN;
			}

			// Un décrochage, ou un regroupement au centre sans décrochage, par pose
			if (!m_bRetreatOn || state.m_bRetreatDone || m_aGathered.Find(name) >= 0)
				continue;
			// Posée, installée et au contact (contact connu depuis moins de repli_contact_min)
			if (!SRP_EnemyComponent.IsSettled(state.m_aGroups, tick))
				continue;
			int zone = front.GetZoneAt(center);
			vector contact;
			int contactUnix;
			if (!m_Commander.LastKnownContact(zone, m_iRetreatContactMin * 60, contact, contactUnix))
				continue;
			int region = RegionAt(center);
			SRP_EnemyGroup hq = SRP_EnemyGarrison.FindTownHQ(enemies, state);

			// a) Sections usées : un groupe du plan (ni centre ni QG) réduit au tiers se replie au centre
			foreach (SRP_EnemyGroup section : state.m_aGroups)
			{
				if (!section || section == hq || section.m_iCmdRole != SRP_ECmdRole.AUCUN)
					continue;
				if (!SRP_EnemyGarrison.IsPlanTask(section.m_sHomeTask) || section.m_sHomeTask == "centre" || SRP_EnemyGarrison.IsCentreGroup(section))
					continue;
				int fit = SRP_EnemyComponent.FitOf(section);
				if (fit < 1 || fit * 100 > m_iRetreatSectionCent * section.m_iExpected)
					continue;
				SendSectionToCentre(enemies, section, center, nowUnix);
				string sectionText = string.Format("une section de %1 réduite à %2 soldats sur %3 se replie au centre", name, fit, section.m_iExpected);
				SRP_CmdLog.Decision(region, false, sectionText, "section usée (#40)", FREE, "");
			}

			// b) Garnison au tiers : décrochage de tous sauf le QG
			int survivors = SRP_EnemyGarrison.CountPlanSurvivors(state);
			int planned = state.m_iPlannedSoldiers;
			if (planned <= 0 || survivors * 100 > m_iRetreatGarrisonCent * planned)
				continue;
			string retreatReason = string.Format("%1 défenseurs debout sur %2 prévus, au contact (#40)", survivors, planned);
			if (StartRetreat(state, contact, nowUnix))
				SRP_CmdLog.Decision(region, false, "décrochage de " + name, retreatReason, FREE, "");
			else
				SRP_CmdLog.Decision(region, false, "regroupement au centre de " + name, retreatReason, FREE, "");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MA5 : une section usée part au centre (ForcedMove, puis défense du centre à l'arrivée, RetreatCheckTick) ; son
	//! poste d'origine reste celui du plan (elle compte toujours dans les survivants de la garnison) ; rôle REPLI :
	//! exclue de l'entraide pendant le trajet
	protected void SendSectionToCentre(SRP_EnemyComponent enemies, SRP_EnemyGroup section, vector center, int nowUnix)
	{
		enemies.OrderForcedMove(section, center, m_iRetreatCentreM);
		section.m_iCmdRole = SRP_ECmdRole.REPLI;
		section.m_sTask = "repli";
		section.m_iCmdStillUnix = nowUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! MA6 : décrochage sous fumigènes (SRP_CmdSupport.RequestSmoke), binôme de couverture, QG qui reste, pertes et
	//! envoyés notés, localité vidée, colonne papier vers la voisine — RetreatCheckTick, RequestRetreat
	protected bool StartRetreat(SRP_SectorState state, vector contact, int nowUnix)
	{
		if (!state || !state.m_Sector || state.m_bRetreatDone)
			return false;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!enemies || !front || !host)
			return false;
		string name = state.m_sName;
		vector center = state.m_Sector.m_vCenter;
		float radius = state.m_Sector.m_fRadius;
		int zone = front.GetZoneAt(center);
		int region = RegionAt(center);
		SRP_EnemyGroup hq = SRP_EnemyGarrison.FindTownHQ(enemies, state);
		int planned = state.m_iPlannedSoldiers;
		int survivors = SRP_EnemyGarrison.CountPlanSurvivors(state);

		// Ceux qui partent : tous les groupes en état de combattre, sauf le QG d'une ville (il tient jusqu'au bout)
		array<ref SRP_EnemyGroup> leaving = {};
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (!record || record == hq)
				continue;
			if (SRP_EnemyComponent.FitOf(record) < 1)
				continue;
			leaving.Insert(record);
		}
		// Personne en état de partir : rien ne décroche. m_bRetreatDone reste faux (sans décrochage, les pertes de la
		// garnison sont notées à son retrait par le territoire, F12) ; la localité n'est plus contrôlée jusqu'à sa
		// prochaine pose (m_aGathered)
		if (leaving.IsEmpty())
		{
			if (m_aGathered.Find(name) < 0)
				m_aGathered.Insert(name);
			return false;
		}
		SRP_SectorState to = PickRetreatTarget(state, contact);
		if (!to)
		{
			// Aucune voisine : les défenseurs restants se regroupent au centre, une fois par pose, on s'arrête là (pas de
			// décrochage : m_bRetreatDone reste faux, le territoire notera les pertes au retrait)
			if (m_aGathered.Find(name) >= 0)
				return false;
			m_aGathered.Insert(name);
			foreach (SRP_EnemyGroup gathered : leaving)
			{
				if (gathered.m_iCmdRole == SRP_ECmdRole.AUCUN && gathered.m_sHomeTask != "centre")
					SendSectionToCentre(enemies, gathered, center, nowUnix);
			}
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, string.Format("les défenseurs de %1 se regroupent au centre : aucune localité de repli à portée", name));
			return false;
		}
		// Un seul décrochage par pose ; pertes et partants notés ci-dessous (le territoire ne les note plus au retrait)
		state.m_bRetreatDone = true;
		vector toCenter = to.m_Sector.m_vCenter;
		vector exitPoint = RetreatExit(center, radius, toCenter);

		// Couverture : le groupe qui a le moins de soldats en état de combattre (1 au moins)
		SRP_EnemyGroup cover = null;
		int coverFit = 1000;
		foreach (SRP_EnemyGroup candidate : leaving)
		{
			int candidateFit = SRP_EnemyComponent.FitOf(candidate);
			if (candidateFit >= 1 && candidateFit < coverFit)
			{
				cover = candidate;
				coverFit = candidateFit;
			}
		}

		SRP_CmdRetreat retreat = new SRP_CmdRetreat();
		retreat.m_sLocality = name;
		retreat.m_sTo = to.m_sName;
		retreat.m_vExit = exitPoint;
		retreat.m_vContact = contact;
		retreat.m_iStartUnix = nowUnix;
		int departing = 0;
		int moving = 0;
		foreach (SRP_EnemyGroup mover : leaving)
		{
			int moverFit = SRP_EnemyComponent.FitOf(mover);
			moving += moverFit;
			if (SRP_EnemyGarrison.IsPlanTask(mover.m_sHomeTask))
				departing += moverFit;
			mover.m_vCmdLast = enemies.GroupPosition(mover);
			mover.m_iCmdStillUnix = nowUnix;
			mover.m_iAssaultZone = -1;
			if (mover == cover)
			{
				enemies.OrderSuppress(mover, contact, m_iRetreatCoverS, 1.5);
				mover.m_iCmdRole = SRP_ECmdRole.COUVERTURE;
				retreat.m_Cover = mover;
				continue;
			}
			// Fumée, ForcedMove vers la sortie, puis Move vers la voisine
			enemies.OrderRetreat(mover, mover.m_vCmdLast, m_iRetreatGrenades, exitPoint, toCenter);
			mover.m_iCmdRole = SRP_ECmdRole.REPLI;
			retreat.m_aGroups.Insert(mover);
		}

		// Rideau de fumigènes du mortier de la région, s'il est permis (C5 : gratuit)
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		string smokeText = "sans obus fumigènes";
		if (support && region >= 0)
		{
			string smoke = support.RequestSmoke(region, center, contact, "décrochage de " + name, false);
			if (smoke.IsEmpty())
				smokeText = "rideau de fumigènes du mortier";
			else
				SRP_CmdLog.Refusal(region, "rideau de fumée sur " + name, smoke);
		}

		// Tous sauf le QG sortent des défenseurs : la sirène et la capture ne les voient plus (la localité reste
		// « posée », même vide)
		for (int i = state.m_aGroups.Count() - 1; i >= 0; i--)
		{
			if (state.m_aGroups[i] != hq)
				state.m_aGroups.Remove(i);
		}
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (trucks)
			trucks.RecallSector(name);

		// F12 : pertes au combat, puis soldats partis (débités ici, une seule fois : la colonne de repli ne débite rien)
		int losses = planned - survivors;
		if (losses > 0)
			host.AddLosses(name, losses);
		if (departing > 0)
			host.AddSent(name, departing);
		SRP_FrontLocality loc = FrontLocality(name);
		if (loc)
			loc.m_iEmptiedAt = nowUnix;

		// Colonne sur le papier vers la voisine, ouverte à 0 soldat : elle part au premier soldat effacé
		int columnId = NewColumn(name, to.m_sName, 0, SRP_ECmdColumnKind.REPLI, zone, contact, nowUnix);
		SRP_CmdColumn column = FindColumn(columnId);
		if (column)
		{
			BuildColumnRoute(column, exitPoint, toCenter);
			column.m_iDepartUnix = 0;
		}
		retreat.m_iColumnId = columnId;
		m_aRetreats.Insert(retreat);
		m_bDirty = true;
		// Ligne d'exécution (la décision est écrite par RetreatCheckTick, ou par le cerveau pour un ordre DECROCHAGE)
		string text;
		if (hq)
			text = string.Format("les défenseurs de %1 décrochent sous fumigènes vers %2 (%3 soldats, %4), le QG reste", name, to.m_sName, moving, smokeText);
		else
			text = string.Format("les défenseurs de %1 décrochent sous fumigènes vers %2 (%3 soldats, %4)", name, to.m_sName, moving, smokeText);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.DECISION, text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MA6 : la localité hostile la plus proche à repli_voisine_max_m dont la direction fait au moins repli_angle_min
	//! avec celle du contact, sinon la plus proche ; null si aucune
	protected SRP_SectorState PickRetreatTarget(SRP_SectorState state, vector contact)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory || !state || !state.m_Sector)
			return null;
		vector center = state.m_Sector.m_vCenter;
		vector threat = contact - center;
		threat[1] = 0;
		float threatLength = threat.Length();
		float cosLimit = Math.Cos(m_iRetreatAngleMin * Math.DEG2RAD);
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		SRP_SectorState bestWide = null;
		float bestWideDistance = m_iRetreatNeighbourM;
		SRP_SectorState bestAny = null;
		float bestAnyDistance = m_iRetreatNeighbourM;
		foreach (SRP_SectorState other : localities)
		{
			if (!other || other == state || !other.m_Sector || IsLocalityEmptied(other.m_sName))
				continue;
			vector away = other.m_Sector.m_vCenter - center;
			away[1] = 0;
			float distance = away.Length();
			if (distance < 1 || distance > m_iRetreatNeighbourM)
				continue;
			if (distance < bestAnyDistance)
			{
				bestAny = other;
				bestAnyDistance = distance;
			}
			bool wide = true;
			if (threatLength >= 1)
				wide = vector.Dot(away, threat) / (distance * threatLength) <= cosLimit;
			if (wide && distance < bestWideDistance)
			{
				bestWide = other;
				bestWideDistance = distance;
			}
		}
		if (bestWide)
			return bestWide;
		return bestAny;
	}

	//------------------------------------------------------------------------------------------------
	//! MA6 : point de sortie = point clé + direction de la voisine x (rayon + repli_sortie_m), rabattu sur une route
	//! (FindRoadPointToward), sinon au sol
	protected vector RetreatExit(vector center, float radius, vector toCenter)
	{
		vector direction = toCenter - center;
		direction[1] = 0;
		float length = direction.Length();
		if (length < 1)
			return center;
		vector probe = SRP_Placement.OnGround(center + direction * ((radius + m_iRetreatExitM) / length));
		vector road;
		if (SRP_Placement.FindRoadPointToward(center, probe, SRP_Placement.ROAD_SNAP, road))
			return road;
		return probe;
	}

	//------------------------------------------------------------------------------------------------
	//! Replis en cours : couverture, groupes effacés hors de vue dans la colonne, groupes bloqués relancés — Tick
	protected void RetreatsTick(int nowUnix)
	{
		if (m_aRetreats.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!enemies)
			return;
		int tick = System.GetTickCount();
		for (int i = m_aRetreats.Count() - 1; i >= 0; i--)
		{
			SRP_CmdRetreat retreat = m_aRetreats[i];
			if (!retreat)
			{
				m_aRetreats.Remove(i);
				continue;
			}
			vector center = retreat.m_vExit;
			float radius = 0;
			SRP_SectorState origin = null;
			if (territory)
				origin = territory.FindState(retreat.m_sLocality);
			if (origin && origin.m_Sector)
			{
				center = origin.m_Sector.m_vCenter;
				radius = origin.m_Sector.m_fRadius;
			}
			vector destination = LocalityPoint(retreat.m_sTo);
			if (destination == vector.Zero)
				destination = retreat.m_vExit;

			// Couverture : après repli_couverture_s, elle décroche à son tour, sans fumée
			SRP_EnemyGroup cover = retreat.m_Cover;
			if (cover && nowUnix - retreat.m_iStartUnix >= m_iRetreatCoverS)
			{
				if (FitNow(cover, tick) > 0)
				{
					enemies.OrderRetreat(cover, enemies.GroupPosition(cover), 0, retreat.m_vExit, destination);
					cover.m_iCmdRole = SRP_ECmdRole.REPLI;
					cover.m_iCmdStillUnix = nowUnix;
					retreat.m_aGroups.Insert(cover);
				}
				retreat.m_Cover = null;
			}

			SRP_CmdColumn column = FindColumn(retreat.m_iColumnId);
			for (int k = retreat.m_aGroups.Count() - 1; k >= 0; k--)
			{
				SRP_EnemyGroup mover = retreat.m_aGroups[k];
				if (!mover || FitNow(mover, tick) <= 0)
				{
					retreat.m_aGroups.Remove(k);
					continue;
				}
				vector position = enemies.GroupPosition(mover);
				if (vector.DistanceXZ(position, mover.m_vCmdLast) > 30)
				{
					mover.m_vCmdLast = position;
					mover.m_iCmdStillUnix = nowUnix;
					retreat.m_bMoved = true;
				}
				bool outside = vector.DistanceXZ(position, center) > radius + 150;
				// Sorti, hors de vue et à repli_effacement_m de tout joueur : ses soldats passent dans la colonne
				if (outside && column && SRP_Placement.NearestPlayer(position, m_aPlayers) >= m_iRetreatEraseM && !SRP_Placement.IsSeenByAnyPlayer(position, m_aPlayers))
				{
					int soldiers = SRP_EnemyComponent.FitOf(mover);
					retreat.m_aGroups.Remove(k);
					column.m_iSoldiers += soldiers;
					if (column.m_iDepartUnix <= 0)
						column.m_iDepartUnix = nowUnix;
					enemies.Delete(mover);
					m_bDirty = true;
					continue;
				}
				// Pas bougé de 30 m en 5 min : nouvel ordre
				if (nowUnix - mover.m_iCmdStillUnix >= 300)
				{
					vector goal = destination;
					if (!outside)
						goal = retreat.m_vExit;
					enemies.OrderForcedMove(mover, goal, 30);
					mover.m_iCmdStillUnix = nowUnix;
				}
			}

			bool finished = retreat.m_aGroups.IsEmpty() && !retreat.m_Cover;
			bool tooLong = nowUnix - retreat.m_iStartUnix >= 1800;
			if (finished || tooLong)
			{
				string why = "plus personne en marche";
				if (!finished)
					why = "30 min écoulées";
				EndRetreat(retreat, why);
				m_aRetreats.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fin d'un décrochage : ceux qui marchent encore se retirent hors de vue (RetireLater) ; colonne vide retirée
	protected void EndRetreat(SRP_CmdRetreat retreat, string why)
	{
		if (!retreat)
			return;
		array<ref SRP_EnemyGroup> left = {};
		foreach (SRP_EnemyGroup record : retreat.m_aGroups)
		{
			if (record)
				left.Insert(record);
		}
		if (retreat.m_Cover)
			left.Insert(retreat.m_Cover);
		retreat.m_aGroups.Clear();
		retreat.m_Cover = null;
		foreach (SRP_EnemyGroup retiring : left)
			retiring.m_iCmdRole = SRP_ECmdRole.REPLI;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies && !left.IsEmpty())
			enemies.RetireLater(left);
		int moving = 0;
		SRP_CmdColumn column = FindColumn(retreat.m_iColumnId);
		if (column)
		{
			moving = column.m_iSoldiers;
			if (column.m_iSoldiers <= 0 && column.m_iState == SRP_ECmdColumnState.PAPIER)
				m_aColumns.RemoveItemOrdered(column);
		}
		m_bDirty = true;
		string text = string.Format("fin du décrochage de %1 (%2) : %3 soldats en colonne vers %4", retreat.m_sLocality, why, moving, retreat.m_sTo);
		SRP_CmdLog.Note(RegionAt(retreat.m_vExit), SRP_ECmdLogKind.DECISION, text);
	}

	//------------------------------------------------------------------------------------------------
	protected void EndAllRetreats(string why)
	{
		for (int i = m_aRetreats.Count() - 1; i >= 0; i--)
			EndRetreat(m_aRetreats[i], why);
		m_aRetreats.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! MA7 : localités vidées (joueur à repli_vide_distance_m : regarnissage repoussé) — Tick (60 s)
	protected void EmptiedTick(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!front || !host)
			return;
		int count = front.GetLocalityCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontLocality loc = front.GetLocality(i);
			if (!loc || loc.m_iEmptiedAt == 0)
				continue;
			vector position = FrontLocalityPos(front, loc);
			if (SRP_Placement.NearestPlayer(position, m_aPlayers) <= m_iEmptiedM)
			{
				// Heure des pertes et des envoyés tenue à « maintenant » : le quart par heure ne part qu'après le départ
				// des joueurs. Une valeur datée d'avant le décrochage (StartRetreat sans AddLosses ni AddSent : décrochage
				// forcé sans perte, aucun partant) est d'abord ramenée à son reste, une seule fois, puis figée
				if (loc.m_iLossesAt < loc.m_iEmptiedAt)
					loc.m_iLosses = host.CurrentLosses(loc.m_sName);
				loc.m_iLossesAt = nowUnix;
				if (loc.m_iSentAt < loc.m_iEmptiedAt)
					loc.m_iSent = host.CurrentSent(loc.m_sName);
				loc.m_iSentAt = nowUnix;
			}
			else
			{
				loc.m_iEmptiedAt = 0;
				SRP_CmdLog.Note(RegionAt(position), SRP_ECmdLogKind.DECISION, string.Format("%1 : plus aucun joueur à %2 m, la localité se regarnit", loc.m_sName, m_iEmptiedM));
			}
			m_bDirty = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MA8 : plan d'assaut (mode selon ce que l'ennemi SAIT) ; seuls les groupes encore présents y entrent (IsPresent)
	//! — OnWavePosted, raids
	protected SRP_CmdAssault BuildAssault(array<ref SRP_EnemyGroup> candidates, vector objective, vector originA, vector originB, bool hasB, int zone)
	{
		int nowUnix = System.GetUnixTime();
		SRP_CmdAssault plan = new SRP_CmdAssault();
		plan.m_iZone = zone;
		plan.m_vObjective = objective;
		plan.m_vOriginA = originA;
		plan.m_vOriginB = originB;
		plan.m_bHasB = hasB;
		plan.m_iStepUnix = nowUnix;
		plan.m_iObjSince = nowUnix;
		plan.m_iStep = 2;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			plan.m_iObjCell = front.CellIndexAt(objective);
		// Un groupe anéanti (SCR_AIGroup supprimé par le jeu) n'entre dans aucun plan : sa position ne se lit plus
		array<ref SRP_EnemyGroup> groups = {};
		if (candidates)
		{
			foreach (SRP_EnemyGroup candidateGroup : candidates)
			{
				if (IsPresent(candidateGroup) && groups.Find(candidateGroup) < 0)
					groups.Insert(candidateGroup);
			}
		}
		if (!enemies || groups.IsEmpty())
			return plan;

		// Ce que l'ennemi sait (CO5) : joueurs connus près de l'objectif, joueurs retranchés
		int intelZone = zone;
		if (intelZone < 0 && front)
			intelZone = front.GetZoneAt(objective);
		int known = 0;
		if (m_Commander)
			known = m_Commander.KnownPlayersNear(objective, m_iKnownRadiusM, m_iKnownMin * 60);
		bool entrenched = IsEntrenched(intelZone, nowUnix);
		int soldiers = 0;
		int count = 0;
		foreach (SRP_EnemyGroup counted : groups)
		{
			if (!counted)
				continue;
			count++;
			soldiers += GroupStrength(counted);
		}
		float needed = m_fTwoAxesRatio * known;
		if (needed < m_iTwoAxesSoldiers)
			needed = m_iTwoAxesSoldiers;
		int mode = SRP_ECmdAssaultMode.FACE;
		if (hasB && count >= m_iTwoAxesGroups && soldiers >= needed)
			mode = SRP_ECmdAssaultMode.DEUX_AXES;
		else if (entrenched && count >= 2)
			mode = SRP_ECmdAssaultMode.FLANC;

		// Deux axes : chaque vague garde son axe (le départ le plus proche) ; un seul axe réel (feinte) -> flanc ou face
		if (mode == SRP_ECmdAssaultMode.DEUX_AXES)
		{
			foreach (SRP_EnemyGroup split : groups)
			{
				if (!split)
					continue;
				vector splitPos = enemies.GroupPosition(split);
				if (vector.DistanceXZ(splitPos, originB) < vector.DistanceXZ(splitPos, originA))
					plan.m_aAxisB.Insert(split);
				else
					plan.m_aMove.Insert(split);
			}
			if (plan.m_aMove.IsEmpty() || plan.m_aAxisB.IsEmpty())
			{
				plan.m_aMove.Clear();
				plan.m_aAxisB.Clear();
				mode = SRP_ECmdAssaultMode.FACE;
				if (entrenched && count >= 2)
					mode = SRP_ECmdAssaultMode.FLANC;
			}
		}
		if (mode == SRP_ECmdAssaultMode.FLANC)
		{
			// Le plus gros groupe fait base de feu, les autres manœuvrent
			SRP_EnemyGroup biggest = null;
			int biggestStrength = -1;
			foreach (SRP_EnemyGroup candidate : groups)
			{
				int strength = GroupStrength(candidate);
				if (candidate && strength > biggestStrength)
				{
					biggest = candidate;
					biggestStrength = strength;
				}
			}
			foreach (SRP_EnemyGroup member : groups)
			{
				if (!member)
					continue;
				if (member == biggest)
					plan.m_aFire.Insert(member);
				else
					plan.m_aMove.Insert(member);
			}
		}
		else if (mode == SRP_ECmdAssaultMode.FACE)
		{
			foreach (SRP_EnemyGroup assaulter : groups)
			{
				if (assaulter)
					plan.m_aMove.Insert(assaulter);
			}
		}
		plan.m_iMode = mode;

		// Rôles : exclus de l'entraide (IsFree) ; rôle d'assaut CRX ; un raid garde son rôle RAID
		array<ref SRP_EnemyGroup> all = {};
		PlanGroups(plan, all);
		foreach (SRP_EnemyGroup engaged : all)
		{
			enemies.ApplyRole(engaged, SRP_ECRXRole.ASSAUT, objective, -1);
			engaged.m_vCmdLast = enemies.GroupPosition(engaged);
			engaged.m_iCmdStillUnix = nowUnix;
			if (zone < 0)
				continue;
			engaged.m_iAssaultZone = zone;
			engaged.m_iCmdRole = SRP_ECmdRole.ASSAUT;
		}

		// Ordres de départ
		if (mode == SRP_ECmdAssaultMode.FACE)
		{
			foreach (SRP_EnemyGroup forward : plan.m_aMove)
				enemies.OrderSearch(forward, objective);
			// 3 joueurs connus ou plus : le 1er groupe ouvre par un tir de suppression
			if (known >= 3 && !plan.m_aMove.IsEmpty())
			{
				enemies.OrderSuppress(plan.m_aMove[0], objective, m_iSuppressOpenS, m_fSuppressHeight);
				plan.m_iStep = 1;
			}
		}
		else if (mode == SRP_ECmdAssaultMode.FLANC)
		{
			SRP_EnemyGroup fire = plan.m_aFire[0];
			plan.m_vFirePos = FindFirePos(objective, enemies.GroupPosition(fire));
			enemies.OrderMove(fire, plan.m_vFirePos);
			if (zone >= 0)
				fire.m_iCmdRole = SRP_ECmdRole.BASE_FEU;
			foreach (SRP_EnemyGroup flanker : plan.m_aMove)
			{
				HoldForLaunch(enemies, flanker, objective);
				if (zone >= 0)
					flanker.m_iCmdRole = SRP_ECmdRole.MANOEUVRE;
			}
			plan.m_iStep = 0;
		}
		else
		{
			plan.m_vWaitA = WaitPoint(objective, originA);
			plan.m_vWaitB = WaitPoint(objective, originB);
			foreach (SRP_EnemyGroup axisA : plan.m_aMove)
				enemies.OrderMove(axisA, plan.m_vWaitA);
			foreach (SRP_EnemyGroup axisB : plan.m_aAxisB)
				enemies.OrderMove(axisB, plan.m_vWaitB);
			plan.m_iStep = 0;
		}
		return plan;
	}

	//------------------------------------------------------------------------------------------------
	//! Une vague posée en plusieurs fois (échelonnement) ou sortie du papier rejoint le plan de sa vague, avec les
	//! ordres de l'étape en cours
	protected void AddToPlan(SRP_CmdAssault plan, array<ref SRP_EnemyGroup> groups, int nowUnix)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!plan || !groups || !enemies)
			return;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!IsPresent(record) || PlanHas(plan, record))
				continue;
			enemies.ApplyRole(record, SRP_ECRXRole.ASSAUT, plan.m_vObjective, -1);
			record.m_vCmdLast = enemies.GroupPosition(record);
			record.m_iCmdStillUnix = nowUnix;
			if (plan.m_iZone >= 0)
			{
				record.m_iAssaultZone = plan.m_iZone;
				record.m_iCmdRole = SRP_ECmdRole.ASSAUT;
			}
			if (plan.m_iMode == SRP_ECmdAssaultMode.DEUX_AXES)
			{
				bool onB = vector.DistanceXZ(record.m_vCmdLast, plan.m_vOriginB) < vector.DistanceXZ(record.m_vCmdLast, plan.m_vOriginA);
				if (onB)
					plan.m_aAxisB.Insert(record);
				else
					plan.m_aMove.Insert(record);
				if (plan.m_iStep > 0)
					enemies.OrderSearch(record, plan.m_vObjective);
				else if (onB)
					enemies.OrderMove(record, plan.m_vWaitB);
				else
					enemies.OrderMove(record, plan.m_vWaitA);
				continue;
			}
			plan.m_aMove.Insert(record);
			if (plan.m_iMode == SRP_ECmdAssaultMode.FLANC)
			{
				if (plan.m_iZone >= 0)
					record.m_iCmdRole = SRP_ECmdRole.MANOEUVRE;
				if (plan.m_iStep >= 2)
					LaunchApproach(enemies, plan, record);
				else
					HoldForLaunch(enemies, record, plan.m_vObjective);
				continue;
			}
			if (plan.m_bFinal)
				enemies.OrderAttackPoint(record, plan.m_vFinalPos);
			else
				enemies.OrderSearch(record, plan.m_vObjective);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MA8 : un pas du plan (feu, manœuvre, fumée, attaque finale) — DriveAssault, RaidsTick
	protected void DriveAssaultPlan(SRP_CmdAssault plan, int nowUnix)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!plan || !enemies)
			return;
		int elapsed = nowUnix - plan.m_iStepUnix;
		if (plan.m_iMode == SRP_ECmdAssaultMode.FLANC)
		{
			if (plan.m_iStep == 0)
			{
				// Base de feu arrivée à 40 m de son point (ou attente assaut_synchro_max_s) : feu
				bool fireReady = plan.m_aFire.IsEmpty() || elapsed >= m_iSyncMaxS;
				// (un groupe anéanti reste dans le plan jusqu'au passage de Prune, 60 s au plus : jamais lu)
				foreach (SRP_EnemyGroup fire : plan.m_aFire)
				{
					if (IsPresent(fire) && vector.DistanceXZ(enemies.GroupPosition(fire), plan.m_vFirePos) <= 40)
						fireReady = true;
				}
				if (!fireReady)
					return;
				foreach (SRP_EnemyGroup opener : plan.m_aFire)
					enemies.OrderSuppress(opener, plan.m_vObjective, m_iSuppressS, m_fSuppressHeight);
				plan.m_iStep = 1;
				plan.m_iStepUnix = nowUnix;
				return;
			}
			if (plan.m_iStep == 1)
			{
				// La manœuvre part 20 s après l'ouverture du feu, par le point de contournement caché
				if (elapsed < 20 && !plan.m_aFire.IsEmpty())
					return;
				foreach (SRP_EnemyGroup mover : plan.m_aMove)
					LaunchApproach(enemies, plan, mover);
				plan.m_iStep = 2;
				plan.m_iStepUnix = nowUnix;
				return;
			}
			// La base de feu rejoint l'assaut à la fin de sa suppression
			if (!plan.m_bFireJoined && elapsed >= m_iSuppressS)
			{
				plan.m_bFireJoined = true;
				foreach (SRP_EnemyGroup joiner : plan.m_aFire)
					enemies.OrderSearch(joiner, plan.m_vObjective);
			}
			SmokeAndFinal(enemies, plan, nowUnix);
			return;
		}
		if (plan.m_iMode == SRP_ECmdAssaultMode.DEUX_AXES)
		{
			if (plan.m_iStep == 0)
			{
				// Assaut lancé quand les deux axes sont à leur point d'attente (ou après assaut_synchro_max_s)
				bool readyA = plan.m_aMove.IsEmpty();
				foreach (SRP_EnemyGroup waitingA : plan.m_aMove)
				{
					if (IsPresent(waitingA) && vector.DistanceXZ(enemies.GroupPosition(waitingA), plan.m_vWaitA) <= 80)
						readyA = true;
				}
				bool readyB = plan.m_aAxisB.IsEmpty();
				foreach (SRP_EnemyGroup waitingB : plan.m_aAxisB)
				{
					if (IsPresent(waitingB) && vector.DistanceXZ(enemies.GroupPosition(waitingB), plan.m_vWaitB) <= 80)
						readyB = true;
				}
				if (!(readyA && readyB) && elapsed < m_iSyncMaxS)
					return;
				OpenAxis(enemies, plan, plan.m_aMove);
				OpenAxis(enemies, plan, plan.m_aAxisB);
				plan.m_iStep = 1;
				plan.m_iStepUnix = nowUnix;
				return;
			}
			if (plan.m_iStep == 1)
			{
				if (elapsed < m_iSuppressOpenS)
					return;
				if (!plan.m_aMove.IsEmpty())
					enemies.OrderSearch(plan.m_aMove[0], plan.m_vObjective);
				if (!plan.m_aAxisB.IsEmpty())
					enemies.OrderSearch(plan.m_aAxisB[0], plan.m_vObjective);
				plan.m_iStep = 2;
				plan.m_iStepUnix = nowUnix;
				return;
			}
			SmokeAndFinal(enemies, plan, nowUnix);
			return;
		}
		// De face : l'ouverture de suppression finie, le 1er groupe part à son tour
		if (plan.m_iStep == 1)
		{
			if (elapsed < m_iSuppressOpenS)
				return;
			if (!plan.m_aMove.IsEmpty())
				enemies.OrderSearch(plan.m_aMove[0], plan.m_vObjective);
			plan.m_iStep = 2;
			plan.m_iStepUnix = nowUnix;
			return;
		}
		SmokeAndFinal(enemies, plan, nowUnix);
	}

	//------------------------------------------------------------------------------------------------
	//! Deux axes : le 1er groupe de l'axe ouvre par un tir de suppression, les autres fouillent et détruisent
	protected void OpenAxis(SRP_EnemyComponent enemies, SRP_CmdAssault plan, array<ref SRP_EnemyGroup> axis)
	{
		foreach (int index, SRP_EnemyGroup record : axis)
		{
			if (!record)
				continue;
			if (index == 0)
				enemies.OrderSuppress(record, plan.m_vObjective, m_iSuppressOpenS, m_fSuppressHeight);
			else
				enemies.OrderSearch(record, plan.m_vObjective);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MA8 : fumigènes d'un groupe à assaut_fumee_distance_m de l'objectif (une fois : le point de fumée passe devant
	//! ses points en cours, qui reprennent ensuite, SRP_EnemyComponent.OrderSmokeCover) ; attaque finale du point clé
	//! relancée une fois en SearchAndDestroy pour les groupes restés sans contact. (Le rideau d'obus fumigènes d'une
	//! contre-attaque, MO7, est déclenché par DriveAssault : CurtainTick.)
	protected void SmokeAndFinal(SRP_EnemyComponent enemies, SRP_CmdAssault plan, int nowUnix)
	{
		int tick = System.GetTickCount();
		if (!plan.m_bSmoked && m_iSmokeGrenades > 0)
		{
			array<ref SRP_EnemyGroup> movers = {};
			foreach (SRP_EnemyGroup moverA : plan.m_aMove)
				movers.Insert(moverA);
			foreach (SRP_EnemyGroup moverB : plan.m_aAxisB)
				movers.Insert(moverB);
			foreach (SRP_EnemyGroup smoker : movers)
			{
				if (!smoker || SRP_EnemyComponent.FitOf(smoker) <= 0)
					continue;
				vector position = enemies.GroupPosition(smoker);
				if (vector.DistanceXZ(position, plan.m_vObjective) > m_iSmokeDistanceM)
					continue;
				vector middle = position + (plan.m_vObjective - position) * 0.5;
				enemies.OrderSmokeCover(smoker, SRP_Placement.OnGround(middle), m_iSmokeGrenades);
				plan.m_bSmoked = true;
				break;
			}
		}
		// Attaque directe du point clé : au bout de 60 s, les groupes sans contact fouillent et détruisent sur le point
		if (plan.m_bFinal && nowUnix - plan.m_iFinalUnix >= 60)
		{
			plan.m_bFinal = false;
			array<ref SRP_EnemyGroup> finalists = {};
			PlanGroups(plan, finalists);
			foreach (SRP_EnemyGroup finalist : finalists)
			{
				if (!InContact(finalist, tick))
					enemies.OrderSearch(finalist, plan.m_vFinalPos);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MO7, CA4 (fumée) : dès qu'un élément de l'assaut (présent, en état de combattre) est à appui_fumee_declenchement_m
	//! (SRP_CmdSupport.m_iSmokeTriggerM) de la cible, rideau d'obus fumigènes — DriveAssault (sans feinte)
	protected void CurtainTick(SRP_EnemyComponent enemies, SRP_CounterAttack attack, int tick)
	{
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		if (!support || !enemies || !attack)
			return;
		bool found = false;
		vector nearestPos;
		float nearestDistance = support.m_iSmokeTriggerM;
		foreach (SRP_EnemyGroup record : attack.m_aGroups)
		{
			if (!IsPresent(record) || FitNow(record, tick) <= 0)
				continue;
			vector position = enemies.GroupPosition(record);
			float distance = vector.DistanceXZ(position, attack.m_vTarget);
			if (distance <= nearestDistance)
			{
				found = true;
				nearestDistance = distance;
				nearestPos = position;
			}
		}
		if (found)
			SmokeShells(attack, nearestPos, support.m_iSmokeTriggerM);
	}

	//------------------------------------------------------------------------------------------------
	//! CA4 (fumée) : obus fumigènes du mortier de la région entre les joueurs connus et l'élément qui arrive (from = son
	//! côté, threat = contact vu près de la cible, sinon la cible), une fois par contre-attaque
	protected void SmokeShells(SRP_CounterAttack attack, vector from, int triggerM)
	{
		m_bCaSmokeDone = true;
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		if (!support || !m_Commander || !attack)
			return;
		int region = attack.m_iRegion;
		if (region < 0)
			region = m_Commander.GetRegionOfZone(attack.m_iZone);
		SRP_CmdRegionBook book = Book();
		if (region < 0 || attack.m_bDisorganized || (book && book.IsRegionDisorganized(region)))
			return;
		vector threat = attack.m_vTarget;
		vector seen;
		if (SeenContactNear(attack.m_vTarget, 500, 600, seen))
			threat = seen;
		string smoke = support.RequestSmoke(region, from, threat, "assaut de la contre-attaque", false);
		LogSupport(region, false, "rideau de fumée de la contre-attaque sur " + attack.m_sZone, "assaut à " + triggerM.ToString() + " m de la cible", smoke, "aucun (fumigènes gratuits, C5)", -1);
	}

	//------------------------------------------------------------------------------------------------
	//! FLANC : la manœuvre passe par le point de contournement caché du #69 (FlankPoint), puis fouille et détruit
	protected void LaunchApproach(SRP_EnemyComponent enemies, SRP_CmdAssault plan, SRP_EnemyGroup record)
	{
		if (!IsPresent(record))
			return;
		vector position = enemies.GroupPosition(record);
		vector flank;
		if (SRP_EnemyAwareness.FlankPoint(position, plan.m_vFirePos, true, plan.m_vObjective, flank))
			enemies.OrderApproach(record, flank, plan.m_vObjective);
		else
			enemies.OrderSearch(record, plan.m_vObjective);
	}

	//------------------------------------------------------------------------------------------------
	//! FLANC : en attendant le feu, la manœuvre s'arrête à assaut_synchro_distance_m de l'objectif (sur place si elle
	//! est déjà plus près)
	protected void HoldForLaunch(SRP_EnemyComponent enemies, SRP_EnemyGroup record, vector objective)
	{
		if (!IsPresent(record))
			return;
		vector position = enemies.GroupPosition(record);
		if (vector.DistanceXZ(position, objective) <= m_iSyncDistanceM)
			enemies.OrderMove(record, position);
		else
			enemies.OrderMove(record, WaitPoint(objective, position));
	}

	//------------------------------------------------------------------------------------------------
	//! Contre-attaque : objectif = carré bleu reprenable de la zone qui minimise distance(groupe) + 0,5 x
	//! distance(point clé visé) ; nouvel objectif quand le carré est repris ou tenu 180 s ; dernier carré avec un point
	//! clé : attaque directe du point (assaut_attaque_finale)
	protected void UpdateObjective(SRP_FrontComponent front, SRP_EnemyComponent enemies, SRP_CounterAttack attack, SRP_CmdAssault plan, int nowUnix)
	{
		bool expired = plan.m_iObjCell < 0 || !IsRetakeCell(front, plan.m_iObjCell, attack.m_iZone) || nowUnix - plan.m_iObjSince >= 180;
		if (!expired)
			return;
		plan.m_iObjSince = nowUnix;
		vector centroid = PlanCentroid(enemies, plan);
		int cell = PickRetakeCell(attack.m_iZone, centroid, attack.m_vTarget);
		if (cell < 0 || cell == plan.m_iObjCell)
			return;
		plan.m_iObjCell = cell;
		vector target = front.CellCenter(cell);
		bool keyCell = false;
		vector keyPos;
		int keyCount = front.GetKeyPointCount(attack.m_iZone);
		for (int k = 0; k < keyCount; k++)
		{
			if (front.GetKeyPointCell(attack.m_iZone, k) != cell)
				continue;
			keyCell = true;
			keyPos = front.GetKeyPointPos(attack.m_iZone, k);
		}
		if (keyCell)
			target = keyPos;
		plan.m_vObjective = target;
		plan.m_bFinal = false;
		// Avant l'assaut (mise en place, ouverture), les ordres suivent la doctrine avec le nouvel objectif
		if (!IsLaunched(plan))
			return;
		array<ref SRP_EnemyGroup> members = {};
		PlanGroups(plan, members);
		if (keyCell && m_bFinalAttack)
		{
			plan.m_bFinal = true;
			plan.m_iFinalUnix = nowUnix;
			plan.m_vFinalPos = keyPos;
			foreach (SRP_EnemyGroup striker : members)
				enemies.OrderAttackPoint(striker, keyPos);
			SRP_CmdLog.Note(attack.m_iRegion, SRP_ECmdLogKind.DECISION, string.Format("assaut sur %1 : attaque directe du point clé (%2)", attack.m_sZone, SRP_CmdScreens.CellText(keyPos)));
			return;
		}
		foreach (SRP_EnemyGroup member : members)
		{
			// Une base de feu encore en suppression garde son tir, comme le groupe qui ouvre un assaut de face
			if (plan.m_iMode == SRP_ECmdAssaultMode.FLANC && !plan.m_bFireJoined && plan.m_aFire.Find(member) >= 0)
				continue;
			if (plan.m_iMode == SRP_ECmdAssaultMode.FACE && plan.m_iStep == 1 && plan.m_aMove.Find(member) == 0)
				continue;
			enemies.OrderSearch(member, target);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! L'assaut du plan est lancé (les ordres de fouille sont donnés)
	protected bool IsLaunched(SRP_CmdAssault plan)
	{
		if (plan.m_iMode == SRP_ECmdAssaultMode.FACE)
			return true;
		return plan.m_iStep >= 2;
	}

	//------------------------------------------------------------------------------------------------
	//! MA10 : essai toutes les 5 min (intervalle, joueurs, contre-attaque récente, tirage) -> RequestHarass sur un
	//! contact CONNU — Tick (300 s)
	protected void HarassTick(int nowUnix)
	{
		if (!m_bHarassOn || !m_Commander)
			return;
		if (m_iLastHarassUnix > 0 && nowUnix - m_iLastHarassUnix < m_iHarassIntervalMin * 60)
			return;
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host || host.IsCounterAttackActive())
			return;
		if (m_iLastCaUnix > 0 && nowUnix - m_iLastCaUnix < m_iHarassAfterCaMin * 60)
			return;
		if (GetGame().GetPlayerManager().GetPlayerCount() < m_iHarassPlayersMin)
			return;
		if (Math.RandomInt(0, 100) >= m_iHarassChance)
			return;
		// Cible : le carré bleu du front près d'un contact CONNU (le plus récent), jamais la vraie position (CO5)
		array<ref SRP_CmdContact> contacts = {};
		m_Commander.GetContacts(-1, m_iHarassKnownMin * 60, contacts);
		SRP_CmdContact best = null;
		foreach (SRP_CmdContact contact : contacts)
		{
			if (!contact || contact.m_bInformantOnly)
				continue;
			if (NearestBlueFrontCell(contact.m_vPos, m_iHarassRangeM) < 0)
				continue;
			if (!best || contact.m_iLastUnix > best.m_iLastUnix)
				best = contact;
		}
		if (!best)
			return;
		// Le harcèlement tiré ici est une décision des manœuvres (pas d'IssueOrder) : décision ou refus écrits ici
		string refusal = RequestHarass(best.m_vPos, "harcèlement du front", "", false);
		int region = RegionAt(best.m_vPos);
		string what = "harcèlement du front vers " + SRP_CmdScreens.CellText(best.m_vPos);
		if (!refusal.IsEmpty())
		{
			SRP_CmdLog.Refusal(region, what, refusal);
			return;
		}
		string reason = string.Format("tirage de %1 sur 100 réussi, %2 joueur(s) connu(s)", m_iHarassChance, best.m_iPlayers);
		SRP_CmdLog.Decision(region, false, what, reason, "selon le harcèlement choisi", "");
	}

	//------------------------------------------------------------------------------------------------
	//! MA10 : raids en cours (fin par combat, pertes ou absence de joueurs, retour puis RetireLater) — Tick
	protected void RaidsTick(int nowUnix)
	{
		if (m_aRaids.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		int tick = System.GetTickCount();
		for (int i = m_aRaids.Count() - 1; i >= 0; i--)
		{
			SRP_CmdRaid raid = m_aRaids[i];
			if (!raid)
			{
				m_aRaids.Remove(i);
				continue;
			}
			int standing = 0;
			bool fighting = false;
			float nearest = 1000000;
			foreach (SRP_EnemyGroup raider : raid.m_aGroups)
			{
				int raiderFit = FitNow(raider, tick);
				if (raiderFit <= 0)
					continue;
				standing += raiderFit;
				if (InContact(raider, tick))
					fighting = true;
				float distance = SRP_Placement.NearestPlayer(enemies.GroupPosition(raider), m_aPlayers);
				if (distance < nearest)
					nearest = distance;
			}
			if (fighting && raid.m_iContactUnix == 0)
				raid.m_iContactUnix = nowUnix;
			if (nearest <= 1500)
				raid.m_iNoPlayerSince = 0;
			else if (raid.m_iNoPlayerSince == 0)
				raid.m_iNoPlayerSince = nowUnix;

			string why = "";
			if (standing <= 0)
				why = "détachement hors de combat";
			else if (raid.m_iContactUnix > 0 && nowUnix - raid.m_iContactUnix >= m_iHarassFightMin * 60)
				why = string.Format("%1 min de combat", m_iHarassFightMin);
			else if (raid.m_iPosted > 0 && (raid.m_iPosted - standing) * 100 >= m_iHarassLossCent * raid.m_iPosted)
				why = "pertes";
			else if (raid.m_iNoPlayerSince > 0 && nowUnix - raid.m_iNoPlayerSince >= 300)
				why = "plus aucun joueur à 1500 m";
			else if (nowUnix - raid.m_iStartUnix >= 3600)
				why = "une heure écoulée";
			if (!why.IsEmpty())
			{
				EndRaid(raid, why);
				m_aRaids.Remove(i);
				continue;
			}
			DriveAssaultPlan(raid.m_Plan, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fin d'un raid : ForcedMove vers son départ rouge, puis retrait hors de vue (RetireLater)
	protected void EndRaid(SRP_CmdRaid raid, string why)
	{
		if (!raid)
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		array<ref SRP_EnemyGroup> leaving = {};
		foreach (SRP_EnemyGroup raider : raid.m_aGroups)
		{
			if (!raider)
				continue;
			if (enemies && SRP_EnemyComponent.AliveAgents(raider) > 0)
				enemies.OrderForcedMove(raider, raid.m_vOrigin, 30);
			raider.m_iCmdRole = SRP_ECmdRole.REPLI;
			leaving.Insert(raider);
		}
		raid.m_aGroups.Clear();
		if (raid.m_Plan)
		{
			raid.m_Plan.m_aFire.Clear();
			raid.m_Plan.m_aMove.Clear();
			raid.m_Plan.m_aAxisB.Clear();
		}
		if (enemies && !leaving.IsEmpty())
			enemies.RetireLater(leaving);
		string text = string.Format("fin du harcèlement sur le carré %1 : %2", SRP_CmdScreens.CellText(raid.m_vTarget), why);
		SRP_CmdLog.Note(RegionAt(raid.m_vTarget), SRP_ECmdLogKind.DECISION, text);
	}

	//------------------------------------------------------------------------------------------------
	protected void EndAllRaids(string why)
	{
		for (int i = m_aRaids.Count() - 1; i >= 0; i--)
			EndRaid(m_aRaids[i], why);
		m_aRaids.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! MA11 : chemin estimé des joueurs par région -> localités du cône allégées d'envoyés, carrés de poste notés —
	//! Tick (60 s)
	protected void PathTick(int nowUnix)
	{
		SRP_CmdRegionBook book = Book();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!book || !front || !host || !territory || !m_Commander)
			return;
		float cosLimit = Math.Cos(m_iPathAngle * Math.DEG2RAD);
		int regions = book.GetRegionCount();
		for (int region = 0; region < regions; region++)
		{
			if (book.GetRegionState(region) != SRP_ECmdRegionState.ACTIVE)
				continue;
			SRP_CmdTrack track = m_mRegionTrack.Get(region);
			if (!track || track.m_iB <= track.m_iA)
				continue;
			// Deux relevés distants d'au moins reorg_releve_m, à reorg_releve_min près, et encore récents
			if (track.m_iB - track.m_iA > m_iTrackMin * 60 || nowUnix - track.m_iB > m_iTrackMin * 60)
				continue;
			int lastPath;
			if (m_mPathUnix.Find(region, lastPath) && nowUnix - lastPath < m_iPathIntervalMin * 60)
				continue;
			vector heading = track.m_vB - track.m_vA;
			heading[1] = 0;
			float headingLength = heading.Length();
			if (headingLength < m_iTrackM)
				continue;
			heading = heading * (1 / headingLength);

			// Localités hostiles du cône, au calme depuis reorg_chemin_calme_min : leurs envoyés rentrent
			int recalled = 0;
			array<SRP_SectorState> localities = {};
			territory.GetEnemyLocalities(localities);
			foreach (SRP_SectorState state : localities)
			{
				if (!state || !state.m_Sector)
					continue;
				vector toward = state.m_Sector.m_vCenter - track.m_vB;
				toward[1] = 0;
				float distance = toward.Length();
				if (distance < 1 || distance > m_iPathM)
					continue;
				if (vector.Dot(toward, heading) / distance < cosLimit)
					continue;
				vector lastContact;
				int lastContactUnix;
				if (m_iPathQuietMin > 0 && m_Commander.LastKnownContact(front.GetZoneAt(state.m_Sector.m_vCenter), m_iPathQuietMin * 60, lastContact, lastContactUnix))
					continue;
				if (host.CurrentSent(state.m_sName) <= 0)
					continue;
				host.ClearSent(state.m_sName);
				recalled++;
			}

			// Carrés rouges du front dans le cône à reorg_postes_distance_m : postes posés en tête (20 min)
			int marked = 0;
			int cells = front.GetCellCount();
			for (int cell = 0; cell < cells; cell++)
			{
				if (!front.IsCellInPlay(cell) || front.GetCellOwner(cell) != SRP_EFrontOwner.ROUGE || !front.IsCellAtFront(cell))
					continue;
				vector cellWay = front.CellCenter(cell) - track.m_vB;
				cellWay[1] = 0;
				float cellDistance = cellWay.Length();
				if (cellDistance < 1 || cellDistance > m_iPathPostsM)
					continue;
				if (vector.Dot(cellWay, heading) / cellDistance < cosLimit)
					continue;
				m_mPathCell.Set(cell, nowUnix);
				marked++;
			}
			m_mPathUnix.Set(region, nowUnix);
			if (recalled > 0)
				m_bDirty = true;
			if (recalled > 0 || marked > 0)
			{
				string text = string.Format("chemin des joueurs vers %1 : %2 localité(s) rappellent leurs envoyés, %3 carré(s) de poste en tête", SRP_CmdScreens.CellText(track.m_vB), recalled, marked);
				SRP_CmdLog.Decision(region, false, text, "deux relevés de contact", FREE, "");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : groupes d'assaut tenus éveillés loin des joueurs (HoldAwake) ou passés sur le papier — Tick (10 s)
	protected void AwakeTick(int nowUnix)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!host || !enemies)
			return;
		SRP_CounterAttack attack = host.GetCounterAttack();
		if (!attack || attack.m_iState != SRP_EFrontAttack.ASSAUT)
			return;
		for (int i = attack.m_aGroups.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup record = attack.m_aGroups[i];
			if (!record || SRP_EnemyComponent.FitOf(record) <= 0)
				continue;
			vector position = enemies.GroupPosition(record);
			float nearest = SRP_Placement.NearestPlayer(position, m_aPlayers);
			// Au-delà de ca_eveil_distance_m : tenu éveillé (le jeu couperait son IA) ; redit à chaque passage, pour les
			// soldats livrés plus tard par la file d'apparition
			if (nearest > m_iAwakeM)
				SRP_EnemyComponent.HoldAwake(record);
			// Aucun joueur à ca_effacement_distance_m depuis ca_effacement_min, hors de vue : sur le papier (pas pendant
			// la pose de sa vague)
			int farIndex = m_aFarGroups.Find(record);
			if (nearest > m_iEraseM && !IsPosting(attack, record, nowUnix) && !SRP_Placement.IsSeenByAnyPlayer(position, m_aPlayers))
			{
				if (farIndex < 0)
				{
					m_aFarGroups.Insert(record);
					m_aFarSince.Insert(nowUnix);
				}
				else if (nowUnix - m_aFarSince[farIndex] >= m_iEraseMin * 60)
				{
					m_aFarGroups.Remove(farIndex);
					m_aFarSince.Remove(farIndex);
					GroupToPaper(enemies, attack, record, nowUnix);
				}
			}
			else if (farIndex >= 0)
			{
				m_aFarGroups.Remove(farIndex);
				m_aFarSince.Remove(farIndex);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Débarqués des camions de contact sans contact depuis renfort_retrait_min : RetireLater SEULEMENT (le retour des
	//! soldats vivants est fait par SRP_EnemyComponent.RetireTick d'après m_sSourceLocality, seule porte) — Tick (60 s)
	protected void ContactGroupsTick(int nowUnix)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!enemies || !front)
			return;
		int tick = System.GetTickCount();
		// Inscription : groupes RENFORT débarqués (plus dans leur camion) qui ne défendent pas une localité
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		foreach (SRP_EnemyGroup candidate : groups)
		{
			if (!candidate || candidate.m_iCmdRole != SRP_ECmdRole.RENFORT || m_aContactGroups.Find(candidate) >= 0)
				continue;
			if (candidate.m_sTask == "camion" || candidate.m_sTask == "chauffeur" || candidate.m_sHomeTask == "débarqués")
				continue;
			if (SRP_EnemyComponent.AliveAgents(candidate) <= 0 || IsInAnyGarrison(candidate))
				continue;
			m_aContactGroups.Insert(candidate);
			m_aContactZones.Insert(front.GetZoneAt(enemies.GroupPosition(candidate)));
			m_aContactQuiet.Insert(nowUnix);
		}
		// Retrait après renfort_retrait_min sans contact (le groupe ou sa zone)
		array<ref SRP_EnemyGroup> retiring = {};
		for (int i = m_aContactGroups.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup landed = m_aContactGroups[i];
			if (!landed || SRP_EnemyComponent.AliveAgents(landed) <= 0 || IsInAnyGarrison(landed))
			{
				RemoveContactAt(i);
				continue;
			}
			bool zoneContact = false;
			vector contactPos;
			int contactUnix;
			if (m_Commander && m_Commander.LastKnownContact(m_aContactZones[i], 120, contactPos, contactUnix))
				zoneContact = vector.DistanceXZ(contactPos, enemies.GroupPosition(landed)) <= 1000;
			if (InContact(landed, tick) || zoneContact)
			{
				m_aContactQuiet[i] = nowUnix;
				continue;
			}
			if (nowUnix - m_aContactQuiet[i] < m_iContactTruckRetireMin * 60)
				continue;
			landed.m_iCmdRole = SRP_ECmdRole.REPLI;
			retiring.Insert(landed);
			RemoveContactAt(i);
		}
		if (!retiring.IsEmpty())
		{
			int count = retiring.Count();
			enemies.RetireLater(retiring);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.DECISION, string.Format("%1 détachement(s) de renfort sans contact depuis %2 min se retirent hors de vue", count, m_iContactTruckRetireMin));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Commandeur gelé : fin propre de ce qui est en cours (colonnes papier annulées : ReturnSent). Ce que le Staff a
	//! forcé (VU5 : ForceOrder marche aussi gelé) continue : colonnes, décrochages et harcèlements m_bStaff — Tick
	protected void FinishOnly(int nowUnix)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (host)
			PruneGathered(host);
		// Colonnes sur le papier annulées (sauf celles du Staff) : leurs soldats rentrent à leur localité
		int cancelled = 0;
		int returned = 0;
		for (int i = m_aColumns.Count() - 1; i >= 0; i--)
		{
			SRP_CmdColumn column = m_aColumns[i];
			if (column && (column.m_iState != SRP_ECmdColumnState.PAPIER || column.m_bStaff))
				continue;
			if (column && host && !column.m_sFrom.IsEmpty() && column.m_iSoldiers > 0)
			{
				host.ReturnSent(column.m_sFrom, column.m_iSoldiers);
				returned += column.m_iSoldiers;
			}
			m_aColumns.RemoveOrdered(i);
			cancelled++;
			m_bDirty = true;
		}
		if (cancelled > 0)
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Commandeur arrêté : %1 colonne(s) sur le papier annulée(s), %2 soldats rendus à leur localité", cancelled, returned));
		// Colonnes réelles (et colonnes du Staff) : elles roulent jusqu'au bout
		ColumnsTick(nowUnix);
		// Replis et raids : fin propre (retrait hors de vue), sauf ceux du Staff qui continuent ; plans d'assaut oubliés
		for (int r = m_aRetreats.Count() - 1; r >= 0; r--)
		{
			SRP_CmdRetreat retreat = m_aRetreats[r];
			if (retreat && retreat.m_bStaff)
				continue;
			EndRetreat(retreat, "Commandeur arrêté");
			m_aRetreats.Remove(r);
		}
		for (int k = m_aRaids.Count() - 1; k >= 0; k--)
		{
			SRP_CmdRaid raid = m_aRaids[k];
			if (raid && raid.m_bStaff)
				continue;
			EndRaid(raid, "Commandeur arrêté");
			m_aRaids.Remove(k);
		}
		RetreatsTick(nowUnix);
		RaidsTick(nowUnix);
		if (!m_aAssaults.IsEmpty() || !m_aPaper.IsEmpty() || !m_aFarGroups.IsEmpty())
			DropAttackPlans();
		if (nowUnix - m_iLast60 >= 60)
		{
			m_iLast60 = nowUnix;
			EmptiedTick(nowUnix);
			ContactGroupsTick(nowUnix);
		}
	}

	//================================================================================================
	// Aides
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Personnages des joueurs relus maintenant (délai de grâce compris)
	protected array<IEntity> FreshPlayers()
	{
		m_aPlayers = SRP_Utils.GetPlayerCharacters();
		return m_aPlayers;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdRegionBook Book()
	{
		if (!m_Commander)
			return null;
		return m_Commander.GetBook();
	}

	//------------------------------------------------------------------------------------------------
	//! Région sous une position (-1 en mer, dans la base ou sans Commandeur)
	protected int RegionAt(vector position)
	{
		SRP_CmdRegionBook book = Book();
		if (!book)
			return -1;
		return book.GetRegionAt(position);
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07) »
	protected string ZoneText(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0)
			return "hors zone";
		return front.GetZoneLabel(zone);
	}

	//------------------------------------------------------------------------------------------------
	protected string ReasonText(string reason, string author, bool staff)
	{
		if (!staff)
			return reason;
		if (author.IsEmpty())
			return "Staff : " + reason;
		return "Staff (" + author + ") : " + reason;
	}

	//------------------------------------------------------------------------------------------------
	//! « Chotain : 14 soldats » (effectif restant de la source, pour la colonne « reste » du journal)
	protected string EffectiveText(string locality)
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host || locality.IsEmpty())
			return "";
		return string.Format("%1 : %2 soldats", locality, host.GetLocalityEffective(locality));
	}

	//------------------------------------------------------------------------------------------------
	protected string ColumnKindWord(int kind)
	{
		if (kind == SRP_ECmdColumnKind.REPLI)
			return "décrochage";
		if (kind == SRP_ECmdColumnKind.STAFF)
			return "colonne demandée par le Staff";
		return "renfort lointain";
	}

	//------------------------------------------------------------------------------------------------
	protected string ModeWord(SRP_CmdAssault plan)
	{
		if (!plan)
			return "de face";
		if (plan.m_iMode == SRP_ECmdAssaultMode.FLANC)
			return "par le flanc";
		if (plan.m_iMode == SRP_ECmdAssaultMode.DEUX_AXES)
			return "sur deux axes";
		return "de face";
	}

	//------------------------------------------------------------------------------------------------
	protected string EndWord(int result)
	{
		if (result == SRP_EAttackEnd.PERDUE)
			return "zone reprise";
		if (result == SRP_EAttackEnd.DEFENDUE)
			return "zone défendue";
		if (result == SRP_EAttackEnd.ROMPUE)
			return "contact rompu";
		if (result == SRP_EAttackEnd.ANNULEE)
			return "annulée";
		return "sans issue";
	}

	//------------------------------------------------------------------------------------------------
	//! Point clé CENTRE d'une localité (celui de la localité ennemie, sinon celui du registre du front)
	protected vector LocalityPoint(string locality)
	{
		if (locality.IsEmpty())
			return vector.Zero;
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
		{
			SRP_SectorState state = territory.FindState(locality);
			if (state && state.m_Sector)
				return state.m_Sector.m_vCenter;
		}
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontLocality loc = FrontLocality(locality);
		if (!front || !loc)
			return vector.Zero;
		return FrontLocalityPos(front, loc);
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_FrontLocality FrontLocality(string locality)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || locality.IsEmpty())
			return null;
		int index = front.FindLocality(locality);
		if (index < 0)
			return null;
		return front.GetLocality(index);
	}

	//------------------------------------------------------------------------------------------------
	protected vector FrontLocalityPos(SRP_FrontComponent front, SRP_FrontLocality loc)
	{
		if (loc.m_iCentre >= 0)
		{
			SRP_FrontKeyPoint point = front.GetKeyPoint(loc.m_iCentre);
			if (point)
				return point.m_vPos;
		}
		return loc.m_vLabel;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité hostile dont le rayon + margin contient la position (la plus proche), null sinon
	protected SRP_SectorState HostileLocalityNear(vector position, float margin)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return null;
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		SRP_SectorState best = null;
		float bestDistance = 1000000;
		foreach (SRP_SectorState state : localities)
		{
			if (!state || !state.m_Sector)
				continue;
			float distance = vector.DistanceXZ(state.m_Sector.m_vCenter, position);
			if (distance <= state.m_Sector.m_fRadius + margin && distance < bestDistance)
			{
				best = state;
				bestDistance = distance;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! MA1 : localité hostile de la zone, garnison posée, dont le rayon + margin contient le contact (la plus proche)
	protected SRP_SectorState PostedLocalityAt(vector position, int zone, float margin)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!territory || !host || !front)
			return null;
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		SRP_SectorState best = null;
		float bestDistance = 1000000;
		foreach (SRP_SectorState state : localities)
		{
			if (!state || !state.m_Sector || !host.IsGarrisonPosted(state.m_sName))
				continue;
			if (zone >= 0 && front.GetZoneAt(state.m_Sector.m_vCenter) != zone)
				continue;
			float distance = vector.DistanceXZ(state.m_Sector.m_vCenter, position);
			if (distance <= state.m_Sector.m_fRadius + margin && distance < bestDistance)
			{
				best = state;
				bestDistance = distance;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité hostile la plus proche à « reach » (garnison posée seulement, ou non vidée), hors « exclude »
	protected SRP_SectorState NearestLocality(vector position, float reach, SRP_SectorState exclude, bool postedOnly)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!territory || !host)
			return null;
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		SRP_SectorState best = null;
		float bestDistance = reach;
		foreach (SRP_SectorState state : localities)
		{
			if (!state || !state.m_Sector || state == exclude)
				continue;
			if (postedOnly && !host.IsGarrisonPosted(state.m_sName))
				continue;
			if (!postedOnly && IsLocalityEmptied(state.m_sName))
				continue;
			float distance = vector.DistanceXZ(state.m_Sector.m_vCenter, position);
			if (distance < bestDistance)
			{
				best = state;
				bestDistance = distance;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! MA2 : délai d'arrivée d'un renfort, x renfort_facteur_desorg en région désorganisée (OF6, trou 12 : seul endroit)
	protected int ReinforceDelay(int region)
	{
		int low = m_iArriveMinS;
		int high = m_iArriveMaxS;
		if (high < low)
			high = low;
		int delay = Math.RandomIntInclusive(low, high);
		SRP_CmdRegionBook book = Book();
		if (region >= 0 && book && book.IsRegionDisorganized(region))
			delay = Math.Round(delay * m_fDesorgFactor);
		return delay;
	}

	//------------------------------------------------------------------------------------------------
	//! C2 : frein noté dès l'envoi, et dernier renfort de la région (Staff)
	protected void NoteReinforcement(int zone, int region, int nowUnix)
	{
		if (zone >= 0)
			m_mLastReinforce.Set(zone, nowUnix);
		if (region >= 0)
		{
			m_mRegionLastUnix.Set(region, nowUnix);
			m_mRegionLastZone.Set(region, zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats en état de combattre, attendus compris pour un groupe que la file d'apparition n'a pas encore servi
	protected int FitNow(SRP_EnemyGroup record, int tick)
	{
		if (!record)
			return 0;
		if (SRP_EnemyComponent.InGraceOf(record, tick))
			return record.m_iExpected;
		return SRP_EnemyComponent.FitOf(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Groupe encore présent (SCR_AIGroup non supprimé) : seule condition pour lire sa position. Le jeu supprime le
	//! groupe dès qu'il est vide (m_bDeleteWhenEmpty, 1 ms) ; OnGroupGone ne passe qu'avec Prune (60 s au plus), et
	//! SRP_EnemyComponent.GroupPosition ne teste rien
	protected bool IsPresent(SRP_EnemyGroup record)
	{
		return record && record.m_Group && !record.m_Group.IsDeleted();
	}

	//------------------------------------------------------------------------------------------------
	//! VU5, sécurité (jamais outrepassée) : un point dans la zone de la base -> refus ; Staff : aussi dans le rayon
	//! interdit aux poses ennemies autour de la base (SRP_Placement.IsNearBase) ; "" si rien ne s'y oppose. Hors Staff,
	//! la cible est déjà en territoire ennemi (MA1)
	protected string BaseRefusal(vector aim, int zone, bool staff)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (zone >= 0 && front && front.IsBaseZone(zone))
			return "sécurité : cible dans la base";
		if (staff && SRP_Placement.IsNearBase(aim))
			return "sécurité : trop près de la base";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Regroupements au centre oubliés pour les localités dont la garnison n'est plus posée (retirée, prise, zone forcée)
	protected void PruneGathered(SRP_FrontEnemyComponent host)
	{
		for (int i = m_aGathered.Count() - 1; i >= 0; i--)
		{
			if (!host.IsGarrisonPosted(m_aGathered[i]))
				m_aGathered.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe a lu un joueur depuis moins de 60 s (AlertTick)
	protected bool InContact(SRP_EnemyGroup record, int tick)
	{
		return record && record.m_iSpotTick != 0 && tick - record.m_iSpotTick < 60000;
	}

	//------------------------------------------------------------------------------------------------
	//! Taille d'un groupe pour la doctrine : attendus ou soldats en état, le plus grand
	protected int GroupStrength(SRP_EnemyGroup record)
	{
		if (!record)
			return 0;
		int fit = SRP_EnemyComponent.FitOf(record);
		if (record.m_iExpected > fit)
			return record.m_iExpected;
		return fit;
	}

	//------------------------------------------------------------------------------------------------
	//! ENQUETE : une unité mobile (ronde de fond, présence ambiante, vague de mission libre) : ni défense de localité, ni
	//! poste, ni garde, ni débarqués en défense, ni rôle du Commandeur, ni assaut, ni véhicule, ni déjà occupée
	protected bool IsMobile(SRP_EnemyGroup record, int tick)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return false;
		if (record.m_iCmdRole != SRP_ECmdRole.AUCUN || record.m_iAssaultZone >= 0)
			return false;
		if (record.m_Vehicle || record.m_sOwner == "helico" || record.m_sOwner == "mission" || record.m_sOwner == "survivant")
			return false;
		string task = record.m_sHomeTask;
		if (SRP_EnemyGarrison.IsPlanTask(task) || task == "poste front" || task == "camion" || task == "chauffeur" || task == "garde" || task == "officier" || task == "débarqués" || task == "jeep" || task == "helico")
			return false;
		if (record.m_iBusyUntilTick > tick)
			return false;
		return SRP_EnemyComponent.AliveAgents(record) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe défend-il une localité (il est dans la liste de ses défenseurs) ?
	protected bool IsInAnyGarrison(SRP_EnemyGroup record)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory || !record)
			return false;
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		foreach (SRP_SectorState state : localities)
		{
			if (state && state.m_aGroups.Find(record) >= 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveContactAt(int index)
	{
		if (index < 0 || index >= m_aContactGroups.Count())
			return;
		m_aContactGroups.Remove(index);
		if (index < m_aContactZones.Count())
			m_aContactZones.Remove(index);
		if (index < m_aContactQuiet.Count())
			m_aContactQuiet.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Carré reprenable par l'ennemi : bleu, de la zone (ou portant un de ses points clés, QG posé dans une zone
	//! voisine, Q2), reprise permise (F3, B4), et qui touche du rouge — même règle que SRP_FrontEnemyComponent.IsRetakeable
	//! et la capture (IsKeyCellOfZone)
	protected bool IsRetakeCell(SRP_FrontComponent front, int cell, int zone)
	{
		if (!front || cell < 0)
			return false;
		if (front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
			return false;
		if (front.GetCellZone(cell) != zone && !IsZoneKeyCell(front, cell, zone))
			return false;
		return front.CanRedRetake(cell) && front.HasNeighbourOwnedBy(cell, SRP_EFrontOwner.ROUGE, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré porte un point clé de la zone (même règle que SRP_FrontCapture.IsKeyCellOfZone) — IsRetakeCell
	protected bool IsZoneKeyCell(SRP_FrontComponent front, int cell, int zone)
	{
		if (!front || cell < 0 || zone < 0)
			return false;
		int keys = front.GetKeyPointCount(zone);
		for (int k = 0; k < keys; k++)
		{
			if (front.GetKeyPointCell(zone, k) == cell)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré reprenable de la zone (carrés de ses points clés posés dans une zone voisine compris, Q2) qui minimise
	//! distance(from) + 0,5 x distance(keyTarget) ; -1 si aucun
	protected int PickRetakeCell(int zone, vector from, vector keyTarget)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0)
			return -1;
		array<int> cells = {};
		front.GetZoneCells(zone, cells);
		int keys = front.GetKeyPointCount(zone);
		for (int k = 0; k < keys; k++)
		{
			int keyCell = front.GetKeyPointCell(zone, k);
			if (keyCell >= 0 && cells.Find(keyCell) < 0)
				cells.Insert(keyCell);
		}
		int best = -1;
		float bestScore = 100000000;
		foreach (int cell : cells)
		{
			if (!IsRetakeCell(front, cell, zone))
				continue;
			vector center = front.CellCenter(cell);
			float score = vector.DistanceXZ(center, from) + 0.5 * vector.DistanceXZ(center, keyTarget);
			if (score < bestScore)
			{
				best = cell;
				bestScore = score;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Objectif de départ d'une vague : le carré reprenable (point clé si le carré en porte un), sinon la cible
	protected vector AttackObjective(SRP_CounterAttack attack, vector from, out int cell)
	{
		cell = -1;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !attack)
			return from;
		cell = PickRetakeCell(attack.m_iZone, from, attack.m_vTarget);
		if (cell < 0)
		{
			cell = front.CellIndexAt(attack.m_vTarget);
			return attack.m_vTarget;
		}
		int keyCount = front.GetKeyPointCount(attack.m_iZone);
		for (int k = 0; k < keyCount; k++)
		{
			if (front.GetKeyPointCell(attack.m_iZone, k) == cell)
				return front.GetKeyPointPos(attack.m_iZone, k);
		}
		return front.CellCenter(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Carré bleu du front (hors base) le plus proche d'un point, à « range » au plus ; -1 si aucun
	protected int NearestBlueFrontCell(vector aim, float range)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return -1;
		int best = -1;
		float bestDistance = range;
		int count = front.GetCellCount();
		for (int cell = 0; cell < count; cell++)
		{
			if (!front.IsCellInPlay(cell) || front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU || front.IsBaseCell(cell) || !front.IsCellAtFront(cell))
				continue;
			float distance = vector.DistanceXZ(front.CellCenter(cell), aim);
			if (distance < bestDistance)
			{
				best = cell;
				bestDistance = distance;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Contact CONNU et VU (jamais un informateur seul, C4) à « radius » d'un point, le plus nombreux puis le plus
	//! récent, depuis maxAgeSeconds au plus ; sa position (floutée) dans « seen »
	protected bool SeenContactNear(vector position, float radius, int maxAgeSeconds, out vector seen)
	{
		seen = vector.Zero;
		if (!m_Commander)
			return false;
		array<ref SRP_CmdContact> contacts = {};
		m_Commander.GetContacts(-1, maxAgeSeconds, contacts);
		SRP_CmdContact best = null;
		foreach (SRP_CmdContact contact : contacts)
		{
			if (!contact || !contact.m_bSeen || contact.m_bInformantOnly)
				continue;
			if (vector.DistanceXZ(contact.m_vPos, position) > radius)
				continue;
			if (!best || contact.m_iPlayers > best.m_iPlayers || (contact.m_iPlayers == best.m_iPlayers && contact.m_iLastUnix > best.m_iLastUnix))
				best = contact;
		}
		if (!best)
			return false;
		seen = best.m_vPos;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! CA4 : un amas d'au moins ca_amas_joueurs joueurs connus sur ca_amas_rayon_m, dans la zone ou à 1 500 m de la
	//! cible (jamais un informateur seul) ; sa position dans « cluster »
	protected bool FindCluster(int zone, vector target, out vector cluster)
	{
		cluster = vector.Zero;
		if (!m_Commander)
			return false;
		array<ref SRP_CmdContact> contacts = {};
		m_Commander.GetContacts(-1, 600, contacts);
		int bestCount = 0;
		foreach (SRP_CmdContact contact : contacts)
		{
			if (!contact || contact.m_bInformantOnly)
				continue;
			if (contact.m_iZone != zone && vector.DistanceXZ(contact.m_vPos, target) > 1500)
				continue;
			int around = m_Commander.KnownPlayersNear(contact.m_vPos, m_iClusterM, 600);
			if (around >= m_iClusterPlayers && around > bestCount)
			{
				bestCount = around;
				cluster = contact.m_vPos;
			}
		}
		return bestCount > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! CA2 : dépôt #49 posé, aucun joueur connu à 1 km depuis ca_cible_min, carrés bleus qui touchent le rouge
	protected int IntelScore(SRP_FrontComponent front, int zone)
	{
		int score = 0;
		SRP_FrontZone info = front.GetZone(zone);
		if (info && info.m_bDepot)
			score += m_iTargetDepot;
		if (m_Commander.KnownPlayersNear(front.GetZoneCentroid(zone), 1000, m_iTargetMin * 60) == 0)
			score += m_iTargetEmpty;
		array<int> cells = {};
		front.GetZoneCells(zone, cells);
		foreach (int cell : cells)
		{
			if (front.GetCellOwner(cell) == SRP_EFrontOwner.BLEU && front.IsCellAtFront(cell))
				score += m_iTargetCell;
		}
		return score;
	}

	//------------------------------------------------------------------------------------------------
	//! MA8 : joueurs « retranchés » : le contact de la zone est resté à assaut_retranche_m pendant
	//! assaut_retranche_min, ou un joueur valide est à 100 m d'un point clé de la zone (carré orange, information visible)
	protected bool IsEntrenched(int zone, int nowUnix)
	{
		if (zone < 0)
			return false;
		SRP_CmdTrack track = m_mZoneTrack.Get(zone);
		if (track && nowUnix - track.m_iB <= 600 && track.m_iB - track.m_iA >= m_iEntrenchedMin * 60 && vector.DistanceXZ(track.m_vA, track.m_vB) <= m_iEntrenchedM)
			return true;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return false;
		SRP_FrontCapture capture = front.GetCapture();
		if (!capture)
			return false;
		int keyCount = front.GetKeyPointCount(zone);
		for (int k = 0; k < keyCount; k++)
		{
			if (capture.CountValidPlayersNear(front.GetKeyPointPos(zone, k), 100) > 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Point à assaut_synchro_distance_m de l'objectif sur l'axe venant de « from » (au sol)
	protected vector WaitPoint(vector objective, vector from)
	{
		vector away = from - objective;
		away[1] = 0;
		float length = away.Length();
		if (length < 1)
			return objective;
		float reach = m_iSyncDistanceM;
		if (reach > length)
			reach = length;
		return SRP_Placement.OnGround(objective + away * (reach / length));
	}

	//------------------------------------------------------------------------------------------------
	//! MA8 : base de feu à assaut_feu_min_m..max_m de l'objectif sur l'axe d'arrivée, au sol, avec vue (relief seul) ;
	//! 6 essais tournés de 15 degrés, sinon le point à 300 m
	protected vector FindFirePos(vector objective, vector from)
	{
		vector axis = from - objective;
		axis[1] = 0;
		float length = axis.Length();
		if (length < 1)
			axis = Vector(1, 0, 0);
		else
			axis = axis * (1 / length);
		float low = m_iFireMinM;
		float high = m_iFireMaxM;
		if (high < low)
			high = low;
		for (int attempt = 0; attempt < 6; attempt++)
		{
			int turn = (attempt + 1) / 2;
			float degrees = turn * 15;
			if (attempt - (attempt / 2) * 2 == 1)
				degrees = -degrees;
			vector direction = RotateFlat(axis, degrees * Math.DEG2RAD);
			float distance = Math.RandomFloat(low, high);
			vector candidate = SRP_Placement.OnGround(objective + direction * distance);
			if (SRP_Placement.IsWater(candidate))
				continue;
			if (SRP_Placement.HasLineOfSight(candidate, objective, null, true))
				return candidate;
		}
		return SRP_Placement.OnGround(objective + axis * 300);
	}

	//------------------------------------------------------------------------------------------------
	//! Rotation d'un vecteur plat autour de la verticale
	protected static vector RotateFlat(vector flat, float radians)
	{
		float c = Math.Cos(radians);
		float s = Math.Sin(radians);
		return Vector(flat[0] * c - flat[2] * s, 0, flat[0] * s + flat[2] * c);
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les groupes d'un plan (sans doublon) ajoutés à « into »
	protected void PlanGroups(SRP_CmdAssault plan, notnull array<ref SRP_EnemyGroup> into)
	{
		if (!plan)
			return;
		foreach (SRP_EnemyGroup fire : plan.m_aFire)
		{
			if (fire && into.Find(fire) < 0)
				into.Insert(fire);
		}
		foreach (SRP_EnemyGroup mover : plan.m_aMove)
		{
			if (mover && into.Find(mover) < 0)
				into.Insert(mover);
		}
		foreach (SRP_EnemyGroup axisB : plan.m_aAxisB)
		{
			if (axisB && into.Find(axisB) < 0)
				into.Insert(axisB);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool PlanHas(SRP_CmdAssault plan, SRP_EnemyGroup record)
	{
		if (!plan || !record)
			return false;
		return plan.m_aFire.Find(record) >= 0 || plan.m_aMove.Find(record) >= 0 || plan.m_aAxisB.Find(record) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Le plan de contre-attaque qui tient ce groupe, null sinon
	protected SRP_CmdAssault PlanOf(SRP_EnemyGroup record)
	{
		foreach (SRP_CmdAssault plan : m_aAssaults)
		{
			if (PlanHas(plan, record))
				return plan;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdAssault PlanOfWave(int wave, int zone)
	{
		foreach (SRP_CmdAssault plan : m_aAssaults)
		{
			if (plan && plan.m_iWave == wave && plan.m_iZone == zone)
				return plan;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveFromPlan(SRP_CmdAssault plan, SRP_EnemyGroup record)
	{
		if (!plan)
			return;
		plan.m_aFire.RemoveItem(record);
		plan.m_aMove.RemoveItem(record);
		plan.m_aAxisB.RemoveItem(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Départ rouge de l'axe du groupe (axe B s'il y est, sinon A ; sinon celui de l'attaque)
	protected vector OriginOf(SRP_EnemyGroup record, SRP_CounterAttack attack)
	{
		SRP_CmdAssault plan = PlanOf(record);
		if (plan)
		{
			if (plan.m_aAxisB.Find(record) >= 0 && plan.m_vOriginB != vector.Zero)
				return plan.m_vOriginB;
			if (plan.m_vOriginA != vector.Zero)
				return plan.m_vOriginA;
		}
		if (attack && attack.m_vOriginA != vector.Zero)
			return attack.m_vOriginA;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies && IsPresent(record))
			return enemies.GroupPosition(record);
		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Centre des groupes vivants d'un plan (son objectif s'il n'en a plus)
	protected vector PlanCentroid(SRP_EnemyComponent enemies, SRP_CmdAssault plan)
	{
		array<ref SRP_EnemyGroup> members = {};
		PlanGroups(plan, members);
		vector sum = vector.Zero;
		int count = 0;
		foreach (SRP_EnemyGroup member : members)
		{
			if (SRP_EnemyComponent.AliveAgents(member) <= 0)
				continue;
			sum = sum + enemies.GroupPosition(member);
			count++;
		}
		if (count == 0)
			return plan.m_vObjective;
		return sum * (1.0 / count);
	}

	//------------------------------------------------------------------------------------------------
	//! Une autre contre-attaque que celle suivie (nouvelle fin d'annonce) : l'état de la précédente est oublié
	protected void SyncAttack(SRP_CounterAttack attack)
	{
		if (!attack || attack.m_iWarnEndUnix == m_iCaKey)
			return;
		if (m_iCaKey != 0)
			ResetAttackState(attack.m_iWarnEndUnix);
		else
			m_iCaKey = attack.m_iWarnEndUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe appartient à la vague en cours de pose (échelonnement du front, 8 min au plus) : il suit encore l'ordre
	//! de la pose, son plan viendra avec OnWavePosted
	protected bool IsPosting(SRP_CounterAttack attack, SRP_EnemyGroup record, int nowUnix)
	{
		if (!attack || !record || attack.m_iWaves <= m_iCaPlannedWave)
			return false;
		if (nowUnix - m_iCaSeenWaveUnix >= 480)
			return false;
		return attack.m_aLastWave.Find(record) >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! État d'une contre-attaque remis à zéro (plans, papier, appuis tirés, 1re vague)
	protected void ResetAttackState(int key)
	{
		DropAttackPlans();
		m_iCaKey = key;
		m_iCaWave1Posted = 0;
		m_iCaPlannedWave = 0;
		m_iCaSeenWave = 0;
		m_iCaSeenWaveUnix = 0;
		m_bFeintRolled = false;
		m_bFeintFired = false;
		m_bFlybyRolled = false;
		m_bCaSmokeDone = false;
		m_bCaEndLogged = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Plans d'assaut et unités sur le papier oubliés : groupes relâchés (AllowMaxLOD), déclarations à la capture retirées
	protected void DropAttackPlans()
	{
		array<ref SRP_EnemyGroup> members = {};
		foreach (SRP_CmdAssault plan : m_aAssaults)
			PlanGroups(plan, members);
		foreach (SRP_EnemyGroup far : m_aFarGroups)
		{
			if (far && members.Find(far) < 0)
				members.Insert(far);
		}
		foreach (SRP_EnemyGroup member : members)
		{
			if (!member || !member.m_bAwake)
				continue;
			SRP_EnemyComponent.ReleaseAwake(member);
			member.m_bAwake = false;
		}
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_FrontCapture capture = null;
		if (front)
			capture = front.GetCapture();
		array<int> zones = {};
		foreach (SRP_CmdPaperUnit unit : m_aPaper)
		{
			if (unit && zones.Find(unit.m_iZone) < 0)
				zones.Insert(unit.m_iZone);
		}
		if (capture)
		{
			foreach (int zone : zones)
			{
				if (zone >= 0)
					capture.ClearPaperAssault(zone);
			}
		}
		m_aAssaults.Clear();
		m_aPaper.Clear();
		m_aFarGroups.Clear();
		m_aFarSince.Clear();
		m_aReordered.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected string JoinLines(array<string> lines)
	{
		string text = "";
		foreach (string line : lines)
		{
			if (!text.IsEmpty())
				text += "\n";
			text += line;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Heure locale du serveur d'un instant Unix passé, « 21:10 »
	protected string ClockText(int unixTime)
	{
		int hour;
		int minute;
		int second;
		System.GetHourMinuteSecond(hour, minute, second);
		int seconds = hour * 3600 + minute * 60 + second - (System.GetUnixTime() - unixTime);
		while (seconds < 0)
			seconds += 86400;
		seconds = seconds - (seconds / 86400) * 86400;
		int clockHour = seconds / 3600;
		int clockMinute = (seconds - clockHour * 3600) / 60;
		return SRP_Time.Pad2(clockHour) + ":" + SRP_Time.Pad2(clockMinute);
	}
}
