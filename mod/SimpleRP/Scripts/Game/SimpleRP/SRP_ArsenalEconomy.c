//------------------------------------------------------------------------------------------------
// SimpleRP — Économie d'arsenal sur l'inventaire
// Le mod d'arsenal (Bacon Loadout Editor ou autre) affiche et équipe ; notre serveur décide.
// Crochet sur l'inventaire de chaque personnage joueur :
//   - objet ajouté dans une zone d'arsenal (SRP_ArsenalZoneComponent) : facturé en paquets de
//     munitions selon son type : armes et consommables médicaux gratuits, 1 paquet par chargeur
//     quelle que soit sa capacité (une boîte de 100 = 1), 5 par roquette, 2 par grenade ;
//     soute insuffisante = objet repris et joueur prévenu
//   - objet retiré ET détruit dans une zone d'arsenal : remboursé ; simplement posé au sol : rien
//   - déplacement d'un objet à l'intérieur de son propre inventaire : ni facturé ni remboursé
//   - loin de toute zone (loot sur le terrain) : rien
//   - certifications par type d'arme, partout : lance-roquettes → AT, mitrailleuse → MG,
//     fusil de précision → TP. Verrou souple : journal + alerte officiers, une alerte par minute
//     et par type. Le kit de base et la restauration au spawn sont exemptés.
//   - la facturation est réglée 400 ms après l'ajout, une fois les remboursements en attente
//     passés (un loadout Bacon remplace le personnage : nouveaux objets facturés, anciens remboursés)
//   - une ligne de journal par bilan, pas par objet
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

//! Marqueur à poser sur les caisses d'arsenal : tout ce qui est pris/rendu dans le rayon est facturé/remboursé
[ComponentEditorProps(category: "SimpleRP", description: "Zone d'arsenal SimpleRP : objets pris/rendus à proximité facturés/remboursés en munitions")]
class SRP_ArsenalZoneComponentClass : ScriptComponentClass
{
}

class SRP_ArsenalZoneComponent : ScriptComponent
{
	[Attribute("6", UIWidgets.EditBox, "Rayon de la zone, en mètres", category: "SimpleRP")]
	protected float m_fRadius;

	protected static ref array<SRP_ArsenalZoneComponent> s_aZones = {};

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
	float GetRadius()
	{
		return m_fRadius;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsInAnyZone(vector position)
	{
		foreach (SRP_ArsenalZoneComponent zone : s_aZones)
		{
			if (!zone)
				continue;

			IEntity owner = zone.GetOwner();
			if (!owner)
				continue;

			if (vector.Distance(owner.GetOrigin(), position) <= zone.GetRadius())
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static int Count()
	{
		return s_aZones.Count();
	}
}

//------------------------------------------------------------------------------------------------
//! Composant du game mode : coûts, facturation, remboursement, certifications
[ComponentEditorProps(category: "SimpleRP", description: "Économie d'arsenal SimpleRP : coûts en paquets de munitions par type d'objet, certifications par type d'arme")]
class SRP_ArsenalEconomyComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_ArsenalEconomyComponent : SCR_BaseGameModeComponent
{
	[Attribute("0", UIWidgets.EditBox, "Coût d'un fusil, en paquets de munitions (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostRifle;

	[Attribute("0", UIWidgets.EditBox, "Coût d'un pistolet, en paquets (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostHandgun;

	[Attribute("0", UIWidgets.EditBox, "Coût d'une mitrailleuse, en paquets (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostMachineGun;

	[Attribute("0", UIWidgets.EditBox, "Coût d'un fusil de précision, en paquets (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostSniper;

	[Attribute("0", UIWidgets.EditBox, "Coût d'un lance-roquettes, en paquets (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostRocketLauncher;

	[Attribute("0", UIWidgets.EditBox, "Coût d'un lance-grenades, en paquets (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostGrenadeLauncher;

	[Attribute("1", UIWidgets.EditBox, "Coût d'un chargeur, en paquets, quelle que soit sa capacité (une boîte de 100 = 1)", category: "SimpleRP - Coûts")]
	protected int m_iCostMagazine;

	[Attribute("5", UIWidgets.EditBox, "Coût d'une roquette ou d'une munition unitaire (chargeur d'un coup), en paquets", category: "SimpleRP - Coûts")]
	protected int m_iCostRocket;

	[Attribute("2", UIWidgets.EditBox, "Coût d'une grenade, en paquets", category: "SimpleRP - Coûts")]
	protected int m_iCostGrenade;

	[Attribute("0", UIWidgets.EditBox, "Coût d'un consommable médical, en paquets (0 = gratuit)", category: "SimpleRP - Coûts")]
	protected int m_iCostMedical;

	[Attribute("60", UIWidgets.EditBox, "Délai minimum entre deux alertes pour le même joueur et la même certification, en secondes", category: "SimpleRP")]
	protected int m_iAlertCooldownSeconds;

	[Attribute("FR", UIWidgets.EditBox, "Faction des caisses d'arsenal quand leur affiliation n'est pas connue (cas du client en serveur dédié)", category: "SimpleRP")]
	protected string m_sArsenalFactionKey;

	protected static SRP_ArsenalEconomyComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	//! Clé de la faction de repli des arsenaux : celle du composant, sinon FR
	static string GetArsenalFactionKey()
	{
		if (s_Instance && !s_Instance.m_sArsenalFactionKey.IsEmpty())
			return s_Instance.m_sArsenalFactionKey;
		return "FR";
	}

	protected ref set<int> m_SuppressedPlayers = new set<int>();					// joueurs exemptés (spawn en cours)
	protected ref map<IEntity, int> m_mRecentlyRemoved = new map<IEntity, int>();	// objet -> tick du retrait (détection des déplacements internes)
	protected ref map<string, int> m_mLastAlert = new map<string, int>();				// "playerId:certif" -> tick de la dernière alerte

	// Regroupement des messages : un seul bilan par joueur et par seconde
	protected ref map<int, int> m_mPendingCost = new map<int, int>();
	protected ref map<int, int> m_mPendingCount = new map<int, int>();
	protected ref map<int, int> m_mPendingRefund = new map<int, int>();
	protected ref map<int, int> m_mPendingRefundCount = new map<int, int>();
	protected ref set<int> m_FlushScheduled = new set<int>();

	//------------------------------------------------------------------------------------------------
	static SRP_ArsenalEconomyComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;

		if (Replication.IsServer())
			SRP_JournalComponent.Log("ARSENAL", "Économie d'arsenal démarrée");
	}

	//------------------------------------------------------------------------------------------------
	//! Exempter un joueur de facturation (kit de base, restauration d'inventaire)
	void SetChargesSuppressed(int playerId, bool suppressed)
	{
		if (suppressed)
			m_SuppressedPlayers.Insert(playerId);
		else
			m_SuppressedPlayers.RemoveItem(playerId);
	}

	//------------------------------------------------------------------------------------------------
	// Points d'entrée appelés par le crochet d'inventaire (classe moddée plus bas)
	//------------------------------------------------------------------------------------------------
	static void NotifyItemAdded(IEntity owner, IEntity item)
	{
		if (!s_Instance || !Replication.IsServer() || !owner || !item)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
		if (playerId <= 0)
			return;

		s_Instance.OnPlayerItemAdded(playerId, owner, item);
	}

	//------------------------------------------------------------------------------------------------
	static void NotifyItemRemoved(IEntity owner, IEntity item)
	{
		if (!s_Instance || !Replication.IsServer() || !owner || !item)
			return;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner);
		if (playerId <= 0)
			return;

		s_Instance.OnPlayerItemRemoved(playerId, owner, item);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerItemAdded(int playerId, IEntity character, IEntity item)
	{
		int now = System.GetTickCount();

		// Déplacement à l'intérieur de son propre inventaire : rien à faire
		int removedTick;
		if (m_mRecentlyRemoved.Find(item, removedTick))
		{
			m_mRecentlyRemoved.Remove(item);
			if (now - removedTick < 1000)
				return;
		}

		// Kit de base ou restauration en cours : ni certification ni facturation
		if (m_SuppressedPlayers.Contains(playerId))
			return;

		bool inZone = SRP_ArsenalZoneComponent.IsInAnyZone(character.GetOrigin());

		// 1. Certification (verrou souple) : les armes partout, le matériel spécialisé seulement à l'arsenal
		CheckCertification(playerId, item, now, inZone);

		// 2. Facturation, seulement dans une zone d'arsenal, réglée après les remboursements en attente
		if (!inZone)
			return;

		string label;
		int cost = GetItemCost(item, label);
		if (cost <= 0)
			return;

		GetGame().GetCallqueue().CallLater(SettleCharge, 400, false, playerId, item, cost, label);
	}

	//------------------------------------------------------------------------------------------------
	//! Règle la facturation d'un objet ajouté 400 ms plus tôt : facturé s'il est toujours dans
	//! l'inventaire et que la soute suffit, repris sinon
	protected void SettleCharge(int playerId, IEntity item, int cost, string label)
	{
		if (!item || item.IsDeleted())
			return;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return;

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory || !inventory.Contains(item))
			return;

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (!resources.Take(SRP_EResource.MUNITIONS, cost, string.Format("arsenal : %1 pour %2", label, playerName), false))
		{
			inventory.TryDeleteItem(item);
			SRP_Utils.NotifyPlayer(playerId, string.Format("Munitions insuffisantes pour %1 (%2) — %3", label, cost, resources.GetStockText(SRP_EResource.MUNITIONS)));
			SRP_JournalComponent.Log("ARSENAL", string.Format("%1 : %2 repris, munitions insuffisantes (%3) — %4", playerName, label, cost, resources.GetStockText(SRP_EResource.MUNITIONS)));
			return;
		}

		AddPending(playerId, cost, 1, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPlayerItemRemoved(int playerId, IEntity character, IEntity item)
	{
		int now = System.GetTickCount();
		m_mRecentlyRemoved.Set(item, now);

		if (m_SuppressedPlayers.Contains(playerId))
			return;
		if (!SRP_ArsenalZoneComponent.IsInAnyZone(character.GetOrigin()))
			return;

		string label;
		int cost = GetItemCost(item, label);
		if (cost <= 0)
			return;

		// On ne rembourse que si l'objet est détruit (repris par l'arsenal), pas s'il est posé au sol
		GetGame().GetCallqueue().CallLater(CheckRefund, 300, false, playerId, item, cost, label);
	}

	//------------------------------------------------------------------------------------------------
	protected void CheckRefund(int playerId, IEntity item, int cost, string label)
	{
		if (item && !item.IsDeleted())
		{
			// Toujours là : posé au sol ou déplacé, pas de remboursement
			m_mRecentlyRemoved.Remove(item);
			return;
		}

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		int refunded = resources.Add(SRP_EResource.MUNITIONS, cost, string.Format("retour arsenal : %1 par %2", label, playerName), false);
		if (refunded > 0)
			AddPending(playerId, refunded, 1, true);
	}

	//------------------------------------------------------------------------------------------------
	// Messages regroupés
	//------------------------------------------------------------------------------------------------
	protected void AddPending(int playerId, int amount, int count, bool refund)
	{
		int current;
		if (refund)
		{
			m_mPendingRefund.Find(playerId, current);
			m_mPendingRefund.Set(playerId, current + amount);
			current = 0;
			m_mPendingRefundCount.Find(playerId, current);
			m_mPendingRefundCount.Set(playerId, current + count);
		}
		else
		{
			m_mPendingCost.Find(playerId, current);
			m_mPendingCost.Set(playerId, current + amount);
			current = 0;
			m_mPendingCount.Find(playerId, current);
			m_mPendingCount.Set(playerId, current + count);
		}

		if (!m_FlushScheduled.Contains(playerId))
		{
			m_FlushScheduled.Insert(playerId);
			GetGame().GetCallqueue().CallLater(FlushPending, 1000, false, playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FlushPending(int playerId)
	{
		m_FlushScheduled.RemoveItem(playerId);

		int cost, count, refund, refundCount;
		m_mPendingCost.Find(playerId, cost);
		m_mPendingCount.Find(playerId, count);
		m_mPendingRefund.Find(playerId, refund);
		m_mPendingRefundCount.Find(playerId, refundCount);
		m_mPendingCost.Remove(playerId);
		m_mPendingCount.Remove(playerId);
		m_mPendingRefund.Remove(playerId);
		m_mPendingRefundCount.Remove(playerId);

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources)
			return;

		string text = "";
		if (count > 0)
			text += string.Format("Arsenal : %1 objet(s), -%2 munitions", count, cost);
		if (refundCount > 0)
		{
			if (!text.IsEmpty())
				text += " ; ";
			text += string.Format("retour : %1 objet(s), +%2 munitions", refundCount, refund);
		}
		if (text.IsEmpty())
			return;

		string stock = resources.GetStockText(SRP_EResource.MUNITIONS);
		SRP_JournalComponent.Log("ARSENAL", string.Format("%1 : %2 -> %3", GetGame().GetPlayerManager().GetPlayerName(playerId), text, stock));
		SRP_Utils.NotifyPlayer(playerId, text + "\n" + stock);
	}

	//------------------------------------------------------------------------------------------------
	// Certifications
	//------------------------------------------------------------------------------------------------
	protected void CheckCertification(int playerId, IEntity item, int now, bool inZone)
	{
		int certif = GetRequiredCertif(item);
		if (certif == 0 && inZone)
			certif = GetRequiredCertifForGear(item);
		if (certif == 0)
			return;

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;

		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record || record.m_bStaff || SRP_Certifs.Has(record.m_iCertifs, certif))
			return;

		string key = string.Format("%1:%2", playerId, certif);
		int last;
		if (m_mLastAlert.Find(key, last) && now - last < m_iAlertCooldownSeconds * 1000)
			return;
		m_mLastAlert.Set(key, now);

		string label;
		GetItemCost(item, label);
		string certifName = SRP_Certifs.GetName(certif);

		players.LogInfraction(playerId, string.Format("prise en main sans certification %1 : %2", certifName, label));
		players.NotifyOfficiers(string.Format("%1 a pris %2 sans la certification %3", record.m_sName, label, certifName));
		SRP_Utils.NotifyPlayer(playerId, string.Format("Vous n'êtes pas habilité (%1) : %2. Prise en main d'urgence enregistrée.", certifName, label));
	}

	//------------------------------------------------------------------------------------------------
	//! Certification requise par le type d'arme, 0 sinon
	int GetRequiredCertif(IEntity item)
	{
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (!weapon)
			return 0;

		switch (weapon.GetWeaponType())
		{
			case EWeaponType.WT_ROCKETLAUNCHER: return SRP_ECertif.AT;
			case EWeaponType.WT_MACHINEGUN: return SRP_ECertif.MG;
			case EWeaponType.WT_SNIPERRIFLE: return SRP_ECertif.TP;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Certification requise par le matériel spécialisé (ACE), d'après le nom du prefab ; 0 sinon.
	//! Ne joue qu'à l'arsenal : ramasser sur le terrain le kit d'un soignant tombé n'est pas une faute.
	int GetRequiredCertifForGear(IEntity item)
	{
		EntityPrefabData prefabData = item.GetPrefabData();
		if (!prefabData)
			return 0;
		string name = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());

		// Médecin : gestes invasifs et médicaments du cœur
		if (name.Contains("ACE_Medical_KingLT") || name.Contains("ACE_Medical_NCDKit") || name.Contains("ACE_Medical_Metoprolol") || name.Contains("ACE_Medical_Phenylephrine") || name.Contains("MedicalKit_01"))
			return SRP_ECertif.MEDECIN;
		// Infirmier : arrêt cardiaque, réveil, oxygène, antidote de la morphine (la solution saline est libre : chaque soldat porte la sienne)
		if (name.Contains("ACE_Medical_Naloxone") || name.Contains("ACE_Medical_AmmoniumCarbonate") || name.Contains("ACE_Medical_OxygenMask") || name.Contains("ACE_Medical_Epinephrine"))
			return SRP_ECertif.MEDIC;
		// Sapeur : détection, explosifs (Hexomax, pétard de tolite) et mise à feu
		if (name.Contains("ACE_MineDetector") || name.Contains("ACE_DeadManSwitch"))
			return SRP_ECertif.GENIE;
		if (name.Contains("DemoBlock_M112") || name.Contains("DemoBlock_TSh400g") || name.Contains("BlastingMachine_M34"))
			return SRP_ECertif.GENIE;
		// Tireur de précision : tables de tir et anémomètre
		if (name.Contains("ACE_BallisticTable") || name.Contains("WeatherMeter_Kestrel"))
			return SRP_ECertif.TP;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Coût en paquets de munitions d'un objet selon son type. 0 = gratuit : par défaut les armes, le médical,
	//! et tout ce qui n'est ni chargeur ni grenade (tenues, sacs, radios, optiques...)
	int GetItemCost(IEntity item, out string label)
	{
		label = "objet";
		// Un paquet de ravitaillement n'est pas un article d'arsenal : jamais facturé, jamais remboursé
		if (item && item.FindComponent(SRP_PacketComponent))
			return 0;
		EntityPrefabData prefabData = item.GetPrefabData();
		if (prefabData)
			label = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());

		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(item.FindComponent(BaseWeaponComponent));
		if (weapon)
		{
			switch (weapon.GetWeaponType())
			{
				case EWeaponType.WT_RIFLE: return m_iCostRifle;
				case EWeaponType.WT_HANDGUN: return m_iCostHandgun;
				case EWeaponType.WT_MACHINEGUN: return m_iCostMachineGun;
				case EWeaponType.WT_SNIPERRIFLE: return m_iCostSniper;
				case EWeaponType.WT_ROCKETLAUNCHER: return m_iCostRocketLauncher;
				case EWeaponType.WT_GRENADELAUNCHER: return m_iCostGrenadeLauncher;
				case EWeaponType.WT_FRAGGRENADE: return m_iCostGrenade;
				case EWeaponType.WT_SMOKEGRENADE: return m_iCostGrenade;
			}
			return 0;
		}

		MagazineComponent magazine = MagazineComponent.Cast(item.FindComponent(MagazineComponent));
		if (magazine)
		{
			// Chargeur d'un coup (roquette, grenade de 40 mm) : tarif roquette ; sinon un chargeur = un paquet, boîte de 100 comprise
			if (magazine.GetMaxAmmoCount() <= 1)
				return m_iCostRocket;
			return m_iCostMagazine;
		}

		SCR_ConsumableItemComponent consumable = SCR_ConsumableItemComponent.Cast(item.FindComponent(SCR_ConsumableItemComponent));
		if (consumable)
			return m_iCostMedical;

		return 0;
	}
}

//------------------------------------------------------------------------------------------------
//! Crochet sur l'inventaire des personnages : chaque ajout/retrait remonte à l'économie d'arsenal
modded class SCR_InventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! Ce gestionnaire est celui des personnages : sa logique vanilla suppose un CharacterInventoryStorageComponent
	//! et plante (NULL pointer) sur un conteneur (caisse, dépôt). Sur un conteneur, on saute la logique vanilla
	//! et on ne garde que nos crochets ; sur un personnage, rien ne change.
	protected bool SRP_IsCharacter()
	{
		IEntity owner = GetOwner();
		return owner && owner.FindComponent(CharacterInventoryStorageComponent);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		if (!SRP_IsCharacter())
		{
			SRP_MissionManagerComponent.NotifyItemAdded(GetOwner(), item);
			if (SRP_DepotComponent.Of(GetOwner()) && SRP_PacketComponent.Of(item))
				SRP_ResourceManagerComponent.MarkDirty();
			return;
		}
		super.OnItemAdded(storageOwner, item);
		SRP_ArsenalEconomyComponent.NotifyItemAdded(GetOwner(), item);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		if (!SRP_IsCharacter())
		{
			if (SRP_DepotComponent.Of(GetOwner()) && SRP_PacketComponent.Of(item))
				SRP_ResourceManagerComponent.MarkDirty();
			return;
		}
		super.OnItemRemoved(storageOwner, item);
		SRP_ArsenalEconomyComponent.NotifyItemRemoved(GetOwner(), item);
	}
}

//------------------------------------------------------------------------------------------------
//! Caisses d'arsenal : sur un client de serveur dédié, la faction affiliée par défaut de la caisse
//! revient nulle (vérifié dans le journal client : « faction aucune ») et la liste d'objets se rabat sur
//! le catalogue sans faction, quasi vide. On complète : faction du composant d'économie, sinon celle
//! du joueur local. Le vanilla et Bacon passent tous deux par GetAssignedFaction().
modded class SCR_ArsenalComponent
{
	//------------------------------------------------------------------------------------------------
	override SCR_Faction GetAssignedFaction()
	{
		SCR_Faction faction = super.GetAssignedFaction();
		if (faction)
			return faction;

		FactionManager manager = GetGame().GetFactionManager();
		if (!manager)
			return null;

		faction = SCR_Faction.Cast(manager.GetFactionByKey(SRP_ArsenalEconomyComponent.GetArsenalFactionKey()));
		if (faction)
			return faction;

		return SCR_Faction.Cast(SCR_FactionManager.SGetLocalPlayerFaction());
	}
}
