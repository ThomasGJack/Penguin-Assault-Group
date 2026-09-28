//------------------------------------------------------------------------------------------------
// SimpleRP — Menus à liste (écran vanilla DialogUI + layout hérité de ConfigurableDialog)
// SRP_ListMenu : la mécanique commune. Le serveur envoie un texte (une ligne par entrée :
//   M|message   H|titre de section   L|ligne simple   I|clé|libellé[|prix]), le client affiche, une ligne se
//   sélectionne au clic (fond orange), le bouton Confirmer envoie la clé au serveur par une commande.
//   À la manette (consoles) : le bouton Confirmer répond à A (action vanilla DialogConfirm) et il n'y a pas de
//   clic. La ligne qui a le focus (croix directionnelle) devient donc la ligne choisie, sans rien déclencher,
//   et A la valide. Le focus est posé à l'ouverture et retrouvé après chaque rafraîchissement du serveur.
// SRP_TerminalMenu : terminal logistique (commande « acheter »)
// SRP_AdminMenu   : menu Staff, désormais dans SRP_Admin.c (écran à onglets)
// Enregistrement : Configs/System/chimeraMenus.conf + UI/Layouts/SimpleRP.
// Phase 3 — étape C
//------------------------------------------------------------------------------------------------

modded enum ChimeraMenuPreset
{
	SRP_TerminalMenu,
	SRP_AdminMenu,
	SRP_SoldierMenu,
	SRP_PCMenu,
	SRP_CharteMenu,
	SRP_RequestMenu,
	SRP_ControlMenu
};

//! Une ligne de la liste
class SRP_TerminalEntry
{
	string m_sKey;
	string m_sLabel;
	bool m_bSelectable;
	int m_iTint;				// 0 blanc, 1 vert, 2 rouge, 3 orange, 4 gris
	Widget m_wRoot;
	TextWidget m_wLabel;
}

//------------------------------------------------------------------------------------------------
//! Clic sur une ligne : prévient le menu
class SRP_TerminalEntryHandler : ScriptedWidgetEventHandler
{
	protected SRP_ListMenu m_Menu;
	protected int m_iIndex;

	//------------------------------------------------------------------------------------------------
	void SRP_TerminalEntryHandler(SRP_ListMenu menu, int index)
	{
		m_Menu = menu;
		m_iIndex = index;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Menu)
			m_Menu.Select(m_iIndex);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Manette : la ligne qui reçoit le focus devient la ligne choisie, sans rien envoyer
	override bool OnFocus(Widget w, int x, int y)
	{
		if (m_Menu)
			m_Menu.OnEntryFocused(m_iIndex);
		return false;
	}
}

//------------------------------------------------------------------------------------------------
class SRP_ListMenu : DialogUI
{
	static const ResourceName ENTRY_LAYOUT = "{6A5B7C8D9E0F1A2C}UI/Layouts/SimpleRP/SRP_TerminalEntry.layout";

	protected Widget m_wList;
	protected ref array<ref SRP_TerminalEntry> m_aEntries = {};
	protected ref array<ref SRP_TerminalEntryHandler> m_aHandlers = {};
	protected int m_iSelected = -1;
	protected string m_sWantedKey;		// ligne à retrouver après un rafraîchissement du serveur (manette)

	//------------------------------------------------------------------------------------------------
	string GetSelectedKey()
	{
		if (m_iSelected < 0 || m_iSelected >= m_aEntries.Count())
			return "";
		return m_aEntries[m_iSelected].m_sKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur se sert-il d'une manette (console, ou manette branchée sur PC) ?
	static bool SRP_IsGamepad()
	{
		InputManager input = GetGame().GetInputManager();
		return input && input.GetLastUsedInputDevice() == EInputDeviceType.GAMEPAD;
	}

	//------------------------------------------------------------------------------------------------
	//! Consigne à afficher quand rien n'est choisi, selon la manette ou la souris
	protected string ChooseHint()
	{
		if (SRP_IsGamepad())
			return "Choisis une ligne avec la croix directionnelle, puis " + GetConfirmLabel() + " (A ou croix)";
		return "Clique sur une ligne, puis " + GetConfirmLabel();
	}

	//------------------------------------------------------------------------------------------------
	//! Titre, texte du bouton de confirmation et commande envoyée au serveur : à définir par le menu
	protected string GetMenuTitle() { return "SimpleRP"; }
	protected string GetConfirmLabel() { return "Valider"; }
	protected string GetCommand() { return ""; }

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		m_wList = GetRootWidget().FindAnyWidget("List");

		SetTitle(GetMenuTitle());
		SetConfirmText(GetConfirmLabel());
		SetCancelText("Fermer");
		SetMessage("Chargement…");
		GetGame().OnInputDeviceIsGamepadInvoker().Insert(SRP_OnDeviceChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! Passage à la manette, menu ouvert : redonner le focus à la ligne choisie, sinon à la première
	protected void SRP_OnDeviceChanged(bool isGamepad)
	{
		if (!isGamepad)
			return;
		if (m_iSelected >= 0 && m_iSelected < m_aEntries.Count() && m_aEntries[m_iSelected].m_wRoot)
		{
			GetGame().GetWorkspace().SetFocusedWidget(m_aEntries[m_iSelected].m_wRoot);
			return;
		}
		GetGame().GetCallqueue().Remove(FocusEntry);
		GetGame().GetCallqueue().CallLater(FocusEntry, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Contenu : une ligne par entrée — M|message  H|section  I|clé|libellé[|prix]
	void Fill(string content)
	{
		ClearEntries();

		array<string> lines = {};
		content.Split("\n", lines, true);

		foreach (string line : lines)
		{
			array<string> fields = {};
			line.Split("|", fields, false);
			if (fields.Count() < 2)
				continue;

			string type = fields[0];
			if (type == "M")
			{
				SetMessage(fields[1]);
				continue;
			}

			if (type == "H")
			{
				AddEntry("", "— " + fields[1] + " —", false);
				continue;
			}

			if (type == "L")
			{
				AddEntry("", fields[1], false);
				continue;
			}

			if (type == "C" && fields.Count() >= 3)
			{
				AddEntryTint("", fields[2], false, TintOf(fields[1]));
				continue;
			}

			if (type == "K" && fields.Count() >= 4)
			{
				AddEntryTint(fields[2], fields[3], true, TintOf(fields[1]));
				continue;
			}

			if (type == "I" && fields.Count() >= 3)
			{
				string label = fields[2];
				if (fields.Count() >= 4 && !fields[3].IsEmpty())
					label = string.Format("%1 — %2 €", fields[2], fields[3]);
				AddEntry(fields[1], label, true);
			}
		}

		if (!m_wList)
			SetMessage("Liste indisponible : le layout ne contient pas de widget « List »");
		else if (m_aEntries.IsEmpty())
			SetMessage("Rien à afficher");

		// Manette : focus sur la ligne d'avant le rafraîchissement, sinon sur la première ligne à choisir
		if (SRP_IsGamepad())
		{
			GetGame().GetCallqueue().Remove(FocusEntry);
			GetGame().GetCallqueue().CallLater(FocusEntry, 0, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Donne le focus à la ligne voulue (même clé qu'avant le rafraîchissement), sinon à la première qu'on peut choisir
	protected void FocusEntry()
	{
		if (GetGame().GetMenuManager().GetTopMenu() != this)
			return;	// un autre écran (la charte) est par-dessus
		int target = -1;
		foreach (int i, SRP_TerminalEntry entry : m_aEntries)
		{
			if (!entry.m_bSelectable || !entry.m_wRoot)
				continue;
			if (target < 0)
				target = i;
			if (!m_sWantedKey.IsEmpty() && entry.m_sKey == m_sWantedKey)
			{
				target = i;
				break;
			}
		}
		if (target < 0)
			return;
		GetGame().GetWorkspace().SetFocusedWidget(m_aEntries[target].m_wRoot);
		Highlight(target, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		GetGame().GetCallqueue().Remove(FocusEntry);
		GetGame().OnInputDeviceIsGamepadInvoker().Remove(SRP_OnDeviceChanged);
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearEntries()
	{
		foreach (SRP_TerminalEntry entry : m_aEntries)
		{
			if (entry.m_wRoot)
				entry.m_wRoot.RemoveFromHierarchy();
		}
		m_sWantedKey = GetSelectedKey();
		m_aEntries.Clear();
		m_aHandlers.Clear();
		m_iSelected = -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEntry(string key, string label, bool selectable)
	{
		AddEntryTint(key, label, selectable, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Teinte d'une ligne C| ou K| : w blanc, g vert, r rouge, y orange, d gris
	protected static int TintOf(string code)
	{
		if (code == "g") return 1;
		if (code == "r") return 2;
		if (code == "y") return 3;
		if (code == "d") return 4;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Couleur du texte d'une ligne selon sa teinte
	protected static void PaintLabel(TextWidget label, int tint)
	{
		if (tint == 1)
			label.SetColor(Color.FromRGBA(120, 220, 120, 255));
		else if (tint == 2)
			label.SetColor(Color.FromRGBA(240, 110, 100, 255));
		else if (tint == 3)
			label.SetColor(Color.FromRGBA(245, 185, 70, 255));
		else if (tint == 4)
			label.SetColor(Color.FromRGBA(170, 170, 170, 255));
		else
			label.SetColor(Color.FromRGBA(255, 255, 255, 255));
	}

	//------------------------------------------------------------------------------------------------
	protected void AddEntryTint(string key, string label, bool selectable, int tint)
	{
		if (!m_wList)
			return;

		Widget root = GetGame().GetWorkspace().CreateWidgets(ENTRY_LAYOUT, m_wList);
		if (!root)
		{
			Print("[SRP] Gabarit d'entrée de menu introuvable : " + ENTRY_LAYOUT, LogLevel.ERROR);
			return;
		}

		LayoutSlot.SetHorizontalAlign(root, 3);	// toute la largeur
		if (!selectable)
			root.SetFlags(WidgetFlags.NOFOCUS);	// la croix directionnelle saute les titres et les lignes d'information

		SRP_TerminalEntry entry = new SRP_TerminalEntry();
		entry.m_sKey = key;
		entry.m_sLabel = label;
		entry.m_bSelectable = selectable;
		entry.m_iTint = tint;
		entry.m_wRoot = root;
		entry.m_wLabel = TextWidget.Cast(root.FindAnyWidget("Label"));
		if (entry.m_wLabel)
		{
			entry.m_wLabel.SetText(label);
			entry.m_wLabel.SetFlags(WidgetFlags.IGNORE_CURSOR);
		}

		int index = m_aEntries.Count();
		m_aEntries.Insert(entry);

		if (selectable)
		{
			SRP_TerminalEntryHandler handler = new SRP_TerminalEntryHandler(this, index);
			m_aHandlers.Insert(handler);
			root.AddHandler(handler);
		}

		Paint(entry, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Fond du bouton : sombre au repos, orange sélectionné, transparent pour un titre de section
	protected void Paint(SRP_TerminalEntry entry, bool selected)
	{
		if (entry.m_wRoot)
		{
			if (!entry.m_bSelectable)
				entry.m_wRoot.SetColor(Color.FromRGBA(0, 0, 0, 0));
			else if (selected)
				entry.m_wRoot.SetColor(Color.FromRGBA(210, 130, 30, 220));
			else
				entry.m_wRoot.SetColor(Color.FromRGBA(35, 40, 45, 200));
		}

		if (!entry.m_wLabel)
			return;

		if (!entry.m_bSelectable && entry.m_iTint == 0)
			entry.m_wLabel.SetColor(Color.FromRGBA(170, 170, 170, 255));
		else
			PaintLabel(entry.m_wLabel, entry.m_iTint);
	}

	//------------------------------------------------------------------------------------------------
	//! Choisit une ligne (fond orange) sans rien envoyer ; announce : réécrire le message du haut
	protected void Highlight(int index, bool announce)
	{
		if (index < 0 || index >= m_aEntries.Count() || !m_aEntries[index].m_bSelectable)
			return;

		if (m_iSelected >= 0 && m_iSelected < m_aEntries.Count())
			Paint(m_aEntries[m_iSelected], false);

		m_iSelected = index;
		Paint(m_aEntries[index], true);
		if (announce)
			SetMessage("Sélection : " + m_aEntries[index].m_sLabel + "\n" + GetConfirmLabel() + " pour confirmer.");
	}

	//------------------------------------------------------------------------------------------------
	//! Clic sur une ligne (souris) ; certains menus y exécutent aussi l'action
	void Select(int index)
	{
		Highlight(index, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Manette : la ligne qui a le focus est la ligne choisie ; le message du serveur reste affiché
	void OnEntryFocused(int index)
	{
		if (SRP_IsGamepad())
			Highlight(index, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Bouton de confirmation : envoie la clé choisie au serveur, qui vérifie, agit et répond
	override protected void OnConfirm()
	{
		if (m_iSelected < 0 || m_iSelected >= m_aEntries.Count())
		{
			SetMessage(ChooseHint());
			return;
		}

		SRP_TerminalEntry entry = m_aEntries[m_iSelected];
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc && !GetCommand().IsEmpty())
			pc.SRP_SendMenuCommand(GetCommand(), entry.m_sKey);

		CloseAnimated();
	}

	//------------------------------------------------------------------------------------------------
	protected static SRP_ListMenu OpenPreset(ChimeraMenuPreset preset, string content)
	{
		SRP_ListMenu menu = SRP_ListMenu.Cast(GetGame().GetMenuManager().OpenDialog(preset));
		if (!menu)
		{
			Print("[SRP] Impossible d'ouvrir le menu : preset absent de chimeraMenus.conf ?", LogLevel.ERROR);
			return null;
		}
		menu.Fill(content);
		return menu;
	}
}

//------------------------------------------------------------------------------------------------
//! Terminal logistique : achats en euros
class SRP_TerminalMenu : SRP_ListMenu
{
	protected static SRP_TerminalMenu s_OpenTerminal;
	protected string m_sArmedKey;	// manette : véhicule à valider une seconde fois

	override protected string GetMenuTitle() { return "Terminal logistique"; }
	override protected string GetConfirmLabel() { return "Acheter"; }
	override protected string GetCommand() { return "terminal"; }

	//------------------------------------------------------------------------------------------------
	//! Ouvre l'écran s'il ne l'est pas, puis y verse le contenu (le serveur renvoie l'écran après chaque action)
	static void Open(string catalogue)
	{
		if (!s_OpenTerminal)
		{
			s_OpenTerminal = SRP_TerminalMenu.Cast(GetGame().GetMenuManager().OpenDialog(ChimeraMenuPreset.SRP_TerminalMenu));
			if (!s_OpenTerminal)
			{
				Print("[SRP] Impossible d'ouvrir le terminal : preset SRP_TerminalMenu absent de chimeraMenus.conf ?", LogLevel.ERROR);
				return;
			}
		}
		s_OpenTerminal.Fill(catalogue);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		if (s_OpenTerminal == this)
			s_OpenTerminal = null;
		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	//! Les réglages de quantité et le retour s'exécutent au clic ; un achat demande le bouton Acheter
	override void Select(int index)
	{
		super.Select(index);
		string key = GetSelectedKey();
		if (key.StartsWith("Q:") || key == "C")
			SendKey(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Acheter : la ligne sélectionnée part au serveur, l'écran reste ouvert et se met à jour
	override protected void OnConfirm()
	{
		string key = GetSelectedKey();
		if (key.IsEmpty())
		{
			SetMessage(ChooseHint());
			return;
		}
		// Manette : le focus peut arriver d'office sur un véhicule ; un achat de véhicule se valide deux fois
		if (SRP_IsGamepad() && key.StartsWith("V:") && key != m_sArmedKey)
		{
			m_sArmedKey = key;
			SetMessage("Achat d'un véhicule : appuie encore sur A (croix) pour confirmer");
			return;
		}
		m_sArmedKey = "";
		SendKey(key);
	}

	//------------------------------------------------------------------------------------------------
	//! Changer de ligne annule une confirmation d'achat en attente
	override void OnEntryFocused(int index)
	{
		m_sArmedKey = "";
		super.OnEntryFocused(index);
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
