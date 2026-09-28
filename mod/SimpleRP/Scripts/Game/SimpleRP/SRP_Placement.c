//------------------------------------------------------------------------------------------------
// SimpleRP — Brique commune de pose de l'IA ennemie (carte #58)
// Tout ce qui pose un groupe ennemi (garnison, garde de mission, vague, patrouille ambiante, renfort, jeep) passe
// par ici pour choisir un point :
// - jamais à moins d'une distance minimale de tout joueur, jamais dans le rayon interdit autour de la base ;
// - HORS DE VUE de tous les joueurs (trace des yeux du joueur au point, comme le projecteur de l'hélicoptère) ;
// - de préférence sur une route (RoadNetworkManager, même logique que le trafic civil, sans en dépendre) ou en
//   lisière (derrière un relief : la trace vers le joueur est bloquée par le terrain seul) ;
// - jamais dans l'eau, sur un emplacement libre (FindEmptyTerrainPosition).
// Fournit aussi les points de route : le plus proche, un point au hasard sur les rues d'un lieu, un itinéraire par
// la route entre deux points (routes puis champs à l'approche).
// Carte #69, livraison 3 : les entrées d'une localité (sur ses routes) et ses bâtiments, pour les postes des garnisons ;
// un véhicule vu ou non (le camion de renfort et ses occupants ne cachent pas la vue : IsVehicleSeenByAnyPlayer).
// Statique, sans état : les réglages (base, rayon interdit) sont poussés par SRP_EnemyComponent au démarrage.
// Version 1.0.26
//------------------------------------------------------------------------------------------------

class SRP_Placement
{
	static float s_fBaseSafeRadius = 1500;
	static string s_sBaseMarkerName = "SRP_SpawnBase";
	static const int TRIES = 14;					// essais par pose avant de renoncer
	static const float EYES = 1.7;					// hauteur des yeux (joueur et point visé)
	static const float VISIBLE_RANGE = 1500;		// au-delà, on ne teste plus la ligne de vue (trop loin pour distinguer un groupe qui apparaît)
	static const float ROAD_SNAP = 150;				// distance maximale pour rabattre un point sur une route

	//------------------------------------------------------------------------------------------------
	//! Position de la base (marqueur), false s'il n'y en a pas
	static bool BasePosition(out vector position)
	{
		IEntity marker = GetGame().GetWorld().FindEntityByName(s_sBaseMarkerName);
		if (!marker)
			return false;
		position = marker.GetOrigin();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Dans le rayon interdit autour de la base ?
	static bool IsNearBase(vector position)
	{
		vector basePosition;
		if (!BasePosition(basePosition))
			return false;
		return vector.Distance(position, basePosition) < s_fBaseSafeRadius;
	}

	//------------------------------------------------------------------------------------------------
	//! Dans l'eau ? (le mod considère qu'en dessous de 1 m d'altitude, c'est la mer)
	static bool IsWater(vector position)
	{
		return position[1] < 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Point posé au sol
	static vector OnGround(vector position)
	{
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		return position;
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne de vue entre deux points (yeux à 1,7 m de chaque côté), bloquée par le terrain, les bâtiments, les arbres
	//! (même trace que SRP_HeliSearch.HasLineOfSight). worldOnly = true : seul le terrain compte (test de lisière).
	static bool HasLineOfSight(vector from, vector to, IEntity exclude, bool worldOnly = false)
	{
		TraceParam trace = new TraceParam();
		trace.Start = from + vector.Up * EYES;
		trace.End = to + vector.Up * EYES;
		if (worldOnly)
			trace.Flags = TraceFlags.WORLD;
		else
			trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		if (exclude)
			trace.Exclude = exclude;
		return GetGame().GetWorld().TraceMove(trace, null) >= 0.99;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur (vivant, à portée) voit-il ce point ?
	static bool IsSeenByAnyPlayer(vector position, array<IEntity> players)
	{
		if (!players)
			return false;
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;
			vector eyes = player.GetOrigin();
			if (vector.Distance(eyes, position) > VISIBLE_RANGE)
				continue;
			if (HasLineOfSight(eyes, position, player))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur voit-il ce VÉHICULE ? (carte #69, livraison 3 : un camion n'est jamais retiré sous les yeux d'un joueur)
	//! IsSeenByAnyPlayer vise un point à 1,7 m au-dessus de l'origine, DANS la caisse : la trace heurte le camion lui-même
	//! et le dit « caché » jusqu'à 125-350 m. Ici, le véhicule, ses pièces attachées et ses occupants ne cachent pas la
	//! vue : trace vers trois points (avant, milieu, arrière, à 1,5 m) ; un joueur à bord, ou à moins de
	//! VEHICLE_SEEN_CLOSE mètres, le voit toujours.
	static bool IsVehicleSeenByAnyPlayer(IEntity vehicle, array<IEntity> players)
	{
		if (!vehicle || vehicle.IsDeleted() || !players)
			return false;
		array<IEntity> exclude = {};
		exclude.Insert(vehicle);
		CollectHierarchy(vehicle, exclude, 0);
		BaseCompartmentManagerComponent compartments = BaseCompartmentManagerComponent.Cast(vehicle.FindComponent(BaseCompartmentManagerComponent));
		if (compartments)
		{
			array<BaseCompartmentSlot> slots = {};
			compartments.GetCompartments(slots);
			foreach (BaseCompartmentSlot slot : slots)
			{
				if (!slot)
					continue;
				IEntity occupant = slot.GetOccupant();
				if (!occupant)
					continue;
				if (players.Contains(occupant))
					return true;	// un joueur à bord
				if (!exclude.Contains(occupant))
					exclude.Insert(occupant);
			}
		}

		vector origin = vehicle.GetOrigin();
		vector forward = vehicle.GetTransformAxis(2);
		vector lift = vector.Up * 1.5;
		array<vector> targets = {};
		targets.Insert(origin + lift);
		targets.Insert(origin + forward * 2.5 + lift);
		targets.Insert(origin - forward * 2.5 + lift);

		int playerSlot = exclude.Count();
		exclude.Insert(null);
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;
			vector eyes = player.GetOrigin();
			float distance = vector.Distance(eyes, origin);
			if (distance > VISIBLE_RANGE)
				continue;
			if (distance < VEHICLE_SEEN_CLOSE)
				return true;
			exclude[playerSlot] = player;
			foreach (vector target : targets)
			{
				TraceParam trace = new TraceParam();
				trace.Start = eyes + vector.Up * EYES;
				trace.End = target;
				trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
				trace.LayerMask = EPhysicsLayerDefs.Projectile;
				trace.ExcludeArray = exclude;
				if (GetGame().GetWorld().TraceMove(trace, null) >= 0.99)
					return true;
			}
		}
		return false;
	}

	protected static const float VEHICLE_SEEN_CLOSE = 150;	// un joueur à moins de … m d'un véhicule le voit toujours (ou l'entend)
	protected static const int HIERARCHY_MAX = 64;			// entités attachées au véhicule prises en compte, au plus

	//------------------------------------------------------------------------------------------------
	//! Les entités attachées à « entity » (pièces, occupants attachés), sur trois niveaux, HIERARCHY_MAX au plus
	protected static void CollectHierarchy(IEntity entity, array<IEntity> into, int depth)
	{
		if (!entity || depth > 3 || into.Count() >= HIERARCHY_MAX)
			return;
		IEntity child = entity.GetChildren();
		while (child && into.Count() < HIERARCHY_MAX)
		{
			if (!into.Contains(child))
				into.Insert(child);
			CollectHierarchy(child, into, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le point est-il derrière un relief pour TOUS les joueurs à portée ? (lisière : le terrain seul bloque la vue)
	static bool IsBehindTerrain(vector position, array<IEntity> players)
	{
		if (!players)
			return true;
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;
			vector eyes = player.GetOrigin();
			if (vector.Distance(eyes, position) > VISIBLE_RANGE)
				continue;
			if (HasLineOfSight(eyes, position, player, true))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance au joueur vivant le plus proche (1 000 000 sans joueur)
	static float NearestPlayer(vector position, array<IEntity> players)
	{
		float best = 1000000;
		if (!players)
			return best;
		foreach (IEntity player : players)
		{
			if (!player || player.IsDeleted() || SRP_Utils.IsDead(player))
				continue;
			float distance = vector.Distance(position, player.GetOrigin());
			if (distance < best)
				best = distance;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	// Routes
	//------------------------------------------------------------------------------------------------
	static RoadNetworkManager Roads()
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return null;
		return aiWorld.GetRoadNetworkManager();
	}

	//------------------------------------------------------------------------------------------------
	//! Le point de route le plus proche d'un lieu, à moins de « reach » (logique de SRP_Civilians.FindRoadTransform,
	//! recopiée pour ne pas dépendre de l'ambiance civile)
	static bool FindRoadPoint(vector around, float reach, out vector result)
	{
		RoadNetworkManager roads = Roads();
		if (!roads)
			return false;
		BaseRoad road;
		float distance;
		roads.GetClosestRoad(around, road, distance, true);
		if (!road || distance > reach)
			return false;
		array<vector> points = {};
		road.GetPoints(points);
		if (points.IsEmpty())
			return false;
		int best = 0;
		float bestDistance = 1000000;
		foreach (int index, vector point : points)
		{
			float d = vector.Distance(point, around);
			if (d < bestDistance)
			{
				bestDistance = d;
				best = index;
			}
		}
		result = OnGround(points[best]);
		return !IsWater(result);
	}

	//------------------------------------------------------------------------------------------------
	//! Un point de route atteignable depuis « from », à moins de « range » de « goal » ; sinon le point de route le
	//! plus proche de goal
	static bool FindRoadPointToward(vector from, vector goal, float range, out vector result)
	{
		RoadNetworkManager roads = Roads();
		if (roads && roads.GetReachableWaypointInRoad(from, goal, range, result))
		{
			result = OnGround(result);
			if (!IsWater(result))
				return true;
		}
		return FindRoadPoint(goal, range, result);
	}

	//------------------------------------------------------------------------------------------------
	//! Un point au hasard SUR LES ROUTES d'un lieu (rues d'un village) : tirage dans le disque, rabattu sur la route la
	//! plus proche ; « avoid » : pas à moins de 40 m de ces points déjà choisis
	static bool RandomRoadPointNear(vector center, float radius, array<vector> avoid, out vector result)
	{
		for (int attempt = 0; attempt < 8; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(radius * 0.2, radius);
			vector probe = center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance);
			vector point;
			if (!FindRoadPoint(probe, ROAD_SNAP, point))
				continue;
			if (vector.Distance(point, center) > radius * 1.2)
				continue;
			bool taken = false;
			if (avoid)
			{
				foreach (vector chosen : avoid)
				{
					if (vector.Distance(chosen, point) < 40)
						taken = true;
				}
			}
			if (taken)
				continue;
			result = point;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Une ronde de « count » points sur les rues d'un lieu (au moins 2 sinon false)
	static bool BuildStreetCircuit(vector center, float radius, int count, out array<vector> circuit)
	{
		for (int i = 0; i < count; i++)
		{
			vector point;
			if (RandomRoadPointNear(center, radius, circuit, point))
				circuit.Insert(point);
		}
		return circuit.Count() >= 2;
	}

	//------------------------------------------------------------------------------------------------
	//! Itinéraire PAR LA ROUTE de « from » vers « to » : des points de route tous les « step » mètres le long de la
	//! droite, jusqu'au dernier tronçon (« lastLeg » mètres de la cible), qui se fait à travers champs. La cible
	//! elle-même n'est pas dans la liste. Vide si aucune route n'est trouvée sur le trajet.
	static void BuildRoadRoute(vector from, vector to, float step, float lastLeg, out array<vector> route)
	{
		vector line = to - from;
		line[1] = 0;
		float length = line.Length();
		if (length <= lastLeg + 50 || step < 50)
			return;
		vector direction = line * (1 / length);
		vector previous = from;
		float travelled = step;
		int guard = 0;
		while (travelled < length - lastLeg && guard < 40)
		{
			guard++;
			vector probe = OnGround(from + direction * travelled);
			travelled += step;
			vector point;
			if (!FindRoadPointToward(previous, probe, ROAD_SNAP, point))
				continue;
			if (vector.Distance(point, previous) < 60)
				continue;	// même tronçon
			if (vector.Distance(point, to) < lastLeg)
				break;
			route.Insert(point);
			previous = point;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Le choix d'un point de pose
	//------------------------------------------------------------------------------------------------
	//! Un point pour poser un groupe, entre minDist et maxDist de « target » :
	//! - jamais à moins de playerMinDist (par défaut minDist) de tout joueur, jamais près de la base, jamais dans l'eau ;
	//! - hors de vue de tous les joueurs ;
	//! - preferRoad : rabattu sur la route la plus proche quand il y en a une à moins de 150 m ;
	//! - preferCover : derrière un relief (les premiers essais seulement, puis on se contente d'être hors de vue) ;
	//! - angleMin/angleMax (radians) : pour répartir plusieurs groupes autour du centre, chacun dans sa part du cercle.
	//! Retourne false après TRIES essais : à l'appelant de se rabattre sur l'ancien tirage et de le noter au journal.
	static bool FindSpawnPosition(vector target, float minDist, float maxDist, array<IEntity> players, bool preferRoad, bool preferCover, out vector result, float playerMinDist = -1, float angleMin = -1, float angleMax = -1)
	{
		if (playerMinDist < 0)
			playerMinDist = minDist;
		if (maxDist < minDist)
			maxDist = minDist;
		for (int attempt = 0; attempt < TRIES; attempt++)
		{
			float angle;
			if (angleMax > angleMin)
				angle = Math.RandomFloat(angleMin, angleMax);
			else
				angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(minDist, maxDist);
			vector candidate = OnGround(target + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));

			// Sur la route quand on le demande (et qu'il y en a une tout près)
			if (preferRoad)
			{
				vector onRoad;
				if (FindRoadPoint(candidate, ROAD_SNAP, onRoad))
					candidate = onRoad;
			}

			if (IsWater(candidate) || IsNearBase(candidate))
				continue;
			if (NearestPlayer(candidate, players) < playerMinDist)
				continue;
			if (IsSeenByAnyPlayer(candidate, players))
				continue;
			// La lisière n'est exigée que sur la première moitié des essais
			if (preferCover && attempt < TRIES / 2 && !IsBehindTerrain(candidate, players))
				continue;

			vector spot;
			if (SCR_WorldTools.FindEmptyTerrainPosition(spot, candidate, 20, 2, 2))
				candidate = spot;
			if (IsWater(candidate))
				continue;
			result = candidate;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôles de sûreté seuls (base, eau, distance aux joueurs) sur un point déjà choisi : pour le repli sur
	//! l'ancien tirage, on refuse quand même la base et l'eau
	static bool IsAcceptableFallback(vector position, array<IEntity> players, float playerMinDist)
	{
		if (IsWater(position) || IsNearBase(position))
			return false;
		return NearestPlayer(position, players) >= playerMinDist;
	}

	//------------------------------------------------------------------------------------------------
	// Localités (carte #69, livraison 3) : entrées sur les routes et bâtiments, pour les postes de la garnison
	//------------------------------------------------------------------------------------------------
	//! Les ENTRÉES d'une localité : 16 sondes à 0,9 × le rayon, chacune rabattue sur la route la plus proche (40 m au
	//! plus) ; on garde les points entre 0,7 et 1,2 × le rayon, à plus de 60 m l'un de l'autre, puis les « count » les
	//! plus éloignés entre eux. Repli pour ce qui manque : un point de route au hasard dans la localité, sinon un point
	//! au sol dans l'anneau (0,7 à 0,9 × le rayon). Vrai si « count » points ont été trouvés.
	static bool FindTownEntrances(vector center, float radius, int count, out array<vector> points)
	{
		if (count <= 0 || radius <= 0)
			return false;

		array<vector> candidates = {};
		for (int i = 0; i < 16; i++)
		{
			float angle = Math.PI2 * i / 16;
			vector probe = OnGround(center + Vector(Math.Cos(angle) * radius * 0.9, 0, Math.Sin(angle) * radius * 0.9));
			vector road;
			if (!FindRoadPoint(probe, 40, road))
				continue;
			float fromCenter = vector.DistanceXZ(road, center);
			if (fromCenter < radius * 0.7 || fromCenter > radius * 1.2)
				continue;
			bool duplicate = false;
			foreach (vector known : candidates)
			{
				if (vector.DistanceXZ(known, road) <= 60)
					duplicate = true;
			}
			if (!duplicate)
				candidates.Insert(road);
		}

		// Les plus éloignés entre eux : le premier au hasard, puis chaque fois celui dont la plus petite distance aux points
		// déjà pris est la plus grande
		array<vector> chosen = {};
		if (!candidates.IsEmpty())
		{
			int first = Math.RandomInt(0, candidates.Count());
			chosen.Insert(candidates[first]);
			candidates.Remove(first);
		}
		while (chosen.Count() < count && !candidates.IsEmpty())
		{
			int bestIndex = 0;
			float bestGap = -1;
			foreach (int index, vector candidate : candidates)
			{
				float gap = 1000000;
				foreach (vector taken : chosen)
				{
					float distance = vector.DistanceXZ(taken, candidate);
					if (distance < gap)
						gap = distance;
				}
				if (gap > bestGap)
				{
					bestGap = gap;
					bestIndex = index;
				}
			}
			chosen.Insert(candidates[bestIndex]);
			candidates.Remove(bestIndex);
		}

		// Repli : un point de route au hasard dans la localité, sinon un point au sol dans l'anneau
		int guard = 0;
		while (chosen.Count() < count && guard < count * 3)
		{
			guard++;
			vector extra;
			if (RandomRoadPointNear(center, radius, chosen, extra))
			{
				chosen.Insert(extra);
				continue;
			}
			float fallbackAngle = Math.RandomFloat(0, Math.PI2);
			float fallbackDistance = Math.RandomFloat(radius * 0.7, radius * 0.9);
			vector ground = OnGround(center + Vector(Math.Cos(fallbackAngle) * fallbackDistance, 0, Math.Sin(fallbackAngle) * fallbackDistance));
			if (IsWater(ground) || IsNearBase(ground))
				continue;
			chosen.Insert(ground);
		}

		foreach (vector point : chosen)
			points.Insert(point);
		return chosen.Count() >= count;
	}

	// Recherche d'un bâtiment (rappel statique de QueryEntitiesBySphere, sur le modèle de SRP_HeliSearch)
	protected static ref array<vector> s_aBuildingSpots = {};
	protected static const int BUILDING_HITS_MAX = 200;		// candidats retenus au plus par recherche

	//------------------------------------------------------------------------------------------------
	//! Un BÂTIMENT de la localité (à moins de 0,7 × le rayon du centre), d'au moins 6 × 6 × 3 m (bornes × échelle),
	//! tiré au hasard, à plus de 30 m des points déjà pris (« avoid ») : le point est au pied du bâtiment (son origine,
	//! au sol). Faux s'il n'y en a pas. Limite connue : la garnison DANS les bâtiments de CRX n'est pas active
	//! (#ifdef CRX_DEVELOPMENT_WIP) ; le point Defend de 8 m y envoie les soldats, qui peuvent rester devant.
	static bool FindBuildingSpot(vector center, float radius, array<vector> avoid, out vector spot)
	{
		if (radius <= 0)
			return false;
		s_aBuildingSpots.Clear();
		GetGame().GetWorld().QueryEntitiesBySphere(center, radius * 0.7, BuildingCandidate, null, EQueryEntitiesFlags.STATIC);

		array<vector> freeSpots = {};
		foreach (vector candidate : s_aBuildingSpots)
		{
			bool taken = false;
			if (avoid)
			{
				foreach (vector used : avoid)
				{
					if (vector.DistanceXZ(used, candidate) < 30)
						taken = true;
				}
			}
			if (!taken)
				freeSpots.Insert(candidate);
		}
		s_aBuildingSpots.Clear();
		if (freeSpots.IsEmpty())
			return false;
		spot = freeSpots.GetRandomElement();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Rappel de la recherche de bâtiment : un Building assez grand (6 × 6 × 3 m au moins), hors de l'eau et de la base
	protected static bool BuildingCandidate(IEntity entity)
	{
		if (!entity || !Building.Cast(entity))
			return true;
		vector mins;
		vector maxs;
		entity.GetBounds(mins, maxs);
		float scale = entity.GetScale();
		if ((maxs[0] - mins[0]) * scale < 6 || (maxs[2] - mins[2]) * scale < 6 || (maxs[1] - mins[1]) * scale < 3)
			return true;
		vector position = OnGround(entity.GetOrigin());
		if (IsWater(position) || IsNearBase(position))
			return true;
		s_aBuildingSpots.Insert(position);
		return s_aBuildingSpots.Count() < BUILDING_HITS_MAX;
	}
}
