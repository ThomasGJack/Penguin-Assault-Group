//------------------------------------------------------------------------------------------------
// SimpleRP — Garnisons des localités ennemies (carte #69, livraison 3 : « effectifs, postes »)
// Statique, serveur seulement ; appelé par SRP_TerritoryComponent (pose, complément, pose différée, posture de nuit) ;
// lu par SRP_FrontEnemyComponent (LiveLosses) et le Commandeur (FindTownHQ, IsCentreGroup, CountPlanSurvivors).
// - COMPOSITION, la même de jour et de nuit (même total, arrondi au pair) : en ville, le QG (groupe de commandement :
//   chef, sergent, opérateur radio, infirmier, tireur d'élite) ; des postes d'entrée de 2 soldats sur les routes (hameau
//   et village 1, ville 2) ; le reste en sections de 6, puis une de 4, ou un binôme de 2 (jamais en ronde).
// - POINTS CLÉS (front, D3 et D6) : le centre de la garnison est le point clé CENTRE de la localité (GetCenter) ; en
//   ville à deux points clés, le QG est posé au point clé QG (m_vHQ) et la plus grande section tient le centre (deux
//   groupes de commandement, tâche « centre ») ; un point clé déjà saisi (carré bleu) : son groupe se pose à l'autre.
//   Entrées et parts du cercle restent calculées depuis le point clé centre. Aucune garnison dans une localité prise
//   (SRP_ZoneFall.IsLocalityTaken) ni vidée par un décrochage (SRP_CmdManeuvers.IsLocalityEmptied, MA7).
// - POSE : le centre d'abord (le QG, ou la plus grande section : groupe de commandement, il tient le centre), puis les
//   entrées, puis les sections, chacune dans sa part du cercle : ronde sur les rues (part de jour), sinon dans un
//   bâtiment, sinon un poste. Toujours hors de vue, loin des joueurs (rayon du secteur, 300 m au moins) ; un joueur dans
//   le secteur : un groupe sans point hors de vue est différé tel quel (réessayé par le territoire toutes les 30 s).
//   Posée la nuit : la posture de nuit est prise aussitôt (SwitchPosture), comme pour une garnison posée le jour à la
//   tombée de la nuit ; au jour, tout reprend (correctif : une garnison posée la nuit gardait sa disposition de nuit).
// - PLACE (Q7, C6), seul endroit où elle s'applique à une garnison : effectif décidé (SRP_FrontEnemyComponent
//   .GetLocalityEffective : base + joueurs + menace locale, × 1,25 si l'officier de la région vit, 60 au plus, moins
//   pertes F12 et envoyés, plus reçus) borné par la part de la localité (SRP_CmdCapacity.GarrisonCap ; localité tenue
//   au complet, MA11 : 60 seulement) ; puis, avant chaque groupe, SRP_CmdCapacity.Ask(« garnison-<localité> »), classe
//   COMBAT si la localité est attaquée, GARNISON sinon, et Tag du groupe posé. Le centre et les postes d'entrée passent
//   en premier ; un centre qui ne tient pas est réduit (section de 4, puis binôme) plutôt que d'être perdu ; un groupe
//   refusé est différé tel quel (réessayé avec les autres toutes les 30 s), sa demande de place reste notée (file RE12),
//   une ligne au journal par minute au plus.
// - POSTURE : la nuit, les rondes en trop par rapport à la part de nuit passent en poste à une entrée libre ; le jour,
//   elles reprennent leur ronde. Ordres seulement, le total ne change pas.
// Journal : catégorie ENNEMI seulement, aucun message aux joueurs.
//------------------------------------------------------------------------------------------------

enum SRP_EGarrisonSlot
{
	CENTRE,		// le QG (ville) ou la plus grande section : groupe de commandement, au point clé centre (ou QG)
	ENTREE,		// binôme de sentinelles à une entrée de la localité, sur la route
	SECTION,	// section de 4 ou 6 : ronde, bâtiment ou poste
	BINOME		// reste de 2 soldats (ou poste d'entrée sans entrée trouvée) : poste ou bâtiment, jamais en ronde
}

//------------------------------------------------------------------------------------------------
//! Un groupe du plan d'une garnison ; gardé tel quel s'il est différé (un joueur dans le secteur, aucun point hors de
//! vue ; ou pas de place pour lui)
class SRP_GarrisonSlot
{
	ResourceName m_sPrefab;
	int m_iSize;
	int m_iKind;			// SRP_EGarrisonSlot
	vector m_vEntry;		// ENTREE : le point d'entrée, sur la route
	bool m_bHasEntry;
	bool m_bAtHQ;			// CENTRE : le QG d'une ville à deux points clés, posé au point clé QG (D6)
}

//------------------------------------------------------------------------------------------------
class SRP_EnemyGarrison
{
	// Réglages (copiés par SRP_TerritoryComponent.OnPostInit ; valeurs par défaut ci-dessous)
	static int s_iPostsHamlet = 1;				// postes d'entrée d'un hameau
	static int s_iPostsVillage = 1;				// … d'un village
	static int s_iPostsTown = 2;				// … d'une ville (bourg compris)
	static int s_iNightExtraPosts = 1;			// ancien réglage (poste d'entrée en plus pour une pose de nuit) : ne sert plus
	static bool s_bNightPosture = true;			// posture de nuit (réglage m_bNightPosture du territoire)
	static int s_iCommandMinSoldiers = 13;		// QG seulement pour une garnison de ville d'au moins … soldats
	static int s_iPatrolPercentDay = 50;		// part des sections en ronde, le jour, en pour cent
	static int s_iPatrolPercentNight = 20;		// … la nuit
	static int s_iBuildingPercent = 40;			// section qui ne fait pas de ronde : part dans un bâtiment, en pour cent
	static float s_fCenterRingFactor = 0.3;		// le centre est posé à moins de … × le rayon du centre…
	static float s_fCenterRingMax = 40;			// … et à moins de … mètres
	static float s_fEntryDefendRadius = 15;		// poste d'entrée : rayon du point Defend
	static float s_fEntryHoldRadius = 20;		// … et maintien de position
	static float s_fBuildingDefendRadius = 8;	// bâtiment : rayon du point Defend
	static float s_fBuildingHoldRadius = 12;	// … et maintien de position
	static float s_fPostHoldMax = 100;			// poste : maintien de position au plus (et jamais plus que le rayon)

	protected static const float PLAYER_MIN_DISTANCE = 300;		// pose : jamais à moins de … m d'un joueur (ou du rayon du secteur, s'il est plus grand)
	protected static const float FALLBACK_PLAYER_DISTANCE = 150;	// repli sur l'ancien tirage : jamais à moins de … m d'un joueur
	protected static const float ENTRY_SPREAD = 25;				// poste d'entrée : posé à moins de … m de son entrée
	protected static const int CAP_LOG_MS = 60000;				// « garnison réduite : plafond » : une ligne par minute au plus
	protected static int s_iNextCapLogTick;

	//------------------------------------------------------------------------------------------------
	// Pose
	//------------------------------------------------------------------------------------------------
	//! Pose (topUp = false) ou complète (topUp = true : sections seulement, ni QG ni poste d'entrée) la garnison de
	//! « state » : « wanted » soldats voulus (pose : l'effectif décidé ; complément : décidé moins prévus). Rien dans une
	//! localité prise ou vidée par un décrochage (MA7). Place : « wanted » est d'abord borné par la part de la localité
	//! (SRP_CmdCapacity.GarrisonCap sur prévus + voulus ; tenue au complet, MA11 : 60 seulement), puis chaque groupe passe
	//! par Ask (PlacePlan). Retourne les soldats posés ; les groupes posés entrent dans outGroups (state.m_aGroups),
	//! marqués de leur localité (sirène), de leur classe de place et de leur poste d'origine ; state.m_iPlannedSoldiers
	//! augmente d'autant. Les groupes différés vont dans state.m_aDeferredSlots. Toujours la composition et la part de
	//! rondes DE JOUR ; posée la nuit (« night », posture de nuit réglée), la posture de nuit est prise aussitôt
	//! (SwitchPosture : les rondes en trop passent en poste de nuit, elles reprendront leur ronde au jour). Un complément
	//! de nuit n'y touche que si la garnison est déjà en posture de nuit (pas pendant une alerte, où le territoire ne
	//! change pas la posture).
	static int Spawn(SRP_EnemyComponent enemies, SRP_SectorState state, int wanted, bool night, bool topUp, out array<ref SRP_EnemyGroup> outGroups)
	{
		if (!enemies || !state || !state.m_Sector || !enemies.HasPrefabs() || wanted < 2)
			return 0;
		if (!MayGarrison(state))
			return 0;
		int allowed = AllowedSoldiers(state, wanted, topUp);
		if (allowed < wanted && (!topUp || allowed >= 2))
		{
			// Un complément que la part ne laisse plus du tout grandir reste muet (réessayé par le territoire)
			string cappedWhat = "pose";
			if (topUp)
				cappedWhat = "complément";
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : %2 de garnison bornée à %3 soldat(s) sur %4 voulus (part des localités, %5 au plus)", state.m_sName, cappedWhat, Math.MaxInt(allowed, 0), wanted, SRP_CmdCapacity.Get().GetGarrisonMax()));
		}
		wanted = allowed;
		if (wanted < 2)
			return 0;
		array<ref SRP_GarrisonSlot> plan = {};
		BuildPlan(enemies, state.m_Sector.m_sKind, wanted, topUp, HasCommand(outGroups), state.m_Sector.m_bHasHQ, plan);
		if (plan.IsEmpty())
			return 0;
		bool nightPosture = night && s_bNightPosture;
		if (topUp)
			nightPosture = nightPosture && state.m_bNightPosture;
		else
			state.m_bNightPosture = false;	// posée de jour ; la nuit, SwitchPosture ci-dessous
		string what = "pose";
		if (topUp)
			what = "complément";
		int placed = PlacePlan(enemies, state, plan, night, what, outGroups);
		if (placed > 0 && nightPosture)
			SwitchPosture(enemies, state, true);
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Les groupes différés de la garnison (un joueur était dans le secteur, aucun point hors de vue ; ou pas de place) :
	//! réessayés tels quels (le centre d'abord, puis les entrées, puis les sections) ; ceux qui ne trouvent toujours pas
	//! de point hors de vue avec un joueur dans le secteur, ou pas de place, y retournent. Localité prise ou vidée entre-
	//! temps : abandonnés. En posture de nuit, les nouvelles rondes en trop passent en poste de nuit (sinon le territoire
	//! prendra la posture de nuit au passage suivant). Retourne les soldats posés.
	static int RetryDeferred(SRP_EnemyComponent enemies, SRP_SectorState state, out array<ref SRP_EnemyGroup> outGroups)
	{
		if (!enemies || !state || !state.m_Sector || state.m_aDeferredSlots.IsEmpty())
			return 0;
		if (!MayGarrison(state))
		{
			// Localité prise ou vidée entre-temps : les groupes en attente sont abandonnés
			state.m_aDeferredSlots.Clear();
			return 0;
		}
		array<ref SRP_GarrisonSlot> plan = {};
		foreach (SRP_GarrisonSlot slot : state.m_aDeferredSlots)
			plan.Insert(slot);
		state.m_aDeferredSlots.Clear();
		int placed = PlacePlan(enemies, state, plan, state.m_bNightPosture, "pose différée", outGroups);
		if (placed > 0 && state.m_bNightPosture && s_bNightPosture)
			SwitchPosture(enemies, state, true);
		return placed;
	}

	//------------------------------------------------------------------------------------------------
	//! Le plan : QG (ville, garnison d'au moins s_iCommandMinSoldiers, pas un complément, pas encore de commandement),
	//! postes d'entrée (pas en complément ; plafonnés pour qu'il reste au moins une section de 4), puis le reste arrondi au
	//! pair au-dessus : sections de 6, puis une de 4, ou un binôme. Sans QG et sans commandement déjà posé, la plus grande
	//! section tient le centre. Ville à deux points clés (« twoKeys », D6) : le QG est posé au point clé QG (m_bAtHQ) et la
	//! plus grande section tient aussi le centre, au point clé centre. Ordre du plan : centre(s), entrées, sections (c'est
	//! l'ordre de pose, donc des plafonds). Même plan de jour et de nuit : les postes de nuit sont pris sur les rondes par
	//! SwitchPosture.
	protected static void BuildPlan(SRP_EnemyComponent enemies, string kind, int wanted, bool topUp, bool hasCommand, bool twoKeys, array<ref SRP_GarrisonSlot> plan)
	{
		int hq = 0;
		ResourceName hqPrefab = enemies.GetCommandGroupPrefab();
		if (kind == "ville" && !topUp && !hasCommand && wanted >= s_iCommandMinSoldiers && !hqPrefab.IsEmpty())
			hq = enemies.GroupSize(hqPrefab);
		if (hq > 0)
		{
			AddSlot(plan, hqPrefab, hq, SRP_EGarrisonSlot.CENTRE);
			plan[plan.Count() - 1].m_bAtHQ = twoKeys;
		}

		// Postes d'entrée (binômes de sentinelles), jamais au point de ne plus laisser une section de 4
		array<ref SRP_GarrisonSlot> entries = {};
		if (!topUp)
		{
			int posts = PostsFor(kind);
			int room = (wanted - hq - 4) / 2;
			if (room < 0)
				room = 0;
			if (posts > room)
				posts = room;
			for (int i = 0; i < posts; i++)
			{
				ResourceName pair = PickPair(enemies);
				if (pair.IsEmpty())
					break;
				AddSlot(entries, pair, enemies.GroupSize(pair), SRP_EGarrisonSlot.ENTREE);
			}
		}
		int postSoldiers = 0;
		foreach (SRP_GarrisonSlot entry : entries)
			postSoldiers += entry.m_iSize;

		// Le reste, arrondi au pair au-dessus (même parité de jour et de nuit : même total)
		int rest = wanted - hq - postSoldiers;
		if (rest < 2)
			rest = 2;
		if (rest - (rest / 2) * 2 == 1)
			rest++;
		array<ref SRP_GarrisonSlot> sections = {};
		AddSections(enemies, rest, sections);

		// Pas de commandement déjà posé : la plus grande section tient le centre (sans QG, ou avec un QG posé au point clé
		// QG d'une ville à deux points clés)
		if ((hq == 0 || twoKeys) && !hasCommand && !sections.IsEmpty())
		{
			int best = 0;
			foreach (int index, SRP_GarrisonSlot candidate : sections)
			{
				if (candidate.m_iSize > sections[best].m_iSize)
					best = index;
			}
			sections[best].m_iKind = SRP_EGarrisonSlot.CENTRE;
			plan.Insert(sections[best]);
		}
		foreach (SRP_GarrisonSlot post : entries)
			plan.Insert(post);
		foreach (SRP_GarrisonSlot section : sections)
		{
			if (section.m_iKind != SRP_EGarrisonSlot.CENTRE)
				plan.Insert(section);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le reste (pair) en groupes : sections de 6, puis une de 4, ou un binôme de 2 ; faute du prefab de la bonne taille,
	//! la taille en dessous, et en dernier recours un groupe de m_aGroupPrefabs à sa taille réelle
	protected static void AddSections(SRP_EnemyComponent enemies, int rest, array<ref SRP_GarrisonSlot> slots)
	{
		int left = rest;
		int guard = 0;
		while (left >= 2 && guard < 40)
		{
			guard++;
			ResourceName prefab = "";
			int kind = SRP_EGarrisonSlot.SECTION;
			if (left >= 6)
				prefab = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), 6, 6);
			if (prefab.IsEmpty() && left >= 4)
				prefab = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), 4, 4);
			if (prefab.IsEmpty())
			{
				prefab = PickPair(enemies);
				kind = SRP_EGarrisonSlot.BINOME;
			}
			if (prefab.IsEmpty())
			{
				prefab = enemies.PickGroupPrefab();
				kind = SRP_EGarrisonSlot.SECTION;
			}
			int size = enemies.GroupSize(prefab);
			if (prefab.IsEmpty() || size <= 0)
				break;
			if (size <= 2)
				kind = SRP_EGarrisonSlot.BINOME;
			AddSlot(slots, prefab, size, kind);
			left -= size;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddSlot(array<ref SRP_GarrisonSlot> slots, ResourceName prefab, int size, int kind)
	{
		SRP_GarrisonSlot slot = new SRP_GarrisonSlot();
		slot.m_sPrefab = prefab;
		slot.m_iSize = size;
		slot.m_iKind = kind;
		slots.Insert(slot);
	}

	//------------------------------------------------------------------------------------------------
	//! Un binôme de 2 soldats : les sentinelles réglées, sinon une section de 2, sinon un groupe de 2 de m_aGroupPrefabs
	protected static ResourceName PickPair(SRP_EnemyComponent enemies)
	{
		ResourceName pair = enemies.PickPrefabOfSize(enemies.GetSentryPrefabs(), 2, 2);
		if (pair.IsEmpty())
			pair = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), 2, 2);
		if (pair.IsEmpty())
			pair = enemies.PickGroupPrefabOfSize(2, 2);
		return pair;
	}

	//------------------------------------------------------------------------------------------------
	//! Postes d'entrée selon le genre de la localité (hameau, village, ville)
	protected static int PostsFor(string kind)
	{
		if (kind == "hameau")
			return s_iPostsHamlet;
		if (kind == "ville")
			return s_iPostsTown;
		return s_iPostsVillage;
	}

	//------------------------------------------------------------------------------------------------
	//! Un de ces groupes est-il déjà le groupe de commandement (le centre) ? Morts compris : pas de remplacement des pertes
	static bool HasCommand(array<ref SRP_EnemyGroup> groups)
	{
		if (!groups)
			return false;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (record && record.m_bCommand)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose les groupes du plan, dans l'ordre (centre, entrées, sections), chacun à son point : la place d'abord
	//! (SRP_CmdCapacity.Ask par groupe, classe COMBAT si la localité est attaquée, GARNISON sinon ; un centre qui ne tient
	//! pas est réduit ; un groupe sans place est différé tel quel), puis un point hors de vue, à au moins max(300, rayon)
	//! de tout joueur ; à défaut, différé si un joueur est dans le secteur, sinon l'ancien tirage à 150 m au moins de tout
	//! joueur. Centre : au point clé centre ; QG d'une ville à deux points clés : au point clé QG ; un point clé saisi
	//! (carré bleu) : son groupe se pose à l'autre. Entrées et parts du cercle : depuis le point clé centre. Une ligne
	//! ENNEMI résume la pose. Retourne les soldats posés.
	protected static int PlacePlan(SRP_EnemyComponent enemies, SRP_SectorState state, array<ref SRP_GarrisonSlot> plan, bool night, string what, out array<ref SRP_EnemyGroup> outGroups)
	{
		SRP_SectorDef sector = state.m_Sector;
		vector center = sector.GetCenter();
		float radius = sector.m_fRadius;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();	// tous : un joueur en délai de grâce ne doit pas non plus voir la pose
		float playerMinDist = Math.Max(PLAYER_MIN_DISTANCE, radius);
		bool deferIfHidden = SRP_Placement.NearestPlayer(center, players) <= radius;

		// Points clés (D6) : le QG au point clé QG, la section du centre au point clé centre ; un point déjà saisi (carré
		// bleu) renvoie son groupe à l'autre point
		bool twoKeys = sector.m_bHasHQ;
		vector hqPoint = center;
		if (twoKeys)
			hqPoint = sector.m_vHQ;
		bool centreSeized = twoKeys && IsSeizedPoint(center);
		bool hqSeized = twoKeys && IsSeizedPoint(hqPoint);

		// Place (Q7) : classe de la localité, demande notée sous « garnison-<localité> »
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		int capClass = GarrisonClass(state.m_sName);
		string demandKey = "garnison-" + state.m_sName;

		// Les entrées des postes qui n'en ont pas encore (un poste différé garde la sienne) ; sans entrée trouvée, le
		// binôme devient un poste ou un bâtiment de la localité
		int needEntries = 0;
		foreach (SRP_GarrisonSlot counted : plan)
		{
			if (counted.m_iKind == SRP_EGarrisonSlot.ENTREE && !counted.m_bHasEntry)
				needEntries++;
		}
		if (needEntries > 0)
		{
			array<vector> found = {};
			SRP_Placement.FindTownEntrances(center, radius, needEntries, found);
			int next = 0;
			foreach (SRP_GarrisonSlot post : plan)
			{
				if (post.m_iKind != SRP_EGarrisonSlot.ENTREE || post.m_bHasEntry)
					continue;
				if (next < found.Count())
				{
					post.m_vEntry = found[next];
					post.m_bHasEntry = true;
					next++;
				}
				else
					post.m_iKind = SRP_EGarrisonSlot.BINOME;
			}
		}

		// Parts du cercle des sections (comme SpawnGroups) : chacune la sienne, réparties autour du centre
		int slices = 0;
		foreach (SRP_GarrisonSlot sliced : plan)
		{
			if (sliced.m_iKind == SRP_EGarrisonSlot.SECTION || sliced.m_iKind == SRP_EGarrisonSlot.BINOME)
				slices++;
		}
		float spread = Math.Max(radius * 0.8, 60.0);
		float startAngle = Math.RandomFloat(0, Math.PI2);
		float slice = Math.PI2 / Math.Max(slices, 1);
		int sliceIndex = 0;

		array<vector> buildingsTaken = {};
		int placedSoldiers = 0;
		int placedGroups = 0;
		int entryGroups = 0;
		int patrols = 0;
		int buildings = 0;
		int capped = 0;
		int waiting = 0;
		int waitingGroups = 0;
		int deferred = 0;
		int fallbacks = 0;
		int refused = 0;
		string lastRefusal = "";
		foreach (SRP_GarrisonSlot slot : plan)
		{
			// Place : un groupe à la fois (le centre et les entrées passent en premier). Un centre qui ne tient pas est
			// réduit (section de 4, puis binôme) : la garnison garde toujours un centre et son groupe de commandement. Un
			// groupe sans place est différé tel quel, réessayé avec les autres (sa demande reste notée, file RE12).
			string refusal = capacity.Ask(demandKey, capClass, slot.m_iSize, 1, center, "territoire");
			if (!refusal.IsEmpty() && slot.m_iKind == SRP_EGarrisonSlot.CENTRE && capacity.GroupRoom(capClass, "territoire") >= 1)
			{
				int room = capacity.Room(capClass, "territoire");
				int oldSize = slot.m_iSize;
				int cut = ShrinkCentre(enemies, slot, room);
				if (cut > 0)
				{
					capped += cut;
					SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : centre de la garnison réduit à %2 soldat(s) au lieu de %3 (place des soldats ennemis : %4 libre(s) %5) ; le centre passe en premier", state.m_sName, slot.m_iSize, oldSize, room, SRP_CmdCapacity.ClassLabel(capClass)));
					refusal = capacity.Ask(demandKey, capClass, slot.m_iSize, 1, center, "territoire");
				}
			}
			if (!refusal.IsEmpty())
			{
				state.m_aDeferredSlots.Insert(slot);
				waiting += slot.m_iSize;
				waitingGroups++;
				lastRefusal = refusal;
				continue;
			}

			// Le point de pose : centre (anneau serré autour de son point clé), entrée (25 m autour), section (sa part du
			// cercle autour du point clé centre)
			vector anchor = center;
			float ringMin = 0;
			float ringMax = Math.Min(radius * s_fCenterRingFactor, s_fCenterRingMax);
			float angleMin = -1;
			float angleMax = -1;
			bool preferRoad = true;
			if (slot.m_iKind == SRP_EGarrisonSlot.CENTRE)
			{
				bool atHQ = twoKeys && slot.m_bAtHQ;
				if (twoKeys && centreSeized && !hqSeized)
					atHQ = true;	// point clé centre saisi, ville encore rouge : le centre se pose au QG
				if (twoKeys && hqSeized && !centreSeized)
					atHQ = false;	// point clé QG saisi : le QG se pose au centre
				if (atHQ)
					anchor = hqPoint;
			}
			else if (slot.m_iKind == SRP_EGarrisonSlot.ENTREE)
			{
				anchor = slot.m_vEntry;
				ringMax = ENTRY_SPREAD;
				preferRoad = false;
			}
			else
			{
				angleMin = startAngle + slice * sliceIndex + slice * 0.15;
				angleMax = startAngle + slice * sliceIndex + slice * 0.85;
				sliceIndex++;
				ringMin = spread * 0.35;
				ringMax = spread;
			}
			vector position;
			if (!SRP_Placement.FindSpawnPosition(anchor, ringMin, ringMax, players, preferRoad, false, position, playerMinDist, angleMin, angleMax))
			{
				if (deferIfHidden)
				{
					// Un joueur est dans le secteur : jamais de repli sur lui, le groupe est réessayé tel quel
					state.m_aDeferredSlots.Insert(slot);
					deferred++;
					continue;
				}
				// Repli : l'ancien tirage, jamais dans l'eau, près de la base ni à moins de 150 m d'un joueur
				float angle = Math.RandomFloat(0, Math.PI2);
				if (angleMax > angleMin)
					angle = Math.RandomFloat(angleMin, angleMax);
				float distance = Math.RandomFloat(ringMin, ringMax);
				vector guess = SRP_Placement.OnGround(anchor + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
				if (!SCR_WorldTools.FindEmptyTerrainPosition(position, guess, 20, 2, 2))
					position = guess;
				if (!SRP_Placement.IsAcceptableFallback(position, players, FALLBACK_PLAYER_DISTANCE))
				{
					refused++;
					continue;
				}
				fallbacks++;
			}

			SRP_EnemyGroup record = PlaceSlot(enemies, slot, position, center, anchor, radius, buildingsTaken);
			if (!record)
				continue;
			record.m_sZone = state.m_sName;
			SRP_CmdCapacity.Tag(record, capClass);
			outGroups.Insert(record);
			int soldiers = record.m_iExpected;
			if (soldiers <= 0)
				soldiers = slot.m_iSize;
			state.m_iPlannedSoldiers += soldiers;
			placedSoldiers += soldiers;
			placedGroups++;
			if (record.m_sHomeTask == "entrée")
				entryGroups++;
			else if (record.m_sHomeTask == "ronde")
				patrols++;
			else if (record.m_sHomeTask == "bâtiment")
				buildings++;
		}

		// Une ligne par pose (rien quand tout a été refusé par les plafonds : la ligne de plafond suffit)
		if (placedGroups > 0 || deferred > 0 || refused > 0)
		{
			string period = "jour";
			if (night)
				period = "nuit";
			string line = string.Format("%1 (%2, %3) : garnison, %4 de %5 soldat(s) en %6 groupe(s)", state.m_sName, sector.m_sKind, period, what, placedSoldiers, placedGroups);
			line += string.Format(" (commandement : %1 ; %2 poste(s) d'entrée, %3 ronde(s), %4 en bâtiment)", CommandLabel(outGroups, enemies), entryGroups, patrols, buildings);
			if (deferred > 0)
				line += string.Format(", %1 groupe(s) différé(s) (joueur dans le secteur)", deferred);
			if (fallbacks > 0)
				line += string.Format(", %1 posé(s) par repli (aucun point hors de vue, à 150 m au moins des joueurs)", fallbacks);
			if (refused > 0)
				line += string.Format(", %1 refusé(s) (repli dans l'eau, près de la base ou des joueurs)", refused);
			if (waitingGroups > 0)
				line += string.Format(", %1 groupe(s) en attente de place", waitingGroups);
			SRP_EnemyComponent.Journal("ENNEMI", line);
		}

		// Demande de place de la garnison : tout ce qui attend (file RE12), sinon retirée
		if (waiting > 0)
			capacity.NoteDemand(demandKey, capClass, waiting, waitingGroups, center);
		else
			capacity.ClearDemand(demandKey);
		if (capped > 0 || waiting > 0)
			LogCap(state, capped, waiting, capClass, lastRefusal);
		return placedSoldiers;
	}

	//------------------------------------------------------------------------------------------------
	//! Classe de place d'une garnison : COMBAT si la localité est attaquée (SRP_FrontEnemyComponent
	//! .IsLocalityUnderAttack), GARNISON sinon
	protected static int GarrisonClass(string locality)
	{
		SRP_FrontEnemyComponent frontEnemy = SRP_FrontEnemyComponent.GetInstance();
		if (frontEnemy && frontEnemy.IsLocalityUnderAttack(locality))
			return SRP_ECmdCapClass.COMBAT;
		return SRP_ECmdCapClass.GARNISON;
	}

	//------------------------------------------------------------------------------------------------
	//! Un point clé saisi : son carré est bleu (faux sans front prêt)
	protected static bool IsSeizedPoint(vector point)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return false;
		return front.IsBlueAt(point);
	}

	//------------------------------------------------------------------------------------------------
	//! Une garnison peut-elle être posée ou complétée ici ? Jamais dans une localité prise (tous ses points clés saisis,
	//! SRP_ZoneFall.IsLocalityTaken), ni dans une localité vidée par un décrochage (SRP_CmdManeuvers.IsLocalityEmptied, MA7)
	protected static bool MayGarrison(SRP_SectorState state)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsReady())
		{
			SRP_ZoneFall zoneFall = front.GetZoneFall();
			if (zoneFall && zoneFall.IsLocalityTaken(state.m_sName))
				return false;
		}
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers && maneuvers.IsLocalityEmptied(state.m_sName))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats que la pose ou le complément peut ajouter (C6) : prévus + voulus bornés par la part de la localité
	//! (SRP_CmdCapacity.GarrisonCap, 60 au plus), moins les prévus ; localité tenue au complet (MA11,
	//! SRP_CmdManeuvers.IsKeptFull) : bornés par le seul plafond de 60 (cap_garnison_max). Jamais une garnison posée
	//! réduite : on ne borne que ce qui s'ajoute.
	protected static int AllowedSoldiers(SRP_SectorState state, int wanted, bool topUp)
	{
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		int already = 0;
		if (topUp)
			already = Math.MaxInt(state.m_iPlannedSoldiers, 0);
		int decided = already + wanted;
		int cap = capacity.GarrisonCap(state.m_sName, decided);
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers && maneuvers.IsKeptFull(state.m_sName))
			cap = Math.MinInt(decided, capacity.GetGarrisonMax());
		return Math.MinInt(wanted, cap - already);
	}

	//------------------------------------------------------------------------------------------------
	//! Un centre qui ne tient pas dans la place des soldats (« room » places libres, SRP_CmdCapacity.Room de sa classe) :
	//! une section plus petite (6, puis 4), sinon un binôme, qui reste le centre (groupe de commandement). Retourne les
	//! soldats retirés du plan (0 : rien de plus petit ne tient, le centre attend tel quel).
	protected static int ShrinkCentre(SRP_EnemyComponent enemies, SRP_GarrisonSlot slot, int room)
	{
		int oldSize = slot.m_iSize;
		ResourceName smaller = "";
		if (room >= 6 && oldSize > 6)
			smaller = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), 6, 6);
		if (smaller.IsEmpty() && room >= 4 && oldSize > 4)
			smaller = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), 4, 4);
		if (smaller.IsEmpty() && room >= 4 && oldSize > 4)
			smaller = enemies.PickGroupPrefabOfSize(4, 4);
		if (smaller.IsEmpty() && room >= 2 && oldSize > 2)
			smaller = PickPair(enemies);
		if (smaller.IsEmpty())
			return 0;
		int size = enemies.GroupSize(smaller);
		if (size <= 0 || size > room || size >= oldSize)
			return 0;
		slot.m_sPrefab = smaller;
		slot.m_iSize = size;
		return oldSize - size;
	}

	//------------------------------------------------------------------------------------------------
	//! Tâche d'origine d'un groupe compté dans les soldats prévus d'une garnison (posé par ce plan, ou assaillant passé
	//! en garnison) ; les débarqués d'un camion, le chauffeur et la jeep n'en sont pas
	static bool IsPlanTask(string task)
	{
		return task == "centre" || task == "entrée" || task == "ronde" || task == "bâtiment" || task == "poste" || task == "poste de nuit";
	}

	//------------------------------------------------------------------------------------------------
	//! Survivants du plan de la garnison : soldats vivants et conscients des groupes dont la tâche d'origine est du plan
	//! (IsPlanTask), plus les soldats que la file d'apparition doit encore livrer et ceux d'une pose abandonnée pas encore
	//! sortie de la liste (DropAbandoned) : ce ne sont pas des pertes — pertes F12 (SRP_FrontEnemyComponent.LiveLosses),
	//! seuil de décrochage du Commandeur (MA5)
	static int CountPlanSurvivors(SRP_SectorState state)
	{
		if (!state)
			return 0;
		int survivors = 0;
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (!record || !IsPlanTask(record.m_sHomeTask))
				continue;
			if (record.m_bAbandoned)
			{
				survivors += Math.MaxInt(record.m_iExpected, 0);
				continue;
			}
			if (!record.m_Group || record.m_Group.IsDeleted())
				continue;
			SRP_EnemyComponent.NoteSeen(record);
			survivors += SRP_EnemyComponent.FitOf(record);
			if (!record.m_bSpawnClosed && !record.m_bAbandoned)
				survivors += Math.MaxInt(record.m_iExpected - record.m_iMaxSeen, 0);
		}
		return survivors;
	}

	//------------------------------------------------------------------------------------------------
	//! Le QG de la ville : le groupe de state.m_aGroups qui tient le commandement (m_bCommand) avec le prefab du groupe de
	//! commandement (même test que CommandLabel) ; null hors d'une ville, ou si le QG n'a pas été posé — Commandeur
	//! (décrochage, MA5 et MA6 : le QG reste)
	static SRP_EnemyGroup FindTownHQ(SRP_EnemyComponent enemies, SRP_SectorState state)
	{
		if (!enemies || !state || !state.m_Sector || state.m_Sector.m_sKind != "ville")
			return null;
		ResourceName hqPrefab = enemies.GetCommandGroupPrefab();
		if (hqPrefab.IsEmpty())
			return null;
		foreach (SRP_EnemyGroup record : state.m_aGroups)
		{
			if (record && record.m_bCommand && IsPrefab(record, hqPrefab))
				return record;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe qui tient le centre (tâche d'origine « centre » : le QG ou la section du centre) — Commandeur
	static bool IsCentreGroup(SRP_EnemyGroup record)
	{
		return record && record.m_sHomeTask == "centre";
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe a-t-il été posé avec ce prefab ?
	protected static bool IsPrefab(SRP_EnemyGroup record, ResourceName prefab)
	{
		if (!record || !record.m_Group || prefab.IsEmpty())
			return false;
		EntityPrefabData data = record.m_Group.GetPrefabData();
		return data && data.GetPrefabName() == prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Les groupes de la garnison dont la pose a été abandonnée (personne livré) ou que le jeu a évincés (limite d'IA
	//! actives) sont sortis de la liste, et leurs soldats de ses soldats prévus : le complément pourra les reposer, hors
	//! de vue. Retourne le nombre de groupes sortis.
	static int DropAbandoned(SRP_SectorState state)
	{
		if (!state)
			return 0;
		int dropped = 0;
		for (int i = state.m_aGroups.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup record = state.m_aGroups[i];
			if (!record || !record.m_bAbandoned)
				continue;
			if (IsPlanTask(record.m_sHomeTask))
			{
				state.m_iPlannedSoldiers -= record.m_iExpected;
				if (state.m_iPlannedSoldiers < 0)
					state.m_iPlannedSoldiers = 0;
			}
			state.m_aGroups.Remove(i);
			dropped++;
		}
		return dropped;
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe de commandement de la garnison, pour le journal : « QG », « section », « QG et section du centre » (ville
	//! à deux points clés) ou « aucun »
	protected static string CommandLabel(array<ref SRP_EnemyGroup> groups, SRP_EnemyComponent enemies)
	{
		ResourceName hqPrefab = enemies.GetCommandGroupPrefab();
		bool hasHQ = false;
		bool hasSection = false;
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_bCommand)
				continue;
			if (IsPrefab(record, hqPrefab))
				hasHQ = true;
			else
				hasSection = true;
		}
		if (hasHQ && hasSection)
			return "QG et section du centre";
		if (hasHQ)
			return "QG";
		if (hasSection)
			return "section";
		return "aucun";
	}

	//------------------------------------------------------------------------------------------------
	//! Place refusée pendant une pose (SRP_CmdCapacity) : une ligne ENNEMI par minute au plus, avec l'état de la place
	protected static void LogCap(SRP_SectorState state, int capped, int waiting, int capClass, string refusal)
	{
		int now = System.GetTickCount();
		if (now < s_iNextCapLogTick)
			return;
		s_iNextCapLogTick = now + CAP_LOG_MS;
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string line = string.Format("%1 : garnison réduite faute de place (%2 soldat(s) retirés du centre, %3 en attente, classe %4", state.m_sName, capped, waiting, SRP_CmdCapacity.ClassLabel(capClass));
		line += string.Format(" : %1 place(s) libre(s), %2 élément(s)", capacity.Room(capClass, "territoire"), capacity.GroupRoom(capClass, "territoire"));
		if (!refusal.IsEmpty())
			line += " ; " + refusal;
		SRP_EnemyComponent.Journal("ENNEMI", line + ") ; le centre et les postes d'entrée passent en premier, le reste est réessayé quand la place se libère");
	}

	//------------------------------------------------------------------------------------------------
	//! Pose un groupe du plan à « position » et lui donne sa tâche : centre (Defend sur tout le rayon autour de son point
	//! clé « anchor », commandement), entrée (Defend s_fEntryDefendRadius sur la route, maintien s_fEntryHoldRadius), ronde
	//! (cycle sur les rues ; jamais un binôme), bâtiment (Defend s_fBuildingDefendRadius au pied d'un bâtiment, maintien
	//! s_fBuildingHoldRadius) ou poste (Defend sur place, maintien min(s_fPostHoldMax, rayon)). Une seule tâche par groupe.
	//! Part des rondes : toujours celle du jour (la posture de nuit est prise ensuite par SwitchPosture, et rendue au jour).
	protected static SRP_EnemyGroup PlaceSlot(SRP_EnemyComponent enemies, SRP_GarrisonSlot slot, vector position, vector center, vector anchor, float radius, array<vector> buildingsTaken)
	{
		SRP_EnemyGroup record;
		if (slot.m_iKind == SRP_EGarrisonSlot.CENTRE)
		{
			record = enemies.SpawnGroup(position, true, anchor, "territoire", radius, vector.Zero, -1, false, slot.m_sPrefab);
			if (!record)
				return null;
			record.m_bCommand = true;	// il tient le centre ; il ne part aider que s'il est le seul groupe libre
			enemies.NoteHome(record, "centre", true);
			return record;
		}
		if (slot.m_iKind == SRP_EGarrisonSlot.ENTREE)
		{
			record = enemies.SpawnGroup(position, true, slot.m_vEntry, "territoire", s_fEntryDefendRadius, vector.Zero, -1, false, slot.m_sPrefab);
			if (!record)
				return null;
			enemies.ApplyRole(record, SRP_ECRXRole.GARNISON, slot.m_vEntry, s_fEntryHoldRadius);
			enemies.NoteHome(record, "entrée", true);
			return record;
		}

		// Une section : ronde sur les rues (jamais un binôme), sinon un bâtiment, sinon un poste
		if (slot.m_iKind == SRP_EGarrisonSlot.SECTION && Math.RandomFloat(0, 100) < s_iPatrolPercentDay)
		{
			record = enemies.SpawnGroup(position, true, center, "territoire", radius, center, radius, false, slot.m_sPrefab);
			if (!record)
				return null;
			// Un cycle de points de route ; sans rue, la patrouille de zone (PatrolTick) reste
			array<vector> circuit = {};
			int pointsMin = Math.Max(2, enemies.GetStreetPointsMin());
			int points = Math.RandomIntInclusive(pointsMin, Math.Max(pointsMin, enemies.GetStreetPointsMax()));
			if (SRP_Placement.BuildStreetCircuit(center, radius, points, circuit))
				enemies.OrderCycle(record, circuit);
			enemies.NoteHome(record, "ronde", false);
			return record;
		}

		vector spot;
		if (Math.RandomFloat(0, 100) < s_iBuildingPercent && SRP_Placement.FindBuildingSpot(center, radius, buildingsTaken, spot))
		{
			record = enemies.SpawnGroup(position, true, spot, "territoire", s_fBuildingDefendRadius, vector.Zero, -1, false, slot.m_sPrefab);
			if (!record)
				return null;
			buildingsTaken.Insert(spot);
			enemies.ApplyRole(record, SRP_ECRXRole.GARNISON, spot, s_fBuildingHoldRadius);
			enemies.NoteHome(record, "bâtiment", true);
			return record;
		}

		// Poste : le groupe tient son propre emplacement, pas tout le secteur
		float hold = Math.Min(s_fPostHoldMax, radius);
		record = enemies.SpawnGroup(position, true, position, "territoire", hold, vector.Zero, -1, false, slot.m_sPrefab);
		if (!record)
			return null;
		enemies.NoteHome(record, "poste", true);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	// Posture de nuit
	//------------------------------------------------------------------------------------------------
	//! Ordres seulement, aucune pose, le total ne change jamais. La nuit : les rondes en trop par rapport à la part de nuit
	//! (s_iPatrolPercentNight des sections), hors aide et ratissage, passent en poste à une entrée libre de la localité
	//! (sinon sur place). Le jour : ces postes de nuit reprennent leur ronde ; un poste de nuit parti aider ou ratisser
	//! retrouvera sa ronde à son retour.
	static void SwitchPosture(SRP_EnemyComponent enemies, SRP_SectorState state, bool night)
	{
		if (!enemies || !state || !state.m_Sector)
			return;
		state.m_bNightPosture = night;
		int now = System.GetTickCount();
		vector center = state.m_Sector.GetCenter();
		float radius = state.m_Sector.m_fRadius;
		int changed = 0;

		if (!night)
		{
			foreach (SRP_EnemyGroup record : state.m_aGroups)
			{
				if (!record || !record.m_bNightPost || !record.m_Group || record.m_Group.IsDeleted())
					continue;
				record.m_bNightPost = false;
				if (record.m_iBusyUntilTick > now)
					SetHomeRound(record);	// parti aider ou ratisser : RestoreHome lui rendra sa ronde
				else
					ResumeRound(enemies, record, center, radius);
				changed++;
			}
			if (changed > 0)
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : posture de jour, %2 poste(s) de nuit reprennent leur ronde", state.m_sName, changed));
			return;
		}

		// Nuit : les sections de la garnison (hors centre et entrées), les rondes disponibles, les entrées déjà tenues
		int sections = 0;
		array<SRP_EnemyGroup> rounds = {};
		array<vector> occupied = {};
		foreach (SRP_EnemyGroup member : state.m_aGroups)
		{
			if (!member || !member.m_Group || member.m_Group.IsDeleted() || member.m_Vehicle)
				continue;
			// Un groupe que la file d'apparition de la 1.8 n'a pas encore servi compte (garnison posée la nuit : la posture
			// de nuit est prise juste après la pose)
			if (SRP_EnemyComponent.AliveAgents(member) == 0 && !SRP_EnemyComponent.InGraceOf(member, now))
				continue;
			string home = member.m_sHomeTask;
			if (home == "entrée" || home == "poste de nuit")
				occupied.Insert(member.m_vHome);
			if (home == "ronde" || home == "poste" || home == "bâtiment" || home == "poste de nuit")
				sections++;
			if (home == "ronde" && member.m_sTask == "ronde" && member.m_iBusyUntilTick <= now)
				rounds.Insert(member);
		}
		int keep = Math.Round(sections * s_iPatrolPercentNight / 100.0);
		int excess = rounds.Count() - keep;
		if (excess <= 0)
			return;

		array<vector> entries = {};
		SRP_Placement.FindTownEntrances(center, radius, occupied.Count() + excess, entries);
		for (int i = 0; i < excess; i++)
		{
			SRP_EnemyGroup patrolRecord = rounds[i];
			vector post = enemies.GroupPosition(patrolRecord);
			foreach (vector entry : entries)
			{
				bool used = false;
				foreach (vector other : occupied)
				{
					if (vector.DistanceXZ(other, entry) < 40)
						used = true;
				}
				if (used)
					continue;
				post = entry;
				break;
			}
			occupied.Insert(post);
			NightPost(enemies, patrolRecord, post);
			changed++;
		}
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("%1 : posture de nuit, %2 ronde(s) sur %3 passent en poste aux entrées (part de nuit : %4 sur %5 sections)", state.m_sName, changed, rounds.Count(), keep, sections));
	}

	//------------------------------------------------------------------------------------------------
	//! Une ronde devient un poste de nuit : point Defend à « post », rôle de garnison, maintien d'un poste d'entrée ; son
	//! circuit est gardé pour le jour
	protected static void NightPost(SRP_EnemyComponent enemies, SRP_EnemyGroup record, vector post)
	{
		enemies.ClearWaypoints(record);
		record.m_bCycle = false;
		record.m_bPatrol = false;
		record.m_iSearchUntilTick = 0;
		record.m_iArrivedTick = 0;
		IEntity waypointEntity = enemies.SpawnDefendWaypoint(post, s_fEntryDefendRadius);
		AIWaypoint waypoint = AIWaypoint.Cast(waypointEntity);
		if (waypoint)
		{
			record.m_Group.AddWaypointToGroup(waypoint);
			record.m_Waypoint = waypointEntity;
		}
		else if (waypointEntity)
			SCR_EntityHelper.DeleteEntityAndChildren(waypointEntity);
		enemies.ApplyRole(record, SRP_ECRXRole.GARNISON, post, s_fEntryHoldRadius);
		record.m_bNightPost = true;
		enemies.NoteHome(record, "poste de nuit", true);
	}

	//------------------------------------------------------------------------------------------------
	//! Un poste de nuit reprend sa ronde : son cycle sur les rues, sinon la patrouille de zone (PatrolTick)
	protected static void ResumeRound(SRP_EnemyComponent enemies, SRP_EnemyGroup record, vector center, float radius)
	{
		if (record.m_aCircuit.Count() >= 2)
			enemies.OrderCycle(record, record.m_aCircuit);
		else
		{
			record.m_bPatrol = true;
			record.m_vPatrolCenter = center;
			record.m_fPatrolRadius = radius;
			enemies.OrderMove(record, center);
			record.m_iNextPatrolTick = System.GetTickCount() + 60000;
		}
		enemies.ApplyRole(record, SRP_ECRXRole.PATROUILLE, center, -1);
		enemies.NoteHome(record, "ronde", false);
	}

	//------------------------------------------------------------------------------------------------
	//! Un poste de nuit parti aider ou ratisser : son poste d'origine redevient sa ronde (RestoreHome la lui rendra)
	protected static void SetHomeRound(SRP_EnemyGroup record)
	{
		record.m_sHomeTask = "ronde";
		record.m_iHomeRole = SRP_ECRXRole.PATROUILLE;
		record.m_fHomeHold = -1;
		record.m_bHomeDefend = false;
		record.m_bHomeCycle = record.m_aCircuit.Count() >= 2;
		if (!record.m_bHomeCycle)
			record.m_bPatrol = true;
	}
}
