//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : L'ENNEMI DU FRONT (module 4) et HÔTE DU COMMANDEUR (#76). Contrats figés.
//
// RÔLE (serveur ; le composant existe sur TOUTES les machines pour la seule RPC de son du mortier) :
// - Garnisons au front d'abord (F11), mémoire des pertes F12 (données sur SRP_FrontLocality, sauvegardées par le
//   socle), effectif Nominal / Effective (trou 7 du Commandeur), postes du front F7 à F9, patrouilles F10.
// - Contre-attaques F1, F2, F4, F6, H4 (+ CA1 à CA7 par SRP_CmdManeuvers) : pose m_iAssaultZone sur chaque groupe
//   d'assaut (remis à -1 avant RetireLater ou ConvertToGarrison) et SetZoneFlag ; la REPRISE des carrés est faite
//   par la capture (trou 11). Défense réussie : menace -1 (F4, C8).
// - Offensive de la nuit H2 : une fois par jour, 3 h - 7 h, serveur vide depuis 30 min, UNE zone du front
//   (jamais une voisine de la base), par SRP_ZoneFall.LoseZone(zone, OFFENSIVE, true).
// - Façade des localités (partie localités de SRP_Territory.c) : IsGarrisonPosted, IsGarrisonSettled,
//   OnLocalityTaken, AdoptAssailants, IsInGarrisonedLocality, WithdrawLocality.
// - Hôte du Commandeur : m_Commander (SRP_Commander), piloté par le Tick de 5 s en heure Unix ; contrats NEUTRES
//   (OnZoneCaptured, OnZoneLost, OnLocalityTaken, OnKeyPointSeized, OnMissionSucceeded, OnSabotageUsed,
//   OnThreatChanged) qui ne font rien quand le Commandeur est absent ; surcharge d'OnControllableDestroyed ; RPC de
//   son et de relance d'obus (modèle SRP_Sirene.c:504-515).
// - Sauvegarde : chaîne unique SRP_FrontComponent.Save -> WriteTo(ctx) (clés fe_*) -> SRP_Commander.WriteTo(ctx)
//   (clés cmd_*). Aucun fichier à part (ennemi_front.json abandonné, trou 16). L'ordre des OnDelete n'étant pas
//   garanti, OnDelete fait écrire le socle (SaveFinalFromEnemy) AVANT d'arrêter le Commandeur.
// Délais en heure Unix (System.GetUnixTime) pour tout ce qui compte au-delà d'une session ; GetTickCount seulement
// pour les champs de SRP_EnemyGroup qui en sont déjà (pose, contact, occupation).
// APPELÉ PAR : SRP_ZoneFall (OnZoneCaptured, OnZoneLost, OnLocalityTaken, OnKeyPointSeized), le socle (WriteTo,
// ReadFrom, ResetAll, OnRestored, OnFrontReset, OnThreatChanged, réglages), SRP_TerritoryComponent (F11, F12),
// SRP_Enemy (F10), SRP_Missions (OnMissionSucceeded, OnSabotageUsed), SRP_FrontScreens (rapports, Staff), le Commandeur.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Cache par carré (la géométrie ne change pas) : note de poste (F8)
class SRP_FrontCellInfo
{
	bool m_bDone;				// calculé
	int m_iScore;				// carrefour 3, route principale 2, hauteur 2 (+1 si route), piste 1, 0 rien
	string m_sKind = "";		// « carrefour », « route », « hauteur » ou « piste »
	vector m_vPost;				// point du poste
	float m_fTop;				// altitude maximale
	bool m_bAlt;				// altitudes relevées (5 x 5 points)
	float m_fMean;				// altitude moyenne des 25 points
	vector m_vTop;				// point le plus haut
	int m_iRoadScore;			// note de la route seule (axe des vagues) : carrefour 3, route principale 2, piste 1
}

//------------------------------------------------------------------------------------------------
//! Un poste du front vivant (F7 à F9)
class SRP_FrontPost
{
	int m_iCell = -1;
	vector m_vPos;
	string m_sKind;
	ref array<ref SRP_EnemyGroup> m_aGroups = {};		// toujours des ref (SRP_EnemyTrucks.c:45-47)
	int m_iNoPlayerSince;								// heure Unix
	int m_iDeferUntil;									// heure Unix
	IEntity m_Fort;										// décor retranché
}

//------------------------------------------------------------------------------------------------
//! La contre-attaque en cours (une à la fois). Lue et pilotée aussi par SRP_CmdManeuvers (crochets CA1 à CA7).
//! Rien n'est sauvegardé (H3) : une attaque en cours s'annule au redémarrage.
class SRP_CounterAttack
{
	int m_iZone = -1;
	string m_sZone;										// libellé « Régina (S07) »
	int m_iState = SRP_EFrontAttack.AUCUNE;				// ANNONCEE puis ASSAUT
	int m_iWarnEndUnix;									// fin de l'annonce (m_iAttackWarningMinutes)
	int m_iAssaultStartUnix;
	int m_iLastWaveUnix;
	int m_iNextWaveUnix;
	int m_iWaves;										// vagues parties
	int m_iPlannedWaves;								// 2 à 4, revu à la 2e vague (CA3)
	bool m_bRevised;
	int m_iPostedSoldiers;
	int m_iWave1Posted;
	int m_iWave1Fit;
	int m_iRetakenAtWave2;
	int m_iRegion = -1;									// région du Commandeur
	bool m_bDisorganized;								// OF8 : une vague de moins, ni mortier ni blindé
	bool m_bFeint;										// MO8
	bool m_bSupportMortar;								// CA4
	bool m_bSupportHeavy;								// CA4
	bool m_bForced;										// forcée par le Staff
	bool m_bCmdDriven;									// lancée ou menée (même un temps) par le Commandeur actif
	vector m_vTarget;									// point clé, sinon centre des carrés bleus
	vector m_vOriginA;									// axe 1
	vector m_vOriginB;									// axe 2
	bool m_bHasB;
	ref array<ref SRP_EnemyGroup> m_aGroups = {};		// tous les groupes d'assaut (m_iAssaultZone = m_iZone)
	ref array<ref SRP_EnemyGroup> m_aLastWave = {};
	ref array<ref SRP_EnemyGroup> m_aArrived = {};
	ref array<ref SRP_EnemyGroup> m_aObjGroups = {};	// AdvanceAssault d'origine (Commandeur gelé)
	ref array<int> m_aObjCells = {};
	ref array<int> m_aObjSince = {};
	int m_iRetaken;										// carrés repris par l'ennemi pendant l'attaque
	int m_iEnd = SRP_EAttackEnd.AUCUNE;
	int m_iBlueSeen = -1;								// carrés bleus visés au dernier passage (compte des reprises, CountAttackBlue)
	ref array<ref SRP_EnemyGroup> m_aWave1 = {};		// groupes réels de la 1re vague (CA3)
	int m_iWave1Paper;									// soldats sur le papier de la 1re vague (CA3)
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Front SimpleRP : l'ennemi du front (garnisons, postes, contre-attaques, offensive de la nuit) et hôte du Commandeur")]
class SRP_FrontEnemyComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
class SRP_FrontEnemyComponent : SCR_BaseGameModeComponent
{
	// --- Réglages : mémoire des pertes et offensive de la nuit (F12, H2) ----------------------------
	[Attribute("1", UIWidgets.CheckBox, "Mémoire des pertes des localités (F12)", category: "SimpleRP - Front : ennemi")]
	bool m_bLossMemory;

	[Attribute("4", UIWidgets.EditBox, "Heures réelles pour qu'une localité soit de nouveau pleine, un quart par heure (F12, clé front_regarnissage_heures)", category: "SimpleRP - Front : ennemi")]
	float m_fLossRefillHours;

	[Attribute("1000", UIWidgets.EditBox, "F11 : une localité au contact passe avant une localité de l'arrière placée jusqu'à … mètres plus près des joueurs", category: "SimpleRP - Front : ennemi")]
	float m_fFrontPriorityBonus;

	[Attribute("1", UIWidgets.CheckBox, "Offensive de la nuit, une fois par jour (H2, Q1)", category: "SimpleRP - Front : ennemi")]
	bool m_bFictiveOffensive;

	[Attribute("3", UIWidgets.EditBox, "Offensive : début de la fenêtre, heure du serveur (H2)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveHourStart;

	[Attribute("7", UIWidgets.EditBox, "Offensive : fin de la fenêtre (exclue), heure du serveur (H2)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveHourEnd;

	[Attribute("30", UIWidgets.EditBox, "Offensive : serveur vide depuis … minutes (H2)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveEmptyMinutes;

	[Attribute("10", UIWidgets.EditBox, "Offensive : chance de réussite de base, sur 100 (Q1)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveChanceBase;

	[Attribute("5", UIWidgets.EditBox, "Offensive : chance ajoutée par point de menace, sur 100 (Q1)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveChancePerThreat;

	[Attribute("60", UIWidgets.EditBox, "Offensive : chance maximale, sur 100 (Q1)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveChanceMax;

	[Attribute("24", UIWidgets.EditBox, "Offensive : le résultat est dit à chaque joueur à sa première apparition pendant … heures (Q1)", category: "SimpleRP - Front : ennemi")]
	int m_iOffensiveNewsHours;

	// --- Réglages : postes du front (F7 à F9, Q7) -----------------------------------------------------
	[Attribute("1", UIWidgets.CheckBox, "Postes du front (F7)", category: "SimpleRP - Front : postes")]
	bool m_bFrontPosts;

	[Attribute("25", UIWidgets.EditBox, "Part des carrés rouges du front tenue par un poste, sur 100 (F8)", category: "SimpleRP - Front : postes")]
	int m_iPostCellPercent;

	[Attribute("4", UIWidgets.EditBox, "Postes vivants en même temps au plus (Q7)", category: "SimpleRP - Front : postes")]
	int m_iPostMaxActive;

	[Attribute("1000", UIWidgets.EditBox, "Un poste se pose quand un joueur actif est à … mètres de son carré", category: "SimpleRP - Front : postes")]
	float m_fPostSpawnDistance;

	[Attribute("300", UIWidgets.EditBox, "Distance minimale aux joueurs à la pose, en mètres (toujours hors de vue)", category: "SimpleRP - Front : postes")]
	float m_fPostPlayerMinDistance;

	[Attribute("1500", UIWidgets.EditBox, "Retrait discret quand plus aucun joueur n'est à … mètres", category: "SimpleRP - Front : postes")]
	float m_fPostDespawnDistance;

	[Attribute("5", UIWidgets.EditBox, "… pendant … minutes", category: "SimpleRP - Front : postes")]
	int m_iPostDespawnMinutes;

	[Attribute("6", UIWidgets.EditBox, "Un poste anéanti ne revient qu'après … heures, ou au redémarrage (F9)", category: "SimpleRP - Front : postes")]
	int m_iPostRespawnHours;

	[Attribute("4", UIWidgets.EditBox, "Soldats d'un poste (MA11 : 4 partout)", category: "SimpleRP - Front : postes")]
	int m_iPostSoldiersKey;

	[Attribute("2", UIWidgets.EditBox, "Soldats d'un petit poste (Commandeur gelé : hauteur, piste)", category: "SimpleRP - Front : postes")]
	int m_iPostSoldiersMinor;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefabs des postes de 4 (vide = tirage par taille)", "et", category: "SimpleRP - Front : postes")]
	ref array<ResourceName> m_aPostKeyPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefabs des postes de 2 (vide = sentinelles)", "et", category: "SimpleRP - Front : postes")]
	ref array<ResourceName> m_aPostMinorPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Décor retranché des postes (vide = aucun)", "et", category: "SimpleRP - Front : postes")]
	ref array<ResourceName> m_aPostFortPrefabs;

	[Attribute("15", UIWidgets.EditBox, "Rayon du point Defend d'un poste, en mètres", category: "SimpleRP - Front : postes")]
	float m_fPostDefendRadius;

	[Attribute("25", UIWidgets.EditBox, "Maintien de position d'un poste, en mètres", category: "SimpleRP - Front : postes")]
	float m_fPostHoldRadius;

	[Attribute("3", UIWidgets.EditBox, "Largeur de route minimale prise en compte, en mètres", category: "SimpleRP - Front : postes")]
	float m_fPostRoadMinWidth;

	[Attribute("6", UIWidgets.EditBox, "Largeur à partir de laquelle une route est « principale », en mètres", category: "SimpleRP - Front : postes")]
	float m_fMainRoadWidth;

	[Attribute("15", UIWidgets.EditBox, "Mètres au-dessus des carrés voisins pour qu'un carré compte comme une hauteur", category: "SimpleRP - Front : postes")]
	float m_fHeightProminence;

	// --- Réglages : patrouilles de fond (F10) --------------------------------------------------------
	[Attribute("15", UIWidgets.EditBox, "Chance, sur 100, d'une patrouille côté bleu près du rouge (F10)", category: "SimpleRP - Front : patrouilles")]
	int m_iInfiltrationChance;

	[Attribute("2", UIWidgets.EditBox, "Carrés au plus entre une infiltration et le rouge (F10)", category: "SimpleRP - Front : patrouilles")]
	int m_iInfiltrationDepthCells;

	// --- Réglages : contre-attaques (F1 à F6, H4) ------------------------------------------------------
	[Attribute("4", UIWidgets.EditBox, "Joueurs connectés minimum pour qu'une contre-attaque parte (H4)", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackMinPlayers;

	[Attribute("180", UIWidgets.EditBox, "Délai entre deux contre-attaques à menace 0, en minutes (F6)", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackMinutesThreat0;

	[Attribute("60", UIWidgets.EditBox, "Délai entre deux contre-attaques à menace 10, en minutes (F6)", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackMinutesThreat10;

	[Attribute("20", UIWidgets.EditBox, "Aléa du délai, en plus ou en moins, sur 100", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackJitterPercent;

	[Attribute("1", UIWidgets.CheckBox, "L'horloge des contre-attaques n'avance qu'à m_iAttackMinPlayers connectés (H4)", category: "SimpleRP - Front : contre-attaques")]
	bool m_bAttackClockNeedsQuorum;

	[Attribute("15", UIWidgets.EditBox, "Annonce avant l'assaut, en minutes", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackWarningMinutes;

	[Attribute("5", UIWidgets.EditBox, "Intervalle entre deux vagues, en minutes", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackWaveMinutes;

	[Attribute("3", UIWidgets.EditBox, "Vagues prévues (clé front_attaque_vagues, UNE clé : Commandeur gelé ou actif ; actif, une de moins si la région est désorganisée, puis révisée de 2 à 4, CA3)", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackMaxWaves;

	[Attribute("60", UIWidgets.EditBox, "Échelonnement des groupes d'une vague : minimum, en secondes", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackStaggerMinSeconds;

	[Attribute("90", UIWidgets.EditBox, "Échelonnement des groupes d'une vague : maximum, en secondes", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackStaggerMaxSeconds;

	[Attribute("70", UIWidgets.EditBox, "Chance, sur 100, de viser la dernière zone prise encore au contact (F2)", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackLastTakenPercent;

	[Attribute("2", UIWidgets.EditBox, "Axes d'arrivée des vagues", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackAxes;

	[Attribute("10", UIWidgets.EditBox, "Commandeur gelé : la dernière vague est réputée arrivée au bout de … minutes", category: "SimpleRP - Front : contre-attaques")]
	int m_iWaveArrivalMaxMinutes;

	[Attribute("60", UIWidgets.EditBox, "Attaque enlisée : l'ennemi rompt le contact après … minutes d'assaut (C8)", category: "SimpleRP - Front : contre-attaques")]
	int m_iAttackMaxMinutes;

	[Attribute("10", UIWidgets.EditBox, "Une poche rouge plus petite (en carrés) ne sert ni d'origine ni de contact (F1)", category: "SimpleRP - Front : contre-attaques")]
	int m_iPocketMinCells;

	// --- Constantes -----------------------------------------------------------------------------------
	static const int WAVE_GROUP_SOLDIERS = 6;			// soldats comptés par élément de vague (place COMBAT, papier)
	static const int WANTED_GARRISONS_MAX = 3;			// localités que F11 veut garnir en même temps (C6)
	static const float WANTED_REACH_M = 2500;			// une localité non posée n'est voulue qu'avec un joueur à cette distance
	static const int SCORE_BUDGET = 40;				// carrés notés au plus par passage (hauteurs, routes)
	static const int OBJECTIVE_HOLD_S = 180;			// AdvanceAssault : objectif revu au bout de … secondes
	static const float ORIGIN_PLAYER_MIN_M = 500;		// origine d'une vague : à … mètres au moins de tout joueur
	static const float ORIGIN_FALLBACK_MAX_M = 1500;	// origine de repli : carré rouge à … mètres au plus de la zone
	static const float FORT_CLEAR_M = 300;				// un décor orphelin n'est retiré qu'à … mètres de tout joueur

	// --- État (serveur, sauf s_Instance) ------------------------------------------------------------
	protected static SRP_FrontEnemyComponent s_Instance;
	protected ref SRP_Commander m_Commander;							// le cerveau (#76), null si absent
	protected ref map<int, ref SRP_FrontCellInfo> m_mCellInfo = new map<int, ref SRP_FrontCellInfo>();
	protected ref array<int> m_aFrontRed = {};							// carrés rouges du front
	protected ref array<int> m_aMainRed = {};							// par carré : 1 = masse rouge principale
	protected ref array<int> m_aPostCells = {};						// carrés choisis pour un poste
	protected ref map<int, ref SRP_FrontPost> m_mPosts = new map<int, ref SRP_FrontPost>();
	protected ref map<int, int> m_mPostDestroyedUnix = new map<int, int>();
	protected ref SRP_CounterAttack m_Attack;							// null sans attaque
	protected int m_iAttackClockSec;									// secondes d'horloge F6 écoulées
	protected int m_iAttackDueSec;										// échéance D de l'horloge
	protected int m_iLastClockUnix;
	protected int m_iWindowTarget = -1;								// cible choisie à l'ouverture de la fenêtre CA1
	protected int m_iFrontRev = -1;									// version d'état du socle vue au dernier RebuildFront
	protected string m_sOffDay;										// jour de la dernière offensive (sauvegardé)
	protected string m_sOffText;										// texte du résultat (sauvegardé)
	protected int m_iOffUnix;											// heure du résultat (sauvegardée)
	protected int m_iOffZone = -1;										// zone visée (sauvegardée)
	protected int m_iEmptySince;										// heure Unix du départ du dernier joueur
	protected ref array<string> m_aNewsSeen = {};						// identités des joueurs déjà prévenus du résultat (sauvées, fe_offSeen)

	protected bool m_bServerStarted;									// serveur hors éditeur : Tick, Commandeur, abonnement
	protected bool m_bSelectPending;									// des carrés du front restent à noter (budget)
	protected bool m_bPostPossible;									// réserve des postes (IsPostPossibleNearPlayers), revue à chaque passage
	protected ref map<int, int> m_mPostDeferUnix = new map<int, int>();	// pose d'un poste repoussée (heure Unix)
	protected ref array<IEntity> m_aOrphanForts = {};					// décors de postes partis, retirés hors de vue
	protected bool m_bWindowOpen;										// CA1 : fenêtre ouverte
	protected int m_iWindowNextSec;									// CA1 : prochaine décision (horloge en secondes)
	protected int m_iWavePending;										// éléments de la vague encore échelonnés (CallLater)
	protected int m_iAttackSerial;										// numéro de l'attaque : un élément échelonné d'une autre attaque est ignoré
	protected bool m_bQuietEnd;										// fin d'attaque sans radio (remise à plat, restauration)
	protected int m_iOffChance = -1;									// chance de la dernière offensive (mémoire seule)
	protected bool m_bOffSuccess;										// résultat de la dernière offensive (sauvé, fe_offWon)
	protected string m_sOffResult;										// résultat en clair pour le Staff (mémoire seule)
	protected int m_iLastRunZone = -1;									// zone du dernier RunOffensive, -1 sans cible
	protected bool m_bLastInfiltration;								// F10 : dernier AllowAmbientFor rendu au titre d'une infiltration

	//================================================================================================
	// Accès, cycle de vie, Commandeur
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! L'instance (toutes machines) — socle, chute, territoire, missions, Commandeur
	static SRP_FrontEnemyComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes machines : s_Instance. Serveur : m_Commander = new SRP_Commander(this) (il DÉCLARE ses réglages, ne
	//! trace rien), DeclareSettings, GetOnPlayerSpawned().Insert(OnPlayerSpawnedNews), CallLater(Tick, 5000, true)
	//! — le moteur
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		// Ni sur un client, ni au World Editor : le Commandeur et la boucle ne vivent que sur le serveur en partie
		if (!Replication.IsServer() || SCR_Global.IsEditMode(owner))
			return;
		m_bServerStarted = true;
		m_Commander = new SRP_Commander(this);
		DeclareSettings();
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.GetOnPlayerSpawned().Insert(OnPlayerSpawnedNews);
		// Le Tick ne fait rien tant que le front n'est pas prêt (SRP_FrontComponent.Start à 5 s)
		GetGame().GetCallqueue().CallLater(Tick, 5000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : d'abord SRP_FrontComponent front = SRP_FrontComponent.GetInstance(); if (front)
	//! front.SaveFinalFromEnemy(); (écriture complète de front.json, chaîne WriteTo, tant que ce composant et le
	//! Commandeur existent : l'ordre des OnDelete n'est pas garanti) ; puis Remove(Tick), désabonnement,
	//! if (m_Commander) m_Commander.Stop() ; enfin s_Instance = null — le moteur
	override void OnDelete(IEntity owner)
	{
		if (m_bServerStarted)
		{
			// Sauvegarde finale complète AVANT l'arrêt du Commandeur : l'ordre des OnDelete n'est pas garanti
			SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
			if (front)
				front.SaveFinalFromEnemy();
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(SendWaveGroup);
			SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
			if (gameMode)
				gameMode.GetOnPlayerSpawned().Remove(OnPlayerSpawnedNews);
			if (m_Commander)
				m_Commander.Stop();
		}
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : super, puis m_Commander.OnControllableDestroyed (pertes, officiers) — le moteur
	//! (SCR_BaseGameModeComponent.c:157)
	override void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnControllableDestroyed(instigatorContextData);
		if (m_Commander)
			m_Commander.OnControllableDestroyed(instigatorContextData);
	}

	//------------------------------------------------------------------------------------------------
	//! Le Commandeur (null s'il est absent) — Staff, SRP_Commander.Get
	SRP_Commander GetCommander()
	{
		return m_Commander;
	}

	//------------------------------------------------------------------------------------------------
	//! SEULE définition de « Commandeur actif » : m_Commander && m_Commander.IsStarted() && !m_Commander.IsFrozen()
	//! (VU6) ; faux : les règles votées du front seules (camion d'alerte #69, AdvanceAssault, vague réputée arrivée…).
	//! Appelé par ce composant (garde de TOUS les crochets de contre-attaque), territoire (GarrisonTrucksTick : camion
	//! d'entrée par RequestEntryTruck, pas de camion d'alerte), camions, SRP_CmdManeuvers.Tick. (L'hélico par tirage,
	//! lui, s'arrête dès que le Commandeur EXISTE : SRP_Commander.Get().)
	bool IsCommanderActive()
	{
		if (!m_Commander)
			return false;
		return m_Commander.IsStarted() && !m_Commander.IsFrozen();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, toutes les 5 s, en heure Unix : si le front est prêt et !m_Commander.IsStarted() -> Start ;
	//! RebuildFront si la version d'état a changé ; PostsTick, AttackClock, AttackTick, m_Commander.Tick(now) si
	//! démarré, OffensiveTick ; SRP_CmdCapacity.Get().SetWantedGarrisons à chaque passage — CallLater
	protected void Tick()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;
		int nowUnix = System.GetUnixTime();
		if (m_Commander && !m_Commander.IsStarted())
			m_Commander.Start();

		int version = front.GetStateVersion();
		if (version != m_iFrontRev)
		{
			m_iFrontRev = version;
			RebuildFront();
		}
		else if (m_bSelectPending)
			SelectPostCells();

		PostsTick(nowUnix);
		AttackClock(nowUnix);
		AttackTick(nowUnix);
		if (m_Commander && m_Commander.IsStarted())
			m_Commander.Tick(nowUnix);
		OffensiveTick(nowUnix);
		WantedGarrisonsTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : Declare de ses clés front_* (contre-attaques, postes, offensive, regarnissage, infiltration) —
	//! OnPostInit ; puis SRP_FrontComponent.Start fait le Reload
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("front_attaque_minutes_menace0", m_iAttackMinutesThreat0, "Délai entre deux contre-attaques à menace 0, en minutes (F6)");
		SRP_CmdSettings.DeclareInt("front_attaque_minutes_menace10", m_iAttackMinutesThreat10, "Délai entre deux contre-attaques à menace 10, en minutes (F6)");
		SRP_CmdSettings.DeclareInt("front_attaque_alea", m_iAttackJitterPercent, "Aléa du délai entre deux contre-attaques, en plus ou en moins, sur 100 (F6)");
		SRP_CmdSettings.DeclareInt("front_attaque_joueurs", m_iAttackMinPlayers, "Joueurs connectés au moins pour que l'horloge des contre-attaques avance (H4)");
		SRP_CmdSettings.DeclareInt("front_attaque_annonce_minutes", m_iAttackWarningMinutes, "Annonce d'une contre-attaque avant l'assaut, en minutes (F6)");
		SRP_CmdSettings.DeclareInt("front_attaque_vague_minutes", m_iAttackWaveMinutes, "Intervalle entre deux vagues, en minutes (F6)");
		SRP_CmdSettings.DeclareInt("front_attaque_vagues", m_iAttackMaxWaves, "Vagues prévues par contre-attaque (Commandeur gelé ou actif ; actif : révisées à la 2e vague, CA3)");
		SRP_CmdSettings.DeclareInt("front_attaque_duree_max_minutes", m_iAttackMaxMinutes, "L'ennemi rompt le contact après ce nombre de minutes d'assaut (C8)");
		SRP_CmdSettings.DeclareInt("front_attaque_derniere_prise", m_iAttackLastTakenPercent, "Chance, sur 100, que la contre-attaque vise la dernière zone prise (F2)");
		SRP_CmdSettings.DeclareInt("front_postes_max", m_iPostMaxActive, "Postes du front vivants en même temps, au plus (Q7)");
		SRP_CmdSettings.DeclareInt("front_postes_part", m_iPostCellPercent, "Part des carrés rouges du front tenus par un poste, sur 100 (F8)");
		SRP_CmdSettings.DeclareInt("front_poste_retour_heures", m_iPostRespawnHours, "Un poste anéanti ne revient qu'après ce nombre d'heures, ou au redémarrage (F9)");
		SRP_CmdSettings.DeclareBool("front_offensive", m_bFictiveOffensive, "Offensive de la nuit, une fois par jour (H2) : 1 oui, 0 non");
		SRP_CmdSettings.DeclareInt("front_offensive_base", m_iOffensiveChanceBase, "Offensive de la nuit : chance de réussite de base, sur 100 (Q1)");
		SRP_CmdSettings.DeclareInt("front_offensive_par_menace", m_iOffensiveChancePerThreat, "Offensive de la nuit : chance ajoutée par point de menace, sur 100 (Q1)");
		SRP_CmdSettings.DeclareInt("front_offensive_max", m_iOffensiveChanceMax, "Offensive de la nuit : chance maximale, sur 100 (Q1)");
		SRP_CmdSettings.DeclareInt("front_offensive_heure_debut", m_iOffensiveHourStart, "Offensive de la nuit : début de la fenêtre, heure du serveur (Q1)");
		SRP_CmdSettings.DeclareInt("front_offensive_heure_fin", m_iOffensiveHourEnd, "Offensive de la nuit : fin de la fenêtre, exclue, heure du serveur (Q1)");
		SRP_CmdSettings.DeclareInt("front_offensive_vide_minutes", m_iOffensiveEmptyMinutes, "Offensive de la nuit : serveur vide depuis ce nombre de minutes (Q1)");
		SRP_CmdSettings.DeclareFloat("front_regarnissage_heures", m_fLossRefillHours, "Heures réelles pour qu'une localité soit de nouveau pleine, un quart par heure (F12)");
		SRP_CmdSettings.DeclareInt("front_infiltration_chance", m_iInfiltrationChance, "Chance, sur 100, d'une patrouille de fond côté bleu près du rouge (F10)");
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : relit ses clés front_*, puis m_Commander.LoadSettings() — SRP_FrontComponent (Start, ReloadSettings)
	void LoadSettings()
	{
		m_iAttackMinutesThreat0 = SRP_CmdSettings.GetIntClamped("front_attaque_minutes_menace0", 5, 1440);
		m_iAttackMinutesThreat10 = SRP_CmdSettings.GetIntClamped("front_attaque_minutes_menace10", 5, 1440);
		m_iAttackJitterPercent = SRP_CmdSettings.GetIntClamped("front_attaque_alea", 0, 90);
		m_iAttackMinPlayers = SRP_CmdSettings.GetIntClamped("front_attaque_joueurs", 1, 128);
		m_iAttackWarningMinutes = SRP_CmdSettings.GetIntClamped("front_attaque_annonce_minutes", 0, 120);
		m_iAttackWaveMinutes = SRP_CmdSettings.GetIntClamped("front_attaque_vague_minutes", 1, 60);
		m_iAttackMaxWaves = SRP_CmdSettings.GetIntClamped("front_attaque_vagues", 1, 10);
		m_iAttackMaxMinutes = SRP_CmdSettings.GetIntClamped("front_attaque_duree_max_minutes", 5, 600);
		m_iAttackLastTakenPercent = SRP_CmdSettings.GetIntClamped("front_attaque_derniere_prise", 0, 100);
		m_iPostMaxActive = SRP_CmdSettings.GetIntClamped("front_postes_max", 0, 20);
		m_iPostCellPercent = SRP_CmdSettings.GetIntClamped("front_postes_part", 0, 100);
		m_iPostRespawnHours = SRP_CmdSettings.GetIntClamped("front_poste_retour_heures", 0, 168);
		m_bFictiveOffensive = SRP_CmdSettings.GetBool("front_offensive");
		m_iOffensiveChanceBase = SRP_CmdSettings.GetIntClamped("front_offensive_base", 0, 100);
		m_iOffensiveChancePerThreat = SRP_CmdSettings.GetIntClamped("front_offensive_par_menace", 0, 100);
		m_iOffensiveChanceMax = SRP_CmdSettings.GetIntClamped("front_offensive_max", 0, 100);
		m_iOffensiveHourStart = SRP_CmdSettings.GetIntClamped("front_offensive_heure_debut", 0, 23);
		m_iOffensiveHourEnd = SRP_CmdSettings.GetIntClamped("front_offensive_heure_fin", 0, 24);
		m_iOffensiveEmptyMinutes = SRP_CmdSettings.GetIntClamped("front_offensive_vide_minutes", 0, 600);
		m_fLossRefillHours = SRP_CmdSettings.GetFloatClamped("front_regarnissage_heures", 0.1, 168);
		m_iInfiltrationChance = SRP_CmdSettings.GetIntClamped("front_infiltration_chance", 0, 100);
		// La part des postes a pu changer : carrés du front et postes revus au prochain passage
		m_iFrontRev = -1;
		if (m_Commander)
			m_Commander.LoadSettings();
	}

	//================================================================================================
	// Contrats NEUTRES appelés par le front (relais vers le Commandeur ; ne font rien sans lui)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Une zone est tombée (D1) ou forcée bleue (staff) : ClearLosses/ClearSent/bonus/vidée de ses localités, postes
	//! de la zone retirés, attaque visant la zone close, puis m_Commander.OnZoneCaptured(zone, staff) (Q9 : rien de
	//! visible si staff) — SRP_ZoneFall (3e appel)
	void OnZoneCaptured(int zone, bool staff)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
		{
			array<int> localities = {};
			front.GetZoneLocalities(zone, localities);
			foreach (int index : localities)
			{
				SRP_FrontLocality loc = front.GetLocality(index);
				if (!loc)
					continue;
				ResetLocalityMemory(loc);
				// Zone forcée par le Staff : ses localités n'ont pas eu d'OnLocalityTaken, on les remet d'accord ici
				SyncLocality(loc.m_sName, "zone prise");
			}
			front.MarkDirty(false);
			RetirePostsOfZone(front, zone);
		}
		if (m_Attack && m_Attack.m_iZone == zone)
		{
			m_bQuietEnd = staff;
			EndAttack(SRP_EAttackEnd.ANNULEE);
			m_bQuietEnd = false;
		}
		if (m_Commander)
			m_Commander.OnZoneCaptured(zone, staff);
	}

	//------------------------------------------------------------------------------------------------
	//! Une zone est repassée rouge (Q2, E4, H2, Staff) : ses localités redeviennent hostiles (SRP_SectorState.m_iOwner
	//! = ENNEMI) ; si m_Attack vise cette zone : EndAttack(SRP_EAttackEnd.PERDUE) (SEUL endroit qui fait
	//! AdoptAssailants et remet m_iAssaultZone à -1) ; puis m_Commander.OnZoneLost(zone, reason, staff) — SRP_ZoneFall
	void OnZoneLost(int zone, int reason, bool staff)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (front && territory)
		{
			array<int> localities = {};
			front.GetZoneLocalities(zone, localities);
			foreach (int index : localities)
			{
				SRP_FrontLocality loc = front.GetLocality(index);
				if (loc)
					territory.SetLocalityOwner(loc.m_sName, SRP_ESectorOwner.ENNEMI);
			}
		}
		// Les localités d'abord : les assaillants deviennent la garnison d'une localité de nouveau hostile
		if (m_Attack && m_Attack.m_iZone == zone)
			EndAttack(SRP_EAttackEnd.PERDUE);
		if (m_Commander)
			m_Commander.OnZoneLost(zone, reason, staff);
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les points clés de la localité sont pris : m_iOwner = NOUS, WithdrawLocality (repli, sirène muette,
	//! camions rappelés), pertes à zéro, puis m_Commander.OnLocalityTaken(locality) — SRP_ZoneFall
	void OnLocalityTaken(string locality)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
			territory.SetLocalityOwner(locality, SRP_ESectorOwner.NOUS);
		WithdrawLocality(locality, "localité prise");
		ClearLosses(locality);
		if (m_Commander)
			m_Commander.OnLocalityTaken(locality);
	}

	//------------------------------------------------------------------------------------------------
	//! D4 réussie, AVANT le changement de camp : SRP_CmdIntel.OnKeyPointSeized(zone) (renseignement, dépôt révélé ;
	//! un 2e appel rafraîchit l'heure) — SRP_ZoneFall.Seize
	void OnKeyPointSeized(int zone, int keyPoint, int playerId)
	{
		if (m_Commander)
			SRP_CmdIntel.OnKeyPointSeized(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Mission réussie, UNE entrée par fait (trou 14) : SRP_Commander.OnMissionSucceeded répartit (renseignement,
	//! dépôt révélé ECOUTE/DOCUMENTS, quart de stock SABOTAGE/CACHE, alerte) — SRP_MissionManagerComponent.End(SUCCES)
	void OnMissionSucceeded(int missionType, vector site, string missionId, string targetPrefab, string author)
	{
		if (m_Commander)
			m_Commander.OnMissionSucceeded(missionType, site, missionId, targetPrefab, author);
	}

	//------------------------------------------------------------------------------------------------
	//! En tête de OnObjectiveUsed : l'objet utilisé est-il le dépôt ennemi ou une pièce d'appui du Commandeur ? Vrai =
	//! consommé (message dans reply) — SRP_MissionManagerComponent.OnObjectiveUsed SEUL (SRP_SabotageAction passe déjà
	//! par OnObjectiveUsed, SRP_Missions.c:3143 : une seule entrée par « Saboter »)
	bool OnSabotageUsed(IEntity target, int playerId, out string reply)
	{
		reply = "";
		if (!m_Commander)
			return false;
		return m_Commander.OnSabotageUsed(target, playerId, reply);
	}

	//------------------------------------------------------------------------------------------------
	//! La menace de l'île a changé : relais aux ressources du Commandeur (OnThreatChanged) — SRP_FrontComponent.AddThreat
	void OnThreatChanged(int threat)
	{
		if (m_Commander)
			m_Commander.OnThreatChanged(threat);
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à plat d'une zone (SRP_EFrontReset.ZONE SEULEMENT) : attaque close si elle la vise, localités de la zone
	//! sans pertes — SRP_FrontComponent.NotifyReset (CAMPAGNE -> ResetAll, RESTAURATION -> ReadFrom puis OnRestored)
	void OnFrontReset(int kind, int zone)
	{
		if (kind != SRP_EFrontReset.ZONE)
			return;
		// Le Commandeur recopie les carrés de cette zone : la remise à plat du Staff n'est pas un compte rendu (#76)
		if (m_Commander)
			m_Commander.OnZoneReset(zone);
		if (m_Attack && m_Attack.m_iZone == zone)
		{
			m_bQuietEnd = true;
			EndAttack(SRP_EAttackEnd.ANNULEE);
			m_bQuietEnd = false;
		}
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		array<int> localities = {};
		front.GetZoneLocalities(zone, localities);
		foreach (int index : localities)
		{
			SRP_FrontLocality loc = front.GetLocality(index);
			if (!loc)
				continue;
			ResetLocalityMemory(loc);
			SyncLocality(loc.m_sName, "zone remise à plat");
		}
		front.MarkDirty(false);
		// Carrés du front et postes revus au prochain passage
		m_iFrontRev = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne (B3, K1, Staff) : attaque, postes et horloges à zéro, offensive oubliée, puis
	//! m_Commander.ResetCampaign(reason) — SRP_FrontComponent.ResetCampaign
	void ResetAll(string reason)
	{
		if (m_Attack)
		{
			m_bQuietEnd = true;
			EndAttack(SRP_EAttackEnd.ANNULEE);
			m_bQuietEnd = false;
		}
		RetireAllPosts();
		m_mPostDestroyedUnix.Clear();
		m_mPostDeferUnix.Clear();
		m_aPostCells.Clear();
		m_aFrontRed.Clear();
		m_aMainRed.Clear();
		m_iFrontRev = -1;
		m_bSelectPending = false;
		ResetAttackClock();
		m_iLastClockUnix = 0;
		m_sOffDay = "";
		m_sOffText = "";
		m_iOffUnix = 0;
		m_iOffZone = -1;
		m_iOffChance = -1;
		m_bOffSuccess = false;
		m_sOffResult = "";
		m_aNewsSeen.Clear();

		// Mémoire des localités (F12, MA2, MA6, MA7) à zéro
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
		{
			int count = front.GetLocalityCount();
			for (int i = 0; i < count; i++)
			{
				SRP_FrontLocality loc = front.GetLocality(i);
				if (loc)
				{
					ResetLocalityMemory(loc);
					loc.m_iFullUntil = 0;
				}
			}
			front.MarkDirty(false);
		}
		// Garnisons, poteaux et camions (territoire), puis propriétaires des localités d'après la nouvelle carte
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
			territory.ResetAll(reason);
		SyncAllLocalities("nouvelle campagne");
		JournalLine("Ennemi du front remis à zéro : " + reason);
		if (m_Commander)
			m_Commander.ResetCampaign(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Après RestoreDaily (ReadFrom déjà fait) : attaque close, postes recalculés, nouvelle d'une offensive réussie
	//! arrêtée si la zone n'est plus rouge, puis m_Commander.OnRestored() — SRP_FrontComponent.RestoreDaily
	void OnRestored()
	{
		if (m_Attack)
		{
			m_bQuietEnd = true;
			EndAttack(SRP_EAttackEnd.ANNULEE);
			m_bQuietEnd = false;
		}
		RetireAllPosts();
		m_mPostDeferUnix.Clear();
		m_iFrontRev = -1;
		m_iOffChance = -1;
		m_sOffResult = "";
		// Les localités suivent la carte restaurée (prises ou hostiles), les défenseurs d'une localité prise se retirent
		SyncAllLocalities("restauration");
		// Offensive réussie dont la copie ne garde pas la perte (copie d'avant la nuit, gardée par ReadFrom) : la zone
		// n'est plus rouge sur la carte, la nouvelle « zone tombée » n'est plus dite aux joueurs qui apparaissent
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && m_bOffSuccess && m_iOffZone > 0 && m_iOffUnix > 0 && front.GetZoneOwner(m_iOffZone) != SRP_EFrontOwner.ROUGE)
		{
			m_iOffUnix = 0;
			m_sOffResult = "réussie, mais la zone n'est plus rouge après la restauration : nouvelle aux joueurs arrêtée";
			JournalLine(string.Format("Offensive de la nuit sur %1 : zone de nouveau à nous après la restauration, nouvelle aux joueurs arrêtée", front.GetZoneLabel(m_iOffZone)));
			front.MarkDirty(false);
		}
		if (m_Commander)
			m_Commander.OnRestored();
	}

	//------------------------------------------------------------------------------------------------
	//! Clés fe_offDay, fe_offText, fe_offUnix, fe_offZone (+ fe_offSeen : joueurs déjà prévenus, Q1 ; + fe_offWon :
	//! réussite, pour OnRestored), puis m_Commander.WriteTo(ctx) (cmd_*) — SRP_FrontComponent.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		ctx.WriteValue("fe_offDay", m_sOffDay);
		ctx.WriteValue("fe_offText", m_sOffText);
		ctx.WriteValue("fe_offUnix", m_iOffUnix);
		ctx.WriteValue("fe_offWon", m_bOffSuccess);
		// La zone est écrite par son CODE (« S07 ») : les zones sont relues par code
		string zoneCode = "";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && m_iOffZone >= 0)
			zoneCode = front.GetZoneCode(m_iOffZone);
		ctx.WriteValue("fe_offZone", zoneCode);
		// Joueurs déjà prévenus, par identité (une reconnexion ou un redémarrage ne redit pas la nouvelle)
		string seen = "";
		foreach (int seenIndex, string who : m_aNewsSeen)
		{
			if (seenIndex > 0)
				seen += ",";
			seen += who;
		}
		ctx.WriteValue("fe_offSeen", seen);
		if (m_Commander)
			m_Commander.WriteTo(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Relit les clés fe_*, puis m_Commander.ReadFrom(ctx) — SRP_FrontComponent.ReadFrom (après zones et localités)
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		// Restauration d'une copie (RestoreDaily) alors que l'offensive de cette nuit a déjà été jouée : elle reste notée
		// telle quelle (jour, zone, texte, réussite, joueurs prévenus) et ne se rejoue pas le même jour (Q1). Si la copie
		// remet à nous la zone perdue cette nuit, OnRestored arrête la nouvelle aux joueurs. Aucun effet au démarrage, où
		// m_sOffDay est vide
		bool keepTonight = !m_sOffDay.IsEmpty() && m_sOffDay == Today();
		if (!keepTonight)
		{
			// Lecteur tolérant : une clé absente laisse la valeur par défaut
			string day = "";
			string text = "";
			int unixTime = 0;
			string zoneCode = "";
			string seen = "";
			bool won = false;
			m_sOffDay = "";
			m_sOffText = "";
			m_iOffUnix = 0;
			m_iOffZone = -1;
			m_bOffSuccess = false;
			m_aNewsSeen.Clear();
			if (ctx.ReadValue("fe_offDay", day))
				m_sOffDay = day;
			if (ctx.ReadValue("fe_offText", text))
				m_sOffText = text;
			if (ctx.ReadValue("fe_offUnix", unixTime))
				m_iOffUnix = unixTime;
			if (ctx.ReadValue("fe_offWon", won))
				m_bOffSuccess = won;
			SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
			if (ctx.ReadValue("fe_offZone", zoneCode) && front && !zoneCode.IsEmpty())
				m_iOffZone = front.FindZone(zoneCode);
			if (ctx.ReadValue("fe_offSeen", seen) && !seen.IsEmpty())
			{
				array<string> seenList = {};
				seen.Split(",", seenList, true);
				foreach (string who : seenList)
				{
					if (!m_aNewsSeen.Contains(who))
						m_aNewsSeen.Insert(who);
				}
			}
		}
		m_iOffChance = -1;
		m_sOffResult = "";
		if (m_Commander)
			m_Commander.ReadFrom(ctx);
	}

	//================================================================================================
	// Garnisons et F12 (données sur SRP_FrontLocality, écrites par le socle dans front.json)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Pertes courantes : pertes x max(0, 1 - (maintenant - date) / (m_fLossRefillHours x 3600)) — territoire, Commandeur
	int CurrentLosses(string locality)
	{
		if (!m_bLossMemory)
			return 0;
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return 0;
		return Decayed(loc.m_iLosses, loc.m_iLossesAt, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Pertes = CurrentLosses + n, date = maintenant (n négatif accepté, RE13), MarkDirty(false) — territoire, Commandeur
	void AddLosses(string locality, int n)
	{
		if (!m_bLossMemory || n == 0)
			return;
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return;
		// La valeur est ramenée à son reste du moment : la date repart donc de maintenant
		loc.m_iLosses = Math.MaxInt(0, CurrentLosses(locality) + n);
		loc.m_iLossesAt = System.GetUnixTime();
		MarkFrontDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Pertes à zéro (localité prise, nouvelle campagne) — OnLocalityTaken, OnZoneCaptured
	void ClearLosses(string locality)
	{
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return;
		if (loc.m_iLosses == 0 && loc.m_iLossesAt == 0)
			return;
		loc.m_iLosses = 0;
		loc.m_iLossesAt = 0;
		MarkFrontDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats manquants d'une garnison posée : prévus - survivants (SRP_EnemyGarrison.CountPlanSurvivors) —
	//! territoire (avant chaque DeleteAll : AddLosses(nom, LiveLosses))
	int LiveLosses(SRP_SectorState state)
	{
		if (!state)
			return 0;
		int survivors = SRP_EnemyGarrison.CountPlanSurvivors(state);
		return Math.MaxInt(0, state.m_iPlannedSoldiers - survivors);
	}

	//------------------------------------------------------------------------------------------------
	//! Envoyés courants (MA2), même formule de regarnissage que les pertes — SRP_CmdManeuvers
	int CurrentSent(string locality)
	{
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return 0;
		return Decayed(loc.m_iSent, loc.m_iSentAt, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Envoyés + n, date = maintenant (débit AU DÉPART, une seule fois) — SRP_CmdManeuvers (OnTruckLaunched pour un
	//! camion hors colonne, NewColumn pour une colonne)
	void AddSent(string locality, int n)
	{
		if (n <= 0)
			return;
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return;
		loc.m_iSent = CurrentSent(locality) + n;
		loc.m_iSentAt = System.GetUnixTime();
		MarkFrontDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Envoyés - n, plancher 0 (soldats rendus vivants) — SRP_Enemy (RetireTick : SEULE porte de retour des soldats
	//! retirés vivants), SRP_CmdManeuvers (colonne annulée ; RE13 : colonnes et camions relus par ReadFrom)
	void ReturnSent(string locality, int n)
	{
		if (n <= 0)
			return;
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return;
		loc.m_iSent = Math.MaxInt(0, CurrentSent(locality) - n);
		loc.m_iSentAt = System.GetUnixTime();
		if (loc.m_iSent == 0)
			loc.m_iSentAt = 0;
		MarkFrontDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Envoyés à zéro — OnZoneCaptured, SRP_CmdManeuvers
	void ClearSent(string locality)
	{
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return;
		if (loc.m_iSent == 0 && loc.m_iSentAt == 0)
			return;
		loc.m_iSent = 0;
		loc.m_iSentAt = 0;
		MarkFrontDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Renforts reçus courants (même formule) — SRP_CmdManeuvers
	int CurrentBonus(string locality)
	{
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return 0;
		return Decayed(loc.m_iBonus, loc.m_iBonusAt, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Reçus + n, plafonné pour que Effective reste <= 60 — SRP_CmdManeuvers
	void AddBonus(string locality, int n)
	{
		if (n <= 0)
			return;
		SRP_FrontLocality loc = FindLoc(locality);
		if (!loc)
			return;
		int bonus = CurrentBonus(locality) + n;
		int ceiling = SRP_CmdCapacity.Get().GetGarrisonMax();
		int without = GetLocalityNominal(locality) - CurrentLosses(locality) - CurrentSent(locality);
		if (without + bonus > ceiling)
			bonus = Math.MaxInt(0, ceiling - without);
		loc.m_iBonus = bonus;
		loc.m_iBonusAt = System.GetUnixTime();
		MarkFrontDirty();
	}

	//------------------------------------------------------------------------------------------------
	//! Nominal = GarrisonSoldiers rendu public (base + joueurs + menace LOCALE, x1,25 si l'officier vit, 60 au plus)
	//! — Commandeur, capacité
	int GetLocalityNominal(string locality)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!territory || !enemies)
			return 0;
		SRP_SectorState state = territory.FindState(locality);
		if (!state || !state.m_Sector)
			return 0;
		int nominal = territory.GarrisonSoldiers(enemies, state.m_Sector);
		return Math.ClampInt(nominal, 0, SRP_CmdCapacity.Get().GetGarrisonMax());
	}

	//------------------------------------------------------------------------------------------------
	//! Effective = Nominal - pertes - envoyés + reçus, borné de 0 à 60 ; SEUL effectif « décidé » d'une pose : la pose
	//! vaut min(Effective, SRP_CmdCapacity.Get().GarrisonCap(nom, Effective)), appliqué par SRP_EnemyGarrison seul ;
	//! rien si SRP_CmdManeuvers.IsLocalityEmptied — territoire (pose F11), Commandeur, capacité
	int GetLocalityEffective(string locality)
	{
		int effective = GetLocalityNominal(locality) - CurrentLosses(locality) - CurrentSent(locality) + CurrentBonus(locality);
		return Math.ClampInt(effective, 0, SRP_CmdCapacity.Get().GetGarrisonMax());
	}

	//------------------------------------------------------------------------------------------------
	//! Dérivé pour le PC : effectif prévu et nominal de la localité principale de la zone ; faux sans localité —
	//! SRP_CmdScreens (VU2), SRP_FrontScreens
	bool GetZoneStrength(int zone, out int planned, out int nominal)
	{
		planned = 0;
		nominal = 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return false;
		string locality = MainLocalityName(front, zone);
		if (locality.IsEmpty())
			return false;
		nominal = GetLocalityNominal(locality);
		planned = GetLocalityEffective(locality);
		// Garnison posée : son effectif prévu à la pose
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory && territory.IsGarrisonPosted(locality))
		{
			SRP_SectorState state = territory.FindState(locality);
			if (state && state.m_iPlannedSoldiers > 0)
				planned = state.m_iPlannedSoldiers;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes de garnison posés dans la zone (Staff) — SRP_FrontScreens
	int GetZoneGarrisonGroups(int zone)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return 0;
		return territory.CountGarrisonGroupsInZone(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur valide dans le rayon + 150 m du point clé, ou groupe de la localité au contact depuis moins de 60 s —
	//! capacité (C6), garnison (classe COMBAT)
	bool IsLocalityUnderAttack(string locality)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return false;
		SRP_SectorState state = territory.FindState(locality);
		if (!state || !state.m_Sector)
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
		{
			SRP_FrontCapture capture = front.GetCapture();
			if (capture && capture.CountValidPlayersNear(state.m_Sector.GetCenter(), state.m_Sector.m_fRadius + 150) > 0)
				return true;
		}
		int tick = System.GetTickCount();
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (record && record.m_iSpotTick > 0 && tick - record.m_iSpotTick < 60000)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Garnison posée (vrai même vide par F12, trou 21) — SRP_ZoneFall (poste de commandement), Commandeur
	bool IsGarrisonPosted(string locality)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return false;
		return territory.IsGarrisonPosted(locality);
	}

	//------------------------------------------------------------------------------------------------
	//! Garnison posée ET installée (SRP_EnemyComponent.IsSettled) — SRP_ZoneFall (saisie D4)
	bool IsGarrisonSettled(string locality)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return false;
		return territory.IsGarrisonSettled(locality);
	}

	//------------------------------------------------------------------------------------------------
	//! Retrait des défenseurs d'une localité prise (SRP_Territory.c:1672-1700 : loin ou vu -> survivant RetireLater,
	//! sinon DeleteAll ; OnGarrisonGone ; sirène muette) — OnLocalityTaken
	void WithdrawLocality(string locality, string why)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
			territory.WithdrawLocality(locality, why);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone repassée rouge : les assaillants présents deviennent la garnison de la localité (ConvertToGarrison,
	//! m_iAssaultZone = -1, m_iPlannedSoldiers, sirène) — EndAttack(PERDUE) SEUL (appelée par OnZoneLost)
	void AdoptAssailants(string locality, array<ref SRP_EnemyGroup> groups)
	{
		if (!groups || groups.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_SectorState state = null;
		if (territory)
			state = territory.FindState(locality);
		if (!state)
		{
			// Localité introuvable : les assaillants se retirent hors de vue
			foreach (SRP_EnemyGroup lost : groups)
			{
				if (lost)
					lost.m_iAssaultZone = -1;
			}
			if (enemies)
				enemies.RetireLater(groups);
			return;
		}
		// Plus des assaillants : défenseurs de la localité (rôle du Commandeur effacé, place de garnison)
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record)
				continue;
			record.m_iAssaultZone = -1;
			record.m_iCmdRole = SRP_ECmdRole.AUCUN;
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.GARNISON);
		}
		territory.AdoptAssailants(state, groups);
		JournalLine(string.Format("%1 : %2 élément(s) d'assaut deviennent ses défenseurs", locality, groups.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Position dans une localité hostile dont la garnison est posée (rayon + margin) ; remplace
	//! IsInsideEnemySector — missions (G11 : IsInEnemyStronghold), Commandeur
	bool IsInGarrisonedLocality(vector position, float margin)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return false;
		return territory.IsInGarrisonedLocality(position, margin);
	}

	//------------------------------------------------------------------------------------------------
	//! F11 : la localité est dans une zone au contact (E1) — SRP_TerritoryComponent (choix de garnison)
	bool IsLocalityOnFront(SRP_SectorState state)
	{
		if (!state)
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return false;
		SRP_FrontLocality loc = FindLoc(state.m_sName);
		if (!loc || loc.m_iZone < 0)
			return false;
		return front.IsZoneInContact(loc.m_iZone);
	}

	//------------------------------------------------------------------------------------------------
	//! F11 : score de garnison = reach - bonus du front ; plus petit = prioritaire — SRP_TerritoryComponent
	float GarrisonScore(SRP_SectorState state, float reach)
	{
		float score = reach;
		if (!state)
			return score;
		if (IsLocalityOnFront(state))
			score -= m_fFrontPriorityBonus;
		// MA11 : la localité tenue au complet passe devant
		if (IsCommanderActive())
		{
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers && maneuvers.IsKeptFull(state.m_sName))
				score -= maneuvers.m_iFullScoreBonus;
		}
		return score;
	}

	//================================================================================================
	// Patrouilles de fond (F10) et postes (F7 à F9)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! F10 : une patrouille peut naître autour de ce joueur (carré rouge ; carré bleu près du rouge selon
	//! m_iInfiltrationChance, résultat gardé pour AcceptAmbientSpawn) — SRP_Enemy (Tick ambiant)
	bool AllowAmbientFor(IEntity player)
	{
		m_bLastInfiltration = false;
		if (!player)
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return true;	// front absent : la règle d'avant (partout)
		int cell = front.CellIndexAt(player.GetOrigin());
		if (cell < 0)
			return false;
		int owner = front.GetCellOwner(cell);
		if (owner == SRP_EFrontOwner.ROUGE)
			return true;
		if (owner != SRP_EFrontOwner.BLEU || front.IsBaseCell(cell))
			return false;
		if (!IsNearRed(front, cell, m_iInfiltrationDepthCells))
			return false;
		if (Math.RandomInt(0, 100) >= m_iInfiltrationChance)
			return false;
		m_bLastInfiltration = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! F10 : point de pose acceptable (rouge, ou bleu près du rouge en infiltration) — SRP_Enemy (SpawnPatrolNear,
	//! SpawnRoadPatrol). Le bleu n'est accepté que si l'appelant parle d'infiltration ET que le dernier
	//! AllowAmbientFor l'a accordée.
	bool AcceptAmbientSpawn(vector position, bool infiltration)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return true;
		int cell = front.CellIndexAt(position);
		if (cell < 0)
			return false;
		int owner = front.GetCellOwner(cell);
		if (owner == SRP_EFrontOwner.ROUGE)
			return true;
		if (!infiltration || !m_bLastInfiltration)
			return false;
		if (owner != SRP_EFrontOwner.BLEU || front.IsBaseCell(cell))
			return false;
		return IsNearRed(front, cell, m_iInfiltrationDepthCells);
	}

	//------------------------------------------------------------------------------------------------
	//! Un carré de poste est à portée d'un joueur actif (m_fPostSpawnDistance + 500) : la réserve des postes joue —
	//! SRP_CmdCapacity (IsPostReserveActive). Valeur revue à chaque passage de 5 s (PostsTick).
	bool IsPostPossibleNearPlayers()
	{
		return m_bPostPossible;
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré appartient à la masse rouge principale (MA12 : poches) — SRP_CmdManeuvers
	bool IsInMainRedMass(int cell)
	{
		if (cell < 0 || cell >= m_aMainRed.Count())
			return false;
		return m_aMainRed[cell] == 1;
	}

	//================================================================================================
	// Contre-attaques
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Une contre-attaque est en cours (annoncée ou en assaut) — Commandeur, capacité
	bool IsCounterAttackActive()
	{
		return m_Attack != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone visée par la contre-attaque en cours, -1 sinon — Commandeur (gravité, appuis), capacité
	int GetCounterAttackZone()
	{
		if (!m_Attack)
			return -1;
		return m_Attack.m_iZone;
	}

	//------------------------------------------------------------------------------------------------
	//! La contre-attaque en cours, null sinon — SRP_CmdManeuvers (crochets CA)
	SRP_CounterAttack GetCounterAttack()
	{
		return m_Attack;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte d'état construit par le serveur (trou 20) : « ATTAQUE annoncée, assaut dans 9 min », « vague 2 » ; "" sans
	//! attaque — SRP_FrontScreens (PC, Staff). L'ÉTAT d'attaque d'une zone se lit UNIQUEMENT sur le socle
	//! (SRP_FrontComponent.GetZoneAttackState, drapeaux posés par SetZoneFlag) : pas de seconde source ici
	string GetAttackStatusText(int zone)
	{
		if (!m_Attack || m_Attack.m_iZone != zone)
			return "";
		int nowUnix = System.GetUnixTime();
		if (m_Attack.m_iState == SRP_EFrontAttack.ANNONCEE)
		{
			int minutesLeft = Math.MaxInt(0, (m_Attack.m_iWarnEndUnix - nowUnix + 59) / 60);
			return string.Format("ATTAQUE annoncée, assaut dans %1 min", minutesLeft);
		}
		return string.Format("ATTAQUE en cours, vague %1", Math.MaxInt(m_Attack.m_iWaves, 1));
	}

	//================================================================================================
	// Staff et rapports (SRP_FrontScreens)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Contre-attaque forcée maintenant sur une zone à nous au front, non protégée (m_bForced) ; rend le compte rendu
	//! — SRP_FrontScreens (action territoire:attaque)
	string ForceAttack(int zone, string author)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!m_bServerStarted || !front || !front.IsReady())
			return "Front pas encore prêt : réessayez dans un instant.";
		if (front.IsFrozen())
			return "Front gelé : aucune contre-attaque possible.";
		if (m_Attack)
			return "Une contre-attaque est déjà en cours sur " + m_Attack.m_sZone + " : annulez-la d'abord.";
		if (zone <= 0 || zone >= front.GetZoneCount())
			return "Zone inconnue.";
		string label = front.GetZoneLabel(zone);
		if (front.GetZoneOwner(zone) != SRP_EFrontOwner.BLEU)
			return label + " n'est pas à nous : rien à contre-attaquer.";
		if (front.IsZoneProtected(zone))
			return label + " est protégée (base ou voisine de la base) : jamais contre-attaquée.";
		if (!front.IsZoneAtFront(zone))
			return label + " ne touche pas le rouge : pas au front.";
		if (!LaunchAttack(zone, true))
			return "Contre-attaque impossible sur " + label + ".";
		return string.Format("Contre-attaque forcée sur %1 : annonce faite, assaut dans %2 min.", label, m_iAttackWarningMinutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Annule la contre-attaque en cours (EndAttack ANNULEE, radio AttackCalledOff si annoncée) — SRP_FrontScreens
	//! (action territoire:attaque-annuler, 2 clics)
	string CancelAttack(string author)
	{
		if (!m_Attack)
			return "Aucune contre-attaque en cours.";
		string label = m_Attack.m_sZone;
		EndAttack(SRP_EAttackEnd.ANNULEE);
		return "Contre-attaque sur " + label + " annulée : les assaillants se retirent hors de vue.";
	}

	//------------------------------------------------------------------------------------------------
	//! Lance l'offensive de la nuit maintenant, réussite imposée ou échec (H2) — SRP_FrontScreens (actions
	//! territoire:offensive:1 et territoire:offensive:0, 2 clics)
	string ForceOffensive(bool success, string author)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!m_bServerStarted || !front || !front.IsReady())
			return "Front pas encore prêt : réessayez dans un instant.";
		if (front.IsFrozen())
			return "Front gelé : aucune offensive possible.";
		RunOffensive(true, success);
		if (m_iLastRunZone < 0)
			return "Offensive de la nuit : pas de zone du front à viser (zones au contact toutes protégées ou front sans contact).";
		string result = "échouée";
		if (m_bOffSuccess)
			result = "réussie, la zone est perdue";
		return string.Format("Offensive de la nuit lancée sur %1 : %2.", front.GetZoneLabel(m_iLastRunZone), result);
	}

	//------------------------------------------------------------------------------------------------
	//! Rapport de l'attaque en cours (zone, état, vagues, groupes, carrés repris, fin prévue) — SRP_FrontScreens
	//! (bloc « Ennemi du front » de la page Territoire)
	string GetAttackReport()
	{
		int nowUnix = System.GetUnixTime();
		if (!m_Attack)
		{
			array<int> ids = {};
			int connected = GetGame().GetPlayerManager().GetPlayers(ids);
			string idle = string.Format("Aucune contre-attaque en cours · horloge %1 / %2 min · %3 joueur(s) connecté(s), %4 requis", m_iAttackClockSec / 60, m_iAttackDueSec / 60, connected, m_iAttackMinPlayers);
			if (m_bWindowOpen)
			{
				SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
				string windowTarget = "aucune cible";
				if (front && m_iWindowTarget > 0)
					windowTarget = front.GetZoneLabel(m_iWindowTarget);
				idle += " · fenêtre du Commandeur ouverte, cible " + windowTarget;
			}
			return idle;
		}
		string forced = "";
		if (m_Attack.m_bForced)
			forced = " (forcée par le Staff)";
		if (m_Attack.m_iState == SRP_EFrontAttack.ANNONCEE)
		{
			int warnLeft = Math.MaxInt(0, (m_Attack.m_iWarnEndUnix - nowUnix + 59) / 60);
			return string.Format("Contre-attaque sur %1%2 : annoncée, assaut dans %3 min, %4 vague(s) prévue(s)", m_Attack.m_sZone, forced, warnLeft, m_Attack.m_iPlannedWaves);
		}
		int engaged = 0;
		foreach (SRP_EnemyGroup record : m_Attack.m_aGroups)
		{
			if (record && SRP_EnemyComponent.AliveAgents(record) > 0)
				engaged++;
		}
		int fit = SRP_EnemyComponent.ActiveAgentsNear(m_Attack.m_aGroups, vector.Zero, 0);
		int since = Math.MaxInt(0, (nowUnix - m_Attack.m_iAssaultStartUnix) / 60);
		int breakLeft = Math.MaxInt(0, m_iAttackMaxMinutes - since);
		string report = string.Format("Contre-attaque sur %1%2 : assaut depuis %3 min, vague %4 sur %5", m_Attack.m_sZone, forced, since, m_Attack.m_iWaves, m_Attack.m_iPlannedWaves);
		report += string.Format(" · %1 élément(s) engagé(s), %2 soldat(s) en état de combattre, %3 carré(s) repris · rupture dans %4 min", engaged, fit, m_Attack.m_iRetaken, breakLeft);
		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Postes du front autour d'un point (carré, genre, effectif, retour après anéantissement) — SRP_FrontScreens
	//! (page « postes », rayon 3000 m autour du Staff ; radius <= 0 : tous). Une ligne par poste, séparées par « \n ».
	string GetPostsReport(vector from, float radius)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "Front pas encore prêt.";
		int nowUnix = System.GetUnixTime();
		// Staff hors jeu : point de départ vector.Zero, aucune distance à écrire (elle serait mesurée depuis le coin de la carte)
		bool withDistance = from != vector.Zero;
		string report = string.Format("Postes du front : %1 en place sur %2 au plus, %3 carré(s) retenu(s) sur %4 carrés rouges au contact", m_mPosts.Count(), m_iPostMaxActive, m_aPostCells.Count(), m_aFrontRed.Count());
		if (!m_bFrontPosts)
			report += " (postes désactivés)";
		int shown = 0;
		for (int k = 0; k < m_mPosts.Count(); k++)
		{
			int cell = m_mPosts.GetKey(k);
			SRP_FrontPost post = m_mPosts.GetElement(k);
			if (!post)
				continue;
			float distance = vector.DistanceXZ(from, post.m_vPos);
			if (radius > 0 && distance > radius)
				continue;
			int fit = SRP_EnemyComponent.ActiveAgentsNear(post.m_aGroups, vector.Zero, 0);
			report += "\n" + string.Format("Poste carré %1 · %2 · %3 · %4 soldat(s) en état de combattre", front.CellRef(cell), post.m_sKind, ZoneLabel(front, front.GetCellZone(cell)), fit);
			if (withDistance)
			{
				int meters = Math.Round(distance);
				report += string.Format(" · à %1 m", meters);
			}
			shown++;
		}
		for (int d = 0; d < m_mPostDestroyedUnix.Count(); d++)
		{
			int deadCell = m_mPostDestroyedUnix.GetKey(d);
			int deadAt = m_mPostDestroyedUnix.GetElement(d);
			int left = m_iPostRespawnHours * 3600 - (nowUnix - deadAt);
			if (left <= 0)
				continue;
			float deadDistance = vector.DistanceXZ(from, front.CellCenter(deadCell));
			if (radius > 0 && deadDistance > radius)
				continue;
			report += "\n" + string.Format("Carré %1 · %2 · poste anéanti, retour possible dans %3 h %4 min", front.CellRef(deadCell), ZoneLabel(front, front.GetCellZone(deadCell)), left / 3600, (left - (left / 3600) * 3600) / 60);
			shown++;
		}
		if (shown == 0)
			report += "\nAucun poste dans ce rayon.";
		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Dernière offensive de la nuit (jour, zone, chance, résultat) et prochaine fenêtre — SRP_FrontScreens (bloc
	//! « Ennemi du front »)
	string GetNightReport()
	{
		if (!m_bFictiveOffensive)
			return "Offensive de la nuit : désactivée (front_offensive = 0)";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		string report;
		if (m_sOffDay.IsEmpty())
			report = "Offensive de la nuit : aucune jusqu'ici";
		else
		{
			// Nuit passée sans offensive (pas de cible, réserves trop basses) : ni zone ni heure (NoOffensiveTonight)
			string where = "aucune offensive lancée";
			if (front && m_iOffZone > 0)
				where = front.GetZoneLabel(m_iOffZone);
			else if (m_iOffUnix > 0)
				where = "zone introuvable";
			report = string.Format("Dernière offensive de la nuit : %1, %2", m_sOffDay, where);
			if (!m_sOffResult.IsEmpty())
				report += " — " + m_sOffResult;
			else if (!m_sOffText.IsEmpty())
				report += " — " + m_sOffText;
		}
		int nowUnix = System.GetUnixTime();
		int emptyMinutes = 0;
		if (m_iEmptySince > 0)
			emptyMinutes = (nowUnix - m_iEmptySince) / 60;
		array<int> ids = {};
		int connected = GetGame().GetPlayerManager().GetPlayers(ids);
		if (m_sOffDay == Today())
			report += string.Format(" · prochaine fenêtre : demain, de %1 h à %2 h", m_iOffensiveHourStart, m_iOffensiveHourEnd);
		else if (connected > 0)
			report += string.Format(" · prochaine fenêtre : de %1 h à %2 h, quand le serveur sera vide depuis %3 min", m_iOffensiveHourStart, m_iOffensiveHourEnd, m_iOffensiveEmptyMinutes);
		else
			report += string.Format(" · prochaine fenêtre : de %1 h à %2 h, serveur vide depuis %3 min sur %4", m_iOffensiveHourStart, m_iOffensiveHourEnd, emptyMinutes, m_iOffensiveEmptyMinutes);
		return report;
	}

	//================================================================================================
	// RPC du Commandeur (seule RPC de #76, modèle SRP_Sirene.c:504-515)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Serveur : Rpc(RpcDo_SRPCmdSound, …) puis appel local si !System.IsConsoleApp() — SRP_CmdSupport (départ du
	//! mortier, MO3)
	void BroadcastSound(string acp, string eventName, vector pos)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_SRPCmdSound, acp, eventName, pos);
		// Le Broadcast ne s'exécute pas sur l'hôte : un serveur avec joueur local joue le son lui-même
		if (!System.IsConsoleApp())
			SRP_CmdSupport.PlaySoundLocal(acp, eventName, pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : Rpc(RpcDo_SRPCmdLaunch, …) (relance d'obus en repli) — SRP_CmdSupport. Rien en local : l'obus du
	//! serveur est déjà parti.
	void BroadcastLaunch(RplId shell, vector dir)
	{
		if (!Replication.IsServer())
			return;
		Rpc(RpcDo_SRPCmdLaunch, shell, dir);
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque machine : SRP_CmdSupport.PlaySoundLocal(acp, eventName, pos) — Broadcast du serveur
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPCmdSound(string acp, string eventName, vector pos)
	{
		SRP_CmdSupport.PlaySoundLocal(acp, eventName, pos);
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque machine : SRP_CmdSupport.LaunchLocal(shell, dir) — Broadcast du serveur
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPCmdLaunch(RplId shell, vector dir)
	{
		SRP_CmdSupport.LaunchLocal(shell, dir);
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Front rouge (carrés rouges au contact du bleu), masse rouge principale (poches < m_iPocketMinCells exclues),
	//! puis SelectPostCells — Tick (quand la version d'état change)
	protected void RebuildFront()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		int count = front.GetCellCount();
		m_aFrontRed.Clear();
		if (m_aMainRed.Count() != count)
		{
			m_aMainRed.Clear();
			m_aMainRed.Resize(count);
		}
		for (int i = 0; i < count; i++)
			m_aMainRed[i] = 0;

		// Composantes rouges par les 4 côtés : 1 = masse principale, 2 = poche (moins de m_iPocketMinCells carrés)
		array<int> queue = {};
		for (int start = 0; start < count; start++)
		{
			if (m_aMainRed[start] != 0 || front.GetCellOwner(start) != SRP_EFrontOwner.ROUGE)
				continue;
			queue.Clear();
			queue.Insert(start);
			m_aMainRed[start] = 3;
			int head = 0;
			while (head < queue.Count())
			{
				int current = queue[head];
				head++;
				for (int dir = 0; dir < 4; dir++)
				{
					int neighbour = front.Neighbour(current, dir);
					if (neighbour < 0 || m_aMainRed[neighbour] != 0)
						continue;
					if (front.GetCellOwner(neighbour) != SRP_EFrontOwner.ROUGE)
						continue;
					m_aMainRed[neighbour] = 3;
					queue.Insert(neighbour);
				}
			}
			int flag = 2;
			if (queue.Count() >= m_iPocketMinCells)
				flag = 1;
			foreach (int member : queue)
				m_aMainRed[member] = flag;
		}

		// Front rouge : un carré rouge dont un côté touche du bleu
		for (int cell = 0; cell < count; cell++)
		{
			if (front.GetCellOwner(cell) == SRP_EFrontOwner.ROUGE && front.IsCellAtFront(cell))
				m_aFrontRed.Insert(cell);
		}
		SelectPostCells();
	}

	//------------------------------------------------------------------------------------------------
	//! Note de poste d'un carré, paresseuse et en cache (hauteurs 5 x 5 points, routes GetRoadsInAABB, carrefours) —
	//! SelectPostCells, PickWaveOrigins
	protected SRP_FrontCellInfo ScoreCell(int cell)
	{
		SRP_FrontCellInfo info = m_mCellInfo.Get(cell);
		if (info && info.m_bDone)
			return info;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsCellInPlay(cell))
			return null;
		if (!info)
		{
			info = new SRP_FrontCellInfo();
			m_mCellInfo.Set(cell, info);
		}
		SampleAltitude(front, cell, info);

		vector center = front.CellCenter(cell);
		float half = front.GetCellSize() * 0.5;
		float x0 = center[0] - half;
		float x1 = center[0] + half;
		float z0 = center[2] - half;
		float z1 = center[2] + half;

		// Hauteur : le sommet domine la moyenne des 8 voisins de terre et n'est dépassé par aucun
		int col = front.CellCol(cell);
		int row = front.CellRow(cell);
		float meanSum = 0;
		int around = 0;
		bool highest = true;
		for (int dz = -1; dz <= 1; dz++)
		{
			for (int dx = -1; dx <= 1; dx++)
			{
				if (dx == 0 && dz == 0)
					continue;
				int other = CellAtColRow(front, col + dx, row + dz);
				if (other < 0)
					continue;
				SRP_FrontCellInfo otherInfo = AltitudeInfo(front, other);
				meanSum += otherInfo.m_fMean;
				around++;
				if (otherInfo.m_fTop > info.m_fTop)
					highest = false;
			}
		}
		bool height = around > 0 && highest && info.m_fTop - meanSum / around >= m_fHeightProminence;

		// Routes du carré (largeur minimale), extrémités et points intérieurs
		RoadNetworkManager roads = SRP_Placement.Roads();
		array<BaseRoad> roadList = {};
		if (roads)
			roads.GetRoadsInAABB(Vector(x0, -100, z0), Vector(x1, 2000, z1), roadList);
		array<vector> points = {};
		array<vector> ends = {};
		array<int> endRoad = {};
		array<vector> inPoints = {};
		array<int> inRoad = {};
		array<bool> inEnd = {};
		bool anyRoad = false;
		bool mainRoad = false;
		vector nearestMain = center;
		float nearestMainDist = 1000000;
		vector nearestAny = center;
		float nearestAnyDist = 1000000;
		foreach (int roadIndex, BaseRoad road : roadList)
		{
			if (!road)
				continue;
			float width = road.GetWidth();
			if (width < m_fPostRoadMinWidth)
				continue;
			points.Clear();
			road.GetPoints(points);
			int last = points.Count() - 1;
			bool inside = false;
			foreach (int p, vector point : points)
			{
				if (point[0] < x0 || point[0] >= x1 || point[2] < z0 || point[2] >= z1)
					continue;
				inside = true;
				bool isEnd = p == 0 || p == last;
				inPoints.Insert(point);
				inRoad.Insert(roadIndex);
				inEnd.Insert(isEnd);
				if (isEnd)
				{
					ends.Insert(point);
					endRoad.Insert(roadIndex);
				}
				float toCenter = vector.DistanceXZ(point, center);
				if (toCenter < nearestAnyDist)
				{
					nearestAnyDist = toCenter;
					nearestAny = point;
				}
				if (width >= m_fMainRoadWidth && toCenter < nearestMainDist)
				{
					nearestMainDist = toCenter;
					nearestMain = point;
				}
			}
			if (inside)
			{
				anyRoad = true;
				if (width >= m_fMainRoadWidth)
					mainRoad = true;
			}
		}

		// Carrefour : 3 extrémités de routes DISTINCTES à 15 m au plus l'une de l'autre
		bool junction = false;
		vector junctionPoint = center;
		array<int> joined = {};
		for (int e = 0; e < ends.Count() && !junction; e++)
		{
			joined.Clear();
			vector endSum = vector.Zero;
			int endCount = 0;
			for (int f = 0; f < ends.Count(); f++)
			{
				if (vector.DistanceXZ(ends[e], ends[f]) > 15)
					continue;
				endSum = endSum + ends[f];
				endCount++;
				if (!joined.Contains(endRoad[f]))
					joined.Insert(endRoad[f]);
			}
			if (joined.Count() >= 3 && endCount > 0)
			{
				junction = true;
				junctionPoint = endSum * (1.0 / endCount);
			}
		}
		// À défaut : deux routes distinctes qui se croisent (deux points à 10 m, pas deux bouts de route qui se suivent)
		if (!junction && inPoints.Count() <= 400)
		{
			for (int a = 0; a < inPoints.Count() && !junction; a++)
			{
				for (int b = a + 1; b < inPoints.Count(); b++)
				{
					if (inRoad[a] == inRoad[b] || (inEnd[a] && inEnd[b]))
						continue;
					if (vector.DistanceXZ(inPoints[a], inPoints[b]) > 10)
						continue;
					junction = true;
					junctionPoint = (inPoints[a] + inPoints[b]) * 0.5;
					break;
				}
			}
		}

		int roadScore = 0;
		string roadKind = "";
		vector roadPoint = center;
		if (junction)
		{
			roadScore = 3;
			roadKind = "carrefour";
			roadPoint = junctionPoint;
		}
		else if (mainRoad)
		{
			roadScore = 2;
			roadKind = "route";
			roadPoint = nearestMain;
		}
		else if (anyRoad)
		{
			roadScore = 1;
			roadKind = "piste";
			roadPoint = nearestAny;
		}
		else
		{
			// Repli : aucune route rendue, le point de route le plus proche du centre (piste)
			vector track;
			if (SRP_Placement.FindRoadPoint(center, 100, track))
			{
				roadScore = 1;
				roadKind = "piste";
				roadPoint = track;
			}
		}

		int heightScore = 0;
		if (height)
		{
			heightScore = 2;
			if (roadScore > 0)
				heightScore = 3;
		}
		info.m_iRoadScore = roadScore;
		if (heightScore > roadScore)
		{
			info.m_iScore = heightScore;
			info.m_sKind = "hauteur";
			info.m_vPost = SRP_Placement.OnGround(info.m_vTop);
		}
		else
		{
			info.m_iScore = roadScore;
			info.m_sKind = roadKind;
			info.m_vPost = SRP_Placement.OnGround(roadPoint);
		}
		info.m_bDone = true;
		return info;
	}

	//------------------------------------------------------------------------------------------------
	//! m_iPostCellPercent des carrés du front, meilleures notes, départage fixe (cell x 7919) modulo 1000, jamais deux
	//! voisins — RebuildFront, Tick (notes restantes)
	protected void SelectPostCells()
	{
		m_bSelectPending = false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		array<vector> circleCenters = {};
		array<float> circleRadii = {};
		CollectHostileCircles(front, circleCenters, circleRadii);
		int attackZone = -1;
		if (m_Attack)
			attackZone = m_Attack.m_iZone;
		int multiplier = Math.MaxInt(front.GetCellCount(), 1);

		// Notes (au plus SCORE_BUDGET carrés neufs par passage) ; clé = (note x 1000 + départage) x cellules + carré
		int budget = SCORE_BUDGET;
		array<int> keys = {};
		foreach (int cell : m_aFrontRed)
		{
			if (attackZone >= 0 && front.GetCellZone(cell) == attackZone)
				continue;
			SRP_FrontCellInfo info = m_mCellInfo.Get(cell);
			if (!info || !info.m_bDone)
			{
				if (budget <= 0)
				{
					m_bSelectPending = true;
					continue;
				}
				budget--;
				info = ScoreCell(cell);
			}
			if (!info || info.m_iScore <= 0)
				continue;
			if (IsInsideCircles(info.m_vPost, circleCenters, circleRadii))
				continue;
			int tie = cell * 7919;
			tie = tie - (tie / 1000) * 1000;
			keys.Insert((info.m_iScore * 1000 + tie) * multiplier + cell);
		}
		// Tant que des carrés restent à noter, la sélection d'avant reste en place
		if (m_bSelectPending)
			return;

		keys.Sort(true);
		int wanted = Math.Ceil(m_iPostCellPercent * m_aFrontRed.Count() / 100.0);
		m_aPostCells.Clear();
		foreach (int key : keys)
		{
			if (m_aPostCells.Count() >= wanted)
				break;
			int chosen = key - (key / multiplier) * multiplier;
			if (IsNextToChosen(front, chosen))
				continue;
			m_aPostCells.Insert(chosen);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Postes vivants (anéantis, carré bleu, retrait discret) puis une pose au plus par passage : place par
	//! SRP_CmdCapacity.Get().Ask("poste-" + carré, SRP_ECmdCapClass.POSTE, soldats, 1, point, "territoire") (porte qui
	//! note la demande, RE12), puis SRP_CmdCapacity.Tag(record, POSTE) et m_sHomeTask = "poste front" ; m_iPostMaxActive ;
	//! carré sur le chemin des joueurs d'abord (IsOnPlayersPath si IsCommanderActive) — Tick
	protected void PostsTick(int nowUnix)
	{
		m_bPostPossible = false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !enemies)
			return;
		int tick = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		array<IEntity> active = enemies.GetActivePlayers();
		CleanOrphanForts(players);

		// 1. Postes vivants
		array<int> gone = {};
		for (int k = 0; k < m_mPosts.Count(); k++)
		{
			int cell = m_mPosts.GetKey(k);
			SRP_FrontPost post = m_mPosts.GetElement(k);
			if (!post)
			{
				gone.Insert(cell);
				continue;
			}
			// Anéanti : plus personne en état de combattre, pose terminée (F9 : retour après m_iPostRespawnHours)
			if (SRP_EnemyComponent.ActiveAgentsNear(post.m_aGroups, post.m_vPos, 0) == 0 && SRP_EnemyComponent.IsSettled(post.m_aGroups, tick))
			{
				m_mPostDestroyedUnix.Set(cell, nowUnix);
				JournalLine(string.Format("Poste du front anéanti : carré %1 (%2), retour dans %3 h au plus tôt", front.CellRef(cell), ZoneLabel(front, front.GetCellZone(cell)), m_iPostRespawnHours));
				enemies.RetireLater(post.m_aGroups);
				ForgetFort(post);
				gone.Insert(cell);
				continue;
			}
			// Carré passé bleu (ou postes coupés) : les survivants se retirent hors de vue
			if (!m_bFrontPosts || front.GetCellOwner(cell) != SRP_EFrontOwner.ROUGE)
			{
				enemies.RetireLater(post.m_aGroups);
				ForgetFort(post);
				gone.Insert(cell);
				continue;
			}
			// Retrait discret : personne à m_fPostDespawnDistance depuis m_iPostDespawnMinutes, et rien de vu
			if (NearestToPost(enemies, post, players) <= m_fPostDespawnDistance)
			{
				post.m_iNoPlayerSince = 0;
				continue;
			}
			if (post.m_iNoPlayerSince == 0)
				post.m_iNoPlayerSince = nowUnix;
			if (nowUnix - post.m_iNoPlayerSince < m_iPostDespawnMinutes * 60)
				continue;
			if (IsPostSeen(enemies, post, players))
				continue;
			enemies.DeleteAll(post.m_aGroups);
			ForgetFort(post);
			gone.Insert(cell);
			int despawnMeters = Math.Round(m_fPostDespawnDistance);
			JournalLine(string.Format("Poste du front retiré hors de vue : carré %1, plus aucun joueur à %2 m", front.CellRef(cell), despawnMeters));
		}
		foreach (int goneCell : gone)
			m_mPosts.Remove(goneCell);

		// 2. Pose : une au plus par passage, le carré le plus proche des joueurs (sur leur chemin d'abord, MA11)
		if (!m_bFrontPosts || m_aPostCells.IsEmpty() || m_mPosts.Count() >= m_iPostMaxActive)
			return;
		SRP_CmdManeuvers maneuvers = null;
		if (IsCommanderActive())
			maneuvers = SRP_CmdManeuvers.Get();
		float possibleReach = m_fPostSpawnDistance + 500;
		int best = -1;
		float bestReach = 1000000;
		bool bestOnPath = false;
		foreach (int candidate : m_aPostCells)
		{
			if (m_mPosts.Contains(candidate))
				continue;
			if (front.GetCellOwner(candidate) != SRP_EFrontOwner.ROUGE)
				continue;
			int destroyedAt;
			if (m_mPostDestroyedUnix.Find(candidate, destroyedAt) && nowUnix - destroyedAt < m_iPostRespawnHours * 3600)
				continue;
			SRP_FrontCellInfo info = m_mCellInfo.Get(candidate);
			if (!info || !info.m_bDone)
				continue;
			float reach = SRP_Placement.NearestPlayer(info.m_vPost, active);
			if (reach <= possibleReach)
				m_bPostPossible = true;
			if (reach > m_fPostSpawnDistance)
				continue;
			int deferUntil;
			if (m_mPostDeferUnix.Find(candidate, deferUntil) && nowUnix < deferUntil)
				continue;
			bool onPath = false;
			if (maneuvers)
				onPath = maneuvers.IsOnPlayersPath(candidate);
			bool better = false;
			if (best < 0)
				better = true;
			else if (onPath && !bestOnPath)
				better = true;
			else if (onPath == bestOnPath && reach < bestReach)
				better = true;
			if (!better)
				continue;
			best = candidate;
			bestReach = reach;
			bestOnPath = onPath;
		}
		if (best >= 0)
			PlacePost(best, nowUnix, players);
	}

	//------------------------------------------------------------------------------------------------
	//! F6 + CA1 : horloge en secondes (4 connectés, front non gelé), échéance D ; fenêtre D ± 20 min notée chaque
	//! minute par SRP_CmdManeuvers.CaWindowDecision si le Commandeur est actif, sinon départ à D — Tick
	protected void AttackClock(int nowUnix)
	{
		// Pas de l'horloge : le temps réel écoulé depuis le passage précédent, 10 s au plus (serveur à la traîne)
		int step = 0;
		if (m_iLastClockUnix > 0)
			step = Math.ClampInt(nowUnix - m_iLastClockUnix, 0, 10);
		m_iLastClockUnix = nowUnix;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || m_Attack || front.IsFrozen())
			return;

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		array<int> ids = {};
		int connected = GetGame().GetPlayerManager().GetPlayers(ids);
		int needed = m_iAttackMinPlayers;
		if (enemies)
			needed = enemies.MinPlayers(m_iAttackMinPlayers);
		bool quorum = connected >= needed;
		if (quorum || !m_bAttackClockNeedsQuorum)
			m_iAttackClockSec += step;
		if (m_iAttackDueSec <= 0)
			DrawAttackDue(front.GetThreat());

		if (IsCommanderActive())
		{
			WindowClock(front, nowUnix, quorum);
			return;
		}
		m_bWindowOpen = false;
		if (m_iAttackClockSec < m_iAttackDueSec)
			return;
		// Règle votée : départ à l'échéance ; sans quorum (horloge qui tourne toujours) l'échéance est perdue
		int zone = -1;
		if (quorum)
			zone = PickTargetZone();
		ResetAttackClock();
		if (zone < 0)
		{
			JournalLine("Échéance de contre-attaque perdue : pas de cible sur le front ou pas assez de joueurs connectés");
			return;
		}
		LaunchAttack(zone, false);
	}

	//------------------------------------------------------------------------------------------------
	//! F1 + F2 (+ CA2) : zone bleue non protégée qui touche la masse rouge principale ; 70 sur 100 la dernière prise,
	//! sinon PickByIntel du Commandeur, sinon au hasard ; -1 si aucune — AttackClock, RunOffensive
	protected int PickTargetZone()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || front.IsFrozen())
			return -1;
		array<int> zones = {};
		front.GetBlueFrontZones(zones);
		array<int> candidates = {};
		foreach (int zone : zones)
		{
			if (IsAttackable(front, zone, false))
				candidates.Insert(zone);
		}
		if (candidates.IsEmpty())
			return -1;

		// F2 : la dernière zone prise encore au contact (aucune si aucune candidate n'a encore été prise : date 0)
		int lastTaken = -1;
		int lastAt = 0;
		foreach (int candidate : candidates)
		{
			int capturedAt = front.GetZoneCapturedAt(candidate);
			if (capturedAt > lastAt)
			{
				lastAt = capturedAt;
				lastTaken = candidate;
			}
		}
		if (lastTaken >= 0 && Math.RandomInt(0, 100) < m_iAttackLastTakenPercent)
			return lastTaken;

		// CA2 : choix sur renseignement du Commandeur
		if (IsCommanderActive())
		{
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers)
			{
				int chosen = maneuvers.PickByIntel(candidates);
				if (chosen >= 0 && candidates.Contains(chosen))
					return chosen;
			}
		}
		// Sinon une autre au hasard
		if (lastTaken >= 0 && candidates.Count() > 1)
			candidates.RemoveItem(lastTaken);
		return candidates.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Annonce (ANNONCEE, SetZoneFlag, SRP_FrontRadio.AttackAnnounced), région et vagues prévues du Commandeur —
	//! AttackClock, ForceAttack
	protected bool LaunchAttack(int zone, bool forced)
	{
		if (m_Attack)
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || front.IsFrozen() || !IsAttackable(front, zone, forced))
			return false;
		int nowUnix = System.GetUnixTime();
		m_Attack = new SRP_CounterAttack();
		m_iAttackSerial++;
		m_Attack.m_iZone = zone;
		m_Attack.m_sZone = front.GetZoneLabel(zone);
		m_Attack.m_iState = SRP_EFrontAttack.ANNONCEE;
		m_Attack.m_iWarnEndUnix = nowUnix + Math.MaxInt(m_iAttackWarningMinutes, 0) * 60;
		m_Attack.m_bForced = forced;
		m_Attack.m_vTarget = AttackTargetPoint(front, zone);
		m_Attack.m_iPlannedWaves = Math.MaxInt(m_iAttackMaxWaves, 1);

		// Commandeur : région, désorganisation (OF8), vagues prévues (CA3)
		if (IsCommanderActive())
		{
			m_Attack.m_bCmdDriven = true;
			m_Attack.m_iRegion = m_Commander.GetRegionOfZone(zone);
			SRP_CmdRegionBook book = m_Commander.GetBook();
			if (book && m_Attack.m_iRegion >= 0)
				m_Attack.m_bDisorganized = book.IsRegionDisorganized(m_Attack.m_iRegion);
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers)
				m_Attack.m_iPlannedWaves = Math.MaxInt(maneuvers.PlannedWaves(zone), 1);
		}

		// Axes connus dès l'annonce (le soutien et la feinte de CA4 en ont besoin) ; revus à chaque vague
		array<vector> origins = {};
		int axes = PickWaveOrigins(origins);
		m_Attack.m_vOriginA = m_Attack.m_vTarget;
		m_Attack.m_vOriginB = m_Attack.m_vTarget;
		m_Attack.m_bHasB = false;
		if (axes > 0)
		{
			m_Attack.m_vOriginA = origins[0];
			m_Attack.m_vOriginB = origins[0];
		}
		if (axes > 1)
		{
			m_Attack.m_vOriginB = origins[1];
			m_Attack.m_bHasB = true;
		}

		ResetAttackClock();
		front.SetZoneFlag(zone, SRP_FrontComponent.FLAG_ATTACK_ANNOUNCED, true);
		SRP_FrontRadio.AttackAnnounced(zone, m_iAttackWarningMinutes);
		string forcedText = "";
		if (forced)
			forcedText = ", forcée par le Staff";
		JournalLine(string.Format("Contre-attaque annoncée sur %1%2 : assaut dans %3 min, %4 vague(s) prévue(s), %5 axe(s)", m_Attack.m_sZone, forcedText, m_iAttackWarningMinutes, m_Attack.m_iPlannedWaves, axes));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Une vague : taille min(ScaleGroupsNear, LocalRoom, WaveRoomGroups), origines PickWaveOrigins, papier ou pose
	//! (SpawnWaveFrom) après SRP_CmdCapacity.Get().Ask("vague-" + code, SRP_ECmdCapClass.COMBAT, …) et
	//! Tag(record, COMBAT), m_iAssaultZone = zone sur chaque groupe, radio AttackWave — AttackTick
	protected void SendWave()
	{
		if (!m_Attack)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !enemies)
			return;
		// Vague précédente encore échelonnée : elle est close telle quelle
		if (m_iWavePending > 0)
		{
			GetGame().GetCallqueue().Remove(SendWaveGroup);
			m_iWavePending = 0;
			FinishWave(m_Attack.m_iWaves);
		}
		int nowUnix = System.GetUnixTime();
		bool active = IsCommanderActive();
		SRP_CmdManeuvers maneuvers = null;
		if (active)
			maneuvers = SRP_CmdManeuvers.Get();
		if (!maneuvers)
			active = false;
		int wave = m_Attack.m_iWaves + 1;
		int zone = m_Attack.m_iZone;

		// CA3 : au moment de la 2e vague, une seule fois, le total est revu d'après la 1re
		if (wave == 2 && active && !m_Attack.m_bRevised)
		{
			m_Attack.m_iRetakenAtWave2 = m_Attack.m_iRetaken;
			m_Attack.m_iWave1Fit = SRP_EnemyComponent.ActiveAgentsNear(m_Attack.m_aWave1, vector.Zero, 0) + m_Attack.m_iWave1Paper;
			m_Attack.m_iPlannedWaves = Math.MaxInt(maneuvers.ReviseWaves(m_Attack), 1);
			m_Attack.m_bRevised = true;
			if (wave > m_Attack.m_iPlannedWaves)
			{
				JournalLine(string.Format("Contre-attaque sur %1 : pas de 2e vague, le total est revu à %2", m_Attack.m_sZone, m_Attack.m_iPlannedWaves));
				return;
			}
		}

		// Taille : au prorata des défenseurs, dans le budget local, la place de la vague (Commandeur) et la place COMBAT
		int threat = front.GetThreat();
		int groups = Math.MinInt(enemies.ScaleGroupsNear(m_Attack.m_vTarget, threat / 4, 4), enemies.LocalRoom(m_Attack.m_vTarget));
		if (active)
			groups = Math.MinInt(groups, maneuvers.WaveRoomGroups());
		int realRoom = SRP_CmdCapacity.Get().Room(SRP_ECmdCapClass.COMBAT, "territoire") / WAVE_GROUP_SOLDIERS;

		array<vector> origins = {};
		int axes = 0;
		if (groups > 0)
			axes = PickWaveOrigins(origins);
		if (axes > 0)
		{
			m_Attack.m_vOriginA = origins[0];
			m_Attack.m_vOriginB = origins[0];
			m_Attack.m_bHasB = axes > 1;
			if (axes > 1)
				m_Attack.m_vOriginB = origins[1];
		}
		// MO8 : avec une feinte, seul l'axe réel (axe 1) porte des soldats ; la feinte vise l'axe 2
		if (active && m_Attack.m_bFeint && axes > 1)
			axes = 1;

		m_Attack.m_iWaves = wave;
		m_Attack.m_aLastWave.Clear();
		m_Attack.m_aArrived.Clear();
		int paper = 0;
		int real = 0;
		if (groups > 0 && axes > 0)
		{
			vector zoneCenter = front.GetZoneCentroid(zone);
			array<vector> targets = {};
			for (int a = 0; a < axes; a++)
				targets.Insert(FirstRetakePoint(front, origins[a]));
			int minSeconds = Math.MaxInt(m_iAttackStaggerMinSeconds, 10);
			int maxSeconds = Math.MaxInt(m_iAttackStaggerMaxSeconds, minSeconds);
			int delay = 0;
			for (int i = 0; i < groups; i++)
			{
				int axis = i - (i / axes) * axes;
				vector origin = origins[axis];
				// CA6 : loin de tout joueur, l'élément reste sur le papier (aucune pose)
				if (active && maneuvers.WantPaperWave(origin, zoneCenter))
				{
					maneuvers.AddPaperWave(m_Attack, origin, WAVE_GROUP_SOLDIERS, wave, axis);
					paper++;
					continue;
				}
				if (realRoom <= 0)
					continue;
				realRoom--;
				if (real == 0)
				{
					// Le 1er élément part tout de suite
					if (PostWaveGroup(origin, targets[axis], wave))
						real++;
					continue;
				}
				// Les suivants, échelonnés (comme SRP_Territory.c:1866-1876)
				delay += Math.RandomIntInclusive(minSeconds, maxSeconds) * 1000;
				GetGame().GetCallqueue().CallLater(SendWaveGroup, delay, false, origin, targets[axis], wave, m_iAttackSerial);
				m_iWavePending++;
				real++;
			}
		}

		if (paper == 0 && real == 0)
		{
			if (wave == 1)
			{
				// 1re vague sans personne : annulation sans effet ni menace
				m_Attack.m_iWaves = 0;
				JournalLine(string.Format("Contre-attaque sur %1 annulée : aucune place ni origine pour la 1re vague", m_Attack.m_sZone));
				EndAttack(SRP_EAttackEnd.ANNULEE);
				return;
			}
			// Vague suivante sans place : comptée, sans élément
			m_Attack.m_iLastWaveUnix = nowUnix;
			JournalLine(string.Format("Contre-attaque sur %1 : vague %2 comptée sans élément (pas de place)", m_Attack.m_sZone, wave));
			return;
		}
		if (wave == 1)
			m_Attack.m_iWave1Paper = paper * WAVE_GROUP_SOLDIERS;
		SRP_FrontRadio.AttackWave(zone, wave, AxesText(front, origins, axes));
		JournalLine(string.Format("Contre-attaque sur %1 : vague %2 sur %3, %4 élément(s) posé(s) ou en route, %5 sur le papier, %6 axe(s)", m_Attack.m_sZone, wave, m_Attack.m_iPlannedWaves, real, paper, axes));
		if (m_iWavePending == 0)
			FinishWave(wave);
	}

	//------------------------------------------------------------------------------------------------
	//! Pose différée d'un groupe de la vague (échelonnement m_iAttack*StaggerSeconds) ; serial = numéro de l'attaque
	//! (un appel resté en file après EndAttack ne pose rien dans l'attaque suivante) — SendWave (CallLater)
	protected void SendWaveGroup(vector origin, vector target, int wave, int serial)
	{
		if (!m_Attack || serial != m_iAttackSerial || wave != m_Attack.m_iWaves)
			return;
		PostWaveGroup(origin, target, wave);
		if (m_iWavePending > 0)
			m_iWavePending--;
		if (m_iWavePending == 0)
			FinishWave(wave);
	}

	//------------------------------------------------------------------------------------------------
	//! Origines des vagues : recul de 3, 2, 1 carrés rouges face à la zone, à 500 m des joueurs ; axe 1 meilleure
	//! route, axe 2 écart d'au moins 60° ; rend le nombre (0 = vague annulée) — SendWave, LaunchAttack
	protected int PickWaveOrigins(notnull array<vector> origins)
	{
		origins.Clear();
		if (!m_Attack)
			return 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return 0;
		int zone = m_Attack.m_iZone;
		vector zoneCenter = front.GetZoneCentroid(zone);
		float cellSize = front.GetCellSize();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		array<int> zoneCells = {};
		front.GetZoneCells(zone, zoneCells);

		// Carrés rouges de la masse principale qui touchent un carré bleu de la zone
		array<int> contacts = {};
		foreach (int zoneCell : zoneCells)
		{
			if (front.GetCellOwner(zoneCell) != SRP_EFrontOwner.BLEU)
				continue;
			for (int dir = 0; dir < 4; dir++)
			{
				int red = front.Neighbour(zoneCell, dir);
				if (red < 0 || contacts.Contains(red) || !IsInMainRedMass(red))
					continue;
				if (front.GetCellOwner(red) != SRP_EFrontOwner.ROUGE)
					continue;
				contacts.Insert(red);
			}
		}

		// Recul face à la zone : 3, puis 2, puis 1 carré(s), sur du rouge de la masse principale, à 500 m des joueurs
		array<int> candCells = {};
		array<vector> candPoints = {};
		array<int> candScores = {};
		foreach (int contact : contacts)
		{
			vector from = front.CellCenter(contact);
			vector direction = Vector(from[0] - zoneCenter[0], 0, from[2] - zoneCenter[2]);
			float length = direction.Length();
			if (length < 1)
				continue;
			direction = direction * (1.0 / length);
			for (int k = 3; k >= 1; k--)
			{
				vector probe = from + direction * (k * cellSize);
				int back = front.CellIndexAt(probe);
				if (back < 0 || front.GetCellOwner(back) != SRP_EFrontOwner.ROUGE || !IsInMainRedMass(back))
					continue;
				if (candCells.Contains(back))
					break;
				vector point = front.CellCenter(back);
				if (SRP_Placement.NearestPlayer(point, players) < ORIGIN_PLAYER_MIN_M)
					continue;
				int roadScore = 0;
				SRP_FrontCellInfo info = ScoreCell(back);
				if (info)
					roadScore = info.m_iRoadScore;
				candCells.Insert(back);
				candPoints.Insert(point);
				candScores.Insert(roadScore);
				break;
			}
		}

		if (!candCells.IsEmpty())
		{
			// Axe 1 : la meilleure route
			int first = 0;
			for (int c = 1; c < candCells.Count(); c++)
			{
				if (candScores[c] > candScores[first])
					first = c;
			}
			origins.Insert(candPoints[first]);
			if (m_iAttackAxes >= 2 && candCells.Count() > 1)
			{
				// Axe 2 : le plus grand écart d'angle vu de la zone, 60° au moins ; sinon la deuxième meilleure route
				float firstAngle = AngleOf(zoneCenter, candPoints[first]);
				int second = -1;
				float bestGap = 0;
				for (int s = 0; s < candCells.Count(); s++)
				{
					if (s == first)
						continue;
					float gap = AngleGap(firstAngle, AngleOf(zoneCenter, candPoints[s]));
					if (gap > bestGap)
					{
						bestGap = gap;
						second = s;
					}
				}
				if (bestGap < 60)
				{
					second = -1;
					for (int t = 0; t < candCells.Count(); t++)
					{
						if (t == first)
							continue;
						if (second < 0 || candScores[t] > candScores[second])
							second = t;
					}
				}
				if (second >= 0)
					origins.Insert(candPoints[second]);
			}
			return origins.Count();
		}

		// Repli : le carré rouge de la masse principale le plus proche de la zone, à 500 m des joueurs, 1 500 m au plus
		array<vector> zoneCenters = {};
		foreach (int target : zoneCells)
			zoneCenters.Insert(front.CellCenter(target));
		int count = front.GetCellCount();
		float bestDistance = ORIGIN_FALLBACK_MAX_M;
		int chosen = -1;
		for (int cell = 0; cell < count; cell++)
		{
			if (!IsInMainRedMass(cell) || front.GetCellOwner(cell) != SRP_EFrontOwner.ROUGE)
				continue;
			vector cellCenter = front.CellCenter(cell);
			// Tri grossier d'abord : une zone tient dans 1,5 km autour de son centre de gravité
			if (vector.DistanceXZ(cellCenter, zoneCenter) > ORIGIN_FALLBACK_MAX_M + 1500)
				continue;
			float toZone = 1000000;
			foreach (vector zonePoint : zoneCenters)
			{
				float d = vector.DistanceXZ(cellCenter, zonePoint);
				if (d < toZone)
					toZone = d;
			}
			if (toZone > bestDistance)
				continue;
			if (SRP_Placement.NearestPlayer(cellCenter, players) < ORIGIN_PLAYER_MIN_M)
				continue;
			bestDistance = toZone;
			chosen = cell;
		}
		if (chosen >= 0)
			origins.Insert(front.CellCenter(chosen));
		else
			JournalLine(string.Format("Contre-attaque sur %1 : aucune origine hors de portée des joueurs", m_Attack.m_sZone));
		return origins.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Annonce (Support CA4, annulation si plus visable ou gel) puis assaut : DriveAssault du Commandeur (sinon
	//! AdvanceAssault), vagues, arrivée, fins DEFENDUE (CheckEnd), ROMPUE (60 min), ANNULEE ; jamais PERDUE ici (la
	//! perte de la zone vient d'OnZoneLost, seule porte) — Tick
	protected void AttackTick(int nowUnix)
	{
		if (!m_Attack)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		int zone = m_Attack.m_iZone;
		if (front.IsFrozen())
		{
			EndAttack(SRP_EAttackEnd.ANNULEE);
			return;
		}
		// Filet : la zone n'est plus à nous sans être passée par OnZoneLost (jamais PERDUE ici)
		if (front.GetZoneOwner(zone) != SRP_EFrontOwner.BLEU)
		{
			EndAttack(SRP_EAttackEnd.ANNULEE);
			return;
		}
		bool active = IsCommanderActive();
		SRP_CmdManeuvers maneuvers = null;
		if (active)
			maneuvers = SRP_CmdManeuvers.Get();
		if (!maneuvers)
			active = false;
		// Le Commandeur a mené l'attaque : il la range à la fin, même s'il est gelé entre-temps (EndAttack)
		if (active)
			m_Attack.m_bCmdDriven = true;

		// --- Annonce ---
		if (m_Attack.m_iState == SRP_EFrontAttack.ANNONCEE)
		{
			if (!IsAttackable(front, zone, m_Attack.m_bForced))
			{
				EndAttack(SRP_EAttackEnd.ANNULEE);
				return;
			}
			if (active)
				maneuvers.Support(m_Attack, nowUnix);
			if (!m_Attack || nowUnix < m_Attack.m_iWarnEndUnix)
				return;
			m_Attack.m_iState = SRP_EFrontAttack.ASSAUT;
			m_Attack.m_iAssaultStartUnix = nowUnix;
			m_Attack.m_iBlueSeen = CountAttackBlue(front, zone);
			front.SetZoneFlag(zone, SRP_FrontComponent.FLAG_ATTACK_ANNOUNCED, false);
			front.SetZoneFlag(zone, SRP_FrontComponent.FLAG_ATTACK, true);
			SendWave();
			if (m_Attack)
				m_Attack.m_iNextWaveUnix = nowUnix + Math.MaxInt(m_iAttackWaveMinutes, 1) * 60;
			return;
		}

		// --- Assaut ---
		UpdateRetaken(front);
		if (active)
			maneuvers.DriveAssault(m_Attack, nowUnix);
		else
			AdvanceAssault(nowUnix);
		if (!m_Attack)
			return;

		// Vague suivante
		if (m_Attack.m_iWaves < m_Attack.m_iPlannedWaves && nowUnix >= m_Attack.m_iNextWaveUnix)
		{
			SendWave();
			if (!m_Attack)
				return;
			m_Attack.m_iNextWaveUnix = nowUnix + Math.MaxInt(m_iAttackWaveMinutes, 1) * 60;
		}

		// CA5, C8 : deux tiers de pertes = défense réussie
		if (active)
		{
			int verdict = maneuvers.CheckEnd(m_Attack, nowUnix);
			if (verdict == SRP_EAttackEnd.DEFENDUE || verdict == SRP_EAttackEnd.ROMPUE || verdict == SRP_EAttackEnd.ANNULEE)
			{
				EndAttack(verdict);
				return;
			}
		}

		// F4 : dernière vague arrivée et plus personne de l'attaque en état de combattre dans la zone
		bool allPosted = m_Attack.m_iWaves >= m_Attack.m_iPlannedWaves && m_iWavePending == 0;
		bool arrived = false;
		if (allPosted)
		{
			if (active)
				arrived = maneuvers.WaveArrived(m_Attack);
			else
				arrived = LegacyWaveArrived(front, nowUnix);
		}
		if (arrived)
		{
			SRP_FrontCapture capture = front.GetCapture();
			if (capture && capture.CountFitAssaultersInZone(zone) == 0)
			{
				EndAttack(SRP_EAttackEnd.DEFENDUE);
				return;
			}
		}

		// C8 : l'ennemi rompt le contact au bout de m_iAttackMaxMinutes, menace inchangée
		if (nowUnix - m_Attack.m_iAssaultStartUnix >= Math.MaxInt(m_iAttackMaxMinutes, 1) * 60)
			EndAttack(SRP_EAttackEnd.ROMPUE);
	}

	//------------------------------------------------------------------------------------------------
	//! Commandeur gelé : mouvement d'origine des groupes vers les carrés reprenables (OrderMove, OrderSearch) — AttackTick
	protected void AdvanceAssault(int nowUnix)
	{
		if (!m_Attack)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !enemies)
			return;
		SRP_FrontCapture capture = front.GetCapture();
		int zone = m_Attack.m_iZone;
		int tick = System.GetTickCount();

		// Carrés reprenables de la zone (bleus, touchant du rouge, hors point clé gardé), avec les carrés de ses points
		// clés posés dans une zone voisine
		array<int> zoneCells = {};
		front.GetZoneCells(zone, zoneCells);
		AddZoneKeyCells(front, zone, zoneCells);
		array<int> retakeable = {};
		foreach (int zoneCell : zoneCells)
		{
			if (IsRetakeable(front, capture, zoneCell, zone))
				retakeable.Insert(zoneCell);
		}

		foreach (SRP_EnemyGroup record : m_Attack.m_aGroups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || SRP_EnemyComponent.AliveAgents(record) == 0)
				continue;
			// Libre : pas occupé, pas au contact depuis une minute, à pied
			if (record.m_Vehicle || record.m_iBusyUntilTick > tick)
				continue;
			if (record.m_iSpotTick > 0 && tick - record.m_iSpotTick < 60000)
				continue;
			int slot = m_Attack.m_aObjGroups.Find(record);
			int current = -1;
			int since = 0;
			if (slot >= 0)
			{
				current = m_Attack.m_aObjCells[slot];
				since = m_Attack.m_aObjSince[slot];
			}
			// Objectif revu : aucun, repris (rouge) ou plus reprenable, tenu depuis OBJECTIVE_HOLD_S, ou fouille alors
			// qu'un carré est de nouveau reprenable
			bool renew = false;
			if (current == -1)
				renew = true;
			else if (current == -2)
				renew = !retakeable.IsEmpty();
			else if (!retakeable.Contains(current) || nowUnix - since >= OBJECTIVE_HOLD_S)
				renew = true;
			if (!renew)
				continue;

			vector position = enemies.GroupPosition(record);
			int best = -1;
			float bestCost = 100000000;
			foreach (int candidate : retakeable)
			{
				vector candidateCenter = front.CellCenter(candidate);
				float cost = vector.DistanceXZ(position, candidateCenter) + 0.5 * vector.DistanceXZ(m_Attack.m_vTarget, candidateCenter);
				if (cost < bestCost)
				{
					bestCost = cost;
					best = candidate;
				}
			}
			if (best < 0)
			{
				// Plus rien à reprendre : fouille du point visé (une seule fois)
				if (current != -2)
					enemies.OrderSearch(record, m_Attack.m_vTarget);
				SetObjective(record, slot, -2, nowUnix);
				continue;
			}
			if (best != current)
				enemies.OrderMove(record, front.CellCenter(best));
			SetObjective(record, slot, best, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fin : SetZoneFlag retirés, ClearPaperAssault ; PERDUE -> AdoptAssailants (SEUL endroit : conversion en
	//! garnison et m_iAssaultZone à -1) ; DEFENDUE -> m_iAssaultZone à -1, RetireLater, AddThreat(-1), radio Defended ;
	//! ROMPUE -> m_iAssaultZone à -1, RetireLater, radio AttackBroken ; ANNULEE -> m_iAssaultZone à -1, radio
	//! AttackCalledOff si annoncée ; puis SRP_CmdManeuvers.EndAttack(attaque, fin) si le Commandeur est actif (ou a mené
	//! cette attaque, m_bCmdDriven) ; m_Attack = null — OnZoneLost (PERDUE), AttackTick (DEFENDUE, ROMPUE, ANNULEE), CancelAttack, OnFrontReset, ResetAll
	protected void EndAttack(int result)
	{
		if (!m_Attack)
			return;
		SRP_CounterAttack attack = m_Attack;
		GetGame().GetCallqueue().Remove(SendWaveGroup);
		m_iWavePending = 0;
		attack.m_iEnd = result;
		int zone = attack.m_iZone;
		// Rien n'est encore parti : l'annonce « n'aura pas lieu »
		bool announcedOnly = attack.m_iState == SRP_EFrontAttack.ANNONCEE || attack.m_iWaves == 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (front)
		{
			if (attack.m_iState == SRP_EFrontAttack.ASSAUT)
				UpdateRetaken(front);
			front.SetZoneFlag(zone, SRP_FrontComponent.FLAG_ATTACK_ANNOUNCED, false);
			front.SetZoneFlag(zone, SRP_FrontComponent.FLAG_ATTACK, false);
			SRP_FrontCapture capture = front.GetCapture();
			if (capture)
				capture.ClearPaperAssault(zone);
		}

		// PERDUE : les assaillants présents dans la zone deviennent la garnison de sa localité principale
		int adopted = 0;
		if (result == SRP_EAttackEnd.PERDUE && front)
		{
			string locality = MainLocalityName(front, zone);
			if (!locality.IsEmpty())
			{
				array<ref SRP_EnemyGroup> inZone = {};
				foreach (SRP_EnemyGroup candidate : attack.m_aGroups)
				{
					if (candidate && HasFitSoldierInZone(front, candidate, zone))
						inZone.Insert(candidate);
				}
				foreach (SRP_EnemyGroup moving : inZone)
					attack.m_aGroups.RemoveItem(moving);
				adopted = inZone.Count();
				AdoptAssailants(locality, inZone);
			}
		}

		// Les autres ne reprennent plus rien (capture) ; le Commandeur ordonne leur repli, puis tout se retire hors de vue.
		// Seulement s'il est actif ou s'il a mené cette attaque (gelé en cours de route, il range quand même ses listes :
		// papier, plans, éveil, appuis). Une attaque lancée et menée pendant le gel reste aux seules règles votées (VU6)
		foreach (SRP_EnemyGroup record : attack.m_aGroups)
		{
			if (record)
				record.m_iAssaultZone = -1;
		}
		if (m_Commander && m_Commander.IsStarted() && (IsCommanderActive() || attack.m_bCmdDriven))
		{
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers)
				maneuvers.EndAttack(attack, result);
		}
		if (enemies)
			enemies.RetireLater(attack.m_aGroups);
		attack.m_aGroups.Clear();

		string outcome = "annulée";
		if (result == SRP_EAttackEnd.PERDUE)
		{
			outcome = string.Format("zone reprise par l'ennemi, %1 élément(s) restent en défense", adopted);
		}
		else if (result == SRP_EAttackEnd.DEFENDUE)
		{
			outcome = "défendue";
			if (front)
			{
				front.AddThreat(-1, "contre-attaque repoussée sur " + attack.m_sZone);
				SRP_FrontRadio.Defended(zone, attack.m_iWaves, front.GetThreat());
			}
		}
		else if (result == SRP_EAttackEnd.ROMPUE)
		{
			outcome = "l'ennemi rompt le contact";
			SRP_FrontRadio.AttackBroken(zone, attack.m_iRetaken);
		}
		else if (announcedOnly && !m_bQuietEnd)
		{
			SRP_FrontRadio.AttackCalledOff(zone);
		}
		JournalLine(string.Format("Contre-attaque sur %1 terminée : %2 (%3 vague(s), %4 carré(s) repris)", attack.m_sZone, outcome, attack.m_iWaves, attack.m_iRetaken));
		m_Attack = null;
		m_iWindowTarget = -1;
		m_bWindowOpen = false;
	}

	//------------------------------------------------------------------------------------------------
	//! H2 : jour différent, heure dans la fenêtre, serveur vide depuis m_iOffensiveEmptyMinutes, front non gelé ->
	//! RunOffensive — Tick
	protected void OffensiveTick(int nowUnix)
	{
		array<int> ids = {};
		int connected = GetGame().GetPlayerManager().GetPlayers(ids);
		if (connected > 0 || m_iEmptySince <= 0)
		{
			// Un joueur est là (ou premier passage) : le serveur est vide à partir de maintenant au plus tôt
			m_iEmptySince = nowUnix;
			return;
		}
		if (!m_bFictiveOffensive)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || front.IsFrozen())
			return;
		if (m_sOffDay == Today())
			return;
		int hour;
		int minute;
		int second;
		System.GetHourMinuteSecond(hour, minute, second);
		bool inWindow;
		if (m_iOffensiveHourStart <= m_iOffensiveHourEnd)
			inWindow = hour >= m_iOffensiveHourStart && hour < m_iOffensiveHourEnd;
		else
			inWindow = hour >= m_iOffensiveHourStart || hour < m_iOffensiveHourEnd;
		if (!inWindow)
			return;
		if (nowUnix - m_iEmptySince < m_iOffensiveEmptyMinutes * 60)
			return;
		RunOffensive(false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! H2 : m_sOffDay noté et SAUVÉ avant le tirage ; zone PickTargetZone hors B4 ; chance min(max, base + parMenace
	//! x menace) (+ NightChance du Commandeur, CA7 ; annulée sous le tiers) ; réussite -> SRP_ZoneFall.LoseZone(zone,
	//! OFFENSIVE, true) ; texte par SRP_FrontRadio.NightOffensive — OffensiveTick, ForceOffensive
	protected void RunOffensive(bool forced, bool success)
	{
		m_iLastRunZone = -1;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		// Le jour est noté et écrit AVANT le tirage : un arrêt brutal ne rejoue jamais la nuit. Le résultat de la nuit
		// d'avant est effacé en même temps : le fichier (et la copie du jour) ne mêle jamais deux nuits
		m_sOffDay = Today();
		m_iOffChance = -1;
		m_iOffZone = -1;
		m_sOffText = "";
		m_iOffUnix = 0;
		m_bOffSuccess = false;
		m_sOffResult = "";
		m_aNewsSeen.Clear();
		front.Save();

		int zone = PickTargetZone();
		if (zone < 0)
		{
			NoOffensiveTonight(front, "pas de zone du front à viser");
			JournalLine("Offensive de la nuit : pas de cible sur le front");
			return;
		}
		int chance = Math.MinInt(m_iOffensiveChanceMax, m_iOffensiveChanceBase + m_iOffensiveChancePerThreat * front.GetThreat());
		// CA7 : les réserves du Commandeur (jamais pour une offensive forcée par le Staff)
		if (!forced && IsCommanderActive())
		{
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers)
			{
				bool cancel = false;
				chance = Math.MinInt(m_iOffensiveChanceMax, maneuvers.NightChance(chance, cancel));
				if (cancel)
				{
					NoOffensiveTonight(front, "réserves trop basses");
					SRP_CmdLog.Note(-1, SRP_ECmdLogKind.REFUS, "Pas d'offensive cette nuit : réserves trop basses");
					return;
				}
			}
		}
		chance = Math.ClampInt(chance, 0, 100);
		bool won = success;
		if (!forced)
			won = Math.RandomInt(0, 100) < chance;
		m_iLastRunZone = zone;
		m_iOffZone = zone;
		m_iOffChance = chance;
		m_iOffUnix = System.GetUnixTime();
		if (won)
		{
			// Porte unique des changements de zone : repeinte en silence, menace, radio ZoneFell(OFFENSIVE), OnZoneLost
			SRP_ZoneFall zoneFall = front.GetZoneFall();
			if (!zoneFall || !zoneFall.LoseZone(zone, SRP_EFrontReason.OFFENSIVE, true))
				won = false;
		}
		m_bOffSuccess = won;
		m_sOffText = SRP_FrontRadio.NightOffensive(zone, won, front.GetThreat());
		string verdict = "échouée";
		if (won)
			verdict = "réussie";
		string how = "au hasard";
		if (forced)
			how = "forcée par le Staff";
		m_sOffResult = string.Format("chance %1 sur 100, %2 (%3)", chance, verdict, how);
		JournalLine(string.Format("Offensive de la nuit sur %1 : %2", front.GetZoneLabel(zone), m_sOffResult));
		front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Nuit sans offensive (pas de cible, réserves trop basses) : le jour reste noté, sans zone ni heure (aucune
	//! nouvelle aux joueurs), le motif est gardé pour le rapport du Staff, même après un redémarrage — RunOffensive
	protected void NoOffensiveTonight(SRP_FrontComponent front, string why)
	{
		m_iOffZone = -1;
		m_iOffUnix = 0;
		m_sOffText = why;
		m_sOffResult = why;
		m_aNewsSeen.Clear();
		front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! À la première apparition de chaque joueur dans les m_iOffensiveNewsHours : SRP_FrontRadio.NightNews(playerId,
	//! m_sOffText) ; le joueur est retenu par son identité (fiche), pas par son numéro de connexion — une reconnexion
	//! ou un redémarrage ne redit pas la nouvelle — SCR_BaseGameMode.GetOnPlayerSpawned()
	protected void OnPlayerSpawnedNews(int playerId, IEntity player)
	{
		if (m_sOffText.IsEmpty() || m_iOffUnix <= 0 || playerId <= 0)
			return;
		if (System.GetUnixTime() - m_iOffUnix >= m_iOffensiveNewsHours * 3600)
			return;
		string who = NewsIdentity(playerId);
		if (m_aNewsSeen.Contains(who))
			return;
		m_aNewsSeen.Insert(who);
		MarkFrontDirty();
		SRP_FrontRadio.NightNews(playerId, m_sOffText);
	}

	//------------------------------------------------------------------------------------------------
	//! Identité durable d'un joueur : celle de sa fiche (SRP_PlayerManagerComponent), sinon l'identité Bohemia (la
	//! même, fiche pas encore enregistrée), sinon « #numéro » — OnPlayerSpawnedNews
	protected string NewsIdentity(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
		{
			SRP_PlayerRecord record = players.GetRecord(playerId);
			if (record && !record.m_sIdentity.IsEmpty())
				return record.m_sIdentity;
		}
		BackendApi backend = GetGame().GetBackendApi();
		if (backend)
		{
			string identity = backend.GetPlayerIdentityId(playerId);
			if (!identity.IsEmpty())
				return identity;
		}
		return "#" + playerId.ToString();
	}

	//================================================================================================
	// Aides internes
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Ligne au journal ENNEMI (Staff) — tout le composant
	protected static void JournalLine(string text)
	{
		SRP_EnemyComponent.Journal("ENNEMI", text);
	}

	//------------------------------------------------------------------------------------------------
	//! Le socle doit réécrire front.json (champs des localités ou fe_*) — F12
	protected static void MarkFrontDirty()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! La localité du socle d'après son nom exact, null sinon — F12
	protected SRP_FrontLocality FindLoc(string locality)
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
	//! Un compte qui fond d'un quart par heure (m_fLossRefillHours = 4) : montant x max(0, 1 - écoulé / durée) — F12
	protected int Decayed(int amount, int since, int nowUnix)
	{
		if (amount <= 0 || m_fLossRefillHours <= 0)
			return 0;
		float elapsed = nowUnix - since;
		if (elapsed < 0)
			elapsed = 0;
		float share = Math.Max(0, 1 - elapsed / (m_fLossRefillHours * 3600.0));
		return Math.Round(amount * share);
	}

	//------------------------------------------------------------------------------------------------
	//! Mémoire F12 et du Commandeur d'une localité à zéro (pertes, envoyés, reçus, vidée) — OnZoneCaptured,
	//! OnFrontReset, ResetAll
	protected void ResetLocalityMemory(SRP_FrontLocality loc)
	{
		if (!loc)
			return;
		loc.m_iLosses = 0;
		loc.m_iLossesAt = 0;
		loc.m_iSent = 0;
		loc.m_iSentAt = 0;
		loc.m_iBonus = 0;
		loc.m_iBonusAt = 0;
		loc.m_iEmptiedAt = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom de la localité principale d'une zone, "" sans localité — EndAttack, GetZoneStrength
	protected string MainLocalityName(SRP_FrontComponent front, int zone)
	{
		if (!front)
			return "";
		SRP_FrontZone zoneData = front.GetZone(zone);
		if (!zoneData || zoneData.m_iLocality < 0)
			return "";
		SRP_FrontLocality loc = front.GetLocality(zoneData.m_iLocality);
		if (!loc)
			return "";
		return loc.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé d'une zone, « ? » hors bornes — textes
	protected string ZoneLabel(SRP_FrontComponent front, int zone)
	{
		if (!front || zone < 0)
			return "?";
		return front.GetZoneLabel(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Met le propriétaire d'une localité du territoire d'accord avec la carte, même règle que le rattrapage du
	//! territoire (F11) : hostile = zone ROUGE et points clés pas tous saisis -> ENNEMI ; sinon (localité prise, ou zone
	//! bleue dont un point clé a été repris pendant une contre-attaque) -> NOUS, défenseurs retirés, pertes à zéro —
	//! OnZoneCaptured (Staff), OnFrontReset, OnRestored, ResetAll
	protected void SyncLocality(string locality, string why)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!front || !territory || locality.IsEmpty())
			return;
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		SRP_SectorState state = territory.FindState(locality);
		if (!zoneFall || !state)
			return;
		// Zone de la localité : celle du secteur du territoire (comme son rattrapage), sinon celle du socle
		int zone = -1;
		if (state.m_Sector)
			zone = state.m_Sector.m_iZone;
		else
		{
			SRP_FrontLocality loc = FindLoc(locality);
			if (loc)
				zone = loc.m_iZone;
		}
		bool hostile = zone >= 0 && front.GetZoneOwner(zone) == SRP_EFrontOwner.ROUGE && !zoneFall.IsLocalityTaken(locality);
		if (!hostile)
		{
			if (state.m_iOwner == SRP_ESectorOwner.NOUS)
				return;
			territory.SetLocalityOwner(locality, SRP_ESectorOwner.NOUS);
			WithdrawLocality(locality, why);
			ClearLosses(locality);
			return;
		}
		if (state.m_iOwner != SRP_ESectorOwner.ENNEMI)
			territory.SetLocalityOwner(locality, SRP_ESectorOwner.ENNEMI);
	}

	//------------------------------------------------------------------------------------------------
	//! SyncLocality pour toutes les localités du front — ResetAll, OnRestored
	protected void SyncAllLocalities(string why)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		int count = front.GetLocalityCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontLocality loc = front.GetLocality(i);
			if (loc && !loc.m_bHome)
				SyncLocality(loc.m_sName, why);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! À chaque passage : parts de place des localités que F11 veut garnir (C6) — les garnisons posées, puis les
	//! meilleurs scores de garnison à portée, 3 au plus — Tick
	protected void WantedGarrisonsTick()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !territory || !enemies)
			return;
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		array<IEntity> active = enemies.GetActivePlayers();
		array<string> keys = {};
		array<int> sizes = {};
		array<bool> near = {};
		array<bool> attacked = {};
		array<string> candNames = {};
		array<float> candScores = {};
		array<float> candReach = {};
		int count = front.GetLocalityCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontLocality loc = front.GetLocality(i);
			if (!loc || loc.m_bHome)
				continue;
			string name = loc.m_sName;
			if (zoneFall && zoneFall.IsLocalityTaken(name))
				continue;
			SRP_SectorState state = territory.FindState(name);
			if (!state || !state.m_Sector)
				continue;
			if (maneuvers && maneuvers.IsLocalityEmptied(name))
				continue;
			float reach = SRP_Placement.NearestPlayer(state.m_Sector.GetCenter(), active);
			if (territory.IsGarrisonPosted(name))
			{
				AddWanted(capacity, keys, sizes, near, attacked, name, reach);
				continue;
			}
			if (reach > WANTED_REACH_M)
				continue;
			candNames.Insert(name);
			candScores.Insert(GarrisonScore(state, reach));
			candReach.Insert(reach);
		}
		while (keys.Count() < WANTED_GARRISONS_MAX && !candNames.IsEmpty())
		{
			int bestIndex = 0;
			for (int c = 1; c < candNames.Count(); c++)
			{
				if (candScores[c] < candScores[bestIndex])
					bestIndex = c;
			}
			AddWanted(capacity, keys, sizes, near, attacked, candNames[bestIndex], candReach[bestIndex]);
			candNames.Remove(bestIndex);
			candScores.Remove(bestIndex);
			candReach.Remove(bestIndex);
		}
		capacity.SetWantedGarrisons(keys, sizes, near, attacked);
	}

	//------------------------------------------------------------------------------------------------
	//! Une localité voulue : clé, effectif décidé, joueur proche (cap_garnison_proche_m), attaquée — WantedGarrisonsTick
	protected void AddWanted(SRP_CmdCapacity capacity, array<string> keys, array<int> sizes, array<bool> near, array<bool> attacked, string name, float reach)
	{
		keys.Insert(name);
		sizes.Insert(GetLocalityEffective(name));
		near.Insert(reach <= capacity.m_iGarrisonNearM);
		attacked.Insert(IsLocalityUnderAttack(name));
	}

	//------------------------------------------------------------------------------------------------
	//! Carré (col, row) en jeu, -1 hors grille ou hors jeu — ScoreCell, IsNearRed
	protected int CellAtColRow(SRP_FrontComponent front, int col, int row)
	{
		int n = front.GetGridSize();
		if (col < 0 || row < 0 || col >= n || row >= n)
			return -1;
		int cell = row * n + col;
		if (!front.IsCellInPlay(cell))
			return -1;
		return cell;
	}

	//------------------------------------------------------------------------------------------------
	//! Un carré rouge à « depth » carrés au plus (boîte de (2 x depth + 1)² carrés) — F10
	protected bool IsNearRed(SRP_FrontComponent front, int cell, int depth)
	{
		int col = front.CellCol(cell);
		int row = front.CellRow(cell);
		int reach = Math.MaxInt(depth, 0);
		for (int dz = -reach; dz <= reach; dz++)
		{
			for (int dx = -reach; dx <= reach; dx++)
			{
				int other = CellAtColRow(front, col + dx, row + dz);
				if (other >= 0 && front.GetCellOwner(other) == SRP_EFrontOwner.ROUGE)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Altitudes d'un carré relevées sur 5 x 5 points espacés de 40 m (sommet, moyenne, point le plus haut) — ScoreCell
	protected void SampleAltitude(SRP_FrontComponent front, int cell, SRP_FrontCellInfo info)
	{
		if (!info || info.m_bAlt)
			return;
		vector center = front.CellCenter(cell);
		BaseWorld world = GetGame().GetWorld();
		float top = -100000;
		float sum = 0;
		int samples = 0;
		vector topPoint = center;
		for (int ix = -2; ix <= 2; ix++)
		{
			for (int iz = -2; iz <= 2; iz++)
			{
				float x = center[0] + ix * 40;
				float z = center[2] + iz * 40;
				float y = center[1];
				if (world)
					y = world.GetSurfaceY(x, z);
				sum += y;
				samples++;
				if (y > top)
				{
					top = y;
					topPoint = Vector(x, y, z);
				}
			}
		}
		info.m_fTop = top;
		info.m_fMean = sum / Math.MaxInt(samples, 1);
		info.m_vTop = topPoint;
		info.m_bAlt = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le cache d'un carré avec ses altitudes (sans la note complète) — ScoreCell (voisins)
	protected SRP_FrontCellInfo AltitudeInfo(SRP_FrontComponent front, int cell)
	{
		SRP_FrontCellInfo info = m_mCellInfo.Get(cell);
		if (!info)
		{
			info = new SRP_FrontCellInfo();
			m_mCellInfo.Set(cell, info);
		}
		SampleAltitude(front, cell, info);
		return info;
	}

	//------------------------------------------------------------------------------------------------
	//! Cercles des localités hostiles (point clé CENTRE et rayon de la localité) : pas de poste dedans — SelectPostCells
	protected void CollectHostileCircles(SRP_FrontComponent front, array<vector> centers, array<float> radii)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		int count = front.GetLocalityCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontLocality loc = front.GetLocality(i);
			if (!loc || loc.m_bHome)
				continue;
			if (zoneFall && zoneFall.IsLocalityTaken(loc.m_sName))
				continue;
			SRP_SectorState state = null;
			if (territory)
				state = territory.FindState(loc.m_sName);
			if (state && state.m_Sector)
			{
				centers.Insert(state.m_Sector.GetCenter());
				radii.Insert(state.m_Sector.m_fRadius);
				continue;
			}
			// Sans état de territoire : le point clé CENTRE, rayon d'un village
			SRP_FrontKeyPoint keyPoint = front.GetKeyPoint(loc.m_iCentre);
			if (keyPoint)
			{
				centers.Insert(keyPoint.m_vPos);
				radii.Insert(250.0);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La position est-elle dans un des cercles ? — SelectPostCells
	protected bool IsInsideCircles(vector position, array<vector> centers, array<float> radii)
	{
		for (int i = 0; i < centers.Count(); i++)
		{
			if (vector.DistanceXZ(position, centers[i]) <= radii[i])
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré touche-t-il (8 voisins) un carré déjà retenu pour un poste ? — SelectPostCells
	protected bool IsNextToChosen(SRP_FrontComponent front, int cell)
	{
		int col = front.CellCol(cell);
		int row = front.CellRow(cell);
		foreach (int chosen : m_aPostCells)
		{
			if (Math.AbsInt(front.CellCol(chosen) - col) <= 1 && Math.AbsInt(front.CellRow(chosen) - row) <= 1)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose d'un poste sur un carré retenu (place, point hors de vue, élément, décor) — PostsTick
	protected void PlacePost(int cell, int nowUnix, array<IEntity> players)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontCellInfo info = m_mCellInfo.Get(cell);
		if (!front || !enemies || !info || !enemies.HasPrefabs())
			return;
		// MA11 : 4 partout avec le Commandeur ; Commandeur gelé : 2 sur une hauteur ou une piste
		int size = m_iPostSoldiersKey;
		if (!IsCommanderActive() && info.m_sKind != "carrefour" && info.m_sKind != "route")
			size = m_iPostSoldiersMinor;
		size = Math.MaxInt(size, 1);

		string refusal = SRP_CmdCapacity.Get().Ask("poste-" + cell.ToString(), SRP_ECmdCapClass.POSTE, size, 1, info.m_vPost, "territoire");
		if (!refusal.IsEmpty())
		{
			m_mPostDeferUnix.Set(cell, nowUnix + 30);
			return;
		}
		vector position;
		if (!SRP_Placement.FindSpawnPosition(info.m_vPost, 5, 35, players, false, true, position, m_fPostPlayerMinDistance))
		{
			m_mPostDeferUnix.Set(cell, nowUnix + 30);
			return;
		}
		ResourceName prefab = PickPostPrefab(enemies, size);
		SRP_EnemyGroup record = enemies.SpawnGroup(position, true, info.m_vPost, "territoire", m_fPostDefendRadius, vector.Zero, -1, false, prefab);
		if (!record)
		{
			m_mPostDeferUnix.Set(cell, nowUnix + 30);
			return;
		}
		enemies.ApplyRole(record, SRP_ECRXRole.GARNISON, info.m_vPost, m_fPostHoldRadius);
		enemies.NoteHome(record, "poste front", true);
		SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.POSTE);

		SRP_FrontPost post = new SRP_FrontPost();
		post.m_iCell = cell;
		post.m_vPos = info.m_vPost;
		post.m_sKind = info.m_sKind;
		post.m_aGroups.Insert(record);
		post.m_Fort = SpawnFort(enemies, info.m_vPost, players);
		m_mPosts.Set(cell, post);
		m_mPostDeferUnix.Remove(cell);
		JournalLine(string.Format("Poste du front posé : carré %1 (%2, %3), %4 soldat(s)", front.CellRef(cell), info.m_sKind, ZoneLabel(front, front.GetCellZone(cell)), size));
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab d'un poste : liste réglée, sinon sections (4) ou sentinelles (2) de la bonne taille, sinon tirage — PlacePost
	protected ResourceName PickPostPrefab(SRP_EnemyComponent enemies, int size)
	{
		ResourceName prefab = "";
		if (size >= 4)
		{
			prefab = RandomPrefab(m_aPostKeyPrefabs);
			if (prefab.IsEmpty())
				prefab = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), size, size);
		}
		else
		{
			prefab = RandomPrefab(m_aPostMinorPrefabs);
			if (prefab.IsEmpty())
				prefab = enemies.PickPrefabOfSize(enemies.GetSentryPrefabs(), size, size);
		}
		if (prefab.IsEmpty())
			prefab = enemies.PickGroupPrefabOfSize(size, size);
		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Un prefab au hasard d'une liste, "" si elle est vide — PickPostPrefab, SpawnFort
	protected ResourceName RandomPrefab(array<ResourceName> list)
	{
		if (!list || list.IsEmpty())
			return "";
		return list.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Décor retranché d'un poste, posé seulement hors de vue et loin des joueurs ; null sans décor — PlacePost
	protected IEntity SpawnFort(SRP_EnemyComponent enemies, vector position, array<IEntity> players)
	{
		ResourceName prefab = RandomPrefab(m_aPostFortPrefabs);
		if (prefab.IsEmpty())
			return null;
		if (SRP_Placement.NearestPlayer(position, players) < m_fPostPlayerMinDistance)
			return null;
		if (SRP_Placement.IsSeenByAnyPlayer(position, players))
			return null;
		return enemies.SpawnAt(prefab, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Le décor d'un poste parti passe aux orphelins, retirés hors de vue — PostsTick, RetireAllPosts
	protected void ForgetFort(SRP_FrontPost post)
	{
		if (!post || !post.m_Fort)
			return;
		m_aOrphanForts.Insert(post.m_Fort);
		post.m_Fort = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Décors orphelins retirés dès qu'aucun joueur n'est à FORT_CLEAR_M et que personne ne les voit — PostsTick
	protected void CleanOrphanForts(array<IEntity> players)
	{
		for (int i = m_aOrphanForts.Count() - 1; i >= 0; i--)
		{
			IEntity fort = m_aOrphanForts[i];
			if (!fort || fort.IsDeleted())
			{
				m_aOrphanForts.Remove(i);
				continue;
			}
			vector origin = fort.GetOrigin();
			if (SRP_Placement.NearestPlayer(origin, players) < FORT_CLEAR_M || SRP_Placement.IsSeenByAnyPlayer(origin, players))
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(fort);
			m_aOrphanForts.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Distance du joueur le plus proche au poste ou à l'un de ses éléments — PostsTick (retrait discret)
	protected float NearestToPost(SRP_EnemyComponent enemies, SRP_FrontPost post, array<IEntity> players)
	{
		float nearest = SRP_Placement.NearestPlayer(post.m_vPos, players);
		foreach (SRP_EnemyGroup record : post.m_aGroups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			float groupDistance = SRP_Placement.NearestPlayer(enemies.GroupPosition(record), players);
			if (groupDistance < nearest)
				nearest = groupDistance;
		}
		return nearest;
	}

	//------------------------------------------------------------------------------------------------
	//! Le poste ou l'un de ses éléments est-il vu d'un joueur ? — PostsTick (retrait discret)
	protected bool IsPostSeen(SRP_EnemyComponent enemies, SRP_FrontPost post, array<IEntity> players)
	{
		if (SRP_Placement.IsSeenByAnyPlayer(post.m_vPos, players))
			return true;
		foreach (SRP_EnemyGroup record : post.m_aGroups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (SRP_Placement.IsSeenByAnyPlayer(enemies.GroupPosition(record), players))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les postes retirés hors de vue (survivants) — ResetAll, OnRestored
	protected void RetireAllPosts()
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		for (int k = 0; k < m_mPosts.Count(); k++)
		{
			SRP_FrontPost post = m_mPosts.GetElement(k);
			if (!post)
				continue;
			if (enemies)
				enemies.RetireLater(post.m_aGroups);
			ForgetFort(post);
		}
		m_mPosts.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Postes d'une zone retirés hors de vue — OnZoneCaptured
	protected void RetirePostsOfZone(SRP_FrontComponent front, int zone)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		array<int> gone = {};
		for (int k = 0; k < m_mPosts.Count(); k++)
		{
			int cell = m_mPosts.GetKey(k);
			if (front.GetCellZone(cell) != zone)
				continue;
			SRP_FrontPost post = m_mPosts.GetElement(k);
			if (post)
			{
				if (enemies)
					enemies.RetireLater(post.m_aGroups);
				ForgetFort(post);
			}
			gone.Insert(cell);
		}
		foreach (int goneCell : gone)
			m_mPosts.Remove(goneCell);
	}

	//------------------------------------------------------------------------------------------------
	//! Horloge F6 remise à zéro (nouvelle échéance tirée au passage suivant) — AttackClock, LaunchAttack, ResetAll
	protected void ResetAttackClock()
	{
		m_iAttackClockSec = 0;
		m_iAttackDueSec = 0;
		m_iWindowTarget = -1;
		m_bWindowOpen = false;
		m_iWindowNextSec = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Échéance D : de m_iAttackMinutesThreat0 (menace 0) à m_iAttackMinutesThreat10 (menace 10), aléa de
	//! m_iAttackJitterPercent — AttackClock
	protected void DrawAttackDue(int threat)
	{
		float minutes = m_iAttackMinutesThreat0 + (m_iAttackMinutesThreat10 - m_iAttackMinutesThreat0) * Math.ClampInt(threat, 0, 10) / 10.0;
		float jitter = Math.ClampInt(m_iAttackJitterPercent, 0, 90) / 100.0;
		float factor = Math.RandomFloat(1 - jitter, 1 + jitter);
		m_iAttackDueSec = Math.Max(60, Math.Round(minutes * factor * 60));
	}

	//------------------------------------------------------------------------------------------------
	//! CA1 : fenêtre D ± ca_fenetre_min ; cible choisie à l'ouverture, revue et soumise au Commandeur chaque minute
	//! d'horloge — AttackClock (Commandeur actif)
	protected void WindowClock(SRP_FrontComponent front, int nowUnix, bool quorum)
	{
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (!maneuvers)
			return;
		int window = Math.MaxInt(maneuvers.m_iWindowMin, 0) * 60;
		if (m_iAttackClockSec < m_iAttackDueSec - window)
		{
			m_bWindowOpen = false;
			m_iWindowTarget = -1;
			return;
		}
		if (!m_bWindowOpen)
		{
			m_bWindowOpen = true;
			m_iWindowTarget = PickTargetZone();
			m_iWindowNextSec = m_iAttackClockSec;
		}
		if (m_iAttackClockSec < m_iWindowNextSec)
			return;
		m_iWindowNextSec = m_iAttackClockSec + 60;
		if (m_iWindowTarget < 0 || !IsAttackable(front, m_iWindowTarget, false))
			m_iWindowTarget = PickTargetZone();
		if (m_iWindowTarget < 0)
		{
			// Fenêtre passée sans aucune cible : l'échéance est perdue, comme la règle votée
			if (m_iAttackClockSec >= m_iAttackDueSec + window)
			{
				JournalLine("Échéance de contre-attaque perdue : pas de cible sur le front pendant la fenêtre");
				ResetAttackClock();
			}
			return;
		}
		if (!quorum && m_bAttackClockNeedsQuorum)
			return;
		if (!maneuvers.CaWindowDecision(m_iAttackClockSec, m_iAttackDueSec, m_iWindowTarget, nowUnix))
			return;
		int zone = m_iWindowTarget;
		ResetAttackClock();
		LaunchAttack(zone, false);
	}

	//------------------------------------------------------------------------------------------------
	//! F1 + B4 : zone à nous, jamais la base ni une voisine, au contact du rouge (non forcée : de la masse rouge
	//! principale) — PickTargetZone, LaunchAttack, AttackTick
	protected bool IsAttackable(SRP_FrontComponent front, int zone, bool forced)
	{
		if (!front || zone <= 0 || zone >= front.GetZoneCount())
			return false;
		if (front.IsBaseZone(zone) || front.IsZoneProtected(zone))
			return false;
		if (front.GetZoneOwner(zone) != SRP_EFrontOwner.BLEU || !front.IsZoneAtFront(zone))
			return false;
		if (forced)
			return true;
		return TouchesMainRed(front, zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Un carré bleu de la zone touche par un côté un carré de la masse rouge principale (F1) — IsAttackable
	protected bool TouchesMainRed(SRP_FrontComponent front, int zone)
	{
		array<int> zoneCells = {};
		front.GetZoneCells(zone, zoneCells);
		foreach (int cell : zoneCells)
		{
			if (front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
				continue;
			for (int dir = 0; dir < 4; dir++)
			{
				int other = front.Neighbour(cell, dir);
				if (other >= 0 && IsInMainRedMass(other) && front.GetCellOwner(other) == SRP_EFrontOwner.ROUGE)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Point visé : le premier point clé de la zone, sinon le centre de ses carrés bleus (le carré bleu le plus proche
	//! de leur moyenne) — LaunchAttack
	protected vector AttackTargetPoint(SRP_FrontComponent front, int zone)
	{
		if (front.GetKeyPointCount(zone) > 0)
			return front.GetKeyPointPos(zone, 0);
		array<int> zoneCells = {};
		front.GetZoneCells(zone, zoneCells);
		vector sum = vector.Zero;
		int blue = 0;
		foreach (int cell : zoneCells)
		{
			if (front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
				continue;
			sum = sum + front.CellCenter(cell);
			blue++;
		}
		if (blue == 0)
			return front.GetZoneCentroid(zone);
		vector mean = sum * (1.0 / blue);
		vector best = mean;
		float bestDistance = 1000000;
		foreach (int blueCell : zoneCells)
		{
			if (front.GetCellOwner(blueCell) != SRP_EFrontOwner.BLEU)
				continue;
			vector blueCenter = front.CellCenter(blueCell);
			float distance = vector.DistanceXZ(blueCenter, mean);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = blueCenter;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré de la zone que l'ennemi peut reprendre (bleu, touchant du rouge, CanRedRetake, point clé non gardé F5) ;
	//! un carré d'une zone voisine qui porte un point clé de la zone (QG posé chez le voisin) compte aussi, comme dans
	//! la capture (IsKeyCellOfZone) — AdvanceAssault, FirstRetakePoint
	protected bool IsRetakeable(SRP_FrontComponent front, SRP_FrontCapture capture, int cell, int zone)
	{
		if (front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
			return false;
		if (front.GetCellZone(cell) != zone && !IsZoneKeyCell(front, cell, zone))
			return false;
		if (!front.CanRedRetake(cell) || !front.HasNeighbourOwnedBy(cell, SRP_EFrontOwner.ROUGE, false))
			return false;
		if (capture && capture.IsKeyCellGuarded(cell))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré porte un point clé de la zone (même règle que SRP_FrontCapture.IsKeyCellOfZone) — IsRetakeable,
	//! HasFitSoldierInZone
	protected bool IsZoneKeyCell(SRP_FrontComponent front, int cell, int zone)
	{
		if (cell < 0 || zone < 0)
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
	//! Ajoute à la liste les carrés des points clés de la zone qui n'y sont pas déjà (QG posé dans un carré d'une
	//! zone voisine) — AdvanceAssault, FirstRetakePoint
	protected void AddZoneKeyCells(SRP_FrontComponent front, int zone, notnull array<int> cells)
	{
		int keys = front.GetKeyPointCount(zone);
		for (int k = 0; k < keys; k++)
		{
			int keyCell = front.GetKeyPointCell(zone, k);
			if (keyCell >= 0 && !cells.Contains(keyCell))
				cells.Insert(keyCell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Centre du 1er carré bleu à reprendre sur un axe : le reprenable le plus proche de l'origine (sinon le point
	//! visé) — SendWave
	protected vector FirstRetakePoint(SRP_FrontComponent front, vector origin)
	{
		if (!m_Attack)
			return origin;
		SRP_FrontCapture capture = front.GetCapture();
		array<int> zoneCells = {};
		front.GetZoneCells(m_Attack.m_iZone, zoneCells);
		AddZoneKeyCells(front, m_Attack.m_iZone, zoneCells);
		vector best = m_Attack.m_vTarget;
		float bestDistance = 1000000;
		foreach (int cell : zoneCells)
		{
			if (!IsRetakeable(front, capture, cell, m_Attack.m_iZone))
				continue;
			vector cellCenter = front.CellCenter(cell);
			float distance = vector.DistanceXZ(origin, cellCenter);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = cellCenter;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose d'un élément de vague : place COMBAT (Ask), SpawnWaveFrom, m_iAssaultZone, tâche « assaut », Tag COMBAT ;
	//! vrai si un élément est posé — SendWave, SendWaveGroup
	protected bool PostWaveGroup(vector origin, vector target, int wave)
	{
		if (!m_Attack || m_Attack.m_iState != SRP_EFrontAttack.ASSAUT || wave != m_Attack.m_iWaves)
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !enemies)
			return false;
		int zone = m_Attack.m_iZone;
		string refusal = SRP_CmdCapacity.Get().Ask("vague-" + front.GetZoneCode(zone), SRP_ECmdCapClass.COMBAT, WAVE_GROUP_SOLDIERS, 1, origin, "territoire");
		if (!refusal.IsEmpty())
		{
			JournalLine(string.Format("Contre-attaque sur %1, vague %2 : élément non posé (%3)", m_Attack.m_sZone, wave, refusal));
			return false;
		}
		int before = m_Attack.m_aGroups.Count();
		int spawned = enemies.SpawnWaveFrom(origin, 150, target, 1, "territoire", m_Attack.m_aGroups);
		if (spawned <= 0)
			return false;
		for (int i = before; i < m_Attack.m_aGroups.Count(); i++)
		{
			SRP_EnemyGroup record = m_Attack.m_aGroups[i];
			if (!record)
				continue;
			// Soldats de l'attaque : seuls à reprendre des carrés (capture), comptes rendus droit au Commandeur
			record.m_iAssaultZone = zone;
			record.m_sTask = "assaut";
			record.m_sHomeTask = "assaut";
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.COMBAT);
			m_Attack.m_aLastWave.Insert(record);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Vague entièrement posée : heure de la dernière pose, 1re vague retenue (CA3), plan d'assaut du Commandeur
	//! (OnWavePosted, une fois par vague) ou soldats posés comptés — SendWave, SendWaveGroup
	protected void FinishWave(int wave)
	{
		if (!m_Attack || wave != m_Attack.m_iWaves)
			return;
		m_Attack.m_iLastWaveUnix = System.GetUnixTime();
		int soldiers = SRP_EnemyComponent.SoldiersIn(m_Attack.m_aLastWave, System.GetTickCount());
		if (wave == 1)
		{
			m_Attack.m_aWave1.Clear();
			foreach (SRP_EnemyGroup record : m_Attack.m_aLastWave)
			{
				if (record)
					m_Attack.m_aWave1.Insert(record);
			}
			m_Attack.m_iWave1Posted = soldiers + m_Attack.m_iWave1Paper;
		}
		if (IsCommanderActive())
		{
			m_Attack.m_bCmdDriven = true;
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers && !m_Attack.m_aLastWave.IsEmpty())
				maneuvers.OnWavePosted(m_Attack, wave, m_Attack.m_aLastWave, m_Attack.m_vOriginA, m_Attack.m_vOriginB, m_Attack.m_bHasB);
			return;
		}
		m_Attack.m_iPostedSoldiers += soldiers;
	}

	//------------------------------------------------------------------------------------------------
	//! Commandeur gelé : dernière vague arrivée = chacun de ses éléments est entré dans la zone ou hors de combat, ou
	//! m_iWaveArrivalMaxMinutes depuis sa pose — AttackTick
	protected bool LegacyWaveArrived(SRP_FrontComponent front, int nowUnix)
	{
		if (!m_Attack)
			return false;
		if (nowUnix - m_Attack.m_iLastWaveUnix >= m_iWaveArrivalMaxMinutes * 60)
			return true;
		int tick = System.GetTickCount();
		int done = 0;
		foreach (SRP_EnemyGroup record : m_Attack.m_aLastWave)
		{
			if (!record || m_Attack.m_aArrived.Contains(record))
			{
				done++;
				continue;
			}
			if (HasFitSoldierInZone(front, record, m_Attack.m_iZone))
			{
				m_Attack.m_aArrived.Insert(record);
				done++;
				continue;
			}
			array<ref SRP_EnemyGroup> single = {};
			single.Insert(record);
			if (SRP_EnemyComponent.ActiveAgentsNear(single, vector.Zero, 0) == 0 && SRP_EnemyComponent.IsSettled(single, tick))
				done++;
		}
		return done >= m_Attack.m_aLastWave.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat du groupe, vivant et conscient, est dans un carré de la zone ou sur le carré d'un de ses points clés
	//! posé dans une zone voisine (Q2, même règle que CountFitAssaultersInZone) — EndAttack (PERDUE), LegacyWaveArrived
	protected bool HasFitSoldierInZone(SRP_FrontComponent front, SRP_EnemyGroup record, int zone)
	{
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(record, members);
		foreach (IEntity member : members)
		{
			if (!IsFitCharacter(member))
				continue;
			int cell = front.CellIndexAt(member.GetOrigin());
			if (cell >= 0 && (front.GetCellZone(cell) == zone || IsZoneKeyCell(front, cell, zone)))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Personnage vivant et conscient — HasFitSoldierInZone
	protected bool IsFitCharacter(IEntity entity)
	{
		if (!entity || entity.IsDeleted() || SRP_Utils.IsDead(entity))
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(entity.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.IsUnconscious())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés repris pendant l'assaut : chaque baisse du nombre de carrés bleus visés par l'attaque est comptée —
	//! AttackTick, EndAttack
	protected void UpdateRetaken(SRP_FrontComponent front)
	{
		if (!m_Attack)
			return;
		int blue = CountAttackBlue(front, m_Attack.m_iZone);
		if (m_Attack.m_iBlueSeen >= 0 && blue < m_Attack.m_iBlueSeen)
			m_Attack.m_iRetaken += m_Attack.m_iBlueSeen - blue;
		m_Attack.m_iBlueSeen = blue;
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés bleus visés par l'attaque : ceux de la zone, plus le carré bleu de chacun de ses points clés posé dans une
	//! zone voisine (Q2, QG chez le voisin, que AdvanceAssault vise aussi) ; un carré n'est compté qu'une fois —
	//! AttackTick (début de l'assaut), UpdateRetaken
	protected int CountAttackBlue(SRP_FrontComponent front, int zone)
	{
		int blue = front.CountZoneBlue(zone);
		array<int> counted = {};
		int keys = front.GetKeyPointCount(zone);
		for (int k = 0; k < keys; k++)
		{
			int keyCell = front.GetKeyPointCell(zone, k);
			if (keyCell < 0 || counted.Contains(keyCell) || front.GetCellZone(keyCell) == zone)
				continue;
			counted.Insert(keyCell);
			if (front.GetCellOwner(keyCell) == SRP_EFrontOwner.BLEU)
				blue++;
		}
		return blue;
	}

	//------------------------------------------------------------------------------------------------
	//! Objectif d'un groupe (AdvanceAssault) : -2 = fouille du point visé — AdvanceAssault
	protected void SetObjective(SRP_EnemyGroup record, int slot, int cell, int nowUnix)
	{
		if (slot < 0)
		{
			m_Attack.m_aObjGroups.Insert(record);
			m_Attack.m_aObjCells.Insert(cell);
			m_Attack.m_aObjSince.Insert(nowUnix);
			return;
		}
		m_Attack.m_aObjCells[slot] = cell;
		m_Attack.m_aObjSince[slot] = nowUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! « Chotain (S12) et Morton (S14) » : les zones d'où partent les axes, pour la radio — SendWave
	protected string AxesText(SRP_FrontComponent front, array<vector> origins, int axes)
	{
		array<string> labels = {};
		for (int i = 0; i < axes && i < origins.Count(); i++)
		{
			int cell = front.CellIndexAt(origins[i]);
			if (cell < 0)
				continue;
			int zone = front.GetCellZone(cell);
			if (zone < 0)
				continue;
			string label = front.GetZoneLabel(zone);
			if (!labels.Contains(label))
				labels.Insert(label);
		}
		if (labels.IsEmpty())
			return "les lignes ennemies";
		string text = labels[0];
		for (int j = 1; j < labels.Count(); j++)
			text += " et " + labels[j];
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Angle (degrés, plan XZ) d'un point vu d'un centre — PickWaveOrigins
	protected static float AngleOf(vector center, vector point)
	{
		return Math.Atan2(point[2] - center[2], point[0] - center[0]) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! Écart entre deux angles, de 0 à 180 degrés — PickWaveOrigins
	protected static float AngleGap(float a, float b)
	{
		float gap = Math.AbsFloat(a - b);
		while (gap > 360)
			gap -= 360;
		if (gap > 180)
			gap = 360 - gap;
		return gap;
	}

	//------------------------------------------------------------------------------------------------
	//! Jour du serveur, « 2026-09-26 » — H2
	protected static string Today()
	{
		int year;
		int month;
		int day;
		System.GetYearMonthDay(year, month, day);
		return string.Format("%1-%2-%3", year, Pad2(month), Pad2(day));
	}

	//------------------------------------------------------------------------------------------------
	//! Deux chiffres — Today
	protected static string Pad2(int value)
	{
		if (value < 10)
			return "0" + value.ToString();
		return value.ToString();
	}
}
