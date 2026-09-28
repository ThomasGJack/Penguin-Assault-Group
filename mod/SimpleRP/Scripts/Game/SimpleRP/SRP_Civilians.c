//------------------------------------------------------------------------------------------------
// SimpleRP — Ambiance civile
// Au démarrage, le gestionnaire (SRP_CivilianManagerComponent, sur le game mode) lit les localités
// de la carte (villes, bourgs, villages, hameaux, celles dont le nom s'affiche sur la carte) et crée
// une zone civile par localité, avec un gabarit par type : rayon, promeneurs, PNJ immobiles,
// voitures garées, voitures en circulation. Rien à poser à la main.
// Une zone posée à la main (prefab SRP_ZoneCivile, composant SRP_CivilZoneComponent) remplace la
// zone automatique de la localité voisine et permet d'affiner : marqueurs enfants dans la
// hiérarchie du World Editor, nommés Poste_x (PNJ immobile, orienté comme le marqueur), Parking_x
// (voiture garée), Route_x (départ d'une voiture en circulation). Sans marqueur, les comptes
// « automatiques » de la zone se placent au hasard.
// Une zone s'anime quand un joueur s'approche, se vide quand tout le monde est loin. Les PNJ tués
// reviennent après un délai. Les voitures en circulation vont de zone en zone, sans fin.
// Circulation routière : en plus, des voitures sont posées SUR les routes (réseau routier du jeu), hors de vue, tournées
// vers les joueurs, avec une localité située au-delà d'eux pour destination : on les croise entre deux villes.
// Base interdite : aucun civil n'apparaît, ne se promène, ne fuit ni ne passe (à pied ou en voiture, trajet en ligne
// droite) dans le rayon réglé autour de la base ; une localité dont le centre y tombe ne s'anime pas.
// Front (#75, G14) : les civils suivent le territoire. Une localité sur un carré bleu prend m_iBluePercent de son
// gabarit (150), sur un carré rouge m_iRedPercent (25) ; les localités bleues se remplissent d'abord sous le plafond
// m_iMaxCivilians (inchangé) ; moins de circulation autour des joueurs en territoire rouge (m_iTrafficPerGroupRed).
// Place du jeu (Q7, limite de 160 IA actives) : chaque civil demande SRP_CmdCapacity.CivilianRefusal avant d'apparaître
// (les civils ne prennent jamais la place que les 120 soldats ennemis peuvent encore demander), et son groupe reçoit
// son importance par SRP_CmdCapacity.TagCivilianGroup (seul SetImportance d'un civil). Tant que la limite du jeu n'est
// pas portée à 160 (128 au départ), seule la marge sous la limite du jeu compte (HasCivilianRoom).
// Commandeur (C4) : GetInformantCandidates donne les civils à pied vivants, que le Commandeur filtre (informateurs).
// Prérequis moteur : SCR_AIWorld dans le monde (navmesh piétons + réseau routier), faction CIV.
// Phase 4 — étape A
//------------------------------------------------------------------------------------------------

//! Gabarit d'une zone automatique selon le type de localité
[BaseContainerProps()]
class SRP_CivPreset
{
	[Attribute("150", UIWidgets.EditBox, "Rayon de la zone en mètres")]
	float m_fRadius = 150;

	[Attribute("6", UIWidgets.EditBox, "Promeneurs")]
	int m_iWalkers = 6;

	[Attribute("2", UIWidgets.EditBox, "PNJ immobiles placés au hasard")]
	int m_iPosts = 2;

	[Attribute("2", UIWidgets.EditBox, "Voitures garées placées au hasard")]
	int m_iParked = 2;

	[Attribute("1", UIWidgets.EditBox, "Voitures en circulation partant d'ici")]
	int m_iDrivers = 1;

	//------------------------------------------------------------------------------------------------
	void SRP_CivPreset(float radius = 150, int walkers = 6, int posts = 2, int parked = 2, int drivers = 1)
	{
		m_fRadius = radius;
		m_iWalkers = walkers;
		m_iPosts = posts;
		m_iParked = parked;
		m_iDrivers = drivers;
	}
}

enum SRP_ECivKind
{
	WALKER,
	POST,
	PARKED,
	DRIVER,
	TRAFFIC		// circulation routière : posée sur une route près d'un joueur, sans zone d'origine
}

//! Un PNJ ou un véhicule civil géré
class SRP_CivRecord
{
	int m_iKind;
	IEntity m_Character;
	SCR_AIGroup m_Group;
	IEntity m_Vehicle;
	IEntity m_Waypoint;		// point de passage IA (promeneur, conducteur)
	IEntity m_Marker;		// marqueur Poste_x / Parking_x / Route_x occupé, null si placé au hasard
	SRP_CivilZoneComponent m_Zone;
	SRP_CivilZoneComponent m_Destination;
	int m_iNextOrderTick;
	int m_iStuckSince;
	vector m_vLastPosition;
	bool m_bCheckpoint;			// prise dans un point de contrôle (SRP_Checkpoint) : c'est lui qui la conduit et qui la retire
	bool m_bCheckpointDone;		// déjà contrôlée : un point de contrôle ne la reprend pas

	// Réflexes de danger
	int m_iDanger;					// 0 calme, 1 à plat ventre, 2 en fuite, 3 caché
	int m_iDangerUntil;				// fin de l'état courant (tick)
	vector m_vThreat;				// d'où vient le danger
	vector m_vHome;					// où revenir une fois calmé (postés)
	int m_iPendingSeverity;			// danger reçu depuis le dernier passage : 1 tir entendu, 2 balle proche / explosion
	vector m_vPendingThreat;
}

enum SRP_ECivDanger
{
	CALME = 0,
	PRONE = 1,
	FUITE = 2,
	CACHE = 3
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Zone d'ambiance civile : promeneurs au hasard dans le rayon, marqueurs enfants Poste_x / Parking_x / Route_x")]
class SRP_CivilZoneComponentClass : ScriptComponentClass
{
}

class SRP_CivilZoneComponent : ScriptComponent
{
	[Attribute("150", UIWidgets.EditBox, "Rayon de la zone en mètres (promeneurs, points de marche)", category: "SimpleRP")]
	float m_fRadius;

	[Attribute("6", UIWidgets.EditBox, "Nombre de promeneurs", category: "SimpleRP")]
	int m_iWalkers;

	[Attribute("1", UIWidgets.CheckBox, "Remplir les marqueurs Poste_x (PNJ immobiles)", category: "SimpleRP")]
	bool m_bPosts;

	[Attribute("1", UIWidgets.CheckBox, "Remplir les marqueurs Parking_x (voitures garées)", category: "SimpleRP")]
	bool m_bParkings;

	[Attribute("1", UIWidgets.CheckBox, "Remplir les marqueurs Route_x (voitures en circulation vers d'autres zones)", category: "SimpleRP")]
	bool m_bDrivers;

	[Attribute("0", UIWidgets.EditBox, "PNJ immobiles placés au hasard, en plus des marqueurs Poste_x", category: "SimpleRP")]
	int m_iAutoPosts;

	[Attribute("0", UIWidgets.EditBox, "Voitures garées placées au hasard, en plus des marqueurs Parking_x", category: "SimpleRP")]
	int m_iAutoParked;

	[Attribute("0", UIWidgets.EditBox, "Voitures en circulation placées au hasard, en plus des marqueurs Route_x", category: "SimpleRP")]
	int m_iAutoDrivers;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefabs de PNJ propres à cette zone (vide = liste du gestionnaire)", "et", category: "SimpleRP")]
	ref array<ResourceName> m_aCharacterPrefabs;

	protected static ref array<SRP_CivilZoneComponent> s_aZones = {};

	bool m_bActive;
	bool m_bAuto;			// créée par le gestionnaire depuis la carte
	bool m_bReported;		// rapport de peuplement écrit dans le journal
	int m_iActivatedTick;
	int m_iDeadCount;
	int m_iRespawnTick;
	int m_iTerritoryPercent = 100;	// G14 : part du gabarit selon le carré du centre (bleu, rouge), recalculée à chaque passage
	bool m_bTerritoryBlue;			// G14 : centre sur un carré bleu (remplie avant les autres)

	//------------------------------------------------------------------------------------------------
	//! Réglages d'une zone automatique
	void ApplyPreset(SRP_CivPreset preset)
	{
		m_fRadius = preset.m_fRadius;
		m_iWalkers = preset.m_iWalkers;
		m_iAutoPosts = preset.m_iPosts;
		m_iAutoParked = preset.m_iParked;
		m_iAutoDrivers = preset.m_iDrivers;
		m_bAuto = true;
	}

	//------------------------------------------------------------------------------------------------
	static array<SRP_CivilZoneComponent> GetZones()
	{
		return s_aZones;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer() && !s_aZones.Contains(this))
			s_aZones.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aZones.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetZoneName()
	{
		string name = GetOwner().GetName();
		if (name.IsEmpty())
			return "zone civile";
		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected string m_sPlaceName;		// nom du lieu sur la carte (zones automatiques)
	protected string m_sPlaceKind;		// "village", "hameau" ; vide pour une zone posée à la main

	void SetPlace(string placeName, string kind)
	{
		m_sPlaceName = placeName;
		m_sPlaceKind = kind;
	}

	string GetPlaceName()
	{
		if (m_sPlaceName.IsEmpty())
			return GetZoneName();
		return m_sPlaceName;
	}

	string GetPlaceKind()
	{
		return m_sPlaceKind;
	}

	//------------------------------------------------------------------------------------------------
	vector GetCenter()
	{
		return GetOwner().GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Marqueurs enfants dont le nom commence par prefix (Poste, Parking, Route)
	void GetMarkers(string prefix, out array<IEntity> markers)
	{
		markers.Clear();
		IEntity child = GetOwner().GetChildren();
		while (child)
		{
			string name = child.GetName();
			if (name.StartsWith(prefix))
				markers.Insert(child);
			child = child.GetSibling();
		}
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Gestionnaire d'ambiance civile SimpleRP : anime les zones civiles (PNJ, voitures)")]
class SRP_CivilianManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_CivilianManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Réflexes de danger : un tir entendu fait fuir, une balle qui passe fait plonger, une explosion fait se coucher puis fuir, puis on se cache le temps que ça se calme", category: "SimpleRP - Danger")]
	protected bool m_bDangerReactions;

	[Attribute("250", UIWidgets.EditBox, "Distance à laquelle un tir entendu fait fuir, en mètres", category: "SimpleRP - Danger")]
	protected float m_fFleeHearingDistance;

	[Attribute("10", UIWidgets.EditBox, "Distance à laquelle une balle qui passe fait plonger, en mètres", category: "SimpleRP - Danger")]
	protected float m_fProneDistance;

	[Attribute("150", UIWidgets.EditBox, "Distance de fuite, en mètres (à l'opposé du danger, derrière un bâtiment si possible)", category: "SimpleRP - Danger")]
	protected float m_fFleeDistance;

	[Attribute("120", UIWidgets.EditBox, "Secondes de calme avant de reprendre sa vie", category: "SimpleRP - Danger")]
	protected int m_iCalmSeconds;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefabs de PNJ civils (tirés au hasard)", "et", category: "SimpleRP - Civils")]
	protected ref array<ResourceName> m_aCharacterPrefabs;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefabs de voitures civiles (tirées au hasard)", "et", category: "SimpleRP - Civils")]
	protected ref array<ResourceName> m_aVehiclePrefabs;

	[Attribute("{000CD338713F2B5A}Prefabs/AI/Groups/Group_Base.et", UIWidgets.ResourceNamePicker, "Prefab de groupe IA vide", "et", category: "SimpleRP - Civils")]
	protected ResourceName m_sGroupPrefab;

	[Attribute("{750A8D1695BD6998}Prefabs/AI/Waypoints/AIWaypoint_Move.et", UIWidgets.ResourceNamePicker, "Prefab de point de passage « Move »", "et", category: "SimpleRP - Civils")]
	protected ResourceName m_sMoveWaypointPrefab;

	[Attribute("CIV", UIWidgets.EditBox, "Clé de la faction civile", category: "SimpleRP - Civils")]
	protected string m_sFactionKey;

	[Attribute("1", UIWidgets.CheckBox, "Créer une zone par localité de la carte au démarrage", category: "SimpleRP - Zones automatiques")]
	protected bool m_bAutoZones;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab de zone (SRP_ZoneCivile.et, porteur de SRP_CivilZoneComponent)", "et", category: "SimpleRP - Zones automatiques")]
	protected ResourceName m_sZonePrefab;

	[Attribute("", UIWidgets.Object, "Gabarit ville (vide = 300 m, 10 promeneurs, 3 immobiles, 4 garées, 2 en route)", category: "SimpleRP - Zones automatiques")]
	protected ref SRP_CivPreset m_CityPreset;

	[Attribute("", UIWidgets.Object, "Gabarit bourg (vide = 200 m, 7, 2, 3, 1)", category: "SimpleRP - Zones automatiques")]
	protected ref SRP_CivPreset m_TownPreset;

	[Attribute("", UIWidgets.Object, "Gabarit village (vide = 130 m, 4, 1, 2, 1)", category: "SimpleRP - Zones automatiques")]
	protected ref SRP_CivPreset m_VillagePreset;

	[Attribute("", UIWidgets.Object, "Gabarit hameau (vide = 80 m, 2, 1, 1, 0)", category: "SimpleRP - Zones automatiques")]
	protected ref SRP_CivPreset m_SettlementPreset;

	[Attribute("300", UIWidgets.EditBox, "Une zone posée à la main à moins de … mètres d'une localité remplace la zone automatique", category: "SimpleRP - Zones automatiques")]
	protected float m_fManualOverrideDistance;

	[Attribute("6000", UIWidgets.EditBox, "Distance maximale entre deux zones pour un trajet de voiture, en mètres", category: "SimpleRP - Comportement")]
	protected float m_fMaxTripDistance;

	[Attribute("1500", UIWidgets.EditBox, "Une zone s'anime quand un joueur est à moins de … mètres", category: "SimpleRP - Distances")]
	protected float m_fActivateDistance;

	[Attribute("2500", UIWidgets.EditBox, "Une zone se vide quand tous les joueurs sont à plus de … mètres", category: "SimpleRP - Distances")]
	protected float m_fDeactivateDistance;

	[Attribute("25", UIWidgets.EditBox, "Nombre maximal de PNJ civils vivants en même temps (25 depuis la carte #69 : limite de 160 IA actives du jeu, partagée avec les soldats ennemis)", category: "SimpleRP - Distances")]
	protected int m_iMaxCivilians;

	[Attribute("10", UIWidgets.EditBox, "Cadence de la boucle d'ambiance, en secondes (activation des zones, apparitions, entretien)", category: "SimpleRP - Comportement")]
	protected int m_iTickSeconds;

	[Attribute("4", UIWidgets.EditBox, "Apparitions maximales par cycle, toutes zones confondues (réparties équitablement entre les zones actives)", category: "SimpleRP - Comportement")]
	protected int m_iSpawnsPerTick;

	[Attribute("1", UIWidgets.CheckBox, "Les promeneurs marchent au pas ; ils ne courent qu'en réaction à un danger (tirs, explosion)", category: "SimpleRP - Comportement")]
	protected bool m_bWalk;

	[Attribute("90", UIWidgets.EditBox, "Délai moyen entre deux points de marche d'un promeneur, en secondes", category: "SimpleRP - Comportement")]
	protected int m_iWanderSeconds;

	[Attribute("600", UIWidgets.EditBox, "Délai avant qu'un PNJ tué soit remplacé, en secondes", category: "SimpleRP - Comportement")]
	protected int m_iRespawnSeconds;

	[Attribute("60", UIWidgets.EditBox, "Distance d'arrivée d'une voiture en circulation, en mètres", category: "SimpleRP - Comportement")]
	protected float m_fArrivalDistance;

	[Attribute("180", UIWidgets.EditBox, "Une voiture immobile depuis … secondes est considérée bloquée et retirée", category: "SimpleRP - Comportement")]
	protected int m_iStuckSeconds;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur de la base militaire", category: "SimpleRP - Base interdite")]
	protected string m_sBaseMarkerName;

	[Attribute("300", UIWidgets.EditBox, "Aucun civil dans ce rayon autour de la base, en mètres : ni apparition, ni promenade, ni fuite, ni trajet à pied ou en voiture qui la traverse (0 = pas d'interdiction)", category: "SimpleRP - Base interdite")]
	protected float m_fBaseExclusionRadius;

	[Attribute("1", UIWidgets.CheckBox, "Circulation routière : des voitures civiles apparaissent SUR les routes, hors de vue, et roulent vers une localité en passant du côté des joueurs (on les croise)", category: "SimpleRP - Circulation")]
	protected bool m_bTraffic;

	[Attribute("2", UIWidgets.EditBox, "Voitures en circulation autour de chaque groupe de joueurs (dans 1 500 m)", category: "SimpleRP - Circulation")]
	protected int m_iTrafficPerGroup;

	[Attribute("8", UIWidgets.EditBox, "Voitures civiles qui roulent en même temps, au plus (circulation + voitures parties des localités)", category: "SimpleRP - Circulation")]
	protected int m_iMaxDrivers;

	[Attribute("20", UIWidgets.EditBox, "Secondes entre deux apparitions de voitures sur les routes", category: "SimpleRP - Circulation")]
	protected int m_iTrafficSeconds;

	[Attribute("450", UIWidgets.EditBox, "Une voiture n'apparaît jamais à moins de … mètres d'un joueur", category: "SimpleRP - Circulation")]
	protected float m_fTrafficMinDistance;

	[Attribute("1000", UIWidgets.EditBox, "… ni à plus de … mètres du groupe de joueurs pour lequel elle est posée", category: "SimpleRP - Circulation")]
	protected float m_fTrafficMaxDistance;

	[Attribute("1", UIWidgets.CheckBox, "Les civils suivent le territoire du front : plus nombreux dans les localités à nous (carré bleu), rares chez l'ennemi (carré rouge)", category: "SimpleRP - Territoire")]
	protected bool m_bFollowTerritory;

	[Attribute("150", UIWidgets.EditBox, "Gabarit d'une localité sur un carré bleu, en pour cent (promeneurs, immobiles, voitures qui partent ; pas les voitures garées)", category: "SimpleRP - Territoire")]
	protected int m_iBluePercent;

	[Attribute("25", UIWidgets.EditBox, "Gabarit d'une localité sur un carré rouge, en pour cent", category: "SimpleRP - Territoire")]
	protected int m_iRedPercent;

	[Attribute("1", UIWidgets.EditBox, "Voitures en circulation autour d'un groupe de joueurs en territoire rouge (au lieu de la valeur de la circulation)", category: "SimpleRP - Territoire")]
	protected int m_iTrafficPerGroupRed;

	protected static SRP_CivilianManagerComponent s_Instance;

	protected bool m_bWarnedCharacters;
	protected bool m_bWarnedVehicles;
	protected int m_iRoomLogTick;			// Q7 : dernier refus de place écrit au journal (au plus un toutes les 10 min)
	protected ref array<ref SRP_CivRecord> m_aRecords = {};
	protected Faction m_Faction;
	protected int m_iNextTrafficTick;
	protected bool m_bBaseKnown;
	protected vector m_vBase;

	//------------------------------------------------------------------------------------------------
	static SRP_CivilianManagerComponent GetInstance()
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
		GetGame().GetCallqueue().CallLater(Start, 5000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(TickDanger);
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Start()
	{
		if (!GetGame().GetAIWorld())
			Print("[SRP] Ambiance civile : aucun SCR_AIWorld dans le monde, les PNJ ne bougeront pas", LogLevel.ERROR);
		if (!m_aCharacterPrefabs || m_aCharacterPrefabs.IsEmpty())
			Print("[SRP] Ambiance civile : aucun prefab de PNJ civil réglé (SRP_CivilianManagerComponent), aucun PNJ n'apparaîtra", LogLevel.WARNING);
		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
			Print("[SRP] Ambiance civile : aucun prefab de voiture civile réglé (SRP_CivilianManagerComponent), aucune voiture n'apparaîtra", LogLevel.WARNING);

		FactionManager factions = GetGame().GetFactionManager();
		if (factions)
			m_Faction = factions.GetFactionByKey(m_sFactionKey);
		if (!m_Faction)
			Print("[SRP] Ambiance civile : faction " + m_sFactionKey + " introuvable, les PNJ seront sans faction", LogLevel.WARNING);

		if (m_bAutoZones)
			CreateAutoZones();

		SRP_JournalComponent.Log("CIVILS", string.Format("Ambiance civile démarrée : %1 zone(s)", SRP_CivilZoneComponent.GetZones().Count()));
		if (m_iTickSeconds < 2)
			m_iTickSeconds = 2;
		GetGame().GetCallqueue().CallLater(Tick, m_iTickSeconds * 1000, true);
		GetGame().GetCallqueue().CallLater(TickDanger, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	// Zones automatiques depuis les localités de la carte
	//------------------------------------------------------------------------------------------------
	protected void CreateAutoZones()
	{
		if (m_sZonePrefab.IsEmpty())
		{
			Print("[SRP] Ambiance civile : aucun prefab de zone réglé, pas de zones automatiques", LogLevel.WARNING);
			return;
		}

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			Print("[SRP] Ambiance civile : pas d'entité de carte dans le monde, pas de zones automatiques", LogLevel.ERROR);
			return;
		}

		int created = 0;
		created += CreateAutoZonesOfType(mapEntity, EMapDescriptorType.MDT_NAME_CITY, Preset(m_CityPreset, 300, 10, 3, 4, 2), "ville");
		created += CreateAutoZonesOfType(mapEntity, EMapDescriptorType.MDT_NAME_TOWN, Preset(m_TownPreset, 200, 7, 2, 3, 1), "bourg");
		created += CreateAutoZonesOfType(mapEntity, EMapDescriptorType.MDT_NAME_VILLAGE, Preset(m_VillagePreset, 130, 4, 1, 2, 1), "village");
		created += CreateAutoZonesOfType(mapEntity, EMapDescriptorType.MDT_NAME_SETTLEMENT, Preset(m_SettlementPreset, 80, 2, 1, 1, 0), "hameau");

		if (created == 0)
			Print("[SRP] Ambiance civile : aucune localité trouvée sur la carte (descripteurs de noms absents ?)", LogLevel.WARNING);
		else
			SRP_JournalComponent.Log("CIVILS", string.Format("%1 zone(s) automatique(s) créée(s) depuis la carte", created));
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CivPreset Preset(SRP_CivPreset configured, float radius, int walkers, int posts, int parked, int drivers)
	{
		if (configured)
			return configured;
		return new SRP_CivPreset(radius, walkers, posts, parked, drivers);
	}

	//------------------------------------------------------------------------------------------------
	protected int CreateAutoZonesOfType(SCR_MapEntity mapEntity, int type, SRP_CivPreset preset, string label)
	{
		array<MapItem> items = {};
		mapEntity.GetByType(items, type);

		int created = 0;
		foreach (MapItem item : items)
		{
			if (!item)
				continue;
			IEntity location = item.Entity();
			if (!location)
				continue;

			vector position = location.GetOrigin();
			if (HasManualZoneNear(position))
				continue;

			string name = WidgetManager.Translate(item.GetDisplayName());
			if (name.IsEmpty())
				name = label;

			IEntity zoneEntity = SpawnAtPosition(m_sZonePrefab, position);
			if (!zoneEntity)
				break;

			SRP_CivilZoneComponent zone = SRP_CivilZoneComponent.Cast(zoneEntity.FindComponent(SRP_CivilZoneComponent));
			if (!zone)
			{
				Print("[SRP] Ambiance civile : le prefab de zone n'a pas de SRP_CivilZoneComponent", LogLevel.ERROR);
				SCR_EntityHelper.DeleteEntityAndChildren(zoneEntity);
				break;
			}

			zoneEntity.SetName(string.Format("%1 (%2)", name, label));
			zone.SetPlace(name, label);
			zone.ApplyPreset(preset);
			created++;
		}
		return created;
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasManualZoneNear(vector position)
	{
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
		{
			if (!zone.m_bAuto && vector.Distance(zone.GetCenter(), position) < m_fManualOverrideDistance)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Boucle
	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		array<IEntity> players = {};
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character)
				players.Insert(character);
		}

		int now = System.GetTickCount();

		// Zones : activation / désactivation
		array<SRP_CivilZoneComponent> zones = SRP_CivilZoneComponent.GetZones();
		foreach (SRP_CivilZoneComponent zone : zones)
		{
			float distance = NearestPlayerDistance(zone.GetCenter(), players);
			if (InBase(zone.GetCenter()))
				continue;	// une localité dont le centre est dans la base ne s'anime jamais
			if (!zone.m_bActive && distance < m_fActivateDistance)
			{
				zone.m_bActive = true;
				zone.m_bReported = false;
				zone.m_iActivatedTick = now;
				SRP_JournalComponent.Log("CIVILS", zone.GetZoneName() + " : animation");
			}
			else if (zone.m_bActive && !zone.m_bReported && now - zone.m_iActivatedTick > 30000)
			{
				zone.m_bReported = true;
				SRP_JournalComponent.Log("CIVILS", string.Format("%1 : %2 promeneur(s), %3 immobile(s), %4 garée(s), %5 en route (rayon %6 m, centre %7)", zone.GetZoneName(), CountZone(zone, SRP_ECivKind.WALKER), CountZone(zone, SRP_ECivKind.POST), CountZone(zone, SRP_ECivKind.PARKED), CountZone(zone, SRP_ECivKind.DRIVER), Math.Round(zone.m_fRadius), zone.GetCenter().ToString()));
			}
			else if (zone.m_bActive && distance > m_fDeactivateDistance)
			{
				zone.m_bActive = false;
				ClearZone(zone, players);
				SRP_JournalComponent.Log("CIVILS", zone.GetZoneName() + " : mise en veille");
			}

			// G14 : la part du gabarit suit le carré du centre (un appel par zone active et par passage)
			if (zone.m_bActive)
			{
				bool blueCentre;
				zone.m_iTerritoryPercent = TerritoryPercent(zone.GetCenter(), blueCentre);
				zone.m_bTerritoryBlue = blueCentre;
			}
		}

		// Entretien des PNJ existants
		for (int i = m_aRecords.Count() - 1; i >= 0; i--)
		{
			SRP_CivRecord record = m_aRecords[i];
			if (record.m_bCheckpoint)
				continue;	// tenue par un point de contrôle : ni ordre de route, ni retrait
			if (!Maintain(record, players, now))
			{
				Remove(record, false);
				m_aRecords.Remove(i);
			}
		}

		// Remplissage progressif des zones actives : un tour de rôle, une apparition par zone et par passe,
		// jusqu'à épuisement du budget du cycle (lisse la charge et n'affame aucune zone)
		foreach (SRP_CivilZoneComponent zone : zones)
		{
			if (zone.m_bActive && zone.m_iDeadCount > 0 && now >= zone.m_iRespawnTick)
			{
				zone.m_iDeadCount--;
				zone.m_iRespawnTick = now + m_iRespawnSeconds * 1000;
			}
		}

		TickTraffic(players, now);

		// G14 : les localités bleues d'abord, puis les autres ; leurs civils passent donc en premier sous le plafond
		array<SRP_CivilZoneComponent> ordered = {};
		foreach (SRP_CivilZoneComponent blueZone : zones)
		{
			if (blueZone.m_bActive && blueZone.m_bTerritoryBlue)
				ordered.Insert(blueZone);
		}
		foreach (SRP_CivilZoneComponent otherZone : zones)
		{
			if (otherZone.m_bActive && !otherZone.m_bTerritoryBlue)
				ordered.Insert(otherZone);
		}

		int budget = m_iSpawnsPerTick;
		bool progressed = true;
		while (budget > 0 && progressed)
		{
			progressed = false;
			foreach (SRP_CivilZoneComponent fillZone : ordered)
			{
				if (budget <= 0)
					break;
				if (!fillZone.m_bActive)
					continue;
				if (FillZone(fillZone, 1, now) == 0)
				{
					budget--;
					progressed = true;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Territoire du front (G14)
	//------------------------------------------------------------------------------------------------
	//! Part du gabarit (en pour cent) d'une localité dont le centre est à cette position : m_iBluePercent sur un carré
	//! bleu (blue = vrai), m_iRedPercent sur un carré rouge, 100 sinon (mer, hors grille, front absent ou pas prêt,
	//! m_bFollowTerritory éteint) — Tick
	protected int TerritoryPercent(vector position, out bool blue)
	{
		blue = false;
		if (!m_bFollowTerritory)
			return 100;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return 100;
		if (front.IsBlueAt(position))
		{
			blue = true;
			return Math.MaxInt(m_iBluePercent, 0);
		}
		if (front.IsRedAt(position))
			return Math.MaxInt(m_iRedPercent, 0);
		return 100;
	}

	//------------------------------------------------------------------------------------------------
	//! Un compte du gabarit mis à la part de la zone, arrondi au plus proche (4 promeneurs à 25 : 1 ; à 150 : 6)
	protected int Scaled(int count, SRP_CivilZoneComponent zone)
	{
		if (!zone)
			return Math.MaxInt(count, 0);
		return Math.MaxInt(0, (count * zone.m_iTerritoryPercent + 50) / 100);
	}

	//------------------------------------------------------------------------------------------------
	// Base interdite aux civils
	//------------------------------------------------------------------------------------------------
	protected bool FindBase()
	{
		if (m_bBaseKnown)
			return true;
		if (m_fBaseExclusionRadius <= 0 || m_sBaseMarkerName.IsEmpty())
			return false;
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (!marker)
			return false;
		m_vBase = marker.GetOrigin();
		m_bBaseKnown = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce point est-il dans la base (rayon interdit, plus une marge) ?
	protected bool InBase(vector position, float margin = 0)
	{
		if (!FindBase())
			return false;
		vector flat = position - m_vBase;
		flat[1] = 0;
		return flat.Length() < m_fBaseExclusionRadius + margin;
	}

	//------------------------------------------------------------------------------------------------
	//! Le trajet en ligne droite de « start » à « finish » passe-t-il par la base ? (un civil ne la traverse pas, même si
	//! son départ et son arrivée sont dehors)
	protected bool CrossesBase(vector start, vector finish, float margin = 0)
	{
		if (!FindBase())
			return false;
		vector a = start;
		vector b = finish;
		vector c = m_vBase;
		a[1] = 0;
		b[1] = 0;
		c[1] = 0;
		vector ab = b - a;
		float lengthSq = ab.LengthSq();
		float t = 0;
		if (lengthSq > 0.01)
			t = Math.Clamp(vector.Dot(c - a, ab) / lengthSq, 0, 1);
		vector closest = a + ab * t;
		return vector.Distance(closest, c) < m_fBaseExclusionRadius + margin;
	}

	//------------------------------------------------------------------------------------------------
	// Circulation routière
	//------------------------------------------------------------------------------------------------
	protected int CountDrivers()
	{
		int count = 0;
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_iKind == SRP_ECivKind.DRIVER || record.m_iKind == SRP_ECivKind.TRAFFIC)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Un emplacement SUR une route près de « around » (à moins de « reach » mètres), orienté dans le sens de la route qui
	//! rapproche de « toward » (vector.Zero = sens au hasard)
	protected bool FindRoadTransform(vector around, float reach, vector toward, out vector mat[4])
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;
		RoadNetworkManager roads = aiWorld.GetRoadNetworkManager();
		if (!roads)
			return false;

		BaseRoad road;
		float distance;
		roads.GetClosestRoad(around, road, distance, true);
		if (!road || distance > reach)
			return false;
		array<vector> points = {};
		road.GetPoints(points);
		if (points.Count() < 2)
			return false;

		// Le point de la route le plus proche, et son voisin pour le sens
		int best = 0;
		float bestDistance = 1000000;
		foreach (int index, vector point : points)
		{
			float d = vector.Distance(point, around);
			if (d < bestDistance)
			{
				bestDistance = d;
				best = index;
			}
		}
		int other = best + 1;
		if (other >= points.Count())
			other = best - 1;
		vector position = points[best];
		vector heading = points[other] - position;
		heading[1] = 0;
		if (heading.Length() < 0.5)
			return false;
		heading.Normalize();

		if (toward != vector.Zero)
		{
			vector wanted = toward - position;
			wanted[1] = 0;
			if (vector.Dot(wanted, heading) < 0)
				heading = heading * -1;
		}
		else if (Math.RandomInt(0, 2) == 0)
			heading = heading * -1;

		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		if (position[1] < 1)
			return false;
		Math3D.AnglesToMatrix(Vector(Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG, 0, 0), mat);
		mat[3] = position;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les « m_iTrafficSeconds » : pour chaque groupe de joueurs hors de la base, s'il manque des voitures autour de
	//! lui, une voiture est posée sur une route, hors de vue, tournée vers lui ; elle part vers une localité située de
	//! l'autre côté, donc elle le croise.
	protected void TickTraffic(array<IEntity> players, int now)
	{
		if (!m_bTraffic || now < m_iNextTrafficTick)
			return;
		m_iNextTrafficTick = now + Math.Max(m_iTrafficSeconds, 5) * 1000;
		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
			return;

		array<vector> groups = {};
		foreach (IEntity player : players)
		{
			vector origin = player.GetOrigin();
			bool known = false;
			foreach (vector leader : groups)
			{
				if (vector.Distance(leader, origin) < 500)
					known = true;
			}
			if (!known)
				groups.Insert(origin);
		}

		// G14 : le front, une fois pour tous les groupes (moins de circulation en territoire rouge)
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		bool followFront = front && front.IsReady();
		if (!m_bFollowTerritory)
			followFront = false;

		foreach (vector group : groups)
		{
			if (CountDrivers() >= m_iMaxDrivers)
				return;
			if (InBase(group, 200))
				continue;	// à la base, on ne fait pas venir la circulation

			int around = 0;
			foreach (SRP_CivRecord record : m_aRecords)
			{
				if (record.m_Vehicle && (record.m_iKind == SRP_ECivKind.DRIVER || record.m_iKind == SRP_ECivKind.TRAFFIC) && vector.Distance(record.m_Vehicle.GetOrigin(), group) < 1500)
					around++;
			}
			int wantedAround = m_iTrafficPerGroup;
			if (followFront && front.IsRedAt(group))
				wantedAround = m_iTrafficPerGroupRed;
			if (around >= wantedAround)
				continue;

			for (int attempt = 0; attempt < 5; attempt++)
			{
				float angle = Math.RandomFloat(0, Math.PI2);
				float distance = Math.RandomFloat(m_fTrafficMinDistance + 100, Math.Max(m_fTrafficMaxDistance, m_fTrafficMinDistance + 200));
				vector spot = group + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
				vector mat[4];
				if (!FindRoadTransform(spot, 250, group, mat))
					continue;
				if (NearestPlayerDistance(mat[3], players) < m_fTrafficMinDistance || InBase(mat[3], 150))
					continue;
				if (SRP_Checkpoint.IsNearActive(mat[3], 1000))
					continue;	// un point de contrôle routier fait venir ses propres voitures : pas d'entassement
				if (SpawnTraffic(mat, group, now))
					break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnTraffic(vector mat[4], vector group, int now)
	{
		ResourceName prefab = PickCharacterPrefab(null);
		if (prefab.IsEmpty() || !HasCivilianRoom())
			return false;	// sans place pour le conducteur, la voiture n'est pas posée

		vector place[4];
		place[0] = mat[0];
		place[1] = mat[1];
		place[2] = mat[2];
		place[3] = mat[3] + vector.Up * 0.3;
		IEntity vehicle = SpawnPrefab(m_aVehiclePrefabs.GetRandomElement(), place);
		if (!vehicle)
			return false;

		vector characterMat[4];
		Math3D.MatrixIdentity4(characterMat);
		characterMat[3] = place[3] + place[0] * 3;

		SRP_CivRecord record = new SRP_CivRecord();
		record.m_iKind = SRP_ECivKind.TRAFFIC;
		record.m_Vehicle = vehicle;
		record.m_vLastPosition = place[3];
		if (!CreateCivilian(record, prefab, characterMat))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
		if (access)
			access.MoveInVehicle(vehicle, ECompartmentType.PILOT);

		// La destination : une localité de l'autre côté des joueurs (elle les croise en route)
		record.m_Destination = PickDestination(place[3], group, null);
		m_aRecords.Insert(record);
		record.m_iNextOrderTick = now + 3000;	// l'ordre de route arrive une fois au volant
		string label = "?";
		if (record.m_Destination)
			label = record.m_Destination.GetZoneName();
		SRP_JournalComponent.Log("CIVILS", string.Format("Circulation : une voiture posée sur la route à %1 m des joueurs, en route vers %2", Math.Round(vector.Distance(place[3], group)), label));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Une localité où aller depuis « here » : jamais la base ni un trajet qui la traverse ; si « through » est donné, de
	//! préférence une localité dans cette direction (au-delà des joueurs)
	protected SRP_CivilZoneComponent PickDestination(vector here, vector through, SRP_CivilZoneComponent except)
	{
		array<SRP_CivilZoneComponent> zones = SRP_CivilZoneComponent.GetZones();
		array<SRP_CivilZoneComponent> beyond = {};
		array<SRP_CivilZoneComponent> others = {};
		vector way = through - here;
		way[1] = 0;
		float wayLength = way.Length();
		foreach (SRP_CivilZoneComponent zone : zones)
		{
			vector center = zone.GetCenter();
			float distance = vector.Distance(center, here);
			if (zone == except || distance < m_fArrivalDistance * 2 || distance > m_fMaxTripDistance)
				continue;
			if (InBase(center, 100) || CrossesBase(here, center, 50))
				continue;
			others.Insert(zone);
			if (through == vector.Zero || wayLength < 1 || distance < wayLength)
				continue;
			vector toZone = center - here;
			toZone[1] = 0;
			if (vector.Dot(toZone, way) / (toZone.Length() * wayLength) > 0.75)
				beyond.Insert(zone);	// à moins de 40° de la direction des joueurs, et plus loin qu'eux
		}
		if (!beyond.IsEmpty())
			return beyond.GetRandomElement();
		if (!others.IsEmpty())
			return others.GetRandomElement();
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected float NearestPlayerDistance(vector position, array<IEntity> players)
	{
		float best = 1000000;
		foreach (IEntity player : players)
		{
			float distance = vector.Distance(position, player.GetOrigin());
			if (distance < best)
				best = distance;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountZone(SRP_CivilZoneComponent zone, int kind)
	{
		int count = 0;
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_Zone == zone && record.m_iKind == kind)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Les piétons (promeneurs, postés). Les conducteurs ont leur propre plafond (m_iMaxDrivers) : avant, les promeneurs
	//! prenaient toutes les places et presque aucune voiture ne partait.
	protected int CountCivilians()
	{
		int count = 0;
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_Character && record.m_iKind != SRP_ECivKind.DRIVER && record.m_iKind != SRP_ECivKind.TRAFFIC)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Commandeur (C4, CO5) : les civils à pied vivants qui peuvent servir d'informateurs (promeneurs, postés ; ni
	//! conducteurs, ni tenus par un point de contrôle), sans autre filtre : le Commandeur choisit (carré rouge, région
	//! active, repos, vue, ni inconscient ni menotté) — SRP_Commander.InformantsTick
	void GetInformantCandidates(notnull array<IEntity> characters)
	{
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (!record || record.m_bCheckpoint)
				continue;
			if (record.m_iKind != SRP_ECivKind.WALKER && record.m_iKind != SRP_ECivKind.POST)
				continue;
			IEntity character = record.m_Character;
			if (!character || character.IsDeleted() || SRP_Utils.IsDead(character))
				continue;
			characters.Insert(character);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsMarkerUsed(IEntity marker)
	{
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_Marker == marker)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de PNJ/véhicules d'un type placés au hasard (sans marqueur) dans une zone
	protected int CountZoneAuto(SRP_CivilZoneComponent zone, int kind)
	{
		int count = 0;
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_Zone == zone && record.m_iKind == kind && !record.m_Marker)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Complète une zone active : promeneurs, postes, parkings, routes. Retourne le budget restant.
	//! G14 : promeneurs, immobiles et voitures qui partent suivent la part du territoire (Scaled) ; les voitures garées,
	//! simple décor, ne changent pas.
	protected int FillZone(SRP_CivilZoneComponent zone, int budget, int now)
	{
		// Les morts récents ne sont pas remplacés tout de suite
		int allowedWalkers = Scaled(zone.m_iWalkers, zone) - zone.m_iDeadCount;

		while (budget > 0 && CountZone(zone, SRP_ECivKind.WALKER) < allowedWalkers && CountCivilians() < m_iMaxCivilians)
		{
			if (!SpawnWalker(zone, now))
				break;
			budget--;
		}

		array<IEntity> markers = {};
		if (zone.m_bPosts && budget > 0)
		{
			zone.GetMarkers("Poste", markers);
			int postCap = Scaled(markers.Count(), zone);	// en rouge, une partie des marqueurs Poste_x reste vide
			foreach (IEntity marker : markers)
			{
				if (budget <= 0 || CountCivilians() >= m_iMaxCivilians || CountZone(zone, SRP_ECivKind.POST) >= postCap)
					break;
				if (IsMarkerUsed(marker))
					continue;
				if (SpawnPost(zone, marker))
					budget--;
			}
			while (budget > 0 && CountZoneAuto(zone, SRP_ECivKind.POST) < Scaled(zone.m_iAutoPosts, zone) && CountCivilians() < m_iMaxCivilians)
			{
				if (!SpawnPost(zone, null))
					break;
				budget--;
			}
		}

		if (zone.m_bParkings && budget > 0)
		{
			zone.GetMarkers("Parking", markers);
			foreach (IEntity marker : markers)
			{
				if (budget <= 0)
					break;
				if (IsMarkerUsed(marker))
					continue;
				if (SpawnParked(zone, marker))
					budget--;
			}
			while (budget > 0 && CountZoneAuto(zone, SRP_ECivKind.PARKED) < zone.m_iAutoParked)
			{
				if (!SpawnParked(zone, null))
					break;
				budget--;
			}
		}

		if (zone.m_bDrivers && budget > 0 && SRP_CivilZoneComponent.GetZones().Count() > 1)
		{
			zone.GetMarkers("Route", markers);
			foreach (IEntity marker : markers)
			{
				if (budget <= 0 || CountDrivers() >= m_iMaxDrivers)
					break;
				if (IsMarkerUsed(marker))
					continue;
				if (SpawnDriver(zone, marker, now))
					budget--;
			}
			while (budget > 0 && CountZoneAuto(zone, SRP_ECivKind.DRIVER) < Scaled(zone.m_iAutoDrivers, zone) && CountDrivers() < m_iMaxDrivers)
			{
				if (!SpawnDriver(zone, null, now))
					break;
				budget--;
			}
		}

		return budget;
	}

	//------------------------------------------------------------------------------------------------
	//! Entretien d'un PNJ : false s'il doit être retiré
	protected bool Maintain(SRP_CivRecord record, array<IEntity> players, int now)
	{
		// Personnage mort ou disparu
		if (record.m_Character)
		{
			if (record.m_Character.IsDeleted())
				record.m_Character = null;
			else if (SRP_Utils.IsDead(record.m_Character))
			{
				if (record.m_Zone)
				{
					record.m_Zone.m_iDeadCount++;
					record.m_Zone.m_iRespawnTick = now + m_iRespawnSeconds * 1000;
				}
				record.m_Character = null;	// le corps reste, on ne le suit plus
				return false;
			}
		}

		switch (record.m_iKind)
		{
			case SRP_ECivKind.WALKER:
				if (!record.m_Character || !record.m_Group)
					return false;
				if (now >= record.m_iNextOrderTick && record.m_iDanger == SRP_ECivDanger.CALME)
					GiveWalkOrder(record, now);
				return true;

			case SRP_ECivKind.POST:
				return record.m_Character != null;

			case SRP_ECivKind.PARKED:
				return record.m_Vehicle && !record.m_Vehicle.IsDeleted();

			case SRP_ECivKind.DRIVER:
				return MaintainDriver(record, players, now);

			case SRP_ECivKind.TRAFFIC:
				return MaintainDriver(record, players, now);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool MaintainDriver(SRP_CivRecord record, array<IEntity> players, int now)
	{
		if (!record.m_Character || !record.m_Group || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return false;

		vector position = record.m_Vehicle.GetOrigin();

		// Loin de tous les joueurs : retirée, elle sera remplacée à sa zone d'origine
		if (NearestPlayerDistance(position, players) > m_fDeactivateDistance)
			return false;

		// Bloquée
		if (vector.Distance(position, record.m_vLastPosition) < 2)
		{
			if (record.m_iStuckSince == 0)
				record.m_iStuckSince = now;
			else if (now - record.m_iStuckSince > m_iStuckSeconds * 1000 && NearestPlayerDistance(position, players) > 200)
				return false;
		}
		else
		{
			record.m_iStuckSince = 0;
			record.m_vLastPosition = position;
		}

		// Arrivée : nouvelle destination
		if (record.m_Destination && vector.Distance(position, record.m_Destination.GetCenter()) < m_fArrivalDistance)
			GiveDriveOrder(record, now);
		else if (now >= record.m_iNextOrderTick)
			GiveDriveOrder(record, now);	// ordre renouvelé de temps en temps, au cas où l'IA l'aurait abandonné

		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Spawns
	//------------------------------------------------------------------------------------------------
	//! Un prefab de PNJ civil (zone = null : liste du gestionnaire), aussi utilisé par les missions
	ResourceName PickCharacterPrefab(SRP_CivilZoneComponent zone)
	{
		array<ResourceName> list = m_aCharacterPrefabs;
		if (zone && zone.m_aCharacterPrefabs && !zone.m_aCharacterPrefabs.IsEmpty())
			list = zone.m_aCharacterPrefabs;
		if (!list || list.IsEmpty())
			return "";
		return list.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	protected bool FindWalkPosition(SRP_CivilZoneComponent zone, out vector position)
	{
		vector center = zone.GetCenter();
		for (int attempt = 0; attempt < 6; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(10, zone.m_fRadius);
			vector target = center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
			if (InBase(target, 20))
				continue;
			if (SCR_WorldTools.FindEmptyTerrainPosition(position, target, 12, 0.5, 2) && !InBase(position, 10))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Emplacement libre pour une voiture (plus d'espace autour), orientation au hasard
	protected bool FindVehicleTransform(SRP_CivilZoneComponent zone, float maxDistance, out vector mat[4])
	{
		vector center = zone.GetCenter();
		for (int attempt = 0; attempt < 8; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(15, maxDistance);
			vector target = center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);

			vector position;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(position, target, 15, 4, 2.5))
				continue;

			vector angles = Vector(Math.RandomFloat(0, 360), 0, 0);
			Math3D.AnglesToMatrix(angles, mat);
			mat[3] = position;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnPrefab(ResourceName prefab, vector mat[4])
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Ambiance civile : prefab invalide " + prefab, LogLevel.ERROR);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnAtPosition(ResourceName prefab, vector position)
	{
		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;
		return SpawnPrefab(prefab, mat);
	}

	//------------------------------------------------------------------------------------------------
	//! Q7 : la place du jeu pour un civil de plus (SRP_CmdCapacity.CivilianRefusal : les civils ne prennent jamais la
	//! place que les 120 soldats ennemis peuvent encore demander) — CreateCivilian, et avant de poser une voiture.
	//! Limite du jeu pas encore portée à cap_limite_jeu (avant SRP_CmdCapacity.Start, front pas prêt, ou clé aiLimit
	//! du serveur qui l'emporte) : seulement la marge sous la limite du jeu (EngineRoom), m_iMaxCivilians borne le reste
	protected bool HasCivilianRoom()
	{
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string roomRefusal;
		int engineLimit = 0;
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
			engineLimit = aiWorld.GetLimitOfActiveAIs();
		if (engineLimit > 0 && engineLimit < capacity.m_iEngineLimit)
		{
			// À 128 : 128 - 10 de marge < 120 soldats gardés, CivilianRefusal refuserait tout civil ; on revient à la
			// règle d'avant le front (plafond des civils), sans jamais pousser le jeu au-delà de sa marge
			if (capacity.EngineRoom() < 1)
				roomRefusal = string.Format("place du jeu : aucune place libre sous la limite du jeu (%1, %2 voulue)", engineLimit, capacity.m_iEngineLimit);
		}
		else
		{
			roomRefusal = capacity.CivilianRefusal(1);
		}
		if (roomRefusal.IsEmpty())
			return true;
		// Au plus une ligne toutes les 10 min : près de la limite, le refus peut revenir à chaque passage
		int nowTick = System.GetTickCount();
		if (m_iRoomLogTick == 0 || nowTick - m_iRoomLogTick > 600000)
		{
			m_iRoomLogTick = nowTick;
			SRP_JournalComponent.Log("CIVILS", "Civils : apparitions suspendues, " + roomRefusal);
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Personnage + groupe IA de la faction civile, IA activée. Q7 : rien sans la place du jeu (HasCivilianRoom) ;
	//! importance du groupe par SRP_CmdCapacity.TagCivilianGroup
	protected bool CreateCivilian(SRP_CivRecord record, ResourceName prefab, vector mat[4])
	{
		if (!HasCivilianRoom())
			return false;

		IEntity character = SpawnPrefab(prefab, mat);
		if (!character)
			return false;

		SCR_AIGroup group = SCR_AIGroup.Cast(SpawnPrefab(m_sGroupPrefab, mat));
		if (!group)
		{
			Print("[SRP] Ambiance civile : prefab de groupe invalide " + m_sGroupPrefab, LogLevel.ERROR);
			SCR_EntityHelper.DeleteEntityAndChildren(character);
			return false;
		}

		if (m_Faction)
			group.SetFaction(m_Faction);
		group.AddAgentFromControlledEntity(character);
		SRP_CmdCapacity.TagCivilianGroup(group);

		AIControlComponent control = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (control)
			control.ActivateAI();

		// CRX : un civil ne tire jamais, n'enquête pas ; un immobile tient sa place
		float holdRadius = -1;
		if (record.m_iKind == SRP_ECivKind.POST)
			holdRadius = 5;
		SRP_CRX.Apply(group, SRP_ECRXRole.CIVIL, vector.Zero, holdRadius);

		record.m_Character = character;
		record.m_Group = group;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool SpawnWalker(SRP_CivilZoneComponent zone, int now)
	{
		ResourceName prefab = PickCharacterPrefab(zone);
		if (prefab.IsEmpty())
			return false;

		vector position;
		if (!FindWalkPosition(zone, position))
		{
			if (!m_bWarnedCharacters)
			{
				m_bWarnedCharacters = true;
				Print("[SRP] Ambiance civile : aucune position libre trouvée dans " + zone.GetZoneName() + " (rayon trop grand, zone en mer ?)", LogLevel.WARNING);
			}
			return false;
		}

		vector mat[4];
		Math3D.MatrixIdentity4(mat);
		mat[3] = position;

		SRP_CivRecord record = new SRP_CivRecord();
		record.m_iKind = SRP_ECivKind.WALKER;
		record.m_Zone = zone;
		if (!CreateCivilian(record, prefab, mat))
			return false;

		m_aRecords.Insert(record);
		record.m_iNextOrderTick = now + Math.RandomInt(3000, 15000);
		if (CountZone(zone, SRP_ECivKind.WALKER) == 1)
			SRP_JournalComponent.Log("CIVILS", string.Format("%1 : premier promeneur en %2", zone.GetZoneName(), position.ToString()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! PNJ immobile : sur un marqueur Poste_x, ou au hasard dans la zone si marker est null
	protected bool SpawnPost(SRP_CivilZoneComponent zone, IEntity marker)
	{
		ResourceName prefab = PickCharacterPrefab(zone);
		if (prefab.IsEmpty())
			return false;

		vector mat[4];
		if (marker)
		{
			marker.GetWorldTransform(mat);
		}
		else
		{
			vector position;
			if (!FindWalkPosition(zone, position))
				return false;
			vector angles = Vector(Math.RandomFloat(0, 360), 0, 0);
			Math3D.AnglesToMatrix(angles, mat);
			mat[3] = position;
		}

		SRP_CivRecord record = new SRP_CivRecord();
		record.m_iKind = SRP_ECivKind.POST;
		record.m_Zone = zone;
		record.m_Marker = marker;
		if (!CreateCivilian(record, prefab, mat))
			return false;

		m_aRecords.Insert(record);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Voiture garée : sur un marqueur Parking_x, ou sur un emplacement libre si marker est null
	protected bool SpawnParked(SRP_CivilZoneComponent zone, IEntity marker)
	{
		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
			return false;

		vector mat[4];
		if (marker)
			marker.GetWorldTransform(mat);
		else if (!FindVehicleTransform(zone, zone.m_fRadius, mat))
			return false;
		mat[3] = mat[3] + vector.Up * 0.3;

		IEntity vehicle = SpawnPrefab(m_aVehiclePrefabs.GetRandomElement(), mat);
		if (!vehicle)
			return false;

		SRP_CivRecord record = new SRP_CivRecord();
		record.m_iKind = SRP_ECivKind.PARKED;
		record.m_Zone = zone;
		record.m_Vehicle = vehicle;
		record.m_Marker = marker;
		m_aRecords.Insert(record);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Voiture en circulation : sur un marqueur Route_x, ou près du centre si marker est null
	protected bool SpawnDriver(SRP_CivilZoneComponent zone, IEntity marker, int now)
	{
		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
			return false;
		ResourceName prefab = PickCharacterPrefab(zone);
		if (prefab.IsEmpty() || !HasCivilianRoom())
			return false;	// sans place pour le conducteur, la voiture n'est pas posée

		// Sans marqueur : sur la route la plus proche du centre (elle part tout de suite), sinon un emplacement libre
		vector mat[4];
		if (marker)
			marker.GetWorldTransform(mat);
		else if (!FindRoadTransform(zone.GetCenter(), 200, vector.Zero, mat) && !FindVehicleTransform(zone, Math.Min(zone.m_fRadius, 120), mat))
			return false;
		if (InBase(mat[3], 50))
			return false;
		mat[3] = mat[3] + vector.Up * 0.3;

		IEntity vehicle = SpawnPrefab(m_aVehiclePrefabs.GetRandomElement(), mat);
		if (!vehicle)
			return false;

		// Le conducteur apparaît à côté, puis est placé au volant
		vector characterMat[4];
		Math3D.MatrixIdentity4(characterMat);
		characterMat[3] = mat[3] + mat[0] * 3;

		SRP_CivRecord record = new SRP_CivRecord();
		record.m_iKind = SRP_ECivKind.DRIVER;
		record.m_Zone = zone;
		record.m_Vehicle = vehicle;
		record.m_Marker = marker;
		record.m_vLastPosition = mat[3];
		if (!CreateCivilian(record, prefab, characterMat))
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			return false;
		}

		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
		if (access)
			access.MoveInVehicle(vehicle, ECompartmentType.PILOT);

		m_aRecords.Insert(record);
		record.m_iNextOrderTick = now + 3000;	// l'ordre de route arrive au tick suivant, une fois au volant
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Ordres
	//------------------------------------------------------------------------------------------------
	//! walk : limite la vitesse au pas pour les déplacements ordinaires (les réactions de danger restent libres)
	protected void ReplaceWaypoint(SRP_CivRecord record, vector destination, bool walk, bool run = false)
	{
		if (!record.m_Group)
			return;

		if (record.m_Waypoint)
		{
			AIWaypoint old = AIWaypoint.Cast(record.m_Waypoint);
			if (old)
			{
				record.m_Group.RemoveWaypointFromGroup(old);
				SCR_EntityHelper.DeleteEntityAndChildren(old);
			}
			record.m_Waypoint = null;
		}

		IEntity entity = SpawnAtPosition(m_sMoveWaypointPrefab, destination);
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint)
		{
			Print("[SRP] Ambiance civile : prefab de point de passage invalide " + m_sMoveWaypointPrefab, LogLevel.ERROR);
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			return;
		}

		// Le réglage doit être posé avant que le point soit donné au groupe. Cause GROUP_GOAL : la limite
		// vaut pour les déplacements ordinaires, pas pour les comportements de danger (DANGER_LOW et au-delà)
		SCR_AIWaypoint scripted = SCR_AIWaypoint.Cast(waypoint);
		if (walk && scripted)
			scripted.AddSetting(SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.WAYPOINT, SCR_EAIBehaviorCause.GROUP_GOAL, EMovementType.WALK));
		else if (run && scripted)
			scripted.AddSetting(SCR_AICharacterMovementSpeedSetting.Create(SCR_EAISettingOrigin.WAYPOINT, SCR_EAIBehaviorCause.GROUP_GOAL, EMovementType.SPRINT));

		record.m_Group.AddWaypointToGroup(waypoint);
		record.m_Waypoint = entity;
	}

	//------------------------------------------------------------------------------------------------
	// Réflexes de danger
	//------------------------------------------------------------------------------------------------
	protected void TickDanger()
	{
		if (!m_bDangerReactions)
			return;
		int now = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();

		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_iKind != SRP_ECivKind.WALKER && record.m_iKind != SRP_ECivKind.POST)
				continue;
			if (!record.m_Character || record.m_Character.IsDeleted() || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (SRP_Utils.IsDead(record.m_Character))
				continue;
			vector position = record.m_Character.GetOrigin();
			if (SRP_Utils.NearestPlayerDistance(position, players) > 600)
				continue;	// loin de tout joueur : rien à réagir

			// 1. Nouveaux dangers perçus
			int severity = 0;		// 1 tir entendu, 2 balle proche ou explosion
			vector threat = vector.Zero;
			ReadDanger(record, position, severity, threat);

			// 2. Transitions
			if (severity == 2 && record.m_iDanger != SRP_ECivDanger.PRONE)
			{
				record.m_iDanger = SRP_ECivDanger.PRONE;
				record.m_iDangerUntil = now + Math.RandomInt(6000, 12000);
				record.m_vThreat = threat;
				RememberHome(record, position);
				SetStance(record, ECharacterStanceChange.STANCECHANGE_TOPRONE);
				continue;
			}
			if (severity == 1 && record.m_iDanger == SRP_ECivDanger.CALME)
			{
				record.m_vThreat = threat;
				RememberHome(record, position);
				StartFlee(record, position, now);
				continue;
			}
			if (severity >= 1 && record.m_iDanger == SRP_ECivDanger.CACHE)
			{
				// Ça recommence : on reste caché plus longtemps, et on refuit si c'est tout près
				record.m_iDangerUntil = now + m_iCalmSeconds * 1000;
				if (vector.Distance(threat, position) < 60)
					StartFlee(record, position, now);
				continue;
			}

			// 3. Fin des états
			if (record.m_iDanger == SRP_ECivDanger.PRONE)
			{
				SetStance(record, ECharacterStanceChange.STANCECHANGE_TOPRONE);
				if (now >= record.m_iDangerUntil)
					StartFlee(record, position, now);
			}
			else if (record.m_iDanger == SRP_ECivDanger.FUITE)
			{
				bool arrived = record.m_Waypoint && vector.DistanceXZ(position, record.m_Waypoint.GetOrigin()) < 6;
				if (arrived || now >= record.m_iDangerUntil)
				{
					record.m_iDanger = SRP_ECivDanger.CACHE;
					record.m_iDangerUntil = now + m_iCalmSeconds * 1000;
					SRP_CRX.SetGroupSpeed(record.m_Group, EMovementType.WALK);
					SetStance(record, ECharacterStanceChange.STANCECHANGE_TOCROUCH);
				}
			}
			else if (record.m_iDanger == SRP_ECivDanger.CACHE)
			{
				SetStance(record, ECharacterStanceChange.STANCECHANGE_TOCROUCH);
				if (now >= record.m_iDangerUntil)
					Calm(record, now);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le danger reçu depuis le dernier passage (posé par OnDangerEvent), puis effacé
	protected void ReadDanger(SRP_CivRecord record, vector position, out int severity, out vector threat)
	{
		severity = record.m_iPendingSeverity;
		threat = record.m_vPendingThreat;
		record.m_iPendingSeverity = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Appelé par le crochet SCR_AIConfigComponent.PerformDangerReaction pour chaque danger reçu par une IA :
	//! si c'est un de nos civils, on classe le danger. Tir : position, direction et cylindre de passage comme
	//! la réaction vanilla des soldats ; explosion et impact : par leur nom de type.
	void OnDangerEvent(IEntity character, AIDangerEvent danger)
	{
		if (!m_bDangerReactions || !character || !danger)
			return;
		SRP_CivRecord record;
		foreach (SRP_CivRecord candidate : m_aRecords)
		{
			if (candidate.m_Character == character)
			{
				record = candidate;
				break;
			}
		}
		if (!record || (record.m_iKind != SRP_ECivKind.WALKER && record.m_iKind != SRP_ECivKind.POST))
			return;

		vector position = character.GetOrigin();
		int severity = 0;
		vector threat = danger.GetPosition();

		AIDangerEventWeaponFire fire = AIDangerEventWeaponFire.Cast(danger);
		if (fire)
		{
			vector shotPos = fire.GetPosition();
			float distance = vector.Distance(position, shotPos);
			bool flyby = Math3D.IntersectionPointCylinder(position, shotPos, fire.GetDirection(), m_fProneDistance);
			float audible = m_fFleeHearingDistance;
			if (fire.IsSuppressed())
				audible = Math.Min(audible, 100);
			if (flyby && distance < 400)
				severity = 2;
			else if (distance <= audible)
				severity = 1;
			threat = shotPos;
		}
		else
		{
			string kind = typename.EnumToString(EAIDangerEventType, danger.GetDangerType());
			float distance = vector.Distance(position, threat);
			if ((kind.Contains("Explosion") || kind.Contains("Grenade")) && distance <= 40)
				severity = 2;
			else if (kind.Contains("ProjectileHit") && distance <= m_fProneDistance)
				severity = 2;
			else if (kind.Contains("DamageTaken") || kind.Contains("Bleeding"))
				severity = 2;
		}

		if (severity > record.m_iPendingSeverity)
		{
			record.m_iPendingSeverity = severity;
			record.m_vPendingThreat = threat;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RememberHome(SRP_CivRecord record, vector position)
	{
		if (record.m_vHome == vector.Zero)
			record.m_vHome = position;
	}

	//------------------------------------------------------------------------------------------------
	//! Fuir : à l'opposé du danger, en sprint, derrière un bâtiment si l'on en trouve un dans la bonne direction
	protected void StartFlee(SRP_CivRecord record, vector position, int now)
	{
		vector away = position - record.m_vThreat;
		away[1] = 0;
		if (away.Length() < 1)
			away = Vector(Math.RandomFloat(-1, 1), 0, Math.RandomFloat(-1, 1));
		away.Normalize();

		// Un peu d'écart pour ne pas courir tout droit
		float spread = Math.RandomFloat(-0.6, 0.6);
		vector direction = Vector(away[0] * Math.Cos(spread) - away[2] * Math.Sin(spread), 0, away[0] * Math.Sin(spread) + away[2] * Math.Cos(spread));
		vector destination = position + direction * m_fFleeDistance;

		// Un bâtiment dans cette direction : on se met derrière
		vector cover;
		if (FindCover(position, direction, cover))
			destination = cover;

		destination[1] = GetGame().GetWorld().GetSurfaceY(destination[0], destination[2]);
		vector free;
		if (SCR_WorldTools.FindEmptyTerrainPosition(free, destination, 12, 1, 2))
			destination = free;

		// Pas de refuge dans la base : on fuit du côté opposé à elle
		if (InBase(destination, 30) || CrossesBase(position, destination, 20))
		{
			vector outward = position - m_vBase;
			outward[1] = 0;
			if (outward.Length() > 1)
			{
				outward.Normalize();
				destination = position + outward * m_fFleeDistance;
				destination[1] = GetGame().GetWorld().GetSurfaceY(destination[0], destination[2]);
			}
		}

		record.m_iDanger = SRP_ECivDanger.FUITE;
		record.m_iDangerUntil = now + 45000;
		SetStance(record, ECharacterStanceChange.STANCECHANGE_TOERECTED);
		SRP_CRX.SetGroupSpeed(record.m_Group, EMovementType.SPRINT);
		ReplaceWaypoint(record, destination, false);
		record.m_iNextOrderTick = now + 10 * 60 * 1000;	// pas d'ordre de promenade pendant ce temps
	}

	protected IEntity m_CoverBest;
	protected float m_fCoverBestScore;
	protected vector m_vCoverFrom;
	protected vector m_vCoverDirection;

	//------------------------------------------------------------------------------------------------
	//! Cherche un bâtiment à moins de 80 m dans la direction de fuite, et retourne un point derrière lui
	protected bool FindCover(vector position, vector direction, out vector cover)
	{
		m_CoverBest = null;
		m_fCoverBestScore = 0;
		m_vCoverFrom = position;
		m_vCoverDirection = direction;
		GetGame().GetWorld().QueryEntitiesBySphere(position, 80, CoverCandidate, null, EQueryEntitiesFlags.STATIC);
		if (!m_CoverBest)
			return false;
		vector center = m_CoverBest.GetOrigin();
		vector toBuilding = center - position;
		toBuilding[1] = 0;
		toBuilding.Normalize();
		cover = center + toBuilding * 8;	// derrière le bâtiment, vu du danger
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CoverCandidate(IEntity entity)
	{
		if (!entity)
			return true;
		// Un bâtiment : une entité statique d'une certaine taille, ni personnage ni véhicule
		if (ChimeraCharacter.Cast(entity) || Vehicle.Cast(entity))
			return true;
		if (!entity.GetVObject())
			return true;
		vector mins;
		vector maxs;
		entity.GetBounds(mins, maxs);
		vector size = maxs - mins;
		if (size[0] < 4 || size[2] < 4 || size[1] < 2.5)
			return true;	// trop petit pour être un bâtiment
		vector to = entity.GetOrigin() - m_vCoverFrom;
		to[1] = 0;
		float distance = to.Length();
		if (distance < 1)
			return true;
		to.Normalize();
		float facing = vector.Dot(to, m_vCoverDirection);
		if (facing < 0.2)
			return true;	// pas dans la direction de fuite
		float score = facing * 100 - distance;
		if (!m_CoverBest || score > m_fCoverBestScore)
		{
			m_CoverBest = entity;
			m_fCoverBestScore = score;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetStance(SRP_CivRecord record, ECharacterStanceChange stance)
	{
		if (!record.m_Character)
			return;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(record.m_Character.FindComponent(CharacterControllerComponent));
		if (controller)
			controller.SetStanceChange(stance);
	}

	//------------------------------------------------------------------------------------------------
	//! Retour au calme : debout, au pas, et les postés rentrent à leur place
	protected void Calm(SRP_CivRecord record, int now)
	{
		record.m_iDanger = SRP_ECivDanger.CALME;
		SetStance(record, ECharacterStanceChange.STANCECHANGE_TOERECTED);
		SRP_CRX.SetGroupSpeed(record.m_Group, EMovementType.WALK);
		if (record.m_iKind == SRP_ECivKind.POST && record.m_vHome != vector.Zero)
			ReplaceWaypoint(record, record.m_vHome, true);
		else
			record.m_iNextOrderTick = now + Math.RandomInt(5000, 20000);
	}

	//------------------------------------------------------------------------------------------------
	protected void GiveWalkOrder(SRP_CivRecord record, int now)
	{
		// Une promenade ne traverse jamais la base : on retire un autre point si le trajet en ligne droite y passe
		vector destination;
		bool found = false;
		for (int attempt = 0; attempt < 4 && !found; attempt++)
		{
			if (!record.m_Zone || !FindWalkPosition(record.m_Zone, destination))
				break;
			found = !record.m_Character || !CrossesBase(record.m_Character.GetOrigin(), destination, 20);
		}
		if (found)
			ReplaceWaypoint(record, destination, m_bWalk);
		record.m_iNextOrderTick = now + Math.RandomInt(m_iWanderSeconds * 700, m_iWanderSeconds * 1300);
	}

	//------------------------------------------------------------------------------------------------
	protected void GiveDriveOrder(SRP_CivRecord record, int now)
	{
		// Une voiture de la circulation part avec sa destination déjà choisie (au-delà des joueurs) ; ensuite, comme les
		// autres, elle va de localité en localité, sans jamais viser la base ni la traverser
		vector here = record.m_Vehicle.GetOrigin();
		bool arrived = record.m_Destination && vector.Distance(here, record.m_Destination.GetCenter()) < m_fArrivalDistance * 2;
		if (!record.m_Destination || arrived || InBase(record.m_Destination.GetCenter(), 100))
			record.m_Destination = PickDestination(here, vector.Zero, record.m_Destination);
		if (!record.m_Destination)
		{
			record.m_iNextOrderTick = now + 60000;
			return;
		}

		ReplaceWaypoint(record, record.m_Destination.GetCenter(), false);
		record.m_iNextOrderTick = now + 8 * 60 * 1000;
	}

	//------------------------------------------------------------------------------------------------
	// Point de contrôle routier (SRP_Checkpoint) : il emprunte des voitures à la circulation, les conduit, les rend.
	// Les conducteurs se rendent (mains levées) et sont menottés par ACE Captives (SCR_CharacterControllerComponent.
	// ACE_Captives_SetSurrender / ACE_Captives_SetCaptive) ; « repartir » passe par un ordre d'embarquement IA.
	//------------------------------------------------------------------------------------------------
	//! Un point SUR une route près de « around », pour y poser un point de contrôle
	bool FindRoadPoint(vector around, float reach, vector toward, out vector position)
	{
		vector mat[4];
		if (!FindRoadTransform(around, reach, toward, mat))
			return false;
		position = mat[3];
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsInBase(vector position, float margin)
	{
		return InBase(position, margin);
	}

	//------------------------------------------------------------------------------------------------
	//! Les voitures civiles en route à moins de « radius » mètres du point ET qui s'en rapprochent (produit scalaire
	//! vitesse / direction du point) passent sous son contrôle. Une voiture qui s'éloigne, ou immobile, est ignorée.
	void CheckpointAdopt(vector point, float radius, array<SRP_CivRecord> adopted)
	{
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_bCheckpoint || record.m_bCheckpointDone)
				continue;
			if (record.m_iKind != SRP_ECivKind.DRIVER && record.m_iKind != SRP_ECivKind.TRAFFIC)
				continue;
			if (!record.m_Vehicle || record.m_Vehicle.IsDeleted() || !record.m_Character || SRP_Utils.IsDead(record.m_Character))
				continue;
			vector position = record.m_Vehicle.GetOrigin();
			if (vector.Distance(position, point) > radius)
				continue;
			Physics physics = record.m_Vehicle.GetPhysics();
			if (!physics)
				continue;
			vector velocity = physics.GetVelocity();
			velocity[1] = 0;
			vector toPoint = point - position;
			toPoint[1] = 0;
			if (velocity.Length() < 0.5 || vector.Dot(velocity, toPoint) <= 0)
				continue;
			record.m_bCheckpoint = true;
			adopted.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! L'axe de la route au point (unitaire, à plat), toujours le même pour un même point : il sépare les deux sens
	bool FindRoadAxis(vector around, float reach, out vector axis)
	{
		vector mat[4];
		if (!FindRoadTransform(around, reach, around + Vector(1000, 0, 0), mat))
			return false;
		vector heading = mat[2];
		heading[1] = 0;
		if (heading.Length() < 0.5)
			return false;
		heading.Normalize();
		axis = heading;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Une voiture posée sur une route à 450-800 m du point, hors de vue, qui roule vers une localité située au-delà
	SRP_CivRecord CheckpointSpawnCar(vector point, array<IEntity> players, int now)
	{
		if (!m_aVehiclePrefabs || m_aVehiclePrefabs.IsEmpty())
			return null;

		for (int attempt = 0; attempt < 6; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(450, 800);
			vector spot = point + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			vector mat[4];
			if (!FindRoadTransform(spot, 250, point, mat))
				continue;
			if (NearestPlayerDistance(mat[3], players) < 350 || InBase(mat[3], 150))
				continue;
			int before = m_aRecords.Count();
			if (!SpawnTraffic(mat, point, now) || m_aRecords.Count() <= before)
				continue;
			SRP_CivRecord record = m_aRecords[m_aRecords.Count() - 1];
			record.m_bCheckpoint = true;
			return record;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	void CheckpointDriveTo(SRP_CivRecord record, vector position)
	{
		SetHandBrake(record, false);
		ReplaceWaypoint(record, position, false);
	}

	//------------------------------------------------------------------------------------------------
	//! À l'arrêt : plus d'ordre de route, frein à main
	void CheckpointStop(SRP_CivRecord record)
	{
		ClearWaypoint(record);
		SetHandBrake(record, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Le conducteur repart vers sa destination et retourne à la circulation ordinaire
	void CheckpointGo(SRP_CivRecord record, int now, bool wasOut)
	{
		if (wasOut && record.m_Character && record.m_Vehicle)
		{
			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
			if (access && !access.IsInCompartment())
				access.MoveInVehicle(record.m_Vehicle, ECompartmentType.PILOT);
		}
		SetHandBrake(record, false);
		record.m_bCheckpoint = false;
		record.m_bCheckpointDone = true;
		record.m_iStuckSince = 0;
		if (record.m_Vehicle)
			GiveDriveOrder(record, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Elle roule vers sa destination sans s'arrêter (barrage forcé), toujours suivie par le point de contrôle
	void CheckpointDriveOn(SRP_CivRecord record, int now)
	{
		SetHandBrake(record, false);
		if (record.m_Vehicle && record.m_Group)
			GiveDriveOrder(record, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Rendue à la circulation sans autre ordre (elle garde sa route)
	void CheckpointRelease(SRP_CivRecord record)
	{
		SetHandBrake(record, false);
		record.m_bCheckpoint = false;
		record.m_bCheckpointDone = true;
		record.m_iStuckSince = 0;
	}

	//------------------------------------------------------------------------------------------------
	void CheckpointGetOut(SRP_CivRecord record)
	{
		if (!record.m_Character)
			return;
		ClearWaypoint(record);
		SetHandBrake(record, true);
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
		if (access && access.IsInCompartment())
			access.GetOutVehicle(EGetOutType.ANIMATED, -1, ECloseDoorAfterActions.LEAVE_OPEN, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Il détale à pied
	void CheckpointRunTo(SRP_CivRecord record, vector target)
	{
		ReplaceWaypoint(record, target, false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Arrêté : à plat ventre, sans ordre
	void CheckpointLieDown(SRP_CivRecord record)
	{
		ClearWaypoint(record);
		SetStance(record, ECharacterStanceChange.STANCECHANGE_TOPRONE);
	}

	//------------------------------------------------------------------------------------------------
	//! Le conducteur s'est échappé : on ne garde que son véhicule
	void CheckpointForgetDriver(SRP_CivRecord record)
	{
		ClearWaypoint(record);
		if (record.m_Character && !record.m_Character.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Character);
		if (record.m_Group && !record.m_Group.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Group);
		record.m_Character = null;
		record.m_Group = null;
	}

	//------------------------------------------------------------------------------------------------
	//! L'ennemi en civil se découvre : un pistolet, et il passe dans le camp ennemi (ce n'est plus un civil)
	void CheckpointMakeHostile(SRP_CivRecord record, string factionKey, ResourceName pistol, ResourceName magazine)
	{
		if (!record.m_Character || !record.m_Group)
			return;

		SCR_InventoryStorageManagerComponent inventory = SCR_InventoryStorageManagerComponent.Cast(record.m_Character.FindComponent(SCR_InventoryStorageManagerComponent));
		if (inventory)
		{
			if (!pistol.IsEmpty())
				inventory.TrySpawnPrefabToStorage(pistol, null, -1, EStoragePurpose.PURPOSE_WEAPON_PROXY);
			if (!magazine.IsEmpty())
			{
				inventory.TrySpawnPrefabToStorage(magazine, null, -1, EStoragePurpose.PURPOSE_DEPOSIT);
				inventory.TrySpawnPrefabToStorage(magazine, null, -1, EStoragePurpose.PURPOSE_DEPOSIT);
			}
		}

		FactionManager factions = GetGame().GetFactionManager();
		Faction faction;
		if (factions)
			faction = factions.GetFactionByKey(factionKey);
		if (!faction)
		{
			Print("[SRP] Point de contrôle : faction ennemie introuvable : " + factionKey, LogLevel.WARNING);
			return;
		}
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(record.m_Character.FindComponent(FactionAffiliationComponent));
		if (affiliation)
			affiliation.SetAffiliatedFaction(faction);
		record.m_Group.SetFaction(faction);
		SRP_CRX.Apply(record.m_Group, SRP_ECRXRole.ASSAUT, vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire la fiche. keepVehicle : le véhicule (épave, prise) reste dans le monde
	void CheckpointDiscard(SRP_CivRecord record, bool keepVehicle)
	{
		if (!record)
			return;
		SetHandBrake(record, true);
		if (keepVehicle)
			record.m_Vehicle = null;
		if (record.m_Character && SRP_Utils.IsDead(record.m_Character))
			record.m_Character = null;	// le corps reste
		int index = m_aRecords.Find(record);
		Remove(record, false);
		if (index >= 0)
			m_aRecords.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Mains levées (ACE Captives, compartiment d'aide « surrender ») : il s'arrête et reste sur place ; false = il
	//! baisse les mains. Seulement vivant (pas inconscient) et hors de son véhicule. true = c'est fait (ou déjà fait)
	bool CheckpointSurrender(SRP_CivRecord record, bool surrender)
	{
		if (!record.m_Character || record.m_Character.IsDeleted())
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(record.m_Character.FindComponent(SCR_CharacterControllerComponent));
		if (!controller)
			return false;
		if (controller.ACE_Captives_HasSurrendered() == surrender)
			return true;
		if (surrender)
		{
			if (controller.GetLifeState() != ECharacterLifeState.ALIVE)
				return false;
			SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
			if (access && access.IsInCompartment())
				return false;	// encore dans sa voiture : on réessaiera
			ClearWaypoint(record);
		}
		controller.ACE_Captives_SetSurrender(surrender);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool CheckpointHasSurrendered(SRP_CivRecord record)
	{
		if (!record.m_Character || record.m_Character.IsDeleted())
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(record.m_Character.FindComponent(SCR_CharacterControllerComponent));
		return controller && controller.ACE_Captives_HasSurrendered();
	}

	//------------------------------------------------------------------------------------------------
	//! Menotté (ACE Captives : à genoux, mains liées, il ne bouge plus) ; sans contrôleur ACE, à plat ventre
	void CheckpointCaptive(SRP_CivRecord record, bool captive)
	{
		if (!record.m_Character || record.m_Character.IsDeleted())
			return;
		if (captive)
			ClearWaypoint(record);
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(record.m_Character.FindComponent(SCR_CharacterControllerComponent));
		if (!controller)
		{
			if (captive)
				CheckpointLieDown(record);
			else
				SetStance(record, ECharacterStanceChange.STANCECHANGE_TOERECTED);
			return;
		}
		if (controller.ACE_Captives_IsCaptive() == captive)
			return;
		controller.ACE_Captives_SetCaptive(captive);
	}

	//------------------------------------------------------------------------------------------------
	//! Menotté, par le menu ou par les serflex d'un joueur
	bool CheckpointIsCaptive(SRP_CivRecord record)
	{
		if (!record.m_Character || record.m_Character.IsDeleted())
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(record.m_Character.FindComponent(SCR_CharacterControllerComponent));
		return controller && controller.ACE_Captives_IsCaptive();
	}

	//------------------------------------------------------------------------------------------------
	//! Blessé (dégâts reçus) ou inconscient : un fuyard dans cet état s'arrête
	bool CheckpointIsHurt(SRP_CivRecord record)
	{
		if (!record.m_Character || record.m_Character.IsDeleted())
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(record.m_Character.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.GetLifeState() == ECharacterLifeState.INCAPACITATED)
			return true;
		DamageManagerComponent damage = DamageManagerComponent.Cast(record.m_Character.FindComponent(DamageManagerComponent));
		return damage && damage.GetHealthScaled() < 0.97;
	}

	//------------------------------------------------------------------------------------------------
	//! Le conducteur est-il dans SON véhicule ?
	bool CheckpointIsAtWheel(SRP_CivRecord record)
	{
		if (!record.m_Character || record.m_Character.IsDeleted() || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return false;
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
		if (!access || !access.IsInCompartment())
			return false;
		return CompartmentAccessComponent.GetVehicleIn(record.m_Character) == record.m_Vehicle;
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre d'embarquement IA : point de passage « GetIn » du jeu posé sur le véhicule (SCR_BoardingEntityWaypoint,
	//! siège conducteur seulement). Sans prefab valide, le conducteur est placé au volant d'office.
	void CheckpointAskGetIn(SRP_CivRecord record, ResourceName prefab)
	{
		if (!record.m_Group || record.m_Group.IsDeleted() || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return;
		ClearWaypoint(record);
		SetHandBrake(record, true);
		IEntity entity;
		if (!prefab.IsEmpty())
			entity = SpawnAtPosition(prefab, record.m_Vehicle.GetOrigin());
		AIWaypoint waypoint = AIWaypoint.Cast(entity);
		if (!waypoint)
		{
			Print("[SRP] Point de contrôle : prefab de point de passage « GetIn » invalide " + prefab, LogLevel.WARNING);
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
			CheckpointForceIn(record);
			return;
		}
		SCR_BoardingEntityWaypoint boarding = SCR_BoardingEntityWaypoint.Cast(waypoint);
		if (boarding)
			boarding.SetEntity(record.m_Vehicle);
		SCR_BoardingWaypoint allowance = SCR_BoardingWaypoint.Cast(waypoint);
		if (allowance)
			allowance.SetAllowance(true, false, false);
		record.m_Group.AddWaypointToGroup(waypoint);
		record.m_Waypoint = entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Placé au volant d'office (ancienne méthode, repli quand l'IA ne remonte pas)
	void CheckpointForceIn(SRP_CivRecord record)
	{
		if (!record.m_Character || record.m_Character.IsDeleted() || !record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return;
		ClearWaypoint(record);
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(record.m_Character.FindComponent(SCR_CompartmentAccessComponent));
		if (access && !access.IsInCompartment())
			access.MoveInVehicle(record.m_Vehicle, ECompartmentType.PILOT);
	}

	//------------------------------------------------------------------------------------------------
	//! Ordre de route AU-DELÀ du point de contrôle : d'abord un point de la route près de « beyond » (dans le
	//! prolongement de son sens de marche, pour ne pas faire demi-tour), puis, une minute plus tard, la circulation
	//! ordinaire reprend vers une localité choisie de préférence dans cette direction
	void CheckpointGoBeyond(SRP_CivRecord record, vector beyond, int now)
	{
		if (!record.m_Vehicle || record.m_Vehicle.IsDeleted() || !record.m_Group || record.m_Group.IsDeleted())
			return;
		SetHandBrake(record, false);
		record.m_iStuckSince = 0;
		vector here = record.m_Vehicle.GetOrigin();
		SRP_CivilZoneComponent destination = PickDestination(here, beyond, null);
		if (destination)
			record.m_Destination = destination;
		vector target;
		if (FindRoadPoint(beyond, 300, vector.Zero, target))
		{
			ReplaceWaypoint(record, target, false);
			record.m_iNextOrderTick = now + 60000;
			return;
		}
		GiveDriveOrder(record, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Le nom d'une autre localité (« vient de… » sur les papiers)
	string CheckpointOtherPlace(string except)
	{
		array<string> names = {};
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
		{
			string name = zone.GetZoneName();
			if (!name.IsEmpty() && name != except)
				names.Insert(name);
		}
		if (names.IsEmpty())
			return "la côte";
		return names.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearWaypoint(SRP_CivRecord record)
	{
		if (!record.m_Waypoint)
			return;
		AIWaypoint old = AIWaypoint.Cast(record.m_Waypoint);
		if (old)
		{
			if (record.m_Group && !record.m_Group.IsDeleted())
				record.m_Group.RemoveWaypointFromGroup(old);
			SCR_EntityHelper.DeleteEntityAndChildren(old);
		}
		record.m_Waypoint = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetHandBrake(SRP_CivRecord record, bool on)
	{
		if (!record.m_Vehicle || record.m_Vehicle.IsDeleted())
			return;
		CarControllerComponent controller = CarControllerComponent.Cast(record.m_Vehicle.FindComponent(CarControllerComponent));
		if (controller)
			controller.SetPersistentHandBrake(on);
	}

	//------------------------------------------------------------------------------------------------
	// Nettoyage
	//------------------------------------------------------------------------------------------------
	//! Un joueur est-il dedans ou tout près ? (on ne retire jamais un véhicule sous un joueur)
	protected bool IsNearPlayer(IEntity entity, array<IEntity> players, float distance)
	{
		if (!entity)
			return false;
		return NearestPlayerDistance(entity.GetOrigin(), players) < distance;
	}

	//------------------------------------------------------------------------------------------------
	protected void Remove(SRP_CivRecord record, bool keepVehicleIfNearPlayer, array<IEntity> players = null)
	{
		if (record.m_Waypoint && !record.m_Waypoint.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Waypoint);

		if (record.m_Character && !record.m_Character.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Character);
		if (record.m_Group && !record.m_Group.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(record.m_Group);

		if (record.m_Vehicle && !record.m_Vehicle.IsDeleted())
		{
			bool keep = keepVehicleIfNearPlayer && players && IsNearPlayer(record.m_Vehicle, players, 150);
			if (!keep)
				SCR_EntityHelper.DeleteEntityAndChildren(record.m_Vehicle);
		}

		record.m_Character = null;
		record.m_Group = null;
		record.m_Vehicle = null;
		record.m_Waypoint = null;
		record.m_Marker = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearZone(SRP_CivilZoneComponent zone, array<IEntity> players)
	{
		for (int i = m_aRecords.Count() - 1; i >= 0; i--)
		{
			SRP_CivRecord record = m_aRecords[i];
			if (record.m_Zone != zone || record.m_iKind == SRP_ECivKind.DRIVER)
				continue;	// les voitures en circulation vivent leur vie tant qu'un joueur est proche
			Remove(record, true, players);
			m_aRecords.Remove(i);
		}
		zone.m_iDeadCount = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : tout retirer, les zones se remplissent à nouveau au tick suivant
	void Reset()
	{
		for (int i = m_aRecords.Count() - 1; i >= 0; i--)
			Remove(m_aRecords[i], false);
		m_aRecords.Clear();
		foreach (SRP_CivilZoneComponent zone : SRP_CivilZoneComponent.GetZones())
			zone.m_iDeadCount = 0;
		SRP_JournalComponent.Log("CIVILS", "Ambiance civile réinitialisée");
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		array<SRP_CivilZoneComponent> zones = SRP_CivilZoneComponent.GetZones();
		int active = 0;
		foreach (SRP_CivilZoneComponent zone : zones)
		{
			if (zone.m_bActive)
				active++;
		}

		string text = string.Format("Civils : %1 PNJ vivant(s), %2 véhicule(s), %3 zone(s) dont %4 active(s)", CountCivilians(), CountKind(SRP_ECivKind.PARKED) + CountKind(SRP_ECivKind.DRIVER), zones.Count(), active);
		foreach (SRP_CivilZoneComponent zone : zones)
		{
			if (!zone.m_bActive)
				continue;
			text += string.Format("\n%1 : %2 promeneur(s), %3 immobile(s), %4 garée(s), %5 en route", zone.GetZoneName(), CountZone(zone, SRP_ECivKind.WALKER), CountZone(zone, SRP_ECivKind.POST), CountZone(zone, SRP_ECivKind.PARKED), CountZone(zone, SRP_ECivKind.DRIVER));
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountKind(int kind)
	{
		int count = 0;
		foreach (SRP_CivRecord record : m_aRecords)
		{
			if (record.m_iKind == kind)
				count++;
		}
		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! Toutes les IA reçoivent leurs dangers par ici : on prévient l'ambiance civile avant de rendre la main
modded class SCR_AIConfigComponent
{
	override bool PerformDangerReaction(SCR_AIUtilityComponent utility, AIDangerEvent dangerEvent, int dangerEventCount)
	{
		if (utility && dangerEvent)
		{
			SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
			if (civils)
				civils.OnDangerEvent(utility.m_OwnerEntity, dangerEvent);
		}
		return super.PerformDangerReaction(utility, dangerEvent, dangerEventCount);
	}
}
