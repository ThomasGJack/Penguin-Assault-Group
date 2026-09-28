//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : PRODUCTION ET DÉPÔTS DE ZONE (#49, G4 à G7, E4), classe serveur tenue par le socle.
// Contrats figés : CONTRATS_FRONT.md (§2, §4).
//
// RÔLE :
// - Production par zone à nous (G4) : vivres pour un village ou une ville, munitions pour un hameau, rien sans
//   localité sauf désignation (Q4) ; carburant ou pièces par la ligne « ressource S12 carburant [débit] » de
//   front_retouches.txt (G5, un seul fichier, trou 17 : lue par le bâtisseur, rangée dans SRP_FrontZone.m_iResource).
//   Au démarrage, la ressource effective (la ligne du fichier, sinon le genre de la zone) est rangée dans m_iResource
//   (-1 = rien) ; un débit 0 la garde, seule la production s'arrête (DefaultProduction).
// - Une zone coupée de la base ne produit plus (E4, lu sur le socle : IsZoneCut) : son compteur reste figé.
// - Dépôt n'importe où dans une zone à nous, sauf sur un carré du front (G6) ; détruit si son carré est REPRIS par
//   l'ennemi (G7, cause REPRISE seule ; les autres causes l'effacent en silence : OnZoneLost, OnFrontReset, Staff).
// - Les DONNÉES (m_iStock, m_bDepot, m_vDepot, m_iResource, m_iRate) sont des champs de SRP_FrontZone, sauvegardés
//   par le socle dans front.json (trou 16) : cette classe n'écrit AUCUN fichier. Le stock suit la caisse (recompte à
//   chaque passage) : un paquet pris par un joueur n'est jamais reposé au redémarrage.
// - Elle ne fait PAS K2 (versement des anciens secteurs : socle, trou 18), ni prime (missions), ni menace (chute).
// - Textes publics par SRP_FrontRadio (DepotProduced, DepotNoDepot, DepotInstalled, DepotDestroyed) ; seules les
//   réponses à un joueur ou au Staff (refus, comptes rendus) sont construites ici. Aucune ligne de journal : la ligne
//   [STAFF] des actions du menu est écrite par SRP_FrontScreens.RunStaff, les notes techniques vont à la console.
// APPELÉ PAR : le socle (Start, Tick 3e de la boucle, OnCellsChanged 3e après chaque lot, OnFrontReset), SRP_ZoneFall
// (OnZoneCaptured, OnZoneLost), SRP_DepotInstallAction, SRP_FrontScreens (Staff), SRP_Bridge (GetBoxes).
// Accès : SRP_FrontComponent.GetInstance().GetEconomy().
//------------------------------------------------------------------------------------------------

class SRP_FrontEconomy
{
	protected static const int HOUR_SECONDS = 3600;		// une heure de production

	protected SRP_FrontComponent m_Front;		// le socle (réglages publics m_iProd*, zones)

	//------------------------------------------------------------------------------------------------
	//! Créée par le socle dans OnPostInit (serveur) — SRP_FrontComponent
	void SRP_FrontEconomy(SRP_FrontComponent front)
	{
		m_Front = front;
	}

	//------------------------------------------------------------------------------------------------
	//! Après Load : dépôt chargé dont le carré n'est plus bleu ou plus dans la zone -> effacé (stock gardé, journal) ;
	//! caisses reposées (RespawnBoxes) et repères des dépôts — SRP_FrontComponent.Start
	void Start()
	{
		if (!m_Front)
			return;
		int count = m_Front.GetZoneCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontZone zone = m_Front.GetZone(i);
			if (!zone)
				continue;
			// Ressource effective : la ligne « ressource » du fichier, sinon le genre de la zone ; -1 = rien (base, zone
			// sans localité ni désignation). Un débit 0 (ligne du fichier ou réglage front_prod_* à 0) garde la ressource :
			// DefaultProduction arrête la production, et le stock restant est toujours compté, reposé et annoncé dans son
			// vrai genre de paquet.
			zone.m_iResource = ResourceOf(zone);
			zone.m_iProductionSec = 0;
		}
		CheckLoadedDepots();
		RespawnBoxes();
	}

	//------------------------------------------------------------------------------------------------
	//! Passage de 5 s (3e de la boucle unique) : pour chaque zone à nous qui produit et n'est pas coupée (E4,
	//! m_bProductionStopsWhenCut), m_iProductionSec += 5 ; à 3600, Produce — SRP_FrontComponent.Tick
	void Tick(int nowUnix)
	{
		if (!m_Front)
			return;
		int step = SRP_FrontComponent.TICK_MS / 1000;
		int count = m_Front.GetZoneCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontZone zone = m_Front.GetZone(i);
			if (!zone || zone.m_bBase || zone.m_iOwner != SRP_EFrontOwner.BLEU)
				continue;

			// Le stock suit la caisse : les joueurs ont pu se servir (sauvegarde et pont toujours justes)
			RefreshStock(zone);

			int resource;
			int perHour;
			DefaultProduction(zone, resource, perHour);
			if (resource < 0 || perHour <= 0)
				continue;
			if (m_Front.m_bProductionStopsWhenCut && m_Front.IsZoneCut(i))
				continue;	// E4 : zone coupée de la base, le compteur reste figé jusqu'à la reprise du passage

			zone.m_iProductionSec += step;
			if (zone.m_iProductionSec >= HOUR_SECONDS)
			{
				zone.m_iProductionSec = 0;
				Produce(zone);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! G7 selon la CAUSE (SRP_EFrontReason) d'un carré du dépôt qui n'est plus bleu (m_bDepotLostWithCell) :
	//! REPRISE seule -> DestroyDepot (radio DepotDestroyed + évènement depot_detruit) ; STAFF (ForceCell, correction
	//! Q9) -> ClearDepot en silence, stock gardé ; ZONE_PERDUE, ISOLEMENT, OFFENSIVE -> rien ici (le dépôt est effacé en
	//! silence par OnZoneLost) ; REMISE, RESTAURATION -> rien ici (OnFrontReset) ; carrés passés bleus -> rien —
	//! socle, 3e appel direct après chaque lot (trou 6)
	void OnCellsChanged(notnull array<int> cells, int owner, int reason)
	{
		if (!m_Front || !m_Front.m_bDepotLostWithCell || owner == SRP_EFrontOwner.BLEU)
			return;
		if (reason != SRP_EFrontReason.REPRISE && reason != SRP_EFrontReason.STAFF)
			return;
		foreach (int cell : cells)
		{
			SRP_FrontZone zone = m_Front.GetZone(m_Front.GetCellZone(cell));
			if (!zone || !zone.m_bDepot || zone.m_iDepotCell != cell)
				continue;
			if (reason == SRP_EFrontReason.REPRISE)
			{
				DestroyDepot(zone);
				continue;
			}
			// STAFF : correction, le dépôt disparaît sans annonce et ce qui attendait dans la caisse est gardé
			RefreshStock(zone);
			ClearDepot(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Zone prise : production et stock à zéro — SRP_ZoneFall (2e appel après la chute)
	void OnZoneCaptured(int zone)
	{
		if (!m_Front)
			return;
		SRP_FrontZone record = m_Front.GetZone(zone);
		if (!record)
			return;
		// Reste d'une ancienne tenue (ne devrait pas arriver : la perte efface tout) : une zone prise repart de rien
		if (record.m_bDepot || record.m_Box != null || record.m_DepotMarker != null)
			ClearDepot(record);
		record.m_iStock = 0;
		record.m_iProductionSec = 0;
		m_Front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone perdue : stock à zéro, dépôt effacé et caisse supprimée (comme Lose, SRP_Territory.c:1744-1747) — SRP_ZoneFall
	void OnZoneLost(int zone)
	{
		if (!m_Front)
			return;
		SRP_FrontZone record = m_Front.GetZone(zone);
		if (record)
			EmptyZone(record);	// la caisse, son contenu et l'emplacement : tout est perdu, il faudra un nouveau kit
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à plat (SRP_EFrontReset) : caisses supprimées, stocks à zéro, dépôts effacés (ZONE : cette zone seule ;
	//! RESTAURATION : caisses et repères refaits d'après les champs restaurés) — SRP_FrontComponent.NotifyReset
	void OnFrontReset(int kind, int zone)
	{
		if (!m_Front)
			return;
		if (kind == SRP_EFrontReset.ZONE)
		{
			SRP_FrontZone record = m_Front.GetZone(zone);
			if (record)
				EmptyZone(record);
			return;
		}

		int count = m_Front.GetZoneCount();
		if (kind == SRP_EFrontReset.RESTAURATION)
		{
			// Les champs (stock, dépôt) viennent de la copie datée : seules la caisse et l'icône vivantes sont refaites
			for (int i = 0; i < count; i++)
			{
				SRP_FrontZone restored = m_Front.GetZone(i);
				if (!restored)
					continue;
				DropLive(restored);
				restored.m_iProductionSec = 0;
			}
			CheckLoadedDepots();
			RespawnBoxes();
			return;
		}

		// CAMPAGNE : tout repart de rien
		for (int j = 0; j < count; j++)
		{
			SRP_FrontZone emptied = m_Front.GetZone(j);
			if (emptied)
				EmptyZone(emptied);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! G6 : kit posé au sol (droits et test du kit repris de SRP_Territory.c:2193-2203) dans une zone à nous hors
	//! base, sur un carré bleu, pas au front (m_bDepotNotOnFront : « Trop près de l'ennemi… %2 carré(s) possible(s) »),
	//! un seul dépôt par zone ; SetDepot, kit supprimé, radio DepotInstalled ; rend "" ou le refus —
	//! SRP_DepotInstallAction
	string InstallDepot(int playerId, IEntity kit)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players || !kit || !m_Front || !m_Front.IsReady())
			return "Installation impossible";
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";
		bool allowed = record.m_bStaff || SRP_Grades.IsOfficier(record.m_iGrade) || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.LOGI);
		if (!allowed)
			return "Installer un dépôt est réservé aux logisticiens (certification Logi) et aux officiers";
		if (kit.GetParent())
			return "Pose d'abord le kit au sol, à l'endroit voulu";

		vector position = kit.GetOrigin();
		int cell = m_Front.CellIndexAt(position);
		int zoneIndex = -1;
		if (cell >= 0)
			zoneIndex = m_Front.GetCellZone(cell);
		SRP_FrontZone zone = m_Front.GetZone(zoneIndex);
		if (!zone || cell < 0 || zone.m_bBase || zone.m_iOwner != SRP_EFrontOwner.BLEU)
			return "Ce kit n'est dans aucune zone tenue par la PAG : un dépôt s'installe dans une zone à nous, hors de la base";

		string label = m_Front.GetZoneLabel(zoneIndex);
		if (zone.m_bDepot)
			return string.Format("%1 a déjà son dépôt (carré %2) : il ne bouge plus tant que la zone est à nous", label, DepotCellRef(zone));
		if (m_Front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
			return "Ce carré est tenu par l'ennemi : un dépôt s'installe sur un carré à nous";
		if (m_Front.m_bDepotNotOnFront && m_Front.HasNeighbourOwnedBy(cell, SRP_EFrontOwner.ROUGE, false))
		{
			int safe = CountSafeCells(zone);
			if (safe == 0)
				return string.Format("%1 touche le front de partout : aucun carré à l'abri pour un dépôt", label);
			return string.Format("Trop près de l'ennemi : ce carré touche un carré rouge. Pose le kit plus à l'arrière de %1 (%2 carré(s) possible(s))", label, safe);
		}

		SetDepot(zone, position, cell);
		SCR_EntityHelper.DeleteEntityAndChildren(kit);
		SRP_FrontRadio.DepotInstalled(zoneIndex, record.m_sName, m_Front.CellRef(cell));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : dépôt fixé ICI sans kit (position dans la zone, carré bleu ; un carré du front est accepté, correction)
	//! — SRP_FrontScreens
	string ForceDepot(int zone, vector position, string author)
	{
		if (!m_Front || !m_Front.IsReady())
			return "Front pas encore prêt";
		SRP_FrontZone record = m_Front.GetZone(zone);
		if (!record)
			return "Zone inconnue";
		string label = m_Front.GetZoneLabel(zone);
		if (record.m_bBase)
			return "La base n'a pas de dépôt de zone";
		if (record.m_iOwner != SRP_EFrontOwner.BLEU)
			return label + " n'est pas à nous";
		int cell = m_Front.CellIndexAt(position);
		if (cell < 0 || m_Front.GetCellZone(cell) != zone)
			return string.Format("Tu es hors de %1 : le dépôt se fixe sur un carré de la zone", label);
		if (m_Front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
			return "Ce carré est tenu par l'ennemi : le dépôt se fixe sur un carré à nous";

		// Ce qui attend dans l'ancienne caisse est recompté, puis livré au nouvel endroit
		RefreshStock(record);
		ClearDepot(record);
		SetDepot(record, position, cell);
		return string.Format("Dépôt de %1 fixé ici, carré %2 (%3 paquet(s) en attente)", label, m_Front.CellRef(cell), record.m_iStock);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : retire le dépôt, garde le stock (comme SRP_Territory.c:2242-2260) — SRP_FrontScreens
	string RemoveDepot(int zone, string author)
	{
		if (!m_Front)
			return "Front pas encore prêt";
		SRP_FrontZone record = m_Front.GetZone(zone);
		if (!record)
			return "Zone inconnue";
		string label = m_Front.GetZoneLabel(zone);
		if (!record.m_bDepot)
			return label + " n'a pas de dépôt";
		RefreshStock(record);
		ClearDepot(record);
		return string.Format("Dépôt de %1 retiré (%2 paquet(s) gardés en attente) : il faudra un nouveau kit", label, record.m_iStock);
	}

	//------------------------------------------------------------------------------------------------
	//! Paquets par heure de la zone (m_iRate, sinon le défaut de sa ressource), 0 si elle ne produit rien — StatusText
	int PerHour(int zone)
	{
		if (!m_Front)
			return 0;
		int resource;
		int perHour;
		DefaultProduction(m_Front.GetZone(zone), resource, perHour);
		return perHour;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte du PC et du Staff : « produit 10 vivres/h, 12 en attente, dépôt carré 072 023 », « coupée de la base :
	//! production arrêtée » ou « ne produit rien » — SRP_FrontScreens
	string StatusText(int zone)
	{
		if (!m_Front)
			return "ne produit rien";
		SRP_FrontZone record = m_Front.GetZone(zone);
		int resource;
		int perHour;
		DefaultProduction(record, resource, perHour);
		if (!record || resource < 0 || perHour <= 0)
			return "ne produit rien";

		string what = string.Format("%1 %2/h", perHour, SRP_Resources.GetName(resource));
		if (record.m_iOwner != SRP_EFrontOwner.BLEU)
			return "ennemie : " + what + " une fois prise";

		string text;
		if (m_Front.m_bProductionStopsWhenCut && m_Front.IsZoneCut(zone))
			text = "coupée de la base : production arrêtée";
		else
			text = "produit " + what;
		text += string.Format(", %1 en attente", record.m_iStock);
		if (record.m_bDepot)
			text += ", dépôt carré " + DepotCellRef(record);
		else
			text += ", pas de dépôt (kit à poser)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones à nous avec un stock > 0 et une ressource : libellé « Régina (S07) », paquets, SRP_EResource ; rend le
	//! nombre — SRP_Bridge (boxes[], clés sector, count, resource inchangées)
	int GetBoxes(notnull array<string> labels, notnull array<int> counts, notnull array<int> resources)
	{
		labels.Clear();
		counts.Clear();
		resources.Clear();
		if (!m_Front)
			return 0;
		int count = m_Front.GetZoneCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontZone zone = m_Front.GetZone(i);
			if (!zone || zone.m_bBase || zone.m_iOwner != SRP_EFrontOwner.BLEU || zone.m_iStock <= 0)
				continue;
			int resource = ResourceOf(zone);
			if (resource < 0)
				continue;
			labels.Insert(m_Front.GetZoneLabel(i));
			counts.Insert(zone.m_iStock);
			resources.Insert(resource);
		}
		return labels.Count();
	}

	//================================================================================================
	// Interne (reprend SRP_Territory.c l.692-704, 1922-1953, 2263-2329, par zone)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Ressource et débit par défaut d'après le genre (zone.m_iKind) : hameau MUNITIONS, village ou ville VIVRES,
	//! sans localité -1 et 0 (Q4) ; la ligne « ressource » du fichier l'emporte (paramètres out sans défaut)
	protected void DefaultProduction(SRP_FrontZone zone, out int resource, out int perHour)
	{
		resource = -1;
		perHour = 0;
		int wanted = ResourceOf(zone);
		if (wanted < 0)
			return;
		// Débit : celui de la ligne du fichier (m_iRate, 0 = rien), sinon le réglage de la ressource
		int rate = zone.m_iRate;
		if (rate < 0)
			rate = DefaultRate(wanted);
		if (rate <= 0)
			return;
		resource = wanted;
		perHour = rate;
	}

	//------------------------------------------------------------------------------------------------
	//! Genre de paquet de la zone, débit non compris : m_iResource s'il est valide (ligne « ressource » du fichier, ou
	//! ressource rangée par Start), sinon le genre (hameau MUNITIONS, village ou ville VIVRES) ; -1 pour la base et une
	//! zone sans localité ni désignation — DefaultProduction, caisses
	protected int ResourceOf(SRP_FrontZone zone)
	{
		if (!zone || zone.m_bBase)
			return -1;
		if (SRP_Resources.IsValide(zone.m_iResource))
			return zone.m_iResource;
		if (zone.m_iKind == SRP_EZoneKind.HAMEAU)
			return SRP_EResource.MUNITIONS;
		if (zone.m_iKind == SRP_EZoneKind.VILLAGE || zone.m_iKind == SRP_EZoneKind.VILLE)
			return SRP_EResource.VIVRES;
		return -1;	// Q4 : sans localité, rien sauf désignation
	}

	//------------------------------------------------------------------------------------------------
	//! Débit par défaut d'une ressource (réglages front_prod_*, relus sans republier) — DefaultProduction
	protected int DefaultRate(int resource)
	{
		if (resource == SRP_EResource.VIVRES)
			return m_Front.m_iProdVivresPerHour;
		if (resource == SRP_EResource.MUNITIONS)
			return m_Front.m_iProdMunitionsPerHour;
		if (resource == SRP_EResource.CARBURANT)
			return m_Front.m_iProdCarburantPerHour;
		if (resource == SRP_EResource.PIECES)
			return m_Front.m_iProdPiecesPerHour;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Une heure de production : plafond PerHour x m_iProductionCapHours, SRP_Packets dans la caisse, DeliverStock,
	//! radio DepotProduced ou DepotNoDepot (mot « produit » pour la couleur Discord) — Tick
	protected void Produce(SRP_FrontZone zone)
	{
		int resource;
		int packets;
		DefaultProduction(zone, resource, packets);
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (resource < 0 || packets <= 0 || !resources)
			return;

		int capHours = m_Front.m_iProductionCapHours;
		if (capHours < 1)
			capHours = 1;
		int cap = packets * capHours;

		// Recompte de ce qui reste réellement dans la caisse (les joueurs ont pu se servir)
		bool hasBox = zone.m_Box != null && !zone.m_Box.IsDeleted();
		if (hasBox)
			zone.m_iStock = SRP_ResourceManagerComponent.CountInBox(zone.m_Box, resource);
		else
			zone.m_Box = null;
		if (zone.m_iStock >= cap)
			return;

		int toAdd = cap - zone.m_iStock;
		if (toAdd > packets)
			toAdd = packets;
		if (hasBox)
		{
			zone.m_iStock += SRP_Packets.Insert(zone.m_Box, resources.GetPacketPrefab(resource), toAdd);
		}
		else
		{
			zone.m_iStock += toAdd;
			DeliverStock(zone);	// au dépôt s'il existe ; sinon la production attend, sans caisse
		}
		m_Front.MarkDirty(false);

		int index = zone.m_iIndex;
		if (zone.m_bDepot)
			SRP_FrontRadio.DepotProduced(index, zone.m_iStock, resource, DepotCellRef(zone));
		else if (zone.m_iStock == toAdd)
			SRP_FrontRadio.DepotNoDepot(index, zone.m_iStock, resource);	// une seule fois, à la première heure sans dépôt
	}

	//------------------------------------------------------------------------------------------------
	//! Remplit la caisse du dépôt avec le stock en attente — Produce, Start
	protected void DeliverStock(SRP_FrontZone zone)
	{
		if (!zone.m_bDepot || zone.m_iStock <= 0)
			return;
		if (zone.m_Box && !zone.m_Box.IsDeleted())
			return;
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		int resource = ResourceOf(zone);
		if (!resources || resource < 0)
			return;
		vector position = zone.m_vDepot;
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.1;
		zone.m_Box = resources.SpawnBox(resource, zone.m_iStock, position);
		if (zone.m_Box)
			zone.m_iStock = SRP_ResourceManagerComponent.CountInBox(zone.m_Box, resource);
	}

	//------------------------------------------------------------------------------------------------
	//! Pose le dépôt (m_bDepot, m_vDepot, m_iDepotCell), la caisse et le repère « Dépôt de Régina (S07) », puis
	//! m_Front.MarkDirty(true) — InstallDepot, ForceDepot
	protected void SetDepot(SRP_FrontZone zone, vector position, int cell)
	{
		zone.m_bDepot = true;
		zone.m_vDepot = position;
		zone.m_iDepotCell = cell;
		PlaceDepotMarker(zone);
		DeliverStock(zone);
		m_Front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Efface le dépôt (caisse, repère), MarkDirty(true) — RemoveDepot, OnZoneLost, DestroyDepot, OnCellsChanged (STAFF)
	protected void ClearDepot(SRP_FrontZone zone)
	{
		DropLive(zone);
		zone.m_bDepot = false;
		zone.m_vDepot = vector.Zero;
		zone.m_iDepotCell = -1;
		m_Front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! G7 : caisse supprimée, stock à 0, ClearDepot, radio DepotDestroyed (évènement depot_detruit) — OnCellsChanged
	//! (cause REPRISE seulement)
	protected void DestroyDepot(SRP_FrontZone zone)
	{
		string cellRef = DepotCellRef(zone);
		zone.m_iStock = 0;
		ClearDepot(zone);	// la caisse et son contenu partent avec le carré
		SRP_FrontRadio.DepotDestroyed(zone.m_iIndex, cellRef);
	}

	//------------------------------------------------------------------------------------------------
	//! Repère du dépôt (m_iDepotMarkerIcon, m_iDepotMarkerColor) par SCR_MapMarkerManagerComponent.InsertStaticMarker
	protected void PlaceDepotMarker(SRP_FrontZone zone)
	{
		RemoveDepotMarker(zone);
		if (!zone.m_bDepot)
			return;
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markers)
			return;
		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		int x = zone.m_vDepot[0];
		int z = zone.m_vDepot[2];
		marker.SetWorldPos(x, z);
		marker.SetIconEntry(m_Front.m_iDepotMarkerIcon);
		marker.SetColorEntry(m_Front.m_iDepotMarkerColor);
		marker.SetCustomText("Dépôt de " + m_Front.GetZoneLabel(zone.m_iIndex));
		markers.InsertStaticMarker(marker, false, true);
		zone.m_DepotMarker = marker;
	}

	//------------------------------------------------------------------------------------------------
	//! Au démarrage, une caisse par dépôt enregistré (SRP_Territory.c:692-704) — Start
	protected void RespawnBoxes()
	{
		int count = m_Front.GetZoneCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontZone zone = m_Front.GetZone(i);
			if (!zone || zone.m_bBase || zone.m_iOwner != SRP_EFrontOwner.BLEU || !zone.m_bDepot)
				continue;	// sans dépôt, le stock attend sans caisse
			PlaceDepotMarker(zone);
			DeliverStock(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés de la zone bleus et hors front (G6), pour le texte de refus — InstallDepot
	protected int CountSafeCells(SRP_FrontZone zone)
	{
		array<int> cells = {};
		m_Front.GetZoneCells(zone.m_iIndex, cells);
		int safe = 0;
		foreach (int cell : cells)
		{
			if (m_Front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
				continue;
			if (!m_Front.HasNeighbourOwnedBy(cell, SRP_EFrontOwner.ROUGE, false))
				safe++;
		}
		return safe;
	}

	//------------------------------------------------------------------------------------------------
	//! Après une lecture (démarrage, restauration) : un dépôt dont la zone n'est plus à nous, dont le carré a quitté la
	//! zone (retouche du fichier) ou n'est plus bleu est effacé, stock gardé, note à la console ; le carré des dépôts
	//! gardés est recalculé ; une zone qui n'est pas à nous n'a pas de stock — Start, OnFrontReset (RESTAURATION)
	protected void CheckLoadedDepots()
	{
		int count = m_Front.GetZoneCount();
		for (int i = 0; i < count; i++)
		{
			SRP_FrontZone zone = m_Front.GetZone(i);
			if (!zone)
				continue;
			bool ours = !zone.m_bBase && zone.m_iOwner == SRP_EFrontOwner.BLEU;
			if (!ours && zone.m_iStock != 0)
			{
				zone.m_iStock = 0;
				m_Front.MarkDirty(false);
			}
			if (!zone.m_bDepot)
			{
				zone.m_iDepotCell = -1;
				continue;
			}

			int cell = m_Front.CellIndexAt(zone.m_vDepot);
			string why;
			if (!ours)
				why = "la zone n'est pas à nous";
			else if (cell < 0 || m_Front.GetCellZone(cell) != i)
				why = "son carré n'est plus dans la zone";
			else if (m_Front.GetCellOwner(cell) != SRP_EFrontOwner.BLEU)
				why = "son carré n'est plus à nous";
			if (why.IsEmpty())
			{
				zone.m_iDepotCell = cell;
				continue;
			}
			ClearDepot(zone);
			Print(string.Format("[SRP] Front : dépôt de %1 effacé à la lecture (%2) ; %3 paquet(s) gardés en attente, il faudra un nouveau kit", m_Front.GetZoneLabel(i), why, zone.m_iStock), LogLevel.WARNING);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le stock suit la caisse vivante (paquets pris par les joueurs) ; sans caisse, rien ne change — Tick, Staff,
	//! OnCellsChanged (STAFF)
	protected void RefreshStock(SRP_FrontZone zone)
	{
		if (!zone.m_Box || zone.m_Box.IsDeleted())
			return;
		int resource = ResourceOf(zone);
		if (resource < 0)
			return;
		int inBox = SRP_ResourceManagerComponent.CountInBox(zone.m_Box, resource);
		if (inBox == zone.m_iStock)
			return;
		zone.m_iStock = inBox;
		m_Front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Stock, production et dépôt à zéro, caisse supprimée — OnZoneLost, OnFrontReset (ZONE, CAMPAGNE)
	protected void EmptyZone(SRP_FrontZone zone)
	{
		zone.m_iStock = 0;
		zone.m_iProductionSec = 0;
		ClearDepot(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire la caisse et l'icône vivantes sans toucher aux champs sauvegardés — ClearDepot, OnFrontReset
	//! (RESTAURATION)
	protected void DropLive(SRP_FrontZone zone)
	{
		if (zone.m_Box && !zone.m_Box.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(zone.m_Box);
		zone.m_Box = null;
		RemoveDepotMarker(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire l'icône du dépôt de la carte — PlaceDepotMarker, DropLive
	protected void RemoveDepotMarker(SRP_FrontZone zone)
	{
		if (!zone.m_DepotMarker)
			return;
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (markers)
			markers.RemoveStaticMarker(zone.m_DepotMarker);
		zone.m_DepotMarker = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Référence de carte du carré du dépôt, « 072 023 » (recalculée si le carré n'est pas encore connu)
	protected string DepotCellRef(SRP_FrontZone zone)
	{
		int cell = zone.m_iDepotCell;
		if (cell < 0)
			cell = m_Front.CellIndexAt(zone.m_vDepot);
		return m_Front.CellRef(cell);
	}
}

//------------------------------------------------------------------------------------------------
//! Action « Installer le dépôt ici » du kit (Prefabs/Paquets/SRP_KitDepot.et la trouve par son NOM de classe).
//! DÉPLACÉE depuis SRP_Territory.c (ancienne copie retirée) ; elle appelle maintenant l'économie du front.
//! Réservée aux logisticiens, aux officiers et au Staff (vérifié par le serveur, SRP_FrontEconomy.InstallDepot).
class SRP_DepotInstallAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Serveur : SRP_FrontComponent.GetInstance().GetEconomy().InstallDepot(joueur, kit), puis NotifyPlayer du refus
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int userId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (userId <= 0 || !front || !front.GetEconomy())
			return;
		string reply = front.GetEconomy().InstallDepot(userId, pOwnerEntity);
		if (!reply.IsEmpty())
			SRP_Utils.NotifyPlayer(userId, reply);
	}

	//------------------------------------------------------------------------------------------------
	//! « Installer le dépôt ici »
	override bool GetActionNameScript(out string outName)
	{
		outName = "Installer le dépôt ici";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Seulement quand le kit est posé au sol (ni dans un sac, ni dans un coffre)
	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;
		return owner.GetParent() == null;
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
