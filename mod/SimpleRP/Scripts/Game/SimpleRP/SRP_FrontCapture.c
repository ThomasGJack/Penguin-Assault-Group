//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : la CAPTURE (module 2), classe serveur tenue par le socle.
//
// RÔLE, à chaque passage de la boucle unique de 5 s du socle (SRP_FrontComponent.Tick, en PREMIER) :
// - SEUL recensement du mod (trou 10) : joueurs valides (C2 au sol ou véhicule bas, C3 hors 5 min après une
//   Game Master ouvert ou possession, seulement pendant ; la téléportation ne compte plus depuis le 27/09 ; vivants et
//   conscients) et ennemis en état de combattre (C6).
//   Le recensement se fait par entité (joueur, soldat), jamais par une recherche carré par carré.
// - Capture d'un carré rouge (C1 par front.CanBlueCapture, jamais un carré SeizeOnly : zones.IsCellSeizeOnly, trou 12),
//   1 min de présence sans ennemi (C4, C5 ; réglage front_capture_minutes), figée au combat puis qui redescend (C7).
// - Passages entre territoire ami et ennemi : le recensement est passé à SRP_FrontRadio.PlayerSides (message au joueur).
// - Blocs de localité (choix de Jack du 27/09) : le centre d'une ville ou d'un village (carrés de sa zone à moins de
//   m_fBlockRadius) se prend d'un seul coup, en m_iBlockMinutes de présence sans ennemi dans le bloc, dès qu'un de ses
//   carrés rouges touche du bleu ; les carrés des points clés restent à saisir (D4). Rien n'est sauvegardé (H3).
// - Reprise F3 (Q2) : seulement par les groupes marqués m_iAssaultZone (et les assaillants sur le papier, CA6), dans la
//   zone visée (ses carrés, plus le carré d'un de ses points clés posé dans une zone voisine), sur un carré qui touche
//   du rouge, 1 min sans joueur ; F5 sur le SEUL carré du point clé.
// - États contestés C8 envoyés au socle (SetLiveCells).
// Elle ne fait NI E4 (socle), NI annonce : la radio reçoit les carrés du socle (SRP_FrontRadio.QueueCell), ni règle
// propre pour Erquy (socle, Q5).
// Rien n'est sauvegardé (H3) : les progressions en cours sont perdues au redémarrage.
// Gelé (J3) : le recensement continue (les services restent frais), mais rien ne progresse et rien n'est contesté.
// APPELÉ PAR : SRP_FrontComponent (Tick, OnOwnersChanged, OnFrozenChanged, OnFrontReset) ; SRP_Admin.Teleport
// (NoteTeleport) ; chute, ennemi, Commandeur (services de recensement) ; SRP_CmdManeuvers (papier).
// Accès : SRP_FrontComponent.GetInstance().GetCapture().
//------------------------------------------------------------------------------------------------

class SRP_FrontCapture
{
	protected SRP_FrontComponent m_Front;			// le socle (réglages publics, carrés)

	// Compteurs par carré (4096 cases, Resize une fois : les nouvelles cases valent 0, Types.c:276)
	protected ref array<int> m_aCaptureMs = {};		// progression de capture
	protected ref array<int> m_aRetakeMs = {};			// progression de reprise
	protected ref array<int> m_aActive = {};			// carrés dont un compteur n'est pas nul
	protected ref array<int> m_aPlayersIn = {};		// joueurs valides par carré, ce passage
	protected ref array<int> m_aEnemiesIn = {};		// ennemis en état de combattre par carré, ce passage
	protected ref array<int> m_aRetakersIn = {};		// soldats de l'attaque de la zone visée, ce passage
	protected ref array<int> m_aTouchStamp = {};		// numéro de passage de la dernière écriture du carré
	protected ref array<int> m_aTouched = {};			// carrés touchés ce passage (seuls remis à zéro)
	protected int m_iStamp;
	protected ref array<int> m_aInActive = {};			// 1 si le carré est dans m_aActive (évite une recherche)
	protected ref array<int> m_aScan = {};				// copie de m_aActive parcourue par EvaluateCells
	protected ref array<int> m_aZoneBuf = {};			// carrés d'une zone (remise à plat d'une zone)

	// Recensement du passage (seule source du mod), tableaux parallèles
	protected ref array<vector> m_aPlayerPos = {};		// joueurs valides
	protected ref array<int> m_aPlayerIds = {};		// leurs identifiants
	protected ref array<int> m_aPlayerCells = {};		// leur carré (-1 hors grille ou en mer)
	protected ref array<IEntity> m_aPlayerEnts = {};	// leur personnage (IsCharacterCounted)
	protected ref array<vector> m_aEnemyPos = {};		// ennemis en état de combattre
	protected ref array<int> m_aEnemyAssault = {};		// m_iAssaultZone de leur groupe
	protected ref array<int> m_aEnemyCells = {};		// leur carré (-1 hors grille ou en mer)
	protected ref array<AIAgent> m_aAgentBuf = {};		// tampon réutilisé (AIGroup.GetAgents)
	protected ref array<int> m_aIdBuf = {};			// tampon réutilisé (PlayerManager.GetPlayers)

	// C3 : exclusions et filet de sécurité du Staff (heures Unix)
	protected ref map<int, int> m_mExcludedUntil = new map<int, int>();
	protected ref map<int, vector> m_mLastPos = new map<int, vector>();
	protected ref map<int, bool> m_mLastOnFoot = new map<int, bool>();

	// C8 : dernier état contesté envoyé (-1 = inconnu, à renvoyer), état de ce passage, tampons de différence
	protected ref array<int> m_aLiveState = {};
	protected ref array<int> m_aLiveNow = {};
	protected ref array<int> m_aDiffCells = {};
	protected ref array<int> m_aDiffStates = {};
	protected ref array<int> m_aLiveNowList = {};		// carrés contestés à ce passage
	protected ref array<int> m_aLiveSentList = {};		// carrés dont le dernier état envoyé n'est pas AUCUN
	protected bool m_bLiveCleared;						// gel : ClearLive déjà fait une fois

	// Lots de bascule du passage
	protected ref array<int> m_aCaptureFlips = {};
	protected ref array<int> m_aRetakeFlips = {};

	// Blocs de localité (choix de Jack du 27/09), refaits par BuildBlocks
	protected ref array<int> m_aCellBlock = {};				// bloc de chaque carré, -1 hors bloc
	protected ref array<ref array<int>> m_aBlockCells = {};	// carrés de chaque bloc (carrés des points clés compris)
	protected ref array<int> m_aBlockLocality = {};			// localité (index du socle) de chaque bloc
	protected ref array<int> m_aBlockMs = {};				// progression de chaque bloc

	// CA6 : assaillants sur le papier (SRP_CmdManeuvers), tableaux parallèles
	protected ref array<int> m_aPaperCell = {};
	protected ref array<int> m_aPaperSoldiers = {};
	protected ref array<int> m_aPaperZone = {};
	protected ref array<int> m_aPaperUnix = {};

	// Horloge du passage
	protected int m_iNowUnix;							// heure Unix du passage en cours
	protected int m_iLastTickMs;						// GetTickCount du passage précédent (mesure interne, non sauvée)
	protected bool m_bHasLastTick;

	// Diagnostic du MODE ESSAI du Workbench (27/09) : dernier état écrit par joueur, heure Unix de la dernière série
	protected ref map<int, string> m_mDiagSaid = new map<int, string>();
	protected int m_iDiagUnix;

	//------------------------------------------------------------------------------------------------
	//! Créée par le socle dans OnPostInit (serveur) — SRP_FrontComponent
	void SRP_FrontCapture(SRP_FrontComponent front)
	{
		m_Front = front;
	}

	//------------------------------------------------------------------------------------------------
	//! Front prêt : Resize de tous les tableaux par carré à GetCellCount(), états contestés à AUCUN — SRP_FrontComponent.Start
	void Start()
	{
		if (!m_Front)
			return;
		int count = m_Front.GetCellCount();
		if (count <= 0)
			return;

		Fill(m_aCaptureMs, count, 0);
		Fill(m_aRetakeMs, count, 0);
		Fill(m_aPlayersIn, count, 0);
		Fill(m_aEnemiesIn, count, 0);
		Fill(m_aRetakersIn, count, 0);
		Fill(m_aTouchStamp, count, 0);
		Fill(m_aInActive, count, 0);
		Fill(m_aLiveState, count, SRP_EFrontLive.AUCUN);
		Fill(m_aLiveNow, count, SRP_EFrontLive.AUCUN);
		m_iStamp = 0;

		m_aActive.Clear();
		m_aTouched.Clear();
		m_aLiveNowList.Clear();
		m_aLiveSentList.Clear();
		m_aCaptureFlips.Clear();
		m_aRetakeFlips.Clear();
		m_aPlayerPos.Clear();
		m_aPlayerIds.Clear();
		m_aPlayerCells.Clear();
		m_aPlayerEnts.Clear();
		m_aEnemyPos.Clear();
		m_aEnemyAssault.Clear();
		m_aEnemyCells.Clear();
		m_bLiveCleared = false;
		m_bHasLastTick = false;
		BuildBlocks();
	}

	//------------------------------------------------------------------------------------------------
	//! Le passage de 5 s (1er de la boucle unique) : rien si gelé (ClearLive une fois) ; CollectPlayers, CollectEnemies,
	//! EvaluateCells, ResetTouched, ApplyFlips (captures puis reprises), PushLive ; diagnostic si m_bTimingDiag —
	//! SRP_FrontComponent.Tick
	void Tick(int nowUnix)
	{
		if (!m_Front || !m_Front.IsReady())
			return;
		int count = m_Front.GetCellCount();
		if (count <= 0 || m_aCaptureMs.Count() != count)
			return;		// Start n'est pas encore passé

		// Temps réellement écoulé depuis le passage précédent (retard du serveur), borné à deux passages
		int startMs = System.GetTickCount();
		int tickMs = SRP_FrontComponent.TICK_MS;
		if (m_bHasLastTick)
			tickMs = Math.ClampInt(startMs - m_iLastTickMs, 0, 2 * SRP_FrontComponent.TICK_MS);
		m_iLastTickMs = startMs;
		m_bHasLastTick = true;
		m_iNowUnix = nowUnix;
		m_iStamp++;

		// Le recensement continue même gelé : chute, ennemi et Commandeur lisent des données de moins de 5 s
		CollectPlayers(nowUnix);
		CollectEnemies();

		// Passages entre territoire ami et ennemi : message radio au joueur (même recensement, même gelé)
		SRP_FrontRadio.PlayerSides(m_Front, m_aPlayerIds, m_aPlayerCells, nowUnix);

		// MODE ESSAI du Workbench : état de chaque joueur au journal (compté ou non, carré, prise possible ou pourquoi)
		if (SRP_EnemyComponent.IsTestMode())
			DiagPlayers(nowUnix);

		if (m_Front.IsFrozen())
		{
			ResetTouched();
			if (!m_bLiveCleared)
			{
				ClearCounters();
				ClearLiveMirror();
				m_Front.ClearLive();
				m_bLiveCleared = true;
			}
			return;
		}
		m_bLiveCleared = false;

		CollectPaper(nowUnix);
		EvaluateCells(tickMs);
		EvaluateBlocks(tickMs);
		int touchedCount = m_aTouched.Count();
		ResetTouched();
		ApplyFlips();
		PushLive();

		if (m_Front.m_bTimingDiag)
		{
			int spentMs = System.GetTickCount() - startMs;
			if (spentMs > 5)
			{
				string diag = string.Format("Capture : passage de %1 ms, %2 joueur(s) valide(s), %3 ennemi(s), %4 carré(s) touché(s), %5 en cours",
					spentMs, m_aPlayerIds.Count(), m_aEnemyPos.Count(), touchedCount, m_aActive.Count());
				SRP_JournalComponent.Log("FRONT", diag);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Des carrés ont changé de camp (toute cause) : leurs deux compteurs à 0, retrait de m_aActive — socle, 1er appel
	//! direct après chaque lot (trou 6)
	void OnOwnersChanged(notnull array<int> cells)
	{
		int count = m_aCaptureMs.Count();
		foreach (int cell : cells)
		{
			if (cell < 0 || cell >= count)
				continue;
			ZeroCell(cell);
			// Un carré qui vient de basculer n'est plus contesté pour ce passage (PushLive enverra AUCUN)
			m_aLiveNow[cell] = SRP_EFrontLive.AUCUN;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! J3 : gel -> compteurs vidés et états contestés à AUCUN (le socle envoie la différence) — SRP_FrontComponent.SetFrozen
	void OnFrozenChanged(bool frozen)
	{
		ClearCounters();
		ClearLiveMirror();
		// Le socle fait ClearLive dans SetFrozen ; le premier passage gelé le refait une fois, par sûreté
		m_bLiveCleared = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à plat (SRP_EFrontReset) : tous les compteurs à 0 (ZONE : seulement les carrés de la zone), papier
	//! effacé — SRP_FrontComponent.NotifyReset
	void OnFrontReset(int kind, int zone)
	{
		if (kind == SRP_EFrontReset.ZONE && zone >= 0)
		{
			m_aZoneBuf.Clear();
			if (m_Front)
				m_Front.GetZoneCells(zone, m_aZoneBuf);
			int count = m_aCaptureMs.Count();
			foreach (int cell : m_aZoneBuf)
			{
				if (cell >= 0 && cell < count)
					ZeroCell(cell);
			}
			m_aZoneBuf.Clear();
			ClearPaperAssault(zone);
			for (int block = 0; block < m_aBlockMs.Count(); block++)
			{
				SRP_FrontLocality blockLocality = m_Front.GetLocality(m_aBlockLocality[block]);
				if (blockLocality && blockLocality.m_iZone == zone)
					m_aBlockMs[block] = 0;
			}
		}
		else
		{
			ClearCounters();
			ClearPaperAssault(-1);
		}
		// Le socle a pu effacer les états contestés : tout ce qui était envoyé repart au prochain passage
		ForgetLive();
	}

	//------------------------------------------------------------------------------------------------
	//! C3 : ancienne protection contre la téléportation, RETIRÉE à la demande de Jack le 27/09 : un joueur téléporté par
	//! le menu Staff compte tout de suite. Gardée vide pour l'appel de SRP_AdminComponent.Teleport
	static void NoteTeleport(int playerId)
	{
		// plus rien : la téléportation ne retire plus le joueur du recensement
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur part : on oublie sa dernière position et son exclusion — SRP_FrontComponent.OnPlayerDisconnected
	void OnPlayerDisconnected(int playerId)
	{
		m_mLastPos.Remove(playerId);
		m_mLastOnFoot.Remove(playerId);
		m_mExcludedUntil.Remove(playerId);
		ForgetCensus(playerId);
	}

	//================================================================================================
	// Services : le SEUL recensement du mod (données de moins de 5 s)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Joueurs valides (C2, C3) à moins de radius (XZ) — chute (D4, F5), ennemi (F4, postes), Commandeur (EQ4 :
	//! GetDosingPlayers, seul nombre réel qu'il lit)
	int CountValidPlayersNear(vector position, float radius)
	{
		int found = 0;
		foreach (vector spot : m_aPlayerPos)
		{
			if (vector.DistanceXZ(spot, position) <= radius)
				found++;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Joueurs valides dans le carré — ennemi, PC
	int CountValidPlayersInCell(int cell)
	{
		if (cell < 0)
			return 0;
		int found = 0;
		foreach (int playerCell : m_aPlayerCells)
		{
			if (playerCell == cell)
				found++;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Ce joueur compte-t-il (recensé valide à ce passage) ? — chute (TrySeize, C3), Commandeur
	bool IsPlayerCounted(int playerId)
	{
		if (playerId <= 0)
			return false;
		return m_aPlayerIds.Contains(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Ce personnage est-il un joueur qui compte (C2, C3) ? — Commandeur (informateurs, CO5)
	bool IsCharacterCounted(IEntity character)
	{
		if (!character)
			return false;
		return m_aPlayerEnts.Contains(character);
	}

	//------------------------------------------------------------------------------------------------
	//! Ennemis au sol en état de combattre à moins de radius (C6) — chute (D5 : poste, saisie), ennemi
	int CountFitEnemiesNear(vector position, float radius)
	{
		int found = 0;
		foreach (vector spot : m_aEnemyPos)
		{
			if (vector.DistanceXZ(spot, position) <= radius)
				found++;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Ennemis en état de combattre dans le carré (C6) — ennemi, Staff
	int CountFitEnemiesInCell(int cell)
	{
		if (cell < 0)
			return 0;
		int found = 0;
		foreach (int enemyCell : m_aEnemyCells)
		{
			if (enemyCell == cell)
				found++;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Ennemis en état de combattre dans la zone (I3 : n'est affiché que pour les zones au front) — PC, Staff
	int CountFitEnemiesInZone(int zone)
	{
		if (zone < 0 || !m_Front)
			return 0;
		int found = 0;
		foreach (int enemyCell : m_aEnemyCells)
		{
			if (enemyCell >= 0 && m_Front.GetCellZone(enemyCell) == zone)
				found++;
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Soldats de l'attaque (m_iAssaultZone == zone) en état de combattre dans la zone (ou sur le carré d'un de ses
	//! points clés posé dans une zone voisine, Q2), plus ceux sur le papier (CA6) — ennemi (F4 : zone nettoyée),
	//! SRP_CmdManeuvers
	int CountFitAssaultersInZone(int zone)
	{
		if (zone < 0 || !m_Front)
			return 0;
		int found = 0;
		int enemyCount = m_aEnemyCells.Count();
		for (int i = 0; i < enemyCount; i++)
		{
			int enemyCell = m_aEnemyCells[i];
			if (m_aEnemyAssault[i] != zone || enemyCell < 0)
				continue;
			if (m_Front.GetCellZone(enemyCell) == zone || IsKeyCellOfZone(enemyCell, zone))
				found++;
		}

		// Sur le papier : entrées fraîches de la zone (m_iPaperFreshSeconds)
		int nowUnix = System.GetUnixTime();
		int fresh = m_Front.m_iPaperFreshSeconds;
		int paperCount = m_aPaperZone.Count();
		for (int p = 0; p < paperCount; p++)
		{
			if (m_aPaperZone[p] == zone && nowUnix - m_aPaperUnix[p] < fresh)
				found += m_aPaperSoldiers[p];
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! F5 : un joueur valide à moins de m_fKeyPointGuardRadius d'un point clé de ce carré — ennemi, chute
	bool IsKeyCellGuarded(int cell)
	{
		if (cell < 0 || !m_Front)
			return false;
		// Registre global des points clés : un QG peut tomber dans un carré d'une zone voisine
		int total = m_Front.GetKeyPointTotal();
		for (int i = 0; i < total; i++)
		{
			SRP_FrontKeyPoint point = m_Front.GetKeyPoint(i);
			if (!point || point.m_iCell != cell)
				continue;
			if (CountValidPlayersNear(point.m_vPos, m_Front.m_fKeyPointGuardRadius) > 0)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! SRP_EFrontLive du carré au dernier passage — PC, Staff, Commandeur
	int GetLiveState(int cell)
	{
		if (cell < 0 || cell >= m_aLiveState.Count())
			return SRP_EFrontLive.AUCUN;
		int state = m_aLiveState[cell];
		if (state < 0)
			return SRP_EFrontLive.AUCUN;
		return state;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes de capture accumulées sur le carré (jamais affichées sur la carte, trou 20) — PC, Staff
	int GetCaptureSeconds(int cell)
	{
		if (cell < 0 || cell >= m_aCaptureMs.Count())
			return 0;
		if (InBlockCapture(cell))
			return m_aBlockMs[m_aCellBlock[cell]] / 1000;
		return m_aCaptureMs[cell] / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes nécessaires pour prendre un carré (m_iSquareCaptureMinutes x 60) — PC, Staff
	int GetCaptureTotalSeconds()
	{
		return CaptureNeedMs() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes nécessaires pour prendre CE carré : celles de son bloc s'il en a un, sinon celles d'un carré — Staff
	int GetCaptureNeedSeconds(int cell)
	{
		if (InBlockCapture(cell))
			return BlockNeedMs() / 1000;
		return CaptureNeedMs() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! « bloc de Régina, 18 carrés » si le carré est dans un bloc de localité, sinon "" — Staff, diagnostic
	string GetBlockText(int cell)
	{
		if (!InBlockCapture(cell))
			return "";
		int block = m_aCellBlock[cell];
		SRP_FrontLocality blockLocality = m_Front.GetLocality(m_aBlockLocality[block]);
		string name = "?";
		if (blockLocality)
			name = blockLocality.m_sName;
		return string.Format("bloc de %1, %2 carrés", name, m_aBlockCells[block].Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes de reprise accumulées sur le carré — Staff
	int GetRetakeSeconds(int cell)
	{
		if (cell < 0 || cell >= m_aRetakeMs.Count())
			return 0;
		return m_aRetakeMs[cell] / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! Secondes nécessaires pour reprendre un carré (m_iRetakeSeconds) — Staff
	int GetRetakeTotalSeconds()
	{
		return RetakeNeedMs() / 1000;
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : des assaillants sur le papier occupent ce carré pour la zone visée (entrée valable m_iPaperFreshSeconds)
	//! — SRP_CmdManeuvers (PaperTick)
	void SetPaperAssault(int cell, int soldiers, int zone, int unixTime)
	{
		if (cell < 0 || zone < 0)
			return;

		int paperCount = m_aPaperCell.Count();
		for (int i = 0; i < paperCount; i++)
		{
			if (m_aPaperCell[i] != cell || m_aPaperZone[i] != zone)
				continue;
			if (soldiers <= 0)
			{
				RemovePaperAt(i);
				return;
			}
			// Même passage de PaperTick : plusieurs unités dans le même carré s'additionnent
			if (m_aPaperUnix[i] == unixTime)
				m_aPaperSoldiers[i] = m_aPaperSoldiers[i] + soldiers;
			else
			{
				m_aPaperSoldiers[i] = soldiers;
				m_aPaperUnix[i] = unixTime;
			}
			return;
		}

		if (soldiers <= 0)
			return;
		m_aPaperCell.Insert(cell);
		m_aPaperSoldiers.Insert(soldiers);
		m_aPaperZone.Insert(zone);
		m_aPaperUnix.Insert(unixTime);
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : efface les assaillants sur le papier de la zone (fin d'attaque) — SRP_CmdManeuvers, ennemi (EndAttack)
	void ClearPaperAssault(int zone)
	{
		// zone < 0 : tout le papier (remise à plat)
		for (int i = m_aPaperZone.Count() - 1; i >= 0; i--)
		{
			if (zone < 0 || m_aPaperZone[i] == zone)
				RemovePaperAt(i);
		}
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Joueurs valides : Game Master ouvert dans un mode non limité ou possession -> exclusion ; invincible écarté ; filet du
	//! Staff (saut > m_fStaffJumpMeters à pied) ; exclu ; IsFitOnGround. Le délai de grâce de l'ennemi après l'apparition
	//! (SRP_EnemyComponent.InGrace) ne s'applique PLUS ici depuis le 27/09 : un joueur compte dès qu'il apparaît
	protected void CollectPlayers(int nowUnix)
	{
		m_aPlayerPos.Clear();
		m_aPlayerIds.Clear();
		m_aPlayerCells.Clear();
		m_aPlayerEnts.Clear();
		m_aIdBuf.Clear();

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;
		playerManager.GetPlayers(m_aIdBuf);

		// Une seule fois pour tout le passage
		SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
		SRP_PlayerManagerComponent records = SRP_PlayerManagerComponent.GetInstance();
		float jumpMeters = m_Front.m_fStaffJumpMeters;

		foreach (int playerId : m_aIdBuf)
		{
			// C3 : Game Master ouvert (le mode photo, limité, ne compte pas) ou possession d'une IA : jamais compté pendant,
			// et encore m_iExcludeMinutes après (0 par défaut)
			if (m_Front.m_bExcludeGameMaster && IsInEditor(playerId))
			{
				Exclude(playerId, nowUnix);
				continue;
			}

			// Staff en mode invincible : ne compte pas, sans exclusion
			if (m_Front.m_bExcludeGodMode && admin && admin.IsGod(playerId))
				continue;

			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			if (!entity || entity.IsDeleted())
			{
				m_mLastPos.Remove(playerId);
				m_mLastOnFoot.Remove(playerId);
				continue;
			}
			ChimeraCharacter character = ChimeraCharacter.Cast(entity);
			if (!character)
				continue;
			CharacterControllerComponent controller = character.GetCharacterController();
			if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			{
				// Mort ou inconscient : ne compte pas ; la réapparition ne sera pas prise pour un saut
				m_mLastPos.Remove(playerId);
				m_mLastOnFoot.Remove(playerId);
				continue;
			}

			vector pos = character.GetOrigin();

			// Filet de sécurité du Staff : à pied au passage précédent comme à celui-ci, un saut trop grand = téléporté
			if (jumpMeters > 0 && records && records.IsStaff(playerId))
			{
				bool onFoot = !character.IsInVehicle();
				vector lastPos;
				bool lastOnFoot;
				if (onFoot && m_mLastPos.Find(playerId, lastPos) && m_mLastOnFoot.Find(playerId, lastOnFoot) && lastOnFoot)
				{
					if (vector.DistanceXZ(lastPos, pos) > jumpMeters)
						Exclude(playerId, nowUnix);
				}
				m_mLastPos.Set(playerId, pos);
				m_mLastOnFoot.Set(playerId, onFoot);
			}

			// Exclu (C3) : jusqu'à l'heure notée
			int until;
			if (m_mExcludedUntil.Find(playerId, until))
			{
				if (nowUnix < until)
					continue;
				m_mExcludedUntil.Remove(playerId);
			}

			// C2 : au sol ou dans un véhicule bas
			if (!IsFitOnGround(entity))
				continue;

			int cell = m_Front.CellIndexAt(pos);
			m_aPlayerPos.Insert(pos);
			m_aPlayerIds.Insert(playerId);
			m_aPlayerCells.Insert(cell);
			m_aPlayerEnts.Insert(entity);
			if (cell >= 0)
				AddCount(m_aPlayersIn, cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Ennemis en état de combattre : SRP_EnemyComponent.GetGroups(), sans l'hélico ; comptes par carré et soldats de
	//! l'attaque de la zone visée (m_bRetakeTargetZoneOnly)
	protected void CollectEnemies()
	{
		m_aEnemyPos.Clear();
		m_aEnemyAssault.Clear();
		m_aEnemyCells.Clear();

		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies)
			return;
		array<ref SRP_EnemyGroup> groups = enemies.GetGroups();
		if (!groups)
			return;
		bool targetOnly = m_Front.m_bRetakeTargetZoneOnly;

		foreach (SRP_EnemyGroup record : groups)
		{
			if (!record || !record.m_Group || record.m_Group.IsDeleted() || record.m_sOwner == "helico")
				continue;

			m_aAgentBuf.Clear();
			record.m_Group.GetAgents(m_aAgentBuf);
			int assaultZone = record.m_iAssaultZone;
			foreach (AIAgent agent : m_aAgentBuf)
			{
				if (!agent)
					continue;
				IEntity soldier = agent.GetControlledEntity();
				if (!soldier || soldier.IsDeleted() || !IsFitOnGround(soldier) || IsCaptive(soldier))
					continue;

				vector spot = soldier.GetOrigin();
				int cell = m_Front.CellIndexAt(spot);
				m_aEnemyPos.Insert(spot);
				m_aEnemyAssault.Insert(assaultZone);
				m_aEnemyCells.Insert(cell);
				if (cell < 0)
					continue;

				AddCount(m_aEnemiesIn, cell);
				// Zone visée : ses carrés, plus le carré d'un de ses points clés posé dans une zone voisine (QG, Q2)
				if (assaultZone >= 0 && (!targetOnly || m_Front.GetCellZone(cell) == assaultZone || IsKeyCellOfZone(cell, assaultZone)))
					AddCount(m_aRetakersIn, cell);
			}
		}
		m_aAgentBuf.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! CA6 : entrées de papier périmées retirées, carrés des entrées fraîches marqués touchés (évalués ce passage) — Tick
	protected void CollectPaper(int nowUnix)
	{
		int fresh = m_Front.m_iPaperFreshSeconds;
		int count = m_aTouchStamp.Count();
		for (int i = m_aPaperCell.Count() - 1; i >= 0; i--)
		{
			if (nowUnix - m_aPaperUnix[i] >= fresh)
			{
				RemovePaperAt(i);
				continue;
			}
			int cell = m_aPaperCell[i];
			if (cell >= 0 && cell < count)
				Touch(cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! MODE ESSAI du Workbench seulement (27/09) : pour chaque joueur, une ligne console « Front (essai) » quand son état
	//! change, et toutes les 30 s : compté ou non (et pourquoi), carré, zone, camp, prise possible ou pourquoi pas,
	//! ennemis dans le carré, avancement. Rien au journal ni sur Discord — Tick, après le recensement
	protected void DiagPlayers(int nowUnix)
	{
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
			return;
		bool everyone = nowUnix - m_iDiagUnix >= 30;
		if (everyone)
			m_iDiagUnix = nowUnix;

		array<int> ids = {};
		playerManager.GetPlayers(ids);
		SRP_AdminComponent admin = SRP_AdminComponent.GetInstance();
		SRP_ZoneFall zoneFall = m_Front.GetZoneFall();
		int needMs = CaptureNeedMs();

		foreach (int playerId : ids)
		{
			string status;
			string progress = "";
			IEntity entity = playerManager.GetPlayerControlledEntity(playerId);
			int index = m_aPlayerIds.Find(playerId);
			if (!entity)
			{
				status = "NON COMPTÉ : pas de personnage (menu de réapparition)";
			}
			else if (index < 0)
			{
				// Pas dans le recensement : la même suite de tests que CollectPlayers, pour dire pourquoi
				string where = m_Front.CellRef(m_Front.CellIndexAt(entity.GetOrigin()));
				ChimeraCharacter character = ChimeraCharacter.Cast(entity);
				CharacterControllerComponent controller;
				if (character)
					controller = character.GetCharacterController();
				int until;
				if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
					status = "NON COMPTÉ : mort ou inconscient";
				else if (m_Front.m_bExcludeGameMaster && IsInEditor(playerId))
					status = "NON COMPTÉ : Game Master ouvert ou possession d'une IA";
				else if (m_mExcludedUntil.Find(playerId, until) && nowUnix < until)
					status = string.Format("NON COMPTÉ : encore %1 s après le Game Master ou une possession (C3)", until - nowUnix);
				else if (m_Front.m_bExcludeGodMode && admin && admin.IsGod(playerId))
					status = "NON COMPTÉ : mode invincible du Staff";
				else if (!IsFitOnGround(entity))
					status = "NON COMPTÉ : véhicule en l'air (C2)";
				else
					status = "NON COMPTÉ : raison inconnue";
				status = status + ", carré " + where;
			}
			else
			{
				int cell = m_aPlayerCells[index];
				if (cell < 0)
				{
					status = "compté, en mer ou hors de la grille";
				}
				else
				{
					int zone = m_Front.GetCellZone(cell);
					int owner = m_Front.GetCellOwner(cell);
					string head = "compté, carré " + m_Front.CellRef(cell) + ", " + m_Front.GetZoneLabel(zone);
					if (owner == SRP_EFrontOwner.BLEU)
						status = head + ", bleu (déjà à nous)";
					else if (owner != SRP_EFrontOwner.ROUGE)
						status = head + ", hors jeu";
					else if (m_Front.IsFrozen())
						status = head + ", rouge : front gelé, rien ne se prend";
					else if (m_Front.IsBaseCell(cell))
						status = head + ", rouge dans la base (jamais pris)";
					else if (InBlockCapture(cell))
					{
						int blockPlayers;
						int blockFoes;
						int blockRed;
						bool blockFront;
						int block = m_aCellBlock[cell];
						BlockCounts(block, blockPlayers, blockFoes, blockRed, blockFront);
						status = head + ", rouge, " + GetBlockText(cell);
						if (!blockFront)
							status = status + " : PAS AU FRONT (aucun carré rouge du bloc ne touche le bleu)";
						else if (blockFoes > 0)
							status = status + string.Format(" : COMBAT, %1 ennemi(s) dans le bloc, prise figée", blockFoes);
						else
							status = status + " : PRISE DU BLOC EN COURS";
						if (m_aBlockMs[block] > 0)
							progress = string.Format(" (%1/%2 s)", m_aBlockMs[block] / 1000, BlockNeedMs() / 1000);
					}
					else if (!m_Front.HasNeighbourOwnedBy(cell, SRP_EFrontOwner.BLEU, true))
						status = head + ", rouge : PAS AU FRONT (aucun carré bleu voisin, il faut avancer depuis le bleu)";
					else if (zoneFall && zoneFall.IsCellSeizeOnly(cell))
						status = head + ", rouge : carré du point clé, il se prend en saisissant le poste de commandement";
					else if (m_aEnemiesIn[cell] > 0)
						status = head + string.Format(", rouge au front : COMBAT, %1 ennemi(s) dans le carré, prise figée", m_aEnemiesIn[cell]);
					else
						status = head + ", rouge au front : PRISE EN COURS";
					if (m_aCaptureMs[cell] > 0)
						progress = string.Format(" (%1/%2 s)", m_aCaptureMs[cell] / 1000, needMs / 1000);
				}
			}

			string said;
			bool changed = !m_mDiagSaid.Find(playerId, said) || said != status;
			if (!changed && !everyone)
				continue;
			m_mDiagSaid.Set(playerId, status);
			Print("[SRP] Front (essai) : " + playerManager.GetPlayerName(playerId) + " — " + status + progress, LogLevel.NORMAL);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! C3 : Game Master ouvert (non limité ; le mode photo ne compte pas, même pour un Staff qui a le Game Master) ou
	//! possession d'une IA — CollectPlayers, DiagPlayers
	protected bool IsInEditor(int playerId)
	{
		SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
		if (core)
		{
			SCR_EditorManagerEntity editor = core.GetEditorManager(playerId);
			if (editor && editor.IsOpened())
			{
				SCR_EditorModeEntity mode = editor.GetCurrentModeEntity();
				if (!mode || !mode.IsLimited())
					return true;
			}
		}
		SCR_PossessingManagerComponent possess = SCR_PossessingManagerComponent.GetInstance();
		return possess && possess.IsPossessing(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Vivant (ALIVE), au sol ou dans un véhicule à moins de m_fMaxVehicleAltitude (C2, C6 ; filtre de
	//! SCR_SeizingComponent) — CollectPlayers, CollectEnemies
	protected bool IsFitOnGround(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;
		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller || controller.GetLifeState() != ECharacterLifeState.ALIVE)
			return false;	// mort ou inconscient
		// true : la surface de la mer compte comme le sol (bateau)
		if (character.IsInVehicle() && SCR_TerrainHelper.GetHeightAboveTerrain(character.GetOrigin(), null, true) > m_Front.m_fMaxVehicleAltitude)
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! C6 : un soldat menotté (ACE Captives) n'est plus en état de combattre — CollectEnemies
	protected bool IsCaptive(IEntity soldier)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(soldier);
		if (!character)
			return false;
		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.GetCharacterController());
		return controller && controller.ACE_Captives_IsCaptive();
	}

	//------------------------------------------------------------------------------------------------
	//! Marque le carré touché ce passage, puis counts[cell] = counts[cell] + 1 — Collect*
	protected void AddCount(array<int> counts, int cell)
	{
		Touch(cell);
		counts[cell] = counts[cell] + 1;
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré est à évaluer ce passage (une seule entrée dans m_aTouched) — AddCount, CollectPaper
	protected void Touch(int cell)
	{
		if (m_aTouchStamp[cell] == m_iStamp)
			return;
		m_aTouchStamp[cell] = m_iStamp;
		m_aTouched.Insert(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Parcourt m_aTouched puis m_aActive (une seule fois chacun) : capture (rouge : CanBlueCapture et
	//! !IsCellSeizeOnly, joueurs sans ennemi), combat figé (C7), reprise (bleu : F3, F5, CanRedRetake), redescente
	protected void EvaluateCells(int tickMs)
	{
		int needCapture = CaptureNeedMs();
		int needRetake = RetakeNeedMs();

		foreach (int cell : m_aTouched)
		{
			EvaluateCell(cell, tickMs, needCapture, needRetake);
		}

		// Carrés en cours sans personne ce passage : leur progression redescend (m_aActive change pendant le parcours)
		m_aScan.Copy(m_aActive);
		foreach (int activeCell : m_aScan)
		{
			if (m_aTouchStamp[activeCell] == m_iStamp)
				continue;	// déjà évalué avec les carrés touchés
			EvaluateCell(activeCell, tickMs, needCapture, needRetake);
		}
		m_aScan.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Un carré : capture ou combat (rouge), reprise ou combat (bleu), sinon redescente ; état contesté noté — EvaluateCells
	protected void EvaluateCell(int cell, int tickMs, int needCapture, int needRetake)
	{
		int owner = m_Front.GetCellOwner(cell);
		int players = m_aPlayersIn[cell];
		int foes = m_aEnemiesIn[cell];
		int live = SRP_EFrontLive.AUCUN;

		if (owner == SRP_EFrontOwner.ROUGE)
		{
			m_aRetakeMs[cell] = 0;
			if (InBlockCapture(cell))
			{
				// Carré d'un bloc de localité : il se prend avec tout son bloc (EvaluateBlocks)
				m_aCaptureMs[cell] = 0;
				UpdateActive(cell);
				return;
			}
			if (players > 0 && foes == 0 && IsCapturable(cell))
			{
				// C4, C5 : même durée pour 1 ou 10 joueurs
				m_aCaptureMs[cell] = m_aCaptureMs[cell] + tickMs;
				live = SRP_EFrontLive.CAPTURE;
				if (m_aCaptureMs[cell] >= needCapture && !m_aCaptureFlips.Contains(cell))
					m_aCaptureFlips.Insert(cell);
			}
			else if (players > 0 && foes > 0)
			{
				// C7 : figée pendant le combat
				live = SRP_EFrontLive.COMBAT;
			}
			else
			{
				m_aCaptureMs[cell] = Decay(m_aCaptureMs[cell], tickMs, m_Front.m_fCaptureDecayRatio);
			}
		}
		else if (owner == SRP_EFrontOwner.BLEU)
		{
			m_aCaptureMs[cell] = 0;

			// F3 : soldats de l'attaque dans le carré, plus ceux sur le papier (CA6)
			int assault = m_aRetakersIn[cell] + PaperAssaultersAt(cell, m_Front.m_bRetakeTargetZoneOnly, m_iNowUnix);
			bool eligible = assault > 0 && IsRetakeable(cell);
			bool guarded = false;
			if (eligible)
				guarded = players > 0 || IsKeyCellGuarded(cell);	// sans joueur dans le carré ; F5 sur le carré du point clé

			if (eligible && !guarded)
			{
				m_aRetakeMs[cell] = m_aRetakeMs[cell] + tickMs;
				live = SRP_EFrontLive.REPRISE;
				if (m_aRetakeMs[cell] >= needRetake && !m_aRetakeFlips.Contains(cell))
					m_aRetakeFlips.Insert(cell);
			}
			else if (eligible || (players > 0 && foes > 0))
			{
				// Tenu par un joueur ou au combat : la reprise est figée
				live = SRP_EFrontLive.COMBAT;
			}
			else
			{
				m_aRetakeMs[cell] = Decay(m_aRetakeMs[cell], tickMs, m_Front.m_fRetakeDecayRatio);
			}
		}
		else
		{
			// Hors jeu : rien ne se prend
			m_aCaptureMs[cell] = 0;
			m_aRetakeMs[cell] = 0;
		}

		UpdateActive(cell);
		if (live != SRP_EFrontLive.AUCUN)
		{
			m_aLiveNow[cell] = live;
			m_aLiveNowList.Insert(cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Blocs de localité (choix de Jack du 27/09) : pour chaque localité hors base dont le genre est dans m_sBlockKinds,
	//! les carrés de SA zone dont le centre est à moins de m_fBlockRadius de son centre (m_vLabel) ; un bloc de moins de
	//! 2 carrés est ignoré ; progressions remises à zéro — Start, SRP_FrontComponent.ReloadSettings
	void BuildBlocks()
	{
		m_aBlockCells.Clear();
		m_aBlockLocality.Clear();
		m_aBlockMs.Clear();
		if (!m_Front)
			return;
		int count = m_Front.GetCellCount();
		if (count <= 0)
			return;
		Fill(m_aCellBlock, count, -1);
		if (m_Front.m_fBlockRadius <= 0)
			return;

		string kindText = m_Front.m_sBlockKinds;
		kindText.ToLower();
		array<string> rawKinds = {};
		kindText.Split(",", rawKinds, true);
		array<string> kinds = {};
		foreach (string rawKind : rawKinds)
		{
			kinds.Insert(rawKind.Trim());
		}

		array<int> zoneCells = {};
		int localities = m_Front.GetLocalityCount();
		for (int index = 0; index < localities; index++)
		{
			SRP_FrontLocality locality = m_Front.GetLocality(index);
			if (!locality || locality.m_bHome || locality.m_iZone < 0 || !kinds.Contains(locality.m_sKind))
				continue;
			zoneCells.Clear();
			m_Front.GetZoneCells(locality.m_iZone, zoneCells);
			array<int> members = new array<int>();
			foreach (int cell : zoneCells)
			{
				if (cell < 0 || cell >= count || m_aCellBlock[cell] >= 0 || m_Front.IsBaseCell(cell))
					continue;
				if (vector.DistanceXZ(m_Front.CellCenter(cell), locality.m_vLabel) > m_Front.m_fBlockRadius)
					continue;
				members.Insert(cell);
			}
			if (members.Count() < 2)
				continue;
			int block = m_aBlockCells.Count();
			foreach (int member : members)
			{
				m_aCellBlock[member] = block;
			}
			m_aBlockCells.Insert(members);
			m_aBlockLocality.Insert(index);
			m_aBlockMs.Insert(0);
		}
		Print(string.Format("[SRP] Front : %1 bloc(s) de localité (genres « %2 », rayon %3 m, prise en %4 min)", m_aBlockCells.Count(), m_Front.m_sBlockKinds, m_Front.m_fBlockRadius, m_Front.m_iBlockMinutes), LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Carré rouge d'un bloc qui se prend avec son bloc (jamais le carré d'un point clé encore à saisir, D4) —
	//! EvaluateCell, services
	protected bool InBlockCapture(int cell)
	{
		if (cell < 0 || cell >= m_aCellBlock.Count() || m_aCellBlock[cell] < 0)
			return false;
		// Le bloc sert à PRENDRE la localité : zone à nous, un carré repris par l'ennemi se reprend seul (1 min)
		if (m_Front.GetZoneOwner(m_Front.GetCellZone(cell)) != SRP_EFrontOwner.ROUGE)
			return false;
		SRP_ZoneFall zoneFall = m_Front.GetZoneFall();
		return !zoneFall || !zoneFall.IsCellSeizeOnly(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Comptes d'un bloc ce passage : joueurs et ennemis dans tous ses carrés, carrés rouges à prendre, et si l'un d'eux
	//! touche du bleu (CanBlueCapture) — EvaluateBlocks, DiagPlayers
	protected void BlockCounts(int block, out int players, out int foes, out int red, out bool atFront)
	{
		players = 0;
		foes = 0;
		red = 0;
		atFront = false;
		SRP_ZoneFall zoneFall = m_Front.GetZoneFall();
		foreach (int cell : m_aBlockCells[block])
		{
			players += m_aPlayersIn[cell];
			foes += m_aEnemiesIn[cell];
			if (m_Front.GetCellOwner(cell) != SRP_EFrontOwner.ROUGE)
				continue;
			if (zoneFall && zoneFall.IsCellSeizeOnly(cell))
				continue;
			red++;
			if (!atFront && m_Front.CanBlueCapture(cell))
				atFront = true;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque passage, après EvaluateCells : un bloc avance quand des joueurs sont dedans, sans ennemi dedans, et qu'un
	//! de ses carrés rouges touche du bleu ; au combat il est figé, sinon il redescend ; à m_iBlockMinutes, tous ses
	//! carrés rouges (hors points clés) basculent ensemble. Ses carrés rouges portent l'état du bloc (la carte les fait
	//! clignoter ensemble) — Tick
	protected void EvaluateBlocks(int tickMs)
	{
		int need = BlockNeedMs();
		SRP_ZoneFall zoneFall = m_Front.GetZoneFall();
		for (int block = 0; block < m_aBlockCells.Count(); block++)
		{
			// Zone déjà à nous : le bloc ne sert plus (ses carrés repris se reprennent un à un)
			SRP_FrontLocality blockLocality = m_Front.GetLocality(m_aBlockLocality[block]);
			if (!blockLocality || m_Front.GetZoneOwner(blockLocality.m_iZone) != SRP_EFrontOwner.ROUGE)
			{
				m_aBlockMs[block] = 0;
				continue;
			}

			int players;
			int foes;
			int red;
			bool atFront;
			BlockCounts(block, players, foes, red, atFront);
			if (red == 0)
			{
				m_aBlockMs[block] = 0;
				continue;
			}

			int live = SRP_EFrontLive.AUCUN;
			if (players > 0 && atFront && foes == 0)
			{
				m_aBlockMs[block] = m_aBlockMs[block] + tickMs;
				live = SRP_EFrontLive.CAPTURE;
			}
			else if (players > 0 && atFront && foes > 0)
			{
				live = SRP_EFrontLive.COMBAT;
			}
			else
			{
				m_aBlockMs[block] = Decay(m_aBlockMs[block], tickMs, m_Front.m_fCaptureDecayRatio);
			}
			if (live == SRP_EFrontLive.AUCUN)
				continue;

			bool done = live == SRP_EFrontLive.CAPTURE && m_aBlockMs[block] >= need;
			if (done)
				m_aBlockMs[block] = 0;
			foreach (int member : m_aBlockCells[block])
			{
				if (m_Front.GetCellOwner(member) != SRP_EFrontOwner.ROUGE)
					continue;
				if (zoneFall && zoneFall.IsCellSeizeOnly(member))
					continue;
				if (done && !m_aCaptureFlips.Contains(member))
					m_aCaptureFlips.Insert(member);
				m_aLiveNow[member] = live;
				m_aLiveNowList.Insert(member);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le carré appartient-il à un bloc de localité (points clés compris) ? — Commandeur (DiffCells : un bloc pris
	//! compte comme UN carré perdu)
	bool IsBlockCell(int cell)
	{
		return cell >= 0 && cell < m_aCellBlock.Count() && m_aCellBlock[cell] >= 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Millisecondes de présence pour prendre un bloc de localité (m_iBlockMinutes), 1 s au moins
	protected int BlockNeedMs()
	{
		return Math.MaxInt(1000, m_Front.m_iBlockMinutes * 60000);
	}

	//------------------------------------------------------------------------------------------------
	//! C1 + trou 12 : carré rouge prenable par présence (voisin bleu ou liaison maritime, hors base, front non gelé,
	//! jamais le carré d'un point clé encore hostile) — EvaluateCell
	protected bool IsCapturable(int cell)
	{
		if (!m_Front.CanBlueCapture(cell))
			return false;
		SRP_ZoneFall zoneFall = m_Front.GetZoneFall();
		if (zoneFall && zoneFall.IsCellSeizeOnly(cell))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! F3 + Q2 + B4 : carré bleu que l'ennemi peut reprendre (hors base et zone protégée, touche du rouge par un
	//! côté si m_bRetakeNeedsRed) — EvaluateCell
	protected bool IsRetakeable(int cell)
	{
		if (!m_Front.CanRedRetake(cell))
			return false;
		if (m_Front.m_bRetakeNeedsRed && !m_Front.HasNeighbourOwnedBy(cell, SRP_EFrontOwner.ROUGE, false))
			return false;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! C7 : redescente d'une progression, au rythme ratio x la montée ; jamais sous 0 — EvaluateCell
	protected int Decay(int progressMs, int tickMs, float ratio)
	{
		if (progressMs <= 0)
			return 0;
		int drop = Math.Round(tickMs * ratio);
		return Math.MaxInt(0, progressMs - drop);
	}

	//------------------------------------------------------------------------------------------------
	//! Tient m_aActive à jour : un carré y entre quand un compteur passe au-dessus de 0, en sort quand les deux sont nuls
	protected void UpdateActive(int cell)
	{
		bool running = m_aCaptureMs[cell] > 0 || m_aRetakeMs[cell] > 0;
		if (running && m_aInActive[cell] == 0)
		{
			m_aInActive[cell] = 1;
			m_aActive.Insert(cell);
		}
		else if (!running && m_aInActive[cell] != 0)
		{
			m_aInActive[cell] = 0;
			m_aActive.RemoveItem(cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les deux compteurs d'un carré à 0 et retrait de m_aActive — OnOwnersChanged, OnFrontReset, ApplyFlips
	protected void ZeroCell(int cell)
	{
		m_aCaptureMs[cell] = 0;
		m_aRetakeMs[cell] = 0;
		UpdateActive(cell);
	}

	//------------------------------------------------------------------------------------------------
	//! Tous les compteurs à 0 (seuls les carrés en cours en ont) et lots vidés — gel, remise à plat
	protected void ClearCounters()
	{
		int count = m_aCaptureMs.Count();
		foreach (int cell : m_aActive)
		{
			if (cell < 0 || cell >= count)
				continue;
			m_aCaptureMs[cell] = 0;
			m_aRetakeMs[cell] = 0;
			m_aInActive[cell] = 0;
		}
		m_aActive.Clear();
		m_aCaptureFlips.Clear();
		m_aRetakeFlips.Clear();
		for (int block = 0; block < m_aBlockMs.Count(); block++)
			m_aBlockMs[block] = 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Remet à 0 les comptes des seuls carrés touchés (jamais les 4096) — Tick
	protected void ResetTouched()
	{
		foreach (int cell : m_aTouched)
		{
			m_aPlayersIn[cell] = 0;
			m_aEnemiesIn[cell] = 0;
			m_aRetakersIn[cell] = 0;
		}
		m_aTouched.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Deux appels au socle, chacun avec une COPIE du lot : SetCellsOwner(captures, BLEU, CAPTURE, "") puis
	//! SetCellsOwner(reprises, ROUGE, REPRISE, "") ; aucune annonce ici (la radio reçoit QueueCell du socle) — Tick
	protected void ApplyFlips()
	{
		if (!m_aCaptureFlips.IsEmpty())
		{
			array<int> captures = {};
			captures.Copy(m_aCaptureFlips);
			m_aCaptureFlips.Clear();
			m_Front.SetCellsOwner(captures, SRP_EFrontOwner.BLEU, SRP_EFrontReason.CAPTURE, "");
			// Refus du socle (cas anormal, déjà au journal) : on repart de zéro plutôt que de redemander toutes les 5 s
			foreach (int taken : captures)
			{
				if (m_Front.GetCellOwner(taken) != SRP_EFrontOwner.BLEU)
					ZeroCell(taken);
			}
		}

		if (!m_aRetakeFlips.IsEmpty())
		{
			array<int> retakes = {};
			retakes.Copy(m_aRetakeFlips);
			m_aRetakeFlips.Clear();
			m_Front.SetCellsOwner(retakes, SRP_EFrontOwner.ROUGE, SRP_EFrontReason.REPRISE, "");
			foreach (int lost : retakes)
			{
				if (m_Front.GetCellOwner(lost) != SRP_EFrontOwner.ROUGE)
					ZeroCell(lost);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Différence d'état contesté avec m_aLiveState (un carré plus évalué repasse à AUCUN), puis
	//! m_Front.SetLiveCells si non vide — Tick
	protected void PushLive()
	{
		m_aDiffCells.Clear();
		m_aDiffStates.Clear();

		// États de ce passage qui diffèrent du dernier envoi
		foreach (int cell : m_aLiveNowList)
		{
			int state = m_aLiveNow[cell];
			if (state == SRP_EFrontLive.AUCUN)
				continue;	// carré basculé pendant le passage : traité avec les anciens envois
			if (state != m_aLiveState[cell])
			{
				m_aDiffCells.Insert(cell);
				m_aDiffStates.Insert(state);
				m_aLiveState[cell] = state;
			}
		}

		// Carrés contestés au dernier envoi qui ne le sont plus (ou dont l'état est inconnu) : AUCUN
		foreach (int sentCell : m_aLiveSentList)
		{
			if (m_aLiveNow[sentCell] != SRP_EFrontLive.AUCUN)
				continue;
			if (m_aLiveState[sentCell] != SRP_EFrontLive.AUCUN)
			{
				m_aDiffCells.Insert(sentCell);
				m_aDiffStates.Insert(SRP_EFrontLive.AUCUN);
				m_aLiveState[sentCell] = SRP_EFrontLive.AUCUN;
			}
		}

		// Nouveau dernier envoi, et état du passage remis à AUCUN
		m_aLiveSentList.Clear();
		foreach (int nowCell : m_aLiveNowList)
		{
			if (m_aLiveNow[nowCell] != SRP_EFrontLive.AUCUN)
				m_aLiveSentList.Insert(nowCell);
			m_aLiveNow[nowCell] = SRP_EFrontLive.AUCUN;
		}
		m_aLiveNowList.Clear();

		if (!m_aDiffCells.IsEmpty())
			m_Front.SetLiveCells(m_aDiffCells, m_aDiffStates);
	}

	//------------------------------------------------------------------------------------------------
	//! Le socle a tout remis à AUCUN (ClearLive) : notre dernier envoi aussi — gel
	protected void ClearLiveMirror()
	{
		int count = m_aLiveState.Count();
		foreach (int cell : m_aLiveSentList)
		{
			if (cell >= 0 && cell < count)
				m_aLiveState[cell] = SRP_EFrontLive.AUCUN;
		}
		m_aLiveSentList.Clear();
		foreach (int nowCell : m_aLiveNowList)
		{
			if (nowCell >= 0 && nowCell < count)
				m_aLiveNow[nowCell] = SRP_EFrontLive.AUCUN;
		}
		m_aLiveNowList.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Remise à plat : on ne sait plus ce que le socle affiche ; les carrés envoyés sont marqués inconnus (-1), le
	//! prochain PushLive renvoie leur état réel (AUCUN ou contesté) — OnFrontReset
	protected void ForgetLive()
	{
		int count = m_aLiveState.Count();
		foreach (int cell : m_aLiveSentList)
		{
			if (cell >= 0 && cell < count)
				m_aLiveState[cell] = -1;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! m_mExcludedUntil[playerId] = nowUnix + m_iExcludeMinutes x 60 — NoteTeleport, CollectPlayers
	protected void Exclude(int playerId, int nowUnix)
	{
		if (playerId <= 0)
			return;
		int minutes = Math.MaxInt(0, m_Front.m_iExcludeMinutes);
		m_mExcludedUntil.Set(playerId, nowUnix + minutes * 60);
		// Hors du recensement tout de suite (une saisie ou une garde ne le compte plus d'ici au prochain passage)
		if (minutes > 0)
			ForgetCensus(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Retire un joueur du recensement du passage (tableaux parallèles, même retrait partout) — Exclude, départ
	protected void ForgetCensus(int playerId)
	{
		int index = m_aPlayerIds.Find(playerId);
		if (index < 0)
			return;
		m_aPlayerIds.Remove(index);
		m_aPlayerPos.Remove(index);
		m_aPlayerCells.Remove(index);
		m_aPlayerEnts.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Assaillants sur le papier pour ce carré, entrée de moins de m_iPaperFreshSeconds ; targetOnly : seulement ceux
	//! dont la zone visée est celle du carré ou possède un point clé sur ce carré (Q2) — EvaluateCell
	protected int PaperAssaultersAt(int cell, bool targetOnly, int nowUnix)
	{
		int found = 0;
		int fresh = m_Front.m_iPaperFreshSeconds;
		int cellZone = m_Front.GetCellZone(cell);
		int paperCount = m_aPaperCell.Count();
		for (int i = 0; i < paperCount; i++)
		{
			if (m_aPaperCell[i] != cell)
				continue;
			if (nowUnix - m_aPaperUnix[i] >= fresh)
				continue;
			int paperZone = m_aPaperZone[i];
			if (targetOnly && paperZone != cellZone && !IsKeyCellOfZone(cell, paperZone))
				continue;
			found += m_aPaperSoldiers[i];
		}
		return found;
	}

	//------------------------------------------------------------------------------------------------
	//! Q2 : le carré porte un point clé de la zone. Un QG posé dans un carré d'une zone voisine compte pour la zone de
	//! sa ville (SRP_FrontGeometry) : l'attaque de cette zone doit pouvoir le reprendre — CollectEnemies,
	//! PaperAssaultersAt, CountFitAssaultersInZone
	protected bool IsKeyCellOfZone(int cell, int zone)
	{
		if (cell < 0 || zone < 0 || !m_Front)
			return false;
		int keys = m_Front.GetKeyPointCount(zone);
		for (int k = 0; k < keys; k++)
		{
			if (m_Front.GetKeyPointCell(zone, k) == cell)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire l'entrée de papier i (même retrait dans les quatre tableaux parallèles) — papier
	protected void RemovePaperAt(int index)
	{
		m_aPaperCell.Remove(index);
		m_aPaperSoldiers.Remove(index);
		m_aPaperZone.Remove(index);
		m_aPaperUnix.Remove(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Millisecondes de présence pour prendre un carré (C4, m_iSquareCaptureMinutes), 1 s au moins
	protected int CaptureNeedMs()
	{
		if (!m_Front)
			return 60000;
		return Math.MaxInt(1000, m_Front.m_iSquareCaptureMinutes * 60000);
	}

	//------------------------------------------------------------------------------------------------
	//! Millisecondes d'occupation pour reprendre un carré (F3, m_iRetakeSeconds), 1 s au moins
	protected int RetakeNeedMs()
	{
		if (!m_Front)
			return 60000;
		return Math.MaxInt(1000, m_Front.m_iRetakeSeconds * 1000);
	}

	//------------------------------------------------------------------------------------------------
	//! Tableau par carré redimensionné et rempli d'une valeur — Start
	protected void Fill(array<int> values, int count, int value)
	{
		values.Resize(count);
		for (int i = 0; i < count; i++)
			values[i] = value;
	}
}
