//------------------------------------------------------------------------------------------------
// SimpleRP — Menu Staff (/admin, touche F10)
// Écran à onglets (SRP_AdminMenu : même mécanique et même layout que le PC), rempli par le serveur :
//   Rapide       retour à la base, localités, soin, invincible, heure, raccourcis
//   Joueurs      les connectés ; un clic ouvre la fiche d'un joueur et ses actions (téléportation,
//                soin et réveil, retour sur son corps, état médical, arme recréée, grade, certifications,
//                états de service, avertissement, expulsion, ban, Staff)
//   Missions     les missions actives (aller sur le site, réussir, annuler), lancer un type précis
//   Territoire   zones, carrés, front gelé, sauvegardes datées : ce carré (bleu, rouge), zones (aller sur place,
//                forcer, remettre à l'état de départ, contre-attaque, dépôt), points clés manquants, ennemi du front,
//                nouvelle campagne ; pages et actions écrites par SRP_FrontScreens (CONTRATS_FRONT §1.13)
//   Commandeur   état-major ennemi (gel, régions, journal des décisions, décisions forcées) ; pages et actions
//                écrites par SRP_CmdScreens (CONTRATS_CMD §8)
//   Logistique   dépôts +/-, coffre +/-, caisse devant soi, parc (aller vers un véhicule)
//   Serveur      heure, vitesse du jour, sauvegarde, civils, IA, livraisons, déconnexions, Discord,
//                réglages du front et du Commandeur, charte, bannis
//   Danger       remises à zéro, chacune confirmée par un second clic sur la même ligne
// Une ligne s'exécute au clic ; l'écran se rafraîchit sur place, le résultat s'affiche en tête.
// Réservé au Staff, vérifié côté serveur à chaque action. Tout est journalisé en [STAFF], sauf l'onglet Commandeur
// (journal COMMANDEUR par SRP_CmdLog.StaffAction : l'onglet Journal du PC montre les lignes STAFF à tous).
//
// Protocole texte (SRP_ListMenu / SRP_PCMenu) : T|clé|libellé (onglets) A|clé (onglet actif)
//   M|message H|section L|ligne I|clé|libellé C|teinte|ligne colorée K|teinte|clé|ligne cliquable colorée
//   teintes : w blanc, g vert, r rouge, y orange, d gris
// Clés : <onglet>:<action>[:<arguments>] ; « page:<page> » navigue sans rien exécuter.
//------------------------------------------------------------------------------------------------

//! Ce qu'on retient d'un joueur au moment de sa mort : où, et avec quel équipement (dernière capture de la fiche)
class SRP_DeathSnapshot
{
	vector m_vPosition;
	float m_fYaw;
	string m_sInventory;
	int m_iTick;
}

[ComponentEditorProps(category: "SimpleRP", description: "Menu Staff SimpleRP (/admin, F10) : joueurs, missions, territoire, commandeur, logistique, serveur, remises à zéro")]
class SRP_AdminComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_AdminComponent : SCR_BaseGameModeComponent
{
	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur du point de la base (téléportation « Retour à la base »)", category: "SimpleRP")]
	protected string m_sBaseMarkerName;

	[Attribute("14", UIWidgets.EditBox, "Localités par page", category: "SimpleRP")]
	protected int m_iPageSize;

	[Attribute("20", UIWidgets.EditBox, "Logistique : paquets ajoutés ou retirés par clic", category: "SimpleRP")]
	protected int m_iStockStep;

	[Attribute("500", UIWidgets.EditBox, "Logistique : euros ajoutés ou retirés par clic", category: "SimpleRP")]
	protected int m_iEurosStep;

	[Attribute("20", UIWidgets.EditBox, "Logistique : paquets dans une caisse posée devant soi", category: "SimpleRP")]
	protected int m_iCrateSize;

	protected static SRP_AdminComponent s_Instance;

	protected ref set<int> m_GodPlayers = new set<int>();
	protected ref array<SRP_CivilZoneComponent> m_aSortedZones = {};
	protected ref map<int, string> m_mLastMessage = new map<int, string>();		// résultat à afficher en tête du prochain écran
	protected ref map<int, string> m_mPending = new map<int, string>();			// action dangereuse en attente d'un second clic
	protected ref map<int, ref SRP_DeathSnapshot> m_mDeaths = new map<int, ref SRP_DeathSnapshot>();	// dernière mort de chaque joueur connecté

	//------------------------------------------------------------------------------------------------
	static SRP_AdminComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce joueur a-t-il le mode invincible du menu Staff ? — SRP_FrontCapture (C3 : m_bExcludeGodMode)
	bool IsGod(int playerId)
	{
		return m_GodPlayers.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer())
			s_Instance = this;

		// Capture aérienne de la carte sans passer par le menu : un fichier de commande posé dans le profil du Workbench
		#ifdef WORKBENCH
		GetGame().GetCallqueue().CallLater(SRP_MapCapture.CheckTrigger, 10000, true);
		#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Appelé par la logique de spawn quand le personnage est prêt : le mode invincible survit à la mort
	void OnCharacterReady(int playerId, IEntity character)
	{
		if (Replication.IsServer() && m_GodPlayers.Contains(playerId))
			SetInvincible(character, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		m_GodPlayers.RemoveItem(playerId);
		m_mLastMessage.Remove(playerId);
		m_mPending.Remove(playerId);
		m_mDeaths.Remove(playerId);
		super.OnPlayerDisconnected(playerId, cause, timeout);
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur meurt : on retient la position du corps et son équipement d'avant la mort (la fiche garde la
	//! dernière capture faite de son vivant, moins de 5 s avant), pour le soin Staff « réapparaître sur son corps »
	override void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnPlayerKilled(instigatorContextData);

		if (!Replication.IsServer())
			return;

		int victimId = instigatorContextData.GetVictimPlayerID();
		IEntity victim = instigatorContextData.GetVictimEntity();
		if (victimId <= 0 || !victim)
			return;

		SRP_DeathSnapshot snapshot = new SRP_DeathSnapshot();
		snapshot.m_vPosition = victim.GetOrigin();
		vector angles = victim.GetYawPitchRoll();
		snapshot.m_fYaw = angles[0];
		snapshot.m_iTick = System.GetTickCount();

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
		{
			SRP_PlayerRecord record = players.GetRecord(victimId);
			if (record && record.m_sInventory.StartsWith("{"))
				snapshot.m_sInventory = record.m_sInventory;
		}
		m_mDeaths.Set(victimId, snapshot);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsStaff(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		return players && players.IsStaff(playerId);
	}

	//------------------------------------------------------------------------------------------------
	// Outils
	//------------------------------------------------------------------------------------------------
	//! Un texte libre dans une ligne du protocole : ni barre verticale ni retour à la ligne
	protected static string Esc(string text)
	{
		string s = text;
		s.Replace("|", "/");
		s.Replace("\n", " · ");
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Les champs d'une clé à partir d'un rang, recollés avec « : »
	protected static string Join(array<string> parts, int from)
	{
		string result = "";
		for (int i = from; i < parts.Count(); i++)
		{
			if (i > from)
				result += ":";
			result += parts[i];
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Un texte multi-lignes en lignes L| (ou C|teinte|)
	protected static string Lines(string text, string tint)
	{
		string result = "";
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
	//! Un bloc rendu par un autre module (SRP_FrontScreens, SRP_CmdScreens) collé sous l'onglet : chaque ligne du
	//! protocole (M, H, L, I, C, K) est gardée telle quelle, une ligne de texte simple devient une ligne L| ; chaque
	//! ligne repart d'un « \n », que le module l'ait mis en tête ou non (sinon elle se collerait à la précédente)
	protected static string ProtocolBlock(string text)
	{
		string result = "";
		if (text.IsEmpty())
			return result;
		array<string> lines = {};
		text.Split("\n", lines, true);
		foreach (string line : lines)
		{
			string head = "";
			if (line.Length() >= 2)
				head = line.Substring(0, 2);
			if (head == "M|" || head == "H|" || head == "L|" || head == "I|" || head == "C|" || head == "K|")
				result += "\n" + line;
			else
				result += "\nL|" + Esc(line);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Name(int playerId)
	{
		return GetGame().GetPlayerManager().GetPlayerName(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity Character(int playerId)
	{
		return GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! « 123 m » depuis le personnage du joueur, ou « ? »
	protected static string DistanceText(int playerId, vector position)
	{
		IEntity me = Character(playerId);
		if (!me)
			return "?";
		int meters = Math.Round(vector.Distance(me.GetOrigin(), position));
		return string.Format("%1 m", meters);
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsTab(string tab)
	{
		return tab == "rapide" || tab == "joueurs" || tab == "missions" || tab == "territoire" || tab == "commandeur" || tab == "logistique" || tab == "serveur" || tab == "danger";
	}

	//------------------------------------------------------------------------------------------------
	// Contenu
	//------------------------------------------------------------------------------------------------
	string BuildMenu(int playerId, string page)
	{
		if (!IsStaff(playerId))
			return "M|Réservé au Staff";

		array<string> parts = {};
		page.Split(":", parts, true);
		string tab = "rapide";
		if (parts.Count() > 0 && IsTab(parts[0]))
			tab = parts[0];

		// 8 onglets, autant que le PC d'un officier (SRP_PC.c:343-345)
		string text = "T|rapide|Rapide\nT|joueurs|Joueurs\nT|missions|Missions\nT|territoire|Territoire\nT|commandeur|Commandeur\nT|logistique|Logistique\nT|serveur|Serveur\nT|danger|Danger";
		text += "\nA|" + tab;
		text += "\nM|" + Esc(Header(playerId));

		string last;
		if (m_mLastMessage.Find(playerId, last))
		{
			if (!last.IsEmpty())
				text += "\nC|y|» " + Esc(last);
			m_mLastMessage.Remove(playerId);
		}

		if (tab == "joueurs") return text + Joueurs(playerId, parts);
		if (tab == "missions") return text + Missions(playerId, parts);
		if (tab == "territoire") return text + Territoire(playerId, parts);
		if (tab == "commandeur") return text + Commandeur(playerId, parts);
		if (tab == "logistique") return text + Logistique(playerId, parts);
		if (tab == "serveur") return text + Serveur(playerId, parts);
		if (tab == "danger") return text + Danger(playerId);
		return text + Rapide(playerId, parts);
	}

	//------------------------------------------------------------------------------------------------
	//! La barre d'état en tête de chaque onglet
	protected string Header(int playerId)
	{
		array<int> ids = {};
		int connected = GetGame().GetPlayerManager().GetPlayers(ids);

		string hour = "?";
		TimeAndWeatherManagerEntity weather = SRP_DayNightComponent.Weather();
		if (weather)
			hour = SRP_DayNightComponent.HourText(weather.GetTimeOfTheDay());

		string coffre = "?";
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (treasury)
			coffre = string.Format("%1 €", treasury.GetBalance());

		// « 3/10, 12/66 zones » (+ « , FRONT GELÉ ») puis « , COMMANDEUR GELÉ » : collés, sans paramètre de plus au Format
		string menace = SRP_FrontScreens.StaffHeaderPart() + SRP_CmdScreens.StaffHeaderPart();

		int actives = 0;
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
			actives = missions.CountActive();

		return string.Format("%1 · %2 connecté(s) · %3 · coffre %4 · menace %5 · %6 mission(s) active(s)", Name(playerId), connected, hour, coffre, menace, actives);
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Rapide
	//------------------------------------------------------------------------------------------------
	protected string Rapide(int playerId, array<string> parts)
	{
		if (parts.Count() >= 2 && parts[1] == "zones")
			return ZonesPage(parts);
		if (parts.Count() >= 2 && parts[1] == "ia-groupes")
			return EnemyGroupsPage(playerId, parts);
		if (parts.Count() >= 2 && parts[1] == "reperages")
			return SightingsPage(parts);

		string text = "\nH|Téléportation";
		text += "\nI|rapide:tp:base|» Retour à la base";
		text += "\nI|rapide:zones:0|» Vers une localité…";
		text += "\nH|Moi";
		text += "\nI|rapide:soigner|» Me soigner complètement";
		if (m_GodPlayers.Contains(playerId))
			text += "\nK|g|rapide:god|[x] Mode invincible : ON";
		else
			text += "\nK|w|rapide:god|[ ] Mode invincible : OFF";
		// Le carré où se tient le Staff (J1) ; les actions sont dans l'onglet Territoire
		text += "\nH|Territoire ici";
		IEntity hereCharacter = Character(playerId);
		if (hereCharacter)
			text += "\nL|" + Esc(SRP_FrontScreens.CellHere(hereCharacter.GetOrigin()));
		else
			text += "\nL|Hors jeu : pas de personnage, pas de carré";
		text += "\nI|page:territoire|» Agir sur ce carré ou cette zone, gel, sauvegardes";
		text += "\nH|Heure";
		text += "\nI|rapide:heure:5|» 5 h (aube)";
		text += "\nI|rapide:heure:8|» 8 h (matin)";
		text += "\nI|rapide:heure:12|» 12 h (midi)";
		text += "\nI|rapide:heure:17|» 17 h (soir)";
		text += "\nI|rapide:heure:20|» 20 h (crépuscule)";
		text += "\nI|rapide:heure:23|» 23 h (nuit)";
		text += "\nH|Essais";
		text += "\nI|rapide:controle-ici|» Établir un point de contrôle routier ici, seul (il faut être sur une route)";
		if (SRP_HeliSearch.Count() > 0)
			text += "\nK|y|rapide:helico-stop|» Retirer l'hélicoptère de recherche (et les épaves)";
		else
			text += "\nI|rapide:helico|» Hélicoptère ennemi qui me cherche, au-dessus de moi (prototype)";
		if (SRP_HeliSearch.Count() == 0 && SRP_HeliSearch.WreckCount() > 0)
			text += "\nK|y|rapide:helico-stop|» Supprimer l'épave de l'hélicoptère de recherche";
		SRP_EnemyComponent heliEnemies = SRP_EnemyComponent.GetInstance();
		if (heliEnemies)
		{
			text += "\nL|" + Esc(heliEnemies.HeliStatus());
			// Avec un Commandeur (même gelé), plus de tirage : l'hélicoptère se force par l'onglet Commandeur (ordre HELICO)
			if (SRP_Commander.Get())
				text += "\nI|page:commandeur:forcer|» Hélico : décidé par le Commandeur, à forcer depuis l'onglet Commandeur…";
			else
				text += "\nI|rapide:helico-forcer|» Hélico : forcer le prochain tirage (gagné, repos ignoré)";
		}
		if (SRP_HeliSearch.s_bFillLight)
			text += "\nK|g|rapide:helico-appoint|[x] Projecteur : appoint au sol ON";
		else
			text += "\nK|w|rapide:helico-appoint|[ ] Projecteur : appoint au sol OFF";
		text += "\nI|rapide:camion|» Forcer un camion de renfort (petit) vers la localité ennemie la plus proche";
		text += "\nI|rapide:camion-plein|» Forcer un camion de renfort plein vers la localité ennemie la plus proche";
		text += "\nI|rapide:ia-groupes:0|» IA ennemie : état des groupes autour de moi (3 km)";
		text += "\nI|rapide:reperages:0|» IA ennemie : journal de repérage";
		#ifdef WORKBENCH
		if (SRP_MapCapture.IsRunning())
			text += "\nK|y|rapide:capture-stop|» Arrêter la capture de la carte";
		else
		{
			text += "\nI|rapide:capture-comparer|» Capture aérienne : essai comparatif de 4 hauteurs sur Levie (2 min)";
			text += "\nI|rapide:capture-essai|» Capture aérienne : essai, 1 km autour de Levie (1 min)";
			text += "\nI|rapide:capture-ile|» Capture aérienne : île entière (plein écran, environ 2 h, reprend où elle s'est arrêtée)";
		}
		#endif
		text += "\nH|Raccourcis";
		text += "\nI|missions:nouvelle:-1|» Lancer une mission au hasard";
		text += "\nI|serveur:sauver|» Sauver toutes les fiches maintenant";
		text += "\nI|page:joueurs|» Voir les joueurs connectés";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshZones()
	{
		m_aSortedZones.Clear();
		array<SRP_CivilZoneComponent> zones = SRP_CivilZoneComponent.GetZones();
		foreach (SRP_CivilZoneComponent zone : zones)
		{
			if (zone)
				m_aSortedZones.Insert(zone);
		}

		// Tri alphabétique simple (peu d'éléments)
		for (int i = 0; i < m_aSortedZones.Count(); i++)
		{
			for (int j = i + 1; j < m_aSortedZones.Count(); j++)
			{
				if (SRP_Utils.CompareStrings(m_aSortedZones[j].GetZoneName(), m_aSortedZones[i].GetZoneName()) < 0)
				{
					SRP_CivilZoneComponent swap = m_aSortedZones[i];
					m_aSortedZones[i] = m_aSortedZones[j];
					m_aSortedZones[j] = swap;
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Page « rapide:zones:<numéro> »
	protected string ZonesPage(array<string> parts)
	{
		int pageIndex = 0;
		if (parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
			pageIndex = parts[2].ToInt();

		if (pageIndex == 0 || m_aSortedZones.IsEmpty())
			RefreshZones();

		int total = m_aSortedZones.Count();
		if (total == 0)
			return "\nL|Aucune localité connue (ambiance civile absente ou zones non créées)\nI|page:rapide|« Retour";

		int pageSize = m_iPageSize;
		if (pageSize < 5)
			pageSize = 5;
		int pages = (total + pageSize - 1) / pageSize;
		if (pageIndex < 0)
			pageIndex = 0;
		if (pageIndex >= pages)
			pageIndex = pages - 1;

		string text = string.Format("\nH|Localités, page %1 / %2", pageIndex + 1, pages);
		int first = pageIndex * pageSize;
		int last = Math.Min(first + pageSize, total);
		for (int i = first; i < last; i++)
			text += string.Format("\nI|rapide:tp:zone:%1|» %2", i, Esc(m_aSortedZones[i].GetZoneName()));

		if (pageIndex + 1 < pages)
			text += string.Format("\nI|rapide:zones:%1|› Page suivante", pageIndex + 1);
		if (pageIndex > 0)
			text += string.Format("\nI|rapide:zones:%1|‹ Page précédente", pageIndex - 1);
		text += "\nI|page:rapide|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page « rapide:ia-groupes:<numéro> » : état de nos groupes ennemis à moins de 3 km du Staff (lecture seule)
	protected string EnemyGroupsPage(int playerId, array<string> parts)
	{
		int pageIndex = 0;
		if (parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
			pageIndex = parts[2].ToInt();

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return "\nL|IA ennemie absente\nI|page:rapide|« Retour";
		IEntity me = Character(playerId);
		if (!me)
			return "\nL|Position inconnue : pas de personnage en jeu\nI|page:rapide|« Retour";

		vector from = me.GetOrigin();
		int pages = enemies.GroupsReportPages(from, 3000);
		if (pageIndex >= pages)
			pageIndex = pages - 1;
		if (pageIndex < 0)
			pageIndex = 0;

		string text = string.Format("\nH|IA ennemie : groupes à moins de 3 km, page %1 / %2", pageIndex + 1, pages);
		text += Lines(enemies.GroupsReport(from, 3000, pageIndex), "");
		// Sirènes des localités (carte #69) : leur état ne se lit qu'ici, jamais dans /territoire ni sur Discord ; sur la
		// première page seulement (pas répété à chaque page)
		SRP_SireneComponent sirens = SRP_SireneComponent.GetInstance();
		if (sirens && pageIndex == 0)
			text += Lines(sirens.GetAlertReport(from, 3000), "");
		// Radio des garnisons et camions de renfort (carte #69, livraison 3) : première page seulement, jamais aux joueurs
		SRP_TerritoryComponent radioTerritory = SRP_TerritoryComponent.GetInstance();
		if (radioTerritory && pageIndex == 0)
			text += Lines(radioTerritory.GetRadioReport(from, 3000), "");
		SRP_EnemyTruckComponent trucks = SRP_EnemyTruckComponent.GetInstance();
		if (trucks && pageIndex == 0)
			text += Lines(trucks.GetStatusText(from, 3000), "");
		// Commandeur (carte #76) : officiers et cachettes, mortier, 2S1, blindés, colonnes autour du Staff, au rayon
		// vu_proximite_m ; première page seulement, jamais aux joueurs
		if (SRP_Commander.Get() && pageIndex == 0)
			text += ProtocolBlock(SRP_CmdScreens.StaffNearReport(from, SRP_CmdScreens.s_iNearM));
		if (pageIndex + 1 < pages)
			text += string.Format("\nI|rapide:ia-groupes:%1|› Page suivante", pageIndex + 1);
		if (pageIndex > 0)
			text += string.Format("\nI|rapide:ia-groupes:%1|‹ Page précédente", pageIndex - 1);
		text += string.Format("\nI|rapide:ia-groupes:%1|» Rafraîchir", pageIndex);
		text += "\nI|page:rapide|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page « rapide:reperages:<numéro> » : le journal de repérage de l'IA ennemie, le plus récent d'abord (lecture seule)
	protected string SightingsPage(array<string> parts)
	{
		int pageIndex = 0;
		if (parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
			pageIndex = parts[2].ToInt();

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return "\nL|IA ennemie absente\nI|page:rapide|« Retour";

		int pages = enemies.SightingsReportPages();
		if (pageIndex >= pages)
			pageIndex = pages - 1;
		if (pageIndex < 0)
			pageIndex = 0;

		string text = string.Format("\nH|IA ennemie : journal de repérage, page %1 / %2", pageIndex + 1, pages);
		text += Lines(enemies.SightingsReport(pageIndex), "");
		if (pageIndex + 1 < pages)
			text += string.Format("\nI|rapide:reperages:%1|› Page suivante", pageIndex + 1);
		if (pageIndex > 0)
			text += string.Format("\nI|rapide:reperages:%1|‹ Page précédente", pageIndex - 1);
		text += string.Format("\nI|rapide:reperages:%1|» Rafraîchir", pageIndex);
		text += "\nI|page:rapide|« Retour";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Joueurs
	//------------------------------------------------------------------------------------------------
	protected string Joueurs(int playerId, array<string> parts)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "\nL|Gestionnaire de joueurs absent";

		if (parts.Count() >= 3 && parts[1] == "fiche" && SRP_Utils.IsNumeric(parts[2]))
			return FichePage(players, playerId, parts[2].ToInt());

		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);

		// Tri par grade décroissant
		array<int> order = {};
		for (int i = 0; i < records.Count(); i++)
			order.Insert(i);
		for (int a = 0; a < order.Count(); a++)
		{
			for (int b = a + 1; b < order.Count(); b++)
			{
				if (records[order[b]].m_iGrade > records[order[a]].m_iGrade)
				{
					int swap = order[a];
					order[a] = order[b];
					order[b] = swap;
				}
			}
		}

		string text = string.Format("\nH|Connectés (%1) — choisis un joueur pour ses actions", ids.Count());
		foreach (int index : order)
		{
			SRP_PlayerRecord record = records[index];
			int id = ids[index];
			string tint = "w";
			string staff = "";
			if (record.m_bStaff)
			{
				tint = "g";
				staff = " [Staff]";
			}
			string where = "sans personnage";
			IEntity character = Character(id);
			if (character)
			{
				if (id == playerId)
					where = "moi";
				else
					where = DistanceText(playerId, character.GetOrigin());
			}
			text += string.Format("\nK|%1|joueurs:fiche:%2|%3 %4%5 — %6 — %7", tint, id, SRP_Grades.GetName(record.m_iGrade), Esc(record.m_sName), staff, SRP_Certifs.MaskToString(record.m_iCertifs), where);
		}
		if (ids.IsEmpty())
			text += "\nL|Personne";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Page « joueurs:fiche:<id> » : la fiche et toutes les actions sur un joueur
	protected string FichePage(SRP_PlayerManagerComponent players, int playerId, int targetId)
	{
		SRP_PlayerRecord record = players.GetRecord(targetId);
		if (!record)
			return "\nL|Ce joueur n'est plus connecté\nI|page:joueurs|« Retour à la liste";

		string staff = "";
		if (record.m_bStaff)
			staff = " [Staff]";
		string text = string.Format("\nH|%1 — %2%3", Esc(record.m_sName), SRP_Grades.GetName(record.m_iGrade), staff);
		text += "\nL|Certifs : " + SRP_Certifs.MaskToString(record.m_iCertifs);
		text += string.Format("\nL|%1 h de jeu, %2 mission(s) réussie(s), %3 infraction(s), %4 € portés", record.m_iPlaytimeSeconds / 3600, record.m_iMissions, record.m_iInfractions, record.m_iCash);
		SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
		if (charte)
			text += "\nL|Charte : " + Esc(charte.StatusOf(record));
		IEntity character = Character(targetId);
		if (character)
		{
			text += "\nL|Position : " + DistanceText(playerId, character.GetOrigin()) + " de moi";
			text += "\nL|État : " + Esc(SRP_MedicalHelper.StateText(character));
		}
		else
			text += "\nC|d|Pas de personnage en ce moment (mort, en attente de réapparition)";
		SRP_DeathSnapshot death;
		if (m_mDeaths.Find(targetId, death))
			text += string.Format("\nC|d|Dernière mort il y a %1 min, carré %2", (System.GetTickCount() - death.m_iTick) / 60000, SRP_FleetManagerComponent.GridOf(death.m_vPosition));

		text += "\nH|Téléportation";
		text += string.Format("\nI|joueurs:tp:%1|» Aller vers lui", targetId);
		text += string.Format("\nI|joueurs:amener:%1|» L'amener à moi", targetId);

		text += "\nH|Soins";
		if (character && !SRP_Utils.IsDead(character))
			text += string.Format("\nI|joueurs:soigner:%1|» Le soigner complètement (et le réveiller s'il est inconscient)", targetId);
		else
			text += string.Format("\nK|g|joueurs:soigner:%1|» Il est mort : le faire réapparaître sur son corps avec son équipement (avant la fin du compte à rebours)", targetId);
		if (character && !SRP_Utils.IsDead(character) && m_mDeaths.Contains(targetId))
			text += string.Format("\nK|y|joueurs:corps:%1|» Le renvoyer sur son dernier corps avec son équipement d'avant la mort", targetId);
		text += string.Format("\nI|joueurs:etat:%1|» État médical détaillé (vivant, santé, sang, saignements, pouls, carré)", targetId);

		text += "\nH|Arme";
		text += string.Format("\nI|joueurs:arme-recreer:%1|» Recréer l'arme en main (arme hors service) : même prefab, accessoires et un chargeur", targetId);

		text += "\nH|Grade (forcé : les conditions ne sont pas vérifiées)";
		if (record.m_iGrade < SRP_EGrade.CAPITAINE)
			text += string.Format("\nK|g|joueurs:grade:%1:up|» Promouvoir : %2", targetId, SRP_Grades.GetName(record.m_iGrade + 1));
		if (record.m_iGrade > SRP_EGrade.RECRUE)
			text += string.Format("\nK|y|joueurs:grade:%1:down|» Rétrograder : %2", targetId, SRP_Grades.GetName(record.m_iGrade - 1));

		text += "\nH|Certifications (un clic accorde ou retire, sans vérifier l'arbre)";
		int certif = 1;
		while (certif <= SRP_Certifs.LAST)
		{
			if (certif == SRP_ECertif.INSTRUCTEUR)
			{
				certif = certif * 2;
				continue;
			}
			if (SRP_Certifs.Has(record.m_iCertifs, certif))
				text += string.Format("\nK|g|joueurs:certif:%1:%2|[x] %3", targetId, certif, SRP_Certifs.GetFullName(certif));
			else
				text += string.Format("\nK|w|joueurs:certif:%1:%2|[ ] %3", targetId, certif, SRP_Certifs.GetFullName(certif));
			certif = certif * 2;
		}

		text += "\nH|Instructeur de… (habilitation par certification, parmi celles qu'il détient)";
		bool anyHab = false;
		int hab = 1;
		while (hab <= SRP_Certifs.LAST)
		{
			if (hab != SRP_ECertif.INSTRUCTEUR && SRP_Certifs.Has(record.m_iCertifs, hab))
			{
				anyHab = true;
				if (SRP_Certifs.Has(record.m_iInstructeurCertifs, hab))
					text += string.Format("\nK|g|joueurs:hab:%1:%2|[x] Instructeur %3", targetId, hab, SRP_Certifs.GetName(hab));
				else
					text += string.Format("\nK|w|joueurs:hab:%1:%2|[ ] Instructeur %3", targetId, hab, SRP_Certifs.GetName(hab));
			}
			hab = hab * 2;
		}
		if (!anyHab)
			text += "\nC|d|Aucune certification détenue : accorde-lui d'abord une certification";

		text += "\nH|États de service";
		text += string.Format("\nI|joueurs:service:%1:1|» +1 mission réussie", targetId);
		text += string.Format("\nI|joueurs:service:%1:-1|» -1 mission réussie", targetId);

		text += "\nH|Discipline";
		text += string.Format("\nK|y|joueurs:warn:%1|» Avertir (compté sur la fiche)", targetId);
		if (targetId != playerId)
		{
			text += string.Format("\nK|y|joueurs:kick:%1|» Expulser du serveur", targetId);
			text += string.Format("\nK|r|joueurs:ban:%1:1|!! Bannir 1 jour (2 clics)", targetId);
			text += string.Format("\nK|r|joueurs:ban:%1:7|!! Bannir 7 jours (2 clics)", targetId);
			text += string.Format("\nK|r|joueurs:ban:%1:0|!! Bannir définitivement (2 clics)", targetId);
		}

		text += "\nH|Staff";
		if (record.m_bStaff)
			text += string.Format("\nK|g|joueurs:staff:%1|[x] Membre du Staff (clic : retirer)", targetId);
		else
			text += string.Format("\nK|w|joueurs:staff:%1|[ ] Membre du Staff (clic : donner)", targetId);

		text += "\nI|page:joueurs|« Retour à la liste";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Missions
	//------------------------------------------------------------------------------------------------
	protected string Missions(int playerId, array<string> parts)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "\nL|Gestionnaire de missions absent du game mode";

		array<SRP_Mission> actives = {};
		missions.GetActiveMissions(actives);
		int now = System.GetTickCount();

		string text = string.Format("\nH|Missions actives (%1)", actives.Count());
		foreach (SRP_Mission mission : actives)
		{
			int left = 0;
			string chef = "";
			if (!mission.m_sClaimedBy.IsEmpty())
			{
				left = mission.m_iFrozenRemainingMs / 60000;
				chef = " · chef " + Esc(mission.m_sClaimedBy) + " (timer gelé)";
			}
			else
				left = (mission.m_iDeadlineTick - now) / 60000;
			if (left < 0)
				left = 0;
			text += string.Format("\nC|w|%1 — %2 — %3 · difficulté %4 · reste %5 min%6 · %7", mission.m_sId, Esc(mission.m_sTitle), Esc(mission.m_sSiteLabel), mission.m_iDifficulty, left, chef, DistanceText(playerId, mission.m_vSite));
			text += string.Format("\nI|missions:tp:%1|    » Aller sur le site", mission.m_sId);
			text += string.Format("\nK|g|missions:reussir:%1|    » Réussir (test)", mission.m_sId);
			text += string.Format("\nK|r|missions:annuler:%1|    » Annuler", mission.m_sId);
		}
		if (actives.IsEmpty())
			text += "\nL|aucune";

		text += "\nH|Lancer une mission";
		text += "\nI|missions:nouvelle:-1|» Au hasard (selon les poids)";
		for (int type = 0; type <= SRP_EMissionType.LIBERER; type++)
			text += string.Format("\nI|missions:nouvelle:%1|» %2", type, SRP_MissionManagerComponent.TypeName(type));

		text += "\nH|Compteurs";
		text += Lines(missions.GetStatusText(), "");
		text += "\nK|r|missions:reset|!! Annuler toutes les missions actives (2 clics)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Territoire (délégué à SRP_FrontScreens, CONTRATS_FRONT §1.13)
	//------------------------------------------------------------------------------------------------
	//! Pages « territoire », « territoire:zone:S08 », « territoire:liste:<bleues|rouges|coupees>:<page> »,
	//! « territoire:sauvegardes:<page> », « territoire:points:<page> », « territoire:postes » : écrites par
	//! SRP_FrontScreens.StaffTab ; le Staff voit tout (pas de filtre I3). Hors jeu, les actions « ce carré » et
	//! « dépôt ici » ne sont pas proposées (inGame faux)
	protected string Territoire(int playerId, array<string> parts)
	{
		IEntity me = Character(playerId);
		bool inGame = me != null;
		vector staffPos = vector.Zero;
		if (me)
			staffPos = me.GetOrigin();
		return ProtocolBlock(SRP_FrontScreens.StaffTab(playerId, staffPos, inGame, parts));
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Commandeur (délégué à SRP_CmdScreens, CONTRATS_CMD §8)
	//------------------------------------------------------------------------------------------------
	//! Pages « commandeur », « commandeur:region:R3 », « commandeur:journal:<page>:<filtre> », « commandeur:forcer »,
	//! « commandeur:cible:<moyen> », « commandeur:ressources » : écrites par SRP_CmdScreens.StaffTab
	protected string Commandeur(int playerId, array<string> parts)
	{
		IEntity me = Character(playerId);
		bool inGame = me != null;
		vector staffPos = vector.Zero;
		if (me)
			staffPos = me.GetOrigin();
		return ProtocolBlock(SRP_CmdScreens.StaffTab(playerId, staffPos, inGame, parts));
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Logistique
	//------------------------------------------------------------------------------------------------
	protected string Logistique(int playerId, array<string> parts)
	{
		string text = "";

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		text += string.Format("\nH|Dépôts (+ ou - %1 paquets par clic)", m_iStockStep);
		if (resources)
		{
			for (int r = 0; r < SRP_Resources.COUNT; r++)
			{
				text += "\nL|" + Esc(resources.GetStockText(r));
				text += string.Format("\nI|logistique:stock:%1:plus|    » + %2 %3", r, m_iStockStep, SRP_Resources.GetName(r));
				text += string.Format("\nI|logistique:stock:%1:moins|    » - %2 %3", r, m_iStockStep, SRP_Resources.GetName(r));
				text += string.Format("\nI|logistique:stock:%1:max|    » Remplir %2", r, SRP_Resources.GetDepotName(r));
			}
		}
		else
			text += "\nL|gestionnaire de ressources absent";

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		text += "\nH|Coffre";
		if (treasury)
		{
			text += "\nL|" + Esc(treasury.GetBalanceText());
			text += string.Format("\nI|logistique:euros:plus|» + %1 €", m_iEurosStep);
			text += string.Format("\nI|logistique:euros:moins|» - %1 €", m_iEurosStep);
			text += string.Format("\nI|logistique:euros:gros|» + %1 €", m_iEurosStep * 10);
			text += "\nL|Commandes en attente : " + Esc(treasury.GetPendingText());
		}
		else
			text += "\nL|trésorerie absente";

		text += string.Format("\nH|Caisse devant moi (%1 paquets)", m_iCrateSize);
		for (int r = 0; r < SRP_Resources.COUNT; r++)
			text += string.Format("\nI|logistique:caisse:%1|» Caisse de %2", r, SRP_Resources.GetName(r));

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		text += "\nH|Parc automobile — choisis un véhicule pour le rejoindre";
		if (fleet)
		{
			array<SRP_FleetVehicle> vehicles = {};
			fleet.GetVehicles(vehicles);
			foreach (SRP_FleetVehicle vehicle : vehicles)
			{
				string tint = "g";
				string state = "en service";
				if (vehicle.m_iState == SRP_EVehicleState.DETRUIT)
				{
					tint = "r";
					state = "détruit";
				}
				else if (vehicle.m_iState == SRP_EVehicleState.EN_COMMANDE)
				{
					tint = "y";
					state = string.Format("en commande, %1 s", vehicle.m_iOrderSecondsLeft);
				}
				string where = "absent";
				IEntity entity = fleet.GetEntity(vehicle.m_sId);
				if (entity)
					where = DistanceText(playerId, entity.GetOrigin());
				text += string.Format("\nK|%1|logistique:vtp:%2|%3 %4 — %5 (%6) — %7", tint, vehicle.m_sId, vehicle.m_sId, Esc(vehicle.m_sName), state, SRP_VehicleCategories.GetName(vehicle.m_iCategory), where);
			}
			if (vehicles.IsEmpty())
				text += "\nL|aucun véhicule";
			text += "\nK|r|logistique:flotte-reset|!! Recréer la flotte de départ (2 clics)";
		}
		else
			text += "\nL|flotte absente";

		SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
		text += "\nH|Points de livraison et garnisons";
		if (deliveries)
			text += Lines(deliveries.GetStatusText(), "");
		else
			text += "\nL|gestionnaire de livraisons absent";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Serveur
	//------------------------------------------------------------------------------------------------
	protected string Serveur(int playerId, array<string> parts)
	{
		string text = "\nH|Heure";
		TimeAndWeatherManagerEntity weather = SRP_DayNightComponent.Weather();
		SRP_DayNightComponent dayNight = SRP_DayNightComponent.GetInstance();
		if (weather)
		{
			string speed = "";
			if (dayNight)
				speed = string.Format(" · une journée dure %1 h réelles", dayNight.GetRealHoursPerDay());
			text += "\nL|Il est " + SRP_DayNightComponent.HourText(weather.GetTimeOfTheDay()) + speed;
		}
		for (int h = 0; h < 24; h += 3)
			text += string.Format("\nI|serveur:heure:%1|» %2 h", h, h);
		text += "\nH|Vitesse du jour (heures réelles par journée)";
		text += "\nI|serveur:vitesse:2|» 2 h (très rapide)";
		text += "\nI|serveur:vitesse:6|» 6 h (réglage normal)";
		text += "\nI|serveur:vitesse:12|» 12 h";
		text += "\nI|serveur:vitesse:24|» 24 h (temps réel)";

		text += "\nH|Sauvegarde";
		text += "\nI|serveur:sauver|» Sauver toutes les fiches maintenant";

		// Chiffres réglables sans republier (Q6, C2) : un seul lecteur, SRP_CmdSettings, relu par le socle du front
		text += "\nH|Réglages du front et du Commandeur";
		text += "\nL|" + Esc(SRP_CmdSettings.GetStatusText());
		text += "\nI|serveur:reglages|» Relire les réglages (front_reglages.txt et commandeur_reglages.txt)";

		SRP_GenerateurComponent generateur = SRP_GenerateurComponent.GetInstance();
		text += "\nH|Générateur de la base";
		if (generateur)
		{
			text += Lines(generateur.GetStatusText(), "");
			text += "\nI|serveur:generateur:on|» Démarrer";
			text += "\nI|serveur:generateur:off|» Couper";
			text += "\nI|serveur:generateur:panne|» Mettre en panne (essai)";
			text += "\nI|serveur:generateur:reparer|» Réparer";
			if (SRP_GenerateurComponent.IsGmNightMode())
				text += "\nI|serveur:nuit|» Mode nuit du Game Master : ON (couper)";
			else
				text += "\nI|serveur:nuit|» Mode nuit du Game Master : OFF (activer)";
		}
		else
			text += "\nL|absent";

		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		text += "\nH|Ambiance civile";
		if (civils)
		{
			text += Lines(civils.GetStatusText(), "");
			text += "\nI|serveur:civils-reset|» Réinitialiser les civils";
		}
		else
			text += "\nL|absente";

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		text += "\nH|IA ennemie";
		if (enemies)
		{
			text += Lines(enemies.GetStatusText(), "");
			text += "\nK|y|serveur:ia-reset|» Retirer toute l'IA (patrouilles, garnisons, vagues) et annuler les opérations du Commandeur";
		}
		else
			text += "\nL|absente";

		SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
		text += "\nH|Déconnexions";
		if (disconnect)
			text += Lines(disconnect.GetStatusText(), "");
		else
			text += "\nL|composant absent";

		SRP_DiscordComponent discord = SRP_DiscordComponent.GetInstance();
		text += "\nH|Pont Discord";
		if (discord)
		{
			text += Lines(discord.GetStatusText(), "");
			text += "\nI|serveur:discord-test|» Envoyer un message de test sur tous les flux";
		}
		else
			text += "\nL|composant absent";

		SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
		text += "\nH|Charte";
		if (charte)
		{
			text += string.Format("\nL|Version %1", charte.GetVersion());
			text += "\nI|serveur:charte-recharger|» Recharger charte.txt";
		}
		else
			text += "\nL|composant absent";

		array<ref SRP_Ban> bans = SRP_Bans.All();
		text += string.Format("\nH|Bannis (%1) — choisis un ban pour le lever", bans.Count());
		foreach (SRP_Ban ban : bans)
		{
			string duration = "définitif";
			if (ban.m_iUntilDays > 0)
				duration = ban.m_iUntilDays.ToString() + " jour(s)";
			text += string.Format("\nK|r|serveur:unban:%1|» %2 — %3 — %4 (par %5, %6)", Esc(ban.m_sIdentity), Esc(ban.m_sName), Esc(ban.m_sReason), duration, Esc(ban.m_sAuthor), Esc(ban.m_sDate));
		}
		if (bans.IsEmpty())
			text += "\nL|aucun";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Onglet Danger
	//------------------------------------------------------------------------------------------------
	protected string Danger(int playerId)
	{
		string text = "\nC|y|Chaque ligne demande deux clics de suite. Les fiches joueurs ne sont jamais touchées.";
		text += "\nH|Tout";
		text += "\nK|r|danger:wipe|!! REMISE À ZÉRO COMPLÈTE : missions, IA et opérations du Commandeur, garnisons, civils, flotte, coffre, dépôts, front (nouvelle campagne), corps, sacoches, heure";
		text += "\nH|Par système";
		text += "\nK|r|danger:missions|!! Missions : tout annuler, compteurs à zéro, numéros repartent de M01";
		text += "\nK|r|danger:territoire|!! Territoire : nouvelle campagne — tout rouge sauf la base, menace 0, dépôts et caisses retirés (les sauvegardes datées restent)";
		text += "\nK|r|danger:flotte|!! Flotte : véhicules supprimés et flotte de départ recréée sur les places SRP_Parc";
		text += "\nK|r|danger:coffre|!! Coffre : solde initial, commandes en attente annulées";
		text += "\nK|r|danger:depots|!! Dépôts : vidés puis regarnis au niveau initial, caisses de livraison retirées";
		text += "\nK|r|danger:ia|!! IA ennemie : tout retirer (patrouilles, garnisons de livraison, vagues) et annuler les opérations du Commandeur (tirs, blindés, colonnes)";
		text += "\nK|r|danger:civils|!! Civils : réinitialisés";
		text += "\nK|r|danger:corps|!! Corps de déconnexion : retirés sans juger personne";
		text += "\nK|r|danger:sacoches|!! Sacoches d'argent au sol : retirées";
		text += "\nK|r|danger:charte|!! Charte : nouvelle version, tout le monde doit la réaccepter";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Exécution (commande « admin <clé> », serveur). L'écran est renvoyé avec le résultat en tête.
	//------------------------------------------------------------------------------------------------
	string Execute(int playerId, string key)
	{
		if (!IsStaff(playerId))
			return "Réservé au Staff";

		array<string> parts = {};
		key.Split(":", parts, true);
		if (parts.IsEmpty())
			return "";

		string author = Name(playerId);

		// Navigation pure
		if (parts[0] == "page")
		{
			SendMenu(playerId, Join(parts, 1));
			return "";
		}

		// Compatibilité avec « /wipe confirmer »
		if (parts[0] == "wipe")
		{
			if (parts.Count() >= 2 && parts[1] == "confirmer")
				return WipeServer(playerId);
			return "Pour confirmer : /wipe confirmer, ou l'onglet Danger du menu Staff";
		}

		string tab = parts[0];
		if (!IsTab(tab))
			return "Action inconnue : " + key;
		string action = "";
		if (parts.Count() >= 2)
			action = parts[1];

		// Pages internes d'un onglet (Territoire et Commandeur : la liste est tenue par leur module d'écrans)
		bool internalPage = (tab == "rapide" && (action == "zones" || action == "ia-groupes" || action == "reperages")) || (tab == "joueurs" && action == "fiche");
		if (tab == "territoire" && SRP_FrontScreens.IsInternalPage(action))
			internalPage = true;
		if (tab == "commandeur" && SRP_CmdScreens.IsInternalPage(action))
			internalPage = true;
		if (internalPage)
		{
			SendMenu(playerId, key);
			return "";
		}

		// Actions dangereuses : deux clics de suite sur la même ligne
		if (NeedsConfirm(tab, action))
		{
			string pending;
			if (!m_mPending.Find(playerId, pending) || pending != key)
			{
				m_mPending.Set(playerId, key);
				Reply(playerId, PageAfter(tab, action, parts), "!! Valide encore la même ligne (clic, ou A à la manette) pour confirmer");
				return "";
			}
		}
		m_mPending.Remove(playerId);

		string result = Run(playerId, tab, action, parts, author);
		Reply(playerId, PageAfter(tab, action, parts), result);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static bool NeedsConfirm(string tab, string action)
	{
		// Territoire (reset, restaurer, zone-reset, attaque-annuler, offensive) et Commandeur : leur module décide
		if (tab == "territoire")
			return SRP_FrontScreens.NeedsConfirm(action);
		if (tab == "commandeur")
			return SRP_CmdScreens.NeedsConfirm(action);
		if (tab == "danger")
			return true;
		if (tab == "joueurs" && action == "ban")
			return true;
		return action == "reset" || action == "flotte-reset";
	}

	//------------------------------------------------------------------------------------------------
	//! La page à rouvrir après une action : la fiche du joueur, la zone ou la région concernée, sinon l'onglet. Une
	//! action à deux clics doit rouvrir la page où se trouve sa ligne (le second clic se fait sur la même ligne)
	protected static string PageAfter(string tab, string action, array<string> parts)
	{
		if (tab == "territoire")
			return SRP_FrontScreens.PageAfter(action, parts);
		if (tab == "commandeur")
			return SRP_CmdScreens.PageAfter(action, parts);
		if (tab == "joueurs" && parts.Count() >= 3 && action != "kick" && action != "ban")
			return "joueurs:fiche:" + parts[2];
		if (tab == "rapide" && action == "tp" && parts.Count() >= 3 && parts[2] == "zone")
			return "rapide";
		return tab;
	}

	//------------------------------------------------------------------------------------------------
	protected void Reply(int playerId, string page, string message)
	{
		m_mLastMessage.Set(playerId, message);
		SendMenu(playerId, page);
	}

	//------------------------------------------------------------------------------------------------
	protected string Run(int playerId, string tab, string action, array<string> parts, string author)
	{
		if (tab == "rapide") return RunRapide(playerId, action, parts, author);
		if (tab == "joueurs") return RunJoueurs(playerId, action, parts, author);
		if (tab == "missions") return RunMissions(playerId, action, parts, author);
		if (tab == "territoire") return RunTerritoire(playerId, action, parts, author);
		if (tab == "commandeur") return RunCommandeur(playerId, action, parts, author);
		if (tab == "logistique") return RunLogistique(playerId, action, parts, author);
		if (tab == "serveur") return RunServeur(playerId, action, parts, author);
		if (tab == "danger") return RunDanger(playerId, action, author);
		return "Onglet inconnu";
	}

	//------------------------------------------------------------------------------------------------
	protected string RunRapide(int playerId, string action, array<string> parts, string author)
	{
		if (action == "helico")
		{
			IEntity hunted = Character(playerId);
			if (!hunted)
				return "Il faut être en jeu, dans un personnage";
			SRP_JournalComponent.Log("STAFF", author + " : hélicoptère de recherche (prototype) au-dessus de sa position");
			return SRP_HeliSearch.Launch(hunted.GetOrigin(), author);
		}
		if (action == "helico-stop")
			return SRP_HeliSearch.StopAll();
		if (action == "helico-forcer")
		{
			SRP_EnemyComponent heliEnemies = SRP_EnemyComponent.GetInstance();
			if (!heliEnemies)
				return "IA ennemie absente du game mode";
			// Avec un Commandeur (même gelé), plus de tirage sur alerte : l'ordre HELICO se force par l'onglet Commandeur
			if (SRP_Commander.Get())
				return "Sans effet : l'hélicoptère est décidé par le Commandeur. Pour le forcer : onglet Commandeur, « Forcer », « Hélico de recherche ».";
			heliEnemies.ForceNextHeliRoll();
			SRP_JournalComponent.Log("STAFF", author + " : prochain tirage de l'hélicoptère de recherche forcé (gagné, repos ignoré)");
			return "Prochain tirage de l'hélicoptère forcé : il sera gagné et le repos ignoré. Il faut une alerte en cours ou nouvelle (joueurs VUS près d'une mission active ou en territoire rouge), assez de joueurs, et être loin de la base.";
		}
		// Camion de renfort forcé (carte #69, livraison 3) : vers la localité ennemie gardée la plus proche (3 km), joueurs et
		// délais ignorés, règles de pose, de vue et de budget gardées
		if (action == "camion" || action == "camion-plein")
		{
			SRP_EnemyTruckComponent truckComponent = SRP_EnemyTruckComponent.GetInstance();
			if (!truckComponent)
				return "Camions de renfort absents du game mode (SRP_EnemyTruckComponent)";
			IEntity truckCaller = Character(playerId);
			if (!truckCaller)
				return "Il faut être en jeu, dans un personnage";
			bool truckFull = action == "camion-plein";
			string truckSize = "petit";
			if (truckFull)
				truckSize = "plein";
			SRP_JournalComponent.Log("STAFF", author + " : camion de renfort forcé (" + truckSize + ") vers la localité ennemie la plus proche");
			return truckComponent.ForceTruck(truckCaller.GetOrigin(), truckFull, author);
		}

		// Bascule d'essai du projecteur de l'hélicoptère : prise en compte au prochain ordre, sous 0,5 s
		if (action == "helico-appoint")
		{
			SRP_HeliSearch.s_bFillLight = !SRP_HeliSearch.s_bFillLight;
			SRP_JournalComponent.Log("STAFF", author + " : appoint au sol du projecteur d'hélicoptère " + SRP_Utils.OnOff(SRP_HeliSearch.s_bFillLight));
			return "Projecteur de l'hélicoptère : appoint au sol " + SRP_Utils.OnOff(SRP_HeliSearch.s_bFillLight);
		}

		// Contrôle routier sans équipier à regarder (essai seul, animation par le Staff) : mêmes règles de lieu
		if (action == "controle-ici")
		{
			SRP_MissionManagerComponent controlMissions = SRP_MissionManagerComponent.GetInstance();
			if (!controlMissions)
				return "Gestionnaire de missions absent";
			SRP_JournalComponent.Log("STAFF", author + " : point de contrôle routier établi par le menu Staff");
			return controlMissions.StartControl(playerId, true);
		}

		// Capture aérienne de la carte : caméra et écran sont ceux du joueur, donc seulement dans le Workbench
		// (serveur et joueur y sont le même programme)
		if (action == "capture-essai" || action == "capture-ile" || action == "capture-stop" || action == "capture-comparer")
		{
			string answer = "La capture de la carte ne se lance que dans le Workbench";
			#ifdef WORKBENCH
			if (action == "capture-stop")
				answer = SRP_MapCapture.Stop();
			else if (action == "capture-comparer")
				answer = SRP_MapCapture.StartCompare();
			else if (action == "capture-essai")
			{
				SRP_MapCapture.UseChosenSettings();
					answer = SRP_MapCapture.Start(7000, 4250, 8000, 5250, "CartePAG_essai");
			}
			else
			{
				SRP_MapCapture.UseChosenSettings();
					answer = SRP_MapCapture.Start(0, 0, 12800, 12800, "CartePAG");
			}
			SRP_JournalComponent.Log("STAFF", author + " : capture aérienne de la carte (" + action + ")");
			#endif
			return answer;
		}

		if (action == "tp")
		{
			if (parts.Count() >= 3 && parts[2] == "base")
			{
				IEntity marker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
				if (!marker)
					return "Marqueur de base introuvable : " + m_sBaseMarkerName;
				Teleport(playerId, marker.GetOrigin() + Vector(2, 0, 2));
				SRP_JournalComponent.Log("STAFF", author + " : téléportation à la base");
				return "Téléporté à la base";
			}
			if (parts.Count() >= 4 && parts[2] == "zone" && SRP_Utils.IsNumeric(parts[3]))
			{
				int index = parts[3].ToInt();
				if (index < 0 || index >= m_aSortedZones.Count() || !m_aSortedZones[index])
					return "Localité inconnue, rouvre la liste";
				SRP_CivilZoneComponent zone = m_aSortedZones[index];
				Teleport(playerId, zone.GetCenter() + Vector(3, 0, 3));
				SRP_JournalComponent.Log("STAFF", author + " : téléportation vers " + zone.GetZoneName());
				return "Téléporté : " + zone.GetZoneName();
			}
			return "Destination inconnue";
		}

		if (action == "soigner")
			return Heal(playerId, playerId, author);

		if (action == "god")
		{
			IEntity me = Character(playerId);
			if (!me)
				return "Pas de personnage";
			bool enable = !m_GodPlayers.Contains(playerId);
			if (enable)
				m_GodPlayers.Insert(playerId);
			else
				m_GodPlayers.RemoveItem(playerId);
			SetInvincible(me, enable);
			SRP_JournalComponent.Log("STAFF", author + " : mode invincible " + SRP_Utils.OnOff(enable));
			return "Mode invincible : " + SRP_Utils.OnOff(enable);
		}

		if (action == "heure")
			return SetHour(parts, author);

		return "Action inconnue";
	}

	//------------------------------------------------------------------------------------------------
	protected string RunJoueurs(int playerId, string action, array<string> parts, string author)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		if (parts.Count() < 3 || !SRP_Utils.IsNumeric(parts[2]))
			return "Joueur manquant";
		int targetId = parts[2].ToInt();
		SRP_PlayerRecord record = players.GetRecord(targetId);
		if (!record)
			return "Ce joueur n'est plus connecté";
		string targetName = record.m_sName;
		string reason;

		if (action == "tp")
		{
			IEntity target = Character(targetId);
			if (!target)
				return targetName + " n'a pas de personnage en ce moment";
			Teleport(playerId, target.GetOrigin() + Vector(2, 0, 2));
			SRP_JournalComponent.Log("STAFF", author + " : téléportation vers " + targetName);
			return "Téléporté vers " + targetName;
		}

		if (action == "amener")
		{
			IEntity me = Character(playerId);
			IEntity target = Character(targetId);
			if (!me || !target)
				return "Personnage introuvable";
			Teleport(targetId, me.GetOrigin() + Vector(2, 0, 2));
			SRP_JournalComponent.Log("STAFF", string.Format("%1 : %2 amené à lui", author, targetName));
			SRP_Utils.NotifyPlayer(targetId, "Le Staff t'a téléporté");
			return targetName + " amené";
		}

		if (action == "soigner")
			return Heal(playerId, targetId, author);

		if (action == "corps")
			return ReturnToBody(targetId, author);

		if (action == "etat")
			return StateOf(targetId);

		if (action == "arme-recreer")
			return RecreateWeapon(targetId, author);

		if (action == "grade")
		{
			int newGrade = record.m_iGrade + 1;
			if (parts.Count() >= 4 && parts[3] == "down")
				newGrade = record.m_iGrade - 1;
			if (!SRP_Grades.IsValide(newGrade))
				return "Grade impossible";
			if (!players.SetGrade(targetId, newGrade, author + " (menu Staff)", true, reason))
				return "Refusé : " + reason;
			SRP_Utils.NotifyPlayer(targetId, string.Format("Tu es maintenant %1 (décision du Staff)", SRP_Grades.GetName(newGrade)));
			return string.Format("%1 : %2", targetName, SRP_Grades.GetName(newGrade));
		}

		if (action == "certif")
		{
			if (parts.Count() < 4 || !SRP_Utils.IsNumeric(parts[3]))
				return "Certification manquante";
			int certif = parts[3].ToInt();
			bool grant = !SRP_Certifs.Has(record.m_iCertifs, certif);
			if (!players.SetCertif(targetId, certif, grant, author + " (menu Staff)", true, reason))
				return "Refusé : " + reason;
			if (grant)
				return string.Format("%1 : %2 accordée", targetName, SRP_Certifs.GetFullName(certif));
			return string.Format("%1 : %2 retirée%3", targetName, SRP_Certifs.GetFullName(certif), reason);
		}

		if (action == "hab")
		{
			if (parts.Count() < 4 || !SRP_Utils.IsNumeric(parts[3]))
				return "Certification manquante";
			int habCertif = parts[3].ToInt();
			bool habiliter = !SRP_Certifs.Has(record.m_iInstructeurCertifs, habCertif);
			if (!players.SetInstructeur(targetId, habCertif, habiliter, author + " (menu Staff)", reason))
				return "Refusé : " + reason;
			if (habiliter)
				return string.Format("%1 : instructeur %2", targetName, SRP_Certifs.GetName(habCertif));
			return string.Format("%1 : n'est plus instructeur %2", targetName, SRP_Certifs.GetName(habCertif));
		}

		if (action == "service")
		{
			int delta = 1;
			if (parts.Count() >= 4 && parts[3] == "-1")
				delta = -1;
			return players.AdjustMissions(targetId, delta, author);
		}

		if (action == "warn")
			return players.Warn(targetId, "avertissement du Staff (menu)", author);

		if (action == "kick")
		{
			if (targetId == playerId)
				return "Tu ne peux pas t'expulser toi-même";
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] expulsé par %3 (menu Staff)", targetName, record.m_sIdentity, author));
			SRP_BridgeComponent.PushEvent("sanction", string.Format("%1 expulsé par %2, depuis le menu Staff en jeu", targetName, author));
			GetGame().GetPlayerManager().KickPlayer(targetId, 0, 0);
			return targetName + " expulsé";
		}

		if (action == "ban")
		{
			if (targetId == playerId)
				return "Tu ne peux pas te bannir toi-même";
			if (record.m_bStaff)
				return "On ne bannit pas un membre du Staff par le menu";
			int days = 0;
			if (parts.Count() >= 4 && SRP_Utils.IsNumeric(parts[3]))
				days = parts[3].ToInt();
			SRP_Bans.Add(record.m_sIdentity, targetName, "banni par le Staff (menu)", author, days);
			GetGame().GetPlayerManager().KickPlayer(targetId, 0, 0);
			string duration = "définitivement";
			if (days > 0)
				duration = days.ToString() + " jour(s)";
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] banni %3 par %4 (menu Staff)", targetName, record.m_sIdentity, duration, author));
			SRP_BridgeComponent.PushEvent("sanction", string.Format("%1 banni %2 par %3, depuis le menu Staff en jeu", targetName, duration, author));
			return string.Format("%1 banni %2", targetName, duration);
		}

		if (action == "staff")
		{
			if (targetId == playerId)
				return "Pas sur toi-même";
			bool give = !record.m_bStaff;
			players.SetStaff(targetId, give, author);
			if (give)
				return targetName + " est maintenant membre du Staff";
			return targetName + " n'est plus membre du Staff";
		}

		return "Action inconnue";
	}

	//------------------------------------------------------------------------------------------------
	protected string RunMissions(int playerId, string action, array<string> parts, string author)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "Gestionnaire de missions absent du game mode";

		if (action == "nouvelle")
		{
			int type = -1;
			if (parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
				type = parts[2].ToInt();
			if (missions.Create(type))
			{
				SRP_JournalComponent.Log("STAFF", author + " : mission lancée par le menu");
				return "Mission créée";
			}
			return "Aucune mission créée (site ou prefabs manquants, voir le journal)";
		}

		if (action == "reset")
		{
			missions.CancelAll("annulation Staff par " + author);
			return "Missions actives annulées, le générateur en relance";
		}

		if (parts.Count() < 3)
			return "Mission manquante";
		SRP_Mission mission = missions.FindById(parts[2]);
		if (!mission)
			return "Mission inconnue : " + parts[2];

		if (action == "tp")
		{
			Teleport(playerId, mission.m_vSite + Vector(3, 0, 3));
			SRP_JournalComponent.Log("STAFF", author + " : téléportation sur le site de " + mission.m_sId);
			return "Téléporté sur le site de " + mission.m_sId;
		}
		if (action == "reussir")
		{
			missions.End(mission, SRP_EMissionState.SUCCES, "validée par " + author + " (menu Staff)");
			return mission.m_sId + " validée";
		}
		if (action == "annuler")
		{
			missions.End(mission, SRP_EMissionState.ANNULEE, "annulée par " + author + " (menu Staff)");
			return mission.m_sId + " annulée";
		}
		return "Action inconnue";
	}

	//------------------------------------------------------------------------------------------------
	//! Onglet Territoire : SRP_FrontScreens.RunStaff exécute (reset, gel, relire, carre-bleu, carre-rouge, tp, tp-centre,
	//! zone-bleue, zone-rouge, zone-reset, attaque, attaque-annuler, offensive, depot-ici, depot-retirer, restaurer,
	//! pc-tp, pc-saisir, pc-reposer) et écrit la SEULE ligne [STAFF] de l'action ; ici, seulement la téléportation
	protected string RunTerritoire(int playerId, string action, array<string> parts, string author)
	{
		IEntity me = Character(playerId);
		bool inGame = me != null;
		vector staffPos = vector.Zero;
		if (me)
			staffPos = me.GetOrigin();

		bool wantTeleport = false;
		vector teleportTo = vector.Zero;
		string result = SRP_FrontScreens.RunStaff(playerId, staffPos, inGame, action, parts, author, wantTeleport, teleportTo);
		return TeleportAfter(playerId, inGame, wantTeleport, teleportTo, result);
	}

	//------------------------------------------------------------------------------------------------
	//! Onglet Commandeur : SRP_CmdScreens.RunStaff exécute et journalise (SRP_CmdLog.StaffAction, catégorie
	//! COMMANDEUR, jamais « STAFF » : l'onglet Journal du PC montre les lignes STAFF à tous) ; ici, seulement la
	//! téléportation (cachette du jour, dépôt ennemi)
	protected string RunCommandeur(int playerId, string action, array<string> parts, string author)
	{
		IEntity me = Character(playerId);
		bool inGame = me != null;
		vector staffPos = vector.Zero;
		if (me)
			staffPos = me.GetOrigin();

		bool wantTeleport = false;
		vector teleportTo = vector.Zero;
		string result = SRP_CmdScreens.RunStaff(playerId, staffPos, inGame, action, parts, author, wantTeleport, teleportTo);
		return TeleportAfter(playerId, inGame, wantTeleport, teleportTo, result);
	}

	//------------------------------------------------------------------------------------------------
	//! Téléportation demandée par un module d'écrans (Territoire, Commandeur) : faite ici, par Teleport (C3 compris) ;
	//! hors jeu, rien ne bouge et le compte rendu le dit
	protected string TeleportAfter(int playerId, bool inGame, bool wantTeleport, vector teleportTo, string result)
	{
		if (!wantTeleport)
			return result;
		if (!inGame)
			return result + " — sans effet : il faut être en jeu, dans un personnage";
		Teleport(playerId, teleportTo);
		return result;
	}

	//------------------------------------------------------------------------------------------------
	protected string RunLogistique(int playerId, string action, array<string> parts, string author)
	{
		// Dépôts, coffre, caisse : exactement le même chemin que les commandes /stock, /euros et /caisse
		if (action == "stock")
		{
			SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
			if (!resources)
				return "Gestionnaire de ressources absent";
			if (parts.Count() < 4 || !SRP_Utils.IsNumeric(parts[2]))
				return "Ressource manquante";
			int r = parts[2].ToInt();
			if (!SRP_Resources.IsValide(r))
				return "Ressource inconnue";

			string value = "";
			if (parts[3] == "plus")
				value = "+" + m_iStockStep.ToString();
			else if (parts[3] == "moins")
				value = "-" + m_iStockStep.ToString();
			else if (parts[3] == "max")
			{
				int capacity = resources.GetCapacity(r);
				value = capacity.ToString();
			}
			else
				return "Opération inconnue";

			int before = resources.GetStock(r);
			string result = SRP_CommandProcessor.Execute(playerId, "stock", SRP_Resources.GetName(r) + " " + value);
			if (resources.GetStock(r) == before)
			{
				string error = SRP_Packets.GetLastError();
				if (!error.IsEmpty())
					result += " — " + error;
				else
					result += " — aucun paquet déplacé (dépôt plein, vide, ou prefab de paquet non réglé ?)";
			}
			return result;
		}

		if (action == "euros")
		{
			string value = "";
			if (parts.Count() >= 3 && parts[2] == "plus")
				value = "+" + m_iEurosStep.ToString();
			else if (parts.Count() >= 3 && parts[2] == "gros")
			{
				int big = m_iEurosStep * 10;
				value = "+" + big.ToString();
			}
			else if (parts.Count() >= 3 && parts[2] == "moins")
				value = "-" + m_iEurosStep.ToString();
			else
				return "Opération inconnue";
			return SRP_CommandProcessor.Execute(playerId, "euros", value);
		}

		if (action == "caisse")
		{
			if (parts.Count() < 3 || !SRP_Utils.IsNumeric(parts[2]))
				return "Ressource manquante";
			int r = parts[2].ToInt();
			if (!SRP_Resources.IsValide(r))
				return "Ressource inconnue";
			return SRP_CommandProcessor.Execute(playerId, "caisse", SRP_Resources.GetName(r) + " " + m_iCrateSize.ToString());
		}

		if (action == "vtp")
		{
			SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
			if (!fleet)
				return "Flotte absente";
			if (parts.Count() < 3)
				return "Véhicule manquant";
			IEntity vehicle = fleet.GetEntity(parts[2]);
			if (!vehicle)
				return parts[2] + " : pas de véhicule en jeu (détruit, en commande ou non chargé)";
			Teleport(playerId, vehicle.GetOrigin() + Vector(3, 0, 3));
			SRP_JournalComponent.Log("STAFF", author + " : téléportation vers le véhicule " + parts[2]);
			return "Téléporté vers " + parts[2];
		}

		if (action == "flotte-reset")
		{
			SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
			if (!fleet)
				return "Flotte absente";
			fleet.Reset();
			SRP_JournalComponent.Log("STAFF", author + " : flotte de départ recréée (menu Staff)");
			return "Flotte de départ recréée sur les places du parc";
		}

		return "Action inconnue";
	}

	//------------------------------------------------------------------------------------------------
	protected string RunServeur(int playerId, string action, array<string> parts, string author)
	{
		if (action == "heure")
			return SetHour(parts, author);

		if (action == "vitesse")
		{
			SRP_DayNightComponent dayNight = SRP_DayNightComponent.GetInstance();
			if (!dayNight)
				return "Composant jour/nuit absent";
			if (parts.Count() < 3 || !SRP_Utils.IsNumeric(parts[2]))
				return "Vitesse manquante";
			return dayNight.SetSpeed(parts[2].ToInt(), author);
		}

		if (action == "sauver")
		{
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			if (!players)
				return "Gestionnaire de joueurs absent";
			players.SaveAll();
			SRP_JournalComponent.Log("STAFF", author + " : sauvegarde manuelle des fiches");
			return "Fiches sauvegardées";
		}

		// Relit front_reglages.txt et commandeur_reglages.txt (SRP_CmdSettings), puis tous les LoadSettings
		if (action == "reglages")
		{
			SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
			if (!front)
				return "Front absent du game mode (SRP_FrontComponent)";
			string report = front.ReloadSettings(author);
			if (report.IsEmpty())
				report = "Réglages relus";
			// Ligne STAFF sobre : l'onglet Journal du PC la montre à tous (le compte rendu détaillé reste au Staff)
			SRP_JournalComponent.Log("STAFF", author + " : fichiers de réglages du serveur relus (menu Staff)");
			return report;
		}

		if (action == "generateur")
		{
			SRP_GenerateurComponent generateur = SRP_GenerateurComponent.GetInstance();
			if (!generateur)
				return "Générateur absent";
			string what = "";
			if (parts.Count() > 2)
				what = parts[2];
			if (what == "on")
				return generateur.Start(author);
			if (what == "off")
				return generateur.Stop(author);
			if (what == "panne")
				return generateur.Break(author);
			if (what == "reparer")
				return generateur.Repair(author);
			return "Ordre inconnu";
		}

		if (action == "nuit")
		{
			return SRP_GenerateurComponent.SetGmNightMode(!SRP_GenerateurComponent.IsGmNightMode(), playerId, author);
		}

		if (action == "civils-reset")
		{
			SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
			if (!civils)
				return "Ambiance civile absente";
			civils.Reset();
			SRP_JournalComponent.Log("STAFF", author + " : civils réinitialisés (menu Staff)");
			return "Civils réinitialisés";
		}

		if (action == "ia-reset")
			return ResetEnemies(author);

		if (action == "discord-test")
		{
			SRP_DiscordComponent discord = SRP_DiscordComponent.GetInstance();
			if (!discord)
				return "Pont Discord absent";
			return discord.SendTest(author);
		}

		if (action == "charte-recharger")
		{
			SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
			if (!charte)
				return "Charte absente";
			return charte.Reload(author);
		}

		if (action == "unban")
		{
			string identity = Join(parts, 2);
			if (identity.IsEmpty())
				return "Identité manquante";
			if (!SRP_Bans.Remove(identity))
				return "Aucun banni ne correspond";
			SRP_JournalComponent.Log("STAFF", author + " : ban levé pour " + identity + " (menu Staff)");
			return "Ban levé";
		}

		return "Action inconnue";
	}

	//------------------------------------------------------------------------------------------------
	protected string RunDanger(int playerId, string action, string author)
	{
		string reason = "remise à zéro par " + author + " (menu Staff)";

		if (action == "wipe")
			return WipeServer(playerId);

		if (action == "missions")
		{
			SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
			if (!missions)
				return "Gestionnaire de missions absent";
			missions.ResetAll(reason);
			return "Missions : tout annulé, compteurs à zéro";
		}
		// Nouvelle campagne du front (B3, J2) : archive, tout rouge sauf la base, menace 0, dépôts et caisses retirés ;
		// l'ennemi du front, les garnisons et le Commandeur sont remis à zéro par le socle (une seule porte)
		if (action == "territoire")
		{
			SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
			if (!front)
				return "Front absent du game mode (SRP_FrontComponent)";
			if (!front.IsReady())
				return "Front pas encore prêt";
			string campaign = front.NewCampaign(author);
			SRP_JournalComponent.Log("STAFF", author + " : territoire, nouvelle campagne (onglet Danger) — " + campaign);
			return "Territoire : " + campaign;
		}
		if (action == "flotte")
		{
			SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
			if (!fleet)
				return "Flotte absente";
			fleet.Reset();
			SRP_JournalComponent.Log("STAFF", author + " : flotte de départ recréée (menu Staff)");
			return "Flotte de départ recréée sur les places du parc";
		}
		if (action == "coffre")
		{
			SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
			if (!treasury)
				return "Trésorerie absente";
			treasury.ResetAll(reason);
			return "Coffre au solde initial, commandes annulées";
		}
		if (action == "depots")
		{
			SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
			if (!resources)
				return "Gestionnaire de ressources absent";
			resources.ResetAll(reason);
			return "Dépôts regarnis au niveau initial, caisses retirées";
		}
		if (action == "ia")
			return ResetEnemies(author);
		if (action == "civils")
		{
			SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
			if (!civils)
				return "Ambiance civile absente";
			civils.Reset();
			SRP_JournalComponent.Log("STAFF", author + " : civils réinitialisés (menu Staff)");
			return "Civils réinitialisés";
		}
		if (action == "corps")
		{
			SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
			if (!disconnect)
				return "Composant de déconnexion absent";
			int removed = disconnect.ResetAll();
			SRP_JournalComponent.Log("STAFF", string.Format("%1 : %2 corps de déconnexion retiré(s) (menu Staff)", author, removed));
			return string.Format("%1 corps retiré(s)", removed);
		}
		if (action == "sacoches")
		{
			int removed = SRP_MoneyComponent.DeleteAll();
			SRP_JournalComponent.Log("STAFF", string.Format("%1 : %2 sacoche(s) retirée(s) (menu Staff)", author, removed));
			return string.Format("%1 sacoche(s) retirée(s)", removed);
		}
		if (action == "charte")
		{
			SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
			if (!charte)
				return "Charte absente";
			return charte.ResetAll(author);
		}
		return "Action inconnue";
	}

	//------------------------------------------------------------------------------------------------
	// Actions partagées
	//------------------------------------------------------------------------------------------------
	//! Depuis la passerelle Discord : ramener un joueur à la base
	string BridgeTeleportToBase(int targetId, string author)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
		if (!marker)
			return "Marqueur de base introuvable : " + m_sBaseMarkerName;
		if (!Character(targetId))
			return "Pas de personnage";
		Teleport(targetId, marker.GetOrigin() + Vector(2, 0, 2));
		string targetName = Name(targetId);
		SRP_JournalComponent.Log("STAFF", author + " : " + targetName + " ramené à la base");
		SRP_Utils.NotifyPlayer(targetId, "Le Staff t'a ramené à la base");
		return targetName + " ramené à la base";
	}

	//------------------------------------------------------------------------------------------------
	//! Depuis la passerelle Discord : soin complet (réveil si inconscient, réapparition sur le corps si mort)
	string BridgeHeal(int targetId, string author)
	{
		return Heal(-1, targetId, author);
	}

	//------------------------------------------------------------------------------------------------
	//! Depuis la passerelle Discord : état médical
	string BridgeState(int targetId)
	{
		return StateOf(targetId);
	}

	//------------------------------------------------------------------------------------------------
	//! Depuis la passerelle Discord : recréer l'arme en main
	string BridgeRecreateWeapon(int targetId, string author)
	{
		return RecreateWeapon(targetId, author);
	}

	//------------------------------------------------------------------------------------------------
	//! « Nom : vivant · santé 80 % · sang 95 % · … », plus l'heure et le lieu de la dernière mort connue
	protected string StateOf(int targetId)
	{
		string targetName = Name(targetId);
		IEntity target = Character(targetId);
		string text = targetName + " : " + SRP_MedicalHelper.StateText(target);
		SRP_DeathSnapshot death;
		if (m_mDeaths.Find(targetId, death))
			text += string.Format(" · dernière mort il y a %1 min, carré %2", (System.GetTickCount() - death.m_iTick) / 60000, SRP_FleetManagerComponent.GridOf(death.m_vPosition));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Soin Staff : vivant = soins complets ; inconscient = soins et réveil ; mort = réapparition sur le corps
	protected string Heal(int playerId, int targetId, string author)
	{
		IEntity target = Character(targetId);
		if (!target || SRP_Utils.IsDead(target))
			return ReviveDead(targetId, target, author);

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(target.FindComponent(SCR_CharacterControllerComponent));
		bool wasUnconscious = controller && controller.IsUnconscious();
		if (!SRP_MedicalHelper.HealAndWake(target))
			return "Pas de gestion de dégâts sur ce personnage";

		string what = "soin complet";
		string done = "soigné";
		if (wasUnconscious)
		{
			what = "soin complet et réveil";
			done = "soigné et réveillé";
		}
		if (targetId == playerId)
		{
			SRP_JournalComponent.Log("STAFF", author + " : " + what);
			return "Soigné";
		}
		string targetName = Name(targetId);
		SRP_JournalComponent.Log("STAFF", author + " : " + what + " de " + targetName);
		SRP_Utils.NotifyPlayer(targetId, "Le Staff t'a " + done);
		return targetName + " " + done;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur mort (corps encore là, ou déjà retiré) : sa fiche reprend la position du corps et l'équipement
	//! d'avant la mort ; la réapparition déjà programmée par la logique de spawn le fera alors revenir sur
	//! place avec son inventaire au lieu de la base avec la dotation. À faire pendant le compte à rebours.
	protected string ReviveDead(int targetId, IEntity corpse, string author)
	{
		string targetName = Name(targetId);
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		SRP_PlayerRecord record = players.GetRecord(targetId);
		if (!record)
			return "Fiche introuvable pour " + targetName;

		SRP_DeathSnapshot death;
		bool hasDeath = m_mDeaths.Find(targetId, death);

		vector position;
		float yaw = 0;
		string where = "sur son corps";
		if (corpse)
		{
			position = corpse.GetOrigin();
			vector angles = corpse.GetYawPitchRoll();
			yaw = angles[0];
		}
		else if (hasDeath)
		{
			position = death.m_vPosition;
			yaw = death.m_fYaw;
		}
		else
		{
			IEntity marker = GetGame().GetWorld().FindEntityByName(m_sBaseMarkerName);
			if (!marker)
				return "Position du corps inconnue et marqueur de base introuvable : " + m_sBaseMarkerName;
			position = marker.GetOrigin();
			where = "à la base (position du corps inconnue)";
		}

		string inventory = "";
		if (hasDeath && death.m_sInventory.StartsWith("{"))
			inventory = death.m_sInventory;
		else if (record.m_sInventory.StartsWith("{"))
			inventory = record.m_sInventory;

		record.SetPosition(position, yaw);
		if (!inventory.IsEmpty())
			record.m_sInventory = inventory;
		record.Save();
		m_mDeaths.Remove(targetId);

		string gear = "avec son équipement";
		if (inventory.IsEmpty())
			gear = "avec la dotation (équipement d'avant la mort inconnu)";
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : %2 (mort) réapparaîtra %3 %4, carré %5", author, targetName, where, gear, SRP_FleetManagerComponent.GridOf(position)));
		SRP_Utils.NotifyPlayer(targetId, string.Format("Le Staff te fait réapparaître %1 %2", where, gear));
		return string.Format("%1 est mort : il réapparaîtra %2 %3 à la fin de son compte à rebours (s'il est déjà réapparu, utilise « Le renvoyer sur son dernier corps »)", targetName, where, gear);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur déjà réapparu à la base : téléporté sur son dernier corps, son équipement d'avant la mort réappliqué
	protected string ReturnToBody(int targetId, string author)
	{
		string targetName = Name(targetId);
		SRP_DeathSnapshot death;
		if (!m_mDeaths.Find(targetId, death))
			return "Aucune mort récente connue pour " + targetName;
		IEntity target = Character(targetId);
		if (!target || SRP_Utils.IsDead(target))
			return targetName + " n'a pas de personnage vivant : utilise « Le soigner » pendant son compte à rebours";

		Teleport(targetId, death.m_vPosition + Vector(1, 0, 1));

		bool restored = false;
		if (death.m_sInventory.StartsWith("{"))
		{
			SRP_ArsenalEconomyComponent economy = SRP_ArsenalEconomyComponent.GetInstance();
			if (economy)
			{
				economy.SetChargesSuppressed(targetId, true);
				GetGame().GetCallqueue().CallLater(economy.SetChargesSuppressed, 5000, false, targetId, false);
			}
			JsonLoadContext context = new JsonLoadContext();
			if (context.LoadFromString(death.m_sInventory))
				restored = SCR_PlayerArsenalLoadout.ApplyLoadoutString(target, context);
		}
		m_mDeaths.Remove(targetId);

		string gear = "équipement d'avant la mort réappliqué";
		if (!restored)
			gear = "équipement inchangé (réapplication impossible)";
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : %2 renvoyé sur son dernier corps, carré %3, %4", author, targetName, SRP_FleetManagerComponent.GridOf(death.m_vPosition), gear));
		SRP_Utils.NotifyPlayer(targetId, "Le Staff te renvoie sur ton dernier corps, " + gear);
		return string.Format("%1 renvoyé sur son dernier corps, %2", targetName, gear);
	}

	//------------------------------------------------------------------------------------------------
	//! Arme hors service (carte #57) : l'arme en main est supprimée puis recréée à l'identique dans le stockage
	//! d'armes du personnage, avec ses accessoires et un chargeur plein du type par défaut. Le joueur la reprend en main.
	protected string RecreateWeapon(int targetId, string author)
	{
		string targetName = Name(targetId);
		IEntity target = Character(targetId);
		if (!target || SRP_Utils.IsDead(target))
			return targetName + " n'a pas de personnage vivant";
		ChimeraCharacter chimera = ChimeraCharacter.Cast(target);
		if (!chimera)
			return "Personnage inattendu (pas un ChimeraCharacter)";
		BaseWeaponManagerComponent weaponManager = chimera.GetWeaponManager();
		if (!weaponManager)
			return "Pas de gestionnaire d'armes sur ce personnage";
		BaseWeaponComponent current = weaponManager.GetCurrentWeapon();
		if (!current)
			return targetName + " n'a pas d'arme en main";

		// L'entité de l'arme : emplacement du personnage, ou composant de l'arme elle-même
		IEntity weaponEntity;
		WeaponSlotComponent slot = WeaponSlotComponent.Cast(current);
		if (slot)
			weaponEntity = slot.GetWeaponEntity();
		if (!weaponEntity)
		{
			WeaponSlotComponent currentSlot = weaponManager.GetCurrentSlot();
			if (currentSlot)
				weaponEntity = currentSlot.GetWeaponEntity();
		}
		if (!weaponEntity)
			weaponEntity = current.GetOwner();
		if (!weaponEntity || weaponEntity == target)
			return "Arme en main introuvable";

		EntityPrefabData prefabData = weaponEntity.GetPrefabData();
		if (!prefabData)
			return "Arme sans prefab";
		ResourceName prefab = prefabData.GetPrefabName();
		string weaponName = SRP_Utils.PrefabShortName(prefab);

		// Accessoires attachés (sans les chargeurs) et chargeur par défaut de la bouche
		array<ResourceName> attachments = {};
		WeaponAttachmentsStorageComponent oldStorage = WeaponAttachmentsStorageComponent.Cast(weaponEntity.FindComponent(WeaponAttachmentsStorageComponent));
		if (oldStorage)
		{
			array<IEntity> items = {};
			oldStorage.GetAll(items, false);
			foreach (IEntity item : items)
			{
				if (!item || item.FindComponent(BaseMagazineComponent))
					continue;
				EntityPrefabData itemData = item.GetPrefabData();
				if (itemData)
					attachments.Insert(itemData.GetPrefabName());
			}
		}
		ResourceName magazinePrefab;
		array<BaseMuzzleComponent> muzzles = {};
		current.GetMuzzlesList(muzzles);
		foreach (BaseMuzzleComponent muzzle : muzzles)
		{
			if (!muzzle)
				continue;
			magazinePrefab = muzzle.GetDefaultMagazineOrProjectileName();
			if (!magazinePrefab.IsEmpty())
				break;
		}

		InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(target.FindComponent(InventoryStorageManagerComponent));
		EquipedWeaponStorageComponent weaponStorage = EquipedWeaponStorageComponent.Cast(target.FindComponent(EquipedWeaponStorageComponent));
		if (!inventory || !weaponStorage)
			return "Pas d'inventaire d'armes sur ce personnage";

		// Pas de facturation ni de contrôle d'arsenal pour cette recréation
		SRP_ArsenalEconomyComponent economy = SRP_ArsenalEconomyComponent.GetInstance();
		if (economy)
		{
			economy.SetChargesSuppressed(targetId, true);
			GetGame().GetCallqueue().CallLater(economy.SetChargesSuppressed, 5000, false, targetId, false);
		}

		// Suppression de l'arme (et de ce qu'elle porte : chargeur douteux compris)
		if (!inventory.TryDeleteItem(weaponEntity))
			SCR_EntityHelper.DeleteEntityAndChildren(weaponEntity);

		if (!inventory.TrySpawnPrefabToStorage(prefab, weaponStorage, -1, EStoragePurpose.PURPOSE_ANY, null, 1))
		{
			SRP_JournalComponent.Log("STAFF", string.Format("%1 : arme %2 de %3 supprimée mais impossible à recréer dans le stockage d'armes", author, weaponName, targetName));
			return string.Format("Arme %1 supprimée mais impossible à recréer : donne-lui une arme à l'arsenal", weaponName);
		}

		// La nouvelle arme : même prefab, dans le stockage d'armes
		IEntity newWeapon;
		array<IEntity> weapons = {};
		weaponStorage.GetAll(weapons, false);
		foreach (IEntity candidate : weapons)
		{
			if (!candidate || candidate == weaponEntity)
				continue;
			EntityPrefabData candidateData = candidate.GetPrefabData();
			if (candidateData && candidateData.GetPrefabName() == prefab)
				newWeapon = candidate;
		}

		// Accessoires : ceux que le prefab neuf a déjà ne sont pas redonnés
		int attached = 0;
		int refused = 0;
		if (newWeapon && !attachments.IsEmpty())
		{
			WeaponAttachmentsStorageComponent newStorage = WeaponAttachmentsStorageComponent.Cast(newWeapon.FindComponent(WeaponAttachmentsStorageComponent));
			if (newStorage)
			{
				array<string> alreadyThere = {};
				array<IEntity> present = {};
				newStorage.GetAll(present, false);
				foreach (IEntity item : present)
				{
					if (!item)
						continue;
					EntityPrefabData presentData = item.GetPrefabData();
					if (presentData)
						alreadyThere.Insert(presentData.GetPrefabName());
				}
				foreach (ResourceName attachment : attachments)
				{
					int index = alreadyThere.Find(attachment);
					if (index >= 0)
					{
						alreadyThere.Remove(index);
						continue;
					}
					if (inventory.TrySpawnPrefabToStorage(attachment, newStorage, -1, EStoragePurpose.PURPOSE_ANY, null, 1))
						attached++;
					else
						refused++;
				}
			}
			else
				refused = attachments.Count();
		}
		else if (!attachments.IsEmpty())
			refused = attachments.Count();

		// Un chargeur plein du type par défaut, rangé dans l'inventaire (le joueur recharge)
		string magazineText = "pas de chargeur (type par défaut inconnu)";
		if (!magazinePrefab.IsEmpty())
		{
			if (inventory.TrySpawnPrefabToStorage(magazinePrefab, null, -1, EStoragePurpose.PURPOSE_ANY, null, 1))
				magazineText = "chargeur " + SRP_Utils.PrefabShortName(magazinePrefab) + " ajouté";
			else
				magazineText = "chargeur " + SRP_Utils.PrefabShortName(magazinePrefab) + " refusé (inventaire plein ?)";
		}

		string summary = string.Format("arme %1 recréée pour %2 : %3 accessoire(s) remis, %4 refusé(s), %5", weaponName, targetName, attached, refused, magazineText);
		SRP_JournalComponent.Log("STAFF", author + " : " + summary);
		SRP_Utils.NotifyPlayer(targetId, string.Format("Le Staff a recréé ton arme (%1) : reprends-la en main et recharge", weaponName));
		return summary + " — il doit la reprendre en main";
	}

	//------------------------------------------------------------------------------------------------
	protected string SetHour(array<string> parts, string author)
	{
		if (parts.Count() < 3 || !SRP_Utils.IsNumeric(parts[2]))
			return "Heure manquante";
		int hour = parts[2].ToInt();
		if (hour < 0 || hour > 23)
			return "Heure invalide";
		SRP_DayNightComponent dayNight = SRP_DayNightComponent.GetInstance();
		if (dayNight)
			return dayNight.SetHour(hour, author);
		TimeAndWeatherManagerEntity weather = SRP_DayNightComponent.Weather();
		if (!weather)
			return "Pas de gestionnaire de temps dans le monde";
		weather.SetTimeOfTheDay(hour, true);
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : heure fixée à %2 h", author, hour));
		return string.Format("Heure : %1 h", hour);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire toute l'IA ennemie : d'abord les opérations du Commandeur (tirs en cours, blindés, colonnes et troupes en
	//! route rendues, officiers remis hors de vue : sans cela un mortier continuerait de tirer), puis garnisons de
	//! livraison, patrouilles, vagues, garnisons des localités
	protected string ResetEnemies(string author)
	{
		// CancelOperations écrit lui-même sa ligne au journal COMMANDEUR
		string commanderText = "";
		SRP_Commander commander = SRP_Commander.Get();
		if (commander)
			commanderText = commander.CancelOperations(author);

		SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
		if (deliveries)
			deliveries.ResetAll();
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
		{
			if (commanderText.IsEmpty())
				return "IA ennemie absente";
			return "IA ennemie absente · " + commanderText;
		}
		int before = enemies.CountAlive();
		enemies.ResetAll();
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : IA ennemie retirée (%2 agent(s)) (menu Staff)", author, before));
		string text = string.Format("IA ennemie retirée (%1 agent(s))", before);
		if (!commanderText.IsEmpty())
			text += " · " + commanderText;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à zéro complète, dans l'ordre : missions (objets et gardes), garnisons de livraison, opérations du
	//! Commandeur (tirs, blindés, colonnes), toute l'IA, civils, flotte, coffre, dépôts et caisses, front (nouvelle
	//! campagne : tout rouge sauf la base, menace 0, dépôts et caisses de zone, ennemi du front, garnisons et
	//! Commandeur remis à zéro par le socle), corps de déconnexion, sacoches au sol, heure du premier lancement,
	//! modes invincibles. Les fiches joueurs ne bougent pas.
	string WipeServer(int playerId)
	{
		string author = Name(playerId);
		string reason = "remise à zéro par " + author;
		SRP_JournalComponent.Log("STAFF", "REMISE À ZÉRO du serveur par " + author + " (fiches joueurs conservées)");

		string report = "Remise à zéro :";

		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
		{
			missions.ResetAll(reason);
			report += " missions,";
		}

		SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
		if (deliveries)
		{
			deliveries.ResetAll();
			report += " garnisons de livraison,";
		}

		// Avant l'IA : le Commandeur rend ses tickets et ses troupes en route, arrête ses tirs et retire ses blindés
		// (sa ligne part au journal COMMANDEUR)
		SRP_Commander commander = SRP_Commander.Get();
		if (commander)
		{
			commander.CancelOperations(author);
			report += " opérations du Commandeur,";
		}

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
		{
			enemies.ResetAll();
			report += " IA,";
		}

		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (civils)
		{
			civils.Reset();
			report += " civils,";
		}

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (fleet)
		{
			fleet.Reset();
			report += " flotte,";
		}

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (treasury)
		{
			treasury.ResetAll(reason);
			report += " coffre,";
		}

		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (resources)
		{
			resources.ResetAll(reason);
			report += " dépôts,";
		}

		// Front : nouvelle campagne, une seule porte (le socle prévient l'ennemi du front, les garnisons et le Commandeur)
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsReady())
		{
			front.NewCampaign(author);
			report += string.Format(" front (campagne n° %1),", front.GetCampaign());
		}

		SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
		if (disconnect)
		{
			int bodies = disconnect.ResetAll();
			report += string.Format(" %1 corps,", bodies);
		}

		int bags = SRP_MoneyComponent.DeleteAll();
		report += string.Format(" %1 sacoche(s),", bags);

		SRP_DayNightComponent dayNight = SRP_DayNightComponent.GetInstance();
		if (dayNight)
		{
			dayNight.ResetTime(author);
			report += " heure,";
		}

		// Modes invincibles
		for (int i = 0; i < m_GodPlayers.Count(); i++)
			SetInvincible(Character(m_GodPlayers.Get(i)), false);
		m_GodPlayers.Clear();
		m_mPending.Clear();

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.SaveAll();

		report += " fiches conservées";
		SRP_Utils.NotifyAll("Radio : le serveur a été remis à zéro par le Staff (missions, ennemis, parc, coffre, dépôts, nouvelle campagne du front). Vos fiches sont conservées.");
		return report;
	}

	//------------------------------------------------------------------------------------------------
	protected void SendMenu(int playerId, string page)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc)
			pc.SRP_OpenAdmin(BuildMenu(playerId, page));
	}

	//------------------------------------------------------------------------------------------------
	protected void SetInvincible(IEntity character, bool invincible)
	{
		if (!character)
			return;
		DamageManagerComponent damage = DamageManagerComponent.Cast(character.FindComponent(DamageManagerComponent));
		if (damage)
			damage.EnableDamageHandling(!invincible);
	}

	//------------------------------------------------------------------------------------------------
	//! Téléportation, méthode vanilla : chez le joueur s'il est à pied, sur le serveur (et diffusée) s'il est dans un véhicule
	protected void Teleport(int playerId, vector position)
	{
		// C3 : un joueur téléporté par le Staff ne compte pas pour la capture pendant quelques minutes
		SRP_FrontCapture.NoteTeleport(playerId);

		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.2;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!pc)
			return;

		IEntity character = Character(playerId);
		if (character && character.GetRootParent() != character)
		{
			SCR_Global.TeleportPlayer(playerId, position);
			pc.SRP_TeleportBroadcast(playerId, position);
			return;
		}

		pc.SRP_TeleportOwner(position);
	}
}

//------------------------------------------------------------------------------------------------
//! L'écran du menu Staff : le PC (onglets, zone défilante), mais une ligne s'exécute au clic
class SRP_AdminMenu : SRP_PCMenu
{
	protected static SRP_AdminMenu s_OpenAdmin;

	override protected string GetMenuTitle() { return "Menu Staff"; }
	override protected string GetConfirmLabel() { return "Exécuter"; }

	//------------------------------------------------------------------------------------------------
	//! Ouvre l'écran s'il ne l'est pas, puis y verse le contenu
	static void Open(string content)
	{
		if (!s_OpenAdmin)
		{
			s_OpenAdmin = SRP_AdminMenu.Cast(GetGame().GetMenuManager().OpenDialog(ChimeraMenuPreset.SRP_AdminMenu));
			if (!s_OpenAdmin)
			{
				Print("[SRP] Impossible d'ouvrir le menu Staff : preset SRP_AdminMenu absent de chimeraMenus.conf ?", LogLevel.ERROR);
				return;
			}
		}
		s_OpenAdmin.Fill(content);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (s_OpenAdmin == this)
			s_OpenAdmin = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! Un onglet : le serveur renvoie la page
	override void RequestTab(string key)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.SRP_RequestAdmin(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Un clic sur une ligne l'exécute tout de suite ; le serveur renvoie l'écran à jour
	override void Select(int index)
	{
		super.Select(index);
		Send(GetSelectedKey());
	}

	//------------------------------------------------------------------------------------------------
	//! Exécuter : rejoue la ligne sélectionnée (clavier, ou second clic de confirmation)
	override protected void OnConfirm()
	{
		string key = GetSelectedKey();
		if (key.IsEmpty())
		{
			if (SRP_IsGamepad())
				SetMessage(ChooseHint() + " ; les actions dangereuses se valident deux fois");
			else
				SetMessage("Clique sur une ligne ; les actions dangereuses demandent deux clics");
			return;
		}
		Send(key);
	}

	//------------------------------------------------------------------------------------------------
	protected void Send(string key)
	{
		if (key.IsEmpty())
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.SRP_SendMenuCommand("admin", key);
		SetMessage("…");
	}
}
