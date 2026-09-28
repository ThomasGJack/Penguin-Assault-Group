//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : CALQUE DE LA CARTE DU JEU (I1 à I3), côté JOUEUR (cartes #75 et #76).
//
// RÔLE : dessiner sur les cartes plein écran, de réapparition et du Game Master : carrés bleus, rouges (très
// transparents, I2) et orange (contestés, C8), trait de front épais, limites et noms des zones (« Régina (S07) » ou
// « S07 » selon le zoom), contour des zones attaquées. Aucun chiffre sur la carte (trou 20). Un carré en train d'être
// pris ou repris CLIGNOTE orange, bleu, orange, rouge (choix de Jack du 27/09) ; figé au combat, il reste orange.
// Survol (27/09) : le carré sous le curseur ressort (contour blanc, ombre décalée, reflet qui respire), sur un second
// canevas de 3 commandes redessiné seul, sans toucher au calque.
// Il ne fait que LIRE SRP_FrontComponent sur la machine du joueur (copie reçue par RplLoad + RPC) ; ses réglages sont
// les attributs publics de SRP_FrontComponent, catégorie « SimpleRP - Front : carte » (identiques partout : prefab).
// Accrochage : modded SCR_MapEntity (aucun .conf ni .layout touché) ; un CanvasWidget à nous, jamais le
// DrawingWidget vanilla (SCR_MapSelectionModule y réécrit ses commandes à chaque image).
// Coût au repos quasi nul : la géométrie en mètres n'est refaite que si la version d'état change, la projection
// écran seulement au pan, au zoom ou au changement de taille. Aucune touche ajoutée : tout marche à la manette
// (le canevas ignore le curseur et ne prend jamais le focus).
// APPELÉ PAR : le moteur (OnMapOpen, OnMapClose, UpdateMap de SCR_MapEntity, vérifiés SCR_MapEntity.c:341, 375, 1483).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Défaut du jeu de base (plantage du Workbench le 27/09, SCR_MapMarkerWidgetComponent.c:433) : un repère de carte
//! détruit après l'arrêt du jeu appelle GetGame().GetCallqueue().Remove sur une file déjà partie. Sans file, rien à
//! retirer : on saute seulement ce retrait.
modded class SCR_MapMarkerWidgetComponent
{
	override void HandlerDeattached(Widget w)
	{
		if (!GetGame() || !GetGame().GetCallqueue())
			return;
		super.HandlerDeattached(w);
	}
}

//------------------------------------------------------------------------------------------------
//! Accrochage du calque. Tout champ ou méthode ajouté porte SRP_ (classe moddée).
modded class SCR_MapEntity
{
	protected ref SRP_FrontMapLayer m_SRP_FrontLayer;

	//------------------------------------------------------------------------------------------------
	//! Crée le calque au besoin et l'attache AVANT super (les repères vanilla, créés ensuite, passent au-dessus) —
	//! le moteur
	override protected void OnMapOpen(MapConfiguration config)
	{
		if (!m_SRP_FrontLayer)
			m_SRP_FrontLayer = new SRP_FrontMapLayer();

		m_SRP_FrontLayer.Attach(this, config);
		super.OnMapOpen(config);
	}

	//------------------------------------------------------------------------------------------------
	//! Détache le calque (la géométrie est gardée pour la prochaine ouverture) — le moteur
	override protected void OnMapClose()
	{
		if (m_SRP_FrontLayer)
			m_SRP_FrontLayer.Detach();

		super.OnMapClose();
	}

	//------------------------------------------------------------------------------------------------
	//! super (pan et zoom de l'image déjà appliqués), puis m_SRP_FrontLayer.Update(this) — le moteur, chaque image
	override protected void UpdateMap(float timeSlice)
	{
		super.UpdateMap(timeSlice);

		if (m_SRP_FrontLayer)
			m_SRP_FrontLayer.Update(this);
	}

	//------------------------------------------------------------------------------------------------
	//! Monde vers pixels du canevas, sans arrondi, même formule que WorldToScreen(…, withPan = true) :
	//! x écran = x x scale + offsetX ; y écran = offsetY - z x scale, avec scale = m_fZoomPPU,
	//! offsetX = m_Workspace.DPIScale(m_iPanX) - m_iMapOffsetX x m_fZoomPPU,
	//! offsetY = m_Workspace.DPIScale(m_iPanY) + (m_iMapSizeY + m_iMapOffsetY) x m_fZoomPPU — SRP_FrontMapLayer.Update
	void SRP_GetScreenTransform(out float scale, out float offsetX, out float offsetY)
	{
		// m_Workspace n'est posé qu'à la première ouverture (OpenMap) : repli sur l'espace de travail du jeu
		WorkspaceWidget workspace = m_Workspace;
		if (!workspace)
			workspace = GetGame().GetWorkspace();

		float panX = m_iPanX;
		float panY = m_iPanY;
		if (workspace)
		{
			panX = workspace.DPIScale(m_iPanX);
			panY = workspace.DPIScale(m_iPanY);
		}

		scale = m_fZoomPPU;
		offsetX = panX - m_iMapOffsetX * m_fZoomPPU;
		offsetY = panY + (m_iMapSizeY + m_iMapOffsetY) * m_fZoomPPU;
	}
}

//------------------------------------------------------------------------------------------------
//! Le calque du front d'une carte ouverte (joueur). Tous les tableaux de commandes sont des ref : SetDrawCommands ne
//! garde qu'un pointeur (CanvasWidget.c:14-24).
class SRP_FrontMapLayer
{
	// Genres de traits (m_aChainKind) : mêmes valeurs que SRP_EFrontAttack pour les contours d'attaque
	protected static const int KIND_BORDER = 0;			// limite de zone
	protected static const int KIND_ATTACK_WARN = 1;		// contour d'une attaque annoncée
	protected static const int KIND_ATTACK = 2;			// contour d'une attaque en cours
	protected static const int KIND_FRONT = 3;				// trait de front

	// Classe d'un carré pour le remplissage : SRP_EFrontOwner (0 rouge, 1 bleu), 2 contesté figé (combat, orange fixe),
	// 3 en cours de prise ou de reprise (clignote), -1 hors jeu
	protected static const int CLASS_CONTESTED = 2;
	protected static const int CLASS_BLINK = 3;
	protected static const int CLASS_COUNT = 4;
	protected static const int BLINK_MS = 500;				// durée de chaque couleur du clignotement

	// Directions depuis un coin de la grille
	protected static const int DIR_N = 0;					// +z
	protected static const int DIR_E = 1;					// +x
	protected static const int DIR_S = 2;					// -z
	protected static const int DIR_W = 3;					// -x

	protected CanvasWidget m_wCanvas;										// notre canevas, null carte fermée
	protected ref array<ref CanvasWidgetCommand> m_aCommands = {};		// commandes de l'image
	protected ref array<ref PolygonDrawCommand> m_aPolys = {};			// réserve réutilisée
	protected ref array<ref LineDrawCommand> m_aLines = {};				// réserve réutilisée
	protected ref array<ref TextDrawCommand> m_aTexts = {};				// réserve réutilisée
	protected ref array<Widget> m_aTextWidgets = {};						// repli TextWidget (m_bMapTextWidgetFallback)
	// Survol : second canevas juste au-dessus du calque, 3 commandes (reflet, ombre, contour)
	protected CanvasWidget m_wHover;
	protected ref array<ref CanvasWidgetCommand> m_aHoverCommands = {};
	protected ref PolygonDrawCommand m_HoverFill;
	protected ref LineDrawCommand m_HoverShadow;
	protected ref LineDrawCommand m_HoverLine;
	protected ref array<int> m_aHoverFillColors = {};						// 16 reflets blancs, du plus léger au plus fort
	protected int m_iHoverCell = -1;										// carré survolé dessiné, -1 aucun
	protected int m_iHoverDrawMs;
	protected int m_iHoverStep = -1;										// marche de reflet dessinée (0 à 15)
	protected float m_fHoverScale;
	protected float m_fHoverOffX;
	protected float m_fHoverOffY;
	protected ref array<PolygonDrawCommand> m_aBlinkPolys = {};			// commandes des carrés qui clignotent (tenues par m_aPolys)
	protected int m_iBlinkPhase = -1;									// 0 orange, 1 bleu, 2 orange, 3 rouge

	// Géométrie en mètres, refaite quand la version change
	protected ref array<float> m_aRect = {};								// x0, z0, x1, z1 par rectangle
	protected ref array<int> m_aRectColor = {};							// 0 rouge, 1 bleu, 2 orange fixe, 3 clignote
	protected ref array<ref array<float>> m_aChain = {};					// polylignes x, z…
	protected ref array<int> m_aChainKind = {};							// 0 limite, 1 attaque annoncée, 2 attaque, 3 front
	protected ref array<float> m_aChainBox = {};							// xmin, zmin, xmax, zmax par tronçon
	protected ref array<bool> m_aChainClosed = {};
	protected ref array<vector> m_aLabelPos = {};
	protected ref array<string> m_aLabelFull = {};							// « Régina (S07) »
	protected ref array<string> m_aLabelShort = {};						// « S07 »
	protected ref array<int> m_aDeg = {};									// degré de chaque coin dans la passe en cours (chaînage)
	protected ref array<int> m_aEdgeAt = {};								// arêtes par coin (4 par coin, N E S O) : 1 = arête pas encore parcourue

	// Tables de travail de BuildWorld (réutilisées, jamais réallouées tant que la grille ne change pas)
	protected ref array<int> m_aCellOwn = {};								// propriétaire de chaque carré, -1 hors jeu
	protected ref array<int> m_aCellZn = {};								// zone de chaque carré, -1 hors jeu
	protected ref array<int> m_aCellCls = {};								// classe de remplissage, -1 hors jeu
	protected ref array<int> m_aOpenRow = {};								// table openRuns : dernière rangée de la suite (clé)
	protected ref array<int> m_aOpenRect = {};								// table openRuns : rectangle de la suite (clé)
	protected ref array<int> m_aTouched = {};								// coins touchés par la passe de chaînage
	protected ref array<int> m_aWalk = {};									// coins d'une polyligne en cours

	// Couleurs empaquetées (Color.PackToInt), prises à l'attache
	protected int m_iBlueFill;
	protected int m_iRedFill;
	protected int m_iContestedFill;
	protected int m_iBlinkOrange;
	protected int m_iBlinkBlue;
	protected int m_iBlinkRed;
	protected int m_iFrontColor;
	protected int m_iFrontOutline;
	protected int m_iZoneBorder;
	protected int m_iAttackWarn;
	protected int m_iAttack;
	protected int m_iLabelColor;

	// Transformation et état
	protected int m_iVersion = -1;						// GetStateVersion + GetLiveVersion vus au dernier BuildWorld
	protected int m_iGeometryVersion = -1;
	protected int m_iChainVersion = -1;					// GetStateVersion vu au dernier tracé des traits et des noms
	protected float m_fScale = -1;
	protected float m_fOffX;
	protected float m_fOffY;
	protected float m_fW;
	protected float m_fH;
	protected bool m_bDirty = true;
	protected int m_iLastDraw;							// GetTickCount du dernier retracé
	protected int m_iLastDiag;							// GetTickCount du dernier message de durée (diagnostic)

	// Liens et grille
	protected SRP_FrontComponent m_Front;				// le socle lu (lien faible : il appartient au mode de jeu)
	protected Widget m_wParent;							// parent du canevas (et des TextWidget de repli)
	protected int m_iCanvasZ;							// rang du canevas
	protected int m_iGridN;								// côté de la grille lue, en carrés
	protected float m_fCell;							// taille d'un carré, en mètres
	protected int m_iMaxPts = 190;						// points par tronçon (m_iMapMaxPointsPerLine, borné)

	// Calage (m_bMapCalibration)
	protected string m_sCalibText;
	protected float m_fCalibX;
	protected float m_fCalibY;

	//================================================================================================
	// Cycle de vie
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Rien si m_bMapLayer est faux ou si le mode n'est pas autorisé (FULLSCREEN, SPAWNSCREEN, PLAIN, EDITOR selon les
	//! réglages) ; canevas VISIBLE | BLEND | IGNORE_CURSOR | NOFOCUS dans MapFrame (ou m_sMapLayerParent), plein cadre,
	//! rang holder + m_iMapZOrderOffset ; diagnostic m_bMapDiag ; rend vrai si attaché — SCR_MapEntity.OnMapOpen
	bool Attach(SCR_MapEntity mapEntity, MapConfiguration config)
	{
		// une ouverture sans fermeture propre ne laisse jamais deux canevas
		Detach();

		if (!mapEntity || !config)
			return false;

		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.m_bMapLayer)
			return false;

		if (!IsModeAllowed(front, config.MapEntityMode))
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		CanvasWidget mapWidget = mapEntity.GetMapWidget();
		Widget root = mapEntity.GetMapMenuRoot();
		if (!root)
			root = config.RootWidgetRef;

		// Parent : le nom imposé par le réglage, sinon MapFrame (là où les repères vanilla créent leurs widgets,
		// SCR_MapMarkerBase.c:357-364), sinon le parent de la carte
		Widget parent;
		if (root && front.m_sMapLayerParent != "")
			parent = root.FindAnyWidget(front.m_sMapLayerParent);
		if (!parent && root)
			parent = root.FindAnyWidget(SCR_MapConstants.MAP_FRAME_NAME);
		if (!parent && mapWidget)
			parent = mapWidget.GetParent();
		if (!parent)
			return false;

		// L'ancêtre de la carte au rang du parent donne le rang de référence
		Widget holder = mapWidget;
		while (holder && holder.GetParent() != parent)
		{
			holder = holder.GetParent();
		}

		int zOrder = front.m_iMapZOrderOffset;
		if (holder)
			zOrder = holder.GetZOrder() + front.m_iMapZOrderOffset;

		Widget created = workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), zOrder, parent);
		m_wCanvas = CanvasWidget.Cast(created);
		if (!m_wCanvas)
		{
			if (created)
				created.RemoveFromHierarchy();
			return false;
		}

		// Plein cadre du parent : l'origine du canevas est celle des repères vanilla (WorldToScreen avec le pan)
		FrameSlot.SetAnchorMin(m_wCanvas, 0, 0);
		FrameSlot.SetAnchorMax(m_wCanvas, 1, 1);
		FrameSlot.SetOffsets(m_wCanvas, 0, 0, 0, 0);
		m_wCanvas.SetName("SRP_FrontLayer");
		m_wCanvas.SetZOrder(zOrder);

		m_wParent = parent;
		m_iCanvasZ = zOrder;

		// Survol : second canevas juste au-dessus, redessiné seul (le calque n'est pas touché)
		m_wHover = null;
		m_iHoverCell = -1;
		if (front.m_bMapHover)
		{
			Widget hoverCreated = workspace.CreateWidget(WidgetType.CanvasWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), zOrder + 1, parent);
			m_wHover = CanvasWidget.Cast(hoverCreated);
			if (m_wHover)
			{
				FrameSlot.SetAnchorMin(m_wHover, 0, 0);
				FrameSlot.SetAnchorMax(m_wHover, 1, 1);
				FrameSlot.SetOffsets(m_wHover, 0, 0, 0, 0);
				m_wHover.SetName("SRP_FrontHover");
				m_wHover.SetZOrder(zOrder + 1);
				PrepareHover();
			}
			else if (hoverCreated)
			{
				hoverCreated.RemoveFromHierarchy();
			}
		}

		// autre instance du mode de jeu depuis la dernière ouverture : la géométrie gardée n'est plus la bonne
		if (front != m_Front)
		{
			m_iVersion = -1;
			m_iGeometryVersion = -1;
			m_iChainVersion = -1;
		}

		m_Front = front;

		// Couleurs des réglages, avec les valeurs de Jack si un attribut manque
		m_iBlueFill = PackOr(front.m_MapBlueFill, 0.16, 0.42, 0.85, 0.20);
		m_iRedFill = PackOr(front.m_MapRedFill, 0.85, 0.18, 0.18, 0.20);
		m_iContestedFill = PackOr(front.m_MapContestedFill, 1, 0.55, 0.1, 0.40);
		// Clignotement : couleurs plus soutenues que les remplissages pour se voir sur la carte
		m_iBlinkOrange = PackOr(null, 1, 0.55, 0.1, 0.60);
		m_iBlinkBlue = PackOr(null, 0.16, 0.42, 0.85, 0.60);
		m_iBlinkRed = PackOr(null, 0.85, 0.18, 0.18, 0.60);
		m_iFrontColor = PackOr(front.m_MapFrontColor, 0.08, 0.08, 0.08, 0.90);
		m_iFrontOutline = PackOr(front.m_MapFrontOutlineColor, 1, 1, 1, 0.55);
		m_iZoneBorder = PackOr(front.m_MapZoneBorderColor, 0.1, 0.1, 0.1, 0.35);
		m_iAttackWarn = PackOr(front.m_MapAttackWarnColor, 1, 0.55, 0.1, 0.9);
		m_iAttack = PackOr(front.m_MapAttackColor, 0.9, 0.1, 0.1, 0.9);
		m_iLabelColor = PackOr(front.m_MapLabelColor, 0.08, 0.08, 0.08, 0.85);

		// Premier tracé à la première image (la géométrie en mètres est gardée si la version n'a pas changé)
		m_fScale = -1;
		m_bDirty = true;
		m_iLastDraw = 0;

		if (front.m_bMapDiag)
		{
			Print(string.Format("[SRP Front] calque de carte attaché, mode %1, rang %2", config.MapEntityMode, zOrder), LogLevel.NORMAL);
			LogSiblings(parent);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! RemoveFromHierarchy du canevas (et des TextWidget de repli), m_fScale = -1 — SCR_MapEntity.OnMapClose
	void Detach()
	{
		if (m_wCanvas)
			m_wCanvas.RemoveFromHierarchy();
		if (m_wHover)
			m_wHover.RemoveFromHierarchy();
		m_wHover = null;
		m_aHoverCommands.Clear();
		m_iHoverCell = -1;

		for (int i = 0; i < m_aTextWidgets.Count(); i++)
		{
			Widget label = m_aTextWidgets[i];
			if (label)
				label.RemoveFromHierarchy();
		}

		m_aTextWidgets.Clear();
		m_aCommands.Clear();
		m_wCanvas = null;
		m_wParent = null;
		m_fScale = -1;
		m_bDirty = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque image carte ouverte : front prêt ? version changée -> BuildWorld ; transformation ou taille changée ->
	//! retracé (BuildScreen puis SetDrawCommands) au plus toutes les m_iMapRedrawMinMs — SCR_MapEntity.UpdateMap
	void Update(SCR_MapEntity mapEntity)
	{
		if (!m_wCanvas || !mapEntity)
			return;

		// Joueur qui arrive : rien tant que l'instantané du front n'est pas reçu
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (!front || !front.IsReady())
			return;

		if (front != m_Front)
		{
			// autre instance du mode de jeu : tout est à refaire
			m_Front = front;
			m_iVersion = -1;
			m_iGeometryVersion = -1;
			m_iChainVersion = -1;
		}

		int version = front.GetStateVersion() + front.GetLiveVersion();
		if (version != m_iVersion || front.GetGeometryVersion() != m_iGeometryVersion)
		{
			BuildWorld(front);
			m_bDirty = true;
		}

		float scale, offsetX, offsetY;
		mapEntity.SRP_GetScreenTransform(scale, offsetX, offsetY);

		float width, height;
		m_wCanvas.GetScreenSize(width, height);
		if (width <= 0 || height <= 0)
		{
			// canevas pas encore mis en page : la carte donne le même cadre
			CanvasWidget mapWidget = mapEntity.GetMapWidget();
			if (mapWidget)
				mapWidget.GetScreenSize(width, height);
		}

		if (scale != m_fScale || offsetX != m_fOffX || offsetY != m_fOffY || width != m_fW || height != m_fH)
		{
			m_fScale = scale;
			m_fOffX = offsetX;
			m_fOffY = offsetY;
			m_fW = width;
			m_fH = height;
			m_bDirty = true;
		}

		// Survol : le carré sous le curseur ressort (second canevas, à chaque image)
		UpdateHover(mapEntity, front);

		// Carrés en cours de prise ou de reprise : couleur suivante du clignotement, sans rien recalculer
		int blinkPhase = (System.GetTickCount() / BLINK_MS) % 4;
		if (blinkPhase != m_iBlinkPhase)
		{
			m_iBlinkPhase = blinkPhase;
			if (!m_bDirty && !m_aBlinkPolys.IsEmpty())
			{
				int blinkColor = BlinkColor();
				foreach (PolygonDrawCommand blinkPoly : m_aBlinkPolys)
				{
					if (blinkPoly)
						blinkPoly.m_iColor = blinkColor;
				}
				m_wCanvas.SetDrawCommands(m_aCommands);
			}
		}

		if (!m_bDirty)
			return;

		int now = System.GetTickCount();
		if (front.m_iMapRedrawMinMs > 0 && m_iLastDraw != 0 && now - m_iLastDraw < front.m_iMapRedrawMinMs)
			return;	// reste à retracer : l'image suivante le fera

		BuildScreen();
		m_wCanvas.SetDrawCommands(m_aCommands);
		m_bDirty = false;
		m_iLastDraw = now;

		if (front.m_bMapDiag)
		{
			int spent = System.GetTickCount() - now;
			if (spent > 2 && now - m_iLastDiag > 5000)
			{
				m_iLastDiag = now;
				Print(string.Format("[SRP Front] retracé du calque : %1 ms, %2 commandes (m_iMapRedrawMinMs peut limiter la cadence)", spent, m_aCommands.Count()), LogLevel.WARNING);
			}
		}
	}

	//================================================================================================
	// Géométrie en mètres
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Géométrie en mètres : rectangles fusionnés par rangée puis en hauteur (table openRuns), arêtes front / limites
	//! / attaque, chaînage (coins impairs d'abord, damier tourné côté bleu), découpe à m_iMapMaxPointsPerLine points,
	//! étiquettes (GetZoneLabelPos, GetZoneLabel, GetZoneCode) — Update
	protected void BuildWorld(SRP_FrontComponent front)
	{
		int stateVersion = front.GetStateVersion();
		int geometryVersion = front.GetGeometryVersion();

		// Les traits et les noms ne dépendent que des propriétaires, des zones et des drapeaux d'attaque : un simple
		// changement d'état contesté (orange) ne refait que les rectangles
		bool chainsNeeded = (stateVersion != m_iChainVersion || geometryVersion != m_iGeometryVersion);

		m_iVersion = stateVersion + front.GetLiveVersion();
		m_iGeometryVersion = geometryVersion;
		m_iChainVersion = stateVersion;

		int n = front.GetGridSize();
		float cellSize = front.GetCellSize();
		if (n <= 0 || n > 256 || cellSize <= 0)
		{
			ClearWorld();
			return;
		}

		if (n != m_iGridN || cellSize != m_fCell)
			chainsNeeded = true;

		m_iGridN = n;
		m_fCell = cellSize;
		m_iMaxPts = Math.ClampInt(front.m_iMapMaxPointsPerLine, 2, 200);	// 200 points = 400 flottants au plus

		ReadCells(front, chainsNeeded);
		BuildRects();

		if (chainsNeeded)
		{
			BuildChains(front);
			BuildLabels(front);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Oublie toute la géométrie (grille illisible) ; la prochaine grille lisible sera relue en entier — BuildWorld
	protected void ClearWorld()
	{
		m_iGridN = 0;
		m_aRect.Clear();
		m_aRectColor.Clear();
		m_aChain.Clear();
		m_aChainKind.Clear();
		m_aChainBox.Clear();
		m_aChainClosed.Clear();
		m_aLabelPos.Clear();
		m_aLabelFull.Clear();
		m_aLabelShort.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Copie locale des carrés : propriétaire réel (le front se calcule dessus, jamais sur l'orange, C8), zone et
	//! classe de remplissage ; full faux = seul l'état contesté a changé, propriétaires et zones sont gardés — BuildWorld
	protected void ReadCells(SRP_FrontComponent front, bool full)
	{
		int count = m_iGridN * m_iGridN;
		if (m_aCellCls.Count() != count)
		{
			m_aCellCls.Resize(count);
			m_aCellOwn.Resize(count);
			m_aCellZn.Resize(count);
			full = true;
		}

		if (!full)
		{
			for (int live = 0; live < count; live++)
			{
				int kept = m_aCellOwn[live];
				if (kept < 0)
					continue;

				m_aCellCls[live] = CellClass(front, live, kept);
			}
			return;
		}

		for (int cell = 0; cell < count; cell++)
		{
			int owner = front.GetCellOwner(cell);
			if (owner != SRP_EFrontOwner.ROUGE && owner != SRP_EFrontOwner.BLEU)
			{
				m_aCellOwn[cell] = -1;
				m_aCellZn[cell] = -1;
				m_aCellCls[cell] = -1;
				continue;
			}

			m_aCellOwn[cell] = owner;
			m_aCellZn[cell] = front.GetCellZone(cell);
			m_aCellCls[cell] = CellClass(front, cell, owner);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! a) Remplissage : suites de même classe par rangée, prolongées vers le nord quand la rangée précédente avait
	//! exactement la même suite (même début, même fin, même classe) ; table openRuns, clé classe x N² + gx x N + fin
	//! — BuildWorld
	protected void BuildRects()
	{
		m_aRect.Clear();
		m_aRectColor.Clear();

		int n = m_iGridN;
		int keys = CLASS_COUNT * n * n;
		if (m_aOpenRow.Count() != keys)
		{
			m_aOpenRow.Resize(keys);
			m_aOpenRect.Resize(keys);
		}

		for (int k = 0; k < keys; k++)
		{
			m_aOpenRow[k] = -2;
			m_aOpenRect[k] = -1;
		}

		float s = m_fCell;
		for (int gz = 0; gz < n; gz++)
		{
			int rowStart = gz * n;
			int gx = 0;
			while (gx < n)
			{
				int cls = m_aCellCls[rowStart + gx];
				if (cls < 0)
				{
					gx++;
					continue;
				}

				int last = gx;
				while (last + 1 < n && m_aCellCls[rowStart + last + 1] == cls)
				{
					last++;
				}

				int key = cls * n * n + gx * n + last;
				int rect = -1;
				if (m_aOpenRow[key] == gz - 1)
					rect = m_aOpenRect[key];

				if (rect >= 0)
				{
					// même suite juste au sud : le rectangle grandit d'un carré vers le nord
					m_aRect[rect * 4 + 3] = (gz + 1) * s;
				}
				else
				{
					rect = m_aRectColor.Count();
					m_aRect.Insert(gx * s);
					m_aRect.Insert(gz * s);
					m_aRect.Insert((last + 1) * s);
					m_aRect.Insert((gz + 1) * s);
					m_aRectColor.Insert(cls);
				}

				m_aOpenRow[key] = gz;
				m_aOpenRect[key] = rect;
				gx = last + 1;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! b) à d) Traits : limites de zones, contour de chaque zone attaquée, front ; une passe de chaînage par genre (et
	//! par zone attaquée, pour que deux zones voisines ne partagent pas une arête) — BuildWorld
	protected void BuildChains(SRP_FrontComponent front)
	{
		m_aChain.Clear();
		m_aChainKind.Clear();
		m_aChainBox.Clear();
		m_aChainClosed.Clear();

		int corners = (m_iGridN + 1) * (m_iGridN + 1);
		if (m_aDeg.Count() != corners)
		{
			m_aDeg.Resize(corners);
			m_aEdgeAt.Resize(corners * 4);
			for (int c = 0; c < corners; c++)
			{
				m_aDeg[c] = 0;
			}
			for (int e = 0; e < corners * 4; e++)
			{
				m_aEdgeAt[e] = 0;
			}
			m_aTouched.Clear();
		}

		if (front.m_bMapZoneBorders)
		{
			CollectBorderEdges();
			ChainAll(KIND_BORDER, -1);
		}

		if (front.m_bMapAttackOutline)
		{
			int zones = front.GetZoneCount();
			for (int zone = 0; zone < zones; zone++)
			{
				int state = front.GetZoneAttackState(zone);
				if (state != SRP_EFrontAttack.ANNONCEE && state != SRP_EFrontAttack.ASSAUT)
					continue;

				CollectZoneEdges(zone);
				if (state == SRP_EFrontAttack.ASSAUT)
					ChainAll(KIND_ATTACK, zone);
				else
					ChainAll(KIND_ATTACK_WARN, zone);
			}
		}

		CollectFrontEdges();
		ChainAll(KIND_FRONT, -1);
	}

	//------------------------------------------------------------------------------------------------
	//! Limites : entre deux carrés en jeu voisins (à l'est et au nord) de zones différentes — BuildChains
	protected void CollectBorderEdges()
	{
		int n = m_iGridN;
		for (int gz = 0; gz < n; gz++)
		{
			for (int gx = 0; gx < n; gx++)
			{
				int cell = gz * n + gx;
				int zone = m_aCellZn[cell];
				if (zone < 0)
					continue;

				if (gx + 1 < n)
				{
					int east = m_aCellZn[cell + 1];
					if (east >= 0 && east != zone)
						AddEdge(CornerAt(gx + 1, gz), DIR_N);
				}

				if (gz + 1 < n)
				{
					int north = m_aCellZn[cell + n];
					if (north >= 0 && north != zone)
						AddEdge(CornerAt(gx, gz + 1), DIR_E);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Front : entre deux carrés en jeu voisins de propriétaires différents (propriétaire réel, sans la liaison
	//! maritime) — BuildChains
	protected void CollectFrontEdges()
	{
		int n = m_iGridN;
		for (int gz = 0; gz < n; gz++)
		{
			for (int gx = 0; gx < n; gx++)
			{
				int cell = gz * n + gx;
				int owner = m_aCellOwn[cell];
				if (owner < 0)
					continue;

				if (gx + 1 < n)
				{
					int east = m_aCellOwn[cell + 1];
					if (east >= 0 && east != owner)
						AddEdge(CornerAt(gx + 1, gz), DIR_N);
				}

				if (gz + 1 < n)
				{
					int north = m_aCellOwn[cell + n];
					if (north >= 0 && north != owner)
						AddEdge(CornerAt(gx, gz + 1), DIR_E);
				}
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Contour d'une zone attaquée : toute arête entre un de ses carrés et ce qui n'est pas elle (autre zone, mer,
	//! bord de la grille) — BuildChains
	protected void CollectZoneEdges(int zone)
	{
		int n = m_iGridN;
		for (int gz = 0; gz < n; gz++)
		{
			for (int gx = 0; gx < n; gx++)
			{
				int cell = gz * n + gx;
				if (m_aCellZn[cell] != zone)
					continue;

				bool openNorth = true;
				if (gz + 1 < n)
					openNorth = (m_aCellZn[cell + n] != zone);
				if (openNorth)
					AddEdge(CornerAt(gx, gz + 1), DIR_E);

				bool openSouth = true;
				if (gz > 0)
					openSouth = (m_aCellZn[cell - n] != zone);
				if (openSouth)
					AddEdge(CornerAt(gx, gz), DIR_E);

				bool openEast = true;
				if (gx + 1 < n)
					openEast = (m_aCellZn[cell + 1] != zone);
				if (openEast)
					AddEdge(CornerAt(gx + 1, gz), DIR_N);

				bool openWest = true;
				if (gx > 0)
					openWest = (m_aCellZn[cell - 1] != zone);
				if (openWest)
					AddEdge(CornerAt(gx, gz), DIR_N);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Index d'un coin de la grille (0 à N sur chaque axe) : cz x (N + 1) + cx
	protected int CornerAt(int cx, int cz)
	{
		return cz * (m_iGridN + 1) + cx;
	}

	//------------------------------------------------------------------------------------------------
	//! Écart d'index vers le coin voisin dans une direction
	protected int CornerStep(int dir)
	{
		if (dir == DIR_N)
			return m_iGridN + 1;
		if (dir == DIR_E)
			return 1;
		if (dir == DIR_S)
			return -(m_iGridN + 1);
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Direction opposée (N <-> S, E <-> O)
	protected static int Opposite(int dir)
	{
		if (dir < 2)
			return dir + 2;
		return dir - 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Ajoute une arête d'un coin vers son voisin (adjacence à plat, notée aux deux bouts) — Collect*Edges
	protected void AddEdge(int corner, int dir)
	{
		if (m_aEdgeAt[corner * 4 + dir] == 1)
			return;

		int other = corner + CornerStep(dir);
		m_aEdgeAt[corner * 4 + dir] = 1;
		m_aEdgeAt[other * 4 + Opposite(dir)] = 1;

		if (m_aDeg[corner] == 0)
			m_aTouched.Insert(corner);
		m_aDeg[corner] = m_aDeg[corner] + 1;

		if (m_aDeg[other] == 0)
			m_aTouched.Insert(other);
		m_aDeg[other] = m_aDeg[other] + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Arêtes pas encore parcourues à un coin
	protected int RemainingDegree(int corner)
	{
		int slot = corner * 4;
		return m_aEdgeAt[slot] + m_aEdgeAt[slot + 1] + m_aEdgeAt[slot + 2] + m_aEdgeAt[slot + 3];
	}

	//------------------------------------------------------------------------------------------------
	//! Remet à zéro les coins touchés par la passe (sans parcourir toute la grille) — ChainAll
	protected void ResetEdges()
	{
		for (int i = 0; i < m_aTouched.Count(); i++)
		{
			int corner = m_aTouched[i];
			int slot = corner * 4;
			m_aDeg[corner] = 0;
			m_aEdgeAt[slot] = 0;
			m_aEdgeAt[slot + 1] = 0;
			m_aEdgeAt[slot + 2] = 0;
			m_aEdgeAt[slot + 3] = 0;
		}

		m_aTouched.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! c) Chaînage d'une passe : d'abord depuis les coins de degré impair (bouts de trait sur la côte, jonctions de
	//! limites), puis les boucles restantes (poches E2, fermées), de préférence depuis un coin de degré 2 — BuildChains
	protected void ChainAll(int kind, int zone)
	{
		int touched = m_aTouched.Count();

		for (int i = 0; i < touched; i++)
		{
			int oddCorner = m_aTouched[i];
			while ((RemainingDegree(oddCorner) & 1) == 1)
			{
				Walk(oddCorner, kind, zone);
			}
		}

		for (int j = 0; j < touched; j++)
		{
			int plainCorner = m_aTouched[j];
			if (m_aDeg[plainCorner] != 2)
				continue;

			while (RemainingDegree(plainCorner) > 0)
			{
				Walk(plainCorner, kind, zone);
			}
		}

		for (int k = 0; k < touched; k++)
		{
			int restCorner = m_aTouched[k];
			while (RemainingDegree(restCorner) > 0)
			{
				Walk(restCorner, kind, zone);
			}
		}

		ResetEdges();
	}

	//------------------------------------------------------------------------------------------------
	//! Un carré compte-t-il comme « dedans » pour tourner à un coin en damier ? Front : carré bleu ; contour
	//! d'attaque : carré de la zone
	protected bool IsInside(int kind, int zone, int gx, int gz)
	{
		if (gx < 0 || gz < 0 || gx >= m_iGridN || gz >= m_iGridN)
			return false;

		int cell = gz * m_iGridN + gx;
		if (kind == KIND_FRONT)
			return m_aCellOwn[cell] == SRP_EFrontOwner.BLEU;

		return m_aCellZn[cell] == zone;
	}

	//------------------------------------------------------------------------------------------------
	//! Coin en damier : on tourne du côté du carré « dedans » de l'arête d'arrivée (edgeDir, vue depuis le coin) ;
	//! les deux traits se touchent au coin sans jamais se croiser. Rend -1 si aucun des deux carrés n'est dedans
	protected int TurnInside(int corner, int edgeDir, int kind, int zone)
	{
		int cz = corner / (m_iGridN + 1);
		int cx = corner - cz * (m_iGridN + 1);

		// carrés autour du coin : NE (cx, cz), NO (cx - 1, cz), SE (cx, cz - 1), SO (cx - 1, cz - 1)
		if (edgeDir == DIR_N)
		{
			if (IsInside(kind, zone, cx, cz))
				return DIR_E;
			if (IsInside(kind, zone, cx - 1, cz))
				return DIR_W;
		}
		else if (edgeDir == DIR_E)
		{
			if (IsInside(kind, zone, cx, cz))
				return DIR_N;
			if (IsInside(kind, zone, cx, cz - 1))
				return DIR_S;
		}
		else if (edgeDir == DIR_S)
		{
			if (IsInside(kind, zone, cx, cz - 1))
				return DIR_E;
			if (IsInside(kind, zone, cx - 1, cz - 1))
				return DIR_W;
		}
		else if (edgeDir == DIR_W)
		{
			if (IsInside(kind, zone, cx - 1, cz - 1))
				return DIR_S;
			if (IsInside(kind, zone, cx - 1, cz))
				return DIR_N;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Arête suivante depuis un coin : damier (degré 4, front et contours) tourné côté dedans, sinon tout droit, sinon
	//! la première libre ; -1 si le coin est épuisé (ou si le tour imposé est déjà pris : fin de boucle)
	protected int NextDir(int corner, int moveDir, int kind, int zone)
	{
		int slot = corner * 4;

		if (moveDir >= 0)
		{
			if (m_aDeg[corner] == 4 && kind != KIND_BORDER)
			{
				int turn = TurnInside(corner, Opposite(moveDir), kind, zone);
				if (turn >= 0)
				{
					if (m_aEdgeAt[slot + turn] == 1)
						return turn;
					return -1;
				}
			}

			if (m_aEdgeAt[slot + moveDir] == 1)
				return moveDir;
		}

		for (int dir = 0; dir < 4; dir++)
		{
			if (m_aEdgeAt[slot + dir] == 1)
				return dir;
		}

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	//! Suit les arêtes libres depuis un coin jusqu'à épuisement ; ne garde que les coins où la direction change ;
	//! boucle fermée si l'on revient au départ — ChainAll
	protected void Walk(int start, int kind, int zone)
	{
		m_aWalk.Clear();
		m_aWalk.Insert(start);

		int corner = start;
		int moveDir = -1;
		int guard = m_aEdgeAt.Count();
		while (guard > 0)
		{
			guard--;
			int dir = NextDir(corner, moveDir, kind, zone);
			if (dir < 0)
				break;

			if (moveDir >= 0 && dir != moveDir)
				m_aWalk.Insert(corner);	// coin où le trait tourne

			int next = corner + CornerStep(dir);
			m_aEdgeAt[corner * 4 + dir] = 0;
			m_aEdgeAt[next * 4 + Opposite(dir)] = 0;
			corner = next;
			moveDir = dir;
		}

		if (moveDir < 0)
			return;	// aucun pas fait

		m_aWalk.Insert(corner);

		bool closed = false;
		if (corner == start && m_aWalk.Count() > 3)
		{
			closed = true;
			m_aWalk.Resize(m_aWalk.Count() - 1);	// le départ n'est pas répété : m_bShouldEnclose ferme la boucle
		}

		StoreChain(kind, closed);
	}

	//------------------------------------------------------------------------------------------------
	//! d) Découpe de m_aWalk en tronçons d'au plus m_iMaxPts points, le dernier point repris au début du suivant ;
	//! une boucle découpée perd m_bShouldEnclose et répète son premier point à la fin — Walk
	protected void StoreChain(int kind, bool closed)
	{
		int count = m_aWalk.Count();
		if (count < 2)
			return;

		bool enclose = closed;
		if (enclose && count > m_iMaxPts)
		{
			m_aWalk.Insert(m_aWalk[0]);
			count++;
			enclose = false;
		}

		int from = 0;
		while (from < count - 1)
		{
			int to = from + m_iMaxPts - 1;
			if (to > count - 1)
				to = count - 1;

			AddChainPiece(kind, from, to, enclose);
			from = to;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un tronçon : points m_aWalk[from..to] en mètres, genre, boîte englobante, fermeture — StoreChain
	protected void AddChainPiece(int kind, int from, int to, bool closed)
	{
		array<float> points = {};
		int stride = m_iGridN + 1;

		int firstCorner = m_aWalk[from];
		int firstZ = firstCorner / stride;
		float xmin = (firstCorner - firstZ * stride) * m_fCell;
		float zmin = firstZ * m_fCell;
		float xmax = xmin;
		float zmax = zmin;

		for (int i = from; i <= to; i++)
		{
			int corner = m_aWalk[i];
			int cz = corner / stride;
			int cx = corner - cz * stride;
			float x = cx * m_fCell;
			float z = cz * m_fCell;
			points.Insert(x);
			points.Insert(z);

			if (x < xmin)
				xmin = x;
			if (x > xmax)
				xmax = x;
			if (z < zmin)
				zmin = z;
			if (z > zmax)
				zmax = z;
		}

		m_aChain.Insert(points);
		m_aChainKind.Insert(kind);
		m_aChainBox.Insert(xmin);
		m_aChainBox.Insert(zmin);
		m_aChainBox.Insert(xmax);
		m_aChainBox.Insert(zmax);
		m_aChainClosed.Insert(closed);
	}

	//------------------------------------------------------------------------------------------------
	//! e) Étiquettes : pour chaque zone, GetZoneLabelPos (centre de ses carrés de terre, pas le point clé, pour ne pas
	//! couvrir l'icône), GetZoneLabel et GetZoneCode — BuildWorld
	protected void BuildLabels(SRP_FrontComponent front)
	{
		m_aLabelPos.Clear();
		m_aLabelFull.Clear();
		m_aLabelShort.Clear();

		int zones = front.GetZoneCount();
		for (int zone = 0; zone < zones; zone++)
		{
			vector pos = front.GetZoneLabelPos(zone);
			if (pos == vector.Zero)
				continue;

			string full = front.GetZoneLabel(zone);
			string code = front.GetZoneCode(zone);
			if (full == "")
				full = code;
			if (full == "")
				continue;

			m_aLabelPos.Insert(pos);
			m_aLabelFull.Insert(full);
			m_aLabelShort.Insert(code);
		}
	}

	//================================================================================================
	// Projection écran
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Projection écran du cadre visible (un carré de marge) : rectangles rouges, bleus, orange, limites, contours
	//! d'attaque, front, textes ; épaisseurs par DPIScale ; calage si m_bMapCalibration — Update
	protected void BuildScreen()
	{
		m_aCommands.Clear();

		SRP_FrontComponent front = m_Front;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!front || !workspace || m_fScale <= 0)
		{
			HideTextWidgets(0);
			return;
		}

		float scale = m_fScale;
		float ox = m_fOffX;
		float oy = m_fOffY;
		float margin = m_fCell;
		if (margin <= 0)
			margin = 200;

		// Cadre visible en mètres (x vers l'est, z vers le nord), un carré de marge ; tout ce qui est dehors est sauté
		float xmin = -ox / scale - margin;
		float xmax = (m_fW - ox) / scale + margin;
		float zmin = (oy - m_fH) / scale - margin;
		float zmax = oy / scale + margin;

		// Remplissages, un lot par couleur : rouge (classe 0), bleu (1), orange par-dessus (2), puis les carrés qui
		// clignotent (3, couleur du moment ; Update change seulement leur couleur ensuite)
		m_aBlinkPolys.Clear();
		int polyCount = 0;
		for (int pass = 0; pass < CLASS_COUNT; pass++)
		{
			int fill = m_iRedFill;
			if (pass == 1)
				fill = m_iBlueFill;
			else if (pass == CLASS_CONTESTED)
				fill = m_iContestedFill;
			else if (pass == CLASS_BLINK)
				fill = BlinkColor();

			for (int r = 0; r < m_aRectColor.Count(); r++)
			{
				if (m_aRectColor[r] != pass)
					continue;

				int k = r * 4;
				float x0 = m_aRect[k];
				float z0 = m_aRect[k + 1];
				float x1 = m_aRect[k + 2];
				float z1 = m_aRect[k + 3];
				if (x1 < xmin || x0 > xmax || z1 < zmin || z0 > zmax)
					continue;

				// rognés au cadre : jamais de sommet à des centaines de milliers de pixels au plus fort zoom
				if (x0 < xmin)
					x0 = xmin;
				if (x1 > xmax)
					x1 = xmax;
				if (z0 < zmin)
					z0 = zmin;
				if (z1 > zmax)
					z1 = zmax;

				float left = x0 * scale + ox;
				float right = x1 * scale + ox;
				float top = oy - z1 * scale;
				float bottom = oy - z0 * scale;

				PolygonDrawCommand poly = PolyAt(polyCount);
				polyCount++;
				poly.m_iColor = fill;
				if (pass == CLASS_BLINK)
					m_aBlinkPolys.Insert(poly);
				array<float> corners = poly.m_Vertices;
				corners[0] = left;
				corners[1] = bottom;
				corners[2] = right;
				corners[3] = bottom;
				corners[4] = right;
				corners[5] = top;
				corners[6] = left;
				corners[7] = top;
				m_aCommands.Insert(poly);
			}
		}

		// Traits : limites de zones, contours d'attaque (annoncée puis en cours), front par-dessus
		float borderWidth = workspace.DPIScale(front.m_fMapZoneBorderWidth);
		float attackWidth = workspace.DPIScale(front.m_fMapAttackOutlineWidth);
		float frontWidth = workspace.DPIScale(front.m_fMapFrontWidth);
		float frontOutline = workspace.DPIScale(front.m_fMapFrontOutlineWidth);

		int lineCount = 0;
		for (int kind = KIND_BORDER; kind <= KIND_FRONT; kind++)
		{
			for (int ch = 0; ch < m_aChain.Count(); ch++)
			{
				if (m_aChainKind[ch] != kind)
					continue;

				int b = ch * 4;
				if (m_aChainBox[b + 2] < xmin || m_aChainBox[b] > xmax || m_aChainBox[b + 3] < zmin || m_aChainBox[b + 1] > zmax)
					continue;

				LineDrawCommand line = LineAt(lineCount);
				lineCount++;
				Project(m_aChain[ch], line.m_Vertices);
				line.m_bShouldEnclose = m_aChainClosed[ch];

				if (kind == KIND_FRONT)
				{
					line.m_iColor = m_iFrontColor;
					line.m_fWidth = frontWidth;
					line.m_fOutlineWidth = frontOutline;
					line.m_iOutlineColor = m_iFrontOutline;
				}
				else if (kind == KIND_BORDER)
				{
					line.m_iColor = m_iZoneBorder;
					line.m_fWidth = borderWidth;
					line.m_fOutlineWidth = 0;
					line.m_iOutlineColor = 0;
				}
				else
				{
					if (kind == KIND_ATTACK)
						line.m_iColor = m_iAttack;
					else
						line.m_iColor = m_iAttackWarn;
					line.m_fWidth = attackWidth;
					line.m_fOutlineWidth = 0;
					line.m_iOutlineColor = 0;
				}

				m_aCommands.Insert(line);
			}
		}

		m_sCalibText = "";
		if (front.m_bMapCalibration)
			lineCount = AddCalibrationLines(lineCount, xmin, xmax, zmin, zmax);

		// Textes : noms des zones (TextDrawCommand, ou TextWidget de repli), puis le texte de calage
		int textCount = 0;
		int widgetCount = 0;
		if (front.m_bMapZoneLabels)
		{
			float labelSize = workspace.DPIScale(front.m_fMapLabelSize);
			for (int lb = 0; lb < m_aLabelPos.Count(); lb++)
			{
				vector at = m_aLabelPos[lb];
				if (at[0] < xmin || at[0] > xmax || at[2] < zmin || at[2] > zmax)
					continue;

				string label = LabelFor(lb, scale);
				if (label == "")
					continue;

				float sx = at[0] * scale + ox;
				float sy = oy - at[2] * scale;

				if (front.m_bMapTextWidgetFallback)
				{
					PlaceTextWidget(widgetCount, label, sx, sy, front.m_fMapLabelSize);
					widgetCount++;
					continue;
				}

				// aucun centrage fourni par le moteur : demi-largeur estimée (caractères x taille x part)
				float halfWidth = label.Length() * labelSize * front.m_fMapLabelCharWidth * 0.5;
				TextDrawCommand text = TextAt(textCount);
				textCount++;
				text.m_sText = label;
				text.m_fSize = labelSize;
				text.m_iColor = m_iLabelColor;
				text.m_Position = Vector(sx - halfWidth, sy - labelSize * 0.5, 0);
				m_aCommands.Insert(text);
			}
		}

		HideTextWidgets(widgetCount);

		if (m_sCalibText != "")
		{
			// toujours en TextDrawCommand : c'est lui que le calage valide
			TextDrawCommand calib = TextAt(textCount);
			textCount++;
			calib.m_sText = m_sCalibText;
			calib.m_fSize = workspace.DPIScale(18);
			calib.m_iColor = Color.MAGENTA;
			calib.m_Position = Vector(m_fCalibX, m_fCalibY, 0);
			m_aCommands.Insert(calib);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Polyligne en mètres (x, z…) vers sommets écran (x, y…) — BuildScreen
	protected void Project(array<float> source, array<float> target)
	{
		int count = source.Count();
		target.Resize(count);
		for (int i = 0; i + 1 < count; i += 2)
		{
			target[i] = source[i] * m_fScale + m_fOffX;
			target[i + 1] = m_fOffY - source[i + 1] * m_fScale;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Calage (m_bMapCalibration) : lignes magenta tous les 1 km, carré du joueur local, texte « CALAGE 074 042 » ;
	//! rend le nombre de LineDrawCommand utilisées — BuildScreen
	protected int AddCalibrationLines(int first, float xmin, float xmax, float zmin, float zmax)
	{
		int used = first;
		float step = 1000;

		int kxFrom = Math.Ceil(xmin / step);
		int kxTo = Math.Floor(xmax / step);
		if (kxTo - kxFrom <= 100)
		{
			for (int kx = kxFrom; kx <= kxTo; kx++)
			{
				float x = kx * step;
				used = AddCalibrationSegment(used, x, zmin, x, zmax);
			}
		}

		int kzFrom = Math.Ceil(zmin / step);
		int kzTo = Math.Floor(zmax / step);
		if (kzTo - kzFrom <= 100)
		{
			for (int kz = kzFrom; kz <= kzTo; kz++)
			{
				float z = kz * step;
				used = AddCalibrationSegment(used, xmin, z, xmax, z);
			}
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		m_sCalibText = "CALAGE";
		m_fCalibX = 24;
		m_fCalibY = 24;
		if (workspace)
		{
			m_fCalibX = workspace.DPIScale(24);
			m_fCalibY = workspace.DPIScale(24);
		}

		IEntity player;
		PlayerController controller = GetGame().GetPlayerController();
		if (controller)
			player = controller.GetControlledEntity();

		if (player && m_fCell > 0)
		{
			vector origin = player.GetOrigin();
			int gx = Math.Floor(origin[0] / m_fCell);
			int gz = Math.Floor(origin[2] / m_fCell);
			float sx0 = gx * m_fCell;
			float sz0 = gz * m_fCell;
			float sx1 = sx0 + m_fCell;
			float sz1 = sz0 + m_fCell;

			// le carré de la grille où se tient le joueur : il doit l'entourer
			used = AddCalibrationSegment(used, sx0, sz0, sx1, sz0);
			used = AddCalibrationSegment(used, sx1, sz0, sx1, sz1);
			used = AddCalibrationSegment(used, sx1, sz1, sx0, sz1);
			used = AddCalibrationSegment(used, sx0, sz1, sx0, sz0);

			if (m_Front && gx >= 0 && gz >= 0 && gx < m_iGridN && gz < m_iGridN)
				m_sCalibText = "CALAGE " + m_Front.CellRef(gz * m_iGridN + gx);

			m_fCalibX = sx1 * m_fScale + m_fOffX + m_fCalibX * 0.25;
			m_fCalibY = m_fOffY - sz1 * m_fScale;
		}

		return used;
	}

	//------------------------------------------------------------------------------------------------
	//! Un segment magenta de calage, en mètres ; rend le rang suivant dans la réserve — AddCalibrationLines
	protected int AddCalibrationSegment(int index, float x0, float z0, float x1, float z1)
	{
		LineDrawCommand line = LineAt(index);
		array<float> verts = line.m_Vertices;
		verts.Resize(4);
		verts[0] = x0 * m_fScale + m_fOffX;
		verts[1] = m_fOffY - z0 * m_fScale;
		verts[2] = x1 * m_fScale + m_fOffX;
		verts[3] = m_fOffY - z1 * m_fScale;

		float width = 2;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			width = workspace.DPIScale(2);

		line.m_iColor = Color.MAGENTA;
		line.m_fWidth = width;
		line.m_fOutlineWidth = 0;
		line.m_iOutlineColor = 0;
		line.m_bShouldEnclose = false;
		m_aCommands.Insert(line);
		return index + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom plein « Régina (S07) » si scale x 1000 >= m_fMapLabelFullMinPxPerKm, sinon « S07 » — BuildScreen
	protected string LabelFor(int index, float scale)
	{
		if (index < 0 || index >= m_aLabelFull.Count())
			return "";

		float fullFrom = 150;
		if (m_Front)
			fullFrom = m_Front.m_fMapLabelFullMinPxPerKm;

		string full = m_aLabelFull[index];
		if (scale * 1000 >= fullFrom)
			return full;

		string code = m_aLabelShort[index];
		if (code == "")
			return full;	// la base n'a que son nom

		return code;
	}

	//================================================================================================
	// Réserves
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Réserve de commandes : la n-ième PolygonDrawCommand (créée une fois, m_Vertices de 8 flottants réécrit) — BuildScreen
	protected PolygonDrawCommand PolyAt(int index)
	{
		while (m_aPolys.Count() <= index)
		{
			PolygonDrawCommand cmd = new PolygonDrawCommand();
			cmd.m_Vertices = new array<float>();
			cmd.m_Vertices.Resize(8);
			m_aPolys.Insert(cmd);
		}

		return m_aPolys[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Réserve de commandes : la n-ième LineDrawCommand — BuildScreen
	protected LineDrawCommand LineAt(int index)
	{
		while (m_aLines.Count() <= index)
		{
			LineDrawCommand cmd = new LineDrawCommand();
			cmd.m_Vertices = new array<float>();
			m_aLines.Insert(cmd);
		}

		return m_aLines[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Réserve de commandes : la n-ième TextDrawCommand — BuildScreen
	protected TextDrawCommand TextAt(int index)
	{
		while (m_aTexts.Count() <= index)
		{
			TextDrawCommand cmd = new TextDrawCommand();
			m_aTexts.Insert(cmd);
		}

		return m_aTexts[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Repli (m_bMapTextWidgetFallback) : le n-ième TextWidget, créé dans le parent du canevas au rang du canevas + 1,
	//! centré sur le point (même méthode que les repères vanilla, SCR_MapMarkerBase.c:425-426) — BuildScreen
	protected void PlaceTextWidget(int index, string text, float sx, float sy, float size)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_wParent)
			return;

		while (m_aTextWidgets.Count() <= index)
		{
			Widget created = workspace.CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.BLEND | WidgetFlags.IGNORE_CURSOR | WidgetFlags.NOFOCUS, Color.FromInt(Color.WHITE), m_iCanvasZ + 1, m_wParent);
			if (!created)
				return;

			// un TextWidget créé par script n'a pas de police : même police que la carte (SCR_MapPropsConfig.c:93)
			TextWidget createdText = TextWidget.Cast(created);
			if (createdText)
				createdText.SetFont("{3E7733BAC8C831F6}UI/Fonts/RobotoCondensed/RobotoCondensed_Regular.fnt");

			FrameSlot.SetSizeToContent(created, true);
			FrameSlot.SetAlignment(created, 0.5, 0.5);
			created.SetZOrder(m_iCanvasZ + 1);
			m_aTextWidgets.Insert(created);
		}

		TextWidget label = TextWidget.Cast(m_aTextWidgets[index]);
		if (!label)
			return;

		int fontSize = size;
		label.SetText(text);
		label.SetExactFontSize(fontSize);
		label.SetColorInt(m_iLabelColor);
		FrameSlot.SetPos(label, workspace.DPIUnscale(sx), workspace.DPIUnscale(sy));	// le cadre attend des unités non mises à l'échelle
		label.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Cache les TextWidget de repli à partir du rang donné (noms sortis du cadre) — BuildScreen
	protected void HideTextWidgets(int from)
	{
		for (int i = from; i < m_aTextWidgets.Count(); i++)
		{
			Widget label = m_aTextWidgets[i];
			if (label)
				label.SetVisible(false);
		}
	}

	//================================================================================================
	// Outils
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! La carte ouverte reçoit-elle le calque ? Plein écran, réapparition (SPAWNSCREEN et PLAIN), Game Master — Attach
	protected bool IsModeAllowed(SRP_FrontComponent front, EMapEntityMode mode)
	{
		if (mode == EMapEntityMode.FULLSCREEN)
			return front.m_bMapOnFullscreen;

		if (mode == EMapEntityMode.SPAWNSCREEN || mode == EMapEntityMode.PLAIN)
			return front.m_bMapOnSpawn;

		if (mode == EMapEntityMode.EDITOR)
			return front.m_bMapOnEditor;

		return false;	// mini-carte, stations, didacticiel : jamais
	}

	//------------------------------------------------------------------------------------------------
	//! Survol : commandes du second canevas créées une fois (reflet, ombre, contour) et les 16 reflets — Attach
	protected void PrepareHover()
	{
		if (!m_HoverFill)
		{
			m_HoverFill = new PolygonDrawCommand();
			m_HoverFill.m_Vertices = new array<float>();
			m_HoverFill.m_Vertices.Resize(8);
		}
		if (!m_HoverShadow)
		{
			m_HoverShadow = new LineDrawCommand();
			m_HoverShadow.m_Vertices = new array<float>();
			m_HoverShadow.m_Vertices.Resize(6);
		}
		if (!m_HoverLine)
		{
			m_HoverLine = new LineDrawCommand();
			m_HoverLine.m_Vertices = new array<float>();
			m_HoverLine.m_Vertices.Resize(8);
		}
		m_HoverShadow.m_bShouldEnclose = false;	// ombre en L, bas puis droite : elle ne tombe que dehors
		m_HoverShadow.m_iColor = PackOr(null, 0, 0, 0, 0.45);
		m_HoverShadow.m_fOutlineWidth = 0;
		m_HoverLine.m_bShouldEnclose = true;
		m_HoverLine.m_iColor = PackOr(null, 1, 1, 1, 0.95);
		m_HoverLine.m_fOutlineWidth = 0;
		if (m_aHoverFillColors.IsEmpty())
		{
			for (int step = 0; step < 16; step++)
			{
				m_aHoverFillColors.Insert(PackOr(null, 1, 1, 1, 0.08 + 0.02 * step));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque image : carré sous le curseur de la carte (GetMapCursorWorldPosition, souris ou manette) ; carré en jeu ->
	//! reflet blanc qui respire (1,2 s), ombre décalée et contour blanc ; hors carré -> rien. Redessiné toutes les 40 ms
	//! au plus, ou tout de suite si le carré, le zoom ou le décalage change — Update
	protected void UpdateHover(SCR_MapEntity mapEntity, SRP_FrontComponent front)
	{
		if (!m_wHover || !m_HoverFill || !m_HoverShadow || !m_HoverLine)
			return;

		int cell = -1;
		if (m_fScale > 0 && m_fCell > 0 && m_iGridN > 0)
		{
			float worldX;
			float worldZ;
			mapEntity.GetMapCursorWorldPosition(worldX, worldZ);
			cell = front.CellIndexAt(Vector(worldX, 0, worldZ));
			if (cell >= 0)
			{
				int owner = front.GetCellOwner(cell);
				if (owner != SRP_EFrontOwner.ROUGE && owner != SRP_EFrontOwner.BLEU)
					cell = -1;
			}
		}

		if (cell < 0)
		{
			if (m_iHoverCell >= 0)
			{
				m_iHoverCell = -1;
				m_aHoverCommands.Clear();
				m_wHover.SetDrawCommands(m_aHoverCommands);
			}
			return;
		}

		int now = System.GetTickCount();
		bool moved = cell != m_iHoverCell || m_fScale != m_fHoverScale || m_fOffX != m_fHoverOffX || m_fOffY != m_fHoverOffY;
		if (!moved && now - m_iHoverDrawMs < 40)
			return;

		// Reflet qui respire (1,2 s) : le carré « ressort » ; même marche et rien d'autre de changé -> rien à redessiner
		int cycle = now % 1200;
		float wave = 0.5 + 0.5 * Math.Sin(cycle / 1200.0 * Math.PI2);
		int rounded = Math.Round(wave * 15);	// flottant vers entier par une affectation, comme ailleurs dans le mod
		int step = Math.ClampInt(rounded, 0, 15);
		if (!moved && step == m_iHoverStep)
			return;
		m_iHoverStep = step;
		m_iHoverCell = cell;
		m_iHoverDrawMs = now;
		m_fHoverScale = m_fScale;
		m_fHoverOffX = m_fOffX;
		m_fHoverOffY = m_fOffY;

		// Rectangle du carré à l'écran (mêmes formules que BuildScreen ; colonne et ligne entières calculées à part)
		int col = front.CellCol(cell);
		int row = front.CellRow(cell);
		float x0 = col * m_fCell;
		float z0 = row * m_fCell;
		float left = x0 * m_fScale + m_fOffX;
		float right = (x0 + m_fCell) * m_fScale + m_fOffX;
		float top = m_fOffY - (z0 + m_fCell) * m_fScale;
		float bottom = m_fOffY - z0 * m_fScale;

		m_HoverFill.m_iColor = m_aHoverFillColors[step];
		HoverRect(m_HoverFill.m_Vertices, left, top, right, bottom);

		// Ombre décalée vers le bas à droite, puis contour blanc : effet de relief
		float shift = 2;
		float lineWidth = 2;
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
		{
			shift = workspace.DPIScale(2);
			lineWidth = workspace.DPIScale(2);
		}
		array<float> shadow = m_HoverShadow.m_Vertices;
		if (shadow.Count() != 6)
			shadow.Resize(6);
		shadow[0] = left + shift;
		shadow[1] = bottom + shift;
		shadow[2] = right + shift;
		shadow[3] = bottom + shift;
		shadow[4] = right + shift;
		shadow[5] = top + shift;
		m_HoverShadow.m_fWidth = lineWidth + 1;
		HoverRect(m_HoverLine.m_Vertices, left, top, right, bottom);
		m_HoverLine.m_fWidth = lineWidth;

		m_aHoverCommands.Clear();
		m_aHoverCommands.Insert(m_HoverFill);
		m_aHoverCommands.Insert(m_HoverShadow);
		m_aHoverCommands.Insert(m_HoverLine);
		m_wHover.SetDrawCommands(m_aHoverCommands);
	}

	//------------------------------------------------------------------------------------------------
	//! Les 4 coins d'un rectangle écran, dans l'ordre de BuildScreen (bas gauche, bas droit, haut droit, haut gauche) —
	//! UpdateHover
	protected void HoverRect(array<float> corners, float left, float top, float right, float bottom)
	{
		if (!corners)
			return;
		if (corners.Count() != 8)
			corners.Resize(8);
		corners[0] = left;
		corners[1] = bottom;
		corners[2] = right;
		corners[3] = bottom;
		corners[4] = right;
		corners[5] = top;
		corners[6] = left;
		corners[7] = top;
	}

	//------------------------------------------------------------------------------------------------
	//! Classe de remplissage d'un carré en jeu : en cours de prise ou de reprise (CAPTURE, REPRISE) -> CLASS_BLINK ;
	//! figé au combat (COMBAT) -> CLASS_CONTESTED (orange fixe) ; sinon son propriétaire — ReadCells
	protected int CellClass(SRP_FrontComponent front, int cell, int owner)
	{
		int liveState = front.GetCellLive(cell);
		if (liveState == SRP_EFrontLive.CAPTURE || liveState == SRP_EFrontLive.REPRISE)
			return CLASS_BLINK;
		if (liveState == SRP_EFrontLive.COMBAT)
			return CLASS_CONTESTED;
		return owner;
	}

	//------------------------------------------------------------------------------------------------
	//! Couleur du clignotement à la phase en cours : orange, bleu, orange, rouge (choix de Jack du 27/09) — Update,
	//! BuildScreen
	protected int BlinkColor()
	{
		if (m_iBlinkPhase == 1)
			return m_iBlinkBlue;
		if (m_iBlinkPhase == 3)
			return m_iBlinkRed;
		return m_iBlinkOrange;
	}

	//------------------------------------------------------------------------------------------------
	//! Couleur d'un réglage empaquetée (ARGB), ou la valeur donnée si l'attribut manque — Attach
	protected int PackOr(Color color, float r, float g, float b, float a)
	{
		if (color)
			return color.PackToInt();

		Color fallback = new Color(r, g, b, a);
		return fallback.PackToInt();
	}

	//------------------------------------------------------------------------------------------------
	//! Diagnostic (m_bMapDiag) : nom et rang des widgets voisins du canevas, une fois par ouverture — Attach
	protected void LogSiblings(Widget parent)
	{
		if (!parent)
			return;

		int canvasZ = -1;
		if (m_wCanvas)
			canvasZ = m_wCanvas.GetZOrder();

		Print(string.Format("[SRP Front] parent du calque : « %1 » (%2), rang du calque %3", parent.GetName(), parent.GetTypeName(), canvasZ), LogLevel.NORMAL);

		Widget child = parent.GetChildren();
		int shown = 0;
		while (child && shown < 64)
		{
			shown++;
			Print(string.Format("[SRP Front]   voisin %1 : « %2 » (%3), rang %4", shown, child.GetName(), child.GetTypeName(), child.GetZOrder()), LogLevel.NORMAL);
			child = child.GetSibling();
		}
	}
}
