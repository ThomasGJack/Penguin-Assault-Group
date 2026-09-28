//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : données et construction de la géométrie (socle).
//
// RÔLE :
// - SRP_FrontZone : une zone (index 0 = la base), présente sur toutes les machines (répliquée par le socle). Ses
//   champs d'état serveur (dates, stock, dépôt, ressource) sont sauvegardés par le socle dans front.json (trou 16).
// - SRP_FrontLocality : une localité de zone (serveur), lue dans SRP_CivilZoneComponent (trou 24) ; porte la
//   mémoire des pertes F12 et les champs du Commandeur (envoyés, reçus, vidée, tenue au complet), sauvegardés.
// - SRP_FrontKeyPoint : une entrée du registre UNIQUE des points clés (trou 13, arbitrage Q8). Pris = carré bleu.
// - SRP_FrontPack : empaquetage des petits entiers pour le réseau (28 bits utiles par entier, jamais le signe).
// - SRP_FrontGeometryBuilder : construction déterministe de la grille et des zones au démarrage du serveur
//   (A1 à A6, B2, B4, Q3, Q4, Q5, Q8), lecture de front_retouches.txt, rapport front_zones.txt.
//   Algorithme de référence : blocs.py du plan (66 zones sur Everon, dont 30 avec localité) : terre ou mer par
//   GetSurfaceY, îlots vides retirés (Erquy gardée), blocs de 1 km coupés par la mer ou la base, blocs à deux
//   localités coupés (Q3), petits morceaux sans localité versés dans le voisin qui partage le plus de côtés (la base
//   comprise, comme blocs.py), numérotation S01… par distance entre la base et le bloc.
//   Sorties : m_aLocalities ne contient QUE les localités de zone (celles de la base servent aux lieux-dits) ;
//   m_aZones[0] est la base (code m_sCodePrefix + « 00 »).
// APPELÉ PAR : SRP_FrontComponent (Start, lecture) ; tout le mod lit les champs publics des zones et localités.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Une zone du front. Champs publics en lecture pour tout le mod ; seul le socle (et SRP_ZoneFall par le socle)
//! les écrit. Les accesseurs Get*/Is* ne font que lire les champs (contrat du pont, mod_web).
class SRP_FrontZone
{
	// Identité (géométrie, toutes machines)
	int m_iIndex;							// 0 = la base, puis 1..Z (numéro du code)
	string m_sCode;							// « S07 », clé stable
	string m_sName;							// « Régina », localité ou lieu-dit, « Secteur » à défaut
	int m_iKind = SRP_EZoneKind.AUCUNE;		// genre de la plus grosse localité (SRP_EZoneKind)
	int m_iLocality = -1;					// localité principale (index socle), -1 sans localité
	ref array<int> m_aLocalities = {};		// toutes ses localités, principale d'abord (serveur)
	ref array<int> m_aCells = {};			// carrés de terre
	ref array<int> m_aNeighbours = {};		// zones voisines par un côté ou par la liaison maritime (Q5)
	ref array<int> m_aKeyPoints = {};		// index des points clés dans le registre du socle
	vector m_vCentroid;						// centre de gravité des carrés (y au sol)
	vector m_vAnchor;						// centre du bloc ou centre de gravité (numérotation A6)
	vector m_vLabelPos;						// où écrire le nom sur la carte
	bool m_bBase;							// B2
	bool m_bShield;							// B4 : touche la base par un côté
	bool m_bIsland;							// île secondaire (Erquy)

	// État (toutes machines pour m_iOwner et m_iFlags ; le reste serveur)
	int m_iOwner = SRP_EFrontOwner.ROUGE;
	int m_iFlags;							// FLAG_ATTACK_ANNOUNCED | FLAG_ATTACK | FLAG_CUT
	int m_iBlueCells;						// carrés bleus, tenu à jour par le socle
	bool m_bInContact;						// E1 (zone rouge au contact), tenu par UpdateTopology
	bool m_bAtFront;						// F1 (zone bleue au front), tenu par UpdateTopology
	bool m_bLinked = true;					// E4 : reliée à la base

	// Serveur, SAUVEGARDÉ par le socle (heures Unix, 0 = rien)
	int m_iCapturedAt;
	int m_iLostAt;
	int m_iTakenCount;						// numéro de prise, F2
	int m_iCutSince;						// E4 : début de la coupure, 0 = reliée
	string m_sLastChange;					// « 25/09 21:14 » pour le Staff

	// Serveur, économie (G4 à G7), données SAUVEGARDÉES par le socle, logique dans SRP_FrontEconomy
	int m_iResource = -1;					// SRP_EResource produite, -1 = rien (G4, G5)
	int m_iRate = -1;						// paquets par heure, -1 = débit par défaut de la ressource
	int m_iStock;							// paquets en attente (sauvegardé)
	bool m_bDepot;							// dépôt installé (sauvegardé)
	vector m_vDepot;						// position du dépôt (sauvegardée)
	int m_iDepotCell = -1;					// carré du dépôt, recalculé
	int m_iProductionSec;					// secondes de production accumulées (non sauvegardé)
	IEntity m_Box;							// caisse de production (vivante)
	ref SCR_MapMarkerBase m_DepotMarker;	// repère du dépôt (vivant)

	//------------------------------------------------------------------------------------------------
	//! Code stable « S07 » — pont (mod_web)
	string GetCode()
	{
		return m_sCode;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom seul « Régina » — pont
	string GetName()
	{
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé « Régina (S07) » (A6), la base garde son nom seul ; même texte que SRP_FrontComponent.GetZoneLabel
	//! (nom vide = « Secteur », code vide = nom seul) — pont, radio, écrans
	string GetLabel()
	{
		string name = m_sName;
		if (name.IsEmpty())
			name = SRP_FrontGeometryBuilder.DEFAULT_ZONE_NAME;
		if (m_bBase || m_iIndex == 0 || m_sCode.IsEmpty())
			return name;
		return name + " (" + m_sCode + ")";
	}

	//------------------------------------------------------------------------------------------------
	//! Genre en clair : « hameau », « village », « ville » ou « lieu-dit » — pont (genre)
	string GetKind()
	{
		if (m_iKind == SRP_EZoneKind.VILLE)
			return "ville";
		if (m_iKind == SRP_EZoneKind.VILLAGE)
			return "village";
		if (m_iKind == SRP_EZoneKind.HAMEAU)
			return "hameau";
		return "lieu-dit";
	}

	//------------------------------------------------------------------------------------------------
	//! Zone à nous (BLEU) — pont (o)
	bool IsOurs()
	{
		return m_iOwner == SRP_EFrontOwner.BLEU;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EFrontAttack d'après m_iFlags — pont (a)
	int GetAttackState()
	{
		// L'assaut l'emporte sur l'annonce si les deux drapeaux sont posés
		if ((m_iFlags & SRP_FrontComponent.FLAG_ATTACK) != 0)
			return SRP_EFrontAttack.ASSAUT;
		if ((m_iFlags & SRP_FrontComponent.FLAG_ATTACK_ANNOUNCED) != 0)
			return SRP_EFrontAttack.ANNONCEE;
		return SRP_EFrontAttack.AUCUNE;
	}

	//------------------------------------------------------------------------------------------------
	//! PRÈS du front au sens large (E1 zone rouge au contact OU F1 zone bleue au front), le filtre I3 — pont (clé f),
	//! PC. Ne PAS confondre avec SRP_FrontComponent.IsZoneAtFront (F1 seul, zone bleue qui touche du rouge)
	bool IsNearFront()
	{
		return m_bInContact || m_bAtFront;
	}

	//------------------------------------------------------------------------------------------------
	//! Coupée de la base (E4) — pont (k)
	bool IsCut()
	{
		return m_iCutSince > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Imprenable par l'ennemi (B2, B4) — pont (p)
	bool IsProtected()
	{
		return m_bBase || m_bShield;
	}

	//------------------------------------------------------------------------------------------------
	//! Dépôt installé (#49) — pont (dp)
	bool HasDepot()
	{
		return m_bDepot;
	}

	//------------------------------------------------------------------------------------------------
	//! Position du dépôt — pont (dp)
	vector GetDepotPosition()
	{
		return m_vDepot;
	}

	//------------------------------------------------------------------------------------------------
	//! Paquets en attente — pont (st), boxes
	int GetStock()
	{
		return m_iStock;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EResource produite, -1 = rien — pont (boxes)
	int GetResource()
	{
		return m_iResource;
	}
}

//------------------------------------------------------------------------------------------------
//! Une localité de zone (serveur). Les champs « sauvegardés » sont écrits dans front.json par le socle (clés lN_*).
class SRP_FrontLocality
{
	string m_sName;							// nom traduit de la carte (clé)
	string m_sKind;							// « hameau », « village » ou « ville » (un bourg compte comme une ville)
	int m_iKind = SRP_EZoneKind.VILLAGE;	// même chose en SRP_EZoneKind
	vector m_vLabel;						// centre de la zone civile (SRP_CivilZoneComponent.GetCenter)
	bool m_bHome;							// localité de la base (ignorée par le front)
	int m_iZone = -1;						// zone de son CENTRE
	int m_iCentre = -1;						// point clé CENTRE (index du registre)
	int m_iQg = -1;							// point clé QG (index du registre), -1 hors ville

	// F12, sauvegardés (heures Unix)
	int m_iLosses;
	int m_iLossesAt;

	// Commandeur (RE13, MA2, MA6, MA7, MA11), sauvegardés (heures Unix, 0 = rien)
	int m_iSent;							// soldats envoyés en renfort (MA2)
	int m_iSentAt;
	int m_iBonus;							// renforts reçus
	int m_iBonusAt;
	int m_iEmptiedAt;						// localité vidée par un décrochage (MA7)
	int m_iFullUntil;						// tenue au complet jusqu'à (MA11)
}

//------------------------------------------------------------------------------------------------
//! Un point clé du registre UNIQUE (trou 13). Pas de m_bTaken : un point est pris quand son carré est bleu.
class SRP_FrontKeyPoint
{
	int m_iIndex;							// place dans le registre
	int m_iLocality = -1;					// localité (index socle)
	int m_iRole = SRP_EKeyPointRole.CENTRE;
	vector m_vPos;							// position au sol
	float m_fYaw;							// orientation du repère (poste de commandement)
	int m_iCell = -1;
	int m_iZone = -1;
	int m_iSource = SRP_EKeyPointSource.NOM_CARTE;
	string m_sSourceText;					// « repère SRP_PointCle_12 » ou « nom de la carte »
}

//------------------------------------------------------------------------------------------------
//! Empaquetage des petits entiers pour le réseau : 28 bits utiles par entier, jamais le bit de signe ;
//! écriture word = word | ((v & mask) << shift) (jamais |=) — appelé par le socle (réseau, RplSave)
class SRP_FrontPack
{
	static const int WORD_BITS = 28;			// bits utiles par entier (WriteIntRange 0..268435455)

	//------------------------------------------------------------------------------------------------
	//! values (0 à 2^bits - 1) vers packed, 28 / bits valeurs par entier — BuildGeometryArrays, BuildStateArrays
	static void Pack(notnull array<int> values, int bits, notnull array<int> packed)
	{
		packed.Clear();
		if (bits < 1 || bits > WORD_BITS)
			return;

		int perWord = WORD_BITS / bits;
		int mask = (1 << bits) - 1;
		int word = 0;
		int slot = 0;
		int count = values.Count();
		for (int i = 0; i < count; i++)
		{
			// Une valeur négative (-1 = hors jeu) devient mask (127 sur 7 bits)
			word = word | ((values[i] & mask) << (slot * bits));
			slot++;
			if (slot >= perWord)
			{
				packed.Insert(word);
				word = 0;
				slot = 0;
			}
		}
		if (slot > 0)
			packed.Insert(word);
	}

	//------------------------------------------------------------------------------------------------
	//! packed vers count valeurs de bits bits — ApplyGeometry, ApplyState
	static void Unpack(notnull array<int> packed, int bits, int count, notnull array<int> values)
	{
		values.Clear();
		if (bits < 1 || bits > WORD_BITS || count <= 0)
			return;

		int perWord = WORD_BITS / bits;
		int mask = (1 << bits) - 1;
		int words = packed.Count();
		for (int i = 0; i < count; i++)
		{
			int wordIndex = i / perWord;
			if (wordIndex >= words)
			{
				values.Insert(0);	// paquet trop court : le socle refuse les tailles aberrantes avant d'appeler
				continue;
			}
			int shift = (i - wordIndex * perWord) * bits;
			values.Insert((packed[wordIndex] >> shift) & mask);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Choix de position d'un point clé pendant la construction (serveur, interne au bâtisseur)
class SRP_FrontPointChoice
{
	bool m_bFound;								// une source a été retenue
	vector m_vPos;
	float m_fYaw;
	int m_iCell = -1;							// carré en jeu hors base retenu (anneaux 1 à 5 si besoin)
	int m_iSource = SRP_EKeyPointSource.NOM_CARTE;
	string m_sText;								// « repère SRP_PointCle_12 » ou « nom de la carte »
}

//------------------------------------------------------------------------------------------------
//! Construction de la géométrie (serveur seulement), déterministe : mêmes entrées, mêmes zones, mêmes codes.
//! Le socle crée le bâtisseur, appelle Build, puis adopte ses tableaux publics de sortie.
class SRP_FrontGeometryBuilder
{
	static const int PIECE_BASE = -2;			// numéro de morceau réservé à la base (B2)
	static const int MAX_GRID = 256;			// garde du réseau (RplLoad : N <= 256)
	static const int MAX_ZONES = 126;			// entrées de zone, base comprise (7 bits par carré, 127 = hors jeu)
	static const int RING_MAX = 5;				// anneaux de recherche d'un carré en jeu pour un point clé
	static const float DUPLICATE_DISTANCE = 300;	// deux localités plus proches sont un doublon (comme SRP_Territory)
	static const string BASE_FALLBACK_NAME = "Levie";	// repli de la base sans marqueur (B2)
	static const string DEFAULT_ZONE_NAME = "Secteur";	// zone sans localité ni lieu-dit (A6)

	protected SRP_FrontComponent m_Front;		// réglages (attributs publics) du socle

	// Sorties, adoptées par le socle
	int m_iN;									// côté de la grille (64)
	int m_iHash;								// somme de contrôle (h x 131 + zone + 2) modulo 8388593
	ref array<int> m_aCellZone = {};			// -1 hors jeu
	ref array<int> m_aCellLand = {};			// numéro de terre, -1 mer
	ref array<int> m_aSeaLink = {};			// -1 sinon
	ref array<ref SRP_FrontZone> m_aZones = {};
	ref array<ref SRP_FrontLocality> m_aLocalities = {};
	ref array<ref SRP_FrontKeyPoint> m_aKeyPoints = {};
	ref array<string> m_aReport = {};			// lignes du rapport front_zones.txt
	ref array<string> m_aWarnings = {};			// avertissements (aussi au journal TERRITOIRE)
	int m_iMissingKeyPoints;					// points clés sans repère : centre sur le nom de la carte, QG absent (Q8)

	// Travail
	protected vector m_vBase;					// position de la base (marqueur, sinon « Levie »)
	protected ref array<int> m_aPiece = {};	// morceau de bloc de chaque carré
	protected ref array<int> m_aQueue = {};	// file des parcours en largeur
	protected ref array<string> m_aRetouchLines = {};	// lignes utiles de front_retouches.txt

	protected float m_fCell;							// taille d'un carré (m_fCellSize)
	protected int m_iBlock;								// côté d'un bloc en carrés (m_iBlockCells)
	protected ref array<int> m_aLandMask = {};			// 1 terre, 0 mer, avant le tri des îlots
	protected ref array<int> m_aPieceSeed = {};			// carré de départ de chaque morceau
	protected ref array<int> m_aPieceFirst = {};		// plus petit morceau versé dans celui-ci (ancre A6, comme blocs.py)
	protected ref array<int> m_aRetouchLineNo = {};		// numéro de ligne de chaque ligne utile
	protected ref array<string> m_aRawName = {};		// localités lues (zone et base), dans l'ordre de lecture
	protected ref array<string> m_aRawKind = {};		// « ville », « bourg », « village », « hameau »
	protected ref array<vector> m_aRawPos = {};
	protected ref array<string> m_aHomeName = {};		// localités de la base (lieux-dits possibles, A6)
	protected ref array<vector> m_aHomePos = {};
	protected ref array<string> m_aPlaceName = {};		// lieux-dits candidats (A6)
	protected ref array<vector> m_aPlacePos = {};
	protected bool m_bPlacesRead;
	protected ref array<string> m_aNotes = {};			// avertissements hors retouches (rapport)
	protected ref array<string> m_aRefused = {};		// retouches refusées (rapport)
	protected ref array<string> m_aApplied = {};		// retouches appliquées (rapport)
	protected ref array<string> m_aSplits = {};			// blocs coupés entre deux localités (rapport, Q3)
	protected int m_iSampleMs;							// durée de l'échantillonnage terre ou mer
	protected int m_iSamplePoints;						// points testés
	protected int m_iLandCells;							// carrés en jeu (terre gardée)
	protected int m_iDroppedIslets;						// îlots sans localité retirés
	protected int m_iDroppedCells;
	protected int m_iIslands;							// îles secondaires gardées (numéros de terre 1..)
	protected int m_iBaseRadiusCells;					// carrés de base dans le rayon (B2)
	protected int m_iMergedIntoBase;					// carrés de petits morceaux versés dans la base (A4)
	protected int m_iMergedPieces;						// morceaux fusionnés (A4)
	protected int m_iPieceCount;						// morceaux de blocs avant coupure et fusion
	protected int m_iCivilLocalities;					// localités lues dans les zones civiles
	protected int m_iMapLocalities;						// localités complétées depuis la carte

	//------------------------------------------------------------------------------------------------
	//! Le bâtisseur lit les réglages du socle — SRP_FrontComponent.Start
	void SRP_FrontGeometryBuilder(SRP_FrontComponent front)
	{
		m_Front = front;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les étapes dans l'ordre (1 à 16 de la consigne) ; faux sur erreur fatale (carte absente, grille impossible,
	//! aucune terre, base introuvable, trop de zones) ; carte sans taille : repli m_fWorldSize — SRP_FrontComponent.Start
	bool Build()
	{
		ResetAll();
		if (!m_Front)
		{
			Print("[SRP] Front : bâtisseur de géométrie sans composant de front", LogLevel.ERROR);
			return false;
		}

		// 1 et 2 : grille, terre ou mer
		if (!BuildGrid())
			return false;
		SampleLand();
		if (m_iLandCells == 0)
		{
			Fail("aucun carré de terre sur la carte : front impossible");
			return false;
		}

		// 4 puis 5 : les localités d'abord (repli de la base sur « Levie », îles qui portent une localité)
		ReadLocalities();
		if (!FindBase())
			return false;

		// 3, puis 6 à 10 : îlots, base, blocs, coupure, fusion, numérotation
		KeepIslands();
		BuildPieces();
		NumberZones();
		if (m_aZones.Count() > MAX_ZONES)
		{
			Fail(string.Format("%1 zones : au-delà de %2, le réseau ne peut plus les coder (augmenter m_iBlockCells ou m_iZoneMinCells)", m_aZones.Count() - 1, MAX_ZONES - 1));
			return false;
		}
		UpdateZoneCells();

		// Noms d'avant les retouches (« 074 042 -> Régina » vise le nom d'avant)
		AssignLocalitiesByName();
		NameZones();

		// 11 : retouches du fichier
		ReadRetouches();
		ApplyMoves();
		UpdateZoneCells();
		ApplyResources();

		// 12 à 15 : points clés, noms définitifs, liaisons maritimes, drapeaux
		PlaceKeyPoints();
		NameZones();
		BuildSeaLinks();
		ComputeFlags();

		// 16 : lignes du rapport (écrites par WriteReport)
		BuildReport();
		Print(string.Format("[SRP] Front : géométrie construite : %1 carrés en jeu, %2 zones, base %3 carrés, %4 localités, %5 points clés (%6 manquants)", m_iLandCells, m_aZones.Count() - 1, m_aZones[0].m_aCells.Count(), m_aLocalities.Count(), m_aKeyPoints.Count(), m_iMissingKeyPoints), LogLevel.NORMAL);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Remet toutes les sorties et le travail à zéro (un bâtisseur peut resservir) — Build
	protected void ResetAll()
	{
		m_iN = 0;
		m_iHash = 0;
		m_iMissingKeyPoints = 0;
		m_aCellZone.Clear();
		m_aCellLand.Clear();
		m_aSeaLink.Clear();
		m_aZones.Clear();
		m_aLocalities.Clear();
		m_aKeyPoints.Clear();
		m_aReport.Clear();
		m_aWarnings.Clear();
		m_aPiece.Clear();
		m_aQueue.Clear();
		m_aRetouchLines.Clear();
		m_aRetouchLineNo.Clear();
		m_aLandMask.Clear();
		m_aPieceSeed.Clear();
		m_aPieceFirst.Clear();
		m_aRawName.Clear();
		m_aRawKind.Clear();
		m_aRawPos.Clear();
		m_aHomeName.Clear();
		m_aHomePos.Clear();
		m_aPlaceName.Clear();
		m_aPlacePos.Clear();
		m_bPlacesRead = false;
		m_aNotes.Clear();
		m_aRefused.Clear();
		m_aApplied.Clear();
		m_aSplits.Clear();
		m_iSampleMs = 0;
		m_iSamplePoints = 0;
		m_iLandCells = 0;
		m_iDroppedIslets = 0;
		m_iDroppedCells = 0;
		m_iIslands = 0;
		m_iBaseRadiusCells = 0;
		m_iMergedIntoBase = 0;
		m_iMergedPieces = 0;
		m_iPieceCount = 0;
		m_iCivilLocalities = 0;
		m_iMapLocalities = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! 1. N = ceil(taille de la carte / taille du carré) (SCR_MapEntity.GetMapSizeX, repli m_fWorldSize si la carte
	//! n'a pas de taille) ; faux sans carte — Build
	protected bool BuildGrid()
	{
		m_fCell = m_Front.m_fCellSize;
		if (m_fCell < 10)
		{
			Fail(string.Format("taille de carré invalide (%1 m)", m_fCell));
			return false;
		}
		m_iBlock = m_Front.m_iBlockCells;
		if (m_iBlock < 1)
			m_iBlock = 1;

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (!mapEntity)
		{
			// Contrat (Build : faux sur « carte absente ») : sans carte, ni localités ni lieux-dits ; une géométrie construite
			// quand même serait adoptée par le socle et écraserait front.json (codes décalés, mémoire des localités perdue)
			Fail("aucune carte (SCR_MapEntity) dans le monde : front impossible");
			return false;
		}
		// Carte présente mais sans taille : repli sur m_fWorldSize
		float size = mapEntity.GetMapSizeX();
		if (size <= 0)
		{
			size = m_Front.m_fWorldSize;
			int defaultMeters = Math.Round(size);
			Warn(string.Format("taille de la carte inconnue : %1 m par défaut (m_fWorldSize)", defaultMeters));
		}
		// Petite marge : 12800 / 200 doit donner 64, jamais 65 par un arrondi de calcul
		m_iN = Math.Ceil(size / m_fCell - 0.001);
		if (m_iN < 1 || m_iN > MAX_GRID)
		{
			Fail(string.Format("grille de %1 carrés de côté impossible (1 à %2)", m_iN, MAX_GRID));
			return false;
		}

		int count = m_iN * m_iN;
		FillInts(m_aCellZone, count, -1);
		FillInts(m_aCellLand, count, -1);
		FillInts(m_aSeaLink, count, -1);
		FillInts(m_aPiece, count, -1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 2. Terre ou mer (A2) : m_iLandSamples² points par carré, GetSurfaceY > GetOceanBaseHeight + m_fLandMinHeight,
	//! part >= m_fLandMinShare ; durée notée au journal — Build
	protected void SampleLand()
	{
		int count = m_iN * m_iN;
		FillInts(m_aLandMask, count, 0);
		m_iLandCells = 0;

		BaseWorld world = GetGame().GetWorld();
		if (!world || !world.IsOcean())
		{
			// Monde sans océan (BaseWorld.IsOcean) : tout est terre
			Warn("le monde n'a pas d'océan : toute la grille est de la terre");
			for (int all = 0; all < count; all++)
				m_aLandMask[all] = 1;
			m_iLandCells = count;
			return;
		}

		int samples = m_Front.m_iLandSamples;
		if (samples < 1)
			samples = 1;
		float step = m_fCell / samples;
		float total = samples * samples;
		// Même comparaison que SCR_CampaignBuildingPlacingObstructionEditorComponent.c:419
		float seaLevel = world.GetOceanBaseHeight() + m_Front.m_fLandMinHeight;
		int started = System.GetTickCount();

		for (int cell = 0; cell < count; cell++)
		{
			// Colonne et ligne calculées à part : dans un calcul à virgule, Enforce passe tout en float (% refusé, / non entier)
			int col = cell % m_iN;
			int row = cell / m_iN;
			float x0 = col * m_fCell;
			float z0 = row * m_fCell;
			int landPoints = 0;
			// Points au centre de sous-carrés (50 m pour 4 x 4 sur 200 m), comme le masque de blocs.py
			for (int sz = 0; sz < samples; sz++)
			{
				float sampleZ = z0 + (sz + 0.5) * step;
				for (int sx = 0; sx < samples; sx++)
				{
					if (world.GetSurfaceY(x0 + (sx + 0.5) * step, sampleZ) > seaLevel)
						landPoints++;
				}
			}
			if (landPoints / total >= m_Front.m_fLandMinShare)
			{
				m_aLandMask[cell] = 1;
				m_iLandCells++;
			}
		}

		m_iSampleMs = System.GetTickCount() - started;
		m_iSamplePoints = count * samples * samples;
		Print(string.Format("[SRP] Front : terre ou mer, %1 points testés en %2 ms, %3 carrés de terre", m_iSamplePoints, m_iSampleMs, m_iLandCells), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! 3. Îlots : composantes 4-connexes ; garde la plus grande et celles qui portent une localité (Erquy) — Build
	protected void KeepIslands()
	{
		int count = m_iN * m_iN;
		array<int> component = {};
		FillInts(component, count, -1);
		array<int> componentSize = {};

		// Composantes numérotées dans l'ordre des carrés (gz x N + gx), parcours en largeur
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aLandMask[cell] == 0 || component[cell] >= 0)
				continue;
			int id = componentSize.Count();
			component[cell] = id;
			m_aQueue.Clear();
			m_aQueue.Insert(cell);
			int head = 0;
			while (head < m_aQueue.Count())
			{
				int at = m_aQueue[head];
				head++;
				for (int dir = 0; dir < 4; dir++)
				{
					int nb = NeighbourOf(at, dir);
					if (nb < 0 || m_aLandMask[nb] == 0 || component[nb] >= 0)
						continue;
					component[nb] = id;
					m_aQueue.Insert(nb);
				}
			}
			componentSize.Insert(m_aQueue.Count());
		}

		// La grande île (la première à égalité)
		int mainLand = -1;
		for (int c = 0; c < componentSize.Count(); c++)
		{
			if (mainLand < 0 || componentSize[c] > componentSize[mainLand])
				mainLand = c;
		}

		// Numéro de terre par composante : 0 la grande île, 1.. les îles gardées, -1 retirée (devient de la mer)
		array<int> landOf = {};
		FillInts(landOf, componentSize.Count(), -1);
		if (mainLand >= 0)
			landOf[mainLand] = 0;
		if (m_Front.m_bKeepIslandsWithLocality)
		{
			foreach (vector place : m_aRawPos)
			{
				int placeCell = CellOf(place);
				if (placeCell < 0)
					continue;
				int placeComponent = component[placeCell];
				if (placeComponent >= 0 && landOf[placeComponent] < 0)
					landOf[placeComponent] = 1;		// gardée, numérotée juste après
			}
		}
		int nextLand = 1;
		for (int k = 0; k < componentSize.Count(); k++)
		{
			if (k == mainLand)
				continue;
			if (landOf[k] < 0)
			{
				m_iDroppedIslets++;
				continue;
			}
			landOf[k] = nextLand;
			nextLand++;
		}
		m_iIslands = nextLand - 1;

		m_iLandCells = 0;
		m_iDroppedCells = 0;
		for (int cellIndex = 0; cellIndex < count; cellIndex++)
		{
			int comp = component[cellIndex];
			if (comp < 0)
			{
				m_aCellLand[cellIndex] = -1;
				continue;
			}
			m_aCellLand[cellIndex] = landOf[comp];
			if (landOf[comp] >= 0)
				m_iLandCells++;
			else
				m_iDroppedCells++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 4. Localités de SRP_CivilZoneComponent.GetZones (GetPlaceKind, GetPlaceName, GetCenter), m_bHome à moins de
	//! m_fBaseRadius de la base (trou 24) — Build
	protected void ReadLocalities()
	{
		m_aRawName.Clear();
		m_aRawKind.Clear();
		m_aRawPos.Clear();
		m_iCivilLocalities = 0;
		m_iMapLocalities = 0;

		// Source unique des garnisons (trou 24) : les zones civiles automatiques, nées des noms de la carte
		array<SRP_CivilZoneComponent> civils = SRP_CivilZoneComponent.GetZones();
		if (civils)
		{
			foreach (SRP_CivilZoneComponent civil : civils)
			{
				if (!civil || !civil.GetOwner())
					continue;
				string civilKind = civil.GetPlaceKind();
				if (civilKind.IsEmpty())
					continue;	// zone posée à la main pour les civils, pas un lieu (comme SRP_Territory.AddAutoSectors)
				if (AddRawLocality(civil.GetPlaceName(), civilKind, civil.GetCenter()))
					m_iCivilLocalities++;
			}
		}

		// Complément par la carte, mêmes noms et mêmes positions que SRP_Civilians.CreateAutoZonesOfType : les zones
		// civiles ne sont peut-être pas encore créées (même délai de 5 s), ou une zone posée à la main a pris la place
		// d'une localité ; aucune localité ne doit manquer au front
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			bool warnEach = m_iCivilLocalities > 0;
			m_iMapLocalities += AddMapLocalities(mapEntity, EMapDescriptorType.MDT_NAME_CITY, "ville", warnEach);
			m_iMapLocalities += AddMapLocalities(mapEntity, EMapDescriptorType.MDT_NAME_TOWN, "bourg", warnEach);
			m_iMapLocalities += AddMapLocalities(mapEntity, EMapDescriptorType.MDT_NAME_VILLAGE, "village", warnEach);
			m_iMapLocalities += AddMapLocalities(mapEntity, EMapDescriptorType.MDT_NAME_SETTLEMENT, "hameau", warnEach);
		}
		if (m_iCivilLocalities == 0 && m_iMapLocalities > 0)
			Warn(string.Format("aucune zone civile de localité au démarrage : les %1 localités sont lues sur la carte (mêmes noms, mêmes positions)", m_iMapLocalities));
		if (m_aRawName.IsEmpty())
			Warn("aucune localité trouvée : toutes les zones seront sans localité");
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute les noms de la carte d'un type (ville, bourg, village, hameau) absents de la liste ; rend le nombre — ReadLocalities
	protected int AddMapLocalities(SCR_MapEntity mapEntity, int descriptorType, string kind, bool warnEach)
	{
		array<MapItem> items = {};
		mapEntity.GetByType(items, descriptorType);
		int added = 0;
		foreach (MapItem item : items)
		{
			if (!item)
				continue;
			IEntity location = item.Entity();
			if (!location)
				continue;
			string name = WidgetManager.Translate(item.GetDisplayName());
			if (!AddRawLocality(name, kind, location.GetOrigin()))
				continue;
			added++;
			if (warnEach)
				Warn(name + " : pas de zone civile, localité lue sur la carte");
		}
		return added;
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute une localité lue ; refusée si le nom est vide ou si elle double une localité déjà lue (même nom ou
	//! moins de 300 m, comme SRP_Territory.AddAutoSectors) — ReadLocalities
	protected bool AddRawLocality(string name, string kind, vector pos)
	{
		string clean = name.Trim();
		if (clean.IsEmpty())
			return false;
		for (int i = 0; i < m_aRawName.Count(); i++)
		{
			if (m_aRawName[i] == clean || vector.DistanceXZ(m_aRawPos[i], pos) < DUPLICATE_DISTANCE)
				return false;
		}
		m_aRawName.Insert(clean);
		m_aRawKind.Insert(kind);
		m_aRawPos.Insert(pos);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 5. Base (B2) : marqueur m_sBaseMarkerName (repli : localité « Levie »), carrés à m_fBaseRadius au plus = zone 0
	protected bool FindBase()
	{
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_Front.m_sBaseMarkerName);
		if (marker)
		{
			m_vBase = marker.GetOrigin();
		}
		else
		{
			int fallback = m_aRawName.Find(BASE_FALLBACK_NAME);
			if (fallback < 0)
			{
				Fail(string.Format("ni marqueur « %1 » ni localité « %2 » : la base est introuvable, front impossible", m_Front.m_sBaseMarkerName, BASE_FALLBACK_NAME));
				return false;
			}
			m_vBase = m_aRawPos[fallback];
			Warn(string.Format("marqueur « %1 » introuvable : base centrée sur le nom « %2 »", m_Front.m_sBaseMarkerName, BASE_FALLBACK_NAME));
		}

		// Localités de la base (même règle que SRP_Territory.c:606) et localités de zone
		m_aLocalities.Clear();
		m_aHomeName.Clear();
		m_aHomePos.Clear();
		for (int i = 0; i < m_aRawName.Count(); i++)
		{
			if (vector.DistanceXZ(m_aRawPos[i], m_vBase) < m_Front.m_fBaseRadius)
			{
				m_aHomeName.Insert(m_aRawName[i]);
				m_aHomePos.Insert(m_aRawPos[i]);
				continue;
			}
			SRP_FrontLocality locality = new SRP_FrontLocality();
			locality.m_sName = m_aRawName[i];
			locality.m_iKind = KindOf(m_aRawKind[i]);
			locality.m_sKind = KindText(locality.m_iKind);
			locality.m_vLabel = m_aRawPos[i];
			locality.m_bHome = false;
			m_aLocalities.Insert(locality);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! 6 à 9. Îles en une zone (Q5), blocs de m_iBlockCells (A3), coupure des blocs à deux localités (Q3), fusion des
	//! petits morceaux sans localité (A4) — Build
	protected void BuildPieces()
	{
		int count = m_iN * m_iN;

		// Base (B2) : carrés en jeu dont le centre est à m_fBaseRadius au plus du marqueur
		FillInts(m_aPiece, count, -1);
		m_iBaseRadiusCells = 0;
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aCellLand[cell] < 0)
				continue;
			if (vector.DistanceXZ(CellCenter(cell), m_vBase) <= m_Front.m_fBaseRadius)
			{
				m_aPiece[cell] = PIECE_BASE;
				m_iBaseRadiusCells++;
			}
		}

		CutBlocks();
		m_iPieceCount = m_aPieceSeed.Count();
		SplitMultiLocality();
		MergeSmallPieces();
	}

	//------------------------------------------------------------------------------------------------
	//! 6 et 7. Morceaux : composantes 4-connexes d'un bloc hors base ; une île secondaire forme un seul morceau.
	//! Départs dans l'ordre de blocs.py (colonne gx, puis rangée gz) : les numéros servent aux départages — BuildPieces
	protected void CutBlocks()
	{
		m_aPieceSeed.Clear();
		m_aPieceFirst.Clear();
		for (int gx = 0; gx < m_iN; gx++)
		{
			for (int gz = 0; gz < m_iN; gz++)
			{
				int seed = gz * m_iN + gx;
				if (m_aCellLand[seed] < 0 || m_aPiece[seed] != -1)
					continue;

				int piece = m_aPieceSeed.Count();
				m_aPieceSeed.Insert(seed);
				m_aPieceFirst.Insert(piece);
				m_aPiece[seed] = piece;
				bool wholeIsland = m_Front.m_bIslandSingleZone && m_aCellLand[seed] != 0;
				int blockX = gx / m_iBlock;
				int blockZ = gz / m_iBlock;

				m_aQueue.Clear();
				m_aQueue.Insert(seed);
				int head = 0;
				while (head < m_aQueue.Count())
				{
					int at = m_aQueue[head];
					head++;
					for (int dir = 0; dir < 4; dir++)
					{
						int nb = NeighbourOf(at, dir);
						if (nb < 0 || m_aCellLand[nb] < 0 || m_aPiece[nb] != -1)
							continue;
						if (!wholeIsland && ((nb % m_iN) / m_iBlock != blockX || (nb / m_iN) / m_iBlock != blockZ))
							continue;
						m_aPiece[nb] = piece;
						m_aQueue.Insert(nb);
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 8. Coupure des blocs à deux localités (Q3, m_bSplitMultiLocality) : chaque carré va à la localité dont le nom
	//! est le plus proche de son centre — BuildPieces
	protected void SplitMultiLocality()
	{
		if (!m_Front.m_bSplitMultiLocality)
			return;

		array<int> nameCells = {};
		foreach (SRP_FrontLocality locality : m_aLocalities)
			nameCells.Insert(CellOf(locality.m_vLabel));

		int original = m_aPieceSeed.Count();
		for (int piece = 0; piece < original; piece++)
		{
			if (m_Front.m_bIslandSingleZone && m_aCellLand[m_aPieceSeed[piece]] != 0)
				continue;	// une île reste une seule zone (Q5)
			array<int> inside = {};
			for (int li = 0; li < nameCells.Count(); li++)
			{
				int nameCell = nameCells[li];
				if (nameCell >= 0 && m_aPiece[nameCell] == piece)
					inside.Insert(li);
			}
			if (inside.Count() >= 2)
				SplitPiece(piece, inside, nameCells);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Coupe un morceau entre ses localités (liste dans l'ordre des localités) : la partie du carré de départ garde le
	//! numéro, les autres en prennent un neuf ; à égalité de distance, la première localité — SplitMultiLocality
	protected void SplitPiece(int piece, notnull array<int> inside, notnull array<int> nameCells)
	{
		int count = m_iN * m_iN;
		array<int> nearest = {};
		FillInts(nearest, count, -1);
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aPiece[cell] != piece)
				continue;
			vector center = CellCenter(cell);
			int best = inside[0];
			float bestDistance = vector.DistanceXZ(center, m_aLocalities[best].m_vLabel);
			for (int k = 1; k < inside.Count(); k++)
			{
				float distance = vector.DistanceXZ(center, m_aLocalities[inside[k]].m_vLabel);
				if (distance < bestDistance)
				{
					best = inside[k];
					bestDistance = distance;
				}
			}
			nearest[cell] = best;
		}

		int kept = nearest[m_aPieceSeed[piece]];
		array<int> labels = {};				// numéro de morceau de chaque localité de inside, -1 si partie vide
		string names = "";
		foreach (int li : inside)
		{
			if (!names.IsEmpty())
				names += " / ";
			names += m_aLocalities[li].m_sName;
			if (li == kept)
			{
				labels.Insert(piece);
				continue;
			}
			int firstCell = -1;
			for (int c2 = 0; c2 < count; c2++)
			{
				if (nearest[c2] == li)
				{
					firstCell = c2;
					break;
				}
			}
			if (firstCell < 0)
			{
				labels.Insert(-1);
				continue;
			}
			int newPiece = m_aPieceSeed.Count();
			labels.Insert(newPiece);
			m_aPieceFirst.Insert(newPiece);
			m_aPieceSeed.Insert(firstCell);
		}

		for (int c3 = 0; c3 < count; c3++)
		{
			if (nearest[c3] < 0)
				continue;
			m_aPiece[c3] = labels[inside.Find(nearest[c3])];
		}

		// Un fragment séparé de sa localité rejoint la partie voisine qui partage le plus de côtés avec lui
		for (int part = 0; part < inside.Count(); part++)
		{
			if (labels[part] >= 0)
				RejoinFragments(labels[part], nameCells[inside[part]], labels);
		}

		m_aSplits.Insert(string.Format("bloc %1 coupé entre %2", CellRefText(m_aPieceSeed[piece]), names));
	}

	//------------------------------------------------------------------------------------------------
	//! Les carrés d'une partie qui ne tiennent pas à son carré de nom passent, par fragment, à l'autre partie qui
	//! partage le plus de côtés avec eux (à égalité, la première de la liste) — SplitPiece
	protected void RejoinFragments(int label, int homeCell, notnull array<int> parts)
	{
		int count = m_iN * m_iN;
		array<int> mark = {};
		FillInts(mark, count, 0);

		// Partie tenue par le carré du nom
		array<int> homeQueue = {};
		if (homeCell >= 0 && m_aPiece[homeCell] == label)
		{
			mark[homeCell] = 1;
			homeQueue.Insert(homeCell);
			int head = 0;
			while (head < homeQueue.Count())
			{
				int at = homeQueue[head];
				head++;
				for (int d1 = 0; d1 < 4; d1++)
				{
					int nb1 = NeighbourOf(at, d1);
					if (nb1 < 0 || m_aPiece[nb1] != label || mark[nb1] != 0)
						continue;
					mark[nb1] = 1;
					homeQueue.Insert(nb1);
				}
			}
		}

		for (int cell = 0; cell < count; cell++)
		{
			if (m_aPiece[cell] != label || mark[cell] != 0)
				continue;

			// Un fragment : carrés de même numéro non atteints, d'un seul tenant
			array<int> fragment = {};
			mark[cell] = 2;
			fragment.Insert(cell);
			int fragmentHead = 0;
			while (fragmentHead < fragment.Count())
			{
				int fragmentAt = fragment[fragmentHead];
				fragmentHead++;
				for (int d2 = 0; d2 < 4; d2++)
				{
					int nb2 = NeighbourOf(fragmentAt, d2);
					if (nb2 < 0 || m_aPiece[nb2] != label || mark[nb2] != 0)
						continue;
					mark[nb2] = 2;
					fragment.Insert(nb2);
				}
			}

			int target = -1;
			int targetSides = 0;
			foreach (int other : parts)
			{
				if (other < 0 || other == label)
					continue;
				int sides = 0;
				foreach (int fragmentCell : fragment)
				{
					for (int d3 = 0; d3 < 4; d3++)
					{
						int nb3 = NeighbourOf(fragmentCell, d3);
						if (nb3 >= 0 && m_aPiece[nb3] == other)
							sides++;
					}
				}
				if (sides > targetSides)
				{
					target = other;
					targetSides = sides;
				}
			}
			if (target < 0)
				continue;
			foreach (int movedCell : fragment)
				m_aPiece[movedCell] = target;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 9. Fusion (A4), comme blocs.py : tant qu'un morceau SANS localité (hors île) a moins de m_iZoneMinCells
	//! carrés, le plus petit (à égalité le premier) est versé dans le voisin qui partage le plus de côtés avec lui ;
	//! égalité : le plus grand, puis le premier ; la base est un voisin comme un autre (elle n'est retenue qu'en
	//! dernier à égalité parfaite) — BuildPieces
	protected void MergeSmallPieces()
	{
		int minCells = m_Front.m_iZoneMinCells;
		if (minCells <= 1)
			return;

		int count = m_iN * m_iN;
		int pieces = m_aPieceSeed.Count();
		array<int> nameCells = {};
		foreach (SRP_FrontLocality locality : m_aLocalities)
			nameCells.Insert(CellOf(locality.m_vLabel));
		array<int> sizes = {};
		array<bool> hasLocality = {};
		array<bool> skipped = {};
		array<int> contact = {};
		FillBools(skipped, pieces, false);

		for (int guard = 0; guard <= pieces; guard++)
		{
			// Tailles courantes
			FillInts(sizes, pieces, 0);
			int baseSize = 0;
			for (int cell = 0; cell < count; cell++)
			{
				int label = m_aPiece[cell];
				if (label >= 0)
					sizes[label] = sizes[label] + 1;
				else if (label == PIECE_BASE)
					baseSize++;
			}
			FillBools(hasLocality, pieces, false);
			foreach (int nameCell : nameCells)
			{
				if (nameCell >= 0 && m_aPiece[nameCell] >= 0)
					hasLocality[m_aPiece[nameCell]] = true;
			}

			// Le plus petit morceau sans localité (à égalité, le premier)
			int small = -1;
			for (int p = 0; p < pieces; p++)
			{
				if (sizes[p] <= 0 || sizes[p] >= minCells || skipped[p] || hasLocality[p])
					continue;
				if (m_Front.m_bIslandSingleZone && m_aCellLand[m_aPieceSeed[p]] != 0)
					continue;	// une île n'est jamais versée ailleurs (Q5)
				if (small < 0 || sizes[p] < sizes[small])
					small = p;
			}
			if (small < 0)
				break;

			// Côtés partagés avec chaque voisin
			FillInts(contact, pieces, 0);
			int baseContact = 0;
			for (int c2 = 0; c2 < count; c2++)
			{
				if (m_aPiece[c2] != small)
					continue;
				for (int dir = 0; dir < 4; dir++)
				{
					int nb = NeighbourOf(c2, dir);
					if (nb < 0)
						continue;
					int nbLabel = m_aPiece[nb];
					if (nbLabel == small)
						continue;
					if (nbLabel == PIECE_BASE)
						baseContact++;
					else if (nbLabel >= 0)
						contact[nbLabel] = contact[nbLabel] + 1;
				}
			}

			int target = -1;
			int targetContact = 0;
			int targetSize = 0;
			for (int q = 0; q < pieces; q++)
			{
				if (contact[q] <= 0)
					continue;
				if (contact[q] > targetContact || (contact[q] == targetContact && sizes[q] > targetSize))
				{
					target = q;
					targetContact = contact[q];
					targetSize = sizes[q];
				}
			}
			if (baseContact > 0 && (baseContact > targetContact || (baseContact == targetContact && baseSize > targetSize)))
				target = PIECE_BASE;
			if (target == -1)
			{
				skipped[small] = true;		// aucun voisin en jeu : il reste tel quel
				continue;
			}

			for (int c3 = 0; c3 < count; c3++)
			{
				if (m_aPiece[c3] == small)
					m_aPiece[c3] = target;
			}
			if (target >= 0 && m_aPieceFirst[small] < m_aPieceFirst[target])
				m_aPieceFirst[target] = m_aPieceFirst[small];
			if (target == PIECE_BASE)
				m_iMergedIntoBase += sizes[small];
			m_iMergedPieces++;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 10. Numérotation par distance base -> ancre, codes m_sCodePrefix + deux chiffres (A6) — Build
	protected void NumberZones()
	{
		int count = m_iN * m_iN;
		int pieces = m_aPieceSeed.Count();
		array<int> sizes = {};
		FillInts(sizes, pieces, 0);
		for (int cell = 0; cell < count; cell++)
		{
			int label = m_aPiece[cell];
			if (label >= 0)
				sizes[label] = sizes[label] + 1;
		}

		// Ancre : centre du bloc du départ du plus petit morceau de la zone (centre_bloc de blocs.py) ; tri par
		// distance croissante à la base, à égalité le plus petit numéro de morceau
		array<int> order = {};
		array<float> distances = {};
		array<vector> anchors = {};
		for (int p = 0; p < pieces; p++)
		{
			if (sizes[p] <= 0)
				continue;
			vector anchor = BlockCenter(m_aPieceSeed[m_aPieceFirst[p]]);
			float distance = vector.DistanceXZ(anchor, m_vBase);
			int at = order.Count();
			while (at > 0 && distances[at - 1] > distance)
				at--;
			order.InsertAt(p, at);
			distances.InsertAt(distance, at);
			anchors.InsertAt(anchor, at);
		}

		m_aZones.Clear();
		SRP_FrontZone baseZone = new SRP_FrontZone();
		baseZone.m_iIndex = 0;
		baseZone.m_sCode = CodeOf(0);
		baseZone.m_sName = BaseName();
		baseZone.m_bBase = true;
		baseZone.m_iOwner = SRP_EFrontOwner.BLEU;
		baseZone.m_vAnchor = Vector(m_vBase[0], GroundY(m_vBase[0], m_vBase[2]), m_vBase[2]);
		m_aZones.Insert(baseZone);

		array<int> zoneOfPiece = {};
		FillInts(zoneOfPiece, pieces, -1);
		for (int i = 0; i < order.Count(); i++)
		{
			SRP_FrontZone zone = new SRP_FrontZone();
			zone.m_iIndex = i + 1;
			zone.m_sCode = CodeOf(i + 1);
			zone.m_sName = DEFAULT_ZONE_NAME;
			vector a = anchors[i];
			zone.m_vAnchor = Vector(a[0], GroundY(a[0], a[2]), a[2]);
			zone.m_bIsland = m_aCellLand[m_aPieceSeed[order[i]]] > 0;
			m_aZones.Insert(zone);
			zoneOfPiece[order[i]] = i + 1;
		}

		for (int c2 = 0; c2 < count; c2++)
		{
			int piece = m_aPiece[c2];
			if (piece == PIECE_BASE)
				m_aCellZone[c2] = 0;
			else if (piece >= 0)
				m_aCellZone[c2] = zoneOfPiece[piece];
			else
				m_aCellZone[c2] = -1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Liste des carrés de chaque zone (ordre des carrés) et centre de gravité, y au sol — Build, ComputeFlags
	protected void UpdateZoneCells()
	{
		foreach (SRP_FrontZone zone : m_aZones)
			zone.m_aCells.Clear();

		int count = m_iN * m_iN;
		for (int cell = 0; cell < count; cell++)
		{
			int zoneIndex = m_aCellZone[cell];
			if (zoneIndex >= 0 && zoneIndex < m_aZones.Count())
				m_aZones[zoneIndex].m_aCells.Insert(cell);
		}

		foreach (SRP_FrontZone target : m_aZones)
		{
			if (target.m_aCells.IsEmpty())
			{
				target.m_vCentroid = target.m_vAnchor;
				continue;
			}
			float sumX = 0;
			float sumZ = 0;
			foreach (int zoneCell : target.m_aCells)
			{
				vector center = CellCenter(zoneCell);
				sumX += center[0];
				sumZ += center[2];
			}
			float cellCount = target.m_aCells.Count();
			float cx = sumX / cellCount;
			float cz = sumZ / cellCount;
			target.m_vCentroid = Vector(cx, GroundY(cx, cz), cz);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Zones provisoires des localités d'après le carré de leur nom (avant les retouches et les points clés) — Build
	protected void AssignLocalitiesByName()
	{
		foreach (SRP_FrontLocality locality : m_aLocalities)
		{
			bool moved;
			int cell = PlayableCellNear(locality.m_vLabel, moved);
			locality.m_iZone = -1;
			if (cell >= 0)
				locality.m_iZone = m_aCellZone[cell];
		}
		AttachLocalities();
	}

	//------------------------------------------------------------------------------------------------
	//! Range les localités dans leur zone (m_iZone) : la plus grosse d'abord, puis la plus proche du centre de la
	//! zone ; m_iLocality et m_iKind de la zone — AssignLocalitiesByName, PlaceKeyPoints
	protected void AttachLocalities()
	{
		foreach (SRP_FrontZone zone : m_aZones)
		{
			zone.m_aLocalities.Clear();
			zone.m_iLocality = -1;
			zone.m_iKind = SRP_EZoneKind.AUCUNE;
		}

		for (int li = 0; li < m_aLocalities.Count(); li++)
		{
			int zoneIndex = m_aLocalities[li].m_iZone;
			if (zoneIndex <= 0 || zoneIndex >= m_aZones.Count())
				continue;
			SRP_FrontZone holder = m_aZones[zoneIndex];
			int at = holder.m_aLocalities.Count();
			while (at > 0 && ComesBefore(li, holder.m_aLocalities[at - 1], holder))
				at--;
			holder.m_aLocalities.InsertAt(li, at);
		}

		foreach (SRP_FrontZone target : m_aZones)
		{
			if (target.m_aLocalities.IsEmpty())
				continue;
			target.m_iLocality = target.m_aLocalities[0];
			target.m_iKind = m_aLocalities[target.m_iLocality].m_iKind;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! La localité a passe-t-elle avant b dans la zone ? Plus grosse, puis plus proche du centre de gravité — AttachLocalities
	protected bool ComesBefore(int a, int b, SRP_FrontZone zone)
	{
		int kindA = m_aLocalities[a].m_iKind;
		int kindB = m_aLocalities[b].m_iKind;
		if (kindA != kindB)
			return kindA > kindB;
		float distanceA = vector.DistanceXZ(LocalityCentre(a), zone.m_vCentroid);
		float distanceB = vector.DistanceXZ(LocalityCentre(b), zone.m_vCentroid);
		if (distanceA != distanceB)
			return distanceA < distanceB;
		return a < b;
	}

	//------------------------------------------------------------------------------------------------
	//! 11. Lit front_retouches.txt (l'écrit avec son mode d'emploi s'il manque, comme SRP_Charte.c:105-119) ; garde les
	//! lignes « -> » et « ressource » ; ignore « region » (lue par le Commandeur) sans avertissement ; refuse « point »
	//! (Q8 : les points clés se posent au Workbench, chaque retouche demande de republier) — Build
	protected void ReadRetouches()
	{
		m_aRetouchLines.Clear();
		m_aRetouchLineNo.Clear();

		string path = SRP_FrontComponent.RETOUCH_PATH;
		if (!FileIO.FileExists(path))
			WriteDefaultRetouches();

		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
		{
			Warn("front_retouches.txt illisible : aucune retouche appliquée");
			return;
		}

		string line;
		int lineNo = 0;
		while (file.ReadLine(line) >= 0 && lineNo < 100000)
		{
			lineNo++;
			string text = CleanLine(line);
			if (text.IsEmpty() || !HasLetterOrDigit(text))
				continue;
			string head = FirstWord(text);
			if (head == "region")
				continue;	// « region S12 -> R3 » : lue par le Commandeur (SRP_CmdRegions)
			if (head == "point")
			{
				// Q8 : pas de point clé par fichier ; seul le repère posé au Workbench compte (repli : nom de la carte)
				Refuse(lineNo, "les points clés ne se règlent plus dans ce fichier : poser un repère SRP_PointCle_Centre ou SRP_PointCle_QG au Workbench, puis republier");
				continue;
			}
			if (head == "ressource" || text.Contains("->"))
			{
				m_aRetouchLines.Insert(text);
				m_aRetouchLineNo.Insert(lineNo);
				continue;
			}
			Refuse(lineNo, "ligne illisible « " + text + " »");
		}
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit le mode d'emploi de front_retouches.txt (déplacement, ressource, region) — ReadRetouches
	protected void WriteDefaultRetouches()
	{
		FileHandle file = FileIO.OpenFile(SRP_FrontComponent.RETOUCH_PATH, FileMode.WRITE);
		if (!file)
		{
			Warn("impossible de créer front_retouches.txt");
			return;
		}
		file.WriteLine("# Retouches du front, lues au démarrage du serveur (redémarrer pour les prendre en compte)");
		file.WriteLine("# Une retouche par ligne ; tout ce qui suit # est un commentaire.");
		file.WriteLine("#");
		file.WriteLine("# 074 042 -> S07        déplace le carré dans la zone S07 (ou -> Régina, nom d'avant les retouches)");
		file.WriteLine("#                       le carré doit toucher la zone d'arrivée par un côté, et sa zone de départ ne doit pas être coupée");
		file.WriteLine("# ressource S12 carburant           la zone produit du carburant (ou pièces) ; débit en option : ressource S12 carburant 8");
		file.WriteLine("# ressource S12 rien                la zone ne produit rien (même un village ou une ville)");
		file.WriteLine("# region S12 -> R3      rattache la zone S12 à la région R3 du Commandeur (liste dans commandeur_regions.txt)");
		file.WriteLine("#");
		file.WriteLine("# Les points clés (centre et QG) ne se règlent pas ici : repères SRP_PointCle_Centre et SRP_PointCle_QG posés au Workbench, puis republier.");
		file.WriteLine("#");
		file.WriteLine("# La liste des zones, des points clés et des villages coupés est dans front_zones.txt");
		file.Close();
		Print("[SRP] Front : front_retouches.txt créé avec son mode d'emploi", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! 11. Déplacements « 074 042 -> S07 » : contrôles (en jeu, hors base, voisin de l'arrivée, départ d'un seul
	//! tenant) ; refus « Retouche ligne 12 refusée : … » au journal et au rapport — Build
	protected void ApplyMoves()
	{
		for (int i = 0; i < m_aRetouchLines.Count(); i++)
		{
			string text = m_aRetouchLines[i];
			int arrow = text.IndexOf("->");
			if (arrow < 0)
				continue;
			if (FirstWord(text) == "ressource")
				continue;
			int lineNo = m_aRetouchLineNo[i];

			string left = text.Substring(0, arrow);
			left = left.Trim();
			string right = text.Substring(arrow + 2, text.Length() - arrow - 2);
			right = right.Trim();

			int cell = ParseCellRef(left);
			if (cell < 0)
			{
				Refuse(lineNo, "référence de carré illisible « " + left + " » (exemple : 074 042)");
				continue;
			}
			string cellText = CellRefText(cell);
			int fromZone = m_aCellZone[cell];
			if (fromZone < 0)
			{
				Refuse(lineNo, "le carré " + cellText + " n'est pas en jeu (mer ou îlot retiré)");
				continue;
			}
			if (fromZone == 0)
			{
				Refuse(lineNo, "le carré " + cellText + " est dans la base");
				continue;
			}
			int toZone = FindZoneByText(right);
			if (toZone < 0)
			{
				Refuse(lineNo, "zone « " + right + " » inconnue");
				continue;
			}
			if (toZone == 0)
			{
				Refuse(lineNo, "un carré ne peut pas entrer dans la base");
				continue;
			}
			if (toZone == fromZone)
			{
				Refuse(lineNo, "le carré " + cellText + " est déjà dans " + ZoneText(toZone));
				continue;
			}
			if (!TouchesZone(cell, toZone))
			{
				Refuse(lineNo, "le carré " + cellText + " ne touche pas " + ZoneText(toZone) + " par un côté");
				continue;
			}
			int fromCells = CountZoneCells(fromZone);
			if (fromCells <= 1)
			{
				Refuse(lineNo, ZoneText(fromZone) + " n'aurait plus aucun carré");
				continue;
			}
			if (!StaysConnected(fromZone, cell, fromCells))
			{
				Refuse(lineNo, ZoneText(fromZone) + " serait coupée en deux");
				continue;
			}

			m_aCellZone[cell] = toZone;
			m_aApplied.Insert(string.Format("ligne %1 : carré %2 déplacé de %3 vers %4", lineNo, cellText, ZoneText(fromZone), ZoneText(toZone)));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! G5 : lignes « ressource S12 carburant [débit] » -> m_iResource = SRP_Resources.FromName, m_iRate = débit (sinon
	//! -1, débit par défaut) ; « ressource S12 rien » -> m_iResource = -1 et m_iRate = 0 (l'économie lit m_iRate == 0
	//! comme « ne produit rien ») ; sans ligne, -1 et -1 : défaut du genre, rangé par SRP_FrontEconomy.Start — Build
	protected void ApplyResources()
	{
		for (int i = 0; i < m_aRetouchLines.Count(); i++)
		{
			string text = m_aRetouchLines[i];
			if (FirstWord(text) != "ressource")
				continue;
			int lineNo = m_aRetouchLineNo[i];

			array<string> words = {};
			text.Split(" ", words, true);
			int last = words.Count() - 1;
			int rate = -1;
			if (last >= 3 && SRP_Utils.IsNumeric(words[last]))
			{
				rate = words[last].ToInt();
				last--;
			}
			if (last < 2)
			{
				Refuse(lineNo, "attendu « ressource S12 carburant » (débit en option) ou « ressource S12 rien »");
				continue;
			}
			string resourceWord = words[last];
			resourceWord.ToLower();
			bool nothing = (resourceWord == "rien");
			int resource = -1;
			if (nothing)
			{
				rate = 0;	// « ne produit rien », même pour un village ou une ville (débit écrit ignoré)
			}
			else
			{
				resource = SRP_Resources.FromName(resourceWord);
				if (resource < 0)
				{
					Refuse(lineNo, "ressource « " + words[last] + " » inconnue (carburant, pièces, munitions, vivres ou rien)");
					continue;
				}
			}
			string zoneText = JoinWords(words, 1, last - 1);
			int zoneIndex = FindZoneByText(zoneText);
			if (zoneIndex < 0)
			{
				Refuse(lineNo, "zone « " + zoneText + " » inconnue");
				continue;
			}
			if (zoneIndex == 0)
			{
				Refuse(lineNo, "la base ne produit pas de ressource de zone");
				continue;
			}

			SRP_FrontZone zone = m_aZones[zoneIndex];
			zone.m_iResource = resource;
			zone.m_iRate = rate;
			if (nothing)
			{
				m_aApplied.Insert(string.Format("ligne %1 : %2 ne produit rien", lineNo, ZoneText(zoneIndex)));
				continue;
			}
			string rateText = "débit par défaut";
			if (rate >= 0)
				rateText = string.Format("%1 par heure", rate);
			m_aApplied.Insert(string.Format("ligne %1 : %2 produit %3 (%4)", lineNo, ZoneText(zoneIndex), SRP_Resources.GetName(resource), rateText));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 12. Points clés (Q8 : posés au Workbench, chaque retouche demande de republier) : pour chaque localité de zone,
	//! CENTRE par REPERE (SRP_KeyPointComponent.GetDefs), sinon NOM_CARTE (repli, compté dans m_iMissingKeyPoints) ;
	//! QG (villes) par REPERE, sinon aucun (ville à un seul point clé, comptée dans m_iMissingKeyPoints, jamais sur le
	//! nom de la carte : il tomberait sur le centre) ; carré hors jeu ou base : carré en jeu le plus proche (anneaux 1
	//! à 5), et le point y est reposé sur la terre (MovePointInto) ; la localité appartient à la zone de son CENTRE — Build
	protected void PlaceKeyPoints()
	{
		m_aKeyPoints.Clear();
		m_iMissingKeyPoints = 0;

		int locCount = m_aLocalities.Count();
		array<ref SRP_FrontPointChoice> centres = {};
		array<ref SRP_FrontPointChoice> hqs = {};
		for (int i = 0; i < locCount; i++)
		{
			centres.Insert(new SRP_FrontPointChoice());
			hqs.Insert(new SRP_FrontPointChoice());
		}

		// 1. Repères posés au Workbench (prioritaires, Q8)
		array<ref SRP_KeyPointDef> defs = SRP_KeyPointComponent.GetDefs();
		if (defs)
		{
			foreach (SRP_KeyPointDef def : defs)
			{
				if (!def)
					continue;
				string defText = def.m_sSource;
				if (defText.IsEmpty())
					defText = "repère sans nom";
				int defLocality = LocalityForPoint(def.m_sLocality, def.m_vPos, defText);
				if (defLocality < 0)
					continue;
				SRP_FrontPointChoice defChoice = PickChoice(defLocality, def.m_iRole, SRP_EKeyPointSource.REPERE, defText, centres, hqs);
				if (!defChoice)
					continue;
				defChoice.m_bFound = true;
				defChoice.m_vPos = def.m_vPos;
				defChoice.m_fYaw = def.m_fYaw;
				defChoice.m_iSource = SRP_EKeyPointSource.REPERE;
				defChoice.m_sText = defText;
			}
		}

		// 2. Repli et carré de chaque point
		array<bool> ignored = {};
		for (int li = 0; li < locCount; li++)
		{
			SRP_FrontLocality locality = m_aLocalities[li];
			SRP_FrontPointChoice centre = centres[li];
			if (!centre.m_bFound)
			{
				// Repli Q8 : l'endroit du nom sur la carte, listé MANQUANT
				centre.m_bFound = true;
				centre.m_vPos = Vector(locality.m_vLabel[0], GroundY(locality.m_vLabel[0], locality.m_vLabel[2]), locality.m_vLabel[2]);
				centre.m_iSource = SRP_EKeyPointSource.NOM_CARTE;
				centre.m_sText = "nom de la carte";
				m_iMissingKeyPoints++;
				Warn(locality.m_sName + " : aucun repère de centre, point clé sur le nom de la carte (MANQUANT, repère SRP_PointCle_Centre à poser au Workbench)");
			}

			bool movedCentre;
			centre.m_iCell = PlayableCellNear(centre.m_vPos, movedCentre);
			if (centre.m_iCell < 0)
			{
				Fail(locality.m_sName + " : aucun carré en jeu hors base à moins de 5 carrés de son centre, localité ignorée");
				ignored.Insert(true);
				continue;
			}
			ignored.Insert(false);
			// Le point est posé DANS son carré : poste de commandement (m_vPos) et saisie (m_iCell) au même endroit, et
			// même carré chez les joueurs, qui le recalculent depuis m_vPos
			centre.m_vPos = MovePointInto(centre.m_vPos, centre.m_iCell, movedCentre);
			if (movedCentre)
				Warn(locality.m_sName + " : centre hors jeu ou dans la base, rattaché au carré " + CellRefText(centre.m_iCell) + " et reposé à " + PosText(centre.m_vPos));

			if (locality.m_iKind != SRP_EZoneKind.VILLE)
				continue;
			SRP_FrontPointChoice hq = hqs[li];
			if (!hq.m_bFound)
			{
				m_iMissingKeyPoints++;
				Warn(locality.m_sName + " : ville sans QG (MANQUANT, repère SRP_PointCle_QG à poser au Workbench)");
				continue;
			}
			bool movedHq;
			hq.m_iCell = PlayableCellNear(hq.m_vPos, movedHq);
			if (hq.m_iCell < 0)
			{
				hq.m_bFound = false;
				m_iMissingKeyPoints++;
				Fail(locality.m_sName + " : aucun carré en jeu hors base près du QG, QG ignoré");
				continue;
			}
			hq.m_vPos = MovePointInto(hq.m_vPos, hq.m_iCell, movedHq);
			if (movedHq)
				Warn(locality.m_sName + " : QG hors jeu ou dans la base, rattaché au carré " + CellRefText(hq.m_iCell) + " et reposé à " + PosText(hq.m_vPos));
		}

		// Localités ignorées retirées avant le registre (les index restent ceux du socle)
		for (int drop = locCount - 1; drop >= 0; drop--)
		{
			if (!ignored[drop])
				continue;
			m_aLocalities.RemoveOrdered(drop);
			centres.RemoveOrdered(drop);
			hqs.RemoveOrdered(drop);
		}

		// 4. Registre : pour chaque localité, CENTRE puis QG ; la localité est dans la zone de son CENTRE
		for (int k = 0; k < m_aLocalities.Count(); k++)
		{
			SRP_FrontLocality member = m_aLocalities[k];
			SRP_FrontPointChoice memberCentre = centres[k];
			member.m_iZone = m_aCellZone[memberCentre.m_iCell];
			member.m_iCentre = AddKeyPoint(k, SRP_EKeyPointRole.CENTRE, memberCentre, member.m_iZone);
			member.m_iQg = -1;
			SRP_FrontPointChoice memberHq = hqs[k];
			if (member.m_iKind != SRP_EZoneKind.VILLE || !memberHq.m_bFound || memberHq.m_iCell < 0)
				continue;
			// Le QG compte pour la zone de la localité, même si son carré est ailleurs
			member.m_iQg = AddKeyPoint(k, SRP_EKeyPointRole.QG, memberHq, member.m_iZone);
			int hqZone = m_aCellZone[memberHq.m_iCell];
			if (hqZone != member.m_iZone)
				Warn(member.m_sName + " : le QG est dans " + ZoneText(hqZone) + ", il compte pour " + ZoneText(member.m_iZone));
		}

		// 5. Localités rangées dans leur zone (principale d'abord), points clés de chaque zone
		AttachLocalities();
		foreach (SRP_FrontZone zone : m_aZones)
		{
			zone.m_aKeyPoints.Clear();
			foreach (int zoneLocality : zone.m_aLocalities)
			{
				SRP_FrontLocality held = m_aLocalities[zoneLocality];
				if (held.m_iCentre >= 0)
					zone.m_aKeyPoints.Insert(held.m_iCentre);
				if (held.m_iQg >= 0)
					zone.m_aKeyPoints.Insert(held.m_iQg);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Place du point (centre ou QG) d'une localité, null avec avertissement si le rôle ne va pas (QG hors ville) ou
	//! si la place est déjà prise (deuxième repère du même rôle : le premier lu est gardé) — PlaceKeyPoints
	protected SRP_FrontPointChoice PickChoice(int locality, int role, int incomingSource, string sourceText, notnull array<ref SRP_FrontPointChoice> centres, notnull array<ref SRP_FrontPointChoice> hqs)
	{
		SRP_FrontLocality target = m_aLocalities[locality];
		SRP_FrontPointChoice slot = centres[locality];
		string roleWord = "centre";
		if (role == SRP_EKeyPointRole.QG)
		{
			if (target.m_iKind != SRP_EZoneKind.VILLE)
			{
				Warn(sourceText + " : QG ignoré, " + target.m_sName + " n'est ni une ville ni un bourg");
				return null;
			}
			slot = hqs[locality];
			roleWord = "QG";
		}
		if (!slot.m_bFound)
			return slot;

		Warn(sourceText + " : deuxième point " + roleWord + " pour " + target.m_sName + ", ignoré (" + slot.m_sText + " gardé)");
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité d'un point : par son nom (localité de zone, sinon ignoré avec avertissement), ou sans nom celle dont le
	//! nom est le plus proche à m_fKeyPointMaxDistance au plus ; -1 si ignoré — PlaceKeyPoints
	protected int LocalityForPoint(string name, vector pos, string sourceText)
	{
		string wanted = name.Trim();
		if (!wanted.IsEmpty())
		{
			int found = FindLocalityByName(wanted);
			if (found >= 0)
				return found;
			if (FindHomeByName(wanted) >= 0)
				Warn(sourceText + " : " + wanted + " est une localité de la base, point ignoré");
			else
				Warn(sourceText + " : localité « " + wanted + " » inconnue, point ignoré");
			return -1;
		}

		int best = -1;
		float bestDistance = 0;
		for (int i = 0; i < m_aLocalities.Count(); i++)
		{
			float distance = vector.DistanceXZ(pos, m_aLocalities[i].m_vLabel);
			if (best < 0 || distance < bestDistance)
			{
				best = i;
				bestDistance = distance;
			}
		}
		int bestHome = -1;
		float bestHomeDistance = 0;
		for (int h = 0; h < m_aHomeName.Count(); h++)
		{
			float homeDistance = vector.DistanceXZ(pos, m_aHomePos[h]);
			if (bestHome < 0 || homeDistance < bestHomeDistance)
			{
				bestHome = h;
				bestHomeDistance = homeDistance;
			}
		}

		float maxDistance = m_Front.m_fKeyPointMaxDistance;
		if (bestHome >= 0 && bestHomeDistance <= maxDistance && (best < 0 || bestHomeDistance < bestDistance))
		{
			Warn(sourceText + " : le nom le plus proche est " + m_aHomeName[bestHome] + ", localité de la base, point ignoré");
			return -1;
		}
		if (best < 0 || bestDistance > maxDistance)
		{
			int maxMeters = Math.Round(maxDistance);
			Warn(string.Format("%1 : aucune localité à moins de %2 m, point ignoré", sourceText, maxMeters));
			return -1;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute un point clé au registre ; rend son index — PlaceKeyPoints
	protected int AddKeyPoint(int locality, int role, SRP_FrontPointChoice choice, int zone)
	{
		SRP_FrontKeyPoint point = new SRP_FrontKeyPoint();
		point.m_iIndex = m_aKeyPoints.Count();
		point.m_iLocality = locality;
		point.m_iRole = role;
		point.m_vPos = choice.m_vPos;
		point.m_fYaw = choice.m_fYaw;
		point.m_iCell = choice.m_iCell;
		point.m_iZone = zone;
		point.m_iSource = choice.m_iSource;
		point.m_sSourceText = choice.m_sText;
		m_aKeyPoints.Insert(point);
		return point.m_iIndex;
	}

	//------------------------------------------------------------------------------------------------
	//! 13. Noms (A6) : zone avec localité = nom de la localité principale ; sinon lieu-dit le plus proche
	//! (MDT_NAME_LOCAL, HILL, RIDGE, VALLEY, ISLAND), attribution gloutonne, « Secteur » à défaut — Build
	protected void NameZones()
	{
		ReadPlaceNames();

		int zoneCount = m_aZones.Count();
		array<bool> named = {};
		FillBools(named, zoneCount, false);
		if (zoneCount > 0)
		{
			m_aZones[0].m_sName = BaseName();
			named[0] = true;
		}
		for (int i = 1; i < zoneCount; i++)
		{
			SRP_FrontZone zone = m_aZones[i];
			zone.m_sName = DEFAULT_ZONE_NAME;
			if (zone.m_iLocality >= 0 && zone.m_iLocality < m_aLocalities.Count())
			{
				zone.m_sName = m_aLocalities[zone.m_iLocality].m_sName;
				named[i] = true;
			}
		}

		// Paires (zone, lieu-dit, distance) : 0 si le nom tombe dans un carré de la zone, sinon distance au centre
		// de gravité ; gardées à m_fNameMaxDistance au plus, dans l'ordre des codes puis des lieux-dits
		array<int> pairZone = {};
		array<int> pairPlace = {};
		array<float> pairDistance = {};
		for (int z = 1; z < zoneCount; z++)
		{
			if (named[z])
				continue;
			SRP_FrontZone candidate = m_aZones[z];
			for (int p = 0; p < m_aPlaceName.Count(); p++)
			{
				vector place = m_aPlacePos[p];
				float distance = 0;
				int placeCell = CellOf(place);
				if (placeCell < 0 || m_aCellZone[placeCell] != z)
					distance = vector.DistanceXZ(place, candidate.m_vCentroid);
				if (distance > m_Front.m_fNameMaxDistance)
					continue;
				pairZone.Insert(z);
				pairPlace.Insert(p);
				pairDistance.Insert(distance);
			}
		}

		// Attribution gloutonne : la plus courte d'abord (à égalité, le plus petit code) ; un nom par zone, jamais deux fois
		array<bool> used = {};
		FillBools(used, m_aPlaceName.Count(), false);
		for (int turn = 0; turn < zoneCount; turn++)
		{
			int best = -1;
			for (int k = 0; k < pairZone.Count(); k++)
			{
				if (named[pairZone[k]] || used[pairPlace[k]])
					continue;
				if (best < 0 || pairDistance[k] < pairDistance[best])
					best = k;
			}
			if (best < 0)
				break;
			m_aZones[pairZone[best]].m_sName = m_aPlaceName[pairPlace[best]];
			named[pairZone[best]] = true;
			used[pairPlace[best]] = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lieux-dits candidats (A6), lus une fois : noms de la carte MDT_NAME_LOCAL, HILL, RIDGE, VALLEY, ISLAND, puis
	//! les localités de la base ; jamais le nom d'une localité de zone — NameZones
	protected void ReadPlaceNames()
	{
		if (m_bPlacesRead)
			return;
		m_bPlacesRead = true;
		m_aPlaceName.Clear();
		m_aPlacePos.Clear();

		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if (mapEntity)
		{
			AddPlaceNames(mapEntity, EMapDescriptorType.MDT_NAME_LOCAL);
			AddPlaceNames(mapEntity, EMapDescriptorType.MDT_NAME_HILL);
			AddPlaceNames(mapEntity, EMapDescriptorType.MDT_NAME_RIDGE);
			AddPlaceNames(mapEntity, EMapDescriptorType.MDT_NAME_VALLEY);
			AddPlaceNames(mapEntity, EMapDescriptorType.MDT_NAME_ISLAND);
		}
		for (int i = 0; i < m_aHomeName.Count(); i++)
			AddPlaceName(m_aHomeName[i], m_aHomePos[i]);
	}

	//------------------------------------------------------------------------------------------------
	//! Noms de la carte d'un type de lieu-dit — ReadPlaceNames
	protected void AddPlaceNames(SCR_MapEntity mapEntity, int descriptorType)
	{
		array<MapItem> items = {};
		mapEntity.GetByType(items, descriptorType);
		foreach (MapItem item : items)
		{
			if (!item)
				continue;
			IEntity location = item.Entity();
			if (!location)
				continue;
			AddPlaceName(WidgetManager.Translate(item.GetDisplayName()), location.GetOrigin());
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un lieu-dit candidat, sans doublon de nom ni nom de localité de zone — ReadPlaceNames, AddPlaceNames
	protected void AddPlaceName(string name, vector pos)
	{
		string clean = name.Trim();
		if (clean.IsEmpty() || clean == DEFAULT_ZONE_NAME || m_aPlaceName.Contains(clean))
			return;
		if (FindLocalityByName(clean) >= 0)
			return;
		m_aPlaceName.Insert(clean);
		m_aPlacePos.Insert(pos);
	}

	//------------------------------------------------------------------------------------------------
	//! 14. Liaisons maritimes (Q5) : pour chaque île, la paire de carrés la plus proche avec la grande île — Build
	protected void BuildSeaLinks()
	{
		int count = m_iN * m_iN;
		FillInts(m_aSeaLink, count, -1);
		if (!m_Front.m_bSeaLinks || m_iIslands <= 0)
			return;

		array<int> mainCells = {};
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aCellLand[cell] == 0 && m_aCellZone[cell] >= 0)
				mainCells.Insert(cell);
		}

		for (int land = 1; land <= m_iIslands; land++)
		{
			int bestIsland = -1;
			int bestMain = -1;
			float bestDistance = 0;
			for (int a = 0; a < count; a++)
			{
				if (m_aCellLand[a] != land || m_aCellZone[a] < 0)
					continue;
				vector islandCenter = CellCenter(a);
				foreach (int b : mainCells)
				{
					if (m_aSeaLink[b] >= 0)
						continue;	// un carré de côte ne porte qu'une liaison
					float distance = vector.DistanceSqXZ(islandCenter, CellCenter(b));
					if (bestIsland < 0 || distance < bestDistance)
					{
						bestIsland = a;
						bestMain = b;
						bestDistance = distance;
					}
				}
			}
			if (bestIsland < 0)
			{
				Warn(string.Format("île n° %1 : aucun carré de côte libre pour la liaison par la mer", land));
				continue;
			}
			m_aSeaLink[bestIsland] = bestMain;
			m_aSeaLink[bestMain] = bestIsland;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! 15. Voisines, m_bShield (B4), centres, position des noms, genre, somme de contrôle m_iHash — Build
	protected void ComputeFlags()
	{
		int count = m_iN * m_iN;
		UpdateZoneCells();

		for (int i = 0; i < m_aZones.Count(); i++)
		{
			SRP_FrontZone zone = m_aZones[i];
			zone.m_iIndex = i;
			zone.m_bBase = (i == 0);
			zone.m_bShield = false;
			zone.m_bIsland = false;
			zone.m_aNeighbours.Clear();
		}

		for (int cell = 0; cell < count; cell++)
		{
			int zoneIndex = m_aCellZone[cell];
			if (zoneIndex < 0)
				continue;
			SRP_FrontZone here = m_aZones[zoneIndex];
			if (zoneIndex > 0 && m_aCellLand[cell] > 0)
				here.m_bIsland = true;
			for (int dir = 0; dir < 4; dir++)
			{
				int nb = NeighbourOf(cell, dir);
				if (nb < 0)
					continue;
				int nbZone = m_aCellZone[nb];
				if (nbZone < 0 || nbZone == zoneIndex)
					continue;
				if (!here.m_aNeighbours.Contains(nbZone))
					here.m_aNeighbours.Insert(nbZone);
				if (nbZone == 0 && zoneIndex > 0 && m_Front.m_bShieldBaseNeighbours)
					here.m_bShield = true;
			}
			int link = m_aSeaLink[cell];
			if (link >= 0)
			{
				int linkZone = m_aCellZone[link];
				if (linkZone >= 0 && linkZone != zoneIndex && !here.m_aNeighbours.Contains(linkZone))
					here.m_aNeighbours.Insert(linkZone);
			}
		}

		foreach (SRP_FrontZone target : m_aZones)
		{
			target.m_aNeighbours.Sort();
			target.m_vLabelPos = LabelPosition(target);
		}

		// Somme de contrôle, sans dépassement d'entier (h < 8388593, h x 131 < 2^31)
		int hash = 0;
		for (int c2 = 0; c2 < count; c2++)
			hash = (hash * 131 + m_aCellZone[c2] + 2) % 8388593;
		m_iHash = hash;
	}

	//------------------------------------------------------------------------------------------------
	//! Où écrire le nom : le centre de gravité s'il tombe dans la zone, sinon le centre du carré de la zone le plus
	//! proche (zone en croissant) — ComputeFlags
	protected vector LabelPosition(SRP_FrontZone zone)
	{
		if (zone.m_aCells.IsEmpty())
			return zone.m_vCentroid;
		int centroidCell = CellOf(zone.m_vCentroid);
		if (centroidCell >= 0 && m_aCellZone[centroidCell] == zone.m_iIndex)
			return zone.m_vCentroid;

		int best = -1;
		float bestDistance = 0;
		foreach (int cell : zone.m_aCells)
		{
			float distance = vector.DistanceSqXZ(CellCenter(cell), zone.m_vCentroid);
			if (best < 0 || distance < bestDistance)
			{
				best = cell;
				bestDistance = distance;
			}
		}
		vector center = CellCenter(best);
		return Vector(center[0], GroundY(center[0], center[2]), center[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! 16. Lignes du rapport (zones, points clés et sources, avertissements, retouches, villages coupés) — Build
	protected void BuildReport()
	{
		m_aReport.Clear();
		int count = m_iN * m_iN;
		int zoneCount = m_aZones.Count() - 1;
		int cellMeters = Math.Round(m_fCell);
		int baseMeters = Math.Round(m_Front.m_fBaseRadius);
		int villageMeters = Math.Round(m_Front.m_fVillageCheckRadius);

		m_aReport.Insert("# Zones du front, écrit au démarrage du serveur le " + SRP_Time.Now() + ". Ne pas modifier : les retouches se font dans front_retouches.txt");
		m_aReport.Insert(string.Format("# Grille de %1 x %2 carrés de %3 m ; %4 carrés en jeu ; %5 zones ; %6 localités de zone", m_iN, m_iN, cellMeters, m_iLandCells, zoneCount, m_aLocalities.Count()));
		m_aReport.Insert(string.Format("# Base « %1 » : %2 carrés (%3 à %4 m au plus du marqueur, %5 venus de petits morceaux versés dans la base)", BaseName(), m_aZones[0].m_aCells.Count(), m_iBaseRadiusCells, baseMeters, m_iMergedIntoBase));
		m_aReport.Insert(string.Format("# Terre ou mer : %1 points testés en %2 ms ; %3 îlot(s) sans localité retiré(s) (%4 carrés) ; %5 île(s) gardée(s)", m_iSamplePoints, m_iSampleMs, m_iDroppedIslets, m_iDroppedCells, m_iIslands));
		m_aReport.Insert(string.Format("# Morceaux de blocs : %1, puis %2 fusion(s) de morceaux de moins de %3 carrés", m_iPieceCount, m_iMergedPieces, m_Front.m_iZoneMinCells));
		m_aReport.Insert(string.Format("# Localités : %1 lue(s) dans les zones civiles, %2 sur la carte ; localités de la base : %3", m_iCivilLocalities, m_iMapLocalities, JoinWords(m_aHomeName, 0, m_aHomeName.Count() - 1)));
		m_aReport.Insert(string.Format("# Points clés : %1 posé(s), %2 manquant(s) (centre sur le nom de la carte ou QG absent)", m_aKeyPoints.Count(), m_iMissingKeyPoints));
		m_aReport.Insert(string.Format("# Somme de contrôle de la géométrie : %1", m_iHash));
		foreach (string splitNote : m_aSplits)
			m_aReport.Insert("# Q3 : " + splitNote);

		// Une ligne par zone
		m_aReport.Insert("");
		m_aReport.Insert("ZONES");
		for (int i = 0; i < m_aZones.Count(); i++)
		{
			SRP_FrontZone zone = m_aZones[i];
			string line = string.Format("%1 — %2 — %3 carré(s)", zone.m_sCode, zone.m_sName, zone.m_aCells.Count());
			if (i == 0)
			{
				line += " — la base (B2)";
				m_aReport.Insert(line);
				continue;
			}
			if (zone.m_aLocalities.IsEmpty())
				line += " — sans localité";
			foreach (int li : zone.m_aLocalities)
			{
				SRP_FrontLocality locality = m_aLocalities[li];
				line += " — " + locality.m_sKind + " " + locality.m_sName + " : centre " + KeyPointText(locality.m_iCentre);
				if (locality.m_iKind == SRP_EZoneKind.VILLE)
				{
					if (locality.m_iQg >= 0)
						line += ", QG " + KeyPointText(locality.m_iQg);
					else
						line += ", QG MANQUANT";
				}
			}
			if (zone.m_bShield)
				line += " — voisine de la base";
			if (zone.m_bIsland)
				line += " — île";
			if (zone.m_iResource >= 0)
				line += " — ressource " + SRP_Resources.GetName(zone.m_iResource);
			else if (zone.m_iRate == 0)
				line += " — ne produit rien (retouche)";
			m_aReport.Insert(line);
		}

		// Liaisons maritimes (Q5)
		for (int cell = 0; cell < count; cell++)
		{
			int link = m_aSeaLink[cell];
			if (link < 0 || m_aCellLand[cell] <= 0)
				continue;
			m_aReport.Insert(string.Format("Liaison par la mer (Q5) : %1 dans %2 <-> %3 dans %4", CellRefText(cell), ZoneText(m_aCellZone[cell]), CellRefText(link), ZoneText(m_aCellZone[link])));
		}

		m_aReport.Insert("");
		m_aReport.Insert(string.Format("AVERTISSEMENTS (%1)", m_aNotes.Count()));
		if (m_aNotes.IsEmpty())
			m_aReport.Insert("- aucun");
		foreach (string note : m_aNotes)
			m_aReport.Insert("- " + note);

		m_aReport.Insert("");
		m_aReport.Insert(string.Format("RETOUCHES : %1 appliquée(s), %2 refusée(s)", m_aApplied.Count(), m_aRefused.Count()));
		foreach (string applied : m_aApplied)
			m_aReport.Insert("- " + applied);
		foreach (string refused : m_aRefused)
			m_aReport.Insert("- " + refused);

		// Villages coupés : carrés à moins de m_fVillageCheckRadius du centre d'une localité, dans une autre zone
		m_aReport.Insert("");
		m_aReport.Insert(string.Format("VILLAGES COUPÉS (carrés dont le centre est à moins de %1 m du centre de la localité, dans une autre zone)", villageMeters));
		int cutVillages = 0;
		for (int k = 0; k < m_aLocalities.Count(); k++)
		{
			SRP_FrontLocality village = m_aLocalities[k];
			vector centre = LocalityCentre(k);
			string cells = "";
			for (int c2 = 0; c2 < count; c2++)
			{
				int cellZone = m_aCellZone[c2];
				if (cellZone < 0 || cellZone == village.m_iZone)
					continue;
				if (vector.DistanceXZ(CellCenter(c2), centre) >= m_Front.m_fVillageCheckRadius)
					continue;
				if (!cells.IsEmpty())
					cells += ", ";
				cells += CellRefText(c2) + " dans " + ZoneText(cellZone);
			}
			if (cells.IsEmpty())
				continue;
			cutVillages++;
			m_aReport.Insert("- " + village.m_sName + " (" + ZoneText(village.m_iZone) + ") : " + cells);
		}
		if (cutVillages == 0)
			m_aReport.Insert("- aucun");
	}

	//------------------------------------------------------------------------------------------------
	//! Source et carré d'un point clé pour le rapport — BuildReport
	protected string KeyPointText(int index)
	{
		if (index < 0 || index >= m_aKeyPoints.Count())
			return "MANQUANT";
		SRP_FrontKeyPoint point = m_aKeyPoints[index];
		string text = point.m_sSourceText;
		if (point.m_iSource == SRP_EKeyPointSource.NOM_CARTE)
			text += " (MANQUANT)";
		return text + ", carré " + CellRefText(point.m_iCell);
	}

	//------------------------------------------------------------------------------------------------
	//! 16. Rapport REPORT_PATH réécrit : une ligne par zone, points clés et sources, avertissements, retouches
	//! refusées, villages coupés (m_fVillageCheckRadius) — SRP_FrontComponent.Start après Build
	void WriteReport()
	{
		FileHandle file = FileIO.OpenFile(SRP_FrontComponent.REPORT_PATH, FileMode.WRITE);
		if (!file)
		{
			Print("[SRP] Front : impossible d'écrire " + SRP_FrontComponent.REPORT_PATH, LogLevel.WARNING);
			return;
		}

		if (m_aReport.IsEmpty())
		{
			// Construction interrompue (erreur fatale) : au moins les avertissements
			file.WriteLine("# Zones du front : construction interrompue au démarrage du " + SRP_Time.Now());
			foreach (string warning : m_aWarnings)
				file.WriteLine("- " + warning);
		}
		else
		{
			foreach (string line : m_aReport)
				file.WriteLine(line);
		}
		file.Close();
	}

	//================================================================================================
	// Outils internes
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Avertissement : liste de sortie, rapport et console
	protected void Warn(string text)
	{
		m_aWarnings.Insert(text);
		m_aNotes.Insert(text);
		Print("[SRP] Front : " + text, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	//! Erreur : comme un avertissement, préfixée ERREUR
	protected void Fail(string text)
	{
		string line = "ERREUR : " + text;
		m_aWarnings.Insert(line);
		m_aNotes.Insert(line);
		Print("[SRP] Front : " + text, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	//! Retouche refusée : « Retouche ligne 12 refusée : S05 serait coupée en deux »
	protected void Refuse(int lineNo, string reason)
	{
		string line = string.Format("Retouche ligne %1 refusée : %2", lineNo, reason);
		m_aWarnings.Insert(line);
		m_aRefused.Insert(line);
		Print("[SRP] Front : " + line, LogLevel.WARNING);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillInts(notnull array<int> values, int count, int value)
	{
		values.Clear();
		for (int i = 0; i < count; i++)
			values.Insert(value);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillBools(notnull array<bool> values, int count, bool value)
	{
		values.Clear();
		for (int i = 0; i < count; i++)
			values.Insert(value);
	}

	//------------------------------------------------------------------------------------------------
	//! Voisin par un côté dans la grille (dir 0 N, 1 E, 2 S, 3 O), -1 hors grille (en jeu ou non)
	protected int NeighbourOf(int cell, int dir)
	{
		int gx = cell % m_iN;
		int gz = cell / m_iN;
		if (dir == 0)
			gz++;
		else if (dir == 1)
			gx++;
		else if (dir == 2)
			gz--;
		else
			gx--;
		if (gx < 0 || gz < 0 || gx >= m_iN || gz >= m_iN)
			return -1;
		return gz * m_iN + gx;
	}

	//------------------------------------------------------------------------------------------------
	//! Carré sous une position, -1 hors grille
	protected int CellOf(vector pos)
	{
		int gx = Math.Floor(pos[0] / m_fCell);
		int gz = Math.Floor(pos[2] / m_fCell);
		if (gx < 0 || gz < 0 || gx >= m_iN || gz >= m_iN)
			return -1;
		return gz * m_iN + gx;
	}

	//------------------------------------------------------------------------------------------------
	//! Centre d'un carré dans le plan (y = 0)
	protected vector CellCenter(int cell)
	{
		int col = cell % m_iN;
		int row = cell / m_iN;
		float x = col * m_fCell + m_fCell * 0.5;
		float z = row * m_fCell + m_fCell * 0.5;
		return Vector(x, 0, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Centre du bloc d'un carré dans le plan (y = 0), l'ancre de numérotation (A6)
	protected vector BlockCenter(int cell)
	{
		float blockSize = m_iBlock * m_fCell;
		// Indices de bloc entiers calculés à part (division entière voulue)
		int blockCol = (cell % m_iN) / m_iBlock;
		int blockRow = (cell / m_iN) / m_iBlock;
		float x = blockCol * blockSize + blockSize * 0.5;
		float z = blockRow * blockSize + blockSize * 0.5;
		return Vector(x, 0, z);
	}

	//------------------------------------------------------------------------------------------------
	protected float GroundY(float x, float z)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;
		return world.GetSurfaceY(x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Carré en jeu hors base sous la position, sinon le plus proche sur les anneaux 1 à 5 (à égalité, le plus petit
	//! index) ; -1 si rien ; moved dit si l'on a dû chercher
	protected int PlayableCellNear(vector pos, out bool moved)
	{
		moved = false;
		int cell = CellOf(pos);
		if (cell >= 0 && m_aCellZone[cell] > 0)
			return cell;

		moved = true;
		int gx = Math.Floor(pos[0] / m_fCell);
		int gz = Math.Floor(pos[2] / m_fCell);
		for (int ring = 1; ring <= RING_MAX; ring++)
		{
			int best = -1;
			float bestDistance = 0;
			for (int dz = -ring; dz <= ring; dz++)
			{
				for (int dx = -ring; dx <= ring; dx++)
				{
					if (Math.AbsInt(dx) != ring && Math.AbsInt(dz) != ring)
						continue;
					int cx = gx + dx;
					int cz = gz + dz;
					if (cx < 0 || cz < 0 || cx >= m_iN || cz >= m_iN)
						continue;
					int candidate = cz * m_iN + cx;
					if (m_aCellZone[candidate] <= 0)
						continue;
					float distance = vector.DistanceXZ(pos, CellCenter(candidate));
					if (best < 0 || distance < bestDistance)
					{
						best = candidate;
						bestDistance = distance;
					}
				}
			}
			if (best >= 0)
				return best;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Position d'un point clé rattaché au carré cell, pour que le serveur (poste de commandement sur m_vPos, saisie
	//! sur m_iCell) et les joueurs (CellIndexAt sur x et z arrondis au mètre, SRP_FrontComponent.DoApplyGeometry)
	//! voient le même carré. Point déplacé (mer, base, hors grille) : le point de terre du carré le plus proche de
	//! l'origine, au sol (mêmes points que SampleLand). Sinon : la position d'origine, gardée à 1 m au moins des bords
	//! du carré — PlaceKeyPoints
	protected vector MovePointInto(vector pos, int cell, bool moved)
	{
		vector centre = CellCenter(cell);
		if (moved)
		{
			int samples = m_Front.m_iLandSamples;
			if (samples < 1)
				samples = 1;
			float step = m_fCell / samples;
			float x0 = centre[0] - m_fCell * 0.5;
			float z0 = centre[2] - m_fCell * 0.5;
			BaseWorld world = GetGame().GetWorld();
			bool ocean = false;
			if (world)
				ocean = world.IsOcean();
			float seaLevel = 0;
			if (ocean)
				seaLevel = world.GetOceanBaseHeight() + m_Front.m_fLandMinHeight;

			// Centre du carré si aucun point n'est de la terre (impossible pour un carré en jeu, sauf réglage changé)
			float bestX = centre[0];
			float bestZ = centre[2];
			float bestDistance = -1;
			for (int sz = 0; sz < samples; sz++)
			{
				float sampleZ = z0 + (sz + 0.5) * step;
				for (int sx = 0; sx < samples; sx++)
				{
					float sampleX = x0 + (sx + 0.5) * step;
					if (ocean && world.GetSurfaceY(sampleX, sampleZ) <= seaLevel)
						continue;
					float distance = vector.DistanceXZ(pos, Vector(sampleX, 0, sampleZ));
					if (bestDistance < 0 || distance < bestDistance)
					{
						bestX = sampleX;
						bestZ = sampleZ;
						bestDistance = distance;
					}
				}
			}
			return Vector(bestX, GroundY(bestX, bestZ), bestZ);
		}

		// Marge de 1 m : arrondis au mètre pour le réseau, x et z restent dans le même carré
		float half = m_fCell * 0.5 - 1;
		float x = Math.Clamp(pos[0], centre[0] - half, centre[0] + half);
		float z = Math.Clamp(pos[2], centre[2] - half, centre[2] + half);
		if (x == pos[0] && z == pos[2])
			return pos;
		// Même hauteur au-dessus du sol qu'à l'origine (repère posé sur une dalle, par exemple)
		float y = pos[1] + GroundY(x, z) - GroundY(pos[0], pos[2]);
		return Vector(x, y, z);
	}

	//------------------------------------------------------------------------------------------------
	//! « 7086 6012 » : x et z au mètre, pour les avertissements
	protected string PosText(vector pos)
	{
		int x = Math.Round(pos[0]);
		int z = Math.Round(pos[2]);
		return string.Format("%1 %2", x, z);
	}

	//------------------------------------------------------------------------------------------------
	//! Position du centre d'une localité : son point clé CENTRE s'il est posé, sinon son nom sur la carte
	protected vector LocalityCentre(int locality)
	{
		SRP_FrontLocality target = m_aLocalities[locality];
		if (target.m_iCentre >= 0 && target.m_iCentre < m_aKeyPoints.Count())
			return m_aKeyPoints[target.m_iCentre].m_vPos;
		return target.m_vLabel;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité de zone par son nom, sans tenir compte de la casse ; -1 sinon
	protected int FindLocalityByName(string name)
	{
		string wanted = name.Trim();
		wanted.ToLower();
		for (int i = 0; i < m_aLocalities.Count(); i++)
		{
			string candidate = m_aLocalities[i].m_sName;
			candidate.ToLower();
			if (candidate == wanted)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Localité de la base par son nom, sans tenir compte de la casse ; -1 sinon
	protected int FindHomeByName(string name)
	{
		string wanted = name.Trim();
		wanted.ToLower();
		for (int i = 0; i < m_aHomeName.Count(); i++)
		{
			string candidate = m_aHomeName[i];
			candidate.ToLower();
			if (candidate == wanted)
				return i;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Zone par son code (« S07 »), son nom ou son libellé (noms d'avant les retouches), ou la zone du nom d'une
	//! localité ; sans tenir compte de la casse ; -1 sinon — ApplyMoves, ApplyResources
	protected int FindZoneByText(string text)
	{
		string wanted = text.Trim();
		wanted.ToLower();
		if (wanted.IsEmpty())
			return -1;
		for (int i = 0; i < m_aZones.Count(); i++)
		{
			string code = m_aZones[i].m_sCode;
			code.ToLower();
			if (code == wanted)
				return i;
		}
		for (int j = 0; j < m_aZones.Count(); j++)
		{
			string zoneName = m_aZones[j].m_sName;
			zoneName.ToLower();
			string zoneLabel = m_aZones[j].GetLabel();
			zoneLabel.ToLower();
			if (zoneName == wanted || zoneLabel == wanted)
				return j;
		}
		int locality = FindLocalityByName(wanted);
		if (locality >= 0)
			return m_aLocalities[locality].m_iZone;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré touche-t-il la zone par un côté ?
	protected bool TouchesZone(int cell, int zone)
	{
		for (int dir = 0; dir < 4; dir++)
		{
			int nb = NeighbourOf(cell, dir);
			if (nb >= 0 && m_aCellZone[nb] == zone)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountZoneCells(int zone)
	{
		int total = 0;
		int count = m_iN * m_iN;
		for (int cell = 0; cell < count; cell++)
		{
			if (m_aCellZone[cell] == zone)
				total++;
		}
		return total;
	}

	//------------------------------------------------------------------------------------------------
	//! La zone reste-t-elle d'un seul tenant sans ce carré ? (parcours en largeur par les côtés) — ApplyMoves
	protected bool StaysConnected(int zone, int removed, int zoneCells)
	{
		int count = m_iN * m_iN;
		int start = -1;
		for (int cell = 0; cell < count; cell++)
		{
			if (cell != removed && m_aCellZone[cell] == zone)
			{
				start = cell;
				break;
			}
		}
		if (start < 0)
			return false;

		array<int> seen = {};
		FillInts(seen, count, 0);
		seen[start] = 1;
		m_aQueue.Clear();
		m_aQueue.Insert(start);
		int head = 0;
		while (head < m_aQueue.Count())
		{
			int at = m_aQueue[head];
			head++;
			for (int dir = 0; dir < 4; dir++)
			{
				int nb = NeighbourOf(at, dir);
				if (nb < 0 || nb == removed || seen[nb] != 0 || m_aCellZone[nb] != zone)
					continue;
				seen[nb] = 1;
				m_aQueue.Insert(nb);
			}
		}
		return m_aQueue.Count() == zoneCells - 1;
	}

	//------------------------------------------------------------------------------------------------
	//! « 074 042 » (ou « 75 43 ») vers un carré : gx = a x 100 / m_fCellSize (a / 2 pour 200 m) ; -1 si illisible
	protected int ParseCellRef(string text)
	{
		array<string> parts = {};
		text.Split(" ", parts, true);
		if (parts.Count() != 2)
			return -1;
		if (!SRP_Utils.IsNumeric(parts[0]) || !SRP_Utils.IsNumeric(parts[1]))
			return -1;
		int a = parts[0].ToInt();
		int b = parts[1].ToInt();
		int gx = Math.Floor(a * 100.0 / m_fCell);
		int gz = Math.Floor(b * 100.0 / m_fCell);
		if (gx < 0 || gz < 0 || gx >= m_iN || gz >= m_iN)
			return -1;
		return gz * m_iN + gx;
	}

	//------------------------------------------------------------------------------------------------
	//! Référence de carte du coin sud-ouest, « 074 042 » (A7, même format que SRP_Fleet.GridOf)
	protected string CellRefText(int cell)
	{
		if (cell < 0 || m_iN <= 0)
			return "???";
		int col = cell % m_iN;
		int row = cell / m_iN;
		int a = Math.Floor(col * m_fCell / 100);
		int b = Math.Floor(row * m_fCell / 100);
		return Pad3(a) + " " + Pad3(b);
	}

	//------------------------------------------------------------------------------------------------
	protected string Pad3(int value)
	{
		if (value < 0)
			value = 0;
		string text = value.ToString();
		while (text.Length() < 3)
			text = "0" + text;
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Code de zone : m_sCodePrefix + deux chiffres (S07), la base en 00
	protected string CodeOf(int index)
	{
		string prefix = m_Front.m_sCodePrefix;
		prefix = prefix.Trim();
		if (prefix.IsEmpty())
			prefix = "S";
		if (index < 10)
			return prefix + "0" + index.ToString();
		return prefix + index.ToString();
	}

	//------------------------------------------------------------------------------------------------
	protected string BaseName()
	{
		string name = m_Front.m_sBaseZoneName;
		name = name.Trim();
		if (name.IsEmpty())
			return "Base";
		return name;
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé d'une zone pour les messages, « Régina (S07) »
	protected string ZoneText(int zone)
	{
		if (zone < 0 || zone >= m_aZones.Count())
			return "hors zone";
		return m_aZones[zone].GetLabel();
	}

	//------------------------------------------------------------------------------------------------
	//! Genre d'une localité : un bourg compte comme une ville (SRP_Territory.c:669-676)
	protected int KindOf(string placeKind)
	{
		string kind = placeKind;
		kind.ToLower();
		if (kind == "hameau")
			return SRP_EZoneKind.HAMEAU;
		if (kind == "ville" || kind == "bourg")
			return SRP_EZoneKind.VILLE;
		return SRP_EZoneKind.VILLAGE;
	}

	//------------------------------------------------------------------------------------------------
	protected string KindText(int kind)
	{
		if (kind == SRP_EZoneKind.HAMEAU)
			return "hameau";
		if (kind == SRP_EZoneKind.VILLE)
			return "ville";
		return "village";
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne nettoyée : tabulations en espaces, commentaire # retiré, espaces de bord retirés
	protected string CleanLine(string line)
	{
		string text = line;
		text.Replace("\t", " ");
		int hash = text.IndexOf("#");
		if (hash == 0)
			return "";
		if (hash > 0)
			text = text.Substring(0, hash);
		return text.Trim();
	}

	//------------------------------------------------------------------------------------------------
	//! Au moins une lettre ou un chiffre (ignore une ligne qui ne porte qu'une marque d'encodage)
	protected bool HasLetterOrDigit(string text)
	{
		for (int i = 0; i < text.Length(); i++)
		{
			if (text.IsDigitAt(i))
				return true;
			string ch = text.Get(i);
			string upper = ch;
			upper.ToUpper();
			string lower = ch;
			lower.ToLower();
			if (upper != lower)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Premier mot d'une ligne, en minuscules
	protected string FirstWord(string text)
	{
		array<string> words = {};
		text.Split(" ", words, true);
		if (words.IsEmpty())
			return "";
		string first = words[0];
		first.ToLower();
		return first;
	}

	//------------------------------------------------------------------------------------------------
	//! Mots first à last (inclus) séparés par une espace ; "" si l'intervalle est vide
	protected string JoinWords(notnull array<string> words, int first, int last)
	{
		string text = "";
		for (int i = first; i <= last && i < words.Count(); i++)
		{
			if (i < 0)
				continue;
			if (!text.IsEmpty())
				text += " ";
			text += words[i];
		}
		return text;
	}
}
