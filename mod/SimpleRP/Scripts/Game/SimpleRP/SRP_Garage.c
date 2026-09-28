//------------------------------------------------------------------------------------------------
// SimpleRP — Garage : citerne, atelier, panneau du parc
// Actions physiques à déclarer dans l'ActionsManagerComponent des prefabs de dépôt :
//   - SRP_RefuelAction (citerne) : plein du véhicule le plus proche, déduit du carburant
//   - SRP_FillJerrycansAction (citerne) : remplit les jerricans de l'inventaire du joueur
//   - SRP_RepairAction (atelier) : réparation du véhicule le plus proche, coût en pièces selon
//     les dégâts, certification Génie en verrou souple. Un véhicule détruit n'est pas réparable.
//   - SRP_FleetSignAction (atelier ou panneau) : état du parc auto
// L'achat de véhicules et de caisses passe par le terminal logistique (SRP_Treasury.c, SRP_TerminalMenu.c).
// Phase 3 — étape C
//------------------------------------------------------------------------------------------------

//! Outils communs : véhicule le plus proche, vérifications
class SRP_GarageUtils
{
	protected static ref array<IEntity> s_aQuery = {};

	//------------------------------------------------------------------------------------------------
	protected static bool QueryAddVehicle(IEntity e)
	{
		if (Vehicle.Cast(e))
			s_aQuery.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Véhicule (Vehicle) le plus proche d'un point, dans un rayon donné
	static IEntity FindNearestVehicle(vector center, float radius)
	{
		s_aQuery.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius, QueryAddVehicle, null, EQueryEntitiesFlags.ALL);

		IEntity best;
		float bestDistance = radius + 1;
		foreach (IEntity candidate : s_aQuery)
		{
			if (!candidate || candidate.IsDeleted())
				continue;
			float distance = vector.Distance(center, candidate.GetOrigin());
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = candidate;
			}
		}
		return best;
	}

	protected static ref array<IEntity> s_aDepotQuery = {};

	//------------------------------------------------------------------------------------------------
	protected static bool QueryAddDepot(IEntity e)
	{
		if (e.FindComponent(SRP_DepotComponent))
			s_aDepotQuery.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le personnage est-il à moins de radius m d'un dépôt de cette ressource ?
	static bool IsNearDepot(IEntity character, int resource, float radius)
	{
		if (!character)
			return false;

		s_aDepotQuery.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(character.GetOrigin(), radius, QueryAddDepot, null, EQueryEntitiesFlags.ALL);

		foreach (IEntity candidate : s_aDepotQuery)
		{
			SRP_DepotComponent depot = SRP_DepotComponent.Cast(candidate.FindComponent(SRP_DepotComponent));
			if (depot && depot.GetResource() == resource)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom lisible d'un véhicule : fiche de flotte, sinon nom court du prefab
	static string VehicleName(IEntity vehicle)
	{
		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (fleet)
		{
			SRP_FleetVehicle record = fleet.FindByEntity(vehicle);
			if (record)
				return string.Format("%1 %2", record.m_sId, record.m_sName);
		}

		EntityPrefabData prefabData = vehicle.GetPrefabData();
		if (prefabData)
			return SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
		return "véhicule";
	}

	//------------------------------------------------------------------------------------------------
	static bool IsDestroyed(IEntity vehicle)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}
}

//------------------------------------------------------------------------------------------------
//! Citerne : plein du véhicule le plus proche
class SRP_RefuelAction : ScriptedUserAction
{
	[Attribute("10", UIWidgets.EditBox, "Rayon de recherche du véhicule autour de la citerne, en mètres", category: "SimpleRP")]
	protected float m_fRadius;

	[Attribute("10", UIWidgets.EditBox, "Litres de carburant pour une unité de stock", category: "SimpleRP")]
	protected float m_fLitersPerUnit;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources)
			return;

		IEntity vehicle = SRP_GarageUtils.FindNearestVehicle(pOwnerEntity.GetOrigin(), m_fRadius);
		if (!vehicle)
		{
			SRP_Utils.NotifyPlayer(playerId, string.Format("Aucun véhicule à moins de %1 m de la citerne", Math.Round(m_fRadius)));
			return;
		}

		array<SCR_FuelManagerComponent> managers = {};
		if (SCR_FuelManagerComponent.GetAllFuelManagers(vehicle, managers) <= 0)
		{
			SRP_Utils.NotifyPlayer(playerId, "Ce véhicule n'a pas de réservoir");
			return;
		}

		float total, max, percentage;
		SCR_FuelManagerComponent.GetTotalValuesOfFuelNodesOfFuelManagers(managers, total, max, percentage);
		float missing = max - total;
		if (missing < 1)
		{
			SRP_Utils.NotifyPlayer(playerId, SRP_GarageUtils.VehicleName(vehicle) + " : réservoir déjà plein");
			return;
		}

		float litersPerUnit = m_fLitersPerUnit;
		if (litersPerUnit < 1)
			litersPerUnit = 1;

		int wanted = Math.Ceil(missing / litersPerUnit);
		int taken = resources.TakeUpTo(SRP_EResource.CARBURANT, wanted, string.Format("plein de %1 par %2", SRP_GarageUtils.VehicleName(vehicle), GetGame().GetPlayerManager().GetPlayerName(playerId)));
		if (taken <= 0)
		{
			SRP_Utils.NotifyPlayer(playerId, "Citerne vide — " + resources.GetStockText(SRP_EResource.CARBURANT));
			return;
		}

		float liters = taken * litersPerUnit;
		if (liters > missing)
			liters = missing;

		float newPercentage = (total + liters) / max;
		if (newPercentage > 1)
			newPercentage = 1;
		SCR_FuelManagerComponent.SetTotalFuelPercentageOfFuelManagers(managers, newPercentage);

		SRP_Utils.NotifyPlayer(playerId, string.Format("%1 : +%2 L (-%3 carburant)\n%4", SRP_GarageUtils.VehicleName(vehicle), Math.Round(liters), taken, resources.GetStockText(SRP_EResource.CARBURANT)));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Faire le plein du véhicule";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Citerne : remplit les jerricans de l'inventaire du joueur
class SRP_FillJerrycansAction : ScriptedUserAction
{
	[Attribute("10", UIWidgets.EditBox, "Litres de carburant pour une unité de stock", category: "SimpleRP")]
	protected float m_fLitersPerUnit;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources)
			return;

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(pUserEntity.FindComponent(InventoryStorageManagerComponent));
		if (!inventory)
			return;

		float litersPerUnit = m_fLitersPerUnit;
		if (litersPerUnit < 1)
			litersPerUnit = 1;

		array<IEntity> items = {};
		inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);

		int jerrycans = 0;
		float litersFilled = 0;
		int unitsTaken = 0;
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		foreach (IEntity item : items)
		{
			if (!item)
				continue;

			array<SCR_FuelManagerComponent> managers = {};
			if (SCR_FuelManagerComponent.GetAllFuelManagers(item, managers) <= 0)
				continue;

			float total, max, percentage;
			SCR_FuelManagerComponent.GetTotalValuesOfFuelNodesOfFuelManagers(managers, total, max, percentage);
			float missing = max - total;
			if (missing < 0.5)
				continue;

			int wanted = Math.Ceil(missing / litersPerUnit);
			int taken = resources.TakeUpTo(SRP_EResource.CARBURANT, wanted, string.Format("jerrican de %1", playerName));
			if (taken <= 0)
				break;

			float liters = taken * litersPerUnit;
			if (liters > missing)
				liters = missing;

			float newPercentage = (total + liters) / max;
			if (newPercentage > 1)
				newPercentage = 1;
			SCR_FuelManagerComponent.SetTotalFuelPercentageOfFuelManagers(managers, newPercentage);

			jerrycans++;
			litersFilled += liters;
			unitsTaken += taken;
		}

		if (jerrycans == 0)
		{
			if (resources.GetStock(SRP_EResource.CARBURANT) <= 0)
				SRP_Utils.NotifyPlayer(playerId, "Citerne vide — " + resources.GetStockText(SRP_EResource.CARBURANT));
			else
				SRP_Utils.NotifyPlayer(playerId, "Aucun jerrican à remplir dans votre inventaire");
			return;
		}

		SRP_Utils.NotifyPlayer(playerId, string.Format("%1 jerrican(s) rempli(s) : +%2 L (-%3 carburant)\n%4", jerrycans, Math.Round(litersFilled), unitsTaken, resources.GetStockText(SRP_EResource.CARBURANT)));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Remplir mes jerricans";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Atelier : réparation du véhicule le plus proche, coût en pièces selon les dégâts
class SRP_RepairAction : ScriptedUserAction
{
	[Attribute("12", UIWidgets.EditBox, "Rayon de recherche du véhicule autour de l'atelier, en mètres", category: "SimpleRP")]
	protected float m_fRadius;

	[Attribute("30", UIWidgets.EditBox, "Pièces pour remettre à neuf un véhicule entièrement endommagé", category: "SimpleRP")]
	protected int m_iPiecesFullRepair;

	[Attribute("1", UIWidgets.CheckBox, "Alerter quand la réparation est faite sans certification Génie (verrou souple)", category: "SimpleRP")]
	protected bool m_bRequireGenie;

	protected static ref map<int, int> s_mLastAlert = new map<int, int>();

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!resources || !players)
			return;

		IEntity vehicle = SRP_GarageUtils.FindNearestVehicle(pOwnerEntity.GetOrigin(), m_fRadius);
		if (!vehicle)
		{
			SRP_Utils.NotifyPlayer(playerId, string.Format("Aucun véhicule à moins de %1 m de l'atelier", Math.Round(m_fRadius)));
			return;
		}

		string vehicleName = SRP_GarageUtils.VehicleName(vehicle);
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		if (!damage)
		{
			SRP_Utils.NotifyPlayer(playerId, vehicleName + " : rien à réparer");
			return;
		}

		if (damage.GetState() == EDamageState.DESTROYED)
		{
			SRP_Utils.NotifyPlayer(playerId, vehicleName + " : détruit, irréparable. Un officier peut en acheter un autre au terminal logistique.");
			return;
		}

		// Dégât moyen sur les zones (0 = neuf, 1 = tout à zéro)
		array<HitZone> zones = {};
		damage.GetAllHitZones(zones);
		float damageSum = 0;
		int count = 0;
		foreach (HitZone zone : zones)
		{
			damageSum += 1 - zone.GetHealthScaled();
			count++;
		}
		if (count == 0 || damageSum < 0.01)
		{
			SRP_Utils.NotifyPlayer(playerId, vehicleName + " : en bon état, rien à réparer");
			return;
		}

		float ratio = damageSum / count;
		int cost = Math.Ceil(ratio * m_iPiecesFullRepair);
		if (cost < 1)
			cost = 1;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (!resources.Take(SRP_EResource.PIECES, cost, string.Format("réparation de %1 par %2", vehicleName, playerName)))
		{
			SRP_Utils.NotifyPlayer(playerId, string.Format("Pièces insuffisantes : réparation à %1 — %2", cost, resources.GetStockText(SRP_EResource.PIECES)));
			return;
		}

		damage.FullHeal();
		SRP_JournalComponent.Log("ATELIER", string.Format("%1 réparé par %2 (%3 pièces, dégâts %4 %%)", vehicleName, playerName, cost, Math.Round(ratio * 100)));
		SRP_Utils.NotifyPlayer(playerId, string.Format("%1 réparé (-%2 pièces)\n%3", vehicleName, cost, resources.GetStockText(SRP_EResource.PIECES)));

		// Verrou souple Génie
		if (!m_bRequireGenie)
			return;

		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record || record.m_bStaff || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.GENIE))
			return;

		int now = System.GetTickCount();
		int last;
		if (s_mLastAlert.Find(playerId, last) && now - last < 60000)
			return;
		s_mLastAlert.Set(playerId, now);

		players.LogInfraction(playerId, "réparation à l'atelier sans certification Génie : " + vehicleName);
		players.NotifyOfficiers(string.Format("%1 a réparé %2 sans la certification Génie", record.m_sName, vehicleName));
		SRP_Utils.NotifyPlayer(playerId, "Vous n'êtes pas habilité (Génie). Réparation d'urgence enregistrée.");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Réparer le véhicule";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Panneau du parc auto : état de la flotte
class SRP_FleetSignAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (!fleet)
		{
			SRP_Utils.NotifyPlayer(playerId, "Gestionnaire de flotte absent du game mode");
			return;
		}

		SRP_Utils.HintPlayer(playerId, "Panneau — Parc auto", fleet.GetStatusText());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Consulter le parc auto";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}
