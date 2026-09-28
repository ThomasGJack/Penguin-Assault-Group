//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : LES BLINDÉS (AP4 à AP6, C3, EQ3, EQ4) et le verrou d'entrée du 2S1 (AP1).
//
// RÔLE (serveur ; SRP_CmdArmorManager est tenu par SRP_CmdSupport.m_Armor) :
// - trois types lus dans les réglages appui_blinde_1_* à appui_blinde_3_* : BTR-70 (poids 45, prime 2 000 €), BRDM-2
//   (45, 2 000 €), Typhoon 30 mm (10, 5 000 €) ; garde AP4 : un prefab T14, K17, Kurganets ou T90 est refusé à la
//   lecture (ligne de journal, type écarté du tirage) ;
// - lancement (SRP_CmdSupport.RequestArmor, contrôles faits ; Staff par ForceOrder) : départ hors de vue côté rouge à
//   appui_blinde_depart_min_m..max_m, halte à appui_blinde_halte_m de la cible (rabattue sur une route proche),
//   place par SRP_CmdCapacity.Ask (COMBAT « blinde »), ticket Reserve(BLINDE) (gratuit pour le Staff si
//   cmd_staff_gratuit), équipage de appui_blinde_equipage (SRP_EnemyComponent.SpawnVehicleCrew, Tag COMBAT, rôle
//   EQUIPAGE), OrderMove vers la halte ;
// - cycle (Tick, 5 s) : ROUTE -> ENGAGE (halte atteinte, contact vu à 500 m, ou bloqué sous les yeux des joueurs) ->
//   REPLI (fin de la contre-attaque, plus aucun joueur à appui_blinde_fin_sans_joueur_m depuis
//   appui_blinde_fin_sans_joueur_min, ou appui_blinde_duree_max_min d'engagement) -> retiré hors de vue (Release :
//   rentré au stock ; après appui_blinde_duree_max_min de repli, même avec des joueurs à moins de
//   appui_blinde_retrait_m) ; bloqué en route hors de vue : retiré (Release) ; DETRUIT (Consume, épave au nettoyage des
//   transports) ; PRENABLE (équipage hors de combat : Consume, survivants retirés hors de vue) -> PRIS (joueur à bord)
//   -> LIVRE (ramené à appui_blinde_base_rayon_m du repère de la base : prime SRP_TreasuryComponent.Add, radio
//   SRP_FrontRadio.ArmorBroughtBack, évènement blinde_ramene), puis retiré après appui_blinde_vide_s sans personne à
//   bord. C3 : un blindé pris non ramené disparaît au redémarrage (rien n'est sauvé, H3).
// Journal (§1.8) : Launch rend "" ou la raison et n'écrit que sa ligne d'exécution (SRP_CmdLog.Note, APPUI) ; la
// décision ou le refus est écrit par l'appelant (IssueOrder, LogSupport) ; puis une Note (APPUI) à chaque étape.
// En fin de fichier : modded SCR_GetInUserAction, verrou d'entrée de la batterie 2S1 (AP1), toutes machines (texte
// exact de CONTRATS_CMD.md §8 ; le test du prefab est SRP_CmdSupport.IsBatteryVehicle).
// APPELÉ PAR : SRP_CmdSupport (Launch, Tick, EndAssault, rapports, réglages, CancelAll), SRP_CmdScreens (types pour le
// Staff, texte « engagé »).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Un type de blindé (réglages appui_blinde_N_*)
class SRP_CmdArmorType
{
	string m_sName;								// « BTR-70 »
	ResourceName m_sPrefab;
	int m_iWeight;								// poids du tirage (AP4)
	int m_iBounty;								// prime en euros s'il est ramené (C3)
	bool m_bAllowed = true;						// faux : prefab refusé par la garde AP4
}

//------------------------------------------------------------------------------------------------
//! Un blindé engagé. Non sauvé (C3, H3).
class SRP_CmdArmor
{
	IEntity m_Vehicle;
	ref SRP_EnemyGroup m_Crew;
	ref SRP_CmdArmorType m_Type;
	int m_iTicket;								// ticket BLINDE de SRP_CmdResources (0 = gratuit)
	int m_iReason = SRP_ECmdArmorReason.RENFORT;
	int m_iZone = -1;
	int m_iRegion = -1;
	vector m_vTarget;
	vector m_vSpawn;
	vector m_vHalt;
	vector m_vLastPos;
	int m_iState = SRP_ECmdArmorState.ROUTE;
	int m_iStateUnix;
	int m_iLastMoveUnix;
	int m_iNoPlayerSinceUnix;
	int m_iEmptySinceUnix;
	int m_iRetargetUnix;
	bool m_bPaid;
	bool m_bStaff;
	int m_iLaunchUnix;							// départ (l'équipage n'est jugé qu'après CREW_GRACE_S)
	int m_iAboardUnix;							// dernière fois qu'un joueur était à bord (prise, livraison)
	int m_iAboardPlayer;						// ce joueur (journal)
}

//------------------------------------------------------------------------------------------------
//! Les blindés du Commandeur (serveur).
class SRP_CmdArmorManager
{
	// --- Réglages (clés appui_blinde_*) --------------------------------------------------------------------------
	int m_iArmorPlayers = 6;		// appui_blinde_seuil_joueurs
	int m_iArmorAtOnce = 1;		// appui_blinde_simultanes
	string m_sType1Name = "BTR-70";		// appui_blinde_1_nom
	string m_sType1Prefab = "{447CFE8D73F95C3E}Prefabs/Vehicles/Wheeled/BTR70/BTR70_AFRF.et";		// appui_blinde_1_prefab
	int m_iType1Weight = 45;		// appui_blinde_1_poids
	int m_iType1Bounty = 2000;		// appui_blinde_1_prime
	string m_sType2Name = "BRDM-2";		// appui_blinde_2_nom
	string m_sType2Prefab = "{9AEDF323A812F9ED}Prefabs/Vehicles/Wheeled/BRDM2/BRDM2_AFRF.et";		// appui_blinde_2_prefab
	int m_iType2Weight = 45;		// appui_blinde_2_poids
	int m_iType2Bounty = 2000;		// appui_blinde_2_prime
	string m_sType3Name = "Typhoon";		// appui_blinde_3_nom
	string m_sType3Prefab = "{AB5DE68E3E654FCA}Prefabs/Vehicles/Wheeled/K4386/K4386_Armed.et";		// appui_blinde_3_prefab
	int m_iType3Weight = 10;		// appui_blinde_3_poids
	int m_iType3Bounty = 5000;		// appui_blinde_3_prime
	int m_iCrewSize = 2;		// appui_blinde_equipage
	string m_sCrewPrefab = "{0848BAB1B94FCBBA}Prefabs/Characters/Factions/OPFOR/RHS_AFRF/MSV/VKPO_Demiseason/Character_RHS_RF_MSV_VKPO_DS_Crew.et";		// appui_blinde_equipage_prefab
	string m_sFaction = "";		// appui_blinde_faction
	int m_iSpawnMinM = 1500;		// appui_blinde_depart_min_m
	int m_iSpawnMaxM = 2500;		// appui_blinde_depart_max_m
	int m_iHaltM = 350;		// appui_blinde_halte_m
	int m_iEngageMaxMin = 25;		// appui_blinde_duree_max_min
	int m_iNoPlayerM = 800;		// appui_blinde_fin_sans_joueur_m
	int m_iNoPlayerMin = 5;		// appui_blinde_fin_sans_joueur_min
	int m_iStuckS = 180;		// appui_blinde_bloque_s
	int m_iRetireM = 1200;		// appui_blinde_retrait_m
	int m_iBaseRadiusM = 300;		// appui_blinde_base_rayon_m
	int m_iEmptyS = 10;		// appui_blinde_vide_s

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected SRP_CmdSupport m_Support;
	protected ref array<ref SRP_CmdArmorType> m_aTypes = {};		// refait par LoadSettings (3 types)
	protected ref array<ref SRP_CmdArmor> m_aArmors = {};
	protected ref array<IEntity> m_aPlayers = {};					// personnages des joueurs, relus à chaque passage
	protected ref array<ref SRP_CmdContact> m_aContacts = {};		// contacts du cerveau, relus à chaque passage puis vidés

	// --- Constantes du cycle (règles AP5 écrites dans le plan, pas des réglages) ---------------------------------
	protected static const int START_TRIES = 6;					// tirages d'un départ hors de vue avant de renoncer
	protected static const float SPAWN_LIFT_M = 0.5;				// véhicule posé un peu au-dessus du sol
	protected static const float HALT_REACHED_M = 60;				// halte atteinte à … m
	protected static const float HALT_ROAD_SNAP_M = 150;			// halte rabattue sur une route à … m au plus
	protected static const float MOVE_STEP_M = 15;					// « avance » : au moins … m depuis le dernier relevé
	protected static const float CONTACT_ENGAGE_M = 500;			// en route : combat dès un contact vu à … m
	protected static const float RETARGET_M = 600;					// au combat : cible recalée sur un contact vu à … m
	protected static const float RETARGET_MOVE_M = 50;				// … si le contact a bougé d'au moins … m
	protected static const int RETARGET_S = 60;					// recalage toutes les … s
	protected static const int CREW_GRACE_S = 15;					// équipage jugé … s après le départ
	protected static const int ABOARD_GRACE_S = 15;				// livraison : joueur descendu depuis … s au plus

	//------------------------------------------------------------------------------------------------
	//! Constructeur seul — SRP_CmdSupport (constructeur)
	void SRP_CmdArmorManager(SRP_CmdSupport support)
	{
		m_Support = support;
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés — SRP_CmdSupport.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("appui_blinde_seuil_joueurs", m_iArmorPlayers, "Blindé dès ce nombre de joueurs à 3 km (EQ4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_simultanes", m_iArmorAtOnce, "Blindés engagés en même temps, au plus (EQ3)");
		SRP_CmdSettings.DeclareString("appui_blinde_1_nom", m_sType1Name, "Blindé 1 : nom (AP4)");
		SRP_CmdSettings.DeclareString("appui_blinde_1_prefab", m_sType1Prefab, "Blindé 1 : prefab (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_1_poids", m_iType1Weight, "Blindé 1 : poids du tirage (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_1_prime", m_iType1Bounty, "Blindé 1 : prime s'il est ramené à la base, en euros (C3)");
		SRP_CmdSettings.DeclareString("appui_blinde_2_nom", m_sType2Name, "Blindé 2 : nom (AP4)");
		SRP_CmdSettings.DeclareString("appui_blinde_2_prefab", m_sType2Prefab, "Blindé 2 : prefab (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_2_poids", m_iType2Weight, "Blindé 2 : poids du tirage (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_2_prime", m_iType2Bounty, "Blindé 2 : prime s'il est ramené à la base, en euros (C3)");
		SRP_CmdSettings.DeclareString("appui_blinde_3_nom", m_sType3Name, "Blindé 3 : nom (AP4)");
		SRP_CmdSettings.DeclareString("appui_blinde_3_prefab", m_sType3Prefab, "Blindé 3 : prefab (30 mm, très rare) (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_3_poids", m_iType3Weight, "Blindé 3 : poids du tirage (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_3_prime", m_iType3Bounty, "Blindé 3 : prime s'il est ramené à la base, en euros (C3)");
		SRP_CmdSettings.DeclareInt("appui_blinde_equipage", m_iCrewSize, "Équipage d'un blindé (AP4)");
		SRP_CmdSettings.DeclareString("appui_blinde_equipage_prefab", m_sCrewPrefab, "Membre d'équipage (AP4)");
		SRP_CmdSettings.DeclareString("appui_blinde_faction", m_sFaction, "Clé de faction du blindé (vide = celle de l'ennemi du mod) (AP4)");
		SRP_CmdSettings.DeclareInt("appui_blinde_depart_min_m", m_iSpawnMinM, "Départ à cette distance au moins de la cible, en mètres (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_depart_max_m", m_iSpawnMaxM, "Départ à cette distance au plus, en mètres (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_halte_m", m_iHaltM, "Halte à cette distance de la cible, en mètres (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_duree_max_min", m_iEngageMaxMin, "Engagement au plus, puis repli au plus avant retrait hors de vue, en minutes (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_fin_sans_joueur_m", m_iNoPlayerM, "Repli quand plus aucun joueur n'est à cette distance, en mètres (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_fin_sans_joueur_min", m_iNoPlayerMin, "Depuis ce nombre de minutes (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_bloque_s", m_iStuckS, "Blindé bloqué après ce nombre de secondes sans avancer de 15 m (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_retrait_m", m_iRetireM, "Retiré en repli quand plus aucun joueur n'est à cette distance, hors de vue, en mètres (AP5)");
		SRP_CmdSettings.DeclareInt("appui_blinde_base_rayon_m", m_iBaseRadiusM, "Blindé pris livré à cette distance du repère de la base, en mètres (C3)");
		SRP_CmdSettings.DeclareInt("appui_blinde_vide_s", m_iEmptyS, "Blindé livré retiré après ce nombre de secondes vide (C3)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés puis refait m_aTypes (garde AP4 à la lecture, ligne de journal par prefab refusé) —
	//! SRP_CmdSupport.LoadSettings
	void LoadSettings()
	{
		m_iArmorPlayers = SRP_CmdSettings.GetIntClamped("appui_blinde_seuil_joueurs", 1, 50);
		m_iArmorAtOnce = SRP_CmdSettings.GetIntClamped("appui_blinde_simultanes", 0, 5);
		m_sType1Name = SRP_CmdSettings.GetString("appui_blinde_1_nom");
		m_sType1Prefab = SRP_CmdSettings.GetString("appui_blinde_1_prefab");
		m_iType1Weight = SRP_CmdSettings.GetIntClamped("appui_blinde_1_poids", 0, 1000);
		m_iType1Bounty = SRP_CmdSettings.GetIntClamped("appui_blinde_1_prime", 0, 100000);
		m_sType2Name = SRP_CmdSettings.GetString("appui_blinde_2_nom");
		m_sType2Prefab = SRP_CmdSettings.GetString("appui_blinde_2_prefab");
		m_iType2Weight = SRP_CmdSettings.GetIntClamped("appui_blinde_2_poids", 0, 1000);
		m_iType2Bounty = SRP_CmdSettings.GetIntClamped("appui_blinde_2_prime", 0, 100000);
		m_sType3Name = SRP_CmdSettings.GetString("appui_blinde_3_nom");
		m_sType3Prefab = SRP_CmdSettings.GetString("appui_blinde_3_prefab");
		m_iType3Weight = SRP_CmdSettings.GetIntClamped("appui_blinde_3_poids", 0, 1000);
		m_iType3Bounty = SRP_CmdSettings.GetIntClamped("appui_blinde_3_prime", 0, 100000);
		m_iCrewSize = SRP_CmdSettings.GetIntClamped("appui_blinde_equipage", 1, 6);
		m_sCrewPrefab = SRP_CmdSettings.GetString("appui_blinde_equipage_prefab");
		m_sFaction = SRP_CmdSettings.GetString("appui_blinde_faction");
		m_iSpawnMinM = SRP_CmdSettings.GetIntClamped("appui_blinde_depart_min_m", 200, 10000);
		m_iSpawnMaxM = SRP_CmdSettings.GetIntClamped("appui_blinde_depart_max_m", 200, 10000);
		m_iHaltM = SRP_CmdSettings.GetIntClamped("appui_blinde_halte_m", 0, 3000);
		m_iEngageMaxMin = SRP_CmdSettings.GetIntClamped("appui_blinde_duree_max_min", 1, 240);
		m_iNoPlayerM = SRP_CmdSettings.GetIntClamped("appui_blinde_fin_sans_joueur_m", 100, 5000);
		m_iNoPlayerMin = SRP_CmdSettings.GetIntClamped("appui_blinde_fin_sans_joueur_min", 0, 120);
		m_iStuckS = SRP_CmdSettings.GetIntClamped("appui_blinde_bloque_s", 10, 1800);
		m_iRetireM = SRP_CmdSettings.GetIntClamped("appui_blinde_retrait_m", 100, 10000);
		m_iBaseRadiusM = SRP_CmdSettings.GetIntClamped("appui_blinde_base_rayon_m", 50, 2000);
		m_iEmptyS = SRP_CmdSettings.GetIntClamped("appui_blinde_vide_s", 0, 600);

		// Fourchette de départ remise dans l'ordre (un maximum sous le minimum vaut le minimum)
		if (m_iSpawnMaxM < m_iSpawnMinM)
			m_iSpawnMaxM = m_iSpawnMinM;
		RebuildTypes();
	}

	//------------------------------------------------------------------------------------------------
	//! Lance un blindé (contrôles déjà faits par RequestArmor, ou Staff) : type (variant, sinon tirage par poids),
	//! départ, halte, véhicule, équipage (Ask COMBAT « blinde », Tag), Reserve(BLINDE) hors Staff gratuit, OrderMove ;
	//! journal ; rend "" ou la raison — SRP_CmdSupport.RequestArmor
	//! Journal (§1.8) : ici seulement la ligne d'exécution (SRP_CmdLog.Note, APPUI) ; la décision ou le refus est écrit par
	//! l'appelant (IssueOrder du cerveau, LogSupport des manœuvres).
	string Launch(int region, vector target, int reason, int zone, int variant, bool staff)
	{
		string what = "blindé";
		SRP_Commander commander = SRP_Commander.Get();
		SRP_CmdResources resources = SRP_CmdResources.Get();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!commander || !resources || !enemies || !front)
			return Refused(region, what, "Commandeur, ennemi ou front pas prêt");

		target = SRP_Placement.OnGround(target);
		int dosing = commander.GetDosingPlayers(target);

		// Plafonds propres aux blindés (EQ3, EQ4), déjà vus par RequestArmor et gardés ici par sûreté ; le Staff
		// passe outre (VU5)
		if (!staff)
		{
			int active = CountActive();
			if (active >= m_iArmorAtOnce)
				return Refused(region, what, string.Format("plafond : %1 blindé(s) déjà engagé(s), %2 à la fois", active, m_iArmorAtOnce));
			if (dosing < m_iArmorPlayers)
				return Refused(region, what, string.Format("seuil : %1 joueur(s) à 3 km, il en faut %2", dosing, m_iArmorPlayers));
		}

		// Sécurité, Staff compris : jamais en mer ni sur la base
		string unsafeWhy = TargetRefusal(front, target);
		if (!unsafeWhy.IsEmpty())
			return Refused(region, what, unsafeWhy);

		SRP_CmdArmorType armorType = PickType(variant);
		if (!armorType)
		{
			if (variant >= 0)
				return Refused(region, what, string.Format("type %1 absent ou refusé dans les réglages (AP4)", variant + 1));
			return Refused(region, what, "aucun type permis dans les réglages (AP4)");
		}

		if (zone < 0)
			zone = front.GetZoneAt(target);
		if (region < 0 && zone >= 0)
			region = commander.GetRegionOfZone(zone);
		string name = armorType.m_sName;
		string place = PlaceOf(front, zone, target);
		what = "blindé " + name + " vers " + place;

		// Stock (C1) vu d'abord, sans rien retirer : ni pose cherchée ni place demandée pour un stock vide (le Staff ne
		// paie rien si cmd_staff_gratuit)
		bool staffFree = staff && commander.m_bStaffFree;
		string stockWhy;
		if (!staffFree && !resources.CanReserve(SRP_ECmdStock.BLINDE, -1, 1, stockWhy))
			return Refused(region, what, "stock : " + stockWhy);

		// Départ hors de vue sur un carré rouge, puis halte à appui_blinde_halte_m de la cible
		m_aPlayers = SRP_Utils.GetPlayerCharacters();
		vector start;
		if (!FindStart(front, target, start))
			return Refused(region, what, string.Format("aucun départ hors de vue sur un carré rouge entre %1 et %2 m", m_iSpawnMinM, m_iSpawnMaxM));
		vector halt = FindHalt(target, start);

		// Place (Q7) : l'équipage compte dans les 120 soldats, classe COMBAT (file RE12 si refus)
		string roomWhy = SRP_CmdCapacity.Get().Ask("blinde", SRP_ECmdCapClass.COMBAT, m_iCrewSize, 1, start, "appui");
		if (!roomWhy.IsEmpty())
			return Refused(region, what, roomWhy);

		// Stock (C1) : un blindé engagé (ticket gratuit pour le Staff si cmd_staff_gratuit ; un refus ici est écrit par
		// Reserve elle-même)
		int ticket = resources.Reserve(SRP_ECmdStock.BLINDE, -1, 1, what, staff);
		if (ticket <= 0)
			return "stock : plus de blindé disponible (" + resources.StockText(SRP_ECmdStock.BLINDE, -1) + ")";

		// Véhicule tourné vers la halte, puis équipage assis (il reste à bord : SetForceStayInVehicle)
		IEntity vehicle = SpawnVehicle(armorType.m_sPrefab, start, halt);
		if (!vehicle)
		{
			resources.Release(ticket, what + " : pose impossible");
			return Refused(region, what, "pose impossible du prefab (appui_blinde_N_prefab)");
		}
		SRP_EnemyGroup crew = enemies.SpawnVehicleCrew(m_sCrewPrefab, vehicle, m_iCrewSize, "appui");
		if (!crew)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			resources.Release(ticket, what + " : équipage impossible");
			return Refused(region, what, "équipage impossible (appui_blinde_equipage_prefab, places à bord)");
		}
		SRP_CmdCapacity.Tag(crew, SRP_ECmdCapClass.COMBAT);
		crew.m_iCmdRole = SRP_ECmdRole.EQUIPAGE;
		ApplyFaction(crew, vehicle);
		enemies.OrderMove(crew, halt);

		int nowUnix = System.GetUnixTime();
		SRP_CmdArmor armor = new SRP_CmdArmor();
		armor.m_Vehicle = vehicle;
		armor.m_Crew = crew;
		armor.m_Type = armorType;
		armor.m_iTicket = ticket;
		armor.m_iReason = reason;
		armor.m_iZone = zone;
		armor.m_iRegion = region;
		armor.m_vTarget = target;
		armor.m_vSpawn = start;
		armor.m_vHalt = halt;
		armor.m_vLastPos = start;
		armor.m_iState = SRP_ECmdArmorState.ROUTE;
		armor.m_iStateUnix = nowUnix;
		armor.m_iLastMoveUnix = nowUnix;
		armor.m_iLaunchUnix = nowUnix;
		armor.m_bStaff = staff;
		m_aArmors.Insert(armor);

		// Journal (VU4) : « Blindé BTR-70 vers Régina (S07) — renfort, 7 joueurs à 3 km, départ … — coût : 1 blindé —
		// reste : 1/2 blindés »
		int startM = Math.Round(vector.DistanceXZ(start, target));
		int haltM = Math.Round(vector.DistanceXZ(halt, target));
		string detail = string.Format("%1, %2 joueur(s) à 3 km, départ à %3 m (carré %4), halte à %5 m, %6 d'équipage", ReasonWord(reason), dosing, startM, SRP_CmdScreens.CellText(start), haltM, SRP_EnemyComponent.AliveAgents(crew));
		if (staff)
			detail += ", forcé par le Staff";
		string cost = "1 blindé";
		if (staffFree)
			cost = "aucun (Staff)";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, "Blindé " + name + " vers " + place + " — " + detail + ", coût : " + cost);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Refus de Launch : rend why sans rien écrire (l'appelant écrit SRP_CmdLog.Refusal, §1.8)
	protected string Refused(int region, string what, string why)
	{
		return why;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s : cycle de vie de chaque blindé (voir l'en-tête) — SRP_CmdSupport.Tick
	void Tick(int nowUnix)
	{
		if (m_aArmors.IsEmpty())
			return;
		m_aPlayers = SRP_Utils.GetPlayerCharacters();
		RefreshContacts();
		for (int i = m_aArmors.Count() - 1; i >= 0; i--)
		{
			SRP_CmdArmor armor = m_aArmors[i];
			if (armor)
				StepArmor(armor, nowUnix);
			// Fini (rentré, détruit, livré puis retiré, disparu) : plus suivi
			if (!armor || !armor.m_Vehicle)
				m_aArmors.RemoveOrdered(i);
		}
		m_aContacts.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Blindés actifs (ROUTE, ENGAGE, REPLI) : EQ3 « un à la fois » (appui_blinde_simultanes) — SRP_CmdSupport
	int CountActive()
	{
		int count = 0;
		foreach (SRP_CmdArmor armor : m_aArmors)
		{
			if (armor && IsActiveState(armor.m_iState))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Contre-attaque finie : le blindé CONTRE_ATTAQUE de la zone passe en REPLI — SRP_CmdSupport.EndAssault
	void EndAssault(int zone)
	{
		if (zone < 0)
			return;
		int nowUnix = System.GetUnixTime();
		foreach (SRP_CmdArmor armor : m_aArmors)
		{
			if (!armor || armor.m_iReason != SRP_ECmdArmorReason.CONTRE_ATTAQUE || armor.m_iZone != zone)
				continue;
			if (armor.m_iState != SRP_ECmdArmorState.ROUTE && armor.m_iState != SRP_ECmdArmorState.ENGAGE)
				continue;
			if (!armor.m_Vehicle || armor.m_Vehicle.IsDeleted())
				continue;
			StartRetreat(armor, nowUnix, "fin de la contre-attaque");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! « engagé : BTR-70 vers Régina (S07) » ou « aucun » — SRP_CmdScreens (page principale)
	string GetEngagedText()
	{
		string text = "";
		int taken = 0;
		int takeable = 0;
		foreach (SRP_CmdArmor armor : m_aArmors)
		{
			if (!armor)
				continue;
			if (!IsActiveState(armor.m_iState))
			{
				// PRIS : un joueur est monté dedans ; PRENABLE : équipage hors de combat, personne à bord
				if (armor.m_iState == SRP_ECmdArmorState.PRIS)
					taken++;
				else if (armor.m_iState == SRP_ECmdArmorState.PRENABLE)
					takeable++;
				continue;
			}
			if (!text.IsEmpty())
				text += " · ";
			if (armor.m_iState == SRP_ECmdArmorState.REPLI)
				text += NameOf(armor) + " en repli";
			else
				text += NameOf(armor) + " vers " + PlaceText(armor);
		}
		if (text.IsEmpty())
			text = "aucun";
		else
			text = "engagé : " + text;
		if (taken > 0 && takeable > 0)
			text += string.Format(" (%1 aux mains des joueurs, %2 prenable(s) sans personne à bord)", taken, takeable);
		else if (taken > 0)
			text += string.Format(" (%1 aux mains des joueurs)", taken);
		else if (takeable > 0)
			text += string.Format(" (%1 prenable(s) sans personne à bord)", takeable);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes Staff : un blindé par ligne (type, état, carré, raison, depuis) — SRP_CmdSupport.GetReport
	//! Texte brut, sans préfixe du protocole des menus (l'appelant le met en forme) ; les lignes sont AJOUTÉES.
	void Report(notnull array<string> lines)
	{
		string types = "";
		foreach (SRP_CmdArmorType armorType : m_aTypes)
		{
			if (!armorType)
				continue;
			if (!types.IsEmpty())
				types += ", ";
			if (armorType.m_bAllowed)
				types += string.Format("%1 (poids %2, prime %3 €)", armorType.m_sName, armorType.m_iWeight, armorType.m_iBounty);
			else
				types += string.Format("%1 (REFUSÉ : prefab interdit ou vide)", armorType.m_sName);
		}
		if (types.IsEmpty())
			types = "aucun (réglages pas encore lus)";
		lines.Insert(string.Format("Blindés : %1 à la fois au plus, dès %2 joueurs à 3 km · types : %3", m_iArmorAtOnce, m_iArmorPlayers, types));

		if (m_aArmors.IsEmpty())
		{
			lines.Insert("Blindés en jeu : aucun");
			return;
		}
		int nowUnix = System.GetUnixTime();
		foreach (SRP_CmdArmor armor : m_aArmors)
		{
			if (!armor)
				continue;
			vector position = armor.m_vLastPos;
			if (armor.m_Vehicle && !armor.m_Vehicle.IsDeleted())
				position = armor.m_Vehicle.GetOrigin();
			string line = string.Format("Blindé %1 : %2 · carré %3 · %4, vers %5 · depuis %6", NameOf(armor), StateWord(armor.m_iState), SRP_CmdScreens.CellText(position), ReasonWord(armor.m_iReason), PlaceText(armor), SRP_CmdScreens.DurationText(nowUnix - armor.m_iStateUnix));
			if (IsActiveState(armor.m_iState))
				line += string.Format(" · équipage : %1 en état de combattre", CountCrewFit(armor));
			if (armor.m_bStaff)
				line += " · forcé par le Staff";
			if (armor.m_bPaid)
				line += " · prime versée";
			lines.Insert(line);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Blindés autour d'un point — SRP_CmdSupport.GetNearReport
	//! « Blindés : BTR-70 au combat à 850 m (carré 074 042, renfort, équipage 2) · … », "" s'il n'y en a aucun
	string GetNearReport(vector from, float radius)
	{
		string text = "";
		foreach (SRP_CmdArmor armor : m_aArmors)
		{
			if (!armor || !armor.m_Vehicle || armor.m_Vehicle.IsDeleted())
				continue;
			vector position = armor.m_Vehicle.GetOrigin();
			float distance = vector.DistanceXZ(from, position);
			if (distance > radius)
				continue;
			if (!text.IsEmpty())
				text += " · ";
			int meters = Math.Round(distance);
			text += string.Format("%1 %2 à %3 m (carré %4, %5, équipage %6)", NameOf(armor), StateWord(armor.m_iState), meters, SRP_CmdScreens.CellText(position), ReasonWord(armor.m_iReason), CountCrewFit(armor));
		}
		if (text.IsEmpty())
			return "";
		return "Blindés : " + text;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de types lus — Staff
	int GetTypeCount()
	{
		return m_aTypes.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Type d'index donné, null hors bornes — Staff
	SRP_CmdArmorType GetType(int index)
	{
		if (index < 0 || index >= m_aTypes.Count())
			return null;
		return m_aTypes[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Type désigné par un mot du Staff (« btr », « brdm », « typhoon », casse ignorée), -1 sinon —
	//! SRP_CmdScreens (forcer)
	//! -1 aussi pour un type refusé par la garde AP4 (prefab vide ou interdit) : l'écran renvoie alors aux réglages
	//! appui_blinde_* au lieu de lancer un ordre que Launch refuserait.
	int FindTypeByWord(string word)
	{
		int index = FindTypeIndex(word);
		if (index < 0 || !m_aTypes[index] || !m_aTypes[index].m_bAllowed)
			return -1;
		return index;
	}

	//------------------------------------------------------------------------------------------------
	//! Index du type désigné par un mot (numéro, nom exact, début du nom, chemin du prefab), permis ou non ; -1 sinon
	//! — FindTypeByWord
	protected int FindTypeIndex(string word)
	{
		string wanted = NormalizeWord(word);
		if (wanted.IsEmpty())
			return -1;

		// Numéro du type (« 1 » à « 3 »)
		int number = wanted.ToInt();
		if (number >= 1 && number <= m_aTypes.Count() && wanted == number.ToString())
			return number - 1;

		// Nom exact, puis début du nom (« btr » pour BTR-70), puis chemin du prefab (« k4386 »)
		for (int i = 0; i < m_aTypes.Count(); i++)
		{
			if (!m_aTypes[i])
				continue;
			string exactName = NormalizeWord(m_aTypes[i].m_sName);
			if (exactName == wanted)
				return i;
		}
		for (int j = 0; j < m_aTypes.Count(); j++)
		{
			if (!m_aTypes[j])
				continue;
			string startName = NormalizeWord(m_aTypes[j].m_sName);
			if (startName.StartsWith(wanted))
				return j;
		}
		for (int k = 0; k < m_aTypes.Count(); k++)
		{
			if (!m_aTypes[k])
				continue;
			string path = m_aTypes[k].m_sPrefab;
			path.ToLower();
			if (path.Contains(wanted))
				return k;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! ia-reset, wipe, nouvelle campagne : équipages et véhicules retirés, tickets rendus (sauf un blindé PRIS ou
	//! LIVRE avec un joueur à bord, jamais supprimé) — SRP_CmdSupport.CancelAll
	void DeleteAll()
	{
		int kept = 0;
		int removed = 0;
		for (int i = m_aArmors.Count() - 1; i >= 0; i--)
		{
			SRP_CmdArmor armor = m_aArmors[i];
			if (!armor)
			{
				m_aArmors.RemoveOrdered(i);
				continue;
			}
			IEntity vehicle = armor.m_Vehicle;
			bool present = vehicle && !vehicle.IsDeleted();
			// Jamais supprimé avec un joueur à bord : il reste suivi (prime s'il est ramené, retrait une fois vide)
			if (present && PlayerAboard(vehicle) > 0)
			{
				kept++;
				continue;
			}
			DeleteCrew(armor);
			if (present)
				SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
			armor.m_Vehicle = null;
			ReleaseTicket(armor, "blindé " + NameOf(armor) + " retiré (remise à plat)");
			m_aArmors.RemoveOrdered(i);
			removed++;
		}
		m_aContacts.Clear();
		if (removed > 0 || kept > 0)
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.APPUI, string.Format("Blindés : %1 retiré(s), %2 gardé(s) avec un joueur à bord", removed, kept));
	}

	//------------------------------------------------------------------------------------------------
	//! Type tiré selon les poids (types permis), ou celui de variant — Launch
	protected SRP_CmdArmorType PickType(int variant)
	{
		if (variant >= 0)
		{
			if (variant >= m_aTypes.Count())
				return null;
			SRP_CmdArmorType chosen = m_aTypes[variant];
			if (!chosen || !chosen.m_bAllowed)
				return null;
			return chosen;
		}

		int total = 0;
		foreach (SRP_CmdArmorType weighed : m_aTypes)
		{
			if (weighed && weighed.m_bAllowed && weighed.m_iWeight > 0)
				total += weighed.m_iWeight;
		}
		if (total <= 0)
			return null;
		int roll = Math.RandomInt(0, total);
		foreach (SRP_CmdArmorType candidate : m_aTypes)
		{
			if (!candidate || !candidate.m_bAllowed || candidate.m_iWeight <= 0)
				continue;
			if (roll < candidate.m_iWeight)
				return candidate;
			roll -= candidate.m_iWeight;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Un pas du cycle d'un blindé — Tick
	protected void StepArmor(SRP_CmdArmor armor, int nowUnix)
	{
		IEntity vehicle = armor.m_Vehicle;
		bool active = IsActiveState(armor.m_iState);

		// Véhicule disparu (supprimé hors de ce module) : compté perdu s'il était encore engagé
		if (!vehicle || vehicle.IsDeleted())
		{
			if (active)
			{
				ConsumeTicket(armor);
				ArmorNote(armor, string.Format("Blindé %1 : véhicule disparu, compté perdu", NameOf(armor)));
			}
			RetireCrew(armor);
			armor.m_Vehicle = null;
			return;
		}
		vector position = vehicle.GetOrigin();

		// Détruit : perdu pour de bon (Consume s'il était encore engagé), l'épave va au nettoyage des transports
		if (IsDestroyed(vehicle))
		{
			if (active)
				ConsumeTicket(armor);
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			if (enemies)
				enemies.RegisterTransport(vehicle);
			RetireCrew(armor);
			SetState(armor, SRP_ECmdArmorState.DETRUIT, nowUnix);
			ArmorNote(armor, string.Format("Blindé %1 détruit près du carré %2", NameOf(armor), SRP_CmdScreens.CellText(position)));
			armor.m_Vehicle = null;
			return;
		}

		// Livré : retiré une fois vide
		if (armor.m_iState == SRP_ECmdArmorState.LIVRE)
		{
			StepDelivered(armor, vehicle, nowUnix);
			return;
		}

		int playerId = PlayerAboard(vehicle);
		if (playerId > 0)
		{
			armor.m_iAboardUnix = nowUnix;
			armor.m_iAboardPlayer = playerId;
		}

		// Engagé : tant que l'équipage tient (et qu'aucun joueur n'est à bord), le cycle AP5 ; sinon il devient prenable
		if (active)
		{
			bool crewDown = nowUnix - armor.m_iLaunchUnix >= CREW_GRACE_S && CountCrewFit(armor) == 0;
			if (playerId <= 0 && !crewDown)
			{
				if (armor.m_iState == SRP_ECmdArmorState.ROUTE)
					StepRoute(armor, vehicle, position, nowUnix);
				else if (armor.m_iState == SRP_ECmdArmorState.ENGAGE)
					StepEngage(armor, position, nowUnix);
				else
					StepRetreat(armor, vehicle, position, nowUnix);
				return;
			}
			BecomeTakeable(armor, position, nowUnix);
		}

		// Prenable -> pris dès qu'un joueur est à bord ; pris et ramené à la base avec un joueur à bord -> prime (C3)
		if (armor.m_iState == SRP_ECmdArmorState.PRENABLE && playerId > 0)
		{
			SetState(armor, SRP_ECmdArmorState.PRIS, nowUnix);
			ArmorNote(armor, string.Format("Blindé %1 pris par %2 près du carré %3", NameOf(armor), PlayerName(playerId), SRP_CmdScreens.CellText(position)));
		}
		if (armor.m_iState != SRP_ECmdArmorState.PRIS || armor.m_bPaid || armor.m_iAboardUnix <= 0)
			return;
		// Joueur à bord maintenant, ou descendu depuis ABOARD_GRACE_S au plus (le passage est toutes les 5 s)
		if (nowUnix - armor.m_iAboardUnix <= ABOARD_GRACE_S && IsAtBase(position))
			Deliver(armor, nowUnix);
	}

	//================================================================================================
	// Cycle (interne)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! ROUTE : halte atteinte ou contact vu à 500 m -> ENGAGE ; bloqué (moins de 15 m en appui_blinde_bloque_s) :
	//! retiré hors de vue (rendu au stock), sinon combat sur place — StepArmor
	protected void StepRoute(SRP_CmdArmor armor, IEntity vehicle, vector position, int nowUnix)
	{
		if (vector.Distance(position, armor.m_vLastPos) >= MOVE_STEP_M)
		{
			armor.m_vLastPos = position;
			armor.m_iLastMoveUnix = nowUnix;
		}

		if (vector.DistanceXZ(position, armor.m_vHalt) <= HALT_REACHED_M)
		{
			StartEngage(armor, armor.m_vTarget, nowUnix, "halte atteinte");
			return;
		}

		vector contact;
		if (FindSeenContact(position, CONTACT_ENGAGE_M, nowUnix, contact))
		{
			StartEngage(armor, contact, nowUnix, "joueurs vus en route");
			return;
		}

		if (nowUnix - armor.m_iLastMoveUnix < m_iStuckS)
			return;
		if (!SRP_Placement.IsVehicleSeenByAnyPlayer(vehicle, m_aPlayers))
		{
			ArmorNote(armor, string.Format("Blindé %1 bloqué en route près du carré %2 : retiré hors de vue, rendu au stock", NameOf(armor), SRP_CmdScreens.CellText(position)));
			RemoveHidden(armor, "blindé " + NameOf(armor) + " bloqué en route, rentré");
			return;
		}
		StartEngage(armor, position, nowUnix, "bloqué sous les yeux des joueurs, combat sur place");
	}

	//------------------------------------------------------------------------------------------------
	//! ENGAGE : cible recalée toutes les 60 s sur le contact vu le plus récent à 600 m ; repli après
	//! appui_blinde_duree_max_min, ou (renfort, riposte, Staff) sans joueur à appui_blinde_fin_sans_joueur_m depuis
	//! appui_blinde_fin_sans_joueur_min ; la contre-attaque finit par EndAssault — StepArmor
	protected void StepEngage(SRP_CmdArmor armor, vector position, int nowUnix)
	{
		if (nowUnix - armor.m_iRetargetUnix >= RETARGET_S)
		{
			armor.m_iRetargetUnix = nowUnix;
			vector contact;
			if (FindSeenContact(position, RETARGET_M, nowUnix, contact) && vector.DistanceXZ(contact, armor.m_vTarget) > RETARGET_MOVE_M)
			{
				armor.m_vTarget = contact;
				SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
				if (enemies && armor.m_Crew)
					enemies.OrderSearch(armor.m_Crew, contact);
			}
		}

		if (nowUnix - armor.m_iStateUnix >= m_iEngageMaxMin * 60)
		{
			StartRetreat(armor, nowUnix, string.Format("%1 min d'engagement", m_iEngageMaxMin));
			return;
		}

		if (armor.m_iReason == SRP_ECmdArmorReason.CONTRE_ATTAQUE)
			return;		// la contre-attaque décide seule de la fin (EndAssault)

		if (NearestPlayer(position) <= m_iNoPlayerM)
		{
			armor.m_iNoPlayerSinceUnix = 0;
			return;
		}
		if (armor.m_iNoPlayerSinceUnix == 0)
		{
			armor.m_iNoPlayerSinceUnix = nowUnix;
			return;
		}
		if (nowUnix - armor.m_iNoPlayerSinceUnix >= m_iNoPlayerMin * 60)
			StartRetreat(armor, nowUnix, string.Format("plus aucun joueur à %1 m depuis %2 min", m_iNoPlayerM, m_iNoPlayerMin));
	}

	//------------------------------------------------------------------------------------------------
	//! REPLI : dès qu'aucun joueur n'est à appui_blinde_retrait_m et que personne ne le voit, équipage et véhicule
	//! sont retirés et le blindé rentre au stock (Release) ; après appui_blinde_duree_max_min de repli (bloqué, ou
	//! joueurs restés près de son départ), retiré dès que personne ne le voit, quelle que soit la distance des
	//! joueurs : un repli ne tient jamais la place d'appui_blinde_simultanes indéfiniment — StepArmor
	protected void StepRetreat(SRP_CmdArmor armor, IEntity vehicle, vector position, int nowUnix)
	{
		bool overdue = nowUnix - armor.m_iStateUnix >= m_iEngageMaxMin * 60;
		if (!overdue && NearestPlayer(position) <= m_iRetireM)
			return;
		if (SRP_Placement.IsVehicleSeenByAnyPlayer(vehicle, m_aPlayers))
			return;
		string late = "";
		if (overdue)
			late = string.Format(", après %1 min de repli", m_iEngageMaxMin);
		ArmorNote(armor, string.Format("Blindé %1 rentré hors de vue (carré %2%3) : rendu au stock", NameOf(armor), SRP_CmdScreens.CellText(position), late));
		RemoveHidden(armor, "blindé " + NameOf(armor) + " rentré");
	}

	//------------------------------------------------------------------------------------------------
	//! LIVRE : retiré (DeleteEntityAndChildren) après appui_blinde_vide_s sans personne de vivant à bord (notre
	//! équipage hors de combat ne compte pas) ; jamais avec un joueur à bord — StepArmor
	protected void StepDelivered(SRP_CmdArmor armor, IEntity vehicle, int nowUnix)
	{
		if (HasSomeoneAboard(armor, vehicle))
		{
			armor.m_iEmptySinceUnix = 0;
			return;
		}
		if (armor.m_iEmptySinceUnix == 0)
		{
			armor.m_iEmptySinceUnix = nowUnix;
			return;
		}
		if (nowUnix - armor.m_iEmptySinceUnix < m_iEmptyS)
			return;
		ArmorNote(armor, string.Format("Blindé %1 livré, vide depuis %2 s : retiré de la base", NameOf(armor), m_iEmptyS));
		SCR_EntityHelper.DeleteEntityAndChildren(vehicle);
		armor.m_Vehicle = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Passage en ENGAGE : SearchAndDestroy vers aim (l'équipage reste à bord) — StepRoute
	protected void StartEngage(SRP_CmdArmor armor, vector aim, int nowUnix, string why)
	{
		armor.m_vTarget = aim;
		SetState(armor, SRP_ECmdArmorState.ENGAGE, nowUnix);
		armor.m_iRetargetUnix = nowUnix;
		armor.m_iNoPlayerSinceUnix = 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies && armor.m_Crew)
			enemies.OrderSearch(armor.m_Crew, aim);
		ArmorNote(armor, string.Format("Blindé %1 au combat vers le carré %2 (%3)", NameOf(armor), SRP_CmdScreens.CellText(aim), why));
	}

	//------------------------------------------------------------------------------------------------
	//! Passage en REPLI : retour vers son point de départ (hors de vue) — StepEngage, EndAssault
	protected void StartRetreat(SRP_CmdArmor armor, int nowUnix, string why)
	{
		SetState(armor, SRP_ECmdArmorState.REPLI, nowUnix);
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies && armor.m_Crew)
			enemies.OrderMove(armor.m_Crew, armor.m_vSpawn);
		ArmorNote(armor, string.Format("Blindé %1 en repli vers son départ (%2)", NameOf(armor), why));
	}

	//------------------------------------------------------------------------------------------------
	//! AP6 : équipage hors de combat (ou joueur à bord) : blindé perdu pour le Commandeur (Consume), survivants
	//! retirés hors de vue ; le jeu vide la faction du véhicule quand plus personne n'est à bord — StepArmor
	protected void BecomeTakeable(SRP_CmdArmor armor, vector position, int nowUnix)
	{
		ConsumeTicket(armor);
		RetireCrew(armor);
		SetState(armor, SRP_ECmdArmorState.PRENABLE, nowUnix);
		ArmorNote(armor, string.Format("Équipage du %1 hors de combat près du carré %2 : blindé prenable", NameOf(armor), SRP_CmdScreens.CellText(position)));
	}

	//------------------------------------------------------------------------------------------------
	//! C3 : prime au coffre (une seule fois), radio et évènement blinde_ramene, puis LIVRE — StepArmor
	protected void Deliver(SRP_CmdArmor armor, int nowUnix)
	{
		int bounty = 0;
		if (armor.m_Type)
			bounty = armor.m_Type.m_iBounty;
		string name = NameOf(armor);
		string who = PlayerName(armor.m_iAboardPlayer);
		armor.m_bPaid = true;
		SetState(armor, SRP_ECmdArmorState.LIVRE, nowUnix);
		armor.m_iEmptySinceUnix = 0;

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (bounty > 0 && treasury)
		{
			treasury.Add(bounty, "prise de guerre : " + name);
			SRP_FrontRadio.ArmorBroughtBack(name, bounty);
			ArmorNote(armor, string.Format("Blindé %1 ramené à la base par %2 : prime de %3 € versée au coffre", name, who, bounty));
			return;
		}
		if (bounty > 0)
			ArmorNote(armor, string.Format("Blindé %1 ramené à la base par %2 : coffre introuvable, prime de %3 € non versée", name, who, bounty));
		else
			ArmorNote(armor, string.Format("Blindé %1 ramené à la base par %2 : aucune prime réglée", name, who));
	}

	//------------------------------------------------------------------------------------------------
	//! Équipage et véhicule retirés (hors de vue), ticket rendu au stock — StepRoute, StepRetreat
	protected void RemoveHidden(SRP_CmdArmor armor, string reason)
	{
		DeleteCrew(armor);
		if (armor.m_Vehicle && !armor.m_Vehicle.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(armor.m_Vehicle);
		armor.m_Vehicle = null;
		ReleaseTicket(armor, reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Équipage supprimé tout de suite (SRP_EnemyComponent.Delete) — RemoveHidden, DeleteAll
	protected void DeleteCrew(SRP_CmdArmor armor)
	{
		if (!armor.m_Crew)
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
			enemies.Delete(armor.m_Crew);
		armor.m_Crew = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Survivants de l'équipage retirés plus tard, hors de vue (SRP_EnemyComponent.RetireLater) — StepArmor,
	//! BecomeTakeable
	protected void RetireCrew(SRP_CmdArmor armor)
	{
		if (!armor.m_Crew || SRP_EnemyComponent.AliveAgents(armor.m_Crew) <= 0)
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		array<ref SRP_EnemyGroup> leaving = {};
		leaving.Insert(armor.m_Crew);
		enemies.RetireLater(leaving);
	}

	//------------------------------------------------------------------------------------------------
	//! Ticket perdu pour de bon (détruit, pris) — StepArmor, BecomeTakeable
	protected void ConsumeTicket(SRP_CmdArmor armor)
	{
		if (armor.m_iTicket <= 0)
			return;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources)
			resources.Consume(armor.m_iTicket, 1);
		armor.m_iTicket = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Ticket rendu au stock (rentré, retiré, remise à plat) — RemoveHidden, DeleteAll
	protected void ReleaseTicket(SRP_CmdArmor armor, string reason)
	{
		if (armor.m_iTicket <= 0)
			return;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources)
			resources.Release(armor.m_iTicket, reason);
		armor.m_iTicket = 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void SetState(SRP_CmdArmor armor, int state, int nowUnix)
	{
		armor.m_iState = state;
		armor.m_iStateUnix = nowUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! Contacts du cerveau (toute l'île, mis à jour depuis cmd_contact_vise_s), seulement si un blindé roule ou
	//! combat — Tick
	protected void RefreshContacts()
	{
		m_aContacts.Clear();
		bool needed = false;
		foreach (SRP_CmdArmor armor : m_aArmors)
		{
			if (armor && (armor.m_iState == SRP_ECmdArmorState.ROUTE || armor.m_iState == SRP_ECmdArmorState.ENGAGE))
				needed = true;
		}
		if (!needed)
			return;
		SRP_Commander commander = SRP_Commander.Get();
		if (commander)
			commander.GetContacts(-1, commander.m_iAimS, m_aContacts);
	}

	//------------------------------------------------------------------------------------------------
	//! Contact VU le plus récent (depuis cmd_contact_vise_s au plus) à radius de position ; jamais un contact qui ne
	//! vient que d'informateurs (C4) ; position connue du Commandeur (floutée, CO5) — StepRoute, StepEngage
	protected bool FindSeenContact(vector position, float radius, int nowUnix, out vector contact)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return false;
		int oldest = nowUnix - commander.m_iAimS;
		int bestUnix = -1;
		foreach (SRP_CmdContact known : m_aContacts)
		{
			if (!known || !known.m_bSeen || known.m_bInformantOnly)
				continue;
			if (known.m_iSeenUnix < oldest || known.m_iSeenUnix <= bestUnix)
				continue;
			if (vector.DistanceXZ(known.m_vPos, position) > radius)
				continue;
			bestUnix = known.m_iSeenUnix;
			contact = known.m_vPos;
		}
		return bestUnix >= 0;
	}

	//================================================================================================
	// Pose (interne)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Types refaits d'après les réglages (index 0, 1, 2 = variant du Staff) — LoadSettings
	protected void RebuildTypes()
	{
		m_aTypes.Clear();
		AddType(1, m_sType1Name, m_sType1Prefab, m_iType1Weight, m_iType1Bounty);
		AddType(2, m_sType2Name, m_sType2Prefab, m_iType2Weight, m_iType2Bounty);
		AddType(3, m_sType3Name, m_sType3Prefab, m_iType3Weight, m_iType3Bounty);
	}

	//------------------------------------------------------------------------------------------------
	//! Un type ; garde AP4 : prefab vide ou interdit -> gardé à sa place mais écarté (m_bAllowed faux), ligne de
	//! journal — RebuildTypes
	protected void AddType(int number, string name, string prefab, int weight, int bounty)
	{
		SRP_CmdArmorType armorType = new SRP_CmdArmorType();
		armorType.m_sName = name.Trim();
		if (armorType.m_sName.IsEmpty())
			armorType.m_sName = string.Format("blindé %1", number);
		string path = prefab.Trim();
		armorType.m_sPrefab = path;
		armorType.m_iWeight = weight;
		armorType.m_iBounty = bounty;
		armorType.m_bAllowed = true;
		if (path.IsEmpty())
		{
			armorType.m_bAllowed = false;
			TypeProblem(string.Format("Blindé %1 (%2) : prefab vide, type écarté (appui_blinde_%1_prefab)", number, armorType.m_sName));
		}
		else if (IsForbiddenPrefab(path))
		{
			armorType.m_bAllowed = false;
			TypeProblem(string.Format("Blindé %1 (%2) : prefab refusé, les T-14, K-17, Kurganets et T-90 sont interdits (AP4) : %3", number, armorType.m_sName, path));
		}
		m_aTypes.Insert(armorType);
	}

	//------------------------------------------------------------------------------------------------
	//! Garde AP4 : le chemin du prefab (sans son identifiant) nomme un T-14, un K-17, un Kurganets ou un T-90
	protected static bool IsForbiddenPrefab(string prefab)
	{
		string path = prefab;
		int close = path.IndexOf("}");
		if (close >= 0)
			path = path.Substring(close + 1, path.Length() - close - 1);
		path.ToLower();
		if (path.Contains("t14") || path.Contains("t-14") || path.Contains("k17") || path.Contains("k-17"))
			return true;
		return path.Contains("kurganets") || path.Contains("t90") || path.Contains("t-90");
	}

	//------------------------------------------------------------------------------------------------
	//! Problème de réglage d'un type : journal du Commandeur (SYSTEME) et console — AddType
	protected void TypeProblem(string text)
	{
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, text);
		Print("[SRP] " + text, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Sécurité (Staff compris) : cible en mer, dans la zone de la base, ou à appui_distance_base_m du repère de la
	//! base -> la raison ; "" sinon — Launch
	protected string TargetRefusal(SRP_FrontComponent front, vector target)
	{
		if (SRP_Placement.IsWater(target))
			return "sécurité : cible en mer";
		if (front.IsInBaseZone(target))
			return "sécurité : cible dans la zone de la base";
		vector basePosition;
		if (m_Support && SRP_Placement.BasePosition(basePosition) && vector.DistanceXZ(target, basePosition) < m_Support.m_iBaseM)
			return string.Format("sécurité : cible à moins de %1 m de la base", m_Support.m_iBaseM);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! AP5 : départ hors de vue (et à appui_blinde_retrait_m de tout joueur), sur une route si possible, entre
	//! appui_blinde_depart_min_m et max_m de la cible, sur un carré ROUGE hors de la zone de la base — Launch
	protected bool FindStart(SRP_FrontComponent front, vector target, out vector start)
	{
		int maxM = m_iSpawnMaxM;
		if (maxM < m_iSpawnMinM)
			maxM = m_iSpawnMinM;
		for (int attempt = 0; attempt < START_TRIES; attempt++)
		{
			vector candidate;
			if (!SRP_Placement.FindSpawnPosition(target, m_iSpawnMinM, maxM, m_aPlayers, true, false, candidate, m_iRetireM))
				continue;
			if (!front.IsRedAt(candidate) || front.IsInBaseZone(candidate))
				continue;
			start = candidate;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! AP5 : halte à appui_blinde_halte_m de la cible, du côté du départ (la moitié du trajet au plus), rabattue sur
	//! une route proche quand elle en reste assez loin ; 0 = la cible elle-même — Launch
	protected vector FindHalt(vector target, vector start)
	{
		if (m_iHaltM <= 0)
			return target;
		vector away = start - target;
		away[1] = 0;
		float length = away.Length();
		if (length < 1)
			return target;
		float halfWay = length * 0.5;
		float haltDistance = m_iHaltM;
		if (haltDistance > halfWay)
			haltDistance = halfWay;
		float ratio = haltDistance / length;
		vector standoff = SRP_Placement.OnGround(target + away * ratio);
		if (SRP_Placement.IsWater(standoff))
			return target;

		float minRoadDistance = haltDistance * 0.5;
		vector onRoad;
		if (SRP_Placement.FindRoadPointToward(start, standoff, HALT_ROAD_SNAP_M, onRoad) && vector.DistanceXZ(onRoad, target) >= minRoadDistance)
			return onRoad;
		return standoff;
	}

	//------------------------------------------------------------------------------------------------
	//! Le véhicule posé tourné vers toward (même recette que les camions de renfort, SRP_EnemyTrucks.SpawnOriented)
	//! — Launch
	protected static IEntity SpawnVehicle(ResourceName prefab, vector position, vector toward)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Blindé : prefab invalide " + prefab, LogLevel.ERROR);
			return null;
		}
		vector heading = toward - position;
		float yaw = Math.Atan2(heading[0], heading[2]) * Math.RAD2DEG;
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = position + vector.Up * SPAWN_LIFT_M;
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! appui_blinde_faction (AP6) : vide = la faction de l'ennemi du mod, déjà donnée par SpawnVehicleCrew ; sinon
	//! l'équipage et le véhicule prennent cette faction (clé inconnue : journal, faction gardée) — Launch
	protected void ApplyFaction(SRP_EnemyGroup crew, IEntity vehicle)
	{
		if (m_sFaction.IsEmpty())
			return;
		Faction faction;
		FactionManager factions = GetGame().GetFactionManager();
		if (factions)
			faction = factions.GetFactionByKey(m_sFaction);
		if (!faction)
		{
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Blindé : clé de faction « %1 » inconnue (appui_blinde_faction), faction de l'ennemi du mod gardée", m_sFaction));
			return;
		}
		if (crew && crew.m_Group)
			crew.m_Group.SetFaction(faction);
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(vehicle.FindComponent(FactionAffiliationComponent));
		if (affiliation)
			affiliation.SetAffiliatedFaction(faction);
	}

	//================================================================================================
	// Lectures (interne)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Membres de l'équipage vivants et conscients, où qu'ils soient
	protected int CountCrewFit(SRP_CmdArmor armor)
	{
		if (!armor.m_Crew)
			return 0;
		array<ref SRP_EnemyGroup> crewList = {};
		crewList.Insert(armor.m_Crew);
		return SRP_EnemyComponent.ActiveAgentsNear(crewList, vector.Zero, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Occupants du véhicule (toutes ses places)
	protected static void GetOccupantsOf(IEntity vehicle, notnull array<IEntity> occupants)
	{
		if (!vehicle)
			return;
		SCR_BaseCompartmentManagerComponent compartments = SCR_BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(SCR_BaseCompartmentManagerComponent));
		if (compartments)
			compartments.GetOccupants(occupants);
	}

	//------------------------------------------------------------------------------------------------
	//! Identifiant d'un joueur vivant à bord, 0 s'il n'y en a aucun
	protected int PlayerAboard(IEntity vehicle)
	{
		array<IEntity> occupants = {};
		GetOccupantsOf(vehicle, occupants);
		foreach (IEntity occupant : occupants)
		{
			if (!occupant || SRP_Utils.IsDead(occupant))
				continue;
			int id = SRP_Utils.GetPlayerIdFromEntity(occupant);
			if (id > 0)
				return id;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Quelqu'un de vivant à bord, hors de notre équipage (joueur, prisonnier amené par les joueurs…) — StepDelivered
	protected bool HasSomeoneAboard(SRP_CmdArmor armor, IEntity vehicle)
	{
		array<IEntity> occupants = {};
		GetOccupantsOf(vehicle, occupants);
		if (occupants.IsEmpty())
			return false;
		array<IEntity> crewMembers = {};
		if (armor.m_Crew)
			SRP_EnemyComponent.GetMembers(armor.m_Crew, crewMembers);
		foreach (IEntity occupant : occupants)
		{
			if (!occupant || SRP_Utils.IsDead(occupant))
				continue;
			if (SRP_Utils.GetPlayerIdFromEntity(occupant) <= 0 && crewMembers.Contains(occupant))
				continue;		// notre équipage hors de combat ne retient pas le retrait
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDestroyed(IEntity vehicle)
	{
		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(vehicle);
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! C3 : à appui_blinde_base_rayon_m du repère de la base (sans repère : dans la zone de la base)
	protected bool IsAtBase(vector position)
	{
		vector basePosition;
		if (SRP_Placement.BasePosition(basePosition))
			return vector.DistanceXZ(position, basePosition) <= m_iBaseRadiusM;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		return front && front.IsInBaseZone(position);
	}

	//------------------------------------------------------------------------------------------------
	//! Distance au joueur vivant le plus proche (joueurs relus au passage)
	protected float NearestPlayer(vector position)
	{
		return SRP_Placement.NearestPlayer(position, m_aPlayers);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsActiveState(int state)
	{
		return state == SRP_ECmdArmorState.ROUTE || state == SRP_ECmdArmorState.ENGAGE || state == SRP_ECmdArmorState.REPLI;
	}

	//================================================================================================
	// Textes (journal du Commandeur et Staff seulement, jamais publics)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	protected void ArmorNote(SRP_CmdArmor armor, string text)
	{
		int region = -1;
		if (armor)
			region = armor.m_iRegion;
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.APPUI, text);
	}

	//------------------------------------------------------------------------------------------------
	protected static string NameOf(SRP_CmdArmor armor)
	{
		if (armor && armor.m_Type && !armor.m_Type.m_sName.IsEmpty())
			return armor.m_Type.m_sName;
		return "blindé";
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07) », sinon « le carré 074 042 »
	protected static string PlaceOf(SRP_FrontComponent front, int zone, vector target)
	{
		if (front && zone >= 0)
		{
			string label = front.GetZoneLabel(zone);
			if (!label.IsEmpty())
				return label;
		}
		return "le carré " + SRP_CmdScreens.CellText(target);
	}

	//------------------------------------------------------------------------------------------------
	protected static string PlaceText(SRP_CmdArmor armor)
	{
		return PlaceOf(SRP_FrontComponent.GetInstance(), armor.m_iZone, armor.m_vTarget);
	}

	//------------------------------------------------------------------------------------------------
	protected static string StateWord(int state)
	{
		if (state == SRP_ECmdArmorState.ROUTE)
			return "en route";
		if (state == SRP_ECmdArmorState.ENGAGE)
			return "au combat";
		if (state == SRP_ECmdArmorState.REPLI)
			return "en repli";
		if (state == SRP_ECmdArmorState.PRENABLE)
			return "équipage hors de combat, prenable";
		if (state == SRP_ECmdArmorState.PRIS)
			return "pris par les joueurs";
		if (state == SRP_ECmdArmorState.LIVRE)
			return "ramené à la base";
		if (state == SRP_ECmdArmorState.DETRUIT)
			return "détruit";
		return "état inconnu";
	}

	//------------------------------------------------------------------------------------------------
	protected static string ReasonWord(int reason)
	{
		if (reason == SRP_ECmdArmorReason.CONTRE_ATTAQUE)
			return "contre-attaque";
		if (reason == SRP_ECmdArmorReason.RENFORT)
			return "renfort";
		if (reason == SRP_ECmdArmorReason.RIPOSTE)
			return "riposte";
		if (reason == SRP_ECmdArmorReason.STAFF)
			return "Staff";
		return "raison inconnue";
	}

	//------------------------------------------------------------------------------------------------
	protected static string PlayerName(int playerId)
	{
		if (playerId <= 0)
			return "un joueur";
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (playerName.IsEmpty())
			return "un joueur";
		return playerName;
	}

	//------------------------------------------------------------------------------------------------
	//! Mot du Staff ramené à une forme simple : minuscules, sans espace, tiret ni souligné
	protected static string NormalizeWord(string text)
	{
		string result = text.Trim();
		result.ToLower();
		result.Replace("-", "");
		result.Replace(" ", "");
		result.Replace("_", "");
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! AP1 : verrou d'entrée de la batterie 2S1 du Commandeur (toutes machines ; aucun champ ajouté)
modded class SCR_GetInUserAction
{
	override bool CanBePerformedScript(IEntity user)
	{
		IEntity root = SCR_EntityHelper.GetMainParent(GetOwner(), true);
		if (SRP_CmdSupport.IsBatteryVehicle(root))
		{
			SetCannotPerformReason("Pièce verrouillée");
			return false;
		}
		return super.CanBePerformedScript(user);
	}
}
