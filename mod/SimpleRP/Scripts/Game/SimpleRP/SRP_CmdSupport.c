//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : LES APPUIS (feux et hélico).
//
// RÔLE (serveur ; tenu par SRP_Commander.m_Support, accès SRP_CmdSupport.Get()) : EXÉCUTE les appuis que le cerveau
// (ou les manœuvres pour une contre-attaque, CA4) a décidés ; il ne choisit AUCUNE cible et ne tient aucun registre
// « en route » (trou 4 : seuls les tickets de SRP_CmdResources, dont il ne garde que les numéros).
// - Mortier (MO1 à MO9) : une équipe par région (vraie pièce appui_mortier_piece + 2 servants, classe COMBAT, rôle
//   SERVANT), posée hors de vue côté rouge à appui_mortier_distance_min_m..max_m de la cible, obus tirés par script
//   (obus lâché au-dessus du point, ProjectileMoveComponent.Launch), bruit de départ par la RPC de
//   SRP_FrontEnemyComponent.BroadcastSound (MO3), tir qui se rapproche tant qu'un soldat voit la cible (MO4),
//   réduite au silence 24 h (MO9, sauvé). C5 : fumigènes GRATUITS (ni obus, ni 2 tirs par heure, ni règle des
//   20 min) ; pas de fumée si la pièce de la région est muette.
// - Rideau de fumée (MO7) et feinte (MO8), artillerie lourde 2S1 (AP1, AP2, EQ1 : coups de réglage puis salve,
//   batterie posée à 2-3 km, verrou d'entrée par SCR_GetInUserAction moddé dans SRP_CmdArmor.c), bombe FAB-500 (AP3,
//   EQ1 : Su-57 puis bombe, 24 h glissantes, 10 joueurs valides à 3 km), passages d'avion, hélico de recherche
//   (AP7 : SRP_HeliSearch.Launch, sur son stock), blindés (SRP_CmdArmor.c, tenu par m_Armor).
// - Contrôles appliqués ICI : sécurité (base, mer, soldats ennemis ou civils trop près : TOUJOURS, Staff compris),
//   EQ3 (plafonds, une grosse opération à la fois, pas deux frappes au même endroit en 20 min), EQ4 (seuils de
//   joueurs à 3 km par SRP_Commander.GetDosingPlayers), MO6 (carré rouge ou bleu du front), interdictions de 24 h,
//   gel VU6 et désorganisation OF6/OF8 (hors Staff), stocks par Reserve/Consume/Release.
// - Staff (ordre m_bStaff) : sans stock (gratuit si cmd_staff_gratuit), plafond, seuil, test de carré, gel ni
//   désorganisation, et rien n'est compté aux plafonds ; la sécurité et les interdictions de 24 h restent (le Staff
//   les lève par ClearBans).
// Chaque Request* rend "" si l'appui part, sinon la raison du refus ; c'est l'appelant qui écrit SRP_CmdLog.Decision
// ou SRP_CmdLog.Refusal (§1.8 : le cerveau par IssueOrder, les manœuvres par LogSupport) ; ici, seulement des lignes
// d'exécution (SRP_CmdLog.Note, APPUI). CallLater ne sert qu'à enchaîner les coups (FireNextId, DropShell, DropBomb) ;
// tout le reste part de Tick (5 s).
// Sauvegarde : clés cmd_ap_* (interdictions de 24 h, plafonds EQ3) ; aucun extra (trou 16). Pièces, batteries et
// missions de tir ne sont jamais sauvées (leurs tickets sont rendus au redémarrage par SRP_CmdResources).
// APPELÉ PAR : SRP_Commander (ordres MORTIER, FUMEE, FEINTE, ARTILLERIE, BOMBE, AVION, BLINDE, HELICO, sabotage),
// SRP_CmdManeuvers (CA4, feinte, rideau de décrochage, harcèlement), SRP_FrontEnemyComponent (RPC : PlaySoundLocal,
// LaunchLocal), SRP_CmdArmor.c (verrou IsBatteryVehicle), SRP_CmdScreens (Staff).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! L'équipe de mortier d'une région (MO1, MO2, MO9). Non sauvée (pièce reposée à la demande).
class SRP_CmdMortarTeam
{
	int m_iRegion = -1;
	IEntity m_Piece;
	ref SRP_EnemyGroup m_Servants;
	vector m_vPos;
	int m_iPlacedUnix;
	int m_iLastNeededUnix;
	int m_iNoServantSinceUnix;
	bool m_bSabotaged;
	bool m_bSilenced;
	bool m_bFiring;
	float m_fHealth0 = -1;						// points de vie de la pièce au premier passage (MO9 : sortie de sa phase de départ)
}

//------------------------------------------------------------------------------------------------
//! La batterie 2S1 d'une région (AP1). Non sauvée.
class SRP_CmdBattery
{
	int m_iRegion = -1;
	IEntity m_Vehicle;
	ref SRP_EnemyGroup m_Guard;
	vector m_vPos;
	int m_iPlacedUnix;
	int m_iLastNeededUnix;
	bool m_bDestroyed;
}

//------------------------------------------------------------------------------------------------
//! Une mission de tir en cours (mortier, rideau, feinte, artillerie). Non sauvée : son ticket est rendu au
//! redémarrage par SRP_CmdResources.
class SRP_CmdFireMission
{
	int m_iKind = SRP_ECmdOrder.MORTIER;		// MORTIER, FUMEE, FEINTE ou ARTILLERIE
	int m_iShell = SRP_ECmdShell.EXPLOSIF;
	int m_iRegion = -1;
	int m_iZone = -1;
	vector m_vAim;
	float m_fError;								// MO4 : écart courant, en mètres
	int m_iShellsLeft;
	int m_iFired;
	int m_iSkippedInRow;						// coups sautés de suite (sécurité)
	int m_iTicket;								// ticket de SRP_CmdResources (0 = gratuit)
	bool m_bStaff;
	bool m_bSalvo;								// salve d'artillerie commencée
	string m_sReason;
	ref array<vector> m_aFixed = {};			// points imposés (rideau, feinte ; coups de réglage de l'artillerie)
	int m_iId;									// numéro passé aux CallLater (jamais l'objet lui-même)
	int m_iHeFired;								// obus explosifs déjà partis (MO4 : l'écart ne se resserre qu'à partir du 2e)
}

//------------------------------------------------------------------------------------------------
//! Une frappe récente (EQ3 : pas deux frappes au même endroit en appui_meme_endroit_min). Sauvée (cmd_ap_frappes).
class SRP_CmdStrike
{
	vector m_vPos;
	int m_iUnix;
}

//------------------------------------------------------------------------------------------------
//! Les appuis du Commandeur (serveur).
class SRP_CmdSupport
{
	// --- Réglages (clés appui_* sauf appui_blinde_*) -------------------------------------------------------------
	bool m_bOn = true;		// appui_actif
	int m_iBaseM = 1500;		// appui_distance_base_m
	int m_iSafeHeM = 150;		// appui_securite_explosif_m
	int m_iSafeSmokeM = 40;		// appui_securite_fumee_m
	int m_iSafeSalvoM = 220;		// appui_securite_salve_m
	int m_iSafeBombM = 250;		// appui_securite_bombe_m
	int m_iSameSpotM = 400;		// appui_meme_endroit_m
	int m_iSameSpotMin = 20;		// appui_meme_endroit_min
	int m_iPlayersM = 3000;		// appui_rayon_joueurs_m
	int m_iShellHeightM = 200;		// appui_obus_hauteur_m
	float m_fShellSpeedCoef = 1.0;		// appui_obus_coef_vitesse
	bool m_bShellClientRelaunch = false;		// appui_obus_relance_clients
	string m_sShellHe = "{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et";		// appui_obus_explosif
	string m_sShellSmoke = "{A544A2C131DE2C64}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_Smoke_D832DU.et";		// appui_obus_fumee
	int m_iMortarPlayers = 3;		// appui_mortier_seuil_joueurs
	int m_iMortarPerHour = 2;		// appui_mortier_tirs_heure
	bool m_bSmokeCounts = false;		// appui_mortier_fumee_compte
	int m_iMortarDelayMinS = 60;		// appui_mortier_delai_min_s
	int m_iMortarDelayMaxS = 120;		// appui_mortier_delai_max_s
	int m_iMortarShellsMin = 4;		// appui_mortier_obus_min
	int m_iMortarShellsMax = 6;		// appui_mortier_obus_max
	int m_iMortarRateMinS = 10;		// appui_mortier_cadence_min_s
	int m_iMortarRateMaxS = 15;		// appui_mortier_cadence_max_s
	int m_iMortarErrorStartM = 150;		// appui_mortier_erreur_depart_m
	int m_iMortarErrorStepM = 40;		// appui_mortier_erreur_pas_m
	int m_iMortarErrorMinM = 30;		// appui_mortier_erreur_min_m
	int m_iMortarObserverM = 100;		// appui_mortier_observateur_m
	int m_iMortarPieceMinM = 800;		// appui_mortier_distance_min_m
	int m_iMortarPieceMaxM = 1200;		// appui_mortier_distance_max_m
	int m_iMortarRangeM = 3500;		// appui_mortier_portee_max_m
	int m_iMortarPlayerMinM = 600;		// appui_mortier_joueur_min_m
	int m_iMortarPresenceM = 2000;		// appui_mortier_presence_m
	int m_iMortarRetireMin = 5;		// appui_mortier_retrait_min
	int m_iMortarCrew = 2;		// appui_mortier_servants
	int m_iMortarCrewM = 25;		// appui_mortier_servant_rayon_m
	int m_iMortarSilenceConfirmS = 10;		// appui_mortier_silence_confirm_s
	int m_iMortarSilenceHours = 24;		// appui_mortier_silence_heures
	int m_iMortarFlightS = 15;		// appui_mortier_temps_vol_s
	string m_sMortarPiece = "{6A5B0C0D0E0F7A05}Prefabs/Missions/SRP_Piece_Mortier.et";		// appui_mortier_piece
	string m_sMortarCrewGroup = "{56FD583BBC989204}Prefabs/Groups/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Group_RHS_RF_MSV_VKPO_DS_SentryTeam.et";		// appui_mortier_servants_groupe
	string m_sMortarSound = "{3A984BD46A47EEC8}Sounds/Weapons/Mortars/2B14/Weapons_Mortars_2B14_Shot.acp";		// appui_mortier_son
	string m_sMortarSoundEvent = "SOUND_SHOT";		// appui_mortier_son_evenement
	int m_iSmokeShells = 3;		// appui_fumee_obus
	int m_iSmokeAheadM = 80;		// appui_fumee_devant_joueurs_m
	int m_iSmokeWidthM = 100;		// appui_fumee_largeur_m
	int m_iSmokeSpreadM = 20;		// appui_fumee_dispersion_m
	int m_iSmokeTriggerM = 400;		// appui_fumee_declenchement_m
	int m_iFeintOffsetM = 300;		// appui_feinte_decalage_m
	int m_iFeintHeShells = 2;		// appui_feinte_obus_explosifs
	int m_iFeintSmokeShells = 3;		// appui_feinte_obus_fumee
	int m_iArtilleryPlayers = 8;		// appui_artillerie_seuil_joueurs
	int m_iArtilleryIntervalH = 2;		// appui_artillerie_intervalle_h
	int m_iAdjustMin = 1;		// appui_artillerie_reglage_min
	int m_iAdjustMax = 2;		// appui_artillerie_reglage_max
	int m_iAdjustOffsetM = 100;		// appui_artillerie_reglage_ecart_m
	int m_iAdjustLeadMinS = 30;		// appui_artillerie_avance_min_s
	int m_iAdjustLeadMaxS = 60;		// appui_artillerie_avance_max_s
	int m_iFirstShotMinS = 20;		// appui_artillerie_premier_coup_min_s
	int m_iFirstShotMaxS = 40;		// appui_artillerie_premier_coup_max_s
	int m_iSalvoMin = 25;		// appui_artillerie_salve_min
	int m_iSalvoMax = 30;		// appui_artillerie_salve_max
	int m_iSalvoRadiusM = 70;		// appui_artillerie_rayon_m
	int m_iSalvoRateMinS = 2;		// appui_artillerie_cadence_min_s
	int m_iSalvoRateMaxS = 3;		// appui_artillerie_cadence_max_s
	int m_iBatteryMinM = 2000;		// appui_artillerie_batterie_min_m
	int m_iBatteryMaxM = 3000;		// appui_artillerie_batterie_max_m
	int m_iBatteryRangeM = 3500;		// appui_artillerie_portee_max_m
	int m_iBatteryGuardM = 1000;		// appui_artillerie_garde_m
	int m_iBatteryPresenceM = 3000;		// appui_artillerie_presence_m
	int m_iBatteryRetireMin = 10;		// appui_artillerie_retrait_min
	int m_iBatterySilenceHours = 24;		// appui_artillerie_silence_heures
	string m_sBatteryPrefab = "{D8136D90BE12445F}Prefabs/Vehicles/Tracked/2S1/Tank_2S1.et";		// appui_artillerie_batterie
	string m_sBatterySound = "";		// appui_artillerie_son
	string m_sBatterySoundEvent = "";		// appui_artillerie_son_evenement
	int m_iBombPlayers = 10;		// appui_bombe_seuil_joueurs
	int m_iBombIntervalH = 24;		// appui_bombe_intervalle_h
	int m_iBombSpreadM = 3;		// appui_bombe_dispersion_m
	int m_iBombPlaneLeadS = 10;		// appui_bombe_avion_avance_s
	string m_sBombPrefab = "{B881CA7B63D4EDF5}Prefabs/Vehicles/Bombs/UMPK500/UMPK500_CAS.et";		// appui_bombe_prefab
	string m_sPlanePrefab = "{FDD01BF3CEAB37E3}Prefabs/Vehicles/Airplanes/SU57/SU57_Flyby.et";		// appui_avion_prefab
	int m_iFlybysPerDay = 3;		// appui_avion_passages_24h
	int m_iHeliRestMin = 30;		// appui_helico_repos_min

	// --- Constantes ----------------------------------------------------------------------------------------------
	protected static const string BATTERY_MARK = "Tracked/2S1/";	// AP1 : un prefab dont le nom contient ceci est une batterie (constante : le client ne lit pas les réglages)
	protected static const int SPOT_TRIES = 3;						// essais de pose d'une pièce ou d'une batterie
	protected static const float CREW_HOLD_M = 12;					// les servants tiennent ce rayon autour de la pièce
	protected static const float GUARD_HOLD_M = 40;				// la garde de la batterie tient ce rayon
	protected static const int ADJUST_GAP_MIN_S = 8;				// écart entre deux coups de réglage
	protected static const int ADJUST_GAP_MAX_S = 15;
	protected static const int SALVO_SKIP_MAX = 3;					// obus de salve sautés de suite avant la fin du tir
	protected static const int MORTAR_SKIP_MAX = 2;					// MO6 : obus de mortier sautés de suite avant la fin du tir
	protected static const int LAUNCH_TRIES = 5;					// relance d'obus chez un client : essais (entité pas encore arrivée)
	protected static const int SERVANT_WAIT_S = 5;					// servants encore en pose au moment du coup : nouvel essai
	protected static const int HOUR_S = 3600;
	protected static const int DAY_S = 86400;

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected SRP_Commander m_Commander;
	protected ref SRP_CmdArmorManager m_Armor;							// blindés (SRP_CmdArmor.c)
	protected ref array<ref SRP_CmdMortarTeam> m_aTeams = {};			// une par région au plus
	protected ref array<ref SRP_CmdBattery> m_aBatteries = {};
	protected ref array<ref SRP_CmdFireMission> m_aMissions = {};
	protected ref array<ref SRP_CmdStrike> m_aStrikes = {};			// sauvées
	protected ref array<int> m_aMortarUnix = {};						// tirs explosifs de la dernière heure (EQ3, sauvés)
	protected ref array<int> m_aFlybyUnix = {};						// passages d'avion sur 24 h (sauvés)
	protected ref array<int> m_aMortarBanUntil = {};					// par région : muette jusqu'à (MO9, sauvé)
	protected ref array<int> m_aArtilleryBanUntil = {};				// par région : batterie détruite jusqu'à (AP1, sauvé)
	protected int m_iArtilleryUnix;									// dernier tir lourd (EQ3, sauvé)
	protected int m_iBombUnix;										// dernière bombe (AP3, sauvé)
	protected int m_iHeliUnix;										// dernière sortie d'hélico (sauvé)
	protected int m_iHeliTicket;										// ticket de la sortie en cours
	protected int m_iNextMissionId = 1;
	protected bool m_bBombPending;									// Su-57 passé, bombe pas encore larguée
	protected int m_iBombTicket;
	protected int m_iBombRegion = -1;
	protected bool m_bBombStaff;
	protected ref array<IEntity> m_aDoomed = {};						// décors à effacer dès qu'aucun joueur ne les voit
	protected static ref array<IEntity> s_aSafetyHits = {};			// rappel de la recherche de sécurité

	//================================================================================================
	// Vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Constructeur seul : m_Armor = new SRP_CmdArmorManager(this) — SRP_Commander (constructeur)
	void SRP_CmdSupport(SRP_Commander commander)
	{
		m_Commander = commander;
		m_Armor = new SRP_CmdArmorManager(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Les appuis, null sans Commandeur — cerveau, manœuvres, écrans
	static SRP_CmdSupport Get()
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return null;
		return commander.GetSupport();
	}

	//------------------------------------------------------------------------------------------------
	//! Les blindés — SRP_CmdScreens, SRP_Commander
	SRP_CmdArmorManager GetArmor()
	{
		return m_Armor;
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés puis m_Armor.DeclareSettings — SRP_Commander.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareBool("appui_actif", m_bOn, "Appuis du Commandeur (mortier, artillerie, aviation, blindés, hélico) (VU6)");
		SRP_CmdSettings.DeclareInt("appui_distance_base_m", m_iBaseM, "Aucune frappe à cette distance de la base, en mètres (MO6)");
		SRP_CmdSettings.DeclareInt("appui_securite_explosif_m", m_iSafeHeM, "Aucun obus explosif à cette distance d'un soldat ennemi ou d'un civil, en mètres (MO6)");
		SRP_CmdSettings.DeclareInt("appui_securite_fumee_m", m_iSafeSmokeM, "Aucun fumigène à cette distance d'un soldat ennemi ou d'un civil, en mètres (MO7)");
		SRP_CmdSettings.DeclareInt("appui_securite_salve_m", m_iSafeSalvoM, "Salve d'artillerie : sécurité à la décision, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_securite_bombe_m", m_iSafeBombM, "Bombe : sécurité, en mètres (AP3)");
		SRP_CmdSettings.DeclareInt("appui_meme_endroit_m", m_iSameSpotM, "Jamais deux frappes à cette distance l'une de l'autre, en mètres (EQ3)");
		SRP_CmdSettings.DeclareInt("appui_meme_endroit_min", m_iSameSpotMin, "En moins de ce nombre de minutes (EQ3)");
		SRP_CmdSettings.DeclareInt("appui_rayon_joueurs_m", m_iPlayersM, "Seuils de joueurs comptés à cette distance de la cible, en mètres (EQ4)");
		SRP_CmdSettings.DeclareInt("appui_obus_hauteur_m", m_iShellHeightM, "Obus lâché à cette hauteur au-dessus du point, en mètres (MO3)");
		SRP_CmdSettings.DeclareFloat("appui_obus_coef_vitesse", m_fShellSpeedCoef, "Coefficient de vitesse de l'obus (MO3)");
		SRP_CmdSettings.DeclareBool("appui_obus_relance_clients", m_bShellClientRelaunch, "Repli : chaque client relance l'obus (si le vol ne se voit pas chez les joueurs) (MO3)");
		SRP_CmdSettings.DeclareString("appui_obus_explosif", m_sShellHe, "Obus explosif de 82 mm (MO1)");
		SRP_CmdSettings.DeclareString("appui_obus_fumee", m_sShellSmoke, "Obus fumigène de 82 mm (MO7)");
		SRP_CmdSettings.DeclareInt("appui_mortier_seuil_joueurs", m_iMortarPlayers, "Mortier dès ce nombre de joueurs à 3 km (EQ4)");
		SRP_CmdSettings.DeclareInt("appui_mortier_tirs_heure", m_iMortarPerHour, "Tirs de mortier explosifs au plus par heure, toute l'île (EQ3)");
		SRP_CmdSettings.DeclareBool("appui_mortier_fumee_compte", m_bSmokeCounts, "Les fumigènes comptent dans les tirs par heure (0 : ils sont gratuits) (C5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_delai_min_s", m_iMortarDelayMinS, "Premier obus au plus tôt, en secondes après la décision (MO5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_delai_max_s", m_iMortarDelayMaxS, "Premier obus au plus tard, en secondes (MO5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_obus_min", m_iMortarShellsMin, "Obus par tir, au moins (MO5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_obus_max", m_iMortarShellsMax, "Obus par tir, au plus (MO5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_cadence_min_s", m_iMortarRateMinS, "Écart entre deux obus, au moins, en secondes (MO5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_cadence_max_s", m_iMortarRateMaxS, "Écart entre deux obus, au plus, en secondes (MO5)");
		SRP_CmdSettings.DeclareInt("appui_mortier_erreur_depart_m", m_iMortarErrorStartM, "Écart du premier obus, en mètres (MO4)");
		SRP_CmdSettings.DeclareInt("appui_mortier_erreur_pas_m", m_iMortarErrorStepM, "Rapprochement à chaque obus tant qu'un soldat voit la cible, en mètres (MO4)");
		SRP_CmdSettings.DeclareInt("appui_mortier_erreur_min_m", m_iMortarErrorMinM, "Écart minimal, en mètres (MO4)");
		SRP_CmdSettings.DeclareInt("appui_mortier_observateur_m", m_iMortarObserverM, "Un soldat voit la cible s'il identifie un joueur à cette distance, en mètres (MO4)");
		SRP_CmdSettings.DeclareInt("appui_mortier_distance_min_m", m_iMortarPieceMinM, "Pièce posée à cette distance au moins de la cible, en mètres (MO2)");
		SRP_CmdSettings.DeclareInt("appui_mortier_distance_max_m", m_iMortarPieceMaxM, "Pièce posée à cette distance au plus, en mètres (MO2)");
		SRP_CmdSettings.DeclareInt("appui_mortier_portee_max_m", m_iMortarRangeM, "Pièce gardée tant que la cible est à cette distance, en mètres (MO2)");
		SRP_CmdSettings.DeclareInt("appui_mortier_joueur_min_m", m_iMortarPlayerMinM, "Pièce jamais posée plus près d'un joueur, en mètres (MO2)");
		SRP_CmdSettings.DeclareInt("appui_mortier_presence_m", m_iMortarPresenceM, "Pièce retirée quand plus aucun joueur n'est à cette distance, en mètres (MO2)");
		SRP_CmdSettings.DeclareInt("appui_mortier_retrait_min", m_iMortarRetireMin, "Depuis ce nombre de minutes, hors de vue (MO2)");
		SRP_CmdSettings.DeclareInt("appui_mortier_servants", m_iMortarCrew, "Servants de la pièce (MO1)");
		SRP_CmdSettings.DeclareInt("appui_mortier_servant_rayon_m", m_iMortarCrewM, "Un servant est à son poste à cette distance de la pièce, en mètres (MO9)");
		SRP_CmdSettings.DeclareInt("appui_mortier_silence_confirm_s", m_iMortarSilenceConfirmS, "Pièce muette après ce nombre de secondes sans servant à son poste (MO9)");
		SRP_CmdSettings.DeclareInt("appui_mortier_silence_heures", m_iMortarSilenceHours, "Pièce réduite au silence : région sans mortier pendant ce nombre d'heures (MO9)");
		SRP_CmdSettings.DeclareInt("appui_mortier_temps_vol_s", m_iMortarFlightS, "Délai entre le départ et l'arrivée de l'obus, en secondes (MO3)");
		SRP_CmdSettings.DeclareString("appui_mortier_piece", m_sMortarPiece, "Pièce de mortier (prefab à créer, voir CONTRATS_CMD.md) (MO1)");
		SRP_CmdSettings.DeclareString("appui_mortier_servants_groupe", m_sMortarCrewGroup, "Groupe des servants (MO1)");
		SRP_CmdSettings.DeclareString("appui_mortier_son", m_sMortarSound, "Bruit de départ (projet sonore) (MO3)");
		SRP_CmdSettings.DeclareString("appui_mortier_son_evenement", m_sMortarSoundEvent, "Évènement du bruit de départ (à vérifier au Workbench) (MO3)");
		SRP_CmdSettings.DeclareInt("appui_fumee_obus", m_iSmokeShells, "Fumigènes d'un rideau (MO7)");
		SRP_CmdSettings.DeclareInt("appui_fumee_devant_joueurs_m", m_iSmokeAheadM, "Rideau à cette distance devant les joueurs, en mètres (MO7)");
		SRP_CmdSettings.DeclareInt("appui_fumee_largeur_m", m_iSmokeWidthM, "Largeur du rideau, en mètres (MO7)");
		SRP_CmdSettings.DeclareInt("appui_fumee_dispersion_m", m_iSmokeSpreadM, "Dispersion des fumigènes, en mètres (MO7)");
		SRP_CmdSettings.DeclareInt("appui_fumee_declenchement_m", m_iSmokeTriggerM, "Rideau lancé quand la vague est à cette distance de la cible, en mètres (MO7)");
		SRP_CmdSettings.DeclareInt("appui_feinte_decalage_m", m_iFeintOffsetM, "Entrée du faux axe à cette distance de la cible, en mètres (MO8)");
		SRP_CmdSettings.DeclareInt("appui_feinte_obus_explosifs", m_iFeintHeShells, "Obus explosifs d'une feinte (MO8)");
		SRP_CmdSettings.DeclareInt("appui_feinte_obus_fumee", m_iFeintSmokeShells, "Fumigènes d'une feinte (MO8)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_seuil_joueurs", m_iArtilleryPlayers, "Artillerie lourde dès ce nombre de joueurs à 3 km (EQ4)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_intervalle_h", m_iArtilleryIntervalH, "Au plus un tir d'artillerie lourde toutes les ce nombre d'heures (EQ3)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_reglage_min", m_iAdjustMin, "Coups de réglage, au moins (EQ1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_reglage_max", m_iAdjustMax, "Coups de réglage, au plus (EQ1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_reglage_ecart_m", m_iAdjustOffsetM, "Coups de réglage à cette distance de la cible, en mètres (EQ1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_avance_min_s", m_iAdjustLeadMinS, "Salve au plus tôt ce nombre de secondes après le réglage (EQ1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_avance_max_s", m_iAdjustLeadMaxS, "Salve au plus tard ce nombre de secondes après le réglage (EQ1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_premier_coup_min_s", m_iFirstShotMinS, "Premier coup de réglage au plus tôt, en secondes après la décision (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_premier_coup_max_s", m_iFirstShotMaxS, "Premier coup de réglage au plus tard, en secondes (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_salve_min", m_iSalvoMin, "Obus de la salve, au moins (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_salve_max", m_iSalvoMax, "Obus de la salve, au plus (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_rayon_m", m_iSalvoRadiusM, "Rayon de la salve, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_cadence_min_s", m_iSalvoRateMinS, "Écart entre deux obus de la salve, au moins, en secondes (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_cadence_max_s", m_iSalvoRateMaxS, "Écart entre deux obus de la salve, au plus, en secondes (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_batterie_min_m", m_iBatteryMinM, "Batterie posée à cette distance au moins de la cible, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_batterie_max_m", m_iBatteryMaxM, "Batterie posée à cette distance au plus, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_portee_max_m", m_iBatteryRangeM, "Batterie gardée tant que la cible est à cette distance, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_garde_m", m_iBatteryGuardM, "Garde de la batterie posée quand un joueur est à cette distance, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_presence_m", m_iBatteryPresenceM, "Batterie retirée quand plus aucun joueur n'est à cette distance, en mètres (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_retrait_min", m_iBatteryRetireMin, "Depuis ce nombre de minutes, hors de vue (AP1)");
		SRP_CmdSettings.DeclareInt("appui_artillerie_silence_heures", m_iBatterySilenceHours, "Batterie détruite : région sans artillerie lourde pendant ce nombre d'heures (AP1)");
		SRP_CmdSettings.DeclareString("appui_artillerie_batterie", m_sBatteryPrefab, "Automoteur 2S1 (AP1)");
		SRP_CmdSettings.DeclareString("appui_artillerie_son", m_sBatterySound, "Bruit de départ de la batterie (vide = aucun) (AP1)");
		SRP_CmdSettings.DeclareString("appui_artillerie_son_evenement", m_sBatterySoundEvent, "Évènement du bruit de départ de la batterie (AP1)");
		SRP_CmdSettings.DeclareInt("appui_bombe_seuil_joueurs", m_iBombPlayers, "Bombe dès ce nombre de joueurs valides à 3 km (AP3)");
		SRP_CmdSettings.DeclareInt("appui_bombe_intervalle_h", m_iBombIntervalH, "Au plus une bombe toutes les ce nombre d'heures (glissantes) ; c'est aussi le temps pour refaire la bombe du stock (AP3)");
		SRP_CmdSettings.DeclareInt("appui_bombe_dispersion_m", m_iBombSpreadM, "Bombe tombée à cette distance du point, au plus, en mètres (AP3)");
		SRP_CmdSettings.DeclareInt("appui_bombe_avion_avance_s", m_iBombPlaneLeadS, "L'avion est entendu ce nombre de secondes avant la bombe (EQ1)");
		SRP_CmdSettings.DeclareString("appui_bombe_prefab", m_sBombPrefab, "Bombe FAB-500 (RHS) (AP3)");
		SRP_CmdSettings.DeclareString("appui_avion_prefab", m_sPlanePrefab, "Passage de Su-57 (RHS) (AP3)");
		SRP_CmdSettings.DeclareInt("appui_avion_passages_24h", m_iFlybysPerDay, "Passages d'avion au plus sur 24 h glissantes (AP3)");
		SRP_CmdSettings.DeclareInt("appui_helico_repos_min", m_iHeliRestMin, "Au plus une sortie d'hélico toutes les ce nombre de minutes (AP7)");
		if (!m_Armor)
			return;
		m_Armor.DeclareSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés puis m_Armor.LoadSettings (garde AP4 : T14, K17, Kurganets, T90 refusés) — SRP_Commander.LoadSettings
	void LoadSettings()
	{
		m_bOn = SRP_CmdSettings.GetBool("appui_actif");
		m_iBaseM = SRP_CmdSettings.GetIntClamped("appui_distance_base_m", 0, 10000);
		m_iSafeHeM = SRP_CmdSettings.GetIntClamped("appui_securite_explosif_m", 0, 1000);
		m_iSafeSmokeM = SRP_CmdSettings.GetIntClamped("appui_securite_fumee_m", 0, 500);
		m_iSafeSalvoM = SRP_CmdSettings.GetIntClamped("appui_securite_salve_m", 0, 1000);
		m_iSafeBombM = SRP_CmdSettings.GetIntClamped("appui_securite_bombe_m", 0, 1000);
		m_iSameSpotM = SRP_CmdSettings.GetIntClamped("appui_meme_endroit_m", 0, 3000);
		m_iSameSpotMin = SRP_CmdSettings.GetIntClamped("appui_meme_endroit_min", 0, 240);
		m_iPlayersM = SRP_CmdSettings.GetIntClamped("appui_rayon_joueurs_m", 500, 10000);
		m_iShellHeightM = SRP_CmdSettings.GetIntClamped("appui_obus_hauteur_m", 20, 1000);
		m_fShellSpeedCoef = SRP_CmdSettings.GetFloatClamped("appui_obus_coef_vitesse", 0.1, 5.0);
		m_bShellClientRelaunch = SRP_CmdSettings.GetBool("appui_obus_relance_clients");
		m_sShellHe = SRP_CmdSettings.GetString("appui_obus_explosif");
		m_sShellSmoke = SRP_CmdSettings.GetString("appui_obus_fumee");
		m_iMortarPlayers = SRP_CmdSettings.GetIntClamped("appui_mortier_seuil_joueurs", 1, 50);
		m_iMortarPerHour = SRP_CmdSettings.GetIntClamped("appui_mortier_tirs_heure", 0, 20);
		m_bSmokeCounts = SRP_CmdSettings.GetBool("appui_mortier_fumee_compte");
		m_iMortarDelayMinS = SRP_CmdSettings.GetIntClamped("appui_mortier_delai_min_s", 0, 900);
		m_iMortarDelayMaxS = SRP_CmdSettings.GetIntClamped("appui_mortier_delai_max_s", 0, 900);
		m_iMortarShellsMin = SRP_CmdSettings.GetIntClamped("appui_mortier_obus_min", 1, 30);
		m_iMortarShellsMax = SRP_CmdSettings.GetIntClamped("appui_mortier_obus_max", 1, 30);
		m_iMortarRateMinS = SRP_CmdSettings.GetIntClamped("appui_mortier_cadence_min_s", 1, 120);
		m_iMortarRateMaxS = SRP_CmdSettings.GetIntClamped("appui_mortier_cadence_max_s", 1, 120);
		m_iMortarErrorStartM = SRP_CmdSettings.GetIntClamped("appui_mortier_erreur_depart_m", 0, 1000);
		m_iMortarErrorStepM = SRP_CmdSettings.GetIntClamped("appui_mortier_erreur_pas_m", 0, 500);
		m_iMortarErrorMinM = SRP_CmdSettings.GetIntClamped("appui_mortier_erreur_min_m", 0, 500);
		m_iMortarObserverM = SRP_CmdSettings.GetIntClamped("appui_mortier_observateur_m", 10, 1000);
		m_iMortarPieceMinM = SRP_CmdSettings.GetIntClamped("appui_mortier_distance_min_m", 100, 5000);
		m_iMortarPieceMaxM = SRP_CmdSettings.GetIntClamped("appui_mortier_distance_max_m", 100, 5000);
		m_iMortarRangeM = SRP_CmdSettings.GetIntClamped("appui_mortier_portee_max_m", 500, 10000);
		m_iMortarPlayerMinM = SRP_CmdSettings.GetIntClamped("appui_mortier_joueur_min_m", 0, 5000);
		m_iMortarPresenceM = SRP_CmdSettings.GetIntClamped("appui_mortier_presence_m", 100, 10000);
		m_iMortarRetireMin = SRP_CmdSettings.GetIntClamped("appui_mortier_retrait_min", 0, 120);
		m_iMortarCrew = SRP_CmdSettings.GetIntClamped("appui_mortier_servants", 1, 6);
		m_iMortarCrewM = SRP_CmdSettings.GetIntClamped("appui_mortier_servant_rayon_m", 5, 200);
		m_iMortarSilenceConfirmS = SRP_CmdSettings.GetIntClamped("appui_mortier_silence_confirm_s", 0, 120);
		m_iMortarSilenceHours = SRP_CmdSettings.GetIntClamped("appui_mortier_silence_heures", 0, 240);
		m_iMortarFlightS = SRP_CmdSettings.GetIntClamped("appui_mortier_temps_vol_s", 1, 120);
		m_sMortarPiece = SRP_CmdSettings.GetString("appui_mortier_piece");
		m_sMortarCrewGroup = SRP_CmdSettings.GetString("appui_mortier_servants_groupe");
		m_sMortarSound = SRP_CmdSettings.GetString("appui_mortier_son");
		m_sMortarSoundEvent = SRP_CmdSettings.GetString("appui_mortier_son_evenement");
		m_iSmokeShells = SRP_CmdSettings.GetIntClamped("appui_fumee_obus", 1, 12);
		m_iSmokeAheadM = SRP_CmdSettings.GetIntClamped("appui_fumee_devant_joueurs_m", 0, 500);
		m_iSmokeWidthM = SRP_CmdSettings.GetIntClamped("appui_fumee_largeur_m", 10, 500);
		m_iSmokeSpreadM = SRP_CmdSettings.GetIntClamped("appui_fumee_dispersion_m", 0, 200);
		m_iSmokeTriggerM = SRP_CmdSettings.GetIntClamped("appui_fumee_declenchement_m", 50, 3000);
		m_iFeintOffsetM = SRP_CmdSettings.GetIntClamped("appui_feinte_decalage_m", 50, 2000);
		m_iFeintHeShells = SRP_CmdSettings.GetIntClamped("appui_feinte_obus_explosifs", 0, 12);
		m_iFeintSmokeShells = SRP_CmdSettings.GetIntClamped("appui_feinte_obus_fumee", 0, 12);
		m_iArtilleryPlayers = SRP_CmdSettings.GetIntClamped("appui_artillerie_seuil_joueurs", 1, 50);
		m_iArtilleryIntervalH = SRP_CmdSettings.GetIntClamped("appui_artillerie_intervalle_h", 0, 48);
		m_iAdjustMin = SRP_CmdSettings.GetIntClamped("appui_artillerie_reglage_min", 0, 5);
		m_iAdjustMax = SRP_CmdSettings.GetIntClamped("appui_artillerie_reglage_max", 0, 5);
		m_iAdjustOffsetM = SRP_CmdSettings.GetIntClamped("appui_artillerie_reglage_ecart_m", 0, 1000);
		m_iAdjustLeadMinS = SRP_CmdSettings.GetIntClamped("appui_artillerie_avance_min_s", 0, 600);
		m_iAdjustLeadMaxS = SRP_CmdSettings.GetIntClamped("appui_artillerie_avance_max_s", 0, 600);
		m_iFirstShotMinS = SRP_CmdSettings.GetIntClamped("appui_artillerie_premier_coup_min_s", 0, 600);
		m_iFirstShotMaxS = SRP_CmdSettings.GetIntClamped("appui_artillerie_premier_coup_max_s", 0, 600);
		m_iSalvoMin = SRP_CmdSettings.GetIntClamped("appui_artillerie_salve_min", 1, 100);
		m_iSalvoMax = SRP_CmdSettings.GetIntClamped("appui_artillerie_salve_max", 1, 100);
		m_iSalvoRadiusM = SRP_CmdSettings.GetIntClamped("appui_artillerie_rayon_m", 10, 500);
		m_iSalvoRateMinS = SRP_CmdSettings.GetIntClamped("appui_artillerie_cadence_min_s", 1, 30);
		m_iSalvoRateMaxS = SRP_CmdSettings.GetIntClamped("appui_artillerie_cadence_max_s", 1, 30);
		m_iBatteryMinM = SRP_CmdSettings.GetIntClamped("appui_artillerie_batterie_min_m", 500, 10000);
		m_iBatteryMaxM = SRP_CmdSettings.GetIntClamped("appui_artillerie_batterie_max_m", 500, 10000);
		m_iBatteryRangeM = SRP_CmdSettings.GetIntClamped("appui_artillerie_portee_max_m", 500, 20000);
		m_iBatteryGuardM = SRP_CmdSettings.GetIntClamped("appui_artillerie_garde_m", 100, 5000);
		m_iBatteryPresenceM = SRP_CmdSettings.GetIntClamped("appui_artillerie_presence_m", 100, 10000);
		m_iBatteryRetireMin = SRP_CmdSettings.GetIntClamped("appui_artillerie_retrait_min", 0, 120);
		m_iBatterySilenceHours = SRP_CmdSettings.GetIntClamped("appui_artillerie_silence_heures", 0, 240);
		m_sBatteryPrefab = SRP_CmdSettings.GetString("appui_artillerie_batterie");
		m_sBatterySound = SRP_CmdSettings.GetString("appui_artillerie_son");
		m_sBatterySoundEvent = SRP_CmdSettings.GetString("appui_artillerie_son_evenement");
		m_iBombPlayers = SRP_CmdSettings.GetIntClamped("appui_bombe_seuil_joueurs", 1, 100);
		m_iBombIntervalH = SRP_CmdSettings.GetIntClamped("appui_bombe_intervalle_h", 0, 240);
		m_iBombSpreadM = SRP_CmdSettings.GetIntClamped("appui_bombe_dispersion_m", 0, 100);
		m_iBombPlaneLeadS = SRP_CmdSettings.GetIntClamped("appui_bombe_avion_avance_s", 0, 120);
		m_sBombPrefab = SRP_CmdSettings.GetString("appui_bombe_prefab");
		m_sPlanePrefab = SRP_CmdSettings.GetString("appui_avion_prefab");
		m_iFlybysPerDay = SRP_CmdSettings.GetIntClamped("appui_avion_passages_24h", 0, 50);
		m_iHeliRestMin = SRP_CmdSettings.GetIntClamped("appui_helico_repos_min", 0, 600);
		if (m_Armor)
			m_Armor.LoadSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Front prêt : tableaux par région dimensionnés (interdictions relues gardées) ; journal des clés de faction
	//! présentes — SRP_Commander.Start
	void Start(int nowUnix)
	{
		EnsureRegionArrays();
		PruneCaps(nowUnix);

		// AP6 : la clé de faction du mod (« AFRF ») et celle de RHS (« RHS_AFRF ») sont notées au démarrage
		FactionManager factions = GetGame().GetFactionManager();
		string afrf = "absente";
		string rhs = "absente";
		if (factions && factions.GetFactionByKey("AFRF"))
			afrf = "présente";
		if (factions && factions.GetFactionByKey("RHS_AFRF"))
			rhs = "présente";
		string chosen = "(vide)";
		if (m_Armor && !m_Armor.m_sFaction.IsEmpty())
			chosen = m_Armor.m_sFaction;
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Appuis prêts : %1 région(s) ; clé de faction AFRF %2, RHS_AFRF %3 ; appui_blinde_faction = %4", m_aMortarBanUntil.Count(), afrf, rhs, chosen));

		// Prefabs des réglages : un nom illisible est signalé tout de suite (pas au premier tir)
		CheckPrefab("appui_mortier_piece", m_sMortarPiece);
		CheckPrefab("prefab des servants du mortier", m_sMortarCrewGroup);
		CheckPrefab("appui_obus_explosif", m_sShellHe);
		CheckPrefab("appui_obus_fumee", m_sShellSmoke);
		CheckPrefab("appui_artillerie_batterie", m_sBatteryPrefab);
		CheckPrefab("appui_bombe_prefab", m_sBombPrefab);
		CheckPrefab("appui_avion_prefab", m_sPlanePrefab);
	}

	//------------------------------------------------------------------------------------------------
	//! Arrêt : tous les CallLater de tir retirés (FireNext, DropShell, DropBomb…) — SRP_Commander.Stop
	void Stop()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (!queue)
			return;
		queue.Remove(FireNextId);
		queue.Remove(DropShell);
		queue.Remove(DropBomb);
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s : équipes (silence MO9, retrait MO2), batteries (garde à l'approche, destruction, retrait),
	//! m_Armor.Tick, hélico (sortie finie -> Consume), frappes périmées — SRP_Commander.Tick
	void Tick(int nowUnix)
	{
		EnsureRegionArrays();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		TickTeams(nowUnix, players);
		TickBatteries(nowUnix, players);
		if (m_Armor)
			m_Armor.Tick(nowUnix);
		TickHeli(nowUnix);
		TickDoomed(players);
		PruneCaps(nowUnix);
	}

	//================================================================================================
	// Demandes (« » = l'appui part ; sinon la raison du refus)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! MO1 à MO9 : EXPLOSIF (Reserve d'obus ; refus : gel, désorganisation, interdiction 24 h, appui_mortier_tirs_heure,
	//! appui_mortier_seuil_joueurs, stock, carré MO6, même endroit, sécurité, pièce déjà en tir) ou FUMEE (C5 :
	//! gratuite, hors plafonds) ; shells = 0 -> appui_mortier_obus_min..max ; pièce posée par EnsureTeam, premier coup
	//! après appui_mortier_delai_min_s..max_s — SRP_Commander.IssueOrder, SRP_CmdManeuvers
	string RequestMortar(int region, vector aim, int shell, int shells, string reason, bool staff)
	{
		bool smoke = shell == SRP_ECmdShell.FUMEE;
		string what = "mortier";
		if (smoke)
			what = "fumée";
		string why = MortarChecks(region, aim, shell, staff, shells);
		if (!why.IsEmpty())
			return Refuse(region, what, why);

		SRP_CmdResources resources = SRP_CmdResources.Get();
		int count = shells;
		if (smoke)
		{
			if (count <= 0)
				count = m_iSmokeShells;
		}
		else
		{
			if (!resources)
				return Refuse(region, what, "stocks du Commandeur absents");
			int inStock = Math.Floor(resources.GetStock(SRP_ECmdStock.OBUS, region));
			if (count <= 0)
			{
				count = RandRange(m_iMortarShellsMin, m_iMortarShellsMax);
				// MO5 : un tir plus court si le stock ne permet pas le compte tiré, jamais sous le minimum
				if (!staff && count > inStock)
					count = inStock;
				if (!staff && count < m_iMortarShellsMin)
					return Refuse(region, what, string.Format("stock : %1 obus dans la région, il en faut %2", inStock, m_iMortarShellsMin));
			}
			else if (!staff && inStock < count)
			{
				return Refuse(region, what, string.Format("stock : %1 obus dans la région, il en faut %2", inStock, count));
			}
		}
		if (count <= 0)
			return Refuse(region, what, "aucun obus à tirer");

		SRP_CmdMortarTeam team = EnsureTeamEx(region, aim, staff, why);
		if (!team)
			return Refuse(region, what, why);

		int ticket = 0;
		if (!smoke)
		{
			ticket = resources.Reserve(SRP_ECmdStock.OBUS, region, count, "mortier sur " + ZoneText(aim), staff);
			if (ticket == 0)
				return "stock : obus refusés par les stocks du Commandeur";
		}

		int now = System.GetUnixTime();
		SRP_CmdFireMission mission = NewMission(SRP_ECmdOrder.MORTIER, shell, region, aim, reason, staff);
		mission.m_iTicket = ticket;
		if (smoke)
		{
			// Fumée sur un point : rideau en travers de l'axe pièce -> point
			mission.m_iKind = SRP_ECmdOrder.FUMEE;
			float width = m_iSmokeWidthM;
			BuildCurtain(aim, FlatDirection(team.m_vPos, aim), count, width, mission.m_aFixed);
			mission.m_iShellsLeft = mission.m_aFixed.Count();
		}
		else
		{
			mission.m_iShellsLeft = count;
		}

		int delay = RandRange(m_iMortarDelayMinS, m_iMortarDelayMaxS);
		StartMission(mission, team, delay);

		// Plafonds (EQ3, C5) : hors Staff ; la fumée n'est jamais une frappe
		if (!staff)
		{
			if (!smoke || m_bSmokeCounts)
				m_aMortarUnix.Insert(now);
			if (!smoke)
				NoteStrike(aim, now);
			SetDirty(false);
		}

		// Ligne d'exécution seulement : la décision (coût, reste) est écrite par l'appelant (§1.8)
		int distance = Math.Round(vector.DistanceXZ(team.m_vPos, aim));
		int players = PlayersNear(aim);
		string shellWord = "obus explosif(s)";
		if (smoke)
			shellWord = "fumigène(s)";
		string detail = string.Format("%1 ; %2 joueur(s) à %3 m, pièce à %4 m, premier obus dans %5 s", reason, players, m_iPlayersM, distance, delay);
		if (staff)
			detail = detail + ", ordre du Staff";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Mortier : %1 %2 sur %3", count, shellWord, ZoneText(aim)) + " — " + detail);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! MO7 : rideau de appui_fumee_obus fumigènes (appui_fumee_largeur_m, perpendiculaire à from -> threat, à
	//! appui_fumee_devant_joueurs_m devant les joueurs) par la pièce de la région (gratuit, C5) — SRP_CmdManeuvers
	//! (assaut, décrochage), SRP_Commander (FUMEE)
	string RequestSmoke(int region, vector from, vector threat, string reason, bool staff)
	{
		string what = "rideau de fumée";
		// Centre : devant les joueurs (threat), du côté d'où vient l'ennemi (from)
		vector toward = FlatDirection(threat, from);
		float ahead = m_iSmokeAheadM;
		vector center = SRP_Placement.OnGround(threat + toward * ahead);
		string why = MortarChecks(region, center, SRP_ECmdShell.FUMEE, staff, 0);
		if (!why.IsEmpty())
			return Refuse(region, what, why);

		SRP_CmdMortarTeam team = EnsureTeamEx(region, center, staff, why);
		if (!team)
			return Refuse(region, what, why);

		int now = System.GetUnixTime();
		SRP_CmdFireMission mission = NewMission(SRP_ECmdOrder.FUMEE, SRP_ECmdShell.FUMEE, region, center, reason, staff);
		float width = m_iSmokeWidthM;
		BuildCurtain(center, toward, m_iSmokeShells, width, mission.m_aFixed);
		mission.m_iShellsLeft = mission.m_aFixed.Count();
		int delay = RandRange(m_iMortarDelayMinS, m_iMortarDelayMaxS);
		StartMission(mission, team, delay);

		if (!staff && m_bSmokeCounts)
		{
			m_aMortarUnix.Insert(now);
			SetDirty(false);
		}

		string detail = string.Format("%1 ; %2 fumigène(s) sur %3 m, à %4 m devant les joueurs, premier dans %5 s", reason, mission.m_iShellsLeft, m_iSmokeWidthM, m_iSmokeAheadM, delay);
		if (staff)
			detail = detail + ", ordre du Staff";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, "Rideau de fumée sur " + ZoneText(center) + " — " + detail);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! MO8 : rideau puis appui_feinte_obus_explosifs obus explosifs sur l'entrée du faux axe (aim) ; l'assaut vient
	//! d'ailleurs — SRP_CmdManeuvers.Support, SRP_Commander (FEINTE)
	string RequestFeint(int region, vector aim, string reason, bool staff)
	{
		string what = "feinte";
		string why = MortarChecks(region, aim, SRP_ECmdShell.FUMEE, staff, 0);
		if (!why.IsEmpty())
			return Refuse(region, what, why);

		// Les obus explosifs de la feinte suivent les règles d'un tir explosif ordinaire (stock : ses propres obus
		// seulement) ; refusés, la feinte part en fumée seule
		int heShells = m_iFeintHeShells;
		string heWhy = "";
		if (heShells > 0 && !staff)
			heWhy = MortarChecks(region, aim, SRP_ECmdShell.EXPLOSIF, false, heShells);
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (heShells > 0 && heWhy.IsEmpty() && !resources)
			heWhy = "stocks du Commandeur absents";
		if (heShells > 0 && heWhy.IsEmpty() && !staff && Math.Floor(resources.GetStock(SRP_ECmdStock.OBUS, region)) < heShells)
			heWhy = "stock d'obus insuffisant";
		if (!heWhy.IsEmpty())
			heShells = 0;
		if (heShells <= 0 && m_iFeintSmokeShells <= 0)
		{
			if (heWhy.IsEmpty())
				heWhy = "réglages : aucun obus de feinte";
			return Refuse(region, what, "aucun obus possible : " + heWhy);
		}

		SRP_CmdMortarTeam team = EnsureTeamEx(region, aim, staff, why);
		if (!team)
			return Refuse(region, what, why);

		int ticket = 0;
		if (heShells > 0)
		{
			ticket = resources.Reserve(SRP_ECmdStock.OBUS, region, heShells, "feinte sur " + ZoneText(aim), staff);
			if (ticket == 0)
			{
				heShells = 0;
				heWhy = "obus refusés par les stocks du Commandeur";
			}
		}
		if (heShells <= 0 && m_iFeintSmokeShells <= 0)
			return Refuse(region, what, "aucun obus possible : " + heWhy);

		// Rideau entre les joueurs (cible de la contre-attaque servie, sinon au-delà du faux axe vu de la pièce) et le
		// point. Seule la feinte des manœuvres (staff faux) sert la contre-attaque en cours : une feinte forcée par le
		// Staff n'y est jamais liée (ni sens du rideau, ni annulation à la fin de l'attaque)
		vector toward = FlatDirection(team.m_vPos, aim);
		SRP_CounterAttack served;
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (host && !staff)
			served = host.GetCounterAttack();
		if (served && served.m_vTarget != vector.Zero)
			toward = FlatDirection(aim, served.m_vTarget);
		float ahead = m_iSmokeAheadM;
		vector center = SRP_Placement.OnGround(aim + toward * ahead);

		int now = System.GetUnixTime();
		int shell = SRP_ECmdShell.FUMEE;
		if (heShells > 0)
			shell = SRP_ECmdShell.EXPLOSIF;
		SRP_CmdFireMission mission = NewMission(SRP_ECmdOrder.FEINTE, shell, region, aim, reason, staff);
		mission.m_iTicket = ticket;
		// Le faux axe est souvent dans une zone voisine : la feinte est rattachée à la contre-attaque servie, pour
		// qu'EndAssault l'annule si l'attaque finit avant le premier coup (feinte du Staff : zone du point visé)
		if (served)
			mission.m_iZone = served.m_iZone;
		float width = m_iSmokeWidthM;
		if (m_iFeintSmokeShells > 0)
			BuildCurtain(center, toward, m_iFeintSmokeShells, width, mission.m_aFixed);
		mission.m_iShellsLeft = mission.m_aFixed.Count() + heShells;
		int delay = RandRange(m_iMortarDelayMinS, m_iMortarDelayMaxS);
		StartMission(mission, team, delay);

		if (!staff)
		{
			if (heShells > 0 || m_bSmokeCounts)
				m_aMortarUnix.Insert(now);
			if (heShells > 0)
				NoteStrike(aim, now);
			SetDirty(false);
		}

		string detail = string.Format("%1 ; %2 fumigène(s) puis %3 obus explosif(s), premier dans %4 s", reason, mission.m_aFixed.Count(), heShells, delay);
		if (!heWhy.IsEmpty())
			detail = detail + " (explosifs écartés : " + heWhy + ")";
		if (staff)
			detail = detail + ", ordre du Staff";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, "Feinte sur " + ZoneText(aim) + " — " + detail);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôle d'un tir de mortier sans rien lancer ("" = possible) — SRP_CmdManeuvers (feinte, harcèlement), Staff
	string MortarRefusal(int region, vector aim, int shell)
	{
		// Nombre d'obus inconnu ici : au moins un en stock ; RequestMortar et RequestFeint revoient le compte exact
		return MortarChecks(region, aim, shell, false, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! AP1, AP2, EQ1 : 2S1 posé à appui_artillerie_batterie_min_m..max_m, coups de réglage puis salve de
	//! appui_artillerie_salve_min..max obus dans appui_artillerie_rayon_m ; attachedZone = zone de la contre-attaque
	//! servie (-1 sinon, EQ3) ; refus : gel, désorganisation, interdiction, 2 h, 8 joueurs, stock, grosse opération,
	//! carré, même endroit, sécurité appui_securite_salve_m — SRP_Commander (AP2), SRP_CmdManeuvers (CA4)
	string RequestArtillery(int region, vector aim, int attachedZone, string reason, bool staff)
	{
		string what = "artillerie lourde";
		string why;
		if (!CanActFor(region, staff, why))
			return Refuse(region, what, why);
		EnsureRegionArrays();
		if (region < 0 || region >= m_aArtilleryBanUntil.Count())
			return Refuse(region, what, "point hors des régions ennemies");
		int now = System.GetUnixTime();
		if (m_aArtilleryBanUntil[region] > now)
			return Refuse(region, what, string.Format("batterie de la %1 détruite : plus d'artillerie lourde encore %2", RegionLabel(region), Dur(m_aArtilleryBanUntil[region] - now)));
		if (HasArtilleryMission(-1))
			return Refuse(region, what, "un tir d'artillerie lourde est déjà en cours");
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (!resources)
			return Refuse(region, what, "stocks du Commandeur absents");

		int players = PlayersNear(aim);
		if (!staff)
		{
			int wait = ArtilleryWait(now);
			if (wait > 0)
				return Refuse(region, what, string.Format("plafond : un tir lourd toutes les %1 h, prochain possible dans %2", m_iArtilleryIntervalH, Dur(wait)));
			if (players < m_iArtilleryPlayers)
				return Refuse(region, what, string.Format("seuil : %1 joueur(s) à %2 m, il en faut %3", players, m_iPlayersM, m_iArtilleryPlayers));
			string stockWhy;
			if (!resources.CanReserve(SRP_ECmdStock.ARTILLERIE, -1, 1, stockWhy))
				return Refuse(region, what, "stock : " + stockWhy);
			string bigWhy = BigOperationReason(attachedZone, true);
			if (!bigWhy.IsEmpty())
				return Refuse(region, what, "une grosse opération est déjà en cours : " + bigWhy);
			if (!IsValidTargetCell(aim))
				return Refuse(region, what, "carré visé ni ennemi ni au front (MO6)");
			if (StruckRecently(aim, now))
				return Refuse(region, what, string.Format("une frappe à moins de %1 m il y a moins de %2 min", m_iSameSpotM, m_iSameSpotMin));
		}
		string safeWhy;
		if (!IsSafeTarget(aim, m_iSafeSalvoM, safeWhy))
			return Refuse(region, what, "sécurité : " + safeWhy);

		SRP_CmdBattery battery = EnsureBattery(region, aim, why);
		if (!battery)
			return Refuse(region, what, why);

		int ticket = resources.Reserve(SRP_ECmdStock.ARTILLERIE, -1, 1, "artillerie lourde sur " + ZoneText(aim), staff);
		if (ticket == 0)
			return "stock : tir d'artillerie lourde refusé par les stocks du Commandeur";

		SRP_CmdFireMission mission = NewMission(SRP_ECmdOrder.ARTILLERIE, SRP_ECmdShell.EXPLOSIF, region, aim, reason, staff);
		mission.m_iTicket = ticket;
		if (attachedZone >= 0)
			mission.m_iZone = attachedZone;
		// EQ1 : coups de réglage à environ appui_artillerie_reglage_ecart_m (plus ou moins 20 m) de la cible
		int adjust = RandRange(m_iAdjustMin, m_iAdjustMax);
		for (int k = 0; k < adjust; k++)
		{
			float offset = m_iAdjustOffsetM + Math.RandomFloat(-20, 20);
			if (offset < 0)
				offset = 0;
			mission.m_aFixed.Insert(Scatter(aim, offset));
		}
		mission.m_iShellsLeft = RandRange(m_iSalvoMin, m_iSalvoMax);
		int delay = RandRange(m_iFirstShotMinS, m_iFirstShotMaxS);
		m_aMissions.Insert(mission);
		ScheduleNext(mission, delay);
		battery.m_iLastNeededUnix = now;

		int distance = Math.Round(vector.DistanceXZ(battery.m_vPos, aim));
		string detail = string.Format("%1 ; %2 joueur(s) à %3 m, batterie à %4 m", reason, players, m_iPlayersM, distance);
		if (staff)
			detail = detail + ", ordre du Staff";
		string plan = string.Format("Artillerie lourde : %1 coup(s) de réglage sur %2 dans %3 s, puis %4 obus", adjust, ZoneText(aim), delay, mission.m_iShellsLeft);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, plan + " — " + detail);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! AP3, EQ1 : Su-57 puis bombe (appui_bombe_avion_avance_s), à appui_bombe_dispersion_m près ; 24 h glissantes,
	//! appui_bombe_seuil_joueurs à 3 km, stock, pas de grosse opération, sécurité appui_securite_bombe_m (revue au
	//! largage) — SRP_Commander (AP3)
	string RequestBomb(vector aim, string reason, bool staff)
	{
		string what = "bombe";
		int region = RegionAt(aim);
		string why;
		if (!CanActFor(region, staff, why))
			return Refuse(region, what, why);
		if (m_bBombPending)
			return Refuse(region, what, "une bombe est déjà en route");
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (!resources)
			return Refuse(region, what, "stocks du Commandeur absents");

		int now = System.GetUnixTime();
		int players = PlayersNear(aim);
		if (!staff)
		{
			int wait = BombWait(now);
			if (wait > 0)
				return Refuse(region, what, string.Format("plafond : une bombe au plus toutes les %1 h, prochaine possible dans %2", m_iBombIntervalH, Dur(wait)));
			if (players < m_iBombPlayers)
				return Refuse(region, what, string.Format("seuil : %1 joueur(s) à %2 m, il en faut %3", players, m_iPlayersM, m_iBombPlayers));
			string stockWhy;
			if (!resources.CanReserve(SRP_ECmdStock.BOMBE, -1, 1, stockWhy))
				return Refuse(region, what, "stock : " + stockWhy);
			string bigWhy = BigOperationReason(-1, true);
			if (!bigWhy.IsEmpty())
				return Refuse(region, what, "une grosse opération est déjà en cours : " + bigWhy);
			if (!IsValidTargetCell(aim))
				return Refuse(region, what, "carré visé ni ennemi ni au front (MO6)");
			if (StruckRecently(aim, now))
				return Refuse(region, what, string.Format("une frappe à moins de %1 m il y a moins de %2 min", m_iSameSpotM, m_iSameSpotMin));
		}
		string safeWhy;
		if (!IsSafeTarget(aim, m_iSafeBombM, safeWhy))
			return Refuse(region, what, "sécurité : " + safeWhy);

		int ticket = resources.Reserve(SRP_ECmdStock.BOMBE, -1, 1, "bombe sur " + ZoneText(aim), staff);
		if (ticket == 0)
			return "stock : bombe refusée par les stocks du Commandeur";

		// EQ1 : l'avion est entendu d'abord (charge vide), la bombe part appui_bombe_avion_avance_s plus tard
		IEntity plane = SpawnCallIn(m_sPlanePrefab, aim);
		m_bBombPending = true;
		m_iBombTicket = ticket;
		m_iBombRegion = region;
		m_bBombStaff = staff;
		GetGame().GetCallqueue().CallLater(DropBomb, m_iBombPlaneLeadS * 1000, false, aim);

		string detail = string.Format("%1 ; %2 joueur(s) à %3 m, largage dans %4 s", reason, players, m_iPlayersM, m_iBombPlaneLeadS);
		if (!plane)
			detail = detail + " (avion non créé : prefab appui_avion_prefab)";
		if (staff)
			detail = detail + ", ordre du Staff";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, "Bombe FAB-500 : Su-57 puis bombe sur " + ZoneText(aim) + " — " + detail);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Passage d'avion sans dégât (appui_avion_passages_24h sur 24 h glissantes) — SRP_CmdManeuvers (CA4), Staff
	string RequestFlyby(vector position, string reason, bool staff)
	{
		string what = "passage d'avion";
		int region = RegionAt(position);
		string why;
		if (!CanActFor(region, staff, why))
			return Refuse(region, what, why);
		int now = System.GetUnixTime();
		int passes = CountSince(m_aFlybyUnix, now, DAY_S);
		if (!staff && passes >= m_iFlybysPerDay)
			return Refuse(region, what, string.Format("plafond : %1 passage(s) sur 24 h, %2 au plus", passes, m_iFlybysPerDay));

		IEntity plane = SpawnCallIn(m_sPlanePrefab, position);
		if (!plane)
			return Refuse(region, what, "avion non créé (prefab appui_avion_prefab)");
		if (!staff)
		{
			m_aFlybyUnix.Insert(now);
			passes++;
			SetDirty(false);
		}
		string detail = string.Format("%1 ; %2 passage(s) sur %3 en 24 h", reason, passes, m_iFlybysPerDay);
		if (staff)
			detail = detail + ", ordre du Staff";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, "Passage de Su-57 au-dessus de " + ZoneText(position) + " — " + detail);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! AP4, AP5 : relais de m_Armor.Launch (armorReason = SRP_ECmdArmorReason ; variant 0 BTR-70, 1 BRDM-2, 2 Typhoon,
	//! -1 tirage selon les poids) — SRP_Commander (AP5), SRP_CmdManeuvers (CA4)
	string RequestArmor(int region, vector target, int armorReason, int zone, int variant, bool staff)
	{
		string what = "blindé";
		if (!m_Armor)
			return Refuse(region, what, "blindés absents");
		string why;
		if (!CanActFor(region, staff, why))
			return Refuse(region, what, why);
		if (!staff)
		{
			int active = m_Armor.CountActive();
			if (active >= m_Armor.m_iArmorAtOnce)
				return Refuse(region, what, string.Format("plafond : %1 blindé(s) déjà engagé(s), %2 à la fois", active, m_Armor.m_iArmorAtOnce));
			int players = PlayersNear(target);
			if (players < m_Armor.m_iArmorPlayers)
				return Refuse(region, what, string.Format("seuil : %1 joueur(s) à %2 m, il en faut %3", players, m_iPlayersM, m_Armor.m_iArmorPlayers));
			SRP_CmdResources resources = SRP_CmdResources.Get();
			if (!resources)
				return Refuse(region, what, "stocks du Commandeur absents");
			string stockWhy;
			if (!resources.CanReserve(SRP_ECmdStock.BLINDE, -1, 1, stockWhy))
				return Refuse(region, what, "stock : " + stockWhy);
			int attached = -1;
			if (armorReason == SRP_ECmdArmorReason.CONTRE_ATTAQUE)
				attached = zone;
			// Les blindés déjà engagés ne sont pas une « autre » grosse opération : leur plafond est
			// appui_blinde_simultanes (test ci-dessus) ; artillerie, bombe ou autre contre-attaque refusent toujours
			string bigWhy = BigOperationReason(attached, false);
			if (!bigWhy.IsEmpty())
				return Refuse(region, what, "une grosse opération est déjà en cours : " + bigWhy);
		}
		// Sécurité d'un blindé : jamais vers la base ni la mer (il combat : pas de test des soldats alentour)
		string safeWhy;
		if (!IsSafeTarget(target, 0, safeWhy))
			return Refuse(region, what, "sécurité : " + safeWhy);
		return m_Armor.Launch(region, target, armorReason, zone, variant, staff);
	}

	//------------------------------------------------------------------------------------------------
	//! AP7 : carré rouge, SRP_HeliSearch.Count() == 0, repos appui_helico_repos_min, stock HELICO (Reserve), puis
	//! SRP_HeliSearch.Launch(position, reason) ; aucun hélico armé (AP8) ; staff (ForceOrder(HELICO), seule porte du
	//! Staff, trou 15) : outre stock, repos et « personne sur place », jamais outre la sécurité — SRP_Commander (AP7)
	string RequestHeli(vector position, string reason, bool staff)
	{
		string what = "hélico";
		int region = RegionAt(position);
		string why;
		if (!CanActFor(region, staff, why))
			return Refuse(region, what, why);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!staff && front && front.IsReady() && !front.IsRedAt(position))
			return Refuse(region, what, "le point n'est pas sur un carré ennemi");
		if (SRP_HeliSearch.Count() > 0)
			return Refuse(region, what, "un hélico est déjà en vol");
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (!resources)
			return Refuse(region, what, "stocks du Commandeur absents");
		int now = System.GetUnixTime();
		if (!staff)
		{
			if (m_iHeliUnix > 0 && now - m_iHeliUnix < m_iHeliRestMin * 60)
				return Refuse(region, what, string.Format("repos : une sortie toutes les %1 min, prochaine dans %2", m_iHeliRestMin, Dur(m_iHeliUnix + m_iHeliRestMin * 60 - now)));
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			int noTroopM = 800;
			if (m_Commander)
				noTroopM = m_Commander.m_iHeliNoTroopM;
			if (enemies && enemies.CountGroupsNear(position, noTroopM) > 0)
				return Refuse(region, what, string.Format("des soldats ennemis sont déjà sur place (moins de %1 m)", noTroopM));
			string stockWhy;
			if (!resources.CanReserve(SRP_ECmdStock.HELICO, -1, 1, stockWhy))
				return Refuse(region, what, "stock : " + stockWhy);
		}
		string safeWhy;
		if (!IsSafeTarget(position, 0, safeWhy))
			return Refuse(region, what, "sécurité : " + safeWhy);

		// Sortie précédente finie depuis le dernier Tick (Staff, sans repos) : son ticket est consommé avant d'en
		// ouvrir un autre (SRP_HeliSearch.Count() vaut 0 ici)
		TickHeli(now);
		int ticket = resources.Reserve(SRP_ECmdStock.HELICO, -1, 1, "hélico vers " + ZoneText(position), staff);
		if (ticket == 0)
			return "stock : sortie d'hélico refusée par les stocks du Commandeur";
		// Le compte rendu de Launch va au journal HELICO ; ici, seul compte le vol réellement parti
		SRP_HeliSearch.Launch(position, "Commandeur");
		if (SRP_HeliSearch.Count() == 0)
		{
			resources.Release(ticket, "hélico non parti");
			return Refuse(region, what, "l'hélico n'a pas pu partir (prefab, pilotes ou place)");
		}
		m_iHeliTicket = ticket;
		if (!staff)
		{
			m_iHeliUnix = now;
			SetDirty(false);
		}
		string detail = reason;
		if (staff)
			detail = detail + ", ordre du Staff";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, "Hélico : sortie vers " + ZoneText(position) + " — " + detail);
		return "";
	}

	//================================================================================================
	// États (lecture)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! EQ3 : une grosse opération est en cours (blindé actif, salve ou bombe en cours, contre-attaque sur une autre
	//! zone que attachedZone) — cerveau, SRP_CmdManeuvers (CA1), m_Armor
	bool IsBigOperationRunning(int attachedZone)
	{
		return !BigOperationReason(attachedZone, true).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Contre-attaque finie sur la zone (CA5, zone perdue, défendue) : rideau en attente annulé, blindé de cette
	//! attaque en repli — SRP_CmdManeuvers.EndAttack
	void EndAssault(int zone)
	{
		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
		{
			SRP_CmdFireMission mission = m_aMissions[i];
			if (!mission || mission.m_iZone != zone || mission.m_iFired > 0)
				continue;
			// Un ordre du Staff n'appartient pas à l'attaque, même tiré dans sa zone : il va au bout
			if (mission.m_bStaff)
				continue;
			if (mission.m_iKind != SRP_ECmdOrder.FUMEE && mission.m_iKind != SRP_ECmdOrder.FEINTE)
				continue;
			EndMission(mission, "contre-attaque finie : rideau annulé", false);
		}
		if (m_Armor)
			m_Armor.EndAssault(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! La région peut tirer au mortier (pas muette, pas en tir, appuis actifs) — cerveau, écrans
	bool IsMortarReady(int region)
	{
		if (!m_bOn)
			return false;
		EnsureRegionArrays();
		if (region < 0 || region >= m_aMortarBanUntil.Count())
			return false;
		if (m_aMortarBanUntil[region] > System.GetUnixTime())
			return false;
		SRP_CmdMortarTeam team = FindTeam(region);
		if (team && (team.m_bSilenced || team.m_bSabotaged || team.m_bFiring))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! La région peut recevoir un tir lourd (batterie non détruite, plafond EQ3 passé) — cerveau, écrans
	bool IsArtilleryReady(int region)
	{
		if (!m_bOn)
			return false;
		EnsureRegionArrays();
		if (region < 0 || region >= m_aArtilleryBanUntil.Count())
			return false;
		int now = System.GetUnixTime();
		if (m_aArtilleryBanUntil[region] > now)
			return false;
		if (ArtilleryWait(now) > 0)
			return false;
		return !HasArtilleryMission(-1);
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdPieceState du mortier de la région — SRP_CmdScreens
	int GetMortarState(int region)
	{
		if (GetMortarSilencedUntil(region) > 0)
			return SRP_ECmdPieceState.MUETTE;
		SRP_CmdMortarTeam team = FindTeam(region);
		if (!team)
			return SRP_ECmdPieceState.PRETE;
		if (team.m_bSilenced)
			return SRP_ECmdPieceState.MUETTE;
		if (team.m_bFiring)
			return SRP_ECmdPieceState.EN_TIR;
		if (team.m_Piece)
			return SRP_ECmdPieceState.EN_BATTERIE;
		return SRP_ECmdPieceState.PRETE;
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de l'interdiction de 24 h du mortier de la région (heure Unix, 0 = aucune) — SRP_CmdScreens
	int GetMortarSilencedUntil(int region)
	{
		EnsureRegionArrays();
		if (region < 0 || region >= m_aMortarBanUntil.Count())
			return 0;
		int until = m_aMortarBanUntil[region];
		if (until <= System.GetUnixTime())
			return 0;
		return until;
	}

	//------------------------------------------------------------------------------------------------
	//! Position de la pièce posée (vector.Zero sinon) — SRP_CmdScreens (Staff)
	vector GetMortarPos(int region)
	{
		SRP_CmdMortarTeam team = FindTeam(region);
		if (!team || !team.m_Piece || team.m_Piece.IsDeleted())
			return vector.Zero;
		return team.m_vPos;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdPieceState de la batterie de la région — SRP_CmdScreens
	int GetBatteryState(int region)
	{
		if (GetBatteryDestroyedUntil(region) > 0)
			return SRP_ECmdPieceState.MUETTE;
		if (HasArtilleryMission(region))
			return SRP_ECmdPieceState.EN_TIR;
		SRP_CmdBattery battery = FindBattery(region);
		if (!battery)
			return SRP_ECmdPieceState.PRETE;
		if (battery.m_bDestroyed)
			return SRP_ECmdPieceState.MUETTE;
		if (battery.m_Vehicle)
			return SRP_ECmdPieceState.EN_BATTERIE;
		return SRP_ECmdPieceState.PRETE;
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de l'interdiction de 24 h de l'artillerie de la région (heure Unix, 0 = aucune) — SRP_CmdScreens
	int GetBatteryDestroyedUntil(int region)
	{
		EnsureRegionArrays();
		if (region < 0 || region >= m_aArtilleryBanUntil.Count())
			return 0;
		int until = m_aArtilleryBanUntil[region];
		if (until <= System.GetUnixTime())
			return 0;
		return until;
	}

	//------------------------------------------------------------------------------------------------
	//! Position de la batterie posée (vector.Zero sinon) — SRP_CmdScreens (Staff)
	vector GetBatteryPos(int region)
	{
		SRP_CmdBattery battery = FindBattery(region);
		if (!battery || !battery.m_Vehicle || battery.m_Vehicle.IsDeleted())
			return vector.Zero;
		return battery.m_vPos;
	}

	//------------------------------------------------------------------------------------------------
	//! « mortier 1 tir sur 2 cette heure · 1 blindé à la fois · artillerie lourde dans 1 h 20 · bombe dans 14 h ·
	//! grosse opération : aucune » (EQ3) — SRP_CmdScreens
	string GetCapsText()
	{
		int now = System.GetUnixTime();
		int shots = CountSince(m_aMortarUnix, now, HOUR_S);
		int armorActive = 0;
		int armorAtOnce = 1;
		if (m_Armor)
		{
			armorActive = m_Armor.CountActive();
			armorAtOnce = m_Armor.m_iArmorAtOnce;
		}
		string artillery = "artillerie lourde prête";
		int artilleryWait = ArtilleryWait(now);
		if (artilleryWait > 0)
			artillery = "artillerie lourde dans " + Dur(artilleryWait);
		string bomb = "bombe prête";
		int bombWait = BombWait(now);
		if (bombWait > 0)
			bomb = "bombe dans " + Dur(bombWait);
		int passes = CountSince(m_aFlybyUnix, now, DAY_S);
		string big = BigOperationReason(-1, true);
		if (big.IsEmpty())
			big = "aucune";
		string first = string.Format("mortier %1 tir(s) sur %2 cette heure · blindés : %3 engagé(s), %4 à la fois · %5 · %6", shots, m_iMortarPerHour, armorActive, armorAtOnce, artillery, bomb);
		return first + string.Format(" · passages d'avion %1 sur %2 en 24 h · grosse opération : %3", passes, m_iFlybysPerDay, big);
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes Staff : par région mortier et batterie, blindés (m_Armor.Report), plafonds — SRP_CmdScreens
	void GetReport(notnull array<string> lines)
	{
		lines.Clear();
		EnsureRegionArrays();
		int now = System.GetUnixTime();
		int count = m_aMortarBanUntil.Count();
		for (int r = 0; r < count; r++)
			lines.Insert(string.Format("%1, %2 : mortier %3 · artillerie lourde %4", RegionCode(r), RegionLabel(r), MortarStateText(r, now), BatteryStateText(r, now)));
		if (m_Armor)
		{
			array<string> armorLines = {};
			m_Armor.Report(armorLines);
			foreach (string armorLine : armorLines)
				lines.Insert(armorLine);
		}
		lines.Insert("Plafonds : " + GetCapsText());

		int mortarRuns = 0;
		int artilleryRuns = 0;
		foreach (SRP_CmdFireMission mission : m_aMissions)
		{
			if (!mission)
				continue;
			if (mission.m_iKind == SRP_ECmdOrder.ARTILLERIE)
				artilleryRuns++;
			else
				mortarRuns++;
		}
		string bombText = "non";
		if (m_bBombPending)
			bombText = "oui";
		string heliText = "non";
		if (m_iHeliTicket != 0)
			heliText = "oui";
		lines.Insert(string.Format("En cours : %1 tir(s) de mortier, %2 tir(s) d'artillerie lourde · bombe en route : %3 · hélico en vol : %4", mortarRuns, artilleryRuns, bombText, heliText));
	}

	//------------------------------------------------------------------------------------------------
	//! Pièces, batteries et blindés autour d'un point — SRP_CmdScreens.StaffNearReport
	string GetNearReport(vector from, float radius)
	{
		array<string> parts = {};
		foreach (SRP_CmdMortarTeam team : m_aTeams)
		{
			if (!team)
				continue;
			float teamDistance = vector.Distance(from, team.m_vPos);
			if (teamDistance > radius)
				continue;
			string teamState = "prête";
			if (team.m_bSilenced)
				teamState = "muette";
			else if (team.m_bFiring)
				teamState = "en tir";
			int teamMeters = Math.Round(teamDistance);
			parts.Insert(string.Format("Mortier %1 : pièce à %2 m, carré %3, %4 servant(s) à leur poste, %5", RegionCode(team.m_iRegion), teamMeters, CellOf(team.m_vPos), ServantsOnDuty(team), teamState));
		}
		foreach (SRP_CmdBattery battery : m_aBatteries)
		{
			if (!battery)
				continue;
			float batteryDistance = vector.Distance(from, battery.m_vPos);
			if (batteryDistance > radius)
				continue;
			string batteryState = "en place";
			if (battery.m_bDestroyed)
				batteryState = "hors d'usage";
			else if (HasArtilleryMission(battery.m_iRegion))
				batteryState = "en tir";
			string guardText = "sans garde";
			if (battery.m_Guard)
				guardText = string.Format("garde de %1 soldat(s)", SRP_EnemyComponent.AliveAgents(battery.m_Guard));
			int batteryMeters = Math.Round(batteryDistance);
			parts.Insert(string.Format("2S1 %1 : à %2 m, carré %3, %4, %5", RegionCode(battery.m_iRegion), batteryMeters, CellOf(battery.m_vPos), guardText, batteryState));
		}
		if (m_Armor)
		{
			string armorText = m_Armor.GetNearReport(from, radius);
			if (!armorText.IsEmpty())
				parts.Insert(armorText);
		}
		string text = "";
		foreach (string part : parts)
		{
			if (!text.IsEmpty())
				text += "\n";
			text += part;
		}
		return text;
	}

	//================================================================================================
	// Sabotage, Staff, remises, sauvegarde
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! « Saboter » sur une pièce de mortier : m_bSabotaged (silence MO9 au passage suivant), message au joueur ; vrai
	//! = consommé — SRP_Commander.OnSabotageUsed
	bool OnSabotage(IEntity target, int playerId, out string reply)
	{
		reply = "";
		if (!target)
			return false;
		IEntity root = SCR_EntityHelper.GetMainParent(target, true);
		foreach (SRP_CmdMortarTeam team : m_aTeams)
		{
			if (!team || !team.m_Piece)
				continue;
			if (team.m_Piece != target && team.m_Piece != root)
				continue;
			if (team.m_bSilenced || team.m_bSabotaged)
			{
				reply = "Cette pièce de mortier est déjà hors d'usage.";
				return true;
			}
			team.m_bSabotaged = true;
			reply = "Pièce de mortier sabotée : elle ne tirera plus.";
			string author = GetGame().GetPlayerManager().GetPlayerName(playerId);
			SRP_CmdLog.Note(team.m_iRegion, SRP_ECmdLogKind.APPUI, string.Format("Pièce de mortier de la %1 sabotée par %2 (carré %3)", RegionLabel(team.m_iRegion), author, CellOf(team.m_vPos)));
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : interdictions de 24 h levées (région -1 = toutes) — SRP_CmdScreens (actions interdictions:R3,
	//! interdictions:tout)
	string ClearBans(int region, string author)
	{
		EnsureRegionArrays();
		int count = m_aMortarBanUntil.Count();
		if (region >= count)
			return "Région inconnue : aucune interdiction levée";
		int now = System.GetUnixTime();
		int lifted = 0;
		for (int r = 0; r < count; r++)
		{
			if (region >= 0 && r != region)
				continue;
			if (m_aMortarBanUntil[r] > now)
				lifted++;
			if (m_aArtilleryBanUntil[r] > now)
				lifted++;
			m_aMortarBanUntil.Set(r, 0);
			m_aArtilleryBanUntil.Set(r, 0);
		}
		// Pièces muettes et batteries détruites de ces régions : effacées hors de vue, une neuve sera posée au besoin
		for (int t = m_aTeams.Count() - 1; t >= 0; t--)
		{
			SRP_CmdMortarTeam team = m_aTeams[t];
			if (team && team.m_bSilenced && (region < 0 || team.m_iRegion == region))
				DiscardTeam(team, false, "interdiction levée par " + author);
		}
		for (int b = m_aBatteries.Count() - 1; b >= 0; b--)
		{
			SRP_CmdBattery battery = m_aBatteries[b];
			if (battery && battery.m_bDestroyed && (region < 0 || battery.m_iRegion == region))
				DiscardBattery(battery, false, "interdiction levée par " + author);
		}
		SetDirty(true);
		string where = "toutes les régions";
		if (region >= 0)
			where = RegionLabel(region);
		// La ligne de journal du Staff est écrite par SRP_CmdScreens.RunStaff (StaffAction) : rien ici
		return string.Format("Interdictions de mortier et d'artillerie lourde levées (%1) : %2 en cours", where, lifted);
	}

	//------------------------------------------------------------------------------------------------
	//! ia-reset et wipe : missions arrêtées (tickets rendus : Release), pièces, batteries et blindés retirés
	//! (SRP_EnemyComponent.Delete, décors supprimés) — SRP_Commander.CancelOperations
	string CancelAll(string author)
	{
		Stop();
		int missions = m_aMissions.Count();
		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
			EndMission(m_aMissions[i], "tirs arrêtés par " + author, true);
		m_aMissions.Clear();
		CancelBomb("appuis arrêtés par " + author);

		int teams = m_aTeams.Count();
		for (int t = m_aTeams.Count() - 1; t >= 0; t--)
			DiscardTeam(m_aTeams[t], true, "appuis arrêtés par " + author);
		m_aTeams.Clear();
		int batteries = m_aBatteries.Count();
		for (int b = m_aBatteries.Count() - 1; b >= 0; b--)
			DiscardBattery(m_aBatteries[b], true, "appuis arrêtés par " + author);
		m_aBatteries.Clear();
		foreach (IEntity doomed : m_aDoomed)
		{
			if (doomed && !doomed.IsDeleted() && !HasPlayerAboard(doomed))
				SCR_EntityHelper.DeleteEntityAndChildren(doomed);
		}
		m_aDoomed.Clear();
		if (m_Armor)
			m_Armor.DeleteAll();

		string text = string.Format("Appuis : %1 tir(s) arrêté(s), %2 pièce(s) de mortier et %3 batterie(s) retirée(s), blindés retirés", missions, teams, batteries);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.APPUI, text + " (" + author + ")");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! cmd_ap_v, cmd_ap_n puis cmd_ap_rN_code, _mortier_ban, _artillerie_ban ; cmd_ap_mortier_tirs (« unix,unix »),
	//! cmd_ap_artillerie_dernier, cmd_ap_bombe_dernier, cmd_ap_avion (« unix,unix,unix »), cmd_ap_helico_dernier,
	//! cmd_ap_frappes (« x;z;unix|… ») — SRP_Commander.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		EnsureRegionArrays();
		int count = m_aMortarBanUntil.Count();
		ctx.WriteValue("cmd_ap_v", 1);
		ctx.WriteValue("cmd_ap_n", count);
		for (int r = 0; r < count; r++)
		{
			string prefix = "cmd_ap_r" + r.ToString();
			ctx.WriteValue(prefix + "_code", RegionCode(r));
			ctx.WriteValue(prefix + "_mortier_ban", m_aMortarBanUntil[r]);
			ctx.WriteValue(prefix + "_artillerie_ban", m_aArtilleryBanUntil[r]);
		}
		ctx.WriteValue("cmd_ap_mortier_tirs", JoinInts(m_aMortarUnix));
		ctx.WriteValue("cmd_ap_artillerie_dernier", m_iArtilleryUnix);
		ctx.WriteValue("cmd_ap_bombe_dernier", m_iBombUnix);
		ctx.WriteValue("cmd_ap_avion", JoinInts(m_aFlybyUnix));
		ctx.WriteValue("cmd_ap_helico_dernier", m_iHeliUnix);

		string strikes = "";
		foreach (SRP_CmdStrike strike : m_aStrikes)
		{
			if (!strike)
				continue;
			int x = Math.Round(strike.m_vPos[0]);
			int z = Math.Round(strike.m_vPos[2]);
			if (!strikes.IsEmpty())
				strikes += "|";
			strikes += string.Format("%1;%2;%3", x, z, strike.m_iUnix);
		}
		ctx.WriteValue("cmd_ap_frappes", strikes);
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture miroir par code de région ; heures passées nettoyées — SRP_Commander.ReadFrom
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		EnsureRegionArrays();
		int now = System.GetUnixTime();
		SRP_CmdRegionBook book = RegionBook();

		int count = 0;
		if (!ctx.ReadValue("cmd_ap_n", count))
			count = 0;
		for (int i = 0; i < count; i++)
		{
			string prefix = "cmd_ap_r" + i.ToString();
			string code = "";
			int mortarBan = 0;
			int artilleryBan = 0;
			ctx.ReadValue(prefix + "_code", code);
			ctx.ReadValue(prefix + "_mortier_ban", mortarBan);
			ctx.ReadValue(prefix + "_artillerie_ban", artilleryBan);
			int region = -1;
			if (book)
				region = book.FindRegion(code);
			if (region < 0 || region >= m_aMortarBanUntil.Count())
			{
				if (mortarBan > now || artilleryBan > now)
					SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Appuis : région %1 introuvable à la relecture, son interdiction est ignorée", code));
				continue;
			}
			if (mortarBan <= now)
				mortarBan = 0;
			if (artilleryBan <= now)
				artilleryBan = 0;
			m_aMortarBanUntil.Set(region, mortarBan);
			m_aArtilleryBanUntil.Set(region, artilleryBan);
		}

		string text = "";
		if (ctx.ReadValue("cmd_ap_mortier_tirs", text))
			ParseInts(text, m_aMortarUnix);
		int value = 0;
		if (ctx.ReadValue("cmd_ap_artillerie_dernier", value))
			m_iArtilleryUnix = value;
		value = 0;
		if (ctx.ReadValue("cmd_ap_bombe_dernier", value))
			m_iBombUnix = value;
		text = "";
		if (ctx.ReadValue("cmd_ap_avion", text))
			ParseInts(text, m_aFlybyUnix);
		value = 0;
		if (ctx.ReadValue("cmd_ap_helico_dernier", value))
			m_iHeliUnix = value;

		text = "";
		if (ctx.ReadValue("cmd_ap_frappes", text))
		{
			m_aStrikes.Clear();
			array<string> entries = {};
			text.Split("|", entries, true);
			int unreadable = 0;
			foreach (string entry : entries)
			{
				array<string> fields = {};
				entry.Split(";", fields, true);
				if (fields.Count() != 3)
				{
					unreadable++;
					continue;
				}
				SRP_CmdStrike strike = new SRP_CmdStrike();
				strike.m_vPos = Vector(fields[0].ToFloat(), 0, fields[1].ToFloat());
				strike.m_iUnix = fields[2].ToInt();
				m_aStrikes.Insert(strike);
			}
			if (unreadable > 0)
				SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Appuis : %1 frappe(s) illisible(s) dans cmd_ap_frappes, ignorée(s)", unreadable));
		}
		PruneCaps(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne : CancelAll, interdictions, plafonds et frappes vidés — SRP_Commander.ResetCampaign
	void ResetCampaign(string reason)
	{
		CancelAll(reason);
		for (int r = 0; r < m_aMortarBanUntil.Count(); r++)
			m_aMortarBanUntil.Set(r, 0);
		for (int a = 0; a < m_aArtilleryBanUntil.Count(); a++)
			m_aArtilleryBanUntil.Set(a, 0);
		m_aMortarUnix.Clear();
		m_aFlybyUnix.Clear();
		m_aStrikes.Clear();
		m_iArtilleryUnix = 0;
		m_iBombUnix = 0;
		m_iHeliUnix = 0;
		m_iHeliTicket = 0;
		SetDirty(true);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.APPUI, "Appuis remis à neuf (nouvelle campagne) : interdictions et plafonds effacés — " + reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Après une restauration : missions de tir arrêtées, pièces et batteries retirées hors de vue — SRP_Commander
	void OnRestored()
	{
		ScriptCallQueue queue = GetGame().GetCallqueue();
		if (queue)
		{
			queue.Remove(FireNextId);
			queue.Remove(DropBomb);
		}
		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
			EndMission(m_aMissions[i], "front ramené à une copie datée", true);
		m_aMissions.Clear();
		CancelBomb("front ramené à une copie datée");
		for (int t = m_aTeams.Count() - 1; t >= 0; t--)
			DiscardTeam(m_aTeams[t], false, "front ramené à une copie datée");
		for (int b = m_aBatteries.Count() - 1; b >= 0; b--)
			DiscardBattery(m_aBatteries[b], false, "front ramené à une copie datée");
		m_iHeliTicket = 0;
		PruneCaps(System.GetUnixTime());
	}

	//================================================================================================
	// Statiques (toutes machines pour les RPC, verrou du 2S1, renseignement)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! AP5, renseignement : le véhicule a au moins une place de tourelle (TurretCompartmentSlot) — cerveau
	//! (NoteSighting), SRP_CmdArmor
	static bool IsArmedVehicle(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (TurretCompartmentSlot.Cast(slot))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! AP1 : le véhicule est un 2S1 (nom de prefab contenant « Tracked/2S1/ », constante : le client ne lit pas le
	//! fichier du serveur) — modded SCR_GetInUserAction (SRP_CmdArmor.c), TickBatteries
	static bool IsBatteryVehicle(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		EntityPrefabData data = vehicle.GetPrefabData();
		if (!data)
			return false;
		string name = data.GetPrefabName();
		return name.Contains(BATTERY_MARK);
	}

	//------------------------------------------------------------------------------------------------
	//! MO3, chaque machine sauf un serveur dédié : son de départ (SCR_AudioSourceConfiguration, projet acp, évènement
	//! eventName) joué à pos — SRP_FrontEnemyComponent.RpcDo_SRPCmdSound
	static void PlaySoundLocal(string acp, string eventName, vector pos)
	{
		if (System.IsConsoleApp())
			return;
		if (acp.IsEmpty() || eventName.IsEmpty())
			return;
		SCR_SoundManagerModule sounds = SCR_SoundManagerModule.GetInstance(GetGame().GetWorld());
		if (!sounds)
			return;
		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = acp;
		config.m_sSoundEventName = eventName;
		// Créée par new : la valeur par défaut de l'attribut ne s'applique pas ; drapeaux posés comme
		// SCR_LongRangeSoundSystem (signaux d'environnement compris)
		config.m_eFlags = EAudioSourceConfigurationFlag.Static | EAudioSourceConfigurationFlag.EnvironmentSignals | EAudioSourceConfigurationFlag.FinishWhenEntityDestroyed;
		SCR_AudioSource source = sounds.CreateAudioSource(config, pos);
		if (!source)
			return;	// trop loin pour être entendu ici
		sounds.PlayAudioSource(source);
	}

	//------------------------------------------------------------------------------------------------
	//! Repli appui_obus_relance_clients : relance locale de l'obus répliqué (Replication.FindItem, Launch) —
	//! SRP_FrontEnemyComponent.RpcDo_SRPCmdLaunch
	static void LaunchLocal(RplId shell, vector dir)
	{
		LaunchLocalTry(shell, dir, 0);
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Staff passe toujours ; sinon faux si appui_actif = 0, Commandeur gelé (VU6) ou région désorganisée (OF6, OF8) —
	//! Request*
	protected bool CanActFor(int region, bool staff, out string why)
	{
		why = "";
		if (staff)
			return true;
		if (!m_bOn)
		{
			why = "appuis coupés (appui_actif = 0)";
			return false;
		}
		if (m_Commander && m_Commander.IsFrozen())
		{
			why = "Commandeur gelé";
			return false;
		}
		SRP_CmdRegionBook book = RegionBook();
		if (region >= 0 && book && book.IsRegionDisorganized(region))
		{
			why = "région désorganisée (officier tombé) : aucun appui";
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sécurité (TOUJOURS, Staff compris) : à appui_distance_base_m de la base ou sur un carré de base, en mer, ou un
	//! soldat ennemi ou civil vivant (ou un véhicule occupé par eux) à radius -> faux et why — Request*, chaque coup
	protected bool IsSafeTarget(vector position, float radius, out string why)
	{
		why = "";
		vector ground = SRP_Placement.OnGround(position);
		vector basePosition;
		if (SRP_Placement.BasePosition(basePosition) && vector.DistanceXZ(ground, basePosition) < m_iBaseM)
		{
			why = string.Format("à moins de %1 m de la base", m_iBaseM);
			return false;
		}
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsReady())
		{
			int cell = front.CellIndexAt(ground);
			if (cell < 0)
			{
				why = "point en mer ou hors de la carte";
				return false;
			}
			if (front.IsBaseCell(cell))
			{
				why = "carré de la base";
				return false;
			}
		}
		if (SRP_Placement.IsWater(ground))
		{
			why = "point en mer";
			return false;
		}
		if (radius <= 0)
			return true;

		s_aSafetyHits.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(ground, radius, QuerySafety, null, EQueryEntitiesFlags.DYNAMIC);
		if (s_aSafetyHits.IsEmpty())
			return true;
		IEntity hit = s_aSafetyHits[0];
		s_aSafetyHits.Clear();
		int meters = 0;
		if (hit)
			meters = Math.Round(vector.Distance(hit.GetOrigin(), ground));
		int limit = Math.Round(radius);
		why = string.Format("soldat ou civil à %1 m du point (moins de %2 m)", meters, limit);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! MO6 : carré rouge, ou bleu au front — Request*
	protected bool IsValidTargetCell(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return false;
		int cell = front.CellIndexAt(position);
		if (cell < 0)
			return false;
		int owner = front.GetCellOwner(cell);
		if (owner == SRP_EFrontOwner.ROUGE)
			return true;
		return owner == SRP_EFrontOwner.BLEU && front.IsCellAtFront(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! EQ3 : une frappe (explosif, artillerie, bombe) à appui_meme_endroit_m depuis appui_meme_endroit_min — Request*
	protected bool StruckRecently(vector position, int nowUnix)
	{
		int window = m_iSameSpotMin * 60;
		float reach = m_iSameSpotM;
		foreach (SRP_CmdStrike strike : m_aStrikes)
		{
			if (!strike || nowUnix - strike.m_iUnix >= window)
				continue;
			if (vector.DistanceXZ(strike.m_vPos, position) < reach)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Équipe de mortier de la région, gardée ou posée (Ask COMBAT « mortier-R3 », Tag COMBAT, rôle SERVANT) ; null et
	//! why si impossible — RequestMortar, RequestSmoke
	protected SRP_CmdMortarTeam EnsureTeam(int region, vector aim, out string why)
	{
		return EnsureTeamEx(region, aim, false, why);
	}

	//------------------------------------------------------------------------------------------------
	//! EnsureTeam, avec le repli du Staff : sans point caché côté rouge, un point caché hors de la base suffit — Request*
	protected SRP_CmdMortarTeam EnsureTeamEx(int region, vector aim, bool staff, out string why)
	{
		why = "";
		int now = System.GetUnixTime();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		SRP_CmdMortarTeam team = FindTeam(region);
		if (team)
		{
			if (team.m_bSilenced)
			{
				why = "pièce de la région hors d'usage";
				return null;
			}
			bool alive = team.m_Piece && !team.m_Piece.IsDeleted();
			if (!alive || team.m_bSabotaged || IsPieceBroken(team))
			{
				// MO9 : pièce détruite ou sabotée avant le passage de TickTeams : réduite au silence, jamais remplacée
				string brokenCause = "pièce détruite";
				if (alive && team.m_bSabotaged)
					brokenCause = "pièce sabotée";
				SilenceTeam(team, now, brokenCause);
				why = "pièce de la région hors d'usage";
				return null;
			}
			if (alive && vector.DistanceXZ(team.m_vPos, aim) <= m_iMortarRangeM)
			{
				team.m_iLastNeededUnix = now;
				return team;
			}
			if (team.m_bFiring)
			{
				why = "pièce déjà en tir";
				return null;
			}
			if (alive && SRP_Placement.IsSeenByAnyPlayer(team.m_vPos, players))
			{
				why = "pièce trop loin de la cible et vue par un joueur : impossible de la déplacer";
				return null;
			}
			DiscardTeam(team, true, "déplacée vers un nouveau tir");
		}

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !enemies.HasPrefabs())
		{
			why = "ennemi absent : aucun servant possible";
			return null;
		}
		vector spot;
		bool found = FindHiddenSpot(aim, m_iMortarPieceMinM, m_iMortarPieceMaxM, m_iMortarPlayerMinM, players, true, spot);
		if (!found && staff)
			found = FindHiddenSpot(aim, m_iMortarPieceMinM, m_iMortarPieceMaxM, m_iMortarPlayerMinM, players, false, spot);
		if (!found)
		{
			why = "aucun emplacement caché côté ennemi pour la pièce";
			return null;
		}

		int soldiers = Math.Max(m_iMortarCrew, enemies.GroupSize(m_sMortarCrewGroup));
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (capacity)
		{
			string room = capacity.Ask("mortier-" + RegionCode(region), SRP_ECmdCapClass.COMBAT, soldiers, 1, spot, "appui");
			if (!room.IsEmpty())
			{
				why = "limite de soldats : " + room;
				return null;
			}
		}
		IEntity piece = SpawnOriented(m_sMortarPiece, spot, aim);
		if (!piece)
		{
			why = "pièce de mortier non créée (prefab appui_mortier_piece)";
			return null;
		}
		vector crewSpot = SRP_Placement.OnGround(spot + Vector(3, 0, 2));
		SRP_EnemyGroup crew = enemies.SpawnGroup(crewSpot, true, spot, "appui", CREW_HOLD_M, vector.Zero, -1, false, m_sMortarCrewGroup);
		if (!crew)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(piece);
			why = "servants non posés (prefab des servants illisible)";
			return null;
		}
		SRP_CmdCapacity.Tag(crew, SRP_ECmdCapClass.COMBAT);
		crew.m_iCmdRole = SRP_ECmdRole.SERVANT;
		enemies.NoteHome(crew, "appui", true);

		team = new SRP_CmdMortarTeam();
		team.m_iRegion = region;
		team.m_Piece = piece;
		team.m_Servants = crew;
		team.m_vPos = piece.GetOrigin();
		team.m_iPlacedUnix = now;
		team.m_iLastNeededUnix = now;
		m_aTeams.Insert(team);
		int distance = Math.Round(vector.DistanceXZ(team.m_vPos, aim));
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Pièce de mortier de la %1 posée au carré %2, à %3 m de la cible, %4 servant(s) attendu(s)", RegionLabel(region), CellOf(team.m_vPos), distance, soldiers));
		return team;
	}

	//------------------------------------------------------------------------------------------------
	//! Un coup (observateur MO4, sécurité, Consume, départ, CallLater(DropShell, appui_mortier_temps_vol_s)) puis le
	//! suivant — CallLater
	protected void FireNext(SRP_CmdFireMission mission)
	{
		if (!mission || !m_aMissions.Contains(mission))
			return;
		if (mission.m_iKind == SRP_ECmdOrder.ARTILLERIE)
		{
			FireArtillery(mission);
			return;
		}
		int now = System.GetUnixTime();
		string stop = MissionStopReason(mission);
		if (!stop.IsEmpty())
		{
			EndMission(mission, stop, false);
			return;
		}
		SRP_CmdMortarTeam team = FindTeam(mission.m_iRegion);
		if (!team || team.m_bSilenced || !team.m_Piece || team.m_Piece.IsDeleted())
		{
			EndMission(mission, "pièce muette ou retirée", false);
			return;
		}
		team.m_iLastNeededUnix = now;

		// MO1 : la pièce ne tire que si un servant est à son poste (pose en cours : on attend quelques secondes)
		if (ServantsOnDuty(team) == 0)
		{
			if (team.m_Servants && SRP_EnemyComponent.InGraceOf(team.m_Servants, System.GetTickCount()))
			{
				ScheduleNext(mission, SERVANT_WAIT_S);
				return;
			}
			EndMission(mission, "aucun servant à son poste", false);
			return;
		}

		// Point d'impact : points imposés d'abord (fumigènes), puis obus explosifs sur la cible
		vector impact;
		bool smokeShot = false;
		if (!mission.m_aFixed.IsEmpty())
		{
			smokeShot = true;
			vector fixedPoint = mission.m_aFixed[0];
			mission.m_aFixed.RemoveOrdered(0);
			impact = Scatter(fixedPoint, Math.RandomFloat(0, m_iSmokeSpreadM));
		}
		else if (mission.m_iShell == SRP_ECmdShell.FUMEE)
		{
			EndMission(mission, "", false);
			return;
		}
		else
		{
			// MO4 : un soldat qui voit un joueur près du point le guide ; l'écart se resserre à partir du 2e obus
			vector seen;
			if (m_Commander && m_Commander.IsPlayerWatchedNear(mission.m_vAim, m_iMortarObserverM, seen))
			{
				mission.m_vAim = seen;
				if (mission.m_iHeFired >= 1)
					mission.m_fError = Math.Max(m_iMortarErrorMinM, mission.m_fError - m_iMortarErrorStepM);
			}
			impact = Scatter(mission.m_vAim, mission.m_fError * Math.RandomFloat(0.85, 1.15));
		}

		float radius = m_iSafeHeM;
		if (smokeShot)
			radius = m_iSafeSmokeM;
		string why;
		if (!IsSafeTarget(impact, radius, why))
		{
			// MO6 : coup sauté, sans obus ni stock ; deux de suite = fin du tir
			mission.m_iSkippedInRow++;
			mission.m_iShellsLeft--;
			if (mission.m_iSkippedInRow >= MORTAR_SKIP_MAX)
			{
				EndMission(mission, "soldats ou civils trop près : " + why, false);
				return;
			}
			SRP_CmdLog.Note(mission.m_iRegion, SRP_ECmdLogKind.APPUI, "Mortier : coup sauté, " + why);
			if (mission.m_iShellsLeft <= 0)
			{
				EndMission(mission, "", false);
				return;
			}
			ScheduleNext(mission, RandRange(m_iMortarRateMinS, m_iMortarRateMaxS));
			return;
		}

		if (!smokeShot)
		{
			SRP_CmdResources resources = SRP_CmdResources.Get();
			if (mission.m_iTicket > 0)
			{
				if (!resources || resources.GetTicketLeft(mission.m_iTicket) <= 0)
				{
					EndMission(mission, "plus d'obus", false);
					return;
				}
				resources.Consume(mission.m_iTicket, 1);
			}
			mission.m_iHeFired++;
		}

		PlayDeparture(m_sMortarSound, m_sMortarSoundEvent, team.m_vPos);
		ResourceName prefab = m_sShellHe;
		if (smokeShot)
			prefab = m_sShellSmoke;
		GetGame().GetCallqueue().CallLater(DropShell, m_iMortarFlightS * 1000, false, prefab, impact);
		mission.m_iFired++;
		mission.m_iShellsLeft--;
		mission.m_iSkippedInRow = 0;
		if (mission.m_iShellsLeft <= 0)
		{
			EndMission(mission, "", false);
			return;
		}
		ScheduleNext(mission, RandRange(m_iMortarRateMinS, m_iMortarRateMaxS));
	}

	//------------------------------------------------------------------------------------------------
	//! Obus lâché à appui_obus_hauteur_m au-dessus du point (ProjectileMoveComponent.Launch vers le bas) — CallLater
	protected void DropShell(ResourceName prefab, vector impact)
	{
		// Dernier contrôle au moment où l'obus arrive : il ne part pas si quelqu'un est entré dans la zone entre-temps
		float radius = m_iSafeHeM;
		if (prefab == m_sShellSmoke)
			radius = m_iSafeSmokeM;
		string why;
		if (!IsSafeTarget(impact, radius, why))
		{
			SRP_CmdLog.Note(RegionAt(impact), SRP_ECmdLogKind.APPUI, "Obus retenu au dernier moment : " + why);
			return;
		}
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Appuis : prefab d'obus illisible " + prefab);
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		// Obus tourné vers le bas : X à droite, Y vers le nord, Z (avant) vers le sol
		params.Transform[0] = Vector(1, 0, 0);
		params.Transform[1] = Vector(0, 0, 1);
		params.Transform[2] = Vector(0, -1, 0);
		float height = m_iShellHeightM;
		params.Transform[3] = SRP_Placement.OnGround(impact) + Vector(0, height, 0);
		IEntity shellEntity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!shellEntity)
			return;
		ProjectileMoveComponent move = ProjectileMoveComponent.Cast(shellEntity.FindComponent(ProjectileMoveComponent));
		if (!move)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(shellEntity);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Appuis : l'obus n'a pas de ProjectileMoveComponent " + prefab);
			return;
		}
		vector down = Vector(0, -1, 0);
		move.Launch(down, vector.Zero, m_fShellSpeedCoef, shellEntity, null, null, null, null);
		// Repli : chaque client relance l'obus (le coefficient de vitesse voyage dans la longueur de la direction)
		if (!m_bShellClientRelaunch || !m_Commander)
			return;
		RplComponent rpl = RplComponent.Cast(shellEntity.FindComponent(RplComponent));
		SRP_FrontEnemyComponent host = m_Commander.GetHost();
		if (rpl && host)
			host.BroadcastLaunch(rpl.Id(), down * m_fShellSpeedCoef);
	}

	//------------------------------------------------------------------------------------------------
	//! Batterie 2S1 de la région, gardée ou posée ; null et why si impossible — RequestArtillery
	protected SRP_CmdBattery EnsureBattery(int region, vector aim, out string why)
	{
		why = "";
		int now = System.GetUnixTime();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		SRP_CmdBattery battery = FindBattery(region);
		if (battery)
		{
			bool alive = !battery.m_bDestroyed && battery.m_Vehicle && !battery.m_Vehicle.IsDeleted();
			if (!alive)
			{
				why = "batterie de la région hors d'usage";
				return null;
			}
			float distance = vector.DistanceXZ(battery.m_vPos, aim);
			if (distance >= m_iBatteryMinM && distance <= m_iBatteryRangeM)
			{
				battery.m_iLastNeededUnix = now;
				return battery;
			}
			if (SRP_Placement.IsVehicleSeenByAnyPlayer(battery.m_Vehicle, players))
			{
				why = "batterie mal placée pour ce tir et vue par un joueur : impossible de la déplacer";
				return null;
			}
			DiscardBattery(battery, true, "déplacée vers un nouveau tir");
		}

		vector spot;
		if (!FindHiddenSpot(aim, m_iBatteryMinM, m_iBatteryMaxM, 1200, players, true, spot))
		{
			why = "aucun emplacement caché côté ennemi pour la batterie";
			return null;
		}
		IEntity vehicle = SpawnOriented(m_sBatteryPrefab, spot, aim);
		if (!vehicle)
		{
			why = "batterie non créée (prefab appui_artillerie_batterie)";
			return null;
		}
		// Faction du véhicule vide (le vrai verrou est le test du prefab, SCR_GetInUserAction moddé dans SRP_CmdArmor.c)
		string factionKey = "";
		if (m_Armor)
			factionKey = m_Armor.m_sFaction;
		FactionManager factions = GetGame().GetFactionManager();
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		if (affiliation && factions && !factionKey.IsEmpty() && factions.GetFactionByKey(factionKey))
			affiliation.SetAffiliatedFactionByKey(factionKey);

		battery = new SRP_CmdBattery();
		battery.m_iRegion = region;
		battery.m_Vehicle = vehicle;
		battery.m_vPos = vehicle.GetOrigin();
		battery.m_iPlacedUnix = now;
		battery.m_iLastNeededUnix = now;
		m_aBatteries.Insert(battery);
		int meters = Math.Round(vector.DistanceXZ(battery.m_vPos, aim));
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Batterie 2S1 de la %1 posée au carré %2, à %3 m de la cible", RegionLabel(region), CellOf(battery.m_vPos), meters));
		return battery;
	}

	//------------------------------------------------------------------------------------------------
	//! Avion ou bombe RHS par script (EOnEditorPlace de l'entité éditable, recette SCR_CallInSupportContainer) —
	//! RequestBomb, RequestFlyby
	protected IEntity SpawnCallIn(ResourceName prefab, vector position)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;
		// Cap au hasard (l'avion ne vient pas toujours du même côté)
		float yaw = Math.RandomFloat(0, Math.PI2);
		float sinus = Math.Sin(yaw);
		float cosinus = Math.Cos(yaw);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = Vector(cosinus, 0, -sinus);
		params.Transform[1] = Vector(0, 1, 0);
		params.Transform[2] = Vector(sinus, 0, cosinus);
		params.Transform[3] = SRP_Placement.OnGround(position);
		IEntity spawned = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!spawned)
			return null;
		// RHS_CAS_EditableEntityComponent programme la charge et le nettoyage dans cet appel ; -1 = aucun auteur
		SCR_EditableEntityComponent editable = SCR_EditableEntityComponent.Cast(spawned.FindComponent(SCR_EditableEntityComponent));
		if (editable)
		{
			SCR_EditableEntityComponent parent;
			editable.EOnEditorPlace(parent, null, 0, false, -1);
		}
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Largage (sécurité revue, Consume, frappe notée) — CallLater
	protected void DropBomb(vector aim)
	{
		if (!m_bBombPending)
			return;
		int now = System.GetUnixTime();
		int region = m_iBombRegion;
		string stop = "";
		if (!m_bBombStaff)
			CanActFor(region, false, stop);
		string safeWhy;
		if (stop.IsEmpty() && !IsSafeTarget(aim, m_iSafeBombM, safeWhy))
			stop = "sécurité : " + safeWhy;
		vector point = aim;
		if (stop.IsEmpty())
		{
			float spread = m_iBombSpreadM * Math.Sqrt(Math.RandomFloat01());
			point = Scatter(aim, spread);
			IEntity bomb = SpawnCallIn(m_sBombPrefab, point);
			if (!bomb)
				stop = "bombe non créée (prefab appui_bombe_prefab)";
		}
		if (!stop.IsEmpty())
		{
			CancelBomb("bombe annulée au largage : " + stop);
			return;
		}

		m_bBombPending = false;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources && m_iBombTicket > 0 && resources.GetTicketLeft(m_iBombTicket) > 0)
			resources.Consume(m_iBombTicket, 1);
		m_iBombTicket = 0;
		if (!m_bBombStaff)
		{
			m_iBombUnix = now;
			NoteStrike(point, now);
		}
		SetDirty(true);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Bombe FAB-500 larguée sur %1 : explosion dans environ 26 s", ZoneText(point)));
	}

	//------------------------------------------------------------------------------------------------
	//! Frappe notée (EQ3) — tirs explosifs, salve, bombe
	protected void NoteStrike(vector position, int nowUnix)
	{
		SRP_CmdStrike strike = new SRP_CmdStrike();
		strike.m_vPos = position;
		strike.m_iUnix = nowUnix;
		m_aStrikes.Insert(strike);
		SetDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Refus d'une demande : rend why, sans rien écrire (§1.8 : l'appelant écrit SRP_CmdLog.Refusal, le cerveau par
	//! IssueOrder, les manœuvres par LogSupport ; une seule ligne par refus) — Request*
	protected string Refuse(int region, string what, string why)
	{
		return why;
	}

	//================================================================================================
	// Interne : missions de tir
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Nouvelle mission (pas encore inscrite) — Request*
	protected SRP_CmdFireMission NewMission(int kind, int shell, int region, vector aim, string reason, bool staff)
	{
		SRP_CmdFireMission mission = new SRP_CmdFireMission();
		mission.m_iId = m_iNextMissionId;
		m_iNextMissionId++;
		mission.m_iKind = kind;
		mission.m_iShell = shell;
		mission.m_iRegion = region;
		mission.m_vAim = aim;
		mission.m_fError = m_iMortarErrorStartM;
		mission.m_bStaff = staff;
		mission.m_sReason = reason;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsReady())
			mission.m_iZone = front.GetZoneAt(aim);
		return mission;
	}

	//------------------------------------------------------------------------------------------------
	//! Inscrit une mission de mortier et programme son premier coup (MO5) — Request*
	protected void StartMission(SRP_CmdFireMission mission, SRP_CmdMortarTeam team, int delaySeconds)
	{
		m_aMissions.Insert(mission);
		team.m_bFiring = true;
		team.m_iLastNeededUnix = System.GetUnixTime();
		ScheduleNext(mission, delaySeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Coup suivant dans seconds secondes (on passe le numéro de la mission, jamais l'objet) — FireNext
	protected void ScheduleNext(SRP_CmdFireMission mission, int seconds)
	{
		int delay = seconds;
		if (delay < 0)
			delay = 0;
		GetGame().GetCallqueue().CallLater(FireNextId, delay * 1000, false, mission.m_iId);
	}

	//------------------------------------------------------------------------------------------------
	//! Relais du CallLater : retrouve la mission par son numéro (disparue : rien) — CallLater
	protected void FireNextId(int id)
	{
		SRP_CmdFireMission mission = FindMission(id);
		if (!mission)
			return;
		FireNext(mission);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin d'une mission : reste du ticket rendu, pièce libérée, ligne de journal ; why vide = tir fini normalement
	protected void EndMission(SRP_CmdFireMission mission, string why, bool quiet)
	{
		if (!mission)
			return;
		// Tout ce qui sert après le retrait de la liste est lu avant (la mission peut disparaître avec elle)
		int ticket = mission.m_iTicket;
		int kind = mission.m_iKind;
		int region = mission.m_iRegion;
		int fired = mission.m_iFired;
		bool salvo = mission.m_bSalvo;
		mission.m_iTicket = 0;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (ticket > 0 && resources && resources.GetTicketLeft(ticket) > 0)
		{
			string releaseWhy = why;
			if (releaseWhy.IsEmpty())
				releaseWhy = "fin du tir";
			resources.Release(ticket, releaseWhy);
		}
		m_aMissions.RemoveItem(mission);

		if (kind != SRP_ECmdOrder.ARTILLERIE)
		{
			SRP_CmdMortarTeam team = FindTeam(region);
			if (team && !HasMortarMission(region))
				team.m_bFiring = false;
		}
		if (quiet)
			return;
		string label = "Mortier";
		if (kind == SRP_ECmdOrder.ARTILLERIE)
			label = "Artillerie lourde";
		if (why.IsEmpty())
		{
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("%1 : tir fini, %2 obus parti(s)", label, fired));
			return;
		}
		string state = "arrêté";
		if (kind == SRP_ECmdOrder.ARTILLERIE && !salvo)
			state = "annulé avant la salve, tir rendu au stock";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("%1 : tir %2 après %3 obus (%4)", label, state, fired, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Raison d'arrêter une mission en cours (VU6 : gel ; OF6 : désorganisation ; appui_actif) ; jamais pour le Staff
	protected string MissionStopReason(SRP_CmdFireMission mission)
	{
		if (!mission || mission.m_bStaff)
			return "";
		string why;
		CanActFor(mission.m_iRegion, false, why);
		return why;
	}

	//------------------------------------------------------------------------------------------------
	//! Un coup de réglage ou de salve de l'artillerie lourde (AP1, EQ1) — FireNext
	protected void FireArtillery(SRP_CmdFireMission mission)
	{
		int now = System.GetUnixTime();
		string stop = MissionStopReason(mission);
		SRP_CmdBattery battery = FindBattery(mission.m_iRegion);
		if (stop.IsEmpty() && (!battery || battery.m_bDestroyed || !battery.m_Vehicle || battery.m_Vehicle.IsDeleted()))
			stop = "batterie détruite ou retirée";
		if (!stop.IsEmpty())
		{
			EndMission(mission, stop, false);
			return;
		}
		battery.m_iLastNeededUnix = now;

		// Coups de réglage (EQ1), puis la salve appui_artillerie_avance_min_s..max_s après le dernier
		if (!mission.m_bSalvo && !mission.m_aFixed.IsEmpty())
		{
			vector adjustPoint = mission.m_aFixed[0];
			mission.m_aFixed.RemoveOrdered(0);
			string adjustWhy;
			if (IsSafeTarget(adjustPoint, m_iSafeHeM, adjustWhy))
			{
				PlayDeparture(m_sBatterySound, m_sBatterySoundEvent, battery.m_vPos);
				ResourceName adjustPrefab = m_sShellHe;
				GetGame().GetCallqueue().CallLater(DropShell, m_iMortarFlightS * 1000, false, adjustPrefab, adjustPoint);
				mission.m_iFired++;
			}
			else
			{
				SRP_CmdLog.Note(mission.m_iRegion, SRP_ECmdLogKind.APPUI, "Artillerie lourde : coup de réglage sauté, " + adjustWhy);
			}
			if (!mission.m_aFixed.IsEmpty())
				ScheduleNext(mission, RandRange(ADJUST_GAP_MIN_S, ADJUST_GAP_MAX_S));
			else
				ScheduleNext(mission, RandRange(m_iAdjustLeadMinS, m_iAdjustLeadMaxS));
			return;
		}

		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (!mission.m_bSalvo)
		{
			// Sécurité à la décision de la salve (220 m), puis 150 m à chaque obus
			string salvoWhy;
			if (!IsSafeTarget(mission.m_vAim, m_iSafeSalvoM, salvoWhy))
			{
				EndMission(mission, "salve annulée, " + salvoWhy, false);
				return;
			}
			mission.m_bSalvo = true;
			if (resources && mission.m_iTicket > 0 && resources.GetTicketLeft(mission.m_iTicket) > 0)
				resources.Consume(mission.m_iTicket, 1);
			if (!mission.m_bStaff)
			{
				m_iArtilleryUnix = now;
				NoteStrike(mission.m_vAim, now);
			}
			SetDirty(true);
			SRP_CmdLog.Note(mission.m_iRegion, SRP_ECmdLogKind.APPUI, string.Format("Artillerie lourde : salve de %1 obus sur %2", mission.m_iShellsLeft, ZoneText(mission.m_vAim)));
		}

		// Un obus de la salve, tiré uniformément dans le disque de appui_artillerie_rayon_m
		float distance = m_iSalvoRadiusM * Math.Sqrt(Math.RandomFloat01());
		vector impact = Scatter(mission.m_vAim, distance);
		string shellWhy;
		mission.m_iShellsLeft--;
		if (!IsSafeTarget(impact, m_iSafeHeM, shellWhy))
		{
			mission.m_iSkippedInRow++;
			if (mission.m_iSkippedInRow >= SALVO_SKIP_MAX)
			{
				EndMission(mission, "soldats ou civils trop près : " + shellWhy, false);
				return;
			}
		}
		else
		{
			PlayDeparture(m_sBatterySound, m_sBatterySoundEvent, battery.m_vPos);
			ResourceName salvoPrefab = m_sShellHe;
			GetGame().GetCallqueue().CallLater(DropShell, m_iMortarFlightS * 1000, false, salvoPrefab, impact);
			mission.m_iFired++;
			mission.m_iSkippedInRow = 0;
		}
		if (mission.m_iShellsLeft <= 0)
		{
			EndMission(mission, "", false);
			return;
		}
		ScheduleNext(mission, RandRange(m_iSalvoRateMinS, m_iSalvoRateMaxS));
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôles d'un tir de mortier (hors Staff : gel, désorganisation, plafond, seuil, stock, MO6, même endroit ;
	//! toujours : interdiction de 24 h, pièce en tir ou hors d'usage, sécurité) ; shells = obus explosifs voulus (0 =
	//! appui_mortier_obus_min) ; "" = possible — Request*, MortarRefusal
	protected string MortarChecks(int region, vector aim, int shell, bool staff, int shells)
	{
		string why;
		if (!CanActFor(region, staff, why))
			return why;
		EnsureRegionArrays();
		if (region < 0 || region >= m_aMortarBanUntil.Count())
			return "point hors des régions ennemies";
		int now = System.GetUnixTime();
		if (m_aMortarBanUntil[region] > now)
			return string.Format("mortier de la %1 réduit au silence, encore %2", RegionLabel(region), Dur(m_aMortarBanUntil[region] - now));
		SRP_CmdMortarTeam team = FindTeam(region);
		if (team && team.m_bFiring)
			return "pièce déjà en tir";
		if (team && (team.m_bSilenced || team.m_bSabotaged))
			return "pièce de la région hors d'usage";
		bool explosive = shell != SRP_ECmdShell.FUMEE;
		if (!staff)
		{
			if (explosive || m_bSmokeCounts)
			{
				int shots = CountSince(m_aMortarUnix, now, HOUR_S);
				if (shots >= m_iMortarPerHour)
					return string.Format("plafond : %1 tir(s) de mortier dans l'heure, %2 au plus", shots, m_iMortarPerHour);
			}
			if (explosive)
			{
				int need = shells;
				if (need <= 0)
					need = m_iMortarShellsMin;
				int players = PlayersNear(aim);
				if (players < m_iMortarPlayers)
					return string.Format("seuil : %1 joueur(s) à %2 m, il en faut %3", players, m_iPlayersM, m_iMortarPlayers);
				SRP_CmdResources resources = SRP_CmdResources.Get();
				if (!resources)
					return "stocks du Commandeur absents";
				string stockWhy;
				if (!resources.CanReserve(SRP_ECmdStock.OBUS, region, need, stockWhy))
					return "stock : " + stockWhy;
				if (!IsValidTargetCell(aim))
					return "carré visé ni ennemi ni au front (MO6)";
				if (StruckRecently(aim, now))
					return string.Format("une frappe à moins de %1 m il y a moins de %2 min", m_iSameSpotM, m_iSameSpotMin);
			}
		}
		float radius = m_iSafeSmokeM;
		if (explosive)
			radius = m_iSafeHeM;
		string safeWhy;
		if (!IsSafeTarget(aim, radius, safeWhy))
			return "sécurité : " + safeWhy;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Bombe en attente de largage annulée : ticket rendu, journal — DropBomb, CancelAll, OnRestored
	protected void CancelBomb(string why)
	{
		if (!m_bBombPending)
			return;
		m_bBombPending = false;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources && m_iBombTicket > 0 && resources.GetTicketLeft(m_iBombTicket) > 0)
			resources.Release(m_iBombTicket, why);
		m_iBombTicket = 0;
		SRP_CmdLog.Note(m_iBombRegion, SRP_ECmdLogKind.APPUI, why);
	}

	//================================================================================================
	// Interne : surveillance (Tick)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! MO9 (silence) et MO2 (retrait) de chaque équipe de mortier — Tick
	protected void TickTeams(int nowUnix, array<IEntity> players)
	{
		int tick = System.GetTickCount();
		for (int i = m_aTeams.Count() - 1; i >= 0; i--)
		{
			SRP_CmdMortarTeam team = m_aTeams[i];
			if (!team)
			{
				m_aTeams.Remove(i);
				continue;
			}
			if (team.m_bSilenced && GetMortarSilencedUntil(team.m_iRegion) == 0)
			{
				// Interdiction finie (ou levée) : l'épave et les survivants partent hors de vue, une pièce neuve viendra
				DiscardTeam(team, false, "interdiction de mortier terminée");
				continue;
			}
			if (!team.m_bSilenced)
			{
				string cause = "";
				if (!team.m_Piece || team.m_Piece.IsDeleted())
					cause = "pièce détruite";
				else if (IsPieceBroken(team))
					cause = "pièce détruite";
				else if (team.m_bSabotaged)
					cause = "pièce sabotée";
				else if (team.m_Servants)
				{
					SRP_EnemyGroup crew = team.m_Servants;
					SRP_EnemyComponent.NoteSeen(crew);
					if (!SRP_EnemyComponent.InGraceOf(crew, tick))
					{
						if (crew.m_bAbandoned || crew.m_iMaxSeen == 0)
						{
							// La file d'apparition n'a livré personne : ce n'est pas l'œuvre des joueurs, pas d'interdiction
							DiscardTeam(team, false, "servants jamais arrivés (file d'apparition)");
							continue;
						}
						if (ServantsOnDuty(team) > 0)
							team.m_iNoServantSinceUnix = 0;
						else if (team.m_iNoServantSinceUnix == 0)
							team.m_iNoServantSinceUnix = nowUnix;
						else if (nowUnix - team.m_iNoServantSinceUnix >= m_iMortarSilenceConfirmS)
							cause = "plus aucun servant à son poste";
					}
				}
				if (!cause.IsEmpty())
					SilenceTeam(team, nowUnix, cause);
			}

			// MO2 : retrait sans interdiction quand plus aucun joueur n'est à portée depuis appui_mortier_retrait_min
			vector piecePos = team.m_vPos;
			if (team.m_Piece && !team.m_Piece.IsDeleted())
				piecePos = team.m_Piece.GetOrigin();
			if (SRP_Placement.NearestPlayer(piecePos, players) <= m_iMortarPresenceM)
			{
				team.m_iLastNeededUnix = nowUnix;
				continue;
			}
			if (team.m_bFiring || nowUnix - team.m_iLastNeededUnix < m_iMortarRetireMin * 60)
				continue;
			if (SRP_Placement.IsSeenByAnyPlayer(piecePos, players))
				continue;
			DiscardTeam(team, true, "plus aucun joueur à portée");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! AP1 : garde à l'approche, batterie neutralisée (détruite, disparue, occupée par un joueur), retrait — Tick
	protected void TickBatteries(int nowUnix, array<IEntity> players)
	{
		for (int i = m_aBatteries.Count() - 1; i >= 0; i--)
		{
			SRP_CmdBattery battery = m_aBatteries[i];
			if (!battery)
			{
				m_aBatteries.Remove(i);
				continue;
			}
			if (battery.m_bDestroyed && GetBatteryDestroyedUntil(battery.m_iRegion) == 0)
			{
				DiscardBattery(battery, false, "interdiction d'artillerie lourde terminée");
				continue;
			}
			IEntity vehicle = battery.m_Vehicle;
			bool present = vehicle && !vehicle.IsDeleted();
			if (!battery.m_bDestroyed)
			{
				string cause = "";
				if (!present)
					cause = "batterie détruite";
				else if (IsVehicleDestroyed(vehicle))
					cause = "batterie détruite";
				else if (HasPlayerAboard(vehicle))
					cause = "batterie occupée par un joueur";
				if (!cause.IsEmpty())
					NeutralizeBattery(battery, nowUnix, cause);
			}

			vector batteryPos = battery.m_vPos;
			if (present)
				batteryPos = vehicle.GetOrigin();
			float nearest = SRP_Placement.NearestPlayer(batteryPos, players);
			if (!battery.m_bDestroyed && !battery.m_Guard && nearest <= m_iBatteryGuardM)
				PostGuard(battery);
			if (nearest <= m_iBatteryPresenceM)
			{
				battery.m_iLastNeededUnix = nowUnix;
				continue;
			}
			if (HasArtilleryMission(battery.m_iRegion) || nowUnix - battery.m_iLastNeededUnix < m_iBatteryRetireMin * 60)
				continue;
			if (present && SRP_Placement.IsVehicleSeenByAnyPlayer(vehicle, players))
				continue;
			DiscardBattery(battery, true, "plus aucun joueur à portée");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sortie d'hélico finie (plus aucun vol) : la sortie est consommée — Tick
	protected void TickHeli(int nowUnix)
	{
		if (m_iHeliTicket == 0)
			return;
		if (SRP_HeliSearch.Count() > 0)
			return;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources && resources.GetTicketLeft(m_iHeliTicket) > 0)
			resources.Consume(m_iHeliTicket, 1);
		m_iHeliTicket = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Décors retirés « hors de vue » : effacés dès qu'aucun joueur ne les voit (jamais avec un joueur à bord) — Tick
	protected void TickDoomed(array<IEntity> players)
	{
		for (int i = m_aDoomed.Count() - 1; i >= 0; i--)
		{
			IEntity doomed = m_aDoomed[i];
			if (!doomed || doomed.IsDeleted())
			{
				m_aDoomed.Remove(i);
				continue;
			}
			if (HasPlayerAboard(doomed))
				continue;
			bool seen = false;
			if (Vehicle.Cast(doomed))
				seen = SRP_Placement.IsVehicleSeenByAnyPlayer(doomed, players);
			else
				seen = SRP_Placement.IsSeenByAnyPlayer(doomed.GetOrigin(), players);
			if (seen)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(doomed);
			m_aDoomed.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Plafonds périmés retirés (tirs de plus d'une heure, passages de plus de 24 h, frappes de plus de
	//! appui_meme_endroit_min, interdictions passées) — Tick, Start, ReadFrom
	protected void PruneCaps(int nowUnix)
	{
		PruneList(m_aMortarUnix, nowUnix, HOUR_S);
		PruneList(m_aFlybyUnix, nowUnix, DAY_S);
		int window = m_iSameSpotMin * 60;
		for (int i = m_aStrikes.Count() - 1; i >= 0; i--)
		{
			SRP_CmdStrike strike = m_aStrikes[i];
			if (!strike || nowUnix - strike.m_iUnix >= window)
				m_aStrikes.Remove(i);
		}
		for (int r = 0; r < m_aMortarBanUntil.Count(); r++)
		{
			if (m_aMortarBanUntil[r] != 0 && m_aMortarBanUntil[r] <= nowUnix)
				m_aMortarBanUntil.Set(r, 0);
		}
		for (int a = 0; a < m_aArtilleryBanUntil.Count(); a++)
		{
			if (m_aArtilleryBanUntil[a] != 0 && m_aArtilleryBanUntil[a] <= nowUnix)
				m_aArtilleryBanUntil.Set(a, 0);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MO9 : pièce réduite au silence -> région sans mortier appui_mortier_silence_heures (sauvé), tirs arrêtés, radio
	protected void SilenceTeam(SRP_CmdMortarTeam team, int nowUnix, string cause)
	{
		team.m_bSilenced = true;
		team.m_bFiring = false;
		int region = team.m_iRegion;
		EnsureRegionArrays();
		int until = nowUnix + m_iMortarSilenceHours * HOUR_S;
		if (region >= 0 && region < m_aMortarBanUntil.Count())
			m_aMortarBanUntil.Set(region, until);
		StopMissions(region, false, "pièce réduite au silence");
		// Les servants encore debout redeviennent des soldats ordinaires (entraide possible)
		if (team.m_Servants)
			team.m_Servants.m_iCmdRole = SRP_ECmdRole.AUCUN;
		SRP_FrontRadio.SupportSilenced(region, false);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Mortier de la %1 réduit au silence (%2) : aucun mortier dans la région jusqu'à %3 (%4)", RegionLabel(region), cause, ClockAt(until, nowUnix), Dur(until - nowUnix)));
		SetDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! AP1 : batterie neutralisée -> région sans artillerie lourde appui_artillerie_silence_heures (sauvé), radio
	protected void NeutralizeBattery(SRP_CmdBattery battery, int nowUnix, string cause)
	{
		battery.m_bDestroyed = true;
		int region = battery.m_iRegion;
		EnsureRegionArrays();
		int until = nowUnix + m_iBatterySilenceHours * HOUR_S;
		if (region >= 0 && region < m_aArtilleryBanUntil.Count())
			m_aArtilleryBanUntil.Set(region, until);
		StopMissions(region, true, "batterie neutralisée");
		if (battery.m_Guard)
			battery.m_Guard.m_iCmdRole = SRP_ECmdRole.AUCUN;
		SRP_FrontRadio.SupportSilenced(region, true);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Batterie 2S1 de la %1 neutralisée (%2) : aucune artillerie lourde dans la région jusqu'à %3 (%4)", RegionLabel(region), cause, ClockAt(until, nowUnix), Dur(until - nowUnix)));
		SetDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! AP1 : section de garde posée près de la batterie quand un joueur approche (Ask GARDE, rôle SERVANT) — TickBatteries
	protected void PostGuard(SRP_CmdBattery battery)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !enemies.HasPrefabs())
			return;
		ResourceName prefab = enemies.PickSectionPrefab();
		int soldiers = 6;
		if (!prefab.IsEmpty())
			soldiers = Math.Max(1, enemies.GroupSize(prefab));
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (capacity)
		{
			string room = capacity.Ask("batterie-" + RegionCode(battery.m_iRegion), SRP_ECmdCapClass.GARDE, soldiers, 1, battery.m_vPos, "appui");
			if (!room.IsEmpty())
				return;	// demande notée par la capacité, nouvel essai au passage suivant
		}
		vector spot = SRP_Placement.OnGround(battery.m_vPos + Vector(8, 0, 8));
		SRP_EnemyGroup guard = enemies.SpawnGroup(spot, true, battery.m_vPos, "appui", GUARD_HOLD_M, vector.Zero, -1, false, prefab);
		if (!guard)
			return;
		SRP_CmdCapacity.Tag(guard, SRP_ECmdCapClass.GARDE);
		guard.m_iCmdRole = SRP_ECmdRole.SERVANT;
		enemies.NoteHome(guard, "appui", true);
		battery.m_Guard = guard;
		SRP_CmdLog.Note(battery.m_iRegion, SRP_ECmdLogKind.APPUI, string.Format("Batterie 2S1 de la %1 : section de garde posée (%2 soldat(s)), un joueur approche", RegionLabel(battery.m_iRegion), soldiers));
	}

	//------------------------------------------------------------------------------------------------
	//! Retire une équipe : tout de suite (immediate) ou hors de vue (servants par RetireLater, pièce effacée par
	//! TickDoomed) ; ses tirs sont arrêtés — TickTeams, EnsureTeamEx, ClearBans, CancelAll, OnRestored
	protected void DiscardTeam(SRP_CmdMortarTeam team, bool immediate, string why)
	{
		if (!team)
			return;
		int region = team.m_iRegion;
		IEntity piece = team.m_Piece;
		SRP_EnemyGroup crew = team.m_Servants;
		team.m_Servants = null;
		m_aTeams.RemoveItem(team);
		StopMissions(region, false, "pièce retirée");
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (crew && enemies)
		{
			if (immediate)
			{
				enemies.Delete(crew);
			}
			else
			{
				array<ref SRP_EnemyGroup> leaving = {};
				leaving.Insert(crew);
				enemies.RetireLater(leaving);
			}
		}
		if (piece && !piece.IsDeleted())
		{
			if (immediate)
				SCR_EntityHelper.DeleteEntityAndChildren(piece);
			else if (!m_aDoomed.Contains(piece))
				m_aDoomed.Insert(piece);
		}
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Mortier de la %1 retiré : %2", RegionLabel(region), why));
	}

	//------------------------------------------------------------------------------------------------
	//! Retire une batterie (même règle ; un véhicule avec un joueur à bord n'est jamais supprimé) — TickBatteries,
	//! EnsureBattery, ClearBans, CancelAll, OnRestored
	protected void DiscardBattery(SRP_CmdBattery battery, bool immediate, string why)
	{
		if (!battery)
			return;
		int region = battery.m_iRegion;
		IEntity vehicle = battery.m_Vehicle;
		SRP_EnemyGroup guard = battery.m_Guard;
		battery.m_Guard = null;
		m_aBatteries.RemoveItem(battery);
		StopMissions(region, true, "batterie retirée");
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (guard && enemies)
		{
			if (immediate)
			{
				enemies.Delete(guard);
			}
			else
			{
				array<ref SRP_EnemyGroup> leaving = {};
				leaving.Insert(guard);
				enemies.RetireLater(leaving);
			}
		}
		if (vehicle && !vehicle.IsDeleted() && !HasPlayerAboard(vehicle))
		{
			if (immediate)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			else if (!m_aDoomed.Contains(vehicle))
				m_aDoomed.Insert(vehicle);
		}
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, string.Format("Batterie 2S1 de la %1 retirée : %2", RegionLabel(region), why));
	}

	//------------------------------------------------------------------------------------------------
	//! Arrête les tirs de la région : artillerie (artillery) ou mortier — SilenceTeam, NeutralizeBattery, Discard*
	protected void StopMissions(int region, bool artillery, string why)
	{
		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
		{
			SRP_CmdFireMission mission = m_aMissions[i];
			if (!mission || mission.m_iRegion != region)
				continue;
			bool isArtillery = mission.m_iKind == SRP_ECmdOrder.ARTILLERIE;
			if (isArtillery != artillery)
				continue;
			EndMission(mission, why, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MO9 : la pièce est sortie de sa phase de départ (dégâts au moins égaux aux points de vie de base du décor
	//! multiphase) ou détruite — TickTeams
	protected bool IsPieceBroken(SRP_CmdMortarTeam team)
	{
		IEntity piece = team.m_Piece;
		if (!piece)
			return true;
		DamageManagerComponent damage = DamageManagerComponent.Cast(piece.FindComponent(DamageManagerComponent));
		if (!damage)
			return false;
		if (damage.IsDestroyed() || damage.GetState() == EDamageState.DESTROYED)
			return true;
		float health = damage.GetHealth();
		if (team.m_fHealth0 < 0)
		{
			team.m_fHealth0 = health;
			return false;
		}
		SCR_DestructionDamageManagerComponentClass data = SCR_DestructionDamageManagerComponentClass.Cast(damage.GetComponentData(piece));
		if (!data || data.m_fBaseHealth <= 0)
			return false;
		return team.m_fHealth0 - health >= data.m_fBaseHealth - 0.01;
	}

	//------------------------------------------------------------------------------------------------
	//! Servants « en service » : vivants, conscients, à appui_mortier_servant_rayon_m de la pièce (MO9)
	protected int ServantsOnDuty(SRP_CmdMortarTeam team)
	{
		if (!team || !team.m_Servants || !team.m_Piece || team.m_Piece.IsDeleted())
			return 0;
		array<ref SRP_EnemyGroup> crew = {};
		crew.Insert(team.m_Servants);
		float reach = m_iMortarCrewM;
		return SRP_EnemyComponent.ActiveAgentsNear(crew, team.m_Piece.GetOrigin(), reach);
	}

	//------------------------------------------------------------------------------------------------
	//! Véhicule détruit (état de dégâts DESTROYED, comme SCR_GetInUserAction.c:56)
	protected static bool IsVehicleDestroyed(IEntity vehicle)
	{
		if (!vehicle)
			return true;
		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur est assis dans ce véhicule
	protected static bool HasPlayerAboard(IEntity vehicle)
	{
		if (!vehicle)
			return false;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return false;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;
			IEntity occupant = slot.GetOccupant();
			if (occupant && SRP_Utils.GetPlayerIdFromEntity(occupant) > 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Rappel de la recherche de sécurité : un personnage vivant qui n'est pas un joueur, ou un véhicule occupé par
	//! un tel personnage, arrête la recherche
	protected static bool QuerySafety(IEntity e)
	{
		if (!e)
			return true;
		if (ChimeraCharacter.Cast(e))
		{
			if (SRP_Utils.IsDead(e) || SRP_Utils.GetPlayerIdFromEntity(e) > 0)
				return true;
			s_aSafetyHits.Insert(e);
			return false;
		}
		if (!Vehicle.Cast(e))
			return true;
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(e.FindComponent(BaseCompartmentManagerComponent));
		if (!compartments)
			return true;
		array<BaseCompartmentSlot> slots = {};
		compartments.GetCompartments(slots);
		foreach (BaseCompartmentSlot slot : slots)
		{
			if (!slot)
				continue;
			IEntity occupant = slot.GetOccupant();
			if (!occupant || SRP_Utils.IsDead(occupant) || SRP_Utils.GetPlayerIdFromEntity(occupant) > 0)
				continue;
			s_aSafetyHits.Insert(occupant);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Relance locale d'un obus répliqué, avec quelques essais si l'entité n'est pas encore arrivée — LaunchLocal
	protected static void LaunchLocalTry(RplId shell, vector dir, int attempt)
	{
		if (Replication.IsServer())
			return;	// le serveur a déjà lancé l'obus
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(shell));
		if (!rpl)
		{
			if (attempt < LAUNCH_TRIES)
				GetGame().GetCallqueue().CallLater(LaunchLocalTry, 100, false, shell, dir, attempt + 1);
			return;
		}
		IEntity entity = rpl.GetEntity();
		if (!entity)
			return;
		ProjectileMoveComponent move = ProjectileMoveComponent.Cast(entity.FindComponent(ProjectileMoveComponent));
		if (!move)
			return;
		float coef = dir.Length();
		if (coef < 0.01)
			coef = 1;
		vector direction = dir * (1.0 / coef);
		move.Launch(direction, vector.Zero, coef, entity, null, null, null, null);
	}

	//================================================================================================
	// Interne : aides
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	protected SRP_CmdRegionBook RegionBook()
	{
		if (!m_Commander)
			return null;
		return m_Commander.GetBook();
	}

	//------------------------------------------------------------------------------------------------
	//! Tableaux par région à la taille du tracé (valeurs déjà lues gardées)
	protected void EnsureRegionArrays()
	{
		int count = 0;
		SRP_CmdRegionBook book = RegionBook();
		if (book)
			count = book.GetRegionCount();
		while (m_aMortarBanUntil.Count() < count)
			m_aMortarBanUntil.Insert(0);
		while (m_aArtilleryBanUntil.Count() < count)
			m_aArtilleryBanUntil.Insert(0);
	}

	//------------------------------------------------------------------------------------------------
	protected int RegionAt(vector position)
	{
		SRP_CmdRegionBook book = RegionBook();
		if (!book)
			return -1;
		return book.GetRegionAt(position);
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe » (« toute l'île » pour -1)
	protected string RegionLabel(int region)
	{
		if (region < 0)
			return "toute l'île";
		string label = "";
		if (m_Commander)
			label = m_Commander.GetRegionLabel(region);
		if (label.IsEmpty())
			label = "région " + RegionCode(region);
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! « R3 »
	protected string RegionCode(int region)
	{
		string code = "";
		SRP_CmdRegionBook book = RegionBook();
		if (book && region >= 0)
			code = book.GetRegionCode(region);
		if (code.IsEmpty())
		{
			int number = region + 1;
			code = "R" + number.ToString();
		}
		return code;
	}

	//------------------------------------------------------------------------------------------------
	//! « 074 042 »
	protected string CellOf(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "?";
		int cell = front.CellIndexAt(position);
		if (cell < 0)
			return "en mer";
		return front.CellRef(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! « 074 042 — Régina (S07) »
	protected string ZoneText(vector position)
	{
		string text = CellOf(position);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return text;
		int zone = front.GetZoneAt(position);
		if (zone < 0)
			return text;
		return text + " — " + front.GetZoneLabel(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! EQ4 : joueurs valides à appui_rayon_joueurs_m (seul nombre réel lu par le Commandeur)
	protected int PlayersNear(vector position)
	{
		if (!m_Commander)
			return 0;
		return m_Commander.GetDosingPlayers(position);
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdMortarTeam FindTeam(int region)
	{
		foreach (SRP_CmdMortarTeam team : m_aTeams)
		{
			if (team && team.m_iRegion == region)
				return team;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdBattery FindBattery(int region)
	{
		foreach (SRP_CmdBattery battery : m_aBatteries)
		{
			if (battery && battery.m_iRegion == region)
				return battery;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdFireMission FindMission(int id)
	{
		foreach (SRP_CmdFireMission mission : m_aMissions)
		{
			if (mission && mission.m_iId == id)
				return mission;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Un tir de mortier (mortier, fumée, feinte) est-il en cours dans la région ?
	protected bool HasMortarMission(int region)
	{
		foreach (SRP_CmdFireMission mission : m_aMissions)
		{
			if (mission && mission.m_iRegion == region && mission.m_iKind != SRP_ECmdOrder.ARTILLERIE)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Un tir d'artillerie lourde est-il en cours (region -1 : partout) ?
	protected bool HasArtilleryMission(int region)
	{
		foreach (SRP_CmdFireMission mission : m_aMissions)
		{
			if (!mission || mission.m_iKind != SRP_ECmdOrder.ARTILLERIE)
				continue;
			if (region < 0 || mission.m_iRegion == region)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! EQ3 : la grosse opération en cours, en clair ("" = aucune) ; les appuis attachés à la contre-attaque de
	//! attachedZone comptent avec elle, mais un seul appui lourd par contre-attaque ; countArmor faux (RequestArmor) : les
	//! blindés engagés ne comptent pas, leur plafond est appui_blinde_simultanes
	protected string BigOperationReason(int attachedZone, bool countArmor)
	{
		if (countArmor && m_Armor && m_Armor.CountActive() > 0)
			return "blindé engagé";
		if (HasArtilleryMission(-1))
			return "artillerie lourde en cours";
		if (m_bBombPending)
			return "bombe en route";
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (host && host.IsCounterAttackActive())
		{
			int zone = host.GetCounterAttackZone();
			if (zone != attachedZone)
			{
				string label = "une zone";
				SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
				if (front && zone >= 0)
					label = front.GetZoneLabel(zone);
				return "contre-attaque sur " + label;
			}
		}
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes avant le prochain tir lourd possible (EQ3), 0 = possible
	protected int ArtilleryWait(int nowUnix)
	{
		if (m_iArtilleryUnix <= 0)
			return 0;
		int left = m_iArtilleryUnix + m_iArtilleryIntervalH * HOUR_S - nowUnix;
		if (left < 0)
			return 0;
		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes avant la prochaine bombe possible (AP3, 24 h glissantes), 0 = possible
	protected int BombWait(int nowUnix)
	{
		if (m_iBombUnix <= 0)
			return 0;
		int left = m_iBombUnix + m_iBombIntervalH * HOUR_S - nowUnix;
		if (left < 0)
			return 0;
		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! Mortier d'une région, en clair (page Staff)
	protected string MortarStateText(int region, int nowUnix)
	{
		int until = GetMortarSilencedUntil(region);
		if (until > 0)
			return string.Format("muet jusqu'à %1 (encore %2)", ClockAt(until, nowUnix), Dur(until - nowUnix));
		SRP_CmdMortarTeam team = FindTeam(region);
		if (!team || !team.m_Piece)
			return "prêt";
		if (team.m_bSilenced)
			return "hors d'usage (retrait en attente)";
		if (team.m_bFiring)
			return "en tir (pièce au carré " + CellOf(team.m_vPos) + ")";
		return string.Format("en batterie au carré %1, %2 servant(s) à leur poste", CellOf(team.m_vPos), ServantsOnDuty(team));
	}

	//------------------------------------------------------------------------------------------------
	//! Batterie d'une région, en clair (page Staff)
	protected string BatteryStateText(int region, int nowUnix)
	{
		int until = GetBatteryDestroyedUntil(region);
		if (until > 0)
			return string.Format("interdite jusqu'à %1 (encore %2)", ClockAt(until, nowUnix), Dur(until - nowUnix));
		SRP_CmdBattery battery = FindBattery(region);
		if (!battery || !battery.m_Vehicle)
			return "prête";
		if (battery.m_bDestroyed)
			return "batterie hors d'usage (retrait en attente)";
		if (HasArtilleryMission(region))
			return "en tir (batterie au carré " + CellOf(battery.m_vPos) + ")";
		string guardText = "sans garde";
		if (battery.m_Guard)
			guardText = "gardée";
		return string.Format("batterie en place au carré %1, %2", CellOf(battery.m_vPos), guardText);
	}

	//------------------------------------------------------------------------------------------------
	//! Point caché côté rouge (requireRed) à minDist..maxDist d'aim, jamais dans la zone de la base — EnsureTeamEx,
	//! EnsureBattery
	protected bool FindHiddenSpot(vector aim, float minDist, float maxDist, float playerMin, array<IEntity> players, bool requireRed, out vector spot)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		for (int attempt = 0; attempt < SPOT_TRIES; attempt++)
		{
			vector candidate;
			if (!SRP_Placement.FindSpawnPosition(aim, minDist, maxDist, players, false, true, candidate, playerMin))
				continue;
			if (front && front.IsReady())
			{
				if (front.IsInBaseZone(candidate))
					continue;
				if (requireRed && !front.IsRedAt(candidate))
					continue;
			}
			spot = candidate;
			return true;
		}
		spot = vector.Zero;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose un décor ou un véhicule tourné vers lookAt (cap seul)
	protected IEntity SpawnOriented(ResourceName prefab, vector position, vector lookAt)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;
		vector forward = FlatDirection(position, lookAt);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = Vector(forward[2], 0, -forward[0]);
		params.Transform[1] = Vector(0, 1, 0);
		params.Transform[2] = forward;
		params.Transform[3] = SRP_Placement.OnGround(position);
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Bruit de départ (MO3) chez tous les joueurs, par la RPC de l'hôte ; rien si le projet ou l'évènement est vide
	protected void PlayDeparture(string acp, string eventName, vector pos)
	{
		if (acp.IsEmpty() || eventName.IsEmpty() || !m_Commander)
			return;
		SRP_FrontEnemyComponent host = m_Commander.GetHost();
		if (host)
			host.BroadcastSound(acp, eventName, pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Signale au journal un prefab de réglage illisible — Start
	protected void CheckPrefab(string key, string prefab)
	{
		if (prefab.IsEmpty())
			return;
		Resource res = Resource.Load(prefab);
		if (res && res.IsValid())
			return;
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Appuis : prefab illisible pour %1 : %2", key, prefab));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetDirty(bool immediate)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(immediate);
	}

	//------------------------------------------------------------------------------------------------
	//! Tirage entier dans [a, b], bornes dans n'importe quel ordre
	protected static int RandRange(int a, int b)
	{
		if (b < a)
			return Math.RandomIntInclusive(b, a);
		return Math.RandomIntInclusive(a, b);
	}

	//------------------------------------------------------------------------------------------------
	//! Direction à plat de from vers to (au hasard si les points sont confondus) ; jamais vector + float
	protected static vector FlatDirection(vector from, vector to)
	{
		vector delta = to - from;
		delta[1] = 0;
		float length = delta.Length();
		if (length < 0.5)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			return Vector(Math.Cos(angle), 0, Math.Sin(angle));
		}
		return delta * (1.0 / length);
	}

	//------------------------------------------------------------------------------------------------
	//! Point décalé de distance mètres dans une direction au hasard, posé au sol
	protected static vector Scatter(vector point, float distance)
	{
		float angle = Math.RandomFloat(0, Math.PI2);
		vector shifted = point + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		return SRP_Placement.OnGround(shifted);
	}

	//------------------------------------------------------------------------------------------------
	//! MO7 : count points sur width mètres, en travers de la direction dir, centrés sur center
	protected static void BuildCurtain(vector center, vector dir, int count, float width, notnull array<vector> points)
	{
		points.Clear();
		vector across = Vector(-dir[2], 0, dir[0]);
		for (int i = 0; i < count; i++)
		{
			float offset = 0;
			if (count > 1)
				offset = -width * 0.5 + width * i / (count - 1);
			points.Insert(SRP_Placement.OnGround(center + across * offset));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Heures de la liste plus récentes que window secondes
	protected static int CountSince(array<int> times, int nowUnix, int window)
	{
		int count = 0;
		foreach (int t : times)
		{
			if (nowUnix - t < window)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static void PruneList(array<int> times, int nowUnix, int window)
	{
		for (int i = times.Count() - 1; i >= 0; i--)
		{
			if (nowUnix - times[i] >= window)
				times.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! « unix,unix »
	protected static string JoinInts(array<int> values)
	{
		string text = "";
		foreach (int v : values)
		{
			if (!text.IsEmpty())
				text += ",";
			text += v.ToString();
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Lecture de « unix,unix » (valeurs illisibles ou nulles ignorées)
	protected static void ParseInts(string text, notnull array<int> values)
	{
		values.Clear();
		array<string> parts = {};
		text.Split(",", parts, true);
		foreach (string part : parts)
		{
			int v = part.ToInt();
			if (v > 0)
				values.Insert(v);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! « 2 h 10 », « 12 min », « 45 s »
	protected static string Dur(int seconds)
	{
		int total = seconds;
		if (total < 0)
			total = 0;
		if (total < 60)
			return string.Format("%1 s", total);
		int minutes = total / 60;
		if (minutes < 60)
			return string.Format("%1 min", minutes);
		int hours = minutes / 60;
		int rest = minutes - hours * 60;
		if (rest == 0)
			return string.Format("%1 h", hours);
		return string.Format("%1 h %2", hours, SRP_Time.Pad2(rest));
	}

	//------------------------------------------------------------------------------------------------
	//! Heure locale « 21:14 » d'une heure Unix à venir (heure locale actuelle + écart)
	protected static string ClockAt(int unix, int nowUnix)
	{
		int hour;
		int minute;
		int second;
		System.GetHourMinuteSecond(hour, minute, second);
		int total = hour * 60 + minute + (unix - nowUnix) / 60;
		int days = total / 1440;
		total = total - days * 1440;
		if (total < 0)
			total = total + 1440;
		int hh = total / 60;
		int mm = total - hh * 60;
		return string.Format("%1:%2", SRP_Time.Pad2(hh), SRP_Time.Pad2(mm));
	}
}
