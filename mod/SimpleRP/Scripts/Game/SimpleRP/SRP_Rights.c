//------------------------------------------------------------------------------------------------
// SimpleRP — Ce que chaque joueur a le droit de faire, pour n'afficher que les actions possibles
// Une action (« Gestion du soldat », « Établir un point de contrôle ici », « Contrôler » un conducteur) s'affiche chez le
// joueur, dont le jeu ne connaît ni la fiche ni l'état des missions. Toutes les 5 s (SRP_PlayerManagerComponent.
// RefreshPositions), le serveur calcule pour chaque joueur connecté ce qu'il peut faire, avec les MÊMES vérifications
// qu'à l'exécution, et l'envoie à son écran quand ça change (SCR_PlayerController.SRP_SetRights), et de toute façon
// toutes les 30 s (un envoi perdu se rattrape). L'action n'apparaît
// que si le droit est accordé ; à l'exécution, le serveur revérifie tout comme avant.
//------------------------------------------------------------------------------------------------

class SRP_Rights
{
	static const int GESTION = 1;		// menu « Gestion du soldat » : Staff, officier, Adjudant, instructeur habilité
	static const int CONTROLE = 2;		// « Établir un point de contrôle ici » : toutes les conditions réunies, ici et maintenant
	static const int AU_CONTROLE = 4;	// « Contrôler » un conducteur : près d'un point de contrôle routier en place
	static const float AU_CONTROLE_RAYON = 1000;	// un barrage forcé est suivi jusqu'à 900 m du point, un fuyard jusqu'à 500 m

	protected static int s_iResendTicks;	// serveur : toutes les 6 passes (30 s), on renvoie tout, au cas où un envoi se serait perdu

	//------------------------------------------------------------------------------------------------
	//! Serveur, toutes les 5 s : calcule les droits de chaque joueur connecté et envoie ceux qui ont changé
	static void Tick()
	{
		if (!Replication.IsServer())
			return;
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;
		SRP_MissionManagerComponent missions = SRP_MissionManagerComponent.GetInstance();

		s_iResendTicks++;
		bool resend = s_iResendTicks >= 6;
		if (resend)
			s_iResendTicks = 0;

		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);
		foreach (int index, int playerId : ids)
		{
			int rights = Compute(players, missions, playerId, records[index]);
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (!pc)
				continue;
			// La valeur gardée par le contrôleur côté serveur fait foi : un contrôleur neuf (reconnexion, nouvelle partie) vaut 0
			if (!resend && pc.SRP_GetRights() == rights)
				continue;
			pc.SRP_SetRights(rights);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Les droits d'un joueur, côté serveur
	static int Compute(SRP_PlayerManagerComponent players, SRP_MissionManagerComponent missions, int playerId, SRP_PlayerRecord record)
	{
		if (!record)
			return 0;
		int rights = 0;
		if (CanManage(players, playerId, record))
			rights = rights | GESTION;
		if (missions && missions.CanStartControl(playerId))
			rights = rights | CONTROLE;
		IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (character && SRP_Checkpoint.IsNearActive(character.GetOrigin(), AU_CONTROLE_RAYON))
			rights = rights | AU_CONTROLE;
		return rights;
	}

	//------------------------------------------------------------------------------------------------
	//! A-t-il au moins un droit dans le menu « Gestion du soldat » ? (mêmes règles que SRP_SoldierManagement.BuildMenu)
	static bool CanManage(SRP_PlayerManagerComponent players, int playerId, SRP_PlayerRecord record)
	{
		if (SRP_SoldierManagement.CanManageGrades(record) || SRP_SoldierManagement.CanManageInstructors(record))
			return true;
		int certif = 1;
		while (certif <= SRP_Certifs.LAST)
		{
			if (certif != SRP_ECertif.INSTRUCTEUR && players.IsInstructeur(playerId, certif))
				return true;
			certif = certif * 2;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Pour CanBeShownScript : ce droit est-il accordé au joueur qui regarde ? Chez le joueur, on lit ce que le serveur
	//! lui a envoyé ; sur le serveur, la même valeur est gardée dans son contrôleur (SRP_SetRights).
	static bool Has(IEntity user, int right)
	{
		if (!user)
			return false;
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0)
			return false;
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!pc || pc.GetPlayerId() != playerId)
			pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!pc)
			return Replication.IsServer();	// serveur sans contrôleur : l'action revérifie tout à l'exécution
		return (pc.SRP_GetRights() & right) != 0;
	}
}
