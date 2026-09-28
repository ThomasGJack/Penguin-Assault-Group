//------------------------------------------------------------------------------------------------
// SimpleRP — Poste de secours
// Deux actions sur les lits du poste de secours (ActionsManagerComponent du prefab SRP_PosteDeSecours) :
//   - « Se faire soigner » : soins complets, gratuits, pour celui qui agit ;
//   - « Soigner le blessé allongé » : soins complets et réveil d'un joueur INCONSCIENT couché à moins de
//     2,5 m du lit (visible seulement s'il y a un tel blessé).
// La règle « refusé si un médecin certifié est connecté » existe encore mais est désactivée par défaut.
// Avec ACE Medical, FullHeal remet aussi les constantes (pouls, arrêt cardiaque) à neuf ; le réveil passe
// par UpdateConsciousness puis, au besoin, SetUnconscious(false).
// Phase 2 — étape E2 ; revu carte #53
//------------------------------------------------------------------------------------------------

//! Outils partagés (poste de secours, soin Staff, commande « etat »)
class SRP_MedicalHelper
{
	//------------------------------------------------------------------------------------------------
	//! Soins complets, puis réveil si le personnage était inconscient. Retourne false sans gestionnaire de dégâts.
	static bool HealAndWake(IEntity character)
	{
		if (!character)
			return false;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (!damage)
			return false;

		damage.FullHeal();

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.IsUnconscious())
		{
			// Le gestionnaire réévalue l'inconscience d'après les zones (toutes soignées) ; en dernier recours on force
			damage.UpdateConsciousness();
			if (controller.IsUnconscious())
				controller.SetUnconscious(false);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Le personnage d'un joueur inconscient (pas mort) le plus proche d'un point, dans le rayon donné ;
	//! null s'il n'y en a pas. Le joueur exclu (celui qui agit) n'est pas retenu.
	static IEntity FindUnconsciousNear(vector position, float radius, int excludePlayerId)
	{
		IEntity best;
		float bestDistance = radius;
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			if (id == excludePlayerId)
				continue;
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (!character)
				continue;
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
			if (!controller || controller.IsDead() || !controller.IsUnconscious())
				continue;
			float distance = vector.Distance(position, character.GetOrigin());
			if (distance <= bestDistance)
			{
				bestDistance = distance;
				best = character;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé d'un état vital ACE
	static string VitalStateName(ACE_Medical_EVitalStateID state)
	{
		if (state == ACE_Medical_EVitalStateID.STABLE)
			return "stable";
		if (state == ACE_Medical_EVitalStateID.UNSTABLE)
			return "instable";
		if (state == ACE_Medical_EVitalStateID.CRITICAL)
			return "critique";
		if (state == ACE_Medical_EVitalStateID.RESUSCITATION)
			return "en réanimation";
		if (state == ACE_Medical_EVitalStateID.CARDIAC_ARREST)
			return "ARRÊT CARDIAQUE";
		return "inconnu";
	}

	//------------------------------------------------------------------------------------------------
	//! État médical d'un personnage en une ligne : vie, santé, sang, saignements, pouls, position
	static string StateText(IEntity character)
	{
		if (!character)
			return "pas de personnage (mort ou en attente de réapparition)";

		string life = "vivant";
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (controller)
		{
			if (controller.IsDead())
				life = "MORT";
			else if (controller.IsUnconscious())
				life = "INCONSCIENT";
		}

		string text = life;

		SCR_CharacterDamageManagerComponent damage = SCR_CharacterDamageManagerComponent.Cast(character.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damage)
		{
			int health = Math.Round(damage.GetHealthScaled() * 100);
			text += " · santé " + health.ToString() + " %";

			SCR_CharacterBloodHitZone blood = damage.GetBloodHitZone();
			if (blood)
			{
				int bloodPercent = Math.Round(blood.GetHealthScaled() * 100);
				text += " · sang " + bloodPercent.ToString() + " %";
			}

			array<HitZone> bleeding = damage.GetBleedingHitZones();
			int bleedCount = 0;
			if (bleeding)
				bleedCount = bleeding.Count();
			text += string.Format(" · %1 saignement(s)", bleedCount);
		}
		else
			text += " · pas de gestion de dégâts";

		ACE_Medical_VitalsComponent vitals = ACE_Medical_VitalsComponent.Cast(character.FindComponent(ACE_Medical_VitalsComponent));
		if (vitals)
		{
			int pulse = Math.Round(vitals.GetHeartRate());
			text += string.Format(" · pouls %1 bpm (%2)", pulse, VitalStateName(vitals.GetVitalStateID()));
		}

		text += " · carré " + SRP_FleetManagerComponent.GridOf(character.GetOrigin());
		return text;
	}
}

//------------------------------------------------------------------------------------------------
//! « Se faire soigner » : celui qui agit est remis à neuf
class SRP_MedicalHealAction : ScriptedUserAction
{
	[Attribute("Se faire soigner", UIWidgets.EditBox, "Nom de l'action", category: "SimpleRP")]
	protected string m_sActionName;

	[Attribute("0", UIWidgets.CheckBox, "Refuser les soins si un joueur certifié Médecin (autre que le demandeur) est connecté", category: "SimpleRP")]
	protected bool m_bRefuseIfMedicOnline;

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		// Règle RP (désactivée par défaut) : un médecin en service passe avant le poste automatique
		if (m_bRefuseIfMedicOnline)
		{
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			if (players)
			{
				string medics;
				if (players.GetConnectedWithCertif(SRP_ECertif.MEDECIN, playerId, medics) > 0)
				{
					SRP_Utils.NotifyPlayer(playerId, "Un médecin est en service : " + medics + ". Fais-toi soigner par lui.");
					return;
				}
			}
		}

		if (!SRP_MedicalHelper.HealAndWake(pUserEntity))
		{
			SRP_Utils.NotifyPlayer(playerId, "Impossible de soigner : pas de gestionnaire de dégâts");
			return;
		}

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		SRP_JournalComponent.Log("MEDICAL", playerName + " soigné au poste de secours");
		SRP_Utils.NotifyPlayer(playerId, "Soins terminés, tu es remis à neuf");
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = m_sActionName;
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
//! « Soigner le blessé allongé » : un joueur inconscient couché près du lit est soigné et réveillé
class SRP_MedicalHealOtherAction : ScriptedUserAction
{
	[Attribute("Soigner le blessé allongé", UIWidgets.EditBox, "Nom de l'action", category: "SimpleRP")]
	protected string m_sActionName;

	[Attribute("2.5", UIWidgets.EditBox, "Rayon autour du lit dans lequel on cherche un joueur inconscient, en mètres", category: "SimpleRP")]
	protected float m_fRadius;

	//------------------------------------------------------------------------------------------------
	protected IEntity FindPatient(IEntity user)
	{
		IEntity bed = GetOwner();
		if (!bed)
			return null;
		return SRP_MedicalHelper.FindUnconsciousNear(bed.GetOrigin(), m_fRadius, SRP_Utils.GetPlayerIdFromEntity(user));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		IEntity patient = FindPatient(pUserEntity);
		if (!patient)
		{
			SRP_Utils.NotifyPlayer(playerId, "Aucun blessé inconscient allongé près de ce lit");
			return;
		}

		int patientId = SRP_Utils.GetPlayerIdFromEntity(patient);
		string patientName = GetGame().GetPlayerManager().GetPlayerName(patientId);
		if (!SRP_MedicalHelper.HealAndWake(patient))
		{
			SRP_Utils.NotifyPlayer(playerId, "Impossible de soigner " + patientName + " : pas de gestionnaire de dégâts");
			return;
		}

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		SRP_JournalComponent.Log("MEDICAL", string.Format("%1 soigné et réveillé au poste de secours par %2", patientName, playerName));
		SRP_Utils.NotifyPlayer(playerId, patientName + " est soigné et réveillé");
		SRP_Utils.NotifyPlayer(patientId, "Tu as été soigné au poste de secours par " + playerName);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = m_sActionName;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Visible seulement s'il y a un joueur inconscient allongé près du lit
	override bool CanBeShownScript(IEntity user)
	{
		return FindPatient(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return FindPatient(user) != null;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}
