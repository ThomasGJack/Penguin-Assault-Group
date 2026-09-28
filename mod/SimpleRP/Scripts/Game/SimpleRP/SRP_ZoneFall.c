//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : CHUTE ET PERTE DES ZONES (module 3), classe serveur tenue par le socle.
//
// RÈGLES : D1 (point clé + au moins la moitié des carrés), D2 (le carré du point clé touche du bleu), D4 (saisir le
// poste de commandement ennemi, action de 60 s), D5 (aucun ennemi en état de combattre à 100 m), D6 (ville : centre
// et QG), Q2 (perte : points clés repris + plus de la moitié des carrés rouges, sans repeinte), Q4 (zone sans
// village : moitié des carrés), G9 (menace +1 / -1, jamais pour le Staff), Q9 (zone forcée = correction).
// SEULES PORTES pour changer une zone de camp (trou 5) : CaptureZone, LoseZone(zone, raison, repeindre), ForceZone.
// E4 (socle) et H2 (ennemi) passent aussi par LoseZone.
// ORDRE DES APPELS après un changement de zone (appels directs, aucun ScriptInvoker) : missions -> économie ->
// SRP_FrontEnemyComponent (qui relaie au Commandeur) -> pont (NoteFrontCell par le socle, pendant la repeinte) -> radio.
// La prime G8 et l'alerte G2 sont à SRP_MissionManagerComponent.OnZoneCaptured(code, staff) SEUL (trou 14).
// Aussi : postes de commandement (table, radio, cartes) posés aux points clés des localités garnies, mât
// tricolore sur un point pris, action de saisie SRP_SaisiePCAction, blocage répliqué sur le poste.
// Plus d'outil Staff « Point clé ICI » ni de points_cles.json : le registre des points clés est au socle (Q8).
// Chute d'une zone : ses carrés rouges sont repeints AVANT SetZoneOwner (la victoire B3, déclenchée par SetZoneOwner,
// gèle le front : une repeinte faite après serait refusée pour la dernière zone).
// APPELÉ PAR : SRP_FrontComponent (Tick 2e, OnCellsChanged 2e, OnFrontReset, ForceZone, CheckCuts), l'ennemi
// (H2 : LoseZone), SRP_TerritoryComponent (OnGarrisonPosted / OnGarrisonGone), l'action de saisie, les dégâts.
// Accès : SRP_FrontComponent.GetInstance().GetZoneFall().
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! État vivant d'un point clé côté chute (serveur) : poste, mât, saisie. Rien n'est sauvegardé (H3) : la saisie
//! est la couleur du carré, sauvegardée par le socle.
class SRP_KeyPointPost
{
	int m_iKeyPoint = -1;			// index dans le registre du socle
	string m_sLocality;				// nom de la localité
	IEntity m_CP;					// poste de commandement posé
	int m_iNextCPTry;				// heure Unix du prochain essai de pose
	bool m_bCPLogged;				// échec de pose déjà écrit pour cet épisode
	IEntity m_Flag;					// mât de nos couleurs
	int m_iNextFlagTry;				// heure Unix
	bool m_bFlagNow;				// hisser tout de suite, même vu (juste après la saisie)
	int m_iSeizerId;				// joueur qui saisit, 0 = personne
	int m_iSeizeStartMs;			// début de la saisie (GetTickCount, non sauvé)
	int m_iHitLockUntilMs;			// saisie refusée jusqu'à (GetTickCount, non sauvé)
	int m_iBlock = SRP_ESeizeBlock.LIBRE;
	IEntity m_Decor;				// décor du QG (tente), posé avec le poste, retiré hors de vue sans lui
	vector m_vSpot;					// emplacement retenu pour le poste puis le mât
	float m_fSpotYaw;				// orientation retenue (celle du repère)
	bool m_bHasSpot;				// m_vSpot est calculé
	bool m_bRespawn;				// Staff : retirer puis reposer le poste (hors de vue)
}

//------------------------------------------------------------------------------------------------
class SRP_ZoneFall
{
	protected SRP_FrontComponent m_Front;					// le socle (réglages publics, zones, carrés)
	protected ref array<ref SRP_KeyPointPost> m_aPosts = {};	// un par point clé du registre
	protected ref array<int> m_aZonesToCheck = {};			// zones à réévaluer au prochain passage
	protected bool m_bRepainting;							// repeinte en cours : OnCellsChanged n'empile rien
	protected static int s_iActiveSeizures;					// saisies en cours (NoteHit ne coûte rien sinon)
	protected bool m_bWatching;								// SeizeWatch tourne

	protected ref map<int, int> m_mCellPost = new map<int, int>();	// carré -> premier point clé posé dessus
	protected ref array<IEntity> m_aPlayers = {};			// personnages des joueurs, relevés à chaque passage
	protected string m_sStaffAuthor;						// auteur d'une saisie forcée (ForceSeize -> Seize)
	protected static bool s_bStartSeen;						// le serveur a déjà reçu un début d'action (OnSeizeStart)
	protected static bool s_bFallbackLogged;				// repli « début jamais reçu » déjà écrit au journal

	// Prefabs de secours si le prefab du mode de jeu n'a pas les attributs (GUIDS.md)
	protected static ResourceName s_sDefaultCPPrefab = "{6A5B0C0D0E0F7A03}Prefabs/Props/SRP_PosteCommandement.et";
	protected static ResourceName s_sDefaultColoursPrefab = "{6A5B0C0D0E0F7A04}Prefabs/Props/SRP_MatCouleurs.et";

	protected static const float FLAG_TOP = 5;				// hauteur du mât pour le test de vue (sommet visible)
	protected static const float DECOR_TOP = 3;				// hauteur de la tente du QG
	protected static const float DECOR_DISTANCE = 6;		// la tente est posée à 6 m du poste
	protected static const float DECOR_CLEARANCE = 4;		// avec 4 m de dégagement

	//------------------------------------------------------------------------------------------------
	//! Créée par le socle dans OnPostInit (serveur) — SRP_FrontComponent
	void SRP_ZoneFall(SRP_FrontComponent front)
	{
		m_Front = front;
		s_iActiveSeizures = 0;	// nouvelle partie dans le même processus (Workbench) : rien ne survit
	}

	//------------------------------------------------------------------------------------------------
	void ~SRP_ZoneFall()
	{
		if (m_bWatching && GetGame() && GetGame().GetCallqueue())
			GetGame().GetCallqueue().Remove(SeizeWatch);
	}

	//------------------------------------------------------------------------------------------------
	//! Front prêt : un SRP_KeyPointPost par point clé du registre, RespawnFlags ; journal FRONT (Staff) « N points clés
	//! sur M localités, K posés au Workbench » — SRP_FrontComponent.Start
	void Start()
	{
		if (!m_Front)
			return;

		m_aPosts.Clear();
		m_mCellPost.Clear();
		m_aZonesToCheck.Clear();
		int total = m_Front.GetKeyPointTotal();
		int placed = 0;
		for (int i = 0; i < total; i++)
		{
			SRP_KeyPointPost post = new SRP_KeyPointPost();
			post.m_iKeyPoint = i;
			SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(i);
			if (kp)
			{
				SRP_FrontLocality locality = m_Front.GetLocality(kp.m_iLocality);
				if (locality)
					post.m_sLocality = locality.m_sName;
				if (kp.m_iSource == SRP_EKeyPointSource.REPERE)
					placed++;
				if (kp.m_iCell >= 0 && !m_mCellPost.Contains(kp.m_iCell))
					m_mCellPost.Insert(kp.m_iCell, i);
			}
			m_aPosts.Insert(post);
		}

		RespawnFlags();
		// Ligne technique (Staff) : FRONT, pas TERRITOIRE (fil public réservé aux faits de zone)
		SRP_EnemyComponent.Journal("FRONT", string.Format("%1 points clés sur %2 localités, %3 posés au Workbench", total, m_Front.GetLocalityCount(), placed));
	}

	//------------------------------------------------------------------------------------------------
	//! Passage de 5 s (2e de la boucle unique) : UpdatePost de chaque point, puis EvaluateZone des seules zones de
	//! m_aZonesToCheck (jamais toute l'île) — SRP_FrontComponent.Tick
	void Tick(int nowUnix)
	{
		if (!m_Front)
			return;

		CollectPlayers();
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			UpdatePost(post, nowUnix);
		}

		if (m_aZonesToCheck.IsEmpty())
			return;
		// Copie : une chute ou une perte peut toucher la liste pendant le parcours
		array<int> zones = {};
		zones.Copy(m_aZonesToCheck);
		m_aZonesToCheck.Clear();
		foreach (int zone : zones)
		{
			EvaluateZone(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un lot de carrés a changé (2e appel direct après chaque lot, trou 6) : leurs zones vont dans m_aZonesToCheck,
	//! sauf pendant une repeinte et sauf pour la cause STAFF (ForceCell = correction Q9 : ni chute ni perte qui
	//! suivent ; la zone sera réévaluée au prochain changement réel). Carré d'un point clé : aussi la zone de sa
	//! localité (KeyPointZone) — SRP_FrontComponent.SetCellsOwner
	void OnCellsChanged(notnull array<int> cells, int owner, int reason)
	{
		if (m_bRepainting || !m_Front)
			return;
		// Remise à plat et retour à une copie datée : état appliqué d'un coup, jamais de chute ni de perte qui suive
		if (reason == SRP_EFrontReason.STAFF || reason == SRP_EFrontReason.REMISE || reason == SRP_EFrontReason.RESTAURATION)
			return;

		foreach (int cell : cells)
		{
			AddZoneToCheck(m_Front.GetCellZone(cell));

			// Le QG d'une ville peut être posé sur un carré d'une zone voisine : sa bascule décide la chute ou la perte
			// de la zone de la ville. m_mCellPost ne garde que le premier point du carré : les suivants ont un index
			// plus grand, on les parcourt à partir de lui.
			int firstPost;
			if (!m_mCellPost.Find(cell, firstPost))
				continue;
			for (int k = firstPost; k < m_aPosts.Count(); k++)
			{
				SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(m_aPosts[k].m_iKeyPoint);
				if (kp && kp.m_iCell == cell)
					AddZoneToCheck(KeyPointZone(kp));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à plat : saisies arrêtées, postes et mâts recalculés (ZONE : seulement ceux de la zone) —
	//! SRP_FrontComponent.NotifyReset
	void OnFrontReset(int kind, int zone)
	{
		if (!m_Front)
			return;

		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (kind == SRP_EFrontReset.ZONE && !IsPostInZone(post, zone))
				continue;
			if (post.m_iSeizerId != 0)
				StopSeize(post, SRP_ESeizeBlock.LIBRE);
			post.m_iNextCPTry = 0;
			post.m_bCPLogged = false;
			post.m_iNextFlagTry = 0;
			post.m_bFlagNow = false;
			post.m_iHitLockUntilMs = 0;
		}

		if (kind == SRP_EFrontReset.ZONE)
		{
			int at = m_aZonesToCheck.Find(zone);
			if (at >= 0)
				m_aZonesToCheck.Remove(at);
		}
		else
		{
			m_aZonesToCheck.Clear();
		}
	}

	//================================================================================================
	// LES SEULES PORTES de changement de zone
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! D1 / Q4 : la zone tombe. Rien si elle est déjà bleue, de base, ou front gelé. m_Front.SetZoneOwner(BLEU,
	//! ZONE_TOMBEE) ; repeinte en silence des carrés rouges (SetCellsOwner, ZONE_TOMBEE) ; localités pas encore prises
	//! -> ennemi.OnLocalityTaken ; menace + m_iThreatPerZoneTaken (G9) ; puis missions.OnZoneCaptured(code, false) ->
	//! économie.OnZoneCaptured -> ennemi.OnZoneCaptured(zone, false) -> SRP_FrontRadio.ZoneFell — EvaluateZone
	bool CaptureZone(int zone)
	{
		if (!m_Front || !m_Front.GetZone(zone))
			return false;
		if (m_Front.IsBaseZone(zone) || m_Front.IsFrozen())
			return false;
		if (m_Front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
			return false;

		StopSeizuresInZone(zone);
		array<string> untaken = {};
		CollectUntakenLocalities(zone, untaken);

		// Repeinte en silence AVANT le changement de propriétaire : SetZoneOwner peut déclarer la victoire (B3) et
		// geler le front, ce qui ferait refuser la repeinte de la dernière zone
		RepaintZone(zone, SRP_EFrontOwner.BLEU, SRP_EFrontReason.ZONE_TOMBEE, "");
		if (!m_Front.SetZoneOwner(zone, SRP_EFrontOwner.BLEU, SRP_EFrontReason.ZONE_TOMBEE))
		{
			SRP_EnemyComponent.Journal("ERREUR", "Chute de la zone " + m_Front.GetZoneLabel(zone) + " refusée par le front (SetZoneOwner)");
			return false;
		}

		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		foreach (string name : untaken)
		{
			if (enemy && IsLocalityTaken(name))
				enemy.OnLocalityTaken(name);
		}

		// G9 : +1 par zone prise, une seule fois (ni à la saisie d'un point clé, ni à la réussite de la mission)
		if (m_Front.m_iThreatPerZoneTaken != 0 && m_Front.CountZoneCells(zone) >= m_Front.m_iThreatMinCells)
			m_Front.AddThreat(m_Front.m_iThreatPerZoneTaken, "zone " + m_Front.GetZoneLabel(zone) + " prise");

		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
			missions.OnZoneCaptured(m_Front.GetZoneCode(zone), false);
		SRP_FrontEconomy economy = m_Front.GetEconomy();
		if (economy)
			economy.OnZoneCaptured(zone);
		if (enemy)
			enemy.OnZoneCaptured(zone, false);
		SRP_FrontRadio.ZoneFell(zone, SRP_EFrontOwner.BLEU, m_Front.GetThreat(), SRP_EFrontReason.ZONE_TOMBEE);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Q2, E4, H2 : la zone repasse rouge. Rien si déjà rouge, protégée (B2, B4) ou gel. m_Front.SetZoneOwner(ROUGE,
	//! reason) ; si repaint, ses carrés bleus passent rouges en silence (ISOLEMENT, OFFENSIVE) ; menace
	//! - m_iThreatPerZoneLost (G9) ; économie.OnZoneLost -> ennemi.OnZoneLost(zone, reason, false) -> radio ZoneFell —
	//! EvaluateZone (ZONE_PERDUE, sans repeinte), socle CheckCuts (ISOLEMENT), ennemi RunOffensive (OFFENSIVE)
	bool LoseZone(int zone, int reason, bool repaint)
	{
		if (!m_Front || !m_Front.GetZone(zone))
			return false;
		if (m_Front.IsBaseZone(zone) || m_Front.IsZoneProtected(zone) || m_Front.IsFrozen())
			return false;
		if (m_Front.GetZoneOwner(zone) == SRP_EFrontOwner.ROUGE)
			return false;

		StopSeizuresInZone(zone);
		if (!m_Front.SetZoneOwner(zone, SRP_EFrontOwner.ROUGE, reason))
		{
			SRP_EnemyComponent.Journal("ERREUR", "Perte de la zone " + m_Front.GetZoneLabel(zone) + " refusée par le front (SetZoneOwner)");
			return false;
		}
		if (repaint)
			RepaintZone(zone, SRP_EFrontOwner.ROUGE, reason, "");

		// G9 : -1 par zone perdue, une seule fois
		if (m_Front.m_iThreatPerZoneLost != 0 && m_Front.CountZoneCells(zone) >= m_Front.m_iThreatMinCells)
			m_Front.AddThreat(-m_Front.m_iThreatPerZoneLost, "zone " + m_Front.GetZoneLabel(zone) + " perdue");

		SRP_FrontEconomy economy = m_Front.GetEconomy();
		if (economy)
			economy.OnZoneLost(zone);
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.OnZoneLost(zone, reason, false);
		SRP_FrontRadio.ZoneFell(zone, SRP_EFrontOwner.ROUGE, m_Front.GetThreat(), reason);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! J1 / Q9 : correction du Staff. Zone entière repeinte à owner (STAFF), ni prime, ni menace, ni radio ;
	//! missions.OnZoneCaptured(code, true) si elle passe bleue (mission de la zone ANNULEE, sans récompense) ;
	//! économie ; ennemi.OnZoneCaptured(zone, true) ou OnZoneLost(zone, STAFF, true) (le Commandeur ne réagit pas,
	//! Q9) ; AUCUNE ligne de journal ici : la seule ligne [STAFF] est écrite par SRP_FrontScreens.RunStaff ; rend le
	//! compte rendu — SRP_FrontComponent.ForceZone
	string ForceZone(int zone, int owner, string author)
	{
		if (!m_Front || !m_Front.GetZone(zone))
			return "Zone inconnue.";
		if (m_Front.IsBaseZone(zone))
			return "La zone de la base ne change jamais de camp.";
		if (owner != SRP_EFrontOwner.BLEU && owner != SRP_EFrontOwner.ROUGE)
			return "Camp inconnu.";

		string label = m_Front.GetZoneLabel(zone);
		bool changed = m_Front.GetZoneOwner(zone) != owner;
		StopSeizuresInZone(zone);
		array<string> untaken = {};
		if (owner == SRP_EFrontOwner.BLEU)
			CollectUntakenLocalities(zone, untaken);

		if (changed && !m_Front.SetZoneOwner(zone, owner, SRP_EFrontReason.STAFF))
			return "Changement refusé par le front : " + label + ".";
		int painted = RepaintZone(zone, owner, SRP_EFrontReason.STAFF, author);

		// Correction du Staff (Q9) : le bot met la zone à jour sans mouvement « prise / perdue » ni photo. Noté juste
		// après la repeinte (le pont a déjà reçu ses carrés), et seulement si quelque chose a changé.
		string code = m_Front.GetZoneCode(zone);
		if ((changed || painted > 0) && !code.IsEmpty())
			SRP_BridgeComponent.NoteStaffZone(code);

		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (owner == SRP_EFrontOwner.BLEU)
		{
			// Localités devenues à nous : leurs défenseurs se replient (état), sans annonce
			foreach (string name : untaken)
			{
				if (enemy && IsLocalityTaken(name))
					enemy.OnLocalityTaken(name);
			}
		}

		if (!changed)
		{
			if (painted == 0)
				return string.Format("Zone %1 déjà %2 : rien à changer.", label, OwnerWord(owner));
			return string.Format("Zone %1 déjà %2 : %3 carré(s) repeint(s) (correction : ni prime, ni menace, ni radio).", label, OwnerWord(owner), painted);
		}

		SRP_FrontEconomy economy = m_Front.GetEconomy();
		if (owner == SRP_EFrontOwner.BLEU)
		{
			SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
			if (missions)
				missions.OnZoneCaptured(m_Front.GetZoneCode(zone), true);
			if (economy)
				economy.OnZoneCaptured(zone);
			if (enemy)
				enemy.OnZoneCaptured(zone, true);
			return string.Format("Zone %1 passée à nous par le Staff : %2 carré(s) repeint(s) (correction : ni prime, ni menace, ni radio).", label, painted);
		}

		if (economy)
			economy.OnZoneLost(zone);
		if (enemy)
			enemy.OnZoneLost(zone, SRP_EFrontReason.STAFF, true);
		return string.Format("Zone %1 rendue à l'ennemi par le Staff : %2 carré(s) repeint(s) (correction : ni menace, ni radio).", label, painted);
	}

	//================================================================================================
	// Services aux autres modules
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Carré d'un point clé d'une localité encore hostile : il ne se prend QUE par la saisie du poste (D4), jamais par
	//! présence — SRP_FrontCapture.EvaluateCells (trou 12)
	bool IsCellSeizeOnly(int cell)
	{
		if (!m_Front || cell < 0)
			return false;
		int index;
		if (!m_mCellPost.Find(cell, index))
			return false;
		// Zone rouge : le point se saisit ; zone bleue : un carré repris par l'ennemi se reprend comme les autres
		return m_Front.GetZoneOwner(m_Front.GetCellZone(cell)) == SRP_EFrontOwner.ROUGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les points clés de la localité sont bleus (localité prise) — SRP_TerritoryComponent (aucune garnison),
	//! ennemi, missions
	bool IsLocalityTaken(string locality)
	{
		if (!m_Front)
			return false;
		int index = m_Front.FindLocality(locality);
		if (index < 0)
			return false;
		SRP_FrontLocality place = m_Front.GetLocality(index);
		if (!place)
			return false;

		int count = 0;
		if (place.m_iCentre >= 0)
		{
			count++;
			if (!IsKeyPointBlue(place.m_iCentre))
				return false;
		}
		if (place.m_iQg >= 0)
		{
			count++;
			if (!IsKeyPointBlue(place.m_iQg))
				return false;
		}
		if (count == 0)
			return m_Front.GetZoneOwner(place.m_iZone) == SRP_EFrontOwner.BLEU;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte de chute de la zone : « points clés 1/2 saisis · 11 carrés bleus sur 25, 13 requis » — PC, Staff, missions
	string GetZoneFallText(int zone)
	{
		if (!m_Front || !m_Front.GetZone(zone))
			return "";
		if (m_Front.IsBaseZone(zone))
			return "zone de la base";

		int total = m_Front.CountZoneCells(zone);
		int blue = m_Front.CountZoneBlue(zone);
		int taken;
		int keys = CountKeyPoints(zone, taken);
		string keyText = "";

		if (m_Front.GetZoneOwner(zone) == SRP_EFrontOwner.ROUGE)
		{
			if (keys > 0)
				keyText = string.Format("points clés %1/%2 saisis · ", taken, keys);
			else
				keyText = "sans point clé · ";
			return keyText + string.Format("%1 carrés bleus sur %2, %3 requis", blue, total, NeededBlue(total, keys));
		}

		if (m_Front.IsZoneProtected(zone))
			return string.Format("imprenable (touche la base) · %1 carrés bleus sur %2", blue, total);
		int red = total - blue;
		int maxRed = total * m_Front.m_iZoneLossPercent / 100;
		if (keys > 0)
		{
			keyText = string.Format("points clés %1/%2 tenus · ", taken, keys);
			return keyText + string.Format("%1 carrés rouges sur %2, perdue au-delà de %3 si l'ennemi reprend ses points clés", red, total, maxRed);
		}
		return string.Format("sans point clé · %1 carrés rouges sur %2, perdue au-delà de %3", red, total, maxRed);
	}

	//------------------------------------------------------------------------------------------------
	//! Raison qui bloque la saisie du point clé (SRP_ESeizeBlock) — Staff (page des points clés)
	int GetSeizeBlock(int keyPoint)
	{
		SRP_KeyPointPost post = FindPostByKeyPoint(keyPoint);
		if (!post)
			return SRP_ESeizeBlock.LIBRE;
		if (post.m_iSeizerId != 0 && post.m_iBlock == SRP_ESeizeBlock.LIBRE)
			return SRP_ESeizeBlock.AUTRE_SOLDAT;
		return post.m_iBlock;
	}

	//------------------------------------------------------------------------------------------------
	//! État de chaque poste pour la page Staff « Points clés » (en place, pas de garnison, saisi, bloqué…) ; rend
	//! le nombre de lignes — SRP_FrontScreens
	int GetPostReport(notnull array<string> lines)
	{
		lines.Clear();
		if (!m_Front)
			return 0;

		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
			if (!kp)
				continue;

			string state;
			int cellOwner = m_Front.GetCellOwner(kp.m_iCell);
			int zone = m_Front.GetCellZone(kp.m_iCell);
			bool hasCP = post.m_CP && !post.m_CP.IsDeleted();
			if (cellOwner == SRP_EFrontOwner.BLEU)
			{
				state = "saisi";
				if (post.m_Flag && !post.m_Flag.IsDeleted())
					state = "saisi, nos couleurs hissées";
				if (hasCP)
					state = state + " (ancien poste encore là, retiré dès que personne ne le voit)";
			}
			else if (cellOwner != SRP_EFrontOwner.ROUGE)
			{
				state = "hors jeu (carré en mer ou hors grille)";
			}
			else if (m_Front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE)
			{
				state = "carré repris par l'ennemi dans une zone à nous : rien à saisir";
			}
			else if (!enemy || !enemy.IsGarrisonPosted(post.m_sLocality))
			{
				state = "pas de garnison : aucun poste, rien à saisir pour l'instant";
			}
			else if (!hasCP)
			{
				state = "poste en attente de pose (hors de la vue des joueurs)";
				if (post.m_bCPLogged)
					state = "poste NON posé (voir le journal ENNEMI), nouvel essai en cours";
			}
			else if (post.m_iSeizerId != 0)
			{
				state = "poste en place, saisie en cours par " + GetGame().GetPlayerManager().GetPlayerName(post.m_iSeizerId);
			}
			else if (post.m_iBlock != SRP_ESeizeBlock.LIBRE)
			{
				state = "poste en place, bloqué : " + BlockText(post.m_iBlock);
			}
			else
			{
				state = "poste en place, saisie possible";
			}
			if (post.m_bRespawn)
				state = state + " · repose demandée";

			lines.Insert(string.Format("%1 (%2) — %3", post.m_sLocality, RoleWord(kp.m_iRole), state));
		}
		return lines.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! La garnison de la localité est posée : réveille la pose de son poste — SRP_TerritoryComponent (pose, même vide F12)
	void OnGarrisonPosted(string locality)
	{
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_sLocality != locality)
				continue;
			post.m_iNextCPTry = 0;
			post.m_bCPLogged = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La garnison de la localité est partie : le poste sera retiré hors de vue — SRP_TerritoryComponent
	void OnGarrisonGone(string locality)
	{
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_sLocality != locality)
				continue;
			post.m_iNextCPTry = 0;
			post.m_bCPLogged = false;
		}
	}

	//================================================================================================
	// Saisie D4 (serveur), appelée par SRP_SaisiePCAction et les dégâts
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Début de la barre d'action : si personne ne saisit et LIBRE, m_iSeizerId = joueur, s_iActiveSeizures++,
	//! SetState, CallLater(SeizeWatch, m_iSeizeCheckMs, true) à la première saisie — SRP_SaisiePCAction.OnActionStart
	void OnSeizeStart(IEntity cp, IEntity user)
	{
		s_bStartSeen = true;
		if (!m_Front)
			return;
		SRP_KeyPointPost post = FindPostByCP(cp);
		if (!post)
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(user);
		if (playerId <= 0 || post.m_iSeizerId == playerId)
			return;
		if (post.m_iSeizerId != 0)
			return;	// un autre soldat saisit : l'action du joueur est annulée par l'état répliqué

		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return;
		if (m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.ROUGE || m_Front.GetZoneOwner(m_Front.GetCellZone(kp.m_iCell)) != SRP_EFrontOwner.ROUGE)
		{
			SRP_Utils.NotifyPlayer(playerId, "Ce point clé est déjà saisi.");
			return;
		}
		UpdateBlock(post);
		if (post.m_iBlock != SRP_ESeizeBlock.LIBRE)
			return;	// l'état répliqué arrête l'action chez le joueur, avec la raison
		SRP_FrontCapture capture = m_Front.GetCapture();
		if (capture && !capture.IsPlayerCounted(playerId))
		{
			SRP_Utils.NotifyPlayer(playerId, "Saisie impossible : tu ne comptes pas pour la prise en ce moment (téléportation ou Game Master récents).");
			return;
		}

		post.m_iSeizerId = playerId;
		post.m_iSeizeStartMs = System.GetTickCount();
		s_iActiveSeizures++;
		SRP_PosteCommandementComponent comp = PostComponent(post);
		if (comp)
			comp.SetState(SRP_ESeizeBlock.LIBRE, playerId);
		if (!m_bWatching)
		{
			m_bWatching = true;
			int period = m_Front.m_iSeizeCheckMs;
			if (period < 200)
				period = 200;
			GetGame().GetCallqueue().CallLater(SeizeWatch, period, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Action annulée : si c'est le saisisseur, StopSeize(LIBRE) — SRP_SaisiePCAction.OnActionCanceled
	void OnSeizeCancel(IEntity cp, IEntity user)
	{
		SRP_KeyPointPost post = FindPostByCP(cp);
		if (!post)
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(user);
		if (playerId > 0 && post.m_iSeizerId == playerId)
			StopSeize(post, SRP_ESeizeBlock.LIBRE);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de la barre : tout revérifié au serveur (saisisseur, durée x m_fSeizeMinRatio, zone rouge, carré rouge au
	//! contact, non gelé, garnison posée et installée, aucun ennemi à m_fKeyPointClearRadius, pas touché,
	//! capture.IsPlayerCounted) puis Seize ; rend "" ou la raison du refus — SRP_SaisiePCAction.PerformAction
	string TrySeize(IEntity cp, IEntity user)
	{
		if (!m_Front)
			return "";
		SRP_KeyPointPost post = FindPostByCP(cp);
		if (!post)
			return "Ce poste de commandement n'est plus en service.";
		int playerId = SRP_Utils.GetPlayerIdFromEntity(user);
		if (playerId <= 0)
			return "";
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return "Point clé inconnu.";

		if (post.m_iSeizerId != playerId)
		{
			// Le serveur reçoit d'habitude le début de l'action (OnActionStart). S'il ne l'a JAMAIS reçu depuis le
			// démarrage, le moteur ne l'envoie pas : repli sur les seules conditions de fin, écrit une fois au journal
			if (post.m_iSeizerId != 0 || s_bStartSeen)
				return "Saisie interrompue : recommence depuis le début.";
			if (!s_bFallbackLogged)
			{
				s_bFallbackLogged = true;
				SRP_EnemyComponent.Journal("ENNEMI", "Saisie de poste de commandement : le début de l'action n'est jamais arrivé au serveur ; saisies validées sur les seules conditions de fin (durée non contrôlée)");
			}
		}
		else
		{
			float needMs = ActionSeconds(cp) * 1000 * m_Front.m_fSeizeMinRatio;
			float elapsedMs = System.GetTickCount() - post.m_iSeizeStartMs;
			if (elapsedMs < needMs)
			{
				StopSeize(post, SRP_ESeizeBlock.LIBRE);
				return "Saisie interrompue : recommence depuis le début.";
			}
		}

		string refusal = SeizeRefusal(post, kp, user, playerId);
		if (!refusal.IsEmpty())
		{
			if (post.m_iSeizerId == playerId)
				StopSeize(post, ComputeBlock(post, kp));
			return refusal;
		}

		Seize(post, playerId, false);
		if (m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.BLEU)
			return "Saisie refusée par le front : recommence.";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Coût nul hors saisie (s_iActiveSeizures == 0) ; dégât MELEE, KINETIC, FRAGMENTATION, EXPLOSIVE, INCENDIARY ou
	//! PROCESSED_FRAGMENTATION sur le saisisseur -> verrou m_iHitLockSeconds, StopSeize(TOUCHE), message — dégâts moddés
	static void NoteHit(IEntity character, EDamageType type, float value)
	{
		if (s_iActiveSeizures <= 0)
			return;
		if (value <= 0 || !character)
			return;
		if (type != EDamageType.MELEE && type != EDamageType.KINETIC && type != EDamageType.FRAGMENTATION && type != EDamageType.EXPLOSIVE && type != EDamageType.INCENDIARY && type != EDamageType.PROCESSED_FRAGMENTATION)
			return;

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		SRP_ZoneFall fall = front.GetZoneFall();
		if (!fall)
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(character);
		if (playerId <= 0)
			return;
		SRP_KeyPointPost post = fall.FindPostBySeizer(playerId);
		if (!post)
			return;

		post.m_iHitLockUntilMs = System.GetTickCount() + front.m_iHitLockSeconds * 1000;
		fall.StopSeize(post, SRP_ESeizeBlock.TOUCHE);
		SRP_Utils.NotifyPlayer(playerId, "Saisie interrompue : tu as été touché.");
	}

	//================================================================================================
	// Staff
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Saisie forcée du point clé (rôle SRP_EKeyPointRole) de la localité, comme une vraie (Q9 : la chute de zone qui
	//! suit reste une vraie chute) — SRP_FrontScreens
	string ForceSeize(string locality, int role, string author)
	{
		if (!m_Front)
			return "";
		int index = m_Front.FindLocality(locality);
		if (index < 0)
			return "Localité inconnue : " + locality + ".";
		SRP_FrontLocality place = m_Front.GetLocality(index);
		if (!place)
			return "Localité inconnue : " + locality + ".";

		int keyPoint = place.m_iCentre;
		if (role == SRP_EKeyPointRole.QG)
			keyPoint = place.m_iQg;
		if (keyPoint < 0)
			return string.Format("%1 n'a pas de point clé %2.", locality, RoleWord(role));
		SRP_KeyPointPost post = FindPostByKeyPoint(keyPoint);
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(keyPoint);
		if (!post || !kp)
			return "Point clé introuvable.";
		if (m_Front.IsFrozen())
			return "Front gelé : dégèle-le avant de forcer une saisie.";

		int cellOwner = m_Front.GetCellOwner(kp.m_iCell);
		if (cellOwner == SRP_EFrontOwner.BLEU)
			return string.Format("Le point clé %1 de %2 est déjà saisi.", RoleWord(role), locality);
		if (cellOwner != SRP_EFrontOwner.ROUGE)
			return "Ce point clé est hors jeu (carré en mer ou hors grille).";
		// Zone pour laquelle le point compte (compte rendu) ; ni elle ni celle de son carré ne peuvent être la base
		int zone = KeyPointZone(kp);
		if (m_Front.IsBaseZone(zone) || m_Front.IsBaseZone(m_Front.GetCellZone(kp.m_iCell)))
			return "Ce point clé est dans la zone de la base.";

		m_sStaffAuthor = author;
		Seize(post, 0, true);
		m_sStaffAuthor = "";
		if (m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.BLEU)
			return "Saisie refusée par le front (carré protégé ou gelé).";
		return string.Format("Poste de commandement de %1 (%2) saisi par le Staff, comme une vraie saisie. Zone %3 : %4", locality, RoleWord(role), m_Front.GetZoneLabel(zone), GetZoneFallText(zone));
	}

	//------------------------------------------------------------------------------------------------
	//! Retire et repose le poste de la localité (hors de vue) — SRP_FrontScreens
	string RespawnCP(string locality)
	{
		int found = 0;
		int present = 0;
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_sLocality != locality)
				continue;
			found++;
			if (post.m_iSeizerId != 0)
				StopSeize(post, SRP_ESeizeBlock.LIBRE);
			if (post.m_CP && !post.m_CP.IsDeleted())
			{
				post.m_bRespawn = true;
				present++;
			}
			post.m_bHasSpot = false;
			post.m_iNextCPTry = 0;
			post.m_bCPLogged = false;
		}
		if (found == 0)
			return "Aucun point clé pour " + locality + ".";
		if (present == 0)
			return string.Format("Aucun poste en place à %1 : pose relancée (il faut une zone rouge, un carré rouge et une garnison posée).", locality);
		return string.Format("%1 poste(s) de %2 : retrait puis nouvelle pose dès que plus aucun joueur ne le voit (et aucun à moins de %3 m).", present, locality, Math.Round(m_Front.m_fCPDeleteMinPlayerDistance));
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! D1, Q2, Q4 pour une zone : chute (tous les points clés bleus, m_bAllLocalitiesRequired, part bleue >=
	//! m_iZoneFallPercent ou m_iNoLocalityFallPercent) ou perte (non protégée, points clés repris selon
	//! m_bLossNeedsAllKeyPoints, part rouge > m_iZoneLossPercent) ; radio HalfReached une fois — Tick
	protected void EvaluateZone(int zone)
	{
		if (!m_Front || m_Front.IsFrozen())
			return;
		if (!m_Front.GetZone(zone) || m_Front.IsBaseZone(zone))
			return;
		int total = m_Front.CountZoneCells(zone);
		if (total <= 0)
			return;
		int blue = m_Front.CountZoneBlue(zone);
		int taken;
		int keys = CountKeyPoints(zone, taken);

		if (m_Front.GetZoneOwner(zone) == SRP_EFrontOwner.ROUGE)
		{
			int percent = m_Front.m_iZoneFallPercent;
			if (keys == 0)
				percent = m_Front.m_iNoLocalityFallPercent;	// Q4 : zone sans village
			if (blue * 100 < total * percent)
				return;
			if (taken >= keys)
			{
				CaptureZone(zone);
				return;
			}
			// La moitié est à nous, reste le PC ennemi (la radio ne le dit qu'une fois par zone)
			if (m_Front.m_bRadioHalf)
				SRP_FrontRadio.HalfReached(zone);
			return;
		}

		if (m_Front.IsZoneProtected(zone))
			return;	// B2, B4
		bool keysLost;
		if (keys == 0)
			keysLost = true;
		else if (m_Front.m_bLossNeedsAllKeyPoints)
			keysLost = taken == 0;
		else
			keysLost = taken < keys;
		int red = total - blue;
		if (keysLost && red * 100 > total * m_Front.m_iZoneLossPercent)
			LoseZone(zone, SRP_EFrontReason.ZONE_PERDUE, false);	// Q2 : sans repeinte
	}

	//------------------------------------------------------------------------------------------------
	//! Poste, mât et blocage d'un point — Tick
	protected void UpdatePost(SRP_KeyPointPost post, int nowUnix)
	{
		if (!post)
			return;
		UpdateCP(post, nowUnix);
		UpdateFlag(post, nowUnix);
		UpdateBlock(post);
	}

	//------------------------------------------------------------------------------------------------
	//! Poste voulu si zone rouge, carré rouge et ennemi.IsGarrisonPosted(localité) ; pose hors de vue (orientation du
	//! repère, SetLabel « Montignac — QG », décor du QG), retrait hors de vue sinon ; gelé : il reste — UpdatePost
	protected void UpdateCP(SRP_KeyPointPost post, int nowUnix)
	{
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return;
		if (post.m_CP && post.m_CP.IsDeleted())
			post.m_CP = null;
		if (post.m_Decor && post.m_Decor.IsDeleted())
			post.m_Decor = null;
		if (m_Front.IsFrozen())
			return;	// J3 : le poste reste où il est

		bool wanted = IsCPWanted(post, kp);

		// Repose demandée par le Staff : d'abord le retrait, hors de vue
		if (post.m_bRespawn)
		{
			if (!post.m_CP)
			{
				post.m_bRespawn = false;
			}
			else if (post.m_iSeizerId == 0 && DeleteIfUnseen(post.m_CP, 0))
			{
				post.m_CP = null;
				post.m_bRespawn = false;
			}
		}

		if (post.m_CP && !wanted && post.m_iSeizerId == 0)
		{
			if (DeleteIfUnseen(post.m_CP, 0))
				post.m_CP = null;
		}
		// Le décor du QG ne reste qu'avec son poste
		if (post.m_Decor && !post.m_CP)
		{
			if (DeleteIfUnseen(post.m_Decor, DECOR_TOP))
				post.m_Decor = null;
		}

		if (!wanted)
		{
			post.m_bCPLogged = false;
			return;
		}
		if (post.m_CP || post.m_bRespawn)
			return;
		if (post.m_Flag && !post.m_Flag.IsDeleted())
			return;	// le mât occupe encore la place : il part d'abord (hors de vue)
		if (nowUnix < post.m_iNextCPTry)
			return;

		vector spot;
		float yaw;
		if (!GetSpot(post, kp, spot, yaw))
		{
			FailCP(post, "aucun emplacement au sec près du point clé", nowUnix);
			return;
		}
		// Hors de vue et personne à moins de m_fCPSpawnPlayerMin : sinon nouvel essai au passage suivant
		if (SRP_Placement.NearestPlayer(spot, m_aPlayers) < m_Front.m_fCPSpawnPlayerMin)
			return;
		if (SRP_Placement.IsSeenByAnyPlayer(spot, m_aPlayers))
			return;

		ResourceName prefab = m_Front.m_sCPPrefab;
		if (prefab.IsEmpty())
			prefab = s_sDefaultCPPrefab;
		IEntity cp = SpawnAt(prefab, spot, yaw);
		if (!cp)
		{
			FailCP(post, "prefab introuvable ou pose refusée (" + prefab + ")", nowUnix);
			return;
		}
		post.m_CP = cp;
		post.m_bCPLogged = false;
		post.m_iNextCPTry = 0;

		SRP_PosteCommandementComponent comp = PostComponent(post);
		if (comp)
		{
			comp.SetLabel(LabelOf(post, kp));
			comp.SetState(post.m_iBlock, post.m_iSeizerId);
		}
		else
		{
			SRP_EnemyComponent.Journal("ENNEMI", "Poste de commandement de " + post.m_sLocality + " posé sans SRP_PosteCommandementComponent : la saisie n'affichera pas ses raisons");
		}
		if (kp.m_iRole == SRP_EKeyPointRole.QG)
			PlaceDecor(post, spot, yaw);
	}

	//------------------------------------------------------------------------------------------------
	//! Mât voulu si m_bRaiseColours et carré bleu ; posé tout de suite après une saisie (m_bFlagNow) ou hors de vue ;
	//! retiré hors de vue — UpdatePost
	protected void UpdateFlag(SRP_KeyPointPost post, int nowUnix)
	{
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return;
		if (post.m_Flag && post.m_Flag.IsDeleted())
			post.m_Flag = null;

		bool wanted = m_Front.m_bRaiseColours && m_Front.GetCellOwner(kp.m_iCell) == SRP_EFrontOwner.BLEU;
		if (post.m_Flag)
		{
			if (wanted)
			{
				post.m_bFlagNow = false;
				return;
			}
			if (DeleteIfUnseen(post.m_Flag, FLAG_TOP))
				post.m_Flag = null;
			return;
		}
		if (!wanted)
		{
			post.m_bFlagNow = false;
			return;
		}
		if (post.m_CP && !post.m_CP.IsDeleted())
			return;	// le poste occupe encore la place
		if (!post.m_bFlagNow && nowUnix < post.m_iNextFlagTry)
			return;

		vector spot;
		float yaw;
		if (!GetSpot(post, kp, spot, yaw))
		{
			post.m_bFlagNow = false;
			post.m_iNextFlagTry = nowUnix + m_Front.m_iCPRetrySeconds;
			return;
		}
		if (!post.m_bFlagNow && IsSeen(spot, FLAG_TOP))
			return;

		ResourceName prefab = m_Front.m_sColoursPrefab;
		if (prefab.IsEmpty())
			prefab = s_sDefaultColoursPrefab;
		post.m_bFlagNow = false;
		post.m_Flag = SpawnAt(prefab, spot, yaw);
		if (!post.m_Flag)
			post.m_iNextFlagTry = nowUnix + m_Front.m_iCPRetrySeconds;
	}

	//------------------------------------------------------------------------------------------------
	//! Raison de blocage par priorité : GELE, GARNISON_EN_ROUTE, PAS_AU_CONTACT, ENNEMIS, TOUCHE, LIBRE ; écrite sur le
	//! poste (SetState) seulement si elle change — UpdatePost
	protected void UpdateBlock(SRP_KeyPointPost post)
	{
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return;
		int block = ComputeBlock(post, kp);

		// Une saisie en cours s'arrête dès qu'une raison la bloque
		if (post.m_iSeizerId != 0 && block != SRP_ESeizeBlock.LIBRE)
		{
			int seizer = post.m_iSeizerId;
			StopSeize(post, block);
			SRP_Utils.NotifyPlayer(seizer, "Saisie interrompue : " + BlockText(block) + ".");
			return;
		}
		if (block == post.m_iBlock)
			return;
		post.m_iBlock = block;
		SRP_PosteCommandementComponent comp = PostComponent(post);
		if (comp)
			comp.SetState(block, post.m_iSeizerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Au démarrage, un mât par point clé dont le carré est bleu — Start
	protected void RespawnFlags()
	{
		if (!m_Front.m_bRaiseColours)
			return;
		ResourceName prefab = m_Front.m_sColoursPrefab;
		if (prefab.IsEmpty())
			prefab = s_sDefaultColoursPrefab;

		int raised = 0;
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_Flag && !post.m_Flag.IsDeleted())
				continue;
			SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
			if (!kp || m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.BLEU)
				continue;
			vector spot;
			float yaw;
			if (!GetSpot(post, kp, spot, yaw))
				continue;
			post.m_Flag = SpawnAt(prefab, spot, yaw);
			if (post.m_Flag)
				raised++;
		}
		if (raised > 0)
			SRP_EnemyComponent.Journal("FRONT", string.Format("%1 mât(s) à nos couleurs reposé(s) sur les points clés saisis", raised));
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les m_iSeizeCheckMs pendant une saisie : saisisseur absent, mort, inconscient, trop loin
	//! (m_fSeizeMaxDistance) ou ennemi revenu -> StopSeize ; plus aucune saisie : Remove(SeizeWatch) — CallLater
	protected void SeizeWatch()
	{
		int active = 0;
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_iSeizerId == 0)
				continue;

			int seizer = post.m_iSeizerId;
			string why = "";
			int block = SRP_ESeizeBlock.LIBRE;
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(seizer);
			SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
			if (!post.m_CP || post.m_CP.IsDeleted() || !kp)
			{
				why = "le poste n'est plus là";
			}
			else if (!character || SRP_Utils.IsDead(character))
			{
				why = "tu es hors de combat";
			}
			else if (IsUnconscious(character))
			{
				why = "tu es inconscient";
			}
			else if (vector.Distance(character.GetOrigin(), post.m_CP.GetOrigin()) > m_Front.m_fSeizeMaxDistance)
			{
				why = "tu t'es éloigné du poste";
			}
			else
			{
				block = ComputeBlock(post, kp);
				if (block != SRP_ESeizeBlock.LIBRE)
					why = BlockText(block);
			}

			if (why.IsEmpty())
			{
				active++;
				continue;
			}
			StopSeize(post, block);
			SRP_Utils.NotifyPlayer(seizer, "Saisie interrompue : " + why + ".");
		}

		// Le compteur suit le nombre réel de saisies (une seule chute par serveur) : il se recale à chaque contrôle
		s_iActiveSeizures = active;
		if (active == 0)
		{
			m_bWatching = false;
			GetGame().GetCallqueue().Remove(SeizeWatch);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Arrête la saisie du point (s_iActiveSeizures--, SetState(block, 0)) — OnSeizeCancel, SeizeWatch, NoteHit, Seize
	protected void StopSeize(SRP_KeyPointPost post, int block)
	{
		if (!post)
			return;
		if (post.m_iSeizerId != 0)
		{
			s_iActiveSeizures--;
			if (s_iActiveSeizures < 0)
				s_iActiveSeizures = 0;
		}
		post.m_iSeizerId = 0;
		post.m_iSeizeStartMs = 0;
		post.m_iBlock = block;
		SRP_PosteCommandementComponent comp = PostComponent(post);
		if (comp)
			comp.SetState(block, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Saisie réussie : ennemi.OnKeyPointSeized(zone, point, joueur) AVANT le changement de camp (renseignement du
	//! Commandeur, trou 14), puis m_Front.SetCellOwner(carré, BLEU, SAISIE, nom) ; poste supprimé, m_bFlagNow ; radio
	//! SRP_FrontRadio.KeyPointSeized ; localité entièrement prise -> ennemi.OnLocalityTaken ; zone à réévaluer
	protected void Seize(SRP_KeyPointPost post, int playerId, bool staff)
	{
		if (!post || !m_Front)
			return;
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return;
		// Zone pour laquelle le point compte (celle de sa localité) : le QG d'une ville peut être sur un carré voisin
		int zone = KeyPointZone(kp);
		int cellZone = m_Front.GetCellZone(kp.m_iCell);

		// Auteur : le soldat ; saisie forcée : le Staff au journal, « l'état-major » à la radio
		string author = "";
		string radioAuthor = "";
		if (playerId > 0)
			author = GetGame().GetPlayerManager().GetPlayerName(playerId);
		else if (staff)
			author = m_sStaffAuthor;
		if (playerId > 0)
			radioAuthor = author;
		if (radioAuthor.IsEmpty())
			radioAuthor = "l'état-major";
		if (author.IsEmpty())
			author = radioAuthor;

		if (post.m_iSeizerId != 0)
			StopSeize(post, SRP_ESeizeBlock.LIBRE);

		// Renseignement du Commandeur AVANT le changement de camp (la région de la zone est encore ennemie)
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.OnKeyPointSeized(zone, post.m_iKeyPoint, playerId);

		if (!m_Front.SetCellOwner(kp.m_iCell, SRP_EFrontOwner.BLEU, SRP_EFrontReason.SAISIE, author))
		{
			SRP_EnemyComponent.Journal("ERREUR", "Saisie du poste de " + post.m_sLocality + " refusée par le front (carré " + m_Front.CellRef(kp.m_iCell) + ")");
			return;
		}

		// Le poste disparaît sous les yeux du soldat (voulu), nos couleurs sont hissées aussitôt
		if (post.m_CP && !post.m_CP.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(post.m_CP);
		post.m_CP = null;
		post.m_bRespawn = false;
		post.m_bFlagNow = true;
		UpdateFlag(post, System.GetUnixTime());

		int taken;
		int keys = CountKeyPoints(zone, taken);
		SRP_FrontRadio.KeyPointSeized(zone, kp.m_iRole, radioAuthor, m_Front.CountZoneBlue(zone), NeededBlue(m_Front.CountZoneCells(zone), keys));

		// Ce point était rouge : si la localité est prise maintenant, elle vient de l'être
		if (enemy && !post.m_sLocality.IsEmpty() && IsLocalityTaken(post.m_sLocality))
			enemy.OnLocalityTaken(post.m_sLocality);

		AddZoneToCheck(zone);
		AddZoneToCheck(cellZone);
	}

	//------------------------------------------------------------------------------------------------
	//! Le point dont le poste est cette entité, null sinon — OnSeizeStart, OnSeizeCancel, TrySeize
	protected SRP_KeyPointPost FindPostByCP(IEntity cp)
	{
		if (!cp)
			return null;
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_CP == cp)
				return post;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Le point que ce joueur est en train de saisir, null sinon — NoteHit
	protected SRP_KeyPointPost FindPostBySeizer(int playerId)
	{
		if (playerId <= 0)
			return null;
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_iSeizerId == playerId)
				return post;
		}
		return null;
	}

	//================================================================================================
	// Outils internes
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Texte lisible d'une raison de blocage (SRP_ESeizeBlock), sans point final — action de saisie, messages, Staff
	static string BlockText(int block)
	{
		if (block == SRP_ESeizeBlock.ENNEMIS)
		{
			float radius = 100;
			SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
			if (front)
				radius = front.m_fKeyPointClearRadius;
			return string.Format("Des ennemis tiennent encore le point clé (moins de %1 m)", Math.Round(radius));
		}
		if (block == SRP_ESeizeBlock.PAS_AU_CONTACT)
			return "Pas encore au contact : ouvrez un chemin de carrés bleus jusqu'ici";
		if (block == SRP_ESeizeBlock.GELE)
			return "Front gelé par l'état-major";
		if (block == SRP_ESeizeBlock.AUTRE_SOLDAT)
			return "Saisie en cours par un autre soldat";
		if (block == SRP_ESeizeBlock.TOUCHE)
			return "Saisie interrompue : tu as été touché";
		if (block == SRP_ESeizeBlock.GARNISON_EN_ROUTE)
			return "L'ennemi n'a pas fini de s'installer";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Raison de blocage calculée (sans l'écrire) ; LIBRE pour un point déjà saisi ou d'une zone à nous — UpdateBlock,
	//! SeizeWatch, TrySeize
	protected int ComputeBlock(SRP_KeyPointPost post, SRP_FrontKeyPoint kp)
	{
		if (m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.ROUGE)
			return SRP_ESeizeBlock.LIBRE;
		if (m_Front.GetZoneOwner(m_Front.GetCellZone(kp.m_iCell)) != SRP_EFrontOwner.ROUGE)
			return SRP_ESeizeBlock.LIBRE;

		if (m_Front.IsFrozen())
			return SRP_ESeizeBlock.GELE;
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (!enemy || !enemy.IsGarrisonSettled(post.m_sLocality))
			return SRP_ESeizeBlock.GARNISON_EN_ROUTE;
		if (!m_Front.HasNeighbourOwnedBy(kp.m_iCell, SRP_EFrontOwner.BLEU, true))
			return SRP_ESeizeBlock.PAS_AU_CONTACT;
		SRP_FrontCapture capture = m_Front.GetCapture();
		if (capture && capture.CountFitEnemiesNear(PostPosition(post, kp), m_Front.m_fKeyPointClearRadius) > 0)
			return SRP_ESeizeBlock.ENNEMIS;
		if (System.GetTickCount() < post.m_iHitLockUntilMs)
			return SRP_ESeizeBlock.TOUCHE;
		return SRP_ESeizeBlock.LIBRE;
	}

	//------------------------------------------------------------------------------------------------
	//! Conditions de fin de saisie revérifiées au serveur ; "" si tout est bon, sinon le refus lisible — TrySeize
	protected string SeizeRefusal(SRP_KeyPointPost post, SRP_FrontKeyPoint kp, IEntity user, int playerId)
	{
		if (m_Front.GetCellOwner(kp.m_iCell) == SRP_EFrontOwner.BLEU)
			return "Ce point clé est déjà saisi.";
		if (m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.ROUGE)
			return "Ce point clé est hors jeu.";
		if (m_Front.GetZoneOwner(m_Front.GetCellZone(kp.m_iCell)) != SRP_EFrontOwner.ROUGE)
			return "Cette zone est déjà à nous.";
		if (!user || SRP_Utils.IsDead(user) || IsUnconscious(user))
			return "Saisie interrompue : tu es hors de combat.";
		if (post.m_CP && vector.Distance(user.GetOrigin(), post.m_CP.GetOrigin()) > m_Front.m_fSeizeMaxDistance)
			return "Saisie interrompue : tu t'es éloigné du poste.";

		int block = ComputeBlock(post, kp);
		if (block != SRP_ESeizeBlock.LIBRE)
			return BlockText(block) + ".";
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (!enemy || !enemy.IsGarrisonPosted(post.m_sLocality))
			return "L'ennemi n'est pas installé ici : rien à saisir pour l'instant.";
		SRP_FrontCapture capture = m_Front.GetCapture();
		if (capture && !capture.IsPlayerCounted(playerId))
			return "Saisie refusée : tu ne comptes pas pour la prise en ce moment (téléportation ou Game Master récents).";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Poste voulu : zone rouge, carré du point rouge et garnison de la localité posée (même vide, F12) — UpdateCP
	protected bool IsCPWanted(SRP_KeyPointPost post, SRP_FrontKeyPoint kp)
	{
		if (post.m_sLocality.IsEmpty())
			return false;
		if (m_Front.GetCellOwner(kp.m_iCell) != SRP_EFrontOwner.ROUGE)
			return false;
		if (m_Front.GetZoneOwner(m_Front.GetCellZone(kp.m_iCell)) != SRP_EFrontOwner.ROUGE)
			return false;
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		return enemy && enemy.IsGarrisonPosted(post.m_sLocality);
	}

	//------------------------------------------------------------------------------------------------
	//! Points clés de la zone qui comptent pour la chute et la perte (m_bAllLocalitiesRequired : ceux de toutes ses
	//! localités, sinon ceux de la principale) ; rend leur nombre, taken = ceux dont le carré est bleu —
	//! EvaluateZone, GetZoneFallText, Seize
	protected int CountKeyPoints(int zone, out int taken)
	{
		taken = 0;
		SRP_FrontZone info = m_Front.GetZone(zone);
		if (!info)
			return 0;
		int count = m_Front.GetKeyPointCount(zone);
		int total = 0;
		for (int k = 0; k < count; k++)
		{
			SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(m_Front.GetKeyPointIndex(zone, k));
			if (!kp)
				continue;
			if (!m_Front.m_bAllLocalitiesRequired && info.m_iLocality >= 0 && kp.m_iLocality != info.m_iLocality)
				continue;
			total++;
			if (m_Front.GetCellOwner(kp.m_iCell) == SRP_EFrontOwner.BLEU)
				taken++;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés bleus requis pour la chute (D1 : m_iZoneFallPercent ; Q4 sans point clé : m_iNoLocalityFallPercent) —
	//! GetZoneFallText, Seize
	protected int NeededBlue(int total, int keys)
	{
		int percent = m_Front.m_iZoneFallPercent;
		if (keys == 0)
			percent = m_Front.m_iNoLocalityFallPercent;
		return (total * percent + 99) / 100;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré bleu sous le point clé d'index global donné — IsLocalityTaken
	protected bool IsKeyPointBlue(int index)
	{
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(index);
		return kp && m_Front.GetCellOwner(kp.m_iCell) == SRP_EFrontOwner.BLEU;
	}

	//------------------------------------------------------------------------------------------------
	//! Localités de la zone (et localités de ses points clés) pas encore prises, par nom ; aussi la ville voisine dont
	//! le QG est sur un carré de la zone (la repeinte le fait basculer : la ville peut être prise sans saisie) —
	//! CaptureZone, ForceZone
	protected void CollectUntakenLocalities(int zone, notnull array<string> names)
	{
		names.Clear();
		array<int> localities = {};
		m_Front.GetZoneLocalities(zone, localities);
		int count = m_Front.GetKeyPointCount(zone);
		for (int k = 0; k < count; k++)
		{
			SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(m_Front.GetKeyPointIndex(zone, k));
			if (kp && kp.m_iLocality >= 0 && !localities.Contains(kp.m_iLocality))
				localities.Insert(kp.m_iLocality);
		}
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			SRP_FrontKeyPoint cellPoint = m_Front.GetKeyPoint(post.m_iKeyPoint);
			if (cellPoint && cellPoint.m_iLocality >= 0 && m_Front.GetCellZone(cellPoint.m_iCell) == zone && !localities.Contains(cellPoint.m_iLocality))
				localities.Insert(cellPoint.m_iLocality);
		}
		foreach (int index : localities)
		{
			SRP_FrontLocality place = m_Front.GetLocality(index);
			if (!place || place.m_sName.IsEmpty() || names.Contains(place.m_sName))
				continue;
			if (!IsLocalityTaken(place.m_sName))
				names.Insert(place.m_sName);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Repeint en silence (m_bRepainting) les carrés de la zone qui n'appartiennent pas à owner ; rend le nombre
	//! appliqué. Le QG d'une ville voisine posé sur un carré repeint fait réévaluer la zone de sa ville (sauf STAFF,
	//! Q9) — CaptureZone, LoseZone, ForceZone
	protected int RepaintZone(int zone, int owner, int reason, string author)
	{
		array<int> cells = {};
		m_Front.GetZoneCells(zone, cells);
		array<int> toPaint = {};
		foreach (int cell : cells)
		{
			if (m_Front.GetCellOwner(cell) != owner && m_Front.IsCellInPlay(cell))
				toPaint.Insert(cell);
		}
		if (toPaint.IsEmpty())
			return 0;
		m_bRepainting = true;
		int painted = m_Front.SetCellsOwner(toPaint, owner, reason, author);
		m_bRepainting = false;

		// OnCellsChanged n'a rien empilé pendant la repeinte : la bascule du QG d'une ville voisine (carré de cette zone,
		// point compté pour la zone de sa ville) doit quand même faire réévaluer la chute ou la perte de cette ville
		if (reason == SRP_EFrontReason.STAFF || reason == SRP_EFrontReason.REMISE || reason == SRP_EFrontReason.RESTAURATION)
			return painted;
		foreach (int paintedCell : toPaint)
		{
			if (m_Front.GetCellOwner(paintedCell) != owner)
				continue;	// carré refusé par le socle : rien n'a changé
			int firstKey;
			if (!m_mCellPost.Find(paintedCell, firstKey))
				continue;
			for (int p = firstKey; p < m_aPosts.Count(); p++)
			{
				SRP_FrontKeyPoint point = m_Front.GetKeyPoint(m_aPosts[p].m_iKeyPoint);
				if (point && point.m_iCell == paintedCell && KeyPointZone(point) != zone)
					AddZoneToCheck(KeyPointZone(point));
			}
		}
		return painted;
	}

	//------------------------------------------------------------------------------------------------
	//! Arrête les saisies en cours sur les points de la zone (elle change de camp) — CaptureZone, LoseZone, ForceZone
	protected void StopSeizuresInZone(int zone)
	{
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_iSeizerId != 0 && IsPostInZone(post, zone))
				StopSeize(post, SRP_ESeizeBlock.LIBRE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le point compte pour cette zone (celle de sa localité, m_iZone), ou son carré y est (QG d'une ville posé sur un
	//! carré d'une zone voisine : la chute de l'une ou de l'autre le touche) — OnFrontReset, StopSeizuresInZone
	protected bool IsPostInZone(SRP_KeyPointPost post, int zone)
	{
		SRP_FrontKeyPoint kp = m_Front.GetKeyPoint(post.m_iKeyPoint);
		if (!kp)
			return false;
		return KeyPointZone(kp) == zone || m_Front.GetCellZone(kp.m_iCell) == zone;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone pour laquelle compte le point clé : m_iZone (zone de sa localité, fixée par le bâtisseur), à défaut la
	//! zone de son carré — Seize, IsPostInZone, ForceSeize
	protected int KeyPointZone(SRP_FrontKeyPoint kp)
	{
		if (kp.m_iZone > 0)
			return kp.m_iZone;
		return m_Front.GetCellZone(kp.m_iCell);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone à réévaluer au prochain passage (une seule fois, jamais la base)
	protected void AddZoneToCheck(int zone)
	{
		if (zone <= 0)
			return;
		if (m_aZonesToCheck.Find(zone) < 0)
			m_aZonesToCheck.Insert(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Le point d'index global donné, null sinon (les postes suivent l'ordre du registre)
	protected SRP_KeyPointPost FindPostByKeyPoint(int keyPoint)
	{
		if (keyPoint >= 0 && keyPoint < m_aPosts.Count() && m_aPosts[keyPoint].m_iKeyPoint == keyPoint)
			return m_aPosts[keyPoint];
		foreach (SRP_KeyPointPost post : m_aPosts)
		{
			if (post.m_iKeyPoint == keyPoint)
				return post;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Emplacement du poste puis du mât : repère = position exacte et orientation du repère ; nom de la carte = point
	//! libre à 15 m près ; faux dans l'eau. Gardé pour le mât qui remplace le poste.
	protected bool GetSpot(SRP_KeyPointPost post, SRP_FrontKeyPoint kp, out vector spot, out float yaw)
	{
		if (post.m_bHasSpot)
		{
			spot = post.m_vSpot;
			yaw = post.m_fSpotYaw;
			return true;
		}

		yaw = kp.m_fYaw;
		// Deux sources seulement (Q8) : la ligne « point » de front_retouches.txt n'existe plus
		if (kp.m_iSource == SRP_EKeyPointSource.REPERE)
		{
			spot = kp.m_vPos;
		}
		else
		{
			vector free;
			if (SCR_WorldTools.FindEmptyTerrainPosition(free, kp.m_vPos, 15, 1.5, 2) && !SRP_Placement.IsWater(free))
				spot = free;
			else
				spot = SRP_Placement.OnGround(kp.m_vPos);
		}
		if (SRP_Placement.IsWater(spot))
			return false;

		post.m_vSpot = spot;
		post.m_fSpotYaw = yaw;
		post.m_bHasSpot = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Où compter les ennemis (D5) : le poste s'il est posé, sinon le point clé
	protected vector PostPosition(SRP_KeyPointPost post, SRP_FrontKeyPoint kp)
	{
		if (post.m_CP && !post.m_CP.IsDeleted())
			return post.m_CP.GetOrigin();
		return kp.m_vPos;
	}

	//------------------------------------------------------------------------------------------------
	//! Tente de commandement du QG à 6 m du poste (derrière, puis sur les côtés, puis devant), seulement avec 4 m
	//! de dégagement — UpdateCP
	protected void PlaceDecor(SRP_KeyPointPost post, vector center, float yaw)
	{
		ResourceName prefab = m_Front.m_sHQDecorPrefab;
		if (prefab.IsEmpty())
			return;
		if (post.m_Decor && !post.m_Decor.IsDeleted())
			return;

		float radians = yaw * Math.DEG2RAD;
		vector forward = Vector(Math.Sin(radians), 0, Math.Cos(radians));
		vector right = Vector(forward[2], 0, -forward[0]);
		array<vector> directions = {};
		directions.Insert(forward * -1);
		directions.Insert(right);
		directions.Insert(right * -1);
		directions.Insert(forward);
		foreach (vector direction : directions)
		{
			vector candidate = SRP_Placement.OnGround(center + direction * DECOR_DISTANCE);
			vector free;
			if (!SCR_WorldTools.FindEmptyTerrainPosition(free, candidate, DECOR_CLEARANCE + 1, DECOR_CLEARANCE, DECOR_TOP))
				continue;
			if (SRP_Placement.IsWater(free))
				continue;
			post.m_Decor = SpawnAt(prefab, free, yaw);
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Pose d'un prefab (serveur), orienté (lacet en degrés) — UpdateCP, UpdateFlag, RespawnFlags, PlaceDecor
	protected IEntity SpawnAt(ResourceName prefab, vector position, float yaw)
	{
		if (prefab.IsEmpty())
			return null;
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
			return null;
		vector mat[4];
		Math3D.AnglesToMatrix(Vector(yaw, 0, 0), mat);
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
	//! Supprime l'entité si aucun joueur n'est à moins de m_fCPDeleteMinPlayerDistance et que personne ne la voit
	//! (pied, et sommet si top > 0) ; vrai si elle n'existe plus — UpdateCP, UpdateFlag
	protected bool DeleteIfUnseen(IEntity entity, float top)
	{
		if (!entity || entity.IsDeleted())
			return true;
		vector position = entity.GetOrigin();
		if (SRP_Placement.NearestPlayer(position, m_aPlayers) <= m_Front.m_fCPDeleteMinPlayerDistance)
			return false;
		if (IsSeen(position, top))
			return false;
		SCR_EntityHelper.DeleteEntityAndChildren(entity);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur voit-il le point (ou le sommet à « top » mètres au-dessus) ?
	protected bool IsSeen(vector position, float top)
	{
		if (SRP_Placement.IsSeenByAnyPlayer(position, m_aPlayers))
			return true;
		if (top <= 0)
			return false;
		vector summit = position;
		summit[1] = position[1] + top;
		return SRP_Placement.IsSeenByAnyPlayer(summit, m_aPlayers);
	}

	//------------------------------------------------------------------------------------------------
	//! Échec de pose : nouvel essai dans m_iCPRetrySeconds, une ligne ENNEMI par épisode — UpdateCP
	protected void FailCP(SRP_KeyPointPost post, string why, int nowUnix)
	{
		post.m_iNextCPTry = nowUnix + m_Front.m_iCPRetrySeconds;
		if (post.m_bCPLogged)
			return;
		post.m_bCPLogged = true;
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("Poste de commandement de %1 non posé : %2 ; nouvel essai toutes les %3 s", post.m_sLocality, why, m_Front.m_iCPRetrySeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! Personnages des joueurs connectés, relevés une fois par passage
	protected void CollectPlayers()
	{
		m_aPlayers.Clear();
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character)
				m_aPlayers.Insert(character);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Personnage inconscient ?
	protected static bool IsUnconscious(IEntity character)
	{
		if (!character)
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		return controller && controller.IsUnconscious();
	}

	//------------------------------------------------------------------------------------------------
	//! Composant du poste posé, null sinon
	protected SRP_PosteCommandementComponent PostComponent(SRP_KeyPointPost post)
	{
		if (!post || !post.m_CP || post.m_CP.IsDeleted())
			return null;
		return SRP_PosteCommandementComponent.Cast(post.m_CP.FindComponent(SRP_PosteCommandementComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Durée de l'action de saisie réglée dans le prefab du poste (Duration), 60 s à défaut — TrySeize
	protected float ActionSeconds(IEntity cp)
	{
		float seconds = 60;
		if (!cp)
			return seconds;
		ActionsManagerComponent manager = ActionsManagerComponent.Cast(cp.FindComponent(ActionsManagerComponent));
		if (!manager)
			return seconds;
		array<BaseUserAction> actions = {};
		manager.GetActionsList(actions);
		foreach (BaseUserAction action : actions)
		{
			SRP_SaisiePCAction seize = SRP_SaisiePCAction.Cast(action);
			if (seize && seize.GetActionDuration() > 0)
				return seize.GetActionDuration();
		}
		return seconds;
	}

	//------------------------------------------------------------------------------------------------
	//! « Montignac — QG », « Montignac — centre » (ville), « Montignac » (village, hameau)
	protected string LabelOf(SRP_KeyPointPost post, SRP_FrontKeyPoint kp)
	{
		if (kp.m_iRole == SRP_EKeyPointRole.QG)
			return post.m_sLocality + " — QG";
		SRP_FrontLocality place = m_Front.GetLocality(kp.m_iLocality);
		if (place && place.m_iQg >= 0)
			return post.m_sLocality + " — centre";
		return post.m_sLocality;
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
	//! « à nous » ou « à l'ennemi » (comptes rendus du Staff)
	protected static string OwnerWord(int owner)
	{
		if (owner == SRP_EFrontOwner.BLEU)
			return "à nous";
		return "à l'ennemi";
	}
}

//------------------------------------------------------------------------------------------------
//! Composant du prefab Prefabs/Props/SRP_PosteCommandement.et (RplComponent obligatoire) : état de saisie répliqué
//! pour que l'action affiche la bonne raison chez le joueur (modèle SCR_CacheNoteComponent.c:4-25)
[ComponentEditorProps(category: "SimpleRP", description: "Poste de commandement ennemi : point clé à saisir (D4)")]
class SRP_PosteCommandementComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class SRP_PosteCommandementComponent : ScriptComponent
{
	[RplProp()]
	protected int m_iBlock;			// SRP_ESeizeBlock
	[RplProp()]
	protected int m_iSeizerId;		// joueur qui saisit, 0 = personne
	[RplProp()]
	protected string m_sLabel;		// « Montignac — QG »

	//------------------------------------------------------------------------------------------------
	//! Serveur : affecte, puis Replication.BumpMe() seulement si quelque chose change — SRP_ZoneFall
	void SetState(int block, int seizer)
	{
		if (m_iBlock == block && m_iSeizerId == seizer)
			return;
		m_iBlock = block;
		m_iSeizerId = seizer;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : nom affiché du poste, puis BumpMe — SRP_ZoneFall.UpdateCP
	void SetLabel(string label)
	{
		if (m_sLabel == label)
			return;
		m_sLabel = label;
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ESeizeBlock répliqué — SRP_SaisiePCAction.CanBePerformedScript
	int GetBlock()
	{
		return m_iBlock;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur qui saisit, 0 = personne — SRP_SaisiePCAction.CanBePerformedScript
	int GetSeizer()
	{
		return m_iSeizerId;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom affiché du poste — SRP_SaisiePCAction
	string GetLabel()
	{
		return m_sLabel;
	}
}

//------------------------------------------------------------------------------------------------
//! Action « Saisir le poste de commandement » du prefab (Duration 60 réglée dans le .et). Modèle
//! SRP_DepotInstallAction. Le gestionnaire d'interaction annule dès que CanBePerformed rend faux.
class SRP_SaisiePCAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	//! Serveur : SRP_ZoneFall.OnSeizeStart(GetOwner(), user) — le moteur, chez l'autorité
	override void OnActionStart(IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		SRP_ZoneFall fall = GetZoneFall();
		if (fall)
			fall.OnSeizeStart(GetOwner(), pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : SRP_ZoneFall.OnSeizeCancel — le moteur
	override void OnActionCanceled(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		SRP_ZoneFall fall = GetZoneFall();
		if (!fall)
			return;
		IEntity owner = pOwnerEntity;
		if (!owner)
			owner = GetOwner();
		fall.OnSeizeCancel(owner, pUserEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : SRP_ZoneFall.TrySeize, puis SRP_Utils.NotifyPlayer du refus — le moteur
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int userId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		SRP_ZoneFall fall = GetZoneFall();
		if (userId <= 0 || !fall)
			return;
		IEntity owner = pOwnerEntity;
		if (!owner)
			owner = GetOwner();
		string reply = fall.TrySeize(owner, pUserEntity);
		if (!reply.IsEmpty())
			SRP_Utils.NotifyPlayer(userId, reply);
	}

	//------------------------------------------------------------------------------------------------
	//! « Saisir le poste de commandement »
	override bool GetActionNameScript(out string outName)
	{
		outName = "Saisir le poste de commandement";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Composant présent et utilisateur à pied
	override bool CanBeShownScript(IEntity user)
	{
		if (!GetPostComponent())
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(user);
		if (character && character.IsInVehicle())
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Client et serveur : autre saisisseur ou raison différente de LIBRE -> SetCannotPerformReason (texte lisible de
	//! SRP_ESeizeBlock) et faux
	override bool CanBePerformedScript(IEntity user)
	{
		SRP_PosteCommandementComponent post = GetPostComponent();
		if (!post)
			return false;
		int userId = SRP_Utils.GetPlayerIdFromEntity(user);
		if (userId <= 0)
			userId = SCR_PlayerController.GetLocalPlayerId();	// chez le joueur : sa propre saisie ne doit jamais l'arrêter
		int seizer = post.GetSeizer();
		if (seizer > 0 && seizer != userId)
		{
			SetCannotPerformReason(SRP_ZoneFall.BlockText(SRP_ESeizeBlock.AUTRE_SOLDAT));
			return false;
		}
		int block = post.GetBlock();
		if (block != SRP_ESeizeBlock.LIBRE)
		{
			SetCannotPerformReason(SRP_ZoneFall.BlockText(block));
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBroadcastScript()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Composant du poste porté par l'entité de l'action, null sinon
	protected SRP_PosteCommandementComponent GetPostComponent()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return null;
		return SRP_PosteCommandementComponent.Cast(owner.FindComponent(SRP_PosteCommandementComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! La chute des zones (serveur), null ailleurs ou avant le démarrage du front
	protected SRP_ZoneFall GetZoneFall()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return null;
		return front.GetZoneFall();
	}
}

//------------------------------------------------------------------------------------------------
//! Interruption de la saisie par un coup reçu (aucun champ ajouté). OnDamage existe :
//! Game/Components/Damage/SCR_CharacterDamageManagerComponent.c:2033
modded class SCR_CharacterDamageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	//! super, puis sur le serveur SRP_ZoneFall.NoteHit(GetOwner(), damageType, damageValue) — le moteur
	protected override void OnDamage(notnull BaseDamageContext damageContext)
	{
		super.OnDamage(damageContext);
		if (Replication.IsServer())
			SRP_ZoneFall.NoteHit(GetOwner(), damageContext.damageType, damageContext.damageValue);
	}
}
