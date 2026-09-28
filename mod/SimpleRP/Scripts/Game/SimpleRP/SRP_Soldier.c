//------------------------------------------------------------------------------------------------
// SimpleRP — Gestion d'un soldat au contact
// Action « Gestion du soldat » sur tout personnage (override additif de Character_Base.et). Le serveur
// construit un menu selon les droits de celui qui regarde :
// - Certifications : seul l'instructeur habilité pour cette certification (et le Staff) ajoute ou retire ;
// - Grade : Lieutenant et Capitaine (et le Staff), jamais au-dessus de son propre grade ;
// - Instructeurs : seul un Adjudant (et le Staff) habilite un joueur comme instructeur d'une certification
//   (instructeur AT, instructeur TP…). Pas d'instructeur global : une habilitation par certification.
// Le menu (SRP_SoldierMenu, même écran que le terminal) renvoie la ligne choisie au serveur, qui revérifie
// et rouvre le menu à jour.
// Phase 5 — étape C
//------------------------------------------------------------------------------------------------

class SRP_SoldierAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int userId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		int targetId = SRP_Utils.GetPlayerIdFromEntity(pOwnerEntity);
		if (userId < 0)
			return;
		if (targetId <= 0)
		{
			// Pas un joueur : peut-être un conducteur à l'arrêt à un point de contrôle routier (SRP_Checkpoint)
			if (!SRP_Checkpoint.OpenFor(userId, pOwnerEntity))
			{
				SRP_Utils.HintPlayer(userId, "Contrôle", "Seul un conducteur arrêté à un point de contrôle routier (mission Contrôle routier) peut être contrôlé.");
				SRP_Utils.NotifyPlayer(userId, "Seul un conducteur arrêté à un point de contrôle routier peut être contrôlé.");
			}
			return;
		}

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;
		players.SendSoldierMenu(userId, targetId);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Gestion du soldat";
		// Sur un civil (contrôle routier), l'action porte un autre nom
		IEntity owner = GetOwner();
		if (owner && GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner) <= 0)
		{
			FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(owner.FindComponent(FactionAffiliationComponent));
			if (affiliation && affiliation.GetAffiliatedFaction() && affiliation.GetAffiliatedFaction().GetFactionKey() == "CIV")
				outName = "Contrôler (point de contrôle)";
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		// Pas sur soi-même, ni sur un mort
		IEntity owner = GetOwner();
		if (!owner || owner == user || SRP_Utils.IsDead(owner))
			return false;
		// Sur un joueur : « Gestion du soldat », seulement pour qui a un droit (officier, Adjudant, instructeur, Staff).
		// Sur un civil : « Contrôler », seulement près d'un point de contrôle routier en place.
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner) > 0)
			return SRP_Rights.Has(user, SRP_Rights.GESTION);
		return SRP_Rights.Has(user, SRP_Rights.AU_CONTROLE);
	}

	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! Le menu : même mécanique que le terminal, commande « soldat »
class SRP_SoldierMenu : SRP_ListMenu
{
	override protected string GetMenuTitle() { return "Gestion du soldat"; }
	override protected string GetConfirmLabel() { return "Appliquer"; }
	override protected string GetCommand() { return "soldat"; }

	static void Open(string content)
	{
		OpenPreset(ChimeraMenuPreset.SRP_SoldierMenu, content);
	}
}

//------------------------------------------------------------------------------------------------
//! Construction du menu et exécution, côté serveur (utilisé par SRP_PlayerManagerComponent)
class SRP_SoldierManagement
{
	//------------------------------------------------------------------------------------------------
	//! Peut gérer les grades : officier ou Staff
	static bool CanManageGrades(SRP_PlayerRecord user)
	{
		return user && (user.m_bStaff || SRP_Grades.IsOfficier(user.m_iGrade));
	}

	//------------------------------------------------------------------------------------------------
	//! Peut gérer les instructeurs : Adjudant ou Staff
	static bool CanManageInstructors(SRP_PlayerRecord user)
	{
		return user && (user.m_bStaff || user.m_iGrade == SRP_EGrade.ADJUDANT);
	}

	//------------------------------------------------------------------------------------------------
	static string BuildMenu(SRP_PlayerManagerComponent players, int userId, int targetId)
	{
		SRP_PlayerRecord user = players.GetRecord(userId);
		SRP_PlayerRecord target = players.GetRecord(targetId);
		if (!user || !target)
			return "M|Fiche introuvable";

		string text = string.Format("M|%1 — %2\nCertifs : %3\nÉtats de service : %4 h, %5 mission(s)",
			target.m_sName, SRP_Grades.GetName(target.m_iGrade), SRP_Certifs.MaskToString(target.m_iCertifs),
			target.m_iPlaytimeSeconds / 3600, target.m_iMissions);

		bool anything = false;

		// Certifications : celles que l'utilisateur est habilité à délivrer
		string certifLines = "";
		int certif = 1;
		while (certif <= SRP_Certifs.LAST)
		{
			if (certif != SRP_ECertif.INSTRUCTEUR && players.IsInstructeur(userId, certif))
			{
				if (SRP_Certifs.Has(target.m_iCertifs, certif))
					certifLines += string.Format("\nI|certif:%1:%2:off|Retirer %3", targetId, certif, SRP_Certifs.GetFullName(certif));
				else
				{
					string reason;
					if (players.CanGrant(target, certif, reason))
						certifLines += string.Format("\nI|certif:%1:%2:on|Accorder %3", targetId, certif, SRP_Certifs.GetFullName(certif));
					else
						certifLines += string.Format("\nH|%1 : %2", SRP_Certifs.GetName(certif), reason);
				}
			}
			certif = certif * 2;
		}
		if (!certifLines.IsEmpty())
		{
			text += "\nH|Certifications" + certifLines;
			anything = true;
		}

		// Grade : officiers
		if (CanManageGrades(user))
		{
			text += "\nH|Grade";
			int next = target.m_iGrade + 1;
			if (SRP_Grades.IsValide(next) && (user.m_bStaff || next <= user.m_iGrade))
			{
				string missing;
				if (next > SRP_EGrade.ADJUDANT || players.MeetsGrade(target, next, missing))
					text += string.Format("\nI|grade:%1:%2|Promouvoir %3", targetId, next, SRP_Grades.GetName(next));
				else
				{
					text += string.Format("\nH|%1 : il manque %2", SRP_Grades.GetName(next), missing);
					if (user.m_bStaff)
						text += string.Format("\nI|grade:%1:%2:force|Promouvoir %3 (forcer, Staff)", targetId, next, SRP_Grades.GetName(next));
				}
			}
			int prev = target.m_iGrade - 1;
			if (SRP_Grades.IsValide(prev))
				text += string.Format("\nI|grade:%1:%2|Rétrograder %3", targetId, prev, SRP_Grades.GetName(prev));

			text += "\nH|États de service (après le débriefing)";
			text += string.Format("\nI|mission:%1:1|+1 mission réussie (a %2)", targetId, target.m_iMissions);
			if (target.m_iMissions > 0)
				text += string.Format("\nI|mission:%1:-1|-1 mission (correction)", targetId);
			anything = true;
		}

		// Instructeurs : Adjudant. Une habilitation par certification, parmi celles que le soldat détient
		if (CanManageInstructors(user))
		{
			text += "\nH|Instructeur de… (responsable : Adjudant)";
			bool any = false;
			int c = 1;
			while (c <= SRP_Certifs.LAST)
			{
				if (c != SRP_ECertif.INSTRUCTEUR && SRP_Certifs.Has(target.m_iCertifs, c))
				{
					any = true;
					if (SRP_Certifs.Has(target.m_iInstructeurCertifs, c))
						text += string.Format("\nI|hab:%1:%2:off|[x] Instructeur %3 (retirer)", targetId, c, SRP_Certifs.GetName(c));
					else
						text += string.Format("\nI|hab:%1:%2:on|[ ] Instructeur %3 (habiliter)", targetId, c, SRP_Certifs.GetName(c));
				}
				c = c * 2;
			}
			if (!any)
				text += "\nH|Aucune certification détenue : on n'instruit que ce qu'on a";
			anything = true;
		}

		if (!anything)
			text += "\nM|Tu n'as aucun droit de gestion sur ce soldat (instructeur habilité, officier, ou Adjudant)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Exécute une ligne du menu. Retourne le message au joueur ; le menu est rouvert par l'appelant.
	static string Execute(SRP_PlayerManagerComponent players, int userId, string key, out int targetId)
	{
		targetId = -1;
		array<string> parts = {};
		key.Split(":", parts, true);
		if (parts.Count() < 3 || !SRP_Utils.IsNumeric(parts[1]))
			return "Ligne invalide";

		targetId = parts[1].ToInt();
		SRP_PlayerRecord user = players.GetRecord(userId);
		SRP_PlayerRecord target = players.GetRecord(targetId);
		if (!user || !target)
			return "Fiche introuvable";

		string author = user.m_sName;
		if (user.m_bStaff)
			author += " (Staff)";
		string action = parts[0];
		string reason;

		if (action == "certif")
		{
			if (parts.Count() < 4 || !SRP_Utils.IsNumeric(parts[2]))
				return "Ligne invalide";
			int certif = parts[2].ToInt();
			bool on = parts[3] == "on";

			if (certif == SRP_ECertif.INSTRUCTEUR)
				return "Plus de certification Instructeur : habilite par certification";
			if (!players.IsInstructeur(userId, certif))
				return "Tu n'es pas habilité pour " + SRP_Certifs.GetName(certif);

			if (!players.SetCertif(targetId, certif, on, author, false, reason))
				return "Refusé : " + reason;
			if (on)
				return string.Format("%1 accordée à %2", SRP_Certifs.GetFullName(certif), target.m_sName);
			return string.Format("%1 retirée à %2%3", SRP_Certifs.GetFullName(certif), target.m_sName, reason);
		}

		if (action == "grade")
		{
			if (!CanManageGrades(user))
				return "Seuls les officiers gèrent les grades";
			if (!SRP_Utils.IsNumeric(parts[2]))
				return "Ligne invalide";
			int grade = parts[2].ToInt();
			bool force = parts.Count() >= 4 && parts[3] == "force" && user.m_bStaff;
			if (!user.m_bStaff && grade > user.m_iGrade)
				return "Tu ne peux pas donner un grade supérieur au tien";
			if (!players.SetGrade(targetId, grade, author, force, reason))
				return "Refusé : " + reason;
			return string.Format("%1 est maintenant %2", target.m_sName, SRP_Grades.GetName(grade));
		}

		if (action == "mission")
		{
			if (!CanManageGrades(user))
				return "Seuls les officiers tiennent les états de service";
			if (parts[2] == "-1")
				return players.AdjustMissions(targetId, -1, author);
			return players.AdjustMissions(targetId, 1, author);
		}

		if (action == "hab")
		{
			if (!CanManageInstructors(user))
				return "Seul un Adjudant (ou le Staff) gère les habilitations";
			if (parts.Count() < 4 || !SRP_Utils.IsNumeric(parts[2]))
				return "Ligne invalide";
			int certif = parts[2].ToInt();
			bool on = parts[3] == "on";
			if (!players.SetInstructeur(targetId, certif, on, author, reason))
				return "Refusé : " + reason;
			if (on)
				return string.Format("%1 habilité instructeur %2", target.m_sName, SRP_Certifs.GetName(certif));
			return string.Format("Habilitation %1 retirée à %2", SRP_Certifs.GetName(certif), target.m_sName);
		}

		return "Action inconnue";
	}
}
