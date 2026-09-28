//------------------------------------------------------------------------------------------------
// SimpleRP — Brèche de porte à l'explosif (cahier des charges validé par Jack le 24/09/2026)
// Une charge d'Hexomax (DemoBlock_M112) ou un pétard de tolite (DemoBlock_TSh400g) qui explose collé à un battant
// de porte, ou posé juste à côté (environ 1 m : au pied de la porte, sur le cadre, sur le mur tout près), fait
// DISPARAÎTRE ce battant pour tout le monde, avec des éclats et de la fumée à sa place. Double porte : seulement le
// battant visé. Les grenades ne sont pas concernées.
//
// Pourquoi « disparaître » : aucune porte de maison n'est destructible dans le jeu (Door_Base.et n'a pas de zone de
// dégâts) et les portes de la carte ne sont pas répliquées (RplComponent RuntimeOnly) : les supprimer sur le serveur
// ne suffit pas. On reprend le schéma du jeu pour l'intérieur d'une maison qui s'écroule
// (SCR_DestructibleBuildingComponent) : le serveur décide, envoie un RPC à tous, chaque machine supprime la porte
// chez elle, et la liste part aux joueurs qui se connectent ensuite (RplSave/RplLoad, comme SCR_MPDestructionManager).
//
// Mise à feu (serveur) : SCR_ExplosiveTriggerComponent.UseTrigger (minuterie, exploseur M34, homme mort ACE,
// « Détruire » du Game Master) et SCR_MineDamageManager.ExplodeWrapper (mise à feu par sympathie). On relève la
// charge AVANT l'explosion, et on agit ~100 ms plus tard, jamais dans UseTrigger : l'explosion n'a lieu qu'à l'image
// suivante (RPC_DoTrigger s'arrête si la charge n'existe plus) et supprimer la porte emporterait ce qui y est posé.
//
// Recherche du battant, dans l'ordre :
//  1) le parent de la charge porte un BaseDoorComponent (portes créées en jeu : bâtiments CQB, Game Master) ;
//  2) tracé court le long de la normale de pose : le battant contre lequel la charge est collée ;
//  3) sphère de 1,2 m : parmi les battants à moins de 1 m de la charge et à moins de 0,6 m de leur plan, celui dont
//     le plan est le plus proche (à égalité, le plus proche tout court : double porte). Réglages sur le composant.
// Portes de véhicule : exclues (elles n'ont pas de BaseDoorComponent ; contrôle du véhicule racine en plus).
// Durée : jusqu'au redémarrage (aucune sauvegarde). Aucune zone protégée. Trace au journal : catégorie BRECHE.
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Une brèche de la partie : le battant supprimé (identifiant dans le monde, position de son origine, prefab)
class SRP_BrecheEntry
{
	EntityID m_Id;
	vector m_vPos;
	string m_sPrefab;
	bool m_bReplicated;		// porte créée en jeu (CQB, Game Master) : sa suppression par le serveur est déjà répliquée
	vector m_vAxisX;
	vector m_vAxisY;
	vector m_vAxisZ;
	vector m_vMins;
	vector m_vMaxs;
	bool m_bHasShape;		// serveur seulement ; faux pour les brèches reçues à la connexion

	//------------------------------------------------------------------------------------------------
	void SRP_BrecheEntry(EntityID id, vector pos, string prefab, bool replicated)
	{
		m_Id = id;
		m_vPos = pos;
		m_sPrefab = prefab;
		m_bReplicated = replicated;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : repère (axes normés, portes à l'échelle 1) et boîte locale du battant au moment de la brèche
	void SetShape(IEntity door)
	{
		vector doorMat[4];
		door.GetWorldTransform(doorMat);
		vector axisX = doorMat[0];
		vector axisY = doorMat[1];
		vector axisZ = doorMat[2];
		axisX.Normalize();
		axisY.Normalize();
		axisZ.Normalize();
		m_vAxisX = axisX;
		m_vAxisY = axisY;
		m_vAxisZ = axisZ;
		vector mins;
		vector maxs;
		door.GetBounds(mins, maxs);
		m_vMins = mins;
		m_vMaxs = maxs;
		m_bHasShape = true;
	}
}

//------------------------------------------------------------------------------------------------
//! Une charge qui vient de partir, relevée avant l'explosion (serveur)
class SRP_BrecheRequest
{
	IEntity m_Charge;
	IEntity m_Parent;
	vector m_vPos;
	vector m_vNormal;
	int m_iPlayerId;
	bool m_bGameMaster;
	string m_sCharge;
	string m_sHow;
	string m_sTraced;
	IEntity m_Door;			// battant visé, cherché à la mise à feu (avant l'explosion)
	string m_sFound;		// comment : parent, tracé, voisinage, aucun
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Brèche SimpleRP : une charge d'Hexomax ou un pétard de tolite souffle le battant de porte visé, pour tous les joueurs")]
class SRP_BrecheComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_BrecheComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.EditBox, "Distance maximale entre la charge et le battant, en mètres (charge posée à côté de la porte)", category: "SimpleRP")]
	protected float m_fMaxDistance;

	[Attribute("0.6", UIWidgets.EditBox, "Distance maximale entre la charge et le plan du battant, en mètres (évite de souffler la porte de la pièce voisine à travers un mur)", category: "SimpleRP")]
	protected float m_fMaxPlaneDistance;

	[Attribute("1.2", UIWidgets.EditBox, "Rayon de recherche des battants autour de la charge, en mètres", category: "SimpleRP")]
	protected float m_fSearchRadius;

	[Attribute("100", UIWidgets.EditBox, "Délai entre la mise à feu et la brèche, en millisecondes (jamais 0 : l'explosion a lieu à l'image suivante)", category: "SimpleRP")]
	protected int m_iDelayMs;

	[Attribute("0.5", UIWidgets.EditBox, "Chez chaque joueur : écart maximal entre la porte retrouvée par son identifiant et la position envoyée par le serveur, en mètres", category: "SimpleRP")]
	protected float m_fMatchDistance;

	[Attribute("0.3", UIWidgets.EditBox, "Chez chaque joueur, repli : rayon de recherche de la porte autour de la position envoyée par le serveur, en mètres", category: "SimpleRP")]
	protected float m_fFallbackRadius;

	[Attribute("1", UIWidgets.CheckBox, "Diagnostic dans la console (parent de la charge, tracé, candidats, porte trouvée chez chaque joueur) : utile aux premiers essais", category: "SimpleRP")]
	protected bool m_bDiag;

	// Effets : la fumée durable (~10 s) est déjà celle de la charge (Explosion_M112.ptc, Explosion_TNT_400g.ptc),
	// à l'endroit de la porte ; on ajoute les effets de destruction d'objets du jeu, à l'échelle d'une porte
	// (volume d'émission de 0,5 à 1,5 m : pas de nuage de 10 m qui traverserait les murs en intérieur)
	[Attribute("{63D673751320F318}Particles/Props/Dest_Prop_Wood_Medium.ptc", UIWidgets.ResourceNamePicker, "Éclats d'une porte en bois (particules des portes destructibles du jeu : latrines, portail de briques)", "ptc", category: "SimpleRP")]
	protected ResourceName m_sDebrisWood;

	[Attribute("{3648A5EB119BD17C}Particles/Props/Dest_Prop_Wood_Large.ptc", UIWidgets.ResourceNamePicker, "Gros éclats et nuage de poussière d'une porte en bois", "ptc", category: "SimpleRP")]
	protected ResourceName m_sDustWood;

	[Attribute("{66DAEB775AA8CA4C}Particles/Props/Dest_Prop_Metal_Medium.ptc", UIWidgets.ResourceNamePicker, "Étincelles et poussière d'une porte en métal (prefab dont le nom contient « Metal »)", "ptc", category: "SimpleRP")]
	protected ResourceName m_sDebrisMetal;

	[Attribute("{98299D0E34CFBB62}Particles/Props/Dest_Prop_Metal_Large.ptc", UIWidgets.ResourceNamePicker, "Gerbe d'étincelles et nuage de poussière d'une porte en métal", "ptc", category: "SimpleRP")]
	protected ResourceName m_sDustMetal;

	static const int RPL_DOOR_DELAY_MS = 500;		// porte répliquée (CQB, Game Master) : les joueurs détachent d'abord ce qui y est posé
	static const int PENDING_FIRST_MS = 1000;		// joueur arrivé en cours de partie : premier essai après la connexion
	static const int PENDING_RETRY_MS = 3000;
	static const int PENDING_MAX_TRIES = 5;
	static const int SEEN_MAX = 64;					// charges déjà relevées (anti-doublon : minuterie + exploseur, sympathie)
	static const float TRACE_FRONT = 0.05;			// tracé : départ devant la charge, le long de la normale de pose
	static const float TRACE_BACK = 0.15;			// tracé : arrivée derrière la charge
	static const float PLANE_TIE = 0.05;			// deux battants dont les plans sont à moins de 5 cm d'écart : le plus proche gagne
	static const float SLIDING_MATCH = 3.5;			// porte coulissante : son origine suit le battant (course du jeu : 2,6 m au plus, Door_FactoryHall_E_01_Sliding)

	protected static SRP_BrecheComponent s_Instance;

	protected static ref array<IEntity> s_aQuery = {};
	protected static ref array<IEntity> s_aPlaced = {};
	protected static ref array<IEntity> s_aChildren = {};

	// Serveur : brèches de la partie, charges en attente, charges déjà relevées
	protected ref array<ref SRP_BrecheEntry> m_aBreches = new array<ref SRP_BrecheEntry>();
	protected ref array<ref SRP_BrecheRequest> m_aRequests = new array<ref SRP_BrecheRequest>();
	protected ref array<EntityID> m_aSeenCharges = new array<EntityID>();
	protected ref SRP_BrecheRequest m_Current;

	// Joueur arrivé en cours de partie : brèches reçues à la connexion, pas encore appliquées
	protected ref array<ref SRP_BrecheEntry> m_aPending = new array<ref SRP_BrecheEntry>();
	protected int m_iPendingTries;

	//------------------------------------------------------------------------------------------------
	static SRP_BrecheComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(ProcessNext);
		GetGame().GetCallqueue().Remove(ApplyPending);
		GetGame().GetCallqueue().Remove(DeleteDoorLater);
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de brèches de la partie (serveur)
	int GetCount()
	{
		return m_aBreches.Count();
	}

	//------------------------------------------------------------------------------------------------
	// Mise à feu (serveur)
	//------------------------------------------------------------------------------------------------
	//! Une charge part (UseTrigger ou sympathie). Ne fait rien pour une autre charge qu'Hexomax ou tolite.
	static void OnChargeFired(IEntity charge, Instigator instigator, string how)
	{
		if (!charge || !s_Instance || !Replication.IsServer())
			return;
		s_Instance.Queue(charge, instigator, how);
	}

	//------------------------------------------------------------------------------------------------
	//! Hexomax (DemoBlock_M112) ou pétard de tolite (DemoBlock_TSh400g), toutes variantes de ces prefabs
	static bool IsBreachCharge(IEntity charge)
	{
		if (!charge)
			return false;
		EntityPrefabData prefabData = charge.GetPrefabData();
		if (!prefabData)
			return false;
		string name = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
		return name.Contains("DemoBlock_M112") || name.Contains("DemoBlock_TSh400g");
	}

	//------------------------------------------------------------------------------------------------
	//! Relève la charge (position, normale de pose, parent, joueur) et confie la suite à ProcessNext, ~100 ms plus tard
	protected void Queue(IEntity charge, Instigator instigator, string how)
	{
		if (charge.IsDeleted() || !IsBreachCharge(charge))
			return;

		// Anti-doublon : minuterie puis exploseur, deux exploseurs, ou sympathie sur une charge déjà partie
		EntityID chargeId = charge.GetID();
		foreach (EntityID seen : m_aSeenCharges)
		{
			if (seen == chargeId)
				return;
		}
		m_aSeenCharges.Insert(chargeId);
		if (m_aSeenCharges.Count() > SEEN_MAX)
			m_aSeenCharges.RemoveOrdered(0);

		string chargeName = PrefabName(charge);

		// Charge portée (sac, gilet) ou dans un véhicule : pas de brèche
		IEntity root = charge.GetRootParent();
		if (root && root != charge && (ChimeraCharacter.Cast(root) || Vehicle.Cast(root)))
		{
			Diag(string.Format("%1 (%2) portée ou dans un véhicule (%3) : pas de brèche", chargeName, how, PrefabName(root)));
			return;
		}

		vector mat[4];
		charge.GetWorldTransform(mat);

		SRP_BrecheRequest request = new SRP_BrecheRequest();
		request.m_Charge = charge;
		request.m_Parent = charge.GetParent();
		request.m_vPos = mat[3];
		request.m_vNormal = mat[1];		// normale de la surface de pose (SCR_ItemPlacementComponent : l'axe haut suit la surface)
		request.m_sCharge = chargeName;
		request.m_sHow = how;
		if (instigator)
		{
			request.m_iPlayerId = instigator.GetInstigatorPlayerID();
			request.m_bGameMaster = instigator.GetInstigatorType() == InstigatorType.INSTIGATOR_GM;
		}

		// Battant visé cherché dès maintenant (lecture seule) : la charge et tous les battants sont encore en place.
		// 100 ms plus tard, il peut avoir disparu (portail détruit par le jeu, battant soufflé par une autre charge)
		// et la recherche se reporterait sur le battant voisin
		string found;
		request.m_Door = FindDoor(request, found);
		request.m_sFound = found;
		m_aRequests.Insert(request);
		GetGame().GetCallqueue().CallLater(ProcessNext, Math.MaxInt(1, m_iDelayMs), false);
	}

	//------------------------------------------------------------------------------------------------
	//! La plus ancienne charge en attente (toutes attendent le même délai)
	protected void ProcessNext()
	{
		if (m_aRequests.IsEmpty())
			return;
		// Gardée en référence forte le temps du traitement, retirée de la file avant (pas de nouvel essai en boucle)
		m_Current = m_aRequests[0];
		m_aRequests.RemoveOrdered(0);
		if (m_Current)
			Process(m_Current);
		m_Current = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : trouve le battant, le retient, le supprime ici et chez tous les joueurs, note au journal
	protected void Process(SRP_BrecheRequest request)
	{
		string how = request.m_sFound;
		IEntity door = request.m_Door;		// null : aucun battant à portée, ou battant visé disparu depuis la mise à feu
		if (!door)
		{
			Diag(string.Format("%1 (%2), parent %3, tracé %4, recherche %5 : pas de brèche (aucun battant à portée, ou battant visé disparu depuis la mise à feu), carré %6", request.m_sCharge, request.m_sHow, PrefabName(request.m_Parent), request.m_sTraced, how, SRP_FleetManagerComponent.GridOf(request.m_vPos)));
			return;
		}

		EntityID id = door.GetID();
		if (door.IsDeleted() || IsKnown(id))
			return;

		vector pos = door.GetOrigin();
		string doorName = PrefabName(door);
		bool replicated = IsReplicated(door);
		SRP_BrecheEntry breach = new SRP_BrecheEntry(id, pos, doorName, replicated);
		breach.SetShape(door);
		m_aBreches.Insert(breach);

		string replicatedText = "non";
		if (replicated)
			replicatedText = "oui";
		Diag(string.Format("%1 (%2), parent %3, tracé %4 : battant %5 trouvé par %6, répliqué %7, identifiant %8", request.m_sCharge, request.m_sHow, PrefabName(request.m_Parent), request.m_sTraced, doorName, how, replicatedText, id));

		// Navigation des IA : les zones sont relevées maintenant, reconstruites 1 s plus tard (porte supprimée)
		SCR_DestructionUtility.RegenerateNavmeshDelayed(door);

		// Tous les joueurs, puis ici (serveur : effets seulement s'il y a un écran, serveur hébergé ou Workbench)
		Rpc(RpcDo_SRPBreche, id, pos);
		RemoveDoorHere(door, !System.IsConsoleApp(), true, replicated);

		SRP_JournalComponent.Log("BRECHE", string.Format("Brèche : %1 a soufflé une porte (%2), carré %3", WhoText(request.m_iPlayerId, request.m_bGameMaster), doorName, SRP_FleetManagerComponent.GridOf(pos)));
	}

	//------------------------------------------------------------------------------------------------
	//! Le battant visé : parent de la charge, sinon tracé le long de la normale, sinon le plus proche (seuils)
	protected IEntity FindDoor(SRP_BrecheRequest request, out string how)
	{
		request.m_sTraced = "non fait";

		// 1) La charge est posée sur le battant (ou sur une vitre du battant) : porte créée en jeu, répliquée
		IEntity door = DoorOf(request.m_Parent);
		if (door && !IsVehiclePart(door))
		{
			how = "parent";
			return door;
		}

		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			how = "aucun monde";
			return null;
		}

		// 2) Tracé court, mêmes réglages que la pose (SCR_ItemPlacementComponent.UseXYZPlacement), sans filtre :
		// il touche le battant contre lequel la charge est collée, même ouvert ou en mouvement
		vector normal = request.m_vNormal;
		if (normal.Length() > 0.001)
		{
			normal.Normalize();
			TraceParam param = new TraceParam();
			param.Start = request.m_vPos + normal * TRACE_FRONT;
			param.End = request.m_vPos - normal * TRACE_BACK;
			param.Flags = TraceFlags.WORLD | TraceFlags.ENTS;
			param.LayerMask = EPhysicsLayerPresets.Projectile;
			if (request.m_Charge && !request.m_Charge.IsDeleted())
				param.Exclude = request.m_Charge;
			float fraction = world.TraceMove(param, null);
			if (fraction < 1)
			{
				request.m_sTraced = PrefabName(param.TraceEnt);
				door = DoorOf(param.TraceEnt);
				if (door && !IsVehiclePart(door))
				{
					how = "tracé";
					return door;
				}
			}
			else
			{
				request.m_sTraced = "rien";
			}
		}

		// 3) Charge posée à côté : le battant dont le plan est le plus proche, dans les seuils
		door = NearestDoor(world, request.m_vPos);
		if (door)
			how = "voisinage";
		else
			how = "aucun";
		return door;
	}

	//------------------------------------------------------------------------------------------------
	//! Parmi les battants autour de la charge : à moins de m_fMaxDistance du battant et de m_fMaxPlaneDistance
	//! de son plan, celui dont le plan est le plus proche ; à égalité (double porte), le plus proche du battant.
	protected IEntity NearestDoor(BaseWorld world, vector pos)
	{
		s_aQuery.Clear();
		world.QueryEntitiesBySphere(pos, m_fSearchRadius, QueryAdd, null, EQueryEntitiesFlags.ALL);

		IEntity best;
		float bestPlane = 1000;
		float bestBox = 1000;
		// Battants déjà soufflés (peut-être déjà supprimés) : ils concourent avec leur forme ; si l'un d'eux gagne,
		// la charge le visait : pas de brèche sur le battant voisin
		bool bestKnown = false;
		foreach (SRP_BrecheEntry known : m_aBreches)
		{
			if (!known || !known.m_bHasShape)
				continue;
			vector rel = pos - known.m_vPos;
			vector knownLocal = Vector(vector.Dot(rel, known.m_vAxisX), vector.Dot(rel, known.m_vAxisY), vector.Dot(rel, known.m_vAxisZ));
			float knownBox = BoxDistance(knownLocal, known.m_vMins, known.m_vMaxs);
			float knownPlane = PlaneDistance(knownLocal, known.m_vMins, known.m_vMaxs);
			if (knownBox > m_fMaxDistance || knownPlane > m_fMaxPlaneDistance)
				continue;
			if (!bestKnown || knownPlane < bestPlane - PLANE_TIE || (knownPlane <= bestPlane + PLANE_TIE && knownBox < bestBox))
			{
				bestKnown = true;
				bestPlane = knownPlane;
				bestBox = knownBox;
			}
		}
		foreach (IEntity candidate : s_aQuery)
		{
			if (!candidate || candidate.IsDeleted())
				continue;
			if (!candidate.FindComponent(BaseDoorComponent) || IsVehiclePart(candidate))
				continue;
			// Battant déjà soufflé (encore là pour une image) : deux charges sur une double porte, une par battant
			if (IsKnown(candidate.GetID()))
				continue;

			// Distances mesurées dans le repère du battant (il tourne avec la porte) : boîte du modèle, plan médian
			// de sa plus petite dimension (l'épaisseur du battant)
			vector mins;
			vector maxs;
			candidate.GetBounds(mins, maxs);
			vector localPos = candidate.CoordToLocal(pos);
			float boxDist = BoxDistance(localPos, mins, maxs);
			float planeDist = PlaneDistance(localPos, mins, maxs);
			Diag(string.Format("candidat %1 : %2 m du battant, %3 m de son plan", PrefabName(candidate), boxDist, planeDist));
			if (boxDist > m_fMaxDistance || planeDist > m_fMaxPlaneDistance)
				continue;

			bool better = false;
			if (!best && !bestKnown)
				better = true;
			else if (planeDist < bestPlane - PLANE_TIE)
				better = true;
			else if (planeDist <= bestPlane + PLANE_TIE && boxDist < bestBox)
				better = true;
			if (better)
			{
				best = candidate;
				bestPlane = planeDist;
				bestBox = boxDist;
				bestKnown = false;
			}
		}
		s_aQuery.Clear();
		if (bestKnown)
		{
			Diag("la charge vise un battant déjà soufflé : pas de brèche sur le battant voisin");
			return null;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance d'un point (repère local) à la boîte [mins, maxs] ; 0 à l'intérieur
	protected static float BoxDistance(vector p, vector mins, vector maxs)
	{
		float dx = Math.Max(Math.Max(mins[0] - p[0], 0), p[0] - maxs[0]);
		float dy = Math.Max(Math.Max(mins[1] - p[1], 0), p[1] - maxs[1]);
		float dz = Math.Max(Math.Max(mins[2] - p[2], 0), p[2] - maxs[2]);
		return Math.Sqrt(dx * dx + dy * dy + dz * dz);
	}

	//------------------------------------------------------------------------------------------------
	//! Distance d'un point (repère local) au plan médian de la plus petite dimension de la boîte (plan du battant)
	protected static float PlaneDistance(vector p, vector mins, vector maxs)
	{
		vector size = maxs - mins;
		int axis = 0;
		if (size[1] < size[axis])
			axis = 1;
		if (size[2] < size[axis])
			axis = 2;
		float middle = (mins[axis] + maxs[axis]) * 0.5;
		return Math.AbsFloat(p[axis] - middle);
	}

	//------------------------------------------------------------------------------------------------
	//! Première entité portant un BaseDoorComponent en remontant depuis e (e compris), ou null
	protected static IEntity DoorOf(IEntity e)
	{
		IEntity current = e;
		int guard = 0;
		while (current && guard < 6)
		{
			if (current.FindComponent(BaseDoorComponent))
				return current;
			current = current.GetParent();
			guard++;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Vrai si l'entité fait partie d'un véhicule (portes de véhicule : jamais de brèche)
	protected static bool IsVehiclePart(IEntity e)
	{
		if (!e)
			return false;
		IEntity root = e.GetRootParent();
		return root != null && Vehicle.Cast(root) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Vrai si l'entité est répliquée (porte créée en jeu, vitre, objet posé) ; faux pour une porte de la carte
	protected static bool IsReplicated(IEntity e)
	{
		RplComponent rpl = RplComponent.Cast(e.FindComponent(RplComponent));
		return rpl && rpl.Id().IsValid();
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsKnown(EntityID id)
	{
		foreach (SRP_BrecheEntry entry : m_aBreches)
		{
			if (entry && entry.m_Id == id)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool QueryAdd(IEntity e)
	{
		s_aQuery.Insert(e);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Suppression du battant sur chaque machine
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPBreche(EntityID id, vector pos)
	{
		string how;
		IEntity door = FindDoorLocal(id, pos, how);
		Diag(string.Format("reçu : identifiant %1, carré %2 : porte %3", id, SRP_FleetManagerComponent.GridOf(pos), how));
		if (!door)
			return;
		RemoveDoorHere(door, true, false, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Sur cette machine : la porte par son identifiant (contrôles : c'est une porte, à moins de m_fMatchDistance
	//! de la position du serveur), sinon la porte la plus proche de cette position (m_fFallbackRadius)
	protected IEntity FindDoorLocal(EntityID id, vector pos, out string how)
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
		{
			how = "introuvable (pas de monde)";
			return null;
		}

		IEntity found = world.FindEntityByID(id);
		if (found && !found.IsDeleted() && found.FindComponent(BaseDoorComponent))
		{
			float gap = vector.Distance(found.GetOrigin(), pos);
			// Porte coulissante : l'origine suit le battant ; un joueur arrivé après la brèche la voit fermée, alors
			// qu'elle a pu être soufflée ouverte (même identifiant, porte coulissante : la garde reste forte)
			float maxGap = m_fMatchDistance;
			if (found.FindComponent(SlidingDoorComponent))
				maxGap = Math.Max(maxGap, SLIDING_MATCH);
			if (gap <= maxGap)
			{
				how = string.Format("trouvée par identifiant (%1, écart %2 m)", PrefabName(found), gap);
				return found;
			}
		}

		s_aQuery.Clear();
		world.QueryEntitiesBySphere(pos, Math.Max(m_fFallbackRadius, 0.05), QueryAdd, null, EQueryEntitiesFlags.ALL);
		IEntity best;
		float bestDist = m_fFallbackRadius;
		foreach (IEntity candidate : s_aQuery)
		{
			if (!candidate || candidate.IsDeleted() || !candidate.FindComponent(BaseDoorComponent))
				continue;
			float dist = vector.Distance(candidate.GetOrigin(), pos);
			if (dist <= bestDist)
			{
				best = candidate;
				bestDist = dist;
			}
		}
		s_aQuery.Clear();

		if (best)
			how = string.Format("trouvée par position (%1, écart %2 m)", PrefabName(best), bestDist);
		else
			how = "introuvable";
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Sur cette machine : détache ce qui est posé sur le battant, écarte ses enfants répliqués (vitres), joue les
	//! effets, supprime le battant. Serveur : les vitres sont supprimées tout de suite (répliqué à tous) et le battant
	//! un cycle plus tard (500 ms pour une porte répliquée). Joueur : RplComponent.DeleteRplEntity supprime ici une
	//! porte de la carte ; pour une porte répliquée, il attend la suppression par le serveur.
	protected void RemoveDoorHere(IEntity door, bool effects, bool authority, bool replicated)
	{
		if (!door || door.IsDeleted())
			return;

		DetachChildren(door, authority);

		if (effects)
			PlayEffects(door);

		// Le battant disparaît de l'écran tout de suite, même quand sa suppression attend (porte répliquée : 500 ms
		// sur le serveur, puis l'arrivée de la suppression du serveur chez les joueurs)
		door.ClearFlags(EntityFlags.VISIBLE, true);

		if (authority)
		{
			int delay = 0;
			if (replicated)
				delay = RPL_DOOR_DELAY_MS;
			GetGame().GetCallqueue().CallLater(DeleteDoorLater, delay, false, door);
			return;
		}
		RplComponent.DeleteRplEntity(door, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : suppression du battant (porte de la carte : ici seulement ; porte répliquée : pour tous)
	protected void DeleteDoorLater(IEntity door)
	{
		if (!door || door.IsDeleted())
			return;
		RplComponent.DeleteRplEntity(door, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce qui est posé sur le battant (autres charges…) est détaché et posé au sol, pas supprimé avec lui ;
	//! ses enfants répliqués directs (vitres) : supprimés par le serveur, détachés chez un joueur (la suppression
	//! du serveur suit) pour que la suppression locale du battant ne les emporte pas.
	protected static void DetachChildren(IEntity door, bool authority)
	{
		s_aPlaced.Clear();
		CollectPlaced(door, 0);
		foreach (IEntity placed : s_aPlaced)
		{
			if (!placed || placed.IsDeleted())
				continue;
			SCR_PlaceableInventoryItemComponent placeable = SCR_PlaceableInventoryItemComponent.Cast(placed.FindComponent(SCR_PlaceableInventoryItemComponent));
			if (placeable)
				placeable.SRP_BrecheDetach();
			IEntity holder = placed.GetParent();
			if (holder)
				holder.RemoveChild(placed, true);
		}
		s_aPlaced.Clear();

		s_aChildren.Clear();
		IEntity child = door.GetChildren();
		while (child)
		{
			s_aChildren.Insert(child);
			child = child.GetSibling();
		}
		foreach (IEntity directChild : s_aChildren)
		{
			if (!directChild || directChild.IsDeleted() || !IsReplicated(directChild))
				continue;
			if (authority)
			{
				RplComponent.DeleteRplEntity(directChild, false);
				continue;
			}
			// Joueur : détachée, masquée et sans collision en attendant la suppression par le serveur (si elle
			// n'arrive jamais, par exemple pour un joueur connecté après la brèche, la vitre ne flotte pas dans l'embrasure)
			door.RemoveChild(directChild, true);
			directChild.ClearFlags(EntityFlags.VISIBLE, true);
			Physics physics = directChild.GetPhysics();
			if (physics)
				physics.Destroy();
		}
		s_aChildren.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Objets posés (SCR_PlaceableInventoryItemComponent) n'importe où sous le battant (vitre comprise)
	protected static void CollectPlaced(IEntity parent, int depth)
	{
		if (!parent || depth > 4)
			return;
		IEntity child = parent.GetChildren();
		while (child)
		{
			if (child.FindComponent(SCR_PlaceableInventoryItemComponent))
				s_aPlaced.Insert(child);
			else
				CollectPlaced(child, depth + 1);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Effets locaux au centre du battant : éclats et nuage de poussière. Bois par défaut, métal si le nom du prefab
	//! contient « Metal ». Le bruit, l'éclair, le souffle et la fumée restent ceux de la charge.
	protected void PlayEffects(IEntity door)
	{
		vector mins;
		vector maxs;
		door.GetWorldBounds(mins, maxs);
		vector center = (mins + maxs) * 0.5;

		string low = PrefabName(door);
		low.ToLower();
		if (low.Contains("metal"))
		{
			SpawnParticle(m_sDebrisMetal, center);
			SpawnParticle(m_sDustMetal, center);
			return;
		}
		SpawnParticle(m_sDebrisWood, center);
		SpawnParticle(m_sDustWood, center);
	}

	//------------------------------------------------------------------------------------------------
	//! Particules posées droites à cette position (comme SCR_BuildingSetup / SCR_DestructionCommon)
	protected static void SpawnParticle(ResourceName particle, vector pos)
	{
		if (particle.IsEmpty())
			return;
		ParticleEffectEntitySpawnParams spawnParams = new ParticleEffectEntitySpawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform[0] = vector.Right;
		spawnParams.Transform[1] = vector.Up;
		spawnParams.Transform[2] = vector.Forward;
		spawnParams.Transform[3] = pos;
		spawnParams.UseFrameEvent = true;
		ParticleEffectEntity.SpawnParticleEffect(particle, spawnParams);
	}

	//------------------------------------------------------------------------------------------------
	// Joueurs arrivés en cours de partie : le game mode transmet la liste des portes de la carte soufflées
	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		// Les portes répliquées (CQB, Game Master) ne sont pas envoyées : leur suppression par le serveur l'est déjà
		int count = 0;
		foreach (SRP_BrecheEntry entry : m_aBreches)
		{
			if (entry && !entry.m_bReplicated)
				count++;
		}
		writer.WriteInt(count);
		foreach (SRP_BrecheEntry sent : m_aBreches)
		{
			if (!sent || sent.m_bReplicated)
				continue;
			writer.WriteEntityId(sent.m_Id);
			writer.WriteVector(sent.m_vPos);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int count;
		reader.ReadInt(count);
		EntityID id;
		vector pos;
		for (int i = 0; i < count; i++)
		{
			reader.ReadEntityId(id);
			reader.ReadVector(pos);
			m_aPending.Insert(new SRP_BrecheEntry(id, pos, "", false));
		}
		if (!m_aPending.IsEmpty())
		{
			m_iPendingTries = 0;
			GetGame().GetCallqueue().Remove(ApplyPending);
			GetGame().GetCallqueue().CallLater(ApplyPending, PENDING_FIRST_MS, false);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Applique les brèches reçues à la connexion, une fois le monde en place ; nouvel essai pour celles dont la
	//! porte n'est pas encore trouvée (PENDING_MAX_TRIES essais). Pas d'effets : la brèche est déjà ancienne.
	protected void ApplyPending()
	{
		m_iPendingTries++;
		string how;
		for (int i = m_aPending.Count() - 1; i >= 0; i--)
		{
			SRP_BrecheEntry entry = m_aPending[i];
			if (!entry)
			{
				m_aPending.RemoveOrdered(i);
				continue;
			}
			IEntity door = FindDoorLocal(entry.m_Id, entry.m_vPos, how);
			if (!door)
				continue;
			Diag(string.Format("connexion : identifiant %1, carré %2 : porte %3", entry.m_Id, SRP_FleetManagerComponent.GridOf(entry.m_vPos), how));
			RemoveDoorHere(door, false, false, false);
			m_aPending.RemoveOrdered(i);
		}

		if (m_aPending.IsEmpty())
			return;
		if (m_iPendingTries >= PENDING_MAX_TRIES)
		{
			Print(string.Format("[SRP] Brèche : %1 porte(s) soufflée(s) avant la connexion introuvable(s) après %2 essais", m_aPending.Count(), m_iPendingTries), LogLevel.WARNING);
			m_aPending.Clear();
			return;
		}
		GetGame().GetCallqueue().CallLater(ApplyPending, PENDING_RETRY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	// Textes
	//------------------------------------------------------------------------------------------------
	//! « Pseudo [identité] », le pseudo seul, ou « inconnu » ; « (Game Master) » pour « Détruire » au Game Master
	protected static string WhoText(int playerId, bool gameMaster)
	{
		string who = "inconnu";
		if (playerId > 0)
		{
			SRP_PlayerRecord record;
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			if (players)
				record = players.GetRecord(playerId);
			if (record)
			{
				who = record.m_sName + " [" + record.m_sIdentity + "]";
			}
			else
			{
				string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
				if (!name.IsEmpty())
					who = name;
			}
		}
		if (gameMaster)
			who += " (Game Master)";
		return who;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom court du prefab (« Door_Village_E_A_LEFT_EXT_ST_B »), à défaut la classe, « aucun » pour null
	protected static string PrefabName(IEntity e)
	{
		if (!e)
			return "aucun";
		EntityPrefabData prefabData = e.GetPrefabData();
		if (!prefabData)
			return e.ClassName();
		string name = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
		if (name.IsEmpty())
			return e.ClassName();
		return name;
	}

	//------------------------------------------------------------------------------------------------
	protected void Diag(string text)
	{
		if (m_bDiag)
			Print("[SRP] Brèche (diag) : " + text, LogLevel.NORMAL);
	}
}

//------------------------------------------------------------------------------------------------
//! Minuterie, exploseur M34, homme mort ACE, « Détruire » du Game Master : tous passent par UseTrigger côté
//! serveur (l'exploseur d'AMF-CORE aussi, AMF_Scorpion_IEDjammer_Detenator_Override.c). On relève la charge avant
//! la mise à feu vanilla, qui reste inchangée.
modded class SCR_ExplosiveTriggerComponent
{
	//------------------------------------------------------------------------------------------------
	override void UseTrigger()
	{
		// UseTrigger passe aussi chez le joueur propriétaire : la brèche ne se décide que sur le serveur
		if (Replication.IsServer())
		{
			IEntity owner = GetOwner();
			if (owner && owner.FindComponent(RplComponent))
				SRP_BrecheComponent.OnChargeFired(owner, GetInstigator(), "mise à feu");
		}
		super.UseTrigger();
	}
}

//------------------------------------------------------------------------------------------------
//! Mise à feu par sympathie (charge détruite par une explosion voisine) : ce chemin n'appelle pas UseTrigger.
//! Les mines passent aussi par là : seulement les entités qui ont un SCR_ExplosiveChargeComponent.
modded class SCR_MineDamageManager
{
	//------------------------------------------------------------------------------------------------
	override void ExplodeWrapper()
	{
		if (Replication.IsServer())
		{
			IEntity owner = GetOwner();
			if (owner && owner.FindComponent(SCR_ExplosiveChargeComponent))
				SRP_BrecheComponent.OnChargeFired(owner, GetInstigator(), "sympathie");
		}
		super.ExplodeWrapper();
	}
}

//------------------------------------------------------------------------------------------------
//! Brèche d'une porte créée en jeu (CQB, Game Master) : ce qui est posé sur le battant n'est pas supprimé avec lui
modded class SCR_PlaceableInventoryItemComponent
{
	//------------------------------------------------------------------------------------------------
	//! Détache l'objet de son support et le pose au sol (DetachFromParent du jeu, protégée)
	void SRP_BrecheDetach()
	{
		DetachFromParent();
	}
}
