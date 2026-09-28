//------------------------------------------------------------------------------------------------
// SimpleRP — Logique de spawn
// Fait apparaître chaque joueur en soldat français (prefab configurable) : à sa dernière
// position sauvegardée s'il en a une, sinon au point de spawn de la base.
// À sélectionner dans GameMode_Plain > SCR_RespawnSystemComponent > Spawn Logic.
// Nécessite le composant SCR_FreeSpawnHandlerComponent sur le game mode.
// Calqué sur SCR_AutoSpawnLogic (vanilla), sans faction aléatoire ni loadout manager.
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

[BaseContainerProps(category: "Respawn")]
class SRP_SpawnLogic : SCR_SpawnLogic
{
	[Attribute("FR", UIWidgets.EditBox, "Clé de la faction des joueurs (FR pour AMF)", category: "SimpleRP")]
	protected FactionKey m_sFactionKey;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab du personnage joueur (kit de base)", "et", category: "SimpleRP")]
	protected ResourceName m_sCharacterPrefab;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Nom du marqueur de spawn de la base (Object Properties > Name). Les marqueurs de même nom suivis de 1, 2, 3… (SRP_SpawnBase1…) sont des points supplémentaires ; le premier sans joueur à côté est choisi", category: "SimpleRP")]
	protected string m_sSpawnEntityName;

	[Attribute("1", UIWidgets.CheckBox, "Reprendre à la dernière position sauvegardée", category: "SimpleRP")]
	protected bool m_bUseSavedPosition;

	[Attribute("", UIWidgets.ResourceNamePicker, "Kit de base : objets donnés à chaque spawn frais (fusil, chargeurs, pansements...). Un même prefab peut apparaître plusieurs fois.", "et", category: "SimpleRP")]
	protected ref array<ResourceName> m_aKitDeBase;

	[Attribute("1", UIWidgets.CheckBox, "Restaurer l'inventaire sauvegardé à la reconnexion (sinon kit de base à chaque connexion)", category: "SimpleRP")]
	protected bool m_bRestoreInventory;

	[Attribute("10", UIWidgets.EditBox, "Délai de respawn après une mort, en secondes", category: "SimpleRP")]
	protected int m_iRespawnDelay;

	[Attribute("60", UIWidgets.EditBox, "Délai de respawn quand l'intendance est vide (vivres à zéro), en secondes", category: "SimpleRP")]
	protected int m_iRespawnDelayNoVivres;

	protected ref map<int, bool> m_mRestoreOnSpawn = new map<int, bool>();	// playerId -> restaurer l'inventaire sauvegardé ?

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess_S(int playerId)
	{
		super.OnPlayerAuditSuccess_S(playerId);
		ExcuteInitialLoadOrSpawn_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerEntityLost_S(int playerId)
	{
		super.OnPlayerEntityLost_S(playerId);

		// Position du corps, pour la caméra de mort (avant d'effacer la position sauvegardée)
		SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();
		vector bodyPosition;
		bool hasBodyPosition = false;

		IEntity body = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (body)
		{
			bodyPosition = body.GetOrigin();
			hasBodyPosition = true;
		}
		else if (manager)
		{
			SRP_PlayerRecord record = manager.GetRecord(playerId);
			if (record && record.m_bHasPosition)
			{
				bodyPosition = record.GetPosition();
				hasBodyPosition = true;
			}
		}

		// Personnage perdu (mort, suppression) : la position sauvegardée est effacée
		if (manager)
			manager.OnPlayerEntityLost(playerId);

		// Coût en vivres et délai de respawn (allongé si l'intendance est vide)
		int delay = m_iRespawnDelay;
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (resources)
		{
			resources.OnRespawn(GetGame().GetPlayerManager().GetPlayerName(playerId));
			if (resources.IsEmpty(SRP_EResource.VIVRES))
				delay = m_iRespawnDelayNoVivres;
		}
		if (delay < 1)
			delay = 1;

		SRP_Utils.NotifyPlayer(playerId, string.Format("Réapparition à la base dans %1 s", delay));

		// Caméra qui tourne autour du corps pendant l'attente
		if (hasBodyPosition)
		{
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (pc)
				pc.SRP_StartDeathCamera(bodyPosition, delay);
		}

		GetGame().GetCallqueue().CallLater(DoSpawn_S, delay * 1000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerSpawnFailed_S(int playerId)
	{
		super.OnPlayerSpawnFailed_S(playerId);
		Print(string.Format("[SRP] Spawn échoué pour le joueur %1, nouvel essai dans 2 s", playerId), LogLevel.WARNING);

		// Nouvel essai dans 2 secondes
		GetGame().GetCallqueue().CallLater(DoSpawn_S, 2000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	override protected void DoSpawn_S(int playerId)
	{
		// 0. Joueur toujours connecté ? (un spawn différé peut arriver après sa déconnexion)
		if (!GetGame().GetPlayerManager().GetPlayerController(playerId))
			return;

		// 1. Faction
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(m_sFactionKey);
		if (!faction)
		{
			Print(string.Format("[SRP] Faction '%1' introuvable dans le FactionManager du monde", m_sFactionKey), LogLevel.ERROR);
			OnPlayerSpawnFailed_S(playerId);
			return;
		}

		auto factionComp = GetPlayerFactionComponent_S(playerId);
		if (factionComp && factionComp.GetAffiliatedFaction() != faction)
			factionComp.RequestFaction(faction);

		// 2. Personnage
		if (m_sCharacterPrefab.IsEmpty())
		{
			Print("[SRP] Aucun prefab de personnage configuré dans SRP_SpawnLogic (Spawn Logic du game mode)", LogLevel.ERROR);
			OnPlayerSpawnFailed_S(playerId);
			return;
		}

		// 3. Position
		vector position;
		if (!GetSpawnPosition(playerId, position))
		{
			Print(string.Format("[SRP] Point de spawn introuvable : aucune entité nommée '%1' et aucun SCR_SpawnPoint pour la faction '%2'", m_sSpawnEntityName, m_sFactionKey), LogLevel.ERROR);
			OnPlayerSpawnFailed_S(playerId);
			return;
		}

		// 4. Demande de spawn libre (prefab + position), traitée par SCR_FreeSpawnHandlerComponent
		SCR_FreeSpawnData data = new SCR_FreeSpawnData(m_sCharacterPrefab, position);

		auto respawn = GetPlayerRespawnComponent_S(playerId);
		if (!respawn || !respawn.CanSpawn(data))
		{
			Print(string.Format("[SRP] Spawn refusé pour le joueur %1 (SCR_FreeSpawnHandlerComponent présent sur le game mode ?)", playerId), LogLevel.WARNING);
			OnPlayerSpawnFailed_S(playerId);
			return;
		}

		// Reconnexion sur une position sauvegardée = on rend l'inventaire sauvegardé ;
		// spawn frais (première connexion, après une mort) = kit de base
		bool restore = false;
		if (m_bRestoreInventory)
		{
			SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();
			if (manager)
			{
				SRP_PlayerRecord record = manager.GetRecord(playerId);
				restore = record && record.m_bHasPosition && record.m_sInventory.StartsWith("{");
			}
		}
		m_mRestoreOnSpawn.Set(playerId, restore);

		IEntity previous = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		respawn.RequestSpawn(data);
		Print(string.Format("[SRP] Spawn demandé pour %1 à %2", GetGame().GetPlayerManager().GetPlayerName(playerId), position.ToString()), LogLevel.NORMAL);

		GetGame().GetCallqueue().CallLater(FinalizeSpawn, 500, false, playerId, previous, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Attend que le nouveau personnage existe, puis remplit son inventaire :
	//! inventaire sauvegardé (reconnexion) ou kit de base (spawn frais)
	protected void FinalizeSpawn(int playerId, IEntity previous, int attempt)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character || character == previous)
		{
			if (attempt < 20)
				GetGame().GetCallqueue().CallLater(FinalizeSpawn, 500, false, playerId, previous, attempt + 1);
			return;
		}

		// Le personnage est là : le menu Staff remet l'invincibilité si elle était active
		SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
		if (admin)
			admin.OnCharacterReady(playerId, character);
		SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
		if (charte)
			charte.OnCharacterReady(playerId);

		// Mort pendant une déconnexion : on le lui dit, une fois
		SRP_PlayerManagerComponent playersForDeath = SRP_PlayerManagerComponent.GetInstance();
		if (playersForDeath)
		{
			SRP_PlayerRecord deathRecord = playersForDeath.GetRecord(playerId);
			if (deathRecord && !deathRecord.m_sMortEnAbsence.IsEmpty())
			{
				GetGame().GetCallqueue().CallLater(SRP_Utils.NotifyPlayer, 4000, false, playerId, "Ton personnage est mort pendant ton absence (" + deathRecord.m_sMortEnAbsence + "). Tu repars de la base avec la dotation.");
				deathRecord.m_sMortEnAbsence = "";
				deathRecord.Save();
			}
		}

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		bool restore = false;
		m_mRestoreOnSpawn.Find(playerId, restore);
		m_mRestoreOnSpawn.Remove(playerId);

		// Exemption de facturation le temps de la distribution (le spawn est souvent près de l'arsenal)
		SRP_ArsenalEconomyComponent economy = SRP_ArsenalEconomyComponent.GetInstance();
		if (economy)
		{
			economy.SetChargesSuppressed(playerId, true);
			if (!restore)
				GetGame().GetCallqueue().CallLater(economy.SetChargesSuppressed, 3000, false, playerId, false);
		}

		// Reconnexion : l'inventaire sauvegardé est réappliqué emplacement par emplacement, une fois le
		// personnage prêt (sur serveur dédié, ses stockages ne le sont pas encore 500 ms après l'apparition)
		if (restore)
		{
			GetGame().GetCallqueue().CallLater(ApplyRestore, 1500, false, playerId, character, 0);
			return;
		}

		GiveBaseKit(playerId, character, inventory, playerName);
	}

	//------------------------------------------------------------------------------------------------
	//! Applique l'inventaire sauvegardé, puis vérifie une seconde plus tard que des objets sont bien là ;
	//! sinon réessaie (trois fois), et en dernier recours donne le kit de base
	protected void ApplyRestore(int playerId, IEntity character, int attempt)
	{
		if (!character || character.IsDeleted() || GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId) != character)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();
		SRP_PlayerRecord record;
		if (manager)
			record = manager.GetRecord(playerId);
		if (!inventory || !record || !record.m_sInventory.StartsWith("{"))
		{
			ReleaseCharges(playerId);
			if (inventory)
				GiveBaseKit(playerId, character, inventory, playerName);
			return;
		}

		bool applied = RestoreInventory(character, record.m_sInventory);
		if (!applied)
			Print(string.Format("[SRP] %1 : inventaire sauvegardé refusé (essai %2)", playerName, attempt + 1), LogLevel.WARNING);

		GetGame().GetCallqueue().CallLater(VerifyRestore, 1000, false, playerId, character, attempt);
	}

	//------------------------------------------------------------------------------------------------
	protected void VerifyRestore(int playerId, IEntity character, int attempt)
	{
		if (!character || character.IsDeleted() || GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId) != character)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		if (!inventory)
			return;

		array<IEntity> items = {};
		int count = inventory.GetItems(items, EStoragePurpose.PURPOSE_ANY);
		if (count > 0)
		{
			SRP_JournalComponent.Log("SPAWN", string.Format("%1 : inventaire restauré (%2 objet(s), essai %3)", playerName, count, attempt + 1));
			ReleaseCharges(playerId);
			return;
		}

		if (attempt < 2)
		{
			Print(string.Format("[SRP] %1 : inventaire vide après restauration, nouvel essai %2", playerName, attempt + 2), LogLevel.WARNING);
			GetGame().GetCallqueue().CallLater(ApplyRestore, 1500, false, playerId, character, attempt + 1);
			return;
		}

		SRP_JournalComponent.Log("SPAWN", playerName + " : inventaire sauvegardé impossible à restaurer après 3 essais, kit de base à la place");
		GiveBaseKit(playerId, character, inventory, playerName);
		ReleaseCharges(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de l'exemption de facturation d'arsenal, 3 s après le dernier objet rangé
	protected void ReleaseCharges(int playerId)
	{
		SRP_ArsenalEconomyComponent economy = SRP_ArsenalEconomyComponent.GetInstance();
		if (economy)
			GetGame().GetCallqueue().CallLater(economy.SetChargesSuppressed, 3000, false, playerId, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn frais (ou restauration impossible) : kit de base
	protected void GiveBaseKit(int playerId, IEntity character, InventoryStorageManagerComponent inventory, string playerName)
	{
		if (!m_aKitDeBase)
			return;

		// Ce que le prefab de personnage a déjà (tenue, casque...) n'est pas redonné
		array<IEntity> existing = {};
		inventory.GetItems(existing, EStoragePurpose.PURPOSE_ANY);
		array<string> alreadyThere = {};
		foreach (IEntity item : existing)
		{
			if (!item)
				continue;
			EntityPrefabData prefabData = item.GetPrefabData();
			if (prefabData)
				alreadyThere.Insert(prefabData.GetPrefabName());
		}

		int given = 0;
		int failed = 0;
		foreach (ResourceName prefab : m_aKitDeBase)
		{
			if (prefab.IsEmpty())
				continue;

			int index = alreadyThere.Find(prefab);
			if (index >= 0)
			{
				alreadyThere.Remove(index);	// un exemplaire déjà présent compte pour un
				continue;
			}

			if (inventory.TrySpawnPrefabToStorage(prefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, 1))
				given++;
			else
				failed++;
		}

		SRP_JournalComponent.Log("SPAWN", string.Format("%1 : kit de base, %2 objet(s) donné(s), %3 refusé(s)", playerName, given, failed));
		if (failed > 0)
			Print(string.Format("[SRP] %1 : %2 objet(s) n'ont pas pu être rangés (inventaire plein ou prefab invalide)", playerName, failed), LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Applique un inventaire sérialisé (SCR_PlayerArsenalLoadout) sur le personnage
	protected bool RestoreInventory(IEntity character, string json)
	{
		if (json.IsEmpty())
			return false;

		JsonLoadContext context = new JsonLoadContext();
		if (!context.LoadFromString(json))
			return false;

		return SCR_PlayerArsenalLoadout.ApplyLoadoutString(character, context);
	}

	//------------------------------------------------------------------------------------------------
	//! Position de spawn : dernière position sauvegardée, sinon l'entité nommée, sinon un
	//! SCR_SpawnPoint de la faction. Retourne false si rien n'est trouvé.
	//! Les marqueurs de spawn : le nom réglé, puis le même nom suivi de 1, 2, 3… (SRP_SpawnBase, SRP_SpawnBase1, …).
	//! On préfère un point sans joueur à moins de 3 m ; sinon n'importe lequel, au hasard.
	protected IEntity PickSpawnMarker()
	{
		array<IEntity> markers = {};
		IEntity main = GetGame().GetWorld().FindEntityByName(m_sSpawnEntityName);
		if (main)
			markers.Insert(main);
		for (int i = 1; i <= 20; i++)
		{
			IEntity extra = GetGame().GetWorld().FindEntityByName(m_sSpawnEntityName + i.ToString());
			if (extra)
				markers.Insert(extra);
		}
		if (markers.IsEmpty())
			return null;
		if (markers.Count() == 1)
			return markers[0];

		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		array<IEntity> free = {};
		foreach (IEntity marker : markers)
		{
			if (SRP_Utils.NearestPlayerDistance(marker.GetOrigin(), players) > 3)
				free.Insert(marker);
		}
		if (!free.IsEmpty())
			return free.GetRandomElement();
		return markers.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	protected bool GetSpawnPosition(int playerId, out vector position)
	{
		if (m_bUseSavedPosition)
		{
			SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();
			if (manager)
			{
				manager.EnsureRegistered(playerId);
				SRP_PlayerRecord record = manager.GetRecord(playerId);
				if (record && record.m_bHasPosition)
				{
					position = record.GetPosition();
					return true;
				}
			}
		}

		if (!m_sSpawnEntityName.IsEmpty())
		{
			IEntity marker = PickSpawnMarker();
			if (marker)
			{
				position = marker.GetOrigin();
				return true;
			}
		}

		SCR_SpawnPoint point = SCR_SpawnPoint.GetRandomSpawnPointForFaction(m_sFactionKey);
		if (point)
		{
			position = point.GetOrigin();
			return true;
		}

		return false;
	}
}
