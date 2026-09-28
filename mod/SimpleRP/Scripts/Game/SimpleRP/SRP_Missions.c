//------------------------------------------------------------------------------------------------
// SimpleRP — Missions
// SRP_MissionManagerComponent (sur le game mode) garde DEUX missions automatiques, tirées au sort
// selon des poids, sur des sites loin de la base et des autres missions : localités (zones
// civiles), points de livraison, marqueurs SRP_SiteMission posés à la main. Les autres missions
// viennent d'une demande (voir SRP_EMissionOrigin) : « Trouver une mission » au tableau (tout soldat
// formé choisit la distance ; 4 missions au plus au tableau, 15 min entre deux demandes), mission
// ORDONNÉE par un officier (type, lieu, chef ; en jeu ou depuis Discord), CAPTURE d'une ZONE ennemie au contact
// du front activée au tableau (G1 : aucun délai pour se préparer, engagée à 3 km du point clé, échec après 15 min de
// zone vide ; une capture à la fois, deux à partir de 10 joueurs connectés : G3 ; difficulté et prime selon la zone :
// Q9, G8 ; zone prise sans mission : alerte G2 ; zone forcée par le Staff : mission annulée, sans prime).
// Front (#75, SRP_FrontComponent) : les missions offensives tombent sur un carré rouge, celles de sécurité sur un
// carré bleu (G13) ; toute mission gardée reçoit sa garde, même dans une localité ennemie gardée (G11) ; le contrôle
// routier s'établit sur un carré bleu (G10, Q10).
// Commandeur (#76) : la mission OFFICIER vise l'officier de région à sa cachette du jour (OF10) ; « Saboter » passe
// d'abord par le Commandeur (dépôt ennemi, pièce d'appui : RE8) ; une mission réussie le prévient une fois
// (OnMissionSucceeded) ; gardes et vagues demandent leur place (SRP_CmdCapacity.Ask, classes GARDE et COMBAT).
// Types (voir TypeName / Briefing) :
//   CACHE       saboter une cache gardée, puis tenir le site jusqu'à l'exfiltration
//   CARGAISON   caisses abandonnées gardées, à ramener aux dépôts, renforts pendant le chargement
//   VEHICULE    véhicule abandonné gardé, à ramener à la base (rejoint la flotte)
//   CONVOI      convoi ennemi (2-3 véhicules, escorte) de A vers B : le stopper, ramener sa cargaison
//   POSTE_OBS   poste d'observation : tant qu'il vit, l'ambiance est doublée dans 2 km ; neutralisé,
//               le secteur est calme une heure
//   SABOTAGE    mortier / AA / radar posé à 300 m de sa garde : l'infiltration est possible
//   OFFICIER    l'officier de région à sa cachette du jour (Commandeur) ; sans Commandeur, un officier isolé et son
//               escorte ; il fuit à pied dès qu'on approche
//   MINES       un passage miné à déminer sous couverture, renforts pendant le travail
//   EPAVE       matériel sensible près d'une épave : le ramener avant l'équipe de récupération ennemie
//   MEDICAL     un civil blessé dans un village : le soigner (santé > 90 %)
//   CARBURANT   dépôt ennemi : des jerricans à ramener à la citerne, long et exposé
//   LARGAGE     caisses larguées dans une zone de 500 m, l'ennemi les cherche aussi
//   DOCUMENTS   une mallette dans un village gardé : la prendre discrètement et la ramener à la base
//   ECOUTE      rester 5 min près d'une antenne sans donner l'alerte : renseignement sur les garnisons
//   PANNE       événement : un véhicule du parc tombe en panne loin de la base, à réparer sous pression
//   CONTROLE    contrôle routier : jamais tiré au sort, c'est un soldat qui l'établit où il veut, sur une route, en
//               regardant un équipier (StartControl) ; tenir 20 min, contrôler les voitures civiles (SRP_Checkpoint.c)
// Chaque mission a un timer, une prise en charge possible (timer gelé), un rapport de fin, des
// récompenses : euros (trésorerie) et caisses livrées au marqueur de la base (SRP_Livraison).
// Effectif ennemi adapté aux joueurs connectés. Marqueur sur la carte, radio, tableau, /missions.
// Les missions actives ne survivent pas au redémarrage (leurs IA non plus) ; le compteur oui.
// Phase 4 — étape B
//------------------------------------------------------------------------------------------------

enum SRP_EMissionType
{
	CACHE,
	CARGAISON,
	VEHICULE,
	CONVOI,
	POSTE_OBS,
	SABOTAGE,
	OFFICIER,
	MINES,
	EPAVE,
	MEDICAL,
	CARBURANT,
	LARGAGE,
	DOCUMENTS,
	ECOUTE,
	PANNE,
	LIBERER,		// capture d'une zone ennemie au contact du front, activée au tableau (jamais tirée au sort)
	CONTROLE		// contrôle routier : tenir un point de route et contrôler les civils (SRP_Checkpoint.c)
}

//! D'où vient une mission : seul le générateur compte les AUTO ; le plafond du tableau compte AUTO + TROUVEE
enum SRP_EMissionOrigin
{
	GENERATEUR,	// tirée au sort par le générateur (deux en permanence)
	TROUVEE,		// « Trouver une mission » au tableau
	ORDONNEE,		// créée par un officier (type, lieu, chef)
	CAPTURE,		// capture d'une zone, activée au tableau
	SOLDAT,			// lancée sur place par un soldat (contrôle routier)
	EVENEMENT		// panne d'un véhicule du parc
}

enum SRP_EMissionState
{
	ACTIVE,
	SUCCES,
	ECHEC,
	EXPIREE,
	ANNULEE
}

//------------------------------------------------------------------------------------------------
//! Marqueur de site de mission posé à la main (facultatif)
[ComponentEditorProps(category: "SimpleRP", description: "Site de mission SimpleRP : un lieu où le générateur peut poser une mission")]
class SRP_MissionSiteComponentClass : ScriptComponentClass
{
}

class SRP_MissionSiteComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Nom affiché (vide = nom de l'entité)", category: "SimpleRP")]
	string m_sLabel;

	protected static ref array<SRP_MissionSiteComponent> s_aSites = {};

	static array<SRP_MissionSiteComponent> GetSites()
	{
		return s_aSites;
	}

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer() && !s_aSites.Contains(this))
			s_aSites.Insert(this);
	}

	override void OnDelete(IEntity owner)
	{
		s_aSites.RemoveItem(this);
		super.OnDelete(owner);
	}

	string GetLabel()
	{
		if (!m_sLabel.IsEmpty())
			return m_sLabel;
		string name = GetOwner().GetName();
		if (name.IsEmpty())
			return "site";
		return name;
	}
}

//------------------------------------------------------------------------------------------------
//! Une mission en cours
class SRP_Mission
{
	string m_sId;
	int m_iType;
	int m_iState = SRP_EMissionState.ACTIVE;
	string m_sTitle;
	string m_sSiteLabel;
	vector m_vSite;
	int m_iDifficulty;			// 1 à 3
	int m_iCreatedTick;
	int m_iDeadlineTick;
	int m_iRewardEuros;
	int m_iRewardResource;
	int m_iRewardPackets;

	int m_iOrigin = SRP_EMissionOrigin.GENERATEUR;
	string m_sFoundBy;			// « Trouver une mission » : qui l'a fait apparaître
	bool m_bEngaged;			// capture : un soldat est entré dans la zone, plus de retour arrière
	int m_iAbsentMs;			// capture engagée : temps sans aucun soldat dans la zone

	string m_sClaimedBy;		// acceptée par : nom du chef ; plus aucune limite de temps
	bool m_bTimerPaused;		// délai d'acceptation en pause (joueurs à proximité du site)
	int m_iClaimedById = -1;
	int m_iClaimTick;
	int m_iFrozenRemainingMs;

	int m_iTarget;				// caisses à livrer, mines à retirer
	int m_iProgress;

	// Seconde phase : vagues ennemies
	bool m_bHoldPhase;
	int m_iHoldMs;
	int m_iHoldTarget;
	int m_iNextWaveTick;
	int m_iWaves;
	int m_iMaxWaves;			// 0 = sans limite
	string m_sPhaseReason;

	// Spécifique
	IEntity m_Target;			// officier, civil blessé, véhicule en panne
	int m_iCarrierId = -1;		// documents : qui les porte
	bool m_bAlerted;			// documents, écoute : l'alerte a été donnée ; convoi : l'escorte a débarqué au contact
	bool m_bFled;				// officier : a fui
	// OF10 : mission OFFICIER du Commandeur (l'officier de région) ; -1 = officier isolé d'aujourd'hui (sans Commandeur)
	int m_iOfficerRegion = -1;	// index de la région visée (SRP_CmdRegionBook)
	string m_sOfficerRegion;	// code de la région visée, « R3 » (SRP_CmdRegionBook.IsTargeted -> IsOfficerTargeted)
	int m_iOfficerSince;		// heure Unix du lancement : seule une chute postérieure compte
	string m_sTargetPrefab;		// sabotage : prefab de la pièce posée (le Commandeur choisit le stock touché, RE8)
	string m_sDoneBy;			// qui a actionné l'objectif (sabotage, neutralisation, documents), pour le Commandeur
	bool m_bRoomLogged;			// refus de place (SRP_CmdCapacity) déjà écrit au journal pour cette mission
	bool m_bWaveRetry;			// vague refusée faute de place : nouvel essai avant que la demande expire (RE12), non sauvé
	int m_iGuardsAtStart;
	bool m_bGuardsPending;		// la garde n'est posée qu'à l'approche des joueurs, au prorata de leur nombre
	int m_iGuardBonus;			// groupes en plus du prorata (difficulté)
	int m_iGuardMax;
	vector m_vGuardCenter;
	bool m_bGuardDefend;
	vector m_vDestination;		// convoi
	string m_sDestinationLabel;
	int m_iSpecialTick;			// convoi : départ ; épave : équipe de récupération
	bool m_bDeparted;
	bool m_bStopped;
	vector m_vLastConvoyPos;
	int m_iStuckSince;
	int m_iLastMarkerTick;
	ref array<IEntity> m_aVehicles = {};
	ref SRP_Checkpoint m_Checkpoint;			// contrôle routier

	ref array<IEntity> m_aEntities = {};		// caisses, cache, cible, mines, mallette, épave
	ref array<ref SRP_EnemyGroup> m_aGroups = {};
	ref SCR_MapMarkerBase m_Marker;
	vector m_vMarkerPosition;
	string m_sMarkerText;
	ref array<int> m_aParticipants = {};		// joueurs vus sur le site, chef de mission, porteur : comptés en cas de succès

	//------------------------------------------------------------------------------------------------
	string TypeName()
	{
		return SRP_MissionManagerComponent.TypeName(m_iType);
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Générateur de missions SimpleRP")]
class SRP_MissionManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_MissionManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("2", UIWidgets.EditBox, "Missions AUTOMATIQUES gardées en permanence par le générateur (jamais plus)", category: "SimpleRP - Générateur")]
	protected int m_iMinActive;

	[Attribute("4", UIWidgets.EditBox, "Missions au plus au tableau : automatiques + trouvées à la demande", category: "SimpleRP - Générateur")]
	protected int m_iMaxActive;

	[Attribute("0", UIWidgets.EditBox, "Ancien réglage, sans effet : il n'y a plus de mission automatique en plus", category: "SimpleRP - Générateur")]
	protected float m_fExtraChance;

	[Attribute("15", UIWidgets.EditBox, "« Trouver une mission » : minutes entre deux demandes d'un même soldat", category: "SimpleRP - Générateur")]
	protected int m_iFindCooldownMinutes;

	[Attribute("3500", UIWidgets.EditBox, "« Trouver une mission » : distance à la base en dessous de laquelle une mission est « proche », en mètres", category: "SimpleRP - Générateur")]
	protected float m_fBandNear;

	[Attribute("6000", UIWidgets.EditBox, "« Trouver une mission » : au-delà, une mission est « lointaine » (entre les deux : « moyenne »), en mètres", category: "SimpleRP - Générateur")]
	protected float m_fBandMid;

	[Attribute("3000", UIWidgets.EditBox, "Capture : rayon de la zone de mission autour du point de mission de la zone (point clé, sinon centre de ses carrés), en mètres (y entrer engage la mission ; réglage front_capture_rayon_m)", category: "SimpleRP - Captures")]
	protected float m_fCaptureZoneRadius;

	[Attribute("15", UIWidgets.EditBox, "Capture engagée : minutes sans aucun soldat dans la zone avant l'échec (réglage front_capture_absence_minutes)", category: "SimpleRP - Captures")]
	protected int m_iCaptureAbsentMinutes;

	[Attribute("1", UIWidgets.CheckBox, "Capture : les soldats dans la zone de la base ne comptent ni pour engager la mission ni pour éviter l'échec", category: "SimpleRP - Captures")]
	protected bool m_bCaptureIgnoreBase;

	[Attribute("1", UIWidgets.EditBox, "Captures en cours à la fois sous le seuil de joueurs connectés (G3 ; réglage front_captures_max)", category: "SimpleRP - Captures")]
	protected int m_iCaptureMaxLow;

	[Attribute("2", UIWidgets.EditBox, "Captures en cours à la fois à partir du seuil de joueurs connectés (G3 ; réglage front_captures_max_monde)", category: "SimpleRP - Captures")]
	protected int m_iCaptureMaxHigh;

	[Attribute("10", UIWidgets.EditBox, "Joueurs connectés à partir desquels deux captures sont possibles (G3 ; réglage front_captures_seuil_joueurs)", category: "SimpleRP - Captures")]
	protected int m_iCaptureHighPlayers;

	[Attribute("300", UIWidgets.EditBox, "Prime d'une zone prise sous mission, hameau, en euros (G8, Q9 ; réglage front_prime_hameau)", category: "SimpleRP - Captures")]
	protected int m_iCapturePrimeHamlet;

	[Attribute("500", UIWidgets.EditBox, "Prime d'une zone prise sous mission, village, en euros (G8, Q9 ; réglage front_prime_village)", category: "SimpleRP - Captures")]
	protected int m_iCapturePrimeVillage;

	[Attribute("800", UIWidgets.EditBox, "Prime d'une zone prise sous mission, ville ou bourg, en euros (G8, Q9 ; réglage front_prime_ville)", category: "SimpleRP - Captures")]
	protected int m_iCapturePrimeTown;

	[Attribute("200", UIWidgets.EditBox, "Prime d'une zone sans localité prise sous mission, en euros (Q4, Q9 ; réglage front_prime_sans_village)", category: "SimpleRP - Captures")]
	protected int m_iCapturePrimeNoPlace;

	// ---- Sites selon le front (G13)
	[Attribute("cache,convoi,poste,sabotage,officier,carburant,documents,ecoute", UIWidgets.EditBox, "Missions offensives : posées sur un carré ROUGE (noms de TypeFromName, séparés par des virgules)", category: "SimpleRP - Sites")]
	protected string m_sOffensiveTypes;

	[Attribute("cargaison,vehicule,mines,medical,largage", UIWidgets.EditBox, "Missions de sécurité : posées sur un carré BLEU (les autres types tombent partout)", category: "SimpleRP - Sites")]
	protected string m_sSecurityTypes;

	[Attribute("1", UIWidgets.CheckBox, "Mission de sécurité sans lieu bleu (début de campagne) : repli sur une zone rouge au contact du front", category: "SimpleRP - Sites")]
	protected bool m_bSecurityFallbackFront;

	// ---- Gardes dans une localité ennemie gardée (G11)
	[Attribute("1", UIWidgets.CheckBox, "Une mission posée dans une localité ennemie gardée reçoit quand même sa garde", category: "SimpleRP - Vagues")]
	protected bool m_bGuardsInStronghold;

	[Attribute("200", UIWidgets.EditBox, "Localité ennemie gardée : marge autour de la localité, en mètres", category: "SimpleRP - Vagues")]
	protected float m_fGuardStrongholdMargin;

	// Types de mission offensifs (carré rouge) et de sécurité (carré bleu), lus dans les deux listes ci-dessus
	protected ref array<int> m_aOffensiveTypeList = {};
	protected ref array<int> m_aSecurityTypeList = {};
	static const int SIDE_ANY = -1;			// n'importe quel carré
	static const int SIDE_RED = 0;			// carré rouge
	static const int SIDE_BLUE = 1;			// carré bleu
	static const int SIDE_FRONT_RED = 2;	// carré rouge d'une zone au contact du front (repli des missions de sécurité)

	// Soldats comptés par groupe de garde ou de vague de mission (sections de 4 à 6) pour la place (SRP_CmdCapacity)
	static const int MISSION_GROUP_SOLDIERS = 5;

	protected ref map<string, int> m_mFindTicks = new map<string, int>();

	[Attribute("180", UIWidgets.EditBox, "Délai pour accepter une mission, en minutes : passé ce délai sans acceptation, elle disparaît. Une mission acceptée n'a plus de limite de temps", category: "SimpleRP - Générateur")]
	protected int m_iAcceptMinutes;

	[Attribute("2000", UIWidgets.EditBox, "Le délai d'acceptation est en pause tant qu'un joueur est à moins de … mètres du site", category: "SimpleRP - Générateur")]
	protected float m_fPauseRadius;

	[Attribute("90", UIWidgets.EditBox, "Ancien réglage, sans effet : la durée est remplacée par le délai d'acceptation", category: "SimpleRP - Générateur")]
	protected int m_iDurationMinutes;

	[Attribute("2000", UIWidgets.EditBox, "Distance minimale d'un site à la base, en mètres", category: "SimpleRP - Sites")]
	protected float m_fMinDistanceFromBase;

	[Attribute("9000", UIWidgets.EditBox, "Distance maximale d'un site à la base, en mètres", category: "SimpleRP - Sites")]
	protected float m_fMaxDistanceFromBase;

	[Attribute("1500", UIWidgets.EditBox, "Distance minimale entre deux missions actives, en mètres", category: "SimpleRP - Sites")]
	protected float m_fMinDistanceBetweenMissions;

	[Attribute("1000", UIWidgets.EditBox, "Aucun joueur à moins de … mètres du site au moment de la création", category: "SimpleRP - Sites")]
	protected float m_fMinDistanceFromPlayers;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur de la base", category: "SimpleRP - Sites")]
	protected string m_sBaseMarkerName;

	[Attribute("SRP_Livraison", UIWidgets.EditBox, "Marqueur où les caisses de récompense sont livrées (à la base)", category: "SimpleRP - Sites")]
	protected string m_sRewardMarkerName;

	[Attribute("200", UIWidgets.EditBox, "Rayon d'arrivée à la base (véhicule récupéré, documents ramenés), en mètres", category: "SimpleRP - Sites")]
	protected float m_fBaseArrivalRadius;

	// ---- Poids
	[Attribute("3", UIWidgets.EditBox, "Poids : Cache (0 = jamais)", category: "SimpleRP - Poids")]
	protected int m_iWeightCache;

	[Attribute("3", UIWidgets.EditBox, "Poids : Cargaison abandonnée", category: "SimpleRP - Poids")]
	protected int m_iWeightCargaison;

	[Attribute("2", UIWidgets.EditBox, "Poids : Véhicule abandonné", category: "SimpleRP - Poids")]
	protected int m_iWeightVehicule;

	[Attribute("2", UIWidgets.EditBox, "Poids : Convoi ennemi (navmesh véhicule requis)", category: "SimpleRP - Poids")]
	protected int m_iWeightConvoi;

	[Attribute("2", UIWidgets.EditBox, "Poids : Poste d'observation", category: "SimpleRP - Poids")]
	protected int m_iWeightPosteObs;

	[Attribute("2", UIWidgets.EditBox, "Poids : Mortier / AA / radar", category: "SimpleRP - Poids")]
	protected int m_iWeightSabotage;

	[Attribute("2", UIWidgets.EditBox, "Poids : Officier ennemi", category: "SimpleRP - Poids")]
	protected int m_iWeightOfficier;

	[Attribute("1", UIWidgets.EditBox, "Poids : Zone minée", category: "SimpleRP - Poids")]
	protected int m_iWeightMines;

	[Attribute("2", UIWidgets.EditBox, "Poids : Matériel abattu (épave)", category: "SimpleRP - Poids")]
	protected int m_iWeightEpave;

	[Attribute("2", UIWidgets.EditBox, "Poids : Colis médical", category: "SimpleRP - Poids")]
	protected int m_iWeightMedical;

	[Attribute("2", UIWidgets.EditBox, "Poids : Dépôt de carburant", category: "SimpleRP - Poids")]
	protected int m_iWeightCarburant;

	[Attribute("2", UIWidgets.EditBox, "Poids : Ravitaillement largué", category: "SimpleRP - Poids")]
	protected int m_iWeightLargage;

	[Attribute("2", UIWidgets.EditBox, "Poids : Documents", category: "SimpleRP - Poids")]
	protected int m_iWeightDocuments;

	[Attribute("0", UIWidgets.EditBox, "Ancien réglage, sans effet : une capture de zone n'est jamais tirée au sort, elle s'active au tableau", category: "SimpleRP - Poids")]
	protected int m_iWeightLiberer;

	[Attribute("1", UIWidgets.EditBox, "Poids : Écoute radio", category: "SimpleRP - Poids")]
	protected int m_iWeightEcoute;

	// ---- Contrôle routier
	[Attribute("20", UIWidgets.EditBox, "Minutes à tenir le point de contrôle", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlMinutes;

	[Attribute("2", UIWidgets.EditBox, "Soldats présents (à moins de 40 m) pour établir le point", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlMinPlayers;

	[Attribute("1000", UIWidgets.EditBox, "Distance minimale de la base, en mètres (Q10 ; réglage front_controle_distance_base_m)", category: "SimpleRP - Contrôle routier")]
	protected float m_fControlMinBaseDistance;

	[Attribute("1", UIWidgets.CheckBox, "Le soldat et le point de route doivent être sur un carré tenu par la PAG (bleu) (G10)", category: "SimpleRP - Contrôle routier")]
	protected bool m_bControlBlueOnly;

	[Attribute("1000", UIWidgets.EditBox, "Distance minimale du contrôle précédent, en mètres (pas deux fois au même endroit)", category: "SimpleRP - Contrôle routier")]
	protected float m_fControlMinLastDistance;

	[Attribute("40", UIWidgets.EditBox, "Le soldat doit être à moins de … mètres d'une route", category: "SimpleRP - Contrôle routier")]
	protected float m_fControlRoadReach;

	[Attribute("10", UIWidgets.EditBox, "Paquets de récompense d'un contrôle routier tenu jusqu'au bout", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlRewardPackets;

	protected vector m_vLastControlSite;

	[Attribute("20", UIWidgets.EditBox, "Part des conducteurs suspects, en % (20 = 1 sur 5)", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlSuspectPercent;

	[Attribute("20", UIWidgets.EditBox, "Durée d'une fouille de véhicule, en secondes", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlSearchSeconds;

	[Attribute("75", UIWidgets.EditBox, "Secondes entre deux voitures (pour 2 soldats ; 8 s de moins par soldat en plus, 30 s au minimum)", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlCarSeconds;

	[Attribute("25", UIWidgets.EditBox, "Chance qu'une patrouille ennemie vienne tâter le point, en % (25 = 1 mission sur 4)", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlThreatPercent;

	[Attribute("750", UIWidgets.EditBox, "Prime par saisie, en euros", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlSeizureBonus;

	[Attribute("400", UIWidgets.EditBox, "Prime par arrestation, en euros", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlArrestBonus;

	[Attribute("750", UIWidgets.EditBox, "Malus par suspect passé au travers, en euros", category: "SimpleRP - Contrôle routier")]
	protected int m_iControlMissedMalus;

	[Attribute("", UIWidgets.EditBox, "Camp que rejoint l'ennemi en civil quand il sort son arme (vide = celui de l'IA ennemie)", category: "SimpleRP - Contrôle routier")]
	protected string m_sControlEnemyFaction;

	[Attribute("{C0F7DD85A86B2900}Prefabs/Weapons/Handguns/PM/Handgun_PM.et", UIWidgets.ResourceNamePicker, "Arme de l'ennemi en civil", "et", category: "SimpleRP - Contrôle routier")]
	protected ResourceName m_sControlPistolPrefab;

	[Attribute("{8B853CDD11BA916E}Prefabs/Weapons/Magazines/Magazine_9x18_PM_8rnd_Ball.et", UIWidgets.ResourceNamePicker, "Chargeurs de l'ennemi en civil", "et", category: "SimpleRP - Contrôle routier")]
	protected ResourceName m_sControlMagazinePrefab;

	[Attribute("{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et", UIWidgets.ResourceNamePicker, "Explosion d'un véhicule chargé d'explosifs (obus lâché sur place ; vide = pas d'explosion)", "et", category: "SimpleRP - Contrôle routier")]
	protected ResourceName m_sControlBlastPrefab;

	// ---- Prefabs
	[Attribute("", UIWidgets.ResourceNamePicker, "Cache ennemie (prop + SRP_SabotageAction)", "et", category: "SimpleRP - Prefabs")]
	protected ResourceName m_sCachePrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Véhicules abandonnés (tirés au hasard)", "et", category: "SimpleRP - Prefabs")]
	protected ref array<ResourceName> m_aVehiclePrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Véhicules de convoi ennemi (UAZ, Ural… tirés au hasard)", "et", category: "SimpleRP - Prefabs")]
	protected ref array<ResourceName> m_aConvoyVehiclePrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Poste d'observation (prop antenne/radio + SRP_SabotageAction)", "et", category: "SimpleRP - Prefabs")]
	protected ResourceName m_sObservationPostPrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Cibles de sabotage : mortier, AA, radar (props + SRP_SabotageAction, tirés au hasard)", "et", category: "SimpleRP - Prefabs")]
	protected ref array<ResourceName> m_aSabotageTargetPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Officiers ennemis (prefabs de personnage, tirés au hasard)", "et", category: "SimpleRP - Prefabs")]
	protected ref array<ResourceName> m_aOfficerPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Mines (ACE Explosives ou autre, tirées au hasard)", "et", category: "SimpleRP - Prefabs")]
	protected ref array<ResourceName> m_aMinePrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Épave (prop d'hélicoptère ou de véhicule détruit)", "et", category: "SimpleRP - Prefabs")]
	protected ResourceName m_sWreckPrefab;

	[Attribute("10", UIWidgets.EditBox, "Paquets par niveau de difficulté dans une cargaison (cargaison, épave, largage, convoi ; carburant : x2)", category: "SimpleRP - Prefabs")]
	protected int m_iPacketsPerDifficulty;

	[Attribute("", UIWidgets.ResourceNamePicker, "Mallette de documents (prop + SRP_DocumentsAction)", "et", category: "SimpleRP - Prefabs")]
	protected ResourceName m_sBriefcasePrefab;

	[Attribute("", UIWidgets.ResourceNamePicker, "Antenne d'écoute (prop, sans action)", "et", category: "SimpleRP - Prefabs")]
	protected ResourceName m_sAntennaPrefab;

	// ---- Vagues et phases
	[Attribute("8", UIWidgets.EditBox, "Cache : minutes à tenir après le sabotage (0 = réussie au sabotage)", category: "SimpleRP - Vagues")]
	protected int m_iCacheHoldMinutes;

	[Attribute("4", UIWidgets.EditBox, "Minutes entre deux vagues ennemies", category: "SimpleRP - Vagues")]
	protected int m_iWaveMinutes;

	[Attribute("2", UIWidgets.EditBox, "Minutes de présence sur le site avant la première vague", category: "SimpleRP - Vagues")]
	protected int m_iFirstWaveMinutes;

	[Attribute("1", UIWidgets.EditBox, "Vagues maximum en plus de la difficulté", category: "SimpleRP - Vagues")]
	protected int m_iExtraWaves;

	[Attribute("150", UIWidgets.EditBox, "Rayon de présence sur le site, en mètres", category: "SimpleRP - Vagues")]
	protected float m_fPresenceRadius;

	[Attribute("1500", UIWidgets.EditBox, "La garde d'une mission est posée quand un joueur approche à … mètres, au prorata des joueurs en approche", category: "SimpleRP - Vagues")]
	protected float m_fGuardSpawnDistance;

	[Attribute("5", UIWidgets.EditBox, "Convoi : minutes entre l'annonce et le départ", category: "SimpleRP - Spécifique")]
	protected int m_iConvoyDepartMinutes;

	[Attribute("45", UIWidgets.EditBox, "Largage : durée de la mission, en minutes", category: "SimpleRP - Spécifique")]
	protected int m_iDropDurationMinutes;

	[Attribute("40", UIWidgets.EditBox, "Épave : durée de la mission, en minutes (l'équipe de récupération ennemie part à mi-temps)", category: "SimpleRP - Spécifique")]
	protected int m_iWreckDurationMinutes;

	[Attribute("5", UIWidgets.EditBox, "Écoute : minutes à rester près de l'antenne", category: "SimpleRP - Spécifique")]
	protected int m_iListenMinutes;

	[Attribute("60", UIWidgets.EditBox, "Poste d'observation : minutes de calme dans le secteur une fois neutralisé", category: "SimpleRP - Spécifique")]
	protected int m_iCalmMinutes;

	[Attribute("0.3", UIWidgets.EditBox, "Colis médical : santé du civil au départ (0 à 1)", category: "SimpleRP - Spécifique")]
	protected float m_fMedicalStartHealth;

	[Attribute("0.9", UIWidgets.EditBox, "Colis médical : santé à atteindre pour réussir (0 à 1). À baisser si ACE ne remonte pas la santé au-delà d'un certain point", category: "SimpleRP - Spécifique")]
	protected float m_fMedicalHealTarget;

	[Attribute("5", UIWidgets.EditBox, "Panne : chance par heure, par véhicule du parc occupé à plus de 3 km de la base, en %", category: "SimpleRP - Spécifique")]
	protected float m_fBreakdownChancePerHour;

	// ---- Prise en charge, carte, récompenses
	[Attribute("5", UIWidgets.ComboBox, "Prise en charge d'une mission : certif CDG, officier, Staff, ou à partir de ce grade (Sergent par défaut)", "", ParamEnumArray.FromEnum(SRP_EGrade), category: "SimpleRP - Prise en charge")]
	protected SRP_EGrade m_eClaimMinGrade;

	[Attribute("0", UIWidgets.EditBox, "Durée maximale d'une prise en charge, en minutes (0 = illimitée)", category: "SimpleRP - Prise en charge")]
	protected int m_iClaimMaxMinutes;

	protected int m_iLastTick;		// dernier passage du Tick, pour mettre les délais en pause

	[Attribute("1", UIWidgets.CheckBox, "Marqueurs de carte (SCR_MapMarkerManagerComponent requis sur le game mode)", category: "SimpleRP - Carte")]
	protected bool m_bMapMarkers;

	[Attribute("1", UIWidgets.CheckBox, "Une icône par type de mission (destroy, pick-up, heal, mine-field… voir /marqueurs) ; sinon l'icône unique ci-dessous", category: "SimpleRP - Carte")]
	protected bool m_bMarkerIconByType;

	[Attribute("27", UIWidgets.EditBox, "Icône unique du marqueur (index dans la config vanilla ; 27 = objective-marker)", category: "SimpleRP - Carte")]
	protected int m_iMarkerIcon;

	[Attribute("1", UIWidgets.EditBox, "Couleur d'une mission libre (index dans la config vanilla ; 1 = orange)", category: "SimpleRP - Carte")]
	protected int m_iMarkerColor;

	[Attribute("8", UIWidgets.EditBox, "Couleur d'une mission prise en charge (8 = bleu clair)", category: "SimpleRP - Carte")]
	protected int m_iMarkerColorClaimed;

	[Attribute("0", UIWidgets.CheckBox, "Compter automatiquement une mission réussie aux joueurs présents sur le site (sinon, un officier crédite après le débriefing)", category: "SimpleRP - Récompenses")]
	protected bool m_bAutoCountMissions;

	[Attribute("1500", UIWidgets.EditBox, "Récompense en euros pour une difficulté 1 (x difficulté)", category: "SimpleRP - Récompenses")]
	protected int m_iRewardEurosBase;

	[Attribute("20", UIWidgets.EditBox, "Paquets de récompense pour une difficulté 1 (x difficulté), livrés en caisse au marqueur de la base", category: "SimpleRP - Récompenses")]
	protected int m_iRewardPacketsBase;

	static const string MISSIONS_PATH = "$profile:SimpleRP/missions.json";

	protected static SRP_MissionManagerComponent s_Instance;

	protected ref array<ref SRP_Mission> m_aMissions = {};
	protected int m_iNextId = 1;
	protected bool m_bLoaded;			// une instance qui n'a pas chargé ne sauvegarde jamais
	protected int m_iSuccesses;
	protected int m_iFailures;
	protected bool m_bQuietEnd;		// Q9 : fin sans radio ni fil public (zone forcée par le Staff) — OnZoneCaptured, End

	//------------------------------------------------------------------------------------------------
	static SRP_MissionManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	static string TypeName(int type)
	{
		switch (type)
		{
			case SRP_EMissionType.CACHE: return "Cache ennemie";
			case SRP_EMissionType.CARGAISON: return "Cargaison abandonnée";
			case SRP_EMissionType.VEHICULE: return "Véhicule abandonné";
			case SRP_EMissionType.CONVOI: return "Convoi ennemi";
			case SRP_EMissionType.POSTE_OBS: return "Poste d'observation";
			case SRP_EMissionType.SABOTAGE: return "Sabotage";
			case SRP_EMissionType.OFFICIER: return "Officier ennemi";
			case SRP_EMissionType.MINES: return "Zone minée";
			case SRP_EMissionType.EPAVE: return "Matériel abattu";
			case SRP_EMissionType.MEDICAL: return "Colis médical";
			case SRP_EMissionType.CARBURANT: return "Dépôt de carburant";
			case SRP_EMissionType.LARGAGE: return "Ravitaillement largué";
			case SRP_EMissionType.DOCUMENTS: return "Documents";
			case SRP_EMissionType.ECOUTE: return "Écoute radio";
			case SRP_EMissionType.PANNE: return "Panne en route";
			case SRP_EMissionType.LIBERER: return "Capture de zone";
			case SRP_EMissionType.CONTROLE: return "Contrôle routier";
		}
		return "Mission";
	}

	//------------------------------------------------------------------------------------------------
	static int TypeFromName(string name)
	{
		string lower = name;
		lower.ToLower();
		if (lower == "cache") return SRP_EMissionType.CACHE;
		if (lower == "cargaison") return SRP_EMissionType.CARGAISON;
		if (lower == "vehicule" || lower == "véhicule") return SRP_EMissionType.VEHICULE;
		if (lower == "convoi") return SRP_EMissionType.CONVOI;
		if (lower == "poste") return SRP_EMissionType.POSTE_OBS;
		if (lower == "sabotage") return SRP_EMissionType.SABOTAGE;
		if (lower == "officier") return SRP_EMissionType.OFFICIER;
		if (lower == "mines") return SRP_EMissionType.MINES;
		if (lower == "epave" || lower == "épave") return SRP_EMissionType.EPAVE;
		if (lower == "medical" || lower == "médical") return SRP_EMissionType.MEDICAL;
		if (lower == "carburant") return SRP_EMissionType.CARBURANT;
		if (lower == "largage") return SRP_EMissionType.LARGAGE;
		if (lower == "documents") return SRP_EMissionType.DOCUMENTS;
		if (lower == "ecoute" || lower == "écoute") return SRP_EMissionType.ECOUTE;
		if (lower == "liberer" || lower == "libérer" || lower == "village" || lower == "zone" || lower == "capture") return SRP_EMissionType.LIBERER;
		if (lower == "controle" || lower == "contrôle" || lower == "checkpoint") return SRP_EMissionType.CONTROLE;
		return -1;
	}

	static const string TYPE_LIST = "cache, cargaison, vehicule, convoi, poste, sabotage, officier, mines, epave, medical, carburant, largage, documents, ecoute, controle";

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;

		// Posé ailleurs que sur le game mode (par erreur sur un prefab de cache, par exemple) : on ne fait rien,
		// sinon cette instance volerait la place du vrai gestionnaire et lancerait un second générateur
		if (!BaseGameMode.Cast(owner))
		{
			Print("[SRP] SRP_MissionManagerComponent posé sur une entité qui n'est pas le game mode : ignoré", LogLevel.WARNING);
			return;
		}

		s_Instance = this;
		SRP_Paths.EnsureDirectories();
		// G13 : types offensifs (carré rouge) et de sécurité (carré bleu)
		ParseTypes(m_sOffensiveTypes, m_aOffensiveTypeList);
		ParseTypes(m_sSecurityTypes, m_aSecurityTypeList);
		Load();
		m_bLoaded = true;
		GetGame().GetCallqueue().CallLater(Tick, 20000, true);
		GetGame().GetCallqueue().CallLater(TickControl, 2000, true);
		GetGame().GetCallqueue().CallLater(Generate, 30000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(TickControl);
			if (m_bLoaded)
				Save();
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Réglages du front tenus par les missions (front_reglages.txt, lecteur unique SRP_CmdSettings)
	//------------------------------------------------------------------------------------------------
	//! Déclare les clés front_* des missions (défaut = attribut = choix de Jack) — SRP_FrontComponent.DeclareSettings,
	//! au démarrage, AVANT SRP_CmdSettings.Reload
	void DeclareFrontSettings()
	{
		SRP_CmdSettings.DeclareInt("front_prime_hameau", m_iCapturePrimeHamlet, "Prime d'une zone prise sous mission, hameau, en euros (G8, Q9)");
		SRP_CmdSettings.DeclareInt("front_prime_village", m_iCapturePrimeVillage, "Prime d'une zone prise sous mission, village, en euros (G8, Q9)");
		SRP_CmdSettings.DeclareInt("front_prime_ville", m_iCapturePrimeTown, "Prime d'une zone prise sous mission, ville ou bourg, en euros (G8, Q9)");
		SRP_CmdSettings.DeclareInt("front_prime_sans_village", m_iCapturePrimeNoPlace, "Prime d'une zone sans localité prise sous mission, en euros (Q4, Q9)");
		SRP_CmdSettings.DeclareInt("front_captures_max", m_iCaptureMaxLow, "Captures en cours à la fois sous le seuil de joueurs connectés (G3)");
		SRP_CmdSettings.DeclareInt("front_captures_max_monde", m_iCaptureMaxHigh, "Captures en cours à la fois à partir du seuil de joueurs connectés (G3)");
		SRP_CmdSettings.DeclareInt("front_captures_seuil_joueurs", m_iCaptureHighPlayers, "Joueurs connectés à partir desquels la limite haute des captures s'applique (G3)");
		SRP_CmdSettings.DeclareFloat("front_capture_rayon_m", m_fCaptureZoneRadius, "Capture : un soldat à cette distance du point clé engage la mission et la tient, en mètres (G1)");
		SRP_CmdSettings.DeclareInt("front_capture_absence_minutes", m_iCaptureAbsentMinutes, "Capture engagée : minutes sans aucun soldat dans la zone avant l'échec (G1)");
		SRP_CmdSettings.DeclareFloat("front_controle_distance_base_m", m_fControlMinBaseDistance, "Contrôle routier : distance minimale de la base, en mètres (Q10)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit les clés front_* des missions dans les attributs (valeur lue, sinon le défaut déclaré) —
	//! SRP_FrontComponent.Start (après SRP_CmdSettings.Reload) et ReloadSettings (bouton Staff)
	void LoadFrontSettings()
	{
		if (!Replication.IsServer())
			return;
		// Sans déclaration préalable, une lecture rendrait 0 : on déclare d'abord (les clés iront au fichier au prochain Reload)
		if (!SRP_CmdSettings.IsDeclared("front_prime_hameau"))
			DeclareFrontSettings();

		m_iCapturePrimeHamlet = SRP_CmdSettings.GetIntClamped("front_prime_hameau", 0, 100000);
		m_iCapturePrimeVillage = SRP_CmdSettings.GetIntClamped("front_prime_village", 0, 100000);
		m_iCapturePrimeTown = SRP_CmdSettings.GetIntClamped("front_prime_ville", 0, 100000);
		m_iCapturePrimeNoPlace = SRP_CmdSettings.GetIntClamped("front_prime_sans_village", 0, 100000);
		m_iCaptureMaxLow = SRP_CmdSettings.GetIntClamped("front_captures_max", 1, 20);
		m_iCaptureMaxHigh = SRP_CmdSettings.GetIntClamped("front_captures_max_monde", 1, 20);
		m_iCaptureHighPlayers = SRP_CmdSettings.GetIntClamped("front_captures_seuil_joueurs", 1, 256);
		m_fCaptureZoneRadius = SRP_CmdSettings.GetFloatClamped("front_capture_rayon_m", 100, 20000);
		m_iCaptureAbsentMinutes = SRP_CmdSettings.GetIntClamped("front_capture_absence_minutes", 1, 240);
		m_fControlMinBaseDistance = SRP_CmdSettings.GetFloatClamped("front_controle_distance_base_m", 0, 20000);

		SRP_JournalComponent.Log("MISSION", string.Format("Réglages des missions relus : primes %1 / %2 / %3 € (sans localité %4 €), captures %5 à la fois, %6 à partir de %7 joueurs", m_iCapturePrimeHamlet, m_iCapturePrimeVillage, m_iCapturePrimeTown, m_iCapturePrimeNoPlace, m_iCaptureMaxLow, m_iCaptureMaxHigh, m_iCaptureHighPlayers));
	}

	//------------------------------------------------------------------------------------------------
	//! G13 : lit une liste « cache,convoi,… » (noms de TypeFromName) dans result ; un nom inconnu est écrit au journal
	protected void ParseTypes(string text, notnull array<int> result)
	{
		result.Clear();
		array<string> parts = {};
		text.Split(",", parts, true);
		foreach (string part : parts)
		{
			string wantedName = part.Trim();
			if (wantedName.IsEmpty())
				continue;
			int type = TypeFromName(wantedName);
			if (type < 0)
			{
				SRP_JournalComponent.Log("MISSION", "Type de mission inconnu dans les réglages des sites (G13) : " + wantedName);
				continue;
			}
			if (!result.Contains(type))
				result.Insert(type);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! G13 : côté du front où poser ce type : SIDE_RED (offensif), SIDE_BLUE (sécurité) ou SIDE_ANY (épave, autres)
	int SideForType(int type)
	{
		if (m_aOffensiveTypeList.Contains(type))
			return SIDE_RED;
		if (m_aSecurityTypeList.Contains(type))
			return SIDE_BLUE;
		return SIDE_ANY;
	}

	//------------------------------------------------------------------------------------------------
	// Consultation
	//------------------------------------------------------------------------------------------------
	void GetActiveSites(out array<vector> sites)
	{
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE)
				sites.Insert(mission.m_vSite);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les centres de garde des missions actives (hélicoptère de recherche : la garde est parfois posée loin du site).
	//! Un centre encore nul (garde pas encore prévue) est ignoré.
	void GetActiveGuardCenters(out array<vector> centers)
	{
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState != SRP_EMissionState.ACTIVE)
				continue;
			if (mission.m_vGuardCenter == vector.Zero)
				continue;
			centers.Insert(mission.m_vGuardCenter);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les missions actives (menu Staff)
	void GetActiveMissions(out array<SRP_Mission> result)
	{
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE)
				result.Insert(mission);
		}
	}

	//------------------------------------------------------------------------------------------------
	int CountActive()
	{
		int count = 0;
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasActiveOfType(int type)
	{
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE && mission.m_iType == type)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Missions actives de ce type (G3 : captures en cours) — CreateCapture, SRP_MissionRequests (en-tête des captures)
	int CountActiveOfType(int type)
	{
		int count = 0;
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE && mission.m_iType == type)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	SRP_Mission FindById(string id)
	{
		string wanted = id;
		wanted.ToUpper();
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_sId == wanted)
				return mission;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	string GetBoardText()
	{
		if (CountActive() == 0)
			return "Aucune mission en cours. Prochaine annonce à venir.";

		string text = "";
		int now = System.GetTickCount();
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState != SRP_EMissionState.ACTIVE)
				continue;
			if (!text.IsEmpty())
				text += "\n";
			text += string.Format("%1 — %2 — %3 — %4 — %5", mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel, ProgressText(mission), TimeText(mission, now));
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	string GetDetailText(SRP_Mission mission)
	{
		string text = string.Format("%1 — %2 (%3)\nSite : %4, grille %5 / %6", mission.m_sId, mission.m_sTitle, mission.TypeName(), mission.m_sSiteLabel, Math.Round(mission.m_vSite[0]), Math.Round(mission.m_vSite[2]));
		text += string.Format("\nDifficulté : %1 — %2", mission.m_iDifficulty, ProgressText(mission));
		text += string.Format("\nRécompense : %1 € + %2 paquets de %3\n%4", mission.m_iRewardEuros, mission.m_iRewardPackets, SRP_Resources.GetName(mission.m_iRewardResource), TimeText(mission, System.GetTickCount()));
		return text + "\n" + Briefing(mission);
	}

	//------------------------------------------------------------------------------------------------
	string TimeText(SRP_Mission mission, int now)
	{
		if (!mission.m_sClaimedBy.IsEmpty())
			return string.Format("acceptée par %1, sans limite de temps", mission.m_sClaimedBy);
		int left = (mission.m_iDeadlineTick - now) / 60000;
		if (left < 0)
			left = 0;
		if (mission.m_bTimerPaused)
			return string.Format("à accepter : reste %1 min, compte à rebours en pause (joueurs à proximité)", left);
		return string.Format("à accepter : reste %1 min, puis elle disparaît", left);
	}

	//------------------------------------------------------------------------------------------------
	protected string ProgressText(SRP_Mission mission)
	{
		switch (mission.m_iType)
		{
			case SRP_EMissionType.CACHE:
				if (mission.m_bHoldPhase)
					return string.Format("cache sabotée, tenir %1 / %2 min (%3 vague(s))", mission.m_iHoldMs / 60000, mission.m_iHoldTarget, mission.m_iWaves);
				return "cache à saboter";
			case SRP_EMissionType.CARGAISON: return string.Format("paquets rangés aux dépôts %1 / %2 (%3 vague(s))", mission.m_iProgress, mission.m_iTarget, mission.m_iWaves);
			case SRP_EMissionType.VEHICULE: return string.Format("véhicule à ramener à la base (%1 vague(s))", mission.m_iWaves);
			case SRP_EMissionType.CONVOI:
				if (!mission.m_bDeparted)
					return string.Format("départ de %1 vers %2 dans %3 min", mission.m_sSiteLabel, mission.m_sDestinationLabel, Math.Max(0, (mission.m_iSpecialTick - System.GetTickCount()) / 60000));
				if (mission.m_bStopped)
					return string.Format("convoi stoppé, paquets rangés %1 / %2", mission.m_iProgress, mission.m_iTarget);
				return "convoi en route vers " + mission.m_sDestinationLabel;
			case SRP_EMissionType.POSTE_OBS: return "poste à neutraliser (garde puis radio)";
			case SRP_EMissionType.SABOTAGE: return "cible à saboter, garde à 300 m";
			case SRP_EMissionType.OFFICIER:
				if (mission.m_iOfficerRegion >= 0)
					return string.Format("officier ennemi à éliminer ou à capturer, cachette du jour : %1", mission.m_sSiteLabel);
				if (mission.m_bFled)
					return "l'officier a fui à pied, à rattraper";
				return "officier à éliminer";
			case SRP_EMissionType.MINES: return string.Format("mines retirées %1 / %2 (%3 vague(s))", mission.m_iTarget - AliveEntities(mission), mission.m_iTarget, mission.m_iWaves);
			case SRP_EMissionType.EPAVE: return string.Format("paquets rangés %1 / %2", mission.m_iProgress, mission.m_iTarget);
			case SRP_EMissionType.MEDICAL: return "civil à soigner";
			case SRP_EMissionType.CARBURANT: return string.Format("paquets de carburant rangés %1 / %2 (%3 vague(s))", mission.m_iProgress, mission.m_iTarget, mission.m_iWaves);
			case SRP_EMissionType.LARGAGE: return string.Format("paquets rangés %1 / %2", mission.m_iProgress, mission.m_iTarget);
			case SRP_EMissionType.DOCUMENTS:
				if (mission.m_iCarrierId > 0)
					return "documents en main, à ramener à la base";
				return "mallette à récupérer";
			case SRP_EMissionType.ECOUTE: return string.Format("écoute %1 / %2 min", mission.m_iHoldMs / 60000, mission.m_iHoldTarget);
			case SRP_EMissionType.LIBERER:
				if (!mission.m_bEngaged)
					return string.Format("zone %1 à capturer : préparez-vous, aucun délai avant l'entrée dans la zone (%2)", mission.m_sSiteLabel, CaptureStateText(mission));
				if (mission.m_iAbsentMs > 0)
					return string.Format("capture de %1 ENGAGÉE, zone vide depuis %2 min (échec à %3 min) — %4", mission.m_sSiteLabel, mission.m_iAbsentMs / 60000, m_iCaptureAbsentMinutes, CaptureStateText(mission));
				return string.Format("capture de %1 ENGAGÉE : %2", mission.m_sSiteLabel, CaptureStateText(mission));
			case SRP_EMissionType.PANNE: return string.Format("véhicule en panne à réparer ou ramener (%1 vague(s))", mission.m_iWaves);
			case SRP_EMissionType.CONTROLE:
				if (mission.m_Checkpoint)
					return mission.m_Checkpoint.ProgressText(System.GetTickCount());
				return "point de contrôle";
		}
		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected string Briefing(SRP_Mission mission)
	{
		switch (mission.m_iType)
		{
			case SRP_EMissionType.CACHE:
				if (m_iCacheHoldMinutes > 0)
					return string.Format("Cache ennemie gardée. Neutralisez la garde, sabotez la cache (action au contact), puis tenez le site %1 minutes face à la contre-attaque, jusqu'à l'ordre d'exfiltration.", m_iCacheHoldMinutes);
				return "Cache ennemie gardée. Neutralisez la garde, puis sabotez la cache (action au contact).";
			case SRP_EMissionType.CARGAISON: return string.Format("Une caisse de %1 paquets abandonnée, sous garde. Transvasez les paquets dans un véhicule et rangez-les dans les dépôts de la base ; seuls les paquets rangés comptent. Renforts ennemis pendant le chargement.", mission.m_iTarget);
			case SRP_EMissionType.VEHICULE: return "Véhicule abandonné, sous garde. Ramenez-le à la base : il rejoindra le parc. Renforts ennemis pendant la récupération.";
			case SRP_EMissionType.CONVOI: return string.Format("Un convoi ennemi (%1 véhicules escortés) part de %2 vers %3. Stoppez-le avant l'arrivée : sa cargaison (%4 paquets) est déposée à côté du dernier véhicule, à ramener aux dépôts. S'il arrive, la mission est perdue.", mission.m_aVehicles.Count(), mission.m_sSiteLabel, mission.m_sDestinationLabel, mission.m_iTarget);
			case SRP_EMissionType.POSTE_OBS: return string.Format("Poste d'observation ennemi. Tant qu'il vit, les patrouilles sont deux fois plus fréquentes dans 2 km. Neutralisez la garde puis la radio (action au contact) : le secteur sera calme %1 minutes.", m_iCalmMinutes);
			case SRP_EMissionType.SABOTAGE: return "Une pièce ennemie (mortier, AA ou radar) est signalée, sa garde campe à 300 m. Sabotez la pièce (action au contact). Passer entre les mailles est possible.";
			case SRP_EMissionType.OFFICIER:
				if (mission.m_iOfficerRegion >= 0)
					return OfficerBriefing(mission);
				return "Un officier ennemi et son escorte sont sur le site. Il fuit à pied dès qu'on approche à 250 m : encerclez avant d'engager. Mission perdue s'il s'échappe à plus de 1,5 km.";
			case SRP_EMissionType.MINES: return string.Format("Un passage obligé a été miné (%1 mines). Le génie démine sous couverture ; les mines qui sautent comptent aussi, mais blessent. Renforts ennemis pendant le travail.", mission.m_iTarget);
			case SRP_EMissionType.EPAVE: return string.Format("Un de nos appareils est tombé. Une caisse de %1 paquets de matériel sensible est près de l'épave, sous garde. Ramenez-les aux dépôts avant l'équipe de récupération ennemie, qui part à mi-temps. Passé le délai, le matériel est perdu.", mission.m_iTarget);
			case SRP_EMissionType.MEDICAL: return string.Format("Un civil blessé attend dans le village. Soignez-le sur place (au-dessus de %1 %% de santé). S'il meurt, la mission est perdue.", Math.Round(m_fMedicalHealTarget * 100));
			case SRP_EMissionType.CARBURANT: return string.Format("Dépôt de carburant ennemi, sous garde : %1 paquets de carburant en caisses, à transvaser et à ranger à la citerne. Long et exposé : deux véhicules ne seront pas de trop. Renforts pendant le chargement.", mission.m_iTarget);
			case SRP_EMissionType.LARGAGE: return string.Format("Des caisses (%1 paquets en tout) ont été larguées dans une zone de 500 m autour du marqueur. L'ennemi les cherche aussi : patrouilles renforcées. Ramenez-les aux dépôts avant la fin du délai.", mission.m_iTarget);
			case SRP_EMissionType.DOCUMENTS: return "Une mallette de documents est dans un village gardé. Récupérez-la (action au contact) et ramenez-la à la base. Sans alerte (aucun garde tué avant la prise), la récompense est majorée ; sinon, des renforts arrivent.";
			case SRP_EMissionType.ECOUTE: return string.Format("Une antenne ennemie émet. Restez %1 minutes à moins de 50 m sans donner l'alerte : si un garde meurt, l'écoute est compromise. En récompense, le renseignement sur les garnisons ennemies.", m_iListenMinutes);
			case SRP_EMissionType.LIBERER: return string.Format("Capture de la zone %1, activée au tableau. Aucun délai pour se préparer : la mission n'est ENGAGÉE que lorsqu'un soldat arrive à moins de %2 km de son point clé, et alors plus de retour arrière. Si plus personne n'y reste pendant %3 minutes, la mission échoue et la menace ennemie monte. Prime de %4 € à la chute de la zone, en plus de la récompense de mission. %5", mission.m_sSiteLabel, Math.Round(m_fCaptureZoneRadius / 1000), m_iCaptureAbsentMinutes, CaptureMissionPrime(mission), CaptureRuleText(mission));
			case SRP_EMissionType.CONTROLE: return string.Format("Contrôle routier établi par %4. Tenez le point %2 minutes, à %1 soldats au moins au départ. Barrez la route avec vos véhicules. Les voitures civiles s'arrêtent : au contact du conducteur, action « Contrôler » (papiers, faire descendre, fouiller, laisser repartir, arrêter). Un conducteur sur cinq transporte des armes, des documents, des explosifs, ou est un ennemi en civil. Les papiers donnent des indices, la fouille (%3 s) tranche. Prime par saisie et par arrestation, malus par suspect passé au travers. Tirer sur un civil reste un crime de guerre, sauf sur celui qui force le barrage ou sort une arme.", m_iControlMinPlayers, m_iControlMinutes, m_iControlSearchSeconds, mission.m_sClaimedBy);
			case SRP_EMissionType.PANNE: return "Un véhicule du parc est tombé en panne loin de la base. Réparez-le sur place (génie, outils) ou ramenez-le. Renforts ennemis en approche.";
		}
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Zone du front visée par une capture (clé m_sDestinationLabel), -1 si le front ou la zone manque
	protected int CaptureZoneOf(SRP_Mission mission)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady() || mission.m_sDestinationLabel.IsEmpty())
			return -1;
		return front.FindZone(mission.m_sDestinationLabel);
	}

	//------------------------------------------------------------------------------------------------
	//! État de chute de la zone d'une capture : « points clés 1/2 saisis · 11 carrés bleus sur 25, 13 requis » (D1)
	protected string CaptureStateText(SRP_Mission mission)
	{
		int zone = CaptureZoneOf(mission);
		if (zone < 0)
			return "zone introuvable sur le front";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_ZoneFall fall = front.GetZoneFall();
		if (fall)
		{
			string text = fall.GetZoneFallText(zone);
			if (!text.IsEmpty())
				return text;
		}
		return string.Format("%1 carrés bleus sur %2", front.CountZoneBlue(zone), front.CountZoneCells(zone));
	}

	//------------------------------------------------------------------------------------------------
	//! Prime G8 que rapportera la chute de la zone d'une capture (0 si la zone manque)
	protected int CaptureMissionPrime(SRP_Mission mission)
	{
		int zone = CaptureZoneOf(mission);
		if (zone < 0)
			return 0;
		return CapturePrime(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Règle de chute D1 en clair pour le briefing d'une capture, avec l'état du moment
	protected string CaptureRuleText(SRP_Mission mission)
	{
		int zone = CaptureZoneOf(mission);
		if (zone < 0)
			return "La zone tombe quand ses points clés sont saisis et qu'assez de ses carrés sont à nous.";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int keys = front.GetKeyPointCount(zone);
		int squareMinutes = front.m_iSquareCaptureMinutes;
		string rule;
		if (keys <= 0)
			rule = string.Format("Zone sans localité : elle tombe quand assez de ses carrés sont à nous (%1 min de présence par carré).", squareMinutes);
		else if (keys == 1)
			rule = string.Format("La zone tombe quand son poste de commandement est saisi (action au contact du poste, au point clé) et qu'assez de ses carrés sont à nous (%1 min de présence par carré).", squareMinutes);
		else
			rule = string.Format("La zone tombe quand ses %1 postes de commandement sont saisis (action au contact du poste, à chaque point clé) et qu'assez de ses carrés sont à nous (%2 min de présence par carré).", keys, squareMinutes);
		// Choix de Jack du 27/09 : le centre d'une ville ou d'un village se prend d'un seul bloc
		if (keys > 0 && front.m_fBlockRadius > 0)
			rule += string.Format(" Le centre d'une ville ou d'un village se prend d'un seul bloc : %1 min de présence sans aucun ennemi dans le bloc.", front.m_iBlockMinutes);
		return rule + " Aujourd'hui : " + CaptureStateText(mission) + ".";
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : briefing de la mission OFFICIER du Commandeur (le nom de l'officier reste caché jusqu'à sa chute, VU2)
	protected string OfficerBriefing(SRP_Mission mission)
	{
		string region = "région ennemie";
		SRP_Commander commander = SRP_Commander.Get();
		if (commander)
		{
			string regionLabel = commander.GetRegionLabel(mission.m_iOfficerRegion);
			if (!regionLabel.IsEmpty())
				region = regionLabel;
		}
		int guards = CmdSettingInt("officier_gardes", 2);
		int flee = CmdSettingInt("officier_fuite_m", 250);
		int escapeMeters = CmdSettingInt("officier_echappe_m", 1500);
		int desorg = CmdSettingInt("desorg_min", 180);
		int factor = Math.MaxInt(CmdSettingInt("desorg_capture_facteur", 2), 1);
		string text = string.Format("L'officier ennemi de la %1 se cache aujourd'hui à %2 avec %3 garde(s). Il fuit à pied si on l'approche à %4 m : encerclez avant d'engager. Mission perdue s'il s'échappe à plus de %5 m de sa cachette.", region, mission.m_sSiteLabel, guards, flee, escapeMeters);
		text += string.Format(" Tué, sa région est désorganisée %1 ; capturé (inconscient puis menotté), %2.", DurationWords(desorg), DurationWords(desorg * factor));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Un chiffre réglé du Commandeur pour un texte (lecteur unique SRP_CmdSettings) ; fallback si la clé n'est pas déclarée
	protected int CmdSettingInt(string key, int fallback)
	{
		if (!SRP_CmdSettings.IsDeclared(key))
			return fallback;
		return SRP_CmdSettings.GetInt(key);
	}

	//------------------------------------------------------------------------------------------------
	//! « 3 h », « 1 h 30 » ou « 45 min »
	protected string DurationWords(int minutes)
	{
		if (minutes < 60)
			return string.Format("%1 min", minutes);
		int hours = minutes / 60;
		int rest = minutes - hours * 60;
		if (rest == 0)
			return string.Format("%1 h", hours);
		return string.Format("%1 h %2", hours, SRP_Time.Pad2(rest));
	}

	//------------------------------------------------------------------------------------------------
	// Génération
	//------------------------------------------------------------------------------------------------
	protected void Generate()
	{
		while (CountOrigin(SRP_EMissionOrigin.GENERATEUR) < m_iMinActive && CountBoard() < Math.Max(m_iMaxActive, m_iMinActive))
		{
			if (!Create(-1))
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Crée une mission (type -1 = tirage au sort). Retourne false si rien ne convient.
	bool Create(int forcedType)
	{
		int type = forcedType;
		if (type < 0)
			type = PickType();
		if (type < 0)
			return false;
		if (type == SRP_EMissionType.PANNE)
			return false;	// événement, pas de création à la demande
		if (type == SRP_EMissionType.CONTROLE)
			return false;	// établi sur place par un soldat (StartControl), jamais créé d'ici
		if (type == SRP_EMissionType.LIBERER)
			return false;	// une capture s'active au tableau, zone par zone (CreateCapture)

		vector site;
		string label;
		int officerRegion;
		if (!PickMissionSite(type, -1, -1, site, label, officerRegion))
		{
			if (type == SRP_EMissionType.OFFICIER && UsesRegionOfficer())
				SRP_JournalComponent.Log("MISSION", "Officier ennemi : aucun officier ennemi à chasser");
			else
				SRP_JournalComponent.Log("MISSION", string.Format("%1 : aucun site disponible (côté du front, distances base/joueurs/missions)", TypeName(type)));
			return false;	// le Tick réessaie 20 s plus tard, avec un autre type
		}

		SRP_Mission mission = NewMission(type, site, label);
		if (officerRegion >= 0)
			SetOfficerTarget(mission, officerRegion);
		if (!Setup(mission))
		{
			Cleanup(mission);
			m_iNextId--;
			return false;
		}

		Announce(mission);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_Mission NewMission(int type, vector site, string label)
	{
		SRP_Mission mission = new SRP_Mission();
		mission.m_sId = "M" + SRP_Time.Pad2(m_iNextId);
		m_iNextId++;
		mission.m_iType = type;
		mission.m_sTitle = TypeName(type);
		mission.m_sSiteLabel = label;
		mission.m_vSite = site;
		mission.m_iCreatedTick = System.GetTickCount();

		int minutes = m_iAcceptMinutes;
		if (minutes < 5)
			minutes = 5;
		mission.m_iDeadlineTick = mission.m_iCreatedTick + minutes * 60 * 1000;

		// Difficulté tirée au hasard (une capture la reçoit ensuite de sa zone : CreateAt, Q9)
		ApplyDifficulty(mission, Math.RandomIntInclusive(1, 3));
		mission.m_iRewardResource = Math.RandomInt(0, SRP_Resources.COUNT);
		if (type == SRP_EMissionType.CARBURANT)
			mission.m_iRewardResource = SRP_EResource.CARBURANT;
		else if (type == SRP_EMissionType.EPAVE || type == SRP_EMissionType.PANNE)
			mission.m_iRewardResource = SRP_EResource.PIECES;
		else if (type == SRP_EMissionType.MEDICAL)
			mission.m_iRewardResource = SRP_EResource.VIVRES;
		return mission;
	}

	//------------------------------------------------------------------------------------------------
	//! Difficulté (1 à 3) et récompense qui en découle (euros et paquets x difficulté) — NewMission, CreateAt (Q9)
	protected void ApplyDifficulty(SRP_Mission mission, int difficulty)
	{
		mission.m_iDifficulty = Math.ClampInt(difficulty, 1, 3);
		mission.m_iRewardEuros = m_iRewardEurosBase * mission.m_iDifficulty;
		mission.m_iRewardPackets = m_iRewardPacketsBase * mission.m_iDifficulty;
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : la mission vise l'officier de cette région (index et code, heure de lancement posée par Setup)
	protected void SetOfficerTarget(SRP_Mission mission, int region)
	{
		mission.m_iOfficerRegion = region;
		mission.m_sOfficerRegion = "";
		SRP_Commander commander = SRP_Commander.Get();
		if (commander && commander.GetBook())
			mission.m_sOfficerRegion = commander.GetBook().GetRegionCode(region);
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : le Commandeur est-il là ? Alors la mission OFFICIER vise l'officier de région (jamais d'officier isolé) ;
	//! sinon, la mission d'aujourd'hui (officier isolé posé par la mission)
	protected bool UsesRegionOfficer()
	{
		return SRP_Commander.Get() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : l'officier de région à viser près de near : région, cachette du jour et son nom de lieu (jamais le nom de
	//! l'officier, VU2) ; faux sans candidat (vivant, non visé par une mission active)
	protected bool PickOfficerHideout(vector near, out int region, out vector site, out string label)
	{
		region = -1;
		site = vector.Zero;
		label = "";
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return false;
		SRP_CmdRegionBook book = commander.GetBook();
		if (!book)
			return false;
		string officerLabel;
		if (!book.PickOfficerTarget(near, region, site, officerLabel))
			return false;
		vector hidePos;
		string hideLocality;
		if (book.GetOfficerHideout(region, hidePos, hideLocality) && !hideLocality.IsEmpty())
			label = hideLocality;
		else
			label = "cachette de l'officier";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : point autour duquel chercher l'officier à viser. PickOfficerTarget prend la cachette la plus proche : un point
	//! fixe viserait toujours le même officier. Avec une distance demandée (« Trouver une mission »), un point tiré dans
	//! l'anneau autour de la base ; sinon (générateur), le centre d'une zone civile tirée au sort.
	protected vector OfficerSearchPoint(float minBase, float maxBase)
	{
		if (maxBase > 0)
		{
			IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
			if (baseMarker)
			{
				float angle = Math.RandomFloat(0, Math.PI2);
				float distance = Math.RandomFloat(Math.Max(minBase, 0), maxBase);
				return baseMarker.GetOrigin() + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			}
		}
		array<SRP_CivilZoneComponent> places = SRP_CivilZoneComponent.GetZones();
		if (places && !places.IsEmpty())
		{
			SRP_CivilZoneComponent place = places.GetRandomElement();
			if (place)
				return place.GetCenter();
		}
		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Le lieu d'une mission de ce type (générateur, « Trouver une mission ») : OFFICIER avec le Commandeur -> cachette
	//! de l'officier de région (OF10, officerRegion >= 0) ; sinon un site du bon côté du front (G13), avec repli des
	//! missions de sécurité sur les zones rouges au contact quand il n'y a pas de lieu bleu. officerRegion = -1 hors OF10.
	protected bool PickMissionSite(int type, float minBase, float maxBase, out vector site, out string label, out int officerRegion)
	{
		officerRegion = -1;
		if (type == SRP_EMissionType.OFFICIER && UsesRegionOfficer())
		{
			int region;
			if (!PickOfficerHideout(OfficerSearchPoint(minBase, maxBase), region, site, label))
				return false;
			officerRegion = region;
			return true;
		}

		int side = SideForType(type);
		bool found = PickSite(site, label, vector.Zero, minBase, maxBase, side);
		if (!found && side == SIDE_BLUE && m_bSecurityFallbackFront)
			found = PickSite(site, label, vector.Zero, minBase, maxBase, SIDE_FRONT_RED);
		return found;
	}

	//------------------------------------------------------------------------------------------------
	protected void Announce(SRP_Mission mission)
	{
		m_aMissions.Insert(mission);
		Save();
		PlaceMarker(mission, mission.m_vSite, mission.m_sId + " " + mission.m_sTitle);

		string text = string.Format("Nouvelle mission %1 : %2 — %3 (difficulté %4)", mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel, mission.m_iDifficulty);
		SRP_JournalComponent.Log("MISSION", text);
		// Radio allégée : une ligne courte à tous pour une mission automatique ou un événement
		SRP_Utils.NotifyAll(string.Format("Radio : mission %1 au tableau — %2, %3", mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel));
	}

	//------------------------------------------------------------------------------------------------
	protected int PickType()
	{
		array<int> types = {};
		array<int> weights = {};
		AddWeighted(types, weights, SRP_EMissionType.CACHE, m_iWeightCache);
		AddWeighted(types, weights, SRP_EMissionType.CARGAISON, m_iWeightCargaison);
		AddWeighted(types, weights, SRP_EMissionType.VEHICULE, m_iWeightVehicule);
		AddWeighted(types, weights, SRP_EMissionType.CONVOI, m_iWeightConvoi);
		AddWeighted(types, weights, SRP_EMissionType.POSTE_OBS, m_iWeightPosteObs);
		AddWeighted(types, weights, SRP_EMissionType.SABOTAGE, m_iWeightSabotage);
		AddWeighted(types, weights, SRP_EMissionType.OFFICIER, m_iWeightOfficier);
		AddWeighted(types, weights, SRP_EMissionType.MINES, m_iWeightMines);
		AddWeighted(types, weights, SRP_EMissionType.EPAVE, m_iWeightEpave);
		AddWeighted(types, weights, SRP_EMissionType.MEDICAL, m_iWeightMedical);
		AddWeighted(types, weights, SRP_EMissionType.CARBURANT, m_iWeightCarburant);
		AddWeighted(types, weights, SRP_EMissionType.LARGAGE, m_iWeightLargage);
		AddWeighted(types, weights, SRP_EMissionType.DOCUMENTS, m_iWeightDocuments);
		AddWeighted(types, weights, SRP_EMissionType.ECOUTE, m_iWeightEcoute);

		int total = 0;
		foreach (int weight : weights)
			total += weight;
		if (total <= 0)
			return -1;

		int roll = Math.RandomInt(0, total);
		foreach (int index, int weight : weights)
		{
			roll -= weight;
			if (roll < 0)
				return types[index];
		}
		return types[types.Count() - 1];
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasPrefabs(array<ResourceName> list)
	{
		return list && !list.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : une mission OFFICIER est-elle possible ? Prefabs d'officier réglés, et, avec le Commandeur, un officier de
	//! région vivant, non visé, avec une cachette du jour (SRP_Commander.HasOfficerTarget)
	protected bool OfficerMissionPossible()
	{
		if (!HasPrefabs(m_aOfficerPrefabs))
			return false;
		if (UsesRegionOfficer())
			return SRP_Commander.HasOfficerTarget();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Les prefabs des officiers de mission (l'officier de région en prend un si officier_prefab est vide) —
	//! SRP_CmdRegionBook.PickOfficerPrefab
	array<ResourceName> GetOfficerPrefabs()
	{
		return m_aOfficerPrefabs;
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : une mission OFFICIER active vise-t-elle la région de ce code (« R3 ») ? — SRP_CmdRegionBook.IsTargeted
	//! (cachette gardée, officier écarté de PickOfficerTarget)
	bool IsOfficerTargeted(string regionCode)
	{
		if (regionCode.IsEmpty())
			return false;
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE && mission.m_iType == SRP_EMissionType.OFFICIER && mission.m_sOfficerRegion == regionCode)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddWeighted(array<int> types, array<int> weights, int type, int weight)
	{
		if (weight <= 0)
			return;

		// Sans IA ennemie, aucune mission
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !enemies.HasPrefabs())
			return;

		// Pas deux fois le même type en même temps
		if (HasActiveOfType(type))
			return;

		// Sans prefab, pas de mission de ce type
		switch (type)
		{
			case SRP_EMissionType.CACHE: if (m_sCachePrefab.IsEmpty()) return; break;
			case SRP_EMissionType.VEHICULE: if (!HasPrefabs(m_aVehiclePrefabs)) return; break;
			case SRP_EMissionType.CONVOI: if (!HasPrefabs(m_aConvoyVehiclePrefabs)) return; break;
			case SRP_EMissionType.POSTE_OBS: if (m_sObservationPostPrefab.IsEmpty()) return; break;
			case SRP_EMissionType.SABOTAGE: if (!HasPrefabs(m_aSabotageTargetPrefabs)) return; break;
			case SRP_EMissionType.OFFICIER: if (!OfficerMissionPossible()) return; break;
			case SRP_EMissionType.MINES: if (!HasPrefabs(m_aMinePrefabs)) return; break;
			case SRP_EMissionType.EPAVE: if (m_sWreckPrefab.IsEmpty()) return; break;
			case SRP_EMissionType.MEDICAL: if (!SRP_CivilianManagerComponent.GetInstance()) return; break;
			case SRP_EMissionType.DOCUMENTS: if (m_sBriefcasePrefab.IsEmpty()) return; break;
			case SRP_EMissionType.ECOUTE: if (m_sAntennaPrefab.IsEmpty()) return; break;
		}

		types.Insert(type);
		weights.Insert(weight);
	}

	//------------------------------------------------------------------------------------------------
	//! Un site : loin de la base, des joueurs, des missions actives. awayFrom : à plus de 3 km de ce point (destination de convoi), vector.Zero = ignoré.
	//! side (G13) : SIDE_RED, SIDE_BLUE, SIDE_FRONT_RED ou SIDE_ANY (-1) : côté du front où le site doit se trouver
	protected bool PickSite(out vector site, out string label, vector awayFrom, float minBase = -1, float maxBase = -1, int side = -1)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (minBase < 0)
			minBase = m_fMinDistanceFromBase;
		if (maxBase < 0)
			maxBase = m_fMaxDistanceFromBase;
		vector basePosition = vector.Zero;
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (baseMarker)
			basePosition = baseMarker.GetOrigin();

		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		array<vector> active = {};
		GetActiveSites(active);

		array<vector> positions = {};
		array<string> labels = {};

		foreach (SRP_MissionSiteComponent manual : SRP_MissionSiteComponent.GetSites())
		{
			positions.Insert(manual.GetOwner().GetOrigin());
			labels.Insert(manual.GetLabel());
		}
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
		{
			positions.Insert(zone.GetCenter());
			labels.Insert(zone.GetZoneName());
		}
		foreach (SRP_DeliveryPointComponent point : SRP_DeliveryPointComponent.GetPoints())
		{
			positions.Insert(point.GetOwner().GetOrigin());
			labels.Insert(point.GetLabel());
		}

		array<int> candidates = {};
		foreach (int index, vector position : positions)
		{
			// G13 : le bon côté du front d'abord (lecture de la grille, sans calcul de distance)
			if (!SiteMatchesSide(front, position, side))
				continue;
			if (baseMarker)
			{
				float toBase = vector.Distance(position, basePosition);
				if (toBase < minBase || toBase > maxBase)
					continue;
			}
			if (SRP_Utils.NearestPlayerDistance(position, players) < m_fMinDistanceFromPlayers)
				continue;
			if (awayFrom != vector.Zero && vector.Distance(position, awayFrom) < 3000)
				continue;
			bool tooClose = false;
			foreach (vector other : active)
			{
				if (vector.Distance(other, position) < m_fMinDistanceBetweenMissions)
				{
					tooClose = true;
					break;
				}
			}
			if (tooClose)
				continue;
			candidates.Insert(index);
		}

		if (candidates.IsEmpty())
			return false;

		int chosen = candidates.GetRandomElement();
		site = positions[chosen];
		label = labels[chosen];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! G13 : la position est-elle du côté voulu ? side < 0, front absent ou pas encore prêt : oui ; SIDE_BLUE : carré
	//! bleu ; SIDE_RED : carré rouge ; SIDE_FRONT_RED : carré rouge d'une zone au contact du front (E1)
	protected bool SiteMatchesSide(SRP_FrontComponent front, vector position, int side)
	{
		if (side < 0 || !front || !front.IsReady())
			return true;
		if (side == SIDE_BLUE)
			return front.IsBlueAt(position);
		if (!front.IsRedAt(position))
			return false;
		if (side == SIDE_RED)
			return true;
		int zone = front.GetZoneAt(position);
		return zone >= 0 && front.IsZoneInContact(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôle routier : un soldat l'établit là où il se tient (action « Établir un point de contrôle ici » en regardant
	//! un équipier, ou menu Staff avec alone = true). Tout soldat formé (pas une recrue) ; un point par groupe de joueurs
	//! (aucun autre contrôle actif à moins de 1 000 m) ; sur une route, loin de la base, sur un carré bleu (G10), pas au
	//! même endroit que le précédent. Le commandement est prévenu.
	string StartControl(int playerId, bool alone)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Contrôle routier indisponible";
		players.EnsureRegistered(playerId);

		vector site;
		int present;
		string refusal = CheckControl(playerId, alone, site, present);
		if (!refusal.IsEmpty())
			return refusal;
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";

		// Le nom du lieu : la localité la plus proche
		string place = "";
		float best = 1500;
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
		{
			float distance = vector.Distance(zone.GetCenter(), site);
			if (distance < best)
			{
				best = distance;
				place = zone.GetZoneName();
			}
		}
		string label = "route, carré " + SRP_FleetManagerComponent.GridOf(site);
		if (!place.IsEmpty())
			label = string.Format("route de %1, carré %2", place, SRP_FleetManagerComponent.GridOf(site));
		return OpenControl(playerId, record, site, label, present);
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les conditions d'un contrôle routier établi par ce joueur, là où il se tient : "" si c'est possible,
	//! sinon la raison. Sert à l'action (StartControl) et à l'affichage de l'action chez le joueur (SRP_Rights).
	string CheckControl(int playerId, bool alone, out vector site, out int present)
	{
		present = 0;
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (!players || !civils)
			return "Contrôle routier indisponible (ambiance civile absente)";
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return "Il faut être en jeu";

		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";
		bool trained = record.m_bStaff || record.m_iGrade > SRP_EGrade.RECRUE || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.FGI);
		if (!trained)
			return "Un contrôle routier se lance après la formation générale initiale";

		vector here = character.GetOrigin();
		float controlSpacing = Math.Max(m_fControlMinLastDistance, 1000.0);
		if (HasControlNear(here, controlSpacing))
			return string.Format("Un point de contrôle routier est déjà en place à moins de %1 m : établissez le vôtre plus loin", Math.Round(controlSpacing));


		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (baseMarker && vector.Distance(baseMarker.GetOrigin(), here) < m_fControlMinBaseDistance)
			return string.Format("Trop près de la base : un contrôle routier s'établit à plus de %1 m", Math.Round(m_fControlMinBaseDistance));
		if (m_vLastControlSite != vector.Zero && vector.Distance(m_vLastControlSite, here) < m_fControlMinLastDistance)
			return string.Format("Trop près du contrôle précédent : changez de route (à plus de %1 m)", Math.Round(m_fControlMinLastDistance));

		// G10 : seulement sur un carré tenu par la PAG. Test léger (lecture de la grille) placé avant la recherche de route,
		// que SRP_Rights.Compute refait toutes les 5 s pour chaque joueur
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		bool blueOnly = m_bControlBlueOnly && front && front.IsReady();
		if (blueOnly && !front.IsBlueAt(here))
			return "Territoire ennemi : un contrôle routier s'établit seulement sur un carré tenu par la PAG (bleu)";

		// Assez de soldats autour ? Compté avant de chercher la route (calcul plus léger, fait pour chaque joueur toutes les 5 s)
		int needed = Math.Max(m_iControlMinPlayers, 1);
		if (alone)
			needed = 1;
		float around = Math.Max(m_fControlRoadReach, 10) + 40;
		int nearby = 0;
		foreach (IEntity near : SRP_Utils.GetPlayerCharacters())
		{
			if (near && vector.Distance(near.GetOrigin(), here) <= around)
				nearby++;
		}
		if (nearby < needed)
			return string.Format("Il faut %1 soldats sur place pour tenir un point de contrôle", needed);

		if (!civils.FindRoadPoint(here, Math.Max(m_fControlRoadReach, 10), vector.Zero, site))
			return "Aucune route ici : placez-vous sur la route à contrôler";
		if (civils.IsInBase(site, 300))
			return "Trop près de la base";
		if (blueOnly && !front.IsBlueAt(site))
			return "La route la plus proche passe sur un carré ennemi : placez-vous sur un carré bleu";

		foreach (IEntity other : SRP_Utils.GetPlayerCharacters())
		{
			if (other && vector.Distance(other.GetOrigin(), site) <= 40)
				present++;
		}
		if (present < needed)
			return string.Format("Il faut %1 soldats sur place pour tenir un point de contrôle", needed);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les conditions sont-elles réunies, ici et maintenant ? (affichage de l'action, SRP_Rights)
	bool CanStartControl(int playerId)
	{
		vector site;
		int present;
		return CheckControl(playerId, false, site, present).IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Crée la mission de contrôle routier une fois les conditions vérifiées (StartControl)
	protected string OpenControl(int playerId, SRP_PlayerRecord record, vector site, string label, int present)
	{
		int now = System.GetTickCount();
		SRP_Mission mission = NewMission(SRP_EMissionType.CONTROLE, site, label);
		mission.m_iOrigin = SRP_EMissionOrigin.SOLDAT;
		mission.m_iDifficulty = 1;
		mission.m_iRewardEuros = m_iRewardEurosBase;
		mission.m_iRewardPackets = m_iControlRewardPackets;
		mission.m_sClaimedBy = record.m_sName;
		mission.m_iClaimedById = playerId;
		mission.m_iClaimTick = now;
		mission.m_aParticipants.Insert(playerId);
		mission.m_Checkpoint = new SRP_Checkpoint(mission.m_sId, site, ControlSettings());
		mission.m_Checkpoint.Establish(now);
		m_vLastControlSite = site;

		m_aMissions.Insert(mission);
		Save();
		PlaceMarker(mission, site, mission.m_sId + " " + mission.m_sTitle);

		string text = string.Format("%1 : point de contrôle routier établi par %2 — %3, %4 soldat(s) sur place, %5 minutes à tenir", mission.m_sId, record.m_sName, label, present, m_iControlMinutes);
		SRP_JournalComponent.Log("MISSION", text);
		SRP_Utils.NotifyAll("Radio : " + text);
		SRP_BridgeComponent.PushEvent("controle", text);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Un contrôle routier actif à moins de « radius » mètres ? (un point par groupe de joueurs, plusieurs en même temps)
	protected bool HasControlNear(vector position, float radius)
	{
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState != SRP_EMissionState.ACTIVE || mission.m_iType != SRP_EMissionType.CONTROLE)
				continue;
			if (vector.Distance(mission.m_vSite, position) < radius)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CheckpointSettings ControlSettings()
	{
		SRP_CheckpointSettings settings = new SRP_CheckpointSettings();
		settings.m_iMinutes = Math.Max(m_iControlMinutes, 5);
		settings.m_iMinPlayers = Math.Max(m_iControlMinPlayers, 1);
		settings.m_iSuspectPercent = m_iControlSuspectPercent;
		settings.m_iSearchSeconds = Math.Max(m_iControlSearchSeconds, 3);
		settings.m_iCarSeconds = Math.Max(m_iControlCarSeconds, 30);
		settings.m_iThreatPercent = m_iControlThreatPercent;
		settings.m_iSeizureBonus = m_iControlSeizureBonus;
		settings.m_iArrestBonus = m_iControlArrestBonus;
		settings.m_iMissedMalus = m_iControlMissedMalus;
		settings.m_sEnemyFactionKey = m_sControlEnemyFaction;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (settings.m_sEnemyFactionKey.IsEmpty() && enemies)
			settings.m_sEnemyFactionKey = enemies.GetFactionKey();
		settings.m_sPistolPrefab = m_sControlPistolPrefab;
		settings.m_sMagazinePrefab = m_sControlMagazinePrefab;
		settings.m_sBlastPrefab = m_sControlBlastPrefab;
		return settings;
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôle routier : le point vit à un rythme plus serré que les autres missions (toutes les 2 s)
	protected void TickControl()
	{
		int now = System.GetTickCount();
		array<IEntity> players;
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState != SRP_EMissionState.ACTIVE || !mission.m_Checkpoint)
				continue;
			if (!players)
				players = SRP_Utils.GetPlayerCharacters();

			int result = mission.m_Checkpoint.Update(players, now, 2000);
			// La menace prévue du point de contrôle, ou une vague refusée faute de place qui retente sa chance (RE12)
			bool retryWave = mission.m_bWaveRetry && now >= mission.m_iNextWaveTick;
			if (mission.m_Checkpoint.m_bWantWave || retryWave)
			{
				mission.m_Checkpoint.m_bWantWave = false;
				SendWave(mission, mission.m_vSite);
			}

			if (result == SRP_ECheckpointResult.ABANDONNE)
			{
				// Échec, mais les saisies et les arrestations faites avant l'abandon sont payées (pas la subvention)
				string abandoned = "point de contrôle abandonné — " + mission.m_Checkpoint.Report();
				int earned = mission.m_Checkpoint.EarnedEuros();
				SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
				if (treasury && earned > 0)
				{
					treasury.Add(earned, "primes du contrôle routier abandonné, mission " + mission.m_sId);
					abandoned += string.Format(" — primes versées malgré l'abandon : %1 €", earned);
				}
				End(mission, SRP_EMissionState.ECHEC, abandoned);
			}
			else if (result == SRP_ECheckpointResult.TENU)
			{
				SRP_Checkpoint checkpoint = mission.m_Checkpoint;
				mission.m_iRewardEuros = Math.Max(0, mission.m_iRewardEuros + checkpoint.BonusEuros());
				if (checkpoint.m_bIntel)
				{
					string intel = "Renseignement tiré des documents saisis : ";
					SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
					if (deliveries)
						intel += deliveries.GetStatusText();
					else
						intel += "rien de neuf";
					SRP_Utils.NotifyAll("Radio : " + intel);
				}
				End(mission, SRP_EMissionState.SUCCES, string.Format("point tenu %1 min — %2", m_iControlMinutes, checkpoint.Report()));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Origine des missions : le générateur n'en garde que deux ; les autres viennent d'une demande
	//------------------------------------------------------------------------------------------------
	protected int CountOrigin(int origin)
	{
		int count = 0;
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE && mission.m_iOrigin == origin)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Missions « du tableau » : les automatiques et celles trouvées à la demande (plafond m_iMaxActive)
	int CountBoard()
	{
		return CountOrigin(SRP_EMissionOrigin.GENERATEUR) + CountOrigin(SRP_EMissionOrigin.TROUVEE);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce type peut-il être créé (prefabs réglés) ? Jamais : panne (événement), contrôle routier (lancé sur place)
	protected bool TypeAvailable(int type)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !enemies.HasPrefabs())
			return false;
		switch (type)
		{
			case SRP_EMissionType.CACHE: return !m_sCachePrefab.IsEmpty();
			case SRP_EMissionType.VEHICULE: return HasPrefabs(m_aVehiclePrefabs);
			case SRP_EMissionType.CONVOI: return HasPrefabs(m_aConvoyVehiclePrefabs);
			case SRP_EMissionType.POSTE_OBS: return !m_sObservationPostPrefab.IsEmpty();
			case SRP_EMissionType.SABOTAGE: return HasPrefabs(m_aSabotageTargetPrefabs);
			case SRP_EMissionType.OFFICIER: return OfficerMissionPossible();
			case SRP_EMissionType.MINES: return HasPrefabs(m_aMinePrefabs);
			case SRP_EMissionType.EPAVE: return !m_sWreckPrefab.IsEmpty();
			case SRP_EMissionType.MEDICAL: return SRP_CivilianManagerComponent.GetInstance() != null;
			case SRP_EMissionType.DOCUMENTS: return !m_sBriefcasePrefab.IsEmpty();
			case SRP_EMissionType.ECOUTE: return !m_sAntennaPrefab.IsEmpty();
			case SRP_EMissionType.PANNE: return false;
			case SRP_EMissionType.CONTROLE: return false;
			case SRP_EMissionType.LIBERER: return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Les types qu'un officier peut ordonner (les captures se choisissent à part, par zone)
	void GetOrderTypes(array<int> types)
	{
		for (int type = 0; type <= SRP_EMissionType.CONTROLE; type++)
		{
			if (TypeAvailable(type))
				types.Insert(type);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Crée une mission de ce type à cet endroit, sans annonce : à l'appelant de prévenir qui il faut. zoneCode : code de
	//! la zone visée par une capture (« S07 ») ; difficulty > 0 : difficulté imposée (capture : celle de la zone, Q9) ;
	//! officerRegion >= 0 : mission OFFICIER sur l'officier de cette région (OF10)
	protected SRP_Mission CreateAt(int type, vector site, string label, int origin, string zoneCode = "", int difficulty = 0, int officerRegion = -1)
	{
		SRP_Mission mission = NewMission(type, site, label);
		mission.m_iOrigin = origin;
		mission.m_sDestinationLabel = zoneCode;
		if (difficulty > 0)
			ApplyDifficulty(mission, difficulty);	// avant Setup : la garde et les vagues suivent cette difficulté
		if (officerRegion >= 0)
			SetOfficerTarget(mission, officerRegion);
		if (!Setup(mission))
		{
			Cleanup(mission);
			m_iNextId--;
			return null;
		}
		m_aMissions.Insert(mission);
		Save();
		PlaceMarker(mission, mission.m_vSite, mission.m_sId + " " + mission.m_sTitle);
		return mission;
	}

	//------------------------------------------------------------------------------------------------
	//! « Trouver une mission » au tableau : tout soldat formé choisit la distance (0 proche, 1 moyenne, 2 lointaine), le
	//! reste est tiré au sort. La mission va au tableau : le commandement doit encore l'accorder.
	string FindForPlayer(int playerId, int band)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";
		bool trained = record.m_bStaff || record.m_iGrade > SRP_EGrade.RECRUE || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.FGI);
		if (!trained)
			return "Trouver une mission se fait après la formation générale initiale";

		int now = System.GetTickCount();
		int last;
		if (m_mFindTicks.Find(record.m_sIdentity, last))
		{
			int waitMs = m_iFindCooldownMinutes * 60000 - (now - last);
			if (waitMs > 0)
				return string.Format("Tu as déjà fait chercher une mission : encore %1 min avant la prochaine", 1 + waitMs / 60000);
		}
		foreach (SRP_Mission waiting : m_aMissions)
		{
			if (waiting.m_iState == SRP_EMissionState.ACTIVE && waiting.m_iOrigin == SRP_EMissionOrigin.TROUVEE && waiting.m_sClaimedBy.IsEmpty())
				return string.Format("%1 (%2, %3) a été trouvée et attend toujours : demande-la au commandement avant d'en chercher une autre", waiting.m_sId, waiting.m_sTitle, waiting.m_sSiteLabel);
		}
		if (CountBoard() >= m_iMaxActive)
			return string.Format("Il y a déjà %1 missions au tableau : prends-en une", CountBoard());

		float minBase = m_fMinDistanceFromBase;
		float maxBase = m_fBandNear;
		string bandName = "proche";
		if (band == 1)
		{
			minBase = m_fBandNear;
			maxBase = m_fBandMid;
			bandName = "moyenne";
		}
		else if (band == 2)
		{
			minBase = m_fBandMid;
			maxBase = m_fMaxDistanceFromBase;
			bandName = "lointaine";
		}

		IEntity bandMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		SRP_Mission mission;
		for (int attempt = 0; attempt < 8 && !mission; attempt++)
		{
			int type = PickType();
			if (type < 0)
				break;
			vector site;
			string label;
			int officerRegion;
			// G13 : le lieu dépend du type (rouge, bleu) ; un type sans lieu n'arrête pas la recherche, un autre est tiré
			if (!PickMissionSite(type, minBase, maxBase, site, label, officerRegion))
				continue;
			// OF10 : la cachette de l'officier n'a pas été choisie par distance : elle doit tomber dans la distance demandée
			if (officerRegion >= 0 && bandMarker)
			{
				float hideoutToBase = vector.Distance(site, bandMarker.GetOrigin());
				if (hideoutToBase < minBase || hideoutToBase > maxBase)
					continue;
			}
			mission = CreateAt(type, site, label, SRP_EMissionOrigin.TROUVEE, "", 0, officerRegion);
		}
		if (!mission)
			return string.Format("Aucune mission possible à distance %1 pour l'instant : essaie une autre distance", bandName);

		mission.m_sFoundBy = record.m_sName;
		m_mFindTicks.Set(record.m_sIdentity, now);

		string text = string.Format("%1 trouvée par %2 : %3 — %4 (difficulté %5), distance %6. À demander au commandement, au tableau des missions.", mission.m_sId, record.m_sName, mission.m_sTitle, mission.m_sSiteLabel, mission.m_iDifficulty, bandName);
		SRP_JournalComponent.Log("MISSION", text);
		players.NotifyOfficiers(text);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Mission ORDONNÉE par un officier : type et lieu choisis, puis un chef (chiefId, -1 = laissée au tableau).
	//! L'officier choisit librement : seule la base est interdite.
	string OrderAt(string author, int type, vector site, string label, int chiefId, out bool ok)
	{
		ok = false;
		if (!TypeAvailable(type))
			return "Ce type de mission ne peut pas être créé (prefabs manquants)";
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (baseMarker && vector.Distance(baseMarker.GetOrigin(), site) < 800)
			return "Trop près de la base : choisis un autre lieu";

		// OF10 : le lieu choisi ne sert qu'à désigner la région ; la mission est posée à la cachette du jour de l'officier
		vector missionSite = site;
		string missionLabel = label;
		int officerRegion = -1;
		if (type == SRP_EMissionType.OFFICIER && UsesRegionOfficer())
		{
			int region;
			vector hide;
			string hideLabel;
			if (!PickOfficerHideout(site, region, hide, hideLabel))
				return "Aucun officier ennemi à chasser dans cette partie de l'île";
			officerRegion = region;
			missionSite = hide;
			missionLabel = hideLabel;
		}

		SRP_Mission mission = CreateAt(type, missionSite, missionLabel, SRP_EMissionOrigin.ORDONNEE, "", 0, officerRegion);
		if (!mission)
			return string.Format("%1 ne peut pas se poser à %2 (pas de place, ou rien à y poser) : choisis un autre lieu", TypeName(type), missionLabel);

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		string text = string.Format("%1 ordonnée par %2 : %3 — %4 (difficulté %5)", mission.m_sId, author, mission.m_sTitle, mission.m_sSiteLabel, mission.m_iDifficulty);
		if (chiefId > 0)
		{
			Claim(chiefId, mission.m_sId, true);
			if (mission.m_iClaimedById == chiefId)
			{
				text += ", chef de mission : " + mission.m_sClaimedBy;
				SRP_Utils.HintPlayer(chiefId, "Ordre de mission", string.Format("%1 te confie %2 : %3 — %4, carré %5.\nTu es chef de mission. Détail au tableau des missions.", author, mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel, SRP_FleetManagerComponent.GridOf(mission.m_vSite)));
			}
		}
		else
			text += ", laissée au tableau";
		SRP_JournalComponent.Log("MISSION", text);
		if (players)
			players.NotifyOfficiers(text);
		ok = true;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Les localités où un officier peut ordonner une mission, dans un ordre stable (l'indice sert de clé au menu)
	void GetOrderPlaces(array<string> names, array<vector> centers)
	{
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
		{
			string name = zone.GetZoneName();
			if (name.IsEmpty())
				continue;
			names.Insert(name);
			centers.Insert(zone.GetCenter());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Depuis Discord : ordonner <n° de type | capture> <identité du chef | -> <lieu ou zone…> (zone : code « S07 »,
	//! nom « Régina », libellé « Régina (S07) » ou localité unique, SRP_FrontComponent.FindZone)
	string OrderFromBridge(string author, array<string> tokens, out bool ok)
	{
		ok = false;
		if (tokens.Count() < 4)
			return "usage : ordonner <type|capture> <identité du chef ou -> <lieu>";

		string place = "";
		for (int i = 3; i < tokens.Count(); i++)
		{
			if (!place.IsEmpty())
				place += " ";
			place += tokens[i];
		}

		int chiefId = -1;
		if (tokens[2] != "-")
		{
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			array<int> ids = {};
			array<SRP_PlayerRecord> records = {};
			if (players)
				players.GetConnectedRecords(ids, records);
			for (int p = 0; p < ids.Count(); p++)
			{
				if (records[p] && records[p].m_sIdentity == tokens[2])
					chiefId = ids[p];
			}
			if (chiefId < 0)
				return "le chef désigné n'est plus en ligne";
		}

		string kind = tokens[1];
		kind.ToLower();
		if (kind == "capture")
		{
			string refusal;
			SRP_Mission capture = CreateCapture(place, chiefId, author, refusal);
			if (!capture)
				return "capture impossible : " + refusal;
			ok = true;
			return string.Format("%1 : capture de %2 ordonnée, chef de mission %3", capture.m_sId, capture.m_sSiteLabel, capture.m_sClaimedBy);
		}

		if (!SRP_Utils.IsNumeric(kind))
			return "type de mission illisible : " + kind;
		string wanted = place;
		wanted.ToLower();
		array<string> names = {};
		array<vector> centers = {};
		GetOrderPlaces(names, centers);
		foreach (int index, string name : names)
		{
			string lower = name;
			lower.ToLower();
			if (lower == wanted)
				return OrderAt(author, kind.ToInt(), centers[index], name, chiefId, ok);
		}
		return "lieu inconnu : " + place;
	}

	//------------------------------------------------------------------------------------------------
	// Captures de zone (G1 à G3, J3, J4, Q9) : des missions comme les autres, activées au tableau
	//------------------------------------------------------------------------------------------------
	//! Code de zone (« S07 ») d'un texte : code, nom, libellé ou localité unique (SRP_FrontComponent.FindZone) ; le texte
	//! lui-même si le front ne le reconnaît pas
	protected string ZoneCodeOf(string zoneText)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return zoneText;
		int zone = front.FindZone(zoneText);
		if (zone < 0)
			return zoneText;
		return front.GetZoneCode(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! La capture de cette zone (code « S07 », ou nom reconnu par le front) est-elle une mission en cours ? (sinon : ni
	//! prime, ni crédit de mission) — SRP_MissionRequests, pont
	bool HasActiveCapture(string zoneCode)
	{
		return FindActiveLiberer(ZoneCodeOf(zoneCode)) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! La mission de capture active de cette zone (code « S07 », ou nom reconnu par le front), null sinon
	SRP_Mission FindCapture(string zoneCode)
	{
		return FindActiveLiberer(ZoneCodeOf(zoneCode));
	}

	//------------------------------------------------------------------------------------------------
	//! G3 : captures possibles à la fois : m_iCaptureMaxHigh à partir de m_iCaptureHighPlayers joueurs connectés, sinon
	//! m_iCaptureMaxLow (compté à la création : une capture en cours n'est jamais annulée si les joueurs partent)
	int CaptureLimit()
	{
		if (GetGame().GetPlayerManager().GetPlayerCount() >= m_iCaptureHighPlayers)
			return Math.MaxInt(m_iCaptureMaxHigh, 1);
		return Math.MaxInt(m_iCaptureMaxLow, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! G3 : limite haute des captures (en-tête « Captures en cours ») — SRP_MissionRequests
	int GetCaptureMaxHigh()
	{
		return Math.MaxInt(m_iCaptureMaxHigh, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! G3 : joueurs connectés à partir desquels la limite haute s'applique — SRP_MissionRequests
	int GetCaptureHighPlayers()
	{
		return m_iCaptureHighPlayers;
	}

	//------------------------------------------------------------------------------------------------
	//! J4 : zones ennemies au contact du front (E1), hors base, pas déjà visées par une capture active ; rien si le front
	//! est gelé (J3) ou pas prêt. SOURCE UNIQUE du tableau, de « Ordonner » et du pont ; rend le nombre
	int GetCaptureTargets(notnull array<int> zones)
	{
		zones.Clear();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady() || front.IsFrozen())
			return 0;
		array<int> contact = {};
		front.GetZonesInContact(contact);
		foreach (int zone : contact)
		{
			if (front.IsBaseZone(zone) || front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
				continue;
			if (FindActiveLiberer(front.GetZoneCode(zone)))
				continue;
			zones.Insert(zone);
		}
		return zones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Q9 : difficulté d'une capture fixée par la zone : sans localité ou hameau 1, village 2, ville (ou bourg) 3
	int CaptureDifficulty(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return 1;
		SRP_FrontZone data = front.GetZone(zone);
		if (!data)
			return 1;
		if (data.m_iKind == SRP_EZoneKind.VILLE)
			return 3;
		if (data.m_iKind == SRP_EZoneKind.VILLAGE)
			return 2;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! G8, Q4, Q9 : prime d'une zone prise sous mission, selon sa plus grosse localité : hameau, village, ville ; zone
	//! sans localité : m_iCapturePrimeNoPlace. Payée par OnZoneCaptured SEULEMENT (seul payeur)
	int CapturePrime(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return 0;
		SRP_FrontZone data = front.GetZone(zone);
		if (!data)
			return 0;
		int prime = m_iCapturePrimeNoPlace;
		if (data.m_iKind == SRP_EZoneKind.HAMEAU)
			prime = m_iCapturePrimeHamlet;
		else if (data.m_iKind == SRP_EZoneKind.VILLAGE)
			prime = m_iCapturePrimeVillage;
		else if (data.m_iKind == SRP_EZoneKind.VILLE)
			prime = m_iCapturePrimeTown;
		return Math.MaxInt(prime, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Active la capture d'une zone ennemie au nom d'un chef (accord du commandement, ou ordre d'un officier). zoneText :
	//! code « S07 », nom, libellé « Régina (S07) » ou localité unique. Refus expliqué dans refusal, dans l'ordre : front
	//! ou joueurs absents, pas de chef, zone inconnue, base, déjà à nous, front gelé (J3), pas au contact (J4), limite
	//! G3 (seulement pour une mission nouvelle). Site : point de mission de la zone (point clé CENTRE, G1) ; difficulté
	//! et récompense fixées par la zone (Q9) — SRP_MissionRequests (tableau, Ordonner, Discord), OrderFromBridge
	SRP_Mission CreateCapture(string zoneText, int chiefId, string author, out string refusal)
	{
		refusal = "";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!front || !front.IsReady() || !players)
		{
			refusal = "le front n'est pas prêt";
			return null;
		}
		if (chiefId <= 0)
		{
			refusal = "une capture a toujours un chef de mission";
			return null;
		}
		SRP_PlayerRecord chief = players.GetRecord(chiefId);
		if (!chief)
		{
			refusal = "le chef désigné n'est plus en ligne";
			return null;
		}
		int zone = front.FindZone(zoneText);
		if (zone < 0)
		{
			refusal = "zone inconnue : " + zoneText;
			return null;
		}
		string zoneLabel = front.GetZoneLabel(zone);
		if (front.IsBaseZone(zone))
		{
			refusal = zoneLabel + " est la zone de la base";
			return null;
		}
		if (front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
		{
			refusal = zoneLabel + " est déjà à nous";
			return null;
		}
		if (front.IsFrozen())
		{
			refusal = "le front est gelé par le Staff : aucune nouvelle capture";
			return null;
		}
		if (!front.IsZoneInContact(zone))
		{
			refusal = zoneLabel + " n'est pas au contact du front (aucun de ses carrés ne touche un carré à nous)";
			return null;
		}

		string zoneCode = front.GetZoneCode(zone);
		SRP_Mission mission = FindActiveLiberer(zoneCode);
		if (!mission)
		{
			int running = CountActiveOfType(SRP_EMissionType.LIBERER);
			if (running >= CaptureLimit())
			{
				refusal = string.Format("déjà %1 capture(s) en cours : %2 à la fois sous %3 joueurs connectés, %4 à partir de %3", running, Math.MaxInt(m_iCaptureMaxLow, 1), m_iCaptureHighPlayers, Math.MaxInt(m_iCaptureMaxHigh, 1));
				return null;
			}
			mission = CreateAt(SRP_EMissionType.LIBERER, front.GetMissionPoint(zone), zoneLabel, SRP_EMissionOrigin.CAPTURE, zoneCode, CaptureDifficulty(zone));
			if (!mission)
			{
				refusal = "mission impossible à poser (ennemi absent du jeu)";
				return null;
			}
		}

		mission.m_sClaimedBy = chief.m_sName;
		mission.m_iClaimedById = chiefId;
		mission.m_iClaimTick = System.GetTickCount();
		mission.m_bTimerPaused = false;
		if (!mission.m_aParticipants.Contains(chiefId))
			mission.m_aParticipants.Insert(chiefId);
		if (mission.m_Marker)
			PlaceMarker(mission, mission.m_vMarkerPosition, mission.m_sMarkerText);

		SRP_JournalComponent.Log("MISSION", string.Format("%1 : capture de %2 activée par %3, chef de mission %4 (difficulté %5, prime %6 €). Aucun délai avant l'entrée à %7 m du point clé.", mission.m_sId, zoneLabel, author, chief.m_sName, mission.m_iDifficulty, CapturePrime(zone), Math.Round(m_fCaptureZoneRadius)));
		return mission;
	}

	//------------------------------------------------------------------------------------------------
	//! G1 : un soldat compte-t-il pour la capture ? Vivant et hors délai de grâce (SRP_EnemyComponent.GetActivePlayers),
	//! à m_fCaptureZoneRadius au plus du point clé, hors de la zone de la base si m_bCaptureIgnoreBase
	protected bool AnyoneNearCapture(SRP_Mission mission)
	{
		array<IEntity> candidates;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
			candidates = enemies.GetActivePlayers();
		else
			candidates = SRP_Utils.GetPlayerCharacters();
		if (!candidates)
			return false;

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		bool ignoreBase = m_bCaptureIgnoreBase && front && front.IsReady();
		foreach (IEntity player : candidates)
		{
			if (!player || SRP_Utils.IsDead(player))
				continue;
			vector position = player.GetOrigin();
			if (vector.Distance(position, mission.m_vSite) > m_fCaptureZoneRadius)
				continue;
			if (ignoreBase && front.IsInBaseZone(position))
				continue;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Une capture n'a aucun délai tant qu'on se prépare. Dès qu'un soldat arrive à 3 km du point clé, elle est ENGAGÉE :
	//! plus de retour arrière. Zone vide 15 minutes = échec, et la menace monte (OnCaptureFailed). Front gelé (J3) : ni
	//! engagement ni compte d'absence. Zone tombée = succès (OnZoneCaptured).
	protected void TrackCapture(SRP_Mission mission, array<IEntity> players, int now)
	{
		if (mission.m_sClaimedBy.IsEmpty())
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsFrozen())
			return;
		bool inside = AnyoneNearCapture(mission);

		if (!mission.m_bEngaged)
		{
			if (!inside)
				return;
			mission.m_bEngaged = true;
			mission.m_iAbsentMs = 0;
			string engaged = string.Format("%1 — capture de %2 ENGAGÉE : plus de retour arrière. Échec si plus personne ne reste à moins de %3 km du point clé pendant %4 minutes.", mission.m_sId, mission.m_sSiteLabel, Math.Round(m_fCaptureZoneRadius / 1000), m_iCaptureAbsentMinutes);
			SRP_JournalComponent.Log("MISSION", engaged);
			SRP_Utils.NotifyAll("Radio : " + engaged);
			return;
		}

		if (inside)
		{
			mission.m_iAbsentMs = 0;
			return;
		}

		int elapsed = 20000;
		if (m_iLastTick > 0)
			elapsed = now - m_iLastTick;
		int before = mission.m_iAbsentMs;
		mission.m_iAbsentMs += elapsed;
		int limit = m_iCaptureAbsentMinutes * 60000;
		int warning = limit - 5 * 60000;
		if (warning > 0 && before < warning && mission.m_iAbsentMs >= warning)
			SRP_Utils.NotifyAll(string.Format("Radio : %1 — plus personne dans la zone de %2 : échec de la capture dans 5 minutes", mission.m_sId, mission.m_sSiteLabel));
		if (mission.m_iAbsentMs < limit)
			return;

		string zoneCode = mission.m_sDestinationLabel;
		End(mission, SRP_EMissionState.ECHEC, string.Format("capture abandonnée : zone vide plus de %1 minutes", m_iCaptureAbsentMinutes));
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
			territory.OnCaptureFailed(zoneCode);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAt(ResourceName prefab, vector position)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Mission : prefab invalide " + prefab, LogLevel.ERROR);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Position libre autour d'un point
	protected vector FreeSpot(vector center, float minDistance, float maxDistance, float clearance)
	{
		for (int attempt = 0; attempt < 8; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(minDistance, maxDistance);
			vector target = center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
			vector position;
			if (SCR_WorldTools.FindEmptyTerrainPosition(position, target, 15, clearance, 2))
				return position;
		}
		vector fallback = center;
		fallback[1] = GetGame().GetWorld().GetSurfaceY(fallback[0], fallback[2]);
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	//! Cargaison de mission : « packets » paquets d'une ressource, étiquetés mission, répartis en caisses de
	//! « perBox » autour d'un point (spread = 0 : les caisses en ligne au centre). Retourne le nombre de paquets posés.
	protected int SpawnCargo(SRP_Mission mission, int resource, int packets, int perBox, vector center, float spread)
	{
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources || packets <= 0)
			return 0;
		if (perBox < 1)
			perBox = 1;

		int placed = 0;
		int boxIndex = 0;
		while (placed < packets)
		{
			int inThisBox = Math.Min(perBox, packets - placed);
			vector position;
			if (spread > 0)
				position = FreeSpot(center, 5, spread, 1);
			else
				position = center + Vector(2.0 * boxIndex, 0, 0);
			position[1] = position[1] + 0.3;

			IEntity box = resources.SpawnBox(resource, inThisBox, position, mission.m_sId);
			if (!box)
				break;
			mission.m_aEntities.Insert(box);
			int inside = SRP_ResourceManagerComponent.CountInBox(box, resource);
			if (inside <= 0)
			{
				Print("[SRP] Mission " + mission.m_sId + " : la caisse de livraison n'accepte aucun paquet (capacité physique ?)", LogLevel.ERROR);
				break;
			}
			placed += inside;
			boxIndex++;
			if (boxIndex > 20)
				break;
		}
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	protected int CargoPackets(SRP_Mission mission)
	{
		return m_iPacketsPerDifficulty * mission.m_iDifficulty;
	}

	//------------------------------------------------------------------------------------------------
	//! Met en place les entités et la garde d'une mission
	protected bool Setup(SRP_Mission mission)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return false;

		// guards : 0 = pas de garde ; sinon 1 + les groupes en plus du prorata des joueurs en approche (la difficulté)
		int guards = mission.m_iDifficulty;
		int guardMax = 4;
		vector guardCenter = mission.m_vSite;
		bool defend = true;

		switch (mission.m_iType)
		{
			case SRP_EMissionType.CACHE:
			{
				IEntity cache = SpawnAt(m_sCachePrefab, mission.m_vSite);
				if (!cache)
					return false;
				mission.m_aEntities.Insert(cache);
				mission.m_iTarget = 1;
				break;
			}

			case SRP_EMissionType.CARGAISON:
			{
				mission.m_iTarget = SpawnCargo(mission, Math.RandomInt(0, SRP_Resources.COUNT), CargoPackets(mission), 40, mission.m_vSite, 0);
				if (mission.m_iTarget == 0)
					return false;
				break;
			}

			case SRP_EMissionType.VEHICULE:
			{
				vector position = FreeSpot(mission.m_vSite, 3, 25, 4);
				position[1] = position[1] + 0.3;
				IEntity vehicle = SpawnAt(m_aVehiclePrefabs.GetRandomElement(), position);
				if (!vehicle)
					return false;
				mission.m_aEntities.Insert(vehicle);
				mission.m_iTarget = 1;
				break;
			}

			case SRP_EMissionType.CONVOI:
			{
				vector destination;
				string destinationLabel;
				// G13 : le convoi va d'un lieu du même côté du front à un autre (rouge à rouge)
				if (!PickSite(destination, destinationLabel, mission.m_vSite, -1, -1, SideForType(SRP_EMissionType.CONVOI)))
					return false;
				mission.m_vDestination = destination;
				mission.m_sDestinationLabel = destinationLabel;
				mission.m_iSpecialTick = System.GetTickCount() + m_iConvoyDepartMinutes * 60 * 1000;
				mission.m_iTarget = CargoPackets(mission);	// paquets à bord
				guards = 0;		// l'escorte est dans les véhicules, posée au départ
				break;
			}

			case SRP_EMissionType.POSTE_OBS:
			{
				vector position = FreeSpot(mission.m_vSite, 0, 15, 1);
				IEntity post = SpawnAt(m_sObservationPostPrefab, position);
				if (!post)
					return false;
				mission.m_aEntities.Insert(post);
				mission.m_iTarget = 1;
				guards = 1;
				guardMax = 2;
				enemies.AddZoneModifier("poste-" + mission.m_sId, mission.m_vSite, 2000, 2, 0);
				break;
			}

			case SRP_EMissionType.SABOTAGE:
			{
				ResourceName targetPrefab = m_aSabotageTargetPrefabs.GetRandomElement();
				IEntity target = SpawnAt(targetPrefab, FreeSpot(mission.m_vSite, 0, 10, 2));
				if (!target)
					return false;
				mission.m_aEntities.Insert(target);
				mission.m_iTarget = 1;
				mission.m_sTargetPrefab = targetPrefab;	// RE8 : le Commandeur en déduit le stock touché (mortier, AA, radar…)
				// La garde campe à 300 m
				float angle = Math.RandomFloat(0, Math.PI2);
				guardCenter = mission.m_vSite + Vector(Math.Cos(angle) * 300, 0, Math.Sin(angle) * 300);
				guardCenter[1] = GetGame().GetWorld().GetSurfaceY(guardCenter[0], guardCenter[2]);
				break;
			}

			case SRP_EMissionType.OFFICIER:
			{
				// OF10 : l'officier de région est posé par le registre des régions à l'approche des joueurs ; la mission ne
				// pose que sa propre garde (G11), à l'approche elle aussi
				if (mission.m_iOfficerRegion >= 0)
				{
					mission.m_iTarget = 1;
					mission.m_iOfficerSince = System.GetUnixTime();
				}
				else
				{
					// Sans Commandeur : l'officier isolé d'aujourd'hui, qui demande sa place (classe GARDE)
					string officerRoom = SRP_CmdCapacity.Get().Ask("mission-" + mission.m_sId, SRP_ECmdCapClass.GARDE, 1, 1, mission.m_vSite, "mission");
					if (!officerRoom.IsEmpty())
					{
						SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : officier ennemi pas posé, %2", mission.m_sId, officerRoom));
						return false;
					}
					SRP_EnemyGroup officer = enemies.SpawnCharacterGroup(m_aOfficerPrefabs.GetRandomElement(), FreeSpot(mission.m_vSite, 0, 10, 1), "mission");
					if (!officer)
						return false;
					SRP_CmdCapacity.Tag(officer, SRP_ECmdCapClass.GARDE);
					array<IEntity> members = {};
					SRP_EnemyComponent.GetMembers(officer, members);
					if (members.IsEmpty())
						return false;
					mission.m_Target = members[0];
					mission.m_aGroups.Insert(officer);
					mission.m_iTarget = 1;
				}
				break;
			}

			case SRP_EMissionType.MINES:
			{
				int count = 3 + mission.m_iDifficulty * 2;
				for (int i = 0; i < count; i++)
				{
					IEntity mine = SpawnAt(m_aMinePrefabs.GetRandomElement(), FreeSpot(mission.m_vSite, 3, 40, 0.5));
					if (mine)
						mission.m_aEntities.Insert(mine);
				}
				mission.m_iTarget = mission.m_aEntities.Count();
				if (mission.m_iTarget == 0)
					return false;
				guards = 1;
				guardMax = 2;
				guardCenter = mission.m_vSite + Vector(120, 0, 0);
				guardCenter[1] = GetGame().GetWorld().GetSurfaceY(guardCenter[0], guardCenter[2]);
				break;
			}

			case SRP_EMissionType.EPAVE:
			{
				IEntity wreck = SpawnAt(m_sWreckPrefab, mission.m_vSite);
				if (wreck)
					mission.m_aEntities.Insert(wreck);
				mission.m_iTarget = SpawnCargo(mission, SRP_EResource.PIECES, CargoPackets(mission), 40, mission.m_vSite, 15);
				if (mission.m_iTarget == 0)
					return false;
				mission.m_iSpecialTick = mission.m_iCreatedTick + (mission.m_iDeadlineTick - mission.m_iCreatedTick) / 2;
				guards = 1;
				guardMax = 2;
				break;
			}

			case SRP_EMissionType.MEDICAL:
			{
				SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
				if (!civils)
					return false;
				IEntity civilian = SpawnAt(civils.PickCharacterPrefab(null), FreeSpot(mission.m_vSite, 0, 15, 0.6));
				if (!civilian)
					return false;
				SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(civilian);
				if (damage)
				{
					HitZone zone = damage.GetDefaultHitZone();
					if (zone)
						zone.SetHealthScaled(m_fMedicalStartHealth);
				}
				mission.m_Target = civilian;
				mission.m_aEntities.Insert(civilian);
				mission.m_iTarget = 1;
				guards = 0;
				break;
			}

			case SRP_EMissionType.CARBURANT:
			{
				mission.m_iTarget = SpawnCargo(mission, SRP_EResource.CARBURANT, CargoPackets(mission) * 2, 40, mission.m_vSite, 0);
				if (mission.m_iTarget == 0)
					return false;
				break;
			}

			case SRP_EMissionType.LARGAGE:
			{
				mission.m_iTarget = SpawnCargo(mission, Math.RandomInt(0, SRP_Resources.COUNT), CargoPackets(mission), 10, mission.m_vSite, 250);
				if (mission.m_iTarget == 0)
					return false;
				guards = 0;
				enemies.AddZoneModifier("largage-" + mission.m_sId, mission.m_vSite, 1500, 3, m_iDropDurationMinutes);
				break;
			}

			case SRP_EMissionType.DOCUMENTS:
			{
				IEntity briefcase = SpawnAt(m_sBriefcasePrefab, FreeSpot(mission.m_vSite, 0, 25, 0.5));
				if (!briefcase)
					return false;
				mission.m_aEntities.Insert(briefcase);
				mission.m_iTarget = 1;
				break;
			}

			case SRP_EMissionType.LIBERER:
			{
				mission.m_iTarget = 1;
				guards = 0;		// la zone et ses garnisons SONT l'objectif : rien en plus
				break;
			}

			case SRP_EMissionType.CONTROLE:
			{
				mission.m_Checkpoint = new SRP_Checkpoint(mission.m_sId, mission.m_vSite, ControlSettings());
				guards = 0;
				break;
			}

			case SRP_EMissionType.ECOUTE:
			{
				IEntity antenna = SpawnAt(m_sAntennaPrefab, FreeSpot(mission.m_vSite, 0, 10, 1));
				if (!antenna)
					return false;
				mission.m_aEntities.Insert(antenna);
				mission.m_iHoldTarget = m_iListenMinutes;
				mission.m_iTarget = 1;
				guards = 1;
				guardMax = 1;
				guardCenter = mission.m_vSite + Vector(0, 0, 150);
				guardCenter[1] = GetGame().GetWorld().GetSurfaceY(guardCenter[0], guardCenter[2]);
				break;
			}
		}

		mission.m_iMaxWaves = mission.m_iDifficulty + m_iExtraWaves;
		mission.m_iNextWaveTick = 0;

		// La garde n'est pas posée tout de suite : elle le sera à l'approche des joueurs (SpawnPendingGuards), à leur mesure
		if (guards > 0)
		{
			mission.m_bGuardsPending = true;
			mission.m_iGuardBonus = guards - 1;
			mission.m_iGuardMax = guardMax;
			mission.m_vGuardCenter = guardCenter;
			mission.m_bGuardDefend = defend;
		}
		mission.m_iGuardsAtStart = SRP_EnemyComponent.SoldiersIn(mission.m_aGroups, System.GetTickCount());	// attendus compris (file d'apparition de la 1.8)

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose la garde d'une mission quand les joueurs approchent : au prorata de ceux qui viennent, dans la limite du
	//! budget local. G11 : dans une localité ennemie gardée, la garde complète s'ajoute quand même à la garnison, sans le
	//! budget local (la place du Commandeur et la limite des 120 soldats restent). La place est demandée au
	//! Commandeur (SRP_CmdCapacity.Ask, classe GARDE) : refusée, la garde reste en attente et réessaie au passage suivant.
	protected void SpawnPendingGuards(SRP_Mission mission, array<IEntity> players)
	{
		if (SRP_Utils.NearestPlayerDistance(mission.m_vGuardCenter, players) > m_fGuardSpawnDistance)
			return;

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
		{
			mission.m_bGuardsPending = false;
			return;
		}
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		bool stronghold = frontEnemy && frontEnemy.IsInGarrisonedLocality(mission.m_vSite, m_fGuardStrongholdMargin);
		if (stronghold && !m_bGuardsInStronghold)
		{
			mission.m_bGuardsPending = false;
			SRP_EnemyComponent.Journal("MISSION", mission.m_sId + " : site dans une localité ennemie gardée, la garnison fait la garde (réglage)");
			return;
		}

		float countRadius = -1;		// le rayon du prorata (3 km)
		int wanted = enemies.ScaleGroupsNear(mission.m_vGuardCenter, mission.m_iGuardBonus, mission.m_iGuardMax, countRadius);
		int groups = Math.Min(wanted, enemies.LocalRoom(mission.m_vGuardCenter, countRadius));
		// G11 : garde complète, même dans une localité gardée (Q7 : limite portée à 120, pas de plafond de 2 groupes) ;
		// seules la place du Commandeur (Ask, classe GARDE) et la limite des 120 soldats la bornent
		if (stronghold)
			groups = wanted;
		if (groups <= 0)
		{
			mission.m_bGuardsPending = false;
			SRP_EnemyComponent.Journal("MISSION", mission.m_sId + " : pas de garde en plus, le lieu est déjà tenu par l'ennemi");
			return;
		}

		// Sabotage et écoute : un poste EN PLUS tient la pièce elle-même
		bool piecePost = mission.m_iType == SRP_EMissionType.SABOTAGE || mission.m_iType == SRP_EMissionType.ECOUTE;
		int askedGroups = groups;
		if (piecePost)
			askedGroups++;
		string refusal = SRP_CmdCapacity.Get().Ask("mission-" + mission.m_sId, SRP_ECmdCapClass.GARDE, askedGroups * MISSION_GROUP_SOLDIERS, askedGroups, mission.m_vGuardCenter, "mission");
		if (!refusal.IsEmpty())
		{
			// La demande est notée (file du Commandeur) ; nouvel essai au passage suivant, une seule ligne de journal
			if (!mission.m_bRoomLogged)
			{
				mission.m_bRoomLogged = true;
				SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : garde en attente de place, %2 (nouvel essai toutes les 20 s)", mission.m_sId, refusal));
			}
			return;
		}
		mission.m_bGuardsPending = false;

		// Rayon de maintien de la garde selon la mission (cache 30, poste d'observation 80, sabotage 150 ; sinon le réglage)
		float holdRadius = GuardHoldRadius(mission.m_iType);

		// Sabotage et écoute : la garde campe à l'écart, mais un poste EN PLUS tient la pièce elle-même (un groupe en Defend
		// sur la pièce, rayon 30 m), dans la limite du budget local (sauf dans une localité gardée, G11)
		int firstGuard = mission.m_aGroups.Count();
		int spawned = 0;
		if (piecePost)
		{
			spawned += enemies.SpawnGroups(mission.m_vSite, 30, 1, true, mission.m_vSite, "mission", mission.m_aGroups, 30);
			if (!stronghold)
				groups = Math.Min(groups, enemies.LocalRoom(mission.m_vGuardCenter, countRadius));
		}
		if (groups > 0)
			spawned += enemies.SpawnGroups(mission.m_vGuardCenter, 60, groups, mission.m_bGuardDefend, mission.m_vGuardCenter, "mission", mission.m_aGroups, holdRadius);
		TagGroups(mission.m_aGroups, firstGuard, SRP_ECmdCapClass.GARDE);
		SRP_EnemyGroup jeep = enemies.MaybeSpawnVehiclePatrol(mission.m_vGuardCenter, "mission");
		if (jeep)
			mission.m_aGroups.Insert(jeep);
		mission.m_iGuardsAtStart = SRP_EnemyComponent.SoldiersIn(mission.m_aGroups, System.GetTickCount());	// attendus compris (file d'apparition de la 1.8)
		if (stronghold)
			SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : localité ennemie gardée, %2 groupe(s) de garde en plus", mission.m_sId, spawned));
		else
			SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : garde de %2 groupe(s) pour %3 joueur(s) en approche (maintien %4 m)", mission.m_sId, spawned, enemies.CountPlayersNear(mission.m_vGuardCenter, countRadius), Math.Round(holdRadius)));
	}

	//------------------------------------------------------------------------------------------------
	//! Classe de place (SRP_ECmdCapClass) des groupes posés pour une mission, à partir de l'indice firstIndex — gardes (GARDE),
	//! vagues (COMBAT)
	protected void TagGroups(array<ref SRP_EnemyGroup> groups, int firstIndex, int cls)
	{
		for (int i = firstIndex; i < groups.Count(); i++)
		{
			SRP_CmdCapacity.Tag(groups[i], cls);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rayon de maintien de position de la garde d'une mission : cache 30 m, poste d'observation 80 m, sabotage 150 m ;
	//! -1 = le réglage par défaut de l'IA ennemie (m_fCRXHoldRadius, 100 m)
	protected float GuardHoldRadius(int type)
	{
		if (type == SRP_EMissionType.CACHE)
			return 30;
		if (type == SRP_EMissionType.POSTE_OBS)
			return 80;
		if (type == SRP_EMissionType.SABOTAGE)
			return 150;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Les joueurs sont-ils sur le site, ou repérés (alerte) près de lui ? Les vagues partent dans les deux cas.
	protected bool PlayersOnSiteOrSpotted(SRP_Mission mission, array<IEntity> players)
	{
		if (SRP_Utils.NearestPlayerDistance(mission.m_vSite, players) <= m_fPresenceRadius)
			return true;
		SRP_EnemyComponent watch = SRP_EnemyComponent.GetInstance();
		return watch && watch.HasAlertNear(mission.m_vSite, m_fPresenceRadius + 250);
	}

	//------------------------------------------------------------------------------------------------
	protected int AliveEntities(SRP_Mission mission)
	{
		int alive = 0;
		foreach (IEntity entity : mission.m_aEntities)
		{
			if (entity && !entity.IsDeleted())
				alive++;
		}
		return alive;
	}

	//------------------------------------------------------------------------------------------------
	// Suivi
	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		int now = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();

		for (int i = m_aMissions.Count() - 1; i >= 0; i--)
		{
			SRP_Mission mission = m_aMissions[i];
			if (mission.m_iState != SRP_EMissionState.ACTIVE)
			{
				m_aMissions.Remove(i);
				continue;
			}

			if (!mission.m_sClaimedBy.IsEmpty())
			{
				// Acceptée : plus aucune limite de temps, jusqu'à la réussite ou l'annulation
				mission.m_bTimerPaused = false;
			}
			else
			{
				// Pas encore acceptée : le délai court, sauf quand des joueurs sont dans les environs du site
				bool near = m_fPauseRadius > 0 && SRP_Utils.NearestPlayerDistance(mission.m_vSite, players) <= m_fPauseRadius;
				if (near && m_iLastTick > 0)
					mission.m_iDeadlineTick += now - m_iLastTick;
				mission.m_bTimerPaused = near;
			}
			if (mission.m_sClaimedBy.IsEmpty() && now >= mission.m_iDeadlineTick)
			{
				string reason = "non acceptée dans le délai";
				if (mission.m_iType == SRP_EMissionType.EPAVE)
					reason = "matériel tombé aux mains de l'ennemi";
				End(mission, SRP_EMissionState.EXPIREE, reason);
				continue;
			}

			RecordParticipants(mission, players);
			if (mission.m_bGuardsPending)
				SpawnPendingGuards(mission, players);

			int type = mission.m_iType;
			if (type == SRP_EMissionType.CACHE)
			{
				if (mission.m_bHoldPhase)
					TrackHold(mission, players, now);
			}
			else if (type == SRP_EMissionType.CARGAISON || type == SRP_EMissionType.CARBURANT)
			{
				TrackLoadingWaves(mission, players, now);
			}
			else if (type == SRP_EMissionType.MINES)
			{
				TrackLoadingWaves(mission, players, now);
				if (AliveEntities(mission) == 0)
					End(mission, SRP_EMissionState.SUCCES, string.Format("%1 mines retirées, %2 vague(s)", mission.m_iTarget, mission.m_iWaves));
			}
			else if (type == SRP_EMissionType.VEHICULE)
			{
				TrackVehicle(mission);
				if (mission.m_iState == SRP_EMissionState.ACTIVE)
					TrackLoadingWaves(mission, players, now);
			}
			else if (type == SRP_EMissionType.CONVOI)
				TrackConvoy(mission, players, now);
			else if (type == SRP_EMissionType.OFFICIER)
				TrackOfficer(mission, players, now);
			else if (type == SRP_EMissionType.EPAVE)
				TrackWreck(mission, now);
			else if (type == SRP_EMissionType.MEDICAL)
				TrackMedical(mission);
			else if (type == SRP_EMissionType.DOCUMENTS)
				TrackDocuments(mission, players, now);
			else if (type == SRP_EMissionType.ECOUTE)
				TrackListening(mission, players);
			else if (type == SRP_EMissionType.PANNE)
				TrackBreakdown(mission, players, now);
			else if (type == SRP_EMissionType.LIBERER)
				TrackCapture(mission, players, now);
		}

		CheckBreakdowns(players, now);
		m_iLastTick = now;

		// Deux missions automatiques, jamais plus : le reste se trouve ou s'ordonne au tableau
		if (CountOrigin(SRP_EMissionOrigin.GENERATEUR) < m_iMinActive)
			Generate();
	}

	//------------------------------------------------------------------------------------------------
	//! Les joueurs présents sur le site (300 m) comptent comme participants
	protected void RecordParticipants(SRP_Mission mission, array<IEntity> players)
	{
		foreach (IEntity player : players)
		{
			if (vector.Distance(player.GetOrigin(), mission.m_vSite) > 300)
				continue;
			int playerId = SRP_Utils.GetPlayerIdFromEntity(player);
			if (playerId > 0 && !mission.m_aParticipants.Contains(playerId))
				mission.m_aParticipants.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Cache sabotée : tenir le site jusqu'à l'exfiltration
	protected void TrackHold(SRP_Mission mission, array<IEntity> players, int now)
	{
		bool present = SRP_Utils.NearestPlayerDistance(mission.m_vSite, players) <= m_fPresenceRadius;
		if (!present)
		{
			// Le site n'est pas tenu, mais des joueurs repérés à côté attirent quand même une vague
			if (PlayersOnSiteOrSpotted(mission, players) && now >= mission.m_iNextWaveTick)
			{
				mission.m_iNextWaveTick = now + m_iWaveMinutes * 60 * 1000;
				SendWave(mission, mission.m_vSite);
			}
			return;
		}

		mission.m_iHoldMs += 20000;
		if (mission.m_iHoldMs >= mission.m_iHoldTarget * 60000)
		{
			End(mission, SRP_EMissionState.SUCCES, string.Format("%1, site tenu %2 min, %3 vague(s) repoussée(s), exfiltration", mission.m_sPhaseReason, mission.m_iHoldTarget, mission.m_iWaves));
			return;
		}

		if (now >= mission.m_iNextWaveTick)
		{
			mission.m_iNextWaveTick = now + m_iWaveMinutes * 60 * 1000;
			SendWave(mission, mission.m_vSite);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Des vagues tant que des joueurs sont sur le site, jusqu'au plafond
	protected void TrackLoadingWaves(SRP_Mission mission, array<IEntity> players, int now)
	{
		if (mission.m_iMaxWaves > 0 && mission.m_iWaves >= mission.m_iMaxWaves)
			return;
		if (!PlayersOnSiteOrSpotted(mission, players))
			return;

		if (mission.m_iNextWaveTick == 0)
		{
			mission.m_iNextWaveTick = now + m_iFirstWaveMinutes * 60 * 1000;
			return;
		}
		if (now >= mission.m_iNextWaveTick)
		{
			mission.m_iNextWaveTick = now + m_iWaveMinutes * 60 * 1000;
			SendWave(mission, mission.m_vSite);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackVehicle(SRP_Mission mission)
	{
		if (mission.m_aEntities.IsEmpty())
			return;
		IEntity vehicle = mission.m_aEntities[0];
		if (!vehicle || vehicle.IsDeleted() || SRP_GarageUtils.IsDestroyed(vehicle))
		{
			End(mission, SRP_EMissionState.ECHEC, "véhicule détruit");
			return;
		}
		if (!IsAtBase(vehicle.GetOrigin()))
			return;

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		string adopted = "";
		if (fleet)
			adopted = fleet.AdoptVehicle(vehicle, "Récupéré " + mission.m_sId);
		mission.m_aEntities.Clear();
		End(mission, SRP_EMissionState.SUCCES, "véhicule ramené à la base" + adopted);
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackConvoy(SRP_Mission mission, array<IEntity> players, int now)
	{
		if (!mission.m_bDeparted)
		{
			if (now < mission.m_iSpecialTick)
				return;
			// L'escorte demande sa place (classe GARDE) : refusée, le départ est remis d'une minute
			int escorts = ConvoySize(mission);
			string escortRoom = SRP_CmdCapacity.Get().Ask("mission-" + mission.m_sId, SRP_ECmdCapClass.GARDE, escorts * MISSION_GROUP_SOLDIERS, escorts, mission.m_vSite, "mission");
			if (!escortRoom.IsEmpty())
			{
				mission.m_iSpecialTick = now + 60000;
				if (!mission.m_bRoomLogged)
				{
					mission.m_bRoomLogged = true;
					SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : départ du convoi retenu, %2 (nouvel essai chaque minute)", mission.m_sId, escortRoom));
				}
				return;
			}
			if (!DepartConvoy(mission))
			{
				End(mission, SRP_EMissionState.ANNULEE, "convoi impossible à former");
				return;
			}
			return;
		}

		if (mission.m_bStopped)
			return;	// les caisses sont au sol, la fin arrive par le déchargement

		// Position du convoi : le véhicule de tête encore entier, sinon le dernier connu
		IEntity lead;
		int intact = 0;
		foreach (IEntity vehicle : mission.m_aVehicles)
		{
			if (!vehicle || vehicle.IsDeleted() || SRP_GarageUtils.IsDestroyed(vehicle))
				continue;
			intact++;
			if (!lead)
				lead = vehicle;
		}

		vector position = mission.m_vLastConvoyPos;
		if (lead)
			position = lead.GetOrigin();

		// Arrivé : perdu
		if (lead && vector.Distance(position, mission.m_vDestination) < 80)
		{
			End(mission, SRP_EMissionState.ECHEC, "le convoi est arrivé à " + mission.m_sDestinationLabel);
			return;
		}

		// Au contact (un véhicule endommagé, ou des joueurs repérés à moins de 150 m) : l'escorte débarque et attaque
		if (!mission.m_bAlerted)
		{
			vector contact = vector.Zero;
			bool damaged = false;
			foreach (IEntity convoyVehicle : mission.m_aVehicles)
			{
				if (!convoyVehicle || convoyVehicle.IsDeleted())
					continue;
				SCR_DamageManagerComponent vehicleDamage = SCR_DamageManagerComponent.GetDamageManager(convoyVehicle);
				if (vehicleDamage && vehicleDamage.GetHealthScaled() < 0.98)
					damaged = true;
			}
			SRP_EnemyComponent watch = SRP_EnemyComponent.GetInstance();
			bool spotted = watch && watch.NearestAlert(position, 150, contact);
			if (damaged || spotted)
			{
				mission.m_bAlerted = true;
				if (contact == vector.Zero)
				{
					contact = position;
					foreach (IEntity player : players)
					{
						if (player && vector.Distance(player.GetOrigin(), position) < 300)
							contact = player.GetOrigin();
					}
				}
				if (watch)
				{
					foreach (SRP_EnemyGroup escort : mission.m_aGroups)
						watch.OrderDismountAndAttack(escort, contact);
				}
				string reason = "joueurs repérés";
				if (damaged)
					reason = "véhicule touché";
				SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : convoi au contact (%2), l'escorte débarque et attaque", mission.m_sId, reason));
				SRP_Utils.NotifyAll(string.Format("Radio : %1 — l'escorte du convoi débarque", mission.m_sId));
			}
		}

		// Stoppé : escorte morte, véhicules détruits, ou immobile depuis 8 min
		bool stuck = false;
		if (vector.Distance(position, mission.m_vLastConvoyPos) < 3)
		{
			if (mission.m_iStuckSince == 0)
				mission.m_iStuckSince = now;
			else if (now - mission.m_iStuckSince > 8 * 60 * 1000)
				stuck = true;
		}
		else
		{
			mission.m_iStuckSince = 0;
			mission.m_vLastConvoyPos = position;
		}

		if (intact == 0 || SRP_EnemyComponent.IsWipedOut(mission.m_aGroups, now) || stuck)
		{
			mission.m_bStopped = true;
			vector drop = position + Vector(3, 0, 3);
			mission.m_iTarget = SpawnCargo(mission, Math.RandomInt(0, SRP_Resources.COUNT), mission.m_iTarget, 40, drop, 6);
			PlaceMarker(mission, drop, mission.m_sId + " Cargaison du convoi");
			string text = string.Format("%1 — convoi stoppé, la cargaison est au sol près du dernier véhicule (grille %2 / %3). À ramener aux dépôts.", mission.m_sId, Math.Round(drop[0]), Math.Round(drop[2]));
			SRP_JournalComponent.Log("MISSION", text);
			SRP_Utils.NotifyAll("Radio : " + text);
			return;
		}

		// Marqueur suivi toutes les 2 min (renseignement radio)
		if (now - mission.m_iLastMarkerTick > 2 * 60 * 1000)
		{
			mission.m_iLastMarkerTick = now;
			PlaceMarker(mission, position, mission.m_sId + " Convoi (dernière position)");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Véhicules (et groupes d'escorte) d'un convoi : 2, 3 en difficulté 3
	protected int ConvoySize(SRP_Mission mission)
	{
		if (mission.m_iDifficulty >= 3)
			return 3;
		return 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Le convoi se forme au départ : véhicules en file, un groupe d'escorte embarqué par véhicule, ordre de route
	protected bool DepartConvoy(SRP_Mission mission)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !HasPrefabs(m_aConvoyVehiclePrefabs))
			return false;

		int count = ConvoySize(mission);

		vector direction = mission.m_vDestination - mission.m_vSite;
		direction[1] = 0;
		float length = direction.Length();
		if (length < 1)
			direction = Vector(1, 0, 0);
		else
			direction = direction * (1 / length);

		for (int i = 0; i < count; i++)
		{
			vector position = mission.m_vSite - direction * (12 * i);
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.3;
			vector spot;
			if (SCR_WorldTools.FindEmptyTerrainPosition(spot, position, 15, 4, 2.5))
				position = spot + vector.Up * 0.3;

			IEntity vehicle = SpawnAt(m_aConvoyVehiclePrefabs.GetRandomElement(), position);
			if (!vehicle)
				continue;
			mission.m_aVehicles.Insert(vehicle);

			SRP_EnemyGroup escort = enemies.SpawnGroup(position + Vector(4, 0, 0), false, mission.m_vDestination, "mission");
			if (escort)
			{
				SRP_CmdCapacity.Tag(escort, SRP_ECmdCapClass.GARDE);
				mission.m_aGroups.Insert(escort);
				GetGame().GetCallqueue().CallLater(BoardConvoyVehicle, 3000, false, escort, vehicle);
			}
		}

		if (mission.m_aVehicles.IsEmpty())
			return false;

		mission.m_bDeparted = true;
		mission.m_vLastConvoyPos = mission.m_vSite;
		mission.m_iStuckSince = 0;
		mission.m_iLastMarkerTick = System.GetTickCount();
		string text = string.Format("%1 — le convoi ennemi quitte %2 vers %3 (%4 véhicules)", mission.m_sId, mission.m_sSiteLabel, mission.m_sDestinationLabel, mission.m_aVehicles.Count());
		SRP_JournalComponent.Log("MISSION", text);
		SRP_Utils.NotifyAll("Radio : " + text);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Embarque un groupe : le premier au volant, les autres en cargo
	protected void BoardConvoyVehicle(SRP_EnemyGroup escort, IEntity vehicle)
	{
		if (!vehicle || vehicle.IsDeleted())
			return;
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(escort, members);
		foreach (int index, IEntity member : members)
		{
			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(member.FindComponent(SCR_CompartmentAccessComponent));
			if (!access)
				continue;
			if (index == 0)
				access.MoveInVehicle(vehicle, ECompartmentType.PILOT);
			else
				access.MoveInVehicle(vehicle, ECompartmentType.CARGO);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackOfficer(SRP_Mission mission, array<IEntity> players, int now)
	{
		if (mission.m_iOfficerRegion >= 0)
		{
			TrackRegionOfficer(mission);
			return;
		}

		// Sans Commandeur : l'officier isolé posé par la mission
		IEntity officer = mission.m_Target;
		if (!officer || officer.IsDeleted() || SRP_Utils.IsDead(officer))
		{
			End(mission, SRP_EMissionState.SUCCES, "officier éliminé");
			return;
		}

		float toPlayers = SRP_Utils.NearestPlayerDistance(officer.GetOrigin(), players);

		if (!mission.m_bFled && toPlayers < 250 && !mission.m_aGroups.IsEmpty())
		{
			// Fuite à pied, à l'opposé du joueur le plus proche
			IEntity nearest = null;
			float best = 1000000;
			foreach (IEntity player : players)
			{
				float distance = vector.Distance(player.GetOrigin(), officer.GetOrigin());
				if (distance < best)
				{
					best = distance;
					nearest = player;
				}
			}
			vector away = officer.GetOrigin();
			if (nearest)
			{
				vector delta = officer.GetOrigin() - nearest.GetOrigin();
				delta[1] = 0;
				float length = delta.Length();
				if (length > 1)
					away = officer.GetOrigin() + delta * (600 / length);
			}
			away[1] = GetGame().GetWorld().GetSurfaceY(away[0], away[2]);
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			if (enemies)
			{
				// Il quitte son poste : maintien de position relâché (rôle de ronde), sinon CRX le ramènerait dans ses 30 m
				enemies.ApplyRole(mission.m_aGroups[0], SRP_ECRXRole.PATROUILLE, away, -1);
				enemies.OrderMove(mission.m_aGroups[0], away);
			}
			mission.m_bFled = true;
			SRP_Utils.NotifyAll(string.Format("Radio : %1 — l'officier prend la fuite à pied", mission.m_sId));
		}

		if (vector.Distance(officer.GetOrigin(), mission.m_vSite) > 1500)
			End(mission, SRP_EMissionState.ECHEC, "l'officier s'est échappé");
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : suivi de l'officier de région par le registre du Commandeur (pose, fuite, évasion et chute sont à lui) :
	//! tombé ou capturé après le lancement = réussite ; échappé = échec ; région libérée ou plus d'officier = annulée
	protected void TrackRegionOfficer(SRP_Mission mission)
	{
		SRP_Commander commander = SRP_Commander.Get();
		SRP_CmdRegionBook book;
		if (commander)
			book = commander.GetBook();
		if (!book)
		{
			End(mission, SRP_EMissionState.ANNULEE, "plus d'officier ennemi à chasser");
			return;
		}

		int status = book.GetOfficerMissionStatus(mission.m_iOfficerRegion, mission.m_iOfficerSince);
		if (status == SRP_ECmdMissionStatus.TOMBE)
		{
			End(mission, SRP_EMissionState.SUCCES, RegionOfficerText(book, mission.m_iOfficerRegion) + " éliminé");
			return;
		}
		if (status == SRP_ECmdMissionStatus.CAPTURE)
		{
			End(mission, SRP_EMissionState.SUCCES, RegionOfficerText(book, mission.m_iOfficerRegion) + " capturé");
			return;
		}
		if (status == SRP_ECmdMissionStatus.ECHAPPE)
		{
			End(mission, SRP_EMissionState.ECHEC, "l'officier ennemi s'est échappé");
			return;
		}
		if (status == SRP_ECmdMissionStatus.SANS_OBJET)
		{
			End(mission, SRP_EMissionState.ANNULEE, "région libérée : plus d'officier ennemi à chasser");
			return;
		}
		// En cours : l'officier posé (null tant qu'il est « sur le papier ») ; sa fuite est gérée par le Commandeur
		mission.m_Target = book.GetOfficerEntity(mission.m_iOfficerRegion);
	}

	//------------------------------------------------------------------------------------------------
	//! « officier ennemi Volkov » (VU1), nom montré seulement une fois l'officier tombé (VU2)
	protected string RegionOfficerText(SRP_CmdRegionBook book, int region)
	{
		string surname = book.GetOfficerName(region);
		if (surname.IsEmpty())
			return "officier ennemi";
		string text = SRP_CmdScreens.OfficerLabel(surname);
		if (text.IsEmpty())
			text = "officier ennemi " + surname;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackWreck(SRP_Mission mission, int now)
	{
		if (mission.m_iSpecialTick > 0 && now >= mission.m_iSpecialTick)
		{
			mission.m_iSpecialTick = 0;
			if (SendWave(mission, mission.m_vSite))
				SRP_Utils.NotifyAll(string.Format("Radio : %1 — équipe de récupération ennemie en route vers l'épave", mission.m_sId));
			else if (mission.m_bWaveRetry)
				mission.m_iSpecialTick = mission.m_iNextWaveTick;	// place refusée : l'équipe attend sa place (RE12), rien n'est annoncé
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackMedical(SRP_Mission mission)
	{
		IEntity civilian = mission.m_Target;
		if (!civilian || civilian.IsDeleted() || SRP_Utils.IsDead(civilian))
		{
			End(mission, SRP_EMissionState.ECHEC, "le civil est mort");
			return;
		}
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(civilian);
		if (!damage)
			return;
		HitZone zone = damage.GetDefaultHitZone();
		if (zone && zone.GetHealthScaled() >= m_fMedicalHealTarget)
		{
			mission.m_aEntities.Clear();	// le civil reste dans le village
			End(mission, SRP_EMissionState.SUCCES, "civil soigné");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackDocuments(SRP_Mission mission, array<IEntity> players, int now)
	{
		// Alerte : un garde tué avant la prise, ou des joueurs repérés sur le site (la garde peut être celle du secteur)
		SRP_EnemyComponent watch = SRP_EnemyComponent.GetInstance();
		bool spotted = false;
		if (watch)
			spotted = watch.HasAlertNear(mission.m_vSite, 250);
		if (!mission.m_bAlerted && mission.m_iCarrierId < 0 && (spotted || SRP_EnemyComponent.SoldiersIn(mission.m_aGroups, now) < mission.m_iGuardsAtStart))
		{
			mission.m_bAlerted = true;
			mission.m_iNextWaveTick = now + 60000;
			SRP_Utils.NotifyAll(string.Format("Radio : %1 — l'alerte est donnée, renforts ennemis en approche", mission.m_sId));
		}

		if (mission.m_bAlerted && (mission.m_iMaxWaves == 0 || mission.m_iWaves < mission.m_iMaxWaves) && now >= mission.m_iNextWaveTick)
		{
			mission.m_iNextWaveTick = now + m_iWaveMinutes * 60 * 1000;
			SendWave(mission, mission.m_vSite);
		}

		if (mission.m_iCarrierId < 0)
			return;

		IEntity carrier = GetGame().GetPlayerManager().GetPlayerControlledEntity(mission.m_iCarrierId);
		if (!carrier || SRP_Utils.IsDead(carrier))
		{
			// Les documents tombent là où le porteur est tombé
			vector where = mission.m_vSite;
			if (carrier)
				where = carrier.GetOrigin();
			IEntity briefcase = SpawnAt(m_sBriefcasePrefab, where + Vector(0.8, 0.2, 0));
			if (briefcase)
				mission.m_aEntities.Insert(briefcase);
			mission.m_iCarrierId = -1;
			PlaceMarker(mission, where, mission.m_sId + " Documents au sol");
			SRP_Utils.NotifyAll(string.Format("Radio : %1 — le porteur des documents est tombé, la mallette est au sol", mission.m_sId));
			return;
		}

		if (IsAtBase(carrier.GetOrigin()))
		{
			string reason = "documents ramenés par " + GetGame().GetPlayerManager().GetPlayerName(mission.m_iCarrierId);
			if (!mission.m_bAlerted)
			{
				mission.m_iRewardEuros = mission.m_iRewardEuros * 3 / 2;
				reason += " sans donner l'alerte (récompense majorée)";
			}
			End(mission, SRP_EMissionState.SUCCES, reason);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackListening(SRP_Mission mission, array<IEntity> players)
	{
		if (SRP_EnemyComponent.SoldiersIn(mission.m_aGroups, System.GetTickCount()) < mission.m_iGuardsAtStart)
		{
			End(mission, SRP_EMissionState.ECHEC, "écoute compromise, l'alerte a été donnée");
			return;
		}
		if (mission.m_aEntities.IsEmpty())
			return;
		IEntity antenna = mission.m_aEntities[0];
		if (!antenna || antenna.IsDeleted())
			return;
		if (SRP_Utils.NearestPlayerDistance(antenna.GetOrigin(), players) > 50)
			return;

		mission.m_iHoldMs += 20000;
		if (mission.m_iHoldMs >= mission.m_iHoldTarget * 60000)
		{
			string intel = "Renseignement : ";
			SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
			if (deliveries)
				intel += deliveries.GetStatusText();
			else
				intel += "rien de neuf";
			SRP_Utils.NotifyAll("Radio : " + intel);
			End(mission, SRP_EMissionState.SUCCES, "écoute complète");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void TrackBreakdown(SRP_Mission mission, array<IEntity> players, int now)
	{
		IEntity vehicle = mission.m_Target;
		if (!vehicle || vehicle.IsDeleted() || SRP_GarageUtils.IsDestroyed(vehicle))
		{
			End(mission, SRP_EMissionState.ECHEC, "véhicule détruit");
			return;
		}

		if (IsAtBase(vehicle.GetOrigin()))
		{
			mission.m_aEntities.Clear();
			End(mission, SRP_EMissionState.SUCCES, "véhicule ramené à la base");
			return;
		}

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (damage)
		{
			array<HitZone> zones = {};
			damage.GetAllHitZones(zones);
			float sum = 0;
			foreach (HitZone zone : zones)
				sum += zone.GetHealthScaled();
			if (!zones.IsEmpty() && sum / zones.Count() >= 0.7)
			{
				mission.m_aEntities.Clear();
				End(mission, SRP_EMissionState.SUCCES, "véhicule réparé sur place");
				return;
			}
		}

		// Vagues vers le véhicule tant que des joueurs sont dessus
		mission.m_vSite = vehicle.GetOrigin();
		TrackLoadingWaves(mission, players, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Événement : un véhicule du parc occupé, loin de la base, tombe en panne
	protected void CheckBreakdowns(array<IEntity> players, int now)
	{
		if (m_fBreakdownChancePerHour <= 0 || HasActiveOfType(SRP_EMissionType.PANNE))
			return;
		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (!fleet)
			return;
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (!baseMarker)
			return;

		float chancePerTick = m_fBreakdownChancePerHour / 180;	// tick de 20 s
		foreach (IEntity player : players)
		{
			IEntity vehicle = player.GetRootParent();
			if (!vehicle || vehicle == player)
				continue;
			if (!fleet.FindByEntity(vehicle))
				continue;
			if (vector.Distance(vehicle.GetOrigin(), baseMarker.GetOrigin()) < 3000)
				continue;
			if (Math.RandomFloat(0, 100) >= chancePerTick)
				continue;

			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
			if (!damage)
				continue;
			array<HitZone> zones = {};
			damage.GetAllHitZones(zones);
			foreach (HitZone zone : zones)
			{
				if (zone.GetHealthScaled() > 0.15)
					zone.SetHealthScaled(0.15);
			}

			SRP_Mission mission = NewMission(SRP_EMissionType.PANNE, vehicle.GetOrigin(), "véhicule du parc");
			mission.m_iOrigin = SRP_EMissionOrigin.EVENEMENT;
			mission.m_Target = vehicle;
			mission.m_iMaxWaves = mission.m_iDifficulty;
			mission.m_iNextWaveTick = 0;
			Announce(mission);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAtBase(vector position)
	{
		IEntity baseMarker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (!baseMarker)
			return false;
		return vector.Distance(position, baseMarker.GetOrigin()) <= m_fBaseArrivalRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Envoie une vague vers target ; vrai si elle part. Place refusée (classe COMBAT) : la demande reste notée
	//! cap_demande_s et fait partir des rondes lointaines (RE12) ; m_iNextWaveTick est alors avancé pour un nouvel essai
	//! AVANT qu'elle expire (m_bWaveRetry vrai), sinon les places libérées ne serviraient à rien.
	protected bool SendWave(SRP_Mission mission, vector target)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return false;

		// Au prorata des joueurs sur place, dans la limite du budget local (garnison, garde et vagues encore en vie comptent)
		int groups = Math.Min(enemies.ScaleGroupsNear(target, 0, 1 + mission.m_iDifficulty), enemies.LocalRoom(target));
		if (groups <= 0)
		{
			mission.m_bWaveRetry = false;	// personne sur place ou budget local plein : cette vague n'aura pas lieu
			return false;
		}
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string refusal = capacity.Ask("vague-" + mission.m_sId, SRP_ECmdCapClass.COMBAT, groups * MISSION_GROUP_SOLDIERS, groups, target, "mission");
		if (!refusal.IsEmpty())
		{
			// Nouvel essai aux deux tiers de la validité de la demande (30 s pour 45 s), une seule ligne de journal
			int retryMs = Math.MaxInt(5, capacity.m_iDemandS * 2 / 3) * 1000;
			mission.m_iNextWaveTick = System.GetTickCount() + retryMs;
			if (!mission.m_bWaveRetry)
				SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : vague en attente de place, %2 (nouvel essai toutes les %3 s)", mission.m_sId, refusal, retryMs / 1000));
			mission.m_bWaveRetry = true;
			return false;
		}
		mission.m_bWaveRetry = false;
		int firstWave = mission.m_aGroups.Count();
		int spawned = enemies.SpawnWave(target, 400, groups, "mission", mission.m_aGroups);
		TagGroups(mission.m_aGroups, firstWave, SRP_ECmdCapClass.COMBAT);
		if (spawned <= 0)
			return false;

		mission.m_iWaves++;
		SRP_EnemyComponent.Journal("MISSION", string.Format("%1 : vague %2, %3 groupe(s)", mission.m_sId, mission.m_iWaves, spawned));
		SRP_Utils.NotifyAll(string.Format("Radio : %1 — mouvement ennemi vers votre position", mission.m_sId));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Événements venus d'ailleurs
	//------------------------------------------------------------------------------------------------
	//! La capture active de la zone de ce code (« S07 », clé m_sDestinationLabel), null sinon
	protected SRP_Mission FindActiveLiberer(string zoneCode)
	{
		if (zoneCode.IsEmpty())
			return null;
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE && mission.m_iType == SRP_EMissionType.LIBERER && mission.m_sDestinationLabel == zoneCode)
				return mission;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Une zone vient de passer à nous, UNE fois par zone (SEUL payeur de la prime G8 et seul auteur de l'alerte G2) —
	//! SRP_ZoneFall.CaptureZone (staff = false) et ForceZone (staff = true).
	//! staff (Q9, correction) : ni prime ni alerte ; la mission de la zone est ANNULÉE (aucune récompense, aucun évènement).
	//! Sinon : mission active -> prime de la zone puis réussite (récompense de mission en plus) ; sans mission -> alerte G2.
	void OnZoneCaptured(string zoneCode, bool staff)
	{
		SRP_Mission mission = FindActiveLiberer(zoneCode);
		if (staff)
		{
			// Q9 : correction, ni radio ni fil Discord public ; la trace reste la ligne [STAFF] de SRP_FrontScreens.RunStaff
			if (mission)
			{
				m_bQuietEnd = true;
				End(mission, SRP_EMissionState.ANNULEE, "zone forcée par le Staff");
				m_bQuietEnd = false;
			}
			return;
		}

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int zone = -1;
		if (front)
			zone = front.FindZone(zoneCode);
		if (!mission)
		{
			// G2 : prise sans mission activée au tableau : ni prime, ni crédit de mission
			if (zone >= 0)
				SRP_FrontRadio.CaptureWithoutMission(zone);
			return;
		}

		string reason = "zone capturée";
		int prime = 0;
		if (zone >= 0)
			prime = CapturePrime(zone);
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (treasury && prime > 0)
		{
			treasury.Add(prime, string.Format("prime de capture, zone %1, mission %2", mission.m_sSiteLabel, mission.m_sId));
			reason = string.Format("zone capturée, prime de capture %1 €", prime);
		}
		End(mission, SRP_EMissionState.SUCCES, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Un objet entre dans un inventaire (crochet SRP_ArsenalEconomy) : un paquet de mission rangé dans un dépôt fait avancer la mission
	static void NotifyItemAdded(IEntity owner, IEntity item)
	{
		if (!s_Instance || !Replication.IsServer() || !owner || !item)
			return;
		if (!SRP_DepotComponent.Of(owner))
			return;
		SRP_PacketComponent packet = SRP_PacketComponent.Of(item);
		if (!packet || packet.GetMissionId().IsEmpty())
			return;

		string missionId = packet.GetMissionId();
		packet.SetMissionId("");	// ne compte qu'une fois
		s_Instance.OnPacketDelivered(missionId);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPacketDelivered(string missionId)
	{
		SRP_Mission mission = FindById(missionId);
		if (!mission || mission.m_iState != SRP_EMissionState.ACTIVE)
			return;

		mission.m_iProgress++;
		if (mission.m_iProgress >= mission.m_iTarget)
		{
			End(mission, SRP_EMissionState.SUCCES, string.Format("cargaison ramenée, %1 paquets rangés", mission.m_iProgress));
			return;
		}

		// Journal et radio aux quarts, pas à chaque paquet
		int quarter = Math.Max(1, mission.m_iTarget / 4);
		if (mission.m_iProgress - (mission.m_iProgress / quarter) * quarter == 0)
		{
			SRP_JournalComponent.Log("MISSION", string.Format("%1 : paquets rangés %2 / %3", mission.m_sId, mission.m_iProgress, mission.m_iTarget));
			SRP_Utils.NotifyAll(string.Format("Radio : %1 — paquets rangés %2 / %3", mission.m_sId, mission.m_iProgress, mission.m_iTarget));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un objectif de mission a été actionné (sabotage, neutralisation, documents). Retourne le message au joueur.
	//! SEULE entrée de « Saboter » vers le Commandeur (RE8) : dépôt ennemi ou pièce d'appui d'abord, missions ensuite.
	static string OnObjectiveUsed(IEntity entity, int playerId)
	{
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		string reply;
		if (frontEnemy && frontEnemy.OnSabotageUsed(entity, playerId, reply))
			return reply;

		if (!s_Instance)
			return "Gestionnaire de missions absent";

		foreach (SRP_Mission mission : s_Instance.m_aMissions)
		{
			if (mission.m_iState != SRP_EMissionState.ACTIVE || !mission.m_aEntities.Contains(entity))
				continue;
			return s_Instance.HandleObjective(mission, entity, playerId);
		}
		return "Cet objet n'appartient à aucune mission en cours";
	}

	//------------------------------------------------------------------------------------------------
	protected string HandleObjective(SRP_Mission mission, IEntity entity, int playerId)
	{
		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		mission.m_sDoneBy = name;
		switch (mission.m_iType)
		{
			case SRP_EMissionType.CACHE:
				return StartHold(mission, "cache sabotée par " + name);

			case SRP_EMissionType.POSTE_OBS:
			{
				SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
				if (enemies)
					enemies.AddZoneModifier("calme-" + mission.m_sId, mission.m_vSite, 2000, 0, m_iCalmMinutes);
				End(mission, SRP_EMissionState.SUCCES, string.Format("poste neutralisé par %1, secteur calme %2 min", name, m_iCalmMinutes));
				return "Poste neutralisé. Le secteur sera calme un moment.";
			}

			case SRP_EMissionType.SABOTAGE:
				End(mission, SRP_EMissionState.SUCCES, "pièce sabotée par " + name);
				return "Pièce sabotée";

			case SRP_EMissionType.DOCUMENTS:
			{
				mission.m_aEntities.RemoveItem(entity);
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
				mission.m_iCarrierId = playerId;
				if (!mission.m_aParticipants.Contains(playerId))
					mission.m_aParticipants.Insert(playerId);
				PlaceMarker(mission, mission.m_vSite, mission.m_sId + " Documents pris");
				SRP_Utils.NotifyAll(string.Format("Radio : %1 — documents récupérés par %2, à ramener à la base", mission.m_sId, name));
				return "Documents en main. Ramenez-les à la base sans les perdre.";
			}
		}
		return "Rien à faire ici";
	}

	//------------------------------------------------------------------------------------------------
	//! Cache : la contre-attaque commence, la mission se termine à l'exfiltration
	protected string StartHold(SRP_Mission mission, string reason)
	{
		if (m_iCacheHoldMinutes <= 0)
		{
			End(mission, SRP_EMissionState.SUCCES, reason);
			return "Cache sabotée";
		}

		foreach (IEntity entity : mission.m_aEntities)
		{
			if (entity && !entity.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		mission.m_aEntities.Clear();

		mission.m_bHoldPhase = true;
		mission.m_sPhaseReason = reason;
		mission.m_iHoldTarget = m_iCacheHoldMinutes;
		mission.m_iHoldMs = 0;
		mission.m_iMaxWaves = 0;
		mission.m_iNextWaveTick = System.GetTickCount() + 60000;

		string text = string.Format("%1 — cache sabotée. L'ennemi contre-attaque : tenez le site %2 minutes jusqu'à l'exfiltration", mission.m_sId, mission.m_iHoldTarget);
		SRP_JournalComponent.Log("MISSION", text);
		SRP_Utils.NotifyAll("Radio : " + text);
		return string.Format("Cache sabotée. Tenez le site %1 minutes jusqu'à l'exfiltration.", mission.m_iHoldTarget);
	}

	//------------------------------------------------------------------------------------------------
	// Prise en charge
	//------------------------------------------------------------------------------------------------
	//! granted : accordée par un officier (demande de mission), la condition de grade ne joue plus
	string Claim(int playerId, string id, bool granted = false)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";
		bool allowed = granted || record.m_bStaff || SRP_Grades.IsOfficier(record.m_iGrade) || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.CDG) || record.m_iGrade >= m_eClaimMinGrade;
		if (!allowed)
			return string.Format("Accepter une mission est réservé aux Chefs de groupe (certif CDG), aux officiers, ou à partir du grade %1", SRP_Grades.GetName(m_eClaimMinGrade));

		SRP_Mission mission = FindById(id);
		if (!mission || mission.m_iState != SRP_EMissionState.ACTIVE)
			return "Mission inconnue : " + id;
		if (!mission.m_sClaimedBy.IsEmpty())
			return string.Format("%1 est déjà acceptée par %2", mission.m_sId, mission.m_sClaimedBy);

		int now = System.GetTickCount();
		mission.m_sClaimedBy = record.m_sName;
		mission.m_iClaimedById = playerId;
		if (!mission.m_aParticipants.Contains(playerId))
			mission.m_aParticipants.Insert(playerId);
		if (mission.m_Marker)
			PlaceMarker(mission, mission.m_vMarkerPosition, mission.m_sMarkerText);
		mission.m_iClaimTick = now;
		mission.m_bTimerPaused = false;

		string text = string.Format("%1 (%2) acceptée par %3 — plus de limite de temps, jusqu'à la réussite ou l'annulation", mission.m_sId, mission.m_sTitle, record.m_sName);
		SRP_JournalComponent.Log("MISSION", text);
		// Mission automatique : tout le monde l'apprend ; trouvée ou ordonnée : les officiers et le chef seulement
		if (mission.m_iOrigin == SRP_EMissionOrigin.GENERATEUR)
			SRP_Utils.NotifyAll("Radio : " + text);
		else
		{
			players.NotifyOfficiers(text);
			SRP_Utils.NotifyPlayer(playerId, "Radio : " + text);
		}
		return string.Format("Mission %1 acceptée, sans limite de temps. Pour l'annuler : ordinateur (PC), onglet Missions.", mission.m_sId);
	}

	//------------------------------------------------------------------------------------------------
	string Abandon(int playerId, string id)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";

		SRP_Mission mission = FindById(id);
		if (!mission || mission.m_iState != SRP_EMissionState.ACTIVE)
			return "Mission inconnue : " + id;
		if (mission.m_sClaimedBy.IsEmpty())
			return mission.m_sId + " n'a été acceptée par personne, rien à annuler";
		if (mission.m_iClaimedById != playerId && !players.IsOfficier(playerId))
			return string.Format("Seul %1, un officier ou le Staff peut abandonner %2", mission.m_sClaimedBy, mission.m_sId);
		if (mission.m_iType == SRP_EMissionType.LIBERER && mission.m_bEngaged && !record.m_bStaff)
			return string.Format("%1 est engagée : plus de retour arrière. Seul le Staff peut l'annuler.", mission.m_sId);

		End(mission, SRP_EMissionState.ANNULEE, string.Format("annulée par %1 (acceptée par %2)", record.m_sName, mission.m_sClaimedBy));
		return "Mission " + mission.m_sId + " annulée";
	}

	//------------------------------------------------------------------------------------------------
	// Fin de mission
	//------------------------------------------------------------------------------------------------
	void End(SRP_Mission mission, int state, string reason)
	{
		if (mission.m_iState != SRP_EMissionState.ACTIVE)
			return;
		mission.m_iState = state;

		string outcome = "échec";
		if (state == SRP_EMissionState.SUCCES)
			outcome = "succès";
		else if (state == SRP_EMissionState.EXPIREE)
			outcome = "expirée";
		else if (state == SRP_EMissionState.ANNULEE)
			outcome = "annulée";

		string report = string.Format("Mission %1 (%2, %3) : %4 — %5", mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel, outcome, reason);
		if (!mission.m_sClaimedBy.IsEmpty() && state == SRP_EMissionState.SUCCES)
			report += " — chef de mission : " + mission.m_sClaimedBy;
		if (state == SRP_EMissionState.SUCCES || state == SRP_EMissionState.ECHEC)
			SRP_BridgeComponent.PushEvent("mission", report);

		if (state == SRP_EMissionState.SUCCES)
		{
			m_iSuccesses++;
			report += Reward(mission);
			// G9 : la réussite d'une capture ne touche plus la menace (seule la chute de la zone compte, dans le front)
			SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
			if (territory && mission.m_iType != SRP_EMissionType.LIBERER)
				territory.OnMissionSuccess(mission.m_iType);
			// Commandeur : UNE entrée par mission réussie (renseignement, dépôt révélé, quart de stock, alerte de région)
			SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
			if (frontEnemy)
			{
				string author = mission.m_sDoneBy;
				if (author.IsEmpty())
					author = mission.m_sClaimedBy;
				if (author.IsEmpty())
					author = "mission " + mission.m_sId;
				frontEnemy.OnMissionSucceeded(mission.m_iType, mission.m_vSite, mission.m_sId, mission.m_sTargetPrefab, author);
			}
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			if (players && !mission.m_aParticipants.IsEmpty())
			{
				if (m_bAutoCountMissions)
					players.AddMissionSuccess(mission.m_aParticipants, mission.m_sId);
				report += string.Format(" — %1 participant(s) vu(s) sur le site", mission.m_aParticipants.Count());
			}
		}
		else
		{
			m_iFailures++;
		}

		// L'hélicoptère de recherche venu pour cette mission (à moins de 1 km du site ou du centre de sa garde) repart
		SRP_HeliSearch.Recall(mission.m_vSite, 1000);
		if (mission.m_vGuardCenter != vector.Zero)
			SRP_HeliSearch.Recall(mission.m_vGuardCenter, 1000);
		Cleanup(mission);
		RemoveMarker(mission);
		Save();
		// Q9 : une fin silencieuse (zone forcée par le Staff) ne part ni au fil public « missions » ni à la radio
		if (m_bQuietEnd)
			SRP_JournalComponent.Log("SYSTEME", report);
		else
			SRP_JournalComponent.Log("MISSION", report);
		// Une mission que personne n'a prise disparaît sans bruit
		if (state != SRP_EMissionState.EXPIREE && !m_bQuietEnd)
			SRP_Utils.NotifyAll("Radio : " + report);
	}

	//------------------------------------------------------------------------------------------------
	protected string Reward(SRP_Mission mission)
	{
		string text = "";

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (treasury && mission.m_iRewardEuros > 0)
		{
			treasury.Add(mission.m_iRewardEuros, "subvention, mission " + mission.m_sId);
			text += string.Format(" — subvention %1 €", mission.m_iRewardEuros);
		}

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_sRewardMarkerName);
		if (resources && marker && mission.m_iRewardPackets > 0)
		{
			vector mat[4];
			marker.GetWorldTransform(mat);
			IEntity box = resources.SpawnBox(mission.m_iRewardResource, mission.m_iRewardPackets, mat[3] + mat[0] * 1.5 + vector.Up * 0.3);
			if (box)
				text += string.Format(" — caisse de %1 paquets de %2 livrée au point %3", SRP_ResourceManagerComponent.CountInBox(box, mission.m_iRewardResource), SRP_Resources.GetName(mission.m_iRewardResource), m_sRewardMarkerName);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void Cleanup(SRP_Mission mission)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
		{
			// Carte #69, livraison 3 : un camion de vague encore à l'embarquement ou en route repart avec ses soldats (il
			// n'irait pas débarquer une vague après la fin de la mission)
			SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
			if (trucks)
				trucks.RecallGroups(mission.m_aGroups);
			enemies.RetireLater(mission.m_aGroups);	// gardes et vagues survivantes : retirées après 15 min, hors de vue
			enemies.RemoveZoneModifier("poste-" + mission.m_sId);
			enemies.RemoveZoneModifier("largage-" + mission.m_sId);
		}
		// Les demandes de place encore en attente (garde, escorte, vague) ne font plus partir de rondes (RE12)
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		capacity.ClearDemand("mission-" + mission.m_sId);
		capacity.ClearDemand("vague-" + mission.m_sId);
		mission.m_bWaveRetry = false;

		foreach (IEntity entity : mission.m_aEntities)
		{
			if (!entity || entity.IsDeleted())
				continue;
			if (entity.GetParent())
				continue;	// déjà entre les mains des joueurs (véhicule, inventaire)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		mission.m_aEntities.Clear();

		// Contrôle routier : conducteurs arrêtés et véhicules saisis sont pris en charge, les autres repartent
		if (mission.m_Checkpoint)
		{
			mission.m_Checkpoint.Close();
			mission.m_Checkpoint = null;
		}

		// Les véhicules d'un convoi restent dans le monde (épaves ou prises de guerre)
		mission.m_aVehicles.Clear();
		mission.m_Target = null;
	}

	//------------------------------------------------------------------------------------------------
	void CancelAll(string reason)
	{
		foreach (SRP_Mission mission : m_aMissions)
		{
			if (mission.m_iState == SRP_EMissionState.ACTIVE)
				End(mission, SRP_EMissionState.ANNULEE, reason);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : tout annuler et repartir de M01, compteurs à zéro
	void ResetAll(string reason)
	{
		CancelAll(reason);
		m_aMissions.Clear();
		m_iNextId = 1;
		m_iSuccesses = 0;
		m_iFailures = 0;
		Save();
		SRP_JournalComponent.Log("MISSION", "Remise à zéro (" + reason + ") : compteurs effacés, le générateur repart de M01");
	}

	//------------------------------------------------------------------------------------------------
	// Marqueurs de carte
	//------------------------------------------------------------------------------------------------
	protected void PlaceMarker(SRP_Mission mission, vector position, string text)
	{
		if (!m_bMapMarkers)
			return;

		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markers)
		{
			Print("[SRP] Missions : pas de SCR_MapMarkerManagerComponent sur le game mode, pas de marqueur de carte", LogLevel.WARNING);
			return;
		}

		RemoveMarker(mission);

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		int x = position[0];
		int z = position[2];
		marker.SetWorldPos(x, z);
		marker.SetIconEntry(MarkerIcon(mission.m_iType));
		if (mission.m_iClaimedById > 0)
			marker.SetColorEntry(m_iMarkerColorClaimed);
		else
			marker.SetColorEntry(m_iMarkerColor);
		marker.SetCustomText(text);
		markers.InsertStaticMarker(marker, false, true);
		mission.m_Marker = marker;
		mission.m_vMarkerPosition = position;
		mission.m_sMarkerText = text;
	}

	//------------------------------------------------------------------------------------------------
	//! L'icône selon le type (index de la config vanilla, voir /marqueurs)
	protected int MarkerIcon(int type)
	{
		if (!m_bMarkerIconByType)
			return m_iMarkerIcon;
		switch (type)
		{
			case SRP_EMissionType.CACHE: return 44;			// destroy
			case SRP_EMissionType.SABOTAGE: return 44;
			case SRP_EMissionType.CARGAISON: return 31;		// pick-up
			case SRP_EMissionType.EPAVE: return 31;
			case SRP_EMissionType.LARGAGE: return 31;
			case SRP_EMissionType.CARBURANT: return 31;
			case SRP_EMissionType.VEHICULE: return 40;		// waypoint
			case SRP_EMissionType.CONVOI: return 60;		// ambush
			case SRP_EMissionType.POSTE_OBS: return 29;		// observation-post
			case SRP_EMissionType.OFFICIER: return 58;		// target-reference-point
			case SRP_EMissionType.MINES: return 21;			// mine-field
			case SRP_EMissionType.MEDICAL: return 46;		// heal
			case SRP_EMissionType.DOCUMENTS: return 33;		// point-of-interest
			case SRP_EMissionType.ECOUTE: return 62;		// reconnaissance
			case SRP_EMissionType.PANNE: return 47;			// help
			case SRP_EMissionType.LIBERER: return 49;		// attack
			case SRP_EMissionType.CONTROLE: return 33;		// point-of-interest
		}
		return m_iMarkerIcon;
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveMarker(SRP_Mission mission)
	{
		if (!mission.m_Marker)
			return;
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (markers)
			markers.RemoveStaticMarker(mission.m_Marker);
		mission.m_Marker = null;
	}

	//------------------------------------------------------------------------------------------------
	// Persistance (compteurs seulement)
	//------------------------------------------------------------------------------------------------
	void Save()
	{
		if (!Replication.IsServer())
			return;
		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("nextId", m_iNextId);
		ctx.WriteValue("successes", m_iSuccesses);
		ctx.WriteValue("failures", m_iFailures);
		ctx.WriteValue("savedAt", SRP_Time.Now());
		if (!ctx.SaveToFile(MISSIONS_PATH))
			Print("[SRP] Échec de sauvegarde des missions : " + MISSIONS_PATH, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		if (!FileIO.FileExists(MISSIONS_PATH))
			return;
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(MISSIONS_PATH))
			return;
		ctx.ReadValue("nextId", m_iNextId);
		ctx.ReadValue("successes", m_iSuccesses);
		ctx.ReadValue("failures", m_iFailures);
		if (m_iNextId < 1)
			m_iNextId = 1;
		SRP_JournalComponent.Log("MISSION", string.Format("Missions : %1 réussie(s), %2 échouée(s) à ce jour", m_iSuccesses, m_iFailures));
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		return string.Format("Missions : %1 active(s), %2 réussie(s), %3 échouée(s)\n%4", CountActive(), m_iSuccesses, m_iFailures, GetBoardText());
	}
}

//------------------------------------------------------------------------------------------------
//! Objectif à saboter ou neutraliser (cache, poste d'observation, mortier, AA, radar)
class SRP_SabotageAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;
		SRP_Utils.NotifyPlayer(playerId, SRP_MissionManagerComponent.OnObjectiveUsed(pOwnerEntity, playerId));
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Saboter";
		return true;
	}

	override bool CanBeShownScript(IEntity user) { return true; }
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! Mallette de documents
class SRP_DocumentsAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;
		SRP_Utils.NotifyPlayer(playerId, SRP_MissionManagerComponent.OnObjectiveUsed(pOwnerEntity, playerId));
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Récupérer les documents";
		return true;
	}

	override bool CanBeShownScript(IEntity user) { return true; }
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! Tableau des missions au PC (prefab SRP_TableauMissions)
class SRP_MissionBoardAction : ScriptedUserAction
{
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
		{
			SRP_Utils.NotifyPlayer(playerId, "Gestionnaire de missions absent du game mode");
			return;
		}
		SRP_Utils.HintPlayer(playerId, "Tableau des missions", missions.GetBoardText() + "\nPour partir : action « Demander une mission » de ce tableau.");
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Consulter les missions";
		return true;
	}

	override bool CanBeShownScript(IEntity user) { return true; }
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}
