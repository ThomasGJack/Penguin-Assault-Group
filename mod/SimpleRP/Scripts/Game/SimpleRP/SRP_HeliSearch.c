//------------------------------------------------------------------------------------------------
// SimpleRP — Hélicoptère de recherche ennemi (PROTOTYPE)
// L'IA de Reforger ne sait pas piloter : le vol est SCRIPTÉ, côté serveur. À chaque image on pose l'appareil sur sa
// trajectoire (SetTransform) et on donne à sa physique la vitesse correspondante (SetVelocity), comme le fait la piste
// cinématique d'hélicoptère du jeu : les clients reçoivent un mouvement cohérent. Le moteur tourne (rotors, son) mais
// la portance des rotors est coupée, pour qu'elle ne se batte pas avec la trajectoire.
//
// Le vol : apparition à 2 km, hors de vue, sur la terre ferme (du côté du territoire ennemi si possible), en
// stationnaire quelques secondes ; approche en spirale ; cercles au-dessus du lieu ; si une alerte ennemie vit près du
// lieu, le cercle se recentre sur la dernière position connue des joueurs (l'alerte la plus récente) et se resserre ; la
// nuit il largue une fusée éclairante à intervalles ; à la fin il repart et disparaît au loin. Feux de navigation et
// d'anticollision ; phares d'atterrissage et phare de roulage (SearchLight) éteints.
// Temps sur zone : compté depuis l'arrivée, prolongé tant qu'il tient un soldat en vue (10 min au plus sur zone) ; il
// repart aussi quand plus aucun joueur n'est à moins de 1 500 m depuis une minute, ou quand la mission est finie (Recall).
// L'équipage : deux pilotes assis à bord dès l'apparition (SRP_EnemyComponent.SpawnHeliCrew) ; ce qu'ils voient alimente
// le système d'alerte comme n'importe quel groupe.
//
// Refonte du 24/09 (carte #11) :
// - le point éclairé est CONTINU : il glisse vers un but à vitesse bornée (10 m/s en fouille, 25 m/s sur un soldat). En
//   fouille, le but est relatif à l'appareil : vers l'intérieur du cercle, balayage de ±30°, de 35 à 55° sous l'horizon ;
// - repérage dans la tache réellement éclairée (une ellipse au sol, la nuit), ou à 30 m du point regardé (le jour, sans
//   lumière : il cherche « à vue ») ;
// - LE FEUILLAGE CACHE : sous un arbre ou un buisson d'au moins 2 m de haut, sous son feuillage (à 5 m au plus), arbres
//   morts exclus, jamais repéré de jour, et la nuit seulement s'il reste dans le faisceau (chance divisée par 10) ; un
//   soldat suivi qui passe sous les arbres est perdu (le faisceau ne l'y suit pas), sauf s'il reste au cœur du
//   faisceau, la nuit ;
// - soldat perdu de vue : le faisceau reste 3 s sur son dernier point vu, fouille 20 s autour, puis reprend sa fouille ;
// - ordres au projecteur deux fois par seconde, aux joueurs à moins de 3 km de l'appareil (et à ceux sans personnage,
//   Game Master ou en attente de réapparition) ; allumé sur zone et la
//   nuit seulement ; « éteint » répété au départ, à la chute, au retrait ;
// - chez le joueur (SRP_HeliLightClient), un faisceau visible attaché sous le nez de l'appareil, orienté comme une
//   tourelle, et une lumière d'appoint invisible posée SUR L'AXE du faisceau, côté hélicoptère, à 70 m du point éclairé,
//   qui éclaire vraiment la tache et projette les ombres des arbres dans le bon sens (appoint obligatoire par défaut : lui
//   seul éclaire le sol et les soldats ; bascule d'essai au menu Staff) ;
// - le faisceau est aussi DESSINÉ par maillage (ajout du 24/09) : un ruban tourné vers la caméra, du nez jusqu'à la
//   tache, créé par script (MeshObject.Create, matériau Assets/Helico/). Ce n'est pas une lumière : on le voit de loin,
//   là où le moteur ne rend plus la lampe du nez (le halo dessiné au nez a été retiré le 24/09 à la demande de Jack) ;
// - le phare du Mi-8 (son emplacement SearchLight, repris et orienté par script) a été essayé le 24/09 puis retiré à la
//   demande de Jack : une lumière posée sur l'appareil n'éclaire pas le sol vu du soldat (limite probable du moteur,
//   rapport Analyse_limite_eclairage_helico) ;
// - chute : feux coupés (anticollision 10 s plus tard), équipage retiré hors de vue, épave supprimée après 20 min sans
//   joueur à moins de 300 m.
//
// Lancé par le menu Staff (onglet Rapide, essais) et :
// - avec le Commandeur ennemi (carte #76, même gelé : plus aucun tirage), seulement sur son ordre HELICO
//   (SRP_CmdSupport.RequestHeli : carré rouge, stock d'hélico, repos, un seul appareil à la fois) ;
// - sans Commandeur, par les ALERTES : joueurs vus près d'une mission active ou sur un carré rouge, tirage (chance qui
//   monte à chaque tirage raté), assez de joueurs sur place, un seul appareil à la fois, repos entre deux sorties
//   (SRP_EnemyComponent.MaybeCallHeli, réglages de la catégorie Hélicoptère).
// Place (Q7) : l'équipage est HORS_COMPTE (SRP_CmdCapacity.Tag), il ne prend aucune des 120 places de soldats. Ce qu'il
// voit part au Commandeur comme un compte rendu direct (SRP_EnemyComponent.ReportSpotted). Journal : HELICO.
//------------------------------------------------------------------------------------------------

class SRP_HeliFlight
{
	IEntity m_Heli;
	ref SRP_EnemyGroup m_Crew;

	// Composants de l'appareil, trouvés une fois pour toutes au lancement
	VehicleHelicopterSimulation m_Simulation;
	BaseLightManagerComponent m_Lights;
	SCR_VehicleDamageManagerComponent m_Damage;
	RplComponent m_Rpl;
	RplId m_HeliId;				// gardé à part : sert encore à l'ordre « éteint » une fois l'appareil supprimé

	vector m_vCenter;
	float m_fRadius;			// rayon courant de la spirale ou du cercle
	float m_fOrbitRadius;		// rayon visé pour les cercles
	float m_fAngle;				// position sur le cercle, en radians
	float m_fAltitude;			// au-dessus du sol
	float m_fSpeed;				// m/s
	float m_fY;					// altitude absolue lissée
	float m_fRadialSpeed;		// vitesses courantes, amenées en douceur vers celles que la phase demande
	float m_fPathSpeed;
	float m_fYaw;				// attitude lissée, en degrés
	float m_fRoll;
	float m_fPitch;
	bool m_bAttitudeSet;
	vector m_vLast;
	int m_iPhase;				// 0 stationnaire (embarquement), 1 approche, 2 cercles, 3 départ
	int m_iHoldUntil;
	int m_iArrivalTick;			// arrivée sur zone (entrée en phase 2)
	int m_iLeaveTick;			// départ prévu : fixé à l'arrivée, repoussé tant qu'un soldat est tenu en vue
	int m_iNoPlayerSince;		// plus aucun joueur vivant à moins de 1 500 m depuis… (0 = il y en a)
	int m_iNextFlareTick;
	int m_iNextLightTick;		// feux de l'appareil réaffirmés, chaque seconde
	int m_iNextOrderTick;		// ordre du projecteur envoyé aux joueurs proches, deux fois par seconde
	bool m_bNight;				// fait-il nuit ? (relu chaque seconde)
	bool m_bTightened;

	// Recherche : le point éclairé (continu), le but vers lequel il glisse, et le soldat suivi
	vector m_vAim;
	vector m_vAimGoal;
	vector m_vAimVelocity;		// vitesse du point éclairé, en m/s (les joueurs l'extrapolent entre deux ordres)
	float m_fSearchTime;		// secondes passées sur zone : le temps propre au balayage, pas l'heure du serveur
	IEntity m_Target;
	bool m_bTargetVisible;		// le soldat suivi est vu en ce moment
	bool m_bTargetCovered;		// … mais sous les arbres (la nuit, au cœur du faisceau) : le point éclairé ne le suit plus
	vector m_vLastSeen;			// dernière position où il a été vu
	int m_iLostTick;			// perdu de vue depuis… (0 = vu, ou fouille autour de son dernier point finie)
	int m_iNextScanTick;
	int m_iNextDamageTick;
	bool m_bCrewSeen;			// les pilotes ont été vus assis à bord au moins une fois
}

//------------------------------------------------------------------------------------------------
//! Une épave d'hélicoptère de recherche, supprimée après un long moment sans joueur autour
class SRP_HeliWreck
{
	IEntity m_Wreck;
	int m_iSeenTick;			// dernier passage où un joueur était à moins de 300 m
}

class SRP_HeliSearch
{
	static ref array<ref SRP_HeliFlight> s_aFlights = {};
	static bool s_bRunning;
	static float s_fLastTime;
	protected static ref array<ref SRP_HeliWreck> s_aWrecks = {};
	protected static bool s_bWreckTick;

	// Réglages (posés par SRP_EnemyComponent au démarrage)
	static ResourceName s_sPrefab = "{5BF04078A1EC66D8}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MTT-A_Black_no_pylons.et";
	static float s_fAltitude = 120;
	static float s_fOrbitRadius = 250;
	static float s_fSpeed = 35;
	static int s_iMinutes = 6;
	static float s_fBank = 10;			// force du roulis en virage (10 = environ 16° sur le cercle de recherche ; 0 = à plat)
	static float s_fPitch = -3;			// assiette en vol, en degrés (négatif = nez bas)
	static int s_iFlareSeconds = 45;

	// Projecteur : en plus du faisceau, une lumière d'appoint invisible près du sol, chez le joueur. Obligatoire par
	// défaut (à l'essai du 24/09, sans elle le sol n'est pas éclairé vu d'en bas) ; bascule d'ESSAI au menu Staff (onglet
	// Rapide, Essais)
	static bool s_bFillLight = true;

	// Recherche (réglés ici, sans passer par le prefab du mode de jeu)
	static float s_fSpotRadius = 30;		// le jour (sans lumière) : un soldat à moins de … mètres du point regardé peut être repéré
	static float s_fBeamCone = 10;			// la nuit : ouverture du faisceau, en degrés (le projecteur chez le joueur lit ce même réglage)
	static float s_fBeamMargin = 3;			// … et cette marge autour de la tache, en mètres (l'appoint au sol la couvre aussi)
	static int s_iLoseSeconds = 3;			// le faisceau lâche un soldat resté invisible … secondes (toit, arbre, relief)
	static float s_fAimSearchSpeed = 10;	// vitesse du point éclairé en fouille, en m/s
	static float s_fAimChaseSpeed = 25;		// … pour rejoindre ou suivre un soldat
	static float s_fSweepAngle = 30;		// fouille : balayage de ± … degrés autour de la direction du centre du cercle…
	static float s_fSweepPeriod = 12;		// … en … secondes (aller et retour)
	static float s_fDipMin = 35;			// fouille : le faisceau descend entre … degrés sous l'horizon…
	static float s_fDipMax = 55;			// … et … degrés…
	static float s_fDipPeriod = 17;			// … en … secondes (aller et retour)
	static int s_iHoldSeconds = 3;			// soldat perdu de vue : le faisceau reste … secondes sur son dernier point vu…
	static int s_iLocalSearchSeconds = 20;	// … puis fouille … secondes autour de ce point…
	static float s_fLocalSearchRadius = 25;	// … en petite boucle de … mètres
	static float s_fCoverDistance = 5;		// le feuillage cache : un arbre ou un buisson à moins de … mètres (à plat) du soldat
	static float s_fCoverMinHeight = 2;		// … d'au moins … mètres de haut (ni herbe, ni plante basse, ni tronc couché)
	static float s_fCoverNightFactor = 0.1;	// la nuit, sous couvert : chance de repérage multipliée par … (il faut rester dans le faisceau)
	static float s_fCoverCoreFactor = 0.4;	// soldat suivi sous couvert, la nuit : encore vu à moins de … × le rayon de la tache du point éclairé
	static float s_fOrderDistance = 3000;	// ordres du projecteur envoyés aux joueurs à moins de … mètres de l'appareil
	static float s_fAbandonDistance = 1500;	// plus aucun joueur vivant à moins de … mètres de l'appareil…
	static int s_iAbandonSeconds = 60;		// … depuis … secondes : il repart
	static int s_iExtendSeconds = 60;		// soldat tenu en vue : départ repoussé à au moins … secondes…
	static int s_iMaxZoneMinutes = 10;		// … mais jamais plus de … minutes sur zone
	static float s_fWreckDistance = 300;	// épave : supprimée quand aucun joueur n'est à moins de … mètres…
	static int s_iWreckMinutes = 20;		// … depuis … minutes

	protected static const float START_DISTANCE = 2000;
	protected static const float EXIT_DISTANCE = 2500;
	protected static const int START_TRIES = 16;			// directions essayées pour le point de départ
	protected static const float MAP_MIN = 300;			// le point de départ doit rester dans la carte (x et z)
	protected static const float MAP_MAX = 12500;

	// Recherche d'un couvert autour d'un soldat (rappel de QueryEntitiesBySphere)
	protected static vector s_vCoverFrom;
	protected static bool s_bCoverFound;

	//------------------------------------------------------------------------------------------------
	//! Vols en cours. Un vol dont l'appareil n'existe plus (supprimé, ou resté inscrit d'une partie arrêtée) est retiré
	//! ici, comme Step le ferait : il ne bloque plus les sorties suivantes
	static int Count()
	{
		for (int i = s_aFlights.Count() - 1; i >= 0; i--)
		{
			SRP_HeliFlight flight = s_aFlights[i];
			if (flight && flight.m_Heli && !flight.m_Heli.IsDeleted())
				continue;
			if (flight)
				Remove(flight, false);
			s_aFlights.Remove(i);
		}
		return s_aFlights.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Nouvelle partie (deleteEntities : un appareil ou une épave encore là sont supprimés) ou fin de partie (false : les
	//! entités disparaissent avec le monde). Vols et épaves oubliés, boucles retirées de la file d'appels : un vol resté
	//! inscrit (partie Workbench arrêtée pendant un vol) bloquait toute nouvelle sortie sur alerte.
	static void ResetStatics(bool deleteEntities)
	{
		foreach (SRP_HeliFlight flight : s_aFlights)
		{
			if (!flight)
				continue;
			if (deleteEntities && flight.m_Heli && !flight.m_Heli.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(flight.m_Heli);
			flight.m_Heli = null;
			flight.m_Crew = null;
			flight.m_Target = null;
		}
		s_aFlights.Clear();
		foreach (SRP_HeliWreck wreck : s_aWrecks)
		{
			if (deleteEntities && wreck && wreck.m_Wreck && !wreck.m_Wreck.IsDeleted())
				SCR_EntityHelper.DeleteEntityAndChildren(wreck.m_Wreck);
		}
		s_aWrecks.Clear();
		s_bRunning = false;
		s_fLastTime = 0;
		s_bWreckTick = false;
		GetGame().GetCallqueue().Remove(Update);
		GetGame().GetCallqueue().Remove(WreckTick);
	}

	//------------------------------------------------------------------------------------------------
	//! Épaves encore inscrites au nettoyage (menu Staff)
	static int WreckCount()
	{
		return s_aWrecks.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Fait venir un hélicoptère de recherche au-dessus de « center ». Retourne le message pour le Staff.
	static string Launch(vector center, string author)
	{
		if (!Replication.IsServer())
			return "";
		if (Count() >= 1)
			return "Un hélicoptère de recherche est déjà en vol";
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !enemies.HasPrefabs())
			return "IA ennemie absente ou sans prefabs de groupes";

		Resource res = Resource.Load(s_sPrefab);
		if (!res || !res.IsValid())
			return "Prefab d'hélicoptère invalide : " + s_sPrefab;

		BaseWorld world = GetGame().GetWorld();
		vector start;
		float angle;
		string origin = PickStart(center, world, start, angle);
		float ground = start[1];
		start[1] = ground + s_fAltitude;

		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = start;
		IEntity heli = GetGame().SpawnEntityPrefab(res, world, params);
		if (!heli)
			return "L'hélicoptère n'a pas pu être créé";

		SRP_HeliFlight flight = new SRP_HeliFlight();
		flight.m_Heli = heli;
		flight.m_Simulation = VehicleHelicopterSimulation.Cast(heli.FindComponent(VehicleHelicopterSimulation));
		flight.m_Lights = BaseLightManagerComponent.Cast(heli.FindComponent(BaseLightManagerComponent));
		flight.m_Damage = SCR_VehicleDamageManagerComponent.Cast(heli.FindComponent(SCR_VehicleDamageManagerComponent));
		flight.m_Rpl = RplComponent.Cast(heli.FindComponent(RplComponent));
		flight.m_HeliId = RplId.Invalid();
		if (flight.m_Rpl)
			flight.m_HeliId = flight.m_Rpl.Id();
		flight.m_vCenter = center;
		flight.m_fRadius = START_DISTANCE;
		flight.m_fOrbitRadius = s_fOrbitRadius;
		flight.m_fAngle = angle;
		flight.m_fAltitude = s_fAltitude;
		flight.m_fSpeed = s_fSpeed;
		flight.m_fY = start[1];
		flight.m_vLast = start;
		flight.m_vAim = center;
		flight.m_vAimGoal = center;
		int now = System.GetTickCount();
		flight.m_iHoldUntil = now + 8000;
		flight.m_iLeaveTick = 0;	// fixé à l'arrivée sur zone

		// Moteur, rotors sans portance
		if (flight.m_Simulation)
		{
			flight.m_Simulation.EngineStart();
			flight.m_Simulation.SetThrottle(1);
			for (int rotor = 0; rotor < flight.m_Simulation.RotorCount(); rotor++)
			{
				flight.m_Simulation.RotorSetForceScaleState(rotor, 0);
				flight.m_Simulation.RotorSetTorqueScaleState(rotor, 0);
			}
		}
		// Les feux sont allumés par KeepLit, dès l'image suivante puis chaque seconde : posés dans la même image que
		// l'apparition, ils ne prenaient pas

		// Équipage : deux pilotes assis à bord tout de suite (à défaut, un groupe posé au sol sous l'appareil, embarqué)
		vector below = start;
		below[1] = ground;
		flight.m_Crew = enemies.SpawnHeliCrew(heli, below);
		// Q7 (#76) : l'équipage de l'hélicoptère ne compte pas dans la place des soldats (rien si la pose a échoué)
		SRP_CmdCapacity.Tag(flight.m_Crew, SRP_ECmdCapClass.HORS_COMPTE);

		s_aFlights.Insert(flight);
		// Boucle marquée en marche mais arrêtée sans passer par Update (monde rechargé : la file d'appels de l'ancienne
		// partie a disparu) : on la relance, comme le fait le projecteur chez le joueur
		float worldTime = world.GetWorldTime();
		if (s_bRunning && Math.AbsFloat(worldTime - s_fLastTime) > 1000)
		{
			GetGame().GetCallqueue().Remove(Update);
			s_bRunning = false;
		}
		if (!s_bRunning)
		{
			s_bRunning = true;
			s_fLastTime = worldTime;
			GetGame().GetCallqueue().CallLater(Update, 0, true);
		}

		int arrival = Math.Round(START_DISTANCE / s_fSpeed) + 35;
		SRP_EnemyComponent.Journal("HELICO", string.Format("Hélicoptère de recherche (prototype) lancé par %1 : départ à %2 m, %3, arrivée dans environ %4 s, %5 min sur zone (prolongées tant qu'il tient un soldat en vue, %6 min au plus)", author, START_DISTANCE, origin, arrival, s_iMinutes, s_iMaxZoneMinutes));
		return string.Format("Hélicoptère en route : il arrive de %1 m dans environ %2 s et tourne %3 min au-dessus de toi", START_DISTANCE, arrival, s_iMinutes);
	}

	//------------------------------------------------------------------------------------------------
	//! Point de départ à 2 km : 16 directions essayées, en partant de celle du territoire ennemi (EnemyDirection ; sinon
	//! d'un angle au hasard), puis de plus en plus loin de part et d'autre ; on garde la première sur la terre ferme (sol à
	//! plus de 2 m) et dans la carte. À défaut, l'ancien tirage au hasard. « start » reçoit le point AU SOL, « angle » la
	//! position sur le cercle. Retourne une description pour le journal.
	protected static string PickStart(vector center, BaseWorld world, out vector start, out float angle)
	{
		float direction;
		string enemySide = EnemyDirection(center, direction);
		float firstAngle = Math.RandomFloat(0, Math.PI2);
		if (!enemySide.IsEmpty())
			firstAngle = direction;

		float slice = Math.PI2 / START_TRIES;
		for (int i = 0; i < START_TRIES; i++)
		{
			// 0, -1, +1, -2, +2… pas d'angle autour de la direction de départ
			int step = (i + 1) / 2;
			if (i % 2 == 1)
				step = -step;
			float candidateAngle = firstAngle + step * slice;
			vector candidate = center + Vector(Math.Cos(candidateAngle) * START_DISTANCE, 0, Math.Sin(candidateAngle) * START_DISTANCE);
			if (candidate[0] < MAP_MIN || candidate[0] > MAP_MAX || candidate[2] < MAP_MIN || candidate[2] > MAP_MAX)
				continue;
			float surface = world.GetSurfaceY(candidate[0], candidate[2]);
			if (surface <= 2)
				continue;	// la mer, ou le rivage
			candidate[1] = surface;
			start = candidate;
			angle = candidateAngle;
			if (!enemySide.IsEmpty())
				return "sur la terre ferme, du côté du territoire ennemi, vers " + enemySide;
			return "sur la terre ferme";
		}

		// À défaut : l'ancien tirage, au hasard (au-dessus de la mer, le sol compte pour 0)
		float fallbackAngle = Math.RandomFloat(0, Math.PI2);
		vector fallback = center + Vector(Math.Cos(fallbackAngle) * START_DISTANCE, 0, Math.Sin(fallbackAngle) * START_DISTANCE);
		fallback[1] = Math.Max(world.GetSurfaceY(fallback[0], fallback[2]), 0);
		start = fallback;
		angle = fallbackAngle;
		return "au hasard (aucune direction sur la terre ferme)";
	}

	//------------------------------------------------------------------------------------------------
	//! Direction (angle sur le cercle, en radians, comme le départ) du territoire ennemi vu du lieu : l'hélicoptère « vient
	//! de chez eux ». D'abord le front (SRP_FrontComponent.RedDirection : moyenne des directions des carrés rouges entre
	//! 500 et 3 000 m, rendue en degrés) ; à défaut (front pas prêt, aucun carré rouge à cette distance), la localité
	//! ennemie la plus proche à plus de 500 m (celle où se trouve le lieu ne donne pas de direction). Retourne le nom de la
	//! zone rouge la plus proche ou de la localité, "" s'il n'y a rien (départ au hasard).
	protected static string EnemyDirection(vector center, out float direction)
	{
		direction = 0;
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front && front.IsReady())
		{
			float degrees = 0;
			string redZone = front.RedDirection(center, degrees);
			if (!redZone.IsEmpty())
			{
				direction = degrees * Math.DEG2RAD;
				return redZone;
			}
		}

		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return "";
		array<SRP_SectorState> localities = {};
		territory.GetEnemyLocalities(localities);
		string best = "";
		float bestDistance = 1000000;
		foreach (SRP_SectorState state : localities)
		{
			if (!state || !state.m_Sector)
				continue;
			vector toLocality = state.m_Sector.GetCenter() - center;
			toLocality[1] = 0;
			float distance = toLocality.Length();
			if (distance < 500 || distance >= bestDistance)
				continue;
			bestDistance = distance;
			best = state.m_sName;
			direction = Math.Atan2(toLocality[2], toLocality[0]);
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire tout de suite l'hélicoptère, son équipage et les épaves
	static string StopAll()
	{
		int count = s_aFlights.Count();
		for (int i = s_aFlights.Count() - 1; i >= 0; i--)
			Remove(s_aFlights[i], true);
		s_aFlights.Clear();
		int wrecks = ClearWrecks();
		return string.Format("%1 hélicoptère(s) retiré(s), %2 épave(s) supprimée(s)", count, wrecks);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de mission : tout hélicoptère encore en route ou sur zone, dont le lieu est à moins de « radius » du site, repart
	//! (y compris s'il attend encore son équipage au point de départ)
	static void Recall(vector site, float radius)
	{
		foreach (SRP_HeliFlight flight : s_aFlights)
		{
			if (!flight || flight.m_iPhase >= 3)
				continue;
			if (vector.DistanceXZ(flight.m_vCenter, site) > radius)
				continue;
			Depart(flight, "mission terminée");
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque seconde : moteur, et feux de l'appareil. Navigation (Presence) et anticollision (Hazard) allumés ; les phares
	//! d'atterrissage (Head, HiBeam) restent ÉTEINTS (ils pendaient sous l'appareil), de même que le phare de roulage
	//! (SearchLight : il n'éclaire pas le sol de si haut, c'est notre projecteur qui le fait).
	protected static void KeepLit(SRP_HeliFlight flight)
	{
		if (flight.m_Simulation && !flight.m_Simulation.EngineIsOn())
		{
			flight.m_Simulation.EngineStart();
			flight.m_Simulation.SetThrottle(1);
		}
		BaseLightManagerComponent lights = flight.m_Lights;
		if (!lights)
			return;
		if (!lights.GetLightsState(ELightType.Presence))
			lights.SetLightsState(ELightType.Presence, true);
		if (!lights.GetLightsState(ELightType.Hazard))
			lights.SetLightsState(ELightType.Hazard, true);
		if (lights.GetLightsState(ELightType.Head))
			lights.SetLightsState(ELightType.Head, false);
		if (lights.GetLightsState(ELightType.HiBeam))
			lights.SetLightsState(ELightType.HiBeam, false);
		if (lights.GetLightsState(ELightType.SearchLight))
			lights.SetLightsState(ELightType.SearchLight, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Le projecteur est-il allumé ? Sur zone et la nuit seulement (l'ordre « allumé » aux joueurs suit cette règle).
	protected static bool IsBeamOn(SRP_HeliFlight flight)
	{
		return flight.m_iPhase == 2 && flight.m_bNight;
	}

	//------------------------------------------------------------------------------------------------
	//! Deux fois par seconde : l'ordre du projecteur aux joueurs dont le personnage est à moins de 3 km de l'appareil, et à
	//! ceux qui n'en ont pas (mort, Game Master) : sans l'appareil chez eux, le projecteur ne crée rien. Allumé seulement
	//! sur zone et la nuit ; le soldat suivi n'est donné que s'il est vu.
	protected static void SendOrder(SRP_HeliFlight flight, BaseWorld world)
	{
		bool on = IsBeamOn(flight);
		if (!flight.m_HeliId.IsValid())
			return;
		vector aim = flight.m_vAim;
		aim[1] = world.GetSurfaceY(aim[0], aim[2]);
		vector aimVelocity = vector.Zero;
		RplId targetId = RplId.Invalid();
		if (on)
		{
			aimVelocity = flight.m_vAimVelocity;
			if (flight.m_Target && flight.m_bTargetVisible && !flight.m_bTargetCovered && !flight.m_Target.IsDeleted())
			{
				RplComponent targetRpl = RplComponent.Cast(flight.m_Target.FindComponent(RplComponent));
				if (targetRpl)
					targetId = targetRpl.Id();
			}
		}

		vector heliPosition = flight.m_Heli.GetOrigin();
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character && vector.Distance(character.GetOrigin(), heliPosition) > s_fOrderDistance)
				continue;
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
			if (pc)
				pc.SRP_HeliLight(flight.m_HeliId, aim, aimVelocity, on, targetId, s_bFillLight);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! L'ordre « éteint » à tous les joueurs, « count » fois à trois quarts de seconde d'intervalle (l'ordre voyage sans
	//! garantie d'arrivée). Sert aussi quand l'appareil n'existe plus : on ne garde que son RplId.
	protected static void SendOff(RplId heliId, int count)
	{
		if (!heliId.IsValid())
			return;
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(id));
			if (pc)
				pc.SRP_HeliLight(heliId, vector.Zero, vector.Zero, false, RplId.Invalid(), false);
		}
		if (count > 1)
			GetGame().GetCallqueue().CallLater(SendOff, 750, false, heliId, count - 1);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Remove(SRP_HeliFlight flight, bool deleteHeli)
	{
		SendOff(flight.m_HeliId, 3);
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies && flight.m_Crew)
			enemies.Delete(flight.m_Crew);
		flight.m_Crew = null;
		flight.m_Target = null;
		if (deleteHeli && flight.m_Heli && !flight.m_Heli.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(flight.m_Heli);
		flight.m_Heli = null;
	}

	//------------------------------------------------------------------------------------------------
	//! À chaque image, côté serveur
	static void Update()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		float time = world.GetWorldTime();
		float dt = (time - s_fLastTime) / 1000;
		s_fLastTime = time;
		if (dt <= 0)
			return;
		if (dt > 0.1)
			dt = 0.1;

		int now = System.GetTickCount();
		for (int i = s_aFlights.Count() - 1; i >= 0; i--)
		{
			if (!Step(s_aFlights[i], dt, now, world))
				s_aFlights.Remove(i);
		}

		if (s_aFlights.IsEmpty())
		{
			s_bRunning = false;
			GetGame().GetCallqueue().Remove(Update);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Avance un vol d'un pas. Retourne false quand le vol est fini.
	protected static bool Step(SRP_HeliFlight flight, float dt, int now, BaseWorld world)
	{
		IEntity heli = flight.m_Heli;
		if (!heli || heli.IsDeleted())
		{
			Remove(flight, false);
			return false;
		}
		// Abattu : on lâche les commandes, la physique du jeu fait le reste (chute, épave)
		if (SRP_GarageUtils.IsDestroyed(heli))
		{
			SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche abattu");
			Fall(flight);
			return false;
		}

		if (now >= flight.m_iNextLightTick)
		{
			flight.m_iNextLightTick = now + 1000;
			KeepLit(flight);
			SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
			flight.m_bNight = enemies && enemies.IsNightNow();
		}

		// Touché : moteur hors service ou dans le rouge, rotor détruit, cellule très abîmée, pilotes tués. On lâche les
		// commandes et on laisse la physique du jeu le faire tomber.
		if (now >= flight.m_iNextDamageTick && flight.m_iPhase >= 1)
		{
			flight.m_iNextDamageTick = now + 500;
			string failure = Failure(flight);
			if (!failure.IsEmpty())
			{
				Crash(flight, failure);
				return false;
			}
		}

		// Ce que la phase demande : une vitesse radiale (vers le lieu ou au large) ; le reste de la vitesse va le long du
		// cercle. Les vitesses courantes y sont amenées avec une accélération bornée : pas d'angle dans la trajectoire.
		float speed = flight.m_fSpeed;
		float wantedRadial = 0;
		bool moving = true;

		if (flight.m_iPhase == 0)
		{
			moving = false;
			if (now >= flight.m_iHoldUntil)
				flight.m_iPhase = 1;
		}
		else if (flight.m_iPhase == 1)
		{
			// Approche : vite de loin, de plus en plus tangente en arrivant sur le cercle (la spirale s'enroule)
			float remaining = flight.m_fRadius - flight.m_fOrbitRadius;
			wantedRadial = -Math.Min(0.85 * speed, remaining * 0.12 + 1.5);
			if (remaining <= 4)
				Arrive(flight, now, world);
		}
		else if (flight.m_iPhase == 2)
		{
			Search(flight, dt, now, world);
			// Le rayon rejoint doucement le rayon visé (resserré sur une alerte)
			wantedRadial = Math.Clamp((flight.m_fOrbitRadius - flight.m_fRadius) * 0.15, -8, 8);
			if (flight.m_iPhase == 2 && now >= flight.m_iLeaveTick)
				Depart(flight, "temps sur zone écoulé");
		}
		else
		{
			// Départ : le cercle s'ouvre peu à peu en spirale, puis l'appareil file au large
			wantedRadial = 0.85 * speed;
			if (flight.m_fRadius >= EXIT_DISTANCE)
			{
				SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche reparti hors de vue : retiré avec son équipage");
				Remove(flight, true);
				return false;
			}
		}

		float radialStep = 2.5 * dt;	// m/s² : une dizaine de secondes pour passer du cercle à la fuite
		flight.m_fRadialSpeed = flight.m_fRadialSpeed + Math.Clamp(wantedRadial - flight.m_fRadialSpeed, -radialStep, radialStep);
		float wantedPath = 0;
		if (moving)
			wantedPath = Math.Sqrt(Math.Max(speed * speed - flight.m_fRadialSpeed * flight.m_fRadialSpeed, 0));
		float pathStep = 4 * dt;
		flight.m_fPathSpeed = flight.m_fPathSpeed + Math.Clamp(wantedPath - flight.m_fPathSpeed, -pathStep, pathStep);
		if (!moving)
			flight.m_fRadialSpeed = 0;

		flight.m_fRadius = Math.Max(flight.m_fRadius + flight.m_fRadialSpeed * dt, 50);
		flight.m_fAngle = flight.m_fAngle + (flight.m_fPathSpeed / flight.m_fRadius) * dt;
		if (flight.m_fAngle > Math.PI2)
			flight.m_fAngle = flight.m_fAngle - Math.PI2;

		vector position = flight.m_vCenter + Vector(Math.Cos(flight.m_fAngle) * flight.m_fRadius, 0, Math.Sin(flight.m_fAngle) * flight.m_fRadius);

		// Altitude : au-dessus du plus haut entre le sol sous l'appareil et le sol du lieu, lissée
		float ground = Math.Max(world.GetSurfaceY(position[0], position[2]), world.GetSurfaceY(flight.m_vCenter[0], flight.m_vCenter[2]));
		ground = Math.Max(ground, 0);
		float wantedY = ground + flight.m_fAltitude;
		flight.m_fY = flight.m_fY + (wantedY - flight.m_fY) * Math.Min(1, dt * 0.6);
		position[1] = flight.m_fY;

		vector velocity = (position - flight.m_vLast) * (1 / dt);
		flight.m_vLast = position;

		// Attitude : le nez suit la marche avec un taux de virage borné ; le roulis vient du taux de virage (il penche
		// vers l'intérieur du virage, d'autant plus que le virage est serré) ; le nez baisse avec la vitesse. Tout est lissé.
		if (!flight.m_bAttitudeSet)
		{
			vector current[4];
			heli.GetTransform(current);
			flight.m_fYaw = Math.Atan2(current[2][0], current[2][2]) * Math.RAD2DEG;
			flight.m_bAttitudeSet = true;
		}
		vector flat = velocity;
		flat[1] = 0;
		float groundSpeed = flat.Length();
		float yawRate = 0;
		if (groundSpeed > 1)
		{
			float turn = Math.Atan2(flat[0], flat[2]) * Math.RAD2DEG - flight.m_fYaw;
			while (turn > 180)
				turn = turn - 360;
			while (turn < -180)
				turn = turn + 360;
			float maxTurn = 45 * dt;
			float applied = Math.Clamp(turn, -maxTurn, maxTurn);
			flight.m_fYaw = flight.m_fYaw + applied;
			yawRate = applied / dt;
		}
		float wantedRoll = Math.Clamp(yawRate * 0.2 * s_fBank, -28, 28);
		float wantedPitch = s_fPitch * Math.Min(groundSpeed / Math.Max(speed, 1), 1.5);
		float ease = Math.Min(1, dt * 1.2);
		flight.m_fRoll = flight.m_fRoll + (wantedRoll - flight.m_fRoll) * ease;
		flight.m_fPitch = flight.m_fPitch + (wantedPitch - flight.m_fPitch) * ease;
		float yaw = flight.m_fYaw;
		float roll = flight.m_fRoll;
		float pitch = flight.m_fPitch;

		vector transform[4];
		Math3D.AnglesToMatrix(Vector(yaw, pitch, roll), transform);
		transform[3] = position;
		heli.SetTransform(transform);

		Physics physics = heli.GetPhysics();
		if (physics)
		{
			physics.SetVelocity(velocity);
			physics.SetAngularVelocity(vector.Zero);
		}

		// Ordre du projecteur, deux fois par seconde (après le déplacement : point éclairé à jour)
		if (now >= flight.m_iNextOrderTick)
		{
			flight.m_iNextOrderTick = now + 500;
			SendOrder(flight, world);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Arrivée sur zone : le temps sur zone commence ; le point éclairé part de devant l'appareil, vers l'intérieur du
	//! cercle (et non du centre, là où sont les joueurs)
	protected static void Arrive(SRP_HeliFlight flight, int now, BaseWorld world)
	{
		flight.m_iPhase = 2;
		flight.m_iArrivalTick = now;
		flight.m_iLeaveTick = now + s_iMinutes * 60 * 1000;
		flight.m_iNextFlareTick = now + 15000;
		flight.m_iNoPlayerSince = 0;
		flight.m_fSearchTime = 0;
		flight.m_vAim = SearchGoal(flight, world);
		flight.m_vAimGoal = flight.m_vAim;
		flight.m_vAimVelocity = vector.Zero;
		SRP_EnemyComponent.Journal("HELICO", string.Format("Hélicoptère de recherche sur zone (grille %1 / %2) : %3 min de fouille", Math.Round(flight.m_vCenter[0]), Math.Round(flight.m_vCenter[2]), s_iMinutes));
	}

	//------------------------------------------------------------------------------------------------
	//! Il repart : plus de soldat suivi, projecteur éteint (l'ordre « éteint » part tout de suite, puis deux fois par
	//! seconde jusqu'au retrait de l'appareil)
	protected static void Depart(SRP_HeliFlight flight, string reason)
	{
		if (flight.m_iPhase >= 3)
			return;
		flight.m_iPhase = 3;
		flight.m_Target = null;
		flight.m_bTargetVisible = false;
		flight.m_bTargetCovered = false;
		flight.m_iLostTick = 0;
		flight.m_vAimVelocity = vector.Zero;
		flight.m_iNextOrderTick = 0;
		SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche : il repart (" + reason + ")");
	}

	//------------------------------------------------------------------------------------------------
	//! Ce qui empêche l'appareil de voler, ou "" s'il va bien
	protected static string Failure(SRP_HeliFlight flight)
	{
		SCR_VehicleDamageManagerComponent damage = flight.m_Damage;
		if (damage)
		{
			if (!damage.GetEngineFunctional())
				return "moteur hors service";
			if (damage.GetHealthScaled() < 0.45)
				return "cellule très endommagée";
			array<HitZone> zones = {};
			damage.GetAllHitZones(zones);
			foreach (HitZone zone : zones)
			{
				if (SCR_RotorHitZone.Cast(zone) && zone.GetDamageState() == EDamageState.DESTROYED)
					return "rotor détruit";
				if (SCR_EngineHitZone.Cast(zone) && zone.GetHealthScaled() < 0.35)
					return "moteur dans le rouge";
			}
		}

		// Pilotes hors de combat : plus aucun membre de l'équipage vivant, conscient et ASSIS DANS CET APPAREIL (compté
		// seulement une fois l'équipage vu à bord)
		if (flight.m_Crew)
		{
			int aboard = CrewAboard(flight);
			if (aboard > 0)
			{
				if (!flight.m_bCrewSeen)
					SRP_EnemyComponent.Journal("HELICO", string.Format("Hélicoptère de recherche : équipage à bord (%1 pilote(s))", aboard));
				flight.m_bCrewSeen = true;
			}
			else if (flight.m_bCrewSeen)
				return "pilotes hors de combat";
		}
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Membres de l'équipage vivants, conscients et assis dans cet appareil
	protected static int CrewAboard(SRP_HeliFlight flight)
	{
		array<IEntity> members = {};
		SRP_EnemyComponent.GetMembers(flight.m_Crew, members);
		int count = 0;
		foreach (IEntity member : members)
		{
			if (SRP_Utils.IsDead(member))
				continue;
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(member.FindComponent(SCR_CharacterControllerComponent));
			if (controller && controller.IsUnconscious())
				continue;
			if (!SRP_EnemyComponent.IsSeatedIn(member, flight.m_Heli))
				continue;
			count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! On lâche les commandes : l'appareil garde son élan, un reste de portance et part en vrille ; la physique et les
	//! dégâts du jeu font le reste (chute, choc, explosion, épave)
	protected static void Crash(SRP_HeliFlight flight, string reason)
	{
		IEntity heli = flight.m_Heli;
		if (flight.m_Simulation)
		{
			for (int rotor = 0; rotor < flight.m_Simulation.RotorCount(); rotor++)
			{
				flight.m_Simulation.RotorSetForceScaleState(rotor, 0.4);
				flight.m_Simulation.RotorSetTorqueScaleState(rotor, 1);
			}
		}
		Physics physics = heli.GetPhysics();
		if (physics)
			physics.SetAngularVelocity(Vector(0.25, 1.4, 0.35));

		SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche touché (" + reason + ") : il tombe");
		Fall(flight);
	}

	//------------------------------------------------------------------------------------------------
	//! La chute (touché ou abattu) : projecteur éteint (ordre répété), phares et feux de navigation coupés tout de suite,
	//! anticollision 10 s plus tard ; l'équipage survivant est retiré hors de vue comme les survivants d'une mission ;
	//! l'épave est inscrite au nettoyage (supprimée après 20 min sans joueur à moins de 300 m).
	protected static void Fall(SRP_HeliFlight flight)
	{
		SendOff(flight.m_HeliId, 3);
		BaseLightManagerComponent lights = flight.m_Lights;
		if (lights)
		{
			lights.SetLightsState(ELightType.Head, false);
			lights.SetLightsState(ELightType.HiBeam, false);
			lights.SetLightsState(ELightType.SearchLight, false);
			lights.SetLightsState(ELightType.Presence, false);
		}
		IEntity heli = flight.m_Heli;
		if (heli && !heli.IsDeleted())
		{
			GetGame().GetCallqueue().CallLater(HazardOff, 10000, false, heli);
			AddWreck(heli);
		}

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies && flight.m_Crew)
		{
			array<ref SRP_EnemyGroup> crew = {};
			crew.Insert(flight.m_Crew);
			enemies.RetireLater(crew);
		}
		flight.m_Target = null;
		flight.m_Crew = null;
		flight.m_Heli = null;
	}

	//------------------------------------------------------------------------------------------------
	//! 10 s après la chute : le feu anticollision s'éteint à son tour (l'épave existe peut-être encore, peut-être plus)
	protected static void HazardOff(IEntity heli)
	{
		if (!heli || heli.IsDeleted())
			return;
		BaseLightManagerComponent lights = BaseLightManagerComponent.Cast(heli.FindComponent(BaseLightManagerComponent));
		if (lights)
			lights.SetLightsState(ELightType.Hazard, false);
	}

	//------------------------------------------------------------------------------------------------
	// Épaves : une liste à part, vérifiée toutes les 30 s par son propre appel différé (le vol, lui, est fini)
	//------------------------------------------------------------------------------------------------
	protected static void AddWreck(IEntity heli)
	{
		foreach (SRP_HeliWreck known : s_aWrecks)
		{
			if (known && known.m_Wreck == heli)
				return;
		}
		SRP_HeliWreck wreck = new SRP_HeliWreck();
		wreck.m_Wreck = heli;
		wreck.m_iSeenTick = System.GetTickCount();
		s_aWrecks.Insert(wreck);
		if (!s_bWreckTick)
		{
			s_bWreckTick = true;
			GetGame().GetCallqueue().CallLater(WreckTick, 30000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 30 s : une épave sans joueur à moins de 300 m depuis 20 min est supprimée
	protected static void WreckTick()
	{
		int now = System.GetTickCount();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		for (int i = s_aWrecks.Count() - 1; i >= 0; i--)
		{
			SRP_HeliWreck wreck = s_aWrecks[i];
			if (!wreck || !wreck.m_Wreck || wreck.m_Wreck.IsDeleted())
			{
				s_aWrecks.Remove(i);
				continue;
			}
			if (SRP_Utils.NearestPlayerDistance(wreck.m_Wreck.GetOrigin(), players) < s_fWreckDistance)
			{
				wreck.m_iSeenTick = now;
				continue;
			}
			if (now - wreck.m_iSeenTick < s_iWreckMinutes * 60 * 1000)
				continue;
			SCR_EntityHelper.DeleteEntityAndChildren(wreck.m_Wreck);
			s_aWrecks.Remove(i);
			SRP_EnemyComponent.Journal("HELICO", string.Format("Épave de l'hélicoptère de recherche supprimée (aucun joueur à moins de %1 m depuis %2 min)", s_fWreckDistance, s_iWreckMinutes));
		}
		if (s_aWrecks.IsEmpty())
		{
			s_bWreckTick = false;
			GetGame().GetCallqueue().Remove(WreckTick);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Supprime toutes les épaves (Staff) ; retourne leur nombre
	protected static int ClearWrecks()
	{
		int count = 0;
		foreach (SRP_HeliWreck wreck : s_aWrecks)
		{
			if (wreck && wreck.m_Wreck && !wreck.m_Wreck.IsDeleted())
			{
				SCR_EntityHelper.DeleteEntityAndChildren(wreck.m_Wreck);
				count++;
			}
		}
		s_aWrecks.Clear();
		if (s_bWreckTick)
		{
			s_bWreckTick = false;
			GetGame().GetCallqueue().Remove(WreckTick);
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Le faisceau voit-il ce soldat ? Une ligne droite de l'appareil au soldat, arrêtée par le relief, les murs, les
	//! toits, les troncs (pas par les feuilles : voir IsUnderCover).
	protected static bool HasLineOfSight(SRP_HeliFlight flight, IEntity soldier)
	{
		TraceParam trace = new TraceParam();
		trace.Start = flight.m_Heli.GetOrigin() - vector.Up * 3;
		trace.End = soldier.GetOrigin() + vector.Up * 0.8;
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		array<IEntity> excluded = {};
		excluded.Insert(flight.m_Heli);
		excluded.Insert(soldier);
		trace.ExcludeArray = excluded;
		return GetGame().GetWorld().TraceMove(trace, null) >= 0.99;
	}

	//------------------------------------------------------------------------------------------------
	//! LE FEUILLAGE CACHE : un arbre ou un buisson (entités de classe BaseTree du jeu, buissons compris) d'au moins 2 m de
	//! haut, sous son feuillage (à 5 m au plus, à plat, du soldat), arbres morts exclus. Appelé seulement pour un soldat
	//! déjà dans la zone de repérage (coût).
	protected static bool IsUnderCover(IEntity soldier)
	{
		s_vCoverFrom = soldier.GetOrigin();
		s_bCoverFound = false;
		GetGame().GetWorld().QueryEntitiesBySphere(s_vCoverFrom, s_fCoverDistance + 1, CoverCandidate, null, EQueryEntitiesFlags.ALL);
		return s_bCoverFound;
	}

	//------------------------------------------------------------------------------------------------
	//! Rappel de la recherche de couvert : ne retient que les arbres et buissons, à moins de 5 m à plat
	protected static bool CoverCandidate(IEntity entity)
	{
		if (!entity || !BaseTree.Cast(entity))
			return true;
		float distance = vector.DistanceXZ(entity.GetOrigin(), s_vCoverFrom);
		if (distance > s_fCoverDistance)
			return true;
		// Un arbre mort (sans feuilles) ne cache rien
		TreeClass treeClass = TreeClass.Cast(entity.GetPrefabData());
		if (treeClass && treeClass.SoundType == ETreeSoundTypes.Withered)
			return true;
		// Ni une plante basse, une touffe d'herbe ou un tronc couché (classés « arbre » eux aussi) ; et il faut être SOUS le
		// feuillage : pas plus loin du pied que la moitié de sa largeur, plus 1 m
		vector mins, maxs;
		entity.GetBounds(mins, maxs);
		float scale = entity.GetScale();
		if ((maxs[1] - mins[1]) * scale < s_fCoverMinHeight)
			return true;
		float spread = Math.Max(maxs[0] - mins[0], maxs[2] - mins[2]) * 0.5 * scale + 1;
		if (distance > spread)
			return true;
		s_bCoverFound = true;
		return false;	// trouvé : inutile de chercher plus loin
	}

	//------------------------------------------------------------------------------------------------
	//! Pourquoi ce soldat ne peut pas (ou plus) être suivi : "" s'il peut l'être ; mort, supprimé, inconscient, en véhicule
	protected static string DropReason(IEntity soldier)
	{
		if (!soldier || soldier.IsDeleted())
			return "disparu";
		if (SRP_Utils.IsDead(soldier))
			return "mort";
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(soldier.FindComponent(SCR_CharacterControllerComponent));
		if (controller && controller.IsUnconscious())
			return "inconscient";
		SCR_CompartmentAccessComponent access = SCR_CompartmentAccessComponent.Cast(soldier.FindComponent(SCR_CompartmentAccessComponent));
		if (access && access.IsInCompartment())
			return "monté en véhicule";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected static string PlayerName(IEntity soldier)
	{
		if (!soldier)
			return "?";
		return GetGame().GetPlayerManager().GetPlayerName(SRP_Utils.GetPlayerIdFromEntity(soldier));
	}

	//------------------------------------------------------------------------------------------------
	//! Rayon de la tache éclairée, perpendiculairement au faisceau : distance lampe → point × tan(cône / 2) + marge
	protected static float BeamRadius(SRP_HeliFlight flight)
	{
		float slant = vector.Distance(flight.m_Heli.GetOrigin(), flight.m_vAim);
		return slant * Math.Tan(s_fBeamCone * 0.5 * Math.DEG2RAD) + s_fBeamMargin;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce point est-il dans la zone de repérage ? La nuit : dans la tache réellement éclairée, une ellipse au sol (le
	//! faisceau frappe le sol en biais : elle s'allonge dans la direction appareil → point, d'autant plus que le faisceau
	//! est rasant). Le jour, sans lumière : à moins de 30 m du point regardé.
	protected static bool InSpotZone(SRP_HeliFlight flight, vector position, bool night)
	{
		vector offset = position - flight.m_vAim;
		offset[1] = 0;
		if (!night)
			return offset.Length() <= s_fSpotRadius;

		vector lamp = flight.m_Heli.GetOrigin();
		float r = BeamRadius(flight);
		vector along = flight.m_vAim - lamp;
		along[1] = 0;
		float flatDistance = along.Length();
		if (flatDistance < 1)
			return offset.Length() <= r;	// lampe à la verticale du point : un cercle
		along = along * (1 / flatDistance);
		float slant = vector.Distance(lamp, flight.m_vAim);
		float sine = Math.Clamp((lamp[1] - flight.m_vAim[1]) / Math.Max(slant, 1), 0.1, 1);
		float major = r / sine;			// demi-grand axe, dans la direction appareil → point
		vector across = Vector(-along[2], 0, along[0]);
		float x = vector.Dot(offset, along) / major;
		float y = vector.Dot(offset, across) / r;
		return x * x + y * y <= 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Le soldat suivi est lâché : le faisceau reste 3 s sur son dernier point vu, fouille 20 s autour, puis reprend sa
	//! fouille normale
	protected static void Release(SRP_HeliFlight flight, int lostTick, string reason)
	{
		IEntity held = flight.m_Target;
		flight.m_Target = null;
		flight.m_bTargetVisible = false;
		flight.m_bTargetCovered = false;
		flight.m_iLostTick = lostTick;
		SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche : soldat lâché (" + PlayerName(held) + ", " + reason + "), il fouille autour de son dernier point vu");
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque seconde. Un soldat suivi reste suivi tant qu'il est VISIBLE (ligne de vue, et pas sous les arbres, sauf la
	//! nuit au cœur du faisceau) ; invisible 3 s, il est lâché ; mort, inconscient ou en véhicule, tout de suite.
	//! Sinon, fouille : un soldat dans la zone de repérage, en vue, risque d'être repéré (debout bien plus que couché, en
	//! mouvement plus qu'immobile, plus facilement de jour ; sous les arbres jamais de jour, et la nuit dix fois moins).
	//! Repéré, le faisceau se fixe sur lui et les troupes au sol sont renseignées.
	protected static void Scan(SRP_HeliFlight flight, SRP_EnemyComponent enemies, int now)
	{
		bool night = flight.m_bNight;
		if (flight.m_Target)
		{
			IEntity held = flight.m_Target;
			string reason = DropReason(held);
			if (!reason.IsEmpty())
			{
				int lostTick = now;
				if (!flight.m_bTargetVisible && flight.m_iLostTick != 0)
					lostTick = flight.m_iLostTick;
				Release(flight, lostTick, reason);
				return;
			}

			bool visible = HasLineOfSight(flight, held);
			bool heldCovered = visible && IsUnderCover(held);
			if (heldCovered)
			{
				// Sous les arbres : caché, sauf la nuit s'il reste au cœur du faisceau (« vraiment sous le projecteur »). Le point
				// éclairé ne le suit plus sous le feuillage : il reste là où il l'a vu à découvert
				visible = night && vector.DistanceXZ(held.GetOrigin(), flight.m_vAim) < s_fCoverCoreFactor * BeamRadius(flight);
			}
			flight.m_bTargetCovered = heldCovered;
			if (visible)
			{
				flight.m_bTargetVisible = true;
				flight.m_iLostTick = 0;
				if (!heldCovered)
					flight.m_vLastSeen = held.GetOrigin();
				enemies.ReportSpotted(held.GetOrigin());
				// Temps sur zone : tant qu'il tient un soldat en vue, il reste au moins une minute de plus (10 min au plus)
				int extended = now + s_iExtendSeconds * 1000;
				int latest = flight.m_iArrivalTick + s_iMaxZoneMinutes * 60 * 1000;
				if (extended > latest)
					extended = latest;
				if (extended > flight.m_iLeaveTick)
					flight.m_iLeaveTick = extended;
				return;
			}
			if (flight.m_bTargetVisible || flight.m_iLostTick == 0)
				flight.m_iLostTick = now;
			flight.m_bTargetVisible = false;
			if (now - flight.m_iLostTick >= s_iLoseSeconds * 1000)
				Release(flight, flight.m_iLostTick, "perdu de vue");
			return;
		}

		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		foreach (IEntity player : players)
		{
			string unfit = DropReason(player);
			if (!unfit.IsEmpty())
				continue;	// mort, inconscient ou en véhicule : le faisceau ne s'y fixe pas
			if (!InSpotZone(flight, player.GetOrigin(), night))
				continue;

			float chance = 35;
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(player.FindComponent(SCR_CharacterControllerComponent));
			if (controller)
			{
				if (controller.GetStance() == ECharacterStance.CROUCH)
					chance = 18;
				else if (controller.GetStance() == ECharacterStance.PRONE)
					chance = 6;
			}
			Physics body = player.GetPhysics();
			if (body && body.GetVelocity().Length() > 2)
				chance = chance * 2;
			if (!night)
				chance = chance * 1.5;
			// Le feuillage cache : de jour, jamais repéré sous les arbres ; la nuit, seulement s'il reste dans le faisceau
			bool covered = IsUnderCover(player);
			if (covered)
			{
				if (!night)
					continue;
				chance = chance * s_fCoverNightFactor;
			}
			if (Math.RandomFloat(0, 100) >= chance)
				continue;
			if (!HasLineOfSight(flight, player))
				continue;

			flight.m_Target = player;
			flight.m_bTargetVisible = true;
			flight.m_bTargetCovered = false;
			flight.m_iLostTick = 0;
			flight.m_vLastSeen = player.GetOrigin();
			enemies.ReportSpotted(player.GetOrigin());
			string how = "dans le faisceau, projecteur fixé sur lui";
			if (!night)
				how = "à vue (de jour, sans projecteur), il le suit";
			if (covered)
				how = how + ", malgré les arbres";
			SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche : soldat repéré " + how + " (" + PlayerName(player) + ")");
			return;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque seconde, sur zone : plus aucun joueur vivant à moins de 1 500 m de l'appareil depuis une minute, il repart
	protected static void WatchPlayers(SRP_HeliFlight flight, int now)
	{
		vector heliPosition = flight.m_Heli.GetOrigin();
		bool anyone = false;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		foreach (IEntity player : players)
		{
			if (player && !SRP_Utils.IsDead(player) && vector.Distance(player.GetOrigin(), heliPosition) <= s_fAbandonDistance)
			{
				anyone = true;
				break;
			}
		}
		if (anyone)
		{
			flight.m_iNoPlayerSince = 0;
			return;
		}
		if (flight.m_iNoPlayerSince == 0)
			flight.m_iNoPlayerSince = now;
		else if (now - flight.m_iNoPlayerSince >= s_iAbandonSeconds * 1000)
			Depart(flight, string.Format("plus aucun joueur à moins de %1 m depuis %2 s", s_fAbandonDistance, s_iAbandonSeconds));
	}

	//------------------------------------------------------------------------------------------------
	//! But de fouille, RELATIF À L'APPAREIL : vers le centre du cercle, décalé d'un balayage de ±30° (12 s), à une
	//! distance qui fait descendre le faisceau entre 35 et 55° sous l'horizon (17 s) ; posé au sol. Temps propre au vol.
	protected static vector SearchGoal(SRP_HeliFlight flight, BaseWorld world)
	{
		vector heliPosition = flight.m_Heli.GetOrigin();
		vector toCenter = flight.m_vCenter - heliPosition;
		toCenter[1] = 0;
		float length = toCenter.Length();
		if (length < 1)
		{
			// À la verticale du centre : droit devant l'appareil
			vector heliTransform[4];
			flight.m_Heli.GetTransform(heliTransform);
			toCenter = heliTransform[2];
			toCenter[1] = 0;
			length = toCenter.Length();
			if (length < 0.01)
			{
				toCenter = Vector(1, 0, 0);
				length = 1;
			}
		}
		toCenter = toCenter * (1 / length);

		float t = flight.m_fSearchTime;
		float sweep = Math.Sin(t * Math.PI2 / Math.Max(s_fSweepPeriod, 1)) * s_fSweepAngle * Math.DEG2RAD;
		float cosine = Math.Cos(sweep);
		float sine = Math.Sin(sweep);
		vector direction = Vector(toCenter[0] * cosine - toCenter[2] * sine, 0, toCenter[0] * sine + toCenter[2] * cosine);

		float middle = (s_fDipMin + s_fDipMax) * 0.5;
		float half = (s_fDipMax - s_fDipMin) * 0.5;
		float dip = middle + half * Math.Sin(t * Math.PI2 / Math.Max(s_fDipPeriod, 1));
		float height = Math.Max(heliPosition[1] - world.GetSurfaceY(heliPosition[0], heliPosition[2]), 20);
		float reach = height / Math.Tan(Math.Clamp(dip, 5, 85) * Math.DEG2RAD);

		vector goal = heliPosition + direction * reach;
		goal[1] = world.GetSurfaceY(goal[0], goal[2]);
		return goal;
	}

	//------------------------------------------------------------------------------------------------
	//! Le but du point éclairé : le soldat suivi s'il est vu ; perdu de vue, son dernier point 3 s puis une petite boucle
	//! autour pendant 20 s ; sinon la fouille
	protected static vector AimGoal(SRP_HeliFlight flight, int now, BaseWorld world)
	{
		if (flight.m_Target && flight.m_bTargetVisible && !flight.m_Target.IsDeleted())
		{
			if (flight.m_bTargetCovered)
				return flight.m_vLastSeen;	// sous les arbres : le point éclairé ne le suit plus
			return flight.m_Target.GetOrigin();
		}

		if (flight.m_iLostTick != 0)
		{
			int lost = now - flight.m_iLostTick;
			if (lost < s_iHoldSeconds * 1000)
				return flight.m_vLastSeen;
			if (lost < (s_iHoldSeconds + s_iLocalSearchSeconds) * 1000)
			{
				// Boucle parcourue un peu moins vite que le point éclairé, pour qu'il la suive
				float radius = Math.Max(s_fLocalSearchRadius, 1);
				float turn = flight.m_fSearchTime * 0.8 * s_fAimSearchSpeed / radius;
				vector loop = flight.m_vLastSeen + Vector(Math.Cos(turn) * radius, 0, Math.Sin(turn) * radius);
				loop[1] = world.GetSurfaceY(loop[0], loop[2]);
				return loop;
			}
			if (!flight.m_Target)
				flight.m_iLostTick = 0;		// fouille autour finie : reprise de la fouille normale
		}
		return SearchGoal(flight, world);
	}

	//------------------------------------------------------------------------------------------------
	//! À chaque image, sur zone : le point éclairé glisse vers son but à vitesse bornée (jamais de saut)
	protected static void MoveAim(SRP_HeliFlight flight, float dt, int now, BaseWorld world)
	{
		flight.m_fSearchTime = flight.m_fSearchTime + dt;
		vector goal = AimGoal(flight, now, world);
		flight.m_vAimGoal = goal;

		float maxSpeed = s_fAimSearchSpeed;
		if (flight.m_Target)
			maxSpeed = s_fAimChaseSpeed;
		vector aim = flight.m_vAim;
		vector toGoal = goal - aim;
		toGoal[1] = 0;
		float distance = toGoal.Length();
		float stepLength = maxSpeed * dt;
		vector moved = toGoal;
		if (distance > stepLength)
			moved = toGoal * (stepLength / distance);
		aim = aim + moved;
		aim[1] = world.GetSurfaceY(aim[0], aim[2]);
		flight.m_vAim = aim;
		flight.m_vAimVelocity = moved * (1 / dt);
	}

	//------------------------------------------------------------------------------------------------
	//! Sur zone : fouiller, se recentrer sur la dernière position connue des joueurs s'il y a une alerte, éclairer la nuit
	protected static void Search(SRP_HeliFlight flight, float dt, int now, BaseWorld world)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;

		if (now >= flight.m_iNextScanTick)
		{
			flight.m_iNextScanTick = now + 1000;
			Scan(flight, enemies, now);
			WatchPlayers(flight, now);
			if (flight.m_iPhase != 2)
				return;		// il repart
		}

		vector alert;
		if (enemies.MostRecentAlert(flight.m_vCenter, 800, alert))
		{
			// Le centre glisse vers l'alerte la plus récente (15 m/s au plus) : pas de saut de trajectoire
			vector toAlert = alert - flight.m_vCenter;
			toAlert[1] = 0;
			float distance = toAlert.Length();
			if (distance > 5)
				flight.m_vCenter = flight.m_vCenter + toAlert * (Math.Min(15 * dt, distance) / distance);
			if (!flight.m_bTightened)
			{
				flight.m_bTightened = true;
				flight.m_fOrbitRadius = Math.Max(flight.m_fOrbitRadius * 0.6, 120);
				SRP_EnemyComponent.Journal("HELICO", "Hélicoptère de recherche : joueurs repérés, il resserre ses cercles");
			}
		}

		MoveAim(flight, dt, now, world);

		if (now >= flight.m_iNextFlareTick)
		{
			flight.m_iNextFlareTick = now + s_iFlareSeconds * 1000;
			if (flight.m_bTightened)
				enemies.FireFlareIfNight(flight.m_vCenter);
		}
	}
}

//------------------------------------------------------------------------------------------------
//! Côté joueur : le projecteur de recherche (refonte du 24/09). On SÉPARE ce qu'on voit de ce qui éclaire :
//! - le FAISCEAU, visible, est une lumière enfant de l'appareil, attachée sous son nez et orientée comme une tourelle :
//!   petit halo au nez, trait de volume très léger ;
//! - l'APPOINT, invisible (ni halo, ni volume, pas de source visible), est posé sur l'axe du faisceau, côté hélicoptère,
//!   à 70 m du point éclairé, avec ombres portées : c'est lui qui éclaire vraiment le sol vu du soldat (le 20/09, une lumière sous l'hélicoptère ne se voyait pas depuis
//!   le sol à 250 m). Obligatoire par défaut ; le serveur peut le couper (bascule d'essai du menu Staff, s_bFillLight).
//! - le RUBAN, dessiné par maillage (pas une lumière, donc visible de loin) : il descend du nez à la tache, tourné vers
//!   la caméra, caché quand elle est dans le faisceau. Enfant de l'appareil, il vit et meurt avec le faisceau.
//! Le serveur envoie l'ordre deux fois par seconde (appareil, point visé et sa vitesse, soldat suivi, appoint) ; entre
//! deux ordres, le point visé est extrapolé. La direction tourne à vitesse bornée puis se lisse, sans dépendre des images
//! par seconde. Ordre « éteint », ou plus d'ordre depuis 3 s : les deux lumières sont supprimées et la boucle s'arrête.
//! Appareil inconnu de ce joueur (trop loin) : lumières supprimées, recréées quand il revient.
class SRP_HeliLightClient
{
	static ResourceName s_sBeamPrefab = "{6A5B0C0D0E0FB021}Prefabs/Missions/SRP_ProjecteurHelico.et";
	static ResourceName s_sFillPrefab = "{6A5B0C0D0E0FB022}Prefabs/Missions/SRP_ProjecteurAppoint.et";

	// Réglages, appliqués par script à la création, par-dessus les prefabs (pour les ajuster sans toucher aux prefabs).
	// L'ouverture du faisceau n'est PAS ici : c'est celle du serveur, SRP_HeliSearch.s_fBeamCone (10°), pour que la tache
	// éclairée et la zone où l'hélicoptère repère un soldat restent la même (l'appoint couvre aussi sa marge, s_fBeamMargin).
	static vector s_vMount = "0 0.4 3.5";		// point d'attache du faisceau dans le repère de l'appareil (sous le nez ; à caler à l'œil)
	static vector s_vColor = "1 0.96 0.88";		// couleur (rouge, vert, bleu) du faisceau et de l'appoint : blanc chaud
	// Le faisceau
	static float s_fBeamLVOffset = 6;			// puissance : LV = … + log2(distance lampe → sol) (4 estimé, + 2 : l'appoint porte l'essentiel)…
	static float s_fBeamLVMin = 9;				// … borné entre …
	static float s_fBeamLVMax = 15;				// … et …
	static float s_fBeamRange = 500;			// portée, en mètres
	static float s_fBeamEdge = 0.3;				// bord du cône (0 = très doux, 1 = net)
	static float s_fBeamAttenuation = 2;		// affaiblissement avec la distance (2 = celui d'une lampe sans réflecteur)
	static float s_fBeamClipEV = -14;			// en dessous de cette intensité, le moteur n'éclaire plus
	static float s_fBeamVolume = 0.006;			// trait de volume : intensité…
	static float s_fBeamVolumeOffset = 0.1;		// … décalage…
	static float s_fBeamVolumeScale = 0.4;		// … échelle
	static float s_fBeamFlare = 0.15;			// halo au nez : taille (0 = pas de halo)
	// Le faisceau DESSINÉ (maillage créé par script : visible de loin, contrairement à la lampe du nez)
	static bool s_bMeshBeam = true;				// faisceau dessiné (visible de loin)
	static ResourceName s_sRibbonMaterial = "{6A5B0C0D0E0FB031}Assets/Helico/SRP_Faisceau.emat";
	static float s_fRibbonWidth = 1.1;			// largeur du ruban / largeur du cône (la texture s'efface vers les bords)
	static float s_fRibbonFadeEnd = 0.8;		// V lue au sol (0,5 = comme au nez ; texture sRGB : ne pas dépasser ~0,85)
	static float s_fRibbonApex = 0.0015;		// demi-largeur au nez, en fraction de la longueur
	static float s_fRibbonOverrun = 1.03;		// dépasse un peu le point éclairé (le relief coupe le reste)
	static bool s_bRibbonInBeam = false;		// false : ruban CACHÉ quand la caméra est dans le faisceau (Jack, 24/09 : le voile gris retiré) ; true : gardé mais arrêté…
	static float s_fRibbonGap = 25;				// … à … mètres au-dessus d'elle, le long du faisceau (essayé : faisait un voile gris)
	static float s_fRibbonMinLength = 10;		// ruban plus court que … m : caché
	static float s_fRibbonHideMargin = 2;		// caméra à moins de … m du bord du faisceau : « dans le faisceau »…
	static float s_fRibbonShowMargin = 4;		// … et « dehors » au-delà de … m (passage progressif entre les deux, pas de saut)
	// L'appoint au sol
	static float s_fFillDistance = 70;			// posé sur l'axe du faisceau, à … mètres du point éclairé, côté hélicoptère (jamais plus loin que l'appareil)
	static float s_fFillLV = 14;				// puissance
	static float s_fFillRange = 250;			// portée, en mètres
	static float s_fFillEdge = 0.2;				// bord du cône (son ouverture est calculée pour couvrir la tache du faisceau)
	static float s_fFillAttenuation = 2;		// affaiblissement avec la distance
	static float s_fFillClipEV = -14;			// en dessous de cette intensité, le moteur n'éclaire plus
	static float s_fFillSpread = 1.3;			// la tache éclairée déborde de … fois la zone de repérage, bord adouci (« portée » demandée par Jack le 24/09)
	static bool s_bFillShadows = true;			// ombres portées de l'appoint (arbres, murs, soldats), demandées par Jack le 24/09
	static float s_fFillNearPlane = 5;			// plan proche des ombres, en mètres (la source est loin de tout obstacle)
	// L'orientation
	static float s_fTurnSearch = 20;			// rotation au plus, en degrés par seconde : en fouille…
	static float s_fTurnChase = 45;				// … et sur un soldat
	static float s_fSmoothTime = 0.25;			// lissage, en secondes
	static float s_fMaxExtrapolation = 0.6;		// le point visé est avancé de sa vitesse, … secondes au plus après l'ordre
	static float s_fTargetHeight = 0.8;			// le faisceau vise le soldat suivi à … mètres au-dessus de ses pieds
	static float s_fTurretLimit = 5;			// butée : jamais plus haut que … degrés sous l'horizontale de l'appareil
	static float s_fGroundStep = 5;				// point éclairé : le rayon est suivi par pas de … mètres…
	static float s_fGroundRange = 600;			// … jusqu'à … mètres, puis affiné
	static float s_fOrderTimeout = 3;			// sans ordre depuis … secondes, le projecteur s'éteint tout seul

	protected static RplId s_HeliId;
	protected static RplId s_TargetId;			// le soldat suivi, s'il est vu (invalide = fouille)
	protected static vector s_vAim;				// point visé au sol, à l'arrivée de l'ordre
	protected static vector s_vAimVelocity;		// sa vitesse, en m/s
	protected static bool s_bFill;				// le serveur demande l'appoint au sol
	protected static bool s_bOn;
	protected static bool s_bRunning;
	protected static float s_fLastOrder;		// heure du monde à l'arrivée du dernier ordre, en ms
	protected static float s_fLastFrame;		// heure du monde de l'image précédente, en ms
	protected static vector s_vDirection;		// direction courante du faisceau, dans le monde (unitaire)
	protected static vector s_vLastWanted;		// direction voulue à l'image précédente (anticipation)
	protected static bool s_bHasDirection;
	protected static LightEntity s_Beam;		// le faisceau, enfant de l'appareil
	protected static LightEntity s_Fill;		// l'appoint au sol, libre
	protected static ref Color s_Color;
	protected static bool s_bBeamBroken;		// prefab du faisceau inutilisable : signalé une fois, plus d'essai
	protected static bool s_bFillBroken;		// … de même pour l'appoint
	protected static IEntity s_Ribbon;			// le ruban dessiné, enfant de l'appareil
	protected static bool s_bMeshBroken;		// maillage, matériau ou rattachement impossible : signalé une fois, plus d'essai
	protected static bool s_bRibbonHidden;		// ruban caché (caméra dans le faisceau), avec hystérésis

	//------------------------------------------------------------------------------------------------
	//! Ordre du serveur : appareil, point visé au sol et sa vitesse (m/s), allumé ou non, soldat suivi (RplId invalide
	//! sinon), appoint au sol
	static void Order(RplId heliId, vector aim, vector aimVelocity, bool on, RplId targetId, bool fill)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return;
		bool sameHeli = heliId == s_HeliId;
		if (!on)
		{
			// « Éteint » d'un autre appareil (l'ancien, répété après son retrait) : ne touche pas celui qu'on éclaire
			if (s_bRunning && !sameHeli)
				return;
			s_bOn = false;
			if (s_bRunning)
				Stop();
			return;
		}

		if (!sameHeli)
			s_bHasDirection = false;	// autre appareil : les lumières seront recréées sur lui (EnsureBeam)
		s_HeliId = heliId;
		s_vAim = aim;
		s_vAimVelocity = aimVelocity;
		s_TargetId = targetId;
		s_bFill = fill;	// appoint : obligatoire par défaut, coupé seulement par la bascule d'essai du menu Staff
		s_bOn = true;
		float time = world.GetWorldTime();
		s_fLastOrder = time;
		if (s_bBeamBroken)
			return;

		// Boucle arrêtée sans passer par Stop (monde rechargé) : on la relance
		if (s_bRunning && Math.AbsFloat(time - s_fLastFrame) > 1000)
		{
			GetGame().GetCallqueue().Remove(Update);
			s_bRunning = false;
		}
		if (!s_bRunning)
		{
			s_bRunning = true;
			s_bHasDirection = false;
			s_fLastFrame = time;
			GetGame().GetCallqueue().CallLater(Update, 0, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Supprime les deux lumières et le ruban dessiné (la boucle continue)
	protected static void DeleteLights()
	{
		if (s_Beam)
			delete s_Beam;
		s_Beam = null;
		if (s_Fill)
			delete s_Fill;
		s_Fill = null;
		DeleteMeshBeam();
		s_bHasDirection = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Éteint : supprime les deux lumières et arrête la boucle
	protected static void Stop()
	{
		DeleteLights();
		s_bRunning = false;
		GetGame().GetCallqueue().Remove(Update);
	}

	//------------------------------------------------------------------------------------------------
	//! À chaque image, tant que le projecteur est allumé
	static void Update()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			Stop();
			return;
		}
		float time = world.GetWorldTime();
		// Monde rechargé (reconnexion, retour au menu) : l'heure du monde repart de zéro, l'ordre gardé ne vaut plus rien
		if (!s_bOn || time < s_fLastOrder || time - s_fLastOrder > s_fOrderTimeout * 1000)
		{
			Stop();
			return;
		}
		// Temps écoulé depuis l'image précédente, en secondes (borné) : tout ce qui bouge en dépend, pas les images par seconde
		float dt = Math.Clamp((time - s_fLastFrame) / 1000, 0, 0.1);
		s_fLastFrame = time;

		IEntity heli = FindEntity(s_HeliId);
		if (!heli)
		{
			// L'appareil n'est pas (ou plus) connu de ce joueur : trop loin. Les lumières reviendront avec lui.
			DeleteLights();
			return;
		}
		if (!EnsureBeam(heli, world))
		{
			Stop();
			return;
		}
		// Hors d'EnsureBeam : son retour anticipé (faisceau déjà attaché) empêcherait de recréer le ruban
		EnsureMeshBeam(heli, world);
		vector lamp = heli.CoordToParent(s_vMount);
		UpdateFill(world, lamp);

		// Où le faisceau doit-il regarder ? (butée de la tourelle comprise)
		bool chasing;
		vector wanted = WantedPoint(world, time, chasing);
		vector toWanted = wanted - lamp;
		float wantedLength = toWanted.Length();
		if (wantedLength < 1)
			return;
		vector wantedDirection = ClampTurret(heli, toWanted * (1 / wantedLength));

		// La direction courante tourne vers la voulue : vitesse bornée, puis lissage
		if (!s_bHasDirection)
		{
			s_vDirection = wantedDirection;
			s_vLastWanted = wantedDirection;
			s_bHasDirection = true;
		}
		float turnRate = s_fTurnSearch;
		if (chasing)
			turnRate = s_fTurnChase;
		float maxTurn = turnRate * Math.DEG2RAD * dt;
		// Anticipation : la direction courante suit d'abord le mouvement propre de la direction voulue (l'appareil qui
		// tourne, le point qui glisse), dans la limite de la vitesse de rotation ; le lissage ne rattrape que l'écart restant
		vector follow = wantedDirection - s_vLastWanted;
		float followLength = follow.Length();
		s_vLastWanted = wantedDirection;
		if (followLength < maxTurn)
		{
			vector followed = s_vDirection + follow;
			followed.Normalize();
			s_vDirection = followed;
			maxTurn = maxTurn - followLength;
		}
		float smoothing = 1 - Math.Pow(Math.E, -dt / Math.Max(s_fSmoothTime, 0.01));
		vector turned = RotateToward(s_vDirection, wantedDirection, smoothing, maxTurn);
		s_vDirection = ClampTurret(heli, turned);

		// Le point éclairé : là où la direction courante touche le sol
		vector lit;
		bool grounded = GroundHit(world, lamp, s_vDirection, lit);
		float distance = Math.Max(vector.Distance(lamp, lit), 1);

		PlaceBeam(heli, distance);
		PlaceMeshBeam(heli, world, distance, grounded);
		PlaceFill(lit, distance);
	}

	//------------------------------------------------------------------------------------------------
	protected static IEntity FindEntity(RplId id)
	{
		if (!id.IsValid())
			return null;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl)
			return null;
		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Crée une lumière locale (chez ce joueur seulement) depuis un prefab. Retourne null si le prefab ne convient pas.
	protected static LightEntity SpawnLight(ResourceName prefab, BaseWorld world, vector position)
	{
		Resource res = Resource.Load(prefab);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Projecteur : prefab introuvable " + prefab, LogLevel.WARNING);
			return null;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = position;
		IEntity spawned = GetGame().SpawnEntityPrefabLocal(res, world, params);
		LightEntity light = LightEntity.Cast(spawned);
		if (!light)
		{
			Print("[SRP] Projecteur : ce prefab n'est pas une lumière " + prefab, LogLevel.WARNING);
			if (spawned)
				delete spawned;
			return null;
		}
		light.SetFlags(EntityFlags.ACTIVE, false);
		return light;
	}

	//------------------------------------------------------------------------------------------------
	//! Le faisceau existe et est attaché à CET appareil ; sinon il est (re)créé, attaché sous son nez et réglé.
	//! Retourne false si c'est impossible.
	protected static bool EnsureBeam(IEntity heli, BaseWorld world)
	{
		if (s_Beam && s_Beam.GetParent() == heli)
			return true;
		DeleteLights();		// attaché à un autre appareil, ou détaché : on repart de zéro
		if (s_bBeamBroken)
			return false;
		s_Beam = SpawnLight(s_sBeamPrefab, world, heli.CoordToParent(s_vMount));
		if (!s_Beam)
		{
			s_bBeamBroken = true;
			return false;
		}
		heli.AddChild(s_Beam, -1, EAddChildFlags.AUTO_TRANSFORM);
		if (s_Beam.GetParent() != heli)
		{
			// Rattachement refusé : signalé une fois, plus d'essai (sinon il serait recréé à chaque image)
			Print("[SRP] Projecteur : le faisceau n'a pas pu être attaché à l'hélicoptère", LogLevel.WARNING);
			delete s_Beam;
			s_Beam = null;
			s_bBeamBroken = true;
			return false;
		}

		s_Color = new Color(s_vColor[0], s_vColor[1], s_vColor[2], 1);
		s_Beam.SetColor(s_Color, s_fBeamLVMin);
		s_Beam.SetRadius(s_fBeamRange);
		s_Beam.SetConeAngle(SRP_HeliSearch.s_fBeamCone);
		s_Beam.SetConeAngleAttenuation(s_fBeamEdge);
		s_Beam.SetDistanceAtt(s_fBeamAttenuation);
		s_Beam.SetIntensityEVClip(s_fBeamClipEV);
		s_Beam.SetVolumeEffect(s_fBeamVolume, s_fBeamVolumeOffset, s_fBeamVolumeScale);
		if (s_fBeamFlare > 0)
		{
			s_Beam.SetLensFlareType(LightLensFlareType.Automatic);
			s_Beam.SetLensFlareScale(s_fBeamFlare);
		}
		else
			s_Beam.SetLensFlareType(LightLensFlareType.Disabled);
		s_bHasDirection = false;

		// Pour les essais : à quelle distance de la caméra l'appareil se trouve-t-il au moment de la création ?
		vector camera[4];
		world.GetCurrentCamera(camera);
		int cameraDistance = Math.Round(vector.Distance(camera[3], heli.GetOrigin()));
		string fillText = "non";
		if (s_bFill)
			fillText = "oui";
		Print(string.Format("[SRP] Projecteur : faisceau créé sous l'hélicoptère, caméra à %1 m de l'appareil, appoint au sol : %2", cameraDistance, fillText));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! L'appoint au sol n'existe que si l'ordre le demande : créé (sans halo, sans volume) ou supprimé ici
	protected static void UpdateFill(BaseWorld world, vector position)
	{
		if (!s_bFill)
		{
			if (s_Fill)
				delete s_Fill;
			s_Fill = null;
			return;
		}
		if (s_Fill || s_bFillBroken)
			return;
		s_Fill = SpawnLight(s_sFillPrefab, world, position);
		if (!s_Fill)
		{
			s_bFillBroken = true;
			return;
		}
		s_Fill.SetColor(s_Color, s_fFillLV);
		s_Fill.SetRadius(s_fFillRange);
		s_Fill.SetConeAngleAttenuation(s_fFillEdge);
		s_Fill.SetDistanceAtt(s_fFillAttenuation);
		s_Fill.SetIntensityEVClip(s_fFillClipEV);
		s_Fill.SetVolumeEffect(0, 0, 0);
		s_Fill.SetLensFlareType(LightLensFlareType.Disabled);
		s_Fill.SetCastShadow(s_bFillShadows);
		s_Fill.SetNearPlane(s_fFillNearPlane);
	}

	//------------------------------------------------------------------------------------------------
	//! Ruban unitaire : longueur 1 suivant +Z (du nez vers le sol), dans le plan X-Z, 13 rangées de 2 sommets. Mis à
	//! l'échelle UNIFORME par la distance lampe → sol, l'angle du cône est gardé. Texture en dégradé radial : lue au centre
	//! (V 0,5) au nez et vers son bord (s_fRibbonFadeEnd) au sol, de bord à bord en travers (U 0 → 1).
	//! Retourne null si le maillage ne peut pas être créé.
	protected static Resource CreateRibbonMesh()
	{
		vector verts[26];
		float uvs[52];
		int indices[72];
		float half = Math.Tan(SRP_HeliSearch.s_fBeamCone * 0.5 * Math.DEG2RAD) * s_fRibbonWidth;
		for (int i = 0; i <= 12; i++)
		{
			float rowIndex = i;
			float t = rowIndex / 12;
			float z = t * t;				// rangées serrées près du nez : la texture se déforme moins sur un trapèze
			float w = s_fRibbonApex + half * z;
			float v = 0.5 + (s_fRibbonFadeEnd - 0.5) * z;
			verts[i * 2] = Vector(-w, 0, z);
			verts[i * 2 + 1] = Vector(w, 0, z);
			uvs[i * 4] = 0;
			uvs[i * 4 + 1] = v;
			uvs[i * 4 + 2] = 1;
			uvs[i * 4 + 3] = v;
		}
		for (int r = 0; r < 12; r++)
		{
			int a = r * 2;
			indices[r * 6] = a;
			indices[r * 6 + 1] = a + 2;
			indices[r * 6 + 2] = a + 1;
			indices[r * 6 + 3] = a + 1;
			indices[r * 6 + 4] = a + 2;
			indices[r * 6 + 5] = a + 3;
		}
		int numVertices[] = {26};
		int numIndices[] = {72};
		string materials[1] = {s_sRibbonMaterial};
		Resource res = MeshObject.Create(1, numVertices, numIndices, materials, 0);
		if (!res || !res.IsValid())
			return null;
		MeshObject mesh = res.GetResource().ToMeshObject();
		if (!mesh)
			return null;
		mesh.UpdateVerts(0, verts, uvs);
		mesh.UpdateIndices(0, indices);
		return res;
	}

	//------------------------------------------------------------------------------------------------
	//! Entité locale (chez ce joueur seulement) qui porte le maillage de « res », rattachée à l'appareil. Retourne null si
	//! c'est impossible.
	protected static IEntity SpawnMeshChild(IEntity heli, BaseWorld world, Resource res)
	{
		if (!res || !res.IsValid())
			return null;
		MeshObject mesh = res.GetResource().ToMeshObject();
		if (!mesh)
			return null;
		IEntity entity = GetGame().SpawnEntity(GenericEntity, world);	// comme l'aperçu de pose du jeu (SCR_ItemPlacementComponent)
		if (!entity)
			return null;
		entity.SetObject(mesh, "");						// l'entité garde sa propre référence au maillage
		entity.ClearFlags(EntityFlags.TRACEABLE, false);	// jamais touché par un tir ou un rayon
		entity.ClearFlags(EntityFlags.VISIBLE, false);	// caché jusqu'au premier placement (PlaceMeshBeam)
		heli.AddChild(entity, -1, EAddChildFlags.AUTO_TRANSFORM);
		if (entity.GetParent() != heli)
		{
			delete entity;
			return null;
		}
		entity.ClearFlags(EntityFlags.PROXY, false);	// ne pas être coupé avec la boîte de l'appareil quand il sort de l'écran
		return entity;
	}

	//------------------------------------------------------------------------------------------------
	//! Le ruban dessiné existe et est attaché à CET appareil ; sinon il est (re)créé. Un échec est signalé une fois et
	//! on n'essaie plus (sinon il serait créé et détruit à chaque image) ; les lumières, elles, continuent.
	protected static void EnsureMeshBeam(IEntity heli, BaseWorld world)
	{
		if (!s_bMeshBeam || s_bMeshBroken || System.IsConsoleApp())
			return;
		if (s_Ribbon && s_Ribbon.GetParent() == heli)
			return;
		DeleteMeshBeam();
		// Matériaux d'abord : MeshObject.Create ne reçoit qu'un nom (un .emat refusé donnerait un ruban au matériau par défaut)
		Resource ribbonMaterial = Resource.Load(s_sRibbonMaterial);
		if (!ribbonMaterial || !ribbonMaterial.IsValid())
		{
			Print("[SRP] Projecteur : faisceau dessiné impossible, cause : matériau Assets/Helico introuvable", LogLevel.WARNING);
			s_bMeshBroken = true;
			return;
		}
		// Resource en variable locale, comme dans le jeu (SCR_GenericBoxEntity) : SetObject garde sa propre référence
		Resource ribbonRes = CreateRibbonMesh();
		s_Ribbon = SpawnMeshChild(heli, world, ribbonRes);
		if (!s_Ribbon)
		{
			// Pour les essais : la cause oriente le repli (cône fait dans Blender, ou entité libre non rattachée)
			string cause = "entité ou rattachement à l'appareil";
			if (!ribbonRes)
				cause = "maillage (MeshObject.Create)";
			Print("[SRP] Projecteur : faisceau dessiné impossible, cause : " + cause, LogLevel.WARNING);
			DeleteMeshBeam();
			s_bMeshBroken = true;
			return;
		}
		s_bRibbonHidden = false;
		Print("[SRP] Projecteur : faisceau dessiné créé");
	}

	//------------------------------------------------------------------------------------------------
	//! Supprime le ruban dessiné
	protected static void DeleteMeshBeam()
	{
		if (s_Ribbon)
			delete s_Ribbon;
		s_Ribbon = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Le point que le faisceau doit regarder : le soldat suivi s'il est connu chez ce joueur (un peu au-dessus de ses
	//! pieds) ; sinon le point visé du dernier ordre, avancé de sa vitesse depuis cet ordre (0,6 s au plus), posé au sol.
	//! « chasing » : le serveur suit un soldat (rotation plus rapide).
	protected static vector WantedPoint(BaseWorld world, float time, out bool chasing)
	{
		chasing = s_TargetId.IsValid();
		if (chasing)
		{
			IEntity target = FindEntity(s_TargetId);
			if (target)
				return target.GetOrigin() + vector.Up * s_fTargetHeight;
		}
		float ahead = Math.Clamp((time - s_fLastOrder) / 1000, 0, s_fMaxExtrapolation);
		vector aim = s_vAim + s_vAimVelocity * ahead;
		aim[1] = GroundY(world, aim);
		return aim;
	}

	//------------------------------------------------------------------------------------------------
	//! Tourne la direction « current » vers « goal » (unitaires) du plus petit de deux angles : « smoothing » × l'écart
	//! (lissage) et « maxAngle » radians (vitesse bornée)
	protected static vector RotateToward(vector current, vector goal, float smoothing, float maxAngle)
	{
		float cosine = Math.Clamp(vector.Dot(current, goal), -1, 1);
		float gap = Math.Acos(cosine);
		if (gap < 0.0001)
			return goal;
		float turn = Math.Min(gap * smoothing, maxAngle);
		vector side = goal - current * cosine;		// la part de « goal » perpendiculaire à « current »
		float sideLength = side.Length();
		if (sideLength < 0.0001)
			return goal;
		side = side * (1 / sideLength);
		vector result = current * Math.Cos(turn) + side * Math.Sin(turn);
		result.Normalize();
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Butée de la tourelle : la direction (dans le monde) ne remonte jamais au-dessus de 5° sous l'horizontale de l'appareil
	protected static vector ClampTurret(IEntity heli, vector direction)
	{
		vector localDirection = heli.VectorToLocal(direction);
		float ceiling = -Math.Sin(s_fTurretLimit * Math.DEG2RAD);
		if (localDirection[1] <= ceiling)
			return direction;
		vector flat = Vector(localDirection[0], 0, localDirection[2]);
		float flatLength = flat.Length();
		if (flatLength < 0.001)
			flat = vector.Forward;		// droit devant
		else
			flat = flat * (1 / flatLength);
		vector clamped = flat * Math.Cos(s_fTurretLimit * Math.DEG2RAD);
		clamped[1] = ceiling;
		return heli.VectorToParent(clamped);
	}

	//------------------------------------------------------------------------------------------------
	//! Hauteur du sol sous ce point (la mer compte pour 0, comme au départ de l'hélicoptère)
	protected static float GroundY(BaseWorld world, vector point)
	{
		return Math.Max(world.GetSurfaceY(point[0], point[2]), 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Là où le rayon (depuis « origin », direction « direction » unitaire) touche le sol : suivi par pas de 5 m jusqu'à
	//! 600 m, puis affiné par dichotomie. Ne touche rien : « hit » reçoit le bout du rayon et on retourne false.
	protected static bool GroundHit(BaseWorld world, vector origin, vector direction, out vector hit)
	{
		float step = Math.Max(s_fGroundStep, 1);
		float above = 0;
		float travelled = step;
		while (travelled <= s_fGroundRange)
		{
			vector point = origin + direction * travelled;
			if (point[1] <= GroundY(world, point))
			{
				// Entre « above » (au-dessus du sol) et « travelled » (dessous) : on resserre
				float below = travelled;
				for (int i = 0; i < 6; i++)
				{
					float middle = (above + below) * 0.5;
					vector probe = origin + direction * middle;
					if (probe[1] <= GroundY(world, probe))
						below = middle;
					else
						above = middle;
				}
				hit = origin + direction * below;
				return true;
			}
			above = travelled;
			travelled = travelled + step;
		}
		hit = origin + direction * s_fGroundRange;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Le faisceau : orienté dans le repère de l'appareil (il le suit), puissance selon la distance de la lampe au sol
	protected static void PlaceBeam(IEntity heli, float distance)
	{
		vector localDirection = heli.VectorToLocal(s_vDirection);
		vector up = vector.Up;
		if (Math.AbsFloat(localDirection[1]) > 0.97)
			up = vector.Forward;
		vector mat[4];
		Math3D.DirectionAndUpMatrix(localDirection, up, mat);
		mat[3] = s_vMount;
		s_Beam.SetLocalTransform(mat);
		s_Beam.Update();
		s_Beam.SetColor(s_Color, Math.Clamp(s_fBeamLVOffset + Math.Log2(distance), s_fBeamLVMin, s_fBeamLVMax));
	}

	//------------------------------------------------------------------------------------------------
	//! Le ruban dessiné, replacé à chaque image après le faisceau (dans le repère de l'appareil) : il part du point
	//! d'attache et suit l'axe du faisceau jusqu'un peu après le sol ; il tourne autour de cet axe pour faire face à la
	//! caméra. Caché quand la caméra est dans le faisceau (avec hystérésis) ou quand le rayon ne touche pas le sol.
	protected static void PlaceMeshBeam(IEntity heli, BaseWorld world, float distance, bool grounded)
	{
		if (!s_Ribbon)
			return;
		vector camera[4];
		world.GetCurrentCamera(camera);
		vector lamp = heli.CoordToParent(s_vMount);
		vector localDirection = heli.VectorToLocal(s_vDirection);
		// Le point de l'axe le plus proche de la caméra, et l'écart de la caméra à cet axe
		float along = Math.Clamp(vector.Dot(camera[3] - lamp, s_vDirection), 0, distance);
		vector toCamera = camera[3] - (lamp + s_vDirection * along);
		float away = toCamera.Length();
		float edge = along * Math.Tan(SRP_HeliSearch.s_fBeamCone * 0.5 * Math.DEG2RAD);
		float ribbonLength = distance * s_fRibbonOverrun;
		if (s_bRibbonInBeam)
		{
			// Caméra dans le faisceau ou tout près : le ruban s'arrête s_fRibbonGap mètres avant elle, le long de l'axe
			// (on voit la colonne descendre vers soi, sans voile à hauteur des yeux) ; passage progressif entre les marges
			float inside = Math.Clamp((edge + s_fRibbonShowMargin - away) / Math.Max(s_fRibbonShowMargin - s_fRibbonHideMargin, 0.1), 0, 1);
			float cut = Math.Min(Math.Max(along - s_fRibbonGap, 0), ribbonLength);
			ribbonLength = ribbonLength + (cut - ribbonLength) * inside;
			s_bRibbonHidden = ribbonLength < s_fRibbonMinLength;
		}
		else if (away < edge + s_fRibbonHideMargin)
			s_bRibbonHidden = true;
		else if (away > edge + s_fRibbonShowMargin)
			s_bRibbonHidden = false;
		// Vers la caméra, perpendiculairement à l'axe (nul si la caméra est sur l'axe : ruban caché)
		vector up = heli.VectorToLocal(toCamera);
		up = up - localDirection * vector.Dot(up, localDirection);
		float upLength = up.Length();
		if (s_bRibbonHidden || !grounded || upLength < 0.01)
		{
			// Dans le faisceau (vu par la tranche ou en plein écran), ou rayon perdu dans le vide (rien touché à 600 m)
			s_Ribbon.ClearFlags(EntityFlags.VISIBLE, false);
		}
		else
		{
			s_Ribbon.SetFlags(EntityFlags.VISIBLE, false);
			up = up * (1 / upLength);
			vector mat[4];
			Math3D.DirectionAndUpMatrix(localDirection, up, mat);	// Z = axe, Y = vers la caméra : le plan X-Z lui fait face
			Math3D.MatrixScale(mat, ribbonLength);	// échelle UNIFORME (l'angle du cône est gardé), pas SetScale (peu fiable d'après le jeu)
			mat[3] = s_vMount;										// après MatrixScale, par sécurité
			s_Ribbon.SetLocalTransform(mat);
			s_Ribbon.Update();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! L'appoint : sur l'axe du faisceau, côté hélicoptère, à 70 m du point éclairé (jamais plus loin que l'appareil),
	//! tourné comme le faisceau ; son cône couvre la tache du faisceau, marge du serveur comprise. Posé sur l'axe (demande
	//! de Jack du 24/09), ses ombres portées partent dans le même sens que celles du vrai projecteur. Quand le faisceau
	//! devient rasant, sa tache s'allonge un peu plus loin derrière le point que celle du faisceau.
	protected static void PlaceFill(vector lit, float distance)
	{
		if (!s_Fill)
			return;
		float fillDistance = Math.Max(Math.Min(s_fFillDistance, distance - 2), 1);
		vector up = vector.Up;
		if (Math.AbsFloat(s_vDirection[1]) > 0.97)
			up = vector.Forward;
		vector mat[4];
		Math3D.DirectionAndUpMatrix(s_vDirection, up, mat);
		mat[3] = lit - s_vDirection * fillDistance;
		s_Fill.SetWorldTransform(mat);
		s_Fill.Update();

		float spot = (distance * Math.Tan(SRP_HeliSearch.s_fBeamCone * 0.5 * Math.DEG2RAD) + SRP_HeliSearch.s_fBeamMargin) * s_fFillSpread;
		float cone = 2 * Math.Atan2(spot, fillDistance) * Math.RAD2DEG;
		s_Fill.SetConeAngle(Math.Clamp(cone, 1, 170));
	}
}
