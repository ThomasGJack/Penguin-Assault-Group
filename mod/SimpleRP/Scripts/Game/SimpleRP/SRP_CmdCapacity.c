//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) et front (Q7) : LA PLACE DES SOLDATS ENNEMIS.
//
// RÔLE (serveur ; singleton créé à la demande, SRP_CmdCapacity.Get(), valable MÊME sans Commandeur ou gelé : c'est
// la règle Q7 du front) :
// - cap_soldats_max = 120 soldats ennemis AU SOL, tout compris (RE11) : cap_reserve_renforts = 20 places gardées au
//   COMBAT et cap_reserve_postes = 12 aux postes du front (Q7, 4 postes au plus : réglage du front) ;
// - cap_groupes_max = 36 groupes, dont 6 aux missions, 5 aux renforts, 4 aux postes ;
// - cap_limite_jeu = 160 IA actives du jeu (AIWorld.SetLimitOfActiveAIs au démarrage, arbitrage) ;
// - classes de priorité (SRP_ECmdCapClass) et file RE12 : une demande COMBAT refusée est notée (NoteDemand) et fait
//   retirer, hors de vue, des patrouilles et des jeeps lointaines (SRP_EnemyComponent.ReleaseForRoom) ;
// - partage des places entre localités (C6) : la localité attaquée d'abord, à sa taille ; les autres au prorata de
//   leur taille (part double si un joueur est à cap_garnison_proche_m), cap_garnison_min au moins chacune,
//   cap_garnison_max au plus ; une garnison déjà posée n'est JAMAIS réduite sous les yeux des joueurs (GarrisonCap ne
//   borne que les nouvelles poses et les compléments) ;
// - TagImportance et TagCivilianGroup : les SEULS appels à SetImportance du mod (trou 18).
// Ni frein C2 (SRP_CmdManeuvers seul), ni registre de renforts, ni sauvegarde. Aucun CallLater (trou 23) : l'horloge
// est SRP_Commander.Tick ; un nouvel essai de la limite du jeu se fait au passage suivant.
//
// API DE POSE (toute pose d'IA du mod passe par ici, trou 18 et arbitrage Q7) :
//   1. string refus = SRP_CmdCapacity.Get().Ask(clé, classe, soldats, groupes, position, propriétaire) ;
//      "" = on pose ; sinon on ne pose pas et on réessaie plus tard (la demande reste notée cap_demande_s) ;
//   2. après chaque groupe posé : SRP_CmdCapacity.Tag(record, classe) (m_iCapClass + importance) ;
//   3. localités : pose = min(GetLocalityEffective, GarrisonCap(localité, décidé)) ;
//   4. civils : CivilianRefusal(nombre) avant la pose, TagCivilianGroup(groupe) après.
// Classes par poseur : garnison au calme GARNISON (COMBAT si la localité est attaquée) ; camions d'entrée, d'alerte,
// de contact, colonnes, vagues de contre-attaque et de mission, chauffeurs COMBAT ; servants de mortier et équipages
// de blindé COMBAT ; postes du front POSTE ; gardes de mission, officier de mission, escorte, garnison de livraison,
// officier de région et ses gardes, gardes du dépôt, garde de la batterie GARDE ; patrouilles de fond, rondes, jeeps,
// infiltrations, raids, survivants en retrait PATROUILLE ; équipage de l'hélico HORS_COMPTE.
// APPELÉ PAR : SRP_Enemy, SRP_EnemyTrucks, SRP_EnemyGarrison, SRP_Territory, SRP_Missions, SRP_Delivery,
// SRP_Civilians, SRP_HeliSearch, SRP_FrontEnemyComponent (postes, vagues, SetWantedGarrisons), le Commandeur
// (officiers, dépôts, servants, équipages, renforts, raids), SRP_CmdScreens (Staff).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Une demande de place en attente (file RE12), valable cap_demande_s, renouvelée à chaque essai. Non sauvée.
class SRP_CmdDemand
{
	string m_sKey;								// « renfort-S07 », « officier-R3 », « depot-R3 », « mission-M07 »…
	int m_iClass = SRP_ECmdCapClass.COMBAT;
	int m_iSoldiers;
	int m_iGroups;
	vector m_vPos;
	int m_iSince;								// heure Unix de la 1re demande
	int m_iUntil;								// heure Unix de fin de validité
}

//------------------------------------------------------------------------------------------------
//! La place des soldats ennemis (serveur).
class SRP_CmdCapacity
{
	// --- Réglages (clés cap_*) ; défauts = choix de Jack, valables avant toute lecture du fichier -----------------
	int m_iEngineLimit = 160;		// cap_limite_jeu
	int m_iMaxSoldiers = 120;		// cap_soldats_max
	int m_iCombatReserve = 20;		// cap_reserve_renforts
	int m_iPostReserve = 12;		// cap_reserve_postes
	int m_iMaxGroups = 36;		// cap_groupes_max
	int m_iMissionGroupReserve = 6;		// cap_groupes_reserve_missions
	int m_iCombatGroupReserve = 5;		// cap_groupes_reserve_renforts
	int m_iPostGroupReserve = 4;		// cap_groupes_reserve_postes
	int m_iEngineMargin = 10;		// cap_marge_jeu
	int m_iPlannedCivilians = 25;		// cap_civils_prevus
	int m_iPlannedPilots = 2;		// cap_pilotes_prevus
	int m_iReleasePatrolM = 800;		// cap_retrait_rondes_m
	int m_iReleaseJeepM = 1500;		// cap_retrait_jeeps_m
	int m_iReleasePerPass = 2;		// cap_retrait_par_passe
	int m_iDemandS = 45;		// cap_demande_s
	int m_iGarrisonMin = 8;		// cap_garnison_min
	int m_iGarrisonMax = 60;		// cap_garnison_max
	int m_iGarrisonNearM = 1000;		// cap_garnison_proche_m
	int m_iGarrisonNearWeight = 2;		// cap_garnison_poids_proche
	int m_iImportanceEnemy = 3;		// cap_importance_ennemis
	int m_iImportancePatrol = 2;		// cap_importance_rondes
	int m_iImportanceCivilian = 2;		// cap_importance_civils

	// --- Constantes ----------------------------------------------------------------------------------------------
	protected static const int CLASS_COUNT = 6;			// COMBAT à HORS_COMPTE (SRP_ECmdCapClass)
	protected static const int LIMIT_TRIES_MAX = 12;		// essais de la limite du jeu sans AIWorld (un par passage de 5 s)
	protected static const int SHARES_SHOWN = 10;		// parts des localités montrées au Staff

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected static ref SRP_CmdCapacity s_Instance;
	protected ref array<ref SRP_CmdDemand> m_aDemands = {};
	protected ref map<string, int> m_mGarrisonShare = new map<string, int>();	// localité -> part (C6)
	protected bool m_bStarted;
	protected int m_iLimitTries;						// essais de ApplyEngineLimit (AIWorld absent)
	protected int m_iLimitRead = -1;					// limite du jeu lue après réglage
	protected int m_iNextMinute;						// sous-cadence de 60 s (heure Unix)
	protected string m_sLastRefusal;					// dernier refus (Staff)

	//================================================================================================
	// Vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Le singleton, créé à la demande avec les valeurs de Jack (même sans Commandeur, même gelé) — toutes les poses
	static SRP_CmdCapacity Get()
	{
		if (!s_Instance)
			s_Instance = new SRP_CmdCapacity();
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés (défaut = valeur du champ) — SRP_Commander.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("cap_limite_jeu", m_iEngineLimit, "Limite d'IA actives du jeu, posée au démarrage (Q7)");
		SRP_CmdSettings.DeclareInt("cap_soldats_max", m_iMaxSoldiers, "Soldats ennemis au sol, tout compris (Q7)");
		SRP_CmdSettings.DeclareInt("cap_reserve_renforts", m_iCombatReserve, "Places gardées aux renforts et aux combats (RE11)");
		SRP_CmdSettings.DeclareInt("cap_reserve_postes", m_iPostReserve, "Places gardées aux postes du front (Q7)");
		SRP_CmdSettings.DeclareInt("cap_groupes_max", m_iMaxGroups, "Éléments ennemis au plus (RE11)");
		SRP_CmdSettings.DeclareInt("cap_groupes_reserve_missions", m_iMissionGroupReserve, "Éléments gardés aux missions (RE11)");
		SRP_CmdSettings.DeclareInt("cap_groupes_reserve_renforts", m_iCombatGroupReserve, "Éléments gardés aux renforts (RE11)");
		SRP_CmdSettings.DeclareInt("cap_groupes_reserve_postes", m_iPostGroupReserve, "Éléments gardés aux postes du front (Q7)");
		SRP_CmdSettings.DeclareInt("cap_marge_jeu", m_iEngineMargin, "Places gardées sous la limite du jeu (Q7)");
		SRP_CmdSettings.DeclareInt("cap_civils_prevus", m_iPlannedCivilians, "Contrôle de cohérence : civils prévus (Q7)");
		SRP_CmdSettings.DeclareInt("cap_pilotes_prevus", m_iPlannedPilots, "Contrôle de cohérence : pilotes prévus (Q7)");
		SRP_CmdSettings.DeclareInt("cap_retrait_rondes_m", m_iReleasePatrolM, "Place manquante : patrouilles retirées hors de vue au-delà de cette distance des joueurs, en mètres (RE12)");
		SRP_CmdSettings.DeclareInt("cap_retrait_jeeps_m", m_iReleaseJeepM, "Place manquante : jeeps retirées au-delà de cette distance, en mètres (RE12)");
		SRP_CmdSettings.DeclareInt("cap_retrait_par_passe", m_iReleasePerPass, "Éléments retirés au plus à chaque passage de 5 s (RE12)");
		SRP_CmdSettings.DeclareInt("cap_demande_s", m_iDemandS, "Une demande de place en attente reste valable ce nombre de secondes (RE12)");
		SRP_CmdSettings.DeclareInt("cap_garnison_min", m_iGarrisonMin, "Part minimale d'une localité, en soldats (C6)");
		SRP_CmdSettings.DeclareInt("cap_garnison_max", m_iGarrisonMax, "Soldats au plus dans une localité (OF5)");
		SRP_CmdSettings.DeclareInt("cap_garnison_proche_m", m_iGarrisonNearM, "Part double d'une localité si un joueur est à cette distance, en mètres (C6)");
		SRP_CmdSettings.DeclareInt("cap_garnison_poids_proche", m_iGarrisonNearWeight, "Poids d'une localité proche d'un joueur (C6)");
		SRP_CmdSettings.DeclareInt("cap_importance_ennemis", m_iImportanceEnemy, "Importance des soldats pour le jeu : 0 basse, 1 normale, 2 haute, 3 critique (RE12)");
		SRP_CmdSettings.DeclareInt("cap_importance_rondes", m_iImportancePatrol, "Importance des patrouilles et rondes (RE12)");
		SRP_CmdSettings.DeclareInt("cap_importance_civils", m_iImportanceCivilian, "Importance des civils (RE12)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés ; si déjà démarrée : ApplyEngineLimit et SRP_EnemyComponent.SetMaxGroups(cap_groupes_max,
	//! cap_groupes_reserve_missions) — SRP_Commander.LoadSettings
	void LoadSettings()
	{
		m_iEngineLimit = SRP_CmdSettings.GetIntClamped("cap_limite_jeu", 40, 1000);
		m_iMaxSoldiers = SRP_CmdSettings.GetIntClamped("cap_soldats_max", 10, 500);
		m_iCombatReserve = SRP_CmdSettings.GetIntClamped("cap_reserve_renforts", 0, 200);
		m_iPostReserve = SRP_CmdSettings.GetIntClamped("cap_reserve_postes", 0, 200);
		m_iMaxGroups = SRP_CmdSettings.GetIntClamped("cap_groupes_max", 1, 200);
		m_iMissionGroupReserve = SRP_CmdSettings.GetIntClamped("cap_groupes_reserve_missions", 0, 50);
		m_iCombatGroupReserve = SRP_CmdSettings.GetIntClamped("cap_groupes_reserve_renforts", 0, 50);
		m_iPostGroupReserve = SRP_CmdSettings.GetIntClamped("cap_groupes_reserve_postes", 0, 50);
		m_iEngineMargin = SRP_CmdSettings.GetIntClamped("cap_marge_jeu", 0, 100);
		m_iPlannedCivilians = SRP_CmdSettings.GetIntClamped("cap_civils_prevus", 0, 200);
		m_iPlannedPilots = SRP_CmdSettings.GetIntClamped("cap_pilotes_prevus", 0, 20);
		m_iReleasePatrolM = SRP_CmdSettings.GetIntClamped("cap_retrait_rondes_m", 0, 10000);
		m_iReleaseJeepM = SRP_CmdSettings.GetIntClamped("cap_retrait_jeeps_m", 0, 10000);
		m_iReleasePerPass = SRP_CmdSettings.GetIntClamped("cap_retrait_par_passe", 0, 20);
		m_iDemandS = SRP_CmdSettings.GetIntClamped("cap_demande_s", 5, 600);
		m_iGarrisonMin = SRP_CmdSettings.GetIntClamped("cap_garnison_min", 0, 60);
		m_iGarrisonMax = SRP_CmdSettings.GetIntClamped("cap_garnison_max", 1, 200);
		m_iGarrisonNearM = SRP_CmdSettings.GetIntClamped("cap_garnison_proche_m", 0, 10000);
		m_iGarrisonNearWeight = SRP_CmdSettings.GetIntClamped("cap_garnison_poids_proche", 1, 10);
		m_iImportanceEnemy = SRP_CmdSettings.GetIntClamped("cap_importance_ennemis", 0, 3);
		m_iImportancePatrol = SRP_CmdSettings.GetIntClamped("cap_importance_rondes", 0, 3);
		m_iImportanceCivilian = SRP_CmdSettings.GetIntClamped("cap_importance_civils", 0, 3);

		// Relu en cours de partie (« Relire les réglages ») : la limite du jeu et le plafond d'éléments suivent tout
		// de suite ; au démarrage (avant Start), c'est Start qui les pose
		if (!m_bStarted)
			return;
		ApplyEngineLimit();
		ApplyGroupCap();
		CheckCoherence();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, front prêt : ApplyEngineLimit (160), SRP_EnemyComponent.SetMaxGroups(36, 6) ; contrôle de cohérence
	//! cap_soldats_max + cap_civils_prevus + cap_pilotes_prevus + cap_marge_jeu <= cap_limite_jeu, sinon ERREUR
	//! « réglages incohérents » au journal — SRP_Commander.Start
	void Start()
	{
		m_bStarted = true;
		m_iLimitTries = 0;
		m_iLimitRead = -1;
		m_iNextMinute = System.GetUnixTime() + 60;
		ApplyEngineLimit();
		ApplyGroupCap();
		CapNote(string.Format("%1 soldats au sol au plus (%2 places gardées aux renforts, %3 aux postes du front), %4 éléments au plus (%5 gardés aux missions, %6 aux renforts, %7 aux postes), limite du jeu voulue : %8", m_iMaxSoldiers, m_iCombatReserve, m_iPostReserve, m_iMaxGroups, m_iMissionGroupReserve, m_iCombatGroupReserve, m_iPostGroupReserve, m_iEngineLimit));
		CheckCoherence();
	}

	//------------------------------------------------------------------------------------------------
	//! Arrêt : demandes et parts oubliées — SRP_Commander.Stop
	void Stop()
	{
		// Le singleton survit aux parties du Workbench (statique) : tout ce qui est propre à la partie est oublié ici
		m_aDemands.Clear();
		m_mGarrisonShare.Clear();
		m_bStarted = false;
		m_iLimitTries = 0;
		m_iLimitRead = -1;
		m_iNextMinute = 0;
		m_sLastRefusal = "";
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s (TOUJOURS, même Commandeur gelé) : demandes expirées retirées ; RE12 : si les demandes COMBAT
	//! dépassent Room(COMBAT), SRP_EnemyComponent.ReleaseForRoom(manque, cap_retrait_rondes_m, cap_retrait_jeeps_m,
	//! cap_retrait_par_passe) ; chaque minute : ApplyEngineLimit si la limite lue a bougé — SRP_Commander.Tick
	void Tick(int nowUnix)
	{
		PruneDemands(nowUnix);

		// RE12 : les renforts attendent une place -> des rondes et des jeeps lointaines partent hors de vue
		int wanted = Pending(SRP_ECmdCapClass.COMBAT);
		if (wanted > 0 && m_iReleasePerPass > 0)
		{
			int deficit = wanted - this.Room(SRP_ECmdCapClass.COMBAT, "");	// « this. » : Room seul désigne la classe Room du jeu
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			if (deficit > 0 && enemies)
			{
				int freed = enemies.ReleaseForRoom(deficit, m_iReleasePatrolM, m_iReleaseJeepM, m_iReleasePerPass);
				if (freed > 0)
					CapNote(string.Format("%1 place(s) manquent aux renforts : %2 soldat(s) des rondes et des jeeps lointaines retirés hors de vue", deficit, freed));
			}
		}

		if (!m_bStarted)
			return;

		// Limite du jeu : AIWorld absent au démarrage -> nouvel essai à chaque passage (12 au plus) ; ensuite, contrôle
		// chaque minute (un autre script ou la configuration du serveur a pu la changer)
		if (m_iLimitRead < 0)
		{
			if (m_iLimitTries < LIMIT_TRIES_MAX)
				ApplyEngineLimit();
			return;
		}
		if (nowUnix < m_iNextMinute)
			return;
		m_iNextMinute = nowUnix + 60;
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld && aiWorld.GetLimitOfActiveAIs() != m_iLimitRead)
			ApplyEngineLimit();
	}

	//------------------------------------------------------------------------------------------------
	//! GetGame().GetAIWorld().SetLimitOfActiveAIs(cap_limite_jeu) si la limite lue diffère ; relit et journalise
	//! « Limite du jeu : %1 avant, %2 voulue, %3 lue » (ERREUR si elle n'a pas pris : la clé aiLimit de la
	//! configuration du serveur l'emporte peut-être) ; AIWorld absent : nouvel essai (12 au plus) — Start, Tick
	void ApplyEngineLimit()
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
		{
			// Nouvel essai au passage suivant (Tick, 5 s), LIMIT_TRIES_MAX fois au plus
			m_iLimitTries++;
			if (m_iLimitTries == LIMIT_TRIES_MAX)
				CapError(string.Format("limite du jeu non posée : AIWorld absent après %1 essais (%2 voulue)", m_iLimitTries, m_iEngineLimit));
			return;
		}
		m_iLimitTries = 0;

		int before = aiWorld.GetLimitOfActiveAIs();
		int wantedLimit = m_iEngineLimit;
		if (before != wantedLimit)
			aiWorld.SetLimitOfActiveAIs(wantedLimit);
		int after = aiWorld.GetLimitOfActiveAIs();
		m_iLimitRead = after;

		string text = string.Format("Limite du jeu : %1 avant, %2 voulue, %3 lue", before, wantedLimit, after);

		// Limite propre à la faction ennemie (-1 = aucune) : elle bornerait les soldats avant les 120
		int factionLimit = -1;
		string faction = "";
		ChimeraAIWorld chimeraWorld = ChimeraAIWorld.Cast(aiWorld);
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (chimeraWorld && enemies)
		{
			faction = enemies.GetFactionKey();
			if (!faction.IsEmpty())
				factionLimit = chimeraWorld.GetAILimitForFaction(faction);
		}
		if (factionLimit >= 0)
			text += string.Format(" ; limite propre à la faction %1 : %2", faction, factionLimit);
		CapNote(text);

		if (after != wantedLimit)
			CapError(string.Format("la limite du jeu n'a pas pris (%1 voulue, %2 lue) : la clé aiLimit de la configuration du serveur l'emporte peut-être", wantedLimit, after));
		if (factionLimit >= 0 && factionLimit < m_iMaxSoldiers)
			CapError(string.Format("la limite propre à la faction %1 (%2) est sous les %3 soldats ennemis voulus", faction, factionLimit, m_iMaxSoldiers));
	}

	//================================================================================================
	// Classes (statiques : utilisables sans instance)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Classe d'un groupe : m_iCapClass s'il est posé (>= 0), sinon devinée (propriétaire « helico » HORS_COMPTE ;
	//! tâche « jeep » ou propriétaire « ambiance » ou « survivant » PATROUILLE ; tâche « vague », « assaut », « camion »,
	//! « chauffeur » ou propriétaire « camion » COMBAT ; « mission », « officier », « depot », « livraison » ou tâche
	//! « garde » GARDE ; « poste front » POSTE ; sinon GARNISON), en if/else — comptes, TagImportance
	static int ClassOf(SRP_EnemyGroup record)
	{
		if (!record)
			return SRP_ECmdCapClass.GARNISON;
		if (record.m_iCapClass >= 0 && record.m_iCapClass < CLASS_COUNT)
			return record.m_iCapClass;

		string owner = record.m_sOwner;
		string task = record.m_sHomeTask;
		if (task.IsEmpty())
			task = record.m_sTask;

		if (owner == "helico")
			return SRP_ECmdCapClass.HORS_COMPTE;
		if (task == "jeep" || owner == "ambiance" || owner == "survivant")
			return SRP_ECmdCapClass.PATROUILLE;
		if (task == "vague" || task == "assaut" || task == "camion" || task == "chauffeur" || owner == "camion" || owner == "vague")
			return SRP_ECmdCapClass.COMBAT;
		if (owner == "mission" || owner == "officier" || owner == "depot" || owner == "livraison" || task == "garde" || task == "officier")
			return SRP_ECmdCapClass.GARDE;
		if (task == "poste front")
			return SRP_ECmdCapClass.POSTE;
		return SRP_ECmdCapClass.GARNISON;
	}

	//------------------------------------------------------------------------------------------------
	//! Après CHAQUE groupe posé : record.m_iCapClass = cls, puis TagImportance — toutes les poses
	static void Tag(SRP_EnemyGroup record, int cls)
	{
		if (!record)
			return;
		record.m_iCapClass = ValidClass(cls);
		TagImportance(record);
	}

	//------------------------------------------------------------------------------------------------
	//! SEUL appel à SetImportance pour l'ennemi : cap_importance_ennemis (3, CRITICAL), cap_importance_rondes (2, HIGH)
	//! pour PATROUILLE — Tag, reclassement (débarqués qui rejoignent une garnison)
	static void TagImportance(SRP_EnemyGroup record)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return;
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		int importance = capacity.m_iImportanceEnemy;
		if (ClassOf(record) == SRP_ECmdCapClass.PATROUILLE)
			importance = capacity.m_iImportancePatrol;
		record.m_Group.SetImportance(importance);
	}

	//------------------------------------------------------------------------------------------------
	//! SEUL appel à SetImportance pour un civil : cap_importance_civils (2, HIGH) — SRP_Civilians (après la pose)
	static void TagCivilianGroup(SCR_AIGroup group)
	{
		if (!group || group.IsDeleted())
			return;
		group.SetImportance(SRP_CmdCapacity.Get().m_iImportanceCivilian);
	}

	//------------------------------------------------------------------------------------------------
	//! « renforts », « défense des localités », « postes », « gardes », « rondes », « hors compte » — Staff, refus
	static string ClassLabel(int cls)
	{
		if (cls == SRP_ECmdCapClass.COMBAT)
			return "renforts";
		if (cls == SRP_ECmdCapClass.GARNISON)
			return "défense des localités";
		if (cls == SRP_ECmdCapClass.POSTE)
			return "postes";
		if (cls == SRP_ECmdCapClass.GARDE)
			return "gardes";
		if (cls == SRP_ECmdCapClass.PATROUILLE)
			return "rondes";
		if (cls == SRP_ECmdCapClass.HORS_COMPTE)
			return "hors compte";
		return "classe inconnue";
	}

	//================================================================================================
	// Comptes (groupes de SRP_EnemyComponent.GetGroups, attendus compris pendant la pose)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Soldats au sol, toutes classes sauf HORS_COMPTE — Room, Staff
	int CountGround()
	{
		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);
		return GroundOf(soldiers);
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats d'une classe — Room, Staff
	int CountClass(int cls)
	{
		if (cls < 0 || cls >= CLASS_COUNT)
			return 0;
		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);
		return soldiers[cls];
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes vivants hors HORS_COMPTE — GroupRoom, Staff
	int CountGroundGroups()
	{
		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);
		return GroundOf(groups);
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes d'une classe — GroupRoom, Staff
	int CountClassGroups(int cls)
	{
		if (cls < 0 || cls >= CLASS_COUNT)
			return 0;
		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);
		return groups[cls];
	}

	//================================================================================================
	// Place
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Soldats qu'une classe peut encore poser : cap_soldats_max - au sol, moins les réserves inutilisées des classes
	//! plus prioritaires (COMBAT garde cap_reserve_postes ; POSTE garde les renforts et les demandes COMBAT ;
	//! GARNISON, GARDE, PATROUILLE gardent aussi les demandes des classes au-dessus), borné par EngineRoom ;
	//! HORS_COMPTE : 1000 — Refusal, poses qui dimensionnent
	int Room(int cls, string owner)
	{
		bool byEngine;
		return RoomOf(cls, byEngine);
	}

	//------------------------------------------------------------------------------------------------
	//! Groupes qu'une classe peut encore poser : cap_groupes_max, moins cap_groupes_reserve_missions hors « mission »,
	//! moins les groupes vivants, moins les réserves inutilisées (renforts sauf COMBAT, postes sauf POSTE si un poste
	//! est possible) — Refusal
	int GroupRoom(int cls, string owner)
	{
		int klass = ValidClass(cls);
		if (klass == SRP_ECmdCapClass.HORS_COMPTE)
			return 1000;

		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);

		int room = m_iMaxGroups - GroundOf(groups);
		if (owner != "mission")
			room -= m_iMissionGroupReserve;
		if (klass != SRP_ECmdCapClass.COMBAT)
			room -= CapMax(0, m_iCombatGroupReserve - groups[SRP_ECmdCapClass.COMBAT]);
		if (klass != SRP_ECmdCapClass.POSTE && IsPostReserveActive())
			room -= CapMax(0, m_iPostGroupReserve - groups[SRP_ECmdCapClass.POSTE]);
		return CapMax(room, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Place du jeu : GetLimitOfActiveAIs - GetCurrentNumOfActiveAIs - SRP_EnemyComponent.CountPendingSpawn -
	//! cap_marge_jeu ; 1000 sans AIWorld — Room, CivilianRefusal
	int EngineRoom()
	{
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (!aiWorld)
			return 1000;
		int limit = aiWorld.GetLimitOfActiveAIs();
		if (limit <= 0)
			return 1000;		// pas de limite du jeu

		// Soldats encore attendus par la file d'apparition : pas encore actifs, mais leur place est prise
		int waiting = 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
			waiting = enemies.CountPendingSpawn();
		return CapMax(limit - aiWorld.GetCurrentNumOfActiveAIs() - waiting - m_iEngineMargin, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! "" si soldiers <= Room(cls, owner) et groups <= GroupRoom(cls, owner) ; sinon la raison en clair pour le journal
	//! (« place : 11 demandées, 4 possibles pour les renforts ») — Ask, poses qui ne font pas la queue
	string Refusal(int cls, int soldiers, int groups, string owner)
	{
		if (soldiers <= 0 && groups <= 0)
			return "";
		int klass = ValidClass(cls);

		string why = "";
		bool byEngine;
		int room = RoomOf(klass, byEngine);
		if (soldiers > room)
		{
			why = string.Format("place : %1 demandées, %2 possibles %3", soldiers, room, ClassFor(klass));
			if (byEngine)
				why += " (limite du jeu)";
		}
		else
		{
			int groupRoom = GroupRoom(klass, owner);
			if (groups > groupRoom)
				why = string.Format("place : %1 éléments demandés, %2 possibles %3 (%4 éléments au plus)", groups, groupRoom, ClassFor(klass), m_iMaxGroups);
		}
		if (!why.IsEmpty())
			m_sLastRefusal = why;
		return why;
	}

	//------------------------------------------------------------------------------------------------
	//! Porte de pose recommandée : Refusal ; "" -> ClearDemand(key) et on pose ; sinon NoteDemand(key, …) (file RE12)
	//! et la raison est rendue — toutes les poses du mod
	string Ask(string key, int cls, int soldiers, int groups, vector position, string owner)
	{
		string why = Refusal(cls, soldiers, groups, owner);
		if (why.IsEmpty())
		{
			ClearDemand(key);
			return "";
		}
		NoteDemand(key, ValidClass(cls), soldiers, groups, position);
		return why;
	}

	//------------------------------------------------------------------------------------------------
	//! Crée ou renouvelle la demande key (valable cap_demande_s) — Ask, poses qui attendent
	void NoteDemand(string key, int cls, int soldiers, int groups, vector position)
	{
		int now = System.GetUnixTime();
		PruneDemands(now);		// sans Commandeur, pas de Tick : la file se nettoie ici

		SRP_CmdDemand demand = FindDemand(key);
		if (!demand)
		{
			demand = new SRP_CmdDemand();
			demand.m_sKey = key;
			demand.m_iSince = now;
			m_aDemands.Insert(demand);
		}
		demand.m_iClass = ValidClass(cls);
		demand.m_iSoldiers = CapMax(soldiers, 0);
		demand.m_iGroups = CapMax(groups, 0);
		demand.m_vPos = position;
		demand.m_iUntil = now + m_iDemandS;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire la demande key (posée ou abandonnée) — Ask, poseurs
	void ClearDemand(string key)
	{
		for (int i = m_aDemands.Count() - 1; i >= 0; i--)
		{
			SRP_CmdDemand demand = m_aDemands[i];
			if (!demand || demand.m_sKey == key)
				m_aDemands.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats demandés en attente pour une classe (demandes valables) — Staff, SetWantedGarrisons
	int GetPendingSoldiers(int cls)
	{
		return Pending(cls);
	}

	//------------------------------------------------------------------------------------------------
	//! Civils : "" si count <= EngineRoom() - max(0, cap_soldats_max - CountGround()) (les civils ne prennent jamais la
	//! place du jeu que les 120 soldats peuvent encore demander) ; sinon la raison — SRP_Civilians (avant la pose)
	string CivilianRefusal(int count)
	{
		if (count <= 0)
			return "";
		int engine = EngineRoom();
		int keptForSoldiers = CapMax(0, m_iMaxSoldiers - CountGround());
		int allowed = engine - keptForSoldiers;
		if (count <= allowed)
			return "";
		string why = string.Format("place du jeu : %1 civils demandés, %2 possibles (%3 places libres sous la limite du jeu, %4 gardées aux soldats ennemis)", count, CapMax(allowed, 0), engine, keptForSoldiers);
		m_sLastRefusal = why;
		return why;
	}

	//================================================================================================
	// Partage des localités (C6)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! À chaque passage de 5 s de l'ennemi : parts de chaque localité voulue (keys, sizes = effectif voulu, near =
	//! joueur à cap_garnison_proche_m, attacked = SRP_FrontEnemyComponent.IsLocalityUnderAttack) : attaquées d'abord à
	//! leur taille, les autres au prorata (poids double si near, 3 passes), plancher cap_garnison_min, plafond
	//! cap_garnison_max — SRP_FrontEnemyComponent.Tick
	void SetWantedGarrisons(notnull array<string> keys, notnull array<int> sizes, notnull array<bool> near, notnull array<bool> attacked)
	{
		// Parts refaites à chaque passage : une localité qui n'est plus voulue n'a plus de part (GarrisonCap la borne
		// alors à cap_garnison_max, et Ask reste la borne absolue)
		m_mGarrisonShare.Clear();
		int count = keys.Count();
		if (sizes.Count() < count)
			count = sizes.Count();
		if (near.Count() < count)
			count = near.Count();
		if (attacked.Count() < count)
			count = attacked.Count();
		if (count <= 0)
			return;

		// Variables déclarées une seule fois (pas de redéclaration dans un bloc imbriqué)
		int i;
		int pass;
		int give;
		int part;
		int weight;
		int weightSum;
		int handed;
		int low;

		// Soldats au sol qui ne défendent aucune localité voulue : tout sauf la défense des localités (GARNISON) et
		// les soldats COMBAT posés POUR une localité voulue (garnison posée pendant une attaque, m_sZone = la
		// localité) ; ceux-là font partie de l'effectif de leur localité et ne sont pas comptés deux fois
		int others = 0;
		int combatSoldiers = 0;
		int postSoldiers = 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
		{
			array<ref SRP_EnemyGroup> records = enemies.GetGroups();
			if (records)
			{
				int tick = System.GetTickCount();		// SoldiersOf compte la grâce de la file en GetTickCount
				foreach (SRP_EnemyGroup record : records)
				{
					if (!record || !record.m_Group || record.m_Group.IsDeleted())
						continue;
					int cls = ClassOf(record);
					if (cls == SRP_ECmdCapClass.HORS_COMPTE)
						continue;
					int soldiers = SRP_EnemyComponent.SoldiersOf(record, tick);
					if (cls == SRP_ECmdCapClass.COMBAT)
						combatSoldiers += soldiers;
					if (cls == SRP_ECmdCapClass.POSTE)
						postSoldiers += soldiers;
					if (cls == SRP_ECmdCapClass.GARNISON)
						continue;
					if (cls == SRP_ECmdCapClass.COMBAT && !record.m_sZone.IsEmpty() && keys.Contains(record.m_sZone))
						continue;
					others += soldiers;
				}
			}
		}

		// Place des localités : 120, moins les réserves inutilisées (renforts ; postes si un poste est possible), moins
		// les soldats hors des localités, moins les renforts qui attendent une place (RE12)
		int keepCombat = CapMax(0, m_iCombatReserve - combatSoldiers);
		int keepPost = 0;
		if (IsPostReserveActive())
			keepPost = CapMax(0, m_iPostReserve - postSoldiers);
		int pool = CapMax(0, m_iMaxSoldiers - keepCombat - keepPost - others - Pending(SRP_ECmdCapClass.COMBAT));

		array<int> shares = {};
		array<bool> done = {};
		for (i = 0; i < count; i++)
		{
			shares.Insert(0);
			if (sizes[i] > 0)
				done.Insert(false);
			else
				done.Insert(true);		// rien de voulu : part nulle, hors du partage
		}

		// (a) Les localités attaquées d'abord, à leur taille
		for (i = 0; i < count; i++)
		{
			if (done[i] || !attacked[i])
				continue;
			give = CapMin(sizes[i], pool);
			shares[i] = give;
			pool -= give;
			done[i] = true;
		}

		// (b) Les autres au prorata de leur taille (poids plus fort si un joueur est proche), 3 passes : une localité
		// servie à sa taille sort du partage et le reste revient aux autres au passage suivant
		for (pass = 0; pass < 3; pass++)
		{
			if (pool <= 0)
				break;
			weightSum = 0;
			for (i = 0; i < count; i++)
			{
				if (!done[i])
					weightSum += WeightOf(sizes[i], near[i]);
			}
			if (weightSum <= 0)
				break;
			handed = 0;
			for (i = 0; i < count; i++)
			{
				if (done[i])
					continue;
				weight = WeightOf(sizes[i], near[i]);
				part = pool * weight / weightSum;
				give = CapMin(part, sizes[i] - shares[i]);
				if (give > 0)
				{
					shares[i] = shares[i] + give;
					handed += give;
				}
				if (shares[i] >= sizes[i])
					done[i] = true;
			}
			pool -= handed;
			if (handed <= 0)
				break;
		}

		// (c) Plancher (cap_garnison_min, jamais plus que la taille voulue) et plafond (cap_garnison_max)
		for (i = 0; i < count; i++)
		{
			low = CapMax(CapMin(sizes[i], m_iGarrisonMin), 0);
			if (shares[i] < low)
				shares[i] = low;
			if (shares[i] > m_iGarrisonMax)
				shares[i] = m_iGarrisonMax;
			m_mGarrisonShare.Set(keys[i], shares[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats qu'une localité peut poser ou compléter : min(decided, part, cap_garnison_max) ; localité sans part :
	//! min(decided, cap_garnison_max) (la pose reste bornée par Ask) ; ne réduit jamais une garnison posée —
	//! SRP_TerritoryComponent (pose F11), SRP_EnemyGarrison
	int GarrisonCap(string locality, int decided)
	{
		// Un plafond de pose et de complément seulement : l'appelant ne retire jamais des soldats déjà posés
		int cap = CapMin(decided, m_iGarrisonMax);
		int share;
		if (m_mGarrisonShare.Find(locality, share))
			cap = CapMin(cap, share);
		return CapMax(cap, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! cap_garnison_max (60) : plafond de l'effectif Nominal et Effective d'une localité (OF5) — SRP_FrontEnemyComponent,
	//! SRP_TerritoryComponent.GarrisonSoldiers
	int GetGarrisonMax()
	{
		return m_iGarrisonMax;
	}

	//------------------------------------------------------------------------------------------------
	//! cap_soldats_max — SRP_EnemyComponent (anciens GetMaxSoldiers), écrans
	int GetMaxSoldiers()
	{
		return m_iMaxSoldiers;
	}

	//------------------------------------------------------------------------------------------------
	//! cap_groupes_max — SRP_EnemyComponent.SetMaxGroups, écrans
	int GetMaxGroups()
	{
		return m_iMaxGroups;
	}

	//================================================================================================
	// Staff
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! « Soldats ennemis au sol : 112 sur 120 (renforts 18, défense des localités 71, postes 6, gardes 8, rondes 9) ·
	//! réserves libres : renforts 2, postes 6 · en attente : renforts 11 · éléments : 28 sur 36 · limite du jeu :
	//! 139 actifs sur 160 » — SRP_CmdScreens
	string GetStaffReport()
	{
		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);

		string text = string.Format("Soldats ennemis au sol : %1 sur %2 (renforts %3, défense des localités %4, postes %5, gardes %6, rondes %7)", GroundOf(soldiers), m_iMaxSoldiers, soldiers[SRP_ECmdCapClass.COMBAT], soldiers[SRP_ECmdCapClass.GARNISON], soldiers[SRP_ECmdCapClass.POSTE], soldiers[SRP_ECmdCapClass.GARDE], soldiers[SRP_ECmdCapClass.PATROUILLE]);
		if (soldiers[SRP_ECmdCapClass.HORS_COMPTE] > 0)
			text += string.Format(" · hors compte (hélico) : %1", soldiers[SRP_ECmdCapClass.HORS_COMPTE]);

		// Réserves libres (inutilisées) : renforts toujours ; postes seulement si un poste est possible
		int keepCombat = CapMax(0, m_iCombatReserve - soldiers[SRP_ECmdCapClass.COMBAT]);
		if (IsPostReserveActive())
			text += string.Format(" · réserves libres : renforts %1, postes %2", keepCombat, CapMax(0, m_iPostReserve - soldiers[SRP_ECmdCapClass.POSTE]));
		else
			text += string.Format(" · réserves libres : renforts %1, postes 0 (aucun poste à portée des joueurs)", keepCombat);

		// Demandes en attente (file RE12), par classe
		string waiting = "";
		for (int c = 0; c < SRP_ECmdCapClass.HORS_COMPTE; c++)
		{
			int pendingSoldiers = Pending(c);
			if (pendingSoldiers <= 0)
				continue;
			if (!waiting.IsEmpty())
				waiting += ", ";
			waiting += string.Format("%1 %2", ClassLabel(c), pendingSoldiers);
		}
		if (waiting.IsEmpty())
			waiting = "aucune";
		text += " · en attente : " + waiting;

		text += string.Format(" · éléments : %1 sur %2 (%3 gardés aux missions)", GroundOf(groups), m_iMaxGroups, m_iMissionGroupReserve);

		// Limite du jeu
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
		{
			text += string.Format(" · limite du jeu : %1 actifs sur %2", aiWorld.GetCurrentNumOfActiveAIs(), aiWorld.GetLimitOfActiveAIs());
			if (aiWorld.GetLimitOfActiveAIs() != m_iEngineLimit)
				text += string.Format(" (%1 voulue)", m_iEngineLimit);
		}
		else
		{
			text += string.Format(" · limite du jeu : inconnue (%1 voulue)", m_iEngineLimit);
		}
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
		{
			int queued = enemies.CountPendingSpawn();
			if (queued > 0)
				text += string.Format(", %1 soldat(s) encore attendus par la file d'apparition", queued);
		}

		// Parts des localités (C6)
		int shown = m_mGarrisonShare.Count();
		if (shown > SHARES_SHOWN)
			shown = SHARES_SHOWN;
		if (shown > 0)
		{
			string sharesText = "";
			for (int k = 0; k < shown; k++)
			{
				if (!sharesText.IsEmpty())
					sharesText += ", ";
				sharesText += string.Format("%1 %2", m_mGarrisonShare.GetKey(k), m_mGarrisonShare.GetElement(k));
			}
			if (m_mGarrisonShare.Count() > shown)
				sharesText += ", …";
			text += " · parts des localités : " + sharesText;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Dernier refus de place (« place : … »), "" sinon — SRP_CmdScreens
	string GetLastRefusal()
	{
		return m_sLastRefusal;
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Soldats des demandes encore valables d'une classe — Room, Tick
	protected int Pending(int cls)
	{
		int now = System.GetUnixTime();
		int total = 0;
		foreach (SRP_CmdDemand demand : m_aDemands)
		{
			if (demand && demand.m_iUntil >= now && demand.m_iClass == cls)
				total += demand.m_iSoldiers;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats demandés par les classes plus prioritaires que cls (ordre COMBAT, GARNISON, POSTE, GARDE, PATROUILLE) —
	//! Room
	protected int PendingAbove(int cls)
	{
		int rank = Rank(cls);
		int now = System.GetUnixTime();
		int total = 0;
		foreach (SRP_CmdDemand demand : m_aDemands)
		{
			if (demand && demand.m_iUntil >= now && Rank(demand.m_iClass) < rank)
				total += demand.m_iSoldiers;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! La réserve des postes joue : SRP_FrontEnemyComponent.IsPostPossibleNearPlayers() — Room, GroupRoom
	protected bool IsPostReserveActive()
	{
		if (m_iPostReserve <= 0 && m_iPostGroupReserve <= 0)
			return false;
		SRP_FrontEnemyComponent front = SRP_FrontEnemyComponent.GetInstance();
		if (!front)
			return false;
		return front.IsPostPossibleNearPlayers();
	}

	//------------------------------------------------------------------------------------------------
	//! Retire les demandes expirées — Tick
	protected void PruneDemands(int nowUnix)
	{
		for (int i = m_aDemands.Count() - 1; i >= 0; i--)
		{
			SRP_CmdDemand demand = m_aDemands[i];
			if (!demand || demand.m_iUntil < nowUnix)
				m_aDemands.RemoveOrdered(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La demande de clé key, null sinon — NoteDemand
	protected SRP_CmdDemand FindDemand(string key)
	{
		foreach (SRP_CmdDemand demand : m_aDemands)
		{
			if (demand && demand.m_sKey == key)
				return demand;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Place d'une classe (voir Room) ; byEngine = vrai si c'est la place du jeu qui borne — Room, Refusal
	protected int RoomOf(int cls, out bool byEngine)
	{
		byEngine = false;
		int klass = ValidClass(cls);
		if (klass == SRP_ECmdCapClass.HORS_COMPTE)
			return 1000;

		array<int> soldiers = {};
		array<int> groups = {};
		Census(soldiers, groups);

		int keepCombat = CapMax(0, m_iCombatReserve - soldiers[SRP_ECmdCapClass.COMBAT]);
		int keepPost = 0;
		if (IsPostReserveActive())
			keepPost = CapMax(0, m_iPostReserve - soldiers[SRP_ECmdCapClass.POSTE]);

		// COMBAT garde la réserve des postes ; POSTE garde celle des renforts et les demandes COMBAT ; les autres
		// gardent les deux réserves et les demandes des classes au-dessus d'elles
		int room = m_iMaxSoldiers - GroundOf(soldiers);
		if (klass == SRP_ECmdCapClass.COMBAT)
			room -= keepPost;
		else if (klass == SRP_ECmdCapClass.POSTE)
			room -= keepCombat + Pending(SRP_ECmdCapClass.COMBAT);
		else
			room -= keepCombat + keepPost + PendingAbove(klass);

		int engine = EngineRoom();
		if (engine < room)
		{
			room = engine;
			byEngine = true;
		}
		return CapMax(room, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Un seul passage sur les groupes : soldats et groupes par classe (CLASS_COUNT cases), attendus compris pendant la
	//! pose (SoldiersOf) ; groupes supprimés ignorés — comptes, place, Staff
	protected void Census(notnull array<int> soldiers, notnull array<int> groups)
	{
		soldiers.Clear();
		groups.Clear();
		for (int c = 0; c < CLASS_COUNT; c++)
		{
			soldiers.Insert(0);
			groups.Insert(0);
		}

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		array<ref SRP_EnemyGroup> records = enemies.GetGroups();
		if (!records)
			return;
		int tick = System.GetTickCount();		// SoldiersOf compte la grâce de la file en GetTickCount (non sauvé)
		foreach (SRP_EnemyGroup record : records)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			int cls = ClassOf(record);
			soldiers[cls] = soldiers[cls] + SRP_EnemyComponent.SoldiersOf(record, tick);
			groups[cls] = groups[cls] + 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Somme des classes au sol (toutes sauf HORS_COMPTE) — comptes, place, Staff
	protected static int GroundOf(notnull array<int> counts)
	{
		int total = 0;
		for (int c = 0; c < counts.Count(); c++)
		{
			if (c != SRP_ECmdCapClass.HORS_COMPTE)
				total += counts[c];
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Rang de priorité (0 = la plus haute) : COMBAT, GARNISON, POSTE, GARDE, PATROUILLE, puis HORS_COMPTE — PendingAbove
	protected static int Rank(int cls)
	{
		if (cls == SRP_ECmdCapClass.COMBAT)
			return 0;
		if (cls == SRP_ECmdCapClass.GARNISON)
			return 1;
		if (cls == SRP_ECmdCapClass.POSTE)
			return 2;
		if (cls == SRP_ECmdCapClass.GARDE)
			return 3;
		if (cls == SRP_ECmdCapClass.PATROUILLE)
			return 4;
		return 5;
	}

	//------------------------------------------------------------------------------------------------
	//! Classe ramenée dans l'énumération (inconnue -> GARNISON, la classe par défaut de ClassOf) — Tag, place
	protected static int ValidClass(int cls)
	{
		if (cls < 0 || cls >= CLASS_COUNT)
			return SRP_ECmdCapClass.GARNISON;
		return cls;
	}

	//------------------------------------------------------------------------------------------------
	//! « pour les renforts », « pour la défense des localités »… (texte des refus) — Refusal
	protected static string ClassFor(int cls)
	{
		if (cls == SRP_ECmdCapClass.GARNISON)
			return "pour la défense des localités";
		if (cls == SRP_ECmdCapClass.HORS_COMPTE)
			return "hors compte";
		return "pour les " + ClassLabel(cls);
	}

	//------------------------------------------------------------------------------------------------
	//! Poids d'une localité dans le partage (C6) : sa taille, multipliée par cap_garnison_poids_proche si un joueur est
	//! proche — SetWantedGarrisons
	protected int WeightOf(int size, bool isNear)
	{
		if (size <= 0)
			return 0;
		if (isNear)
			return size * m_iGarrisonNearWeight;
		return size;
	}

	//------------------------------------------------------------------------------------------------
	//! Plafond d'éléments de SRP_EnemyComponent (cap_groupes_max, cap_groupes_reserve_missions) — Start, LoadSettings
	protected void ApplyGroupCap()
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
		{
			CapError("plafond d'éléments non posé : composant ennemi absent");
			return;
		}
		enemies.SetMaxGroups(m_iMaxGroups, m_iMissionGroupReserve);
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôle de cohérence des réglages (Q7) : soldats + civils + pilotes + marge sous la limite du jeu ; réserves
	//! sous les plafonds ; ERREUR au journal sinon ; vrai si tout va — Start, LoadSettings
	protected bool CheckCoherence()
	{
		bool ok = true;
		int total = m_iMaxSoldiers + m_iPlannedCivilians + m_iPlannedPilots + m_iEngineMargin;
		if (total > m_iEngineLimit)
		{
			CapError(string.Format("réglages incohérents : %1 soldats + %2 civils + %3 pilotes + %4 de marge = %5, au-dessus de la limite du jeu (%6) ; baisser cap_soldats_max ou relever cap_limite_jeu", m_iMaxSoldiers, m_iPlannedCivilians, m_iPlannedPilots, m_iEngineMargin, total, m_iEngineLimit));
			ok = false;
		}
		if (m_iCombatReserve + m_iPostReserve >= m_iMaxSoldiers)
		{
			CapError(string.Format("réglages incohérents : places gardées aux renforts (%1) et aux postes (%2) pour %3 soldats au plus", m_iCombatReserve, m_iPostReserve, m_iMaxSoldiers));
			ok = false;
		}
		if (m_iMissionGroupReserve + m_iCombatGroupReserve + m_iPostGroupReserve >= m_iMaxGroups)
		{
			CapError(string.Format("réglages incohérents : éléments gardés aux missions (%1), aux renforts (%2) et aux postes (%3) pour %4 éléments au plus", m_iMissionGroupReserve, m_iCombatGroupReserve, m_iPostGroupReserve, m_iMaxGroups));
			ok = false;
		}
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne du journal du Commandeur (SYSTEME) — interne
	protected static void CapNote(string text)
	{
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Place des soldats ennemis : " + text);
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne ERREUR : console du serveur et journal du Commandeur (SYSTEME) — interne
	protected static void CapError(string text)
	{
		Print("[SRP] Place des soldats ennemis, ERREUR : " + text, LogLevel.ERROR);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "ERREUR, place des soldats ennemis : " + text);
	}

	//------------------------------------------------------------------------------------------------
	protected static int CapMin(int a, int b)
	{
		if (a < b)
			return a;
		return b;
	}

	//------------------------------------------------------------------------------------------------
	protected static int CapMax(int a, int b)
	{
		if (a > b)
			return a;
		return b;
	}
}
