//------------------------------------------------------------------------------------------------
// SimpleRP — Ressources de la base (paquets)
// Quatre ressources (munitions, carburant, pièces, vivres). L'unité est un paquet physique de
// 1 kg (voir SRP_Packet.c). Le stock d'une ressource, c'est le nombre de paquets présents dans
// ses dépôts : des conteneurs qu'on ouvre comme une caisse (SRP_DepotComponent).
// Le gestionnaire compte, retire (respawn, citerne, atelier, arsenal) et ajoute (livraisons,
// récompenses, retours) des paquets dans ces conteneurs. Persistance : les comptes dans
// base.json ; au démarrage, les dépôts sont regarnis à hauteur des comptes sauvegardés (le monde
// ne conserve pas les objets d'inventaire d'une session à l'autre). IsRestockDone dit quand ce regarnissage est fini :
// le versement des anciens secteurs (K2, SRP_FrontComponent.MigrationTick) l'attend pour ne pas être absorbé.
// Caisses de livraison : un conteneur posé au sol, rempli de paquets (SpawnBox), retiré une fois
// vide depuis un moment.
// Phase 6 — étape A
//------------------------------------------------------------------------------------------------

//! Une caisse de livraison posée par nous, pour le ménage
class SRP_DeliveryBox
{
	IEntity m_Entity;
	int m_iEmptySinceTick;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Ressources SimpleRP : paquets dans les dépôts, caisses de livraison, consommation des vivres")]
class SRP_ResourceManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_ResourceManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Paquet de munitions (prefab SRP_Paquet_Munitions)", "et", category: "SimpleRP - Paquets")]
	protected ResourceName m_sPacketMunitions;

	[Attribute("", UIWidgets.ResourceNamePicker, "Paquet de carburant", "et", category: "SimpleRP - Paquets")]
	protected ResourceName m_sPacketCarburant;

	[Attribute("", UIWidgets.ResourceNamePicker, "Paquet de pièces", "et", category: "SimpleRP - Paquets")]
	protected ResourceName m_sPacketPieces;

	[Attribute("", UIWidgets.ResourceNamePicker, "Paquet de vivres", "et", category: "SimpleRP - Paquets")]
	protected ResourceName m_sPacketVivres;

	[Attribute("", UIWidgets.ResourceNamePicker, "Caisse de livraison (prefab SRP_CaisseLivraison : conteneur ouvrable)", "et", category: "SimpleRP - Paquets")]
	protected ResourceName m_sBoxPrefab;

	[Attribute("50", UIWidgets.EditBox, "Stock initial en % de la capacité des dépôts, au tout premier lancement", category: "SimpleRP")]
	protected int m_iInitialPercent;

	[Attribute("2", UIWidgets.EditBox, "Vivres (paquets) consommés par joueur connecté et par heure", category: "SimpleRP")]
	protected float m_fVivresPerPlayerPerHour;

	[Attribute("2", UIWidgets.EditBox, "Vivres (paquets) débités à chaque respawn", category: "SimpleRP")]
	protected int m_iVivresPerRespawn;

	[Attribute("10", UIWidgets.EditBox, "Une caisse de livraison vide est retirée après … minutes", category: "SimpleRP")]
	protected int m_iEmptyBoxMinutes;

	protected static const int TICK_MS = 60000;			// consommation des vivres : toutes les minutes
	protected static const int SAVE_CHECK_MS = 30000;		// écriture sur disque si quelque chose a changé
	protected static const int RESTOCK_BATCH = 20;			// regarnissage au démarrage : paquets par passe

	protected static SRP_ResourceManagerComponent s_Instance;

	protected float m_fVivresAccumulator = 0;
	protected bool m_bDirty;
	protected bool m_bHasSave;			// base.json existait au démarrage
	protected bool m_bRestockWarned;
	protected ref array<int> m_aSavedStock = {0, 0, 0, 0};	// comptes lus dans base.json, à reconstituer
	protected ref array<int> m_aRestockLeft = {0, 0, 0, 0};	// reste à regarnir au démarrage
	protected ref array<int> m_aLastKnown = {0, 0, 0, 0};	// dernier compte fiable, pour l'arrêt (les dépôts disparaissent avant nous)
	protected bool m_bShuttingDown;
	protected bool m_bCounted;			// au moins un comptage fiable effectué depuis le démarrage
	protected bool m_bRestockDone;		// regarnissage du démarrage terminé (ou aucun dépôt) : K2 peut verser
	protected ref array<ref SRP_DeliveryBox> m_aBoxes = {};
	protected string m_sLastBoxError;

	//------------------------------------------------------------------------------------------------
	//! Ce qui a coincé lors de la dernière caisse posée ("" si rien)
	string GetLastBoxError()
	{
		return m_sLastBoxError;
	}

	//------------------------------------------------------------------------------------------------
	static SRP_ResourceManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur a déposé ou pris un paquet dans un dépôt : à sauvegarder
	static void MarkDirty()
	{
		if (s_Instance)
			s_Instance.m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;

		s_Instance = this;
		SRP_Paths.EnsureDirectories();
		Load();

		// Les dépôts s'enregistrent à leur propre init : on regarnit un peu après le démarrage
		GetGame().GetCallqueue().CallLater(StartRestock, 4000, false);
		GetGame().GetCallqueue().CallLater(TickVivres, TICK_MS, true);
		GetGame().GetCallqueue().CallLater(SaveIfDirty, SAVE_CHECK_MS, true);
		GetGame().GetCallqueue().CallLater(Save, 120000, true);		// filet de sécurité : les comptes sur disque toutes les 2 min
		GetGame().GetCallqueue().CallLater(TickBoxes, 60000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			// À l'arrêt, les dépôts peuvent déjà être détruits : on écrit le dernier compte fiable, jamais un recomptage à zéro
			m_bShuttingDown = true;
			Save();
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Les dépôts sont-ils encore là et intacts ? (faux pendant l'arrêt du monde)
	protected bool DepotsAreLive()
	{
		array<SRP_DepotComponent> depots = SRP_DepotComponent.GetDepots();
		if (depots.IsEmpty())
			return false;
		foreach (SRP_DepotComponent depot : depots)
		{
			IEntity owner = depot.GetOwner();
			if (!owner || owner.IsDeleted())
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Prefabs
	//------------------------------------------------------------------------------------------------
	ResourceName GetPacketPrefab(int resource)
	{
		switch (resource)
		{
			case SRP_EResource.MUNITIONS: return m_sPacketMunitions;
			case SRP_EResource.CARBURANT: return m_sPacketCarburant;
			case SRP_EResource.PIECES: return m_sPacketPieces;
			case SRP_EResource.VIVRES: return m_sPacketVivres;
		}
		return "";
	}

	//------------------------------------------------------------------------------------------------
	// Lecture : le stock est physique, on compte dans les dépôts
	//------------------------------------------------------------------------------------------------
	int GetStock(int resource)
	{
		if (!SRP_Resources.IsValide(resource))
			return 0;
		int total = 0;
		foreach (SRP_DepotComponent depot : SRP_DepotComponent.GetDepots())
		{
			if (depot.GetResource() == resource)
				total += SRP_Packets.Count(depot.GetOwner(), resource);
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Le regarnissage du démarrage est fini (dernière passe faite, ou aucun dépôt dans le monde). Avant, un versement
	//! par Add serait absorbé par le calcul du reste à regarnir (StartRestock : cible - stock). Pas m_bCounted, que
	//! GetStockForSave met à vrai même en plein regarnissage — SRP_FrontComponent.MigrationTick (K2)
	bool IsRestockDone()
	{
		return m_bRestockDone;
	}

	//------------------------------------------------------------------------------------------------
	int GetCapacity(int resource)
	{
		int total = 0;
		foreach (SRP_DepotComponent depot : SRP_DepotComponent.GetDepots())
		{
			if (depot.GetResource() == resource)
				total += depot.GetCapacity();
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	bool IsEmpty(int resource)
	{
		return GetStock(resource) <= 0;
	}

	//------------------------------------------------------------------------------------------------
	string GetStockText(int resource)
	{
		if (!SRP_Resources.IsValide(resource))
			return "?";
		int capacity = GetCapacity(resource);
		if (capacity <= 0)
			return string.Format("%1 : aucun dépôt posé", SRP_Resources.GetDepotName(resource));
		return string.Format("%1 : %2 / %3 %4", SRP_Resources.GetDepotName(resource), GetStock(resource), capacity, SRP_Resources.GetName(resource));
	}

	//------------------------------------------------------------------------------------------------
	string GetAllStockText(string separator)
	{
		string result = "";
		for (int r = 0; r < SRP_Resources.COUNT; r++)
		{
			if (!result.IsEmpty())
				result += separator;
			result += GetStockText(r);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	// Écriture : ajouter et retirer des paquets dans les dépôts
	//------------------------------------------------------------------------------------------------
	//! Ajoute jusqu'à « amount » paquets dans les dépôts de la ressource. Retourne le nombre ajouté (moins si les dépôts sont pleins).
	int Add(int resource, int amount, string reason, bool log = true)
	{
		if (!SRP_Resources.IsValide(resource) || amount <= 0)
			return 0;

		ResourceName prefab = GetPacketPrefab(resource);
		if (prefab.IsEmpty())
		{
			Print("[SRP] Aucun prefab de paquet réglé pour " + SRP_Resources.GetName(resource) + " (SRP_ResourceManagerComponent)", LogLevel.WARNING);
			return 0;
		}

		int added = 0;
		foreach (SRP_DepotComponent depot : SRP_DepotComponent.GetDepots())
		{
			if (added >= amount || depot.GetResource() != resource)
				continue;
			int room = depot.GetCapacity() - SRP_Packets.Count(depot.GetOwner(), resource);
			if (room <= 0)
				continue;
			added += SRP_Packets.Insert(depot.GetOwner(), prefab, Math.Min(room, amount - added));
		}

		if (added > 0)
		{
			m_bDirty = true;
			if (log)
				SRP_JournalComponent.Log("RESSOURCE", string.Format("+%1 %2 (%3) -> %4", added, SRP_Resources.GetName(resource), reason, GetStock(resource)));
		}
		if (added < amount && log)
			SRP_JournalComponent.Log("RESSOURCE", string.Format("%1 %2 non rangés : dépôts pleins (%3)", amount - added, SRP_Resources.GetName(resource), reason));
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire « amount » paquets, tout ou rien. false si le stock ne suffit pas.
	bool Take(int resource, int amount, string reason, bool log = true)
	{
		if (!SRP_Resources.IsValide(resource) || amount < 0)
			return false;
		if (amount == 0)
			return true;
		if (GetStock(resource) < amount)
			return false;

		int removed = 0;
		foreach (SRP_DepotComponent depot : SRP_DepotComponent.GetDepots())
		{
			if (removed >= amount || depot.GetResource() != resource)
				continue;
			removed += SRP_Packets.Remove(depot.GetOwner(), resource, amount - removed);
		}

		m_bDirty = true;
		if (log)
			SRP_JournalComponent.Log("RESSOURCE", string.Format("-%1 %2 (%3) -> %4", removed, SRP_Resources.GetName(resource), reason, GetStock(resource)));
		return removed >= amount;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire ce qui est possible, jusqu'à « amount ». Retourne le nombre retiré.
	int TakeUpTo(int resource, int amount, string reason)
	{
		int available = GetStock(resource);
		int wanted = Math.Min(available, amount);
		if (wanted <= 0)
			return 0;
		if (Take(resource, wanted, reason))
			return wanted;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : fixe le stock (ajoute ou retire des paquets)
	void SetStock(int resource, int amount, string reason)
	{
		if (!SRP_Resources.IsValide(resource))
			return;
		int current = GetStock(resource);
		if (amount > current)
			Add(resource, amount - current, reason);
		else if (amount < current)
			Take(resource, current - amount, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire des paquets d'une ressource dans un conteneur quelconque (véhicule, joueur) : utilisé par les livraisons inverses
	static int TakeFromContainer(IEntity container, int resource, int amount)
	{
		return SRP_Packets.Remove(container, resource, amount);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : dépôts vidés puis regarnis au niveau initial, caisses de livraison retirées
	void ResetAll(string reason)
	{
		for (int r = 0; r < SRP_Resources.COUNT; r++)
		{
			foreach (SRP_DepotComponent depot : SRP_DepotComponent.GetDepots())
			{
				if (depot.GetResource() == r)
					SRP_Packets.Remove(depot.GetOwner(), r, 100000);
			}
		}
		foreach (SRP_DeliveryBox record : m_aBoxes)
		{
			if (record.m_Entity && !record.m_Entity.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(record.m_Entity);
		}
		m_aBoxes.Clear();

		for (int r = 0; r < SRP_Resources.COUNT; r++)
		{
			m_aSavedStock[r] = 0;
			m_aRestockLeft[r] = GetCapacity(r) * m_iInitialPercent / 100;
		}
		m_bDirty = true;
		SRP_JournalComponent.Log("RESSOURCE", string.Format("Remise à zéro (%1) : dépôts regarnis à %2 %%", reason, m_iInitialPercent));
		RestockBatch();
	}

	//------------------------------------------------------------------------------------------------
	// Caisses de livraison
	//------------------------------------------------------------------------------------------------
	//! Pose une caisse à cette position et la remplit de paquets. Retourne la caisse (null si prefab manquant).
	IEntity SpawnBox(int resource, int packets, vector position, string missionId = "")
	{
		if (m_sBoxPrefab.IsEmpty())
		{
			Print("[SRP] Aucun prefab de caisse de livraison réglé (SRP_ResourceManagerComponent)", LogLevel.WARNING);
			return null;
		}
		ResourceName packetPrefab = GetPacketPrefab(resource);
		if (packetPrefab.IsEmpty())
		{
			Print("[SRP] Aucun prefab de paquet réglé pour " + SRP_Resources.GetName(resource), LogLevel.WARNING);
			return null;
		}

		Resource res = Resource.Load(m_sBoxPrefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Prefab de caisse de livraison invalide : " + m_sBoxPrefab, LogLevel.ERROR);
			return null;
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity box = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!box)
			return null;

		int added = SRP_Packets.Insert(box, packetPrefab, packets, missionId);
		m_sLastBoxError = SRP_Packets.GetLastError();
		if (added < packets && m_sLastBoxError.IsEmpty())
			m_sLastBoxError = string.Format("caisse trop petite : %1 / %2 paquets placés", added, packets);
		if (added > 0 && CountInBox(box, resource) == 0)
			m_sLastBoxError = string.Format("les %1 objets placés ne sont pas des paquets de %2 : vérifie SRP_PacketComponent et sa ressource sur %3", added, SRP_Resources.GetName(resource), SRP_Utils.PrefabShortName(packetPrefab));
		if (!m_sLastBoxError.IsEmpty())
			SRP_JournalComponent.Log("RESSOURCE", "Caisse de livraison : " + m_sLastBoxError);

		SRP_DeliveryBox record = new SRP_DeliveryBox();
		record.m_Entity = box;
		m_aBoxes.Insert(record);
		return box;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de paquets d'une ressource encore dans une caisse
	static int CountInBox(IEntity box, int resource)
	{
		return SRP_Packets.Count(box, resource);
	}

	//------------------------------------------------------------------------------------------------
	//! Ménage : une caisse vide depuis un moment est retirée
	protected void TickBoxes()
	{
		int now = System.GetTickCount();
		for (int i = m_aBoxes.Count() - 1; i >= 0; i--)
		{
			SRP_DeliveryBox record = m_aBoxes[i];
			if (!record.m_Entity || record.m_Entity.IsDeleted())
			{
				m_aBoxes.Remove(i);
				continue;
			}
			if (SRP_Packets.Count(record.m_Entity, -1) > 0)
			{
				record.m_iEmptySinceTick = 0;
				continue;
			}
			if (record.m_iEmptySinceTick == 0)
			{
				record.m_iEmptySinceTick = now;
				continue;
			}
			if (now - record.m_iEmptySinceTick > m_iEmptyBoxMinutes * 60 * 1000)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(record.m_Entity);
				m_aBoxes.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	// Vivres
	//------------------------------------------------------------------------------------------------
	void OnRespawn(string playerName)
	{
		if (m_iVivresPerRespawn <= 0)
			return;
		int taken = TakeUpTo(SRP_EResource.VIVRES, m_iVivresPerRespawn, "respawn de " + playerName);
		if (taken < m_iVivresPerRespawn)
			SRP_JournalComponent.Log("RESSOURCE", string.Format("Intendance insuffisante pour le respawn de %1 (%2 / %3)", playerName, taken, m_iVivresPerRespawn));
	}

	//------------------------------------------------------------------------------------------------
	protected void TickVivres()
	{
		if (!Replication.IsServer())
			return;

		if (DepotsAreLive())
		{
			for (int r = 0; r < SRP_Resources.COUNT; r++)
				m_aLastKnown[r] = GetStock(r) + Math.Max(0, m_aRestockLeft[r]);
		}

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;

		int connected = players.GetConnectedCount();
		if (connected <= 0)
			return;

		m_fVivresAccumulator += connected * m_fVivresPerPlayerPerHour / 60.0;

		int toConsume = 0;
		while (m_fVivresAccumulator >= 1.0)
		{
			m_fVivresAccumulator -= 1.0;
			toConsume++;
		}

		if (toConsume > 0)
		{
			int taken = TakeUpTo(SRP_EResource.VIVRES, toConsume, string.Format("entretien, %1 joueur(s)", connected));
			if (taken == 0)
				SRP_JournalComponent.Log("RESSOURCE", "Intendance vide : la base tourne au ralenti");
		}
	}

	//------------------------------------------------------------------------------------------------
	// Regarnissage au démarrage (les objets d'inventaire ne survivent pas à un redémarrage)
	//------------------------------------------------------------------------------------------------
	protected void StartRestock()
	{
		if (SRP_DepotComponent.GetDepots().IsEmpty())
		{
			Print("[SRP] Aucun dépôt (SRP_DepotComponent) dans le monde : rien à regarnir", LogLevel.WARNING);
			m_bRestockDone = true;		// rien à attendre : un versement (K2) partira en caisses
			return;
		}

		// Diagnostic : chaque dépôt, son gestionnaire et son stockage
		foreach (SRP_DepotComponent depot : SRP_DepotComponent.GetDepots())
			Print(string.Format("[SRP] Dépôt %1 (%2) : %3", depot.GetDepotName(), SRP_Resources.GetName(depot.GetResource()), SRP_Packets.DescribeInventory(depot.GetOwner())), LogLevel.NORMAL);
		Print(string.Format("[SRP] Comptes lus dans base.json : munitions %1, carburant %2, pièces %3, vivres %4 (fichier présent : %5)", m_aSavedStock[0], m_aSavedStock[1], m_aSavedStock[2], m_aSavedStock[3], m_bHasSave), LogLevel.NORMAL);

		bool firstLaunch = !m_bHasSave;

		for (int r = 0; r < SRP_Resources.COUNT; r++)
		{
			int target = m_aSavedStock[r];
			if (firstLaunch)
				target = GetCapacity(r) * m_iInitialPercent / 100;
			m_aRestockLeft[r] = Math.Max(0, target - GetStock(r));
		}

		if (firstLaunch)
			SRP_JournalComponent.Log("RESSOURCE", string.Format("Premier lancement : dépôts garnis à %1 %% de leur capacité", m_iInitialPercent));

		RestockBatch();
	}

	//------------------------------------------------------------------------------------------------
	//! Quelques paquets par passe, pour ne pas figer le serveur au démarrage
	protected void RestockBatch()
	{
		bool remaining = false;
		for (int r = 0; r < SRP_Resources.COUNT; r++)
		{
			if (m_aRestockLeft[r] <= 0)
				continue;
			int batch = Math.Min(RESTOCK_BATCH, m_aRestockLeft[r]);
			int added = Add(r, batch, "regarnissage", false);
			m_aRestockLeft[r] = m_aRestockLeft[r] - added;
			if (added < batch)
			{
				// Dépôts pleins ou prefab manquant : on n'insiste pas, mais le reste n'est pas perdu (voir Save)
				if (!m_bRestockWarned && m_aRestockLeft[r] > 0)
				{
					m_bRestockWarned = true;
					Print(string.Format("[SRP] Regarnissage incomplet : %1 %2 non placés (dépôt trop petit, capacité physique, ou prefab de paquet manquant). Le compte est conservé dans base.json.", m_aRestockLeft[r], SRP_Resources.GetName(r)), LogLevel.WARNING);
				}
				continue;
			}
			if (m_aRestockLeft[r] > 0)
				remaining = true;
		}

		if (remaining)
			GetGame().GetCallqueue().CallLater(RestockBatch, 200, false);
		else
		{
			for (int r = 0; r < SRP_Resources.COUNT; r++)
				m_aLastKnown[r] = GetStock(r) + Math.Max(0, m_aRestockLeft[r]);
			m_bCounted = true;
			m_bRestockDone = true;
			SRP_JournalComponent.Log("RESSOURCE", "Ressources démarrées : " + GetAllStockText(" | "));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Stock à sauvegarder : ce qui est dans les dépôts plus ce qui n'a pas pu y être remis ; à l'arrêt, le dernier compte fiable
	protected int GetStockForSave(int resource)
	{
		if (m_bShuttingDown || !DepotsAreLive())
			return m_aLastKnown[resource];
		int value = GetStock(resource) + Math.Max(0, m_aRestockLeft[resource]);
		m_aLastKnown[resource] = value;
		m_bCounted = true;
		return value;
	}

	//------------------------------------------------------------------------------------------------
	// Persistance (comptes)
	//------------------------------------------------------------------------------------------------
	protected void SaveIfDirty()
	{
		if (m_bDirty)
			Save();
	}

	//------------------------------------------------------------------------------------------------
	void Save()
	{
		if (!Replication.IsServer())
			return;

		// Arrêt avant le premier comptage fiable (regarnissage pas fini) : on garde le fichier tel quel
		if (m_bShuttingDown && m_bHasSave && !m_bCounted)
			return;

		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("initialized", true);
		ctx.WriteValue("munitions", GetStockForSave(SRP_EResource.MUNITIONS));
		ctx.WriteValue("carburant", GetStockForSave(SRP_EResource.CARBURANT));
		ctx.WriteValue("pieces", GetStockForSave(SRP_EResource.PIECES));
		ctx.WriteValue("vivres", GetStockForSave(SRP_EResource.VIVRES));
		ctx.WriteValue("savedAt", SRP_Time.Now());

		if (ctx.SaveToFile(SRP_Paths.BASE))
		{
			m_bDirty = false;
			Print(string.Format("[SRP] Ressources sauvegardées : munitions %1, carburant %2, pièces %3, vivres %4", GetStockForSave(0), GetStockForSave(1), GetStockForSave(2), GetStockForSave(3)), LogLevel.NORMAL);
		}
		else
			Print("[SRP] Échec de sauvegarde des ressources : " + SRP_Paths.BASE, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		if (!FileIO.FileExists(SRP_Paths.BASE))
			return;
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(SRP_Paths.BASE))
		{
			Print("[SRP] base.json illisible", LogLevel.ERROR);
			return;
		}
		m_bHasSave = true;
		int value;
		if (ctx.ReadValue("munitions", value)) m_aSavedStock[SRP_EResource.MUNITIONS] = value;
		if (ctx.ReadValue("carburant", value)) m_aSavedStock[SRP_EResource.CARBURANT] = value;
		if (ctx.ReadValue("pieces", value)) m_aSavedStock[SRP_EResource.PIECES] = value;
		if (ctx.ReadValue("vivres", value)) m_aSavedStock[SRP_EResource.VIVRES] = value;
	}
}
