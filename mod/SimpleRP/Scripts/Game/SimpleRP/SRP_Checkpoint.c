//------------------------------------------------------------------------------------------------
// SimpleRP — Point de contrôle routier (mission CONTROLE)
// C'est un soldat qui choisit l'endroit : sur une route, il regarde un équipier et choisit « Établir un point de contrôle
// ici » (SRP_CheckpointStartAction, sur Character_Base.et). Le serveur vérifie le lieu (SRP_MissionManagerComponent.
// StartControl : sur un carré bleu du front et à 1000 m au moins de la base, G10 et Q10), cale le point sur la route,
// crée la mission et prévient le commandement sur Discord. Pas de matériel :
// la section barre la route avec ses véhicules. Tant que le point tient (20 min), la circulation civile passe par lui :
// les voitures s'arrêtent seules. Un conducteur sur cinq est suspect (armes, documents, ennemi en civil, explosifs).
//   - Deux files, une par sens d'arrivée (l'axe de la route au point donne le sens) : une voiture « au point » par sens,
//     les suivantes attendent 40 m derrière et avancent quand la place se libère. Au plus 2 voitures par file.
//   - Les voitures de la circulation ambiante qui se rapprochent (à moins de 200 m) sont prises dans le contrôle comme
//     les autres ; celle qui passe le point sans s'arrêter (> 8 km/h) force le barrage. Pendant un contrôle, la
//     circulation ambiante ne pose plus de voiture à moins de 1 km du point.
//   - Au contact du conducteur, action « Contrôler » (la même action que « Gestion du soldat », sur un civil) : papiers,
//     faire descendre, fouiller (20 s, à côté du véhicule), laisser repartir, arrêter, désamorcer (explosifs, sapeur).
//   - Les papiers donnent des indices, jamais une certitude (sauf des papiers FAUX : suspect certain ; mais un suspect
//     peut avoir des papiers en règle). La fouille tranche.
//   - Braqué (un joueur le vise à moins de 15 m), un conducteur descendu lève les mains (ACE Captives) : les serflex
//     du joueur marchent alors sur lui, comme l'ordre « Arrêter » du menu (menotté, à genoux).
//   - Un suspect se rend, force le barrage, fuit à pied (seulement si personne ne le braque), ou (l'ennemi en civil
//     seulement) sort une arme. Un fuyard rattrapé, braqué ou blessé s'arrête et lève les mains : il reste soignable
//     puis attachable, et compte comme arrestation une fois menotté.
//   - « Repartir » : le conducteur remonte de lui-même (ordre d'embarquement IA), puis roule au-delà du point.
//   - Arrêté, il reste à genoux au bord de la route et disparaît avec son véhicule à la fin de la mission.
//   - Prime : celle de la mission + une prime par saisie et par arrestation − un malus par suspect passé au travers.
//     En cas d'abandon du point, les primes par saisie et arrestation sont quand même versées.
// Tuer un civil reste un crime de guerre (SRP_Treasury). Exceptions : tout suspect révélé (barrage forcé, fuite à pied,
// arme sortie, cargaison trouvée à la fouille). Un civil ordinaire du contrôle reste protégé.
// Journal CONTROLE : chaque changement d'état d'une voiture, avec son numéro et sa distance au point (SetState).
// Le gestionnaire de missions crée le point (StartControl), l'anime toutes les 2 s (TickControl) et le solde (End).
// Le gestionnaire des civils fournit les voitures (section « Point de contrôle » de SRP_Civilians.c).
//------------------------------------------------------------------------------------------------

enum SRP_ECargo
{
	RIEN,
	ARMES,
	DOCUMENTS,
	ENNEMI,
	EXPLOSIFS
}

enum SRP_EReaction
{
	SE_REND,
	FORCE,
	FUIT_A_PIED,
	SORT_UNE_ARME
}

enum SRP_ECarState
{
	APPROCHE,			// roule vers le point (tête de file)
	EN_ATTENTE,			// dans la file, derrière une autre voiture
	ARRETEE,			// à l'arrêt au point : le menu Contrôler s'ouvre
	REPART,				// libérée : remonte en voiture puis s'éloigne au-delà du point
	FORCE_LE_BARRAGE,
	A_PIED,				// fuit à pied
	RENDU,				// fuyard arrêté (braqué, rattrapé, blessé) : mains levées, à arrêter
	HOSTILE,
	CONDUCTEUR_ARRETE,
	CLOSE
}

enum SRP_ECheckpointResult
{
	EN_COURS,
	TENU,
	ABANDONNE
}

//------------------------------------------------------------------------------------------------
//! Réglages, remplis par le gestionnaire de missions à partir de ses attributs
class SRP_CheckpointSettings
{
	int m_iMinutes = 20;
	int m_iMinPlayers = 2;
	int m_iSuspectPercent = 20;
	int m_iSearchSeconds = 20;
	int m_iDefuseSeconds = 30;
	int m_iCarSeconds = 75;
	int m_iThreatPercent = 25;
	int m_iSeizureBonus = 750;
	int m_iArrestBonus = 400;
	int m_iMissedMalus = 750;
	string m_sEnemyFactionKey = "USSR";
	ResourceName m_sPistolPrefab = "{C0F7DD85A86B2900}Prefabs/Weapons/Handguns/PM/Handgun_PM.et";
	ResourceName m_sMagazinePrefab = "{8B853CDD11BA916E}Prefabs/Weapons/Magazines/Magazine_9x18_PM_8rnd_Ball.et";
	ResourceName m_sBlastPrefab = "{98EC9C526AFBA282}Prefabs/Weapons/Ammo/Ammo_Shell_82mm_HE_O832DU.et";
	ResourceName m_sGetInWaypointPrefab = "{712F4795CF8B91C7}Prefabs/AI/Waypoints/AIWaypoint_GetIn.et";
}

//------------------------------------------------------------------------------------------------
//! Une voiture prise dans le contrôle
class SRP_CheckpointCar
{
	int m_iNumber;
	SRP_CivRecord m_Record;
	int m_iState = SRP_ECarState.APPROCHE;
	int m_iStateTick;
	int m_iAddedTick;
	int m_iReadyTick;			// voiture posée : le conducteur est au volant après 3 s
	bool m_bAmbient;			// voiture de la circulation ambiante, adoptée en route
	int m_iLane;				// file : 0 arrive dans le sens de l'axe de la route, 1 dans l'autre
	vector m_vApproachDir;		// sens de marche à l'arrivée (le long de la route), suivi tant qu'elle n'a pas approché le point
	vector m_vTarget;			// dernier point de passage donné (point ou place dans la file)
	bool m_bParked;				// à l'arrêt dans la file (frein à main)
	int m_iCargo = SRP_ECargo.RIEN;
	int m_iReaction = SRP_EReaction.SE_REND;
	bool m_bReacted;
	bool m_bPapers;
	bool m_bFakePapers;			// papiers faux : suspect certain, l'arrestation est justifiée sans fouille
	bool m_bOut;
	int m_iOutTick;
	bool m_bSearched;
	bool m_bFound;				// la fouille (ou la fuite, le barrage forcé) a révélé la cargaison
	bool m_bSeizureCounted;
	bool m_bSurrendered;		// mains levées (ACE Captives)
	bool m_bCaptive;			// menotté (ACE Captives), par le menu ou par les serflex d'un joueur
	bool m_bDefused;			// explosifs rendus inoffensifs
	int m_iSearcherId = -1;
	int m_iSearchEndTick;
	int m_iDefuserId = -1;
	int m_iDefuseEndTick;
	int m_iOrderTick;			// dernier ordre donné (repartir)
	bool m_bDriving;			// repartir : ordre de route donné, le conducteur est au volant
	int m_iRetries;
	string m_sName;
	string m_sFrom;
	string m_sTo;
	ref array<string> m_aClues = {};
	vector m_vLast;
	int m_iStillMs;
	float m_fClosest = 100000;		// plus courte distance entre le point et sa trajectoire (à plat) depuis sa prise
	float m_fLastDistance = 100000;	// distance au point au passage précédent : se rapproche-t-elle ?
	bool m_bThrough;				// barrage forcé : elle a passé le point, ordre de filer au loin donné

	//------------------------------------------------------------------------------------------------
	bool IsSuspect()
	{
		return m_iCargo != SRP_ECargo.RIEN;
	}
}

//------------------------------------------------------------------------------------------------
//! Un véhicule laissé sur place (conducteur mort, hostile expiré, chargement d'explosifs immobilisé) : retiré après
//! 10 min sans joueur à moins de 100 m, ou à la fin de la mission
class SRP_CheckpointWreck
{
	IEntity m_Vehicle;
	SRP_CheckpointCar m_Car;	// si la fiche est encore suivie (conducteur arrêté) : elle part avec le véhicule
	int m_iNumber;
	int m_iAbsentMs;
	string m_sWhy;
}

//------------------------------------------------------------------------------------------------
//! Un point de contrôle : un par mission CONTROLE (plusieurs peuvent tenir en même temps, à plus de 1 km l'un de l'autre)
class SRP_Checkpoint
{
	protected static ref array<SRP_Checkpoint> s_aActive = {};

	string m_sMissionId;
	vector m_vPoint;
	ref SRP_CheckpointSettings m_Settings;

	bool m_bEstablished;
	int m_iAbsentMs;
	int m_iEndTick;
	int m_iNextCarTick;
	int m_iCarNumber;
	int m_iSuspectsSoFar;
	bool m_bAxisKnown;
	vector m_vRoadAxis;			// axe de la route au point (unitaire, à plat)
	IEntity m_Lantern;			// lanterne posée la nuit à côté du point (SRP_GenerateurComponent)

	bool m_bThreat;
	int m_iThreatTick;
	bool m_bWantWave;			// lu et remis à zéro par le gestionnaire de missions, qui envoie la vague

	int m_iChecked;				// conducteurs en règle repartis
	int m_iSeizures;
	int m_iArrests;
	int m_iMissed;				// suspects repartis ou échappés
	int m_iUseless;				// fouilles pour rien
	int m_iKilled;				// suspects abattus (barrage forcé, arme sortie)
	bool m_bIntel;				// des documents ont été saisis

	ref array<ref SRP_CheckpointCar> m_aCars = {};
	ref array<ref SRP_CheckpointWreck> m_aWrecks = {};

	//------------------------------------------------------------------------------------------------
	void SRP_Checkpoint(string missionId, vector point, SRP_CheckpointSettings settings)
	{
		m_sMissionId = missionId;
		m_vPoint = point;
		m_Settings = settings;
		s_aActive.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	void ~SRP_Checkpoint()
	{
		if (s_aActive)
			s_aActive.RemoveItem(this);
	}

	//------------------------------------------------------------------------------------------------
	// Recherches
	//------------------------------------------------------------------------------------------------
	protected static SRP_Checkpoint FindByMission(string missionId)
	{
		foreach (SRP_Checkpoint checkpoint : s_aActive)
		{
			if (checkpoint && checkpoint.m_sMissionId == missionId)
				return checkpoint;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	SRP_CheckpointCar FindCar(int number)
	{
		foreach (SRP_CheckpointCar car : m_aCars)
		{
			if (car.m_iNumber == number)
				return car;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static SRP_CheckpointCar FindCarOf(IEntity character, out SRP_Checkpoint owner)
	{
		foreach (SRP_Checkpoint checkpoint : s_aActive)
		{
			if (!checkpoint)
				continue;
			foreach (SRP_CheckpointCar car : checkpoint.m_aCars)
			{
				if (car.m_Record && car.m_Record.m_Character == character)
				{
					owner = checkpoint;
					return car;
				}
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected static SRP_CheckpointCar FindCarOfVehicle(IEntity vehicle, out SRP_Checkpoint owner)
	{
		foreach (SRP_Checkpoint checkpoint : s_aActive)
		{
			if (!checkpoint)
				continue;
			foreach (SRP_CheckpointCar car : checkpoint.m_aCars)
			{
				if (car.m_Record && car.m_Record.m_Vehicle == vehicle)
				{
					owner = checkpoint;
					return car;
				}
			}
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Un point de contrôle établi à moins de « radius » mètres ? (la circulation ambiante s'en écarte)
	static bool IsNearActive(vector position, float radius)
	{
		foreach (SRP_Checkpoint checkpoint : s_aActive)
		{
			if (checkpoint && checkpoint.m_bEstablished && vector.Distance(checkpoint.m_vPoint, position) < radius)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Tuer ce personnage n'est pas un crime de guerre (appelé par SRP_Treasury) : tout suspect révélé, qu'il force
	//! le barrage (annoncé ou non), fuie à pied, ait sorti une arme, ou dont la cargaison a été trouvée.
	//! Un civil ordinaire du contrôle (rien trouvé, ou pas encore fouillé) reste protégé.
	static bool IsLegitimateTarget(IEntity character)
	{
		SRP_Checkpoint owner;
		SRP_CheckpointCar car = FindCarOf(character, owner);
		if (!car)
			return false;
		if (car.m_iState == SRP_ECarState.FORCE_LE_BARRAGE || car.m_iState == SRP_ECarState.HOSTILE || car.m_iState == SRP_ECarState.A_PIED)
			return true;
		return car.IsSuspect() && car.m_bFound;
	}

	//------------------------------------------------------------------------------------------------
	// Messages et journal
	//------------------------------------------------------------------------------------------------
	protected void NotifyNear(string text, float radius = 250)
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character && vector.Distance(character.GetOrigin(), m_vPoint) <= radius)
				SRP_Utils.NotifyPlayer(id, text);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Log(string text)
	{
		SRP_JournalComponent.Log("CONTROLE", m_sMissionId + " : " + text);
	}

	//------------------------------------------------------------------------------------------------
	protected static string StateName(int state)
	{
		switch (state)
		{
			case SRP_ECarState.APPROCHE: return "en approche";
			case SRP_ECarState.EN_ATTENTE: return "dans la file d'attente";
			case SRP_ECarState.ARRETEE: return "à l'arrêt au point";
			case SRP_ECarState.REPART: return "repart";
			case SRP_ECarState.FORCE_LE_BARRAGE: return "force le barrage";
			case SRP_ECarState.A_PIED: return "conducteur en fuite à pied";
			case SRP_ECarState.RENDU: return "conducteur rendu, mains levées";
			case SRP_ECarState.HOSTILE: return "hostile, arme sortie";
			case SRP_ECarState.CONDUCTEUR_ARRETE: return "conducteur arrêté";
			case SRP_ECarState.CLOSE: return "retirée du contrôle";
		}
		return "état inconnu";
	}

	//------------------------------------------------------------------------------------------------
	//! Distance au point du véhicule (ou du conducteur s'il n'y a plus de véhicule)
	protected float DistanceOf(SRP_CheckpointCar car)
	{
		if (!car.m_Record)
			return 0;
		if (car.m_Record.m_Vehicle && !car.m_Record.m_Vehicle.IsDeleted())
			return vector.Distance(car.m_Record.m_Vehicle.GetOrigin(), m_vPoint);
		if (car.m_Record.m_Character && !car.m_Record.m_Character.IsDeleted())
			return vector.Distance(car.m_Record.m_Character.GetOrigin(), m_vPoint);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Tout changement d'état passe par ici : journal CONTROLE avec le numéro du véhicule et sa distance au point
	protected void SetState(SRP_CheckpointCar car, int state, int now, string note = "")
	{
		car.m_iState = state;
		car.m_iStateTick = now;
		car.m_iStillMs = 0;
		string suffix = "";
		if (!note.IsEmpty())
			suffix = " — " + note;
		Log(string.Format("véhicule n° %1 %2, à %3 m du point (file %4)%5", car.m_iNumber, StateName(state), Math.Round(DistanceOf(car)), car.m_iLane + 1, suffix));
	}

	//------------------------------------------------------------------------------------------------
	protected int CountPlayersWithin(array<IEntity> players, float radius)
	{
		int count = 0;
		foreach (IEntity player : players)
		{
			if (player && vector.Distance(player.GetOrigin(), m_vPoint) <= radius)
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur est-il dans ce véhicule, ou à moins de « distance » mètres ?
	protected bool IsPlayerAt(IEntity entity, array<IEntity> players, float distance)
	{
		if (!entity || entity.IsDeleted())
			return false;
		foreach (IEntity player : players)
		{
			if (!player)
				continue;
			if (player.GetRootParent() == entity)
				return true;
			if (vector.Distance(player.GetOrigin(), entity.GetOrigin()) <= distance)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Boucle (toutes les 2 s)
	//------------------------------------------------------------------------------------------------
	int Update(array<IEntity> players, int now, int dtMs)
	{
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (!civils || !m_bEstablished)
			return SRP_ECheckpointResult.EN_COURS;

		// Le point doit rester tenu
		if (CountPlayersWithin(players, 300) == 0)
		{
			m_iAbsentMs += dtMs;
			if (m_iAbsentMs > 180000)
			{
				Log("point abandonné : 3 min sans joueur à moins de 300 m — " + Report());
				return SRP_ECheckpointResult.ABANDONNE;
			}
		}
		else
		{
			m_iAbsentMs = 0;
		}

		if (now >= m_iEndTick)
		{
			Log("point tenu jusqu'au bout — " + Report());
			return SRP_ECheckpointResult.TENU;
		}

		UpdateLantern();

		if (m_bThreat && now >= m_iThreatTick)
		{
			m_bThreat = false;
			m_bWantWave = true;
		}

		if (!m_bAxisKnown)
			FindAxis(civils);

		Adopt(civils, now);
		MaybeSpawnCar(civils, players, now);
		IssueOrders(civils, now);

		for (int i = m_aCars.Count() - 1; i >= 0; i--)
		{
			SRP_CheckpointCar car = m_aCars[i];
			UpdateCar(civils, car, players, now, dtMs);
			if (car.m_iState == SRP_ECarState.CLOSE)
				m_aCars.Remove(i);
		}

		TickWrecks(civils, players, now, dtMs);
		return SRP_ECheckpointResult.EN_COURS;
	}

	//------------------------------------------------------------------------------------------------
	//! Le point est établi à l'instant où le soldat le décide (SRP_MissionManagerComponent.StartControl)
	void Establish(int now)
	{
		m_bEstablished = true;
		m_iEndTick = now + m_Settings.m_iMinutes * 60000;
		m_iNextCarTick = now + 20000;
		if (Math.RandomInt(0, 100) < m_Settings.m_iThreatPercent)
		{
			m_bThreat = true;
			int latest = Math.Max(6, m_Settings.m_iMinutes - 5);
			m_iThreatTick = now + Math.RandomIntInclusive(5, latest) * 60000;
		}
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (civils)
			FindAxis(civils);
		NotifyNear(string.Format("Point de contrôle établi : tenez-le %1 minutes. Barrez la route avec vos véhicules. Les conducteurs s'arrêtent : regardez-les, action « Contrôler ». Braquez un conducteur descendu pour qu'il lève les mains.", m_Settings.m_iMinutes));
		Log(string.Format("point de contrôle établi pour %1 min, menace prévue : %2, axe de la route connu : %3", m_Settings.m_iMinutes, m_bThreat.ToString(), m_bAxisKnown.ToString()));
	}

	//------------------------------------------------------------------------------------------------
	//! Une lanterne à 4 m du point la nuit (posée par le générateur de base), retirée le jour
	protected void UpdateLantern()
	{
		bool night = SRP_DayNightComponent.IsNight();
		if (night && !m_Lantern)
		{
			vector side = m_vRoadAxis;
			if (!m_bAxisKnown)
				side = "1 0 0";
			vector across = Vector(-side[2], 0, side[0]);
			m_Lantern = SRP_GenerateurComponent.SpawnLantern(m_vPoint + across * 4);
		}
		else if (!night && m_Lantern)
		{
			SRP_GenerateurComponent.RemoveLantern(m_Lantern);
			m_Lantern = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! L'axe de la route au point : il sépare les deux sens d'arrivée (deux files)
	protected void FindAxis(SRP_CivilianManagerComponent civils)
	{
		vector axis;
		if (!civils.FindRoadAxis(m_vPoint, 60, axis))
			return;
		m_vRoadAxis = axis;
		m_bAxisKnown = true;
	}

	//------------------------------------------------------------------------------------------------
	int MinutesLeft(int now)
	{
		if (!m_bEstablished)
			return m_Settings.m_iMinutes;
		return Math.Max(0, (m_iEndTick - now) / 60000);
	}

	//------------------------------------------------------------------------------------------------
	// Arrivée des voitures et files d'attente
	//------------------------------------------------------------------------------------------------
	//! Voitures qui occupent une file : en approche, en attente, à l'arrêt, ou qui repartent sans avoir encore dégagé
	protected bool OccupiesLane(SRP_CheckpointCar car)
	{
		if (car.m_iState == SRP_ECarState.APPROCHE || car.m_iState == SRP_ECarState.EN_ATTENTE || car.m_iState == SRP_ECarState.ARRETEE)
			return true;
		return car.m_iState == SRP_ECarState.REPART && DistanceOf(car) < 25;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountLane(int lane)
	{
		int count = 0;
		foreach (SRP_CheckpointCar car : m_aCars)
		{
			if (car.m_iLane == lane && OccupiesLane(car))
				count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	protected int CountBusy()
	{
		return CountLane(0) + CountLane(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Une voiture civile déjà en route qui se rapproche du point (à moins de 200 m) est prise dans le contrôle
	protected void Adopt(SRP_CivilianManagerComponent civils, int now)
	{
		if (CountBusy() >= 6)
			return;
		array<SRP_CivRecord> found = {};
		civils.CheckpointAdopt(m_vPoint, 200, found);
		foreach (SRP_CivRecord record : found)
			AddCar(civils, record, now, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Une voiture de plus seulement si une file compte moins de 2 voitures
	protected void MaybeSpawnCar(SRP_CivilianManagerComponent civils, array<IEntity> players, int now)
	{
		if (now < m_iNextCarTick)
			return;
		if (CountLane(0) >= 2 && CountLane(1) >= 2)
			return;
		if (CountBusy() >= 4)
			return;
		if (m_iEndTick - now < 90000)
			return;	// trop tard pour une voiture de plus

		// Plus la section est nombreuse, plus la circulation est dense
		int present = CountPlayersWithin(players, 300);
		int seconds = Math.Max(30, m_Settings.m_iCarSeconds - 8 * Math.Max(0, present - 2));
		m_iNextCarTick = now + Math.RandomIntInclusive(seconds * 700, seconds * 1300);

		SRP_CivRecord record = civils.CheckpointSpawnCar(m_vPoint, players, now);
		if (record)
			AddCar(civils, record, now, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void AddCar(SRP_CivilianManagerComponent civils, SRP_CivRecord record, int now, bool ambient)
	{
		SRP_CheckpointCar car = new SRP_CheckpointCar();
		m_iCarNumber++;
		car.m_iNumber = m_iCarNumber;
		car.m_Record = record;
		car.m_iStateTick = now;
		car.m_iAddedTick = now;
		car.m_bAmbient = ambient;
		car.m_iReadyTick = now;
		if (!ambient)
			car.m_iReadyTick = now + 3000;	// le temps que le conducteur soit au volant
		if (record.m_Vehicle)
			car.m_vLast = record.m_Vehicle.GetOrigin();

		// Le sens d'arrivée : d'où elle vient par rapport à l'axe de la route au point
		vector dir = vector.Zero;
		if (record.m_Vehicle)
			dir = m_vPoint - record.m_Vehicle.GetOrigin();
		dir[1] = 0;
		if (dir.Length() < 1)
			dir = m_vRoadAxis;
		else
			dir.Normalize();
		if (m_bAxisKnown)
		{
			if (vector.Dot(dir, m_vRoadAxis) >= 0)
				car.m_vApproachDir = m_vRoadAxis;
			else
			{
				car.m_vApproachDir = m_vRoadAxis * -1;
				car.m_iLane = 1;
			}
		}
		else
		{
			car.m_vApproachDir = dir;
		}

		// Un conducteur sur cinq est suspect ; jamais une mission sans aucun suspect
		bool suspect = Math.RandomInt(0, 100) < m_Settings.m_iSuspectPercent;
		if (!suspect && m_iSuspectsSoFar == 0 && m_iCarNumber >= 6)
			suspect = true;
		if (suspect)
		{
			m_iSuspectsSoFar++;
			RollSuspect(car);
		}
		BuildIdentity(civils, car);
		m_aCars.Insert(car);

		string origin = "posée sur la route";
		if (ambient)
			origin = "circulation ambiante, adoptée en route";
		Log(string.Format("véhicule n° %1 pris dans le contrôle (%2, %3, cargaison %4, réaction %5)", car.m_iNumber, origin, car.m_sName, car.m_iCargo, car.m_iReaction));
		SetState(car, SRP_ECarState.APPROCHE, now);
	}

	//------------------------------------------------------------------------------------------------
	//! Les ordres de route des deux files : la tête de chaque file roule vers le point, les suivantes vers une place
	//! 40 m derrière la précédente ; elles avancent quand la place se libère. Celui qui compte forcer le barrage roule
	//! droit sur le point, sans ordre de file, puis au-delà sans s'arrêter (UpdateForcing).
	protected void IssueOrders(SRP_CivilianManagerComponent civils, int now)
	{
		for (int lane = 0; lane < 2; lane++)
		{
			array<SRP_CheckpointCar> line = {};
			bool headBusy = false;
			foreach (SRP_CheckpointCar car : m_aCars)
			{
				if (car.m_iLane != lane || !car.m_Record || !car.m_Record.m_Vehicle)
					continue;
				if (car.m_iState == SRP_ECarState.ARRETEE || (car.m_iState == SRP_ECarState.REPART && DistanceOf(car) < 25))
				{
					headBusy = true;
					continue;
				}
				if (car.m_iState != SRP_ECarState.APPROCHE && car.m_iState != SRP_ECarState.EN_ATTENTE)
					continue;
				if (now < car.m_iReadyTick)
					continue;
				if (car.m_iReaction == SRP_EReaction.FORCE)
				{
					// Vers le point lui-même : rouler vers sa destination ne garantissait pas qu'elle y passe
					if (car.m_vTarget == vector.Zero)
					{
						civils.CheckpointDriveTo(car.m_Record, m_vPoint);
						car.m_vTarget = m_vPoint;
					}
					continue;
				}
				// Tri par distance au point (insertion)
				float distance = DistanceOf(car);
				int at = line.Count();
				for (int i = 0; i < line.Count(); i++)
				{
					if (distance < DistanceOf(line[i]))
					{
						at = i;
						break;
					}
				}
				if (at >= line.Count())
					line.Insert(car);
				else
					line.InsertAt(car, at);
			}

			int rank = 0;
			if (headBusy)
				rank = 1;
			foreach (SRP_CheckpointCar queued : line)
			{
				vector target = m_vPoint;
				int wanted = SRP_ECarState.APPROCHE;
				if (rank > 0)
				{
					target = m_vPoint - queued.m_vApproachDir * (rank * 40.0);
					target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
					wanted = SRP_ECarState.EN_ATTENTE;
				}
				if (queued.m_iState != wanted)
					SetState(queued, wanted, now, string.Format("place %1", rank + 1));
				if (vector.Distance(queued.m_vTarget, target) > 5)
				{
					civils.CheckpointDriveTo(queued.m_Record, target);
					queued.m_vTarget = target;
					queued.m_bParked = false;
				}
				rank++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RollSuspect(SRP_CheckpointCar car)
	{
		int roll = Math.RandomInt(0, 100);
		if (roll < 40)
			car.m_iCargo = SRP_ECargo.ARMES;
		else if (roll < 65)
			car.m_iCargo = SRP_ECargo.DOCUMENTS;
		else if (roll < 85)
			car.m_iCargo = SRP_ECargo.ENNEMI;
		else
			car.m_iCargo = SRP_ECargo.EXPLOSIFS;

		roll = Math.RandomInt(0, 100);
		if (car.m_iCargo == SRP_ECargo.ENNEMI)
		{
			if (roll < 55)
				car.m_iReaction = SRP_EReaction.SORT_UNE_ARME;
			else if (roll < 75)
				car.m_iReaction = SRP_EReaction.FORCE;
			else
				car.m_iReaction = SRP_EReaction.SE_REND;
			return;
		}
		if (roll < 50)
			car.m_iReaction = SRP_EReaction.SE_REND;
		else if (roll < 75)
			car.m_iReaction = SRP_EReaction.FORCE;
		else
			car.m_iReaction = SRP_EReaction.FUIT_A_PIED;
	}

	//------------------------------------------------------------------------------------------------
	//! Le nom, le trajet annoncé et ce qu'on remarque en regardant les papiers. Un suspect sur deux a des papiers FAUX
	//! (certitude) ; l'autre a des papiers en règle et seulement des indices. Un innocent n'a jamais de faux papiers.
	protected void BuildIdentity(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car)
	{
		array<string> first = {"Jean", "Pierre", "Marc", "Louis", "Henri", "Paul", "André", "Michel", "Jacques", "René", "Émile", "Lucien", "Marcel", "Georges", "Antoine", "Bernard"};
		array<string> last = {"Morel", "Girard", "Lefèvre", "Roussel", "Fontaine", "Chevalier", "Blanchard", "Gauthier", "Perrin", "Marchand", "Dumas", "Carpentier", "Renaud", "Vasseur", "Colin", "Aubert"};
		car.m_sName = first.GetRandomElement() + " " + last.GetRandomElement();

		car.m_sTo = "la prochaine localité";
		if (car.m_Record.m_Destination)
			car.m_sTo = car.m_Record.m_Destination.GetZoneName();
		car.m_sFrom = civils.CheckpointOtherPlace(car.m_sTo);

		if (!car.IsSuspect())
		{
			// Un innocent peut avoir l'air louche : les indices ne sont jamais une preuve
			if (Math.RandomInt(0, 100) < 25)
			{
				array<string> benign = {};
				benign.Insert("Il est nerveux : il dit n'avoir jamais été contrôlé par l'armée.");
				benign.Insert("Il s'agace du contrôle et regarde sa montre : il se dit en retard.");
				benign.Insert("Il transpire et parle vite. Il explique que sa femme l'attend à l'hôpital.");
				benign.Insert("Le coffre est plein de cageots : il dit revenir du marché.");
				car.m_aClues.Insert(benign.GetRandomElement());
			}
			return;
		}

		car.m_bFakePapers = Math.RandomInt(0, 100) < 50;

		array<string> clues = {};
		clues.Insert("Il évite votre regard et ses mains tremblent sur le volant.");
		clues.Insert("Ses papiers sont neufs : le tampon date de la semaine dernière.");
		clues.Insert(string.Format("Il dit aller à %1, mais hésite sur le nom de la route.", car.m_sTo));
		switch (car.m_iCargo)
		{
			case SRP_ECargo.ARMES:
				clues.Insert("L'arrière du véhicule est très bas sur ses amortisseurs.");
				clues.Insert("Une bâche couvre quelque chose à l'arrière.");
				break;
			case SRP_ECargo.EXPLOSIFS:
				clues.Insert("L'arrière du véhicule est très bas sur ses amortisseurs.");
				clues.Insert("Une odeur de produit chimique flotte dans l'habitacle.");
				break;
			case SRP_ECargo.DOCUMENTS:
				clues.Insert("Une sacoche est coincée sous le siège passager.");
				clues.Insert("Il garde une main posée sur la boîte à gants.");
				break;
			case SRP_ECargo.ENNEMI:
				clues.Insert("Il porte des bottes militaires sous son pantalon civil.");
				clues.Insert("Son accent n'est pas d'ici et il répond par monosyllabes.");
				break;
		}

		int count = Math.RandomIntInclusive(1, 2);
		for (int i = 0; i < count && !clues.IsEmpty(); i++)
		{
			int index = Math.RandomInt(0, clues.Count());
			car.m_aClues.Insert(clues[index]);
			clues.Remove(index);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Suivi d'une voiture
	//------------------------------------------------------------------------------------------------
	protected float SpeedOf(IEntity vehicle)
	{
		if (!vehicle || vehicle.IsDeleted())
			return 0;
		Physics physics = vehicle.GetPhysics();
		if (!physics)
			return 0;
		return physics.GetVelocity().Length();
	}

	//------------------------------------------------------------------------------------------------
	//! A-t-elle vraiment passé le point ? Elle doit l'avoir approché (sa trajectoire à moins de 30 m) et rouler
	//! maintenant à plus de « margin » mètres au-delà, dans son sens de marche. Sans la première condition, une
	//! voiture encore loin, sur une route qui tourne ou qui arrive par l'autre bout, passait pour « au-delà » du point
	protected bool PassedPoint(SRP_CheckpointCar car, vector position, float margin)
	{
		if (car.m_fClosest > 30)
			return false;
		vector rel = position - m_vPoint;
		rel[1] = 0;
		return vector.Dot(rel, car.m_vApproachDir) > margin;
	}

	//------------------------------------------------------------------------------------------------
	//! La plus courte distance (à plat) entre le point et le trajet fait depuis le passage précédent (2 s, jusqu'à
	//! 30 m à bonne allure) : une voiture qui traverse le point entre deux passages est vue quand même
	protected float PathDistance(vector from, vector to)
	{
		vector path = to - from;
		path[1] = 0;
		vector toPoint = m_vPoint - from;
		toPoint[1] = 0;
		float length2 = vector.Dot(path, path);
		float t = 0;
		if (length2 > 0.01)
			t = Math.Clamp(vector.Dot(toPoint, path) / length2, 0, 1);
		vector gap = toPoint - path * t;
		gap[1] = 0;
		return gap.Length();
	}

	//------------------------------------------------------------------------------------------------
	//! Le sens d'arrivée suit la voiture tant qu'elle n'a pas approché le point : c'est le côté par lequel elle arrive
	//! vraiment qui compte (la route peut faire le tour et arriver par l'autre bout), pas la ligne droite depuis
	//! l'endroit où elle a été prise. Une marge de 20 m de part et d'autre évite de changer d'avis sans arrêt
	protected void TrackApproach(SRP_CheckpointCar car, vector position)
	{
		if (car.m_fClosest < 30)
			return;	// déjà au point : le sens est figé
		vector rel = position - m_vPoint;
		rel[1] = 0;
		if (!m_bAxisKnown)
		{
			if (rel.Length() > 20)
			{
				vector dir = rel * -1;
				dir.Normalize();
				car.m_vApproachDir = dir;
			}
			return;
		}
		float along = vector.Dot(rel, m_vRoadAxis);
		int lane = car.m_iLane;
		if (along < -20)
			lane = 0;
		else if (along > 20)
			lane = 1;
		// L'axe peut n'être trouvé qu'après sa prise : on s'y aligne dès qu'il est connu
		if (lane == 0)
			car.m_vApproachDir = m_vRoadAxis;
		else
			car.m_vApproachDir = m_vRoadAxis * -1;
		if (lane != car.m_iLane)
		{
			car.m_iLane = lane;
			Log(string.Format("véhicule n° %1 : elle arrive en fait par l'autre bout de la route, file %2", car.m_iNumber, lane + 1));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Sur la route qui mène au point, avant lui : à moins de 25 m de l'axe de la route, du côté d'où elle arrive
	protected bool OnApproach(SRP_CheckpointCar car, vector position)
	{
		vector rel = position - m_vPoint;
		rel[1] = 0;
		if (vector.Dot(rel, car.m_vApproachDir) > 0)
			return false;
		if (!m_bAxisKnown)
			return true;
		vector lateral = rel - m_vRoadAxis * vector.Dot(rel, m_vRoadAxis);
		lateral[1] = 0;
		return lateral.Length() < 25;
	}

	//------------------------------------------------------------------------------------------------
	//! Celui qui force le barrage roule droit sur le point. L'alerte ne part que lorsqu'il y est vraiment : sur la route
	//! du point à moins de 90 m et qui s'en rapproche (ou à moins de 35 m), déjà passé dessus, ou bloqué juste devant
	//! par les véhicules de la section. Avant, elle partait dès 140 m à vol d'oiseau, alors que la voiture pouvait
	//! être encore sur une autre route, ou ne jamais passer par le point
	protected void UpdateForcing(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, vector position, float distance, float wasDistance, float pathGap, int now)
	{
		SRP_CivRecord record = car.m_Record;
		bool blocked = distance < 60 && car.m_iStillMs >= 6000;
		bool closing = distance < wasDistance - 1;
		bool arriving = pathGap < 25 || (closing && (distance < 35 || (distance < 90 && OnApproach(car, position))));
		if (!arriving && !blocked)
		{
			if (now - car.m_iAddedTick > 8 * 60000)
			{
				civils.CheckpointRelease(record);
				SetState(car, SRP_ECarState.CLOSE, now, "jamais arrivée, rendue à la circulation après 8 min");
			}
			return;
		}

		// Elle ne s'arrête pas : ordre de route au-delà du point
		civils.CheckpointGoBeyond(record, Beyond(car, 300), now);
		car.m_bReacted = true;
		if (blocked && !arriving)
		{
			SetState(car, SRP_ECarState.FORCE_LE_BARRAGE, now, "veut forcer, bloquée devant le barrage");
			NotifyNear(string.Format("Le véhicule n° %1 tente de forcer le barrage, mais vos véhicules le bloquent !", car.m_iNumber));
			return;
		}
		SetState(car, SRP_ECarState.FORCE_LE_BARRAGE, now, "arrive sans ralentir");
		NotifyNear(string.Format("Le véhicule n° %1 arrive au point sans ralentir : il force le barrage ! Stoppez-le (pneus, moteur).", car.m_iNumber));
	}

	//------------------------------------------------------------------------------------------------
	//! Le point au-delà du contrôle, dans le sens de marche de cette voiture (destination de départ)
	protected vector Beyond(SRP_CheckpointCar car, float distance)
	{
		vector beyond = m_vPoint + car.m_vApproachDir * distance;
		beyond[1] = GetGame().GetWorld().GetSurfaceY(beyond[0], beyond[2]);
		return beyond;
	}

	//------------------------------------------------------------------------------------------------
	//! Direction de visée d'un joueur (arme ou tête), sinon l'avant du personnage
	protected vector AimDirection(IEntity player)
	{
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(player.FindComponent(CharacterControllerComponent));
		if (controller)
		{
			CharacterAimingComponent aiming = controller.GetAimingComponent();
			if (aiming)
			{
				vector aim = aiming.GetAimingDirectionWorld();
				if (aim.Length() > 0.5)
					return aim;
			}
		}
		return player.GetWorldTransformAxis(2);
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur vivant, à moins de « maxDistance » mètres, vise-t-il ce personnage (angle < 10°) ?
	protected bool IsAimedAt(IEntity target, array<IEntity> players, float maxDistance)
	{
		if (!target || target.IsDeleted())
			return false;
		vector chest = target.GetOrigin() + vector.Up * 1.2;
		foreach (IEntity player : players)
		{
			if (!player || SRP_Utils.IsDead(player))
				continue;
			vector eye = player.GetOrigin() + vector.Up * 1.5;
			vector toTarget = chest - eye;
			float distance = toTarget.Length();
			if (distance > maxDistance || distance < 0.3)
				continue;
			vector aim = AimDirection(player);
			if (aim.Length() < 0.5)
				continue;
			toTarget.Normalize();
			aim.Normalize();
			if (vector.Dot(aim, toTarget) > 0.985)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCar(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, array<IEntity> players, int now, int dtMs)
	{
		if (car.m_iState == SRP_ECarState.CLOSE)
			return;
		SRP_CivRecord record = car.m_Record;
		if (!record)
		{
			SetState(car, SRP_ECarState.CLOSE, now, "fiche perdue");
			return;
		}

		// Un désamorçage en cours (le conducteur peut déjà être arrêté)
		if (car.m_iDefuserId > 0 && now >= car.m_iDefuseEndTick)
			FinishDefuse(car, now);

		// Arrêté : plus rien à suivre, tout part à la fin de la mission (ou avec le véhicule d'explosifs après 10 min)
		if (car.m_iState == SRP_ECarState.CONDUCTEUR_ARRETE)
			return;

		if (!record.m_Vehicle || record.m_Vehicle.IsDeleted())
		{
			civils.CheckpointDiscard(record, true);
			SetState(car, SRP_ECarState.CLOSE, now, "véhicule disparu");
			return;
		}

		// Le conducteur est mort
		if (!record.m_Character || record.m_Character.IsDeleted() || SRP_Utils.IsDead(record.m_Character))
		{
			OnDriverDead(civils, car, now);
			return;
		}

		vector position = record.m_Vehicle.GetOrigin();
		float moved = vector.Distance(position, car.m_vLast);
		float pathGap = PathDistance(car.m_vLast, position);
		if (pathGap < car.m_fClosest)
			car.m_fClosest = pathGap;
		car.m_vLast = position;
		if (moved < 1.5)
			car.m_iStillMs += dtMs;
		else
			car.m_iStillMs = 0;
		float distance = vector.Distance(position, m_vPoint);
		float wasDistance = car.m_fLastDistance;
		car.m_fLastDistance = distance;

		// Menotté par un joueur (serflex ACE) : arrestation si le motif est là, sinon il attend la fouille
		if (!car.m_bCaptive && car.m_iState != SRP_ECarState.REPART && civils.CheckpointIsCaptive(record))
		{
			car.m_bCaptive = true;
			car.m_bSurrendered = false;
			car.m_iSearcherId = -1;
			if (car.m_bFound || car.m_bFakePapers)
			{
				m_iArrests++;
				SetState(car, SRP_ECarState.CONDUCTEUR_ARRETE, now, "menotté par un joueur");
				NotifyNear(string.Format("Conducteur du véhicule n° %1 menotté : arrêté. La prévôté le prendra en charge à la fin de la mission.", car.m_iNumber), 600);
				return;
			}
			NotifyNear(string.Format("Conducteur du véhicule n° %1 menotté sans motif : fouillez le véhicule, ou libérez-le (« Laisser repartir »).", car.m_iNumber), 600);
			Log(string.Format("véhicule n° %1 : conducteur menotté sans motif", car.m_iNumber));
		}

		switch (car.m_iState)
		{
			case SRP_ECarState.APPROCHE:
			{
				TrackApproach(car, position);
				if (car.m_iReaction == SRP_EReaction.FORCE)
				{
					UpdateForcing(civils, car, position, distance, wasDistance, pathGap, now);
					break;
				}

				// Arrivée au point (ou passée dessus depuis le passage précédent), ou bloquée juste avant par les
				// véhicules de la section
				if (distance < 18 || pathGap < 18 || (distance < 60 && car.m_iStillMs >= 6000))
				{
					civils.CheckpointStop(record);
					SetState(car, SRP_ECarState.ARRETEE, now);
					NotifyNear(string.Format("Véhicule n° %1 à l'arrêt au point de contrôle.", car.m_iNumber));
					break;
				}

				// Passée au travers sans s'arrêter (elle a vraiment traversé le point) : elle force le barrage
				if (PassedPoint(car, position, 15) && SpeedOf(record.m_Vehicle) > 2.2)
				{
					car.m_bReacted = true;
					car.m_bThrough = true;
					civils.CheckpointGoBeyond(record, Beyond(car, 1200), now);
					SetState(car, SRP_ECarState.FORCE_LE_BARRAGE, now, "passée au travers sans s'arrêter");
					NotifyNear(string.Format("Le véhicule n° %1 passe le barrage sans s'arrêter : il force le barrage ! Stoppez-le (pneus, moteur).", car.m_iNumber));
					break;
				}

				if (now - car.m_iAddedTick > 8 * 60000)
				{
					// Jamais arrivée (coincée en route) : on la rend à la circulation
					civils.CheckpointRelease(record);
					SetState(car, SRP_ECarState.CLOSE, now, "jamais arrivée, rendue à la circulation après 8 min");
				}
				break;
			}

			case SRP_ECarState.EN_ATTENTE:
			{
				TrackApproach(car, position);
				if (PassedPoint(car, position, 15) && SpeedOf(record.m_Vehicle) > 2.2)
				{
					car.m_bReacted = true;
					car.m_bThrough = true;
					civils.CheckpointGoBeyond(record, Beyond(car, 1200), now);
					SetState(car, SRP_ECarState.FORCE_LE_BARRAGE, now, "sortie de la file et passée au travers");
					NotifyNear(string.Format("Le véhicule n° %1 sort de la file et force le barrage ! Stoppez-le (pneus, moteur).", car.m_iNumber));
					break;
				}
				float toSlot = vector.Distance(position, car.m_vTarget);
				if (!car.m_bParked && (toSlot < 12 || (toSlot < 50 && car.m_iStillMs >= 6000)))
				{
					civils.CheckpointStop(record);
					car.m_bParked = true;
					Log(string.Format("véhicule n° %1 à l'arrêt dans la file, à %2 m du point", car.m_iNumber, Math.Round(distance)));
				}
				else if (now - car.m_iAddedTick > 8 * 60000)
				{
					civils.CheckpointRelease(record);
					SetState(car, SRP_ECarState.CLOSE, now, "trop longtemps dans la file, rendue à la circulation après 8 min");
				}
				break;
			}

			case SRP_ECarState.ARRETEE:
			{
				if (car.m_iSearcherId > 0)
				{
					if (now >= car.m_iSearchEndTick)
						FinishSearch(civils, car, now);
					break;
				}
				if (car.m_bOut && !car.m_bCaptive)
				{
					bool aimed = IsAimedAt(record.m_Character, players, 15);
					if (aimed && !car.m_bSurrendered)
					{
						if (civils.CheckpointSurrender(record, true))
						{
							car.m_bSurrendered = true;
							Log(string.Format("véhicule n° %1 : conducteur braqué, il lève les mains", car.m_iNumber));
							NotifyNear(string.Format("Le conducteur du véhicule n° %1 lève les mains : vous pouvez le menotter (serflex) ou l'arrêter (action « Contrôler »).", car.m_iNumber));
						}
					}
					else if (!aimed && !car.m_bSurrendered && !car.m_bReacted && car.m_iReaction == SRP_EReaction.FUIT_A_PIED && now - car.m_iOutTick > 6000)
					{
						Flee(civils, car, now);
						break;
					}
				}
				// Personne ne s'occupe de lui : il finit par repartir
				if (now - car.m_iStateTick > 4 * 60000 && !car.m_bFound && !car.m_bCaptive)
				{
					NotifyNear(string.Format("Le véhicule n° %1 repart : personne ne l'a contrôlé.", car.m_iNumber));
					Release(civils, car, now);
				}
				break;
			}

			case SRP_ECarState.REPART:
			{
				UpdateLeaving(civils, car, position, distance, now);
				break;
			}

			case SRP_ECarState.FORCE_LE_BARRAGE:
			{
				if (car.m_iCargo == SRP_ECargo.EXPLOSIFS && IsDestroyed(record.m_Vehicle))
				{
					Blast(position);
					NotifyNear(string.Format("Le véhicule n° %1 était chargé d'explosifs !", car.m_iNumber), 600);
					CountSeizure(car, true);
					m_iKilled++;
					civils.CheckpointDiscard(record, true);
					SetState(car, SRP_ECarState.CLOSE, now, "explosifs détonés");
					break;
				}
				// Le point passé, elle s'en éloigne (dans un sens ou dans l'autre : à un carrefour, elle peut tourner) : ordre
				// de filer au loin de ce côté. Sans nouvel ordre, elle s'arrêterait 300 m plus loin et se rendrait
				if (!car.m_bThrough && car.m_fClosest < 30 && distance > 60 && distance > wasDistance)
				{
					vector away = position - m_vPoint;
					away[1] = 0;
					away.Normalize();
					car.m_vApproachDir = away;
					car.m_bThrough = true;
					civils.CheckpointGoBeyond(record, Beyond(car, 1200), now);
					Log(string.Format("véhicule n° %1 : barrage passé, elle file au loin", car.m_iNumber));
				}
				if (distance > 900)
				{
					m_iMissed++;
					if (car.m_fClosest > 40)
					{
						NotifyNear(string.Format("Le véhicule n° %1 a fait demi-tour avant le barrage et s'est échappé.", car.m_iNumber), 1000);
						civils.CheckpointRelease(record);
						SetState(car, SRP_ECarState.CLOSE, now, "échappé sans passer le point");
						break;
					}
					NotifyNear(string.Format("Le véhicule n° %1 a forcé le barrage et s'est échappé.", car.m_iNumber), 1000);
					civils.CheckpointRelease(record);
					SetState(car, SRP_ECarState.CLOSE, now, "échappé, passé au travers");
					break;
				}
				if (now - car.m_iStateTick > 8000 && (car.m_iStillMs >= 10000 || !EngineWorks(record.m_Vehicle)))
				{
					// Stoppé : il se rend
					civils.CheckpointStop(record);
					civils.CheckpointGetOut(record);
					car.m_bOut = true;
					car.m_iOutTick = now;
					car.m_bFound = true;
					CountSeizure(car, false);
					SetState(car, SRP_ECarState.ARRETEE, now, "stoppé, il se rend");
					NotifyNear(string.Format("Véhicule n° %1 stoppé : le conducteur se rend. Arrêtez-le (action « Contrôler » ou serflex). %2", car.m_iNumber, CargoText(car)), 1000);
				}
				break;
			}

			case SRP_ECarState.A_PIED:
			{
				vector runner = record.m_Character.GetOrigin();
				bool hurt = civils.CheckpointIsHurt(record);
				bool caught = SRP_Utils.NearestPlayerDistance(runner, players) < 4;
				bool aimed = IsAimedAt(record.m_Character, players, 15);
				if (hurt || caught || aimed)
				{
					string why = "braqué";
					if (hurt)
						why = "blessé";
					else if (caught)
						why = "rattrapé";
					civils.CheckpointSurrender(record, true);
					car.m_bSurrendered = true;
					SetState(car, SRP_ECarState.RENDU, now, why);
					if (hurt)
						NotifyNear(string.Format("Le conducteur du véhicule n° %1 est touché : il s'arrête. Soignez-le, puis menottez-le (serflex) ou arrêtez-le (action « Contrôler »).", car.m_iNumber), 800);
					else
						NotifyNear(string.Format("Le conducteur du véhicule n° %1 s'arrête et lève les mains (%2). Menottez-le (serflex) ou arrêtez-le (action « Contrôler »).", car.m_iNumber, why), 800);
				}
				else if (vector.Distance(runner, m_vPoint) > 500 || now - car.m_iStateTick > 4 * 60000)
				{
					NotifyNear(string.Format("Le conducteur du véhicule n° %1 s'est échappé à pied. Son véhicule reste saisi.", car.m_iNumber), 800);
					civils.CheckpointForgetDriver(record);
					SetState(car, SRP_ECarState.CONDUCTEUR_ARRETE, now, "échappé à pied, véhicule saisi");
					AddWreck(car, "conducteur échappé à pied");
				}
				break;
			}

			case SRP_ECarState.RENDU:
			{
				// Il reste sur place, mains levées, jusqu'à ce qu'on le menotte (serflex ou menu). S'il n'a pas encore
				// levé les mains (descendu de voiture pas fini, inconscient), on réessaie
				if (!car.m_bSurrendered || !civils.CheckpointHasSurrendered(record))
				{
					if (civils.CheckpointSurrender(record, true))
						car.m_bSurrendered = true;
				}
				break;
			}

			case SRP_ECarState.HOSTILE:
			{
				if (now - car.m_iStateTick > 6 * 60000)
				{
					civils.CheckpointDiscard(record, true);
					SetState(car, SRP_ECarState.CLOSE, now, "hostile expiré, véhicule laissé sur place");
					AddWreck(car, "hostile expiré");
				}
				break;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Repartir : d'abord remonter en voiture (ordre d'embarquement IA, relancé toutes les 20 s, téléportation au bout
	//! de 60 s), puis rouler au-delà du point ; rendue à la circulation une fois dégagée
	protected void UpdateLeaving(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, vector position, float distance, int now)
	{
		SRP_CivRecord record = car.m_Record;
		if (!car.m_bDriving)
		{
			if (civils.CheckpointIsAtWheel(record))
			{
				civils.CheckpointGoBeyond(record, Beyond(car, 300), now);
				car.m_bDriving = true;
				car.m_iOrderTick = now;
				car.m_iStillMs = 0;
				Log(string.Format("véhicule n° %1 : conducteur au volant, ordre de route au-delà du point", car.m_iNumber));
			}
			else if (now - car.m_iStateTick > 60000)
			{
				civils.CheckpointForceIn(record);
				civils.CheckpointGoBeyond(record, Beyond(car, 300), now);
				car.m_bDriving = true;
				car.m_iOrderTick = now;
				car.m_iStillMs = 0;
				Log(string.Format("véhicule n° %1 : le conducteur n'est pas remonté en 60 s, placé au volant d'office", car.m_iNumber));
			}
			else if (now - car.m_iOrderTick > 20000)
			{
				civils.CheckpointAskGetIn(record, m_Settings.m_sGetInWaypointPrefab);
				car.m_iOrderTick = now;
				Log(string.Format("véhicule n° %1 : ordre d'embarquement relancé", car.m_iNumber));
			}
			return;
		}

		vector rel = position - m_vPoint;
		rel[1] = 0;
		if (distance > 120 || vector.Dot(rel, car.m_vApproachDir) > 60)
		{
			civils.CheckpointRelease(record);
			SetState(car, SRP_ECarState.CLOSE, now, "rendue à la circulation");
			return;
		}
		if (car.m_iStillMs >= 20000)
		{
			car.m_iStillMs = 0;
			car.m_iRetries++;
			if (car.m_iRetries >= 3)
			{
				civils.CheckpointRelease(record);
				SetState(car, SRP_ECarState.CLOSE, now, "ne démarre pas, rendue à la circulation (l'ambiance la retirera si elle reste bloquée)");
				return;
			}
			civils.CheckpointGoBeyond(record, Beyond(car, 300), now);
			Log(string.Format("véhicule n° %1 : immobile 20 s, ordre de route relancé (%2)", car.m_iNumber, car.m_iRetries));
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDriverDead(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, int now)
	{
		if (car.m_iState == SRP_ECarState.FORCE_LE_BARRAGE || car.m_iState == SRP_ECarState.HOSTILE || car.m_iState == SRP_ECarState.A_PIED)
		{
			m_iKilled++;
			CountSeizure(car, false);
			if (car.m_iState == SRP_ECarState.HOSTILE)
				NotifyNear(string.Format("L'ennemi en civil du véhicule n° %1 est neutralisé.", car.m_iNumber), 600);
			else
				NotifyNear(string.Format("Le conducteur du véhicule n° %1 est abattu. %2", car.m_iNumber, CargoText(car)), 600);
		}
		civils.CheckpointDiscard(car.m_Record, true);
		SetState(car, SRP_ECarState.CLOSE, now, "conducteur mort, véhicule laissé sur place");
		AddWreck(car, "conducteur mort");
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsDestroyed(IEntity vehicle)
	{
		DamageManagerComponent damage = DamageManagerComponent.Cast(vehicle.FindComponent(DamageManagerComponent));
		return damage && damage.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	protected bool EngineWorks(IEntity vehicle)
	{
		SCR_VehicleDamageManagerComponent damage = SCR_VehicleDamageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleDamageManagerComponent));
		if (!damage)
			return true;
		return damage.GetEngineFunctional() && !damage.IsDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	//! Un obus explosif lâché sur place : l'explosion du chargement
	protected void Blast(vector position)
	{
		if (m_Settings.m_sBlastPrefab.IsEmpty())
			return;
		Resource resource = Resource.Load(m_Settings.m_sBlastPrefab);
		if (!resource || !resource.IsValid())
			return;
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		Math3D.MatrixIdentity4(params.Transform);
		params.Transform[3] = position + vector.Up * 4;
		IEntity shell = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (!shell)
			return;
		ProjectileMoveComponent move = ProjectileMoveComponent.Cast(shell.FindComponent(ProjectileMoveComponent));
		if (move)
			move.Launch(Vector(0, -1, 0), vector.Zero, 0.2, shell, null, null, null, null);
	}

	//------------------------------------------------------------------------------------------------
	//! Une saisie de plus. Des explosifs ne comptent qu'une fois désamorcés (ou détonés : force = true)
	protected void CountSeizure(SRP_CheckpointCar car, bool force)
	{
		if (car.m_bSeizureCounted || !car.IsSuspect())
			return;
		if (car.m_iCargo == SRP_ECargo.EXPLOSIFS && !car.m_bDefused && !force)
			return;
		car.m_bSeizureCounted = true;
		m_iSeizures++;
		if (car.m_iCargo == SRP_ECargo.DOCUMENTS)
			m_bIntel = true;
	}

	//------------------------------------------------------------------------------------------------
	protected string CargoText(SRP_CheckpointCar car)
	{
		switch (car.m_iCargo)
		{
			case SRP_ECargo.ARMES: return "Des armes et des munitions sous une bâche : saisies.";
			case SRP_ECargo.DOCUMENTS: return "Une sacoche de documents ennemis : saisie, le renseignement sera transmis en fin de mission.";
			case SRP_ECargo.ENNEMI: return "Un pistolet et une tenue ennemie sous le siège : c'est un soldat ennemi en civil.";
			case SRP_ECargo.EXPLOSIFS:
			{
				if (car.m_bDefused)
					return "Le coffre était chargé d'explosifs : désamorcés, véhicule inoffensif.";
				return string.Format("Le coffre est chargé d'explosifs ! Véhicule immobilisé, écartez tout le monde : un sapeur doit le désamorcer (action « Contrôler » → Désamorcer, %1 s).", m_Settings.m_iDefuseSeconds);
			}
		}
		return "Rien à signaler.";
	}

	//------------------------------------------------------------------------------------------------
	// Réactions d'un suspect
	//------------------------------------------------------------------------------------------------
	//! L'ennemi en civil sort son arme à cet ordre (descendre, fouille) ? Si oui, l'ordre tombe. La fuite à pied, elle,
	//! ne part plus sur un ordre : seulement quand le conducteur est descendu et que personne ne le braque (UpdateCar)
	protected bool React(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, int now)
	{
		if (car.m_bReacted || !car.IsSuspect() || car.m_bCaptive)
			return false;
		if (car.m_iReaction != SRP_EReaction.SORT_UNE_ARME)
			return false;

		car.m_bReacted = true;
		car.m_iSearcherId = -1;
		if (car.m_bSurrendered)
		{
			civils.CheckpointSurrender(car.m_Record, false);	// il baisse les mains… pour saisir son arme
			car.m_bSurrendered = false;
		}
		if (!car.m_bOut)
			civils.CheckpointGetOut(car.m_Record);
		car.m_bOut = true;
		car.m_iOutTick = now;

		SetState(car, SRP_ECarState.HOSTILE, now);
		NotifyNear(string.Format("ATTENTION : le conducteur du véhicule n° %1 sort une arme !", car.m_iNumber));
		GetGame().GetCallqueue().CallLater(TurnHostile, 1500, false, car);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Personne ne le braque : il détale
	protected void Flee(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, int now)
	{
		car.m_bReacted = true;
		car.m_iSearcherId = -1;
		car.m_bFound = true;
		CountSeizure(car, false);
		SetState(car, SRP_ECarState.A_PIED, now, "personne ne le braquait");
		NotifyNear(string.Format("Le conducteur du véhicule n° %1 s'enfuit à pied ! Rattrapez-le ou braquez-le : il s'arrêtera. Son véhicule est saisi.", car.m_iNumber));
		RunAway(car);
	}

	//------------------------------------------------------------------------------------------------
	protected void RunAway(SRP_CheckpointCar car)
	{
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (!civils || !car || !car.m_Record || !car.m_Record.m_Character)
			return;
		float angle = Math.RandomFloat(0, Math.PI2);
		vector target = m_vPoint + Vector(Math.Cos(angle) * 700, 0, Math.Sin(angle) * 700);
		target[1] = GetGame().GetWorld().GetSurfaceY(target[0], target[2]);
		civils.CheckpointRunTo(car.m_Record, target);
	}

	//------------------------------------------------------------------------------------------------
	protected void TurnHostile(SRP_CheckpointCar car)
	{
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (!civils || !car || !car.m_Record || !car.m_Record.m_Character)
			return;
		civils.CheckpointMakeHostile(car.m_Record, m_Settings.m_sEnemyFactionKey, m_Settings.m_sPistolPrefab, m_Settings.m_sMagazinePrefab);
	}

	//------------------------------------------------------------------------------------------------
	// Fouille et désamorçage
	//------------------------------------------------------------------------------------------------
	protected void FinishSearch(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, int now)
	{
		int searcherId = car.m_iSearcherId;
		car.m_iSearcherId = -1;
		car.m_iStateTick = now;

		IEntity searcher = GetGame().GetPlayerManager().GetPlayerControlledEntity(searcherId);
		if (!searcher || vector.Distance(searcher.GetOrigin(), car.m_Record.m_Vehicle.GetOrigin()) > 7)
		{
			SRP_Utils.NotifyPlayer(searcherId, string.Format("Fouille du véhicule n° %1 interrompue : vous vous êtes éloigné.", car.m_iNumber));
			Log(string.Format("véhicule n° %1 : fouille interrompue", car.m_iNumber));
			return;
		}

		car.m_bSearched = true;
		if (car.IsSuspect())
		{
			car.m_bFound = true;
			CountSeizure(car, false);
			Log(string.Format("véhicule n° %1 : fouille positive, cargaison %2", car.m_iNumber, car.m_iCargo));
			if (car.m_bCaptive)
			{
				m_iArrests++;
				NotifyNear(string.Format("Fouille du véhicule n° %1 : %2 Le conducteur, déjà menotté, est arrêté.", car.m_iNumber, CargoText(car)));
				SetState(car, SRP_ECarState.CONDUCTEUR_ARRETE, now, "cargaison trouvée, déjà menotté");
				if (car.m_iCargo == SRP_ECargo.EXPLOSIFS)
					SendMenu(searcherId, car);
				return;
			}
			NotifyNear(string.Format("Fouille du véhicule n° %1 : %2 Arrêtez le conducteur.", car.m_iNumber, CargoText(car)));
			// Celui qui comptait fuir descend : il détalera si personne ne le braque
			if (car.m_iReaction == SRP_EReaction.FUIT_A_PIED && !car.m_bOut)
			{
				civils.CheckpointGetOut(car.m_Record);
				car.m_bOut = true;
				car.m_iOutTick = now;
			}
		}
		else
		{
			m_iUseless++;
			Log(string.Format("véhicule n° %1 : fouille négative", car.m_iNumber));
			SRP_Utils.NotifyPlayer(searcherId, string.Format("Fouille du véhicule n° %1 : rien à signaler. Le conducteur peut repartir.", car.m_iNumber));
		}
		SendMenu(searcherId, car);
	}

	//------------------------------------------------------------------------------------------------
	protected void FinishDefuse(SRP_CheckpointCar car, int now)
	{
		int defuserId = car.m_iDefuserId;
		car.m_iDefuserId = -1;
		IEntity defuser = GetGame().GetPlayerManager().GetPlayerControlledEntity(defuserId);
		if (!defuser || !car.m_Record || !car.m_Record.m_Vehicle || vector.Distance(defuser.GetOrigin(), car.m_Record.m_Vehicle.GetOrigin()) > 7)
		{
			SRP_Utils.NotifyPlayer(defuserId, string.Format("Désamorçage du véhicule n° %1 interrompu : vous vous êtes éloigné.", car.m_iNumber));
			Log(string.Format("véhicule n° %1 : désamorçage interrompu", car.m_iNumber));
			return;
		}
		car.m_bDefused = true;
		CountSeizure(car, false);
		Log(string.Format("véhicule n° %1 : explosifs désamorcés, saisie comptée", car.m_iNumber));
		NotifyNear(string.Format("Véhicule n° %1 : chargement d'explosifs désamorcé. Saisie comptée.", car.m_iNumber), 600);
	}

	//------------------------------------------------------------------------------------------------
	//! Sapeur (certification Génie) ou Staff, à côté du véhicule, 30 s sans s'éloigner
	protected string StartDefuse(int userId, SRP_CheckpointCar car, int now)
	{
		if (car.m_iCargo != SRP_ECargo.EXPLOSIFS || !car.m_bFound)
			return "Ce véhicule ne transporte pas d'explosifs connus : fouillez-le d'abord";
		if (car.m_bDefused)
			return "Ce chargement est déjà désamorcé";
		if (car.m_iDefuserId > 0)
			return "Un désamorçage est déjà en cours";
		IEntity user = GetGame().GetPlayerManager().GetPlayerControlledEntity(userId);
		if (!user || !car.m_Record || !car.m_Record.m_Vehicle)
			return "Désamorçage impossible";
		if (vector.Distance(user.GetOrigin(), car.m_Record.m_Vehicle.GetOrigin()) > 6)
			return "Approchez-vous du véhicule pour désamorcer le chargement";
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		SRP_PlayerRecord record;
		if (players)
			record = players.GetRecord(userId);
		bool trained = record && (record.m_bStaff || SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.GENIE));
		if (!trained)
			return "Seul un sapeur (certification Génie) peut désamorcer ce chargement";
		car.m_iDefuserId = userId;
		car.m_iDefuseEndTick = now + m_Settings.m_iDefuseSeconds * 1000;
		Log(string.Format("véhicule n° %1 : désamorçage commencé", car.m_iNumber));
		return string.Format("Désamorçage du véhicule n° %1 : %2 s, restez à côté", car.m_iNumber, m_Settings.m_iDefuseSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! Depuis une action posée sur le véhicule (SRP_CheckpointDefuseAction)
	static string DefuseVehicle(int userId, IEntity vehicle)
	{
		SRP_Checkpoint owner;
		SRP_CheckpointCar car = FindCarOfVehicle(vehicle, owner);
		if (!car)
			return "Ce véhicule n'est pas un véhicule du contrôle routier";
		return owner.StartDefuse(userId, car, System.GetTickCount());
	}

	//------------------------------------------------------------------------------------------------
	protected void Release(SRP_CivilianManagerComponent civils, SRP_CheckpointCar car, int now)
	{
		if (car.IsSuspect())
			m_iMissed++;
		else
			m_iChecked++;
		if (car.m_bCaptive)
			civils.CheckpointCaptive(car.m_Record, false);
		if (car.m_bSurrendered)
			civils.CheckpointSurrender(car.m_Record, false);
		car.m_bCaptive = false;
		car.m_bSurrendered = false;
		car.m_iSearcherId = -1;
		car.m_bDriving = false;
		car.m_iRetries = 0;
		car.m_iOrderTick = now;
		SetState(car, SRP_ECarState.REPART, now);
		if (car.m_bOut)
			civils.CheckpointAskGetIn(car.m_Record, m_Settings.m_sGetInWaypointPrefab);
		else
		{
			civils.CheckpointGoBeyond(car.m_Record, Beyond(car, 300), now);
			car.m_bDriving = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	// Menu « Contrôler »
	//------------------------------------------------------------------------------------------------
	//! Appelé par l'action au contact d'un personnage qui n'est pas un joueur. false = ce n'est pas un conducteur contrôlé
	static bool OpenFor(int userId, IEntity character)
	{
		SRP_Checkpoint owner;
		SRP_CheckpointCar car = FindCarOf(character, owner);
		if (!car)
			return false;
		bool explosives = car.m_iCargo == SRP_ECargo.EXPLOSIFS && car.m_bFound && !car.m_bDefused;
		if (car.m_iState == SRP_ECarState.ARRETEE || car.m_iState == SRP_ECarState.RENDU || (car.m_iState == SRP_ECarState.CONDUCTEUR_ARRETE && explosives))
		{
			owner.SendMenu(userId, car);
			return true;
		}
		if (car.m_iState == SRP_ECarState.CONDUCTEUR_ARRETE)
			SRP_Utils.NotifyPlayer(userId, "Ce conducteur est arrêté : la prévôté le prendra en charge à la fin de la mission.");
		else if (car.m_iState == SRP_ECarState.A_PIED)
			SRP_Utils.NotifyPlayer(userId, "Ce conducteur fuit : braquez-le ou rattrapez-le, il s'arrêtera.");
		else
			SRP_Utils.NotifyPlayer(userId, "Ce véhicule n'est pas à l'arrêt au point de contrôle.");
		return true;
	}

	//------------------------------------------------------------------------------------------------
	void SendMenu(int userId, SRP_CheckpointCar car)
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(userId));
		if (pc)
			pc.SRP_OpenControl(BuildMenu(car));
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildMenu(SRP_CheckpointCar car)
	{
		string key = string.Format("%1:%2:", m_sMissionId, car.m_iNumber);
		string text = string.Format("M|Véhicule n° %1 — point de contrôle %2", car.m_iNumber, m_sMissionId);

		if (car.m_bPapers)
		{
			text += "\nH|Papiers";
			text += string.Format("\nL|%1, vient de %2, dit aller à %3", car.m_sName, car.m_sFrom, car.m_sTo);
			if (car.m_bFakePapers)
				text += "\nC|r|Papiers FAUX : le tampon est grossier et la photo ne lui ressemble pas. Suspect certain.";
			else if (car.m_aClues.IsEmpty())
				text += "\nL|Papiers en règle, attitude calme. Rien de particulier.";
			foreach (string clue : car.m_aClues)
				text += "\nC|y|" + clue;
		}
		if (car.m_bSearched || car.m_bFound)
		{
			text += "\nH|Véhicule";
			if (car.m_bFound)
				text += "\nC|r|" + CargoText(car);
			else
				text += "\nC|g|Fouillé : rien à signaler.";
		}
		if (car.m_bCaptive || car.m_bSurrendered || car.m_iState == SRP_ECarState.RENDU)
		{
			text += "\nH|Conducteur";
			if (car.m_bCaptive)
				text += "\nL|Menotté.";
			else
				text += "\nL|Mains levées.";
		}

		text += "\nH|Ordres";
		if (car.m_iSearcherId > 0)
		{
			text += "\nL|Fouille en cours…";
			return text;
		}
		if (car.m_iDefuserId > 0)
		{
			text += "\nL|Désamorçage en cours…";
			return text;
		}
		bool explosives = car.m_iCargo == SRP_ECargo.EXPLOSIFS && car.m_bFound && !car.m_bDefused;
		if (car.m_iState == SRP_ECarState.CONDUCTEUR_ARRETE)
		{
			if (explosives)
				text += string.Format("\nI|%1desamorcer|Désamorcer le chargement d'explosifs (%2 s, sapeur)", key, m_Settings.m_iDefuseSeconds);
			return text;
		}
		if (!car.m_bPapers)
			text += "\nI|" + key + "papiers|Demander les papiers";
		if (!car.m_bOut && !car.m_bCaptive)
			text += "\nI|" + key + "descendre|Faire descendre le conducteur";
		if (!car.m_bSearched && !car.m_bFound && car.m_iState == SRP_ECarState.ARRETEE)
			text += string.Format("\nI|%1fouiller|Fouiller le véhicule (%2 s, restez à côté)", key, m_Settings.m_iSearchSeconds);
		if (explosives)
			text += string.Format("\nI|%1desamorcer|Désamorcer le chargement d'explosifs (%2 s, sapeur)", key, m_Settings.m_iDefuseSeconds);
		if (car.m_bFound || car.m_bFakePapers)
			text += "\nI|" + key + "arreter|Arrêter le conducteur";
		else
			text += "\nI|" + key + "repartir|Laisser repartir";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne du menu choisie (commande « controle », clé mission:numéro:ordre)
	static string ExecuteMenu(int userId, string key)
	{
		array<string> parts = {};
		key.Split(":", parts, false);
		if (parts.Count() < 3)
			return "Ordre de contrôle illisible";

		SRP_Checkpoint checkpoint = FindByMission(parts[0]);
		if (!checkpoint)
			return "Ce point de contrôle n'existe plus";
		SRP_CheckpointCar car = checkpoint.FindCar(parts[1].ToInt());
		if (!car)
			return "Ce véhicule n'est plus suivi";
		bool stopped = car.m_iState == SRP_ECarState.ARRETEE || car.m_iState == SRP_ECarState.RENDU;
		if (!stopped && !(car.m_iState == SRP_ECarState.CONDUCTEUR_ARRETE && parts[2] == "desamorcer"))
			return "Ce véhicule n'est plus à l'arrêt";
		return checkpoint.Order(userId, car, parts[2]);
	}

	//------------------------------------------------------------------------------------------------
	string Order(int userId, SRP_CheckpointCar car, string order)
	{
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		IEntity user = GetGame().GetPlayerManager().GetPlayerControlledEntity(userId);
		if (!civils || !user || !car.m_Record || !car.m_Record.m_Vehicle)
			return "Contrôle impossible";
		// Le conducteur peut être loin de sa voiture (fuyard rendu) : on ordonne au contact de l'un ou de l'autre
		float toVehicle = vector.Distance(user.GetOrigin(), car.m_Record.m_Vehicle.GetOrigin());
		float toDriver = toVehicle;
		if (car.m_Record.m_Character && !car.m_Record.m_Character.IsDeleted())
			toDriver = vector.Distance(user.GetOrigin(), car.m_Record.m_Character.GetOrigin());
		if (toVehicle > 12 && toDriver > 12)
			return "Vous êtes trop loin du véhicule et de son conducteur";
		if (car.m_iSearcherId > 0)
			return "Une fouille est en cours sur ce véhicule";
		if (car.m_iDefuserId > 0)
			return "Un désamorçage est en cours sur ce véhicule";

		int now = System.GetTickCount();
		car.m_iStateTick = now;

		if (order == "desamorcer")
			return StartDefuse(userId, car, now);

		if (car.m_iState == SRP_ECarState.CONDUCTEUR_ARRETE)
			return "Ce conducteur est arrêté";

		if (order == "papiers")
		{
			car.m_bPapers = true;
			Log(string.Format("véhicule n° %1 : papiers demandés", car.m_iNumber));
			GetGame().GetCallqueue().CallLater(SendMenu, 300, false, userId, car);
			return "";
		}

		if (order == "descendre")
		{
			if (car.m_bCaptive)
				return "Le conducteur est menotté";
			if (React(civils, car, now))
				return "";
			if (!car.m_bOut)
			{
				civils.CheckpointGetOut(car.m_Record);
				car.m_bOut = true;
				car.m_iOutTick = now;
				Log(string.Format("véhicule n° %1 : conducteur descendu sur ordre", car.m_iNumber));
			}
			GetGame().GetCallqueue().CallLater(SendMenu, 300, false, userId, car);
			return "Le conducteur descend et attend à côté de son véhicule. Braquez-le pour qu'il lève les mains.";
		}

		if (order == "fouiller")
		{
			if (car.m_iState != SRP_ECarState.ARRETEE)
				return "Le véhicule se fouille au point de contrôle";
			if (toVehicle > 6)
				return "Approchez-vous du véhicule pour le fouiller";
			if (React(civils, car, now))
				return "";
			car.m_iSearcherId = userId;
			car.m_iSearchEndTick = now + m_Settings.m_iSearchSeconds * 1000;
			Log(string.Format("véhicule n° %1 : fouille commencée", car.m_iNumber));
			return string.Format("Fouille du véhicule n° %1 : %2 s, restez à côté", car.m_iNumber, m_Settings.m_iSearchSeconds);
		}

		if (order == "repartir")
		{
			if (car.m_bFound)
				return "Ce conducteur transportait du matériel interdit : arrêtez-le";
			if (car.m_iState != SRP_ECarState.ARRETEE)
				return "Ce conducteur s'est rendu : arrêtez-le";
			Release(civils, car, now);
			return string.Format("Le véhicule n° %1 repart", car.m_iNumber);
		}

		if (order == "arreter")
		{
			if (!car.m_bFound && !car.m_bFakePapers)
				return "Rien ne justifie une arrestation : fouillez d'abord le véhicule";
			car.m_iSearcherId = -1;
			if (!car.m_bOut)
			{
				civils.CheckpointGetOut(car.m_Record);
				car.m_bOut = true;
				car.m_iOutTick = now;
				GetGame().GetCallqueue().CallLater(MakeCaptive, 3000, false, car);
			}
			else
			{
				MakeCaptive(car);
			}
			car.m_bCaptive = true;
			m_iArrests++;
			SetState(car, SRP_ECarState.CONDUCTEUR_ARRETE, now, "arrêté sur ordre");
			NotifyNear(string.Format("Conducteur du véhicule n° %1 arrêté. La prévôté le prendra en charge à la fin de la mission.", car.m_iNumber));
			if (car.m_iCargo == SRP_ECargo.EXPLOSIFS && car.m_bFound && !car.m_bDefused)
			{
				AddWreck(car, "chargement d'explosifs immobilisé");
				GetGame().GetCallqueue().CallLater(SendMenu, 300, false, userId, car);
			}
			return "";
		}

		return "Ordre inconnu";
	}

	//------------------------------------------------------------------------------------------------
	protected void MakeCaptive(SRP_CheckpointCar car)
	{
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		if (civils && car && car.m_Record)
			civils.CheckpointCaptive(car.m_Record, true);
	}

	//------------------------------------------------------------------------------------------------
	// Véhicules laissés sur place
	//------------------------------------------------------------------------------------------------
	protected void AddWreck(SRP_CheckpointCar car, string why)
	{
		if (!car || !car.m_Record)
			return;
		IEntity vehicle = car.m_Record.m_Vehicle;
		if (!vehicle || vehicle.IsDeleted())
			return;
		foreach (SRP_CheckpointWreck known : m_aWrecks)
		{
			if (known.m_Vehicle == vehicle)
				return;
		}
		SRP_CheckpointWreck wreck = new SRP_CheckpointWreck();
		wreck.m_Vehicle = vehicle;
		wreck.m_iNumber = car.m_iNumber;
		wreck.m_sWhy = why;
		if (car.m_iState != SRP_ECarState.CLOSE)
			wreck.m_Car = car;
		m_aWrecks.Insert(wreck);
		Log(string.Format("véhicule n° %1 laissé sur place (%2) : retiré après 10 min sans joueur à moins de 100 m", car.m_iNumber, why));
	}

	//------------------------------------------------------------------------------------------------
	protected void TickWrecks(SRP_CivilianManagerComponent civils, array<IEntity> players, int now, int dtMs)
	{
		for (int i = m_aWrecks.Count() - 1; i >= 0; i--)
		{
			SRP_CheckpointWreck wreck = m_aWrecks[i];
			if (!wreck.m_Vehicle || wreck.m_Vehicle.IsDeleted())
			{
				m_aWrecks.Remove(i);
				continue;
			}
			if (IsPlayerAt(wreck.m_Vehicle, players, 100))
			{
				wreck.m_iAbsentMs = 0;
				continue;
			}
			wreck.m_iAbsentMs += dtMs;
			if (wreck.m_iAbsentMs < 10 * 60000)
				continue;
			RemoveWreck(civils, wreck, now, "10 min sans joueur à moins de 100 m");
			m_aWrecks.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveWreck(SRP_CivilianManagerComponent civils, SRP_CheckpointWreck wreck, int now, string why)
	{
		if (wreck.m_Car && wreck.m_Car.m_Record && wreck.m_Car.m_iState != SRP_ECarState.CLOSE)
		{
			// Le conducteur arrêté part avec son véhicule (pris en charge par la prévôté)
			civils.CheckpointDiscard(wreck.m_Car.m_Record, false);
			SetState(wreck.m_Car, SRP_ECarState.CLOSE, now, "retiré avec son véhicule : " + why);
		}
		else if (wreck.m_Vehicle && !wreck.m_Vehicle.IsDeleted())
		{
			SCR_EntityHelper.DeleteEntityAndChildren(wreck.m_Vehicle);
			Log(string.Format("véhicule n° %1 retiré (%2) : %3", wreck.m_iNumber, wreck.m_sWhy, why));
		}
		wreck.m_Vehicle = null;
		wreck.m_Car = null;
	}

	//------------------------------------------------------------------------------------------------
	// Bilan et nettoyage
	//------------------------------------------------------------------------------------------------
	//! Prime ajoutée (ou retirée) à celle de la mission
	int BonusEuros()
	{
		return m_iSeizures * m_Settings.m_iSeizureBonus + m_iArrests * m_Settings.m_iArrestBonus - m_iMissed * m_Settings.m_iMissedMalus;
	}

	//------------------------------------------------------------------------------------------------
	//! Primes par saisie et arrestation seules (versées même si le point est abandonné), sans le malus
	int EarnedEuros()
	{
		return m_iSeizures * m_Settings.m_iSeizureBonus + m_iArrests * m_Settings.m_iArrestBonus;
	}

	//------------------------------------------------------------------------------------------------
	string Report()
	{
		string text = string.Format("%1 conducteur(s) en règle contrôlé(s), %2 saisie(s), %3 arrestation(s)", m_iChecked, m_iSeizures, m_iArrests);
		if (m_iMissed > 0)
			text += string.Format(", %1 suspect(s) passé(s) au travers", m_iMissed);
		if (m_iKilled > 0)
			text += string.Format(", %1 suspect(s) abattu(s)", m_iKilled);
		if (m_iUseless > 0)
			text += string.Format(", %1 fouille(s) pour rien", m_iUseless);
		return text;
	}

	//------------------------------------------------------------------------------------------------
	string ProgressText(int now)
	{
		return string.Format("point tenu, encore %1 min — %2 saisie(s), %3 arrestation(s)", MinutesLeft(now), m_iSeizures, m_iArrests);
	}

	//------------------------------------------------------------------------------------------------
	//! Fin de mission : les conducteurs arrêtés et les véhicules saisis sont pris en charge (retirés), les autres
	//! repartent. Un véhicule n'est jamais retiré avec un joueur dedans ou à moins de 5 m ; un conducteur arrêté
	//! qu'un joueur escorte (à moins de 5 m) reste lui aussi.
	void Close()
	{
		SRP_CivilianManagerComponent civils = SRP_CivilianManagerComponent.GetInstance();
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		int now = System.GetTickCount();
		if (m_Lantern)
		{
			SRP_GenerateurComponent.RemoveLantern(m_Lantern);
			m_Lantern = null;
		}
		foreach (SRP_CheckpointCar car : m_aCars)
		{
			if (!civils || !car.m_Record)
				continue;
			SRP_CivRecord record = car.m_Record;
			if (car.m_iState == SRP_ECarState.APPROCHE || car.m_iState == SRP_ECarState.EN_ATTENTE || (car.m_iState == SRP_ECarState.ARRETEE && !car.m_bFound && !car.m_bCaptive))
			{
				if (car.m_iState == SRP_ECarState.ARRETEE)
				{
					if (car.m_bSurrendered)
						civils.CheckpointSurrender(record, false);
					civils.CheckpointGo(record, now, car.m_bOut);
				}
				else
				{
					civils.CheckpointRelease(record);
				}
				Log(string.Format("véhicule n° %1 rendu à la circulation (fin du point)", car.m_iNumber));
			}
			else if (car.m_iState == SRP_ECarState.REPART)
			{
				if (!car.m_bDriving)
					civils.CheckpointGo(record, now, true);
				else
					civils.CheckpointRelease(record);
				Log(string.Format("véhicule n° %1 rendu à la circulation (fin du point)", car.m_iNumber));
			}
			else
			{
				bool keepVehicle = IsPlayerAt(record.m_Vehicle, players, 5);
				if (record.m_Character && !record.m_Character.IsDeleted() && !SRP_Utils.IsDead(record.m_Character) && IsPlayerAt(record.m_Character, players, 5))
					record.m_Character = null;	// escorté par un joueur : il reste
				civils.CheckpointDiscard(record, keepVehicle);
				Log(string.Format("véhicule n° %1 retiré (fin du point, véhicule gardé près d'un joueur : %2)", car.m_iNumber, keepVehicle.ToString()));
			}
		}
		m_aCars.Clear();

		foreach (SRP_CheckpointWreck wreck : m_aWrecks)
		{
			if (!wreck.m_Vehicle || wreck.m_Vehicle.IsDeleted())
				continue;
			if (IsPlayerAt(wreck.m_Vehicle, players, 5))
			{
				Log(string.Format("véhicule n° %1 laissé en place (fin du point) : un joueur est dedans ou à côté", wreck.m_iNumber));
				continue;
			}
			wreck.m_Car = null;	// les fiches sont déjà soldées ci-dessus
			if (civils)
				RemoveWreck(civils, wreck, now, "fin du point");
		}
		m_aWrecks.Clear();
		s_aActive.RemoveItem(this);
	}
}

//------------------------------------------------------------------------------------------------
//! « Établir un point de contrôle ici » : en regardant un équipier (override additif de Character_Base.et). Le lieu est
//! celui où se tient le soldat ; tout est vérifié par le serveur (SRP_MissionManagerComponent.StartControl).
class SRP_CheckpointStartAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int userId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (userId <= 0)
			return;
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();
		if (!missions)
			return;
		string reply = missions.StartControl(userId, false);
		if (!reply.IsEmpty())
			SRP_Utils.NotifyPlayer(userId, reply);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Établir un point de contrôle ici";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Seulement en regardant un autre JOUEUR vivant, à pied (il faut être deux pour tenir un point)
	override bool CanBeShownScript(IEntity user)
	{
		IEntity owner = GetOwner();
		if (!owner || owner == user || SRP_Utils.IsDead(owner))
			return false;
		if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(owner) <= 0)
			return false;
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(user.FindComponent(CompartmentAccessComponent));
		if (access && access.IsInCompartment())
			return false;
		// Seulement quand toutes les conditions sont réunies ici (formé, route, à 1000 m au moins de la base, sur un carré
		// bleu, aucun autre contrôle proche, deux soldats) : calculé par le serveur toutes les 5 s (SRP_Rights) avec les
		// règles de SRP_MissionManagerComponent.CheckControl (G10, Q10)
		return SRP_Rights.Has(user, SRP_Rights.CONTROLE);
	}

	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! « Désamorcer le chargement » : à poser sur les prefabs de voitures civiles (ActionsManagerComponent) si l'on
//! préfère l'action au contact du véhicule à l'ordre du menu Contrôler. Le serveur vérifie tout (SRP_Checkpoint.
//! DefuseVehicle : véhicule du contrôle, explosifs trouvés, sapeur). L'affichage se décide côté client, qui ne connaît
//! pas les points de contrôle : l'action est donc visible sur tout véhicule qui la porte.
class SRP_CheckpointDefuseAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;
		int userId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (userId <= 0)
			return;
		string reply = SRP_Checkpoint.DefuseVehicle(userId, pOwnerEntity);
		if (!reply.IsEmpty())
			SRP_Utils.NotifyPlayer(userId, reply);
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Désamorcer le chargement (contrôle routier)";
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		CompartmentAccessComponent access = CompartmentAccessComponent.Cast(user.FindComponent(CompartmentAccessComponent));
		return !access || !access.IsInCompartment();
	}

	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}

//------------------------------------------------------------------------------------------------
//! Le menu « Contrôler » : même mécanique que le terminal, commande « controle »
class SRP_ControlMenu : SRP_ListMenu
{
	override protected string GetMenuTitle() { return "Contrôle du véhicule"; }
	override protected string GetConfirmLabel() { return "Ordonner"; }
	override protected string GetCommand() { return "controle"; }

	static void Open(string content)
	{
		OpenPreset(ChimeraMenuPreset.SRP_ControlMenu, content);
	}
}
