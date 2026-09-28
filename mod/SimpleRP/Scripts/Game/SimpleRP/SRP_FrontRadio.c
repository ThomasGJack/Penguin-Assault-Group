//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : RADIO, JOURNAL, ÉVÈNEMENTS DU PONT et REPÈRES DES POINTS CLÉS (module carte, serveur).
// Contrats figés (CONTRATS_FRONT.md §2 et §6, CONTRATS_CMD.md §6 et §8), corps écrits le 26/09.
//
// RÔLE : SRP_FrontRadio est le SEUL auteur des textes publics du front et du Commandeur (trou 15) :
// - radio : SRP_Utils.NotifyAll("Radio : …") (ou NotifyPlayer pour une nouvelle personnelle) ;
// - journal : SRP_EnemyComponent.Journal("TERRITOIRE", …) au niveau zone, ("FRONT", …) carré par carré ;
// - pont : SRP_BridgeComponent.PushZoneEvent(type, code de zone ou "", texte), types de la liste unique.
// Règles de rédaction : aucun signe pour cent (« 6/25 carrés »), string.Format 9 paramètres au plus, jamais les mots
// confidentiels de SRP_Discord.c:246-248 (garnison, groupe(s), patrouille, automatique, « à nous, menace », tirage,
// aucune zone), jamais « IA » ; garder les mots qui colorent Discord (captur, prise, PERDUE, défendu, attaque, vague,
// coupée, reliée, produit, VICTOIRE, Nouvelle campagne, gelé, ramené, officier ennemi, renseignement, dépôt ennemi,
// blindé ennemi).
// Carrés : QueueCell reçoit CHAQUE changement du socle (5e appel direct après chaque lot) ; Flush, en fin de boucle
// de 5 s, écrit le journal FRONT de chaque carré (une ligne par carré pour la capture, la saisie et la reprise ; une
// ligne par zone qui cite tous ses carrés pour une repeinte d'un coup) et une ligne radio seulement pour CAPTURE et
// REPRISE (I5), regroupée au-delà de m_iRadioMergeAbove carrés d'une même zone. Une zone qui bascule d'un coup, le
// Staff, E4, H2, une remise ou une restauration ne donnent aucune ligne radio par carré.
// SRP_FrontMarkers : icônes des points clés (I3) et des dépôts ennemis révélés (RE9), reposées seulement quand leur
// état change (plus de clignotement : l'orange des carrés le remplace).
// APPELÉ PAR : le socle (QueueCell, Flush, Tick des repères, E4, gel, victoire, campagne, restauration), SRP_ZoneFall,
// SRP_FrontEnemyComponent, SRP_FrontEconomy, SRP_MissionManagerComponent (G2), SRP_TerritoryComponent (capture
// abandonnée), le Commandeur (8 annonces).
// Les réglages sont les attributs publics de SRP_FrontComponent (catégories « radio » et « repères ») et, pour le
// Commandeur, les champs vu_* de SRP_CmdScreens.
//------------------------------------------------------------------------------------------------

class SRP_FrontRadio
{
	// File des carrés du passage (vidée par Flush)
	protected static ref array<int> s_aCells = {};
	protected static ref array<int> s_aOwners = {};
	protected static ref array<int> s_aReasons = {};
	// Zones pour lesquelles « la moitié est à nous » a déjà été dite (remise à la chute ou à la perte)
	protected static ref array<int> s_aHalfSaid = {};
	// Zones dont la perte vient d'être annoncée par ZoneFell (vidée à chaque Flush) : leurs reprises du passage ne
	// donnent pas de ligne par carré en plus
	protected static ref array<int> s_aLostSaid = {};
	// Socle de la partie en cours : les statiques survivent aux parties du Workbench, la file d'une autre partie est oubliée
	protected static SRP_FrontComponent s_Front;

	// Passages entre territoire ami et ennemi (PlayerSides), par identifiant de joueur : camp annoncé, camp vu au passage
	// précédent, passages de suite dans ce camp, zone du passage précédent, heure Unix du dernier recensement et du
	// dernier message
	protected static const int SIDE_CONFIRM = 2;			// passages de 5 s de suite dans le nouveau camp avant le message
	protected static const int SIDE_FORGET_S = 15;			// absence du recensement au-delà : retour traité à part
	protected static const int SIDE_GAP_S = 30;				// écart minimal entre deux messages à un même joueur
	protected static const int SIDE_DROP_S = 600;			// absence au-delà : joueur oublié
	protected static ref map<int, int> s_mSideSaid = new map<int, int>();
	protected static ref map<int, int> s_mSideSeen = new map<int, int>();
	protected static ref map<int, int> s_mSideStreak = new map<int, int>();
	protected static ref map<int, int> s_mSideZone = new map<int, int>();
	protected static ref map<int, int> s_mSideLastSeen = new map<int, int>();
	protected static ref map<int, int> s_mSideMsgUnix = new map<int, int>();
	protected static SRP_FrontComponent s_SideFront;

	//================================================================================================
	// Carrés (I5, I7)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Note un carré qui a changé de camp avec sa cause (SRP_EFrontReason) — socle, 5e appel direct après chaque lot
	static void QueueCell(int cell, int owner, int reason)
	{
		CheckSession(SRP_FrontComponent.GetInstance());
		s_aCells.Insert(cell);
		s_aOwners.Insert(owner);
		s_aReasons.Insert(reason);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de chaque boucle de 5 s (4e) : journal FRONT par carré (m_bJournalCells) ; radio CAPTURE / REPRISE par
	//! (zone, sens) : « Radio : carré %1 pris — %2, %3/%4 carrés à nous », « Radio : carré %1 perdu, repris par
	//! l'ennemi — %2, %3/%4 carrés à nous », ou « Radio : %1 carrés pris — %2 : %3 » (au plus m_iRadioMaxRefs
	//! références) ; puis vide la file — SRP_FrontComponent.Tick
	static void Flush()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		CheckSession(front);
		if (s_aCells.IsEmpty())
		{
			s_aLostSaid.Clear();
			return;
		}
		if (!front || !front.IsReady())
		{
			DropQueue();
			return;
		}

		if (front.m_bJournalCells)
			JournalCells(front);
		if (front.m_bRadioCells)
			RadioCells(front);
		DropQueue();
	}

	//------------------------------------------------------------------------------------------------
	//! Vide la file sans rien dire (remise à plat, restauration) — socle
	static void ClearQueue()
	{
		DropQueue();
		// Après une remise à plat, « la moitié est à nous » pourra être redite une fois
		s_aHalfSaid.Clear();
	}

	//================================================================================================
	// Zones (radio + journal TERRITOIRE + pont)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Chute ou perte d'une zone, selon la cause : ZONE_TOMBEE « Zone %1 capturée — menace %2/10 » (zone_prise) ;
	//! ZONE_PERDUE « Zone %1 PERDUE, reprise par l'ennemi — menace %2/10 » (zone_perdue) ; ISOLEMENT « Zone %1 PERDUE :
	//! restée coupée de la base %2 — menace %3/10 » (« 24 h », « 2 min ») (zone_perdue) ; OFFENSIVE « Pendant l'absence de tous, l'ennemi a
	//! pris %1 — menace %2/10 » (offensive) ; STAFF : rien (Q9) — SRP_ZoneFall
	static void ZoneFell(int zone, int owner, int threat, int reason)
	{
		// Mémoires de la partie en cours seulement (s_aLostSaid est lue par le Flush qui suit)
		CheckSession(SRP_FrontComponent.GetInstance());
		// La zone a changé de camp : « la moitié est à nous » pourra être redite à la prochaine campagne sur elle
		s_aHalfSaid.RemoveItem(zone);

		string label = ZoneLabel(zone);
		if (reason == SRP_EFrontReason.ZONE_TOMBEE)
		{
			Publish("zone_prise", zone, string.Format("Zone %1 capturée — menace %2/10", label, threat));
			return;
		}
		// Perte annoncée : les reprises de cette zone dans le même passage ne donnent pas de ligne par carré (RadioCells)
		if (reason == SRP_EFrontReason.ZONE_PERDUE)
		{
			s_aLostSaid.Insert(zone);
			Publish("zone_perdue", zone, string.Format("Zone %1 PERDUE, reprise par l'ennemi — menace %2/10", label, threat));
			return;
		}
		if (reason == SRP_EFrontReason.ISOLEMENT)
		{
			int minutes = 1440;
			SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
			if (front)
				minutes = front.m_iCutLossMinutes;
			s_aLostSaid.Insert(zone);
			Publish("zone_perdue", zone, string.Format("Zone %1 PERDUE : restée coupée de la base %2 — menace %3/10", label, DurationText(minutes * 60), threat));
			return;
		}
		if (reason == SRP_EFrontReason.OFFENSIVE)
		{
			s_aLostSaid.Insert(zone);
			Publish("offensive", zone, string.Format("Pendant l'absence de tous, l'ennemi a pris %1 — menace %2/10", label, threat));
			return;
		}
		// STAFF (correction Q9), REMISE, RESTAURATION : aucune annonce
	}

	//------------------------------------------------------------------------------------------------
	//! D4 : « Radio : poste de commandement ennemi de %1 capturé par %2 — la zone tombera à %3/%4 carrés à nous » ;
	//! « QG ennemi » pour le rôle QG (D6) ; journal TERRITOIRE — SRP_ZoneFall.Seize
	static void KeyPointSeized(int zone, int role, string author, int blue, int need)
	{
		string what = "poste de commandement ennemi";
		if (role == SRP_EKeyPointRole.QG)
			what = "QG ennemi";
		string by = "";
		if (!author.IsEmpty())
			by = " par " + author;

		// Points clés encore ennemis dans la zone (le point saisi est déjà bleu : SetCellOwner passe avant la radio)
		int land = 0;
		int remaining = 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && zone >= 0)
		{
			land = front.CountZoneCells(zone);
			int points = front.GetKeyPointCount(zone);
			for (int k = 0; k < points; k++)
			{
				if (!front.IsKeyPointTaken(zone, k))
					remaining++;
			}
		}

		string text;
		if (remaining > 0)
			text = string.Format("%1 de %2 capturé%3 — reste %4 point(s) clé(s) ennemi(s) dans la zone", what, ZoneLabel(zone), by, remaining);
		else if (blue >= need)
			text = string.Format("%1 de %2 capturé%3 — %4/%5 carrés à nous : la zone tombe", what, ZoneLabel(zone), by, blue, land);
		else
			text = string.Format("%1 de %2 capturé%3 — la zone tombera à %4/%5 carrés à nous (actuellement %6)", what, ZoneLabel(zone), by, need, land, blue);
		Say(text);
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! D1, radio seule, une fois par zone (m_bRadioHalf) : « Radio : %1 — la moitié des carrés est à nous, reste le PC
	//! ennemi » — SRP_ZoneFall.EvaluateZone
	static void HalfReached(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.m_bRadioHalf || zone < 0)
			return;
		CheckSession(front);
		if (s_aHalfSaid.Find(zone) >= 0)
			return;
		s_aHalfSaid.Insert(zone);

		int remaining = 0;
		int points = front.GetKeyPointCount(zone);
		for (int k = 0; k < points; k++)
		{
			if (!front.IsKeyPointTaken(zone, k))
				remaining++;
		}
		if (remaining > 1)
			Say(string.Format("%1 — la moitié des carrés est à nous, restent %2 PC ennemis", ZoneLabel(zone), remaining));
		else
			Say(string.Format("%1 — la moitié des carrés est à nous, reste le PC ennemi", ZoneLabel(zone)));
	}

	//------------------------------------------------------------------------------------------------
	//! F6 (G15 : alerte seule) : « Renseignement : l'ennemi prépare une attaque sur %1, assaut dans %2 minutes.
	//! Défendez la zone. » ; évènement zone_attaque — SRP_FrontEnemyComponent.LaunchAttack
	static void AttackAnnounced(int zone, int minutes)
	{
		Publish("zone_attaque", zone, string.Format("Renseignement : l'ennemi prépare une attaque sur %1, assaut dans %2 minutes. Défendez la zone.", ZoneLabel(zone), minutes));
	}

	//------------------------------------------------------------------------------------------------
	//! « Radio : %1 — l'ennemi attaque, vague %2, mouvement signalé depuis %3 » ; évènement zone_assaut à la 1re vague
	//! seulement — SRP_FrontEnemyComponent.SendWave
	static void AttackWave(int zone, int wave, string axes)
	{
		string text;
		if (axes.IsEmpty())
			text = string.Format("%1 — l'ennemi attaque, vague %2", ZoneLabel(zone), wave);
		else
			text = string.Format("%1 — l'ennemi attaque, vague %2, mouvement signalé depuis %3", ZoneLabel(zone), wave, axes);
		// Radio seule après la 1re vague (§6) : le salon et le site ne reçoivent que le début de l'assaut
		if (wave <= 1)
			Publish("zone_assaut", zone, text);
		else
			Say(text);
	}

	//------------------------------------------------------------------------------------------------
	//! F4, C8 : « Zone %1 défendue, %2 vague(s) repoussée(s) — menace %3/10 » ; évènement zone_defendue —
	//! SRP_FrontEnemyComponent.EndAttack
	static void Defended(int zone, int waves, int threat)
	{
		Publish("zone_defendue", zone, string.Format("Zone %1 défendue, %2 vague(s) repoussée(s) — menace %3/10", ZoneLabel(zone), waves, threat));
	}

	//------------------------------------------------------------------------------------------------
	//! C8 (60 min) : « Radio : %1 — l'ennemi rompt le contact et se replie ; %2 carré(s) repris par lui restent
	//! rouges. » ; journal ; évènement zone_assaut — SRP_FrontEnemyComponent.EndAttack
	static void AttackBroken(int zone, int retaken)
	{
		string text;
		if (retaken > 0)
			text = string.Format("%1 — l'ennemi rompt le contact et se replie ; %2 carré(s) repris par lui restent rouges.", ZoneLabel(zone), retaken);
		else
			text = string.Format("%1 — l'ennemi rompt le contact et se replie sans avoir repris de carré.", ZoneLabel(zone));
		Publish("zone_assaut", zone, text);
	}

	//------------------------------------------------------------------------------------------------
	//! « Radio : l'attaque ennemie annoncée sur %1 n'aura pas lieu. » ; journal, pas d'évènement —
	//! SRP_FrontEnemyComponent.EndAttack (ANNULEE pendant l'annonce)
	static void AttackCalledOff(int zone)
	{
		string text = string.Format("l'attaque ennemie annoncée sur %1 n'aura pas lieu.", ZoneLabel(zone));
		Say(text);
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! E4 : « Radio : %1 est coupée de la base : plus de production, perdue dans %2 si le passage n'est pas rouvert »
	//! (%2 : « 24 h », « 1 h 30 », « 2 min », tiré de m_iCutLossMinutes) ; évènement zone_coupee —
	//! SRP_FrontComponent.CheckCuts
	static void ZoneCut(int zone, int hours)
	{
		// Le socle passe des heures arrondies (jamais « 0 h ») : le délai exact vient du réglage, hours ne sert qu'à défaut
		int minutes = hours * 60;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			minutes = Math.MaxInt(front.m_iCutLossMinutes, 1);
		Publish("zone_coupee", zone, string.Format("%1 est coupée de la base : plus de production, perdue dans %2 si le passage n'est pas rouvert", ZoneLabel(zone), DurationText(minutes * 60)));
	}

	//------------------------------------------------------------------------------------------------
	//! E4, radio seule, m_iCutReminderMinutes avant la perte : « Radio : %1 coupée de la base, perdue dans %2 min » —
	//! SRP_FrontComponent.CheckCuts
	static void CutReminder(int zone, int minutesLeft)
	{
		Say(string.Format("%1 coupée de la base, perdue dans %2 min", ZoneLabel(zone), minutesLeft));
	}

	//------------------------------------------------------------------------------------------------
	//! E4 : « Radio : %1 de nouveau reliée à la base » ; évènement zone_reliee — SRP_FrontComponent.CheckCuts
	static void Reconnected(int zone)
	{
		Publish("zone_reliee", zone, string.Format("%1 de nouveau reliée à la base", ZoneLabel(zone)));
	}

	//------------------------------------------------------------------------------------------------
	//! J3 : « Front gelé par l'état-major : ni capture ni perte » / « Front dégelé » ; évènement front —
	//! SRP_FrontComponent.SetFrozen
	static void Frozen(bool on, string author)
	{
		// L'auteur n'est pas cité en public : la ligne [STAFF] de SRP_FrontScreens.RunStaff le garde
		if (on)
			Publish("front", -1, "Front gelé par l'état-major : ni capture ni perte");
		else
			Publish("front", -1, "Front dégelé par l'état-major : captures, pertes et contre-attaques reprennent");
	}

	//------------------------------------------------------------------------------------------------
	//! J2 : UNE ligne « Front ramené à l'état du %1 » ; évènement front (remise à plat, jamais des dizaines de prises)
	//! — SRP_FrontComponent.RestoreDaily
	static void Restored(string day, string author)
	{
		Publish("front", -1, "Front ramené à l'état du " + DayText(day));
	}

	//------------------------------------------------------------------------------------------------
	//! B3, K1 : « Nouvelle campagne : tout est à reprendre depuis Levie » ; évènement campagne —
	//! SRP_FrontComponent.ResetCampaign
	static void NewCampaign(int campaign)
	{
		// Nom de la base sans « Base de » (« Base de Levie » -> « Levie »), la base à défaut
		string place = "la base";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
		{
			string baseName = front.GetZoneName(0);
			if (baseName.StartsWith("Base de ") && baseName.Length() > 8)
				place = baseName.Substring(8, baseName.Length() - 8);
			else if (!baseName.IsEmpty())
				place = baseName;
		}
		Publish("campagne", -1, string.Format("Nouvelle campagne (n° %1) : tout est à reprendre depuis %2", campaign, place));
	}

	//------------------------------------------------------------------------------------------------
	//! B3 : « VICTOIRE : toute l'île est à nous » ; évènement victoire — SRP_FrontComponent.CheckVictory
	static void Victory()
	{
		Publish("victoire", -1, "VICTOIRE : toute l'île est à nous");
	}

	//------------------------------------------------------------------------------------------------
	//! G2 : « Zone %1 capturée SANS mission activée au tableau : ni prime, ni crédit de mission » ; journal
	//! TERRITOIRE, évènement commandement, NotifyOfficiers — SRP_MissionManagerComponent.OnZoneCaptured (seul payeur)
	static void CaptureWithoutMission(int zone)
	{
		string text = string.Format("Zone %1 capturée SANS mission activée au tableau : ni prime, ni crédit de mission", ZoneLabel(zone));
		JournalLine(text);
		SendEvent("commandement", zone, text);
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Mission de capture abandonnée : « Radio : capture de %1 abandonnée : l'ennemi reprend position — menace %2/10 »
	//! (libellé de zone ; zone -1 : « la zone visée ») ; journal TERRITOIRE, sans évènement ni mot confidentiel —
	//! SRP_TerritoryComponent.OnCaptureFailed (après AddThreat(+1))
	static void CaptureAbandoned(int zone, int threat)
	{
		string text = string.Format("capture de %1 abandonnée : l'ennemi reprend position — menace %2/10", ZoneLabel(zone), threat);
		Say(text);
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! H2 : texte du résultat pour les nouvelles personnelles ; en cas d'ÉCHEC, annonce aussi (journal, évènement
	//! offensive) ; en cas de réussite, l'annonce est faite par ZoneFell(OFFENSIVE) — SRP_FrontEnemyComponent.RunOffensive
	static string NightOffensive(int zone, bool success, int threat)
	{
		string label = "une zone du front";
		if (zone >= 0)
			label = ZoneLabel(zone);
		if (success)
			return string.Format("Pendant l'absence de tous, l'ennemi a pris %1 — menace %2/10", label, threat);

		string text = string.Format("Pendant l'absence de tous, l'ennemi a attaqué %1 sans parvenir à la prendre — menace %2/10", label, threat);
		// Journal et évènement seulement : le serveur était vide, chacun l'apprend à sa première apparition (NightNews)
		JournalLine(text);
		SendEvent("offensive", zone, text);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! H2 : « Radio : » + texte au joueur, à sa première apparition — SRP_FrontEnemyComponent.OnPlayerSpawnedNews
	static void NightNews(int playerId, string text)
	{
		if (text.IsEmpty())
			return;
		SRP_Utils.NotifyPlayer(playerId, "Radio : " + text);
	}

	//------------------------------------------------------------------------------------------------
	//! Choix de Jack du 27/09 : à chaque passage d'un joueur entre territoire ami et ennemi (camp de la ZONE du carré,
	//! la base compte bleue), message radio pour lui seul : « vous entrez en territoire ennemi — Régina (S07) » ou
	//! « vous êtes de retour en territoire ami — … ». Le nouveau camp doit être vu SIDE_CONFIRM passages de suite, et
	//! SIDE_GAP_S secondes au moins séparent deux messages (pas de rafale en longeant une limite). Une zone qui change de
	//! camp sous les pieds du joueur (chute, perte, restauration) n'est pas un passage : noté sans message. Retour dans le
	//! recensement après SIDE_FORGET_S secondes d'absence (mort, délai de grâce après l'apparition, Game Master, véhicule
	//! haut) : en territoire ami ou dans la même zone, noté sans message ; arrivé en zone ennemie entre-temps, annoncé
	//! au passage suivant. En mer ou hors de la grille : toujours suivi, camp inchangé. Réglage front_radio_passage
	//! (m_bRadioCrossing) — SRP_FrontCapture.Tick, après le recensement
	static void PlayerSides(SRP_FrontComponent front, notnull array<int> ids, notnull array<int> cells, int nowUnix)
	{
		if (!front)
			return;
		if (front != s_SideFront)
		{
			// autre partie (Workbench) : on repart de zéro
			s_SideFront = front;
			s_mSideSaid.Clear();
			s_mSideSeen.Clear();
			s_mSideStreak.Clear();
			s_mSideZone.Clear();
			s_mSideLastSeen.Clear();
			s_mSideMsgUnix.Clear();
		}

		int count = Math.MinInt(ids.Count(), cells.Count());
		for (int i = 0; i < count; i++)
		{
			int playerId = ids[i];
			int lastSeen;
			bool known = s_mSideLastSeen.Find(playerId, lastSeen);
			if (known && (nowUnix - lastSeen > SIDE_FORGET_S || !s_mSideSaid.Contains(playerId)))
				known = false;

			int zone = -1;
			if (cells[i] >= 0)
				zone = front.GetCellZone(cells[i]);
			int side = -1;
			if (zone >= 0)
				side = front.GetZoneOwner(zone);
			if (side != SRP_EFrontOwner.ROUGE && side != SRP_EFrontOwner.BLEU)
			{
				// En mer ou hors de la grille : toujours recensé, camp inchangé
				if (known)
					s_mSideLastSeen.Set(playerId, nowUnix);
				continue;
			}
			s_mSideLastSeen.Set(playerId, nowUnix);

			int lastZone;
			bool sameZone = false;
			if (s_mSideZone.Find(playerId, lastZone))
				sameZone = lastZone == zone;
			s_mSideZone.Set(playerId, zone);

			if (!known)
			{
				s_mSideSeen.Set(playerId, side);
				if (side == SRP_EFrontOwner.BLEU || sameZone)
				{
					// territoire ami, ou relevé sur place : noté sans message
					s_mSideSaid.Set(playerId, side);
					s_mSideStreak.Set(playerId, SIDE_CONFIRM);
				}
				else
				{
					// arrivé en zone ennemie pendant l'absence (délai de grâce, vol, Game Master) : annoncé au passage suivant
					s_mSideSaid.Set(playerId, SRP_EFrontOwner.BLEU);
					s_mSideStreak.Set(playerId, 1);
				}
				continue;
			}

			if (sameZone && s_mSideSeen.Get(playerId) != side)
			{
				// la zone a changé de camp sous ses pieds (chute, perte, restauration) : pas un passage, noté sans message
				s_mSideSaid.Set(playerId, side);
				s_mSideSeen.Set(playerId, side);
				s_mSideStreak.Set(playerId, SIDE_CONFIRM);
				continue;
			}

			int streak = 1;
			if (s_mSideSeen.Get(playerId) == side)
				streak = s_mSideStreak.Get(playerId) + 1;
			s_mSideSeen.Set(playerId, side);
			s_mSideStreak.Set(playerId, streak);
			if (streak < SIDE_CONFIRM || s_mSideSaid.Get(playerId) == side)
				continue;

			// Deux messages trop proches : redit au passage suivant s'il y est encore
			int lastMsg;
			if (s_mSideMsgUnix.Find(playerId, lastMsg) && nowUnix - lastMsg < SIDE_GAP_S)
				continue;

			s_mSideSaid.Set(playerId, side);
			s_mSideMsgUnix.Set(playerId, nowUnix);
			if (!front.m_bRadioCrossing)
				continue;
			string label = front.GetZoneLabel(zone);
			if (side == SRP_EFrontOwner.ROUGE)
				SRP_Utils.NotifyPlayer(playerId, "Radio : vous entrez en territoire ennemi — " + label + ".");
			else
				SRP_Utils.NotifyPlayer(playerId, "Radio : vous êtes de retour en territoire ami — " + label + ".");
		}

		// Joueurs absents depuis longtemps (déconnectés) : oubliés
		array<int> gone = {};
		for (int k = 0; k < s_mSideLastSeen.Count(); k++)
		{
			if (nowUnix - s_mSideLastSeen.GetElement(k) > SIDE_DROP_S)
				gone.Insert(s_mSideLastSeen.GetKey(k));
		}
		foreach (int goneId : gone)
		{
			s_mSideSaid.Remove(goneId);
			s_mSideSeen.Remove(goneId);
			s_mSideStreak.Remove(goneId);
			s_mSideZone.Remove(goneId);
			s_mSideLastSeen.Remove(goneId);
			s_mSideMsgUnix.Remove(goneId);
		}
	}

	//================================================================================================
	// Économie (G4 à G7) et migration (K1, K2)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! « Radio : %1 — %2 paquet(s) de %3 produits à ramasser au dépôt, carré %4 » — SRP_FrontEconomy.Produce
	static void DepotProduced(int zone, int packets, int resource, string cellRef)
	{
		string text = string.Format("%1 — %2 paquet(s) de %3 produits à ramasser au dépôt, carré %4", ZoneLabel(zone), packets, ResourceWord(resource), cellRef);
		Say(text);
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! « Radio : %1 a produit %2 paquet(s) de %3, mais n'a pas de dépôt : posez un kit » — SRP_FrontEconomy.Produce
	static void DepotNoDepot(int zone, int packets, int resource)
	{
		string text = string.Format("%1 a produit %2 paquet(s) de %3, mais n'a pas de dépôt : posez un kit", ZoneLabel(zone), packets, ResourceWord(resource));
		Say(text);
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! « Dépôt de %1 installé par %2, carré %3 » (radio, journal) — SRP_FrontEconomy.InstallDepot
	static void DepotInstalled(int zone, string author, string cellRef)
	{
		string text = string.Format("Dépôt de %1 installé par %2, carré %3", ZoneLabel(zone), author, cellRef);
		Say(text);
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! G7 : « Dépôt de %1 perdu : détruit par l'ennemi, carré %2. Il faudra un nouveau kit » ; évènement depot_detruit
	//! — SRP_FrontEconomy.DestroyDepot
	static void DepotDestroyed(int zone, string cellRef)
	{
		Publish("depot_detruit", zone, string.Format("Dépôt de %1 perdu : détruit par l'ennemi, carré %2. Il faudra un nouveau kit", ZoneLabel(zone), cellRef));
	}

	//------------------------------------------------------------------------------------------------
	//! K1 : journal TERRITOIRE public « Nouvelle campagne… » avec le résumé de l'ancien territoire archivé —
	//! SRP_FrontComponent.ImportLegacy
	static void LegacyImported(string info)
	{
		string text = "Nouvelle campagne : le front par zones remplace les anciens secteurs";
		if (!info.IsEmpty())
			text += " (" + info + ")";
		JournalLine(text);
	}

	//------------------------------------------------------------------------------------------------
	//! K2 : « Versement des anciens secteurs : %1 vivres et %2 munitions rangés à la base » (journal RESSOURCE,
	//! NotifyOfficiers) — SRP_FrontComponent.MigrationTick
	static void LegacyPoured(int vivres, int munitions)
	{
		string text = string.Format("Versement des anciens secteurs : %1 vivres et %2 munitions rangés à la base", vivres, munitions);
		SRP_JournalComponent.Log("RESSOURCE", text);
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers(text);
	}

	//================================================================================================
	// Commandeur (#76) : les 8 annonces figées (VU1 à VU3, OF9, OF11, RE8, RE9, MO9, AP1, C3, CA5, C8)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! « Radio : l'officier ennemi %1 est tombé, région de %2. » ou « … a été capturé … » ; journal (+ « — zone » si
	//! zone >= 0) et évènement officier_tombe (radio seule si vu_officier_discord = 0) — SRP_CmdRegions
	static void OfficerFell(int region, string surname, bool captured, int zone)
	{
		if (!SRP_Commander.Get())
			return;
		string core = OfficerText(surname) + " est tombé, " + RegionLabel(region);
		if (captured)
			core = OfficerText(surname) + " a été capturé, " + RegionLabel(region);
		Say(core + ".");
		if (!SRP_CmdScreens.s_bOfficerDiscord)
			return;

		string entry = core;
		if (zone >= 0)
			entry += " — " + ZoneLabel(zone);
		entry += ".";
		JournalLine(entry);
		SendEvent("officier_tombe", zone, entry);
	}

	//------------------------------------------------------------------------------------------------
	//! OF11 : « Radio : toute la région de %1 est à nous : l'officier ennemi %2 a quitté l'île. » ; sans nom (officier
	//! tué ou capturé, plus d'officier en poste) : « Radio : toute la région de %1 est à nous. » seulement ; évènement
	//! region_liberee — SRP_CmdRegionBook.CheckLiberation (jamais pour une zone forcée par le Staff, Q9)
	static void RegionLiberated(int region, string surname)
	{
		if (!SRP_Commander.Get())
			return;
		string text = "toute la " + RegionLabel(region) + " est à nous";
		// Seul un officier vivant « quitte l'île » : mort ou capturé, il n'y a plus personne pour partir
		if (!surname.IsEmpty())
			text += " : " + OfficerText(surname) + " a quitté l'île";
		Publish("region_liberee", -1, text + ".");
	}

	//------------------------------------------------------------------------------------------------
	//! VU2, RE9 : « Radio : renseignement — … : la région de %1 est renseignée (PC, onglet Territoire). » selon la
	//! source (SRP_CmdIntel : PC, OFFICIER, MISSION), + « Dépôt ennemi repéré au carré %2. » ; évènement renseignement
	//! — SRP_CmdIntel
	static void IntelGained(int region, int source, string detail, bool depotRevealed, string depotRef, int zone)
	{
		if (!SRP_Commander.Get())
			return;
		string area = RegionLabel(region);
		string text;
		if (source == SRP_ECmdIntelSource.PC)
		{
			string where = detail;
			if (where.IsEmpty())
				where = ZoneLabel(zone);
			text = string.Format("renseignement — documents saisis au PC ennemi de %1 : la %2 est renseignée (PC, onglet Territoire).", where, area);
		}
		else if (source == SRP_ECmdIntelSource.OFFICIER)
		{
			text = string.Format("renseignement — l'interrogatoire de %1 renseigne la %2 (PC, onglet Territoire).", OfficerText(detail), area);
		}
		else if (source == SRP_ECmdIntelSource.MISSION)
		{
			string mission = "mission réussie";
			if (!detail.IsEmpty())
				mission = "mission " + detail + " réussie";
			text = string.Format("renseignement — %1 : la %2 est renseignée (PC, onglet Territoire).", mission, area);
		}
		else
		{
			text = string.Format("renseignement — la %1 est renseignée (PC, onglet Territoire).", area);
		}
		if (depotRevealed && !depotRef.IsEmpty())
			text += string.Format(" Dépôt ennemi repéré au carré %1.", depotRef);
		Publish("renseignement", zone, text);
	}

	//------------------------------------------------------------------------------------------------
	//! RE8 : « Radio : dépôt ennemi saboté, région de %1 : son ravitaillement est réduit de moitié pendant %2 h. » ;
	//! évènement depot_ennemi — SRP_CmdDepot
	static void EnemyDepotSabotaged(int region, int hours, int zone)
	{
		if (!SRP_Commander.Get())
			return;
		Publish("depot_ennemi", zone, string.Format("dépôt ennemi saboté, %1 : son ravitaillement est réduit de moitié pendant %2 h.", RegionLabel(region), hours));
	}

	//------------------------------------------------------------------------------------------------
	//! MO9, AP1, radio SEULE si vu_radio_silence = 1 : « Radio : pièce de mortier ennemie réduite au silence, région
	//! de %1. » ou « Radio : batterie d'artillerie ennemie détruite, région de %1. » — SRP_CmdSupport
	static void SupportSilenced(int region, bool battery)
	{
		if (!SRP_CmdScreens.s_bRadioSilence || !SRP_Commander.Get())
			return;
		if (battery)
			Say("batterie d'artillerie ennemie détruite, " + RegionLabel(region) + ".");
		else
			Say("pièce de mortier ennemie réduite au silence, " + RegionLabel(region) + ".");
	}

	//------------------------------------------------------------------------------------------------
	//! C3 : « Radio : blindé ennemi %1 ramené à la base — prime de %2 € versée au coffre. » ; évènement blinde_ramene
	//! (le versement reste à SRP_CmdArmor) — SRP_CmdArmor
	static void ArmorBroughtBack(string vehicleName, int reward)
	{
		Publish("blinde_ramene", -1, string.Format("blindé ennemi %1 ramené à la base — prime de %2 € versée au coffre.", vehicleName, reward));
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Sortie publique d'un fait de zone : NotifyAll("Radio : " + texte), Journal("TERRITOIRE", texte),
	//! PushZoneEvent(type, code de la zone ou "", texte)
	protected static void Publish(string type, int zone, string text)
	{
		Say(text);
		JournalLine(text);
		SendEvent(type, zone, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Radio seule : NotifyAll("Radio : " + texte)
	protected static void Say(string text)
	{
		SRP_Utils.NotifyAll("Radio : " + text);
	}

	//------------------------------------------------------------------------------------------------
	//! Journal public TERRITOIRE (sans « Radio : », première lettre en capitale)
	protected static void JournalLine(string text)
	{
		SRP_EnemyComponent.Journal("TERRITOIRE", Capital(text));
	}

	//------------------------------------------------------------------------------------------------
	//! Évènement du pont (liste unique, CONTRATS_FRONT §6) avec le code de la zone, "" sans zone
	protected static void SendEvent(string type, int zone, string text)
	{
		if (type.IsEmpty())
			return;
		SRP_BridgeComponent.PushZoneEvent(type, ZoneCode(zone), Capital(text));
	}

	//------------------------------------------------------------------------------------------------
	//! Nom de la ressource pour un texte (« vivres », « munitions »…, SRP_Resources.GetName)
	protected static string ResourceWord(int resource)
	{
		if (!SRP_Resources.IsValide(resource))
			return "ravitaillement";
		return SRP_Resources.GetName(resource);
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé affiché de la zone, « Régina (S07) » ; « la zone visée » si la zone est inconnue
	protected static string ZoneLabel(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0 || zone >= front.GetZoneCount())
			return "la zone visée";
		string label = front.GetZoneLabel(zone);
		if (label.IsEmpty())
			return "la zone visée";
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Code de la zone pour le pont (« S07 »), "" si la zone est inconnue
	protected static string ZoneCode(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone < 0 || zone >= front.GetZoneCount())
			return "";
		return front.GetZoneCode(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe » (SRP_Commander.GetRegionLabel) ; « région ennemie » à défaut
	protected static string RegionLabel(int region)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || region < 0)
			return "région ennemie";
		string label = commander.GetRegionLabel(region);
		if (label.IsEmpty())
			return "région ennemie";
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! « l'officier ennemi Volkov » (VU1), « l'officier ennemi » sans nom
	protected static string OfficerText(string surname)
	{
		if (surname.IsEmpty())
			return "l'officier ennemi";
		return "l'officier ennemi " + surname;
	}

	//------------------------------------------------------------------------------------------------
	//! Première lettre en capitale pour le journal (lettres a à z seulement : un caractère accentué reste tel quel)
	protected static string Capital(string text)
	{
		if (text.IsEmpty())
			return text;
		string first = text.Substring(0, 1);
		if (!"abcdefghijklmnopqrstuvwxyz".Contains(first))
			return text;
		string rest = text.Substring(1, text.Length() - 1);
		first.ToUpper();
		return first + rest;
	}

	//------------------------------------------------------------------------------------------------
	//! « 24 h », « 1 h 30 » ou « 12 min »
	protected static string DurationText(int seconds)
	{
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
	//! Nouvelle partie (Workbench) : la file et la mémoire de l'ancienne partie sont oubliées
	protected static void CheckSession(SRP_FrontComponent front)
	{
		if (front == s_Front)
			return;
		s_Front = front;
		DropQueue();
		s_aHalfSaid.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Vide la file des carrés (et la liste des pertes annoncées qui l'accompagne)
	protected static void DropQueue()
	{
		s_aCells.Clear();
		s_aOwners.Clear();
		s_aReasons.Clear();
		s_aLostSaid.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Causes écrites carré par carré au journal FRONT (les autres sont des repeintes d'un coup, une ligne par zone)
	protected static bool IsSingleCause(int reason)
	{
		return reason == SRP_EFrontReason.CAPTURE || reason == SRP_EFrontReason.SAISIE || reason == SRP_EFrontReason.REPRISE;
	}

	//------------------------------------------------------------------------------------------------
	//! La cause en clair, pour le journal FRONT
	protected static string ReasonWord(int reason)
	{
		switch (reason)
		{
			case SRP_EFrontReason.CAPTURE: return "capture";
			case SRP_EFrontReason.SAISIE: return "saisie du poste";
			case SRP_EFrontReason.REPRISE: return "reprise par l'ennemi";
			case SRP_EFrontReason.ZONE_TOMBEE: return "zone tombée";
			case SRP_EFrontReason.ZONE_PERDUE: return "zone perdue";
			case SRP_EFrontReason.ISOLEMENT: return "zone coupée de la base";
			case SRP_EFrontReason.OFFENSIVE: return "offensive de la nuit";
			case SRP_EFrontReason.STAFF: return "correction du Staff";
			case SRP_EFrontReason.REMISE: return "remise à plat";
			case SRP_EFrontReason.RESTAURATION: return "restauration";
		}
		return "cause inconnue";
	}

	//------------------------------------------------------------------------------------------------
	//! « au bleu » ou « au rouge »
	protected static string OwnerWord(int owner)
	{
		if (owner == SRP_EFrontOwner.BLEU)
			return "au bleu";
		return "au rouge";
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07), 7/25 carrés bleus » pour le journal FRONT
	protected static string ZoneShare(SRP_FrontComponent front, int zone)
	{
		if (zone < 0)
			return "hors zone";
		return string.Format("%1, %2/%3 carrés bleus", front.GetZoneLabel(zone), front.CountZoneBlue(zone), front.CountZoneCells(zone));
	}

	//------------------------------------------------------------------------------------------------
	//! Journal FRONT (Staff seulement, I7) : une ligne par carré pris, saisi ou repris ; une ligne par (zone, cause,
	//! sens) pour une repeinte d'un coup, avec la référence de chacun de ses carrés
	protected static void JournalCells(SRP_FrontComponent front)
	{
		array<int> groupZones = {};
		array<int> groupReasons = {};
		array<int> groupOwners = {};
		int count = s_aCells.Count();
		for (int i = 0; i < count; i++)
		{
			int cellZone = front.GetCellZone(s_aCells[i]);
			if (IsSingleCause(s_aReasons[i]))
			{
				SRP_EnemyComponent.Journal("FRONT", string.Format("Carré %1 passé %2 (%3) — %4", front.CellRef(s_aCells[i]), OwnerWord(s_aOwners[i]), ReasonWord(s_aReasons[i]), ZoneShare(front, cellZone)));
				continue;
			}
			bool known = false;
			for (int g = 0; g < groupZones.Count(); g++)
			{
				if (groupZones[g] == cellZone && groupReasons[g] == s_aReasons[i] && groupOwners[g] == s_aOwners[i])
				{
					known = true;
					break;
				}
			}
			if (!known)
			{
				groupZones.Insert(cellZone);
				groupReasons.Insert(s_aReasons[i]);
				groupOwners.Insert(s_aOwners[i]);
			}
		}

		for (int k = 0; k < groupZones.Count(); k++)
		{
			string refs = "";
			int cited = 0;
			for (int j = 0; j < count; j++)
			{
				if (s_aReasons[j] != groupReasons[k] || s_aOwners[j] != groupOwners[k])
					continue;
				if (front.GetCellZone(s_aCells[j]) != groupZones[k])
					continue;
				if (cited > 0)
					refs += ", ";
				refs += front.CellRef(s_aCells[j]);
				cited++;
			}
			SRP_EnemyComponent.Journal("FRONT", string.Format("%1 carré(s) passé(s) %2 (%3) — %4 : %5", cited, OwnerWord(groupOwners[k]), ReasonWord(groupReasons[k]), ZoneShare(front, groupZones[k]), refs));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Radio I5 : une ligne par carré pris ou repris au combat, regroupée au-delà de m_iRadioMergeAbove carrés d'une
	//! même zone et d'un même sens dans le passage
	protected static void RadioCells(SRP_FrontComponent front)
	{
		array<int> groupZones = {};
		array<int> groupOwners = {};
		int count = s_aCells.Count();
		for (int i = 0; i < count; i++)
		{
			if (s_aReasons[i] != SRP_EFrontReason.CAPTURE && s_aReasons[i] != SRP_EFrontReason.REPRISE)
				continue;
			int cellZone = front.GetCellZone(s_aCells[i]);
			if (cellZone < 0)
				continue;
			bool known = false;
			for (int g = 0; g < groupZones.Count(); g++)
			{
				if (groupZones[g] == cellZone && groupOwners[g] == s_aOwners[i])
				{
					known = true;
					break;
				}
			}
			if (!known)
			{
				groupZones.Insert(cellZone);
				groupOwners.Insert(s_aOwners[i]);
			}
		}

		int mergeAbove = front.m_iRadioMergeAbove;
		int maxRefs = Math.MaxInt(1, front.m_iRadioMaxRefs);
		for (int k = 0; k < groupZones.Count(); k++)
		{
			int groupZone = groupZones[k];
			bool taken = groupOwners[k] == SRP_EFrontOwner.BLEU;
			// La zone est tombée dans ce même passage : ZoneFell l'a déjà annoncée, pas de ligne par carré en plus
			if (taken && FellInQueue(front, groupZone))
				continue;
			// Zone dont la perte vient d'être annoncée (ZoneFell, même passage : ZoneFall.Tick passe avant Flush) : pas
			// de ligne par carré en plus. Jamais la couleur actuelle : une reprise peut toucher un carré bleu d'une zone
			// rouge (QG d'une ville posé chez un voisin rouge, ou m_bRetakeTargetZoneOnly décoché)
			if (!taken && s_aLostSaid.Find(groupZone) >= 0)
				continue;

			array<int> members = {};
			for (int j = 0; j < count; j++)
			{
				if (s_aReasons[j] != SRP_EFrontReason.CAPTURE && s_aReasons[j] != SRP_EFrontReason.REPRISE)
					continue;
				if (s_aOwners[j] != groupOwners[k] || front.GetCellZone(s_aCells[j]) != groupZone)
					continue;
				members.Insert(s_aCells[j]);
			}
			int n = members.Count();
			if (n == 0)
				continue;

			string label = front.GetZoneLabel(groupZone);
			int land = front.CountZoneCells(groupZone);
			int blue = front.CountZoneBlue(groupZone);
			if (n == 1 || n <= mergeAbove)
			{
				for (int m = 0; m < n; m++)
				{
					// Part bleue au moment de ce carré : la part finale moins (ou plus) les carrés annoncés après lui
					int shown = blue + (n - 1 - m);
					if (taken)
						shown = blue - (n - 1 - m);
					shown = Math.ClampInt(shown, 0, land);
					if (taken)
						SRP_Utils.NotifyAll(string.Format("Radio : carré %1 pris — %2, %3/%4 carrés à nous", front.CellRef(members[m]), label, shown, land));
					else
						SRP_Utils.NotifyAll(string.Format("Radio : carré %1 perdu, repris par l'ennemi — %2, %3/%4 carrés à nous", front.CellRef(members[m]), label, shown, land));
				}
				continue;
			}

			string refs = "";
			int cited = Math.MinInt(n, maxRefs);
			for (int r = 0; r < cited; r++)
			{
				if (r > 0)
					refs += ", ";
				refs += front.CellRef(members[r]);
			}
			if (n > cited)
				refs += " …";
			if (taken)
				SRP_Utils.NotifyAll(string.Format("Radio : %1 carrés pris — %2 : %3", n, label, refs));
			else
				SRP_Utils.NotifyAll(string.Format("Radio : %1 carrés perdus, repris par l'ennemi — %2 : %3", n, label, refs));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La file contient-elle la repeinte d'une chute (ZONE_TOMBEE) de cette zone ?
	protected static bool FellInQueue(SRP_FrontComponent front, int zone)
	{
		int count = s_aCells.Count();
		for (int i = 0; i < count; i++)
		{
			if (s_aReasons[i] == SRP_EFrontReason.ZONE_TOMBEE && front.GetCellZone(s_aCells[i]) == zone)
				return true;
		}
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Repères de carte des points clés (I3) et des dépôts ennemis révélés (RE9), serveur. Remplace PlaceMarker et
//! UpdateBlink de SRP_Territory.c:2072-2163. Les repères des dépôts de ZONE restent à SRP_FrontEconomy.
class SRP_FrontMarkers
{
	protected static ref array<ref SCR_MapMarkerBase> s_aMarkers = {};		// un par point clé du registre
	protected static ref array<int> s_aSig = {};							// signature du dernier état posé
	protected static ref array<ref SCR_MapMarkerBase> s_aDepotMarkers = {};	// dépôts ennemis révélés
	protected static ref array<int> s_aDepotRegions = {};
	protected static ref array<vector> s_aDepotPos = {};					// position posée (un dépôt déplacé est reposé)
	// Socle de la partie en cours : les repères d'une autre partie (Workbench) sont oubliés sans y toucher
	protected static SRP_FrontComponent s_Front;

	//------------------------------------------------------------------------------------------------
	//! Fin de chaque boucle de 5 s (5e) : pour chaque point clé, visible = m_bKeyPointMarkers et zone hors base et
	//! (à nous, au front, attaquée, ou !m_bHideRearKeyPoints) ; signature propriétaire + 2 x front + 4 x attaque + 16 x
	//! point pris + 32 x visible ; si elle change : retrait puis repose (« PC ennemi S08 », « QG ennemi S08 »,
	//! « Point clé S07 », « ATTAQUE ANNONCÉE S07 », « ATTAQUE S07 ») ; puis TickEnemyDepots — SRP_FrontComponent.Tick
	static void Tick()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;
		CheckSession(front);
		if (!SCR_MapMarkerManagerComponent.GetInstance())
			return;

		int total = front.GetKeyPointTotal();
		if (s_aSig.Count() != total)
		{
			// Registre changé (démarrage) : tout est reposé
			RemoveKeyPointMarkers();
			for (int i = 0; i < total; i++)
			{
				s_aMarkers.Insert(null);
				s_aSig.Insert(-1);
			}
		}

		for (int k = 0; k < total; k++)
		{
			SRP_FrontKeyPoint point = front.GetKeyPoint(k);
			if (!point)
				continue;

			int owner = 0;
			int nearFront = 0;
			int attack = SRP_EFrontAttack.AUCUNE;
			int taken = 0;
			bool visible = false;
			SRP_FrontZone zone = front.GetZone(point.m_iZone);
			if (zone && !zone.m_bBase)
			{
				owner = front.GetZoneOwner(point.m_iZone);
				if (zone.IsNearFront())
					nearFront = 1;
				attack = front.GetZoneAttackState(point.m_iZone);
				if (front.GetCellOwner(point.m_iCell) == SRP_EFrontOwner.BLEU)
					taken = 1;
				// I3 : les points ennemis de l'arrière restent cachés (m_bHideRearKeyPoints)
				visible = front.m_bKeyPointMarkers && (owner == SRP_EFrontOwner.BLEU || nearFront == 1 || attack != SRP_EFrontAttack.AUCUNE || !front.m_bHideRearKeyPoints);
			}
			int visibleBit = 0;
			if (visible)
				visibleBit = 1;
			int sig = owner + 2 * nearFront + 4 * attack + 16 * taken + 32 * visibleBit;
			if (sig == s_aSig[k])
				continue;

			// L'état a changé : retrait, puis repose si le point est visible (jamais de clignotement)
			RemoveMarker(s_aMarkers[k]);
			s_aMarkers.Set(k, null);
			s_aSig.Set(k, sig);
			if (!visible)
				continue;

			string code = front.GetZoneCode(point.m_iZone);
			int icon = front.m_iKeyPointIcon;
			int color = front.m_iKeyPointColorEnemy;
			string text = "PC ennemi " + code;
			if (attack == SRP_EFrontAttack.ANNONCEE)
			{
				icon = front.m_iAttackWarnIcon;
				color = front.m_iAttackWarnColor;
				text = "ATTAQUE ANNONCÉE " + code;
			}
			else if (attack == SRP_EFrontAttack.ASSAUT)
			{
				icon = front.m_iAttackIcon;
				color = front.m_iAttackColor;
				text = "ATTAQUE " + code;
			}
			else if (taken == 1)
			{
				color = front.m_iKeyPointColorOurs;
				text = "Point clé " + code;
			}
			else if (point.m_iRole == SRP_EKeyPointRole.QG)
			{
				text = "QG ennemi " + code;
			}

			SCR_MapMarkerBase marker = PlaceMarker(point.m_vPos, icon, color, text);
			s_aMarkers.Set(k, marker);
			if (!marker)
				s_aSig.Set(k, -1);		// nouvel essai au passage suivant
		}

		TickEnemyDepots();
	}

	//------------------------------------------------------------------------------------------------
	//! RE9 : un repère par dépôt ennemi révélé (SRP_CmdResources, dépôts révélés), retiré quand il ne l'est plus
	//! (vu_depot_icone, vu_depot_couleur) — Tick
	static void TickEnemyDepots()
	{
		if (!SCR_MapMarkerManagerComponent.GetInstance())
			return;

		array<int> regions = {};
		array<vector> positions = {};
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources)
		{
			SRP_CmdDepots depots = resources.GetDepots();
			if (depots)
				depots.GetRevealedDepots(regions, positions);
		}

		// Retrait : dépôt qui n'est plus révélé, ou qui a changé de place
		for (int i = s_aDepotRegions.Count() - 1; i >= 0; i--)
		{
			bool keep = false;
			for (int j = 0; j < regions.Count(); j++)
			{
				if (regions[j] == s_aDepotRegions[i] && vector.DistanceXZ(positions[j], s_aDepotPos[i]) < 1)
				{
					keep = true;
					break;
				}
			}
			if (keep)
				continue;
			RemoveMarker(s_aDepotMarkers[i]);
			s_aDepotMarkers.RemoveOrdered(i);
			s_aDepotRegions.RemoveOrdered(i);
			s_aDepotPos.RemoveOrdered(i);
		}

		// Pose : dépôt révélé et posé qui n'a pas encore de repère
		for (int k = 0; k < regions.Count(); k++)
		{
			if (positions[k] == vector.Zero)
				continue;
			if (s_aDepotRegions.Find(regions[k]) >= 0)
				continue;
			SCR_MapMarkerBase marker = PlaceMarker(positions[k], SRP_CmdScreens.s_iDepotIcon, SRP_CmdScreens.s_iDepotColor, "Dépôt ennemi — " + DepotRegionLabel(regions[k]));
			if (!marker)
				continue;
			s_aDepotMarkers.Insert(marker);
			s_aDepotRegions.Insert(regions[k]);
			s_aDepotPos.Insert(positions[k]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Retire tous les repères (remise à zéro, restauration) ; ils reviennent au passage suivant — socle (NotifyReset)
	static void Clear()
	{
		// Peut venir avant le premier Tick d'une nouvelle partie (Start, victoire enregistrée) : les repères de
		// l'ancienne partie sont oubliés sans être retirés
		CheckSession(SRP_FrontComponent.GetInstance());
		RemoveKeyPointMarkers();
		foreach (SCR_MapMarkerBase depotMarker : s_aDepotMarkers)
		{
			RemoveMarker(depotMarker);
		}
		s_aDepotMarkers.Clear();
		s_aDepotRegions.Clear();
		s_aDepotPos.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Pose un repère PLACED_CUSTOM (SetWorldPos, SetIconEntry, SetColorEntry, SetCustomText) par
	//! SCR_MapMarkerManagerComponent.InsertStaticMarker(marker, false, true) ; null en cas d'échec
	protected static SCR_MapMarkerBase PlaceMarker(vector position, int icon, int color, string text)
	{
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markers)
			return null;

		SCR_MapMarkerBase marker = new SCR_MapMarkerBase();
		marker.SetType(SCR_EMapMarkerType.PLACED_CUSTOM);
		int x = position[0];
		int z = position[2];
		marker.SetWorldPos(x, z);
		marker.SetIconEntry(icon);
		marker.SetColorEntry(color);
		marker.SetCustomText(text);
		markers.InsertStaticMarker(marker, false, true);
		return marker;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire un repère (RemoveStaticMarker)
	protected static void RemoveMarker(SCR_MapMarkerBase marker)
	{
		if (!marker)
			return;
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (markers)
			markers.RemoveStaticMarker(marker);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire les repères des points clés et oublie leurs signatures
	protected static void RemoveKeyPointMarkers()
	{
		foreach (SCR_MapMarkerBase marker : s_aMarkers)
		{
			RemoveMarker(marker);
		}
		s_aMarkers.Clear();
		s_aSig.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe » pour le repère d'un dépôt ennemi, « région ennemie » à défaut
	protected static string DepotRegionLabel(int region)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return "région ennemie";
		string label = commander.GetRegionLabel(region);
		if (label.IsEmpty())
			return "région ennemie";
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle partie (Workbench) : les repères de l'ancienne appartenaient à l'ancien gestionnaire, on les oublie
	//! sans les retirer (leurs numéros pourraient désigner des repères neufs)
	protected static void CheckSession(SRP_FrontComponent front)
	{
		if (front == s_Front)
			return;
		s_Front = front;
		s_aMarkers.Clear();
		s_aSig.Clear();
		s_aDepotMarkers.Clear();
		s_aDepotRegions.Clear();
		s_aDepotPos.Clear();
	}
}
