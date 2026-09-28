//------------------------------------------------------------------------------------------------
// SimpleRP — La charte
// Texte dans $profile:SimpleRP/charte.txt (une règle par ligne, « version: N » en tête ; créé avec un
// texte de départ s'il manque). À l'arrivée du personnage, si la fiche ne porte pas l'acceptation de la
// version courante, l'écran s'ouvre (SRP_CharteMenu, même dialogue que le PC) : « J'accepte » écrit la
// version et la date dans la fiche, « Plus tard » ferme. Tant que ce n'est pas accepté, le joueur ne
// s'éloigne pas du spawn (ramené au-delà du rayon) et reste Recrue.
// /charte : relire ; /charte <pseudo> : état (officiers) ; /charte reset : tout le monde à zéro (Staff).
// Phase 5 — étape E
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Charte SimpleRP : texte, acceptation, périmètre du spawn")]
class SRP_CharteComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_CharteComponent : SCR_BaseGameModeComponent
{
	[Attribute("50", UIWidgets.EditBox, "Rayon autour du spawn tant que la charte n'est pas acceptée, en mètres", category: "SimpleRP")]
	protected float m_fRadius;

	[Attribute("SRP_SpawnBase", UIWidgets.EditBox, "Marqueur du spawn de la base", category: "SimpleRP")]
	protected string m_sSpawnMarkerName;

	[Attribute("1", UIWidgets.CheckBox, "Ouvrir la charte automatiquement à l'arrivée du personnage tant qu'elle n'est pas acceptée", category: "SimpleRP")]
	protected bool m_bAutoOpen;

	static const string PATH = "$profile:SimpleRP/charte.txt";

	protected static SRP_CharteComponent s_Instance;

	protected int m_iVersion = 1;
	protected ref array<string> m_aLines = {};
	protected ref map<int, int> m_mCharteReopen = new map<int, int>();	// joueur -> dernière réouverture de l'écran (ms)
	protected static const int REOPEN_MS = 60000;							// au plus une réouverture par minute

	//------------------------------------------------------------------------------------------------
	static SRP_CharteComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		s_Instance = this;
		SRP_Paths.EnsureDirectories();
		Load();
		GetGame().GetCallqueue().CallLater(Tick, 5000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
			GetGame().GetCallqueue().Remove(Tick);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Texte
	//------------------------------------------------------------------------------------------------
	protected void Load()
	{
		m_aLines.Clear();
		m_iVersion = 1;

		if (!FileIO.FileExists(PATH))
			WriteDefault();

		FileHandle file = FileIO.OpenFile(PATH, FileMode.READ);
		if (!file)
		{
			Print("[SRP] Charte illisible : " + PATH, LogLevel.ERROR);
			return;
		}

		string line;
		while (file.ReadLine(line) >= 0)
		{
			string trimmed = line.Trim();
			if (trimmed.IsEmpty())
				continue;
			string lower = trimmed;
			lower.ToLower();
			if (lower.StartsWith("version:"))
			{
				string value = trimmed.Substring(8, trimmed.Length() - 8);
				value = value.Trim();
				if (SRP_Utils.IsNumeric(value))
					m_iVersion = value.ToInt();
				continue;
			}
			m_aLines.Insert(trimmed);
		}
		file.Close();

		SRP_JournalComponent.Log("CHARTE", string.Format("Charte chargée : version %1, %2 ligne(s)", m_iVersion, m_aLines.Count()));
	}

	//------------------------------------------------------------------------------------------------
	protected void WriteDefault()
	{
		FileHandle file = FileIO.OpenFile(PATH, FileMode.WRITE);
		if (!file)
			return;
		file.WriteLine("version: 1");
		file.WriteLine("Bienvenue. Ce serveur est un milsim RP de l'armée française : on y joue un soldat, pas un joueur.");
		file.WriteLine("1. Respect entre joueurs, en jeu comme sur le Discord. Pas d'insulte, pas de harcèlement, pas de politique.");
		file.WriteLine("2. Le rôle avant tout : on reste dans son personnage, on parle comme sur un vrai réseau radio, on suit la chaîne de commandement.");
		file.WriteLine("3. Les grades et les certifications sont des règles de jeu, pas des murs : un Recrue ne prend pas un blindé, un non-certifié ne conduit pas.");
		file.WriteLine("4. Pas de tir sur les civils, pas de pillage, pas de destruction gratuite. Un crime de guerre est sanctionné.");
		file.WriteLine("5. Le matériel est commun : on range les paquets, on ramène les véhicules, on signale la casse.");
		file.WriteLine("6. Pas de triche, pas d'exploitation de bug, pas de contournement des règles du mod. On signale au Staff.");
		file.WriteLine("7. Le Staff a le dernier mot en jeu ; les désaccords se règlent après, sur le Discord.");
		file.WriteLine("En acceptant, vous vous engagez à respecter ces règles. Bon jeu.");
		file.Close();
		Print("[SRP] Charte créée avec un texte de départ : " + PATH, LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	int GetVersion()
	{
		return m_iVersion;
	}

	//------------------------------------------------------------------------------------------------
	//! Contenu de l'écran (format des menus à liste)
	string BuildContent(SRP_PlayerRecord record)
	{
		string text = string.Format("M|Charte du serveur, version %1. Lis-la, puis J'accepte.", m_iVersion);
		if (record && HasAccepted(record))
			text = string.Format("M|Charte du serveur, version %1 — acceptée le %2.", m_iVersion, record.m_sCharteDate);
		foreach (string line : m_aLines)
			text += "\nL|" + line;
		if (m_aLines.IsEmpty())
			text += "\nL|(charte vide : écris-la dans " + PATH + ")";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Acceptation
	//------------------------------------------------------------------------------------------------
	bool HasAccepted(SRP_PlayerRecord record)
	{
		return record && record.m_bCharteAcceptee && record.m_iCharteVersion >= m_iVersion;
	}

	//------------------------------------------------------------------------------------------------
	string Accept(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return "Gestionnaire de joueurs absent";
		players.EnsureRegistered(playerId);
		SRP_PlayerRecord record = players.GetRecord(playerId);
		if (!record)
			return "Fiche introuvable";
		if (HasAccepted(record))
			return "Charte déjà acceptée le " + record.m_sCharteDate;

		record.m_bCharteAcceptee = true;
		record.m_iCharteVersion = m_iVersion;
		record.m_sCharteDate = SRP_Time.Now();
		record.Save();
		m_mCharteReopen.Remove(playerId);

		SRP_JournalComponent.Log("CHARTE", string.Format("%1 [%2] accepte la charte v%3", record.m_sName, record.m_sIdentity, m_iVersion));
		return string.Format("Charte acceptée. Bienvenue, %1. Passe voir un instructeur pour ta FGI.", record.m_sName);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : tout le monde doit réaccepter (nouvelle version écrite dans le fichier)
	string ResetAll(string author)
	{
		m_iVersion++;
		// Réécrit le fichier avec la nouvelle version en tête
		FileHandle file = FileIO.OpenFile(PATH, FileMode.WRITE);
		if (file)
		{
			file.WriteLine("version: " + m_iVersion);
			foreach (string line : m_aLines)
				file.WriteLine(line);
			file.Close();
		}
		m_mCharteReopen.Clear();
		SRP_JournalComponent.Log("CHARTE", string.Format("Charte passée en version %1 par %2 : à réaccepter par tous", m_iVersion, author));
		SRP_Utils.NotifyAll("La charte du serveur a changé : à relire et accepter (/charte).");
		OpenForAllPending();
		return string.Format("Charte en version %1 : chacun devra la réaccepter", m_iVersion);
	}

	//------------------------------------------------------------------------------------------------
	//! Recharge le fichier (après une modification à la main) sans changer la version
	string Reload(string author)
	{
		Load();
		SRP_JournalComponent.Log("CHARTE", "Charte rechargée par " + author);
		return string.Format("Charte rechargée : version %1, %2 ligne(s)", m_iVersion, m_aLines.Count());
	}

	//------------------------------------------------------------------------------------------------
	string StatusOf(SRP_PlayerRecord record)
	{
		if (!record)
			return "fiche introuvable";
		if (HasAccepted(record))
			return string.Format("charte v%1 acceptée le %2", record.m_iCharteVersion, record.m_sCharteDate);
		if (record.m_bCharteAcceptee)
			return string.Format("charte v%1 acceptée le %2, mais la version courante est la %3 : à réaccepter", record.m_iCharteVersion, record.m_sCharteDate, m_iVersion);
		return "charte non acceptée";
	}

	//------------------------------------------------------------------------------------------------
	// Écran
	//------------------------------------------------------------------------------------------------
	void Open(int playerId)
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		SRP_PlayerRecord record;
		if (players)
			record = players.GetRecord(playerId);
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (pc)
			pc.SRP_OpenCharte(BuildContent(record));
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenForAllPending()
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;
		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);
		foreach (int index, int playerId : ids)
		{
			if (!HasAccepted(records[index]))
			{
				m_mCharteReopen.Set(playerId, System.GetTickCount());	// le Tick ne rouvre pas un second écran tout de suite
				Open(playerId);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Appelé par la logique de spawn quand le personnage est prêt
	void OnCharacterReady(int playerId)
	{
		if (!m_bAutoOpen)
			return;
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;
		players.EnsureRegistered(playerId);
		if (HasAccepted(players.GetRecord(playerId)))
			return;
		GetGame().GetCallqueue().CallLater(Open, 3000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	// Périmètre du spawn
	//------------------------------------------------------------------------------------------------
	protected void Tick()
	{
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (!players)
			return;
		IEntity marker = GetGame().GetWorld().FindEntityByName(m_sSpawnMarkerName);
		if (!marker)
			return;

		array<int> ids = {};
		array<SRP_PlayerRecord> records = {};
		players.GetConnectedRecords(ids, records);
		foreach (int index, int playerId : ids)
		{
			SRP_PlayerRecord record = records[index];
			if (HasAccepted(record) || record.m_bStaff)
				continue;
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!character || SRP_Utils.IsDead(character))
				continue;
			if (vector.Distance(character.GetOrigin(), marker.GetOrigin()) <= m_fRadius)
				continue;

			// Ramené au spawn, prévenu, et l'écran rouvert la première fois
			vector position = marker.GetOrigin() + Vector(2, 0, 2);
			position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]) + 0.2;
			SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
			if (pc)
			{
				if (character.GetRootParent() != character)
				{
					SCR_Global.TeleportPlayer(playerId, position);
					pc.SRP_TeleportBroadcast(playerId, position);
				}
				else
					pc.SRP_TeleportOwner(position);
			}
			// L'écran se rouvre (au plus une fois par minute) : sur console, taper /charte n'est pas une solution
			int now = System.GetTickCount();
			if (!m_mCharteReopen.Contains(playerId) || now - m_mCharteReopen.Get(playerId) > REOPEN_MS)
			{
				m_mCharteReopen.Set(playerId, now);
				SRP_Utils.NotifyPlayer(playerId, "Tu dois d'abord accepter la charte du serveur : l'écran de la charte s'ouvre (J'accepte).");
				Open(playerId);
			}
			else
				SRP_Utils.NotifyPlayer(playerId, "Tu dois d'abord accepter la charte du serveur : l'écran se rouvrira d'ici une minute (J'accepte).");
		}
	}
}

//------------------------------------------------------------------------------------------------
//! L'écran : le dialogue du PC, sans onglets, avec la charte ligne par ligne
class SRP_CharteMenu : SRP_ListMenu
{
	override protected string GetMenuTitle() { return "Charte du serveur"; }
	override protected string GetConfirmLabel() { return "J'accepte"; }
	override protected string GetCommand() { return "charte"; }

	//------------------------------------------------------------------------------------------------
	static void Open(string content)
	{
		OpenPreset(ChimeraMenuPreset.SRP_CharteMenu, content);
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		SetCancelText("Plus tard");
	}

	//------------------------------------------------------------------------------------------------
	//! J'accepte : pas de ligne à choisir, on envoie directement
	override protected void OnConfirm()
	{
		SCR_PlayerController pc = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (pc)
			pc.SRP_SendMenuCommand("charte", "accepter");
		CloseAnimated();
	}
}
