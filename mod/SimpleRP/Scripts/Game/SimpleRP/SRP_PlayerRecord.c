//------------------------------------------------------------------------------------------------
// SimpleRP — Fiche joueur persistante
// Un fichier JSON par joueur : $profile:SimpleRP/players/<identité>.json
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

class SRP_PlayerRecord
{
	// Identité
	string m_sIdentity;					// identité Bohemia (ou "local_<pseudo>" en Workbench)
	string m_sName;						// dernier pseudo vu

	// Rôle
	int m_iGrade = SRP_EGrade.RECRUE;
	int m_iCertifs = SRP_ECertif.AUCUNE;			// certifications obtenues (bits SRP_ECertif)
	int m_iInstructeurCertifs = SRP_ECertif.AUCUNE;	// certifications que ce joueur est habilité à valider
	bool m_bStaff = false;
	string m_sMortEnAbsence = "";		// motif de la mort pendant une déconnexion, à annoncer au retour
	bool m_bCharteAcceptee = false;
	int m_iCharteVersion = 0;
	string m_sCharteDate = "";

	// États de service
	string m_sFirstSeen;
	string m_sLastSeen;
	int m_iPlaytimeSeconds = 0;
	int m_iConnections = 0;
	int m_iMissions = 0;
	int m_iInfractions = 0;
	int m_iVotes = 0;					// votes pour le serveur sur top-serveurs.net, comptés par le bot
	string m_sVoteStamps = "";			// horodatages (bot) des 10 derniers votes comptés, « ,t1,t2, » : un renvoi ne compte pas deux fois

	// Argent porté sur soi (sacoches ramassées, pas encore déposées au coffre)
	int m_iCash = 0;

	// Persistance du personnage : inventaire (JSON vanilla SCR_PlayerArsenalLoadout) au moment de la dernière sauvegarde
	string m_sInventory;

	// Persistance du personnage (position au moment de la dernière sauvegarde)
	bool m_bHasPosition = false;
	float m_fPosX = 0;
	float m_fPosY = 0;
	float m_fPosZ = 0;
	float m_fYaw = 0;

	//------------------------------------------------------------------------------------------------
	static string GetPathFor(string identity)
	{
		return SRP_Paths.PLAYERS + "/" + SRP_Utils.SafeFileName(identity) + ".json";
	}

	//------------------------------------------------------------------------------------------------
	string GetPath()
	{
		return GetPathFor(m_sIdentity);
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit la fiche sur le disque. Retourne false en cas d'échec.
	bool Save()
	{
		JsonSaveContext ctx = new JsonSaveContext();

		ctx.WriteValue("identity", m_sIdentity);
		ctx.WriteValue("name", m_sName);

		ctx.WriteValue("grade", m_iGrade);
		ctx.WriteValue("gradeScale", 2);
		ctx.WriteValue("certifs", m_iCertifs);
		ctx.WriteValue("instructeurCertifs", m_iInstructeurCertifs);
		ctx.WriteValue("staff", m_bStaff);
		ctx.WriteValue("mortEnAbsence", m_sMortEnAbsence);
		ctx.WriteValue("charteAcceptee", m_bCharteAcceptee);
		ctx.WriteValue("charteVersion", m_iCharteVersion);
		ctx.WriteValue("charteDate", m_sCharteDate);

		ctx.WriteValue("firstSeen", m_sFirstSeen);
		ctx.WriteValue("lastSeen", m_sLastSeen);
		ctx.WriteValue("playtimeSeconds", m_iPlaytimeSeconds);
		ctx.WriteValue("connections", m_iConnections);
		ctx.WriteValue("missions", m_iMissions);
		ctx.WriteValue("infractions", m_iInfractions);
		ctx.WriteValue("votes", m_iVotes);
		ctx.WriteValue("voteStamps", m_sVoteStamps);
		ctx.WriteValue("cash", m_iCash);

		ctx.WriteValue("inventory", m_sInventory);
		ctx.WriteValue("hasPosition", m_bHasPosition);
		ctx.WriteValue("posX", m_fPosX);
		ctx.WriteValue("posY", m_fPosY);
		ctx.WriteValue("posZ", m_fPosZ);
		ctx.WriteValue("yaw", m_fYaw);

		bool ok = ctx.SaveToFile(GetPath());
		if (!ok)
			Print("[SRP] Échec de sauvegarde de la fiche : " + GetPath(), LogLevel.ERROR);
		return ok;
	}

	//------------------------------------------------------------------------------------------------
	//! Charge la fiche d'un joueur. Retourne null si elle n'existe pas encore.
	//------------------------------------------------------------------------------------------------
	//! Toutes les fiches présentes sur le disque (annuaire du PC)
	static void LoadAll(out array<ref SRP_PlayerRecord> records)
	{
		array<string> files = {};
		SRP_RecordFinder finder = new SRP_RecordFinder();
		finder.m_aFiles = files;
		FileIO.FindFiles(finder.OnFile, SRP_Paths.PLAYERS, ".json");
		foreach (string file : files)
		{
			string stem = file;
			int slash = stem.LastIndexOf("/");
			if (slash >= 0)
				stem = stem.Substring(slash + 1, stem.Length() - slash - 1);
			int dot = stem.LastIndexOf(".");
			if (dot >= 0)
				stem = stem.Substring(0, dot);
			SRP_PlayerRecord record = Load(stem);
			if (record)
				records.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	static SRP_PlayerRecord Load(string identity)
	{
		string path = GetPathFor(identity);
		if (!FileIO.FileExists(path))
			return null;

		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(path))
		{
			Print("[SRP] Fiche illisible : " + path, LogLevel.ERROR);
			return null;
		}

		SRP_PlayerRecord r = new SRP_PlayerRecord();
		r.m_sIdentity = identity;

		ctx.ReadValue("name", r.m_sName);

		ctx.ReadValue("grade", r.m_iGrade);
		int gradeScale = 1;
		ctx.ReadValue("gradeScale", gradeScale);
		if (gradeScale < 2)
			r.m_iGrade = SRP_Grades.FromLegacy(r.m_iGrade);	// ancienne échelle à 7 grades
		ctx.ReadValue("certifs", r.m_iCertifs);
		ctx.ReadValue("instructeurCertifs", r.m_iInstructeurCertifs);
		ctx.ReadValue("staff", r.m_bStaff);
		ctx.ReadValue("mortEnAbsence", r.m_sMortEnAbsence);
		ctx.ReadValue("charteAcceptee", r.m_bCharteAcceptee);
		ctx.ReadValue("charteVersion", r.m_iCharteVersion);
		ctx.ReadValue("charteDate", r.m_sCharteDate);

		ctx.ReadValue("firstSeen", r.m_sFirstSeen);
		ctx.ReadValue("lastSeen", r.m_sLastSeen);
		ctx.ReadValue("playtimeSeconds", r.m_iPlaytimeSeconds);
		ctx.ReadValue("connections", r.m_iConnections);
		ctx.ReadValue("missions", r.m_iMissions);
		ctx.ReadValue("infractions", r.m_iInfractions);
		ctx.ReadValue("votes", r.m_iVotes);
		ctx.ReadValue("voteStamps", r.m_sVoteStamps);
		ctx.ReadValue("cash", r.m_iCash);

		ctx.ReadValue("inventory", r.m_sInventory);
		ctx.ReadValue("hasPosition", r.m_bHasPosition);
		ctx.ReadValue("posX", r.m_fPosX);
		ctx.ReadValue("posY", r.m_fPosY);
		ctx.ReadValue("posZ", r.m_fPosZ);
		ctx.ReadValue("yaw", r.m_fYaw);

		if (!SRP_Grades.IsValide(r.m_iGrade))
			r.m_iGrade = SRP_EGrade.RECRUE;

		return r;
	}

	//------------------------------------------------------------------------------------------------
	vector GetPosition()
	{
		return Vector(m_fPosX, m_fPosY, m_fPosZ);
	}

	//------------------------------------------------------------------------------------------------
	void SetPosition(vector pos, float yaw)
	{
		m_fPosX = pos[0];
		m_fPosY = pos[1];
		m_fPosZ = pos[2];
		m_fYaw = yaw;
		m_bHasPosition = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Résumé lisible pour le journal et les commandes
	string ToSummary()
	{
		return string.Format("%1 [%2] — %3 — certifs : %4 — staff : %5 — %6 min de jeu",
			m_sName,
			m_sIdentity,
			SRP_Grades.GetName(m_iGrade),
			SRP_Certifs.MaskToString(m_iCertifs),
			SRP_Utils.OuiNon(m_bStaff),
			m_iPlaytimeSeconds / 60);
	}
}

//------------------------------------------------------------------------------------------------
//! Collecte des fichiers de fiches (rappel de FileIO.FindFiles)
class SRP_RecordFinder
{
	ref array<string> m_aFiles;

	void OnFile(string fileName, FileAttribute attributes)
	{
		if (m_aFiles)
			m_aFiles.Insert(fileName);
	}
}
