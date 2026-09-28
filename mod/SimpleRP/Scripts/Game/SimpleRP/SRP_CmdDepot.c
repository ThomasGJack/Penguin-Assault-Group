//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : LES DÉPÔTS ENNEMIS CACHÉS (RE8, RE9).
//
// RÔLE (serveur ; SRP_CmdDepots est tenu par SRP_CmdResources.m_Depots) : un dépôt caché par région, en zone rouge
// loin du bleu (res_depot_distance_bleu_m) et des points clés, près d'une route, posé hors de vue (décor
// res_depot_prefab = SRP_Cache.et, qui porte déjà l'action « Saboter », aucun prefab neuf).
// - Saboté (action du joueur, par SRP_Commander.OnSabotageUsed -> TryUse) : revenu de la région coupé
//   (SRP_CmdResources.CutRegion), radio SRP_FrontRadio.EnemyDepotSabotaged (évènement depot_ennemi), alerte de
//   région, puis le dépôt change de place, caché (RE9).
// - Zone du dépôt passée bleue : sabotage « zone prise » (même suite, radio comprise) ; zone FORCÉE bleue par le
//   Staff (Q9) : le dépôt change de place en silence (comme StaffMove).
// - Révélé (RE9) par SRP_CmdIntel (PC saisi, officier capturé, mission ECOUTE ou DOCUMENTS) ; un dépôt en cours de
//   déplacement est révélé dès sa pose ; la carte montre les dépôts révélés (SRP_FrontMarkers.TickEnemyDepots).
// - Gardes posées à l'approche d'un joueur (res_depot_gardes_distance_m) comme celles des missions : place par
//   SRP_CmdCapacity.Ask(GARDE, « depot-R3 »), classe GARDE, rôle GARDE_DEPOT ; retirées hors de vue ensuite.
// Un décor qui n'est plus le dépôt (saboté, déplacé, région libérée) est effacé HORS DE VUE (aucun joueur à 500 m,
// personne ne le voit) : il reste en attendant, et « Saboter » dessus ne fait plus rien.
// Sauvegarde : clés cmd_dep_* (WriteTo / ReadFrom, par code de région) ; au chargement le décor est reposé.
// APPELÉ PAR : SRP_CmdResources (vie, Tick de 60 s, zone changée, sauvegarde), SRP_Commander (TryUse),
// SRP_CmdIntel (Reveal), SRP_CmdScreens (Staff), SRP_FrontMarkers (GetRevealedDepots).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Le dépôt d'une région. Champs « sauvé » : clés cmd_dep_rN_*.
class SRP_CmdDepot
{
	int m_iRegion = -1;
	string m_sCode;								// code de la région (sauvé)
	vector m_vPos;								// vector.Zero = à reposer (sauvé)
	int m_iZone = -1;							// sauvé (par code de zone)
	int m_iPrevZone = -1;						// zone du dépôt précédent, évitée (sauvé)
	bool m_bRevealed;							// sauvé
	string m_sRevealedBy;						// « PC ennemi saisi à Perelle (S08) » (sauvé)
	bool m_bRevealPending;						// révéler dès la prochaine pose (sauvé)
	int m_iMoves;								// déplacements (sauvé)
	// En jeu, non sauvé
	IEntity m_Entity;							// décor posé
	IEntity m_OldEntity;						// décor saboté, effacé hors de vue
	ref array<ref SRP_EnemyGroup> m_aGuards = {};	// toujours des ref
	int m_iQuietSince;							// plus aucun joueur à res_depot_gardes_retrait_m depuis (heure Unix)
	int m_iNextTryAt;							// prochaine tentative de pose (heure Unix)
	vector m_vGuardPos;							// où les gardes ont été posées (le dépôt a pu bouger depuis)
}

//------------------------------------------------------------------------------------------------
//! Les dépôts ennemis (serveur).
class SRP_CmdDepots
{
	// --- Réglages (clés res_depot_*) -----------------------------------------------------------------------------
	int m_iDepotBlueM = 1000;		// res_depot_distance_bleu_m
	int m_iDepotKeyPointM = 400;		// res_depot_distance_point_cle_m
	int m_iDepotRoadM = 150;		// res_depot_route_m
	int m_iDepotPlaceM = 1500;		// res_depot_distance_pose_m
	int m_iDepotGuardsM = 1500;		// res_depot_gardes_distance_m
	int m_iDepotGuardsMax = 2;		// res_depot_gardes_max
	int m_iDepotGuardsRetireM = 2500;		// res_depot_gardes_retrait_m
	int m_iDepotGuardsRetireMin = 10;		// res_depot_gardes_retrait_min
	string m_sDepotPrefab = "{361B78F0EC88F339}Prefabs/Props/Military/SupplyBox/SupplyStack/SupplyStack_Large_01/SRP_Cache.et";		// res_depot_prefab

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected SRP_CmdResources m_Resources;
	protected ref array<ref SRP_CmdDepot> m_aDepots = {};		// un par région
	protected ref array<IEntity> m_aTrash = {};					// décors qui ne sont plus des dépôts, effacés hors de vue
	protected ref array<vector> m_aBlue = {};					// centres des carrés bleus (choix d'un site)
	protected int m_iBlueVersion = -1;							// version d'état du front de m_aBlue

	protected static const int SITE_DRAWS = 40;					// carrés tirés par recherche de site
	protected static const float ROAD_SIDE_M = 20;				// décalage du bord de route
	protected static const float TRASH_CLEAR_M = 500;			// aucun joueur à cette distance pour effacer un décor
	protected static const float RESPAWN_CLEAR_M = 150;			// décor absent reposé à sa place : aucun joueur plus près
	protected static const int GUARD_SOLDIERS = 5;				// soldats comptés par section de garde (demande de place)
	protected static const int RETRY_S = 60;					// nouvel essai de pose

	//------------------------------------------------------------------------------------------------
	//! Constructeur seul — SRP_CmdResources (constructeur)
	void SRP_CmdDepots(SRP_CmdResources resources)
	{
		m_Resources = resources;
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés — SRP_CmdResources.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("res_depot_distance_bleu_m", m_iDepotBlueM, "Dépôt ennemi : à cette distance au moins d'un carré bleu, en mètres (RE8)");
		SRP_CmdSettings.DeclareInt("res_depot_distance_point_cle_m", m_iDepotKeyPointM, "Dépôt ennemi : à cette distance au moins d'un point clé, en mètres (RE8)");
		SRP_CmdSettings.DeclareInt("res_depot_route_m", m_iDepotRoadM, "Dépôt ennemi : route cherchée à cette distance, en mètres (RE8)");
		SRP_CmdSettings.DeclareInt("res_depot_distance_pose_m", m_iDepotPlaceM, "Dépôt ennemi : jamais posé plus près d'un joueur, en mètres (RE8)");
		SRP_CmdSettings.DeclareInt("res_depot_gardes_distance_m", m_iDepotGuardsM, "Gardes du dépôt posées quand un joueur est à cette distance, en mètres (RE9)");
		SRP_CmdSettings.DeclareInt("res_depot_gardes_max", m_iDepotGuardsMax, "Sections de garde du dépôt, au plus (RE9)");
		SRP_CmdSettings.DeclareInt("res_depot_gardes_retrait_m", m_iDepotGuardsRetireM, "Retrait des gardes quand plus aucun joueur n'est à cette distance, en mètres (RE9)");
		SRP_CmdSettings.DeclareInt("res_depot_gardes_retrait_min", m_iDepotGuardsRetireMin, "Depuis ce nombre de minutes (RE9)");
		SRP_CmdSettings.DeclareString("res_depot_prefab", m_sDepotPrefab, "Décor du dépôt (porte déjà l'action Saboter) (RE8)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés — SRP_CmdResources.LoadSettings
	void LoadSettings()
	{
		m_iDepotBlueM = SRP_CmdSettings.GetIntClamped("res_depot_distance_bleu_m", 0, 5000);
		m_iDepotKeyPointM = SRP_CmdSettings.GetIntClamped("res_depot_distance_point_cle_m", 0, 3000);
		m_iDepotRoadM = SRP_CmdSettings.GetIntClamped("res_depot_route_m", 0, 1000);
		m_iDepotPlaceM = SRP_CmdSettings.GetIntClamped("res_depot_distance_pose_m", 0, 5000);
		m_iDepotGuardsM = SRP_CmdSettings.GetIntClamped("res_depot_gardes_distance_m", 100, 5000);
		m_iDepotGuardsMax = SRP_CmdSettings.GetIntClamped("res_depot_gardes_max", 0, 6);
		m_iDepotGuardsRetireM = SRP_CmdSettings.GetIntClamped("res_depot_gardes_retrait_m", 100, 10000);
		m_iDepotGuardsRetireMin = SRP_CmdSettings.GetIntClamped("res_depot_gardes_retrait_min", 0, 120);
		m_sDepotPrefab = SRP_CmdSettings.GetString("res_depot_prefab");
	}

	//------------------------------------------------------------------------------------------------
	//! Un dépôt par région (ceux relus par ReadFrom gardés) — SRP_CmdResources.Start
	void Start(int regions)
	{
		EnsureDepots(regions);
		int toPlace = 0;
		int revealed = 0;
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			depot.m_iNextTryAt = 0;		// premier passage : pose ou repose du décor
			if (depot.m_vPos == vector.Zero)
				toPlace++;
			if (depot.m_bRevealed)
				revealed++;
		}
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Dépôts ennemis : %1 région(s), %2 à poser, %3 révélé(s)", m_aDepots.Count(), toPlace, revealed));
	}

	//------------------------------------------------------------------------------------------------
	//! Arrêt (rien à écrire) — SRP_CmdResources.Stop
	void Stop()
	{
		// Les décors partent avec le monde : on oublie seulement les listes de travail
		m_aTrash.Clear();
		m_aBlue.Clear();
		m_iBlueVersion = -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne ou remise : décors effacés hors de vue, gardes retirées (RetireLater), tout est à reposer —
	//! SRP_CmdResources.ResetCampaign
	void Reset(string author)
	{
		EnsureDepots(BookRegionCount());
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			Trash(depot.m_Entity);
			Trash(depot.m_OldEntity);
			if (enemies && !depot.m_aGuards.IsEmpty())
				enemies.RetireLater(depot.m_aGuards);
			depot.m_aGuards.Clear();
			depot.m_iQuietSince = 0;
			depot.m_vGuardPos = vector.Zero;
			ClearState(depot);
		}
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Dépôts ennemis remis à zéro (%1) : tous à reposer, cachés", author));
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 60 s, par dépôt : région sans zone rouge -> effacé ; à reposer -> PickSite puis Place ; ancien
	//! décor effacé hors de vue ; décor absent après redémarrage -> reposé ; gardes à l'approche ou retirées —
	//! SRP_CmdResources.Tick
	void Tick(int nowUnix)
	{
		if (m_aDepots.Count() != BookRegionCount())
			EnsureDepots(BookRegionCount());
		TrashTick();
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			int red = 0;
			if (m_Resources)
				red = m_Resources.GetRedZones(depot.m_iRegion);
			if (red <= 0)
			{
				// OF11 : région sans zone rouge, plus de dépôt (il reviendra si la région renaît)
				if (depot.m_vPos != vector.Zero || depot.m_Entity != null)
				{
					Trash(depot.m_Entity);
					depot.m_Entity = null;
					if (depot.m_iZone >= 0)
						depot.m_iPrevZone = depot.m_iZone;
					depot.m_vPos = vector.Zero;
					depot.m_iZone = -1;
					depot.m_bRevealed = false;
					depot.m_bRevealPending = false;
					depot.m_sRevealedBy = "";
					SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 retiré : plus aucune zone rouge", RegionLabel(depot.m_iRegion)));
					Dirty(false);
				}
				GuardsTick(depot, nowUnix);
				continue;
			}

			if (depot.m_vPos == vector.Zero)
			{
				if (nowUnix >= depot.m_iNextTryAt)
				{
					vector site;
					int siteZone;
					bool placed = false;
					if (PickSite(depot.m_iRegion, site, siteZone))
						placed = Place(depot, site, siteZone, nowUnix);
					if (!placed)
						depot.m_iNextTryAt = nowUnix + RETRY_S;
				}
			}
			else if (!depot.m_Entity || depot.m_Entity.IsDeleted())
			{
				Respawn(depot);
			}
			GuardsTick(depot, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! En tête de OnObjectiveUsed (par SRP_Commander.OnSabotageUsed) : l'entité est-elle le décor d'un dépôt ? Si oui,
	//! Sabotage(nom du joueur, « sabotage ») et message au joueur ; vrai = consommé
	bool TryUse(IEntity entity, int playerId, out string message)
	{
		message = "";
		if (!entity)
			return false;
		IEntity root = entity;
		while (root.GetParent())
			root = root.GetParent();
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			if (!depot.m_Entity)
				continue;
			if (depot.m_Entity != entity && depot.m_Entity != root)
				continue;
			string author = GetGame().GetPlayerManager().GetPlayerName(playerId);
			int hours = 0;
			if (m_Resources)
				hours = m_Resources.m_iDepotCutHours;
			Sabotage(depot, author, "sabotage");
			message = string.Format("Dépôt ennemi saboté : le ravitaillement de cette région est réduit pendant %1 h.", hours);
			return true;
		}
		// Un ancien dépôt pas encore effacé : plus rien à saboter (la mission, elle, ne le connaît pas)
		if (m_aTrash.Contains(entity) || m_aTrash.Contains(root))
		{
			message = "Ce dépôt ennemi est déjà saboté.";
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! RE9 : dépôt posé et caché -> révélé (m_sRevealedBy = reason) ; en déplacement -> m_bRevealPending ; déjà révélé
	//! -> rien. Les annonces sont faites par SRP_CmdIntel (IntelGained) ; rend la référence du carré (« 074 042 »),
	//! "" si rien n'est encore posé — SRP_CmdIntel.Grant, SRP_CmdScreens (depot-reveler:R3, sans radio)
	string Reveal(int region, string reason)
	{
		SRP_CmdDepot depot = GetDepot(region);
		if (!depot)
			return "";
		if (depot.m_vPos == vector.Zero)
		{
			// en déplacement : révélé dès sa pose
			if (!depot.m_bRevealPending)
			{
				depot.m_bRevealPending = true;
				depot.m_sRevealedBy = reason;
				SRP_CmdLog.Note(region, SRP_ECmdLogKind.RENSEIGNEMENT, string.Format("Dépôt de la %1 en déplacement : il sera révélé dès sa pose (%2)", RegionLabel(region), reason));
				Dirty(false);
			}
			return "";
		}
		string cellRef = CellRefAt(depot.m_vPos);
		if (depot.m_bRevealed)
			return cellRef;
		depot.m_bRevealed = true;
		depot.m_sRevealedBy = reason;
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.RENSEIGNEMENT, string.Format("Dépôt de la %1 révélé au carré %2 (%3)", RegionLabel(region), cellRef, reason));
		Dirty(false);
		return cellRef;
	}

	//------------------------------------------------------------------------------------------------
	//! Le dépôt de la région est connu des joueurs — écrans, carte
	bool IsRevealed(int region)
	{
		SRP_CmdDepot depot = GetDepot(region);
		if (!depot)
			return false;
		return depot.m_bRevealed && depot.m_vPos != vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Position du dépôt de la région (vector.Zero si rien n'est posé) — Staff (téléportation), carte
	vector GetDepotPos(int region)
	{
		SRP_CmdDepot depot = GetDepot(region);
		if (!depot)
			return vector.Zero;
		return depot.m_vPos;
	}

	//------------------------------------------------------------------------------------------------
	//! Dépôts révélés : régions et positions ; rend leur nombre — SRP_FrontMarkers.TickEnemyDepots, PC
	int GetRevealedDepots(notnull array<int> regions, notnull array<vector> positions)
	{
		regions.Clear();
		positions.Clear();
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			if (!depot.m_bRevealed || depot.m_vPos == vector.Zero)
				continue;
			regions.Insert(depot.m_iRegion);
			positions.Insert(depot.m_vPos);
		}
		return regions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Une zone a changé de camp : zone d'un dépôt passée BLEU -> Sabotage(« », « zone prise ») (radio, évènement
	//! depot_ennemi, coupure du revenu) ; staff (Q9, zone forcée) : le dépôt change de place EN SILENCE, comme
	//! StaffMove (ni CutRegion, ni radio, ni depot_ennemi, ni alerte) — SRP_CmdResources.OnZoneOwnerChanged
	void OnZoneOwnerChanged(int zone, int owner, bool staff)
	{
		if (owner != SRP_EFrontOwner.BLEU || zone < 0)
			return;
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			if (depot.m_vPos == vector.Zero || depot.m_iZone != zone)
				continue;
			if (staff)
				Relocate(depot, "zone forcée bleue par le Staff");
			else
				Sabotage(depot, "", "zone prise");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : le dépôt de la région change de place (caché), sans coupure — SRP_CmdScreens (depot-deplacer:R3),
	//! OnZoneOwnerChanged (staff)
	void StaffMove(int region, string author)
	{
		SRP_CmdDepot depot = GetDepot(region);
		if (!depot)
			return;
		Relocate(depot, "déplacé par " + author);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : sabotage forcé (même suite qu'un vrai) — SRP_CmdScreens (depot-saboter:R3, 2 clics)
	void StaffSabotage(int region, string author)
	{
		SRP_CmdDepot depot = GetDepot(region);
		if (!depot)
			return;
		Sabotage(depot, author, "sabotage forcé par le Staff");
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : coupure du revenu levée — SRP_CmdScreens (coupure-lever:R3)
	void StaffClearCut(int region, string author)
	{
		if (!m_Resources)
			return;
		m_Resources.ClearCut(region, author);
	}

	//------------------------------------------------------------------------------------------------
	//! Par région : carré, révélé ou non (par qui), déplacements, coupure restante — SRP_CmdScreens
	string GetStaffReport()
	{
		string text = "";
		SRP_CmdRegionBook book = Book();
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			string name = depot.m_sCode;
			if (book && depot.m_iRegion < book.GetRegionCount())
				name = depot.m_sCode + " " + book.GetRegionName(depot.m_iRegion);
			string part = name + " : ";
			if (depot.m_vPos == vector.Zero)
			{
				part = part + "à poser";
				if (depot.m_bRevealPending)
					part = part + " (révélé dès la pose : " + depot.m_sRevealedBy + ")";
			}
			else
			{
				part = part + "carré " + CellRefAt(depot.m_vPos) + ", " + ZoneLabel(depot.m_iZone);
				if (depot.m_bRevealed)
					part = part + ", révélé (" + depot.m_sRevealedBy + ")";
				else
					part = part + ", caché";
				if (!depot.m_Entity || depot.m_Entity.IsDeleted())
					part = part + ", décor à reposer";
			}
			part = part + string.Format(", %1 déplacement(s)", depot.m_iMoves);
			int cutLeft = 0;
			if (m_Resources)
				cutLeft = m_Resources.GetCutSecondsLeft(depot.m_iRegion);
			if (cutLeft > 0)
				part = part + ", revenu coupé encore " + SRP_CmdResources.DurationWords(cutLeft);
			if (!depot.m_aGuards.IsEmpty())
				part = part + string.Format(", %1 section(s) de garde", depot.m_aGuards.Count());
			else if (depot.m_vGuardPos != vector.Zero)
				part = part + ", gardes tombées (pas de nouvelle garde avant le départ des joueurs)";
			if (!text.IsEmpty())
				text = text + " · ";
			text = text + part;
		}
		if (text.IsEmpty())
			return "Dépôts ennemis : aucune région tracée";
		return "Dépôts ennemis : " + text;
	}

	//------------------------------------------------------------------------------------------------
	//! cmd_dep_n, puis cmd_dep_rN_code, _x, _y, _z, _zone (code), _prev (code), _revele, _par, _attente, _moves —
	//! SRP_CmdResources.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		ctx.WriteValue("cmd_dep_n", m_aDepots.Count());
		foreach (int i, SRP_CmdDepot depot : m_aDepots)
		{
			string prefix = "cmd_dep_r" + i.ToString() + "_";
			float x = depot.m_vPos[0];
			float y = depot.m_vPos[1];
			float z = depot.m_vPos[2];
			ctx.WriteValue(prefix + "code", depot.m_sCode);
			ctx.WriteValue(prefix + "x", x);
			ctx.WriteValue(prefix + "y", y);
			ctx.WriteValue(prefix + "z", z);
			ctx.WriteValue(prefix + "zone", ZoneCode(depot.m_iZone));
			ctx.WriteValue(prefix + "prev", ZoneCode(depot.m_iPrevZone));
			ctx.WriteValue(prefix + "revele", depot.m_bRevealed);
			ctx.WriteValue(prefix + "par", depot.m_sRevealedBy);
			ctx.WriteValue(prefix + "attente", depot.m_bRevealPending);
			ctx.WriteValue(prefix + "moves", depot.m_iMoves);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture miroir par code ; zone qui n'est plus rouge : le dépôt change de place sans coupure —
	//! SRP_CmdResources.ReadFrom
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		EnsureDepots(BookRegionCount());
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();

		// État en mémoire mis de côté (restauration d'une copie : un décor qui ne correspond plus est effacé hors de vue)
		array<IEntity> previous = {};
		array<vector> previousPos = {};
		foreach (SRP_CmdDepot current : m_aDepots)
		{
			previous.Insert(current.m_Entity);
			previousPos.Insert(current.m_vPos);
			ClearState(current);
		}

		int count = SRP_CmdResources.JsonInt(ctx, "cmd_dep_n", 0);
		for (int i = 0; i < count; i++)
		{
			string prefix = "cmd_dep_r" + i.ToString() + "_";
			SRP_CmdDepot depot = FindByCode(SRP_CmdResources.JsonString(ctx, prefix + "code", ""));
			if (!depot)
				continue;
			float x = SRP_CmdResources.JsonFloat(ctx, prefix + "x", 0);
			float y = SRP_CmdResources.JsonFloat(ctx, prefix + "y", 0);
			float z = SRP_CmdResources.JsonFloat(ctx, prefix + "z", 0);
			depot.m_vPos = Vector(x, y, z);
			depot.m_iZone = ZoneOfCode(SRP_CmdResources.JsonString(ctx, prefix + "zone", ""));
			depot.m_iPrevZone = ZoneOfCode(SRP_CmdResources.JsonString(ctx, prefix + "prev", ""));
			depot.m_bRevealed = SRP_CmdResources.JsonBool(ctx, prefix + "revele", false);
			depot.m_sRevealedBy = SRP_CmdResources.JsonString(ctx, prefix + "par", "");
			depot.m_bRevealPending = SRP_CmdResources.JsonBool(ctx, prefix + "attente", false);
			depot.m_iMoves = SRP_CmdResources.JsonInt(ctx, prefix + "moves", 0);
			if (depot.m_vPos == vector.Zero)
			{
				depot.m_iZone = -1;
				depot.m_bRevealed = false;
				continue;
			}
			// Zone qui n'est plus rouge (ou inconnue) : le dépôt change de place, caché, sans coupure
			if (!front || depot.m_iZone < 0 || front.GetZoneOwner(depot.m_iZone) != SRP_EFrontOwner.ROUGE)
			{
				if (depot.m_iZone >= 0)
					depot.m_iPrevZone = depot.m_iZone;
				depot.m_vPos = vector.Zero;
				depot.m_iZone = -1;
				depot.m_bRevealed = false;
				depot.m_bRevealPending = false;
				depot.m_sRevealedBy = "";
				depot.m_iMoves = depot.m_iMoves + 1;
				SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 relu sur une zone qui n'est plus rouge : il change de place, caché", RegionLabel(depot.m_iRegion)));
			}
		}

		// Décors déjà posés : gardés s'ils sont toujours à la place relue, sinon effacés hors de vue
		foreach (int k, SRP_CmdDepot placed : m_aDepots)
		{
			if (k >= previous.Count())
				break;
			IEntity old = previous[k];
			if (!old || old.IsDeleted())
				continue;
			if (placed.m_vPos != vector.Zero && vector.Distance(placed.m_vPos, previousPos[k]) < 1)
				placed.m_Entity = old;
			else
				Trash(old);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Candidats : zones rouges de la région hors contact (sinon au contact), autres que la précédente ; 40 carrés
	//! tirés, gardés à res_depot_distance_bleu_m du bleu, res_depot_distance_point_cle_m des points clés, hors base,
	//! route à res_depot_route_m préférée — Tick
	protected bool PickSite(int region, out vector position, out int zone)
	{
		position = vector.Zero;
		zone = -1;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdRegionBook book = Book();
		SRP_CmdDepot depot = GetDepot(region);
		if (!front || !book || !depot)
			return false;

		// Zones candidates : rouges hors contact d'abord, puis au contact ; la zone précédente en dernier recours
		array<int> regionZones = {};
		book.GetRegionZones(region, regionZones);
		array<int> quiet = {};
		array<int> contact = {};
		array<int> lastResort = {};
		foreach (int candidateZone : regionZones)
		{
			if (front.GetZoneOwner(candidateZone) != SRP_EFrontOwner.ROUGE || front.IsBaseZone(candidateZone))
				continue;
			if (candidateZone == depot.m_iPrevZone || candidateZone == depot.m_iZone)
				lastResort.Insert(candidateZone);
			else if (front.IsZoneInContact(candidateZone))
				contact.Insert(candidateZone);
			else
				quiet.Insert(candidateZone);
		}
		array<int> chosen = {};
		if (!quiet.IsEmpty())
			chosen.Copy(quiet);
		else if (!contact.IsEmpty())
			chosen.Copy(contact);
		else
			chosen.Copy(lastResort);
		if (chosen.IsEmpty())
			return false;

		// Carrés rouges de ces zones
		array<int> cells = {};
		array<int> zoneCells = {};
		foreach (int chosenZone : chosen)
		{
			zoneCells.Clear();
			front.GetZoneCells(chosenZone, zoneCells);
			foreach (int zoneCell : zoneCells)
			{
				if (front.GetCellOwner(zoneCell) == SRP_EFrontOwner.ROUGE && !front.IsBaseCell(zoneCell))
					cells.Insert(zoneCell);
			}
		}
		if (cells.IsEmpty())
			return false;

		BuildBlueCache(front);
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		bool hasFallback = false;
		vector fallbackPos = vector.Zero;
		int fallbackZone = -1;
		int accepted = 0;
		for (int draw = 0; draw < SITE_DRAWS && !cells.IsEmpty(); draw++)
		{
			int pick = Math.RandomInt(0, cells.Count());
			int drawnCell = cells[pick];
			cells.Remove(pick);
			vector center = front.CellCenter(drawnCell);
			if (!IsSiteOk(center, front, players))
				continue;
			accepted++;

			// Préférence : près d'une route (res_depot_route_m), décalé sur le bas-côté
			vector road;
			if (m_iDepotRoadM > 0 && SRP_Placement.FindRoadPoint(center, m_iDepotRoadM, road))
			{
				float angle = Math.RandomFloat(0, Math.PI2);
				vector side = road + Vector(Math.Cos(angle) * ROAD_SIDE_M, 0, Math.Sin(angle) * ROAD_SIDE_M);
				vector roadSpot = GroundSpot(side);
				if (IsSiteOk(roadSpot, front, players) && IsRegionRed(roadSpot, region, front, book))
				{
					position = roadSpot;
					zone = front.GetZoneAt(roadSpot);
					return true;
				}
			}
			if (!hasFallback)
			{
				vector plainSpot = GroundSpot(center);
				if (IsSiteOk(plainSpot, front, players) && IsRegionRed(plainSpot, region, front, book))
				{
					hasFallback = true;
					fallbackPos = plainSpot;
					fallbackZone = front.GetZoneAt(plainSpot);
				}
			}
		}
		if (!hasFallback)
		{
			// Refus noté (texte stable : répété au plus toutes les vu_refus_repos_min) ; le Staff peut régler
			// res_depot_distance_* sans republier. Joueurs trop près : on réessaie simplement au passage suivant.
			if (accepted == 0)
				SRP_CmdLog.Refusal(region, "pose du dépôt de la " + RegionLabel(region), string.Format("aucun carré rouge à %1 m du bleu, %2 m des points clés et %3 m des joueurs", m_iDepotBlueM, m_iDepotKeyPointM, m_iDepotPlaceM));
			return false;
		}
		position = fallbackPos;
		zone = fallbackZone;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Pose du décor hors de vue, aucun joueur à res_depot_distance_pose_m ; révélé si m_bRevealPending ; journal —
	//! Tick
	protected bool Place(SRP_CmdDepot depot, vector position, int zone, int nowUnix)
	{
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		if (SRP_Placement.NearestPlayer(position, players) < m_iDepotPlaceM)
			return false;
		if (SRP_Placement.IsSeenByAnyPlayer(position, players))
			return false;
		IEntity entity = SpawnDecor(position);
		if (!entity)
		{
			SRP_CmdLog.Refusal(depot.m_iRegion, "pose du dépôt de la " + RegionLabel(depot.m_iRegion), "décor illisible : " + m_sDepotPrefab);
			return false;
		}
		depot.m_Entity = entity;
		depot.m_vPos = position;
		depot.m_iZone = zone;
		depot.m_iNextTryAt = 0;
		depot.m_iQuietSince = 0;
		// Nouveau dépôt ailleurs : si les gardes de l'ancien sont toutes tombées, il pourra être gardé de nouveau
		PruneGuards(depot);
		if (depot.m_aGuards.IsEmpty())
			depot.m_vGuardPos = vector.Zero;
		string revealText = "";
		if (depot.m_bRevealPending)
		{
			depot.m_bRevealed = true;
			depot.m_bRevealPending = false;
			revealText = ", révélé dès la pose (" + depot.m_sRevealedBy + ")";
		}
		SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 posé au carré %2, %3 (déplacement n° %4)%5", RegionLabel(depot.m_iRegion), CellRefAt(position), ZoneLabel(zone), depot.m_iMoves, revealText));
		Dirty(false);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! RE8 : CutRegion, décor gardé pour effacement hors de vue, dépôt à reposer ailleurs (caché), radio
	//! EnemyDepotSabotaged, alerte alerte_sabotage de la région, MarkDirty(true) — TryUse, OnZoneOwnerChanged, Staff
	protected void Sabotage(SRP_CmdDepot depot, string author, string how)
	{
		if (!depot)
			return;
		int region = depot.m_iRegion;
		int zone = depot.m_iZone;
		string place = "rien de posé";
		if (depot.m_vPos != vector.Zero)
			place = "carré " + CellRefAt(depot.m_vPos);
		string by = how;
		if (!author.IsEmpty())
			by = how + ", " + author;

		// Revenu coupé (relancé, jamais cumulé)
		if (m_Resources)
			m_Resources.CutRegion(region, "dépôt saboté : " + by);

		// Le décor reste en attendant d'être effacé hors de vue ; le dépôt sera reposé ailleurs, caché
		if (depot.m_Entity)
		{
			depot.m_OldEntity = depot.m_Entity;
			Trash(depot.m_Entity);
		}
		depot.m_Entity = null;
		if (zone >= 0)
			depot.m_iPrevZone = zone;
		depot.m_vPos = vector.Zero;
		depot.m_iZone = -1;
		depot.m_bRevealed = false;
		depot.m_bRevealPending = false;
		depot.m_sRevealedBy = "";
		depot.m_iMoves = depot.m_iMoves + 1;
		depot.m_iNextTryAt = 0;

		// Radio (évènement depot_ennemi) et alerte de la région
		int hours = 0;
		if (m_Resources)
			hours = m_Resources.m_iDepotCutHours;
		SRP_FrontRadio.EnemyDepotSabotaged(region, hours, zone);
		SRP_CmdRegionBook book = Book();
		if (book)
			book.AddRegionAlert(region, book.m_iAlertSabotage, "dépôt saboté");
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 saboté (%2), %3 : revenu coupé %4 h, nouvelle pose cachée au prochain passage", RegionLabel(region), by, place, hours));
		Dirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Gardes du dépôt : pose à l'approche (Ask GARDE, Tag), retrait après res_depot_gardes_retrait_min sans
	//! joueur à res_depot_gardes_retrait_m — Tick
	protected void GuardsTick(SRP_CmdDepot depot, int nowUnix)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !depot)
			return;

		// Groupes supprimés entre-temps (tous tombés, retrait, remise à plat de l'IA ennemie) : oubliés
		PruneGuards(depot);

		// Gardes posées : l'état dure jusqu'au retrait, même si toutes sont tombées (sections supprimées par le jeu
		// quand elles sont vides) ; sinon des joueurs qui tiennent le secteur verraient revenir une section chaque minute
		if (!depot.m_aGuards.IsEmpty() || depot.m_vGuardPos != vector.Zero)
		{
			// Retrait : plus aucun joueur à res_depot_gardes_retrait_m depuis res_depot_gardes_retrait_min minutes
			vector around = depot.m_vGuardPos;
			if (around == vector.Zero)
				around = depot.m_vPos;
			array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
			if (around != vector.Zero && SRP_Placement.NearestPlayer(around, everyone) <= m_iDepotGuardsRetireM)
			{
				depot.m_iQuietSince = 0;
				return;
			}
			if (depot.m_iQuietSince <= 0)
			{
				depot.m_iQuietSince = nowUnix;
				return;
			}
			if (nowUnix - depot.m_iQuietSince < m_iDepotGuardsRetireMin * 60)
				return;
			int retired = depot.m_aGuards.Count();
			if (retired > 0)
				enemies.RetireLater(depot.m_aGuards);		// retirées hors de vue ; la liste est vidée
			depot.m_aGuards.Clear();
			depot.m_iQuietSince = 0;
			depot.m_vGuardPos = vector.Zero;
			if (retired > 0)
				SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 : %2 section(s) de garde retirée(s), plus personne à %3 m", RegionLabel(depot.m_iRegion), retired, m_iDepotGuardsRetireM));
			else
				SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 : gardes tombées, plus personne à %2 m ; une nouvelle garde pourra être posée", RegionLabel(depot.m_iRegion), m_iDepotGuardsRetireM));
			return;
		}

		// Pose à l'approche d'un joueur actif (hors délai de grâce), comme les gardes des missions
		if (depot.m_vPos == vector.Zero || m_iDepotGuardsMax <= 0)
			return;
		array<IEntity> active = enemies.GetActivePlayers();
		float nearest = SRP_Placement.NearestPlayer(depot.m_vPos, active);
		if (nearest > m_iDepotGuardsM)
			return;
		int groups = Math.Min(enemies.ScaleGroupsNear(depot.m_vPos, 0, m_iDepotGuardsMax), enemies.LocalRoom(depot.m_vPos));
		if (groups <= 0)
			return;		// le lieu est déjà tenu (localité, mission) : pas de doublon
		string refusal = SRP_CmdCapacity.Get().Ask("depot-" + depot.m_sCode, SRP_ECmdCapClass.GARDE, groups * GUARD_SOLDIERS, groups, depot.m_vPos, "depot");
		if (!refusal.IsEmpty())
		{
			SRP_CmdLog.Refusal(depot.m_iRegion, "gardes du dépôt de la " + RegionLabel(depot.m_iRegion), refusal);
			return;		// demande notée (file RE12), nouvel essai au passage suivant
		}
		array<ref SRP_EnemyGroup> posted = {};
		enemies.SpawnGroups(depot.m_vPos, 40, groups, true, depot.m_vPos, "depot", posted, 40);
		foreach (SRP_EnemyGroup record : posted)
		{
			if (!record)
				continue;
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.GARDE);
			record.m_iCmdRole = SRP_ECmdRole.GARDE_DEPOT;
			depot.m_aGuards.Insert(record);
		}
		if (depot.m_aGuards.IsEmpty())
			return;		// rien de posé (plafond de groupes du jeu) : nouvel essai au passage suivant
		depot.m_vGuardPos = depot.m_vPos;		// gardes posées : état tenu jusqu'au retrait, même si elles tombent
		depot.m_iQuietSince = 0;
		int nearestM = Math.Round(nearest);
		SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 : %2 section(s) de garde posée(s), joueur à %3 m", RegionLabel(depot.m_iRegion), depot.m_aGuards.Count(), nearestM));
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Le dépôt change de place, caché, SANS coupure ni radio (Staff, zone forcée, relecture) ; l'ancien décor est
	//! effacé hors de vue
	protected void Relocate(SRP_CmdDepot depot, string why)
	{
		if (!depot)
			return;
		Trash(depot.m_Entity);
		depot.m_Entity = null;
		if (depot.m_iZone >= 0)
			depot.m_iPrevZone = depot.m_iZone;
		depot.m_vPos = vector.Zero;
		depot.m_iZone = -1;
		depot.m_bRevealed = false;
		depot.m_bRevealPending = false;
		depot.m_sRevealedBy = "";
		depot.m_iMoves = depot.m_iMoves + 1;
		depot.m_iNextTryAt = 0;
		SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 changé de place (%2) : nouvelle pose cachée au prochain passage", RegionLabel(depot.m_iRegion), why));
		Dirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Oublie les sections de garde dont le groupe n'existe plus (toutes tombées : le jeu supprime un groupe vide)
	protected void PruneGuards(SRP_CmdDepot depot)
	{
		for (int i = depot.m_aGuards.Count() - 1; i >= 0; i--)
		{
			SRP_EnemyGroup guard = depot.m_aGuards[i];
			if (!guard || !guard.m_Group || guard.m_Group.IsDeleted())
				depot.m_aGuards.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Décor absent alors que la place est connue (redémarrage, restauration) : reposé au même endroit, hors de vue
	protected bool Respawn(SRP_CmdDepot depot)
	{
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		if (SRP_Placement.NearestPlayer(depot.m_vPos, players) < RESPAWN_CLEAR_M)
			return false;
		if (SRP_Placement.IsSeenByAnyPlayer(depot.m_vPos, players))
			return false;
		IEntity entity = SpawnDecor(depot.m_vPos);
		if (!entity)
		{
			SRP_CmdLog.Refusal(depot.m_iRegion, "repose du dépôt de la " + RegionLabel(depot.m_iRegion), "décor illisible : " + m_sDepotPrefab);
			return false;
		}
		depot.m_Entity = entity;
		SRP_CmdLog.Note(depot.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Dépôt de la %1 reposé à sa place, carré %2", RegionLabel(depot.m_iRegion), CellRefAt(depot.m_vPos)));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Décor du dépôt posé à cette position, orienté au hasard ; null si le prefab est illisible
	protected IEntity SpawnDecor(vector position)
	{
		if (m_sDepotPrefab.IsEmpty())
			return null;
		Resource res = Resource.Load(m_sDepotPrefab);
		if (!res || !res.IsValid())
			return null;
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(Math.RandomFloat(0, 360), 0, 0), mat);
		mat[3] = position;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[0] = mat[0];
		params.Transform[1] = mat[1];
		params.Transform[2] = mat[2];
		params.Transform[3] = mat[3];
		return GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
	}

	//------------------------------------------------------------------------------------------------
	//! Point au sol libre autour d'un point (FindEmptyTerrainPosition, sinon le point lui-même au sol)
	protected vector GroundSpot(vector point)
	{
		vector ground = SRP_Placement.OnGround(point);
		vector freeSpot;
		if (SCR_WorldTools.FindEmptyTerrainPosition(freeSpot, ground, 25, 3, 3))
			ground = SRP_Placement.OnGround(freeSpot);
		return ground;
	}

	//------------------------------------------------------------------------------------------------
	//! Site acceptable : terre hors base, loin des joueurs, du bleu (m_aBlue) et des points clés
	protected bool IsSiteOk(vector position, SRP_FrontComponent front, array<IEntity> players)
	{
		if (SRP_Placement.IsWater(position) || SRP_Placement.IsNearBase(position) || front.IsInBaseZone(position))
			return false;
		if (SRP_Placement.NearestPlayer(position, players) < m_iDepotPlaceM)
			return false;
		foreach (vector blue : m_aBlue)
		{
			if (vector.DistanceXZ(position, blue) < m_iDepotBlueM)
				return false;
		}
		int keyPoints = front.GetKeyPointTotal();
		for (int k = 0; k < keyPoints; k++)
		{
			SRP_FrontKeyPoint point = front.GetKeyPoint(k);
			if (point && vector.DistanceXZ(position, point.m_vPos) < m_iDepotKeyPointM)
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! La position est sur un carré rouge d'une zone rouge de la région
	protected bool IsRegionRed(vector position, int region, SRP_FrontComponent front, SRP_CmdRegionBook book)
	{
		int cell = front.CellIndexAt(position);
		if (cell < 0 || front.GetCellOwner(cell) != SRP_EFrontOwner.ROUGE)
			return false;
		int cellZone = front.GetCellZone(cell);
		if (cellZone < 0 || front.GetZoneOwner(cellZone) != SRP_EFrontOwner.ROUGE)
			return false;
		return book.GetRegionOfZone(cellZone) == region;
	}

	//------------------------------------------------------------------------------------------------
	//! Centres des carrés bleus (base comprise), refaits quand l'état du front a changé
	protected void BuildBlueCache(SRP_FrontComponent front)
	{
		int version = front.GetStateVersion();
		if (version == m_iBlueVersion && !m_aBlue.IsEmpty())
			return;
		m_aBlue.Clear();
		int cellCount = front.GetCellCount();
		for (int c = 0; c < cellCount; c++)
		{
			if (front.IsCellInPlay(c) && front.GetCellOwner(c) == SRP_EFrontOwner.BLEU)
				m_aBlue.Insert(front.CellCenter(c));
		}
		m_iBlueVersion = version;
	}

	//------------------------------------------------------------------------------------------------
	//! Décors à effacer hors de vue : aucun joueur à TRASH_CLEAR_M et personne ne les voit
	protected void TrashTick()
	{
		if (m_aTrash.IsEmpty())
			return;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		for (int i = m_aTrash.Count() - 1; i >= 0; i--)
		{
			IEntity old = m_aTrash[i];
			if (!old || old.IsDeleted())
			{
				m_aTrash.Remove(i);
				continue;
			}
			vector at = old.GetOrigin();
			if (SRP_Placement.NearestPlayer(at, players) < TRASH_CLEAR_M || SRP_Placement.IsSeenByAnyPlayer(at, players))
				continue;
			foreach (SRP_CmdDepot depot : m_aDepots)
			{
				if (depot.m_OldEntity == old)
					depot.m_OldEntity = null;
			}
			m_aTrash.Remove(i);
			SCR_EntityHelper.DeleteEntityAndChildren(old);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un décor à effacer hors de vue (une seule fois)
	protected void Trash(IEntity entity)
	{
		if (!entity || entity.IsDeleted())
			return;
		if (!m_aTrash.Contains(entity))
			m_aTrash.Insert(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! Dépôt remis « à poser, caché » (champs sauvés et décor oubliés ; les gardes sont traitées à part)
	protected void ClearState(SRP_CmdDepot depot)
	{
		depot.m_vPos = vector.Zero;
		depot.m_iZone = -1;
		depot.m_iPrevZone = -1;
		depot.m_bRevealed = false;
		depot.m_sRevealedBy = "";
		depot.m_bRevealPending = false;
		depot.m_iMoves = 0;
		depot.m_Entity = null;
		depot.m_iNextTryAt = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Un dépôt par région du tracé (index = région), codes relus ; les dépôts en trop sont effacés hors de vue
	protected void EnsureDepots(int count)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		while (m_aDepots.Count() > count && !m_aDepots.IsEmpty())
		{
			int last = m_aDepots.Count() - 1;
			SRP_CmdDepot extra = m_aDepots[last];
			if (extra)
			{
				Trash(extra.m_Entity);
				if (enemies && !extra.m_aGuards.IsEmpty())
					enemies.RetireLater(extra.m_aGuards);
			}
			m_aDepots.Remove(last);
		}
		while (m_aDepots.Count() < count)
		{
			SRP_CmdDepot added = new SRP_CmdDepot();
			m_aDepots.Insert(added);
		}
		SRP_CmdRegionBook book = Book();
		foreach (int i, SRP_CmdDepot depot : m_aDepots)
		{
			depot.m_iRegion = i;
			if (book && i < book.GetRegionCount())
				depot.m_sCode = book.GetRegionCode(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdDepot GetDepot(int region)
	{
		if (region < 0 || region >= m_aDepots.Count())
			return null;
		return m_aDepots[region];
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdDepot FindByCode(string code)
	{
		if (code.IsEmpty())
			return null;
		foreach (SRP_CmdDepot depot : m_aDepots)
		{
			if (depot.m_sCode == code)
				return depot;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Code de zone pour la sauvegarde (« S07 »), "" sans zone
	protected string ZoneCode(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0 || zone >= front.GetZoneCount())
			return "";
		return front.GetZoneCode(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone d'un code relu, -1 si vide ou inconnu
	protected int ZoneOfCode(string code)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || code.IsEmpty())
			return -1;
		return front.FindZone(code);
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07) » (textes Staff)
	protected string ZoneLabel(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0 || zone >= front.GetZoneCount())
			return "zone inconnue";
		return front.GetZoneLabel(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Référence du carré sous une position (« 074 042 »)
	protected string CellRefAt(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "";
		return front.CellRef(front.CellIndexAt(position));
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe » (repli : « région R3 »)
	protected string RegionLabel(int region)
	{
		SRP_CmdRegionBook book = Book();
		if (book && region >= 0 && region < book.GetRegionCount())
			return book.GetRegionLabel(region);
		SRP_CmdDepot depot = GetDepot(region);
		if (depot)
			return "région " + depot.m_sCode;
		return "région inconnue";
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdRegionBook Book()
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return null;
		return commander.GetBook();
	}

	//------------------------------------------------------------------------------------------------
	protected int BookRegionCount()
	{
		SRP_CmdRegionBook book = Book();
		if (!book)
			return m_aDepots.Count();
		return book.GetRegionCount();
	}

	//------------------------------------------------------------------------------------------------
	protected void Dirty(bool immediate)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(immediate);
	}
}
