//------------------------------------------------------------------------------------------------
// SimpleRP — Paquets de ravitaillement
// L'unité de ressource est un PAQUET : un objet d'inventaire de 1 kg (prefabs SRP_Paquet_Munitions,
// SRP_Paquet_Carburant, SRP_Paquet_Pieces, SRP_Paquet_Vivres, porteurs de SRP_PacketComponent).
// Il se manipule comme n'importe quel objet : sac, coffre d'un véhicule, dépôt.
// Une CAISSE DE LIVRAISON (prefab SRP_CaisseLivraison, un conteneur ouvrable) sert de contenant
// aux livraisons et aux cargaisons de mission : posée au sol, pleine de paquets, à transvaser.
// Un paquet peut porter l'identifiant d'une mission : déposé dans un dépôt, il fait avancer la mission.
// Phase 6 — étape A
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Paquet de ravitaillement SimpleRP : une unité de ressource, 1 kg")]
class SRP_PacketComponentClass : ScriptComponentClass
{
}

class SRP_PacketComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Ressource représentée par ce paquet", "", ParamEnumArray.FromEnum(SRP_EResource), category: "SimpleRP")]
	protected SRP_EResource m_eResource;

	protected string m_sMissionId;	// serveur : paquet d'une cargaison de mission

	//------------------------------------------------------------------------------------------------
	int GetResource()
	{
		return m_eResource;
	}

	//------------------------------------------------------------------------------------------------
	void SetMissionId(string id)
	{
		m_sMissionId = id;
	}

	//------------------------------------------------------------------------------------------------
	string GetMissionId()
	{
		return m_sMissionId;
	}

	//------------------------------------------------------------------------------------------------
	//! Le composant paquet d'un objet, ou null
	static SRP_PacketComponent Of(IEntity item)
	{
		if (!item)
			return null;
		return SRP_PacketComponent.Cast(item.FindComponent(SRP_PacketComponent));
	}
}

//------------------------------------------------------------------------------------------------
//! Outils d'inventaire communs : compter, retirer, ajouter des paquets dans un conteneur
class SRP_Packets
{
	protected static string s_sLastError;
	protected static bool s_bWarnedManager;

	//------------------------------------------------------------------------------------------------
	//! Dernière cause d'échec d'insertion, pour l'afficher au joueur ("" si tout va bien)
	static string GetLastError()
	{
		return s_sLastError;
	}

	//------------------------------------------------------------------------------------------------
	static string Describe(IEntity container)
	{
		if (!container)
			return "(null)";
		EntityPrefabData prefabData = container.GetPrefabData();
		if (prefabData)
			return SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
		return container.GetName();
	}

	//------------------------------------------------------------------------------------------------
	//! Le gestionnaire d'inventaire à utiliser pour écrire dans un conteneur : le nôtre en priorité,
	//! sinon le premier actif (un gestionnaire hérité et décoché ne compte pas)
	static InventoryStorageManagerComponent FindManager(IEntity container)
	{
		if (!container)
			return null;
		InventoryStorageManagerComponent ours = InventoryStorageManagerComponent.Cast(container.FindComponent(SRP_ContainerInventoryStorageManagerComponent));
		if (ours)
			return ours;
		array<Managed> managers = {};
		container.FindComponents(InventoryStorageManagerComponent, managers);
		foreach (Managed candidate : managers)
		{
			InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(candidate);
			if (manager && manager.IsActive())
				return manager;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les objets d'un conteneur, lus directement dans ses stockages (indépendant de tout gestionnaire)
	static void CollectItems(IEntity container, out array<IEntity> items)
	{
		if (!container)
			return;
		array<Managed> storages = {};
		container.FindComponents(BaseInventoryStorageComponent, storages);
		foreach (Managed candidate : storages)
		{
			BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(candidate);
			if (!storage)
				continue;
			array<IEntity> inside = {};
			storage.GetAll(inside);
			foreach (IEntity item : inside)
			{
				if (item && !items.Contains(item))
					items.Insert(item);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Description des composants d'inventaire d'un conteneur, pour le diagnostic au démarrage
	static string DescribeInventory(IEntity container)
	{
		if (!container)
			return "(null)";
		string text = Describe(container) + " : ";
		array<Managed> managers = {};
		container.FindComponents(InventoryStorageManagerComponent, managers);
		if (managers.IsEmpty())
			text += "AUCUN gestionnaire (SRP_ContainerInventoryStorageManagerComponent manquant : lecture possible, remplissage impossible)";
		else
		{
			text += "gestionnaire(s) :";
			foreach (Managed candidate : managers)
			{
				InventoryStorageManagerComponent manager = InventoryStorageManagerComponent.Cast(candidate);
				if (!manager)
					continue;
				string state = "actif";
				if (!manager.IsActive())
					state = "décoché";
				text += string.Format(" %1 (%2)", manager.ClassName(), state);
			}
		}
		array<Managed> storages = {};
		container.FindComponents(BaseInventoryStorageComponent, storages);
		text += string.Format(", %1 stockage(s)", storages.Count());
		if (storages.IsEmpty())
			text += " (SCR_UniversalInventoryStorageComponent manquant)";
		array<IEntity> items = {};
		CollectItems(container, items);
		text += string.Format(", %1 objet(s) dedans", items.Count());
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les paquets d'un conteneur (entité porteuse d'un inventaire), filtrés par ressource (-1 = toutes)
	static int Collect(IEntity container, int resource, out array<IEntity> packets)
	{
		if (!container)
			return 0;

		array<IEntity> items = {};
		CollectItems(container, items);
		int count = 0;
		foreach (IEntity item : items)
		{
			SRP_PacketComponent packet = SRP_PacketComponent.Of(item);
			if (!packet)
				continue;
			if (resource >= 0 && packet.GetResource() != resource)
				continue;
			packets.Insert(item);
			count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	static int Count(IEntity container, int resource)
	{
		array<IEntity> packets = {};
		return Collect(container, resource, packets);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire jusqu'à « amount » paquets d'une ressource. Retourne le nombre retiré.
	static int Remove(IEntity container, int resource, int amount)
	{
		if (amount <= 0 || !container)
			return 0;
		InventoryStorageManagerComponent inventory = FindManager(container);

		array<IEntity> packets = {};
		Collect(container, resource, packets);
		int removed = 0;
		foreach (IEntity packet : packets)
		{
			if (removed >= amount)
				break;
			if (inventory)
			{
				if (inventory.TryDeleteItem(packet))
					removed++;
			}
			else
			{
				// Sans gestionnaire : suppression directe, le stockage libère la case
				SCR_EntityHelper.DeleteEntityAndChildren(packet);
				removed++;
			}
		}
		return removed;
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute jusqu'à « amount » paquets (spawn dans le conteneur). S'arrête quand le conteneur est plein. Retourne le nombre ajouté.
	//! Vérifie que le prefab est bien un paquet (SRP_PacketComponent) : sinon, rien n'est compté nulle part.
	static int Insert(IEntity container, ResourceName packetPrefab, int amount, string missionId = "")
	{
		s_sLastError = "";
		if (amount <= 0 || packetPrefab.IsEmpty() || !container)
		{
			s_sLastError = "prefab de paquet non réglé";
			return 0;
		}

		InventoryStorageManagerComponent inventory = FindManager(container);
		if (!inventory)
		{
			s_sLastError = Describe(container) + " n'a pas de gestionnaire d'inventaire actif : ajoute SRP_ContainerInventoryStorageManagerComponent au prefab";
			Print("[SRP] Paquets : " + s_sLastError, LogLevel.ERROR);
			return 0;
		}

		// Le gestionnaire des personnages sur un prop : toléré (notre crochet le protège), mais signalé une fois
		if (SCR_InventoryStorageManagerComponent.Cast(inventory) && !container.FindComponent(CharacterInventoryStorageComponent) && !s_bWarnedManager)
		{
			s_bWarnedManager = true;
			Print("[SRP] Paquets : " + Describe(container) + " porte SCR_InventoryStorageManagerComponent (celui des personnages). Ça fonctionne, mais SRP_ContainerInventoryStorageManagerComponent est le bon composant pour un conteneur.", LogLevel.WARNING);
		}

		BaseInventoryStorageComponent storage = BaseInventoryStorageComponent.Cast(container.FindComponent(BaseInventoryStorageComponent));
		if (!storage)
		{
			s_sLastError = Describe(container) + " n'a aucun stockage (SCR_UniversalInventoryStorageComponent)";
			Print("[SRP] Paquets : " + s_sLastError, LogLevel.ERROR);
			return 0;
		}

		Resource res = Resource.Load(packetPrefab);
		if (!res || !res.IsValid())
		{
			s_sLastError = "prefab de paquet invalide : " + packetPrefab;
			Print("[SRP] Paquets : " + s_sLastError, LogLevel.ERROR);
			return 0;
		}

		// Chaque paquet est créé par nous, puis rangé dans le stockage du conteneur : on sait exactement ce qui entre
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = container.GetOrigin() + vector.Up * 0.5;

		array<IEntity> spawned = {};
		int added = 0;
		for (int i = 0; i < amount; i++)
		{
			IEntity item = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
			if (!item)
			{
				s_sLastError = "création impossible du paquet " + SRP_Utils.PrefabShortName(packetPrefab);
				Print("[SRP] Paquets : " + s_sLastError, LogLevel.ERROR);
				break;
			}

			bool ok = inventory.TryInsertItemInStorage(item, storage);
			if (!ok)
				ok = inventory.TryInsertItem(item);
			if (!ok)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(item);
				if (added == 0)
				{
					s_sLastError = string.Format("%1 refuse %2 : capacité du stockage (poids ou volume), ou objet non stockable (InventoryItemComponent, dimensions)", Describe(container), SRP_Utils.PrefabShortName(packetPrefab));
					Print("[SRP] Paquets : " + s_sLastError, LogLevel.ERROR);
				}
				break;
			}

			SRP_PacketComponent packet = SRP_PacketComponent.Of(item);
			if (packet && !missionId.IsEmpty())
				packet.SetMissionId(missionId);
			spawned.Insert(item);
			added++;
		}

		if (added == 0)
			return 0;

		// Contrôle : les paquets rangés sont-ils bien lus dans le stockage ?
		array<IEntity> now = {};
		CollectAll(container, now);
		int recognized = 0;
		foreach (IEntity item : spawned)
		{
			if (now.Contains(item) && SRP_PacketComponent.Of(item))
				recognized++;
		}
		if (recognized == 0)
		{
			if (!SRP_PacketComponent.Of(spawned[0]))
				s_sLastError = string.Format("%1 objets rangés, mais %2 n'a pas de SRP_PacketComponent : le prefab n'est pas reconnu comme paquet (ajoute le composant et règle sa ressource)", added, SRP_Utils.PrefabShortName(packetPrefab));
			else
				s_sLastError = string.Format("%1 paquets acceptés par le gestionnaire %2 de %3 mais introuvables dans son stockage : mets SRP_ContainerInventoryStorageManagerComponent sur le prefab", added, inventory.ClassName(), Describe(container));
			Print("[SRP] Paquets : " + s_sLastError, LogLevel.ERROR);
		}
		else if (recognized < added)
			Print(string.Format("[SRP] Paquets : %1 sur %2 retrouvés dans %3", recognized, added, Describe(container)), LogLevel.WARNING);
		return recognized;
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les objets d'un conteneur, paquets ou non
	protected static void CollectAll(IEntity container, out array<IEntity> items)
	{
		CollectItems(container, items);
	}
}
