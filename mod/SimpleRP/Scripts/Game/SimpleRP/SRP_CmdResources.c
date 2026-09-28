//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : LES STOCKS.
//
// RÔLE (serveur ; tenu par SRP_Commander.m_Resources, accès SRP_CmdResources.Get()) :
// - arbitrage C1 : SEULS stocks = obus de mortier (par région, RE2), blindés, sorties d'hélico, tirs d'artillerie
//   lourde, bombe (moyens de l'île). Soldats et camions sont GRATUITS : aucun appel de stock pour l'infanterie ;
// - pleins (RE3, RE10 : + res_bonus_menace_centiemes par point de menace), revenu selon les zones rouges tenues
//   avec un plancher (RE4), vide -> plein en res_heures_plein (RE5), en temps réel même serveur vide (RE6),
//   dépôt saboté = revenu de la région coupé de moitié pendant 24 h (RE8), renflouement d'une région à sec (RE2) ;
// - dépense par TICKETS : Reserve (retrait immédiat), puis Consume (effet réel) ou Release (annulation, retour) ;
//   c'est la SEULE restitution au redémarrage (RE13 : les tickets ouverts sont rendus à la relecture, trou 4) ;
// - quart de stock retiré par une mission SABOTAGE ou CACHE réussie ; CA7 (SEULE implémentation : niveau des stocks
//   noté au départ du dernier joueur) ; GetFillRatio (humeur CO7, CA1).
// Ordre des paramètres TOUJOURS (kind, region) (trou 4) ; region = -1 pour les moyens de l'île (pour les OBUS,
// region = -1 rend la somme des régions : lecture seulement).
// Le revenu est CONTINU : Advance l'ajoute de m_iUpdatedAt à maintenant, par tranches coupées aux fins de coupure ;
// les stocks et m_iUpdatedAt sont toujours cohérents, si bien qu'un redémarrage rattrape le temps serveur éteint.
// Aucune IA posée ici (le dépôt et ses gardes : SRP_CmdDepot.c, tenu par m_Depots).
// Sauvegarde : clés cmd_res_* de front.json (WriteTo / ReadFrom, chaîne du Commandeur).
// APPELÉ PAR : SRP_Commander (vie, humeur, missions, menace), SRP_CmdSupport et SRP_CmdArmor (Reserve, Consume,
// Release, GetStock), SRP_CmdManeuvers (GetFillRatio CA1, NightChance CA7), SRP_CmdScreens (Staff), SRP_FrontMarkers.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! La part d'une région (RE2) : obus et coupure du dépôt. Clés cmd_res_rN_* retrouvées par le code de région.
class SRP_CmdRegionSupply
{
	int m_iRegion = -1;
	string m_sCode;					// « R3 »
	int m_iRefZones;				// zones de départ (tracé d'origine)
	int m_iShellShare;				// part entière des res_obus_plein à menace 0
	float m_fShells;				// sauvé
	int m_iCutUntil;				// heure Unix, 0 = pas de coupure (RE8, sauvé)
	int m_iRedCache;				// zones rouges au dernier comptage
}

//------------------------------------------------------------------------------------------------
//! Un engagement de moyens (RE13). Sauvé ; relu = rendu au stock (Release « redémarrage »).
class SRP_CmdTicket
{
	int m_iId;
	int m_iKind = SRP_ECmdStock.OBUS;
	int m_iRegion = -1;
	int m_iLeft;					// unités encore engagées
	string m_sWhat;					// « mortier sur Régina (S07) »
	int m_iOpenedAt;				// heure Unix
	bool m_bFree;					// décision forcée par le Staff (cmd_staff_gratuit) : rien n'a été retiré
}

//------------------------------------------------------------------------------------------------
//! Un renflouement d'obus en route (RE2). Sauvé ; relu = rendu au donneur.
class SRP_CmdTransfer
{
	int m_iFrom = -1;
	int m_iTo = -1;
	int m_iAmount;
	int m_iArriveAt;				// heure Unix
}

//------------------------------------------------------------------------------------------------
//! Les stocks du Commandeur (serveur).
class SRP_CmdResources
{
	// --- Réglages (clés res_* sauf res_depot_*) ----------------------------------------------------------------
	int m_iShellsFull = 48;		// res_obus_plein
	int m_iArmorFull = 2;		// res_blindes_plein
	int m_iHeliFull = 3;		// res_helico_plein
	int m_iArtilleryFull = 1;		// res_artillerie_plein
	int m_iBombFull = 1;		// res_bombe_plein
	int m_iThreatBonusCent = 5;		// res_bonus_menace_centiemes
	bool m_bBombThreatBonus = false;		// res_bombe_bonus_menace
	float m_fRefillHours = 4.0;		// res_heures_plein
	int m_iFloorCent = 50;		// res_plancher_centiemes
	int m_iRefZonesOverride = 0;		// res_zones_depart
	int m_iDepotCutHours = 24;		// res_depot_coupure_heures
	int m_iDepotCutCent = 50;		// res_depot_coupure_centiemes
	int m_iResupplyMinutes = 60;		// res_renflouement_minutes
	int m_iResupplyThreshold = 4;		// res_renflouement_seuil_obus
	int m_iResupplyCent = 50;		// res_renflouement_centiemes
	int m_iDonorKeepCent = 50;		// res_renflouement_donneur_centiemes
	int m_iSabotageCent = 25;		// res_sabotage_centiemes
	int m_iOffensiveBonus = 10;		// res_offensive_bonus
	int m_iOffensiveFullCent = 95;		// res_offensive_plein_centiemes
	int m_iOffensiveLowCent = 33;		// res_offensive_bas_centiemes
	int m_iStateWeakCent = 34;		// res_etat_affaiblie_centiemes
	int m_iStateStrongCent = 90;		// res_etat_renforcee_centiemes

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected SRP_Commander m_Commander;
	protected ref SRP_CmdDepots m_Depots;								// dépôts cachés (SRP_CmdDepot.c)
	protected ref array<ref SRP_CmdRegionSupply> m_aRegions = {};
	protected float m_fArmor;											// stocks de l'île (sauvés)
	protected float m_fHeli;
	protected float m_fArtillery;
	protected float m_fBomb;
	protected ref array<ref SRP_CmdTicket> m_aTickets = {};
	protected ref array<ref SRP_CmdTransfer> m_aTransfers = {};
	protected int m_iNextTicket = 1;
	protected int m_iUpdatedAt;										// dernière avance du revenu (heure Unix, sauvée)
	protected float m_fLevelAtLastLeave = -1;							// CA7 : niveau noté au départ du dernier joueur (sauvé)
	protected int m_iLastLeaveAt;										// sauvé
	protected int m_iLastPlayerCount;
	protected int m_iRefTotal;											// somme des zones de départ (66)
	protected bool m_bStateRead;										// état défini : front.json relu, ou pleins posés (Start, remise)
	protected bool m_bStarted;											// Start fait
	protected bool m_bChanged;											// un fait à sauver pendant ce passage (Tick)

	protected static const int SAVE_FORMAT = 1;						// cmd_res_v
	protected static const int SLICES_MAX = 32;						// tranches d'une avance du revenu, au plus

	//================================================================================================
	// Vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Constructeur seul : m_Depots = new SRP_CmdDepots(this) — SRP_Commander (constructeur)
	void SRP_CmdResources(SRP_Commander commander)
	{
		m_Commander = commander;
		m_Depots = new SRP_CmdDepots(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Les stocks du Commandeur, null s'il est absent — tous les moyens, écrans
	static SRP_CmdResources Get()
	{
		SRP_Commander commander = SRP_Commander.Get();
		if (!commander)
			return null;
		return commander.GetResources();
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés puis m_Depots.DeclareSettings — SRP_Commander.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("res_obus_plein", m_iShellsFull, "Obus de mortier pour toute l'île à menace 0, partagés entre les régions (RE3)");
		SRP_CmdSettings.DeclareInt("res_blindes_plein", m_iArmorFull, "Blindés du Commandeur (RE3)");
		SRP_CmdSettings.DeclareInt("res_helico_plein", m_iHeliFull, "Sorties de l'hélico de recherche (AP7)");
		SRP_CmdSettings.DeclareInt("res_artillerie_plein", m_iArtilleryFull, "Tirs d'artillerie lourde (RE3)");
		SRP_CmdSettings.DeclareInt("res_bombe_plein", m_iBombFull, "Bombes (AP3)");
		SRP_CmdSettings.DeclareInt("res_bonus_menace_centiemes", m_iThreatBonusCent, "Pleins grossis de ce nombre de centièmes par point de menace (RE10)");
		SRP_CmdSettings.DeclareBool("res_bombe_bonus_menace", m_bBombThreatBonus, "La bombe grossit aussi avec la menace (AP3)");
		SRP_CmdSettings.DeclareFloat("res_heures_plein", m_fRefillHours, "Heures pour passer de vide à plein au revenu plein (RE5)");
		SRP_CmdSettings.DeclareInt("res_plancher_centiemes", m_iFloorCent, "Revenu jamais sous ce nombre de centièmes (RE4)");
		SRP_CmdSettings.DeclareInt("res_zones_depart", m_iRefZonesOverride, "Zones de départ de l'île (0 = somme du tracé des régions) (RE4)");
		SRP_CmdSettings.DeclareInt("res_depot_coupure_heures", m_iDepotCutHours, "Dépôt saboté : coupure du revenu de la région, en heures (RE8)");
		SRP_CmdSettings.DeclareInt("res_depot_coupure_centiemes", m_iDepotCutCent, "Dépôt saboté : part du revenu coupée, en centièmes (RE8)");
		SRP_CmdSettings.DeclareInt("res_renflouement_minutes", m_iResupplyMinutes, "Renflouement d'une région à sec : délai d'arrivée, en minutes (RE2)");
		SRP_CmdSettings.DeclareInt("res_renflouement_seuil_obus", m_iResupplyThreshold, "Une région est à sec sous ce nombre d'obus (RE2)");
		SRP_CmdSettings.DeclareInt("res_renflouement_centiemes", m_iResupplyCent, "Renflouement jusqu'à ce nombre de centièmes du plein de la région (RE2)");
		SRP_CmdSettings.DeclareInt("res_renflouement_donneur_centiemes", m_iDonorKeepCent, "Le donneur garde ce nombre de centièmes de son plein (RE2)");
		SRP_CmdSettings.DeclareInt("res_sabotage_centiemes", m_iSabotageCent, "Mission SABOTAGE ou CACHE réussie : retrait de ce nombre de centièmes du plein (RE8)");
		SRP_CmdSettings.DeclareInt("res_offensive_bonus", m_iOffensiveBonus, "Offensive de la nuit : chances en plus si les stocks étaient pleins au départ du dernier joueur (CA7)");
		SRP_CmdSettings.DeclareInt("res_offensive_plein_centiemes", m_iOffensiveFullCent, "Stocks jugés pleins à partir de ce nombre de centièmes (CA7)");
		SRP_CmdSettings.DeclareInt("res_offensive_bas_centiemes", m_iOffensiveLowCent, "Pas d'offensive de la nuit sous ce nombre de centièmes (CA7)");
		SRP_CmdSettings.DeclareInt("res_etat_affaiblie_centiemes", m_iStateWeakCent, "Staff : ravitaillement d'une région affaibli sous ce nombre de centièmes (VU2)");
		SRP_CmdSettings.DeclareInt("res_etat_renforcee_centiemes", m_iStateStrongCent, "Staff : ravitaillement renforcé à partir de ce nombre de centièmes (VU2)");
		if (!m_Depots)
			return;
		m_Depots.DeclareSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés puis m_Depots.LoadSettings ; les pleins sont bornés aux nouveaux pleins — SRP_Commander.LoadSettings
	void LoadSettings()
	{
		m_iShellsFull = SRP_CmdSettings.GetIntClamped("res_obus_plein", 0, 1000);
		m_iArmorFull = SRP_CmdSettings.GetIntClamped("res_blindes_plein", 0, 20);
		m_iHeliFull = SRP_CmdSettings.GetIntClamped("res_helico_plein", 0, 20);
		m_iArtilleryFull = SRP_CmdSettings.GetIntClamped("res_artillerie_plein", 0, 20);
		m_iBombFull = SRP_CmdSettings.GetIntClamped("res_bombe_plein", 0, 10);
		m_iThreatBonusCent = SRP_CmdSettings.GetIntClamped("res_bonus_menace_centiemes", 0, 50);
		m_bBombThreatBonus = SRP_CmdSettings.GetBool("res_bombe_bonus_menace");
		m_fRefillHours = SRP_CmdSettings.GetFloatClamped("res_heures_plein", 0.1, 72.0);
		m_iFloorCent = SRP_CmdSettings.GetIntClamped("res_plancher_centiemes", 0, 100);
		m_iRefZonesOverride = SRP_CmdSettings.GetIntClamped("res_zones_depart", 0, 200);
		m_iDepotCutHours = SRP_CmdSettings.GetIntClamped("res_depot_coupure_heures", 0, 240);
		m_iDepotCutCent = SRP_CmdSettings.GetIntClamped("res_depot_coupure_centiemes", 0, 100);
		m_iResupplyMinutes = SRP_CmdSettings.GetIntClamped("res_renflouement_minutes", 0, 600);
		m_iResupplyThreshold = SRP_CmdSettings.GetIntClamped("res_renflouement_seuil_obus", 0, 100);
		m_iResupplyCent = SRP_CmdSettings.GetIntClamped("res_renflouement_centiemes", 0, 100);
		m_iDonorKeepCent = SRP_CmdSettings.GetIntClamped("res_renflouement_donneur_centiemes", 0, 100);
		m_iSabotageCent = SRP_CmdSettings.GetIntClamped("res_sabotage_centiemes", 0, 100);
		m_iOffensiveBonus = SRP_CmdSettings.GetIntClamped("res_offensive_bonus", 0, 100);
		m_iOffensiveFullCent = SRP_CmdSettings.GetIntClamped("res_offensive_plein_centiemes", 0, 100);
		m_iOffensiveLowCent = SRP_CmdSettings.GetIntClamped("res_offensive_bas_centiemes", 0, 100);
		m_iStateWeakCent = SRP_CmdSettings.GetIntClamped("res_etat_affaiblie_centiemes", 0, 100);
		m_iStateStrongCent = SRP_CmdSettings.GetIntClamped("res_etat_renforcee_centiemes", 0, 100);
		if (m_Depots)
			m_Depots.LoadSettings();

		// Relecture en cours de partie : parts d'obus refaites d'après le nouveau plein de l'île, stocks bornés aux
		// nouveaux pleins (au démarrage, les régions ne sont pas encore connues : Start le fera)
		if (m_aRegions.IsEmpty())
			return;
		ComputeShellShares();
		CapToFull();
	}

	//------------------------------------------------------------------------------------------------
	//! Front et régions prêts : une SRP_CmdRegionSupply par région (zones de départ), parts d'obus (48 x zones ÷ total,
	//! plus forts restes), rien lu -> tout plein, sinon rattrapage du revenu (RE6) ; m_Depots.Start — SRP_Commander.Start
	void Start(int nowUnix)
	{
		BuildSupplies();
		RefreshRedCache();
		if (!m_bStateRead)
		{
			FillAll();
			m_iUpdatedAt = nowUnix;
			m_bStateRead = true;
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, "Stocks du Commandeur pleins au premier démarrage : " + SummaryText());
		}
		else
		{
			int readAt = m_iUpdatedAt;
			Advance(nowUnix);
			// Au démarrage, LoadSettings est passé avant le tracé des régions : les stocks relus sont bornés ici aux
			// pleins du profil (un plein abaissé dans le fichier puis redémarrage)
			CapToFull();
			int elapsed = 0;
			if (readAt > 0 && nowUnix > readAt)
				elapsed = nowUnix - readAt;
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Stocks relus, revenu rattrapé sur %1 (temps réel, serveur éteint compris) : %2", DurationWords(elapsed), SummaryText()));
		}
		m_iLastPlayerCount = GetGame().GetPlayerManager().GetPlayerCount();
		m_bStarted = true;
		if (m_Depots)
			m_Depots.Start(m_aRegions.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Arrêt : m_Depots.Stop — SRP_Commander.Stop
	void Stop()
	{
		m_bStarted = false;
		if (m_Depots)
			m_Depots.Stop();
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 60 s : zones rouges recomptées, revenu avancé, renflouements (arrivées, nouveaux si le Commandeur
	//! n'est pas gelé), départ du dernier joueur (CA7), m_Depots.Tick, MarkDirty(false) si changé — SRP_Commander.Tick
	void Tick(int nowUnix)
	{
		if (m_aRegions.Count() != BookRegionCount())
			BuildSupplies();
		m_bChanged = false;
		RefreshRedCache();
		Advance(nowUnix);
		CheckTransfers(nowUnix);
		if (m_Commander && !m_Commander.IsFrozen())
			CheckResupply(nowUnix);
		CheckLastLeave(nowUnix);
		if (m_Depots)
			m_Depots.Tick(nowUnix);
		// Le revenu seul ne demande pas d'écriture : stocks et m_iUpdatedAt restent cohérents, la relecture rattrape
		if (m_bChanged)
			Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Les dépôts cachés (RE8, RE9) — SRP_Commander, SRP_CmdIntel, SRP_CmdScreens, SRP_FrontMarkers
	SRP_CmdDepots GetDepots()
	{
		return m_Depots;
	}

	//================================================================================================
	// Lecture (ordre des paramètres : kind, region)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Stock actuel (décimal : revenu continu) ; region ignorée hors OBUS — moyens, écrans
	float GetStock(int kind, int region)
	{
		if (kind == SRP_ECmdStock.OBUS)
		{
			if (region < 0)
			{
				float total = 0;
				foreach (SRP_CmdRegionSupply entry : m_aRegions)
					total += entry.m_fShells;
				return total;
			}
			SRP_CmdRegionSupply supply = GetSupply(region);
			if (!supply)
				return 0;
			return supply.m_fShells;
		}
		if (kind == SRP_ECmdStock.BLINDE)
			return m_fArmor;
		if (kind == SRP_ECmdStock.HELICO)
			return m_fHeli;
		if (kind == SRP_ECmdStock.ARTILLERIE)
			return m_fArtillery;
		if (kind == SRP_ECmdStock.BOMBE)
			return m_fBomb;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Plein actuel : part x (1 + res_bonus_menace_centiemes / 100 x menace) ; 0 d'obus pour une région sans zone
	//! rouge ; bombe sans bonus sauf res_bombe_bonus_menace — moyens, écrans
	float GetFull(int kind, int region)
	{
		return FullAt(kind, region, Threat());
	}

	//------------------------------------------------------------------------------------------------
	//! Facteur de revenu (plancher res_plancher_centiemes, coupure du dépôt) — écrans
	float GetRevenueFactor(int kind, int region)
	{
		int nowUnix = System.GetUnixTime();
		if (kind == SRP_ECmdStock.OBUS)
		{
			SRP_CmdRegionSupply supply = GetSupply(region);
			if (supply)
				return RegionFactor(supply, nowUnix);
		}
		return HqFactor(nowUnix);
	}

	//------------------------------------------------------------------------------------------------
	//! Minutes avant la prochaine unité entière, -1 si plein ou revenu nul — écrans
	int MinutesToNextUnit(int kind, int region)
	{
		float stock = GetStock(kind, region);
		float full = GetFull(kind, region);
		if (full <= 0 || stock >= full)
			return -1;
		float hours = m_fRefillHours;
		if (kind == SRP_ECmdStock.BOMBE)
			hours = BombHours();
		float perHour = full / Math.Max(0.1, hours) * GetRevenueFactor(kind, region);
		if (perHour <= 0)
			return -1;
		float target = Math.Floor(stock) + 1;
		if (target > full)
			target = full;
		float minutes = (target - stock) / perHour * 60;
		int result = Math.Ceil(minutes);
		if (result < 1)
			result = 1;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Tickets ouverts d'un stock (EQ3 : un blindé à la fois) — SRP_CmdArmor, écrans
	int CountOpen(int kind)
	{
		int count = 0;
		foreach (SRP_CmdTicket ticket : m_aTickets)
		{
			if (ticket && ticket.m_iKind == kind && ticket.m_iLeft > 0)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! CA7 : moyenne des remplissages (obus de l'île, blindés, hélico, artillerie), de 0 à 1 — CheckLastLeave, écrans
	float GetStockLevel()
	{
		float sum = 0;
		int counted = 0;
		float shells = Ratio(SRP_ECmdStock.OBUS, -1);
		if (shells >= 0)
		{
			sum += shells;
			counted++;
		}
		float armor = Ratio(SRP_ECmdStock.BLINDE, -1);
		if (armor >= 0)
		{
			sum += armor;
			counted++;
		}
		float heli = Ratio(SRP_ECmdStock.HELICO, -1);
		if (heli >= 0)
		{
			sum += heli;
			counted++;
		}
		float artillery = Ratio(SRP_ECmdStock.ARTILLERIE, -1);
		if (artillery >= 0)
		{
			sum += artillery;
			counted++;
		}
		if (counted == 0)
			return 1.0;
		return sum / counted;
	}

	//------------------------------------------------------------------------------------------------
	//! Remplissage d'une région (obus ÷ plein), ou des moyens lourds de l'île pour region = -1 ; de 0 à 1 — humeur
	//! (CO7), SRP_CmdManeuvers (CA1)
	float GetFillRatio(int region)
	{
		if (region >= 0)
		{
			SRP_CmdRegionSupply supply = GetSupply(region);
			if (!supply)
				return 0;
			float shellsFull = ShellsFull(supply);
			if (shellsFull <= 0)
			{
				// plein réglé à 0 : une région vivante n'est pas « à sec » pour autant
				if (supply.m_iRedCache > 0)
					return 1.0;
				return 0;
			}
			return Math.Clamp(supply.m_fShells / shellsFull, 0, 1);
		}
		float sum = 0;
		int counted = 0;
		float armor = Ratio(SRP_ECmdStock.BLINDE, -1);
		if (armor >= 0)
		{
			sum += armor;
			counted++;
		}
		float heli = Ratio(SRP_ECmdStock.HELICO, -1);
		if (heli >= 0)
		{
			sum += heli;
			counted++;
		}
		float artillery = Ratio(SRP_ECmdStock.ARTILLERIE, -1);
		if (artillery >= 0)
		{
			sum += artillery;
			counted++;
		}
		if (counted == 0)
			return 1.0;
		return sum / counted;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff seulement : 0 affaiblie, 1 normale, 2 renforcée (res_etat_*_centiemes ; coupure = affaiblie) — écrans
	int GetRegionSupplyState(int region)
	{
		SRP_CmdRegionSupply supply = GetSupply(region);
		if (!supply)
			return 1;
		if (supply.m_iCutUntil > System.GetUnixTime())
			return 0;
		int cent = Math.Round(GetFillRatio(region) * 100);
		if (cent < m_iStateWeakCent)
			return 0;
		if (cent >= m_iStateStrongCent)
			return 2;
		return 1;
	}

	//================================================================================================
	// Dépense par tickets (Reserve, puis Consume ou Release)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Contrôle sans rien retirer : OBUS -> région valide avec une zone rouge ; Math.Floor(stock) >= amount ; rend
	//! faux avec la raison — moyens (avant de poser une pièce)
	bool CanReserve(int kind, int region, int amount, out string refusal)
	{
		refusal = "";
		RefreshRedCache();
		Advance(System.GetUnixTime());
		if (kind < SRP_ECmdStock.OBUS || kind > SRP_ECmdStock.BOMBE)
		{
			refusal = "stock inconnu";
			return false;
		}
		if (amount <= 0)
		{
			refusal = "rien à engager";
			return false;
		}
		if (kind == SRP_ECmdStock.OBUS)
		{
			SRP_CmdRegionSupply supply = GetSupply(region);
			if (!supply)
			{
				refusal = "obus : région inconnue";
				return false;
			}
			if (supply.m_iRedCache <= 0)
			{
				refusal = "obus : la " + RegionLabel(region) + " n'a plus de zone rouge";
				return false;
			}
		}
		if (Math.Floor(GetStock(kind, region)) < amount)
		{
			refusal = string.Format("stock insuffisant : %1, %2 demandé(s)", StockText(kind, region), amount);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire amount tout de suite et ouvre un ticket (staff et cmd_staff_gratuit : ticket gratuit, rien retiré) ;
	//! 0 si refusé (SRP_CmdLog.Refusal) ; sinon SRP_CmdLog « %1 : %2 engagé(s), reste %3 », MarkDirty(false) ; rend le
	//! numéro du ticket — SRP_CmdSupport, SRP_CmdArmor
	int Reserve(int kind, int region, int amount, string what, bool staff)
	{
		int logRegion = LogRegion(kind, region);
		if (kind < SRP_ECmdStock.OBUS || kind > SRP_ECmdStock.BOMBE || amount <= 0)
		{
			SRP_CmdLog.Refusal(logRegion, what, "engagement invalide");
			return 0;
		}
		bool isFree = false;
		if (staff && m_Commander && m_Commander.m_bStaffFree)
			isFree = true;		// VU5 : décision forcée par le Staff, gratuite (cmd_staff_gratuit)
		string freeText = "";
		if (isFree)
		{
			freeText = " sans frais (décision du Staff)";
		}
		else
		{
			string refusal;
			if (!CanReserve(kind, region, amount, refusal))
			{
				SRP_CmdLog.Refusal(logRegion, what, refusal);
				return 0;
			}
			SetStockValue(kind, region, GetStock(kind, region) - amount);
		}

		SRP_CmdTicket ticket = new SRP_CmdTicket();
		ticket.m_iId = m_iNextTicket;
		m_iNextTicket++;
		ticket.m_iKind = kind;
		ticket.m_iRegion = region;
		if (kind != SRP_ECmdStock.OBUS)
			ticket.m_iRegion = -1;
		ticket.m_iLeft = amount;
		ticket.m_sWhat = what;
		ticket.m_iOpenedAt = System.GetUnixTime();
		ticket.m_bFree = isFree;
		m_aTickets.Insert(ticket);

		SRP_CmdLog.Note(logRegion, SRP_ECmdLogKind.STOCK, string.Format("%1 : %2 %3 engagé(s)%4, reste %5", what, amount, KindLabel(kind), freeText, StockText(kind, region)));
		Dirty(false);
		return ticket.m_iId;
	}

	//------------------------------------------------------------------------------------------------
	//! Effet réel : le ticket perd amount (perdu pour de bon), fermé à 0 — moyens (obus tiré, blindé détruit ou pris,
	//! sortie d'hélico finie, salve partie, bombe larguée)
	void Consume(int ticket, int amount)
	{
		SRP_CmdTicket open = FindTicket(ticket);
		if (!open || amount <= 0)
			return;
		open.m_iLeft = open.m_iLeft - amount;
		if (open.m_iLeft <= 0)
			m_aTickets.RemoveItem(open);
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Annulation ou retour : le reste du ticket revient au stock (borné au plein), ticket fermé, journal — moyens,
	//! ReadFrom (« redémarrage », RE13)
	void Release(int ticket, string reason)
	{
		SRP_CmdTicket open = FindTicket(ticket);
		if (!open)
			return;
		int kind = open.m_iKind;
		int region = open.m_iRegion;
		int left = open.m_iLeft;
		bool isFree = open.m_bFree;
		string what = open.m_sWhat;
		m_aTickets.RemoveItem(open);

		RefreshRedCache();
		Advance(System.GetUnixTime());
		float back = 0;
		if (!isFree && left > 0)
			back = AddToStock(kind, region, left);
		SRP_CmdLog.Note(LogRegion(kind, region), SRP_ECmdLogKind.STOCK, string.Format("%1 : %2 %3 rendu(s) au stock (%4), reste %5", what, DecText(back), KindLabel(kind), reason, StockText(kind, region)));
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Unités encore engagées sur le ticket, 0 s'il est fermé — moyens
	int GetTicketLeft(int ticket)
	{
		SRP_CmdTicket open = FindTicket(ticket);
		if (!open || open.m_iLeft < 0)
			return 0;
		return open.m_iLeft;
	}

	//================================================================================================
	// Évènements
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! RE8 « pareil » : mission SABOTAGE ou CACHE réussie dans la région : retrait de res_sabotage_centiemes du plein
	//! du stock visé (CACHE -> OBUS ; SABOTAGE selon le prefab : Mortier OBUS, Antichar BLINDE, _AA ou Radar HELICO,
	//! Transmissions ou Generateur ARTILLERIE, sinon OBUS) ; SRP_CmdLog seulement — SRP_Commander.OnMissionSucceeded
	void OnSabotageMission(int region, int missionType, string prefabName, string author)
	{
		int kind = -1;
		string title = "";
		if (missionType == SRP_EMissionType.CACHE)
		{
			kind = SRP_ECmdStock.OBUS;
			title = "Cache ennemie détruite";
		}
		else if (missionType == SRP_EMissionType.SABOTAGE)
		{
			kind = SabotageKind(prefabName);
			title = "Sabotage réussi";
		}
		if (kind < 0)
			return;

		RefreshRedCache();
		Advance(System.GetUnixTime());
		string by = "";
		if (!author.IsEmpty())
			by = " (" + author + ")";
		if (kind == SRP_ECmdStock.OBUS && !GetSupply(region))
		{
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, title + by + " hors des régions ennemies : aucun obus retiré");
			return;
		}
		float full = GetFull(kind, region);
		float removed = m_iSabotageCent / 100.0 * full;
		float before = GetStock(kind, region);
		if (removed > before)
			removed = before;
		SetStockValue(kind, region, before - removed);
		SRP_CmdLog.Note(LogRegion(kind, region), SRP_ECmdLogKind.STOCK, string.Format("%1%2 : %3 %4 en moins, reste %5", title, by, DecText(removed), KindLabel(kind), StockText(kind, region)));
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! RE8 : revenu de la région coupé de res_depot_coupure_centiemes pendant res_depot_coupure_heures (relancé, jamais
	//! cumulé), MarkDirty(true) — SRP_CmdDepots.Sabotage
	void CutRegion(int region, string reason)
	{
		SRP_CmdRegionSupply supply = GetSupply(region);
		if (!supply)
			return;
		int nowUnix = System.GetUnixTime();
		RefreshRedCache();
		Advance(nowUnix);		// revenu d'avant la coupure compté au taux d'avant
		if (m_iDepotCutHours <= 0)
		{
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.STOCK, string.Format("Revenu de la %1 : coupure réglée à 0 h, aucun effet (%2)", RegionLabel(region), reason));
			return;
		}
		supply.m_iCutUntil = nowUnix + m_iDepotCutHours * 3600;		// relancée, jamais cumulée
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.STOCK, string.Format("Revenu de la %1 réduit de %2 centièmes pendant %3 h (%4)", RegionLabel(region), m_iDepotCutCent, m_iDepotCutHours, reason));
		Dirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! RE2 : renflouement tout de suite, sans seuil (donneur : région vivante qui a le plus d'obus au-dessus de
	//! res_renflouement_donneur_centiemes) — CheckResupply, Staff (SRP_CmdScreens, action renflouer:R3)
	void RequestResupply(int region, string author)
	{
		int nowUnix = System.GetUnixTime();
		RefreshRedCache();
		Advance(nowUnix);
		string refusal = DoResupply(region, nowUnix, author);
		if (!refusal.IsEmpty())
		{
			SRP_CmdLog.Note(LogRegion(SRP_ECmdStock.OBUS, region), SRP_ECmdLogKind.STOCK, string.Format("Renflouement de la %1 demandé par %2 : impossible, %3", RegionLabel(region), AuthorText(author), refusal));
			return;
		}
		CheckTransfers(nowUnix);		// délai réglé à 0 : arrivée tout de suite
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Une zone a changé de camp : revenu avancé avec les anciennes zones, zones rouges recomptées ; région sans zone
	//! rouge : obus à 0 et renflouements vers elle rendus ; puis m_Depots.OnZoneOwnerChanged(zone, owner, staff) ;
	//! staff (Q9) : même état, aucune annonce — SRP_Commander.OnZoneCaptured / OnZoneLost
	void OnZoneOwnerChanged(int zone, int owner, bool staff)
	{
		if (m_aRegions.Count() != BookRegionCount())
			BuildSupplies();
		Advance(System.GetUnixTime());		// le comptage n'a pas encore bougé : revenu jusqu'ici avec les anciennes zones
		RefreshRedCache();
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			if (supply.m_iRedCache > 0)
				continue;
			if (supply.m_fShells > 0)
				SRP_CmdLog.Note(supply.m_iRegion, SRP_ECmdLogKind.STOCK, string.Format("Plus aucune zone rouge dans la %1 : %2 obus perdus", RegionLabel(supply.m_iRegion), DecText(supply.m_fShells)));
			supply.m_fShells = 0;
			ReturnTransfersTo(supply.m_iRegion);
		}
		if (m_Depots)
			m_Depots.OnZoneOwnerChanged(zone, owner, staff);
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! RE10 : la menace a changé : revenu avancé, puis chaque stock borné au nouveau plein — SRP_Commander.OnThreatChanged
	void OnThreatChanged(int threat)
	{
		RefreshRedCache();
		Advance(System.GetUnixTime());
		CapToFull();
		string fulls = string.Format("obus %1, blindés %2, hélico %3, artillerie %4, bombe %5", DecText(GetFull(SRP_ECmdStock.OBUS, -1)), DecText(GetFull(SRP_ECmdStock.BLINDE, -1)), DecText(GetFull(SRP_ECmdStock.HELICO, -1)), DecText(GetFull(SRP_ECmdStock.ARTILLERIE, -1)), DecText(GetFull(SRP_ECmdStock.BOMBE, -1)));
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Menace %1 : nouveaux pleins (%2)", threat, fulls));
		Dirty(false);
	}

	//================================================================================================
	// Offensive de la nuit (CA7, seule implémentation)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Pas d'offensive si le niveau noté au départ du dernier joueur est sous res_offensive_bas_centiemes —
	//! SRP_CmdManeuvers.NightChance
	bool IsOffensiveAllowedByStocks()
	{
		int cent = Math.Round(NotedLevel() * 100);
		return cent >= m_iOffensiveLowCent;
	}

	//------------------------------------------------------------------------------------------------
	//! + res_offensive_bonus si le niveau noté atteint res_offensive_plein_centiemes, sinon 0 (le plafond de 60 reste
	//! au front) — SRP_CmdManeuvers.NightChance
	int OffensiveChanceBonus()
	{
		int cent = Math.Round(NotedLevel() * 100);
		if (cent >= m_iOffensiveFullCent)
			return m_iOffensiveBonus;
		return 0;
	}

	//================================================================================================
	// Sauvegarde (front.json, clés cmd_res_*) et remises
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! cmd_res_v, cmd_res_updated, cmd_res_blinde, cmd_res_helico, cmd_res_artillerie, cmd_res_bombe,
	//! cmd_res_niveau_depart, cmd_res_depart_unix ; cmd_res_n puis cmd_res_rN_code, _obus, _coupe ; cmd_res_tn puis
	//! cmd_res_tN_kind, _region, _left, _what, _free ; cmd_res_vn puis cmd_res_vN_from, _to, _amount ; puis
	//! m_Depots.WriteTo — SRP_Commander.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		// État jamais défini (sauvegarde du socle avant le premier Start) : rien d'écrit, la relecture partira des pleins
		if (!m_bStateRead)
			return;
		int saveFormat = SAVE_FORMAT;
		ctx.WriteValue("cmd_res_v", saveFormat);
		ctx.WriteValue("cmd_res_updated", m_iUpdatedAt);
		ctx.WriteValue("cmd_res_blinde", m_fArmor);
		ctx.WriteValue("cmd_res_helico", m_fHeli);
		ctx.WriteValue("cmd_res_artillerie", m_fArtillery);
		ctx.WriteValue("cmd_res_bombe", m_fBomb);
		ctx.WriteValue("cmd_res_niveau_depart", m_fLevelAtLastLeave);
		ctx.WriteValue("cmd_res_depart_unix", m_iLastLeaveAt);

		ctx.WriteValue("cmd_res_n", m_aRegions.Count());
		foreach (int i, SRP_CmdRegionSupply supply : m_aRegions)
		{
			string prefix = "cmd_res_r" + i.ToString() + "_";
			ctx.WriteValue(prefix + "code", supply.m_sCode);
			ctx.WriteValue(prefix + "obus", supply.m_fShells);
			ctx.WriteValue(prefix + "coupe", supply.m_iCutUntil);
		}

		ctx.WriteValue("cmd_res_tn", m_aTickets.Count());
		foreach (int t, SRP_CmdTicket ticket : m_aTickets)
		{
			string ticketPrefix = "cmd_res_t" + t.ToString() + "_";
			ctx.WriteValue(ticketPrefix + "kind", ticket.m_iKind);
			ctx.WriteValue(ticketPrefix + "region", CodeOf(ticket.m_iRegion));
			ctx.WriteValue(ticketPrefix + "left", ticket.m_iLeft);
			ctx.WriteValue(ticketPrefix + "what", ticket.m_sWhat);
			ctx.WriteValue(ticketPrefix + "free", ticket.m_bFree);
		}

		ctx.WriteValue("cmd_res_vn", m_aTransfers.Count());
		foreach (int v, SRP_CmdTransfer transfer : m_aTransfers)
		{
			string transferPrefix = "cmd_res_v" + v.ToString() + "_";
			ctx.WriteValue(transferPrefix + "from", CodeOf(transfer.m_iFrom));
			ctx.WriteValue(transferPrefix + "to", CodeOf(transfer.m_iTo));
			ctx.WriteValue(transferPrefix + "amount", transfer.m_iAmount);
		}

		if (m_Depots)
			m_Depots.WriteTo(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture miroir (lecteur tolérant : clé absente = défaut) ; nombre de régions changé : tout plein et ERREUR ;
	//! RE13 : chaque ticket lu est rendu, chaque renflouement lu revient au donneur (une seule fois) ; journal ; puis
	//! m_Depots.ReadFrom — SRP_Commander.ReadFrom
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		int nowUnix = System.GetUnixTime();
		BuildSupplies();
		RefreshRedCache();
		// Les engagements en mémoire (restauration d'une copie) ne valent plus : l'état relu les remplace
		m_aTickets.Clear();
		m_aTransfers.Clear();

		int saveFormat = JsonInt(ctx, "cmd_res_v", 0);
		if (saveFormat <= 0)
		{
			// Aucune clé des stocks (fichier d'avant le Commandeur, ou copie datée sans eux)
			if (m_bStarted)
			{
				FillAll();
				m_iUpdatedAt = nowUnix;
				m_bStateRead = true;
				SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, "Copie relue sans stocks du Commandeur : stocks remis pleins");
			}
			else
			{
				m_bStateRead = false;		// Start posera les pleins
			}
			if (m_Depots)
				m_Depots.ReadFrom(ctx);
			return;
		}

		m_iUpdatedAt = JsonInt(ctx, "cmd_res_updated", nowUnix);
		m_fLevelAtLastLeave = JsonFloat(ctx, "cmd_res_niveau_depart", -1);
		m_iLastLeaveAt = JsonInt(ctx, "cmd_res_depart_unix", 0);

		int count = JsonInt(ctx, "cmd_res_n", -1);
		if (count != m_aRegions.Count())
		{
			FillAll();
			foreach (SRP_CmdRegionSupply fresh : m_aRegions)
				fresh.m_iCutUntil = 0;
			m_iUpdatedAt = nowUnix;
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("ERREUR : front.json garde %1 région(s), le tracé en compte %2 : stocks du Commandeur remis pleins", count, m_aRegions.Count()));
		}
		else
		{
			m_fArmor = JsonFloat(ctx, "cmd_res_blinde", GetFull(SRP_ECmdStock.BLINDE, -1));
			m_fHeli = JsonFloat(ctx, "cmd_res_helico", GetFull(SRP_ECmdStock.HELICO, -1));
			m_fArtillery = JsonFloat(ctx, "cmd_res_artillerie", GetFull(SRP_ECmdStock.ARTILLERIE, -1));
			m_fBomb = JsonFloat(ctx, "cmd_res_bombe", GetFull(SRP_ECmdStock.BOMBE, -1));
			// Une région dont le code n'est pas retrouvé (tracé refait avec d'autres codes) part pleine, sans coupure ;
			// la boucle suivante écrase les régions retrouvées
			foreach (SRP_CmdRegionSupply unread : m_aRegions)
			{
				unread.m_fShells = ShellsFull(unread);
				unread.m_iCutUntil = 0;
			}
			int found = 0;
			for (int i = 0; i < count; i++)
			{
				string prefix = "cmd_res_r" + i.ToString() + "_";
				SRP_CmdRegionSupply supply = FindSupplyByCode(JsonString(ctx, prefix + "code", ""));
				if (!supply)
					continue;
				supply.m_fShells = Math.Max(0, JsonFloat(ctx, prefix + "obus", ShellsFull(supply)));
				supply.m_iCutUntil = JsonInt(ctx, prefix + "coupe", 0);
				found++;
			}
			if (found < count)
				SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("front.json : %1 région(s) sur %2 retrouvée(s) par leur code ; les autres repartent pleines", found, count));

			// RE13 : chaque engagement relu revient au stock, une seule fois (il n'est pas gardé)
			int tickets = JsonInt(ctx, "cmd_res_tn", 0);
			float unitsBack = 0;
			for (int t = 0; t < tickets; t++)
			{
				string ticketPrefix = "cmd_res_t" + t.ToString() + "_";
				int kind = JsonInt(ctx, ticketPrefix + "kind", SRP_ECmdStock.OBUS);
				int left = JsonInt(ctx, ticketPrefix + "left", 0);
				bool isFree = JsonBool(ctx, ticketPrefix + "free", false);
				if (isFree || left <= 0)
					continue;
				int ticketRegion = RegionOfCode(JsonString(ctx, ticketPrefix + "region", ""));
				unitsBack += AddToStock(kind, ticketRegion, left);
			}
			// RE13 : chaque renflouement relu revient au donneur
			int transfers = JsonInt(ctx, "cmd_res_vn", 0);
			float shellsBack = 0;
			for (int v = 0; v < transfers; v++)
			{
				string transferPrefix = "cmd_res_v" + v.ToString() + "_";
				SRP_CmdRegionSupply donor = FindSupplyByCode(JsonString(ctx, transferPrefix + "from", ""));
				shellsBack += AddShells(donor, JsonInt(ctx, transferPrefix + "amount", 0));
			}
			if (tickets > 0 || transfers > 0)
				SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Redémarrage : %1 engagement(s) (%2 unité(s)) et %3 renflouement(s) en route (%4 obus) rendus aux stocks", tickets, DecText(unitsBack), transfers, DecText(shellsBack)));
		}
		m_bStateRead = true;
		if (m_Depots)
			m_Depots.ReadFrom(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne : stocks pleins (menace 0), coupures, tickets et renflouements vidés, m_Depots.Reset —
	//! SRP_Commander.ResetCampaign
	void ResetCampaign(string author)
	{
		BuildSupplies();
		RefreshRedCache();
		// Pleins à menace 0 ; toutes les zones repartent rouges : la part entière de chaque région, sans attendre le
		// prochain comptage
		m_fArmor = FullAt(SRP_ECmdStock.BLINDE, -1, 0);
		m_fHeli = FullAt(SRP_ECmdStock.HELICO, -1, 0);
		m_fArtillery = FullAt(SRP_ECmdStock.ARTILLERIE, -1, 0);
		m_fBomb = FullAt(SRP_ECmdStock.BOMBE, -1, 0);
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			supply.m_fShells = supply.m_iShellShare;
			supply.m_iCutUntil = 0;
		}
		m_aTickets.Clear();
		m_aTransfers.Clear();
		m_fLevelAtLastLeave = -1;
		m_iLastLeaveAt = 0;
		m_iUpdatedAt = System.GetUnixTime();
		m_bStateRead = true;
		if (m_Depots)
			m_Depots.Reset(author);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Nouvelle campagne (%1) : stocks du Commandeur pleins, coupures et engagements effacés", author));
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Après une restauration (ReadFrom de la copie fait) : revenu avancé jusqu'à maintenant (RE6) — SRP_Commander
	void OnRestored()
	{
		int nowUnix = System.GetUnixTime();
		int readAt = m_iUpdatedAt;
		RefreshRedCache();
		Advance(nowUnix);
		CapToFull();
		int elapsed = 0;
		if (readAt > 0 && nowUnix > readAt)
			elapsed = nowUnix - readAt;
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Restauration : revenu rattrapé sur %1 : %2", DurationWords(elapsed), SummaryText()));
		Dirty(false);
	}

	//================================================================================================
	// Staff (VU5 ; textes réservés au Staff)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Fixe un stock (borné au plein) ; SRP_CmdLog.StaffAction — SRP_CmdScreens (stock:<genre>:<R3|ile>:<0|plein>)
	void StaffSetStock(int kind, int region, float value, string author)
	{
		if (kind < SRP_ECmdStock.OBUS || kind > SRP_ECmdStock.BOMBE)
			return;
		RefreshRedCache();
		Advance(System.GetUnixTime());
		float full = GetFull(kind, region);
		float wanted = Math.Clamp(value, 0, Math.Max(0, full));
		string place = "";
		if (kind == SRP_ECmdStock.OBUS && region < 0)
		{
			// obus de toute l'île : chaque région à la même part de son plein
			float ratio = 0;
			if (full > 0)
				ratio = wanted / full;
			foreach (SRP_CmdRegionSupply supply : m_aRegions)
				supply.m_fShells = ShellsFull(supply) * ratio;
			place = " (toute l'île)";
		}
		else
		{
			if (kind == SRP_ECmdStock.OBUS && !GetSupply(region))
				return;
			SetStockValue(kind, region, wanted);
			if (kind == SRP_ECmdStock.OBUS)
				place = " (" + RegionLabel(region) + ")";
		}
		SRP_CmdLog.StaffAction(author, string.Format("stock %1%2 fixé : %3", KindLabel(kind), place, StockText(kind, region)));
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les stocks pleins ; SRP_CmdLog.StaffAction — SRP_CmdScreens (stock-plein)
	void StaffRefillAll(string author)
	{
		RefreshRedCache();
		Advance(System.GetUnixTime());
		FillAll();
		SRP_CmdLog.StaffAction(author, "tous les stocks du Commandeur remis pleins : " + SummaryText());
		Dirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! « Obus : 31 sur 48 (…) · Blindés : 1,6 sur 2, prochain dans 48 min · Hélico : 3 sur 3 · … » — SRP_CmdScreens
	string GetStaffReport()
	{
		int nowUnix = System.GetUnixTime();
		SRP_CmdRegionBook book = Book();
		string regions = "";
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			string name = supply.m_sCode;
			if (book)
				name = book.GetRegionName(supply.m_iRegion);
			string part = string.Format("%1 %2/%3", name, DecText(supply.m_fShells), DecText(ShellsFull(supply)));
			if (supply.m_iRedCache <= 0)
				part = part + " sans zone rouge";
			if (supply.m_iCutUntil > nowUnix)
				part = part + ", coupé encore " + DurationWords(supply.m_iCutUntil - nowUnix);
			if (HasTransferTo(supply.m_iRegion))
				part = part + ", renflouement en route";
			if (!regions.IsEmpty())
				regions = regions + " ; ";
			regions = regions + part;
		}
		string text = string.Format("Obus : %1 sur %2", DecText(GetStock(SRP_ECmdStock.OBUS, -1)), DecText(GetFull(SRP_ECmdStock.OBUS, -1)));
		if (!regions.IsEmpty())
			text = text + " (" + regions + ")";
		text = text + " · " + IslandLine("Blindés", SRP_ECmdStock.BLINDE);
		text = text + " · " + IslandLine("Hélico", SRP_ECmdStock.HELICO);
		text = text + " · " + IslandLine("Tir lourd", SRP_ECmdStock.ARTILLERIE);
		text = text + " · " + IslandLine("Bombe", SRP_ECmdStock.BOMBE);
		int hqCent = Math.Round(HqFactor(nowUnix) * 100);
		text = text + string.Format(" · revenu de l'île : %1 centièmes · engagés : %2 · renflouements en route : %3", hqCent, m_aTickets.Count(), m_aTransfers.Count());
		if (m_fLevelAtLastLeave >= 0)
		{
			int levelCent = Math.Round(m_fLevelAtLastLeave * 100);
			text = text + string.Format(" · niveau au départ du dernier joueur : %1 centièmes", levelCent);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « 31/48 obus », « 1/2 blindés » : stock d'une région ou de l'île en clair — SRP_CmdLog (coût, reste), écrans
	string StockText(int kind, int region)
	{
		return string.Format("%1/%2 %3", DecText(GetStock(kind, region)), DecText(GetFull(kind, region)), KindPlural(kind));
	}

	//------------------------------------------------------------------------------------------------
	//! Nom d'un stock : « obus », « blindé », « sortie d'hélico », « tir d'artillerie lourde », « bombe » — journal
	static string KindLabel(int kind)
	{
		if (kind == SRP_ECmdStock.OBUS)
			return "obus";
		if (kind == SRP_ECmdStock.BLINDE)
			return "blindé";
		if (kind == SRP_ECmdStock.HELICO)
			return "sortie d'hélico";
		if (kind == SRP_ECmdStock.ARTILLERIE)
			return "tir d'artillerie lourde";
		if (kind == SRP_ECmdStock.BOMBE)
			return "bombe";
		return "stock inconnu";
	}

	//================================================================================================
	// Accès en plus (dépôts, écrans)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Zones rouges de la région au dernier comptage (0 hors bornes) — SRP_CmdDepots
	int GetRedZones(int region)
	{
		SRP_CmdRegionSupply supply = GetSupply(region);
		if (!supply)
			return 0;
		return supply.m_iRedCache;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes de coupure du revenu restantes pour la région (0 = aucune) — SRP_CmdDepots (Staff), écrans
	int GetCutSecondsLeft(int region)
	{
		SRP_CmdRegionSupply supply = GetSupply(region);
		if (!supply)
			return 0;
		int left = supply.m_iCutUntil - System.GetUnixTime();
		if (left < 0)
			return 0;
		return left;
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : coupure du revenu de la région levée (revenu d'avant compté au taux coupé) — SRP_CmdDepots.StaffClearCut
	void ClearCut(int region, string author)
	{
		SRP_CmdRegionSupply supply = GetSupply(region);
		if (!supply)
			return;
		RefreshRedCache();
		Advance(System.GetUnixTime());
		if (supply.m_iCutUntil <= 0)
		{
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.STOCK, string.Format("Revenu de la %1 : aucune coupure à lever (%2)", RegionLabel(region), AuthorText(author)));
			return;
		}
		supply.m_iCutUntil = 0;
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.STOCK, string.Format("Revenu de la %1 : coupure levée par %2", RegionLabel(region), AuthorText(author)));
		Dirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! « 31 », « 1,6 » : une décimale, virgule française, « ,0 » omis (textes Staff)
	static string DecText(float value)
	{
		int tenths = Math.Round(value * 10);
		string sign = "";
		if (tenths < 0)
		{
			sign = "-";
			tenths = -tenths;
		}
		int whole = tenths / 10;
		int rest = tenths - whole * 10;
		if (rest == 0)
			return sign + whole.ToString();
		return sign + whole.ToString() + "," + rest.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! « 45 min », « 17 h », « 2 h 05 » (textes Staff)
	static string DurationWords(int seconds)
	{
		int total = seconds;
		if (total < 0)
			total = 0;
		if (total < 3600)
		{
			int minutes = (total + 59) / 60;
			return minutes.ToString() + " min";
		}
		int hours = total / 3600;
		int rest = (total - hours * 3600) / 60;
		if (rest == 0)
			return hours.ToString() + " h";
		string restText = rest.ToString();
		if (rest < 10)
			restText = "0" + restText;
		return hours.ToString() + " h " + restText;
	}

	//------------------------------------------------------------------------------------------------
	//! Lecteurs tolérants de front.json : clé absente -> fallback (ReadValue rend faux, LoadContext.c:21)
	static int JsonInt(JsonLoadContext ctx, string key, int fallback)
	{
		int value;
		if (ctx && ctx.ReadValue(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	static float JsonFloat(JsonLoadContext ctx, string key, float fallback)
	{
		float value;
		if (ctx && ctx.ReadValue(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	static string JsonString(JsonLoadContext ctx, string key, string fallback)
	{
		string value;
		if (ctx && ctx.ReadValue(key, value))
			return value;
		return fallback;
	}

	//------------------------------------------------------------------------------------------------
	static bool JsonBool(JsonLoadContext ctx, string key, bool fallback)
	{
		bool value;
		if (ctx && ctx.ReadValue(key, value))
			return value;
		return fallback;
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Facteur de revenu d'une région à l'instant at (plancher, coupure) — ApplyRevenue
	protected float RegionFactor(SRP_CmdRegionSupply supply, int at)
	{
		if (!supply || supply.m_iRedCache <= 0)
			return 0;
		// RE4 : part des zones de départ encore tenues, jamais sous le plancher
		float ratio = Math.Min(1.0, supply.m_iRedCache / Math.Max(1.0, supply.m_iRefZones));
		float factor = Math.Max(m_iFloorCent / 100.0, ratio);
		// RE8 : dépôt saboté
		if (supply.m_iCutUntil > at)
			factor = factor * (1 - m_iDepotCutCent / 100.0);
		return factor;
	}

	//------------------------------------------------------------------------------------------------
	//! Facteur de revenu des moyens de l'île (zones rouges tenues, régions coupées) — ApplyRevenue
	protected float HqFactor(int at)
	{
		if (m_aRegions.IsEmpty())
			return 1.0;
		int red = 0;
		float weighted = 0;
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			red += supply.m_iRedCache;
			float weight = supply.m_iRedCache;
			if (supply.m_iCutUntil > at)
				weight = weight * (1 - m_iDepotCutCent / 100.0);
			weighted += weight;
		}
		if (red <= 0)
			return 0;
		// RE4 : part des zones de départ de l'île encore tenues, jamais sous le plancher ; C1 : chaque région coupée
		// retire sa part (pondérée par ses zones rouges)
		float ratio = Math.Min(1.0, red / Math.Max(1.0, IslandRefZones()));
		float factor = Math.Max(m_iFloorCent / 100.0, ratio);
		return factor * weighted / red;
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute le revenu de hours heures (pleins en res_heures_plein ; bombe refaite en SRP_CmdSupport.m_iBombIntervalH
	//! heures, clé appui_bombe_intervalle_h, UNE clé pour « 1 bombe par jour » : 24 si les appuis sont absents, 0 lu
	//! comme 0,1 h) — Advance
	protected void ApplyRevenue(float hours, int at)
	{
		if (hours <= 0)
			return;
		float refill = Math.Max(0.1, m_fRefillHours);
		float hq = HqFactor(at);

		float armorFull = GetFull(SRP_ECmdStock.BLINDE, -1);
		if (m_fArmor < armorFull)
			m_fArmor = Math.Min(armorFull, m_fArmor + armorFull / refill * hq * hours);
		float heliFull = GetFull(SRP_ECmdStock.HELICO, -1);
		if (m_fHeli < heliFull)
			m_fHeli = Math.Min(heliFull, m_fHeli + heliFull / refill * hq * hours);
		float artilleryFull = GetFull(SRP_ECmdStock.ARTILLERIE, -1);
		if (m_fArtillery < artilleryFull)
			m_fArtillery = Math.Min(artilleryFull, m_fArtillery + artilleryFull / refill * hq * hours);
		float bombFull = GetFull(SRP_ECmdStock.BOMBE, -1);
		if (m_fBomb < bombFull)
			m_fBomb = Math.Min(bombFull, m_fBomb + bombFull / BombHours() * hq * hours);

		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			if (supply.m_iRedCache <= 0)
			{
				supply.m_fShells = 0;		// OF11 : région sans zone rouge, plus d'obus
				continue;
			}
			float shellsFull = ShellsFull(supply);
			if (supply.m_fShells < shellsFull)
				supply.m_fShells = Math.Min(shellsFull, supply.m_fShells + shellsFull / refill * RegionFactor(supply, at) * hours);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Avance le revenu de m_iUpdatedAt à now, par tranches coupées aux fins de coupure (32 au plus) — partout
	protected void Advance(int nowUnix)
	{
		if (m_iUpdatedAt <= 0 || nowUnix < m_iUpdatedAt)
		{
			m_iUpdatedAt = nowUnix;
			return;
		}
		int t = m_iUpdatedAt;
		int slices = 0;
		while (t < nowUnix && slices < SLICES_MAX)
		{
			int next = nowUnix;
			if (slices < SLICES_MAX - 1)
			{
				foreach (SRP_CmdRegionSupply supply : m_aRegions)
				{
					if (supply.m_iCutUntil > t && supply.m_iCutUntil < next)
						next = supply.m_iCutUntil;
				}
			}
			ApplyRevenue((next - t) / 3600.0, t);
			t = next;
			slices++;
		}
		m_iUpdatedAt = nowUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones rouges par région (SRP_Commander.GetBook().GetRegionOfZone, GetZoneOwner) — Tick, CanReserve
	protected void RefreshRedCache()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		SRP_CmdRegionBook book = Book();
		if (!front || !book || m_aRegions.IsEmpty())
			return;
		int zoneCount = front.GetZoneCount();
		if (zoneCount <= 1)
			return;		// géométrie pas encore construite : le dernier comptage reste
		array<int> counts = {};
		for (int k = 0; k < m_aRegions.Count(); k++)
			counts.Insert(0);
		for (int zone = 0; zone < zoneCount; zone++)
		{
			if (front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE)
				continue;
			int region = book.GetRegionOfZone(zone);
			if (region < 0 || region >= counts.Count())
				continue;
			counts[region] = counts[region] + 1;
		}
		foreach (int i, SRP_CmdRegionSupply supply : m_aRegions)
		{
			if (supply.m_iRedCache != counts[i])
				m_bChanged = true;
			supply.m_iRedCache = counts[i];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Parts entières des res_obus_plein au prorata des zones de départ (plus forts restes) — Start
	protected void ComputeShellShares()
	{
		m_iRefTotal = 0;
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
			m_iRefTotal += supply.m_iRefZones;
		if (m_iRefTotal <= 0)
		{
			foreach (SRP_CmdRegionSupply empty : m_aRegions)
				empty.m_iShellShare = 0;
			return;
		}
		array<float> rests = {};
		int given = 0;
		foreach (SRP_CmdRegionSupply share : m_aRegions)
		{
			float exact = m_iShellsFull * 1.0 * share.m_iRefZones / m_iRefTotal;
			int whole = Math.Floor(exact);
			share.m_iShellShare = whole;
			rests.Insert(exact - whole);
			given += whole;
		}
		// Plus forts restes : une unité de plus aux régions les mieux placées jusqu'au plein de l'île (ordre du tracé
		// en cas d'égalité : déterministe)
		while (given < m_iShellsFull)
		{
			int best = -1;
			float bestRest = -1;
			foreach (int j, float rest : rests)
			{
				if (rest > bestRest)
				{
					bestRest = rest;
					best = j;
				}
			}
			if (best < 0)
				break;
			m_aRegions[best].m_iShellShare = m_aRegions[best].m_iShellShare + 1;
			rests[best] = -1;
			given++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! RE2 : région vivante, organisée, à sec (sous res_renflouement_seuil_obus) -> RequestResupply — Tick
	protected void CheckResupply(int nowUnix)
	{
		SRP_CmdRegionBook book = Book();
		if (!book || m_iResupplyThreshold <= 0)
			return;
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			if (supply.m_iRedCache <= 0)
				continue;
			if (!book.IsRegionAlive(supply.m_iRegion) || book.IsRegionDisorganized(supply.m_iRegion))
				continue;
			if (Math.Floor(supply.m_fShells) >= m_iResupplyThreshold)
				continue;
			if (HasTransferTo(supply.m_iRegion))
				continue;
			// déjà au niveau visé (petite part) : rien à demander
			if (m_iResupplyCent / 100.0 * ShellsFull(supply) - supply.m_fShells < 1)
				continue;
			string refusal = DoResupply(supply.m_iRegion, nowUnix, "");
			if (!refusal.IsEmpty())
				SRP_CmdLog.Refusal(supply.m_iRegion, "renflouement de la " + RegionLabel(supply.m_iRegion), refusal);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Arrivée des renflouements (obus bornés au plein ; receveur sans zone rouge : rendus au donneur) — Tick
	protected void CheckTransfers(int nowUnix)
	{
		for (int i = m_aTransfers.Count() - 1; i >= 0; i--)
		{
			SRP_CmdTransfer transfer = m_aTransfers[i];
			if (!transfer)
			{
				m_aTransfers.Remove(i);
				continue;
			}
			if (transfer.m_iArriveAt > nowUnix)
				continue;
			int fromRegion = transfer.m_iFrom;
			int toRegion = transfer.m_iTo;
			int amount = transfer.m_iAmount;
			m_aTransfers.Remove(i);

			SRP_CmdRegionSupply target = GetSupply(toRegion);
			float kept = 0;
			if (target && target.m_iRedCache > 0)
				kept = AddShells(target, amount);
			float back = 0;
			if (amount - kept > 0)
				back = AddShells(GetSupply(fromRegion), amount - kept);
			SRP_CmdLog.Note(toRegion, SRP_ECmdLogKind.STOCK, string.Format("Renflouement arrivé dans la %1 : %2 obus reçus, %3 rendus à la %4, reste %5", RegionLabel(toRegion), DecText(kept), DecText(back), RegionLabel(fromRegion), StockText(SRP_ECmdStock.OBUS, toRegion)));
			m_bChanged = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! CA7 : le nombre de joueurs passe de plus de 0 à 0 -> m_fLevelAtLastLeave = GetStockLevel(), MarkDirty(false) —
	//! Tick
	protected void CheckLastLeave(int nowUnix)
	{
		int players = GetGame().GetPlayerManager().GetPlayerCount();
		if (m_iLastPlayerCount > 0 && players == 0)
		{
			m_fLevelAtLastLeave = GetStockLevel();
			m_iLastLeaveAt = nowUnix;
			m_bChanged = true;
			int cent = Math.Round(m_fLevelAtLastLeave * 100);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.STOCK, string.Format("Dernier joueur parti : niveau des stocks noté à %1 centièmes (offensive de la nuit)", cent));
		}
		m_iLastPlayerCount = players;
	}

	//------------------------------------------------------------------------------------------------
	//! Plein d'un stock pour une menace donnée (region = -1 et OBUS : somme des régions) — GetFull, ResetCampaign
	protected float FullAt(int kind, int region, int threat)
	{
		float bonus = 1 + m_iThreatBonusCent / 100.0 * threat;
		if (kind == SRP_ECmdStock.OBUS)
		{
			if (region < 0)
			{
				float total = 0;
				foreach (SRP_CmdRegionSupply entry : m_aRegions)
				{
					if (entry.m_iRedCache > 0)
						total += entry.m_iShellShare * bonus;
				}
				return total;
			}
			SRP_CmdRegionSupply supply = GetSupply(region);
			if (!supply || supply.m_iRedCache <= 0)
				return 0;
			return supply.m_iShellShare * bonus;
		}
		if (kind == SRP_ECmdStock.BLINDE)
			return m_iArmorFull * bonus;
		if (kind == SRP_ECmdStock.HELICO)
			return m_iHeliFull * bonus;
		if (kind == SRP_ECmdStock.ARTILLERIE)
			return m_iArtilleryFull * bonus;
		if (kind == SRP_ECmdStock.BOMBE)
		{
			if (m_bBombThreatBonus)
				return m_iBombFull * bonus;
			return m_iBombFull;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Plein d'obus d'une région à la menace actuelle (0 sans zone rouge)
	protected float ShellsFull(SRP_CmdRegionSupply supply)
	{
		if (!supply)
			return 0;
		return FullAt(SRP_ECmdStock.OBUS, supply.m_iRegion, Threat());
	}

	//------------------------------------------------------------------------------------------------
	//! Remplissage d'un stock (0 à 1), -1 si son plein est nul (hors moyenne)
	protected float Ratio(int kind, int region)
	{
		float full = GetFull(kind, region);
		if (full <= 0)
			return -1;
		return Math.Clamp(GetStock(kind, region) / full, 0, 1);
	}

	//------------------------------------------------------------------------------------------------
	//! CA7 : niveau noté au départ du dernier joueur, sinon le niveau actuel (jamais noté)
	protected float NotedLevel()
	{
		if (m_fLevelAtLastLeave >= 0)
			return m_fLevelAtLastLeave;
		return GetStockLevel();
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit un stock (jamais négatif) ; OBUS : région valide seulement
	protected void SetStockValue(int kind, int region, float value)
	{
		float stored = value;
		if (stored < 0)
			stored = 0;
		if (kind == SRP_ECmdStock.OBUS)
		{
			SRP_CmdRegionSupply supply = GetSupply(region);
			if (supply)
				supply.m_fShells = stored;
			return;
		}
		if (kind == SRP_ECmdStock.BLINDE)
			m_fArmor = stored;
		else if (kind == SRP_ECmdStock.HELICO)
			m_fHeli = stored;
		else if (kind == SRP_ECmdStock.ARTILLERIE)
			m_fArtillery = stored;
		else if (kind == SRP_ECmdStock.BOMBE)
			m_fBomb = stored;
	}

	//------------------------------------------------------------------------------------------------
	//! Rend des unités à un stock, borné au plein ; rend ce qui a vraiment été ajouté
	protected float AddToStock(int kind, int region, float amount)
	{
		if (kind == SRP_ECmdStock.OBUS)
			return AddShells(GetSupply(region), amount);
		if (kind < SRP_ECmdStock.OBUS || kind > SRP_ECmdStock.BOMBE || amount <= 0)
			return 0;
		float full = GetFull(kind, -1);
		float current = GetStock(kind, -1);
		float room = full - current;
		if (room <= 0)
			return 0;
		float added = Math.Min(room, amount);
		SetStockValue(kind, -1, current + added);
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Rend des obus à une région, bornés à son plein (0 sans zone rouge) ; rend ce qui a vraiment été ajouté
	protected float AddShells(SRP_CmdRegionSupply supply, float amount)
	{
		if (!supply || amount <= 0)
			return 0;
		float room = ShellsFull(supply) - supply.m_fShells;
		if (room <= 0)
			return 0;
		float added = Math.Min(room, amount);
		supply.m_fShells = supply.m_fShells + added;
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les stocks à leur plein actuel
	protected void FillAll()
	{
		m_fArmor = GetFull(SRP_ECmdStock.BLINDE, -1);
		m_fHeli = GetFull(SRP_ECmdStock.HELICO, -1);
		m_fArtillery = GetFull(SRP_ECmdStock.ARTILLERIE, -1);
		m_fBomb = GetFull(SRP_ECmdStock.BOMBE, -1);
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
			supply.m_fShells = ShellsFull(supply);
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque stock borné à son plein actuel (menace, réglages)
	protected void CapToFull()
	{
		float armorFull = GetFull(SRP_ECmdStock.BLINDE, -1);
		if (m_fArmor > armorFull)
			m_fArmor = armorFull;
		float heliFull = GetFull(SRP_ECmdStock.HELICO, -1);
		if (m_fHeli > heliFull)
			m_fHeli = heliFull;
		float artilleryFull = GetFull(SRP_ECmdStock.ARTILLERIE, -1);
		if (m_fArtillery > artilleryFull)
			m_fArtillery = artilleryFull;
		float bombFull = GetFull(SRP_ECmdStock.BOMBE, -1);
		if (m_fBomb > bombFull)
			m_fBomb = bombFull;
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			float shellsFull = ShellsFull(supply);
			if (supply.m_fShells > shellsFull)
				supply.m_fShells = shellsFull;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Une part par région du tracé (index = région), codes et zones de départ relus, parts d'obus refaites ; les
	//! valeurs des parts existantes sont gardées
	protected void BuildSupplies()
	{
		SRP_CmdRegionBook book = Book();
		int count = 0;
		if (book)
			count = book.GetRegionCount();
		while (m_aRegions.Count() > count)
			m_aRegions.Remove(m_aRegions.Count() - 1);
		while (m_aRegions.Count() < count)
		{
			SRP_CmdRegionSupply added = new SRP_CmdRegionSupply();
			m_aRegions.Insert(added);
		}
		foreach (int i, SRP_CmdRegionSupply supply : m_aRegions)
		{
			supply.m_iRegion = i;
			supply.m_sCode = book.GetRegionCode(i);
			supply.m_iRefZones = book.GetRegionStartZones(i);
		}
		ComputeShellShares();
	}

	//------------------------------------------------------------------------------------------------
	//! Renflouement vers la région (sans seuil) : donneur vivant et organisé qui a le plus d'obus au-dessus de ce qu'il
	//! garde ; "" si un renflouement part, sinon la raison
	protected string DoResupply(int region, int nowUnix, string author)
	{
		SRP_CmdRegionSupply target = GetSupply(region);
		if (!target)
			return "région inconnue";
		if (target.m_iRedCache <= 0)
			return "plus aucune zone rouge";
		if (HasTransferTo(region))
			return "un renflouement est déjà en route";
		float need = m_iResupplyCent / 100.0 * ShellsFull(target) - target.m_fShells;
		if (need < 1)
			return string.Format("déjà à %1 sur %2 obus", DecText(target.m_fShells), DecText(ShellsFull(target)));

		SRP_CmdRegionBook book = Book();
		SRP_CmdRegionSupply donor = null;
		float donorSpare = 0;
		foreach (SRP_CmdRegionSupply other : m_aRegions)
		{
			if (other == target || other.m_iRedCache <= 0)
				continue;
			if (book && (!book.IsRegionAlive(other.m_iRegion) || book.IsRegionDisorganized(other.m_iRegion)))
				continue;
			float spare = other.m_fShells - m_iDonorKeepCent / 100.0 * ShellsFull(other);
			if (spare > donorSpare)
			{
				donorSpare = spare;
				donor = other;
			}
		}
		if (!donor || donorSpare < 1)
			return "aucune région ne peut donner d'obus";
		int amount = Math.Floor(Math.Min(need, donorSpare));
		if (amount < 1)
			return "aucune région ne peut donner d'obus";

		donor.m_fShells = donor.m_fShells - amount;
		SRP_CmdTransfer transfer = new SRP_CmdTransfer();
		transfer.m_iFrom = donor.m_iRegion;
		transfer.m_iTo = region;
		transfer.m_iAmount = amount;
		transfer.m_iArriveAt = nowUnix + m_iResupplyMinutes * 60;
		m_aTransfers.Insert(transfer);
		m_bChanged = true;
		string by = "";
		if (!author.IsEmpty())
			by = " (demandé par " + author + ")";
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.STOCK, string.Format("Renflouement : %1 obus de la %2 vers la %3, arrivée dans %4 min%5", amount, RegionLabel(donor.m_iRegion), RegionLabel(region), m_iResupplyMinutes, by));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Renflouements en route vers une région qui n'a plus de zone rouge : rendus tout de suite au donneur
	protected void ReturnTransfersTo(int region)
	{
		for (int i = m_aTransfers.Count() - 1; i >= 0; i--)
		{
			SRP_CmdTransfer transfer = m_aTransfers[i];
			if (!transfer || transfer.m_iTo != region)
				continue;
			int fromRegion = transfer.m_iFrom;
			int amount = transfer.m_iAmount;
			m_aTransfers.Remove(i);
			float back = AddShells(GetSupply(fromRegion), amount);
			SRP_CmdLog.Note(fromRegion, SRP_ECmdLogKind.STOCK, string.Format("Renflouement vers la %1 annulé : %2 obus rendus à la %3", RegionLabel(region), DecText(back), RegionLabel(fromRegion)));
			m_bChanged = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un renflouement est-il en route vers la région ?
	protected bool HasTransferTo(int region)
	{
		foreach (SRP_CmdTransfer transfer : m_aTransfers)
		{
			if (transfer && transfer.m_iTo == region)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Stock visé par une mission SABOTAGE d'après le nom de sa cible (RE8)
	protected int SabotageKind(string prefabName)
	{
		if (prefabName.Contains("Mortier"))
			return SRP_ECmdStock.OBUS;
		if (prefabName.Contains("Antichar"))
			return SRP_ECmdStock.BLINDE;
		if (prefabName.Contains("_AA") || prefabName.Contains("Radar"))
			return SRP_ECmdStock.HELICO;
		if (prefabName.Contains("Transmissions") || prefabName.Contains("Generateur"))
			return SRP_ECmdStock.ARTILLERIE;
		return SRP_ECmdStock.OBUS;
	}

	//------------------------------------------------------------------------------------------------
	//! « Blindés : 1,6 sur 2, prochain dans 48 min »
	protected string IslandLine(string label, int kind)
	{
		string line = string.Format("%1 : %2 sur %3", label, DecText(GetStock(kind, -1)), DecText(GetFull(kind, -1)));
		int minutes = MinutesToNextUnit(kind, -1);
		if (minutes > 0)
			line = line + ", prochain dans " + DurationWords(minutes * 60);
		return line;
	}

	//------------------------------------------------------------------------------------------------
	//! « 48/48 obus, 2/2 blindés, … » (journal)
	protected string SummaryText()
	{
		string text = StockText(SRP_ECmdStock.OBUS, -1);
		text = text + ", " + StockText(SRP_ECmdStock.BLINDE, -1);
		text = text + ", " + StockText(SRP_ECmdStock.HELICO, -1);
		text = text + ", " + StockText(SRP_ECmdStock.ARTILLERIE, -1);
		text = text + ", " + StockText(SRP_ECmdStock.BOMBE, -1);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom pluriel d'un stock (« 1/2 blindés »)
	protected static string KindPlural(int kind)
	{
		if (kind == SRP_ECmdStock.OBUS)
			return "obus";
		if (kind == SRP_ECmdStock.BLINDE)
			return "blindés";
		if (kind == SRP_ECmdStock.HELICO)
			return "sorties d'hélico";
		if (kind == SRP_ECmdStock.ARTILLERIE)
			return "tirs d'artillerie lourde";
		if (kind == SRP_ECmdStock.BOMBE)
			return "bombes";
		return "stock inconnu";
	}

	//------------------------------------------------------------------------------------------------
	//! Heures pour refaire la bombe : appui_bombe_intervalle_h des appuis (24 sans appuis, 0 lu comme 0,1 h)
	protected float BombHours()
	{
		float hours = 24;
		SRP_CmdSupport support = SRP_CmdSupport.Get();
		if (support)
			hours = support.m_iBombIntervalH;
		if (hours <= 0)
			hours = 0.1;
		return hours;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones de départ de l'île (res_zones_depart, sinon la somme du tracé)
	protected int IslandRefZones()
	{
		if (m_iRefZonesOverride > 0)
			return m_iRefZonesOverride;
		return m_iRefTotal;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdRegionSupply GetSupply(int region)
	{
		if (region < 0 || region >= m_aRegions.Count())
			return null;
		return m_aRegions[region];
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdRegionSupply FindSupplyByCode(string code)
	{
		if (code.IsEmpty())
			return null;
		foreach (SRP_CmdRegionSupply supply : m_aRegions)
		{
			if (supply.m_sCode == code)
				return supply;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Région d'un code relu (« R3 »), -1 si vide ou inconnu (moyens de l'île)
	protected int RegionOfCode(string code)
	{
		SRP_CmdRegionSupply supply = FindSupplyByCode(code);
		if (!supply)
			return -1;
		return supply.m_iRegion;
	}

	//------------------------------------------------------------------------------------------------
	//! Code d'une région pour la sauvegarde, "" pour l'île
	protected string CodeOf(int region)
	{
		SRP_CmdRegionSupply supply = GetSupply(region);
		if (!supply)
			return "";
		return supply.m_sCode;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdTicket FindTicket(int id)
	{
		if (id <= 0)
			return null;
		foreach (SRP_CmdTicket ticket : m_aTickets)
		{
			if (ticket && ticket.m_iId == id)
				return ticket;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Région d'une ligne de journal : celle des obus, -1 (l'île) pour les autres stocks
	protected int LogRegion(int kind, int region)
	{
		if (kind == SRP_ECmdStock.OBUS && GetSupply(region))
			return region;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe » (repli : « région R3 »)
	protected string RegionLabel(int region)
	{
		SRP_CmdRegionBook book = Book();
		if (book && region >= 0 && region < book.GetRegionCount())
			return book.GetRegionLabel(region);
		return "région " + CodeOf(region);
	}

	//------------------------------------------------------------------------------------------------
	protected static string AuthorText(string author)
	{
		if (author.IsEmpty())
			return "le Commandeur";
		return author;
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_CmdRegionBook Book()
	{
		if (!m_Commander)
			return null;
		return m_Commander.GetBook();
	}

	//------------------------------------------------------------------------------------------------
	protected int BookRegionCount()
	{
		SRP_CmdRegionBook book = Book();
		if (!book)
			return m_aRegions.Count();
		return book.GetRegionCount();
	}

	//------------------------------------------------------------------------------------------------
	protected int Threat()
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return 0;
		return front.GetThreat();
	}

	//------------------------------------------------------------------------------------------------
	protected void Dirty(bool immediate)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(immediate);
	}
}
