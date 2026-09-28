//------------------------------------------------------------------------------------------------
// SimpleRP — Sens de l'IA ennemie (carte #69, livraison 1 : « ils voient »)
// A. modded SCR_AICombatComponent : la perception de NOS soldats ennemis (marqués par SRP_Manage) suit la formule
//    SimpleRP : base selon l'état (tranquille, vigilant, alerté, menacé) plus les tirs entendus, moins la suppression ;
//    un ennemi allongé voit moins bien ; facteur de rôle (sentinelle, patrouille, assaut, équipage) ; facteur
//    d'environnement posé toutes les 2 s (vraie lumière, lampe, tirs, fusées, proximité, posture de la cible,
//    buisson). Les autres IA (civils, IA du Game Master, équipage de l'hélicoptère, ennemi en civil du contrôle
//    routier) gardent la formule CRX.
// B. modded SCR_AIGetAimErrorOffset : sous un tir nourri, la dispersion de chaque tir de nos soldats grandit.
// C. SRP_EnemySenses (statique, serveur) : le passage régulier lancé par SRP_EnemyComponent.SensesTick — ce que
//    l'ennemi peut percevoir de chaque joueur vivant, les soldats arrivés réglés un par un (la file d'apparition de la
//    1.8 les livre un par un), la visée de base d'un soldat que CRX n'a pas classé, la révélation de près la nuit, le journal de
//    repérage (catégorie ENNEMI, jamais montré aux joueurs).
// Livraison 2 : pendant une alerte, les soldats TRANQUILLES des groupes proches sont plus vigilants sur place (leur
// perception au calme est multipliée par s_fVigilantFactor, posée à chaque passage par UpdateGroup) ; aucune position
// ne leur est signalée, ils ne quittent pas leur poste.
// Correctifs après l'essai de Jack (25/09) :
// - un soldat de garde tient un court rayon autour de SON poste (pris là où il s'est posé, pas à son point d'apparition)
//   au lieu de tout le secteur ; l'origine n'est plus réimposée à chaque passage ;
// - filet contre les gardes assis ou adossés : un soldat qui flâne et voit un joueur tout près (devant lui, ou déjà dans
//   sa perception) se relève ;
// - le dernier facteur réellement posé et le dernier état de menace sont retenus (journal, ligne de diagnostic toutes
//   les 10 s en mode essai ou avec le réglage de débogage) ;
// - tirs entendus par groupe (SCR_AIDangerReaction_WeaponFired.NotifyGroup) : un tireur est « trahi » même si le groupe
//   l'avait déjà identifié ; visée dans une lunette grossissante : perception multipliée (logique CRX) ;
// - ResetStatics : rien ne survit d'une partie Workbench (ou d'un redémarrage de scénario) à la suivante.
// Commandeur ennemi (carte #76, CO6) : chaque tir de joueur entendu par un de nos groupes est aussi compté dans le
// compte rendu de ce groupe (SRP_Commander.NoteShot, qui ne fait rien sans Commandeur).
// SimpleRP dépend de CRX : ces surcharges passent après les siennes.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! A. Perception de nos soldats ennemis
modded class SCR_AICombatComponent
{
	protected bool m_bSRP_Managed;				// un de nos soldats ennemis : formule SimpleRP
	protected int m_iSRP_Role = -1;				// SRP_ECRXRole appliqué
	protected vector m_vSRP_Hold;				// maintien appliqué (origine, rayon ; -1 = aucun)
	protected float m_fSRP_Hold = -1;
	protected float m_fSRP_RoleFactor = 1;		// sentinelle, patrouille, assaut, équipage
	protected float m_fSRP_EnvCalm = 1;			// visibilité des joueurs, buisson compris : ennemi tranquille ou vigilant
	protected float m_fSRP_EnvCombat = 1;		// sans buisson : ennemi alerté ou menacé
	protected float m_fSRP_Vigilance = 1;		// alerte voisine (livraison 2) : perception au calme multipliée
	protected float m_fSRP_LastFactor = -1;		// dernier facteur passé à SetPerceptionFactor par notre formule (-1 : formule CRX)
	protected int m_iSRP_LastState = -1;		// dernier état de menace lu (EAIThreatState ; -1 : jamais lu)
	protected bool m_bSRP_PostSet;				// poste d'un soldat de garde retenu (maintien court autour de lui)
	protected vector m_vSRP_Post;
	protected int m_iSRP_StillPasses;			// passages de suite où il est tranquille et immobile (poste pris là où il se pose)
	protected bool m_bSRP_WakeLogged;			// réveil (assis ou adossé, joueur en vue) déjà écrit au journal d'essai

	//------------------------------------------------------------------------------------------------
	//! Dernier facteur réellement posé sur la perception de ce soldat par la formule SimpleRP (-1 : formule CRX)
	float SRP_GetLastFactor()
	{
		return m_fSRP_LastFactor;
	}

	//------------------------------------------------------------------------------------------------
	//! Dernier état de menace lu par la formule (valeur de EAIThreatState ; -1 : jamais lu)
	int SRP_GetLastState()
	{
		return m_iSRP_LastState;
	}

	//------------------------------------------------------------------------------------------------
	//! Le poste d'un soldat de garde est-il retenu ?
	bool SRP_HasPost()
	{
		return m_bSRP_PostSet;
	}

	//------------------------------------------------------------------------------------------------
	//! Poste d'un soldat de garde : là où il s'est posé (SRP_EnemySenses.UpdateGroup), repris seulement s'il s'est posé
	//! hors de son cercle
	void SRP_SetPost(vector position)
	{
		m_vSRP_Post = position;
		m_bSRP_PostSet = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un passage de plus : tranquille et immobile (« still ») ou non ; rend le nombre de passages de suite où il l'est
	int SRP_NoteStill(bool still)
	{
		if (still)
			m_iSRP_StillPasses++;
		else
			m_iSRP_StillPasses = 0;
		return m_iSRP_StillPasses;
	}

	//------------------------------------------------------------------------------------------------
	vector SRP_GetPost()
	{
		return m_vSRP_Post;
	}

	//------------------------------------------------------------------------------------------------
	bool SRP_IsWakeLogged()
	{
		return m_bSRP_WakeLogged;
	}

	//------------------------------------------------------------------------------------------------
	void SRP_SetWakeLogged()
	{
		m_bSRP_WakeLogged = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Vigilance d'une alerte voisine (1 = aucune), posée toutes les 2 s par SRP_EnemySenses.UpdateGroup
	void SRP_SetVigilance(float factor)
	{
		m_fSRP_Vigilance = factor;
	}

	//------------------------------------------------------------------------------------------------
	float SRP_GetVigilance()
	{
		return m_fSRP_Vigilance;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce soldat est un des nôtres : sa perception suit désormais la formule SimpleRP
	void SRP_Manage(int role, float roleFactor, vector holdOrigin, float holdRadius)
	{
		m_bSRP_Managed = true;
		m_iSRP_Role = role;
		m_fSRP_RoleFactor = roleFactor;
		m_vSRP_Hold = holdOrigin;
		m_fSRP_Hold = holdRadius;
	}

	//------------------------------------------------------------------------------------------------
	bool SRP_IsManaged()
	{
		return m_bSRP_Managed;
	}

	//------------------------------------------------------------------------------------------------
	int SRP_GetRole()
	{
		return m_iSRP_Role;
	}

	//------------------------------------------------------------------------------------------------
	float SRP_GetHoldRadius()
	{
		return m_fSRP_Hold;
	}

	//------------------------------------------------------------------------------------------------
	vector SRP_GetHoldOrigin()
	{
		return m_vSRP_Hold;
	}

	//------------------------------------------------------------------------------------------------
	float SRP_GetRoleFactor()
	{
		return m_fSRP_RoleFactor;
	}

	//------------------------------------------------------------------------------------------------
	//! Facteurs d'environnement (posés toutes les 2 s) ; une valeur n'est réécrite que si elle bouge de plus de 0,02
	void SRP_SetEnv(float calm, float combat)
	{
		if (Math.AbsFloat(calm - m_fSRP_EnvCalm) > 0.02)
			m_fSRP_EnvCalm = calm;
		if (Math.AbsFloat(combat - m_fSRP_EnvCombat) > 0.02)
			m_fSRP_EnvCombat = combat;
	}

	//------------------------------------------------------------------------------------------------
	float SRP_GetEnvCalm()
	{
		return m_fSRP_EnvCalm;
	}

	//------------------------------------------------------------------------------------------------
	float SRP_GetEnvCombat()
	{
		return m_fSRP_EnvCombat;
	}

	//------------------------------------------------------------------------------------------------
	//! Formule SimpleRP pour nos soldats ; formule CRX (super) pour toutes les autres IA et pour un état inconnu
	override void UpdatePerceptionFactor(PerceptionComponent perceptionComp, SCR_AIThreatSystem threatSystem)
	{
		if (!perceptionComp || !threatSystem)
			return;
		if (!m_bSRP_Managed)
		{
			super.UpdatePerceptionFactor(perceptionComp, threatSystem);
			return;
		}

		EAIThreatState threatState = threatSystem.GetThreatState();
		m_iSRP_LastState = threatState;
		float shots = threatSystem.GetThreatShotsFired();
		float suppression = threatSystem.GetThreatSuppression();
		float baseFactor = 0;
		bool known = true;
		bool calmState = false;
		switch (threatState)
		{
			case EAIThreatState.SAFE:
			{
				// Tranquille : plus attentif pendant une alerte voisine (livraison 2 ; 1 sinon)
				baseFactor = SRP_EnemySenses.s_fSafe * m_fSRP_Vigilance + shots;
				calmState = true;
				break;
			}
			case EAIThreatState.VIGILANT:
			{
				// Vigilant : pas de bonus d'alerte voisine (la vigilance d'alerte relève le calme VERS la vigilance :
				// 1.3 x 1.5 < 2.5 ; multiplier ici passerait au-dessus d'alerté et de menacé). Plancher : jamais moins
				// qu'un soldat tranquille sous bonus, quels que soient les réglages.
				baseFactor = Math.Max(SRP_EnemySenses.s_fVigilant, SRP_EnemySenses.s_fSafe * m_fSRP_Vigilance) + shots;
				calmState = true;
				break;
			}
			case EAIThreatState.ALERTED:
			{
				baseFactor = SRP_EnemySenses.s_fAlerted + shots - suppression * 0.5;
				break;
			}
			case EAIThreatState.THREATENED:
			{
				baseFactor = SRP_EnemySenses.s_fThreatened - suppression * 0.5;
				break;
			}
			default:
			{
				known = false;	// SUPPRESSED de CRX ou valeur inconnue : jamais un soldat aveugle
				break;
			}
		}
		if (!known)
		{
			m_fSRP_LastFactor = -1;	// facteur posé par CRX : inconnu de nous
			super.UpdatePerceptionFactor(perceptionComp, threatSystem);
			return;
		}

		// Visée dans une lunette grossissante : il voit mieux (logique CRX : x2 ; réglage s_fOpticsFactor, 1 = sans effet)
		if (SRP_EnemySenses.s_fOpticsFactor != 1 && m_CharacterController && m_WpnManager && m_CharacterController.IsWeaponADS())
		{
			SCR_2DOpticsComponent optics = SCR_2DOpticsComponent.Cast(m_WpnManager.GetCurrentSights());
			if (optics && optics.GetMagnification() > 0)
				baseFactor *= SRP_EnemySenses.s_fOpticsFactor;
		}

		// Un ennemi allongé voit moins bien (logique CRX)
		if (m_CharacterController && m_CharacterController.GetStance() == ECharacterStance.PRONE && baseFactor > SRP_EnemySenses.s_fProneObserverPenalty)
			baseFactor -= SRP_EnemySenses.s_fProneObserverPenalty;

		baseFactor *= m_fSRP_RoleFactor;

		// Environnement : avec buisson au calme, sans buisson au combat
		if (calmState)
			baseFactor *= m_fSRP_EnvCalm;
		else
			baseFactor *= m_fSRP_EnvCombat;

		if (baseFactor < 0)
			baseFactor = 0;
		baseFactor *= m_fEquipmentPerceptionFactor * m_fPerceptionFactor;
		m_fSRP_LastFactor = baseFactor;	// le facteur réellement posé (journal de repérage, diagnostic)
		perceptionComp.SetPerceptionFactor(baseFactor);
	}
}

//------------------------------------------------------------------------------------------------
//! Tirs entendus : le jeu (et CRX, qui garde cet appel) signale au groupe chaque tir audible ou chaque balle passée près
//! de son chef. On note, pour nos groupes, l'heure du dernier tir de chaque joueur : un joueur identifié une fois n'est
//! plus rafraîchi par ses tirs dans la perception du groupe (AddOrUpdateGunshot du jeu), il doit rester « trahi ».
//! Le Commandeur (#76, CO6) compte aussi ces tirs de joueurs (SRP_EnemySenses.NoteShotHeard -> SRP_Commander.NoteShot).
[BaseContainerProps()]
modded class SCR_AIDangerReaction_WeaponFired : SCR_AIDangerReaction
{
	//------------------------------------------------------------------------------------------------
	override void NotifyGroup(AIGroup group, IEntity shooterEntity, IEntity instigatorEntity, Faction faction, vector posWorld, bool endangering)
	{
		super.NotifyGroup(group, shooterEntity, instigatorEntity, faction, posWorld, endangering);
		IEntity shooter = instigatorEntity;
		if (!shooter)
			shooter = shooterEntity;
		SRP_EnemySenses.NoteShotHeard(group, shooter);
	}
}

//------------------------------------------------------------------------------------------------
//! B. Tir de nos soldats : l'écart de chaque tir est multiplié par le réglage de précision (0,7 = 30 pour cent plus
//! précis, demandé par Jack le 25/09), puis grandit avec la suppression (tir sous le feu)
modded class SCR_AIGetAimErrorOffset
{
	//------------------------------------------------------------------------------------------------
	override float GetRandomFactor(EAISkill skill, float mu)
	{
		float drawn = super.GetRandomFactor(skill, mu);
		if (!m_ChimeraAIAgent)
			return drawn;
		SCR_AIUtilityComponent utility = m_ChimeraAIAgent.m_UtilityComponent;
		if (!utility || !utility.m_CombatComponent || !utility.m_CombatComponent.SRP_IsManaged())
			return drawn;

		// Le jeu passe mu = 0 : c'est l'écart de chaque tir qui est multiplié
		float spread = Math.Max(SRP_EnemySenses.s_fAimSpread, 0);
		if (SRP_EnemySenses.s_fSuppressionAim > 0 && m_ThreatSystem)
		{
			float suppression = Math.Clamp(m_ThreatSystem.GetThreatSuppression(), 0, 1);
			spread = spread * (1 + SRP_EnemySenses.s_fSuppressionAim * suppression);
		}
		return mu + (drawn - mu) * spread;
	}
}

//------------------------------------------------------------------------------------------------
//! C. Ce que l'ennemi perçoit des joueurs, soldat par soldat (statique, serveur seulement)
class SRP_EnemySenses
{
	// Réglages (copiés par SRP_EnemyComponent.OnPostInit ; valeurs par défaut ci-dessous)
	static float s_fSafe = 1.3;					// perception de base par état
	static float s_fVigilant = 2.5;
	static float s_fAlerted = 2.5;
	static float s_fThreatened = 3.0;
	static float s_fSentry = 1.4;				// facteurs de rôle
	static float s_fPatrol = 0.9;
	static float s_fAssault = 1.1;
	static float s_fCrew = 0.8;
	static float s_fCrouchTarget = 0.7;			// posture de la cible
	static float s_fProneTarget = 0.4;
	static float s_fProneObserverPenalty = 0.3;	// ennemi allongé
	static float s_fRange = 750;				// joueurs pris en compte autour d'un soldat
	static float s_fConcealRange = 250;			// si des joueurs sont plus près, seuls ceux-là comptent
	static float s_fLightFullLV = -3.5;			// plein jour au-dessus (seuil du jeu)
	static float s_fLightDarkLV = -7.0;			// nuit noire en dessous
	static float s_fNightMin = 0.3;				// facteur de nuit noire
	static float s_fLitIllum = 0.5;				// éclairé à partir de ce facteur d'éclairage du jeu
	static bool s_bFlashlightReveals = true;
	static int s_iShotRevealSeconds = 5;		// un tir entendu éclaire le tireur pour ce groupe
	static float s_fFlareRadius = 350;
	static int s_iFlareMs = 60000;
	static float s_fCloseDistance = 40;			// de nuit, de si près, on se voit quand même
	static float s_fCloseFactor = 0.8;
	static bool s_bReveal = true;				// révélation de près, de nuit
	static int s_iRevealMs = 4000;
	static int s_iRevealCooldownMs = 30000;
	static bool s_bVegetation = true;			// buissons (expérimental)
	static float s_fBushFactor = 0.5;
	static float s_fBushMinHeight = 0.8;
	static float s_fBushMaxHeight = 4;
	static float s_fBushReach = 0.5;
	static float s_fBushStillSpeed = 0.5;
	static float s_fSuppressionAim = 1.0;		// tir sous le feu (0 = comme CRX)
	static float s_fAimDefault = 1.0;			// visée de base d'un soldat que CRX n'a pas classé
	static float s_fAimSpread = 0.7;			// précision de tir : écart de chaque tir × … (1 = comme CRX)
	static int s_iSightingLogMs = 60000;		// une ligne de repérage par groupe et par joueur au plus
	static float s_fVigilantFactor = 1.5;		// alerte voisine : perception au calme multipliée (livraison 2)
	static float s_fOpticsFactor = 2;			// visée dans une lunette grossissante : perception multipliée (CRX : 2)
	static float s_fLoiterWakeDistance = 60;	// un soldat assis ou adossé qui voit un joueur à moins de … m se relève (0 = jamais)
	static bool s_bPerceptionDebug;				// ligne de diagnostic par soldat, hors mode essai aussi
	static bool s_bProfileLogged;				// profil CRX écrit au journal (une fois par partie)

	// Ligne de diagnostic par soldat (mode essai du Workbench, ou s_bPerceptionDebug) : toutes les DIAG_MS, pour chaque
	// soldat à moins de DIAG_RANGE d'un joueur vivant
	protected static const int DIAG_MS = 10000;
	protected static const float DIAG_RANGE = 150;
	protected static const int DIAG_MAX_LINES = 10;		// hors mode essai (réglage de débogage sur le serveur) : lignes par passage
	protected static int s_iNextDiagTick;
	protected static bool s_bDiagNow;					// ce passage écrit les lignes de diagnostic
	protected static int s_iDiagLines;					// lignes de diagnostic écrites dans ce passage
	protected static int s_iDiagSkipped;				// soldats proches non écrits (plafond atteint)

	// Filet contre les gardes assis : le joueur doit être devant lui (… degrés de part et d'autre de son cap), ou déjà
	// dans sa propre perception
	protected static const float LOITER_WAKE_ANGLE = 100;

	// Poste d'un soldat de garde : pris là où il s'est posé (tranquille, plus lent que POST_STILL_SPEED m/s pendant
	// POST_STILL_PASSES passages de suite), pas là où il est apparu
	protected static const float POST_STILL_SPEED = 0.3;
	protected static const int POST_STILL_PASSES = 2;

	// État d'un passage : un indice par joueur vivant (tableaux parallèles, vidés à chaque BeginTick)
	protected static ref array<IEntity> s_aPlayers = {};
	protected static ref array<int> s_aPlayerIds = {};
	protected static ref array<bool> s_aLit = {};			// éclairé : vu comme de jour
	protected static ref array<bool> s_aHidden = {};		// dans un buisson (accroupi, allongé ou immobile)
	protected static ref array<float> s_aStanceFactor = {};
	protected static ref array<float> s_aIllum = {};		// facteur d'éclairage du jeu (journal)
	protected static ref array<bool> s_aTorch = {};			// lampe allumée (journal)
	protected static ref array<int> s_aStance = {};			// posture (journal)
	static float s_fAmbientLV;								// lumière ambiante totale (LV)
	static float s_fNightGlobal = 1;						// 1 = plein jour ; s_fNightMin = nuit noire

	// Fusées éclairantes allumées : position, fin (tick)
	protected static ref array<vector> s_aFlarePos = {};
	protected static ref array<int> s_aFlareEnd = {};

	// Anneau du journal de repérage (40 lignes au plus)
	protected static ref array<string> s_aSightings = {};
	protected static const int SIGHTINGS_MAX = 40;

	// Recherche d'un buisson (rappel statique de QueryEntitiesBySphere, sur le modèle de SRP_HeliSearch)
	protected static vector s_vBushFrom;
	protected static bool s_bBushFound;

	//------------------------------------------------------------------------------------------------
	//! Nouvelle partie (SRP_EnemyComponent.OnPostInit, avant les passages) et fin de partie (OnDelete) : rien ne doit
	//! survivre d'une partie Workbench ou d'un redémarrage de scénario à la suivante (profil CRX écrit une fois par partie,
	//! journal de repérage, fusées, diagnostic). Les réglages ne sont pas touchés : OnPostInit les recopie.
	static void ResetStatics()
	{
		s_bProfileLogged = false;
		s_aSightings.Clear();
		s_aFlarePos.Clear();
		s_aFlareEnd.Clear();
		s_aPlayers.Clear();
		s_aPlayerIds.Clear();
		s_aLit.Clear();
		s_aHidden.Clear();
		s_aStanceFactor.Clear();
		s_aIllum.Clear();
		s_aTorch.Clear();
		s_aStance.Clear();
		s_fAmbientLV = 0;
		s_fNightGlobal = 1;
		s_iNextDiagTick = 0;
		s_bDiagNow = false;
		s_iDiagLines = 0;
		s_iDiagSkipped = 0;
	}

	//------------------------------------------------------------------------------------------------
	// Passage : ce que l'ennemi peut percevoir de chaque joueur
	//------------------------------------------------------------------------------------------------
	//! Début d'un passage : vraie lumière ambiante (facteur de nuit), fusées éteintes retirées, puis pour chaque joueur
	//! vivant : éclairé (lampe, éclairage, fusée), posture, caché dans un buisson. Décide aussi si ce passage écrit les
	//! lignes de diagnostic (une fois toutes les DIAG_MS, en mode essai ou avec le réglage de débogage).
	static void BeginTick(array<IEntity> players, int now)
	{
		s_bDiagNow = false;
		s_iDiagLines = 0;
		s_iDiagSkipped = 0;
		if ((s_bPerceptionDebug || SRP_EnemyComponent.IsTestMode()) && now >= s_iNextDiagTick)
		{
			s_bDiagNow = true;
			s_iNextDiagTick = now + DIAG_MS;
		}

		s_aPlayers.Clear();
		s_aPlayerIds.Clear();
		s_aLit.Clear();
		s_aHidden.Clear();
		s_aStanceFactor.Clear();
		s_aIllum.Clear();
		s_aTorch.Clear();
		s_aStance.Clear();

		// Lumière ambiante réelle (lune, crépuscule, ciel couvert) : 1 au-dessus du seuil de jour, s_fNightMin en dessous du
		// seuil de nuit noire, progression linéaire entre les deux
		s_fNightGlobal = 1;
		PerceptionManager perception = GetGame().GetPerceptionManager();
		if (perception)
		{
			float directLV;
			float ambientLV;
			float totalLV;
			perception.GetAmbientLV(directLV, ambientLV, totalLV);
			s_fAmbientLV = totalLV;
			if (totalLV >= s_fLightFullLV)
				s_fNightGlobal = 1;
			else if (totalLV <= s_fLightDarkLV)
				s_fNightGlobal = s_fNightMin;
			else
				s_fNightGlobal = s_fNightMin + (1 - s_fNightMin) * (totalLV - s_fLightDarkLV) / (s_fLightFullLV - s_fLightDarkLV);
		}

		// Fusées éteintes
		for (int f = s_aFlareEnd.Count() - 1; f >= 0; f--)
		{
			if (now < s_aFlareEnd[f])
				continue;
			s_aFlarePos.Remove(f);
			s_aFlareEnd.Remove(f);
		}

		if (!players)
			return;
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;

			int stance = StanceOf(player);
			float stanceFactor = 1;
			if (stance == ECharacterStance.CROUCH)
				stanceFactor = s_fCrouchTarget;
			else if (stance == ECharacterStance.PRONE)
				stanceFactor = s_fProneTarget;

			// Éclairé : lampe allumée, éclairage du jeu (lampadaire, projecteur, phares), ou sous une fusée
			bool torch = HasFlashlightOn(player);
			float illumination = 0;
			PerceivableComponent perceivable = PerceivableComponent.Cast(player.FindComponent(PerceivableComponent));
			if (perceivable)
				illumination = perceivable.GetIlluminationFactor();
			bool lit = (s_bFlashlightReveals && torch) || illumination >= s_fLitIllum || InFlare(player.GetOrigin());

			// Caché : accroupi, allongé ou immobile, dans un buisson (jamais en véhicule)
			bool hidden = false;
			if (s_bVegetation && !IsInVehicle(player))
			{
				if (stance != ECharacterStance.STAND || SpeedOf(player) < s_fBushStillSpeed)
					hidden = IsInBush(player);
			}

			s_aPlayers.Insert(player);
			s_aPlayerIds.Insert(SRP_Utils.GetPlayerIdFromEntity(player));
			s_aLit.Insert(lit);
			s_aHidden.Insert(hidden);
			s_aStanceFactor.Insert(stanceFactor);
			s_aIllum.Insert(illumination);
			s_aTorch.Insert(torch);
			s_aStance.Insert(stance);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Fin d'un passage (après tous les groupes) : si le plafond de lignes de diagnostic a été atteint (réglage de
	//! débogage hors mode essai), une ligne dit combien de soldats proches n'ont pas été écrits
	static void EndTick()
	{
		if (!s_bDiagNow || s_iDiagSkipped <= 0)
			return;
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("Diagnostic : %1 autre(s) soldat(s) à moins de %2 m d'un joueur non écrit(s) (plafond de %3 lignes par passage hors mode essai)", s_iDiagSkipped, Math.Round(DIAG_RANGE), DIAG_MAX_LINES));
	}

	//------------------------------------------------------------------------------------------------
	//! Une fusée éclairante vient d'être allumée : elle éclaire les joueurs dessous pendant s_iFlareMs
	static void AddFlare(vector pos, int now)
	{
		s_aFlarePos.Insert(pos);
		s_aFlareEnd.Insert(now + s_iFlareMs);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce point est-il sous une fusée allumée ? (distance à plat : la fusée brûle haut dans le ciel)
	protected static bool InFlare(vector position)
	{
		foreach (vector flare : s_aFlarePos)
		{
			if (vector.DistanceXZ(flare, position) <= s_fFlareRadius)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Une lampe (gadget) allumée ? L'état est remonté au serveur quand le joueur l'allume
	protected static bool HasFlashlightOn(IEntity player)
	{
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(player);
		if (!gadgets)
			return false;
		array<SCR_GadgetComponent> lights = gadgets.GetGadgetsByType(EGadgetType.FLASHLIGHT);
		if (!lights)
			return false;
		foreach (SCR_GadgetComponent light : lights)
		{
			if (light && light.IsToggledOn())
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Posture d'un personnage (debout si inconnue)
	static int StanceOf(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return ECharacterStance.STAND;
		CharacterControllerComponent controller = chimera.GetCharacterController();
		if (!controller)
			return ECharacterStance.STAND;
		return controller.GetStance();
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsInVehicle(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		return chimera && chimera.IsInVehicle();
	}

	//------------------------------------------------------------------------------------------------
	//! Soldat assis dans un vrai véhicule (jeep, camion, hélicoptère, tourelle montée sur un véhicule) : le servant d'une
	//! tourelle fixe (mitrailleuse sur trépied d'un poste) n'est pas un équipage, il reste une sentinelle
	protected static bool IsVehicleCrew(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera || !chimera.IsInVehicle())
			return false;
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(chimera.GetCompartmentAccessComponent());
		if (!access)
			return true;
		return Vehicle.Cast(access.GetVehicle()) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Vitesse d'un personnage, en mètres par seconde : celle de son contrôleur (tenue à jour sur le serveur pour un joueur
	//! distant, comme le lit le jeu pour les barbelés), sinon celle de sa physique
	protected static float SpeedOf(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (chimera)
		{
			CharacterControllerComponent controller = chimera.GetCharacterController();
			if (controller)
				return controller.GetVelocity().Length();
		}
		Physics physics = character.GetPhysics();
		if (!physics)
			return 0;
		return physics.GetVelocity().Length();
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur est-il dans un buisson ? Une plante du jeu (BaseTree) à moins de 6 m : buisson pour le jeu, ou de la
	//! bonne hauteur ; pas morte ; et le joueur dans son feuillage (demi-largeur + marge, à plat)
	protected static bool IsInBush(IEntity player)
	{
		s_vBushFrom = player.GetOrigin();
		s_bBushFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vBushFrom, 6, BushCandidate, null, EQueryEntitiesFlags.ALL);
		return s_bBushFound;
	}

	//------------------------------------------------------------------------------------------------
	//! Rappel de la recherche de buisson : true = continuer, false = trouvé
	protected static bool BushCandidate(IEntity entity)
	{
		if (!entity || !BaseTree.Cast(entity))
			return true;

		int soundType = ETreeSoundTypes.None;
		TreeClass treeData = TreeClass.Cast(entity.GetPrefabData());
		if (treeData)
			soundType = treeData.SoundType;
		if (soundType == ETreeSoundTypes.Withered)
			return true;	// une plante morte ne cache rien

		vector mins;
		vector maxs;
		entity.GetBounds(mins, maxs);
		float scale = entity.GetScale();
		float height = (maxs[1] - mins[1]) * scale;
		bool bushType = soundType == ETreeSoundTypes.Bush || soundType == ETreeSoundTypes.Bush_Leafy || soundType == ETreeSoundTypes.Bush_Reed || soundType == ETreeSoundTypes.Bush_Small;
		if (!bushType && (height < s_fBushMinHeight || height > s_fBushMaxHeight))
			return true;	// ni une touffe d'herbe, ni un arbre

		float reach = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5 * scale + s_fBushReach;
		if (vector.DistanceXZ(entity.GetOrigin(), s_vBushFrom) >= reach)
			return true;
		s_bBushFound = true;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce soldat voit-il ce joueur ? Des yeux (selon sa posture) à la poitrine du joueur (selon la sienne), arrêté par le
	//! relief, les murs, les toits, les troncs ; les deux personnages sont exclus de la trace
	static bool CanSee(IEntity observer, IEntity player)
	{
		if (!observer || !player)
			return false;
		return CanSeeFrom(observer, EyeHeight(StanceOf(observer)), player);
	}

	//------------------------------------------------------------------------------------------------
	//! Comme CanSee, les yeux de l'observateur à « eyeHeight » au-dessus de ses pieds (soldat assis : sa posture reste
	//! « debout » pendant la flânerie)
	protected static bool CanSeeFrom(notnull IEntity observer, float eyeHeight, notnull IEntity player)
	{
		TraceParam trace = new TraceParam();
		trace.Start = observer.GetOrigin() + Vector(0, eyeHeight, 0);
		trace.End = player.GetOrigin() + Vector(0, ChestHeight(StanceOf(player)), 0);
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		array<IEntity> excluded = {};
		excluded.Insert(observer);
		excluded.Insert(player);
		trace.ExcludeArray = excluded;
		float fraction = GetGame().GetWorld().TraceMove(trace, null);
		return fraction >= 0.99 || trace.TraceEnt == player;
	}

	//------------------------------------------------------------------------------------------------
	protected static float EyeHeight(int stance)
	{
		if (stance == ECharacterStance.CROUCH)
			return 1.0;
		if (stance == ECharacterStance.PRONE)
			return 0.3;
		return 1.6;
	}

	//------------------------------------------------------------------------------------------------
	protected static float ChestHeight(int stance)
	{
		if (stance == ECharacterStance.CROUCH)
			return 0.8;
		if (stance == ECharacterStance.PRONE)
			return 0.25;
		return 1.3;
	}

	//------------------------------------------------------------------------------------------------
	//! Signale un joueur à un groupe comme un tir entendu à cette position (seul utilisateur : la révélation de près,
	//! de nuit). Rien si le groupe ou le joueur manque, ou sans heure de perception. Le jeu ne met plus à jour par un tir
	//! une cible que le groupe a déjà identifiée (AddOrUpdateGunshot) : une telle cible, devenue ancienne (la révélation
	//! n'a lieu que si le groupe ne le voit pas en ce moment), est rafraîchie ici ET ramenée à « détectée », comme pour
	//! un joueur jamais vu : la révélation reste un bruit (« entendus », ni sirène ni hélicoptère), quel que soit
	//! l'historique du groupe. Un soldat qui l'identifie ensuite le fait repasser « identifié » (jeu de base).
	static void Inform(SCR_AIGroup group, IEntity player, vector pos, float timestamp)
	{
		if (!group || group.IsDeleted() || !player || player.IsDeleted() || timestamp < 0)
			return;
		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!utility || !utility.m_Perception)
			return;
		foreach (SCR_AITargetInfo known : utility.m_Perception.m_aTargets)
		{
			if (!known || known.m_Entity != player || known.m_eCategory != EAITargetInfoCategory.IDENTIFIED)
				continue;
			if (timestamp > known.m_fTimestamp)
			{
				known.m_eCategory = EAITargetInfoCategory.DETECTED;
				known.UpdateFromGunshot(pos, timestamp, false);
			}
			return;
		}
		Faction faction = null;
		PerceivableComponent perceivable = PerceivableComponent.Cast(player.FindComponent(PerceivableComponent));
		if (perceivable)
			faction = perceivable.GetPerceivedFaction();
		utility.m_Perception.AddOrUpdateGunshot(player, pos, faction, timestamp, false);
	}

	//------------------------------------------------------------------------------------------------
	// Un groupe : soldats réglés, environnement de chaque soldat, révélation de près la nuit
	//------------------------------------------------------------------------------------------------
	//! Pour un de nos groupes (sauf l'hélicoptère) : joueurs trahis par leurs tirs pour ce groupe ; chaque soldat arrivé
	//! réglé d'après le rôle et le maintien voulus (retardataires de la file de la 1.8, rôle changé) ; visée de base d'un
	//! soldat que CRX n'a pas classé ; facteurs d'environnement de chaque soldat ; révélation de près, de nuit
	static void UpdateGroup(SRP_EnemyComponent enemies, SRP_EnemyGroup record, int now, float timeNow)
	{
		if (!enemies || !record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		if (record.m_sOwner == "helico")
			return;	// l'équipage de l'hélicoptère reste comme aujourd'hui

		int playerCount = s_aPlayers.Count();
		array<bool> betrayed = {};
		MarkBetrayed(record, timeNow, betrayed);

		int role = record.m_iRole;
		if (role < 0)
			role = SRP_ECRXRole.PATROUILLE;
		bool revealWanted = s_bReveal && s_fNightGlobal < 1 && playerCount > 0;

		// Vigilance d'une alerte voisine (SRP_EnemyAwareness.SpreadVigilance), jusqu'à son échéance
		float vigilance = 1;
		if (record.m_iVigilantUntilTick > now && s_fVigilantFactor > 0)
			vigilance = s_fVigilantFactor;
		array<bool> closeSeen = {};
		for (int c = 0; c < playerCount; c++)
			closeSeen.Insert(false);

		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
			if (!chimera || !chimera.m_UtilityComponent)
				continue;
			SCR_AICombatComponent combat = chimera.m_UtilityComponent.m_CombatComponent;
			SCR_AIInfoComponent info = chimera.m_UtilityComponent.m_AIInfo;
			IEntity soldier = agent.GetControlledEntity();
			if (!combat || !info || !soldier || soldier.IsDeleted() || SRP_Utils.IsDead(soldier))
				continue;

			// Profil CRX, une seule fois, sur le premier soldat, avant nos réglages
			if (!s_bProfileLogged)
			{
				s_bProfileLogged = true;
				LogProfile(info);
			}

			// Maintien voulu pour ce soldat. Un soldat de garde tient un court rayon (SRP_CRX.s_fGarrisonPostHold, jamais plus
			// que la zone du groupe) autour de SON poste ; le rayon du secteur ne sert plus qu'au point Defend du groupe. Le
			// poste est pris là où il s'est POSÉ (tranquille et immobile deux passages de suite), pas là où il est apparu :
			// le point Defend l'envoie ensuite sur un poste d'observation, une porte, une tourelle ou un couvert, parfois à
			// 200 m de son point d'apparition. Le poste est repris seulement s'il s'est posé HORS de son cercle (un nouveau
			// poste donné par le point Defend, le retour d'une aide) : ses enquêtes et ses déplacements de combat restent
			// dans le cercle, le poste ne dérive donc pas au fil des alertes. Tant qu'il ne s'est jamais posé : le maintien
			// du groupe. Les autres rôles : le maintien du groupe (aucun pour une ronde ou un assaut : l'aide, le ratissage
			// et le retour au poste le relâchent, l'arrivée au poste le rétablit).
			float holdRadius = record.m_fHold;
			vector holdOrigin = record.m_vHold;
			bool postMoved = false;
			bool calm = false;
			SCR_AIThreatSystem threat = chimera.m_UtilityComponent.m_ThreatSystem;
			if (threat && threat.GetThreatState() == EAIThreatState.SAFE)
				calm = true;
			int stillPasses = combat.SRP_NoteStill(calm && SpeedOf(soldier) < POST_STILL_SPEED);
			if (role == SRP_ECRXRole.GARNISON && record.m_fHold > 0 && SRP_CRX.s_fGarrisonPostHold > 0)
			{
				float postRadius = Math.Min(record.m_fHold, SRP_CRX.s_fGarrisonPostHold);
				vector here = soldier.GetOrigin();
				if (stillPasses >= POST_STILL_PASSES && (!combat.SRP_HasPost() || vector.DistanceXZ(here, combat.SRP_GetPost()) > postRadius))
				{
					combat.SRP_SetPost(here);
					postMoved = true;
				}
				if (combat.SRP_HasPost())
				{
					holdRadius = postRadius;
					holdOrigin = combat.SRP_GetPost();
				}
			}

			// Rattrapage : soldat arrivé après la pose, rôle ou rayon changé, poste (re)pris, maintien perdu (réinitialisation
			// CRX). L'origine n'est pas comparée : CRX la déplace lui-même (fin d'un déplacement en formation), la réimposer à
			// chaque passage ramènerait sans cesse le soldat à son premier point
			bool mismatch = postMoved || !combat.SRP_IsManaged() || combat.SRP_GetRole() != role || combat.SRP_GetHoldRadius() != holdRadius;
			if (!mismatch && holdRadius > 0 && !info.GetHoldPosition())
				mismatch = true;
			if (mismatch && enemies.UsesCRX())
				SRP_CRX.ApplySoldier(agent, role, holdOrigin, holdRadius);
			combat.SRP_Manage(role, RoleFactor(soldier, role), holdOrigin, holdRadius);
			combat.SRP_SetVigilance(vigilance);

			// Filet : un soldat assis ou adossé (action intelligente d'un point Defend, animation) qui voit un joueur tout
			// près se relève pour pouvoir réagir
			WakeIfLoitering(record, role, combat, info, soldier);

			// Visée de base : CRX ne classe les soldats qu'à l'expansion complète d'un groupe à emplacements. Jusque-là (et
			// pour toujours si le groupe n'est jamais complet : officier isolé, jeep dont on a retiré un soldat, demandes
			// abandonnées par la file) l'erreur de visée reste nulle, c'est-à-dire un tir parfait. Si CRX classe le groupe
			// plus tard, il remplace cette valeur par celle du rang.
			if (info.GetAimAccuracyErrorOriginal() < 0.01 && info.GetCombatComponent())
			{
				info.SetAimAccuracyErrorOriginal(s_fAimDefault);
				info.SetAimAccuracyError(Math.Clamp(s_fAimDefault + info.GetAimAccuracyErrorModifier(), 0, 3));
			}

			SoldierEnvironment(combat, soldier, betrayed, revealWanted, closeSeen);

			// Diagnostic (mode essai ou réglage de débogage), une fois toutes les DIAG_MS
			if (s_bDiagNow)
				DiagSoldier(record, role, agent, chimera, combat, info, soldier);
		}

		if (revealWanted)
			RevealAtNight(record, role, closeSeen, now, timeNow);
		else
			record.m_mRevealStart.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Un tir d'un joueur vient d'être signalé à ce groupe (SCR_AIDangerReaction_WeaponFired.NotifyGroup : tir audible ou
	//! balle passée près du chef) : l'heure de perception est retenue sur notre enregistrement du groupe, et le tir est
	//! compté pour le compte rendu du groupe au Commandeur (CO6 ; seuls les tirs de joueurs arrivent jusque-là : le jeu
	//! ne signale que les tirs d'une faction ennemie, et les tirs d'IA sont écartés ci-dessous)
	static void NoteShotHeard(AIGroup group, IEntity shooter)
	{
		if (!group || !shooter)
			return;
		// Le tireur d'abord : les tirs entre IA (les plus nombreux en plein accrochage) ne parcourent pas nos groupes
		int playerId = SRP_Utils.GetPlayerIdFromEntity(shooter);
		if (playerId <= 0)
			return;	// une IA : pas notre affaire
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		SRP_EnemyGroup record = enemies.FindRecord(SCR_AIGroup.Cast(group));
		if (!record)
			return;
		PerceptionManager perception = GetGame().GetPerceptionManager();
		if (!perception)
			return;
		record.m_mShotHeard.Set(playerId, perception.GetTime());
		// Commandeur (#76, CO6) : tir compté dans le tampon du groupe (500 au plus) ; rien sans Commandeur
		SRP_Commander.NoteShot(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Les joueurs « trahis » pour ce groupe : ses tirs entendus par le groupe depuis moins de s_iShotRevealSeconds
	//! (même si le groupe l'avait déjà identifié : le jeu ne rafraîchit plus alors sa cible de groupe), ou une cible joueur
	//! détectée ou identifiée depuis moins de s_iShotRevealSeconds ; ils sont vus comme de jour par ce groupe seulement
	protected static void MarkBetrayed(SRP_EnemyGroup record, float timeNow, array<bool> betrayed)
	{
		int playerCount = s_aPlayers.Count();
		for (int b = 0; b < playerCount; b++)
			betrayed.Insert(false);
		if (timeNow < 0 || s_iShotRevealSeconds <= 0 || playerCount == 0)
			return;
		for (int s = 0; s < playerCount; s++)
		{
			float shotTime;
			if (record.m_mShotHeard.Find(s_aPlayerIds[s], shotTime) && timeNow - shotTime <= s_iShotRevealSeconds)
				betrayed[s] = true;
		}
		SCR_AIGroupUtilityComponent groupUtility = SCR_AIGroupUtilityComponent.Cast(record.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!groupUtility || !groupUtility.m_Perception)
			return;
		foreach (SCR_AITargetInfo target : groupUtility.m_Perception.m_aTargets)
		{
			if (!target || !target.m_Entity)
				continue;
			if (target.m_eCategory != EAITargetInfoCategory.IDENTIFIED && target.m_eCategory != EAITargetInfoCategory.DETECTED)
				continue;
			if (timeNow - target.m_fTimestamp > s_iShotRevealSeconds)
				continue;
			int index = s_aPlayers.Find(target.m_Entity);
			if (index >= 0)
				betrayed[index] = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Facteur de rôle d'un soldat : assis dans un véhicule = équipage (pas le servant d'une tourelle fixe) ; sinon
	//! sentinelle, patrouille ou assaut
	protected static float RoleFactor(IEntity soldier, int role)
	{
		if (IsVehicleCrew(soldier))
			return s_fCrew;
		if (role == SRP_ECRXRole.GARNISON)
			return s_fSentry;
		if (role == SRP_ECRXRole.PATROUILLE)
			return s_fPatrol;
		if (role == SRP_ECRXRole.ASSAUT)
			return s_fAssault;
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Facteurs d'environnement d'un soldat : pour chaque joueur retenu (à portée ; seulement les plus proches s'il y en
	//! a), nuit (1 de jour, éclairé ou trahi ; sinon la nuit globale, relevée de près) × posture, buisson compris au
	//! calme. Le meilleur joueur donne le facteur ; aucun joueur : 1. Note aussi les joueurs vus de près la nuit.
	protected static void SoldierEnvironment(SCR_AICombatComponent combat, IEntity soldier, array<bool> betrayed, bool revealWanted, array<bool> closeSeen)
	{
		int playerCount = s_aPlayers.Count();
		vector eye = soldier.GetOrigin();
		bool anyConceal = false;
		for (int p = 0; p < playerCount; p++)
		{
			if (vector.Distance(eye, s_aPlayers[p].GetOrigin()) <= s_fConcealRange)
			{
				anyConceal = true;
				break;
			}
		}

		float envCalm = 0;
		float envCombat = 0;
		bool anyInRange = false;
		for (int q = 0; q < playerCount; q++)
		{
			IEntity watched = s_aPlayers[q];
			float distance = vector.Distance(eye, watched.GetOrigin());
			if (distance > s_fRange)
				continue;
			if (anyConceal && distance > s_fConcealRange)
				continue;
			anyInRange = true;

			float night = 1;
			if (s_fNightGlobal < 1 && !s_aLit[q] && !betrayed[q])
			{
				night = s_fNightGlobal;
				if (distance < s_fCloseDistance && s_fCloseFactor > night)
					night = s_fCloseFactor;
			}
			float seen = night * s_aStanceFactor[q];
			float seenCalm = seen;
			if (s_aHidden[q])
				seenCalm = seen * s_fBushFactor;
			if (seenCalm > envCalm)
				envCalm = seenCalm;
			if (seen > envCombat)
				envCombat = seen;

			// Révélation de près, de nuit : un joueur non éclairé, tout près, vu (une trace par joueur et par passage)
			if (revealWanted && !s_aLit[q] && !closeSeen[q] && distance < s_fCloseDistance && CanSee(soldier, watched))
				closeSeen[q] = true;
		}
		if (!anyInRange)
		{
			envCalm = 1;
			envCombat = 1;
		}
		combat.SRP_SetEnv(envCalm, envCombat);
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôleur de personnage (script) d'un soldat, ou null
	protected static SCR_CharacterControllerComponent ControllerOf(IEntity character)
	{
		ChimeraCharacter chimera = ChimeraCharacter.Cast(character);
		if (!chimera)
			return null;
		return SCR_CharacterControllerComponent.Cast(chimera.GetCharacterController());
	}

	//------------------------------------------------------------------------------------------------
	//! Un soldat qui flâne est-il assis (et non adossé ou debout) ? Type de flânerie lu sur le gestionnaire de commandes
	protected static bool IsSeated(SCR_CharacterControllerComponent controller)
	{
		CharacterAnimationComponent animation = controller.GetAnimationComponent();
		if (!animation)
			return false;
		SCR_CharacterCommandHandlerComponent handler = SCR_CharacterCommandHandlerComponent.Cast(animation.GetCommandHandler());
		if (!handler)
			return false;
		ELoiteringType loiterType;
		if (!handler.IsLoitering(loiterType))
			return false;
		return loiterType == ELoiteringType.SIT;
	}

	//------------------------------------------------------------------------------------------------
	//! Écart, en degrés et à plat, entre le cap du corps d'un soldat et la direction d'un joueur (-1 : indéfini, même
	//! position)
	protected static float HeadingGap(IEntity soldier, IEntity player)
	{
		vector facing = soldier.GetTransformAxis(2);
		facing[1] = 0;
		vector toPlayer = player.GetOrigin() - soldier.GetOrigin();
		toPlayer[1] = 0;
		if (facing.Length() <= 0.01 || toPlayer.Length() <= 0.01)
			return -1;
		float cosine = Math.Clamp(vector.Dot(facing.Normalized(), toPlayer.Normalized()), -1, 1);
		return Math.Acos(cosine) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	//! Filet contre les gardes assis ou adossés : un soldat qui flâne (SCR_CharacterControllerComponent.IsLoitering) et
	//! voit un joueur vivant à moins de s_fLoiterWakeDistance se relève tout de suite (StopLoitering rapide, comme le jeu
	//! le fait en passant en alerte). Il faut qu'il puisse s'en apercevoir : le joueur devant lui (LOITER_WAKE_ANGLE de
	//! part et d'autre de son cap) ou déjà dans sa propre perception (un bruit, une silhouette) ; sinon un garde qui se
	//! lève trahirait le joueur qui arrive dans son dos. Assis, ses yeux sont plus bas (la posture reste « debout »
	//! pendant la flânerie). Une ligne au journal par soldat, en mode essai seulement.
	protected static void WakeIfLoitering(SRP_EnemyGroup record, int role, SCR_AICombatComponent combat, SCR_AIInfoComponent info, IEntity soldier)
	{
		if (s_fLoiterWakeDistance <= 0 || s_aPlayers.IsEmpty())
			return;
		SCR_CharacterControllerComponent controller = ControllerOf(soldier);
		if (!controller || !controller.IsLoitering())
			return;
		float eyeHeight = EyeHeight(ECharacterStance.STAND);
		if (IsSeated(controller))
			eyeHeight = EyeHeight(ECharacterStance.CROUCH);
		PerceptionComponent perception = info.m_Perception;
		if (!perception)
			perception = PerceptionComponent.Cast(soldier.FindComponent(PerceptionComponent));
		vector from = soldier.GetOrigin();
		int count = s_aPlayers.Count();
		for (int w = 0; w < count; w++)
		{
			IEntity watched = s_aPlayers[w];
			if (!watched)
				continue;
			float distance = vector.Distance(from, watched.GetOrigin());
			if (distance > s_fLoiterWakeDistance)
				continue;
			float gap = HeadingGap(soldier, watched);
			bool inFront = gap <= LOITER_WAKE_ANGLE;	// -1 : collé à lui, compte comme devant
			if (!inFront && !(perception && perception.FindTargetPerceptionObject(watched) != null))
				continue;
			if (!CanSeeFrom(soldier, eyeHeight, watched))
				continue;
			controller.StopLoitering(true);
			if (SRP_EnemyComponent.IsTestMode() && !combat.SRP_IsWakeLogged())
			{
				combat.SRP_SetWakeLogged();
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Réveil : un soldat (%1, %2) assis ou adossé voit %3 à %4 m, il se relève", RoleLabel(role), record.m_sOwner, PlayerName(s_aPlayerIds[w]), Math.Round(distance)));
			}
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne de diagnostic d'un soldat à moins de DIAG_RANGE d'un joueur vivant (le plus proche) : rôle, état de menace,
	//! dernier facteur réellement posé, distance, ligne de vue, écart entre son cap et le joueur, assis ou adossé, ce que
	//! sa propre perception fait de ce joueur (catégorie, reconnaissance accumulée détection / identification), niveau
	//! de détail de l'IA (LOD), IA active, comportement en cours, environnement, à bord ou non (un servant de tourelle
	//! fixe est « assis » sans flâner). Catégorie ENNEMI.
	protected static void DiagSoldier(SRP_EnemyGroup record, int role, AIAgent agent, SCR_ChimeraAIAgent chimera, SCR_AICombatComponent combat, SCR_AIInfoComponent info, IEntity soldier)
	{
		vector from = soldier.GetOrigin();
		int nearest = -1;
		float best = DIAG_RANGE;
		int count = s_aPlayers.Count();
		for (int p = 0; p < count; p++)
		{
			float gap = vector.Distance(from, s_aPlayers[p].GetOrigin());
			if (gap > best)
				continue;
			best = gap;
			nearest = p;
		}
		if (nearest < 0)
			return;
		// Réglage de débogage sur le serveur (hors mode essai) : chaque ligne passe par le journal (fichier, console,
		// terminal Staff) ; au-delà du plafond, les soldats sont seulement comptés (ligne de fin de passage)
		if (!SRP_EnemyComponent.IsTestMode() && s_iDiagLines >= DIAG_MAX_LINES)
		{
			s_iDiagSkipped++;
			return;
		}
		s_iDiagLines++;
		IEntity player = s_aPlayers[nearest];

		// Écart entre le cap du corps et la direction du joueur, à plat, en degrés
		string heading = "?";
		float headingGap = HeadingGap(soldier, player);
		if (headingGap >= 0)
		{
			int degrees = Math.Round(headingGap);
			heading = degrees.ToString() + "°";
		}

		// Ce que sa propre perception fait de ce joueur
		string category = "aucune";
		string recognition = "-";
		PerceptionComponent perception = info.m_Perception;
		if (!perception)
			perception = PerceptionComponent.Cast(soldier.FindComponent(PerceptionComponent));
		if (perception)
		{
			BaseTarget target = perception.FindTargetPerceptionObject(player);
			if (target)
			{
				category = typename.EnumToString(ETargetCategory, target.GetTargetCategory());
				float detect;
				float identify;
				target.GetAccumulatedRecognition(detect, identify);
				recognition = Dec2(detect) + " / " + Dec2(identify);
			}
		}

		string factor = "CRX";
		if (combat.SRP_GetLastFactor() >= 0)
			factor = Dec2(combat.SRP_GetLastFactor());
		bool loitering = false;
		SCR_CharacterControllerComponent controller = ControllerOf(soldier);
		if (controller)
			loitering = controller.IsLoitering();
		string behavior = "aucun";
		if (chimera.m_UtilityComponent)
		{
			SCR_AIBehaviorBase current = chimera.m_UtilityComponent.GetCurrentBehavior();
			if (current)
				behavior = current.Type().ToString();
		}

		string line = string.Format("Diagnostic : %1 (%2) · état %3 · facteur %4 · %5 à %6 m · en vue %7 · écart de cap %8 · assis/adossé %9", RoleLabel(role), record.m_sOwner, StateLabel(combat.SRP_GetLastState()), factor, PlayerName(s_aPlayerIds[nearest]), Math.Round(best), YesNo(CanSee(soldier, player)), heading, YesNo(loitering));
		line += string.Format(" · sa perception : %1 (reconnaissance %2) · LOD %3 · IA active %4 · comportement %5 · calme %6 / combat %7 · à bord (tourelle, véhicule) %8", category, recognition, agent.GetLOD(), YesNo(agent.IsAIActivated()), behavior, Dec2(combat.SRP_GetEnvCalm()), Dec2(combat.SRP_GetEnvCombat()), YesNo(IsInVehicle(soldier)));
		SRP_EnemyComponent.Journal("ENNEMI", line);
	}

	//------------------------------------------------------------------------------------------------
	//! Révélation de près, de nuit : un joueur non éclairé vu de près par un soldat du groupe pendant s_iRevealMs est
	//! signalé au groupe (comme un tir entendu), une fois par s_iRevealCooldownMs au plus pour ce groupe
	protected static void RevealAtNight(SRP_EnemyGroup record, int role, array<bool> closeSeen, int now, float timeNow)
	{
		int playerCount = s_aPlayers.Count();
		for (int r = 0; r < playerCount; r++)
		{
			int playerId = s_aPlayerIds[r];
			if (!closeSeen[r])
			{
				record.m_mRevealStart.Remove(playerId);
				continue;
			}
			int start;
			if (!record.m_mRevealStart.Find(playerId, start))
			{
				record.m_mRevealStart.Set(playerId, now);
				continue;
			}
			if (now - start < s_iRevealMs)
				continue;
			if (record.m_iLastReveal != 0 && now - record.m_iLastReveal < s_iRevealCooldownMs)
				continue;
			IEntity revealed = s_aPlayers[r];
			if (IsIdentifiedBy(record, revealed, timeNow))
				continue;	// le groupe le voit en ce moment : le signalement ne changerait rien (ni ligne ni délai consommé)
			record.m_iLastReveal = now;
			Inform(record.m_Group, revealed, revealed.GetOrigin(), timeNow);
			NoteLine(string.Format("Repéré de près, de nuit : %1 (%2) a vu %3 à moins de %4 m, il le signale au groupe", RoleLabel(role), record.m_sOwner, PlayerName(playerId), Math.Round(s_fCloseDistance)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ce joueur est-il identifié par le groupe EN CE MOMENT (vu depuis moins de 4 s) ? Sous CRX, une cible identifiée
	//! n'est jamais oubliée : sans limite d'âge, un joueur vu une fois ne serait plus jamais révélé de près la nuit.
	protected static bool IsIdentifiedBy(SRP_EnemyGroup record, IEntity player, float timeNow)
	{
		if (!record || !record.m_Group || !player)
			return false;
		SCR_AIGroupUtilityComponent groupUtility = SCR_AIGroupUtilityComponent.Cast(record.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!groupUtility || !groupUtility.m_Perception)
			return false;
		foreach (SCR_AITargetInfo target : groupUtility.m_Perception.m_aTargets)
		{
			if (target && target.m_Entity == player && target.m_eCategory == EAITargetInfoCategory.IDENTIFIED && timeNow - target.m_fTimestamp < 4)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Profil CRX lu sur le premier soldat (avant nos réglages) : le fichier $profile:CRX_EAI s'écrit dans chaque
	//! soldat, le composant de réglages CRX donne les valeurs globales
	protected static void LogProfile(SCR_AIInfoComponent info)
	{
		string text;
		SCR_AISettingsComponent settings = SCR_AISettingsComponent.GetInstance();
		if (settings)
			text = string.Format("Profil CRX (premier soldat, avant nos réglages) : perception au calme %1, modificateur de perception %2, modificateur de visée %3, modificateur du délai de réaction %4 ; réglages globaux CRX : perception au calme %5, modificateur de visée %6, nuit prise en compte %7", Dec2(info.GetPerceptionSafe()), Dec2(info.GetPerceptionModifier()), Dec2(info.GetAimAccuracyErrorModifier()), Dec2(info.GetAttackReactionDelayModifier()), Dec2(settings.m_fGlobalPerceptionSafe), Dec2(settings.m_fGlobalAimAccuracyErrorModifier), YesNo(settings.m_bLowLightEnvironmentAffectsAI));
		else
			text = string.Format("Profil CRX (premier soldat, avant nos réglages) : perception au calme %1, modificateur de perception %2, modificateur de visée %3, modificateur du délai de réaction %4 ; pas de composant de réglages CRX", Dec2(info.GetPerceptionSafe()), Dec2(info.GetPerceptionModifier()), Dec2(info.GetAimAccuracyErrorModifier()), Dec2(info.GetAttackReactionDelayModifier()));
		SRP_EnemyComponent.Journal("ENNEMI", text);
	}

	//------------------------------------------------------------------------------------------------
	// Journal de repérage
	//------------------------------------------------------------------------------------------------
	//! Ce que le groupe a repéré : un joueur détecté note le premier doute ; un joueur identifié depuis moins de 4 s
	//! donne une ligne (une par s_iSightingLogMs au plus, pour ce groupe et ce joueur)
	static void NoteSightings(SRP_EnemyGroup record, int now, float timeNow)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted() || timeNow < 0)
			return;
		SCR_AIGroupUtilityComponent utility = SCR_AIGroupUtilityComponent.Cast(record.m_Group.FindComponent(SCR_AIGroupUtilityComponent));
		if (!utility || !utility.m_Perception)
			return;
		array<int> known = {};
		foreach (SCR_AITargetInfo target : utility.m_Perception.m_aTargets)
		{
			if (!target || !target.m_Entity)
				continue;
			int playerId = SRP_Utils.GetPlayerIdFromEntity(target.m_Entity);
			if (playerId <= 0)
				continue;	// une IA, un civil : pas notre affaire
			known.Insert(playerId);
			if (target.m_eCategory == EAITargetInfoCategory.DETECTED)
			{
				if (!record.m_mDoubtTick.Contains(playerId))
					record.m_mDoubtTick.Set(playerId, now);
				continue;
			}
			if (target.m_eCategory != EAITargetInfoCategory.IDENTIFIED || timeNow - target.m_fTimestamp > 4)
				continue;
			// Identifié : le doute est levé, même si la ligne attend son délai
			int doubt;
			bool hadDoubt = record.m_mDoubtTick.Find(playerId, doubt);
			record.m_mDoubtTick.Remove(playerId);
			int lastLine;
			if (record.m_mSpottedTick.Find(playerId, lastLine) && now - lastLine < s_iSightingLogMs)
				continue;
			record.m_mSpottedTick.Set(playerId, now);
			WriteSighting(record, target.m_Entity, playerId, hadDoubt, now - doubt);
		}

		// Un joueur que le groupe a oublié (le jeu efface une cible au bout de 150 s) n'a plus de doute en cours : sans
		// cela, un repérage 40 min plus tard serait daté du premier doute
		array<int> forgotten = {};
		foreach (int doubtId, int doubtTick : record.m_mDoubtTick)
		{
			if (!known.Contains(doubtId))
				forgotten.Insert(doubtId);
		}
		foreach (int forgottenId : forgotten)
			record.m_mDoubtTick.Remove(forgottenId);
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne de repérage : qui (le soldat qui tient lui-même le joueur pour ennemi dans sa perception, le plus proche ;
	//! à défaut le soldat du groupe le plus proche du joueur), quoi, dans quelles conditions, et le facteur de perception
	//! réellement posé sur ce soldat
	protected static void WriteSighting(SRP_EnemyGroup record, IEntity player, int playerId, bool hadDoubt, int elapsed)
	{
		IEntity nearest = null;
		SCR_AICombatComponent nearestCombat = null;
		bool nearestKnows = false;
		float best = 1000000;
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			IEntity soldier = agent.GetControlledEntity();
			if (!soldier || soldier.IsDeleted() || SRP_Utils.IsDead(soldier))
				continue;
			SCR_ChimeraAIAgent chimera = SCR_ChimeraAIAgent.Cast(agent);
			bool knows = false;
			PerceptionComponent perception = PerceptionComponent.Cast(soldier.FindComponent(PerceptionComponent));
			if (perception)
				knows = perception.GetTargetPerceptionObject(player, ETargetCategory.ENEMY) != null;
			float distance = vector.Distance(soldier.GetOrigin(), player.GetOrigin());
			// Celui qui le tient pour ennemi passe avant les autres ; à égalité, le plus proche
			if (nearestKnows && !knows)
				continue;
			if (knows == nearestKnows && distance >= best)
				continue;
			best = distance;
			nearest = soldier;
			nearestKnows = knows;
			nearestCombat = null;
			if (chimera && chimera.m_UtilityComponent)
				nearestCombat = chimera.m_UtilityComponent.m_CombatComponent;
		}

		int shownRole = record.m_iRole;
		if (shownRole < 0)
			shownRole = SRP_ECRXRole.PATROUILLE;	// comme UpdateGroup : un groupe sans rôle est réglé en ronde
		string who = RoleLabel(shownRole);
		if (nearest && IsVehicleCrew(nearest))
			who = "équipage";
		string meters = "?";
		if (nearest)
		{
			int rounded = Math.Round(best);
			meters = rounded.ToString();
		}
		string daylight = "jour";
		if (s_fNightGlobal < 1)
			daylight = "nuit";

		string light = "?";
		string torch = "?";
		string bush = "?";
		int index = s_aPlayers.Find(player);
		if (index >= 0)
		{
			light = Dec2(s_aIllum[index]);
			torch = YesNo(s_aTorch[index]);
			bush = YesNo(s_aHidden[index]);
		}

		// Facteur : le dernier réellement posé sur la perception de ce soldat (état, tirs, rôle, environnement, lunette,
		// équipement), et son état de menace à ce moment
		string factor = "?";
		string stateText = "?";
		if (nearestCombat && nearestCombat.SRP_IsManaged())
		{
			factor = "CRX";
			if (nearestCombat.SRP_GetLastFactor() >= 0)
				factor = Dec2(nearestCombat.SRP_GetLastFactor());
			stateText = StateLabel(nearestCombat.SRP_GetLastState());
		}

		string doubtText = "sans doute préalable";
		if (hadDoubt)
		{
			int seconds = Math.Round(elapsed / 1000.0);
			doubtText = string.Format("%1 s après le premier doute", seconds);
		}

		string line = string.Format("Repérage : %1 (%2) a VU %3 à %4 m, %5, %6", who, record.m_sOwner, PlayerName(playerId), meters, StanceLabel(StanceOf(player)), daylight);
		line += string.Format(", lumière %1, lampe %2, buisson %3, facteur posé %4 (%5), vu lui-même %6, %7", light, torch, bush, factor, stateText, YesNo(nearestKnows), doubtText);
		NoteLine(line);
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne dans l'anneau (heure en tête) et au journal ENNEMI
	protected static void NoteLine(string line)
	{
		string stamp = SRP_Time.Now();
		if (stamp.Length() >= 19)
			stamp = stamp.Substring(11, 8);	// « 21:14:03 »
		s_aSightings.Insert(stamp + " " + line);
		while (s_aSightings.Count() > SIGHTINGS_MAX)
			s_aSightings.RemoveOrdered(0);
		SRP_EnemyComponent.Journal("ENNEMI", line);
	}

	//------------------------------------------------------------------------------------------------
	//! Les lignes de l'anneau, la plus récente d'abord
	static void GetSightings(out array<string> lines)
	{
		if (!lines)
			return;
		for (int i = s_aSightings.Count() - 1; i >= 0; i--)
			lines.Insert(s_aSightings[i]);
	}

	//------------------------------------------------------------------------------------------------
	// Outils (pages Staff, journal)
	//------------------------------------------------------------------------------------------------
	static int CountPlayers()
	{
		return s_aPlayers.Count();
	}

	//------------------------------------------------------------------------------------------------
	static int CountLit()
	{
		int count = 0;
		foreach (bool lit : s_aLit)
		{
			if (lit)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	static int CountHidden()
	{
		int count = 0;
		foreach (bool hidden : s_aHidden)
		{
			if (hidden)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! « 1,30 », « -3,50 » : deux décimales, virgule française
	static string Dec2(float value)
	{
		int hundredths = Math.Round(value * 100);
		string sign = "";
		if (hundredths < 0)
		{
			sign = "-";
			hundredths = -hundredths;
		}
		int whole = hundredths / 100;
		int rest = hundredths - whole * 100;
		string restText = rest.ToString();
		if (rest < 10)
			restText = "0" + restText;
		return sign + whole.ToString() + "," + restText;
	}

	//------------------------------------------------------------------------------------------------
	static string YesNo(bool value)
	{
		if (value)
			return "oui";
		return "non";
	}

	//------------------------------------------------------------------------------------------------
	static string RoleLabel(int role)
	{
		if (role == SRP_ECRXRole.GARNISON)
			return "garde";
		if (role == SRP_ECRXRole.PATROUILLE)
			return "ronde";
		if (role == SRP_ECRXRole.ASSAUT)
			return "assaut";
		if (role == SRP_ECRXRole.CIVIL)
			return "civil";
		return "sans rôle";
	}

	//------------------------------------------------------------------------------------------------
	static string StanceLabel(int stance)
	{
		if (stance == ECharacterStance.CROUCH)
			return "accroupi";
		if (stance == ECharacterStance.PRONE)
			return "allongé";
		return "debout";
	}

	//------------------------------------------------------------------------------------------------
	static string ThreatLabel(EAIThreatState state)
	{
		if (state == EAIThreatState.SAFE)
			return "tranquille";
		if (state == EAIThreatState.VIGILANT)
			return "vigilant";
		if (state == EAIThreatState.ALERTED)
			return "alerté";
		if (state == EAIThreatState.THREATENED)
			return "menacé";
		return "supprimé";
	}

	//------------------------------------------------------------------------------------------------
	//! État de menace retenu par SRP_GetLastState (une valeur de EAIThreatState ; -1 : jamais lu)
	static string StateLabel(int state)
	{
		if (state < 0)
			return "inconnu";
		if (state == EAIThreatState.SAFE)
			return "tranquille";
		if (state == EAIThreatState.VIGILANT)
			return "vigilant";
		if (state == EAIThreatState.ALERTED)
			return "alerté";
		if (state == EAIThreatState.THREATENED)
			return "menacé";
		return "supprimé";
	}

	//------------------------------------------------------------------------------------------------
	protected static string PlayerName(int playerId)
	{
		return GetGame().GetPlayerManager().GetPlayerName(playerId);
	}
}
