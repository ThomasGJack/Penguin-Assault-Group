//------------------------------------------------------------------------------------------------
// SimpleRP — Éclairage de la base : le générateur
// Un générateur (prefab Prefabs/Props/Base/SRP_Generateur.et, posé au garage par Jack) alimente toutes
// les lampes à moins de m_fBaseRadius (300 m) du centre de la base (marqueur SRP_SpawnBase, à défaut le
// générateur lui-même). Coupé (action « Couper le générateur ») ou détruit (tir, explosion) : la base est
// dans le noir jusqu'à ce qu'un logisticien, un officier ou le Staff le répare (30 s). L'état (allumé,
// éteint, en panne) est sauvegardé dans $profile:SimpleRP/generateur.json et rétabli au démarrage.
//
// Ce que le générateur commande, à chaque changement d'état (serveur, puis diffusion à tous les clients) :
//  - les lampes interactives du jeu (SCR_BaseInteractiveLightComponent : projecteur GeneratorFloodlight_US_01,
//    nos SRP_Plafonnier / SRP_Neon) : ToggleLight, et leur interrupteur « Allumer » est refusé pendant une
//    coupure (classe moddée SCR_SwitchLightUserAction en bas de ce fichier) ;
//  - les lampes statiques (plafonniers LightCeiling_01_on, néons LightIndustrial_02_on_interior, lampe
//    chirurgicale…) : ce sont des LightEntity enfants, éteintes par LightEntity.SetEnabled ;
//  - les lampadaires et mâts à StreetLampComponent (LampStreet_E_01, LightTower_01, LampIndustrial_01) :
//    StreetLampComponent.SetBroken, ils reprennent leur allumage automatique à la nuit quand le courant revient.
//  Les lampes à pétrole (SCR_LampComponent : LanternMilitary_US_01, Lamp_Interactive) ne dépendent pas du
//  courant : elles restent le secours pendant une panne.
//
// Aussi ici : SpawnLantern / RemoveLantern (lanterne posée la nuit sur un point de livraison, un contrôle
// routier), et le mode nuit du Game Master (SCR_NightModeGameModeComponent, à poser sur le game mode).
// Carte #54 — 1.0.26
//------------------------------------------------------------------------------------------------

enum SRP_EGenerateurState
{
	ETEINT = 0,
	ALLUME = 1,
	EN_PANNE = 2
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Générateur SimpleRP : alimente les lampes de la base, état sauvegardé, lanternes de nuit")]
class SRP_GenerateurComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_GenerateurComponent : SCR_BaseGameModeComponent
{
	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur du centre de la base (à défaut : le générateur lui-même)", category: "SimpleRP")]
	protected string m_sBaseMarkerName;

	[Attribute("300", UIWidgets.EditBox, "Rayon de la base : les lampes à moins de … mètres du centre suivent le générateur", category: "SimpleRP")]
	protected float m_fBaseRadius;

	[Attribute("1", UIWidgets.CheckBox, "Générateur allumé au tout premier lancement (sans generateur.json)", category: "SimpleRP")]
	protected bool m_bFirstLaunchOn;

	[Attribute("300", UIWidgets.EditBox, "Les joueurs à moins de … mètres du générateur sont prévenus de chaque changement", category: "SimpleRP")]
	protected float m_fNotifyRadius;

	[Attribute("60", UIWidgets.EditBox, "Pendant une coupure, période de vérification des lampes, en secondes (une lampe apparue ou rallumée est réteinte)", category: "SimpleRP")]
	protected int m_iCheckSeconds;

	[Attribute("{50E6A3845C9F20FD}Prefabs/Props/Military/Camps/LanternMilitary_US_01.et", UIWidgets.ResourceNamePicker, "Lanterne posée la nuit (points de livraison, contrôle routier)", "et", category: "SimpleRP")]
	protected ResourceName m_sLanternPrefab;

	[Attribute("{6A5B0C0D0E0FE008}Prefabs/Props/Base/SRP_Generateur.et", UIWidgets.ResourceNamePicker, "Prefab du générateur : à la réparation, une épave est remplacée par un neuf au même endroit", "et", category: "SimpleRP")]
	protected ResourceName m_sGeneratorPrefab;

	static const string PATH = "$profile:SimpleRP/generateur.json";
	static const string DEFAULT_LANTERN = "{50E6A3845C9F20FD}Prefabs/Props/Military/Camps/LanternMilitary_US_01.et";

	protected static SRP_GenerateurComponent s_Instance;

	// Copie locale de l'état, tenue à jour sur chaque machine (serveur et clients) par RPC et à la connexion :
	// les libellés des actions et l'interrupteur des lampes la lisent sans passer par le serveur
	protected static int s_iLocalState = SRP_EGenerateurState.ALLUME;
	protected static vector s_vLocalCenter;
	protected static float s_fLocalRadius;
	protected static bool s_bLocalKnown;

	protected static ref array<IEntity> s_aQuery = {};
	protected static ref array<IEntity> s_aLanterns = {};

	// Serveur
	protected int m_iState = SRP_EGenerateurState.ALLUME;
	protected bool m_bStarted;
	protected bool m_bCenterKnown;
	protected vector m_vCenter;
	protected string m_sLastChange;

	//------------------------------------------------------------------------------------------------
	static SRP_GenerateurComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		if (!Replication.IsServer())
			return;
		SRP_Paths.EnsureDirectories();
		GetGame().GetCallqueue().CallLater(Boot, 2000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Boot);
			GetGame().GetCallqueue().Remove(Check);
			if (m_bStarted)
				Save();
		}
		GetGame().GetCallqueue().Remove(ApplyLocalFromStatic);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Démarrage (2 s après le game mode, comme le jour/nuit) : état sauvegardé, centre de la base, lampes
	protected void Boot()
	{
		Load();
		if (!FindCenter())
			Print("[SRP] Générateur : ni marqueur " + m_sBaseMarkerName + " ni générateur posé, l'éclairage attend (nouvel essai chaque minute)", LogLevel.WARNING);

		m_bStarted = true;
		int count = Apply();
		int period = Math.Max(10, m_iCheckSeconds) * 1000;
		GetGame().GetCallqueue().CallLater(Check, period, true);
		SRP_JournalComponent.Log("BASE", string.Format("Générateur : %1 au démarrage, %2 lampe(s) dans un rayon de %3 m", StateText(m_iState), count, Math.Round(m_fBaseRadius)));
	}

	//------------------------------------------------------------------------------------------------
	//! Le centre de la base : le marqueur, sinon le générateur posé
	protected bool FindCenter()
	{
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (marker)
		{
			m_vCenter = marker.GetOrigin();
			m_bCenterKnown = true;
			return true;
		}
		SRP_GenerateurEntityComponent generator = SRP_GenerateurEntityComponent.First();
		if (generator)
		{
			m_vCenter = generator.GetOwner().GetOrigin();
			m_bCenterKnown = true;
			return true;
		}
		return m_bCenterKnown;
	}

	//------------------------------------------------------------------------------------------------
	//! Entretien pendant une coupure : les lampes apparues ou rallumées entre-temps sont réteintes
	protected void Check()
	{
		if (!m_bCenterKnown)
		{
			if (!FindCenter())
				return;
			Apply();
			return;
		}
		if (m_iState != SRP_EGenerateurState.ALLUME)
			Apply();
	}

	//------------------------------------------------------------------------------------------------
	// Application de l'état aux lampes
	//------------------------------------------------------------------------------------------------
	//! Serveur : applique l'état ici puis chez tous les clients. Retourne le nombre de lampes touchées ici.
	protected int Apply()
	{
		if (!m_bCenterKnown)
			return 0;
		bool powered = m_iState == SRP_EGenerateurState.ALLUME;
		SetLocal(m_iState, m_vCenter, m_fBaseRadius);
		int count = ApplyLocal(powered, m_vCenter, m_fBaseRadius);
		Rpc(RpcDo_SRPApplyPower, m_iState, m_vCenter, m_fBaseRadius);
		return count;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPApplyPower(int state, vector center, float radius)
	{
		SetLocal(state, center, radius);
		ApplyLocal(state == SRP_EGenerateurState.ALLUME, center, radius);
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetLocal(int state, vector center, float radius)
	{
		s_iLocalState = state;
		s_vLocalCenter = center;
		s_fLocalRadius = radius;
		s_bLocalKnown = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur arrivé en cours de partie : l'état reçu à la connexion est appliqué une fois le monde en place
	protected void ApplyLocalFromStatic()
	{
		if (!s_bLocalKnown)
			return;
		ApplyLocal(s_iLocalState == SRP_EGenerateurState.ALLUME, s_vLocalCenter, s_fLocalRadius);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool QueryAdd(IEntity e)
	{
		s_aQuery.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Sur cette machine : allume ou éteint tout ce qui éclaire à moins de radius du centre.
	//! Deux passes : d'abord les lumières statiques (LightEntity, lampadaires), ensuite les lampes interactives,
	//! dont ToggleLight(false) supprime ses propres LightEntity — on ne touche plus à la liste après.
	static int ApplyLocal(bool powered, vector center, float radius)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		s_aQuery.Clear();
		world.QueryEntitiesBySphere(center, radius, QueryAdd, null, EQueryEntitiesFlags.ALL);

		int count = 0;
		foreach (IEntity e : s_aQuery)
		{
			if (!e || e.IsDeleted())
				continue;

			StreetLampComponent street = StreetLampComponent.Cast(e.FindComponent(StreetLampComponent));
			if (street)
			{
				// Lampadaire ou mât : « cassé » = éteint ; réparé = il suit à nouveau la nuit tout seul
				street.SetBroken(!powered);
				count++;
				continue;
			}

			LightEntity light = LightEntity.Cast(e);
			if (light)
			{
				if (light.GetParent() && light.GetParent().FindComponent(SCR_LampComponent))
					continue;	// flamme d'une lampe à pétrole
				light.SetEnabled(powered);
				count++;
			}
		}

		foreach (IEntity e : s_aQuery)
		{
			if (!e || e.IsDeleted() || LightEntity.Cast(e))
				continue;
			SCR_BaseInteractiveLightComponent lamp = SCR_BaseInteractiveLightComponent.Cast(e.FindComponent(SCR_BaseInteractiveLightComponent));
			if (!lamp)
				continue;
			if (SCR_LampComponent.Cast(lamp))
				continue;	// lanterne, lampe à pétrole : pas besoin de courant
			if (lamp.IsOn() != powered)
			{
				lamp.ToggleLight(powered, true, true);
				count++;
			}
		}
		s_aQuery.Clear();
		return count;
	}

	//------------------------------------------------------------------------------------------------
	// Connexion en cours de partie : le game mode transmet l'état
	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteInt(m_iState);
		writer.WriteBool(m_bCenterKnown);
		writer.WriteVector(m_vCenter);
		writer.WriteFloat(m_fBaseRadius);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int state;
		bool known;
		vector center;
		float radius;
		reader.ReadInt(state);
		reader.ReadBool(known);
		reader.ReadVector(center);
		reader.ReadFloat(radius);
		if (known)
		{
			SetLocal(state, center, radius);
			GetGame().GetCallqueue().CallLater(ApplyLocalFromStatic, 2000, false);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// État (serveur)
	//------------------------------------------------------------------------------------------------
	bool IsPowered()
	{
		return m_iState == SRP_EGenerateurState.ALLUME;
	}

	//------------------------------------------------------------------------------------------------
	int GetState()
	{
		return m_iState;
	}

	//------------------------------------------------------------------------------------------------
	//! L'état connu sur cette machine (actions, interrupteurs)
	static int GetLocalState()
	{
		return s_iLocalState;
	}

	//------------------------------------------------------------------------------------------------
	//! Vrai si, sur cette machine, la position est dans la base et que le courant est coupé
	static bool IsBlackoutAt(vector position)
	{
		if (!s_bLocalKnown || s_iLocalState == SRP_EGenerateurState.ALLUME)
			return false;
		return vector.Distance(position, s_vLocalCenter) <= s_fLocalRadius;
	}

	//------------------------------------------------------------------------------------------------
	static string StateText(int state)
	{
		if (state == SRP_EGenerateurState.ALLUME)
			return "allumé";
		if (state == SRP_EGenerateurState.EN_PANNE)
			return "en panne";
		return "éteint";
	}

	//------------------------------------------------------------------------------------------------
	//! Démarre le générateur. Retourne le message pour le joueur.
	string Start(string author = "")
	{
		if (m_iState == SRP_EGenerateurState.EN_PANNE)
			return "Le générateur est en panne : à réparer d'abord (logisticien, officier ou Staff)";
		if (m_iState == SRP_EGenerateurState.ALLUME)
			return "Le générateur tourne déjà";
		Change(SRP_EGenerateurState.ALLUME, author, "Le générateur démarre : la base est éclairée");
		return "Générateur démarré";
	}

	//------------------------------------------------------------------------------------------------
	//! Coupe le générateur
	string Stop(string author = "")
	{
		if (m_iState == SRP_EGenerateurState.EN_PANNE)
			return "Le générateur est en panne";
		if (m_iState == SRP_EGenerateurState.ETEINT)
			return "Le générateur est déjà coupé";
		Change(SRP_EGenerateurState.ETEINT, author, "Le générateur est coupé : la base est dans le noir");
		return "Générateur coupé";
	}

	//------------------------------------------------------------------------------------------------
	//! Panne (destruction, sabotage, Staff) : le noir jusqu'à réparation
	string Break(string cause = "")
	{
		if (m_iState == SRP_EGenerateurState.EN_PANNE)
			return "Le générateur est déjà en panne";
		string text = "Le générateur est en panne : la base est dans le noir jusqu'à sa réparation (logisticien, officier ou Staff)";
		Change(SRP_EGenerateurState.EN_PANNE, cause, text);
		return "Générateur en panne";
	}

	//------------------------------------------------------------------------------------------------
	//! Réparation : le générateur repart, une épave est remplacée par un neuf
	string Repair(string author = "")
	{
		if (m_iState != SRP_EGenerateurState.EN_PANNE)
			return "Le générateur n'est pas en panne";
		ReplaceWreck();
		Change(SRP_EGenerateurState.ALLUME, author, "Le générateur est réparé : la base est éclairée");
		return "Générateur réparé et démarré";
	}

	//------------------------------------------------------------------------------------------------
	//! Réparation par un joueur : réservée aux logisticiens (certification Logi), aux officiers et au Staff
	string RepairBy(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";
		bool allowed = record.m_bStaff || SRP_Grades.IsOfficier(record.m_iGrade) || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.LOGI);
		if (!allowed)
			return "Réparer le générateur est réservé aux logisticiens (certification Logi) et aux officiers";
		return Repair(record.m_sName);
	}

	//------------------------------------------------------------------------------------------------
	//! Action au contact : démarre ou coupe selon l'état
	string Toggle(int playerId)
	{
		string author = "";
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
		{
			SRP_PlayerRecord record = players.GetRecord(playerId);
			if (record)
				author = record.m_sName;
		}
		if (m_iState == SRP_EGenerateurState.ALLUME)
			return Stop(author);
		return Start(author);
	}

	//------------------------------------------------------------------------------------------------
	protected void Change(int state, string author, string playerText)
	{
		m_iState = state;
		if (author.IsEmpty())
			m_sLastChange = SRP_Time.Now();
		else
			m_sLastChange = SRP_Time.Now() + " (" + author + ")";
		int count = Apply();
		Save();

		string who = "";
		if (!author.IsEmpty())
			who = " — " + author;
		SRP_JournalComponent.Log("BASE", string.Format("Générateur %1%2 : %3 lampe(s)", StateText(state), who, count));
		NotifyNear(playerText);
	}

	//------------------------------------------------------------------------------------------------
	//! Message aux joueurs à moins de m_fNotifyRadius du générateur (à défaut, du centre de la base)
	protected void NotifyNear(string text)
	{
		vector origin = m_vCenter;
		SRP_GenerateurEntityComponent generator = SRP_GenerateurEntityComponent.First();
		if (generator)
			origin = generator.GetOwner().GetOrigin();
		else if (!m_bCenterKnown)
			return;

		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character && vector.Distance(character.GetOrigin(), origin) <= m_fNotifyRadius)
				SRP_Utils.NotifyPlayer(id, text);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un générateur détruit est une épave : à la réparation, on la remplace par un neuf au même endroit
	protected void ReplaceWreck()
	{
		SRP_GenerateurEntityComponent generator = SRP_GenerateurEntityComponent.First();
		if (!generator || !generator.IsWrecked())
			return;

		IEntity wreck = generator.GetOwner();
		Resource res = Resource.Load(m_sGeneratorPrefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Générateur : prefab de remplacement introuvable " + m_sGeneratorPrefab + ", l'épave reste", LogLevel.WARNING);
			generator.MarkRepaired();
			return;
		}

		vector mat[4];
		wreck.GetWorldTransform(mat);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		SCR_EntityHelper.DeleteEntityAndChildren(wreck);
		IEntity fresh = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!fresh)
			Print("[SRP] Générateur : échec du remplacement de l'épave", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Texte d'état (menu Staff, journal)
	string GetStatusText()
	{
		string text = "Générateur : " + StateText(m_iState);
		if (!m_sLastChange.IsEmpty())
			text += " — dernier changement " + m_sLastChange;
		SRP_GenerateurEntityComponent generator = SRP_GenerateurEntityComponent.First();
		if (!generator)
			text += "\nAucun générateur posé dans le monde (prefab SRP_Generateur.et)";
		else if (generator.IsWrecked())
			text += "\nLe générateur est détruit (épave)";
		if (m_bCenterKnown)
			text += string.Format("\nRayon de la base : %1 m", Math.Round(m_fBaseRadius));
		else
			text += "\nCentre de la base inconnu : ni marqueur " + m_sBaseMarkerName + " ni générateur";
		text += string.Format("\nLanternes de nuit posées : %1", s_aLanterns.Count());
		text += "\nMode nuit du Game Master : " + SRP_Utils.OnOff(IsGmNightMode());
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Mode nuit du Game Master (SCR_NightModeGameModeComponent sur le game mode)
	//------------------------------------------------------------------------------------------------
	protected static SCR_NightModeGameModeComponent NightMode()
	{
		IEntity gameMode;
		if (s_Instance)
			gameMode = s_Instance.GetOwner();
		if (!gameMode)
			gameMode = GetGame().GetGameMode();
		if (!gameMode)
			return null;
		return SCR_NightModeGameModeComponent.Cast(gameMode.FindComponent(SCR_NightModeGameModeComponent));
	}

	//------------------------------------------------------------------------------------------------
	static bool IsGmNightMode()
	{
		SCR_NightModeGameModeComponent nightMode = NightMode();
		return nightMode && nightMode.IsGlobalNightModeEnabled();
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : mode nuit global (image éclaircie la nuit pour tous). Retourne le message.
	static string SetGmNightMode(bool enable, int playerId, string author)
	{
		SCR_NightModeGameModeComponent nightMode = NightMode();
		if (!nightMode)
			return "SCR_NightModeGameModeComponent absent du game mode";
		if (!nightMode.IsGlobalNightModeAllowed())
			return "Mode nuit global interdit : cocher m_bAllowGlobalNightMode sur SCR_NightModeGameModeComponent";
		nightMode.EnableGlobalNightMode(enable, playerId);
		SRP_JournalComponent.Log("STAFF", author + " : mode nuit du Game Master " + SRP_Utils.OnOff(enable));
		return "Mode nuit du Game Master : " + SRP_Utils.OnOff(nightMode.IsGlobalNightModeEnabled());
	}

	//------------------------------------------------------------------------------------------------
	// Lanternes de nuit (points de livraison, contrôle routier)
	//------------------------------------------------------------------------------------------------
	//! Pose une lanterne allumée au sol à cette position (la hauteur est prise sur le terrain). Serveur. Retourne l'entité ou null.
	static IEntity SpawnLantern(vector position)
	{
		if (!Replication.IsServer())
			return null;

		ResourceName prefab = DEFAULT_LANTERN;
		if (s_Instance && !s_Instance.m_sLanternPrefab.IsEmpty())
			prefab = s_Instance.m_sLanternPrefab;

		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Lanterne : prefab introuvable " + prefab, LogLevel.WARNING);
			return null;
		}

		vector pos = position;
		pos[1] = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]) + 0.02;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		IEntity lantern = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!lantern)
			return null;

		// LIT_ON_SPAWN du prefab : un objet créé en jeu (non chargé avec le monde) apparaît allumé, ici et chez les clients
		s_aLanterns.Insert(lantern);
		return lantern;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire une lanterne posée par SpawnLantern (null accepté)
	static void RemoveLantern(IEntity lantern)
	{
		if (!lantern)
			return;
		s_aLanterns.RemoveItem(lantern);
		if (!lantern.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(lantern);
	}

	//------------------------------------------------------------------------------------------------
	static int LanternCount()
	{
		return s_aLanterns.Count();
	}

	//------------------------------------------------------------------------------------------------
	// Sauvegarde
	//------------------------------------------------------------------------------------------------
	void Save()
	{
		if (!Replication.IsServer() || !m_bStarted)
			return;
		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("state", m_iState);
		ctx.WriteValue("stateText", StateText(m_iState));
		ctx.WriteValue("changed", m_sLastChange);
		ctx.WriteValue("savedAt", SRP_Time.Now());
		if (!ctx.SaveToFile(PATH))
			Print("[SRP] Échec de sauvegarde du générateur : " + PATH, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		if (!FileIO.FileExists(PATH))
		{
			if (m_bFirstLaunchOn)
				m_iState = SRP_EGenerateurState.ALLUME;
			else
				m_iState = SRP_EGenerateurState.ETEINT;
			return;
		}
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(PATH))
		{
			Print("[SRP] Générateur : generateur.json illisible, générateur allumé", LogLevel.WARNING);
			return;
		}
		int state = SRP_EGenerateurState.ALLUME;
		ctx.ReadValue("state", state);
		ctx.ReadValue("changed", m_sLastChange);
		if (state == SRP_EGenerateurState.ETEINT || state == SRP_EGenerateurState.EN_PANNE)
			m_iState = state;
		else
			m_iState = SRP_EGenerateurState.ALLUME;
	}
}

//------------------------------------------------------------------------------------------------
//! Composant du prefab SRP_Generateur.et : s'enregistre, surveille sa destruction (sondage toutes les 5 s,
//! comme le parc automobile) et met le générateur en panne quand il est détruit.
[ComponentEditorProps(category: "SimpleRP", description: "Générateur de la base SimpleRP : détruit = panne")]
class SRP_GenerateurEntityComponentClass : ScriptComponentClass
{
}

class SRP_GenerateurEntityComponent : ScriptComponent
{
	protected static ref array<SRP_GenerateurEntityComponent> s_aGenerators = {};

	protected bool m_bWrecked;

	//------------------------------------------------------------------------------------------------
	//! Le premier générateur posé encore présent, ou null
	static SRP_GenerateurEntityComponent First()
	{
		foreach (SRP_GenerateurEntityComponent generator : s_aGenerators)
		{
			if (generator && generator.GetOwner() && !generator.GetOwner().IsDeleted())
				return generator;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static SRP_GenerateurEntityComponent Of(IEntity entity)
	{
		if (!entity)
			return null;
		return SRP_GenerateurEntityComponent.Cast(entity.FindComponent(SRP_GenerateurEntityComponent));
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		if (!s_aGenerators.Contains(this))
			s_aGenerators.Insert(this);
		GetGame().GetCallqueue().CallLater(Watch, 5000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aGenerators.RemoveItem(this);
		if (Replication.IsServer())
			GetGame().GetCallqueue().Remove(Watch);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected void Watch()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted() || m_bWrecked)
			return;
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(owner);
		if (!damage || damage.GetState() != EDamageState.DESTROYED)
			return;

		m_bWrecked = true;
		SRP_GenerateurComponent generator = SRP_GenerateurComponent.GetInstance();
		if (generator)
			generator.Break("détruit");
		else
			Print("[SRP] Générateur détruit mais SRP_GenerateurComponent absent du game mode", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	bool IsWrecked()
	{
		return m_bWrecked;
	}

	//------------------------------------------------------------------------------------------------
	void MarkRepaired()
	{
		m_bWrecked = false;
	}
}

//------------------------------------------------------------------------------------------------
//! « Démarrer le générateur » / « Couper le générateur » : sur le contexte « powerswitch » du prefab. Tout le monde ;
//! refusée en panne.
class SRP_GenerateurStartAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		SRP_GenerateurComponent generator = SRP_GenerateurComponent.GetInstance();
		if (!generator)
			return;
		string reply = generator.Toggle(playerId);
		if (playerId > 0 && !reply.IsEmpty())
			SRP_Utils.NotifyPlayer(playerId, reply);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		if (SRP_GenerateurComponent.GetLocalState() == SRP_EGenerateurState.ALLUME)
			outName = "Couper le générateur";
		else
			outName = "Démarrer le générateur";
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
		if (SRP_GenerateurComponent.GetLocalState() == SRP_EGenerateurState.EN_PANNE)
		{
			SetCannotPerformReason("En panne : à réparer (logisticien, officier ou Staff)");
			return false;
		}
		return true;
	}

	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! « Réparer le générateur » (30 s, Duration du prefab) : visible seulement en panne ; logisticiens, officiers, Staff
class SRP_GenerateurRepairAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		SRP_GenerateurComponent generator = SRP_GenerateurComponent.GetInstance();
		if (playerId <= 0 || !generator)
			return;
		string reply = generator.RepairBy(playerId);
		if (!reply.IsEmpty())
			SRP_Utils.NotifyPlayer(playerId, reply);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Réparer le générateur";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return SRP_GenerateurComponent.GetLocalState() == SRP_EGenerateurState.EN_PANNE;
	}

	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! Interrupteur des lampes du jeu : pendant une coupure, une lampe électrique de la base ne s'allume pas
//! (les lampes à pétrole, SCR_LampComponent, restent utilisables). Éteindre reste toujours possible.
modded class SCR_SwitchLightUserAction
{
	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!m_LightComp || m_LightComp.IsOn() || SCR_LampComponent.Cast(m_LightComp))
			return true;
		IEntity owner = GetOwner();
		if (owner && SRP_GenerateurComponent.IsBlackoutAt(owner.GetOrigin()))
		{
			SetCannotPerformReason("Pas de courant : générateur coupé ou en panne");
			return false;
		}
		return true;
	}
}
