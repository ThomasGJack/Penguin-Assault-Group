//------------------------------------------------------------------------------------------------
// SimpleRP — Territoire : les LOCALITÉS ennemies et leurs GARNISONS (refonte du front, cartes #75 et #76)
// Propriétaire : l'ennemi du front. La capture, la perte, les contre-attaques, la production, les dépôts, les marqueurs
// et la sauvegarde sont passés au front (SRP_FrontComponent, SRP_ZoneFall, SRP_FrontEnemyComponent, SRP_FrontEconomy,
// SRP_FrontRadio) ; ce composant ne sauvegarde plus rien (la mémoire des pertes F12 est dans front.json, par le socle).
// - Localités : construites une fois le front prêt, d'après SRP_FrontComponent.GetLocality et les points clés posés au
//   Workbench (centre = point clé CENTRE, m_vHQ = point clé QG des villes). Une localité est HOSTILE (m_iOwner =
//   ENNEMI) tant que sa zone est rouge et que ses points clés ne sont pas tous saisis (SRP_ZoneFall.IsLocalityTaken) ;
//   le changement de camp arrive par la façade SRP_FrontEnemyComponent (SetLocalityOwner, WithdrawLocality,
//   AdoptAssailants), avec un rattrapage discret chaque minute (remise à plat, restauration, correction du Staff).
// - Garnison (F11) : une seule pose par passage de 5 s, la localité hostile au plus petit score
//   (SRP_FrontEnemyComponent.GarrisonScore : distance au joueur actif le plus proche, moins l'avance du front), à
//   m_fGarrisonSpawnDistance d'un joueur pour une localité du front, m_fRearGarrisonSpawnDistance pour l'arrière ;
//   m_iGarrisonMaxActive garnisons vivantes au plus (la plus mauvaise cède sa place, hors de vue, loin des joueurs,
//   pas en alerte ni attaquée). Effectif décidé = SRP_FrontEnemyComponent.GetLocalityEffective (Nominal moins pertes
//   F12, envoyés MA2, plus reçus), borné par SRP_EnemyGarrison (GarrisonCap puis Ask) ; sous m_iGarrisonMinSoldiers,
//   rien n'est posé mais la garnison compte comme posée (village vide et prenable, poste de commandement posé).
//   Localité vidée par un décrochage (SRP_CmdManeuvers.IsLocalityEmptied) : ni posée, ni complétée.
// - Pertes F12 : avant chaque retrait (éviction, plus personne, anéantie, capture abandonnée), les soldats manquants
//   de la garnison sont notés (SRP_FrontEnemyComponent.AddLosses / LiveLosses). Les débarqués d'un renfort entrés dans
//   la garnison ne sont jamais supprimés avec elle : RetireSourced les passe à SRP_EnemyComponent.RetireTick, seule
//   porte de retour des soldats envoyés (MA2).
// - Retrait quand plus personne n'est à m_fGarrisonDespawnDistance pendant 10 min ; anéantie sans capture : oubliée
//   dès que plus aucun joueur n'est à m_fGarrisonWipeClearDistance. Pose différée (joueur dans la localité), complément
//   quand d'autres joueurs arrivent, posture de nuit : par SRP_EnemyGarrison, comme la carte #69.
// - Camions et radio (SRP_EnemyTrucks) : camion d'entrée à l'approche (Commandeur actif :
//   SRP_CmdManeuvers.RequestEntryTruck, même frein C2, et plus de camion d'alerte) ; camion d'alerte par radio sinon ;
//   opérateur radio tué : le premier appel qui suit est retardé (SRP_Commander.RadioDelayMinutes).
// - Sirène (SRP_SireneComponent) : un poteau au point clé centre d'une localité garnie ; muet après la prise.
// - Menace : lue et écrite sur SRP_FrontComponent (GetThreat et AddThreat ne font que relayer) ; la garnison suit la
//   menace LOCALE (SRP_Commander.ThreatAt) et l'officier de la région (SRP_Commander.GarrisonFactorAt).
// APPELÉ PAR : SRP_FrontEnemyComponent (façade des localités), SRP_ZoneFall (par la façade), SRP_Enemy (alerte,
// radio, ReducePlanned), SRP_EnemyTrucks et SRP_Sirene (localités), SRP_HeliSearch, SRP_Missions (OnMissionSuccess,
// OnCaptureFailed), SRP_Admin (rapport radio), SRP_Delivery (menace).
//------------------------------------------------------------------------------------------------

// Ancien type de production d'un secteur : gardé tel quel, le territoire ne produit plus rien (SRP_FrontEconomy)
enum SRP_ESectorType
{
	VILLAGE,		// produit des vivres
	CARREFOUR,		// ne produit rien
	DEPOT,			// carburant
	CARRIERE,		// pièces
	POSTE			// munitions
}

// Camp d'une LOCALITÉ (mêmes valeurs que SRP_EFrontOwner) : ENNEMI = hostile (zone rouge, points clés pas tous saisis)
enum SRP_ESectorOwner
{
	ENNEMI = 0,
	NOUS = 1
}

//------------------------------------------------------------------------------------------------
//! Définition d'une localité de zone, d'après le front (SRP_FrontLocality et ses points clés posés au Workbench)
class SRP_SectorDef
{
	string m_sName;
	vector m_vCenter;				// point clé CENTRE : garnison, sirène, camions, rayon
	float m_fRadius = 150;
	bool m_bAuto;
	string m_sKind = "village";		// genre de la localité (carte #69, livraison 3) : « hameau », « village » ou « ville »
	vector m_vHQ;					// point clé QG d'une ville ou d'un bourg (D6), si m_bHasHQ
	bool m_bHasHQ;
	int m_iZone = -1;				// zone du front de son centre

	vector GetCenter()
	{
		return m_vCenter;
	}
}

//------------------------------------------------------------------------------------------------
//! État d'une localité (vivant : rien n'est sauvegardé ici, la mémoire des pertes F12 est au front)
class SRP_SectorState
{
	string m_sName;
	int m_iOwner = SRP_ESectorOwner.ENNEMI;
	string m_sZoneCode;				// code de sa zone, « S07 »

	ref SRP_SectorDef m_Sector;
	ref array<ref SRP_EnemyGroup> m_aGroups = {};
	int m_iNoPlayerSince;
	int m_iActiveInZone;			// ennemis en état de combattre dans la zone, au dernier passage

	// Garnison posée pour cet épisode (F12, trou 21) : vrai même vide (effectif décidé sous m_iGarrisonMinSoldiers) ou
	// vidée par un décrochage ; remis à faux quand la garnison est retirée, anéantie, abandonnée ou la localité prise
	bool m_bPosted;
	bool m_bRetreatDone;			// un décrochage par pose (SRP_CmdManeuvers), remis à faux avec la garnison
	int m_iOwnerMismatch;			// rattrapage du camp : passages (60 s) où le camp diffère de celui du front

	// Pose différée (carte #69, correctif du 25/09) : groupes de la garnison pas encore posés faute de point hors de vue
	// (un joueur dans le secteur) ou faute de place (SRP_CmdCapacity.Ask, carte #76), gardés tels quels (livraison 3),
	// prochain essai, ligne de journal déjà écrite pour cet épisode
	ref array<ref SRP_GarrisonSlot> m_aDeferredSlots = {};
	int m_iDeferRetryTick;
	bool m_bDeferLogged;

	// Garnison (carte #69, livraison 3) : soldats prévus (attendus des groupes posés, morts compris : pas de remplacement
	// des pertes ; le complément ajoute « voulus - prévus »), posture en place (vrai = nuit)
	int m_iPlannedSoldiers;
	bool m_bNightPosture;
	int m_iTopUpRetryTick;			// complément qui n'a rien posé (plafond, repli refusé) : pas de nouvel essai avant

	// Radio et camions (carte #69, livraison 3, SRP_EnemyTrucks) : opérateur radio de la garnison, radio perdue et appel
	// retardé jusqu'à ; camion d'entrée (moment de l'entrée d'un joueur dans la zone, camion parti) ; camion d'alerte
	// (échéance, camion parti). Tous remis à zéro avec la garnison (ResetGarrisonFlags).
	IEntity m_RadioOperator;
	bool m_bRadioLost;
	int m_iRadioBlockedUntil;
	int m_iEntryTick;
	bool m_bEntryTruckDone;
	int m_iAlertTruckDue;
	bool m_bAlertTruckDone;

	// Sirène (carte #69) : le poteau posé au centre (reste en place, muet, après une capture), un poteau à (re)poser
	// après la pose de la garnison, poses manquées depuis, prochain essai
	IEntity m_Siren;
	bool m_bPoleWanted;
	int m_iPoleTries;
	int m_iNextPoleTry;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Territoire SimpleRP : localités ennemies et leurs garnisons (radio, sirène, camions)")]
class SRP_TerritoryComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_TerritoryComponent : SCR_BaseGameModeComponent
{
	[Attribute("3", UIWidgets.EditBox, "Garnisons vivantes en même temps, maximum (les localités au meilleur score d'abord : front, puis distance aux joueurs)", category: "SimpleRP - Localités")]
	protected int m_iGarrisonMaxActive;

	[Attribute("2000", UIWidgets.EditBox, "Une localité ennemie AU FRONT (zone au contact) reçoit sa garnison quand un joueur actif approche à … mètres de son point clé", category: "SimpleRP - Localités")]
	protected float m_fGarrisonSpawnDistance;

	[Attribute("2000", UIWidgets.EditBox, "Une localité ennemie de l'ARRIÈRE reçoit sa garnison quand un joueur actif approche à … mètres (elle passe après celles du front)", category: "SimpleRP - Localités")]
	protected float m_fRearGarrisonSpawnDistance;

	[Attribute("2", UIWidgets.EditBox, "Effectif décidé (pertes retirées) sous lequel rien n'est posé : la localité est vide et prenable, sa garnison compte comme posée", category: "SimpleRP - Localités")]
	protected int m_iGarrisonMinSoldiers;

	[Attribute("3500", UIWidgets.EditBox, "La garnison est retirée si plus aucun joueur n'est à moins de … mètres pendant 10 min", category: "SimpleRP - Localités")]
	protected float m_fGarrisonDespawnDistance;

	[Attribute("1500", UIWidgets.EditBox, "Garnison anéantie sans capture : une nouvelle garnison est possible dès que plus aucun joueur n'est à moins de … mètres", category: "SimpleRP - Localités")]
	protected float m_fGarrisonWipeClearDistance;

	[Attribute("1200", UIWidgets.EditBox, "Plafond de garnisons atteint : la garnison au plus mauvais score cède sa place à une localité au score meilleur d'au moins 300, si personne n'est à moins de … mètres d'elle, qu'aucun de ses groupes n'est vu et qu'elle n'est ni en alerte ni attaquée", category: "SimpleRP - Localités")]
	protected float m_fGarrisonEvictDistance;

	[Attribute("150", UIWidgets.EditBox, "Sirène : un joueur vu par la garnison déclenche la sirène s'il est à moins du rayon du secteur + … mètres du centre", category: "SimpleRP - Garnison")]
	protected float m_fSirenZoneMargin;

	// Effectif et composition des garnisons (carte #69, livraison 3, chiffres de Jack du 25/09)
	[Attribute("10", UIWidgets.EditBox, "Effectif d'un HAMEAU, en soldats, pour 1 à « joueurs compris » joueurs proches", category: "SimpleRP - Garnison")]
	protected int m_iSoldiersHamlet;

	[Attribute("20", UIWidgets.EditBox, "Effectif d'un VILLAGE, en soldats", category: "SimpleRP - Garnison")]
	protected int m_iSoldiersVillage;

	[Attribute("30", UIWidgets.EditBox, "Effectif d'une VILLE ou d'un bourg, en soldats (le QG compris)", category: "SimpleRP - Garnison")]
	protected int m_iSoldiersTown;

	[Attribute("2", UIWidgets.EditBox, "Joueurs compris dans l'effectif de base (joueurs proches : à moins de 3 km, hors base et délai de grâce)", category: "SimpleRP - Garnison")]
	protected int m_iPlayersIncluded;

	[Attribute("3", UIWidgets.EditBox, "Soldats en plus par joueur proche au-delà des joueurs compris", category: "SimpleRP - Garnison")]
	protected float m_fSoldiersPerExtraPlayer;

	[Attribute("18", UIWidgets.EditBox, "Soldats en plus pour les joueurs, au plus (18 = jusqu'à 8 joueurs proches)", category: "SimpleRP - Garnison")]
	protected int m_iPlayerBonusMax;

	[Attribute("0.5", UIWidgets.EditBox, "Soldats en plus par point de menace (0-10), arrondi au-dessus : la menace ajoute toujours au moins un soldat", category: "SimpleRP - Garnison")]
	protected float m_fSoldiersPerThreat;

	[Attribute("60", UIWidgets.EditBox, "Effectif d'une garnison, au plus, en soldats (60 ; le plus petit de ce réglage et de la clé cap_garnison_max du Commandeur ; le plafond des 120 soldats ennemis est tenu par SRP_CmdCapacity)", category: "SimpleRP - Garnison")]
	protected int m_iGarrisonSoldiersMax;

	[Attribute("13", UIWidgets.EditBox, "QG (groupe de commandement : opérateur radio, tireur d'élite) seulement pour une garnison de VILLE d'au moins … soldats", category: "SimpleRP - Garnison")]
	protected int m_iCommandMinSoldiers;

	[Attribute("1", UIWidgets.EditBox, "Postes d'entrée (binômes de sentinelles sur les routes) d'un hameau", category: "SimpleRP - Garnison")]
	protected int m_iPostsHamlet;

	[Attribute("1", UIWidgets.EditBox, "Postes d'entrée d'un village", category: "SimpleRP - Garnison")]
	protected int m_iPostsVillage;

	[Attribute("2", UIWidgets.EditBox, "Postes d'entrée d'une ville", category: "SimpleRP - Garnison")]
	protected int m_iPostsTown;

	[Attribute("50", UIWidgets.EditBox, "Part des sections qui font une ronde sur les rues, LE JOUR, en pour cent (jamais un binôme)", category: "SimpleRP - Garnison")]
	protected int m_iPatrolPercentDay;

	[Attribute("20", UIWidgets.EditBox, "Part des sections qui font une ronde sur les rues, LA NUIT, en pour cent", category: "SimpleRP - Garnison")]
	protected int m_iPatrolPercentNight;

	[Attribute("40", UIWidgets.EditBox, "Section qui ne fait pas de ronde : chance de tenir un bâtiment (sinon un poste), en pour cent", category: "SimpleRP - Garnison")]
	protected int m_iBuildingPercent;

	[Attribute("1", UIWidgets.CheckBox, "Posture de nuit : sans alerte, les rondes en trop passent en poste aux entrées à la tombée de la nuit, et reprennent leur ronde au jour", category: "SimpleRP - Garnison")]
	protected bool m_bNightPosture;

	[Attribute("0.3", UIWidgets.EditBox, "Centre de la garnison : posé à moins de … × le rayon du secteur du centre…", category: "SimpleRP - Garnison")]
	protected float m_fCenterRingFactor;

	[Attribute("40", UIWidgets.EditBox, "… et à moins de … mètres", category: "SimpleRP - Garnison")]
	protected float m_fCenterRingMax;

	[Attribute("15", UIWidgets.EditBox, "Poste d'entrée : rayon du point Defend, en mètres", category: "SimpleRP - Garnison")]
	protected float m_fEntryDefendRadius;

	[Attribute("20", UIWidgets.EditBox, "Poste d'entrée : maintien de position, en mètres", category: "SimpleRP - Garnison")]
	protected float m_fEntryHoldRadius;

	[Attribute("8", UIWidgets.EditBox, "Bâtiment : rayon du point Defend, en mètres", category: "SimpleRP - Garnison")]
	protected float m_fBuildingDefendRadius;

	[Attribute("12", UIWidgets.EditBox, "Bâtiment : maintien de position, en mètres", category: "SimpleRP - Garnison")]
	protected float m_fBuildingHoldRadius;

	[Attribute("100", UIWidgets.EditBox, "Poste d'une section : maintien de position au plus, en mètres (jamais plus que le rayon du secteur)", category: "SimpleRP - Garnison")]
	protected float m_fPostHoldMax;

	[Attribute("5", UIWidgets.EditBox, "Radio : l'opérateur radio de la garnison tué (ou inconscient, ou rendu), le premier appel qui suit (camion d'alerte ou hélicoptère) est retardé de … minutes, puis la radio est reprise ; aucun message aux joueurs. Repli sans Commandeur : avec lui, c'est sa clé cmd_radio_retard_min", category: "SimpleRP - Garnison")]
	protected int m_iRadioDelayMinutes;

	[Attribute("150", UIWidgets.EditBox, "Rayon d'un HAMEAU, en mètres, autour de son point clé (découplé du rayon de la zone civile)", category: "SimpleRP - Localités")]
	protected float m_fSectorRadiusHamlet;

	[Attribute("250", UIWidgets.EditBox, "Rayon d'un VILLAGE, en mètres", category: "SimpleRP - Localités")]
	protected float m_fSectorRadiusVillage;

	[Attribute("350", UIWidgets.EditBox, "Rayon d'un BOURG ou d'une VILLE, en mètres", category: "SimpleRP - Localités")]
	protected float m_fSectorRadiusTown;

	protected static const int TICK_MS = 5000;		// le suivi tourne toutes les 5 s
	protected static const int DEFER_RETRY_MS = 30000;	// pose de garnison différée : nouvel essai toutes les 30 s
	protected static const int TOPUP_RETRY_MS = 60000;	// complément qui n'a rien posé : nouvel essai dans 60 s
	protected static const int START_RETRY_MS = 2000;	// front pas encore prêt : nouvel essai du démarrage toutes les 2 s
	protected static const int START_WARN_TRIES = 60;	// … et une ligne d'avertissement au bout de 2 min
	protected static const int SYNC_TICKS = 12;		// rattrapage du camp des localités : une fois par minute
	protected static const float CONTACT_MARGIN = 400;	// complément seulement si aucun joueur n'est à moins du rayon + … m

	protected static SRP_TerritoryComponent s_Instance;

	protected ref array<ref SRP_SectorState> m_aStates = {};
	protected bool m_bBuilt;			// localités construites d'après le front
	protected int m_iStartTries;
	protected int m_iSyncCountdown;		// passages avant le prochain rattrapage du camp

	//------------------------------------------------------------------------------------------------
	static SRP_TerritoryComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : réglages des garnisons copiés, puis Start dans 5 s (jamais de travail ici : le Workbench crée une
	//! instance jetable au lancement)
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		s_Instance = this;
		SRP_Paths.EnsureDirectories();
		PushGarrisonSettings();
		GetGame().GetCallqueue().CallLater(Start, 5000, false);	// les localités attendent que le front soit prêt
	}

	//------------------------------------------------------------------------------------------------
	//! Réglages de composition et de pose des garnisons, copiés dans SRP_EnemyGarrison (statique)
	protected void PushGarrisonSettings()
	{
		SRP_EnemyGarrison.s_iPostsHamlet = Math.Max(m_iPostsHamlet, 0);
		SRP_EnemyGarrison.s_iPostsVillage = Math.Max(m_iPostsVillage, 0);
		SRP_EnemyGarrison.s_iPostsTown = Math.Max(m_iPostsTown, 0);
		SRP_EnemyGarrison.s_bNightPosture = m_bNightPosture;
		SRP_EnemyGarrison.s_iCommandMinSoldiers = m_iCommandMinSoldiers;
		SRP_EnemyGarrison.s_iPatrolPercentDay = Math.ClampInt(m_iPatrolPercentDay, 0, 100);
		SRP_EnemyGarrison.s_iPatrolPercentNight = Math.ClampInt(m_iPatrolPercentNight, 0, 100);
		SRP_EnemyGarrison.s_iBuildingPercent = Math.ClampInt(m_iBuildingPercent, 0, 100);
		SRP_EnemyGarrison.s_fCenterRingFactor = Math.Clamp(m_fCenterRingFactor, 0.05, 1);
		SRP_EnemyGarrison.s_fCenterRingMax = Math.Max(m_fCenterRingMax, 10);
		SRP_EnemyGarrison.s_fEntryDefendRadius = Math.Max(m_fEntryDefendRadius, 5);
		SRP_EnemyGarrison.s_fEntryHoldRadius = Math.Max(m_fEntryHoldRadius, 5);
		SRP_EnemyGarrison.s_fBuildingDefendRadius = Math.Max(m_fBuildingDefendRadius, 3);
		SRP_EnemyGarrison.s_fBuildingHoldRadius = Math.Max(m_fBuildingHoldRadius, 3);
		SRP_EnemyGarrison.s_fPostHoldMax = Math.Max(m_fPostHoldMax, 10);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : Start et Tick retirés de la file (rien à sauvegarder ici : la mémoire des pertes est au front)
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Start);
			GetGame().GetCallqueue().Remove(Tick);
			if (s_Instance == this)
				s_Instance = null;
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, 5 s après OnPostInit : attend que le front soit prêt (nouvel essai toutes les 2 s), puis construit les
	//! localités et lance le suivi de 5 s
	protected void Start()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
		{
			m_iStartTries++;
			if (m_iStartTries == START_WARN_TRIES)
				Print("[SRP] Territoire : le front n'est toujours pas prêt (SRP_FrontComponent absent du game mode ?), les garnisons attendent", LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(Start, START_RETRY_MS, false);
			return;
		}
		BuildLocalities(front);
		m_bBuilt = true;
		m_iSyncCountdown = SYNC_TICKS;
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Les localités de zone du front (une seule source : SRP_FrontComponent.GetLocality, hameaux compris, jamais celles
	//! de la base) : centre au point clé CENTRE, QG au point clé QG d'une ville, rayon selon le genre, camp d'après le
	//! front (hostile : zone rouge et points clés pas tous saisis)
	protected void BuildLocalities(SRP_FrontComponent front)
	{
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		int hostile = 0;
		int withHQ = 0;
		int count = front.GetLocalityCount();
		for (int index = 0; index < count; index++)
		{
			SRP_FrontLocality place = front.GetLocality(index);
			if (!place || place.m_bHome || place.m_sName.IsEmpty())
				continue;
			if (place.m_iZone < 0 || front.IsBaseZone(place.m_iZone))
				continue;

			SRP_SectorDef def = new SRP_SectorDef();
			def.m_sName = place.m_sName;
			def.m_sKind = KindOf(place.m_sKind);
			def.m_fRadius = SectorRadiusFor(place.m_sKind);
			def.m_bAuto = true;
			def.m_iZone = place.m_iZone;
			// Centre : le point clé CENTRE (repère, fichier ou nom de la carte, Q8) ; à défaut le centre de la zone civile
			def.m_vCenter = place.m_vLabel;
			SRP_FrontKeyPoint centre;
			if (place.m_iCentre >= 0)
				centre = front.GetKeyPoint(place.m_iCentre);
			if (centre)
				def.m_vCenter = centre.m_vPos;
			// QG d'une ville ou d'un bourg (D6) : seulement s'il est posé (jamais sur le nom de la carte)
			SRP_FrontKeyPoint hq;
			if (place.m_iQg >= 0)
				hq = front.GetKeyPoint(place.m_iQg);
			if (hq)
			{
				def.m_vHQ = hq.m_vPos;
				def.m_bHasHQ = true;
				withHQ++;
			}

			SRP_SectorState state = FindState(def.m_sName);
			if (!state)
			{
				state = new SRP_SectorState();
				state.m_sName = def.m_sName;
				m_aStates.Insert(state);
			}
			state.m_Sector = def;
			state.m_sZoneCode = front.GetZoneCode(place.m_iZone);
			state.m_iOwner = SRP_ESectorOwner.NOUS;
			if (IsHostile(front, zoneFall, state))
			{
				state.m_iOwner = SRP_ESectorOwner.ENNEMI;
				hostile++;
			}
		}
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("Localités : %1 d'après le front, %2 hostile(s), %3 prise(s), %4 ville(s) avec un QG", m_aStates.Count(), hostile, m_aStates.Count() - hostile, withHQ));
	}

	//------------------------------------------------------------------------------------------------
	//! Localité hostile d'après le front (F11) : sa zone est rouge et ses points clés ne sont pas tous saisis
	protected bool IsHostile(SRP_FrontComponent front, SRP_ZoneFall zoneFall, SRP_SectorState state)
	{
		if (!front || !state || !state.m_Sector)
			return false;
		int zone = state.m_Sector.m_iZone;
		if (zone < 0 || front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE)
			return false;
		if (zoneFall && zoneFall.IsLocalityTaken(state.m_sName))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Même chose, lue à l'instant sur le front (garde de la pose : jamais de garnison dans une localité prise)
	protected bool IsHostileNow(SRP_SectorState state)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return state.m_iOwner == SRP_ESectorOwner.ENNEMI;
		return IsHostile(front, front.GetZoneFall(), state);
	}

	//------------------------------------------------------------------------------------------------
	//! Rattrapage du camp des localités d'après le front, une fois par minute (et d'un coup à la remise à zéro) : une
	//! remise à plat, une restauration ou une correction du Staff (carré forcé) ne passent pas toujours par la façade.
	//! Un écart doit être vu deux fois de suite (« immediate » : aussitôt). Localité devenue à nous : ses défenseurs se
	//! retirent (WithdrawLocality) ; redevenue hostile : sa garnison sera posée à la prochaine approche.
	protected void SyncOwners(bool immediate)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		foreach (SRP_SectorState state : m_aStates)
		{
			if (!state.m_Sector)
				continue;
			int owner = SRP_ESectorOwner.NOUS;
			if (IsHostile(front, zoneFall, state))
				owner = SRP_ESectorOwner.ENNEMI;
			if (owner == state.m_iOwner)
			{
				state.m_iOwnerMismatch = 0;
				continue;
			}
			state.m_iOwnerMismatch++;
			if (!immediate && state.m_iOwnerMismatch < 2)
				continue;
			state.m_iOwnerMismatch = 0;
			state.m_iOwner = owner;
			if (owner == SRP_ESectorOwner.NOUS)
			{
				WithdrawLocality(state.m_sName, "rattrapage d'après le front");
				SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : localité à nous d'après le front (rattrapage : remise à plat, restauration ou correction du Staff)");
			}
			else
				SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : localité de nouveau hostile d'après le front (rattrapage), garnison à la prochaine approche");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rayon de secteur d'une localité : hameau 150, village 250, bourg et ville 350 (réglables)
	protected float SectorRadiusFor(string kind)
	{
		float radius = m_fSectorRadiusVillage;
		if (kind == "hameau")
			radius = m_fSectorRadiusHamlet;
		else if (kind == "bourg" || kind == "ville")
			radius = m_fSectorRadiusTown;
		return Math.Max(radius, 60.0);
	}

	//------------------------------------------------------------------------------------------------
	//! Genre de garnison d'un lieu de la carte (carte #69, livraison 3) : hameau, village, ville (bourg compris)
	protected string KindOf(string placeKind)
	{
		if (placeKind == "hameau")
			return "hameau";
		if (placeKind == "bourg" || placeKind == "ville")
			return "ville";
		return "village";
	}

	//------------------------------------------------------------------------------------------------
	// Consultation
	//------------------------------------------------------------------------------------------------
	SRP_SectorState FindState(string name)
	{
		foreach (SRP_SectorState state : m_aStates)
		{
			if (state.m_sName == name)
				return state;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Menace de l'île (0-10) : ne fait plus que relayer SRP_FrontComponent.GetThreat (0 sans front)
	int GetThreat()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return 0;
		return front.GetThreat();
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les localités de zone, hostiles ou prises (remplace GetStates) — camions, sirène, Staff
	void GetLocalities(notnull array<SRP_SectorState> result)
	{
		foreach (SRP_SectorState state : m_aStates)
		{
			if (state.m_Sector)
				result.Insert(state);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les localités hostiles (remplace GetEnemySectors) — hélico de recherche, cachettes, missions
	void GetEnemyLocalities(notnull array<SRP_SectorState> result)
	{
		foreach (SRP_SectorState state : m_aStates)
		{
			if (state.m_Sector && state.m_iOwner == SRP_ESectorOwner.ENNEMI)
				result.Insert(state);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Portée de pose d'une localité : m_fGarrisonSpawnDistance au front (zone au contact, F11),
	//! m_fRearGarrisonSpawnDistance à l'arrière
	protected float SpawnRangeFor(SRP_FrontEnemyComponent frontEnemy, SRP_SectorState state)
	{
		if (frontEnemy && !frontEnemy.IsLocalityOnFront(state))
			return m_fRearGarrisonSpawnDistance;
		return m_fGarrisonSpawnDistance;
	}

	//------------------------------------------------------------------------------------------------
	//! Score de garnison (F11, plus petit = prioritaire) : SRP_FrontEnemyComponent.GarrisonScore, sinon la distance
	protected float ScoreOf(SRP_FrontEnemyComponent frontEnemy, SRP_SectorState state, float reach)
	{
		if (frontEnemy)
			return frontEnemy.GarrisonScore(state, reach);
		return reach;
	}

	//------------------------------------------------------------------------------------------------
	//! Effectif DÉCIDÉ d'une pose ou d'un complément : SRP_FrontEnemyComponent.GetLocalityEffective (Nominal moins pertes
	//! F12 et envoyés, plus reçus), SRP_EnemyGarrison n'y applique que la place (GarrisonCap puis Ask) ; sans ennemi du
	//! front : l'effectif Nominal
	protected int DecidedSoldiers(SRP_EnemyComponent enemies, SRP_SectorState state)
	{
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (frontEnemy)
			return frontEnemy.GetLocalityEffective(state.m_sName);
		return GarrisonSoldiers(enemies, state.m_Sector);
	}

	//------------------------------------------------------------------------------------------------
	//! Localité vidée par un décrochage (MA7) : ni posée ni complétée tant qu'un joueur est près (Commandeur)
	protected bool IsEmptied(SRP_SectorState state)
	{
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		return maneuvers && maneuvers.IsLocalityEmptied(state.m_sName);
	}

	//------------------------------------------------------------------------------------------------
	//! F12 : avant un retrait, les soldats manquants de la garnison posée (prévus moins survivants en état de combattre)
	//! s'ajoutent aux pertes de la localité ; rien à noter, rien n'est écrit (la date de regarnissage ne bouge pas).
	//! Après un décrochage (MA6), les pertes et les partants ont déjà été notés par SRP_CmdManeuvers : rien de plus
	//! (sinon les partants, sortis des groupes mais pas des soldats prévus, compteraient deux fois)
	protected int NoteLosses(SRP_SectorState state)
	{
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (!frontEnemy || !state || !state.m_bPosted || state.m_bRetreatDone)
			return 0;
		int lost = frontEnemy.LiveLosses(state);
		if (lost > 0)
			frontEnemy.AddLosses(state.m_sName, lost);
		return lost;
	}

	//------------------------------------------------------------------------------------------------
	// Suivi
	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s (serveur) : rattrapage du camp (une fois par minute), choix de la prochaine garnison (F11), puis pour
	//! chaque localité hostile : pose, pose différée, complément, posture de nuit, poteau, retrait, anéantissement, camions
	//! et radio. Une localité prise n'a plus rien à faire ici (capture, production et contre-attaques sont au front).
	protected void Tick()
	{
		if (!m_bBuilt)
			return;
		int now = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();

		m_iSyncCountdown--;
		if (m_iSyncCountdown <= 0)
		{
			m_iSyncCountdown = SYNC_TICKS;
			SyncOwners(false);
		}

		// Une seule garnison posée par passage : la localité hostile au meilleur score (F11 : le front d'abord, puis la
		// distance au joueur EN OPÉRATION le plus proche ; ceux restés à la base ne comptent pas), et pas plus de garnisons
		// vivantes que le maximum réglé. Sans cela, dix localités à portée recevaient leur garnison d'un coup.
		SRP_SectorState nextGarrison;
		float nextScore = 1000000;
		array<IEntity> activePlayers = players;		// hors délai de grâce : un joueur qui vient d'apparaître ne déclenche rien
		if (enemies)
		{
			activePlayers = enemies.GetActivePlayers();
			// Groupes de garnison jamais livrés (pose abandonnée) ou évincés par le jeu (limite d'IA actives) : sortis de la
			// garnison et de ses soldats prévus (le complément pourra les reposer, hors de vue)
			foreach (SRP_SectorState dropState : m_aStates)
			{
				if (dropState.m_Sector && dropState.m_iOwner == SRP_ESectorOwner.ENNEMI && !dropState.m_aGroups.IsEmpty())
					SRP_EnemyGarrison.DropAbandoned(dropState);
			}
		}
		if (enemies && enemies.HasPrefabs())
		{
			int activeGarrisons = 0;
			SRP_SectorState worstGarrison;
			float worstScore = -1000000;
			float worstReach = 0;
			foreach (SRP_SectorState candidate : m_aStates)
			{
				if (!candidate.m_Sector || candidate.m_iOwner != SRP_ESectorOwner.ENNEMI)
					continue;
				vector candidateCenter = candidate.m_Sector.GetCenter();
				if (candidate.m_bPosted)
				{
					// Une garnison dont la file d'apparition de la 1.8 livre encore les soldats compte déjà ; une garnison
					// posée vide (F12) ne prend pas de place
					if (!candidate.m_aGroups.IsEmpty() && (SRP_EnemyComponent.AliveAgentsIn(candidate.m_aGroups) > 0 || !SRP_EnemyComponent.IsSettled(candidate.m_aGroups, now)))
					{
						activeGarrisons++;
						float reachGarrison = SRP_Utils.NearestPlayerDistance(candidateCenter, players);
						float scoreGarrison = ScoreOf(frontEnemy, candidate, reachGarrison);
						if (scoreGarrison > worstScore)
						{
							worstScore = scoreGarrison;
							worstReach = reachGarrison;
							worstGarrison = candidate;
						}
					}
					continue;
				}
				// Pose différée il y a moins de 30 s (joueur dans la localité et aucun point hors de vue, ou pas de place) :
				// elle cède son tour, les autres localités reçoivent leur garnison pendant ce temps
				if (candidate.m_iDeferRetryTick > now)
					continue;
				// Jamais de garnison dans une localité prise (lu à l'instant sur le front) ni vidée par un décrochage (MA7)
				if (!IsHostileNow(candidate) || IsEmptied(candidate))
					continue;
				float candidateRange = SpawnRangeFor(frontEnemy, candidate);
				float reach = SRP_Utils.NearestPlayerDistance(candidateCenter, activePlayers);
				if (reach > candidateRange)
					continue;
				if (enemies.CountPlayersNear(candidateCenter, candidateRange) <= 0)
					continue;
				float score = ScoreOf(frontEnemy, candidate, reach);
				if (score >= nextScore)
					continue;
				nextScore = score;
				nextGarrison = candidate;
			}
			// Plafond de garnisons atteint, ou plus assez de place (SRP_CmdCapacity) pour la garnison décidée : priorité au
			// meilleur score
			bool atCount = activeGarrisons >= m_iGarrisonMaxActive;
			bool noRoom = false;
			if (nextGarrison && !atCount)
				noRoom = !HasGarrisonRoom(enemies, nextGarrison);
			if (nextGarrison && (atCount || noRoom))
			{
				// La garnison au plus mauvais score cède sa place si le nouveau score est meilleur d'au moins 300, que personne
				// n'est à moins de m_fGarrisonEvictDistance d'elle, qu'elle n'est ni en alerte ni attaquée, et qu'aucun de ses
				// groupes n'est vu (jamais de disparition sous les yeux d'un joueur). Faute de place seulement (moins de
				// garnisons que le maximum) et sans retrait possible : la garnison est posée réduite (le centre et les postes
				// d'abord, SRP_EnemyGarrison) et complétée plus tard.
				bool evict = worstGarrison != null && worstReach > m_fGarrisonEvictDistance && nextScore + 300 < worstScore;
				if (evict && enemies.HasAlertNear(worstGarrison.m_Sector.GetCenter(), worstGarrison.m_Sector.m_fRadius + 300))
					evict = false;
				if (evict && frontEnemy && frontEnemy.IsLocalityUnderAttack(worstGarrison.m_sName))
					evict = false;
				if (evict && IsGarrisonSeen(enemies, worstGarrison, players))
					evict = false;
				if (evict)
				{
					int evictLosses = NoteLosses(worstGarrison);
					RetireSourced(enemies, worstGarrison);	// débarqués d'un renfort : rendus à leur source (MA2)
					enemies.DeleteAll(worstGarrison.m_aGroups);
					worstGarrison.m_aGroups.Clear();
					OnGarrisonGone(worstGarrison, "plafond de garnisons");
					string why = string.Format("plafond de %1 garnisons atteint", m_iGarrisonMaxActive);
					if (!atCount)
						why = "plus de place pour les soldats ennemis";
					SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : garnison retirée hors de vue (%2, joueur le plus proche à %3 m, %4 perte(s) notée(s)) au profit de %5, au meilleur score", worstGarrison.m_sName, why, Math.Round(worstReach), evictLosses, nextGarrison.m_sName));
				}
				else if (atCount)
					nextGarrison = null;
			}
		}

		foreach (SRP_SectorState state : m_aStates)
		{
			SRP_SectorDef sector = state.m_Sector;
			if (!sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI)
				continue;	// localité prise : plus rien à faire ici
			vector center = sector.GetCenter();
			float nearest = SRP_Utils.NearestPlayerDistance(center, players);
			int alive = SRP_EnemyComponent.AliveAgentsIn(state.m_aGroups);							// agents encore dans les groupes (morts récents et inconscients compris)
			int activeInZone = SRP_EnemyComponent.ActiveAgentsNear(state.m_aGroups, center, sector.m_fRadius);	// en état de combattre, dans la zone
			float stateRange = SpawnRangeFor(frontEnemy, state);

			if (state == nextGarrison && !state.m_bPosted && nearest <= stateRange)
			{
				// Garnison à l'approche (carte #69, livraison 3 : composition et pose par SRP_EnemyGarrison), effectif F12
				PoseGarrison(enemies, state, now);
				alive = SRP_EnemyComponent.AliveAgentsIn(state.m_aGroups);
			}
			else if (alive > 0 && enemies && !state.m_aDeferredSlots.IsEmpty())
			{
				// Garnison incomplète (groupes différés : joueur dans la localité, ou pas de place) : ils sont réessayés tels
				// quels toutes les 30 s, joueurs au contact ou non (le complément ci-dessous attend que plus personne ne soit
				// à moins du rayon + 400 m) ; le centre d'abord s'il manque. Toujours sans repli sur un joueur dans la localité.
				if (now >= state.m_iDeferRetryTick && nearest <= stateRange)
				{
					int placed = SRP_EnemyGarrison.RetryDeferred(enemies, state, state.m_aGroups);
					NoteDeferral(state, state.m_aDeferredSlots.Count(), now);
					if (placed > 0)
						MarkZone(state);
					alive = SRP_EnemyComponent.AliveAgentsIn(state.m_aGroups);
				}
			}
			else if (enemies && state.m_bPosted && nearest > sector.m_fRadius + CONTACT_MARGIN && nearest <= stateRange && now >= state.m_iTopUpRetryTick && !IsEmptied(state))
			{
				// Complément quand personne n'est au contact (davantage de joueurs, place libérée, pertes regarnies)
				TopUpGarrison(enemies, state, alive, now);
				alive = SRP_EnemyComponent.AliveAgentsIn(state.m_aGroups);
			}

			// Posture de nuit : à la tombée de la nuit, sans alerte sur la localité, les rondes en trop passent en poste aux
			// entrées ; au jour, elles reprennent leur ronde (ordres seulement, le total ne change pas)
			if (m_bNightPosture && enemies && alive > 0)
			{
				bool nightNow = SRP_DayNightComponent.IsNight();
				if (nightNow != state.m_bNightPosture && !enemies.HasAlertNear(center, sector.m_fRadius + 300))
					SRP_EnemyGarrison.SwitchPosture(enemies, state, nightNow);
			}

			// Poteau de sirène voulu après la pose (au plus un essai par m_iPoleRetrySeconds)
			UpdatePole(state, now);

			// Retrait : plus aucun joueur à m_fGarrisonDespawnDistance pendant 10 min (les pertes sont notées, F12)
			if (nearest > m_fGarrisonDespawnDistance)
			{
				if (state.m_iNoPlayerSince == 0)
					state.m_iNoPlayerSince = now;
				else if (now - state.m_iNoPlayerSince > 10 * 60 * 1000 && state.m_bPosted)
				{
					NoteLosses(state);
					if (enemies)
					{
						RetireSourced(enemies, state);	// débarqués d'un renfort : rendus à leur source (MA2)
						enemies.DeleteAll(state.m_aGroups);
					}
					state.m_aGroups.Clear();
					OnGarrisonGone(state, "plus personne");
					state.m_iNoPlayerSince = 0;
				}
			}
			else
				state.m_iNoPlayerSince = 0;

			// Garnison anéantie sans capture : dès que plus aucun joueur n'est à 1 500 m, on l'oublie (pertes notées) ; une
			// nouvelle garnison, diminuée de ses pertes, sera posée à la prochaine approche. Pas de RetireSourced ici : plus
			// aucun soldat de ses groupes n'est en état de combattre (activeAnywhereGarrison compte comme FitOf), il n'y a
			// rien à rendre
			int activeAnywhereGarrison = 0;
			// Pose terminée : chaque groupe a déjà eu un soldat, ou sa grâce est passée (la file d'apparition de la 1.8
			// livre les soldats un par un)
			bool settled = !state.m_aGroups.IsEmpty() && SRP_EnemyComponent.IsSettled(state.m_aGroups, now);
			if (!state.m_aGroups.IsEmpty())
				activeAnywhereGarrison = SRP_EnemyComponent.ActiveAgentsNear(state.m_aGroups, center, 0);
			if (settled && activeAnywhereGarrison == 0 && nearest > m_fGarrisonWipeClearDistance)
			{
				int wipeLosses = NoteLosses(state);
				if (enemies)
					enemies.DeleteAll(state.m_aGroups);
				state.m_aGroups.Clear();
				OnGarrisonGone(state, "anéantie");
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : garnison anéantie et localité quittée (%2 perte(s) notée(s)), une nouvelle garnison sera posée à la prochaine approche", state.m_sName, wipeLosses));
			}

			// Camions de renfort et opérateur radio de la garnison vivante
			if (enemies && activeAnywhereGarrison > 0)
				GarrisonTrucksTick(state, activePlayers, now);

			state.m_iActiveInZone = activeInZone;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pose de la garnison d'une localité hostile (F11, F12) : effectif DÉCIDÉ (DecidedSoldiers). Sous
	//! m_iGarrisonMinSoldiers, rien n'est posé mais la garnison compte comme posée (localité vide et prenable, poste de
	//! commandement posé) ; sinon SRP_EnemyGarrison pose (place : GarrisonCap puis Ask) et une jeep peut suivre. Rien de
	//! posé du tout (place refusée, tout différé) : la localité reste candidate (nouvel essai, recomposée).
	protected void PoseGarrison(SRP_EnemyComponent enemies, SRP_SectorState state, int now)
	{
		if (!enemies || !state || !state.m_Sector)
			return;
		SRP_SectorDef sector = state.m_Sector;
		vector center = sector.GetCenter();
		int nominal = GarrisonSoldiers(enemies, sector);
		int wanted = DecidedSoldiers(enemies, state);
		int approaching = enemies.CountPlayersNear(center, m_fGarrisonSpawnDistance);
		state.m_iPlannedSoldiers = 0;
		state.m_aDeferredSlots.Clear();	// une garnison entièrement différée est recomposée
		if (wanted < Math.Max(m_iGarrisonMinSoldiers, 1))
		{
			// F12 : localité décimée, vide et prenable ; la garnison compte comme posée (poste de commandement)
			MarkPosted(state);
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : garnison décimée, rien de posé (%2 soldat(s) décidé(s) sur %3 au complet) : localité vide et prenable, regarnie d'un quart par heure", state.m_sName, wanted, nominal));
			return;
		}
		int spawned = SRP_EnemyGarrison.Spawn(enemies, state, wanted, SRP_DayNightComponent.IsNight(), false, state.m_aGroups);
		NoteDeferral(state, state.m_aDeferredSlots.Count(), now);
		if (spawned <= 0 && state.m_aGroups.IsEmpty())
			return;
		MarkPosted(state);
		string placedLine = string.Format("%1 : garnison de %2 soldat(s) en place, %3 décidés sur %4 au complet, pour %5 joueur(s) en approche", state.m_sName, spawned, wanted, nominal, approaching);
		SRP_EnemyComponent.Journal("ENNEMI", placedLine + string.Format(" (rayon %1 m, menace %2)", Math.Round(sector.m_fRadius), GetThreat()));
		// Jeep : sa place est décidée par SRP_CmdCapacity (patrouilles), plus par l'ancien plafond des garnisons
		SRP_EnemyGroup jeep = enemies.MaybeSpawnVehiclePatrol(center, "territoire");
		if (jeep)
			state.m_aGroups.Insert(jeep);
		MarkZone(state);	// sirène : ses soldats savent à quelle localité ils appartiennent
		WantPole(state);	// le poteau de sirène sera (re)posé s'il manque ou s'il est détruit
	}

	//------------------------------------------------------------------------------------------------
	//! Complément d'une garnison posée, personne au contact : sections jusqu'à l'effectif décidé (les soldats prévus
	//! comptent les morts : pas de remplacement des pertes sur place) ; garnison posée VIDE (F12, rien de prévu) : posée
	//! d'un coup dès que l'effectif décidé, regarni d'un quart par heure, atteint m_iGarrisonMinSoldiers. Un complément
	//! qui n'a rien posé (place, repli refusé) n'est pas retenté avant TOPUP_RETRY_MS.
	protected void TopUpGarrison(SRP_EnemyComponent enemies, SRP_SectorState state, int alive, int now)
	{
		int wanted = DecidedSoldiers(enemies, state);
		if (alive > 0)
		{
			int missing = wanted - state.m_iPlannedSoldiers;
			if (missing < 2)
				return;
			int added = SRP_EnemyGarrison.Spawn(enemies, state, missing, SRP_DayNightComponent.IsNight(), true, state.m_aGroups);
			NoteDeferral(state, state.m_aDeferredSlots.Count(), now);
			if (added > 0)
			{
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : garnison complétée de %2 soldat(s), %3 prévus en tout (davantage de joueurs en approche, place libérée ou pertes regarnies)", state.m_sName, added, state.m_iPlannedSoldiers));
				MarkZone(state);
			}
			else
				state.m_iTopUpRetryTick = now + TOPUP_RETRY_MS;
			return;
		}
		// Seule une garnison posée VIDE se regarnit ici ; une garnison décimée sur place ou vidée par un décrochage attend
		// son retrait (les pertes sont alors notées, F12)
		if (!state.m_aGroups.IsEmpty() || state.m_iPlannedSoldiers > 0 || !state.m_aDeferredSlots.IsEmpty())
			return;
		if (wanted < Math.Max(m_iGarrisonMinSoldiers, 1))
			return;
		int regarnished = SRP_EnemyGarrison.Spawn(enemies, state, wanted, SRP_DayNightComponent.IsNight(), false, state.m_aGroups);
		NoteDeferral(state, state.m_aDeferredSlots.Count(), now);
		if (regarnished <= 0)
		{
			state.m_iTopUpRetryTick = now + TOPUP_RETRY_MS;
			return;
		}
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : localité vide regarnie, %2 soldat(s) posés sur %3 décidés (pertes regarnies)", state.m_sName, regarnished, wanted));
		MarkZone(state);
		WantPole(state);
	}

	//------------------------------------------------------------------------------------------------
	//! La garnison compte comme posée pour cet épisode (même vide) : le poste de commandement de la localité peut être
	//! posé (SRP_ZoneFall.OnGarrisonPosted)
	protected void MarkPosted(SRP_SectorState state)
	{
		state.m_bPosted = true;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		if (zoneFall)
			zoneFall.OnGarrisonPosted(state.m_sName);
	}

	//------------------------------------------------------------------------------------------------
	//! Effectif NOMINAL d'une garnison, en SOLDATS (carte #69, livraison 3, chiffres de Jack du 25/09) : base selon le genre
	//! (hameau 10, village 20, ville 30) pour 1 à m_iPlayersIncluded joueurs proches, + m_fSoldiersPerExtraPlayer par
	//! joueur au-delà (au plus m_iPlayerBonusMax), + menace LOCALE (SRP_Commander.ThreatAt, OF5, CO9) × m_fSoldiersPerThreat
	//! arrondi au-dessus (la menace ajoute toujours au moins un soldat) ; le tout × SRP_Commander.GarrisonFactorAt (1,25
	//! si l'officier de la région vit, 1 sinon) ; plafond : le plus petit de m_iGarrisonSoldiersMax et de cap_garnison_max
	//! (60). Même nombre de jour et de nuit. Les pertes F12 sont retirées ensuite (SRP_FrontEnemyComponent.Effective) —
	//! SRP_FrontEnemyComponent.GetLocalityNominal, pose
	int GarrisonSoldiers(SRP_EnemyComponent enemies, SRP_SectorDef sector)
	{
		if (!sector)
			return 0;
		int baseSoldiers = m_iSoldiersVillage;
		if (sector.m_sKind == "hameau")
			baseSoldiers = m_iSoldiersHamlet;
		else if (sector.m_sKind == "ville")
			baseSoldiers = m_iSoldiersTown;

		int extraPlayers = 0;
		if (enemies)
			extraPlayers = enemies.CountPlayersNear(sector.GetCenter(), m_fGarrisonSpawnDistance) - m_iPlayersIncluded;
		if (extraPlayers < 0)
			extraPlayers = 0;
		int playerBonus = Math.Ceil(extraPlayers * m_fSoldiersPerExtraPlayer - 0.001);
		if (playerBonus > m_iPlayerBonusMax)
			playerBonus = m_iPlayerBonusMax;
		if (playerBonus < 0)
			playerBonus = 0;

		int localThreat = SRP_Commander.ThreatAt(sector.GetCenter(), GetThreat());
		int threatBonus = Math.Ceil(localThreat * m_fSoldiersPerThreat - 0.001);
		if (threatBonus < 0)
			threatBonus = 0;

		int wanted = baseSoldiers + playerBonus + threatBonus;
		float factor = SRP_Commander.GarrisonFactorAt(sector.GetCenter());
		if (factor > 0)
			wanted = Math.Round(wanted * factor);

		int ceiling = m_iGarrisonSoldiersMax;
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (capacity && capacity.GetGarrisonMax() > 0 && capacity.GetGarrisonMax() < ceiling)
			ceiling = capacity.GetGarrisonMax();
		if (ceiling < 2)
			ceiling = 2;
		return Math.ClampInt(wanted, 2, ceiling);
	}

	//------------------------------------------------------------------------------------------------
	//! Une garnison qui disparaît (retrait, anéantie, capture) remet à zéro ses compteurs : pose différée, effectif prévu,
	//! posture, radio et camions (livraison 3)
	protected void ResetGarrisonFlags(SRP_SectorState state)
	{
		state.m_aDeferredSlots.Clear();
		state.m_iDeferRetryTick = 0;
		state.m_bDeferLogged = false;
		state.m_iPlannedSoldiers = 0;
		state.m_bNightPosture = false;
		state.m_iTopUpRetryTick = 0;
		state.m_RadioOperator = null;
		state.m_bRadioLost = false;
		state.m_iRadioBlockedUntil = 0;
		state.m_iEntryTick = 0;
		state.m_bEntryTruckDone = false;
		state.m_iAlertTruckDue = 0;
		state.m_bAlertTruckDone = false;
		state.m_bRetreatDone = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Des soldats d'un groupe de garnison de « sector » ne seront jamais livrés par la file d'apparition
	//! (SRP_EnemyComponent.CloseStalledSpawns) : ils sortent de ses soldats prévus, le complément pourra les reposer
	void ReducePlanned(string sector, int soldiers)
	{
		SRP_SectorState state = FindState(sector);
		if (!state || soldiers <= 0)
			return;
		state.m_iPlannedSoldiers -= soldiers;
		if (state.m_iPlannedSoldiers < 0)
			state.m_iPlannedSoldiers = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! La garnison d'une localité vient d'être retirée, anéantie, abandonnée ou prise (ses groupes sont déjà supprimés,
	//! convertis ou en repli) : elle n'est plus posée, compteurs remis à zéro, sirène (le poteau est retiré s'il n'est pas
	//! vu ; sauf à la prise, où il reste muet), camions rappelés, poste de commandement retiré hors de vue
	//! (SRP_ZoneFall.OnGarrisonGone). « why » : « plafond de garnisons », « plus personne », « anéantie », « capture »
	//! (localité prise), « capture abandonnée », « remise à zéro ».
	protected void OnGarrisonGone(SRP_SectorState state, string why)
	{
		bool wasPosted = state.m_bPosted;
		state.m_bPosted = false;
		ResetGarrisonFlags(state);
		if (why != "capture" && why != "remise à zéro")
			RetirePole(state);
		// Camions (livraison 3) : un camion de cette localité pas encore posé est annulé, à l'embarquement ou en route il repart
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (trucks)
			trucks.RecallSector(state.m_sName);
		if (!wasPosted)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		if (zoneFall)
			zoneFall.OnGarrisonGone(state.m_sName);
	}

	//------------------------------------------------------------------------------------------------
	//! Camions et radio (livraison 3, SRP_EnemyTrucks), à chaque passage (5 s) pour un secteur ennemi dont la garnison a
	//! au moins un soldat en état de combattre : l'opérateur radio (UpdateRadio) ; le camion d'ENTRÉE au premier passage
	//! où un joueur actif est à moins de m_fTruckEntryDistance du centre avec assez de joueurs proches (MODE ESSAI : un
	//! seul) ; le camion d'ALERTE à son échéance (posée par OnGarrisonSpotted), si l'appel radio passe (TryRadioCall :
	//! sinon il réessaie au passage suivant). Commandeur actif (SRP_FrontEnemyComponent.IsCommanderActive) : le camion
	//! d'entrée passe par SRP_CmdManeuvers.RequestEntryTruck (même frein C2, même source) et le camion d'alerte n'est plus
	//! envoyé (le camion de contact du Commandeur le remplace) ; sinon, les règles de la carte #69.
	protected void GarrisonTrucksTick(SRP_SectorState state, array<IEntity> activePlayers, int now)
	{
		UpdateRadio(state, now);
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!trucks || !enemies || !trucks.IsEnabled())
			return;
		vector center = state.m_Sector.GetCenter();
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		bool commanderActive = frontEnemy && frontEnemy.IsCommanderActive();

		if (!state.m_bEntryTruckDone && SRP_Placement.NearestPlayer(center, activePlayers) < trucks.GetEntryDistance())
		{
			int entryPlayers = trucks.CountPlayersAround(center, activePlayers);
			if (entryPlayers >= enemies.MinPlayers(trucks.GetMinPlayers()))
			{
				// Posé dans tous les cas : pas de nouvelle demande à chaque passage
				state.m_iEntryTick = now;
				state.m_bEntryTruckDone = true;
				if (commanderActive)
				{
					SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
					if (maneuvers)
						maneuvers.RequestEntryTruck(state, entryPlayers, System.GetUnixTime());
				}
				else
					trucks.Dispatch(state, entryPlayers, "entrée");
			}
		}

		if (commanderActive)
			return;
		if (!state.m_bAlertTruckDone && state.m_iAlertTruckDue > 0 && now >= state.m_iAlertTruckDue)
		{
			if (TryRadioCall(center, now))
			{
				state.m_bAlertTruckDone = true;
				trucks.Dispatch(state, trucks.CountPlayersAround(center, activePlayers), "alerte");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Radio de la garnison (carte #69, livraison 3)
	//------------------------------------------------------------------------------------------------
	//! L'opérateur radio de la garnison, à chaque passage. Désigné une fois la pose terminée : un soldat dont le prefab
	//! porte « _RTO » (le QG d'abord, puis toute la garnison) ; sinon le premier du groupe de commandement ; sinon le
	//! premier soldat en état de combattre. Un opérateur d'emprunt cède sa place à un vrai opérateur radio dès qu'il en
	//! arrive un. Mort, inconscient, supprimé ou sorti de son groupe (reddition) : la radio est perdue (ligne ENNEMI, aucun
	//! message aux joueurs) jusqu'à ce que le retard du premier appel qui suit soit écoulé (TryRadioCall).
	protected void UpdateRadio(SRP_SectorState state, int now)
	{
		if (state.m_bRadioLost)
			return;
		IEntity radioMan = state.m_RadioOperator;
		if (radioMan)
		{
			if (!IsFitSoldier(radioMan) || !IsInGarrison(state, radioMan))
			{
				state.m_RadioOperator = null;
				state.m_bRadioLost = true;
				state.m_iRadioBlockedUntil = 0;
				SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : opérateur radio hors de combat (le prochain appel du camion d'alerte ou de l'hélicoptère sera retardé)");
				return;
			}
			if (IsRadioMan(radioMan))
				return;	// un vrai opérateur radio : rien à changer
		}
		if (!SRP_EnemyComponent.IsSettled(state.m_aGroups, now))
			return;	// pose en cours : la file d'apparition livre les soldats un par un

		IEntity rto = FindGarrisonSoldier(state, true, true);
		if (!rto)
			rto = FindGarrisonSoldier(state, false, true);
		if (rto)
		{
			if (rto != radioMan)
			{
				state.m_RadioOperator = rto;
				SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : opérateur radio désigné (" + SoldierPrefabName(rto) + ")");
			}
			return;
		}
		if (radioMan)
			return;	// l'opérateur d'emprunt reste en place
		IEntity standIn = FindGarrisonSoldier(state, true, false);
		if (!standIn)
			standIn = FindGarrisonSoldier(state, false, false);
		if (!standIn)
			return;
		state.m_RadioOperator = standIn;
		SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : pas d'opérateur radio dans la garnison, la radio est tenue par " + SoldierPrefabName(standIn));
	}

	//------------------------------------------------------------------------------------------------
	//! Un appel radio (camion d'alerte, hélicoptère) depuis ce lieu : vrai s'il part. Le secteur ennemi garni qui contient
	//! le lieu (rayon + 300 m) ; sans secteur, ou radio en service, l'appel part. Radio perdue : le PREMIER appel qui suit
	//! fixe le retard (m_iRadioDelayMinutes, ligne ENNEMI) et échoue ; les appels échouent jusqu'à l'échéance ; ensuite
	//! la radio est reprise (un nouvel opérateur sera désigné) et l'appel part. Le camion d'alerte et l'hélicoptère
	//! partagent ce retard (OF3 : cmd_radio_retard_min du Commandeur, m_iRadioDelayMinutes sans lui). Le délai interne
	//! reste en GetTickCount (jamais sauvegardé). Aucun message aux joueurs.
	bool TryRadioCall(vector position, int now)
	{
		SRP_SectorState state = GarrisonAt(position);
		if (!state || !state.m_bRadioLost)
			return true;
		if (state.m_iRadioBlockedUntil == 0)
		{
			int minutes = Math.ClampInt(SRP_Commander.RadioDelayMinutes(m_iRadioDelayMinutes), 0, 120);
			state.m_iRadioBlockedUntil = now + minutes * 60000;
			if (minutes > 0)
			{
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : appel radio retardé de %2 min (opérateur radio hors de combat), rien ne part avant %3", state.m_sName, minutes, SRP_EnemyTruckComponent.ClockAt(state.m_iRadioBlockedUntil, now)));
				return false;
			}
		}
		if (now < state.m_iRadioBlockedUntil)
			return false;
		state.m_bRadioLost = false;
		state.m_iRadioBlockedUntil = 0;
		SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : radio reprise après le retard, l'appel part");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le secteur ennemi garni (garnison posée) le plus proche qui contient ce lieu (rayon + 300 m), ou null
	protected SRP_SectorState GarrisonAt(vector position)
	{
		SRP_SectorState found;
		float best = 1000000;
		foreach (SRP_SectorState state : m_aStates)
		{
			if (!state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || state.m_aGroups.IsEmpty())
				continue;
			float distance = vector.Distance(state.m_Sector.GetCenter(), position);
			if (distance > state.m_Sector.m_fRadius + 300 || distance >= best)
				continue;
			best = distance;
			found = state;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Le premier soldat en état de combattre de la garnison : dans les groupes de commandement seulement (« commandOnly »),
	//! opérateur radio seulement (« rtoOnly » : prefab portant « _RTO »), ou null
	protected static IEntity FindGarrisonSoldier(SRP_SectorState state, bool commandOnly, bool rtoOnly)
	{
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (commandOnly && !record.m_bCommand)
				continue;
			array<AIAgent> agents = {};
			record.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity soldier = agent.GetControlledEntity();
				if (!IsFitSoldier(soldier))
					continue;
				if (rtoOnly && !IsRadioMan(soldier))
					continue;
				return soldier;
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce soldat est-il encore dans un groupe de la garnison ? (un soldat qui se rend en sort)
	protected static bool IsInGarrison(SRP_SectorState state, IEntity soldier)
	{
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			array<AIAgent> agents = {};
			record.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (agent && agent.GetControlledEntity() == soldier)
					return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat en état de combattre : présent, vivant, conscient
	protected static bool IsFitSoldier(IEntity soldier)
	{
		if (!soldier || soldier.IsDeleted() || SRP_Utils.IsDead(soldier))
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(soldier.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.IsUnconscious())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un opérateur radio : son prefab porte « _RTO » (RHS : RTO du QG, Scout_RTO de l'équipe de reconnaissance)
	protected static bool IsRadioMan(IEntity soldier)
	{
		string name = SoldierPrefabName(soldier);
		return name.Contains("_RTO");
	}

	//------------------------------------------------------------------------------------------------
	//! Nom court du prefab d'un soldat (« Character_RHS_RF_MSV_VKPO_DS_RTO_Random »), "" sans prefab
	protected static string SoldierPrefabName(IEntity soldier)
	{
		if (!soldier)
			return "";
		EntityPrefabData prefabData = soldier.GetPrefabData();
		if (!prefabData)
			return "";
		return SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
	}

	//------------------------------------------------------------------------------------------------
	//! Staff (page « État des groupes ») : la radio et les camions des garnisons ennemies à moins de « radius » : opérateur
	//! en place (vrai opérateur ou d'emprunt), hors de combat (retard à venir, ou appels bloqués jusqu'à hh:mm:ss) ; camion
	//! d'entrée envoyé ou non ; camion d'alerte prévu, envoyé ou non demandé. Jamais montré aux joueurs.
	string GetRadioReport(vector from, float radius)
	{
		int now = System.GetTickCount();
		string text = "";
		foreach (SRP_SectorState state : m_aStates)
		{
			if (!state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || state.m_aGroups.IsEmpty())
				continue;
			float distance = vector.Distance(from, state.m_Sector.GetCenter());
			if (distance > radius)
				continue;
			string radio = "pas encore d'opérateur (pose en cours)";
			if (state.m_bRadioLost && state.m_iRadioBlockedUntil == 0)
				radio = string.Format("opérateur hors de combat, le prochain appel sera retardé de %1 min", Math.ClampInt(SRP_Commander.RadioDelayMinutes(m_iRadioDelayMinutes), 0, 120));
			else if (state.m_bRadioLost)
				radio = "opérateur hors de combat, appels retardés jusqu'à " + SRP_EnemyTruckComponent.ClockAt(state.m_iRadioBlockedUntil, now);
			else if (state.m_RadioOperator && IsRadioMan(state.m_RadioOperator))
				radio = "opérateur radio en place";
			else if (state.m_RadioOperator)
				radio = "radio tenue par un soldat d'emprunt (pas d'opérateur radio)";
			string entry = "camion d'entrée : pas encore";
			if (state.m_bEntryTruckDone)
				entry = "camion d'entrée : envoyé";
			string alertTruck = "camion d'alerte : non demandé";
			if (state.m_bAlertTruckDone)
				alertTruck = "camion d'alerte : envoyé";
			else if (state.m_iAlertTruckDue > 0)
				alertTruck = "camion d'alerte : appel prévu à " + SRP_EnemyTruckComponent.ClockAt(state.m_iAlertTruckDue, now);
			if (!text.IsEmpty())
				text += "\n";
			text += string.Format("Radio de %1 : %2 · %3 · %4 · %5 m", state.m_sName, radio, entry, alertTruck, Math.Round(distance));
		}
		if (text.IsEmpty())
			return string.Format("Radio : aucune garnison ennemie à moins de %1 m", Math.Round(radius));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Reste-t-il la place de poser la garnison décidée de cette localité (SRP_CmdCapacity, seule porte de la place :
	//! soldats de sa classe, GARNISON ou COMBAT si elle est attaquée, et au moins trois éléments) ? Rien à poser (F12,
	//! localité vide) : oui. Lecture seule, sans demande notée (la pose, elle, passe par Ask dans SRP_EnemyGarrison).
	protected bool HasGarrisonRoom(SRP_EnemyComponent enemies, SRP_SectorState state)
	{
		if (!enemies || !state || !state.m_Sector)
			return true;
		int wanted = DecidedSoldiers(enemies, state);
		if (wanted < Math.Max(m_iGarrisonMinSoldiers, 1))
			return true;
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		if (!capacity)
			return true;
		int soldiers = capacity.GarrisonCap(state.m_sName, wanted);
		int cls = SRP_ECmdCapClass.GARNISON;
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (frontEnemy && frontEnemy.IsLocalityUnderAttack(state.m_sName))
			cls = SRP_ECmdCapClass.COMBAT;
		if (capacity.GroupRoom(cls, "territoire") < 3)
			return false;
		return soldiers <= capacity.Room(cls, "territoire");
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe de cette garnison est-il vu par un joueur ? (une garnison n'est jamais retirée sous les yeux d'un joueur)
	protected bool IsGarrisonSeen(SRP_EnemyComponent enemies, SRP_SectorState state, array<IEntity> players)
	{
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (IsGroupSeen(enemies, record, players))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce groupe (avec au moins un soldat) est-il vu par un joueur ? Un joueur à moins de GROUP_SEEN_CLOSE mètres le voit
	//! toujours (la trace vers la tête d'un soldat tout proche est arrêtée par son propre corps)
	protected static bool IsGroupSeen(SRP_EnemyComponent enemies, SRP_EnemyGroup record, array<IEntity> players)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted() || SRP_EnemyComponent.AliveAgents(record) == 0)
			return false;
		vector position = enemies.GroupPosition(record);
		if (SRP_Placement.NearestPlayer(position, players) < GROUP_SEEN_CLOSE)
			return true;
		return SRP_Placement.IsSeenByAnyPlayer(position, players);
	}

	protected static const float GROUP_SEEN_CLOSE = 150;	// un joueur à moins de … m d'un groupe le voit toujours

	//------------------------------------------------------------------------------------------------
	//! Après une pose de garnison : groupes différés, gardés dans m_aDeferredSlots (joueur dans le secteur et aucun point
	//! hors de vue, ou place refusée par SRP_CmdCapacity.Ask), prochain essai dans 30 s (RetryDeferred), une ligne de
	//! journal par épisode (jusqu'à ce que tout soit posé ou la garnison retirée)
	protected void NoteDeferral(SRP_SectorState state, int deferred, int now)
	{
		if (deferred <= 0)
		{
			state.m_iDeferRetryTick = 0;
			state.m_bDeferLogged = false;
			return;
		}
		state.m_iDeferRetryTick = now + DEFER_RETRY_MS;
		if (state.m_bDeferLogged)
			return;
		state.m_bDeferLogged = true;
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : pose de garnison différée, %2 groupe(s) en attente, sans point hors de vue (un joueur est dans le secteur) ou sans place pour les soldats ennemis ; nouvel essai toutes les %3 s, jamais de repli sur un joueur", state.m_sName, deferred, DEFER_RETRY_MS / 1000));
	}

	//------------------------------------------------------------------------------------------------
	// Sirène (carte #69, livraison 2)
	//------------------------------------------------------------------------------------------------
	//! Les groupes de la garnison savent à quelle localité ils appartiennent (un soldat qui voit un joueur demande la
	//! sirène de CETTE localité) ; un groupe déjà marqué garde sa localité
	protected void MarkZone(SRP_SectorState state)
	{
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (record && record.m_sZone.IsEmpty())
				record.m_sZone = state.m_sName;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La garnison vient d'être posée : un poteau sera (re)posé s'il manque ou s'il est détruit
	protected void WantPole(SRP_SectorState state)
	{
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		if (!sirens || !sirens.IsSirenEnabled())
			return;
		state.m_bPoleWanted = true;
		state.m_iPoleTries = 0;
		state.m_iNextPoleTry = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Poteau voulu : un poteau intact suffit ; un poteau abîmé est d'abord retiré s'il n'est pas vu et qu'aucun joueur
	//! n'est à moins de 300 m (sinon on attend) ; puis la pose, au plus un essai par m_iPoleRetrySeconds
	protected void UpdatePole(SRP_SectorState state, int now)
	{
		if (!state.m_bPoleWanted || now < state.m_iNextPoleTry)
			return;
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		if (!sirens || !sirens.IsSirenEnabled() || state.m_aGroups.IsEmpty())
		{
			state.m_bPoleWanted = false;
			return;
		}
		if (sirens.IsIntact(state.m_Siren))
		{
			state.m_bPoleWanted = false;
			return;
		}
		if (state.m_Siren && !state.m_Siren.IsDeleted())
		{
			if (!sirens.DeletePoleIfUnseen(state.m_Siren, 300))
			{
				state.m_iNextPoleTry = now + sirens.GetPoleRetryMs();
				return;
			}
		}
		state.m_Siren = sirens.SpawnPole(state.m_sName, state.m_Sector.GetCenter(), state.m_iPoleTries);
		if (state.m_Siren)
		{
			state.m_bPoleWanted = false;
			return;
		}
		state.m_iPoleTries++;
		state.m_iNextPoleTry = now + sirens.GetPoleRetryMs();
	}

	//------------------------------------------------------------------------------------------------
	//! Garnison retirée (plafond, plus personne, anéantie) : la sirène se tait, le poteau est retiré s'il n'est pas vu
	//! (sinon il reste, et resservira à la prochaine garnison)
	protected void RetirePole(SRP_SectorState state)
	{
		state.m_bPoleWanted = false;
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		if (!sirens)
			return;
		if (sirens.RemovePole(state.m_sName, state.m_Siren, false))
			state.m_Siren = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat de la garnison de « zone » a VU un joueur à « position » (SRP_EnemyComponent.AlertTick, à chaque
	//! lecture) : la sirène de la localité est demandée si le secteur est ennemi, sa garnison vivante, et le joueur dans
	//! la localité (rayon du secteur + m_fSirenZoneMargin). La sirène sonne même si l'opérateur radio est mort.
	//! Livraison 3 : le camion d'alerte se branche aussi ici (OnGarrisonSpotted).
	void OnGarrisonAlert(string zone, vector position, SCR_AIGroup source, int now)
	{
		SRP_SectorState state = FindState(zone);
		if (!state || !state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || state.m_aGroups.IsEmpty())
			return;
		vector center = state.m_Sector.GetCenter();
		if (vector.Distance(position, center) > state.m_Sector.m_fRadius + m_fSirenZoneMargin)
			return;
		if (SRP_EnemyComponent.ActiveAgentsNear(state.m_aGroups, center, 0) == 0)
			return;
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		if (sirens)
			sirens.Request(zone, state.m_Siren, source);
		OnGarrisonSpotted(state, position, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Camion d'alerte (livraison 3, SRP_EnemyTrucks) : un soldat de la garnison VIVANTE de « state » a VU un joueur dans la
	//! localité (mêmes conditions que la sirène ; appelé à chaque lecture, ce sont les drapeaux qui limitent). Dès
	//! m_iFullFromPlayers joueurs proches seulement (de 2 à 4 joueurs, le camion d'entrée suffit), une fois par
	//! garnison : l'appel est prévu m_iAlertDelaySeconds plus tard ; l'envoi se fait à l'échéance, dans
	//! GarrisonTrucksTick, si la radio passe (TryRadioCall). Commandeur actif : pas de camion d'alerte (son camion de
	//! contact le remplace, MA1), rien n'est prévu.
	protected void OnGarrisonSpotted(SRP_SectorState state, vector position, int now)
	{
		if (state.m_bAlertTruckDone || state.m_iAlertTruckDue > 0)
			return;
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (frontEnemy && frontEnemy.IsCommanderActive())
			return;
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!trucks || !enemies || !trucks.IsEnabled())
			return;
		int players = trucks.CountPlayersAround(state.m_Sector.GetCenter(), enemies.GetActivePlayers());
		if (players < trucks.GetFullFromPlayers())
			return;
		state.m_iAlertTruckDue = now + trucks.GetAlertDelayMs();
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : garnison en alerte avec %2 joueurs proches, camion d'alerte appelé par radio dans %3 s", state.m_sName, players, trucks.GetAlertDelayMs() / 1000));
	}

	//------------------------------------------------------------------------------------------------
	//! Les localités hostiles les plus proches d'un point (au plus « count », à moins de 4 000 m, « exclude » mis à part),
	//! du plus proche au plus loin : les origines des camions de renfort (public : SRP_EnemyTrucks)
	void NearestEnemySectors(vector position, int count, SRP_SectorState exclude, out array<SRP_SectorState> result)
	{
		array<SRP_SectorState> pool = {};
		foreach (SRP_SectorState state : m_aStates)
		{
			if (!state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || state == exclude)
				continue;
			if (vector.Distance(state.m_Sector.GetCenter(), position) > 4000)
				continue;
			pool.Insert(state);
		}
		while (result.Count() < count && !pool.IsEmpty())
		{
			int bestIndex = 0;
			float best = 1000000;
			foreach (int index, SRP_SectorState candidate : pool)
			{
				float distance = vector.Distance(candidate.m_Sector.GetCenter(), position);
				if (distance < best)
				{
					best = distance;
					bestIndex = index;
				}
			}
			result.Insert(pool[bestIndex]);
			pool.Remove(bestIndex);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Façade des localités (appelée par SRP_FrontEnemyComponent ; les autres modules passent par elle)
	//------------------------------------------------------------------------------------------------
	//! La garnison de la localité est posée pour cet épisode, même vide (F12, trou 21) ou vidée par un décrochage —
	//! SRP_FrontEnemyComponent.IsGarrisonPosted (poste de commandement, Commandeur)
	bool IsGarrisonPosted(string locality)
	{
		SRP_SectorState state = FindState(locality);
		if (!state)
			return false;
		return state.m_iOwner == SRP_ESectorOwner.ENNEMI && state.m_bPosted;
	}

	//------------------------------------------------------------------------------------------------
	//! Garnison posée ET installée : chaque groupe a eu un soldat ou sa grâce est passée (SRP_EnemyComponent.IsSettled) ;
	//! une garnison posée vide est installée (localité vide et prenable) — SRP_FrontEnemyComponent.IsGarrisonSettled
	//! (saisie D4)
	bool IsGarrisonSettled(string locality)
	{
		SRP_SectorState state = FindState(locality);
		if (!state || state.m_iOwner != SRP_ESectorOwner.ENNEMI || !state.m_bPosted)
			return false;
		return SRP_EnemyComponent.IsSettled(state.m_aGroups, System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Retire les groupes de la garnison sans jamais les faire disparaître sous les yeux des joueurs (des débarqués de
	//! camion ou des soldats sortis au combat peuvent être loin) : un groupe loin du centre (rayon + 300 m) ou vu devient
	//! « survivant », retiré plus tard hors de vue (RetireLater) ; les débarqués d'un renfort encore proches et hors de
	//! vue sont rendus à leur source (RetireSourced) ; les autres sont supprimés. Rend le nombre de groupes en repli
	//! (survivants et débarqués rendus).
	protected int RemoveGarrisonGroups(SRP_EnemyComponent enemies, SRP_SectorState state, array<IEntity> players)
	{
		array<ref SRP_EnemyGroup> survivors = {};
		float keepRadius = state.m_Sector.m_fRadius + 300;
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || SRP_EnemyComponent.AliveAgents(record) == 0)
				continue;
			bool away = vector.Distance(enemies.GroupPosition(record), state.m_Sector.GetCenter()) > keepRadius;
			if (away || IsGroupSeen(enemies, record, players))
				survivors.Insert(record);
		}
		foreach (SRP_EnemyGroup survivor : survivors)
			state.m_aGroups.RemoveItem(survivor);
		// Compté avant RetireLater, qui vide la liste qu'on lui passe
		int retiring = survivors.Count();
		retiring += RetireSourced(enemies, state);
		enemies.DeleteAll(state.m_aGroups);
		if (!survivors.IsEmpty())
			enemies.RetireLater(survivors);
		state.m_aGroups.Clear();
		return retiring;
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #76 (MA2) : les débarqués d'un renfort entrés dans la garnison (m_sSourceLocality, JoinTown ou colonne
	//! jointe) et encore en état de combattre ne sont pas supprimés avec elle (DeleteAll ne rend rien) : ils sortent de
	//! la garnison et passent par SRP_EnemyComponent.RetireLater, échéance ramenée à maintenant ; RetireTick, SEULE porte
	//! de retour, les rend à leur source (ReturnSent, selon renfort_rendre_survivants) et les retire à son prochain
	//! passage (60 s), dès qu'aucun joueur ne les voit. Appelé juste avant chaque DeleteAll d'une garnison (éviction,
	//! plus personne, RemoveGarrisonGroups) ; jamais à la remise à zéro (les envoyés y sont remis à zéro). Rend le nombre
	//! de groupes ainsi rendus.
	protected int RetireSourced(SRP_EnemyComponent enemies, SRP_SectorState state)
	{
		if (!enemies || !state)
			return 0;
		array<ref SRP_EnemyGroup> sourced = {};
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (record && !record.m_sSourceLocality.IsEmpty() && SRP_EnemyComponent.FitOf(record) > 0)
				sourced.Insert(record);
		}
		if (sourced.IsEmpty())
			return 0;
		array<ref SRP_EnemyGroup> handed = {};
		foreach (SRP_EnemyGroup back : sourced)
		{
			state.m_aGroups.RemoveItem(back);
			handed.Insert(back);
		}
		// Échéance à maintenant (dueNow) : ils devaient disparaître avec la garnison, pas rester 15 min en survivants
		enemies.RetireLater(handed, true);	// vide « handed » ; « sourced » garde les groupes
		return sourced.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Localité prise (tous ses points clés saisis) : ses défenseurs se retirent (RemoveGarrisonGroups), la garnison
	//! n'est plus posée, les camions sont rappelés, la sirène se tait (le poteau reste en place, muet, il resservira si
	//! l'ennemi reprend la localité). Les pertes ne sont pas notées : elles repartent de zéro à la prise (ClearLosses de
	//! la façade). « why » : la raison, pour le journal — SRP_FrontEnemyComponent.OnLocalityTaken (après
	//! SetLocalityOwner NOUS), rattrapage du camp
	void WithdrawLocality(string locality, string why)
	{
		SRP_SectorState state = FindState(locality);
		if (!state || !state.m_Sector)
			return;
		int groups = state.m_aGroups.Count();
		int survivors = 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
			survivors = RemoveGarrisonGroups(enemies, state, SRP_Utils.GetPlayerCharacters());
		state.m_aGroups.Clear();
		state.m_iNoPlayerSince = 0;
		OnGarrisonGone(state, "capture");
		state.m_bPoleWanted = false;
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		if (sirens)
			sirens.Silence(state.m_sName);
		if (groups > 0)
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : défenseurs retirés (%2), %3 groupe(s) dont %4 en repli hors de vue", state.m_sName, why, groups, survivors));
	}

	//------------------------------------------------------------------------------------------------
	//! Zone repassée rouge (SRP_FrontEnemyComponent.EndAttack PERDUE, seule porte) : les assaillants encore là deviennent
	//! la garnison de la localité : point Defend, rôle CRX de garnison et maintien dans son rayon (ConvertToGarrison),
	//! plus groupes d'assaut (m_iAssaultZone = -1), classe GARNISON de la place ; soldats prévus = leurs soldats (le
	//! complément n'ajoute que ce qui manque), posture du moment, sirène. Aucun assaillant : la garnison sera posée à la
	//! prochaine approche — SRP_FrontEnemyComponent.AdoptAssailants
	void AdoptAssailants(SRP_SectorState state, array<ref SRP_EnemyGroup> groups)
	{
		if (!state || !state.m_Sector || !groups)
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		int now = System.GetTickCount();
		vector center = state.m_Sector.GetCenter();
		int converted = 0;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			// Un groupe que la file d'apparition de la 1.8 n'a pas encore servi passe aussi en garnison
			if (SRP_EnemyComponent.AliveAgents(record) == 0 && !SRP_EnemyComponent.InGraceOf(record, now))
				continue;
			record.m_iAssaultZone = -1;
			enemies.ConvertToGarrison(record, center, state.m_Sector.m_fRadius);
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.GARNISON);
			if (!state.m_aGroups.Contains(record))
				state.m_aGroups.Insert(record);
			converted++;
		}
		if (converted == 0)
			return;
		ResetGarrisonFlags(state);
		state.m_iOwner = SRP_ESectorOwner.ENNEMI;
		state.m_iOwnerMismatch = 0;
		state.m_iNoPlayerSince = 0;
		state.m_bNightPosture = SRP_DayNightComponent.IsNight();
		state.m_iPlannedSoldiers = SRP_EnemyComponent.SoldiersIn(state.m_aGroups, now);
		MarkPosted(state);
		MarkZone(state);
		WantPole(state);
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : %2 groupe(s) d'assaut passés en garnison, %3 soldat(s) prévus", state.m_sName, converted, state.m_iPlannedSoldiers));
	}

	//------------------------------------------------------------------------------------------------
	//! Position dans une localité hostile dont la garnison est posée (rayon + margin) : la garnison y fait déjà la garde
	//! (remplace IsInsideEnemySector) — SRP_FrontEnemyComponent.IsInGarrisonedLocality (missions G11, Commandeur)
	bool IsInGarrisonedLocality(vector position, float margin)
	{
		foreach (SRP_SectorState state : m_aStates)
		{
			if (!state.m_Sector || state.m_iOwner != SRP_ESectorOwner.ENNEMI || !state.m_bPosted)
				continue;
			if (vector.Distance(state.m_Sector.GetCenter(), position) <= state.m_Sector.m_fRadius + margin)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Camp d'une localité (SRP_ESectorOwner) posé par la façade : NOUS quand tous ses points clés sont saisis
	//! (OnLocalityTaken, suivi de WithdrawLocality), ENNEMI quand sa zone repasse rouge (OnZoneLost, avant
	//! AdoptAssailants) ; redevenue hostile sans assaillants, sa garnison sera posée à la prochaine approche —
	//! SRP_FrontEnemyComponent
	void SetLocalityOwner(string locality, int owner)
	{
		if (owner != SRP_ESectorOwner.ENNEMI && owner != SRP_ESectorOwner.NOUS)
			return;
		SRP_SectorState state = FindState(locality);
		if (!state)
			return;
		state.m_iOwnerMismatch = 0;
		if (state.m_iOwner == owner)
			return;
		state.m_iOwner = owner;
		state.m_iNoPlayerSince = 0;
		if (owner == SRP_ESectorOwner.NOUS)
			SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : localité prise, plus de garnison");
		else
			SRP_EnemyComponent.Journal("ENNEMI", state.m_sName + " : localité de nouveau hostile (zone reprise par l'ennemi)");
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes de garnison posés (au moins un soldat) dans les localités de la zone (Staff) —
	//! SRP_FrontEnemyComponent.GetZoneGarrisonGroups
	int CountGarrisonGroupsInZone(int zone)
	{
		int count = 0;
		foreach (SRP_SectorState state : m_aStates)
		{
			if (!state.m_Sector || state.m_Sector.m_iZone != zone || !state.m_bPosted)
				continue;
			foreach (SRP_EnemyGroup record : state.m_aGroups)
			{
				if (SRP_EnemyComponent.AliveAgents(record) > 0)
					count++;
			}
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	// Événements extérieurs
	//------------------------------------------------------------------------------------------------
	//! Niveau de menace : ne fait plus que relayer SRP_FrontComponent.AddThreat (bornée 0-10, sauvegarde immédiate,
	//! journal, ressources du Commandeur)
	void AddThreat(int delta, string reason)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.AddThreat(delta, reason);
		else
			Print("[SRP] Territoire : front absent, menace inchangée (" + reason + ")", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Une mission réussie fait monter la menace ; un poste d'observation neutralisé, un officier éliminé ou une pièce
	//! sabotée la font BAISSER (l'ennemi est affaibli). La capture d'une zone (LIBERER) est comptée par la chute de la
	//! zone seule (G9 : jamais deux fois)
	void OnMissionSuccess(int type = -1)
	{
		if (type == SRP_EMissionType.LIBERER)
			return;
		if (type == SRP_EMissionType.POSTE_OBS || type == SRP_EMissionType.OFFICIER || type == SRP_EMissionType.SABOTAGE)
			AddThreat(-1, "mission réussie, " + SRP_MissionManagerComponent.TypeName(type));
		else
			AddThreat(1, "mission réussie");
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne (SRP_FrontEnemyComponent.ResetAll, après la remise à plat du front) ou remise à zéro de l'IA :
	//! garnisons supprimées, poteaux de sirène retirés, camions rappelés ; puis le camp de chaque localité est relu sur le
	//! front (tout est hostile après une nouvelle campagne). La menace, les zones et les pertes F12 sont au front.
	void ResetAll(string reason)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		foreach (SRP_SectorState state : m_aStates)
		{
			if (enemies)
				enemies.DeleteAll(state.m_aGroups);
			state.m_aGroups.Clear();
			if (sirens)
				sirens.RemovePole(state.m_sName, state.m_Siren, true);
			state.m_Siren = null;
			state.m_bPoleWanted = false;
			OnGarrisonGone(state, "remise à zéro");
			state.m_iNoPlayerSince = 0;
			state.m_iOwnerMismatch = 0;
		}
		SyncOwners(true);
		SRP_EnemyComponent.Journal("ENNEMI", "Localités remises à zéro (" + reason + ") : garnisons, sirènes et camions retirés, camp relu sur le front");
	}

	//------------------------------------------------------------------------------------------------
	//! Une mission de capture activée au tableau a échoué (zone abandonnée) : les garnisons des localités hostiles de la
	//! zone se reconstituent (pertes notées F12, défenseurs retirés hors de vue, nouvelle pose à la prochaine approche),
	//! la menace monte de 1 (SRP_FrontComponent.AddThreat), puis SRP_FrontRadio.CaptureAbandoned, SEUL texte (sans mot
	//! confidentiel). « zoneCode » : le code de la zone (« S07 » ; nom ou libellé acceptés) — SRP_MissionManagerComponent
	void OnCaptureFailed(string zoneCode)
	{
		int zone = -1;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			zone = front.FindZone(zoneCode);
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		foreach (SRP_SectorState state : m_aStates)
		{
			if (zone < 0 || !state.m_Sector || state.m_Sector.m_iZone != zone || state.m_iOwner != SRP_ESectorOwner.ENNEMI)
				continue;
			if (!state.m_bPosted && state.m_aGroups.IsEmpty())
				continue;
			NoteLosses(state);
			if (enemies)
				RemoveGarrisonGroups(enemies, state, players);
			state.m_aGroups.Clear();
			OnGarrisonGone(state, "capture abandonnée");	// sirène : elle se tait ; poteau retiré seulement loin des joueurs et hors de vue
		}
		string label = zoneCode;
		if (front && zone >= 0)
			label = front.GetZoneLabel(zone);
		AddThreat(1, "capture de " + label + " abandonnée");
		SRP_FrontRadio.CaptureAbandoned(zone, GetThreat());
	}
}
