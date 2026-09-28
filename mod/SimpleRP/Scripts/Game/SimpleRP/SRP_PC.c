//------------------------------------------------------------------------------------------------
// SimpleRP — Terminal du PC
// Un ordinateur au poste de commandement (prefab SRP_PC : prop + action « Consulter le PC ») ouvre un
// écran à onglets (SRP_PCMenu, layout SRP_PC.layout : rangée d'onglets, zone défilante) :
//   Effectifs   joueurs connectés (cliquer un nom : sa fiche), puis l'annuaire de toutes les fiches
//   Ma fiche    grade, certifs, états de service, certifs à portée, prochain grade
//   Instructeurs   qui est habilité pour quelle certification
//   Missions    le tableau des missions, avec les prises en charge
//   Territoire  les zones du front (SRP_FrontScreens.PCTerritoire)
//   Journal     les dernières lignes du journal (sans les décisions du Commandeur ennemi ni les lignes FRONT, Staff seul)
//   Base        stocks, coffre, parc, ambiance
//   Officiers   (officiers et Staff) promotions en attente, commandes, IA, livraisons
// Le serveur construit le contenu d'un onglet à la demande (RPC) : premières lignes T|clé|libellé
// (les onglets), A|clé (l'onglet actif), puis M/H/L/I comme les autres menus. Une ligne I est un lien :
// la valider demande la vue correspondante (ex. fiche:<playerId>).
// Phase 5 — étape D
//------------------------------------------------------------------------------------------------

class SRP_PCAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc)
			pc.SRP_OpenPCTab(playerId, "effectifs");
	}

	override bool GetActionNameScript(out string outName)
	{
		outName = "Consulter le PC";
		return true;
	}

	override bool CanBeShownScript(IEntity user) { return true; }
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! Une case de la rangée d'onglets
class SRP_PCTab
{
	string m_sKey;
	string m_sLabel;
	Widget m_wRoot;
	TextWidget m_wLabel;
}

//------------------------------------------------------------------------------------------------
class SRP_PCTabHandler : ScriptedWidgetEventHandler
{
	protected SRP_PCMenu m_Menu;
	protected string m_sKey;

	void SRP_PCTabHandler(SRP_PCMenu menu, string key)
	{
		m_Menu = menu;
		m_sKey = key;
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Menu)
			m_Menu.RequestTab(m_sKey);
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! L'écran : le dialogue vanilla (SRP_PC.layout hérite de ConfigurableDialog, à ajuster dans l'éditeur de
//! layouts du Workbench) ; les onglets sont construits depuis le contenu, la liste par la mécanique commune
class SRP_PCMenu : SRP_ListMenu
{
	protected static SRP_PCMenu s_Open;

	protected Widget m_wTabs;
	protected ref array<ref SRP_PCTab> m_aTabs = {};
	protected ref array<ref SRP_PCTabHandler> m_aTabHandlers = {};
	protected string m_sActiveTab;

	override protected string GetMenuTitle() { return "Poste de commandement"; }
	override protected string GetConfirmLabel() { return "Ouvrir"; }
	override protected string GetCommand() { return ""; }

	//------------------------------------------------------------------------------------------------
	//! Ouvre l'écran s'il ne l'est pas, puis y verse le contenu
	static void Show(string content)
	{
		if (!s_Open)
		{
			s_Open = SRP_PCMenu.Cast(GetGame().GetMenuManager().OpenDialog(ChimeraMenuPreset.SRP_PCMenu));
			if (!s_Open)
			{
				Print("[SRP] Impossible d'ouvrir le PC : preset SRP_PCMenu absent de chimeraMenus.conf ?", LogLevel.ERROR);
				return;
			}
		}
		s_Open.Fill(content);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		m_wTabs = GetRootWidget().FindAnyWidget("Tabs");
		SetMessage("Chargement…");

		// Manette : L1 / R1 changent d'onglet (Q / E au clavier), actions vanilla du DialogContext
		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.AddActionListener("MenuTabLeft", EActionTrigger.DOWN, SRP_OnTabLeft);
			input.AddActionListener("MenuTabRight", EActionTrigger.DOWN, SRP_OnTabRight);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.RemoveActionListener("MenuTabLeft", EActionTrigger.DOWN, SRP_OnTabLeft);
			input.RemoveActionListener("MenuTabRight", EActionTrigger.DOWN, SRP_OnTabRight);
		}
		if (s_Open == this)
			s_Open = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_OnTabLeft()
	{
		SRP_StepTab(-1);
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_OnTabRight()
	{
		SRP_StepTab(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Onglet précédent (-1) ou suivant (1), en boucle ; le serveur renvoie la page
	protected void SRP_StepTab(int direction)
	{
		if (GetGame().GetMenuManager().GetTopMenu() != this)
			return;	// un autre écran (la charte) est par-dessus
		int count = m_aTabs.Count();
		if (count == 0)
			return;
		int current = 0;
		for (int i = 0; i < count; i++)
		{
			if (m_aTabs[i].m_sKey == m_sActiveTab)
				current = i;
		}
		int next = current + direction;
		if (next < 0)
			next = count - 1;
		if (next >= count)
			next = 0;
		RequestTab(m_aTabs[next].m_sKey);
	}

	//------------------------------------------------------------------------------------------------
	//! Les lignes T (onglets) et A (actif) sont lues ici, le reste par la mécanique commune
	override void Fill(string content)
	{
		array<string> lines = {};
		content.Split("\n", lines, true);

		string rest = "";
		array<string> tabKeys = {};
		array<string> tabLabels = {};
		string active = m_sActiveTab;
		foreach (string line : lines)
		{
			array<string> fields = {};
			line.Split("|", fields, false);
			if (fields.Count() >= 3 && fields[0] == "T")
			{
				tabKeys.Insert(fields[1]);
				tabLabels.Insert(fields[2]);
				continue;
			}
			if (fields.Count() >= 2 && fields[0] == "A")
			{
				active = fields[1];
				continue;
			}
			if (!rest.IsEmpty())
				rest += "\n";
			rest += line;
		}

		if (!tabKeys.IsEmpty())
			BuildTabs(tabKeys, tabLabels);
		m_sActiveTab = active;
		PaintTabs();

		super.Fill(rest);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildTabs(array<string> keys, array<string> labels)
	{
		if (!m_wTabs)
			return;

		bool same = keys.Count() == m_aTabs.Count();
		if (same)
		{
			foreach (int i, string key : keys)
			{
				if (m_aTabs[i].m_sKey != key)
					same = false;
			}
		}
		if (same)
			return;

		foreach (SRP_PCTab tab : m_aTabs)
		{
			if (tab.m_wRoot)
				tab.m_wRoot.RemoveFromHierarchy();
		}
		m_aTabs.Clear();
		m_aTabHandlers.Clear();

		foreach (int index, string key : keys)
		{
			Widget root = GetGame().GetWorkspace().CreateWidgets(ENTRY_LAYOUT, m_wTabs);
			if (!root)
				continue;
			LayoutSlot.SetPadding(root, 0, 0, 10, 0);
			LayoutSlot.SetHorizontalAlign(root, 0);
			root.SetFlags(WidgetFlags.NOFOCUS);	// manette : les onglets se changent à L1 / R1, la croix reste dans la liste
			SRP_PCTab tab = new SRP_PCTab();
			tab.m_sKey = key;
			tab.m_sLabel = labels[index];
			tab.m_wRoot = root;
			tab.m_wLabel = TextWidget.Cast(root.FindAnyWidget("Label"));
			if (tab.m_wLabel)
			{
				tab.m_wLabel.SetText(tab.m_sLabel);
				tab.m_wLabel.SetFlags(WidgetFlags.IGNORE_CURSOR);
			}
			SRP_PCTabHandler handler = new SRP_PCTabHandler(this, key);
			m_aTabHandlers.Insert(handler);
			root.AddHandler(handler);
			m_aTabs.Insert(tab);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintTabs()
	{
		foreach (SRP_PCTab tab : m_aTabs)
		{
			if (!tab.m_wRoot)
				continue;
			if (tab.m_sKey == m_sActiveTab)
				tab.m_wRoot.SetColor(Color.FromRGBA(210, 130, 30, 220));
			else
				tab.m_wRoot.SetColor(Color.FromRGBA(35, 40, 45, 200));
		}
	}

	//------------------------------------------------------------------------------------------------
	void RequestTab(string key)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.SRP_RequestPC(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Ouvrir : la ligne sélectionnée est un lien vers une autre vue
	override protected void OnConfirm()
	{
		string key = GetSelectedKey();
		if (key.IsEmpty())
		{
			if (SRP_IsGamepad())
				SetMessage(ChooseHint() + " ; ou change d'onglet (L1 / R1)");
			else
				SetMessage(ChooseHint() + " ; ou change d'onglet");
			return;
		}
		RequestTab(key);
	}
}

//------------------------------------------------------------------------------------------------
//! Construction des vues, côté serveur
class SRP_PCScreens
{
	//------------------------------------------------------------------------------------------------
	static string Build(int playerId, string tab)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "M|Gestionnaire de joueurs absent du game mode";
		players.EnsureRegistered(playerId);
		SRP_PlayerRecord me = players.GetRecord(playerId);
		if (!me)
			return "M|Fiche introuvable";

		bool officer = me.m_bStaff || SRP_Grades.IsOfficier(me.m_iGrade);

		string text = "T|effectifs|Effectifs\nT|moi|Ma fiche\nT|instructeurs|Instructeurs\nT|missions|Missions\nT|territoire|Territoire\nT|journal|Journal\nT|base|Base";
		if (officer)
			text += "\nT|officiers|Officiers";

		string key = tab;
		if (key.IsEmpty())
			key = "effectifs";

		if (key.StartsWith("fiche:"))
		{
			text += "\nA|effectifs\n" + Fiche(players, key.Substring(6, key.Length() - 6), me);
			return text;
		}
		if (key.StartsWith("dossier#"))
		{
			text += "\nA|effectifs\n" + Dossier(players, key.Substring(8, key.Length() - 8));
			return text;
		}

		if (key.StartsWith("mission:"))
		{
			array<string> parts = {};
			key.Split(":", parts, true);
			string result = "Ligne invalide";
			SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
			if (missions && parts.Count() >= 3)
			{
				if (parts[1] == "accepter")
					result = missions.Claim(playerId, parts[2]);
				else if (parts[1] == "annuler")
					result = missions.Abandon(playerId, parts[2]);
			}
			text += "\nA|missions\n" + Missions(result);
			return text;
		}

		text += "\nA|" + key + "\n";
		if (key == "moi") return text + FicheOf(players, me, true);
		if (key == "instructeurs") return text + Instructeurs(players);
		if (key == "missions") return text + Missions("");
		if (key == "territoire") return text + Territoire(playerId);
		if (key == "journal") return text + Journal();
		if (key == "base") return text + Base(officer);
		if (key == "officiers" && officer) return text + Officiers(players);
		return text + Effectifs(players, playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Effectifs(SRP_PlayerManagerComponent players, int playerId)
	{
		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);

		string text = string.Format("M|%1 connecté(s). Choisis un nom puis Ouvrir pour sa fiche.", ids.Count());
		text += "\nH|Connectés";
		// Tri par grade décroissant (petit effectif : sélection simple)
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
		foreach (int index : order)
		{
			SRP_PlayerRecord record = records[index];
			string staff = "";
			if (record.m_bStaff)
				staff = " [Staff]";
			text += string.Format("\nI|fiche:%1|%2 %3%4 — %5", ids[index], SRP_Grades.GetName(record.m_iGrade), record.m_sName, staff, SRP_Certifs.MaskToString(record.m_iCertifs));
		}

		// Annuaire : toutes les fiches, hors ligne comprises
		array<ref SRP_PlayerRecord> all = {};
		SRP_PlayerRecord.LoadAll(all);
		int offline = 0;
		string annuaire = "";
		foreach (SRP_PlayerRecord record : all)
		{
			bool connected = false;
			foreach (SRP_PlayerRecord online : records)
			{
				if (online.m_sIdentity == record.m_sIdentity)
					connected = true;
			}
			if (connected)
				continue;
			offline++;
			if (offline <= 60)
				annuaire += string.Format("\nI|dossier#%1|%2 %3 — %4 h, %5 mission(s)", record.m_sIdentity, SRP_Grades.GetName(record.m_iGrade), record.m_sName, record.m_iPlaytimeSeconds / 3600, record.m_iMissions);
		}
		if (offline > 0)
		{
			text += string.Format("\nH|Annuaire, hors ligne (%1)", offline);
			text += annuaire;
			if (offline > 60)
				text += "\nL|… et d'autres";
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Fiche(SRP_PlayerManagerComponent players, string playerIdText, SRP_PlayerRecord me)
	{
		if (!SRP_Utils.IsNumeric(playerIdText))
			return "M|Fiche invalide";
		SRP_PlayerRecord record = players.GetRecord(playerIdText.ToInt());
		if (!record)
			return "M|Ce joueur n'est plus connecté\nI|effectifs|Retour aux effectifs";
		return FicheOf(players, record, record == me) + "\nI|effectifs|Retour aux effectifs";
	}

	//------------------------------------------------------------------------------------------------
	protected static string Dossier(SRP_PlayerManagerComponent players, string identity)
	{
		SRP_PlayerRecord record = SRP_PlayerRecord.Load(identity);
		if (!record)
			return "M|Fiche introuvable\nI|effectifs|Retour aux effectifs";
		return FicheOf(players, record, false) + "\nI|effectifs|Retour aux effectifs";
	}

	//------------------------------------------------------------------------------------------------
	protected static string FicheOf(SRP_PlayerManagerComponent players, SRP_PlayerRecord record, bool mine)
	{
		string staff = "";
		if (record.m_bStaff)
			staff = " [Staff]";
		string text = string.Format("M|%1 — %2%3", record.m_sName, SRP_Grades.GetName(record.m_iGrade), staff);
		text += "\nH|Certifications";
		text += "\nL|" + SRP_Certifs.MaskToString(record.m_iCertifs);
		if (record.m_iInstructeurCertifs != 0)
			text += "\nL|Instructeur habilité : " + SRP_Certifs.MaskToString(record.m_iInstructeurCertifs);
		text += "\nH|États de service";
		text += string.Format("\nL|%1 h de jeu, %2 connexion(s), %3 mission(s) réussie(s), %4 infraction(s)", record.m_iPlaytimeSeconds / 3600, record.m_iConnections, record.m_iMissions, record.m_iInfractions);
		text += "\nL|Prochain grade : " + players.GetNextGradeText(record);
		SRP_CharteComponent charte = SRP_CharteComponent.GetInstance();
		if (charte)
			text += "\nL|Charte : " + charte.StatusOf(record);
		text += "\nL|Certifs à portée : " + players.GetReachableText(record);
		if (mine)
			text += string.Format("\nL|Argent porté : %1 €", record.m_iCash);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Instructeurs(SRP_PlayerManagerComponent players)
	{
		array<ref SRP_PlayerRecord> all = {};
		SRP_PlayerRecord.LoadAll(all);

		string text = "M|Qui peut délivrer quoi : un instructeur par certification. Le Staff délivre tout.";
		int certif = 1;
		while (certif <= SRP_Certifs.LAST)
		{
			if (certif != SRP_ECertif.INSTRUCTEUR)
			{
				string names = "";
				foreach (SRP_PlayerRecord record : all)
				{
					if (!SRP_Certifs.Has(record.m_iInstructeurCertifs, certif) || !SRP_Certifs.Has(record.m_iCertifs, certif))
						continue;
					if (!names.IsEmpty())
						names += ", ";
					names += record.m_sName;
				}
				if (names.IsEmpty())
					names = "personne";
				text += string.Format("\nL|%1 : %2", SRP_Certifs.GetFullName(certif), names);
			}
			certif = certif * 2;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Missions(string message)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "M|Gestionnaire de missions absent";
		string text = "M|" + message;
		if (message.IsEmpty())
			text = "M|Tableau des missions. Une mission non acceptée disparaît au bout du délai ; acceptée, elle reste jusqu'à sa réussite ou son annulation.";
		array<SRP_Mission> active = {};
		missions.GetActiveMissions(active);
		if (active.IsEmpty())
			text += "\nL|Aucune mission en cours";
		int now = System.GetTickCount();
		foreach (SRP_Mission mission : active)
		{
			text += string.Format("\nH|%1 — %2 — %3 (difficulté %4)", mission.m_sId, mission.m_sTitle, mission.m_sSiteLabel, mission.m_iDifficulty);
			text += "\nL|" + missions.TimeText(mission, now);
			if (mission.m_sClaimedBy.IsEmpty())
				text += string.Format("\nI|mission:accepter:%1|» Accepter %1 (CDG, sous-officiers, officiers)", mission.m_sId);
			else
				text += string.Format("\nI|mission:annuler:%1|» Annuler %1 (le chef, un officier ou le Staff)", mission.m_sId);
		}
		text += "\nL|Détails d'une mission : /missions <id>";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Onglet Territoire : les zones du front, construites par SRP_FrontScreens (lignes M, H, L, C, K ;
	//! SRP_PCMenu hérite de SRP_ListMenu, qui les comprend toutes)
	protected static string Territoire(int playerId)
	{
		return SRP_FrontScreens.PCTerritoire(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected static string Journal()
	{
		array<string> recent = SRP_JournalComponent.GetRecent();
		string text = string.Format("M|Les %1 dernières lignes, la plus récente en premier", recent.Count());
		for (int i = recent.Count() - 1; i >= 0; i--)
			text += "\nL|" + recent[i];
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Base(bool officer)
	{
		string text = "M|État de la base";
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		text += "\nH|Dépôts";
		if (resources)
		{
			for (int r = 0; r < SRP_Resources.COUNT; r++)
				text += "\nL|" + resources.GetStockText(r);
		}
		else
			text += "\nL|gestionnaire absent";

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		text += "\nH|Coffre";
		if (treasury)
		{
			text += "\nL|" + treasury.GetBalanceText();
			if (officer)
			{
				string pending = treasury.GetPendingText();
				pending.Replace("\n", " · ");
				text += "\nL|Commandes en attente : " + pending;
			}
		}
		else
			text += "\nL|trésorerie absente";

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		text += "\nH|Parc";
		if (fleet)
		{
			array<string> lines = {};
			fleet.GetStatusText().Split("\n", lines, true);
			foreach (string line : lines)
				text += "\nL|" + line;

			text += "\nH|Balises GPS (carré de 100 m, position approximative)";
			array<string> beacons = {};
			fleet.GetBeaconText().Split("\n", beacons, true);
			foreach (string beacon : beacons)
				text += "\nL|" + beacon;
		}
		else
			text += "\nL|flotte absente";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Officiers(SRP_PlayerManagerComponent players)
	{
		string text = "M|Vue officiers";

		text += "\nH|Corps de joueurs déconnectés";
		SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
		if (disconnect)
		{
			array<string> bodies = {};
			disconnect.GetStatusText().Split("\n", bodies, true);
			foreach (string body : bodies)
				text += "\nL|" + body;
		}
		else
			text += "\nL|gestionnaire absent";

		text += "\nH|Promotions en attente (conditions remplies)";
		array<ref SRP_PlayerRecord> all = {};
		SRP_PlayerRecord.LoadAll(all);
		int pending = 0;
		foreach (SRP_PlayerRecord record : all)
		{
			if (record.m_iGrade >= SRP_EGrade.ADJUDANT)
				continue;
			string missing;
			if (players.MeetsGrade(record, record.m_iGrade + 1, missing))
			{
				pending++;
				text += string.Format("\nL|%1 : %2 → %3", record.m_sName, SRP_Grades.GetName(record.m_iGrade), SRP_Grades.GetName(record.m_iGrade + 1));
			}
		}
		if (pending == 0)
			text += "\nL|aucune";

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		text += "\nH|Commandes en attente";
		if (treasury)
		{
			array<string> lines = {};
			treasury.GetPendingText().Split("\n", lines, true);
			foreach (string line : lines)
				text += "\nL|" + line;
		}

		SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
		text += "\nH|Points de livraison";
		if (deliveries)
		{
			array<string> lines = {};
			deliveries.GetStatusText().Split("\n", lines, true);
			foreach (string line : lines)
				text += "\nL|" + line;
		}

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		text += "\nH|Renseignement";
		if (enemies)
		{
			array<string> lines = {};
			enemies.GetStatusText().Split("\n", lines, true);
			foreach (string line : lines)
				text += "\nL|" + line;
		}
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (civils)
		{
			string civilText = civils.GetStatusText();
			civilText.Replace("\n", " · ");
			text += "\nL|" + civilText;
		}
		return text;
	}
}
