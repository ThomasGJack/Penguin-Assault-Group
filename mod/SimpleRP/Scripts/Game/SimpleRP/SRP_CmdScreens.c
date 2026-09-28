//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : JOURNAL, RENSEIGNEMENT ET ÉCRANS (contrats figés du 26/09, corps codés).
//
// RÔLE (serveur ; trois classes statiques, aucune entité, aucune RPC) :
// - SRP_CmdLog : le SEUL journal des décisions (VU4). vu_journal_memoire lignes en mémoire (200) pour le menu Staff ;
//   chaque entrée (sauf les refus, qui ne partent qu'avec vu_refus_discord) est écrite par
//   SRP_EnemyComponent.Journal("COMMANDEUR", …) : fichier du mois, console, flux Discord « etat-major » (Staff seul,
//   secours journal-serveur). La catégorie COMMANDEUR n'est JAMAIS publique ni dans l'onglet Journal du PC. Jamais
//   Log("STAFF"), Log("ENNEMI") ni PushEvent. Rien n'est sauvé : le menu repart vide au redémarrage.
// - SRP_CmdIntel : ce que les JOUEURS savent (VU2, RE9), par RÉGION : saisie D4 (avant le changement de camp),
//   officier capturé, mission réussie sur un carré rouge (vu_renseignement_missions) ; le dépôt est révélé pour un PC
//   saisi, un officier capturé ou une mission de vu_depot_missions. Le renseignement tombe de lui-même à l'arrivée
//   d'un nouvel officier (numéro unique) ou après vu_renseignement_heures (0 = jamais). Annonce :
//   SRP_FrontRadio.IntelGained (évènement renseignement). Sauvé : clés cmd_vu_* (par code de région).
// - SRP_CmdScreens : 8e onglet Staff « Commandeur » (page principale, région, journal, forcer, cible, ressources ;
//   actions par RunStaff, SRP_Admin ne fait que déléguer), morceaux de l'en-tête, de l'onglet Territoire, du PC
//   (suffixe de zone « · région de X », section Renseignement) et du rapport « autour de moi ». Protocole de menus
//   SRP_Admin.c:17-19 (M|, H|, L|, C|teinte|, K|teinte|clé|, I|clé|) ; chaque page commence par un saut de ligne
//   (les lignes vides sont ignorées par le menu) ; la navigation pure passe par « page:commandeur:… » ; textes libres
//   par Esc ; aucun signe pour cent ; jamais les mots IA, automatique, garnison, groupe, patrouille dans un texte vu
//   des joueurs.
// Réglages vu_* : champs statiques de SRP_CmdScreens, lus par LoadSettings (SRP_FrontRadio lit s_bRadioSilence,
// s_bOfficerDiscord, s_iDepotIcon, s_iDepotColor).
// APPELÉ PAR : tout le Commandeur (SRP_CmdLog), SRP_CmdSettings (Note), SRP_FrontEnemyComponent.OnKeyPointSeized,
// SRP_CmdRegions (OnOfficerCaptured), SRP_Commander.OnMissionSucceeded, SRP_AdminComponent (onglet, en-tête, rapide),
// SRP_FrontScreens (PCZoneSuffix, PCIntelSection, StaffTerritoryLine).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Une ligne du journal des décisions (mémoire seulement)
class SRP_CmdLogEntry
{
	int m_iUnix;
	string m_sHour;							// « 21:14 »
	int m_iRegion = -1;						// -1 = toute l'île
	int m_iKind = SRP_ECmdLogKind.DECISION;
	string m_sText;
}

//------------------------------------------------------------------------------------------------
//! Le journal des décisions du Commandeur (VU4), statique.
class SRP_CmdLog
{
	protected static ref array<ref SRP_CmdLogEntry> s_aEntries = {};		// la plus ancienne d'abord
	protected static ref array<string> s_aRefusalKeys = {};				// refus déjà écrits (vu_refus_repos_min)
	protected static ref array<int> s_aRefusalUnix = {};

	static const int REFUSAL_KEYS_MAX = 400;		// clés de refus retenues au plus (les plus anciennes tombent)

	//------------------------------------------------------------------------------------------------
	//! Une décision prise : « what — reason — coût : cost — reste : left » ; APPUI si heavy (artillerie, bombe, blindé),
	//! sinon DECISION ; mémoire + Journal("COMMANDEUR", « [région] … ») — tout le Commandeur, pour CHAQUE décision
	static void Decision(int region, bool heavy, string what, string reason, string cost, string left)
	{
		// Un morceau vide est omis (pas de « coût : » orphelin)
		string text = what;
		if (!reason.IsEmpty())
			text += " — " + reason;
		if (!cost.IsEmpty())
			text += " — coût : " + cost;
		if (!left.IsEmpty())
			text += " — reste : " + left;
		int kind = SRP_ECmdLogKind.DECISION;
		if (heavy)
			kind = SRP_ECmdLogKind.APPUI;
		Remember(region, kind, text);
		Write(region, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Une décision envisagée puis écartée : « écarté : what — why » ; mémoire (vu_refus_menu) ; Discord seulement si
	//! vu_refus_discord ; une même clé what + why n'est réécrite qu'après vu_refus_repos_min — tout le Commandeur
	static void Refusal(int region, string what, string why)
	{
		string key = what + " — " + why;
		int now = System.GetUnixTime();
		int repeat = SRP_CmdScreens.s_iRefusalRepeatMin * 60;
		if (repeat > 0)
		{
			// Clés échues oubliées : elles sont rangées dans l'ordre d'écriture, la plus ancienne d'abord
			while (!s_aRefusalUnix.IsEmpty() && now - s_aRefusalUnix[0] >= repeat)
			{
				s_aRefusalKeys.RemoveOrdered(0);
				s_aRefusalUnix.RemoveOrdered(0);
			}
			if (s_aRefusalKeys.Find(key) >= 0)
				return;
			s_aRefusalKeys.Insert(key);
			s_aRefusalUnix.Insert(now);
			while (s_aRefusalKeys.Count() > REFUSAL_KEYS_MAX)
			{
				s_aRefusalKeys.RemoveOrdered(0);
				s_aRefusalUnix.RemoveOrdered(0);
			}
		}

		string text = "écarté : " + what;
		if (!why.IsEmpty())
			text += " — " + why;
		if (SRP_CmdScreens.s_bRefusalsInMenu)
			Remember(region, SRP_ECmdLogKind.REFUS, text);
		if (SRP_CmdScreens.s_bRefusalsToDiscord)
			Write(region, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Un fait (officier, stock, dépôt, renseignement, système) de nature kind (SRP_ECmdLogKind) — tout le Commandeur,
	//! SRP_CmdSettings
	static void Note(int region, int kind, string text)
	{
		Remember(region, kind, text);
		Write(region, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Action du Staff : « [Staff] Jack : Commandeur gelé » — SRP_CmdScreens.RunStaff, sous-modules (Staff*)
	static void StaffAction(string author, string text)
	{
		string line = "[Staff] " + author + " : " + text;
		Remember(-1, SRP_ECmdLogKind.STAFF, line);
		Write(-1, line);
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre d'entrées d'une région (-1 = toutes), refus compris ou non — SRP_CmdScreens (pagination)
	static int CountEntries(int region, bool withRefusals)
	{
		int count = 0;
		foreach (SRP_CmdLogEntry entry : s_aEntries)
		{
			if (Matches(entry, region, withRefusals))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Entrées filtrées, la plus récente d'abord, à partir de first (count au plus) — SRP_CmdScreens
	static void GetEntries(int region, bool withRefusals, int first, int count, notnull array<ref SRP_CmdLogEntry> result)
	{
		result.Clear();
		if (count <= 0)
			return;
		int skipped = 0;
		for (int i = s_aEntries.Count() - 1; i >= 0; i--)
		{
			SRP_CmdLogEntry entry = s_aEntries[i];
			if (!Matches(entry, region, withRefusals))
				continue;
			if (skipped < first)
			{
				skipped++;
				continue;
			}
			result.Insert(entry);
			if (result.Count() >= count)
				return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Dernière entrée hors refus, null sinon — SRP_CmdScreens (« Dernière décision »)
	static SRP_CmdLogEntry GetLast()
	{
		for (int i = s_aEntries.Count() - 1; i >= 0; i--)
		{
			SRP_CmdLogEntry entry = s_aEntries[i];
			if (entry && entry.m_iKind != SRP_ECmdLogKind.REFUS)
				return entry;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Tout effacer (nouvelle campagne, construction du Commandeur : les statiques survivent aux parties du Workbench)
	//! — SRP_Commander
	static void Clear()
	{
		s_aEntries.Clear();
		s_aRefusalKeys.Clear();
		s_aRefusalUnix.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! L'entrée passe-t-elle le filtre (région, refus) ? — CountEntries, GetEntries
	protected static bool Matches(SRP_CmdLogEntry entry, int region, bool withRefusals)
	{
		if (!entry)
			return false;
		if (!withRefusals && entry.m_iKind == SRP_ECmdLogKind.REFUS)
			return false;
		return region < 0 || entry.m_iRegion == region;
	}

	//------------------------------------------------------------------------------------------------
	//! Range une ligne en mémoire (vu_journal_memoire lignes au plus, les plus anciennes tombent) — Decision, Refusal,
	//! Note, StaffAction
	protected static void Remember(int region, int kind, string text)
	{
		SRP_CmdLogEntry entry = new SRP_CmdLogEntry();
		entry.m_iUnix = System.GetUnixTime();
		entry.m_sHour = HourNow();
		entry.m_iRegion = region;
		entry.m_iKind = kind;
		entry.m_sText = text;
		s_aEntries.Insert(entry);

		int keep = SRP_CmdScreens.s_iLogMemory;
		if (keep < 1)
			keep = 1;
		while (s_aEntries.Count() > keep)
			s_aEntries.RemoveOrdered(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Journal du serveur, catégorie COMMANDEUR (fichier du mois, console, flux Discord etat-major du Staff) — Decision,
	//! Refusal (vu_refus_discord), Note, StaffAction
	protected static void Write(int region, string text)
	{
		string line = "[" + RegionTag(region) + "] " + text;
		// Aide aux essais seulement (pas une censure) : ce salon est lu sur Discord, les mots confidentiels n'y ont pas
		// leur place
		if (SRP_EnemyComponent.IsTestMode())
			CheckWords(line);
		SRP_EnemyComponent.Journal("COMMANDEUR", line);
	}

	//------------------------------------------------------------------------------------------------
	//! « Saint-Philippe », « Île » pour toute l'île — Write
	protected static string RegionTag(int region)
	{
		if (region < 0)
			return "Île";
		SRP_Commander commander = SRP_Commander.Get();
		if (commander && commander.GetBook())
		{
			string name = commander.GetBook().GetRegionName(region);
			if (!name.IsEmpty())
				return name;
			string code = commander.GetBook().GetRegionCode(region);
			if (!code.IsEmpty())
				return code;
		}
		int number = region + 1;
		return "région " + number.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Mode essai : avertit à la console si un mot confidentiel passe dans le journal du Commandeur — Write
	protected static void CheckWords(string text)
	{
		string lower = " " + text + " ";
		lower.ToLower();
		array<string> marks = {".", ",", ";", ":", "(", ")", "[", "]", "/", "'", "«", "»", "—"};
		foreach (string mark : marks)
			lower.Replace(mark, " ");
		array<string> words = {"garnison", "groupe", "patrouille", "automatique", " ia "};
		foreach (string word : words)
		{
			if (lower.Contains(word))
			{
				Print("[SRP] Commandeur : mot confidentiel « " + word.Trim() + " » dans le journal : " + text, LogLevel.WARNING);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! « 21:14 », heure locale du serveur — Remember
	protected static string HourNow()
	{
		int hour;
		int minute;
		int second;
		System.GetHourMinuteSecond(hour, minute, second);
		return SRP_Time.Pad2(hour) + ":" + SRP_Time.Pad2(minute);
	}
}

//------------------------------------------------------------------------------------------------
//! Le renseignement des joueurs, par région (VU2, RE9), statique. État sauvé par SRP_Commander.WriteTo (cmd_vu_*).
class SRP_CmdIntel
{
	protected static ref array<string> s_aCodes = {};		// code de chaque région
	protected static ref array<int> s_aSince = {};			// heure Unix du renseignement (0 = aucun)
	protected static ref array<int> s_aSource = {};		// SRP_ECmdIntelSource
	protected static ref array<string> s_aDetail = {};		// « Perelle (S08) », « Volkov », « M07 »
	protected static ref array<int> s_aSerial = {};		// numéro de l'officier en poste à ce moment

	//------------------------------------------------------------------------------------------------
	//! Dimensionne l'état aux régions tracées, tout effacé — SRP_Commander.BuildRegions, ResetCampaign
	static void Reset(int regions)
	{
		s_aCodes.Clear();
		s_aSince.Clear();
		s_aSource.Clear();
		s_aDetail.Clear();
		s_aSerial.Clear();
		for (int i = 0; i < regions; i++)
			AddSlot(i);
	}

	//------------------------------------------------------------------------------------------------
	//! D4 réussie, AVANT le changement de camp : Grant(région de la zone, PC, libellé de la zone, dépôt révélé) ; un 2e
	//! appel rafraîchit l'heure — SRP_FrontEnemyComponent.OnKeyPointSeized
	static void OnKeyPointSeized(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdRegionBook book = Book();
		if (!front || !book)
			return;
		int region = book.GetRegionOfZone(zone);
		if (region < 0)
			return;
		Grant(region, SRP_ECmdIntelSource.PC, front.GetZoneLabel(zone), true, zone);
	}

	//------------------------------------------------------------------------------------------------
	//! OF9 : Grant(région, OFFICIER, nom de l'officier, dépôt révélé) — SRP_CmdRegionBook.OfficerDown (capturé)
	static void OnOfficerCaptured(int region)
	{
		SRP_CmdRegionBook book = Book();
		if (!book || region < 0 || region >= book.GetRegionCount())
			return;
		Grant(region, SRP_ECmdIntelSource.OFFICIER, book.GetOfficerName(region), true, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Mission réussie : rien si le site n'est pas sur un carré ROUGE ; type (EnumToString de SRP_EMissionType) dans
	//! vu_renseignement_missions -> Grant(région, MISSION, missionId, dépôt si le type est dans vu_depot_missions) —
	//! SRP_Commander.OnMissionSucceeded
	static void OnMissionSucceeded(int missionType, vector site, string missionId)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdRegionBook book = Book();
		if (!front || !book || !front.IsReady())
			return;
		int cell = front.CellIndexAt(site);
		if (cell < 0 || front.GetCellOwner(cell) != SRP_EFrontOwner.ROUGE)
			return;
		int zone = front.GetCellZone(cell);
		int region = book.GetRegionOfZone(zone);
		if (region < 0)
			return;
		string typeName = typename.EnumToString(SRP_EMissionType, missionType);
		if (!ListHas(SRP_CmdScreens.s_sIntelMissions, typeName))
			return;
		bool depot = ListHas(SRP_CmdScreens.s_sDepotMissions, typeName);
		Grant(region, SRP_ECmdIntelSource.MISSION, missionId, depot, zone);
	}

	//------------------------------------------------------------------------------------------------
	//! La région est renseignée : noté ET (numéro d'officier noté == officier en poste, ou région sans officier) ET
	//! (vu_renseignement_heures = 0 ou pas encore échu) — SRP_CmdScreens (PC), carte
	static bool HasIntel(int region)
	{
		if (region < 0 || region >= s_aSince.Count())
			return false;
		int since = s_aSince[region];
		if (since <= 0)
			return false;
		SRP_CmdRegionBook book = Book();
		if (book)
		{
			// Un nouvel officier (autre numéro) fait tomber le renseignement ; sans officier, il reste
			int current = book.GetOfficerSerial(region);
			if (current != 0 && current != s_aSerial[region])
				return false;
		}
		int hours = SRP_CmdScreens.s_iIntelHours;
		if (hours > 0 && System.GetUnixTime() >= since + hours * 3600)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Heure Unix du renseignement, 0 sans — SRP_CmdScreens
	static int GetIntelSince(int region)
	{
		if (!HasIntel(region))
			return 0;
		return s_aSince[region];
	}

	//------------------------------------------------------------------------------------------------
	//! « PC ennemi saisi à Perelle (S08) », « officier ennemi Volkov capturé », « mission M07 réussie », "" sans —
	//! SRP_CmdScreens
	static string GetIntelSourceText(int region)
	{
		if (!HasIntel(region))
			return "";
		return SourceText(s_aSource[region], s_aDetail[region]);
	}

	//------------------------------------------------------------------------------------------------
	//! cmd_vu_n puis cmd_vu_rN_code, _since, _source, _detail, _serial — SRP_Commander.WriteTo
	static void WriteTo(JsonSaveContext ctx)
	{
		int count = s_aCodes.Count();
		ctx.WriteValue("cmd_vu_n", count);
		for (int i = 0; i < count; i++)
		{
			string prefix = string.Format("cmd_vu_r%1_", i);
			ctx.WriteValue(prefix + "code", s_aCodes[i]);
			ctx.WriteValue(prefix + "since", s_aSince[i]);
			ctx.WriteValue(prefix + "source", s_aSource[i]);
			ctx.WriteValue(prefix + "detail", s_aDetail[i]);
			ctx.WriteValue(prefix + "serial", s_aSerial[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture par code de région — SRP_Commander.ReadFrom
	static void ReadFrom(JsonLoadContext ctx)
	{
		int count = 0;
		if (!ctx.ReadValue("cmd_vu_n", count))
			return;
		for (int i = 0; i < count; i++)
		{
			string prefix = string.Format("cmd_vu_r%1_", i);
			string code = "";
			if (!ctx.ReadValue(prefix + "code", code) || code.IsEmpty())
				continue;
			int region = FindSlot(code);
			if (region < 0)
			{
				SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Renseignement relu pour une région qui n'existe plus (" + code + ") : ignoré");
				continue;
			}
			int since = 0;
			int source = SRP_ECmdIntelSource.AUCUNE;
			string detail = "";
			int serial = 0;
			ctx.ReadValue(prefix + "since", since);
			ctx.ReadValue(prefix + "source", source);
			ctx.ReadValue(prefix + "detail", detail);
			ctx.ReadValue(prefix + "serial", serial);
			s_aSince[region] = since;
			s_aSource[region] = source;
			s_aDetail[region] = detail;
			s_aSerial[region] = serial;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Note le renseignement, révèle le dépôt si demandé (SRP_CmdResources.Get().GetDepots().Reveal), SRP_CmdLog
	//! (RENSEIGNEMENT), puis SRP_FrontRadio.IntelGained(région, source, détail, dépôt révélé, carré, zone),
	//! SRP_FrontComponent.MarkDirty(false) — OnKeyPointSeized, OnOfficerCaptured, OnMissionSucceeded
	protected static void Grant(int region, int source, string detail, bool revealDepot, int zone)
	{
		if (region < 0)
			return;
		EnsureSlots(region);
		if (region >= s_aCodes.Count())
			return;

		SRP_CmdRegionBook book = Book();
		int serial = 0;
		if (book)
			serial = book.GetOfficerSerial(region);

		// Même fait déjà connu (2e point clé de la même zone) : l'heure est rafraîchie, sans nouvelle annonce
		bool refresh = HasIntel(region) && s_aSource[region] == source && s_aDetail[region] == detail;
		s_aSince[region] = System.GetUnixTime();
		s_aSource[region] = source;
		s_aDetail[region] = detail;
		s_aSerial[region] = serial;

		string sourceText = SourceText(source, detail);
		string depotRef = "";
		if (revealDepot)
		{
			SRP_CmdResources resources = SRP_CmdResources.Get();
			if (resources && resources.GetDepots())
				depotRef = resources.GetDepots().Reveal(region, sourceText);
		}
		bool depotRevealed = !depotRef.IsEmpty();

		if (refresh)
		{
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.RENSEIGNEMENT, "Renseignement des joueurs rafraîchi : " + sourceText);
		}
		else
		{
			string note = "Renseignement des joueurs : " + sourceText;
			if (depotRevealed)
				note += " · dépôt révélé au carré " + depotRef;
			else if (revealDepot)
				note += " · dépôt pas encore posé : il sera révélé à sa pose";
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.RENSEIGNEMENT, note);
			SRP_FrontRadio.IntelGained(region, source, detail, depotRevealed, depotRef, zone);
		}

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Texte d'une source : « PC ennemi saisi à Perelle (S08) », « officier ennemi Volkov capturé », « mission M07
	//! réussie » — Grant, GetIntelSourceText
	protected static string SourceText(int source, string detail)
	{
		if (source == SRP_ECmdIntelSource.PC)
			return "PC ennemi saisi à " + detail;
		if (source == SRP_ECmdIntelSource.OFFICIER)
			return SRP_CmdScreens.OfficerLabel(detail) + " capturé";
		if (source == SRP_ECmdIntelSource.MISSION)
			return "mission " + detail + " réussie";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Le registre des régions, null sans Commandeur
	protected static SRP_CmdRegionBook Book()
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return null;
		return commander.GetBook();
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute la case de la région index (code du tracé, sinon « R<n> ») — Reset, EnsureSlots
	protected static void AddSlot(int index)
	{
		int number = index + 1;
		string code = "R" + number.ToString();
		SRP_CmdRegionBook book = Book();
		if (book)
		{
			string traced = book.GetRegionCode(index);
			if (!traced.IsEmpty())
				code = traced;
		}
		s_aCodes.Insert(code);
		s_aSince.Insert(0);
		s_aSource.Insert(SRP_ECmdIntelSource.AUCUNE);
		s_aDetail.Insert("");
		s_aSerial.Insert(0);
	}

	//------------------------------------------------------------------------------------------------
	//! Cases ajoutées jusqu'à la région donnée si le tracé en compte autant (Reset manqué) — Grant, FindSlot
	protected static void EnsureSlots(int region)
	{
		SRP_CmdRegionBook book = Book();
		if (!book || region >= book.GetRegionCount())
			return;
		while (s_aCodes.Count() <= region)
			AddSlot(s_aCodes.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Case d'un code de région (casse ignorée), -1 sinon — ReadFrom
	protected static int FindSlot(string code)
	{
		string wanted = code;
		wanted.ToUpper();
		for (int i = 0; i < s_aCodes.Count(); i++)
		{
			string known = s_aCodes[i];
			known.ToUpper();
			if (known == wanted)
				return i;
		}
		SRP_CmdRegionBook book = Book();
		if (!book)
			return -1;
		int region = book.FindRegion(code);
		if (region < 0)
			return -1;
		EnsureSlots(region);
		if (region >= s_aCodes.Count())
			return -1;
		return region;
	}

	//------------------------------------------------------------------------------------------------
	//! La liste « a, b, c » (point-virgule accepté) contient-elle le mot (casse ignorée) ? « toutes » vaut tout —
	//! OnMissionSucceeded
	protected static bool ListHas(string listText, string word)
	{
		string wanted = word;
		wanted.ToUpper();
		string flat = listText;
		flat.Replace(";", ",");
		array<string> items = {};
		flat.Split(",", items, true);
		foreach (string item : items)
		{
			string clean = item.Trim();
			clean.ToUpper();
			if (clean == "TOUTES" || clean == "TOUS" || clean == wanted)
				return true;
		}
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Les écrans du Commandeur (Staff et PC), statique ; serveur.
class SRP_CmdScreens
{
	// --- Réglages (clés vu_*), champs statiques publics ----------------------------------------------------------
	static int s_iLogMemory = 200;		// vu_journal_memoire
	static int s_iLogPerPage = 15;		// vu_journal_par_page
	static bool s_bRefusalsInMenu = true;		// vu_refus_menu
	static bool s_bRefusalsToDiscord = false;		// vu_refus_discord
	static int s_iRefusalRepeatMin = 10;		// vu_refus_repos_min
	static int s_iIntelHours = 0;		// vu_renseignement_heures
	static string s_sIntelMissions = "toutes";		// vu_renseignement_missions
	static string s_sDepotMissions = "ECOUTE, DOCUMENTS";		// vu_depot_missions
	static int s_iTroopsStrongCent = 110;		// vu_troupes_renforcees
	static int s_iTroopsWeakCent = 70;		// vu_troupes_affaiblies
	static bool s_bRadioSilence = true;		// vu_radio_silence
	static bool s_bOfficerDiscord = true;		// vu_officier_discord
	static int s_iDepotIcon = 31;		// vu_depot_icone
	static int s_iDepotColor = 4;		// vu_depot_couleur
	static int s_iNearM = 3000;		// vu_proximite_m

	static const int PROBLEMS_SHOWN = 5;		// problèmes de réglages montrés sur la page principale

	//================================================================================================
	// Réglages
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Déclare les clés vu_* (défaut = valeur du champ) — SRP_Commander.DeclareSettings
	static void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("vu_journal_memoire", s_iLogMemory, "Décisions gardées en mémoire pour le menu Staff (VU4)");
		SRP_CmdSettings.DeclareInt("vu_journal_par_page", s_iLogPerPage, "Lignes par page du journal dans le menu Staff (VU4)");
		SRP_CmdSettings.DeclareBool("vu_refus_menu", s_bRefusalsInMenu, "Les décisions écartées se lisent dans le menu Staff (VU4)");
		SRP_CmdSettings.DeclareBool("vu_refus_discord", s_bRefusalsToDiscord, "Les décisions écartées partent aussi au salon Staff (VU4)");
		SRP_CmdSettings.DeclareInt("vu_refus_repos_min", s_iRefusalRepeatMin, "Un refus identique n'est réécrit qu'après ce nombre de minutes (VU4)");
		SRP_CmdSettings.DeclareInt("vu_renseignement_heures", s_iIntelHours, "Durée d'un renseignement en heures (0 = jusqu'à l'arrivée d'un nouvel officier) (VU2)");
		SRP_CmdSettings.DeclareString("vu_renseignement_missions", s_sIntelMissions, "Missions réussies sur un carré rouge qui renseignent leur région : toutes, ou une liste de types (VU2)");
		SRP_CmdSettings.DeclareString("vu_depot_missions", s_sDepotMissions, "Missions qui révèlent aussi le dépôt ennemi (RE9)");
		SRP_CmdSettings.DeclareInt("vu_troupes_renforcees", s_iTroopsStrongCent, "Troupes renforcées à partir de ce nombre de centièmes de l'effectif normal (VU2)");
		SRP_CmdSettings.DeclareInt("vu_troupes_affaiblies", s_iTroopsWeakCent, "Troupes affaiblies à ce nombre de centièmes ou moins (VU2)");
		SRP_CmdSettings.DeclareBool("vu_radio_silence", s_bRadioSilence, "Annonce radio quand une pièce de mortier ou une batterie est réduite au silence (MO9)");
		SRP_CmdSettings.DeclareBool("vu_officier_discord", s_bOfficerDiscord, "La chute d'un officier part aussi dans le salon territoire (VU3)");
		SRP_CmdSettings.DeclareInt("vu_depot_icone", s_iDepotIcon, "Icône du repère du dépôt ennemi révélé (RE9)");
		SRP_CmdSettings.DeclareInt("vu_depot_couleur", s_iDepotColor, "Couleur du repère du dépôt ennemi révélé (RE9)");
		SRP_CmdSettings.DeclareInt("vu_proximite_m", s_iNearM, "Rayon du rapport Staff « autour de moi », en mètres (VU5)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit les clés vu_* — SRP_Commander.LoadSettings
	static void LoadSettings()
	{
		s_iLogMemory = SRP_CmdSettings.GetIntClamped("vu_journal_memoire", 10, 2000);
		s_iLogPerPage = SRP_CmdSettings.GetIntClamped("vu_journal_par_page", 5, 50);
		s_bRefusalsInMenu = SRP_CmdSettings.GetBool("vu_refus_menu");
		s_bRefusalsToDiscord = SRP_CmdSettings.GetBool("vu_refus_discord");
		s_iRefusalRepeatMin = SRP_CmdSettings.GetIntClamped("vu_refus_repos_min", 0, 240);
		s_iIntelHours = SRP_CmdSettings.GetIntClamped("vu_renseignement_heures", 0, 720);
		s_sIntelMissions = SRP_CmdSettings.GetString("vu_renseignement_missions");
		s_sDepotMissions = SRP_CmdSettings.GetString("vu_depot_missions");
		s_iTroopsStrongCent = SRP_CmdSettings.GetIntClamped("vu_troupes_renforcees", 0, 500);
		s_iTroopsWeakCent = SRP_CmdSettings.GetIntClamped("vu_troupes_affaiblies", 0, 500);
		s_bRadioSilence = SRP_CmdSettings.GetBool("vu_radio_silence");
		s_bOfficerDiscord = SRP_CmdSettings.GetBool("vu_officier_discord");
		s_iDepotIcon = SRP_CmdSettings.GetIntClamped("vu_depot_icone", 0, 200);
		s_iDepotColor = SRP_CmdSettings.GetIntClamped("vu_depot_couleur", 0, 50);
		s_iNearM = SRP_CmdSettings.GetIntClamped("vu_proximite_m", 100, 20000);
	}

	//================================================================================================
	// Onglet Staff « commandeur » (SRP_Admin délègue)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Page demandée selon parts[1] : « region:R3 », « journal:<page>:<filtre> » (tout, refus, R1..R6), « forcer »,
	//! « cible:<moyen> », « ressources » ; sinon la page principale (gel, dernière décision, rythme, soldats 96/120 et
	//! IA 131/160, moyens de l'île et lien « ressources », plafonds, réglages, régions, journal, forcer) ; Commandeur
	//! absent : « L|Commandeur pas encore prêt (démarrage du front) » — SRP_AdminComponent.BuildMenu
	static string StaffTab(int playerId, vector staffPos, bool inGame, array<string> parts)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.IsStarted() || !commander.GetBook())
			return "\nL|Commandeur pas encore prêt (démarrage du front)";

		string page = "";
		if (parts && parts.Count() >= 2)
			page = parts[1];

		if (page == "region")
		{
			string code = "";
			if (parts.Count() >= 3)
				code = parts[2];
			int region = commander.GetBook().FindRegion(code);
			if (region < 0)
				return "\nL|Région inconnue : " + Esc(code) + "\nI|page:commandeur|« Retour";
			return RegionPage(region, staffPos);
		}
		if (page == "journal")
		{
			int pageIndex = 0;
			if (parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
				pageIndex = parts[2].ToInt();
			string filter = "tout";
			if (parts.Count() >= 4 && !parts[3].IsEmpty())
				filter = parts[3];
			return JournalPage(pageIndex, filter);
		}
		if (page == "forcer")
			return ForcePage();
		if (page == "cible")
		{
			string word = "";
			if (parts.Count() >= 3)
				word = parts[2];
			return TargetPage(word, staffPos, inGame);
		}
		if (page == "ressources")
			return ResourcesPage();
		return MainPage(staffPos);
	}

	//------------------------------------------------------------------------------------------------
	//! Exécute une action : gel -> SetFrozen ; relire -> SRP_Commander.ReloadSettings ; forcer et frappe -> moyen
	//! parts[2], cible parts[3] (« moi » = staffPos si inGame, « j<id> » = personnage du joueur) ->
	//! SRP_Commander.ForceOrder(kind, variant, position, author) (SEULE entrée pour forcer une DÉCISION, trou 15 ;
	//! hélico et colonne compris) ; officier-tue, officier-capture -> ForceOfficerDown ; officier-nouveau ->
	//! ForceOfficerReplace ; tp-cachette, tp-depot -> wantTeleport. Corrections des moyens (pas des décisions) :
	//! stock-plein -> SRP_CmdResources.StaffRefillAll ; stock:<genre>:<R3|ile>:<0|plein> -> StaffSetStock(kind,
	//! region, 0 ou GetFull) ; renflouer:R3 -> RequestResupply ; depot-deplacer:R3 -> GetDepots().StaffMove ;
	//! depot-saboter:R3 -> StaffSabotage ; coupure-lever:R3 -> StaffClearCut ; depot-reveler:R3 -> Reveal(région,
	//! « Staff ») (repère seul, sans radio) ; interdictions:R3 ou interdictions:tout -> SRP_CmdSupport.ClearBans(région
	//! ou -1) ; toujours SRP_CmdLog.StaffAction ; rend le texte affiché en tête — SRP_AdminComponent.Run (qui téléporte
	//! si wantTeleport)
	static string RunStaff(int playerId, vector staffPos, bool inGame, string action, array<string> parts, string author, out bool wantTeleport, out vector teleportTo)
	{
		wantTeleport = false;
		teleportTo = vector.Zero;

		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.IsStarted() || !commander.GetBook())
			return "Commandeur pas encore prêt (démarrage du front)";
		SRP_CmdRegionBook book = commander.GetBook();
		string arg = "";
		if (parts && parts.Count() >= 3)
			arg = parts[2];

		// Une seule ligne [Staff] par action : SetFrozen, StaffSetStock et StaffRefillAll l'écrivent eux-mêmes (contrat),
		// RunStaff l'écrit pour toutes les autres
		if (action == "gel")
		{
			bool freeze = !commander.IsFrozen();
			string frozenReply = commander.SetFrozen(freeze, author);
			if (frozenReply.IsEmpty())
			{
				if (freeze)
					frozenReply = "Commandeur gelé : aucune décision, aucun appui";
				else
					frozenReply = "Commandeur dégelé : il décide de nouveau";
			}
			return frozenReply;
		}
		if (action == "relire")
		{
			string reloadReply = commander.ReloadSettings(author);
			if (reloadReply.IsEmpty())
				reloadReply = "Réglages relus";
			SRP_CmdLog.StaffAction(author, "réglages relus : " + reloadReply);
			return reloadReply;
		}
		if (action == "forcer" || action == "frappe")
			return RunForce(action, parts, staffPos, inGame, author);
		if (action == "stock-plein")
		{
			SRP_CmdResources refill = SRP_CmdResources.Get();
			if (!refill)
				return "Stocks du Commandeur absents";
			refill.StaffRefillAll(author);
			return "Tous les stocks du Commandeur sont pleins";
		}
		if (action == "stock")
			return RunStock(parts, author);
		if (action == "interdictions")
		{
			SRP_CmdSupport banSupport = SRP_CmdSupport.Get();
			if (!banSupport)
				return "Appuis du Commandeur absents";
			int banRegion = -1;
			if (arg != "tout")
			{
				banRegion = book.FindRegion(arg);
				if (banRegion < 0)
					return "Région inconnue : " + arg;
			}
			string banReply = banSupport.ClearBans(banRegion, author);
			if (banReply.IsEmpty())
			{
				if (banRegion < 0)
					banReply = "Interdictions de 24 h levées dans toutes les régions";
				else
					banReply = "Interdictions de 24 h levées, " + book.GetRegionLabel(banRegion);
			}
			SRP_CmdLog.StaffAction(author, banReply);
			return banReply;
		}

		// Actions sur une région : officier-*, tp-*, renflouer, depot-*, coupure-lever
		if (!IsRegionAction(action))
			return "Action inconnue : " + action;
		int region = book.FindRegion(arg);
		if (region < 0)
			return "Région inconnue : " + arg;
		string regionLabel = book.GetRegionLabel(region);
		SRP_CmdResources resources = SRP_CmdResources.Get();
		SRP_CmdDepots depots = null;
		if (resources)
			depots = resources.GetDepots();

		if (action == "officier-tue" || action == "officier-capture")
		{
			bool captured = (action == "officier-capture");
			string downReply = commander.ForceOfficerDown(region, captured, author);
			if (downReply.IsEmpty())
			{
				if (captured)
					downReply = "Officier capturé, " + regionLabel;
				else
					downReply = "Officier tué, " + regionLabel;
			}
			SRP_CmdLog.StaffAction(author, downReply);
			return downReply;
		}
		if (action == "officier-nouveau")
		{
			string newReply = commander.ForceOfficerReplace(region, author);
			if (newReply.IsEmpty())
				newReply = "Nouvel officier, " + regionLabel;
			SRP_CmdLog.StaffAction(author, newReply);
			return newReply;
		}
		if (action == "tp-cachette")
		{
			vector hide = vector.Zero;
			string hideLocality = "";
			if (!book.GetOfficerHideout(region, hide, hideLocality) || hide == vector.Zero)
				return "Pas de cachette : aucun officier vivant dans la " + regionLabel;
			wantTeleport = true;
			teleportTo = hide + Vector(3, 0, 3);
			SRP_CmdLog.StaffAction(author, "téléportation vers la cachette du jour (" + hideLocality + "), " + regionLabel);
			return "Cachette du jour de l'officier : " + hideLocality + ", carré " + CellText(hide);
		}
		if (action == "tp-depot")
		{
			if (!depots)
				return "Dépôts du Commandeur absents";
			vector depotPos = depots.GetDepotPos(region);
			if (depotPos == vector.Zero)
				return "Aucun dépôt posé pour la " + regionLabel + " (il se pose au prochain passage)";
			wantTeleport = true;
			teleportTo = depotPos + Vector(3, 0, 3);
			SRP_CmdLog.StaffAction(author, "téléportation vers le dépôt, " + regionLabel);
			return "Dépôt de la " + regionLabel + ", carré " + CellText(depotPos);
		}
		if (action == "renflouer")
		{
			if (!resources)
				return "Stocks du Commandeur absents";
			resources.RequestResupply(region, author);
			string supplyReply = "Renflouement d'obus demandé, " + regionLabel + " : " + resources.StockText(SRP_ECmdStock.OBUS, region);
			SRP_CmdLog.StaffAction(author, supplyReply);
			return supplyReply;
		}
		if (!depots)
			return "Dépôts du Commandeur absents";
		if (action == "depot-deplacer")
		{
			depots.StaffMove(region, author);
			string moveReply = "Dépôt déplacé (reste caché, sans coupure), " + regionLabel;
			SRP_CmdLog.StaffAction(author, moveReply);
			return moveReply;
		}
		if (action == "depot-saboter")
		{
			depots.StaffSabotage(region, author);
			string sabotageReply = "Dépôt saboté comme par les joueurs, " + regionLabel;
			SRP_CmdLog.StaffAction(author, sabotageReply);
			return sabotageReply;
		}
		if (action == "coupure-lever")
		{
			// resources existe ici (depots en vient) ; le compte rendu dit s'il y avait vraiment une coupure
			int cutBefore = resources.GetCutSecondsLeft(region);
			depots.StaffClearCut(region, author);
			string clearReply;
			if (cutBefore > 0)
				clearReply = "Coupure du ravitaillement levée (il restait " + DurationText(cutBefore) + "), " + regionLabel;
			else
				clearReply = "Aucune coupure du ravitaillement en cours, " + regionLabel;
			SRP_CmdLog.StaffAction(author, clearReply);
			return clearReply;
		}
		if (action == "depot-reveler")
		{
			string revealRef = depots.Reveal(region, "Staff");
			string revealReply;
			if (revealRef.IsEmpty())
				revealReply = "Dépôt pas encore posé : il sera révélé dès sa pose (repère seul, sans radio), " + regionLabel;
			else
				revealReply = "Dépôt révélé au carré " + revealRef + " (repère seul, sans radio), " + regionLabel;
			SRP_CmdLog.StaffAction(author, revealReply);
			return revealReply;
		}
		return "Action inconnue : " + action;
	}

	//------------------------------------------------------------------------------------------------
	//! Actions qui ouvrent une page sans rien exécuter (region, journal, forcer, cible, ressources) —
	//! SRP_AdminComponent.Execute
	static bool IsInternalPage(string action)
	{
		// « forcer » n'en est pas : c'est aussi l'action qui force un moyen (commandeur:forcer:<moyen>:<cible>) ; la
		// page du choix du moyen s'ouvre par « page:commandeur:forcer » (et PageAfter y ramène)
		return action == "region" || action == "journal" || action == "cible" || action == "ressources";
	}

	//------------------------------------------------------------------------------------------------
	//! Actions à deux clics (officier-tue, officier-capture, frappe, stock-plein, stock, depot-saboter, depot-reveler)
	//! — SRP_AdminComponent.NeedsConfirm
	static bool NeedsConfirm(string action)
	{
		return action == "officier-tue" || action == "officier-capture" || action == "frappe" || action == "stock-plein" || action == "stock" || action == "depot-saboter" || action == "depot-reveler";
	}

	//------------------------------------------------------------------------------------------------
	//! Page à rouvrir après une action : officier-*, tp-*, renflouer, depot-*, coupure-lever et interdictions:<R> ->
	//! « commandeur:region:<R> », forcer et frappe -> « commandeur:forcer », stock-plein et stock -> « commandeur:
	//! ressources », le reste -> « commandeur » — SRP_AdminComponent.PageAfter
	static string PageAfter(string action, array<string> parts)
	{
		string arg = "";
		if (parts && parts.Count() >= 3)
			arg = parts[2];
		if (IsRegionAction(action) || (action == "interdictions" && arg != "tout"))
		{
			if (arg.IsEmpty())
				return "commandeur";
			return "commandeur:region:" + arg;
		}
		// La frappe (2 clics) revient sur la page de sa cible : le second clic se fait sur la même ligne
		if (action == "frappe" && !arg.IsEmpty())
			return "commandeur:cible:" + arg;
		if (action == "forcer" || action == "frappe")
			return "commandeur:forcer";
		if (action == "stock-plein" || action == "stock")
			return "commandeur:ressources";
		return "commandeur";
	}

	//================================================================================================
	// Morceaux pour les autres écrans
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! « , COMMANDEUR GELÉ » s'il est gelé, sinon "" (collé après SRP_FrontScreens.StaffHeaderPart) —
	//! SRP_AdminComponent.Header
	static string StaffHeaderPart()
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (commander && commander.IsFrozen())
			return ", COMMANDEUR GELÉ";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! « C|y|Commandeur : GELÉ — interrupteur séparé, onglet Commandeur » ou « L|Commandeur : actif — … » —
	//! SRP_FrontScreens.StaffTerritoire
	static string StaffTerritoryLine()
	{
		// Saut de ligne en tête : la ligne se colle telle quelle (une ligne vide est ignorée par le menu)
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return "\nL|Commandeur : absent — onglet Commandeur";
		if (!commander.IsStarted())
			return "\nL|Commandeur : pas encore prêt (démarrage du front) — onglet Commandeur";
		if (commander.IsFrozen())
			return "\nC|y|Commandeur : GELÉ — interrupteur séparé, onglet Commandeur";
		return "\nL|Commandeur : actif — interrupteur séparé, onglet Commandeur";
	}

	//------------------------------------------------------------------------------------------------
	//! Page « rapide:ia-groupes » : officiers et cachettes (m_Book), mortier, 2S1, blindés (appuis), colonnes
	//! (manœuvres) à radius (vu_proximite_m par défaut) — SRP_AdminComponent
	static string StaffNearReport(vector from, float radius)
	{
		float range = radius;
		if (range <= 0)
			range = s_iNearM;
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.IsStarted())
			return "\nL|Commandeur pas encore prêt (démarrage du front)";

		string body = "";
		SRP_CmdRegionBook book = commander.GetBook();
		if (book)
			body += ReportLines(book.GetNearReport(from, range), "");
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		if (support)
			body += ReportLines(support.GetNearReport(from, range), "");
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		if (maneuvers)
			body += ReportLines(maneuvers.GetReport(from, range), "");
		body += ReportLines(commander.GetKnowledgeReport(from, range), "");

		string text = "\nH|" + Esc("Commandeur ennemi à moins de " + DistanceText(range));
		if (commander.IsFrozen())
			text += "\nC|y|Commandeur GELÉ : aucune décision, aucun appui (onglet Commandeur)";
		if (body.IsEmpty())
			text += "\nL|Rien du Commandeur dans ce rayon (officier, pièce, blindé, colonne, contact connu)";
		return text + body;
	}

	//------------------------------------------------------------------------------------------------
	//! PC, zones ENNEMIES seulement (I3) : « · région de Saint-Philippe » (+ « · désorganisée ») ; région renseignée :
	//! « · officier ennemi Volkov · troupes renforcées | normales | affaiblies » (GetZoneStrength : prévu x 100 /
	//! nominal, seuils vu_troupes_renforcees et vu_troupes_affaiblies) ; "" sans région — SRP_FrontScreens.PCTerritoire
	static string PCZoneSuffix(int zone)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.IsStarted() || !commander.GetBook())
			return "";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady() || front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE)
			return "";
		SRP_CmdRegionBook book = commander.GetBook();
		int region = book.GetRegionOfZone(zone);
		if (region < 0)
			return "";

		string text = " · " + RegionOf(book.GetRegionName(region));
		if (book.IsRegionDisorganized(region))
			text += " · désorganisée";
		if (!SRP_CmdIntel.HasIntel(region))
			return Esc(text);

		if (book.HasLivingOfficer(region))
		{
			string name = book.GetOfficerName(region);
			if (!name.IsEmpty())
				text += " · " + OfficerLabel(name);
		}
		SRP_FrontEnemyComponent fe = SRP_FrontEnemyComponent.GetInstance();
		int planned = 0;
		int nominal = 0;
		if (fe && fe.GetZoneStrength(zone, planned, nominal) && nominal > 0)
		{
			int ratio = planned * 100 / nominal;
			if (ratio >= s_iTroopsStrongCent)
				text += " · troupes renforcées";
			else if (ratio <= s_iTroopsWeakCent)
				text += " · troupes affaiblies";
			else
				text += " · troupes normales";
		}
		return Esc(text);
	}

	//------------------------------------------------------------------------------------------------
	//! PC, section « H|Renseignement sur l'ennemi » : une ligne par région (renseignées, désorganisées, autres,
	//! libérées), dépôt seulement s'il est révélé, puis le mode d'emploi ; "" sans Commandeur —
	//! SRP_FrontScreens.PCTerritoire
	static string PCIntelSection(int playerId)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.IsStarted() || !commander.GetBook())
			return "";
		SRP_CmdRegionBook book = commander.GetBook();
		int count = book.GetRegionCount();
		if (count <= 0)
			return "";
		SRP_CmdDepots depots = null;
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (resources)
			depots = resources.GetDepots();

		string intelLines = "";
		string desorgLines = "";
		string otherLines = "";
		string freeLines = "";
		for (int r = 0; r < count; r++)
		{
			string label = Capital(RegionOf(book.GetRegionName(r)));
			int state = book.GetRegionState(r);
			if (state == SRP_ECmdRegionState.LIBEREE)
			{
				freeLines += "\nC|d|" + Esc(label + " — libérée");
				continue;
			}

			// Dépôt : seulement s'il est révélé (RE9)
			string depotText = "";
			if (depots && depots.IsRevealed(r))
			{
				vector depotSpot = depots.GetDepotPos(r);
				if (depotSpot != vector.Zero)
					depotText = " · dépôt ennemi repéré au carré " + CellText(depotSpot);
				else
					depotText = " · dépôt ennemi repéré";
			}
			string desorgText = "";
			if (state == SRP_ECmdRegionState.DESORGANISEE)
				desorgText = DesorgText(book, r);

			if (SRP_CmdIntel.HasIntel(r))
			{
				string intelLine = label + " — ";
				if (book.HasLivingOfficer(r))
					intelLine += OfficerLabel(book.GetOfficerName(r)) + ", en poste";
				else if (!desorgText.IsEmpty())
					intelLine += desorgText;
				else
					intelLine += "pas d'officier en poste";
				intelLine += depotText;
				intelLine += " · renseignée depuis " + WhenText(SRP_CmdIntel.GetIntelSince(r));
				string sourceText = SRP_CmdIntel.GetIntelSourceText(r);
				if (!sourceText.IsEmpty())
					intelLine += " (" + sourceText + ")";
				intelLines += "\nC|w|" + Esc(intelLine);
			}
			else if (state == SRP_ECmdRegionState.DESORGANISEE)
			{
				desorgLines += "\nC|g|" + Esc(label + " — " + desorgText + depotText);
			}
			else
			{
				otherLines += "\nC|d|" + Esc(label + " — pas de renseignement" + depotText);
			}
		}

		string text = "\nH|Renseignement sur l'ennemi";
		text += intelLines + desorgLines + otherLines + freeLines;
		text += "\nL|Pour renseigner une région : saisir un PC ennemi, capturer un officier ennemi ou réussir une mission chez l'ennemi.";
		return text;
	}

	//================================================================================================
	// Outils de texte
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! « région de X », ou « région d'X » devant a, e, i, o, u, y, é, è, h — radio, écrans, SRP_CmdRegionBook
	static string RegionOf(string regionName)
	{
		string name = regionName.Trim();
		if (name.IsEmpty())
			return "région inconnue";
		string first = name.Substring(0, 1);
		first.ToLower();
		if (first == "a" || first == "e" || first == "i" || first == "o" || first == "u" || first == "y" || first == "h")
			return "région d'" + name;
		// Lettres accentuées (deux octets) : comparées au début du nom
		if (name.StartsWith("é") || name.StartsWith("è") || name.StartsWith("É") || name.StartsWith("È"))
			return "région d'" + name;
		return "région de " + name;
	}

	//------------------------------------------------------------------------------------------------
	//! « officier ennemi Volkov » (VU1) — radio, écrans
	static string OfficerLabel(string surname)
	{
		string name = surname.Trim();
		if (name.IsEmpty())
			return "officier ennemi";
		return "officier ennemi " + name;
	}

	//------------------------------------------------------------------------------------------------
	//! « 2 h 10 » ou « 12 min » — écrans, journal
	static string DurationText(int seconds)
	{
		if (seconds < 60)
		{
			if (seconds < 0)
				return "0 s";
			return string.Format("%1 s", seconds);
		}
		if (seconds < 3600)
			return string.Format("%1 min", seconds / 60);
		int hours = seconds / 3600;
		int minutes = (seconds - hours * 3600) / 60;
		if (minutes == 0)
			return string.Format("%1 h", hours);
		return string.Format("%1 h %2", hours, SRP_Time.Pad2(minutes));
	}

	//------------------------------------------------------------------------------------------------
	//! « 074 042 » : référence du carré sous la position — écrans, journal
	static string CellText(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "?";
		int cell = front.CellIndexAt(position);
		if (cell < 0)
			return "hors carte";
		return front.CellRef(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! « prudent », « audacieux », « lent », « méthodique » — écrans, journal
	static string TraitWord(int trait)
	{
		if (trait == SRP_ECmdTrait.PRUDENT)
			return "prudent";
		if (trait == SRP_ECmdTrait.AUDACIEUX)
			return "audacieux";
		if (trait == SRP_ECmdTrait.LENT)
			return "lent";
		if (trait == SRP_ECmdTrait.METHODIQUE)
			return "méthodique";
		return "?";
	}

	//------------------------------------------------------------------------------------------------
	//! « prudente », « normale », « agressive » — écrans
	static string MoodWord(int mood)
	{
		if (mood == SRP_ECmdMood.PRUDENTE)
			return "prudente";
		if (mood == SRP_ECmdMood.NORMALE)
			return "normale";
		if (mood == SRP_ECmdMood.AGRESSIVE)
			return "agressive";
		return "?";
	}

	//------------------------------------------------------------------------------------------------
	//! Texte libre sans « | » ni retour à la ligne (comme SRP_AdminComponent.Esc) — toutes les pages
	static string Esc(string text)
	{
		string s = text;
		s.Replace("|", "/");
		s.Replace("\n", " · ");
		return s;
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Page principale de l'onglet — StaffTab
	protected static string MainPage(vector staffPos)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.GetBook())
			return "\nL|Commandeur pas encore prêt (démarrage du front)";
		SRP_CmdRegionBook book = commander.GetBook();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdResources resources = SRP_CmdResources.Get();
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();

		// Gel (VU6) : interrupteur du Commandeur, séparé de celui du front
		string text = "\nH|État-major ennemi";
		if (commander.IsFrozen())
			text += "\nK|g|commandeur:gel|[x] Commandeur gelé : ON — aucune décision, aucun appui ; les soldats déjà posés réagissent sur place";
		else
			text += "\nK|w|commandeur:gel|[ ] Commandeur gelé : OFF — il décide seul, dans les règles du front";
		if (front && front.IsFrozen())
			text += "\nC|y|Front gelé : ON — interrupteur séparé, onglet Territoire";
		else
			text += "\nL|Front gelé : OFF — interrupteur séparé, onglet Territoire";

		SRP_CmdLogEntry last = LastDecision();
		if (last)
			text += "\nL|" + Esc("Dernière décision : " + last.m_sHour + " · " + RegionShort(last.m_iRegion) + " · " + last.m_sText);
		else
			text += "\nL|Dernière décision : aucune depuis le démarrage";

		if (commander.IsFrozen())
			text += "\nL|Rythme : aucun cycle tant qu'il est gelé";
		else
			text += "\nL|" + Esc("Rythme : cycle rapide (mortier, hélico, fouilles) dans " + DurationText(commander.GetNextFastSeconds()) + " · cycle lent (blindé, artillerie lourde, bombe) dans " + DurationText(commander.GetNextSlowSeconds()));

		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string soldiers = string.Format("Soldats ennemis au sol : %1/%2", capacity.CountGround(), capacity.GetMaxSoldiers());
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
			soldiers += string.Format(" · soldats actifs du jeu : %1/%2", aiWorld.GetCurrentNumOfActiveAIs(), aiWorld.GetLimitOfActiveAIs());
		text += "\nL|" + Esc(soldiers);

		// Interrupteurs du fichier de réglages (changés dans commandeur_reglages.txt, puis « Relire »)
		string switches = "Interrupteurs du fichier : appuis " + SRP_Utils.OuiNon(support && support.m_bOn);
		if (maneuvers)
			switches += " · renforts " + SRP_Utils.OuiNon(maneuvers.m_bReinforceOn) + " · replis " + SRP_Utils.OuiNon(maneuvers.m_bRetreatOn) + " · harcèlement " + SRP_Utils.OuiNon(maneuvers.m_bHarassOn);
		switches += " · informateurs " + SRP_Utils.OuiNon(commander.m_bInformants) + " · gelé au démarrage " + SRP_Utils.OuiNon(commander.m_bFrozenAtStart);
		text += "\nL|" + Esc(switches);
		// Résumé du cerveau : une seule ligne en morceaux « · », coupée en lignes lisibles
		text += DotLines(commander.GetStaffReport(), "");

		// Moyens de l'île (les obus sont par région : lignes des régions et page « ressources »)
		if (resources)
		{
			text += "\nH|Moyens tenus par le Commandeur (toute l'île)";
			string armorLine = "Blindés : " + resources.StockText(SRP_ECmdStock.BLINDE, -1);
			if (support && support.GetArmor())
			{
				string engaged = support.GetArmor().GetEngagedText();
				if (!engaged.IsEmpty())
					armorLine += " · " + engaged;
			}
			text += "\nL|" + Esc(armorLine);
			text += "\nL|" + Esc("Artillerie lourde : " + resources.StockText(SRP_ECmdStock.ARTILLERIE, -1) + " · Hélico : " + resources.StockText(SRP_ECmdStock.HELICO, -1) + " · Bombe : " + resources.StockText(SRP_ECmdStock.BOMBE, -1));
			text += "\nL|" + Esc("Remplissage moyen des moyens lourds : " + DecimalText(resources.GetStockLevel()) + " du plein");
			text += "\nI|page:commandeur:ressources|» Stocks par région, dépôts, place des soldats, appuis, colonnes";
		}
		if (support)
		{
			string caps = support.GetCapsText();
			if (!caps.IsEmpty())
				text += "\nL|" + Esc("Plafonds : " + caps);
		}

		// Réglages
		text += "\nL|" + Esc("Réglages : " + SRP_CmdSettings.GetStatusText());
		array<string> problems = {};
		int problemCount = SRP_CmdSettings.GetProblems(problems);
		for (int p = 0; p < problems.Count() && p < PROBLEMS_SHOWN; p++)
			text += "\nC|y|" + Esc(problems[p]);
		if (problemCount > PROBLEMS_SHOWN)
			text += string.Format("\nC|y|… et %1 autre(s) problème(s) : voir la console du serveur", problemCount - PROBLEMS_SHOWN);
		text += "\nI|commandeur:relire|» Relire les fichiers de réglages (sans redémarrer)";

		// Régions
		int regions = book.GetRegionCount();
		text += string.Format("\nH|Régions (%1) — choisis une région", regions);
		for (int r = 0; r < regions; r++)
			text += RegionLine(book, resources, r);
		if (regions == 0)
			text += "\nL|Aucune région tracée (voir commandeur_regions.txt)";

		// Journal et forçage
		text += "\nH|Journal des décisions";
		text += string.Format("\nI|page:commandeur:journal:0:tout|» Décisions, la plus récente d'abord (%1 gardées en mémoire)", s_iLogMemory);
		text += "\nI|page:commandeur:journal:0:refus|» Avec les décisions écartées (seuil, plafond, stock, frein)";
		text += "\nH|Forcer une décision";
		text += "\nI|page:commandeur:forcer|» Choisir un moyen, puis sa cible…";
		text += "\nI|page:rapide:ia-groupes|» Autour de moi : officiers, pièces, blindés, colonnes (onglet Rapide)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page d'une région (officier, cachette, désorganisation, alerte, rapports, obus, mortier, 2S1, dépôt, renforts,
	//! renseignement, zones) ; actions officier-*, tp-*, renflouer, depot-deplacer, depot-saboter, coupure-lever,
	//! depot-reveler, interdictions ; région libérée : date et retour — StaffTab
	protected static string RegionPage(int region, vector staffPos)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.GetBook())
			return "\nL|Commandeur pas encore prêt (démarrage du front)";
		SRP_CmdRegionBook book = commander.GetBook();
		SRP_CmdResources resources = SRP_CmdResources.Get();
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();
		string code = book.GetRegionCode(region);
		string label = Capital(RegionOf(book.GetRegionName(region)));

		// Région libérée (OF11) : sa date seulement
		if (book.GetRegionState(region) == SRP_ECmdRegionState.LIBEREE)
		{
			string freeText = "\nH|" + Esc(label + " (" + code + ") — libérée");
			SRP_CmdRegion freeData = book.GetRegion(region);
			if (freeData && freeData.m_iLiberatedAt > 0)
				freeText += "\nL|" + Esc("Libérée le " + UnixDateText(freeData.m_iLiberatedAt) + " : plus d'officier ; une zone reprise par l'ennemi la rattache à une région voisine");
			else
				freeText += "\nL|Libérée : plus d'officier";
			freeText += "\nI|page:commandeur:journal:0:" + code + "|» Décisions de cette région";
			freeText += "\nI|page:commandeur|« Retour";
			return freeText;
		}

		array<int> zones = {};
		int zoneCount = book.GetRegionZones(region, zones);
		int red = book.CountRegionRedZones(region);
		string text = "\nH|" + Esc(string.Format("%1 (%2) — %3 zone(s) ennemie(s), %4 à nous", label, code, red, zoneCount - red));

		// Officier, cachette, désorganisation, alerte, rapports : page du registre, sinon lignes de secours
		string report = book.GetRegionReport(region);
		if (!report.IsEmpty())
			text += ReportLines(report, "");
		else
			text += OfficerLines(book, region);

		if (resources)
		{
			string shells = "Obus de mortier : " + resources.StockText(SRP_ECmdStock.OBUS, region) + " · revenu " + DecimalText(resources.GetRevenueFactor(SRP_ECmdStock.OBUS, region)) + " du normal";
			int nextShell = resources.MinutesToNextUnit(SRP_ECmdStock.OBUS, region);
			if (nextShell >= 0)
				shells += string.Format(" · prochain dans %1 min", nextShell);
			text += "\nL|" + Esc(shells);
		}
		if (support)
		{
			text += "\nL|" + Esc("Équipe de mortier : " + MortarText(support, region));
			text += "\nL|" + Esc("Batterie 2S1 : " + BatteryText(support, region));
		}
		if (resources && resources.GetDepots())
			text += "\nL|" + Esc(DepotText(resources, region));
		if (maneuvers)
		{
			string reinforce = maneuvers.GetReinforcementText(region);
			if (reinforce.IsEmpty())
				reinforce = "aucun depuis le démarrage";
			text += "\nL|" + Esc("Renforts : " + reinforce);
		}
		if (SRP_CmdIntel.HasIntel(region))
			text += "\nL|" + Esc("Renseignement des joueurs : oui depuis " + WhenText(SRP_CmdIntel.GetIntelSince(region)) + " (" + SRP_CmdIntel.GetIntelSourceText(region) + ")");
		else
			text += "\nL|Renseignement des joueurs : non";

		text += "\nH|Zones de la région";
		text += ZoneLines(zones, staffPos);

		// Actions (celles à deux clics sont rouges ou orange et le disent)
		text += "\nH|Actions";
		text += "\nI|commandeur:tp-cachette:" + code + "|» Aller à la cachette du jour de l'officier";
		text += "\nI|commandeur:tp-depot:" + code + "|» Aller au dépôt de la région";
		if (book.HasLivingOfficer(region))
		{
			string officer = OfficerLabel(book.GetOfficerName(region));
			int desorgSeconds = book.m_iDesorgMin * 60;
			text += "\nK|r|commandeur:officier-tue:" + code + "|" + Esc("!! L'" + officer + " tombe (tué) : région désorganisée " + DurationText(desorgSeconds) + ", annoncé à la radio (2 clics)");
			text += "\nK|r|commandeur:officier-capture:" + code + "|" + Esc("!! L'" + officer + " est capturé : désorganisée " + DurationText(desorgSeconds * book.m_iDesorgCaptureFactor) + ", renseignement et dépôt révélés, annoncé (2 clics)");
		}
		text += "\nK|y|commandeur:officier-nouveau:" + code + "|» Nouvel officier maintenant (fin de la désorganisation, correction sans annonce)";
		text += "\nI|commandeur:renflouer:" + code + "|» Renflouer ses obus maintenant (depuis la région la mieux fournie)";
		text += "\nI|commandeur:depot-deplacer:" + code + "|» Déplacer le dépôt (reste caché, sans coupure)";
		text += "\nK|r|commandeur:depot-saboter:" + code + "|!! Saboter le dépôt comme un vrai sabotage : radio, ravitaillement réduit (2 clics)";
		text += "\nI|commandeur:coupure-lever:" + code + "|» Lever la coupure du ravitaillement";
		text += "\nK|y|commandeur:depot-reveler:" + code + "|» Révéler le dépôt aux joueurs : repère seul, sans radio (2 clics)";
		text += "\nI|commandeur:interdictions:" + code + "|» Lever les interdictions de 24 h (mortier, artillerie lourde)";
		text += "\nI|page:commandeur:journal:0:" + code + "|» Décisions de cette région";
		text += "\nI|page:commandeur:region:" + code + "|» Rafraîchir";
		text += "\nI|page:commandeur|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Journal paginé (vu_journal_par_page lignes, teintes par nature) ; filtre « tout », « refus » ou code de région —
	//! StaffTab
	protected static string JournalPage(int page, string filter)
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander || !commander.GetBook())
			return "\nL|Commandeur pas encore prêt (démarrage du front)";
		SRP_CmdRegionBook book = commander.GetBook();

		// « tout » : décisions et faits ; « refus » : avec les décisions écartées ; « R3 » : une région, refus compris
		int region = -1;
		bool withRefusals = false;
		string title = "Décisions du Commandeur";
		string regionCode = "";
		if (filter == "refus")
		{
			withRefusals = true;
			title = "Décisions du Commandeur, écartées comprises";
		}
		else if (filter != "tout")
		{
			region = book.FindRegion(filter);
			if (region < 0)
				return "\nL|Région inconnue : " + Esc(filter) + "\nI|page:commandeur|« Retour";
			withRefusals = true;
			regionCode = book.GetRegionCode(region);
			title = "Décisions du Commandeur, " + book.GetRegionLabel(region);
		}

		int total = SRP_CmdLog.CountEntries(region, withRefusals);
		int perPage = s_iLogPerPage;
		if (perPage < 1)
			perPage = 15;
		int pages = (total + perPage - 1) / perPage;
		if (pages < 1)
			pages = 1;
		int current = page;
		if (current >= pages)
			current = pages - 1;
		if (current < 0)
			current = 0;

		string text = "\nH|" + Esc(string.Format("%1 — page %2 / %3, la plus récente d'abord", title, current + 1, pages));
		if (total == 0)
			text += "\nL|Aucune ligne en mémoire (le menu repart vide au redémarrage ; l'historique reste dans le salon Staff et le journal du mois)";

		array<ref SRP_CmdLogEntry> entries = {};
		SRP_CmdLog.GetEntries(region, withRefusals, current * perPage, perPage, entries);
		foreach (SRP_CmdLogEntry entry : entries)
			text += "\nC|" + KindTint(entry.m_iKind) + "|" + Esc(entry.m_sHour + " · " + RegionShort(entry.m_iRegion) + " · " + entry.m_sText);

		if (withRefusals && !s_bRefusalsInMenu)
			text += "\nC|d|Les décisions écartées ne sont pas gardées (réglage vu_refus_menu = 0)";

		if (current + 1 < pages)
			text += string.Format("\nI|page:commandeur:journal:%1:%2|› Page suivante", current + 1, filter);
		if (current > 0)
			text += string.Format("\nI|page:commandeur:journal:%1:%2|‹ Page précédente", current - 1, filter);
		text += string.Format("\nI|page:commandeur:journal:%1:%2|» Rafraîchir", current, filter);
		if (!regionCode.IsEmpty())
			text += "\nI|page:commandeur:region:" + regionCode + "|« Retour à la région";
		text += "\nI|page:commandeur|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page « ressources » : SRP_CmdResources.GetStaffReport, dépôts (GetDepots().GetStaffReport), capacité
	//! (SRP_CmdCapacity.Get().GetStaffReport), plafonds (SRP_CmdSupport.GetCapsText) ; par stock « vider » / « remplir »
	//! (stock:<genre>:<R3|ile>:<0|plein>) et « Tout remplir » (stock-plein, 2 clics) — StaffTab
	protected static string ResourcesPage()
	{
		SRP_Commander commander = SRP_Commander.Get();
		SRP_CmdResources resources = SRP_CmdResources.Get();
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		SRP_CmdManeuvers maneuvers = SRP_CmdManeuvers.Get();

		string text = "\nH|Moyens du Commandeur";
		if (!resources || !commander || !commander.GetBook())
		{
			text += "\nL|Stocks du Commandeur absents";
		}
		else
		{
			SRP_CmdRegionBook book = commander.GetBook();
			text += DotLines(resources.GetStaffReport(), "");
			text += "\nK|r|commandeur:stock-plein|!! Tout remplir : tous les stocks pleins (2 clics)";

			text += "\nH|Moyens de l'île : vider ou remplir (2 clics)";
			array<string> islandWords = {"blinde", "helico", "artillerie", "bombe"};
			foreach (string word : islandWords)
			{
				int kind = StockOfWord(word);
				string kindName = Capital(SRP_CmdResources.KindLabel(kind));
				text += "\nK|y|commandeur:stock:" + word + ":ile:0|" + Esc("» " + kindName + " (" + resources.StockText(kind, -1) + ") : vider (2 clics)");
				text += "\nK|g|commandeur:stock:" + word + ":ile:plein|" + Esc("» " + kindName + " : remplir (2 clics)");
			}

			text += "\nH|Obus des régions : vider ou remplir (2 clics)";
			// Toute l'île : chaque région mise à la même part de son plein (StaffSetStock, région -1)
			text += "\nK|y|commandeur:stock:obus:ile:0|" + Esc("» Obus, toute l'île (" + resources.StockText(SRP_ECmdStock.OBUS, -1) + ") : vider partout (2 clics)");
			text += "\nK|g|commandeur:stock:obus:ile:plein|" + Esc("» Obus, toute l'île : tout remplir (2 clics)");
			for (int r = 0; r < book.GetRegionCount(); r++)
			{
				if (book.GetRegionState(r) == SRP_ECmdRegionState.LIBEREE)
					continue;
				string code = book.GetRegionCode(r);
				string label = Capital(RegionOf(book.GetRegionName(r)));
				text += "\nK|y|commandeur:stock:obus:" + code + ":0|" + Esc("» Obus, " + label + " (" + resources.StockText(SRP_ECmdStock.OBUS, r) + ") : vider (2 clics)");
				text += "\nK|g|commandeur:stock:obus:" + code + ":plein|" + Esc("» Obus, " + label + " : remplir (2 clics)");
			}

			if (resources.GetDepots())
			{
				text += "\nH|Dépôts ennemis";
				string depotReport = resources.GetDepots().GetStaffReport();
				if (depotReport.IsEmpty())
					text += "\nL|Aucun dépôt posé";
				else
					text += DotLines(depotReport, "");
			}
		}

		text += "\nH|Place des soldats ennemis";
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		text += DotLines(capacity.GetStaffReport(), "");
		string refusal = capacity.GetLastRefusal();
		if (!refusal.IsEmpty())
			text += "\nC|y|" + Esc("Dernier refus de place : " + refusal);

		if (support)
		{
			// Les plafonds sont déjà une des lignes de GetReport (« Plafonds : … ») : pas de doublon ici
			text += "\nH|Appuis";
			array<string> supportLines = {};
			support.GetReport(supportLines);
			foreach (string supportLine : supportLines)
				text += ReportLines(supportLine, "");
			text += "\nI|commandeur:interdictions:tout|» Lever toutes les interdictions de 24 h (mortier, artillerie lourde)";
		}
		if (maneuvers)
		{
			text += "\nH|Colonnes en route";
			string columns = maneuvers.GetColumnsReport();
			if (columns.IsEmpty())
				text += "\nL|Aucune colonne en route";
			else
				text += ReportLines(columns, "");
		}
		text += "\nI|page:commandeur:ressources|» Rafraîchir";
		text += "\nI|page:commandeur|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Choix du moyen à forcer — StaffTab
	protected static string ForcePage()
	{
		string text = "\nH|Forcer — choisis le moyen (la cible vient ensuite)";
		array<string> words = {"mortier", "fumee", "feinte", "artillerie", "bombe", "avion", "btr", "brdm", "typhoon", "renfort", "helico", "harcelement", "colonne", "decrochage"};
		foreach (string word : words)
		{
			string clicks = "";
			if (IsStrikeWord(word))
				clicks = " (2 clics)";
			text += "\nI|page:commandeur:cible:" + word + "|" + Esc("» " + MeansLabel(word) + " : " + MeansHelp(word) + clicks);
		}
		text += "\nL|Contre-attaque : onglet Territoire, page de la zone (« Forcer une contre-attaque »)";

		string cost = "payé sur les stocks (réglage cmd_staff_gratuit = 0)";
		SRP_Commander commander = SRP_Commander.Get();
		if (commander && commander.m_bStaffFree)
			cost = "gratuit (réglage cmd_staff_gratuit)";
		int maxSoldiers = SRP_CmdCapacity.Get().GetMaxSoldiers();
		text += "\nC|d|" + Esc(string.Format("Forcé : passe outre seuils, plafonds et délais, jamais la sécurité (base, soldats ennemis ou civils trop près, %1 soldats) ; %2 ; marche aussi Commandeur gelé", maxSoldiers, cost));
		text += "\nI|page:commandeur|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Choix de la cible d'un moyen (ma position, chaque joueur connecté avec un personnage) — StaffTab
	protected static string TargetPage(string word, vector staffPos, bool inGame)
	{
		int variant = -1;
		int kind = OrderOfWord(word, variant);
		if (kind < 0)
			return "\nL|Moyen inconnu : " + Esc(word) + "\nI|page:commandeur:forcer|« Autre moyen";

		// Artillerie lourde et bombe : action « frappe », à deux clics
		string action = "forcer";
		string tint = "y";
		string clicks = "";
		if (IsStrikeWord(word))
		{
			action = "frappe";
			tint = "r";
			clicks = " (2 clics)";
		}

		string text = "\nH|" + Esc(MeansLabel(word) + " — choisis la cible");
		if (inGame)
			text += "\nK|" + tint + "|commandeur:" + action + ":" + word + ":moi|" + Esc("» À ma position — " + PlaceText(staffPos) + clicks);
		else
			text += "\nC|d|Il faut être en jeu, dans un personnage, pour viser sa propre position";

		// Joueurs connectés qui ont un personnage, triés par nom
		PlayerManager manager = GetGame().GetPlayerManager();
		array<int> ids = {};
		manager.GetPlayers(ids);
		array<int> shownIds = {};
		array<string> names = {};
		foreach (int id : ids)
		{
			if (!manager.GetPlayerControlledEntity(id))
				continue;
			shownIds.Insert(id);
			names.Insert(manager.GetPlayerName(id));
		}
		for (int i = 0; i < names.Count(); i++)
		{
			for (int j = i + 1; j < names.Count(); j++)
			{
				if (names[j].Compare(names[i], false) < 0)
				{
					string swapName = names[i];
					names[i] = names[j];
					names[j] = swapName;
					int swapId = shownIds[i];
					shownIds[i] = shownIds[j];
					shownIds[j] = swapId;
				}
			}
		}
		for (int k = 0; k < shownIds.Count(); k++)
		{
			IEntity character = manager.GetPlayerControlledEntity(shownIds[k]);
			if (!character)
				continue;
			text += string.Format("\nK|%1|commandeur:%2:%3:j%4|", tint, action, word, shownIds[k]) + Esc("» Sur " + names[k] + " — " + PlaceText(character.GetOrigin()) + clicks);
		}
		if (shownIds.IsEmpty())
			text += "\nL|Aucun joueur en jeu";
		text += "\nI|page:commandeur:forcer|« Autre moyen";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Mot du Staff -> SRP_ECmdOrder (mortier, fumee, feinte, artillerie, bombe, avion, btr, brdm, typhoon, renfort,
	//! helico, harcelement, colonne, decrochage) et variant (type de blindé, sinon -1) ; -1 si inconnu — RunStaff
	protected static int OrderOfWord(string word, out int variant)
	{
		variant = -1;
		if (word == "mortier")
		{
			variant = 0;
			return SRP_ECmdOrder.MORTIER;
		}
		if (word == "fumee")
		{
			variant = 0;
			return SRP_ECmdOrder.FUMEE;
		}
		if (word == "feinte")
			return SRP_ECmdOrder.FEINTE;
		if (word == "artillerie")
			return SRP_ECmdOrder.ARTILLERIE;
		if (word == "bombe")
			return SRP_ECmdOrder.BOMBE;
		if (word == "avion")
			return SRP_ECmdOrder.AVION;
		if (word == "renfort")
			return SRP_ECmdOrder.RENFORT;
		if (word == "helico")
			return SRP_ECmdOrder.HELICO;
		if (word == "harcelement")
			return SRP_ECmdOrder.HARCELEMENT;
		if (word == "colonne")
			return SRP_ECmdOrder.COLONNE;
		if (word == "decrochage")
			return SRP_ECmdOrder.DECROCHAGE;
		if (word == "btr" || word == "brdm" || word == "typhoon")
		{
			// Type lu dans les réglages des blindés ; sans appuis, rang du contrat (0 BTR-70, 1 BRDM-2, 2 Typhoon)
			int fallback = 2;
			if (word == "btr")
				fallback = 0;
			else if (word == "brdm")
				fallback = 1;
			variant = fallback;
			SRP_CmdSupport support = SRP_CmdSupport.Get();
			if (support && support.GetArmor())
				variant = support.GetArmor().FindTypeByWord(word);
			return SRP_ECmdOrder.BLINDE;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! forcer et frappe : moyen parts[2], cible parts[3] -> SRP_Commander.ForceOrder, puis la ligne [Staff] — RunStaff
	protected static string RunForce(string action, array<string> parts, vector staffPos, bool inGame, string author)
	{
		if (!parts || parts.Count() < 4)
		{
			// « commandeur:forcer » seul : rien à exécuter, PageAfter ouvre le choix du moyen
			if (action == "forcer")
				return "";
			return "Frappe : moyen ou cible manquant";
		}
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return "Commandeur absent";

		string word = parts[2];
		string target = parts[3];
		int variant = -1;
		int kind = OrderOfWord(word, variant);
		if (kind < 0)
			return "Moyen inconnu : " + word;
		if (IsStrikeWord(word) && action != "frappe")
			return MeansLabel(word) + " : passe par la ligne rouge de sa cible (2 clics)";
		if (kind == SRP_ECmdOrder.BLINDE && variant < 0)
			return MeansLabel(word) + " : type introuvable dans les réglages des blindés (appui_blinde_*)";

		vector position = vector.Zero;
		string targetText = "";
		if (target == "moi")
		{
			if (!inGame)
				return "Il faut être en jeu, dans un personnage, pour viser sa propre position";
			position = staffPos;
			targetText = "à ma position";
		}
		else if (target.StartsWith("j") && target.Length() > 1)
		{
			string idText = target.Substring(1, target.Length() - 1);
			if (!SRP_Utils.IsNumeric(idText))
				return "Cible inconnue : " + target;
			int targetId = idText.ToInt();
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(targetId);
			if (!character)
				return "Joueur absent ou sans personnage : cible perdue";
			position = character.GetOrigin();
			targetText = "sur " + GetGame().GetPlayerManager().GetPlayerName(targetId);
		}
		else
		{
			return "Cible inconnue : " + target;
		}

		string reply = commander.ForceOrder(kind, variant, position, author);
		if (reply.IsEmpty())
			reply = "ordre donné";
		string what = MeansLabel(word) + " " + targetText + " (carré " + CellText(position) + ")";
		SRP_CmdLog.StaffAction(author, "forcé : " + what + " — " + reply);
		return "Forcé : " + what + " — " + reply;
	}

	//------------------------------------------------------------------------------------------------
	//! stock:<genre>:<R3|ile>:<0|plein> -> SRP_CmdResources.StaffSetStock (qui écrit sa ligne [Staff]) — RunStaff
	protected static string RunStock(array<string> parts, string author)
	{
		SRP_Commander commander = SRP_Commander.Get();
		SRP_CmdResources resources = SRP_CmdResources.Get();
		if (!commander || !commander.GetBook() || !resources)
			return "Stocks du Commandeur absents";
		if (!parts || parts.Count() < 5)
			return "Stock : action incomplète";
		int kind = StockOfWord(parts[2]);
		if (kind < 0)
			return "Stock inconnu : " + parts[2];

		// Ordre des paramètres des stocks : TOUJOURS (kind, region), region = -1 pour les moyens de l'île ; les obus
		// acceptent aussi « ile » (toutes les régions à la même part de leur plein)
		int region = -1;
		string place = "toute l'île";
		if (kind == SRP_ECmdStock.OBUS && parts[3] != "ile")
		{
			region = commander.GetBook().FindRegion(parts[3]);
			if (region < 0)
				return "Région inconnue : " + parts[3];
			place = commander.GetBook().GetRegionLabel(region);
		}
		float value = 0;
		if (parts[4] == "plein")
			value = resources.GetFull(kind, region);
		resources.StaffSetStock(kind, region, value, author);
		return Capital(SRP_CmdResources.KindLabel(kind)) + ", " + place + " : " + resources.StockText(kind, region);
	}

	//------------------------------------------------------------------------------------------------
	//! Actions qui visent une région par parts[2] — RunStaff, PageAfter
	protected static bool IsRegionAction(string action)
	{
		return action == "officier-tue" || action == "officier-capture" || action == "officier-nouveau" || action == "tp-cachette" || action == "tp-depot" || action == "renflouer" || action == "depot-deplacer" || action == "depot-saboter" || action == "coupure-lever" || action == "depot-reveler";
	}

	//------------------------------------------------------------------------------------------------
	//! Moyens à deux clics (action « frappe ») : artillerie lourde et bombe — TargetPage, RunForce, ForcePage
	protected static bool IsStrikeWord(string word)
	{
		return word == "artillerie" || word == "bombe";
	}

	//------------------------------------------------------------------------------------------------
	//! Mot d'un stock -> SRP_ECmdStock, -1 si inconnu — RunStock, ResourcesPage
	protected static int StockOfWord(string word)
	{
		if (word == "obus")
			return SRP_ECmdStock.OBUS;
		if (word == "blinde")
			return SRP_ECmdStock.BLINDE;
		if (word == "helico")
			return SRP_ECmdStock.HELICO;
		if (word == "artillerie")
			return SRP_ECmdStock.ARTILLERIE;
		if (word == "bombe")
			return SRP_ECmdStock.BOMBE;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom d'un moyen du Staff — ForcePage, TargetPage, RunForce
	protected static string MeansLabel(string word)
	{
		if (word == "mortier")
			return "Mortier explosif";
		if (word == "fumee")
			return "Mortier fumigène";
		if (word == "feinte")
			return "Feinte";
		if (word == "artillerie")
			return "Artillerie lourde";
		if (word == "bombe")
			return "Bombe";
		if (word == "avion")
			return "Passages d'avion";
		if (word == "btr")
			return "Blindé BTR-70";
		if (word == "brdm")
			return "Blindé BRDM-2";
		if (word == "typhoon")
			return "Blindé Typhoon";
		if (word == "renfort")
			return "Renfort";
		if (word == "helico")
			return "Hélico de recherche";
		if (word == "harcelement")
			return "Harcèlement du front";
		if (word == "colonne")
			return "Colonne";
		if (word == "decrochage")
			return "Décrochage";
		return word;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce que fait un moyen forcé, en une phrase — ForcePage
	protected static string MeansHelp(string word)
	{
		if (word == "mortier")
			return "quelques obus explosifs, la pièce de la région se pose d'abord";
		if (word == "fumee")
			return "fumigènes, gratuits";
		if (word == "feinte")
			return "fumée puis obus sur un faux axe, l'assaut vient d'ailleurs";
		if (word == "artillerie")
			return "coups de réglage, puis salve";
		if (word == "bombe")
			return "passage d'avion, puis bombe";
		if (word == "avion")
			return "passages bas, sans bombe";
		if (word == "renfort")
			return "un camion de la localité ennemie voisine";
		if (word == "helico")
			return "sortie de l'hélico au-dessus de la cible";
		if (word == "harcelement")
			return "raid ou quelques obus sur le front le plus proche";
		if (word == "colonne")
			return "soldats de la localité ennemie la plus proche vers la suivante (sur le papier, réelle près des joueurs)";
		if (word == "decrochage")
			return "repli de la localité ennemie la plus proche de la cible";
		return "blindé vers la cible";
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne de région de la page principale (lien vers sa page) — MainPage
	protected static string RegionLine(SRP_CmdRegionBook book, SRP_CmdResources resources, int region)
	{
		string code = book.GetRegionCode(region);
		string label = Capital(RegionOf(book.GetRegionName(region)));
		int state = book.GetRegionState(region);
		if (state == SRP_ECmdRegionState.LIBEREE)
			return "\nK|d|page:commandeur:region:" + code + "|" + Esc(label + " — libérée, plus d'officier");

		string line = label + " — ";
		if (book.HasLivingOfficer(region))
			line += OfficerLabel(book.GetOfficerName(region)) + " (" + TraitWord(book.GetOfficerTrait(region)) + ")";
		else
			line += "pas d'officier en poste";
		string tint = "r";
		if (state == SRP_ECmdRegionState.DESORGANISEE)
		{
			tint = "y";
			line += " · " + DesorgText(book, region);
		}
		int alert = Math.Round(book.GetRegionAlert(region));
		line += string.Format(" · alerte %1/%2", alert, book.m_iAlertMax);
		if (resources)
		{
			line += " · obus " + resources.StockText(SRP_ECmdStock.OBUS, region);
			if (resources.GetRegionSupplyState(region) == 0)
				line += " · ravitaillement affaibli";
			SRP_CmdDepots depots = resources.GetDepots();
			if (depots && depots.IsRevealed(region))
				line += " · dépôt connu des joueurs";
		}
		line += string.Format(" · %1 zone(s) ennemie(s)", book.CountRegionRedZones(region));
		if (SRP_CmdIntel.HasIntel(region))
			line += " · renseignée";
		return "\nK|" + tint + "|page:commandeur:region:" + code + "|" + Esc(line);
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes de secours de la page d'une région quand le registre ne rend pas de page : officier, cachette,
	//! désorganisation, alerte, rapports — RegionPage
	protected static string OfficerLines(SRP_CmdRegionBook book, int region)
	{
		string text = "";
		int now = System.GetUnixTime();
		if (book.HasLivingOfficer(region))
		{
			string officer = Capital(OfficerLabel(book.GetOfficerName(region))) + " · " + TraitWord(book.GetOfficerTrait(region));
			if (book.IsOfficerPosted(region))
				officer += " · posé dans le monde";
			else
				officer += " · sur le papier";
			text += "\nL|" + Esc(officer);
			vector hide = vector.Zero;
			string hideLocality = "";
			if (book.GetOfficerHideout(region, hide, hideLocality))
				text += "\nL|" + Esc("Cachette du jour : " + hideLocality + ", carré " + CellText(hide));
		}
		else
		{
			text += "\nL|Pas d'officier en poste";
		}
		if (book.IsRegionDisorganized(region))
			text += "\nC|y|" + Esc(Capital(DesorgText(book, region)) + " : sans rapports, renforts plus lents, ni mortier ni artillerie");
		else
			text += "\nL|Désorganisation : non";
		int alert = Math.Round(book.GetRegionAlert(region));
		text += string.Format("\nL|Alerte de la région : %1/%2", alert, book.m_iAlertMax);

		SRP_CmdRegion data = book.GetRegion(region);
		if (data)
		{
			string reports = "Rapports : ";
			if (data.m_iLastReportUnix > 0)
				reports += "dernier il y a " + DurationText(now - data.m_iLastReportUnix);
			else
				reports += "aucun depuis le démarrage";
			reports += string.Format(" · perdus %1 · retardés %2", data.m_iReportsLost, data.m_iReportsDelayed);
			if (!data.m_sRadioCutLocality.IsEmpty() && data.m_iRadioCutUntil > now)
				reports += " · radio de " + data.m_sRadioCutLocality + " coupée encore " + DurationText(data.m_iRadioCutUntil - now);
			text += "\nL|" + Esc(reports);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « désorganisée encore 5 h 12 (officier ennemi Sokolov capturé) » — pages Staff et PC
	protected static string DesorgText(SRP_CmdRegionBook book, int region)
	{
		string text = "désorganisée";
		SRP_CmdRegion data = book.GetRegion(region);
		if (data)
		{
			int left = data.m_iDesorgUntil - System.GetUnixTime();
			if (left > 0)
				text += " encore " + DurationText(left);
		}
		string name = book.GetOfficerName(region);
		if (!name.IsEmpty())
		{
			int officerState = book.GetOfficerState(region);
			if (officerState == SRP_ECmdOfficerState.CAPTURE)
				text += " (" + OfficerLabel(name) + " capturé)";
			else if (officerState == SRP_ECmdOfficerState.TUE)
				text += " (" + OfficerLabel(name) + " tombé)";
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « prête (rien de posé) », « en batterie, carré 074 050 », « muette encore 18 h » (MO9) — RegionPage
	protected static string MortarText(SRP_CmdSupport support, int region)
	{
		int state = support.GetMortarState(region);
		if (state == SRP_ECmdPieceState.MUETTE)
		{
			int left = support.GetMortarSilencedUntil(region) - System.GetUnixTime();
			if (left > 0)
				return "muette encore " + DurationText(left);
			return "muette";
		}
		if (state == SRP_ECmdPieceState.EN_TIR)
			return "en tir, carré " + CellText(support.GetMortarPos(region));
		if (state == SRP_ECmdPieceState.EN_BATTERIE)
			return "en batterie, carré " + CellText(support.GetMortarPos(region));
		return "prête (rien de posé)";
	}

	//------------------------------------------------------------------------------------------------
	//! « prête (rien de posé) », « posée, carré 090 060 », « détruite encore 20 h » (AP1) — RegionPage
	protected static string BatteryText(SRP_CmdSupport support, int region)
	{
		int state = support.GetBatteryState(region);
		if (state == SRP_ECmdPieceState.MUETTE)
		{
			int left = support.GetBatteryDestroyedUntil(region) - System.GetUnixTime();
			if (left > 0)
				return "détruite encore " + DurationText(left);
			return "détruite";
		}
		if (state == SRP_ECmdPieceState.EN_TIR)
			return "en tir, carré " + CellText(support.GetBatteryPos(region));
		if (state == SRP_ECmdPieceState.EN_BATTERIE)
			return "posée, carré " + CellText(support.GetBatteryPos(region));
		return "prête (rien de posé)";
	}

	//------------------------------------------------------------------------------------------------
	//! « Dépôt : carré 074 042 · ravitaillement normal · connu des joueurs : non » (RE8, RE9) — RegionPage
	protected static string DepotText(SRP_CmdResources resources, int region)
	{
		SRP_CmdDepots depots = resources.GetDepots();
		string text = "Dépôt : ";
		vector position = depots.GetDepotPos(region);
		if (position == vector.Zero)
			text += "pas encore posé";
		else
			text += "carré " + CellText(position);
		int supply = resources.GetRegionSupplyState(region);
		if (supply == 0)
			text += " · ravitaillement affaibli (sabotage ou stock bas)";
		else if (supply == 2)
			text += " · ravitaillement renforcé";
		else
			text += " · ravitaillement normal";
		int cutLeft = resources.GetCutSecondsLeft(region);
		if (cutLeft > 0)
			text += " · revenu coupé encore " + DurationText(cutLeft);
		text += " · connu des joueurs : " + SRP_Utils.OuiNon(depots.IsRevealed(region));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones d'une région, liens vers leur page de l'onglet Territoire — RegionPage
	protected static string ZoneLines(array<int> zones, vector staffPos)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "\nL|Front pas encore prêt";
		if (zones.IsEmpty())
			return "\nL|Aucune zone rattachée";
		SRP_FrontEnemyComponent fe = SRP_FrontEnemyComponent.GetInstance();
		string text = "";
		foreach (int zone : zones)
		{
			bool ours = (front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU);
			string line = front.GetZoneLabel(zone);
			string tint = "r";
			if (ours)
			{
				tint = "g";
				line += " — à nous";
			}
			else
			{
				line += " — ennemie";
				int planned = 0;
				int nominal = 0;
				if (fe && fe.GetZoneStrength(zone, planned, nominal))
					line += string.Format(" · troupes %1/%2", planned, nominal);
			}
			if (staffPos != vector.Zero)
				line += " · " + DistanceText(vector.Distance(staffPos, front.GetZoneCentroid(zone)));
			text += "\nK|" + tint + "|page:territoire:zone:" + front.GetZoneCode(zone) + "|" + Esc(line);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Dernière DÉCISION prise (décision ou appui), null sinon — MainPage (« Dernière décision »)
	protected static SRP_CmdLogEntry LastDecision()
	{
		array<ref SRP_CmdLogEntry> entries = {};
		SRP_CmdLog.GetEntries(-1, false, 0, SRP_CmdLog.CountEntries(-1, false), entries);
		foreach (SRP_CmdLogEntry entry : entries)
		{
			if (entry.m_iKind == SRP_ECmdLogKind.DECISION || entry.m_iKind == SRP_ECmdLogKind.APPUI)
				return entry;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Teinte d'une ligne du journal : APPUI r, DECISION y, REFUS d, STAFF g, autres w — JournalPage
	protected static string KindTint(int kind)
	{
		if (kind == SRP_ECmdLogKind.APPUI)
			return "r";
		if (kind == SRP_ECmdLogKind.DECISION)
			return "y";
		if (kind == SRP_ECmdLogKind.REFUS)
			return "d";
		if (kind == SRP_ECmdLogKind.STAFF)
			return "g";
		return "w";
	}

	//------------------------------------------------------------------------------------------------
	//! « Saint-Philippe », « Île » pour toute l'île — lignes du journal
	protected static string RegionShort(int region)
	{
		if (region < 0)
			return "Île";
		SRP_Commander commander = SRP_Commander.Get();
		if (commander && commander.GetBook())
		{
			string name = commander.GetBook().GetRegionName(region);
			if (!name.IsEmpty())
				return name;
		}
		int number = region + 1;
		return "R" + number.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! « carré 074 042 · Régina (S07) » — TargetPage
	protected static string PlaceText(vector position)
	{
		string text = "carré " + CellText(position);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsReady())
		{
			int zone = front.GetZoneAt(position);
			if (zone >= 0)
				text += " · " + front.GetZoneLabel(zone);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Première lettre en capitale (lettre simple ; une lettre accentuée reste telle quelle)
	protected static string Capital(string text)
	{
		if (text.IsEmpty())
			return text;
		string first = text.Substring(0, 1);
		first.ToUpper();
		return first + text.Substring(1, text.Length() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Un rapport multi-lignes en lignes L| (ou C|teinte|), lignes vides retirées
	protected static string ReportLines(string text, string tint)
	{
		if (text.IsEmpty())
			return "";
		array<string> lines = {};
		text.Split("\n", lines, true);
		string result = "";
		foreach (string line : lines)
		{
			string clean = line.Trim();
			if (clean.IsEmpty())
				continue;
			if (tint.IsEmpty())
				result += "\nL|" + Esc(clean);
			else
				result += "\nC|" + tint + "|" + Esc(clean);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Comme ReportLines, en coupant aussi aux « · » (rapports d'une seule longue ligne)
	protected static string DotLines(string text, string tint)
	{
		string flat = text;
		flat.Replace(" · ", "\n");
		return ReportLines(flat, tint);
	}

	//------------------------------------------------------------------------------------------------
	//! « 1240 m » ou « 3,4 km »
	protected static string DistanceText(float meters)
	{
		if (meters < 1000)
		{
			int roundMeters = Math.Round(meters);
			return string.Format("%1 m", roundMeters);
		}
		int tenths = Math.Round(meters / 100);
		int km = tenths / 10;
		int rest = tenths - km * 10;
		return string.Format("%1,%2 km", km, rest);
	}

	//------------------------------------------------------------------------------------------------
	//! « 0,82 » (deux décimales, virgule)
	protected static string DecimalText(float value)
	{
		int hundredths = Math.Round(value * 100);
		if (hundredths < 0)
			hundredths = 0;
		int whole = hundredths / 100;
		int rest = hundredths - whole * 100;
		return string.Format("%1,%2", whole, SRP_Time.Pad2(rest));
	}

	//------------------------------------------------------------------------------------------------
	//! « 21:02 » si c'est aujourd'hui (heure locale du serveur), sinon « 25/09 21:02 »
	protected static string WhenText(int unix)
	{
		if (unix <= 0)
			return "?";
		int offset = LocalOffset();
		int day = (unix + offset) / 86400;
		int today = (System.GetUnixTime() + offset) / 86400;
		if (day == today)
			return UnixHourText(unix);
		return UnixDateText(unix);
	}

	//------------------------------------------------------------------------------------------------
	//! « 21:02 » : heure locale du serveur d'une heure Unix
	protected static string UnixHourText(int unix)
	{
		int shifted = unix + LocalOffset();
		int days = shifted / 86400;
		int secs = shifted - days * 86400;
		int hour = secs / 3600;
		int minute = (secs - hour * 3600) / 60;
		return SRP_Time.Pad2(hour) + ":" + SRP_Time.Pad2(minute);
	}

	//------------------------------------------------------------------------------------------------
	//! « 25/09 22:40 » : date et heure locales du serveur d'une heure Unix
	protected static string UnixDateText(int unix)
	{
		int shifted = unix + LocalOffset();
		int days = shifted / 86400;
		int year = 0;
		int month = 0;
		int dayOfMonth = 0;
		CivilFromDays(days, year, month, dayOfMonth);
		return SRP_Time.Pad2(dayOfMonth) + "/" + SRP_Time.Pad2(month) + " " + UnixHourText(unix);
	}

	//------------------------------------------------------------------------------------------------
	//! Heure locale - heure UTC du serveur, en secondes (arrondie au quart d'heure)
	protected static int LocalOffset()
	{
		int year = 0;
		int month = 0;
		int day = 0;
		int hour = 0;
		int minute = 0;
		int second = 0;
		System.GetYearMonthDay(year, month, day);
		System.GetHourMinuteSecond(hour, minute, second);
		int utcYear = 0;
		int utcMonth = 0;
		int utcDay = 0;
		int utcHour = 0;
		int utcMinute = 0;
		int utcSecond = 0;
		System.GetYearMonthDayUTC(utcYear, utcMonth, utcDay);
		System.GetHourMinuteSecondUTC(utcHour, utcMinute, utcSecond);
		int localSeconds = DaysFromCivil(year, month, day) * 86400 + hour * 3600 + minute * 60 + second;
		int utcSeconds = DaysFromCivil(utcYear, utcMonth, utcDay) * 86400 + utcHour * 3600 + utcMinute * 60 + utcSecond;
		float quarters = (localSeconds - utcSeconds) / 900.0;
		int roundQuarters = Math.Round(quarters);
		return roundQuarters * 900;
	}

	//------------------------------------------------------------------------------------------------
	//! Jours depuis le 1er janvier 1970 d'une date du calendrier (années après 1970)
	protected static int DaysFromCivil(int year, int month, int day)
	{
		int y = year;
		if (month <= 2)
			y = y - 1;
		int era = y / 400;
		int yearOfEra = y - era * 400;
		int shiftedMonth = month + 9;
		if (month > 2)
			shiftedMonth = month - 3;
		int dayOfYear = (153 * shiftedMonth + 2) / 5 + day - 1;
		int dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
		return era * 146097 + dayOfEra - 719468;
	}

	//------------------------------------------------------------------------------------------------
	//! Date du calendrier d'un nombre de jours depuis le 1er janvier 1970 (inverse de DaysFromCivil)
	protected static void CivilFromDays(int days, out int year, out int month, out int day)
	{
		int z = days + 719468;
		int era = z / 146097;
		int dayOfEra = z - era * 146097;
		int yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
		int y = yearOfEra + era * 400;
		int dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
		int shiftedMonth = (5 * dayOfYear + 2) / 153;
		day = dayOfYear - (153 * shiftedMonth + 2) / 5 + 1;
		if (shiftedMonth < 10)
			month = shiftedMonth + 3;
		else
			month = shiftedMonth - 9;
		if (month <= 2)
			y = y + 1;
		year = y;
	}
}
