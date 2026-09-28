//------------------------------------------------------------------------------------------------
// SimpleRP — Compatibilité Bacon Loadout Editor
// - Le dialogue « Obligatory warning » est contrôlé par le drapeau statique public
//   Bacon_GunBuilderUI.m_bIntroDialogConfirmed : on le met à vrai à l'ouverture du menu, avant Init().
// - Le filigrane « Before reporting bugs... » vient du layout : on cherche les textes qui le contiennent
//   dans l'arbre des widgets du menu et on les masque (deux passes, le layout se remplit en différé).
// - Le panneau de debug « Preview UI Component » n'existe qu'en Workbench (#ifdef WORKBENCH dans une
//   classe sealed) : rien à faire pour le jeu.
// - Diagnostic : 2 s après l'ouverture de l'éditeur, l'état de l'arsenal tel que le voit CE client est
//   écrit dans console.log (lignes « [SRP] Diag arsenal »). Sert à comprendre un arsenal vide en dédié.
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

modded class ChimeraMenuBase
{
	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		if (!Bacon_GunBuilderUI.Cast(this))
			return;

		Bacon_GunBuilderUI.m_bIntroDialogConfirmed = true;

		GetGame().GetCallqueue().CallLater(SRP_HideBaconWatermark, 250, false);
		GetGame().GetCallqueue().CallLater(SRP_HideBaconWatermark, 1500, false);
		GetGame().GetCallqueue().CallLater(SRP_DiagArsenal, 2000, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_HideBaconWatermark()
	{
		Widget root = GetRootWidget();
		if (!root)
			return;

		SRP_BaconCompat.HideTextsContaining(root, "other mods");
		SRP_BaconCompat.HideTextsContaining(root, "reporting bugs");
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_DiagArsenal()
	{
		SRP_BaconCompat.Diagnose(GetRootWidget());
	}
}

//------------------------------------------------------------------------------------------------
class SRP_BaconCompat
{
	protected static ref array<IEntity> s_aArsenalQuery = {};

	//------------------------------------------------------------------------------------------------
	//! Masque tout widget texte de l'arbre dont le contenu contient needle (comparaison en minuscules)
	static void HideTextsContaining(Widget widget, string needle)
	{
		if (!widget)
			return;

		TextWidget text = TextWidget.Cast(widget);
		if (text)
		{
			string content = text.GetText();
			content.ToLower();
			if (content.Contains(needle))
				widget.SetVisible(false);
		}

		Widget child = widget.GetChildren();
		while (child)
		{
			HideTextsContaining(child, needle);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de widgets enfants directs
	static int CountChildren(Widget widget)
	{
		if (!widget)
			return -1;

		int count = 0;
		Widget child = widget.GetChildren();
		while (child)
		{
			count++;
			child = child.GetSibling();
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool QueryAddArsenal(IEntity e)
	{
		if (e && e.FindComponent(SCR_ArsenalComponent))
			s_aArsenalQuery.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static void Log(string message)
	{
		Print("[SRP] Diag arsenal : " + message, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected static string OuiNon(bool value)
	{
		if (value)
			return "oui";
		return "non";
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit dans le log l'état de l'arsenal vu par ce client : catalogues, faction FR, caisses proches, menu
	static void Diagnose(Widget root)
	{
		string side = "client";
		if (Replication.IsServer())
			side = "serveur ou partie locale";
		Log("début (" + side + ")");

		// 1. Gestionnaire de catalogues et gestionnaire d'arsenal
		SCR_EntityCatalogManagerComponent catalogManager = SCR_EntityCatalogManagerComponent.GetInstance();
		if (catalogManager)
			Log("gestionnaire de catalogues : présent");
		else
			Log("gestionnaire de catalogues : ABSENT (GetInstance() nul)");

		SCR_ArsenalManagerComponent arsenalManager;
		if (SCR_ArsenalManagerComponent.GetArsenalManager(arsenalManager))
		{
			int gameModeType = SCR_ArsenalManagerComponent.GetArsenalGameModeType_Static();
			bool staticEnabled = SCR_ArsenalManagerComponent.IsArsenalTypeEnabled_Static(SCR_EArsenalTypes.STATIC_ENTITIES);
			Log("gestionnaire d'arsenal : présent, type de mode de jeu " + gameModeType.ToString() + " (-1 = sans restriction), caisses fixes autorisées : " + OuiNon(staticEnabled));
		}
		else
			Log("gestionnaire d'arsenal : ABSENT");

		// 2. Faction FR et son catalogue d'objets
		FactionManager factionManager = GetGame().GetFactionManager();
		SCR_Faction fr;
		if (factionManager)
			fr = SCR_Faction.Cast(factionManager.GetFactionByKey("FR"));
		if (!fr)
			Log("faction FR : INTROUVABLE dans le gestionnaire de factions");
		else
		{
			SCR_EntityCatalog catalog = fr.GetFactionEntityCatalogOfType(EEntityCatalogType.ITEM, false);
			if (!catalog)
				Log("faction FR : présente, mais AUCUN catalogue d'objets (ITEM) initialisé");
			else
			{
				array<SCR_EntityCatalogEntry> entries = {};
				int withArsenalData = catalog.GetEntityListWithData(SCR_ArsenalItem, entries);
				array<SCR_EntityCatalogEntry> all = {};
				int total = catalog.GetEntityList(all);
				Log("faction FR : catalogue d'objets " + total.ToString() + " entrée(s), dont " + withArsenalData.ToString() + " avec données d'arsenal");
			}
		}

		// 3. Le joueur et ses stockages
		IEntity me = SCR_PlayerController.GetLocalControlledEntity();
		if (!me)
			Log("personnage local : AUCUN");
		else
		{
			array<Managed> storages = {};
			int storageCount = me.FindComponents(BaseInventoryStorageComponent, storages);
			Log("personnage local : " + storageCount.ToString() + " stockage(s) d'inventaire");

			// 4. Les caisses d'arsenal à moins de 8 m
			s_aArsenalQuery.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(me.GetOrigin(), 8, QueryAddArsenal, null, EQueryEntitiesFlags.ALL);
			Log(s_aArsenalQuery.Count().ToString() + " caisse(s) d'arsenal à moins de 8 m");

			foreach (IEntity box : s_aArsenalQuery)
			{
				SCR_ArsenalComponent arsenal = SCR_ArsenalComponent.Cast(box.FindComponent(SCR_ArsenalComponent));
				if (!arsenal)
					continue;

				string factionKey = "aucune";
				SCR_Faction assigned = arsenal.GetAssignedFaction();
				if (assigned)
					factionKey = assigned.GetFactionKey();

				int types = arsenal.GetSupportedArsenalItemTypes();
				int modes = arsenal.GetSupportedArsenalItemModes();
				Log("caisse " + SRP_Utils.PrefabShortName(box.GetPrefabData().GetPrefabName()) + " : activée " + OuiNon(arsenal.IsArsenalEnabled()) + ", faction " + factionKey + ", types " + types.ToString() + ", modes " + modes.ToString());

				array<SCR_ArsenalItem> items = {};
				bool ok = arsenal.GetFilteredArsenalItems(items);
				int itemCount = items.Count();
				Log("caisse : GetFilteredArsenalItems = " + OuiNon(ok) + ", " + itemCount.ToString() + " objet(s)");
			}
		}

		// 5. Le menu Bacon lui-même
		if (!root)
		{
			Log("menu : widget racine ABSENT");
			return;
		}

		Widget slots = root.FindAnyWidget("ListBoxSlotChoices");
		Widget panel = root.FindAnyWidget("InventoryPanelMain");
		Widget categories = root.FindAnyWidget("CategorySelector");
		Log("menu : ListBoxSlotChoices " + CountChildren(slots).ToString() + " enfant(s), InventoryPanelMain " + CountChildren(panel).ToString() + " enfant(s), CategorySelector " + CountChildren(categories).ToString() + " enfant(s) (-1 = widget introuvable)");
		Log("fin");
	}
}
