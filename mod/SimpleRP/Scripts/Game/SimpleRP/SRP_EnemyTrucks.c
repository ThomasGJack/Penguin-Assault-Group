//------------------------------------------------------------------------------------------------
// SimpleRP — Camions de renfort de l'IA ennemie (carte #69, livraison 3 : « camions, radio »)
// SRP_EnemyTruckComponent (sur le game mode, serveur seulement), un passage toutes les 2 s. Remplace partout l'ancien
// renfort motorisé (plus de jeep laissée garée à 300 m).
// - CAMION D'ENTRÉE (SRP_TerritoryComponent) : un joueur arrive à m_fTruckEntryDistance du centre d'une localité ennemie
//   gardée, avec au moins m_iMinPlayers joueurs à moins de m_fPlayersRadius : un camion part pour arriver 2 à 4 min
//   plus tard. De 2 à 4 joueurs : 2 à 6 soldats ; dès m_iFullFromPlayers joueurs : plein (m_iCargoFull).
// - CAMION D'ALERTE, dès m_iFullFromPlayers joueurs seulement : la garnison a VU les joueurs, m_iAlertDelaySeconds plus
//   tard un camion plein est appelé par radio (SRP_TerritoryComponent.TryRadioCall : opérateur radio tué, appel retardé).
// - VAGUE DE MISSION (et contre-attaque sans secteur voisin) : SpawnWave garde le tirage d'aujourd'hui ; chaque camion
//   remplace un groupe à pied, porte des sections de 4 à 6 (4 soldats au moins, jamais de binôme), se gare à 150-250 m
//   du site, et tout le monde part à l'assaut. Mission finie avant l'arrivée : le camion repart (RecallGroups).
// - Déroulé : camion posé hors de vue, sur une route, à 1 000-1 800 m (2 500 m au plus), jamais près des joueurs ;
//   départ différé pour tenir l'heure d'arrivée visée. Chauffeur seul dans son groupe (il reste à bord, son IA n'est
//   jamais coupée au loin), soldats en passagers (embarquement réessayé, traînards retirés). Il roule par la route
//   jusqu'à un stationnement DANS la localité, à la vue ; tout le monde descend : moitié vers la dernière position
//   connue des joueurs, moitié en défense de la localité (ils rejoignent sa garnison) ; le camion repart par la route
//   et n'est retiré que hors de vue, loin des joueurs (jamais sous leurs yeux).
// - EMBUSCADE en route (camion touché ou un homme touché avec un joueur à portée de tir, feu nourri, joueur à moins de
//   m_fAmbushDistance) : arrêt, tout le monde saute là où le camion s'arrête et attaque ; le camion reste (nettoyage des
//   transports de l'IA ennemie). Un choc loin de tout joueur n'est pas une embuscade.
// - BLOCAGE (jamais à plus de 15 m de son point de référence) : trajet relancé, puis tout le monde descend sur place,
//   chauffeur compris, et la suite à pied ; renversé : débarquement sur place. Au retour : trajet relancé, puis le
//   chauffeur descend (il rejoint la garnison s'il est encore dans la localité) et le camion reste.
// - Garnison retirée, prise ou anéantie : un camion encore à l'embarquement ou en route repart (RecallSector).
// - Vu ou pas : le camion, ses pièces et ses occupants ne cachent pas la vue (SRP_Placement.IsVehicleSeenByAnyPlayer) ;
//   un joueur à bord ou à moins de 150 m le voit toujours.
// - FRONT (#75) : le départ est choisi sur un carré ROUGE (m_bOriginRedOnly ; faute de départ rouge, un second passage
//   sans ce filtre, noté au journal) ; la route, elle, peut traverser le bleu. Localités : SRP_TerritoryComponent
//   (GetLocalities), le point clé centre sert de centre (m_vTown).
// - COMMANDEUR (#76, SRP_CmdManeuvers) : renfort ou camion d'entrée vers une localité (DispatchFrom : départ cherché
//   d'abord sur la route de la localité source), camion de contact tout à l'assaut (SendContactTruck : groupes RENFORT,
//   retirés par le Commandeur sans contact), camion de colonne posé sur la route de la colonne (LaunchColumn, sans
//   attente, tout le monde en défense de la destination). Débit des envoyés (MA2) une seule fois, AU DÉPART d'un camion
//   hors colonne (fin de l'embarquement : OnTruckLaunched des seuls soldats assis ; traînards, camion annulé ou rappelé
//   à l'embarquement : rien n'est débité) ; une colonne est débitée à son départ par le Commandeur, et les soldats
//   demandés qui ne partent pas dans son camion lui sont rendus au départ (ReturnSent). Passagers débarqués ou camion
//   perdu : OnTruckUnloaded ; colonne livrée : OnColumnDelivered. Dès le départ, les passagers portent leur localité
//   source (m_sSourceLocality) : retirés vivants, ils y rentrent (SRP_EnemyComponent.RetireTick ; passagers d'un camion
//   rappelé ou annulé, supprimés sans avoir débarqué : ReturnSurvivors juste avant Delete).
// Place avant chaque pose (Q7) : SRP_CmdCapacity.Ask(« camion-… », COMBAT, soldats + chauffeur, groupes) décide seule des
// soldats (120, réserves, marge du jeu), en plus de 4 camions posés au plus et du plafond de groupes (hors ceux gardés
// pour les missions, sauf camion de vague) ; refusée, un camion moins chargé part si la place le permet (4 soldats au
// moins, jamais une colonne), sinon le départ est reporté (une ligne de journal). Chauffeur et passagers en classe
// COMBAT (SRP_CmdCapacity.Tag), GARNISON quand ils rejoignent une localité. L'IA du chauffeur n'est jamais coupée au loin
// (niveau de détail ramené sous le maximum) ; son état est écrit au journal à la pose et 10 s après le départ.
// Journal : catégorie ENNEMI seulement, aucun message aux joueurs.
//------------------------------------------------------------------------------------------------

//! Phases d'un camion de renfort (enchaînées en if/else dans SRP_EnemyTruckComponent.Tick)
enum SRP_ETruckPhase
{
	ATTENTE,		// prévu, pas encore posé : départ différé (heure d'arrivée visée), budget, ou départ hors de vue à trouver
	EMBARQUEMENT,	// camion posé hors de vue, chauffeur aux commandes, les soldats montent à bord
	ROUTE,			// en route vers le stationnement : embuscade, blocage, arrivée
	DEBARQUEMENT,	// arrêté : les soldats descendent
	DEPART,			// repart vers son point de départ ; retiré seulement hors de vue, loin des joueurs
	FINI			// terminé : sorti de la liste au passage suivant
}

//------------------------------------------------------------------------------------------------
//! Un camion de renfort. Les groupes sont gardés par « ref » (jamais de pointeur faible vers un SRP_EnemyGroup hors des
//! listes qui les possèdent) : un groupe retiré par l'IA ennemie garde ici son enregistrement, m_Group revenu à null.
//! La localité est retrouvée par son nom (SRP_TerritoryComponent.FindState), jamais gardée.
class SRP_EnemyTruck
{
	int m_iId;
	IEntity m_Vehicle;
	ref SRP_EnemyGroup m_Driver;
	ref array<ref SRP_EnemyGroup> m_aCargo = {};
	int m_iPhase = SRP_ETruckPhase.ATTENTE;
	vector m_vOrigin;			// départ, sur une route, hors de vue
	vector m_vPark;				// stationnement : dans la localité, ou à 150-250 m du site d'une vague
	vector m_vTown;				// centre de la localité, ou site visé par une vague
	float m_fTownRadius;		// rayon du secteur (0 pour une vague)
	string m_sSector;			// localité (camion de territoire) ; vide pour une vague
	string m_sOwner;			// propriétaire des groupes : « territoire » ou « mission »
	string m_sReason;			// « entrée », « alerte », « Staff », « vague »
	bool m_bAllAssault;			// vague : tout le monde à l'assaut
	bool m_bPlanned;			// départ, stationnement et route choisis
	ref array<vector> m_aRoute = {};
	float m_fRouteLength;
	int m_iLoad;				// soldats voulus à bord (chauffeur non compris)
	int m_iSeated;				// soldats assis à l'arrière au dernier passage de l'embarquement
	int m_iFitAtStart;			// soldats en état de combattre au départ, chauffeur compris (un homme touché en route)
	int m_iFreeSeats = -1;		// places passagers libres comptées à la pose (-1 : inconnu)
	int m_iEstimateMs;			// embarquement + trajet estimé
	int m_iCreatedTick;
	int m_iLaunchTick;			// pose prévue
	int m_iArriveTarget;		// arrivée visée (0 : aucune)
	int m_iArrivedTick;
	int m_iPhaseTick;			// début de la phase en cours
	int m_iNextPlanTick;		// prochain essai de choix du départ
	int m_iLastMoveTick;		// dernier déplacement de plus de 5 m
	int m_iStillSince;			// arrêté (moins de 1,5 m/s) depuis (0 : il roule)
	int m_iStrikes;				// blocages constatés en route
	int m_iOrderTick;			// débarquement ordonné à (0 : pas encore)
	int m_iExitTries;			// filet du débarquement : descentes forcées déjà demandées
	vector m_vLastPos;
	float m_fHealthStart = 1;
	bool m_bAmbushed;			// pris sous le feu en route : tout le monde a sauté, le camion reste
	bool m_bAbandon;			// renversé : débarquement sur place, le camion reste
	bool m_bJoined;				// débarqués rattachés à la garnison de la localité
	bool m_bWaitLogged;			// raison de l'attente déjà écrite au journal
	bool m_bLodLogged;			// état de l'IA du chauffeur écrit au journal 10 s après le départ

	// Front et Commandeur (#75, #76)
	string m_sSource;			// localité source des soldats (MA2), "" si aucune
	vector m_vSource;			// son point clé : départs cherchés d'abord sur la route source -> cible
	int m_iTargetZone = -1;		// zone visée (journal, demande de place)
	int m_iColumnId;			// colonne du Commandeur (0 : camion hors colonne)
	string m_sColumnTo;			// colonne : localité de destination ("" : vers un contact)
	int m_iColumnLoad;			// colonne : soldats demandés pour ce camion (débités par le Commandeur à la colonne)
	int m_iColumnDeparted = -1;	// colonne : soldats partis et encore comptés dans la colonne (SettleDeparture) ; -1 = départ pas réglé
	ref SRP_CmdColumn m_Column;	// colonne : relue à la livraison (sa destination est-elle encore créditée ?)
	int m_iSentId;				// numéro rendu par SRP_CmdManeuvers.OnTruckLaunched au départ (0 : rien d'inscrit)
	bool m_bDeparted;			// départ réglé (débit, ou retour à la colonne des soldats pas partis), une seule fois
	bool m_bContact;			// camion de contact du Commandeur (ou colonne vers un contact) : tout à l'assaut, groupes RENFORT
	bool m_bSectionsOnly;		// vague de mission : sections de 4 à 6 seulement, jamais de binôme
	bool m_bLaunched;			// posé (sa demande de place a été retirée par Ask)
	bool m_bLanded;				// ordre de débarquement donné (arrivée ou embuscade)
	bool m_bInGarrison;			// débarqués entrés dans la garnison posée de la localité (JoinTown réussi)
	bool m_bUnloadNoted;		// OnTruckUnloaded déjà appelé
	bool m_bDelivered;			// colonne : OnColumnDelivered déjà appelé
	bool m_bRedFallbackLogged;	// « aucun départ rouge » déjà écrit au journal
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Camions de renfort de l'IA ennemie : entrée dans une localité, alerte, vagues de mission")]
class SRP_EnemyTruckComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_EnemyTruckComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Camions de renfort : à l'entrée d'une localité ennemie gardée, sur alerte (dès 5 joueurs) et pour les vagues de mission (au tirage d'aujourd'hui). Décoché : aucun camion, les vagues viennent à pied.", category: "SimpleRP - Camions")]
	protected bool m_bEnabled;

	[Attribute("{16C1F16C9B053801}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et", UIWidgets.ResourceNamePicker, "Camion (Ural de transport du jeu : 1 chauffeur, 14 places passagers)", "et", category: "SimpleRP - Camions")]
	protected ResourceName m_sTruckPrefab;

	[Attribute("{0848BAB1B94FCBBA}Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Character_RHS_RF_MSV_VKPO_DS_Crew.et", UIWidgets.ResourceNamePicker, "Chauffeur (seul dans son groupe, il reste à bord)", "et", category: "SimpleRP - Camions")]
	protected ResourceName m_sDriverPrefab;

	[Attribute("14", UIWidgets.EditBox, "Places passagers du camion : le chargement ne les dépasse jamais (Ural de transport, relu dans les prefabs du jeu : 2 en cabine + 12 à l'arrière)", category: "SimpleRP - Camions")]
	protected int m_iCargoSeats;

	[Attribute("2", UIWidgets.EditBox, "Pas de camion à moins de … joueurs proches (MODE ESSAI du Workbench : 1)", category: "SimpleRP - Camions (chargement)")]
	protected int m_iMinPlayers;

	[Attribute("5", UIWidgets.EditBox, "Dès … joueurs proches : camions pleins, et camion d'alerte en plus du camion d'entrée (en dessous, pas de camion d'alerte)", category: "SimpleRP - Camions (chargement)")]
	protected int m_iFullFromPlayers;

	[Attribute("10", UIWidgets.EditBox, "Camion plein : soldats à bord (chauffeur non compris)", category: "SimpleRP - Camions (chargement)")]
	protected int m_iCargoFull;

	[Attribute("2", UIWidgets.EditBox, "Moins de joueurs : … soldats par joueur au-delà du premier (2 joueurs : 2, 3 joueurs : 4, 4 joueurs : 6)", category: "SimpleRP - Camions (chargement)")]
	protected int m_iSmallPerPlayer;

	[Attribute("2", UIWidgets.EditBox, "… au moins … soldats", category: "SimpleRP - Camions (chargement)")]
	protected int m_iSmallMin;

	[Attribute("6", UIWidgets.EditBox, "… et au plus … soldats (c'est aussi le camion « petit » forcé par le Staff)", category: "SimpleRP - Camions (chargement)")]
	protected int m_iSmallMax;

	[Attribute("4", UIWidgets.EditBox, "Moins de … soldats à bord, ou un seul groupe : tous à l'assaut (sinon moitié vers les joueurs, moitié en défense)", category: "SimpleRP - Camions (chargement)")]
	protected int m_iSplitMin;

	[Attribute("1000", UIWidgets.EditBox, "Joueurs proches d'une localité : comptés à moins de … mètres de son centre (hors délai de grâce)", category: "SimpleRP - Camions (chargement)")]
	protected float m_fPlayersRadius;

	[Attribute("500", UIWidgets.EditBox, "Camion d'entrée : envoyé quand un joueur arrive à moins de … mètres du centre d'une localité ennemie gardée", category: "SimpleRP - Camions (horaires)")]
	protected float m_fTruckEntryDistance;

	[Attribute("120", UIWidgets.EditBox, "Camion d'entrée : arrivée visée au plus tôt … secondes après l'entrée du joueur", category: "SimpleRP - Camions (horaires)")]
	protected int m_iArriveMinSeconds;

	[Attribute("240", UIWidgets.EditBox, "… et au plus tard … secondes après", category: "SimpleRP - Camions (horaires)")]
	protected int m_iArriveMaxSeconds;

	[Attribute("8", UIWidgets.EditBox, "Vitesse moyenne estimée d'un camion, en mètres par seconde (pour tenir l'heure d'arrivée ; la route réelle est plus sinueuse que la ligne droite)", category: "SimpleRP - Camions (horaires)")]
	protected float m_fDriveSpeedEstimate;

	[Attribute("30", UIWidgets.EditBox, "Camion d'alerte : appelé par radio … secondes après que la garnison a VU les joueurs (dès 5 joueurs)", category: "SimpleRP - Camions (horaires)")]
	protected int m_iAlertDelaySeconds;

	[Attribute("800", UIWidgets.EditBox, "Départ : sur une route, à … mètres au moins de la localité (800 : une route d'environ 900 m, 2 min de trajet embarquement compris, pour tenir une arrivée à 2 min ; toujours hors de vue et loin des joueurs)", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fSpawnMin;

	[Attribute("1800", UIWidgets.EditBox, "… et à … mètres au plus (de quoi arriver 2 à 4 min plus tard)", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fSpawnMax;

	[Attribute("2500", UIWidgets.EditBox, "Si rien ne convient dans cet anneau : jusqu'à … mètres", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fSpawnFallbackMax;

	[Attribute("700", UIWidgets.EditBox, "Départ : jamais à moins de … mètres d'un joueur, et toujours hors de vue", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fSpawnPlayerMin;

	[Attribute("1", UIWidgets.CheckBox, "Front : départ seulement sur un carré rouge (faute de départ rouge, un second passage sans ce filtre, noté au journal) ; la route, elle, peut traverser le bleu", category: "SimpleRP - Camions (départ et stationnement)")]
	protected bool m_bOriginRedOnly;

	[Attribute("0.5", UIWidgets.EditBox, "Stationnement : sur une route à moins de … × le rayon du secteur du centre, à la vue", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fParkFactor;

	[Attribute("100", UIWidgets.EditBox, "Stationnement : jamais prévu à moins de … mètres d'un joueur (sinon à 0,8 × le rayon, sinon à l'entrée de ce côté)", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fParkPlayerMin;

	[Attribute("150", UIWidgets.EditBox, "Vague de mission : stationnement à … mètres du site au moins…", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fWaveParkMin;

	[Attribute("250", UIWidgets.EditBox, "… et au plus", category: "SimpleRP - Camions (départ et stationnement)")]
	protected float m_fWaveParkMax;

	[Attribute("50", UIWidgets.EditBox, "Vitesse sur la route, en km/h", category: "SimpleRP - Camions (conduite)")]
	protected int m_iRoadSpeed;

	[Attribute("25", UIWidgets.EditBox, "Vitesse sur le dernier tronçon, dans la localité, en km/h", category: "SimpleRP - Camions (conduite)")]
	protected int m_iTownSpeed;

	[Attribute("20", UIWidgets.EditBox, "Embarquement : … secondes au plus (la file d'apparition livre les soldats un par un) ; ceux encore au sol sont retirés (pas des pertes)", category: "SimpleRP - Camions (conduite)")]
	protected int m_iBoardSeconds;

	[Attribute("30", UIWidgets.EditBox, "Blocage : jamais à plus de 15 m de son point de référence pendant … secondes (un camion qui avance et recule sur place est bloqué) ; en route, la 1re fois le trajet est relancé, la 2e fois tout le monde descend sur place (camion laissé) ; au retour, la 1re fois le trajet est relancé, la 2e fois le chauffeur descend (camion laissé)", category: "SimpleRP - Camions (conduite)")]
	protected int m_iStuckSeconds;

	[Attribute("300", UIWidgets.EditBox, "Route : au-delà de … secondes (ou du double du trajet estimé s'il est plus long), débarquement sur place", category: "SimpleRP - Camions (conduite)")]
	protected int m_iMaxDriveSeconds;

	[Attribute("0.3", UIWidgets.EditBox, "Embuscade : le chauffeur ou un passager sous un feu nourri (suppression au-delà de …, de 0 à 1) : arrêt, tout le monde saute et attaque", category: "SimpleRP - Camions (embuscade)")]
	protected float m_fAmbushSuppression;

	[Attribute("30", UIWidgets.EditBox, "Embuscade : un joueur à moins de … mètres du camion", category: "SimpleRP - Camions (embuscade)")]
	protected float m_fAmbushDistance;

	[Attribute("800", UIWidgets.EditBox, "Embuscade : le camion touché ou un homme touché ne comptent que si un joueur est à moins de … mètres (un choc contre un arbre n'est pas une embuscade : sans joueur à cette distance, la santé et l'effectif de référence sont repris à chaque passage)", category: "SimpleRP - Camions (embuscade)")]
	protected float m_fAmbushFireRange;

	[Attribute("0.5", UIWidgets.EditBox, "Débarquement : la moitié en défense tient … × le rayon du secteur autour du centre", category: "SimpleRP - Camions (débarquement)")]
	protected float m_fDefenseRadiusFactor;

	[Attribute("800", UIWidgets.EditBox, "Débarquement : sans alerte récente, l'assaut part vers le joueur le plus proche à moins de … mètres (sinon le centre)", category: "SimpleRP - Camions (débarquement)")]
	protected float m_fAssaultSearchRadius;

	[Attribute("50", UIWidgets.EditBox, "… à … mètres près (point décalé au hasard autour de lui)", category: "SimpleRP - Camions (débarquement)")]
	protected float m_fAssaultOffset;

	[Attribute("600", UIWidgets.EditBox, "Retour : le camion vide est retiré à plus de … mètres de tout joueur, et hors de vue (ou revenu à son départ, hors de vue)", category: "SimpleRP - Camions (disparition et budget)")]
	protected float m_fVanishDistance;

	[Attribute("10", UIWidgets.EditBox, "Retour : au plus tard … minutes après son départ, dès qu'il n'est plus vu", category: "SimpleRP - Camions (disparition et budget)")]
	protected int m_iVanishMaxMinutes;

	[Attribute("4", UIWidgets.EditBox, "Camions posés en même temps, au plus (de l'embarquement au retour)", category: "SimpleRP - Camions (disparition et budget)")]
	protected int m_iMaxActiveTrucks;

	[Attribute("10", UIWidgets.EditBox, "Départ reporté (budget, aucun départ hors de vue) : le camion est abandonné après … minutes", category: "SimpleRP - Camions (disparition et budget)")]
	protected int m_iWaitMaxMinutes;

	protected static const int TICK_MS = 2000;				// un passage toutes les 2 s
	protected static const int PLAN_RETRY_MS = 10000;		// aucun départ hors de vue : nouvel essai toutes les 10 s
	protected static const int MAX_ROUTES = 4;				// départs dont la route est calculée, au plus, par anneau (les mieux placés à vol d'oiseau d'abord)
	protected static const float STUCK_RADIUS = 15;			// blocage : jamais à plus de … m de son point de référence pendant m_iStuckSeconds
	protected static const float ROUTE_FACTOR = 1.25;		// route réelle ≈ … × la distance à vol d'oiseau (tri des départs avant le calcul des routes)
	protected static const int WAVE_MIN_LOAD = 4;			// vague de mission : 4 soldats au moins à bord (une section)
	protected static const float FORCE_RANGE = 3000;		// Staff : localité ennemie gardée à moins de … mètres

	protected static SRP_EnemyTruckComponent s_Instance;

	protected ref array<ref SRP_EnemyTruck> m_aTrucks = {};
	protected int m_iNextId;
	protected int m_iSent;		// camions posés depuis le démarrage

	//------------------------------------------------------------------------------------------------
	static SRP_EnemyTruckComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		s_Instance = this;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
			GetGame().GetCallqueue().Remove(Tick);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Réglages lus par le territoire et l'IA ennemie
	//------------------------------------------------------------------------------------------------
	bool IsEnabled()
	{
		return m_bEnabled && !m_sTruckPrefab.IsEmpty() && !m_sDriverPrefab.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	int GetMinPlayers()
	{
		return m_iMinPlayers;
	}

	//------------------------------------------------------------------------------------------------
	int GetFullFromPlayers()
	{
		return m_iFullFromPlayers;
	}

	//------------------------------------------------------------------------------------------------
	float GetEntryDistance()
	{
		return m_fTruckEntryDistance;
	}

	//------------------------------------------------------------------------------------------------
	int GetAlertDelayMs()
	{
		return Math.ClampInt(m_iAlertDelaySeconds, 0, 3600) * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs vivants de « players » à moins de m_fPlayersRadius de ce centre (le territoire passe les joueurs actifs :
	//! hors délai de grâce)
	int CountPlayersAround(vector center, array<IEntity> players)
	{
		int count = 0;
		if (!players)
			return 0;
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;
			if (vector.Distance(player.GetOrigin(), center) <= m_fPlayersRadius)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	// Entrées publiques : territoire, vagues, Staff
	//------------------------------------------------------------------------------------------------
	//! Camion de territoire pour la localité « state » : « entrée » (arrivée visée entre m_iArriveMinSeconds et
	//! m_iArriveMaxSeconds après l'entrée du joueur, state.m_iEntryTick ; chargement selon « players ») ou « alerte »
	//! (plein, départ aussitôt). Le camion est posé au passage suivant, ou plus tard (départ différé, budget). Faux si les
	//! camions sont coupés ou si la localité a déjà un camion pour cette raison.
	bool Dispatch(SRP_SectorState state, int players, string reason)
	{
		if (!IsEnabled() || !state || !state.m_Sector)
			return false;
		if (HasTruckFor(state.m_sName, reason))
			return false;
		int now = System.GetTickCount();
		SRP_EnemyTruck truck = NewTruck(state.m_sName, "territoire", reason, now);
		truck.m_vTown = state.m_Sector.GetCenter();
		truck.m_fTownRadius = state.m_Sector.m_fRadius;
		truck.m_iTargetZone = state.m_Sector.m_iZone;
		int load = m_iCargoFull;
		if (reason != "alerte")
			load = LoadFor(players);
		truck.m_iLoad = CapLoad(load);
		if (reason == "entrée")
		{
			int entry = state.m_iEntryTick;
			if (entry <= 0)
				entry = now;
			int minSeconds = Math.ClampInt(m_iArriveMinSeconds, 0, 3600);
			int maxSeconds = Math.ClampInt(m_iArriveMaxSeconds, minSeconds, 3600);
			truck.m_iArriveTarget = entry + Math.RandomIntInclusive(minSeconds, maxSeconds) * 1000;
		}
		TruckLog(string.Format("%1 : demandé, %2 joueur(s) à moins de %3 m, %4 soldats à bord prévus", TruckLabel(truck), players, Math.Round(m_fPlayersRadius), truck.m_iLoad));
		if (PlanTruck(truck, SRP_Utils.GetPlayerCharacters(), now))
			LogPlan(truck, now);
		else
			truck.m_iNextPlanTick = now + PLAN_RETRY_MS;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Entrées du Commandeur (SRP_CmdManeuvers)
	//------------------------------------------------------------------------------------------------
	//! Renfort vers une localité ennemie (« renfort ») ou camion d'entrée #69 (« entrée ») du Commandeur : comme
	//! Dispatch, mais chargement imposé (CapLoad(load)) et arrivée visée « arriveSeconds » après la demande (tick interne
	//! en ms : il ne sert qu'à tenir l'heure pendant la session) ; départ cherché d'abord sur la route de la localité
	//! source (sourcePos) vers la localité, puis comme aujourd'hui. La source est débitée au départ, des soldats assis
	//! (SettleDeparture : OnTruckLaunched). Faux si les camions sont coupés, si la localité n'est plus ennemie, ou si elle
	//! a déjà son camion d'entrée.
	bool DispatchFrom(SRP_SectorState state, string source, vector sourcePos, int load, int arriveSeconds, string reason)
	{
		if (!IsEnabled() || !state || !state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI)
			return false;
		if (reason == "entrée" && HasTruckFor(state.m_sName, reason))
			return false;
		int now = System.GetTickCount();
		int seconds = Math.ClampInt(arriveSeconds, 0, 7200);
		SRP_EnemyTruck truck = NewTruck(state.m_sName, "territoire", reason, now);
		truck.m_vTown = state.m_Sector.GetCenter();
		truck.m_fTownRadius = state.m_Sector.m_fRadius;
		truck.m_iTargetZone = state.m_Sector.m_iZone;
		truck.m_iLoad = CapLoad(load);
		truck.m_sSource = source;
		truck.m_vSource = sourcePos;
		truck.m_iArriveTarget = now + seconds * 1000;
		TruckLog(string.Format("%1 : demandé, %2 soldats à bord prévus, arrivée visée dans %3 s", TruckLabel(truck), truck.m_iLoad, seconds));
		if (PlanTruck(truck, SRP_Utils.GetPlayerCharacters(), now))
			LogPlan(truck, now);
		else
			truck.m_iNextPlanTick = now + PLAN_RETRY_MS;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Camion de contact du Commandeur (MA1) : tout le monde à l'assaut, comme un camion de vague (garé à 150-250 m du
	//! contact, propriétaire « territoire »), départ différé pour arriver « arriveSeconds » après la demande, d'abord sur
	//! la route de la localité source. Ses passagers sont des groupes RENFORT (m_iCmdRole, hors de l'entraide), portent
	//! leur source et ne font partie d'aucune contre-attaque (m_iAssaultZone = -1) ; une fois débarqués, le Commandeur
	//! les retire après renfort_retrait_min sans contact (SRP_CmdManeuvers.ContactGroupsTick). Faux si les camions sont
	//! coupés.
	bool SendContactTruck(vector contact, string source, vector sourcePos, int load, int arriveSeconds, int zone)
	{
		if (!IsEnabled())
			return false;
		int now = System.GetTickCount();
		int seconds = Math.ClampInt(arriveSeconds, 0, 7200);
		SRP_EnemyTruck truck = NewTruck("", "territoire", "contact", now);
		truck.m_vTown = SRP_Placement.OnGround(contact);
		truck.m_fTownRadius = 0;
		truck.m_bAllAssault = true;
		truck.m_bContact = true;
		truck.m_iTargetZone = zone;
		truck.m_iLoad = CapLoad(load);
		truck.m_sSource = source;
		truck.m_vSource = sourcePos;
		truck.m_iArriveTarget = now + seconds * 1000;
		TruckLog(string.Format("%1 : demandé, %2 soldats à bord prévus, arrivée visée dans %3 s (grille %4 / %5)", TruckLabel(truck), truck.m_iLoad, seconds, Math.Round(contact[0]), Math.Round(contact[2])));
		if (PlanTruck(truck, SRP_Utils.GetPlayerCharacters(), now))
			LogPlan(truck, now);
		else
			truck.m_iNextPlanTick = now + PLAN_RETRY_MS;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Colonne du Commandeur qui passe au réel (MA4) : un camion de « load » soldats posé à « at », sur la route de la
	//! colonne (revérifié : hors de vue, loin des joueurs), sans attente ; route = « remaining » (points déjà dépassés et
	//! au-delà du stationnement retirés), stationnement vers le point clé de la destination. Au débarquement : tout le
	//! monde en défense de la destination, puis dans sa garnison si elle est posée ; colonne vers un contact : tout à
	//! l'assaut, groupes RENFORT. Jamais de débit ici (la colonne l'a été à son départ) ; les soldats demandés (« load »)
	//! qui ne partent pas dans ce camion (places, arrondi au pair, prefab, traînards, camion annulé) sont rendus à la
	//! source au départ du camion (SettleDeparture). Rend le nombre de camions posés (1), 0 si refusé (destination prise,
	//! départ vu ou trop près des joueurs, place, 4 camions posés) : la colonne reste sur le papier et réessaie. Moins de
	//! 2 soldats : refusé (CapLoad et PlanCargo poseraient un binôme, un soldat de plus que le débit de la colonne) ; ce
	//! soldat finit la route sur le papier (SRP_CmdManeuvers.ColumnsTick : ArriveOnPaper).
	int LaunchColumn(SRP_CmdColumn column, vector at, array<vector> remaining, int load)
	{
		if (!IsEnabled() || !column || load < 2)
			return 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return 0;
		SRP_SectorState state;
		if (!column.m_sTo.IsEmpty())
		{
			state = FindLocality(column.m_sTo);
			if (!state || !state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI)
				return 0;	// destination inconnue ou prise : la colonne reste sur le papier (le Commandeur la réoriente)
		}
		int now = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		vector origin = SRP_Placement.OnGround(at);
		if (!ColumnOriginUsable(origin, players))
			return 0;

		SRP_EnemyTruck truck = NewTruck(column.m_sTo, "territoire", "colonne", now);
		truck.m_iColumnId = column.m_iId;
		truck.m_sColumnTo = column.m_sTo;
		truck.m_iColumnLoad = load;
		truck.m_Column = column;
		truck.m_sSource = column.m_sFrom;
		truck.m_iTargetZone = column.m_iZone;
		truck.m_iLoad = CapLoad(load);
		if (state)
		{
			truck.m_vTown = state.m_Sector.GetCenter();
			truck.m_fTownRadius = state.m_Sector.m_fRadius;
			if (truck.m_iTargetZone < 0)
				truck.m_iTargetZone = state.m_Sector.m_iZone;
		}
		else
		{
			truck.m_vTown = SRP_Placement.OnGround(column.m_vContact);
			truck.m_fTownRadius = 0;
			truck.m_bAllAssault = true;
			truck.m_bContact = true;
		}
		truck.m_vOrigin = origin;

		// Stationnement vers la destination (à défaut, son centre) ; route : le reste de la route de la colonne
		vector park;
		if (!FindPark(truck, origin, players, park))
			park = SRP_Placement.OnGround(truck.m_vTown);
		truck.m_vPark = park;
		float parkFromTown = vector.DistanceXZ(park, truck.m_vTown);
		if (remaining)
		{
			foreach (vector step : remaining)
			{
				if (vector.DistanceXZ(step, origin) < 30)
					continue;	// déjà sur place
				if (vector.DistanceXZ(step, truck.m_vTown) <= parkFromTown + 30)
					continue;	// au-delà du stationnement
				truck.m_aRoute.Insert(step);
			}
		}
		truck.m_fRouteLength = RouteLength(origin, truck.m_aRoute, park);
		truck.m_iEstimateMs = EstimateMs(truck.m_fRouteLength);
		truck.m_iLaunchTick = now;
		truck.m_bPlanned = true;

		// Place (la demande refusée reste notée : la colonne réessaie), puis la pose tout de suite
		string refusal = LaunchRefusal(enemies, truck);
		if (!refusal.IsEmpty())
		{
			m_aTrucks.RemoveItem(truck);
			return 0;
		}
		if (!LaunchNow(enemies, truck, players, now))
		{
			DropTruck(truck);
			return 0;
		}
		TruckLog(string.Format("%1 : colonne n°%2 de %3 passée au réel, posé sur sa route à %4 m de sa destination, route de %5 m", TruckLabel(truck), column.m_iId, column.m_sFrom, Math.Round(vector.DistanceXZ(origin, truck.m_vTown)), Math.Round(truck.m_fRouteLength)));
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Vague de mission (ou contre-attaque sans secteur voisin), au tirage de SpawnWave : un camion (deux dès
	//! m_iFullFromPlayers joueurs, au plus « maxTrucks »), chargé selon « players » mais jamais moins de 4 soldats, en
	//! sections de 4 à 6 seulement (décision de Jack : vagues de mission en sections de 4 à 6, jamais de binôme), posé et
	//! parti tout de suite ; il se gare à 150-250 m de « target » et tout le monde part à l'assaut. Les groupes de
	//! passagers entrent dans outGroups dès la pose (le chauffeur n'y entre pas : il repart ; s'il saute, il est retiré
	//! plus tard, hors de vue). Retourne le nombre de camions partis (chacun remplace un groupe à pied de la vague).
	int SendWaveTruck(vector target, string owner, int players, int maxTrucks, out array<ref SRP_EnemyGroup> outGroups)
	{
		if (!IsEnabled() || maxTrucks <= 0)
			return 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return 0;
		int wanted = 1;
		if (players >= m_iFullFromPlayers)
			wanted = 2;
		if (wanted > maxTrucks)
			wanted = maxTrucks;
		array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
		int now = System.GetTickCount();
		int launched = 0;
		for (int i = 0; i < wanted; i++)
		{
			SRP_EnemyTruck truck = NewTruck("", owner, "vague", now);
			truck.m_vTown = target;
			truck.m_fTownRadius = 0;
			truck.m_bAllAssault = true;
			truck.m_bSectionsOnly = true;
			truck.m_iTargetZone = ZoneAt(target);
			truck.m_iLoad = CapLoad(Math.Max(LoadFor(players), WAVE_MIN_LOAD));	// sections de 4 à 6 : 4 au moins
			string refusal = "";
			if (!PlanTruck(truck, everyone, now))
				refusal = "aucun départ hors de vue sur une route";
			else
				refusal = LaunchRefusal(enemies, truck);
			if (refusal.IsEmpty() && !LaunchNow(enemies, truck, everyone, now))
				refusal = "pose impossible";
			if (!refusal.IsEmpty())
			{
				TruckLog(TruckLabel(truck) + " : pas de camion pour cette vague (" + refusal + "), elle vient à pied");
				DropTruck(truck);	// la vague vient à pied : sa demande de place ne doit pas rester
				break;
			}
			foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
				outGroups.Insert(cargo);
			launched++;
		}
		return launched;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : un camion vers la localité ennemie gardée (garnison en état de combattre) la plus proche de « around », à
	//! moins de 3 km ; plein (m_iCargoFull) ou petit (m_iSmallMax). Joueurs et délais ignorés (départ aussitôt), mais les
	//! règles de pose, de vue et de budget restent. Réponse en français : localité, distance de départ, ou raison du refus.
	string ForceTruck(vector around, bool full, string author)
	{
		if (!IsEnabled())
			return "Camions de renfort désactivés (SRP_EnemyTruckComponent)";
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!enemies || !territory)
			return "IA ennemie ou territoire absents du game mode";
		array<SRP_SectorState> states = {};
		territory.GetLocalities(states);
		SRP_SectorState target;
		float best = FORCE_RANGE;
		foreach (SRP_SectorState candidate : states)
		{
			if (!candidate || !candidate.m_Sector || candidate.m_iOwner != SRP_ESectorOwner.ENNEMI || candidate.m_aGroups.IsEmpty())
				continue;
			if (SRP_EnemyComponent.ActiveAgentsNear(candidate.m_aGroups, candidate.m_Sector.GetCenter(), 0) == 0)
				continue;
			float distance = vector.Distance(candidate.m_Sector.GetCenter(), around);
			if (distance > best)
				continue;
			best = distance;
			target = candidate;
		}
		if (!target)
			return "Aucune localité ennemie gardée (garnison en place) à moins de 3 km";

		int now = System.GetTickCount();
		SRP_EnemyTruck truck = NewTruck(target.m_sName, "territoire", "Staff", now);
		truck.m_vTown = target.m_Sector.GetCenter();
		truck.m_fTownRadius = target.m_Sector.m_fRadius;
		truck.m_iTargetZone = target.m_Sector.m_iZone;
		int load = m_iSmallMax;
		if (full)
			load = m_iCargoFull;
		truck.m_iLoad = CapLoad(load);
		array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
		if (!PlanTruck(truck, everyone, now))
		{
			DropTruck(truck);
			return string.Format("%1 : aucun départ possible (route hors de vue entre %2 et %3 m, à %4 m au moins des joueurs)", target.m_sName, Math.Round(m_fSpawnMin), Math.Round(Math.Max(m_fSpawnFallbackMax, m_fSpawnMax)), Math.Round(m_fSpawnPlayerMin));
		}
		string refusal = LaunchRefusal(enemies, truck);
		if (!refusal.IsEmpty())
		{
			DropTruck(truck);	// camion forcé refusé : sa demande de place ne doit pas rester
			return target.m_sName + " : camion refusé, " + refusal;
		}
		if (!LaunchNow(enemies, truck, everyone, now))
		{
			DropTruck(truck);
			return target.m_sName + " : camion impossible à poser (voir le journal ENNEMI)";
		}
		TruckLog(TruckLabel(truck) + " : forcé par " + author);
		return string.Format("Camion n°%1 de %2 soldats vers %3 : départ à %4 m de la localité, route de %5 m, arrivée estimée dans %6 s", truck.m_iId, truck.m_iLoad, target.m_sName, Math.Round(vector.DistanceXZ(truck.m_vOrigin, truck.m_vTown)), Math.Round(truck.m_fRouteLength), truck.m_iEstimateMs / 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! La garnison de « sector » est retirée, prise ou anéantie (SRP_TerritoryComponent.OnGarrisonGone) : un camion pas
	//! encore posé est annulé ; un camion à l'embarquement ou en route repart avec ses soldats (retirés avec lui, hors de
	//! vue) ; un camion qui débarque ou repart déjà n'est pas touché. Un camion de colonne n'est jamais rappelé : il va au
	//! bout de sa route et rend compte au Commandeur (OnColumnDelivered).
	void RecallSector(string sector)
	{
		if (sector.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		int now = System.GetTickCount();
		foreach (SRP_EnemyTruck truck : m_aTrucks)
		{
			if (!truck || truck.m_sSector != sector || truck.m_iColumnId != 0)
				continue;
			if (truck.m_iPhase == SRP_ETruckPhase.ATTENTE)
			{
				truck.m_iPhase = SRP_ETruckPhase.FINI;
				TruckLog(TruckLabel(truck) + " : annulé avant le départ (garnison de la localité retirée, prise ou anéantie)");
				continue;
			}
			if (enemies)
				RecallTruck(enemies, truck, now, "garnison de la localité retirée, prise ou anéantie");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Une mission finit (SRP_MissionManagerComponent.Cleanup, avant RetireLater de ses groupes) : un camion de vague dont
	//! un groupe de passagers est dans « groups », encore à l'embarquement ou en route, repart avec ses soldats (retirés
	//! avec lui, hors de vue) au lieu d'aller débarquer une vague après la fin de la mission
	void RecallGroups(array<ref SRP_EnemyGroup> groups)
	{
		if (!groups || groups.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		int now = System.GetTickCount();
		foreach (SRP_EnemyTruck truck : m_aTrucks)
		{
			if (!truck || !truck.m_sSector.IsEmpty())
				continue;
			bool ours = false;
			foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
			{
				if (cargo && groups.Contains(cargo))
					ours = true;
			}
			if (ours)
				RecallTruck(enemies, truck, now, "mission terminée");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rappel d'un camion : à l'embarquement (traînards retirés ; pas encore parti, rien n'est débité) ou en route, il
	//! repart avec ses passagers ; plus tard, rien ne change
	protected void RecallTruck(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, int now, string why)
	{
		if (truck.m_iPhase == SRP_ETruckPhase.EMBARQUEMENT)
		{
			DropStragglers(enemies, truck);
			SettleDeparture(truck, 0);
			StartLeaving(enemies, truck, now, "rappelé à l'embarquement : " + why);
		}
		else if (truck.m_iPhase == SRP_ETruckPhase.ROUTE)
			StartLeaving(enemies, truck, now, "rappelé en route : " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à zéro de l'IA ennemie (Staff) : les camions sont supprimés (leurs groupes le sont par l'IA ennemie) ; un
	//! camion hors colonne pas encore parti sort des demandes en attente de sa source (SettleDeparture à 0 : rien de
	//! débité), un camion parti sort du registre du Commandeur (OnTruckUnloaded, rien n'est rendu) ; les colonnes ne sont
	//! pas signalées (le Commandeur annule et rend les siennes : SRP_Commander.CancelOperations) ; demandes de place
	//! retirées
	void ResetAll()
	{
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		foreach (SRP_EnemyTruck truck : m_aTrucks)
		{
			if (!truck)
				continue;
			if (truck.m_iColumnId == 0)
				SettleDeparture(truck, 0);	// déjà parti : sans effet
			NoteUnloaded(truck);
			capacity.ClearDemand(DemandKey(truck));
			if (truck.m_Vehicle && !truck.m_Vehicle.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(truck.m_Vehicle);
		}
		m_aTrucks.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Staff (page « État des groupes ») : les camions en cours autour de « from » (phase, assis/voulus, départ prévu,
	//! arrivée visée et réelle, distance). Jamais montré aux joueurs.
	string GetStatusText(vector from, float radius)
	{
		int now = System.GetTickCount();
		string text = "";
		foreach (SRP_EnemyTruck truck : m_aTrucks)
		{
			if (!truck || truck.m_iPhase == SRP_ETruckPhase.FINI)
				continue;
			vector where = truck.m_vTown;
			if (truck.m_Vehicle && !truck.m_Vehicle.IsDeleted())
				where = truck.m_Vehicle.GetOrigin();
			float distance = vector.Distance(from, where);
			if (distance > radius && vector.Distance(from, truck.m_vTown) > radius)
				continue;
			if (!text.IsEmpty())
				text += "\n";
			text += TruckLine(truck, distance, now);
		}
		if (text.IsEmpty())
			return string.Format("Camions : aucun en cours à moins de %1 m (%2 posé(s) depuis le démarrage)", Math.Round(radius), m_iSent);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Passage toutes les 2 s : une méthode par phase, enchaînées en if/else
	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (m_aTrucks.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();	// tous : même un joueur en délai de grâce ne doit rien voir apparaître ni disparaître
		int now = System.GetTickCount();
		for (int i = m_aTrucks.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyTruck truck = m_aTrucks[i];
			if (!truck)
			{
				m_aTrucks.Remove(i);
				continue;
			}
			if (truck.m_iPhase == SRP_ETruckPhase.ATTENTE)
				TickWaiting(enemies, truck, players, now);
			else if (truck.m_iPhase == SRP_ETruckPhase.EMBARQUEMENT)
				TickBoarding(enemies, truck, players, now);
			else if (truck.m_iPhase == SRP_ETruckPhase.ROUTE)
				TickRoute(enemies, truck, players, now);
			else if (truck.m_iPhase == SRP_ETruckPhase.DEBARQUEMENT)
				TickDismount(enemies, truck, now);
			else if (truck.m_iPhase == SRP_ETruckPhase.DEPART)
				TickLeaving(enemies, truck, players, now);
			if (truck.m_iPhase == SRP_ETruckPhase.FINI)
			{
				EndTruck(enemies, truck);
				m_aTrucks.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fin d'un camion (sorti de la liste) : départ réglé s'il ne l'a jamais été (filet : rien n'est parti ; camion hors
	//! colonne jamais posé, abandonné en ATTENTE : sa demande en attente sort du frein C2 de sa source) ; le Commandeur
	//! est prévenu une seule fois — camion hors colonne déchargé ou perdu (OnTruckUnloaded) ; colonne jamais livrée :
	//! aucun survivant (ses soldats encore vivants, retirés plus tard, rentrent à leur source par le retrait) ; un camion
	//! jamais posé retire sa demande de place
	protected void EndTruck(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		// Jamais pour une colonne non posée : SettleDeparture rendrait alors à tort ses soldats demandés (m_iColumnLoad)
		if (truck.m_bLaunched || truck.m_iColumnId == 0)
			SettleDeparture(truck, 0);
		NoteUnloaded(truck);
		if (truck.m_iColumnId != 0 && truck.m_bLaunched && !truck.m_bDelivered)
			DeliverColumn(enemies, truck, 0);
		if (!truck.m_bLaunched)
			SRP_CmdCapacity.Get().ClearDemand(DemandKey(truck));
	}

	//------------------------------------------------------------------------------------------------
	//! ATTENTE : la localité doit rester ennemie et gardée (renfort du Commandeur : ennemie seulement, il vient justement
	//! tenir une localité qui s'épuise) ; départ choisi (nouvel essai toutes les 10 s) ; à l'heure de départ : place
	//! (sinon reporté, une ligne au journal), départ encore hors de vue et loin des joueurs (sinon un autre), puis la pose.
	//! Abandonné après m_iWaitMaxMinutes.
	protected void TickWaiting(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		int waitMs = Math.ClampInt(m_iWaitMaxMinutes, 1, 120) * 60000;
		if (!truck.m_sSector.IsEmpty())
		{
			if (truck.m_sReason == "renfort" && !IsSectorHostile(truck.m_sSector))
			{
				TruckLog(TruckLabel(truck) + " : annulé avant le départ (la localité n'est plus ennemie)");
				truck.m_iPhase = SRP_ETruckPhase.FINI;
				return;
			}
			if (truck.m_sReason != "renfort" && !IsSectorHeld(truck.m_sSector))
			{
				TruckLog(TruckLabel(truck) + " : annulé avant le départ (la localité n'a plus de garnison en état de combattre)");
				truck.m_iPhase = SRP_ETruckPhase.FINI;
				return;
			}
		}
		if (!truck.m_bPlanned)
		{
			if (now < truck.m_iNextPlanTick)
				return;
			truck.m_iNextPlanTick = now + PLAN_RETRY_MS;
			if (!PlanTruck(truck, players, now))
			{
				if (now - truck.m_iCreatedTick > waitMs)
				{
					TruckLog(TruckLabel(truck) + " : abandonné, aucun départ hors de vue sur une route");
					truck.m_iPhase = SRP_ETruckPhase.FINI;
				}
				else if (!truck.m_bWaitLogged)
				{
					truck.m_bWaitLogged = true;
					TruckLog(string.Format("%1 : aucun départ hors de vue pour l'instant (route entre %2 et %3 m, à %4 m au moins des joueurs), nouvel essai toutes les %5 s", TruckLabel(truck), Math.Round(m_fSpawnMin), Math.Round(Math.Max(m_fSpawnFallbackMax, m_fSpawnMax)), Math.Round(m_fSpawnPlayerMin), PLAN_RETRY_MS / 1000));
				}
				return;
			}
			LogPlan(truck, now);
		}
		if (now < truck.m_iLaunchTick)
			return;

		string refusal = LaunchRefusal(enemies, truck);
		if (!refusal.IsEmpty())
		{
			if (now - truck.m_iLaunchTick > waitMs)
			{
				TruckLog(TruckLabel(truck) + " : abandonné, départ reporté trop longtemps (" + refusal + ")");
				truck.m_iPhase = SRP_ETruckPhase.FINI;
			}
			else if (!truck.m_bWaitLogged)
			{
				truck.m_bWaitLogged = true;
				TruckLog(TruckLabel(truck) + " : départ reporté (" + refusal + "), nouvel essai à chaque passage");
			}
			return;
		}

		// Les joueurs ont pu bouger depuis le choix : un autre départ si celui-ci est vu ou trop près d'eux
		if (!OriginUsable(truck, truck.m_vOrigin, players))
		{
			truck.m_bPlanned = false;
			if (!PlanTruck(truck, players, now))
			{
				truck.m_iNextPlanTick = now + PLAN_RETRY_MS;	// nouvel essai dans 10 s
				return;
			}
			TruckLog(TruckLabel(truck) + " : départ déplacé (l'ancien était vu ou trop près des joueurs)");
			if (now < truck.m_iLaunchTick)
				return;
		}
		if (!LaunchNow(enemies, truck, players, now))
			truck.m_iPhase = SRP_ETruckPhase.FINI;
	}

	//------------------------------------------------------------------------------------------------
	//! EMBARQUEMENT : chaque soldat livré monte à l'arrière (réessayé à chaque passage) ; fin quand le chauffeur est aux
	//! commandes et que tous les groupes sont complets et assis, ou après m_iBoardSeconds
	protected void TickBoarding(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		if (!truck.m_Vehicle || truck.m_Vehicle.IsDeleted() || IsDestroyed(truck.m_Vehicle))
		{
			CancelTruck(enemies, truck, players, now, "camion détruit ou disparu à l'embarquement");
			return;
		}
		int seated = SeatPassengers(truck);
		bool complete = IsDriverAboard(truck);
		int fitCount = 0;
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			if (!cargo || !cargo.m_Group || cargo.m_Group.IsDeleted())
				continue;
			if (!cargo.m_Group.IsExpandComplete())
				complete = false;
			fitCount += CountFit(cargo);
		}
		if (seated < fitCount)
			complete = false;
		int boardMs = Math.ClampInt(m_iBoardSeconds, 5, 600) * 1000;
		if (!complete && now - truck.m_iPhaseTick < boardMs)
			return;
		FinishBoarding(enemies, truck, players, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de l'embarquement : traînards retirés (pas des pertes), plus rien ne sera livré ; sans chauffeur ou sans
	//! passager, le camion est annulé ; sinon il part par la route vers son stationnement, et son départ est réglé avec
	//! les seuls soldats assis (SettleDeparture : débit MA2, ou retour à la colonne de ceux qui ne partent pas)
	protected void FinishBoarding(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		DropStragglers(enemies, truck);
		int seated = CountSeated(truck);	// sans nouvel essai : les traînards viennent d'être retirés
		truck.m_iSeated = seated;
		bool driverAboard = IsDriverAboard(truck);
		if (!driverAboard || seated == 0)
		{
			string why = "personne à bord après l'embarquement";
			if (!driverAboard)
				why = "chauffeur pas aux commandes après l'embarquement";
			CancelTruck(enemies, truck, players, now, why);
			return;
		}
		SettleDeparture(truck, seated);
		truck.m_iFitAtStart = seated + 1;	// passagers assis et chauffeur (les traînards retirés ne comptent pas, même avant leur suppression effective)
		truck.m_fHealthStart = HealthOf(truck.m_Vehicle);
		DriveTo(enemies, truck, truck.m_aRoute, truck.m_vPark, true);
		truck.m_iPhase = SRP_ETruckPhase.ROUTE;
		truck.m_iPhaseTick = now;
		truck.m_iLastMoveTick = now;
		truck.m_iStillSince = 0;
		truck.m_iStrikes = 0;
		truck.m_vLastPos = truck.m_Vehicle.GetOrigin();
		TruckLog(string.Format("%1 : en route, %2 soldat(s) à bord sur %3 voulus, %4 point(s) de route, stationnement à %5 m du centre", TruckLabel(truck), seated, truck.m_iLoad, truck.m_aRoute.Count(), Math.Round(vector.DistanceXZ(truck.m_vPark, truck.m_vTown))));
	}

	//------------------------------------------------------------------------------------------------
	//! ROUTE : embuscade (tout le monde saute et attaque), renversé (débarquement sur place), arrivée (moins de 25 m du
	//! stationnement, ou moins de 60 m et arrêté depuis 5 s), blocage (trajet relancé, puis débarquement sur place), trop
	//! longtemps en route (débarquement sur place)
	protected void TickRoute(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
		{
			Vanished(enemies, truck, "camion disparu en route");
			return;
		}
		KeepAboard(truck, true);
		vector position = vehicle.GetOrigin();

		// Diagnostic (IA du chauffeur coupée au loin ?) : son état 10 s après le départ
		if (!truck.m_bLodLogged && now - truck.m_iPhaseTick >= 10000)
		{
			truck.m_bLodLogged = true;
			TruckLog(string.Format("%1 : 10 s après le départ, %2 m parcourus, %3", TruckLabel(truck), Math.Round(vector.DistanceXZ(position, truck.m_vOrigin)), DriverAIText(truck)));
		}

		string ambush = AmbushReason(truck, players, position);
		if (!ambush.IsEmpty())
		{
			StartAmbush(enemies, truck, players, position, ambush, now);
			return;
		}

		vector up = vehicle.GetTransformAxis(1);
		if (up[1] < 0.5)
		{
			truck.m_bAbandon = true;
			BeginDismount(enemies, truck, now, "renversé en route, débarquement sur place, la suite à pied");
			return;
		}

		float speed = SpeedOf(vehicle);
		if (speed < 1.5)
		{
			if (truck.m_iStillSince == 0)
				truck.m_iStillSince = now;
		}
		else
			truck.m_iStillSince = 0;
		float toPark = vector.DistanceXZ(position, truck.m_vPark);
		bool stopped = truck.m_iStillSince > 0 && now - truck.m_iStillSince >= 5000;
		if (toPark < 25 || (toPark < 60 && stopped))
		{
			truck.m_iArrivedTick = now;
			LogArrival(truck, now);
			BeginDismount(enemies, truck, now, "");
			return;
		}

		// Blocage : jamais à plus de STUCK_RADIUS de son point de référence pendant m_iStuckSeconds (un camion qui avance
		// et recule de quelques mètres à un carrefour ne remet plus le compteur à zéro)
		if (vector.DistanceXZ(position, truck.m_vLastPos) >= STUCK_RADIUS)
		{
			truck.m_vLastPos = position;
			truck.m_iLastMoveTick = now;
		}
		int stuckMs = Math.ClampInt(m_iStuckSeconds, 10, 600) * 1000;
		if (now - truck.m_iLastMoveTick >= stuckMs)
		{
			truck.m_iStrikes++;
			truck.m_iLastMoveTick = now;
			truck.m_vLastPos = position;
			if (truck.m_iStrikes >= 2)
			{
				// Toujours bloqué : tout le monde descend, chauffeur compris, et le camion reste (il ne pourrait pas repartir)
				truck.m_bAbandon = true;
				BeginDismount(enemies, truck, now, string.Format("bloqué une 2e fois à %1 m du stationnement, tout le monde descend sur place (chauffeur compris, le camion reste), la suite à pied", Math.Round(toPark)));
				return;
			}
			array<vector> again = {};
			SRP_Placement.BuildRoadRoute(position, truck.m_vPark, 300, 0, again);
			DriveTo(enemies, truck, again, truck.m_vPark, true);
			TruckLog(string.Format("%1 : bloqué à %2 m du stationnement, trajet relancé", TruckLabel(truck), Math.Round(toPark)));
			return;
		}

		int driveMs = Math.ClampInt(m_iMaxDriveSeconds, 30, 3600) * 1000;
		int doubleEstimate = 2 * truck.m_iEstimateMs;
		if (doubleEstimate > driveMs)
			driveMs = doubleEstimate;
		if (now - truck.m_iPhaseTick > driveMs)
			BeginDismount(enemies, truck, now, string.Format("%1 s de route sans arriver, débarquement sur place à %2 m du stationnement", driveMs / 1000, Math.Round(toPark)));
	}

	//------------------------------------------------------------------------------------------------
	//! DEBARQUEMENT : attente de l'arrêt (moins de 1,5 m/s, 4 s au plus), puis l'ordre (moitié assaut, moitié défense) ;
	//! filet : après 10 s, les soldats encore assis sont descendus (animé, puis téléporté après trois essais) ; plus
	//! personne à bord ou 20 s passées : le camion repart (ou reste sur place après une embuscade ou s'il est renversé)
	protected void TickDismount(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, int now)
	{
		IEntity vehicle = truck.m_Vehicle;
		bool hasVehicle = vehicle != null && !vehicle.IsDeleted();
		if (truck.m_iOrderTick == 0)
		{
			if (hasVehicle && SpeedOf(vehicle) >= 1.5 && now - truck.m_iPhaseTick < 4000)
				return;
			OrderDismount(enemies, truck);
			truck.m_iOrderTick = now;
			return;
		}
		if (hasVehicle)
			KeepAboard(truck, false);
		int aboard = ForceExits(truck, now);
		if (aboard > 0 && now - truck.m_iOrderTick < 20000)
			return;
		if (truck.m_bAmbushed || truck.m_bAbandon || !hasVehicle)
		{
			if (hasVehicle)
				enemies.RegisterTransport(vehicle);	// laissé sur place : nettoyage des transports (20 min sans joueur à 300 m)
			ReleaseDriver(enemies, truck);			// jamais un chauffeur sans maître (vague de mission : retiré plus tard, hors de vue)
			truck.m_iPhase = SRP_ETruckPhase.FINI;
			return;
		}
		StartLeaving(enemies, truck, now, "débarquement terminé");
	}

	//------------------------------------------------------------------------------------------------
	//! DEPART : épave ou plus de chauffeur aux commandes, le camion reste (nettoyage des transports) ; sinon il est retiré
	//! dès qu'il n'est vu par aucun joueur (le camion et ses occupants ne cachent pas la vue ; un joueur à bord ou à moins
	//! de 150 m le voit toujours) et qu'il est loin d'eux (m_fVanishDistance), revenu à son départ, ou parti depuis
	//! m_iVanishMaxMinutes. Jamais sous les yeux d'un joueur. Surveillance du retour : renversé, ou bloqué deux fois (demi-
	//! tour impossible dans une rue étroite…), le chauffeur descend et le camion reste (AbandonLeaving).
	protected void TickLeaving(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
		{
			Vanished(enemies, truck, "camion disparu en repartant");
			return;
		}
		if (IsDestroyed(vehicle) || !IsDriverAboard(truck))
		{
			enemies.RegisterTransport(vehicle);
			ReleaseAboard(truck, true);
			array<ref SRP_EnemyGroup> leftovers = {};
			if (OwnsLeftovers(truck))
			{
				foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
				{
					if (cargo)
						leftovers.Insert(cargo);
				}
			}
			enemies.RetireLater(leftovers);	// survivants : retirés plus tard, hors de vue
			ReleaseDriver(enemies, truck);	// chauffeur hors de combat compris : retiré plus tard, hors de vue
			TruckLog(TruckLabel(truck) + " : épave ou plus de chauffeur aux commandes en repartant, le camion reste sur place");
			truck.m_iPhase = SRP_ETruckPhase.FINI;
			return;
		}
		KeepAboard(truck, true);
		vector position = vehicle.GetOrigin();
		bool home = vector.DistanceXZ(position, truck.m_vOrigin) < 50;
		if (!SRP_Placement.IsVehicleSeenByAnyPlayer(vehicle, players))
		{
			float nearest = SRP_Placement.NearestPlayer(position, players);
			bool far = nearest > m_fVanishDistance;
			bool late = now - truck.m_iPhaseTick > Math.ClampInt(m_iVanishMaxMinutes, 1, 120) * 60000;
			if (far || home || late)
			{
				Vanish(enemies, truck, nearest);
				return;
			}
		}
		if (home)
			return;	// revenu à son départ, encore vu : il attend là d'être hors de vue

		// Surveillance du retour : renversé, ou bloqué (jamais à plus de STUCK_RADIUS de son point de référence pendant
		// m_iStuckSeconds) ; la 1re fois le trajet est relancé, la 2e fois le chauffeur descend
		vector up = vehicle.GetTransformAxis(1);
		if (up[1] < 0.5)
		{
			AbandonLeaving(enemies, truck, position, "renversé en repartant");
			return;
		}
		if (vector.DistanceXZ(position, truck.m_vLastPos) >= STUCK_RADIUS)
		{
			truck.m_vLastPos = position;
			truck.m_iLastMoveTick = now;
		}
		int stuckMs = Math.ClampInt(m_iStuckSeconds, 10, 600) * 1000;
		if (now - truck.m_iLastMoveTick < stuckMs)
			return;
		truck.m_iStrikes++;
		truck.m_iLastMoveTick = now;
		truck.m_vLastPos = position;
		if (truck.m_iStrikes >= 2)
		{
			AbandonLeaving(enemies, truck, position, string.Format("bloqué une 2e fois en repartant, à %1 m de son départ", Math.Round(vector.DistanceXZ(position, truck.m_vOrigin))));
			return;
		}
		array<vector> again = {};
		SRP_Placement.BuildRoadRoute(position, truck.m_vOrigin, 300, 0, again);
		DriveTo(enemies, truck, again, truck.m_vOrigin, false);
		TruckLog(string.Format("%1 : bloqué en repartant à %2 m de son départ, trajet relancé", TruckLabel(truck), Math.Round(vector.DistanceXZ(position, truck.m_vOrigin))));
	}

	//------------------------------------------------------------------------------------------------
	//! Le camion ne peut pas repartir (renversé, bloqué deux fois) : il reste sur place (nettoyage des transports) et le
	//! chauffeur descend. Encore dans la localité gardée (rayon + 300 m), il rejoint sa garnison en défense ; sinon il
	//! tient le coin et sera retiré plus tard, hors de vue. Des passagers encore assis (camion rappelé) descendent aussi
	//! et sont retirés plus tard, hors de vue (ceux d'un camion de contact aussi : descendus en défense, le Commandeur ne
	//! les reprend pas). Le camion ne compte plus parmi les camions posés.
	protected void AbandonLeaving(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, vector position, string why)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (truck.m_Driver)
			enemies.ClearWaypoints(truck.m_Driver);
		ReleaseAboard(truck, true);
		array<ref SRP_EnemyGroup> leftovers = {};
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			if (!cargo || !HasSeated(cargo, vehicle))
				continue;
			enemies.OrderDismountAndDefend(cargo, position, 30);
			if (OwnsLeftovers(truck) || truck.m_bContact)
				leftovers.Insert(cargo);
		}
		enemies.RetireLater(leftovers);

		SRP_EnemyGroup driver = truck.m_Driver;
		string driverFate = "chauffeur hors de combat";
		if (driver && CountFit(driver) > 0)
		{
			SRP_SectorState state = HeldStateNear(truck.m_sSector, position);
			if (state)
			{
				float defenseRadius = Math.Max(truck.m_fTownRadius * m_fDefenseRadiusFactor, 30);
				enemies.OrderDismountAndDefend(driver, truck.m_vTown, defenseRadius);
				driver.m_sZone = state.m_sName;
				if (!state.m_aGroups.Contains(driver))
					state.m_aGroups.Insert(driver);
				SRP_CmdCapacity.Tag(driver, SRP_ECmdCapClass.GARNISON);
				driverFate = "le chauffeur rejoint la garnison";
			}
			else
			{
				enemies.OrderDismountAndDefend(driver, position, 30);
				driverFate = "le chauffeur descend, retiré plus tard hors de vue";
			}
		}
		ReleaseDriver(enemies, truck);
		if (vehicle && !vehicle.IsDeleted())
			enemies.RegisterTransport(vehicle);	// laissé sur place : nettoyage des transports (20 min sans joueur à 300 m)
		truck.m_iPhase = SRP_ETruckPhase.FINI;
		TruckLog(TruckLabel(truck) + " : " + why + ", le camion reste sur place, " + driverFate);
	}

	//------------------------------------------------------------------------------------------------
	//! Le chauffeur n'est plus à bord pour de bon (embuscade, camion renversé, bloqué ou en épave) : son IA peut de nouveau
	//! être coupée au loin ; s'il n'a pas rejoint la garnison de la localité (vague de mission, localité sans garnison,
	//! chauffeur hors de combat), il est retiré plus tard, hors de vue (RetireLater), au lieu de rester sans maître dans
	//! le décompte des groupes et des soldats
	protected void ReleaseDriver(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		SRP_EnemyGroup driver = truck.m_Driver;
		if (!driver)
			return;
		driver.m_Vehicle = null;
		AllowLod(FirstMember(driver));
		if (!truck.m_sSector.IsEmpty())
		{
			SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
			if (territory)
			{
				SRP_SectorState state = territory.FindState(truck.m_sSector);
				if (state && state.m_aGroups.Contains(driver))
					return;	// il tient la localité avec sa garnison
			}
		}
		array<ref SRP_EnemyGroup> leftovers = {};
		leftovers.Insert(driver);
		enemies.RetireLater(leftovers);
	}

	//------------------------------------------------------------------------------------------------
	//! La localité « sector » si elle est encore ennemie et gardée et que « position » est à moins de son rayon + 300 m,
	//! sinon null (vague : toujours null)
	protected static SRP_SectorState HeldStateNear(string sector, vector position)
	{
		if (sector.IsEmpty())
			return null;
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return null;
		SRP_SectorState state = territory.FindState(sector);
		if (!state || !state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || state.m_aGroups.IsEmpty())
			return null;
		if (vector.Distance(state.m_Sector.GetCenter(), position) > state.m_Sector.m_fRadius + 300)
			return null;
		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Le camion retire-t-il lui-même ses passagers (RetireLater, ou Delete s'il les emporte) quand il finit sans les
	//! avoir laissés à quelqu'un ? Oui pour un camion de localité ou de colonne pas rattaché à une garnison, et pour un
	//! camion de contact qui n'a pas débarqué (le Commandeur ne reprend que des débarqués : ContactGroupsTick) ; jamais
	//! pour une vague (sa mission retire ses groupes) ni pour des débarqués au combat.
	protected static bool OwnsLeftovers(SRP_EnemyTruck truck)
	{
		if (truck.m_bJoined)
			return false;
		if (!truck.m_bAllAssault)
			return true;
		return truck.m_bContact && !truck.m_bLanded;
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat de ce groupe est-il assis dans ce véhicule ?
	protected static bool HasSeated(SRP_EnemyGroup record, IEntity vehicle)
	{
		if (!vehicle)
			return false;
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(record, members);
		foreach (IEntity member : members)
		{
			if (SRP_EnemyComponent.IsSeatedIn(member, vehicle))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Choix du départ, du stationnement et de la route
	//------------------------------------------------------------------------------------------------
	//! Départ entre m_fSpawnMin et m_fSpawnMax de la localité (à défaut, entre m_fSpawnMax et m_fSpawnFallbackMax), sur
	//! une route, hors de vue, loin des joueurs, jamais près de la base ni dans l'eau, pas sur un autre camion ; parmi ceux
	//! qui conviennent, celui dont le trajet estimé tient dans l'heure d'arrivée visée (le plus long : le moins d'attente),
	//! sinon le plus court. Départ différé pour arriver à l'heure visée. Front : un premier passage avec les départs sur
	//! un carré rouge seulement (m_bOriginRedOnly) ; aucun ne convient : un second passage sans ce filtre, noté au
	//! journal (une fois par camion).
	protected bool PlanTruck(SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		bool redOnly = m_bOriginRedOnly && IsFrontReady();
		if (PlanPass(truck, players, now, redOnly))
			return true;
		if (!redOnly)
			return false;
		if (!PlanPass(truck, players, now, false))
			return false;
		if (!truck.m_bRedFallbackLogged)
		{
			truck.m_bRedFallbackLogged = true;
			TruckLog(string.Format("%1 : aucun départ sur un carré rouge, départ pris hors du rouge à %2 m de sa cible", TruckLabel(truck), Math.Round(vector.DistanceXZ(truck.m_vOrigin, truck.m_vTown))));
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un passage du choix du départ : renfort du Commandeur, d'abord sur la route de sa localité source (toute la
	//! couronne, de m_fSpawnMin à m_fSpawnFallbackMax) ; puis l'anneau habituel, puis la couronne de repli
	protected bool PlanPass(SRP_EnemyTruck truck, array<IEntity> players, int now, bool redOnly)
	{
		float wideMax = Math.Max(m_fSpawnFallbackMax, m_fSpawnMax);
		if (HasSourcePoint(truck))
		{
			array<vector> fromSource = {};
			CollectSourceOrigins(truck, fromSource, redOnly);
			if (PickOrigin(truck, fromSource, players, now, m_fSpawnMin, wideMax))
				return true;
		}
		array<vector> candidates = {};
		CollectOrigins(truck, candidates, redOnly);
		if (PickOrigin(truck, candidates, players, now, m_fSpawnMin, m_fSpawnMax))
			return true;
		return PickOrigin(truck, candidates, players, now, m_fSpawnMax, wideMax);
	}

	//------------------------------------------------------------------------------------------------
	//! Le camion vient-il d'une localité source connue (renfort du Commandeur, hors colonne) ?
	protected static bool HasSourcePoint(SRP_EnemyTruck truck)
	{
		return !truck.m_sSource.IsEmpty() && truck.m_vSource != vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Les départs sur la route de la localité source vers la cible (son point clé compris), rabattus sur une route
	protected void CollectSourceOrigins(SRP_EnemyTruck truck, array<vector> candidates, bool redOnly)
	{
		AddCandidate(truck, candidates, truck.m_vSource, 150, redOnly);
		array<vector> road = {};
		SRP_Placement.BuildRoadRoute(truck.m_vSource, truck.m_vTown, 300, 0, road);
		foreach (vector step : road)
			AddCandidate(truck, candidates, step, 150, redOnly);
	}

	//------------------------------------------------------------------------------------------------
	//! Les départs possibles, rabattus sur une route : les points clés des localités ennemies voisines (à 4 km au plus)
	//! d'abord, puis les lieux-dits (l'ancien départ des renforts motorisés), puis 12 sondes en couronne autour de la
	//! localité et 8 sur la couronne de repli. « redOnly » : sur un carré rouge seulement.
	protected void CollectOrigins(SRP_EnemyTruck truck, array<vector> candidates, bool redOnly)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
		{
			SRP_SectorState exclude;
			if (!truck.m_sSector.IsEmpty())
				exclude = territory.FindState(truck.m_sSector);
			array<SRP_SectorState> neighbours = {};
			territory.NearestEnemySectors(truck.m_vTown, 6, exclude, neighbours);
			foreach (SRP_SectorState neighbour : neighbours)
			{
				if (neighbour && neighbour.m_Sector)
					AddCandidate(truck, candidates, neighbour.m_Sector.GetCenter(), 150, redOnly);
			}
		}
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
		{
			if (zone)
				AddCandidate(truck, candidates, zone.GetCenter(), 150, redOnly);
		}
		foreach (SRP_DeliveryPointComponent point : SRP_DeliveryPointComponent.GetPoints())
		{
			if (point && point.GetOwner())
				AddCandidate(truck, candidates, point.GetOwner().GetOrigin(), 150, redOnly);
		}
		float ring = (m_fSpawnMin + m_fSpawnMax) * 0.5;
		float start = Math.RandomFloat(0, Math.PI2);
		for (int i = 0; i < 12; i++)
		{
			float angle = start + Math.PI2 * i / 12;
			vector probe = truck.m_vTown + Vector(Math.Cos(angle) * ring, 0, Math.Sin(angle) * ring);
			AddCandidate(truck, candidates, probe, 250, redOnly);
		}
		float outerRing = (m_fSpawnMax + Math.Max(m_fSpawnFallbackMax, m_fSpawnMax)) * 0.5;
		for (int k = 0; k < 8; k++)
		{
			float outerAngle = start + Math.PI2 * (k + 0.5) / 8;
			vector outerProbe = truck.m_vTown + Vector(Math.Cos(outerAngle) * outerRing, 0, Math.Sin(outerAngle) * outerRing);
			AddCandidate(truck, candidates, outerProbe, 250, redOnly);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un départ possible : assez loin de la localité (grossièrement), rabattu sur la route la plus proche, sur un carré
	//! rouge si « redOnly » (seul le départ doit être rouge, la route peut traverser le bleu), pas en doublon
	protected void AddCandidate(SRP_EnemyTruck truck, array<vector> candidates, vector around, float reach, bool redOnly)
	{
		float fromTown = vector.DistanceXZ(around, truck.m_vTown);
		float widest = Math.Max(m_fSpawnFallbackMax, m_fSpawnMax) + 300;
		if (fromTown < m_fSpawnMin * 0.9 || fromTown > widest)
			return;
		vector road;
		if (!SRP_Placement.FindRoadPoint(around, reach, road))
			return;
		if (redOnly && !IsRedOrigin(road))
			return;
		foreach (vector known : candidates)
		{
			if (vector.DistanceXZ(known, road) < 150)
				return;
		}
		candidates.Insert(road);
	}

	//------------------------------------------------------------------------------------------------
	//! Le front est-il prêt (grille lue) ? Sans lui, pas de filtre rouge
	protected static bool IsFrontReady()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		return front && front.IsReady();
	}

	//------------------------------------------------------------------------------------------------
	//! Un départ sur un carré rouge du front (vrai sans front prêt)
	protected static bool IsRedOrigin(vector point)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return true;
		return front.IsRedAt(point);
	}

	//------------------------------------------------------------------------------------------------
	//! Colonne : son point de pose convient-il encore ? Ni eau ni base, à m_fSpawnPlayerMin au moins de tout joueur, vu
	//! par aucun, pas sur un camion posé (le 2e camion d'une colonne se pose 40 m derrière le 1er : seul l'emplacement
	//! libre compte, trouvé à la pose)
	protected bool ColumnOriginUsable(vector point, array<IEntity> players)
	{
		if (SRP_Placement.IsWater(point) || SRP_Placement.IsNearBase(point))
			return false;
		if (SRP_Placement.NearestPlayer(point, players) < m_fSpawnPlayerMin)
			return false;
		if (SRP_Placement.IsSeenByAnyPlayer(point, players))
			return false;
		foreach (SRP_EnemyTruck other : m_aTrucks)
		{
			if (!other || other.m_iPhase == SRP_ETruckPhase.FINI || !other.m_Vehicle || other.m_Vehicle.IsDeleted())
				continue;
			if (vector.DistanceXZ(other.m_Vehicle.GetOrigin(), point) < 12)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le meilleur départ dans l'anneau [minRange, maxRange] (voir PlanTruck). Les départs utilisables sont d'abord triés à
	//! vol d'oiseau (route ≈ ROUTE_FACTOR × la distance) : avec une heure visée, ceux qui la tiennent d'abord (le plus loin
	//! en premier), puis les plus proches ; sans heure visée, les plus proches. Seuls les MAX_ROUTES premiers ont leur
	//! route calculée (le calcul des routes, fait dans l'image du passage, ne doit pas figer le serveur).
	protected bool PickOrigin(SRP_EnemyTruck truck, array<vector> candidates, array<IEntity> players, int now, float minRange, float maxRange)
	{
		int remaining = -1;
		if (truck.m_iArriveTarget > 0)
		{
			remaining = truck.m_iArriveTarget - now;
			if (remaining < 0)
				remaining = 0;
		}

		// Tri à vol d'oiseau (score le plus petit d'abord ; négatif : tient dans l'heure visée)
		array<vector> usable = {};
		array<float> scores = {};
		foreach (vector probe : candidates)
		{
			float fromTown = vector.DistanceXZ(probe, truck.m_vTown);
			if (fromTown < minRange || fromTown > maxRange)
				continue;
			if (!OriginUsable(truck, probe, players))
				continue;
			float score = fromTown;
			if (remaining >= 0 && EstimateMs(fromTown * ROUTE_FACTOR) <= remaining)
				score = -fromTown;
			int slot = 0;
			while (slot < scores.Count() && scores[slot] <= score)
				slot++;
			usable.InsertAt(probe, slot);
			scores.InsertAt(score, slot);
		}

		bool found = false;
		bool bestFits = false;
		int bestEstimate = 0;
		float bestLength = 0;
		vector bestOrigin;
		vector bestPark;
		array<vector> bestRoute = {};
		int evaluated = 0;
		foreach (vector candidate : usable)
		{
			if (evaluated >= MAX_ROUTES)
				break;
			vector park;
			if (!FindPark(truck, candidate, players, park))
				continue;
			array<vector> route = {};
			SRP_Placement.BuildRoadRoute(candidate, park, 300, 0, route);
			float length = RouteLength(candidate, route, park);
			int estimate = EstimateMs(length);
			evaluated++;
			bool fits = remaining >= 0 && estimate <= remaining;
			bool better = false;
			if (!found)
				better = true;
			else if (remaining < 0)
				better = estimate < bestEstimate;	// pas d'heure visée : le plus court
			else if (fits && !bestFits)
				better = true;
			else if (fits && bestFits)
				better = estimate > bestEstimate;	// tient dans l'heure : le plus long, le départ le plus tôt
			else if (!fits && !bestFits)
				better = estimate < bestEstimate;	// aucun ne tient : le plus court
			if (!better)
				continue;
			found = true;
			bestFits = fits;
			bestEstimate = estimate;
			bestLength = length;
			bestOrigin = candidate;
			bestPark = park;
			bestRoute.Clear();
			foreach (vector step : route)
				bestRoute.Insert(step);
		}
		if (!found)
			return false;
		truck.m_vOrigin = bestOrigin;
		truck.m_vPark = bestPark;
		truck.m_aRoute.Clear();
		foreach (vector point : bestRoute)
			truck.m_aRoute.Insert(point);
		truck.m_fRouteLength = bestLength;
		truck.m_iEstimateMs = bestEstimate;
		truck.m_iLaunchTick = now;
		if (remaining > bestEstimate)
			truck.m_iLaunchTick = now + remaining - bestEstimate;
		truck.m_bPlanned = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un départ convient-il ? Ni eau ni base, à m_fSpawnPlayerMin au moins de tout joueur, vu par aucun, pas sur le
	//! départ d'un autre camion qui n'est pas encore parti ni sur un camion posé
	protected bool OriginUsable(SRP_EnemyTruck truck, vector point, array<IEntity> players)
	{
		if (SRP_Placement.IsWater(point) || SRP_Placement.IsNearBase(point))
			return false;
		if (SRP_Placement.NearestPlayer(point, players) < m_fSpawnPlayerMin)
			return false;
		if (SRP_Placement.IsSeenByAnyPlayer(point, players))
			return false;
		foreach (SRP_EnemyTruck other : m_aTrucks)
		{
			if (!other || other == truck || other.m_iPhase == SRP_ETruckPhase.FINI)
				continue;
			if (other.m_bPlanned && other.m_iPhase <= SRP_ETruckPhase.EMBARQUEMENT && vector.DistanceXZ(other.m_vOrigin, point) < 60)
				return false;
			if (other.m_Vehicle && !other.m_Vehicle.IsDeleted() && vector.DistanceXZ(other.m_Vehicle.GetOrigin(), point) < 40)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le stationnement, du côté d'où vient le camion. Vague : à 150-250 m du site, sur une route si possible. Localité :
	//! une route atteignable à moins de m_fParkFactor × le rayon du centre ; trop près d'un joueur, une route à 0,8 × le
	//! rayon ; sinon l'entrée de ce côté. Aucune contrainte de vue : il se gare à la vue.
	protected bool FindPark(SRP_EnemyTruck truck, vector origin, array<IEntity> players, out vector park)
	{
		vector away = origin - truck.m_vTown;
		away[1] = 0;
		float length = away.Length();
		if (length < 1)
			return false;
		vector direction = away * (1 / length);
		vector road;
		if (truck.m_bAllAssault)
		{
			float waveMax = Math.Max(m_fWaveParkMax, m_fWaveParkMin);
			float distance = Math.RandomFloat(m_fWaveParkMin, waveMax);
			vector probe = SRP_Placement.OnGround(truck.m_vTown + direction * distance);
			if (SRP_Placement.FindRoadPointToward(origin, probe, 80, road))
			{
				float fromSite = vector.DistanceXZ(road, truck.m_vTown);
				if (fromSite >= m_fWaveParkMin * 0.7 && fromSite <= waveMax * 1.3)
				{
					park = road;
					return true;
				}
			}
			if (SRP_Placement.IsWater(probe))
				return false;
			park = probe;
			return true;
		}

		float radius = Math.Max(truck.m_fTownRadius, 60);
		if (SRP_Placement.FindRoadPointToward(origin, truck.m_vTown, m_fParkFactor * radius, road))
		{
			if (vector.DistanceXZ(road, truck.m_vTown) <= radius && SRP_Placement.NearestPlayer(road, players) >= m_fParkPlayerMin)
			{
				park = road;
				return true;
			}
		}
		vector inner = SRP_Placement.OnGround(truck.m_vTown + direction * (radius * 0.8));
		if (SRP_Placement.FindRoadPoint(inner, radius * 0.3, road) && SRP_Placement.NearestPlayer(road, players) >= m_fParkPlayerMin)
		{
			park = road;
			return true;
		}
		vector edge = SRP_Placement.OnGround(truck.m_vTown + direction * radius);
		if (SRP_Placement.FindRoadPoint(edge, 80, road))
		{
			park = road;
			return true;
		}
		if (SRP_Placement.IsWater(edge))
			return false;
		park = edge;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Longueur du trajet : départ, points de route, stationnement (à plat)
	protected static float RouteLength(vector origin, array<vector> route, vector park)
	{
		float length = 0;
		vector previous = origin;
		foreach (vector step : route)
		{
			length += vector.DistanceXZ(previous, step);
			previous = step;
		}
		length += vector.DistanceXZ(previous, park);
		return length;
	}

	//------------------------------------------------------------------------------------------------
	//! Durée estimée, en millisecondes : embarquement + trajet à m_fDriveSpeedEstimate
	protected int EstimateMs(float length)
	{
		float speed = Math.Max(m_fDriveSpeedEstimate, 1);
		int boardMs = Math.ClampInt(m_iBoardSeconds, 5, 600) * 1000;
		int driveMs = Math.Round(length / speed * 1000);
		return boardMs + driveMs;
	}

	//------------------------------------------------------------------------------------------------
	// Pose
	//------------------------------------------------------------------------------------------------
	//! Place avant la pose : camions posés (4 au plus), plafond de groupes, puis la place des soldats ennemis décidée par
	//! la seule capacité (SRP_CmdCapacity.Ask, classe COMBAT, chauffeur compris : 120 soldats, réserves, marge du jeu ;
	//! une demande refusée reste notée pour la file RE12). Refusée : un camion moins chargé part si la place en laisse au
	//! moins 4 (jamais une colonne, dont les soldats sont comptés camion par camion). "" si la pose est permise, sinon la
	//! raison (en français)
	protected string LaunchRefusal(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		int launched = CountLaunched();
		int maxTrucks = Math.ClampInt(m_iMaxActiveTrucks, 1, 20);
		if (launched >= maxTrucks)
			return string.Format("%1 camion(s) déjà posé(s), %2 au plus", launched, maxTrucks);
		int groups = CargoGroupCount(truck.m_iLoad, truck.m_bSectionsOnly) + 1;
		int alive = enemies.CountAlive();
		int groupCap = enemies.GetMaxGroups();
		if (truck.m_sOwner != "mission")
			groupCap = enemies.GetTerritoryGroupCap();	// les groupes gardés pour les missions ne sont pas pour le territoire
		if (alive + groups > groupCap)
			return string.Format("plafond de groupes : %1 + %2 sur %3", alive, groups, groupCap);

		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string key = DemandKey(truck);
		vector where = truck.m_vTown;
		if (truck.m_bPlanned)
			where = truck.m_vOrigin;
		string refusal = capacity.Ask(key, SRP_ECmdCapClass.COMBAT, truck.m_iLoad + 1, groups, where, truck.m_sOwner);
		if (refusal.IsEmpty())
			return "";
		if (truck.m_iColumnId != 0)
			return refusal;
		int smaller = EvenFloor(capacity.Room(SRP_ECmdCapClass.COMBAT, truck.m_sOwner) - 1);
		if (smaller < WAVE_MIN_LOAD || smaller >= truck.m_iLoad)
			return refusal;
		int smallerGroups = CargoGroupCount(smaller, truck.m_bSectionsOnly) + 1;
		string second = capacity.Ask(key, SRP_ECmdCapClass.COMBAT, smaller + 1, smallerGroups, where, truck.m_sOwner);
		if (!second.IsEmpty())
			return second;
		TruckLog(string.Format("%1 : chargement réduit de %2 à %3 soldats (place des soldats ennemis : %4)", TruckLabel(truck), truck.m_iLoad, smaller, refusal));
		truck.m_iLoad = smaller;
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Clé de la demande de place d'un camion (SRP_CmdCapacity) : « camion-<localité> », « camion-contact-<zone> »,
	//! « camion-colonne-<n> », sinon « camion-<n> » (vague)
	protected static string DemandKey(SRP_EnemyTruck truck)
	{
		if (truck.m_iColumnId != 0)
			return "camion-colonne-" + truck.m_iColumnId.ToString();
		if (!truck.m_sSector.IsEmpty())
			return "camion-" + truck.m_sSector;
		if (truck.m_bContact)
			return "camion-contact-" + ZoneCode(truck.m_iTargetZone);
		return "camion-" + truck.m_iId.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Un camion jamais posé sort de la liste et retire sa demande de place
	protected void DropTruck(SRP_EnemyTruck truck)
	{
		if (!truck)
			return;
		SRP_CmdCapacity.Get().ClearDemand(DemandKey(truck));
		m_aTrucks.RemoveItem(truck);
	}

	//------------------------------------------------------------------------------------------------
	//! La pose : le camion hors de vue sur son départ, orienté vers le premier point de route ; le chauffeur seul dans son
	//! groupe, assis aux commandes dans la même image (il reste à bord, IA jamais coupée au loin) ; les groupes de passagers
	//! (tailles exactes), sans ordre tant qu'ils sont à bord ; chauffeur et passagers en classe COMBAT ; camion de
	//! contact : passagers RENFORT, hors de toute contre-attaque. Rien n'est débité ici : la source l'est au départ, des
	//! seuls soldats assis (SettleDeparture, fin de l'embarquement). Faux si rien n'a pu être posé (tout est retiré).
	protected bool LaunchNow(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now)
	{
		vector position = truck.m_vOrigin;
		vector spot;
		if (SCR_WorldTools.FindEmptyTerrainPosition(spot, truck.m_vOrigin, 25, 4, 2.5))
			position = spot;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.3;
		vector toward = truck.m_vPark;
		if (!truck.m_aRoute.IsEmpty())
			toward = truck.m_aRoute[0];
		IEntity vehicle = SpawnOriented(m_sTruckPrefab, position, toward);
		if (!vehicle)
		{
			TruckLog(TruckLabel(truck) + " : camion impossible à poser (prefab " + m_sTruckPrefab + ")");
			return false;
		}
		truck.m_Vehicle = vehicle;
		truck.m_iFreeSeats = CountCargoSeats(vehicle);

		SRP_EnemyGroup driver = enemies.SpawnVehicleDriver(m_sDriverPrefab, vehicle, truck.m_sOwner);
		if (!driver)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			truck.m_Vehicle = null;
			TruckLog(TruckLabel(truck) + " : chauffeur impossible à poser ou à asseoir (prefab " + m_sDriverPrefab + "), camion annulé");
			return false;
		}
		truck.m_Driver = driver;
		SRP_CmdCapacity.Tag(driver, SRP_ECmdCapClass.COMBAT);
		IEntity driverEntity = FirstMember(driver);
		if (driverEntity)
		{
			SetStayAboard(driverEntity, true);
			PreventLod(driverEntity);
		}

		array<ResourceName> prefabs = {};
		int planned = PlanCargo(enemies, truck.m_iLoad, truck.m_bSectionsOnly, prefabs);
		vector side = vehicle.GetTransformAxis(0);
		vector ground = SRP_Placement.OnGround(position + side * 6);
		foreach (ResourceName prefab : prefabs)
		{
			SRP_EnemyGroup cargo = enemies.SpawnGroup(ground, false, ground, truck.m_sOwner, -1, vector.Zero, -1, false, prefab);
			if (!cargo)
				continue;
			enemies.ClearWaypoints(cargo);	// aucun ordre tant qu'ils sont à bord : ils ne descendent qu'au stationnement
			enemies.NoteHome(cargo, "camion", false);
			SRP_CmdCapacity.Tag(cargo, SRP_ECmdCapClass.COMBAT);
			truck.m_aCargo.Insert(cargo);
		}
		if (truck.m_aCargo.IsEmpty())
		{
			enemies.Delete(driver);
			truck.m_Driver = null;
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			truck.m_Vehicle = null;
			TruckLog(TruckLabel(truck) + " : aucun groupe de passagers de la bonne taille (prefabs de sections et de binômes), camion annulé");
			return false;
		}
		if (planned > 0)
			truck.m_iLoad = planned;
		truck.m_iPhase = SRP_ETruckPhase.EMBARQUEMENT;
		truck.m_iPhaseTick = now;
		truck.m_iLaunchTick = now;
		truck.m_bLaunched = true;
		m_iSent++;
		MarkContactPassengers(truck);
		SeatPassengers(truck);

		string seats = "inconnues";
		if (truck.m_iFreeSeats >= 0)
			seats = truck.m_iFreeSeats.ToString();
		TruckLog(string.Format("%1 : posé hors de vue à %2 m de la localité (joueur le plus proche à %3 m), %4 soldats en %5 groupe(s) + chauffeur, places passagers libres comptées : %6 ; %7", TruckLabel(truck), Math.Round(vector.DistanceXZ(position, truck.m_vTown)), Math.Round(SRP_Placement.NearestPlayer(position, players)), truck.m_iLoad, truck.m_aCargo.Count(), seats, DriverAIText(truck)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! À la pose : les passagers d'un camion de contact (ou d'une colonne vers un contact) sont des groupes RENFORT, hors
	//! de l'entraide et de toute contre-attaque
	protected void MarkContactPassengers(SRP_EnemyTruck truck)
	{
		if (!truck.m_bContact)
			return;
		foreach (SRP_EnemyGroup passenger : truck.m_aCargo)
		{
			if (!passenger)
				continue;
			passenger.m_iCmdRole = SRP_ECmdRole.RENFORT;
			passenger.m_iAssaultZone = -1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MA2, une seule fois, au DÉPART (fin de l'embarquement, « departed » soldats assis) ou quand le camion ne part pas
	//! (annulé ou rappelé à l'embarquement, jamais posé, remise à zéro : « departed » = 0). Camion hors colonne d'une
	//! localité source : SRP_CmdManeuvers.OnTruckLaunched toujours appelé (il retire la demande en attente de la source,
	//! C2), qui débite les seuls soldats partis et l'inscrit au registre des camions en route (RE13) : un traînard jamais
	//! livré, un camion qui ne part pas ne coûtent rien à la source. Colonne (débitée par le Commandeur à
	//! son départ, jamais ici) : les soldats demandés pour ce camion qui ne partent pas (places, arrondi au pair, prefab,
	//! traînards, camion annulé) sont rendus à la source (ReturnSent : ils ne sont jamais partis). Les passagers partis
	//! portent leur source (m_sSourceLocality) : retirés vivants, ils y rentrent (RetireTick, ou ReturnSurvivors avant
	//! Delete).
	protected void SettleDeparture(SRP_EnemyTruck truck, int departed)
	{
		if (truck.m_bDeparted)
			return;
		truck.m_bDeparted = true;
		if (truck.m_sSource.IsEmpty())
			return;
		bool sourced = false;
		if (truck.m_iColumnId != 0)
		{
			sourced = departed > 0;
			int stayed = truck.m_iColumnLoad - Math.MaxInt(departed, 0);
			truck.m_iColumnDeparted = Math.ClampInt(departed, 0, truck.m_iColumnLoad);
			SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
			if (stayed > 0 && frontEnemy)
			{
				frontEnemy.ReturnSent(truck.m_sSource, stayed);
				// Les manœuvres retirent ces soldats de la colonne (ColumnPending : jamais rendus deux fois)
				SRP_CmdManeuvers columnManeuvers = SRP_CmdManeuvers.Get();
				if (columnManeuvers)
					columnManeuvers.OnColumnTruckDeparted(truck.m_iColumnId, stayed);
				TruckLog(string.Format("%1 : %2 soldat(s) de la colonne n°%3 ne sont pas partis (%4 demandés), rendus à %5", TruckLabel(truck), stayed, truck.m_iColumnId, truck.m_iColumnLoad, truck.m_sSource));
			}
		}
		else if (truck.m_iSentId == 0)
		{
			// Appelé même quand rien ne part (departed = 0) : le camion sort des demandes en attente de sa source (C2,
			// CanGive), puis OnTruckLaunched rend 0 sans rien débiter ni inscrire
			SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
			if (maneuvers)
				truck.m_iSentId = maneuvers.OnTruckLaunched(truck.m_sSource, Math.MaxInt(departed, 0));
			sourced = truck.m_iSentId != 0;
		}
		if (!sourced)
			return;
		foreach (SRP_EnemyGroup passenger : truck.m_aCargo)
		{
			if (passenger)
				passenger.m_sSourceLocality = truck.m_sSource;
		}
		TruckLog(string.Format("%1 : %2 soldats partis de %3 (comptés comme envoyés)", TruckLabel(truck), departed, truck.m_sSource));
	}

	//------------------------------------------------------------------------------------------------
	//! Le Commandeur est prévenu une seule fois que le camion hors colonne est déchargé (passagers débarqués) ou perdu
	//! (détruit, retiré) : il sort du registre des camions en route ; rien n'est rendu ici
	protected void NoteUnloaded(SRP_EnemyTruck truck)
	{
		if (truck.m_iSentId == 0 || truck.m_bUnloadNoted)
			return;
		truck.m_bUnloadNoted = true;
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers)
			maneuvers.OnTruckUnloaded(truck.m_iSentId);
	}

	//------------------------------------------------------------------------------------------------
	//! Débarquement donné (arrivée ou embuscade) : camion hors colonne déchargé (NoteUnloaded) ; colonne livrée avec ses
	//! passagers en état de combattre (DeliverColumn)
	protected void NoteLanded(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		NoteUnloaded(truck);
		if (truck.m_iColumnId == 0 || truck.m_bDelivered)
			return;
		int survivors = 0;
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
			survivors += CountFit(cargo);
		DeliverColumn(enemies, truck, survivors);
	}

	//------------------------------------------------------------------------------------------------
	//! Une colonne rend compte au Commandeur, une seule fois (SRP_CmdManeuvers.OnColumnDelivered) : débarqués entrés dans
	//! la garnison posée de la destination (ils gardent leur source) ; sinon, arrivés dans une localité sans garnison
	//! posée que le Commandeur crédite encore (ColumnCredited), ils comptent dans son effectif (reçus) : leurs groupes,
	//! absorbés, perdent leur source et sont retirés plus tard, hors de vue ; destination prise en route (le Commandeur
	//! ne crédite plus rien) : retirés plus tard hors de vue AVEC leur source, ils y rentrent par le retrait ; colonne
	//! vers un contact : les débarqués restent au combat (groupes RENFORT, avec leur source)
	protected void DeliverColumn(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, int survivors)
	{
		if (truck.m_iColumnId == 0 || truck.m_bDelivered)
			return;
		truck.m_bDelivered = true;
		bool joined = truck.m_bInGarrison;
		if (!joined && survivors > 0 && !truck.m_sColumnTo.IsEmpty())
		{
			bool credited = ColumnCredited(truck);
			array<ref SRP_EnemyGroup> landed = {};
			foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
			{
				if (!cargo)
					continue;
				if (credited)
					cargo.m_sSourceLocality = "";
				landed.Insert(cargo);
			}
			if (enemies)
				enemies.RetireLater(landed);
			if (credited)
				TruckLog(string.Format("%1 : %2 soldat(s) arrivés à %3, comptés dans son effectif, retirés plus tard hors de vue", TruckLabel(truck), survivors, truck.m_sColumnTo));
			else
				TruckLog(string.Format("%1 : %2 soldat(s) arrivés à %3, prise en route : retirés plus tard hors de vue, ils rentrent à %4", TruckLabel(truck), survivors, truck.m_sColumnTo, truck.m_sSource));
		}
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers)
		{
			// Part de ce camion encore comptée dans la colonne : ses partis, ou toute sa charge si le départ n'a pas été réglé
			int share = truck.m_iColumnLoad;
			if (truck.m_iColumnDeparted >= 0)
				share = truck.m_iColumnDeparted;
			maneuvers.OnColumnTruckDone(truck.m_iColumnId, share);
			maneuvers.OnColumnDelivered(truck.m_iColumnId, survivors, joined);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La destination de la colonne est-elle encore celle que le Commandeur crédite à la livraison (OnColumnDelivered :
	//! AddBonus seulement si la colonne vise toujours une localité) ? Une destination prise en route est effacée de la
	//! colonne (SRP_CmdManeuvers.OnLocalityTaken) : plus rien n'est crédité, les débarqués gardent leur source.
	protected static bool ColumnCredited(SRP_EnemyTruck truck)
	{
		if (!truck.m_Column || truck.m_sColumnTo.IsEmpty())
			return false;
		return truck.m_Column.m_sTo == truck.m_sColumnTo;
	}

	//------------------------------------------------------------------------------------------------
	//! Un prefab posé orienté vers « toward » (le Resource reste dans une variable locale pendant la pose)
	protected static IEntity SpawnOriented(ResourceName prefab, vector position, vector toward)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;
		vector heading = toward - position;
		float yaw = Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG;
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
		mat[3] = position;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Places passagers libres du véhicule (compartiments de soute de son gestionnaire, arrière compris s'il y est
	//! enregistré) ; -1 sans gestionnaire. Écrit au journal pour vérification : le chargement est plafonné par m_iCargoSeats.
	protected static int CountCargoSeats(IEntity vehicle)
	{
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (!compartments)
			return -1;
		array<BaseCompartmentSlot> seats = {};
		compartments.GetFreeCompartmentsOfType(seats, ECompartmentType.CARGO);
		return seats.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Les groupes de passagers, de taille exacte : le premier fait la moitié du chargement arrondie au pair au-dessus (6 au
	//! plus), les suivants le reste (10 = 6 + 4, 6 = 4 + 2, 4 = 2 + 2, 2 = un binôme, 12 = 6 + 6) ; faute de prefab de
	//! la bonne taille, la taille en dessous. Vague de mission (« sectionsOnly ») : sections de 4 à 6 seulement, jamais de
	//! binôme (4, 6, 8 = 4 + 4, 10 = 6 + 4, 12 = 6 + 6, 14 = 6 + 4 + 4). Un chargement impair est arrondi au pair EN
	//! DESSOUS (7 = 4 + 2) : jamais un soldat de plus que demandé (place Q7 demandée pour ce chargement, moitié gardée
	//! par la source C2). Retourne les soldats prévus.
	protected int PlanCargo(SRP_EnemyComponent enemies, int load, bool sectionsOnly, array<ResourceName> prefabs)
	{
		int total = 0;
		int guard = 0;
		int minSize = 2;
		if (sectionsOnly)
			minSize = WAVE_MIN_LOAD;
		int left = EvenFloor(load);
		if (left < minSize)
			left = minSize;
		while (left >= minSize && guard < 10)
		{
			guard++;
			int size = left;
			if (sectionsOnly)
				size = WaveSectionSize(left);
			else
			{
				if (prefabs.IsEmpty())
					size = EvenCeil(left / 2);
				if (size > 6)
					size = 6;
				if (size > left)
					size = left;
			}
			ResourceName chosen = "";
			int trySize = size;
			while (chosen.IsEmpty() && trySize >= minSize)
			{
				chosen = PickCargoPrefab(enemies, trySize);
				if (chosen.IsEmpty())
					trySize -= 2;
			}
			if (chosen.IsEmpty())
				break;
			int actual = enemies.GroupSize(chosen);
			if (actual <= 0)
				break;
			prefabs.Insert(chosen);
			total += actual;
			left -= actual;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de groupes de passagers pour ce chargement (même découpe que PlanCargo, pour le budget de groupes)
	protected static int CargoGroupCount(int load, bool sectionsOnly)
	{
		int groups = 0;
		int minSize = 2;
		if (sectionsOnly)
			minSize = WAVE_MIN_LOAD;
		int left = EvenFloor(load);
		if (left < minSize)
			left = minSize;
		while (left >= minSize && groups < 10)
		{
			int size = left;
			if (sectionsOnly)
				size = WaveSectionSize(left);
			else
			{
				if (groups == 0)
					size = EvenCeil(left / 2);
				if (size > 6)
					size = 6;
				if (size > left)
					size = left;
			}
			left -= size;
			groups++;
		}
		return groups;
	}

	//------------------------------------------------------------------------------------------------
	//! Vague de mission : la taille de la prochaine section pour « left » soldats (pair, 4 au moins) : 6, sauf 8 = 4 + 4
	//! et 4, pour ne jamais laisser un binôme
	protected static int WaveSectionSize(int left)
	{
		if (left == 8 || left < 6)
			return 4;
		return 6;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe de passagers de « size » soldats : binômes de sentinelles pour 2, sections pour 4 et 6, sinon un groupe
	//! de cette taille (jamais le groupe de commandement : PickPrefabOfSize) ; "" s'il n'y en a pas
	protected static ResourceName PickCargoPrefab(SRP_EnemyComponent enemies, int size)
	{
		ResourceName prefab = "";
		if (size <= 2)
			prefab = enemies.PickPrefabOfSize(enemies.GetSentryPrefabs(), size, size);
		if (prefab.IsEmpty())
			prefab = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), size, size);
		if (prefab.IsEmpty())
			prefab = enemies.PickGroupPrefabOfSize(size, size);
		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! L'entier pair égal ou juste au-dessus (entier positif)
	protected static int EvenCeil(int value)
	{
		if (value - (value / 2) * 2 == 1)
			return value + 1;
		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! L'entier pair égal ou juste en dessous (0 pour un entier négatif ou nul)
	protected static int EvenFloor(int value)
	{
		if (value <= 0)
			return 0;
		return (value / 2) * 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Chargement selon les joueurs proches : plein dès m_iFullFromPlayers, sinon m_iSmallPerPlayer par joueur au-delà du
	//! premier, entre m_iSmallMin et m_iSmallMax — territoire, Commandeur (camion d'entrée #69)
	int LoadFor(int players)
	{
		if (players >= m_iFullFromPlayers)
			return m_iCargoFull;
		int smallMax = m_iSmallMax;
		if (smallMax < m_iSmallMin)
			smallMax = m_iSmallMin;
		return Math.ClampInt(m_iSmallPerPlayer * (players - 1), m_iSmallMin, smallMax);
	}

	//------------------------------------------------------------------------------------------------
	//! Chargement plafonné aux places passagers (m_iCargoSeats), 2 au moins — camions, Commandeur
	int CapLoad(int load)
	{
		int seats = m_iCargoSeats;
		if (seats < 2)
			seats = 2;
		int capped = load;
		if (capped > seats)
			capped = seats;
		if (capped < 2)
			capped = 2;
		return capped;
	}

	//------------------------------------------------------------------------------------------------
	// À bord : embarquement, maintien, descente
	//------------------------------------------------------------------------------------------------
	//! Embarquement : chaque soldat livré par la file d'apparition monte à l'arrière (réessayé à chaque passage) ; ceux qui
	//! sont assis y restent (SetForceStayInVehicle) ; le chauffeur est rassis s'il le faut. Retourne les passagers assis.
	protected static int SeatPassengers(SRP_EnemyTruck truck)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
			return 0;
		IEntity driverEntity = FirstMember(truck.m_Driver);
		if (IsFit(driverEntity) && !SRP_EnemyComponent.IsSeatedIn(driverEntity, vehicle))
		{
			ChimeraCharacter driverCharacter = ChimeraCharacter.Cast(driverEntity);
			SCR_CompartmentAccessComponent driverAccess = SCR_CompartmentAccessComponent.Cast(driverEntity.FindComponent(SCR_CompartmentAccessComponent));
			if (driverCharacter && driverAccess && !driverCharacter.IsInVehicle())
				driverAccess.MoveInVehicle(vehicle, ECompartmentType.PILOT);
		}
		int seated = 0;
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(cargo, members);
			foreach (IEntity member : members)
			{
				if (!IsFit(member))
					continue;
				if (SRP_EnemyComponent.IsSeatedIn(member, vehicle))
				{
					SetStayAboard(member, true);
					seated++;
					continue;
				}
				ChimeraCharacter character = ChimeraCharacter.Cast(member);
				if (character && character.IsInVehicle())
					continue;
				SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
				if (access && !access.IsGettingIn() && access.MoveInVehicle(vehicle, ECompartmentType.CARGO))
					SetStayAboard(member, true);
			}
		}
		truck.m_iSeated = seated;
		return seated;
	}

	//------------------------------------------------------------------------------------------------
	//! Passagers assis à l'arrière, en état de combattre
	protected static int CountSeated(SRP_EnemyTruck truck)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
			return 0;
		int seated = 0;
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(cargo, members);
			foreach (IEntity member : members)
			{
				if (IsFit(member) && SRP_EnemyComponent.IsSeatedIn(member, vehicle))
					seated++;
			}
		}
		return seated;
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de l'embarquement ou rappel : plus rien ne sera livré aux groupes de passagers, ceux qui ne sont pas assis sont
	//! retirés (pas des pertes), un groupe sans personne à bord est retiré ; effectif attendu ramené aux soldats gardés
	protected void DropStragglers(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		for (int i = truck.m_aCargo.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup cargo = truck.m_aCargo[i];
			if (!cargo || !cargo.m_Group || cargo.m_Group.IsDeleted())
			{
				truck.m_aCargo.Remove(i);
				continue;
			}
			if (aiWorld)
				aiWorld.PurgeSpawnRequestsForGroup(cargo.m_Group);
			cargo.m_bSpawnClosed = true;	// plus rien ne sera livré (SRP_EnemyComponent.CloseStalledSpawns n'y revient pas)
			SRP_EnemyComponent.NoteSeen(cargo);	// avant de retirer ceux qui n'ont pas de place : ce ne sont pas des pertes
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(cargo, members);
			int aboard = 0;
			foreach (IEntity member : members)
			{
				if (SRP_Utils.IsDead(member))
					continue;
				if (truck.m_Vehicle && SRP_EnemyComponent.IsSeatedIn(member, truck.m_Vehicle))
				{
					aboard++;
					continue;
				}
				RemoveSoldier(cargo, member);
			}
			if (aboard == 0)
			{
				enemies.Delete(cargo);
				truck.m_aCargo.Remove(i);
				continue;
			}
			// Effectif attendu : les soldats livrés et gardés (les demandes purgées ne comptent plus dans les plafonds)
			int kept = cargo.m_iMaxSeen - cargo.m_iRemoved;
			if (kept > 0 && kept < cargo.m_iExpected)
				cargo.m_iExpected = kept;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat retiré par nous (traînard sans place, passager emporté par le camion) : sorti de son groupe tout de suite
	//! (la suppression de l'entité peut attendre la fin de l'image : le décompte des pertes reste juste), puis supprimé ;
	//! compté parmi les soldats retirés, pas parmi les pertes
	protected static void RemoveSoldier(SRP_EnemyGroup record, IEntity member)
	{
		if (!member)
			return;
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (control && record && record.m_Group && !record.m_Group.IsDeleted())
		{
			AIAgent agent = control.GetControlAIAgent();
			if (agent)
				record.m_Group.RemoveAgent(agent);
		}
		SCR_EntityHelper.DeleteEntityAndChildren(member);
		if (record)
			record.m_iRemoved = record.m_iRemoved + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! À chaque passage : le chauffeur reste aux commandes (sauf s'il a sauté) et son IA n'est jamais coupée au loin ;
	//! « passengers » : les passagers assis restent à bord (SetForceStayInVehicle redit à chaque passage)
	protected static void KeepAboard(SRP_EnemyTruck truck, bool passengers)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
			return;
		if (!truck.m_bAmbushed && !truck.m_bAbandon)
		{
			IEntity driverEntity = FirstMember(truck.m_Driver);
			if (driverEntity && SRP_EnemyComponent.IsSeatedIn(driverEntity, vehicle))
			{
				SetStayAboard(driverEntity, true);
				PreventLod(driverEntity);
			}
		}
		if (!passengers)
			return;
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(cargo, members);
			foreach (IEntity member : members)
			{
				if (SRP_EnemyComponent.IsSeatedIn(member, vehicle))
					SetStayAboard(member, true);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les passagers peuvent descendre (et le chauffeur avec « withDriver »)
	protected static void ReleaseAboard(SRP_EnemyTruck truck, bool withDriver)
	{
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(cargo, members);
			foreach (IEntity member : members)
				SetStayAboard(member, false);
		}
		if (!withDriver)
			return;
		IEntity driverEntity = FirstMember(truck.m_Driver);
		if (driverEntity)
			SetStayAboard(driverEntity, false);
	}

	//------------------------------------------------------------------------------------------------
	//! CRX : ce soldat reste à bord (il ne descend pas de lui-même, même sous le feu) ou peut descendre
	static void SetStayAboard(IEntity member, bool stay)
	{
		if (!member || member.IsDeleted())
			return;
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (!control)
			return;
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return;
		SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(agent.FindComponent(SCR_AIInfoComponent));
		if (info)
			info.SetForceStayInVehicle(stay);
	}

	//------------------------------------------------------------------------------------------------
	//! L'IA de ce soldat n'est jamais coupée au loin (au-delà d'environ 1 km des joueurs, le jeu coupe l'IA au niveau de
	//! détail maximal : le chauffeur s'arrêterait en route). PreventMaxLOD ne fait rien à un agent DÉJÀ au niveau maximal
	//! (V generated\AI\AIAgent.c) : il est d'abord ramené un niveau en dessous, comme le fait le jeu
	//! (SCR_ResupplyTaskSolver.PreventMaxLOD), puis son IA est rallumée si elle était coupée. Même chose pour son groupe
	//! (un AIGroup est un AIAgent : ce sont ses points de passage qui mènent le camion). Redit à chaque passage. Public :
	//! le Commandeur tient aussi éveillés ses groupes engagés et les équipages de blindé.
	static void PreventLod(IEntity member)
	{
		if (!member || member.IsDeleted())
			return;
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (!control)
			return;
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return;
		int maxLod = AIAgent.GetMaxLOD();
		if (agent.GetLOD() >= maxLod)
			agent.SetLOD(maxLod - 1);
		agent.PreventMaxLOD();
		if (!agent.IsAIActivated())
			agent.ActivateAI();
		AIGroup group = agent.GetParentGroup();
		if (!group)
			return;
		if (group.GetLOD() >= maxLod)
			group.SetLOD(maxLod - 1);
		group.PreventMaxLOD();
	}

	//------------------------------------------------------------------------------------------------
	//! Le chauffeur a quitté son camion pour de bon : son IA (et celle de son groupe) peut de nouveau être coupée au loin
	//! (public : le Commandeur rend au jeu les groupes qu'il ne tient plus éveillés)
	static void AllowLod(IEntity member)
	{
		if (!member || member.IsDeleted())
			return;
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (!control)
			return;
		AIAgent agent = control.GetControlAIAgent();
		if (!agent)
			return;
		agent.AllowMaxLOD();
		AIGroup group = agent.GetParentGroup();
		if (group)
			group.AllowMaxLOD();
	}

	//------------------------------------------------------------------------------------------------
	//! Journal : l'état de l'IA du chauffeur (« IA du chauffeur : niveau de détail 3 sur 10, active »)
	protected static string DriverAIText(SRP_EnemyTruck truck)
	{
		IEntity driverEntity = FirstMember(truck.m_Driver);
		if (!driverEntity)
			return "IA du chauffeur : pas de chauffeur";
		AIControlComponent control = AIControlComponent.Cast(driverEntity.FindComponent(AIControlComponent));
		if (!control || !control.GetControlAIAgent())
			return "IA du chauffeur : pas d'agent";
		AIAgent agent = control.GetControlAIAgent();
		string active = "coupée";
		if (agent.IsAIActivated())
			active = "active";
		return string.Format("IA du chauffeur : niveau de détail %1 sur %2, %3", agent.GetLOD(), AIAgent.GetMaxLOD(), active);
	}

	//------------------------------------------------------------------------------------------------
	//! Filet du débarquement : les soldats encore assis qui doivent descendre (passagers ; le chauffeur aussi s'il a sauté
	//! ou si le camion reste sur place). Après 10 s : descente animée, puis téléportée après trois essais. Retourne ceux
	//! qui sont encore à bord.
	protected static int ForceExits(SRP_EnemyTruck truck, int now)
	{
		IEntity vehicle = truck.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
			return 0;
		array<IEntity> leaving = {};
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
			SRP_EnemyComponent.GetMembers(cargo, leaving);
		if (truck.m_bAmbushed || truck.m_bAbandon)
			SRP_EnemyComponent.GetMembers(truck.m_Driver, leaving);
		bool force = now - truck.m_iOrderTick >= 10000;
		EGetOutType how = EGetOutType.ANIMATED;
		if (truck.m_iExitTries >= 3)
			how = EGetOutType.TELEPORT;
		int aboard = 0;
		foreach (IEntity member : leaving)
		{
			if (!IsFit(member) || !SRP_EnemyComponent.IsSeatedIn(member, vehicle))
				continue;
			aboard++;
			if (!force)
				continue;
			SetStayAboard(member, false);
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(member.FindComponent(CompartmentAccessComponent));
			if (access)
				access.GetOutVehicle(how, -1, ECloseDoorAfterActions.INVALID, false);
		}
		if (force && aboard > 0)
			truck.m_iExitTries = truck.m_iExitTries + 1;
		return aboard;
	}

	//------------------------------------------------------------------------------------------------
	// Arrivée, embuscade, débarquement
	//------------------------------------------------------------------------------------------------
	//! Le camion s'arrête (plus de point de passage pour le chauffeur) ; la phase de débarquement commence
	protected void BeginDismount(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, int now, string why)
	{
		if (truck.m_Driver)
			enemies.ClearWaypoints(truck.m_Driver);
		truck.m_iPhase = SRP_ETruckPhase.DEBARQUEMENT;
		truck.m_iPhaseTick = now;
		truck.m_iOrderTick = 0;
		truck.m_iExitTries = 0;
		if (!why.IsEmpty())
			TruckLog(TruckLabel(truck) + " : " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! L'ordre de débarquement : les passagers peuvent descendre ; moitié des groupes vers la dernière position connue des
	//! joueurs (assaut), moitié en défense de la localité (m_fDefenseRadiusFactor × rayon) ; tous à l'assaut pour une
	//! vague, un seul groupe ou moins de m_iSplitMin soldats. Colonne vers une localité : tout le monde en défense de la
	//! destination. Renversé : le chauffeur descend aussi, en défense. Les débarqués rejoignent la garnison de la localité ;
	//! le Commandeur est prévenu (camion déchargé, colonne livrée).
	protected void OrderDismount(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		ReleaseAboard(truck, truck.m_bAbandon);
		truck.m_bLanded = true;
		vector target = AssaultTarget(enemies, truck);
		bool allAssault = truck.m_bAllAssault || truck.m_aCargo.Count() < 2 || truck.m_iSeated < m_iSplitMin;
		bool allDefend = truck.m_iColumnId != 0 && !truck.m_bAllAssault;
		float defenseRadius = Math.Max(truck.m_fTownRadius * m_fDefenseRadiusFactor, 30);
		int assault = 0;
		int defense = 0;
		foreach (int index, SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			int fit = CountFit(cargo);
			if (fit == 0)
				continue;
			if (!allDefend && (allAssault || index - (index / 2) * 2 == 0))
			{
				enemies.OrderDismountAndAttack(cargo, target);
				assault += fit;
			}
			else
			{
				enemies.OrderDismountAndDefend(cargo, truck.m_vTown, defenseRadius);
				defense += fit;
			}
		}
		if (truck.m_bAbandon && truck.m_Driver && CountFit(truck.m_Driver) > 0)
		{
			enemies.OrderDismountAndDefend(truck.m_Driver, truck.m_vTown, defenseRadius);
			truck.m_Driver.m_Vehicle = null;	// le camion reste sur place : il n'est plus le sien (nettoyage des transports)
			defense++;
		}
		JoinTown(enemies, truck);
		TruckLog(string.Format("%1 : %2 soldat(s) débarquent, %3 vers la dernière position connue des joueurs (grille %4 / %5), %6 en défense de la localité", TruckLabel(truck), assault + defense, assault, Math.Round(target[0]), Math.Round(target[2]), defense));
		NoteLanded(enemies, truck);
	}

	//------------------------------------------------------------------------------------------------
	//! Cible de l'assaut : la dernière position connue des joueurs (alerte la plus récente à moins du rayon + 600 m) ;
	//! sinon le joueur actif le plus proche du camion à moins de m_fAssaultSearchRadius, à m_fAssaultOffset près ; sinon le
	//! centre (le site pour une vague). Camions du Commandeur (renfort, contact, colonne) : jamais la vraie position des
	//! joueurs (CO5), le contact visé ou le centre à défaut d'alerte.
	protected vector AssaultTarget(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		vector alertPosition;
		if (enemies.MostRecentAlert(truck.m_vTown, truck.m_fTownRadius + 600, alertPosition))
			return alertPosition;
		if (truck.m_bContact || truck.m_iColumnId != 0 || truck.m_sReason == "renfort")
			return truck.m_vTown;
		vector from = truck.m_vTown;
		if (truck.m_Vehicle && !truck.m_Vehicle.IsDeleted())
			from = truck.m_Vehicle.GetOrigin();
		array<IEntity> active = enemies.GetActivePlayers();
		IEntity nearest;
		float best = m_fAssaultSearchRadius;
		foreach (IEntity player : active)
		{
			float distance = vector.Distance(player.GetOrigin(), from);
			if (distance >= best)
				continue;
			best = distance;
			nearest = player;
		}
		if (!nearest)
			return truck.m_vTown;
		float angle = Math.RandomFloat(0, Math.PI2);
		float offset = Math.RandomFloat(0, Math.Max(m_fAssaultOffset, 0));
		return SRP_Placement.OnGround(nearest.GetOrigin() + Vector(Math.Cos(angle) * offset, 0, Math.Sin(angle) * offset));
	}

	//------------------------------------------------------------------------------------------------
	//! Embuscade en route ? (feu réel ou joueur tout près) : "" si non, sinon la raison. Joueur à moins de
	//! m_fAmbushDistance ; avec un joueur à portée de tir (m_fAmbushFireRange) : camion touché (santé en baisse) ou un
	//! homme touché (mort ou inconscient) ; le chauffeur ou un passager sous un feu nourri (suppression >
	//! m_fAmbushSuppression). Sans joueur à portée de tir, une perte vient d'un choc (arbre, clôture, lampadaire) ou d'un
	//! accident : la santé et l'effectif de référence sont repris à chaque passage (plus de fausse embuscade à l'approche).
	protected string AmbushReason(SRP_EnemyTruck truck, array<IEntity> players, vector position)
	{
		float nearest = SRP_Placement.NearestPlayer(position, players);
		if (nearest <= m_fAmbushDistance)
			return string.Format("joueur à %1 m", Math.Round(nearest));
		if (nearest > m_fAmbushFireRange)
		{
			truck.m_fHealthStart = HealthOf(truck.m_Vehicle);
			truck.m_iFitAtStart = CountFitTruck(truck);
		}
		else
		{
			if (HealthOf(truck.m_Vehicle) < truck.m_fHealthStart - 0.02)
				return "camion touché";
			if (CountFitTruck(truck) < truck.m_iFitAtStart)
				return "un homme touché";
		}
		float suppression = MaxSuppression(truck);
		if (suppression > m_fAmbushSuppression)
			return "feu nourri, suppression " + SRP_EnemySenses.Dec2(suppression);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Embuscade : arrêt là où il est, tout le monde (chauffeur compris) saute et attaque le contact ; le camion reste sur
	//! place (confié au nettoyage des transports à la fin du débarquement) ; les débarqués rejoignent la garnison
	protected void StartAmbush(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, vector position, string why, int now)
	{
		truck.m_bAmbushed = true;
		truck.m_bLanded = true;
		vector contact = ContactPoint(enemies, players, position);
		if (truck.m_Driver)
			enemies.ClearWaypoints(truck.m_Driver);
		ReleaseAboard(truck, true);
		int attackers = 0;
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			int fit = CountFit(cargo);
			if (fit == 0)
				continue;
			enemies.OrderDismountAndAttack(cargo, contact);
			attackers += fit;
		}
		if (truck.m_Driver)
		{
			if (CountFit(truck.m_Driver) > 0)
			{
				enemies.OrderDismountAndAttack(truck.m_Driver, contact);
				attackers++;
			}
			truck.m_Driver.m_Vehicle = null;	// le camion reste sur place : il n'est plus le sien
		}
		JoinTown(enemies, truck);
		truck.m_iPhase = SRP_ETruckPhase.DEBARQUEMENT;
		truck.m_iPhaseTick = now;
		truck.m_iOrderTick = now;
		truck.m_iExitTries = 0;
		TruckLog(string.Format("%1 : pris sous le feu en route (%2) à %3 m du stationnement : arrêt, %4 soldat(s) sautent et attaquent (grille %5 / %6)", TruckLabel(truck), why, Math.Round(vector.DistanceXZ(position, truck.m_vPark)), attackers, Math.Round(contact[0]), Math.Round(contact[2])));
		NoteLanded(enemies, truck);
	}

	//------------------------------------------------------------------------------------------------
	//! Le contact d'une embuscade : le joueur le plus proche à portée de tir, sinon l'alerte la plus récente, sinon le camion
	protected vector ContactPoint(SRP_EnemyComponent enemies, array<IEntity> players, vector position)
	{
		IEntity nearest;
		float best = m_fAmbushFireRange;
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;
			float distance = vector.Distance(player.GetOrigin(), position);
			if (distance >= best)
				continue;
			best = distance;
			nearest = player;
		}
		if (nearest)
			return nearest.GetOrigin();
		vector alertPosition;
		if (enemies.MostRecentAlert(position, 600, alertPosition))
			return alertPosition;
		return position;
	}

	//------------------------------------------------------------------------------------------------
	//! Territoire : les débarqués (et le chauffeur s'il a sauté) rejoignent la garnison de la localité (m_sZone : sirène,
	//! capture ; classe GARNISON pour la place) ; ils ne comptent pas dans l'effectif prévu (le complément ne rétrécit pas)
	//! et gardent leur source (renfort du Commandeur). Garnison partie entre-temps : retirés plus tard, hors de vue
	//! (RetireLater) ; colonne : ils sont comptés dans l'effectif de la localité (DeliverColumn). Une vague reste à sa
	//! mission (déjà dans ses groupes).
	protected void JoinTown(SRP_EnemyComponent enemies, SRP_EnemyTruck truck)
	{
		if (truck.m_bJoined || truck.m_sSector.IsEmpty())
			return;
		truck.m_bJoined = true;
		array<ref SRP_EnemyGroup> joining = {};
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			if (cargo && cargo.m_Group && !cargo.m_Group.IsDeleted())
				joining.Insert(cargo);
		}
		SRP_EnemyGroup driver = truck.m_Driver;
		if (driver && !driver.m_Vehicle && driver.m_Group && !driver.m_Group.IsDeleted())
			joining.Insert(driver);
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		SRP_SectorState state;
		if (territory)
			state = territory.FindState(truck.m_sSector);
		bool posted = false;
		if (state && state.m_Sector && state.m_iOwner == SRP_ESectorOwner.ENNEMI)
			posted = !state.m_aGroups.IsEmpty() || territory.IsGarrisonPosted(state.m_sName);
		if (!posted)
		{
			if (truck.m_iColumnId != 0)
				return;	// colonne : ses soldats entrent dans l'effectif de la localité (DeliverColumn)
			enemies.RetireLater(joining);
			TruckLog(TruckLabel(truck) + " : la localité n'a plus de garnison, les débarqués seront retirés plus tard, hors de vue");
			return;
		}
		foreach (SRP_EnemyGroup joiner : joining)
		{
			joiner.m_sZone = state.m_sName;
			if (!state.m_aGroups.Contains(joiner))
				state.m_aGroups.Insert(joiner);
			SRP_CmdCapacity.Tag(joiner, SRP_ECmdCapClass.GARNISON);
		}
		truck.m_bInGarrison = true;
	}

	//------------------------------------------------------------------------------------------------
	// Retour et retrait
	//------------------------------------------------------------------------------------------------
	//! Le camion repart vers son point de départ, par la route, à m_iRoadSpeed
	protected void StartLeaving(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, int now, string why)
	{
		truck.m_iPhase = SRP_ETruckPhase.DEPART;
		truck.m_iPhaseTick = now;
		truck.m_iLastMoveTick = now;	// surveillance du retour (TickLeaving)
		truck.m_iStrikes = 0;
		IEntity vehicle = truck.m_Vehicle;
		if (vehicle && !vehicle.IsDeleted())
		{
			truck.m_vLastPos = vehicle.GetOrigin();
			array<vector> back = {};
			SRP_Placement.BuildRoadRoute(vehicle.GetOrigin(), truck.m_vOrigin, 300, 0, back);
			DriveTo(enemies, truck, back, truck.m_vOrigin, false);
		}
		TruckLog(TruckLabel(truck) + " : repart vers son point de départ (" + why + ")");
	}

	//------------------------------------------------------------------------------------------------
	//! Retrait hors de vue : le chauffeur, le camion, et les passagers encore à bord (retirés par nous, pas des pertes ;
	//! un camion rappelé ou annulé, qui n'a jamais débarqué, emporte ses groupes entiers, rendus d'abord à leur source
	//! s'ils en portent une (ReturnSurvivors, avant Delete) ; des débarqués ne sont jamais retirés ici)
	protected void Vanish(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, float nearest)
	{
		IEntity vehicle = truck.m_Vehicle;
		int removed = 0;
		bool takesGroups = !truck.m_bLanded && OwnsLeftovers(truck);
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			if (!cargo || !cargo.m_Group || cargo.m_Group.IsDeleted())
				continue;
			if (takesGroups)
			{
				removed += CountFit(cargo);
				enemies.ReturnSurvivors(cargo);	// parti de sa source (débité), jamais débarqué : il y rentre
				enemies.Delete(cargo);	// camion rappelé ou annulé : le groupe n'a jamais débarqué
				continue;
			}
			SRP_EnemyComponent.NoteSeen(cargo);
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(cargo, members);
			foreach (IEntity member : members)
			{
				if (!vehicle || SRP_Utils.IsDead(member) || !SRP_EnemyComponent.IsSeatedIn(member, vehicle))
					continue;
				RemoveSoldier(cargo, member);
				removed++;
			}
		}
		if (truck.m_Driver)
			enemies.Delete(truck.m_Driver);
		if (vehicle && !vehicle.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		truck.m_Vehicle = null;
		truck.m_iPhase = SRP_ETruckPhase.FINI;
		TruckLog(string.Format("%1 : retiré hors de vue en repartant (joueur le plus proche à %2 m, %3 soldat(s) encore à bord retirés avec lui)", TruckLabel(truck), Math.Round(nearest), removed));
	}

	//------------------------------------------------------------------------------------------------
	//! Le camion a disparu sans nous (remise à zéro, nettoyage) : chauffeur et passagers qui n'ont pas débarqué sont
	//! retirés plus tard, hors de vue
	protected void Vanished(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, string why)
	{
		array<ref SRP_EnemyGroup> leftovers = {};
		if (truck.m_Driver)
		{
			truck.m_Driver.m_Vehicle = null;	// le camion n'est plus le sien (nettoyage des transports)
			AllowLod(FirstMember(truck.m_Driver));
			leftovers.Insert(truck.m_Driver);
		}
		if (OwnsLeftovers(truck))
		{
			foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
			{
				if (cargo)
					leftovers.Insert(cargo);
			}
		}
		enemies.RetireLater(leftovers);
		truck.m_Vehicle = null;
		truck.m_iPhase = SRP_ETruckPhase.FINI;
		TruckLog(TruckLabel(truck) + " : " + why);
	}

	//------------------------------------------------------------------------------------------------
	//! Camion annulé avant la route (embarquement raté, camion détruit) : il n'est jamais parti (départ réglé à zéro :
	//! rien de débité, soldats d'une colonne rendus) ; tout est retiré s'il n'est vu par personne, sinon il repart (retiré
	//! plus tard, hors de vue)
	protected void CancelTruck(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<IEntity> players, int now, string why)
	{
		SettleDeparture(truck, 0);
		bool seen = false;
		if (truck.m_Vehicle && !truck.m_Vehicle.IsDeleted())
			seen = SRP_Placement.IsVehicleSeenByAnyPlayer(truck.m_Vehicle, players);	// un joueur à bord compte comme vu
		if (seen && IsDriverAboard(truck))
		{
			StartLeaving(enemies, truck, now, "annulé : " + why + " ; vu par un joueur, il sera retiré hors de vue");
			return;
		}
		if (seen)
		{
			// Vu, sans chauffeur : le camion reste (nettoyage des transports), les soldats seront retirés hors de vue
			enemies.RegisterTransport(truck.m_Vehicle);
			ReleaseAboard(truck, true);
			Vanished(enemies, truck, "annulé : " + why + " ; vu par un joueur, le camion reste sur place");
			return;
		}
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			if (!cargo)
				continue;
			enemies.ReturnSurvivors(cargo);	// une source déjà portée est rendue avant Delete (FitOf compte les vivants)
			enemies.Delete(cargo);
		}
		if (truck.m_Driver)
			enemies.Delete(truck.m_Driver);
		if (truck.m_Vehicle && !truck.m_Vehicle.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(truck.m_Vehicle);
		truck.m_Vehicle = null;
		truck.m_iPhase = SRP_ETruckPhase.FINI;
		TruckLog(TruckLabel(truck) + " : annulé (" + why + "), retiré hors de vue");
	}

	//------------------------------------------------------------------------------------------------
	// Conduite
	//------------------------------------------------------------------------------------------------
	//! Les points de passage du chauffeur : les points de route à m_iRoadSpeed, puis la destination (m_iTownSpeed vers la
	//! localité, m_iRoadSpeed au retour). Vitesse par point : CRX (SCR_AIWaypoint.SetVehicleSpeed, km/h).
	protected void DriveTo(SRP_EnemyComponent enemies, SRP_EnemyTruck truck, array<vector> route, vector destination, bool toTown)
	{
		SRP_EnemyGroup driver = truck.m_Driver;
		if (!driver || !driver.m_Group || driver.m_Group.IsDeleted())
			return;
		enemies.ClearWaypoints(driver);
		foreach (vector step : route)
			AddDriveWaypoint(enemies, driver, step, m_iRoadSpeed, 25);
		int lastSpeed = m_iRoadSpeed;
		if (toTown)
			lastSpeed = m_iTownSpeed;
		AddDriveWaypoint(enemies, driver, destination, lastSpeed, 10);
	}

	//------------------------------------------------------------------------------------------------
	//! Un point « Move » pour le chauffeur, sa vitesse (km/h) et son rayon d'arrivée ; retenu parmi les points du groupe
	//! (supprimé avec lui)
	protected static void AddDriveWaypoint(SRP_EnemyComponent enemies, SRP_EnemyGroup driver, vector position, int speed, float completion)
	{
		IEntity entity = enemies.SpawnAt(enemies.GetMoveWaypointPrefab(), position);
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}
		waypoint.SetCompletionRadius(completion);
		SCR_AIWaypoint speedWaypoint = SCR_AIWaypoint.Cast(waypoint);
		if (speedWaypoint && speed > 0)
			speedWaypoint.SetVehicleSpeed(speed);
		driver.m_Group.AddWaypointToGroup(waypoint);
		driver.m_aExtraWaypoints.Insert(entity);
	}

	//------------------------------------------------------------------------------------------------
	// Petits outils
	//------------------------------------------------------------------------------------------------
	protected SRP_EnemyTruck NewTruck(string sector, string owner, string reason, int now)
	{
		m_iNextId++;
		SRP_EnemyTruck truck = new SRP_EnemyTruck();
		truck.m_iId = m_iNextId;
		truck.m_sSector = sector;
		truck.m_sOwner = owner;
		truck.m_sReason = reason;
		truck.m_iCreatedTick = now;
		m_aTrucks.Insert(truck);
		return truck;
	}

	//------------------------------------------------------------------------------------------------
	//! Un camion pour cette localité et cette raison est-il déjà prévu ou en cours ?
	protected bool HasTruckFor(string sector, string reason)
	{
		foreach (SRP_EnemyTruck truck : m_aTrucks)
		{
			if (truck && truck.m_iPhase != SRP_ETruckPhase.FINI && truck.m_sSector == sector && truck.m_sReason == reason)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Camions posés (de l'embarquement au retour) — place des camions, Commandeur
	int CountLaunched()
	{
		int count = 0;
		foreach (SRP_EnemyTruck truck : m_aTrucks)
		{
			if (truck && truck.m_iPhase >= SRP_ETruckPhase.EMBARQUEMENT && truck.m_iPhase <= SRP_ETruckPhase.DEPART)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! La localité est-elle encore ennemie, avec une garnison en état de combattre ?
	protected static bool IsSectorHeld(string sector)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return false;
		SRP_SectorState state = territory.FindState(sector);
		if (!state || !state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || state.m_aGroups.IsEmpty())
			return false;
		return SRP_EnemyComponent.ActiveAgentsNear(state.m_aGroups, state.m_Sector.GetCenter(), 0) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! La localité est-elle encore ennemie (garnison ou non) ?
	protected static bool IsSectorHostile(string sector)
	{
		SRP_SectorState state = FindLocality(sector);
		return state && state.m_Sector && state.m_iOwner == SRP_ESectorOwner.ENNEMI;
	}

	//------------------------------------------------------------------------------------------------
	//! La localité de ce nom (SRP_TerritoryComponent), null si inconnue
	protected static SRP_SectorState FindLocality(string sector)
	{
		if (sector.IsEmpty())
			return null;
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return null;
		return territory.FindState(sector);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone du front sous la position (-1 sans front prêt ou en mer)
	protected static int ZoneAt(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return -1;
		return front.GetZoneAt(position);
	}

	//------------------------------------------------------------------------------------------------
	//! Code de la zone (« S07 »), sinon son numéro
	protected static string ZoneCode(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		string code = "";
		if (front && zone >= 0)
			code = front.GetZoneCode(zone);
		if (code.IsEmpty())
			return zone.ToString();
		return code;
	}

	//------------------------------------------------------------------------------------------------
	//! Le premier soldat d'un groupe (null s'il n'y en a pas)
	protected static IEntity FirstMember(SRP_EnemyGroup record)
	{
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(record, members);
		if (members.IsEmpty())
			return null;
		return members[0];
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat en état de combattre : présent, vivant, conscient
	protected static bool IsFit(IEntity soldier)
	{
		if (!soldier || soldier.IsDeleted() || SRP_Utils.IsDead(soldier))
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(soldier.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.IsUnconscious())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le chauffeur est-il aux commandes, en état de conduire ?
	protected static bool IsDriverAboard(SRP_EnemyTruck truck)
	{
		if (!truck.m_Vehicle || truck.m_Vehicle.IsDeleted())
			return false;
		IEntity driverEntity = FirstMember(truck.m_Driver);
		if (!IsFit(driverEntity))
			return false;
		return SRP_EnemyComponent.IsSeatedIn(driverEntity, truck.m_Vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats d'un groupe en état de combattre
	protected static int CountFit(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return 0;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		int fit = 0;
		foreach (AIAgent agent : agents)
		{
			if (agent && IsFit(agent.GetControlledEntity()))
				fit++;
		}
		return fit;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats du camion en état de combattre, chauffeur compris
	protected static int CountFitTruck(SRP_EnemyTruck truck)
	{
		int fit = CountFit(truck.m_Driver);
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
			fit += CountFit(cargo);
		return fit;
	}

	//------------------------------------------------------------------------------------------------
	//! La plus forte suppression (feu subi, de 0 à 1) parmi le chauffeur et les passagers
	protected static float MaxSuppression(SRP_EnemyTruck truck)
	{
		float strongest = Suppression(truck.m_Driver);
		foreach (SRP_EnemyGroup cargo : truck.m_aCargo)
		{
			float value = Suppression(cargo);
			if (value > strongest)
				strongest = value;
		}
		return strongest;
	}

	//------------------------------------------------------------------------------------------------
	//! La plus forte suppression des soldats d'un groupe (système de menace du jeu : GetSuppressionMeasure)
	protected static float Suppression(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return 0;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		float strongest = 0;
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
			if (!chimera || !chimera.m_UtilityComponent || !chimera.m_UtilityComponent.m_ThreatSystem)
				continue;
			float value = chimera.m_UtilityComponent.m_ThreatSystem.GetSuppressionMeasure();
			if (value > strongest)
				strongest = value;
		}
		return strongest;
	}

	//------------------------------------------------------------------------------------------------
	protected static float HealthOf(IEntity vehicle)
	{
		if (!vehicle)
			return 0;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!damage)
			return 1;
		return damage.GetHealthScaled();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDestroyed(IEntity vehicle)
	{
		if (!vehicle)
			return true;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	protected static float SpeedOf(IEntity vehicle)
	{
		Physics physics = vehicle.GetPhysics();
		if (!physics)
			return 0;
		return physics.GetVelocity().Length();
	}

	//------------------------------------------------------------------------------------------------
	//! « Camion n°3 (Vernon, entrée) », « Camion n°4 (Vernon, renfort de Chotain) », « Camion n°5 (contact S07, contact de
	//! Chotain) », « Camion n°6 (vague mission, vague) »
	protected static string TruckLabel(SRP_EnemyTruck truck)
	{
		string place = truck.m_sSector;
		if (truck.m_bContact)
			place = "contact " + ZoneCode(truck.m_iTargetZone);
		else if (place.IsEmpty())
			place = "vague " + truck.m_sOwner;
		string reason = truck.m_sReason;
		if (!truck.m_sSource.IsEmpty())
			reason += " de " + truck.m_sSource;
		return string.Format("Camion n°%1 (%2, %3)", truck.m_iId, place, reason);
	}

	//------------------------------------------------------------------------------------------------
	protected static string PhaseLabel(int phase)
	{
		if (phase == SRP_ETruckPhase.ATTENTE)
			return "en attente du départ";
		if (phase == SRP_ETruckPhase.EMBARQUEMENT)
			return "embarquement";
		if (phase == SRP_ETruckPhase.ROUTE)
			return "en route";
		if (phase == SRP_ETruckPhase.DEBARQUEMENT)
			return "débarquement";
		if (phase == SRP_ETruckPhase.DEPART)
			return "repart";
		return "fini";
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne de la page Staff : phase, assis/voulus, départ prévu, arrivée visée et réelle, distance
	protected static string TruckLine(SRP_EnemyTruck truck, float distance, int now)
	{
		string line = string.Format("%1 : %2 · %3/%4 soldats à bord", TruckLabel(truck), PhaseLabel(truck.m_iPhase), truck.m_iSeated, truck.m_iLoad);
		if (truck.m_iPhase == SRP_ETruckPhase.ATTENTE && truck.m_bPlanned)
			line += " · départ prévu " + ClockAt(truck.m_iLaunchTick, now);
		if (truck.m_iArriveTarget > 0)
			line += " · arrivée visée " + ClockAt(truck.m_iArriveTarget, now);
		if (truck.m_iArrivedTick > 0)
			line += " · arrivé " + ClockAt(truck.m_iArrivedTick, now);
		line += string.Format(" · %1 m", Math.Round(distance));
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! L'heure du serveur à l'instant « tick » (compteur System.GetTickCount), « 21:14:05 »
	static string ClockAt(int tick, int now)
	{
		int hour;
		int minute;
		int second;
		System.GetHourMinuteSecond(hour, minute, second);
		int total = hour * 3600 + minute * 60 + second + (tick - now) / 1000;
		while (total < 0)
			total += 86400;
		while (total >= 86400)
			total -= 86400;
		int shownHour = total / 3600;
		int shownMinute = (total - shownHour * 3600) / 60;
		int shownSecond = total - shownHour * 3600 - shownMinute * 60;
		return SRP_Time.Pad2(shownHour) + ":" + SRP_Time.Pad2(shownMinute) + ":" + SRP_Time.Pad2(shownSecond);
	}

	//------------------------------------------------------------------------------------------------
	protected void LogPlan(SRP_EnemyTruck truck, int now)
	{
		string aim = "sans heure visée, départ aussitôt";
		if (truck.m_iArriveTarget > 0)
			aim = "arrivée visée " + ClockAt(truck.m_iArriveTarget, now);
		string line = string.Format("%1 : départ prévu %2, %3", TruckLabel(truck), ClockAt(truck.m_iLaunchTick, now), aim);
		line += string.Format(" (%1 soldats, départ à %2 m de la localité, route de %3 m, trajet estimé %4 s embarquement compris)", truck.m_iLoad, Math.Round(vector.DistanceXZ(truck.m_vOrigin, truck.m_vTown)), Math.Round(truck.m_fRouteLength), truck.m_iEstimateMs / 1000);
		TruckLog(line);
	}

	//------------------------------------------------------------------------------------------------
	protected static void LogArrival(SRP_EnemyTruck truck, int now)
	{
		float fromCenter = 0;
		if (truck.m_Vehicle)
			fromCenter = vector.DistanceXZ(truck.m_Vehicle.GetOrigin(), truck.m_vTown);
		string line = string.Format("%1 : arrivé à %2, stationné à %3 m du centre", TruckLabel(truck), ClockAt(now, now), Math.Round(fromCenter));
		if (truck.m_iArriveTarget > 0)
		{
			int gap = (now - truck.m_iArriveTarget) / 1000;
			string sign = "+";
			if (gap < 0)
				sign = "";
			line += string.Format(" (arrivée visée %1, écart %2%3 s)", ClockAt(truck.m_iArriveTarget, now), sign, gap);
		}
		TruckLog(line);
	}

	//------------------------------------------------------------------------------------------------
	protected static void TruckLog(string text)
	{
		SRP_EnemyComponent.Journal("ENNEMI", text);
	}
}
