//------------------------------------------------------------------------------------------------
// SimpleRP — Bannissements persistants
// $profile:SimpleRP/bannis.json : identité, pseudo, motif, auteur, date, fin (vide = définitif).
// Un banni est expulsé dès son enregistrement (SRP_PlayerManagerComponent.TryRegister). Staff :
// /ban <pseudo> [jours] [motif], /unban <pseudo ou identité>, /bans.
// Phase 5 — étape G
//------------------------------------------------------------------------------------------------

class SRP_Ban
{
	string m_sIdentity;
	string m_sName;
	string m_sReason;
	string m_sAuthor;
	string m_sDate;
	int m_iUntilDays;			// 0 = définitif
	int m_iCreatedDay;			// jour absolu (année*366 + jour de l'année approx.) pour l'expiration
}

class SRP_Bans
{
	static const string PATH = "$profile:SimpleRP/bannis.json";
	protected static ref array<ref SRP_Ban> s_aBans;

	//------------------------------------------------------------------------------------------------
	static array<ref SRP_Ban> All()
	{
		if (!s_aBans)
			Load();
		return s_aBans;
	}

	//------------------------------------------------------------------------------------------------
	protected static int Today()
	{
		int y, m, d;
		System.GetYearMonthDay(y, m, d);
		return y * 372 + m * 31 + d;	// ordre monotone suffisant pour compter des jours
	}

	//------------------------------------------------------------------------------------------------
	static SRP_Ban Find(string identity)
	{
		foreach (SRP_Ban ban : All())
		{
			if (ban.m_sIdentity == identity)
				return ban;
		}
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Le ban actif pour cette identité, ou null (expire les temporaires échus)
	static SRP_Ban Active(string identity)
	{
		SRP_Ban ban = Find(identity);
		if (!ban)
			return null;
		if (ban.m_iUntilDays > 0 && Today() - ban.m_iCreatedDay >= ban.m_iUntilDays)
		{
			s_aBans.RemoveItem(ban);
			Save();
			return null;
		}
		return ban;
	}

	//------------------------------------------------------------------------------------------------
	static void Add(string identity, string name, string reason, string author, int days)
	{
		SRP_Ban ban = Find(identity);
		if (!ban)
		{
			ban = new SRP_Ban();
			ban.m_sIdentity = identity;
			All().Insert(ban);
		}
		ban.m_sName = name;
		ban.m_sReason = reason;
		ban.m_sAuthor = author;
		ban.m_sDate = SRP_Time.Now();
		ban.m_iUntilDays = days;
		ban.m_iCreatedDay = Today();
		Save();
	}

	//------------------------------------------------------------------------------------------------
	static bool Remove(string identityOrName)
	{
		string lower = identityOrName;
		lower.ToLower();
		for (int i = All().Count() - 1; i >= 0; i--)
		{
			SRP_Ban ban = s_aBans[i];
			string name = ban.m_sName;
			name.ToLower();
			if (ban.m_sIdentity == identityOrName || name == lower)
			{
				s_aBans.Remove(i);
				Save();
				return true;
			}
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	static string ListText()
	{
		if (All().IsEmpty())
			return "Aucun banni";
		string text = "";
		foreach (SRP_Ban ban : s_aBans)
		{
			if (!text.IsEmpty())
				text += "\n";
			string duration = "définitif";
			if (ban.m_iUntilDays > 0)
				duration = string.Format("%1 jour(s)", ban.m_iUntilDays);
			text += string.Format("%1 — %2 (%3, par %4, %5)", ban.m_sName, ban.m_sReason, duration, ban.m_sAuthor, ban.m_sDate);
		}
		return text;
	}

	//------------------------------------------------------------------------------------------------
	static void Save()
	{
		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("count", All().Count());
		foreach (int index, SRP_Ban ban : s_aBans)
		{
			string prefix = "b" + index.ToString() + "_";
			ctx.WriteValue(prefix + "identity", ban.m_sIdentity);
			ctx.WriteValue(prefix + "name", ban.m_sName);
			ctx.WriteValue(prefix + "reason", ban.m_sReason);
			ctx.WriteValue(prefix + "author", ban.m_sAuthor);
			ctx.WriteValue(prefix + "date", ban.m_sDate);
			ctx.WriteValue(prefix + "days", ban.m_iUntilDays);
			ctx.WriteValue(prefix + "created", ban.m_iCreatedDay);
		}
		if (!ctx.SaveToFile(PATH))
			Print("[SRP] Échec de sauvegarde des bannis : " + PATH, LogLevel.ERROR);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Load()
	{
		s_aBans = {};
		if (!FileIO.FileExists(PATH))
			return;
		JsonLoadContext ctx = new JsonLoadContext();
		if (!ctx.LoadFromFile(PATH))
			return;
		int count = 0;
		ctx.ReadValue("count", count);
		for (int index = 0; index < count; index++)
		{
			string prefix = "b" + index.ToString() + "_";
			SRP_Ban ban = new SRP_Ban();
			ctx.ReadValue(prefix + "identity", ban.m_sIdentity);
			ctx.ReadValue(prefix + "name", ban.m_sName);
			ctx.ReadValue(prefix + "reason", ban.m_sReason);
			ctx.ReadValue(prefix + "author", ban.m_sAuthor);
			ctx.ReadValue(prefix + "date", ban.m_sDate);
			ctx.ReadValue(prefix + "days", ban.m_iUntilDays);
			ctx.ReadValue(prefix + "created", ban.m_iCreatedDay);
			if (!ban.m_sIdentity.IsEmpty())
				s_aBans.Insert(ban);
		}
	}
}
