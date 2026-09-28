//------------------------------------------------------------------------------------------------
// SimpleRP — Entraide de l'IA ennemie (carte #69, livraison 2 : « section, aide, ratissage »)
// Statique, serveur seulement ; appelé par SRP_EnemyComponent (ServeAlert et AlertTick, toutes les 10 s).
// - VIGILANCE : pendant une alerte, les groupes à moins de s_fVigilantRadius du contact deviennent plus vigilants SUR
//   PLACE quelques minutes (perception au calme relevée par SRP_EnemySenses) ; aucune position ne leur est signalée :
//   ils ne quittent pas leur poste pour enquêter.
// - AIDE : environ 20 s après le premier repérage, le groupe libre le plus proche (à moins de s_fHelpRadius) vient
//   aider ; par le flanc si un point de contournement caché du contact par le relief existe, sinon en direct. Jamais le
//   groupe qui a donné l'alerte, un groupe déjà au contact, un véhicule, l'officier, un poste d'entrée de garnison, un
//   groupe déjà occupé ; le centre d'une défense (QG) seulement s'il est le SEUL groupe libre (livraison 3). Contact
//   repris : les mêmes aidants reviennent (un nouveau seulement pour remplacer un aidant tombé).
// - JEEPS : une fois par alerte, elles viennent se poster près de la dernière position connue.
// - RATISSAGE : contact perdu depuis s_iContactLostMs, les groupes venus aider, puis ceux qui ont déjà ratissé pour
//   cette alerte, puis les plus proches fouillent autour de la dernière position connue (fouille et détruit, bâtiments
//   compris ; un nouveau point toutes les s_iSweepStepMs), puis rentrent à leur poste ou à leur ronde (RestoreTick).
// - FRONT ET COMMANDEUR (cartes #75 et #76) : jamais un groupe qui a un rôle du Commandeur (m_iCmdRole : assaut, base de
//   feu, manœuvre, couverture, repli, renfort, raid, officier et ses gardes, servants, équipages, garde de dépôt), ni un
//   groupe d'une contre-attaque (m_iAssaultZone), ni un poste du front (« poste front » : il tient son carré) ; un aidant
//   ou un ratisseur que le Commandeur reprend en cours de route n'est plus ni relancé ni ramené à son poste.
//   FlankPoint est public : les manœuvres du Commandeur s'en servent pour l'approche par le flanc (MA8).
// Journal : catégorie ENNEMI seulement (hors du flux Discord public), aucun message aux joueurs.
//------------------------------------------------------------------------------------------------

class SRP_EnemyAwareness
{
	// Réglages (copiés par SRP_EnemyComponent.OnPostInit ; valeurs par défaut ci-dessous)
	static int s_iHelpDelayMs = 20000;			// aide : délai après le premier repérage du contact
	static float s_fHelpRadius = 600;			// aide : groupes candidats à moins de … du contact
	static int s_iHelpGroups = 1;				// aide : nombre de groupes envoyés
	static bool s_bFlanking = true;				// aide : par le flanc
	static float s_fFlankAngle = 75;			// aide : angle du contournement, en degrés
	static float s_fFlankMin = 60;				// aide : distance du point de contournement au contact
	static float s_fFlankMax = 150;
	static int s_iContactLostMs = 60000;		// ratissage : contact perdu depuis …
	static int s_iSweepMs = 600000;				// ratissage : durée
	static int s_iSweepGroups = 2;				// ratissage : groupes au total
	static float s_fSweepJoinRadius = 800;		// ratissage : groupes candidats à moins de … de la dernière position
	static float s_fSweepRadius = 120;			// ratissage : rayon fouillé
	static int s_iSweepStepMs = 90000;			// ratissage : un nouveau point toutes les …
	static float s_fVigilantRadius = 800;		// vigilance : groupes à moins de … du contact
	static int s_iVigilantMs = 300000;			// vigilance : durée après le dernier repérage

	//------------------------------------------------------------------------------------------------
	// Alerte : aide, jeeps, ratissage
	//------------------------------------------------------------------------------------------------
	//! À chaque passage d'une alerte vivante (si l'alerte cherche) : l'aide une fois par contact, les jeeps une fois par
	//! alerte, le ratissage une fois le contact perdu
	static void Serve(SRP_EnemyComponent enemies, SRP_EnemyAlert alert, int now)
	{
		if (!enemies || !alert)
			return;

		if (!alert.m_bHelpSent && now - alert.m_iContactTick >= s_iHelpDelayMs)
		{
			alert.m_bHelpSent = true;
			SendHelp(enemies, alert, now);
		}

		if (!alert.m_bSearchSent && now - alert.m_iFirstTick >= s_iHelpDelayMs)
		{
			alert.m_bSearchSent = true;
			enemies.SendJeeps(alert, now);
		}

		if (!alert.m_bSweepSent && now - alert.m_iLastTick >= s_iContactLostMs)
		{
			alert.m_bSweepSent = true;
			SendSweep(enemies, alert, now);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Vigilance : à chaque nouveau repérage de l'alerte, les groupes proches restent plus vigilants sur place jusqu'à
	//! s_iVigilantMs après ce repérage (SRP_EnemySenses relève leur perception au calme). Aucune position injectée.
	static void SpreadVigilance(SRP_EnemyComponent enemies, SRP_EnemyAlert alert, int now)
	{
		if (!enemies || !alert || alert.m_iVigilantTick == alert.m_iLastTick)
			return;
		bool firstSpread = alert.m_iVigilantTick == 0;
		alert.m_iVigilantTick = alert.m_iLastTick;
		if (s_iVigilantMs <= 0 || Math.AbsFloat(SRP_EnemySenses.s_fVigilantFactor - 1) < 0.001)
			return;

		int until = alert.m_iLastTick + s_iVigilantMs;
		int count = 0;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || record.m_sOwner == "helico")
				continue;
			if (SRP_EnemyComponent.AliveAgents(record) == 0)
				continue;
			if (vector.Distance(enemies.GroupPosition(record), alert.m_vPosition) > s_fVigilantRadius)
				continue;
			if (record.m_iVigilantUntilTick < until)
				record.m_iVigilantUntilTick = until;
			count++;
		}
		if (firstSpread && count > 0)
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("Vigilance : %1 groupe(s) à moins de %2 m du contact (grille %3 / %4) restent plus attentifs sur place", count, Math.Round(s_fVigilantRadius), Math.Round(alert.m_vPosition[0]), Math.Round(alert.m_vPosition[2])));
	}

	//------------------------------------------------------------------------------------------------
	//! Aide : les s_iHelpGroups groupes libres les plus proches du contact, chacun par son point de contournement s'il y
	//! en a un (à défaut en direct) ; ils sont retenus sur l'alerte pour le ratissage. Contact repris : les groupes déjà
	//! venus aider pour cette alerte (encore vivants) reviennent sur le nouveau contact ; on n'en tire de nouveaux que
	//! pour remplacer ceux qui sont tombés, et au plus s_iHelpGroups + s_iSweepGroups groupes différents par alerte (un
	//! contact intermittent ne vide pas la localité).
	protected static void SendHelp(SRP_EnemyComponent enemies, SRP_EnemyAlert alert, int now)
	{
		if (s_iHelpGroups <= 0)
			return;	// aide désactivée
		array<SRP_EnemyGroup> picked = {};
		array<float> distances = {};
		foreach (SCR_AIGroup helperGroup : alert.m_aHelpers)
		{
			if (picked.Count() >= s_iHelpGroups)
				break;
			SRP_EnemyGroup previous = enemies.FindRecord(helperGroup);
			if (!previous || picked.Contains(previous) || SRP_EnemyComponent.AliveAgents(previous) == 0)
				continue;
			if (IsCommanded(previous))
				continue;	// repris par le Commandeur depuis : il garde ses ordres
			picked.Insert(previous);
			distances.Insert(vector.Distance(enemies.GroupPosition(previous), alert.m_vPosition));
		}
		int kept = picked.Count();
		// Nouveaux aidants : ce qui manque, dans la limite des groupes différents encore permis pour cette alerte
		int room = s_iHelpGroups + Math.ClampInt(s_iSweepGroups, 0, 100) - alert.m_aHelpers.Count();
		if (room < 0)
			room = 0;
		int added = s_iHelpGroups - kept;
		if (added > room)
			added = room;
		Nearest(enemies, alert, now, s_fHelpRadius, kept + added, picked, distances, true);
		if (picked.IsEmpty())
		{
			if (room == 0)
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Aide : aucun nouveau groupe, %1 groupe(s) déjà envoyés pour cette alerte (grille %2 / %3)", alert.m_aHelpers.Count(), Math.Round(alert.m_vPosition[0]), Math.Round(alert.m_vPosition[2])));
			else
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Aide : aucun groupe libre à moins de %1 m du contact (grille %2 / %3)", Math.Round(s_fHelpRadius), Math.Round(alert.m_vPosition[0]), Math.Round(alert.m_vPosition[2])));
			return;
		}

		// La section qui tient le contact : l'axe du contournement part d'elle (à défaut, de l'aidant)
		vector source = vector.Zero;
		bool hasSource = false;
		SRP_EnemyGroup sourceRecord = enemies.FindRecord(alert.m_SourceGroup);
		if (sourceRecord && SRP_EnemyComponent.AliveAgents(sourceRecord) > 0)
		{
			source = enemies.GroupPosition(sourceRecord);
			hasSource = true;
		}

		foreach (int index, SRP_EnemyGroup record : picked)
		{
			string task = record.m_sTask;
			bool again = index < kept;	// déjà venu aider pour cette alerte
			if (again && InContact(record, now))
			{
				// Il se bat déjà : on ne lui retire pas ses points de passage en plein accrochage
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Aide : %1 (%2) déjà au contact, à %3 m, il garde ses ordres", task, record.m_sOwner, Math.Round(distances[index])));
				continue;
			}
			vector helper = enemies.GroupPosition(record);
			vector flank = vector.Zero;
			string how = "en direct (contournement désactivé)";
			if (s_bFlanking)
			{
				if (FlankPoint(helper, source, hasSource, alert.m_vPosition, flank))
					how = string.Format("par le flanc (point de contournement à %1 m du contact)", Math.Round(vector.Distance(flank, alert.m_vPosition)));
				else
				{
					flank = vector.Zero;
					how = "en direct (aucun point de contournement hors de vue du contact)";
				}
			}
			enemies.OrderHelp(record, flank, alert.m_vPosition, now);
			if (again)
			{
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Aide : %1 (%2) revient sur le contact repris, à %3 m, %4", task, record.m_sOwner, Math.Round(distances[index]), how));
				continue;
			}
			alert.m_aHelpers.Insert(record.m_Group);
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("Aide : %1 (%2) part aider à %3 m du contact, %4", task, record.m_sOwner, Math.Round(distances[index]), how));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ratissage : ceux venus aider (même occupés : c'est leur contact), puis ceux qui ont déjà ratissé pour cette alerte
	//! (un contact intermittent ne tire pas de nouveaux groupes à chaque rupture), puis les groupes libres les plus
	//! proches de la dernière position connue, s_iSweepGroups au total
	protected static void SendSweep(SRP_EnemyComponent enemies, SRP_EnemyAlert alert, int now)
	{
		if (s_iSweepGroups <= 0)
			return;	// ratissage désactivé
		array<SRP_EnemyGroup> picked = {};
		array<float> distances = {};
		foreach (SCR_AIGroup helperGroup : alert.m_aHelpers)
		{
			if (picked.Count() >= s_iSweepGroups)
				break;
			SRP_EnemyGroup helper = enemies.FindRecord(helperGroup);
			if (!helper || picked.Contains(helper) || SRP_EnemyComponent.AliveAgents(helper) == 0)
				continue;
			if (IsCommanded(helper))
				continue;	// repris par le Commandeur depuis : il garde ses ordres
			picked.Insert(helper);
			distances.Insert(vector.Distance(enemies.GroupPosition(helper), alert.m_vPosition));
		}
		foreach (SCR_AIGroup sweeperGroup : alert.m_aSweepers)
		{
			if (picked.Count() >= s_iSweepGroups)
				break;
			SRP_EnemyGroup sweeper = enemies.FindRecord(sweeperGroup);
			if (!sweeper || picked.Contains(sweeper) || SRP_EnemyComponent.AliveAgents(sweeper) == 0)
				continue;
			if (IsCommanded(sweeper))
				continue;	// repris par le Commandeur depuis : il garde ses ordres
			picked.Insert(sweeper);
			distances.Insert(vector.Distance(enemies.GroupPosition(sweeper), alert.m_vPosition));
		}
		Nearest(enemies, alert, now, s_fSweepJoinRadius, s_iSweepGroups, picked, distances, false);
		if (picked.IsEmpty())
		{
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("Ratissage : contact perdu (grille %1 / %2), aucun groupe libre à moins de %3 m", Math.Round(alert.m_vPosition[0]), Math.Round(alert.m_vPosition[2]), Math.Round(s_fSweepJoinRadius)));
			return;
		}

		int until = now + s_iSweepMs;
		foreach (SRP_EnemyGroup record : picked)
		{
			enemies.OrderSweep(record, alert.m_vPosition, until);
			if (!alert.m_aHelpers.Contains(record.m_Group) && !alert.m_aSweepers.Contains(record.m_Group))
				alert.m_aSweepers.Insert(record.m_Group);
		}
		int seconds = (now - alert.m_iLastTick) / 1000;
		int minutes = s_iSweepMs / 60000;
		SRP_EnemyComponent.Journal("ENNEMI", string.Format("Ratissage : contact perdu depuis %1 s, %2 groupe(s) fouillent %3 m autour de la dernière position (grille %4 / %5) pendant %6 min", seconds, picked.Count(), Math.Round(s_fSweepRadius), Math.Round(alert.m_vPosition[0]), Math.Round(alert.m_vPosition[2]), minutes));
	}

	//------------------------------------------------------------------------------------------------
	//! Complète « picked » (jusqu'à « count ») avec les groupes libres les plus proches du contact, à moins de « radius »,
	//! du plus proche au plus loin ; « distances » suit « picked ». Le groupe de commandement (le centre d'une défense,
	//! le QG d'une ville) n'est pris que si « commandIfAlone » et qu'il n'y a AUCUN autre groupe libre à portée (carte #69,
	//! livraison 3, décision de Jack du 25/09 : l'aide doit partir même d'une petite garnison)
	protected static void Nearest(SRP_EnemyComponent enemies, SRP_EnemyAlert alert, int now, float radius, int count, array<SRP_EnemyGroup> picked, array<float> distances, bool commandIfAlone)
	{
		if (picked.Count() >= count)
			return;
		array<SRP_EnemyGroup> pool = {};
		array<float> poolDistances = {};
		Collect(enemies, alert, now, radius, false, picked, pool, poolDistances);
		if (pool.IsEmpty() && commandIfAlone)
		{
			Collect(enemies, alert, now, radius, true, picked, pool, poolDistances);
			if (!pool.IsEmpty())
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Aide : aucun autre groupe libre à moins de %1 m du contact, le groupe de commandement (%2) peut partir", Math.Round(radius), pool[0].m_sOwner));
		}
		for (int i = 0; i < pool.Count(); i++)
		{
			if (picked.Count() >= count)
				break;
			picked.Insert(pool[i]);
			distances.Insert(poolDistances[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les groupes libres à moins de « radius » du contact, hors « picked », triés du plus proche au plus loin :
	//! groupes de commandement seulement si « command », sinon tous les autres
	protected static void Collect(SRP_EnemyComponent enemies, SRP_EnemyAlert alert, int now, float radius, bool command, array<SRP_EnemyGroup> picked, array<SRP_EnemyGroup> pool, array<float> poolDistances)
	{
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		foreach (SRP_EnemyGroup record : groups)
		{
			if (picked.Contains(record) || !IsFree(record, alert, now, command))
				continue;
			float distance = vector.Distance(enemies.GroupPosition(record), alert.m_vPosition);
			if (distance > radius)
				continue;
			int slot = 0;
			while (slot < poolDistances.Count() && poolDistances[slot] <= distance)
				slot++;
			pool.InsertAt(record, slot);
			poolDistances.InsertAt(distance, slot);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un groupe peut-il partir aider ou ratisser ? Pas le groupe qui a donné l'alerte, ni un groupe déjà au contact (il
	//! a lu un joueur depuis moins de s_iContactLostMs : il se bat déjà, on ne le sort pas de son abri), ni un véhicule
	//! (jeep, hélicoptère, renfort motorisé), ni l'officier, ni un poste d'entrée de garnison (ses sentinelles gardent les
	//! accès), ni un groupe déjà occupé, vide, ou dont un soldat est assis (escorte de convoi à bord, servant d'une arme de
	//! poste). « command » : on cherche les groupes de commandement (centre d'une défense, QG), sinon tous les autres.
	//! Front et Commandeur : jamais un groupe aux ordres du Commandeur ou d'une contre-attaque (IsCommanded), ni un poste
	//! du front (« poste front » : il tient son carré, comme un poste d'entrée tient son accès).
	protected static bool IsFree(SRP_EnemyGroup record, SRP_EnemyAlert alert, int now, bool command)
	{
		if (!record || !record.m_Group || record.m_Group.IsDeleted())
			return false;
		if (IsCommanded(record))
			return false;
		if (record.m_Group == alert.m_SourceGroup || InContact(record, now))
			return false;
		if (record.m_Vehicle || record.m_sOwner == "helico")
			return false;
		if (record.m_bCommand != command)
			return false;
		if (record.m_sTask == "officier" || record.m_sTask == "jeep" || record.m_sTask == "helico")
			return false;
		if (record.m_sHomeTask == "entrée" || record.m_sHomeTask == "poste front")
			return false;
		if (record.m_iBusyUntilTick > now)
			return false;
		if (SRP_EnemyComponent.AliveAgents(record) == 0)
			return false;
		return !HasSeatedMember(record);
	}

	//------------------------------------------------------------------------------------------------
	//! Groupe aux ordres du Commandeur (un rôle SRP_ECmdRole autre qu'AUCUN : assaut, repli, officier et ses gardes,
	//! servants, équipages…) ou d'une contre-attaque du front (m_iAssaultZone) : l'entraide ne le prend jamais, ne le
	//! relance pas et ne le ramène pas à son poste (cartes #75 et #76 ; un rôle remis à AUCUN le rend à l'entraide)
	protected static bool IsCommanded(SRP_EnemyGroup record)
	{
		if (!record)
			return false;
		return record.m_iCmdRole != SRP_ECmdRole.AUCUN || record.m_iAssaultZone >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe a-t-il lu un joueur depuis moins de s_iContactLostMs (SRP_EnemyComponent.AlertTick) ?
	protected static bool InContact(SRP_EnemyGroup record, int now)
	{
		return record && record.m_iSpotTick != 0 && now - record.m_iSpotTick < s_iContactLostMs;
	}

	//------------------------------------------------------------------------------------------------
	protected static bool HasSeatedMember(SRP_EnemyGroup record)
	{
		array<AIAgent> agents = {};
		record.m_Group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			ChimeraCharacter character = ChimeraCharacter.Cast(agent.GetControlledEntity());
			if (character && character.IsInVehicle())
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Point de contournement : l'axe « contact vers la section qui le tient » (à défaut « contact vers l'aidant ») tourné
	//! de s_fFlankAngle du côté de l'aidant, à la moitié de la distance aidant-contact (bornée entre s_fFlankMin et
	//! s_fFlankMax). Six essais (angle et distance un peu décalés), chacun au sol, hors de l'eau, et caché du contact par
	//! le relief seul (test de lisière : une clôture, un lampadaire ou une voiture ne cachent pas un groupe). Faux si aucun
	//! ne convient : approche directe. Public : aussi appelé par les manœuvres du Commandeur (MA8, approche par le flanc).
	static bool FlankPoint(vector helper, vector source, bool hasSource, vector contact, out vector flank)
	{
		vector axis = helper - contact;
		if (hasSource)
			axis = source - contact;
		axis[1] = 0;
		float axisLength = axis.Length();
		if (axisLength < 1)
		{
			axis = helper - contact;
			axis[1] = 0;
			axisLength = axis.Length();
			if (axisLength < 1)
				return false;
		}
		axis = axis * (1 / axisLength);

		vector toHelper = helper - contact;
		toHelper[1] = 0;
		float distance = Math.Clamp(toHelper.Length() * 0.5, s_fFlankMin, s_fFlankMax);
		float baseDegrees = Math.Clamp(s_fFlankAngle, 10, 170);

		// Le côté de l'aidant : celui des deux points à l'angle réglé qui est le plus près de lui (au hasard si égalité)
		vector left = contact + RotateFlat(axis, baseDegrees * Math.DEG2RAD) * distance;
		vector right = contact + RotateFlat(axis, -baseDegrees * Math.DEG2RAD) * distance;
		float leftGap = vector.DistanceXZ(left, helper);
		float rightGap = vector.DistanceXZ(right, helper);
		float side = 1;
		if (rightGap < leftGap - 1)
			side = -1;
		else if (Math.AbsFloat(rightGap - leftGap) <= 1 && Math.RandomInt(0, 2) == 0)
			side = -1;

		for (int attempt = 0; attempt < 6; attempt++)
		{
			// Décalages successifs : 0, +15, -15, +30, -30, +45 degrés ; distance réduite d'un quart au-delà du 3e essai
			int step = (attempt + 1) / 2;
			float offset = step * 15;
			if (attempt - (attempt / 2) * 2 == 0)
				offset = -offset;
			float degrees = Math.Clamp(baseDegrees + offset, 10, 170);
			float tryDistance = distance;
			if (attempt >= 3)
				tryDistance = Math.Clamp(distance * 0.75, s_fFlankMin, s_fFlankMax);

			vector candidate = SRP_Placement.OnGround(contact + RotateFlat(axis, side * degrees * Math.DEG2RAD) * tryDistance);
			if (SRP_Placement.IsWater(candidate))
				continue;
			// Caché du contact par le relief seul, aux six essais (en terrain plat, aucun point ne convient : approche
			// directe ; le réglage m_bFlanking coupe le contournement s'il paraît artificiel)
			if (SRP_Placement.HasLineOfSight(candidate, contact, null, true))
				continue;	// le contact le verrait arriver : pas un contournement
			flank = candidate;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Une direction à plat tournée de « radians » autour de la verticale
	protected static vector RotateFlat(vector direction, float radians)
	{
		float cosine = Math.Cos(radians);
		float sine = Math.Sin(radians);
		return Vector(direction[0] * cosine - direction[2] * sine, 0, direction[0] * sine + direction[2] * cosine);
	}

	//------------------------------------------------------------------------------------------------
	// Ratissage guidé et retour au poste
	//------------------------------------------------------------------------------------------------
	//! Toutes les 10 s (AlertTick) : un ratisseur reçoit un nouveau point toutes les s_iSweepStepMs ; un groupe dont
	//! l'aide ou le ratissage est fini rentre à son poste (RestoreHome) ; un groupe revenu reprend sa tâche d'origine
	static void RestoreTick(SRP_EnemyComponent enemies, int now)
	{
		if (!enemies)
			return;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted())
				continue;
			if (IsCommanded(record))
			{
				// Repris par le Commandeur (ou une contre-attaque) pendant l'aide, le ratissage ou le retour : ses ordres
				// priment, l'entraide le lâche sans le ramener à son poste
				if (record.m_iBusyUntilTick > 0)
					record.m_iBusyUntilTick = 0;
				continue;
			}
			if (record.m_iBusyUntilTick > 0)
			{
				if (SRP_EnemyComponent.AliveAgents(record) == 0)
					continue;	// plus personne : le nettoyage s'en charge
				if (now >= record.m_iBusyUntilTick)
				{
					string finished = record.m_sTask;
					enemies.RestoreHome(record);
					SRP_EnemyComponent.Journal("ENNEMI", string.Format("Retour : %1 (%2) a fini (%3), il reprend : %4", record.m_sHomeTask, record.m_sOwner, finished, HomeLabel(record)));
					continue;
				}
				if (record.m_sTask == "ratissage" && now >= record.m_iNextSweepTick)
				{
					enemies.OrderSearch(record, SweepPoint(record.m_vSweepCenter));	// fouille, bâtiments compris
					record.m_iNextSweepTick = now + s_iSweepStepMs;
				}
				continue;
			}

			if (record.m_sTask != "retour")
				continue;
			bool back = now >= record.m_iReturnUntilTick || record.m_bHomeCycle || record.m_bPatrol;
			if (!back && SRP_EnemyComponent.AliveAgents(record) > 0)
				back = vector.Distance(enemies.GroupPosition(record), record.m_vHome) <= Math.Max(record.m_fHomeHold, 60);
			if (!back)
				continue;
			record.m_sTask = record.m_sHomeTask;
			// Arrivé (ou délai de retour écoulé) : rôle et maintien d'origine rétablis (un groupe de garde rentre sans
			// maintien, RestoreHome) ; les soldats reprennent ensuite leur poste là où ils se posent (SRP_EnemySenses)
			int homeRole = record.m_iHomeRole;
			if (homeRole < 0)
				homeRole = SRP_ECRXRole.PATROUILLE;
			if (record.m_iRole != homeRole || record.m_fHold != record.m_fHomeHold)
				enemies.ApplyRole(record, homeRole, record.m_vHome, record.m_fHomeHold);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Un point de ratissage autour de la dernière position connue : sur une rue quand il y en a une, sinon au sol
	protected static vector SweepPoint(vector center)
	{
		vector point;
		if (SRP_Placement.RandomRoadPointNear(center, s_fSweepRadius, null, point))
			return point;
		for (int attempt = 0; attempt < 6; attempt++)
		{
			float angle = Math.RandomFloat(0, Math.PI2);
			float distance = Math.RandomFloat(s_fSweepRadius * 0.3, s_fSweepRadius);
			vector probe = SRP_Placement.OnGround(center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
			if (!SRP_Placement.IsWater(probe))
				return probe;
		}
		return center;
	}

	//------------------------------------------------------------------------------------------------
	protected static string HomeLabel(SRP_EnemyGroup record)
	{
		if (record.m_bHomeDefend)
			return "son poste";
		if (record.m_bHomeCycle || record.m_bPatrol)
			return "sa ronde";
		return "son itinéraire";
	}

	//------------------------------------------------------------------------------------------------
	// Pages Staff
	//------------------------------------------------------------------------------------------------
	//! La tâche d'un groupe : aide ou ratissage jusqu'à hh:mm, au poste, ronde, retour ; rôle donné par le Commandeur
	//! (repli, couverture, base de feu, manœuvre, raid, renfort…) et contre-attaque du front ; vigilance d'alerte
	static string TaskLabel(SRP_EnemyGroup record, int now)
	{
		if (!record)
			return "?";
		string task = record.m_sTask;
		if (task.IsEmpty())
			task = "sans tâche";
		string label = task;
		if ((task == "aide" || task == "ratissage") && record.m_iBusyUntilTick > now)
			label = task + " jusqu'à " + ClockIn(record.m_iBusyUntilTick - now);
		else if (task == "centre" || task == "poste" || task == "garde" || task == "entrée" || task == "bâtiment" || task == "poste de nuit" || task == "débarqués")
			label = "au poste (" + task + ")";
		else if (task == "poste front" || task == "garde officier" || task == "appui")
			label = "au poste (" + task + ")";
		else if (task == "retour")
			label = "retour (" + record.m_sHomeTask + ")";

		// Commandeur (#76) : le rôle qu'il a donné, s'il ne se lit pas déjà dans la tâche
		string role = CmdRoleLabel(record.m_iCmdRole);
		if (!role.IsEmpty() && role != task)
			label += ", Commandeur : " + role;
		// Front (#75) : groupe d'une contre-attaque (zone visée)
		if (record.m_iAssaultZone >= 0)
			label += ", contre-attaque sur " + AssaultZoneLabel(record.m_iAssaultZone);

		if (record.m_iVigilantUntilTick > now)
			label += ", vigilant jusqu'à " + ClockIn(record.m_iVigilantUntilTick - now);
		return label;
	}

	//------------------------------------------------------------------------------------------------
	//! Libellé Staff d'un rôle du Commandeur (SRP_ECmdRole) ; "" pour AUCUN
	static string CmdRoleLabel(int role)
	{
		if (role == SRP_ECmdRole.AUCUN)
			return "";
		if (role == SRP_ECmdRole.ASSAUT)
			return "assaut";
		if (role == SRP_ECmdRole.BASE_FEU)
			return "base de feu";
		if (role == SRP_ECmdRole.MANOEUVRE)
			return "manœuvre";
		if (role == SRP_ECmdRole.COUVERTURE)
			return "couverture";
		if (role == SRP_ECmdRole.REPLI)
			return "repli";
		if (role == SRP_ECmdRole.RENFORT)
			return "renfort";
		if (role == SRP_ECmdRole.RAID)
			return "raid";
		if (role == SRP_ECmdRole.OFFICIER)
			return "officier";
		if (role == SRP_ECmdRole.GARDE_OFFICIER)
			return "garde officier";
		if (role == SRP_ECmdRole.SERVANT)
			return "servant d'appui";
		if (role == SRP_ECmdRole.EQUIPAGE)
			return "équipage";
		if (role == SRP_ECmdRole.GARDE_DEPOT)
			return "garde du dépôt";
		return string.Format("rôle %1", role);
	}

	//------------------------------------------------------------------------------------------------
	//! « Régina (S07) » pour la zone visée par une contre-attaque (numéro seul si le front n'est pas là)
	protected static string AssaultZoneLabel(int zone)
	{
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		if (front)
		{
			string zoneLabel = front.GetZoneLabel(zone);
			if (!zoneLabel.IsEmpty())
				return zoneLabel;
		}
		return string.Format("la zone %1", zone);
	}

	//------------------------------------------------------------------------------------------------
	//! En-tête de l'état des groupes : alertes, groupes en aide, en ratissage, en retour, vigilants
	static string Summary(SRP_EnemyComponent enemies, int now, int alerts)
	{
		int helping = 0;
		int sweeping = 0;
		int returning = 0;
		int vigilant = 0;
		if (enemies)
		{
			array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
			foreach (SRP_EnemyGroup record : groups)
			{
				if (!record || !record.m_Group || record.m_Group.IsDeleted())
					continue;
				if (record.m_sTask == "aide")
					helping++;
				else if (record.m_sTask == "ratissage")
					sweeping++;
				else if (record.m_sTask == "retour")
					returning++;
				if (record.m_iVigilantUntilTick > now)
					vigilant++;
			}
		}
		return string.Format("Entraide : %1 alerte(s) · %2 groupe(s) en aide, %3 en ratissage, %4 en retour · %5 groupe(s) vigilant(s)", alerts, helping, sweeping, returning, vigilant);
	}

	//------------------------------------------------------------------------------------------------
	//! L'heure du serveur dans « ms » millisecondes, « 21:14 »
	static string ClockIn(int ms)
	{
		int hour;
		int minute;
		int second;
		System.GetHourMinuteSecond(hour, minute, second);
		int total = hour * 3600 + minute * 60 + second + ms / 1000;
		while (total >= 86400)
			total -= 86400;
		int shownHour = total / 3600;
		int shownMinute = (total - shownHour * 3600) / 60;
		return SRP_Time.Pad2(shownHour) + ":" + SRP_Time.Pad2(shownMinute);
	}
}
