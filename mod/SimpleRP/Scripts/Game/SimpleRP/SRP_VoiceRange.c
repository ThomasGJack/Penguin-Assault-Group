//------------------------------------------------------------------------------------------------
// SimpleRP — Portée de voix modulable
// La portée de la voix directe n'est pas réglable en script : elle vit dans le projet audio (.acp)
// du composant de voix, nœud Amplitude (outerRange = distance maximale, slopeFactor = atténuation).
// On donne donc au personnage trois composants de voix, chacun avec son projet audio (override
// additif de Character_Base.et, le prefab vanilla dont héritent tous les personnages), et le
// contrôleur de voix choisit lequel capture le micro.
// Touche « SRP_VoiceRange » (F9 par défaut, réglable dans les contrôles) : chuchoté 5 m, normal
// 30 m, fort 68 m (le projet audio vanilla). Le composant fort est déclaré en premier sur le prefab :
// c'est lui que le jeu prend par défaut, la voix marche donc même sans jamais toucher à la touche. La radio suit : l'émetteur est branché sur le composant actif.
// Affichage : un message dans le chat à chaque changement, rien de permanent (HUD minimal).
// Les portées affichées ici doivent correspondre aux outerRange des trois projets audio.
// Un seul composant écoute le micro à la fois : à chaque changement de composant (Game Master ouvert ou fermé,
// réapparition, touche), l'ancien est coupé, et un filet de sécurité coupe les autres à chaque fin de parole.
// Sans cela, un composant laissé à l'écoute continuait d'émettre (voix en double, émission radio sans fin : carte #64).
// Phase 5 — étape B
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Voix directe, portée courte (chuchoté)")]
class SRP_VoNComponentNearClass : SCR_VoNComponentClass
{
}

class SRP_VoNComponentNear : SCR_VoNComponent
{
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Voix directe, portée normale")]
class SRP_VoNComponentNormalClass : SCR_VoNComponentClass
{
}

class SRP_VoNComponentNormal : SCR_VoNComponent
{
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Voix directe, portée forte (cri)")]
class SRP_VoNComponentLoudClass : SCR_VoNComponentClass
{
}

class SRP_VoNComponentLoud : SCR_VoNComponent
{
}

//------------------------------------------------------------------------------------------------
modded class SCR_VONController
{
	static const string SRP_ACTION_RANGE = "SRP_VoiceRange";
	static const string SRP_ACTION_STAFF = "SRP_StaffMenu";		// F10 : menu Staff (le serveur vérifie le droit)

	// Portées, dans l'ordre de la touche ; les valeurs suivent les projets audio von_5m / von_30m / von
	protected static const int SRP_RANGE_COUNT = 3;
	protected ref array<int> m_aSRPRangeMeters = {5, 30, 68};
	protected ref array<string> m_aSRPRangeNames = {"chuchoté", "normal", "fort"};

	protected int m_iSRPRange = 2;			// index de départ : fort, le comportement vanilla
	protected IEntity m_SRPEntity;
	protected ref array<SCR_VoNComponent> m_aSRPComponents = {};
	protected bool m_bSRPListening;
	protected int m_iSRPApplyTries;			// personnage pas encore connu du contrôleur : nouvelles tentatives

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (System.IsConsoleApp())
			return;	// serveur dédié : pas de clavier
		// Le contrôleur local n'est pas forcément désigné à cet instant : on réessaie chaque seconde
		GetGame().GetCallqueue().CallLater(SRP_EnsureListener, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	//! La touche est enregistrée une fois, sur le contrôleur du joueur local seulement
	protected void SRP_EnsureListener()
	{
		if (m_bSRPListening)
		{
			GetGame().GetCallqueue().Remove(SRP_EnsureListener);
			return;
		}
		if (GetGame().GetPlayerController() != GetOwner())
			return;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		input.AddActionListener(SRP_ACTION_RANGE, EActionTrigger.DOWN, SRP_CycleRange);
		input.AddActionListener(SRP_ACTION_STAFF, EActionTrigger.DOWN, SRP_OpenStaffMenu);
		m_bSRPListening = true;
		GetGame().GetCallqueue().Remove(SRP_EnsureListener);
		Print("[SRP] Voix : touche SRP_VoiceRange enregistrée sur le contrôleur local", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! F10 : demande le menu Staff au serveur, qui refuse aux non-Staff
	protected void SRP_OpenStaffMenu()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.SRP_RequestAdmin("");
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(SRP_EnsureListener);
		GetGame().GetCallqueue().Remove(SRP_ApplyRangeDeferred);
		if (m_bSRPListening)
		{
			InputManager input = GetGame().GetInputManager();
			if (input)
			{
				input.RemoveActionListener(SRP_ACTION_RANGE, EActionTrigger.DOWN, SRP_CycleRange);
				input.RemoveActionListener(SRP_ACTION_STAFF, EActionTrigger.DOWN, SRP_OpenStaffMenu);
			}
			m_bSRPListening = false;
		}
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Quand le contrôleur reçoit un composant de voix (Game Master ouvert ou fermé, changement de personnage) : le
	//! vanilla prend le composant de l'éditeur, ou le premier composant du personnage. On applique la portée choisie un
	//! instant plus tard, quand l'entité contrôlée est connue du contrôleur (elle ne l'est pas encore pendant cet appel).
	//! Le vanilla change seulement de composant : l'ancien, s'il écoutait le micro, continuait d'émettre. On arrête donc
	//! proprement la parole en cours (le joueur rappuie pour parler) et on coupe l'ancien composant.
	override void SetVONComponent(SCR_VoNComponent VONComp)
	{
		SCR_VoNComponent previous = m_VONComp;
		bool changing = SRP_IsLocal() && previous != null && previous != VONComp;
		if (changing && (m_bIsActive || m_bIsToggledDirect))
		{
			if (m_bIsToggledDirect)
			{
				m_bIsToggledDirect = false;
				m_OnVONActiveToggled.Invoke(false, false);
			}
			DeactivateVON(m_eVONType);	// type en cours : pas de « clac » radio d'ACE si c'était de la voix directe
		}

		super.SetVONComponent(VONComp);

		if (changing)
		{
			previous.SetCapture(false);
			// Le nouveau composant n'écoute pas le micro tant que personne ne parle (reste d'une capture précédente)
			if (VONComp && !m_bIsActive)
				VONComp.SetCapture(false);
		}

		GetGame().GetCallqueue().Remove(SRP_ApplyRangeDeferred);
		if (!VONComp)
		{
			m_SRPEntity = null;
			m_aSRPComponents.Clear();
			return;
		}
		m_iSRPApplyTries = 0;
		GetGame().GetCallqueue().CallLater(SRP_ApplyRangeDeferred, 200, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Retrouve les trois composants du personnage contrôlé et bascule sur celui de la portée en cours. Une parole
	//! commencée entre-temps (dans les 200 ms) passe sur le nouveau composant, l'ancien cesse d'écouter le micro.
	protected void SRP_ApplyRangeDeferred()
	{
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;
		IEntity entity = pc.GetControlledEntity();
		if (!entity || !ChimeraCharacter.Cast(entity) || !m_VONComp)
		{
			// Le personnage n'est pas encore connu : on réessaie (sinon la portée affichée ne serait pas la vraie)
			m_iSRPApplyTries++;
			if (m_iSRPApplyTries <= 10)
				GetGame().GetCallqueue().CallLater(SRP_ApplyRangeDeferred, 300, false);
			return;
		}
		if (!SRP_Belongs(entity, m_VONComp))
			return;	// composant d'éditeur ou d'une autre entité : on n'y touche pas

		if (entity != m_SRPEntity)
		{
			m_SRPEntity = entity;
			SRP_CollectComponents(entity);
		}
		SCR_VoNComponent ranged = SRP_ComponentForRange(m_iSRPRange);
		if (ranged && ranged != m_VONComp)
		{
			SCR_VoNComponent previous = m_VONComp;
			ranged.SetCommMethod(previous.GetCommMethod());
			ranged.SetTransmitRadio(previous.GetTransmitRadio());
			previous.SetCapture(false);
			super.SetVONComponent(ranged);
			if (m_bIsActive)
				ranged.SetCapture(true);
		}
		SRP_StopStrayCaptures();
	}

	//------------------------------------------------------------------------------------------------
	//! Filet de sécurité à chaque fin de parole : aucun composant de voix du personnage, sauf l'actif, n'écoute le micro
	override void DeactivateVON(EVONTransmitType transmitType = EVONTransmitType.NONE)
	{
		super.DeactivateVON(transmitType);
		SRP_StopStrayCaptures();
	}

	//------------------------------------------------------------------------------------------------
	//! Coupe le micro de tous les composants de voix du personnage contrôlé (les trois portées et celui du jeu), sauf
	//! le composant actif. Quand le Game Master est ouvert, l'actif est celui de l'éditeur : ceux du personnage se taisent.
	protected void SRP_StopStrayCaptures()
	{
		if (!SRP_IsLocal())
			return;
		if (m_VONComp && !m_bIsActive)
			m_VONComp.SetCapture(false);
		PlayerController pc = PlayerController.Cast(GetOwner());
		if (!pc)
			return;
		IEntity entity = pc.GetControlledEntity();
		if (!entity)
			return;
		array<Managed> found = {};
		entity.FindComponents(SCR_VoNComponent, found);
		foreach (Managed candidate : found)
		{
			SCR_VoNComponent component = SCR_VoNComponent.Cast(candidate);
			if (component && component != m_VONComp)
				component.SetCapture(false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le contrôleur du joueur de cette machine (pas celui d'un autre joueur, ni le serveur dédié)
	protected bool SRP_IsLocal()
	{
		return GetGame().GetPlayerController() == GetOwner();
	}

	//------------------------------------------------------------------------------------------------
	protected bool SRP_Belongs(IEntity entity, SCR_VoNComponent component)
	{
		array<Managed> found = {};
		entity.FindComponents(SCR_VoNComponent, found);
		foreach (Managed candidate : found)
		{
			if (candidate == component)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_CollectComponents(IEntity entity)
	{
		m_aSRPComponents.Clear();
		if (!entity)
			return;
		m_aSRPComponents.Insert(SCR_VoNComponent.Cast(entity.FindComponent(SRP_VoNComponentNear)));
		m_aSRPComponents.Insert(SCR_VoNComponent.Cast(entity.FindComponent(SRP_VoNComponentNormal)));
		m_aSRPComponents.Insert(SCR_VoNComponent.Cast(entity.FindComponent(SRP_VoNComponentLoud)));
	}

	//------------------------------------------------------------------------------------------------
	protected SCR_VoNComponent SRP_ComponentForRange(int range)
	{
		if (range < 0 || range >= m_aSRPComponents.Count())
			return null;
		return m_aSRPComponents[range];
	}

	//------------------------------------------------------------------------------------------------
	//! Touche : portée suivante. La capture, la méthode (directe / radio) et l'émetteur passent au nouveau composant.
	protected void SRP_CycleRange()
	{
		Print("[SRP] Voix : touche pressée", LogLevel.NORMAL);
		if (m_aSRPComponents.IsEmpty() || !m_VONComp)
		{
			SRP_ShowRange("indisponible (personnage sans composants de portée)");
			return;
		}

		int next = m_iSRPRange + 1;
		if (next >= SRP_RANGE_COUNT)
			next = 0;

		// Game Master ouvert (le composant actif est celui de l'éditeur) ou personnage en train de changer : on retient
		// la portée, elle s'appliquera au retour. Changer de composant ici brancherait celui du personnage à la place
		// de celui de l'éditeur.
		if (m_aSRPComponents.Find(m_VONComp) < 0)
		{
			m_iSRPRange = next;
			SRP_ShowRange(string.Format("%1 (%2 m), appliquée au retour en jeu", m_aSRPRangeNames[m_iSRPRange], m_aSRPRangeMeters[m_iSRPRange]));
			return;
		}

		SCR_VoNComponent target = SRP_ComponentForRange(next);
		if (!target)
		{
			SRP_ShowRange("indisponible pour cette portée");
			return;
		}

		SCR_VoNComponent current = m_VONComp;
		target.SetCommMethod(current.GetCommMethod());
		target.SetTransmitRadio(current.GetTransmitRadio());

		// On coupe proprement l'émission en cours avant de changer de composant (le joueur rappuie pour parler).
		// La bascule d'abord, puis la parole en cours avec son type : pas de « clac » radio d'ACE en voix directe.
		SetVONProximityToggle(false);
		DeactivateVON(m_eVONType);

		m_iSRPRange = next;
		super.SetVONComponent(target);

		SRP_ShowRange(string.Format("%1 (%2 m)", m_aSRPRangeNames[m_iSRPRange], m_aSRPRangeMeters[m_iSRPRange]));
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_ShowRange(string text)
	{
		Print("[SRP] Voix : " + text, LogLevel.NORMAL);
		SCR_HintManagerComponent.ShowCustomHint("Voix : " + text, "Portée de la voix", 3);
	}
}
