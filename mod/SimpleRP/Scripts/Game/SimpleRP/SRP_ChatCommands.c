//------------------------------------------------------------------------------------------------
// SimpleRP — Commandes de chat
// Côté client : le chat vanilla intercepte tout message commençant par "/" et cherche la commande
// dans le registre de SCR_ChatPanelManager. On y enregistre les nôtres via l'API officielle
// GetCommandInvoker (appel dans SRP_PlayerManagerComponent.OnPostInit). La commande part au
// serveur via SCR_PlayerController (RPC) et n'apparaît jamais dans le chat.
// Côté serveur : SRP_CommandProcessor vérifie les droits, exécute, journalise, répond.
//
// Commandes de chat conservées (tout le reste passe par une action en jeu, pour le RP) :
//   /aide            liste des commandes et où trouver les actions
//   /mp <pseudo> <message>   message privé à un joueur en ligne
//   /lier <code>     lier son compte Discord (code donné par /lier sur le Discord)
//   /vote            lien personnel pour voter pour la PAG sur top-serveurs (vote relié à la fiche, prime au coffre)
//   /charte          relire la charte (elle s'ouvre seule à la première connexion)
//   /admin           menu Staff, si la touche F10 n'est pas disponible
// Les autres commandes existent encore côté serveur (menu Staff, bot Discord), mais tapées au chat
// elles répondent par l'endroit où faire l'action : ordinateur (PC), Gestion du soldat, terminal…
// Phase 3 — étape C
//------------------------------------------------------------------------------------------------

//! Côté client : enregistrement des commandes dans le chat, envoi au serveur, affichage des réponses
class SRP_ChatCommands
{
	protected static ref SRP_ChatCommands s_Instance;

	//------------------------------------------------------------------------------------------------
	//! À appeler à chaque démarrage de partie (le gestionnaire de chat est recréé à chaque fois)
	static void Register()
	{
		SCR_ChatPanelManager mgr = SCR_ChatPanelManager.GetInstance();
		if (!mgr)
		{
			Print("[SRP] Gestionnaire de chat introuvable, commandes non enregistrées", LogLevel.WARNING);
			return;
		}

		if (!s_Instance)
			s_Instance = new SRP_ChatCommands();

		s_Instance.BindAll(mgr);
		Print("[SRP] Commandes de chat enregistrées", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	protected void BindAll(SCR_ChatPanelManager mgr)
	{
		ChatCommandInvoker inv;

		inv = mgr.GetCommandInvoker("srp");
		if (inv) inv.Insert(OnAide);

		inv = mgr.GetCommandInvoker("aide");
		if (inv) inv.Insert(OnAide);

		inv = mgr.GetCommandInvoker("mp");
		if (inv) inv.Insert(OnMp);

		inv = mgr.GetCommandInvoker("lier");
		if (inv) inv.Insert(OnLier);

		inv = mgr.GetCommandInvoker("vote");
		if (inv) inv.Insert(OnVote);

		inv = mgr.GetCommandInvoker("charte");
		if (inv) inv.Insert(OnCharte);

		inv = mgr.GetCommandInvoker("admin");
		if (inv) inv.Insert(OnAdmin);
	}

	//------------------------------------------------------------------------------------------------
	// Un gestionnaire par commande (signature imposée par ChatCommandCallback)
	//------------------------------------------------------------------------------------------------
	void OnAide(SCR_ChatPanel panel, string data)			{ SendToServer("aide", data); }
	void OnMoi(SCR_ChatPanel panel, string data)			{ SendToServer("moi", data); }
	void OnJoueurs(SCR_ChatPanel panel, string data)		{ SendToServer("joueurs", data); }
	void OnInfo(SCR_ChatPanel panel, string data)			{ SendToServer("info", data); }
	void OnGrade(SCR_ChatPanel panel, string data)			{ SendToServer("grade", data); }
	void OnCertif(SCR_ChatPanel panel, string data)			{ SendToServer("certif", data); }
	void OnInstructeur(SCR_ChatPanel panel, string data)	{ SendToServer("instructeur", data); }
	void OnStaff(SCR_ChatPanel panel, string data)			{ SendToServer("staff", data); }
	void OnKick(SCR_ChatPanel panel, string data)			{ SendToServer("kick", data); }
	void OnSauver(SCR_ChatPanel panel, string data)			{ SendToServer("sauver", data); }
	void OnStock(SCR_ChatPanel panel, string data)			{ SendToServer("stock", data); }
	void OnCaisse(SCR_ChatPanel panel, string data)			{ SendToServer("caisse", data); }
	void OnFlotte(SCR_ChatPanel panel, string data)			{ SendToServer("flotte", data); }
	void OnAcheter(SCR_ChatPanel panel, string data)		{ SendToServer("acheter", data); }
	void OnEuros(SCR_ChatPanel panel, string data)			{ SendToServer("euros", data); }
	void OnCivils(SCR_ChatPanel panel, string data)			{ SendToServer("civils", data); }
	void OnMissions(SCR_ChatPanel panel, string data)		{ SendToServer("missions", data); }
	void OnWipe(SCR_ChatPanel panel, string data)			{ SendToServer("wipe", data); }
	void OnService(SCR_ChatPanel panel, string data)		{ SendToServer("service", data); }
	void OnCharte(SCR_ChatPanel panel, string data)		{ SendToServer("charte", data); }
	void OnTerritoire(SCR_ChatPanel panel, string data)	{ SendToServer("territoire", data); }
	void OnMarqueurs(SCR_ChatPanel panel, string data)	{ SendToServer("marqueurs", data); }
	void OnHeure(SCR_ChatPanel panel, string data)		{ SendToServer("heure", data); }
	void OnDeco(SCR_ChatPanel panel, string data)		{ SendToServer("deco", data); }
	void OnBan(SCR_ChatPanel panel, string data)		{ SendToServer("ban", data); }
	void OnUnban(SCR_ChatPanel panel, string data)		{ SendToServer("unban", data); }
	void OnBans(SCR_ChatPanel panel, string data)		{ SendToServer("bans", data); }
	void OnWarn(SCR_ChatPanel panel, string data)		{ SendToServer("warn", data); }
	void OnLier(SCR_ChatPanel panel, string data)		{ SendToServer("lier", data); }
	void OnVote(SCR_ChatPanel panel, string data)		{ SendToServer("vote", data); }
	void OnMp(SCR_ChatPanel panel, string data)			{ SendToServer("mp", data); }

	//------------------------------------------------------------------------------------------------
	//! /admin : ouvre le menu Staff (le serveur vérifie le droit et envoie le contenu)
	void OnAdmin(SCR_ChatPanel panel, string data)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
		{
			ShowLocal("SimpleRP : pas de contrôleur joueur");
			return;
		}
		pc.SRP_RequestAdmin("");
	}

	//------------------------------------------------------------------------------------------------
	static void SendToServer(string command, string args)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc)
		{
			ShowLocal("SimpleRP : pas de contrôleur joueur, commande impossible");
			return;
		}

		pc.SRP_SendCommand(command, args);
	}

	//------------------------------------------------------------------------------------------------
	//! Affiche un texte dans le chat du joueur local (style message système)
	static void ShowLocal(string text)
	{
		SCR_ChatPanelManager mgr = SCR_ChatPanelManager.GetInstance();
		if (mgr)
			mgr.ShowHelpMessage(text);
		else
			Print("[SRP] " + text, LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
//! Côté serveur : exécution des commandes
class SRP_CommandProcessor
{
	//------------------------------------------------------------------------------------------------
	//! Retourne le texte de réponse à afficher au joueur (vide = rien)
	//! Entrée du chat : seules les commandes sans équivalent en jeu passent ; les autres renvoient vers l'action
	static string ExecuteFromChat(int playerId, string command, string args)
	{
		string cmd = command;
		cmd.ToLower();
		if (cmd == "srp" || cmd == "aide" || cmd == "mp" || cmd == "lier" || cmd == "vote" || cmd == "charte" || cmd == "admin")
			return Execute(playerId, cmd, args);
		return "Plus de commande /" + cmd + " : " + WhereInGame(cmd);
	}

	//------------------------------------------------------------------------------------------------
	//! Entrée des interfaces (terminal logistique, gestion du soldat, charte, menu Staff). Les droits du joueur
	//! sont vérifiés dans Execute, comme pour le chat : ce chemin ne donne aucun pouvoir de plus.
	static string ExecuteFromMenu(int playerId, string command, string args)
	{
		string cmd = command;
		cmd.ToLower();
		if (cmd == "terminal" || cmd == "soldat" || cmd == "charte" || cmd == "admin" || cmd == "demande" || cmd == "controle")
			return Execute(playerId, cmd, args);
		return "Action d'interface inconnue : " + cmd;
	}

	//------------------------------------------------------------------------------------------------
	//! Où se fait l'action en jeu, pour une commande retirée du chat
	protected static string WhereInGame(string cmd)
	{
		if (cmd == "moi" || cmd == "joueurs" || cmd == "info")
			return "ordinateur (PC), onglets Ma fiche et Effectifs";
		if (cmd == "grade" || cmd == "certif" || cmd == "instructeur" || cmd == "service" || cmd == "warn" || cmd == "soldat")
			return "menu Gestion du soldat, en regardant le joueur";
		if (cmd == "missions")
			return "ordinateur (PC), onglet Missions, ou le tableau des missions";
		if (cmd == "territoire")
			return "ordinateur (PC), onglet Territoire, et la carte (M)";
		if (cmd == "flotte" || cmd == "euros" || cmd == "heure")
			return "ordinateur (PC), onglet Base";
		if (cmd == "acheter")
			return "terminal logistique, à l'atelier";
		if (cmd == "deco")
			return "ordinateur (PC), onglet Officiers";
		return "menu Staff (F10, Vue + Carré maintenus à la manette, ou /admin)";
	}

	//------------------------------------------------------------------------------------------------
	//! Exécution complète, pour le menu Staff et les appels internes
	static string Execute(int playerId, string command, string args)
	{
		if (!Replication.IsServer())
			return "";

		SRP_PlayerManagerComponent manager = SRP_PlayerManagerComponent.GetInstance();
		if (!manager)
			return "SimpleRP : gestionnaire de joueurs absent du game mode";

		manager.EnsureRegistered(playerId);
		SRP_PlayerRecord me = manager.GetRecord(playerId);
		if (!me)
			return "SimpleRP : fiche introuvable, réessaie dans quelques secondes";

		array<string> tokens = {};
		args.Split(" ", tokens, true);

		string cmd = command;
		cmd.ToLower();

		if (cmd == "srp" || cmd == "aide")
			return Aide(manager, playerId);
		if (cmd == "moi")
			return Moi(manager, me);
		if (cmd == "joueurs")
			return Joueurs(manager, playerId);
		if (cmd == "info")
			return Info(manager, playerId, tokens);
		if (cmd == "grade")
			return Grade(manager, playerId, me, tokens);
		if (cmd == "certif")
			return Certif(manager, playerId, me, tokens);
		if (cmd == "instructeur")
			return Instructeur(manager, playerId, me, tokens);
		if (cmd == "staff")
			return Staff(manager, playerId, me, tokens);
		if (cmd == "kick" || cmd == "expulser")
			return Kick(manager, playerId, me, tokens);
		if (cmd == "lier")
			return SRP_BridgeComponent.Link(me, tokens);
		if (cmd == "vote")
			return SRP_BridgeComponent.RequestVote(me);
		if (cmd == "sauver")
			return Sauver(manager, playerId);
		if (cmd == "stock")
			return Stock(manager, playerId, me, tokens);
		if (cmd == "caisse")
			return Caisse(manager, playerId, me, tokens);
		if (cmd == "flotte")
			return Flotte(manager, playerId, me, tokens);
		if (cmd == "acheter")
			return Acheter(manager, playerId, me, tokens);
		if (cmd == "euros")
			return Euros(manager, playerId, me, tokens);
		if (cmd == "civils")
			return Civils(manager, playerId, me, tokens);
		if (cmd == "missions")
			return Missions(manager, playerId, me, tokens);
		if (cmd == "bans")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";
			return SRP_Bans.ListText();
		}

		if (cmd == "unban")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";
			if (tokens.IsEmpty())
				return "Usage : /unban <pseudo ou identité>";
			if (!SRP_Bans.Remove(tokens[0]))
				return "Aucun banni ne correspond à " + tokens[0];
			SRP_JournalComponent.Log("STAFF", string.Format("%1 lève le bannissement de %2", Auteur(me), tokens[0]));
			return tokens[0] + " n'est plus banni";
		}

		if (cmd == "ban")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";
			if (tokens.IsEmpty())
				return "Usage : /ban <pseudo> [jours] [motif]  (sans jours : définitif)";
			string error;
			int target = Cible(manager, tokens, 0, error);
			if (target < 0)
				return error;
			if (target == playerId)
				return "Tu ne peux pas te bannir toi-même";
			SRP_PlayerRecord cible = manager.GetRecord(target);
			if (cible.m_bStaff)
				return "On ne bannit pas un membre du Staff par commande";
			int days = 0;
			int start = 1;
			if (tokens.Count() >= 2 && SRP_Utils.IsNumeric(tokens[1]))
			{
				days = tokens[1].ToInt();
				start = 2;
			}
			string reason = "";
			for (int i = start; i < tokens.Count(); i++)
			{
				if (!reason.IsEmpty())
					reason += " ";
				reason += tokens[i];
			}
			if (reason.IsEmpty())
				reason = "aucun motif donné";
			SRP_Bans.Add(cible.m_sIdentity, cible.m_sName, reason, Auteur(me), days);
			string duration = "définitivement";
			if (days > 0)
				duration = string.Format("pour %1 jour(s)", days);
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] banni %3 par %4 : %5", cible.m_sName, cible.m_sIdentity, duration, Auteur(me), reason));
			GetGame().GetPlayerManager().KickPlayer(target, 0, 0);
			return string.Format("%1 banni %2 (%3)", cible.m_sName, duration, reason);
		}

		if (cmd == "warn")
		{
			if (!manager.IsOfficier(playerId))
				return "Réservé aux officiers";
			if (tokens.Count() < 2)
				return "Usage : /warn <pseudo> <motif>";
			string error;
			int target = Cible(manager, tokens, 0, error);
			if (target < 0)
				return error;
			string reason = "";
			for (int i = 1; i < tokens.Count(); i++)
			{
				if (!reason.IsEmpty())
					reason += " ";
				reason += tokens[i];
			}
			return manager.Warn(target, reason, Auteur(me));
		}

		if (cmd == "mp")
		{
			if (tokens.Count() < 2)
				return "Usage : /mp <pseudo> <message>";
			string error;
			int target = Cible(manager, tokens, 0, error);
			if (target < 0)
				return error;
			string message = "";
			for (int i = 1; i < tokens.Count(); i++)
			{
				if (!message.IsEmpty())
					message += " ";
				message += tokens[i];
			}
			SRP_Utils.NotifyPlayer(target, string.Format("[MP de %1] %2", me.m_sName, message));
			return string.Format("[MP à %1] %2", tokens[0], message);
		}

		if (cmd == "deco")
		{
			if (!manager.IsOfficier(playerId))
				return "Réservé aux officiers";
			SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
			if (!disconnect)
				return "Déconnexion absente du game mode (SRP_DisconnectComponent)";
			return disconnect.GetStatusText();
		}

		if (cmd == "heure")
		{
			TimeAndWeatherManagerEntity weather = SRP_DayNightComponent.Weather();
			if (!weather)
				return "Pas de gestionnaire d'heure dans le monde";
			if (tokens.IsEmpty())
				return "Il est " + SRP_DayNightComponent.HourText(weather.GetTimeOfTheDay());
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff : /heure <HH[:MM]>, /heure vitesse <heures réelles par journée>";
			string sub = tokens[0];
			sub.ToLower();
			if (sub == "vitesse")
			{
				SRP_DayNightComponent dayNight = SRP_DayNightComponent.GetInstance();
				if (!dayNight)
					return "Cycle jour/nuit absent du game mode (SRP_DayNightComponent)";
				if (tokens.Count() < 2)
					return "Usage : /heure vitesse <heures réelles par journée>";
				return dayNight.SetSpeed(tokens[1].ToFloat(), Auteur(me));
			}
			array<string> parts = {};
			sub.Split(":", parts, true);
			if (parts.IsEmpty() || !SRP_Utils.IsNumeric(parts[0]))
				return "Usage : /heure <HH[:MM]>";
			float hour = parts[0].ToInt();
			if (parts.Count() >= 2 && SRP_Utils.IsNumeric(parts[1]))
				hour += parts[1].ToInt() / 60.0;
			if (hour < 0 || hour >= 24)
				return "Heure entre 0 et 23";
			weather.SetTimeOfTheDay(hour, true);
			SRP_JournalComponent.Log("STAFF", string.Format("%1 règle l'heure à %2", Auteur(me), SRP_DayNightComponent.HourText(hour)));
			return "Il est maintenant " + SRP_DayNightComponent.HourText(hour);
		}

		if (cmd == "marqueurs")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";
			return MarkerCatalogue();
		}

		if (cmd == "territoire")
			return Territoire(manager, playerId, me, tokens);

		if (cmd == "charte")
		{
			SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
			if (!charte)
				return "Charte absente du game mode (SRP_CharteComponent)";
			if (tokens.IsEmpty())
			{
				charte.Open(playerId);
				return "";
			}
			string sub = tokens[0];
			sub.ToLower();
			if (sub == "accepter")
				return charte.Accept(playerId);
			if (sub == "reset")
			{
				if (!manager.IsStaff(playerId))
					return "Réservé au Staff";
				return charte.ResetAll(Auteur(me));
			}
			if (sub == "recharger")
			{
				if (!manager.IsStaff(playerId))
					return "Réservé au Staff";
				return charte.Reload(Auteur(me));
			}
			if (!manager.IsOfficier(playerId))
				return "Réservé aux officiers";
			string error;
			int target = Cible(manager, tokens, 0, error);
			if (target < 0)
				return error;
			return tokens[0] + " : " + charte.StatusOf(manager.GetRecord(target));
		}

		if (cmd == "service")
		{
			if (!manager.IsOfficier(playerId))
				return "Réservé aux officiers";
			if (tokens.Count() < 1)
				return "Usage : /service <pseudo> [+1|-1]  (mission réussie créditée après le débriefing)";
			string error;
			int target = Cible(manager, tokens, 0, error);
			if (target < 0)
				return error;
			int delta = 1;
			if (tokens.Count() >= 2 && tokens[1] == "-1")
				delta = -1;
			return manager.AdjustMissions(target, delta, Auteur(me));
		}

		if (cmd == "demande")
		{
			if (tokens.IsEmpty())
				return "La demande de mission se fait au tableau des missions";
			return SRP_MissionRequests.ExecuteMenu(playerId, tokens[0]);
		}

		if (cmd == "controle")
		{
			if (tokens.IsEmpty())
				return "Le contrôle se fait au contact d'un conducteur arrêté à un point de contrôle";
			return SRP_Checkpoint.ExecuteMenu(playerId, tokens[0]);
		}

		if (cmd == "soldat")
		{
			if (tokens.IsEmpty())
				return "Le menu Gestion du soldat s'ouvre en regardant un joueur";
			return manager.ExecuteSoldier(playerId, tokens[0]);
		}

		if (cmd == "wipe")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";
			SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
			if (!admin)
				return "Menu Staff absent du game mode (SRP_AdminComponent)";
			if (tokens.Count() == 0 || tokens[0] != "confirmer")
				return "Remise à zéro complète du serveur (missions, flotte, coffre, dépôts, IA, civils), sauf les fiches joueurs. Pour confirmer : /wipe confirmer";
			return admin.WipeServer(playerId);
		}

		if (cmd == "terminal")
		{
			SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
			if (!treasury)
				return "Trésorerie absente du game mode";
			if (tokens.IsEmpty())
				return "";
			return treasury.Terminal(playerId, tokens[0]);
		}

		if (cmd == "admin")
		{
			SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
			if (!admin)
				return "Menu Staff absent du game mode (SRP_AdminComponent)";
			if (tokens.IsEmpty())
				return "Usage : /admin (ouvre le menu)";
			return admin.Execute(playerId, tokens[0]);
		}

		return "Commande inconnue : /" + command;
	}

	//------------------------------------------------------------------------------------------------
	// Outils
	//------------------------------------------------------------------------------------------------
	//! Les icônes et couleurs des marqueurs de carte (config vanilla PLACED_CUSTOM), avec leurs index
	protected static string MarkerCatalogue()
	{
		SCR_MapMarkerManagerComponent markers = SCR_MapMarkerManagerComponent.GetInstance();
		if (!markers || !markers.GetMarkerConfig())
			return "Gestionnaire de marqueurs absent";
		SCR_MapMarkerEntryPlaced placed = SCR_MapMarkerEntryPlaced.Cast(markers.GetMarkerConfig().GetMarkerEntryConfigByType(SCR_EMapMarkerType.PLACED_CUSTOM));
		if (!placed)
			return "Entrée PLACED_CUSTOM introuvable dans le config des marqueurs";

		string icons = "Icônes (index : nom) :";
		int i = 0;
		while (i < 200)
		{
			ResourceName imageset;
			ResourceName glow;
			string quad;
			if (!placed.GetIconEntry(i, imageset, glow, quad))
				break;
			icons += string.Format("\n%1 : %2", i, quad);
			i++;
		}

		string colors = "Couleurs (index : R V B) :";
		array<ref SCR_MarkerColorEntry> entries = placed.GetColorEntries();
		int count = 0;
		if (entries)
			count = entries.Count();
		for (int c = 0; c < count; c++)
		{
			Color color = placed.GetColorEntry(c);
			if (!color)
				break;
			colors += string.Format("\n%1 : %2 %3 %4", c, Math.Round(color.R() * 255), Math.Round(color.G() * 255), Math.Round(color.B() * 255));
		}

		Print("[SRP] Marqueurs — " + icons, LogLevel.NORMAL);
		Print("[SRP] Marqueurs — " + colors, LogLevel.NORMAL);
		return string.Format("%1 icône(s), %2 couleur(s) : la liste complète est dans le log ([SRP] Marqueurs).\n%3", i, count, colors);
	}

	//------------------------------------------------------------------------------------------------
	//! Le mot est-il présent parmi les jetons (insensible à la casse) ?
	protected static bool HasWord(array<string> tokens, string word)
	{
		foreach (string token : tokens)
		{
			string lower = token;
			lower.ToLower();
			if (lower == word)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Auteur(SRP_PlayerRecord me)
	{
		string suffix = SRP_Grades.GetName(me.m_iGrade);
		if (me.m_bStaff)
			suffix = "Staff";
		return string.Format("%1 (%2)", me.m_sName, suffix);
	}

	//------------------------------------------------------------------------------------------------
	//! Retrouve le joueur visé par le token d'index donné. Retourne -1 et remplit error sinon.
	protected static int Cible(SRP_PlayerManagerComponent manager, array<string> tokens, int index, out string error)
	{
		if (tokens.Count() <= index)
		{
			error = "Il manque le pseudo du joueur";
			return -1;
		}

		int target = manager.FindPlayerIdByName(tokens[index]);
		if (target < 0)
			error = string.Format("Joueur '%1' introuvable : il doit être connecté, pseudo exact sans espace", tokens[index]);

		return target;
	}

	//------------------------------------------------------------------------------------------------
	//! Lit "on"/"off" (défaut : on)
	protected static bool OnOff(array<string> tokens, int index)
	{
		if (tokens.Count() <= index)
			return true;

		string s = tokens[index];
		s.ToLower();
		return !(s == "off" || s == "non" || s == "0" || s == "retirer");
	}

	//------------------------------------------------------------------------------------------------
	// Commandes
	//------------------------------------------------------------------------------------------------
	//! /territoire (bloc gardé cohérent : la commande n'est plus ouverte au chat, WhereInGame renvoie au PC et à la
	//! carte). Sans argument : l'état du front (attaques, zones au front). Staff : « capturer|perdre <code ou nom de
	//! zone> » (zone entière forcée, correction), « carre bleu|rouge » (carré sous le personnage), « gel on|off » et
	//! « reset » (nouvelle campagne). Tout passe par SRP_FrontScreens.RunStaff, comme le menu Staff : mêmes
	//! primitives, UNE seule ligne de journal [STAFF] par action, écrite par RunStaff.
	protected static string Territoire(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (tokens.IsEmpty())
			return SRP_FrontScreens.StatusPlain();

		string usage = "Usage : /territoire capturer S08, /territoire perdre S08 (code ou nom de la zone), /territoire carre bleu|rouge, /territoire gel on|off, /territoire reset";
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff. " + usage;

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return "Front pas encore prêt : réessaie dans quelques secondes";

		string sub = tokens[0];
		sub.ToLower();
		string action = "";
		string zoneCode = "";
		if (sub == "reset")
		{
			action = "reset";
		}
		else if (sub == "gel")
		{
			// RunStaff bascule le gel : on ne l'appelle que si l'état demandé diffère de l'état actuel
			bool wanted = OnOff(tokens, 1);
			if (wanted == front.IsFrozen())
			{
				if (wanted)
					return "Le front est déjà gelé";
				return "Le front n'est pas gelé";
			}
			action = "gel";
		}
		else if (sub == "carre" || sub == "carré")
		{
			if (tokens.Count() < 2)
				return usage;
			string color = tokens[1];
			color.ToLower();
			if (color == "bleu")
				action = "carre-bleu";
			else if (color == "rouge")
				action = "carre-rouge";
			else
				return usage;
		}
		else if (sub == "capturer" || sub == "perdre")
		{
			if (tokens.Count() < 2)
				return usage;
			string zoneText = tokens[1];
			for (int i = 2; i < tokens.Count(); i++)
				zoneText += " " + tokens[i];
			int zone = front.FindZone(zoneText);
			if (zone < 0)
				return "Zone inconnue : " + zoneText + " (code comme S08, nom ou libellé de la zone)";
			zoneCode = front.GetZoneCode(zone);
			if (sub == "capturer")
				action = "zone-bleue";
			else
				action = "zone-rouge";
		}
		else
		{
			return usage;
		}

		// Carré sous le personnage (carre bleu|rouge) : il faut être en jeu
		vector position = vector.Zero;
		bool inGame = false;
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (character)
		{
			position = character.GetOrigin();
			inGame = true;
		}

		// Même forme que les clés du menu Staff : « territoire:<action>[:<code>] »
		array<string> parts = {};
		parts.Insert("territoire");
		parts.Insert(action);
		if (!zoneCode.IsEmpty())
			parts.Insert(zoneCode);
		bool wantTeleport;
		vector teleportTo;
		return SRP_FrontScreens.RunStaff(playerId, position, inGame, action, parts, Auteur(me), wantTeleport, teleportTo);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Aide(SRP_PlayerManagerComponent manager, int playerId)
	{
		string help = "SimpleRP — au chat, seulement ce qui n'existe pas en jeu :";
		help += "\n/mp <pseudo> <message>  (message privé)";
		help += "\n/lier <code>  (lier son compte Discord, code donné par /lier sur le Discord)";
		help += "\n/vote  (voter pour la PAG sur top-serveurs : ton lien personnel, prime au coffre de la base)";
		help += "\n/charte  (relire la charte)";
		help += "\nTouches PAG : F9 portée de voix, F8 bouchons d'oreilles ; à la manette, L1 + croix haut et L1 + croix bas.";
		help += "\nTout le reste se fait sur place : l'ordinateur (PC) pour les effectifs, ta fiche, les instructeurs, les missions, le territoire, le journal et la base ; le menu Gestion du soldat en regardant un joueur pour les grades, certifications et habilitations ; le terminal logistique pour les achats ; le tableau des missions.";
		if (manager.IsStaff(playerId))
			help += "\nStaff : menu F10 (manette : Vue + Carré maintenus, ou /admin) pour tout le reste : joueurs, missions, territoire, logistique, serveur, bans, remise à zéro.";
		return help;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Moi(SRP_PlayerManagerComponent manager, SRP_PlayerRecord me)
	{
		return string.Format("%1\nInstructeur : %2 — connexions : %3 — infractions : %4 — argent porté : %5 €\nCertifs à portée : %6\nProchain grade : %7",
			me.ToSummary(),
			SRP_Certifs.MaskToString(me.m_iInstructeurCertifs),
			me.m_iConnections,
			me.m_iInfractions,
			me.m_iCash,
			manager.GetReachableText(me),
			manager.GetNextGradeText(me));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Joueurs(SRP_PlayerManagerComponent manager, int playerId)
	{
		if (!manager.IsOfficier(playerId))
			return "Réservé aux officiers";

		return string.Format("Connectés (%1) :\n%2", manager.GetConnectedCount(), manager.GetConnectedList());
	}

	//------------------------------------------------------------------------------------------------
	protected static string Info(SRP_PlayerManagerComponent manager, int playerId, array<string> tokens)
	{
		if (!manager.IsOfficier(playerId))
			return "Réservé aux officiers";

		string error;
		int target = Cible(manager, tokens, 0, error);
		if (target < 0)
			return error;

		return Moi(manager, manager.GetRecord(target));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Grade(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!manager.IsOfficier(playerId))
			return "Réservé aux officiers";

		string error;
		int target = Cible(manager, tokens, 0, error);
		if (target < 0)
			return error;

		if (tokens.Count() < 2)
			return "Usage : /grade <pseudo> <grade> [force] — grades : " + SRP_Grades.GetCommandNames();

		int newGrade = SRP_Grades.FromName(tokens[1]);
		if (newGrade < 0)
			return "Grade inconnu : " + tokens[1];

		if (!me.m_bStaff)
		{
			if (target == playerId)
				return "Tu ne peux pas changer ton propre grade";
			if (newGrade > me.m_iGrade)
				return "Tu ne peux pas donner un grade supérieur au tien";
		}

		bool force = manager.IsStaff(playerId) && HasWord(tokens, "force");
		string reason;
		if (!manager.SetGrade(target, newGrade, Auteur(me), force, reason))
			return "Refusé : " + reason;

		return string.Format("%1 est maintenant %2", tokens[0], SRP_Grades.GetName(newGrade));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Certif(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		string error;
		int target = Cible(manager, tokens, 0, error);
		if (target < 0)
			return error;

		if (tokens.Count() < 2)
			return "Usage : /certif <pseudo> <certif> [on|off] — certifs : " + SRP_Certifs.GetCommandNames();

		int certif = SRP_Certifs.FromName(tokens[1]);
		if (certif == 0)
			return "Certification inconnue : " + tokens[1] + " — certifs : " + SRP_Certifs.GetCommandNames();

		if (certif == SRP_ECertif.INSTRUCTEUR)
			return "Plus de certification Instructeur : on habilite par certification avec /instructeur";
		if (!manager.IsInstructeur(playerId, certif))
			return "Tu n'es pas instructeur " + SRP_Certifs.GetName(certif);

		bool accorder = OnOff(tokens, 2);
		bool force = manager.IsStaff(playerId) && HasWord(tokens, "force");
		string reason;
		if (!manager.SetCertif(target, certif, accorder, Auteur(me), force, reason))
		{
			if (manager.IsStaff(playerId))
				return "Refusé : " + reason + "\nPour passer outre : /certif " + tokens[0] + " " + tokens[1] + " on force";
			return "Refusé : " + reason;
		}

		if (accorder)
			return string.Format("Certification %1 accordée à %2", SRP_Certifs.GetFullName(certif), tokens[0]);
		return string.Format("Certification %1 retirée à %2%3", SRP_Certifs.GetFullName(certif), tokens[0], reason);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Instructeur(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!SRP_SoldierManagement.CanManageInstructors(me))
			return "Réservé à l'Adjudant, responsable des formations (et au Staff)";

		string error;
		int target = Cible(manager, tokens, 0, error);
		if (target < 0)
			return error;

		if (tokens.Count() < 2)
			return "Usage : /instructeur <pseudo> <certif> [on|off] — certifs : " + SRP_Certifs.GetCommandNames();

		int certif = SRP_Certifs.FromName(tokens[1]);
		if (certif == 0)
			return "Certification inconnue : " + tokens[1];

		bool habiliter = OnOff(tokens, 2);
		string reason;
		if (!manager.SetInstructeur(target, certif, habiliter, Auteur(me), reason))
			return "Refusé : " + reason;

		if (habiliter)
			return string.Format("%1 est maintenant instructeur %2", tokens[0], SRP_Certifs.GetName(certif));
		return string.Format("%1 n'est plus instructeur %2", tokens[0], SRP_Certifs.GetName(certif));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Staff(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff";

		string error;
		int target = Cible(manager, tokens, 0, error);
		if (target < 0)
			return error;

		bool staff = OnOff(tokens, 1);
		if (target == playerId && !staff)
			return "Tu ne peux pas te retirer toi-même du Staff";

		if (!manager.SetStaff(target, staff, Auteur(me)))
			return "Échec";

		return string.Format("%1 : staff = %2", tokens[0], SRP_Utils.OuiNon(staff));
	}

	//------------------------------------------------------------------------------------------------
	protected static string Kick(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff";

		string error;
		int target = Cible(manager, tokens, 0, error);
		if (target < 0)
			return error;

		if (target == playerId)
			return "Tu ne peux pas t'expulser toi-même";

		string raison = "";
		for (int i = 1; i < tokens.Count(); i++)
		{
			if (!raison.IsEmpty())
				raison += " ";
			raison += tokens[i];
		}
		if (raison.IsEmpty())
			raison = "aucune raison donnée";

		SRP_PlayerRecord cible = manager.GetRecord(target);
		SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] expulsé par %3 : %4", cible.m_sName, cible.m_sIdentity, Auteur(me), raison));

		GetGame().GetPlayerManager().KickPlayer(target, 0, 0);	// cause générique : le motif est dans le journal, pas à l'écran du joueur
		return string.Format("%1 expulsé (%2)", tokens[0], raison);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Sauver(SRP_PlayerManagerComponent manager, int playerId)
	{
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff";

		manager.SaveAll();
		return "Fiches sauvegardées";
	}

	//------------------------------------------------------------------------------------------------
	//! /stock                      tous les stocks
	//! /stock vivres               un stock
	//! /stock vivres 120           fixer
	//! /stock munitions +50        ajouter (plafonné)      /stock munitions -50   retirer (jusqu'à zéro)
	protected static string Stock(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff";

		SRP_ResourceManagerComponent res = SRP_ResourceManagerComponent.GetInstance();
		if (!res)
			return "Gestionnaire de ressources absent du game mode (SRP_ResourceManagerComponent)";

		if (tokens.Count() == 0)
			return "Stocks :\n" + res.GetAllStockText("\n");

		int resource = SRP_Resources.FromName(tokens[0]);
		if (resource < 0)
			return "Ressource inconnue : munitions, carburant, pieces, vivres";

		if (tokens.Count() < 2)
			return res.GetStockText(resource);

		string value = tokens[1];
		string first = value.Get(0);
		string reason = "commande de " + Auteur(me);

		if (first == "+" || first == "-")
		{
			string numStr = value.Substring(1, value.Length() - 1);
			if (!SRP_Utils.IsNumeric(numStr))
				return "Valeur invalide : " + value;

			int delta = numStr.ToInt();
			if (first == "+")
				res.Add(resource, delta, reason);
			else
				res.TakeUpTo(resource, delta, reason);
			res.Save();
		}
		else
		{
			if (!SRP_Utils.IsNumeric(value))
				return "Valeur invalide : " + value;
			res.SetStock(resource, value.ToInt(), reason);
		}

		return res.GetStockText(resource);
	}

	//------------------------------------------------------------------------------------------------
	//! /flotte : état du parc auto ; /flotte reset (Staff) : repartir de la composition
	protected static string Flotte(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (!fleet)
			return "Gestionnaire de flotte absent du game mode";

		if (tokens.Count() >= 1 && tokens[0] == "reset")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";
			fleet.Reset();
			return "Flotte réinitialisée depuis la composition du game mode";
		}

		return fleet.GetStatusText();
	}

	//------------------------------------------------------------------------------------------------
	//! /acheter : catalogue ; /acheter vehicule 2 ; /acheter caisse 0 ; clé du terminal (V:2, S:0)
	protected static string Acheter(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
			return "Trésorerie absente du game mode";

		if (tokens.Count() == 0)
		{
			string text = treasury.GetBalanceText();
			SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
			if (fleet)
				text += "\n" + fleet.GetCatalogueText();
			text += "\n" + treasury.GetCratesText();
			return text + "\nUsage : /acheter vehicule <n> | /acheter caisse <n>";
		}

		string first = tokens[0];
		string lower = first;
		lower.ToLower();

		// Clé du terminal telle quelle
		if (first.Contains(":"))
			return treasury.Buy(playerId, first);

		if (tokens.Count() < 2 || !SRP_Utils.IsNumeric(tokens[1]))
			return "Usage : /acheter vehicule <n> | /acheter caisse <n>";

		if (lower == "vehicule" || lower == "véhicule")
			return treasury.Buy(playerId, "V:" + tokens[1]);
		if (lower == "caisse" || lower == "caisses")
			return treasury.Buy(playerId, "S:" + tokens[1]);

		return "Usage : /acheter vehicule <n> | /acheter caisse <n>";
	}

	//------------------------------------------------------------------------------------------------
	//! /missions : missions en cours ; /missions M03 : détails ; Staff : /missions nouvelle [type],
	//! /missions annuler <id>, /missions reussir <id> (test), /missions reset
	protected static string Missions(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "Gestionnaire de missions absent du game mode";

		if (tokens.Count() == 0)
			return "Missions en cours :\n" + missions.GetBoardText() + "\nDétails : /missions <id>";

		string first = tokens[0];
		string lower = first;
		lower.ToLower();

		if (lower == "claim" || lower == "prendre" || lower == "accepter")
		{
			if (tokens.Count() < 2)
				return "Usage : /missions claim <id>";
			return missions.Claim(playerId, tokens[1]);
		}

		if (lower == "abandonner" || lower == "rendre")
		{
			if (tokens.Count() < 2)
				return "Usage : /missions abandonner <id>";
			return missions.Abandon(playerId, tokens[1]);
		}

		if (lower == "nouvelle" || lower == "annuler" || lower == "reussir" || lower == "reset")
		{
			if (!manager.IsStaff(playerId))
				return "Réservé au Staff";

			if (lower == "reset")
			{
				missions.CancelAll("réinitialisation par " + Auteur(me));
				return "Missions annulées, le générateur en relance";
			}

			if (lower == "nouvelle")
			{
				int type = -1;
				if (tokens.Count() >= 2)
				{
					type = SRP_MissionManagerComponent.TypeFromName(tokens[1]);
					if (type < 0)
						return "Types : " + SRP_MissionManagerComponent.TYPE_LIST;
				}
				if (missions.Create(type))
					return "Mission créée";
				return "Aucune mission créée (site ou prefabs manquants, voir le journal)";
			}

			if (tokens.Count() < 2)
				return "Usage : /missions " + lower + " <id>";
			SRP_Mission mission = missions.FindById(tokens[1]);
			if (!mission)
				return "Mission inconnue : " + tokens[1];

			if (lower == "annuler")
				missions.End(mission, SRP_EMissionState.ANNULEE, "annulée par " + Auteur(me));
			else
				missions.End(mission, SRP_EMissionState.SUCCES, "validée par " + Auteur(me) + " (test)");
			return "Fait";
		}

		SRP_Mission detail = missions.FindById(first);
		if (!detail)
			return "Mission inconnue : " + first + "\n" + missions.GetBoardText();
		return missions.GetDetailText(detail);
	}

	//------------------------------------------------------------------------------------------------
	//! /civils : état de l'ambiance civile ; /civils reset (Staff)
	protected static string Civils(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff";

		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (!civils)
			return "Gestionnaire d'ambiance civile absent du game mode";

		if (tokens.Count() > 0 && tokens[0] == "reset")
		{
			civils.Reset();
			return "Ambiance civile réinitialisée";
		}

		return civils.GetStatusText();
	}

	//------------------------------------------------------------------------------------------------
	//! /euros : solde ; Staff : /euros 5000, /euros +500, /euros -200
	protected static string Euros(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
			return "Trésorerie absente du game mode";

		if (tokens.Count() == 0)
			return treasury.GetBalanceText();

		if (!manager.IsStaff(playerId))
			return "Réservé au Staff : " + treasury.GetBalanceText();

		string value = tokens[0];
		string author = Auteur(me);
		if (value.StartsWith("+") || value.StartsWith("-"))
		{
			string digits = value.Substring(1, value.Length() - 1);
			if (!SRP_Utils.IsNumeric(digits))
				return "Usage : /euros <valeur|+n|-n>";
			int delta = digits.ToInt();
			if (value.StartsWith("+"))
				treasury.Add(delta, "commande de " + author);
			else if (!treasury.Spend(delta, "commande de " + author))
				return "Solde insuffisant — " + treasury.GetBalanceText();
			return treasury.GetBalanceText();
		}

		if (!SRP_Utils.IsNumeric(value))
			return "Usage : /euros <valeur|+n|-n>";

		treasury.SetBalance(value.ToInt(), "commande de " + author);
		return treasury.GetBalanceText();
	}

	//------------------------------------------------------------------------------------------------
	//! /caisse munitions 3 : fait apparaître 3 caisses de munitions devant le joueur
	protected static string Caisse(SRP_PlayerManagerComponent manager, int playerId, SRP_PlayerRecord me, array<string> tokens)
	{
		if (!manager.IsStaff(playerId))
			return "Réservé au Staff";

		SRP_ResourceManagerComponent res = SRP_ResourceManagerComponent.GetInstance();
		if (!res)
			return "Gestionnaire de ressources absent du game mode";

		if (tokens.Count() == 0)
			return "Usage : /caisse <munitions|carburant|pieces|vivres> [paquets]  (une caisse de livraison pleine devant toi)";

		int resource = SRP_Resources.FromName(tokens[0]);
		if (resource < 0)
			return "Ressource inconnue : munitions, carburant, pieces, vivres";

		int packets = 20;
		if (tokens.Count() >= 2 && SRP_Utils.IsNumeric(tokens[1]))
			packets = tokens[1].ToInt();
		if (packets < 1)
			packets = 1;
		if (packets > 200)
			packets = 200;

		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!character)
			return "Pas de personnage";

		// Devant le joueur, légèrement en hauteur pour ne pas traverser le sol
		vector mat[4];
		character.GetTransform(mat);
		vector position = mat[3] + mat[2] * 2.0 + Vector(0, 0.3, 0);

		IEntity box = res.SpawnBox(resource, packets, position);
		if (!box)
			return "Aucune caisse créée : caisse de livraison ou paquet non réglé (SRP_ResourceManagerComponent)";

		int inside = SRP_ResourceManagerComponent.CountInBox(box, resource);
		SRP_JournalComponent.Log("STAFF", string.Format("%1 a fait apparaître une caisse de %2 paquets de %3", Auteur(me), inside, SRP_Resources.GetName(resource)));
		string reply = string.Format("Caisse de %1 paquets de %2 créée", inside, SRP_Resources.GetName(resource));
		if (!res.GetLastBoxError().IsEmpty())
			reply += "\nProblème : " + res.GetLastBoxError();
		return reply;
	}
}
