//------------------------------------------------------------------------------------------------
// SimpleRP — Flotte persistante et certifications de conduite
// - Composition de la flotte en config (prefab, nombre, catégorie, capacité, coût en pièces)
// - Premier lancement : un véhicule par marqueur SRP_Parc_1, SRP_Parc_2… dans l'ordre de la config
// - Persistance dans $profile:SimpleRP/flotte.json : position, orientation, carburant, dégâts
//   par zone, caisses SimpleRP dans le coffre. Un véhicule laissé sur le terrain y reste.
// - Perdu = détruit à 100 % (irréparable) : retiré du parc, journal + alerte officiers. Un véhicule
//   endommagé mais réparable reste là où il est, aussi longtemps qu'il faut, redémarrage compris :
//   à aller réparer et ramener. Le ramassage automatique vanilla (SCR_GarbageSystem) est neutralisé
//   pour chaque véhicule du parc, à chaque cycle de surveillance.
// - Balises GPS : chaque véhicule remonte sa position au carré de 100 m (« 087 028 »), onglet Base du PC
// - Poste conducteur/pilote : certification par catégorie, verrou souple (avertissement,
//   journal, alerte officiers, une alerte par minute et par catégorie). Passagers et tourelles libres.
// - /flotte : état de la flotte ; /flotte reset (Staff) : tout supprimer et repartir de la config
// - Achat d'un véhicule du catalogue (terminal logistique ou /acheter vehicule <n>) : euros
//   débités de la trésorerie, nouvelle fiche, livraison au marqueur SRP_Livraison après le délai.
//   Le délai survit au redémarrage. Un véhicule détruit reste dans l'historique (/flotte).
// Phase 3 — étape C
//------------------------------------------------------------------------------------------------

enum SRP_EVehicleCategory
{
	LEGER = 0,
	LOURD = 1,
	BLINDE = 2,
	HELICO = 3
};

enum SRP_EVehicleState
{
	ACTIF = 0,
	DETRUIT = 1,
	EN_COMMANDE = 2
};

//------------------------------------------------------------------------------------------------
class SRP_VehicleCategories
{
	//------------------------------------------------------------------------------------------------
	static int GetCertif(int category)
	{
		switch (category)
		{
			case SRP_EVehicleCategory.LEGER: return SRP_ECertif.COND_LEGER;
			case SRP_EVehicleCategory.LOURD: return SRP_ECertif.COND_LOURD;
			case SRP_EVehicleCategory.BLINDE: return SRP_ECertif.BLINDE;
			case SRP_EVehicleCategory.HELICO: return SRP_ECertif.PILOTE;
		}
		return SRP_ECertif.COND_LEGER;
	}

	//------------------------------------------------------------------------------------------------
	static string GetName(int category)
	{
		switch (category)
		{
			case SRP_EVehicleCategory.LEGER: return "léger";
			case SRP_EVehicleCategory.LOURD: return "lourd";
			case SRP_EVehicleCategory.BLINDE: return "blindé";
			case SRP_EVehicleCategory.HELICO: return "hélico";
		}
		return "?";
	}
}

//------------------------------------------------------------------------------------------------
//! Une ligne de la composition de la flotte (config du composant)
[BaseContainerProps(), BaseContainerCustomTitleField("m_sName")]
class SRP_FleetEntry
{
	[Attribute("Véhicule", UIWidgets.EditBox, "Nom affiché")]
	string m_sName;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab du véhicule", "et")]
	ResourceName m_sPrefab;

	[Attribute("1", UIWidgets.EditBox, "Nombre créé au premier lancement et à chaque remise à zéro (flotte de départ ; 0 = aucun)")]
	int m_iCount;

	[Attribute("0", UIWidgets.ComboBox, "Catégorie de conduite", "", ParamEnumArray.FromEnum(SRP_EVehicleCategory))]
	int m_iCategory;

	[Attribute("4", UIWidgets.EditBox, "Capacité en caisses (informative pour l'instant)")]
	int m_iCargoCapacity;

	[Attribute("10000", UIWidgets.EditBox, "Prix d'achat en euros (terminal logistique)")]
	int m_iPriceEuros;
}

//------------------------------------------------------------------------------------------------
//! Un véhicule de la flotte, tel que sauvegardé dans flotte.json
class SRP_FleetVehicle
{
	string m_sId;
	string m_sName;
	ResourceName m_sPrefab;
	int m_iCategory;
	int m_iState;
	int m_iCargoCapacity;

	bool m_bHasTransform;
	float m_fX;
	float m_fY;
	float m_fZ;
	float m_fYaw;
	float m_fPitch;
	float m_fRoll;

	bool m_bHasFuel;
	float m_fFuel;

	ref array<string> m_aHitZones = {};
	ref array<float> m_aHitZoneHealth = {};
	ref array<string> m_aCargo = {};

	int m_iOrderSecondsLeft;

	//------------------------------------------------------------------------------------------------
	void GetTransform(out vector mat[4])
	{
		vector angles = Vector(m_fYaw, m_fPitch, m_fRoll);
		Math3D.AnglesToMatrix(angles, mat);
		mat[3] = Vector(m_fX, m_fY, m_fZ);
	}

	//------------------------------------------------------------------------------------------------
	void SetTransform(vector mat[4])
	{
		vector angles = Math3D.MatrixToAngles(mat);
		m_fYaw = angles[0];
		m_fPitch = angles[1];
		m_fRoll = angles[2];
		m_fX = mat[3][0];
		m_fY = mat[3][1];
		m_fZ = mat[3][2];
		m_bHasTransform = true;
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Flotte persistante SimpleRP : composition, spawn au parc, sauvegarde, certifications de conduite")]
class SRP_FleetManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_FleetManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.Object, "Flotte de départ : véhicules créés au premier lancement et à chaque remise à zéro, dans l'ordre, sur les marqueurs SRP_Parc_1, SRP_Parc_2… (« Nombre » par ligne). Vide = les véhicules du catalogue avec leur nombre", category: "SimpleRP - Flotte de départ")]
	protected ref array<ref SRP_FleetEntry> m_aFlotte;

	[Attribute("15", UIWidgets.EditBox, "Un véhicule détruit est retiré du parc (à recommander au terminal) ; son épave est supprimée après … minutes (0 = jamais)", category: "SimpleRP - Flotte de départ")]
	protected int m_iWreckMinutes;

	[Attribute("SRP_Parc_", UIWidgets.EditBox, "Préfixe des marqueurs de stationnement (SRP_Parc_1, SRP_Parc_2…)", category: "SimpleRP")]
	protected string m_sParcPrefix;

	[Attribute("SRP_Livraison", UIWidgets.EditBox, "Marqueur de livraison de secours (surplus du premier lancement, et véhicules commandés sans SRP_DeliveryComponent)", category: "SimpleRP")]
	protected string m_sLivraisonName;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur du centre de la base (pour l'état « à la base »)", category: "SimpleRP")]
	protected string m_sBaseMarkerName;

	[Attribute("300", UIWidgets.EditBox, "Rayon autour du centre de la base dans lequel un véhicule est « à la base », en mètres", category: "SimpleRP")]
	protected float m_fBaseRadius;

	[Attribute("10", UIWidgets.EditBox, "Période de surveillance (position, carburant, dégâts, destruction), en secondes", category: "SimpleRP")]
	protected int m_iTickSeconds;

	[Attribute("120", UIWidgets.EditBox, "Période de sauvegarde de flotte.json si quelque chose a changé, en secondes", category: "SimpleRP")]
	protected int m_iSaveSeconds;

	[Attribute("0.5", UIWidgets.EditBox, "Hauteur ajoutée au spawn pour ne pas traverser le sol, en mètres", category: "SimpleRP")]
	protected float m_fSpawnLift;

	[Attribute("60", UIWidgets.EditBox, "Délai minimum entre deux alertes pour le même joueur et la même catégorie, en secondes", category: "SimpleRP")]
	protected int m_iAlertCooldownSeconds;

	[Attribute("600", UIWidgets.EditBox, "Délai de livraison d'un véhicule commandé, en secondes", category: "SimpleRP")]
	protected int m_iOrderDelaySeconds;


	static const string FLEET_PATH = "$profile:SimpleRP/flotte.json";

	protected static SRP_FleetManagerComponent s_Instance;

	protected ref array<ref SRP_FleetVehicle> m_aVehicles = {};
	protected ref map<string, IEntity> m_mEntities = new map<string, IEntity>();
	protected ref map<string, int> m_mLastAlert = new map<string, int>();
	protected bool m_bDirty;
	protected bool m_bReady;
	protected int m_iNextNumber;		// prochain numéro Vxx : un numéro n'est jamais réutilisé (flotte.json)

	//------------------------------------------------------------------------------------------------
	static SRP_FleetManagerComponent GetInstance()
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
		SRP_Paths.EnsureDirectories();
	}

	//------------------------------------------------------------------------------------------------
	override void OnGameModeStart()
	{
		super.OnGameModeStart();

		if (!Replication.IsServer())
			return;

		// Un peu de délai : les marqueurs et le monde doivent être en place
		GetGame().GetCallqueue().CallLater(Initialize, 1500, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		// À l'arrêt, les coffres des véhicules peuvent déjà être vidés par la destruction du monde :
		// on écrit le dernier état capturé (le tick capture toutes les 10 s), sans relire les véhicules
		if (Replication.IsServer() && m_bReady)
			Save();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Initialize()
	{
		if (FileIO.FileExists(FLEET_PATH))
			Load();
		else
			FirstLaunch();

		m_bReady = true;
		GetGame().GetCallqueue().CallLater(Tick, m_iTickSeconds * 1000, true);
		GetGame().GetCallqueue().CallLater(SaveIfDirty, m_iSaveSeconds * 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	// Premier lancement
	//------------------------------------------------------------------------------------------------
	protected void FirstLaunch()
	{
		m_aVehicles.Clear();
		m_mEntities.Clear();

		array<ref SRP_FleetEntry> entries = GetStartingFleet();
		if (!entries || entries.IsEmpty())
		{
			SRP_JournalComponent.Log("FLOTTE", "Aucune flotte de départ (SRP_FleetManagerComponent → Flotte de départ) ni catalogue : parc vide, tout s'achète au terminal");
			m_iNextNumber = 1;
			Save();
			return;
		}

		array<IEntity> parcs = {};
		for (int i = 1; i <= 60; i++)
		{
			IEntity marker = GetGame().GetWorld().FindEntityByName(m_sParcPrefix + i.ToString());
			if (!marker)
				break;
			parcs.Insert(marker);
		}

		IEntity livraison = GetGame().GetWorld().FindEntityByName(m_sLivraisonName);
		if (parcs.IsEmpty() && !livraison)
		{
			Print(string.Format("[SRP] Aucun marqueur de parc (%1...) ni de livraison (%2) dans le monde : flotte non créée", m_sParcPrefix, m_sLivraisonName), LogLevel.ERROR);
			return;
		}

		int index = 0;
		int overflow = 0;
		foreach (SRP_FleetEntry entry : entries)
		{
			if (!entry || entry.m_sPrefab.IsEmpty() || entry.m_iCount < 1)
				continue;

			for (int n = 0; n < entry.m_iCount; n++)
			{
				index++;
				SRP_FleetVehicle record = new SRP_FleetVehicle();
				record.m_sId = "V" + SRP_Time.Pad2(index);
				record.m_sName = entry.m_sName;
				record.m_sPrefab = entry.m_sPrefab;
				record.m_iCategory = entry.m_iCategory;
				record.m_iState = SRP_EVehicleState.ACTIF;
				record.m_iCargoCapacity = entry.m_iCargoCapacity;

				vector mat[4];
				if (index <= parcs.Count())
				{
					parcs[index - 1].GetWorldTransform(mat);
				}
				else
				{
					// Plus de places : au point de livraison, décalé pour ne pas empiler
					if (livraison)
						livraison.GetWorldTransform(mat);
					else
						parcs[0].GetWorldTransform(mat);
					mat[3] = mat[3] + mat[0] * (6 * overflow);
					overflow++;
					Print(string.Format("[SRP] Pas assez de marqueurs %1N pour %2 : posé au point de livraison", m_sParcPrefix, record.m_sId), LogLevel.WARNING);
				}

				m_aVehicles.Insert(record);
				IEntity entity = SpawnVehicle(record, mat);
				if (entity)
					CaptureVehicle(record, entity);
			}
		}

		m_iNextNumber = index + 1;
		string source = "flotte de départ du composant";
		if (!m_aFlotte || m_aFlotte.IsEmpty())
			source = "catalogue";
		SRP_JournalComponent.Log("FLOTTE", string.Format("Premier lancement : %1 véhicule(s) créé(s) sur %2 place(s) (%3)", m_aVehicles.Count(), parcs.Count(), source));
		Save();
	}

	//------------------------------------------------------------------------------------------------
	//! La flotte de départ : la liste du composant, sinon les véhicules du catalogue avec leur nombre
	protected array<ref SRP_FleetEntry> GetStartingFleet()
	{
		if (m_aFlotte && !m_aFlotte.IsEmpty())
			return m_aFlotte;
		return GetCatalogue();
	}

	//------------------------------------------------------------------------------------------------
	// Chargement
	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(FLEET_PATH))
		{
			Print("[SRP] flotte.json illisible, premier lancement à la place", LogLevel.ERROR);
			FirstLaunch();
			return;
		}

		array<ref SRP_FleetVehicle> loaded = {};
		ctx.ReadValue("vehicles", loaded);
		m_iNextNumber = 0;
		ctx.ReadValue("nextNumber", m_iNextNumber);

		m_aVehicles.Clear();
		m_mEntities.Clear();

		int spawned = 0;
		int destroyed = 0;
		int pending = 0;
		foreach (SRP_FleetVehicle record : loaded)
		{
			if (!record || record.m_sId.IsEmpty())
				continue;

			// Un véhicule détruit ne fait plus partie du parc (ancienne sauvegarde) : il se recommande au terminal
			if (record.m_iState == SRP_EVehicleState.DETRUIT)
			{
				destroyed++;
				continue;
			}

			m_aVehicles.Insert(record);

			if (record.m_iState != SRP_EVehicleState.ACTIF)
			{
				pending++;
				continue;
			}

			vector mat[4];
			if (record.m_bHasTransform)
			{
				record.GetTransform(mat);
			}
			else
			{
				IEntity marker = GetGame().GetWorld().FindEntityByName(m_sParcPrefix + "1");
				if (!marker)
					marker = GetGame().GetWorld().FindEntityByName(m_sLivraisonName);
				if (!marker)
					continue;
				marker.GetWorldTransform(mat);
			}

			IEntity entity = SpawnVehicle(record, mat);
			if (!entity)
				continue;

			spawned++;
			GetGame().GetCallqueue().CallLater(ApplySavedState, 1000, false, record.m_sId);
		}

		if (destroyed > 0)
			m_bDirty = true;
		SRP_JournalComponent.Log("FLOTTE", string.Format("Flotte chargée : %1 véhicule(s) en place, %2 en commande, %3 détruit(s) retiré(s) du parc", spawned, pending, destroyed));
	}

	//------------------------------------------------------------------------------------------------
	//! Carburant, dégâts par zone et cargo, un peu après le spawn (les composants doivent être prêts)
	protected void ApplySavedState(string id)
	{
		SRP_FleetVehicle record = FindById(id);
		IEntity entity;
		if (!record || !m_mEntities.Find(id, entity) || !entity)
			return;

		if (record.m_bHasFuel)
		{
			array<SCR_FuelManagerComponent> managers = {};
			if (SCR_FuelManagerComponent.GetAllFuelManagers(entity, managers) > 0)
				SCR_FuelManagerComponent.SetTotalFuelPercentageOfFuelManagers(managers, record.m_fFuel);
		}

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(entity);
		if (damage && record.m_aHitZones.Count() == record.m_aHitZoneHealth.Count())
		{
			array<HitZone> zones = {};
			damage.GetAllHitZones(zones);
			foreach (HitZone zone : zones)
			{
				int index = record.m_aHitZones.Find(zone.GetName());
				if (index < 0)
					continue;
				float health = record.m_aHitZoneHealth[index];
				if (health < 0.05)
					health = 0.05;	// jamais détruit au chargement : un véhicule détruit n'est pas recréé
				zone.SetHealthScaled(health);
			}
		}

		if (record.m_aCargo.Count() > 0)
		{
			InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
			if (inventory)
			{
				foreach (string prefab : record.m_aCargo)
					inventory.TrySpawnPrefabToStorage(prefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, 1);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Spawn et capture
	//------------------------------------------------------------------------------------------------
	protected IEntity SpawnVehicle(SRP_FleetVehicle record, vector mat[4])
	{
		Resource res = Resource.Load(record.m_sPrefab);
		if (!res || !res.IsValid())
		{
			Print(string.Format("[SRP] Prefab de véhicule invalide pour %1 : %2", record.m_sId, record.m_sPrefab), LogLevel.ERROR);
			return null;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3] + vector.Up * m_fSpawnLift;

		IEntity entity = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!entity)
		{
			Print(string.Format("[SRP] Spawn impossible pour %1 (%2)", record.m_sId, record.m_sPrefab), LogLevel.ERROR);
			return null;
		}

		m_mEntities.Set(record.m_sId, entity);
		ProtectFromGarbage(entity);
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Le ramassage vanilla efface les véhicules abandonnés loin des joueurs : jamais pour ceux du parc.
	//! Le jeu les y remet quand le dernier occupant descend, d'où l'appel à chaque cycle.
	protected static void ProtectFromGarbage(IEntity entity)
	{
		if (!entity || entity.IsDeleted())
			return;
		SCR_GarbageSystem garbage = SCR_GarbageSystem.GetByEntityWorld(entity);
		if (garbage)
			garbage.Withdraw(entity);
	}

	//------------------------------------------------------------------------------------------------
	protected void CaptureVehicle(SRP_FleetVehicle record, IEntity entity)
	{
		vector mat[4];
		entity.GetWorldTransform(mat);
		record.SetTransform(mat);

		array<SCR_FuelManagerComponent> managers = {};
		if (SCR_FuelManagerComponent.GetAllFuelManagers(entity, managers) > 0)
		{
			float total, max, percentage;
			SCR_FuelManagerComponent.GetTotalValuesOfFuelNodesOfFuelManagers(managers, total, max, percentage);
			record.m_fFuel = percentage;
			record.m_bHasFuel = true;
		}

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(entity);
		if (damage)
		{
			array<HitZone> zones = {};
			damage.GetAllHitZones(zones);
			record.m_aHitZones.Clear();
			record.m_aHitZoneHealth.Clear();
			foreach (HitZone zone : zones)
			{
				record.m_aHitZones.Insert(zone.GetName());
				record.m_aHitZoneHealth.Insert(zone.GetHealthScaled());
			}
		}

		record.m_aCargo.Clear();
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		if (inventory)
		{
			array<IEntity> items = {};
			inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);
			foreach (IEntity item : items)
			{
				if (!item || !item.FindComponent(SRP_PacketComponent))
					continue;
				EntityPrefabData prefabData = item.GetPrefabData();
				if (prefabData)
					record.m_aCargo.Insert(prefabData.GetPrefabName());
			}
		}

		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void CaptureAll()
	{
		foreach (SRP_FleetVehicle record : m_aVehicles)
		{
			if (record.m_iState != SRP_EVehicleState.ACTIF)
				continue;
			IEntity entity;
			if (m_mEntities.Find(record.m_sId, entity) && entity && !entity.IsDeleted())
				CaptureVehicle(record, entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Surveillance
	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		for (int i = m_aVehicles.Count() - 1; i >= 0; i--)
		{
			SRP_FleetVehicle record = m_aVehicles[i];
			if (record.m_iState == SRP_EVehicleState.EN_COMMANDE)
			{
				record.m_iOrderSecondsLeft -= m_iTickSeconds;
				m_bDirty = true;
				if (record.m_iOrderSecondsLeft <= 0)
					Deliver(record);
				continue;
			}

			if (record.m_iState != SRP_EVehicleState.ACTIF)
				continue;

			IEntity entity;
			m_mEntities.Find(record.m_sId, entity);
			if (!entity || entity.IsDeleted())
			{
				MarkDestroyed(record, "disparu");
				continue;
			}

			SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(entity);
			if (damage && damage.GetState() == EDamageState.DESTROYED)
			{
				CaptureVehicle(record, entity);
				MarkDestroyed(record, "détruit");
				continue;
			}

			ProtectFromGarbage(entity);
			CaptureVehicle(record, entity);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un véhicule détruit ou disparu est perdu : retiré du parc, à recommander au terminal logistique.
	//! L'épave reste sur place le temps réglé, puis est supprimée.
	protected void MarkDestroyed(SRP_FleetVehicle record, string how)
	{
		IEntity entity;
		m_mEntities.Find(record.m_sId, entity);
		m_mEntities.Remove(record.m_sId);
		record.m_iState = SRP_EVehicleState.DETRUIT;

		string where = "";
		if (record.m_bHasTransform)
			where = string.Format(" en %1 / %2", Math.Round(record.m_fX), Math.Round(record.m_fZ));

		string text = string.Format("%1 (%2) %3%4 : retiré du parc, à recommander au terminal logistique", record.m_sId, record.m_sName, how, where);
		SRP_JournalComponent.Log("FLOTTE", text);
		SRP_BridgeComponent.PushEvent("vehicule", string.Format("Véhicule perdu : %1 (%2) %3, carré %4", record.m_sName, record.m_sId, how, SRP_FleetManagerComponent.GridOf(Vector(record.m_fX, record.m_fY, record.m_fZ))));

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers("Parc auto : " + text);

		if (entity && !entity.IsDeleted() && m_iWreckMinutes > 0)
			GetGame().GetCallqueue().CallLater(DeleteWreck, m_iWreckMinutes * 60 * 1000, false, entity);

		m_aVehicles.RemoveItem(record);
		m_bDirty = true;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	protected void DeleteWreck(IEntity wreck)
	{
		if (wreck && !wreck.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(wreck);
	}

	//------------------------------------------------------------------------------------------------
	// Persistance
	//------------------------------------------------------------------------------------------------
	protected void SaveIfDirty()
	{
		if (!m_bDirty)
			return;
		Save();
	}

	//------------------------------------------------------------------------------------------------
	void Save()
	{
		if (!Replication.IsServer())
			return;

		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("initialized", true);
		ctx.WriteValue("savedAt", SRP_Time.Now());
		ctx.WriteValue("nextNumber", m_iNextNumber);
		ctx.WriteValue("vehicles", m_aVehicles);

		if (ctx.SaveToFile(FLEET_PATH))
			m_bDirty = false;
		else
			Print("[SRP] Échec de sauvegarde de la flotte : " + FLEET_PATH, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	// Consultation
	//------------------------------------------------------------------------------------------------
	//! Les fiches du parc (menu Staff)
	void GetVehicles(out array<SRP_FleetVehicle> result)
	{
		foreach (SRP_FleetVehicle vehicle : m_aVehicles)
			result.Insert(vehicle);
	}

	//------------------------------------------------------------------------------------------------
	//! L'entité vivante d'un véhicule du parc, ou null
	//! Véhicule en service et garé à la base (rayon m_fBaseRadius autour du marqueur de base)
	bool IsAtBase(SRP_FleetVehicle record)
	{
		if (!record || record.m_iState != SRP_EVehicleState.ACTIF)
			return false;
		IEntity entity = GetEntity(record.m_sId);
		IEntity base = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (!entity || !base)
			return false;
		return vector.Distance(base.GetOrigin(), entity.GetOrigin()) <= m_fBaseRadius;
	}

	//------------------------------------------------------------------------------------------------
	IEntity GetEntity(string id)
	{
		IEntity entity;
		if (m_mEntities.Find(id, entity) && entity && !entity.IsDeleted())
			return entity;
		return null;
	}

	//------------------------------------------------------------------------------------------------
	SRP_FleetVehicle FindById(string id)
	{
		foreach (SRP_FleetVehicle record : m_aVehicles)
		{
			if (record.m_sId == id)
				return record;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	SRP_FleetVehicle FindByEntity(IEntity entity)
	{
		if (!entity)
			return null;

		foreach (string id, IEntity candidate : m_mEntities)
		{
			if (candidate == entity)
				return FindById(id);
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Catégorie d'un véhicule : fiche de flotte, sinon composition (même prefab), sinon devinée
	int GetCategory(IEntity vehicle)
	{
		SRP_FleetVehicle record = FindByEntity(vehicle);
		if (record)
			return record.m_iCategory;

		EntityPrefabData prefabData = vehicle.GetPrefabData();
		array<ref SRP_FleetEntry> entries = GetCatalogue();
		if (prefabData && entries)
		{
			ResourceName prefab = prefabData.GetPrefabName();
			foreach (SRP_FleetEntry entry : entries)
			{
				if (entry && entry.m_sPrefab == prefab)
					return entry.m_iCategory;
			}
		}

		if (vehicle.FindComponent(VehicleHelicopterSimulation))
			return SRP_EVehicleCategory.HELICO;

		return SRP_EVehicleCategory.LEGER;
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		if (m_aVehicles.IsEmpty())
			return "Parc auto : aucun véhicule";

		IEntity base = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);

		string text = "Parc auto :";
		foreach (SRP_FleetVehicle record : m_aVehicles)
		{
			string state;
			switch (record.m_iState)
			{
				case SRP_EVehicleState.DETRUIT:
					state = "DÉTRUIT";
					break;
				case SRP_EVehicleState.EN_COMMANDE:
					state = string.Format("en commande, livraison dans %1 min", Math.Ceil(record.m_iOrderSecondsLeft / 60.0));
					break;
				default:
				{
					IEntity entity;
					m_mEntities.Find(record.m_sId, entity);
					if (!entity)
					{
						state = "?";
						break;
					}

					float distance = 0;
					if (base)
						distance = vector.Distance(base.GetOrigin(), entity.GetOrigin());
					if (base && distance > m_fBaseRadius)
						state = string.Format("sorti (%1 m)", Math.Round(distance));
					else
						state = "à la base";

					if (record.m_bHasFuel)
						state += string.Format(", carburant %1 %%", Math.Round(record.m_fFuel * 100));
					if (record.m_aCargo.Count() > 0)
						state += string.Format(", %1 paquet(s)", record.m_aCargo.Count());
					break;
				}
			}

			text += string.Format("\n%1 %2 [%3] : %4", record.m_sId, record.m_sName, SRP_VehicleCategories.GetName(record.m_iCategory), state);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré de carte de 100 m : « 087 028 » (colonne puis ligne, trois chiffres)
	static string GridOf(vector position)
	{
		int gx = Math.Floor(position[0] / 100);
		int gz = Math.Floor(position[2] / 100);
		return Pad3(gx) + " " + Pad3(gz);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Pad3(int value)
	{
		if (value < 0)
			value = 0;
		string s = value.ToString();
		while (s.Length() < 3)
			s = "0" + s;
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Balises GPS : une ligne par véhicule du parc avec son carré de 100 m, sans précision supplémentaire
	string GetBeaconText()
	{
		if (m_aVehicles.IsEmpty())
			return "aucun véhicule au parc";

		IEntity base = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		string text = "";
		foreach (SRP_FleetVehicle record : m_aVehicles)
		{
			string line = record.m_sId + " " + record.m_sName + " : ";
			if (record.m_iState == SRP_EVehicleState.EN_COMMANDE)
				line += "en livraison, pas encore de balise";
			else
			{
				IEntity entity = GetEntity(record.m_sId);
				vector position;
				bool known = false;
				if (entity)
				{
					position = entity.GetOrigin();
					known = true;
				}
				else if (record.m_bHasTransform)
				{
					position = Vector(record.m_fX, record.m_fY, record.m_fZ);
					known = true;
				}
				if (!known)
					line += "balise muette";
				else
				{
					line += "carré " + GridOf(position);
					if (base && vector.Distance(base.GetOrigin(), position) <= m_fBaseRadius)
						line += " (à la base)";
					else if (base)
						line += " (sur le terrain)";
					if (entity)
					{
						SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(entity);
						if (damage && damage.GetState() != EDamageState.UNDAMAGED)
							line += ", endommagé";
					}
				}
			}
			if (!text.IsEmpty())
				text += "\n";
			text += line;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : supprime tous les véhicules de la flotte et repart de la composition
	void Reset()
	{
		foreach (string id, IEntity entity : m_mEntities)
		{
			if (entity && !entity.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		m_mEntities.Clear();
		m_aVehicles.Clear();

		if (FileIO.FileExists(FLEET_PATH))
			FileIO.DeleteFile(FLEET_PATH);

		SRP_JournalComponent.Log("FLOTTE", "Réinitialisation : flotte de départ recréée");
		FirstLaunch();
	}

	//------------------------------------------------------------------------------------------------
	// Commandes de remplacement
	//------------------------------------------------------------------------------------------------
	//! Véhicules du catalogue (fichier SRP_Catalogue.conf via la trésorerie), sinon la liste du composant
	array<ref SRP_FleetEntry> GetCatalogue()
	{
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (treasury)
		{
			SRP_Catalogue catalogue = treasury.GetCatalogue();
			if (catalogue && catalogue.m_aVehicules)
				return catalogue.m_aVehicules;
		}
		return m_aFlotte;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte du catalogue pour le chat
	string GetCatalogueText()
	{
		array<ref SRP_FleetEntry> entries = GetCatalogue();
		if (!entries || entries.IsEmpty())
			return "Catalogue de véhicules vide";

		string text = "Véhicules :";
		foreach (int index, SRP_FleetEntry entry : entries)
		{
			if (!entry || entry.m_sPrefab.IsEmpty())
				continue;
			text += string.Format("\n%1. %2 [%3] : %4 €", index, entry.m_sName, SRP_VehicleCategories.GetName(entry.m_iCategory), entry.m_iPriceEuros);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected string NextId()
	{
		int highest = 0;
		foreach (SRP_FleetVehicle existing : m_aVehicles)
		{
			string digits = existing.m_sId;
			digits.Replace("V", "");
			if (SRP_Utils.IsNumeric(digits) && digits.ToInt() > highest)
				highest = digits.ToInt();
		}
		int number = highest + 1;
		if (m_iNextNumber > number)
			number = m_iNextNumber;		// un véhicule détruit garde son numéro dans le journal : jamais réattribué
		m_iNextNumber = number + 1;
		m_bDirty = true;
		return "V" + SRP_Time.Pad2(number);
	}

	//------------------------------------------------------------------------------------------------
	//! Un véhicule déjà dans le monde (mission « véhicule abandonné ») rejoint la flotte. Retourne un suffixe de rapport.
	string AdoptVehicle(IEntity vehicle, string name)
	{
		if (!vehicle || vehicle.IsDeleted())
			return "";
		if (FindByEntity(vehicle))
			return "";

		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (!prefabData)
			return "";

		SRP_FleetVehicle record = new SRP_FleetVehicle();
		record.m_sId = NextId();
		record.m_sName = name;
		record.m_sPrefab = prefabData.GetPrefabName();
		record.m_iCategory = GetCategory(vehicle);
		record.m_iCargoCapacity = 4;
		record.m_iState = SRP_EVehicleState.ACTIF;
		m_aVehicles.Insert(record);
		m_mEntities.Set(record.m_sId, vehicle);
		CaptureVehicle(record, vehicle);
		m_bDirty = true;
		Save();

		SRP_JournalComponent.Log("FLOTTE", string.Format("%1 %2 intégré à la flotte (%3)", record.m_sId, record.m_sName, SRP_Utils.PrefabShortName(record.m_sPrefab)));
		return string.Format(", intégré au parc sous le numéro %1", record.m_sId);
	}

	//------------------------------------------------------------------------------------------------
	//! Achète un véhicule du catalogue (droits et proximité vérifiés par la trésorerie). Retourne le message au joueur.
	string BuyVehicle(int playerId, int entryIndex)
	{
		array<ref SRP_FleetEntry> entries = GetCatalogue();
		if (!entries || entryIndex < 0 || entryIndex >= entries.Count())
			return "Véhicule inconnu\n" + GetCatalogueText();

		SRP_FleetEntry entry = entries[entryIndex];
		if (!entry || entry.m_sPrefab.IsEmpty())
			return "Véhicule inconnu";

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
			return "Trésorerie absente du game mode";

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (!treasury.Spend(entry.m_iPriceEuros, string.Format("achat de %1 par %2", entry.m_sName, playerName)))
			return string.Format("Solde insuffisant : %1 coûte %2 € — %3", entry.m_sName, entry.m_iPriceEuros, treasury.GetBalanceText());

		SRP_FleetVehicle record = new SRP_FleetVehicle();
		record.m_sId = NextId();
		record.m_sName = entry.m_sName;
		record.m_sPrefab = entry.m_sPrefab;
		record.m_iCategory = entry.m_iCategory;
		record.m_iCargoCapacity = entry.m_iCargoCapacity;
		record.m_iState = SRP_EVehicleState.EN_COMMANDE;
		record.m_iOrderSecondsLeft = m_iOrderDelaySeconds;
		m_aVehicles.Insert(record);
		m_bDirty = true;
		Save();

		string text = string.Format("%1 %2 acheté par %3 (%4 €), livraison dans %5 min", record.m_sId, record.m_sName, playerName, entry.m_iPriceEuros, Math.Ceil(m_iOrderDelaySeconds / 60.0));
		SRP_JournalComponent.Log("FLOTTE", text);

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers("Parc auto : " + text);

		return string.Format("Commande passée : %1 %2, livraison dans %3 min au point de livraison\n%4", record.m_sId, record.m_sName, Math.Ceil(m_iOrderDelaySeconds / 60.0), treasury.GetBalanceText());
	}

	//------------------------------------------------------------------------------------------------
	protected void Deliver(SRP_FleetVehicle record)
	{
		// Point de livraison : gestionnaire de points (choix hors de vue des joueurs, garnison possible), sinon marqueur unique
		vector mat[4];
		string label = m_sLivraisonName;
		bool hostile = false;
		SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
		bool found = false;
		if (deliveries)
			found = deliveries.ChoosePoint(SRP_EDeliveryKind.VEHICLE, mat, label, hostile);
		if (!found)
		{
			IEntity marker = GetGame().GetWorld().FindEntityByName(m_sLivraisonName);
			if (!marker)
				marker = GetGame().GetWorld().FindEntityByName(m_sParcPrefix + "1");
			if (!marker)
			{
				Print("[SRP] Livraison impossible : aucun point de livraison ni marqueur " + m_sLivraisonName, LogLevel.ERROR);
				record.m_iOrderSecondsLeft = 60;	// nouvel essai dans une minute
				return;
			}
			marker.GetWorldTransform(mat);
		}

		record.m_aHitZones.Clear();
		record.m_aHitZoneHealth.Clear();
		record.m_aCargo.Clear();
		record.m_bHasFuel = false;
		record.m_bHasTransform = false;

		IEntity entity = SpawnVehicle(record, mat);
		if (!entity)
		{
			record.m_iOrderSecondsLeft = 60;
			return;
		}

		record.m_iState = SRP_EVehicleState.ACTIF;
		record.m_iOrderSecondsLeft = 0;
		CaptureVehicle(record, entity);
		Save();

		string what = string.Format("%1 %2", record.m_sId, record.m_sName);
		string text = string.Format("%1 : livré au point « %2 »", what, label);
		if (deliveries)
			text = deliveries.ArrivalText(what, label, hostile);
		SRP_JournalComponent.Log("FLOTTE", text + SRP_Utils.HostileSuffix(hostile));

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers("Parc auto : " + text);
	}

	//------------------------------------------------------------------------------------------------
	// Certification de conduite (appelé par le crochet ci-dessous, serveur)
	//------------------------------------------------------------------------------------------------
	void OnDriverSeatEntered(IEntity character, IEntity vehicle)
	{
		int playerId = SRP_Utils.GetPlayerIdFromEntity(character);
		if (playerId <= 0)
			return;

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;

		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record || record.m_bStaff)
			return;

		int category = GetCategory(vehicle);
		int certif = SRP_VehicleCategories.GetCertif(category);
		if (SRP_Certifs.Has(record.m_iCertifs, certif))
			return;

		string key = string.Format("%1:%2", playerId, category);
		int now = System.GetTickCount();
		int last;
		if (m_mLastAlert.Find(key, last) && now - last < m_iAlertCooldownSeconds * 1000)
			return;
		m_mLastAlert.Set(key, now);

		string vehicleName = "véhicule";
		SRP_FleetVehicle fleetRecord = FindByEntity(vehicle);
		if (fleetRecord)
			vehicleName = string.Format("%1 %2", fleetRecord.m_sId, fleetRecord.m_sName);
		else
		{
			EntityPrefabData prefabData = vehicle.GetPrefabData();
			if (prefabData)
				vehicleName = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
		}

		string certifName = SRP_Certifs.GetName(certif);
		players.LogInfraction(playerId, string.Format("conduite sans certification %1 : %2", certifName, vehicleName));
		players.NotifyOfficiers(string.Format("%1 conduit %2 sans la certification %3", record.m_sName, vehicleName, certifName));
		SRP_Utils.NotifyPlayer(playerId, string.Format("Vous n'êtes pas habilité (%1) : %2. Prise en main d'urgence enregistrée.", certifName, vehicleName));
	}
}

//------------------------------------------------------------------------------------------------
//! Crochet : entrée dans un compartiment. Seul le poste de pilotage nous intéresse.
modded class SCR_CompartmentAccessComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnCompartmentEntered(IEntity targetEntity, BaseCompartmentManagerComponent manager, int mgrID, int slotID, bool move)
	{
		super.OnCompartmentEntered(targetEntity, manager, mgrID, slotID, move);

		if (!Replication.IsServer() || !targetEntity || !manager)
			return;

		BaseCompartmentSlot slot = manager.FindCompartment(slotID);
		if (!slot || !PilotCompartmentSlot.Cast(slot))
			return;

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (fleet)
			fleet.OnDriverSeatEntered(GetOwner(), targetEntity);
	}
}
