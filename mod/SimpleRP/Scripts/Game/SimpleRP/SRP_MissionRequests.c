//------------------------------------------------------------------------------------------------
// SimpleRP — Demandes de mission (suivi de mission par Discord)
// Au tableau des missions, l'action « Demander une mission » ouvre un assistant :
//   1. mission présente au tableau, ou capture d'une zone ennemie AU CONTACT du front (J4) ;
//   2. choix de la mission ou de la zone (liste unique : SRP_MissionManagerComponent.GetCaptureTargets) ;
//   3. effectif : les joueurs en ligne qui partent (le demandeur en fait toujours partie) ;
//   4. envoi.
// La demande part au bot PAG par la passerelle (liste « requests » de l'état, capacité « demande »). Le bot
// la publie dans le salon commandement ; un officier prend l'assignation puis décide depuis Discord :
//   demande assigner <id> | liberer <id> | accorder <id> <pied|moto> <véhicules ou -> [consignes]
//   demande refuser <id> [motif] | clore <id> [raison]
// Accordée : une mission est acceptée au nom du demandeur (SRP_MissionManagerComponent.Claim, sans
// condition de grade puisqu'un officier l'a décidé) ; une capture devient une opération suivie. Le groupe
// reçoit la décision, le transport autorisé et les consignes. Le suivi se fait ici, toutes les 5 s (Tick) :
// mission réussie ou perdue, zone prise, demandeur parti. Les officiers en jeu sont prévenus aussi.
// Une zone se désigne par son CODE (« S07 », SRP_FrontComponent.GetZoneCode : ni « : » ni « | », qui séparent les
// champs du menu) ; tout texte affiché utilise son libellé (« Régina (S07) », GetZoneLabel).
// Tout est en mémoire : comme les missions, les demandes ne survivent pas à un redémarrage du serveur.
// Le même tableau sert aussi à (SRP_MissionManagerComponent) :
//   - « Trouver une mission » : tout soldat formé fait tirer une mission au sort, en ne choisissant que la distance ;
//     elle va au tableau et se demande ensuite comme les autres ;
//   - « Ordonner une mission » (Lieutenant, Capitaine, Staff) : type, lieu (ou zone au contact), chef de mission ;
//     créée et accordée d'un coup ;
//   - une capture accordée devient une VRAIE mission (m_sMissionId) : aucun délai pour se préparer, engagée à 3 km,
//     échec après 15 minutes de zone vide, plus d'abandon une fois engagée.
//------------------------------------------------------------------------------------------------

enum SRP_ERequestState
{
	EN_ATTENTE,		// publiée, aucun officier ne l'a prise
	ASSIGNEE,		// un officier l'examine
	ACCORDEE,		// accordée : opération en cours
	REFUSEE,
	ANNULEE,		// par le demandeur, ou devenue sans objet
	REUSSIE,
	ECHOUEE
}

//------------------------------------------------------------------------------------------------
class SRP_MissionRequest
{
	string m_sId;
	bool m_bTerritory;				// faux : mission du tableau ; vrai : capture d'une zone
	string m_sTarget;				// identifiant de mission ou CODE de zone (« S07 »)
	string m_sTitle;
	string m_sDetails;
	string m_sGrid;
	string m_sRequester;
	string m_sRequesterIdentity;
	ref array<string> m_aMemberNames = {};
	ref array<string> m_aMemberIdentities = {};
	int m_iState = SRP_ERequestState.EN_ATTENTE;
	string m_sOfficer;
	bool m_bMotorised;
	ref array<string> m_aVehicles = {};		// « nom n° id », déjà lisibles
	string m_sNote;					// consignes, motif du refus, résultat
	string m_sMissionId;			// capture accordée : la mission créée pour elle
	string m_sCreated;
	int m_iClosedTick;				// fin : la demande reste visible du bot quelques minutes
	int m_iRequesterGoneTick;

	//------------------------------------------------------------------------------------------------
	bool IsOpen()
	{
		return m_iState == SRP_ERequestState.EN_ATTENTE || m_iState == SRP_ERequestState.ASSIGNEE || m_iState == SRP_ERequestState.ACCORDEE;
	}

	//------------------------------------------------------------------------------------------------
	string KindText()
	{
		if (m_bTerritory)
			return "capture de la zone";
		return "mission";
	}
}

//------------------------------------------------------------------------------------------------
//! Assistant en cours de remplissage, par joueur
class SRP_RequestDraft
{
	int m_iPage;					// 0 accueil, 1 missions, 2 zones, 3 effectif, 4 trouver, 5 à 7 ordonner (type, lieu, chef)
	int m_iOrderType = -1;			// ordonner : type de mission, ou -2 pour une capture de zone
	int m_iOrderPlace = -1;			// ordonner : indice de la localité (missions du tableau)
	string m_sOrderZone;			// ordonner une capture : code de la zone (« S07 »)
	bool m_bTerritory;
	string m_sTarget;
	ref array<int> m_aMembers = {};
}

//------------------------------------------------------------------------------------------------
class SRP_MissionRequests
{
	protected static const int KEEP_CLOSED_MS = 900000;		// 15 min : le temps que le bot lise l'issue
	protected static const int REQUESTER_GONE_MS = 300000;		// 5 min de déconnexion avant d'annuler une demande en attente

	protected static ref array<ref SRP_MissionRequest> s_aRequests = {};
	protected static ref map<int, ref SRP_RequestDraft> s_mDrafts = new map<int, ref SRP_RequestDraft>();
	protected static int s_iNextId;

	//------------------------------------------------------------------------------------------------
	//! Nouvelle partie (démarrage du gestionnaire de joueurs) : on repart de zéro
	static void Reset()
	{
		s_aRequests.Clear();
		s_mDrafts.Clear();
	}

	//------------------------------------------------------------------------------------------------
	static string StateName(int state)
	{
		switch (state)
		{
			case SRP_ERequestState.EN_ATTENTE: return "attente";
			case SRP_ERequestState.ASSIGNEE: return "assignee";
			case SRP_ERequestState.ACCORDEE: return "accordee";
			case SRP_ERequestState.REFUSEE: return "refusee";
			case SRP_ERequestState.ANNULEE: return "annulee";
			case SRP_ERequestState.REUSSIE: return "reussie";
		}
		return "echouee";
	}

	//------------------------------------------------------------------------------------------------
	protected static string StateLabel(int state)
	{
		switch (state)
		{
			case SRP_ERequestState.EN_ATTENTE: return "en attente d'un officier";
			case SRP_ERequestState.ASSIGNEE: return "examinée par un officier";
			case SRP_ERequestState.ACCORDEE: return "accordée, opération en cours";
			case SRP_ERequestState.REFUSEE: return "refusée";
			case SRP_ERequestState.ANNULEE: return "annulée";
			case SRP_ERequestState.REUSSIE: return "réussie";
		}
		return "close";
	}

	//------------------------------------------------------------------------------------------------
	//! Identifiant unique d'un redémarrage à l'autre : le bot garde les anciens en mémoire
	protected static string NextId()
	{
		if (s_iNextId <= 0)
		{
			int y, m, d, h, mi, s;
			System.GetYearMonthDay(y, m, d);
			System.GetHourMinuteSecond(h, mi, s);
			s_iNextId = d * 100000 + h * 3600 + mi * 60 + s;
		}
		s_iNextId++;
		return "D" + s_iNextId.ToString();
	}

	//------------------------------------------------------------------------------------------------
	static SRP_MissionRequest FindById(string id)
	{
		string wanted = id;
		wanted.ToUpper();
		foreach (SRP_MissionRequest request : s_aRequests)
		{
			if (request.m_sId == wanted)
				return request;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static SRP_MissionRequest FindOpenOf(string identity)
	{
		foreach (SRP_MissionRequest request : s_aRequests)
		{
			if (request.IsOpen() && request.m_sRequesterIdentity == identity)
				return request;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsTargeted(bool territory, string target)
	{
		foreach (SRP_MissionRequest request : s_aRequests)
		{
			if (request.IsOpen() && request.m_bTerritory == territory && request.m_sTarget == target)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur connecté qui porte cette identité, -1 sinon
	protected static int PlayerIdOf(string identity)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players || identity.IsEmpty())
			return -1;
		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);
		for (int i = 0; i < ids.Count(); i++)
		{
			if (records[i] && records[i].m_sIdentity == identity)
				return ids[i];
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Le front, s'il est construit (null sinon) : aucune zone ne se lit avant
	protected static SRP_FrontComponent ReadyFront()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return null;
		return front;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone désignée par sa clé (code « S07 », SRP_FrontComponent.FindZone) ; -1 si elle est inconnue ou si le front
	//! n'est pas prêt
	protected static int ZoneOf(string key)
	{
		SRP_FrontComponent front = ReadyFront();
		if (!front || key.IsEmpty())
			return -1;
		return front.FindZone(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé affiché d'une zone désignée par sa clé (« Régina (S07) ») ; la clé elle-même si la zone est inconnue
	protected static string ZoneLabelOf(string key)
	{
		SRP_FrontComponent front = ReadyFront();
		int zone = ZoneOf(key);
		if (!front || zone < 0)
			return key;
		return front.GetZoneLabel(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! J4 : les zones ennemies au contact du front qu'on peut viser (source unique du tableau, de « Ordonner » et du
	//! pont : SRP_MissionManagerComponent.GetCaptureTargets, vide si le front est gelé)
	protected static void CaptureTargets(notnull array<int> zones)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions && ReadyFront())
			missions.GetCaptureTargets(zones);
	}

	//------------------------------------------------------------------------------------------------
	//! G3 : en-tête des listes de capture, « Captures en cours : 1 / 1 (2 à partir de 10 joueurs connectés) »
	protected static string CaptureHeader()
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "";
		return string.Format("\nH|Captures en cours : %1 / %2 (%3 à partir de %4 joueurs connectés)", missions.CountActiveOfType(SRP_EMissionType.LIBERER), missions.CaptureLimit(), missions.GetCaptureMaxHigh(), missions.GetCaptureHighPlayers());
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne d'une liste de zones vide : front gelé (J3) ou rien au contact
	protected static string NoTargetLine(string whenNone)
	{
		SRP_FrontComponent front = ReadyFront();
		if (!front)
			return "\nH|Le front n'est pas encore prêt : réessaie dans un instant";
		if (front.IsFrozen())
			return "\nH|Front gelé par l'état-major : aucune capture pour l'instant";
		return "\nH|" + whenNone;
	}

	//------------------------------------------------------------------------------------------------
	//! Message à tout le groupe d'une demande (les membres encore en ligne)
	protected static void TellGroup(SRP_MissionRequest request, string title, string text)
	{
		foreach (string identity : request.m_aMemberIdentities)
		{
			int playerId = PlayerIdOf(identity);
			if (playerId < 0)
				continue;
			if (title.IsEmpty())
				SRP_Utils.NotifyPlayer(playerId, text);
			else
				SRP_Utils.HintPlayer(playerId, title, text);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void Close(SRP_MissionRequest request, int state, string note)
	{
		request.m_iState = state;
		if (!note.IsEmpty())
			request.m_sNote = note;
		request.m_iClosedTick = System.GetTickCount();
		SRP_JournalComponent.Log("MISSION", string.Format("Demande %1 (%2 %3, par %4) : %5 — %6", request.m_sId, request.KindText(), request.m_sTitle, request.m_sRequester, StateLabel(state), request.m_sNote));
	}

	//------------------------------------------------------------------------------------------------
	// Assistant (tableau des missions)
	//------------------------------------------------------------------------------------------------
	static void OpenMenu(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;
		players.EnsureRegistered(playerId);
		s_mDrafts.Set(playerId, new SRP_RequestDraft());
		Send(playerId, "");
	}

	//------------------------------------------------------------------------------------------------
	protected static void Send(int playerId, string message)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!pc)
			return;
		// Le dernier « M| » l'emporte à l'écran : le message de l'action passe après celui de la page
		string content = BuildMenu(playerId);
		if (!message.IsEmpty())
			content += "\nM|" + message;
		pc.SRP_OpenRequest(content);
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildMenu(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		SRP_PlayerRecord me;
		if (players)
			me = players.GetRecord(playerId);
		if (!me)
			return "M|Fiche introuvable";

		// Une demande déjà ouverte : on la suit, on ne peut que l'annuler
		SRP_MissionRequest mine = FindOpenOf(me.m_sIdentity);
		if (mine)
		{
			string text = string.Format("M|Demande %1 : %2 « %3 »\nL|État : %4", mine.m_sId, mine.KindText(), mine.m_sTitle, StateLabel(mine.m_iState));
			if (!mine.m_sOfficer.IsEmpty())
				text += "\nL|Officier : " + mine.m_sOfficer;
			text += "\nH|Effectif : " + Join(mine.m_aMemberNames);
			if (mine.m_iState == SRP_ERequestState.ACCORDEE)
			{
				text += "\nH|Transport : " + TransportText(mine);
				if (!mine.m_sNote.IsEmpty())
					text += "\nH|Consignes : " + mine.m_sNote;
				text += "\nK|r|annuler|Abandonner l'opération (choisir puis Valider)";
			}
			else
				text += "\nK|r|annuler|Annuler ma demande (choisir puis Valider)";
			return text;
		}

		SRP_RequestDraft draft;
		if (!s_mDrafts.Find(playerId, draft))
		{
			draft = new SRP_RequestDraft();
			s_mDrafts.Set(playerId, draft);
		}

		if (draft.m_iPage == 1)
			return BuildMissions();
		if (draft.m_iPage == 2)
			return BuildZones();
		if (draft.m_iPage == 3)
			return BuildMembers(playerId, draft);
		if (draft.m_iPage == 4)
			return BuildBands();
		if (draft.m_iPage == 5)
			return BuildOrderTypes();
		if (draft.m_iPage == 6)
			return BuildOrderPlaces(draft);
		if (draft.m_iPage == 7)
			return BuildOrderChiefs(draft);

		string home = "M|Tableau des missions\nL|Un officier reçoit ta demande sur Discord et l'accorde ou non, avec le transport autorisé."
			+ "\nH|Demander au commandement"
			+ "\nI|type:m|Une mission du tableau"
			+ "\nI|type:t|La capture d'une zone ennemie (au contact du front)"
			+ "\nH|Aucune mission ne te convient ?"
			+ "\nI|trouver|Trouver une mission (tirée au sort, à faire accorder ensuite)";
		if (CanOrder(me))
			home += "\nH|Officier" + "\nI|ordre|Ordonner une mission (type, lieu, chef de mission)";
		return home;
	}

	//------------------------------------------------------------------------------------------------
	//! Ordonner une mission : Lieutenant, Capitaine, Staff
	protected static bool CanOrder(SRP_PlayerRecord record)
	{
		return record && (record.m_bStaff || SRP_Grades.IsOfficier(record.m_iGrade));
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildBands()
	{
		return "M|Trouver une mission\nL|Le type et le lieu sont tirés au sort. Tu ne choisis que la distance à la base."
			+ "\nL|La mission ira au tableau : il faudra ensuite la demander au commandement."
			+ "\nH|À quelle distance ?"
			+ "\nI|bande:0|Proche"
			+ "\nI|bande:1|Moyenne"
			+ "\nI|bande:2|Lointaine"
			+ "\nK|d|home|< Retour";
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildOrderTypes()
	{
		string text = "M|Ordonner une mission : quel type ?";
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		array<int> types = {};
		if (missions)
			missions.GetOrderTypes(types);
		foreach (int type : types)
			text += string.Format("\nI|otype:%1|%2", type, SRP_MissionManagerComponent.TypeName(type));
		text += "\nK|y|otype:c|Capture d'une zone ennemie (au contact du front)";
		return text + "\nK|d|home|< Retour";
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildOrderPlaces(SRP_RequestDraft draft)
	{
		// Capture : les zones ennemies au contact du front (J4), désignées par leur code
		if (draft.m_iOrderType == -2)
		{
			string zonesText = "M|Capture : quelle zone ?" + CaptureHeader();
			SRP_FrontComponent front = ReadyFront();
			array<int> targets = {};
			CaptureTargets(targets);
			int listed = 0;
			if (front)
			{
				foreach (int targetZone : targets)
				{
					zonesText += string.Format("\nI|ozone:%1|%2 — point clé au carré %3", front.GetZoneCode(targetZone), front.GetZoneLabel(targetZone), SRP_FleetManagerComponent.GridOf(front.GetMissionPoint(targetZone)));
					listed++;
				}
			}
			if (listed == 0)
				zonesText += NoTargetLine("Aucune zone ennemie au contact du front");
			return zonesText + "\nK|d|ordre|< Retour";
		}

		string text = string.Format("M|%1 : dans quelle localité ?", SRP_MissionManagerComponent.TypeName(draft.m_iOrderType));
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		array<string> names = {};
		array<vector> centers = {};
		if (missions)
			missions.GetOrderPlaces(names, centers);
		foreach (int index, string name : names)
			text += string.Format("\nI|olieu:%1|%2 (carré %3)", index, name, SRP_FleetManagerComponent.GridOf(centers[index]));
		return text + "\nK|d|ordre|< Retour";
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildOrderChiefs(SRP_RequestDraft draft)
	{
		string text = "M|Qui sera chef de mission ?";
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		if (players)
			players.GetConnectedRecords(ids, records);
		for (int i = 0; i < ids.Count(); i++)
		{
			if (records[i])
				text += string.Format("\nI|ochef:%1|%2 %3", ids[i], SRP_Grades.GetName(records[i].m_iGrade), records[i].m_sName);
		}
		if (draft.m_iOrderType != -2)
			text += "\nK|y|ochef:0|Personne : laisser la mission au tableau";
		return text + "\nK|d|ordre|< Recommencer";
	}

	//------------------------------------------------------------------------------------------------
	//! Dernier écran de « Ordonner » : la mission est créée et, s'il y a un chef, accordée d'un coup
	protected static string RunOrder(SRP_PlayerRecord me, SRP_RequestDraft draft, int chiefId)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "Gestionnaire de missions absent";
		string author = SRP_Grades.GetName(me.m_iGrade) + " " + me.m_sName;

		// Capture d'une zone au contact : toutes les règles (zone à nous, base, gel, contact J4, plafond G3, chef) sont
		// dans CreateCapture, qui dit pourquoi elle refuse
		if (draft.m_iOrderType == -2)
		{
			if (draft.m_sOrderZone.IsEmpty())
				return "Zone introuvable";
			string refusal = "";
			SRP_Mission capture = missions.CreateCapture(draft.m_sOrderZone, chiefId, author, refusal);
			if (!capture)
				return "Capture impossible : " + refusal;
			string zoneLabel = ZoneLabelOf(draft.m_sOrderZone);
			SRP_Utils.HintPlayer(chiefId, "Ordre de mission", string.Format("%1 te confie %2 : capture de la zone %3.\nTu es chef de mission. Aucun délai pour te préparer : la mission s'engage à l'entrée dans la zone.", author, capture.m_sId, zoneLabel));
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			if (players)
				players.NotifyOfficiers(string.Format("%1 : capture de %2 ordonnée par %3, chef de mission %4", capture.m_sId, zoneLabel, author, capture.m_sClaimedBy));
			return string.Format("%1 : capture de %2 ordonnée, chef de mission %3", capture.m_sId, zoneLabel, capture.m_sClaimedBy);
		}

		array<string> names = {};
		array<vector> centers = {};
		missions.GetOrderPlaces(names, centers);
		if (draft.m_iOrderPlace < 0 || draft.m_iOrderPlace >= names.Count())
			return "Lieu introuvable";
		bool ok;
		return missions.OrderAt(author, draft.m_iOrderType, centers[draft.m_iOrderPlace], names[draft.m_iOrderPlace], chiefId, ok);
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildMissions()
	{
		string text = "M|Choisis la mission";
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		array<SRP_Mission> active = {};
		if (missions)
			missions.GetActiveMissions(active);

		int shown = 0;
		foreach (SRP_Mission mission : active)
		{
			if (!mission.m_sClaimedBy.IsEmpty() || IsTargeted(false, mission.m_sId))
				continue;
			text += string.Format("\nI|cible:m:%1|%1 — %2, %3 (difficulté %4)", mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel, mission.m_iDifficulty);
			shown++;
		}
		if (shown == 0)
			text += "\nH|Aucune mission libre au tableau pour l'instant";
		return text + "\nK|d|home|< Retour";
	}

	//------------------------------------------------------------------------------------------------
	//! J4 : les zones ennemies au contact du front, sans celles qui ont déjà une demande ouverte ; clé « cible:t:S07 »
	protected static string BuildZones()
	{
		string text = "M|Choisis la zone à capturer" + CaptureHeader();
		text += "\nL|Seules les zones ennemies au contact du front se demandent. Une demande peut attendre qu'une capture se libère.";
		SRP_FrontComponent front = ReadyFront();
		array<int> targets = {};
		CaptureTargets(targets);

		int shown = 0;
		if (front)
		{
			foreach (int targetZone : targets)
			{
				string code = front.GetZoneCode(targetZone);
				if (code.IsEmpty() || IsTargeted(true, code))
					continue;
				text += string.Format("\nI|cible:t:%1|%2 — point clé au carré %3", code, front.GetZoneLabel(targetZone), SRP_FleetManagerComponent.GridOf(front.GetMissionPoint(targetZone)));
				shown++;
			}
		}
		if (shown == 0)
			text += NoTargetLine("Aucune zone au contact du front à demander");
		return text + "\nK|d|home|< Retour";
	}

	//------------------------------------------------------------------------------------------------
	protected static string BuildMembers(int playerId, SRP_RequestDraft draft)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		string kind = "Mission";
		string target = draft.m_sTarget;
		if (draft.m_bTerritory)
		{
			kind = "Capture de la zone";
			target = ZoneLabelOf(draft.m_sTarget);
		}
		string text = string.Format("M|%1 : %2\nL|Coche les soldats qui partent avec toi, puis envoie la demande.", kind, target);

		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);

		text += string.Format("\nH|Effectif : %1 soldat(s)", draft.m_aMembers.Count() + 1);
		for (int i = 0; i < ids.Count(); i++)
		{
			if (!records[i])
				continue;
			string line = SRP_Grades.GetName(records[i].m_iGrade) + " " + records[i].m_sName;
			if (ids[i] == playerId)
				text += "\nC|g|[x] " + line + " (toi)";
			else if (draft.m_aMembers.Contains(ids[i]))
				text += string.Format("\nK|g|membre:%1|[x] %2", ids[i], line);
			else
				text += string.Format("\nI|membre:%1|[ ] %2", ids[i], line);
		}
		text += "\nL| ";
		text += "\nK|y|envoyer|>>> Envoyer la demande (choisir puis Valider)";
		return text + "\nK|d|home|< Recommencer";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Join(array<string> items)
	{
		string text = "";
		foreach (string item : items)
		{
			if (!text.IsEmpty())
				text += ", ";
			text += item;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string TransportText(SRP_MissionRequest request)
	{
		if (!request.m_bMotorised)
			return "à pied";
		if (request.m_aVehicles.IsEmpty())
			return "motorisé";
		return "motorisé — " + Join(request.m_aVehicles);
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne de l'assistant (commande d'interface « demande »)
	static string ExecuteMenu(int playerId, string key)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		SRP_PlayerRecord me = players.GetRecord(playerId);
		if (!me)
			return "Fiche introuvable";

		SRP_RequestDraft draft;
		if (!s_mDrafts.Find(playerId, draft))
		{
			draft = new SRP_RequestDraft();
			s_mDrafts.Set(playerId, draft);
		}

		array<string> parts = {};
		key.Split(":", parts, true);
		string message = "";

		if (key == "annuler")
		{
			SRP_MissionRequest mine = FindOpenOf(me.m_sIdentity);
			if (mine)
			{
				string refusal = "";
				if (mine.m_iState == SRP_ERequestState.ACCORDEE)
				{
					SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
					string missionId = mine.m_sTarget;
					if (mine.m_bTerritory)
						missionId = mine.m_sMissionId;
					if (missions && !missionId.IsEmpty())
					{
						string reply = missions.Abandon(playerId, missionId);
						SRP_Mission still = missions.FindById(missionId);
						if (still && still.m_iState == SRP_EMissionState.ACTIVE)
							refusal = reply;	// capture engagée : plus de retour arrière
					}
				}
				if (refusal.IsEmpty())
				{
					Close(mine, SRP_ERequestState.ANNULEE, "annulée par " + me.m_sName);
					TellGroup(mine, "", string.Format("Demande %1 (%2) annulée par %3", mine.m_sId, mine.m_sTitle, me.m_sName));
					message = "Demande annulée";
				}
				else
					message = refusal;
			}
			draft.m_iPage = 0;
		}
		else if (key == "home")
		{
			draft.m_iPage = 0;
			draft.m_sTarget = "";
			draft.m_aMembers.Clear();
		}
		else if (parts.Count() == 2 && parts[0] == "type")
		{
			draft.m_bTerritory = parts[1] == "t";
			if (draft.m_bTerritory)
				draft.m_iPage = 2;
			else
				draft.m_iPage = 1;
		}
		else if (parts.Count() == 3 && parts[0] == "cible")
		{
			draft.m_bTerritory = parts[1] == "t";
			draft.m_sTarget = "";
			if (draft.m_bTerritory)
			{
				// La clé est le code de la zone (« S07 ») ; on garde le code exact rendu par le front
				SRP_FrontComponent front = ReadyFront();
				int chosenZone = ZoneOf(parts[2]);
				if (front && chosenZone >= 0)
					draft.m_sTarget = front.GetZoneCode(chosenZone);
			}
			else
				draft.m_sTarget = parts[2];

			if (draft.m_sTarget.IsEmpty())
				message = "Cible introuvable";
			else
				draft.m_iPage = 3;
		}
		else if (parts.Count() == 2 && parts[0] == "membre" && SRP_Utils.IsNumeric(parts[1]))
		{
			int memberId = parts[1].ToInt();
			if (memberId != playerId)
			{
				int at = draft.m_aMembers.Find(memberId);
				if (at >= 0)
					draft.m_aMembers.RemoveOrdered(at);
				else if (players.GetRecord(memberId))
					draft.m_aMembers.Insert(memberId);
			}
		}
		else if (key == "envoyer")
			message = Submit(playerId, me, draft);
		else if (key == "trouver")
			draft.m_iPage = 4;
		else if (parts.Count() == 2 && parts[0] == "bande" && SRP_Utils.IsNumeric(parts[1]))
		{
			SRP_MissionManagerComponent finder = SRP_MissionManagerComponent.GetInstance();
			if (finder)
				message = finder.FindForPlayer(playerId, parts[1].ToInt());
			draft.m_iPage = 0;
		}
		else if (key == "ordre")
		{
			draft.m_iOrderType = -1;
			draft.m_iOrderPlace = -1;
			draft.m_sOrderZone = "";
			if (CanOrder(me))
				draft.m_iPage = 5;
			else
				message = "Ordonner une mission est réservé aux officiers";
		}
		else if (parts.Count() == 2 && parts[0] == "otype" && CanOrder(me))
		{
			if (parts[1] == "c")
				draft.m_iOrderType = -2;
			else
				draft.m_iOrderType = parts[1].ToInt();
			draft.m_iOrderPlace = -1;
			draft.m_sOrderZone = "";
			draft.m_iPage = 6;
		}
		else if (parts.Count() == 2 && parts[0] == "ozone" && CanOrder(me))
		{
			// Capture ordonnée : la zone par son code (J4 revérifié par CreateCapture au dernier écran)
			if (ZoneOf(parts[1]) < 0)
				message = "Zone introuvable";
			else
			{
				draft.m_sOrderZone = parts[1];
				draft.m_iPage = 7;
			}
		}
		else if (parts.Count() == 2 && parts[0] == "olieu" && SRP_Utils.IsNumeric(parts[1]) && CanOrder(me))	// localités (missions du tableau)
		{
			draft.m_iOrderPlace = parts[1].ToInt();
			draft.m_iPage = 7;
		}
		else if (parts.Count() == 2 && parts[0] == "ochef" && SRP_Utils.IsNumeric(parts[1]) && CanOrder(me))
		{
			message = RunOrder(me, draft, parts[1].ToInt());
			draft.m_iPage = 0;
		}
		else
			message = "Ligne inconnue";

		Send(playerId, message);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Submit(int playerId, SRP_PlayerRecord me, SRP_RequestDraft draft)
	{
		if (FindOpenOf(me.m_sIdentity))
			return "Tu as déjà une demande en cours";
		if (draft.m_sTarget.IsEmpty())
			return "Choisis d'abord une mission ou une zone";
		if (IsTargeted(draft.m_bTerritory, draft.m_sTarget))
			return "Une demande existe déjà pour cette cible";

		SRP_MissionRequest request = new SRP_MissionRequest();
		request.m_bTerritory = draft.m_bTerritory;
		request.m_sTarget = draft.m_sTarget;

		// Zone : ennemie, hors base, au contact du front (J4). La limite G3 n'est PAS vérifiée ici : une demande peut
		// attendre qu'une capture se libère
		if (draft.m_bTerritory)
		{
			SRP_FrontComponent front = ReadyFront();
			int zone = ZoneOf(draft.m_sTarget);
			if (!front || zone < 0 || front.IsBaseZone(zone) || front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
				return "Cette zone n'est plus à capturer";
			if (!front.IsZoneInContact(zone))
				return "Cette zone n'est plus au contact du front";
			string zoneCode = front.GetZoneCode(zone);
			SRP_MissionManagerComponent captureMissions = SRP_MissionManagerComponent.GetInstance();
			if (captureMissions && captureMissions.HasActiveCapture(zoneCode))
				return "Une capture de cette zone est déjà en cours";
			request.m_sTarget = zoneCode;
			request.m_sTitle = front.GetZoneLabel(zone);
			request.m_sDetails = "Zone tenue par l'ennemi, au contact du front";
			request.m_sGrid = SRP_FleetManagerComponent.GridOf(front.GetMissionPoint(zone));
		}
		else
		{
			SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
			SRP_Mission mission;
			if (missions)
				mission = missions.FindById(draft.m_sTarget);
			if (!mission || mission.m_iState != SRP_EMissionState.ACTIVE || !mission.m_sClaimedBy.IsEmpty())
				return "Cette mission n'est plus disponible";
			request.m_sTarget = mission.m_sId;
			request.m_sTitle = mission.m_sTitle;
			request.m_sDetails = string.Format("%1 — %2, difficulté %3, prime %4 €", mission.m_sId, mission.m_sSiteLabel, mission.m_iDifficulty, mission.m_iRewardEuros);
			request.m_sGrid = SRP_FleetManagerComponent.GridOf(mission.m_vSite);
		}

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		request.m_sId = NextId();
		request.m_sRequester = SRP_Grades.GetName(me.m_iGrade) + " " + me.m_sName;
		request.m_sRequesterIdentity = me.m_sIdentity;
		request.m_sCreated = SRP_Time.Now();
		request.m_aMemberNames.Insert(request.m_sRequester);
		request.m_aMemberIdentities.Insert(me.m_sIdentity);
		foreach (int memberId : draft.m_aMembers)
		{
			SRP_PlayerRecord member = players.GetRecord(memberId);
			if (!member || member.m_sIdentity == me.m_sIdentity)
				continue;
			request.m_aMemberNames.Insert(SRP_Grades.GetName(member.m_iGrade) + " " + member.m_sName);
			request.m_aMemberIdentities.Insert(member.m_sIdentity);
		}
		s_aRequests.Insert(request);

		draft.m_iPage = 0;
		draft.m_sTarget = "";
		draft.m_aMembers.Clear();

		string text = string.Format("Demande %1 de %2 : %3 « %4 », %5 soldat(s)", request.m_sId, request.m_sRequester, request.KindText(), request.m_sTitle, request.m_aMemberNames.Count());
		SRP_JournalComponent.Log("MISSION", text + " — " + Join(request.m_aMemberNames));
		players.NotifyOfficiers(text + ". À traiter sur Discord, salon commandement.");
		TellGroup(request, "", string.Format("%1 demande au commandement : %2 « %3 ». Tu fais partie de l'effectif.", request.m_sRequester, request.KindText(), request.m_sTitle));

		if (!SRP_BridgeComponent.GetInstance())
			return "Demande " + request.m_sId + " enregistrée, mais la passerelle Discord est absente : préviens un officier";
		return "Demande " + request.m_sId + " envoyée au commandement. Tu seras prévenu de la décision.";
	}

	//------------------------------------------------------------------------------------------------
	// Décisions venues de Discord (SRP_BridgeComponent, commande « demande »)
	//------------------------------------------------------------------------------------------------
	static string ExecuteBridge(string author, array<string> tokens, out bool ok)
	{
		ok = false;
		if (tokens.Count() < 3)
			return "usage : demande assigner|liberer|accorder|refuser|clore <id> …";

		string action = tokens[1];
		action.ToLower();
		SRP_MissionRequest request = FindById(tokens[2]);
		if (!request)
			return "demande inconnue (le serveur a peut-être redémarré) : " + tokens[2];

		string rest = "";
		for (int i = 3; i < tokens.Count(); i++)
		{
			if (!rest.IsEmpty())
				rest += " ";
			rest += tokens[i];
		}

		if (action == "assigner")
		{
			if (request.m_iState == SRP_ERequestState.ASSIGNEE && request.m_sOfficer != author)
				return "déjà prise par " + request.m_sOfficer;
			if (request.m_iState != SRP_ERequestState.EN_ATTENTE && request.m_iState != SRP_ERequestState.ASSIGNEE)
				return "demande déjà " + StateLabel(request.m_iState);
			request.m_iState = SRP_ERequestState.ASSIGNEE;
			request.m_sOfficer = author;
			TellGroup(request, "", string.Format("Demande %1 : %2 examine ta demande", request.m_sId, author));
			ok = true;
			return "assignation prise";
		}

		if (action == "liberer")
		{
			if (request.m_iState != SRP_ERequestState.ASSIGNEE)
				return "rien à rendre : demande " + StateLabel(request.m_iState);
			request.m_iState = SRP_ERequestState.EN_ATTENTE;
			request.m_sOfficer = "";
			ok = true;
			return "assignation rendue";
		}

		if (action == "refuser")
		{
			if (request.m_iState != SRP_ERequestState.EN_ATTENTE && request.m_iState != SRP_ERequestState.ASSIGNEE)
				return "demande déjà " + StateLabel(request.m_iState);
			request.m_sOfficer = author;
			if (rest.IsEmpty())
				rest = "sans motif";
			Close(request, SRP_ERequestState.REFUSEE, rest);
			TellGroup(request, "Demande refusée", string.Format("%1 refuse la %2 « %3 ».\nMotif : %4", author, request.KindText(), request.m_sTitle, rest));
			ok = true;
			return "demande refusée";
		}

		if (action == "clore")
		{
			if (request.m_iState != SRP_ERequestState.ACCORDEE)
				return "seule une opération en cours se clôt : demande " + StateLabel(request.m_iState);
			if (rest.IsEmpty())
				rest = "close par " + author;
			Close(request, SRP_ERequestState.ECHOUEE, rest);
			TellGroup(request, "Opération close", string.Format("%1 met fin à l'opération « %2 ».\n%3", author, request.m_sTitle, rest));
			ok = true;
			return "opération close";
		}

		if (action == "accorder")
			return Grant(request, author, tokens, ok);

		return "action inconnue : " + action;
	}

	//------------------------------------------------------------------------------------------------
	//! demande accorder <id> <pied|moto> <véhicules séparés par des virgules, ou -> [consignes]
	protected static string Grant(SRP_MissionRequest request, string author, array<string> tokens, out bool ok)
	{
		if (request.m_iState != SRP_ERequestState.EN_ATTENTE && request.m_iState != SRP_ERequestState.ASSIGNEE)
			return "demande déjà " + StateLabel(request.m_iState);
		if (tokens.Count() < 5)
			return "usage : demande accorder <id> <pied|moto> <véhicules ou -> [consignes]";

		int requesterId = PlayerIdOf(request.m_sRequesterIdentity);
		if (requesterId < 0)
			return "le demandeur n'est plus en ligne : demande laissée en attente";

		// Cible toujours valable ?
		if (request.m_bTerritory)
		{
			SRP_FrontComponent front = ReadyFront();
			if (!front)
				return "le front n'est pas encore prêt : demande laissée en attente";
			int zone = ZoneOf(request.m_sTarget);
			if (zone < 0)
			{
				Close(request, SRP_ERequestState.ANNULEE, "zone introuvable");
				return "cette zone n'existe plus : demande close";
			}
			if (front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
			{
				Close(request, SRP_ERequestState.ANNULEE, "zone déjà à nous");
				return "cette zone est déjà à nous : demande close";
			}
			// La capture devient une mission au nom du demandeur : engagée à l'entrée dans la zone, suivie jusqu'au bout
			SRP_MissionManagerComponent captureManager = SRP_MissionManagerComponent.GetInstance();
			if (!captureManager)
				return "gestionnaire de missions absent du game mode";
			// Une capture ordonnée entre-temps garde son chef : la demande n'a plus d'objet
			if (captureManager.HasActiveCapture(request.m_sTarget))
			{
				Close(request, SRP_ERequestState.ANNULEE, "capture de la zone déjà en cours");
				return "une capture de cette zone est déjà en cours : demande close";
			}
			// Refus de CreateCapture (gel J3, zone plus au contact J4, plafond G3…) : pas de clôture, le front ou la limite
			// peuvent se libérer
			string refusal = "";
			SRP_Mission capture = captureManager.CreateCapture(request.m_sTarget, requesterId, author + " (Discord)", refusal);
			if (!capture)
				return "capture impossible (" + refusal + ") : demande laissée en attente";
			request.m_sMissionId = capture.m_sId;
		}
		else
		{
			SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
			if (!missions)
				return "gestionnaire de missions absent du game mode";
			SRP_Mission mission = missions.FindById(request.m_sTarget);
			if (!mission || mission.m_iState != SRP_EMissionState.ACTIVE)
			{
				Close(request, SRP_ERequestState.ANNULEE, "mission plus disponible");
				return "cette mission n'est plus au tableau : demande close";
			}
			if (mission.m_sClaimedBy.IsEmpty())
				missions.Claim(requesterId, request.m_sTarget, true);
			if (mission.m_iClaimedById != requesterId)
			{
				Close(request, SRP_ERequestState.ANNULEE, "mission déjà acceptée par " + mission.m_sClaimedBy);
				return "mission déjà acceptée par " + mission.m_sClaimedBy + " : demande close";
			}
		}

		string mode = tokens[3];
		mode.ToLower();
		request.m_bMotorised = mode == "moto";
		request.m_aVehicles.Clear();
		if (request.m_bMotorised && tokens[4] != "-")
		{
			SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
			array<SRP_FleetVehicle> vehicles = {};
			if (fleet)
				fleet.GetVehicles(vehicles);
			array<string> wanted = {};
			tokens[4].Split(",", wanted, true);
			foreach (string vehicleId : wanted)
			{
				foreach (SRP_FleetVehicle vehicle : vehicles)
				{
					if (vehicle.m_sId == vehicleId)
						request.m_aVehicles.Insert(vehicle.m_sName + " n° " + vehicle.m_sId);
				}
			}
		}

		string note = "";
		for (int i = 5; i < tokens.Count(); i++)
		{
			if (!note.IsEmpty())
				note += " ";
			note += tokens[i];
		}
		request.m_sNote = note;
		request.m_sOfficer = author;
		request.m_iState = SRP_ERequestState.ACCORDEE;

		string text = string.Format("%1 accorde la %2 « %3 ».\nTransport : %4", author, request.KindText(), request.m_sTitle, TransportText(request));
		if (!request.m_sGrid.IsEmpty())
			text += "\nCarré : " + request.m_sGrid;
		if (!note.IsEmpty())
			text += "\nConsignes : " + note;
		text += "\nEffectif : " + Join(request.m_aMemberNames);
		TellGroup(request, "Mission accordée", text);

		SRP_JournalComponent.Log("MISSION", string.Format("Demande %1 accordée par %2 (Discord) : %3 « %4 », transport %5, effectif %6", request.m_sId, author, request.KindText(), request.m_sTitle, TransportText(request), Join(request.m_aMemberNames)));
		ok = true;
		return "demande accordée, le groupe est prévenu en jeu";
	}

	//------------------------------------------------------------------------------------------------
	// Suivi (toutes les 5 s, depuis SRP_PlayerManagerComponent.RefreshPositions)
	//------------------------------------------------------------------------------------------------
	static void Tick()
	{
		if (!Replication.IsServer() || s_aRequests.IsEmpty())
			return;

		int now = System.GetTickCount();
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();

		for (int i = s_aRequests.Count() - 1; i >= 0; i--)
		{
			SRP_MissionRequest request = s_aRequests[i];
			if (!request.IsOpen())
			{
				if (now - request.m_iClosedTick > KEEP_CLOSED_MS)
					s_aRequests.RemoveOrdered(i);
				continue;
			}

			bool waiting = request.m_iState != SRP_ERequestState.ACCORDEE;

			// La cible a-t-elle bougé ?
			if (request.m_bTerritory)
			{
				SRP_FrontComponent front = ReadyFront();
				int zone = ZoneOf(request.m_sTarget);
				if (front && zone >= 0 && front.GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
				{
					SRP_Mission taken = null;
					if (missions && !request.m_sMissionId.IsEmpty())
						taken = missions.FindById(request.m_sMissionId);
					if (waiting)
						Close(request, SRP_ERequestState.ANNULEE, "zone prise entre-temps");
					else if (taken && taken.m_iState == SRP_EMissionState.ANNULEE)
					{
						// Zone forcée bleue par le Staff (correction, Q9) : sa mission est annulée, rien à mettre au crédit
						Close(request, SRP_ERequestState.ANNULEE, "zone attribuée par l'état-major (correction)");
						TellGroup(request, "", string.Format("Opération « %1 » close : la zone a été attribuée par l'état-major.", request.m_sTitle));
					}
					else
					{
						Close(request, SRP_ERequestState.REUSSIE, "zone capturée");
						TellGroup(request, "", string.Format("Opération « %1 » réussie : zone prise. Compte rendu transmis au commandement.", request.m_sTitle));
					}
					continue;
				}
				if (!waiting && missions && !request.m_sMissionId.IsEmpty())
				{
					SRP_Mission capture = missions.FindById(request.m_sMissionId);
					if (!capture || capture.m_iState != SRP_EMissionState.ACTIVE)
					{
						Close(request, SRP_ERequestState.ECHOUEE, "capture échouée ou annulée (zone abandonnée ?)");
						TellGroup(request, "", string.Format("Opération « %1 » : capture échouée. Compte rendu transmis au commandement.", request.m_sTitle));
						continue;
					}
				}
			}
			else if (missions)
			{
				SRP_Mission mission = missions.FindById(request.m_sTarget);
				int missionState = SRP_EMissionState.EXPIREE;
				if (mission)
					missionState = mission.m_iState;
				if (missionState != SRP_EMissionState.ACTIVE)
				{
					if (waiting)
						Close(request, SRP_ERequestState.ANNULEE, "mission retirée du tableau avant la décision");
					else if (missionState == SRP_EMissionState.SUCCES)
						Close(request, SRP_ERequestState.REUSSIE, "mission réussie");
					else if (missionState == SRP_EMissionState.ANNULEE)
						Close(request, SRP_ERequestState.ECHOUEE, "mission annulée");
					else
						Close(request, SRP_ERequestState.ECHOUEE, "mission échouée");
					continue;
				}
				if (waiting && !mission.m_sClaimedBy.IsEmpty())
				{
					Close(request, SRP_ERequestState.ANNULEE, "mission acceptée entre-temps par " + mission.m_sClaimedBy);
					continue;
				}
			}

			// Demandeur parti depuis trop longtemps : une demande en attente n'a plus de sens
			if (waiting)
			{
				if (PlayerIdOf(request.m_sRequesterIdentity) >= 0)
					request.m_iRequesterGoneTick = 0;
				else if (request.m_iRequesterGoneTick == 0)
					request.m_iRequesterGoneTick = now;
				else if (now - request.m_iRequesterGoneTick > REQUESTER_GONE_MS)
					Close(request, SRP_ERequestState.ANNULEE, "demandeur déconnecté");
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les demandes pour la passerelle : tableau JSON (l'échappement vient de SRP_BridgeComponent.Esc)
	static string ToJson()
	{
		string json = "[";
		bool first = true;
		foreach (SRP_MissionRequest request : s_aRequests)
		{
			if (!first)
				json += ",";
			first = false;

			string kind = "mission";
			if (request.m_bTerritory)
				kind = "territoire";
			string transport = "";
			if (request.m_iState == SRP_ERequestState.ACCORDEE || request.m_iState == SRP_ERequestState.REUSSIE || request.m_iState == SRP_ERequestState.ECHOUEE)
				transport = TransportText(request);

			json += "{\"id\":\"" + SRP_BridgeComponent.Esc(request.m_sId) + "\"";
			json += ",\"kind\":\"" + kind + "\"";
			json += ",\"state\":\"" + StateName(request.m_iState) + "\"";
			json += ",\"target\":\"" + SRP_BridgeComponent.Esc(request.m_sTarget) + "\"";
			json += ",\"title\":\"" + SRP_BridgeComponent.Esc(request.m_sTitle) + "\"";
			json += ",\"details\":\"" + SRP_BridgeComponent.Esc(request.m_sDetails) + "\"";
			json += ",\"grid\":\"" + SRP_BridgeComponent.Esc(request.m_sGrid) + "\"";
			json += ",\"requester\":\"" + SRP_BridgeComponent.Esc(request.m_sRequester) + "\"";
			json += ",\"created\":\"" + SRP_BridgeComponent.Esc(request.m_sCreated) + "\"";
			json += ",\"officer\":\"" + SRP_BridgeComponent.Esc(request.m_sOfficer) + "\"";
			json += ",\"transport\":\"" + SRP_BridgeComponent.Esc(transport) + "\"";
			json += ",\"note\":\"" + SRP_BridgeComponent.Esc(request.m_sNote) + "\"";
			json += ",\"members\":[";
			for (int m = 0; m < request.m_aMemberNames.Count(); m++)
			{
				if (m > 0)
					json += ",";
				json += "\"" + SRP_BridgeComponent.Esc(request.m_aMemberNames[m]) + "\"";
			}
			json += "]}";
		}
		return json + "]";
	}
}

//------------------------------------------------------------------------------------------------
//! Action du tableau des missions
class SRP_MissionRequestAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;
		SRP_MissionRequests.OpenMenu(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Demander une mission";
		return true;
	}

	override bool CanBeShownScript(IEntity user) { return true; }
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! L'écran : même mécanique que le terminal. Un clic navigue ou coche ; envoyer et annuler demandent Valider.
class SRP_RequestMenu : SRP_ListMenu
{
	protected static SRP_RequestMenu s_Open;

	override protected string GetMenuTitle() { return "Demande de mission"; }
	override protected string GetConfirmLabel() { return "Valider"; }
	override protected string GetCommand() { return "demande"; }

	//------------------------------------------------------------------------------------------------
	static void Open(string content)
	{
		if (!s_Open)
		{
			s_Open = SRP_RequestMenu.Cast(GetGame().GetMenuManager().OpenDialog(ChimeraMenuPreset.SRP_RequestMenu));
			if (!s_Open)
			{
				Print("[SRP] Impossible d'ouvrir la demande de mission : preset SRP_RequestMenu absent de chimeraMenus.conf ?", LogLevel.ERROR);
				return;
			}
		}
		s_Open.Fill(content);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (s_Open == this)
			s_Open = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected bool NeedsConfirm(string key)
	{
		return key == "envoyer" || key == "annuler";
	}

	//------------------------------------------------------------------------------------------------
	override void Select(int index)
	{
		super.Select(index);
		string key = GetSelectedKey();
		if (!key.IsEmpty() && !NeedsConfirm(key))
			SendKey(key);
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnConfirm()
	{
		string key = GetSelectedKey();
		if (key.IsEmpty())
		{
			if (SRP_IsGamepad())
				SetMessage(ChooseHint());
			else
				SetMessage("Clique sur « Envoyer la demande », puis Valider");
			return;
		}
		// Souris : les options partent au clic, Valider ne sert qu'à envoyer ou annuler.
		// Manette : pas de clic, A valide la ligne qui a le focus, quelle qu'elle soit.
		if (!NeedsConfirm(key) && !SRP_IsGamepad())
		{
			SetMessage("Clique sur « Envoyer la demande », puis Valider");
			return;
		}
		SendKey(key);
	}

	//------------------------------------------------------------------------------------------------
	protected void SendKey(string key)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.SRP_SendMenuCommand(GetCommand(), key);
		SetMessage("…");
	}
}
