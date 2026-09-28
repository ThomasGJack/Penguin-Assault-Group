//------------------------------------------------------------------------------------------------
// SimpleRP — Pont Discord
// Chaque ligne du journal (SRP_JournalComponent.Log) est routée vers un flux Discord selon sa
// catégorie, mise en attente, puis envoyée regroupée toutes les N secondes sur le webhook du flux,
// sous forme d'embeds (un encadré coloré par catégorie, plusieurs lignes dedans).
//   - flux publics : missions, territoire, logistique, promotions, effectifs, alertes
//     (territoire ne reçoit que les zones du front : catégorie TERRITOIRE ; les lignes carré par carré, catégorie
//     FRONT, n'y vont que si m_bFrontSquaresPublic est coché)
//   - journal-serveur (Staff) : tout, brut, avec les identités (FRONT selon m_bFrontSquaresStaff)
//   - etat-major (Staff) : décisions du Commandeur ennemi (catégorie COMMANDEUR) ; à défaut, journal-serveur ;
//     jamais un flux public
// Confidentialité : une ligne qui contient un des mots réglés sur le composant (garnison, groupe(s),
// emprise ennemie, tirage…) ne part que dans journal-serveur. Les identités Bohemia sont masquées en public.
//
// Limites Discord respectées : 30 messages par minute et par webhook, 10 embeds par message,
// 6 000 caractères par message, 4 096 par description.
//
// Fichier : $profile:SimpleRP/discord.json, copie du webhooks.json produit par setup_discord.py :
//   { "missions": "https://discord.com/api/webhooks/ID/JETON", "territoire": "...", "logistique": "...",
//     "promotions": "...", "effectifs": "...", "journal-serveur": "...", "alertes": "...", "etat-major": "..." }
// Ce fichier vit dans le profil du serveur, jamais dans le dossier du mod (il partirait au Workshop).
//
// API REST Enfusion 1.8 : GetGame().GetRestApi().GetContext(url) -> RestContext.POST(cb, chemin, json) ;
// le callback se règle par RestCallback.SetOnSuccess / SetOnError et doit rester référencé tant que
// la requête est en vol (sinon il est détruit avant la réponse).
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Pont Discord SimpleRP : le journal vers les webhooks, en embeds (profil/SimpleRP/discord.json)")]
class SRP_DiscordComponentClass : SCR_BaseGameModeComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Une ligne du journal en attente d'envoi
class SRP_DiscordLine
{
	string m_sCategorie;
	string m_sHeure;							// "23:20"
	string m_sTexte;
	int m_iColor;								// couleur de l'embed (décimal)
}

//------------------------------------------------------------------------------------------------
//! Un flux Discord : son webhook et ses lignes en attente
class SRP_DiscordFeed
{
	string m_sName;								// clé dans discord.json (missions, territoire…)
	string m_sPath;								// "ID/JETON", la partie après https://discord.com/api/webhooks/
	ref array<ref SRP_DiscordLine> m_aPending = {};
	int m_iErrors = 0;							// erreurs consécutives
	bool m_bDisabled = false;					// coupé après trop d'erreurs
}

//------------------------------------------------------------------------------------------------
//! Une requête en vol : le callback REST et le flux concerné
class SRP_DiscordRequest : RestCallback
{
	protected SRP_DiscordComponent m_Owner;
	protected string m_sFeed;

	//------------------------------------------------------------------------------------------------
	void SRP_DiscordRequest(SRP_DiscordComponent owner, string feed)
	{
		m_Owner = owner;
		m_sFeed = feed;
		SetOnSuccess(OnDoneOk);
		SetOnError(OnDoneError);
	}

	//------------------------------------------------------------------------------------------------
	string GetFeed()
	{
		return m_sFeed;
	}

	//------------------------------------------------------------------------------------------------
	void OnDoneOk(RestCallback cb)
	{
		int code = cb.GetHttpCode();
		if (m_Owner)
			m_Owner.OnRequestDone(this, true, code);
	}

	//------------------------------------------------------------------------------------------------
	void OnDoneError(RestCallback cb)
	{
		int code = cb.GetHttpCode();
		if (m_Owner)
			m_Owner.OnRequestDone(this, false, code);
	}
}

//------------------------------------------------------------------------------------------------
class SRP_DiscordComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Envoyer le journal sur Discord (fichier profil/SimpleRP/discord.json requis)", category: "SimpleRP")]
	protected bool m_bEnabled;

	[Attribute("10", UIWidgets.EditBox, "Regroupement : secondes entre deux envois par flux (Discord limite chaque webhook à 30 messages par minute)", category: "SimpleRP")]
	protected int m_iFlushSeconds;

	[Attribute("SimpleRP", UIWidgets.EditBox, "Nom d'expéditeur affiché dans Discord", category: "SimpleRP")]
	protected string m_sUsername;

	[Attribute("SimpleRP · PAG", UIWidgets.EditBox, "Pied de page des embeds", category: "SimpleRP")]
	protected string m_sFooter;

	[Attribute("1", UIWidgets.CheckBox, "Masquer les identités Bohemia dans les flux publics (elles restent dans journal-serveur, réservé au Staff)", category: "SimpleRP")]
	protected bool m_bHideIdentities;

	[Attribute("1", UIWidgets.CheckBox, "Confidentialité : une ligne qui contient un des mots ci-dessous ne part que dans journal-serveur (Staff)", category: "SimpleRP - Confidentialité")]
	protected bool m_bFilterConfidential;

	[Attribute("", UIWidgets.Auto, "Mots confidentiels (sans tenir compte de la casse). Vide = liste par défaut du code : garnison, groupe(s), emprise ennemie, patrouille, automatique, candidat, chargé, premier lancement, démarré, remise à zéro, à ce jour, aucun site, aucun secteur, aucune zone, à nous, menace, prefab, catalogue, le plus éloigné, tirage", category: "SimpleRP - Confidentialité")]
	protected ref array<string> m_aConfidential;

	[Attribute("0", UIWidgets.CheckBox, "Envoyer aussi les catégories bavardes (CIVILS, ENNEMI, SPAWN, SAUVEGARDE, SYSTEME, FRONT) dans journal-serveur", category: "SimpleRP")]
	protected bool m_bVerbose;

	[Attribute("0", UIWidgets.CheckBox, "Envoyer aussi les lignes carré par carré (catégorie FRONT : un carré pris ou perdu) dans le fil public territoire", category: "SimpleRP - Front")]
	protected bool m_bFrontSquaresPublic;

	[Attribute("1", UIWidgets.CheckBox, "Envoyer les lignes carré par carré (catégorie FRONT) dans journal-serveur (Staff)", category: "SimpleRP - Front")]
	protected bool m_bFrontSquaresStaff;

	protected static SRP_DiscordComponent s_Instance;

	protected static const string BASE_URL = "https://discord.com/api/webhooks/";
	protected static const string CONFIG_FILE = "$profile:SimpleRP/discord.json";
	protected static const int MAX_EMBEDS = 10;					// limite Discord par message
	protected static const int DESC_MAX = 3500;					// limite Discord : 4 096 par description
	protected static const int TOTAL_MAX = 5500;				// limite Discord : 6 000 par message
	protected static const int MAX_MESSAGES_PER_FLUSH = 2;		// par flux et par cycle
	protected static const int MAX_PENDING = 200;				// lignes gardées par flux si le réseau ne suit pas
	protected static const int MAX_ERRORS = 5;					// erreurs consécutives avant de couper un flux

	// Couleurs (décimal) : orange 15105570, ambre 15965202, vert 2600544, vert clair 3066993, bleu 3049153,
	// rouge 12597547, rouge sombre 8070172, rouge alerte 15158332, gris 8359053, gris clair 9807270,
	// or 15844367, violet 9323693, sarcelle 1482885, marine 2051705, ardoise 2899536, défaut 3426654, mauve 10181046
	protected static const int COLOR_DEFAULT = 3426654;

	protected RestContext m_Context;
	protected ref array<ref SRP_DiscordFeed> m_aFeeds = {};
	protected ref array<ref SRP_DiscordRequest> m_aInFlight = {};
	protected ref array<string> m_aConfidentialLower = {};
	protected bool m_bReady = false;

	//------------------------------------------------------------------------------------------------
	static SRP_DiscordComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;

		if (!Replication.IsServer() || !m_bEnabled)
			return;

		if (!LoadConfig())
			return;

		RestApi api = GetGame().GetRestApi();
		if (!api)
		{
			SRP_JournalComponent.Log("ERREUR", "Pont Discord : API REST indisponible");
			return;
		}

		m_Context = api.GetContext(BASE_URL);
		if (!m_Context)
		{
			SRP_JournalComponent.Log("ERREUR", "Pont Discord : contexte REST introuvable pour " + BASE_URL);
			return;
		}
		m_Context.SetHeaders("Content-Type,application/json");
		m_Context.SetTimeout(15);

		if (m_iFlushSeconds < 3)
			m_iFlushSeconds = 3;

		BuildConfidentialList();

		m_bReady = true;
		GetGame().GetCallqueue().CallLater(Flush, m_iFlushSeconds * 1000, true);

		SRP_JournalComponent.Log("SYSTEME", string.Format("Pont Discord démarré : %1 flux, envoi toutes les %2 s, %3 mot(s) confidentiel(s)", m_aFeeds.Count(), m_iFlushSeconds, m_aConfidentialLower.Count()));
		Queue("journal-serveur", "SYSTEME", HourNow(), string.Format("Pont Discord démarré : %1 flux", m_aFeeds.Count()));
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_bReady)
			GetGame().GetCallqueue().Remove(Flush);
		m_bReady = false;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Lit discord.json : une URL de webhook par flux
	protected bool LoadConfig()
	{
		if (!FileIO.FileExists(CONFIG_FILE))
		{
			SRP_JournalComponent.Log("SYSTEME", "Pont Discord : pas de fichier " + CONFIG_FILE + ", pont inactif");
			return false;
		}

		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(CONFIG_FILE))
		{
			SRP_JournalComponent.Log("ERREUR", "Pont Discord : " + CONFIG_FILE + " illisible");
			return false;
		}

		array<string> names = {"missions", "territoire", "logistique", "promotions", "effectifs", "journal-serveur", "alertes", "etat-major"};
		foreach (string name : names)
		{
			string url = "";
			ctx.ReadValue(name, url);
			if (url.IsEmpty())
				continue;

			int at = url.IndexOf("/api/webhooks/");
			if (at < 0)
			{
				SRP_JournalComponent.Log("ERREUR", "Pont Discord : URL de webhook invalide pour le flux " + name);
				continue;
			}
			int start = at + 14;	// longueur de "/api/webhooks/"

			SRP_DiscordFeed feed = new SRP_DiscordFeed();
			feed.m_sName = name;
			feed.m_sPath = url.Substring(start, url.Length() - start);
			m_aFeeds.Insert(feed);
		}

		if (m_aFeeds.IsEmpty())
		{
			SRP_JournalComponent.Log("ERREUR", "Pont Discord : aucun flux valide dans " + CONFIG_FILE);
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Liste des mots confidentiels en minuscules (celle du composant, sinon celle du code)
	protected void BuildConfidentialList()
	{
		m_aConfidentialLower.Clear();

		array<string> defaults = {"garnison", "groupe(s)", "emprise ennemie", "patrouille", "automatique", "candidat", "chargé",
			"premier lancement", "démarré", "remise à zéro", "à ce jour", "aucun site", "aucun secteur", "aucune zone",
			"à nous, menace", "prefab", "catalogue", "le plus éloigné", "tirage"};
		array<string> source = m_aConfidential;
		if (!source || source.IsEmpty())
			source = defaults;

		foreach (string word : source)
		{
			string low = word;
			low.ToLower();
			low = low.Trim();
			if (!low.IsEmpty())
				m_aConfidentialLower.Insert(low);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected SRP_DiscordFeed FindFeed(string name)
	{
		foreach (SRP_DiscordFeed feed : m_aFeeds)
		{
			if (feed.m_sName == name)
				return feed;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	// Routage
	//------------------------------------------------------------------------------------------------
	//! Appelé par SRP_JournalComponent.Log pour chaque ligne du journal
	static void Push(string categorie, string message)
	{
		if (!s_Instance || !s_Instance.m_bReady)
			return;
		s_Instance.Route(categorie, message);
	}

	//------------------------------------------------------------------------------------------------
	protected void Route(string categorie, string message)
	{
		string heure = HourNow();

		// Décisions du Commandeur ennemi (VU4) : Staff seul, flux « etat-major », à défaut journal-serveur (flux absent
		// de discord.json ou coupé) ; jamais un flux public, jamais en double dans journal-serveur
		if (categorie == "COMMANDEUR")
		{
			string cmdFeed = "etat-major";
			SRP_DiscordFeed staffFeed = FindFeed(cmdFeed);
			if (!staffFeed || staffFeed.m_bDisabled)
				cmdFeed = "journal-serveur";
			Queue(cmdFeed, categorie, heure, message);
			return;
		}

		bool secret = m_bFilterConfidential && IsConfidential(message);

		string target = PublicFeedFor(categorie);
		// Carré par carré (FRONT) : le fil public territoire ne les reçoit que sur demande
		if (categorie == "FRONT" && !m_bFrontSquaresPublic)
			target = "";
		if (!target.IsEmpty() && !secret)
		{
			string texte = message;
			if (m_bHideIdentities)
				texte = HideIdentities(message);
			Queue(target, categorie, heure, texte);
		}

		// Le journal brut, réservé au Staff : tout, sauf les catégories bavardes si non demandé ; les lignes FRONT
		// suivent leur propre case (m_bFrontSquaresStaff), ou le mode bavard
		bool staffCopy = m_bVerbose || !IsChatty(categorie);
		if (categorie == "FRONT")
			staffCopy = m_bFrontSquaresStaff || m_bVerbose;
		if (staffCopy)
			Queue("journal-serveur", categorie, heure, message);
	}

	//------------------------------------------------------------------------------------------------
	//! La ligne contient-elle un mot confidentiel ?
	protected bool IsConfidential(string message)
	{
		string low = message;
		low.ToLower();
		foreach (string word : m_aConfidentialLower)
		{
			if (low.Contains(word))
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Flux public d'une catégorie ("" = aucun)
	protected static string PublicFeedFor(string categorie)
	{
		if (categorie == "MISSION")
			return "missions";
		if (categorie == "TERRITOIRE")
			return "territoire";
		// Carrés du front : même fil, mais Route ne l'utilise que si m_bFrontSquaresPublic est coché
		if (categorie == "FRONT")
			return "territoire";
		// MEDICAL ne part plus dans un flux public : les soins restent dans journal-serveur (Staff) et le journal
		if (categorie == "TRESORERIE" || categorie == "FLOTTE" || categorie == "LIVRAISON" || categorie == "RESSOURCE" || categorie == "ATELIER" || categorie == "ARSENAL")
			return "logistique";
		if (categorie == "GRADE" || categorie == "CERTIF" || categorie == "SERVICE" || categorie == "CHARTE")
			return "promotions";
		if (categorie == "CONNEXION" || categorie == "DECONNEXION" || categorie == "MORT")
			return "effectifs";
		if (categorie == "AVERTISSEMENT" || categorie == "INFRACTION" || categorie == "STAFF" || categorie == "ERREUR")
			return "alertes";
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Catégories bavardes : pas dans journal-serveur sans m_bVerbose (FRONT : voir Route, m_bFrontSquaresStaff)
	protected static bool IsChatty(string categorie)
	{
		return categorie == "CIVILS" || categorie == "ENNEMI" || categorie == "SPAWN" || categorie == "SAUVEGARDE" || categorie == "SYSTEME" || categorie == "FRONT";
	}

	//------------------------------------------------------------------------------------------------
	//! Titre de l'embed : émoji + libellé
	protected static string TitleFor(string categorie)
	{
		if (categorie == "MISSION") return "🎯 Mission";
		if (categorie == "TERRITOIRE") return "🚩 Territoire";
		if (categorie == "FRONT") return "🗺 Front";
		if (categorie == "COMMANDEUR") return "🎖 État-major ennemi";
		if (categorie == "TRESORERIE") return "💰 Trésorerie";
		if (categorie == "FLOTTE") return "🚗 Parc automobile";
		if (categorie == "LIVRAISON") return "📦 Livraison";
		if (categorie == "RESSOURCE") return "🧰 Ressources";
		if (categorie == "ATELIER") return "🔧 Atelier";
		if (categorie == "ARSENAL") return "🔫 Arsenal";
		if (categorie == "MEDICAL") return "🏥 Infirmerie";
		if (categorie == "GRADE") return "🎖 Grade";
		if (categorie == "CERTIF") return "🎓 Certification";
		if (categorie == "SERVICE") return "🏅 États de service";
		if (categorie == "CHARTE") return "📜 Charte";
		if (categorie == "CONNEXION") return "🟢 Connexion";
		if (categorie == "DECONNEXION") return "⚪ Déconnexion";
		if (categorie == "MORT") return "☠ Mort";
		if (categorie == "AVERTISSEMENT") return "⚠ Avertissement";
		if (categorie == "INFRACTION") return "🚫 Infraction";
		if (categorie == "STAFF") return "🛡 Staff";
		if (categorie == "ERREUR") return "❌ Erreur";
		if (categorie == "SYSTEME") return "⚙ Système";
		if (categorie == "CIVILS") return "👥 Civils";
		if (categorie == "ENNEMI") return "💀 Ennemi";
		if (categorie == "HELICO") return "🚁 Hélico";
		if (categorie == "SPAWN") return "🪖 Spawn";
		if (categorie == "SAUVEGARDE") return "💾 Sauvegarde";
		if (categorie == "BRECHE") return "💥 Brèche";
		return "📖 " + categorie;
	}

	//------------------------------------------------------------------------------------------------
	//! Couleur de l'embed selon la catégorie, nuancée par le contenu pour les missions, le front (TERRITOIRE, FRONT)
	//! et le Commandeur ennemi (COMMANDEUR)
	protected static int ColorFor(string categorie, string texte)
	{
		string low = texte;
		low.ToLower();

		if (categorie == "MISSION")
		{
			if (low.Contains("succès") || low.Contains("réussi"))
				return 2600544;		// vert
			if (low.Contains("échec") || low.Contains("échou") || low.Contains("annul") || low.Contains("expir") || low.Contains("perdu"))
				return 8359053;		// gris
			if (low.Contains("nouvelle mission"))
				return 15105570;	// orange
			return 15965202;		// ambre
		}
		if (categorie == "TERRITOIRE" || categorie == "FRONT")
			return FrontColor(categorie, low);
		if (categorie == "COMMANDEUR")
			return CommanderColor(low);
		if (categorie == "TRESORERIE") return 15844367;		// or
		if (categorie == "FLOTTE") return 8359053;			// gris
		if (categorie == "LIVRAISON") return 9323693;		// violet
		if (categorie == "RESSOURCE" || categorie == "ATELIER" || categorie == "ARSENAL") return 1482885;	// sarcelle
		if (categorie == "MEDICAL") return 15158332;		// rouge alerte
		if (categorie == "GRADE") return 2051705;			// marine
		if (categorie == "CERTIF") return 2600544;			// vert
		if (categorie == "SERVICE") return 3066993;			// vert clair
		if (categorie == "CHARTE") return 9807270;			// gris clair
		if (categorie == "CONNEXION") return 3066993;		// vert clair
		if (categorie == "DECONNEXION") return 9807270;		// gris clair
		if (categorie == "MORT") return 2899536;			// ardoise
		if (categorie == "AVERTISSEMENT") return 15105570;	// orange
		if (categorie == "INFRACTION") return 12597547;		// rouge
		if (categorie == "STAFF") return 10181046;			// mauve
		if (categorie == "ERREUR") return 15158332;			// rouge alerte
		return COLOR_DEFAULT;
	}

	//------------------------------------------------------------------------------------------------
	//! Couleur d'une ligne du front (TERRITOIRE : niveau zone ; FRONT : carré par carré), texte déjà en minuscules.
	//! Le premier mot trouvé décide, dans cet ordre : les faits du Commandeur annoncés au public passent avant
	//! « captur » et « ramené » (« officier ennemi … capturé », « blindé ennemi … ramené ») ; les pertes passent avant
	//! les prises (« reprise » contient « prise »).
	protected static int FrontColor(string categorie, string low)
	{
		// Journal FRONT : « Carré 074 042 passé au bleu (capture) », « … passé au rouge (reprise par l'ennemi) »
		if (categorie == "FRONT")
		{
			if (low.Contains("au rouge"))
				return 8070172;		// rouge sombre
			if (low.Contains("au bleu"))
				return 3049153;		// bleu
		}
		// L'annonce d'attaque commence par « Renseignement : » : elle reste orange
		if (low.Contains("prépare une attaque"))
			return 15105570;		// orange
		if (low.Contains("officier ennemi") || low.Contains("blindé ennemi"))
			return 2600544;			// vert
		if (low.Contains("renseignement"))
			return 2051705;			// marine
		if (low.Contains("dépôt ennemi"))
			return 1482885;			// sarcelle
		if (low.Contains("victoire"))
			return 15844367;		// or
		if (low.Contains("nouvelle campagne"))
			return 9323693;			// violet
		// L'annonce de coupure dit aussi « perdue dans 24 h » : elle reste ambre (la perte, elle, dit « restée coupée »)
		if (low.Contains("est coupée"))
			return 15965202;		// ambre
		// « PERDUE » ; l'offensive de la nuit réussie (« l'ennemi a pris … ») ; capture abandonnée (« capture de … »)
		if (low.Contains("perdu") || low.Contains("l'ennemi a pris") || low.Contains("abandonnée"))
			return 8070172;			// rouge sombre
		if (low.Contains("coupée"))
			return 15965202;		// ambre
		// Front gelé ou dégelé, front ramené à une copie datée
		if (low.Contains("gelé") || low.Contains("ramené"))
			return 9807270;			// gris clair
		// « pris » avec l'espace (« carré 074 042 pris ») ; offensive de la nuit repoussée (« sans parvenir à la prendre »)
		if (low.Contains("captur") || low.Contains("prise") || low.Contains("pris ") || low.Contains("défendu") || low.Contains("repouss") || low.Contains("reliée") || low.Contains("sans parvenir"))
			return 3049153;			// bleu
		if (low.Contains("attaque") || low.Contains("vague") || low.Contains("assaut"))
			return 15105570;		// orange
		if (low.Contains("produit") || low.Contains("paquet") || low.Contains("installé"))
			return 1482885;			// sarcelle
		return 12597547;			// rouge
	}

	//------------------------------------------------------------------------------------------------
	//! Couleur d'une ligne du Commandeur ennemi (flux etat-major), texte déjà en minuscules, premier mot trouvé
	protected static int CommanderColor(string low)
	{
		if (low.Contains("[staff]"))
			return 10181046;		// mauve : action du Staff
		if (low.Contains("écarté"))
			return 8359053;			// gris : décision envisagée puis écartée
		if (low.Contains("artillerie") || low.Contains("bombe") || low.Contains("blindé"))
			return 15158332;		// rouge alerte : gros moyens
		if (low.Contains("mortier") || low.Contains("fumée") || low.Contains("feinte"))
			return 15105570;		// orange
		if (low.Contains("renfort") || low.Contains("colonne") || low.Contains("décroch") || low.Contains("harcèlement") || low.Contains("assaut"))
			return 15965202;		// ambre : manœuvres
		if (low.Contains("officier"))
			return 9323693;			// violet
		if (low.Contains("renseignement"))
			return 2051705;			// marine
		if (low.Contains("dépôt") || low.Contains("revenu") || low.Contains("stock"))
			return 1482885;			// sarcelle
		return 2899536;				// ardoise
	}

	//------------------------------------------------------------------------------------------------
	//! "23:20" à partir de SRP_Time.Now() ("2026-09-13 23:20:03")
	protected static string HourNow()
	{
		string now = SRP_Time.Now();
		if (now.Length() >= 16)
			return now.Substring(11, 5);
		return now;
	}

	//------------------------------------------------------------------------------------------------
	//! Retire les " [identité]" (UUID Bohemia ou local_pseudo) d'une ligne, garde les autres crochets
	protected static string HideIdentities(string text)
	{
		string done = "";
		string todo = text;
		int guard = 0;
		while (guard < 20)
		{
			guard++;
			int open = todo.IndexOf(" [");
			if (open < 0)
				break;

			string rest = todo.Substring(open + 2, todo.Length() - open - 2);
			int close = rest.IndexOf("]");
			if (close < 0)
				break;

			string inside = rest.Substring(0, close);
			string after = rest.Substring(close + 1, rest.Length() - close - 1);
			bool isIdentity = inside.Length() >= 8 && !inside.Contains(" ") && (inside.Contains("-") || inside.IndexOf("local_") == 0);

			if (isIdentity)
				done += todo.Substring(0, open);
			else
				done += todo.Substring(0, open + 2 + close + 1);
			todo = after;
		}
		return done + todo;
	}

	//------------------------------------------------------------------------------------------------
	protected void Queue(string feedName, string categorie, string heure, string texte)
	{
		SRP_DiscordFeed feed = FindFeed(feedName);
		if (!feed || feed.m_bDisabled)
			return;

		SRP_DiscordLine line = new SRP_DiscordLine();
		line.m_sCategorie = categorie;
		line.m_sHeure = heure;
		line.m_sTexte = texte;
		line.m_iColor = ColorFor(categorie, texte);

		feed.m_aPending.Insert(line);
		while (feed.m_aPending.Count() > MAX_PENDING)
			feed.m_aPending.RemoveOrdered(0);
	}

	//------------------------------------------------------------------------------------------------
	// Envoi
	//------------------------------------------------------------------------------------------------
	//! Toutes les N secondes : pour chaque flux, au plus quelques messages regroupés
	protected void Flush()
	{
		if (!m_bReady || !m_Context)
			return;

		foreach (SRP_DiscordFeed feed : m_aFeeds)
		{
			if (feed.m_bDisabled)
				continue;

			int sent = 0;
			while (!feed.m_aPending.IsEmpty() && sent < MAX_MESSAGES_PER_FLUSH)
			{
				string json = BuildMessage(feed);
				if (json.IsEmpty())
					break;
				Send(feed, json);
				sent++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Consomme les lignes en attente d'un flux et construit un message : un embed par groupe de
	//! lignes consécutives de même catégorie et de même couleur, dans les limites Discord
	protected string BuildMessage(SRP_DiscordFeed feed)
	{
		string embeds = "";
		int embedCount = 0;
		int total = 0;

		string curKey = "";
		string curTitle = "";
		int curColor = 0;
		string curDesc = "";

		while (!feed.m_aPending.IsEmpty())
		{
			SRP_DiscordLine line = feed.m_aPending[0];

			string entry = "`" + line.m_sHeure + "` " + line.m_sTexte;
			if (entry.Length() > DESC_MAX)
				entry = entry.Substring(0, DESC_MAX);

			string key = line.m_sCategorie + "#" + line.m_iColor.ToString();
			bool sameGroup = (key == curKey) && !curDesc.IsEmpty() && (curDesc.Length() + 1 + entry.Length() <= DESC_MAX);

			if (sameGroup)
			{
				if (total + 1 + entry.Length() > TOTAL_MAX)
					break;
				curDesc += "\n" + entry;
				total = total + 1 + entry.Length();
			}
			else
			{
				if (embedCount >= MAX_EMBEDS)
					break;
				if (total + entry.Length() > TOTAL_MAX)
					break;

				if (!curDesc.IsEmpty())
					embeds = AppendEmbed(embeds, curTitle, curColor, curDesc);

				curKey = key;
				curTitle = TitleFor(line.m_sCategorie);
				curColor = line.m_iColor;
				curDesc = entry;
				embedCount++;
				total = total + entry.Length();
			}

			feed.m_aPending.RemoveOrdered(0);
		}

		if (!curDesc.IsEmpty())
			embeds = AppendEmbed(embeds, curTitle, curColor, curDesc);

		if (embeds.IsEmpty())
			return "";

		return "{\"username\":\"" + JsonEscape(m_sUsername) + "\",\"allowed_mentions\":{\"parse\":[]},\"embeds\":[" + embeds + "]}";
	}

	//------------------------------------------------------------------------------------------------
	protected string AppendEmbed(string embeds, string title, int color, string description)
	{
		string embed = "{\"title\":\"" + JsonEscape(title) + "\",\"description\":\"" + JsonEscape(description) + "\",\"color\":" + color.ToString() + ",\"footer\":{\"text\":\"" + JsonEscape(m_sFooter) + "\"}}";
		if (embeds.IsEmpty())
			return embed;
		return embeds + "," + embed;
	}

	//------------------------------------------------------------------------------------------------
	protected void Send(SRP_DiscordFeed feed, string json)
	{
		SRP_DiscordRequest request = new SRP_DiscordRequest(this, feed.m_sName);
		m_aInFlight.Insert(request);
		m_Context.POST(request, feed.m_sPath, json);
	}

	//------------------------------------------------------------------------------------------------
	//! Réponse d'une requête (appelé par SRP_DiscordRequest)
	void OnRequestDone(SRP_DiscordRequest request, bool ok, int httpCode)
	{
		SRP_DiscordFeed feed = FindFeed(request.GetFeed());
		m_aInFlight.RemoveItem(request);

		if (!feed)
			return;

		// Discord répond 204 (sans contenu) ou 200 : succès
		if (ok && httpCode < 400)
		{
			feed.m_iErrors = 0;
			return;
		}

		feed.m_iErrors++;
		if (feed.m_iErrors == 1)
			Print(string.Format("[SRP] Pont Discord : échec sur le flux %1 (HTTP %2)", feed.m_sName, httpCode), LogLevel.WARNING);

		if (feed.m_iErrors >= MAX_ERRORS)
		{
			feed.m_bDisabled = true;
			feed.m_aPending.Clear();
			SRP_JournalComponent.Log("ERREUR", string.Format("Pont Discord : flux %1 coupé après %2 échecs (HTTP %3) — webhook supprimé ou réseau sortant bloqué ?", feed.m_sName, feed.m_iErrors, httpCode));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Échappe une chaîne pour l'insérer dans un JSON
	protected static string JsonEscape(string text)
	{
		string s = text;
		s.Replace("\\", "\\\\");
		s.Replace("\"", "\\\"");
		s.Replace("\r", "");
		s.Replace("\n", "\\n");
		s.Replace("\t", " ");
		return s;
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne de test sur tous les flux (menu Staff)
	string SendTest(string author)
	{
		if (!m_bReady)
			return "Pont Discord inactif (pas de discord.json ou erreur au démarrage)";
		string heure = HourNow();
		foreach (SRP_DiscordFeed feed : m_aFeeds)
			Queue(feed.m_sName, "SYSTEME", heure, "Test du pont Discord par " + author + " : ce flux fonctionne");
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : message de test Discord envoyé sur %2 flux", author, m_aFeeds.Count()));
		return string.Format("Message de test mis en file sur %1 flux, envoi dans %2 s au plus", m_aFeeds.Count(), m_iFlushSeconds);
	}

	//------------------------------------------------------------------------------------------------
	//! État lisible (menu Staff, /admin)
	string GetStatusText()
	{
		if (!m_bEnabled)
			return "Pont Discord : désactivé";
		if (!m_bReady)
			return "Pont Discord : inactif (pas de discord.json ou erreur au démarrage)";

		string text = string.Format("Pont Discord : %1 flux, %2 requête(s) en vol, %3 mot(s) confidentiel(s)", m_aFeeds.Count(), m_aInFlight.Count(), m_aConfidentialLower.Count());
		foreach (SRP_DiscordFeed feed : m_aFeeds)
		{
			string state = "ok";
			if (feed.m_bDisabled)
				state = "COUPÉ";
			else if (feed.m_iErrors > 0)
				state = string.Format("%1 échec(s)", feed.m_iErrors);
			text += string.Format("\n  %1 : %2, %3 en attente", feed.m_sName, state, feed.m_aPending.Count());
		}
		return text;
	}
}
