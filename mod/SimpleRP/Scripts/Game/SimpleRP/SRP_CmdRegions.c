//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) : RÉGIONS ET OFFICIERS.
//
// RÔLE (serveur ; SRP_CmdRegionBook est tenu par SRP_Commander.m_Book) :
// - tracé déterministe des régions (OF1 : cmd_regions = 6, de cmd_region_min_zones à cmd_region_max_zones zones),
//   retouches « region S12 -> R3 » de front_retouches.txt (SRP_FrontComponent.RETOUCH_PATH ; le socle ignore ces
//   lignes), nom de la plus grande localité (VU1), rapport $profile:SimpleRP/commandeur_regions.txt ;
// - rattachement d'une zone reprise quand sa région est libérée, libération d'une région toute bleue (OF11, par
//   CheckLiberation, appelée par SRP_Commander.OnZoneCaptured seul ; rien de visible pour une zone forcée, Q9) ;
// - alerte de région CO9 (retombe en alerte_retombee_min) et menace LOCALE (ThreatAtZone) ;
// - officiers (OF2 à OF10) : nom et caractère tirés, numéro unique m_iSerial (jamais réutilisé, il sert au
//   renseignement VU2), cachette du jour (change à officier_changement_heure), pose hors de vue à
//   officier_apparition_m d'un joueur, retrait, fuite, évasion, chute (tué, capturé par les menottes ACE, laissé
//   pour mort), désorganisation de desorg_min (x desorg_capture_facteur si capturé), remplacement.
// C7 : si les menottes ACE ne marchent pas sur un officier IA inconscient, AUCUNE action de remplacement : il ne peut
// alors qu'être tué (3 h).
// Textes publics : SRP_FrontRadio.OfficerFell et RegionLiberated SEULEMENT ; journal : SRP_CmdLog.
// La part de moyens d'une région (obus, dépôt) est tenue par SRP_CmdResources (SRP_CmdRegionSupply), pas ici.
// Heures Unix pour tout ce qui est sauvé ; officiers posés : classe GARDE (SRP_CmdCapacity.Tag), rôle OFFICIER et
// GARDE_OFFICIER (exclus de l'entraide).
// Tracé (principe de regions.py) : graines éloignées (hors zones sans voisine), croissance équilibrée (la région la
// plus petite prend la zone libre voisine la plus proche de sa graine), zones jamais atteintes à la graine la plus
// proche, équilibrage entre les bornes sans jamais couper une région, R1 la plus proche de la base.
// APPELÉ PAR : SRP_Commander (vie, cycles, sauvegarde), SRP_CmdScreens (Staff), SRP_Missions (OF10 par
// SRP_Commander.Get().GetBook()), SRP_CmdResources (zones de départ), SRP_FrontRadio (noms).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Un officier de région (OF2 à OF10). Champs « sauvé » : écrits par SRP_CmdRegionBook.WriteTo (clés cmd_rN_of_*).
class SRP_CmdOfficer
{
	int m_iSerial;								// numéro unique, jamais réutilisé (sauvé)
	string m_sName;								// « Volkov » (sauvé)
	int m_iTrait = SRP_ECmdTrait.METHODIQUE;	// sauvé
	int m_iState = SRP_ECmdOfficerState.VIVANT;	// sauvé
	int m_iSinceUnix;							// arrivée (sauvé)
	int m_iDownUnix;							// chute (sauvé)
	int m_iHideDay = -1;						// numéro de jour de la cachette (DayIndex, sauvé)
	string m_sHideLocality;						// cachette du jour (sauvé) : nom de localité, ou code de zone sans localité
	string m_sPrevLocality;						// cachette précédente (sauvé)
	int m_iEscapedUnix;							// évasion (mission OFFICIER échouée, sauvé)
	// En jeu, non sauvé
	ref array<string> m_aHideChoices = {};		// cachettes candidates du jour (Staff : « choisie parmi … »)
	vector m_vHidePos;
	ref array<ref SRP_EnemyGroup> m_aGroups = {};	// case 0 : l'officier ; case 1 : ses gardes (toujours des ref)
	IEntity m_Entity;							// l'officier posé
	int m_iNoPlayerSince;
	int m_iUnconsciousSince;					// inconscient et seul depuis (0 = conscient)
	int m_iCaptiveIdleSince;
	int m_iDeferUntil;
	bool m_bFleeing;
	vector m_vFleeTo;
	string m_sFleeLocality;
	bool m_bWeDelete;							// retrait par nous : pas une chute
	int m_iPostLogUnix;							// dernière pose refusée écrite au journal (une ligne par 10 min au plus)
}

//------------------------------------------------------------------------------------------------
//! Une région (OF1). Champs « sauvé » : clés cmd_rN_* retrouvées par le CODE de la région.
class SRP_CmdRegion
{
	int m_iIndex;
	string m_sCode;								// « R1 » (le plus proche de la base) à « R6 »
	string m_sName;								// plus grande localité (VU1)
	int m_iSeedZone = -1;
	vector m_vCentroid;
	ref array<int> m_aZones = {};				// zones rattachées, retouches et rattachements OF11 compris
	ref array<int> m_aHomeZones = {};			// tracé d'origine (zones de départ, RE2)
	int m_iState = SRP_ECmdRegionState.ACTIVE;	// sauvé
	int m_iDesorgUntil;							// heure Unix (sauvé)
	bool m_bDesorgCaptured;						// sauvé
	int m_iLiberatedAt;							// sauvé
	float m_fAlert;								// CO9, lue par CurrentAlert (sauvé)
	int m_iAlertAt;								// sauvé
	int m_iAlertStep;							// dernier palier de 20 écrit au journal
	ref SRP_CmdOfficer m_Officer;				// null = pas encore d'officier (sauvé)
	int m_iPrevTrait = -1;						// caractère du prédécesseur (sauvé)
	// Non sauvé
	int m_iMood = SRP_ECmdMood.NORMALE;			// recalculée par le cerveau (CO7)
	int m_iReportsLost;							// OF6 (Staff)
	int m_iReportsDelayed;						// OF3 (Staff)
	int m_iLastReportUnix;
	string m_sRadioCutLocality;					// « radio de Durras coupée encore 3 min » (Staff)
	int m_iRadioCutUntil;
	int m_iLastFallUnix;						// dernière chute d'officier (mission OFFICIER d'un officier remplacé)
	int m_iLastFallStatus = SRP_ECmdMissionStatus.SANS_OBJET;	// TOMBE ou CAPTURE
}

//------------------------------------------------------------------------------------------------
//! Le registre des régions et des officiers (tenu par SRP_Commander.m_Book), serveur.
class SRP_CmdRegionBook
{
	static const string REPORT_PATH = "$profile:SimpleRP/commandeur_regions.txt";
	protected static const int HASH_MOD = 8388593;			// somme de contrôle, comme m_iGeomHash du socle
	protected static const int BALANCE_PASSES = 300;		// OF1 : passes d'équilibrage au plus
	protected static const int ALERT_STEP = 20;				// CO9 : palier écrit au journal
	protected static const int RETRY_S = 30;				// OF2 : pose refusée, nouvel essai
	protected static const int POST_LOG_S = 600;			// OF2 : une ligne de refus de pose par 10 min au plus
	protected static const int ESCAPE_CLEAR_M = 800;		// OF10 : évasion sans joueur à cette distance
	protected static const float FLEE_FALLBACK_M = 600;		// OF10 : fuite sans autre cachette, à l'opposé du joueur
	protected static const float ARRIVED_M = 25;			// OF10 : fuyard arrivé
	protected static const float HIDE_ZONE_RADIUS = 400;	// OF2 : cachette sans localité, bâtiment près du centre
	protected static const int DEPTH_NONE = 1000;			// profondeur d'une zone sans bleu atteignable

	// --- Réglages (clés cmd_regions, cmd_region_*, alerte_*, officier_*, desorg_*) -------------------------------
	int m_iRegionsWanted = 6;		// cmd_regions
	int m_iRegionMinZones = 8;		// cmd_region_min_zones
	int m_iRegionMaxZones = 13;		// cmd_region_max_zones
	int m_iAlertMax = 100;		// alerte_max
	int m_iAlertDecayMin = 180;		// alerte_retombee_min
	int m_iAlertSeen = 2;		// alerte_vu
	int m_iAlertHeard = 1;		// alerte_entendu
	int m_iAlertShotsPer = 25;		// alerte_tirs_par
	int m_iAlertLoss = 2;		// alerte_perte
	int m_iAlertCell = 4;		// alerte_carre
	int m_iAlertZone = 15;		// alerte_zone
	int m_iAlertOfficer = 30;		// alerte_officier
	int m_iAlertSabotage = 15;		// alerte_sabotage
	int m_iAlertInformant = 2;		// alerte_informateur
	int m_iAlertVehicle = 3;		// alerte_vehicule
	int m_iAlertThreatPer = 20;		// alerte_menace_par
	int m_iAlertThreatMax = 5;		// alerte_menace_max
	int m_iOfficerGuards = 2;		// officier_gardes
	int m_iOfficerHideouts = 3;		// officier_cachettes
	int m_iOfficerDayHour = 6;		// officier_changement_heure
	int m_iOfficerSpawnM = 1500;		// officier_apparition_m
	int m_iOfficerDespawnM = 2500;		// officier_retrait_m
	int m_iOfficerDespawnMin = 5;		// officier_retrait_min
	int m_iOfficerAiMargin = 10;		// officier_marge_ia
	int m_iOfficerFleeM = 250;		// officier_fuite_m
	int m_iOfficerEscapeM = 1500;		// officier_echappe_m
	int m_iOfficerAbandonM = 300;		// officier_abandon_m
	int m_iOfficerUnconsciousMin = 10;		// officier_inconscient_min
	int m_iOfficerCaptiveRetireMin = 10;		// officier_captif_retrait_min
	string m_sOfficerPrefab = "";		// officier_prefab
	int m_iOfficerGarrisonBonusCent = 25;		// officier_bonus_garnison
	string m_sOfficerNames = "Volkov, Orlov, Sokolov, Morozov, Lebedev, Kozlov, Novikov, Pavlov, Smirnov, Fedorov, Belov, Zaitsev, Karpov, Gromov, Titov, Zhukov, Tarasov, Kuznetsov, Popov, Vasiliev, Mikhailov, Frolov, Yegorov, Nikitin, Makarov, Andreev, Kovalev, Ilyin, Gusev, Baranov";		// officier_noms
	int m_iOfficerNamesMemory = 12;		// officier_noms_memoire
	int m_iDesorgMin = 180;		// desorg_min
	int m_iDesorgCaptureFactor = 2;		// desorg_capture_facteur

	// --- État (serveur) ------------------------------------------------------------------------------------------
	protected SRP_Commander m_Commander;
	protected ref array<ref SRP_CmdRegion> m_aRegions = {};
	protected ref array<int> m_aZoneRegion = {};		// zone -> région (-1 : base ou sans région)
	protected ref array<int> m_aHomeRegion = {};		// zone -> région du tracé d'origine
	protected ref array<string> m_aUsedNames = {};		// officier_noms_memoire derniers noms (sauvé)
	protected ref array<string> m_aReport = {};			// lignes de commandeur_regions.txt
	protected int m_iNextSerial = 1;					// sauvé
	protected int m_iHash;								// somme de contrôle du tracé
	protected int m_iTzOffset;							// heure locale - heure UTC, en secondes

	// Graphe des zones (géométrie fixe du socle), à plat : voisines de z = m_aAdj[m_aAdjStart[z] .. m_aAdjStart[z + 1] - 1]
	protected ref array<int> m_aAdjStart = {};
	protected ref array<int> m_aAdj = {};				// voisines par un côté ou par la liaison maritime (base comprise)
	protected ref array<vector> m_aZonePos = {};		// centre de chaque zone
	protected ref array<int> m_aZoneCells = {};		// carrés de chaque zone
	protected ref array<string> m_aTraceNotes = {};	// retouches et avertissements du tracé (rapport)
	protected ref array<string> m_aReadNotes = {};		// avertissements de la relecture de front.json (rapport)

	//================================================================================================
	// Vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Constructeur seul (aucune lecture) — SRP_Commander (constructeur)
	void SRP_CmdRegionBook(SRP_Commander commander)
	{
		m_Commander = commander;
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare ses clés (défaut = valeur du champ) — SRP_Commander.DeclareSettings
	void DeclareSettings()
	{
		SRP_CmdSettings.DeclareInt("cmd_regions", m_iRegionsWanted, "Nombre de régions et d'officiers (pris en compte au redémarrage) (OF1)");
		SRP_CmdSettings.DeclareInt("cmd_region_min_zones", m_iRegionMinZones, "Zones au moins par région (pris en compte au redémarrage) (OF1)");
		SRP_CmdSettings.DeclareInt("cmd_region_max_zones", m_iRegionMaxZones, "Zones au plus par région (pris en compte au redémarrage) (OF1)");
		SRP_CmdSettings.DeclareInt("alerte_max", m_iAlertMax, "Alerte de région maximale (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_retombee_min", m_iAlertDecayMin, "L'alerte retombe de son maximum à 0 en ce nombre de minutes (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_vu", m_iAlertSeen, "Alerte : joueur vu (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_entendu", m_iAlertHeard, "Alerte : joueur entendu (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_tirs_par", m_iAlertShotsPer, "Alerte : plus 1 par tranche de ce nombre de tirs (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_perte", m_iAlertLoss, "Alerte : soldat perdu (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_carre", m_iAlertCell, "Alerte : carré perdu (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_zone", m_iAlertZone, "Alerte : zone perdue (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_officier", m_iAlertOfficer, "Alerte : officier tombé (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_sabotage", m_iAlertSabotage, "Alerte : sabotage ou mission réussie (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_informateur", m_iAlertInformant, "Alerte : signalement d'un civil (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_vehicule", m_iAlertVehicle, "Alerte : véhicule armé vu (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_menace_par", m_iAlertThreatPer, "Plus 1 de menace locale par tranche de ce nombre de points d'alerte (CO9)");
		SRP_CmdSettings.DeclareInt("alerte_menace_max", m_iAlertThreatMax, "Menace locale ajoutée au plus (CO9)");
		SRP_CmdSettings.DeclareInt("officier_gardes", m_iOfficerGuards, "Gardes de l'officier (OF2)");
		SRP_CmdSettings.DeclareInt("officier_cachettes", m_iOfficerHideouts, "Cachettes possibles, tirées chaque jour (OF2)");
		SRP_CmdSettings.DeclareInt("officier_changement_heure", m_iOfficerDayHour, "Heure du serveur à laquelle l'officier change de cachette (OF2)");
		SRP_CmdSettings.DeclareInt("officier_apparition_m", m_iOfficerSpawnM, "L'officier est posé quand un joueur est à cette distance de sa cachette, en mètres (OF2)");
		SRP_CmdSettings.DeclareInt("officier_retrait_m", m_iOfficerDespawnM, "Retrait quand plus aucun joueur n'est à cette distance, en mètres (OF2)");
		SRP_CmdSettings.DeclareInt("officier_retrait_min", m_iOfficerDespawnMin, "Retrait après ce nombre de minutes sans joueur (OF2)");
		SRP_CmdSettings.DeclareInt("officier_marge_ia", m_iOfficerAiMargin, "Places du jeu gardées libres pour poser l'officier (OF2)");
		SRP_CmdSettings.DeclareInt("officier_fuite_m", m_iOfficerFleeM, "L'officier fuit quand un joueur est à cette distance, en mètres (OF10)");
		SRP_CmdSettings.DeclareInt("officier_echappe_m", m_iOfficerEscapeM, "Il s'échappe à cette distance de sa cachette, hors de vue, en mètres (OF10)");
		SRP_CmdSettings.DeclareInt("officier_abandon_m", m_iOfficerAbandonM, "Officier inconscient laissé pour mort si personne n'est à cette distance, en mètres (OF9)");
		SRP_CmdSettings.DeclareInt("officier_inconscient_min", m_iOfficerUnconsciousMin, "Pendant ce nombre de minutes (OF9)");
		SRP_CmdSettings.DeclareInt("officier_captif_retrait_min", m_iOfficerCaptiveRetireMin, "Officier capturé retiré après ce nombre de minutes sans joueur à 300 m (OF9)");
		SRP_CmdSettings.DeclareString("officier_prefab", m_sOfficerPrefab, "Prefab de l'officier ; vide = la liste des officiers de mission (OF2)");
		SRP_CmdSettings.DeclareInt("officier_bonus_garnison", m_iOfficerGarrisonBonusCent, "Garnisons de la région plus fournies de ce nombre de centièmes tant que l'officier vit (OF5)");
		SRP_CmdSettings.DeclareString("officier_noms", m_sOfficerNames, "Noms des officiers ennemis, séparés par des virgules (VU1)");
		SRP_CmdSettings.DeclareInt("officier_noms_memoire", m_iOfficerNamesMemory, "Un nom n'est pas repris avant ce nombre d'officiers (VU1)");
		SRP_CmdSettings.DeclareInt("desorg_min", m_iDesorgMin, "Désorganisation après la chute d'un officier, en minutes (redémarrages compris) (OF7)");
		SRP_CmdSettings.DeclareInt("desorg_capture_facteur", m_iDesorgCaptureFactor, "Désorganisation multipliée par ce nombre si l'officier est capturé (OF9)");
	}

	//------------------------------------------------------------------------------------------------
	//! Relit ses clés (cmd_regions et les bornes ne servent qu'au prochain tracé, donc au redémarrage) —
	//! SRP_Commander.LoadSettings
	void LoadSettings()
	{
		m_iRegionsWanted = SRP_CmdSettings.GetIntClamped("cmd_regions", 1, 12);
		m_iRegionMinZones = SRP_CmdSettings.GetIntClamped("cmd_region_min_zones", 1, 60);
		m_iRegionMaxZones = SRP_CmdSettings.GetIntClamped("cmd_region_max_zones", 1, 60);
		m_iAlertMax = SRP_CmdSettings.GetIntClamped("alerte_max", 1, 1000);
		m_iAlertDecayMin = SRP_CmdSettings.GetIntClamped("alerte_retombee_min", 1, 1440);
		m_iAlertSeen = SRP_CmdSettings.GetIntClamped("alerte_vu", 0, 100);
		m_iAlertHeard = SRP_CmdSettings.GetIntClamped("alerte_entendu", 0, 100);
		m_iAlertShotsPer = SRP_CmdSettings.GetIntClamped("alerte_tirs_par", 1, 1000);
		m_iAlertLoss = SRP_CmdSettings.GetIntClamped("alerte_perte", 0, 100);
		m_iAlertCell = SRP_CmdSettings.GetIntClamped("alerte_carre", 0, 100);
		m_iAlertZone = SRP_CmdSettings.GetIntClamped("alerte_zone", 0, 100);
		m_iAlertOfficer = SRP_CmdSettings.GetIntClamped("alerte_officier", 0, 1000);
		m_iAlertSabotage = SRP_CmdSettings.GetIntClamped("alerte_sabotage", 0, 100);
		m_iAlertInformant = SRP_CmdSettings.GetIntClamped("alerte_informateur", 0, 100);
		m_iAlertVehicle = SRP_CmdSettings.GetIntClamped("alerte_vehicule", 0, 100);
		m_iAlertThreatPer = SRP_CmdSettings.GetIntClamped("alerte_menace_par", 1, 1000);
		m_iAlertThreatMax = SRP_CmdSettings.GetIntClamped("alerte_menace_max", 0, 10);
		m_iOfficerGuards = SRP_CmdSettings.GetIntClamped("officier_gardes", 0, 6);
		m_iOfficerHideouts = SRP_CmdSettings.GetIntClamped("officier_cachettes", 1, 10);
		m_iOfficerDayHour = SRP_CmdSettings.GetIntClamped("officier_changement_heure", 0, 23);
		m_iOfficerSpawnM = SRP_CmdSettings.GetIntClamped("officier_apparition_m", 100, 5000);
		m_iOfficerDespawnM = SRP_CmdSettings.GetIntClamped("officier_retrait_m", 100, 10000);
		m_iOfficerDespawnMin = SRP_CmdSettings.GetIntClamped("officier_retrait_min", 0, 120);
		m_iOfficerAiMargin = SRP_CmdSettings.GetIntClamped("officier_marge_ia", 0, 100);
		m_iOfficerFleeM = SRP_CmdSettings.GetIntClamped("officier_fuite_m", 10, 2000);
		m_iOfficerEscapeM = SRP_CmdSettings.GetIntClamped("officier_echappe_m", 100, 5000);
		m_iOfficerAbandonM = SRP_CmdSettings.GetIntClamped("officier_abandon_m", 10, 2000);
		m_iOfficerUnconsciousMin = SRP_CmdSettings.GetIntClamped("officier_inconscient_min", 1, 120);
		m_iOfficerCaptiveRetireMin = SRP_CmdSettings.GetIntClamped("officier_captif_retrait_min", 1, 120);
		m_sOfficerPrefab = SRP_CmdSettings.GetString("officier_prefab");
		m_iOfficerGarrisonBonusCent = SRP_CmdSettings.GetIntClamped("officier_bonus_garnison", 0, 200);
		m_sOfficerNames = SRP_CmdSettings.GetString("officier_noms");
		m_iOfficerNamesMemory = SRP_CmdSettings.GetIntClamped("officier_noms_memoire", 0, 29);
		m_iDesorgMin = SRP_CmdSettings.GetIntClamped("desorg_min", 1, 2880);
		m_iDesorgCaptureFactor = SRP_CmdSettings.GetIntClamped("desorg_capture_facteur", 1, 10);
	}

	//------------------------------------------------------------------------------------------------
	//! OF1, AVANT Load, déterministe : graines (la plus éloignée de la base, puis la plus éloignée de la graine la
	//! plus proche), croissance, équilibrage (300 passes), numérotation R1..R6, retouches « region », noms, somme de
	//! contrôle (h x 131 + région + 2) modulo 8388593, m_iTzOffset, rapport WriteReport — SRP_Commander.BuildRegions
	void Trace()
	{
		m_aRegions.Clear();
		m_aZoneRegion.Clear();
		m_aHomeRegion.Clear();
		m_aReport.Clear();
		m_aTraceNotes.Clear();
		m_aReadNotes.Clear();
		m_iHash = 0;
		m_iTzOffset = ComputeTzOffset();

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || front.GetZoneCount() < 2)
		{
			m_aReport.Insert("Tracé impossible : le front n'a aucune zone (géométrie absente)");
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Régions : tracé impossible, le front n'a aucune zone");
			WriteReport();
			return;
		}
		BuildGraph(front);
		int zoneCount = front.GetZoneCount();
		for (int z = 0; z < zoneCount; z++)
		{
			m_aZoneRegion.Insert(-1);
			m_aHomeRegion.Insert(-1);
		}

		// Position de la base : son repère, sinon le centre de la zone de la base
		vector basePos;
		if (!SRP_Placement.BasePosition(basePos))
			basePos = m_aZonePos[0];

		// 1. Graines
		array<int> seeds = {};
		PickSeeds(basePos, seeds);
		if (seeds.IsEmpty())
		{
			m_aReport.Insert("Tracé impossible : aucune zone hors de la base");
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Régions : tracé impossible, aucune zone hors de la base");
			WriteReport();
			return;
		}

		// 2. Croissance
		array<int> owner = {};
		for (int k = 0; k < zoneCount; k++)
			owner.Insert(-1);
		foreach (int seedRegion, int seedZone : seeds)
			owner[seedZone] = seedRegion;
		Grow(owner, seeds);

		// 3. Équilibrage entre les bornes (un maximum sous le minimum vaut le minimum)
		int maxZones = m_iRegionMaxZones;
		if (maxZones < m_iRegionMinZones)
			maxZones = m_iRegionMinZones;
		int passes = Balance(owner, seeds, m_iRegionMinZones, maxZones);

		// 4. Numérotation R1 (la plus proche de la base) .. Rn
		Number(owner, seeds, basePos);

		// 5. Retouches « region S12 -> R3 »
		RebuildRegionZones(m_aHomeRegion);
		ApplyRetouches();

		// Le tracé d'origine devient l'état courant
		for (int c = 0; c < zoneCount; c++)
			m_aZoneRegion[c] = m_aHomeRegion[c];
		RebuildRegionZones(m_aZoneRegion);
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			reg.m_aHomeZones.Clear();
			reg.m_aHomeZones.Copy(reg.m_aZones);
			reg.m_vCentroid = RegionCentroid(reg.m_iIndex, m_aHomeRegion);
		}

		// 6. Noms (VU1)
		NameRegions(front);

		// 7. Somme de contrôle
		int h = 0;
		for (int hz = 0; hz < zoneCount; hz++)
			h = HashStep(h, m_aHomeRegion[hz] + 2);
		m_iHash = h;

		// 8. Rapport
		BuildTraceReport(front, basePos, passes, maxZones);
		WriteReport();
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, string.Format("Régions tracées : %1 région(s) sur %2 voulue(s), %3 zone(s), somme de contrôle %4 (détail dans commandeur_regions.txt)", m_aRegions.Count(), m_iRegionsWanted, zoneCount - 1, m_iHash));
	}

	//------------------------------------------------------------------------------------------------
	//! Front prêt : officier créé pour chaque région non libérée qui n'en a pas (nouvelle campagne), cachette du
	//! jour choisie — SRP_Commander.Start
	void Start(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		EnsureGraph(front);
		int created = 0;
		int alive = 0;
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || reg.m_iState == SRP_ECmdRegionState.LIBEREE)
				continue;
			if (!reg.m_Officer)
			{
				if (reg.m_iState == SRP_ECmdRegionState.DESORGANISEE && nowUnix < reg.m_iDesorgUntil)
					continue;
				NewOfficer(reg.m_iIndex, "");
				created++;
			}
			else if (reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT)
				RefreshHideout(reg, nowUnix);
			if (reg.m_Officer && reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT)
				alive++;
		}
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.OFFICIER, string.Format("Régions prêtes : %1 région(s), %2 officier(s) en poste, dont %3 nouveau(x)", m_aRegions.Count(), alive, created));
		WriteReport();
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s : officiers (remplacement à la fin de la désorganisation, cachette du jour, pose, retrait,
	//! fuite, évasion, chute par mort, menottes ou abandon) — SRP_Commander.Tick
	void Tick(int nowUnix)
	{
		if (m_aRegions.IsEmpty())
			return;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;
		EnsureGraph(front);

		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg)
				continue;

			// Remplacement (OF7) : fin de la désorganisation, ou région sans officier (relecture partielle)
			if (reg.m_iState != SRP_ECmdRegionState.LIBEREE)
			{
				bool due = reg.m_iState == SRP_ECmdRegionState.DESORGANISEE && nowUnix >= reg.m_iDesorgUntil;
				bool missing = !reg.m_Officer && reg.m_iState == SRP_ECmdRegionState.ACTIVE;
				if (due || missing)
					NewOfficer(reg.m_iIndex, "");
			}

			SRP_CmdOfficer officer = reg.m_Officer;
			if (!officer)
				continue;
			if (officer.m_iState == SRP_ECmdOfficerState.VIVANT)
			{
				if (!officer.m_aGroups.IsEmpty())
				{
					WatchOfficer(reg, nowUnix);
					continue;
				}
				if (reg.m_iState == SRP_ECmdRegionState.LIBEREE)
					continue;
				// Cachette du jour (OF2) : jamais pendant la pose, jamais tant qu'une mission le vise
				if (officer.m_iHideDay != DayIndex(nowUnix) && !IsTargeted(reg))
					ChooseHideout(reg, nowUnix);
				if (nowUnix >= officer.m_iDeferUntil)
					PlaceOfficer(reg, nowUnix);
			}
			else if (!officer.m_aGroups.IsEmpty())
				WatchCaptive(reg, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La version du front a changé : cachettes tombées (localité de la cachette du jour prise -> nouvelle cachette)
	//! SEULEMENT ; les libérations OF11 passent par CheckLiberation — SRP_Commander.Tick
	void OnFrontChanged(int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;
		EnsureGraph(front);
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || !reg.m_Officer || reg.m_iState == SRP_ECmdRegionState.LIBEREE)
				continue;
			SRP_CmdOfficer officer = reg.m_Officer;
			if (officer.m_iState != SRP_ECmdOfficerState.VIVANT || !officer.m_aGroups.IsEmpty())
				continue;
			if (IsHideoutValid(front, reg, officer))
				continue;
			if (IsTargeted(reg))
				continue;
			ChooseHideout(reg, nowUnix);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! OF11, SEULE porte de libération : la région de la zone (ACTIVE ou DESORGANISEE) dont toutes les zones sont
	//! bleues -> LIBEREE, officier DISPARU, unités retirées hors de vue, MarkDirty(true) ; SRP_FrontRadio
	//! .RegionLiberated et SRP_CmdLog SEULEMENT si !staff (Q9 : une zone forcée par le Staff ne donne ni radio ni
	//! évènement region_liberee, l'état seul change) — SRP_Commander.OnZoneCaptured(zone, staff), seul appelant
	void CheckLiberation(int zone, bool staff, int nowUnix)
	{
		int region = GetRegionOfZone(zone);
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || reg.m_iState == SRP_ECmdRegionState.LIBEREE)
			return;
		if (CountRegionRedZones(region) > 0)
			return;

		// Seul un officier vivant « quitte l'île » : tué ou capturé, la radio dit « l'officier ennemi » sans nom
		string surname = "";
		if (reg.m_Officer && reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT)
			surname = reg.m_Officer.m_sName;
		reg.m_iState = SRP_ECmdRegionState.LIBEREE;
		reg.m_iLiberatedAt = nowUnix;
		reg.m_iDesorgUntil = 0;
		reg.m_bDesorgCaptured = false;
		SRP_CmdOfficer officer = reg.m_Officer;
		if (officer)
		{
			if (officer.m_iState == SRP_ECmdOfficerState.VIVANT)
			{
				officer.m_iState = SRP_ECmdOfficerState.DISPARU;
				officer.m_iDownUnix = nowUnix;
			}
			RetireGroups(officer);
		}

		if (!staff)
		{
			SRP_FrontRadio.RegionLiberated(region, surname);
			string who = "sans officier en poste";
			if (!surname.IsEmpty())
				who = "l'officier ennemi " + surname + " a quitté l'île";
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : %2 libérée, %3", reg.m_sCode, GetRegionLabel(region), who));
		}
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(true);
	}

	//================================================================================================
	// Régions (lecture)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Nombre de régions tracées — tout le Commandeur, écrans
	int GetRegionCount()
	{
		return m_aRegions.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! La région d'index donné, null hors bornes — écrans, sous-modules
	SRP_CmdRegion GetRegion(int region)
	{
		if (region < 0 || region >= m_aRegions.Count())
			return null;
		return m_aRegions[region];
	}

	//------------------------------------------------------------------------------------------------
	//! Région qui tient ou tenait la zone (OF11 compris), -1 pour la base ou sans région — tout le Commandeur
	int GetRegionOfZone(int zone)
	{
		if (zone <= 0 || zone >= m_aZoneRegion.Count())
			return -1;
		return m_aZoneRegion[zone];
	}

	//------------------------------------------------------------------------------------------------
	//! Région sous une position (zone du carré), -1 en mer ou dans la base — accroches statiques, sous-modules
	int GetRegionAt(vector position)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return -1;
		int zone = front.GetZoneAt(position);
		if (zone <= 0)
			return -1;
		return GetRegionOfZone(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Code stable « R3 » (clé de sauvegarde) — sauvegardes, écrans
	string GetRegionCode(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return "";
		return reg.m_sCode;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom seul « Saint-Philippe » (plus grande localité, VU1) — écrans, radio
	string GetRegionName(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return "";
		return reg.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	//! « région de Saint-Philippe », « région d'Erquy » (VU1) — radio, écrans, journal
	string GetRegionLabel(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return "";
		// Une seule règle d'élision pour tout le Commandeur (SRP_CmdScreens.RegionOf), repli local identique
		string label = SRP_CmdScreens.RegionOf(reg.m_sName);
		if (label.IsEmpty())
			label = LabelOf(reg.m_sName);
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Région par code (« R3 », casse ignorée), -1 sinon — lecture de front.json, Staff
	int FindRegion(string code)
	{
		string wanted = code.Trim();
		wanted.ToLower();
		if (wanted.IsEmpty())
			return -1;
		foreach (int i, SRP_CmdRegion reg : m_aRegions)
		{
			string own = reg.m_sCode;
			own.ToLower();
			if (own == wanted)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdRegionState de la région (DESORGANISEE levée d'elle-même à m_iDesorgUntil par Tick) — tout le Commandeur
	int GetRegionState(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return SRP_ECmdRegionState.ACTIVE;
		return reg.m_iState;
	}

	//------------------------------------------------------------------------------------------------
	//! OF6 à OF9 : région désorganisée — manœuvres (x renfort_facteur_desorg, OF8), appuis (ni mortier ni artillerie)
	bool IsRegionDisorganized(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		return reg && reg.m_iState == SRP_ECmdRegionState.DESORGANISEE;
	}

	//------------------------------------------------------------------------------------------------
	//! Région vivante (ACTIVE ou DESORGANISEE : elle a encore une zone rouge) — ressources, appuis
	bool IsRegionAlive(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		return reg && reg.m_iState != SRP_ECmdRegionState.LIBEREE;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones de départ de la région (tracé d'origine, RE2 : parts des 48 obus) — SRP_CmdResources.Start
	int GetRegionStartZones(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return 0;
		return reg.m_aHomeZones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Zones rattachées à la région ; rend leur nombre — ressources, dépôts, écrans
	int GetRegionZones(int region, notnull array<int> zones)
	{
		zones.Clear();
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return 0;
		zones.Copy(reg.m_aZones);
		return zones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Zones ROUGES de la région (revenu RE4, OF11) — ressources, écrans
	int CountRegionRedZones(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!reg || !front)
			return 0;
		int count = 0;
		foreach (int zone : reg.m_aZones)
		{
			if (front.GetZoneOwner(zone) == SRP_EFrontOwner.ROUGE)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Somme de contrôle du tracé (sauvée, comparée au chargement) — SRP_Commander.WriteTo, ReadFrom
	int GetHash()
	{
		return m_iHash;
	}

	//================================================================================================
	// Alerte (CO9) et menace locale
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Alerte actuelle = max(0, alerte - (now - m_iAlertAt) x alerte_max / (alerte_retombee_min x 60)), calculée à la
	//! lecture (le temps serveur éteint compte) — cerveau, écrans
	float GetRegionAlert(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return 0;
		return CurrentAlert(reg, System.GetUnixTime());
	}

	//------------------------------------------------------------------------------------------------
	//! Alerte + amount (alerte_max au plus), m_iAlertAt = maintenant ; ligne SRP_CmdLog à chaque palier de 20 —
	//! cerveau (comptes rendus, missions), SRP_Commander.AddAlertAt, dépôts (sabotage)
	void AddRegionAlert(int region, float amount, string reason)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || amount == 0)
			return;
		int now = System.GetUnixTime();
		float before = CurrentAlert(reg, now);
		float after = before + amount;
		if (after > m_iAlertMax)
			after = m_iAlertMax;
		if (after < 0)
			after = 0;
		reg.m_fAlert = after;
		reg.m_iAlertAt = now;

		int stepBefore = Math.Floor(before / ALERT_STEP);
		int stepAfter = Math.Floor(after / ALERT_STEP);
		reg.m_iAlertStep = stepAfter;
		if (stepAfter > stepBefore)
			SRP_CmdLog.Note(region, SRP_ECmdLogKind.SYSTEME, string.Format("%1 : alerte de la %2 à %3 sur %4 (%5)", reg.m_sCode, GetRegionLabel(region), stepAfter * ALERT_STEP, m_iAlertMax, reason));

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! CO9 : min(10, islandThreat + min(alerte_menace_max, floor(alerte / alerte_menace_par))) pour la région de la zone
	//! (islandThreat hors région) — SRP_Commander.ThreatAt
	int ThreatAtZone(int zone, int islandThreat)
	{
		SRP_CmdRegion reg = GetRegion(GetRegionOfZone(zone));
		if (!reg)
			return islandThreat;
		int total = islandThreat + ThreatAdd(CurrentAlert(reg, System.GetUnixTime()));
		if (total > 10)
			total = 10;
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! OF5 : 1 + officier_bonus_garnison / 100 si l'officier de la région de la zone vit, sinon 1 —
	//! SRP_Commander.GarrisonFactorAt
	float GarrisonFactorAtZone(int zone)
	{
		int region = GetRegionOfZone(zone);
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || reg.m_iState != SRP_ECmdRegionState.ACTIVE || !HasLivingOfficer(region))
			return 1.0;
		return 1.0 + m_iOfficerGarrisonBonusCent / 100.0;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdMood de la région (recalculée par le cerveau) — manœuvres (CA1), écrans
	int GetRegionMood(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return SRP_ECmdMood.NORMALE;
		return reg.m_iMood;
	}

	//------------------------------------------------------------------------------------------------
	//! Range l'humeur calculée par le cerveau (CO7) — SRP_Commander.RecomputeMoods
	void SetRegionMood(int region, int mood)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (reg)
			reg.m_iMood = mood;
	}

	//------------------------------------------------------------------------------------------------
	//! OF3/OF6 : un compte rendu de la région a été perdu (désorganisée) ou retardé (radio coupée à locality) — cerveau
	void NoteReportTrouble(int region, bool lost, string locality, int untilUnix)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return;
		reg.m_iLastReportUnix = System.GetUnixTime();
		if (lost)
		{
			reg.m_iReportsLost++;
			return;
		}
		reg.m_iReportsDelayed++;
		reg.m_sRadioCutLocality = locality;
		reg.m_iRadioCutUntil = untilUnix;
	}

	//================================================================================================
	// Officiers (lecture)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! L'officier de la région est VIVANT (posé ou « sur le papier ») — OF5, cerveau, écrans
	bool HasLivingOfficer(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		return reg && reg.m_Officer && reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdTrait de l'officier (METHODIQUE sans officier) — cerveau, manœuvres (CA1, source prudente)
	int GetOfficerTrait(int region)
	{
		// OF6 : une région sans officier vivant n'a plus de caractère
		if (!HasLivingOfficer(region))
			return SRP_ECmdTrait.METHODIQUE;
		return m_aRegions[region].m_Officer.m_iTrait;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom de l'officier en poste (ou du dernier tombé), « » sans officier — radio, écrans
	string GetOfficerName(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || !reg.m_Officer)
			return "";
		return reg.m_Officer.m_sName;
	}

	//------------------------------------------------------------------------------------------------
	//! Numéro unique de l'officier en poste, 0 sans officier (VU2 : le renseignement tombe au changement) —
	//! SRP_CmdIntel.HasIntel
	int GetOfficerSerial(int region)
	{
		// L'officier tombé reste « en poste » jusqu'à son remplaçant : le renseignement tient jusque-là
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || !reg.m_Officer)
			return 0;
		return reg.m_Officer.m_iSerial;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdOfficerState de l'officier, -1 sans officier — écrans, missions
	int GetOfficerState(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || !reg.m_Officer)
			return -1;
		return reg.m_Officer.m_iState;
	}

	//------------------------------------------------------------------------------------------------
	//! L'officier posé (null s'il est « sur le papier ») — écrans, missions
	IEntity GetOfficerEntity(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || !reg.m_Officer || reg.m_Officer.m_aGroups.IsEmpty())
			return null;
		IEntity entity = reg.m_Officer.m_Entity;
		if (!entity || entity.IsDeleted())
			return null;
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Cachette du jour (position et localité) ; faux sans officier vivant — missions (OF10), Staff (téléportation)
	bool GetOfficerHideout(int region, out vector position, out string locality)
	{
		position = vector.Zero;
		locality = "";
		if (!HasLivingOfficer(region))
			return false;
		SRP_CmdOfficer officer = m_aRegions[region].m_Officer;
		if (officer.m_vHidePos == vector.Zero)
			return false;
		position = officer.m_vHidePos;
		locality = HideLabel(officer);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! L'officier est posé dans le monde — écrans
	bool IsOfficerPosted(int region)
	{
		return GetOfficerEntity(region) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! OnControllableDestroyed : la victime est-elle un officier posé ? Si oui, OfficerDown(TUE) et vrai —
	//! SRP_Commander.OnControllableDestroyed
	bool OnPossibleOfficerDeath(IEntity victim)
	{
		if (!victim)
			return false;
		foreach (int i, SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || !reg.m_Officer || reg.m_Officer.m_Entity != victim)
				continue;
			// Un officier déjà tombé (capturé puis abattu) ne tombe pas deux fois
			if (reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT)
				OfficerDown(i, false, "tué", "");
			return true;
		}
		return false;
	}

	//================================================================================================
	// Officiers (actions)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! OF6, OF7, OF9 : officier TUE ou CAPTURE, région DESORGANISEE jusqu'à maintenant + desorg_min (x
	//! desorg_capture_facteur si capturé), AddRegionAlert(alerte_officier), SRP_FrontRadio.OfficerFell, capturé :
	//! SRP_CmdIntel.OnOfficerCaptured (renseignement et dépôt révélé), SRP_CmdLog, MarkDirty(true) ; rend le compte
	//! rendu — Tick (chute vue), OnPossibleOfficerDeath, SRP_Commander.ForceOfficerDown
	string OfficerDown(int region, bool captured, string cause, string author)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return "Région inconnue";
		SRP_CmdOfficer officer = reg.m_Officer;
		if (!officer || officer.m_iState != SRP_ECmdOfficerState.VIVANT)
			return string.Format("%1 : pas d'officier vivant à faire tomber", reg.m_sCode);
		if (reg.m_iState == SRP_ECmdRegionState.LIBEREE)
			return string.Format("%1 : région libérée, pas d'officier", reg.m_sCode);

		int now = System.GetUnixTime();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();

		// Où il est tombé (un officier « sur le papier » n'a pas de zone)
		int zone = -1;
		IEntity entity = officer.m_Entity;
		if (front && entity && !entity.IsDeleted())
			zone = front.GetZoneAt(entity.GetOrigin());

		if (captured)
			officer.m_iState = SRP_ECmdOfficerState.CAPTURE;
		else
			officer.m_iState = SRP_ECmdOfficerState.TUE;
		officer.m_iDownUnix = now;
		officer.m_bFleeing = false;
		officer.m_iUnconsciousSince = 0;
		officer.m_iCaptiveIdleSince = 0;

		// OF7, OF9 : désorganisation en heure Unix (redémarrages compris)
		int minutes = m_iDesorgMin;
		if (captured)
			minutes = minutes * m_iDesorgCaptureFactor;
		reg.m_iState = SRP_ECmdRegionState.DESORGANISEE;
		reg.m_iDesorgUntil = now + minutes * 60;
		reg.m_bDesorgCaptured = captured;
		reg.m_iLastFallUnix = now;
		if (captured)
			reg.m_iLastFallStatus = SRP_ECmdMissionStatus.CAPTURE;
		else
			reg.m_iLastFallStatus = SRP_ECmdMissionStatus.TOMBE;

		// Ses gardes deviennent des défenseurs ordinaires jusqu'au retrait habituel ; le captif reste sur place
		ReleaseFallenGroups(officer, captured);

		AddRegionAlert(region, m_iAlertOfficer, "officier tombé");
		SRP_FrontRadio.OfficerFell(region, officer.m_sName, captured, zone);
		if (captured)
			SRP_CmdIntel.OnOfficerCaptured(region);

		string fate = "tué";
		if (captured)
			fate = "capturé";
		if (!cause.IsEmpty() && cause != fate)
		{
			// « tué, chute forcée par le Staff » se suffit ; « laissé pour mort » complète « tué »
			if (cause.StartsWith(fate))
				fate = cause;
			else
				fate = fate + " (" + cause + ")";
		}
		string staffPart = "";
		if (!author.IsEmpty())
			staffPart = " (Staff : " + author + ")";
		string text = string.Format("%1 : officier ennemi %2 %3%4 ; région désorganisée jusqu'à %5 (%6)", reg.m_sCode, officer.m_sName, fate, staffPart, HourText(reg.m_iDesorgUntil), DurText(minutes * 60));
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.OFFICIER, text);
		if (front)
			front.MarkDirty(true);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! OF7 : nouvel officier (autre nom que les officier_noms_memoire derniers, autre caractère que le prédécesseur,
	//! pas un 3e du même caractère), région ACTIVE, cachette choisie ; silencieux pour les joueurs (VU2) — Tick,
	//! Start, SRP_Commander.ForceOfficerReplace
	string NewOfficer(int region, string author)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return "Région inconnue";
		if (reg.m_iState == SRP_ECmdRegionState.LIBEREE)
			return string.Format("%1 : région libérée, pas de nouvel officier", reg.m_sCode);

		int now = System.GetUnixTime();
		SRP_CmdOfficer previous = reg.m_Officer;
		if (previous)
		{
			// Ce qui reste posé de l'ancien (captif, gardes) se retire hors de vue
			RetireGroups(previous);
			reg.m_iPrevTrait = previous.m_iTrait;
		}

		SRP_CmdOfficer officer = new SRP_CmdOfficer();
		officer.m_iSerial = m_iNextSerial;
		m_iNextSerial++;
		officer.m_sName = PickName();
		officer.m_iTrait = PickTrait(reg);
		officer.m_iState = SRP_ECmdOfficerState.VIVANT;
		officer.m_iSinceUnix = now;
		if (previous)
			officer.m_sHideLocality = previous.m_sHideLocality;	// pour ne pas reprendre la même cachette
		reg.m_Officer = officer;
		reg.m_iState = SRP_ECmdRegionState.ACTIVE;
		reg.m_iDesorgUntil = 0;
		reg.m_bDesorgCaptured = false;

		m_aUsedNames.Insert(officer.m_sName);
		while (m_aUsedNames.Count() > m_iOfficerNamesMemory)
			m_aUsedNames.RemoveOrdered(0);

		ChooseHideout(reg, now);

		string staffPart = "";
		if (!author.IsEmpty())
			staffPart = " (Staff : " + author + ")";
		string place = HideLabel(officer);
		if (place.IsEmpty())
			place = "sans cachette pour l'instant";
		else
			place = "à " + place + " aujourd'hui";
		string text = string.Format("%1 : nouvel officier ennemi %2 (%3), %4%5", reg.m_sCode, officer.m_sName, TraitText(officer.m_iTrait), place, staffPart);
		SRP_CmdLog.Note(region, SRP_ECmdLogKind.OFFICIER, text);
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(true);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! OF11 : une zone passe rouge alors que sa région est LIBEREE : rattachée à la région voisine non libérée qui
	//! partage le plus de côtés (sinon parcours en largeur) ; aucune : la région renaît DESORGANISEE ; sauvé, journal —
	//! SRP_Commander.OnZoneLost
	void AttachZone(int zone, int nowUnix)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || zone <= 0 || zone >= m_aZoneRegion.Count())
			return;
		int source = m_aZoneRegion[zone];
		SRP_CmdRegion origin = GetRegion(source);
		if (!origin || origin.m_iState != SRP_ECmdRegionState.LIBEREE)
			return;
		EnsureGraph(front);

		int target = SharedSidesRegion(front, zone, source);
		if (target < 0)
			target = NearestLiveRegion(zone, source);

		if (target < 0)
		{
			// Plus aucune région n'a d'officier : la région d'origine renaît, un officier arrive après desorg_min
			origin.m_iState = SRP_ECmdRegionState.DESORGANISEE;
			origin.m_iDesorgUntil = nowUnix + m_iDesorgMin * 60;
			origin.m_bDesorgCaptured = false;
			origin.m_iLiberatedAt = 0;
			SRP_CmdLog.Note(source, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : l'ennemi reprend %2 alors qu'aucune région n'a plus d'officier ; la %3 renaît, désorganisée, nouvel officier vers %4", origin.m_sCode, front.GetZoneLabel(zone), GetRegionLabel(source), HourText(origin.m_iDesorgUntil)));
			front.MarkDirty(true);
			return;
		}

		SRP_CmdRegion host = m_aRegions[target];
		m_aZoneRegion[zone] = target;
		origin.m_aZones.RemoveItemOrdered(zone);
		if (!host.m_aZones.Contains(zone))
		{
			host.m_aZones.Insert(zone);
			host.m_aZones.Sort();
		}
		SRP_CmdLog.Note(target, SRP_ECmdLogKind.OFFICIER, string.Format("%1 reprise par l'ennemi dans la %2 (libérée) : rattachée à %3, %4", front.GetZoneLabel(zone), GetRegionLabel(source), host.m_sCode, GetRegionLabel(target)));
		front.MarkDirty(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Officiers posés retirés hors de vue (ils restent vivants « sur le papier ») — CancelOperations, OnRestored
	void UnpostOfficers(string reason)
	{
		int now = System.GetUnixTime();
		int count = 0;
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || !reg.m_Officer || reg.m_Officer.m_aGroups.IsEmpty())
				continue;
			count += RetireGroups(reg.m_Officer);
			if (reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT)
				reg.m_Officer.m_iDeferUntil = now + RETRY_S * 2;
		}
		if (count > 0)
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.OFFICIER, string.Format("Officiers posés retirés hors de vue (%1) : %2 unité(s) ; les officiers vivants restent « sur le papier »", reason, count));
	}

	//================================================================================================
	// Missions (OF10)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Officier à viser pour une mission OFFICIER près de near : région, point de la cachette du jour, libellé
	//! (« officier ennemi Volkov, Chotain ») ; faux sans candidat (vivant, non visé par une mission active) —
	//! SRP_MissionManagerComponent
	bool PickOfficerTarget(vector near, out int region, out vector site, out string label)
	{
		// near = vector.Zero : sans préférence de lieu (SRP_Commander.HasOfficerTarget ne lit que la réponse)
		region = -1;
		site = vector.Zero;
		label = "";
		int best = -1;
		float bestDist = 0;
		foreach (int i, SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || reg.m_iState != SRP_ECmdRegionState.ACTIVE || !HasLivingOfficer(i))
				continue;
			SRP_CmdOfficer officer = reg.m_Officer;
			if (officer.m_vHidePos == vector.Zero || IsTargeted(reg))
				continue;
			float distance = vector.Distance(near, officer.m_vHidePos);
			if (best < 0 || distance < bestDist)
			{
				best = i;
				bestDist = distance;
			}
		}
		if (best < 0)
			return false;
		SRP_CmdOfficer chosen = m_aRegions[best].m_Officer;
		region = best;
		site = chosen.m_vHidePos;
		label = string.Format("officier ennemi %1, %2", chosen.m_sName, HideLabel(chosen));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_ECmdMissionStatus de l'officier depuis sinceUnix (tombé, capturé, échappé, en cours, sans objet) —
	//! SRP_MissionManagerComponent (fin de la mission OFFICIER)
	int GetOfficerMissionStatus(int region, int sinceUnix)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || !reg.m_Officer)
			return SRP_ECmdMissionStatus.SANS_OBJET;
		SRP_CmdOfficer officer = reg.m_Officer;

		// L'officier visé a déjà été remplacé : sa chute compte si elle a eu lieu après le lancement
		if (officer.m_iSinceUnix > sinceUnix)
		{
			if (reg.m_iLastFallUnix > 0 && reg.m_iLastFallUnix >= sinceUnix)
				return reg.m_iLastFallStatus;
			return SRP_ECmdMissionStatus.SANS_OBJET;
		}
		if (officer.m_iState == SRP_ECmdOfficerState.TUE)
		{
			if (officer.m_iDownUnix > 0 && officer.m_iDownUnix >= sinceUnix)
				return SRP_ECmdMissionStatus.TOMBE;
			return SRP_ECmdMissionStatus.SANS_OBJET;
		}
		if (officer.m_iState == SRP_ECmdOfficerState.CAPTURE)
		{
			if (officer.m_iDownUnix > 0 && officer.m_iDownUnix >= sinceUnix)
				return SRP_ECmdMissionStatus.CAPTURE;
			return SRP_ECmdMissionStatus.SANS_OBJET;
		}
		// Région libérée (DISPARU) : la mission est annulée
		if (officer.m_iState != SRP_ECmdOfficerState.VIVANT)
			return SRP_ECmdMissionStatus.SANS_OBJET;
		if (officer.m_iEscapedUnix > 0 && officer.m_iEscapedUnix >= sinceUnix)
			return SRP_ECmdMissionStatus.ECHAPPE;
		return SRP_ECmdMissionStatus.EN_COURS;
	}

	//================================================================================================
	// Sauvegarde (front.json, clés cmd_r*) et remises
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! cmd_rn, cmd_rserial, cmd_rnames, puis par région cmd_rN_code, _state, _desorg, _desorgCap, _lib, _alert,
	//! _alertAt, _prevTrait, _of (1 si officier), _of_serial, _of_name, _of_trait, _of_state, _of_since, _of_down,
	//! _of_day, _of_loc, _of_prev, _of_escaped ; rattachements cmd_rmc, cmd_rmN_z (code de zone), cmd_rmN_r (code de
	//! région) — SRP_Commander.WriteTo
	void WriteTo(JsonSaveContext ctx)
	{
		if (!ctx)
			return;
		ctx.WriteValue("cmd_rn", m_aRegions.Count());
		ctx.WriteValue("cmd_rserial", m_iNextSerial);
		ctx.WriteValue("cmd_rnames", JoinNames(m_aUsedNames));
		foreach (int i, SRP_CmdRegion reg : m_aRegions)
		{
			string p = "cmd_r" + i.ToString() + "_";
			ctx.WriteValue(p + "code", reg.m_sCode);
			ctx.WriteValue(p + "state", reg.m_iState);
			ctx.WriteValue(p + "desorg", reg.m_iDesorgUntil);
			ctx.WriteValue(p + "desorgCap", reg.m_bDesorgCaptured);
			ctx.WriteValue(p + "lib", reg.m_iLiberatedAt);
			ctx.WriteValue(p + "alert", reg.m_fAlert);
			ctx.WriteValue(p + "alertAt", reg.m_iAlertAt);
			ctx.WriteValue(p + "prevTrait", reg.m_iPrevTrait);
			SRP_CmdOfficer officer = reg.m_Officer;
			if (!officer)
			{
				ctx.WriteValue(p + "of", 0);
				continue;
			}
			ctx.WriteValue(p + "of", 1);
			ctx.WriteValue(p + "of_serial", officer.m_iSerial);
			ctx.WriteValue(p + "of_name", officer.m_sName);
			ctx.WriteValue(p + "of_trait", officer.m_iTrait);
			ctx.WriteValue(p + "of_state", officer.m_iState);
			ctx.WriteValue(p + "of_since", officer.m_iSinceUnix);
			ctx.WriteValue(p + "of_down", officer.m_iDownUnix);
			ctx.WriteValue(p + "of_day", officer.m_iHideDay);
			ctx.WriteValue(p + "of_loc", officer.m_sHideLocality);
			ctx.WriteValue(p + "of_prev", officer.m_sPrevLocality);
			ctx.WriteValue(p + "of_escaped", officer.m_iEscapedUnix);
		}

		// Rattachements OF11 : zone (code) -> région (code)
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int links = 0;
		if (front)
		{
			for (int z = 1; z < m_aZoneRegion.Count(); z++)
			{
				int holder = m_aZoneRegion[z];
				if (holder < 0 || holder >= m_aRegions.Count() || holder == m_aHomeRegion[z])
					continue;
				string q = "cmd_rm" + links.ToString() + "_";
				ctx.WriteValue(q + "z", front.GetZoneCode(z));
				ctx.WriteValue(q + "r", m_aRegions[holder].m_sCode);
				links++;
			}
		}
		ctx.WriteValue("cmd_rmc", links);
	}

	//------------------------------------------------------------------------------------------------
	//! Relecture miroir par CODE de région et de zone (rattachements appliqués) — SRP_Commander.ReadFrom
	void ReadFrom(JsonLoadContext ctx)
	{
		if (!ctx)
			return;
		int n = -1;
		if (!ctx.ReadValue("cmd_rn", n) || n < 0)
		{
			WriteReport();
			return;
		}

		// Une restauration peut arriver pendant la partie : rien ne doit rester posé sans suivi
		UnpostOfficers("relecture de front.json");
		ResetStates();
		m_aReadNotes.Clear();

		int serial = 0;
		if (ctx.ReadValue("cmd_rserial", serial) && serial > m_iNextSerial)
			m_iNextSerial = serial;
		string names = "";
		if (ctx.ReadValue("cmd_rnames", names))
			SplitNames(names, m_aUsedNames);

		int now = System.GetUnixTime();
		for (int i = 0; i < n; i++)
		{
			string p = "cmd_r" + i.ToString() + "_";
			string code = "";
			if (!ctx.ReadValue(p + "code", code))
				continue;
			int region = FindRegion(code);
			if (region < 0)
			{
				m_aReadNotes.Insert(string.Format("front.json : région %1 inconnue du tracé actuel, son état est ignoré", code));
				continue;
			}
			SRP_CmdRegion reg = m_aRegions[region];

			int state = reg.m_iState;
			if (ctx.ReadValue(p + "state", state) && state >= SRP_ECmdRegionState.ACTIVE && state <= SRP_ECmdRegionState.LIBEREE)
				reg.m_iState = state;
			int desorg = reg.m_iDesorgUntil;
			if (ctx.ReadValue(p + "desorg", desorg))
				reg.m_iDesorgUntil = desorg;
			bool desorgCap = reg.m_bDesorgCaptured;
			if (ctx.ReadValue(p + "desorgCap", desorgCap))
				reg.m_bDesorgCaptured = desorgCap;
			int liberated = reg.m_iLiberatedAt;
			if (ctx.ReadValue(p + "lib", liberated))
				reg.m_iLiberatedAt = liberated;
			float alert = reg.m_fAlert;
			if (ctx.ReadValue(p + "alert", alert))
				reg.m_fAlert = alert;
			int alertAt = reg.m_iAlertAt;
			if (ctx.ReadValue(p + "alertAt", alertAt))
				reg.m_iAlertAt = alertAt;
			int prevTrait = reg.m_iPrevTrait;
			if (ctx.ReadValue(p + "prevTrait", prevTrait))
				reg.m_iPrevTrait = prevTrait;
			reg.m_iAlertStep = Math.Floor(CurrentAlert(reg, now) / ALERT_STEP);

			int hasOfficer = 0;
			ctx.ReadValue(p + "of", hasOfficer);
			if (hasOfficer != 1)
				continue;
			SRP_CmdOfficer officer = new SRP_CmdOfficer();
			ctx.ReadValue(p + "of_serial", officer.m_iSerial);
			ctx.ReadValue(p + "of_name", officer.m_sName);
			ctx.ReadValue(p + "of_trait", officer.m_iTrait);
			ctx.ReadValue(p + "of_state", officer.m_iState);
			ctx.ReadValue(p + "of_since", officer.m_iSinceUnix);
			ctx.ReadValue(p + "of_down", officer.m_iDownUnix);
			ctx.ReadValue(p + "of_day", officer.m_iHideDay);
			ctx.ReadValue(p + "of_loc", officer.m_sHideLocality);
			ctx.ReadValue(p + "of_prev", officer.m_sPrevLocality);
			ctx.ReadValue(p + "of_escaped", officer.m_iEscapedUnix);
			if (officer.m_iSerial <= 0)
			{
				officer.m_iSerial = m_iNextSerial;
				m_iNextSerial++;
			}
			if (officer.m_iSerial >= m_iNextSerial)
				m_iNextSerial = officer.m_iSerial + 1;
			if (officer.m_iTrait < SRP_ECmdTrait.PRUDENT || officer.m_iTrait > SRP_ECmdTrait.METHODIQUE)
				officer.m_iTrait = SRP_ECmdTrait.METHODIQUE;
			if (officer.m_iState < SRP_ECmdOfficerState.VIVANT || officer.m_iState > SRP_ECmdOfficerState.DISPARU)
				officer.m_iState = SRP_ECmdOfficerState.VIVANT;
			if (officer.m_sName.IsEmpty())
				officer.m_sName = PickName();
			reg.m_Officer = officer;
		}

		// Rattachements OF11
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int links = 0;
		ctx.ReadValue("cmd_rmc", links);
		for (int k = 0; k < links; k++)
		{
			string q = "cmd_rm" + k.ToString() + "_";
			string zoneCode = "";
			string regionCode = "";
			if (!ctx.ReadValue(q + "z", zoneCode) || !ctx.ReadValue(q + "r", regionCode))
				continue;
			int zone = -1;
			if (front)
				zone = front.FindZone(zoneCode);
			int target = FindRegion(regionCode);
			if (zone <= 0 || zone >= m_aZoneRegion.Count() || target < 0)
			{
				m_aReadNotes.Insert(string.Format("front.json : rattachement %1 -> %2 ignoré (zone ou région inconnue)", zoneCode, regionCode));
				continue;
			}
			m_aZoneRegion[zone] = target;
		}
		RebuildRegionZones(m_aZoneRegion);
		foreach (string note : m_aReadNotes)
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Régions : " + note);
		WriteReport();
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne : tracé refait, officiers neufs, alertes à 0, rattachements effacés, désorganisations
	//! levées — SRP_Commander.ResetCampaign
	void ResetCampaign(string reason)
	{
		UnpostOfficers(reason);
		// Le tracé est déterministe (mêmes zones, mêmes retouches) : on repart du tracé d'origine
		ResetStates();
		m_aUsedNames.Clear();
		foreach (SRP_CmdRegion reg : m_aRegions)
			NewOfficer(reg.m_iIndex, "");
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.OFFICIER, string.Format("Régions remises à neuf (%1) : %2 région(s), alertes à 0, rattachements effacés, officiers neufs", reason, m_aRegions.Count()));
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(true);
		WriteReport();
	}

	//------------------------------------------------------------------------------------------------
	//! Après une restauration : officiers retirés hors de vue, cachettes recalculées — SRP_Commander.OnRestored
	void OnRestored()
	{
		UnpostOfficers("restauration");
		int now = System.GetUnixTime();
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (reg && reg.m_iState != SRP_ECmdRegionState.LIBEREE && reg.m_Officer && reg.m_Officer.m_iState == SRP_ECmdOfficerState.VIVANT)
				RefreshHideout(reg, now);
		}
	}

	//================================================================================================
	// Rapports Staff
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Page Staff d'une région (officier, cachette, désorganisation, alerte, rapports, zones) — SRP_CmdScreens
	string GetRegionReport(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return "Région inconnue";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		int now = System.GetUnixTime();
		array<string> lines = {};
		lines.Insert(string.Format("%1 — %2 : %3", reg.m_sCode, GetRegionLabel(region), RegionStateText(reg, now)));
		lines.Insert("Officier : " + OfficerText(reg, now));

		SRP_CmdOfficer officer = reg.m_Officer;
		if (officer && officer.m_iState == SRP_ECmdOfficerState.VIVANT)
		{
			lines.Insert(HideoutText(front, officer));
			if (officer.m_bFleeing)
			{
				string toward = officer.m_sFleeLocality;
				if (toward.IsEmpty())
					toward = "un point à l'écart";
				lines.Insert("En fuite vers " + toward);
			}
			if (officer.m_iEscapedUnix > 0)
				lines.Insert("Dernière évasion il y a " + DurText(now - officer.m_iEscapedUnix));
			if (officer.m_aGroups.IsEmpty() && officer.m_iDeferUntil > now)
				lines.Insert("Pose retenue encore " + DurText(officer.m_iDeferUntil - now));
			if (IsTargeted(reg))
				lines.Insert("Visé par une mission OFFICIER : la cachette ne change pas");
		}

		float alert = CurrentAlert(reg, now);
		int alertShown = Math.Round(alert);
		lines.Insert(string.Format("Alerte : %1 sur %2 (menace locale +%3) ; humeur : %4 ; renforts de défense +%5 centièmes", alertShown, m_iAlertMax, ThreatAdd(alert), MoodText(reg.m_iMood), GarrisonBonusText(region)));

		string reports = string.Format("Comptes rendus : %1 perdu(s), %2 retardé(s)", reg.m_iReportsLost, reg.m_iReportsDelayed);
		if (reg.m_iRadioCutUntil > now && !reg.m_sRadioCutLocality.IsEmpty())
			reports += string.Format(" ; radio de %1 coupée encore %2", reg.m_sRadioCutLocality, DurText(reg.m_iRadioCutUntil - now));
		lines.Insert(reports);

		lines.Insert(string.Format("Zones : %1 dont %2 rouge(s), %3 de départ : %4", reg.m_aZones.Count(), CountRegionRedZones(region), reg.m_aHomeZones.Count(), ZoneList(front, reg.m_aZones)));
		array<int> attached = {};
		foreach (int zone : reg.m_aZones)
		{
			if (!reg.m_aHomeZones.Contains(zone))
				attached.Insert(zone);
		}
		if (!attached.IsEmpty())
			lines.Insert("Rattachées après libération d'une voisine (OF11) : " + ZoneList(front, attached));
		return JoinLines(lines);
	}

	//------------------------------------------------------------------------------------------------
	//! Officiers et cachettes autour d'un point (distance, posé ou non, gardes) — SRP_CmdScreens.StaffNearReport
	string GetNearReport(vector from, float radius)
	{
		array<string> lines = {};
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || !reg.m_Officer)
				continue;
			SRP_CmdOfficer officer = reg.m_Officer;
			if (officer.m_iState == SRP_ECmdOfficerState.DISPARU)
				continue;
			vector where = officer.m_vHidePos;
			bool posted = officer.m_Entity && !officer.m_Entity.IsDeleted() && !officer.m_aGroups.IsEmpty();
			if (posted)
				where = officer.m_Entity.GetOrigin();
			if (where == vector.Zero)
				continue;
			float distance = vector.Distance(from, where);
			if (distance > radius)
				continue;
			int meters = Math.Round(distance);
			string what;
			if (officer.m_iState == SRP_ECmdOfficerState.VIVANT)
			{
				if (posted)
				{
					what = string.Format("posé à %1, %2 garde(s) debout", HideLabel(officer), CountGuards(officer));
					if (officer.m_bFleeing)
						what += ", en fuite";
				}
				else
					what = "sur le papier, cachette " + HideLabel(officer);
			}
			else if (officer.m_iState == SRP_ECmdOfficerState.CAPTURE)
			{
				what = "capturé";
				if (posted)
					what += ", encore sur place";
			}
			else
				what = "tué";
			lines.Insert(string.Format("%1 officier ennemi %2 — %3 m — %4", reg.m_sCode, officer.m_sName, meters, what));
		}
		if (lines.IsEmpty())
		{
			int shown = Math.Round(radius);
			return string.Format("Aucun officier ni cachette à %1 m", shown);
		}
		return JoinLines(lines);
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Réécrit $profile:SimpleRP/commandeur_regions.txt : une ligne par région (code, nom, zones, localités),
	//! retouches refusées, régions hors de la fourchette, état des officiers — Trace, ReadFrom
	protected void WriteReport()
	{
		int now = System.GetUnixTime();
		array<string> lines = {};
		lines.Insert("# Régions du Commandeur ennemi (OF1), rapport réécrit à chaque démarrage du serveur (" + SRP_Time.Now() + ")");
		lines.Insert("# Pour changer une zone de région : ajouter « region S12 -> R3 » dans front_retouches.txt (la zone doit toucher R3), puis redémarrer.");
		lines.Insert("# R1 est la région la plus proche de la base ; la base n'appartient à aucune région.");
		lines.Insert("");
		foreach (string line : m_aReport)
			lines.Insert(line);
		lines.Insert("");
		lines.Insert("État des officiers :");
		if (m_aRegions.IsEmpty())
			lines.Insert("   aucune région tracée");
		foreach (int i, SRP_CmdRegion reg : m_aRegions)
		{
			float alert = CurrentAlert(reg, now);
			int alertShown = Math.Round(alert);
			lines.Insert(string.Format("   %1 — %2 ; officier : %3 ; alerte %4", reg.m_sCode, RegionStateText(reg, now), OfficerText(reg, now), alertShown));
		}
		if (!m_aReadNotes.IsEmpty())
		{
			lines.Insert("");
			lines.Insert("Relecture de front.json :");
			foreach (string note : m_aReadNotes)
				lines.Insert("   " + note);
		}

		if (FileIO.FileExists(REPORT_PATH))
			FileIO.DeleteFile(REPORT_PATH);
		FileHandle file = FileIO.OpenFile(REPORT_PATH, FileMode.WRITE);
		if (!file)
		{
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Régions : écriture impossible de " + REPORT_PATH);
			return;
		}
		foreach (string text : lines)
			file.WriteLine(text);
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Lit les lignes « region S12 -> R3 » de front_retouches.txt et les applique dans l'ordre (refus notés au
	//! rapport : zone inconnue ou base, région inconnue, zone qui ne touche pas R3, région de départ vidée ou coupée)
	//! — Trace
	protected void ApplyRetouches()
	{
		string path = SRP_FrontComponent.RETOUCH_PATH;
		if (!FileIO.FileExists(path))
			return;
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
		{
			m_aTraceNotes.Insert("front_retouches.txt illisible : aucune retouche de région");
			return;
		}

		// Lecture ligne à ligne comme le socle : commentaire après #, lignes vides ignorées, seules les lignes « region »
		array<string> bodies = {};
		array<int> numbers = {};
		string line;
		string body;
		string lower;
		int cut;
		int lineNumber = 0;
		while (file.ReadLine(line) >= 0)
		{
			lineNumber++;
			body = line;
			cut = body.IndexOf("#");
			if (cut >= 0)
				body = body.Substring(0, cut);
			body = body.Trim();
			if (body.IsEmpty())
				continue;
			lower = body;
			lower.ToLower();
			if (!lower.StartsWith("region ") && !lower.StartsWith("région "))
				continue;
			bodies.Insert(body);
			numbers.Insert(lineNumber);
		}
		file.Close();

		int applied = 0;
		int refused = 0;
		foreach (int k, string text : bodies)
		{
			string refusal = ApplyRetouche(text, numbers[k]);
			if (refusal.IsEmpty())
			{
				applied++;
				continue;
			}
			refused++;
			string note = string.Format("Retouche ligne %1 refusée (« %2 ») : %3", numbers[k], text, refusal);
			m_aTraceNotes.Insert(note);
			SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Régions : " + note);
		}
		if (applied > 0 || refused > 0)
			m_aTraceNotes.Insert(string.Format("Retouches « region » : %1 appliquée(s), %2 refusée(s)", applied, refused));
	}

	//------------------------------------------------------------------------------------------------
	//! Numéro du jour de l'officier : (now + m_iTzOffset - officier_changement_heure x 3600) / 86400 — cachettes
	protected int DayIndex(int nowUnix)
	{
		return (nowUnix + m_iTzOffset - m_iOfficerDayHour * 3600) / 86400;
	}

	//------------------------------------------------------------------------------------------------
	//! OF2 : cachette du jour parmi les officier_cachettes localités hostiles les plus profondes de la région (point
	//! QG du socle en ville, sinon bâtiment proche du point clé), sans reprendre la précédente — Tick
	protected void ChooseHideout(SRP_CmdRegion region, int nowUnix)
	{
		if (!region || !region.m_Officer)
			return;
		SRP_CmdOfficer officer = region.m_Officer;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return;
		EnsureGraph(front);

		string oldLabel = HideLabel(officer);
		int oldDay = officer.m_iHideDay;
		string previous = officer.m_sHideLocality;
		if (previous.IsEmpty())
			previous = officer.m_sPrevLocality;

		array<int> depth = {};
		ComputeDepths(front, depth);
		array<int> choices = {};
		int count = HideoutCandidates(front, region, depth, choices);
		officer.m_aHideChoices.Clear();

		if (count > 0)
		{
			array<int> pool = {};
			foreach (int li : choices)
			{
				SRP_FrontLocality loc = front.GetLocality(li);
				officer.m_aHideChoices.Insert(loc.m_sName);
				if (loc.m_sName != previous)
					pool.Insert(li);
			}
			if (pool.IsEmpty())
				pool.Copy(choices);
			int chosen = pool.GetRandomElement();
			SRP_FrontLocality picked = front.GetLocality(chosen);
			if (!officer.m_sHideLocality.IsEmpty() && officer.m_sHideLocality != picked.m_sName)
				officer.m_sPrevLocality = officer.m_sHideLocality;
			officer.m_sHideLocality = picked.m_sName;
			officer.m_vHidePos = LocalityHidePoint(front, chosen);
		}
		else
		{
			// Sans localité hostile : la zone rouge la plus profonde de la région, au bâtiment le plus proche de son centre
			int zone = DeepestRedZone(front, region.m_iIndex, depth);
			if (zone > 0)
			{
				string zoneCode = front.GetZoneCode(zone);
				if (!officer.m_sHideLocality.IsEmpty() && officer.m_sHideLocality != zoneCode)
					officer.m_sPrevLocality = officer.m_sHideLocality;
				officer.m_sHideLocality = zoneCode;
				officer.m_aHideChoices.Insert(front.GetZoneLabel(zone));
				officer.m_vHidePos = ZoneHidePoint(zone);
			}
			else
			{
				officer.m_sHideLocality = "";
				officer.m_vHidePos = vector.Zero;
			}
		}
		officer.m_iHideDay = DayIndex(nowUnix);

		string newLabel = HideLabel(officer);
		if (newLabel == oldLabel && oldDay == officer.m_iHideDay)
			return;
		if (newLabel.IsEmpty())
			SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 sans cachette (plus aucune zone rouge dans la région)", region.m_sCode, officer.m_sName));
		else
			SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : cachette du jour de l'officier ennemi %2 : %3 (choisie parmi %4)", region.m_sCode, officer.m_sName, newLabel, JoinNames(officer.m_aHideChoices)));
		front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Pose hors de vue de l'officier (SpawnCharacterGroup) et de ses officier_gardes gardes, place demandée à la
	//! capacité (Ask GARDE, « officier-R3 »), Tag GARDE, rôles OFFICIER et GARDE_OFFICIER ; faux s'il faut réessayer
	//! — Tick
	protected bool PlaceOfficer(SRP_CmdRegion region, int nowUnix)
	{
		if (!region || !region.m_Officer)
			return false;
		SRP_CmdOfficer officer = region.m_Officer;
		if (officer.m_iState != SRP_ECmdOfficerState.VIVANT || !officer.m_aGroups.IsEmpty())
			return false;
		if (region.m_iState == SRP_ECmdRegionState.LIBEREE || officer.m_vHidePos == vector.Zero)
			return false;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return false;

		// Un joueur actif, hors de la base, à officier_apparition_m ou moins de la cachette
		array<IEntity> active = enemies.GetActivePlayers();
		bool wanted = false;
		foreach (IEntity player : active)
		{
			if (!player)
				continue;
			vector at = player.GetOrigin();
			if (SRP_Placement.IsNearBase(at))
				continue;
			if (vector.Distance(at, officer.m_vHidePos) <= m_iOfficerSpawnM)
			{
				wanted = true;
				break;
			}
		}
		if (!wanted)
			return false;

		int soldiers = 1 + m_iOfficerGuards;
		int groups = 1;
		if (m_iOfficerGuards > 0)
			groups = 2;

		// Place du jeu, avec la marge de l'officier
		AIWorld aiWorld = GetGame().GetAIWorld();
		if (aiWorld)
		{
			int room = aiWorld.GetLimitOfActiveAIs() - aiWorld.GetCurrentNumOfActiveAIs();
			if (room < soldiers + m_iOfficerAiMargin)
				return DeferPost(region, nowUnix, string.Format("limite du jeu : %1 place(s) libre(s), %2 voulue(s)", room, soldiers + m_iOfficerAiMargin));
		}

		// Jamais sous les yeux d'un joueur
		array<IEntity> living = LivingPlayers();
		if (SRP_Placement.IsSeenByAnyPlayer(officer.m_vHidePos, living))
		{
			officer.m_iDeferUntil = nowUnix + RETRY_S;
			return false;
		}

		// Place dans les 120 soldats (RE11)
		string refusal = SRP_CmdCapacity.Get().Ask("officier-" + region.m_sCode, SRP_ECmdCapClass.GARDE, soldiers, groups, officer.m_vHidePos, "territoire");
		if (!refusal.IsEmpty())
			return DeferPost(region, nowUnix, refusal);

		ResourceName prefab = PickOfficerPrefab();
		if (prefab.IsEmpty())
			return DeferPost(region, nowUnix, "aucun prefab d'officier (officier_prefab vide et liste des officiers de mission vide)");
		SRP_EnemyGroup record = enemies.SpawnCharacterGroup(prefab, officer.m_vHidePos, "territoire");
		if (!record)
			return DeferPost(region, nowUnix, "pose refusée par le jeu");
		record.m_bBudget = true;
		record.m_iCmdRole = SRP_ECmdRole.OFFICIER;
		SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.GARDE);
		officer.m_aGroups.Insert(record);

		// Les gardes, tout près, hors de vue
		int guardsPosted = 0;
		if (m_iOfficerGuards > 0)
		{
			vector guardPos;
			if (!SRP_Placement.FindSpawnPosition(officer.m_vHidePos, 5, 25, active, false, true, guardPos, 300))
				guardPos = officer.m_vHidePos;
			ResourceName guardPrefab = PickGuardPrefab(enemies);
			if (!guardPrefab.IsEmpty())
			{
				SRP_EnemyGroup guards = enemies.SpawnGroup(guardPos, true, officer.m_vHidePos, "territoire", 20, vector.Zero, -1, false, guardPrefab);
				if (guards)
				{
					enemies.ApplyRole(guards, SRP_ECRXRole.GARNISON, officer.m_vHidePos, 20);
					enemies.NoteHome(guards, "garde officier", true);
					guards.m_bBudget = true;
					guards.m_iCmdRole = SRP_ECmdRole.GARDE_OFFICIER;
					SRP_CmdCapacity.Tag(guards, SRP_ECmdCapClass.GARDE);
					officer.m_aGroups.Insert(guards);
					guardsPosted = m_iOfficerGuards;
				}
			}
		}

		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(record, members);
		officer.m_Entity = null;
		if (!members.IsEmpty())
			officer.m_Entity = members[0];
		officer.m_iNoPlayerSince = 0;
		officer.m_iUnconsciousSince = 0;
		officer.m_iCaptiveIdleSince = 0;
		officer.m_bFleeing = false;
		officer.m_bWeDelete = false;
		officer.m_sFleeLocality = "";
		officer.m_iPostLogUnix = 0;
		SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 posé à %3 (%4 garde(s)), un joueur approche", region.m_sCode, officer.m_sName, HideLabel(officer), guardsPosted));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Officier posé : retrait (personne à officier_retrait_m depuis officier_retrait_min, non vu), fuite
	//! (officier_fuite_m), évasion (officier_echappe_m), chute (mort, ACE_Captives_IsCaptive, inconscient abandonné),
	//! captif retiré (officier_captif_retrait_min) — Tick
	protected void WatchOfficer(SRP_CmdRegion region, int nowUnix)
	{
		if (!region || !region.m_Officer)
			return;
		SRP_CmdOfficer officer = region.m_Officer;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || officer.m_aGroups.IsEmpty())
			return;
		SRP_EnemyGroup own = officer.m_aGroups[0];

		if (!officer.m_Entity && own)
		{
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(own, members);
			if (!members.IsEmpty())
				officer.m_Entity = members[0];
		}
		IEntity entity = officer.m_Entity;
		if (!entity || entity.IsDeleted())
		{
			// Supprimé sans mort (éviction du moteur, retrait) : ce n'est PAS une chute (OF9)
			if (!officer.m_bWeDelete)
				SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 retiré par le jeu sans être tombé ; il redevient « sur le papier »", region.m_sCode, officer.m_sName));
			RetireGroups(officer);
			officer.m_iDeferUntil = nowUnix + RETRY_S * 2;
			return;
		}

		// Chute : mort, puis menottes ACE (C7 : aucune autre action ne vaut capture)
		if (SRP_Utils.IsDead(entity))
		{
			OfficerDown(region.m_iIndex, false, "tué", "");
			return;
		}
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(entity.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.ACE_Captives_IsCaptive())
		{
			OfficerDown(region.m_iIndex, true, "capturé", "");
			return;
		}

		array<IEntity> players = LivingPlayers();
		vector pos = entity.GetOrigin();

		// Inconscient : laissé pour mort si personne n'est à officier_abandon_m pendant officier_inconscient_min
		if (controller && controller.GetLifeState() == ECharacterLifeState.INCAPACITATED)
		{
			if (AnyPlayerWithin(pos, m_iOfficerAbandonM, players) || officer.m_iUnconsciousSince == 0)
				officer.m_iUnconsciousSince = nowUnix;
			else if (nowUnix - officer.m_iUnconsciousSince >= m_iOfficerUnconsciousMin * 60)
				OfficerDown(region.m_iIndex, false, "laissé pour mort", "");
			return;
		}
		officer.m_iUnconsciousSince = 0;

		// Fuite (OF2, OF10) : un joueur à officier_fuite_m, l'officier conscient et libre ; ses gardes restent
		float nearest = SRP_Utils.NearestPlayerDistance(pos, players);
		if (!officer.m_bFleeing && nearest <= m_iOfficerFleeM)
			StartFlee(region, pos, players, nowUnix);
		if (officer.m_bFleeing)
		{
			if (vector.Distance(pos, officer.m_vHidePos) > m_iOfficerEscapeM && nearest > ESCAPE_CLEAR_M && !SRP_Placement.IsSeenByAnyPlayer(pos, players))
			{
				Escape(region, nowUnix);
				return;
			}
			if (vector.DistanceXZ(pos, officer.m_vFleeTo) < ARRIVED_M)
				FleeArrived(region, nowUnix);
		}

		// Retrait hors de vue : personne à officier_retrait_m depuis officier_retrait_min
		if (nearest <= m_iOfficerDespawnM)
		{
			officer.m_iNoPlayerSince = 0;
			return;
		}
		if (officer.m_iNoPlayerSince == 0)
		{
			officer.m_iNoPlayerSince = nowUnix;
			return;
		}
		if (nowUnix - officer.m_iNoPlayerSince < m_iOfficerDespawnMin * 60)
			return;
		foreach (SRP_EnemyGroup record : officer.m_aGroups)
		{
			if (IsGroupSeen(record, players))
				return;
		}
		RetireGroups(officer);
		SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 retiré hors de vue (personne à %3 m depuis %4 min), toujours vivant « sur le papier »", region.m_sCode, officer.m_sName, m_iOfficerDespawnM, m_iOfficerDespawnMin));
	}

	//------------------------------------------------------------------------------------------------
	//! Nom d'officier tiré dans officier_noms, jamais porté en ce moment ni dans les officier_noms_memoire derniers —
	//! NewOfficer
	protected string PickName()
	{
		array<string> names = {};
		SplitNames(m_sOfficerNames, names);
		array<string> busy = {};
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (reg && reg.m_Officer && !reg.m_Officer.m_sName.IsEmpty())
				busy.Insert(reg.m_Officer.m_sName);
		}
		array<string> recent = {};
		int first = m_aUsedNames.Count() - m_iOfficerNamesMemory;
		if (first < 0)
			first = 0;
		for (int i = first; i < m_aUsedNames.Count(); i++)
			recent.Insert(m_aUsedNames[i]);

		array<string> pool = {};
		foreach (string name : names)
		{
			if (!busy.Contains(name) && !recent.Contains(name))
				pool.Insert(name);
		}
		// Liste trop courte : on relâche la mémoire, jamais un nom en poste si possible
		if (pool.IsEmpty())
		{
			foreach (string other : names)
			{
				if (!busy.Contains(other))
					pool.Insert(other);
			}
		}
		if (pool.IsEmpty())
			pool.Copy(names);
		if (pool.IsEmpty())
			return "Ivanov";
		return pool.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Caractère tiré selon cmd_trait_poids du cerveau, jamais celui du prédécesseur — NewOfficer
	protected int PickTrait(SRP_CmdRegion region)
	{
		array<int> weights = {};
		TraitWeights(weights);
		int previous = -1;
		if (region)
			previous = region.m_iPrevTrait;

		// Officiers vivants des autres régions, par caractère (pas un 3e du même caractère en même temps)
		array<int> same = {0, 0, 0, 0};
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			if (!reg || reg == region || !reg.m_Officer || reg.m_Officer.m_iState != SRP_ECmdOfficerState.VIVANT)
				continue;
			int held = reg.m_Officer.m_iTrait;
			if (held >= 0 && held < 4)
				same[held] = same[held] + 1;
		}

		// Trois essais de plus en plus larges : règles complètes, puis sans la règle du 3e, puis poids nuls admis
		array<int> pool = {};
		array<int> poolWeights = {};
		for (int attempt = 0; attempt < 3; attempt++)
		{
			pool.Clear();
			poolWeights.Clear();
			for (int t = 0; t < 4; t++)
			{
				if (t == previous)
					continue;
				if (attempt == 0 && same[t] >= 2)
					continue;
				int weight = weights[t];
				if (weight <= 0)
				{
					if (attempt < 2)
						continue;
					weight = 1;
				}
				pool.Insert(t);
				poolWeights.Insert(weight);
			}
			if (!pool.IsEmpty())
				break;
		}
		if (pool.IsEmpty())
			return SRP_ECmdTrait.METHODIQUE;

		int total = 0;
		foreach (int w : poolWeights)
			total += w;
		int roll = Math.RandomInt(0, total);
		for (int j = 0; j < pool.Count(); j++)
		{
			roll -= poolWeights[j];
			if (roll < 0)
				return pool[j];
		}
		return pool[pool.Count() - 1];
	}

	//================================================================================================
	// Interne : tracé
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Graphe des zones lu sur le socle (voisines par un côté ou par la mer, centres, carrés) — Trace, EnsureGraph
	protected void BuildGraph(SRP_FrontComponent front)
	{
		m_aAdjStart.Clear();
		m_aAdj.Clear();
		m_aZonePos.Clear();
		m_aZoneCells.Clear();
		int zoneCount = front.GetZoneCount();
		for (int z = 0; z < zoneCount; z++)
		{
			m_aAdjStart.Insert(m_aAdj.Count());
			SRP_FrontZone fz = front.GetZone(z);
			if (!fz)
			{
				m_aZonePos.Insert(vector.Zero);
				m_aZoneCells.Insert(0);
				continue;
			}
			m_aZonePos.Insert(fz.m_vCentroid);
			m_aZoneCells.Insert(fz.m_aCells.Count());
			foreach (int n : fz.m_aNeighbours)
			{
				if (n >= 0 && n < zoneCount && n != z)
					m_aAdj.Insert(n);
			}
		}
		m_aAdjStart.Insert(m_aAdj.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Graphe présent et à la taille du socle (relu si besoin) — Start, Tick, cachettes, rattachements
	protected void EnsureGraph(SRP_FrontComponent front)
	{
		if (m_aAdjStart.Count() != front.GetZoneCount() + 1)
			BuildGraph(front);
	}

	//------------------------------------------------------------------------------------------------
	//! Graines : la zone la plus éloignée de la base, puis la plus éloignée de la graine la plus proche (à égalité, le
	//! plus petit numéro), parmi les zones qui ont une voisine hors base (îles sans liaison exclues) — Trace
	protected void PickSeeds(vector basePos, notnull array<int> seeds)
	{
		seeds.Clear();
		int zoneCount = m_aZonePos.Count();
		array<int> candidates = {};
		for (int z = 1; z < zoneCount; z++)
		{
			if (HasZoneNeighbour(z))
				candidates.Insert(z);
		}
		if (candidates.IsEmpty())
		{
			for (int y = 1; y < zoneCount; y++)
				candidates.Insert(y);
		}
		int wanted = m_iRegionsWanted;
		if (wanted > candidates.Count())
			wanted = candidates.Count();
		if (wanted <= 0)
			return;

		int first = -1;
		float farthest = -1;
		foreach (int c : candidates)
		{
			float d = vector.DistanceXZ(m_aZonePos[c], basePos);
			if (d > farthest)
			{
				farthest = d;
				first = c;
			}
		}
		seeds.Insert(first);

		while (seeds.Count() < wanted)
		{
			int nextSeed = -1;
			float best = -1;
			foreach (int cand : candidates)
			{
				if (seeds.Contains(cand))
					continue;
				float closest = -1;
				foreach (int s : seeds)
				{
					float ds = vector.DistanceXZ(m_aZonePos[cand], m_aZonePos[s]);
					if (closest < 0 || ds < closest)
						closest = ds;
				}
				if (closest > best)
				{
					best = closest;
					nextSeed = cand;
				}
			}
			if (nextSeed < 0)
				break;
			seeds.Insert(nextSeed);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Croissance : la région qui a le moins de zones (puis de carrés, puis le plus petit index) et touche une zone
	//! libre prend la zone libre voisine la plus proche de sa graine ; une zone jamais atteinte va à la graine la plus
	//! proche — Trace
	protected void Grow(notnull array<int> owner, notnull array<int> seeds)
	{
		int regionCount = seeds.Count();
		int zoneCount = owner.Count();
		array<int> zonesOf = {};
		array<int> cellsOf = {};
		for (int r = 0; r < regionCount; r++)
		{
			zonesOf.Insert(1);
			cellsOf.Insert(m_aZoneCells[seeds[r]]);
		}
		int freeLeft = 0;
		for (int z = 1; z < zoneCount; z++)
		{
			if (owner[z] < 0)
				freeLeft++;
		}

		while (freeLeft > 0)
		{
			int bestRegion = -1;
			int bestZone = -1;
			for (int q = 0; q < regionCount; q++)
			{
				int candidate = ClosestFreeNeighbour(owner, q, m_aZonePos[seeds[q]]);
				if (candidate < 0)
					continue;
				if (bestRegion < 0 || zonesOf[q] < zonesOf[bestRegion] || (zonesOf[q] == zonesOf[bestRegion] && cellsOf[q] < cellsOf[bestRegion]))
				{
					bestRegion = q;
					bestZone = candidate;
				}
			}
			if (bestRegion < 0)
				break;
			owner[bestZone] = bestRegion;
			zonesOf[bestRegion] = zonesOf[bestRegion] + 1;
			cellsOf[bestRegion] = cellsOf[bestRegion] + m_aZoneCells[bestZone];
			freeLeft--;
		}

		// Îles sans liaison : région de la graine la plus proche
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		for (int i = 1; i < zoneCount; i++)
		{
			if (owner[i] >= 0)
				continue;
			int nearestRegion = 0;
			float nearestDist = -1;
			for (int s = 0; s < regionCount; s++)
			{
				float d = vector.DistanceXZ(m_aZonePos[i], m_aZonePos[seeds[s]]);
				if (nearestDist < 0 || d < nearestDist)
				{
					nearestDist = d;
					nearestRegion = s;
				}
			}
			owner[i] = nearestRegion;
			if (front)
				m_aTraceNotes.Insert(string.Format("Zone %1 jamais atteinte par ses voisines : donnée à la région de la graine la plus proche", front.GetZoneLabel(i)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Zone libre voisine de la région la plus proche de sa graine (à égalité, le plus petit numéro), -1 sinon — Grow
	protected int ClosestFreeNeighbour(notnull array<int> owner, int region, vector seedPos)
	{
		int best = -1;
		float bestDist = 0;
		int zoneCount = owner.Count();
		for (int z = 1; z < zoneCount; z++)
		{
			if (owner[z] != region)
				continue;
			int firstAdj = m_aAdjStart[z];
			int lastAdj = m_aAdjStart[z + 1];
			for (int a = firstAdj; a < lastAdj; a++)
			{
				int n = m_aAdj[a];
				if (n <= 0 || owner[n] >= 0)
					continue;
				float d = vector.DistanceXZ(m_aZonePos[n], seedPos);
				if (best < 0 || d < bestDist || (d == bestDist && n < best))
				{
					best = n;
					bestDist = d;
				}
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Équilibrage entre minZones et maxZones, 300 passes au plus (un échange par passe) ; rend les passes faites ;
	//! aucun échange possible : avertissement et tracé gardé — Trace
	protected int Balance(notnull array<int> owner, notnull array<int> seeds, int minZones, int maxZones)
	{
		int regionCount = seeds.Count();
		for (int pass = 0; pass < BALANCE_PASSES; pass++)
		{
			array<int> counts = {};
			for (int r = 0; r < regionCount; r++)
				counts.Insert(CountOwned(owner, r));
			bool outOfRange = false;
			foreach (int cnt : counts)
			{
				if (cnt < minZones || cnt > maxZones)
					outOfRange = true;
			}
			if (!outOfRange)
				return pass;
			if (!BalanceStep(owner, seeds, counts, minZones, maxZones))
			{
				m_aTraceNotes.Insert(string.Format("Équilibrage arrêté après %1 échange(s) : plus aucun échange possible entre régions voisines, tracé gardé tel quel", pass));
				return pass;
			}
		}
		m_aTraceNotes.Insert(string.Format("Équilibrage arrêté après %1 passes, tracé gardé tel quel", BALANCE_PASSES));
		return BALANCE_PASSES;
	}

	//------------------------------------------------------------------------------------------------
	//! Un échange : une région sous le minimum reçoit d'une voisine au-dessus du minimum (la plus grosse d'abord) ;
	//! sinon une région au-dessus du maximum cède à sa voisine la plus petite ; zone de frontière qui ne coupe pas la
	//! donneuse, la plus proche de la graine de la receveuse ; faux si rien n'est possible — Balance
	protected bool BalanceStep(notnull array<int> owner, notnull array<int> seeds, notnull array<int> counts, int minZones, int maxZones)
	{
		int regionCount = seeds.Count();

		// Régions de la plus petite à la plus grosse (à égalité, le plus petit index d'abord)
		array<int> ascending = {};
		for (int r = 0; r < regionCount; r++)
		{
			int pos = 0;
			while (pos < ascending.Count() && counts[ascending[pos]] <= counts[r])
				pos++;
			ascending.InsertAt(r, pos);
		}

		// Sous le minimum : reçoit d'une voisine qui a plus que le minimum, la plus grosse d'abord
		foreach (int receiver : ascending)
		{
			if (counts[receiver] >= minZones)
				continue;
			array<int> donors = {};
			NeighbourRegions(owner, receiver, regionCount, donors);
			array<int> biggest = {};
			foreach (int d : donors)
			{
				if (counts[d] <= minZones)
					continue;
				int at = 0;
				while (at < biggest.Count() && counts[biggest[at]] >= counts[d])
					at++;
				biggest.InsertAt(d, at);
			}
			foreach (int donor : biggest)
			{
				int given = PickGiveZone(owner, seeds, donor, receiver);
				if (given < 0)
					continue;
				owner[given] = receiver;
				return true;
			}
		}

		// Régions de la plus grosse à la plus petite (à égalité, le plus petit index d'abord)
		array<int> descending = {};
		for (int g = 0; g < regionCount; g++)
		{
			int place = 0;
			while (place < descending.Count() && counts[descending[place]] >= counts[g])
				place++;
			descending.InsertAt(g, place);
		}

		// Au-dessus du maximum : cède à sa voisine la plus petite
		foreach (int giver : descending)
		{
			if (counts[giver] <= maxZones)
				continue;
			array<int> takers = {};
			NeighbourRegions(owner, giver, regionCount, takers);
			array<int> smallest = {};
			foreach (int t : takers)
			{
				if (counts[t] >= maxZones)
					continue;
				int slot = 0;
				while (slot < smallest.Count() && counts[smallest[slot]] <= counts[t])
					slot++;
				smallest.InsertAt(t, slot);
			}
			foreach (int taker : smallest)
			{
				int ceded = PickGiveZone(owner, seeds, giver, taker);
				if (ceded < 0)
					continue;
				owner[ceded] = taker;
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone que donor peut céder à receiver : à lui, pas sa graine, voisine de receiver, sans couper donor ; la plus
	//! proche de la graine de receiver (à égalité, le plus petit numéro) ; -1 sinon — BalanceStep
	protected int PickGiveZone(notnull array<int> owner, notnull array<int> seeds, int donor, int receiver)
	{
		int best = -1;
		float bestDist = 0;
		vector target = m_aZonePos[seeds[receiver]];
		int zoneCount = owner.Count();
		int before = CountComponents(owner, donor, -1);
		for (int z = 1; z < zoneCount; z++)
		{
			if (owner[z] != donor || z == seeds[donor])
				continue;
			if (!TouchesRegion(owner, z, receiver))
				continue;
			float d = vector.DistanceXZ(m_aZonePos[z], target);
			if (best >= 0 && (d > bestDist || (d == bestDist && z > best)))
				continue;
			if (CountComponents(owner, donor, z) > before)
				continue;
			best = z;
			bestDist = d;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Régions voisines de region (index croissant) — BalanceStep
	protected void NeighbourRegions(notnull array<int> owner, int region, int regionCount, notnull array<int> result)
	{
		result.Clear();
		array<bool> touch = {};
		for (int r = 0; r < regionCount; r++)
			touch.Insert(false);
		int zoneCount = owner.Count();
		for (int z = 1; z < zoneCount; z++)
		{
			if (owner[z] != region)
				continue;
			int firstAdj = m_aAdjStart[z];
			int lastAdj = m_aAdjStart[z + 1];
			for (int a = firstAdj; a < lastAdj; a++)
			{
				int n = m_aAdj[a];
				if (n <= 0)
					continue;
				int other = owner[n];
				if (other >= 0 && other < regionCount && other != region)
					touch[other] = true;
			}
		}
		for (int k = 0; k < regionCount; k++)
		{
			if (touch[k])
				result.Insert(k);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La zone touche-t-elle une zone de la région (par un côté ou la mer) ? — PickGiveZone, retouches
	protected bool TouchesRegion(notnull array<int> owner, int zone, int region)
	{
		int firstAdj = m_aAdjStart[zone];
		int lastAdj = m_aAdjStart[zone + 1];
		for (int a = firstAdj; a < lastAdj; a++)
		{
			int n = m_aAdj[a];
			if (n > 0 && owner[n] == region)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Morceaux d'une région (parcours en largeur sur ses zones), la zone skip mise de côté — coupure d'une région
	protected int CountComponents(notnull array<int> owner, int region, int skip)
	{
		int zoneCount = owner.Count();
		array<bool> seen = {};
		for (int i = 0; i < zoneCount; i++)
			seen.Insert(false);
		array<int> queue = {};
		int components = 0;
		for (int z = 1; z < zoneCount; z++)
		{
			if (owner[z] != region || z == skip || seen[z])
				continue;
			components++;
			queue.Clear();
			queue.Insert(z);
			seen[z] = true;
			int head = 0;
			while (head < queue.Count())
			{
				int current = queue[head];
				head++;
				int firstAdj = m_aAdjStart[current];
				int lastAdj = m_aAdjStart[current + 1];
				for (int a = firstAdj; a < lastAdj; a++)
				{
					int n = m_aAdj[a];
					if (n <= 0 || n == skip || owner[n] != region || seen[n])
						continue;
					seen[n] = true;
					queue.Insert(n);
				}
			}
		}
		return components;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones tenues par la région dans owner — Balance, retouches
	protected int CountOwned(notnull array<int> owner, int region)
	{
		int count = 0;
		for (int z = 1; z < owner.Count(); z++)
		{
			if (owner[z] == region)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Centre de la région dans owner (centres des zones pondérés par leurs carrés) — Number, Trace
	protected vector RegionCentroid(int region, notnull array<int> owner)
	{
		vector sum = vector.Zero;
		float weight = 0;
		for (int z = 1; z < owner.Count(); z++)
		{
			if (owner[z] != region)
				continue;
			float w = m_aZoneCells[z];
			if (w <= 0)
				w = 1;
			sum = sum + m_aZonePos[z] * w;
			weight += w;
		}
		if (weight <= 0)
			return vector.Zero;
		return sum * (1.0 / weight);
	}

	//------------------------------------------------------------------------------------------------
	//! Numérotation : R1 = centre le plus proche de la base, puis dans l'ordre (à égalité, la graine au plus petit
	//! numéro) ; crée les régions et remplit m_aHomeRegion — Trace
	protected void Number(notnull array<int> owner, notnull array<int> seeds, vector basePos)
	{
		int regionCount = seeds.Count();
		array<float> dists = {};
		for (int r = 0; r < regionCount; r++)
			dists.Insert(vector.DistanceXZ(RegionCentroid(r, owner), basePos));

		array<int> order = {};
		for (int t = 0; t < regionCount; t++)
		{
			int pos = 0;
			while (pos < order.Count())
			{
				int other = order[pos];
				bool before = dists[t] < dists[other] || (dists[t] == dists[other] && seeds[t] < seeds[other]);
				if (before)
					break;
				pos++;
			}
			order.InsertAt(t, pos);
		}

		array<int> rank = {};
		for (int q = 0; q < regionCount; q++)
			rank.Insert(0);
		foreach (int place, int temp : order)
		{
			rank[temp] = place;
			SRP_CmdRegion reg = new SRP_CmdRegion();
			reg.m_iIndex = place;
			reg.m_sCode = string.Format("R%1", place + 1);
			reg.m_iSeedZone = seeds[temp];
			m_aRegions.Insert(reg);
		}
		for (int z = 1; z < owner.Count(); z++)
		{
			if (owner[z] >= 0 && owner[z] < regionCount)
				m_aHomeRegion[z] = rank[owner[z]];
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Une retouche « region S12 -> R3 » : "" si appliquée, sinon la raison du refus — ApplyRetouches
	protected string ApplyRetouche(string text, int lineNumber)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return "front absent";
		int space = text.IndexOf(" ");
		if (space < 0)
			return "ligne incomplète";
		string rest = text.Substring(space + 1, text.Length() - space - 1);
		int arrow = rest.IndexOf("->");
		if (arrow < 0)
			return "flèche -> absente (forme attendue : region S12 -> R3)";
		string zoneText = rest.Substring(0, arrow);
		zoneText = zoneText.Trim();
		string regionText = rest.Substring(arrow + 2, rest.Length() - arrow - 2);
		regionText = regionText.Trim();

		int zone = front.FindZone(zoneText);
		if (zone < 0 || zone >= m_aHomeRegion.Count())
			return "zone inconnue : " + zoneText;
		if (zone == 0 || front.IsBaseZone(zone))
			return "la base n'appartient à aucune région";
		int target = FindRegion(regionText);
		if (target < 0)
			return "région inconnue : " + regionText;
		int source = m_aHomeRegion[zone];
		if (source == target)
		{
			m_aTraceNotes.Insert(string.Format("Retouche ligne %1 sans effet : %2 est déjà dans %3", lineNumber, front.GetZoneLabel(zone), m_aRegions[target].m_sCode));
			return "";
		}
		if (!TouchesRegion(m_aHomeRegion, zone, target))
			return string.Format("%1 ne touche aucune zone de %2", front.GetZoneLabel(zone), m_aRegions[target].m_sCode);
		SRP_CmdRegion from = GetRegion(source);
		if (from)
		{
			if (CountOwned(m_aHomeRegion, source) <= 1)
				return from.m_sCode + " serait vidée";
			if (CountComponents(m_aHomeRegion, source, zone) > CountComponents(m_aHomeRegion, source, -1))
				return from.m_sCode + " serait coupée en deux";
		}

		m_aHomeRegion[zone] = target;
		SRP_CmdRegion to = m_aRegions[target];
		if (from)
		{
			from.m_aZones.RemoveItemOrdered(zone);
			// La graine partie, la zone restante la plus proche d'elle la remplace (renseignement du rapport)
			if (from.m_iSeedZone == zone)
				from.m_iSeedZone = ClosestOwnedZone(m_aHomeRegion, source, m_aZonePos[zone]);
		}
		to.m_aZones.Insert(zone);
		to.m_aZones.Sort();
		string fromCode = "aucune région";
		if (from)
			fromCode = from.m_sCode;
		m_aTraceNotes.Insert(string.Format("Retouche ligne %1 appliquée : %2 passe de %3 à %4", lineNumber, front.GetZoneLabel(zone), fromCode, to.m_sCode));
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Zone de la région la plus proche d'un point (à égalité, le plus petit numéro), -1 sans zone — ApplyRetouche
	protected int ClosestOwnedZone(notnull array<int> owner, int region, vector point)
	{
		int best = -1;
		float bestDist = 0;
		for (int z = 1; z < owner.Count(); z++)
		{
			if (owner[z] != region)
				continue;
			float d = vector.DistanceXZ(m_aZonePos[z], point);
			if (best < 0 || d < bestDist)
			{
				best = z;
				bestDist = d;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! m_aZones de chaque région d'après owner (index croissant) — Trace, ReadFrom, remises
	protected void RebuildRegionZones(notnull array<int> owner)
	{
		foreach (SRP_CmdRegion reg : m_aRegions)
			reg.m_aZones.Clear();
		int regionCount = m_aRegions.Count();
		for (int z = 1; z < owner.Count(); z++)
		{
			int region = owner[z];
			if (region >= 0 && region < regionCount)
				m_aRegions[region].m_aZones.Insert(z);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! VU1 : plus grande localité de la région (ville, puis village, puis hameau ; à égalité, ordre alphabétique) ;
	//! sans localité, le nom de sa plus grande zone — Trace
	protected void NameRegions(SRP_FrontComponent front)
	{
		int localityCount = front.GetLocalityCount();
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			string bestName = "";
			int bestRank = -1;
			for (int li = 0; li < localityCount; li++)
			{
				SRP_FrontLocality loc = front.GetLocality(li);
				if (!loc || loc.m_bHome || loc.m_iZone <= 0 || loc.m_iZone >= m_aHomeRegion.Count())
					continue;
				if (m_aHomeRegion[loc.m_iZone] != reg.m_iIndex)
					continue;
				int rank = KindRank(loc);
				if (rank > bestRank || (rank == bestRank && SRP_Utils.CompareStrings(loc.m_sName, bestName) < 0))
				{
					bestRank = rank;
					bestName = loc.m_sName;
				}
			}
			if (bestName.IsEmpty())
			{
				int bestZone = -1;
				foreach (int zone : reg.m_aHomeZones)
				{
					if (bestZone < 0 || m_aZoneCells[zone] > m_aZoneCells[bestZone])
						bestZone = zone;
				}
				if (bestZone > 0)
					bestName = front.GetZoneName(bestZone);
			}
			if (bestName.IsEmpty())
				bestName = reg.m_sCode;
			reg.m_sName = bestName;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rang d'une localité : ville 3 (un bourg compte comme une ville), village 2, hameau 1 — NameRegions
	protected int KindRank(SRP_FrontLocality loc)
	{
		string kind = loc.m_sKind;
		kind.ToLower();
		if (kind == "ville" || kind == "bourg")
			return 3;
		if (kind == "village")
			return 2;
		if (kind == "hameau")
			return 1;
		return loc.m_iKind;
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes du tracé pour commandeur_regions.txt : réglages, une entrée par région, retouches et avertissements —
	//! Trace
	protected void BuildTraceReport(SRP_FrontComponent front, vector basePos, int passes, int maxZones)
	{
		m_aReport.Insert(string.Format("Tracé : %1 région(s) (voulues : %2), %3 zone(s) hors base, de %4 à %5 zones par région, %6 échange(s) d'équilibrage, somme de contrôle %7", m_aRegions.Count(), m_iRegionsWanted, m_aZonePos.Count() - 1, m_iRegionMinZones, maxZones, passes, m_iHash));
		int localityCount = front.GetLocalityCount();
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			int meters = Math.Round(vector.DistanceXZ(reg.m_vCentroid, basePos));
			m_aReport.Insert("");
			m_aReport.Insert(string.Format("%1 — %2 : %3 zone(s), graine %4, centre à %5 m de la base", reg.m_sCode, GetRegionLabel(reg.m_iIndex), reg.m_aHomeZones.Count(), front.GetZoneLabel(reg.m_iSeedZone), meters));
			m_aReport.Insert("   zones : " + ZoneList(front, reg.m_aHomeZones));
			string locs = "";
			for (int li = 0; li < localityCount; li++)
			{
				SRP_FrontLocality loc = front.GetLocality(li);
				if (!loc || loc.m_bHome || loc.m_iZone <= 0 || loc.m_iZone >= m_aHomeRegion.Count())
					continue;
				if (m_aHomeRegion[loc.m_iZone] != reg.m_iIndex)
					continue;
				if (!locs.IsEmpty())
					locs += ", ";
				locs += string.Format("%1 (%2)", loc.m_sName, loc.m_sKind);
			}
			if (locs.IsEmpty())
				locs = "aucune";
			m_aReport.Insert("   localités : " + locs);
			int count = reg.m_aHomeZones.Count();
			if (count < m_iRegionMinZones || count > maxZones)
				m_aTraceNotes.Insert(string.Format("%1 hors de la fourchette : %2 zone(s) pour %3 à %4", reg.m_sCode, count, m_iRegionMinZones, maxZones));
		}
		m_aReport.Insert("");
		if (m_aTraceNotes.IsEmpty())
		{
			m_aReport.Insert("Retouches et avertissements : aucun");
			return;
		}
		m_aReport.Insert("Retouches et avertissements :");
		foreach (string note : m_aTraceNotes)
			m_aReport.Insert("   " + note);
	}

	//------------------------------------------------------------------------------------------------
	//! La zone a-t-elle une voisine hors base ? — PickSeeds
	protected bool HasZoneNeighbour(int zone)
	{
		int firstAdj = m_aAdjStart[zone];
		int lastAdj = m_aAdjStart[zone + 1];
		for (int a = firstAdj; a < lastAdj; a++)
		{
			if (m_aAdj[a] > 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Heure locale moins heure UTC, en secondes, entre -12 et +12 h, au quart d'heure près — Trace
	protected int ComputeTzOffset()
	{
		int lh;
		int lm;
		int ls;
		int uh;
		int um;
		int us;
		System.GetHourMinuteSecond(lh, lm, ls);
		System.GetHourMinuteSecondUTC(uh, um, us);
		int diff = (lh * 3600 + lm * 60 + ls) - (uh * 3600 + um * 60 + us);
		if (diff > 43200)
			diff -= 86400;
		if (diff < -43200)
			diff += 86400;
		int quarters = Math.Round(diff / 900.0);
		return quarters * 900;
	}

	//------------------------------------------------------------------------------------------------
	//! Un pas de la somme de contrôle : (h x 131 + v) modulo 8388593 — Trace
	protected int HashStep(int h, int v)
	{
		int x = h * 131 + v;
		int q = x / HASH_MOD;
		int r = x - q * HASH_MOD;
		if (r < 0)
			r += HASH_MOD;
		return r;
	}

	//================================================================================================
	// Interne : officiers
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Toutes les régions à l'état neuf (tracé d'origine, ni officier, ni alerte, ni désorganisation) — ReadFrom,
	//! ResetCampaign
	protected void ResetStates()
	{
		for (int z = 0; z < m_aZoneRegion.Count() && z < m_aHomeRegion.Count(); z++)
			m_aZoneRegion[z] = m_aHomeRegion[z];
		foreach (SRP_CmdRegion reg : m_aRegions)
		{
			reg.m_iState = SRP_ECmdRegionState.ACTIVE;
			reg.m_iDesorgUntil = 0;
			reg.m_bDesorgCaptured = false;
			reg.m_iLiberatedAt = 0;
			reg.m_fAlert = 0;
			reg.m_iAlertAt = 0;
			reg.m_iAlertStep = 0;
			reg.m_Officer = null;
			reg.m_iPrevTrait = -1;
			reg.m_iMood = SRP_ECmdMood.NORMALE;
			reg.m_iReportsLost = 0;
			reg.m_iReportsDelayed = 0;
			reg.m_iLastReportUnix = 0;
			reg.m_sRadioCutLocality = "";
			reg.m_iRadioCutUntil = 0;
			reg.m_iLastFallUnix = 0;
			reg.m_iLastFallStatus = SRP_ECmdMissionStatus.SANS_OBJET;
		}
		RebuildRegionZones(m_aZoneRegion);
	}

	//------------------------------------------------------------------------------------------------
	//! Après un redémarrage ou une restauration : cachette gardée si elle vaut encore pour aujourd'hui (ou si une
	//! mission la vise), position et candidates recalculées ; sinon nouvelle cachette — Start, OnRestored
	protected void RefreshHideout(SRP_CmdRegion region, int nowUnix)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!officer || !front)
			return;
		EnsureGraph(front);
		bool keep = IsHideoutValid(front, region, officer) && (officer.m_iHideDay == DayIndex(nowUnix) || IsTargeted(region));
		if (!keep)
		{
			ChooseHideout(region, nowUnix);
			return;
		}
		array<int> depth = {};
		ComputeDepths(front, depth);
		array<int> choices = {};
		HideoutCandidates(front, region, depth, choices);
		officer.m_aHideChoices.Clear();
		foreach (int li : choices)
			officer.m_aHideChoices.Insert(front.GetLocality(li).m_sName);
		int locality = front.FindLocality(officer.m_sHideLocality);
		if (locality >= 0)
		{
			officer.m_vHidePos = LocalityHidePoint(front, locality);
			return;
		}
		int zone = front.FindZone(officer.m_sHideLocality);
		if (zone > 0)
		{
			officer.m_aHideChoices.Insert(front.GetZoneLabel(zone));
			officer.m_vHidePos = ZoneHidePoint(zone);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La cachette du jour vaut-elle encore ? Localité hostile de la région, ou zone rouge de la région — OnFrontChanged
	protected bool IsHideoutValid(SRP_FrontComponent front, SRP_CmdRegion region, SRP_CmdOfficer officer)
	{
		if (officer.m_sHideLocality.IsEmpty())
			return false;
		int locality = front.FindLocality(officer.m_sHideLocality);
		if (locality >= 0)
			return IsHostileLocality(front, front.GetLocality(locality), region.m_iIndex);
		int zone = front.FindZone(officer.m_sHideLocality);
		if (zone <= 0 || GetRegionOfZone(zone) != region.m_iIndex)
			return false;
		return front.GetZoneOwner(zone) == SRP_EFrontOwner.ROUGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité hostile de la région : hors base, point clé dans une zone rouge de la région, pas prise — cachettes
	protected bool IsHostileLocality(SRP_FrontComponent front, SRP_FrontLocality loc, int region)
	{
		if (!loc || loc.m_bHome)
			return false;
		int zone = loc.m_iZone;
		if (zone <= 0 || GetRegionOfZone(zone) != region)
			return false;
		if (front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE)
			return false;
		SRP_ZoneFall fall = front.GetZoneFall();
		if (fall && fall.IsLocalityTaken(loc.m_sName))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! OF2 : localités hostiles de la région, triées par profondeur décroissante puis par nom (celles au contact du
	//! front écartées s'il en reste d'autres), officier_cachettes premières ; rend leur nombre — cachettes
	protected int HideoutCandidates(SRP_FrontComponent front, SRP_CmdRegion region, notnull array<int> depth, notnull array<int> result)
	{
		result.Clear();
		array<int> found = {};
		array<int> depths = {};
		array<bool> contact = {};
		bool anyRear = false;
		int localityCount = front.GetLocalityCount();
		for (int li = 0; li < localityCount; li++)
		{
			SRP_FrontLocality loc = front.GetLocality(li);
			if (!IsHostileLocality(front, loc, region.m_iIndex))
				continue;
			bool inContact = front.IsZoneInContact(loc.m_iZone);
			int d = DEPTH_NONE;
			if (loc.m_iZone < depth.Count())
				d = depth[loc.m_iZone];
			found.Insert(li);
			depths.Insert(d);
			contact.Insert(inContact);
			if (!inContact)
				anyRear = true;
		}

		array<int> order = {};
		for (int k = 0; k < found.Count(); k++)
		{
			if (anyRear && contact[k])
				continue;
			int pos = 0;
			while (pos < order.Count())
			{
				int other = order[pos];
				bool before = depths[k] > depths[other];
				if (!before && depths[k] == depths[other])
					before = SRP_Utils.CompareStrings(front.GetLocality(found[k]).m_sName, front.GetLocality(found[other]).m_sName) < 0;
				if (before)
					break;
				pos++;
			}
			order.InsertAt(k, pos);
		}
		for (int m = 0; m < order.Count() && m < m_iOfficerHideouts; m++)
			result.Insert(found[order[m]]);
		return result.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Profondeur de chaque zone : distance en zones jusqu'à la zone bleue la plus proche (base comprise), par
	//! parcours en largeur sur les voisines ; DEPTH_NONE si aucune — cachettes
	protected void ComputeDepths(SRP_FrontComponent front, notnull array<int> depth)
	{
		depth.Clear();
		int zoneCount = m_aAdjStart.Count() - 1;
		array<int> queue = {};
		for (int z = 0; z < zoneCount; z++)
		{
			if (front.GetZoneOwner(z) == SRP_EFrontOwner.BLEU)
			{
				depth.Insert(0);
				queue.Insert(z);
			}
			else
				depth.Insert(DEPTH_NONE);
		}
		int head = 0;
		while (head < queue.Count())
		{
			int current = queue[head];
			head++;
			int firstAdj = m_aAdjStart[current];
			int lastAdj = m_aAdjStart[current + 1];
			for (int a = firstAdj; a < lastAdj; a++)
			{
				int n = m_aAdj[a];
				if (depth[n] <= depth[current] + 1)
					continue;
				depth[n] = depth[current] + 1;
				queue.Insert(n);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Zone rouge la plus profonde de la région (à égalité, le plus petit numéro), -1 sans zone rouge — cachettes
	protected int DeepestRedZone(SRP_FrontComponent front, int region, notnull array<int> depth)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg)
			return -1;
		int best = -1;
		foreach (int zone : reg.m_aZones)
		{
			if (front.GetZoneOwner(zone) != SRP_EFrontOwner.ROUGE || zone >= depth.Count())
				continue;
			if (best < 0 || depth[zone] > depth[best] || (depth[zone] == depth[best] && zone < best))
				best = zone;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Point de la cachette dans une localité : point QG du socle en ville, sinon un bâtiment près du point clé
	//! (rayon de la localité), sinon le point clé lui-même — cachettes, fuite
	protected vector LocalityHidePoint(SRP_FrontComponent front, int locality)
	{
		SRP_FrontLocality loc = front.GetLocality(locality);
		if (!loc)
			return vector.Zero;
		if (loc.m_iQg >= 0)
		{
			SRP_FrontKeyPoint hq = front.GetKeyPoint(loc.m_iQg);
			if (hq)
				return hq.m_vPos;
		}
		vector centre = loc.m_vLabel;
		if (loc.m_iCentre >= 0)
		{
			SRP_FrontKeyPoint key = front.GetKeyPoint(loc.m_iCentre);
			if (key)
				centre = key.m_vPos;
		}
		float radius = LocalityRadius(loc);
		vector spot;
		array<vector> none = {};
		if (SRP_Placement.FindBuildingSpot(centre, radius, none, spot))
			return spot;
		return SRP_Placement.OnGround(centre);
	}

	//------------------------------------------------------------------------------------------------
	//! Rayon d'une localité : celui du territoire s'il la connaît, sinon selon son genre — LocalityHidePoint
	protected float LocalityRadius(SRP_FrontLocality loc)
	{
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (territory)
		{
			SRP_SectorState state = territory.FindState(loc.m_sName);
			if (state && state.m_Sector && state.m_Sector.m_fRadius > 0)
				return state.m_Sector.m_fRadius;
		}
		int rank = KindRank(loc);
		if (rank >= 3)
			return 250;
		if (rank == 2)
			return 150;
		return 100;
	}

	//------------------------------------------------------------------------------------------------
	//! Point de la cachette sans localité : un bâtiment près du centre de la zone, sinon le centre au sol — cachettes
	protected vector ZoneHidePoint(int zone)
	{
		if (zone <= 0 || zone >= m_aZonePos.Count())
			return vector.Zero;
		vector centre = m_aZonePos[zone];
		vector spot;
		array<vector> none = {};
		if (SRP_Placement.FindBuildingSpot(centre, HIDE_ZONE_RADIUS, none, spot))
			return spot;
		return SRP_Placement.OnGround(centre);
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé de la cachette : nom de la localité, ou libellé de la zone (« Régina (S07) ») ; "" sans cachette —
	//! journal, écrans, missions
	protected string HideLabel(SRP_CmdOfficer officer)
	{
		if (!officer || officer.m_sHideLocality.IsEmpty())
			return "";
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front)
			return officer.m_sHideLocality;
		if (front.FindLocality(officer.m_sHideLocality) >= 0)
			return officer.m_sHideLocality;
		int zone = front.FindZone(officer.m_sHideLocality);
		if (zone > 0)
			return front.GetZoneLabel(zone);
		return officer.m_sHideLocality;
	}

	//------------------------------------------------------------------------------------------------
	//! Une mission OFFICIER active vise-t-elle cette région ? — cachettes, PickOfficerTarget
	protected bool IsTargeted(SRP_CmdRegion region)
	{
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions || !region)
			return false;
		return missions.IsOfficerTargeted(region.m_sCode);
	}

	//------------------------------------------------------------------------------------------------
	//! Pose remise à RETRY_S secondes ; la raison part au journal au plus une fois par 10 min — PlaceOfficer
	protected bool DeferPost(SRP_CmdRegion region, int nowUnix, string reason)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		officer.m_iDeferUntil = nowUnix + RETRY_S;
		if (officer.m_iPostLogUnix == 0 || nowUnix - officer.m_iPostLogUnix >= POST_LOG_S)
		{
			officer.m_iPostLogUnix = nowUnix;
			SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 pas encore posé à %3 : %4 (nouvel essai toutes les %5 s)", region.m_sCode, officer.m_sName, HideLabel(officer), reason, RETRY_S));
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab de l'officier : le réglage officier_prefab, sinon un des officiers de mission — PlaceOfficer
	protected ResourceName PickOfficerPrefab()
	{
		if (!m_sOfficerPrefab.IsEmpty())
			return m_sOfficerPrefab;
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return "";
		array<ResourceName> prefabs = missions.GetOfficerPrefabs();
		if (!prefabs || prefabs.IsEmpty())
			return "";
		return prefabs.GetRandomElement();
	}

	//------------------------------------------------------------------------------------------------
	//! Prefab des gardes : sentinelles de officier_gardes soldats, sinon une plus petite, sinon une section courte ;
	//! "" s'il n'y en a pas — PlaceOfficer
	protected ResourceName PickGuardPrefab(SRP_EnemyComponent enemies)
	{
		ResourceName prefab = enemies.PickPrefabOfSize(enemies.GetSentryPrefabs(), m_iOfficerGuards, m_iOfficerGuards);
		if (prefab.IsEmpty())
			prefab = enemies.PickPrefabOfSize(enemies.GetSentryPrefabs(), 1, m_iOfficerGuards);
		if (prefab.IsEmpty())
			prefab = enemies.PickPrefabOfSize(enemies.GetSectionPrefabs(), 1, m_iOfficerGuards);
		return prefab;
	}

	//------------------------------------------------------------------------------------------------
	//! OF2, OF10 : l'officier fuit à pied vers la cachette candidate suivante (une autre localité), sinon à 600 m à
	//! l'opposé du joueur le plus proche ; ses gardes restent — WatchOfficer
	protected void StartFlee(SRP_CmdRegion region, vector pos, array<IEntity> players, int nowUnix)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!officer || !enemies || !front || officer.m_aGroups.IsEmpty())
			return;
		SRP_EnemyGroup own = officer.m_aGroups[0];

		vector destination = vector.Zero;
		string destinationName = "";
		int nextLocality = NextHideLocality(front, region);
		if (nextLocality >= 0)
		{
			destination = LocalityHidePoint(front, nextLocality);
			destinationName = front.GetLocality(nextLocality).m_sName;
		}
		if (destination == vector.Zero)
		{
			destinationName = "";
			IEntity closest = NearestPlayer(pos, players);
			vector away = Vector(1, 0, 0);
			if (closest)
			{
				away = pos - closest.GetOrigin();
				away[1] = 0;
				if (away.Length() < 1)
					away = Vector(1, 0, 0);
				away.Normalize();
			}
			destination = SRP_Placement.OnGround(pos + away * FLEE_FALLBACK_M);
			if (SRP_Placement.IsWater(destination))
			{
				vector side = Vector(-away[2], 0, away[0]);
				destination = SRP_Placement.OnGround(pos + side * FLEE_FALLBACK_M);
				if (SRP_Placement.IsWater(destination))
					destination = SRP_Placement.OnGround(pos - side * FLEE_FALLBACK_M);
			}
		}

		enemies.ApplyRole(own, SRP_ECRXRole.PATROUILLE, destination, -1);
		enemies.OrderMove(own, destination);
		officer.m_bFleeing = true;
		officer.m_vFleeTo = destination;
		officer.m_sFleeLocality = destinationName;
		string toward = destinationName;
		if (toward.IsEmpty())
			toward = "un point à l'écart";
		SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 en fuite vers %3 (joueur à %4 m ou moins), ses gardes couvrent", region.m_sCode, officer.m_sName, toward, m_iOfficerFleeM));
	}

	//------------------------------------------------------------------------------------------------
	//! Cachette candidate suivante de la région (autre localité hostile que celle du jour), -1 sinon — StartFlee
	protected int NextHideLocality(SRP_FrontComponent front, SRP_CmdRegion region)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		int count = officer.m_aHideChoices.Count();
		if (count == 0)
			return -1;
		int start = officer.m_aHideChoices.Find(officer.m_sHideLocality);
		for (int step = 1; step <= count; step++)
		{
			int index = start + step;
			while (index >= count)
				index -= count;
			if (index < 0)
				index = 0;
			string name = officer.m_aHideChoices[index];
			if (name == officer.m_sHideLocality)
				continue;
			int locality = front.FindLocality(name);
			if (locality < 0)
				continue;
			if (IsHostileLocality(front, front.GetLocality(locality), region.m_iIndex))
				return locality;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Fuyard arrivé sans s'échapper : il tient sa nouvelle position, qui devient sa cachette du jour (sauvée) —
	//! WatchOfficer
	protected void FleeArrived(SRP_CmdRegion region, int nowUnix)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!officer || !enemies || officer.m_aGroups.IsEmpty())
			return;
		enemies.ApplyRole(officer.m_aGroups[0], SRP_ECRXRole.GARNISON, officer.m_vFleeTo, 30);
		MoveHideoutToFlight(officer, nowUnix);
		officer.m_bFleeing = false;
		SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 arrivé au bout de sa fuite, il s'y cache (%3)", region.m_sCode, officer.m_sName, HideLabel(officer)));
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! OF10 : évasion (plus de officier_echappe_m de sa cachette, personne à 800 m, hors de vue) : retiré, la
	//! destination devient la cachette du jour (sauvée), m_iEscapedUnix (la mission échoue) — WatchOfficer
	protected void Escape(SRP_CmdRegion region, int nowUnix)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		if (!officer)
			return;
		officer.m_iEscapedUnix = nowUnix;
		MoveHideoutToFlight(officer, nowUnix);
		RetireGroups(officer);
		officer.m_iDeferUntil = nowUnix + RETRY_S * 2;
		SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 s'est échappé vers %3 (mission OFFICIER échouée)", region.m_sCode, officer.m_sName, HideLabel(officer)));
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
			front.MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! La destination de la fuite devient la cachette du jour — FleeArrived, Escape
	protected void MoveHideoutToFlight(SRP_CmdOfficer officer, int nowUnix)
	{
		if (!officer.m_sFleeLocality.IsEmpty() && officer.m_sFleeLocality != officer.m_sHideLocality)
		{
			officer.m_sPrevLocality = officer.m_sHideLocality;
			officer.m_sHideLocality = officer.m_sFleeLocality;
		}
		if (officer.m_vFleeTo != vector.Zero)
			officer.m_vHidePos = officer.m_vFleeTo;
		officer.m_iHideDay = DayIndex(nowUnix);
		officer.m_bFleeing = false;
		officer.m_sFleeLocality = "";
	}

	//------------------------------------------------------------------------------------------------
	//! OF9 : officier capturé resté sur place : retiré quand personne n'est à officier_abandon_m (300 m) depuis
	//! officier_captif_retrait_min et qu'il n'est pas vu ; mort ou disparu : plus de suivi — Tick
	protected void WatchCaptive(SRP_CmdRegion region, int nowUnix)
	{
		SRP_CmdOfficer officer = region.m_Officer;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!officer || !enemies || officer.m_aGroups.IsEmpty())
			return;
		IEntity entity = officer.m_Entity;
		if (officer.m_iState != SRP_ECmdOfficerState.CAPTURE || !entity || entity.IsDeleted() || SRP_Utils.IsDead(entity))
		{
			// Plus rien à suivre : ce qui reste se retire comme des survivants ordinaires
			enemies.RetireLater(officer.m_aGroups);
			officer.m_aGroups.Clear();
			return;
		}
		array<IEntity> players = LivingPlayers();
		vector pos = entity.GetOrigin();
		if (AnyPlayerWithin(pos, m_iOfficerAbandonM, players))
		{
			officer.m_iCaptiveIdleSince = 0;
			return;
		}
		if (officer.m_iCaptiveIdleSince == 0)
		{
			officer.m_iCaptiveIdleSince = nowUnix;
			return;
		}
		if (nowUnix - officer.m_iCaptiveIdleSince < m_iOfficerCaptiveRetireMin * 60)
			return;
		if (SRP_Placement.IsSeenByAnyPlayer(pos, players))
			return;
		officer.m_bWeDelete = true;
		enemies.DeleteAll(officer.m_aGroups);
		officer.m_aGroups.Clear();
		// Le captif peut avoir quitté son unité (menottes ACE) : il est retiré lui-même
		if (entity && !entity.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
		officer.m_Entity = null;
		SRP_CmdLog.Note(region.m_iIndex, SRP_ECmdLogKind.OFFICIER, string.Format("%1 : officier ennemi %2 (capturé) retiré, personne à %3 m depuis %4 min", region.m_sCode, officer.m_sName, m_iOfficerAbandonM, m_iOfficerCaptiveRetireMin));
	}

	//------------------------------------------------------------------------------------------------
	//! Après la chute : les gardes deviennent des défenseurs ordinaires (rôle AUCUN, retrait habituel des survivants) ;
	//! le captif reste suivi (case 0) ; un tué n'est plus suivi (son corps reste) — OfficerDown
	protected void ReleaseFallenGroups(SRP_CmdOfficer officer, bool captured)
	{
		if (officer.m_aGroups.IsEmpty())
			return;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		array<ref SRP_EnemyGroup> released = {};
		SRP_EnemyGroup own = null;
		foreach (int i, SRP_EnemyGroup record : officer.m_aGroups)
		{
			if (!record)
				continue;
			if (i == 0)
			{
				own = record;
				continue;
			}
			record.m_iCmdRole = SRP_ECmdRole.AUCUN;
			released.Insert(record);
		}
		officer.m_aGroups.Clear();
		if (own)
		{
			if (captured)
				officer.m_aGroups.Insert(own);
			else
				released.Insert(own);
		}
		if (enemies && !released.IsEmpty())
			enemies.RetireLater(released);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire hors de vue tout ce qui est posé pour l'officier (vu : retrait différé des survivants ; pas vu : tout
	//! de suite) ; rend le nombre d'unités retirées — retrait, évasion, libération, remplacement, remises
	protected int RetireGroups(SRP_CmdOfficer officer)
	{
		IEntity entity = officer.m_Entity;
		officer.m_bWeDelete = true;
		officer.m_bFleeing = false;
		officer.m_iNoPlayerSince = 0;
		officer.m_iUnconsciousSince = 0;
		officer.m_iCaptiveIdleSince = 0;
		officer.m_Entity = null;
		if (officer.m_aGroups.IsEmpty())
			return 0;
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
		{
			officer.m_aGroups.Clear();
			return 0;
		}
		array<IEntity> players = LivingPlayers();

		// Un captif peut avoir quitté son unité (menottes ACE) : hors de vue, il est retiré lui-même
		if (officer.m_iState == SRP_ECmdOfficerState.CAPTURE && entity && !entity.IsDeleted() && !SRP_Utils.IsDead(entity))
		{
			if (!SRP_Placement.IsSeenByAnyPlayer(entity.GetOrigin(), players))
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}

		array<ref SRP_EnemyGroup> seen = {};
		array<ref SRP_EnemyGroup> unseen = {};
		foreach (SRP_EnemyGroup record : officer.m_aGroups)
		{
			if (!record)
				continue;
			if (IsGroupSeen(record, players))
				seen.Insert(record);
			else
				unseen.Insert(record);
		}
		int count = seen.Count() + unseen.Count();
		officer.m_aGroups.Clear();
		if (!unseen.IsEmpty())
			enemies.DeleteAll(unseen);
		if (!seen.IsEmpty())
			enemies.RetireLater(seen);
		return count;
	}

	//================================================================================================
	// Interne : outils
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Personnages des joueurs vivants (proximité et vue) — officiers
	protected array<IEntity> LivingPlayers()
	{
		array<IEntity> result = {};
		array<IEntity> everyone = SRP_Utils.GetPlayerCharacters();
		foreach (IEntity player : everyone)
		{
			if (player && !player.IsDeleted() && !SRP_Utils.IsDead(player))
				result.Insert(player);
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur est-il à radius mètres ou moins ? — officiers
	protected bool AnyPlayerWithin(vector position, float radius, array<IEntity> players)
	{
		if (!players)
			return false;
		foreach (IEntity player : players)
		{
			if (player && vector.Distance(player.GetOrigin(), position) <= radius)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur le plus proche, null sans joueur — StartFlee
	protected IEntity NearestPlayer(vector position, array<IEntity> players)
	{
		IEntity best = null;
		float bestDist = -1;
		if (!players)
			return null;
		foreach (IEntity player : players)
		{
			if (!player)
				continue;
			float d = vector.Distance(player.GetOrigin(), position);
			if (bestDist < 0 || d < bestDist)
			{
				bestDist = d;
				best = player;
			}
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur voit-il un soldat de l'unité ? — retraits
	protected bool IsGroupSeen(SRP_EnemyGroup record, array<IEntity> players)
	{
		if (!record)
			return false;
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(record, members);
		foreach (IEntity member : members)
		{
			if (member && SRP_Placement.IsSeenByAnyPlayer(member.GetOrigin(), players))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Gardes encore debout autour de l'officier — GetNearReport
	protected int CountGuards(SRP_CmdOfficer officer)
	{
		int count = 0;
		foreach (int i, SRP_EnemyGroup record : officer.m_aGroups)
		{
			if (i == 0 || !record)
				continue;
			array<IEntity> members = {};
			SRP_EnemyComponent.GetMembers(record, members);
			count += members.Count();
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Alerte à l'instant nowUnix (retombée linéaire, temps serveur éteint compris) — CO9
	protected float CurrentAlert(SRP_CmdRegion reg, int nowUnix)
	{
		if (!reg || reg.m_fAlert <= 0)
			return 0;
		int elapsed = nowUnix - reg.m_iAlertAt;
		if (elapsed <= 0)
			return reg.m_fAlert;
		float perSecond = m_iAlertMax / (m_iAlertDecayMin * 60.0);
		float value = reg.m_fAlert - elapsed * perSecond;
		if (value < 0)
			return 0;
		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! CO9 : menace locale ajoutée par une alerte = min(alerte_menace_max, floor(alerte / alerte_menace_par))
	protected int ThreatAdd(float alert)
	{
		int add = Math.Floor(alert / m_iAlertThreatPer);
		if (add > m_iAlertThreatMax)
			add = m_iAlertThreatMax;
		if (add < 0)
			add = 0;
		return add;
	}

	//------------------------------------------------------------------------------------------------
	//! Bonus OF5 en cours dans la région, en centièmes (0 sans officier vivant) — GetRegionReport
	protected int GarrisonBonusText(int region)
	{
		SRP_CmdRegion reg = GetRegion(region);
		if (!reg || reg.m_iState != SRP_ECmdRegionState.ACTIVE || !HasLivingOfficer(region))
			return 0;
		return m_iOfficerGarrisonBonusCent;
	}

	//------------------------------------------------------------------------------------------------
	//! Poids des quatre caractères lus dans cmd_trait_poids du cerveau (prudent, audacieux, lent, méthodique) — PickTrait
	protected void TraitWeights(notnull array<int> weights)
	{
		weights.Clear();
		string text = "1, 1, 1, 1";
		if (m_Commander)
			text = m_Commander.m_sTraitWeights;
		text.Replace(";", ",");
		array<string> parts = {};
		text.Split(",", parts, true);
		for (int t = 0; t < 4; t++)
		{
			int weight = 1;
			if (t < parts.Count())
			{
				string one = parts[t];
				one = one.Trim();
				weight = one.ToInt();
			}
			if (weight < 0)
				weight = 0;
			weights.Insert(weight);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Liste « a, b ; c » découpée, espaces retirés, vides et doublons écartés — noms
	protected void SplitNames(string text, notnull array<string> names)
	{
		names.Clear();
		string all = text;
		all.Replace(";", ",");
		array<string> parts = {};
		all.Split(",", parts, true);
		foreach (string part : parts)
		{
			string one = part.Trim();
			if (!one.IsEmpty() && !names.Contains(one))
				names.Insert(one);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! « a, b, c » — sauvegarde des noms, journal
	protected string JoinNames(notnull array<string> names)
	{
		string text = "";
		foreach (string name : names)
		{
			if (name.IsEmpty())
				continue;
			if (!text.IsEmpty())
				text += ", ";
			text += name;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Lignes jointes par des retours à la ligne — rapports Staff
	protected string JoinLines(notnull array<string> lines)
	{
		string text = "";
		foreach (int i, string line : lines)
		{
			if (i > 0)
				text += "\n";
			text += line;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07), Perelle (S08) » — rapports
	protected string ZoneList(SRP_FrontComponent front, notnull array<int> zones)
	{
		if (zones.IsEmpty())
			return "aucune";
		string text = "";
		foreach (int zone : zones)
		{
			if (!text.IsEmpty())
				text += ", ";
			if (front)
				text += front.GetZoneLabel(zone);
			else
				text += zone.ToString();
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « active », « désorganisée encore 2 h 10 (officier capturé) », « libérée depuis 3 h 05 » — rapports
	protected string RegionStateText(SRP_CmdRegion reg, int nowUnix)
	{
		if (reg.m_iState == SRP_ECmdRegionState.DESORGANISEE)
		{
			string why = "";
			if (reg.m_bDesorgCaptured)
				why = " (officier capturé)";
			if (reg.m_iDesorgUntil > nowUnix)
				return string.Format("désorganisée encore %1%2, jusqu'à %3", DurText(reg.m_iDesorgUntil - nowUnix), why, HourText(reg.m_iDesorgUntil));
			return "désorganisée, nouvel officier imminent" + why;
		}
		if (reg.m_iState == SRP_ECmdRegionState.LIBEREE)
		{
			if (reg.m_iLiberatedAt > 0)
				return "libérée depuis " + DurText(nowUnix - reg.m_iLiberatedAt);
			return "libérée";
		}
		return "active";
	}

	//------------------------------------------------------------------------------------------------
	//! « officier ennemi Volkov (prudent), n° 7, en poste depuis 2 h 10 — posé » — rapports
	protected string OfficerText(SRP_CmdRegion reg, int nowUnix)
	{
		SRP_CmdOfficer officer = reg.m_Officer;
		if (!officer)
			return "aucun pour l'instant";
		string head = string.Format("officier ennemi %1 (%2), n° %3, arrivé il y a %4", officer.m_sName, TraitText(officer.m_iTrait), officer.m_iSerial, DurText(nowUnix - officer.m_iSinceUnix));
		if (officer.m_iState == SRP_ECmdOfficerState.VIVANT)
		{
			if (!officer.m_aGroups.IsEmpty())
				return head + " — posé";
			return head + " — sur le papier";
		}
		if (officer.m_iState == SRP_ECmdOfficerState.TUE)
			return head + " — tué il y a " + DurText(nowUnix - officer.m_iDownUnix);
		if (officer.m_iState == SRP_ECmdOfficerState.CAPTURE)
			return head + " — capturé il y a " + DurText(nowUnix - officer.m_iDownUnix);
		return head + " — a quitté l'île (région libérée)";
	}

	//------------------------------------------------------------------------------------------------
	//! « Cachette du jour : Chotain, carré 074 042 (choisie parmi …) ; précédente : Durras » — GetRegionReport
	protected string HideoutText(SRP_FrontComponent front, SRP_CmdOfficer officer)
	{
		string label = HideLabel(officer);
		if (label.IsEmpty())
			return "Cachette du jour : aucune (plus de zone rouge dans la région)";
		string text = "Cachette du jour : " + label;
		if (front && officer.m_vHidePos != vector.Zero)
		{
			int cell = front.CellIndexAt(officer.m_vHidePos);
			if (cell >= 0)
				text += ", carré " + front.CellRef(cell);
		}
		if (!officer.m_aHideChoices.IsEmpty())
			text += " (choisie parmi " + JoinNames(officer.m_aHideChoices) + ")";
		if (!officer.m_sPrevLocality.IsEmpty())
			text += " ; précédente : " + officer.m_sPrevLocality;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! « prudent », « audacieux », « lent », « méthodique » — journal, rapports
	protected static string TraitText(int trait)
	{
		if (trait == SRP_ECmdTrait.PRUDENT)
			return "prudent";
		if (trait == SRP_ECmdTrait.AUDACIEUX)
			return "audacieux";
		if (trait == SRP_ECmdTrait.LENT)
			return "lent";
		return "méthodique";
	}

	//------------------------------------------------------------------------------------------------
	//! « prudente », « normale », « agressive » — rapports
	protected static string MoodText(int mood)
	{
		if (mood == SRP_ECmdMood.PRUDENTE)
			return "prudente";
		if (mood == SRP_ECmdMood.AGRESSIVE)
			return "agressive";
		return "normale";
	}

	//------------------------------------------------------------------------------------------------
	//! « région de X », ou « région d'X » devant a, e, i, o, u, y, é, è, h (même règle que SRP_CmdScreens.RegionOf) —
	//! GetRegionLabel
	protected static string LabelOf(string name)
	{
		if (name.IsEmpty())
			return "région sans nom";
		string first = name.Substring(0, 1);
		first.ToLower();
		bool elide = first == "a" || first == "e" || first == "i" || first == "o" || first == "u" || first == "y" || first == "h";
		if (!elide)
			elide = name.StartsWith("É") || name.StartsWith("é") || name.StartsWith("È") || name.StartsWith("è");
		if (elide)
			return "région d'" + name;
		return "région de " + name;
	}

	//------------------------------------------------------------------------------------------------
	//! « 2 h 10 », « 12 min », « moins d'une minute » — journal, rapports
	protected static string DurText(int seconds)
	{
		if (seconds < 60)
			return "moins d'une minute";
		int minutes = seconds / 60;
		if (minutes < 60)
			return string.Format("%1 min", minutes);
		int hours = minutes / 60;
		int rest = minutes - hours * 60;
		if (rest == 0)
			return string.Format("%1 h", hours);
		return string.Format("%1 h %2", hours, SRP_Time.Pad2(rest));
	}

	//------------------------------------------------------------------------------------------------
	//! « 03:40 » : heure locale du serveur d'une heure Unix — journal, rapports
	protected string HourText(int unixTime)
	{
		int seconds = unixTime + m_iTzOffset;
		int days = seconds / 86400;
		int inDay = seconds - days * 86400;
		if (inDay < 0)
			inDay += 86400;
		int hours = inDay / 3600;
		int minutes = (inDay - hours * 3600) / 60;
		return SRP_Time.Pad2(hours) + ":" + SRP_Time.Pad2(minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! OF11 : région non libérée (autre que exclude) qui partage le plus de côtés de carrés (ou la liaison maritime)
	//! avec la zone ; à égalité, le plus petit index ; -1 sinon — AttachZone
	protected int SharedSidesRegion(SRP_FrontComponent front, int zone, int exclude)
	{
		int regionCount = m_aRegions.Count();
		array<int> sides = {};
		for (int r = 0; r < regionCount; r++)
			sides.Insert(0);
		array<int> cells = {};
		front.GetZoneCells(zone, cells);
		foreach (int cell : cells)
		{
			for (int dir = 0; dir < 4; dir++)
				CountSide(front, front.Neighbour(cell, dir), zone, exclude, sides);
			CountSide(front, front.GetSeaLink(cell), zone, exclude, sides);
		}
		int best = -1;
		for (int k = 0; k < regionCount; k++)
		{
			if (sides[k] <= 0)
				continue;
			if (best < 0 || sides[k] > sides[best])
				best = k;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Compte un côté partagé avec le carré voisin cell s'il est dans une zone d'une région vivante — SharedSidesRegion
	protected void CountSide(SRP_FrontComponent front, int cell, int zone, int exclude, notnull array<int> sides)
	{
		if (cell < 0)
			return;
		int other = front.GetCellZone(cell);
		if (other <= 0 || other == zone)
			return;
		int region = GetRegionOfZone(other);
		if (region < 0 || region == exclude || region >= sides.Count())
			return;
		if (m_aRegions[region].m_iState == SRP_ECmdRegionState.LIBEREE)
			return;
		sides[region] = sides[region] + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! OF11 : parcours en largeur sur les zones depuis zone, jusqu'à la première zone d'une région non libérée
	//! (autre que exclude) ; -1 sinon — AttachZone
	protected int NearestLiveRegion(int zone, int exclude)
	{
		int zoneCount = m_aAdjStart.Count() - 1;
		if (zone <= 0 || zone >= zoneCount)
			return -1;
		array<bool> seen = {};
		for (int i = 0; i < zoneCount; i++)
			seen.Insert(false);
		array<int> queue = {};
		queue.Insert(zone);
		seen[zone] = true;
		int head = 0;
		while (head < queue.Count())
		{
			int current = queue[head];
			head++;
			int region = GetRegionOfZone(current);
			if (current != zone && region >= 0 && region != exclude && m_aRegions[region].m_iState != SRP_ECmdRegionState.LIBEREE)
				return region;
			int firstAdj = m_aAdjStart[current];
			int lastAdj = m_aAdjStart[current + 1];
			for (int a = firstAdj; a < lastAdj; a++)
			{
				int n = m_aAdj[a];
				if (n <= 0 || seen[n])
					continue;
				seen[n] = true;
				queue.Insert(n);
			}
		}
		return -1;
	}
}
