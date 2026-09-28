//------------------------------------------------------------------------------------------------
// SimpleRP — Pont client / serveur pour les commandes
// Ajoute au contrôleur joueur vanilla deux RPC :
//   - le client envoie une commande tapée dans le chat (RpcAsk_SRPCommand, exécuté sur le serveur)
//   - le serveur renvoie la réponse au joueur concerné (RpcDo_SRPFeedback, exécuté chez lui)
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

modded class SCR_PlayerController
{
	protected SRP_DeathCamera m_SRPDeathCamera;
	protected CameraBase m_SRPPreviousCamera;		// caméra active avant la mort (celle du joueur)

	// Surveillance de l'arme en main (carte #57), chez le joueur local seulement
	protected bool m_bSRPWeaponWatch;				// la vérification périodique est lancée
	protected IEntity m_SRPWeaponWarned;			// dernière arme signalée hors service (une seule alerte par arme)
	protected IEntity m_SRPWeaponSuspect;			// arme trouvée sans mode de tir au dernier passage
	protected int m_iSRPWeaponSuspectTicks;			// passages consécutifs où elle l'était

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : lance la caméra de mort chez le joueur (position du corps, durée en secondes)
	void SRP_StartDeathCamera(vector position, int seconds)
	{
		Rpc(RpcDo_SRPDeathCamera, position, seconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client : crée la caméra orbitale et l'active
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPDeathCamera(vector position, int seconds)
	{
		SRP_StopDeathCamera();

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
			return;

		m_SRPPreviousCamera = cameraManager.CurrentCamera();

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;

		m_SRPDeathCamera = SRP_DeathCamera.Cast(GetGame().SpawnEntity(SRP_DeathCamera, GetGame().GetWorld(), params));
		if (!m_SRPDeathCamera)
		{
			Print("[SRP] Impossible de créer la caméra de mort", LogLevel.WARNING);
			return;
		}

		m_SRPDeathCamera.Setup(position);
		cameraManager.SetCamera(m_SRPDeathCamera);

		// Sécurité : si le respawn n'arrive jamais, on rend la main quand même
		GetGame().GetCallqueue().CallLater(SRP_StopDeathCamera, (seconds + 15) * 1000, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client : rend la main à la caméra du personnage, puis supprime la caméra de mort
	void SRP_StopDeathCamera()
	{
		if (!m_SRPDeathCamera)
			return;

		SRP_RestoreCamera(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Cherche la caméra du joueur (l'ancienne si elle existe encore, sinon n'importe quelle
	//! caméra enregistrée qui n'est pas la nôtre). La caméra du nouveau personnage peut mettre
	//! quelques dixièmes de seconde à s'enregistrer : on réessaie jusqu'à 10 fois.
	protected void SRP_RestoreCamera(int attempt)
	{
		if (!m_SRPDeathCamera)
			return;

		CameraManager cameraManager = GetGame().GetCameraManager();
		if (!cameraManager)
		{
			SRP_DeleteDeathCamera();
			return;
		}

		if (cameraManager.CurrentCamera() != m_SRPDeathCamera)
		{
			// Quelqu'un d'autre a déjà repris la main : il ne reste qu'à nettoyer
			SRP_DeleteDeathCamera();
			return;
		}

		CameraBase target = m_SRPPreviousCamera;
		if (!target || target == m_SRPDeathCamera)
		{
			array<CameraBase> cameras = {};
			cameraManager.GetCamerasList(cameras);
			foreach (CameraBase cam : cameras)
			{
				if (cam && cam != m_SRPDeathCamera)
				{
					target = cam;
					break;
				}
			}
		}

		if (target)
		{
			cameraManager.SetCamera(target);
			Print("[SRP] Caméra rendue au personnage", LogLevel.NORMAL);
			SRP_DeleteDeathCamera();
			return;
		}

		if (attempt < 10)
		{
			GetGame().GetCallqueue().CallLater(SRP_RestoreCamera, 200, false, attempt + 1);
			return;
		}

		Print("[SRP] Aucune caméra joueur trouvée pour remplacer la caméra de mort", LogLevel.WARNING);
		SRP_DeleteDeathCamera();
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_DeleteDeathCamera()
	{
		if (!m_SRPDeathCamera)
			return;

		SCR_EntityHelper.DeleteEntityAndChildren(m_SRPDeathCamera);
		m_SRPDeathCamera = null;
		m_SRPPreviousCamera = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Nouveau personnage contrôlé : fin de la caméra de mort (petit délai, le temps que la
	//! caméra du personnage soit enregistrée)
	override void OnControlledEntityChanged(IEntity from, IEntity to)
	{
		super.OnControlledEntityChanged(from, to);

		if (to && m_SRPDeathCamera)
			GetGame().GetCallqueue().CallLater(SRP_StopDeathCamera, 200, false);

		// Surveillance de l'arme : seulement pour le contrôleur du joueur local (jamais sur un serveur dédié)
		if (to && !m_bSRPWeaponWatch && GetGame().GetPlayerController() == this)
		{
			m_bSRPWeaponWatch = true;
			GetGame().GetCallqueue().CallLater(SRP_WeaponWatch, 2000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client, toutes les 2 s : l'arme en main a-t-elle perdu sa bouche ou son mode de tir ? Dans ce cas
	//! (deux passages de suite, une seule fois par arme) : encart, ligne de console et rapport au serveur.
	protected void SRP_WeaponWatch()
	{
		IEntity character = GetControlledEntity();
		if (!character || SRP_Utils.IsDead(character))
			return;

		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return;
		BaseWeaponManagerComponent weaponManager = chimera.GetWeaponManager();
		if (!weaponManager)
			return;
		BaseWeaponComponent weapon = weaponManager.GetCurrentWeapon();
		if (!weapon)
			return;

		IEntity weaponEntity = SRP_WeaponEntityOf(weapon, weaponManager);
		if (!weaponEntity || weaponEntity == character)
			return;

		// Une arme sans aucune bouche (jumelles, objet tenu) n'est pas concernée
		array<BaseMuzzleComponent> muzzles = {};
		weapon.GetMuzzlesList(muzzles);
		if (muzzles.IsEmpty())
			return;

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		bool broken = !muzzle;
		if (muzzle && muzzle.GetFireModesCount() > 0 && !muzzle.GetCurrentFireMode())
			broken = true;

		if (!broken)
		{
			m_SRPWeaponSuspect = null;
			m_iSRPWeaponSuspectTicks = 0;
			return;
		}

		if (weaponEntity == m_SRPWeaponWarned)
			return;

		// Deux passages de suite : on évite l'alerte pendant qu'une arme se charge tout juste
		if (m_SRPWeaponSuspect != weaponEntity)
		{
			m_SRPWeaponSuspect = weaponEntity;
			m_iSRPWeaponSuspectTicks = 1;
			return;
		}
		m_iSRPWeaponSuspectTicks++;
		if (m_iSRPWeaponSuspectTicks < 2)
			return;

		m_SRPWeaponWarned = weaponEntity;
		m_SRPWeaponSuspect = null;
		m_iSRPWeaponSuspectTicks = 0;

		string report = SRP_WeaponReportOf(weaponEntity, weapon, muzzle);
		Print("[SRP] Arme hors service chez le joueur local : " + report, LogLevel.WARNING);

		string text = "Arme hors service. Ne posez pas le chargeur au sol : mettez-le dans le sac. Le Staff peut recréer l'arme.";
		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if (hints)
			hints.ShowCustomHint(text, "Arme hors service", 10.0);
		SRP_ChatCommands.ShowLocal(text);

		Rpc(RpcAsk_SRPWeaponReport, report);
	}

	//------------------------------------------------------------------------------------------------
	//! L'entité de l'arme derrière un BaseWeaponComponent (composant de l'arme, ou emplacement du personnage)
	protected static IEntity SRP_WeaponEntityOf(BaseWeaponComponent weapon, BaseWeaponManagerComponent weaponManager)
	{
		WeaponSlotComponent slot = WeaponSlotComponent.Cast(weapon);
		if (slot)
			return slot.GetWeaponEntity();
		WeaponSlotComponent currentSlot = weaponManager.GetCurrentSlot();
		if (currentSlot && currentSlot.GetWeaponEntity())
			return currentSlot.GetWeaponEntity();
		return weapon.GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	//! Description d'une arme pour le journal : prefab, bouche, mode de tir, chargeur, accessoires
	protected static string SRP_WeaponReportOf(IEntity weaponEntity, BaseWeaponComponent weapon, BaseMuzzleComponent muzzle)
	{
		string prefab = "?";
		EntityPrefabData prefabData = weaponEntity.GetPrefabData();
		if (prefabData)
			prefab = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());

		string fireMode = "non";
		string magazine = "non";
		if (muzzle)
		{
			if (muzzle.GetCurrentFireMode())
				fireMode = "oui";
			if (muzzle.GetMagazine())
				magazine = "oui";
		}

		string attachments = "";
		WeaponAttachmentsStorageComponent storage = WeaponAttachmentsStorageComponent.Cast(weaponEntity.FindComponent(WeaponAttachmentsStorageComponent));
		if (storage)
		{
			array<IEntity> items = {};
			storage.GetAll(items, false);
			foreach (IEntity item : items)
			{
				if (!item)
					continue;
				EntityPrefabData itemData = item.GetPrefabData();
				if (!itemData)
					continue;
				if (!attachments.IsEmpty())
					attachments += ", ";
				attachments += SRP_Utils.PrefabShortName(itemData.GetPrefabName());
			}
		}
		if (attachments.IsEmpty())
			attachments = "aucun";

		return string.Format("arme %1, bouche %2, mode de tir %3, chargeur %4, accessoires : %5", prefab, SRP_Utils.OuiNon(muzzle != null), fireMode, magazine, attachments);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : rapport d'arme hors service envoyé par le client, pour le journal
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPWeaponReport(string report)
	{
		string name = GetGame().GetPlayerManager().GetPlayerName(GetPlayerId());
		SRP_JournalComponent.Log("ARME", string.Format("%1 : arme hors service — %2", name, report));
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client : envoie une commande au serveur
	void SRP_SendCommand(string command, string args)
	{
		Rpc(RpcAsk_SRPCommand, command, args);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : exécute la commande et renvoie la réponse
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPCommand(string command, string args)
	{
		string reply = SRP_CommandProcessor.ExecuteFromChat(GetPlayerId(), command, args);
		if (!reply.IsEmpty())
			Rpc(RpcDo_SRPFeedback, reply);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client : une INTERFACE (terminal logistique, gestion du soldat, charte, menu Staff) envoie son action.
	//! Chemin distinct du chat : la purge des commandes du chat ne doit pas bloquer les menus.
	void SRP_SendMenuCommand(string command, string args)
	{
		Rpc(RpcAsk_SRPMenuCommand, command, args);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPMenuCommand(string command, string args)
	{
		string reply = SRP_CommandProcessor.ExecuteFromMenu(GetPlayerId(), command, args);
		if (!reply.IsEmpty())
			Rpc(RpcDo_SRPFeedback, reply);
	}

	//------------------------------------------------------------------------------------------------
	//! Client : demande le catalogue du terminal logistique au serveur
	void SRP_RequestTerminal()
	{
		Rpc(RpcAsk_SRPTerminal);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPTerminal()
	{
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
		{
			Rpc(RpcDo_SRPFeedback, "Trésorerie absente du game mode");
			return;
		}
		Rpc(RpcDo_SRPOpenTerminal, treasury.BuildCatalogue(GetPlayerId()));
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : (r)ouvre le terminal chez le joueur avec ce contenu (catalogue ou page de quantité)
	void SRP_OpenTerminal(string content)
	{
		Rpc(RpcDo_SRPOpenTerminal, content);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenTerminal(string catalogue)
	{
		SRP_TerminalMenu.Open(catalogue);
	}

	//------------------------------------------------------------------------------------------------
	//! Client : demande le menu Staff au serveur (page vide = accueil)
	void SRP_RequestAdmin(string page)
	{
		Rpc(RpcAsk_SRPAdmin, page);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPAdmin(string page)
	{
		SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
		if (!admin)
		{
			Rpc(RpcDo_SRPFeedback, "Menu Staff absent du game mode (SRP_AdminComponent)");
			return;
		}

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players || !players.IsStaff(GetPlayerId()))
		{
			Rpc(RpcDo_SRPFeedback, "Réservé au Staff");
			return;
		}

		Rpc(RpcDo_SRPOpenAdmin, admin.BuildMenu(GetPlayerId(), page));
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : (r)ouvre le menu Staff chez le joueur avec ce contenu
	void SRP_OpenAdmin(string content)
	{
		Rpc(RpcDo_SRPOpenAdmin, content);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenAdmin(string content)
	{
		SRP_AdminMenu.Open(content);
	}

	//------------------------------------------------------------------------------------------------
	//! Client : demande un onglet (ou une fiche) du terminal du PC
	void SRP_RequestPC(string tab)
	{
		Rpc(RpcAsk_SRPPC, tab);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPPC(string tab)
	{
		Rpc(RpcDo_SRPOpenPC, SRP_PCScreens.Build(GetPlayerId(), tab));
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : ouvre (ou rafraîchit) le PC chez ce joueur sur cet onglet
	void SRP_OpenPCTab(int playerId, string tab)
	{
		Rpc(RpcDo_SRPOpenPC, SRP_PCScreens.Build(playerId, tab));
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenPC(string content)
	{
		SRP_PCMenu.Show(content);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : ouvre la charte chez ce joueur
	void SRP_OpenCharte(string content)
	{
		Rpc(RpcDo_SRPOpenCharte, content);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenCharte(string content)
	{
		SRP_CharteMenu.Open(content);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : ouvre le menu « Gestion du soldat » chez ce joueur
	void SRP_OpenSoldier(string content)
	{
		Rpc(RpcDo_SRPOpenSoldier, content);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenSoldier(string content)
	{
		SRP_SoldierMenu.Open(content);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : ouvre le menu « Contrôle du véhicule » (point de contrôle routier) chez ce joueur
	void SRP_OpenControl(string content)
	{
		Rpc(RpcDo_SRPOpenControl, content);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenControl(string content)
	{
		SRP_ControlMenu.Open(content);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : ordre de projecteur de l'hélicoptère de recherche, deux fois par seconde (appareil, point visé au
	//! sol, vitesse de ce point en m/s pour l'extrapoler, allumé ou non, soldat suivi ou RplId invalide, appoint au sol)
	void SRP_HeliLight(RplId heliId, vector aim, vector aimVelocity, bool on, RplId targetId, bool fill)
	{
		Rpc(RpcDo_SRPHeliLight, heliId, aim, aimVelocity, on, targetId, fill);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Unreliable, RplRcver.Owner)]
	protected void RpcDo_SRPHeliLight(RplId heliId, vector aim, vector aimVelocity, bool on, RplId targetId, bool fill)
	{
		SRP_HeliLightClient.Order(heliId, aim, aimVelocity, on, targetId, fill);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : (r)ouvre l'assistant de demande de mission chez le joueur
	void SRP_OpenRequest(string content)
	{
		Rpc(RpcDo_SRPOpenRequest, content);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPOpenRequest(string content)
	{
		SRP_RequestMenu.Open(content);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : téléporte le joueur à pied (exécuté chez lui, comme le fait l'éditeur vanilla)
	void SRP_TeleportOwner(vector position)
	{
		Rpc(RpcDo_SRPTeleportOwner, position);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPTeleportOwner(vector position)
	{
		SCR_Global.TeleportPlayer(GetPlayerId(), position);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : diffuse une téléportation faite sur le serveur (joueur dans un véhicule)
	void SRP_TeleportBroadcast(int playerId, vector position)
	{
		Rpc(RpcDo_SRPTeleportBroadcast, playerId, position);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPTeleportBroadcast(int playerId, vector position)
	{
		SCR_Global.TeleportPlayer(playerId, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : envoie un message au joueur propriétaire de ce contrôleur
	void SRP_Notify(string text)
	{
		Rpc(RpcDo_SRPFeedback, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Droits d'interaction (SRP_Rights) : gardés ici côté serveur, envoyés à l'écran du joueur
	protected int m_iSRPRights;

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : nouveaux droits de ce joueur (SRP_Rights.Tick, seulement quand ils changent)
	void SRP_SetRights(int rights)
	{
		m_iSRPRights = rights;
		Rpc(RpcDo_SRPRights, rights);
	}

	//------------------------------------------------------------------------------------------------
	int SRP_GetRights()
	{
		return m_iSRPRights;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPRights(int rights)
	{
		m_iSRPRights = rights;
	}

	//------------------------------------------------------------------------------------------------
	//! Côté serveur : affiche un encart (titre + texte) chez le joueur propriétaire
	void SRP_Hint(string title, string text)
	{
		Rpc(RpcDo_SRPHint, title, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client (propriétaire) : affiche la réponse dans le chat local
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPFeedback(string text)
	{
		SRP_ChatCommands.ShowLocal(text);
		// Consoles : le chat est discret et masqué pendant les menus ; la réponse s'affiche aussi en encart
		if (SRP_ListMenu.SRP_IsGamepad())
		{
			// Ne jamais écraser un autre encart (ordre de mission, contrôle, panneau) : seulement un encart « PAG » ou rien
			SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
			SCR_HintUIInfo current;
			if (hintManager)
				current = hintManager.GetCurrentHint();
			if (!current || current.GetName() == "PAG")
				SCR_HintManagerComponent.ShowCustomHint(text, "PAG", 6.0, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Côté client (propriétaire) : encart à l'écran pendant quelques secondes
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPHint(string title, string text)
	{
		// Encart refusé (astuces coupées dans les réglages, autre encart prioritaire) : le texte passe par le chat
		if (!SCR_HintManagerComponent.ShowCustomHint(text, title, 8.0))
			SRP_ChatCommands.ShowLocal(title + " — " + text);
	}
}
