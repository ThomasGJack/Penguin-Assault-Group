//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : le SOCLE (cartes #75 et #76).
//
// RÔLE : seule source de vérité du front, présente sur TOUTES les machines (serveur et joueurs).
// - Grille de carrés de 200 m (A1, A2), zones de 1 km (A3 à A6), base (B2) et zones protégées (B4), localités lues
//   dans SRP_CivilZoneComponent (trou 24), registre unique des points clés (D3, D6, arbitrage Q8).
// - Propriétaires des carrés et des zones, états contestés (C8), gel (J3), menace (G9), victoire (B3), coupure E4
//   par zone (seule implémentation, trou 8).
// - Réplication (RplSave/RplLoad + RPC), versions (état, géométrie, contesté, pont), compteur de restaurations.
// - Sauvegarde UNIQUE $profile:SimpleRP/front.json (trou 16) + copies datées front_jours/ (J2), migration K1/K2.
// - UNE SEULE boucle de 5 s (trou 7) : capture -> zones -> économie -> radio -> marqueurs.
// - Après chaque lot de carrés, appels directs (trou 6, aucun ScriptInvoker) : capture -> zones -> économie -> pont
//   -> radio.
// - Classes tenues : SRP_FrontCapture (m_Capture), SRP_ZoneFall (m_ZoneFall), SRP_FrontEconomy (m_Economy) ; la
//   radio et les repères sont statiques (SRP_FrontRadio, SRP_FrontMarkers), la carte est côté joueur (SRP_FrontMap.c).
// - Réglages : attributs publics ci-dessous (défauts = choix de Jack), surchargés au démarrage par les clés front_*
//   de $profile:SimpleRP/front_reglages.txt, lues par le SEUL lecteur du mod : SRP_CmdSettings (arbitrage Q6).
//
// APPELÉ PAR : tout le mod (lecture) ; SRP_ZoneFall (SetZoneOwner, seul), SRP_FrontCapture (SetCellsOwner,
// SetLiveCells), SRP_Admin / SRP_FrontScreens (primitives Staff), SRP_Bridge (versions, chaînes), SRP_FrontMap (joueurs).
// Toutes les énumérations du front sont déclarées ICI et nulle part ailleurs (trou 4).
//------------------------------------------------------------------------------------------------

//! Propriétaire d'un carré ou d'une zone. Hors jeu (mer, îlot vide) = -1 (SRP_FrontComponent.HORS_JEU).
//! Mêmes valeurs que SRP_ESectorOwner (ENNEMI 0, NOUS 1), gardé pour les localités.
enum SRP_EFrontOwner
{
	ROUGE = 0,
	BLEU = 1
};

//! État contesté d'un carré (C8), jamais sauvegardé, répliqué pour l'orange de la carte (I1)
enum SRP_EFrontLive
{
	AUCUN = 0,		// rien
	CAPTURE = 1,	// C4 : un joueur prend le carré (rouge)
	COMBAT = 2,		// C7, F3 : joueurs et ennemis présents, progression figée
	REPRISE = 3		// F3 : les soldats de l'attaque reprennent le carré (bleu)
};

//! Cause d'un changement de carré ou de zone : fusion des listes grille, capture et carte (trou 4)
enum SRP_EFrontReason
{
	CAPTURE = 0,		// C4 : 1 min de présence (ex « COMBAT » de la carte)
	SAISIE = 1,			// D4 : saisie du poste de commandement, le carré du point clé passe bleu
	REPRISE = 2,		// F3 : reprise par les soldats de l'attaque (ex « CONTRE_ATTAQUE »)
	ZONE_TOMBEE = 3,	// D1 : la zone tombe, ses carrés rouges sont repeints en bleu en silence
	ZONE_PERDUE = 4,	// Q2 : la zone repasse rouge (points clés + plus de la moitié des carrés), sans repeinte
	ISOLEMENT = 5,		// E4 : zone coupée de la base depuis 24 h (ex « COUPURE »), repeinte
	OFFENSIVE = 6,		// H2 : offensive de la nuit (ex « HORS_LIGNE »), repeinte
	STAFF = 7,			// J1 : correction du Staff, ni prime, ni menace, ni radio (Q9)
	REMISE = 8,			// B3, K1 : nouvelle campagne, ou remise à zéro d'une zone
	RESTAURATION = 9	// J2 : retour à une copie datée
};

//! Rôle d'un point clé (D6) : le centre de toute localité, le QG en plus pour une ville ou un bourg
enum SRP_EKeyPointRole
{
	CENTRE = 0,
	QG = 1
};

//! D'où vient la position d'un point clé (arbitrage Q8 : posés AU WORKBENCH, repli sur le nom de la carte).
//! Priorité (Q8) : CENTRE : REPERE > NOM_CARTE ; QG : REPERE > aucun (ville à un seul point clé, listée MANQUANT au
//! menu Staff). Les valeurs numériques ne sont pas un ordre de priorité.
enum SRP_EKeyPointSource
{
	NOM_CARTE = 0,	// repli du CENTRE seulement : l'endroit du nom sur la carte (le menu Staff le liste « manquant »)
	REPERE = 1,		// prefab repère SRP_PointCle_Centre / SRP_PointCle_QG posé au Workbench (SRP_KeyPointComponent), prioritaire
	FICHIER = 2		// plus jamais produit : l'ancienne ligne « point » de front_retouches.txt est refusée par le bâtisseur
					// (Q8) ; valeur gardée, encore citée ailleurs (SRP_ZoneFall)
};

//! Genre d'une zone, d'après sa plus grosse localité (un bourg compte comme une ville)
enum SRP_EZoneKind
{
	AUCUNE = 0,		// zone sans localité (Q4)
	HAMEAU = 1,
	VILLAGE = 2,
	VILLE = 3
};

//! État d'attaque d'une zone (drapeaux FLAG_ATTACK_ANNOUNCED et FLAG_ATTACK), lu par la carte, le PC et le pont
enum SRP_EFrontAttack
{
	AUCUNE = 0,
	ANNONCEE = 1,
	ASSAUT = 2
};

//! Fin d'une contre-attaque (F4, CA5, CA6, C8) : seule énumération de fin, aussi rendue par SRP_CmdManeuvers.CheckEnd
enum SRP_EAttackEnd
{
	AUCUNE = 0,		// pas finie
	PERDUE = 1,		// la zone est repassée rouge (Q2)
	DEFENDUE = 2,	// F4 ou deux tiers de pertes (C8) : menace -1, radio « défendue »
	ROMPUE = 3,		// 60 min sans résultat : menace inchangée, carrés repris restent rouges
	ANNULEE = 4		// gel, zone plus attaquable, 1re vague sans place
};

//! Raison qui bloque la saisie d'un poste de commandement (D2, D4, D5, J3), répliquée sur le poste
enum SRP_ESeizeBlock
{
	LIBRE = 0,
	ENNEMIS = 1,			// D5 : un ennemi en état de combattre à moins de 100 m
	PAS_AU_CONTACT = 2,		// D2 : le carré du point clé ne touche pas de carré bleu
	GELE = 3,				// J3
	AUTRE_SOLDAT = 4,		// saisie en cours par un autre joueur
	TOUCHE = 5,				// saisisseur touché il y a moins de m_iHitLockSeconds
	GARNISON_EN_ROUTE = 6	// la garnison n'est pas encore posée et installée
};

//! Nature d'une remise à plat, passée aux modules (capture, économie, ennemi, radio, pont)
enum SRP_EFrontReset
{
	CAMPAGNE = 0,		// nouvelle campagne (B3, K1, Staff)
	RESTAURATION = 1,	// retour à une copie datée (J2)
	ZONE = 2			// remise d'UNE zone à son état de départ (Staff)
};

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Front SimpleRP : grille de carrés, zones, points clés, capture, chute, économie, radio (socle, toutes machines)")]
class SRP_FrontComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
class SRP_FrontComponent : SCR_BaseGameModeComponent
{
	// --- Constantes ---------------------------------------------------------------------------------
	static const string PATH = "$profile:SimpleRP/front.json";
	static const string DAILY_DIR = "$profile:SimpleRP/front_jours";
	static const string ARCHIVE_DIR = "$profile:SimpleRP/archives";
	static const string RETOUCH_PATH = "$profile:SimpleRP/front_retouches.txt";
	static const string REPORT_PATH = "$profile:SimpleRP/front_zones.txt";
	static const string OLD_PATH = "$profile:SimpleRP/territoire.json";
	static const int FORMAT = 2;					// version du format de front.json
	static const int HORS_JEU = -1;					// propriétaire d'un carré hors jeu
	static const int FLAG_ATTACK_ANNOUNCED = 1;		// drapeau de zone : attaque annoncée (F6)
	static const int FLAG_ATTACK = 2;				// drapeau de zone : assaut en cours
	static const int FLAG_CUT = 4;					// drapeau de zone : coupée de la base (E4)
	static const int TICK_MS = 5000;				// la boucle unique du front
	static const int HOUSEKEEPING_TICKS = 12;		// entretien toutes les 12 boucles (60 s)

	// --- Réglages : grille (A1 à A6, B2, B4) ----------------------------------------------------
	[Attribute("200", UIWidgets.EditBox, "Taille d'un carré, en mètres (A1)", category: "SimpleRP - Front : grille")]
	float m_fCellSize;

	[Attribute("12800", UIWidgets.EditBox, "Taille de la carte si le jeu ne la fournit pas, en mètres", category: "SimpleRP - Front : grille")]
	float m_fWorldSize;

	[Attribute("4", UIWidgets.EditBox, "Points testés par côté d'un carré pour la terre (4 x 4 = 16)", category: "SimpleRP - Front : grille")]
	int m_iLandSamples;

	[Attribute("0.1", UIWidgets.EditBox, "Part de terre minimale pour qu'un carré soit en jeu, de 0 à 1 (A2)", category: "SimpleRP - Front : grille")]
	float m_fLandMinShare;

	[Attribute("0", UIWidgets.EditBox, "Un point est terre s'il dépasse le niveau de la mer de … mètres", category: "SimpleRP - Front : grille")]
	float m_fLandMinHeight;

	[Attribute("1", UIWidgets.CheckBox, "Garder les îles qui portent une localité, comme Erquy (A2)", category: "SimpleRP - Front : grille")]
	bool m_bKeepIslandsWithLocality;

	[Attribute("1", UIWidgets.CheckBox, "Une île secondaire forme une seule zone (Q5)", category: "SimpleRP - Front : grille")]
	bool m_bIslandSingleZone;

	[Attribute("1", UIWidgets.CheckBox, "Liaison par la mer entre une île et le carré de côte le plus proche (Q5 : C1, E1, E4)", category: "SimpleRP - Front : grille")]
	bool m_bSeaLinks;

	[Attribute("5", UIWidgets.EditBox, "Côté d'un bloc de zone, en carrés (5 x 200 m = 1 km, A3)", category: "SimpleRP - Front : grille")]
	int m_iBlockCells;

	[Attribute("5", UIWidgets.EditBox, "Un morceau sans localité plus petit est fusionné dans un voisin (A4)", category: "SimpleRP - Front : grille")]
	int m_iZoneMinCells;

	[Attribute("1", UIWidgets.CheckBox, "Un bloc à deux localités est coupé en deux zones (Q3)", category: "SimpleRP - Front : grille")]
	bool m_bSplitMultiLocality;

	[Attribute("2500", UIWidgets.EditBox, "Un lieu-dit plus loin ne donne pas son nom à une zone sans localité, en mètres (A6)", category: "SimpleRP - Front : grille")]
	float m_fNameMaxDistance;

	[Attribute("S", UIWidgets.EditBox, "Préfixe des codes de zone (S07, A6)", category: "SimpleRP - Front : grille")]
	string m_sCodePrefix;

	[Attribute("Base de Levie", UIWidgets.EditBox, "Nom de la zone de la base", category: "SimpleRP - Front : grille")]
	string m_sBaseZoneName;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Entité qui marque la base (B2)", category: "SimpleRP - Front : grille")]
	string m_sBaseMarkerName;

	[Attribute("1500", UIWidgets.EditBox, "Rayon de la base jusqu'au centre des carrés, en mètres (B2) ; les localités plus proches sont celles de la base", category: "SimpleRP - Front : grille")]
	float m_fBaseRadius;

	[Attribute("1", UIWidgets.CheckBox, "Une zone qui touche la base ne peut plus être perdue une fois prise (B4)", category: "SimpleRP - Front : grille")]
	bool m_bShieldBaseNeighbours;

	[Attribute("600", UIWidgets.EditBox, "Un repère de point clé sans nom se rattache à la localité la plus proche dans ce rayon, en mètres (Q8)", category: "SimpleRP - Front : grille")]
	float m_fKeyPointMaxDistance;

	[Attribute("200", UIWidgets.EditBox, "Rapport : rayon du relevé des villages coupés par une limite de zone, en mètres", category: "SimpleRP - Front : grille")]
	float m_fVillageCheckRadius;

	[Attribute("2", UIWidgets.EditBox, "Démarrage : intervalle entre deux essais si la carte n'est pas prête, en secondes", category: "SimpleRP - Front : grille")]
	int m_iMapRetrySeconds;

	[Attribute("30", UIWidgets.EditBox, "Démarrage : nombre d'essais au plus si la carte n'est pas prête", category: "SimpleRP - Front : grille")]
	int m_iMapRetryMax;

	// --- Réglages : sauvegarde, coupure, victoire, migration -----------------------------------------
	[Attribute("30", UIWidgets.EditBox, "front.json est écrit au plus … secondes après un changement de carré (aussitôt pour une zone, le Staff, la menace, l'arrêt)", category: "SimpleRP - Front : sauvegarde")]
	int m_iSaveDelaySeconds;

	[Attribute("30", UIWidgets.EditBox, "Copies datées de front.json gardées (J2)", category: "SimpleRP - Front : sauvegarde")]
	int m_iDailyKeep;

	[Attribute("1440", UIWidgets.EditBox, "Minutes réelles au bout desquelles une zone bleue coupée de la base repasse rouge (E4, Q1 : 24 h ; 2 pour les essais)", category: "SimpleRP - Front : sauvegarde")]
	int m_iCutLossMinutes;

	[Attribute("0", UIWidgets.EditBox, "Minutes entre la victoire et la nouvelle campagne ; 0 = au prochain démarrage du serveur (B3)", category: "SimpleRP - Front : sauvegarde")]
	int m_iVictoryResetMinutes;

	[Attribute("1000", UIWidgets.EditBox, "Intervalle minimal d'envoi des carrés contestés aux joueurs, en millisecondes (C8)", category: "SimpleRP - Front : sauvegarde")]
	int m_iLiveSendMinMs;

	[Attribute("1", UIWidgets.CheckBox, "Premier démarrage sans front.json : lire l'ancien territoire.json, l'archiver et lancer la campagne 2 (K1)", category: "SimpleRP - Front : migration")]
	bool m_bLegacyImport;

	[Attribute("1", UIWidgets.CheckBox, "Verser une seule fois aux dépôts de la base les paquets en attente de l'ancien fichier (K2)", category: "SimpleRP - Front : migration")]
	bool m_bLegacyStockToBase;

	[Attribute("10", UIWidgets.EditBox, "K2 : si les dépôts n'ont pas fini leur regarnissage au bout de … minutes, le versement part quand même", category: "SimpleRP - Front : migration")]
	int m_iLegacyPayTimeoutMinutes;

	[Attribute("10", UIWidgets.EditBox, "K2 : dépôts pleins, ce qui n'a pas pu être rangé est réessayé toutes les … minutes", category: "SimpleRP - Front : migration")]
	int m_iLegacyRetryMinutes;

	[Attribute("1", UIWidgets.CheckBox, "Une victoire ou une nouvelle campagne archive d'abord l'état final dans archives/", category: "SimpleRP - Front : migration")]
	bool m_bArchiveOnNewCampaign;

	// --- Réglages : capture (C1 à C8, F3, F5), lus par SRP_FrontCapture --------------------------------
	[Attribute("1", UIWidgets.EditBox, "Minutes de présence sans ennemi pour prendre un carré (C4, clé front_capture_minutes ; 1 min, choix de Jack du 27/09)", category: "SimpleRP - Front : capture")]
	int m_iSquareCaptureMinutes;

	[Attribute("1", UIWidgets.EditBox, "Vitesse de redescente de la progression sans personne, comparée à la montée (C7 ; 1 = même vitesse)", category: "SimpleRP - Front : capture")]
	float m_fCaptureDecayRatio;

	[Attribute("ville,village", UIWidgets.EditBox, "Genres de localités dont le centre se prend d'un seul bloc, séparés par des virgules : ville, village, hameau (choix de Jack du 27/09 : villes et villages)", category: "SimpleRP - Front : capture")]
	string m_sBlockKinds;

	[Attribute("600", UIWidgets.EditBox, "Bloc d'une localité : carrés de sa zone dont le centre est à moins de … mètres du centre de la localité (0 = aucun bloc)", category: "SimpleRP - Front : capture")]
	float m_fBlockRadius;

	[Attribute("5", UIWidgets.EditBox, "Minutes de présence sans ennemi dans le bloc d'une localité pour prendre tous ses carrés d'un coup", category: "SimpleRP - Front : capture")]
	int m_iBlockMinutes;

	[Attribute("7", UIWidgets.EditBox, "Au-delà de cette hauteur dans un véhicule, un joueur ou un ennemi ne compte pas, en mètres (C2, C6)", category: "SimpleRP - Front : capture")]
	float m_fMaxVehicleAltitude;

	[Attribute("0", UIWidgets.EditBox, "Minutes pendant lesquelles un joueur ne compte pas APRÈS la fermeture du Game Master ou la fin d'une possession (C3 ; 0 = seulement pendant). La téléportation ne compte plus (choix de Jack du 27/09)", category: "SimpleRP - Front : capture")]
	int m_iExcludeMinutes;

	[Attribute("1", UIWidgets.CheckBox, "Tout joueur téléporté par le menu Staff est écarté, pas seulement le Staff (C3)", category: "SimpleRP - Front : capture")]
	bool m_bExcludeAllTeleported;

	[Attribute("1", UIWidgets.CheckBox, "Écarter un joueur qui a le Game Master ouvert (non limité) ou qui possède une IA (C3)", category: "SimpleRP - Front : capture")]
	bool m_bExcludeGameMaster;

	[Attribute("0", UIWidgets.EditBox, "Filet de sécurité : un Staff à pied qui saute de plus de … mètres en un passage est écarté (0 = arrêt ; arrêté le 27/09, la téléportation ne compte plus)", category: "SimpleRP - Front : capture")]
	float m_fStaffJumpMeters;

	[Attribute("1", UIWidgets.CheckBox, "Un membre du Staff en mode invincible ne compte pas", category: "SimpleRP - Front : capture")]
	bool m_bExcludeGodMode;

	[Attribute("60", UIWidgets.EditBox, "Secondes d'occupation par un soldat de l'attaque, sans joueur, pour reprendre un carré (F3, Q2, clé front_reprise_secondes)", category: "SimpleRP - Front : capture")]
	int m_iRetakeSeconds;

	[Attribute("1", UIWidgets.EditBox, "Vitesse de redescente de la reprise sans assaillant", category: "SimpleRP - Front : capture")]
	float m_fRetakeDecayRatio;

	[Attribute("1", UIWidgets.CheckBox, "L'ennemi ne reprend qu'un carré qui touche du rouge par un côté (Q2)", category: "SimpleRP - Front : capture")]
	bool m_bRetakeNeedsRed;

	[Attribute("1", UIWidgets.CheckBox, "L'ennemi ne reprend que les carrés de la zone qu'il attaque (Q2)", category: "SimpleRP - Front : capture")]
	bool m_bRetakeTargetZoneOnly;

	[Attribute("100", UIWidgets.EditBox, "Un joueur valide à moins de … mètres d'un point clé empêche la reprise de son carré (F5, Q2, clé front_garde_point_cle_m)", category: "SimpleRP - Front : capture")]
	float m_fKeyPointGuardRadius;

	[Attribute("10", UIWidgets.EditBox, "Une entrée « assaillants sur le papier » compte pour la reprise pendant … secondes (CA6)", category: "SimpleRP - Front : capture")]
	int m_iPaperFreshSeconds;

	[Attribute("0", UIWidgets.CheckBox, "Diagnostic : écrit au journal FRONT la durée d'un passage de capture quand elle dépasse 5 ms", category: "SimpleRP - Front : capture")]
	bool m_bTimingDiag;

	// --- Réglages : chute des zones et points clés (D1 à D6, Q2, Q4, G9), lus par SRP_ZoneFall ---------
	[Attribute("50", UIWidgets.EditBox, "Part minimale de carrés bleus, sur 100, pour qu'une zone AVEC localité tombe une fois ses points clés saisis (D1)", category: "SimpleRP - Front : chute des zones")]
	int m_iZoneFallPercent;

	[Attribute("50", UIWidgets.EditBox, "Part de carrés bleus, sur 100, pour qu'une zone SANS localité tombe (Q4)", category: "SimpleRP - Front : chute des zones")]
	int m_iNoLocalityFallPercent;

	[Attribute("50", UIWidgets.EditBox, "Une zone à nous repasse rouge quand PLUS de cette part, sur 100, de ses carrés est rouge et ses points clés repris (Q2)", category: "SimpleRP - Front : chute des zones")]
	int m_iZoneLossPercent;

	[Attribute("1", UIWidgets.CheckBox, "Perte : l'ennemi doit reprendre TOUS les points clés de la zone (Q2)", category: "SimpleRP - Front : chute des zones")]
	bool m_bLossNeedsAllKeyPoints;

	[Attribute("1", UIWidgets.CheckBox, "Zone à deux localités : saisir les points clés des deux", category: "SimpleRP - Front : chute des zones")]
	bool m_bAllLocalitiesRequired;

	[Attribute("100", UIWidgets.EditBox, "D5 : aucun ennemi au sol en état de combattre à moins de … mètres du point clé pour saisir (et interrompre)", category: "SimpleRP - Front : chute des zones")]
	float m_fKeyPointClearRadius;

	[Attribute("0.9", UIWidgets.EditBox, "Garde-fou serveur : la saisie n'est validée que si cette part de la durée de l'action s'est écoulée", category: "SimpleRP - Front : chute des zones")]
	float m_fSeizeMinRatio;

	[Attribute("4", UIWidgets.EditBox, "La saisie s'interrompt si le soldat s'éloigne de plus de … mètres du poste", category: "SimpleRP - Front : chute des zones")]
	float m_fSeizeMaxDistance;

	[Attribute("1000", UIWidgets.EditBox, "Contrôle pendant une saisie, en millisecondes", category: "SimpleRP - Front : chute des zones")]
	int m_iSeizeCheckMs;

	[Attribute("3", UIWidgets.EditBox, "Après un coup reçu, la saisie est refusée pendant … secondes", category: "SimpleRP - Front : chute des zones")]
	int m_iHitLockSeconds;

	[Attribute("50", UIWidgets.EditBox, "Le poste n'est posé que si aucun joueur n'est à moins de … mètres et que personne ne le voit", category: "SimpleRP - Front : chute des zones")]
	float m_fCPSpawnPlayerMin;

	[Attribute("300", UIWidgets.EditBox, "Un poste ou un mât qui n'a plus lieu d'être n'est retiré que hors de vue et sans joueur à moins de … mètres", category: "SimpleRP - Front : chute des zones")]
	float m_fCPDeleteMinPlayerDistance;

	[Attribute("30", UIWidgets.EditBox, "Délai entre deux essais de pose d'un poste, en secondes", category: "SimpleRP - Front : chute des zones")]
	int m_iCPRetrySeconds;

	[Attribute("", UIWidgets.ResourceNamePicker, "Poste de commandement ennemi (Prefabs/Props/SRP_PosteCommandement.et, D4)", "et", category: "SimpleRP - Front : chute des zones")]
	ResourceName m_sCPPrefab;

	[Attribute("{1D2887BB9A7D4670}Prefabs/Compositions/Misc/SubCompositions/Tents/Tent_CommandPost_USSR_01.et", UIWidgets.ResourceNamePicker, "Décor du QG des villes (vide = aucun)", "et", category: "SimpleRP - Front : chute des zones")]
	ResourceName m_sHQDecorPrefab;

	[Attribute("1", UIWidgets.CheckBox, "Un mât tricolore remplace le poste saisi tant que le carré du point clé est bleu", category: "SimpleRP - Front : chute des zones")]
	bool m_bRaiseColours;

	[Attribute("", UIWidgets.ResourceNamePicker, "Mât de nos couleurs (Prefabs/Props/SRP_MatCouleurs.et)", "et", category: "SimpleRP - Front : chute des zones")]
	ResourceName m_sColoursPrefab;

	[Attribute("1", UIWidgets.EditBox, "Menace ajoutée par zone prise (G9, clé front_menace_prise)", category: "SimpleRP - Front : chute des zones")]
	int m_iThreatPerZoneTaken;

	[Attribute("1", UIWidgets.EditBox, "Menace retirée par zone perdue (G9, clé front_menace_perte)", category: "SimpleRP - Front : chute des zones")]
	int m_iThreatPerZoneLost;

	[Attribute("1", UIWidgets.EditBox, "Une zone de moins de … carrés ne fait pas bouger la menace (1 = toutes)", category: "SimpleRP - Front : chute des zones")]
	int m_iThreatMinCells;

	// --- Réglages : économie (G4 à G7, E4), lus par SRP_FrontEconomy -----------------------------------
	[Attribute("10", UIWidgets.EditBox, "Vivres par heure d'une zone de village, de bourg ou de ville (G4)", category: "SimpleRP - Front : économie")]
	int m_iProdVivresPerHour;

	[Attribute("6", UIWidgets.EditBox, "Munitions par heure d'une zone de hameau (G4)", category: "SimpleRP - Front : économie")]
	int m_iProdMunitionsPerHour;

	[Attribute("8", UIWidgets.EditBox, "Débit par défaut d'une zone désignée carburant dans front_retouches.txt (G5)", category: "SimpleRP - Front : économie")]
	int m_iProdCarburantPerHour;

	[Attribute("6", UIWidgets.EditBox, "Débit par défaut d'une zone désignée pièces dans front_retouches.txt (G5)", category: "SimpleRP - Front : économie")]
	int m_iProdPiecesPerHour;

	[Attribute("3", UIWidgets.EditBox, "Heures de production au plus en attente dans la caisse d'une zone", category: "SimpleRP - Front : économie")]
	int m_iProductionCapHours;

	[Attribute("1", UIWidgets.CheckBox, "Une zone coupée de la base ne produit plus (E4)", category: "SimpleRP - Front : économie")]
	bool m_bProductionStopsWhenCut;

	[Attribute("1", UIWidgets.CheckBox, "Refus d'installer le dépôt sur un carré qui touche le rouge (G6)", category: "SimpleRP - Front : économie")]
	bool m_bDepotNotOnFront;

	[Attribute("1", UIWidgets.CheckBox, "Dépôt détruit quand son carré repasse rouge (G7)", category: "SimpleRP - Front : économie")]
	bool m_bDepotLostWithCell;

	[Attribute("31", UIWidgets.EditBox, "Icône du dépôt de zone sur la carte", category: "SimpleRP - Front : économie")]
	int m_iDepotMarkerIcon;

	[Attribute("7", UIWidgets.EditBox, "Couleur du dépôt de zone (7 = bleu)", category: "SimpleRP - Front : économie")]
	int m_iDepotMarkerColor;

	// --- Réglages : repères des points clés (I3), lus par SRP_FrontMarkers (serveur) ---------------------
	[Attribute("1", UIWidgets.CheckBox, "Icônes des points clés sur la carte", category: "SimpleRP - Front : repères")]
	bool m_bKeyPointMarkers;

	[Attribute("11", UIWidgets.EditBox, "Icône d'un point clé (11 = drapeau uni)", category: "SimpleRP - Front : repères")]
	int m_iKeyPointIcon;

	[Attribute("4", UIWidgets.EditBox, "Couleur d'un point clé ennemi (4 = rouge)", category: "SimpleRP - Front : repères")]
	int m_iKeyPointColorEnemy;

	[Attribute("7", UIWidgets.EditBox, "Couleur d'un point clé à nous (7 = bleu)", category: "SimpleRP - Front : repères")]
	int m_iKeyPointColorOurs;

	[Attribute("49", UIWidgets.EditBox, "Icône d'une attaque annoncée (49)", category: "SimpleRP - Front : repères")]
	int m_iAttackWarnIcon;

	[Attribute("1", UIWidgets.EditBox, "Couleur d'une attaque annoncée (1 = orange)", category: "SimpleRP - Front : repères")]
	int m_iAttackWarnColor;

	[Attribute("50", UIWidgets.EditBox, "Icône d'une attaque en cours (50)", category: "SimpleRP - Front : repères")]
	int m_iAttackIcon;

	[Attribute("4", UIWidgets.EditBox, "Couleur d'une attaque en cours (4 = rouge)", category: "SimpleRP - Front : repères")]
	int m_iAttackColor;

	[Attribute("1", UIWidgets.CheckBox, "Pas d'icône pour les zones ennemies loin du front (I3)", category: "SimpleRP - Front : repères")]
	bool m_bHideRearKeyPoints;

	// --- Réglages : radio (I5, I7), lus par SRP_FrontRadio -----------------------------------------------
	[Attribute("1", UIWidgets.CheckBox, "Une ligne radio par carré pris ou perdu au combat (I5)", category: "SimpleRP - Front : radio")]
	bool m_bRadioCells;

	[Attribute("3", UIWidgets.EditBox, "Au-delà de … carrés d'une même zone dans un passage de 5 s, une seule ligne radio", category: "SimpleRP - Front : radio")]
	int m_iRadioMergeAbove;

	[Attribute("6", UIWidgets.EditBox, "Références de carrés citées au plus dans une ligne regroupée", category: "SimpleRP - Front : radio")]
	int m_iRadioMaxRefs;

	[Attribute("1", UIWidgets.CheckBox, "Chaque carré est écrit au journal, catégorie FRONT (Staff seulement, I7)", category: "SimpleRP - Front : radio")]
	bool m_bJournalCells;

	[Attribute("1", UIWidgets.CheckBox, "Annonce « la moitié des carrés est à nous, reste le PC ennemi » (D1)", category: "SimpleRP - Front : radio")]
	bool m_bRadioHalf;

	[Attribute("60", UIWidgets.EditBox, "Rappel radio … minutes avant la perte d'une zone coupée (E4)", category: "SimpleRP - Front : radio")]
	int m_iCutReminderMinutes;

	[Attribute("1", UIWidgets.CheckBox, "Message radio au joueur à chaque passage entre territoire ami et ennemi (choix de Jack du 27/09)", category: "SimpleRP - Front : radio")]
	bool m_bRadioCrossing;

	// --- Réglages : carte du jeu (I1 à I3), lus par SRP_FrontMapLayer chez chaque joueur ------------------
	[Attribute("1", UIWidgets.CheckBox, "Dessiner le calque du front sur les cartes", category: "SimpleRP - Front : carte")]
	bool m_bMapLayer;

	[Attribute("1", UIWidgets.CheckBox, "Survol : le carré sous le curseur de la carte ressort (contour, ombre et reflet animé ; demande de Jack du 27/09)", category: "SimpleRP - Front : carte")]
	bool m_bMapHover;

	[Attribute("1", UIWidgets.CheckBox, "Calque sur la carte plein écran", category: "SimpleRP - Front : carte")]
	bool m_bMapOnFullscreen;

	[Attribute("1", UIWidgets.CheckBox, "Calque sur la carte de réapparition (modes SPAWNSCREEN et PLAIN)", category: "SimpleRP - Front : carte")]
	bool m_bMapOnSpawn;

	[Attribute("1", UIWidgets.CheckBox, "Calque sur la carte du Game Master", category: "SimpleRP - Front : carte")]
	bool m_bMapOnEditor;

	[Attribute("0.16 0.42 0.85 0.20", UIWidgets.ColorPicker, "Remplissage bleu, très transparent (I2)", category: "SimpleRP - Front : carte")]
	ref Color m_MapBlueFill;

	[Attribute("0.85 0.18 0.18 0.20", UIWidgets.ColorPicker, "Remplissage rouge (I2)", category: "SimpleRP - Front : carte")]
	ref Color m_MapRedFill;

	[Attribute("1 0.55 0.1 0.40", UIWidgets.ColorPicker, "Orange des carrés contestés (C8)", category: "SimpleRP - Front : carte")]
	ref Color m_MapContestedFill;

	[Attribute("0.08 0.08 0.08 0.90", UIWidgets.ColorPicker, "Couleur du trait de front (I1)", category: "SimpleRP - Front : carte")]
	ref Color m_MapFrontColor;

	[Attribute("4", UIWidgets.EditBox, "Épaisseur du trait de front, en pixels", category: "SimpleRP - Front : carte")]
	float m_fMapFrontWidth;

	[Attribute("1 1 1 0.55", UIWidgets.ColorPicker, "Liseré du trait de front", category: "SimpleRP - Front : carte")]
	ref Color m_MapFrontOutlineColor;

	[Attribute("7", UIWidgets.EditBox, "Épaisseur du liseré du front, en pixels", category: "SimpleRP - Front : carte")]
	float m_fMapFrontOutlineWidth;

	[Attribute("1", UIWidgets.CheckBox, "Limites des zones en trait fin", category: "SimpleRP - Front : carte")]
	bool m_bMapZoneBorders;

	[Attribute("0.1 0.1 0.1 0.35", UIWidgets.ColorPicker, "Couleur des limites de zones", category: "SimpleRP - Front : carte")]
	ref Color m_MapZoneBorderColor;

	[Attribute("1.5", UIWidgets.EditBox, "Épaisseur des limites de zones, en pixels", category: "SimpleRP - Front : carte")]
	float m_fMapZoneBorderWidth;

	[Attribute("1", UIWidgets.CheckBox, "Contour de la zone attaquée", category: "SimpleRP - Front : carte")]
	bool m_bMapAttackOutline;

	[Attribute("1 0.55 0.1 0.9", UIWidgets.ColorPicker, "Contour d'une attaque annoncée", category: "SimpleRP - Front : carte")]
	ref Color m_MapAttackWarnColor;

	[Attribute("0.9 0.1 0.1 0.9", UIWidgets.ColorPicker, "Contour d'une attaque en cours", category: "SimpleRP - Front : carte")]
	ref Color m_MapAttackColor;

	[Attribute("3", UIWidgets.EditBox, "Épaisseur du contour d'attaque, en pixels", category: "SimpleRP - Front : carte")]
	float m_fMapAttackOutlineWidth;

	[Attribute("1", UIWidgets.CheckBox, "Noms des zones sur la carte", category: "SimpleRP - Front : carte")]
	bool m_bMapZoneLabels;

	[Attribute("0.08 0.08 0.08 0.85", UIWidgets.ColorPicker, "Couleur des noms de zones", category: "SimpleRP - Front : carte")]
	ref Color m_MapLabelColor;

	[Attribute("15", UIWidgets.EditBox, "Taille des noms de zones, en pixels", category: "SimpleRP - Front : carte")]
	float m_fMapLabelSize;

	[Attribute("0.55", UIWidgets.EditBox, "Largeur estimée d'un caractère, en part de la taille (centrage des noms)", category: "SimpleRP - Front : carte")]
	float m_fMapLabelCharWidth;

	[Attribute("150", UIWidgets.EditBox, "Sous ce zoom (pixels par km), le code seul « S07 », au-dessus « Régina (S07) »", category: "SimpleRP - Front : carte")]
	float m_fMapLabelFullMinPxPerKm;

	[Attribute("190", UIWidgets.EditBox, "Points par tronçon de ligne (limite moteur de 400)", category: "SimpleRP - Front : carte")]
	int m_iMapMaxPointsPerLine;

	[Attribute("0", UIWidgets.EditBox, "Délai minimal entre deux retracés pendant un glissé, en millisecondes (0 = chaque image)", category: "SimpleRP - Front : carte")]
	int m_iMapRedrawMinMs;

	[Attribute("0", UIWidgets.EditBox, "Rang du calque par rapport à la carte (Z-order)", category: "SimpleRP - Front : carte")]
	int m_iMapZOrderOffset;

	[Attribute("", UIWidgets.EditBox, "Widget parent du calque (vide = MapFrame, sinon le parent de la carte)", category: "SimpleRP - Front : carte")]
	string m_sMapLayerParent;

	[Attribute("0", UIWidgets.CheckBox, "Écrire les noms de zones en TextWidget si TextDrawCommand ne s'affiche pas", category: "SimpleRP - Front : carte")]
	bool m_bMapTextWidgetFallback;

	[Attribute("0", UIWidgets.CheckBox, "Diagnostic : liste les widgets voisins du calque et leurs rangs", category: "SimpleRP - Front : carte")]
	bool m_bMapDiag;

	[Attribute("0", UIWidgets.CheckBox, "Calage : grille magenta d'un km et carré du joueur", category: "SimpleRP - Front : carte")]
	bool m_bMapCalibration;

	// --- Réglages : menus --------------------------------------------------------------------------------
	[Attribute("1", UIWidgets.CheckBox, "Le PC liste aussi les zones ennemies de l'arrière, en gris et sans renseignement", category: "SimpleRP - Front : menus")]
	bool m_bPCRearEnemies;

	[Attribute("30", UIWidgets.EditBox, "Nombre de copies datées proposées au Staff", category: "SimpleRP - Front : menus")]
	int m_iSavesShown;

	// --- Toutes machines ---------------------------------------------------------------------------------
	protected static SRP_FrontComponent s_Instance;
	protected bool m_bReady;							// géométrie reçue (joueur) ou construite (serveur)
	protected int m_iN;									// côté de la grille en carrés (Everon : 64)
	protected ref array<int> m_aCellZone = {};			// zone de chaque carré, -1 hors jeu
	protected ref array<int> m_aCellOwner = {};		// SRP_EFrontOwner, -1 hors jeu
	protected ref array<int> m_aCellLive = {};			// SRP_EFrontLive
	protected ref array<int> m_aCellLand = {};			// numéro de terre : 0 grande île, 1 Erquy… ; -1 mer
	protected ref array<int> m_aSeaLink = {};			// carré relié par la mer (Q5), -1 sinon
	protected ref array<ref SRP_FrontZone> m_aZones = {};			// index 0 = la base
	protected ref array<ref SRP_FrontKeyPoint> m_aKeyPoints = {};	// registre UNIQUE des points clés (trou 13)
	protected bool m_bFrozen;							// J3
	protected bool m_bVictory;							// B3
	protected int m_iCampaign = 1;						// numéro de campagne (migration : 2)
	protected int m_iGeomHash;							// somme de contrôle de la géométrie
	protected int m_iGeometryVersion;					// +1 à chaque géométrie appliquée
	protected int m_iStateVersion;						// +1 à chaque lot de carrés ou état de zone appliqué
	protected int m_iLiveVersion;						// +1 à chaque état contesté appliqué

	// --- Serveur seulement --------------------------------------------------------------------------------
	protected ref SRP_FrontCapture m_Capture;
	protected ref SRP_ZoneFall m_ZoneFall;
	protected ref SRP_FrontEconomy m_Economy;
	protected ref array<ref SRP_FrontLocality> m_aLocalities = {};
	protected int m_iThreat;							// 0 à 10 (G9)
	protected int m_iBridgeVersion;						// +1 par carré qui change de camp, SAUVEGARDÉE, ne redescend jamais (pont)
	protected int m_iRestoreCount;						// +1 à chaque retour à une copie datée (J2), sauvegardé
	protected ref array<int> m_aPendingCells = {};		// changements à répliquer : (carré << 1) | propriétaire
	protected ref array<int> m_aLiveCells = {};		// carrés dont l'état contesté est à répliquer (envoyés en (carré << 2) | état)
	protected ref array<int> m_aLinkStamp = {};		// parcours E4 : numéro de passe par carré (jamais remis à zéro)
	protected ref array<int> m_aQueue = {};			// file du parcours en largeur, réutilisée
	protected int m_iStamp;
	protected bool m_bStatePending;					// un état de zone ou le gel est à répliquer
	protected bool m_bFlushQueued;						// un CallLater(FlushChanges, 0) est déjà demandé
	protected bool m_bTopologyDirty = true;			// contact, front et liaison à refaire (E1, F1, E4)
	protected bool m_bLoaded;							// une instance qui n'a pas chargé n'écrit jamais (Workbench)
	protected bool m_bDirty;							// front.json à écrire
	protected int m_iDirtySince;						// heure Unix du premier changement non écrit
	protected bool m_bSaveNow;							// écrire au prochain passage
	protected int m_iLastLiveSend;						// dernier envoi d'état contesté, GetTickCount (non sauvé)
	protected int m_iFrozenSince;						// heure Unix du gel (décalage E4 au dégel)
	protected int m_iTickCount;							// compteur de boucles (sous-cadence de l'entretien)
	protected int m_iMapTries;							// essais d'attente de la carte au démarrage
	protected string m_sCampaignStart;					// date de début de campagne
	protected string m_sVictoryAt;						// date de la victoire
	protected string m_sLastDaily;						// jour de la dernière copie datée (AAAA-MM-JJ)
	protected ref array<string> m_aDailyCopies = {};	// jours des copies gardées, du plus ancien au plus récent
	protected ref map<string, string> m_mExtra = new map<string, string>();	// petites valeurs des autres modules
	protected bool m_bMigrated;							// K1/K2 faits
	protected int m_iLegacyVivres;						// K2 : reste à verser
	protected int m_iLegacyMunitions;
	protected int m_iLegacyNextTry;						// heure Unix du prochain essai de versement
	protected int m_iStartUnix;							// heure Unix du démarrage (délai K2)
	protected string m_sLegacyInfo;						// résumé de l'import K1, pour le journal et le Staff
	protected bool m_bEnemyChained;						// l'ennemi du front était présent au démarrage (Start) : front.json porte fe_* et cmd_*
	protected bool m_bEnemyFinalSaved;					// l'ennemi a fait écrire la sauvegarde finale à son OnDelete

	// --- Ajouts du socle (non contractuels) ------------------------------------------------------------
	protected static const int NET_MAX = 268435455;		// plus grand entier envoyé (28 bits, jamais le signe)
	protected static const string HEX = "0123456789ABCDEF";
	protected static const string PREV_PATH = "$profile:SimpleRP/front.json.prec";	// version précédente de front.json (arrêt brutal)
	protected float m_fGridCell;						// taille d'un carré adoptée (serveur : attribut ; joueur : reçue)
	protected int m_iLandCells;							// carrés en jeu (géométrie)
	protected int m_iOwnerStamp;						// +1 à chaque propriétaire changé (cache de GetCellsString)
	protected int m_iCellsCacheStamp = -1;
	protected string m_sCellsCache;
	protected int m_iZoneMapCacheVersion = -1;
	protected string m_sZoneMapCache;
	protected bool m_bSaveQueued;						// un CallLater(SaveQueued, 0) est demandé
	protected bool m_bLiveQueued;						// un CallLater(SendLive, …) est demandé
	protected bool m_bChainErrorLogged;					// refus de sauvegarde (ennemi parti) déjà écrit
	protected bool m_bSaveErrorLogged;					// échec d'écriture de front.json déjà écrit (une ligne, pas une toutes les 5 s)
	protected bool m_bPathGood;							// front.json sur le disque est bon (relu ou écrit ici) : copiable dans PREV_PATH
	protected int m_iVictoryUnix;						// heure Unix de la victoire (nouvelle campagne différée)
	protected ref array<string> m_aDailySummaries = {};	// résumé de chaque copie datée (même ordre que m_aDailyCopies)
	protected ref array<string> m_aGeomWarnings = {};	// avertissements du bâtisseur (rapport Staff)
	protected int m_iMissingKeyPoints;					// points clés manquants (Q8)
	protected ref array<int> m_aCutReminded = {};		// E4 : m_iCutSince déjà rappelé, par zone (non sauvé)
	protected ref array<string> m_aOrphanZones = {};	// zones du fichier au code inconnu : code…
	protected ref array<string> m_aOrphanData = {};		// … et leurs champs, gardés tels quels pour la sauvegarde

	//================================================================================================
	// Accès et cycle de vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! L'instance du game mode (serveur et joueurs), null avant OnPostInit — appelé par tout le mod
	static SRP_FrontComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes machines : s_Instance. Serveur : EnsureDirectories, dossiers front_jours et archives, crée
	//! m_Capture, m_ZoneFall, m_Economy, puis CallLater(Start, 5000) (jamais de travail ici : instance jetable du Workbench)
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		m_fGridCell = m_fCellSize;
		if (!Replication.IsServer() || SCR_Global.IsEditMode(owner))
			return;

		SRP_Paths.EnsureDirectories();
		FileIO.MakeDirectory(DAILY_DIR);
		FileIO.MakeDirectory(ARCHIVE_DIR);
		m_Capture = new SRP_FrontCapture(this);
		m_ZoneFall = new SRP_ZoneFall(this);
		m_Economy = new SRP_FrontEconomy(this);
		GetGame().GetCallqueue().CallLater(Start, 5000, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : retire Start, Tick et FlushChanges de la file, puis Save() si m_bLoaded (SRP_Territory.c:534-544) ;
	//! si l'ennemi est déjà parti, Save refuse d'écrire un fichier amputé (voir Save) — le moteur
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Start);
			GetGame().GetCallqueue().Remove(Tick);
			GetGame().GetCallqueue().Remove(FlushChanges);
			GetGame().GetCallqueue().Remove(SaveQueued);
			GetGame().GetCallqueue().Remove(SendLive);
			if (m_bLoaded)
				Save();
		}
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : un joueur part, la capture oublie sa position et son exclusion (C3) — appelé par le jeu
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);
		if (Replication.IsServer() && m_Capture)
			m_Capture.OnPlayerDisconnected(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, 5 s après OnPostInit, voir l'ORDRE DE DÉMARRAGE de CONTRATS_FRONT.md : carte prête, réglages,
	//! géométrie, SRP_Commander.BuildRegions, Load (ou campagne neuve + K1), m_bEnemyChained = (SRP_FrontEnemyComponent
	//! présent), m_bReady, envoi, E4, victoire, boucle 5 s
	protected void Start()
	{
		if (!Replication.IsServer() || m_bReady)
			return;

		// 1. La carte du jeu (taille de la grille, noms des lieux)
		if (!SCR_MapEntity.GetMapInstance())
		{
			m_iMapTries++;
			if (m_iMapTries <= m_iMapRetryMax)
			{
				GetGame().GetCallqueue().CallLater(Start, Math.MaxInt(m_iMapRetrySeconds, 1) * 1000, false);
				return;
			}
			// Le bâtisseur échoue sans carte (aucune localité, rien à sauvegarder) : le front restera arrêté
			Note("ERREUR", string.Format("Front : la carte du jeu n'est toujours pas prête après %1 essai(s) ; construction tentée quand même (elle échouera sans carte)", m_iMapRetryMax));
		}
		int now = System.GetUnixTime();
		m_iStartUnix = now;

		// 2. Réglages : déclaration, lecture des deux fichiers (lecteur unique), puis relecture par chaque module
		DeclareSettings();
		SRP_CmdSettings.Reload("démarrage");
		LoadSettings();
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.LoadSettings();
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
			missions.LoadFrontSettings();

		// 3. Géométrie (déterministe), adoptée telle quelle ; la liste des repères est vidée une fois lue
		m_fGridCell = m_fCellSize;
		SRP_FrontGeometryBuilder builder = new SRP_FrontGeometryBuilder(this);
		bool built = builder.Build();
		SRP_KeyPointComponent.ClearDefs();
		if (!built || !AdoptGeometry(builder))
		{
			builder.WriteReport();	// « construction interrompue » : au moins les avertissements, jamais le rapport d'avant
			Note("ERREUR", "Front : construction de la grille impossible (voir la console et " + REPORT_PATH + ") ; le front reste arrêté");
			return;
		}
		builder.WriteReport();

		// 4. Régions du Commandeur, tracées AVANT la lecture de leur état
		SRP_Commander commander = SRP_Commander.Get();
		if (commander)
			commander.BuildRegions();

		// 5. État : front.json, sinon campagne neuve (et import de l'ancien territoire, K1)
		bool freshFile = false;
		if (FileIO.FileExists(PATH))
		{
			if (Load())
			{
				m_bLoaded = true;
				if (FileIO.FileExists(OLD_PATH))
					Note("FRONT", "territoire.json est encore présent mais ignoré : front.json existe déjà (supprimer front.json pour rejouer la migration)");
			}
			else
			{
				// Surtout ne pas repartir à neuf par-dessus : rien ne sera écrit (m_bLoaded reste faux). Les copies datées
				// sont relues sur le disque : c'est la réparation proposée au Staff (liste d'habitude tenue par front.json)
				NewCampaignState();
				RebuildDailyList();
				string alert = "Front : " + PATH + " est illisible. Rien ne sera écrit tant qu'il n'est pas réparé ou qu'une copie datée n'est pas rechargée (menu Staff, Territoire, sauvegardes)";
				Note("ERREUR", alert);
				NotifyOfficers(alert);
			}
		}
		else
		{
			NewCampaignState();
			m_iCampaign = 1;
			m_sCampaignStart = SRP_Time.Now();
			m_bLoaded = true;
			freshFile = true;
			if (m_bLegacyImport && FileIO.FileExists(OLD_PATH))
				ImportLegacy();
		}

		// 6. Garde de la chaîne de sauvegarde, puis les modules et l'envoi aux joueurs déjà là
		if (SRP_FrontEnemyComponent.GetInstance())
			m_bEnemyChained = true;
		m_bReady = true;
		if (m_Capture)
			m_Capture.Start();
		if (m_ZoneFall)
			m_ZoneFall.Start();
		if (m_Economy)
			m_Economy.Start();
		BroadcastGeometry();
		m_aPendingCells.Clear();
		m_bStatePending = false;
		BroadcastState();

		// 7. Topologie, puis E4 : le temps passé serveur éteint compte (Q1)
		m_bTopologyDirty = true;
		UpdateTopology();
		CheckCuts(now);

		// 8. Victoire enregistrée : nouvelle campagne au redémarrage (B3)
		if (m_bVictory && m_iVictoryResetMinutes <= 0)
			ResetCampaign("victoire", "");

		// 9. LA boucle unique
		GetGame().GetCallqueue().CallLater(Tick, TICK_MS, true);

		// 10. Résumé technique : catégorie FRONT (Staff, journal-serveur), jamais le fil public à chaque démarrage ; les
		// avertissements du bâtisseur sont détaillés dans le rapport
		string summary = string.Format("Front : %1 carrés, %2 zones, base %3 carrés, %4 localités, %5 points clés posés, %6 manquants", CountLandCells(), CountZonesTotal(), CountZoneCells(0), CountFrontLocalities(), CountPlacedKeyPoints(), m_iMissingKeyPoints);
		if (!m_aGeomWarnings.IsEmpty())
			summary += string.Format(", %1 avertissement(s), voir front_zones.txt", m_aGeomWarnings.Count());
		Note("FRONT", summary);

		// 11. Fichier neuf
		if (freshFile)
			Save();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, LA boucle unique de 5 s (trou 7) : capture.Tick -> zones.Tick -> économie.Tick -> SRP_FrontRadio.Flush
	//! -> SRP_FrontMarkers.Tick, puis l'entretien une fois sur HOUSEKEEPING_TICKS
	protected void Tick()
	{
		if (!m_bReady)
			return;
		int now = System.GetUnixTime();
		if (m_Capture)
			m_Capture.Tick(now);
		if (m_ZoneFall)
			m_ZoneFall.Tick(now);
		if (m_Economy)
			m_Economy.Tick(now);
		SRP_FrontRadio.Flush();
		SRP_FrontMarkers.Tick();

		m_iTickCount++;
		if (m_iTickCount >= HOUSEKEEPING_TICKS)
		{
			m_iTickCount = 0;
			Housekeeping(now);
		}
		SaveIfDue(now);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, toutes les 60 s (sous-cadence de Tick) : sauvegarde en retard, copie du jour, CheckCuts (E4),
	//! rappels E4, versement K2 (MigrationTick), victoire différée (m_iVictoryResetMinutes)
	protected void Housekeeping(int nowUnix)
	{
		if (!m_bReady)
			return;
		UpdateTopology();
		CheckCuts(nowUnix);
		MigrationTick(nowUnix);

		// Victoire : nouvelle campagne différée (au moins 30 s, pour que le pont photographie l'île toute bleue)
		if (m_bVictory && m_iVictoryResetMinutes > 0 && m_iVictoryUnix > 0)
		{
			int wait = Math.MaxInt(m_iVictoryResetMinutes * 60, 30);
			if (nowUnix - m_iVictoryUnix >= wait)
			{
				ResetCampaign("victoire", "");
				return;
			}
		}

		// Changement de jour : écriture forcée pour avoir la copie datée (J2)
		string today = Today();
		if (m_bLoaded && !today.IsEmpty() && today != m_sLastDaily)
			MarkDirty(true);
		SaveIfDue(nowUnix);
	}

	//------------------------------------------------------------------------------------------------
	//! Le front est construit (serveur) ou reçu (joueur) — appelé par tout le mod avant toute lecture
	bool IsReady()
	{
		return m_bReady;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : la capture (C, F3, F5, recensement unique) — appelé par la chute, l'ennemi, le Commandeur (EQ4)
	SRP_FrontCapture GetCapture()
	{
		return m_Capture;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : la seule porte des changements de zone (CaptureZone, LoseZone, ForceZone) — appelé par l'ennemi (H2),
	//! les missions, le Staff
	SRP_ZoneFall GetZoneFall()
	{
		return m_ZoneFall;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : production et dépôts de zone (G4 à G7) — appelé par SRP_Bridge (boxes), SRP_Admin, SRP_DepotInstallAction
	SRP_FrontEconomy GetEconomy()
	{
		return m_Economy;
	}

	//================================================================================================
	// Réglages (arbitrage Q6) : un seul lecteur, SRP_CmdSettings, fichier front_reglages.txt pour les clés front_*
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Serveur : SRP_CmdSettings.Declare de chaque clé front_* du socle, de la capture, de la chute, de l'économie et
	//! de la radio (liste au §7 de CONTRATS_FRONT.md), avec la valeur de l'attribut comme défaut, puis
	//! SRP_MissionManagerComponent.DeclareFrontSettings — appelé par Start AVANT SRP_CmdSettings.Reload
	protected void DeclareSettings()
	{
		// Capture (C3, C4, C7, F3, F5, D5)
		SRP_CmdSettings.DeclareInt("front_capture_minutes", m_iSquareCaptureMinutes, "Minutes de présence sans ennemi pour prendre un carré (C4 ; 1 pour les essais)");
		SRP_CmdSettings.DeclareString("front_bloc_genres", m_sBlockKinds, "Genres de localités dont le centre se prend d'un seul bloc (ville, village, hameau ; séparés par des virgules)");
		SRP_CmdSettings.DeclareFloat("front_bloc_rayon_m", m_fBlockRadius, "Bloc d'une localité : carrés de sa zone à moins de tant de mètres de son centre (0 = aucun bloc)");
		SRP_CmdSettings.DeclareInt("front_bloc_minutes", m_iBlockMinutes, "Minutes de présence sans ennemi dans le bloc pour prendre tous ses carrés d'un coup");
		SRP_CmdSettings.DeclareFloat("front_capture_redescente", m_fCaptureDecayRatio, "Vitesse de redescente de la progression sans personne, comparée à la montée (C7 ; 1 = même vitesse)");
		SRP_CmdSettings.DeclareInt("front_exclusion_minutes", m_iExcludeMinutes, "Minutes pendant lesquelles un joueur ne compte pas après la fermeture du Game Master ou une possession (C3 ; 0 = seulement pendant ; la téléportation ne compte plus)");
		SRP_CmdSettings.DeclareInt("front_reprise_secondes", m_iRetakeSeconds, "Secondes d'occupation par un soldat de l'attaque, sans joueur, pour reprendre un carré (F3, Q2)");
		SRP_CmdSettings.DeclareFloat("front_garde_point_cle_m", m_fKeyPointGuardRadius, "Un joueur à moins de tant de mètres d'un point clé empêche la reprise de son carré (F5, Q2)");
		SRP_CmdSettings.DeclareFloat("front_saisie_rayon_m", m_fKeyPointClearRadius, "Aucun ennemi en état de combattre à moins de tant de mètres du point clé pour saisir le poste (D5)");

		// Chute et perte des zones, menace (D1, Q2, Q4, G9)
		SRP_CmdSettings.DeclareInt("front_chute_part", m_iZoneFallPercent, "Part minimale de carrés bleus, sur 100, pour qu'une zone avec localité tombe une fois ses points clés saisis (D1)");
		SRP_CmdSettings.DeclareInt("front_chute_sans_village_part", m_iNoLocalityFallPercent, "Part de carrés bleus, sur 100, pour qu'une zone sans localité tombe (Q4)");
		SRP_CmdSettings.DeclareInt("front_perte_part", m_iZoneLossPercent, "Une zone à nous repasse rouge quand plus de cette part, sur 100, de ses carrés est rouge et ses points clés repris (Q2)");
		SRP_CmdSettings.DeclareInt("front_menace_prise", m_iThreatPerZoneTaken, "Menace ajoutée par zone prise (G9, Q6)");
		SRP_CmdSettings.DeclareInt("front_menace_perte", m_iThreatPerZoneLost, "Menace retirée par zone perdue (G9, Q6)");

		// Coupure, sauvegarde, victoire (E4, Q1, J2, B3)
		SRP_CmdSettings.DeclareInt("front_coupure_minutes", m_iCutLossMinutes, "Minutes réelles au bout desquelles une zone coupée de la base repasse rouge (E4, Q1 : 1440 = 24 h ; 2 pour les essais)");
		SRP_CmdSettings.DeclareInt("front_coupure_rappel_minutes", m_iCutReminderMinutes, "Rappel radio tant de minutes avant la perte d'une zone coupée (E4)");
		SRP_CmdSettings.DeclareInt("front_sauvegarde_secondes", m_iSaveDelaySeconds, "front.json est écrit au plus tant de secondes après un changement de carré");
		SRP_CmdSettings.DeclareInt("front_copies_jours", m_iDailyKeep, "Copies datées de front.json gardées, une par jour (J2)");
		SRP_CmdSettings.DeclareInt("front_victoire_minutes", m_iVictoryResetMinutes, "Minutes entre la victoire et la nouvelle campagne ; 0 = au prochain démarrage du serveur (B3)");

		// Production des zones (G4, G5)
		SRP_CmdSettings.DeclareInt("front_prod_vivres", m_iProdVivresPerHour, "Vivres par heure d'une zone de village, de bourg ou de ville (G4)");
		SRP_CmdSettings.DeclareInt("front_prod_munitions", m_iProdMunitionsPerHour, "Munitions par heure d'une zone de hameau (G4)");
		SRP_CmdSettings.DeclareInt("front_prod_carburant", m_iProdCarburantPerHour, "Débit par défaut d'une zone désignée carburant dans front_retouches.txt (G5)");
		SRP_CmdSettings.DeclareInt("front_prod_pieces", m_iProdPiecesPerHour, "Débit par défaut d'une zone désignée pièces dans front_retouches.txt (G5)");
		SRP_CmdSettings.DeclareInt("front_prod_plafond_heures", m_iProductionCapHours, "Heures de production au plus en attente dans la caisse d'une zone (G4)");

		// Radio (I5)
		SRP_CmdSettings.DeclareBool("front_radio_carres", m_bRadioCells, "Une ligne radio par carré pris ou perdu au combat (I5 ; 1 = oui, 0 = non)");
		SRP_CmdSettings.DeclareBool("front_radio_passage", m_bRadioCrossing, "Message radio au joueur à chaque passage entre territoire ami et ennemi (1 = oui, 0 = non)");
		SRP_CmdSettings.DeclareInt("front_radio_regroupe", m_iRadioMergeAbove, "Au-delà de tant de carrés d'une même zone dans un passage de 5 s, une seule ligne radio (I5)");

		// Clés du front tenues par les missions (primes, captures, contrôle routier)
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
			missions.DeclareFrontSettings();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : relit les clés front_* dans les attributs publics ci-dessus (une clé absente garde l'attribut) —
	//! appelé par Start et ReloadSettings
	protected void LoadSettings()
	{
		if (!Replication.IsServer())
			return;
		m_iSquareCaptureMinutes = SRP_CmdSettings.GetIntClamped("front_capture_minutes", 1, 240);
		m_fCaptureDecayRatio = SRP_CmdSettings.GetFloatClamped("front_capture_redescente", 0, 10);
		m_sBlockKinds = SRP_CmdSettings.GetString("front_bloc_genres");
		m_fBlockRadius = SRP_CmdSettings.GetFloatClamped("front_bloc_rayon_m", 0, 3000);
		m_iBlockMinutes = SRP_CmdSettings.GetIntClamped("front_bloc_minutes", 1, 240);
		m_iExcludeMinutes = SRP_CmdSettings.GetIntClamped("front_exclusion_minutes", 0, 120);
		m_iRetakeSeconds = SRP_CmdSettings.GetIntClamped("front_reprise_secondes", 5, 3600);
		m_fKeyPointGuardRadius = SRP_CmdSettings.GetFloatClamped("front_garde_point_cle_m", 0, 1000);
		m_fKeyPointClearRadius = SRP_CmdSettings.GetFloatClamped("front_saisie_rayon_m", 0, 1000);

		m_iZoneFallPercent = SRP_CmdSettings.GetIntClamped("front_chute_part", 1, 100);
		m_iNoLocalityFallPercent = SRP_CmdSettings.GetIntClamped("front_chute_sans_village_part", 1, 100);
		m_iZoneLossPercent = SRP_CmdSettings.GetIntClamped("front_perte_part", 0, 99);
		m_iThreatPerZoneTaken = SRP_CmdSettings.GetIntClamped("front_menace_prise", 0, 10);
		m_iThreatPerZoneLost = SRP_CmdSettings.GetIntClamped("front_menace_perte", 0, 10);

		m_iCutLossMinutes = SRP_CmdSettings.GetIntClamped("front_coupure_minutes", 1, 20160);
		m_iCutReminderMinutes = SRP_CmdSettings.GetIntClamped("front_coupure_rappel_minutes", 0, 1440);
		m_iSaveDelaySeconds = SRP_CmdSettings.GetIntClamped("front_sauvegarde_secondes", 5, 600);
		m_iDailyKeep = SRP_CmdSettings.GetIntClamped("front_copies_jours", 1, 365);
		m_iVictoryResetMinutes = SRP_CmdSettings.GetIntClamped("front_victoire_minutes", 0, 10080);

		m_iProdVivresPerHour = SRP_CmdSettings.GetIntClamped("front_prod_vivres", 0, 1000);
		m_iProdMunitionsPerHour = SRP_CmdSettings.GetIntClamped("front_prod_munitions", 0, 1000);
		m_iProdCarburantPerHour = SRP_CmdSettings.GetIntClamped("front_prod_carburant", 0, 1000);
		m_iProdPiecesPerHour = SRP_CmdSettings.GetIntClamped("front_prod_pieces", 0, 1000);
		m_iProductionCapHours = SRP_CmdSettings.GetIntClamped("front_prod_plafond_heures", 1, 48);

		m_bRadioCells = SRP_CmdSettings.GetBool("front_radio_carres");
		m_iRadioMergeAbove = SRP_CmdSettings.GetIntClamped("front_radio_regroupe", 1, 100);
		m_bRadioCrossing = SRP_CmdSettings.GetBool("front_radio_passage");
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, bouton Staff « Relire les réglages » : SRP_CmdSettings.Reload(author), puis LoadSettings ici,
	//! SRP_FrontEnemyComponent.LoadSettings (qui relaie au Commandeur) et SRP_MissionManagerComponent.LoadFrontSettings
	//! — rend le compte rendu (SRP_FrontScreens, SRP_Admin, SRP_CmdScreens)
	string ReloadSettings(string author)
	{
		if (!Replication.IsServer())
			return "";
		string report = SRP_CmdSettings.Reload(author);
		LoadSettings();
		// Blocs de localité refaits avec le genre et le rayon relus (leurs progressions repartent de zéro)
		if (m_Capture && m_bReady)
			m_Capture.BuildBlocks();
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.LoadSettings();
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (missions)
			missions.LoadFrontSettings();
		return report;
	}

	//================================================================================================
	// Lecture des carrés (toutes machines ; -1 hors grille ou hors jeu ; aucune allocation)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Côté de la grille en carrés (Everon : 64) — carte, pont
	int GetGridSize()
	{
		return m_iN;
	}

	//------------------------------------------------------------------------------------------------
	//! Taille d'un carré en mètres (200, A1) — carte, pont
	float GetCellSize()
	{
		if (m_fGridCell > 0)
			return m_fGridCell;
		return m_fCellSize;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de carrés de la grille (N x N = 4096), en jeu ou non — capture (dimension des tableaux)
	int GetCellCount()
	{
		return m_iN * m_iN;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré sous une position : gz x N + gx, gz compté depuis le sud ; -1 hors grille ou en mer — tout le mod
	int CellIndexAt(vector position)
	{
		float size = GetCellSize();
		if (m_iN <= 0 || size <= 0)
			return -1;
		int gx = Math.Floor(position[0] / size);
		int gz = Math.Floor(position[2] / size);
		if (gx < 0 || gz < 0 || gx >= m_iN || gz >= m_iN)
			return -1;
		int cell = gz * m_iN + gx;
		if (cell >= m_aCellZone.Count() || m_aCellZone[cell] < 0)
			return -1;
		return cell;
	}

	//------------------------------------------------------------------------------------------------
	//! Colonne gx d'un carré (sans opérateur modulo) — géométrie, carte
	int CellCol(int cell)
	{
		if (m_iN <= 0 || cell < 0 || cell >= m_iN * m_iN)
			return -1;
		return cell - (cell / m_iN) * m_iN;
	}

	//------------------------------------------------------------------------------------------------
	//! Rangée gz d'un carré, depuis le sud — géométrie, carte
	int CellRow(int cell)
	{
		if (m_iN <= 0 || cell < 0 || cell >= m_iN * m_iN)
			return -1;
		return cell / m_iN;
	}

	//------------------------------------------------------------------------------------------------
	//! Centre d'un carré, posé au sol (GetSurfaceY) ; vector.Zero hors grille — ennemi (vagues), Commandeur
	vector CellCenter(int cell)
	{
		if (m_iN <= 0 || cell < 0 || cell >= m_iN * m_iN)
			return vector.Zero;
		float size = GetCellSize();
		float fx = CellCol(cell);
		float fz = CellRow(cell);
		float x = (fx + 0.5) * size;
		float z = (fz + 0.5) * size;
		float y = 0;
		BaseWorld world = GetGame().GetWorld();
		if (world)
			y = world.GetSurfaceY(x, z);
		return Vector(x, y, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Référence de carte du coin sud-ouest, « 074 042 » (A7, même format que SRP_Fleet.c:802) — radio, Staff, pont
	string CellRef(int cell)
	{
		if (m_iN <= 0 || cell < 0 || cell >= m_iN * m_iN)
			return "";
		float size = GetCellSize();
		float fx = CellCol(cell);
		float fz = CellRow(cell);
		int refX = Math.Floor(fx * size / 100.0 + 0.01);
		int refZ = Math.Floor(fz * size / 100.0 + 0.01);
		return Pad3(refX) + " " + Pad3(refZ);
	}

	//------------------------------------------------------------------------------------------------
	//! Inverse de CellRef (« 074 042 » ou « 75 43 » : gx = a / 2) ; -1 si illisible — retouches, Staff
	int CellFromRef(string cellRef)
	{
		float size = GetCellSize();
		if (m_iN <= 0 || size <= 0)
			return -1;
		string text = cellRef.Trim();
		text.Replace(",", " ");
		text.Replace(";", " ");
		array<string> parts = {};
		text.Split(" ", parts, true);
		if (parts.Count() != 2)
			return -1;
		string first = parts[0].Trim();
		string second = parts[1].Trim();
		if (!SRP_Utils.IsNumeric(first) || !SRP_Utils.IsNumeric(second))
			return -1;
		float a = first.ToInt();
		float b = second.ToInt();
		int gx = Math.Floor(a * 100.0 / size + 0.001);
		int gz = Math.Floor(b * 100.0 / size + 0.001);
		if (gx < 0 || gz < 0 || gx >= m_iN || gz >= m_iN)
			return -1;
		return gz * m_iN + gx;
	}

	//------------------------------------------------------------------------------------------------
	//! Voisin par un côté, dir 0 N, 1 E, 2 S, 3 O ; -1 hors grille ou hors jeu (sans la liaison maritime) — ennemi, capture
	int Neighbour(int cell, int dir)
	{
		if (m_iN <= 0 || cell < 0 || cell >= m_iN * m_iN || cell >= m_aCellZone.Count())
			return -1;
		int gz = cell / m_iN;
		int gx = cell - gz * m_iN;
		if (dir == 0)
			gz++;
		else if (dir == 1)
			gx++;
		else if (dir == 2)
			gz--;
		else if (dir == 3)
			gx--;
		else
			return -1;
		if (gx < 0 || gz < 0 || gx >= m_iN || gz >= m_iN)
			return -1;
		int next = gz * m_iN + gx;
		if (m_aCellZone[next] < 0)
			return -1;
		return next;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré relié par la mer (Q5 : Erquy), -1 sinon — capture (C1), calque, parcours E4
	int GetSeaLink(int cell)
	{
		if (cell < 0 || cell >= m_aSeaLink.Count())
			return -1;
		return m_aSeaLink[cell];
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré est-il en jeu (terre gardée, A2) ? — tout le mod
	bool IsCellInPlay(int cell)
	{
		if (cell < 0 || cell >= m_aCellZone.Count())
			return false;
		return m_aCellZone[cell] >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EFrontOwner du carré, -1 hors jeu — tout le mod
	int GetCellOwner(int cell)
	{
		if (!IsCellInPlay(cell) || cell >= m_aCellOwner.Count())
			return HORS_JEU;
		return m_aCellOwner[cell];
	}

	//------------------------------------------------------------------------------------------------
	//! Zone du carré (0 = base), -1 hors jeu — tout le mod
	int GetCellZone(int cell)
	{
		if (cell < 0 || cell >= m_aCellZone.Count())
			return -1;
		return m_aCellZone[cell];
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EFrontLive du carré (C8) — carte (orange), PC, Staff
	int GetCellLive(int cell)
	{
		if (cell < 0 || cell >= m_aCellLive.Count())
			return SRP_EFrontLive.AUCUN;
		return m_aCellLive[cell];
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré est-il contesté (état différent d'AUCUN) ? — carte (orange, I1), pont (cs)
	bool IsCellContested(int cell)
	{
		return GetCellLive(cell) != SRP_EFrontLive.AUCUN;
	}

	//------------------------------------------------------------------------------------------------
	//! Un des 4 côtés en jeu a l'autre propriétaire (sans la liaison maritime) — missions (G6), ennemi, calque (tracé)
	bool IsCellAtFront(int cell)
	{
		int owner = GetCellOwner(cell);
		if (owner < 0)
			return false;
		for (int dir = 0; dir < 4; dir++)
		{
			int next = Neighbour(cell, dir);
			if (next >= 0 && m_aCellOwner[next] >= 0 && m_aCellOwner[next] != owner)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Un des 4 côtés (et la liaison maritime si withSeaLink) appartient à owner — chute (D2), capture, économie
	bool HasNeighbourOwnedBy(int cell, int owner, bool withSeaLink)
	{
		if (!IsCellInPlay(cell))
			return false;
		for (int dir = 0; dir < 4; dir++)
		{
			int next = Neighbour(cell, dir);
			if (next >= 0 && m_aCellOwner[next] == owner)
				return true;
		}
		if (withSeaLink)
		{
			int link = GetSeaLink(cell);
			if (link >= 0 && IsCellInPlay(link) && m_aCellOwner[link] == owner)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! C1 + Q5 : carré rouge, hors base, voisin bleu par un côté ou par la mer, front non gelé — capture (seule à capturer)
	bool CanBlueCapture(int cell)
	{
		if (m_bFrozen || GetCellOwner(cell) != SRP_EFrontOwner.ROUGE || IsBaseCell(cell))
			return false;
		return HasNeighbourOwnedBy(cell, SRP_EFrontOwner.BLEU, true);
	}

	//------------------------------------------------------------------------------------------------
	//! F3 + B4 : carré bleu, hors base, hors zone protégée bleue, front non gelé — capture (reprise)
	bool CanRedRetake(int cell)
	{
		if (m_bFrozen || GetCellOwner(cell) != SRP_EFrontOwner.BLEU || IsBaseCell(cell))
			return false;
		int zone = m_aCellZone[cell];
		if (IsZoneProtected(zone) && GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré de la zone de la base (B2) — capture, Commandeur (sécurité des appuis)
	bool IsBaseCell(int cell)
	{
		return GetCellZone(cell) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré d'une zone protégée (B2 + B4) — capture (F3), ennemi
	bool IsCellProtected(int cell)
	{
		int zone = GetCellZone(cell);
		if (zone < 0)
			return false;
		return IsZoneProtected(zone);
	}

	//------------------------------------------------------------------------------------------------
	//! Numéro de terre du carré : 0 grande île, 1 et suivants les îles gardées, -1 mer — capture, calque
	int GetCellLand(int cell)
	{
		if (cell < 0 || cell >= m_aCellLand.Count())
			return -1;
		return m_aCellLand[cell];
	}

	//================================================================================================
	// Lecture par position (toutes machines ; faux en mer)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Carré rouge sous la position — missions (G10, G13), ennemi (G12, F10), civils (G14), Commandeur
	bool IsRedAt(vector position)
	{
		return GetOwnerAt(position) == SRP_EFrontOwner.ROUGE;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré bleu sous la position — missions (G10), civils (G14), livraisons
	bool IsBlueAt(vector position)
	{
		return GetOwnerAt(position) == SRP_EFrontOwner.BLEU;
	}

	//------------------------------------------------------------------------------------------------
	//! Propriétaire sous la position, -1 en mer — missions, ennemi
	int GetOwnerAt(vector position)
	{
		int cell = CellIndexAt(position);
		if (cell < 0)
			return HORS_JEU;
		return GetCellOwner(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Zone sous la position, -1 en mer — missions, Commandeur, Staff
	int GetZoneAt(vector position)
	{
		int cell = CellIndexAt(position);
		if (cell < 0)
			return -1;
		return m_aCellZone[cell];
	}

	//------------------------------------------------------------------------------------------------
	//! La position est-elle dans la zone de la base (B2) ? — missions (m_bCaptureIgnoreBase), Commandeur
	bool IsInBaseZone(vector position)
	{
		return GetZoneAt(position) == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Direction moyenne (plan XZ) des carrés rouges entre 500 et 3000 m ; angle en degrés dans angle ; rend le nom
	//! de la zone rouge la plus proche, ou "" s'il n'y a pas de rouge — SRP_HeliSearch (remplace EnemySectorDirection)
	// Angle compté comme Math.Atan2(dz, dx) (0 = est, 90 = nord), converti en degrés ; nom = libellé « Régina (S07) »
	string RedDirection(vector center, out float angle)
	{
		angle = 0;
		if (!m_bReady || m_iN <= 0)
			return "";
		float size = GetCellSize();
		float sumX = 0;
		float sumZ = 0;
		float bestDistance = 1000000;
		float bestX = 0;
		float bestZ = 0;
		int bestZone = -1;
		int count = Math.MinInt(m_iN * m_iN, m_aCellOwner.Count());
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aCellOwner[cell] != SRP_EFrontOwner.ROUGE)
				continue;
			float fx = CellCol(cell);
			float fz = CellRow(cell);
			float dx = (fx + 0.5) * size - center[0];
			float dz = (fz + 0.5) * size - center[2];
			float distance = Math.Sqrt(dx * dx + dz * dz);
			if (distance < 500 || distance > 3000)
				continue;
			sumX += dx / distance;
			sumZ += dz / distance;
			if (distance < bestDistance)
			{
				bestDistance = distance;
				bestX = dx;
				bestZ = dz;
				bestZone = m_aCellZone[cell];
			}
		}
		if (bestZone < 0)
			return "";
		if (Math.AbsFloat(sumX) < 0.001 && Math.AbsFloat(sumZ) < 0.001)
		{
			sumX = bestX;
			sumZ = bestZ;
		}
		angle = Math.Atan2(sumZ, sumX) * Math.RAD2DEG;
		return GetZoneLabel(bestZone);
	}

	//================================================================================================
	// Lecture des zones (toutes machines)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Nombre d'entrées de zone, base comprise (index 0) ; toujours sous 127 — tout le mod
	int GetZoneCount()
	{
		return m_aZones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de zones jouables (sans la base), la référence des textes « 12 zones sur 66 » — PC, Staff, pont
	int CountZonesTotal()
	{
		return Math.MaxInt(m_aZones.Count() - 1, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Zones jouables à nous (sans la base) — PC, Staff, pont (territory.ours)
	int CountZonesOurs()
	{
		int count = 0;
		for (int zone = 1; zone < m_aZones.Count(); zone++)
		{
			SRP_FrontZone z = m_aZones[zone];
			if (z && z.m_iOwner == SRP_EFrontOwner.BLEU)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! La zone d'index donné, null hors bornes (lecture de ses champs publics) — tout le mod
	SRP_FrontZone GetZone(int zone)
	{
		if (zone < 0 || zone >= m_aZones.Count())
			return null;
		return m_aZones[zone];
	}

	//------------------------------------------------------------------------------------------------
	//! Zone par code (« S07 »), nom (« Régina »), libellé (« Régina (S07) ») ou localité unique, sans tenir compte de
	//! la casse ni des espaces en double ; -1 sinon — missions (CreateCapture), pont (/ordonner), Staff
	int FindZone(string text)
	{
		string wanted = NormText(text);
		if (wanted.IsEmpty())
			return -1;
		int count = m_aZones.Count();

		// 1. Code, puis libellé complet
		for (int i = 0; i < count; i++)
		{
			if (m_aZones[i] && NormText(m_aZones[i].m_sCode) == wanted)
				return i;
		}
		for (int j = 0; j < count; j++)
		{
			if (m_aZones[j] && NormText(GetZoneLabel(j)) == wanted)
				return j;
		}

		// 2. Nom seul, s'il n'appartient qu'à une zone
		int found = -1;
		int matches = 0;
		for (int k = 0; k < count; k++)
		{
			if (m_aZones[k] && NormText(m_aZones[k].m_sName) == wanted)
			{
				found = k;
				matches++;
			}
		}
		if (matches == 1)
			return found;

		// 3. Localité (serveur), si elle désigne une seule zone
		int localityZone = -1;
		bool ambiguous = false;
		foreach (SRP_FrontLocality locality : m_aLocalities)
		{
			if (!locality || locality.m_bHome || locality.m_iZone < 0)
				continue;
			if (NormText(locality.m_sName) != wanted)
				continue;
			if (localityZone >= 0 && localityZone != locality.m_iZone)
				ambiguous = true;
			localityZone = locality.m_iZone;
		}
		if (!ambiguous && ValidZone(localityZone))
			return localityZone;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EFrontOwner de la zone (la base est BLEU) — tout le mod
	int GetZoneOwner(int zone)
	{
		if (!ValidZone(zone))
			return -1;
		if (zone == 0)
			return SRP_EFrontOwner.BLEU;
		return m_aZones[zone].m_iOwner;
	}

	//------------------------------------------------------------------------------------------------
	//! Code stable de la zone, « S07 » (clé technique, sans « : » ni « | ») — pont, missions, Staff
	string GetZoneCode(int zone)
	{
		if (!ValidZone(zone))
			return "";
		return m_aZones[zone].m_sCode;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom seul de la zone, « Régina » (localité ou lieu-dit ; « Secteur » à défaut) — carte, pont
	string GetZoneName(int zone)
	{
		if (!ValidZone(zone))
			return "";
		string name = m_aZones[zone].m_sName;
		if (name.IsEmpty())
			return "Secteur";
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé affiché partout, « Régina (S07) » (A6) — radio, PC, Staff, missions, Commandeur
	string GetZoneLabel(int zone)
	{
		if (!ValidZone(zone))
			return "";
		SRP_FrontZone z = m_aZones[zone];
		string name = GetZoneName(zone);
		if (zone == 0 || z.m_bBase || z.m_sCode.IsEmpty())
			return name;
		return name + " (" + z.m_sCode + ")";
	}

	//------------------------------------------------------------------------------------------------
	//! Centre de gravité des carrés de terre de la zone (y au sol) — Commandeur, ennemi
	vector GetZoneCentroid(int zone)
	{
		if (!ValidZone(zone))
			return vector.Zero;
		return m_aZones[zone].m_vCentroid;
	}

	//------------------------------------------------------------------------------------------------
	//! Où écrire le nom de la zone sur la carte (centre de ses carrés de terre, pas le point clé) ; vector.Zero pour une
	//! zone sans position (elle n'a alors pas de nom sur la carte) — calque
	vector GetZoneLabelPos(int zone)
	{
		if (!ValidZone(zone))
			return vector.Zero;
		vector pos = m_aZones[zone].m_vLabelPos;
		if (pos[0] == 0 && pos[2] == 0)
			return vector.Zero;
		return pos;
	}

	//------------------------------------------------------------------------------------------------
	//! Point de mission de la zone : point clé CENTRE de la localité principale, sinon le carré de terre le plus
	//! proche du centre de gravité — missions (CreateCapture, rayon 3 km), Staff (téléportation)
	vector GetMissionPoint(int zone)
	{
		if (!ValidZone(zone))
			return vector.Zero;
		SRP_FrontZone z = m_aZones[zone];

		// Centre de la localité principale (serveur)
		SRP_FrontLocality principal = GetLocality(z.m_iLocality);
		if (principal)
		{
			SRP_FrontKeyPoint centre = GetKeyPoint(principal.m_iCentre);
			if (centre)
				return centre.m_vPos;
		}

		// Premier point clé CENTRE de la zone (joueurs : pas de localités)
		foreach (int index : z.m_aKeyPoints)
		{
			SRP_FrontKeyPoint point = GetKeyPoint(index);
			if (point && point.m_iRole == SRP_EKeyPointRole.CENTRE)
				return point.m_vPos;
		}

		// Carré de terre le plus proche du centre de gravité
		int best = -1;
		float bestDistance = 1000000000;
		foreach (int cell : z.m_aCells)
		{
			vector center = CellCenter(cell);
			float distance = vector.DistanceXZ(center, z.m_vCentroid);
			if (distance < bestDistance)
			{
				bestDistance = distance;
				best = cell;
			}
		}
		if (best >= 0)
			return CellCenter(best);
		return z.m_vCentroid;
	}

	//------------------------------------------------------------------------------------------------
	//! Remplit cells avec les carrés de terre de la zone, rend leur nombre — chute, économie, ennemi
	int GetZoneCells(int zone, notnull array<int> cells)
	{
		cells.Clear();
		if (!ValidZone(zone))
			return 0;
		foreach (int cell : m_aZones[zone].m_aCells)
		{
			cells.Insert(cell);
		}
		return cells.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de carrés de terre de la zone — chute (D1, Q2), PC, Staff
	int CountZoneCells(int zone)
	{
		if (!ValidZone(zone))
			return 0;
		return m_aZones[zone].m_aCells.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de carrés bleus de la zone (tenu à jour à chaque lot) — chute, PC, Staff, pont
	int CountZoneBlue(int zone)
	{
		if (!ValidZone(zone))
			return 0;
		return m_aZones[zone].m_iBlueCells;
	}

	//------------------------------------------------------------------------------------------------
	//! E1 : zone ROUGE dont un carré touche du bleu (par un côté ou la mer) — missions (J4), ennemi (F11), carte (I3)
	bool IsZoneInContact(int zone)
	{
		if (!ValidZone(zone) || zone == 0)
			return false;
		UpdateTopology();
		return m_aZones[zone].m_bInContact;
	}

	//------------------------------------------------------------------------------------------------
	//! F1 : zone BLEUE dont un carré touche du rouge — ennemi (F1, cibles de contre-attaque), carte (I3)
	bool IsZoneAtFront(int zone)
	{
		if (!ValidZone(zone) || zone == 0)
			return false;
		UpdateTopology();
		return m_aZones[zone].m_bAtFront;
	}

	//------------------------------------------------------------------------------------------------
	//! B2 + B4 : la base, ou une zone qui touche la base (m_bShield) — chute (Q2), ennemi (F1, H2), pont (p)
	bool IsZoneProtected(int zone)
	{
		if (!ValidZone(zone))
			return false;
		if (zone == 0)
			return true;
		SRP_FrontZone z = m_aZones[zone];
		return z.m_bBase || z.m_bShield;
	}

	//------------------------------------------------------------------------------------------------
	//! La zone de la base (index 0, B2) — tout le mod
	bool IsBaseZone(int zone)
	{
		return zone == 0;
	}

	//------------------------------------------------------------------------------------------------
	//! E4 : un carré bleu de la zone est relié à la base par des carrés bleus (et la mer, Q5) — INTERNE au socle
	//! (UpdateTopology, CheckCuts) ; les autres modules lisent IsZoneCut, seul prédicat public de « zone coupée »
	protected bool IsZoneLinked(int zone)
	{
		if (!ValidZone(zone))
			return false;
		if (zone == 0)
			return true;
		UpdateTopology();
		return m_aZones[zone].m_bLinked;
	}

	//------------------------------------------------------------------------------------------------
	//! E4, SEUL prédicat public de « zone coupée » : zone bleue non protégée, coupée de la base depuis m_iCutSince —
	//! économie (plus de production), PC, pont (k)
	bool IsZoneCut(int zone)
	{
		if (!ValidZone(zone) || zone == 0 || IsZoneProtected(zone))
			return false;
		SRP_FrontZone z = m_aZones[zone];
		if (z.m_iOwner != SRP_EFrontOwner.BLEU)
			return false;
		// Serveur : m_iCutSince ; joueurs : le drapeau FLAG_CUT répliqué
		return z.m_iCutSince > 0 || (z.m_iFlags & FLAG_CUT) != 0;
	}

	//------------------------------------------------------------------------------------------------
	//! E4 : secondes avant la perte d'une zone coupée, -1 si elle est reliée (gel : décompte arrêté) — PC, Staff, pont (kr)
	int GetZoneCutSecondsLeft(int zone)
	{
		if (!IsZoneCut(zone))
			return -1;
		SRP_FrontZone z = m_aZones[zone];
		if (z.m_iCutSince <= 0)
			return -1;
		int clockUnix = System.GetUnixTime();
		if (m_bFrozen && m_iFrozenSince > 0)
			clockUnix = m_iFrozenSince;
		int left = z.m_iCutSince + Math.MaxInt(m_iCutLossMinutes, 1) * 60 - clockUnix;
		return Math.MaxInt(left, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Drapeaux d'affichage de la zone (FLAG_ATTACK_ANNOUNCED, FLAG_ATTACK, FLAG_CUT) — carte, PC, pont
	int GetZoneFlags(int zone)
	{
		if (!ValidZone(zone))
			return 0;
		return m_aZones[zone].m_iFlags;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EFrontAttack de la zone, déduit des drapeaux (posés par SRP_FrontEnemyComponent via SetZoneFlag) : SEULE
	//! source de l'état d'attaque d'une zone — carte, repères, PC, Staff, pont (a), ennemi, Commandeur
	int GetZoneAttackState(int zone)
	{
		int flags = GetZoneFlags(zone);
		if ((flags & FLAG_ATTACK) != 0)
			return SRP_EFrontAttack.ASSAUT;
		if ((flags & FLAG_ATTACK_ANNOUNCED) != 0)
			return SRP_EFrontAttack.ANNONCEE;
		return SRP_EFrontAttack.AUCUNE;
	}

	//------------------------------------------------------------------------------------------------
	//! Heure Unix de la dernière prise de la zone (F2 : dernière zone prise) — ennemi (PickTargetZone)
	int GetZoneCapturedAt(int zone)
	{
		if (!ValidZone(zone))
			return 0;
		return m_aZones[zone].m_iCapturedAt;
	}

	//------------------------------------------------------------------------------------------------
	//! Dernier changement de camp, en clair (« 25/09 21:14 ») — Staff (page de zone)
	string GetZoneLastChange(int zone)
	{
		if (!ValidZone(zone))
			return "";
		return m_aZones[zone].m_sLastChange;
	}

	//------------------------------------------------------------------------------------------------
	//! Zones rouges au contact (E1), sans la base — missions (J4 : GetCaptureTargets), Commandeur ; rend le nombre
	int GetZonesInContact(notnull array<int> zones)
	{
		zones.Clear();
		UpdateTopology();
		for (int zone = 1; zone < m_aZones.Count(); zone++)
		{
			SRP_FrontZone z = m_aZones[zone];
			if (z && z.m_iOwner == SRP_EFrontOwner.ROUGE && z.m_bInContact)
				zones.Insert(zone);
		}
		return zones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Zones bleues au front (F1), non protégées (B4) — ennemi (PickTargetZone, H2) ; rend le nombre
	int GetBlueFrontZones(notnull array<int> zones)
	{
		zones.Clear();
		UpdateTopology();
		for (int zone = 1; zone < m_aZones.Count(); zone++)
		{
			SRP_FrontZone z = m_aZones[zone];
			if (z && z.m_iOwner == SRP_EFrontOwner.BLEU && z.m_bAtFront && !IsZoneProtected(zone))
				zones.Insert(zone);
		}
		return zones.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Index des localités de la zone (principale d'abord), rend le nombre — chute, ennemi, Staff
	int GetZoneLocalities(int zone, notnull array<int> localities)
	{
		localities.Clear();
		if (!ValidZone(zone))
			return 0;
		foreach (int index : m_aZones[zone].m_aLocalities)
		{
			localities.Insert(index);
		}
		return localities.Count();
	}

	//================================================================================================
	// Points clés : registre UNIQUE (trou 13, arbitrage Q8). Un point est « pris » quand son carré est bleu.
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Nombre de points clés de la zone (0 sans localité, 1 par hameau ou village, 2 par ville : D6) — chute, ennemi, carte
	int GetKeyPointCount(int zone)
	{
		if (!ValidZone(zone))
			return 0;
		return m_aZones[zone].m_aKeyPoints.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Position du k-ième point clé de la zone — chute (poste), ennemi (garnison, F5), Commandeur (QG = m_vHQ)
	vector GetKeyPointPos(int zone, int k)
	{
		SRP_FrontKeyPoint point = ZoneKeyPoint(zone, k);
		if (!point)
			return vector.Zero;
		return point.m_vPos;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EKeyPointRole du k-ième point clé de la zone — chute, ennemi, repères
	int GetKeyPointRole(int zone, int k)
	{
		SRP_FrontKeyPoint point = ZoneKeyPoint(zone, k);
		if (!point)
			return SRP_EKeyPointRole.CENTRE;
		return point.m_iRole;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré du k-ième point clé de la zone — capture (F5), chute (D2), ennemi
	int GetKeyPointCell(int zone, int k)
	{
		SRP_FrontKeyPoint point = ZoneKeyPoint(zone, k);
		if (!point)
			return -1;
		return point.m_iCell;
	}

	//------------------------------------------------------------------------------------------------
	//! Index du k-ième point clé de la zone dans le registre global (GetKeyPoint) — chute (postes)
	int GetKeyPointIndex(int zone, int k)
	{
		if (!ValidZone(zone))
			return -1;
		SRP_FrontZone z = m_aZones[zone];
		if (k < 0 || k >= z.m_aKeyPoints.Count())
			return -1;
		return z.m_aKeyPoints[k];
	}

	//------------------------------------------------------------------------------------------------
	//! Le point clé est pris : son carré est bleu — chute (D1, Q2), repères, PC
	bool IsKeyPointTaken(int zone, int k)
	{
		SRP_FrontKeyPoint point = ZoneKeyPoint(zone, k);
		if (!point)
			return false;
		return GetCellOwner(point.m_iCell) == SRP_EFrontOwner.BLEU;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre total de points clés du registre — chute (postes), Staff
	int GetKeyPointTotal()
	{
		return m_aKeyPoints.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Le point clé d'index global donné, null hors bornes — chute, Staff, repères
	SRP_FrontKeyPoint GetKeyPoint(int index)
	{
		if (index < 0 || index >= m_aKeyPoints.Count())
			return null;
		return m_aKeyPoints[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Page Staff « Points clés » (Q8) : une ligne par localité, source de chaque point (REPERE = nom du repère ;
	//! NOM_CARTE = centre MANQUANT ; QG absent d'une ville = MANQUANT) ; rend le nombre de points manquants —
	//! SRP_FrontScreens
	int GetKeyPointReport(notnull array<string> lines)
	{
		lines.Clear();
		int missing = 0;
		foreach (SRP_FrontLocality locality : m_aLocalities)
		{
			if (!locality || locality.m_bHome)
				continue;
			string where = "hors zone";
			if (ValidZone(locality.m_iZone))
				where = "zone " + GetZoneCode(locality.m_iZone);
			string kind = locality.m_sKind;
			if (kind.IsEmpty())
				kind = "localité";

			SRP_FrontKeyPoint centre = GetKeyPoint(locality.m_iCentre);
			if (!centre || centre.m_iSource == SRP_EKeyPointSource.NOM_CARTE)
				missing++;
			string line = string.Format("%1 (%2, %3) — centre : %4", locality.m_sName, kind, where, KeyPointSourceText(centre));

			if (locality.m_iKind == SRP_EZoneKind.VILLE)
			{
				SRP_FrontKeyPoint hq = GetKeyPoint(locality.m_iQg);
				if (!hq)
				{
					missing++;
					line += " · QG : MANQUANT (ville à un seul point clé, repère QG à poser au Workbench)";
				}
				else
				{
					line += " · QG : " + KeyPointSourceText(hq);
				}
			}
			lines.Insert(line);
		}
		return missing;
	}

	//================================================================================================
	// Localités (serveur) : lues dans SRP_CivilZoneComponent (trou 24), champs F12 et Commandeur sauvegardés ici
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Nombre de localités de zone (sans celles de la base) — ennemi, Commandeur (régions), SRP_Territory (BuildLocalities)
	int GetLocalityCount()
	{
		return m_aLocalities.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! La localité d'index donné, null hors bornes (champs publics : pertes F12, envoyés, bonus…) — ennemi, Commandeur
	SRP_FrontLocality GetLocality(int index)
	{
		if (index < 0 || index >= m_aLocalities.Count())
			return null;
		return m_aLocalities[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Index de la localité par son nom exact (nom traduit de la carte), -1 sinon — ennemi, territoire, chute
	int FindLocality(string name)
	{
		if (name.IsEmpty())
			return -1;
		for (int i = 0; i < m_aLocalities.Count(); i++)
		{
			if (m_aLocalities[i] && m_aLocalities[i].m_sName == name)
				return i;
		}
		return -1;
	}

	//================================================================================================
	// État global (toutes machines sauf mention)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! J3 : front gelé, ni capture, ni perte, ni contre-attaque — tout le mod
	bool IsFrozen()
	{
		return m_bFrozen;
	}

	//------------------------------------------------------------------------------------------------
	//! B3 : toutes les zones jouables sont bleues — Staff, PC
	bool IsVictory()
	{
		return m_bVictory;
	}

	//------------------------------------------------------------------------------------------------
	//! Menace de l'île, 0 à 10 (G9) ; SRP_TerritoryComponent.GetThreat ne fait plus que relayer — serveur, tout le mod
	int GetThreat()
	{
		return m_iThreat;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : menace + delta, bornée de 0 à 10, sauvegarde immédiate, puis SRP_FrontEnemyComponent.OnThreatChanged
	//! (ressources du Commandeur) — appelé par SRP_ZoneFall (G9), l'ennemi (F4, C8), SRP_TerritoryComponent (relais)
	void AddThreat(int delta, string reason)
	{
		if (!Replication.IsServer())
			return;
		int before = m_iThreat;
		m_iThreat = Math.ClampInt(before + delta, 0, 10);
		if (m_iThreat == before)
			return;
		Note("FRONT", string.Format("Menace %1 -> %2 (%3)", before, m_iThreat, reason));
		m_bStatePending = true;
		QueueFlush();
		MarkDirty(true);
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.OnThreatChanged(m_iThreat);
	}

	//------------------------------------------------------------------------------------------------
	//! Version d'état : +1 à chaque lot appliqué, y compris sur l'hôte (le Broadcast ne s'y exécute pas) — calque, Commandeur
	int GetStateVersion()
	{
		return m_iStateVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Version de géométrie, +1 à chaque géométrie appliquée — calque
	int GetGeometryVersion()
	{
		return m_iGeometryVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Version des états contestés — calque (orange)
	int GetLiveVersion()
	{
		return m_iLiveVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : version pour le pont, +1 par carré qui change de camp, sauvegardée, ne redescend jamais (trou 19) —
	//! SRP_Bridge (v, NoteFrontCell)
	int GetBridgeVersion()
	{
		return m_iBridgeVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : nombre de retours à une copie datée (J2), sauvegardé — SRP_Bridge (rs)
	int GetRestoreCount()
	{
		return m_iRestoreCount;
	}

	//------------------------------------------------------------------------------------------------
	//! Numéro de campagne (première campagne du front = 2, trou 18) — Staff, Commandeur
	int GetCampaign()
	{
		return m_iCampaign;
	}

	//------------------------------------------------------------------------------------------------
	//! Identifiant de campagne pour le pont, « C2 » (^[A-Za-z0-9_-]+$, stable jusqu'à la suivante) — SRP_Bridge
	string GetCampaignId()
	{
		// front.json illisible : rien n'est sauvegardé, la campagne en mémoire est une campagne neuve par défaut ; le pont
		// ne doit pas l'annoncer (le bot archiverait la vraie campagne). Le bot ignore un bloc front sans identifiant.
		if (Replication.IsServer() && !m_bLoaded)
			return "";
		return "C" + m_iCampaign.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés de terre en jeu — PC, Staff, pont (land)
	int CountLandCells()
	{
		return m_iLandCells;
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés bleus (base comprise) — PC, Staff, pont (blue)
	int CountBlueCells()
	{
		int count = 0;
		foreach (SRP_FrontZone z : m_aZones)
		{
			if (z)
				count += z.m_iBlueCells;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Carrés contestés (C8), rend le nombre — SRP_Bridge (cs)
	int GetContestedCells(notnull array<int> cells)
	{
		cells.Clear();
		for (int cell = 0; cell < m_aCellLive.Count(); cell++)
		{
			if (m_aCellLive[cell] != SRP_EFrontLive.AUCUN)
				cells.Insert(cell);
		}
		return cells.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! N x N caractères, « . » hors jeu, « R », « B », index gz x N + gx — SRP_Bridge (g), sauvegarde (cells)
	string GetCellsString()
	{
		if (m_iCellsCacheStamp == m_iOwnerStamp && m_sCellsCache.Length() == m_iN * m_iN)
			return m_sCellsCache;

		// Une rangée à la fois : peu de copies de chaînes (4 096 caractères)
		array<string> rows = {};
		int count = Math.MinInt(m_iN * m_iN, m_aCellOwner.Count());
		for (int gz = 0; gz < m_iN; gz++)
		{
			string row = "";
			for (int gx = 0; gx < m_iN; gx++)
			{
				int cell = gz * m_iN + gx;
				int owner = HORS_JEU;
				if (cell < count)
					owner = m_aCellOwner[cell];
				if (owner == SRP_EFrontOwner.BLEU)
					row += "B";
				else if (owner == SRP_EFrontOwner.ROUGE)
					row += "R";
				else
					row += ".";
			}
			rows.Insert(row);
		}
		m_sCellsCache = string.Join("", rows, false);
		m_iCellsCacheStamp = m_iOwnerStamp;
		return m_sCellsCache;
	}

	//------------------------------------------------------------------------------------------------
	//! Deux caractères hexadécimaux de zone par carré, « FF » hors jeu — SRP_Bridge (zm, en cache côté pont)
	string GetZoneMapString()
	{
		if (m_iZoneMapCacheVersion == m_iGeometryVersion && m_sZoneMapCache.Length() == 2 * m_iN * m_iN)
			return m_sZoneMapCache;

		array<string> rows = {};
		int count = Math.MinInt(m_iN * m_iN, m_aCellZone.Count());
		for (int gz = 0; gz < m_iN; gz++)
		{
			string row = "";
			for (int gx = 0; gx < m_iN; gx++)
			{
				int cell = gz * m_iN + gx;
				int zone = -1;
				if (cell < count)
					zone = m_aCellZone[cell];
				if (zone < 0 || zone > 254)
				{
					row += "FF";
				}
				else
				{
					int high = zone / 16;
					int low = zone - high * 16;
					row += HEX.Get(high) + HEX.Get(low);
				}
			}
			rows.Insert(row);
		}
		m_sZoneMapCache = string.Join("", rows, false);
		m_iZoneMapCacheVersion = m_iGeometryVersion;
		return m_sZoneMapCache;
	}

	//------------------------------------------------------------------------------------------------
	//! Rapport de géométrie (zones, carrés, localités, points clés manquants, retouches refusées) — Staff
	string GetGeometryReport()
	{
		if (!m_bReady)
			return "Front pas encore construit";
		string text = string.Format("Grille %1 x %2, carrés de %3 m · %4 carrés en jeu · %5 zones et la base (%6 carrés) · %7 localités · %8 points clés, %9 manquant(s)", m_iN, m_iN, Math.Round(GetCellSize()), CountLandCells(), CountZonesTotal(), CountZoneCells(0), CountFrontLocalities(), m_aKeyPoints.Count(), m_iMissingKeyPoints);
		text += string.Format("\nPoints clés posés au Workbench : %1 ; somme de contrôle : %2", CountPlacedKeyPoints(), m_iGeomHash);
		if (m_aGeomWarnings)
		{
			foreach (string warning : m_aGeomWarnings)
			{
				text += "\n" + warning;
			}
		}
		if (!m_aOrphanZones.IsEmpty())
			text += "\nZones de front.json sans correspondance (gardées telles quelles) : " + string.Join(", ", m_aOrphanZones, true);
		text += "\nRapport complet : " + REPORT_PATH;
		return text;
	}

	//================================================================================================
	// Extras : petites valeurs de campagne des autres modules, sauvées dans front.json (effacées à la nouvelle
	// campagne, reprises par RestoreDaily). Le Commandeur n'en utilise AUCUN (il passe par WriteTo/ReadFrom).
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Serveur : range une valeur (clé sans « | »), puis MarkDirty(false) — missions, économie
	void SetExtra(string key, string value)
	{
		if (!Replication.IsServer() || key.IsEmpty())
			return;
		string k = key;
		k.Replace("|", "_");
		string old;
		if (m_mExtra.Find(k, old) && old == value)
			return;
		m_mExtra.Set(k, value);
		MarkDirty(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : la valeur rangée, "" si absente — missions, économie
	string GetExtra(string key)
	{
		string k = key;
		k.Replace("|", "_");
		string value;
		if (m_mExtra.Find(k, value))
			return value;
		return "";
	}

	//================================================================================================
	// Écriture (SERVEUR). SetCellOwner / SetCellsOwner = SEULE porte bas niveau des carrés (trou 5) ;
	// SetZoneOwner est réservée à SRP_ZoneFall, SEULE porte des changements de zone.
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Un carré change de camp : refus (false + journal) hors jeu, base, déjà en place, gel (sauf STAFF, REMISE,
	//! RESTAURATION), passage au rouge d'une zone protégée bleue (sauf STAFF) ; sinon comme SetCellsOwner avec un seul
	//! carré — capture (via SetCellsOwner), chute (SAISIE)
	bool SetCellOwner(int cell, int owner, int reason, string author)
	{
		array<int> one = {};
		one.Insert(cell);
		return SetCellsOwner(one, owner, reason, author) == 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Un lot de carrés passe à owner, mêmes refus carré par carré ; rend le nombre appliqué. Puis, dans l'ordre et
	//! en appels directs (trou 6) : m_Capture.OnOwnersChanged -> m_ZoneFall.OnCellsChanged -> m_Economy.OnCellsChanged
	//! -> SRP_BridgeComponent.NoteFrontCell (par carré) -> SRP_FrontRadio.QueueCell (par carré) ; m_iBridgeVersion++
	//! par carré, compteurs de zone, topologie à refaire, QueueFlush, MarkDirty(false) — capture, chute, Staff
	int SetCellsOwner(notnull array<int> cells, int owner, int reason, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return 0;
		if (owner != SRP_EFrontOwner.ROUGE && owner != SRP_EFrontOwner.BLEU)
			return 0;

		// Application carré par carré, dans un tableau à nous : les modules appelés ensuite ne touchent pas « cells »
		array<int> applied = {};
		int refused = 0;
		int firstRefusedCell = -1;
		string firstRefusal;
		int firstVersion = m_iBridgeVersion;
		foreach (int cell : cells)
		{
			string why = CellRefusal(cell, owner, reason);
			if (!why.IsEmpty())
			{
				refused++;
				if (firstRefusedCell < 0)
				{
					firstRefusedCell = cell;
					firstRefusal = why;
				}
				continue;
			}
			SRP_FrontZone z = m_aZones[m_aCellZone[cell]];
			m_aCellOwner[cell] = owner;
			if (owner == SRP_EFrontOwner.BLEU)
				z.m_iBlueCells = z.m_iBlueCells + 1;
			else if (z.m_iBlueCells > 0)
				z.m_iBlueCells = z.m_iBlueCells - 1;
			m_iBridgeVersion++;
			m_aPendingCells.Insert((cell << 1) | owner);
			applied.Insert(cell);
		}
		if (refused > 0)
			Note("FRONT", string.Format("%1 carré(s) refusé(s), passage %2, cause %3%4 : %5 (carré %6)", refused, OwnerWord(owner), ReasonName(reason), AuthorSuffix(author), firstRefusal, CellRef(firstRefusedCell)));

		int count = applied.Count();
		if (count == 0)
			return 0;
		m_iOwnerStamp++;
		m_bTopologyDirty = true;

		// Appels directs, dans cet ordre (trou 6)
		if (m_Capture)
			m_Capture.OnOwnersChanged(applied);
		if (m_ZoneFall)
			m_ZoneFall.OnCellsChanged(applied, owner, reason);
		if (m_Economy)
			m_Economy.OnCellsChanged(applied, owner, reason);
		for (int i = 0; i < count; i++)
		{
			SRP_BridgeComponent.NoteFrontCell(applied[i], owner, firstVersion + i + 1);
		}
		for (int j = 0; j < count; j++)
		{
			SRP_FrontRadio.QueueCell(applied[j], owner, reason);
		}
		QueueFlush();
		MarkDirty(false);
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! RÉSERVÉ À SRP_ZoneFall : note le propriétaire de la zone, m_iCapturedAt/m_iLostAt, numéro de prise (F2),
	//! m_iCutSince = 0, drapeaux d'attaque effacés, m_bStatePending, sauvegarde immédiate, CheckVictory. Ne repeint
	//! AUCUN carré (la chute appelle SetCellsOwner elle-même) — SRP_ZoneFall.CaptureZone/LoseZone/ForceZone
	bool SetZoneOwner(int zone, int owner, int reason)
	{
		if (!Replication.IsServer() || !m_bReady)
			return false;
		if (!ValidZone(zone) || zone == 0)
			return false;
		if (owner != SRP_EFrontOwner.ROUGE && owner != SRP_EFrontOwner.BLEU)
			return false;
		SRP_FrontZone z = m_aZones[zone];
		if (z.m_iOwner == owner)
			return false;
		if (m_bFrozen && !IsCorrectionReason(reason))
		{
			Note("FRONT", string.Format("Zone %1 : changement refusé (front gelé, cause %2)", GetZoneLabel(zone), ReasonName(reason)));
			return false;
		}
		if (owner == SRP_EFrontOwner.ROUGE && reason != SRP_EFrontReason.STAFF && IsZoneProtected(zone))
		{
			Note("FRONT", string.Format("Zone %1 : perte refusée, zone voisine de la base (B4, cause %2)", GetZoneLabel(zone), ReasonName(reason)));
			return false;
		}

		int now = System.GetUnixTime();
		z.m_iOwner = owner;
		if (owner == SRP_EFrontOwner.BLEU)
		{
			z.m_iCapturedAt = now;
			z.m_iTakenCount = z.m_iTakenCount + 1;
		}
		else
		{
			z.m_iLostAt = now;
			ClearVictory();
		}
		z.m_iCutSince = 0;
		z.m_iFlags = 0;
		z.m_sLastChange = NowShort();
		if (zone < m_aCutReminded.Count())
			m_aCutReminded[zone] = 0;

		m_bStatePending = true;
		m_bTopologyDirty = true;
		QueueFlush();
		MarkDirty(true);
		CheckVictory();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Drapeau d'affichage d'une zone (FLAG_ATTACK_ANNOUNCED, FLAG_ATTACK) posé ou retiré, m_bStatePending (trou 11) —
	//! SRP_FrontEnemyComponent (contre-attaques)
	void SetZoneFlag(int zone, int flag, bool on)
	{
		if (!Replication.IsServer() || !ValidZone(zone) || flag <= 0)
			return;
		SRP_FrontZone z = m_aZones[zone];
		int before = z.m_iFlags;
		if (on)
			z.m_iFlags = z.m_iFlags | flag;
		else
			z.m_iFlags = z.m_iFlags - (z.m_iFlags & flag);
		if (z.m_iFlags == before)
			return;
		m_bStatePending = true;
		QueueFlush();
	}

	//------------------------------------------------------------------------------------------------
	//! C8 : différences d'état contesté du passage (carrés, SRP_EFrontLive) ; envoi au plus toutes les m_iLiveSendMinMs,
	//! le dernier état part toujours — SRP_FrontCapture.PushLive
	void SetLiveCells(notnull array<int> cells, notnull array<int> states)
	{
		if (!Replication.IsServer() || !m_bReady)
			return;
		int count = Math.MinInt(cells.Count(), states.Count());
		bool changed = false;
		for (int i = 0; i < count; i++)
		{
			int cell = cells[i];
			int state = states[i];
			if (cell < 0 || cell >= m_aCellLive.Count() || state < SRP_EFrontLive.AUCUN || state > SRP_EFrontLive.REPRISE)
				continue;
			if (m_aCellLive[cell] == state)
				continue;
			m_aCellLive[cell] = state;
			if (!m_aLiveCells.Contains(cell))
				m_aLiveCells.Insert(cell);
			changed = true;
		}
		if (!changed)
			return;
		m_iLiveVersion++;	// l'hôte (son calque) : le Broadcast ne s'y exécute pas
		TrySendLive();
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les états contestés à AUCUN et envoi (gel, restauration, remise) — socle, capture
	void ClearLive()
	{
		if (!Replication.IsServer())
			return;
		bool changed = false;
		for (int cell = 0; cell < m_aCellLive.Count(); cell++)
		{
			if (m_aCellLive[cell] == SRP_EFrontLive.AUCUN)
				continue;
			m_aCellLive[cell] = SRP_EFrontLive.AUCUN;
			if (!m_aLiveCells.Contains(cell))
				m_aLiveCells.Insert(cell);
			changed = true;
		}
		if (!changed)
			return;
		m_iLiveVersion++;
		if (m_bReady)
			SendLive();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : front.json à écrire ; immediate = au prochain passage, sinon au plus m_iSaveDelaySeconds plus tard —
	//! ennemi (F12), Commandeur, économie, missions
	void MarkDirty(bool immediate)
	{
		if (!Replication.IsServer())
			return;
		if (!m_bDirty)
		{
			m_bDirty = true;
			m_iDirtySince = System.GetUnixTime();
		}
		if (!immediate)
			return;
		// « Aussitôt » : à la fin de l'image, une fois toute la chaîne d'appels en cours terminée
		m_bSaveNow = true;
		if (!m_bSaveQueued)
		{
			m_bSaveQueued = true;
			GetGame().GetCallqueue().CallLater(SaveQueued, 0, false);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : écrit front.json (refusé si !m_bLoaded), puis la copie datée du jour si le jour a changé ; rend vrai
	//! si le fichier est écrit. Garde de la chaîne unique : si m_bEnemyChained et SRP_FrontEnemyComponent.GetInstance()
	//! est null, RIEN n'est écrit (un front.json sans fe_* ni cmd_* ferait tout repartir à neuf : stocks pleins,
	//! officiers neufs, interdictions effacées) ; journal ERREUR sauf si m_bEnemyFinalSaved (sauvegarde finale déjà
	//! faite par l'ennemi) — Housekeeping, OnDelete, Staff, SaveFinalFromEnemy
	bool Save()
	{
		if (!Replication.IsServer() || !m_bLoaded || m_iN <= 0)
			return false;
		if (m_bEnemyChained && !SRP_FrontEnemyComponent.GetInstance())
		{
			if (!m_bEnemyFinalSaved && !m_bChainErrorLogged)
			{
				m_bChainErrorLogged = true;
				Note("ERREUR", "Front : sauvegarde refusée, l'ennemi du front est déjà arrêté ; front.json est gardé tel quel pour ne pas perdre l'état de l'ennemi et du Commandeur");
			}
			return false;
		}

		JsonSaveContext ctx = new JsonSaveContext();
		WriteTo(ctx);

		// La dernière bonne version est gardée à côté : un arrêt brutal pendant l'écriture ne laisse jamais un seul
		// front.json tronqué (Load relit alors PREV_PATH). Jamais de copie d'un front.json qui n'a pas été relu ou écrit ici.
		if (m_bPathGood && FileIO.FileExists(PATH))
			FileIO.CopyFile(PATH, PREV_PATH);
		if (!ctx.SaveToFile(PATH))
		{
			m_bPathGood = false;
			// Une seule ligne (disque plein, droits) : SaveIfDue réessaie à chaque passage de 5 s
			if (!m_bSaveErrorLogged)
			{
				m_bSaveErrorLogged = true;
				Note("ERREUR", "Front : échec d'écriture de " + PATH + " (nouvel essai à chaque passage, sans autre message)");
			}
			return false;
		}
		if (m_bSaveErrorLogged)
		{
			m_bSaveErrorLogged = false;
			Note("FRONT", "Front : écriture de " + PATH + " rétablie");
		}
		m_bPathGood = true;
		m_bDirty = false;
		m_iDirtySince = 0;
		m_bSaveNow = false;

		string today = Today();
		if (!today.IsEmpty() && today != m_sLastDaily)
			DailyCopy();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, OnDelete de SRP_FrontEnemyComponent (AVANT son m_Commander.Stop et son s_Instance = null) : Save() tant
	//! que la chaîne complète existe, puis m_bEnemyFinalSaved = vrai ; rend vrai si le fichier est écrit —
	//! SRP_FrontEnemyComponent.OnDelete
	bool SaveFinalFromEnemy()
	{
		bool written = Save();
		m_bEnemyFinalSaved = true;
		return written;
	}

	//================================================================================================
	// Primitives du Staff (serveur). Chacune rend le texte de compte rendu du menu (vide = rien fait).
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! J1 : le carré où se tient le Staff passe à owner (cause STAFF : ni prime, ni menace, ni radio ; correction Q9 :
	//! la zone n'est PAS réévaluée, SRP_ZoneFall.OnCellsChanged ignore STAFF) — SRP_FrontScreens
	string ForceCellAt(vector position, int owner, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return "Front pas encore prêt";
		int cell = CellIndexAt(position);
		if (cell < 0)
			return "Ici : hors du front (mer ou hors de la carte)";
		return ForceCell(cell, owner, author);
	}

	//------------------------------------------------------------------------------------------------
	//! J1 : un carré passe à owner (cause STAFF), refusé dans la base ; ni chute ni perte de zone ne suivent (Q9 :
	//! correction ; pour une vraie chute, ForceSeize ; pour corriger toute la zone, ForceZone) — SRP_FrontScreens,
	//! ForceCellAt
	string ForceCell(int cell, int owner, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return "Front pas encore prêt";
		if (owner != SRP_EFrontOwner.ROUGE && owner != SRP_EFrontOwner.BLEU)
			return "Couleur inconnue";
		if (!IsCellInPlay(cell))
			return "Carré hors du front (mer ou hors de la carte)";
		string where = CellRef(cell) + " (" + GetZoneLabel(m_aCellZone[cell]) + ")";
		if (IsBaseCell(cell))
			return "Carré " + where + " : c'est la base, il reste toujours à nous";
		if (m_aCellOwner[cell] == owner)
			return "Carré " + where + " : déjà " + OwnerWord(owner);
		array<int> one = {};
		one.Insert(cell);
		if (SetCellsOwner(one, owner, SRP_EFrontReason.STAFF, author) != 1)
			return "Carré " + where + " : refusé (voir le journal FRONT)";
		MarkDirty(true);	// correction du Staff : écrite aussitôt (§1.10), pas 30 s plus tard
		return "Carré " + where + " passé " + OwnerWord(owner) + " (correction : ni prime, ni menace, ni radio ; la zone n'est pas réévaluée)";
	}

	//------------------------------------------------------------------------------------------------
	//! J1 : relais de SRP_ZoneFall.ForceZone (zone entière repeinte, Q9 : ni prime, ni menace, ni radio) — SRP_FrontScreens
	string ForceZone(int zone, int owner, string author)
	{
		if (!Replication.IsServer() || !m_bReady || !m_ZoneFall)
			return "Front pas encore prêt";
		if (!ValidZone(zone))
			return "Zone inconnue";
		if (zone == 0)
			return "La base reste toujours à nous";
		if (owner != SRP_EFrontOwner.ROUGE && owner != SRP_EFrontOwner.BLEU)
			return "Couleur inconnue";
		return m_ZoneFall.ForceZone(zone, owner, author);
	}

	//------------------------------------------------------------------------------------------------
	//! J2 : la zone revient à son état de départ (rouge, stock et dépôt effacés), relais REMISE à la capture, à
	//! l'économie et à l'ennemi (OnFrontReset ZONE) — SRP_FrontScreens
	string ResetZone(int zone, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return "Front pas encore prêt";
		if (!ValidZone(zone))
			return "Zone inconnue";
		if (zone == 0)
			return "La base ne se remet pas à zéro : elle reste toujours à nous";

		// État de départ de la zone (le stock et le dépôt sont effacés par l'économie, dans NotifyReset)
		SRP_FrontZone z = m_aZones[zone];
		z.m_iOwner = SRP_EFrontOwner.ROUGE;
		z.m_iFlags = 0;
		z.m_iCapturedAt = 0;
		z.m_iLostAt = 0;
		z.m_iTakenCount = 0;
		z.m_iCutSince = 0;
		z.m_sLastChange = NowShort();
		if (zone < m_aCutReminded.Count())
			m_aCutReminded[zone] = 0;
		ClearVictory();

		// Ses carrés bleus repassent rouges (cause REMISE : ni chute, ni radio de combat)
		array<int> blue = {};
		foreach (int cell : z.m_aCells)
		{
			if (GetCellOwner(cell) == SRP_EFrontOwner.BLEU)
				blue.Insert(cell);
		}
		int repainted = 0;
		if (!blue.IsEmpty())
			repainted = SetCellsOwner(blue, SRP_EFrontOwner.ROUGE, SRP_EFrontReason.REMISE, author);

		m_bStatePending = true;
		m_bTopologyDirty = true;
		NotifyReset(SRP_EFrontReset.ZONE, zone);
		SRP_BridgeComponent.NoteFrontReset();
		// Correction du Staff (Q9) : le bot met la zone à jour sans mouvement « prise / perdue » ni photo
		if (!z.m_sCode.IsEmpty())
			SRP_BridgeComponent.NoteStaffZone(z.m_sCode);
		QueueFlush();
		Save();
		return string.Format("Zone %1 remise à son état de départ : rouge, %2 carré(s) repeint(s), stock et dépôt effacés", GetZoneLabel(zone), repainted);
	}

	//------------------------------------------------------------------------------------------------
	//! B3, J2 : nouvelle campagne à la demande du Staff (archive, tout rouge sauf la base, menace 0) — SRP_FrontScreens,
	//! SRP_Admin (danger, wipe)
	string NewCampaign(string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return "Front pas encore prêt";
		int finished = m_iCampaign;
		ResetCampaign("Staff", author);
		string text = string.Format("Nouvelle campagne n° %1 (la campagne n° %2 est archivée) : tout est rouge sauf la base, menace 0, front dégelé", m_iCampaign, finished);
		if (!m_bLoaded)
			text += " (front.json était illisible au démarrage : rien n'est écrit tant qu'une copie datée n'est pas rechargée)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! J3 : gel ou dégel ; au dégel chaque m_iCutSince est décalé de la durée du gel ; ClearLive ; capture prévenue ;
	//! radio SRP_FrontRadio.Frozen — SRP_FrontScreens
	string SetFrozen(bool frozen, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return "Front pas encore prêt";
		if (frozen == m_bFrozen)
		{
			if (frozen)
				return "Le front est déjà gelé";
			return "Le front n'est pas gelé";
		}

		int now = System.GetUnixTime();
		if (frozen)
		{
			m_bFrozen = true;
			m_iFrozenSince = now;
		}
		else
		{
			// E4 (Q1) : la minuterie de coupure est arrêtée pendant le gel ; une coupure née pendant le gel repart de zéro
			int since = m_iFrozenSince;
			if (since <= 0)
				since = now;
			foreach (SRP_FrontZone z : m_aZones)
			{
				if (z && z.m_iCutSince > 0)
					z.m_iCutSince = z.m_iCutSince + now - Math.MaxInt(since, z.m_iCutSince);
			}
			m_bFrozen = false;
			m_iFrozenSince = 0;
		}

		ClearLive();
		if (m_Capture)
			m_Capture.OnFrozenChanged(frozen);
		SRP_FrontRadio.Frozen(frozen, author);
		m_bStatePending = true;
		QueueFlush();
		Save();
		if (frozen)
			return "Front gelé : ni capture, ni perte, ni contre-attaque ; les minuteries de coupure sont arrêtées";
		return "Front dégelé : captures et contre-attaques reprennent ; les minuteries de coupure repartent là où elles étaient";
	}

	//------------------------------------------------------------------------------------------------
	//! J2 : copies datées, de la plus récente à la plus ancienne (au plus m_iSavesShown), avec un résumé « 14 zones,
	//! 231 carrés à nous, menace 4 » ; rend le nombre — SRP_FrontScreens
	int GetDailyCopies(notnull array<string> days, notnull array<string> summaries)
	{
		days.Clear();
		summaries.Clear();
		int shown = Math.MaxInt(m_iSavesShown, 1);
		for (int i = m_aDailyCopies.Count() - 1; i >= 0; i--)
		{
			if (days.Count() >= shown)
				break;
			string day = m_aDailyCopies[i];
			if (!FileIO.FileExists(DailyPath(day)))
				continue;
			string summary = "";
			if (i < m_aDailySummaries.Count())
				summary = m_aDailySummaries[i];
			days.Insert(day);
			summaries.Insert(summary);
		}
		return days.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! J2 : revient à la copie du jour donné (AAAA-MM-JJ) : lecture dans des tableaux neufs, application d'un coup,
	//! m_iRestoreCount++, ClearLive, OnFrontReset(RESTAURATION) aux modules, FrontEnemy.ReadFrom puis OnRestored,
	//! SRP_BridgeComponent.NoteFrontReset, radio Restored, sauvegarde — SRP_FrontScreens
	string RestoreDaily(string day, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return "Front pas encore prêt";
		string wanted = day.Trim();
		if (wanted.IsEmpty())
			return "Jour de la copie manquant (AAAA-MM-JJ)";
		string path = DailyPath(wanted);
		if (!FileIO.FileExists(path))
			return "Copie du " + wanted + " introuvable (" + path + ")";

		// Lecture et contrôle AVANT de toucher à quoi que ce soit
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(path))
			return "Copie du " + wanted + " illisible : rien n'est changé";
		int format = 0;
		int grid = 0;
		string cells;
		ctx.ReadValue("format", format);
		ctx.ReadValue("grid", grid);
		ctx.ReadValue("cells", cells);
		if (format <= 0 || grid != m_iN || cells.Length() != m_iN * m_iN)
			return "Copie du " + wanted + " faite sur une autre grille (ou abîmée) : rien n'est changé";

		// Application d'un coup. Retour ordinaire : liste des copies, migration, gel, campagne et version du pont gardés.
		// Réparation d'un front.json illisible au démarrage (m_bLoaded faux) : la mémoire ne tient qu'un état neuf par
		// défaut (campagne 1, pont à 0, aucune copie, restes K2 perdus) ; tout l'état global vient donc de la copie
		bool recovered = !m_bLoaded;
		bool wasFrozen = m_bFrozen;
		array<string> knownDays = {};
		knownDays.Copy(m_aDailyCopies);
		ClearLive();
		ReadCore(ctx, true, !recovered);
		if (recovered)
		{
			// Copies relues sur le disque au démarrage, en plus de la liste de la copie ; celle du jour n'est pas
			// réécrite par la sauvegarde qui suit (elle reste un point de retour)
			MergeDailyDays(knownDays);
			string today = Today();
			if (!today.IsEmpty() && m_aDailyCopies.Contains(today) && FileIO.FileExists(DailyPath(today)))
				m_sLastDaily = today;
		}
		m_iRestoreCount++;
		m_iBridgeVersion++;
		m_bLoaded = true;	// une copie rechargée répare aussi un front.json illisible au démarrage
		if (m_bFrozen != wasFrozen && m_Capture)
			m_Capture.OnFrozenChanged(m_bFrozen);	// réparation : le gel vient de la copie

		// Modules : une seule porte par genre de remise (§4)
		NotifyReset(SRP_EFrontReset.RESTAURATION, -1);
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
		{
			enemy.ReadFrom(ctx);
			enemy.OnRestored();
		}
		SRP_BridgeComponent.NoteFrontReset();
		SRP_FrontRadio.Restored(wanted, author);

		// Réparation : le front.json illisible est gardé aux archives avant d'être remplacé
		if (recovered && FileIO.FileExists(PATH))
		{
			FileIO.MakeDirectory(ARCHIVE_DIR);
			string broken = ARCHIVE_DIR + "/front_illisible_" + Stamp() + ".json";
			if (FileIO.CopyFile(PATH, broken))
				Note("FRONT", "Front : le front.json illisible est gardé dans " + broken);
		}

		// Envoi de l'état complet et sauvegarde
		m_aPendingCells.Clear();
		m_bStatePending = false;
		BroadcastState();
		UpdateTopology();
		Save();

		string text = string.Format("Front ramené à l'état du %1 : %2 zone(s) sur %3, %4 carré(s) bleus, menace %5", wanted, CountZonesOurs(), CountZonesTotal(), CountBlueCells(), m_iThreat);
		if (recovered)
			text += " (front.json de nouveau écrit)";
		return text;
	}

	//================================================================================================
	// Interne : campagne, coupure, victoire
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Nouvelle campagne (B3, K1, Staff) : archive si m_bArchiveOnNewCampaign, campagne + 1, menace 0, gel faux,
	//! NewCampaignState, extras effacés, OnFrontReset(CAMPAGNE) aux modules, SRP_FrontEnemyComponent.ResetAll (qui
	//! prévient le Commandeur), NoteFrontReset, radio NewCampaign, envoi, sauvegarde — NewCampaign, Start (victoire)
	protected void ResetCampaign(string reason, string author)
	{
		if (!Replication.IsServer() || !m_bReady)
			return;
		int finished = m_iCampaign;

		// Archive de l'état final (B3)
		if (m_bArchiveOnNewCampaign && m_bLoaded)
		{
			Save();
			if (FileIO.FileExists(PATH))
			{
				FileIO.MakeDirectory(ARCHIVE_DIR);
				string archive = ARCHIVE_DIR + "/front_campagne" + finished.ToString() + "_fin_" + Stamp() + ".json";
				if (!FileIO.CopyFile(PATH, archive))
					Note("ERREUR", "Front : archive de fin de campagne impossible vers " + archive);
			}
		}

		// L'état neuf, appliqué d'un coup
		bool wasFrozen = m_bFrozen;
		ClearLive();
		m_iCampaign = finished + 1;
		m_sCampaignStart = SRP_Time.Now();
		m_iThreat = 0;
		m_bFrozen = false;
		m_iFrozenSince = 0;
		m_bVictory = false;
		m_sVictoryAt = "";
		m_iVictoryUnix = 0;
		NewCampaignState();
		m_mExtra.Clear();
		m_aOrphanZones.Clear();
		m_aOrphanData.Clear();

		// Les modules, puis l'ennemi (une seule porte : ResetAll), le pont, la radio
		if (wasFrozen && m_Capture)
			m_Capture.OnFrozenChanged(false);
		NotifyReset(SRP_EFrontReset.CAMPAGNE, -1);
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.ResetAll(reason);
		m_iBridgeVersion++;
		SRP_BridgeComponent.NoteFrontReset();
		SRP_FrontRadio.NewCampaign(m_iCampaign);
		Note("FRONT", string.Format("Nouvelle campagne n° %1 : %2%3", m_iCampaign, reason, AuthorSuffix(author)));

		// Envoi de l'état complet et sauvegarde
		m_aPendingCells.Clear();
		m_bStatePending = false;
		BroadcastState();
		UpdateTopology();
		Save();
	}

	//------------------------------------------------------------------------------------------------
	//! B1 : tout rouge sauf la base (bleue), zones rouges, stocks et dépôts vides, localités sans pertes — ResetCampaign, Start
	protected void NewCampaignState()
	{
		int count = Math.MinInt(m_iN * m_iN, m_aCellZone.Count());
		for (int cell = 0; cell < count; cell++)
		{
			int zone = m_aCellZone[cell];
			if (zone < 0)
				m_aCellOwner[cell] = HORS_JEU;
			else if (zone == 0)
				m_aCellOwner[cell] = SRP_EFrontOwner.BLEU;
			else
				m_aCellOwner[cell] = SRP_EFrontOwner.ROUGE;
			m_aCellLive[cell] = SRP_EFrontLive.AUCUN;
		}
		foreach (SRP_FrontZone z : m_aZones)
		{
			if (z)
				ResetZoneState(z, true);
		}
		foreach (SRP_FrontLocality locality : m_aLocalities)
		{
			if (locality)
				ResetLocalityState(locality);
		}
		m_aPendingCells.Clear();
		m_aLiveCells.Clear();
		m_aCutReminded.Clear();
		m_aCutReminded.Resize(m_aZones.Count());
		RecountBlue();
		m_iOwnerStamp++;
		m_bTopologyDirty = true;
		m_bStatePending = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Appelle OnFrontReset(kind, zone) de la capture, de l'économie et de la chute ; ENNEMI, une seule porte par
	//! genre de remise : ZONE -> SRP_FrontEnemyComponent.OnFrontReset(kind, zone) ; CAMPAGNE -> rien ici (ResetCampaign
	//! appelle ResetAll(reason)) ; RESTAURATION -> rien ici (RestoreDaily appelle ReadFrom(copie) puis OnRestored) ;
	//! puis SRP_FrontMarkers.Clear — ResetCampaign, ResetZone, RestoreDaily
	protected void NotifyReset(int kind, int zone)
	{
		if (m_Capture)
			m_Capture.OnFrontReset(kind, zone);
		if (m_ZoneFall)
			m_ZoneFall.OnFrontReset(kind, zone);
		if (m_Economy)
			m_Economy.OnFrontReset(kind, zone);
		if (kind == SRP_EFrontReset.ZONE)
		{
			SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
			if (enemy)
				enemy.OnFrontReset(kind, zone);
		}
		SRP_FrontMarkers.Clear();
		SRP_FrontRadio.ClearQueue();
	}

	//------------------------------------------------------------------------------------------------
	//! Contact (E1), front (F1), carrés bleus par zone, liaison à la base (E4, parcours en largeur marqué par
	//! m_iStamp, liaisons maritimes comprises) ; seulement si m_bTopologyDirty — FlushChanges, Housekeeping
	// Aussi appelée par les lectures de zone (IsZoneInContact…) sur chaque machine : les joueurs refont la même
	// topologie sur leur copie (I3). Aucune allocation : tableaux réutilisés.
	protected void UpdateTopology()
	{
		if (!m_bTopologyDirty || m_iN <= 0 || m_aZones.IsEmpty())
			return;
		m_bTopologyDirty = false;
		int count = Math.MinInt(m_iN * m_iN, Math.MinInt(m_aCellZone.Count(), m_aCellOwner.Count()));

		foreach (SRP_FrontZone z : m_aZones)
		{
			if (!z)
				continue;
			z.m_iBlueCells = 0;
			z.m_bInContact = false;
			z.m_bAtFront = false;
			z.m_bLinked = false;
		}

		// Carrés bleus, contact (E1) et front (F1)
		int zoneCount = m_aZones.Count();
		for (int cell = 0; cell < count; cell++)
		{
			int zone = m_aCellZone[cell];
			if (zone < 0 || zone >= zoneCount)
				continue;
			SRP_FrontZone zc = m_aZones[zone];
			if (!zc)
				continue;
			int owner = m_aCellOwner[cell];
			if (owner == SRP_EFrontOwner.BLEU)
				zc.m_iBlueCells = zc.m_iBlueCells + 1;
			if (zone == 0)
				continue;
			if (zc.m_iOwner == SRP_EFrontOwner.ROUGE && !zc.m_bInContact)
			{
				if (owner == SRP_EFrontOwner.BLEU || HasNeighbourOwnedBy(cell, SRP_EFrontOwner.BLEU, true))
					zc.m_bInContact = true;
			}
			else if (zc.m_iOwner == SRP_EFrontOwner.BLEU && !zc.m_bAtFront)
			{
				if (owner == SRP_EFrontOwner.ROUGE || HasNeighbourOwnedBy(cell, SRP_EFrontOwner.ROUGE, true))
					zc.m_bAtFront = true;
			}
		}

		// Liaison à la base (E4) : parcours en largeur depuis les carrés de la base, à travers les carrés bleus
		if (m_aLinkStamp.Count() != count)
		{
			m_aLinkStamp.Clear();
			m_aLinkStamp.Resize(count);
			m_iStamp = 0;
		}
		m_iStamp++;
		m_aQueue.Clear();
		for (int start = 0; start < count; start++)
		{
			if (m_aCellZone[start] == 0 && m_aCellOwner[start] == SRP_EFrontOwner.BLEU)
			{
				m_aLinkStamp[start] = m_iStamp;
				m_aQueue.Insert(start);
			}
		}
		int head = 0;
		while (head < m_aQueue.Count())
		{
			int current = m_aQueue[head];
			head++;
			int currentZone = m_aCellZone[current];
			if (currentZone >= 0 && currentZone < zoneCount && m_aZones[currentZone])
				m_aZones[currentZone].m_bLinked = true;
			for (int dir = 0; dir < 5; dir++)
			{
				int next = -1;
				if (dir < 4)
					next = Neighbour(current, dir);
				else
					next = GetSeaLink(current);
				if (next < 0 || next >= count || m_aCellZone[next] < 0)
					continue;
				if (m_aCellOwner[next] != SRP_EFrontOwner.BLEU || m_aLinkStamp[next] == m_iStamp)
					continue;
				m_aLinkStamp[next] = m_iStamp;
				m_aQueue.Insert(next);
			}
		}
		if (m_aZones[0])
			m_aZones[0].m_bLinked = true;
	}

	//------------------------------------------------------------------------------------------------
	//! E4 par ZONE (seule implémentation, trou 8) : coupée -> m_iCutSince = maintenant, FLAG_CUT, radio ZoneCut ;
	//! reliée -> 0 et radio Reconnected ; m_iCutLossMinutes dépassées hors gel -> m_ZoneFall.LoseZone(zone,
	//! ISOLEMENT, true) ; rappel SRP_FrontRadio.CutReminder — Housekeeping, Start (temps serveur éteint compris)
	protected void CheckCuts(int nowUnix)
	{
		if (!Replication.IsServer() || !m_bReady)
			return;
		UpdateTopology();
		int total = Math.MaxInt(m_iCutLossMinutes, 1) * 60;
		int reminder = Math.MaxInt(m_iCutReminderMinutes, 0) * 60;
		int zoneCount = m_aZones.Count();
		if (m_aCutReminded.Count() != zoneCount)
			m_aCutReminded.Resize(zoneCount);

		array<int> toLose = {};
		for (int zone = 1; zone < zoneCount; zone++)
		{
			SRP_FrontZone z = m_aZones[zone];
			if (!z)
				continue;

			// Hors E4 : zone rouge ou protégée (B4) ; une coupure oubliée est effacée en silence
			if (z.m_iOwner != SRP_EFrontOwner.BLEU || IsZoneProtected(zone))
			{
				if (z.m_iCutSince > 0 || (z.m_iFlags & FLAG_CUT) != 0)
				{
					z.m_iCutSince = 0;
					z.m_iFlags = z.m_iFlags - (z.m_iFlags & FLAG_CUT);
					m_aCutReminded[zone] = 0;
					m_bStatePending = true;
					MarkDirty(false);
				}
				continue;
			}

			// Reliée à la base
			if (z.m_bLinked)
			{
				if (z.m_iCutSince > 0 || (z.m_iFlags & FLAG_CUT) != 0)
				{
					z.m_iCutSince = 0;
					z.m_iFlags = z.m_iFlags - (z.m_iFlags & FLAG_CUT);
					m_aCutReminded[zone] = 0;
					m_bStatePending = true;
					MarkDirty(true);
					SRP_FrontRadio.Reconnected(zone);
				}
				continue;
			}

			// Coupée à l'instant
			if (z.m_iCutSince <= 0)
			{
				z.m_iCutSince = nowUnix;
				z.m_iFlags = z.m_iFlags | FLAG_CUT;
				m_aCutReminded[zone] = 0;
				m_bStatePending = true;
				MarkDirty(true);
				// Arrondi à l'heure, jamais « 0 h » (réglage d'essai front_coupure_minutes = 2)
				SRP_FrontRadio.ZoneCut(zone, Math.MaxInt((total + 1800) / 3600, 1));
				continue;
			}
			if ((z.m_iFlags & FLAG_CUT) == 0)
			{
				z.m_iFlags = z.m_iFlags | FLAG_CUT;	// relue du fichier : le drapeau suit m_iCutSince
				m_bStatePending = true;
			}

			// Déjà coupée : décompte, arrêté pendant le gel (Q1)
			if (m_bFrozen)
				continue;
			int left = z.m_iCutSince + total - nowUnix;
			if (left <= 0)
			{
				toLose.Insert(zone);
				continue;
			}
			if (reminder > 0 && left <= reminder && m_aCutReminded[zone] != z.m_iCutSince)
			{
				m_aCutReminded[zone] = z.m_iCutSince;
				SRP_FrontRadio.CutReminder(zone, (left + 59) / 60);
			}
		}
		if (m_bStatePending)
			QueueFlush();

		// Pertes par isolement : la chute est la seule porte des changements de zone (repeinte comprise)
		foreach (int lost : toLose)
		{
			if (m_ZoneFall)
				m_ZoneFall.LoseZone(lost, SRP_EFrontReason.ISOLEMENT, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! B3 : toutes les zones jouables bleues -> m_bVictory, gel, SRP_FrontRadio.Victory ; nouvelle campagne au
	//! prochain démarrage (ou après m_iVictoryResetMinutes) — SetZoneOwner
	protected void CheckVictory()
	{
		if (!Replication.IsServer() || m_bVictory || m_aZones.Count() <= 1)
			return;
		for (int zone = 1; zone < m_aZones.Count(); zone++)
		{
			SRP_FrontZone z = m_aZones[zone];
			if (!z || z.m_iOwner != SRP_EFrontOwner.BLEU)
				return;
		}

		int now = System.GetUnixTime();
		m_bVictory = true;
		m_sVictoryAt = SRP_Time.Now();
		m_iVictoryUnix = now;
		if (!m_bFrozen)
		{
			// Gel sans annonce de gel : la victoire a sa propre annonce
			m_bFrozen = true;
			m_iFrozenSince = now;
			ClearLive();
			if (m_Capture)
				m_Capture.OnFrozenChanged(true);
		}
		m_bStatePending = true;
		QueueFlush();
		MarkDirty(true);
		SRP_FrontRadio.Victory();
	}

	//================================================================================================
	// Interne : réseau (modèle SRP_Sirene.c:504-515 et 658-700 ; RPC à tableaux comme SCR_VoiceoverSystem.c:404)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Un seul CallLater(FlushChanges, 0) par image — SetCellsOwner, SetZoneOwner, SetZoneFlag, SetFrozen
	protected void QueueFlush()
	{
		if (!Replication.IsServer() || m_bFlushQueued)
			return;
		m_bFlushQueued = true;
		GetGame().GetCallqueue().CallLater(FlushChanges, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Envoie RpcDo_FrontCells (m_aPendingCells) et RpcDo_FrontState si m_bStatePending ; m_iStateVersion++ ICI (hôte),
	//! puis UpdateTopology — QueueFlush
	protected void FlushChanges()
	{
		m_bFlushQueued = false;
		if (!Replication.IsServer() || !m_bReady)
			return;
		if (m_bStatePending)
		{
			// L'état complet contient aussi les carrés en attente
			m_bStatePending = false;
			m_aPendingCells.Clear();
			BroadcastState();
		}
		else if (!m_aPendingCells.IsEmpty())
		{
			array<int> changes = {};
			changes.Copy(m_aPendingCells);
			m_aPendingCells.Clear();
			Rpc(RpcDo_FrontCells, changes);
			m_iStateVersion++;	// l'hôte : le Broadcast ne s'y exécute pas, ses données sont déjà à jour
		}
		UpdateTopology();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : Rpc(RpcDo_FrontGeometry, …) avec les tableaux de BuildGeometryArrays — Start (joueurs arrivés avant)
	protected void BroadcastGeometry()
	{
		if (!Replication.IsServer() || !m_bReady)
			return;
		array<int> header = {};
		array<int> zones = {};
		array<string> names = {};
		array<int> zoneStatic = {};
		array<int> keyPoints = {};
		array<int> seaLinks = {};
		BuildGeometryArrays(header, zones, names, zoneStatic, keyPoints, seaLinks);
		Rpc(RpcDo_FrontGeometry, header, zones, names, zoneStatic, keyPoints, seaLinks);
		m_iGeometryVersion++;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : Rpc(RpcDo_FrontState, …) avec les tableaux de BuildStateArrays — Start, FlushChanges, remises
	protected void BroadcastState()
	{
		if (!Replication.IsServer() || !m_bReady)
			return;
		array<int> header = {};
		array<int> owners = {};
		array<int> zoneData = {};
		array<int> live = {};
		BuildStateArrays(header, owners, zoneData, live);
		Rpc(RpcDo_FrontState, header, owners, zoneData, live);
		m_iStateVersion++;
		m_iLiveVersion++;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : header = [1, N, taille, zones, points clés, somme] ; zones = 7 bits par carré (127 hors jeu, 4 par
	//! entier) ; names ; zoneStatic = protégée | (île << 1) | (genre << 2) ; keyPoints = 4 entiers (zone, rôle, x, z) ;
	//! seaLinks — BroadcastGeometry, RplSave
	// Précisions du format : names = code puis nom de chaque zone ; zoneStatic porte aussi le numéro de terre de la
	// zone (terre << 4) ; le rôle d'un point clé porte aussi son carré (rôle | (carré + 1) << 1) ; keyPoints est suivi
	// de 4 entiers par zone (centre x, z, position du nom x, z ; 0, 0 = sans position) ; seaLinks = paires (carré,
	// carré relié).
	protected void BuildGeometryArrays(notnull array<int> header, notnull array<int> zones, notnull array<string> names, notnull array<int> zoneStatic, notnull array<int> keyPoints, notnull array<int> seaLinks)
	{
		header.Clear();
		zones.Clear();
		names.Clear();
		zoneStatic.Clear();
		keyPoints.Clear();
		seaLinks.Clear();
		int count = m_iN * m_iN;
		int size = Math.Round(GetCellSize());
		header.Insert(1);
		header.Insert(m_iN);
		header.Insert(size);
		header.Insert(m_aZones.Count());
		header.Insert(m_aKeyPoints.Count());
		header.Insert(Math.MaxInt(m_iGeomHash, 0));

		// Zone de chaque carré, 7 bits (127 = hors jeu)
		array<int> values = {};
		for (int cell = 0; cell < count; cell++)
		{
			int zone = -1;
			if (cell < m_aCellZone.Count())
				zone = m_aCellZone[cell];
			if (zone < 0 || zone > 126)
				zone = 127;
			values.Insert(zone);
		}
		SRP_FrontPack.Pack(values, 7, zones);

		// Zones : noms et données fixes
		foreach (int index, SRP_FrontZone z : m_aZones)
		{
			if (!z)
			{
				names.Insert("");
				names.Insert("");
				zoneStatic.Insert(0);
				continue;
			}
			names.Insert(z.m_sCode);
			names.Insert(z.m_sName);
			int packed = 0;
			if (index == 0 || z.m_bBase || z.m_bShield)
				packed = 1;
			if (z.m_bIsland)
				packed = packed | 2;
			packed = packed | (Math.ClampInt(z.m_iKind, 0, 3) << 2);
			int land = 0;
			if (!z.m_aCells.IsEmpty())
				land = Math.ClampInt(GetCellLand(z.m_aCells[0]), 0, 15);
			packed = packed | (land << 4);
			zoneStatic.Insert(packed);
		}

		// Points clés, puis centres et positions des noms des zones
		foreach (SRP_FrontKeyPoint point : m_aKeyPoints)
		{
			if (!point)
			{
				keyPoints.Insert(0);
				keyPoints.Insert(0);
				keyPoints.Insert(0);
				keyPoints.Insert(0);
				continue;
			}
			// Rôle | (carré + 1) << 1 : les joueurs gardent le carré du serveur (un point rattaché à un autre carré que
			// celui de sa position, en mer ou dans la base, donnerait sinon un autre carré chez eux)
			int pointCell = Math.ClampInt(point.m_iCell + 1, 0, count);
			keyPoints.Insert(Math.MaxInt(point.m_iZone, 0));
			keyPoints.Insert(Math.ClampInt(point.m_iRole, 0, 1) | (pointCell << 1));
			keyPoints.Insert(NetCoord(point.m_vPos[0]));
			keyPoints.Insert(NetCoord(point.m_vPos[2]));
		}
		foreach (SRP_FrontZone zp : m_aZones)
		{
			if (!zp)
			{
				keyPoints.Insert(0);
				keyPoints.Insert(0);
				keyPoints.Insert(0);
				keyPoints.Insert(0);
				continue;
			}
			keyPoints.Insert(NetCoord(zp.m_vCentroid[0]));
			keyPoints.Insert(NetCoord(zp.m_vCentroid[2]));
			keyPoints.Insert(NetCoord(zp.m_vLabelPos[0]));
			keyPoints.Insert(NetCoord(zp.m_vLabelPos[2]));
		}

		// Liaisons maritimes (Q5)
		int links = Math.MinInt(count, m_aSeaLink.Count());
		for (int from = 0; from < links; from++)
		{
			int to = m_aSeaLink[from];
			if (to < 0 || to >= count)
				continue;
			seaLinks.Insert(from);
			seaLinks.Insert(to);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : header = [version, gelé, victoire, campagne] ; owners = 1 bit par carré (28 par entier) ; zoneData =
	//! propriétaire | (drapeaux << 1) ; live = (carré << 2) | état des carrés contestés — BroadcastState, RplSave
	// Le header porte aussi la menace en 5e position (lue par les joueurs si présente).
	protected void BuildStateArrays(notnull array<int> header, notnull array<int> owners, notnull array<int> zoneData, notnull array<int> live)
	{
		header.Clear();
		owners.Clear();
		zoneData.Clear();
		live.Clear();
		header.Insert(Math.MaxInt(m_iStateVersion, 0));
		if (m_bFrozen)
			header.Insert(1);
		else
			header.Insert(0);
		if (m_bVictory)
			header.Insert(1);
		else
			header.Insert(0);
		header.Insert(Math.MaxInt(m_iCampaign, 0));
		header.Insert(Math.ClampInt(m_iThreat, 0, 10));

		int count = m_iN * m_iN;
		array<int> bits = {};
		for (int cell = 0; cell < count; cell++)
		{
			if (cell < m_aCellOwner.Count() && m_aCellOwner[cell] == SRP_EFrontOwner.BLEU)
				bits.Insert(1);
			else
				bits.Insert(0);
		}
		SRP_FrontPack.Pack(bits, 1, owners);

		foreach (SRP_FrontZone z : m_aZones)
		{
			if (!z)
			{
				zoneData.Insert(0);
				continue;
			}
			int owner = 0;
			if (z.m_iOwner == SRP_EFrontOwner.BLEU)
				owner = 1;
			zoneData.Insert(owner | (Math.ClampInt(z.m_iFlags, 0, 255) << 1));
		}

		int liveCount = Math.MinInt(count, m_aCellLive.Count());
		for (int c = 0; c < liveCount; c++)
		{
			if (m_aCellLive[c] != SRP_EFrontLive.AUCUN)
				live.Insert((c << 2) | (m_aCellLive[c] & 3));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : la géométrie complète — Broadcast du serveur
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_FrontGeometry(array<int> header, array<int> zones, array<string> names, array<int> zoneStatic, array<int> keyPoints, array<int> seaLinks)
	{
		ApplyGeometry(header, zones, names, zoneStatic, keyPoints, seaLinks);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : l'état complet (propriétaires, zones, gel, victoire) — Broadcast du serveur
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_FrontState(array<int> header, array<int> owners, array<int> zoneData, array<int> live)
	{
		ApplyState(header, owners, zoneData, live);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : un lot de changements (carré << 1) | propriétaire — Broadcast du serveur
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_FrontCells(array<int> changes)
	{
		ApplyCells(changes);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : états contestés (carré << 2) | état — Broadcast du serveur
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_FrontLive(array<int> live)
	{
		ApplyLive(live);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur, arrivant : WriteBool(m_bReady) puis, si prêt, les tableaux de géométrie et d'état (5 à 6 Ko,
	//! WriteIntRange 0..268435455, WriteString) — le moteur (EnNetwork.c:126-196)
	override bool RplSave(ScriptBitWriter writer)
	{
		writer.WriteBool(m_bReady);
		if (!m_bReady)
			return true;

		array<int> header = {};
		array<int> zones = {};
		array<string> names = {};
		array<int> zoneStatic = {};
		array<int> keyPoints = {};
		array<int> seaLinks = {};
		BuildGeometryArrays(header, zones, names, zoneStatic, keyPoints, seaLinks);
		WriteInts(writer, header);
		WriteInts(writer, zones);
		writer.WriteInt(names.Count());
		foreach (string name : names)
		{
			writer.WriteString(name);
		}
		WriteInts(writer, zoneStatic);
		WriteInts(writer, keyPoints);
		WriteInts(writer, seaLinks);

		array<int> stateHeader = {};
		array<int> owners = {};
		array<int> zoneData = {};
		array<int> live = {};
		BuildStateArrays(stateHeader, owners, zoneData, live);
		WriteInts(writer, stateHeader);
		WriteInts(writer, owners);
		WriteInts(writer, zoneData);
		WriteInts(writer, live);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueur arrivant : lecture miroir avec gardes (N <= 256, zones <= 126, points <= 256, sinon false), puis
	//! ApplyGeometry et ApplyState — le moteur (EnNetwork.c:198-240)
	override bool RplLoad(ScriptBitReader reader)
	{
		bool ready;
		if (!reader.ReadBool(ready))
			return false;
		if (!ready)
			return true;	// le front n'était pas encore construit : la géométrie arrivera par RPC

		array<int> header = {};
		array<int> zones = {};
		array<string> names = {};
		array<int> zoneStatic = {};
		array<int> keyPoints = {};
		array<int> seaLinks = {};
		if (!ReadInts(reader, header, 16) || !ReadInts(reader, zones, 16384))
			return false;
		int nameCount;
		if (!reader.ReadInt(nameCount) || nameCount < 0 || nameCount > 252)
			return false;
		string name;
		for (int i = 0; i < nameCount; i++)
		{
			if (!reader.ReadString(name))
				return false;
			names.Insert(name);
		}
		if (!ReadInts(reader, zoneStatic, 126) || !ReadInts(reader, keyPoints, 256 * 4 + 126 * 4) || !ReadInts(reader, seaLinks, 4096))
			return false;

		array<int> stateHeader = {};
		array<int> owners = {};
		array<int> zoneData = {};
		array<int> live = {};
		if (!ReadInts(reader, stateHeader, 16) || !ReadInts(reader, owners, 2400) || !ReadInts(reader, zoneData, 126) || !ReadInts(reader, live, 65536))
			return false;

		if (!DoApplyGeometry(header, zones, names, zoneStatic, keyPoints, seaLinks))
			return false;
		DoApplyState(stateHeader, owners, zoneData, live);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes machines : applique la géométrie reçue (zones, noms, statiques, points clés, liaisons), m_bReady,
	//! m_iGeometryVersion++ — RplLoad, RpcDo_FrontGeometry
	protected void ApplyGeometry(array<int> header, array<int> zones, array<string> names, array<int> zoneStatic, array<int> keyPoints, array<int> seaLinks)
	{
		if (!DoApplyGeometry(header, zones, names, zoneStatic, keyPoints, seaLinks))
			Print("[SRP] Front : géométrie reçue illisible, ignorée", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes machines : applique l'état complet, m_iStateVersion++ — RplLoad, RpcDo_FrontState
	protected void ApplyState(array<int> header, array<int> owners, array<int> zoneData, array<int> live)
	{
		if (!DoApplyState(header, owners, zoneData, live))
			Print("[SRP] Front : état reçu illisible ou avant la géométrie, ignoré", LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : applique un lot de carrés, m_iStateVersion++ — RpcDo_FrontCells
	protected void ApplyCells(array<int> changes)
	{
		if (!changes || m_iN <= 0 || Replication.IsServer())
			return;
		int count = Math.MinInt(m_iN * m_iN, Math.MinInt(m_aCellOwner.Count(), m_aCellZone.Count()));
		int zoneCount = m_aZones.Count();
		foreach (int change : changes)
		{
			int cell = change >> 1;
			int owner = change & 1;
			if (cell < 0 || cell >= count)
				continue;
			int zone = m_aCellZone[cell];
			if (zone <= 0 || zone >= zoneCount)
				continue;	// hors jeu, ou la base (toujours bleue)
			if (m_aCellOwner[cell] == owner)
				continue;
			m_aCellOwner[cell] = owner;
			SRP_FrontZone z = m_aZones[zone];
			if (z)
			{
				if (owner == SRP_EFrontOwner.BLEU)
					z.m_iBlueCells = z.m_iBlueCells + 1;
				else if (z.m_iBlueCells > 0)
					z.m_iBlueCells = z.m_iBlueCells - 1;
			}
		}
		m_iOwnerStamp++;
		m_bTopologyDirty = true;
		m_iStateVersion++;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : applique des états contestés, m_iLiveVersion++ — RpcDo_FrontLive
	protected void ApplyLive(array<int> live)
	{
		if (!live || Replication.IsServer())
			return;
		int count = m_aCellLive.Count();
		foreach (int value : live)
		{
			int cell = value >> 2;
			if (cell >= 0 && cell < count)
				m_aCellLive[cell] = value & 3;
		}
		m_iLiveVersion++;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : contrôle puis application de la géométrie ; faux si les tableaux sont incohérents (rien n'est
	//! touché) — ApplyGeometry, RplLoad
	protected bool DoApplyGeometry(array<int> header, array<int> zones, array<string> names, array<int> zoneStatic, array<int> keyPoints, array<int> seaLinks)
	{
		if (!header || !zones || !names || !zoneStatic || !keyPoints || !seaLinks)
			return false;
		if (header.Count() < 6)
			return false;
		int n = header[1];
		int size = header[2];
		int zoneCount = header[3];
		int pointCount = header[4];
		if (n <= 0 || n > 256 || size <= 0 || zoneCount <= 0 || zoneCount > 126 || pointCount < 0 || pointCount > 256)
			return false;
		if (names.Count() != zoneCount * 2 || zoneStatic.Count() != zoneCount || keyPoints.Count() != pointCount * 4 + zoneCount * 4)
			return false;
		int cellCount = n * n;
		array<int> values = {};
		SRP_FrontPack.Unpack(zones, 7, cellCount, values);
		if (values.Count() != cellCount)
			return false;

		// Grille
		m_iN = n;
		m_fGridCell = size;
		m_aCellZone.Clear();
		m_aCellZone.Resize(cellCount);
		m_aCellOwner.Clear();
		m_aCellOwner.Resize(cellCount);
		m_aCellLive.Clear();
		m_aCellLive.Resize(cellCount);
		m_aCellLand.Clear();
		m_aCellLand.Resize(cellCount);
		m_aSeaLink.Clear();
		m_aSeaLink.Resize(cellCount);
		m_aLinkStamp.Clear();
		m_aLinkStamp.Resize(cellCount);
		m_iStamp = 0;

		// Zones
		m_aZones.Clear();
		array<int> landOfZone = {};
		for (int i = 0; i < zoneCount; i++)
		{
			SRP_FrontZone z = new SRP_FrontZone();
			z.m_iIndex = i;
			z.m_sCode = names[i * 2];
			z.m_sName = names[i * 2 + 1];
			int packed = zoneStatic[i];
			z.m_bBase = (i == 0);
			z.m_bShield = (i != 0 && (packed & 1) != 0);
			z.m_bIsland = (packed & 2) != 0;
			z.m_iKind = (packed >> 2) & 3;
			if (i == 0)
				z.m_iOwner = SRP_EFrontOwner.BLEU;
			landOfZone.Insert((packed >> 4) & 15);
			int tail = pointCount * 4 + i * 4;
			z.m_vCentroid = GroundPoint(keyPoints[tail], keyPoints[tail + 1]);
			z.m_vLabelPos = GroundPoint(keyPoints[tail + 2], keyPoints[tail + 3]);
			m_aZones.Insert(z);
		}

		// Carrés
		m_iLandCells = 0;
		for (int cell = 0; cell < cellCount; cell++)
		{
			m_aSeaLink[cell] = -1;
			int zone = values[cell];
			if (zone < 0 || zone >= zoneCount)
			{
				m_aCellZone[cell] = -1;
				m_aCellOwner[cell] = HORS_JEU;
				m_aCellLand[cell] = -1;
				continue;
			}
			m_aCellZone[cell] = zone;
			m_aCellLand[cell] = landOfZone[zone];
			if (zone == 0)
				m_aCellOwner[cell] = SRP_EFrontOwner.BLEU;
			else
				m_aCellOwner[cell] = SRP_EFrontOwner.ROUGE;
			m_aZones[zone].m_aCells.Insert(cell);
			m_iLandCells++;
		}

		// Liaisons maritimes
		int pairs = seaLinks.Count() / 2;
		for (int p = 0; p < pairs; p++)
		{
			int from = seaLinks[p * 2];
			int to = seaLinks[p * 2 + 1];
			if (from >= 0 && from < cellCount && to >= 0 && to < cellCount)
				m_aSeaLink[from] = to;
		}

		// Registre des points clés (position au sol, carré, zone)
		m_aKeyPoints.Clear();
		for (int k = 0; k < pointCount; k++)
		{
			SRP_FrontKeyPoint point = new SRP_FrontKeyPoint();
			point.m_iIndex = k;
			point.m_iZone = keyPoints[k * 4];
			int roleAndCell = keyPoints[k * 4 + 1];
			point.m_iRole = roleAndCell & 1;
			point.m_vPos = GroundPoint(keyPoints[k * 4 + 2], keyPoints[k * 4 + 3]);
			// Le carré du serveur (voir BuildGeometryArrays) ; à défaut, celui de la position
			int sentCell = (roleAndCell >> 1) - 1;
			if (sentCell >= 0 && sentCell < cellCount && m_aCellZone[sentCell] >= 0)
				point.m_iCell = sentCell;
			else
				point.m_iCell = CellIndexAt(point.m_vPos);
			m_aKeyPoints.Insert(point);
			if (point.m_iZone >= 0 && point.m_iZone < zoneCount)
				m_aZones[point.m_iZone].m_aKeyPoints.Insert(k);
		}

		m_iGeomHash = header[5];
		m_bReady = true;
		m_iGeometryVersion++;
		m_iOwnerStamp++;
		m_bTopologyDirty = true;
		RecountBlue();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs : contrôle puis application de l'état complet ; faux sans géométrie ou si les tableaux sont
	//! incohérents — ApplyState, RplLoad
	protected bool DoApplyState(array<int> header, array<int> owners, array<int> zoneData, array<int> live)
	{
		if (!header || !owners || !zoneData || !live)
			return false;
		if (m_iN <= 0 || !m_bReady || header.Count() < 4)
			return false;
		int cellCount = m_iN * m_iN;
		if (m_aCellZone.Count() != cellCount || m_aCellOwner.Count() != cellCount || zoneData.Count() != m_aZones.Count())
			return false;
		array<int> bits = {};
		SRP_FrontPack.Unpack(owners, 1, cellCount, bits);
		if (bits.Count() != cellCount)
			return false;

		m_bFrozen = header[1] != 0;
		m_bVictory = header[2] != 0;
		m_iCampaign = header[3];
		if (header.Count() >= 5)
			m_iThreat = header[4];

		for (int cell = 0; cell < cellCount; cell++)
		{
			int zone = m_aCellZone[cell];
			if (zone < 0)
				m_aCellOwner[cell] = HORS_JEU;
			else if (zone == 0 || bits[cell] != 0)
				m_aCellOwner[cell] = SRP_EFrontOwner.BLEU;
			else
				m_aCellOwner[cell] = SRP_EFrontOwner.ROUGE;
			m_aCellLive[cell] = SRP_EFrontLive.AUCUN;
		}
		for (int i = 0; i < zoneData.Count(); i++)
		{
			SRP_FrontZone z = m_aZones[i];
			if (!z)
				continue;
			int data = zoneData[i];
			z.m_iOwner = data & 1;
			if (i == 0)
				z.m_iOwner = SRP_EFrontOwner.BLEU;
			z.m_iFlags = data >> 1;
		}
		foreach (int value : live)
		{
			int liveCell = value >> 2;
			if (liveCell >= 0 && liveCell < cellCount)
				m_aCellLive[liveCell] = value & 3;
		}

		RecountBlue();
		m_iOwnerStamp++;
		m_bTopologyDirty = true;
		m_iStateVersion++;
		m_iLiveVersion++;
		return true;
	}

	//================================================================================================
	// Interne : sauvegarde front.json (JsonSaveContext à plat, comme SRP_Territory.c:2356-2437), schéma dans CONTRATS
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Écrit toutes les clés du socle (zones avec leurs champs d'économie, localités avec F12 et champs du Commandeur,
	//! extras), puis SRP_FrontEnemyComponent.WriteTo (clés fe_*, qui chaîne SRP_Commander.WriteTo, clés cmd_*) ; n'est
	//! jamais appelé sans l'ennemi quand m_bEnemyChained (garde dans Save) — Save
	protected void WriteTo(JsonSaveContext ctx)
	{
		int now = System.GetUnixTime();
		int format = FORMAT;
		ctx.WriteValue("format", format);
		ctx.WriteValue("savedAt", SRP_Time.Now());
		ctx.WriteValue("savedUnix", now);
		ctx.WriteValue("campaign", m_iCampaign);
		ctx.WriteValue("campaignStart", m_sCampaignStart);
		ctx.WriteValue("threat", m_iThreat);
		ctx.WriteValue("frozen", m_bFrozen);
		ctx.WriteValue("frozenSince", m_iFrozenSince);
		ctx.WriteValue("victory", m_bVictory);
		ctx.WriteValue("victoryAt", m_sVictoryAt);
		ctx.WriteValue("victoryUnix", m_iVictoryUnix);
		ctx.WriteValue("bridgeVersion", m_iBridgeVersion);
		ctx.WriteValue("restores", m_iRestoreCount);
		ctx.WriteValue("grid", m_iN);
		float cellSize = GetCellSize();
		ctx.WriteValue("cell", cellSize);
		ctx.WriteValue("geom", m_iGeomHash);
		ctx.WriteValue("cells", GetCellsString());

		// Zones (relues par CODE), puis les zones du fichier restées sans correspondance, gardées telles quelles
		int zoneCount = m_aZones.Count();
		int orphanCount = Math.MinInt(m_aOrphanZones.Count(), m_aOrphanData.Count());
		ctx.WriteValue("zones", zoneCount + orphanCount);
		for (int i = 0; i < zoneCount; i++)
		{
			SRP_FrontZone z = m_aZones[i];
			if (!z)
				continue;
			string prefix = "z" + i.ToString() + "_";
			ctx.WriteValue(prefix + "code", z.m_sCode);
			ctx.WriteValue(prefix + "owner", z.m_iOwner);
			ctx.WriteValue(prefix + "captured", z.m_iCapturedAt);
			ctx.WriteValue(prefix + "lost", z.m_iLostAt);
			ctx.WriteValue(prefix + "taken", z.m_iTakenCount);
			ctx.WriteValue(prefix + "cut", z.m_iCutSince);
			ctx.WriteValue(prefix + "last", z.m_sLastChange);
			ctx.WriteValue(prefix + "stock", z.m_iStock);
			ctx.WriteValue(prefix + "depot", z.m_bDepot);
			float depotX = z.m_vDepot[0];
			float depotY = z.m_vDepot[1];
			float depotZ = z.m_vDepot[2];
			ctx.WriteValue(prefix + "dx", depotX);
			ctx.WriteValue(prefix + "dy", depotY);
			ctx.WriteValue(prefix + "dz", depotZ);
		}
		for (int o = 0; o < orphanCount; o++)
		{
			int orphanIndex = zoneCount + o;
			WriteOrphanZone(ctx, "z" + orphanIndex.ToString() + "_", m_aOrphanZones[o], m_aOrphanData[o]);
		}

		// Localités (relues par NOM) : mémoire F12 et champs du Commandeur
		ctx.WriteValue("loc", m_aLocalities.Count());
		for (int l = 0; l < m_aLocalities.Count(); l++)
		{
			SRP_FrontLocality locality = m_aLocalities[l];
			if (!locality)
				continue;
			string lp = "l" + l.ToString() + "_";
			ctx.WriteValue(lp + "name", locality.m_sName);
			ctx.WriteValue(lp + "losses", locality.m_iLosses);
			ctx.WriteValue(lp + "lossesAt", locality.m_iLossesAt);
			ctx.WriteValue(lp + "sent", locality.m_iSent);
			ctx.WriteValue(lp + "sentAt", locality.m_iSentAt);
			ctx.WriteValue(lp + "bonus", locality.m_iBonus);
			ctx.WriteValue(lp + "bonusAt", locality.m_iBonusAt);
			ctx.WriteValue(lp + "emptied", locality.m_iEmptiedAt);
			ctx.WriteValue(lp + "full", locality.m_iFullUntil);
		}

		// Copies datées (J2)
		ctx.WriteValue("daily", m_aDailyCopies.Count());
		for (int d = 0; d < m_aDailyCopies.Count(); d++)
		{
			string summary = "";
			if (d < m_aDailySummaries.Count())
				summary = m_aDailySummaries[d];
			ctx.WriteValue("d" + d.ToString(), m_aDailyCopies[d]);
			ctx.WriteValue("d" + d.ToString() + "_s", summary);
		}
		ctx.WriteValue("lastDaily", m_sLastDaily);

		// Migration (K1, K2)
		ctx.WriteValue("migrated", m_bMigrated);
		ctx.WriteValue("legacyVivres", m_iLegacyVivres);
		ctx.WriteValue("legacyMunitions", m_iLegacyMunitions);
		ctx.WriteValue("legacyNext", m_iLegacyNextTry);
		ctx.WriteValue("legacyInfo", m_sLegacyInfo);

		// Extras des autres modules
		int extraCount = m_mExtra.Count();
		ctx.WriteValue("extra", extraCount);
		for (int x = 0; x < extraCount; x++)
		{
			ctx.WriteValue("x" + x.ToString() + "_k", m_mExtra.GetKey(x));
			ctx.WriteValue("x" + x.ToString() + "_v", m_mExtra.GetElement(x));
		}

		// La chaîne unique : ennemi du front (fe_*), qui chaîne le Commandeur (cmd_*)
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.WriteTo(ctx);
	}

	//------------------------------------------------------------------------------------------------
	//! Lit toutes les clés (carrés par index, zones par code, localités par nom ; code inconnu gardé de côté et
	//! signalé ; somme différente = avertissement), puis SRP_FrontEnemyComponent.ReadFrom APRÈS zones et localités
	//! (chaîne SRP_Commander.ReadFrom) ; carrés de base toujours bleus — Load, RestoreDaily
	protected bool ReadFrom(JsonLoadContext ctx)
	{
		if (!ReadCore(ctx, false, false))
			return false;
		SRP_FrontEnemyComponent enemy = SRP_FrontEnemyComponent.GetInstance();
		if (enemy)
			enemy.ReadFrom(ctx);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Charge PATH, sinon sa version précédente PREV_PATH (front.json tronqué par un arrêt brutal pendant l'écriture) ;
	//! faux si absent ou si les deux sont illisibles (ERREUR, m_bLoaded reste faux : rien n'est écrit) — Start
	protected bool Load()
	{
		if (!FileIO.FileExists(PATH))
			return false;
		if (LoadPath(PATH))
		{
			m_bPathGood = true;
			return true;
		}
		if (!FileIO.FileExists(PREV_PATH))
			return false;
		Note("ERREUR", "Front : " + PATH + " est illisible ; relecture de la version précédente (" + PREV_PATH + "), les dernières secondes avant l'arrêt sont perdues");
		// m_bPathGood reste faux : le front.json abîmé ne remplacera pas la bonne version précédente
		return LoadPath(PREV_PATH);
	}

	//------------------------------------------------------------------------------------------------
	//! Lit un fichier de sauvegarde (format, puis ReadFrom) ; faux s'il est illisible — Load
	protected bool LoadPath(string filePath)
	{
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(filePath))
			return false;
		int format = 0;
		if (!ctx.ReadValue("format", format) || format <= 0)
			return false;
		if (!ReadFrom(ctx))
			return false;
		Note("FRONT", string.Format("%1 relu : campagne %2, %3 zone(s) sur %4, %5 carré(s) bleus, menace %6", FilePath.StripPath(filePath), m_iCampaign, CountZonesOurs(), CountZonesTotal(), CountBlueCells(), m_iThreat));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Copie datée DAILY_DIR/front_AAAA-MM-JJ.json si le jour a changé, puis nettoyage au-delà de m_iDailyKeep — Save
	protected void DailyCopy()
	{
		string today = Today();
		if (today.IsEmpty() || !FileIO.FileExists(PATH))
			return;
		m_sLastDaily = today;	// un seul essai par jour, même en cas d'échec (pas d'erreur à chaque écriture)
		FileIO.MakeDirectory(DAILY_DIR);
		string target = DailyPath(today);
		if (FileIO.FileExists(target))
			FileIO.DeleteFile(target);
		if (!FileIO.CopyFile(PATH, target))
		{
			Note("ERREUR", "Front : copie datée impossible vers " + target);
			return;
		}

		// La liste des copies (tenue dans front.json) et leur résumé pour le menu Staff
		while (m_aDailySummaries.Count() < m_aDailyCopies.Count())
		{
			m_aDailySummaries.Insert("");
		}
		string summary = string.Format("%1 zone(s), %2 carré(s) à nous, menace %3", CountZonesOurs(), CountBlueCells(), m_iThreat);
		int index = m_aDailyCopies.Find(today);
		if (index >= 0)
		{
			m_aDailySummaries[index] = summary;
		}
		else
		{
			m_aDailyCopies.Insert(today);
			m_aDailySummaries.Insert(summary);
		}
		int keep = Math.MaxInt(m_iDailyKeep, 1);
		while (m_aDailyCopies.Count() > keep)
		{
			string oldPath = DailyPath(m_aDailyCopies[0]);
			if (FileIO.FileExists(oldPath))
				FileIO.DeleteFile(oldPath);
			m_aDailyCopies.RemoveOrdered(0);
			if (!m_aDailySummaries.IsEmpty())
				m_aDailySummaries.RemoveOrdered(0);
		}
		MarkDirty(false);	// la liste à jour part avec la prochaine écriture
	}

	//------------------------------------------------------------------------------------------------
	//! front.json illisible au démarrage : la liste des copies datées (d'habitude relue dans front.json) est refaite
	//! d'après les fichiers front_AAAA-MM-JJ.json de DAILY_DIR, sans résumé ; le Staff peut ainsi en recharger une —
	//! Start
	protected void RebuildDailyList()
	{
		array<string> found = {};
		FileIO.FindFiles(found.Insert, DAILY_DIR, ".json");
		array<string> days = {};
		foreach (string foundPath : found)
		{
			// « front_2026-09-25.json » : 6 + 10 + 5 caractères
			string fileName = FilePath.StripPath(foundPath);
			if (fileName.Length() != 21 || !fileName.StartsWith("front_") || !fileName.EndsWith(".json"))
				continue;
			string day = fileName.Substring(6, 10);
			if (days.Contains(day) || !FileIO.FileExists(DailyPath(day)))
				continue;
			days.Insert(day);
		}
		m_aDailyCopies.Clear();
		m_aDailySummaries.Clear();
		MergeDailyDays(days);
		Note("FRONT", string.Format("Front : %1 copie(s) datée(s) retrouvée(s) dans %2", m_aDailyCopies.Count(), DAILY_DIR));
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute des jours à la liste des copies datées (résumé vide pour un jour nouveau), sans doublon, puis la remet
	//! dans l'ordre, du plus ancien au plus récent (AAAA-MM-JJ se trie comme du texte) — RebuildDailyList, RestoreDaily
	protected void MergeDailyDays(notnull array<string> extraDays)
	{
		map<string, string> summaryOf = new map<string, string>();
		for (int i = 0; i < m_aDailyCopies.Count(); i++)
		{
			string known = m_aDailyCopies[i];
			if (known.IsEmpty() || summaryOf.Contains(known))
				continue;
			string summary = "";
			if (i < m_aDailySummaries.Count())
				summary = m_aDailySummaries[i];
			summaryOf.Set(known, summary);
		}
		foreach (string extra : extraDays)
		{
			if (!extra.IsEmpty() && !summaryOf.Contains(extra))
				summaryOf.Set(extra, "");
		}

		array<string> ordered = {};
		for (int k = 0; k < summaryOf.Count(); k++)
		{
			ordered.Insert(summaryOf.GetKey(k));
		}
		ordered.Sort();
		m_aDailyCopies.Clear();
		m_aDailySummaries.Clear();
		foreach (string day : ordered)
		{
			m_aDailyCopies.Insert(day);
			m_aDailySummaries.Insert(summaryOf.Get(day));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Date du jour du serveur, AAAA-MM-JJ (System.GetYearMonthDay) — DailyCopy, archives
	protected string Today()
	{
		int year, month, day;
		System.GetYearMonthDay(year, month, day);
		if (year <= 0)
			return "";
		return string.Format("%1-%2-%3", year, SRP_Time.Pad2(month), SRP_Time.Pad2(day));
	}

	//================================================================================================
	// Interne : migration K1 et K2 (code du module migration, trou 18)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! K1 : sans front.json et avec territoire.json : lecture (count, sN_name, sN_owner, sN_stock), copie vers
	//! ARCHIVE_DIR/territoire_campagne1_<date>.json (arrêt si la copie échoue), campagne 2, restes K2 notés, Save,
	//! puis DeleteFile(OLD_PATH) ; journal TERRITOIRE public sans mot confidentiel — Start
	protected bool ImportLegacy()
	{
		JsonLoadContext legacy = new JsonLoadContext();
		if (!legacy.LoadFromFile(OLD_PATH))
		{
			Note("ERREUR", "Migration : territoire.json illisible, laissé en place, rien n'est versé (supprimer front.json pour réessayer)");
			return false;
		}

		// Ancien format : count, puis sN_name, sN_owner (ENNEMI 0, NOUS 1), sN_stock
		int count = 0;
		legacy.ReadValue("count", count);
		int held = 0;
		int vivres = 0;
		int munitions = 0;
		array<string> unknown = {};
		for (int i = 0; i < count; i++)
		{
			string prefix = "s" + i.ToString() + "_";
			string oldName;
			int oldOwner = 0;
			int oldStock = 0;
			legacy.ReadValue(prefix + "name", oldName);
			legacy.ReadValue(prefix + "owner", oldOwner);
			legacy.ReadValue(prefix + "stock", oldStock);
			if (oldOwner != 1)
				continue;
			held++;
			if (oldStock <= 0)
				continue;
			int kind = LegacyKind(oldName);
			if (kind == 1)
			{
				munitions += oldStock;	// hameau (ancien poste) : munitions
			}
			else
			{
				vivres += oldStock;		// village, bourg, ville : vivres
				if (kind < 0)
					unknown.Insert(oldName);
			}
		}
		if (!unknown.IsEmpty())
			Note("FRONT", "Migration : localité(s) introuvable(s), paquets comptés en vivres : " + string.Join(", ", unknown, true));

		// Archive d'abord : sans elle, on s'arrête (rien versé, rien effacé)
		FileIO.MakeDirectory(ARCHIVE_DIR);
		string fileName = "territoire_campagne1_" + Stamp() + ".json";
		string archive = ARCHIVE_DIR + "/" + fileName;
		if (!FileIO.CopyFile(OLD_PATH, archive))
		{
			Note("ERREUR", "Migration : archive impossible vers " + archive + " ; territoire.json laissé en place, rien n'est versé (supprimer front.json pour réessayer)");
			return false;
		}

		m_iCampaign = 2;
		m_bMigrated = true;
		m_iLegacyVivres = 0;
		m_iLegacyMunitions = 0;
		m_iLegacyNextTry = 0;
		if (m_bLegacyStockToBase)
		{
			m_iLegacyVivres = vivres;
			m_iLegacyMunitions = munitions;
		}
		m_sLegacyInfo = string.Format("campagne 1 archivée (%1 localités, %2 prises) ; production en attente : %3 vivres et %4 munitions, versés aux dépôts de la base", count, held, vivres, munitions);
		m_bLoaded = true;

		// Nouvelle sauvegarde AVANT l'effacement : jamais deux migrations, jamais deux versements (K2)
		if (!Save())
		{
			Note("ERREUR", "Migration : front.json n'a pas pu être écrit ; territoire.json est gardé");
			return false;
		}
		FileIO.DeleteFile(OLD_PATH);
		SRP_FrontRadio.LegacyImported(m_sLegacyInfo);
		SRP_FrontRadio.NewCampaign(m_iCampaign);	// évènement « campagne » (§6 : B3, K1, Staff), radio et Discord
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! K2 : attend SRP_ResourceManagerComponent.IsRestockDone (ou m_iLegacyPayTimeoutMinutes), met les restes à 0 et
	//! SAUVE D'ABORD, verse par Add(ressource, reste, …), réessaie le non-rangé toutes les m_iLegacyRetryMinutes —
	//! Housekeeping
	protected void MigrationTick(int nowUnix)
	{
		if (!m_bLoaded || (m_iLegacyVivres <= 0 && m_iLegacyMunitions <= 0))
			return;
		if (nowUnix < m_iLegacyNextTry)
			return;
		SRP_ResourceManagerComponent resources = SRP_ResourceManagerComponent.GetInstance();
		if (!resources)
			return;
		bool timeout = nowUnix - m_iStartUnix >= Math.MaxInt(m_iLegacyPayTimeoutMinutes, 0) * 60;
		if (!resources.IsRestockDone() && !timeout)
			return;	// le regarnissage des dépôts absorberait le versement

		// Restes à zéro et écrits D'ABORD : un arrêt en plein versement ne verse jamais deux fois
		int wantVivres = Math.MaxInt(m_iLegacyVivres, 0);
		int wantMunitions = Math.MaxInt(m_iLegacyMunitions, 0);
		m_iLegacyVivres = 0;
		m_iLegacyMunitions = 0;
		if (!Save())
		{
			m_iLegacyVivres = wantVivres;
			m_iLegacyMunitions = wantMunitions;
			m_iLegacyNextTry = nowUnix + 60;
			Note("ERREUR", "Migration : versement des anciens paquets reporté, front.json n'a pas pu être écrit d'abord");
			return;
		}

		int gotVivres = 0;
		int gotMunitions = 0;
		if (wantVivres > 0)
			gotVivres = resources.Add(SRP_EResource.VIVRES, wantVivres, "campagne 1 : production en attente");
		if (wantMunitions > 0)
			gotMunitions = resources.Add(SRP_EResource.MUNITIONS, wantMunitions, "campagne 1 : production en attente");
		if (gotVivres > 0 || gotMunitions > 0)
			SRP_FrontRadio.LegacyPoured(gotVivres, gotMunitions);

		// Dépôts pleins : le reste attend et sera réessayé
		m_iLegacyVivres = Math.MaxInt(wantVivres - gotVivres, 0);
		m_iLegacyMunitions = Math.MaxInt(wantMunitions - gotMunitions, 0);
		if (m_iLegacyVivres > 0 || m_iLegacyMunitions > 0)
		{
			int retry = Math.MaxInt(m_iLegacyRetryMinutes, 1);
			m_iLegacyNextTry = nowUnix + retry * 60;
			Note("FRONT", string.Format("Migration : %1 vivres et %2 munitions non rangés (dépôts pleins), nouvel essai dans %3 min", m_iLegacyVivres, m_iLegacyMunitions, retry));
		}
		MarkDirty(true);
	}

	//================================================================================================
	// Interne : outils du socle (ajouts, non contractuels)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Adopte les tableaux du bâtisseur de géométrie et prépare les tableaux par carré (tous propriétaires à l'état de
	//! départ : base bleue, reste rouge) ; faux si la géométrie est incohérente — Start
	protected bool AdoptGeometry(SRP_FrontGeometryBuilder builder)
	{
		if (!builder || builder.m_iN <= 0 || builder.m_iN > 256)
			return false;
		int count = builder.m_iN * builder.m_iN;
		if (!builder.m_aCellZone || builder.m_aCellZone.Count() != count)
			return false;
		if (!builder.m_aZones || builder.m_aZones.IsEmpty() || builder.m_aZones.Count() > 126)
			return false;

		m_iN = builder.m_iN;
		m_fGridCell = m_fCellSize;
		m_iGeomHash = builder.m_iHash;
		m_aCellZone = builder.m_aCellZone;
		m_aZones = builder.m_aZones;
		if (builder.m_aCellLand)
			m_aCellLand = builder.m_aCellLand;
		if (builder.m_aSeaLink)
			m_aSeaLink = builder.m_aSeaLink;
		if (builder.m_aLocalities)
			m_aLocalities = builder.m_aLocalities;
		if (builder.m_aKeyPoints)
			m_aKeyPoints = builder.m_aKeyPoints;
		if (builder.m_aWarnings)
			m_aGeomWarnings = builder.m_aWarnings;
		m_iMissingKeyPoints = builder.m_iMissingKeyPoints;

		// Garde-fous de taille (un tableau manquant ne doit jamais faire planter une lecture)
		if (m_aCellLand.Count() != count)
		{
			m_aCellLand.Clear();
			m_aCellLand.Resize(count);
			for (int land = 0; land < count; land++)
			{
				if (m_aCellZone[land] < 0)
					m_aCellLand[land] = -1;
			}
		}
		if (m_aSeaLink.Count() != count)
		{
			m_aSeaLink.Clear();
			m_aSeaLink.Resize(count);
			for (int link = 0; link < count; link++)
			{
				m_aSeaLink[link] = -1;
			}
		}
		int zoneCount = m_aZones.Count();
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aCellZone[cell] >= zoneCount)
				m_aCellZone[cell] = -1;
		}

		// Zones : index, base, B4 (réglage m_bShieldBaseNeighbours)
		bool baseMarked = false;
		if (m_aZones[0] && m_aZones[0].m_bBase)
			baseMarked = true;
		for (int i = 0; i < zoneCount; i++)
		{
			SRP_FrontZone z = m_aZones[i];
			if (!z)
			{
				z = new SRP_FrontZone();
				m_aZones[i] = z;
			}
			z.m_iIndex = i;
			if (i == 0)
				z.m_bBase = true;
			if (!m_bShieldBaseNeighbours)
				z.m_bShield = false;
		}
		if (!baseMarked)
			Note("FRONT", "Géométrie : la zone 0 n'était pas marquée comme base, elle est traitée comme telle");

		// Tableaux par carré et état de départ
		m_aCellOwner.Clear();
		m_aCellOwner.Resize(count);
		m_aCellLive.Clear();
		m_aCellLive.Resize(count);
		m_aLinkStamp.Clear();
		m_aLinkStamp.Resize(count);
		m_iStamp = 0;
		m_iLandCells = 0;
		for (int c = 0; c < count; c++)
		{
			int zone = m_aCellZone[c];
			if (zone < 0)
			{
				m_aCellOwner[c] = HORS_JEU;
				continue;
			}
			m_iLandCells++;
			if (zone == 0)
				m_aCellOwner[c] = SRP_EFrontOwner.BLEU;
			else
				m_aCellOwner[c] = SRP_EFrontOwner.ROUGE;
		}
		m_aZones[0].m_iOwner = SRP_EFrontOwner.BLEU;
		m_aCutReminded.Clear();
		m_aCutReminded.Resize(zoneCount);
		RecountBlue();
		m_iOwnerStamp++;
		m_bTopologyDirty = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Lecture des clés du socle, sans la chaîne de l'ennemi. restore = retour à une copie datée : chaque coupure E4
	//! reprend avec le temps qu'il lui restait dans la copie. keepGlobal = la liste des copies, la migration, le gel, la
	//! campagne, la version du pont et le compteur de restaurations sont GARDÉS (retour ordinaire ; faux au démarrage
	//! et pour la réparation d'un front.json illisible) — ReadFrom, RestoreDaily
	protected bool ReadCore(JsonLoadContext ctx, bool restore, bool keepGlobal)
	{
		int now = System.GetUnixTime();
		int format = 0;
		ctx.ReadValue("format", format);
		if (format > FORMAT)
			Note("FRONT", string.Format("front.json vient d'une version plus récente du mod (format %1) : lecture au mieux", format));
		int savedUnix = 0;
		bool fileFrozen = false;
		int fileFrozenSince = 0;
		ctx.ReadValue("savedUnix", savedUnix);
		ctx.ReadValue("frozen", fileFrozen);
		ctx.ReadValue("frozenSince", fileFrozenSince);

		// État global
		if (!keepGlobal)
		{
			int campaign = m_iCampaign;
			ctx.ReadValue("campaign", campaign);
			m_iCampaign = Math.MaxInt(campaign, 1);
			ctx.ReadValue("campaignStart", m_sCampaignStart);
			m_bFrozen = fileFrozen;
			m_iFrozenSince = 0;
			if (fileFrozen)
			{
				m_iFrozenSince = fileFrozenSince;
				if (m_iFrozenSince <= 0)
					m_iFrozenSince = now;
			}
			int bridge = 0;
			ctx.ReadValue("bridgeVersion", bridge);
			m_iBridgeVersion = Math.MaxInt(bridge, m_iBridgeVersion);
			ctx.ReadValue("restores", m_iRestoreCount);
		}
		int threat = 0;
		ctx.ReadValue("threat", threat);
		m_iThreat = Math.ClampInt(threat, 0, 10);
		m_bVictory = false;
		m_sVictoryAt = "";
		m_iVictoryUnix = 0;
		ctx.ReadValue("victory", m_bVictory);
		ctx.ReadValue("victoryAt", m_sVictoryAt);
		ctx.ReadValue("victoryUnix", m_iVictoryUnix);
		if (m_bVictory && m_iVictoryUnix <= 0)
			m_iVictoryUnix = now;

		// Géométrie de la sauvegarde
		int grid = 0;
		int geom = 0;
		string cells;
		ctx.ReadValue("grid", grid);
		ctx.ReadValue("geom", geom);
		ctx.ReadValue("cells", cells);
		if (geom != 0 && geom != m_iGeomHash)
			Note("FRONT", "Front : les zones ont changé depuis la sauvegarde (somme de contrôle différente) ; zones relues par leur code, vérifier le rapport " + REPORT_PATH);
		int count = m_iN * m_iN;
		bool cellsOk = grid == m_iN && cells.Length() == count;
		if (!cellsOk)
			Note("ERREUR", string.Format("Front : grille de la sauvegarde différente (%1 carrés de côté contre %2) ; les carrés sont refaits d'après les zones", grid, m_iN));

		// Zones, par CODE (les zones absentes du fichier repartent de l'état de départ)
		map<string, int> byCode = new map<string, int>();
		foreach (int index, SRP_FrontZone known : m_aZones)
		{
			if (!known)
				continue;
			ResetZoneState(known, true);
			byCode.Set(known.m_sCode, index);
		}
		m_aOrphanZones.Clear();
		m_aOrphanData.Clear();
		int zoneEntries = 0;
		ctx.ReadValue("zones", zoneEntries);
		for (int n = 0; n < zoneEntries; n++)
		{
			string prefix = "z" + n.ToString() + "_";
			string code;
			string last;
			int owner = SRP_EFrontOwner.ROUGE;
			int captured = 0;
			int lost = 0;
			int taken = 0;
			int cut = 0;
			int stock = 0;
			bool depot = false;
			float dx = 0;
			float dy = 0;
			float dz = 0;
			ctx.ReadValue(prefix + "code", code);
			ctx.ReadValue(prefix + "owner", owner);
			ctx.ReadValue(prefix + "captured", captured);
			ctx.ReadValue(prefix + "lost", lost);
			ctx.ReadValue(prefix + "taken", taken);
			ctx.ReadValue(prefix + "cut", cut);
			ctx.ReadValue(prefix + "last", last);
			ctx.ReadValue(prefix + "stock", stock);
			ctx.ReadValue(prefix + "depot", depot);
			ctx.ReadValue(prefix + "dx", dx);
			ctx.ReadValue(prefix + "dy", dy);
			ctx.ReadValue(prefix + "dz", dz);
			if (code.IsEmpty())
				continue;

			int zone = -1;
			if (!byCode.Find(code, zone) || !ValidZone(zone))
			{
				// Code inconnu : gardé de côté tel quel (réécrit à chaque sauvegarde) et signalé
				if (!m_aOrphanZones.Contains(code))
				{
					m_aOrphanZones.Insert(code);
					m_aOrphanData.Insert(OrphanData(owner, captured, lost, taken, cut, stock, depot, Vector(dx, dy, dz), last));
				}
				continue;
			}

			// E4 : une copie datée reprend avec le temps de coupure qu'il lui restait
			if (restore && cut > 0 && savedUnix > 0)
			{
				int copyReference = savedUnix;
				if (fileFrozen && fileFrozenSince > 0)
					copyReference = fileFrozenSince;
				int elapsed = Math.MaxInt(copyReference - cut, 0);
				int clockUnix = now;
				if (m_bFrozen && m_iFrozenSince > 0)
					clockUnix = m_iFrozenSince;
				cut = Math.MaxInt(clockUnix - elapsed, 1);
			}

			SRP_FrontZone z = m_aZones[zone];
			z.m_iOwner = SRP_EFrontOwner.ROUGE;
			if (owner == SRP_EFrontOwner.BLEU)
				z.m_iOwner = SRP_EFrontOwner.BLEU;
			z.m_iCapturedAt = captured;
			z.m_iLostAt = lost;
			z.m_iTakenCount = Math.MaxInt(taken, 0);
			z.m_iCutSince = 0;
			if (z.m_iOwner == SRP_EFrontOwner.BLEU && cut > 0)
			{
				z.m_iCutSince = cut;
				z.m_iFlags = FLAG_CUT;
			}
			z.m_sLastChange = last;
			z.m_iStock = Math.MaxInt(stock, 0);
			z.m_bDepot = depot;
			z.m_vDepot = Vector(dx, dy, dz);
			z.m_iDepotCell = -1;
			if (depot)
				z.m_iDepotCell = CellIndexAt(z.m_vDepot);
		}
		if (!m_aOrphanZones.IsEmpty())
			Note("FRONT", "Front : zone(s) de la sauvegarde sans correspondance, gardées de côté : " + string.Join(", ", m_aOrphanZones, true));
		if (m_aZones.Count() > 0 && m_aZones[0])
		{
			m_aZones[0].m_iOwner = SRP_EFrontOwner.BLEU;
			m_aZones[0].m_iCutSince = 0;
			m_aZones[0].m_iFlags = 0;
		}

		// Carrés : par index ; carrés de base toujours bleus ; grille différente : d'après le propriétaire de la zone
		for (int cell = 0; cell < count; cell++)
		{
			int cellZone = m_aCellZone[cell];
			if (cellZone < 0)
			{
				m_aCellOwner[cell] = HORS_JEU;
				continue;
			}
			if (cellZone == 0)
			{
				m_aCellOwner[cell] = SRP_EFrontOwner.BLEU;
				continue;
			}
			if (cellsOk)
			{
				if (cells.Get(cell) == "B")
					m_aCellOwner[cell] = SRP_EFrontOwner.BLEU;
				else
					m_aCellOwner[cell] = SRP_EFrontOwner.ROUGE;
			}
			else
			{
				m_aCellOwner[cell] = m_aZones[cellZone].m_iOwner;
			}
		}

		// Localités, par NOM (F12 et champs du Commandeur)
		foreach (SRP_FrontLocality reset : m_aLocalities)
		{
			if (reset)
				ResetLocalityState(reset);
		}
		int locCount = 0;
		int missingLocalities = 0;
		ctx.ReadValue("loc", locCount);
		for (int l = 0; l < locCount; l++)
		{
			string lp = "l" + l.ToString() + "_";
			string locName;
			ctx.ReadValue(lp + "name", locName);
			int locIndex = FindLocality(locName);
			if (locIndex < 0)
			{
				if (!locName.IsEmpty())
					missingLocalities++;
				continue;
			}
			SRP_FrontLocality locality = m_aLocalities[locIndex];
			ctx.ReadValue(lp + "losses", locality.m_iLosses);
			ctx.ReadValue(lp + "lossesAt", locality.m_iLossesAt);
			ctx.ReadValue(lp + "sent", locality.m_iSent);
			ctx.ReadValue(lp + "sentAt", locality.m_iSentAt);
			ctx.ReadValue(lp + "bonus", locality.m_iBonus);
			ctx.ReadValue(lp + "bonusAt", locality.m_iBonusAt);
			ctx.ReadValue(lp + "emptied", locality.m_iEmptiedAt);
			ctx.ReadValue(lp + "full", locality.m_iFullUntil);
		}
		if (missingLocalities > 0)
			Note("FRONT", string.Format("Front : %1 localité(s) de la sauvegarde introuvable(s) sur la carte (mémoire des pertes ignorée)", missingLocalities));

		// Copies datées et migration : au démarrage et pour une réparation (un retour ordinaire les garde)
		if (!keepGlobal)
		{
			m_aDailyCopies.Clear();
			m_aDailySummaries.Clear();
			int dailyCount = 0;
			ctx.ReadValue("daily", dailyCount);
			for (int d = 0; d < dailyCount; d++)
			{
				string dayKey;
				string daySummary;
				ctx.ReadValue("d" + d.ToString(), dayKey);
				ctx.ReadValue("d" + d.ToString() + "_s", daySummary);
				if (dayKey.IsEmpty() || m_aDailyCopies.Contains(dayKey))
					continue;
				m_aDailyCopies.Insert(dayKey);
				m_aDailySummaries.Insert(daySummary);
			}
			ctx.ReadValue("lastDaily", m_sLastDaily);
			ctx.ReadValue("migrated", m_bMigrated);
			ctx.ReadValue("legacyVivres", m_iLegacyVivres);
			ctx.ReadValue("legacyMunitions", m_iLegacyMunitions);
			ctx.ReadValue("legacyNext", m_iLegacyNextTry);
			ctx.ReadValue("legacyInfo", m_sLegacyInfo);
		}

		// Extras des autres modules (repris aussi par une restauration)
		m_mExtra.Clear();
		int extraCount = 0;
		ctx.ReadValue("extra", extraCount);
		for (int x = 0; x < extraCount; x++)
		{
			string extraKey;
			string extraValue;
			ctx.ReadValue("x" + x.ToString() + "_k", extraKey);
			ctx.ReadValue("x" + x.ToString() + "_v", extraValue);
			if (!extraKey.IsEmpty())
				m_mExtra.Set(extraKey, extraValue);
		}

		m_aPendingCells.Clear();
		m_aCutReminded.Clear();
		m_aCutReminded.Resize(m_aZones.Count());
		RecountBlue();
		m_iOwnerStamp++;
		m_bTopologyDirty = true;
		m_bStatePending = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone à l'état de départ : rouge (la base bleue), sans date, sans coupure ni drapeau ; economy = stock et dépôt
	//! vidés aussi (les caisses et repères vivants restent à SRP_FrontEconomy) — NewCampaignState, ReadCore
	protected void ResetZoneState(SRP_FrontZone z, bool economy)
	{
		if (!z)
			return;
		z.m_iOwner = SRP_EFrontOwner.ROUGE;
		if (z.m_iIndex == 0 || z.m_bBase)
			z.m_iOwner = SRP_EFrontOwner.BLEU;
		z.m_iFlags = 0;
		z.m_iCapturedAt = 0;
		z.m_iLostAt = 0;
		z.m_iTakenCount = 0;
		z.m_iCutSince = 0;
		z.m_sLastChange = "";
		if (!economy)
			return;
		z.m_iStock = 0;
		z.m_bDepot = false;
		z.m_vDepot = vector.Zero;
		z.m_iDepotCell = -1;
		z.m_iProductionSec = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité sans pertes ni envois (F12, MA2, MA6, MA7, MA11) — NewCampaignState, ReadCore
	protected void ResetLocalityState(SRP_FrontLocality locality)
	{
		locality.m_iLosses = 0;
		locality.m_iLossesAt = 0;
		locality.m_iSent = 0;
		locality.m_iSentAt = 0;
		locality.m_iBonus = 0;
		locality.m_iBonusAt = 0;
		locality.m_iEmptiedAt = 0;
		locality.m_iFullUntil = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Recompte les carrés bleus de chaque zone — après tout état appliqué d'un coup
	protected void RecountBlue()
	{
		int zoneCount = m_aZones.Count();
		foreach (SRP_FrontZone z : m_aZones)
		{
			if (z)
				z.m_iBlueCells = 0;
		}
		int count = Math.MinInt(m_aCellZone.Count(), m_aCellOwner.Count());
		for (int cell = 0; cell < count; cell++)
		{
			int zone = m_aCellZone[cell];
			if (zone < 0 || zone >= zoneCount || m_aCellOwner[cell] != SRP_EFrontOwner.BLEU)
				continue;
			SRP_FrontZone owner = m_aZones[zone];
			if (owner)
				owner.m_iBlueCells = owner.m_iBlueCells + 1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Raison du refus d'un changement de carré, "" s'il est permis (règles de SetCellOwner) — SetCellsOwner
	protected string CellRefusal(int cell, int owner, int reason)
	{
		if (!IsCellInPlay(cell) || cell >= m_aCellOwner.Count())
			return "carré hors jeu";
		int zone = m_aCellZone[cell];
		if (zone == 0)
			return "carré de la base";
		if (m_aCellOwner[cell] == owner)
			return "carré déjà " + OwnerWord(owner);
		if (m_bFrozen && !IsCorrectionReason(reason))
			return "front gelé";
		if (owner == SRP_EFrontOwner.ROUGE && reason != SRP_EFrontReason.STAFF && IsZoneProtected(zone) && GetZoneOwner(zone) == SRP_EFrontOwner.BLEU)
			return "zone voisine de la base déjà à nous (B4)";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Causes de correction, permises pendant le gel : STAFF, REMISE, RESTAURATION
	protected bool IsCorrectionReason(int reason)
	{
		return reason == SRP_EFrontReason.STAFF || reason == SRP_EFrontReason.REMISE || reason == SRP_EFrontReason.RESTAURATION;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone d'index valide
	protected bool ValidZone(int zone)
	{
		return zone >= 0 && zone < m_aZones.Count() && m_aZones[zone] != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Le k-ième point clé de la zone, null hors bornes
	protected SRP_FrontKeyPoint ZoneKeyPoint(int zone, int k)
	{
		return GetKeyPoint(GetKeyPointIndex(zone, k));
	}

	//------------------------------------------------------------------------------------------------
	//! Source d'un point clé pour la page Staff (Q8) : le repère posé au Workbench, sinon le repli sur le nom de la carte
	//! (FICHIER n'est plus produit : la ligne « point » de front_retouches.txt est refusée par le bâtisseur)
	protected string KeyPointSourceText(SRP_FrontKeyPoint point)
	{
		if (!point)
			return "MANQUANT";
		if (point.m_iSource == SRP_EKeyPointSource.REPERE)
			return point.m_sSourceText;
		return "MANQUANT (endroit du nom sur la carte, repère à poser au Workbench)";
	}

	//------------------------------------------------------------------------------------------------
	//! Points clés posés au Workbench (source REPERE)
	protected int CountPlacedKeyPoints()
	{
		int count = 0;
		foreach (SRP_FrontKeyPoint point : m_aKeyPoints)
		{
			if (point && point.m_iSource == SRP_EKeyPointSource.REPERE)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Localités de zone (hors celles de la base)
	protected int CountFrontLocalities()
	{
		int count = 0;
		foreach (SRP_FrontLocality locality : m_aLocalities)
		{
			if (locality && !locality.m_bHome)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : sauvegarde demandée « aussitôt » (MarkDirty(true)), faite à la fin de l'image
	protected void SaveQueued()
	{
		m_bSaveQueued = false;
		if (m_bDirty && m_bLoaded)
			Save();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : écriture en attente, aussitôt si demandée, sinon au plus m_iSaveDelaySeconds après le changement — Tick
	protected void SaveIfDue(int nowUnix)
	{
		if (!m_bDirty || !m_bLoaded)
			return;
		if (m_bSaveNow || nowUnix - m_iDirtySince >= Math.MaxInt(m_iSaveDelaySeconds, 1))
			Save();
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : envoie les états contestés en attente tout de suite, ou dès que m_iLiveSendMinMs est écoulé
	protected void TrySendLive()
	{
		if (m_aLiveCells.IsEmpty() || m_bLiveQueued)
			return;
		int elapsed = System.GetTickCount() - m_iLastLiveSend;
		int wait = Math.MaxInt(m_iLiveSendMinMs, 0);
		if (m_iLastLiveSend == 0 || elapsed >= wait || elapsed < 0)
		{
			SendLive();
			return;
		}
		m_bLiveQueued = true;
		GetGame().GetCallqueue().CallLater(SendLive, wait - elapsed, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : RpcDo_FrontLive avec le DERNIER état de chaque carré noté (le dernier état part toujours)
	protected void SendLive()
	{
		m_bLiveQueued = false;
		if (!Replication.IsServer() || !m_bReady || m_aLiveCells.IsEmpty())
			return;
		array<int> live = {};
		foreach (int cell : m_aLiveCells)
		{
			if (cell >= 0 && cell < m_aCellLive.Count())
				live.Insert((cell << 2) | (m_aCellLive[cell] & 3));
		}
		m_aLiveCells.Clear();
		m_iLastLiveSend = System.GetTickCount();
		if (!live.IsEmpty())
			Rpc(RpcDo_FrontLive, live);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire la victoire enregistrée (une zone est repassée rouge ou remise à zéro)
	protected void ClearVictory()
	{
		if (!m_bVictory)
			return;
		m_bVictory = false;
		m_sVictoryAt = "";
		m_iVictoryUnix = 0;
		Note("FRONT", "Victoire retirée : une zone n'est plus à nous (le gel éventuel reste, à lever au menu Staff)");
	}

	//------------------------------------------------------------------------------------------------
	//! Données d'une zone orpheline, « | » entre les champs — ReadCore
	protected string OrphanData(int owner, int captured, int lost, int taken, int cut, int stock, bool depot, vector depotPos, string last)
	{
		int depotFlag = 0;
		if (depot)
			depotFlag = 1;
		string text = string.Format("%1|%2|%3|%4|%5|%6|%7", owner, captured, lost, taken, cut, stock, depotFlag);
		float depotX = depotPos[0];
		float depotY = depotPos[1];
		float depotZ = depotPos[2];
		text += "|" + depotX.ToString() + "|" + depotY.ToString() + "|" + depotZ.ToString() + "|" + last;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Réécrit une zone orpheline telle qu'elle a été lue — WriteTo
	protected void WriteOrphanZone(JsonSaveContext ctx, string prefix, string code, string data)
	{
		array<string> parts = {};
		data.Split("|", parts, false);
		if (parts.Count() < 11)
			return;
		ctx.WriteValue(prefix + "code", code);
		ctx.WriteValue(prefix + "owner", parts[0].ToInt());
		ctx.WriteValue(prefix + "captured", parts[1].ToInt());
		ctx.WriteValue(prefix + "lost", parts[2].ToInt());
		ctx.WriteValue(prefix + "taken", parts[3].ToInt());
		ctx.WriteValue(prefix + "cut", parts[4].ToInt());
		ctx.WriteValue(prefix + "stock", parts[5].ToInt());
		bool depot = parts[6].ToInt() != 0;
		ctx.WriteValue(prefix + "depot", depot);
		ctx.WriteValue(prefix + "dx", parts[7].ToFloat());
		ctx.WriteValue(prefix + "dy", parts[8].ToFloat());
		ctx.WriteValue(prefix + "dz", parts[9].ToFloat());
		ctx.WriteValue(prefix + "last", parts[10]);
	}

	//------------------------------------------------------------------------------------------------
	//! K2 : genre d'une ancienne localité : 1 hameau (munitions), 0 autre (vivres), -1 introuvable — ImportLegacy
	protected int LegacyKind(string name)
	{
		if (name.IsEmpty())
			return -1;
		int index = FindLocality(name);
		if (index >= 0)
		{
			SRP_FrontLocality locality = m_aLocalities[index];
			if (locality.m_iKind == SRP_EZoneKind.HAMEAU || locality.m_sKind == "hameau")
				return 1;
			return 0;
		}
		// Localité de la base ou hors registre : la carte elle-même (même source que l'ancien territoire)
		foreach (SRP_CivilZoneComponent civil : SRP_CivilZoneComponent.GetZones())
		{
			if (!civil || civil.GetPlaceName() != name)
				continue;
			if (civil.GetPlaceKind() == "hameau")
				return 1;
			return 0;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Chemin d'une copie datée
	protected string DailyPath(string day)
	{
		return DAILY_DIR + "/front_" + day + ".json";
	}

	//------------------------------------------------------------------------------------------------
	//! Horodatage pour un nom de fichier : « 2026-09-26_21-14-03 »
	protected string Stamp()
	{
		string stamp = SRP_Time.Now();
		stamp.Replace(":", "-");
		stamp.Replace(" ", "_");
		return stamp;
	}

	//------------------------------------------------------------------------------------------------
	//! « 25/09 21:14 » (dernier changement d'une zone)
	protected string NowShort()
	{
		int year, month, day;
		int hour, minute, second;
		System.GetYearMonthDay(year, month, day);
		System.GetHourMinuteSecond(hour, minute, second);
		return string.Format("%1/%2 %3:%4", SRP_Time.Pad2(day), SRP_Time.Pad2(month), SRP_Time.Pad2(hour), SRP_Time.Pad2(minute));
	}

	//------------------------------------------------------------------------------------------------
	//! Trois chiffres, comme SRP_Fleet.c (« 074 »)
	protected static string Pad3(int value)
	{
		if (value < 0)
			value = 0;
		string text = value.ToString();
		while (text.Length() < 3)
		{
			text = "0" + text;
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte de recherche : sans espaces autour ni en double, en minuscules
	protected static string NormText(string text)
	{
		string result = text.Trim();
		result.ToLower();
		while (result.Contains("  "))
		{
			result.Replace("  ", " ");
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! « bleu » ou « rouge »
	protected static string OwnerWord(int owner)
	{
		if (owner == SRP_EFrontOwner.BLEU)
			return "bleu";
		return "rouge";
	}

	//------------------------------------------------------------------------------------------------
	//! Nom d'une cause (SRP_EFrontReason) pour le journal
	protected static string ReasonName(int reason)
	{
		if (reason == SRP_EFrontReason.CAPTURE)
			return "capture";
		if (reason == SRP_EFrontReason.SAISIE)
			return "saisie";
		if (reason == SRP_EFrontReason.REPRISE)
			return "reprise";
		if (reason == SRP_EFrontReason.ZONE_TOMBEE)
			return "zone tombée";
		if (reason == SRP_EFrontReason.ZONE_PERDUE)
			return "zone perdue";
		if (reason == SRP_EFrontReason.ISOLEMENT)
			return "isolement";
		if (reason == SRP_EFrontReason.OFFENSIVE)
			return "offensive";
		if (reason == SRP_EFrontReason.STAFF)
			return "Staff";
		if (reason == SRP_EFrontReason.REMISE)
			return "remise";
		if (reason == SRP_EFrontReason.RESTAURATION)
			return "restauration";
		return "inconnue";
	}

	//------------------------------------------------------------------------------------------------
	//! « , par Jack » pour le journal ("" sans auteur)
	protected static string AuthorSuffix(string author)
	{
		if (author.IsEmpty())
			return "";
		return ", par " + author;
	}

	//------------------------------------------------------------------------------------------------
	//! Coordonnée envoyée aux joueurs : mètres entiers, jamais négatifs
	protected static int NetCoord(float value)
	{
		int meters = Math.Round(value);
		return Math.ClampInt(meters, 0, NET_MAX);
	}

	//------------------------------------------------------------------------------------------------
	//! Point au sol d'après x et z (joueurs : centres, noms, points clés reçus) ; (0, 0) = pas de position, rendu
	//! vector.Zero comme sur le serveur (le calque ne nomme pas une zone dont GetZoneLabelPos est vector.Zero)
	protected static vector GroundPoint(int x, int z)
	{
		if (x == 0 && z == 0)
			return vector.Zero;
		float fx = x;
		float fz = z;
		float y = 0;
		BaseWorld world = GetGame().GetWorld();
		if (world)
			y = world.GetSurfaceY(fx, fz);
		return Vector(fx, y, fz);
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit un tableau d'entiers : nombre, puis chaque valeur sur 28 bits (0 à NET_MAX) — RplSave
	protected static void WriteInts(ScriptBitWriter writer, notnull array<int> values)
	{
		writer.WriteInt(values.Count());
		foreach (int value : values)
		{
			writer.WriteIntRange(Math.ClampInt(value, 0, NET_MAX), 0, NET_MAX);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Relit un tableau écrit par WriteInts ; faux si la taille dépasse maxCount ou si la lecture échoue — RplLoad
	protected static bool ReadInts(ScriptBitReader reader, notnull array<int> values, int maxCount)
	{
		values.Clear();
		int count;
		if (!reader.ReadInt(count))
			return false;
		if (count < 0 || count > maxCount)
			return false;
		int value;
		for (int i = 0; i < count; i++)
		{
			if (!reader.ReadIntRange(value, 0, NET_MAX))
				return false;
			values.Insert(value);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Journal SimpleRP (serveur)
	protected static void Note(string category, string text)
	{
		SRP_JournalComponent.Log(category, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : prévient les officiers connectés (front.json illisible)
	protected static void NotifyOfficers(string text)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			players.NotifyOfficiers(text);
	}
}
