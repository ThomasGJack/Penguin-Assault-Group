//------------------------------------------------------------------------------------------------
// SimpleRP — IA ennemie : socle commun et présence ambiante
// SRP_EnemyComponent (sur le game mode) :
// - connaît les prefabs de groupes ennemis et les points de passage, pose des groupes autour d'un
//   point avec un ordre (défense, déplacement), les retire proprement (membres + groupe) ; registre
//   de tout ce qui a été posé, plafond global ;
// - effectif au prorata des joueurs PROCHES du lieu (pas des connectés), et budget local : garnison, garde de
//   mission, vagues et patrouilles d'un même lieu partagent un même plafond, pour ne pas faire doublon ;
// - perception réglée par état et par soldat (SRP_EnemySenses, carte #69) ; ALERTE au premier
//   repérage d'un joueur (vu ou entendu, lu dans la perception des groupes) : une patrouille vient voir, la jeep se
//   rapproche, des fusées éclairantes blanches montent la nuit ; l'alerte s'éteint sans nouveau repérage ;
// - patrouilles en jeep armée : circuit en boucle par les lieux-dits voisins d'un lieu gardé ;
// - renforts en camion (carte #69, livraison 3, SRP_EnemyTrucks) : une vague peut venir en camion, au même tirage
//   qu'avant ; le camion se gare à la vue, tout le monde débarque, il repart et disparaît hors de vue ;
// - présence ambiante : patrouilles rares posées hors de vue des joueurs, nettement plus fréquentes
//   à proximité d'une mission active, jamais près de la base, retirées loin des joueurs ou après un
//   délai ; une fois sur deux la patrouille suit une route entre deux lieux-dits et continue sa ronde.
// Carte #58 (1.0.26) — déplacements et apparitions refaits :
// - toute pose passe par SRP_Placement (hors de vue, distance aux joueurs, rayon interdit de la base, route ou lisière) ;
// - garnison : premier groupe en Defend (postes, tourelles, bâtiments : AIWaypoint_Defend_Large), les suivants en ronde
//   sur les rues (AIWaypoint_Cycle de 3-5 points de route) ou en Defend ;
// - trajets longs par la route puis à travers champs pour le dernier tronçon ; renforts motorisés débarqués sur la route ;
// - délai de grâce de 3 min pour un joueur qui vient d'apparaître (il ne compte pas comme « proche ») ;
// - survivants d'une mission retirés après 15 min et hors de vue ; reddition d'un groupe réduit à 1-2 hommes au
//   contact (ACE Captives) ; fréquence de présence, jeep et renforts selon la menace ; journal préfixé [ESSAI] en mode essai.
// Carte #69, livraison 1 — l'ennemi voit :
// - perception de chaque soldat réglée par SRP_EnemySenses (rôle, lumière réelle, lampe, tirs, fusées, posture, buisson),
//   tir moins précis sous le feu ; les soldats sont réglés un par un à leur arrivée (passage toutes les 2 s) ;
// - pose de la 1.8 : un groupe encore vide est gardé pendant sa grâce (il compte dans le plafond), abandonné ensuite ;
//   effectif attendu, soldats comptés sans fausse perte (alerte « garde tuée », garnison neutralisée, reddition) ;
// - pages Staff : état des groupes autour de soi, journal de repérage.
// Carte #69, livraison 2 — section, aide, ratissage, sirène :
// - l'alerte ne fait plus venir « une patrouille » : le groupe libre le plus proche vient AIDER environ 20 s après le
//   premier repérage, par le flanc (point hors de vue du contact), sans position injectée à sa perception ; contact
//   perdu depuis 1 min : RATISSAGE d'environ 10 min (ceux venus aider, puis le plus proche), puis retour au poste ou à
//   la ronde (SRP_EnemyAwareness) ; les autres groupes proches deviennent plus VIGILANTS sur place quelques minutes
//   (perception au calme relevée, ils ne quittent pas leur poste) ;
// - chaque groupe retient sa tâche et son poste d'origine ; le premier groupe d'une défense (centre) ne part aider que
//   s'il est le seul groupe libre (livraison 3) ;
// - un soldat d'une garnison de territoire qui VOIT un joueur dans sa localité demande la sirène (SRP_Territory,
//   SRP_Sirene) ; les jeeps se postent comme avant ; les groupes de 4 et plus combattent en binômes (SRP_CRX).
// Carte #69, correctifs après l'essai de Jack du 25/09 (sentinelle assise qui ne réagit pas) :
// - nos points Defend ne gardent que les postes debout (m_bGuardsNoSitting) ; filet dans SRP_EnemySenses ;
// - le rayon du secteur ne sert plus qu'au point Defend : chaque soldat de garde tient m_fGarrisonPostHoldRadius autour de
//   son poste ; garnison sans point hors de vue avec un joueur dans le secteur : pose différée (plus de repli sur lui) ;
// - états statiques remis à zéro à chaque partie (hélicoptère, sens de l'ennemi) ; alerte « vus » / « entendus » exacte,
//   passage à « vus » écrit ; alertes comparées à plat ; IA d'un soldat qui se rend coupée tout de suite.
// Carte #69, livraison 3 — effectifs (partie garnisons) :
// - taille d'un prefab de groupe lue dans le prefab (GroupSize) ; prefab choisi par taille : sections de 4 à 6 pour les
//   gardes et vagues de mission, patrouilles hors mission légères (2 à 4) ; le groupe de commandement (QG, tireur
//   d'élite) n'est jamais tiré au hasard : il ne vient qu'avec une garnison de ville (SRP_EnemyGarrison) ;
// - plafond de soldats des garnisons et des camions (m_iMaxSoldiers, groupes marqués m_bBudget, jeep d'une garnison
//   comprise), à part des missions ; plafond de groupes : m_iMissionGroupReserve groupes toujours gardés pour les missions ;
// - pose de la 1.8 : grâce passée, la file ne livre plus rien à un groupe (CloseStalledSpawns : pas de soldat qui apparaît
//   des minutes plus tard sous les yeux d'un joueur) ; groupe évincé par le jeu (limite d'IA actives) retiré en entier.
// Carte #69, livraison 3 — camions et radio :
// - l'ancien renfort motorisé (camion ou UAZ laissé garé à 300 m, SpawnMotorizedGroup, Board) est remplacé partout par
//   SRP_EnemyTruckComponent (SpawnWave garde son tirage) ; chauffeur de camion (SpawnVehicleDriver), débarqués en
//   défense (OrderDismountAndDefend), camion laissé sur place confié au nettoyage des transports (RegisterTransport) ;
// - l'hélicoptère sur alerte passe par la radio de la garnison (SRP_TerritoryComponent.TryRadioCall : opérateur radio
//   tué, appel retardé, refus HELICO [radio]).
// Cartes #75 (front) et #76 (Commandeur ennemi) — branchements :
// - place des soldats (Q7) : toute pose de fond passe par SRP_CmdCapacity (Ask avant, Tag après ; TagImportance à la fin
//   de chaque Spawn*) ; plafond d'éléments posé par SetMaxGroups ; file d'apparition (CountPendingSpawn) ; retrait hors de
//   vue de rondes et de jeeps lointaines pour faire place aux renforts (ReleaseForRoom, RE12) ;
// - patrouilles de fond côté rouge seulement, rares infiltrations côté bleu près du rouge (F10, SRP_FrontEnemyComponent) ;
//   menace LOCALE (SRP_Commander.ThreatAt) pour les patrouilles, les jeeps et les camions de vague (CO9) ;
// - ordres du Commandeur (tir de neutralisation, attaque d'un point, fumée, déplacement forcé, repli, approche),
//   équipage de blindé (SpawnVehicleCrew), IA tenue éveillée loin des joueurs (HoldAwake / ReleaseAwake) ;
// - comptes rendus au Commandeur (NoteSighting, NoteAirSighting) ; groupe disparu signalé aux manœuvres (OnGroupGone) ;
//   soldats envoyés rendus vivants à leur localité au retrait (ReturnSurvivors : RetireTick, DeleteAll d'une garnison,
//   camion rappelé ou annulé ; une seule fois par groupe) ;
// - hélicoptère sur alerte : avec un Commandeur (même gelé), il ne part plus que sur son ordre HELICO (AP7) ; sans lui,
//   l'ancien tirage, en territoire rouge (G12) ou près d'une mission.
// Utilisé par les livraisons (garnisons), les missions, le territoire, la présence ambiante, l'ennemi du front
// (SRP_FrontEnemyComponent) et le Commandeur (SRP_CmdCapacity, SRP_CmdManeuvers, SRP_CmdSupport, SRP_CmdArmor).
// Phase 4 — étape B
//------------------------------------------------------------------------------------------------

//! Gardes debout (carte #69, correctif du 25/09) : filtre des étiquettes de postes d'un preset de point Defend, appliqué
//! par SRP_EnemyComponent.KeepStandingPosts à chacun de nos points Defend juste après sa création (avant qu'il soit
//! donné au groupe : la recherche des actions intelligentes lit les étiquettes au démarrage de la défense)
[BaseContainerProps()]
modded class SCR_DefendWaypointPreset
{
	//------------------------------------------------------------------------------------------------
	//! Ne garde dans la recherche que les étiquettes présentes dans « keep » (ordre conservé) ; retourne le nombre
	//! d'étiquettes restantes, -1 s'il n'y avait aucune liste (preset créé par script : une liste vide lui est donnée, la
	//! lecture des étiquettes par le jeu ne recopie jamais une liste absente)
	int SRP_KeepTags(array<string> keep)
	{
		if (!m_aTagsForSearch)
		{
			m_aTagsForSearch = new array<string>();
			return -1;
		}
		for (int i = m_aTagsForSearch.Count() - 1; i >= 0; i--)
		{
			if (!keep || !keep.Contains(m_aTagsForSearch[i]))
				m_aTagsForSearch.RemoveOrdered(i);
		}
		return m_aTagsForSearch.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! Modificateur local de la présence ambiante (poste d'observation : x2 ; secteur calmé : x0 ; largage : x3)
class SRP_EnemyZoneModifier
{
	vector m_vCenter;
	float m_fRadius;
	float m_fFactor;
	int m_iUntilTick;		// 0 = tant qu'il n'est pas retiré
	string m_sTag;
}

//------------------------------------------------------------------------------------------------
//! Une alerte : des joueurs ont été repérés (vus ou entendus) à cet endroit. Tant qu'elle vit, l'ennemi cherche :
//! le groupe voisin vient aider, la jeep se rapproche, ratissage si le contact est perdu, des fusées éclairantes
//! montent la nuit.
class SRP_EnemyAlert
{
	vector m_vPosition;
	bool m_bPosSeen;			// m_vPosition vient d'un joueur VU (sinon entendu ou pas encore identifié)
	float m_fPosTime = -1;		// heure de perception du repérage qui a donné m_vPosition (-1 : inconnue)
	int m_iFirstTick;
	int m_iLastTick;			// dernier repérage
	int m_iLastFlareTick;
	int m_iFlares;
	bool m_bSeen;				// vus (sinon seulement entendus)
	bool m_bSearchSent;			// jeeps envoyées se poster
	bool m_bHeliRolled;			// le tirage de l'hélicoptère a eu lieu pour cette alerte (un seul par alerte)
	string m_sHeliRefusal;		// raisons de refus de l'hélicoptère déjà écrites au journal pour cette alerte ("[repos][joueurs]…")

	// Carte #69, livraison 2 : le groupe qui a donné l'alerte (l'entité, remise à null toute seule : jamais de pointeur
	// vers son enregistrement), les groupes venus aider, l'aide et le ratissage déjà envoyés pour ce contact, le début
	// du contact en cours (repris après une rupture de m_iContactLostSeconds), le dernier repérage déjà diffusé en
	// vigilance aux groupes voisins
	SCR_AIGroup m_SourceGroup;
	ref array<SCR_AIGroup> m_aHelpers = {};
	ref array<SCR_AIGroup> m_aSweepers = {};	// groupes (hors aidants) déjà envoyés ratisser pour cette alerte : repris
												// au ratissage suivant plutôt que d'en tirer de nouveaux
	bool m_bHelpSent;
	bool m_bSweepSent;
	int m_iContactTick;
	int m_iVigilantTick;
}

//------------------------------------------------------------------------------------------------
//! Un groupe posé par nous, avec ce qu'il faut pour le retirer
class SRP_EnemyGroup
{
	SCR_AIGroup m_Group;
	IEntity m_Waypoint;
	int m_iSpawnTick;
	string m_sOwner;		// "livraison", "mission", "ambiance"

	// Véhicule du groupe (jeep, hélicoptère, chauffeur d'un camion de renfort) et points de passage en plus (route,
	// débarquement, ronde en cycle)
	IEntity m_Vehicle;
	ref array<IEntity> m_aExtraWaypoints = {};

	// Patrouille en véhicule : un circuit de points (les lieux-dits autour du site), parcouru en boucle
	ref array<vector> m_aCircuit = {};
	int m_iCircuitIndex;
	int m_iSearchUntilTick;		// parti voir un lieu où des joueurs ont été repérés : ne reprend sa ronde qu'après

	// Patrouille de zone : le groupe reçoit un nouveau point de passage dans la zone à chaque étape
	bool m_bPatrol;
	vector m_vPatrolCenter;
	float m_fPatrolRadius;
	int m_iNextPatrolTick;
	int m_iArrivedTick;			// ronde à pied : arrivé à son point depuis (0 = en route), il marque une pause avant de repartir

	// Ronde sur les rues : un AIWaypoint_Cycle et ses points (m_aCircuit) ; le cycle est refait après une recherche d'alerte
	bool m_bCycle;

	// Reddition : effectif de départ (compté quelques secondes après la pose), et le tirage n'a lieu qu'une fois
	int m_iInitialAgents;
	bool m_bSurrenderRolled;

	// Survivants d'une mission finie : retirés après ce moment, et seulement hors de vue des joueurs
	int m_iRetireTick;

	// Carte #69 : la file d'apparition de la 1.8 livre les soldats un par un. Effectif attendu (emplacements du prefab ;
	// 1 pour un personnage isolé ; les pilotes assis pour l'hélicoptère), plus grand effectif vu, soldats retirés par
	// nous (places manquantes dans une jeep : ce ne sont pas des pertes), pose abandonnée (personne n'a été livré)
	int m_iExpected;
	int m_iMaxSeen;
	int m_iRemoved;
	bool m_bAbandoned;

	// Rôle CRX et maintien voulus (SRP_ECRXRole, -1 = aucun) : chaque soldat arrivé est réglé d'après eux
	int m_iRole = -1;
	vector m_vHold;
	float m_fHold = -1;

	// Repérages (id du joueur -> tick) : premier doute, dernière ligne de repérage, début d'une vue de près la nuit
	ref map<int, int> m_mDoubtTick = new map<int, int>();
	ref map<int, int> m_mSpottedTick = new map<int, int>();
	ref map<int, int> m_mRevealStart = new map<int, int>();
	int m_iLastReveal;		// dernière révélation de près, de nuit (une par délai réglé et par groupe)

	// Tirs de joueurs entendus par le groupe (id du joueur -> heure du gestionnaire de perception), relevés par
	// SCR_AIDangerReaction_WeaponFired.NotifyGroup : un tireur reste « trahi » même si le groupe l'a déjà identifié
	ref map<int, float> m_mShotHeard = new map<int, float>();

	// Carte #69, livraison 2 : tâche affichée (« centre », « ronde », « poste », « garde », « vague », « ambiance »,
	// « jeep », « helico », « officier », « assaut », « aide », « ratissage », « retour » ; garnisons de la livraison 3 :
	// « entrée », « bâtiment », « poste de nuit » ; front et Commandeur : « poste front », « blinde », « repli ») et tâche
	// d'origine ;
	// m_sZone : nom de la LOCALITÉ de la garnison (sirène, radio), vide hors garnison ; groupe de commandement (le centre
	// d'une défense : il ne part aider que s'il est le seul groupe libre, jamais ratisser)
	string m_sTask;
	string m_sHomeTask;
	string m_sZone;
	bool m_bCommand;

	// Poste d'origine, rétabli après l'aide ou le ratissage : point et rayon tenus, rôle, point Defend ou ronde en cycle
	vector m_vHome;
	float m_fHomeHold = -1;
	int m_iHomeRole = -1;
	bool m_bHomeDefend;
	bool m_bHomeCycle;

	// Occupé (aide, ratissage) jusqu'à ce moment (0 = libre) ; retour au poste au plus tard ; ratissage : centre et
	// prochain point ; vigilance d'une alerte voisine jusqu'à ce moment
	int m_iBusyUntilTick;
	int m_iReturnUntilTick;
	vector m_vSweepCenter;
	int m_iNextSweepTick;
	int m_iVigilantUntilTick;

	// Dernière lecture d'un joueur par ce groupe (AlertTick ; 0 = jamais) : un groupe au contact depuis moins de
	// SRP_EnemyAwareness.s_iContactLostMs est déjà au combat, il ne part ni aider ni ratisser
	int m_iSpotTick;

	// Carte #69, livraison 3 : ancien plafond de soldats des garnisons et des camions (champ gardé, plus lu : la place
	// est tenue par SRP_CmdCapacity, carte #76) ; ronde d'une garnison passée en poste pour la nuit (elle reprend sa
	// ronde au jour, SRP_EnemyGarrison.SwitchPosture)
	bool m_bBudget;
	bool m_bNightPost;

	// Carte #69, livraison 3 (correctifs) : la file d'apparition ne livre plus rien à ce groupe (grâce passée, ou
	// embarquement terminé) ; les soldats jamais livrés ne sont plus attendus
	bool m_bSpawnClosed;

	// Carte #75 (front) : zone visée par la contre-attaque dont le groupe fait partie (débarqués de camion compris), -1
	// sinon ; posé par SRP_FrontEnemyComponent, remis à -1 à la fin de l'attaque (lu par la capture et l'entraide)
	int m_iAssaultZone = -1;

	// Carte #76 (Commandeur) : classe de place (SRP_ECmdCapClass, posée par SRP_CmdCapacity.Tag ; -1 = devinée par
	// SRP_CmdCapacity.ClassOf) ; rôle donné par le Commandeur (SRP_ECmdRole, AUCUN = 0 : tout autre rôle exclut le groupe
	// de l'entraide) ; localité qui a envoyé ces soldats (ils lui sont rendus s'ils sont retirés vivants, RetireTick) ;
	// IA tenue éveillée loin des joueurs (HoldAwake) ; surveillance d'un groupe bloqué (dernière position, heure Unix)
	int m_iCapClass = -1;
	int m_iCmdRole;
	string m_sSourceLocality;
	bool m_bAwake;
	vector m_vCmdLast;
	int m_iCmdStillUnix;

	// Carte #76 (OF3) : tampon des comptes rendus du groupe au Commandeur, vidé par lui (SRP_Commander.NoteSighting,
	// NoteShot) : contact, joueur vu, joueurs perçus, position perçue, tirs entendus, véhicule, véhicule armé, prochain
	// compte rendu (heure Unix)
	bool m_bCmdContact;
	bool m_bCmdSeen;
	ref array<int> m_aCmdPlayers = {};
	vector m_vCmdPos;
	int m_iCmdShots;
	bool m_bCmdVehicle;
	bool m_bCmdArmed;
	int m_iCmdNextReport;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "IA ennemie SimpleRP : prefabs de groupes, spawn/nettoyage, présence ambiante")]
class SRP_EnemyComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_EnemyComponent : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Prefabs de groupes ennemis (Prefabs/Groups, tirés au hasard)", "et", category: "SimpleRP - Groupes")]
	protected ref array<ResourceName> m_aGroupPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Point de passage « Defend » (garnisons)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sDefendWaypointPrefab;

	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, "Point de passage « Move » (patrouilles, vagues)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sMoveWaypointPrefab;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, "Prefab de groupe IA vide (pour un personnage isolé : officier ennemi)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sEmptyGroupPrefab;

	[Attribute("USSR", UIWidgets.EditBox, "Clé de faction donnée aux personnages ennemis isolés", category: "SimpleRP - Groupes")]
	protected string m_sFactionKey;

	[Attribute("14", UIWidgets.EditBox, "Plafond global de groupes ennemis vivants (livraisons + missions + ambiance). Remplacé au démarrage du front par cap_groupes_max du fichier commandeur_reglages.txt (SetMaxGroups)", category: "SimpleRP - Groupes")]
	protected int m_iMaxGroups;

	[Attribute("6", UIWidgets.EditBox, "Plafond de groupes : … groupes toujours gardés libres pour les MISSIONS (gardes, vagues, officier, camions de vague). Garnisons, camions de territoire, jeeps de territoire et patrouilles ambiantes s'arrêtent avant (carte #69 : une garnison en sections et ses camions prennent beaucoup de groupes). Remplacé au démarrage du front par cap_groupes_reserve_missions (SetMaxGroups)", category: "SimpleRP - Groupes")]
	protected int m_iMissionGroupReserve;

	[Attribute("", UIWidgets.ResourceNamePicker, "Garnison d'une VILLE : groupe de commandement (QG de section, avec son opérateur radio et son tireur d'élite ; le tireur d'élite n'existe que là). Jamais tiré au hasard ailleurs.", "et", category: "SimpleRP - Garnison (groupes)")]
	protected ResourceName m_sCommandGroupPrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Sections de 4 ou 6 soldats (garnisons, gardes et vagues de mission) ; la taille est lue dans le prefab", "et", category: "SimpleRP - Garnison (groupes)")]
	protected ref array<ResourceName> m_aSectionPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Binômes de sentinelles (postes d'entrée des garnisons, reste de 2 soldats)", "et", category: "SimpleRP - Garnison (groupes)")]
	protected ref array<ResourceName> m_aSentryPrefabs;

	[Attribute("100", UIWidgets.EditBox, "Ancien plafond de soldats des garnisons et des camions : NE SERT PLUS (carte #76 : la place de tous les soldats ennemis au sol est cap_soldats_max du fichier commandeur_reglages.txt, 120)", category: "SimpleRP - Garnison (groupes)")]
	protected int m_iMaxSoldiers;

	[Attribute("4", UIWidgets.EditBox, "Ancien réglage (effectif selon les joueurs CONNECTÉS) : ne sert plus, l'effectif suit les joueurs PROCHES, voir « Prorata »", category: "SimpleRP - Groupes")]
	protected int m_iPlayersPerExtraGroup;

	[Attribute("2", UIWidgets.EditBox, "Un groupe ennemi par tranche de … joueurs proches du lieu (1 à 2 joueurs = 1 groupe, 3 à 4 = 2 groupes…)", category: "SimpleRP - Prorata")]
	protected int m_iPlayersPerGroup;

	[Attribute("3000", UIWidgets.EditBox, "Un joueur compte comme « proche » à moins de … mètres du lieu (les joueurs restés à la base ne comptent jamais)", category: "SimpleRP - Prorata")]
	protected float m_fScaleRadius;

	[Attribute("2", UIWidgets.EditBox, "Budget local : groupes ennemis tolérés autour d'un même lieu, EN PLUS du prorata (garnison + mission + vagues + patrouilles confondues)", category: "SimpleRP - Prorata")]
	protected int m_iLocalBonus;

	[Attribute("8", UIWidgets.EditBox, "Budget local : plafond absolu de groupes ennemis autour d'un même lieu", category: "SimpleRP - Prorata")]
	protected int m_iLocalMax;

	[Attribute("1200", UIWidgets.EditBox, "Budget local : rayon du « même lieu », en mètres", category: "SimpleRP - Prorata")]
	protected float m_fLocalRadius;

	[Attribute("1", UIWidgets.CheckBox, "Renforts en camion : une vague (mission, contre-attaque sans secteur voisin) peut arriver en camion (SRP_EnemyTruckComponent : garé à 150-250 m du site, tout le monde à l'assaut, le camion repart et disparaît hors de vue)", category: "SimpleRP - Motorisé")]
	protected bool m_bMotorized;

	[Attribute("3", UIWidgets.EditBox, "Pas de jeep de patrouille à moins de … joueurs proches (les camions de renfort ont leur propre seuil, SRP_EnemyTruckComponent)", category: "SimpleRP - Motorisé")]
	protected int m_iMotorizedMinPlayers;

	[Attribute("50", UIWidgets.EditBox, "Chance qu'une vague qui peut venir en camion le fasse, en pour cent (multipliée par le facteur de menace)", category: "SimpleRP - Motorisé")]
	protected int m_iMotorizedChance;

	[Attribute("{C40316EE26846CAB}Prefabs/AI/Waypoints/AIWaypoint_GetOut.et", UIWidgets.ResourceNamePicker, "Point de passage « GetOut » (débarquement)", "et", category: "SimpleRP - Motorisé")]
	protected ResourceName m_sGetOutWaypointPrefab;

	[Attribute("20", UIWidgets.EditBox, "Un véhicule laissé sur place (camion de renfort pris en embuscade ou renversé, épave, jeep) est retiré après … minutes sans joueur à moins de 300 m", category: "SimpleRP - Motorisé")]
	protected int m_iTransportCleanupMinutes;

	[Attribute("1", UIWidgets.CheckBox, "Patrouilles en jeep armée : une jeep fait le tour des lieux-dits autour d'un secteur ou d'une mission gardés", category: "SimpleRP - Motorisé")]
	protected bool m_bVehiclePatrols;

	[Attribute("", UIWidgets.ResourceNamePicker, "Jeeps armées de patrouille (vide = UAZ-469 PKM du jeu de base)", "et", category: "SimpleRP - Motorisé")]
	protected ref array<ResourceName> m_aPatrolVehiclePrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Groupe d'équipage de la jeep (2 ou 3 soldats ; vide = un groupe tiré au hasard, les soldats en trop sont retirés)", "et", category: "SimpleRP - Motorisé")]
	protected ResourceName m_sPatrolCrewPrefab;

	[Attribute("35", UIWidgets.EditBox, "Chance qu'un lieu gardé reçoive sa jeep de patrouille, en % (il faut le nombre minimum de joueurs des renforts motorisés)", category: "SimpleRP - Motorisé")]
	protected int m_iVehiclePatrolChance;

	[Attribute("2", UIWidgets.EditBox, "Jeeps de patrouille vivantes en même temps, maximum", category: "SimpleRP - Motorisé")]
	protected int m_iVehiclePatrolMax;

	[Attribute("1.3", UIWidgets.EditBox, "Perception d'un ennemi TRANQUILLE (formule SimpleRP ; jeu de base 1.0/2.5/2.5/0.4, CRX 1.0/2.5/2.5/3.0). Plus bas = on progresse plus facilement à couvert, allongé ou accroupi", category: "SimpleRP - Perception")]
	protected float m_fPerceptionSafe;

	[Attribute("2.5", UIWidgets.EditBox, "Perception d'un ennemi VIGILANT, qui a un doute (formule SimpleRP ; jeu de base 1.0/2.5/2.5/0.4, CRX 1.0/2.5/2.5/3.0)", category: "SimpleRP - Perception")]
	protected float m_fPerceptionVigilant;

	[Attribute("2.5", UIWidgets.EditBox, "Perception d'un ennemi ALERTÉ, qui cherche (formule SimpleRP ; jeu de base 1.0/2.5/2.5/0.4, CRX 1.0/2.5/2.5/3.0)", category: "SimpleRP - Perception")]
	protected float m_fPerceptionAlerted;

	[Attribute("3.0", UIWidgets.EditBox, "Perception d'un ennemi MENACÉ, au combat (formule SimpleRP ; jeu de base 1.0/2.5/2.5/0.4, CRX 1.0/2.5/2.5/3.0)", category: "SimpleRP - Perception")]
	protected float m_fPerceptionThreatened;

	[Attribute("1", UIWidgets.CheckBox, "Les patrouilles allument leurs lampes la nuit (on les voit venir)", category: "SimpleRP - Perception")]
	protected bool m_bPatrolFlashlights;

	[Attribute("1.4", UIWidgets.EditBox, "Rôle : une sentinelle (garde en place) voit … fois mieux (multiplie sa perception)", category: "SimpleRP - Perception")]
	protected float m_fSentryFactor;

	[Attribute("0.9", UIWidgets.EditBox, "Rôle : une patrouille qui marche voit … fois", category: "SimpleRP - Perception")]
	protected float m_fPatrolFactor;

	[Attribute("1.1", UIWidgets.EditBox, "Rôle : un groupe d'assaut (vague, renfort) voit … fois", category: "SimpleRP - Perception")]
	protected float m_fAssaultFactor;

	[Attribute("0.8", UIWidgets.EditBox, "Rôle : un soldat assis dans un véhicule voit … fois", category: "SimpleRP - Perception")]
	protected float m_fCrewFactor;

	[Attribute("0.7", UIWidgets.EditBox, "Cible accroupie : sa visibilité est multipliée par … (debout = 1 ; plan : accroupi repéré vers 200 m)", category: "SimpleRP - Perception")]
	protected float m_fCrouchTargetFactor;

	[Attribute("0.4", UIWidgets.EditBox, "Cible allongée : sa visibilité est multipliée par … (debout = 1 ; le jeu la réduit déjà selon sa taille ; plan : allongé repéré vers 50-100 m)", category: "SimpleRP - Perception")]
	protected float m_fProneTargetFactor;

	[Attribute("2.0", UIWidgets.EditBox, "Un ennemi qui vise dans une lunette grossissante voit … fois mieux (comme CRX : 2 ; 1 = sans effet)", category: "SimpleRP - Perception")]
	protected float m_fOpticsFactor;

	[Attribute("60", UIWidgets.EditBox, "Filet : un soldat assis ou adossé qui voit un joueur à moins de … mètres se relève aussitôt, si le joueur est devant lui ou déjà dans sa perception (0 = jamais)", category: "SimpleRP - Perception")]
	protected float m_fLoiterWakeDistance;

	[Attribute("0", UIWidgets.CheckBox, "Débogage : toutes les 10 s, une ligne ENNEMI par soldat à moins de 150 m d'un joueur (état, facteur posé, ligne de vue, cap, assis, perception, LOD, comportement). Toujours actif en MODE ESSAI du Workbench. Hors mode essai : 10 lignes par passage au plus, à réserver à une courte séance de diagnostic (chaque ligne va au journal et au terminal Staff).", category: "SimpleRP - Perception")]
	protected bool m_bPerceptionDebug;

	[Attribute("0.3", UIWidgets.EditBox, "Un ennemi allongé voit moins bien : … retiré à sa perception (comme CRX)", category: "SimpleRP - Perception")]
	protected float m_fProneObserverPenalty;

	[Attribute("750", UIWidgets.EditBox, "Environnement : joueurs pris en compte à moins de … mètres d'un soldat ennemi", category: "SimpleRP - Perception")]
	protected float m_fSensesRange;

	[Attribute("250", UIWidgets.EditBox, "Environnement : si des joueurs sont à moins de … mètres d'un soldat, seuls ceux-là comptent (un joueur éclairé au loin n'aide pas un joueur sombre tout près)", category: "SimpleRP - Perception")]
	protected float m_fConcealCheckRange;

	[Attribute("2000", UIWidgets.EditBox, "Sens de l'ennemi : période de mise à jour, en millisecondes", category: "SimpleRP - Perception")]
	protected int m_iSensesTickMs;

	[Attribute("-3.5", UIWidgets.EditBox, "Lumière ambiante (LV) au-dessus de laquelle il fait plein jour pour l'ennemi (seuil du jeu : -3.5)", category: "SimpleRP - Perception")]
	protected float m_fLightFullLV;

	[Attribute("-7.0", UIWidgets.EditBox, "Lumière ambiante (LV) en dessous de laquelle il fait nuit noire pour l'ennemi", category: "SimpleRP - Perception")]
	protected float m_fLightDarkLV;

	[Attribute("0.3", UIWidgets.EditBox, "Nuit noire : la visibilité d'un joueur non éclairé est multipliée par … (entre les deux seuils, progression linéaire)", category: "SimpleRP - Perception")]
	protected float m_fNightMinFactor;

	[Attribute("0.5", UIWidgets.EditBox, "Un joueur est « éclairé » (vu comme de jour) à partir de ce facteur d'éclairage du jeu (lampadaire, projecteur, phares)", category: "SimpleRP - Perception")]
	protected float m_fLitIllumination;

	[Attribute("1", UIWidgets.CheckBox, "Une lampe allumée trahit le joueur la nuit (il est vu comme de jour)", category: "SimpleRP - Perception")]
	protected bool m_bFlashlightReveals;

	[Attribute("5", UIWidgets.EditBox, "Un joueur entendu (tir) par un groupe est vu comme de jour par ce groupe pendant … secondes", category: "SimpleRP - Perception")]
	protected int m_iShotRevealSeconds;

	[Attribute("350", UIWidgets.EditBox, "Fusée éclairante : les joueurs à moins de … mètres (à plat) sont éclairés", category: "SimpleRP - Perception")]
	protected float m_fFlareLightRadius;

	[Attribute("60", UIWidgets.EditBox, "Fusée éclairante : elle éclaire pendant … secondes", category: "SimpleRP - Perception")]
	protected int m_iFlareLightSeconds;

	[Attribute("40", UIWidgets.EditBox, "Nuit : à moins de … mètres d'un soldat, un joueur non éclairé se voit quand même", category: "SimpleRP - Perception")]
	protected float m_fNightCloseDistance;

	[Attribute("0.8", UIWidgets.EditBox, "Nuit : de si près, sa visibilité est multipliée par … au moins", category: "SimpleRP - Perception")]
	protected float m_fNightCloseFactor;

	[Attribute("1", UIWidgets.CheckBox, "Nuit : un joueur vu de près assez longtemps est signalé à tout le groupe", category: "SimpleRP - Perception")]
	protected bool m_bNightReveal;

	[Attribute("4", UIWidgets.EditBox, "Nuit : il faut le voir de près pendant … secondes", category: "SimpleRP - Perception")]
	protected int m_iNightRevealSeconds;

	[Attribute("30", UIWidgets.EditBox, "Nuit : une révélation au plus toutes les … secondes par groupe", category: "SimpleRP - Perception")]
	protected int m_iNightRevealCooldownSeconds;

	[Attribute("1", UIWidgets.CheckBox, "Végétation : un joueur accroupi, allongé ou immobile dans un buisson est repéré plus lentement (EXPÉRIMENTAL : décocher si ça marche mal)", category: "SimpleRP - Perception")]
	protected bool m_bVegetationSlowdown;

	[Attribute("0.5", UIWidgets.EditBox, "Végétation : dans un buisson, sa visibilité (ennemi tranquille ou vigilant) est multipliée par …", category: "SimpleRP - Perception")]
	protected float m_fBushFactor;

	[Attribute("0.8", UIWidgets.EditBox, "Végétation : hauteur minimale d'une plante qui cache, en mètres (si le jeu ne dit pas que c'est un buisson)", category: "SimpleRP - Perception")]
	protected float m_fBushMinHeight;

	[Attribute("4", UIWidgets.EditBox, "Végétation : hauteur maximale d'une plante qui cache, en mètres (au-delà c'est un arbre)", category: "SimpleRP - Perception")]
	protected float m_fBushMaxHeight;

	[Attribute("0.5", UIWidgets.EditBox, "Végétation : on est dans le buisson jusqu'à … mètres au-delà de sa demi-largeur", category: "SimpleRP - Perception")]
	protected float m_fBushReach;

	[Attribute("0.5", UIWidgets.EditBox, "Végétation : un joueur debout compte comme immobile sous … mètres par seconde", category: "SimpleRP - Perception")]
	protected float m_fBushStillSpeed;

	[Attribute("1.0", UIWidgets.EditBox, "Tir sous le feu : la dispersion d'un ennemi supprimé est multipliée jusqu'à 1 + … (0 = comme CRX ; 1,5 avant le 25/09)", category: "SimpleRP - Perception")]
	protected float m_fSuppressionAim;

	[Attribute("1.0", UIWidgets.EditBox, "Visée de base d'un soldat que CRX n'a pas classé (officier isolé, groupe pas encore ou jamais complet) : erreur de visée (CRX : 1.3 pour un soldat régulier ; 1,3 avant le 25/09)", category: "SimpleRP - Perception")]
	protected float m_fAimDefault;

	[Attribute("0.7", UIWidgets.EditBox, "Précision de tir : l'écart de chaque tir de nos soldats est multiplié par … (1 = comme CRX ; 0,7 = 30 pour cent plus précis, demandé par Jack le 25/09)", category: "SimpleRP - Perception")]
	protected float m_fAimSpreadFactor;

	[Attribute("60", UIWidgets.EditBox, "Journal de repérage : une ligne au plus toutes les … secondes par groupe et par joueur", category: "SimpleRP - Perception")]
	protected int m_iSightingLogSeconds;

	[Attribute("1", UIWidgets.CheckBox, "Alerte : quand des joueurs sont repérés, le groupe voisin le plus proche vient aider (par le flanc), la jeep se rapproche, et le contact perdu est ratissé", category: "SimpleRP - Alerte")]
	protected bool m_bAlertSearch;

	[Attribute("6", UIWidgets.EditBox, "Alerte : elle s'éteint après … minutes sans nouveau repérage, l'ennemi reprend ses rondes", category: "SimpleRP - Alerte")]
	protected int m_iAlertMinutes;

	[Attribute("20", UIWidgets.EditBox, "Aide : le groupe voisin part aider … secondes après le premier repérage (la jeep se poste au même moment)", category: "SimpleRP - Alerte")]
	protected int m_iHelpDelaySeconds;

	[Attribute("600", UIWidgets.EditBox, "Aide : seuls les groupes à moins de … mètres du contact peuvent venir aider", category: "SimpleRP - Alerte")]
	protected float m_fHelpRadius;

	[Attribute("1", UIWidgets.EditBox, "Aide : nombre de groupes envoyés (les plus proches ; jamais le groupe qui a donné l'alerte, ni le centre d'une défense, ni un véhicule)", category: "SimpleRP - Alerte")]
	protected int m_iHelpGroups;

	[Attribute("1", UIWidgets.CheckBox, "Aide : le groupe arrive par le flanc, par un point hors de vue du contact (EXPÉRIMENTAL : décocher pour l'approche directe)", category: "SimpleRP - Alerte")]
	protected bool m_bFlanking;

	[Attribute("75", UIWidgets.EditBox, "Aide : angle du contournement par rapport à l'axe du groupe qui tient le contact, en degrés", category: "SimpleRP - Alerte")]
	protected float m_fFlankAngle;

	[Attribute("60", UIWidgets.EditBox, "Aide : le point de contournement est à … mètres du contact au moins", category: "SimpleRP - Alerte")]
	protected float m_fFlankMin;

	[Attribute("150", UIWidgets.EditBox, "… et à … mètres au plus (la moitié de la distance du groupe au contact, bornée)", category: "SimpleRP - Alerte")]
	protected float m_fFlankMax;

	[Attribute("60", UIWidgets.EditBox, "Ratissage : il commence quand le contact est perdu depuis … secondes (un nouveau repérage après ce délai relance l'aide)", category: "SimpleRP - Alerte")]
	protected int m_iContactLostSeconds;

	[Attribute("10", UIWidgets.EditBox, "Ratissage : durée, en minutes, puis retour au poste ou à la ronde", category: "SimpleRP - Alerte")]
	protected int m_iSweepMinutes;

	[Attribute("2", UIWidgets.EditBox, "Ratissage : groupes au total (ceux venus aider d'abord, puis les plus proches)", category: "SimpleRP - Alerte")]
	protected int m_iSweepGroups;

	[Attribute("800", UIWidgets.EditBox, "Ratissage : les groupes à moins de … mètres de la dernière position connue peuvent s'y joindre", category: "SimpleRP - Alerte")]
	protected float m_fSweepJoinRadius;

	[Attribute("120", UIWidgets.EditBox, "Ratissage : rayon fouillé autour de la dernière position connue, en mètres", category: "SimpleRP - Alerte")]
	protected float m_fSweepRadius;

	[Attribute("90", UIWidgets.EditBox, "Ratissage : un nouveau point fouillé toutes les … secondes", category: "SimpleRP - Alerte")]
	protected int m_iSweepStepSeconds;

	[Attribute("20", UIWidgets.EditBox, "Aide : un groupe parti aider revient à son poste au plus tard après … minutes", category: "SimpleRP - Alerte")]
	protected int m_iBusyMaxMinutes;

	[Attribute("150", UIWidgets.EditBox, "Alerte : la jeep de patrouille (à moins de 2 000 m) vient se poster à … mètres de la dernière position connue", category: "SimpleRP - Alerte")]
	protected float m_fJeepPostDistance;

	[Attribute("1.5", UIWidgets.EditBox, "Vigilance : pendant une alerte, les soldats TRANQUILLES des groupes proches voient … fois mieux, sur place, sans quitter leur poste (1 = sans effet)", category: "SimpleRP - Alerte")]
	protected float m_fVigilantFactor;

	[Attribute("5", UIWidgets.EditBox, "Vigilance : elle dure … minutes après le dernier repérage", category: "SimpleRP - Alerte")]
	protected int m_iVigilantMinutes;

	[Attribute("800", UIWidgets.EditBox, "Vigilance : les groupes à moins de … mètres du contact deviennent vigilants", category: "SimpleRP - Alerte")]
	protected float m_fVigilantRadius;

	[Attribute("1", UIWidgets.CheckBox, "Fusées éclairantes blanches, la nuit, au-dessus du lieu où des joueurs ont été repérés", category: "SimpleRP - Alerte")]
	protected bool m_bFlares;

	[Attribute("{5964C8C1CCB57D93}Prefabs/Weapons/Ammo/FlareEffect_82mm_S832S_White.et", UIWidgets.ResourceNamePicker, "Fusée éclairante (effet déjà déployé, sous parachute)", "et", category: "SimpleRP - Alerte")]
	protected ResourceName m_sFlarePrefab;

	[Attribute("50", UIWidgets.EditBox, "Secondes entre deux fusées sur une même alerte", category: "SimpleRP - Alerte")]
	protected int m_iFlareIntervalSeconds;

	[Attribute("5", UIWidgets.EditBox, "Fusées maximum par alerte", category: "SimpleRP - Alerte")]
	protected int m_iFlareMax;

	[Attribute("170", UIWidgets.EditBox, "Hauteur d'allumage d'une fusée, en mètres au-dessus du sol", category: "SimpleRP - Alerte")]
	protected float m_fFlareHeight;

	[Attribute("{5BF04078A1EC66D8}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MTT-A_Black_no_pylons.et", UIWidgets.ResourceNamePicker, "Hélicoptère de recherche (Mi-8 RHS noir sans pylônes par défaut)", "et", category: "SimpleRP - Hélicoptère")]
	protected ResourceName m_sHeliPrefab;

	[Attribute("120", UIWidgets.EditBox, "Hélicoptère : hauteur de vol au-dessus du sol, en mètres", category: "SimpleRP - Hélicoptère")]
	protected float m_fHeliAltitude;

	[Attribute("250", UIWidgets.EditBox, "Hélicoptère : rayon des cercles au-dessus du lieu, en mètres (resserré à 60 % sur une alerte)", category: "SimpleRP - Hélicoptère")]
	protected float m_fHeliRadius;

	[Attribute("35", UIWidgets.EditBox, "Hélicoptère : vitesse, en mètres par seconde (35 = 126 km/h)", category: "SimpleRP - Hélicoptère")]
	protected float m_fHeliSpeed;

	[Attribute("6", UIWidgets.EditBox, "Hélicoptère : minutes passées sur zone avant de repartir", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliMinutes;

	[Attribute("10", UIWidgets.EditBox, "Hélicoptère : force du roulis en virage (10 = environ 16° sur le cercle de recherche, 0 = vol à plat)", category: "SimpleRP - Hélicoptère")]
	protected float m_fHeliBank;

	[Attribute("1", UIWidgets.CheckBox, "MODE ESSAI, dans le Workbench seulement : un seul joueur suffit et les tirages réussissent toujours (hélicoptère sur alerte, jeep de patrouille, renfort en camion), repos de l'hélicoptère ramené à 2 min. Sans aucun effet en jeu publié ni sur le serveur.", category: "SimpleRP - Hélicoptère")]
	protected bool m_bWorkbenchTestMode;

	[Attribute("1", UIWidgets.CheckBox, "Hélicoptère sur alerte, SANS Commandeur seulement (avec lui, l'hélicoptère ne part que sur son ordre HELICO, AP7) : quand des joueurs sont VUS près d'une mission active ou en territoire rouge, l'ennemi peut faire venir son hélicoptère de recherche", category: "SimpleRP - Hélicoptère")]
	protected bool m_bHeliOnAlert;

	// Chances, pas et repos du tirage de l'hélicoptère : remplacés par le Commandeur, AP7 (lus seulement sans Commandeur)
	[Attribute("10", UIWidgets.EditBox, "Hélicoptère sur alerte (sans Commandeur) : chance de JOUR, en pour cent, au premier tirage (un tirage par alerte ; chaque tirage raté ajoute le pas ci-dessous au suivant)", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliChance;

	[Attribute("25", UIWidgets.EditBox, "Hélicoptère sur alerte (sans Commandeur) : chance de NUIT, en pour cent, au premier tirage", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliChanceNight;

	[Attribute("15", UIWidgets.EditBox, "Hélicoptère sur alerte (sans Commandeur) : chaque tirage raté ajoute … points de chance au suivant (compteur remis à zéro à chaque sortie)", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliChanceStep;

	[Attribute("70", UIWidgets.EditBox, "Hélicoptère sur alerte (sans Commandeur) : chance maximale, en pour cent, quel que soit le nombre de tirages ratés", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliChanceMax;

	[Attribute("2", UIWidgets.EditBox, "Hélicoptère sur alerte : joueurs minimum dans les 3 km du lieu (mettre 1 pour essayer seul)", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliMinPlayers;

	[Attribute("30", UIWidgets.EditBox, "Hélicoptère sur alerte (sans Commandeur) : minutes de repos entre deux sorties", category: "SimpleRP - Hélicoptère")]
	protected int m_iHeliCooldownMinutes;

	[Attribute("900", UIWidgets.EditBox, "Hélicoptère sur alerte (sans Commandeur) : hors territoire rouge, l'alerte doit être à moins de … mètres d'une mission active (son site ou le centre de sa garde). En territoire rouge, l'alerte suffit (G12) : cette marge ne vaut plus que pour les missions", category: "SimpleRP - Hélicoptère")]
	protected float m_fHeliSiteDistance;

	[Attribute("{38D59019ABF4D5F6}Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Character_RHS_RF_MSV_VKPO_DS_HeliPilot.et", UIWidgets.ResourceNamePicker, "Hélicoptère : pilote (deux sont créés directement à bord de l'appareil ; vide = l'ancien équipage, un groupe posé au sol puis embarqué)", "et", category: "SimpleRP - Hélicoptère")]
	protected ResourceName m_sHeliPilotPrefab;

	[Attribute("20.5", UIWidgets.EditBox, "Secours si le jeu ne sait pas dire s'il fait nuit : la nuit commence à … heures", category: "SimpleRP - Alerte")]
	protected float m_fNightStart;

	[Attribute("5.5", UIWidgets.EditBox, "… et finit à … heures", category: "SimpleRP - Alerte")]
	protected float m_fNightEnd;

	[Attribute("2", UIWidgets.EditBox, "Présence ambiante : chance par minute et par joueur qu'une patrouille apparaisse, en % (rare)", category: "SimpleRP - Ambiance")]
	protected float m_fAmbientChance;

	[Attribute("15", UIWidgets.EditBox, "Même chance quand le joueur est à moins de « distance d'une mission » d'une mission active, en %", category: "SimpleRP - Ambiance")]
	protected float m_fMissionChance;

	[Attribute("1500", UIWidgets.EditBox, "Distance d'une mission active pour la chance renforcée, en mètres", category: "SimpleRP - Ambiance")]
	protected float m_fMissionRadius;

	[Attribute("600", UIWidgets.EditBox, "Distance minimale d'apparition d'une patrouille par rapport aux joueurs, en mètres", category: "SimpleRP - Ambiance")]
	protected float m_fSpawnMinDistance;

	[Attribute("1200", UIWidgets.EditBox, "Distance maximale d'apparition, en mètres", category: "SimpleRP - Ambiance")]
	protected float m_fSpawnMaxDistance;

	[Attribute("1500", UIWidgets.EditBox, "Aucune patrouille à moins de … mètres de la base", category: "SimpleRP - Ambiance")]
	protected float m_fBaseSafeRadius;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur de la base", category: "SimpleRP - Ambiance")]
	protected string m_sBaseMarkerName;

	[Attribute("4", UIWidgets.EditBox, "Patrouilles ambiantes vivantes en même temps, maximum", category: "SimpleRP - Ambiance")]
	protected int m_iAmbientMax;

	[Attribute("45", UIWidgets.EditBox, "Une patrouille est retirée après … minutes", category: "SimpleRP - Ambiance")]
	protected int m_iAmbientLifetimeMinutes;

	[Attribute("2500", UIWidgets.EditBox, "Une patrouille sans joueur à moins de … mètres est retirée", category: "SimpleRP - Ambiance")]
	protected float m_fDespawnDistance;

	[Attribute("50", UIWidgets.EditBox, "Part des patrouilles ambiantes qui suivent une route entre deux lieux-dits (ronde continue) au lieu de venir vers le joueur, en %", category: "SimpleRP - Ambiance")]
	protected int m_iAmbientRoadPercent;

	[Attribute("{35BD6541CBB8AC08}Prefabs/AI/Waypoints/AIWaypoint_Cycle.et", UIWidgets.ResourceNamePicker, "Point de passage « Cycle » (rondes sur les rues d'un lieu)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sCycleWaypointPrefab;

	[Attribute("{B3E7B8DC2BAB8ACC}Prefabs/AI/Waypoints/AIWaypoint_SearchAndDestroy.et", UIWidgets.ResourceNamePicker, "Point de passage « SearchAndDestroy » (escorte d'un convoi débarquée au contact)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sSearchDestroyWaypointPrefab;

	// Carte #76 : points de passage des ordres du Commandeur (prefabs du jeu, GUID relevés dans ScenarioFramework/Waypoints)
	[Attribute("{ED8277F35B46B4AA}Prefabs/AI/Waypoints/AIWaypoint_Suppress.et", UIWidgets.ResourceNamePicker, "Point de passage « Suppress » (tir de neutralisation ordonné par le Commandeur)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sSuppressWaypointPrefab;

	[Attribute("{1B0E3436C30FA211}Prefabs/AI/Waypoints/AIWaypoint_Attack.et", UIWidgets.ResourceNamePicker, "Point de passage « Attack » (attaque d'un point, sans cible désignée)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sAttackWaypointPrefab;

	[Attribute("{CE97215CE55CF734}Prefabs/AI/Waypoints/AIWaypoint_DeploySmokeCover.et", UIWidgets.ResourceNamePicker, "Point de passage « DeploySmokeCover » (fumigènes à main pour couvrir un point)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sSmokeWaypointPrefab;

	[Attribute("{06E1B6EBD480C6E0}Prefabs/AI/Waypoints/AIWaypoint_ForcedMove.et", UIWidgets.ResourceNamePicker, "Point de passage « ForcedMove » (déplacement prioritaire : repli, sortie d'une localité)", "et", category: "SimpleRP - Groupes")]
	protected ResourceName m_sForcedMoveWaypointPrefab;

	[Attribute("3", UIWidgets.EditBox, "Ronde sur les rues : points de route minimum", category: "SimpleRP - Patrouilles")]
	protected int m_iStreetPointsMin;

	[Attribute("5", UIWidgets.EditBox, "Ronde sur les rues : points de route maximum", category: "SimpleRP - Patrouilles")]
	protected int m_iStreetPointsMax;

	[Attribute("3", UIWidgets.EditBox, "Délai de grâce : un joueur qui vient d'apparaître (connexion, réapparition) ne compte pas comme « proche » pendant … minutes (pose des garnisons et des vagues)", category: "SimpleRP - Prorata")]
	protected int m_iGraceMinutes;

	[Attribute("15", UIWidgets.EditBox, "Survivants d'une mission finie : retirés après … minutes, et seulement hors de vue des joueurs", category: "SimpleRP - Groupes")]
	protected int m_iRetireMinutes;

	[Attribute("180", UIWidgets.EditBox, "Pose : un groupe encore vide (la file d'apparition du jeu livre les soldats un par un) est gardé … secondes ; toujours vide ensuite, la pose est abandonnée", category: "SimpleRP - Groupes")]
	protected int m_iSpawnGraceSeconds;

	[Attribute("30", UIWidgets.EditBox, "Officier ennemi isolé : il tient son poste dans un rayon de … mètres", category: "SimpleRP - Groupes")]
	protected float m_fOfficerHoldRadius;

	[Attribute("0", UIWidgets.CheckBox, "Reddition : un groupe réduit à 1 ou 2 hommes en état de combattre, avec un joueur à portée, peut se rendre (ACE Captives). Retirée à la demande de Jack le 25/09 (décochée) : les ennemis se battent jusqu'au bout", category: "SimpleRP - Reddition")]
	protected bool m_bSurrender;

	[Attribute("50", UIWidgets.EditBox, "Reddition : chance, en % (un seul tirage par groupe)", category: "SimpleRP - Reddition")]
	protected int m_iSurrenderChance;

	[Attribute("30", UIWidgets.EditBox, "Reddition : il faut un joueur à moins de … mètres d'un des survivants", category: "SimpleRP - Reddition")]
	protected float m_fSurrenderDistance;

	[Attribute("30", UIWidgets.EditBox, "Un soldat qui s'est rendu est retiré après … minutes sans joueur à moins de 500 m", category: "SimpleRP - Reddition")]
	protected int m_iSurrenderedCleanupMinutes;

	protected static SRP_EnemyComponent s_Instance;
	protected static bool s_bTestMode;			// mode essai actif : les lignes de journal portent le préfixe [ESSAI]
	protected static int s_iSpawnGraceMs = 180000;	// grâce d'un groupe encore vide (m_iSpawnGraceSeconds), pour les méthodes statiques

	protected ref array<ref SRP_EnemyGroup> m_aAll = {};		// tout ce qui a été posé, pour le plafond
	protected ref array<ref SRP_EnemyGroup> m_aAmbient = {};	// les patrouilles ambiantes, pour leur entretien
	protected ref array<ref SRP_EnemyGroup> m_aRetiring = {};	// survivants de missions finies, à retirer après délai et hors de vue
	protected ref array<ref SRP_EnemyZoneModifier> m_aModifiers = {};
	protected ref array<ref SRP_EnemyAlert> m_aAlerts = {};
	protected int m_iNextSpottedOrderTick;
	protected int m_iNextHeliTick;				// pas de nouvelle sortie de l'hélicoptère avant
	protected int m_iHeliMisses;				// tirages de l'hélicoptère ratés de suite (la chance monte ; 0 à chaque sortie)
	protected bool m_bHeliForceNext;			// Staff : le prochain tirage est gagné et le repos ignoré (puis le drapeau retombe)
	protected ref array<IEntity> m_aTransports = {};			// camions de renfort posés, pour leur retrait
	protected ref array<int> m_aTransportIdleSince = {};		// depuis quand chacun est sans joueur à proximité (0 = un joueur est près)
	protected ref map<int, int> m_mGraceUntil = new map<int, int>();	// playerId -> fin du délai de grâce (tick)
	protected ref array<IEntity> m_aSurrendered = {};			// soldats qui se sont rendus (hors de leur groupe), pour leur retrait
	protected ref array<int> m_aSurrenderedIdleSince = {};
	protected ref map<ResourceName, int> m_mGroupSizes = new map<ResourceName, int>();	// prefab de groupe -> soldats (lu une fois)

	//------------------------------------------------------------------------------------------------
	static SRP_EnemyComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Journal de l'IA ennemie : en mode essai (Workbench), chaque ligne porte le préfixe [ESSAI]
	static void Journal(string category, string text)
	{
		if (s_bTestMode)
			SRP_JournalComponent.Log(category, "[ESSAI] " + text);
		else
			SRP_JournalComponent.Log(category, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Le mode essai est-il actif ? (Territoire et missions : préfixe [ESSAI] de leurs lignes)
	static bool IsTestMode()
	{
		return s_bTestMode;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		s_Instance = this;

		// Nouvelle partie : rien ne survit de la précédente (partie Workbench arrêtée, scénario redémarré dans le même
		// processus) : un vol d'hélicoptère resté inscrit bloquait toute nouvelle sortie, le profil CRX ne s'écrivait
		// plus, le journal de repérage et les fusées de l'ancienne partie restaient
		SRP_HeliSearch.ResetStatics(true);
		SRP_EnemySenses.ResetStatics();
		SRP_CRX.ResetStatics();

		if (!m_aGroupPrefabs || m_aGroupPrefabs.IsEmpty())
			Print("[SRP] IA ennemie : aucun prefab de groupe réglé (SRP_EnemyComponent), ni garnison, ni patrouille, ni mission armée", LogLevel.WARNING);
		GetGame().GetCallqueue().CallLater(Tick, 60000, true);
		GetGame().GetCallqueue().CallLater(PatrolTick, 15000, true);
		GetGame().GetCallqueue().CallLater(AlertTick, 10000, true);
		s_bTestMode = TestMode();
		if (s_bTestMode)
			Print("[SRP] IA ennemie : MODE ESSAI du Workbench actif (un joueur suffit, tirages toujours réussis pour l'hélicoptère, la jeep et les camions ; journal préfixé [ESSAI])", LogLevel.WARNING);

		// Brique commune de pose : la base et son rayon interdit valent pour tous les chemins de pose
		SRP_Placement.s_fBaseSafeRadius = m_fBaseSafeRadius;
		SRP_Placement.s_sBaseMarkerName = m_sBaseMarkerName;

		// Délai de grâce : chaque apparition d'un joueur (connexion, réapparition) est notée
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
			gameMode.GetOnPlayerSpawned().Insert(OnPlayerSpawnedGrace);

		SRP_CRX.s_fPerceptionSafe = m_fPerceptionSafe;
		SRP_CRX.s_fPerceptionVigilant = m_fPerceptionVigilant;
		SRP_CRX.s_fPerceptionAlerted = m_fPerceptionAlerted;
		SRP_CRX.s_fPerceptionThreatened = m_fPerceptionThreatened;
		SRP_CRX.s_bFlashlights = m_bPatrolFlashlights;
		SRP_CRX.s_iInvestigateRadius = m_iCRXInvestigateRadius;
		SRP_CRX.s_bFireteams = m_bCRXFireteams;
		SRP_CRX.s_fGarrisonPostHold = m_fGarrisonPostHoldRadius;
		s_iSpawnGraceMs = m_iSpawnGraceSeconds * 1000;
		if (s_iSpawnGraceMs < 10000)
			s_iSpawnGraceMs = 10000;

		// Sens de l'ennemi (carte #69) : perception de nos soldats, lumière, végétation, tir sous le feu
		SRP_EnemySenses.s_fSafe = m_fPerceptionSafe;
		SRP_EnemySenses.s_fVigilant = m_fPerceptionVigilant;
		SRP_EnemySenses.s_fAlerted = m_fPerceptionAlerted;
		SRP_EnemySenses.s_fThreatened = m_fPerceptionThreatened;
		SRP_EnemySenses.s_fSentry = m_fSentryFactor;
		SRP_EnemySenses.s_fPatrol = m_fPatrolFactor;
		SRP_EnemySenses.s_fAssault = m_fAssaultFactor;
		SRP_EnemySenses.s_fCrew = m_fCrewFactor;
		SRP_EnemySenses.s_fCrouchTarget = m_fCrouchTargetFactor;
		SRP_EnemySenses.s_fProneTarget = m_fProneTargetFactor;
		SRP_EnemySenses.s_fProneObserverPenalty = m_fProneObserverPenalty;
		SRP_EnemySenses.s_fRange = m_fSensesRange;
		SRP_EnemySenses.s_fConcealRange = m_fConcealCheckRange;
		SRP_EnemySenses.s_fLightFullLV = m_fLightFullLV;
		SRP_EnemySenses.s_fLightDarkLV = m_fLightDarkLV;
		SRP_EnemySenses.s_fNightMin = m_fNightMinFactor;
		SRP_EnemySenses.s_fLitIllum = m_fLitIllumination;
		SRP_EnemySenses.s_bFlashlightReveals = m_bFlashlightReveals;
		SRP_EnemySenses.s_iShotRevealSeconds = m_iShotRevealSeconds;
		SRP_EnemySenses.s_fFlareRadius = m_fFlareLightRadius;
		SRP_EnemySenses.s_iFlareMs = m_iFlareLightSeconds * 1000;
		SRP_EnemySenses.s_fCloseDistance = m_fNightCloseDistance;
		SRP_EnemySenses.s_fCloseFactor = m_fNightCloseFactor;
		SRP_EnemySenses.s_bReveal = m_bNightReveal;
		SRP_EnemySenses.s_iRevealMs = m_iNightRevealSeconds * 1000;
		SRP_EnemySenses.s_iRevealCooldownMs = m_iNightRevealCooldownSeconds * 1000;
		SRP_EnemySenses.s_bVegetation = m_bVegetationSlowdown;
		SRP_EnemySenses.s_fBushFactor = m_fBushFactor;
		SRP_EnemySenses.s_fBushMinHeight = m_fBushMinHeight;
		SRP_EnemySenses.s_fBushMaxHeight = m_fBushMaxHeight;
		SRP_EnemySenses.s_fBushReach = m_fBushReach;
		SRP_EnemySenses.s_fBushStillSpeed = m_fBushStillSpeed;
		SRP_EnemySenses.s_fSuppressionAim = m_fSuppressionAim;
		SRP_EnemySenses.s_fAimDefault = m_fAimDefault;
		SRP_EnemySenses.s_fAimSpread = m_fAimSpreadFactor;
		SRP_EnemySenses.s_iSightingLogMs = m_iSightingLogSeconds * 1000;
		SRP_EnemySenses.s_fVigilantFactor = m_fVigilantFactor;
		SRP_EnemySenses.s_fOpticsFactor = m_fOpticsFactor;
		SRP_EnemySenses.s_fLoiterWakeDistance = m_fLoiterWakeDistance;
		SRP_EnemySenses.s_bPerceptionDebug = m_bPerceptionDebug;

		// Entraide (carte #69, livraison 2) : aide par le flanc, ratissage, vigilance des groupes voisins
		SRP_EnemyAwareness.s_iHelpDelayMs = Math.ClampInt(m_iHelpDelaySeconds, 0, 3600) * 1000;
		SRP_EnemyAwareness.s_fHelpRadius = m_fHelpRadius;
		SRP_EnemyAwareness.s_iHelpGroups = m_iHelpGroups;
		SRP_EnemyAwareness.s_bFlanking = m_bFlanking;
		SRP_EnemyAwareness.s_fFlankAngle = m_fFlankAngle;
		SRP_EnemyAwareness.s_fFlankMin = m_fFlankMin;
		SRP_EnemyAwareness.s_fFlankMax = Math.Max(m_fFlankMax, m_fFlankMin);
		SRP_EnemyAwareness.s_iContactLostMs = Math.ClampInt(m_iContactLostSeconds, 10, 3600) * 1000;
		SRP_EnemyAwareness.s_iSweepMs = Math.ClampInt(m_iSweepMinutes, 1, 120) * 60 * 1000;
		SRP_EnemyAwareness.s_iSweepGroups = m_iSweepGroups;
		SRP_EnemyAwareness.s_fSweepJoinRadius = m_fSweepJoinRadius;
		SRP_EnemyAwareness.s_fSweepRadius = Math.Max(m_fSweepRadius, 20);
		SRP_EnemyAwareness.s_iSweepStepMs = Math.ClampInt(m_iSweepStepSeconds, 10, 3600) * 1000;
		SRP_EnemyAwareness.s_fVigilantRadius = m_fVigilantRadius;
		SRP_EnemyAwareness.s_iVigilantMs = Math.ClampInt(m_iVigilantMinutes, 0, 120) * 60 * 1000;

		int sensesDelay = m_iSensesTickMs;
		if (sensesDelay < 500)
			sensesDelay = 500;
		GetGame().GetCallqueue().CallLater(SensesTick, sensesDelay, true);

		if (!m_sHeliPrefab.IsEmpty())
			SRP_HeliSearch.s_sPrefab = m_sHeliPrefab;
		SRP_HeliSearch.s_fAltitude = m_fHeliAltitude;
		SRP_HeliSearch.s_fOrbitRadius = m_fHeliRadius;
		SRP_HeliSearch.s_fSpeed = m_fHeliSpeed;
		SRP_HeliSearch.s_iMinutes = m_iHeliMinutes;
		SRP_HeliSearch.s_fBank = m_fHeliBank;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(PatrolTick);
			GetGame().GetCallqueue().Remove(AlertTick);
			GetGame().GetCallqueue().Remove(SensesTick);
			SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
			if (gameMode)
				gameMode.GetOnPlayerSpawned().Remove(OnPlayerSpawnedGrace);

			// Fin de partie : états statiques vidés (les entités disparaissent avec le monde : rien n'est supprimé ici)
			SRP_HeliSearch.ResetStatics(false);
			SRP_EnemySenses.ResetStatics();
			SRP_CRX.ResetStatics();
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Délai de grâce : un joueur qui vient de se connecter ou de réapparaître ne compte pas comme « proche » pendant
	// quelques minutes (pas de garnison ni de vague posées sur un joueur qui charge encore sa partie)
	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);
		GrantGrace(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerSpawnedGrace(int playerId, IEntity player)
	{
		GrantGrace(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void GrantGrace(int playerId)
	{
		if (playerId <= 0 || m_iGraceMinutes <= 0)
			return;
		m_mGraceUntil.Set(playerId, System.GetTickCount() + m_iGraceMinutes * 60 * 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce joueur est-il encore dans son délai de grâce ?
	bool InGrace(int playerId)
	{
		int until;
		if (playerId <= 0 || !m_mGraceUntil.Find(playerId, until))
			return false;
		if (System.GetTickCount() < until)
			return true;
		m_mGraceUntil.Remove(playerId);
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Les personnages des joueurs qui comptent pour la pose : vivants, et hors délai de grâce
	array<IEntity> GetActivePlayers()
	{
		array<IEntity> result = {};
		array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
		foreach (IEntity player : everyone)
		{
			if (!player || SRP_Utils.IsDead(player))
				continue;
			if (InGrace(SRP_Utils.GetPlayerIdFromEntity(player)))
				continue;
			result.Insert(player);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	// Spawn
	[Attribute("1", UIWidgets.CheckBox, "Régler nos groupes avec CRX Enfusion A.I. (garnison défensive, assaut offensif, patrouille) au spawn", category: "SimpleRP - CRX")]
	protected bool m_bUseCRX;

	[Attribute("100", UIWidgets.EditBox, "CRX : zone tenue par une garde de mission par défaut (rayon de son point Defend), en mètres (cache 30, poste d'observation 80, sabotage 150 ; une garnison tient le rayon de son secteur). Chaque soldat de garde reste, lui, près de son poste (réglage suivant).", category: "SimpleRP - CRX")]
	protected float m_fCRXHoldRadius;

	[Attribute("30", UIWidgets.EditBox, "CRX : un soldat de garde (garnison, garde de mission, officier) tient un rayon de … mètres autour de SON poste (pris là où il s'est posé : tranquille et immobile), jamais plus que la zone de son groupe ; ses enquêtes et ses déplacements de combat restent dans ce cercle. 0 = ancien comportement (toute la zone autour de son centre).", category: "SimpleRP - CRX")]
	protected float m_fGarrisonPostHoldRadius;

	[Attribute("1", UIWidgets.CheckBox, "Gardes debout : sur nos points Defend, les soldats ne prennent que des postes debout (observation, porte, couvert), jamais les postes où l'on s'assoit ou s'adosse ; les tourelles restent occupées. Décocher pour le point Defend du jeu tel quel.", category: "SimpleRP - Groupes")]
	protected bool m_bGuardsNoSitting;

	[Attribute("-1", UIWidgets.EditBox, "CRX : rayon d'enquête de nos groupes hostiles, en mètres (-1 = rayon CRX dynamique ; 300 = comportement d'avant 1.0.27)", category: "SimpleRP - CRX")]
	protected int m_iCRXInvestigateRadius;

	[Attribute("1", UIWidgets.CheckBox, "CRX : nos groupes de 4 soldats et plus (garde, assaut, ronde) combattent en binômes, une moitié fixe pendant que l'autre bouge", category: "SimpleRP - CRX")]
	protected bool m_bCRXFireteams;

	[Attribute("60", UIWidgets.EditBox, "Part des groupes de garnison (après le premier, toujours en garde au centre) qui font une ronde sur les rues du lieu au lieu de tenir un poste, en %", category: "SimpleRP - Patrouilles")]
	protected int m_iPatrolPercent;

	[Attribute("45", UIWidgets.EditBox, "Patrouille : délai minimal avant l'étape suivante, en secondes", category: "SimpleRP - Patrouilles")]
	protected int m_iPatrolMinSeconds;

	[Attribute("150", UIWidgets.EditBox, "Patrouille : délai maximal avant l'étape suivante, en secondes", category: "SimpleRP - Patrouilles")]
	protected int m_iPatrolMaxSeconds;

	[Attribute("25", UIWidgets.EditBox, "Patrouille : le groupe est considéré arrivé à moins de … mètres de son point, et repart", category: "SimpleRP - Patrouilles")]
	protected float m_fPatrolArrivalDistance;

	//------------------------------------------------------------------------------------------------
	bool HasPrefabs()
	{
		return m_aGroupPrefabs && !m_aGroupPrefabs.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Effectif : base + un groupe par tranche de joueurs connectés, dans [min, max]
	int ScaleGroups(int minimum, int max)
	{
		array<int> ids = {};
		int connected = GetGame().GetPlayerManager().GetPlayers(ids);
		int groups = minimum;
		if (m_iPlayersPerExtraGroup > 0)
			groups += connected / m_iPlayersPerExtraGroup;
		if (groups > max)
			groups = max;
		if (groups < 1)
			groups = 1;
		return groups;
	}

	//------------------------------------------------------------------------------------------------
	//! Mode essai : vrai seulement dans le Workbench, et si la case est cochée
	protected bool TestMode()
	{
		bool test = false;
		#ifdef WORKBENCH
		test = m_bWorkbenchTestMode;
		#endif
		return test;
	}

	//------------------------------------------------------------------------------------------------
	//! Un seuil de joueurs, levé à 1 en mode essai
	int MinPlayers(int configured)
	{
		if (TestMode())
			return 1;
		return configured;
	}

	//------------------------------------------------------------------------------------------------
	//! Un tirage en %, toujours réussi en mode essai
	protected bool Roll(int chance)
	{
		if (TestMode())
			return true;
		return Math.RandomInt(0, 100) < chance;
	}

	//------------------------------------------------------------------------------------------------
	//! Facteur de la menace LOCALE en ce point (CO2, CO9 : patrouilles, jeeps et camions de vague suivent la menace de la
	//! région) : 1 + SRP_Commander.ThreatAt(position, menace de l'île) / 10 (menace 0 = x1, menace 10 = x2) ; sans
	//! Commandeur, ThreatAt rend la menace de l'île (comme avant)
	protected float ThreatFactor(vector position)
	{
		int island = 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			island = front.GetThreat();
		else
		{
			SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
			if (territory)
				island = territory.GetThreat();
		}
		return 1 + SRP_Commander.ThreatAt(position, island) / 10.0;
	}

	//------------------------------------------------------------------------------------------------
	//! Un tirage en %, multiplié par le facteur de menace locale en ce point (jeep, renforts motorisés) ; toujours réussi
	//! en mode essai
	protected bool RollThreat(int chance, vector position)
	{
		if (TestMode())
			return true;
		return Math.RandomFloat(0, 100) < chance * ThreatFactor(position);
	}

	//------------------------------------------------------------------------------------------------
	// Prorata : l'effectif suit les joueurs PROCHES du lieu, pas les joueurs connectés
	//------------------------------------------------------------------------------------------------
	//! Joueurs à moins du rayon réglé (3 km) du lieu ; « radius » ne peut que l'élargir (un secteur dont la garnison
	//! se pose de plus loin). Ceux restés à la base ne comptent pas.
	int CountPlayersNear(vector center, float radius = -1)
	{
		if (radius < m_fScaleRadius)
			radius = m_fScaleRadius;
		vector basePosition = vector.Zero;
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (baseMarker)
			basePosition = baseMarker.GetOrigin();

		int count = 0;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		foreach (IEntity player : players)
		{
			if (!player || SRP_Utils.IsDead(player))
				continue;
			if (InGrace(SRP_Utils.GetPlayerIdFromEntity(player)))
				continue;	// vient d'apparaître : pas encore en opération
			vector position = player.GetOrigin();
			if (vector.Distance(position, center) > radius)
				continue;
			if (baseMarker && vector.Distance(position, basePosition) < m_fBaseSafeRadius && vector.Distance(center, basePosition) > m_fBaseSafeRadius)
				continue;	// à la base, pas en opération sur ce lieu
			count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes au prorata des joueurs proches : un groupe par tranche de joueurs, plus « bonus » (difficulté, menace), dans [1, max]
	int ScaleGroupsNear(vector center, int bonus, int max, float radius = -1)
	{
		int near = Math.Max(1, CountPlayersNear(center, radius));
		int per = Math.Max(1, m_iPlayersPerGroup);
		int groups = (near + per - 1) / per + bonus;
		if (groups > max)
			groups = max;
		if (groups < 1)
			groups = 1;
		return groups;
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes ennemis vivants posés par nous à moins de « radius » d'un lieu (un groupe que la file d'apparition de la 1.8
	//! n'a pas encore servi compte déjà : ses soldats arrivent)
	int CountGroupsNear(vector center, float radius)
	{
		int count = 0;
		int now = System.GetTickCount();
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (AliveAgents(record) == 0 && !InGraceOf(record, now))
				continue;
			if (vector.Distance(GroupPosition(record), center) <= radius)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Budget local : combien de groupes on peut encore poser autour de ce lieu sans faire doublon avec ce qui s'y
	//! trouve déjà (garnison de secteur, garde de mission, vague, patrouille). 0 = le lieu est déjà assez gardé.
	int LocalRoom(vector center, float countRadius = -1)
	{
		int budget = ScaleGroupsNear(center, m_iLocalBonus, m_iLocalMax, countRadius);
		return Math.Max(0, budget - CountGroupsNear(center, m_fLocalRadius));
	}

	//------------------------------------------------------------------------------------------------
	int CountAlive()
	{
		Prune();
		return m_aAll.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Pose « count » groupes dispersés dans « radius » autour de center, chacun dans sa part du cercle. Ordre : défense
	//! sur place (defend = true : garnison, garde) ou déplacement vers target (vagues ; viaRoads = par la route puis à
	//! travers champs). Chaque point passe par la brique commune (hors de vue, distance aux joueurs, base, route ou
	//! lisière) ; à défaut, l'ancien tirage, noté au journal.
	//! Garnison (defend) : le premier groupe tient le CENTRE en Defend (postes, tourelles, bâtiments), les suivants font
	//! une ronde sur les rues (m_iPatrolPercent %) ou tiennent leur propre emplacement en Defend. Garnison de territoire
	//! complétée ou reprise (outGroups non vide) : le centre seulement s'il n'a pas encore de groupe de commandement.
	//! holdRadius : rayon à tenir (rayon du secteur, ou rayon de maintien d'une garde) ; -1 = réglage m_fCRXHoldRadius.
	//! Groupes différés par cet appel (garnison de territoire, joueur dans le secteur) : GetLastDeferred().
	int SpawnGroups(vector center, float radius, int count, bool defend, vector target, string owner, out array<ref SRP_EnemyGroup> outGroups, float holdRadius = -1, bool viaRoads = false)
	{
		m_iLastDeferred = 0;
		if (!HasPrefabs())
			return 0;

		array<IEntity> players = SRP_Utils.GetPlayerCharacters();	// tous : un joueur en délai de grâce ne doit pas non plus voir la pose
		float playerMinDist = 250;		// vague : l'origine a déjà été choisie loin des joueurs, ici on disperse autour
		if (defend)
		{
			playerMinDist = 300;		// garde de mission : posée à l'approche, hors de vue
			if (holdRadius > playerMinDist)
				playerMinDist = holdRadius;	// garnison : le rayon du secteur
		}

		// Garnison de territoire avec un joueur déjà dans le secteur : l'ancien tirage de repli poserait les soldats sur lui
		// (essai du 25/09 : 6 à 10 m). Un groupe sans point hors de vue est alors différé : le territoire réessaie les
		// groupes différés toutes les 30 s (SRP_TerritoryComponent, pose différée), joueurs au contact ou non
		bool deferIfNoHiddenPoint = defend && owner == "territoire" && holdRadius > 0 && SRP_Placement.NearestPlayer(center, players) <= holdRadius;
		int deferred = 0;

		// Le centre (groupe de commandement) : le premier groupe d'une défense ; pour une garnison de territoire complétée
		// ou reprise après une pose différée, seulement s'il manque encore (jamais deux groupes de commandement)
		bool centreWanted = defend && (owner != "territoire" || !HasCommandGroup(outGroups));

		int spawned = 0;
		float startAngle = Math.RandomFloat(0, Math.PI2);
		float slice = Math.PI2 / Math.Max(count, 1);
		for (int i = 0; i < count; i++)
		{
			if (CountAlive() >= m_iMaxGroups)
			{
				Print("[SRP] IA ennemie : plafond de groupes atteint, spawn refusé", LogLevel.WARNING);
				break;
			}

			// Chaque groupe dans sa propre part du cercle : des emplacements distincts, répartis autour du centre
			float angleMin = startAngle + slice * i + slice * 0.15;
			float angleMax = startAngle + slice * i + slice * 0.85;
			float ringMin = radius * 0.35;
			float ringMax = radius;
			bool first = centreWanted && i == 0;
			if (first)
			{
				// Le premier groupe d'une garnison ou d'une garde : au centre, sur les postes
				ringMin = 0;
				ringMax = Math.Min(radius * 0.3, 40.0);
			}

			vector spawnPosition;
			if (!SRP_Placement.FindSpawnPosition(center, ringMin, ringMax, players, defend, !defend, spawnPosition, playerMinDist, angleMin, angleMax))
			{
				if (deferIfNoHiddenPoint)
				{
					deferred++;
					continue;
				}
				// Repli : l'ancien tirage (mais jamais dans l'eau ni près de la base)
				float angle = Math.RandomFloat(angleMin, angleMax);
				float distance = Math.RandomFloat(ringMin, ringMax);
				vector position = SRP_Placement.OnGround(center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
				if (!SCR_WorldTools.FindEmptyTerrainPosition(spawnPosition, position, 20, 2, 2))
					spawnPosition = position;
				if (!SRP_Placement.IsAcceptableFallback(spawnPosition, players, 0))
				{
					Journal("ENNEMI", string.Format("Pose refusée (%1) : aucun point hors de vue, et le repli tombe dans l'eau ou près de la base", owner));
					continue;
				}
				Journal("ENNEMI", string.Format("Pose (%1) : aucun point hors de vue après %2 essais, repli sur l'ancien tirage", owner, SRP_Placement.TRIES));
			}

			// Garnison : le premier groupe tient les postes du centre ; ensuite une part fait sa ronde sur les rues
			float patrolRadius = -1;
			if (defend && !first && Math.RandomFloat(0, 100) < m_iPatrolPercent)
			{
				patrolRadius = radius;
				if (holdRadius > patrolRadius)
					patrolRadius = holdRadius;
			}

			// Les groupes en Defend après le premier tiennent leur propre emplacement (100 m au plus), pas tout le secteur
			vector holdOrigin = target;
			float hold = holdRadius;
			if (defend && !first && patrolRadius < 0)
			{
				holdOrigin = spawnPosition;
				if (hold > 100)
					hold = 100;
			}

			// Carte #69, livraison 3 : gardes et vagues de mission en sections de 4 à 6 soldats (m_aSectionPrefabs)
			ResourceName groupPrefab = "";
			if (owner == "mission")
				groupPrefab = PickSectionPrefab();
			SRP_EnemyGroup record = SpawnGroup(spawnPosition, defend, holdOrigin, owner, hold, center, patrolRadius, viaRoads, groupPrefab);
			if (!record)
				continue;

			// Ronde sur les rues du lieu : un cycle de 3 à 5 points de route ; sans route, l'ancienne patrouille de zone reste
			if (patrolRadius > 0)
			{
				array<vector> circuit = {};
				int points = Math.RandomIntInclusive(Math.Max(2, m_iStreetPointsMin), Math.Max(m_iStreetPointsMin, m_iStreetPointsMax));
				if (SRP_Placement.BuildStreetCircuit(center, patrolRadius, points, circuit))
				{
					OrderCycle(record, circuit);
					NoteHome(record, "ronde", false);	// le cycle fait partie du poste d'origine
				}
			}

			// Carte #69 : le premier groupe d'une défense tient le centre, c'est le groupe de commandement (il ne part
			// aider que s'il est le seul groupe libre, jamais ratisser)
			if (first)
			{
				record.m_bCommand = true;
				NoteHome(record, "centre", true);
			}

			outGroups.Insert(record);
			spawned++;
		}

		m_iLastDeferred = deferred;
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes différés par le dernier SpawnGroups (garnison de territoire, joueur dans le secteur, aucun point hors de
	//! vue) : le territoire les réessaie et écrit la ligne de journal (une par épisode)
	int GetLastDeferred()
	{
		return m_iLastDeferred;
	}

	protected int m_iLastDeferred;

	//------------------------------------------------------------------------------------------------
	//! Un de ces groupes est-il déjà le groupe de commandement (le centre) ? Morts compris : pas de remplacement des pertes
	protected static bool HasCommandGroup(array<ref SRP_EnemyGroup> groups)
	{
		if (!groups)
			return false;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (record && record.m_bCommand)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Un point praticable au hasard dans la zone : sur une route du lieu quand il y en a une, sinon l'ancien tirage
	//! (jamais en mer)
	protected vector RandomPatrolPoint(vector center, float radius)
	{
		vector onRoad;
		if (SRP_Placement.RandomRoadPointNear(center, radius, null, onRoad))
			return onRoad;
		for (int attempt = 0; attempt < 6; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(radius * 0.3, radius * 0.9);
			vector position = center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
			if (position[1] < 1)
				continue;
			vector found;
			if (SCR_WorldTools.FindEmptyTerrainPosition(found, position, 15, 1, 2))
				return found;
			return position;
		}
		return center;
	}

	//------------------------------------------------------------------------------------------------
	//! Position courante d'un groupe : son premier soldat vivant, sinon l'entité de groupe. Groupe absent : vector.Zero
	vector GroupPosition(SRP_EnemyGroup record)
	{
		// Filet de sécurité : le jeu supprime un SCR_AIGroup vide 1 ms après (m_bDeleteWhenEmpty), bien avant que Prune
		// ne sorte sa fiche des listes ; sans ce test, l'appel suivant planterait sur un groupe nul
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return vector.Zero;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (entity && !entity.IsDeleted() && !SRP_Utils.IsDead(entity))
				return entity.GetOrigin();
		}
		return record.m_Group.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Étape suivante d'une patrouille de zone ou d'un circuit
	protected void NextPatrolLeg(SRP_EnemyGroup record, int now)
	{
		record.m_iArrivedTick = 0;
		if (!record.m_aCircuit.IsEmpty())
		{
			// Circuit : le point suivant ; en véhicule, cinq minutes au plus par étape (un véhicule coincé repart ailleurs)
			record.m_iCircuitIndex = record.m_iCircuitIndex + 1;
			if (record.m_iCircuitIndex >= record.m_aCircuit.Count())
				record.m_iCircuitIndex = 0;
			OrderMove(record, record.m_aCircuit[record.m_iCircuitIndex]);
			if (record.m_Vehicle)
				record.m_iNextPatrolTick = now + 5 * 60 * 1000;
			else
				record.m_iNextPatrolTick = now + 12 * 60 * 1000;	// à pied, une étape de route peut être longue
			return;
		}
		OrderMove(record, RandomPatrolPoint(record.m_vPatrolCenter, record.m_fPatrolRadius));
		int minSeconds = Math.Max(m_iPatrolMinSeconds, 10);
		int maxSeconds = Math.Max(m_iPatrolMaxSeconds, minSeconds);
		record.m_iNextPatrolTick = now + Math.RandomInt(minSeconds, maxSeconds + 1) * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 15 s : les patrouilles de zone arrivées à leur point (après une courte pause à pied), ou dont le délai
	//! est écoulé, repartent ; une ronde sur les rues interrompue par une alerte reprend son cycle une fois la recherche finie
	protected void PatrolTick()
	{
		int now = System.GetTickCount();
		CloseStalledSpawns(now);
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_bPatrol || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (AliveAgents(record) == 0)
				continue;
			if (record.m_iBusyUntilTick > now)
				continue;	// parti aider ou ratisser (carte #69) : SRP_EnemyAwareness le ramène à sa ronde ensuite

			if (record.m_iSearchUntilTick > 0)
			{
				if (now < record.m_iSearchUntilTick)
					continue;	// parti voir un lieu d'alerte : il finit sa recherche avant de reprendre sa ronde
				record.m_iSearchUntilTick = 0;
				if (record.m_bCycle)
				{
					OrderCycle(record, record.m_aCircuit);
					continue;
				}
			}

			if (record.m_bCycle)
				continue;	// le cycle est mené par le moteur

			float arrival = m_fPatrolArrivalDistance;
			if (record.m_Vehicle)
				arrival = 45;
			bool arrived = false;
			if (record.m_Waypoint && !record.m_Waypoint.IsDeleted())
				arrived = vector.Distance(GroupPosition(record), record.m_Waypoint.GetOrigin()) < arrival;
			else
				arrived = true;

			// À pied, une pause de 30 s au point atteint avant de repartir ; en véhicule, on enchaîne
			if (arrived && !record.m_Vehicle)
			{
				if (record.m_iArrivedTick == 0)
				{
					record.m_iArrivedTick = now;
					arrived = false;
				}
				else
					arrived = now - record.m_iArrivedTick >= 30000;
			}

			if (arrived || now >= record.m_iNextPatrolTick)
				NextPatrolLeg(record, now);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le point de passage « Defend » : le prefab réglé s'il donne bien un AIWaypoint, sinon AIWaypoint_Defend_Large du
	//! jeu (postes, tourelles, bâtiments du lieu), sinon AIWaypoint_Defend. Rayon = zone à tenir. Sans preset de défense
	//! sur le prefab, un preset est ajouté (tourelles, toute la zone).
	IEntity SpawnDefendWaypoint(vector position, float radius)
	{
		IEntity entity;
		AIWaypoint waypoint;
		if (!m_sDefendWaypointPrefab.IsEmpty())
		{
			entity = SpawnAt(m_sDefendWaypointPrefab, position);
			waypoint = AIWaypoint.Cast(entity);
			if (!waypoint && entity)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				entity = null;
				if (!m_bLoggedDefendFallback)
				{
					m_bLoggedDefendFallback = true;
					Print("[SRP] IA ennemie : le prefab « Defend » réglé n'est pas un AIWaypoint, AIWaypoint_Defend_Large du jeu utilisé à la place", LogLevel.WARNING);
				}
			}
		}
		if (!waypoint)
		{
			entity = SpawnAt("{FAD1D789EE291964}Prefabs/AI/Waypoints/AIWaypoint_Defend_Large.et", position);
			waypoint = AIWaypoint.Cast(entity);
		}
		if (!waypoint)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			entity = SpawnAt("{93291E72AC23930F}Prefabs/AI/Waypoints/AIWaypoint_Defend.et", position);
			waypoint = AIWaypoint.Cast(entity);
		}
		if (!waypoint)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}

		if (radius > 0)
			waypoint.SetCompletionRadius(radius);
		SCR_DefendWaypoint defend = SCR_DefendWaypoint.Cast(waypoint);
		if (defend && !defend.GetCurrentDefendPreset())
		{
			SCR_DefendWaypointPreset preset = new SCR_DefendWaypointPreset();
			preset.SetPresetName("SimpleRP");
			preset.SetUseTurrets(true);
			preset.SetFractionOfSA(1);
			if (m_bGuardsNoSitting)
				preset.SetFractionOfSA(0);	// sans liste d'étiquettes, la recherche prendrait tous les postes, assis compris
			defend.AddDefendPreset(preset);
			defend.SetCurrentDefendPreset(0);
		}
		if (defend && m_bGuardsNoSitting)
			KeepStandingPosts(defend);
		return entity;
	}

	protected bool m_bLoggedDefendFallback;
	protected bool m_bLoggedStandingPosts;

	//------------------------------------------------------------------------------------------------
	//! Gardes debout (carte #69, correctif du 25/09) : le preset « Guarding » du point Defend du jeu envoie une part des
	//! soldats (m_fFractionOfSA 0,6) sur les actions intelligentes « ObservationPost », « GatePost » et « LoiterPost ».
	//! LoiterPost les fait asseoir ou s'adosser (SCR_LoiterUserAction, arbre SA_LoiterPost) : un soldat qui flâne réagit
	//! très mal. Seuls les postes debout restent dans la recherche (observation, porte, couvert : leurs arbres ne lancent
	//! aucune animation de flânerie). Sans poste debout, plus aucune action intelligente : chacun défend son secteur. Les
	//! tourelles (réglage séparé du preset) restent occupées.
	protected void KeepStandingPosts(SCR_DefendWaypoint defend)
	{
		SCR_DefendWaypointPreset current = defend.GetCurrentDefendPreset();
		if (!current)
			return;
		array<string> standing = {"ObservationPost", "GatePost", "CoverPost"};
		int kept = current.SRP_KeepTags(standing);
		if (kept <= 0)
		{
			current.SetFractionOfSA(0);
			kept = 0;
		}
		if (!m_bLoggedStandingPosts)
		{
			m_bLoggedStandingPosts = true;
			Journal("ENNEMI", string.Format("Gardes debout : preset « %1 » du point Defend, %2 étiquette(s) de poste debout gardée(s), part des actions intelligentes %3", current.GetPresetName(), kept, SRP_EnemySenses.Dec2(current.GetFractionOfSA())));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! patrolRadius > 0 : au lieu de tenir sur place, le groupe patrouille dans la zone (patrolCenter, patrolRadius).
	//! viaRoads : un déplacement long (> 400 m) passe par des points de route, puis à travers champs pour le dernier tronçon.
	//! prefab (carte #69, livraison 3) : le prefab de groupe voulu ; vide = tiré au hasard dans m_aGroupPrefabs, jamais le
	//! groupe de commandement (PickGroupPrefab).
	SRP_EnemyGroup SpawnGroup(vector position, bool defend, vector target, string owner, float holdRadius = -1, vector patrolCenter = vector.Zero, float patrolRadius = -1, bool viaRoads = false, ResourceName prefab = "")
	{
		if (!HasPrefabs())
			return null;

		bool patrol = defend && patrolRadius > 0;
		if (patrol)
		{
			// Une patrouille est un groupe en déplacement : point de passage « Move » vers un premier point de la zone
			defend = false;
			target = RandomPatrolPoint(patrolCenter, patrolRadius);
		}

		ResourceName chosenPrefab = prefab;
		if (chosenPrefab.IsEmpty())
			chosenPrefab = PickGroupPrefab();
		IEntity entity = SpawnAt(chosenPrefab, position);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			Print("[SRP] IA ennemie : le prefab n'est pas un SCR_AIGroup", LogLevel.ERROR);
			return null;
		}

		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = owner;
		record.m_iExpected = SlotCount(group);

		if (defend)
		{
			float defendRadius = holdRadius;
			if (defendRadius <= 0)
				defendRadius = m_fCRXHoldRadius;
			IEntity waypointEntity = SpawnDefendWaypoint(target, defendRadius);
			if (waypointEntity)
			{
				group.AddWaypointToGroup(AIWaypoint.Cast(waypointEntity));
				record.m_Waypoint = waypointEntity;
			}
		}
		else
		{
			// Routes puis champs : les points de route d'abord (dans l'ordre), la cible en dernier
			if (viaRoads && !patrol && vector.Distance(position, target) > 400)
			{
				array<vector> route = {};
				SRP_Placement.BuildRoadRoute(position, target, 300, 350, route);
				foreach (vector step : route)
					AddExtraWaypoint(record, m_sMoveWaypointPrefab, step);
				if (!route.IsEmpty())
					Journal("ENNEMI", string.Format("Groupe (%1) : %2 point(s) de route avant le dernier tronçon à travers champs", owner, route.Count()));
			}
			IEntity waypointEntity = SpawnAt(m_sMoveWaypointPrefab, target);
			AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
			if (waypoint)
			{
				group.AddWaypointToGroup(waypoint);
				record.m_Waypoint = waypointEntity;
			}
			else if (waypointEntity)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
			}
		}

		if (patrol)
		{
			record.m_bPatrol = true;
			record.m_vPatrolCenter = patrolCenter;
			record.m_fPatrolRadius = patrolRadius;
			int minSeconds = Math.Max(m_iPatrolMinSeconds, 10);
			int maxSeconds = Math.Max(m_iPatrolMaxSeconds, minSeconds);
			record.m_iNextPatrolTick = record.m_iSpawnTick + Math.RandomInt(minSeconds, maxSeconds + 1) * 1000;
		}

		m_aAll.Insert(record);

		// Réglages CRX selon le rôle : garde en place, assaut vers une cible, patrouille (zone ou ambiante). Le rôle est
		// retenu même sans CRX : chaque soldat arrivé est réglé d'après lui (SRP_EnemySenses)
		int role = SRP_ECRXRole.ASSAUT;
		float hold = -1;
		if (defend)
		{
			role = SRP_ECRXRole.GARNISON;
			hold = m_fCRXHoldRadius;
			if (holdRadius > 0)
				hold = holdRadius;	// la zone à tenir (rayon du secteur, ou de la garde)
		}
		else if (patrol || owner == "ambiance")
			role = SRP_ECRXRole.PATROUILLE;
		ApplyRole(record, role, target, hold);

		// Tâche et poste d'origine (carte #69) ; SpawnGroups précise ensuite le centre et les rondes en cycle
		string task = "vague";
		if (defend)
		{
			task = "garde";
			if (owner == "territoire")
				task = "poste";
		}
		else if (patrol)
			task = "ronde";
		else if (owner == "ambiance")
			task = "ambiance";
		NoteHome(record, task, defend);

		// Carte #76 : importance pour le jeu d'après la classe devinée (le poseur précise ensuite la classe par Tag)
		SRP_CmdCapacity.TagImportance(record);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre d'emplacements de soldats d'un groupe (son effectif attendu) ; 0 pour un groupe vide
	protected static int SlotCount(SCR_AIGroup group)
	{
		if (!group || !group.m_aUnitPrefabSlots)
			return 0;
		return group.m_aUnitPrefabSlots.Count();
	}

	//------------------------------------------------------------------------------------------------
	// Prefabs de groupes par taille (carte #69, livraison 3)
	//------------------------------------------------------------------------------------------------
	//! Soldats d'un prefab de groupe (emplacements m_aUnitPrefabSlots, lus dans le prefab sans le poser), mis en cache ;
	//! 0 pour un prefab vide, invalide ou qui n'est pas un groupe
	int GroupSize(ResourceName prefab)
	{
		if (prefab.IsEmpty())
			return 0;
		int cached;
		if (m_mGroupSizes.Find(prefab, cached))
			return cached;
		int size = 0;
		Resource res = Resource.Load(prefab);
		if (res && res.IsValid())
		{
			IEntitySource src = SCR_BaseContainerTools.FindEntitySource(res);
			if (src)
			{
				array<ResourceName> slots;
				src.Get("m_aUnitPrefabSlots", slots);
				if (slots)
					size = slots.Count();
			}
		}
		m_mGroupSizes.Set(prefab, size);
		return size;
	}

	//------------------------------------------------------------------------------------------------
	//! Un prefab de « list » de minSize à maxSize soldats, au hasard ; jamais le groupe de commandement ; "" s'il n'y en a pas
	ResourceName PickPrefabOfSize(array<ResourceName> list, int minSize, int maxSize)
	{
		if (!list || list.IsEmpty())
			return "";
		array<ResourceName> fitting = {};
		foreach (ResourceName candidate : list)
		{
			if (candidate.IsEmpty() || candidate == m_sCommandGroupPrefab)
				continue;
			int size = GroupSize(candidate);
			if (size >= minSize && size <= maxSize)
				fitting.Insert(candidate);
		}
		if (fitting.IsEmpty())
			return "";
		return fitting.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Un prefab de m_aGroupPrefabs au hasard, jamais le groupe de commandement (le tireur d'élite ne vient qu'avec le QG,
	//! en ville) ; s'il n'y a que lui, lui
	ResourceName PickGroupPrefab()
	{
		if (!HasPrefabs())
			return "";
		for (int attempt = 0; attempt < 8; attempt++)
		{
			ResourceName candidate = m_aGroupPrefabs.GetRandomElement();
			if (candidate != m_sCommandGroupPrefab)
				return candidate;
		}
		foreach (ResourceName other : m_aGroupPrefabs)
		{
			if (other != m_sCommandGroupPrefab)
				return other;
		}
		return m_aGroupPrefabs[0];
	}

	//------------------------------------------------------------------------------------------------
	//! Une section de 4 à 6 soldats (gardes et vagues de mission) : m_aSectionPrefabs, sinon un groupe de 4 à 6 de
	//! m_aGroupPrefabs ; "" (tirage habituel) s'il n'y en a aucun
	ResourceName PickSectionPrefab()
	{
		ResourceName section = PickPrefabOfSize(m_aSectionPrefabs, 4, 6);
		if (section.IsEmpty())
			section = PickPrefabOfSize(m_aGroupPrefabs, 4, 6);
		return section;
	}

	//------------------------------------------------------------------------------------------------
	//! Une patrouille légère de 2 à 4 soldats (présence ambiante, hors mission) ; "" (tirage habituel) s'il n'y en a aucune
	ResourceName PickLightPrefab()
	{
		return PickPrefabOfSize(m_aGroupPrefabs, 2, 4);
	}

	//------------------------------------------------------------------------------------------------
	//! Un prefab de m_aGroupPrefabs de minSize à maxSize soldats (jamais le groupe de commandement) ; "" s'il n'y en a pas
	ResourceName PickGroupPrefabOfSize(int minSize, int maxSize)
	{
		return PickPrefabOfSize(m_aGroupPrefabs, minSize, maxSize);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetCommandGroupPrefab()
	{
		return m_sCommandGroupPrefab;
	}

	//------------------------------------------------------------------------------------------------
	array<ResourceName> GetSectionPrefabs()
	{
		return m_aSectionPrefabs;
	}

	//------------------------------------------------------------------------------------------------
	array<ResourceName> GetSentryPrefabs()
	{
		return m_aSentryPrefabs;
	}

	//------------------------------------------------------------------------------------------------
	//! Place des soldats ennemis au sol (Q7, cap_soldats_max, 120) : TEXTES seulement (Staff, journal) ; une pose n'est
	//! jamais refusée ici, c'est SRP_CmdCapacity.Ask qui décide (carte #76)
	int GetMaxSoldiers()
	{
		return SRP_CmdCapacity.Get().GetMaxSoldiers();
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats ennemis au sol, toutes classes sauf l'équipage de l'hélicoptère (SRP_CmdCapacity.CountGround), attendus
	//! compris pendant la pose : TEXTES seulement (m_bBudget n'est plus lu)
	int CountBudgetSoldiers()
	{
		return SRP_CmdCapacity.Get().CountGround();
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #76 : plafond d'éléments (cap_groupes_max, 36) et groupes gardés aux missions (cap_groupes_reserve_missions,
	//! 6) lus dans commandeur_reglages.txt ; remplacent les attributs du prefab — SRP_CmdCapacity (Start, LoadSettings)
	void SetMaxGroups(int maxGroups, int missionReserve)
	{
		m_iMaxGroups = Math.MaxInt(maxGroups, 1);
		m_iMissionGroupReserve = Math.ClampInt(missionReserve, 0, m_iMaxGroups);
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #76 : soldats encore attendus par la file d'apparition de la 1.8 (attendus moins le plus grand effectif vu,
	//! groupes dont la file n'est pas close) : leur place du jeu est déjà prise — SRP_CmdCapacity.EngineRoom
	int CountPendingSpawn()
	{
		int pending = 0;
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || record.m_bSpawnClosed || record.m_bAbandoned || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			NoteSeen(record);
			pending += Math.MaxInt(0, record.m_iExpected - record.m_iMaxSeen);
		}
		return pending;
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #76, RE12 : des renforts attendent une place. Des rondes (classe PATROUILLE) vivantes, posées, sans contact
	//! depuis 60 s, loin des joueurs (patrolMinDist ; jeepMinDist pour une jeep) et hors de leur vue sont retirées, de la
	//! plus lointaine à la plus proche, jusqu'à soldiersWanted soldats ou maxGroups éléments (la jeep avec son véhicule).
	//! Jamais un groupe qui obéit au Commandeur, ni un envoyé à rendre à sa localité (rendu par RetireTick). Rend
	//! les soldats libérés — SRP_CmdCapacity.Tick
	int ReleaseForRoom(int soldiersWanted, float patrolMinDist, float jeepMinDist, int maxGroups)
	{
		if (soldiersWanted <= 0 || maxGroups <= 0)
			return 0;
		int now = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();

		// Candidats, triés du plus loin des joueurs au plus proche
		array<SRP_EnemyGroup> candidates = {};
		array<float> distances = {};
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (SRP_CmdCapacity.ClassOf(record) != SRP_ECmdCapClass.PATROUILLE)
				continue;
			if (record.m_iCmdRole != SRP_ECmdRole.AUCUN || record.m_iAssaultZone >= 0 || !record.m_sSourceLocality.IsEmpty())
				continue;
			if (AliveAgents(record) == 0 || InGraceOf(record, now))
				continue;
			if (record.m_iBusyUntilTick > now)
				continue;	// parti aider ou ratisser : il va au contact
			if (record.m_iSpotTick > 0 && now - record.m_iSpotTick < 60000)
				continue;	// au contact depuis moins d'une minute
			vector position = GroupPosition(record);
			float minDistance = patrolMinDist;
			if (record.m_Vehicle && record.m_sHomeTask == "jeep")
				minDistance = jeepMinDist;
			float nearest = SRP_Placement.NearestPlayer(position, players);
			if (nearest < minDistance)
				continue;
			if (SRP_Placement.IsSeenByAnyPlayer(position, players))
				continue;
			int index = 0;
			while (index < distances.Count() && distances[index] >= nearest)
				index++;
			candidates.InsertAt(record, index);
			distances.InsertAt(nearest, index);
		}

		int freed = 0;
		int removed = 0;
		foreach (SRP_EnemyGroup chosen : candidates)
		{
			if (freed >= soldiersWanted || removed >= maxGroups)
				break;
			int soldiers = SoldiersOf(chosen, now);
			IEntity jeep = null;
			if (chosen.m_Vehicle && chosen.m_sHomeTask == "jeep")
				jeep = chosen.m_Vehicle;
			m_aAmbient.RemoveItem(chosen);
			m_aRetiring.RemoveItem(chosen);
			Delete(chosen);
			// La jeep part avec son équipage (comme le nettoyage des transports)
			if (jeep && !jeep.IsDeleted())
			{
				int slot = m_aTransports.Find(jeep);
				if (slot >= 0)
				{
					m_aTransports.Remove(slot);
					m_aTransportIdleSince.Remove(slot);
				}
				SCR_EntityHelper.DeleteEntityAndChildren(jeep);
			}
			freed += soldiers;
			removed++;
		}
		if (removed > 0)
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Place faite aux renforts : %1 élément(s) de ronde retiré(s) hors de vue, %2 soldat(s)", removed, freed));
		return freed;
	}

	//------------------------------------------------------------------------------------------------
	//! Le rôle CRX et le maintien voulus pour un groupe : retenus sur l'enregistrement (chaque soldat arrivé est réglé
	//! d'après eux par SRP_EnemySenses), réglages du GROUPE appliqués par CRX
	void ApplyRole(SRP_EnemyGroup record, int role, vector holdOrigin, float holdRadius)
	{
		if (!record)
			return;
		record.m_iRole = role;
		record.m_vHold = holdOrigin;
		record.m_fHold = holdRadius;
		// Binômes CRX pour nos groupes à pied seulement, jamais pour un équipage de jeep, d'hélicoptère ou de blindé
		bool fireteams = record.m_sOwner != "helico" && record.m_sTask != "jeep" && record.m_sTask != "helico" && record.m_sTask != "blinde";
		if (m_bUseCRX && record.m_Group)
			SRP_CRX.Apply(record.m_Group, role, holdOrigin, holdRadius, false, fireteams);
	}

	//------------------------------------------------------------------------------------------------
	//! Tâche et poste d'origine d'un groupe (carte #69), retenus à la pose (après ApplyRole) et rétablis après l'aide ou
	//! le ratissage : point et rayon tenus, rôle, point Defend (defend) ou ronde en cycle (m_bCycle au moment de l'appel)
	void NoteHome(SRP_EnemyGroup record, string task, bool defend)
	{
		if (!record)
			return;
		record.m_sTask = task;
		record.m_sHomeTask = task;
		record.m_iHomeRole = record.m_iRole;
		record.m_vHome = record.m_vHold;
		record.m_fHomeHold = record.m_fHold;
		record.m_bHomeDefend = defend;
		record.m_bHomeCycle = record.m_bCycle;
	}

	//------------------------------------------------------------------------------------------------
	//! Nos groupes sont-ils réglés avec CRX ? (réglages de chaque soldat par SRP_EnemySenses)
	bool UsesCRX()
	{
		return m_bUseCRX;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire tous les points de passage du groupe (les nôtres sont supprimés : point courant, points de route, cycle)
	void ClearWaypoints(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		array<AIWaypoint> current = {};
		record.m_Group.GetWaypoints(current);
		foreach (AIWaypoint waypoint : current)
		{
			if (waypoint)
				record.m_Group.RemoveWaypointFromGroup(waypoint);
		}
		if (record.m_Waypoint && !record.m_Waypoint.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Waypoint);
		record.m_Waypoint = null;
		DeleteExtraWaypoints(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvel ordre de déplacement pour un groupe existant (une ronde sur les rues perd son cycle : il est refait après)
	void OrderMove(SRP_EnemyGroup record, vector target)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;

		if (record.m_bCycle)
			ClearWaypoints(record);
		else if (record.m_Waypoint && !record.m_Waypoint.IsDeleted())
		{
			AIWaypoint old = AIWaypoint.Cast(record.m_Waypoint);
			if (old)
				record.m_Group.RemoveWaypointFromGroup(old);
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Waypoint);
			record.m_Waypoint = null;
		}
		record.m_iArrivedTick = 0;

		IEntity waypointEntity = SpawnAt(m_sMoveWaypointPrefab, target);
		AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
		if (waypoint)
		{
			record.m_Group.AddWaypointToGroup(waypoint);
			record.m_Waypoint = waypointEntity;
		}
		else if (waypointEntity)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ronde sur les rues : un AIWaypoint_Cycle (infini) sur des points de route ; le groupe devient une patrouille
	//! (alertes : il va voir, puis reprend son cycle). Sans prefab de cycle, l'ancien circuit point par point.
	void OrderCycle(SRP_EnemyGroup record, array<vector> points)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted() || !points || points.Count() < 2)
			return;
		ClearWaypoints(record);
		record.m_bPatrol = true;
		record.m_iArrivedTick = 0;
		if (record.m_aCircuit != points)
		{
			record.m_aCircuit.Clear();
			foreach (vector point : points)
				record.m_aCircuit.Insert(point);
		}

		IEntity cycleEntity = SpawnAt(m_sCycleWaypointPrefab, points[0]);
		AIWaypointCycle cycle = AIWaypointCycle.Cast(cycleEntity);
		if (!cycle)
		{
			if (cycleEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(cycleEntity);
			// Repli : circuit mené par PatrolTick, point par point
			record.m_bCycle = false;
			record.m_iCircuitIndex = 0;
			OrderMove(record, points[0]);
			record.m_iNextPatrolTick = System.GetTickCount() + 12 * 60 * 1000;
			return;
		}

		array<AIWaypoint> steps = {};
		foreach (vector point : points)
		{
			IEntity stepEntity = SpawnAt(m_sMoveWaypointPrefab, point);
			AIWaypoint step = AIWaypoint.Cast(stepEntity);
			if (!step)
			{
				if (stepEntity)
					SCR_EntityHelper.DeleteEntityAndChildren(stepEntity);
				continue;
			}
			steps.Insert(step);
			record.m_aExtraWaypoints.Insert(stepEntity);
		}
		cycle.SetRerunCounter(-1);
		cycle.SetWaypoints(steps);
		record.m_Group.AddWaypointToGroup(cycle);
		record.m_Waypoint = cycleEntity;
		record.m_bCycle = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe d'assaut qui a pris un secteur devient sa garnison : point Defend sur place, rôle CRX de garnison, maintien
	//! de position dans le rayon du secteur. Un groupe que la file d'apparition de la 1.8 n'a pas encore servi est converti
	//! lui aussi (ses soldats arriveront réglés en garnison).
	void ConvertToGarrison(SRP_EnemyGroup record, vector center, float holdRadius)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		if (AliveAgents(record) == 0 && !InGraceOf(record, System.GetTickCount()))
			return;
		ClearWaypoints(record);
		record.m_bPatrol = false;
		record.m_bCycle = false;
		record.m_aCircuit.Clear();
		record.m_iSearchUntilTick = 0;
		record.m_sOwner = "territoire";
		vector position = GroupPosition(record);
		if (vector.Distance(position, center) > holdRadius)
			position = center;
		IEntity waypointEntity = SpawnDefendWaypoint(position, holdRadius);
		if (waypointEntity)
		{
			record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(waypointEntity));
			record.m_Waypoint = waypointEntity;
		}
		ApplyRole(record, SRP_ECRXRole.GARNISON, center, holdRadius);
		record.m_iBusyUntilTick = 0;
		record.m_iAssaultZone = -1;		// carte #75 : une garnison ne fait plus partie d'une contre-attaque
		NoteHome(record, "poste", true);
	}

	//------------------------------------------------------------------------------------------------
	//! Escorte d'un convoi au contact : tout le monde descend (GetOut), puis fouille et détruit vers le lieu du contact
	//! (SearchAndDestroy ; à défaut Move). Rôle CRX d'assaut.
	void OrderDismountAndAttack(SRP_EnemyGroup record, vector contact)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted() || AliveAgents(record) == 0)
			return;
		ClearWaypoints(record);
		AddExtraWaypoint(record, m_sGetOutWaypointPrefab, GroupPosition(record));
		SetAttackWaypoint(record, contact);
		ApplyRole(record, SRP_ECRXRole.ASSAUT, contact, -1);
		// Carte #69 : sa mission devient cet assaut (s'il part aider ailleurs, il revient ici ensuite)
		record.m_iBusyUntilTick = 0;
		NoteHome(record, "assaut", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Débarqués d'un camion de renfort en défense de la localité (carte #69, livraison 3) : tout le monde descend
	//! (GetOut), puis Defend sur « center » dans « radius », rôle de garnison ; leur poste d'origine est cette défense
	//! (tâche « débarqués » : ils peuvent partir aider, puis y reviennent)
	void OrderDismountAndDefend(SRP_EnemyGroup record, vector center, float radius)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted() || AliveAgents(record) == 0)
			return;
		ClearWaypoints(record);
		record.m_bPatrol = false;
		record.m_bCycle = false;
		record.m_iSearchUntilTick = 0;
		record.m_iArrivedTick = 0;
		AddExtraWaypoint(record, m_sGetOutWaypointPrefab, GroupPosition(record));
		IEntity waypointEntity = SpawnDefendWaypoint(center, radius);
		if (waypointEntity)
		{
			record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(waypointEntity));
			record.m_Waypoint = waypointEntity;
		}
		ApplyRole(record, SRP_ECRXRole.GARNISON, center, radius);
		record.m_iBusyUntilTick = 0;
		NoteHome(record, "débarqués", true);
	}

	//------------------------------------------------------------------------------------------------
	//! Point principal d'un assaut : fouille et détruit sur le contact (SearchAndDestroy ; à défaut Move)
	protected void SetAttackWaypoint(SRP_EnemyGroup record, vector contact)
	{
		ResourceName attackPrefab = m_sSearchDestroyWaypointPrefab;
		if (attackPrefab.IsEmpty())
			attackPrefab = m_sMoveWaypointPrefab;
		IEntity waypointEntity = SpawnAt(attackPrefab, contact);
		AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
		if (!waypoint && !attackPrefab.IsEmpty() && attackPrefab != m_sMoveWaypointPrefab)
		{
			if (waypointEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
			waypointEntity = SpawnAt(m_sMoveWaypointPrefab, contact);
			waypoint = AIWaypoint.Cast(waypointEntity);
		}
		if (waypoint)
		{
			record.m_Group.AddWaypointToGroup(waypoint);
			record.m_Waypoint = waypointEntity;
		}
		else if (waypointEntity)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Entraide (carte #69, livraison 2) : ordres donnés par SRP_EnemyAwareness
	//------------------------------------------------------------------------------------------------
	//! Aide : le groupe part vers le contact, d'abord par le point de contournement s'il y en a un (flank différent de
	//! vector.Zero : hors de vue du contact), puis fouille et détruit sur le contact. Rôle d'assaut : la boucle des sens
	//! retire le maintien de chaque soldat. Aucune position n'est injectée à sa perception (une cible signalée
	//! déclencherait l'enquête CRX tout droit et casserait le contournement). Occupé m_iBusyMaxMinutes au plus.
	void OrderHelp(SRP_EnemyGroup record, vector flank, vector contact, int now)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		record.m_bCycle = false;			// le cycle est supprimé avec les points de passage ; il est refait au retour
		record.m_iSearchUntilTick = 0;
		record.m_iArrivedTick = 0;
		if (flank != vector.Zero)
			AddExtraWaypoint(record, m_sMoveWaypointPrefab, flank);
		SetAttackWaypoint(record, contact);
		ApplyRole(record, SRP_ECRXRole.ASSAUT, contact, -1);
		record.m_sTask = "aide";
		record.m_iBusyUntilTick = now + Math.ClampInt(m_iBusyMaxMinutes, 1, 120) * 60 * 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Ratissage : le groupe fouille la dernière position connue, puis reçoit un nouveau point de fouille autour d'elle
	//! à intervalles (SRP_EnemyAwareness.RestoreTick), jusqu'à « untilTick ». Rôle d'assaut, sans maintien. Fouille et
	//! détruit (SearchAndDestroy, à défaut Move) : le point de passage répartit les binômes sur une grille de points du
	//! maillage de navigation autour du point, et la fouille de bâtiments CRX du rôle d'assaut les fait entrer dans les
	//! bâtiments (bâtiments compris).
	void OrderSweep(SRP_EnemyGroup record, vector lastKnown, int untilTick)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		int now = System.GetTickCount();
		ClearWaypoints(record);
		record.m_bCycle = false;
		record.m_iSearchUntilTick = 0;
		record.m_iArrivedTick = 0;
		SetAttackWaypoint(record, lastKnown);
		ApplyRole(record, SRP_ECRXRole.ASSAUT, lastKnown, -1);
		record.m_sTask = "ratissage";
		record.m_vSweepCenter = lastKnown;
		record.m_iBusyUntilTick = untilTick;
		record.m_iNextSweepTick = now + SRP_EnemyAwareness.s_iSweepStepMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Ratissage en cours : un nouveau point de fouille (SearchAndDestroy, à défaut Move) remplace le précédent
	void OrderSearch(SRP_EnemyGroup record, vector target)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		record.m_iArrivedTick = 0;
		SetAttackWaypoint(record, target);
	}

	//------------------------------------------------------------------------------------------------
	//! Retour au poste d'origine après l'aide ou le ratissage : groupe de défense, son point Defend et son maintien ;
	//! ronde en cycle, son cycle ; circuit ou patrouille de zone, l'étape en cours (PatrolTick reprend ensuite) ; sinon
	//! un déplacement vers son point d'origine. Tâche « retour » jusqu'à l'arrivée, puis la tâche d'origine.
	void RestoreHome(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		int now = System.GetTickCount();
		ClearWaypoints(record);
		record.m_bCycle = false;
		record.m_iSearchUntilTick = 0;
		record.m_iBusyUntilTick = 0;
		record.m_iArrivedTick = 0;

		if (record.m_bHomeDefend)
		{
			IEntity waypointEntity = SpawnDefendWaypoint(record.m_vHome, record.m_fHomeHold);
			if (waypointEntity)
			{
				record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(waypointEntity));
				record.m_Waypoint = waypointEntity;
			}
		}
		else if (record.m_bHomeCycle && record.m_aCircuit.Count() >= 2)
			OrderCycle(record, record.m_aCircuit);
		else if (!record.m_aCircuit.IsEmpty())
		{
			int index = Math.ClampInt(record.m_iCircuitIndex, 0, record.m_aCircuit.Count() - 1);
			OrderMove(record, record.m_aCircuit[index]);
			record.m_iNextPatrolTick = now + 12 * 60 * 1000;
		}
		else if (record.m_bPatrol)
		{
			OrderMove(record, record.m_vPatrolCenter);
			record.m_iNextPatrolTick = now + 5 * 60 * 1000;
		}
		else
			OrderMove(record, record.m_vHome);

		int role = record.m_iHomeRole;
		if (role < 0)
			role = SRP_ECRXRole.PATROUILLE;
		// Un groupe de garde rentre SANS maintien (rôle d'assaut, comme pendant l'aide) : le maintien autour de son poste,
		// posé dès le départ, ferait viser à chacune de ses enquêtes et de ses recherches de couvert un point près du poste
		// encore lointain (il romprait le contact pour rentrer). Rôle et maintien d'origine rétablis à l'arrivée
		// (SRP_EnemyAwareness.RestoreTick).
		if (role == SRP_ECRXRole.GARNISON && record.m_fHomeHold > 0)
			ApplyRole(record, SRP_ECRXRole.ASSAUT, record.m_vHome, -1);
		else
			ApplyRole(record, role, record.m_vHome, record.m_fHomeHold);
		record.m_sTask = "retour";
		record.m_iReturnUntilTick = now + 10 * 60 * 1000;
	}

	//------------------------------------------------------------------------------------------------
	// Ordres du Commandeur (carte #76, MA5, MA8, MA10) : donnés par SRP_CmdManeuvers et SRP_CmdSupport. Jamais la vraie
	// position d'un joueur (CO5) : les points viennent de ce que le Commandeur sait.
	//------------------------------------------------------------------------------------------------
	//! Tir de neutralisation sur un point (base de feu) : point « Suppress » à la hauteur voulue, tenu holdSeconds (sans
	//! effet si le prefab n'a pas de paramètres de durée : le Commandeur donne alors l'ordre suivant à l'heure prévue) ;
	//! à défaut, fouille et détruit sur le point. Rôle d'assaut, sans maintien.
	void OrderSuppress(SRP_EnemyGroup record, vector target, float holdSeconds, float height)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		BeginCmdOrder(record);
		IEntity waypointEntity = SpawnAt(m_sSuppressWaypointPrefab, target);
		SCR_SuppressWaypoint suppress = SCR_SuppressWaypoint.Cast(waypointEntity);
		if (suppress)
		{
			suppress.SetSuppressionHeight(height);
			suppress.SetHoldingTime(holdSeconds);
			record.m_Group.AddWaypointToGroup(suppress);
			record.m_Waypoint = waypointEntity;
		}
		else
		{
			if (waypointEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
			SetAttackWaypoint(record, target);
		}
		ApplyRole(record, SRP_ECRXRole.ASSAUT, target, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Attaque d'un point (objectif d'assaut, carré à reprendre) : point « Attack » SANS entité visée (CO5) ; à défaut,
	//! fouille et détruit sur le point. Rôle d'assaut, sans maintien.
	void OrderAttackPoint(SRP_EnemyGroup record, vector target)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		BeginCmdOrder(record);
		IEntity waypointEntity = SpawnAt(m_sAttackWaypointPrefab, target);
		AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
		if (waypoint)
		{
			record.m_Group.AddWaypointToGroup(waypoint);
			record.m_Waypoint = waypointEntity;
		}
		else
		{
			if (waypointEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
			SetAttackWaypoint(record, target);
		}
		ApplyRole(record, SRP_ECRXRole.ASSAUT, target, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Fumigènes à main pour couvrir un point (au plus « grenades ») : le point « DeploySmokeCover » passe DEVANT les
	//! points en cours, qui sont gardés et remis derrière (l'état du point lance les fumigènes puis le termine aussitôt,
	//! le groupe reprend son objectif). Rien si le prefab manque.
	void OrderSmokeCover(SRP_EnemyGroup record, vector protect, int grenades)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted() || grenades <= 0)
			return;
		IEntity smokeEntity = SpawnSmokeWaypoint(protect, grenades);
		if (!smokeEntity)
			return;

		// Les points en cours (l'objectif, et les points de route qui le précèdent) sont retirés du groupe sans être
		// supprimés, puis remis derrière le point de fumée, dans le même ordre
		array<AIWaypoint> current = {};
		record.m_Group.GetWaypoints(current);
		foreach (AIWaypoint waypoint : current)
		{
			if (waypoint)
				record.m_Group.RemoveWaypointFromGroup(waypoint);
		}
		record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(smokeEntity));
		record.m_aExtraWaypoints.Insert(smokeEntity);
		foreach (AIWaypoint kept : current)
		{
			if (kept && !kept.IsDeleted())
				record.m_Group.AddWaypointToGroup(kept);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Déplacement forcé (sortie d'une localité, repli) : point « ForcedMove » (priorité 2000 : le groupe ne s'arrête pas
	//! pour combattre) ; à défaut, un « Move » de priorité 2000. Rayon d'arrivée « radius ». Rôle d'assaut SANS maintien :
	//! un rôle de garnison le ramènerait à son poste.
	void OrderForcedMove(SRP_EnemyGroup record, vector target, float radius)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		BeginCmdOrder(record);
		IEntity waypointEntity = SpawnForcedMoveWaypoint(target, radius);
		if (waypointEntity)
		{
			record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(waypointEntity));
			record.m_Waypoint = waypointEntity;
		}
		ApplyRole(record, SRP_ECRXRole.ASSAUT, target, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Repli (décrochage #40, MA5, MA6) : dans l'ordre, fumigènes sur smokeAt si grenades > 0, déplacement forcé jusqu'à
	//! la sortie (30 m), puis déplacement vers la destination. Rôle d'assaut sans maintien ; tâche et poste d'origine
	//! « repli » (le groupe ne revient pas à son ancien poste).
	void OrderRetreat(SRP_EnemyGroup record, vector smokeAt, int grenades, vector exit, vector destination)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		BeginCmdOrder(record);
		if (grenades > 0)
		{
			IEntity smokeEntity = SpawnSmokeWaypoint(smokeAt, grenades);
			if (smokeEntity)
			{
				record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(smokeEntity));
				record.m_aExtraWaypoints.Insert(smokeEntity);
			}
		}
		IEntity exitEntity = SpawnForcedMoveWaypoint(exit, 30);
		if (exitEntity)
		{
			record.m_Group.AddWaypointToGroup(AIWaypoint.Cast(exitEntity));
			record.m_aExtraWaypoints.Insert(exitEntity);
		}
		IEntity destinationEntity = SpawnAt(m_sMoveWaypointPrefab, destination);
		AIWaypoint destinationWaypoint = AIWaypoint.Cast(destinationEntity);
		if (destinationWaypoint)
		{
			record.m_Group.AddWaypointToGroup(destinationWaypoint);
			record.m_Waypoint = destinationEntity;
		}
		else if (destinationEntity)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(destinationEntity);
		}
		ApplyRole(record, SRP_ECRXRole.ASSAUT, destination, -1);
		NoteHome(record, "repli", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Approche d'un objectif (renfort, raid) : comme l'aide (OrderHelp), sans tâche « aide » ni délai d'occupation :
	//! d'abord par « via » (point de route ou de contournement ; vector.Zero = direct), puis fouille et détruit sur
	//! l'objectif. Rôle d'assaut, sans maintien.
	void OrderApproach(SRP_EnemyGroup record, vector via, vector target)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		ClearWaypoints(record);
		BeginCmdOrder(record);
		if (via != vector.Zero)
			AddExtraWaypoint(record, m_sMoveWaypointPrefab, via);
		SetAttackWaypoint(record, target);
		ApplyRole(record, SRP_ECRXRole.ASSAUT, target, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Tient l'IA de chaque soldat du groupe (et du groupe) éveillée loin des joueurs (le jeu la coupe vers 1 km) ; redit
	//! à chaque passage par le Commandeur (les soldats livrés plus tard par la file sont pris au passage suivant) —
	//! SRP_CmdManeuvers (assauts, colonnes)
	static void HoldAwake(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		array<IEntity> members = {};
		GetMembers(record, members);
		foreach (IEntity member : members)
			SRP_EnemyTruckComponent.PreventLod(member);
		record.m_bAwake = true;
	}

	//------------------------------------------------------------------------------------------------
	//! L'IA du groupe peut de nouveau être coupée au loin (fin d'assaut, retrait du groupe) — SRP_CmdManeuvers, Delete
	static void ReleaseAwake(SRP_EnemyGroup record)
	{
		if (!record)
			return;
		record.m_bAwake = false;
		if (!record.m_Group || record.m_Group.IsDeleted())
			return;
		array<IEntity> members = {};
		GetMembers(record, members);
		foreach (IEntity member : members)
			SRP_EnemyTruckComponent.AllowLod(member);
		record.m_Group.AllowMaxLOD();		// le groupe lui-même, même sans soldat vivant
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats du groupe en état de combattre (vivants, conscients), où qu'ils soient — Commandeur, RetireTick
	static int FitOf(SRP_EnemyGroup record)
	{
		if (!record)
			return 0;
		array<ref SRP_EnemyGroup> single = {};
		single.Insert(record);
		return ActiveAgentsNear(single, vector.Zero, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Un ordre du Commandeur prend la main sur le groupe : plus de ronde (PatrolTick ne le relance plus), ni de recherche
	//! d'alerte, ni d'aide ou de ratissage en cours (l'entraide ne le ramènerait plus à son poste)
	protected void BeginCmdOrder(SRP_EnemyGroup record)
	{
		record.m_bPatrol = false;
		record.m_bCycle = false;
		record.m_iSearchUntilTick = 0;
		record.m_iArrivedTick = 0;
		record.m_iBusyUntilTick = 0;
		if (record.m_sTask == "aide" || record.m_sTask == "ratissage" || record.m_sTask == "retour")
			record.m_sTask = record.m_sHomeTask;
	}

	//------------------------------------------------------------------------------------------------
	//! Un point « DeploySmokeCover » réglé (fumigènes au plus, protéger le point), pas encore donné au groupe ; null si le
	//! prefab n'en donne pas un
	protected IEntity SpawnSmokeWaypoint(vector protect, int grenades)
	{
		IEntity entity = SpawnAt(m_sSmokeWaypointPrefab, protect);
		SCR_DeploySmokeCoverWaypoint smoke = SCR_DeploySmokeCoverWaypoint.Cast(entity);
		if (!smoke)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}
		smoke.SetMaxGrenadeCount(grenades);
		smoke.SetSmokeCoverProperties(SCR_AIActivitySmokeCoverFeatureProperties.PROTECT_POS);
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Un point de déplacement forcé, pas encore donné au groupe : « ForcedMove », sinon un « Move » de priorité 2000 ;
	//! rayon d'arrivée « radius » (> 0) ; null si aucun des deux prefabs ne donne un AIWaypoint
	protected IEntity SpawnForcedMoveWaypoint(vector target, float radius)
	{
		IEntity entity = SpawnAt(m_sForcedMoveWaypointPrefab, target);
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			entity = SpawnAt(m_sMoveWaypointPrefab, target);
			waypoint = AIWaypoint.Cast(entity);
			SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(entity);
			if (scripted)
				scripted.SetPriorityLevel(2000);
		}
		if (!waypoint)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}
		if (radius > 0)
			waypoint.SetCompletionRadius(radius);
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe disparaît (supprimé ou anéanti) : son IA n'est plus tenue éveillée, et le Commandeur le retire de ses
	//! replis, raids, plans et débarqués — Delete, Prune
	protected static void NotifyGroupGone(SRP_EnemyGroup record)
	{
		if (!record)
			return;
		if (record.m_bAwake)
			ReleaseAwake(record);
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers)
			maneuvers.OnGroupGone(record);
	}

	//------------------------------------------------------------------------------------------------
	// Vagues (contre-attaques de secteur, renforts de mission)
	//------------------------------------------------------------------------------------------------
	//! Une vague de « groups » groupes vers target. À pied depuis une origine à « distance » mètres environ, choisie par la
	//! brique commune (hors de vue, à 500 m au moins de tout joueur, sur une route ou en lisière, jamais près de la base) ;
	//! les groupes rejoignent la cible par la route puis à travers champs. Renforts en camion (carte #69, livraison 3) :
	//! même tirage qu'avant (m_iMotorizedChance × menace, carte #76 : menace LOCALE de la cible ; au moins m_iMinPlayers
	//! des camions) ; chaque camion (SRP_EnemyTruckComponent : un, deux dès 5 joueurs) remplace un groupe à pied, se gare à
	//! 150-250 m de la cible et tout le monde part à l'assaut. Retourne les groupes posés (passagers des camions compris).
	//! La place est demandée par l'appelant (SRP_CmdCapacity.Ask, COMBAT), qui classe ensuite les groupes posés (Tag).
	int SpawnWave(vector target, float distance, int groups, string owner, out array<ref SRP_EnemyGroup> outGroups)
	{
		if (groups <= 0 || !HasPrefabs())
			return 0;

		int spawned = 0;
		int footGroups = groups;
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (m_bMotorized && trucks && trucks.IsEnabled())
		{
			int nearPlayers = CountPlayersNear(target);
			if (nearPlayers >= MinPlayers(trucks.GetMinPlayers()) && RollThreat(m_iMotorizedChance, target))
			{
				int before = outGroups.Count();
				int trucksSent = trucks.SendWaveTruck(target, owner, nearPlayers, groups, outGroups);
				spawned += outGroups.Count() - before;
				footGroups -= trucksSent;
			}
		}

		if (footGroups > 0)
		{
			array<IEntity> players = SRP_Utils.GetPlayerCharacters();
			vector origin;
			if (!SRP_Placement.FindSpawnPosition(target, distance * 0.8, distance * 1.4, players, true, true, origin, 500))
			{
				float angle = Math.RandomFloat(0, Math.PI2);
				origin = SRP_Placement.OnGround(target + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
				if (!SRP_Placement.IsAcceptableFallback(origin, players, 300))
				{
					Journal("ENNEMI", string.Format("Vague (%1) : aucune origine hors de vue, et le repli tombe dans l'eau, près de la base ou sur les joueurs — vague annulée", owner));
					return spawned;
				}
				Journal("ENNEMI", string.Format("Vague (%1) : aucune origine hors de vue après %2 essais, repli sur l'ancien tirage", owner, SRP_Placement.TRIES));
			}
			spawned += SpawnGroups(origin, 60, footGroups, false, target, owner, outGroups, -1, true);
		}
		return spawned;
	}

	//------------------------------------------------------------------------------------------------
	//! Une vague qui part d'un lieu précis (contre-attaque depuis un point en profondeur de la zone rouge d'en face,
	//! renfort d'une garnison) : les groupes sont posés autour de « origin » (hors de vue, à 500 m au moins des joueurs,
	//! sur une route) et rejoignent la cible PAR LA ROUTE, à travers champs pour le dernier tronçon. L'appelant classe les
	//! groupes posés (SRP_CmdCapacity.Tag, COMBAT).
	int SpawnWaveFrom(vector origin, float originRadius, vector target, int groups, string owner, out array<ref SRP_EnemyGroup> outGroups)
	{
		if (groups <= 0 || !HasPrefabs())
			return 0;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		vector start;
		if (!SRP_Placement.FindSpawnPosition(origin, 0, Math.Max(originRadius, 60.0), players, true, false, start, 500))
		{
			start = origin;
			if (!SRP_Placement.IsAcceptableFallback(start, players, 300))
			{
				Journal("ENNEMI", string.Format("Vague depuis une zone (%1) : origine en vue ou trop près des joueurs — vague annulée", owner));
				return 0;
			}
			Journal("ENNEMI", string.Format("Vague depuis une zone (%1) : aucun point hors de vue après %2 essais, départ du point d'origine", owner, SRP_Placement.TRIES));
		}
		return SpawnGroups(start, 60, groups, false, target, owner, outGroups, -1, true);
	}

	//------------------------------------------------------------------------------------------------
	void AddExtraWaypoint(SRP_EnemyGroup record, ResourceName prefab, vector position)
	{
		if (prefab.IsEmpty() || !record.m_Group)
			return;
		IEntity entity = SpawnAt(prefab, position);
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}
		record.m_Group.AddWaypointToGroup(waypoint);
		record.m_aExtraWaypoints.Insert(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Chauffeur d'un camion de renfort (carte #69, livraison 3, SRP_EnemyTrucks) : le personnage est créé au point du
	//! véhicule, rangé dans un groupe vide de notre faction, IA activée et assis aux commandes dans la même image (modèle
	//! SpawnHeliPilots : la file d'apparition de la 1.8 n'y touche pas). Le camion le garde à bord (SetForceStayInVehicle)
	//! et empêche que son IA soit coupée au loin (PreventMaxLOD). Groupe compté dans la place des soldats (carte #76 :
	//! SRP_CmdCapacity, classe précisée par le camion). Null si le prefab est invalide ou s'il n'a pas pu s'asseoir (tout
	//! est alors supprimé).
	SRP_EnemyGroup SpawnVehicleDriver(ResourceName characterPrefab, IEntity vehicle, string owner)
	{
		if (!vehicle || characterPrefab.IsEmpty() || m_sEmptyGroupPrefab.IsEmpty())
			return null;
		vector position = vehicle.GetOrigin();
		IEntity driver = SpawnAt(characterPrefab, position);
		if (!driver)
			return null;
		SCR_AIGroup group = SCR_AIGroup.Cast(SpawnAt(m_sEmptyGroupPrefab, position));
		if (!group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(driver);
			return null;
		}
		FactionManager factions = GetGame().GetFactionManager();
		if (factions)
		{
			Faction faction = factions.GetFactionByKey(m_sFactionKey);
			if (faction)
				group.SetFaction(faction);
		}
		group.AddAgentFromControlledEntity(driver);
		AIControlComponent control = AIControlComponent.Cast(driver.FindComponent(AIControlComponent));
		if (control)
			control.ActivateAI();
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(driver.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.MoveInVehicle(vehicle, ECompartmentType.PILOT))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(driver);
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return null;
		}

		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = owner;
		record.m_Vehicle = vehicle;
		record.m_iExpected = 1;
		record.m_bSurrenderRolled = true;	// un chauffeur seul ne se rend pas
		record.m_bBudget = true;
		record.m_sTask = "chauffeur";
		m_aAll.Insert(record);
		ApplyRole(record, SRP_ECRXRole.ASSAUT, position, -1);
		NoteHome(record, "chauffeur", false);
		SRP_CmdCapacity.TagImportance(record);	// carte #76 : classe devinée (COMBAT) ; le camion la précise par Tag
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #76 (AP4) : équipage d'un blindé du Commandeur. Un groupe vide de notre faction au point du véhicule (clé de
	//! faction absente : le groupe garde celle de ses soldats), « seats » personnages créés au même point, IA activée et
	//! assis dans la même image (modèle SpawnHeliPilots) : le premier aux commandes, les suivants en tourelle ; celui qui
	//! ne trouve pas de place est supprimé. Chacun reste à bord (SetForceStayInVehicle) et son IA n'est jamais coupée au
	//! loin (le blindé roule loin des joueurs). Places vérifiées chaque seconde pendant 20 s (BoardVehicleCrew).
	//! Null si personne n'a pu s'asseoir (tout est alors supprimé) — SRP_CmdArmorManager.Launch (puis Tag COMBAT)
	SRP_EnemyGroup SpawnVehicleCrew(ResourceName crewPrefab, IEntity vehicle, int seats, string owner)
	{
		if (!vehicle || vehicle.IsDeleted() || crewPrefab.IsEmpty() || m_sEmptyGroupPrefab.IsEmpty() || seats <= 0)
			return null;
		Resource res = Resource.Load(crewPrefab);
		if (!res || !res.IsValid())
		{
			// Journal du Commandeur (Staff seul) : l'onglet Journal du PC ne parle pas de ses moyens
			Print("[SRP] IA ennemie : prefab d'équipage de blindé invalide " + crewPrefab, LogLevel.ERROR);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Équipage de blindé : prefab de soldat invalide (" + crewPrefab + ")");
			return null;
		}

		vector position = vehicle.GetOrigin();
		SCR_AIGroup group = SCR_AIGroup.Cast(SpawnAt(m_sEmptyGroupPrefab, position));
		if (!group)
			return null;
		FactionManager factions = GetGame().GetFactionManager();
		if (factions)
		{
			Faction faction = factions.GetFactionByKey(m_sFactionKey);
			if (faction)
				group.SetFaction(faction);
		}

		int seated = 0;
		for (int i = 0; i < seats; i++)
		{
			IEntity crewman = SpawnAt(crewPrefab, position);
			if (!crewman)
				continue;
			group.AddAgentFromControlledEntity(crewman);
			AIControlComponent control = AIControlComponent.Cast(crewman.FindComponent(AIControlComponent));
			if (control)
				control.ActivateAI();
			if (!SeatVehicleCrewman(crewman, vehicle, seated == 0))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(crewman);	// pas de place : supprimé, pas de traînard à pied
				continue;
			}
			SetupVehicleCrewman(crewman);
			seated++;
		}
		if (seated == 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Équipage de blindé : personne n'a pu s'asseoir à bord");
			return null;
		}

		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = owner;
		record.m_Vehicle = vehicle;
		record.m_iExpected = seated;
		record.m_bBudget = true;				// compté dans les 120 (champ gardé sans effet : SRP_CmdCapacity compte tout)
		record.m_bSurrenderRolled = true;		// un équipage de blindé ne se rend pas
		record.m_sTask = "blinde";				// avant ApplyRole : un équipage n'a pas de binômes
		m_aAll.Insert(record);
		ApplyRole(record, SRP_ECRXRole.ASSAUT, position, -1);
		NoteHome(record, "blinde", false);
		SRP_CmdCapacity.TagImportance(record);
		GetGame().GetCallqueue().CallLater(BoardVehicleCrew, 1000, false, record, 1);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Assied un membre d'équipage de blindé : aux commandes s'il est le premier (« driver »), sinon en tourelle ; un
	//! suivant qui ne trouve pas de tourelle prend les commandes si elles sont libres
	protected bool SeatVehicleCrewman(IEntity member, IEntity vehicle, bool driver)
	{
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;
		if (driver)
			return access.MoveInVehicle(vehicle, ECompartmentType.PILOT);
		if (access.MoveInVehicle(vehicle, ECompartmentType.TURRET))
			return true;
		return access.MoveInVehicle(vehicle, ECompartmentType.PILOT);
	}

	//------------------------------------------------------------------------------------------------
	//! Un membre d'équipage de blindé assis : il reste à bord, et son IA (et celle du groupe) n'est jamais coupée au loin
	protected void SetupVehicleCrewman(IEntity member)
	{
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (control)
		{
			AIAgent crewAgent = control.GetControlAIAgent();
			if (crewAgent)
			{
				SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(crewAgent.FindComponent(SCR_AIInfoComponent));
				if (info)
					info.SetForceStayInVehicle(true);
			}
		}
		SRP_EnemyTruckComponent.PreventLod(member);
	}

	//------------------------------------------------------------------------------------------------
	//! Places de l'équipage d'un blindé vérifiées chaque seconde pendant 20 s : un membre vivant qui n'est plus assis est
	//! rassis (tourelle, sinon commandes) ; passé les 20 s, celui qui ne trouve pas de place est supprimé (pas de
	//! traînard à pied à côté du blindé). S'arrête si le groupe a changé de tâche (blindé pris, équipage descendu).
	protected void BoardVehicleCrew(SRP_EnemyGroup record, int attempt)
	{
		if (!record || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return;
		if (!record.m_Group || record.m_Group.IsDeleted())
			return;
		if (record.m_sHomeTask != "blinde")
			return;
		IEntity vehicle = record.m_Vehicle;
		NoteSeen(record);	// avant de retirer ceux qui n'ont pas de place : ce ne sont pas des pertes
		array<IEntity> members = {};
		GetMembers(record, members);
		bool last = attempt >= 20;
		foreach (IEntity member : members)
		{
			if (SRP_Utils.IsDead(member) || IsSeatedIn(member, vehicle))
				continue;
			if (SeatVehicleCrewman(member, vehicle, false))
			{
				SetupVehicleCrewman(member);
				continue;
			}
			if (last)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(member);
				record.m_iRemoved = record.m_iRemoved + 1;
			}
		}
		if (!last)
			GetGame().GetCallqueue().CallLater(BoardVehicleCrew, 1000, false, record, attempt + 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Un véhicule laissé sur place (camion de renfort pris en embuscade, renversé ou en épave) : confié au nettoyage des
	//! transports, retiré après m_iTransportCleanupMinutes sans joueur à moins de 300 m (et sans groupe vivant à bord)
	void RegisterTransport(IEntity vehicle)
	{
		if (!vehicle || vehicle.IsDeleted() || m_aTransports.Contains(vehicle))
			return;
		m_aTransports.Insert(vehicle);
		m_aTransportIdleSince.Insert(0);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteExtraWaypoints(SRP_EnemyGroup record)
	{
		foreach (IEntity waypoint : record.m_aExtraWaypoints)
		{
			if (waypoint && !waypoint.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(waypoint);
		}
		record.m_aExtraWaypoints.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Les véhicules laissés sur place (camions de renfort pris en embuscade, renversés ou en épave ; jeeps) : retirés
	//! quand plus aucun joueur n'est à moins de 300 m depuis le délai réglé (un camion pris ou fouillé par les joueurs
	//! reste tant qu'ils sont autour). Un camion de renfort qui repart est retiré par SRP_EnemyTruckComponent, hors de vue.
	protected void CleanTransports(array<IEntity> players, int now)
	{
		for (int i = m_aTransports.Count() - 1; i >= 0; i--)
		{
			IEntity vehicle = m_aTransports[i];
			if (!vehicle || vehicle.IsDeleted())
			{
				m_aTransports.Remove(i);
				m_aTransportIdleSince.Remove(i);
				continue;
			}
			if (SRP_Utils.NearestPlayerDistance(vehicle.GetOrigin(), players) < 300)
			{
				m_aTransportIdleSince[i] = 0;
				continue;
			}
			if (m_aTransportIdleSince[i] == 0)
			{
				m_aTransportIdleSince[i] = now;
				continue;
			}
			if (now - m_aTransportIdleSince[i] < m_iTransportCleanupMinutes * 60 * 1000)
				continue;

			// Encore utilisé par un groupe vivant : on attend
			bool inUse = false;
			foreach (SRP_EnemyGroup record : m_aAll)
			{
				if (record.m_Vehicle == vehicle && AliveAgents(record) > 0)
					inUse = true;
			}
			if (inUse)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			m_aTransports.Remove(i);
			m_aTransportIdleSince.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le camp des ennemis (celui que rejoint l'ennemi en civil d'un contrôle routier)
	string GetFactionKey()
	{
		return m_sFactionKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Un personnage ennemi isolé (officier…) dans son propre groupe, faction m_sFactionKey
	SRP_EnemyGroup SpawnCharacterGroup(ResourceName characterPrefab, vector position, string owner)
	{
		if (characterPrefab.IsEmpty() || m_sEmptyGroupPrefab.IsEmpty())
			return null;
		if (CountAlive() >= m_iMaxGroups)
			return null;

		IEntity character = SpawnAt(characterPrefab, position);
		if (!character)
			return null;

		SCR_AIGroup group = SCR_AIGroup.Cast(SpawnAt(m_sEmptyGroupPrefab, position));
		if (!group)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(character);
			return null;
		}

		FactionManager factions = GetGame().GetFactionManager();
		if (factions)
		{
			Faction faction = factions.GetFactionByKey(m_sFactionKey);
			if (faction)
				group.SetFaction(faction);
		}
		group.AddAgentFromControlledEntity(character);
		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (control)
			control.ActivateAI();

		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = owner;
		record.m_iExpected = 1;
		m_aAll.Insert(record);

		// L'officier tient son poste (garde, maintien de position) ; sa visée de base est posée par SRP_EnemySenses
		ApplyRole(record, SRP_ECRXRole.GARNISON, position, m_fOfficerHoldRadius);
		NoteHome(record, "officier", false);	// cible d'une mission : il ne part jamais aider
		SRP_CmdCapacity.TagImportance(record);	// carte #76 : classe devinée ; le poseur la précise par Tag
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Les personnages d'un groupe (vivants)
	static void GetMembers(SRP_EnemyGroup record, out array<IEntity> members)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity character = agent.GetControlledEntity();
			if (character && !character.IsDeleted())
				members.Insert(character);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Patrouille en jeep armée
	//------------------------------------------------------------------------------------------------
	protected int CountVehiclePatrols()
	{
		int count = 0;
		int now = System.GetTickCount();
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			// Un équipage que la file d'apparition de la 1.8 n'a pas encore servi compte déjà
			if (record && !record.m_aCircuit.IsEmpty() && (AliveAgents(record) > 0 || InGraceOf(record, now)))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Le circuit d'une jeep : le site, puis les lieux-dits voisins (donc des routes) à moins de 2 500 m, jamais près de
	//! la base ; à défaut de voisins, quatre points à 450 m autour du site
	protected void BuildCircuit(vector center, out array<vector> circuit)
	{
		array<vector> places = {};
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
			places.Insert(zone.GetCenter());
		foreach (SRP_DeliveryPointComponent point : SRP_DeliveryPointComponent.GetPoints())
			places.Insert(point.GetOwner().GetOrigin());
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);

		circuit.Insert(center);
		for (int pick = 0; pick < 3; pick++)
		{
			float best = 2500;
			int bestIndex = -1;
			foreach (int index, vector place : places)
			{
				float distance = vector.Distance(place, center);
				if (distance < 250 || distance >= best)
					continue;
				if (baseMarker && vector.Distance(place, baseMarker.GetOrigin()) < m_fBaseSafeRadius)
					continue;
				bool taken = false;
				foreach (vector chosen : circuit)
				{
					if (vector.Distance(chosen, place) < 200)
						taken = true;
				}
				if (taken)
					continue;
				best = distance;
				bestIndex = index;
			}
			if (bestIndex < 0)
				break;
			circuit.Insert(places[bestIndex]);
		}

		if (circuit.Count() < 3)
		{
			for (int i = 0; i < 4; i++)
			{
				float angle = Math.PI2 * i / 4;
				vector position = center + Vector(Math.Cos(angle) * 450, 0, Math.Sin(angle) * 450);
				position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
				if (position[1] > 1)
					circuit.Insert(position);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Peut-être une jeep armée pour ce lieu gardé : assez de joueurs en approche, tirage (menace locale), plafond de
	//! jeeps, budget local, place des soldats (carte #76 : SRP_CmdCapacity.Ask, classe PATROUILLE, puis Tag). Elle fait le
	//! tour des lieux-dits voisins, en boucle, mitrailleur à son poste. Retourne null si rien n'est posé.
	//! « soldierCap » : ancien plafond des garnisons et camions, plus lu (la place est décidée par Ask seul).
	SRP_EnemyGroup MaybeSpawnVehiclePatrol(vector center, string owner, int soldierCap = -1)
	{
		if (!m_bVehiclePatrols || !HasPrefabs())
			return null;
		if (CountPlayersNear(center) < MinPlayers(m_iMotorizedMinPlayers) || !RollThreat(m_iVehiclePatrolChance, center))
			return null;
		int groupCap = m_iMaxGroups;
		if (owner != "mission")
			groupCap = GetTerritoryGroupCap();	// les groupes gardés pour les missions ne sont pas pour une jeep de territoire
		if (CountVehiclePatrols() >= m_iVehiclePatrolMax || CountAlive() >= groupCap)
			return null;
		// Budget local : une garnison de territoire (carte #69, livraison 3 : plusieurs sections) remplit à elle seule le
		// budget de son lieu ; sa jeep reste possible, dans la place des soldats (Ask ci-dessous)
		if (owner != "territoire" && LocalRoom(center) <= 0)
			return null;

		// Équipage : le groupe réglé, sinon un groupe au hasard (jamais le groupe de commandement)
		ResourceName crewPrefab = m_sPatrolCrewPrefab;
		if (crewPrefab.IsEmpty())
			crewPrefab = PickGroupPrefab();

		// Place des soldats (Q7, carte #76) : un élément de ronde, son équipage entier (la file le livre avant qu'il monte)
		int crewSize = Math.MaxInt(GroupSize(crewPrefab), 1);
		string refusal = SRP_CmdCapacity.Get().Ask("jeep-" + owner, SRP_ECmdCapClass.PATROUILLE, crewSize, 1, center, owner);
		if (!refusal.IsEmpty())
		{
			Journal("ENNEMI", string.Format("Jeep de patrouille (%1) : pas de jeep, %2", owner, refusal));
			return null;
		}

		array<vector> circuit = {};
		BuildCircuit(center, circuit);
		if (circuit.Count() < 2)
			return null;

		// Départ au point du circuit le plus loin des joueurs, hors de leur vue, jamais près de la base
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		int start = -1;
		float farthest = m_fSpawnMinDistance;
		foreach (int index, vector point : circuit)
		{
			float distance = SRP_Utils.NearestPlayerDistance(point, players);
			if (distance <= farthest)
				continue;
			if (SRP_Placement.IsNearBase(point) || SRP_Placement.IsSeenByAnyPlayer(point, players))
				continue;
			farthest = distance;
			start = index;
		}
		if (start < 0)
		{
			Journal("ENNEMI", string.Format("Jeep de patrouille (%1) : aucun point de départ hors de vue, pas de jeep", owner));
			return null;
		}

		ResourceName vehiclePrefab = "{0B4DEA8078B78A9B}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM.et";
		if (m_aPatrolVehiclePrefabs && !m_aPatrolVehiclePrefabs.IsEmpty())
			vehiclePrefab = m_aPatrolVehiclePrefabs.GetRandomElement();

		vector position = circuit[start];
		vector spot;
		if (SCR_WorldTools.FindEmptyTerrainPosition(spot, position, 25, 4, 2.5))
			position = spot;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.3;

		IEntity vehicle = SpawnAt(vehiclePrefab, position);
		if (!vehicle)
			return null;

		int next = start + 1;
		if (next >= circuit.Count())
			next = 0;

		// Équipage (choisi plus haut) ; owner « ambiance » = rôle CRX de patrouille
		SRP_EnemyGroup record = SpawnCrew(crewPrefab, position + Vector(5, 0, 0), circuit[next], owner);
		if (!record)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return null;
		}
		record.m_bBudget = soldierCap >= 0;		// champ gardé sans effet (carte #76 : SRP_CmdCapacity compte tout)
		record.m_Vehicle = vehicle;
		record.m_bPatrol = true;
		record.m_iCircuitIndex = next;
		foreach (vector step : circuit)
			record.m_aCircuit.Insert(step);
		record.m_iNextPatrolTick = System.GetTickCount() + 5 * 60 * 1000;
		m_aTransports.Insert(vehicle);
		m_aTransportIdleSince.Insert(0);
		SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.PATROUILLE);
		GetGame().GetCallqueue().CallLater(BoardCrew, 3000, false, record);

		Journal("ENNEMI", string.Format("Jeep de patrouille (%1) : circuit de %2 points autour du site", owner, circuit.Count()));
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe d'équipage, avec son premier ordre de route (réglages CRX de patrouille)
	protected SRP_EnemyGroup SpawnCrew(ResourceName prefab, vector position, vector target, string owner)
	{
		IEntity entity = SpawnAt(prefab, position);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}
		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = owner;
		record.m_iExpected = SlotCount(group);
		record.m_sTask = "jeep";	// avant ApplyRole : un équipage n'a pas de binômes (NoteHome la retient ensuite)
		m_aAll.Insert(record);
		OrderMove(record, target);
		ApplyRole(record, SRP_ECRXRole.PATROUILLE, target, -1);
		NoteHome(record, "jeep", false);
		SRP_CmdCapacity.TagImportance(record);	// carte #76 : classe devinée (PATROUILLE)
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Équipage de jeep : pilote, mitrailleur, un passager ; les soldats qui n'ont pas trouvé de place sont retirés
	//! (une jeep n'a que trois places : pas de traînards à pied derrière elle)
	protected void BoardCrew(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return;
		NoteSeen(record);	// avant de retirer les soldats sans place : ils ne comptent pas comme des pertes
		array<IEntity> members = {};
		GetMembers(record, members);
		foreach (int index, IEntity member : members)
		{
			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
			if (!access)
				continue;
			bool seated = false;
			if (index == 0)
				seated = access.MoveInVehicle(record.m_Vehicle, ECompartmentType.PILOT);
			else if (index == 1)
				seated = access.MoveInVehicle(record.m_Vehicle, ECompartmentType.TURRET);
			if (!seated && index > 0)
				seated = access.MoveInVehicle(record.m_Vehicle, ECompartmentType.CARGO);
			if (!seated && index > 0)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(member);
				record.m_iRemoved = record.m_iRemoved + 1;
				continue;
			}

			// Pilote et mitrailleur restent à leur poste au contact (une jeep armée qui se vide n'est plus une menace)
			if (index > 1)
				continue;
			AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
			if (!control)
				continue;
			AIAgent crewAgent = control.GetControlAIAgent();
			if (!crewAgent)
				continue;
			SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(crewAgent.FindComponent(SCR_AIInfoComponent));
			if (info)
				info.SetForceStayInVehicle(true);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Alerte : repérage, recherche, fusées éclairantes
	//------------------------------------------------------------------------------------------------
	//! Fait-il nuit ? (le jeu le sait d'après le lever et le coucher du soleil ; sinon les heures réglées)
	protected bool IsNight()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return false;
		TimeAndWeatherManagerEntity manager = world.GetTimeAndWeatherManager();
		if (!manager)
			return false;
		float hour = manager.GetTimeOfTheDay();
		float sunrise;
		float sunset;
		if (manager.GetSunriseHour(sunrise) && manager.GetSunsetHour(sunset))
			return hour < sunrise - 0.3 || hour > sunset + 0.3;
		return hour >= m_fNightStart || hour <= m_fNightEnd;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 10 s : ce que nos groupes ont repéré depuis le dernier passage. Un joueur vu (ou entendu) ouvre ou
	//! prolonge une alerte à l'endroit où il a été repéré. Avant le premier repérage, il ne se passe rien : on peut
	//! progresser sans être vu. Un joueur VU par un soldat d'une garnison de territoire demande aussi la sirène de la
	//! localité (carte #69 ; à chaque lecture : ce sont le repos et les drapeaux de la sirène qui limitent).
	protected void AlertTick()
	{
		int now = System.GetTickCount();
		PerceptionManager perception = GetGame().GetPerceptionManager();
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (perception)
		{
			float timeNow = perception.GetTime();
			// Deux passes : d'abord les joueurs IDENTIFIÉS (vus), puis les seulement DÉTECTÉS (entendus, ou silhouette pas
			// encore identifiée) : une alerte ouverte dans ce passage porte d'emblée le bon mot
			for (int pass = 0; pass < 2; pass++)
			{
				bool wantIdentified = pass == 0;
				foreach (SRP_EnemyGroup record : m_aAll)
				{
					if (!record || !record.m_Group || record.m_Group.IsDeleted() || AliveAgents(record) == 0)
						continue;
					SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(record.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
					if (!utility || !utility.m_Perception)
						continue;
					foreach (SCR_AITargetInfo target : utility.m_Perception.m_aTargets)
					{
						if (!target || !target.m_Entity || timeNow - target.m_fTimestamp > 15)
							continue;
						if (target.m_eCategory != EAITargetInfoCategory.IDENTIFIED && target.m_eCategory != EAITargetInfoCategory.DETECTED)
							continue;
						bool identified = target.m_eCategory == EAITargetInfoCategory.IDENTIFIED;
						if (identified != wantIdentified)
							continue;
						if (SRP_Utils.GetPlayerIdFromEntity(target.m_Entity) <= 0)
							continue;	// une IA, un civil : pas notre affaire
						record.m_iSpotTick = now;	// ce groupe est au contact : il ne sera pas envoyé aider ailleurs
						// Carte #76 (OF3) : le groupe en rendra compte au Commandeur (position perçue, jamais la vraie)
						SRP_Commander.NoteSighting(record, target.m_Entity, target.m_vWorldPos, identified);
						RaiseAlert(target.m_vWorldPos, identified, now, record.m_Group, target.m_fTimestamp);

						// Sirène : seulement un joueur VU (un simple bruit ne la déclenche jamais) par une garnison de territoire
						if (identified && territory && !record.m_sZone.IsEmpty())
							territory.OnGarrisonAlert(record.m_sZone, target.m_vWorldPos, record.m_Group, now);
					}
				}
			}
		}

		for (int i = m_aAlerts.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyAlert alert = m_aAlerts[i];
			if (now - alert.m_iLastTick > m_iAlertMinutes * 60 * 1000)
			{
				m_aAlerts.Remove(i);
				continue;
			}
			ServeAlert(alert, now);
		}

		// Entraide : ratisseurs guidés, groupes rendus à leur poste (même quand leur alerte s'est éteinte)
		SRP_EnemyAwareness.RestoreTick(this, now);
		CheckSurrenders(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Ouvre ou prolonge l'alerte la plus proche (400 m, à plat : une alerte sur une colline et une autre au pied ne font
	//! pas deux alertes). « source » : le groupe qui a repéré (null pour l'hélicoptère). « seen » : un groupe a IDENTIFIÉ
	//! le joueur (vus) ; sinon il n'est que DÉTECTÉ (entendus, ou silhouette pas encore identifiée). Une alerte qui passe
	//! d'« entendus » à « vus » écrit une ligne. Un contact repris après m_iContactLostSeconds sans repérage est traité
	//! comme nouveau : l'aide et le ratissage peuvent repartir. « contactTime » : heure de perception du repérage (-1 :
	//! inconnue).
	protected void RaiseAlert(vector position, bool seen, int now, SCR_AIGroup source = null, float contactTime = -1)
	{
		foreach (SRP_EnemyAlert existing : m_aAlerts)
		{
			if (vector.DistanceXZ(existing.m_vPosition, position) > 400)
				continue;
			if (now - existing.m_iLastTick >= SRP_EnemyAwareness.s_iContactLostMs)
			{
				existing.m_bHelpSent = false;
				existing.m_bSweepSent = false;
				existing.m_iContactTick = now;
				if (source)
					existing.m_SourceGroup = source;
			}
			// La dernière position connue (fusées, hélicoptère, aide, ratissage). Dans un même passage (les joueurs VUS
			// d'abord, puis les entendus), un joueur vu l'emporte sur un bruit, et à sorte égale le repérage le plus récent ;
			// d'un passage à l'autre, le premier repérage du passage la remplace
			bool keepPosition = false;
			if (existing.m_iLastTick == now)
			{
				if (existing.m_bPosSeen && !seen)
					keepPosition = true;
				else if (existing.m_bPosSeen == seen && contactTime >= 0 && existing.m_fPosTime > contactTime)
					keepPosition = true;
			}
			if (!keepPosition)
			{
				existing.m_vPosition = position;
				existing.m_bPosSeen = seen;
				existing.m_fPosTime = contactTime;
			}
			existing.m_iLastTick = now;
			if (seen && !existing.m_bSeen)
			{
				existing.m_bSeen = true;
				Journal("ENNEMI", string.Format("Alerte (grille %1 / %2) : les joueurs entendus sont maintenant VUS par l'ennemi", Math.Round(position[0]), Math.Round(position[2])));
			}
			if (!existing.m_SourceGroup && source)
				existing.m_SourceGroup = source;
			return;
		}

		SRP_EnemyAlert alert = new SRP_EnemyAlert();
		alert.m_vPosition = position;
		alert.m_bPosSeen = seen;
		alert.m_fPosTime = contactTime;
		alert.m_iFirstTick = now;
		alert.m_iLastTick = now;
		alert.m_iContactTick = now;
		alert.m_bSeen = seen;
		alert.m_SourceGroup = source;
		m_aAlerts.Insert(alert);

		// « vus » : un groupe a identifié le joueur ; « entendus » : seulement détecté (un bruit, ou une silhouette que
		// personne n'a encore identifiée)
		string how = "entendus (ou aperçus sans être identifiés)";
		if (seen)
			how = "vus";
		Journal("ENNEMI", string.Format("Alerte : des joueurs ont été %1 par l'ennemi (grille %2 / %3)", how, Math.Round(position[0]), Math.Round(position[2])));
	}

	//------------------------------------------------------------------------------------------------
	//! Ce que l'ennemi fait d'une alerte : les groupes proches deviennent vigilants sur place ; aide par le flanc, jeeps
	//! postées, ratissage du contact perdu (SRP_EnemyAwareness, carte #69) ; hélicoptère ; éclairer (la nuit, à intervalles)
	protected void ServeAlert(SRP_EnemyAlert alert, int now)
	{
		SRP_EnemyAwareness.SpreadVigilance(this, alert, now);
		if (m_bAlertSearch)
			SRP_EnemyAwareness.Serve(this, alert, now);

		// L'hélicoptère de recherche : un seul tirage par alerte, une fois les joueurs VUS, 30 s après le premier repérage.
		// Le tirage n'est pas gaspillé : tant qu'une condition manque (repos, joueurs, lieu…), on réessaie au passage suivant.
		// Tirage forcé par le Staff : vaut aussi pour une alerte déjà tirée (essai en plein accrochage)
		if (m_bHeliOnAlert && alert.m_bSeen && (!alert.m_bHeliRolled || m_bHeliForceNext) && now - alert.m_iFirstTick > 30000)
		{
			if (MaybeCallHeli(alert, now))
				alert.m_bHeliRolled = true;
		}

		if (!m_bFlares || m_sFlarePrefab.IsEmpty() || alert.m_iFlares >= m_iFlareMax || !IsNight())
			return;
		if (now - alert.m_iLastTick > 60000)
			return;		// on n'éclaire que ce qu'on vient de repérer
		if (alert.m_iLastFlareTick != 0 && now - alert.m_iLastFlareTick < m_iFlareIntervalSeconds * 1000)
			return;
		if (alert.m_iLastFlareTick == 0 && now - alert.m_iFirstTick < 12000)
			return;		// le temps de sortir le lance-fusées
		alert.m_iLastFlareTick = now;
		alert.m_iFlares++;
		FireFlare(alert.m_vPosition);
	}

	//------------------------------------------------------------------------------------------------
	//! L'ennemi fait-il venir son hélicoptère pour cette alerte ? SANS Commandeur seulement : avec lui (même gelé),
	//! l'hélicoptère ne part plus que sur son ordre HELICO (AP7, SRP_CmdSupport.RequestHeli). Il faut : aucun hélicoptère
	//! en vol, le repos écoulé, assez de joueurs sur place, une alerte liée à un objectif (territoire rouge, G12, ou
	//! mission active ou sa garde ; pas un accrochage en rase campagne côté bleu), loin de la base, et le tirage. La
	//! chance monte à chaque tirage raté.
	//! Retourne vrai si le tirage a eu lieu (l'alerte n'en aura pas d'autre) ; faux si une condition manque : on
	//! réessaiera au passage suivant, la raison est écrite une seule fois par alerte (catégorie HELICO).
	protected bool MaybeCallHeli(SRP_EnemyAlert alert, int now)
	{
		if (SRP_Commander.Get())
			return false;	// carte #76 : le Commandeur décide seul de l'hélicoptère (trou 11)
		if (SRP_HeliSearch.Count() > 0)
			return false;	// déjà en vol : l'alerte attend son retour
		if (!m_bHeliForceNext && now < m_iNextHeliTick)
		{
			HeliRefusal(alert, "repos", string.Format("repos encore %1 min", HeliRestMinutes(now)));
			return false;
		}
		int near = CountPlayersNear(alert.m_vPosition);
		int needed = MinPlayers(m_iHeliMinPlayers);
		if (near < needed)
		{
			HeliRefusal(alert, "joueurs", string.Format("%1 joueur(s) sur %2 requis", near, needed));
			return false;
		}
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (baseMarker)
		{
			float fromBase = vector.Distance(alert.m_vPosition, baseMarker.GetOrigin());
			if (fromBase < m_fBaseSafeRadius)
			{
				HeliRefusal(alert, "base", string.Format("alerte à %1 m de la base", Math.Round(fromBase)));
				return false;
			}
		}

		// Carte #75 (G12) : l'hélicoptère peut venir partout en territoire rouge ; ailleurs, seulement près d'une mission
		string place = "";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsRedAt(alert.m_vPosition))
			place = "territoire rouge";
		if (place.IsEmpty())
		{
			SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
			if (missions)
			{
				// Le site de la mission, et le centre de sa garde (parfois posée à 150-300 m du site)
				array<vector> sites = {};
				missions.GetActiveSites(sites);
				missions.GetActiveGuardCenters(sites);
				foreach (vector site : sites)
				{
					if (vector.Distance(site, alert.m_vPosition) <= m_fHeliSiteDistance)
						place = "mission";
				}
			}
		}
		if (place.IsEmpty())
		{
			HeliRefusal(alert, "lieu", "alerte hors mission et hors territoire rouge");
			return false;
		}

		// Radio de la garnison (carte #69, livraison 3) : opérateur radio hors de combat, le premier appel qui suit est
		// retardé (délai partagé avec le camion d'alerte) ; le tirage n'est pas gaspillé, on réessaie au passage suivant.
		// Un tirage forcé par le Staff passe outre (essai).
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!m_bHeliForceNext && territory && !territory.TryRadioCall(alert.m_vPosition, now))
		{
			HeliRefusal(alert, "radio", "opérateur radio hors de combat, appel retardé");
			return false;
		}

		// Le tirage : toutes les conditions sont réunies
		bool night = IsNight();
		int chance = HeliChance(night);
		bool forced = m_bHeliForceNext;
		m_bHeliForceNext = false;
		if (!forced && !Roll(chance))
		{
			m_iHeliMisses++;
			string rank = m_iHeliMisses.ToString() + "e";
			if (m_iHeliMisses == 1)
				rank = "1er";
			Journal("HELICO", string.Format("Alerte (%1) : tirage de l'hélicoptère à %2 pour cent raté (%3 de suite), prochaine chance %4 pour cent", place, chance, rank, HeliChance(night)));
			return true;
		}

		if (forced)
			Journal("HELICO", "Alerte (" + place + ") : tirage de l'hélicoptère forcé par le Staff, gagné");
		else
			Journal("HELICO", string.Format("Alerte (%1) : tirage de l'hélicoptère à %2 pour cent gagné", place, chance));
		m_iHeliMisses = 0;
		int rest = m_iHeliCooldownMinutes;
		if (TestMode())
			rest = 2;
		m_iNextHeliTick = now + rest * 60 * 1000;
		string result = SRP_HeliSearch.Launch(alert.m_vPosition, "alerte, " + place);
		Journal("HELICO", "Alerte (" + place + ") : l'ennemi fait venir son hélicoptère de recherche — " + result);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Raison d'un refus de l'hélicoptère, écrite une seule fois par alerte et par raison (« key » : repos, joueurs…)
	protected void HeliRefusal(SRP_EnemyAlert alert, string key, string text)
	{
		string tag = "[" + key + "]";
		if (alert.m_sHeliRefusal.Contains(tag))
			return;
		alert.m_sHeliRefusal = alert.m_sHeliRefusal + tag;
		Journal("HELICO", string.Format("Alerte (grille %1 / %2) : pas d'hélicoptère pour l'instant — %3", Math.Round(alert.m_vPosition[0]), Math.Round(alert.m_vPosition[2]), text));
	}

	//------------------------------------------------------------------------------------------------
	//! Chance du prochain tirage de l'hélicoptère, en pour cent : base de jour ou de nuit, plus le pas par tirage raté
	protected int HeliChance(bool night)
	{
		int chance = m_iHeliChance;
		if (night)
			chance = m_iHeliChanceNight;
		chance = chance + m_iHeliChanceStep * m_iHeliMisses;
		if (chance > m_iHeliChanceMax)
			chance = m_iHeliChanceMax;
		return chance;
	}

	//------------------------------------------------------------------------------------------------
	//! Minutes de repos restantes avant une nouvelle sortie de l'hélicoptère (arrondies au-dessus)
	protected int HeliRestMinutes(int now)
	{
		if (now >= m_iNextHeliTick)
			return 0;
		return (m_iNextHeliTick - now + 59999) / 60000;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : le prochain tirage de l'hélicoptère sera gagné, repos ignoré (les autres conditions restent : alerte en cours
	//! ou nouvelle, joueurs VUS, joueurs, lieu, base). Le drapeau retombe après ce tirage. Sans effet avec un Commandeur
	//! (carte #76 : plus de tirage ; le Staff force l'hélicoptère par la page Commandeur, ordre HELICO)
	void ForceNextHeliRoll()
	{
		if (SRP_Commander.Get())
		{
			// Journal du Commandeur (Staff seul) : l'onglet Journal du PC ne doit rien dire du Commandeur
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STAFF, "Tirage forcé de l'hélicoptère sans effet : il ne part que sur ordre HELICO (page Commandeur, forcer HELICO)");
			return;
		}
		m_bHeliForceNext = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : l'état de l'hélicoptère de recherche sur alerte, en une ligne
	string HeliStatus()
	{
		if (SRP_Commander.Get())
		{
			string flying = "au sol";
			if (SRP_HeliSearch.Count() > 0)
				flying = "en vol";
			return "Hélico : " + flying + " · sorties décidées par le Commandeur (ordre HELICO, AP7) : plus de tirage sur alerte ; pour le forcer, page Commandeur";
		}
		int now = System.GetTickCount();
		string state = "prêt";
		if (SRP_HeliSearch.Count() > 0)
			state = "en vol";
		else if (now < m_iNextHeliTick)
			state = string.Format("repos encore %1 min", HeliRestMinutes(now));
		if (!m_bHeliOnAlert)
			state = state + " (sortie sur alerte désactivée)";
		string forced = "non";
		if (m_bHeliForceNext)
			forced = "oui";
		return string.Format("Hélico : %1 · %2 tirage(s) raté(s) de suite · prochaine chance %3 pour cent le jour, %4 pour cent la nuit · prochain tirage forcé : %5", state, m_iHeliMisses, HeliChance(false), HeliChance(true), forced);
	}

	//------------------------------------------------------------------------------------------------
	//! Alerte (une fois, depuis SRP_EnemyAwareness.Serve) : la jeep de patrouille (à moins de 2 000 m) vient se poster à
	//! m_fJeepPostDistance de la dernière position connue. Les groupes à pied sont servis par l'entraide (aide par le
	//! flanc, ratissage) ; les groupes en garde ne bougent pas : CRX s'occupe de leur propre enquête.
	void SendJeeps(SRP_EnemyAlert alert, int now)
	{
		if (!alert)
			return;
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_Vehicle || !record.m_bPatrol || AliveAgents(record) == 0 || record.m_iSearchUntilTick > 0)
				continue;
			vector origin = GroupPosition(record);
			float distance = vector.Distance(origin, alert.m_vPosition);
			if (distance > 2000 || distance < 200)
				continue;
			vector away = origin - alert.m_vPosition;
			away[1] = 0;
			float length = away.Length();
			if (length < 1)
				continue;
			vector post = alert.m_vPosition + away * (m_fJeepPostDistance / length);
			post[1] = GetGame().GetWorld().GetSurfaceY(post[0], post[2]);
			OrderMove(record, post);
			record.m_iSearchUntilTick = now + 4 * 60 * 1000;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Une fusée éclairante blanche sous parachute, allumée au-dessus du lieu (décalée au hasard : l'ennemi tire au jugé)
	protected void FireFlare(vector position)
	{
		float angle = Math.RandomFloat(0, Math.PI2);
		float offset = Math.RandomFloat(10, 60);
		vector origin = position + Vector(Math.Cos(angle) * offset, 0, Math.Sin(angle) * offset);
		origin[1] = GetGame().GetWorld().GetSurfaceY(origin[0], origin[2]) + m_fFlareHeight;

		IEntity flare = SpawnAt(m_sFlarePrefab, origin);
		if (!flare)
			return;
		SRP_EnemySenses.AddFlare(origin, System.GetTickCount());	// les joueurs dessous sont éclairés pour l'ennemi
		ProjectileMoveComponent move = ProjectileMoveComponent.Cast(flare.FindComponent(ProjectileMoveComponent));
		if (move)
			move.Launch(Vector(0, -1, 0), vector.Zero, 0.05, flare, null, null, null, null);
		GetGame().GetCallqueue().CallLater(DeleteFlare, 90000, false, flare);
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteFlare(IEntity flare)
	{
		if (flare && !flare.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(flare);
	}

	//------------------------------------------------------------------------------------------------
	//! L'hélicoptère tient un joueur dans son faisceau : il renseigne les troupes au sol. L'alerte est ouverte ou
	//! prolongée à cette position, et toutes les 30 s les patrouilles à pied à moins de 900 m y sont (re)dirigées,
	//! la jeep vient se poster à 150 m. Les groupes en garde restent à leur poste.
	void ReportSpotted(vector position)
	{
		SRP_Commander.NoteAirSighting(position);	// carte #76 (AP7) : l'hélicoptère renseigne directement le Commandeur
		int now = System.GetTickCount();
		RaiseAlert(position, true, now);
		if (now < m_iNextSpottedOrderTick)
			return;
		m_iNextSpottedOrderTick = now + 30000;

		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_bPatrol || AliveAgents(record) == 0)
				continue;
			if (record.m_iBusyUntilTick > now)
				continue;	// parti aider ou ratisser (carte #69) : il garde ses ordres
			vector origin = GroupPosition(record);
			float distance = vector.Distance(origin, position);
			if (record.m_Vehicle)
			{
				if (distance > 2000 || distance < 200)
					continue;
				vector away = origin - position;
				away[1] = 0;
				vector post = position + away * (150 / away.Length());
				post[1] = GetGame().GetWorld().GetSurfaceY(post[0], post[2]);
				OrderMove(record, post);
			}
			else
			{
				if (distance > 900)
					continue;
				OrderMove(record, position);
			}
			record.m_iSearchUntilTick = now + 4 * 60 * 1000;
		}
	}

	//------------------------------------------------------------------------------------------------
	bool IsNightNow()
	{
		return IsNight();
	}

	//------------------------------------------------------------------------------------------------
	//! L'alerte vivante la plus proche de ce lieu, dans « radius », à plat (hélicoptère de recherche)
	bool NearestAlert(vector center, float radius, out vector position)
	{
		float best = radius;
		bool found = false;
		foreach (SRP_EnemyAlert alert : m_aAlerts)
		{
			float distance = vector.DistanceXZ(alert.m_vPosition, center);
			if (distance > best)
				continue;
			best = distance;
			position = alert.m_vPosition;
			found = true;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! L'alerte vivante la plus RÉCENTE (dernier repérage le plus proche dans le temps) à moins de « radius » de ce lieu :
	//! l'hélicoptère de recherche se recentre sur la dernière position connue des joueurs
	bool MostRecentAlert(vector center, float radius, out vector position)
	{
		bool found = false;
		int latest = 0;
		foreach (SRP_EnemyAlert alert : m_aAlerts)
		{
			if (vector.DistanceXZ(alert.m_vPosition, center) > radius)
				continue;
			if (found && alert.m_iLastTick <= latest)
				continue;
			latest = alert.m_iLastTick;
			position = alert.m_vPosition;
			found = true;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Une fusée éclairante au-dessus de ce lieu, s'il fait nuit (hélicoptère de recherche)
	void FireFlareIfNight(vector position)
	{
		if (m_bFlares && !m_sFlarePrefab.IsEmpty() && IsNight())
			FireFlare(position);
	}

	//------------------------------------------------------------------------------------------------
	//! Équipage de l'hélicoptère de recherche. Avec le prefab de pilote réglé : deux pilotes créés AU POINT DE L'APPAREIL,
	//! rangés dans un groupe vide de notre faction et assis tout de suite (la file d'apparition de la 1.8 livre les soldats
	//! d'un groupe un par un : des traînards restaient au sol, parfois sans aucun pilote à bord). Sans prefab de pilote
	//! (vide ou invalide), ou si aucun des deux n'a pu s'asseoir : l'ancien équipage, un de nos groupes posé au sol sous
	//! l'appareil puis embarqué.
	//! Le plafond de groupes (m_iMaxGroups) ne s'applique pas : l'hélicoptère est déjà rare, et sans équipage il volerait
	//! sans pilote.
	SRP_EnemyGroup SpawnHeliCrew(IEntity heli, vector ground)
	{
		if (!heli)
			return null;
		SRP_EnemyGroup pilots = SpawnHeliPilots(heli);
		if (pilots)
		{
			SRP_CmdCapacity.TagImportance(pilots);	// carte #76 : HORS_COMPTE (propriétaire « helico »), Tag par l'appelant
			return pilots;
		}

		// Ancien équipage : un groupe au sol, embarqué chaque seconde pendant 20 s
		if (!HasPrefabs())
			return null;
		ResourceName crewPrefab = m_sPatrolCrewPrefab;
		if (crewPrefab.IsEmpty())
			crewPrefab = PickGroupPrefab();	// jamais le groupe de commandement (le tireur d'élite ne vient qu'avec le QG, en ville)
		IEntity entity = SpawnAt(crewPrefab, ground);
		SCR_AIGroup group = SCR_AIGroup.Cast(entity);
		if (!group)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return null;
		}
		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = "helico";
		record.m_Vehicle = heli;
		record.m_bSurrenderRolled = true;	// un équipage d'hélicoptère ne se rend pas (même retiré après une chute)
		record.m_iExpected = Math.Min(SlotCount(group), 2);	// deux pilotes à bord, les autres sont retirés
		record.m_sTask = "helico";
		record.m_sHomeTask = "helico";
		m_aAll.Insert(record);
		if (m_bUseCRX)
			SRP_CRX.Apply(group, SRP_ECRXRole.PATROUILLE, ground, -1);
		GetGame().GetCallqueue().CallLater(BoardHeliCrew, 1000, false, record, 1);
		SRP_CmdCapacity.TagImportance(record);	// carte #76 : HORS_COMPTE (propriétaire « helico »), Tag par l'appelant
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Deux pilotes (prefab m_sHeliPilotPrefab) créés au point de l'appareil et assis tout de suite : le premier aux
	//! commandes, le second aux commandes, sinon en tourelle, sinon en soute ; celui qui ne peut pas s'asseoir est
	//! supprimé. Null si le prefab est vide ou invalide, ou si aucun pilote n'a pu s'asseoir.
	protected SRP_EnemyGroup SpawnHeliPilots(IEntity heli)
	{
		if (m_sHeliPilotPrefab.IsEmpty() || m_sEmptyGroupPrefab.IsEmpty())
			return null;
		Resource res = Resource.Load(m_sHeliPilotPrefab);
		if (!res || !res.IsValid())
		{
			Journal("HELICO", "Prefab de pilote invalide (" + m_sHeliPilotPrefab + ") : équipage à l'ancienne, posé au sol puis embarqué");
			return null;
		}

		vector position = heli.GetOrigin();
		SCR_AIGroup group = SCR_AIGroup.Cast(SpawnAt(m_sEmptyGroupPrefab, position));
		if (!group)
			return null;
		FactionManager factions = GetGame().GetFactionManager();
		if (factions)
		{
			Faction faction = factions.GetFactionByKey(m_sFactionKey);
			if (faction)
				group.SetFaction(faction);
		}

		int seated = 0;
		for (int i = 0; i < 2; i++)
		{
			IEntity pilot = SpawnAt(m_sHeliPilotPrefab, position);
			if (!pilot)
				continue;
			group.AddAgentFromControlledEntity(pilot);
			AIControlComponent control = AIControlComponent.Cast(pilot.FindComponent(AIControlComponent));
			if (control)
				control.ActivateAI();
			if (!SeatHeliCrewman(pilot, heli, seated > 0))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(pilot);	// pas de place : supprimé, il ne tombe pas du ciel
				continue;
			}
			SetupHeliPilot(pilot);
			seated++;
		}
		if (seated == 0)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			Journal("HELICO", "Équipage de l'hélicoptère : aucun pilote n'a pu s'asseoir à bord, équipage à l'ancienne (posé au sol puis embarqué)");
			return null;
		}

		SRP_EnemyGroup record = new SRP_EnemyGroup();
		record.m_Group = group;
		record.m_iSpawnTick = System.GetTickCount();
		record.m_sOwner = "helico";
		record.m_Vehicle = heli;
		record.m_bSurrenderRolled = true;	// un équipage d'hélicoptère ne se rend pas (même retiré après une chute)
		record.m_iExpected = seated;
		record.m_sTask = "helico";
		record.m_sHomeTask = "helico";
		m_aAll.Insert(record);
		if (m_bUseCRX)
			SRP_CRX.Apply(group, SRP_ECRXRole.PATROUILLE, position, -1);
		Journal("HELICO", string.Format("Équipage de l'hélicoptère : %1 pilote(s) créé(s) et assis directement à bord", seated));
		// Vérification chaque seconde pendant 20 s : un pilote qui ne serait finalement pas assis est réessayé, puis
		// supprimé (jamais de traînard sous l'appareil)
		GetGame().GetCallqueue().CallLater(BoardHeliCrew, 1000, false, record, 1);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Assied un membre d'équipage dans l'appareil : aux commandes ; « other » : sinon en tourelle, sinon en soute
	protected bool SeatHeliCrewman(IEntity member, IEntity heli, bool other)
	{
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
		if (!access)
			return false;
		if (access.MoveInVehicle(heli, ECompartmentType.PILOT))
			return true;
		if (!other)
			return false;
		if (access.MoveInVehicle(heli, ECompartmentType.TURRET))
			return true;
		return access.MoveInVehicle(heli, ECompartmentType.CARGO);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce personnage est-il assis dans CE véhicule ? (équipage de l'hélicoptère : embarquement, pilotes hors de combat)
	static bool IsSeatedIn(IEntity member, IEntity vehicle)
	{
		if (!member || !vehicle)
			return false;
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
		return access && access.IsInCompartment() && access.GetVehicle() == vehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! Un pilote assis : il ne descend jamais, et ne touche pas aux feux de l'appareil
	protected void SetupHeliPilot(IEntity member)
	{
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (!control)
			return;
		AIAgent pilotAgent = control.GetControlAIAgent();
		if (!pilotAgent)
			return;
		SCR_AIInfoComponent info = SCR_AIInfoComponent.Cast(pilotAgent.FindComponent(SCR_AIInfoComponent));
		if (info)
			info.SetForceStayInVehicle(true);	// à 120 m du sol, personne ne descend

		// Un pilote IA « au repos » gère lui-même les phares de son véhicule et les coupe : on lui interdit d'y toucher
		SCR_AICharacterSettingsComponent settings = SCR_AICharacterSettingsComponent.Cast(pilotAgent.FindComponent(SCR_AICharacterSettingsComponent));
		if (settings)
			settings.AddCharacterSetting(SCR_AICharacterLightInteractionSetting.Create(SCR_EAISettingOrigin.EDITOR, SCR_EAIBehaviorCause.ALWAYS, false), false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Embarquement de l'équipage, chaque seconde pendant 20 s, puis toutes les 5 s pendant 5 min pour les traînards : tant
	//! que moins de deux pilotes sont assis, ceux qui arrivent (la file d'apparition de la 1.8 les livre un par un) sont
	//! assis ; dès que deux sont à bord, les autres sont supprimés, et passé les 20 s tous ceux qui ne peuvent pas s'asseoir
	//! le sont aussi (pas de traînard au sol).
	protected void BoardHeliCrew(SRP_EnemyGroup record, int attempt)
	{
		if (!record || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return;
		if (!record.m_Group || record.m_Group.IsDeleted())
			return;
		if (record.m_sOwner != "helico")
			return;		// après une chute, l'équipage est passé aux survivants (RetireLater) : on n'y touche plus
		IEntity heli = record.m_Vehicle;
		NoteSeen(record);	// avant de retirer ceux qui n'ont pas de place : ce ne sont pas des pertes
		array<IEntity> members = {};
		GetMembers(record, members);

		int seated = 0;
		foreach (IEntity aboard : members)
		{
			if (IsSeatedIn(aboard, heli))
				seated++;
		}

		bool last = attempt >= 20;
		foreach (IEntity member : members)
		{
			if (IsSeatedIn(member, heli))
				continue;
			if (seated < 2 && !SRP_Utils.IsDead(member) && SeatHeliCrewman(member, heli, seated > 0))
			{
				SetupHeliPilot(member);
				seated++;
				continue;
			}
			if (seated >= 2 || last)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(member);	// pas de place à bord : pas de traînard au sol
				record.m_iRemoved = record.m_iRemoved + 1;
			}
		}

		if (attempt < 80)
		{
			// Chaque seconde pendant 20 s ; ensuite toutes les 5 s pendant 5 min, pour les traînards que la file d'apparition
			// livre encore (assis s'il reste une place, sinon supprimés)
			int delay = 1000;
			if (attempt >= 20)
				delay = 5000;
			GetGame().GetCallqueue().CallLater(BoardHeliCrew, delay, false, record, attempt + 1);
		}
		if (attempt == 20)
			Journal("HELICO", string.Format("Équipage de l'hélicoptère : %1 pilote(s) à bord après 20 s d'embarquement", seated));
	}

	//------------------------------------------------------------------------------------------------
	//! Y a-t-il une alerte vivante à moins de « radius » de ce lieu, à plat ? (missions : l'alerte est donnée ; renfort
	//! d'une garnison attaquée)
	bool HasAlertNear(vector center, float radius)
	{
		foreach (SRP_EnemyAlert alert : m_aAlerts)
		{
			if (vector.DistanceXZ(alert.m_vPosition, center) <= radius)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Zones chaudes et froides (présence ambiante)
	//------------------------------------------------------------------------------------------------
	void AddZoneModifier(string tag, vector center, float radius, float factor, int durationMinutes)
	{
		RemoveZoneModifier(tag);
		SRP_EnemyZoneModifier modifier = new SRP_EnemyZoneModifier();
		modifier.m_sTag = tag;
		modifier.m_vCenter = center;
		modifier.m_fRadius = radius;
		modifier.m_fFactor = factor;
		if (durationMinutes > 0)
			modifier.m_iUntilTick = System.GetTickCount() + durationMinutes * 60 * 1000;
		m_aModifiers.Insert(modifier);
	}

	//------------------------------------------------------------------------------------------------
	void RemoveZoneModifier(string tag)
	{
		for (int i = m_aModifiers.Count() - 1; i >= 0; i--)
		{
			if (m_aModifiers[i].m_sTag == tag)
				m_aModifiers.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float ZoneFactor(vector position, int now)
	{
		float factor = 1;
		for (int i = m_aModifiers.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyZoneModifier modifier = m_aModifiers[i];
			if (modifier.m_iUntilTick > 0 && now > modifier.m_iUntilTick)
			{
				m_aModifiers.Remove(i);
				continue;
			}
			if (vector.Distance(modifier.m_vCenter, position) <= modifier.m_fRadius)
				factor *= modifier.m_fFactor;
		}
		return factor;
	}

	//------------------------------------------------------------------------------------------------
	IEntity SpawnAt(ResourceName prefab, vector position)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] IA ennemie : prefab invalide " + prefab, LogLevel.ERROR);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	// Nettoyage
	//------------------------------------------------------------------------------------------------
	static int AliveAgents(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return 0;
		return record.m_Group.GetAgentsCount();
	}

	//------------------------------------------------------------------------------------------------
	static int AliveAgentsIn(array<ref SRP_EnemyGroup> records)
	{
		int alive = 0;
		foreach (SRP_EnemyGroup record : records)
			alive += AliveAgents(record);
		return alive;
	}

	//------------------------------------------------------------------------------------------------
	// Pose de la 1.8 : la file d'apparition livre les soldats d'un groupe un par un (carte #69)
	//------------------------------------------------------------------------------------------------
	//! Retient le plus grand effectif vu d'un groupe, soldats retirés par nous compris : la file comble la place d'un
	//! soldat retiré (jeep, hélicoptère) tant qu'elle a des soldats à livrer, et ce remplaçant ne doit pas cacher une perte
	static void NoteSeen(SRP_EnemyGroup r)
	{
		if (!r)
			return;
		int seen = AliveAgents(r) + r.m_iRemoved;
		if (seen > r.m_iMaxSeen)
			r.m_iMaxSeen = seen;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe encore jamais vu avec un soldat, posé depuis moins que la grâce : la file ne l'a pas encore servi (un
	//! groupe retiré ne le sera jamais)
	static bool InGraceOf(SRP_EnemyGroup r, int now)
	{
		if (!r || !r.m_Group || r.m_Group.IsDeleted())
			return false;
		return r.m_iMaxSeen == 0 && now - r.m_iSpawnTick < s_iSpawnGraceMs;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats d'un groupe : les attendus moins les pertes réelles (morts, redditions ; pas les soldats retirés par nous),
	//! jamais moins que les vivants. Un groupe complet vaut donc ses vivants, un groupe en cours de pose ses attendus, et
	//! un soldat que la file n'a jamais livré ne compte pas comme une perte. Pose abandonnée : ses attendus (personne
	//! n'a été tué).
	static int SoldiersOf(SRP_EnemyGroup r, int now)
	{
		if (!r)
			return 0;
		if (r.m_bAbandoned)
			return r.m_iExpected;
		if (!r.m_Group || r.m_Group.IsDeleted())
			return 0;
		NoteSeen(r);
		int alive = AliveAgents(r);
		int lost = r.m_iMaxSeen - alive - r.m_iRemoved;
		if (lost < 0)
			lost = 0;
		int soldiers = r.m_iExpected - lost;
		if (soldiers < alive)
			soldiers = alive;
		return soldiers;
	}

	//------------------------------------------------------------------------------------------------
	static int SoldiersIn(array<ref SRP_EnemyGroup> records, int now)
	{
		int soldiers = 0;
		if (!records)
			return 0;
		foreach (SRP_EnemyGroup record : records)
			soldiers += SoldiersOf(record, now);
		return soldiers;
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque groupe a déjà eu un soldat, ou sa grâce est passée (la pose est terminée)
	static bool IsSettled(array<ref SRP_EnemyGroup> records, int now)
	{
		if (!records)
			return true;
		foreach (SRP_EnemyGroup record : records)
		{
			NoteSeen(record);
			if (InGraceOf(record, now))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose terminée et plus un seul soldat dans ces groupes
	static bool IsWipedOut(array<ref SRP_EnemyGroup> records, int now)
	{
		return IsSettled(records, now) && AliveAgentsIn(records) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats de tous nos groupes (attendus compris pour les groupes en cours de pose)
	int CountSoldiers()
	{
		int now = System.GetTickCount();
		return SoldiersIn(m_aAll, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats de ces groupes encore en état de combattre (vivants, conscients) à moins de « radius » du centre
	//! (radius <= 0 : partout). Un agent peut rester dans son groupe après la mort ou dans l'inconscience :
	//! c'est ce comptage qu'il faut pour une capture.
	static int ActiveAgentsNear(array<ref SRP_EnemyGroup> records, vector center, float radius)
	{
		int count = 0;
		foreach (SRP_EnemyGroup record : records)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			array<AIAgent> agents = {};
			record.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity entity = agent.GetControlledEntity();
				if (!entity || entity.IsDeleted() || SRP_Utils.IsDead(entity))
					continue;
				SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(entity.FindComponent(SCR_CharacterControllerComponent));
				if (controller && controller.IsUnconscious())
					continue;
				if (radius > 0 && vector.Distance(entity.GetOrigin(), center) > radius)
					continue;
				count++;
			}
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire un groupe : membres, groupe, point de passage (carte #76 : IA rendue au jeu, Commandeur prévenu)
	void Delete(SRP_EnemyGroup record)
	{
		if (!record)
			return;
		NotifyGroupGone(record);

		if (record.m_Waypoint && !record.m_Waypoint.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Waypoint);
		record.m_Waypoint = null;
		DeleteExtraWaypoints(record);

		if (record.m_Group && !record.m_Group.IsDeleted())
		{
			// La file d'apparition de la 1.8 ne doit plus rien livrer à ce groupe
			ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
			if (aiWorld)
				aiWorld.PurgeSpawnRequestsForGroup(record.m_Group);
			array<AIAgent> agents = {};
			record.m_Group.GetAgents(agents);
			foreach (AIAgent agent : agents)
			{
				if (!agent)
					continue;
				IEntity character = agent.GetControlledEntity();
				if (character && !character.IsDeleted())
					SCR_EntityHelper.DeleteEntityAndChildren(character);
			}
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Group);
		}
		record.m_Group = null;
		m_aAll.RemoveItem(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire tous les groupes de la liste, puis la vide. Carte #76 (MA2) : des envoyés d'une localité (débarqués d'un
	//! renfort entrés dans une garnison, qui gardent leur source) retirés vivants avec elle lui sont rendus
	//! (ReturnSurvivors, une seule fois : la source est effacée) avant Delete, comme par RetireTick
	void DeleteAll(array<ref SRP_EnemyGroup> records)
	{
		foreach (SRP_EnemyGroup record : records)
		{
			ReturnSurvivors(record);
			Delete(record);
		}
		records.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : tous les groupes posés par nous (garnisons, missions, patrouilles) et les zones chaudes
	void ResetAll()
	{
		// Camions de renfort : supprimés d'abord (leurs chauffeurs et passagers sont dans m_aAll)
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (trucks)
			trucks.ResetAll();
		for (int i = m_aAll.Count() - 1; i >= 0; i--)
			Delete(m_aAll[i]);
		m_aAll.Clear();
		m_aAmbient.Clear();
		m_aRetiring.Clear();
		m_aModifiers.Clear();
		foreach (IEntity transport : m_aTransports)
		{
			if (transport && !transport.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(transport);
		}
		m_aTransports.Clear();
		m_aTransportIdleSince.Clear();
		foreach (IEntity prisoner : m_aSurrendered)
		{
			if (prisoner && !prisoner.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(prisoner);
		}
		m_aSurrendered.Clear();
		m_aSurrenderedIdleSince.Clear();
		m_aAlerts.Clear();
		SRP_HeliSearch.StopAll();
	}

	//------------------------------------------------------------------------------------------------
	//! Oublie les groupes morts ou disparus (les corps restent). Un groupe encore vide dans sa grâce est gardé (la file
	//! d'apparition de la 1.8 livre ses soldats un par un : il compte dans le plafond, ses points de passage restent) ;
	//! toujours vide après la grâce, sa pose est abandonnée (demandes d'apparition purgées, groupe retiré). Carte #76 :
	//! chaque groupe oublié est signalé au Commandeur (OnGroupGone, après la boucle ; Delete le fait pour les autres).
	protected void Prune()
	{
		int now = System.GetTickCount();
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		array<ref SRP_EnemyGroup> abandoned = {};
		array<ref SRP_EnemyGroup> evicted = {};
		array<ref SRP_EnemyGroup> vanished = {};
		for (int i = m_aAll.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup record = m_aAll[i];
			NoteSeen(record);
			if (AliveAgents(record) > 0 || InGraceOf(record, now))
				continue;
			if (record && record.m_iMaxSeen == 0 && record.m_Group && !record.m_Group.IsDeleted())
			{
				abandoned.Insert(record);	// retiré après la boucle (Delete le sort de m_aAll et purge ses demandes)
				continue;
			}
			// Évincé par le jeu (limite d'IA actives de la 1.8 : ChimeraAIWorld rend le groupe « dormant », ses soldats sont
			// supprimés, le groupe reste) : ce ne sont pas des pertes, et le groupe ne reviendrait jamais seul
			if (record && record.m_Group && !record.m_Group.IsDeleted() && record.m_Group.GetDormantAliveCount() > 0)
			{
				evicted.Insert(record);
				continue;
			}
			if (record)
			{
				// Plus un soldat : la file ne livre plus rien à ce groupe (des soldats sans point de passage, hors de tout décompte)
				if (aiWorld && record.m_Group && !record.m_Group.IsDeleted())
					aiWorld.PurgeSpawnRequestsForGroup(record.m_Group);
				if (record.m_Waypoint && !record.m_Waypoint.IsDeleted())
					SCR_EntityHelper.DeleteEntityAndChildren(record.m_Waypoint);
				DeleteExtraWaypoints(record);
				vanished.Insert(record);	// signalé après la boucle (le Commandeur ne doit pas toucher m_aAll pendant qu'on la parcourt)
			}
			m_aAll.Remove(i);
		}

		// Groupes anéantis : le Commandeur les retire de ses replis, raids et plans ; IA plus tenue éveillée
		foreach (SRP_EnemyGroup dead : vanished)
			NotifyGroupGone(dead);

		// Groupes évincés : retirés en entier (le groupe dormant ne ferait que traîner) ; leurs soldats vivants à l'éviction
		// deviennent leur effectif attendu et la pose est notée abandonnée, pour qu'une garnison les déduise de ses soldats
		// prévus (SRP_EnemyGarrison.DropAbandoned) et puisse les reposer plus tard, hors de vue
		foreach (SRP_EnemyGroup gone : evicted)
		{
			int dormant = gone.m_Group.GetDormantAliveCount();
			string goneOwner = gone.m_sOwner;
			string goneTask = gone.m_sTask;
			gone.m_iExpected = dormant;
			gone.m_bAbandoned = true;
			Delete(gone);
			Journal("ENNEMI", string.Format("Groupe évincé par le jeu (%1, %2) : %3 soldat(s) rendus dormants par la limite d'IA actives, groupe retiré", goneOwner, goneTask, dormant));
		}

		if (abandoned.IsEmpty())
			return;
		foreach (SRP_EnemyGroup lost : abandoned)
		{
			lost.m_bAbandoned = true;
			string owner = lost.m_sOwner;
			Delete(lost);
			Journal("ENNEMI", string.Format("Pose abandonnée (%1) : la file d'apparition n'a livré personne (budget IA ?)", owner));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pose de la 1.8 (toutes les 15 s, PatrolTick) : la grâce d'un groupe passée (m_iSpawnGraceSeconds), la file ne lui
	//! livre plus personne (demandes purgées, une fois). Sans cela, un soldat retenu par la limite de 128 IA actives
	//! apparaîtrait des minutes plus tard là où se trouve son groupe, peut-être au combat, sous les yeux d'un joueur, et
	//! un soldat tué pendant la pose serait remplacé. Groupe de territoire (garnison, camion, contre-attaque) : l'effectif
	//! attendu est ramené aux soldats livrés et gardés (plafond des garnisons et camions juste) ; une garnison déduit les
	//! manquants de ses soldats prévus (le complément pourra les reposer, hors de vue). Groupe de mission : attendus
	//! inchangés (l'alerte « garde tuée » compare aux attendus du départ).
	protected void CloseStalledSpawns(int now)
	{
		ChimeraAIWorld aiWorld = ChimeraAIWorld.Cast(GetGame().GetAIWorld());
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || record.m_bSpawnClosed || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (now - record.m_iSpawnTick < s_iSpawnGraceMs)
				continue;
			NoteSeen(record);
			if (record.m_iMaxSeen == 0)
				continue;	// personne n'a été livré : Prune abandonne la pose
			record.m_bSpawnClosed = true;
			bool complete = record.m_Group.IsExpandComplete();
			if (aiWorld)
				aiWorld.PurgeSpawnRequestsForGroup(record.m_Group);
			if (complete || record.m_sOwner != "territoire")
				continue;
			int kept = record.m_iMaxSeen - record.m_iRemoved;
			if (kept <= 0 || kept >= record.m_iExpected)
				continue;
			int undelivered = record.m_iExpected - record.m_iMaxSeen;
			record.m_iExpected = kept;	// soldats livrés et gardés (ceux retirés par nous, places manquantes d'une jeep, n'y sont plus)
			if (undelivered <= 0)
				continue;
			Journal("ENNEMI", string.Format("Pose incomplète (%1, %2) : %3 soldat(s) jamais livré(s) par la file d'apparition (limite d'IA actives ?), plus rien ne sera livré à ce groupe", record.m_sOwner, record.m_sTask, undelivered));
			if (territory && !record.m_sZone.IsEmpty() && SRP_EnemyGarrison.IsPlanTask(record.m_sHomeTask))
				territory.ReducePlanned(record.m_sZone, undelivered);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Survivants de mission : retirés après délai, et seulement hors de vue
	//------------------------------------------------------------------------------------------------
	//! Les groupes d'une mission finie (garde, vagues) ne sont plus retirés sur-le-champ : ils restent 15 min, puis sont
	//! retirés dès qu'aucun joueur ne les voit (ou tout de suite s'ils sont très loin). Un groupe encore en attente de la
	//! file d'apparition (personne n'a été livré) est retiré tout de suite : personne ne l'a vu, et une fois livré il
	//! n'appartiendrait plus à aucune liste. La liste de l'appelant est vidée. dueNow : échéance ramenée à maintenant
	//! (SRP_Territory.RetireSourced : débarqués rendus à leur source), retirés au prochain passage dès qu'hors de vue.
	void RetireLater(array<ref SRP_EnemyGroup> records, bool dueNow = false)
	{
		if (!records)
			return;
		int now = System.GetTickCount();
		int due = now + Math.Max(m_iRetireMinutes, 1) * 60 * 1000;
		if (dueNow)
			due = now;
		int kept = 0;
		array<ref SRP_EnemyGroup> pending = {};
		foreach (SRP_EnemyGroup record : records)
		{
			if (!record)
				continue;
			NoteSeen(record);
			if (AliveAgents(record) == 0)
			{
				if (InGraceOf(record, now))
					pending.Insert(record);	// retiré après la boucle (Delete purge ses demandes d'apparition)
				continue;	// morts : Prune s'en charge, les corps restent
			}
			record.m_iRetireTick = due;
			record.m_sOwner = "survivant";
			record.m_iAssaultZone = -1;		// carte #75 : un survivant ne fait plus partie d'une contre-attaque
			// Carte #76 : un survivant en retrait est une ronde pour la place des soldats (classe la plus basse, retirable
			// hors de vue pour faire place aux renforts, sauf s'il doit être rendu à sa localité)
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.PATROUILLE);
			if (!m_aRetiring.Contains(record))
				m_aRetiring.Insert(record);
			kept++;
		}
		records.Clear();
		if (kept > 0 && dueNow)
			Journal("ENNEMI", string.Format("%1 groupe(s) retiré(s) sans délai, dès qu'aucun joueur ne les voit", kept));
		else if (kept > 0)
			Journal("ENNEMI", string.Format("%1 groupe(s) survivant(s) gardé(s) %2 min, retirés ensuite hors de vue", kept, m_iRetireMinutes));
		if (pending.IsEmpty())
			return;
		foreach (SRP_EnemyGroup waiting : pending)
			Delete(waiting);
		Journal("ENNEMI", string.Format("%1 groupe(s) encore en attente de la file d'apparition retiré(s) tout de suite (personne n'avait été livré)", pending.Count()));
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe n'entre ici qu'avec des soldats (RetireLater retire tout de suite ceux que la file n'a pas encore servis) :
	//! vide, il est mort. Carte #76 : porte de retour des soldats envoyés par une localité (renfort, camion, colonne) :
	//! retirés vivants, ils lui sont rendus (ReturnSurvivors) juste avant Delete (qui prévient le Commandeur et rend l'IA
	//! au jeu). Seuls les groupes retirés sans passer par ici (DeleteAll d'une garnison, camion rappelé ou annulé avant le
	//! débarquement) appellent aussi ReturnSurvivors : la source effacée interdit tout double retour.
	protected void RetireTick(array<IEntity> players, int now)
	{
		for (int i = m_aRetiring.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup record = m_aRetiring[i];
			if (!record || AliveAgents(record) == 0)
			{
				m_aRetiring.Remove(i);
				continue;
			}
			vector position = GroupPosition(record);
			bool far = SRP_Placement.NearestPlayer(position, players) > m_fDespawnDistance;
			if (!far && now < record.m_iRetireTick)
				continue;
			if (!far && SRP_Placement.IsSeenByAnyPlayer(position, players))
				continue;	// on attend qu'ils soient hors de vue
			ReturnSurvivors(record);
			Delete(record);
			m_aRetiring.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #76 (MA2) : les soldats en état de combattre d'un groupe envoyé par une localité (m_sSourceLocality) sont
	//! rendus à cette localité (SRP_FrontEnemyComponent.ReturnSent), si le réglage renfort_rendre_survivants le veut
	//! (vrai sans Commandeur) ; une seule fois (la source est effacée). Toujours AVANT Delete (les vivants sont comptés
	//! par FitOf) — RetireTick, DeleteAll, et les camions (SRP_EnemyTruckComponent : passagers d'un camion rappelé ou
	//! annulé, retirés sans avoir débarqué)
	void ReturnSurvivors(SRP_EnemyGroup record)
	{
		if (!record || record.m_sSourceLocality.IsEmpty())
			return;
		string source = record.m_sSourceLocality;
		record.m_sSourceLocality = "";
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers && !maneuvers.m_bReturnSurvivors)
			return;
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (!frontEnemy)
			return;
		int survivors = FitOf(record);
		if (survivors <= 0)
			return;
		frontEnemy.ReturnSent(source, survivors);
		// Journal du Commandeur (Staff seul), pas le journal ENNEMI que l'onglet Journal du PC montre à tous
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Retour à %1 : %2 soldat(s) rentré(s) vivant(s)", source, survivors));
	}

	//------------------------------------------------------------------------------------------------
	// Reddition (ACE Captives) : un groupe réduit à un ou deux hommes, avec un joueur au contact, peut se rendre
	//------------------------------------------------------------------------------------------------
	//! Les soldats d'un groupe encore en état de combattre (vivants, conscients)
	protected void ActiveMembers(SRP_EnemyGroup record, out array<IEntity> members)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity entity = agent.GetControlledEntity();
			if (!entity || entity.IsDeleted() || SRP_Utils.IsDead(entity))
				continue;
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(entity.FindComponent(SCR_CharacterControllerComponent));
			if (controller && controller.IsUnconscious())
				continue;
			members.Insert(entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 10 s (depuis AlertTick) : un groupe d'au moins 3 hommes au départ, réduit à 1 ou 2 en état de combattre,
	//! avec un joueur à moins de 30 m de l'un d'eux : un tirage (une seule fois par groupe), et ils se rendent
	protected void CheckSurrenders(int now)
	{
		if (!m_bSurrender)
			return;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		if (players.IsEmpty())
			return;
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || record.m_bSurrenderRolled)
				continue;
			if (record.m_sOwner == "helico")
				continue;
			if (record.m_iInitialAgents == 0)
			{
				// Effectif de départ : le plus grand effectif vu, sans les soldats retirés par nous (places manquantes d'une
				// jeep). La file d'apparition de la 1.8 livre les soldats un par un : il est retenu une fois tous les attendus
				// vus, ou la grâce passée ; des morts pendant la pose ne le font donc pas baisser
				NoteSeen(record);
				if (record.m_iMaxSeen == 0)
					continue;	// personne n'a encore été livré
				if (record.m_iMaxSeen < record.m_iExpected && now - record.m_iSpawnTick < s_iSpawnGraceMs)
					continue;	// pose en cours
				record.m_iInitialAgents = record.m_iMaxSeen - record.m_iRemoved;
				if (record.m_iInitialAgents < 3)
					record.m_bSurrenderRolled = true;	// un équipage de deux ne « se réduit » pas : pas de reddition
				continue;
			}
			if (record.m_iInitialAgents < 3)
			{
				record.m_bSurrenderRolled = true;	// un équipage de deux ne « se réduit » pas : pas de reddition
				continue;
			}
			array<IEntity> active = {};
			ActiveMembers(record, active);
			if (active.IsEmpty() || active.Count() > 2)
				continue;

			bool close = false;
			foreach (IEntity member : active)
			{
				if (SRP_Placement.NearestPlayer(member.GetOrigin(), players) <= m_fSurrenderDistance)
					close = true;
			}
			if (!close)
				continue;

			record.m_bSurrenderRolled = true;
			if (!Roll(m_iSurrenderChance))
			{
				Journal("ENNEMI", string.Format("Groupe (%1) réduit à %2 homme(s) au contact : ils ne se rendent pas", record.m_sOwner, active.Count()));
				continue;
			}
			Surrender(record, active, players);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La reddition : arme lâchée, soldat sorti de son groupe (il compte comme neutralisé pour les missions et les
	//! captures), IA coupée, posture de reddition ACE Captives (les joueurs peuvent le ligoter et l'escorter)
	protected void Surrender(SRP_EnemyGroup record, array<IEntity> members, array<IEntity> players)
	{
		int surrendered = 0;
		vector where = vector.Zero;
		foreach (IEntity member : members)
		{
			ChimeraCharacter character = ChimeraCharacter.Cast(member);
			if (!character || character.IsInVehicle())
				continue;
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(member.FindComponent(SCR_CharacterControllerComponent));
			if (!controller)
				continue;

			BaseWeaponManagerComponent weapons = BaseWeaponManagerComponent.Cast(member.FindComponent(BaseWeaponManagerComponent));
			if (weapons)
			{
				WeaponSlotComponent slot = weapons.GetCurrentSlot();
				if (slot)
					controller.DropWeapon(slot);
			}

			AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
			if (control)
			{
				AIAgent agent = control.GetControlAIAgent();
				if (agent && record.m_Group && !record.m_Group.IsDeleted())
					record.m_Group.RemoveAgent(agent);
			}
			controller.ACE_Captives_SetSurrender(true);
			// IA coupée tout de suite après la posture (même image) : l'arbre de comportement ne tourne plus pendant l'entrée
			// dans le compartiment d'animation ACE (le jeu y levait deux exceptions « phy » à chaque reddition). L'entrée
			// dans ce compartiment passe par le personnage, pas par l'IA ; le rappel différé reste en filet.
			DeactivateSurrendered(member);
			GetGame().GetCallqueue().CallLater(DeactivateSurrendered, 1500, false, member);

			m_aSurrendered.Insert(member);
			m_aSurrenderedIdleSince.Insert(0);
			where = member.GetOrigin();
			surrendered++;
		}
		if (surrendered == 0)
			return;

		Journal("ENNEMI", string.Format("Reddition : %1 soldat(s) du groupe (%2) se rendent, grille %3 / %4", surrendered, record.m_sOwner, Math.Round(where[0]), Math.Round(where[2])));
		foreach (IEntity player : players)
		{
			if (!player || vector.Distance(player.GetOrigin(), where) > 200)
				continue;
			int playerId = SRP_Utils.GetPlayerIdFromEntity(player);
			if (playerId > 0)
				SRP_Utils.NotifyPlayer(playerId, "L'ennemi se rend !");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! L'IA d'un soldat qui s'est rendu est coupée juste après la posture, puis vérifiée 1,5 s plus tard (sans effet si
	//! elle est déjà coupée)
	protected void DeactivateSurrendered(IEntity member)
	{
		if (!member || member.IsDeleted())
			return;
		AIControlComponent control = AIControlComponent.Cast(member.FindComponent(AIControlComponent));
		if (control && control.IsAIActivated())
			control.DeactivateAI();
	}

	//------------------------------------------------------------------------------------------------
	//! Les soldats rendus : retirés après le délai réglé sans joueur à moins de 500 m (un prisonnier escorté reste)
	protected void CleanSurrendered(array<IEntity> players, int now)
	{
		for (int i = m_aSurrendered.Count() - 1; i >= 0; i--)
		{
			IEntity member = m_aSurrendered[i];
			if (!member || member.IsDeleted() || SRP_Utils.IsDead(member))
			{
				m_aSurrendered.Remove(i);
				m_aSurrenderedIdleSince.Remove(i);
				continue;
			}
			if (SRP_Placement.NearestPlayer(member.GetOrigin(), players) < 500)
			{
				m_aSurrenderedIdleSince[i] = 0;
				continue;
			}
			if (m_aSurrenderedIdleSince[i] == 0)
			{
				m_aSurrenderedIdleSince[i] = now;
				continue;
			}
			if (now - m_aSurrenderedIdleSince[i] < m_iSurrenderedCleanupMinutes * 60 * 1000)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(member);
			m_aSurrendered.Remove(i);
			m_aSurrenderedIdleSince.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Présence ambiante
	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		Prune();

		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		int now = System.GetTickCount();

		// Entretien des patrouilles
		for (int i = m_aAmbient.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup record = m_aAmbient[i];
			if (AliveAgents(record) == 0)
			{
				if (InGraceOf(record, now))
					continue;	// pose en cours (file d'apparition de la 1.8) : on garde la patrouille à entretenir
				m_aAmbient.Remove(i);
				continue;
			}

			vector position = record.m_Group.GetOrigin();
			bool expired = now - record.m_iSpawnTick > m_iAmbientLifetimeMinutes * 60 * 1000;
			bool far = SRP_Utils.NearestPlayerDistance(position, players) > m_fDespawnDistance;
			if (expired || far)
			{
				Delete(record);
				m_aAmbient.Remove(i);
			}
		}

		CleanTransports(players, now);
		RetireTick(players, now);
		CleanSurrendered(players, now);

		if (!HasPrefabs() || players.IsEmpty())
			return;

		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		array<vector> missionSites = {};
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
			missions.GetActiveSites(missionSites);
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();

		// Un tirage par GROUPE de joueurs (ceux à moins de 500 m d'un joueur déjà tiré ne retirent pas) : huit soldats
		// ensemble n'attirent pas huit fois plus de patrouilles qu'un seul. Un joueur en délai de grâce ne tire pas.
		array<vector> rolled = {};
		array<IEntity> active = GetActivePlayers();
		foreach (IEntity player : active)
		{
			if (m_aAmbient.Count() >= m_iAmbientMax || CountAlive() >= GetTerritoryGroupCap())
				break;	// les groupes gardés pour les missions ne vont jamais aux patrouilles ambiantes

			vector origin = player.GetOrigin();

			// Carte #75 (F10) : une patrouille de fond ne naît que côté rouge ; côté bleu, seulement en infiltration près
			// du rouge (tirage fait par le front). Sans ennemi du front : l'ancienne règle, partout
			bool infiltration = false;
			if (frontEnemy)
			{
				if (!frontEnemy.AllowAmbientFor(player))
					continue;
				infiltration = !front || !front.IsRedAt(origin);
			}

			if (baseMarker && vector.Distance(origin, baseMarker.GetOrigin()) < m_fBaseSafeRadius)
				continue;

			bool sameCluster = false;
			foreach (vector previous : rolled)
			{
				if (vector.Distance(previous, origin) < 500)
					sameCluster = true;
			}
			if (sameCluster)
				continue;
			rolled.Insert(origin);

			// Budget local : pas de patrouille en plus là où garnison, garde de mission ou vague occupent déjà le terrain
			if (LocalRoom(origin) <= 0)
				continue;

			float chance = m_fAmbientChance;
			foreach (vector site : missionSites)
			{
				if (vector.Distance(site, origin) < m_fMissionRadius)
				{
					chance = m_fMissionChance;
					break;
				}
			}
			// Carte #76 (CO9) : la menace LOCALE au joueur tiré (région du Commandeur), plus celle de toute l'île
			chance *= ZoneFactor(origin, now) * ThreatFactor(origin);

			if (chance <= 0 || Math.RandomFloat(0, 100) >= chance)
				continue;

			SpawnPatrolNear(player, players, infiltration);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Une patrouille ambiante près d'un joueur : une fois sur deux elle suit une route entre deux lieux-dits dont l'un est
	//! proche du joueur (ronde continue) ; sinon elle vient vers les abords du joueur, puis continue sa ronde sur les
	//! routes des environs. Posée hors de vue, à 600-1 200 m, jamais près de la base.
	//! Carte #75 (F10) : le point de pose est côté rouge (côté bleu près du rouge seulement pour une infiltration).
	//! Carte #76 (Q7) : place des soldats demandée d'abord (SRP_CmdCapacity.Ask, classe PATROUILLE) ; refus = pas de
	//! patrouille à ce passage ; la patrouille posée est classée PATROUILLE (Tag).
	protected void SpawnPatrolNear(IEntity player, array<IEntity> players, bool infiltration)
	{
		// Patrouille légère (2 à 4 soldats), sinon un groupe au hasard : le prefab est choisi avant la demande de place
		ResourceName prefab = PickLightPrefab();
		if (prefab.IsEmpty())
			prefab = PickGroupPrefab();
		vector origin = player.GetOrigin();
		string key = string.Format("ambiance-%1", SRP_Utils.GetPlayerIdFromEntity(player));
		string refusal = SRP_CmdCapacity.Get().Ask(key, SRP_ECmdCapClass.PATROUILLE, Math.MaxInt(GroupSize(prefab), 1), 1, origin, "ambiance");
		if (!refusal.IsEmpty())
			return;

		if (Math.RandomInt(0, 100) < m_iAmbientRoadPercent && SpawnRoadPatrol(player, players, infiltration, prefab))
			return;

		// Jusqu'à 4 recherches hors de vue ; un point n'est gardé que s'il convient au front (F10)
		vector spawnPosition;
		bool found = false;
		for (int attempt = 0; attempt < 4 && !found; attempt++)
		{
			vector candidate;
			if (!SRP_Placement.FindSpawnPosition(origin, m_fSpawnMinDistance, m_fSpawnMaxDistance, players, false, true, candidate, m_fSpawnMinDistance))
				continue;
			if (!AcceptsAmbient(candidate, infiltration))
				continue;
			spawnPosition = candidate;
			found = true;
		}
		if (!found)
		{
			if (!LegacyAmbientPosition(origin, players, infiltration, spawnPosition))
				return;
			Journal("ENNEMI", string.Format("Patrouille ambiante : aucun point hors de vue qui convienne après 4 recherches de %1 essais, repli sur l'ancien tirage", SRP_Placement.TRIES));
		}
		float distance = vector.Distance(spawnPosition, origin);

		// Ordre : marcher vers les abords du joueur (200 m de lui, du côté opposé à l'apparition)
		vector toPlayer = origin - spawnPosition;
		toPlayer[1] = 0;
		float length = toPlayer.Length();
		vector target = origin;
		if (length > 1)
			target = origin - toPlayer * (200 / length);
		target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);

		// Carte #69, livraison 3 : les patrouilles hors mission restent légères (2 à 4 soldats)
		SRP_EnemyGroup record = SpawnGroup(spawnPosition, false, target, "ambiance", -1, vector.Zero, -1, true, prefab);
		if (!record)
			return;

		// Arrivée aux abords : la patrouille ne s'arrête pas, elle continue sa ronde sur les routes des environs
		record.m_bPatrol = true;
		record.m_vPatrolCenter = target;
		record.m_fPatrolRadius = 400;
		record.m_iNextPatrolTick = System.GetTickCount() + 12 * 60 * 1000;
		SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.PATROUILLE);

		m_aAmbient.Insert(record);
		Journal("ENNEMI", string.Format("Patrouille ambiante posée à %1 m de %2, elle vient voir les abords", Math.Round(distance), GetGame().GetPlayerManager().GetPlayerName(SRP_Utils.GetPlayerIdFromEntity(player))));
	}

	//------------------------------------------------------------------------------------------------
	//! L'ancien tirage d'une patrouille ambiante (distance aux joueurs, base, eau, emplacement libre ; sans ligne de vue) ;
	//! carte #75 (F10) : le point doit aussi convenir au front (côté rouge, ou bleu près du rouge en infiltration)
	protected bool LegacyAmbientPosition(vector origin, array<IEntity> players, bool infiltration, out vector result)
	{
		for (int attempt = 0; attempt < 6; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(m_fSpawnMinDistance, m_fSpawnMaxDistance);
			vector position = SRP_Placement.OnGround(origin + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
			if (SRP_Utils.NearestPlayerDistance(position, players) < m_fSpawnMinDistance)
				continue;
			if (SRP_Placement.IsNearBase(position) || SRP_Placement.IsWater(position))
				continue;
			vector spawnPosition;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(spawnPosition, position, 25, 2, 2))
				continue;
			if (!AcceptsAmbient(spawnPosition, infiltration))
				continue;
			result = spawnPosition;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Carte #75 (F10) : un point de pose de patrouille de fond convient-il au front ? Carré rouge ; carré bleu près du
	//! rouge seulement pour une infiltration (SRP_FrontEnemyComponent.AcceptAmbientSpawn). Sans ennemi du front : oui.
	protected bool AcceptsAmbient(vector position, bool infiltration)
	{
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (!frontEnemy)
			return true;
		return frontEnemy.AcceptAmbientSpawn(position, infiltration);
	}

	//------------------------------------------------------------------------------------------------
	//! Les lieux-dits de la carte (zones civiles, points de livraison)
	protected void GetPlaces(out array<vector> places)
	{
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
			places.Insert(zone.GetCenter());
		foreach (SRP_DeliveryPointComponent point : SRP_DeliveryPointComponent.GetPoints())
			places.Insert(point.GetOwner().GetOrigin());
	}

	//------------------------------------------------------------------------------------------------
	//! Patrouille sur une route : de A (le lieu-dit le plus proche du joueur) à B (un autre lieu-dit à 600-3 000 m de A),
	//! par les points de route, aller et retour en boucle. Posée au point du trajet le plus loin des joueurs, hors de vue.
	//! Carte #75 (F10) : A, B et le point de départ doivent convenir au front (côté rouge, ou bleu près du rouge en
	//! infiltration). « prefab » : la patrouille dont la place a déjà été accordée (SpawnPatrolNear).
	//! false si aucune route ne relie les deux lieux, ou aucun point de départ convenable.
	protected bool SpawnRoadPatrol(IEntity player, array<IEntity> players, bool infiltration, ResourceName prefab)
	{
		vector origin = player.GetOrigin();
		array<vector> places = {};
		GetPlaces(places);
		if (places.Count() < 2)
			return false;

		// A : le lieu-dit le plus proche du joueur (à moins de 1 500 m), jamais près de la base
		int a = -1;
		float bestA = 1500;
		foreach (int index, vector place : places)
		{
			float distance = vector.Distance(place, origin);
			if (distance >= bestA || SRP_Placement.IsNearBase(place))
				continue;
			if (!AcceptsAmbient(place, infiltration))
				continue;
			bestA = distance;
			a = index;
		}
		if (a < 0)
			return false;

		// B : un autre lieu-dit, à 600-3 000 m de A, au hasard parmi ceux qui conviennent
		array<int> candidates = {};
		foreach (int index, vector place : places)
		{
			if (index == a)
				continue;
			float distance = vector.Distance(place, places[a]);
			if (distance < 600 || distance > 3000 || SRP_Placement.IsNearBase(place))
				continue;
			if (!AcceptsAmbient(place, infiltration))
				continue;
			candidates.Insert(index);
		}
		if (candidates.IsEmpty())
			return false;
		int b = candidates.GetRandomElement();

		array<vector> route = {};
		SRP_Placement.BuildRoadRoute(places[a], places[b], 250, 0, route);
		if (route.IsEmpty())
			return false;

		// Le circuit : A, la route, B, puis la route à rebours (aller-retour en boucle)
		array<vector> circuit = {};
		circuit.Insert(places[a]);
		foreach (vector step : route)
			circuit.Insert(step);
		circuit.Insert(places[b]);
		for (int i = route.Count() - 1; i >= 0; i--)
			circuit.Insert(route[i]);

		// Départ : le point du circuit le plus loin des joueurs (600 m au moins, 2 500 m au plus), hors de vue
		int start = -1;
		float farthest = m_fSpawnMinDistance;
		foreach (int index, vector point : circuit)
		{
			float distance = SRP_Placement.NearestPlayer(point, players);
			if (distance <= farthest || distance > m_fDespawnDistance)
				continue;
			if (!AcceptsAmbient(point, infiltration))
				continue;
			if (SRP_Placement.IsSeenByAnyPlayer(point, players))
				continue;
			farthest = distance;
			start = index;
		}
		if (start < 0)
			return false;

		vector spawnPosition;
		if (!SRP_Placement.FindSpawnPosition(circuit[start], 0, 40, players, true, false, spawnPosition, m_fSpawnMinDistance) || !AcceptsAmbient(spawnPosition, infiltration))
			spawnPosition = circuit[start];

		int next = start + 1;
		if (next >= circuit.Count())
			next = 0;
		SRP_EnemyGroup record = SpawnGroup(spawnPosition, false, circuit[next], "ambiance", -1, vector.Zero, -1, false, prefab);	// légère (2 à 4)
		if (!record)
			return false;
		record.m_bPatrol = true;
		record.m_iCircuitIndex = next;
		foreach (vector point : circuit)
			record.m_aCircuit.Insert(point);
		record.m_iNextPatrolTick = System.GetTickCount() + 12 * 60 * 1000;
		SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.PATROUILLE);

		m_aAmbient.Insert(record);
		Journal("ENNEMI", string.Format("Patrouille ambiante sur route posée à %1 m de %2 : %3 points de route entre deux lieux-dits, ronde continue", Math.Round(farthest), GetGame().GetPlayerManager().GetPlayerName(SRP_Utils.GetPlayerIdFromEntity(player)), circuit.Count()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Sens de l'ennemi (carte #69) : passage régulier, accès pour les autres classes, pages Staff
	//------------------------------------------------------------------------------------------------
	//! Toutes les m_iSensesTickMs : ce que l'ennemi peut percevoir des joueurs vivants (lumière, posture, lampe,
	//! buisson), puis pour chaque groupe sauf l'hélicoptère : soldats arrivés réglés, environnement de chaque soldat,
	//! révélation de près la nuit, journal de repérage
	protected void SensesTick()
	{
		int now = System.GetTickCount();
		float timeNow = -1;
		PerceptionManager perception = GetGame().GetPerceptionManager();
		if (perception)
			timeNow = perception.GetTime();

		array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
		array<IEntity> living = {};
		foreach (IEntity player : everyone)
		{
			if (player && !player.IsDeleted() && !SRP_Utils.IsDead(player))
				living.Insert(player);
		}
		SRP_EnemySenses.BeginTick(living, now);

		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || record.m_sOwner == "helico")
				continue;
			NoteSeen(record);
			SRP_EnemySenses.UpdateGroup(this, record, now, timeNow);
			SRP_EnemySenses.NoteSightings(record, now, timeNow);
		}
		SRP_EnemySenses.EndTick();
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les groupes posés par nous (lecture seule)
	array<ref SRP_EnemyGroup> GetGroups()
	{
		return m_aAll;
	}

	//------------------------------------------------------------------------------------------------
	//! L'enregistrement d'un de nos groupes, ou null
	SRP_EnemyGroup FindRecord(SCR_AIGroup g)
	{
		if (!g)
			return null;
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (record && record.m_Group == g)
				return record;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	int GetMaxGroups()
	{
		return m_iMaxGroups;
	}

	//------------------------------------------------------------------------------------------------
	//! Plafond de groupes pour tout ce qui n'est pas une mission (garnisons, camions de territoire, jeeps de territoire,
	//! patrouilles ambiantes) : m_iMaxGroups moins les m_iMissionGroupReserve groupes gardés pour les missions
	int GetTerritoryGroupCap()
	{
		return Math.Max(m_iMaxGroups - Math.Max(m_iMissionGroupReserve, 0), 1);
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetEmptyGroupPrefab()
	{
		return m_sEmptyGroupPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetMoveWaypointPrefab()
	{
		return m_sMoveWaypointPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetGetOutWaypointPrefab()
	{
		return m_sGetOutWaypointPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetSearchDestroyWaypointPrefab()
	{
		return m_sSearchDestroyWaypointPrefab;
	}

	//------------------------------------------------------------------------------------------------
	ResourceName GetCycleWaypointPrefab()
	{
		return m_sCycleWaypointPrefab;
	}

	//------------------------------------------------------------------------------------------------
	int GetStreetPointsMin()
	{
		return m_iStreetPointsMin;
	}

	//------------------------------------------------------------------------------------------------
	int GetStreetPointsMax()
	{
		return m_iStreetPointsMax;
	}

	//------------------------------------------------------------------------------------------------
	float GetCRXHoldRadius()
	{
		return m_fCRXHoldRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Nos groupes à moins de « radius » de ce point, du plus proche au plus loin (distances parallèles)
	protected void GroupsNear(vector from, float radius, array<SRP_EnemyGroup> records, array<float> distances)
	{
		foreach (SRP_EnemyGroup record : m_aAll)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			float distance = vector.Distance(from, GroupPosition(record));
			if (distance > radius)
				continue;
			int index = 0;
			while (index < distances.Count() && distances[index] <= distance)
				index++;
			records.InsertAt(record, index);
			distances.InsertAt(distance, index);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : nombre de pages de l'état des groupes autour de ce point
	int GroupsReportPages(vector from, float radius)
	{
		array<SRP_EnemyGroup> records = {};
		array<float> distances = {};
		GroupsNear(from, radius, records, distances);
		return Math.Max(1, (records.Count() + REPORT_LINES - 1) / REPORT_LINES);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : l'état de nos groupes à moins de « radius » de ce point, une page de 12 lignes (en-tête : soldats,
	//! groupes et plafond, IA actives du jeu, lumière ambiante et facteur de nuit, joueurs éclairés et cachés)
	string GroupsReport(vector from, float radius, int page)
	{
		int groups = CountAlive();
		string text = string.Format("Soldats : %1 · groupes : %2 sur %3 au plus (%4 gardés pour les missions)", CountSoldiers(), groups, m_iMaxGroups, m_iMaxGroups - GetTerritoryGroupCap());
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
			text += string.Format(" · IA actives du jeu : %1 sur %2", aiWorld.GetCurrentNumOfActiveAIs(), aiWorld.GetLimitOfActiveAIs());
		ChimeraAIWorld chimeraWorld = ChimeraAIWorld.Cast(aiWorld);
		if (chimeraWorld)
			text += string.Format(" · évictions du jeu au dernier passage de la file : %1", chimeraWorld.GetLastTickEvictions());
		// Carte #76 : place des soldats ennemis au sol par classe, réserves, demandes en attente (Q7)
		text += "\n" + SRP_CmdCapacity.Get().GetStaffReport();
		text += "\n" + string.Format("Lumière ambiante %1 LV · facteur de nuit %2 · joueurs éclairés %3, cachés %4, sur %5", SRP_EnemySenses.Dec2(SRP_EnemySenses.s_fAmbientLV), SRP_EnemySenses.Dec2(SRP_EnemySenses.s_fNightGlobal), SRP_EnemySenses.CountLit(), SRP_EnemySenses.CountHidden(), SRP_EnemySenses.CountPlayers());
		text += "\n" + SRP_EnemyAwareness.Summary(this, System.GetTickCount(), m_aAlerts.Count());

		array<SRP_EnemyGroup> records = {};
		array<float> distances = {};
		GroupsNear(from, radius, records, distances);
		if (records.IsEmpty())
			return text + "\n" + string.Format("Aucun de nos groupes à moins de %1 m", Math.Round(radius));

		int pages = (records.Count() + REPORT_LINES - 1) / REPORT_LINES;
		int pageIndex = page;
		if (pageIndex >= pages)
			pageIndex = pages - 1;
		if (pageIndex < 0)
			pageIndex = 0;
		int first = pageIndex * REPORT_LINES;
		int last = Math.Min(first + REPORT_LINES, records.Count());
		for (int i = first; i < last; i++)
			text += "\n" + GroupLine(records[i], distances[i]);
		return text;
	}

	protected static const int REPORT_LINES = 12;

	//------------------------------------------------------------------------------------------------
	//! Une ligne de l'état des groupes : propriétaire, rôle, vivants/attendus, soldats réglés, état du chef, facteurs
	//! d'environnement du chef (calme / combat), dernier facteur de perception posé sur le chef, distance, carré,
	//! tâche (aide ou ratissage jusqu'à hh:mm, au poste,
	//! ronde, retour ; vigilance d'alerte)
	protected string GroupLine(SRP_EnemyGroup record, float distance)
	{
		int alive = AliveAgents(record);
		int managed = 0;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
			if (!chimera || !chimera.m_UtilityComponent || !chimera.m_UtilityComponent.m_CombatComponent)
				continue;
			if (chimera.m_UtilityComponent.m_CombatComponent.SRP_IsManaged())
				managed++;
		}

		string leader = "absent";
		string calm = "-";
		string fight = "-";
		string leaderFactor = "-";
		SCR_ChimeraAIAgent leaderAgent = SCR_ChimeraAIAgent.Cast(record.m_Group.GetLeaderAgent());
		if (leaderAgent && leaderAgent.m_UtilityComponent)
		{
			SCR_AIUtilityComponent utility = leaderAgent.m_UtilityComponent;
			if (utility.m_ThreatSystem)
				leader = SRP_EnemySenses.ThreatLabel(utility.m_ThreatSystem.GetThreatState());
			if (utility.m_CombatComponent)
			{
				calm = SRP_EnemySenses.Dec2(utility.m_CombatComponent.SRP_GetEnvCalm());
				fight = SRP_EnemySenses.Dec2(utility.m_CombatComponent.SRP_GetEnvCombat());
				leaderFactor = "CRX";
				if (utility.m_CombatComponent.SRP_GetLastFactor() >= 0)
					leaderFactor = SRP_EnemySenses.Dec2(utility.m_CombatComponent.SRP_GetLastFactor());
			}
		}

		string line = string.Format("%1 · %2 · %3/%4 · réglés %5/%6 · chef %7", record.m_sOwner, SRP_EnemySenses.RoleLabel(record.m_iRole), alive, record.m_iExpected, managed, agents.Count(), leader);
		line += string.Format(" · calme %1 / combat %2 · facteur posé %3 · %4 m · carré %5", calm, fight, leaderFactor, Math.Round(distance), SRP_FleetManagerComponent.GridOf(GroupPosition(record)));
		line += " · " + SRP_EnemyAwareness.TaskLabel(record, System.GetTickCount());
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : nombre de pages du journal de repérage
	int SightingsReportPages()
	{
		array<string> lines = {};
		SRP_EnemySenses.GetSightings(lines);
		return Math.Max(1, (lines.Count() + REPORT_LINES - 1) / REPORT_LINES);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : le journal de repérage (les 40 dernières lignes, la plus récente d'abord), une page de 12 lignes
	string SightingsReport(int page)
	{
		array<string> lines = {};
		SRP_EnemySenses.GetSightings(lines);
		if (lines.IsEmpty())
			return "Aucun repérage noté depuis le démarrage du serveur";
		int pages = (lines.Count() + REPORT_LINES - 1) / REPORT_LINES;
		int pageIndex = page;
		if (pageIndex >= pages)
			pageIndex = pages - 1;
		if (pageIndex < 0)
			pageIndex = 0;
		int first = pageIndex * REPORT_LINES;
		int last = Math.Min(first + REPORT_LINES, lines.Count());
		string text = "";
		for (int i = first; i < last; i++)
		{
			if (i > first)
				text += "\n";
			text += lines[i];
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		string text = string.Format("IA ennemie : %1 groupe(s) vivant(s) sur %2, dont %3 patrouille(s) ambiante(s), %4 jeep(s), %5 survivant(s) de mission, %6 alerte(s), %7 rendu(s)", CountAlive(), m_iMaxGroups, m_aAmbient.Count(), CountVehiclePatrols(), m_aRetiring.Count(), m_aAlerts.Count(), m_aSurrendered.Count());
		if (s_bTestMode)
			text += " — MODE ESSAI";
		foreach (SRP_EnemyZoneModifier modifier : m_aModifiers)
			text += string.Format("\nZone %1 : x%2 dans %3 m", modifier.m_sTag, modifier.m_fFactor, Math.Round(modifier.m_fRadius));
		return text;
	}
}
