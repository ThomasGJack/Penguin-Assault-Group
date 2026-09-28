//------------------------------------------------------------------------------------------------
// SimpleRP — Gestion des joueurs
// Reconnaît chaque joueur par son identité Bohemia, charge ou crée sa fiche, la sauvegarde
// à la déconnexion, à l'arrêt du jeu et à intervalle régulier. Porte l'API grades /
// certifications / staff utilisée par les commandes et, plus tard, par les terminaux.
// Tout tourne côté serveur (autorité). Sur les clients, le composant existe mais ne fait rien.
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Gestion des joueurs SimpleRP : identité, grade, certifications, persistance")]
class SRP_PlayerManagerComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_PlayerManagerComponent : SCR_BaseGameModeComponent
{
	[Attribute("", UIWidgets.Auto, "Identités Bohemia des membres du Staff (copie-les depuis le journal après une première connexion)", category: "SimpleRP")]
	protected ref array<string> m_aStaffIdentities;

	[Attribute("", UIWidgets.ResourceNamePicker, "Arbre des certifications (fichier SRP_CertifTree.conf : prérequis et grade minimum par certif ; vide = règles par défaut du code)", "conf", category: "SimpleRP")]
	protected ResourceName m_sCertifTreePath;

	[Attribute("1", UIWidgets.CheckBox, "La FGI est exigée pour tout grade au-dessus de Recrue (le Staff peut forcer)", category: "SimpleRP")]
	protected bool m_bFgiRequiredForGrade;

	[Attribute("300", UIWidgets.EditBox, "Intervalle de sauvegarde automatique, en secondes", category: "SimpleRP")]
	protected int m_iAutosaveInterval;

	protected static const int IDENTITY_MAX_ATTEMPTS = 5;		// 5 essais x 2 s avant repli "local_"
	protected static const int POSITION_REFRESH_MS = 5000;		// mémorisation de la position toutes les 5 s (en mémoire, pas sur disque)

	protected static SRP_PlayerManagerComponent s_Instance;

	protected ref map<int, ref SRP_PlayerRecord> m_mRecords = new map<int, ref SRP_PlayerRecord>();	// playerId -> fiche
	protected ref map<int, int> m_mLastTick = new map<int, int>();									// playerId -> dernier comptage du temps de jeu

	//------------------------------------------------------------------------------------------------
	static SRP_PlayerManagerComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		SRP_MissionRequests.Reset();

		// Sur toutes les machines : enregistre nos /commandes dans le chat (le gestionnaire
		// de chat est recréé à chaque partie, donc à refaire à chaque démarrage)
		SRP_ChatCommands.Register();

		if (!Replication.IsServer())
			return;

		SRP_Paths.EnsureDirectories();
		LoadCertifTree();

		if (m_iAutosaveInterval < 30)
			m_iAutosaveInterval = 30;

		GetGame().GetCallqueue().CallLater(SaveAll, m_iAutosaveInterval * 1000, true);
		GetGame().GetCallqueue().CallLater(RefreshPositions, POSITION_REFRESH_MS, true);
		SRP_JournalComponent.Log("SYSTEME", string.Format("Gestion des joueurs démarrée, sauvegarde auto toutes les %1 s", m_iAutosaveInterval));
	}

	//------------------------------------------------------------------------------------------------
	//! Arrêt du jeu (fermeture du serveur, Échap en Workbench) : on sauvegarde tout le monde
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer() && m_mRecords.Count() > 0)
		{
			SaveAll();
			SRP_JournalComponent.Log("SYSTEME", "Arrêt du jeu : fiches sauvegardées");
		}

		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Connexion / déconnexion
	//------------------------------------------------------------------------------------------------
	override void OnPlayerConnected(int playerId)
	{
		super.OnPlayerConnected(playerId);

		if (!Replication.IsServer())
			return;

		TryRegister(playerId, 0);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerAuditSuccess(int playerId)
	{
		super.OnPlayerAuditSuccess(playerId);

		if (!Replication.IsServer())
			return;

		// Audit réussi : l'identité Bohemia est disponible, on enregistre sans attendre
		TryRegister(playerId, IDENTITY_MAX_ATTEMPTS);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected(playerId, cause, timeout);

		if (!Replication.IsServer())
			return;

		SRP_PlayerRecord record;
		if (!m_mRecords.Find(playerId, record))
			return;

		UpdateLiveData(playerId, record);
		record.Save();

		SRP_JournalComponent.Log("DECONNEXION", string.Format("%1 [%2] — %3 min de jeu au total", record.m_sName, record.m_sIdentity, record.m_iPlaytimeSeconds / 60));

		// Le corps reste sur place (SRP_DisconnectComponent)
		SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
		if (disconnect)
			disconnect.OnPlayerLeaving(playerId, record, GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId));

		m_mRecords.Remove(playerId);
		m_mLastTick.Remove(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Appelé par la logique de spawn quand le personnage du joueur est perdu (mort, suppression).
	//! La position sauvegardée n'a plus de sens : le prochain spawn se fera à la base.
	void OnPlayerEntityLost(int playerId)
	{
		SRP_PlayerRecord record;
		if (!m_mRecords.Find(playerId, record))
			return;

		if (record.m_bHasPosition)
		{
			record.m_bHasPosition = false;
			record.Save();
			SRP_JournalComponent.Log("MORT", string.Format("%1 [%2] : personnage perdu, position effacée", record.m_sName, record.m_sIdentity));
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Garantit qu'un joueur connecté a sa fiche chargée (utilisé par le spawn et les commandes,
	//! qui peuvent arriver avant les callbacks de connexion)
	void EnsureRegistered(int playerId)
	{
		if (!Replication.IsServer() || m_mRecords.Contains(playerId))
			return;

		TryRegister(playerId, IDENTITY_MAX_ATTEMPTS);
	}

	//------------------------------------------------------------------------------------------------
	//! Tente d'enregistrer le joueur. L'identité Bohemia peut ne pas être disponible dans les
	//! premières secondes : on réessaie, puis on se rabat sur "local_<pseudo>".
	protected void TryRegister(int playerId, int attempt)
	{
		if (m_mRecords.Contains(playerId))
			return;

		string name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (name.IsEmpty())
			name = "Joueur" + playerId.ToString();

		string identity = GetBackendIdentity(playerId);

		if (identity.IsEmpty() && attempt < IDENTITY_MAX_ATTEMPTS)
		{
			GetGame().GetCallqueue().CallLater(TryRegister, 2000, false, playerId, attempt + 1);
			return;
		}

		if (identity.IsEmpty())
		{
			identity = "local_" + SRP_Utils.SafeFileName(name);
			if (RplSession.Mode() == RplMode.Dedicated)
				SRP_JournalComponent.Log("ERREUR", string.Format("Identité Bohemia vide pour %1 sur serveur dédié : vérifier la config backend du serveur (publicAddress)", name));
		}

		RegisterPlayer(playerId, name, identity);
	}

	//------------------------------------------------------------------------------------------------
	protected string GetBackendIdentity(int playerId)
	{
		BackendApi backend = GetGame().GetBackendApi();
		if (!backend)
			return "";

		return backend.GetPlayerIdentityId(playerId);
	}

	//------------------------------------------------------------------------------------------------
	protected void RegisterPlayer(int playerId, string name, string identity)
	{
		SRP_PlayerRecord record = SRP_PlayerRecord.Load(identity);
		bool nouveau = (record == null);

		if (nouveau)
		{
			record = new SRP_PlayerRecord();
			record.m_sIdentity = identity;
			record.m_iGrade = SRP_EGrade.RECRUE;
			record.m_sFirstSeen = SRP_Time.Now();
		}

		record.m_sName = name;
		record.m_sLastSeen = SRP_Time.Now();
		record.m_iConnections = record.m_iConnections + 1;

		if (IsStaffIdentity(identity))
			record.m_bStaff = true;

		// Banni : expulsé, journalisé, pas de fiche en mémoire
		SRP_Ban ban = SRP_Bans.Active(identity);
		if (ban && !record.m_bStaff)
		{
			SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] banni (%3) a tenté de se connecter : expulsé", record.m_sName, identity, ban.m_sReason));
			GetGame().GetPlayerManager().KickPlayer(playerId, 0, 0);
			return;
		}

		record.Save();

		m_mRecords.Set(playerId, record);
		m_mLastTick.Set(playerId, System.GetTickCount());

		// De retour : son corps laissé sur place disparaît
		SRP_DisconnectComponent disconnect = SRP_DisconnectComponent.GetInstance();
		if (disconnect)
			disconnect.OnPlayerBack(identity);

		string statut = "connu";
		if (nouveau)
			statut = "NOUVEAU";

		SRP_JournalComponent.Log("CONNEXION", string.Format("%1 — %2", statut, record.ToSummary()));
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsStaffIdentity(string identity)
	{
		if (!m_aStaffIdentities)
			return false;

		foreach (string staffId : m_aStaffIdentities)
		{
			if (staffId == identity)
				return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	// Sauvegarde
	//------------------------------------------------------------------------------------------------
	//! Met à jour temps de jeu et position d'un joueur connecté
	protected void UpdateLiveData(int playerId, SRP_PlayerRecord record)
	{
		int now = System.GetTickCount();
		int last;
		if (m_mLastTick.Find(playerId, last))
		{
			int elapsedMs = now - last;
			if (elapsedMs > 0)
				record.m_iPlaytimeSeconds = record.m_iPlaytimeSeconds + (elapsedMs / 1000);
		}
		m_mLastTick.Set(playerId, now);

		CapturePosition(playerId, record);

		record.m_sLastSeen = SRP_Time.Now();
	}

	//------------------------------------------------------------------------------------------------
	//! Mémorise la position du personnage s'il existe et s'il est vivant. Un personnage mort ne
	//! compte pas : le prochain spawn se fera à la base. Si le personnage a déjà été supprimé
	//! (arrêt du jeu), la dernière position mémorisée reste valable.
	protected void CapturePosition(int playerId, SRP_PlayerRecord record)
	{
		IEntity controlled = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!controlled)
			return;

		CharacterControllerComponent charCtrl = CharacterControllerComponent.Cast(controlled.FindComponent(CharacterControllerComponent));
		if (charCtrl && charCtrl.IsDead())
			return;

		vector angles = controlled.GetYawPitchRoll();
		record.SetPosition(controlled.GetOrigin(), angles[0]);

		CaptureInventory(controlled, record);
	}

	//------------------------------------------------------------------------------------------------
	//! Inventaire complet du personnage (tenue, poches, chargeurs, accessoires) sérialisé par le
	//! système de loadout vanilla, emplacement par emplacement : la restauration est exacte
	protected void CaptureInventory(IEntity character, SRP_PlayerRecord record)
	{
		if (!character || !character.FindComponent(InventoryStorageManagerComponent))
			return;

		JsonSaveContext context = new JsonSaveContext();
		if (!SCR_PlayerArsenalLoadout.ReadLoadoutString(character, context))
		{
			Print("[SRP] Capture d'inventaire impossible pour " + record.m_sName, LogLevel.WARNING);
			return;
		}

		record.m_sInventory = context.SaveToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Toutes les 5 s : positions en mémoire pour tous les joueurs connectés (rien n'est écrit)
	protected void RefreshPositions()
	{
		if (!Replication.IsServer())
			return;

		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
			CapturePosition(playerId, record);
		// Radios : on vérifie qu'elles suivent bien leur porteur (voir SRP_RadioWatch)
		SRP_RadioWatch.Tick();

		// Demandes de mission : issue des opérations accordées, demandes devenues sans objet
		SRP_MissionRequests.Tick();

		// Droits d'interaction : n'afficher chez chaque joueur que les actions qu'il peut faire (SRP_Rights)
		SRP_Rights.Tick();
	}

	//------------------------------------------------------------------------------------------------
	//! Sauvegarde tous les joueurs connectés (sauvegarde auto, arrêt, commande /sauver)
	void SaveAll()
	{
		if (!Replication.IsServer())
			return;

		int count = 0;
		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
		{
			UpdateLiveData(playerId, record);
			if (record.Save())
				count++;
		}

		if (count > 0)
			SRP_JournalComponent.Log("SAUVEGARDE", string.Format("%1 fiche(s) sauvegardée(s)", count));
	}

	//------------------------------------------------------------------------------------------------
	// Accès aux fiches
	//------------------------------------------------------------------------------------------------
	SRP_PlayerRecord GetRecord(int playerId)
	{
		SRP_PlayerRecord record;
		m_mRecords.Find(playerId, record);
		return record;
	}

	//------------------------------------------------------------------------------------------------
	//! Retrouve un joueur connecté par son pseudo (sans tenir compte de la casse)
	int FindPlayerIdByName(string name)
	{
		string wanted = name;
		wanted.ToLower();

		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
		{
			string current = record.m_sName;
			current.ToLower();
			if (current == wanted)
				return playerId;
		}
		return -1;
	}

	//------------------------------------------------------------------------------------------------
	int GetConnectedCount()
	{
		return m_mRecords.Count();
	}

	//------------------------------------------------------------------------------------------------
	//! Les fiches des joueurs connectés (playerId -> fiche), pour le terminal du PC
	void GetConnectedRecords(out array<int> ids, out array<SRP_PlayerRecord> records)
	{
		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
		{
			ids.Insert(playerId);
			records.Insert(record);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Liste lisible des joueurs connectés, une ligne par joueur
	string GetConnectedList()
	{
		string result = "";
		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
		{
			if (!result.IsEmpty())
				result += "\n";
			result += string.Format("%1 — %2", record.m_sName, SRP_Grades.GetName(record.m_iGrade));
			if (record.m_bStaff)
				result += " [Staff]";
		}
		if (result.IsEmpty())
			return "Personne";
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! Pseudos des joueurs connectés qui ont une certification, hors un joueur donné. Retourne le nombre.
	int GetConnectedWithCertif(int certif, int excludePlayerId, out string names)
	{
		names = "";
		int count = 0;
		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
		{
			if (playerId == excludePlayerId)
				continue;
			if (!SRP_Certifs.Has(record.m_iCertifs, certif))
				continue;

			if (!names.IsEmpty())
				names += ", ";
			names += record.m_sName;
			count++;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Alerte radio : message envoyé à tous les officiers et membres du Staff connectés
	void NotifyOfficiers(string text)
	{
		if (!Replication.IsServer())
			return;

		foreach (int playerId, SRP_PlayerRecord record : m_mRecords)
		{
			if (record.m_bStaff || SRP_Grades.IsOfficier(record.m_iGrade))
				SRP_Utils.NotifyPlayer(playerId, "[Alerte] " + text);
		}
	}

	//------------------------------------------------------------------------------------------------
	// Permissions
	//------------------------------------------------------------------------------------------------
	bool IsStaff(int playerId)
	{
		SRP_PlayerRecord record = GetRecord(playerId);
		return record && record.m_bStaff;
	}

	//------------------------------------------------------------------------------------------------
	//! Lieutenant et plus, ou Staff
	bool IsOfficier(int playerId)
	{
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record)
			return false;
		return record.m_bStaff || SRP_Grades.IsOfficier(record.m_iGrade);
	}

	//------------------------------------------------------------------------------------------------
	// Arbre des certifications
	//------------------------------------------------------------------------------------------------
	protected ref SRP_CertifTree m_CertifTree;

	protected void LoadCertifTree()
	{
		m_CertifTree = null;
		if (m_sCertifTreePath.IsEmpty())
		{
			SRP_JournalComponent.Log("SYSTEME", "Arbre des certifications : règles par défaut du code");
			return;
		}
		Resource res = BaseContainerTools.LoadContainer(m_sCertifTreePath);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Arbre des certifications illisible : " + m_sCertifTreePath + " (règles par défaut)", LogLevel.ERROR);
			return;
		}
		m_CertifTree = SRP_CertifTree.Cast(BaseContainerTools.CreateInstanceFromContainer(res.GetResource().ToBaseContainer()));
		int count = 0;
		if (m_CertifTree && m_CertifTree.m_aRules)
			count = m_CertifTree.m_aRules.Count();
		SRP_JournalComponent.Log("SYSTEME", string.Format("Arbre des certifications chargé : %1 règle(s), le reste par défaut", count));
	}

	//------------------------------------------------------------------------------------------------
	int GetPrerequisites(int certif)
	{
		return SRP_CertifTree.PrerequisitesOf(m_CertifTree, certif);
	}

	//------------------------------------------------------------------------------------------------
	int GetMinGrade(int certif)
	{
		return SRP_CertifTree.MinGradeOf(m_CertifTree, certif);
	}

	//------------------------------------------------------------------------------------------------
	//! Peut-on accorder cette certification à ce joueur ? Sinon, la raison.
	bool CanGrant(SRP_PlayerRecord record, int certif, out string reason)
	{
		reason = "";
		if (!record)
		{
			reason = "fiche introuvable";
			return false;
		}
		if (certif == SRP_ECertif.INSTRUCTEUR)
		{
			reason = "plus de certification Instructeur : on habilite par certification (/instructeur)";
			return false;
		}
		if (SRP_Certifs.Has(record.m_iCertifs, certif))
		{
			reason = "déjà détenue";
			return false;
		}

		int missing = GetPrerequisites(certif) & ~record.m_iCertifs;
		if (missing != 0)
		{
			reason = string.Format("%1 demande d'abord : %2", SRP_Certifs.GetName(certif), SRP_Certifs.MaskToString(missing));
			return false;
		}

		int minGrade = GetMinGrade(certif);
		if (record.m_iGrade < minGrade)
		{
			reason = string.Format("%1 demande le grade %2 (actuel : %3)", SRP_Certifs.GetName(certif), SRP_Grades.GetName(minGrade), SRP_Grades.GetName(record.m_iGrade));
			return false;
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Les certifications qu'un joueur peut viser maintenant (prérequis et grade remplis)
	string GetReachableText(SRP_PlayerRecord record)
	{
		string reason;
		string text = "";
		int certif = 1;
		while (certif <= SRP_Certifs.LAST)
		{
			if (certif != SRP_ECertif.INSTRUCTEUR && CanGrant(record, certif, reason))
			{
				if (!text.IsEmpty())
					text += ", ";
				text += SRP_Certifs.GetName(certif);
			}
			certif = certif * 2;
		}
		if (text.IsEmpty())
			return "aucune (prérequis ou grade manquants)";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	//! Habilité à valider cette certification : Staff, ou habilitation explicite pour cette certification
	//! (/instructeur, menu Gestion du soldat). Un instructeur l'est par certification : instructeur AT,
	//! instructeur TP… Il doit détenir lui-même la certification qu'il délivre.
	bool IsInstructeur(int playerId, int certif)
	{
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record)
			return false;
		if (record.m_bStaff)
			return true;
		if (!SRP_Certifs.Has(record.m_iInstructeurCertifs, certif))
			return false;
		return SRP_Certifs.Has(record.m_iCertifs, certif);
	}

	//------------------------------------------------------------------------------------------------
	//! Menu « Gestion du soldat » : construit selon les droits de l'utilisateur et envoyé à son écran
	void SendSoldierMenu(int userId, int targetId)
	{
		EnsureRegistered(userId);
		EnsureRegistered(targetId);
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(userId));
		if (pc)
			pc.SRP_OpenSoldier(SRP_SoldierManagement.BuildMenu(this, userId, targetId));
	}

	//------------------------------------------------------------------------------------------------
	//! Une ligne du menu choisie (commande « soldat ») : exécute, prévient, rouvre le menu à jour
	string ExecuteSoldier(int userId, string key)
	{
		int targetId;
		string message = SRP_SoldierManagement.Execute(this, userId, key, targetId);
		if (targetId > 0)
			GetGame().GetCallqueue().CallLater(SendSoldierMenu, 300, false, userId, targetId);
		return message;
	}

	//------------------------------------------------------------------------------------------------
	//! Nombre de spécialités (certifications hors FGI et Instructeur)
	static int CountSpecialties(int certifs)
	{
		int count = 0;
		int certif = 1;
		while (certif <= SRP_Certifs.LAST)
		{
			if (certif != SRP_ECertif.FGI && certif != SRP_ECertif.INSTRUCTEUR && (certifs & certif) != 0)
				count++;
			certif = certif * 2;
		}
		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Le joueur remplit-il les conditions d'accès à ce grade ? Sinon, ce qui manque.
	bool MeetsGrade(SRP_PlayerRecord record, int grade, out string missing)
	{
		missing = "";
		if (!record)
			return false;

		int required = SRP_CertifTree.RequiredCertifsOf(m_CertifTree, grade) & ~record.m_iCertifs;
		if (required != 0)
			missing += "certif " + SRP_Certifs.MaskToString(required);

		int specialties = CountSpecialties(record.m_iCertifs);
		int minSpecialties = SRP_CertifTree.MinSpecialtiesOf(m_CertifTree, grade);
		if (specialties < minSpecialties)
			missing += string.Format("%1%2 spécialité(s) (a %3)", Sep(missing), minSpecialties - specialties, specialties);

		int hours = record.m_iPlaytimeSeconds / 3600;
		int minHours = SRP_CertifTree.MinHoursOf(m_CertifTree, grade);
		if (hours < minHours)
			missing += string.Format("%1%2 h de jeu (a %3 h)", Sep(missing), minHours - hours, hours);

		int minMissions = SRP_CertifTree.MinMissionsOf(m_CertifTree, grade);
		if (record.m_iMissions < minMissions)
			missing += string.Format("%1%2 mission(s) (a %3)", Sep(missing), minMissions - record.m_iMissions, record.m_iMissions);

		return missing.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	protected static string Sep(string current)
	{
		if (current.IsEmpty())
			return "";
		return ", ";
	}

	//------------------------------------------------------------------------------------------------
	//! Le prochain grade et ce qu'il manque, pour /moi
	string GetNextGradeText(SRP_PlayerRecord record)
	{
		if (!record || record.m_iGrade >= SRP_EGrade.ADJUDANT)
			return "sur nomination";
		int next = record.m_iGrade + 1;
		string missing;
		if (MeetsGrade(record, next, missing))
			return SRP_Grades.GetName(next) + " : conditions remplies, sur décision d'un officier";
		return string.Format("%1 : il manque %2", SRP_Grades.GetName(next), missing);
	}

	//------------------------------------------------------------------------------------------------
	//! États de service : un officier crédite (ou décrédite) une mission après le débriefing
	string AdjustMissions(int targetId, int delta, string author)
	{
		SRP_PlayerRecord record = GetRecord(targetId);
		if (!record)
			return "Fiche introuvable";
		record.m_iMissions = Math.Max(0, record.m_iMissions + delta);
		record.Save();

		string verb = "créditée";
		if (delta < 0)
			verb = "retirée";
		SRP_JournalComponent.Log("SERVICE", string.Format("%1 [%2] : mission %3 par %4 -> %5 mission(s)", record.m_sName, record.m_sIdentity, verb, author, record.m_iMissions));
		SRP_Utils.NotifyPlayer(targetId, string.Format("Mission %1 dans tes états de service par %2 : %3 mission(s) réussie(s)", verb, author, record.m_iMissions));

		string missing;
		if (delta > 0 && record.m_iGrade < SRP_EGrade.ADJUDANT && MeetsGrade(record, record.m_iGrade + 1, missing))
			NotifyOfficiers(string.Format("%1 remplit les conditions pour %2", record.m_sName, SRP_Grades.GetName(record.m_iGrade + 1)));

		return string.Format("%1 : %2 mission(s) réussie(s)", record.m_sName, record.m_iMissions);
	}

	//------------------------------------------------------------------------------------------------
	//! Une mission réussie pour ces joueurs (comptage automatique, désactivé par défaut)
	void AddMissionSuccess(array<int> playerIds, string missionId)
	{
		foreach (int playerId : playerIds)
		{
			SRP_PlayerRecord record = GetRecord(playerId);
			if (!record)
				continue;
			record.m_iMissions++;
			record.Save();
			SRP_Utils.NotifyPlayer(playerId, string.Format("Mission %1 comptée dans tes états de service (%2 réussie(s))", missionId, record.m_iMissions));
			string missing;
			if (record.m_iGrade < SRP_EGrade.ADJUDANT && MeetsGrade(record, record.m_iGrade + 1, missing))
				NotifyOfficiers(string.Format("%1 remplit les conditions pour %2", record.m_sName, SRP_Grades.GetName(record.m_iGrade + 1)));
		}
	}

	//------------------------------------------------------------------------------------------------
	// API grades / certifications / staff
	// "auteur" = qui fait l'action, pour le journal
	//------------------------------------------------------------------------------------------------
	//! Retourne false avec la raison si la promotion est refusée (FGI manquante) ; force = Staff
	bool SetGrade(int playerId, int newGrade, string auteur, bool force, out string reason)
	{
		reason = "";
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record || !SRP_Grades.IsValide(newGrade))
		{
			reason = "fiche introuvable ou grade invalide";
			return false;
		}

		if (m_bFgiRequiredForGrade && !force && newGrade > SRP_EGrade.RECRUE && !SRP_Certifs.Has(record.m_iCertifs, SRP_ECertif.FGI))
		{
			reason = record.m_sName + " n'a pas la FGI : il reste Recrue tant qu'elle n'est pas validée (le Staff peut forcer)";
			return false;
		}

		// Promotion : les conditions du grade (spécialités, heures, missions, certifs) ; rétrogradation et nomination officier libres
		if (!force && newGrade > record.m_iGrade && newGrade <= SRP_EGrade.ADJUDANT)
		{
			string missing;
			if (!MeetsGrade(record, newGrade, missing))
			{
				reason = string.Format("%1 pour %2 : il manque %3", record.m_sName, SRP_Grades.GetName(newGrade), missing);
				return false;
			}
		}

		int ancien = record.m_iGrade;
		record.m_iGrade = newGrade;
		record.Save();

		SRP_JournalComponent.Log("GRADE", string.Format("%1 [%2] : %3 -> %4 (par %5)", record.m_sName, record.m_sIdentity, SRP_Grades.GetName(ancien), SRP_Grades.GetName(newGrade), auteur));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Accorde ou retire. Accorder vérifie l'arbre (prérequis, grade) sauf force (Staff) ; retirer emporte
	//! les certifications qui en dépendent. Retourne false avec la raison en cas de refus.
	bool SetCertif(int playerId, int certif, bool accorder, string auteur, bool force, out string reason)
	{
		reason = "";
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record || certif <= 0)
		{
			reason = "fiche introuvable";
			return false;
		}

		if (accorder)
		{
			if (!force && !CanGrant(record, certif, reason))
				return false;

			record.m_iCertifs = record.m_iCertifs | certif;

			// La FGI validée fait passer une Recrue au grade de Soldat
			if (certif == SRP_ECertif.FGI && record.m_iGrade == SRP_EGrade.RECRUE)
			{
				record.m_iGrade = SRP_EGrade.SOLDAT;
				SRP_JournalComponent.Log("GRADE", string.Format("%1 [%2] : Recrue -> Soldat (FGI validée)", record.m_sName, record.m_sIdentity));
			}

			record.Save();
			string forced = "";
			if (force)
				forced = ", forcée";
			SRP_JournalComponent.Log("CERTIF", string.Format("%1 [%2] : %3 accordée (par %4%5)", record.m_sName, record.m_sIdentity, SRP_Certifs.GetName(certif), auteur, forced));
			SRP_Utils.NotifyPlayer(playerId, string.Format("Certification %1 accordée par %2", SRP_Certifs.GetFullName(certif), auteur));
			return true;
		}

		// Retrait, en cascade
		int cascade = SRP_Certifs.Dependents(certif, m_CertifTree) & record.m_iCertifs;
		record.m_iCertifs = record.m_iCertifs & ~(certif | cascade);
		record.Save();

		string extra = "";
		if (cascade != 0)
			extra = " et par conséquent " + SRP_Certifs.MaskToString(cascade);
		SRP_JournalComponent.Log("CERTIF", string.Format("%1 [%2] : %3 retirée%4 (par %5)", record.m_sName, record.m_sIdentity, SRP_Certifs.GetName(certif), extra, auteur));
		SRP_Utils.NotifyPlayer(playerId, string.Format("Certification %1 retirée par %2%3", SRP_Certifs.GetFullName(certif), auteur, extra));
		reason = extra;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool SetInstructeur(int playerId, int certif, bool habiliter, string auteur, out string reason)
	{
		reason = "";
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record || certif <= 0)
		{
			reason = "fiche introuvable";
			return false;
		}

		if (certif == SRP_ECertif.INSTRUCTEUR)
		{
			reason = "l'habilitation se fait par certification : menu Gestion du soldat, en regardant le joueur";
			return false;
		}

		if (habiliter)
		{
			if (!SRP_Certifs.Has(record.m_iCertifs, certif))
			{
				reason = string.Format("%1 ne détient pas %2 : on ne délivre que ce qu'on a", record.m_sName, SRP_Certifs.GetName(certif));
				return false;
			}
		}

		if (habiliter)
			record.m_iInstructeurCertifs = record.m_iInstructeurCertifs | certif;
		else
			record.m_iInstructeurCertifs = record.m_iInstructeurCertifs & ~certif;

		record.Save();

		string action = "retirée";
		if (habiliter)
			action = "accordée";

		SRP_JournalComponent.Log("CERTIF", string.Format("%1 [%2] : habilitation instructeur %3 %4 (par %5)", record.m_sName, record.m_sIdentity, SRP_Certifs.GetName(certif), action, auteur));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool SetStaff(int playerId, bool staff, string auteur)
	{
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record)
			return false;

		record.m_bStaff = staff;
		record.Save();

		SRP_JournalComponent.Log("STAFF", string.Format("%1 [%2] : staff = %3 (par %4)", record.m_sName, record.m_sIdentity, SRP_Utils.OuiNon(staff), auteur));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Avertissement d'un officier ou du Staff : compté sur la fiche, journalisé, annoncé au joueur
	string Warn(int targetId, string reason, string author)
	{
		// Les avertissements donnés en jeu partent au journal des sanctions Discord (ceux du bot y sont déjà)
		if (!author.Contains("(Discord)"))
			SRP_BridgeComponent.PushEvent("sanction", string.Format("Avertissement à %1 par %2 : %3", GetGame().GetPlayerManager().GetPlayerName(targetId), author, reason));

		SRP_PlayerRecord record = GetRecord(targetId);
		if (!record)
			return "Fiche introuvable";
		record.m_iInfractions = record.m_iInfractions + 1;
		record.Save();
		SRP_JournalComponent.Log("AVERTISSEMENT", string.Format("%1 [%2] par %3 : %4 (total %5)", record.m_sName, record.m_sIdentity, author, reason, record.m_iInfractions));
		SRP_Utils.NotifyPlayer(targetId, string.Format("AVERTISSEMENT de %1 : %2 (%3 au total)", author, reason, record.m_iInfractions));
		return string.Format("%1 averti (%2 avertissement(s))", record.m_sName, record.m_iInfractions);
	}

	//------------------------------------------------------------------------------------------------
	//! Enregistre une infraction (verrous souples) : journal + compteur sur la fiche
	void LogInfraction(int playerId, string description)
	{
		SRP_PlayerRecord record = GetRecord(playerId);
		if (!record)
			return;

		record.m_iInfractions = record.m_iInfractions + 1;
		record.Save();

		SRP_JournalComponent.Log("INFRACTION", string.Format("%1 [%2] : %3", record.m_sName, record.m_sIdentity, description));
	}
}
