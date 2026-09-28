//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : LE CERVEAU (contrats figés du 26/09, corps codés).
//
// RÔLE (serveur seulement ; classe simple tenue par SRP_FrontEnemyComponent.m_Commander, AUCUN composant neuf) :
// - l'état-major ennemi hors carte (CO3) : comptes rendus des unités (OF3), connaissance floutée des joueurs (CO5,
//   CO6), informateurs civils (C4), réflexes des officiers (OF4, MA1), cycles rapide et lent (CO4), humeur (CO7),
//   gravité MA9 (SEULE formule), choix de TOUTES les cibles d'appui (mortier, artillerie AP2, bombe AP3, blindé AP5,
//   hélico AP7, enquête) et de la taille du renfort ;
// - gel VU6 (IsFrozen, SetFrozen), SEULE entrée du Staff (ForceOrder, VU5), CancelOperations, OnMissionSucceeded,
//   OnSabotageUsed, sauvegarde chaînée (WriteTo/ReadFrom, clés cmd_*), ResetCampaign, OnRestored ;
// - il tient les sous-modules : m_Book (SRP_CmdRegions.c), m_Resources (SRP_CmdResources.c, qui tient les dépôts),
//   m_Maneuvers (SRP_CmdManeuvers.c), m_Support (SRP_CmdSupport.c, qui tient les blindés). La capacité
//   (SRP_CmdCapacity.c) est un singleton à part : elle vaut même sans Commandeur (règle Q7 du front).
// Il ne lit JAMAIS la vraie position d'un joueur pour décider (CO5) ; seule exception : GetDosingPlayers (EQ4).
// Pas de répartiteur : les ordres appellent directement SRP_CmdManeuvers.Request* (RENFORT, COLONNE, DECROCHAGE,
// HARCELEMENT, ENQUETE) et SRP_CmdSupport.Request* (MORTIER, FUMEE, FEINTE, ARTILLERIE, BOMBE, AVION, BLINDE, HELICO).
// Horloge : tout part de SRP_FrontEnemyComponent.Tick (5 s) -> Tick(nowUnix), heure Unix (System.GetUnixTime) ;
// CallLater ne sert qu'à enchaîner des coups de feu (SRP_CmdSupport). Aucun ScriptInvoker.
// TOUTES les énumérations SRP_ECmd* du Commandeur sont déclarées ICI et nulle part ailleurs (trou 3).
// APPELÉ PAR : SRP_FrontEnemyComponent (vie, relais neutres, sauvegarde, OnZoneReset), SRP_Enemy (NoteSighting, NoteShot,
// NoteAirSighting), SRP_Territory (ThreatAt, GarrisonFactorAt), SRP_Missions (officier OF10), SRP_CmdScreens (Staff).
//------------------------------------------------------------------------------------------------

//! Caractère d'un officier (CO7) ; METHODIQUE = caractère neutre, sans effet
enum SRP_ECmdTrait
{
	PRUDENT = 0,
	AUDACIEUX = 1,
	LENT = 2,
	METHODIQUE = 3
};

//! Humeur d'une région (CO7), recalculée à chaque cycle rapide
enum SRP_ECmdMood
{
	PRUDENTE = 0,
	NORMALE = 1,
	AGRESSIVE = 2
};

//! État d'un officier de région (OF2 à OF11)
enum SRP_ECmdOfficerState
{
	VIVANT = 0,
	TUE = 1,
	CAPTURE = 2,
	DISPARU = 3		// région libérée (OF11) : plus jamais remplacé
};

//! État d'une région (OF6, OF7, OF11)
enum SRP_ECmdRegionState
{
	ACTIVE = 0,
	DESORGANISEE = 1,
	LIBEREE = 2
};

//! Nature d'un compte rendu en route vers le Commandeur (OF3)
enum SRP_ECmdReport
{
	CONTACT = 0,
	PERTE = 1,
	CARRE_PERDU = 2,
	CARRE_REPRIS = 3,
	INFORMATEUR = 4
};

//! Ordre du Commandeur (fusion tranchée, trou 3) : manœuvres (RENFORT, ENQUETE, HARCELEMENT, COLONNE, DECROCHAGE)
//! ou appuis (MORTIER, FUMEE, FEINTE, ARTILLERIE, BOMBE, AVION, BLINDE, HELICO)
enum SRP_ECmdOrder
{
	RENFORT = 0,
	MORTIER = 1,
	FUMEE = 2,
	FEINTE = 3,
	ARTILLERIE = 4,
	BOMBE = 5,
	AVION = 6,
	BLINDE = 7,
	HELICO = 8,
	ENQUETE = 9,
	HARCELEMENT = 10,
	COLONNE = 11,
	DECROCHAGE = 12
};

//! Suivi de la mission OFFICIER (OF10) pour SRP_Missions
enum SRP_ECmdMissionStatus
{
	EN_COURS = 0,
	TOMBE = 1,
	CAPTURE = 2,
	ECHAPPE = 3,
	SANS_OBJET = 4
};

//! Les SEULS stocks du Commandeur (arbitrage C1 : l'infanterie et les camions sont gratuits)
enum SRP_ECmdStock
{
	OBUS = 0,		// par région (RE2)
	BLINDE = 1,		// île (moyens du Commandeur)
	HELICO = 2,		// île, sorties de l'hélico de recherche (AP7)
	ARTILLERIE = 3,	// île, tirs d'artillerie lourde (AP1)
	BOMBE = 4		// île (AP3)
};

//! Classe de place d'un groupe ennemi (Q7, RE11, RE12) ; rangée dans SRP_EnemyGroup.m_iCapClass
enum SRP_ECmdCapClass
{
	COMBAT = 0,		// renforts, camions, vagues (contre-attaque, mission), complément d'une localité attaquée, servants, équipages
	GARNISON = 1,	// défense d'une localité au calme, débarqués qui la rejoignent
	POSTE = 2,		// postes du front (F7 à F9)
	GARDE = 3,		// gardes de mission, garnison de livraison, officier de région et ses gardes, gardes du dépôt, garde de batterie
	PATROUILLE = 4,	// patrouilles de fond, rondes, jeeps, infiltrations, raids, survivants en retrait
	HORS_COMPTE = 5	// équipage de l'hélico en vol : hors des 120, dans la marge des 160
};

//! Obus de mortier (MO7, C5 : la fumée est gratuite)
enum SRP_ECmdShell
{
	EXPLOSIF = 0,
	FUMEE = 1
};

//! Raison d'un blindé (AP5)
enum SRP_ECmdArmorReason
{
	CONTRE_ATTAQUE = 0,
	RENFORT = 1,
	RIPOSTE = 2,
	STAFF = 3
};

//! Cycle de vie d'un blindé (AP4 à AP6, C3)
enum SRP_ECmdArmorState
{
	ROUTE = 0,
	ENGAGE = 1,
	REPLI = 2,
	PRENABLE = 3,	// équipage hors de combat : les joueurs peuvent monter
	PRIS = 4,		// un joueur à bord
	LIVRE = 5,		// ramené à la base, prime versée (C3)
	DETRUIT = 6
};

//! État d'une pièce de mortier ou d'une batterie d'une région (Staff)
enum SRP_ECmdPieceState
{
	PRETE = 0,		// rien de posé, peut tirer
	EN_BATTERIE = 1,	// posée
	EN_TIR = 2,		// mission de tir en cours
	MUETTE = 3		// réduite au silence ou détruite : 24 h sans (MO9, AP1)
};

//! Mode d'un assaut (MA8)
enum SRP_ECmdAssaultMode
{
	FACE = 0,
	FLANC = 1,
	DEUX_AXES = 2
};

//! Rôle donné par le Commandeur à un groupe (SRP_EnemyGroup.m_iCmdRole). Tout rôle différent d'AUCUN exclut le groupe
//! de l'entraide (SRP_EnemyAwareness.IsFree) : officier et ses gardes, servants, équipages, replis, assauts, raids.
enum SRP_ECmdRole
{
	AUCUN = 0,
	ASSAUT = 1,
	BASE_FEU = 2,
	MANOEUVRE = 3,
	COUVERTURE = 4,
	REPLI = 5,
	RENFORT = 6,
	RAID = 7,
	OFFICIER = 8,
	GARDE_OFFICIER = 9,
	SERVANT = 10,
	EQUIPAGE = 11,
	GARDE_DEPOT = 12
};

//! État d'une colonne (MA4)
enum SRP_ECmdColumnState
{
	PAPIER = 0,
	REELLE = 1,
	ARRIVEE = 2,
	PERDUE = 3
};

//! Nature d'une colonne (MA4, MA6, VU5)
enum SRP_ECmdColumnKind
{
	REPLI = 0,
	RENFORT = 1,
	STAFF = 2
};

//! Nature d'une ligne du journal des décisions (VU4) ; teintes Staff : APPUI r, DECISION y, REFUS d, STAFF g, autres w
enum SRP_ECmdLogKind
{
	DECISION = 0,
	APPUI = 1,
	REFUS = 2,
	OFFICIER = 3,
	STOCK = 4,
	RENSEIGNEMENT = 5,
	STAFF = 6,
	SYSTEME = 7
};

//! Source d'un renseignement des joueurs (VU2, RE9) ; aussi le paramètre source de SRP_FrontRadio.IntelGained
enum SRP_ECmdIntelSource
{
	AUCUNE = 0,
	PC = 1,			// poste de commandement saisi (D4)
	OFFICIER = 2,	// officier capturé (OF9)
	MISSION = 3		// mission réussie sur un carré rouge
};

//------------------------------------------------------------------------------------------------
//! Compteur glissant d'une heure, une case par minute Unix (tirs, pertes, carrés perdus et repris d'une zone)
class SRP_CmdRing
{
	ref array<int> m_aCounts = {};		// 60 cases
	int m_iMinute;						// minute Unix (nowUnix / 60) de la case 0

	//------------------------------------------------------------------------------------------------
	//! Décale jusqu'à la minute nowUnix / 60 (cases dépassées remises à 0), puis ajoute amount — SRP_Commander.Integrate
	void Add(int nowUnix, int amount)
	{
		Shift(nowUnix);
		m_aCounts[0] = m_aCounts[0] + amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Somme des minutes récentes (1 à 60) — gravité, humeur, IsHeavySupportJustified, GetZoneShots/Losses
	int Sum(int nowUnix, int minutes)
	{
		Shift(nowUnix);
		int count = Math.ClampInt(minutes, 1, 60);
		int total = 0;
		for (int i = 0; i < count; i++)
		{
			total = total + m_aCounts[i];
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Tout à 0 — remise à plat, restauration
	void Clear()
	{
		m_aCounts.Clear();
		for (int i = 0; i < 60; i++)
		{
			m_aCounts.Insert(0);
		}
		m_iMinute = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Case 0 = la minute en cours, case k = il y a k minutes : on glisse vers le passé jusqu'à la minute nowUnix / 60
	//! (une horloge qui recule ne décale rien) — Add, Sum
	protected void Shift(int nowUnix)
	{
		if (m_aCounts.Count() != 60)
			Clear();
		int minute = nowUnix / 60;
		int gap = minute - m_iMinute;
		if (gap <= 0)
			return;
		if (gap >= 60)
		{
			for (int i = 0; i < 60; i++)
			{
				m_aCounts[i] = 0;
			}
		}
		else
		{
			for (int k = 59; k >= gap; k--)
			{
				m_aCounts[k] = m_aCounts[k - gap];
			}
			for (int j = 0; j < gap; j++)
			{
				m_aCounts[j] = 0;
			}
		}
		m_iMinute = minute;
	}
}

//------------------------------------------------------------------------------------------------
//! Un compte rendu en route vers le Commandeur (OF3). Jamais sauvegardé (H3).
class SRP_CmdReport
{
	int m_iKind = SRP_ECmdReport.CONTACT;
	int m_iRegion = -1;
	int m_iZone = -1;
	int m_iCell = -1;
	vector m_vPos;						// déjà floutée pour un contact, exacte pour une perte ou un carré
	int m_iPlayers;
	bool m_bSeen;
	bool m_bVehicle;
	bool m_bArmed;
	int m_iShots;
	int m_iDueUnix;						// livraison au plus tôt
	string m_sSource;					// « poste », « Chotain », « unité mobile », « camion », « hélico », « civil », « assaut »
	bool m_bNeedsRadio;					// unité d'une localité : passe par la radio de la localité (TryRadioCall)
	vector m_vRadioPos;
	bool m_bRadioLogged;				// retard déjà écrit au journal
	bool m_bDirect;						// hélico et vagues : parlent droit au Commandeur, même sans officier
	IEntity m_Informant;				// le civil (C4), pour annuler s'il est mort, inconscient ou menotté
}

//------------------------------------------------------------------------------------------------
//! Ce que le Commandeur sait d'un groupe de joueurs (CO5, version du cerveau + m_iRegion, trou 3). Jamais sauvegardé.
class SRP_CmdContact
{
	vector m_vPos;						// position connue (floutée)
	vector m_vFirstPos;					// gardée tant que la position reste à 100 m (joueurs immobiles, AP2)
	int m_iZone = -1;
	int m_iCell = -1;
	int m_iRegion = -1;
	int m_iPlayers;						// joueurs ESTIMÉS
	bool m_bSeen;						// vu (sinon seulement entendu)
	bool m_bInformantOnly;				// ne vient que d'informateurs : fouille seulement, jamais d'appui (C4)
	bool m_bArmed;						// véhicule armé (AP5)
	bool m_bVehicle;
	int m_iFirstUnix;
	int m_iLastUnix;					// dernier compte rendu
	int m_iSeenUnix;					// dernière fois VU (0 = jamais)
	int m_iReports;
	bool m_bInvestigated;				// ENQUETE déjà donnée
}

//------------------------------------------------------------------------------------------------
//! Mémoire du Commandeur par zone (index = numéro de zone du socle). Jamais sauvegardée (H3).
class SRP_CmdZoneIntel
{
	ref SRP_CmdRing m_Shots = new SRP_CmdRing();
	ref SRP_CmdRing m_Losses = new SRP_CmdRing();
	ref SRP_CmdRing m_CellsLost = new SRP_CmdRing();
	ref SRP_CmdRing m_CellsRetaken = new SRP_CmdRing();
	int m_iLastReportUnix;
	int m_iLastContactUnix;
	int m_iLastSeenUnix;
	int m_iLastInformantUnix;			// C4 : un signalement par zone toutes les info_zone_repos_s
	int m_iLastMortarUnix;				// cmd_mortier_repos_zone_s
	int m_iArmedSeenUnix;				// AP5 riposte
	vector m_vArmedPos;
	float m_fGravity;					// MA9, recalculée au cycle rapide
}

//------------------------------------------------------------------------------------------------
//! Un ordre du Commandeur, exécuté aussitôt ou à m_iNotBefore (officier lent). Jamais sauvegardé.
class SRP_CmdOrder
{
	int m_iKind = SRP_ECmdOrder.RENFORT;
	int m_iVariant = -1;				// BLINDE : type (0 BTR-70, 1 BRDM-2, 2 Typhoon, -1 tirage) ; MORTIER, FUMEE : obus (0 = défaut)
	int m_iRegion = -1;
	int m_iZone = -1;
	vector m_vAim;
	int m_iSize;						// RENFORT : soldats voulus (0 = au choix des manœuvres)
	float m_fDelayFactor = 1;			// réflexe local d'une région désorganisée : desorg (appliqué par les manœuvres)
	int m_iNotBefore;					// heure Unix (officier lent : + cmd_trait_lent_retard_s)
	string m_sReason;
	string m_sAuthor;					// vide = le Commandeur
	bool m_bStaff;						// VU5 : sans seuil, plafond ni délai ; jamais sans la sécurité
	int m_iArmorReason = SRP_ECmdArmorReason.RENFORT;	// BLINDE : raison AP5 (STAFF pour une décision forcée)
}

//------------------------------------------------------------------------------------------------
//! LE CERVEAU. Serveur seulement. Point d'accès unique : SRP_Commander.Get() (null si absent : les accroches
//! statiques ne font alors rien). Managed : le pointeur statique retombe à null quand l'hôte le libère.
class SRP_Commander : Managed
{
	// --- Réglages (clés cmd_*, info_*, alerte_renfort_plein, renfort_taille_*, renfort_plein_* de commandeur_reglages.txt)
	bool m_bFrozenAtStart = false;		// cmd_gel_au_depart
	bool m_bStaffFree = true;		// cmd_staff_gratuit
	int m_iFastMinS = 60;		// cmd_cycle_rapide_min_s
	int m_iFastMaxS = 120;		// cmd_cycle_rapide_max_s
	int m_iSlowMinS = 900;		// cmd_cycle_lent_min_s
	int m_iSlowMaxS = 1200;		// cmd_cycle_lent_max_s
	int m_iReportMinS = 30;		// cmd_rapport_min_s
	int m_iReportMaxS = 60;		// cmd_rapport_max_s
	int m_iReportShotsMax = 60;		// cmd_rapport_tirs_max
	int m_iRadioDelayMin = 5;		// cmd_radio_retard_min
	int m_iBlurSeenMinM = 50;		// cmd_flou_vu_min_m
	int m_iBlurSeenMaxM = 100;		// cmd_flou_vu_max_m
	int m_iBlurHeardMinM = 100;		// cmd_flou_entendu_min_m
	int m_iBlurHeardMaxM = 150;		// cmd_flou_entendu_max_m
	int m_iForgetS = 1200;		// cmd_oubli_s
	int m_iMergeM = 200;		// cmd_fusion_contacts_m
	int m_iConfirmS = 300;		// cmd_contact_confirme_s
	int m_iLossJustifyS = 600;		// cmd_pertes_appui_s
	int m_iAimS = 120;		// cmd_contact_vise_s
	int m_iIntenseShots = 30;		// cmd_tirs_intenses_3min
	string m_sTraitWeights = "1, 1, 1, 1";		// cmd_trait_poids
	int m_iSlowTraitDelayS = 75;		// cmd_trait_lent_retard_s
	int m_iPrudentSizeCent = 75;		// cmd_trait_prudent_taille
	int m_iBoldSizeCent = 125;		// cmd_trait_audacieux_taille
	int m_iPrudentShellKeepCent = 50;		// cmd_trait_prudent_reserve_obus
	int m_iMoodStockLowCent = 33;		// cmd_humeur_stock_bas
	int m_iMoodStockHighCent = 66;		// cmd_humeur_stock_haut
	int m_iMoodLosses60 = 15;		// cmd_humeur_pertes_region_60min
	int m_iMoodRetaken60 = 2;		// cmd_humeur_reprises_60min
	int m_iMoodPrudentSizeCent = 75;		// cmd_humeur_prudente_taille
	int m_iMoodAggressiveSizeCent = 125;		// cmd_humeur_agressive_taille
	int m_iGravKeyPoint = 5;		// cmd_grav_point_cle
	int m_iGravKeyPointM = 150;		// cmd_grav_point_cle_m
	int m_iGravCellLost = 3;		// cmd_grav_carre_perdu
	int m_iGravLoss = 1;		// cmd_grav_perte
	int m_iGravShotsPer = 10;		// cmd_grav_tirs_par
	int m_iGravPlayersMax = 6;		// cmd_grav_joueurs_max
	int m_iGravCounterAttack = 2;		// cmd_grav_contre_attaque
	int m_iGravWindowMin = 15;		// cmd_grav_fenetre_min
	int m_iGravServed = 1;		// cmd_grav_zones_servies
	int m_iGravServedMany = 2;		// cmd_grav_zones_servies_nombreux
	int m_iGravManyPlayers = 10;		// cmd_grav_joueurs_nombreux
	int m_iMortarZoneRestS = 600;		// cmd_mortier_repos_zone_s
	int m_iMortarZonesPerCycle = 3;		// cmd_mortier_zones_par_cycle
	int m_iHeliNoTroopM = 800;		// cmd_helico_sans_troupe_m
	int m_iArmorVehicleS = 1200;		// cmd_blinde_vehicule_s
	int m_iArmorGravity = 8;		// cmd_blinde_gravite
	int m_iArtilleryPlayers = 4;		// cmd_artillerie_joueurs
	int m_iArtilleryStillS = 600;		// cmd_artillerie_immobile_s
	int m_iArtilleryStillM = 100;		// cmd_artillerie_immobile_m
	int m_iBombPlayers = 4;		// cmd_bombe_joueurs
	int m_iAlertFullReinforce = 60;		// alerte_renfort_plein
	int m_iReinforceBase = 4;		// renfort_taille_base
	int m_iReinforcePerPlayer = 2;		// renfort_taille_par_joueur
	int m_iReinforceFull = 10;		// renfort_taille_plein
	int m_iFullLosses = 3;		// renfort_plein_pertes
	int m_iFullLossesMin = 10;		// renfort_plein_pertes_min
	bool m_bInformants = true;		// info_actif
	int m_iInfoPeriodS = 30;		// info_periode_s
	int m_iInfoRangeM = 120;		// info_portee_m
	int m_iInfoChance = 25;		// info_chance
	int m_iInfoCivilRestS = 900;		// info_civil_repos_s
	int m_iInfoZoneRestS = 600;		// info_zone_repos_s
	int m_iInfoDelayMinS = 120;		// info_delai_min_s
	int m_iInfoDelayMaxS = 300;		// info_delai_max_s
	int m_iInfoBlurMinM = 150;		// info_flou_min_m
	int m_iInfoBlurMaxM = 300;		// info_flou_max_m
	int m_iInfoCountM = 50;		// info_compte_m

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected static SRP_Commander s_Instance;
	protected SRP_FrontEnemyComponent m_Host;
	protected ref SRP_CmdRegionBook m_Book;							// régions et officiers (SRP_CmdRegions.c)
	protected ref SRP_CmdResources m_Resources;						// stocks et dépôts (SRP_CmdResources.c, SRP_CmdDepot.c)
	protected ref SRP_CmdManeuvers m_Maneuvers;						// infanterie qui bouge (SRP_CmdManeuvers.c)
	protected ref SRP_CmdSupport m_Support;							// feux, hélico, blindés (SRP_CmdSupport.c, SRP_CmdArmor.c)
	protected ref array<ref SRP_CmdZoneIntel> m_aIntel = {};		// par zone (GetZoneCount() + 1)
	protected ref array<ref SRP_CmdContact> m_aContacts = {};		// connaissance (CO5)
	protected ref array<ref SRP_CmdReport> m_aPending = {};			// comptes rendus en route (OF3)
	protected ref array<ref SRP_CmdOrder> m_aReflexQueue = {};		// ordres réflexes en attente (officier lent)
	protected ref array<int> m_aCellSeen = {};						// propriétaire de chaque carré au dernier passage
	protected int m_iFrontVersionSeen = -1;
	protected ref array<IEntity> m_aInformRolled = {};				// civils déjà tirés (pas de map à clé d'entité)
	protected ref array<int> m_aInformRolledAt = {};
	protected ref array<int> m_aServedZones = {};					// zones servies (MA9), les plus graves d'abord
	protected bool m_bStarted;
	protected bool m_bFrozen;										// VU6, sauvé (cmd_gel)
	protected int m_iNextFast;										// heures Unix des prochains cycles
	protected int m_iNextSlow;
	protected int m_iNextInform;
	protected int m_iNextMinute;									// sous-cadence de 60 s (ressources, dépôts)
	protected float m_fIslandMood;									// humeur de l'île G (CO7)
	protected bool m_bStateRead;									// ReadFrom a trouvé cmd_v (sinon le gel part de cmd_gel_au_depart)
	protected bool m_bMergeNew;										// MergeContact : contact neuf
	protected bool m_bMergeUpgraded;								// MergeContact : passé de « entendu » à « vu »
	protected int m_iHeliRetryUnix;									// AP7 : pas d'ordre HELICO du cerveau avant (repos, refus)

	//================================================================================================
	// Vie (hôte : SRP_FrontEnemyComponent ; socle : BuildRegions, WriteTo, ReadFrom, OnRestored)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Serveur, OnPostInit de l'hôte : s_Instance = this, m_Host ; crée m_Book, m_Resources, m_Maneuvers, m_Support
	//! (constructeurs seuls), puis DECLARE les clés (DeclareSettings ici et dans chaque sous-module, capacité et écrans
	//! compris) ; ne trace rien, ne lit rien — SRP_FrontEnemyComponent.OnPostInit
	void SRP_Commander(SRP_FrontEnemyComponent host)
	{
		s_Instance = this;
		m_Host = host;
		m_Book = new SRP_CmdRegionBook(this);
		m_Resources = new SRP_CmdResources(this);
		m_Maneuvers = new SRP_CmdManeuvers(this);
		m_Support = new SRP_CmdSupport(this);
		SRP_CmdLog.Clear();
		DeclareSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Le Commandeur, null s'il est absent (client, composant ennemi absent, avant OnPostInit) — tout le mod
	static SRP_Commander Get()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, OnDelete de l'hôte, APRÈS SRP_FrontComponent.SaveFinalFromEnemy (sauvegarde finale complète) : Stop des
	//! sous-modules (ressources, appuis et leurs CallLater, capacité), puis s_Instance = null ; aucune écriture ici —
	//! SRP_FrontEnemyComponent.OnDelete
	void Stop()
	{
		if (m_Resources)
			m_Resources.Stop();
		if (m_Support)
			m_Support.Stop();
		SRP_CmdCapacity.Get().Stop();
		m_bStarted = false;
		if (s_Instance == this)
			s_Instance = null;
	}

	//------------------------------------------------------------------------------------------------
	//! L'hôte (composant ennemi du front) — sous-modules
	SRP_FrontEnemyComponent GetHost()
	{
		return m_Host;
	}

	//------------------------------------------------------------------------------------------------
	//! Régions et officiers — SRP_CmdScreens, SRP_Missions (OF10), SRP_FrontMarkers, sous-modules
	SRP_CmdRegionBook GetBook()
	{
		return m_Book;
	}

	//------------------------------------------------------------------------------------------------
	//! Stocks et dépôts — SRP_CmdResources.Get, SRP_CmdScreens
	SRP_CmdResources GetResources()
	{
		return m_Resources;
	}

	//------------------------------------------------------------------------------------------------
	//! Manœuvres — SRP_CmdManeuvers.Get
	SRP_CmdManeuvers GetManeuvers()
	{
		return m_Maneuvers;
	}

	//------------------------------------------------------------------------------------------------
	//! Appuis — SRP_CmdSupport.Get
	SRP_CmdSupport GetSupport()
	{
		return m_Support;
	}

	//------------------------------------------------------------------------------------------------
	//! Socle, AVANT Load : m_Book.Trace() (tracé OF1, lignes « region » de front_retouches.txt, rapport
	//! commandeur_regions.txt), m_aIntel dimensionné à GetZoneCount() + 1, SRP_CmdIntel.Reset(régions) —
	//! SRP_FrontComponent.Start
	void BuildRegions()
	{
		m_Book.Trace();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int zones = 0;
		if (front)
			zones = front.GetZoneCount();
		ResizeIntel(zones + 1);
		SRP_CmdIntel.Reset(m_Book.GetRegionCount());
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, 1er Tick de l'hôte qui voit le front prêt : m_aCellSeen copié, SRP_CmdCapacity.Get().Start()
	//! (limite 160, groupes 36/6), m_Book.Start (officiers manquants), m_Resources.Start, m_Maneuvers.Start,
	//! m_Support.Start, premières échéances des cycles ; journal « Commandeur prêt : 6 régions, 6 officiers » —
	//! SRP_FrontEnemyComponent.Tick
	void Start()
	{
		if (m_bStarted)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;

		int nowUnix = System.GetUnixTime();
		if (m_aIntel.Count() != front.GetZoneCount() + 1)
			ResizeIntel(front.GetZoneCount() + 1);
		CopyCells(front);
		// Serveur neuf (aucun état relu) : le gel part du réglage ; sinon l'état sauvé dans front.json fait foi (VU6)
		if (!m_bStateRead)
			m_bFrozen = m_bFrozenAtStart;

		// Q7 : limite des IA actives du jeu (160) et plafond d'éléments (36 dont 6 aux missions), même gelé
		SRP_CmdCapacity.Get().Start();
		m_Book.Start(nowUnix);
		m_Resources.Start(nowUnix);
		m_Maneuvers.Start(nowUnix);
		m_Support.Start(nowUnix);

		// CO4 : premières échéances tirées au hasard ; informateurs et ressources à leur cadence
		m_iNextFast = nowUnix + RandomBetween(m_iFastMinS, m_iFastMaxS);
		m_iNextSlow = nowUnix + RandomBetween(m_iSlowMinS, m_iSlowMaxS);
		m_iNextInform = nowUnix + m_iInfoPeriodS;
		m_iNextMinute = nowUnix + 60;
		m_bStarted = true;

		int regions = m_Book.GetRegionCount();
		int officers = 0;
		for (int region = 0; region < regions; region++)
		{
			if (m_Book.HasLivingOfficer(region))
				officers++;
		}
		string frozenText = "";
		if (m_bFrozen)
			frozenText = " (gelé : aucune décision tant que le Staff ne le dégèle pas)";
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Commandeur prêt : %1 régions, %2 officiers%3", regions, officers, frozenText));
	}

	//------------------------------------------------------------------------------------------------
	//! Start déjà fait — SRP_FrontEnemyComponent.Tick, IsCommanderActive
	bool IsStarted()
	{
		return m_bStarted;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s (après AttackTick de l'hôte) : front absent ou pas prêt -> rien ; capacité (TOUJOURS, même
	//! gelé) ; version du front changée -> DiffCells, m_Book.OnFrontChanged (cachettes) ; CollectGroupReports, ProcessPending,
	//! ExpireKnowledge ; m_Book.Tick ; informateurs (info_periode_s) ; ressources toutes les 60 s ; m_Maneuvers.Tick,
	//! m_Support.Tick ; FastCycle et SlowCycle à leur échéance si non gelé — SRP_FrontEnemyComponent.Tick
	void Tick(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!m_bStarted || !front || !front.IsReady())
			return;

		// 1. Place des soldats (Q7) : toujours, même gelé
		SRP_CmdCapacity.Get().Tick(nowUnix);

		// 2. Carrés changés depuis le dernier passage : comptes rendus de carré, cachettes tombées
		int version = front.GetStateVersion();
		if (version != m_iFrontVersionSeen)
		{
			DiffCells(nowUnix);
			m_iFrontVersionSeen = version;
			m_Book.OnFrontChanged(nowUnix);
		}

		// 3. Comptes rendus (OF3) et connaissance (CO5)
		CollectGroupReports(nowUnix);
		ProcessPending(nowUnix);
		ExpireKnowledge(nowUnix);

		// 4. Officiers
		m_Book.Tick(nowUnix);

		// 5. Informateurs civils (C4) : ils signalent même gelé, mais rien n'en sort tant que les cycles sont arrêtés
		if (nowUnix >= m_iNextInform)
		{
			m_iNextInform = nowUnix + m_iInfoPeriodS;
			InformantsTick(nowUnix);
		}

		// 6. Stocks et dépôts, toutes les 60 s
		if (nowUnix >= m_iNextMinute)
		{
			m_iNextMinute = nowUnix + 60;
			m_Resources.Tick(nowUnix);
		}

		// 7 et 8. Manœuvres (fin propre seulement si gelé) et appuis (aucun tir neuf si gelé)
		m_Maneuvers.Tick(nowUnix);
		m_Support.Tick(nowUnix);

		// 9 et 10. Décisions à deux vitesses (CO4), jamais gelé (VU6)
		if (m_bFrozen)
			return;
		if (nowUnix >= m_iNextFast)
		{
			m_iNextFast = nowUnix + RandomBetween(m_iFastMinS, m_iFastMaxS);
			FastCycle(nowUnix);
		}
		if (nowUnix >= m_iNextSlow)
		{
			m_iNextSlow = nowUnix + RandomBetween(m_iSlowMinS, m_iSlowMaxS);
			SlowCycle(nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés (champs publics ci-dessus) puis LoadSettings de m_Book, m_Resources (et dépôts), m_Maneuvers,
	//! m_Support (et blindés), SRP_CmdCapacity.Get(), SRP_CmdScreens — SRP_FrontEnemyComponent.LoadSettings (après
	//! SRP_CmdSettings.Reload)
	void LoadSettings()
	{
		m_bFrozenAtStart = SRP_CmdSettings.GetBool("cmd_gel_au_depart");
		m_bStaffFree = SRP_CmdSettings.GetBool("cmd_staff_gratuit");
		m_iFastMinS = SRP_CmdSettings.GetIntClamped("cmd_cycle_rapide_min_s", 10, 3600);
		m_iFastMaxS = SRP_CmdSettings.GetIntClamped("cmd_cycle_rapide_max_s", 10, 3600);
		m_iSlowMinS = SRP_CmdSettings.GetIntClamped("cmd_cycle_lent_min_s", 60, 14400);
		m_iSlowMaxS = SRP_CmdSettings.GetIntClamped("cmd_cycle_lent_max_s", 60, 14400);
		m_iReportMinS = SRP_CmdSettings.GetIntClamped("cmd_rapport_min_s", 5, 600);
		m_iReportMaxS = SRP_CmdSettings.GetIntClamped("cmd_rapport_max_s", 5, 600);
		m_iReportShotsMax = SRP_CmdSettings.GetIntClamped("cmd_rapport_tirs_max", 1, 500);
		m_iRadioDelayMin = SRP_CmdSettings.GetIntClamped("cmd_radio_retard_min", 0, 60);
		m_iBlurSeenMinM = SRP_CmdSettings.GetIntClamped("cmd_flou_vu_min_m", 0, 1000);
		m_iBlurSeenMaxM = SRP_CmdSettings.GetIntClamped("cmd_flou_vu_max_m", 0, 1000);
		m_iBlurHeardMinM = SRP_CmdSettings.GetIntClamped("cmd_flou_entendu_min_m", 0, 2000);
		m_iBlurHeardMaxM = SRP_CmdSettings.GetIntClamped("cmd_flou_entendu_max_m", 0, 2000);
		m_iForgetS = SRP_CmdSettings.GetIntClamped("cmd_oubli_s", 60, 7200);
		m_iMergeM = SRP_CmdSettings.GetIntClamped("cmd_fusion_contacts_m", 10, 2000);
		m_iConfirmS = SRP_CmdSettings.GetIntClamped("cmd_contact_confirme_s", 10, 3600);
		m_iLossJustifyS = SRP_CmdSettings.GetIntClamped("cmd_pertes_appui_s", 10, 7200);
		m_iAimS = SRP_CmdSettings.GetIntClamped("cmd_contact_vise_s", 10, 3600);
		m_iIntenseShots = SRP_CmdSettings.GetIntClamped("cmd_tirs_intenses_3min", 1, 1000);
		m_sTraitWeights = SRP_CmdSettings.GetString("cmd_trait_poids");
		m_iSlowTraitDelayS = SRP_CmdSettings.GetIntClamped("cmd_trait_lent_retard_s", 0, 1800);
		m_iPrudentSizeCent = SRP_CmdSettings.GetIntClamped("cmd_trait_prudent_taille", 10, 300);
		m_iBoldSizeCent = SRP_CmdSettings.GetIntClamped("cmd_trait_audacieux_taille", 10, 300);
		m_iPrudentShellKeepCent = SRP_CmdSettings.GetIntClamped("cmd_trait_prudent_reserve_obus", 0, 100);
		m_iMoodStockLowCent = SRP_CmdSettings.GetIntClamped("cmd_humeur_stock_bas", 0, 100);
		m_iMoodStockHighCent = SRP_CmdSettings.GetIntClamped("cmd_humeur_stock_haut", 0, 100);
		m_iMoodLosses60 = SRP_CmdSettings.GetIntClamped("cmd_humeur_pertes_region_60min", 1, 500);
		m_iMoodRetaken60 = SRP_CmdSettings.GetIntClamped("cmd_humeur_reprises_60min", 1, 100);
		m_iMoodPrudentSizeCent = SRP_CmdSettings.GetIntClamped("cmd_humeur_prudente_taille", 10, 300);
		m_iMoodAggressiveSizeCent = SRP_CmdSettings.GetIntClamped("cmd_humeur_agressive_taille", 10, 300);
		m_iGravKeyPoint = SRP_CmdSettings.GetIntClamped("cmd_grav_point_cle", 0, 100);
		m_iGravKeyPointM = SRP_CmdSettings.GetIntClamped("cmd_grav_point_cle_m", 10, 1000);
		m_iGravCellLost = SRP_CmdSettings.GetIntClamped("cmd_grav_carre_perdu", 0, 100);
		m_iGravLoss = SRP_CmdSettings.GetIntClamped("cmd_grav_perte", 0, 100);
		m_iGravShotsPer = SRP_CmdSettings.GetIntClamped("cmd_grav_tirs_par", 1, 1000);
		m_iGravPlayersMax = SRP_CmdSettings.GetIntClamped("cmd_grav_joueurs_max", 0, 100);
		m_iGravCounterAttack = SRP_CmdSettings.GetIntClamped("cmd_grav_contre_attaque", 0, 100);
		m_iGravWindowMin = SRP_CmdSettings.GetIntClamped("cmd_grav_fenetre_min", 1, 60);
		m_iGravServed = SRP_CmdSettings.GetIntClamped("cmd_grav_zones_servies", 1, 20);
		m_iGravServedMany = SRP_CmdSettings.GetIntClamped("cmd_grav_zones_servies_nombreux", 1, 20);
		m_iGravManyPlayers = SRP_CmdSettings.GetIntClamped("cmd_grav_joueurs_nombreux", 1, 100);
		m_iMortarZoneRestS = SRP_CmdSettings.GetIntClamped("cmd_mortier_repos_zone_s", 0, 7200);
		m_iMortarZonesPerCycle = SRP_CmdSettings.GetIntClamped("cmd_mortier_zones_par_cycle", 1, 20);
		m_iHeliNoTroopM = SRP_CmdSettings.GetIntClamped("cmd_helico_sans_troupe_m", 0, 5000);
		m_iArmorVehicleS = SRP_CmdSettings.GetIntClamped("cmd_blinde_vehicule_s", 0, 7200);
		m_iArmorGravity = SRP_CmdSettings.GetIntClamped("cmd_blinde_gravite", 0, 100);
		m_iArtilleryPlayers = SRP_CmdSettings.GetIntClamped("cmd_artillerie_joueurs", 1, 50);
		m_iArtilleryStillS = SRP_CmdSettings.GetIntClamped("cmd_artillerie_immobile_s", 60, 7200);
		m_iArtilleryStillM = SRP_CmdSettings.GetIntClamped("cmd_artillerie_immobile_m", 10, 1000);
		m_iBombPlayers = SRP_CmdSettings.GetIntClamped("cmd_bombe_joueurs", 1, 50);
		m_iAlertFullReinforce = SRP_CmdSettings.GetIntClamped("alerte_renfort_plein", 0, 1000);
		m_iReinforceBase = SRP_CmdSettings.GetIntClamped("renfort_taille_base", 1, 30);
		m_iReinforcePerPlayer = SRP_CmdSettings.GetIntClamped("renfort_taille_par_joueur", 0, 10);
		m_iReinforceFull = SRP_CmdSettings.GetIntClamped("renfort_taille_plein", 1, 30);
		m_iFullLosses = SRP_CmdSettings.GetIntClamped("renfort_plein_pertes", 1, 50);
		m_iFullLossesMin = SRP_CmdSettings.GetIntClamped("renfort_plein_pertes_min", 1, 60);
		m_bInformants = SRP_CmdSettings.GetBool("info_actif");
		m_iInfoPeriodS = SRP_CmdSettings.GetIntClamped("info_periode_s", 5, 600);
		m_iInfoRangeM = SRP_CmdSettings.GetIntClamped("info_portee_m", 10, 1000);
		m_iInfoChance = SRP_CmdSettings.GetIntClamped("info_chance", 0, 100);
		m_iInfoCivilRestS = SRP_CmdSettings.GetIntClamped("info_civil_repos_s", 0, 7200);
		m_iInfoZoneRestS = SRP_CmdSettings.GetIntClamped("info_zone_repos_s", 0, 7200);
		m_iInfoDelayMinS = SRP_CmdSettings.GetIntClamped("info_delai_min_s", 0, 3600);
		m_iInfoDelayMaxS = SRP_CmdSettings.GetIntClamped("info_delai_max_s", 0, 3600);
		m_iInfoBlurMinM = SRP_CmdSettings.GetIntClamped("info_flou_min_m", 0, 2000);
		m_iInfoBlurMaxM = SRP_CmdSettings.GetIntClamped("info_flou_max_m", 0, 2000);
		m_iInfoCountM = SRP_CmdSettings.GetIntClamped("info_compte_m", 0, 500);
		if (m_Book)
			m_Book.LoadSettings();
		if (m_Resources)
			m_Resources.LoadSettings();
		if (m_Maneuvers)
			m_Maneuvers.LoadSettings();
		if (m_Support)
			m_Support.LoadSettings();
		SRP_CmdCapacity.Get().LoadSettings();
		SRP_CmdScreens.LoadSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés (défaut = valeur du champ) puis DeclareSettings de chaque sous-module, de la capacité et des
	//! écrans — constructeur
	protected void DeclareSettings()
	{
		SRP_CmdSettings.DeclareBool("cmd_gel_au_depart", m_bFrozenAtStart, "Commandeur gelé au début d'une nouvelle campagne (ensuite l'état sauvé dans front.json fait foi) (VU6)");
		SRP_CmdSettings.DeclareBool("cmd_staff_gratuit", m_bStaffFree, "Une décision forcée par le Staff ne coûte rien aux stocks (VU5)");
		SRP_CmdSettings.DeclareInt("cmd_cycle_rapide_min_s", m_iFastMinS, "Cycle rapide (renforts, mortier, hélico, enquêtes) : écart minimal entre deux, en secondes (CO4)");
		SRP_CmdSettings.DeclareInt("cmd_cycle_rapide_max_s", m_iFastMaxS, "Cycle rapide : écart maximal, en secondes (CO4)");
		SRP_CmdSettings.DeclareInt("cmd_cycle_lent_min_s", m_iSlowMinS, "Cycle lent (blindé, artillerie lourde, bombe) : écart minimal, en secondes (CO4)");
		SRP_CmdSettings.DeclareInt("cmd_cycle_lent_max_s", m_iSlowMaxS, "Cycle lent : écart maximal, en secondes (CO4)");
		SRP_CmdSettings.DeclareInt("cmd_rapport_min_s", m_iReportMinS, "Comptes rendus de chaque unité : écart minimal, en secondes (OF3)");
		SRP_CmdSettings.DeclareInt("cmd_rapport_max_s", m_iReportMaxS, "Comptes rendus de chaque unité : écart maximal, en secondes (OF3)");
		SRP_CmdSettings.DeclareInt("cmd_rapport_tirs_max", m_iReportShotsMax, "Tirs retenus au plus par unité et par compte rendu (CO6)");
		SRP_CmdSettings.DeclareInt("cmd_radio_retard_min", m_iRadioDelayMin, "Retard des nouvelles d'une localité dont l'opérateur radio est tombé, en minutes (OF3)");
		SRP_CmdSettings.DeclareInt("cmd_flou_vu_min_m", m_iBlurSeenMinM, "Précision d'une position vue : écart minimal, en mètres (CO5)");
		SRP_CmdSettings.DeclareInt("cmd_flou_vu_max_m", m_iBlurSeenMaxM, "Précision d'une position vue : écart maximal, en mètres (CO5)");
		SRP_CmdSettings.DeclareInt("cmd_flou_entendu_min_m", m_iBlurHeardMinM, "Précision d'une position entendue : écart minimal, en mètres (CO5)");
		SRP_CmdSettings.DeclareInt("cmd_flou_entendu_max_m", m_iBlurHeardMaxM, "Précision d'une position entendue : écart maximal, en mètres (CO5)");
		SRP_CmdSettings.DeclareInt("cmd_oubli_s", m_iForgetS, "Un contact sans nouveau compte rendu est oublié après ce nombre de secondes (CO5)");
		SRP_CmdSettings.DeclareInt("cmd_fusion_contacts_m", m_iMergeM, "Deux contacts plus proches que cette distance, en mètres, n'en font qu'un (CO5)");
		SRP_CmdSettings.DeclareInt("cmd_contact_confirme_s", m_iConfirmS, "Appui lourd justifié : joueur vu depuis moins de ce nombre de secondes (CO6)");
		SRP_CmdSettings.DeclareInt("cmd_pertes_appui_s", m_iLossJustifyS, "Appui lourd justifié aussi : pertes depuis moins de ce nombre de secondes (CO6)");
		SRP_CmdSettings.DeclareInt("cmd_contact_vise_s", m_iAimS, "Pour viser (mortier, artillerie, bombe, hélico) : contact vu depuis ce nombre de secondes au plus (CO6)");
		SRP_CmdSettings.DeclareInt("cmd_tirs_intenses_3min", m_iIntenseShots, "Au-delà de ce nombre de tirs en 3 min, le combat est intense : renfort plein (CO6)");
		SRP_CmdSettings.DeclareString("cmd_trait_poids", m_sTraitWeights, "Poids du tirage des caractères : prudent, audacieux, lent, méthodique (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_trait_lent_retard_s", m_iSlowTraitDelayS, "Officier lent : ses ordres partent ce nombre de secondes plus tard (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_trait_prudent_taille", m_iPrudentSizeCent, "Officier prudent : taille des renforts, en centièmes (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_trait_audacieux_taille", m_iBoldSizeCent, "Officier audacieux : taille des renforts, en centièmes (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_trait_prudent_reserve_obus", m_iPrudentShellKeepCent, "Officier ou humeur prudents : pas de mortier sous ce nombre de centièmes d'obus de la région (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_humeur_stock_bas", m_iMoodStockLowCent, "Humeur : moyens lourds bas sous ce nombre de centièmes (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_humeur_stock_haut", m_iMoodStockHighCent, "Humeur : moyens lourds hauts à partir de ce nombre de centièmes (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_humeur_pertes_region_60min", m_iMoodLosses60, "Humeur prudente d'une région à partir de ce nombre de pertes en 60 min (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_humeur_reprises_60min", m_iMoodRetaken60, "Humeur agressive d'une région à partir de ce nombre de carrés repris en 60 min (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_humeur_prudente_taille", m_iMoodPrudentSizeCent, "Humeur prudente : taille des renforts, en centièmes (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_humeur_agressive_taille", m_iMoodAggressiveSizeCent, "Humeur agressive : taille des renforts, en centièmes (CO7)");
		SRP_CmdSettings.DeclareInt("cmd_grav_point_cle", m_iGravKeyPoint, "Gravité d'une zone : point clé menacé (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_point_cle_m", m_iGravKeyPointM, "Gravité : un point clé est menacé par un joueur connu à cette distance, en mètres (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_carre_perdu", m_iGravCellLost, "Gravité : par carré perdu dans la fenêtre (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_perte", m_iGravLoss, "Gravité : par soldat perdu dans la fenêtre (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_tirs_par", m_iGravShotsPer, "Gravité : plus 1 par tranche de ce nombre de tirs (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_joueurs_max", m_iGravPlayersMax, "Gravité : plus 1 par joueur connu, au plus ce nombre (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_contre_attaque", m_iGravCounterAttack, "Gravité : en plus si une contre-attaque est en assaut sur la zone (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_fenetre_min", m_iGravWindowMin, "Fenêtre de calcul de la gravité, en minutes (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_zones_servies", m_iGravServed, "Zones servies par les moyens du Commandeur (la contre-attaque l'est toujours) (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_zones_servies_nombreux", m_iGravServedMany, "Zones servies quand les joueurs sont nombreux (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_grav_joueurs_nombreux", m_iGravManyPlayers, "Joueurs connectés à partir desquels ils sont nombreux (MA9)");
		SRP_CmdSettings.DeclareInt("cmd_mortier_repos_zone_s", m_iMortarZoneRestS, "Pas deux demandes de mortier sur une même zone en moins de ce nombre de secondes (CO4)");
		SRP_CmdSettings.DeclareInt("cmd_mortier_zones_par_cycle", m_iMortarZonesPerCycle, "Zones examinées pour le mortier à chaque cycle rapide, les plus graves d'abord (CO4)");
		SRP_CmdSettings.DeclareInt("cmd_helico_sans_troupe_m", m_iHeliNoTroopM, "Hélico : seulement si aucune unité ennemie n'est à cette distance du contact, en mètres (AP7)");
		SRP_CmdSettings.DeclareInt("cmd_blinde_vehicule_s", m_iArmorVehicleS, "Blindé en riposte : véhicule armé vu depuis moins de ce nombre de secondes (AP5)");
		SRP_CmdSettings.DeclareInt("cmd_blinde_gravite", m_iArmorGravity, "Blindé en renfort : gravité de la zone au moins égale, avec un point clé menacé (AP5)");
		SRP_CmdSettings.DeclareInt("cmd_artillerie_joueurs", m_iArtilleryPlayers, "Artillerie lourde : contact vu d'au moins ce nombre de joueurs, resté immobile (AP2)");
		SRP_CmdSettings.DeclareInt("cmd_artillerie_immobile_s", m_iArtilleryStillS, "Artillerie lourde : immobile depuis ce nombre de secondes (AP2)");
		SRP_CmdSettings.DeclareInt("cmd_artillerie_immobile_m", m_iArtilleryStillM, "Artillerie lourde : resté à cette distance de sa première position, en mètres (AP2)");
		SRP_CmdSettings.DeclareInt("cmd_bombe_joueurs", m_iBombPlayers, "Bombe : contact immobile d'au moins ce nombre de joueurs (AP3)");
		SRP_CmdSettings.DeclareInt("alerte_renfort_plein", m_iAlertFullReinforce, "À partir de cette alerte de région, les renforts partent pleins (CO9)");
		SRP_CmdSettings.DeclareInt("renfort_taille_base", m_iReinforceBase, "Taille du renfort demandé : base (MA1)");
		SRP_CmdSettings.DeclareInt("renfort_taille_par_joueur", m_iReinforcePerPlayer, "Taille du renfort demandé : plus ce nombre par joueur connu (MA1)");
		SRP_CmdSettings.DeclareInt("renfort_taille_plein", m_iReinforceFull, "Taille du renfort si le combat est intense, les pertes lourdes ou l'alerte haute (MA1)");
		SRP_CmdSettings.DeclareInt("renfort_plein_pertes", m_iFullLosses, "Renfort plein aussi à partir de ce nombre de soldats tombés dans la zone (MA1)");
		SRP_CmdSettings.DeclareInt("renfort_plein_pertes_min", m_iFullLossesMin, "Pertes du renfort plein comptées sur ce nombre de minutes (MA1, 60 au plus)");
		SRP_CmdSettings.DeclareBool("info_actif", m_bInformants, "Informateurs civils (C4)");
		SRP_CmdSettings.DeclareInt("info_periode_s", m_iInfoPeriodS, "Passage des informateurs, en secondes (C4)");
		SRP_CmdSettings.DeclareInt("info_portee_m", m_iInfoRangeM, "Un civil signale un joueur vu à cette distance ou moins, en mètres (C4)");
		SRP_CmdSettings.DeclareInt("info_chance", m_iInfoChance, "Chances sur 100 qu'il le signale (C4)");
		SRP_CmdSettings.DeclareInt("info_civil_repos_s", m_iInfoCivilRestS, "Un même civil ne signale qu'une fois par ce nombre de secondes (C4)");
		SRP_CmdSettings.DeclareInt("info_zone_repos_s", m_iInfoZoneRestS, "Une zone ne donne qu'un signalement par ce nombre de secondes (C4)");
		SRP_CmdSettings.DeclareInt("info_delai_min_s", m_iInfoDelayMinS, "Le signalement arrive au plus tôt ce nombre de secondes plus tard (C4)");
		SRP_CmdSettings.DeclareInt("info_delai_max_s", m_iInfoDelayMaxS, "Le signalement arrive au plus tard ce nombre de secondes plus tard (C4)");
		SRP_CmdSettings.DeclareInt("info_flou_min_m", m_iInfoBlurMinM, "Position signalée : écart minimal, en mètres (C4)");
		SRP_CmdSettings.DeclareInt("info_flou_max_m", m_iInfoBlurMaxM, "Position signalée : écart maximal, en mètres (C4)");
		SRP_CmdSettings.DeclareInt("info_compte_m", m_iInfoCountM, "Joueurs comptés autour du joueur vu, en mètres (C4)");
		if (!m_Book || !m_Resources || !m_Maneuvers || !m_Support)
			return;
		m_Book.DeclareSettings();
		m_Resources.DeclareSettings();
		m_Maneuvers.DeclareSettings();
		m_Support.DeclareSettings();
		SRP_CmdCapacity.Get().DeclareSettings();
		SRP_CmdScreens.DeclareSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Bouton « commandeur:relire » : SRP_FrontComponent.GetInstance().ReloadSettings(author) (relit les DEUX
	//! fichiers et tous les LoadSettings) ; rend le compte rendu, en rappelant que cmd_regions et les bornes de
	//! région ne changent qu'au redémarrage — SRP_CmdScreens.RunStaff
	string ReloadSettings(string author)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "Front absent : réglages non relus";
		string report = front.ReloadSettings(author);
		return report + " · le découpage des régions (cmd_regions et ses bornes) ne change qu'au redémarrage";
	}

	//================================================================================================
	// Gel (VU6) et Staff (VU5) : seules entrées
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Commandeur gelé : aucune décision, aucun appui ; la capacité et les règles du front continuent — tout le mod
	bool IsFrozen()
	{
		return m_bFrozen;
	}

	//------------------------------------------------------------------------------------------------
	//! VU6 : gèle ou dégèle (sauvé, cmd_gel, MarkDirty(true)) ; au gel, les réflexes en file sont vidés ; journal
	//! StaffAction ; aucune radio ; rend le compte rendu — SRP_CmdScreens.RunStaff
	string SetFrozen(bool frozen, string author)
	{
		if (m_bFrozen == frozen)
		{
			if (frozen)
				return "Commandeur déjà gelé";
			return "Commandeur déjà actif";
		}
		m_bFrozen = frozen;
		int nowUnix = System.GetUnixTime();
		string text;
		if (frozen)
		{
			m_aReflexQueue.Clear();
			text = "Commandeur gelé : plus aucune décision ni appui ; la place des soldats, les comptes rendus, les officiers et les règles du front continuent";
		}
		else
		{
			// Les cycles repartent de zéro : pas de rafale de décisions au dégel
			m_iNextFast = nowUnix + RandomBetween(m_iFastMinS, m_iFastMaxS);
			m_iNextSlow = nowUnix + RandomBetween(m_iSlowMinS, m_iSlowMaxS);
			text = "Commandeur dégelé : il reprend ses décisions au prochain cycle";
		}
		SRP_CmdLog.StaffAction(author, text);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(true);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! VU5, SEULE entrée pour forcer une décision : ordre m_bStaff (sans seuil, plafond ni délai, gratuit si
	//! cmd_staff_gratuit ; jamais sans la sécurité : base, soldats ou civils trop près, 120 soldats), passé
	//! directement au moyen (IssueOrder). variant : BLINDE -> type (0 BTR-70, 1 BRDM-2, 2 Typhoon, -1 tirage) ;
	//! MORTIER, FUMEE -> obus (0 = défaut) ; sinon -1. HELICO (outre stock, repos et « personne sur place ») et COLONNE
	//! (RequestColumn) passent aussi par ici : aucune autre porte Staff pour une décision (ForceNextHeli et
	//! RequestTransfer n'existent pas). Marche aussi gelé. Rend un compte rendu court (« ordre donné » ou « refusé :
	//! raison ») ; n'écrit PAS la ligne [Staff] (SRP_CmdScreens.RunStaff l'écrit) — SRP_CmdScreens
	string ForceOrder(int kind, int variant, vector position, string author)
	{
		if (!m_bStarted)
			return "refusé : Commandeur pas encore prêt (démarrage du front)";
		if (kind < SRP_ECmdOrder.RENFORT || kind > SRP_ECmdOrder.DECROCHAGE)
			return "refusé : moyen inconnu";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "refusé : front pas encore prêt";

		int nowUnix = System.GetUnixTime();
		string who = author;
		if (who.IsEmpty())
			who = "Staff";
		int zone = front.GetZoneAt(position);
		int region = m_Book.GetRegionAt(position);
		SRP_CmdOrder order = NewOrder(kind, region, zone, position, "décision forcée par " + who, nowUnix);
		order.m_iVariant = variant;
		order.m_sAuthor = who;
		order.m_bStaff = true;
		if (kind == SRP_ECmdOrder.RENFORT)
		{
			int players = KnownPlayersNear(position, m_iMergeM, m_iForgetS);
			order.m_iSize = ReinforcementSize(zone, Math.MaxInt(1, players), nowUnix);
		}
		if (kind == SRP_ECmdOrder.BLINDE)
			order.m_iArmorReason = SRP_ECmdArmorReason.STAFF;

		// Compte rendu court : RunStaff l'affiche après « Forcé : <moyen> <cible> (carré X) » et écrit la ligne [Staff] ;
		// le détail (coût, stock, refus) est déjà au journal, écrit par IssueOrder (et la ligne d'exécution par le moyen)
		string why = IssueOrder(order);
		if (why.IsEmpty())
			return "ordre donné";
		return "refusé : " + why;
	}

	//------------------------------------------------------------------------------------------------
	//! VU5 : chute forcée de l'officier (tué ou capturé), même suite qu'une vraie chute (radio, désorganisation,
	//! renseignement si capturé) — SRP_CmdScreens.RunStaff
	string ForceOfficerDown(int region, bool captured, string author)
	{
		if (!m_bStarted)
			return "Commandeur pas encore prêt (démarrage du front)";
		if (!m_Book.GetRegion(region))
			return "Région inconnue";
		if (!m_Book.HasLivingOfficer(region))
			return m_Book.GetRegionLabel(region) + " : aucun officier vivant";
		string cause = "tué, chute forcée par le Staff";
		if (captured)
			cause = "capturé, chute forcée par le Staff";
		return m_Book.OfficerDown(region, captured, cause, author);
	}

	//------------------------------------------------------------------------------------------------
	//! VU5 : nouvel officier tout de suite (fin de la désorganisation, correction silencieuse, VU2) —
	//! SRP_CmdScreens.RunStaff
	string ForceOfficerReplace(int region, string author)
	{
		if (!m_bStarted)
			return "Commandeur pas encore prêt (démarrage du front)";
		if (!m_Book.GetRegion(region))
			return "Région inconnue";
		if (m_Book.GetRegionState(region) == SRP_ECmdRegionState.LIBEREE)
			return m_Book.GetRegionLabel(region) + " : région libérée, plus jamais d'officier (OF11)";
		return m_Book.NewOfficer(region, author);
	}

	//------------------------------------------------------------------------------------------------
	//! ia-reset et wipe : réflexes et comptes rendus vidés, m_Maneuvers.CancelAll (colonnes rendues, papier, raids,
	//! replis), m_Support.CancelAll (pièces, batteries, blindés retirés, tickets rendus), officiers posés remis
	//! « sur le papier » ; rend le compte rendu — SRP_AdminComponent (ia-reset, wipe)
	string CancelOperations(string author)
	{
		int orders = m_aReflexQueue.Count();
		int reports = m_aPending.Count();
		m_aReflexQueue.Clear();
		m_aPending.Clear();
		string maneuvers = m_Maneuvers.CancelAll(author);
		string support = m_Support.CancelAll(author);
		m_Book.UnpostOfficers("opérations annulées par le Staff");

		string text = string.Format("Opérations du Commandeur annulées : %1 ordre(s) en attente et %2 compte(s) rendu(s) en route effacés, officiers remis hors de vue", orders, reports);
		if (!maneuvers.IsEmpty())
			text = text + " · " + maneuvers;
		if (!support.IsEmpty())
			text = text + " · " + support;
		SRP_CmdLog.StaffAction(author, text);
		return text;
	}

	//================================================================================================
	// Relais neutres du front (appelés par SRP_FrontEnemyComponent ; Q9 : staff = aucun effet visible)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Serveur : victime = data.GetVictimEntity() ; officier (m_Book.OnPossibleOfficerDeath) -> fini ; joueur ->
	//! ignoré (CO5) ; soldat ennemi reconnu par son GROUPE enregistré (SRP_EnemyComponent.FindRecord, jamais la clé de
	//! faction, trou 22) -> compte rendu PERTE — SRP_FrontEnemyComponent.OnControllableDestroyed
	void OnControllableDestroyed(notnull SCR_InstigatorContextData data)
	{
		IEntity victim = data.GetVictimEntity();
		if (!victim)
			return;
		// Un officier de région : sa chute a sa propre suite (OF6, OF7), ce n'est pas une perte ordinaire
		if (m_Book.OnPossibleOfficerDeath(victim))
			return;
		// Un joueur tué n'apprend rien au Commandeur (CO5)
		if (data.GetVictimPlayerID() > 0)
			return;
		if (!m_bStarted)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!front || !front.IsReady() || !enemies)
			return;
		SRP_EnemyGroup record = FindVictimRecord(enemies, victim);
		if (!record)
			return;		// civil, ou soldat qui n'est pas à nous

		vector position = victim.GetOrigin();
		int zone = front.GetZoneAt(position);
		if (zone < 0 || front.IsBaseZone(zone))
			return;
		int region = m_Book.GetRegionOfZone(zone);
		if (region < 0)
			return;
		int nowUnix = System.GetUnixTime();
		SRP_CmdReport report = new SRP_CmdReport();
		report.m_iKind = SRP_ECmdReport.PERTE;
		report.m_iRegion = region;
		report.m_iZone = zone;
		report.m_iCell = front.CellIndexAt(position);
		report.m_vPos = position;		// une perte est connue à sa place exacte
		report.m_sSource = SourceOf(record);
		report.m_bNeedsRadio = !record.m_sZone.IsEmpty();
		report.m_vRadioPos = position;
		report.m_bDirect = IsDirectUnit(record);
		report.m_iDueUnix = nowUnix + RandomBetween(m_iReportMinS, m_iReportMaxS);
		m_aPending.Insert(report);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone tombée (D1) ou forcée bleue : alerte alerte_zone de sa région (hors staff), m_Book.CheckLiberation(zone,
	//! staff, maintenant) (SEULE porte OF11), m_Resources.OnZoneOwnerChanged(zone, BLEU, staff) (qui relaie aux
	//! dépôts), m_Maneuvers.OnZoneCaptured(zone, staff) (MA11 hors staff) ; staff : état seul, sans alerte ni annonce
	//! (Q9) — SRP_FrontEnemyComponent.OnZoneCaptured
	void OnZoneCaptured(int zone, bool staff)
	{
		int nowUnix = System.GetUnixTime();
		if (!staff)
		{
			int region = m_Book.GetRegionOfZone(zone);
			if (region >= 0)
				m_Book.AddRegionAlert(region, m_Book.m_iAlertZone, "zone perdue : " + ZoneText(zone));
		}
		m_Book.CheckLiberation(zone, staff, nowUnix);
		m_Resources.OnZoneOwnerChanged(zone, SRP_EFrontOwner.BLEU, staff);
		m_Maneuvers.OnZoneCaptured(zone, staff);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone repassée rouge : rattachement OF11 si sa région est libérée (m_Book.AttachZone), m_Resources
	//! .OnZoneOwnerChanged(zone, ROUGE, staff), m_Maneuvers.OnZoneLost ; staff : état seul (Q9) —
	//! SRP_FrontEnemyComponent.OnZoneLost
	void OnZoneLost(int zone, int reason, bool staff)
	{
		int nowUnix = System.GetUnixTime();
		int region = m_Book.GetRegionOfZone(zone);
		if (region >= 0 && m_Book.GetRegionState(region) == SRP_ECmdRegionState.LIBEREE)
			m_Book.AttachZone(zone, nowUnix);
		m_Resources.OnZoneOwnerChanged(zone, SRP_EFrontOwner.ROUGE, staff);
		m_Maneuvers.OnZoneLost(zone, reason, staff);
	}

	//------------------------------------------------------------------------------------------------
	//! Localité prise : cachettes de l'officier revues (m_Book), replis et colonnes réorientés (m_Maneuvers) —
	//! SRP_FrontEnemyComponent.OnLocalityTaken
	void OnLocalityTaken(string locality)
	{
		// La cachette du jour tombée est la seule suite côté registre (même porte que le changement de version)
		m_Book.OnFrontChanged(System.GetUnixTime());
		m_Maneuvers.OnLocalityTaken(locality);
	}

	//------------------------------------------------------------------------------------------------
	//! UNE entrée par mission réussie (trou 14) : renseignement si le site est sur un carré rouge
	//! (SRP_CmdIntel.OnMissionSucceeded, qui révèle aussi le dépôt pour ECOUTE et DOCUMENTS) ; quart de stock retiré
	//! pour SABOTAGE et CACHE (m_Resources.OnSabotageMission) ; alerte alerte_sabotage comptée UNE fois —
	//! SRP_FrontEnemyComponent.OnMissionSucceeded
	void OnMissionSucceeded(int missionType, vector site, string missionId, string targetPrefab, string author)
	{
		SRP_CmdIntel.OnMissionSucceeded(missionType, site, missionId);
		if (missionType != SRP_EMissionType.SABOTAGE && missionType != SRP_EMissionType.CACHE)
			return;
		int region = m_Book.GetRegionAt(site);
		if (region < 0 || !m_Book.IsRegionAlive(region))
			return;
		m_Resources.OnSabotageMission(region, missionType, targetPrefab, author);
		// CO9 : un sabotage fait monter l'alerte de sa région, une seule fois par mission
		m_Book.AddRegionAlert(region, m_Book.m_iAlertSabotage, "mission " + missionId + " réussie");
	}

	//------------------------------------------------------------------------------------------------
	//! Objet utilisé par un joueur (« Saboter ») : dépôt ennemi (m_Resources.GetDepots().TryUse) puis pièce d'appui
	//! (m_Support.OnSabotage) ; vrai = consommé, message dans reply — SRP_FrontEnemyComponent.OnSabotageUsed
	bool OnSabotageUsed(IEntity target, int playerId, out string reply)
	{
		reply = "";
		if (!target)
			return false;
		SRP_CmdDepots depots = m_Resources.GetDepots();
		string message;
		if (depots && depots.TryUse(target, playerId, message))
		{
			reply = message;
			return true;
		}
		string answer;
		if (m_Support.OnSabotage(target, playerId, answer))
		{
			reply = answer;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! La menace de l'île a changé : m_Resources.OnThreatChanged (pleins RE10) — SRP_FrontEnemyComponent.OnThreatChanged
	void OnThreatChanged(int threat)
	{
		m_Resources.OnThreatChanged(threat);
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne (B3, K1, Staff) : régions retracées, officiers neufs, alertes à 0, rattachements effacés,
	//! connaissance vidée, puis ResetCampaign de m_Resources, m_Maneuvers, m_Support ; SRP_CmdIntel.Reset,
	//! SRP_CmdLog.Clear ; m_bFrozen = m_bFrozenAtStart — SRP_FrontEnemyComponent.ResetAll
	void ResetCampaign(string reason)
	{
		m_Book.ResetCampaign(reason);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int zones = 0;
		if (front)
			zones = front.GetZoneCount();
		ResizeIntel(zones + 1);
		ForgetKnowledge();
		m_iHeliRetryUnix = 0;		// repos d'hélico effacé aussi par les appuis (m_Support.ResetCampaign)
		m_Resources.ResetCampaign(reason);
		m_Maneuvers.ResetCampaign(reason);
		m_Support.ResetCampaign(reason);
		SRP_CmdIntel.Reset(m_Book.GetRegionCount());
		SRP_CmdLog.Clear();
		m_bFrozen = m_bFrozenAtStart;
		if (front && front.IsReady())
			CopyCells(front);

		string frozenText = "";
		if (m_bFrozen)
			frozenText = ", Commandeur gelé (cmd_gel_au_depart)";
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Nouvelle campagne (%1) : %2 régions retracées, officiers neufs, alertes à zéro%3", reason, m_Book.GetRegionCount(), frozenText));
		if (front)
			front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Après RestoreDaily (ReadFrom déjà fait) : officiers posés retirés hors de vue, connaissance et files vidées,
	//! m_aCellSeen recopié sans compte rendu, OnRestored des sous-modules — SRP_FrontEnemyComponent.OnRestored
	void OnRestored()
	{
		m_Book.OnRestored();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int zones = 0;
		if (front)
			zones = front.GetZoneCount();
		ResizeIntel(zones + 1);
		ForgetKnowledge();
		m_iHeliRetryUnix = 0;		// les appuis relisent leur repos d'hélico de la copie (m_Support.ReadFrom)
		if (front && front.IsReady())
			CopyCells(front);
		m_Resources.OnRestored();
		m_Maneuvers.OnRestored();
		m_Support.OnRestored();
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Commandeur ramené à la copie du jour : connaissance et ordres en attente effacés");
	}

	//------------------------------------------------------------------------------------------------
	//! J2, Q9 : zone remise à son état de départ par le Staff (SRP_FrontComponent.ResetZone, appelé après la repeinte
	//! de ses carrés) : ses carrés sont recopiés dans m_aCellSeen sans compte rendu (ni carré repris, ni humeur) ; les
	//! autres carrés gardent leur comparaison au prochain DiffCells — SRP_FrontEnemyComponent.OnFrontReset (ZONE)
	void OnZoneReset(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!m_bStarted || !front || !front.IsReady())
			return;
		int count = front.GetCellCount();
		if (m_aCellSeen.Count() != count)
		{
			CopyCells(front);
			return;
		}
		for (int cell = 0; cell < count; cell++)
		{
			if (front.GetCellZone(cell) == zone)
				m_aCellSeen[cell] = front.GetCellOwner(cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Clés cmd_v, cmd_gel, cmd_hash, puis m_Book (cmd_r*), m_Resources (cmd_res_*, puis dépôts cmd_dep_*),
	//! m_Maneuvers (cmd_ma_*), m_Support (cmd_ap_*), SRP_CmdIntel (cmd_vu_*) — SRP_FrontEnemyComponent.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		int frozen = 0;
		if (m_bFrozen)
			frozen = 1;
		ctx.WriteValue("cmd_v", 1);
		ctx.WriteValue("cmd_gel", frozen);
		ctx.WriteValue("cmd_hash", m_Book.GetHash());
		m_Book.WriteTo(ctx);
		m_Resources.WriteTo(ctx);
		m_Maneuvers.WriteTo(ctx);
		m_Support.WriteTo(ctx);
		SRP_CmdIntel.WriteTo(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture miroir, APRÈS BuildRegions, les zones et les localités (régions retrouvées par leur code ;
	//! somme de contrôle différente : avertissement). RE13 : tickets ouverts rendus (ressources), colonnes et camions
	//! hors colonne en route rendus à leur source (manœuvres), rien d'autre — SRP_FrontEnemyComponent.ReadFrom
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		int version;
		if (!ctx.ReadValue("cmd_v", version))
		{
			// front.json d'avant le Commandeur (ou tout neuf) : tout part à neuf, le gel suit cmd_gel_au_depart
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Aucun état du Commandeur dans front.json : départ à neuf");
			return;
		}
		m_bStateRead = true;
		int frozen;
		if (ctx.ReadValue("cmd_gel", frozen))
			m_bFrozen = frozen != 0;
		int hash;
		if (ctx.ReadValue("cmd_hash", hash) && hash != m_Book.GetHash())
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Découpage des régions changé depuis la dernière sauvegarde (somme %1, maintenant %2) : les états sont repris par code de région", hash, m_Book.GetHash()));
		m_Book.ReadFrom(ctx);
		m_Resources.ReadFrom(ctx);
		m_Maneuvers.ReadFrom(ctx);
		m_Support.ReadFrom(ctx);
		SRP_CmdIntel.ReadFrom(ctx);
	}

	//================================================================================================
	// Accroches statiques (ne font rien si le Commandeur est absent)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! OF3 : un soldat du groupe perçoit un joueur (au plus une fois par passage de 10 s, par unité et par cible) :
	//! tampon du groupe (m_bCmdContact, m_bCmdSeen, m_aCmdPlayers, m_vCmdPos, m_bCmdVehicle, m_bCmdArmed) —
	//! SRP_EnemyComponent.AlertTick
	static void NoteSighting(SRP_EnemyGroup record, IEntity player, vector perceived, bool identified)
	{
		if (!record || !player || !s_Instance)
			return;
		// Un joueur vu l'emporte sur un joueur entendu (comme RaiseAlert) : la position d'un bruit ne remplace pas une vue
		if (identified || !record.m_bCmdSeen)
			record.m_vCmdPos = perceived;
		record.m_bCmdContact = true;
		if (identified)
			record.m_bCmdSeen = true;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(player);
		if (playerId > 0 && record.m_aCmdPlayers && record.m_aCmdPlayers.Find(playerId) < 0)
			record.m_aCmdPlayers.Insert(playerId);

		// À bord d'un véhicule ? Armé (une place de tourelle) seulement s'il est vu (AP5)
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(player.FindComponent(CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
			return;
		record.m_bCmdVehicle = true;
		if (!identified)
			return;
		BaseCompartmentSlot slot = access.GetCompartment();
		if (!slot)
			return;
		IEntity vehicle = slot.GetVehicle();
		if (vehicle && SRP_CmdSupport.IsArmedVehicle(vehicle))
			record.m_bCmdArmed = true;
	}

	//------------------------------------------------------------------------------------------------
	//! CO6 : un tir de joueur entendu par le groupe (m_iCmdShots, 500 au plus) — SRP_EnemyComponent (réaction au tir)
	static void NoteShot(SRP_EnemyGroup record)
	{
		if (!record || !s_Instance)
			return;
		record.m_iCmdShots = Math.MinInt(record.m_iCmdShots + 1, 500);
	}

	//------------------------------------------------------------------------------------------------
	//! AP7 : l'hélico voit des joueurs : compte rendu CONTACT vu, direct, flouté — SRP_EnemyComponent.ReportSpotted
	static void NoteAirSighting(vector position)
	{
		SRP_Commander commander = Get();
		if (commander)
			commander.AddAirReport(position);
	}

	//------------------------------------------------------------------------------------------------
	//! CO9 : menace LOCALE = min(10, islandThreat + min(alerte_menace_max, alerte de la région / alerte_menace_par)) ;
	//! sans Commandeur : islandThreat — SRP_TerritoryComponent.GarrisonSoldiers (Nominal), SRP_FrontEnemyComponent
	static int ThreatAt(vector position, int islandThreat)
	{
		SRP_Commander commander = Get();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!commander || !front || !front.IsReady())
			return islandThreat;
		int zone = front.GetZoneAt(position);
		if (zone < 0)
			return islandThreat;
		return commander.m_Book.ThreatAtZone(zone, islandThreat);
	}

	//------------------------------------------------------------------------------------------------
	//! OF5 : 1 + officier_bonus_garnison / 100 si l'officier de la région vit, sinon 1 — GarrisonSoldiers (Nominal)
	static float GarrisonFactorAt(vector position)
	{
		SRP_Commander commander = Get();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!commander || !front || !front.IsReady())
			return 1.0;
		int zone = front.GetZoneAt(position);
		if (zone < 0)
			return 1.0;
		return commander.m_Book.GarrisonFactorAtZone(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : une mission OFFICIER est possible (un officier VIVANT, non visé, avec une cachette du jour) —
	//! SRP_MissionManagerComponent (tirage des missions)
	static bool HasOfficerTarget()
	{
		SRP_Commander commander = Get();
		if (!commander || !commander.m_bStarted)
			return false;
		// Même porte que la mission (position indifférente : vector.Zero), qui écarte les officiers déjà visés
		int region;
		vector site;
		string label;
		return commander.m_Book.PickOfficerTarget(vector.Zero, region, site, label);
	}

	//------------------------------------------------------------------------------------------------
	//! CO9 : alerte de la région sous la position (m_Book.AddRegionAlert) — sabotages hors mission, SRP_Missions
	static void AddAlertAt(vector position, float amount, string reason)
	{
		SRP_Commander commander = Get();
		if (!commander || amount <= 0)
			return;
		int region = commander.m_Book.GetRegionAt(position);
		if (region >= 0)
			commander.m_Book.AddRegionAlert(region, amount, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! OF3 : retard radio d'une localité dont l'opérateur est tombé = cmd_radio_retard_min si le Commandeur existe,
	//! sinon fallback (une clé par chiffre : l'attribut m_iRadioDelayMinutes du territoire n'est plus que le repli) —
	//! SRP_TerritoryComponent.TryRadioCall
	static int RadioDelayMinutes(int fallback)
	{
		SRP_Commander commander = Get();
		if (!commander)
			return fallback;
		return commander.m_iRadioDelayMin;
	}

	//================================================================================================
	// Régions pour les écrans (relais de m_Book ; le reste de l'API des régions est sur SRP_CmdRegionBook)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Région qui tient ou tenait la zone (OF11 compris), -1 pour la base ou sans région — SRP_FrontScreens,
	//! SRP_FrontRadio, sous-modules
	int GetRegionOfZone(int zone)
	{
		return m_Book.GetRegionOfZone(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe » ou « région d'Entre-Deux-Monts » (VU1) — SRP_FrontRadio, SRP_FrontScreens
	string GetRegionLabel(int region)
	{
		return m_Book.GetRegionLabel(region);
	}

	//================================================================================================
	// Connaissance (CO5, CO6) : ce que l'ennemi SAIT, jamais la vraie position
	//================================================================================================
	// Les contacts qui ne viennent QUE d'informateurs (C4) ne sortent jamais par ces lectures : ils ne donnent qu'une
	// fouille (SearchCycle), ni appui, ni blindé, ni renfort. GetKnowledgeReport (Staff) les montre tous.
	//------------------------------------------------------------------------------------------------
	//! Contacts de la région (-1 = toute l'île) mis à jour depuis maxAgeSeconds au plus ; rend leur nombre —
	//! SRP_CmdArmor (ENGAGE), SRP_CmdScreens
	int GetContacts(int region, int maxAgeSeconds, notnull array<ref SRP_CmdContact> result)
	{
		result.Clear();
		int nowUnix = System.GetUnixTime();
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly)
				continue;
			if (nowUnix - contact.m_iLastUnix > maxAgeSeconds)
				continue;
			if (region >= 0 && contact.m_iRegion != region)
				continue;
			result.Insert(contact);
		}
		return result.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Meilleur contact d'une zone (vu d'abord si seenOnly, puis le plus nombreux, puis le plus récent) —
	//! SRP_CmdManeuvers (CA2, MA8), SRP_CmdSupport
	bool GetBestContact(int zone, bool seenOnly, int maxAgeSeconds, out vector position, out int players)
	{
		position = vector.Zero;
		players = 0;
		SRP_CmdContact best = BestContact(zone, seenOnly, maxAgeSeconds, System.GetUnixTime());
		if (!best)
			return false;
		position = best.m_vPos;
		players = best.m_iPlayers;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Dernier contact connu d'une zone et son heure Unix — SRP_CmdManeuvers (MA5, MA8)
	bool LastKnownContact(int zone, int maxAgeSeconds, out vector position, out int unixTime)
	{
		position = vector.Zero;
		unixTime = 0;
		int nowUnix = System.GetUnixTime();
		bool found = false;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || contact.m_iZone != zone)
				continue;
			if (nowUnix - contact.m_iLastUnix > maxAgeSeconds)
				continue;
			if (found && contact.m_iLastUnix <= unixTime)
				continue;
			found = true;
			position = contact.m_vPos;
			unixTime = contact.m_iLastUnix;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs ESTIMÉS connus à radius d'un point, contacts de moins de maxAgeSeconds — SRP_CmdManeuvers (MA8, CA1)
	int KnownPlayersNear(vector position, float radius, int maxAgeSeconds)
	{
		int nowUnix = System.GetUnixTime();
		int total = 0;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly)
				continue;
			if (nowUnix - contact.m_iLastUnix > maxAgeSeconds)
				continue;
			if (vector.DistanceXZ(contact.m_vPos, position) > radius)
				continue;
			total = total + contact.m_iPlayers;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs ESTIMÉS dans une zone (contacts récents) — gravité, SRP_CmdScreens
	int GetKnownPlayers(int zone)
	{
		int nowUnix = System.GetUnixTime();
		int total = 0;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || contact.m_iZone != zone)
				continue;
			if (nowUnix - contact.m_iLastUnix > m_iForgetS)
				continue;
			total = total + contact.m_iPlayers;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Contact de la zone confirmé (vu, ou 2 comptes rendus) depuis withinSeconds au plus — cycles, SRP_CmdManeuvers
	bool IsContactConfirmed(int zone, int withinSeconds)
	{
		int nowUnix = System.GetUnixTime();
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || contact.m_iZone != zone)
				continue;
			if (contact.m_bSeen && nowUnix - contact.m_iSeenUnix <= withinSeconds)
				return true;
			if (contact.m_iReports >= 2 && nowUnix - contact.m_iLastUnix <= withinSeconds)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! CO6 : appui lourd justifié = joueur VU depuis moins de cmd_contact_confirme_s, ou pertes depuis moins de
	//! cmd_pertes_appui_s — cycles, SRP_CmdSupport
	bool IsHeavySupportJustified(int zone)
	{
		int nowUnix = System.GetUnixTime();
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || contact.m_iZone != zone)
				continue;
			if (contact.m_bSeen && nowUnix - contact.m_iSeenUnix < m_iConfirmS)
				return true;
		}
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (!intel)
			return false;
		// Compteur par minute : la fenêtre des pertes est arrondie à la minute supérieure (60 min au plus)
		int minutes = (m_iLossJustifyS + 59) / 60;
		return intel.m_Losses.Sum(nowUnix, minutes) > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tirs entendus dans la zone sur les minutes récentes — gravité, SRP_CmdScreens
	int GetZoneShots(int zone, int minutes)
	{
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (!intel)
			return 0;
		return intel.m_Shots.Sum(System.GetUnixTime(), minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Pertes ennemies rapportées dans la zone sur les minutes récentes — gravité, SRP_CmdManeuvers (harcèlement)
	int GetZoneLosses(int zone, int minutes)
	{
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (!intel)
			return 0;
		return intel.m_Losses.Sum(System.GetUnixTime(), minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! AP5 riposte : véhicule armé vu dans la zone depuis withinSeconds — SlowCycle, SRP_CmdScreens
	bool WasArmedVehicleSeen(int zone, int withinSeconds, out vector position)
	{
		position = vector.Zero;
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (!intel || intel.m_iArmedSeenUnix <= 0)
			return false;
		if (System.GetUnixTime() - intel.m_iArmedSeenUnix > withinSeconds)
			return false;
		position = intel.m_vArmedPos;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! MO4 : un soldat ennemi vivant et conscient IDENTIFIE en ce moment (4 s au plus) un joueur à moins de radius :
	//! sa position dans playerPos (seul usage d'une position vraie : l'observateur la voit) — SRP_CmdSupport.FireNext
	bool IsPlayerWatchedNear(vector position, float radius, out vector playerPos)
	{
		playerPos = vector.Zero;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		PerceptionManager perception = GetGame().GetPerceptionManager();
		if (!enemies || !perception)
			return false;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		if (!groups)
			return false;
		float timeNow = perception.GetTime();
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(record.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
			if (!utility || !utility.m_Perception)
				continue;
			foreach (SCR_AITargetInfo target : utility.m_Perception.m_aTargets)
			{
				if (!target || !target.m_Entity || target.m_eCategory != EAITargetInfoCategory.IDENTIFIED)
					continue;
				if (timeNow - target.m_fTimestamp > 4)
					continue;
				if (SRP_Utils.GetPlayerIdFromEntity(target.m_Entity) <= 0)
					continue;
				vector seen = target.m_Entity.GetOrigin();
				if (vector.DistanceXZ(seen, position) > radius)
					continue;
				if (!HasFitMember(record))
					break;		// l'unité n'a plus personne en état d'observer
				playerPos = seen;
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! EQ4 SEULEMENT (dosage, jamais pour viser) : capture.CountValidPlayersNear(position, appui_rayon_joueurs_m) —
	//! SRP_CmdSupport, SRP_CmdArmor
	int GetDosingPlayers(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.GetCapture())
			return 0;
		float radius = 3000;
		if (m_Support)
			radius = m_Support.m_iPlayersM;
		return front.GetCapture().CountValidPlayersNear(position, radius);
	}

	//================================================================================================
	// Gravité (MA9, SEULE formule, trou 10)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Gravité de la zone : point clé menacé + carrés perdus + pertes + tirs + joueurs connus + contre-attaque en
	//! assaut (cmd_grav_*) — SRP_CmdManeuvers, SRP_CmdScreens
	float GetZoneGravity(int zone)
	{
		if (!m_bStarted)
			return 0;
		return ComputeGravity(zone, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Zone servie par les moyens (les cmd_grav_zones_servies plus graves ; la contre-attaque l'est toujours) —
	//! cycles, SRP_CmdSupport, SRP_CmdManeuvers
	bool IsServedZone(int zone)
	{
		if (!m_bStarted)
			return true;
		if (zone >= 0 && zone == CounterAttackZone())
			return true;
		return m_aServedZones.Contains(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Zones au contact triées par gravité décroissante ; rend leur nombre — FastCycle, SRP_CmdScreens
	int GetZonesByGravity(notnull array<int> zones)
	{
		zones.Clear();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!m_bStarted || !front || !front.IsReady())
			return 0;
		int nowUnix = System.GetUnixTime();
		array<float> values = {};
		int count = Math.MinInt(front.GetZoneCount(), m_aIntel.Count());
		for (int zone = 1; zone < count; zone++)
		{
			float gravity = ComputeGravity(zone, nowUnix);
			if (gravity <= 0)
				continue;
			int at = 0;
			while (at < values.Count() && values[at] >= gravity)
			{
				at++;
			}
			values.InsertAt(gravity, at);
			zones.InsertAt(zone, at);
		}
		return zones.Count();
	}

	//================================================================================================
	// Rapports Staff (textes du serveur, jamais publics)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Secondes avant le prochain cycle rapide (0 = gelé ou pas prêt) — SRP_CmdScreens (« Rythme »)
	int GetNextFastSeconds()
	{
		if (!m_bStarted || m_bFrozen)
			return 0;
		return Math.MaxInt(0, m_iNextFast - System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes avant le prochain cycle lent — SRP_CmdScreens
	int GetNextSlowSeconds()
	{
		if (!m_bStarted || m_bFrozen)
			return 0;
		return Math.MaxInt(0, m_iNextSlow - System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Contacts connus autour d'un point (position floutée, joueurs estimés, âge, vu ou entendu, source) — Staff
	string GetKnowledgeReport(vector from, float radius)
	{
		int nowUnix = System.GetUnixTime();
		string text = "";
		int shown = 0;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact)
				continue;
			float distance = vector.DistanceXZ(contact.m_vPos, from);
			if (radius > 0 && distance > radius)
				continue;
			string how = "entendus";
			if (contact.m_bSeen)
				how = string.Format("vus il y a %1", SRP_CmdScreens.DurationText(nowUnix - contact.m_iSeenUnix));
			string origin = "unités";
			if (contact.m_bInformantOnly)
				origin = "civil seulement (fouille, jamais d'appui)";
			int meters = Math.Round(distance);
			string line = string.Format("Carré %1, %2 · %3 joueur(s) estimé(s), %4 · dernier compte rendu il y a %5 (%6 en tout) · source : %7 · à %8 m", SRP_CmdScreens.CellText(contact.m_vPos), ZoneText(contact.m_iZone), contact.m_iPlayers, how, SRP_CmdScreens.DurationText(nowUnix - contact.m_iLastUnix), contact.m_iReports, origin, meters);
			if (contact.m_bArmed)
				line = line + " · véhicule armé";
			else if (contact.m_bVehicle)
				line = line + " · en véhicule";
			if (shown > 0)
				text = text + "\n";
			text = text + line;
			shown++;
		}
		if (shown == 0)
			return "Aucun contact connu du Commandeur ici";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Résumé Staff : gel, cycles, comptes rendus perdus ou retardés, contacts, humeur de l'île — SRP_CmdScreens
	string GetStaffReport()
	{
		if (!m_bStarted)
			return "Commandeur pas encore démarré (front en préparation)";
		string state = "actif";
		string rhythm;
		if (m_bFrozen)
		{
			state = "GELÉ (aucune décision, aucun appui)";
			rhythm = "cycles arrêtés";
		}
		else
			rhythm = string.Format("cycle rapide dans %1, cycle lent dans %2", SRP_CmdScreens.DurationText(GetNextFastSeconds()), SRP_CmdScreens.DurationText(GetNextSlowSeconds()));

		int lost = 0;
		int delayed = 0;
		int regions = m_Book.GetRegionCount();
		for (int region = 0; region < regions; region++)
		{
			SRP_CmdRegion item = m_Book.GetRegion(region);
			if (!item)
				continue;
			lost = lost + item.m_iReportsLost;
			delayed = delayed + item.m_iReportsDelayed;
		}
		int players = 0;
		int informants = 0;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact)
				continue;
			players = players + contact.m_iPlayers;
			if (contact.m_bInformantOnly)
				informants++;
		}
		string served = "";
		foreach (int zone : m_aServedZones)
		{
			if (!served.IsEmpty())
				served = served + ", ";
			served = served + ZoneText(zone);
		}
		if (served.IsEmpty())
			served = "aucune";

		string part1 = string.Format("Commandeur %1 · %2", state, rhythm);
		string part2 = string.Format("comptes rendus en route : %1 · perdus : %2 (désorganisation) · retardés : %3 (radio)", m_aPending.Count(), lost, delayed);
		string part3 = string.Format("contacts connus : %1 dont %2 de civils (%3 joueur(s) estimé(s)) · ordres en attente : %4", m_aContacts.Count(), informants, players, m_aReflexQueue.Count());
		string part4 = string.Format("zones servies : %1 · humeur de l'île : %2", served, SRP_CmdScreens.MoodWord(MoodOf(m_fIslandMood)));
		return part1 + " · " + part2 + " · " + part3 + " · " + part4;
	}

	//================================================================================================
	// Interne : comptes rendus, connaissance, informateurs, réflexes, cycles
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Version du front changée : un compte rendu CARRE_PERDU (rouge -> bleu) ou CARRE_REPRIS (bleu -> rouge) par
	//! carré changé depuis m_aCellSeen, avec le retard OF3 — Tick
	protected void DiffCells(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		int count = front.GetCellCount();
		if (m_aCellSeen.Count() != count)
		{
			CopyCells(front);
			return;
		}
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		// Bloc de localité pris d'un coup (choix de Jack du 27/09) : un seul compte rendu par zone et par passage
		SRP_FrontCapture capture = front.GetCapture();
		array<int> blockZones = {};
		for (int cell = 0; cell < count; cell++)
		{
			int owner = front.GetCellOwner(cell);
			int before = m_aCellSeen[cell];
			if (owner == before)
				continue;
			m_aCellSeen[cell] = owner;
			if (before < 0 || owner < 0)
				continue;
			int zone = front.GetCellZone(cell);
			if (zone < 0 || front.IsBaseZone(zone))
				continue;
			int region = m_Book.GetRegionOfZone(zone);
			if (region < 0)
				continue;
			// Repeinte d'une zone entière qui vient de changer de camp (chute D1, isolement E4, offensive H2, correction
			// du Staff Q9) : la zone a son propre compte rendu (alerte_zone) ou c'est une correction sans effet ; ses
			// carrés repeints ne sont pas des carrés perdus ou repris un à un (remise à zéro J2 : déjà recopiés par
			// OnZoneReset)
			if (IsZoneRepaint(front, zone, owner, nowUnix))
				continue;
			if (owner == SRP_EFrontOwner.BLEU && capture && capture.IsBlockCell(cell))
			{
				if (blockZones.Contains(zone))
					continue;
				blockZones.Insert(zone);
			}

			SRP_CmdReport report = new SRP_CmdReport();
			if (owner == SRP_EFrontOwner.BLEU)
				report.m_iKind = SRP_ECmdReport.CARRE_PERDU;
			else
				report.m_iKind = SRP_ECmdReport.CARRE_REPRIS;
			report.m_iRegion = region;
			report.m_iZone = zone;
			report.m_iCell = cell;
			report.m_vPos = front.CellCenter(cell);
			report.m_sSource = MainLocalityName(zone);
			// Carré d'une localité tenue : la nouvelle passe par la radio de la localité (OF3)
			if (host && host.IsInGarrisonedLocality(report.m_vPos, 300))
			{
				report.m_bNeedsRadio = true;
				report.m_vRadioPos = report.m_vPos;
			}
			report.m_iDueUnix = nowUnix + RandomBetween(m_iReportMinS, m_iReportMaxS);
			m_aPending.Insert(report);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré passé à owner appartient à une zone qui vient elle-même de passer à owner (SetZoneOwner il y a 60 s au
	//! plus : m_iCapturedAt pour le bleu, m_iLostAt pour le rouge) : c'est la repeinte de la zone, pas un carré pris
	//! ou repris — DiffCells
	protected bool IsZoneRepaint(SRP_FrontComponent front, int zone, int owner, int nowUnix)
	{
		if (front.GetZoneOwner(zone) != owner)
			return false;
		SRP_FrontZone zoneData = front.GetZone(zone);
		if (!zoneData)
			return false;
		int changedAt = zoneData.m_iLostAt;
		if (owner == SRP_EFrontOwner.BLEU)
			changedAt = zoneData.m_iCapturedAt;
		return changedAt > 0 && nowUnix - changedAt <= 60;
	}

	//------------------------------------------------------------------------------------------------
	//! OF3 : chaque groupe vivant rend compte toutes les cmd_rapport_min_s à cmd_rapport_max_s (tampon vidé) ;
	//! région désorganisée : réflexe local, puis compte rendu perdu à la livraison — Tick
	protected void CollectGroupReports(int nowUnix)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!enemies || !front)
			return;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		if (!groups)
			return;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			// Première échéance tirée à la première vue de l'unité, puis retirée au sort à chaque compte rendu
			if (record.m_iCmdNextReport <= 0)
			{
				record.m_iCmdNextReport = nowUnix + RandomBetween(m_iReportMinS, m_iReportMaxS);
				continue;
			}
			if (nowUnix < record.m_iCmdNextReport)
				continue;
			record.m_iCmdNextReport = nowUnix + RandomBetween(m_iReportMinS, m_iReportMaxS);
			if (!record.m_bCmdContact && record.m_iCmdShots <= 0)
				continue;		// rien vu, rien entendu : rien à dire

			SRP_CmdReport report = BuildGroupReport(enemies, front, record, nowUnix);
			ClearBuffer(record);
			if (!report)
				continue;
			// OF6 : région désorganisée, l'unité se débrouille (réflexe local) ; son compte rendu sera perdu en route
			if (!report.m_bDirect && report.m_iPlayers > 0 && m_Book.GetRegionState(report.m_iRegion) == SRP_ECmdRegionState.DESORGANISEE)
				OfficerReflex(report, null, nowUnix);
			m_aPending.Insert(report);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Livre les comptes rendus échus : région absente ou non ACTIVE (sauf direct) -> perdu ; radio de la localité
	//! coupée (SRP_TerritoryComponent.TryRadioCall) -> retardé ; informateur mort, inconscient ou menotté -> perdu ;
	//! sinon Integrate — Tick
	protected void ProcessPending(int nowUnix)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		for (int i = m_aPending.Count() - 1; i >= 0; i--)
		{
			SRP_CmdReport report = m_aPending[i];
			if (!report)
			{
				m_aPending.Remove(i);
				continue;
			}
			if (report.m_iDueUnix > nowUnix)
				continue;
			if (report.m_iRegion < 0)
			{
				m_aPending.Remove(i);
				continue;
			}
			// C4 : le civil doit pouvoir passer son appel (retiré par la mise en veille de sa localité : l'appel est parti)
			if (report.m_iKind == SRP_ECmdReport.INFORMATEUR && report.m_Informant && !IsFit(report.m_Informant))
			{
				SRP_CmdLog.Note(report.m_iRegion, SRP_ECmdLogKind.RENSEIGNEMENT, string.Format("Signalement d'un civil perdu vers %1 : il n'a pas pu passer son appel (mort, inconscient ou menotté)", SRP_CmdScreens.CellText(report.m_vPos)));
				m_aPending.Remove(i);
				continue;
			}
			// OF6 : une région désorganisée ou libérée ne transmet plus rien (sauf hélico et vagues, qui parlent droit) ;
			// seul un compte rendu perdu par désorganisation est compté pour le Staff
			int state = m_Book.GetRegionState(report.m_iRegion);
			if (!report.m_bDirect && state != SRP_ECmdRegionState.ACTIVE)
			{
				if (state == SRP_ECmdRegionState.DESORGANISEE)
					m_Book.NoteReportTrouble(report.m_iRegion, true, report.m_sSource, 0);
				m_aPending.Remove(i);
				continue;
			}
			// OF3 : l'opérateur radio de la localité est tombé : tout attend la fin du retard (partagé avec les camions)
			if (report.m_bNeedsRadio && territory && !territory.TryRadioCall(report.m_vRadioPos, System.GetTickCount()))
			{
				if (!report.m_bRadioLogged)
				{
					report.m_bRadioLogged = true;
					SRP_CmdRegion item = m_Book.GetRegion(report.m_iRegion);
					bool known = false;
					if (item && item.m_sRadioCutLocality == report.m_sSource && item.m_iRadioCutUntil > nowUnix)
						known = true;
					m_Book.NoteReportTrouble(report.m_iRegion, false, report.m_sSource, nowUnix + m_iRadioDelayMin * 60);
					if (!known)
						SRP_CmdLog.Note(report.m_iRegion, SRP_ECmdLogKind.RENSEIGNEMENT, string.Format("Comptes rendus de %1 retardés : opérateur radio hors de combat (%2 min)", report.m_sSource, m_iRadioDelayMin));
				}
				continue;
			}
			Integrate(report, nowUnix);
			m_aPending.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Intègre un compte rendu : connaissance (MergeContact), anneaux de la zone, alerte de la région, puis
	//! OfficerReflex ; une ligne SRP_CmdLog par contact neuf ou passé de « entendu » à « vu » — ProcessPending
	protected void Integrate(SRP_CmdReport report, int nowUnix)
	{
		SRP_CmdZoneIntel intel = IntelOf(report.m_iZone);
		if (!intel)
			return;
		int region = report.m_iRegion;
		intel.m_iLastReportUnix = nowUnix;
		SRP_CmdRegion item = m_Book.GetRegion(region);
		if (item)
			item.m_iLastReportUnix = nowUnix;

		if (report.m_iKind == SRP_ECmdReport.CONTACT)
		{
			SRP_CmdContact contact = null;
			float alert = 0;
			if (report.m_iPlayers > 0)
			{
				contact = MergeContact(report, nowUnix);
				intel.m_iLastContactUnix = nowUnix;
				if (report.m_bSeen)
				{
					intel.m_iLastSeenUnix = nowUnix;
					alert = alert + m_Book.m_iAlertSeen;
				}
				else
					alert = alert + m_Book.m_iAlertHeard;
				m_Maneuvers.NoteContact(region, report.m_iZone, report.m_vPos, nowUnix);
			}
			if (report.m_iShots > 0)
			{
				intel.m_Shots.Add(nowUnix, report.m_iShots);
				alert = alert + report.m_iShots / Math.Max(1.0, m_Book.m_iAlertShotsPer);
			}
			if (report.m_bArmed && report.m_bSeen)
			{
				intel.m_iArmedSeenUnix = nowUnix;
				intel.m_vArmedPos = report.m_vPos;
				alert = alert + m_Book.m_iAlertVehicle;
			}
			if (alert > 0)
				m_Book.AddRegionAlert(region, alert, "compte rendu de " + report.m_sSource);
			if (contact && (m_bMergeNew || m_bMergeUpgraded))
				LogContact(report, contact);
			if (contact)
				OfficerReflex(report, contact, nowUnix);
			return;
		}
		if (report.m_iKind == SRP_ECmdReport.PERTE)
		{
			intel.m_Losses.Add(nowUnix, 1);
			m_Book.AddRegionAlert(region, m_Book.m_iAlertLoss, "soldat perdu");
			m_Maneuvers.OnLossReported(report.m_iZone, report.m_vPos);
			return;
		}
		if (report.m_iKind == SRP_ECmdReport.CARRE_PERDU)
		{
			intel.m_CellsLost.Add(nowUnix, 1);
			m_Book.AddRegionAlert(region, m_Book.m_iAlertCell, "carré perdu");
			return;
		}
		if (report.m_iKind == SRP_ECmdReport.CARRE_REPRIS)
		{
			intel.m_CellsRetaken.Add(nowUnix, 1);
			return;
		}
		if (report.m_iKind == SRP_ECmdReport.INFORMATEUR)
		{
			SRP_CmdContact informed = MergeContact(report, nowUnix);
			intel.m_iLastInformantUnix = nowUnix;
			m_Book.AddRegionAlert(region, m_Book.m_iAlertInformant, "signalement d'un civil");
			if (informed && m_bMergeNew)
				LogContact(report, informed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fusionne dans le contact connu le plus proche à cmd_fusion_contacts_m, sinon en crée un ; rend le contact.
	//! C4 : un signalement de civil n'enrichit qu'un contact « civil seul » ; une unité reprend à neuf un contact
	//! « civil seul » — Integrate
	protected SRP_CmdContact MergeContact(SRP_CmdReport report, int nowUnix)
	{
		m_bMergeNew = false;
		m_bMergeUpgraded = false;
		bool informant = report.m_iKind == SRP_ECmdReport.INFORMATEUR;
		int players = Math.MaxInt(1, report.m_iPlayers);

		SRP_CmdContact best = null;
		float bestDistance = m_iMergeM;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || nowUnix - contact.m_iLastUnix > m_iForgetS)
				continue;
			float distance = vector.DistanceXZ(contact.m_vPos, report.m_vPos);
			if (distance > bestDistance)
				continue;
			bestDistance = distance;
			best = contact;
		}

		if (!best)
		{
			SRP_CmdContact created = new SRP_CmdContact();
			FillContact(created, report, players, nowUnix);
			m_aContacts.Insert(created);
			m_bMergeNew = true;
			return created;
		}

		// C4 : un signalement de civil ne déplace, ne grossit, ne rafraîchit ni ne confirme un contact rapporté par nos
		// unités (sa position, son nombre et son heure servent à viser et à confirmer) : il est absorbé sans effet
		if (informant && !best.m_bInformantOnly)
			return best;
		// C4 : le premier compte rendu d'une unité sur un contact « civil seul » le remplace entièrement : rien de ce
		// qu'a dit le civil (position, nombre, heure de départ, compte des comptes rendus) ne sert ensuite à un appui
		if (!informant && best.m_bInformantOnly)
		{
			FillContact(best, report, players, nowUnix);
			m_bMergeNew = true;
			return best;
		}

		// Même groupe de joueurs : l'estimation garde le plus grand nombre tant que l'ancien relevé est frais (2 min)
		if (nowUnix - best.m_iLastUnix < 120)
			best.m_iPlayers = Math.MaxInt(best.m_iPlayers, players);
		else
			best.m_iPlayers = players;
		// AP2 : première position gardée tant que les joueurs n'ont pas bougé. Seuls les comptes rendus VUS la font
		// repartir : un bruit (flou de 100 à 150 m) relancerait le compte des 10 min face à des joueurs immobiles. Les
		// positions vues sont floutées (CO5) : la tolérance ajoute l'écart d'une vue (cmd_flou_vu_max_m) au seuil
		// cmd_artillerie_immobile_m, sinon le flou seul ferait croire à un déplacement.
		if (report.m_bSeen && vector.DistanceXZ(best.m_vFirstPos, report.m_vPos) > m_iArtilleryStillM + m_iBlurSeenMaxM)
		{
			best.m_vFirstPos = report.m_vPos;
			best.m_iFirstUnix = nowUnix;
		}
		best.m_vPos = report.m_vPos;
		best.m_iZone = report.m_iZone;
		best.m_iCell = report.m_iCell;
		best.m_iRegion = report.m_iRegion;
		if (report.m_bSeen)
		{
			if (!best.m_bSeen)
				m_bMergeUpgraded = true;
			best.m_bSeen = true;
			best.m_iSeenUnix = nowUnix;
		}
		best.m_bArmed = best.m_bArmed || report.m_bArmed;
		best.m_bVehicle = best.m_bVehicle || report.m_bVehicle;
		best.m_iLastUnix = nowUnix;
		best.m_iReports = best.m_iReports + 1;
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Contact rempli à neuf d'après un seul compte rendu (1 compte rendu, première position et heure = celles-ci) —
	//! MergeContact (contact neuf, ou contact « civil seul » repris par une unité)
	protected void FillContact(SRP_CmdContact contact, SRP_CmdReport report, int players, int nowUnix)
	{
		contact.m_vPos = report.m_vPos;
		contact.m_vFirstPos = report.m_vPos;
		contact.m_iZone = report.m_iZone;
		contact.m_iCell = report.m_iCell;
		contact.m_iRegion = report.m_iRegion;
		contact.m_iPlayers = players;
		contact.m_bSeen = report.m_bSeen;
		contact.m_bInformantOnly = report.m_iKind == SRP_ECmdReport.INFORMATEUR;
		contact.m_bArmed = report.m_bArmed;
		contact.m_bVehicle = report.m_bVehicle;
		contact.m_iFirstUnix = nowUnix;
		contact.m_iLastUnix = nowUnix;
		contact.m_iSeenUnix = 0;
		if (report.m_bSeen)
			contact.m_iSeenUnix = nowUnix;
		contact.m_iReports = 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Oublie les contacts sans compte rendu depuis cmd_oubli_s — Tick
	protected void ExpireKnowledge(int nowUnix)
	{
		for (int i = m_aContacts.Count() - 1; i >= 0; i--)
		{
			if (!m_aContacts[i] || nowUnix - m_aContacts[i].m_iLastUnix > m_iForgetS)
				m_aContacts.Remove(i);
		}
		// C4 : un civil qui a tiré est oublié à la fin de son repos (tableaux parallèles, retirés ensemble)
		for (int k = m_aInformRolled.Count() - 1; k >= 0; k--)
		{
			if (k >= m_aInformRolledAt.Count() || !m_aInformRolled[k] || nowUnix - m_aInformRolledAt[k] > m_iInfoCivilRestS)
			{
				m_aInformRolled.Remove(k);
				if (k < m_aInformRolledAt.Count())
					m_aInformRolledAt.Remove(k);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! C4 : civils de SRP_CivilianManagerComponent.GetInformantCandidates en zone rouge d'une région ACTIVE, joueur
	//! valide (capture.IsCharacterCounted) sur un carré rouge à info_portee_m en vue, repos du civil et de la zone,
	//! tirage info_chance -> compte rendu INFORMATEUR retardé de info_delai_min_s à info_delai_max_s — Tick
	protected void InformantsTick(int nowUnix)
	{
		if (!m_bInformants)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CivilianManagerComponent civilians = SRP_CivilianManagerComponent.GetInstance();
		if (!front || !front.GetCapture() || !civilians)
			return;
		SRP_FrontCapture capture = front.GetCapture();

		// Joueurs qui comptent (C2, C3 : vivants, conscients, hors de la base, pas exclus), eux aussi sur un carré rouge
		array<IEntity> targets = {};
		array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
		foreach (IEntity player : everyone)
		{
			if (!player || !capture.IsCharacterCounted(player))
				continue;
			if (!front.IsRedAt(player.GetOrigin()))
				continue;
			targets.Insert(player);
		}
		if (targets.IsEmpty())
			return;

		array<IEntity> candidates = {};
		civilians.GetInformantCandidates(candidates);
		foreach (IEntity civilian : candidates)
		{
			if (!IsFit(civilian))
				continue;
			vector civilianPos = civilian.GetOrigin();
			if (!front.IsRedAt(civilianPos))
				continue;
			int zone = front.GetZoneAt(civilianPos);
			SRP_CmdZoneIntel intel = IntelOf(zone);
			if (!intel)
				continue;
			int region = m_Book.GetRegionOfZone(zone);
			if (region < 0 || m_Book.GetRegionState(region) != SRP_ECmdRegionState.ACTIVE || !m_Book.HasLivingOfficer(region))
				continue;
			if (intel.m_iLastInformantUnix > 0 && nowUnix - intel.m_iLastInformantUnix < m_iInfoZoneRestS)
				continue;
			int rolled = m_aInformRolled.Find(civilian);
			if (rolled >= 0 && rolled < m_aInformRolledAt.Count() && nowUnix - m_aInformRolledAt[rolled] < m_iInfoCivilRestS)
				continue;

			// Le joueur valide le plus proche, en vue du civil
			IEntity spotted = null;
			float nearest = m_iInfoRangeM;
			foreach (IEntity target : targets)
			{
				float distance = vector.Distance(target.GetOrigin(), civilianPos);
				if (distance > nearest)
					continue;
				if (!SRP_Placement.HasLineOfSight(civilianPos, target.GetOrigin(), civilian))
					continue;
				nearest = distance;
				spotted = target;
			}
			if (!spotted)
				continue;

			// Le tirage est noté (repos du civil), réussi ou non
			if (rolled >= 0 && rolled < m_aInformRolledAt.Count())
				m_aInformRolledAt[rolled] = nowUnix;
			else
			{
				m_aInformRolled.Insert(civilian);
				m_aInformRolledAt.Insert(nowUnix);
			}
			if (Math.RandomInt(0, 100) >= m_iInfoChance)
				continue;

			vector seenPos = spotted.GetOrigin();
			SRP_CmdReport report = new SRP_CmdReport();
			report.m_iKind = SRP_ECmdReport.INFORMATEUR;
			report.m_vPos = Blur(seenPos, m_iInfoBlurMinM, m_iInfoBlurMaxM);
			int reportZone = front.GetZoneAt(report.m_vPos);
			if (reportZone < 0 || front.IsBaseZone(reportZone))
				reportZone = zone;
			report.m_iZone = reportZone;
			report.m_iRegion = m_Book.GetRegionOfZone(reportZone);
			if (report.m_iRegion < 0)
				report.m_iRegion = region;
			report.m_iCell = front.CellIndexAt(report.m_vPos);
			report.m_iPlayers = Math.MaxInt(1, capture.CountValidPlayersNear(seenPos, m_iInfoCountM));
			report.m_bSeen = false;		// un civil n'est pas un soldat : jamais « vu » au sens de CO6
			CompartmentAccessComponent access = CompartmentAccessComponent.Cast(spotted.FindComponent(CompartmentAccessComponent));
			if (access && access.IsInCompartment())
			{
				report.m_bVehicle = true;
				BaseCompartmentSlot slot = access.GetCompartment();
				IEntity vehicle = null;
				if (slot)
					vehicle = slot.GetVehicle();
				if (vehicle && SRP_CmdSupport.IsArmedVehicle(vehicle))
					report.m_bArmed = true;
			}
			report.m_sSource = "civil";
			report.m_Informant = civilian;
			report.m_iDueUnix = nowUnix + RandomBetween(m_iInfoDelayMinS, m_iInfoDelayMaxS);
			m_aPending.Insert(report);
			intel.m_iLastInformantUnix = nowUnix;		// une zone ne donne qu'un signalement par info_zone_repos_s

			SRP_CmdLog.Note(region, SRP_ECmdLogKind.RENSEIGNEMENT, string.Format("Un civil a vu des soldats vers %1 (%2) : signalement dans %3, sauf s'il en est empêché", SRP_CmdScreens.CellText(report.m_vPos), ZoneText(report.m_iZone), SRP_CmdScreens.DurationText(report.m_iDueUnix - nowUnix)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! OF4, MA1 (dès le premier contact) : contact en zone ROUGE, région ACTIVE ou désorganisée (réflexe local),
	//! m_Maneuvers.CanReinforceZone (frein C2) -> ordre RENFORT de taille ReinforcementSize, officier lent :
	//! m_iNotBefore = now + cmd_trait_lent_retard_s (SEUL endroit du retard, trou 12) — Integrate
	protected void OfficerReflex(SRP_CmdReport report, SRP_CmdContact contact, int nowUnix)
	{
		if (m_bFrozen || !report)
			return;
		if (report.m_iKind != SRP_ECmdReport.CONTACT)
			return;		// C4 : un signalement de civil ne donne qu'une fouille
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		int zone = report.m_iZone;
		int region = report.m_iRegion;
		if (zone < 0 || region < 0 || front.IsBaseZone(zone) || front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE)
			return;
		int state = m_Book.GetRegionState(region);
		if (state == SRP_ECmdRegionState.LIBEREE)
			return;
		if (!m_Maneuvers.CanReinforceZone(zone))
			return;		// C2 : un renfort par zone toutes les renfort_frein_min
		foreach (SRP_CmdOrder queued : m_aReflexQueue)
		{
			if (queued && queued.m_iKind == SRP_ECmdOrder.RENFORT && queued.m_iZone == zone)
				return;		// déjà demandé, en attente
		}

		vector aim = report.m_vPos;
		int players = Math.MaxInt(1, report.m_iPlayers);
		bool seen = report.m_bSeen;
		if (contact)
		{
			aim = contact.m_vPos;
			players = Math.MaxInt(1, contact.m_iPlayers);
			seen = contact.m_bSeen;
		}
		string how = "entendu(s)";
		if (seen)
			how = "vu(s)";

		SRP_CmdOrder order = NewOrder(SRP_ECmdOrder.RENFORT, region, zone, aim, "", nowUnix);
		if (state == SRP_ECmdRegionState.DESORGANISEE)
		{
			// OF6 : réflexe local, sans caractère ni humeur, taille de base ; la lenteur (x2) est appliquée par les manœuvres
			order.m_iSize = m_iReinforceBase;
			order.m_fDelayFactor = m_Maneuvers.m_fDesorgFactor;
			order.m_sReason = string.Format("réflexe local de l'unité (région désorganisée) : %1 joueur(s) %2 vers %3", players, how, SRP_CmdScreens.CellText(aim));
		}
		else
		{
			order.m_iSize = ReinforcementSize(zone, players, nowUnix);
			order.m_sReason = string.Format("contact : %1 joueur(s) %2 vers %3", players, how, SRP_CmdScreens.CellText(aim));
			if (m_Book.GetOfficerTrait(region) == SRP_ECmdTrait.LENT)
			{
				order.m_iNotBefore = nowUnix + m_iSlowTraitDelayS;
				order.m_sReason = order.m_sReason + string.Format(" (officier lent : ordre retardé de %1 s)", m_iSlowTraitDelayS);
			}
		}
		m_aReflexQueue.Insert(order);
	}

	//------------------------------------------------------------------------------------------------
	//! MA1 sur les joueurs ESTIMÉS : renfort_taille_base + renfort_taille_par_joueur x joueurs, renfort_taille_plein si
	//! combat intense (cmd_tirs_intenses_3min), pertes lourdes (renfort_plein_pertes soldats tombés dans la zone en
	//! renfort_plein_pertes_min) ou alerte >= alerte_renfort_plein ; x caractère (prudent, audacieux) et humeur
	//! (centièmes) — OfficerReflex, ForceOrder
	protected int ReinforcementSize(int zone, int players, int nowUnix)
	{
		// 4 soldats, plus 2 par joueur estimé au-delà du premier
		int size = m_iReinforceBase + m_iReinforcePerPlayer * Math.MaxInt(0, players - 1);
		bool full = false;
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (intel && intel.m_Shots.Sum(nowUnix, 3) >= m_iIntenseShots)
			full = true;
		// Plan MA1 : 3 soldats tombés en 10 min (pertes rapportées au Commandeur, OF3)
		if (intel && intel.m_Losses.Sum(nowUnix, m_iFullLossesMin) >= m_iFullLosses)
			full = true;
		int region = m_Book.GetRegionOfZone(zone);
		if (region >= 0 && m_Book.GetRegionAlert(region) >= m_iAlertFullReinforce)
			full = true;
		if (full)
			size = Math.MaxInt(size, m_iReinforceFull);

		float factor = 1;
		if (region >= 0)
		{
			int trait = m_Book.GetOfficerTrait(region);
			if (trait == SRP_ECmdTrait.PRUDENT)
				factor = factor * m_iPrudentSizeCent / 100.0;
			else if (trait == SRP_ECmdTrait.AUDACIEUX)
				factor = factor * m_iBoldSizeCent / 100.0;
			int mood = m_Book.GetRegionMood(region);
			if (mood == SRP_ECmdMood.PRUDENTE)
				factor = factor * m_iMoodPrudentSizeCent / 100.0;
			else if (mood == SRP_ECmdMood.AGRESSIVE)
				factor = factor * m_iMoodAggressiveSizeCent / 100.0;
		}
		int result = Math.Round(size * factor);
		// Borné entre la taille de base et le renfort plein : les manœuvres fixent ensuite l'effectif réel
		int low = Math.MinInt(m_iReinforceBase, m_iReinforceFull);
		int high = Math.MaxInt(m_iReinforceBase, m_iReinforceFull);
		return Math.ClampInt(result, low, high);
	}

	//------------------------------------------------------------------------------------------------
	//! CO4 cycle rapide : humeurs, gravité, réflexes échus, mortier (zones servies, contact VU depuis cmd_contact_vise_s,
	//! appui justifié, repos de zone, prudence), hélico (AP7 : contact VU en zone rouge, aucune unité à
	//! cmd_helico_sans_troupe_m), fouilles des contacts « informateur seul » (C4 : ENQUETE par une unité mobile, ou
	//! HELICO si aucune unité n'est à cmd_helico_sans_troupe_m ; JAMAIS mortier, artillerie, blindé ni bombe) — Tick
	protected void FastCycle(int nowUnix)
	{
		RecomputeMoods(nowUnix);
		RecomputeGravity(nowUnix);
		RunDueReflexes(nowUnix);
		MortarCycle(nowUnix);
		bool heliSent = HeliCycle(nowUnix);
		SearchCycle(nowUnix, heliSent);
	}

	//------------------------------------------------------------------------------------------------
	//! CO4 cycle lent : blindé (AP5 riposte ou gravité), artillerie lourde (AP2 : contact immobile d'au moins
	//! cmd_artillerie_joueurs), bombe (AP3 : cmd_bombe_joueurs) ; la contre-attaque n'est PAS ici (CaWindowDecision,
	//! chaque minute, trou 19) — Tick
	protected void SlowCycle(int nowUnix)
	{
		// Chiffres frais pour les deux décisions lourdes (le cycle rapide les recalcule aussi)
		RecomputeGravity(nowUnix);
		ArmorCycle(nowUnix);
		HeavyFireCycle(nowUnix);
	}

	//------------------------------------------------------------------------------------------------
	//! CO7 : humeur de l'île (stocks lourds, zones reprises ou perdues en 24 h) puis de chaque région (pertes, obus,
	//! reprises, caractère) -> m_Book.SetRegionMood — FastCycle
	protected void RecomputeMoods(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;

		// Île : moyens lourds (blindés, hélico, artillerie, bombe) et bilan des zones sur 24 h
		float island = 0;
		float heavy = m_Resources.GetFillRatio(-1) * 100.0;
		if (heavy >= m_iMoodStockHighCent)
			island = island + 0.5;
		else if (heavy <= m_iMoodStockLowCent)
			island = island - 0.5;
		int retaken = 0;
		int lost = 0;
		int zoneCount = front.GetZoneCount();
		for (int zone = 0; zone < zoneCount; zone++)
		{
			SRP_FrontZone data = front.GetZone(zone);
			if (!data || data.m_bBase)
				continue;
			if (data.m_iLostAt > 0 && nowUnix - data.m_iLostAt <= 86400)
				retaken++;		// perdue par nous = reprise par l'ennemi
			if (data.m_iCapturedAt > 0 && nowUnix - data.m_iCapturedAt <= 86400)
				lost++;
		}
		if (retaken > lost)
			island = island + 0.5;
		else if (lost - retaken >= 2)
			island = island - 0.5;
		m_fIslandMood = island;

		// Régions : l'île, puis pertes, obus, carrés repris et caractère de l'officier
		array<int> zones = {};
		int regions = m_Book.GetRegionCount();
		for (int region = 0; region < regions; region++)
		{
			if (m_Book.GetRegionState(region) == SRP_ECmdRegionState.LIBEREE)
			{
				m_Book.SetRegionMood(region, SRP_ECmdMood.NORMALE);
				continue;
			}
			float mood = island;
			int losses = 0;
			int retakenCells = 0;
			zones.Clear();
			m_Book.GetRegionZones(region, zones);
			foreach (int regionZone : zones)
			{
				SRP_CmdZoneIntel intel = IntelOf(regionZone);
				if (!intel)
					continue;
				losses = losses + intel.m_Losses.Sum(nowUnix, 60);
				retakenCells = retakenCells + intel.m_CellsRetaken.Sum(nowUnix, 60);
			}
			if (losses >= m_iMoodLosses60)
				mood = mood - 0.5;
			if (m_Resources.GetFillRatio(region) * 100.0 <= m_iMoodStockLowCent)
				mood = mood - 0.5;
			if (retakenCells >= m_iMoodRetaken60)
				mood = mood + 0.5;
			int trait = m_Book.GetOfficerTrait(region);
			if (trait == SRP_ECmdTrait.PRUDENT)
				mood = mood - 0.5;
			else if (trait == SRP_ECmdTrait.AUDACIEUX)
				mood = mood + 0.5;
			m_Book.SetRegionMood(region, MoodOf(mood));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MA9 : gravité de chaque zone au contact ou attaquée, puis m_aServedZones — FastCycle
	protected void RecomputeGravity(int nowUnix)
	{
		foreach (SRP_CmdZoneIntel intel : m_aIntel)
		{
			if (intel)
				intel.m_fGravity = 0;
		}
		array<int> zones = {};
		GetZonesByGravity(zones);
		foreach (int zone : zones)
		{
			SRP_CmdZoneIntel zoneIntel = IntelOf(zone);
			if (zoneIntel)
				zoneIntel.m_fGravity = ComputeGravity(zone, nowUnix);
		}
		// Les zones servies : 1, ou 2 quand les joueurs sont nombreux (connectés, pas localisés) ; la zone de la
		// contre-attaque est servie en plus (IsServedZone), elle ne prend pas de place
		int served = m_iGravServed;
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (playerManager && playerManager.GetPlayerCount() >= m_iGravManyPlayers)
			served = m_iGravServedMany;
		int caZone = CounterAttackZone();
		m_aServedZones.Clear();
		foreach (int candidate : zones)
		{
			if (m_aServedZones.Count() >= served)
				break;
			if (candidate != caZone)
				m_aServedZones.Insert(candidate);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Exécute un ordre en appelant DIRECTEMENT le moyen (manœuvres ou appuis) ; rend "" ou la raison du refus.
	//! Journal (§1.8) : le moyen n'écrit que sa ligne d'exécution (SRP_CmdLog.Note), le cerveau écrit ICI la décision
	//! (SRP_CmdLog.Decision, coût, stock restant) ou le refus (SRP_CmdLog.Refusal), appuis comme manœuvres — FastCycle,
	//! SlowCycle, ForceOrder
	protected string IssueOrder(SRP_CmdOrder order)
	{
		if (!order)
			return "ordre vide";
		int kind = order.m_iKind;
		int region = order.m_iRegion;
		int count = Math.MaxInt(0, order.m_iVariant);

		string why;
		string cost = "aucun (infanterie)";
		bool heavy = false;
		int stockKind = -1;
		if (kind == SRP_ECmdOrder.MORTIER)
		{
			why = m_Support.RequestMortar(region, order.m_vAim, SRP_ECmdShell.EXPLOSIF, count, order.m_sReason, order.m_bStaff);
			cost = "obus de mortier";
			stockKind = SRP_ECmdStock.OBUS;
		}
		else if (kind == SRP_ECmdOrder.FUMEE)
		{
			why = m_Support.RequestMortar(region, order.m_vAim, SRP_ECmdShell.FUMEE, count, order.m_sReason, order.m_bStaff);
			cost = "aucun (fumigènes gratuits, C5)";
		}
		else if (kind == SRP_ECmdOrder.FEINTE)
		{
			why = m_Support.RequestFeint(region, order.m_vAim, order.m_sReason, order.m_bStaff);
			cost = "obus de mortier";
			stockKind = SRP_ECmdStock.OBUS;
		}
		else if (kind == SRP_ECmdOrder.ARTILLERIE)
		{
			why = m_Support.RequestArtillery(region, order.m_vAim, -1, order.m_sReason, order.m_bStaff);
			cost = "1 tir d'artillerie lourde";
			heavy = true;
			stockKind = SRP_ECmdStock.ARTILLERIE;
		}
		else if (kind == SRP_ECmdOrder.BOMBE)
		{
			why = m_Support.RequestBomb(order.m_vAim, order.m_sReason, order.m_bStaff);
			cost = "1 bombe";
			heavy = true;
			stockKind = SRP_ECmdStock.BOMBE;
		}
		else if (kind == SRP_ECmdOrder.AVION)
		{
			why = m_Support.RequestFlyby(order.m_vAim, order.m_sReason, order.m_bStaff);
			cost = "aucun (passage sans frappe)";
		}
		else if (kind == SRP_ECmdOrder.HELICO)
		{
			why = m_Support.RequestHeli(order.m_vAim, order.m_sReason, order.m_bStaff);
			cost = "1 sortie d'hélico";
			stockKind = SRP_ECmdStock.HELICO;
		}
		else if (kind == SRP_ECmdOrder.BLINDE)
		{
			int armorReason = order.m_iArmorReason;
			if (order.m_bStaff)
				armorReason = SRP_ECmdArmorReason.STAFF;
			why = m_Support.RequestArmor(region, order.m_vAim, armorReason, order.m_iZone, order.m_iVariant, order.m_bStaff);
			cost = "1 blindé";
			heavy = true;
			stockKind = SRP_ECmdStock.BLINDE;
		}
		else if (kind == SRP_ECmdOrder.RENFORT)
			why = m_Maneuvers.RequestReinforcement(order.m_iZone, order.m_vAim, order.m_iSize, order.m_sReason, order.m_sAuthor, order.m_bStaff);
		else if (kind == SRP_ECmdOrder.ENQUETE)
			why = m_Maneuvers.RequestSearch(order.m_vAim, order.m_iZone, order.m_sReason, order.m_bStaff);
		else if (kind == SRP_ECmdOrder.HARCELEMENT)
		{
			why = m_Maneuvers.RequestHarass(order.m_vAim, order.m_sReason, order.m_sAuthor, order.m_bStaff);
			cost = "selon le harcèlement choisi (quelques obus ou une incursion)";
		}
		else if (kind == SRP_ECmdOrder.COLONNE)
			why = m_Maneuvers.RequestColumn(order.m_vAim, order.m_sReason, order.m_sAuthor, order.m_bStaff);
		else if (kind == SRP_ECmdOrder.DECROCHAGE)
			why = m_Maneuvers.RequestRetreat(order.m_vAim, order.m_sReason, order.m_sAuthor, order.m_bStaff);
		else
			why = "ordre inconnu";

		// AP7 : le cerveau ne repropose l'hélico qu'au bout du repos compté par les appuis depuis cette sortie
		// (appui_helico_repos_min) ou, après un refus (repos relu d'une sauvegarde, stock, sécurité), au bout de
		// vu_refus_repos_min : un seul « écarté : hélico » par délai, au lieu d'un à chaque cycle rapide
		if (kind == SRP_ECmdOrder.HELICO && !order.m_bStaff)
		{
			int heliNow = System.GetUnixTime();
			if (why.IsEmpty())
				m_iHeliRetryUnix = heliNow + m_Support.m_iHeliRestMin * 60;
			else
				m_iHeliRetryUnix = heliNow + SRP_CmdScreens.s_iRefusalRepeatMin * 60;
		}

		string what = DescribeOrder(order);
		if (!why.IsEmpty())
		{
			SRP_CmdLog.Refusal(region, what, why);
			return why;
		}
		string reason = order.m_sReason;
		if (reason.IsEmpty())
			reason = "décision du Commandeur";
		// VU5 : ticket gratuit (SRP_CmdResources.Reserve) pour une décision forcée par le Staff : rien n'a été retiré
		if (order.m_bStaff && m_bStaffFree && stockKind >= 0)
			cost = "aucun (décision forcée par le Staff, cmd_staff_gratuit)";
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
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Position floutée (CO5) : direction au hasard, distance entre minMeters et maxMeters, posée au sol (jamais
	//! vector + float) — CollectGroupReports, InformantsTick, NoteAirSighting
	protected vector Blur(vector position, float minMeters, float maxMeters)
	{
		float low = Math.Min(minMeters, maxMeters);
		float high = Math.Max(minMeters, maxMeters);
		float angle = Math.RandomFloat(0, Math.PI2);
		float distance = Math.RandomFloatInclusive(low, high);
		vector result = position + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
		BaseWorld world = GetGame().GetWorld();
		if (world)
			result[1] = world.GetSurfaceY(result[0], result[2]);
		return result;
	}

	//================================================================================================
	// Outils internes (ajoutés au squelette, protégés)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Entier tiré entre deux bornes incluses, dans n'importe quel ordre — cycles, comptes rendus
	protected static int RandomBetween(int a, int b)
	{
		if (b < a)
			return Math.RandomIntInclusive(b, a);
		return Math.RandomIntInclusive(a, b);
	}

	//------------------------------------------------------------------------------------------------
	//! Mémoire d'une zone, null hors bornes — partout
	protected SRP_CmdZoneIntel IntelOf(int zone)
	{
		if (zone < 0 || zone >= m_aIntel.Count())
			return null;
		return m_aIntel[zone];
	}

	//------------------------------------------------------------------------------------------------
	//! Mémoire des zones refaite à neuf (anneaux à zéro) — BuildRegions, Start, ResetCampaign, OnRestored
	protected void ResizeIntel(int count)
	{
		m_aIntel.Clear();
		for (int i = 0; i < count; i++)
		{
			m_aIntel.Insert(new SRP_CmdZoneIntel());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Propriétaires des carrés recopiés sans compte rendu (-1 hors jeu) — Start, DiffCells, ResetCampaign, OnRestored
	protected void CopyCells(SRP_FrontComponent front)
	{
		m_aCellSeen.Clear();
		int count = front.GetCellCount();
		for (int cell = 0; cell < count; cell++)
		{
			m_aCellSeen.Insert(front.GetCellOwner(cell));
		}
		m_iFrontVersionSeen = front.GetStateVersion();
	}

	//------------------------------------------------------------------------------------------------
	//! Connaissance, files et zones servies vidées (H3 : rien de cela n'est sauvé) — ResetCampaign, OnRestored
	protected void ForgetKnowledge()
	{
		m_aContacts.Clear();
		m_aPending.Clear();
		m_aReflexQueue.Clear();
		m_aInformRolled.Clear();
		m_aInformRolledAt.Clear();
		m_aServedZones.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07) », ou « hors des zones » — textes Staff
	protected string ZoneText(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0 || zone >= front.GetZoneCount())
			return "hors des zones";
		return front.GetZoneLabel(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07), carré 074 042 » — textes Staff
	protected string PlaceText(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int zone = -1;
		if (front)
			zone = front.GetZoneAt(position);
		return string.Format("%1, carré %2", ZoneText(zone), SRP_CmdScreens.CellText(position));
	}

	//------------------------------------------------------------------------------------------------
	//! Nom de la localité principale d'une zone (radio d'une localité), sinon le libellé de la zone — DiffCells
	protected string MainLocalityName(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "";
		array<int> localities = {};
		if (front.GetZoneLocalities(zone, localities) > 0)
		{
			SRP_FrontLocality locality = front.GetLocality(localities[0]);
			if (locality)
				return locality.m_sName;
		}
		return ZoneText(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Personnage vivant, conscient et libre (pas menotté ACE) — informateurs, observateurs, unités
	protected static bool IsFit(IEntity character)
	{
		if (!character || character.IsDeleted() || SRP_Utils.IsDead(character))
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (!controller)
			return true;
		if (controller.IsUnconscious())
			return false;
		if (controller.ACE_Captives_IsCaptive())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! L'unité a encore au moins un soldat en état de combattre — comptes rendus, présence des unités
	protected static bool HasFitMember(SRP_EnemyGroup record)
	{
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(record, members);
		foreach (IEntity member : members)
		{
			if (IsFit(member))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! AP7, C4 : une de nos unités (hélico exclu) est-elle à radius du point ? Le Commandeur connaît ses propres
	//! troupes — HeliCycle, SearchCycle
	protected bool IsUnitNear(vector position, float radius)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return false;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		if (!groups)
			return false;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || record.m_sOwner == "helico")
				continue;
			if (vector.DistanceXZ(enemies.GroupPosition(record), position) > radius)
				continue;
			if (HasFitMember(record))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Compte rendu CONTACT d'une unité d'après son tampon (position floutée CO5, tirs bornés CO6), null si l'unité
	//! n'a plus personne ou si le point est dans la base ou hors des régions — CollectGroupReports
	protected SRP_CmdReport BuildGroupReport(SRP_EnemyComponent enemies, SRP_FrontComponent front, SRP_EnemyGroup record, int nowUnix)
	{
		if (!HasFitMember(record))
			return null;
		vector unitPos = enemies.GroupPosition(record);
		SRP_CmdReport report = new SRP_CmdReport();
		report.m_iKind = SRP_ECmdReport.CONTACT;
		report.m_iShots = Math.MinInt(record.m_iCmdShots, m_iReportShotsMax);
		if (record.m_bCmdContact)
		{
			report.m_bSeen = record.m_bCmdSeen;
			int players = 0;
			if (record.m_aCmdPlayers)
				players = record.m_aCmdPlayers.Count();
			report.m_iPlayers = Math.MaxInt(1, players);
			report.m_bVehicle = record.m_bCmdVehicle;
			report.m_bArmed = record.m_bCmdArmed;
			if (record.m_bCmdSeen)
				report.m_vPos = Blur(record.m_vCmdPos, m_iBlurSeenMinM, m_iBlurSeenMaxM);
			else
				report.m_vPos = Blur(record.m_vCmdPos, m_iBlurHeardMinM, m_iBlurHeardMaxM);
		}
		else
			report.m_vPos = unitPos;		// tirs seuls : l'unité dit où elle est, pas où sont les tireurs
		int zone = front.GetZoneAt(report.m_vPos);
		if (zone < 0)
			zone = front.GetZoneAt(unitPos);
		if (zone < 0 || front.IsBaseZone(zone))
			return null;
		int region = m_Book.GetRegionOfZone(zone);
		if (region < 0)
			return null;
		report.m_iZone = zone;
		report.m_iRegion = region;
		report.m_iCell = front.CellIndexAt(report.m_vPos);
		report.m_sSource = SourceOf(record);
		report.m_bNeedsRadio = !record.m_sZone.IsEmpty();
		report.m_vRadioPos = unitPos;
		report.m_bDirect = IsDirectUnit(record);
		report.m_iDueUnix = nowUnix;		// la cadence de 30 à 60 s est le retard du compte rendu
		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! Tampon des comptes rendus de l'unité vidé — CollectGroupReports
	protected void ClearBuffer(SRP_EnemyGroup record)
	{
		record.m_bCmdContact = false;
		record.m_bCmdSeen = false;
		if (record.m_aCmdPlayers)
			record.m_aCmdPlayers.Clear();
		record.m_iCmdShots = 0;
		record.m_bCmdVehicle = false;
		record.m_bCmdArmed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Qui rend compte : localité (« Chotain »), « poste », « unité mobile », « camion », « hélico », « assaut »… —
	//! comptes rendus, journal
	protected string SourceOf(SRP_EnemyGroup record)
	{
		if (!record.m_sZone.IsEmpty())
			return record.m_sZone;
		string task = record.m_sTask;
		if (record.m_sOwner == "helico" || task == "helico")
			return "hélico";
		if (task == "assaut" || task == "vague" || record.m_iAssaultZone >= 0)
			return "assaut";
		if (task == "chauffeur" || task == "camion" || record.m_sOwner == "camion")
			return "camion";
		if (task == "poste" || task == "poste front" || record.m_sHomeTask == "poste front")
			return "poste";
		if (task == "jeep" || task == "ronde" || record.m_sOwner == "ambiance")
			return "unité mobile";
		if (task == "officier" || task == "garde officier")
			return "officier";
		return "unité";
	}

	//------------------------------------------------------------------------------------------------
	//! OF3 : l'hélico et les vagues d'assaut parlent droit au Commandeur, même sans officier — comptes rendus
	protected bool IsDirectUnit(SRP_EnemyGroup record)
	{
		if (record.m_sOwner == "helico" || record.m_sTask == "helico")
			return true;
		return record.m_sTask == "assaut" || record.m_sTask == "vague" || record.m_iAssaultZone >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! L'unité enregistrée de la victime : par son agent et son groupe (FindRecord), sinon parmi les membres encore
	//! inscrits ; null pour un civil ou un soldat qui n'est pas à nous (trou 22) — OnControllableDestroyed
	protected SRP_EnemyGroup FindVictimRecord(SRP_EnemyComponent enemies, IEntity victim)
	{
		AIControlComponent control = AIControlComponent.Cast(victim.FindComponent(AIControlComponent));
		if (control)
		{
			AIAgent agent = control.GetControlAIAgent();
			if (agent)
			{
				SCR_AIGroup group = SCR_AIGroup.Cast(agent.GetParentGroup());
				if (group)
				{
					SRP_EnemyGroup found = enemies.FindRecord(group);
					if (found)
						return found;
				}
			}
		}
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		if (!groups)
			return null;
		array<IEntity> members = {};
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group)
				continue;
			members.Clear();
			SRP_EnemyComponent.GetMembers(record, members);
			if (members.Contains(victim))
				return record;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! AP7 : compte rendu CONTACT de l'hélico (vu, direct, flouté), un seul en route par endroit — NoteAirSighting
	protected void AddAirReport(vector position)
	{
		if (!m_bStarted)
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;
		foreach (SRP_CmdReport pending : m_aPending)
		{
			if (pending && pending.m_bDirect && pending.m_sSource == "hélico" && vector.DistanceXZ(pending.m_vPos, position) <= m_iMergeM)
				return;
		}
		int nowUnix = System.GetUnixTime();
		vector blurred = Blur(position, m_iBlurSeenMinM, m_iBlurSeenMaxM);
		int zone = front.GetZoneAt(blurred);
		if (zone < 0)
			zone = front.GetZoneAt(position);
		if (zone < 0 || front.IsBaseZone(zone))
			return;
		int region = m_Book.GetRegionOfZone(zone);
		if (region < 0)
			return;
		SRP_CmdReport report = new SRP_CmdReport();
		report.m_iKind = SRP_ECmdReport.CONTACT;
		report.m_iRegion = region;
		report.m_iZone = zone;
		report.m_iCell = front.CellIndexAt(blurred);
		report.m_vPos = blurred;
		report.m_iPlayers = 1;
		report.m_bSeen = true;
		report.m_bDirect = true;
		report.m_sSource = "hélico";
		report.m_iDueUnix = nowUnix + m_iReportMinS;
		m_aPending.Insert(report);
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne Staff d'un contact neuf ou passé de « entendu » à « vu » — Integrate
	protected void LogContact(SRP_CmdReport report, SRP_CmdContact contact)
	{
		string text;
		if (report.m_iKind == SRP_ECmdReport.INFORMATEUR)
			text = string.Format("Signalement d'un civil : %1 joueur(s) vers %2, %3, à %4 m près (fouille seulement)", contact.m_iPlayers, SRP_CmdScreens.CellText(contact.m_vPos), ZoneText(contact.m_iZone), m_iInfoBlurMaxM);
		else if (report.m_bSeen)
			text = string.Format("%1 joueur(s) vus vers %2, %3, à %4 m près (%5)", contact.m_iPlayers, SRP_CmdScreens.CellText(contact.m_vPos), ZoneText(contact.m_iZone), m_iBlurSeenMaxM, report.m_sSource);
		else
			text = string.Format("%1 joueur(s) entendus vers %2, %3, à %4 m près (%5)", contact.m_iPlayers, SRP_CmdScreens.CellText(contact.m_vPos), ZoneText(contact.m_iZone), m_iBlurHeardMaxM, report.m_sSource);
		SRP_CmdLog.Note(report.m_iRegion, SRP_ECmdLogKind.RENSEIGNEMENT, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Meilleur contact d'une zone (informateurs exclus, C4) : vu récemment d'abord, puis le plus nombreux, puis le
	//! plus récent ; seenOnly : seulement vu depuis maxAgeSeconds — GetBestContact, cycles
	protected SRP_CmdContact BestContact(int zone, bool seenOnly, int maxAgeSeconds, int nowUnix)
	{
		SRP_CmdContact best = null;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || contact.m_iZone != zone)
				continue;
			bool seenRecent = contact.m_bSeen && nowUnix - contact.m_iSeenUnix <= maxAgeSeconds;
			if (seenOnly && !seenRecent)
				continue;
			if (!seenOnly && nowUnix - contact.m_iLastUnix > maxAgeSeconds)
				continue;
			if (!best || IsBetterContact(contact, best, nowUnix, maxAgeSeconds))
				best = contact;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre de préférence de deux contacts — BestContact, HeliCycle
	protected bool IsBetterContact(SRP_CmdContact a, SRP_CmdContact b, int nowUnix, int maxAgeSeconds)
	{
		bool aSeen = a.m_bSeen && nowUnix - a.m_iSeenUnix <= maxAgeSeconds;
		bool bSeen = b.m_bSeen && nowUnix - b.m_iSeenUnix <= maxAgeSeconds;
		if (aSeen != bSeen)
			return aSeen;
		if (a.m_iPlayers != b.m_iPlayers)
			return a.m_iPlayers > b.m_iPlayers;
		return a.m_iLastUnix > b.m_iLastUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! MA9, la SEULE formule : contre-attaque en assaut sur la zone, puis (si un compte rendu est arrivé dans la
	//! fenêtre) point clé menacé, carrés perdus, pertes, tirs des 3 dernières minutes, joueurs connus ; région
	//! désorganisée : 0 — GetZoneGravity, GetZonesByGravity
	protected float ComputeGravity(int zone, int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (!front || !intel || zone >= front.GetZoneCount() || front.IsBaseZone(zone))
			return 0;
		int region = m_Book.GetRegionOfZone(zone);
		if (region >= 0 && m_Book.IsRegionDisorganized(region))
			return 0;
		float gravity = 0;
		if (IsAssaultZone(zone))
			gravity = gravity + m_iGravCounterAttack;
		if (intel.m_iLastReportUnix <= 0 || nowUnix - intel.m_iLastReportUnix > m_iGravWindowMin * 60)
			return gravity;
		vector point;
		if (FindThreatenedKeyPoint(zone, nowUnix, point))
			gravity = gravity + m_iGravKeyPoint;
		gravity = gravity + m_iGravCellLost * intel.m_CellsLost.Sum(nowUnix, m_iGravWindowMin);
		gravity = gravity + m_iGravLoss * intel.m_Losses.Sum(nowUnix, m_iGravWindowMin);
		gravity = gravity + intel.m_Shots.Sum(nowUnix, 3) / Math.MaxInt(1, m_iGravShotsPer);
		gravity = gravity + Math.MinInt(GetKnownPlayers(zone), m_iGravPlayersMax);
		return gravity;
	}

	//------------------------------------------------------------------------------------------------
	//! MA9, AP5 : un point clé non pris de la zone a un contact connu (informateurs exclus) à cmd_grav_point_cle_m,
	//! mis à jour dans la fenêtre de gravité ; sa position dans point — ComputeGravity, ArmorCycle
	protected bool FindThreatenedKeyPoint(int zone, int nowUnix, out vector point)
	{
		point = vector.Zero;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return false;
		int window = m_iGravWindowMin * 60;
		int points = front.GetKeyPointCount(zone);
		for (int k = 0; k < points; k++)
		{
			if (front.IsKeyPointTaken(zone, k))
				continue;
			vector position = front.GetKeyPointPos(zone, k);
			foreach (SRP_CmdContact contact : m_aContacts)
			{
				if (!contact || contact.m_bInformantOnly || nowUnix - contact.m_iLastUnix > window)
					continue;
				if (vector.DistanceXZ(contact.m_vPos, position) <= m_iGravKeyPointM)
				{
					point = position;
					return true;
				}
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone visée par la contre-attaque en cours, -1 sinon — gravité, cycles
	protected int CounterAttackZone()
	{
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		if (!host)
			return -1;
		return host.GetCounterAttackZone();
	}

	//------------------------------------------------------------------------------------------------
	//! La contre-attaque en cours est en ASSAUT sur cette zone (terme MA9, trou 10) — ComputeGravity
	protected bool IsAssaultZone(int zone)
	{
		if (zone < 0 || zone != CounterAttackZone())
			return false;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return false;
		return front.GetZoneAttackState(zone) == SRP_EFrontAttack.ASSAUT;
	}

	//------------------------------------------------------------------------------------------------
	//! CO7 : -0,5 ou moins = prudente, +0,5 ou plus = agressive, sinon normale — RecomputeMoods, Staff
	protected int MoodOf(float value)
	{
		if (value <= -0.5)
			return SRP_ECmdMood.PRUDENTE;
		if (value >= 0.5)
			return SRP_ECmdMood.AGRESSIVE;
		return SRP_ECmdMood.NORMALE;
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvel ordre du Commandeur (exécutable tout de suite) — cycles, réflexes, ForceOrder
	protected SRP_CmdOrder NewOrder(int kind, int region, int zone, vector aim, string reason, int nowUnix)
	{
		SRP_CmdOrder order = new SRP_CmdOrder();
		order.m_iKind = kind;
		order.m_iRegion = region;
		order.m_iZone = zone;
		order.m_vAim = aim;
		order.m_sReason = reason;
		order.m_iNotBefore = nowUnix;
		return order;
	}

	//------------------------------------------------------------------------------------------------
	//! « Renfort de 6 soldats vers Régina (S07), carré 074 042 » : l'ordre en clair pour le journal (manœuvres ; les
	//! appuis écrivent leur propre libellé) — IssueOrder
	protected string DescribeOrder(SRP_CmdOrder order)
	{
		string place = PlaceText(order.m_vAim);
		int kind = order.m_iKind;
		if (kind == SRP_ECmdOrder.RENFORT)
		{
			if (order.m_iSize > 0)
				return string.Format("Renfort de %1 soldats vers %2", order.m_iSize, place);
			return "Renfort vers " + place;
		}
		if (kind == SRP_ECmdOrder.MORTIER)
			return "Mortier sur " + place;
		if (kind == SRP_ECmdOrder.FUMEE)
			return "Fumée au mortier sur " + place;
		if (kind == SRP_ECmdOrder.FEINTE)
			return "Feinte au mortier sur " + place;
		if (kind == SRP_ECmdOrder.ARTILLERIE)
			return "Artillerie lourde sur " + place;
		if (kind == SRP_ECmdOrder.BOMBE)
			return "Bombe sur " + place;
		if (kind == SRP_ECmdOrder.AVION)
			return "Passage d'avion au-dessus de " + place;
		if (kind == SRP_ECmdOrder.BLINDE)
		{
			string why = "renfort";
			if (order.m_iArmorReason == SRP_ECmdArmorReason.RIPOSTE)
				why = "riposte à un véhicule armé";
			else if (order.m_iArmorReason == SRP_ECmdArmorReason.CONTRE_ATTAQUE)
				why = "contre-attaque";
			else if (order.m_iArmorReason == SRP_ECmdArmorReason.STAFF || order.m_bStaff)
				why = "forcé";
			return string.Format("Blindé (%1) vers %2", why, place);
		}
		if (kind == SRP_ECmdOrder.HELICO)
			return "Hélico de recherche vers " + place;
		if (kind == SRP_ECmdOrder.ENQUETE)
			return "Fouille d'une unité mobile vers " + place;
		if (kind == SRP_ECmdOrder.HARCELEMENT)
			return "Harcèlement du front près de " + place;
		if (kind == SRP_ECmdOrder.COLONNE)
			return "Colonne depuis la localité la plus proche de " + place;
		if (kind == SRP_ECmdOrder.DECROCHAGE)
			return "Décrochage de la localité la plus proche de " + place;
		return "Ordre inconnu vers " + place;
	}

	//------------------------------------------------------------------------------------------------
	//! Réflexes de renfort échus (officier lent : après son retard) : zone encore rouge, puis le moyen (le frein C2
	//! est revu par les manœuvres) — FastCycle
	protected void RunDueReflexes(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		// Les ordres échus sont gardés par un tableau de ref le temps de leur exécution
		array<ref SRP_CmdOrder> due = {};
		for (int i = 0; i < m_aReflexQueue.Count(); i++)
		{
			if (m_aReflexQueue[i] && m_aReflexQueue[i].m_iNotBefore <= nowUnix)
				due.Insert(m_aReflexQueue[i]);
		}
		for (int k = m_aReflexQueue.Count() - 1; k >= 0; k--)
		{
			if (!m_aReflexQueue[k] || m_aReflexQueue[k].m_iNotBefore <= nowUnix)
				m_aReflexQueue.Remove(k);
		}
		foreach (SRP_CmdOrder order : due)
		{
			if (order.m_iZone < 0 || front.GetZoneOwner(order.m_iZone) != SRP_EFrontOwner.ROUGE)
				continue;		// zone tombée entre-temps : le renfort n'a plus d'objet
			IssueOrder(order);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CO4, MO1 : mortier sur les zones servies les plus graves (cmd_mortier_zones_par_cycle au plus) — FastCycle
	protected void MortarCycle(int nowUnix)
	{
		array<int> zones = {};
		GetZonesByGravity(zones);
		int examined = 0;
		int caZone = CounterAttackZone();
		foreach (int zone : zones)
		{
			if (examined >= m_iMortarZonesPerCycle)
				break;
			if (!IsServedZone(zone) || zone == caZone)
				continue;		// la contre-attaque est soutenue par les manœuvres (CA4)
			examined++;
			TryMortar(zone, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Mortier sur une zone : région ACTIVE avec son officier, appui justifié (CO6), contact VU depuis
	//! cmd_contact_vise_s (la cible), repos de la zone, prudence (CO7) ; les plafonds, seuils, stock et sécurité
	//! restent aux appuis — MortarCycle
	protected void TryMortar(int zone, int nowUnix)
	{
		SRP_CmdZoneIntel intel = IntelOf(zone);
		if (!intel)
			return;
		int region = m_Book.GetRegionOfZone(zone);
		if (region < 0 || m_Book.GetRegionState(region) != SRP_ECmdRegionState.ACTIVE || !m_Book.HasLivingOfficer(region))
			return;
		if (intel.m_iLastMortarUnix > 0 && nowUnix - intel.m_iLastMortarUnix < m_iMortarZoneRestS)
			return;
		if (!IsHeavySupportJustified(zone))
			return;
		SRP_CmdContact target = BestContact(zone, true, m_iAimS, nowUnix);
		if (!target)
			return;
		if (!m_Support.IsMortarReady(region))
			return;		// pièce muette, déjà en tir ou appuis coupés : rien à demander

		// Clé stable par zone : la position connue bouge à chaque compte rendu, l'anti-répétition des refus
		// (vu_refus_repos_min, sur what + why) ne jouerait jamais
		string what = "Mortier sur " + ZoneText(zone);
		bool prudent = m_Book.GetOfficerTrait(region) == SRP_ECmdTrait.PRUDENT || m_Book.GetRegionMood(region) == SRP_ECmdMood.PRUDENTE;
		if (prudent)
		{
			// CO7 : un prudent garde sa réserve d'obus et attend un contact confirmé (2 comptes rendus) ou 2 pertes en 10 min
			if (m_Resources.GetFillRatio(region) * 100.0 < m_iPrudentShellKeepCent)
			{
				intel.m_iLastMortarUnix = nowUnix;
				SRP_CmdLog.Refusal(region, what, "prudence : obus de la région sous la réserve");
				return;
			}
			if (target.m_iReports < 2 && GetZoneLosses(zone, 10) < 2)
			{
				SRP_CmdLog.Refusal(region, what, "prudence : contact pas encore confirmé");
				return;
			}
		}
		intel.m_iLastMortarUnix = nowUnix;
		string reason = string.Format("%1 joueur(s) vus il y a %2", target.m_iPlayers, SRP_CmdScreens.DurationText(nowUnix - target.m_iSeenUnix));
		SRP_CmdOrder order = NewOrder(SRP_ECmdOrder.MORTIER, region, zone, target.m_vPos, reason, nowUnix);
		order.m_iVariant = 0;		// nombre d'obus au choix des appuis
		IssueOrder(order);
	}

	//------------------------------------------------------------------------------------------------
	//! AP7 : le cerveau peut proposer une sortie d'hélico : appuis actifs (appui_actif), aucun hélico en vol, repos de
	//! sa dernière sortie passé et aucun refus d'hélico depuis vu_refus_repos_min (m_iHeliRetryUnix, posé par
	//! IssueOrder) ; le stock et la sécurité restent aux appuis — HeliCycle, SearchCycle
	protected bool CanTryHeli(int nowUnix)
	{
		if (!m_Support.m_bOn || SRP_HeliSearch.Count() > 0)
			return false;
		return nowUnix >= m_iHeliRetryUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! AP7 : le point visé est-il sur un carré ennemi, comme RequestHeli l'exige hors Staff (front absent ou pas prêt :
	//! pas de refus) ? Un point flouté (CO5) peut tomber sur un carré bleu ou en mer : l'hélico n'y est pas proposé,
	//! sinon ce refus, qui ne tient qu'à la cible, bloquerait tout hélico du cerveau pendant vu_refus_repos_min —
	//! HeliCycle, SearchCycle
	protected bool IsHeliTargetRed(SRP_FrontComponent front, vector position)
	{
		if (!front || !front.IsReady())
			return true;
		return front.IsRedAt(position);
	}

	//------------------------------------------------------------------------------------------------
	//! EQ3 pour un blindé du cerveau : plafond appui_blinde_simultanes atteint, ou autre grosse opération en cours
	//! (artillerie, bombe, contre-attaque) ; les blindés engagés sous le plafond ne bloquent pas (même règle que
	//! RequestArmor) — ArmorCycle
	protected bool IsArmorBlocked()
	{
		SRP_CmdArmorManager armor = m_Support.GetArmor();
		if (!armor)
			return true;
		int active = armor.CountActive();
		if (active >= armor.m_iArmorAtOnce)
			return true;
		// Aucun blindé engagé : IsBigOperationRunning ne voit que les autres grosses opérations
		if (active == 0)
			return m_Support.IsBigOperationRunning(-1);
		// Un blindé engagé sous le plafond : IsBigOperationRunning le compterait ; la contre-attaque se lit ici (le cerveau
		// ne lance ni salve ni bombe pendant qu'un blindé est engagé ; une salve ou une bombe du Staff reste refusée par
		// RequestArmor)
		SRP_FrontEnemyComponent host = SRP_FrontEnemyComponent.GetInstance();
		return host && host.IsCounterAttackActive();
	}

	//------------------------------------------------------------------------------------------------
	//! AP7 : joueurs VUS en zone rouge (point sur un carré rouge) depuis cmd_contact_vise_s, région ACTIVE, aucune de nos unités à
	//! cmd_helico_sans_troupe_m : un ordre HELICO par cycle au plus ; rend vrai si l'ordre est parti — FastCycle
	protected bool HeliCycle(int nowUnix)
	{
		if (!CanTryHeli(nowUnix))
			return false;		// appuis coupés, hélico en l'air, repos ou refus récent : pas d'ordre ni de refus au journal
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return false;
		SRP_CmdContact best = null;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || !contact.m_bSeen)
				continue;
			if (nowUnix - contact.m_iSeenUnix > m_iAimS)
				continue;
			if (contact.m_iZone < 0 || front.GetZoneOwner(contact.m_iZone) != SRP_EFrontOwner.ROUGE)
				continue;
			// Zone rouge, mais le point (flouté) doit aussi être sur un carré rouge (RequestHeli)
			if (!IsHeliTargetRed(front, contact.m_vPos))
				continue;
			if (contact.m_iRegion < 0 || m_Book.GetRegionState(contact.m_iRegion) != SRP_ECmdRegionState.ACTIVE)
				continue;
			if (best && !IsBetterContact(contact, best, nowUnix, m_iAimS))
				continue;
			if (IsUnitNear(contact.m_vPos, m_iHeliNoTroopM))
				continue;
			best = contact;
		}
		if (!best)
			return false;
		string reason = string.Format("%1 joueur(s) vus il y a %2, aucune de nos unités à %3 m", best.m_iPlayers, SRP_CmdScreens.DurationText(nowUnix - best.m_iSeenUnix), m_iHeliNoTroopM);
		SRP_CmdOrder order = NewOrder(SRP_ECmdOrder.HELICO, best.m_iRegion, best.m_iZone, best.m_vPos, reason, nowUnix);
		return IssueOrder(order).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! C4 : chaque contact « informateur seul » pas encore traité donne UNE fouille : l'unité mobile la plus proche,
	//! ou l'hélico si aucune unité n'est à cmd_helico_sans_troupe_m (et qu'aucun n'est parti à ce cycle) — FastCycle
	protected void SearchCycle(int nowUnix, bool heliSent)
	{
		SRP_FrontComponent searchFront = SRP_FrontComponent.GetInstance();
		bool heliUsed = heliSent;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || !contact.m_bInformantOnly || contact.m_bInvestigated)
				continue;
			contact.m_bInvestigated = true;
			string reason = string.Format("signalement d'un civil : %1 joueur(s) vers %2", contact.m_iPlayers, SRP_CmdScreens.CellText(contact.m_vPos));
			// Hélico seulement sur un carré rouge (position floutée, RequestHeli) : sinon la fouille à pied
			if (!heliUsed && CanTryHeli(nowUnix) && IsHeliTargetRed(searchFront, contact.m_vPos) && !IsUnitNear(contact.m_vPos, m_iHeliNoTroopM))
			{
				heliUsed = true;
				SRP_CmdOrder heli = NewOrder(SRP_ECmdOrder.HELICO, contact.m_iRegion, contact.m_iZone, contact.m_vPos, reason, nowUnix);
				if (IssueOrder(heli).IsEmpty())
					continue;
			}
			SRP_CmdOrder search = NewOrder(SRP_ECmdOrder.ENQUETE, contact.m_iRegion, contact.m_iZone, contact.m_vPos, reason, nowUnix);
			IssueOrder(search);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! AP5 : blindé en riposte (véhicule armé vu depuis cmd_blinde_vehicule_s) sinon en renfort d'une localité
	//! attaquée (gravité cmd_blinde_gravite et point clé menacé) ; région ACTIVE, appui justifié ; un ordre par cycle
	//! (EQ3 : appui_blinde_simultanes blindés à la fois et aucune autre grosse opération, contrôlés aussi par les
	//! appuis) — SlowCycle
	protected void ArmorCycle(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || IsArmorBlocked())
			return;
		int caZone = CounterAttackZone();
		int count = Math.MinInt(front.GetZoneCount(), m_aIntel.Count());

		// Riposte : un véhicule armé des joueurs a été vu
		for (int zone = 1; zone < count; zone++)
		{
			if (zone == caZone)
				continue;
			vector armedPos;
			if (!WasArmedVehicleSeen(zone, m_iArmorVehicleS, armedPos))
				continue;
			int region = m_Book.GetRegionOfZone(zone);
			if (region < 0 || m_Book.GetRegionState(region) != SRP_ECmdRegionState.ACTIVE)
				continue;
			if (!IsHeavySupportJustified(zone))
				continue;
			SRP_CmdOrder riposte = NewOrder(SRP_ECmdOrder.BLINDE, region, zone, armedPos, "véhicule armé vu il y a moins de " + SRP_CmdScreens.DurationText(m_iArmorVehicleS), nowUnix);
			riposte.m_iArmorReason = SRP_ECmdArmorReason.RIPOSTE;
			IssueOrder(riposte);
			return;
		}

		// Renfort blindé d'une localité attaquée : zones les plus graves d'abord
		array<int> zones = {};
		GetZonesByGravity(zones);
		foreach (int graveZone : zones)
		{
			if (graveZone == caZone)
				continue;
			float gravity = ComputeGravity(graveZone, nowUnix);
			if (gravity < m_iArmorGravity)
				break;		// triées par gravité décroissante
			if (front.GetZoneOwner(graveZone) != SRP_EFrontOwner.ROUGE)
				continue;
			int graveRegion = m_Book.GetRegionOfZone(graveZone);
			if (graveRegion < 0 || m_Book.GetRegionState(graveRegion) != SRP_ECmdRegionState.ACTIVE)
				continue;
			vector keyPoint;
			if (!FindThreatenedKeyPoint(graveZone, nowUnix, keyPoint))
				continue;
			if (!IsHeavySupportJustified(graveZone))
				continue;
			vector aim = keyPoint;
			SRP_CmdContact target = BestContact(graveZone, false, m_iConfirmS, nowUnix);
			if (target)
				aim = target.m_vPos;
			int shownGravity = Math.Round(gravity);
			SRP_CmdOrder support = NewOrder(SRP_ECmdOrder.BLINDE, graveRegion, graveZone, aim, string.Format("localité attaquée : gravité %1, point clé menacé", shownGravity), nowUnix);
			support.m_iArmorReason = SRP_ECmdArmorReason.RENFORT;
			IssueOrder(support);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! AP2, AP3 : contact VU (cmd_contact_vise_s) d'au moins cmd_artillerie_joueurs (ou cmd_bombe_joueurs) joueurs,
	//! resté sur place depuis cmd_artillerie_immobile_s : artillerie lourde, sinon bombe ; une frappe par cycle —
	//! SlowCycle
	protected void HeavyFireCycle(int nowUnix)
	{
		if (m_Support.IsBigOperationRunning(-1))
			return;
		int caZone = CounterAttackZone();
		int needed = Math.MinInt(m_iArtilleryPlayers, m_iBombPlayers);
		SRP_CmdContact best = null;
		foreach (SRP_CmdContact contact : m_aContacts)
		{
			if (!contact || contact.m_bInformantOnly || !contact.m_bSeen)
				continue;
			if (nowUnix - contact.m_iSeenUnix > m_iAimS)
				continue;
			if (contact.m_iPlayers < needed || nowUnix - contact.m_iFirstUnix < m_iArtilleryStillS)
				continue;
			if (contact.m_iZone < 0 || contact.m_iZone == caZone || !IsHeavySupportJustified(contact.m_iZone))
				continue;
			if (contact.m_iRegion < 0 || !m_Book.IsRegionAlive(contact.m_iRegion))
				continue;
			if (!best || IsBetterContact(contact, best, nowUnix, m_iAimS))
				best = contact;
		}
		if (!best)
			return;

		string reason = string.Format("%1 joueur(s) vus il y a %2, sur place depuis %3", best.m_iPlayers, SRP_CmdScreens.DurationText(nowUnix - best.m_iSeenUnix), SRP_CmdScreens.DurationText(nowUnix - best.m_iFirstUnix));
		bool artilleryAllowed = best.m_iPlayers >= m_iArtilleryPlayers && m_Book.GetRegionState(best.m_iRegion) == SRP_ECmdRegionState.ACTIVE;
		if (artilleryAllowed)
		{
			SRP_CmdOrder artillery = NewOrder(SRP_ECmdOrder.ARTILLERIE, best.m_iRegion, best.m_iZone, best.m_vPos, reason, nowUnix);
			if (IssueOrder(artillery).IsEmpty())
				return;
		}
		if (best.m_iPlayers < m_iBombPlayers)
			return;
		SRP_CmdOrder bomb = NewOrder(SRP_ECmdOrder.BOMBE, best.m_iRegion, best.m_iZone, best.m_vPos, reason, nowUnix);
		IssueOrder(bomb);
	}
}
