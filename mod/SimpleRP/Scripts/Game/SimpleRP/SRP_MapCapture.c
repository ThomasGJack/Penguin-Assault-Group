//------------------------------------------------------------------------------------------------
// SimpleRP — Capture aérienne de la carte (outil Staff, à lancer dans le Workbench en plein écran)
// Fabrique le fond « satellite » de la carte de situation web : une caméra scriptée, à la verticale, saute de case en
// case au-dessus de l'île et prend une capture d'écran à chaque case (bâtiments, arbres, notre base compris). Les
// captures vont dans $profile:CartePAG/ avec un fichier capture.txt qui décrit la grille ; le script Python
// E:\Projets\PAG-Bot\carte\assembler_captures.py les recadre et les assemble en tuiles.
//
// Principe : champ très étroit (peu de déformation), caméra à hauteur constante AU-DESSUS DU SOL de la case (échelle
// constante), nord en haut de l'écran, midi, ciel dégagé, heure figée, HUD masqué. Chaque case n'utilise que le centre
// de l'image (le pas est plus petit que la zone vue).
// Tout se passe côté client (caméra, écran) ; l'heure et la météo sont réglées côté serveur, donc à faire dans le
// Workbench, où les deux ne font qu'un.
//------------------------------------------------------------------------------------------------

class SRP_MapCapture
{
	static float s_fAltitude = 1500;		// hauteur de la caméra au-dessus du sol de la case, en mètres
	static float s_fFov = 12;				// champ vertical, en degrés
	static float s_fStep = 250;				// côté d'une case, en mètres (doit rester plus petit que la hauteur vue)
	static int s_iSettleMs = 1600;			// attente après chaque saut, le temps que le décor se charge
	static int s_iFirstSettleMs = 6000;		// attente avant la toute première capture

	protected static bool s_bRunning;
	protected static SCR_CameraBase s_Camera;
	protected static CameraBase s_Previous;
	protected static float s_fX0;
	protected static float s_fZ0;
	protected static int s_iCols;
	protected static int s_iRows;
	protected static int s_iIndex;
	protected static string s_sFolder;

	// Essai comparatif : les variantes qui restent à faire (hauteur, champ, pas), l'une après l'autre sur le même carré
	protected static ref array<float> s_aQueueAltitude = {};
	protected static ref array<float> s_aQueueFov = {};
	protected static ref array<float> s_aQueueStep = {};
	protected static ref array<string> s_aQueueFolder = {};
	protected static float s_fSavedViewDistance = -1;
	protected static int s_iSavedGrassDistance = -1;

	//------------------------------------------------------------------------------------------------
	//! Les réglages retenus pour les vraies captures (l'essai comparatif les change le temps de ses variantes)
	//! Essai comparatif du 20 sept. 2026 : à 1 500 m et 700 m le terrain est flou et voilé ; à 350 m il est net (marquage
	//! des routes, herbe, potagers) et les cases se raccordent ; 200 m n'apporte presque rien de plus pour le double de temps.
	static void UseChosenSettings()
	{
		s_fAltitude = 350;
		s_fFov = 30;
		s_fStep = 120;
		s_iSettleMs = 1200;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsRunning()
	{
		return s_bRunning;
	}

	//------------------------------------------------------------------------------------------------
	//! Essai comparatif : le centre de Levie (360 m de côté) photographié à quatre hauteurs. Plus la caméra est basse,
	//! plus le terrain et les ombres sont détaillés, mais plus il faut de cases (et plus les murs penchent sur les bords).
	static string StartCompare()
	{
		if (s_bRunning)
			return "Une capture est déjà en cours";
		s_aQueueAltitude.Clear();
		s_aQueueFov.Clear();
		s_aQueueStep.Clear();
		s_aQueueFolder.Clear();
		AddVariant(1500, 12, 250, "CartePAG_essai_A");
		AddVariant(700, 18, 170, "CartePAG_essai_B");
		AddVariant(350, 30, 120, "CartePAG_essai_C");
		AddVariant(200, 40, 90, "CartePAG_essai_D");
		NextVariant();
		return "Essai comparatif lancé : 4 hauteurs sur le centre de Levie, environ 2 min. Plein écran, ne touche à rien.";
	}

	//------------------------------------------------------------------------------------------------
	protected static void AddVariant(float altitude, float fov, float step, string folder)
	{
		s_aQueueAltitude.Insert(altitude);
		s_aQueueFov.Insert(fov);
		s_aQueueStep.Insert(step);
		s_aQueueFolder.Insert(folder);
	}

	//------------------------------------------------------------------------------------------------
	protected static void NextVariant()
	{
		if (s_aQueueFolder.IsEmpty())
			return;
		s_fAltitude = s_aQueueAltitude[0];
		s_fFov = s_aQueueFov[0];
		s_fStep = s_aQueueStep[0];
		string folder = s_aQueueFolder[0];
		s_aQueueAltitude.Remove(0);
		s_aQueueFov.Remove(0);
		s_aQueueStep.Remove(0);
		s_aQueueFolder.Remove(0);
		Print("[SRP] Capture de la carte, variante " + folder + " : " + Start(7300, 4560, 7660, 4920, folder), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Lance la capture du rectangle (x0, z0) - (x1, z1), en mètres du monde. Retourne le message pour le Staff.
	static string Start(float x0, float z0, float x1, float z1, string folder)
	{
		if (s_bRunning)
			return "Une capture est déjà en cours";
		BaseWorld world = GetGame().GetWorld();
		CameraManager cameras = GetGame().GetCameraManager();
		if (!world || !cameras)
			return "Capture impossible : pas de monde ou pas de gestionnaire de caméras";

		s_fX0 = x0;
		s_fZ0 = z0;
		s_iCols = Math.Ceil((x1 - x0) / s_fStep);
		s_iRows = Math.Ceil((z1 - z0) / s_fStep);
		s_iIndex = 0;
		s_sFolder = "$profile:" + folder;
		FileIO.MakeDirectory(s_sFolder);

		// Midi, ciel dégagé, heure figée : le même éclairage sur toutes les cases
		ChimeraWorld chimera = ChimeraWorld.CastFrom(world);
		if (chimera)
		{
			TimeAndWeatherManagerEntity weather = chimera.GetTimeAndWeatherManager();
			if (weather)
			{
				weather.SetIsDayAutoAdvanced(false);
				weather.SetTimeOfTheDay(12.5, true);
				weather.ForceWeatherTo(true, "Clear");
			}
		}

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = Vector(x0, 2000, z0);
		s_Camera = SCR_CameraBase.Cast(GetGame().SpawnEntity(SCR_CameraBase, world, params));
		if (!s_Camera)
			return "Capture impossible : la caméra n'a pas pu être créée";
		s_Previous = cameras.CurrentCamera();
		s_Camera.SetVerticalFOV(s_fFov);
		s_Camera.SetNearPlane(5);
		s_Camera.SetFarPlane(s_fAltitude + 1500);
		cameras.SetCamera(s_Camera);

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (hud)
			hud.SetVisible(false);

		// Distances d'affichage du jeu au maximum le temps de la capture (remises ensuite)
		if (s_fSavedViewDistance < 0)
		{
			s_fSavedViewDistance = GetGame().GetViewDistance();
			s_iSavedGrassDistance = GetGame().GetGrassDistance();
			GetGame().SetViewDistance(GetGame().GetMaximumViewDistance());
			GetGame().SetGrassDistance(GetGame().GetMaximumGrassDistance());
		}

		// La description de la grille, pour l'assemblage
		int width;
		int height;
		System.GetRenderingResolution(width, height);
		float seen = 2 * s_fAltitude * Math.Tan(s_fFov * 0.5 * Math.DEG2RAD);
		FileHandle info = FileIO.OpenFile(s_sFolder + "/capture.txt", FileMode.WRITE);
		if (info)
		{
			info.WriteLine(string.Format("x0=%1", x0));
			info.WriteLine(string.Format("z0=%1", z0));
			info.WriteLine(string.Format("pas=%1", s_fStep));
			info.WriteLine(string.Format("colonnes=%1", s_iCols));
			info.WriteLine(string.Format("rangees=%1", s_iRows));
			info.WriteLine(string.Format("hauteur_vue_m=%1", seen));
			info.WriteLine(string.Format("altitude=%1", s_fAltitude));
			info.WriteLine(string.Format("champ=%1", s_fFov));
			info.WriteLine(string.Format("ecran=%1x%2", width, height));
			info.Close();
		}

		s_bRunning = true;
		MoveTo(0);
		GetGame().GetCallqueue().CallLater(Shoot, s_iFirstSettleMs, false);
		int total = s_iCols * s_iRows;
		int minutes = total * (s_iSettleMs + 200) / 60000 + 1;
		Print(string.Format("[SRP] Capture de la carte : %1 x %2 cases de %3 m, environ %4 min, dossier %5", s_iCols, s_iRows, s_fStep, minutes, s_sFolder), LogLevel.NORMAL);
		return string.Format("Capture lancée : %1 cases, environ %2 min. Plein écran (F11), ne touche à rien. Échap dans le menu Staff pour l'arrêter.", total, minutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 10 s dans le Workbench : le fichier $profile:CartePAG_lancer.txt contient « essai » ou « ile » ?
	//! On le supprime et on lance la capture correspondante (« stop » l'arrête). Permet de la déclencher sans le menu.
	static void CheckTrigger()
	{
		string path = "$profile:CartePAG_lancer.txt";
		if (!FileIO.FileExists(path))
			return;
		string order;
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (file)
		{
			file.ReadLine(order);
			file.Close();
		}
		FileIO.DeleteFile(path);
		order.TrimInPlace();
		string answer = "ordre inconnu : " + order;
		if (order == "stop")
			answer = Stop();
		else if (order == "essai")
		{
			UseChosenSettings();
				answer = Start(7000, 4250, 8000, 5250, "CartePAG_essai");
		}
		else if (order == "comparer")
			answer = StartCompare();
		else if (order == "ile")
		{
			UseChosenSettings();
				answer = Start(0, 0, 12800, 12800, "CartePAG");
		}
		Print("[SRP] Capture de la carte, ordre par fichier « " + order + " » : " + answer, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	static string Stop()
	{
		if (!s_bRunning)
			return "Aucune capture en cours";
		Finish("arrêtée à la case " + s_iIndex.ToString());
		return "Capture arrêtée";
	}

	//------------------------------------------------------------------------------------------------
	//! Place la caméra à la verticale de la case : nord en haut de l'écran, est à droite
	protected static void MoveTo(int index)
	{
		int col = index % s_iCols;
		int row = index / s_iCols;
		float x = s_fX0 + (col + 0.5) * s_fStep;
		float z = s_fZ0 + (row + 0.5) * s_fStep;
		float ground = Math.Max(GetGame().GetWorld().GetSurfaceY(x, z), 0);

		vector transform[4];
		transform[0] = Vector(1, 0, 0);		// la droite de l'écran : l'est
		transform[1] = Vector(0, 0, 1);		// le haut de l'écran : le nord
		transform[2] = Vector(0, -1, 0);	// on regarde vers le bas
		transform[3] = Vector(x, ground + s_fAltitude, z);
		s_Camera.SetWorldTransform(transform);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Shoot()
	{
		if (!s_bRunning)
			return;
		int col = s_iIndex % s_iCols;
		int row = s_iIndex / s_iCols;
		System.MakeScreenshot(string.Format("%1/c%2_r%3.bmp", s_sFolder, col, row));
		// La capture se fait à l'image suivante : on ne bouge qu'après
		GetGame().GetCallqueue().CallLater(Next, 200, false);
	}

	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Une case en pleine mer (rien au-dessus de l'eau dans la case ni à 125 m autour) : inutile de la photographier,
	//! l'assemblage y peint la mer lui-même
	protected static bool IsOpenSea(int index)
	{
		int col = index % s_iCols;
		int row = index / s_iCols;
		BaseWorld world = GetGame().GetWorld();
		for (int i = 0; i <= 6; i++)
		{
			for (int j = 0; j <= 6; j++)
			{
				float x = s_fX0 + (col - 0.5 + i / 3.0) * s_fStep;
				float z = s_fZ0 + (row - 0.5 + j / 3.0) * s_fStep;
				if (world.GetSurfaceY(x, z) > -1)
					return false;
			}
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool IsDone(int index)
	{
		int col = index % s_iCols;
		int row = index / s_iCols;
		return FileIO.FileExists(string.Format("%1/c%2_r%3.bmp.png", s_sFolder, col, row));
	}

	//------------------------------------------------------------------------------------------------
	protected static void Next()
	{
		if (!s_bRunning)
			return;
		s_iIndex++;
		// On saute la pleine mer, et les cases déjà photographiées (reprise après une interruption : relancer suffit)
		while (s_iIndex < s_iCols * s_iRows && (IsOpenSea(s_iIndex) || IsDone(s_iIndex)))
			s_iIndex++;
		if (s_iIndex >= s_iCols * s_iRows)
		{
			Finish("terminée");
			return;
		}
		MoveTo(s_iIndex);
		GetGame().GetCallqueue().CallLater(Shoot, s_iSettleMs, false);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish(string how)
	{
		s_bRunning = false;
		GetGame().GetCallqueue().Remove(Shoot);
		GetGame().GetCallqueue().Remove(Next);

		CameraManager cameras = GetGame().GetCameraManager();
		if (cameras && s_Previous)
			cameras.SetCamera(s_Previous);
		if (s_Camera)
			SCR_EntityHelper.DeleteEntityAndChildren(s_Camera);
		s_Camera = null;
		s_Previous = null;

		SCR_HUDManagerComponent hud = SCR_HUDManagerComponent.GetHUDManager();
		if (hud)
			hud.SetVisible(true);

		if (s_fSavedViewDistance >= 0)
		{
			GetGame().SetViewDistance(s_fSavedViewDistance);
			GetGame().SetGrassDistance(s_iSavedGrassDistance);
			s_fSavedViewDistance = -1;
		}

		ChimeraWorld chimera = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (chimera && chimera.GetTimeAndWeatherManager())
			chimera.GetTimeAndWeatherManager().SetIsDayAutoAdvanced(true);

		FileHandle info = FileIO.OpenFile(s_sFolder + "/fin.txt", FileMode.WRITE);
		if (info)
		{
			info.WriteLine(how);
			info.Close();
		}
		Print("[SRP] Capture de la carte " + how + " (" + s_iIndex.ToString() + " cases)", LogLevel.NORMAL);

		// Essai comparatif : la variante suivante, sauf si on a arrêté à la main
		if (how == "terminée" && !s_aQueueFolder.IsEmpty())
			GetGame().GetCallqueue().CallLater(NextVariant, 1500, false);
		else
			s_aQueueFolder.Clear();
	}
}
