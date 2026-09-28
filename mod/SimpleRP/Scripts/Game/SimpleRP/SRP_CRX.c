//------------------------------------------------------------------------------------------------
// SimpleRP — Réglages CRX Enfusion A.I. pour nos IA
// CRX modde les composants vanilla SCR_AIGroupInfoComponent (groupe) et SCR_AIInfoComponent (soldat) ;
// on règle chaque groupe au moment où on le fait apparaître, selon son rôle :
//   GARNISON   défensif, mode rouge, enquête, suppression, retour à la position toujours, maintien de
//              position par soldat autour de SON poste (s_fGarrisonPostHold, 30 m par défaut ; le rayon du secteur
//              ne sert plus qu'au point Defend du groupe : carte #69, correctif du 25/09)
//   ASSAUT     offensif, mode rouge, enquête, suppression, jamais de retour
//   PATROUILLE défaut, mode rouge, enquête, retour en fin d'enquête
//   CIVIL      mode blanc (ne tire jamais), ni enquête ni suppression ; les immobiles tiennent leur place
// Tous les rôles hostiles sont en mode ROUGE (tir à vue) ; le mode blanc est réservé aux civils.
// Rayon d'enquête : -1 par défaut (rayon dynamique de CRX : ils vont vraiment voir d'où vient le tir) pour tous les
// rôles hostiles, patrouilles d'ambiance comprises (carte #69).
// Perception : nos soldats ennemis sont réglés un par un par SRP_EnemySenses (formule SimpleRP, carte #69) ; les
// valeurs par état posées ici (1.3 / 2.5 / 2.5 / 3.0) ne servent que de filet à la formule CRX (état inconnu, soldat
// pas encore pris en main, ennemi en civil du contrôle routier).
// Binômes (carte #69, livraison 2) : un groupe à pied de 4 soldats et plus (garde, assaut, ronde) combat en binômes CRX
// (une moitié fixe pendant que l'autre bouge), choix imposé (pas laissé au mode autonome de CRX) ; jamais un équipage
// (jeep, hélicoptère).
// Les réglages de groupe sont appliqués deux secondes après le spawn. Les réglages de chaque soldat (ApplySoldier)
// sont posés par SRP_EnemySenses au fil de l'arrivée des soldats (la file d'apparition de la 1.8 les livre un par un) ;
// les civils et l'hélicoptère gardent la boucle d'aujourd'hui (soldiers = true).
// Phase 4 — étape D (dépendance CRX Enfusion A.I.)
//------------------------------------------------------------------------------------------------

enum SRP_ECRXRole
{
	GARNISON,
	ASSAUT,
	PATROUILLE,
	CIVIL
}

class SRP_CRX
{
	static bool s_bEnabled = true;
	// Maintien d'un soldat de garde autour de son propre poste (réglé par SRP_EnemyComponent, m_fGarrisonPostHoldRadius ;
	// jamais plus que la zone du groupe ; 0 = ancien comportement, toute la zone autour de son centre). Appliqué par
	// SRP_EnemySenses.UpdateGroup : CRX envoie ses enquêtes et ses déplacements de combat à un point au hasard dans ce
	// cercle, un grand rayon faisait errer les gardes dans tout le village.
	static float s_fGarrisonPostHold = 30;
	static bool s_bLoggedOnce;

	// Perception par état posée dans le composant CRX de chaque soldat (réglée par SRP_EnemyComponent) : filet de la
	// formule CRX. CRX : 1.0 / 2.5 / 2.5 / 3.0.
	static float s_fPerceptionSafe = 1.3;
	static float s_fPerceptionVigilant = 2.5;
	static float s_fPerceptionAlerted = 2.5;
	static float s_fPerceptionThreatened = 3.0;
	static bool s_bFlashlights = true;

	// Rayon d'enquête des rôles hostiles (réglé par SRP_EnemyComponent) : -1 = rayon dynamique de CRX (il sert de
	// précision d'arrivée : ils vont jusqu'au tir) ; 300 = comportement d'avant 1.0.27
	static int s_iInvestigateRadius = -1;

	// Binômes CRX pour les groupes hostiles de 4 soldats et plus (réglé par SRP_EnemyComponent)
	static bool s_bFireteams = true;

	//------------------------------------------------------------------------------------------------
	//! Programme l'application des réglages sur un groupe (deux secondes plus tard). soldiers = false : réglages du
	//! groupe seulement (nos soldats ennemis sont réglés un par un par SRP_EnemySenses) ; true : aussi la boucle des
	//! soldats d'aujourd'hui (civils, hélicoptère, ennemi en civil). fireteams = false : jamais de binômes (équipage
	//! d'une jeep ; les groupes « soldiers » n'en ont jamais non plus)
	static void Apply(SCR_AIGroup group, int role, vector holdOrigin, float holdRadius = -1, bool soldiers = true, bool fireteams = true)
	{
		if (!s_bEnabled || !group)
			return;
		GetGame().GetCallqueue().CallLater(ApplyNow, 2000, false, group, role, holdOrigin, holdRadius, soldiers, fireteams);
	}

	//------------------------------------------------------------------------------------------------
	//! Vitesse de déplacement imposée à tout le groupe (marche, course, sprint), indépendante de CRX
	static void SetGroupSpeed(SCR_AIGroup group, EMovementType speed)
	{
		if (!group)
			return;
		SCR_AIGroupSettingsComponent settings = SCR_AIGroupSettingsComponent.Cast(group.FindComponent(SCR_AIGroupSettingsComponent));
		if (!settings)
		{
			if (!s_bLoggedSpeed)
			{
				s_bLoggedSpeed = true;
				Print("[SRP] Vitesse IA : pas de SCR_AIGroupSettingsComponent sur le prefab de groupe, réglage ignoré", LogLevel.WARNING);
			}
			return;
		}
		SCR_AIGroupCharactersMovementSpeedSetting setting = SCR_AIGroupCharactersMovementSpeedSetting.Create(SCR_EAISettingOrigin.EDITOR, speed);
		settings.AddSetting(setting, false, true);
	}

	static bool s_bLoggedSpeed;

	//------------------------------------------------------------------------------------------------
	//! Nouvelle partie et fin de partie (SRP_EnemyComponent.OnPostInit et OnDelete) : les avertissements écrits une seule
	//! fois le sont de nouveau à la partie suivante (partie Workbench relancée, scénario redémarré). Les réglages ne sont
	//! pas touchés : OnPostInit les recopie.
	static void ResetStatics()
	{
		s_bLoggedOnce = false;
		s_bLoggedSpeed = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Partie groupe (toujours) ; partie soldats seulement si « soldiers » (civils, hélicoptère, ennemi en civil) ;
	//! binômes seulement pour nos groupes à pied (« fireteams », et jamais avec « soldiers »)
	static void ApplyNow(SCR_AIGroup group, int role, vector holdOrigin, float holdRadius, bool soldiers, bool fireteams)
	{
		if (!group || group.IsDeleted())
			return;

		if (role == SRP_ECRXRole.CIVIL)
			SetGroupSpeed(group, EMovementType.WALK);

		SCR_AIGroupInfoComponent info = SCR_AIGroupInfoComponent.Cast(group.FindComponent(SCR_AIGroupInfoComponent));
		if (!info)
		{
			if (!s_bLoggedOnce)
			{
				s_bLoggedOnce = true;
				Print("[SRP] CRX : pas de SCR_AIGroupInfoComponent sur le groupe, réglages ignorés", LogLevel.WARNING);
			}
			return;
		}

		switch (role)
		{
			case SRP_ECRXRole.GARNISON:
			{
				// Neutre jusqu'au contact (menace vanilla), puis tir à vue ; un tir entendu à 500 m les fait venir chercher,
				// bâtiments compris, cinq minutes, puis ils rentrent tenir leur zone (le maintien de position les borne)
				info.SetCombatBehaviorType(CRX_EAICombatBehaviorType.DEFENSIVE);
				info.SetCombatMode(CRX_EAICombatMode.RED);
				info.SetCombatModeAutonomous(false);
				info.SetWeaponFiredReactionDistance(500);
				info.SetInvestigate(true);
				info.SetInvestigateDuration(300);
				info.SetInvestigateRadius(s_iInvestigateRadius);
				info.SetInvestigateBuildingSearch(true);
				info.SetSuppress(true);
				info.SetReturnToPositionOriginType(CRX_EAIReturnToPositionOriginType.ALWAYS);
				break;
			}
			case SRP_ECRXRole.ASSAUT:
			{
				info.SetCombatBehaviorType(CRX_EAICombatBehaviorType.OFFENSIVE);
				info.SetCombatMode(CRX_EAICombatMode.RED);
				info.SetCombatModeAutonomous(false);
				info.SetWeaponFiredReactionDistance(600);
				info.SetInvestigate(true);
				info.SetInvestigateDuration(300);
				info.SetInvestigateRadius(s_iInvestigateRadius);
				info.SetInvestigateBuildingSearch(true);
				info.SetSuppress(true);
				info.SetReturnToPositionOriginType(CRX_EAIReturnToPositionOriginType.NEVER);
				break;
			}
			case SRP_ECRXRole.PATROUILLE:
			{
				// Poursuite courte : une patrouille va voir d'où vient le tir pendant trois minutes, puis revient sur son
				// itinéraire (retour à la position à la fin de l'enquête)
				info.SetCombatBehaviorType(CRX_EAICombatBehaviorType.DEFAULT);
				info.SetCombatMode(CRX_EAICombatMode.RED);
				info.SetCombatModeAutonomous(false);
				info.SetWeaponFiredReactionDistance(400);
				info.SetInvestigate(true);
				info.SetInvestigateDuration(180);
				info.SetInvestigateRadius(s_iInvestigateRadius);
				info.SetInvestigateBuildingSearch(false);
				info.SetSuppress(true);
				info.SetReturnToPositionOriginType(CRX_EAIReturnToPositionOriginType.INVESTIGATING);
				break;
			}
			case SRP_ECRXRole.CIVIL:
			{
				info.SetCombatBehaviorType(CRX_EAICombatBehaviorType.DEFAULT);
				info.SetCombatMode(CRX_EAICombatMode.WHITE);
				info.SetCombatModeAutonomous(false);
				info.SetInvestigate(false);
				info.SetSuppress(false);
				info.SetReturnToPositionOriginType(CRX_EAIReturnToPositionOriginType.NEVER);
				break;
			}
		}

		// Binômes : un de nos groupes hostiles À PIED de 4 soldats et plus combat en binômes (une moitié fixe, l'autre
		// bouge) ; jamais un équipage (hélicoptère : « soldiers » ; jeep : « fireteams » faux), ni un civil
		bool hostile = role == SRP_ECRXRole.GARNISON || role == SRP_ECRXRole.ASSAUT || role == SRP_ECRXRole.PATROUILLE;
		if (s_bFireteams && fireteams && !soldiers && hostile && group.m_aUnitPrefabSlots && group.m_aUnitPrefabSlots.Count() >= 4)
		{
			info.SetCombatMovementType(CRX_EAICombatMovementType.FIRETEAM);
			info.SetCombatMovementTypeAutonomous(false);
		}

		// Nos soldats ennemis sont réglés un par un par SRP_EnemySenses (ApplySoldier), au fil de leur arrivée
		if (!soldiers)
			return;

		// Boucle d'aujourd'hui (civils, hélicoptère, ennemi en civil) : perception et lampes des hostiles, maintien de
		// position si un rayon est donné (sans le relâcher explicitement sinon)
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SoldierSettings(agent, role, holdOrigin, holdRadius, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Réglages d'un soldat ennemi (appelé par SRP_EnemySenses pour chaque soldat arrivé, et à chaque changement de
	//! rôle ou de maintien) : perception par état (filet de la formule CRX), lampe autonome pour une patrouille, maintien
	//! de position et observation au repos ; sans rayon de maintien, le maintien est relâché explicitement
	static void ApplySoldier(AIAgent agent, int role, vector holdOrigin, float holdRadius)
	{
		if (!s_bEnabled)
			return;
		SoldierSettings(agent, role, holdOrigin, holdRadius, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Les réglages d'un soldat ; « release » : relâcher explicitement le maintien quand aucun rayon n'est donné
	protected static void SoldierSettings(AIAgent agent, int role, vector holdOrigin, float holdRadius, bool release)
	{
		if (!agent)
			return;
		SCR_AIInfoComponent senses = SCR_AIInfoComponent.Cast(agent.FindComponent(SCR_AIInfoComponent));
		if (!senses)
			return;

		// Perception et lampes : tous nos soldats ennemis
		if (role != SRP_ECRXRole.CIVIL)
		{
			senses.SetPerceptionSafe(s_fPerceptionSafe);
			senses.SetPerceptionVigilant(s_fPerceptionVigilant);
			senses.SetPerceptionAlerted(s_fPerceptionAlerted);
			senses.SetPerceptionThreatened(s_fPerceptionThreatened);
			if (s_bFlashlights && role == SRP_ECRXRole.PATROUILLE)
				senses.SetFlashlightState(CRX_EAIFlashlightState.AUTONOMOUS);	// la nuit, une patrouille se voit venir
		}

		// Maintien de position par soldat
		if (holdRadius > 0)
		{
			IEntity entity = agent.GetControlledEntity();
			vector origin = holdOrigin;
			if (entity && holdOrigin == vector.Zero)
				origin = entity.GetOrigin();
			senses.SetHoldPosition(true);
			senses.SetHoldPositionOrigin(origin);
			senses.SetHoldPositionRadius(holdRadius);
			senses.SetIdleObserve(true);	// au repos, ils regardent autour d'eux
		}
		else if (release)
			senses.SetHoldPosition(false);	// ronde, assaut : aucun maintien (un rôle changé ne garde pas l'ancien)
	}
}
