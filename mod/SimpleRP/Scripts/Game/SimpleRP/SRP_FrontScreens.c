//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : TEXTES DES MENUS (onglet Territoire du PC, onglet Territoire du menu Staff), serveur.
// Contrats figés (CONTRATS_FRONT.md §1.13 et §2, CONTRATS_CMD.md §8), corps écrits le 26/09.
//
// RÔLE : construire, côté serveur, les textes du protocole de menus existant (SRP_Admin.c:17-19, analyseur
// SRP_TerminalMenu.c:157-190 : lignes M|, H|, L|, C|teinte|, K|teinte|clé|, I|clé|, teintes w g r y d). Aucune
// entité, aucune RPC. Chaque texte libre passe par Esc (ni « | » ni retour à la ligne) ; string.Format 9 paramètres
// au plus ; jamais de signe pour cent.
// - PC (I10, I3) : toutes les zones regroupées (attaques, au front ennemies, au front à nous, arrière) ; présence
//   ennemie seulement pour les zones au front ; suffixes et section Renseignement du Commandeur
//   (SRP_CmdScreens.PCZoneSuffix, PCIntelSection).
// - Staff (J1 à J3, Q8) : page Territoire (ce carré, gel, zones au front, listes, sauvegardes datées, nouvelle
//   campagne, bloc « Ennemi du front » : attaque, offensive de la nuit, postes), page de zone, listes paginées, copies
//   datées, page des points clés (ce qui manque), page des postes du front, et l'exécution des actions (primitives
//   du socle, de la chute, de l'économie et de l'ennemi, trou 22). SRP_Admin ne fait que déléguer,
//   comme pour l'onglet « commandeur » (SRP_CmdScreens.StaffTab / RunStaff). Clés : « territoire:<action>[:<arg>…] » ;
//   les pages rendent des lignes qui commencent par « \n » (comme les onglets de SRP_Admin), le PC commence par « M| ».
// Les minutes d'attaque, les vagues et la progression « 95/180 s » ne s'affichent qu'ici (trou 20).
// APPELÉ PAR : SRP_PC (Territoire), SRP_Admin (Header, onglet territoire, Execute, NeedsConfirm, PageAfter, Run),
// SRP_ChatCommands (StatusPlain, bloc /territoire).
//------------------------------------------------------------------------------------------------

class SRP_FrontScreens
{
	static const int LIST_PAGE = 14;			// zones par page des listes Staff (comme les localités de SRP_Admin)
	static const int SAVES_PAGE = 10;			// copies datées par page
	static const int POINTS_PAGE = 5;			// localités par page des points clés
	static const float POSTS_RADIUS = 3000.0;	// page « postes » : rayon autour du Staff, en mètres

	//================================================================================================
	// PC (onglet Territoire)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Remplace SRP_PC.c:528-544 : « M|Territoire : 12 zones sur 66 à nous · 214/1395 carrés · menace 3/10 », gel,
	//! Attaques, Au front — zones ennemies (+ PCZoneSuffix), PCIntelSection, Au front — nos zones, Nos zones à
	//! l'arrière (coupées : E4), Zones ennemies à l'arrière (m_bPCRearEnemies, gris, « · région de X ») ; chaque
	//! groupe trié par distance au joueur — SRP_PCScreens.Territoire(playerId)
	static string PCTerritoire(int playerId)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "M|Front en préparation : la carte des zones arrive dans quelques secondes";

		string text = "M|" + Esc(SummaryText(front));
		if (front.IsVictory())
			text += "\nC|g|VICTOIRE : toute l'île est à nous";
		if (front.IsFrozen())
			text += "\nC|y|Front gelé par l'état-major : ni capture ni perte";

		array<int> attacks = {};
		array<int> enemyFront = {};
		array<int> ourFront = {};
		array<int> ourRear = {};
		array<int> enemyRear = {};
		SplitZones(front, attacks, enemyFront, ourFront, ourRear, enemyRear);
		vector from = PlayerPos(playerId);
		SortByDistance(front, attacks, from);
		SortByDistance(front, enemyFront, from);
		SortByDistance(front, ourFront, from);
		SortByDistance(front, ourRear, from);
		SortByDistance(front, enemyRear, from);

		if (!attacks.IsEmpty())
		{
			text += "\nH|Attaques";
			foreach (int attacked : attacks)
			{
				text += "\nC|" + AttackTint(front, attacked) + "|" + Esc(AttackLine(front, attacked));
			}
		}

		text += string.Format("\nH|Au front — zones ennemies (%1)", enemyFront.Count());
		foreach (int enemyZone : enemyFront)
		{
			// Le suffixe du Commandeur est déjà passé par Esc : collé tel quel
			text += "\nC|r|" + Esc(ZoneLine(enemyZone, false, vector.Zero)) + Suffix(SRP_CmdScreens.PCZoneSuffix(enemyZone));
		}
		if (enemyFront.IsEmpty())
			text += "\nL|Pas de contact avec l'ennemi";

		// Renseignement du Commandeur, juste après les zones ennemies du front (CONTRATS_FRONT §2)
		text += AsLines(SRP_CmdScreens.PCIntelSection(playerId));

		text += string.Format("\nH|Au front — nos zones (%1)", ourFront.Count());
		foreach (int ourZone : ourFront)
		{
			text += "\nC|" + ZoneTint(front, ourZone) + "|" + Esc(ZoneLine(ourZone, false, vector.Zero));
		}

		text += string.Format("\nH|Nos zones à l'arrière (%1)", ourRear.Count());
		foreach (int rearZone : ourRear)
		{
			text += "\nC|" + ZoneTint(front, rearZone) + "|" + Esc(ZoneLine(rearZone, false, vector.Zero));
		}

		if (front.m_bPCRearEnemies)
		{
			text += string.Format("\nH|Zones ennemies à l'arrière (%1) — pas de renseignement", enemyRear.Count());
			foreach (int farZone : enemyRear)
			{
				// I3 : rien que la part des carrés et la géographie (région), aucune présence ennemie
				text += "\nC|d|" + Esc(front.GetZoneLabel(farZone) + " — " + ZoneOwnerWord(farZone) + " · " + ShareText(farZone) + RegionSuffix(farZone));
			}
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Même contenu sans préfixes, limité aux attaques et aux zones au front — SRP_ChatCommands (/territoire)
	static string StatusPlain()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "Front en préparation : la carte des zones arrive dans quelques secondes";

		string text = SummaryText(front);
		if (front.IsVictory())
			text += "\nVICTOIRE : toute l'île est à nous";
		if (front.IsFrozen())
			text += "\nFront gelé par l'état-major : ni capture ni perte";

		array<int> attacks = {};
		array<int> enemyFront = {};
		array<int> ourFront = {};
		array<int> ourRear = {};
		array<int> enemyRear = {};
		SplitZones(front, attacks, enemyFront, ourFront, ourRear, enemyRear);

		if (!attacks.IsEmpty())
		{
			text += "\nAttaques :";
			foreach (int attacked : attacks)
			{
				text += "\n  " + AttackLine(front, attacked);
			}
		}
		text += string.Format("\nAu front — zones ennemies (%1) :", enemyFront.Count());
		foreach (int enemyZone : enemyFront)
		{
			text += "\n  " + ZoneLine(enemyZone, false, vector.Zero);
		}
		text += string.Format("\nAu front — nos zones (%1) :", ourFront.Count());
		foreach (int ourZone : ourFront)
		{
			text += "\n  " + ZoneLine(ourZone, false, vector.Zero);
		}
		return text;
	}

	//================================================================================================
	// Morceaux communs
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! En-tête Staff : « 3/10, 12/66 zones » (+ « , FRONT GELÉ ») ; SRP_Admin y colle SRP_CmdScreens.StaffHeaderPart
	//! par concaténation — SRP_AdminComponent.Header
	static string StaffHeaderPart()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "?";
		string text = string.Format("%1/10, %2/%3 zones", front.GetThreat(), front.CountZonesOurs(), front.CountZonesTotal());
		if (front.IsFrozen())
			text += ", FRONT GELÉ";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « Carré 074 042 · Perelle (S08) · ennemi · au front · capture 95/180 s » — SRP_Admin (page Rapide), StaffTerritoire
	static string CellHere(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "Front pas encore prêt";
		int cell = front.CellIndexAt(position);
		if (cell < 0)
			return "Hors de la grille du front (mer)";

		string text = "Carré " + front.CellRef(cell);
		if (!front.IsCellInPlay(cell))
			return text + " · hors jeu";
		int zone = front.GetCellZone(cell);
		if (zone >= 0)
			text += " · " + front.GetZoneLabel(zone);
		if (front.IsBaseCell(cell))
			return text + " · base";

		bool blue = front.GetCellOwner(cell) == SRP_EFrontOwner.BLEU;
		if (blue)
			text += " · à nous";
		else
			text += " · ennemi";
		if (front.IsCellAtFront(cell))
			text += " · au front";

		SRP_FrontCapture capture = front.GetCapture();
		if (capture)
		{
			int live = capture.GetLiveState(cell);
			if (!blue)
			{
				int captureSec = capture.GetCaptureSeconds(cell);
				int captureTotal = capture.GetCaptureNeedSeconds(cell);
				string blockText = capture.GetBlockText(cell);
				if (!blockText.IsEmpty())
					text += " · " + blockText;
				if (live == SRP_EFrontLive.COMBAT)
					text += string.Format(" · combat : capture figée à %1/%2 s", captureSec, captureTotal);
				else if (live == SRP_EFrontLive.CAPTURE || captureSec > 0)
					text += string.Format(" · capture %1/%2 s", captureSec, captureTotal);
			}
			else
			{
				int retakeSec = capture.GetRetakeSeconds(cell);
				if (live == SRP_EFrontLive.REPRISE || retakeSec > 0)
					text += string.Format(" · reprise par l'ennemi %1/%2 s", retakeSec, capture.GetRetakeTotalSeconds());
			}
		}

		SRP_ZoneFall zoneFall = front.GetZoneFall();
		if (!blue && zoneFall && zoneFall.IsCellSeizeOnly(cell))
			text += " · carré d'un PC ennemi : pris seulement par la saisie du poste";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « à nous », « ennemie » ou « base » — lignes de zone
	static string ZoneOwnerWord(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "?";
		if (front.IsBaseZone(zone))
			return "base";
		if (front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
			return "à nous";
		return "ennemie";
	}

	//------------------------------------------------------------------------------------------------
	//! « 6/25 carrés » — lignes de zone
	static string ShareText(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "?";
		return string.Format("%1/%2 carrés", front.CountZoneBlue(zone), front.CountZoneCells(zone));
	}

	//================================================================================================
	// Menu Staff, onglet Territoire (délégation depuis SRP_Admin, comme l'onglet commandeur)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Page demandée selon parts (« territoire », « zone:S08 », « liste:bleues:0 », « sauvegardes:0 », « points:0 »,
	//! « postes ») — SRP_AdminComponent.BuildMenu
	static string StaffTab(int playerId, vector staffPos, bool inGame, array<string> parts)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt (il démarre quelques secondes après le lancement du serveur)";

		// parts vient de la clé de page découpée par « : » ; « territoire » en tête est sauté s'il y est
		int first = 0;
		if (parts && parts.Count() > 0 && parts[0] == "territoire")
			first = 1;
		string page = Arg(parts, first);

		if (page == "zone")
		{
			string code = Arg(parts, first + 1);
			int zone = front.FindZone(code);
			if (zone < 0)
				return "\nL|Zone inconnue : " + Esc(code) + "\nI|page:territoire|« Retour";
			return StaffZone(zone, staffPos);
		}
		if (page == "liste")
		{
			string which = Arg(parts, first + 1);
			if (which.IsEmpty())
				which = "bleues";
			return StaffZoneList(which, PageNumber(Arg(parts, first + 2)), staffPos);
		}
		if (page == "sauvegardes")
			return StaffSaves(PageNumber(Arg(parts, first + 1)));
		if (page == "points")
			return StaffKeyPoints(PageNumber(Arg(parts, first + 1)), staffPos);
		if (page == "postes")
			return StaffPosts(staffPos, inGame);
		return StaffTerritoire(playerId, staffPos, inGame);
	}

	//------------------------------------------------------------------------------------------------
	//! Page principale : ici (CellHere, carre-bleu, carre-rouge), gel [x], menace et comptes, zones au front (liens
	//! zone:<code>), listes, sauvegardes, points clés (« N manquants »), nouvelle campagne (2 clics) ; le Staff voit tout
	//! (pas de filtre I3) + ligne SRP_CmdScreens.StaffTerritoryLine sous le gel ; bloc « H|Ennemi du front » (trou 22) :
	//! lignes I de SRP_FrontEnemyComponent.GetAttackReport() et GetNightReport(), K|r|territoire:attaque-annuler
	//! (CancelAttack, 2 clics, seulement si une attaque est en cours), K|r|territoire:offensive:1 et
	//! K|r|territoire:offensive:0 (ForceOffensive réussite / échec, 2 clics), lien « postes » — StaffTab
	static string StaffTerritoire(int playerId, vector staffPos, bool inGame)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt (il démarre quelques secondes après le lancement du serveur)";

		string text = "";

		// --- Ce carré (J1) ---
		if (inGame)
		{
			int cell = front.CellIndexAt(staffPos);
			int hereZone = -1;
			if (cell >= 0)
				hereZone = front.GetCellZone(cell);
			text += "\nH|Ici, où je me tiens";
			text += "\nC|" + CellTint(front, cell) + "|" + Esc(CellHere(staffPos));
			if (cell >= 0 && front.IsCellInPlay(cell) && !front.IsBaseCell(cell))
			{
				text += "\nK|g|territoire:carre-bleu|» Ce carré passe BLEU (correction : ni prime, ni menace, ni radio)";
				text += "\nK|r|territoire:carre-rouge|» Ce carré passe ROUGE (correction)";
			}
			if (hereZone >= 0)
				text += "\nI|territoire:zone:" + front.GetZoneCode(hereZone) + "|» Ouvrir la zone " + Esc(front.GetZoneLabel(hereZone));
		}
		else
		{
			text += "\nH|Ici";
			text += "\nL|Hors jeu : les actions « ce carré » et « dépôt ici » demandent d'être dans un personnage";
		}

		// --- Front (J3, menace, réglages) ---
		text += "\nH|Front";
		if (front.IsFrozen())
			text += "\nK|g|territoire:gel|[x] Front gelé : ON — ni capture, ni perte, ni contre-attaque";
		else
			text += "\nK|w|territoire:gel|[ ] Front gelé : OFF";
		text += AsLines(SRP_CmdScreens.StaffTerritoryLine());
		text += "\nL|" + Esc(string.Format("Menace %1/10 · %2 zones sur %3 à nous · %4/%5 carrés · campagne %6", front.GetThreat(), front.CountZonesOurs(), front.CountZonesTotal(), front.CountBlueCells(), front.CountLandCells(), front.GetCampaign()));
		if (front.IsVictory())
			text += "\nC|g|VICTOIRE : toutes les zones sont à nous";
		text += "\nL|Réglages : " + Esc(SRP_CmdSettings.GetStatusText());
		text += "\nI|territoire:relire|» Relire les réglages (front_reglages.txt et commandeur_reglages.txt)";

		// --- Ennemi du front (trou 22) ---
		text += "\nH|Ennemi du front";
		SRP_FrontEnemyComponent enemyFront = SRP_FrontEnemyComponent.GetInstance();
		if (!enemyFront)
		{
			text += "\nL|Ennemi du front absent du game mode (SRP_FrontEnemyComponent)";
		}
		else
		{
			// Le rapport n'est jamais vide (repos : « Aucune contre-attaque en cours · horloge… ») : orange seulement
			// pendant une attaque
			string attackReport = enemyFront.GetAttackReport();
			if (attackReport.IsEmpty())
				text += "\nL|Aucune contre-attaque en cours";
			else if (enemyFront.IsCounterAttackActive())
				text += InfoLines(attackReport, "y");
			else
				text += InfoLines(attackReport, "");
			if (enemyFront.IsCounterAttackActive())
				text += "\nK|r|territoire:attaque-annuler|!! Annuler la contre-attaque en cours (2 clics)";
			text += InfoLines(enemyFront.GetNightReport(), "");
			text += "\nK|r|territoire:offensive:1|!! Offensive de la nuit maintenant, RÉUSSITE imposée (2 clics)";
			text += "\nK|r|territoire:offensive:0|!! Offensive de la nuit maintenant, ÉCHEC imposé (2 clics)";
			text += "\nI|territoire:postes|» Postes du front autour de moi (3 km)…";
		}

		// --- Zones au front : le Staff voit tout (pas de filtre I3) ---
		array<int> frontZones = {};
		int cutCount = 0;
		int zoneCount = front.GetZoneCount();
		for (int zone = 0; zone < zoneCount; zone++)
		{
			SRP_FrontZone z = front.GetZone(zone);
			if (!z || z.m_bBase)
				continue;
			if (front.IsZoneCut(zone))
				cutCount++;
			if (z.IsNearFront() || front.GetZoneAttackState(zone) != SRP_EFrontAttack.AUCUNE)
				frontZones.Insert(zone);
		}
		vector from = vector.Zero;
		if (inGame)
			from = staffPos;
		SortByDistance(front, frontZones, from);
		text += string.Format("\nH|Zones au front (%1) — choisis une zone", frontZones.Count());
		foreach (int frontZone : frontZones)
		{
			text += "\nK|" + ZoneTint(front, frontZone) + "|territoire:zone:" + front.GetZoneCode(frontZone) + "|" + Esc(ZoneLine(frontZone, true, from));
		}
		if (frontZones.IsEmpty())
			text += "\nL|Pas de front pour l'instant";

		// --- Autres zones ---
		int ours = front.CountZonesOurs();
		int total = front.CountZonesTotal();
		text += "\nH|Autres zones";
		text += string.Format("\nI|territoire:liste:bleues:0|» Nos zones (%1)…", ours);
		text += string.Format("\nI|territoire:liste:rouges:0|» Zones ennemies (%1)…", total - ours);
		text += string.Format("\nI|territoire:liste:coupees:0|» Zones coupées de la base (%1)…", cutCount);

		// --- Points clés (Q8) ---
		array<string> keyReport = {};
		int missing = front.GetKeyPointReport(keyReport);
		text += "\nH|Points clés";
		if (missing > 0)
			text += string.Format("\nK|y|territoire:points:0|» Points clés : %1 manquant(s), à poser au Workbench…", missing);
		else
			text += string.Format("\nI|territoire:points:0|» Points clés (%1) : tous posés…", front.GetKeyPointTotal());

		// --- Sauvegardes et remise ---
		text += "\nH|Sauvegardes datées";
		text += "\nI|territoire:sauvegardes:0|» Revenir à l'état d'un jour…";
		text += "\nH|Remise à zéro";
		text += "\nK|r|territoire:reset|!! Nouvelle campagne : tout rouge sauf la base, menace 0 (2 clics)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page d'une zone : part bleue, contestés, contact, coupure, points clés et chute (GetZoneFallText), garnison
	//! (groupes, ennemis en zone), attaque (GetAttackStatusText), production (StatusText), dernier changement,
	//! distance ; actions tp, tp-centre, zone-bleue, zone-rouge, zone-reset, attaque, depot-ici, depot-retirer — StaffTab
	static string StaffZone(int zone, vector staffPos)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt\nI|page:territoire|« Retour";
		SRP_FrontZone z = front.GetZone(zone);
		if (!z)
			return "\nL|Zone inconnue\nI|page:territoire|« Retour";

		string code = front.GetZoneCode(zone);
		bool ours = front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU;
		int attack = front.GetZoneAttackState(zone);

		string head = front.GetZoneLabel(zone) + " — " + ZoneOwnerWord(zone);
		if (!z.m_bBase && z.m_bShield)
			head += " · protégée (touche la base)";
		string text = "\nH|" + Esc(head);

		// Genre et localités
		array<int> localities = {};
		front.GetZoneLocalities(zone, localities);
		string places = "";
		foreach (int localityIndex : localities)
		{
			SRP_FrontLocality locality = front.GetLocality(localityIndex);
			if (!locality)
				continue;
			if (!places.IsEmpty())
				places += ", ";
			places += locality.m_sName + " (" + locality.m_sKind + ")";
		}
		if (places.IsEmpty())
			places = "sans localité (tombe à la part de carrés bleus)";
		text += "\nL|" + Esc("Genre : " + KindWord(z.m_iKind) + " · localités : " + places);

		// Carrés
		int capturing;
		int fighting;
		int retaking;
		CountLive(front, zone, capturing, fighting, retaking);
		text += "\nL|" + Esc(string.Format("Carrés : %1/%2 à nous · %3 contesté(s) · près du front : %4 · coupée de la base : %5", front.CountZoneBlue(zone), front.CountZoneCells(zone), capturing + fighting + retaking, SRP_Utils.OuiNon(z.IsNearFront()), SRP_Utils.OuiNon(front.IsZoneCut(zone))));

		// Chute ou perte (D1, Q2) et points clés
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		if (zoneFall)
		{
			string fallText = zoneFall.GetZoneFallText(zone);
			if (!fallText.IsEmpty())
				text += "\nL|" + Esc("État : " + fallText);
		}
		int points = front.GetKeyPointCount(zone);
		for (int k = 0; k < points; k++)
		{
			SRP_FrontKeyPoint point = front.GetKeyPoint(front.GetKeyPointIndex(zone, k));
			if (!point)
				continue;
			string owner = "";
			SRP_FrontLocality pointLocality = front.GetLocality(point.m_iLocality);
			if (pointLocality)
				owner = " de " + pointLocality.m_sName;
			text += "\nC|" + SourceTint(point) + "|" + Esc("Point clé " + RoleWord(point.m_iRole) + owner + " : " + KeyPointState(front, zoneFall, point, staffPos));
		}
		if (points == 0 && !z.m_bBase)
			text += "\nL|Points clés : aucun (zone sans localité, Q4)";

		// Ennemis (le Staff voit tout)
		int groups = 0;
		int enemies = 0;
		SRP_FrontEnemyComponent enemyFront = SRP_FrontEnemyComponent.GetInstance();
		if (enemyFront)
			groups = enemyFront.GetZoneGarrisonGroups(zone);
		SRP_FrontCapture capture = front.GetCapture();
		if (capture)
			enemies = capture.CountFitEnemiesInZone(zone);
		text += string.Format("\nL|Garnison : %1 groupe(s), %2 ennemi(s) en état de combattre dans la zone", groups, enemies);
		int planned;
		int nominal;
		if (enemyFront && enemyFront.GetZoneStrength(zone, planned, nominal))
			text += string.Format("\nL|Localité principale : %1 soldat(s) prévu(s), nominal %2", planned, nominal);

		// Attaque, coupure, production
		if (attack != SRP_EFrontAttack.AUCUNE)
			text += "\nC|y|" + Esc("Attaque : " + AttackText(front, zone, attack));
		if (front.IsZoneCut(zone))
			text += "\nC|y|" + Esc("Coupée : " + CutText(front, zone));
		SRP_FrontEconomy economy = front.GetEconomy();
		if (ours && !z.m_bBase && economy)
			text += "\nL|" + Esc("Production : " + economy.StatusText(zone));

		string last = front.GetZoneLastChange(zone);
		if (last.IsEmpty())
			last = "jamais";
		string lastLine = "Dernier changement : " + last;
		if (staffPos != vector.Zero)
			lastLine += " · " + DistanceText(vector.DistanceXZ(staffPos, front.GetZoneCentroid(zone))) + " de moi";
		text += "\nL|" + Esc(lastLine);

		// Actions
		text += "\nH|Actions";
		text += "\nI|territoire:tp:" + code + "|» Aller au point clé (sinon au point de mission de la zone)";
		text += "\nI|territoire:tp-centre:" + code + "|» Aller au centre de la zone";
		if (!z.m_bBase)
		{
			text += "\nK|g|territoire:zone-bleue:" + code + "|» Zone entière BLEUE, points clés compris (correction : ni prime, ni menace, ni radio)";
			text += "\nK|r|territoire:zone-rouge:" + code + "|» Zone entière ROUGE, dépôt effacé (correction : ni menace, ni radio)";
			text += "\nK|y|territoire:zone-reset:" + code + "|!! Remettre la zone à son état de départ (2 clics)";
			bool attackRunning = enemyFront && enemyFront.IsCounterAttackActive();
			if (ours && !z.m_bShield && !attackRunning && front.IsZoneAtFront(zone))
				text += "\nK|y|territoire:attaque:" + code + "|» Forcer une contre-attaque maintenant";
			if (ours)
			{
				// Hors jeu (SRP_Admin passe vector.Zero), « dépôt ici » n'est pas proposé
				if (staffPos != vector.Zero)
					text += "\nI|territoire:depot-ici:" + code + "|» Fixer le dépôt de la zone ICI, où je me tiens (sans kit)";
				if (z.m_bDepot)
					text += "\nK|y|territoire:depot-retirer:" + code + "|» Retirer le dépôt (le stock reste, il faudra un nouveau kit)";
			}
		}
		text += "\nI|page:territoire|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Liste paginée « bleues », « rouges » ou « coupees » (une ligne K par zone, suivant / précédent / retour) — StaffTab
	static string StaffZoneList(string which, int page, vector staffPos)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt\nI|page:territoire|« Retour";

		string kind = which;
		string title = "Nos zones";
		if (kind == "rouges")
			title = "Zones ennemies";
		else if (kind == "coupees")
			title = "Zones coupées de la base";
		else
			kind = "bleues";

		array<int> zones = {};
		int zoneCount = front.GetZoneCount();
		for (int zone = 0; zone < zoneCount; zone++)
		{
			SRP_FrontZone z = front.GetZone(zone);
			if (!z || z.m_bBase)
				continue;
			bool ours = front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU;
			if (kind == "bleues" && ours)
				zones.Insert(zone);
			else if (kind == "rouges" && !ours)
				zones.Insert(zone);
			else if (kind == "coupees" && front.IsZoneCut(zone))
				zones.Insert(zone);
		}
		SortByDistance(front, zones, staffPos);

		int total = zones.Count();
		if (total == 0)
			return "\nH|" + title + " (0)\nL|Aucune pour l'instant\nI|page:territoire|« Retour";

		int pages = (total + LIST_PAGE - 1) / LIST_PAGE;
		int current = Math.ClampInt(page, 0, pages - 1);
		string text = string.Format("\nH|%1 (%2), page %3 / %4", title, total, current + 1, pages);
		int first = current * LIST_PAGE;
		int last = Math.MinInt(first + LIST_PAGE, total);
		for (int i = first; i < last; i++)
		{
			int listed = zones[i];
			text += "\nK|" + ZoneTint(front, listed) + "|territoire:zone:" + front.GetZoneCode(listed) + "|" + Esc(ZoneLine(listed, true, staffPos));
		}
		if (current + 1 < pages)
			text += string.Format("\nI|territoire:liste:%1:%2|› Page suivante", kind, current + 1);
		if (current > 0)
			text += string.Format("\nI|territoire:liste:%1:%2|‹ Page précédente", kind, current - 1);
		text += "\nI|page:territoire|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Copies datées (J2) : avertissement, puis « K|r|territoire:restaurer:2026-09-25|!! 25/09/2026 — 14 zones… (2
	//! clics) », les plus récentes d'abord — StaffTab
	static string StaffSaves(int page)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt\nI|page:territoire|« Retour";

		array<string> days = {};
		array<string> summaries = {};
		int total = front.GetDailyCopies(days, summaries);
		string text = string.Format("\nH|Sauvegardes datées (%1)", total);
		text += "\nC|y|Revenir en arrière remet TOUTE la carte (zones, carrés, menace, dépôts de zone) à l'état de ce jour, pour tous les joueurs.";
		if (total == 0)
			return text + "\nL|Pas encore de copie datée (une par jour, écrite par le serveur)\nI|page:territoire|« Retour";

		int pages = (total + SAVES_PAGE - 1) / SAVES_PAGE;
		int current = Math.ClampInt(page, 0, pages - 1);
		if (pages > 1)
			text += string.Format("\nL|Page %1 / %2", current + 1, pages);
		int first = current * SAVES_PAGE;
		int last = Math.MinInt(first + SAVES_PAGE, total);
		for (int i = first; i < last; i++)
		{
			string summary = "";
			if (i < summaries.Count())
				summary = " — " + summaries[i];
			text += "\nK|r|territoire:restaurer:" + days[i] + "|" + Esc("!! " + DayText(days[i]) + summary + " (2 clics)");
		}
		if (current + 1 < pages)
			text += string.Format("\nI|territoire:sauvegardes:%1|› Page suivante", current + 1);
		if (current > 0)
			text += string.Format("\nI|territoire:sauvegardes:%1|‹ Page précédente", current - 1);
		text += "\nI|page:territoire|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Points clés (Q8) : une ligne par localité (genre, zone, source de chaque point : repère posé au Workbench ou
	//! MANQUANT — nom de la carte), état du poste (GetPostReport), distance ; actions pc-tp, pc-saisir, pc-reposer — StaffTab
	static string StaffKeyPoints(int page, vector staffPos)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt\nI|page:territoire|« Retour";

		array<string> keyReport = {};
		int missing = front.GetKeyPointReport(keyReport);
		int total = front.GetLocalityCount();
		string text = string.Format("\nH|Points clés — %1 point(s), %2 localité(s), %3 manquant(s)", front.GetKeyPointTotal(), total, missing);
		if (missing > 0)
			text += "\nC|y|MANQUANT : centre pris sur le nom de la carte, ou QG absent d'une ville. Poser le repère SRP_PointCle_Centre ou SRP_PointCle_QG au Workbench, puis republier.";
		if (total == 0)
			return text + "\nL|Aucune localité de zone\nI|page:territoire|« Retour";

		int pages = (total + POINTS_PAGE - 1) / POINTS_PAGE;
		int current = Math.ClampInt(page, 0, pages - 1);
		text += string.Format("\nL|Localités, page %1 / %2", current + 1, pages);
		SRP_ZoneFall zoneFall = front.GetZoneFall();
		int first = current * POINTS_PAGE;
		int last = Math.MinInt(first + POINTS_PAGE, total);
		for (int i = first; i < last; i++)
		{
			SRP_FrontLocality locality = front.GetLocality(i);
			if (!locality)
				continue;
			string zoneText = "hors zone";
			if (locality.m_iZone >= 0)
				zoneText = front.GetZoneLabel(locality.m_iZone);
			text += "\nH|" + Esc(locality.m_sName + " — " + locality.m_sKind + " · " + zoneText);
			text += KeyPointBlock(front, zoneFall, i, locality.m_iCentre, SRP_EKeyPointRole.CENTRE, staffPos);
			if (locality.m_iKind == SRP_EZoneKind.VILLE || locality.m_iQg >= 0)
				text += KeyPointBlock(front, zoneFall, i, locality.m_iQg, SRP_EKeyPointRole.QG, staffPos);
			text += string.Format("\nI|territoire:pc-reposer:%1|» Reposer le poste de %2 (retiré puis reposé hors de vue)", i, Esc(locality.m_sName));
		}
		if (current + 1 < pages)
			text += string.Format("\nI|territoire:points:%1|› Page suivante", current + 1);
		if (current > 0)
			text += string.Format("\nI|territoire:points:%1|‹ Page précédente", current - 1);

		// État de tous les postes (première page seulement : la liste est celle de la chute des zones)
		if (current == 0 && zoneFall)
		{
			array<string> posts = {};
			zoneFall.GetPostReport(posts);
			text += string.Format("\nH|État des postes (%1)", posts.Count());
			foreach (string post : posts)
			{
				text += "\nL|" + Esc(post);
			}
		}
		text += "\nI|page:territoire|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page « postes » : SRP_FrontEnemyComponent.GetPostsReport(staffPos, 3000) en lignes I (Staff hors jeu : tous les
	//! postes), puis retour vers « territoire » — StaffTab
	static string StaffPosts(vector staffPos, bool inGame)
	{
		SRP_FrontEnemyComponent enemyFront = SRP_FrontEnemyComponent.GetInstance();
		if (!enemyFront)
			return "\nL|Ennemi du front absent du game mode (SRP_FrontEnemyComponent)\nI|page:territoire|« Retour";

		string text;
		string report;
		if (inGame)
		{
			text = "\nH|Postes du front à moins de 3 km";
			report = enemyFront.GetPostsReport(staffPos, POSTS_RADIUS);
		}
		else
		{
			// Hors jeu : tous les postes de l'île
			text = "\nH|Postes du front (hors jeu : tous)";
			report = enemyFront.GetPostsReport(vector.Zero, 100000);
		}
		if (report.IsEmpty())
			text += "\nL|Aucun poste";
		else
			text += InfoLines(report, "");
		text += "\nI|page:territoire|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Exécute une action de l'onglet : reset -> NewCampaign ; gel -> SetFrozen ; carre-bleu / carre-rouge ->
	//! ForceCellAt ; tp, tp-centre (wantTeleport) ; zone-bleue / zone-rouge -> ForceZone ; zone-reset -> ResetZone ;
	//! attaque -> SRP_FrontEnemyComponent.ForceAttack ; attaque-annuler -> CancelAttack ; offensive:1 / offensive:0 ->
	//! ForceOffensive(true / false) ; depot-ici / depot-retirer -> économie ; restaurer -> RestoreDaily ; pc-tp,
	//! pc-saisir (ForceSeize), pc-reposer (RespawnCP) ; puis UNE ligne de journal [STAFF] (la seule pour l'action :
	//! les primitives appelées n'en écrivent pas) ; rend le compte rendu — SRP_AdminComponent.Run (qui téléporte si
	//! wantTeleport)
	static string RunStaff(int playerId, vector staffPos, bool inGame, string action, array<string> parts, string author, out bool wantTeleport, out vector teleportTo)
	{
		wantTeleport = false;
		teleportTo = vector.Zero;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "Front pas encore prêt";

		int first = ArgStart(action, parts);
		string arg = Arg(parts, first);
		string result = "";
		string what = "";

		if (action == "reset")
		{
			result = front.NewCampaign(author);
			what = "nouvelle campagne du front";
		}
		else if (action == "gel")
		{
			bool freeze = !front.IsFrozen();
			result = front.SetFrozen(freeze, author);
			what = "gel du front " + SRP_Utils.OnOff(freeze);
		}
		else if (action == "relire")
		{
			result = front.ReloadSettings(author);
			what = "réglages du front et du Commandeur relus";
		}
		else if (action == "carre-bleu" || action == "carre-rouge")
		{
			if (!inGame)
				return "Il faut être en jeu, dans un personnage";
			int owner = SRP_EFrontOwner.ROUGE;
			string color = "ROUGE";
			if (action == "carre-bleu")
			{
				owner = SRP_EFrontOwner.BLEU;
				color = "BLEU";
			}
			string cellText = "hors grille";
			int cell = front.CellIndexAt(staffPos);
			if (cell >= 0)
				cellText = front.CellRef(cell);
			result = front.ForceCellAt(staffPos, owner, author);
			what = "carré " + cellText + " forcé " + color + " (correction)";
		}
		else if (action == "attaque-annuler" || action == "offensive")
		{
			SRP_FrontEnemyComponent enemyFront = SRP_FrontEnemyComponent.GetInstance();
			if (!enemyFront)
				return "Ennemi du front absent du game mode";
			if (action == "attaque-annuler")
			{
				result = enemyFront.CancelAttack(author);
				what = "contre-attaque en cours annulée";
			}
			else
			{
				bool success = arg == "1";
				result = enemyFront.ForceOffensive(success, author);
				if (success)
					what = "offensive de la nuit forcée, réussite imposée";
				else
					what = "offensive de la nuit forcée, échec imposé";
			}
		}
		else if (action == "restaurer")
		{
			if (arg.IsEmpty())
				return "Jour manquant";
			result = front.RestoreDaily(arg, author);
			what = "front ramené à l'état du " + DayText(arg);
		}
		else if (action == "pc-tp" || action == "pc-saisir" || action == "pc-reposer")
		{
			if (!SRP_Utils.IsNumeric(arg))
				return "Localité inconnue : " + arg;
			SRP_FrontLocality locality = front.GetLocality(arg.ToInt());
			if (!locality)
				return "Localité inconnue : " + arg;
			int role = SRP_EKeyPointRole.CENTRE;
			if (Arg(parts, first + 1) == "1")
				role = SRP_EKeyPointRole.QG;
			if (action == "pc-tp")
			{
				int keyPoint = locality.m_iCentre;
				if (role == SRP_EKeyPointRole.QG)
					keyPoint = locality.m_iQg;
				SRP_FrontKeyPoint point = front.GetKeyPoint(keyPoint);
				if (!point)
					return "Point clé absent : " + RoleWord(role) + " de " + locality.m_sName;
				wantTeleport = true;
				teleportTo = point.m_vPos + Vector(3, 0, 3);
				result = "Téléporté au point clé " + RoleWord(role) + " de " + locality.m_sName;
				what = "téléportation au point clé " + RoleWord(role) + " de " + locality.m_sName;
			}
			else
			{
				SRP_ZoneFall zoneFall = front.GetZoneFall();
				if (!zoneFall)
					return "Chute des zones absente";
				if (action == "pc-saisir")
				{
					result = zoneFall.ForceSeize(locality.m_sName, role, author);
					what = "saisie forcée du poste " + RoleWord(role) + " de " + locality.m_sName;
				}
				else
				{
					result = zoneFall.RespawnCP(locality.m_sName);
					what = "poste de " + locality.m_sName + " reposé";
				}
			}
		}
		else
		{
			// Actions sur une zone : « territoire:<action>:<code> »
			int zone = front.FindZone(arg);
			if (zone < 0)
				return "Zone inconnue : " + arg;
			string label = front.GetZoneLabel(zone);
			if (action == "tp")
			{
				wantTeleport = true;
				teleportTo = front.GetMissionPoint(zone) + Vector(3, 0, 3);
				result = "Téléporté : " + label;
				what = "téléportation vers " + label;
			}
			else if (action == "tp-centre")
			{
				wantTeleport = true;
				teleportTo = front.GetZoneCentroid(zone) + Vector(3, 0, 3);
				result = "Téléporté au centre de " + label;
				what = "téléportation au centre de " + label;
			}
			else if (action == "zone-bleue")
			{
				result = front.ForceZone(zone, SRP_EFrontOwner.BLEU, author);
				what = "zone " + label + " forcée BLEUE (correction)";
			}
			else if (action == "zone-rouge")
			{
				result = front.ForceZone(zone, SRP_EFrontOwner.ROUGE, author);
				what = "zone " + label + " forcée ROUGE (correction)";
			}
			else if (action == "zone-reset")
			{
				result = front.ResetZone(zone, author);
				what = "zone " + label + " remise à son état de départ";
			}
			else if (action == "attaque")
			{
				SRP_FrontEnemyComponent attacker = SRP_FrontEnemyComponent.GetInstance();
				if (!attacker)
					return "Ennemi du front absent du game mode";
				result = attacker.ForceAttack(zone, author);
				what = "contre-attaque forcée sur " + label;
			}
			else if (action == "depot-ici" || action == "depot-retirer")
			{
				SRP_FrontEconomy economy = front.GetEconomy();
				if (!economy)
					return "Économie du front absente";
				if (action == "depot-ici")
				{
					if (!inGame)
						return "Il faut être en jeu, dans un personnage";
					string depotCell = "hors grille";
					int here = front.CellIndexAt(staffPos);
					if (here >= 0)
						depotCell = front.CellRef(here);
					result = economy.ForceDepot(zone, staffPos, author);
					what = "dépôt de " + label + " fixé au carré " + depotCell;
				}
				else
				{
					result = economy.RemoveDepot(zone, author);
					what = "dépôt de " + label + " retiré";
				}
			}
			else
			{
				return "Action inconnue : " + action;
			}
		}

		if (result.IsEmpty())
			result = "Fait : " + what;
		// UNE ligne [STAFF] par action (les primitives n'en écrivent pas)
		SRP_JournalComponent.Log("STAFF", author + " : " + what + " — " + result);
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Actions qui ouvrent une page sans rien exécuter (zone, liste, sauvegardes, points, postes) —
	//! SRP_AdminComponent.Execute
	static bool IsInternalPage(string action)
	{
		return action == "zone" || action == "liste" || action == "sauvegardes" || action == "points" || action == "postes";
	}

	//------------------------------------------------------------------------------------------------
	//! Actions à deux clics (reset, restaurer, zone-reset, attaque-annuler, offensive) — SRP_AdminComponent.NeedsConfirm
	static bool NeedsConfirm(string action)
	{
		return action == "reset" || action == "restaurer" || action == "zone-reset" || action == "attaque-annuler" || action == "offensive";
	}

	//------------------------------------------------------------------------------------------------
	//! Page à rouvrir après une action (zone:<code>, sauvegardes:0, territoire ; attaque-annuler et offensive ->
	//! territoire) — SRP_AdminComponent.PageAfter
	static string PageAfter(string action, array<string> parts)
	{
		int first = ArgStart(action, parts);
		string arg = Arg(parts, first);

		// Restauration : la page où se trouve ce jour (le second clic doit retrouver la même ligne)
		if (action == "restaurer")
		{
			int savePage = SavePageOf(arg);
			return "territoire:sauvegardes:" + savePage.ToString();
		}
		if (action == "pc-tp" || action == "pc-saisir" || action == "pc-reposer")
		{
			int pointsPage = 0;
			if (SRP_Utils.IsNumeric(arg))
				pointsPage = arg.ToInt() / POINTS_PAGE;
			return "territoire:points:" + pointsPage.ToString();
		}
		bool zoneAction = action == "tp" || action == "tp-centre" || action == "zone-bleue" || action == "zone-rouge" || action == "zone-reset";
		zoneAction = zoneAction || action == "attaque" || action == "depot-ici" || action == "depot-retirer";
		if (zoneAction && !arg.IsEmpty())
			return "territoire:zone:" + arg;
		// reset, gel, relire, carre-bleu, carre-rouge, attaque-annuler, offensive
		return "territoire";
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Le texte d'une ligne de zone du PC ou du Staff, sans préfixe ni teinte (ZoneTint) : libellé, propriétaire,
	//! part, attaque, points clés, carrés contestés, présence ennemie (Staff : toujours ; PC : seulement près du front,
	//! I3), coupure, production, distance si from n'est pas nul
	protected static string ZoneLine(int zone, bool staff, vector from)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "";
		SRP_FrontZone z = front.GetZone(zone);
		if (!z)
			return "";

		bool ours = front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU;
		string text = front.GetZoneLabel(zone) + " — " + ZoneOwnerWord(zone) + " · " + ShareText(zone);

		int attack = front.GetZoneAttackState(zone);
		if (attack != SRP_EFrontAttack.AUCUNE)
			text += " · " + AttackText(front, zone, attack);

		if (!ours && !z.m_bBase)
			text += " · " + KeyPointsShort(front, zone);

		int capturing;
		int fighting;
		int retaking;
		CountLive(front, zone, capturing, fighting, retaking);
		if (capturing > 0)
			text += string.Format(" · %1 carré(s) en cours de capture", capturing);
		if (fighting > 0)
			text += string.Format(" · %1 carré(s) au combat", fighting);
		if (retaking > 0)
			text += string.Format(" · %1 carré(s) en cours de reprise par l'ennemi", retaking);

		SRP_FrontCapture capture = front.GetCapture();
		if (staff)
		{
			int enemies = 0;
			if (capture)
				enemies = capture.CountFitEnemiesInZone(zone);
			if (!ours)
			{
				int groups = 0;
				SRP_FrontEnemyComponent enemyFront = SRP_FrontEnemyComponent.GetInstance();
				if (enemyFront)
					groups = enemyFront.GetZoneGarrisonGroups(zone);
				text += string.Format(" · garnison %1 groupe(s), %2 en zone", groups, enemies);
			}
			else if (enemies > 0)
			{
				text += string.Format(" · %1 ennemi(s) en zone", enemies);
			}
		}
		else if (z.IsNearFront() && capture)
		{
			// I3 : la présence ennemie n'est dite que pour les zones près du front
			int seen = capture.CountFitEnemiesInZone(zone);
			if (seen > 0)
				text += string.Format(" · %1 ennemi(s) repéré(s) en zone", seen);
		}

		if (front.IsZoneCut(zone))
		{
			text += " · " + CutText(front, zone);
		}
		else if (ours && !z.m_bBase)
		{
			SRP_FrontEconomy economy = front.GetEconomy();
			if (economy && (economy.PerHour(zone) > 0 || z.m_iStock > 0 || z.m_bDepot))
				text += " · " + economy.StatusText(zone);
		}

		if (from != vector.Zero)
			text += " · " + DistanceText(vector.DistanceXZ(from, front.GetZoneCentroid(zone)));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte libre sans « | » ni retour à la ligne (comme SRP_AdminComponent.Esc)
	protected static string Esc(string text)
	{
		string s = text;
		s.Replace("|", "/");
		s.Replace("\n", " · ");
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! « 1240 m » ou « 3,4 km »
	protected static string DistanceText(float meters)
	{
		if (meters < 1000)
		{
			int rounded = Math.Round(meters);
			return string.Format("%1 m", rounded);
		}
		int tenths = Math.Round(meters / 100);
		int km = tenths / 10;
		int tenth = tenths - km * 10;
		return string.Format("%1,%2 km", km, tenth);
	}

	//------------------------------------------------------------------------------------------------
	//! « Territoire : 12 zones sur 66 à nous · 214/1395 carrés · menace 3/10 »
	protected static string SummaryText(SRP_FrontComponent front)
	{
		return string.Format("Territoire : %1 zones sur %2 à nous · %3/%4 carrés · menace %5/10", front.CountZonesOurs(), front.CountZonesTotal(), front.CountBlueCells(), front.CountLandCells(), front.GetThreat());
	}

	//------------------------------------------------------------------------------------------------
	//! Répartit les zones jouables (sans la base) : attaquées, ennemies près du front, à nous près du front, à nous à
	//! l'arrière, ennemies à l'arrière. Une zone attaquée n'est listée qu'une fois, dans les attaques.
	protected static void SplitZones(SRP_FrontComponent front, notnull array<int> attacks, notnull array<int> enemyFront, notnull array<int> ourFront, notnull array<int> ourRear, notnull array<int> enemyRear)
	{
		int count = front.GetZoneCount();
		for (int zone = 0; zone < count; zone++)
		{
			SRP_FrontZone z = front.GetZone(zone);
			if (!z || z.m_bBase)
				continue;
			bool ours = front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU;
			if (front.GetZoneAttackState(zone) != SRP_EFrontAttack.AUCUNE)
				attacks.Insert(zone);
			else if (z.IsNearFront() && ours)
				ourFront.Insert(zone);
			else if (z.IsNearFront())
				enemyFront.Insert(zone);
			else if (ours)
				ourRear.Insert(zone);
			else
				enemyRear.Insert(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Trie les zones par distance croissante à from (centre de gravité, plan XZ) ; rien si from est nul
	protected static void SortByDistance(SRP_FrontComponent front, notnull array<int> zones, vector from)
	{
		if (from == vector.Zero)
			return;
		array<float> keys = {};
		foreach (int zone : zones)
		{
			keys.Insert(vector.DistanceXZ(from, front.GetZoneCentroid(zone)));
		}
		// Tri par insertion (quelques dizaines de zones au plus)
		for (int i = 1; i < zones.Count(); i++)
		{
			int movingZone = zones[i];
			float movingKey = keys[i];
			int j = i - 1;
			while (j >= 0 && keys[j] > movingKey)
			{
				zones[j + 1] = zones[j];
				keys[j + 1] = keys[j];
				j--;
			}
			zones[j + 1] = movingZone;
			keys[j + 1] = movingKey;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Position du personnage du joueur, vector.Zero s'il n'en a pas
	protected static vector PlayerPos(int playerId)
	{
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return vector.Zero;
		return character.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Teinte d'une ligne de zone : attaque ou coupure orange, à nous vert, base blanc, ennemie rouge
	protected static string ZoneTint(SRP_FrontComponent front, int zone)
	{
		if (front.IsBaseZone(zone))
			return "w";
		if (front.GetZoneAttackState(zone) != SRP_EFrontAttack.AUCUNE || front.IsZoneCut(zone))
			return "y";
		if (front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
			return "g";
		return "r";
	}

	//------------------------------------------------------------------------------------------------
	//! Teinte d'une ligne d'attaque : annoncée orange, en cours rouge
	protected static string AttackTint(SRP_FrontComponent front, int zone)
	{
		if (front.GetZoneAttackState(zone) == SRP_EFrontAttack.ASSAUT)
			return "r";
		return "y";
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07) — ATTAQUE annoncée, assaut dans 9 min · à nous · 20/25 carrés » (+ coupure éventuelle)
	protected static string AttackLine(SRP_FrontComponent front, int zone)
	{
		string text = front.GetZoneLabel(zone) + " — " + AttackText(front, zone, front.GetZoneAttackState(zone));
		text += " · " + ZoneOwnerWord(zone) + " · " + ShareText(zone);
		if (front.IsZoneCut(zone))
			text += " · " + CutText(front, zone);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « ATTAQUE annoncée, assaut dans 9 min » ou « ATTAQUE EN COURS, vague 2 » (texte d'état construit par l'ennemi
	//! du front, trou 20 ; l'état lui-même vient du socle)
	protected static string AttackText(SRP_FrontComponent front, int zone, int state)
	{
		string head = "ATTAQUE annoncée";
		if (state == SRP_EFrontAttack.ASSAUT)
			head = "ATTAQUE EN COURS";
		SRP_FrontEnemyComponent enemyFront = SRP_FrontEnemyComponent.GetInstance();
		if (!enemyFront)
			return head;
		string status = enemyFront.GetAttackStatusText(zone);
		if (status.IsEmpty())
			return head;
		string low = status;
		low.ToLower();
		if (low.Contains("attaque"))
			return status;
		return head + ", " + status;
	}

	//------------------------------------------------------------------------------------------------
	//! PC, zone ennemie : « PC ennemi tenu », « PC ennemi saisi », « points clés 1/2 saisis » ou « sans PC ennemi »
	protected static string KeyPointsShort(SRP_FrontComponent front, int zone)
	{
		int count = front.GetKeyPointCount(zone);
		if (count == 0)
			return "sans PC ennemi";
		int taken = 0;
		for (int k = 0; k < count; k++)
		{
			if (front.IsKeyPointTaken(zone, k))
				taken++;
		}
		if (count == 1)
		{
			if (taken == 1)
				return "PC ennemi saisi";
			return "PC ennemi tenu";
		}
		return string.Format("points clés %1/%2 saisis", taken, count);
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés contestés de la zone (C8) : en capture, au combat, en reprise
	protected static void CountLive(SRP_FrontComponent front, int zone, out int capturing, out int fighting, out int retaking)
	{
		capturing = 0;
		fighting = 0;
		retaking = 0;
		SRP_FrontZone z = front.GetZone(zone);
		if (!z)
			return;
		foreach (int cell : z.m_aCells)
		{
			int live = front.GetCellLive(cell);
			if (live == SRP_EFrontLive.CAPTURE)
				capturing++;
			else if (live == SRP_EFrontLive.COMBAT)
				fighting++;
			else if (live == SRP_EFrontLive.REPRISE)
				retaking++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! E4 : « coupée de la base depuis 3 h, perdue dans 21 h » (+ « décompte arrêté » pendant le gel)
	protected static string CutText(SRP_FrontComponent front, int zone)
	{
		string text = "coupée de la base";
		SRP_FrontZone z = front.GetZone(zone);
		int now = System.GetUnixTime();
		if (z && z.m_iCutSince > 0 && now > z.m_iCutSince)
			text += " depuis " + DurationText(now - z.m_iCutSince);
		int left = front.GetZoneCutSecondsLeft(zone);
		if (left >= 0)
			text += ", perdue dans " + DurationText(left);
		if (front.IsFrozen())
			text += " (décompte arrêté : front gelé)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « · région de Saint-Philippe » (géographie seule) pour une zone ennemie de l'arrière ; "" sans Commandeur
	protected static string RegionSuffix(int zone)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return "";
		int region = commander.GetRegionOfZone(zone);
		if (region < 0)
			return "";
		string label = commander.GetRegionLabel(region);
		if (label.IsEmpty())
			return "";
		return " · " + label;
	}

	//------------------------------------------------------------------------------------------------
	//! Suffixe du Commandeur collé à une ligne : « · » ajouté s'il n'y est pas déjà
	protected static string Suffix(string text)
	{
		if (text.IsEmpty())
			return "";
		if (text.StartsWith(" "))
			return text;
		if (text.StartsWith("·"))
			return " " + text;
		return " · " + text;
	}

	//------------------------------------------------------------------------------------------------
	//! Bloc de lignes d'un autre module (Commandeur) : saut de ligne ajouté en tête s'il manque
	protected static string AsLines(string block)
	{
		if (block.IsEmpty())
			return "";
		if (block.StartsWith("\n"))
			return block;
		return "\n" + block;
	}

	//------------------------------------------------------------------------------------------------
	//! Un rapport multi-lignes en lignes L| (ou C|teinte| si tint n'est pas vide)
	protected static string InfoLines(string text, string tint)
	{
		string result = "";
		if (text.IsEmpty())
			return result;
		array<string> lines = {};
		text.Split("\n", lines, true);
		foreach (string line : lines)
		{
			if (tint.IsEmpty())
				result += "\nL|" + Esc(line);
			else
				result += "\nC|" + tint + "|" + Esc(line);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Teinte d'une ligne « ce carré » : contesté orange, à nous vert, ennemi rouge, hors jeu gris
	protected static string CellTint(SRP_FrontComponent front, int cell)
	{
		if (cell < 0 || !front.IsCellInPlay(cell))
			return "d";
		if (front.IsCellContested(cell))
			return "y";
		if (front.GetCellOwner(cell) == SRP_EFrontOwner.BLEU)
			return "g";
		return "r";
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes d'un point clé de la page Staff « Points clés » : état, puis aller et saisir
	protected static string KeyPointBlock(SRP_FrontComponent front, SRP_ZoneFall zoneFall, int locality, int keyPoint, int role, vector from)
	{
		SRP_FrontKeyPoint point = null;
		if (keyPoint >= 0)
			point = front.GetKeyPoint(keyPoint);
		if (!point)
		{
			if (role == SRP_EKeyPointRole.QG)
				return "\nC|y|QG : MANQUANT — ville à un seul point clé ; poser SRP_PointCle_QG au Workbench";
			return "\nC|y|Centre : MANQUANT — aucun point, même sur le nom de la carte";
		}
		string text = "\nC|" + SourceTint(point) + "|" + Esc(RoleCapital(role) + " : " + KeyPointState(front, zoneFall, point, from));
		text += string.Format("\nI|territoire:pc-tp:%1:%2|» Aller au point clé %3", locality, role, RoleWord(role));
		if (front.GetCellOwner(point.m_iCell) != SRP_EFrontOwner.BLEU)
			text += string.Format("\nK|r|territoire:pc-saisir:%1:%2|» Saisir le poste %3 maintenant (vraie saisie : la zone peut tomber)", locality, role, RoleWord(role));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « tenu par l'ennemi · poste : saisie possible · carré 074 042 · repère posé (…) · 240 m »
	protected static string KeyPointState(SRP_FrontComponent front, SRP_ZoneFall zoneFall, SRP_FrontKeyPoint point, vector from)
	{
		string text;
		if (front.GetCellOwner(point.m_iCell) == SRP_EFrontOwner.BLEU)
		{
			text = "saisi (carré bleu)";
		}
		else
		{
			text = "tenu par l'ennemi";
			if (zoneFall)
				text += " · poste : " + BlockWord(zoneFall.GetSeizeBlock(point.m_iIndex));
		}
		if (point.m_iCell >= 0)
			text += " · carré " + front.CellRef(point.m_iCell);
		text += " · " + SourceText(point);
		if (from != vector.Zero)
			text += " · " + DistanceText(vector.DistanceXZ(from, point.m_vPos));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Source du point clé (Q8) : repère posé au Workbench, sinon MANQUANT (centre pris sur le nom de la carte) ; le
	//! bâtisseur refuse les lignes « point » de front_retouches.txt, il n'y a pas d'autre source
	protected static string SourceText(SRP_FrontKeyPoint point)
	{
		if (point.m_iSource == SRP_EKeyPointSource.REPERE)
			return "repère posé (" + point.m_sSourceText + ")";
		return "MANQUANT — placé sur le nom de la carte";
	}

	//------------------------------------------------------------------------------------------------
	//! Blanc pour un repère posé, orange pour un point manquant (nom de la carte)
	protected static string SourceTint(SRP_FrontKeyPoint point)
	{
		if (point.m_iSource == SRP_EKeyPointSource.REPERE)
			return "w";
		return "y";
	}

	//------------------------------------------------------------------------------------------------
	//! « centre » ou « QG »
	protected static string RoleWord(int role)
	{
		if (role == SRP_EKeyPointRole.QG)
			return "QG";
		return "centre";
	}

	//------------------------------------------------------------------------------------------------
	//! « Centre » ou « QG » (début de ligne)
	protected static string RoleCapital(int role)
	{
		if (role == SRP_EKeyPointRole.QG)
			return "QG";
		return "Centre";
	}

	//------------------------------------------------------------------------------------------------
	//! Raison qui bloque la saisie (SRP_ESeizeBlock), en clair : le texte de SRP_ZoneFall.BlockText (le même que
	//! GetPostReport sur cette page), « saisie possible » quand rien ne bloque
	protected static string BlockWord(int block)
	{
		if (block == SRP_ESeizeBlock.LIBRE)
			return "saisie possible";
		string reason = SRP_ZoneFall.BlockText(block);
		if (reason.IsEmpty())
			return "état inconnu";
		return reason;
	}

	//------------------------------------------------------------------------------------------------
	//! Genre de zone (SRP_EZoneKind) en clair
	protected static string KindWord(int kind)
	{
		switch (kind)
		{
			case SRP_EZoneKind.AUCUNE: return "sans localité";
			case SRP_EZoneKind.HAMEAU: return "hameau";
			case SRP_EZoneKind.VILLAGE: return "village";
			case SRP_EZoneKind.VILLE: return "ville";
		}
		return "?";
	}

	//------------------------------------------------------------------------------------------------
	//! « 3 h 05 », « 12 min », « moins d'une minute »
	protected static string DurationText(int seconds)
	{
		if (seconds < 60)
			return "moins d'une minute";
		int minutes = seconds / 60;
		if (minutes < 60)
			return string.Format("%1 min", minutes);
		int hours = minutes / 60;
		int rest = minutes - hours * 60;
		if (rest == 0)
			return string.Format("%1 h", hours);
		return string.Format("%1 h %2", hours, SRP_Time.Pad2(rest));
	}

	//------------------------------------------------------------------------------------------------
	//! « 2026-09-25 » -> « 25/09/2026 »
	protected static string DayText(string day)
	{
		array<string> bits = {};
		day.Split("-", bits, true);
		if (bits.Count() != 3)
			return day;
		return bits[2] + "/" + bits[1] + "/" + bits[0];
	}

	//------------------------------------------------------------------------------------------------
	//! Page des copies datées où se trouve ce jour (0 s'il n'y est pas)
	protected static int SavePageOf(string day)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || day.IsEmpty())
			return 0;
		array<string> days = {};
		array<string> summaries = {};
		front.GetDailyCopies(days, summaries);
		int index = days.Find(day);
		if (index < 0)
			return 0;
		return index / SAVES_PAGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Numéro de page lu dans une clé (0 s'il est absent ou illisible)
	protected static int PageNumber(string text)
	{
		if (!SRP_Utils.IsNumeric(text))
			return 0;
		return text.ToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! Rang du premier argument d'une action dans parts : « territoire:<action>:<arg> » -> 2 ; « <action>:<arg> » -> 1
	protected static int ArgStart(string action, array<string> parts)
	{
		if (!parts)
			return 2;
		if (parts.Count() >= 2 && parts[0] == "territoire" && parts[1] == action)
			return 2;
		if (parts.Count() >= 1 && parts[0] == action)
			return 1;
		return 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Élément index de parts, "" s'il n'existe pas
	protected static string Arg(array<string> parts, int index)
	{
		if (!parts || index < 0 || index >= parts.Count())
			return "";
		return parts[index];
	}
}
