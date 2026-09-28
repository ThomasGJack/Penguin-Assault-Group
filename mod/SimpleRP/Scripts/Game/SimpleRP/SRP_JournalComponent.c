//------------------------------------------------------------------------------------------------
// SimpleRP — Journal
// Écrit chaque événement dans $profile:SimpleRP/journal/journal_<année-mois>.log
// et le répète dans la console (Log Console du Workbench, console du serveur).
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Journal SimpleRP : événements horodatés dans le dossier profil du serveur")]
class SRP_JournalComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_JournalComponent : SCR_BaseGameModeComponent
{
	protected static SRP_JournalComponent s_Instance;

	//------------------------------------------------------------------------------------------------
	static SRP_JournalComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;

		if (!Replication.IsServer())
			return;

		SRP_Paths.EnsureDirectories();
		Log("SYSTEME", "Journal démarré");
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit une ligne dans le journal. Catégories utilisées : SYSTEME, CONNEXION, DECONNEXION,
	//! GRADE, CERTIF, STAFF, INFRACTION, SAUVEGARDE, ERREUR ; front : TERRITOIRE (zones, public), FRONT (carré par
	//! carré, Staff seul) ; COMMANDEUR (décisions du Commandeur ennemi, Staff seul). FRONT et COMMANDEUR n'entrent
	//! jamais dans l'onglet Journal du PC
	static const int RECENT_MAX = 80;
	protected static ref array<string> s_aRecent = {};

	//------------------------------------------------------------------------------------------------
	//! Les dernières lignes du journal (les plus récentes en dernier), pour le terminal du PC
	static array<string> GetRecent()
	{
		return s_aRecent;
	}

	//------------------------------------------------------------------------------------------------
	static void Log(string categorie, string message)
	{
		if (!Replication.IsServer())
			return;

		string line = string.Format("[%1] [%2] %3", SRP_Time.Now(), categorie, message);
		Print("[SRP] " + line, LogLevel.NORMAL);

		// L'onglet Journal du PC (SRP_PC.c) montre ces lignes à TOUS les joueurs : les décisions du Commandeur ennemi
		// (catégorie COMMANDEUR, Staff seul) n'y entrent pas, ni les lignes FRONT (carré par carré, menace, carrés
		// refusés : Staff seul, CONTRATS_FRONT §1.12), qui rempliraient vite les RECENT_MAX lignes ; le fichier du
		// mois, la console et Discord les gardent (FRONT : journal-serveur, m_bFrontSquaresStaff)
		if (categorie != "COMMANDEUR" && categorie != "FRONT")
		{
			s_aRecent.Insert(line);
			if (s_aRecent.Count() > RECENT_MAX)
				s_aRecent.RemoveOrdered(0);
		}

		string path = SRP_Paths.JOURNAL + "/journal_" + SRP_Time.Month() + ".log";
		FileHandle file = FileIO.OpenFile(path, FileMode.APPEND);
		if (!file)
		{
			Print("[SRP] Impossible d'ouvrir le journal : " + path, LogLevel.WARNING);
			return;
		}

		file.WriteLine(line);
		file.Close();

		// Pont Discord (SRP_Discord.c) : ne fait rien si le composant est absent ou inactif
		SRP_DiscordComponent.Push(categorie, message);
	}
}
