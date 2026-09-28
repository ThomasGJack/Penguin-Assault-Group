//------------------------------------------------------------------------------------------------
// SimpleRP — Déconnexion
// Le jeu efface le personnage d'un joueur qui part. Ici, une COPIE de son corps (même prefab, même
// équipement) reste allongée sur place pendant N minutes (15). Sort du joueur :
// - il revient avant la fin : la copie disparaît, il reprend là où il était, avec son équipement ;
// - la copie est tuée pendant l'absence : MORT ;
// - N minutes sans retour : MORT, sauf si le corps est à moins de R mètres (2 000) de la base ;
// - parti inconscient : traité comme une déconnexion normale, le corps reste (réglable : mort immédiate).
// Mort = la fiche perd sa position et son inventaire : au retour, spawn à la base avec la dotation, prévenu.
// Le Staff n'est pas concerné. /deco (officiers) liste les corps en attente.
// Phase 5 — étape G
//------------------------------------------------------------------------------------------------

class SRP_LeftBody
{
	string m_sIdentity;
	string m_sName;
	IEntity m_Body;
	int m_iLeftTick;
	vector m_vPosition;
}

[ComponentEditorProps(category: "SimpleRP", description: "Déconnexion SimpleRP : le corps reste, mort en absence")]
class SRP_DisconnectComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_DisconnectComponent : SCR_BaseGameModeComponent
{
	[Attribute("15", UIWidgets.EditBox, "Minutes pendant lesquelles le corps reste après une déconnexion", category: "SimpleRP")]
	protected int m_iMinutes;

	[Attribute("2000", UIWidgets.EditBox, "Rayon de sécurité autour de la base, en mètres : passé le délai, un corps à l'intérieur n'est pas déclaré mort", category: "SimpleRP")]
	protected float m_fSafeRadius;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur de la base", category: "SimpleRP")]
	protected string m_sBaseMarkerName;

	[Attribute("0", UIWidgets.CheckBox, "Se déconnecter inconscient vaut mort immédiate", category: "SimpleRP")]
	protected bool m_bUnconsciousIsDeath;

	[Attribute("1", UIWidgets.CheckBox, "Le Staff n'est pas concerné (pas de corps, pas de mort en absence)", category: "SimpleRP")]
	protected bool m_bStaffExempt;

	protected static SRP_DisconnectComponent s_Instance;
	protected ref array<ref SRP_LeftBody> m_aBodies = {};

	static SRP_DisconnectComponent GetInstance()
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
		GetGame().GetCallqueue().CallLater(Tick, 5000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Tick);
			// Arrêt du serveur : les corps en attente sont jugés tout de suite selon la règle de distance
			foreach (SRP_LeftBody left : m_aBodies)
				Judge(left, true);
			m_aBodies.Clear();
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Appelé par le gestionnaire de joueurs au départ d'un joueur, avant que le jeu efface son personnage
	void OnPlayerLeaving(int playerId, SRP_PlayerRecord record, IEntity character)
	{
		if (!record || !character || character.IsDeleted())
			return;
		if (m_bStaffExempt && record.m_bStaff)
			return;
		if (SRP_Utils.IsDead(character))
			return;	// déjà mort, la logique de spawn s'en charge

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (m_bUnconsciousIsDeath && controller && controller.IsUnconscious())
		{
			MarkDead(record.m_sIdentity, record.m_sName, "déconnecté inconscient");
			return;
		}

		// La copie du corps : même prefab, même position, même équipement, allongée, sans IA
		EntityPrefabData prefabData = character.GetPrefabData();
		if (!prefabData)
			return;
		ResourceName prefab = prefabData.GetPrefabName();
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		character.GetWorldTransform(params.Transform);
		IEntity body = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!body)
			return;

		if (record.m_sInventory.StartsWith("{"))
		{
			JsonLoadContext context = new JsonLoadContext();
			if (context.LoadFromString(record.m_sInventory))
				SCR_PlayerArsenalLoadout.ApplyLoadoutString(body, context);
		}

		CharacterControllerComponent bodyController = CharacterControllerComponent.Cast(body.FindComponent(CharacterControllerComponent));
		if (bodyController)
			bodyController.SetStanceChange(ECharacterStanceChange.STANCECHANGE_TOPRONE);

		SRP_LeftBody left = new SRP_LeftBody();
		left.m_sIdentity = record.m_sIdentity;
		left.m_sName = record.m_sName;
		left.m_Body = body;
		left.m_iLeftTick = System.GetTickCount();
		left.m_vPosition = character.GetOrigin();
		m_aBodies.Insert(left);

		SRP_JournalComponent.Log("DECONNEXION", string.Format("%1 : corps laissé sur place %2 min (grille %3 / %4)", record.m_sName, m_iMinutes, Math.Round(left.m_vPosition[0]), Math.Round(left.m_vPosition[2])));
	}

	//------------------------------------------------------------------------------------------------
	//! Appelé par le gestionnaire de joueurs quand un joueur (re)vient : son corps disparaît, il reprend
	void OnPlayerBack(string identity)
	{
		for (int i = m_aBodies.Count() - 1; i >= 0; i--)
		{
			SRP_LeftBody left = m_aBodies[i];
			if (left.m_sIdentity != identity)
				continue;
			RemoveBody(left);
			m_aBodies.Remove(i);
			SRP_JournalComponent.Log("DECONNEXION", left.m_sName + " est revenu à temps : corps retiré");
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		int now = System.GetTickCount();
		for (int i = m_aBodies.Count() - 1; i >= 0; i--)
		{
			SRP_LeftBody left = m_aBodies[i];
			bool bodyDead = !left.m_Body || left.m_Body.IsDeleted() || SRP_Utils.IsDead(left.m_Body);
			if (bodyDead)
			{
				MarkDead(left.m_sIdentity, left.m_sName, "corps tué pendant l'absence");
				RemoveBody(left);
				m_aBodies.Remove(i);
				continue;
			}
			if (now - left.m_iLeftTick >= m_iMinutes * 60 * 1000)
			{
				left.m_vPosition = left.m_Body.GetOrigin();
				Judge(left, false);
				RemoveBody(left);
				m_aBodies.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Passé le délai : mort, sauf près de la base
	protected void Judge(SRP_LeftBody left, bool shutdown)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		float distance = -1;
		if (marker)
			distance = vector.Distance(left.m_vPosition, marker.GetOrigin());
		if (marker && distance <= m_fSafeRadius)
		{
			SRP_JournalComponent.Log("DECONNEXION", string.Format("%1 : absent %2 min mais à %3 m de la base, il reprendra sa partie", left.m_sName, m_iMinutes, Math.Round(distance)));
			return;
		}
		string why = string.Format("absent %1 min à %2 m de la base", m_iMinutes, Math.Round(distance));
		if (shutdown)
			why = "serveur arrêté pendant l'absence, hors de la base";
		MarkDead(left.m_sIdentity, left.m_sName, why);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveBody(SRP_LeftBody left)
	{
		if (left.m_Body && !left.m_Body.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(left.m_Body);
		left.m_Body = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Mort en absence : la fiche perd position et inventaire, et le joueur sera prévenu à son retour
	protected void MarkDead(string identity, string name, string why)
	{
		SRP_PlayerRecord record = SRP_PlayerRecord.Load(identity);
		if (!record)
			return;
		record.m_bHasPosition = false;
		record.m_sInventory = "";
		record.m_sMortEnAbsence = why;
		record.Save();
		SRP_JournalComponent.Log("DECONNEXION", string.Format("%1 [%2] : MORT en absence (%3)", name, identity, why));
	}

	//------------------------------------------------------------------------------------------------
	//! Retire tous les corps en attente sans juger personne (remise à zéro). Retourne le nombre.
	int ResetAll()
	{
		int count = m_aBodies.Count();
		for (int i = m_aBodies.Count() - 1; i >= 0; i--)
			RemoveBody(m_aBodies[i]);
		m_aBodies.Clear();
		if (count > 0)
			SRP_JournalComponent.Log("DECONNEXION", string.Format("%1 corps retiré(s) par une remise à zéro", count));
		return count;
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		if (m_aBodies.IsEmpty())
			return "Aucun corps en attente";
		int now = System.GetTickCount();
		string text = "";
		foreach (SRP_LeftBody left : m_aBodies)
		{
			if (!text.IsEmpty())
				text += "\n";
			int remaining = Math.Max(0, (m_iMinutes * 60 * 1000 - (now - left.m_iLeftTick)) / 60000);
			text += string.Format("%1 : grille %2 / %3, encore %4 min", left.m_sName, Math.Round(left.m_vPosition[0]), Math.Round(left.m_vPosition[2]), remaining);
		}
		return text;
	}
}
