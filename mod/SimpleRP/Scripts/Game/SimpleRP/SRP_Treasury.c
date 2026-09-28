//------------------------------------------------------------------------------------------------
// SimpleRP — Trésorerie de la base (euros)
// - Un compte commun, visible et alimenté sur le coffre (actions « Consulter le coffre » et
//   « Déposer l'argent »), sauvegardé dans $profile:SimpleRP/tresorerie.json
// - L'argent n'est jamais un objet d'inventaire (trop fragile dans Reforger) : c'est une valeur
//   portée par le personnage (fiche, persistante). Une SACOCHE est un prop à nous (SRP_Sacoche,
//   composant SRP_MoneyComponent + action « Ramasser l'argent ») posé au sol :
//   - à la mort d'un IA d'une faction ennemie, avec une somme aléatoire ;
//   - jamais à la mort d'un civil. Un civil tué PAR UN JOUEUR est un crime de guerre : infraction sur
//     la fiche, alerte aux officiers, amende pour la base ;
//   - à la mort d'un joueur qui portait de l'argent : sa sacoche tombe à côté de son corps.
// - Achats (officiers, depuis le terminal, où qu'il soit) : véhicules et caisses du fichier
//   SRP_Catalogue.conf, avec leurs prix. Tout est livré au marqueur SRP_Livraison après un délai ;
//   les livraisons de caisses en attente survivent au redémarrage.
// - Subventions : SRP_TreasuryComponent.Add(montant, motif) pour les missions (phase 4),
//   /euros pour le Staff.
// Phase 3 — étape C
//------------------------------------------------------------------------------------------------

//! Une sacoche : composant à poser sur le prefab SRP_Sacoche (prop au sol)
[ComponentEditorProps(category: "SimpleRP", description: "Sacoche d'euros SimpleRP : somme contenue, ramassée par l'action SRP_CashPickupAction")]
class SRP_MoneyComponentClass : ScriptComponentClass
{
}

class SRP_MoneyComponent : ScriptComponent
{
	[Attribute("100", UIWidgets.EditBox, "Somme contenue (le butin la remplace au spawn)", category: "SimpleRP")]
	protected int m_iAmount;

	protected bool m_bCivilian;

	//------------------------------------------------------------------------------------------------
	int GetAmount()
	{
		return m_iAmount;
	}

	//------------------------------------------------------------------------------------------------
	void SetAmount(int amount)
	{
		m_iAmount = amount;
	}

	//------------------------------------------------------------------------------------------------
	bool IsCivilian()
	{
		return m_bCivilian;
	}

	//------------------------------------------------------------------------------------------------
	void SetCivilian(bool civilian)
	{
		m_bCivilian = civilian;
	}

	//------------------------------------------------------------------------------------------------
	// Registre des sacoches posées au sol, pour la remise à zéro
	//------------------------------------------------------------------------------------------------
	protected static ref array<SRP_MoneyComponent> s_aBags = {};

	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer())
			s_aBags.Insert(this);
	}

	override void OnDelete(IEntity owner)
	{
		s_aBags.RemoveItem(this);
		super.OnDelete(owner);
	}

	//! Retire toutes les sacoches au sol. Retourne le nombre.
	static int DeleteAll()
	{
		int count = 0;
		for (int i = s_aBags.Count() - 1; i >= 0; i--)
		{
			SRP_MoneyComponent bag = s_aBags[i];
			if (bag && bag.GetOwner() && !bag.GetOwner().IsDeleted())
			{
				SCR_EntityHelper.DeleteEntityAndChildren(bag.GetOwner());
				count++;
			}
		}
		s_aBags.Clear();
		return count;
	}
}

//------------------------------------------------------------------------------------------------
//! Une livraison de caisses en attente (sauvegardée)
class SRP_PendingDelivery
{
	string m_sName;
	bool m_bResource;			// caisse de paquets d'une ressource
	int m_iResource;
	int m_iPackets;
	ResourceName m_sPrefab;		// sinon : objet quelconque
	int m_iCrates;				// exemplaires de l'objet
	int m_iSecondsLeft;
	string m_sOrderedBy;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Trésorerie SimpleRP : euros de la base, butin, achats, livraisons")]
class SRP_TreasuryComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_TreasuryComponent : SCR_BaseGameModeComponent
{
	[Attribute("5000", UIWidgets.EditBox, "Solde au premier lancement, en euros", category: "SimpleRP - Trésorerie")]
	protected int m_iInitialBalance;

	[Attribute("1", UIWidgets.CheckBox, "Achats réservés aux officiers", category: "SimpleRP - Trésorerie")]
	protected bool m_bOfficersOnly;

	[Attribute("0", UIWidgets.EditBox, "Distance maximale au dépôt de pièces pour acheter, en mètres (0 = aucune limite, le terminal suffit)", category: "SimpleRP - Trésorerie")]
	protected float m_fShopRadius;

	[Attribute("", UIWidgets.ResourceNamePicker, "Fichier catalogue (SRP_Catalogue.conf) : véhicules et caisses en vente, avec leurs prix", "conf", category: "SimpleRP - Catalogue")]
	protected ResourceName m_sCataloguePath;

	[Attribute("300", UIWidgets.EditBox, "Délai de livraison des caisses, en secondes", category: "SimpleRP - Catalogue")]
	protected int m_iDeliveryDelaySeconds;

	[Attribute("50", UIWidgets.EditBox, "Paquets par caisse livrée : une grosse commande arrive en plusieurs caisses", category: "SimpleRP - Catalogue")]
	protected int m_iPacketsPerBox;

	[Attribute("SRP_Livraison", UIWidgets.EditBox, "Marqueur de livraison de secours, utilisé seulement sans SRP_DeliveryComponent", category: "SimpleRP - Catalogue")]
	protected string m_sLivraisonName;

	[Attribute("", UIWidgets.ResourceNamePicker, "Prefab de la sacoche (SRP_Sacoche : prop + SRP_MoneyComponent + action Ramasser l'argent)", "et", category: "SimpleRP - Butin")]
	protected ResourceName m_sMoneyPrefab;

	[Attribute("USSR,FIA", UIWidgets.EditBox, "Clés des factions ennemies, séparées par des virgules", category: "SimpleRP - Butin")]
	protected string m_sEnemyFactions;

	[Attribute("CIV", UIWidgets.EditBox, "Clés des factions civiles, séparées par des virgules", category: "SimpleRP - Butin")]
	protected string m_sCivilianFactions;

	[Attribute("0.5", UIWidgets.EditBox, "Probabilité qu'un ennemi tué porte une liasse (0 à 1)", category: "SimpleRP - Butin")]
	protected float m_fLootChance;

	[Attribute("50", UIWidgets.EditBox, "Somme minimale d'une liasse de butin", category: "SimpleRP - Butin")]
	protected int m_iLootMin;

	[Attribute("300", UIWidgets.EditBox, "Somme maximale d'une liasse de butin", category: "SimpleRP - Butin")]
	protected int m_iLootMax;

	[Attribute("1000", UIWidgets.EditBox, "Amende pour la base quand un joueur tue un civil (crime de guerre, 0 = aucune)", category: "SimpleRP - Butin")]
	protected int m_iWarCrimeFine;

	static const string TREASURY_PATH = "$profile:SimpleRP/tresorerie.json";

	protected static SRP_TreasuryComponent s_Instance;

	protected int m_iBalance;
	protected bool m_bDirty;
	protected ref array<ref SRP_PendingDelivery> m_aDeliveries = {};
	protected ref map<int, int> m_mTerminalIndex = new map<int, int>();		// joueur -> article de ravitaillement en cours de réglage
	protected ref map<int, int> m_mTerminalQty = new map<int, int>();		// joueur -> quantité choisie
	protected ref SRP_Catalogue m_Catalogue;

	//------------------------------------------------------------------------------------------------
	static SRP_TreasuryComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		s_Instance = this;
		SRP_Paths.EnsureDirectories();
		ReloadCatalogue();
		Load();

		GetGame().GetCallqueue().CallLater(Tick, 10000, true);
		GetGame().GetCallqueue().CallLater(SaveIfDirty, 60000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer() && m_bDirty)
			Save();
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Solde
	//------------------------------------------------------------------------------------------------
	int GetBalance()
	{
		return m_iBalance;
	}

	//------------------------------------------------------------------------------------------------
	string GetBalanceText()
	{
		return string.Format("Coffre de la base : %1 €", m_iBalance);
	}

	//------------------------------------------------------------------------------------------------
	void Add(int amount, string reason)
	{
		if (amount <= 0)
			return;

		m_iBalance += amount;
		m_bDirty = true;
		SRP_JournalComponent.Log("TRESORERIE", string.Format("+%1 € (%2) -> %3 €", amount, reason, m_iBalance));
	}

	//------------------------------------------------------------------------------------------------
	//! false si le solde ne suffit pas
	bool Spend(int amount, string reason)
	{
		if (amount < 0)
			return false;
		if (m_iBalance < amount)
			return false;

		m_iBalance -= amount;
		m_bDirty = true;
		SRP_JournalComponent.Log("TRESORERIE", string.Format("-%1 € (%2) -> %3 €", amount, reason, m_iBalance));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : fixe le solde
	void SetBalance(int amount, string reason)
	{
		if (amount < 0)
			amount = 0;
		m_iBalance = amount;
		m_bDirty = true;
		SRP_JournalComponent.Log("TRESORERIE", string.Format("solde fixé (%1) -> %2 €", reason, m_iBalance));
	}

	//------------------------------------------------------------------------------------------------
	//! Commandes en attente, pour le PC
	string GetPendingText()
	{
		if (m_aDeliveries.IsEmpty())
			return "aucune";
		string text = "";
		foreach (SRP_PendingDelivery delivery : m_aDeliveries)
		{
			if (!text.IsEmpty())
				text += "\n";
			text += string.Format("%1 (par %2) dans %3 min", delivery.m_sName, delivery.m_sOrderedBy, Math.Ceil(delivery.m_iSecondsLeft / 60.0));
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : solde initial, livraisons en attente abandonnées
	void ResetAll(string reason)
	{
		m_iBalance = m_iInitialBalance;
		m_aDeliveries.Clear();
		m_bDirty = true;
		Save();
		SRP_JournalComponent.Log("TRESORERIE", string.Format("Remise à zéro (%1) : solde %2 €, livraisons en attente annulées", reason, m_iBalance));
	}

	//------------------------------------------------------------------------------------------------
	// Catalogue (fichier)
	//------------------------------------------------------------------------------------------------
	void ReloadCatalogue()
	{
		m_Catalogue = SRP_Catalogue.Load(m_sCataloguePath);
		if (m_Catalogue)
			SRP_JournalComponent.Log("TRESORERIE", string.Format("Catalogue chargé : %1 véhicule(s), %2 caisse(s)", m_Catalogue.GetVehicleCount(), m_Catalogue.GetCrateCount()));
		else
			Print("[SRP] Aucun catalogue chargé (SRP_TreasuryComponent → Fichier catalogue) : rien à vendre au terminal", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	SRP_Catalogue GetCatalogue()
	{
		return m_Catalogue;
	}

	//------------------------------------------------------------------------------------------------
	string GetCratesText()
	{
		if (!m_Catalogue || m_Catalogue.GetCrateCount() == 0)
			return "Aucune caisse en vente";

		string text = "Ravitaillement :";
		foreach (int index, SRP_CatalogueCrate crate : m_Catalogue.m_aCaisses)
		{
			if (!crate || !crate.IsUsable())
				continue;
			if (crate.m_bResource)
				text += string.Format("\n%1. %2 (/acheter caisse %1 [paquets])", index, crate.Label());
			else
				text += string.Format("\n%1. %2 : %3 €", index, crate.Label(), crate.m_iPrice);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Catalogue envoyé au terminal (une ligne par entrée, champs séparés par |)
	//   M|message    H|titre de section    I|clé|libellé|prix
	//------------------------------------------------------------------------------------------------
	string BuildCatalogue(int playerId)
	{
		return BuildCatalogueWith(playerId, "");
	}

	//------------------------------------------------------------------------------------------------
	protected static string Esc(string text)
	{
		string s = text;
		s.Replace("|", "/");
		s.Replace("\n", " · ");
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Le catalogue, avec un résultat en tête (achat, refus…)
	string BuildCatalogueWith(int playerId, string message)
	{
		string text = string.Format("M|Solde : %1 € — choisis un article, puis Acheter", m_iBalance);
		if (!message.IsEmpty())
			text = string.Format("M|%1 — solde %2 €", Esc(message), m_iBalance);

		SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
		if (fleet)
		{
			array<ref SRP_FleetEntry> entries = fleet.GetCatalogue();
			if (entries && !entries.IsEmpty())
			{
				text += "\nH|Véhicules";
				foreach (int index, SRP_FleetEntry entry : entries)
				{
					if (!entry || entry.m_sPrefab.IsEmpty())
						continue;
					text += string.Format("\nI|V:%1|%2 [%3]|%4", index, entry.m_sName, SRP_VehicleCategories.GetName(entry.m_iCategory), entry.m_iPriceEuros);
				}
			}
		}

		if (m_Catalogue && m_Catalogue.GetCrateCount() > 0)
		{
			text += "\nH|Ravitaillement";
			foreach (int index, SRP_CatalogueCrate crate : m_Catalogue.m_aCaisses)
			{
				if (!crate || !crate.IsUsable())
					continue;
				if (crate.m_bResource)
					text += string.Format("\nI|S:%1|%2|", index, crate.Label());
				else
					text += string.Format("\nI|S:%1|%2|%3", index, crate.Label(), crate.m_iPrice);
			}
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CatalogueCrate CrateAt(int index)
	{
		if (!m_Catalogue || index < 0 || index >= m_Catalogue.GetCrateCount())
			return null;
		SRP_CatalogueCrate crate = m_Catalogue.m_aCaisses[index];
		if (!crate || !crate.IsUsable())
			return null;
		return crate;
	}

	//------------------------------------------------------------------------------------------------
	//! La quantité en cours de réglage pour cet article (celle proposée par défaut si on vient d'arriver)
	protected int CurrentQuantity(int playerId, int index, SRP_CatalogueCrate crate)
	{
		int current;
		int qty;
		if (m_mTerminalIndex.Find(playerId, current) && current == index && m_mTerminalQty.Find(playerId, qty))
			return crate.Clamp(qty);
		return crate.Clamp(crate.m_iPackets);
	}

	//------------------------------------------------------------------------------------------------
	//! La page de quantité d'un article de ravitaillement
	protected string BuildQuantityPage(int playerId, int index, SRP_CatalogueCrate crate, int qty, string message)
	{
		qty = crate.Clamp(qty);
		m_mTerminalIndex.Set(playerId, index);
		m_mTerminalQty.Set(playerId, qty);

		int total = qty * crate.m_iPricePerPacket;
		int step = crate.StepPackets();
		int bigStep = step * 5;
		string name = crate.DisplayName();

		string text = string.Format("M|%1 : %2 paquets × %3 € = %4 € — coffre %5 €", name, qty, crate.m_iPricePerPacket, total, m_iBalance);
		if (!message.IsEmpty())
			text += "\nC|y|" + Esc(message);
		if (total > m_iBalance)
			text += string.Format("\nC|r|Solde insuffisant : il manque %1 €", total - m_iBalance);

		text += string.Format("\nH|Quantité (de %1 à %2, pas de %3)", crate.MinPackets(), crate.MaxPackets(), step);
		text += string.Format("\nI|Q:%1:-%2|- %2 paquets", index, bigStep);
		text += string.Format("\nI|Q:%1:-%2|- %2 paquets", index, step);
		text += string.Format("\nI|Q:%1:+%2|+ %2 paquets", index, step);
		text += string.Format("\nI|Q:%1:+%2|+ %2 paquets", index, bigStep);

		text += "\nH|Raccourcis";
		text += string.Format("\nI|Q:%1:=%2|%2 paquets (minimum)", index, crate.MinPackets());
		int quarter = crate.Clamp(crate.MaxPackets() / 4);
		int half = crate.Clamp(crate.MaxPackets() / 2);
		if (quarter > crate.MinPackets())
			text += string.Format("\nI|Q:%1:=%2|%2 paquets", index, quarter);
		if (half > quarter)
			text += string.Format("\nI|Q:%1:=%2|%2 paquets", index, half);
		if (crate.MaxPackets() > half)
			text += string.Format("\nI|Q:%1:=%2|%2 paquets (maximum)", index, crate.MaxPackets());

		text += "\nH|Commande";
		text += string.Format("\nK|g|P:%1:%2|Commander %2 paquets de %3 pour %4 € (sélectionne, puis Acheter)", index, qty, SRP_Resources.GetName(crate.m_eResource), total);
		text += "\nI|C|« Retour au catalogue";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Terminal : une clé cliquée ou validée dans l'écran. L'écran est toujours renvoyé à jour ; rien pour le chat.
	//!   C          retour au catalogue
	//!   S:<i>      article : ressource -> page de quantité ; objet -> achat
	//!   Q:<i>:+n / -n / =n   réglage de la quantité
	//!   P:<i>:<n>  commande de n paquets
	//!   V:<i>      véhicule
	string Terminal(int playerId, string key)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!pc)
			return "";

		array<string> parts = {};
		key.Split(":", parts, true);
		if (parts.IsEmpty() || parts[0] == "C")
		{
			pc.SRP_OpenTerminal(BuildCatalogueWith(playerId, ""));
			return "";
		}

		string kind = parts[0];
		int index = -1;
		if (parts.Count() >= 2 && SRP_Utils.IsNumeric(parts[1]))
			index = parts[1].ToInt();

		if (kind == "Q" && parts.Count() >= 3)
		{
			SRP_CatalogueCrate crate = CrateAt(index);
			if (!crate || !crate.m_bResource)
			{
				pc.SRP_OpenTerminal(BuildCatalogueWith(playerId, "Article inconnu"));
				return "";
			}
			int qty = CurrentQuantity(playerId, index, crate);
			string op = parts[2];
			if (op.Length() >= 2)
			{
				string sign = op.Get(0);
				string digits = op.Substring(1, op.Length() - 1);
				if (SRP_Utils.IsNumeric(digits))
				{
					int n = digits.ToInt();
					if (sign == "+")
						qty = qty + n;
					else if (sign == "-")
						qty = qty - n;
					else if (sign == "=")
						qty = n;
				}
			}
			pc.SRP_OpenTerminal(BuildQuantityPage(playerId, index, crate, qty, ""));
			return "";
		}

		if (kind == "S")
		{
			SRP_CatalogueCrate crate = CrateAt(index);
			if (crate && crate.m_bResource)
			{
				pc.SRP_OpenTerminal(BuildQuantityPage(playerId, index, crate, CurrentQuantity(playerId, index, crate), ""));
				return "";
			}
			pc.SRP_OpenTerminal(BuildCatalogueWith(playerId, Buy(playerId, key)));
			return "";
		}

		if (kind == "P" && parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
		{
			SRP_CatalogueCrate crate = CrateAt(index);
			int qty = parts[2].ToInt();
			string result = BuyPackets(playerId, index, qty);
			if (crate && crate.m_bResource && m_bLastBuyFailed)
				pc.SRP_OpenTerminal(BuildQuantityPage(playerId, index, crate, qty, result));
			else
				pc.SRP_OpenTerminal(BuildCatalogueWith(playerId, result));
			return "";
		}

		if (kind == "V")
		{
			pc.SRP_OpenTerminal(BuildCatalogueWith(playerId, Buy(playerId, key)));
			return "";
		}

		pc.SRP_OpenTerminal(BuildCatalogueWith(playerId, "Achat inconnu : " + key));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur a-t-il le droit d'acheter ? Vide si oui, sinon la raison.
	protected string CheckBuyer(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent du game mode";
		if (m_bOfficersOnly && !players.IsOfficier(playerId))
			return "Les achats sont réservés aux officiers";
		if (m_fShopRadius > 0)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!SRP_GarageUtils.IsNearDepot(character, SRP_EResource.PIECES, m_fShopRadius))
				return string.Format("Les achats se font à l'atelier (à moins de %1 m)", Math.Round(m_fShopRadius));
		}
		return "";
	}

	protected bool m_bLastBuyFailed;

	//------------------------------------------------------------------------------------------------
	//! Commande de paquets d'une ressource : quantité × prix du paquet, débité au coffre, livrée en caisses
	string BuyPackets(int playerId, int index, int qty)
	{
		m_bLastBuyFailed = true;
		string refusal = CheckBuyer(playerId);
		if (!refusal.IsEmpty())
			return refusal;

		SRP_CatalogueCrate crate = CrateAt(index);
		if (!crate || !crate.m_bResource)
			return "Article inconnu\n" + GetCratesText();

		qty = crate.Clamp(qty);
		int total = qty * crate.m_iPricePerPacket;
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		string what = string.Format("%1 paquets de %2", qty, SRP_Resources.GetName(crate.m_eResource));

		if (!Spend(total, string.Format("achat de %1 par %2", what, playerName)))
			return string.Format("Solde insuffisant : %1 coûtent %2 € — %3", what, total, GetBalanceText());

		SRP_PendingDelivery delivery = new SRP_PendingDelivery();
		delivery.m_sName = what;
		delivery.m_bResource = true;
		delivery.m_iResource = crate.m_eResource;
		delivery.m_iPackets = qty;
		delivery.m_sPrefab = "";
		delivery.m_iCrates = 1;
		delivery.m_iSecondsLeft = m_iDeliveryDelaySeconds;
		delivery.m_sOrderedBy = playerName;
		m_aDeliveries.Insert(delivery);
		m_bDirty = true;
		Save();
		m_mTerminalIndex.Remove(playerId);
		m_mTerminalQty.Remove(playerId);
		m_bLastBuyFailed = false;

		string text = string.Format("%1 commandés par %2 (%3 € à %4 € le paquet), livraison dans %5 min", what, playerName, total, crate.m_iPricePerPacket, Math.Ceil(m_iDeliveryDelaySeconds / 60.0));
		SRP_JournalComponent.Log("TRESORERIE", text);
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers("Logistique : " + text);

		return string.Format("Commande passée : %1 pour %2 €, livraison dans %3 min au point de livraison\n%4", what, total, Math.Ceil(m_iDeliveryDelaySeconds / 60.0), GetBalanceText());
	}

	//------------------------------------------------------------------------------------------------
	// Achats
	//------------------------------------------------------------------------------------------------
	//! key : "V:<index>" (véhicule du catalogue) ou "S:<ressource>:<caisses>". Retourne le message au joueur.
	string Buy(int playerId, string key)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent du game mode";

		if (m_bOfficersOnly && !players.IsOfficier(playerId))
			return "Les achats sont réservés aux officiers";

		if (m_fShopRadius > 0)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!SRP_GarageUtils.IsNearDepot(character, SRP_EResource.PIECES, m_fShopRadius))
				return string.Format("Les achats se font à l'atelier (à moins de %1 m)", Math.Round(m_fShopRadius));
		}

		array<string> parts = {};
		key.Split(":", parts, true);
		if (parts.Count() < 2)
			return "Achat inconnu : " + key;

		string kind = parts[0];
		kind.ToUpper();
		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		if (kind == "V")
		{
			SRP_FleetManagerComponent fleet = SRP_FleetManagerComponent.GetInstance();
			if (!fleet)
				return "Gestionnaire de flotte absent du game mode";
			if (!SRP_Utils.IsNumeric(parts[1]))
				return "Numéro de véhicule invalide";
			return fleet.BuyVehicle(playerId, parts[1].ToInt());
		}

		if (kind == "S")
		{
			if (!m_Catalogue || !SRP_Utils.IsNumeric(parts[1]))
				return "Achat de caisses invalide";

			int index = parts[1].ToInt();
			if (index < 0 || index >= m_Catalogue.GetCrateCount())
				return "Caisse inconnue\n" + GetCratesText();

			SRP_CatalogueCrate crate = m_Catalogue.m_aCaisses[index];
			if (!crate || !crate.IsUsable())
				return "Caisse inconnue";

			// Ressource : quantité × prix du paquet (quantité donnée, sinon celle par défaut)
			if (crate.m_bResource)
			{
				int packets = crate.m_iPackets;
				if (parts.Count() >= 3 && SRP_Utils.IsNumeric(parts[2]))
					packets = parts[2].ToInt();
				return BuyPackets(playerId, index, packets);
			}

			int quantity = crate.m_iQuantity;
			if (quantity < 1)
				quantity = 1;

			string label = crate.Label();

			if (!Spend(crate.m_iPrice, string.Format("achat de %1 par %2", label, playerName)))
				return string.Format("Solde insuffisant : %1 coûte %2 € — %3", label, crate.m_iPrice, GetBalanceText());

			SRP_PendingDelivery delivery = new SRP_PendingDelivery();
			delivery.m_sName = crate.m_sName;
			delivery.m_bResource = crate.m_bResource;
			delivery.m_iResource = crate.m_eResource;
			delivery.m_iPackets = crate.m_iPackets;
			delivery.m_sPrefab = crate.m_sPrefab;
			delivery.m_iCrates = quantity;
			delivery.m_iSecondsLeft = m_iDeliveryDelaySeconds;
			delivery.m_sOrderedBy = playerName;
			m_aDeliveries.Insert(delivery);
			m_bDirty = true;
			Save();

			string text = string.Format("%1 commandé(s) par %2 (%3 €), livraison dans %4 min", label, playerName, crate.m_iPrice, Math.Ceil(m_iDeliveryDelaySeconds / 60.0));
			SRP_JournalComponent.Log("TRESORERIE", text);
			players.NotifyOfficiers("Logistique : " + text);
			return string.Format("Commande passée : %1, livraison dans %2 min au point de livraison\n%3", label, Math.Ceil(m_iDeliveryDelaySeconds / 60.0), GetBalanceText());
		}

		return "Achat inconnu : " + key;
	}

	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		if (m_aDeliveries.IsEmpty())
			return;

		for (int i = m_aDeliveries.Count() - 1; i >= 0; i--)
		{
			SRP_PendingDelivery delivery = m_aDeliveries[i];
			delivery.m_iSecondsLeft -= 10;
			m_bDirty = true;
			if (delivery.m_iSecondsLeft > 0)
				continue;

			if (DeliverCrates(delivery))
				m_aDeliveries.Remove(i);
			else
				delivery.m_iSecondsLeft = 60;	// nouvel essai dans une minute
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool DeliverCrates(SRP_PendingDelivery delivery)
	{
		Resource res;
		if (!delivery.m_bResource)
		{
			res = Resource.Load(delivery.m_sPrefab);
			if (!res || !res.IsValid())
			{
				Print("[SRP] Livraison impossible, prefab invalide : " + delivery.m_sPrefab, LogLevel.ERROR);
				return true;	// on abandonne cette livraison plutôt que de réessayer sans fin
			}
		}

		// Point de livraison : gestionnaire de points (choix hors de vue des joueurs, garnison possible), sinon marqueur unique
		vector mat[4];
		string label = m_sLivraisonName;
		bool hostile = false;
		SRP_DeliveryComponent deliveries = SRP_DeliveryComponent.GetInstance();
		if (deliveries)
		{
			if (!deliveries.ChoosePoint(SRP_EDeliveryKind.CRATES, mat, label, hostile))
			{
				Print("[SRP] Livraison impossible : aucun point de livraison ni marqueur " + m_sLivraisonName, LogLevel.ERROR);
				return false;
			}
		}
		else
		{
			IEntity marker = GetGame().GetWorld().FindEntityByName(m_sLivraisonName);
			if (!marker)
			{
				Print("[SRP] Livraison impossible : marqueur " + m_sLivraisonName + " absent", LogLevel.ERROR);
				return false;
			}
			marker.GetWorldTransform(mat);
		}

		string what;
		if (delivery.m_bResource)
		{
			// Une caisse de livraison pleine de paquets
			SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
			if (!resources)
			{
				Print("[SRP] Livraison impossible : gestionnaire de ressources absent", LogLevel.ERROR);
				return false;
			}
			int perBox = m_iPacketsPerBox;
			if (perBox < 1)
				perBox = 50;
			int remaining = delivery.m_iPackets;
			int boxes = 0;
			int placed = 0;
			while (remaining > 0 && boxes < 20)
			{
				int inThis = Math.Min(perBox, remaining);
				int row = boxes / 5;
				int column = boxes - row * 5;
				vector position = mat[3] + mat[0] * (1.5 + 1.6 * column) + mat[2] * (1.6 * row) + vector.Up * 0.3;
				IEntity box = resources.SpawnBox(delivery.m_iResource, inThis, position);
				if (!box)
				{
					if (boxes == 0)
					{
						Print("[SRP] Livraison impossible : caisse de livraison ou paquet non réglé (SRP_ResourceManagerComponent)", LogLevel.ERROR);
						return false;
					}
					break;
				}
				placed += SRP_ResourceManagerComponent.CountInBox(box, delivery.m_iResource);
				remaining = remaining - inThis;
				boxes++;
			}
			what = string.Format("%1, %2 paquets de %3 en %4 caisse(s) (commande de %5)", delivery.m_sName, placed, SRP_Resources.GetName(delivery.m_iResource), boxes, delivery.m_sOrderedBy);
		}
		else
		{
			int spawned = 0;
			for (int n = 0; n < delivery.m_iCrates; n++)
			{
				// En rangées de cinq à côté du marqueur, légèrement en hauteur
				int row = n / 5;
				int column = n - row * 5;
				EntitySpawnParams params = new EntitySpawnParams();
				params.TransformMode = ETransformMode.WORLD;
				params.Transform[3] = mat[3] + mat[0] * (1.2 * column) + mat[2] * (1.2 * row) + vector.Up * 0.3;
				if (GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params))
					spawned++;
			}
			what = string.Format("%1 x%2 (commande de %3)", delivery.m_sName, spawned, delivery.m_sOrderedBy);
		}
		string text = string.Format("%1 : arrivé au point « %2 »", what, label);
		if (deliveries)
			text = deliveries.ArrivalText(what, label, hostile);
		SRP_JournalComponent.Log("TRESORERIE", text + SRP_Utils.HostileSuffix(hostile));

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers("Logistique : " + text);

		Save();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Butin et argent porté
	//------------------------------------------------------------------------------------------------
	override void OnControllableDestroyed(notnull SCR_InstigatorContextData instigatorContextData)
	{
		super.OnControllableDestroyed(instigatorContextData);

		if (!Replication.IsServer())
			return;

		IEntity victim = instigatorContextData.GetVictimEntity();
		if (!victim)
			return;

		// Un joueur qui meurt avec de l'argent sur lui : sa sacoche tombe à côté du corps
		int victimPlayerId = instigatorContextData.GetVictimPlayerID();
		if (victimPlayerId > 0)
		{
			DropPlayerCash(victimPlayerId, victim);
			return;
		}

		// Un IA d'une faction ennemie ou civile : butin
		FactionAffiliationComponent affiliation = FactionAffiliationComponent.Cast(victim.FindComponent(FactionAffiliationComponent));
		if (!affiliation)
			return;
		Faction faction = affiliation.GetAffiliatedFaction();
		if (!faction)
			return;

		string key = faction.GetFactionKey();

		// Un civil : jamais de sacoche. S'il a été tué par un joueur, c'est un crime de guerre
		// (un civil tué par l'IA ennemie ou par accident sans joueur en cause ne compte pas).
		if (IsFactionListed(m_sCivilianFactions, key))
		{
			int killerPlayerId = instigatorContextData.GetKillerPlayerID();
			if (killerPlayerId > 0 && !SRP_Checkpoint.IsLegitimateTarget(victim))
				RegisterWarCrime(killerPlayerId);	// sauf un suspect révélé d'un contrôle routier (barrage forcé, fuite, arme, cargaison trouvée)
			return;
		}

		if (!IsFactionListed(m_sEnemyFactions, key))
			return;

		if (Math.RandomFloat01() > m_fLootChance)
			return;

		int amount = Math.RandomIntInclusive(m_iLootMin, m_iLootMax);
		if (amount <= 0)
			return;

		SpawnBag(victim, amount, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur a tué un civil : infraction sur sa fiche, alerte aux officiers, amende pour la base
	protected void RegisterWarCrime(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;

		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return;

		players.LogInfraction(playerId, "CRIME DE GUERRE : civil tué");
		players.NotifyOfficiers(string.Format("CRIME DE GUERRE : %1 a tué un civil", record.m_sName));
		SRP_BridgeComponent.PushEvent("crime", string.Format("CRIME DE GUERRE : %1 a tué un civil", record.m_sName));

		int fine = m_iWarCrimeFine;
		if (fine > m_iBalance)
			fine = m_iBalance;
		if (fine > 0)
			Spend(fine, string.Format("amende, crime de guerre de %1", record.m_sName));

		SRP_JournalComponent.Log("TRESORERIE", string.Format("crime de guerre : %1 a tué un civil, amende de %2 €", record.m_sName, fine));
		SRP_Utils.NotifyPlayer(playerId, string.Format("CRIME DE GUERRE : vous avez tué un civil. Infraction enregistrée, amende de %1 € pour la base.", fine));
	}

	//------------------------------------------------------------------------------------------------
	//! Pose une sacoche à côté d'une entité (corps), légèrement décalée pour ne pas être dans le corps
	protected IEntity SpawnBag(IEntity near, int amount, bool civilian)
	{
		if (m_sMoneyPrefab.IsEmpty())
		{
			Print("[SRP] Aucun prefab de sacoche réglé (SRP_TreasuryComponent → Prefab de la sacoche)", LogLevel.WARNING);
			return null;
		}

		Resource res = Resource.Load(m_sMoneyPrefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Prefab de sacoche invalide : " + m_sMoneyPrefab, LogLevel.ERROR);
			return null;
		}

		vector position = near.GetOrigin() + Vector(0.8, 0, 0.4);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.05;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity bag = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
		if (!bag)
			return null;

		SRP_MoneyComponent money = SRP_MoneyComponent.Cast(bag.FindComponent(SRP_MoneyComponent));
		if (money)
		{
			money.SetAmount(amount);
			money.SetCivilian(civilian);
		}
		else
		{
			Print("[SRP] Le prefab de sacoche n'a pas de SRP_MoneyComponent : " + m_sMoneyPrefab, LogLevel.ERROR);
		}
		return bag;
	}

	//------------------------------------------------------------------------------------------------
	protected void DropPlayerCash(int playerId, IEntity body)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;

		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record || record.m_iCash <= 0)
			return;

		int amount = record.m_iCash;
		record.m_iCash = 0;

		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (SpawnBag(body, amount, false))
		{
			SRP_JournalComponent.Log("TRESORERIE", string.Format("%1 est mort avec %2 € sur lui : sacoche tombée à côté du corps", name, amount));
			SRP_Utils.NotifyPlayer(playerId, string.Format("Tu es tombé avec %1 € sur toi : la sacoche est restée à côté de ton corps.", amount));
		}
		else
		{
			// Pas de prefab : l'argent est perdu, mais on le dit
			SRP_JournalComponent.Log("TRESORERIE", string.Format("%1 est mort avec %2 € sur lui, perdus (pas de prefab de sacoche)", name, amount));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsFactionListed(string list, string key)
	{
		if (key.IsEmpty())
			return false;

		array<string> keys = {};
		list.Split(",", keys, true);
		foreach (string candidate : keys)
		{
			string trimmed = candidate.Trim();
			if (trimmed == key)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Ramassage d'une sacoche (action SRP_CashPickupAction, serveur). Retourne le message au joueur.
	string PickUp(int playerId, IEntity bag)
	{
		SRP_MoneyComponent money = SRP_MoneyComponent.Cast(bag.FindComponent(SRP_MoneyComponent));
		if (!money)
			return "Cette sacoche est vide";

		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent du game mode";

		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";

		int amount = money.GetAmount();
		bool civilian = money.IsCivilian();
		SCR_EntityHelper.DeleteEntityAndChildren(bag);

		if (civilian)
		{
			players.LogInfraction(playerId, "CRIME DE GUERRE : argent pris sur un civil");
			players.NotifyOfficiers(string.Format("CRIME DE GUERRE : %1 a pris de l'argent sur un civil", record.m_sName));

			int fine = m_iWarCrimeFine;
			if (fine > m_iBalance)
				fine = m_iBalance;
			if (fine > 0)
				Spend(fine, string.Format("amende, crime de guerre de %1", record.m_sName));

			SRP_JournalComponent.Log("TRESORERIE", string.Format("%1 € d'argent civil confisqués (%2)", amount, record.m_sName));
			return string.Format("CRIME DE GUERRE : dépouiller un civil. Infraction enregistrée, %1 € confisqués, amende de %2 € pour la base.", amount, fine);
		}

		record.m_iCash += amount;
		SRP_JournalComponent.Log("TRESORERIE", string.Format("%1 ramasse %2 € (porte %3 €)", record.m_sName, amount, record.m_iCash));
		return string.Format("+%1 € ramassés. Tu portes %2 € : à déposer au coffre de la base.", amount, record.m_iCash);
	}

	//------------------------------------------------------------------------------------------------
	//! Dépôt au coffre : l'argent porté passe à la base. Retourne le message au joueur.
	string Deposit(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent du game mode";

		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";

		if (record.m_iCash <= 0)
			return "Tu ne portes pas d'argent\n" + GetBalanceText();

		int amount = record.m_iCash;
		record.m_iCash = 0;
		Add(amount, string.Format("dépôt de %1", record.m_sName));
		return string.Format("Déposé : %1 €\n%2", amount, GetBalanceText());
	}

	//------------------------------------------------------------------------------------------------
	//! Ce qu'un joueur porte sur lui
	int GetCash(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return 0;
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return 0;
		return record.m_iCash;
	}

	//------------------------------------------------------------------------------------------------
	// Persistance
	//------------------------------------------------------------------------------------------------
	protected void SaveIfDirty()
	{
		if (m_bDirty)
			Save();
	}

	//------------------------------------------------------------------------------------------------
	void Save()
	{
		if (!Replication.IsServer())
			return;

		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("initialized", true);
		ctx.WriteValue("balance", m_iBalance);
		ctx.WriteValue("savedAt", SRP_Time.Now());
		ctx.WriteValue("deliveries", m_aDeliveries);

		if (ctx.SaveToFile(TREASURY_PATH))
			m_bDirty = false;
		else
			Print("[SRP] Échec de sauvegarde de la trésorerie : " + TREASURY_PATH, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		if (FileIO.FileExists(TREASURY_PATH))
		{
			JsonLoadContext ctx = new JsonLoadContext();
			if (ctx.LoadFromFile(TREASURY_PATH))
			{
				int balance;
				if (ctx.ReadValue("balance", balance))
					m_iBalance = balance;

				array<ref SRP_PendingDelivery> deliveries = {};
				if (ctx.ReadValue("deliveries", deliveries))
				{
					m_aDeliveries.Clear();
					foreach (SRP_PendingDelivery delivery : deliveries)
					{
						if (delivery && (delivery.m_iCrates > 0 || delivery.m_iPackets > 0))
							m_aDeliveries.Insert(delivery);
					}
				}

				SRP_JournalComponent.Log("TRESORERIE", string.Format("Trésorerie chargée : %1 €, %2 livraison(s) en attente", m_iBalance, m_aDeliveries.Count()));
				return;
			}

			Print("[SRP] tresorerie.json illisible, solde initial appliqué", LogLevel.ERROR);
		}

		m_iBalance = m_iInitialBalance;
		m_bDirty = true;
		SRP_JournalComponent.Log("TRESORERIE", string.Format("Premier lancement : solde initial %1 €", m_iBalance));
		Save();
	}
}

//------------------------------------------------------------------------------------------------
//! Coffre : consulter le solde
class SRP_SafeConsultAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
		{
			SRP_Utils.NotifyPlayer(playerId, "Trésorerie absente du game mode");
			return;
		}

		SRP_Utils.HintPlayer(playerId, "Coffre de la base", string.Format("%1\nSur toi : %2 €", treasury.GetBalanceText(), treasury.GetCash(playerId)));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Consulter le coffre";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Coffre : déposer les liasses de l'inventaire
class SRP_SafeDepositAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
		{
			SRP_Utils.NotifyPlayer(playerId, "Trésorerie absente du game mode");
			return;
		}

		SRP_Utils.NotifyPlayer(playerId, treasury.Deposit(playerId));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Déposer l'argent porté";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Sacoche au sol : ramasser l'argent
class SRP_CashPickupAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_TreasuryComponent treasury = SRP_TreasuryComponent.GetInstance();
		if (!treasury)
		{
			SRP_Utils.NotifyPlayer(playerId, "Trésorerie absente du game mode");
			return;
		}

		SRP_Utils.NotifyPlayer(playerId, treasury.PickUp(playerId, pOwnerEntity));
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Ramasser l'argent";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool HasLocalEffectOnlyScript()
	{
		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Ordinateur de l'atelier : ouvre le terminal d'achat (le client demande le catalogue au serveur)
class SRP_TerminalAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		// Seule la machine du joueur qui utilise l'ordinateur ouvre l'écran
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc || pc.GetControlledEntity() != pUserEntity)
			return;

		pc.SRP_RequestTerminal();
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Utiliser le terminal logistique";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! true : l'action ne fait rien côté serveur, tout part d'une requête du client
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}
}
