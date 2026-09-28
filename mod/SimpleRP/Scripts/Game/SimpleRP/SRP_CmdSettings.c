//------------------------------------------------------------------------------------------------
// SimpleRP — Commandeur ennemi (#76) et front (#75) : LE SEUL LECTEUR DE RÉGLAGES DU MOD. COMPLET (étape 1 du plan).
//
// RÔLE (serveur) : deux fichiers texte du profil, que Jack modifie sans republier :
// - $profile:SimpleRP/front_reglages.txt : les clés qui commencent par « front_ » (arbitrage Q6) ;
// - $profile:SimpleRP/commandeur_reglages.txt : toutes les autres (cmd_, info_, alerte_, officier_, desorg_, res_,
//   cap_, renfort_, colonne_, repli_, assaut_, harcelement_, reorg_, ca_, appui_, vu_).
// Format : une ligne « clé = valeur   # aide ». Le texte après # est une aide ; les lignes vides sont ignorées ; la
// clé est lue en minuscules ; virgule ou point acceptés pour les décimaux ; 1, oui, vrai (0, non, faux) pour les
// interrupteurs. Une clé par chiffre (trou 8).
// Cycle : chaque module DÉCLARE ses clés (Declare*, défaut = valeur de son champ) avant le premier Reload, puis les
// RELIT dans son LoadSettings (Get*). Reload relit les deux fichiers, contrôle chaque valeur selon le type déclaré
// (valeur illisible : défaut gardé et problème noté), ignore les clés inconnues (problème noté) et AJOUTE en fin de
// fichier les clés déclarées absentes (fichier absent : il est écrit en entier, groupé par section). Une nouvelle
// version du mod montre ainsi d'elle-même ses nouveaux réglages.
// Les réglages de STRUCTURE (cmd_regions, cmd_region_min_zones, cmd_region_max_zones) sont relus mais ne changent
// qu'au redémarrage (le tracé des régions est fait une fois, avant Load).
// APPELÉ PAR : SRP_FrontComponent (DeclareSettings, Start : Reload("démarrage"), ReloadSettings), les modules du
// front et SRP_MissionManagerComponent (Declare*, Get*), le Commandeur (constructeur : Declare* ; LoadSettings : Get*),
// SRP_CmdScreens (GetStatusText, GetProblems).
// Règles : aucun signe pour cent dans les textes écrits ; une seule déclaration par nom de variable par fonction.
//------------------------------------------------------------------------------------------------

class SRP_CmdSettings
{
	static const string PATH_CMD = "$profile:SimpleRP/commandeur_reglages.txt";
	static const string PATH_FRONT = "$profile:SimpleRP/front_reglages.txt";
	static const string FRONT_PREFIX = "front_";

	// Type déclaré d'une clé (contrôle de la valeur lue)
	protected static const int TYPE_TEXT = 0;
	protected static const int TYPE_INT = 1;
	protected static const int TYPE_FLOAT = 2;
	protected static const int TYPE_BOOL = 3;

	// Table des clés déclarées, dans l'ordre de déclaration (tableaux parallèles)
	protected static ref array<string> s_aKeys = {};
	protected static ref array<string> s_aDefaults = {};
	protected static ref array<string> s_aHelps = {};
	protected static ref array<int> s_aTypes = {};

	// Dernière lecture
	protected static ref map<string, string> s_mValues = new map<string, string>();	// valeurs valides lues
	protected static ref array<string> s_aSeen = {};		// clés présentes dans un fichier (même illisibles)
	protected static ref array<string> s_aWarned = {};		// clés déjà signalées depuis le dernier Reload
	protected static ref array<string> s_aProblems = {};	// problèmes du dernier Reload (menu Staff)
	protected static int s_iReadCount;
	protected static int s_iAddedCount;
	protected static int s_iReadUnix;
	protected static string s_sReadAt;

	//================================================================================================
	// Déclaration (avant Reload ; la première déclaration d'une clé fait foi)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Déclare une clé (mise en minuscules) avec son défaut et son aide ; « front_* » -> front_reglages.txt, sinon
	//! commandeur_reglages.txt ; type deviné d'après le défaut (entier, décimal ou texte) ; une clé déjà déclarée est
	//! ignorée — modules du front (défaut = attribut.ToString()), SRP_MissionManagerComponent.DeclareFrontSettings
	static void Declare(string key, string defaultValue, string help)
	{
		DeclareTyped(key, defaultValue, help, GuessType(defaultValue));
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare une clé entière (valeur du champ comme défaut) — DeclareSettings des modules
	static void DeclareInt(string key, int value, string help)
	{
		DeclareTyped(key, value.ToString(), help, TYPE_INT);
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare une clé décimale (valeur du champ comme défaut) — DeclareSettings des modules
	static void DeclareFloat(string key, float value, string help)
	{
		DeclareTyped(key, value.ToString(), help, TYPE_FLOAT);
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare un interrupteur (écrit 1 ou 0) — DeclareSettings des modules
	static void DeclareBool(string key, bool value, string help)
	{
		if (value)
			DeclareTyped(key, "1", help, TYPE_BOOL);
		else
			DeclareTyped(key, "0", help, TYPE_BOOL);
	}

	//------------------------------------------------------------------------------------------------
	//! Déclare une clé texte (prefab, liste « a, b, c ») ; toute valeur est acceptée, vide compris — DeclareSettings
	static void DeclareString(string key, string value, string help)
	{
		DeclareTyped(key, value, help, TYPE_TEXT);
	}

	//------------------------------------------------------------------------------------------------
	//! La clé est-elle déclarée ? — modules (contrôles), SRP_CmdScreens
	static bool IsDeclared(string key)
	{
		return s_aKeys.Contains(NormKey(key));
	}

	//------------------------------------------------------------------------------------------------
	//! La clé va-t-elle dans front_reglages.txt ? — Reload
	static bool IsFrontKey(string key)
	{
		return key.StartsWith(FRONT_PREFIX);
	}

	//================================================================================================
	// Lecture des fichiers
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Serveur : relit les DEUX fichiers, contrôle les valeurs, ajoute les clés déclarées absentes, puis journal ;
	//! rend le compte rendu (« Réglages relus (Jack) : 212 valeur(s) lue(s), 3 ajoutée(s), 0 problème ») ; ne relance
	//! aucun LoadSettings (c'est l'appelant qui le fait) — SRP_FrontComponent.Start (« démarrage »), ReloadSettings
	static string Reload(string author)
	{
		s_mValues.Clear();
		s_aSeen.Clear();
		s_aWarned.Clear();
		s_aProblems.Clear();
		s_iReadCount = 0;
		s_iAddedCount = 0;
		if (!Replication.IsServer())
			return "";

		FileIO.MakeDirectory(SRP_Paths.ROOT);
		ReadFile(PATH_CMD);
		ReadFile(PATH_FRONT);
		s_iAddedCount = AppendMissing(PATH_CMD, false);
		s_iAddedCount = s_iAddedCount + AppendMissing(PATH_FRONT, true);
		s_iReadUnix = System.GetUnixTime();
		s_sReadAt = HourText();

		string who = author;
		if (who.IsEmpty())
			who = "serveur";
		string report = string.Format("Réglages relus (%1) : %2 valeur(s) lue(s), %3 ajoutée(s) aux fichiers, %4 problème(s)", who, s_iReadCount, s_iAddedCount, s_aProblems.Count());
		Note(report);
		return report;
	}

	//------------------------------------------------------------------------------------------------
	//! « commandeur_reglages.txt et front_reglages.txt lus à 20:01, 0 problème » (ou « pas encore lus ») —
	//! SRP_CmdScreens (page principale), SRP_FrontScreens
	static string GetStatusText()
	{
		if (s_iReadUnix <= 0)
			return "commandeur_reglages.txt et front_reglages.txt : pas encore lus";
		return string.Format("commandeur_reglages.txt et front_reglages.txt lus à %1, %2 problème(s)", s_sReadAt, s_aProblems.Count());
	}

	//------------------------------------------------------------------------------------------------
	//! Heure Unix de la dernière lecture (0 = jamais) — Staff
	static int GetReadUnix()
	{
		return s_iReadUnix;
	}

	//------------------------------------------------------------------------------------------------
	//! Problèmes du dernier Reload (valeurs illisibles, clés inconnues, bornes) ; rend leur nombre — SRP_CmdScreens
	static int GetProblems(notnull array<string> lines)
	{
		lines.Clear();
		foreach (string problem : s_aProblems)
		{
			lines.Insert(problem);
		}
		return lines.Count();
	}

	//================================================================================================
	// Valeurs (valeur lue, sinon le défaut déclaré)
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Texte brut : valeur lue, sinon le défaut déclaré ; clé non déclarée : "" et problème noté — Get*
	static string GetString(string key)
	{
		string k = NormKey(key);
		string value;
		if (s_mValues.Find(k, value))
			return value;
		int index = s_aKeys.Find(k);
		if (index >= 0)
			return s_aDefaults[index];
		Warn(k, "réglage demandé mais jamais déclaré : " + k);
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Entier (un décimal est arrondi, oui/vrai = 1, non/faux = 0) ; illisible : défaut déclaré, sinon 0 — modules
	static int GetInt(string key)
	{
		string text = GetString(key);
		if (IsIntText(text))
			return text.ToInt();
		if (IsFloatText(text))
		{
			int rounded = Math.Round(ParseFloat(text));
			return rounded;
		}
		if (IsBoolWord(text))
		{
			if (IsTrueWord(text))
				return 1;
			return 0;
		}
		string fallback = DefaultOf(key);
		if (IsIntText(fallback))
			return fallback.ToInt();
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Décimal (virgule acceptée) ; illisible : défaut déclaré, sinon 0 — modules
	static float GetFloat(string key)
	{
		string text = GetString(key);
		if (IsFloatText(text))
			return ParseFloat(text);
		string fallback = DefaultOf(key);
		if (IsFloatText(fallback))
			return ParseFloat(fallback);
		return 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Interrupteur : vrai pour 1 (ou tout entier non nul), oui, vrai, true — modules
	static bool GetBool(string key)
	{
		string text = GetString(key);
		if (IsTrueWord(text))
			return true;
		if (IsIntText(text))
			return text.ToInt() != 0;
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Liste « a, b, c » (le point-virgule vaut une virgule) : éléments sans espaces autour, vides retirés — modules
	static void GetList(string key, notnull array<string> result)
	{
		result.Clear();
		string text = GetString(key);
		text.Replace(";", ",");
		array<string> parts = {};
		text.Split(",", parts, true);
		string item;
		foreach (string part : parts)
		{
			item = part.Trim();
			if (!item.IsEmpty())
				result.Insert(item);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Entier borné : une valeur hors bornes est ramenée dans [min, max] et signalée une fois par Reload —
	//! LoadSettings des modules
	static int GetIntClamped(string key, int min, int max)
	{
		int value = GetInt(key);
		if (value < min || value > max)
		{
			int clamped = Math.ClampInt(value, min, max);
			Warn(NormKey(key), string.Format("réglage %1 ramené à %2 (bornes %3 à %4)", NormKey(key), clamped, min, max));
			return clamped;
		}
		return value;
	}

	//------------------------------------------------------------------------------------------------
	//! Décimal borné, même règle — LoadSettings des modules
	static float GetFloatClamped(string key, float min, float max)
	{
		float value = GetFloat(key);
		if (value < min || value > max)
		{
			float clamped = Math.Clamp(value, min, max);
			Warn(NormKey(key), string.Format("réglage %1 ramené à %2 (bornes %3 à %4)", NormKey(key), clamped, min, max));
			return clamped;
		}
		return value;
	}

	//================================================================================================
	// Interne
	//================================================================================================
	//------------------------------------------------------------------------------------------------
	//! Ajoute la clé à la table si elle n'y est pas — Declare*
	protected static void DeclareTyped(string key, string defaultValue, string help, int type)
	{
		string k = NormKey(key);
		if (k.IsEmpty() || s_aKeys.Contains(k))
			return;
		s_aKeys.Insert(k);
		s_aDefaults.Insert(OneLine(defaultValue));
		s_aHelps.Insert(OneLine(help));
		s_aTypes.Insert(type);
	}

	//------------------------------------------------------------------------------------------------
	//! Lit un fichier ligne à ligne (modèle SRP_Charte.c:73-98) : commentaire après #, découpe au premier « = »,
	//! clé en minuscules ; clé inconnue ou valeur illisible -> problème, défaut gardé — Reload
	protected static void ReadFile(string path)
	{
		if (!FileIO.FileExists(path))
			return;
		FileHandle file = FileIO.OpenFile(path, FileMode.READ);
		if (!file)
		{
			Problem("fichier illisible : " + path);
			return;
		}

		string line;
		string body;
		string key;
		string value;
		int cut;
		int index;
		int lineNumber;
		while (file.ReadLine(line) >= 0)
		{
			lineNumber++;
			body = line;
			cut = body.IndexOf("#");
			if (cut >= 0)
				body = body.Substring(0, cut);
			body = body.Trim();
			if (body.IsEmpty())
				continue;

			cut = body.IndexOf("=");
			if (cut <= 0)
			{
				Problem(string.Format("%1, ligne %2 : pas de signe égal, ligne ignorée", FileLabel(path), lineNumber));
				continue;
			}
			key = NormKey(body.Substring(0, cut));
			value = body.Substring(cut + 1, body.Length() - cut - 1);
			value = value.Trim();

			index = s_aKeys.Find(key);
			if (index < 0)
			{
				Problem(string.Format("%1, ligne %2 : réglage inconnu ignoré : %3", FileLabel(path), lineNumber, key));
				continue;
			}
			if (!s_aSeen.Contains(key))
				s_aSeen.Insert(key);
			if (!IsValid(value, s_aTypes[index]))
			{
				Problem(string.Format("%1, ligne %2 : valeur illisible pour %3 (« %4 »), valeur de départ %5 gardée", FileLabel(path), lineNumber, key, value, s_aDefaults[index]));
				continue;
			}
			if (s_mValues.Contains(key))
				Problem(string.Format("%1, ligne %2 : %3 écrit deux fois, la dernière valeur compte", FileLabel(path), lineNumber, key));
			s_mValues.Set(key, value);
			s_iReadCount++;
		}
		file.Close();
	}

	//------------------------------------------------------------------------------------------------
	//! Écrit les clés déclarées de ce fichier qui n'y figurent pas : fichier absent -> en-tête puis tout, groupé par
	//! section ; fichier présent -> « # Ajoutés le AAAA-MM-JJ » en fin de fichier ; rend le nombre de clés écrites —
	//! Reload
	protected static int AppendMissing(string path, bool front)
	{
		array<int> missing = {};
		for (int i = 0; i < s_aKeys.Count(); i++)
		{
			if (IsFrontKey(s_aKeys[i]) != front)
				continue;
			if (s_aSeen.Contains(s_aKeys[i]))
				continue;
			missing.Insert(i);
		}
		if (missing.IsEmpty())
			return 0;

		bool existed = FileIO.FileExists(path);
		FileHandle file;
		if (existed)
			file = FileIO.OpenFile(path, FileMode.APPEND);
		else
			file = FileIO.OpenFile(path, FileMode.WRITE);
		if (!file)
		{
			Problem("écriture impossible : " + path);
			return 0;
		}

		if (existed)
		{
			file.WriteLine("");
			file.WriteLine("# Ajoutés le " + DayText() + " : réglages nouveaux de cette version du mod, avec leur valeur de départ");
		}
		else
			WriteHeader(file, front);

		array<string> sections = {};
		string prefix;
		foreach (int m : missing)
		{
			prefix = SectionOf(s_aKeys[m]);
			if (!sections.Contains(prefix))
				sections.Insert(prefix);
		}
		foreach (string section : sections)
		{
			file.WriteLine("");
			file.WriteLine("# --- " + SectionTitle(section) + " ---");
			foreach (int k : missing)
			{
				if (SectionOf(s_aKeys[k]) == section)
					file.WriteLine(string.Format("%1 = %2   # %3", s_aKeys[k], s_aDefaults[k], s_aHelps[k]));
			}
		}
		file.Close();
		return missing.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! En-tête d'un fichier neuf (mode d'emploi), sans signe pour cent — AppendMissing
	protected static void WriteHeader(FileHandle file, bool front)
	{
		if (front)
			file.WriteLine("# Réglages du FRONT (cartes #75 et #76), lus au démarrage du serveur et par le bouton Staff « Relire les réglages »");
		else
			file.WriteLine("# Réglages du COMMANDEUR ENNEMI (carte #76), lus au démarrage du serveur et par le bouton Staff « Relire les réglages »");
		file.WriteLine("# Une ligne par réglage : clé = valeur   (le texte après # est une aide). Virgule ou point acceptés pour les décimaux.");
		file.WriteLine("# Interrupteurs : 1 ou 0 (oui ou non). Une ligne effacée reprend sa valeur de départ : elle est réécrite en fin de fichier.");
		file.WriteLine("# Durées en secondes (_s), minutes (_min) ou heures (_h) ; distances en mètres (_m) ; parts en centièmes ou « sur 100 ».");
		file.WriteLine("# Valeurs de départ : les choix de Jack (QCM et arbitrages). Modifier la valeur après le signe égal, puis relire les réglages.");
	}

	//------------------------------------------------------------------------------------------------
	//! Section d'une clé = son préfixe avant le premier tiret bas (« appui », « cap », « front »…) — AppendMissing
	protected static string SectionOf(string key)
	{
		int cut = key.IndexOf("_");
		if (cut <= 0)
			return key;
		return key.Substring(0, cut);
	}

	//------------------------------------------------------------------------------------------------
	//! Titre lisible d'une section du fichier — AppendMissing
	protected static string SectionTitle(string section)
	{
		if (section == "cmd")
			return "Cerveau : cycles, comptes rendus, connaissance, caractères, humeur, gravité, déclenchements (CO4 à CO7, MA9)";
		if (section == "info")
			return "Informateurs civils (C4)";
		if (section == "alerte")
			return "Alerte des régions (CO9)";
		if (section == "officier")
			return "Officiers de région (OF2 à OF10)";
		if (section == "desorg")
			return "Désorganisation (OF6, OF7, OF9)";
		if (section == "res")
			return "Stocks et dépôts ennemis (C1, RE2 à RE10)";
		if (section == "cap")
			return "Place des soldats ennemis : 120 soldats, 160 IA (Q7, RE11, RE12, C6)";
		if (section == "renfort")
			return "Renforts (MA1 à MA3, C2)";
		if (section == "colonne")
			return "Colonnes (MA4)";
		if (section == "repli")
			return "Décrochage (MA5 à MA7)";
		if (section == "assaut")
			return "Assaut (MA8)";
		if (section == "harcelement")
			return "Harcèlement du front (MA10)";
		if (section == "reorg")
			return "Réorganisation (MA11)";
		if (section == "ca")
			return "Contre-attaques du Commandeur (CA1 à CA6)";
		if (section == "appui")
			return "Appuis : mortier, artillerie, aviation, blindés, hélico (MO, AP, EQ)";
		if (section == "vu")
			return "Staff, radio et renseignement (VU)";
		if (section == "front")
			return "Front";
		return section;
	}

	//------------------------------------------------------------------------------------------------
	//! Contrôle d'une valeur selon le type déclaré — ReadFile
	protected static bool IsValid(string value, int type)
	{
		if (type == TYPE_INT)
			return IsIntText(value) || IsBoolWord(value);
		if (type == TYPE_FLOAT)
			return IsFloatText(value);
		if (type == TYPE_BOOL)
			return IsBoolWord(value) || IsIntText(value);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Type deviné d'après le défaut : entier, décimal, sinon texte — Declare
	protected static int GuessType(string defaultValue)
	{
		if (IsIntText(defaultValue))
			return TYPE_INT;
		if (IsFloatText(defaultValue))
			return TYPE_FLOAT;
		return TYPE_TEXT;
	}

	//------------------------------------------------------------------------------------------------
	//! Défaut déclaré de la clé, "" si elle ne l'est pas — Get*
	protected static string DefaultOf(string key)
	{
		int index = s_aKeys.Find(NormKey(key));
		if (index < 0)
			return "";
		return s_aDefaults[index];
	}

	//------------------------------------------------------------------------------------------------
	//! Entier écrit en chiffres, signe moins permis — contrôles
	protected static bool IsIntText(string text)
	{
		string s = text.Trim();
		int n = s.Length();
		if (n == 0)
			return false;
		int start = 0;
		if (s.Get(0) == "-")
			start = 1;
		if (start >= n)
			return false;
		for (int i = start; i < n; i++)
		{
			if (!s.IsDigitAt(i))
				return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Décimal : chiffres, un seul séparateur (point ou virgule), signe moins permis — contrôles
	protected static bool IsFloatText(string text)
	{
		string s = text.Trim();
		s.Replace(",", ".");
		int n = s.Length();
		if (n == 0)
			return false;
		int start = 0;
		if (s.Get(0) == "-")
			start = 1;
		int digits = 0;
		int dots = 0;
		for (int i = start; i < n; i++)
		{
			if (s.IsDigitAt(i))
			{
				digits++;
				continue;
			}
			if (s.Get(i) == ".")
			{
				dots++;
				if (dots > 1)
					return false;
				continue;
			}
			return false;
		}
		return digits > 0;
	}

	//------------------------------------------------------------------------------------------------
	//! Lecture d'un décimal déjà contrôlé (virgule acceptée) — GetFloat, GetInt
	protected static float ParseFloat(string text)
	{
		string s = text.Trim();
		s.Replace(",", ".");
		return s.ToFloat();
	}

	//------------------------------------------------------------------------------------------------
	//! Mot d'interrupteur reconnu (oui, non, vrai, faux, true, false) — contrôles
	protected static bool IsBoolWord(string text)
	{
		string s = text.Trim();
		s.ToLower();
		return s == "oui" || s == "non" || s == "vrai" || s == "faux" || s == "true" || s == "false";
	}

	//------------------------------------------------------------------------------------------------
	//! Mot qui vaut vrai (1, oui, vrai, true) — GetBool, GetInt
	protected static bool IsTrueWord(string text)
	{
		string s = text.Trim();
		s.ToLower();
		return s == "1" || s == "oui" || s == "vrai" || s == "true";
	}

	//------------------------------------------------------------------------------------------------
	//! Clé normalisée : sans espaces autour, en minuscules — partout
	protected static string NormKey(string key)
	{
		string k = key.Trim();
		k.ToLower();
		return k;
	}

	//------------------------------------------------------------------------------------------------
	//! Texte sur une seule ligne (défauts et aides écrits dans le fichier) — DeclareTyped
	protected static string OneLine(string text)
	{
		string s = text;
		s.Replace("\r", " ");
		s.Replace("\n", " ");
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom court du fichier pour les messages — ReadFile
	protected static string FileLabel(string path)
	{
		if (path == PATH_FRONT)
			return "front_reglages.txt";
		return "commandeur_reglages.txt";
	}

	//------------------------------------------------------------------------------------------------
	//! « 2026-09-26 » — AppendMissing
	protected static string DayText()
	{
		int y;
		int mo;
		int d;
		System.GetYearMonthDay(y, mo, d);
		return string.Format("%1-%2-%3", y, SRP_Time.Pad2(mo), SRP_Time.Pad2(d));
	}

	//------------------------------------------------------------------------------------------------
	//! « 20:01 » — Reload
	protected static string HourText()
	{
		int h;
		int mi;
		int s;
		System.GetHourMinuteSecond(h, mi, s);
		return string.Format("%1:%2", SRP_Time.Pad2(h), SRP_Time.Pad2(mi));
	}

	//------------------------------------------------------------------------------------------------
	//! Problème du dernier Reload : noté pour le Staff, console et journal — ReadFile, AppendMissing
	protected static void Problem(string text)
	{
		s_aProblems.Insert(text);
		Print("[SRP] Réglages : " + text, LogLevel.WARNING);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, "Réglages : " + text);
	}

	//------------------------------------------------------------------------------------------------
	//! Problème signalé une seule fois par clé et par Reload (bornes, clé non déclarée) — Get*
	protected static void Warn(string key, string text)
	{
		if (s_aWarned.Contains(key))
			return;
		s_aWarned.Insert(key);
		Problem(text);
	}

	//------------------------------------------------------------------------------------------------
	//! Ligne d'information (compte rendu de lecture) : console et journal — Reload
	protected static void Note(string text)
	{
		Print("[SRP] " + text, LogLevel.NORMAL);
		SRP_CmdLog.Note(-1, SRP_ECmdLogKind.SYSTEME, text);
	}
}
