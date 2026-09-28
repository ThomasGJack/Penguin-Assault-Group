//------------------------------------------------------------------------------------------------
// SimpleRP — Milsim RP Armée Française (Arma Reforger)
// Définitions communes : grades, certifications, chemins, utilitaires
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

//! Grades, dans l'ordre hiérarchique (la valeur numérique sert aux comparaisons)
//! Grades, dans l'ordre. Les anciennes fiches (7 grades) sont converties au chargement (SRP_PlayerRecord).
enum SRP_EGrade
{
	RECRUE = 0,
	SOLDAT = 1,			// Soldat de 2e classe
	PREMIERE_CLASSE = 2,
	CAPORAL = 3,
	CAPORAL_CHEF = 4,
	SERGENT = 5,
	SERGENT_CHEF = 6,
	ADJUDANT = 7,
	LIEUTENANT = 8,
	CAPITAINE = 9
};

//! Certifications, en bits : un joueur peut en cumuler plusieurs dans un seul entier
//! Certifications : un masque de bits, persistant dans la fiche. Les noms internes ne changent pas
//! (compatibilité des fiches) ; les noms affichés et les sigles sont dans SRP_Certifs.
enum SRP_ECertif
{
	AUCUNE = 0,
	FGI = 1,			// Formation Générale Initiale
	MEDIC = 2,			// Infirmier
	GENIE = 4,			// Sapeur
	COND_LEGER = 8,		// Conducteur VL
	COND_LOURD = 16,	// Conducteur PL
	BLINDE = 32,		// Chef d'engin blindé
	PILOTE = 64,		// Pilote
	MG = 128,			// Mitrailleur
	AT = 256,			// Antichar
	TP = 512,			// Tireur de précision
	OR = 1024,			// Opérateur radio
	LG = 2048,			// Lance-grenades
	LOGI = 4096,		// Logisticien
	CDG = 8192,			// Chef de groupe
	INSTRUCTEUR = 16384,	// obsolète : plus de certification Instructeur globale, l'habilitation se fait par certification (conservé pour les anciennes fiches)
	MEDECIN = 32768
};

//! Une règle de l'arbre des certifications (fichier SRP_CertifTree.conf)
[BaseContainerProps(), BaseContainerCustomTitleField("m_sTitle")]
class SRP_CertifRule
{
	[Attribute("Règle", UIWidgets.EditBox, "Titre affiché dans l'éditeur")]
	string m_sTitle;

	[Attribute("1", UIWidgets.ComboBox, "Certification concernée", "", ParamEnumArray.FromEnum(SRP_ECertif))]
	SRP_ECertif m_eCertif;

	[Attribute("0", UIWidgets.Flags, "Certifications exigées avant celle-ci", "", ParamEnumArray.FromEnum(SRP_ECertif))]
	int m_iPrerequisites;

	[Attribute("0", UIWidgets.ComboBox, "Grade minimum", "", ParamEnumArray.FromEnum(SRP_EGrade))]
	SRP_EGrade m_eMinGrade;
}

//! Une règle d'accès à un grade
[BaseContainerProps(), BaseContainerCustomTitleField("m_sTitle")]
class SRP_GradeRule
{
	[Attribute("Règle", UIWidgets.EditBox, "Titre affiché dans l'éditeur")]
	string m_sTitle;

	[Attribute("2", UIWidgets.ComboBox, "Grade concerné", "", ParamEnumArray.FromEnum(SRP_EGrade))]
	SRP_EGrade m_eGrade;

	[Attribute("0", UIWidgets.EditBox, "Spécialités minimum (0 = aucune condition ; laisser à 0 par défaut)")]
	int m_iMinSpecialties;

	[Attribute("0", UIWidgets.EditBox, "Heures de jeu minimum")]
	int m_iMinHours;

	[Attribute("0", UIWidgets.EditBox, "Missions réussies minimum")]
	int m_iMinMissions;

	[Attribute("1", UIWidgets.Flags, "Certifications exigées", "", ParamEnumArray.FromEnum(SRP_ECertif))]
	int m_iRequiredCertifs;
}

[BaseContainerProps(configRoot: true)]
class SRP_CertifTree
{
	[Attribute("", UIWidgets.Object, "Règles de certification : une par certification ; celles qui manquent gardent la règle par défaut du code")]
	ref array<ref SRP_CertifRule> m_aRules;

	[Attribute("", UIWidgets.Object, "Règles de grade : conditions d'accès ; les grades absents gardent la règle par défaut du code")]
	ref array<ref SRP_GradeRule> m_aGradeRules;

	//------------------------------------------------------------------------------------------------
	static SRP_GradeRule FindGrade(SRP_CertifTree tree, int grade)
	{
		if (!tree || !tree.m_aGradeRules)
			return null;
		foreach (SRP_GradeRule rule : tree.m_aGradeRules)
		{
			if (rule && rule.m_eGrade == grade)
				return rule;
		}
		return null;
	}

	static int MinSpecialtiesOf(SRP_CertifTree tree, int grade)
	{
		SRP_GradeRule rule = FindGrade(tree, grade);
		if (rule) return rule.m_iMinSpecialties;
		return SRP_Grades.DefaultMinSpecialties(grade);
	}

	static int MinHoursOf(SRP_CertifTree tree, int grade)
	{
		SRP_GradeRule rule = FindGrade(tree, grade);
		if (rule) return rule.m_iMinHours;
		return SRP_Grades.DefaultMinHours(grade);
	}

	static int MinMissionsOf(SRP_CertifTree tree, int grade)
	{
		SRP_GradeRule rule = FindGrade(tree, grade);
		if (rule) return rule.m_iMinMissions;
		return SRP_Grades.DefaultMinMissions(grade);
	}

	static int RequiredCertifsOf(SRP_CertifTree tree, int grade)
	{
		SRP_GradeRule rule = FindGrade(tree, grade);
		if (rule) return rule.m_iRequiredCertifs;
		return SRP_Grades.DefaultRequiredCertifs(grade);
	}

	//------------------------------------------------------------------------------------------------
	static SRP_CertifRule Find(SRP_CertifTree tree, int certif)
	{
		if (!tree || !tree.m_aRules)
			return null;
		foreach (SRP_CertifRule rule : tree.m_aRules)
		{
			if (rule && rule.m_eCertif == certif)
				return rule;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	static int PrerequisitesOf(SRP_CertifTree tree, int certif)
	{
		SRP_CertifRule rule = Find(tree, certif);
		if (rule)
			return rule.m_iPrerequisites;
		return SRP_Certifs.DefaultPrerequisites(certif);
	}

	//------------------------------------------------------------------------------------------------
	static int MinGradeOf(SRP_CertifTree tree, int certif)
	{
		SRP_CertifRule rule = Find(tree, certif);
		if (rule)
			return rule.m_eMinGrade;
		return SRP_Certifs.DefaultMinGrade(certif);
	}
}

//! Les 4 ressources de la base
enum SRP_EResource
{
	MUNITIONS = 0,
	CARBURANT = 1,
	PIECES = 2,
	VIVRES = 3
};

//------------------------------------------------------------------------------------------------
class SRP_Resources
{
	static const int COUNT = 4;

	//------------------------------------------------------------------------------------------------
	static string GetName(int resource)
	{
		switch (resource)
		{
			case SRP_EResource.MUNITIONS: return "munitions";
			case SRP_EResource.CARBURANT: return "carburant";
			case SRP_EResource.PIECES: return "pièces";
			case SRP_EResource.VIVRES: return "vivres";
		}
		return "?";
	}

	//------------------------------------------------------------------------------------------------
	//! Nom du dépôt qui stocke la ressource
	static string GetDepotName(int resource)
	{
		switch (resource)
		{
			case SRP_EResource.MUNITIONS: return "Soute";
			case SRP_EResource.CARBURANT: return "Citerne";
			case SRP_EResource.PIECES: return "Atelier";
			case SRP_EResource.VIVRES: return "Intendance";
		}
		return "Dépôt";
	}

	//------------------------------------------------------------------------------------------------
	//! Convertit un nom tapé dans une commande en ressource. Retourne -1 si inconnu.
	static int FromName(string name)
	{
		string s = name;
		s.ToLower();

		if (s == "munitions" || s == "munition" || s == "mun" || s == "soute") return SRP_EResource.MUNITIONS;
		if (s == "carburant" || s == "carbu" || s == "citerne" || s == "fuel") return SRP_EResource.CARBURANT;
		if (s == "pieces" || s == "pièces" || s == "piece" || s == "pièce" || s == "atelier") return SRP_EResource.PIECES;
		if (s == "vivres" || s == "vivre" || s == "intendance") return SRP_EResource.VIVRES;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsValide(int resource)
	{
		return resource >= 0 && resource < COUNT;
	}
}

//------------------------------------------------------------------------------------------------
class SRP_Grades
{
	//------------------------------------------------------------------------------------------------
	static string GetName(int grade)
	{
		switch (grade)
		{
			case SRP_EGrade.RECRUE: return "Recrue";
			case SRP_EGrade.SOLDAT: return "Soldat de 2e classe";
			case SRP_EGrade.PREMIERE_CLASSE: return "Soldat de 1re classe";
			case SRP_EGrade.CAPORAL: return "Caporal";
			case SRP_EGrade.CAPORAL_CHEF: return "Caporal-chef";
			case SRP_EGrade.SERGENT: return "Sergent";
			case SRP_EGrade.SERGENT_CHEF: return "Sergent-chef";
			case SRP_EGrade.ADJUDANT: return "Adjudant";
			case SRP_EGrade.LIEUTENANT: return "Lieutenant";
			case SRP_EGrade.CAPITAINE: return "Capitaine";
		}
		return "Inconnu";
	}

	//------------------------------------------------------------------------------------------------
	//! Lieutenant et plus
	static bool IsOfficier(int grade)
	{
		return grade >= SRP_EGrade.LIEUTENANT;
	}

	//------------------------------------------------------------------------------------------------
	//! Sergent à Adjudant
	static bool IsSousOfficier(int grade)
	{
		return grade >= SRP_EGrade.SERGENT && grade < SRP_EGrade.LIEUTENANT;
	}

	//------------------------------------------------------------------------------------------------
	static bool IsValide(int grade)
	{
		return grade >= SRP_EGrade.RECRUE && grade <= SRP_EGrade.CAPITAINE;
	}

	//------------------------------------------------------------------------------------------------
	//! Convertit "3", "sergent", "1re", "cch"... en valeur de grade. Retourne -1 si inconnu.
	static int FromName(string name)
	{
		string s = name;
		s.ToLower();

		if (SRP_Utils.IsNumeric(s))
		{
			int value = s.ToInt();
			if (IsValide(value))
				return value;
			return -1;
		}

		if (s == "recrue") return SRP_EGrade.RECRUE;
		if (s == "soldat" || s == "2e" || s == "2eclasse" || s == "2e-classe") return SRP_EGrade.SOLDAT;
		if (s == "1re" || s == "1ere" || s == "1reclasse" || s == "1re-classe" || s == "premiere") return SRP_EGrade.PREMIERE_CLASSE;
		if (s == "caporal" || s == "cpl") return SRP_EGrade.CAPORAL;
		if (s == "caporal-chef" || s == "caporalchef" || s == "cch") return SRP_EGrade.CAPORAL_CHEF;
		if (s == "sergent" || s == "sgt") return SRP_EGrade.SERGENT;
		if (s == "sergent-chef" || s == "sergentchef" || s == "sch") return SRP_EGrade.SERGENT_CHEF;
		if (s == "adjudant" || s == "adj") return SRP_EGrade.ADJUDANT;
		if (s == "lieutenant" || s == "ltn") return SRP_EGrade.LIEUTENANT;
		if (s == "capitaine" || s == "cne") return SRP_EGrade.CAPITAINE;
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	static string GetCommandNames()
	{
		return "recrue, soldat (2e), 1re, caporal, cch, sergent, sch, adjudant, lieutenant, capitaine";
	}

	//------------------------------------------------------------------------------------------------
	//! Conversion d'une fiche écrite avec l'ancienne échelle à 7 grades
	static int FromLegacy(int oldGrade)
	{
		switch (oldGrade)
		{
			case 0: return SRP_EGrade.RECRUE;
			case 1: return SRP_EGrade.SOLDAT;
			case 2: return SRP_EGrade.CAPORAL;
			case 3: return SRP_EGrade.SERGENT;
			case 4: return SRP_EGrade.ADJUDANT;
			case 5: return SRP_EGrade.LIEUTENANT;
			case 6: return SRP_EGrade.CAPITAINE;
		}
		return SRP_EGrade.RECRUE;
	}

	// Conditions par défaut pour accéder à un grade (modifiables dans SRP_CertifTree.conf, règles de grade)
	//------------------------------------------------------------------------------------------------
	static int DefaultMinSpecialties(int grade)
	{
		return 0;	// pas de condition de spécialités : ce sont les heures et les missions qui comptent
	}

	//------------------------------------------------------------------------------------------------
	static int DefaultMinHours(int grade)
	{
		switch (grade)
		{
			case SRP_EGrade.PREMIERE_CLASSE: return 10;
			case SRP_EGrade.CAPORAL: return 25;
			case SRP_EGrade.CAPORAL_CHEF: return 50;
			case SRP_EGrade.SERGENT: return 100;
			case SRP_EGrade.SERGENT_CHEF: return 200;
			case SRP_EGrade.ADJUDANT: return 400;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static int DefaultMinMissions(int grade)
	{
		switch (grade)
		{
			case SRP_EGrade.PREMIERE_CLASSE: return 8;
			case SRP_EGrade.CAPORAL: return 20;
			case SRP_EGrade.CAPORAL_CHEF: return 40;
			case SRP_EGrade.SERGENT: return 70;
			case SRP_EGrade.SERGENT_CHEF: return 120;
			case SRP_EGrade.ADJUDANT: return 200;
		}
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static int DefaultRequiredCertifs(int grade)
	{
		if (grade >= SRP_EGrade.SOLDAT)
		{
			if (grade >= SRP_EGrade.SERGENT)
				return SRP_ECertif.FGI | SRP_ECertif.CDG;
			return SRP_ECertif.FGI;
		}
		return 0;
	}
}

//------------------------------------------------------------------------------------------------
class SRP_Certifs
{
	static const int LAST = SRP_ECertif.MEDECIN;

	//------------------------------------------------------------------------------------------------
	//! Sigle court (tableaux, /moi)
	static string GetName(int certif)
	{
		switch (certif)
		{
			case SRP_ECertif.FGI: return "FGI";
			case SRP_ECertif.MEDIC: return "Infirmier";
			case SRP_ECertif.GENIE: return "Sapeur";
			case SRP_ECertif.COND_LEGER: return "VL";
			case SRP_ECertif.COND_LOURD: return "PL";
			case SRP_ECertif.BLINDE: return "Blindé";
			case SRP_ECertif.PILOTE: return "Pilote";
			case SRP_ECertif.MG: return "MG";
			case SRP_ECertif.AT: return "AT";
			case SRP_ECertif.TP: return "TP";
			case SRP_ECertif.OR: return "OR";
			case SRP_ECertif.LG: return "LG";
			case SRP_ECertif.LOGI: return "Logi";
			case SRP_ECertif.CDG: return "CDG";
			case SRP_ECertif.INSTRUCTEUR: return "Instructeur";
			case SRP_ECertif.MEDECIN: return "Médecin";
		}
		return "Inconnue";
	}

	//------------------------------------------------------------------------------------------------
	//! Nom complet (fiche, journal)
	static string GetFullName(int certif)
	{
		switch (certif)
		{
			case SRP_ECertif.FGI: return "Formation Générale Initiale";
			case SRP_ECertif.MEDIC: return "Infirmier";
			case SRP_ECertif.GENIE: return "Sapeur";
			case SRP_ECertif.COND_LEGER: return "Conducteur VL";
			case SRP_ECertif.COND_LOURD: return "Conducteur PL";
			case SRP_ECertif.BLINDE: return "Chef d'engin blindé";
			case SRP_ECertif.PILOTE: return "Pilote";
			case SRP_ECertif.MG: return "Mitrailleur";
			case SRP_ECertif.AT: return "Antichar";
			case SRP_ECertif.TP: return "Tireur de précision";
			case SRP_ECertif.OR: return "Opérateur radio";
			case SRP_ECertif.LG: return "Lance-grenades";
			case SRP_ECertif.LOGI: return "Logisticien";
			case SRP_ECertif.CDG: return "Chef de groupe";
			case SRP_ECertif.INSTRUCTEUR: return "Instructeur";
			case SRP_ECertif.MEDECIN: return "Médecin";
		}
		return "Inconnue";
	}

	//------------------------------------------------------------------------------------------------
	//! Le masque contient-il la certification ?
	static bool Has(int mask, int certif)
	{
		return (mask & certif) == certif;
	}

	//------------------------------------------------------------------------------------------------
	//! Convertit un nom tapé dans une commande en certification. Retourne 0 si inconnu.
	static int FromName(string name)
	{
		string s = name;
		s.ToLower();

		if (s == "fgi") return SRP_ECertif.FGI;
		if (s == "infirmier" || s == "medic" || s == "médic") return SRP_ECertif.MEDIC;
		if (s == "medecin" || s == "médecin") return SRP_ECertif.MEDECIN;
		if (s == "sapeur" || s == "genie" || s == "génie") return SRP_ECertif.GENIE;
		if (s == "vl" || s == "leger" || s == "léger") return SRP_ECertif.COND_LEGER;
		if (s == "pl" || s == "lourd") return SRP_ECertif.COND_LOURD;
		if (s == "blinde" || s == "blindé") return SRP_ECertif.BLINDE;
		if (s == "pilote") return SRP_ECertif.PILOTE;
		if (s == "mg") return SRP_ECertif.MG;
		if (s == "lg") return SRP_ECertif.LG;
		if (s == "at") return SRP_ECertif.AT;
		if (s == "tp") return SRP_ECertif.TP;
		if (s == "or" || s == "radio") return SRP_ECertif.OR;
		if (s == "logi") return SRP_ECertif.LOGI;
		if (s == "cdg") return SRP_ECertif.CDG;
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	static string GetCommandNames()
	{
		return "fgi, or, infirmier, medecin, sapeur, logi, vl, pl, blinde, pilote, mg, lg, at, tp, cdg";
	}

	//------------------------------------------------------------------------------------------------
	//! Liste lisible des certifications d'un masque, ex : "FGI, Infirmier"
	static string MaskToString(int mask)
	{
		if (mask == 0)
			return "aucune";

		string result = "";
		int certif = 1;
		while (certif <= LAST)
		{
			if ((mask & certif) != 0 && certif != SRP_ECertif.INSTRUCTEUR)
			{
				if (!result.IsEmpty())
					result += ", ";
				result += GetName(certif);
			}
			certif = certif * 2;
		}
		return result;
	}

	//------------------------------------------------------------------------------------------------
	// L'arbre : prérequis et grade minimum. Modifiable dans le Workbench via SRP_CertifTree.conf
	// (SRP_PlayerManagerComponent → Arbre des certifications) ; ceci est la valeur par défaut.
	//------------------------------------------------------------------------------------------------
	static int DefaultPrerequisites(int certif)
	{
		switch (certif)
		{
			case SRP_ECertif.FGI: return 0;
			case SRP_ECertif.CDG: return SRP_ECertif.FGI | SRP_ECertif.OR;
			case SRP_ECertif.MEDECIN: return SRP_ECertif.FGI | SRP_ECertif.MEDIC;
			case SRP_ECertif.COND_LOURD: return SRP_ECertif.FGI | SRP_ECertif.COND_LEGER;
			case SRP_ECertif.BLINDE: return SRP_ECertif.FGI | SRP_ECertif.COND_LEGER | SRP_ECertif.COND_LOURD;
			case SRP_ECertif.PILOTE: return SRP_ECertif.FGI | SRP_ECertif.COND_LEGER;
		}
		return SRP_ECertif.FGI;
	}

	//------------------------------------------------------------------------------------------------
	static int DefaultMinGrade(int certif)
	{
		switch (certif)
		{
			case SRP_ECertif.CDG: return SRP_EGrade.CAPORAL;
			case SRP_ECertif.MEDECIN: return SRP_EGrade.CAPORAL;
			case SRP_ECertif.BLINDE: return SRP_EGrade.CAPORAL;
			case SRP_ECertif.PILOTE: return SRP_EGrade.CAPORAL;
			case SRP_ECertif.TP: return SRP_EGrade.CAPORAL;
		}
		return SRP_EGrade.RECRUE;
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les certifications qui ont « certif » (directement ou par cascade) dans leurs prérequis
	static int Dependents(int certif, SRP_CertifTree tree)
	{
		int result = 0;
		bool changed = true;
		while (changed)
		{
			changed = false;
			int candidate = 1;
			while (candidate <= LAST)
			{
				if ((result & candidate) == 0 && candidate != certif)
				{
					int prereq = SRP_CertifTree.PrerequisitesOf(tree, candidate);
					if ((prereq & (certif | result)) != 0)
					{
						result = result | candidate;
						changed = true;
					}
				}
				candidate = candidate * 2;
			}
		}
		return result;
	}
}

//------------------------------------------------------------------------------------------------
//! Chemins des fichiers de persistance. "$profile:" = dossier profil du serveur
//! (en Workbench : Documents\My Games\ArmaReforgerWorkbench\profile)
class SRP_Paths
{
	static const string ROOT = "$profile:SimpleRP";
	static const string PLAYERS = "$profile:SimpleRP/players";
	static const string JOURNAL = "$profile:SimpleRP/journal";
	static const string BASE = "$profile:SimpleRP/base.json";

	//------------------------------------------------------------------------------------------------
	static void EnsureDirectories()
	{
		FileIO.MakeDirectory(ROOT);
		FileIO.MakeDirectory(PLAYERS);
		FileIO.MakeDirectory(JOURNAL);
	}
}

//------------------------------------------------------------------------------------------------
class SRP_Time
{
	//------------------------------------------------------------------------------------------------
	static string Pad2(int v)
	{
		if (v < 10)
			return "0" + v.ToString();
		return v.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Date et heure locales du serveur, ex : "2026-09-06 21:14:03"
	static string Now()
	{
		int y, m, d, h, mi, s;
		System.GetYearMonthDay(y, m, d);
		System.GetHourMinuteSecond(h, mi, s);
		return string.Format("%1-%2-%3 %4:%5:%6", y, Pad2(m), Pad2(d), Pad2(h), Pad2(mi), Pad2(s));
	}

	//------------------------------------------------------------------------------------------------
	//! Mois courant, ex : "2026-09" (sert au découpage mensuel du journal)
	static string Month()
	{
		int y, m, d;
		System.GetYearMonthDay(y, m, d);
		return string.Format("%1-%2", y, Pad2(m));
	}
}

//------------------------------------------------------------------------------------------------
class SRP_Utils
{
	//------------------------------------------------------------------------------------------------
	static string OuiNon(bool value)
	{
		if (value)
			return "oui";
		return "non";
	}

	//------------------------------------------------------------------------------------------------
	//! Vrai si la chaîne ne contient que des chiffres (au moins un)
	static bool IsNumeric(string s)
	{
		if (s.IsEmpty())
			return false;

		for (int i = 0; i < s.Length(); i++)
		{
			if (!"0123456789".Contains(s.Get(i)))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur -> joueur : message dans son chat (style système). Ne fait rien hors serveur.
	static void NotifyPlayer(int playerId, string text)
	{
		if (!Replication.IsServer())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc)
			pc.SRP_Notify(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur -> joueur : encart à l'écran (titre + texte), comme la lecture d'un panneau
	static void HintPlayer(int playerId, string title, string text)
	{
		if (!Replication.IsServer())
			return;

		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc)
			pc.SRP_Hint(title, text);
	}

	//------------------------------------------------------------------------------------------------
	//! Mention journal (jamais montrée aux joueurs) quand un point de livraison est gardé
	static string HostileSuffix(bool hostile)
	{
		if (hostile)
			return " [emprise ennemie]";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Personnages des joueurs connectés
	static array<IEntity> GetPlayerCharacters()
	{
		array<IEntity> players = {};
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(id);
			if (character)
				players.Insert(character);
		}
		return players;
	}

	//------------------------------------------------------------------------------------------------
	//! Distance au joueur le plus proche (1 000 000 si aucun)
	static float NearestPlayerDistance(vector position, array<IEntity> players)
	{
		float best = 1000000;
		foreach (IEntity player : players)
		{
			float distance = vector.Distance(position, player.GetOrigin());
			if (distance < best)
				best = distance;
		}
		return best;
	}

	//------------------------------------------------------------------------------------------------
	//! Message à tous les joueurs connectés
	static void NotifyAll(string text)
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int id : ids)
			NotifyPlayer(id, text);
	}

	//------------------------------------------------------------------------------------------------
	static string OnOff(bool value)
	{
		if (value)
			return "ON";
		return "OFF";
	}

	//------------------------------------------------------------------------------------------------
	//! Comparaison alphabétique simple (< 0 si a avant b), sans dépendre des opérateurs de chaîne
	static int CompareStrings(string a, string b)
	{
		string la = a;
		string lb = b;
		la.ToLower();
		lb.ToLower();
		int n = Math.Min(la.Length(), lb.Length());
		for (int i = 0; i < n; i++)
		{
			int ca = la.Get(i).ToAscii();
			int cb = lb.Get(i).ToAscii();
			if (ca != cb)
				return ca - cb;
		}
		return la.Length() - lb.Length();
	}

	//------------------------------------------------------------------------------------------------
	//! Le personnage (joueur ou IA) est-il mort ?
	static bool IsDead(IEntity entity)
	{
		if (!entity)
			return true;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(entity.FindComponent(SCR_CharacterControllerComponent));
		if (controller)
			return controller.IsDead();

		SCR_DamageManagerComponent damage = SCR_DamageManagerComponent.GetDamageManager(entity);
		return damage && damage.GetState() == EDamageState.DESTROYED;
	}

	//------------------------------------------------------------------------------------------------
	//! Identifiant joueur à partir de son personnage (-1 si ce n'est pas un joueur)
	static int GetPlayerIdFromEntity(IEntity entity)
	{
		if (!entity)
			return -1;
		return GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(entity);
	}

	//------------------------------------------------------------------------------------------------
	//! "{GUID}Prefabs/Weapons/FAMAS_F1.et" -> "FAMAS_F1"
	static string PrefabShortName(ResourceName prefab)
	{
		string s = prefab;
		int slash = s.LastIndexOf("/");
		if (slash >= 0)
			s = s.Substring(slash + 1, s.Length() - slash - 1);
		int dot = s.LastIndexOf(".");
		if (dot > 0)
			s = s.Substring(0, dot);
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Rend une chaîne utilisable comme nom de fichier
	static string SafeFileName(string input)
	{
		string result = input;
		result.Replace(" ", "_");
		result.Replace("/", "_");
		result.Replace("\\", "_");
		result.Replace(":", "_");
		result.Replace("*", "_");
		result.Replace("?", "_");
		result.Replace("\"", "_");
		result.Replace("<", "_");
		result.Replace(">", "_");
		result.Replace("|", "_");
		return result;
	}
}
