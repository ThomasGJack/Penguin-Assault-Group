//------------------------------------------------------------------------------------------------
// SimpleRP — Passerelle avec le bot Discord permanent (PAG-Bot)
// Le serveur de jeu ne peut pas recevoir de connexion : c'est lui qui appelle le bot, toutes les
// N secondes, avec un POST sur <bridge_url>api/sync :
//   - il ENVOIE l'état du serveur (joueurs en ligne avec grade et certifs, missions, territoire en zones,
//     trésorerie, dépôts), les résultats des commandes exécutées depuis le dernier contact et les
//     demandes de liaison Discord tapées en jeu (/lier <code>) ;
//   - front (cap « front », SRP_Front.c) : la grille des carrés, en instantané complet une fois par minute ou
//     à la demande, sinon les seuls changements depuis la version que le bot dit tenir ; l'état des zones
//     remarquables (zs) et les carrés contestés (cs) à chaque envoi, les zones forcées par le Staff (stf) ; rien
//     tant que la campagne n'a pas d'identifiant (front.json illisible) ; les zones à capturer (capture_targets) ;
//     les alertes portent le code de leur zone et ne sont retirées de la file qu'une fois reçues ;
//   - il REÇOIT en réponse les commandes en attente, une par ligne : id <tab> auteur <tab> commande,
//     et une ligne d'accusé du front : « #front <campagne> <version> » ou « #front plein ».
// Commandes acceptées (le bot ne les envoie qu'au Staff Discord) : ban, unban, kick, grade, certif,
// instructeur, staff, avertir, dire, mp, sauver, info, tp, soigner, etat (état médical), arme-recreer
// (arme hors service recréée). Les joueurs hors ligne sont gérés sur leur fiche.
// Réglages dans $profile:SimpleRP/discord.json :  "bridge_url": "http://IP:8787/", "bridge_secret": "…"
// Phase 6 — bot permanent
//------------------------------------------------------------------------------------------------

//! Le résultat d'une commande, renvoyé au bot au prochain contact
class SRP_BridgeResult
{
	string m_sId;
	bool m_bOk;
	string m_sMessage;
}

//------------------------------------------------------------------------------------------------
//! La requête en vol
class SRP_BridgeRequest : RestCallback
{
	protected SRP_BridgeComponent m_Owner;

	//------------------------------------------------------------------------------------------------
	void SRP_BridgeRequest(SRP_BridgeComponent owner)
	{
		m_Owner = owner;
		SetOnSuccess(OnDoneOk);
		SetOnError(OnDoneError);
	}

	//------------------------------------------------------------------------------------------------
	void OnDoneOk(RestCallback cb)
	{
		if (m_Owner)
			m_Owner.OnSyncDone(true, cb.GetHttpCode(), cb.GetData());
	}

	//------------------------------------------------------------------------------------------------
	void OnDoneError(RestCallback cb)
	{
		if (m_Owner)
			m_Owner.OnSyncDone(false, cb.GetHttpCode(), "");
	}
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Passerelle SimpleRP vers le bot Discord permanent (profil/SimpleRP/discord.json : bridge_url, bridge_secret)")]
class SRP_BridgeComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_BridgeComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Activer la passerelle vers le bot Discord (bridge_url et bridge_secret requis dans discord.json)", category: "SimpleRP")]
	protected bool m_bEnabled;

	[Attribute("5", UIWidgets.EditBox, "Contact avec le bot toutes les … secondes (état envoyé, commandes reçues ; 5 au minimum)", category: "SimpleRP")]
	protected int m_iSyncSeconds;

	[Attribute("12", UIWidgets.EditBox, "Front : instantané complet de la grille tous les … envois (12 x 5 s = 1 min), en plus des changements", category: "SimpleRP - Front")]
	protected int m_iFrontFullEvery;

	[Attribute("300", UIWidgets.EditBox, "Front : au-delà de … changements de carré en attente, la grille entière repart", category: "SimpleRP - Front")]
	protected int m_iFrontMaxDelta;

	[Attribute("2000", UIWidgets.EditBox, "Front : changements de carré gardés en attendant l'accusé du bot", category: "SimpleRP - Front")]
	protected int m_iFrontLogMax;

	[Attribute("40", UIWidgets.EditBox, "Alertes gardées en attente d'envoi au bot (au-delà, les plus anciennes sont jetées)", category: "SimpleRP - Front")]
	protected int m_iMaxEvents;

	protected int m_iMapPlacesCountdown;		// carte de situation : les localités ne partent qu'un envoi sur douze

	protected static SRP_BridgeComponent s_Instance;
	protected static const string CONFIG_FILE = "$profile:SimpleRP/discord.json";
	protected static const int MAX_ERRORS_BEFORE_NOTICE = 4;

	protected RestContext m_Context;
	protected string m_sSecret;
	protected ref SRP_BridgeRequest m_InFlight;
	protected ref array<ref SRP_BridgeResult> m_aResults = {};
	protected ref array<string> m_aLinks = {};				// "code|identité|pseudo"
	protected ref array<string> m_aVotes = {};				// demandes /vote en jeu : "code|identité|pseudo"
	protected int m_iVotesSent = 0;						// combien de ces demandes partent avec l'envoi en cours
	protected int m_iLinksSent = 0;						// idem pour les demandes de liaison
	protected ref map<string, string> m_mVoteCodes = new map<string, string>();		// identité -> code du lien de vote
	protected ref map<string, int> m_mVoteCodeTimes = new map<string, int>();		// identité -> heure du dernier /vote
	protected static const int MAX_VOTE_REQUESTS = 20;
	protected static const string VOTE_LINK = "penguinassaultgroup.fr/vote/";
	protected static ref RandomGenerator s_VoteRandom;
	protected ref array<string> m_aEventTypes = {};		// alertes en attente d'envoi au bot (type)
	protected ref array<string> m_aEventTexts = {};		// … leur texte
	protected ref array<string> m_aEventZones = {};		// … et le code de leur zone ("" sans zone)
	protected int m_iEventsSent = 0;					// combien de ces alertes partent avec l'envoi en cours

	// Front : journal des changements de carré en attendant l'accusé du bot (versions croissantes)
	protected ref array<int> m_aFrontLogVersions = {};
	protected ref array<int> m_aFrontLogCells = {};
	protected ref array<int> m_aFrontLogOwners = {};
	protected int m_iFrontLogFloor = -1;				// le journal est complet pour toutes les versions au-dessus
	protected int m_iFrontAck = -1;						// la version que le bot dit tenir
	protected string m_sFrontAckId = "";				// … et sa campagne
	protected bool m_bFrontFullWanted = true;			// prochain envoi : grille entière
	protected int m_iFrontCountdown = 0;				// envois avant le prochain instantané complet
	protected string m_sFrontZoneMap = "";				// caches : le découpage ne change pas tant que le serveur tourne
	protected string m_sFrontZonesJson = "";
	protected int m_iFrontGeomVersion = -1;				// version de géométrie des caches

	// Front : zones forcées ou remises par le Staff (correction, Q9) en attendant un envoi reçu, clé « stf » : le bot
	// n'en fait ni mouvement ni photo. Sans doublon parmi celles pas encore parties, MAX_STAFF_ZONES au plus
	protected static ref array<string> s_aStaffZones = {};
	protected static int s_iStaffSent = 0;				// combien de ces zones partent avec l'envoi en cours
	protected static const int MAX_STAFF_ZONES = 80;

	protected int m_iErrors = 0;
	protected bool m_bReady = false;
	protected bool m_bOnline = false;

	//------------------------------------------------------------------------------------------------
	static SRP_BridgeComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		// Liste statique : ce qu'une partie précédente y a laissé n'a plus de sens
		s_aStaffZones.Clear();
		s_iStaffSent = 0;

		if (!Replication.IsServer() || !m_bEnabled)
			return;

		if (!LoadConfig())
			return;

		RestApi api = GetGame().GetRestApi();
		if (!api)
		{
			SRP_JournalComponent.Log("ERREUR", "Passerelle bot : API REST indisponible");
			return;
		}
		m_Context.SetHeaders("Content-Type,application/json");
		m_Context.SetTimeout(10);

		if (m_iSyncSeconds < 5)
			m_iSyncSeconds = 5;

		m_bReady = true;
		GetGame().GetCallqueue().CallLater(Sync, m_iSyncSeconds * 1000, true);
		SRP_JournalComponent.Log("SYSTEME", string.Format("Passerelle bot Discord démarrée : contact toutes les %1 s", m_iSyncSeconds));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bReady)
			GetGame().GetCallqueue().Remove(Sync);
		m_bReady = false;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	protected bool LoadConfig()
	{
		if (!FileIO.FileExists(CONFIG_FILE))
			return false;

		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(CONFIG_FILE))
			return false;

		string url = "";
		ctx.ReadValue("bridge_url", url);
		ctx.ReadValue("bridge_secret", m_sSecret);
		url = url.Trim();
		if (url.IsEmpty() || m_sSecret.IsEmpty())
		{
			SRP_JournalComponent.Log("SYSTEME", "Passerelle bot : bridge_url ou bridge_secret absent de discord.json, passerelle inactive");
			return false;
		}
		if (url.Substring(url.Length() - 1, 1) != "/")
			url += "/";

		RestApi api = GetGame().GetRestApi();
		if (!api)
			return false;
		m_Context = api.GetContext(url);
		if (!m_Context)
		{
			SRP_JournalComponent.Log("ERREUR", "Passerelle bot : contexte REST introuvable pour " + url);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool IsOnline()
	{
		return m_bReady && m_bOnline;
	}

	//------------------------------------------------------------------------------------------------
	//! /lier <code> en jeu : la demande part au bot au prochain contact
	static string Link(SRP_PlayerRecord me, array<string> tokens)
	{
		if (!s_Instance || !s_Instance.m_bReady)
			return "La passerelle Discord n'est pas active sur ce serveur";
		if (!me || tokens.IsEmpty())
			return "Usage : /lier <code>  (le code vient de la commande /lier du bot Discord)";

		string code = tokens[0];
		code.ToUpper();
		s_Instance.m_aLinks.Insert(code + "|" + me.m_sIdentity + "|" + me.m_sName);
		return "Demande envoyée : ton compte Discord sera lié dans quelques secondes si le code est bon";
	}

	//------------------------------------------------------------------------------------------------
	//! /vote en jeu : un lien court et personnel vers la page de vote top-serveurs, le pseudo déjà rempli. La demande
	//! part au bot avec l'identité du joueur : le vote est rattaché à sa fiche, et le bot ne le compte qu'une fois.
	static string RequestVote(SRP_PlayerRecord me)
	{
		if (!s_Instance || !s_Instance.m_bReady)
			return "Le bot Discord n'est pas relié à ce serveur : vote indisponible pour l'instant";
		if (!me || me.m_sIdentity.IsEmpty())
			return "Fiche introuvable, réessaie dans quelques secondes";

		// Même soldat, même lien pendant 2 h : un /vote retapé redonne le code déjà donné et prolonge sa validité
		string code;
		int now = System.GetUnixTime();
		if (!s_Instance.m_mVoteCodes.Find(me.m_sIdentity, code) || now - s_Instance.m_mVoteCodeTimes.Get(me.m_sIdentity) > 7000)
			code = NewVoteCode();
		s_Instance.m_mVoteCodes.Set(me.m_sIdentity, code);
		s_Instance.m_mVoteCodeTimes.Set(me.m_sIdentity, now);

		// Une seule demande de ce soldat en attente d'envoi ; la file est bornée
		string request = code + "|" + me.m_sIdentity + "|" + me.m_sName;
		bool alreadyQueued = false;
		for (int queueIndex = s_Instance.m_iVotesSent; queueIndex < s_Instance.m_aVotes.Count(); queueIndex++)
		{
			if (s_Instance.m_aVotes[queueIndex] == request)
				alreadyQueued = true;
		}
		if (!alreadyQueued)
		{
			if (s_Instance.m_aVotes.Count() >= MAX_VOTE_REQUESTS)
				return "Trop de demandes de vote en attente, réessaie dans une minute";
			s_Instance.m_aVotes.Insert(request);
		}
		string text = "Ton lien de vote pour la PAG : " + VOTE_LINK + code;
		text += "\nOuvre-le dans ton navigateur : ton pseudo « " + me.m_sName + " » y est déjà rempli, ne le change pas.";
		text += "\nUn vote toutes les 2 h. Chaque vote compte sur ta fiche et verse une prime au coffre de la base. Lien valable 2 h.";
		if (!s_Instance.m_bOnline)
			text += "\nLe bot est injoignable en ce moment : le lien marchera dès son retour.";
		SRP_JournalComponent.Log("VOTE", string.Format("%1 [%2] demande son lien de vote (%3)", me.m_sName, me.m_sIdentity, code));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Code du lien de vote : 5 caractères sans 0/O ni 1/I, tirés d'un générateur propre à la passerelle
	protected static string NewVoteCode()
	{
		if (!s_VoteRandom)
		{
			s_VoteRandom = new RandomGenerator();
			s_VoteRandom.SetSeed(System.GetUnixTime());
		}
		string letters = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
		string code = "";
		for (int charIndex = 0; charIndex < 5; charIndex++)
			code += letters.Substring(s_VoteRandom.RandInt(0, letters.Length()), 1);
		return code;
	}

	//------------------------------------------------------------------------------------------------
	// Envoi de l'état
	//------------------------------------------------------------------------------------------------
	//! Une alerte à publier sur Discord (salon des alertes), sans zone. Types : crime, vehicule, mission, sanction,
	//! controle, info (les anciens attaque, defense, secteur_pris et secteur_perdu ne sont plus émis). Sans
	//! passerelle active, l'appel ne fait rien.
	static void PushEvent(string type, string text)
	{
		PushZoneEvent(type, "", text);
	}

	//------------------------------------------------------------------------------------------------
	//! Une alerte du front, avec le code de sa zone (« S07 », "" sans zone) — SRP_FrontRadio seul (liste unique,
	//! CONTRATS_FRONT §6) : zone_prise, zone_perdue, offensive, zone_attaque, zone_assaut, zone_defendue,
	//! zone_coupee, zone_reliee, victoire, campagne, front, commandement, depot_detruit ; Commandeur :
	//! officier_tombe, region_liberee, renseignement, depot_ennemi, blinde_ramene. Même file que PushEvent
	//! (m_iMaxEvents au plus) ; une alerte n'en sort qu'une fois reçue par le bot.
	static void PushZoneEvent(string type, string zoneCode, string text)
	{
		if (!s_Instance || !s_Instance.m_bReady)
			return;
		s_Instance.AddEvent(type, zoneCode, text);
	}

	//------------------------------------------------------------------------------------------------
	//! File pleine : la plus ancienne est jetée ; si elle faisait partie de l'envoi en vol, cet envoi en compte une
	//! de moins (sinon la réponse retirerait une alerte qui n'est pas partie)
	protected void AddEvent(string type, string zoneCode, string text)
	{
		int maxEvents = Math.MaxInt(m_iMaxEvents, 1);
		while (m_aEventTypes.Count() >= maxEvents)
		{
			m_aEventTypes.RemoveOrdered(0);
			m_aEventTexts.RemoveOrdered(0);
			m_aEventZones.RemoveOrdered(0);
			if (m_iEventsSent > 0)
				m_iEventsSent--;
		}
		m_aEventTypes.Insert(type);
		m_aEventTexts.Insert(text);
		m_aEventZones.Insert(zoneCode);
	}

	//------------------------------------------------------------------------------------------------
	//! Un carré a changé de camp (toute cause) : noté avec la version du pont qu'il porte, pour l'envoi des seuls
	//! changements au bot — SRP_FrontComponent.SetCellsOwner, à CHAQUE carré (après l'économie, avant la radio)
	static void NoteFrontCell(int cell, int owner, int version)
	{
		if (!s_Instance || !s_Instance.m_bReady)
			return;
		s_Instance.AddFrontLog(cell, owner, version);
	}

	//------------------------------------------------------------------------------------------------
	//! Journal plein : l'entrée la plus ancienne sort, le journal n'est plus complet que pour les versions
	//! au-dessus d'elle (un bot resté en deçà recevra la grille entière)
	protected void AddFrontLog(int cell, int owner, int version)
	{
		int logMax = Math.MaxInt(m_iFrontLogMax, 1);
		while (m_aFrontLogVersions.Count() >= logMax)
		{
			m_iFrontLogFloor = m_aFrontLogVersions[0];
			m_aFrontLogVersions.RemoveOrdered(0);
			m_aFrontLogCells.RemoveOrdered(0);
			m_aFrontLogOwners.RemoveOrdered(0);
		}
		m_aFrontLogVersions.Insert(version);
		m_aFrontLogCells.Insert(cell);
		m_aFrontLogOwners.Insert(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à plat du front (nouvelle campagne, remise d'une zone, restauration d'une copie datée), appelée APRÈS
	//! la remise en place : journal vidé, complet à partir de la version courante, grille entière au prochain envoi
	//! — SRP_FrontComponent (ResetCampaign, ResetZone, RestoreDaily)
	static void NoteFrontReset()
	{
		if (!s_Instance || !s_Instance.m_bReady)
			return;
		s_Instance.ResetFrontLog();
	}

	//------------------------------------------------------------------------------------------------
	//! Une zone forcée ou remise par le Staff (correction, Q9) : son code part au bot dans « stf », avec l'état qui suit,
	//! pour qu'il n'en fasse ni mouvement « prise / perdue » ni photo — SRP_ZoneFall.ForceZone, SRP_FrontComponent.ResetZone.
	//! Sans passerelle active (pas de bridge_url), rien n'est noté ; la liste ne se vide qu'une fois l'envoi reçu
	static void NoteStaffZone(string code)
	{
		if (!s_Instance || !s_Instance.m_bReady || code.IsEmpty())
			return;

		// Déjà en attente d'envoi : rien à ajouter. Une zone qui fait partie de l'envoi en vol est notée à nouveau :
		// son nouvel état ne partira qu'avec le suivant
		for (int staffIndex = s_iStaffSent; staffIndex < s_aStaffZones.Count(); staffIndex++)
		{
			if (s_aStaffZones[staffIndex] == code)
				return;
		}

		// Liste pleine : les plus anciennes sortent ; si l'une faisait partie de l'envoi en vol, cet envoi en compte une
		// de moins (sinon la réponse retirerait une zone qui n'est pas partie)
		while (s_aStaffZones.Count() >= MAX_STAFF_ZONES)
		{
			s_aStaffZones.RemoveOrdered(0);
			if (s_iStaffSent > 0)
				s_iStaffSent--;
		}
		s_aStaffZones.Insert(code);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetFrontLog()
	{
		m_aFrontLogVersions.Clear();
		m_aFrontLogCells.Clear();
		m_aFrontLogOwners.Clear();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			m_iFrontLogFloor = front.GetBridgeVersion();
		m_bFrontFullWanted = true;
		m_sFrontZonesJson = "";
	}

	//------------------------------------------------------------------------------------------------
	protected void Sync()
	{
		if (!m_bReady || m_InFlight)
			return;

		m_InFlight = new SRP_BridgeRequest(this);
		m_Context.POST(m_InFlight, "api/sync", BuildState());
	}

	//------------------------------------------------------------------------------------------------
	//! Échappe une chaîne pour JSON
	static string Esc(string s)
	{
		string r = s;
		r.Replace("\\", "\\\\");
		r.Replace("\"", "\\\"");
		r.Replace("\n", "\\n");
		r.Replace("\r", "");
		r.Replace("\t", " ");
		return r;
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildState()
	{
		string json = "{\"secret\":\"" + Esc(m_sSecret) + "\"";
		json += ",\"time\":\"" + Esc(SRP_Time.Now()) + "\"";
		// Ce que ce mod sait faire : le bot n'affiche que les boutons correspondants
		json += ",\"caps\":[\"cible\",\"tp\",\"soigner\",\"etat\",\"arme\",\"heure\",\"mission\",\"radio\",\"demande\",\"ordonner\",\"vote\",\"vote2\",\"front\"]";

		// Joueurs en ligne
		SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		json += ",\"players\":[";
		bool first = true;
		foreach (int id : ids)
		{
			SRP_PlayerRecord record;
			if (manager)
				record = manager.GetRecord(id);
			string name = GetGame().GetPlayerManager().GetPlayerName(id);
			if (!first)
				json += ",";
			first = false;
			if (record)
			{
				json += "{\"name\":\"" + Esc(record.m_sName) + "\",\"identity\":\"" + Esc(record.m_sIdentity) + "\"";
				json += ",\"grade\":" + record.m_iGrade.ToString() + ",\"gradeName\":\"" + Esc(SRP_Grades.GetName(record.m_iGrade)) + "\"";
				json += ",\"certifs\":\"" + Esc(SRP_Certifs.MaskToString(record.m_iCertifs)) + "\"";
				json += ",\"instructeur\":\"" + Esc(SRP_Certifs.MaskToString(record.m_iInstructeurCertifs)) + "\"";
				int minutes = record.m_iPlaytimeSeconds / 60;
				json += ",\"staff\":" + BoolText(record.m_bStaff) + ",\"minutes\":" + minutes.ToString();
				json += ",\"missions\":" + record.m_iMissions.ToString() + ",\"infractions\":" + record.m_iInfractions.ToString() + ",\"votes\":" + record.m_iVotes.ToString();
				IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
				if (controlled)
					json += ",\"grid\":\"" + SRP_FleetManagerComponent.GridOf(controlled.GetOrigin()) + "\"";
				json += "}";
			}
			else
			{
				json += "{\"name\":\"" + Esc(name) + "\",\"identity\":\"\",\"grade\":0,\"gradeName\":\"Recrue\",\"certifs\":\"\",\"instructeur\":\"\",\"staff\":false,\"minutes\":0}";
			}
		}
		json += "]";

		// Missions
		json += ",\"missions\":[";
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
		{
			array<SRP_Mission> active = {};
			missions.GetActiveMissions(active);
			first = true;
			foreach (SRP_Mission mission : active)
			{
				if (!first)
					json += ",";
				first = false;
				json += "{\"id\":\"" + Esc(mission.m_sId) + "\",\"title\":\"" + Esc(mission.m_sTitle) + "\",\"site\":\"" + Esc(mission.m_sSiteLabel) + "\",\"difficulty\":" + mission.m_iDifficulty.ToString();
				// Pour la carte de situation : le carré de 100 m du site, et qui a pris la mission
				json += ",\"grid\":\"" + SRP_FleetManagerComponent.GridOf(mission.m_vSite) + "\",\"claimed\":\"" + Esc(mission.m_sClaimedBy) + "\"}";
			}
		}
		json += "]";

		// Territoire, compté en ZONES du front (les clés ours, total et threat sont gardées : le bot les lit) ; blue et
		// land en carrés ; campaign et campaign_n : information (le bot archive sur l'id du bloc front)
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		bool frontReady = (front != null) && front.IsReady();
		if (frontReady)
		{
			int zonesOurs = front.CountZonesOurs();
			int zonesTotal = front.CountZonesTotal();
			int threat = front.GetThreat();
			int blueCells = front.CountBlueCells();
			int landCells = front.CountLandCells();
			int campaignNumber = front.GetCampaign();
			json += ",\"territory\":{\"ours\":" + zonesOurs.ToString() + ",\"total\":" + zonesTotal.ToString() + ",\"threat\":" + threat.ToString();
			json += ",\"blue\":" + blueCells.ToString() + ",\"land\":" + landCells.ToString() + ",\"frozen\":" + BoolText(front.IsFrozen());
			json += ",\"campaign\":\"" + Esc(front.GetCampaignId()) + "\",\"campaign_n\":" + campaignNumber.ToString() + "}";
		}
		else
		{
			// Front pas encore construit (premières secondes) : des zéros, comme avant sans territoire
			json += ",\"territory\":{\"ours\":0,\"total\":0,\"threat\":0}";
		}

		// Carte de situation web : la grille du front (remplace l'ancienne liste des secteurs) ; les zones Staff ne
		// comptent comme parties que si BuildFront les écrit
		s_iStaffSent = 0;
		if (frontReady)
			json += BuildFront(front);

		// Zones ennemies au contact qu'on peut donner en capture (J4) : code et libellé, une dizaine au plus
		json += ",\"capture_targets\":[";
		if (frontReady && missions)
		{
			array<int> targets = {};
			missions.GetCaptureTargets(targets);
			foreach (int targetIndex, int targetZone : targets)
			{
				if (targetIndex > 0)
					json += ",";
				json += "{\"c\":\"" + Esc(front.GetZoneCode(targetZone)) + "\",\"l\":\"" + Esc(front.GetZoneLabel(targetZone)) + "\"}";
			}
		}
		json += "]";

		// L'heure en jeu
		ChimeraWorld mapWorld = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (mapWorld && mapWorld.GetTimeAndWeatherManager())
			json += ",\"hour\":\"" + Esc(SRP_DayNightComponent.HourText(mapWorld.GetTimeAndWeatherManager().GetTimeOfTheDay())) + "\"";

		// Les localités (noms posés sur la carte) : elles ne bougent pas, une fois par minute suffit
		m_iMapPlacesCountdown--;
		if (m_iMapPlacesCountdown <= 0)
		{
			m_iMapPlacesCountdown = 12;
			json += ",\"mission_types\":[";
			SRP_MissionManagerComponent orderMissions = SRP_MissionManagerComponent.GetInstance();
			array<int> orderTypes = {};
			if (orderMissions)
				orderMissions.GetOrderTypes(orderTypes);
			foreach (int orderIndex, int orderType : orderTypes)
			{
				if (orderIndex > 0)
					json += ",";
				json += "{\"id\":" + orderType.ToString() + ",\"name\":\"" + Esc(SRP_MissionManagerComponent.TypeName(orderType)) + "\"}";
			}
			json += "]";
			json += ",\"places\":[";
			bool firstPlace = true;
			foreach (SRP_CivilZoneComponent place : SRP_CivilZoneComponent.GetZones())
			{
				string placeName = place.GetPlaceName();
				if (placeName.IsEmpty())
					continue;
				if (!firstPlace)
					json += ",";
				firstPlace = false;
				vector placeCenter = place.GetCenter();
				json += "{\"name\":\"" + Esc(placeName) + "\",\"x\":" + Math.Round(placeCenter[0]).ToString() + ",\"z\":" + Math.Round(placeCenter[2]).ToString() + "}";
			}
			json += "]";
		}

		// Trésorerie et dépôts
		int balance = 0;
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (treasury)
			balance = treasury.GetBalance();
		json += ",\"euros\":" + balance.ToString();
		json += ",\"stocks\":{";
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		int mun = 0;
		int carb = 0;
		int pieces = 0;
		int vivres = 0;
		if (resources)
		{
			mun = resources.GetStock(SRP_EResource.MUNITIONS);
			carb = resources.GetStock(SRP_EResource.CARBURANT);
			pieces = resources.GetStock(SRP_EResource.PIECES);
			vivres = resources.GetStock(SRP_EResource.VIVRES);
		}
		json += "\"munitions\":" + mun.ToString() + ",\"carburant\":" + carb.ToString() + ",\"pieces\":" + pieces.ToString() + ",\"vivres\":" + vivres.ToString() + "}";

		// Caisses de production à ramasser : zones à nous avec un stock et une ressource (la clé « sector » est gardée
		// pour le bot ; elle porte le libellé de la zone, « Régina (S07) »)
		json += ",\"boxes\":[";
		SRP_FrontEconomy economy;
		if (frontReady)
			economy = front.GetEconomy();
		if (economy)
		{
			array<string> boxLabels = {};
			array<int> boxCounts = {};
			array<int> boxResources = {};
			economy.GetBoxes(boxLabels, boxCounts, boxResources);
			for (int boxIndex = 0; boxIndex < boxLabels.Count(); boxIndex++)
			{
				int boxCount = boxCounts[boxIndex];
				int boxResource = boxResources[boxIndex];
				if (boxIndex > 0)
					json += ",";
				json += "{\"sector\":\"" + Esc(boxLabels[boxIndex]) + "\",\"count\":" + boxCount.ToString() + ",\"resource\":\"" + Esc(SRP_Resources.GetName(boxResource)) + "\"}";
			}
		}
		json += "]";

		// Parc de véhicules
		json += ",\"fleet\":[";
		first = true;
		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (fleet)
		{
			array<SRP_FleetVehicle> vehicles = {};
			fleet.GetVehicles(vehicles);
			foreach (SRP_FleetVehicle vehicle : vehicles)
			{
				if (vehicle.m_iState == SRP_EVehicleState.DETRUIT)
					continue;
				string vehicleState = "en service";
				if (vehicle.m_iState == SRP_EVehicleState.EN_COMMANDE)
					vehicleState = "en livraison";
				string vehicleGrid = "?";
				if (vehicle.m_bHasTransform)
					vehicleGrid = SRP_FleetManagerComponent.GridOf(Vector(vehicle.m_fX, vehicle.m_fY, vehicle.m_fZ));
				if (!first)
					json += ",";
				first = false;
				json += "{\"id\":\"" + Esc(vehicle.m_sId) + "\",\"name\":\"" + Esc(vehicle.m_sName) + "\",\"state\":\"" + vehicleState + "\",\"grid\":\"" + vehicleGrid + "\",\"base\":" + BoolText(fleet.IsAtBase(vehicle)) + "}";
			}
		}
		json += "]";

		// Demandes de mission (salon commandement)
		json += ",\"requests\":" + SRP_MissionRequests.ToJson();

		// Alertes à publier : retirées de la file seulement quand le bot a bien reçu cet envoi (une alerte poussée
		// pendant l'envoi partira au suivant)
		m_iEventsSent = m_aEventTypes.Count();
		json += ",\"events\":[";
		for (int e = 0; e < m_iEventsSent; e++)
		{
			if (e > 0)
				json += ",";
			json += "{\"type\":\"" + Esc(m_aEventTypes[e]) + "\",\"text\":\"" + Esc(m_aEventTexts[e]) + "\",\"zone\":\"" + Esc(m_aEventZones[e]) + "\"}";
		}
		json += "]";

		// Résultats des commandes précédentes
		json += ",\"results\":[";
		first = true;
		foreach (SRP_BridgeResult result : m_aResults)
		{
			if (!first)
				json += ",";
			first = false;
			json += "{\"id\":\"" + Esc(result.m_sId) + "\",\"ok\":" + BoolText(result.m_bOk) + ",\"message\":\"" + Esc(result.m_sMessage) + "\"}";
		}
		json += "]";

		// Demandes de liaison : retirées de la file seulement quand le bot a bien reçu cet envoi (un /lier tapé
		// pendant l'envoi partira au suivant)
		m_iLinksSent = m_aLinks.Count();
		json += ",\"links\":[";
		for (int linkIndex = 0; linkIndex < m_iLinksSent; linkIndex++)
		{
			if (linkIndex > 0)
				json += ",";
			json += "\"" + Esc(m_aLinks[linkIndex]) + "\"";
		}
		json += "]";

		// Demandes de vote (/vote en jeu) : retirées de la file seulement quand le bot a bien reçu cet envoi
		m_iVotesSent = m_aVotes.Count();
		json += ",\"votereq\":[";
		for (int voteIndex = 0; voteIndex < m_iVotesSent; voteIndex++)
		{
			if (voteIndex > 0)
				json += ",";
			json += "\"" + Esc(m_aVotes[voteIndex]) + "\"";
		}
		json += "]}";
		return json;
	}

	//------------------------------------------------------------------------------------------------
	protected static string BoolText(bool value)
	{
		if (value)
			return "true";
		return "false";
	}

	//------------------------------------------------------------------------------------------------
	//! 1 ou 0 (clés courtes du bloc front)
	protected static string BoolInt(bool value)
	{
		if (value)
			return "1";
		return "0";
	}

	//================================================================================================
	// Front (cap « front ») : grille, zones, versions, accusé du bot
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Bloc « front » de l'envoi, qui commence par la virgule. Instantané complet (plein, n, k, g, zm, zones) quand le
	//! bot le demande, une fois tous les m_iFrontFullEvery envois, sans accusé, après un changement de campagne, si
	//! le journal ne remonte plus jusqu'à l'accusé ou s'il y a trop de changements ; sinon les seuls changements
	//! depuis la version tenue par le bot (base, d). Toujours : les zones remarquables (zs) et les carrés contestés
	//! (cs) ; les zones forcées par le Staff (stf) s'il y en a. Une zone absente de zs est rouge, calme et sans dépôt.
	//! Rien ("") tant que la campagne n'a pas d'identifiant — BuildState
	protected string BuildFront(SRP_FrontComponent front)
	{
		if (!front || !front.IsReady())
			return "";

		// front.json illisible au démarrage : pas d'identifiant, le bot ignorerait le bloc et la grille entière
		// repartirait à chaque contact ; rien ne part tant qu'il manque
		string campaign = front.GetCampaignId();
		if (campaign.IsEmpty())
			return "";

		int version = front.GetBridgeVersion();
		int restores = front.GetRestoreCount();
		m_iFrontCountdown--;

		// Changements que le bot n'a pas encore
		int pending = 0;
		foreach (int loggedVersion : m_aFrontLogVersions)
		{
			if (loggedVersion > m_iFrontAck)
				pending++;
		}
		bool full = m_bFrontFullWanted || m_iFrontCountdown <= 0 || m_iFrontAck < 0 || m_sFrontAckId != campaign || m_iFrontAck < m_iFrontLogFloor || pending > m_iFrontMaxDelta;

		string json = ",\"front\":{\"id\":\"" + Esc(campaign) + "\",\"v\":" + version.ToString() + ",\"rs\":" + restores.ToString() + ",\"gel\":" + BoolText(front.IsFrozen());
		if (full)
		{
			m_iFrontCountdown = Math.MaxInt(m_iFrontFullEvery, 1);
			m_bFrontFullWanted = false;
			RefreshFrontCaches(front);
			int gridSize = front.GetGridSize();
			int cellMeters = Math.Round(front.GetCellSize());
			// g : N x N caractères « . » « R » « B », index gz x N + gx, gz compté depuis le sud
			json += ",\"plein\":true,\"n\":" + gridSize.ToString() + ",\"k\":" + cellMeters.ToString();
			json += ",\"g\":\"" + front.GetCellsString() + "\"";
			json += ",\"zm\":\"" + m_sFrontZoneMap + "\"";
			json += ",\"zones\":[" + m_sFrontZonesJson + "]";
		}
		else
		{
			// Changements dans l'ordre, depuis la version tenue par le bot : « 074 042 B »
			int deltaFrom = m_iFrontAck;
			json += ",\"base\":" + deltaFrom.ToString() + ",\"d\":[";
			bool firstDelta = true;
			for (int logIndex = 0; logIndex < m_aFrontLogVersions.Count(); logIndex++)
			{
				if (m_aFrontLogVersions[logIndex] <= deltaFrom)
					continue;
				string ownerLetter = "R";
				if (m_aFrontLogOwners[logIndex] == SRP_EFrontOwner.BLEU)
					ownerLetter = "B";
				if (!firstDelta)
					json += ",";
				firstDelta = false;
				json += "\"" + FrontRef(front, m_aFrontLogCells[logIndex]) + " " + ownerLetter + "\"";
			}
			json += "]";
		}

		// zs : les zones remarquables (à nous, attaquées, près du front, coupées, imprenables ou avec un dépôt)
		json += ",\"zs\":[";
		bool firstZone = true;
		int zoneCount = front.GetZoneCount();
		for (int zoneIndex = 0; zoneIndex < zoneCount; zoneIndex++)
		{
			SRP_FrontZone frontZone = front.GetZone(zoneIndex);
			if (!frontZone)
				continue;
			bool ours = frontZone.IsOurs();
			int attack = frontZone.GetAttackState();
			bool nearFront = frontZone.IsNearFront();
			bool cut = front.IsZoneCut(zoneIndex);
			bool shielded = frontZone.IsProtected();
			bool depot = frontZone.HasDepot();
			if (!ours && attack <= 0 && !nearFront && !cut && !shielded && !depot)
				continue;
			int cutLeft = front.GetZoneCutSecondsLeft(zoneIndex);
			int stock = frontZone.GetStock();
			if (!firstZone)
				json += ",";
			firstZone = false;
			json += "{\"c\":\"" + Esc(frontZone.GetCode()) + "\",\"o\":" + BoolInt(ours) + ",\"a\":" + attack.ToString() + ",\"f\":" + BoolInt(nearFront);
			json += ",\"k\":" + BoolInt(cut) + ",\"kr\":" + cutLeft.ToString() + ",\"p\":" + BoolInt(shielded);
			// Dépôt de zone (#49) : son carré de 100 m, comme aujourd'hui, et les paquets en attente
			if (depot)
				json += ",\"dp\":\"" + SRP_FleetManagerComponent.GridOf(frontZone.GetDepotPosition()) + "\"";
			if (depot || stock > 0)
				json += ",\"st\":" + stock.ToString();
			json += "}";
		}
		json += "]";

		// cs : les carrés contestés (C8), références de coin
		array<int> contested = {};
		front.GetContestedCells(contested);
		json += ",\"cs\":[";
		foreach (int contestedIndex, int contestedCell : contested)
		{
			if (contestedIndex > 0)
				json += ",";
			json += "\"" + FrontRef(front, contestedCell) + "\"";
		}
		json += "]";

		// stf : les zones forcées ou remises par le Staff depuis le dernier envoi reçu (Q9), retirées à la réponse
		if (!s_aStaffZones.IsEmpty())
		{
			s_iStaffSent = s_aStaffZones.Count();
			json += ",\"stf\":[";
			for (int staffIndex = 0; staffIndex < s_iStaffSent; staffIndex++)
			{
				if (staffIndex > 0)
					json += ",";
				json += "\"" + Esc(s_aStaffZones[staffIndex]) + "\"";
			}
			json += "]";
		}
		json += "}";
		return json;
	}

	//------------------------------------------------------------------------------------------------
	//! Caches de l'instantané : zm (deux caractères hexadécimaux de zone par carré, « FF » hors jeu) et la liste des
	//! zones {i, c, n, l, genre} ; refaits si la géométrie a changé ou après une remise à plat
	protected void RefreshFrontCaches(SRP_FrontComponent front)
	{
		int geometry = front.GetGeometryVersion();
		if (geometry == m_iFrontGeomVersion && !m_sFrontZoneMap.IsEmpty() && !m_sFrontZonesJson.IsEmpty())
			return;
		m_iFrontGeomVersion = geometry;
		m_sFrontZoneMap = front.GetZoneMapString();

		string zonesJson = "";
		int zoneCount = front.GetZoneCount();
		for (int zoneIndex = 0; zoneIndex < zoneCount; zoneIndex++)
		{
			SRP_FrontZone frontZone = front.GetZone(zoneIndex);
			if (!frontZone)
				continue;
			if (!zonesJson.IsEmpty())
				zonesJson += ",";
			zonesJson += "{\"i\":" + zoneIndex.ToString() + ",\"c\":\"" + Esc(frontZone.GetCode()) + "\",\"n\":\"" + Esc(frontZone.GetName()) + "\",\"l\":\"" + Esc(frontZone.GetLabel()) + "\",\"genre\":\"" + Esc(frontZone.GetKind()) + "\"}";
		}
		m_sFrontZonesJson = zonesJson;
	}

	//------------------------------------------------------------------------------------------------
	//! Référence d'un carré pour le bot (A7) : coin sud-ouest en centaines de mètres, « 074 042 », la même que la radio
	//! et le Staff (SRP_FrontComponent.CellRef, qui suit la taille du carré m_fCellSize, envoyée dans « k ») ; le bot
	//! retrouve le carré en divisant par k / 100 (2 avec les carrés de 200 m)
	protected static string FrontRef(SRP_FrontComponent front, int cell)
	{
		return front.CellRef(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne d'accusé du bot : « #front <campagne> <version> » (il tient cette campagne à cette version : le journal
	//! est purgé jusque-là) ou « #front plein » (il demande la grille entière) — OnSyncDone
	protected void ReadFrontAck(string line)
	{
		array<string> ackTokens = {};
		line.Split(" ", ackTokens, true);
		if (ackTokens.Count() < 2)
			return;
		if (ackTokens[1] == "plein")
		{
			m_bFrontFullWanted = true;
			return;
		}
		if (ackTokens.Count() < 3)
			return;
		int ackVersion = ackTokens[2].ToInt(-1);
		if (ackVersion < 0)
		{
			// Version illisible : on repart sur la grille entière
			m_bFrontFullWanted = true;
			return;
		}
		m_sFrontAckId = ackTokens[1];
		m_iFrontAck = ackVersion;
		for (int logIndex = m_aFrontLogVersions.Count() - 1; logIndex >= 0; logIndex--)
		{
			if (m_aFrontLogVersions[logIndex] > m_iFrontAck)
				continue;
			m_aFrontLogVersions.RemoveOrdered(logIndex);
			m_aFrontLogCells.RemoveOrdered(logIndex);
			m_aFrontLogOwners.RemoveOrdered(logIndex);
		}
		if (m_iFrontLogFloor < m_iFrontAck)
			m_iFrontLogFloor = m_iFrontAck;
	}

	//------------------------------------------------------------------------------------------------
	// Réponse du bot
	//------------------------------------------------------------------------------------------------
	void OnSyncDone(bool ok, int code, string data)
	{
		m_InFlight = null;

		if (!ok || code < 200 || code >= 300)
		{
			m_iErrors++;
			if (m_bOnline || m_iErrors == MAX_ERRORS_BEFORE_NOTICE)
				SRP_JournalComponent.Log("SYSTEME", string.Format("Passerelle bot : injoignable (code HTTP %1, %2 échec(s) de suite)", code, m_iErrors));
			m_bOnline = false;
			m_iVotesSent = 0;		// les demandes de vote et de liaison, les alertes et les zones Staff restent dans la file et repartiront
			m_iLinksSent = 0;
			m_iEventsSent = 0;
			s_iStaffSent = 0;
			return;
		}

		if (!m_bOnline)
			SRP_JournalComponent.Log("SYSTEME", "Passerelle bot : contact établi");
		m_bOnline = true;
		m_iErrors = 0;

		// Le bot a reçu nos résultats et nos liaisons : on les oublie
		m_aResults.Clear();
		for (int linkSentIndex = 0; linkSentIndex < m_iLinksSent; linkSentIndex++)
		{
			if (m_aLinks.IsEmpty())
				break;
			m_aLinks.RemoveOrdered(0);
		}
		m_iLinksSent = 0;
		for (int sentIndex = 0; sentIndex < m_iVotesSent; sentIndex++)
		{
			if (m_aVotes.IsEmpty())
				break;
			m_aVotes.RemoveOrdered(0);
		}
		m_iVotesSent = 0;
		// Seules les alertes parties avec cet envoi sortent de la file (une alerte poussée pendant l'envoi reste)
		for (int eventSentIndex = 0; eventSentIndex < m_iEventsSent; eventSentIndex++)
		{
			if (m_aEventTypes.IsEmpty())
				break;
			m_aEventTypes.RemoveOrdered(0);
			m_aEventTexts.RemoveOrdered(0);
			m_aEventZones.RemoveOrdered(0);
		}
		m_iEventsSent = 0;
		// Idem pour les zones forcées par le Staff (stf)
		for (int staffSentIndex = 0; staffSentIndex < s_iStaffSent; staffSentIndex++)
		{
			if (s_aStaffZones.IsEmpty())
				break;
			s_aStaffZones.RemoveOrdered(0);
		}
		s_iStaffSent = 0;

		if (data.IsEmpty())
			return;

		array<string> lines = {};
		data.Split("\n", lines, true);
		int executed = 0;
		foreach (string line : lines)
		{
			string clean = line.Trim();
			if (clean.IsEmpty())
				continue;

			// Accusé du front (« #front <campagne> <version> » ou « #front plein ») : pas une commande
			if (clean.StartsWith("#front"))
			{
				ReadFrontAck(clean);
				continue;
			}

			array<string> parts = {};
			clean.Split("\t", parts, false);
			if (parts.Count() < 3)
				continue;

			SRP_BridgeResult result = new SRP_BridgeResult();
			result.m_sId = parts[0];
			result.m_bOk = true;
			result.m_sMessage = Execute(parts[1], parts[2], result.m_bOk);
			m_aResults.Insert(result);
			executed++;
		}

		// Des résultats à rendre : prochain contact rapide
		if (executed > 0)
			GetGame().GetCallqueue().CallLater(Sync, 1500, false);
	}

	//------------------------------------------------------------------------------------------------
	// Exécution des commandes du bot
	//------------------------------------------------------------------------------------------------
	//! Retrouve une fiche par pseudo (en ligne d'abord, sinon sur disque) ou par identité
	protected SRP_PlayerRecord FindRecord(string who, out int playerId)
	{
		playerId = -1;
		SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();

		// « @identité » : la forme sûre, envoyée par le panel du bot (un pseudo peut contenir des espaces)
		if (who.StartsWith("@"))
		{
			string identity = who.Substring(1, who.Length() - 1);
			if (manager)
			{
				array<int> ids = {};
				array<SRP_PlayerRecord> records = {};
				manager.GetConnectedRecords(ids, records);
				for (int i = 0; i < ids.Count(); i++)
				{
					if (records[i] && records[i].m_sIdentity == identity)
					{
						playerId = ids[i];
						return records[i];
					}
				}
			}
			who = identity;
		}
		else if (who.Contains("+"))
		{
			// Pseudo saisi à la main dans Discord : les espaces y sont écrits « + »
			string spaced = who;
			spaced.Replace("+", " ");
			if (!manager || manager.FindPlayerIdByName(who) < 0)
				who = spaced;
		}

		if (manager)
		{
			int id = manager.FindPlayerIdByName(who);
			if (id >= 0)
			{
				playerId = id;
				return manager.GetRecord(id);
			}
		}

		string lower = who;
		lower.ToLower();
		array<ref SRP_PlayerRecord> all = {};
		SRP_PlayerRecord.LoadAll(all);
		foreach (SRP_PlayerRecord record : all)
		{
			string name = record.m_sName;
			name.ToLower();
			if (name == lower || record.m_sIdentity == who)
				return record;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Rest(array<string> tokens, int from)
	{
		string text = "";
		for (int i = from; i < tokens.Count(); i++)
		{
			if (!text.IsEmpty())
				text += " ";
			text += tokens[i];
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Exécute une commande venue de Discord. L'auteur est le membre Discord (déjà contrôlé par le bot).
	string Execute(string author, string commandLine, out bool ok)
	{
		ok = false;
		array<string> tokens = {};
		commandLine.Split(" ", tokens, true);
		if (tokens.IsEmpty())
			return "commande vide";

		string cmd = tokens[0];
		cmd.ToLower();
		string signature = author + " (Discord)";
		SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();

		if (cmd == "sauver")
		{
			if (manager)
				manager.SaveAll();
			ok = true;
			return "fiches et état sauvegardés";
		}

		if (cmd == "dire")
		{
			string text = Rest(tokens, 1);
			if (text.IsEmpty())
				return "texte vide";
			SRP_Utils.NotifyAll("[Discord] " + author + " : " + text);
			ok = true;
			return "annonce envoyée à tous les joueurs en ligne";
		}

		if (cmd == "joueurs")
		{
			array<int> ids = {};
			int count = GetGame().GetPlayerManager().GetPlayers(ids);
			string names = "";
			foreach (int id : ids)
			{
				if (!names.IsEmpty())
					names += ", ";
				names += GetGame().GetPlayerManager().GetPlayerName(id);
			}
			if (names.IsEmpty())
				names = "personne";
			ok = true;
			return count.ToString() + " en ligne : " + names;
		}

		if (cmd == "radio")
		{
			ok = true;
			return SRP_RadioWatch.Report();
		}

		if (cmd == "heure")
		{
			if (tokens.Count() < 2)
				return "il manque l'heure, de 0 à 23";
			string hourText = tokens[1];
			hourText.Replace(",", ".");
			float hour = hourText.ToFloat();
			if (hour < 0 || hour >= 24 || (hour == 0 && !hourText.StartsWith("0")))
				return "heure invalide : " + tokens[1] + " (de 0 à 23, par exemple 6 ou 21.5)";
			SRP_DayNightComponent dayNight = SRP_DayNightComponent.GetInstance();
			if (!dayNight)
				return "cycle jour-nuit absent du game mode";
			ok = true;
			return dayNight.SetHour(hour, signature);
		}

		if (cmd == "demande")
			return SRP_MissionRequests.ExecuteBridge(author, tokens, ok);

		// Mission ordonnée par un officier depuis Discord : ordonner <n° de type | capture> <identité du chef | -> <lieu>
		if (cmd == "ordonner")
		{
			SRP_MissionManagerComponent orderManager = SRP_MissionManagerComponent.GetInstance();
			if (!orderManager)
				return "gestionnaire de missions absent du game mode";
			return orderManager.OrderFromBridge(author, tokens, ok);
		}

		if (cmd == "mission")
		{
			SRP_MissionManagerComponent missionManager = SRP_MissionManagerComponent.GetInstance();
			if (!missionManager)
				return "gestionnaire de missions absent du game mode";
			if (tokens.Count() < 2)
				return "il manque l'action : creer, annuler <id>, reussir <id>";
			string missionAction = tokens[1];
			missionAction.ToLower();
			if (missionAction == "creer")
			{
				if (!missionManager.Create(-1))
					return "aucune mission créée (site ou prefabs manquants, voir le journal)";
				SRP_JournalComponent.Log("STAFF", signature + " : mission lancée depuis Discord");
				ok = true;
				return "mission créée";
			}
			if (tokens.Count() < 3)
				return "il manque l'identifiant de la mission";
			SRP_Mission targetMission = missionManager.FindById(tokens[2]);
			if (!targetMission)
				return "mission inconnue : " + tokens[2];
			if (missionAction == "reussir")
			{
				missionManager.End(targetMission, SRP_EMissionState.SUCCES, "validée par " + signature);
				ok = true;
				return targetMission.m_sId + " validée";
			}
			if (missionAction == "annuler")
			{
				missionManager.End(targetMission, SRP_EMissionState.ANNULEE, "annulée par " + signature);
				ok = true;
				return targetMission.m_sId + " annulée";
			}
			return "action inconnue : " + missionAction;
		}

		// Toutes les autres commandes visent un joueur
		if (tokens.Count() < 2)
			return "il manque le pseudo du joueur";
		int targetId;
		SRP_PlayerRecord target = FindRecord(tokens[1], targetId);
		if (!target)
			return "joueur inconnu : " + tokens[1] + " (pseudo exact, ou identité)";
		bool online = targetId >= 0;

		if (cmd == "info")
		{
			ok = true;
			string status = "hors ligne";
			if (online)
				status = "en ligne";
			return target.ToSummary() + " — " + status + " — instructeur : " + SRP_Certifs.MaskToString(target.m_iInstructeurCertifs) + " — " + target.m_iMissions.ToString() + " mission(s), " + target.m_iInfractions.ToString() + " infraction(s)";
		}

		// Vote sur top-serveurs.net, réclamé par le bot auprès de leur API : vote <joueur> [prime en €] [horodatage]
		// L'horodatage vient du bot : un vote renvoyé (réponse perdue, redémarrage) n'est jamais compté deux fois.
		if (cmd == "vote")
		{
			int prime = 150;
			if (tokens.Count() >= 3)
				prime = tokens[2].ToInt();
			int voteStamp = 0;
			if (tokens.Count() >= 4)
				voteStamp = tokens[3].ToInt();
			string stampKey = "," + voteStamp.ToString() + ",";
			if (voteStamp > 0 && target.m_sVoteStamps.Contains(stampKey))
			{
				ok = true;
				return target.m_sName + " : ce vote est déjà compté";
			}
			target.m_iVotes = target.m_iVotes + 1;
			if (voteStamp > 0)
			{
				// garder les 10 derniers horodatages, sous la forme « ,t1,t2, »
				array<string> voteStamps = {};
				target.m_sVoteStamps.Split(",", voteStamps, true);
				voteStamps.Insert(voteStamp.ToString());
				while (voteStamps.Count() > 10)
					voteStamps.RemoveOrdered(0);
				string stampList = ",";
				foreach (string oneStamp : voteStamps)
					stampList += oneStamp + ",";
				target.m_sVoteStamps = stampList;
			}
			target.Save();
			// La fiche d'abord, puis le coffre écrit aussitôt : un plantage entre les deux perd la prime, ne la double jamais
			SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
			if (treasury && prime > 0)
			{
				treasury.Add(prime, "vote de " + target.m_sName + " sur top-serveurs");
				treasury.Save();
			}
			SRP_JournalComponent.Log("VOTE", string.Format("%1 [%2] a voté sur top-serveurs (%3 vote(s)), +%4 € pour la base", target.m_sName, target.m_sIdentity, target.m_iVotes, prime));
			if (online)
				SRP_Utils.NotifyPlayer(targetId, string.Format("Merci pour ton vote sur top-serveurs : +%1 € pour la base (%2 vote(s) à ton nom)", prime, target.m_iVotes));
			ok = true;
			return string.Format("%1 : vote compté (%2 au total), +%3 € pour la base", target.m_sName, target.m_iVotes, prime);
		}

		if (cmd == "tp" || cmd == "soigner" || cmd == "etat" || cmd == "arme-recreer")
		{
			if (!online)
				return target.m_sName + " n'est pas en ligne";
			SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
			if (!admin)
				return "menu Staff absent du game mode";
			ok = true;
			if (cmd == "tp")
				return admin.BridgeTeleportToBase(targetId, signature);
			if (cmd == "etat")
				return admin.BridgeState(targetId);
			if (cmd == "arme-recreer")
				return admin.BridgeRecreateWeapon(targetId, signature);
			return admin.BridgeHeal(targetId, signature);
		}

		if (cmd == "mp")
		{
			if (!online)
				return target.m_sName + " n'est pas en ligne";
			string text = Rest(tokens, 2);
			if (text.IsEmpty())
				return "texte vide";
			SRP_Utils.NotifyPlayer(targetId, "[Discord] " + author + " : " + text);
			ok = true;
			return "message remis à " + target.m_sName;
		}

		if (cmd == "kick")
		{
			if (!online)
				return target.m_sName + " n'est pas en ligne";
			string reason = Rest(tokens, 2);
			if (reason.IsEmpty())
				reason = "aucun motif donné";
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] expulsé par %3 : %4", target.m_sName, target.m_sIdentity, signature, reason));
			GetGame().GetPlayerManager().KickPlayer(targetId, 0, 0);
			ok = true;
			return target.m_sName + " expulsé (" + reason + ")";
		}

		if (cmd == "ban")
		{
			if (target.m_bStaff)
				return "on ne bannit pas un membre du Staff par commande";
			int days = 0;
			int start = 2;
			if (tokens.Count() >= 3 && SRP_Utils.IsNumeric(tokens[2]))
			{
				days = tokens[2].ToInt();
				start = 3;
			}
			string reason = Rest(tokens, start);
			if (reason.IsEmpty())
				reason = "aucun motif donné";
			SRP_Bans.Add(target.m_sIdentity, target.m_sName, reason, signature, days);
			string duration = "définitivement";
			if (days > 0)
				duration = "pour " + days.ToString() + " jour(s)";
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] banni %3 par %4 : %5", target.m_sName, target.m_sIdentity, duration, signature, reason));
			if (online)
				GetGame().GetPlayerManager().KickPlayer(targetId, 0, 0);
			ok = true;
			return target.m_sName + " banni " + duration + " (" + reason + ")";
		}

		if (cmd == "unban")
		{
			if (!SRP_Bans.Remove(target.m_sIdentity))
				return target.m_sName + " n'est pas banni";
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] débanni par %3", target.m_sName, target.m_sIdentity, signature));
			ok = true;
			return target.m_sName + " débanni";
		}

		if (cmd == "avertir")
		{
			string reason = Rest(tokens, 2);
			if (reason.IsEmpty())
				reason = "aucun motif donné";
			if (online && manager)
			{
				ok = true;
				return manager.Warn(targetId, reason, signature);
			}
			target.m_iInfractions = target.m_iInfractions + 1;
			target.Save();
			SRP_JournalComponent.Log("AVERTISSEMENT", string.Format("%1 [%2] par %3 : %4 (total %5, hors ligne)", target.m_sName, target.m_sIdentity, signature, reason, target.m_iInfractions));
			ok = true;
			return target.m_sName + " averti hors ligne (" + target.m_iInfractions.ToString() + " avertissement(s))";
		}

		if (cmd == "grade")
		{
			if (tokens.Count() < 3)
				return "il manque le grade : " + SRP_Grades.GetCommandNames();
			int grade = SRP_Grades.FromName(tokens[2]);
			if (!SRP_Grades.IsValide(grade))
				return "grade inconnu : " + tokens[2] + " — " + SRP_Grades.GetCommandNames();
			string reason;
			if (online && manager)
			{
				if (!manager.SetGrade(targetId, grade, signature, true, reason))
					return "refusé : " + reason;
			}
			else
			{
				int ancien = target.m_iGrade;
				target.m_iGrade = grade;
				target.Save();
				SRP_JournalComponent.Log("GRADE", string.Format("%1 [%2] : %3 -> %4 (par %5, hors ligne)", target.m_sName, target.m_sIdentity, SRP_Grades.GetName(ancien), SRP_Grades.GetName(grade), signature));
			}
			ok = true;
			return target.m_sName + " est maintenant " + SRP_Grades.GetName(grade);
		}

		if (cmd == "certif" || cmd == "instructeur")
		{
			if (tokens.Count() < 3)
				return "il manque la certification : " + SRP_Certifs.GetCommandNames();
			int certif = SRP_Certifs.FromName(tokens[2]);
			if (certif == 0)
				return "certification inconnue : " + tokens[2] + " — " + SRP_Certifs.GetCommandNames();
			bool on = true;
			if (tokens.Count() >= 4)
			{
				string flag = tokens[3];
				flag.ToLower();
				on = !(flag == "off" || flag == "non" || flag == "0" || flag == "retirer");
			}
			string reason;
			if (cmd == "certif")
			{
				if (online && manager)
				{
					if (!manager.SetCertif(targetId, certif, on, signature, true, reason))
						return "refusé : " + reason;
				}
				else
				{
					if (on)
						target.m_iCertifs = target.m_iCertifs | certif;
					else
						target.m_iCertifs = target.m_iCertifs & ~certif;
					target.Save();
					SRP_JournalComponent.Log("CERTIF", string.Format("%1 [%2] : %3 %4 (par %5, hors ligne)", target.m_sName, target.m_sIdentity, SRP_Certifs.GetFullName(certif), OnOffText(on), signature));
				}
				ok = true;
				return target.m_sName + " : " + SRP_Certifs.GetFullName(certif) + " " + OnOffText(on);
			}

			// Habilitation d'instructeur pour cette certification
			if (on && !SRP_Certifs.Has(target.m_iCertifs, certif))
				return target.m_sName + " ne détient pas " + SRP_Certifs.GetName(certif) + " : on n'instruit que ce qu'on a";
			if (online && manager)
			{
				if (!manager.SetInstructeur(targetId, certif, on, signature, reason))
					return "refusé : " + reason;
			}
			else
			{
				if (on)
					target.m_iInstructeurCertifs = target.m_iInstructeurCertifs | certif;
				else
					target.m_iInstructeurCertifs = target.m_iInstructeurCertifs & ~certif;
				target.Save();
				SRP_JournalComponent.Log("CERTIF", string.Format("%1 [%2] : habilitation instructeur %3 %4 (par %5, hors ligne)", target.m_sName, target.m_sIdentity, SRP_Certifs.GetName(certif), OnOffText(on), signature));
			}
			ok = true;
			return target.m_sName + " : instructeur " + SRP_Certifs.GetName(certif) + " " + OnOffText(on);
		}

		if (cmd == "staff")
		{
			bool on = true;
			if (tokens.Count() >= 3)
			{
				string flag = tokens[2];
				flag.ToLower();
				on = !(flag == "off" || flag == "non" || flag == "0" || flag == "retirer");
			}
			if (online && manager)
				manager.SetStaff(targetId, on, signature);
			else
			{
				target.m_bStaff = on;
				target.Save();
				SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] : staff = %3 (par %4, hors ligne)", target.m_sName, target.m_sIdentity, SRP_Utils.OuiNon(on), signature));
			}
			ok = true;
			return target.m_sName + " : staff = " + SRP_Utils.OuiNon(on);
		}

		return "commande inconnue : " + cmd;
	}

	//------------------------------------------------------------------------------------------------
	protected static string OnOffText(bool on)
	{
		if (on)
			return "accordée";
		return "retirée";
	}
}
