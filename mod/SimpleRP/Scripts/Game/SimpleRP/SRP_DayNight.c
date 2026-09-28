//------------------------------------------------------------------------------------------------
// SimpleRP — Cycle jour/nuit
// Une journée en jeu dure N heures réelles (6 par défaut : le soleil fait un tour en une soirée), et
// l'heure est persistante : sauvegardée toutes les 5 minutes dans $profile:SimpleRP/temps.json et
// rétablie au démarrage, pour que la nuit ne recommence pas à midi à chaque relance.
// Staff : /heure <HH[:MM]> (déjà dans le menu Staff via l'admin) ; /heure vitesse <heures réelles par jour>.
// Phase 5 — étape F
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Cycle jour/nuit SimpleRP : durée réelle d'une journée, heure persistante")]
class SRP_DayNightComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_DayNightComponent : SCR_BaseGameModeComponent
{
	[Attribute("6", UIWidgets.EditBox, "Durée réelle d'une journée en jeu, en heures (24 = temps réel, 6 = quatre fois plus vite)", category: "SimpleRP")]
	protected float m_fRealHoursPerDay;

	[Attribute("1", UIWidgets.CheckBox, "Rétablir l'heure sauvegardée au démarrage (sinon, celle du monde)", category: "SimpleRP")]
	protected bool m_bRestoreTime;

	[Attribute("8", UIWidgets.EditBox, "Heure de départ au tout premier lancement (0-23)", category: "SimpleRP")]
	protected float m_fFirstLaunchHour;

	static const string PATH = "$profile:SimpleRP/temps.json";

	protected static SRP_DayNightComponent s_Instance;

	static SRP_DayNightComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	//! Le gestionnaire d'heure et de météo du monde
	static TimeAndWeatherManagerEntity Weather()
	{
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return null;
		return world.GetTimeAndWeatherManager();
	}

	//------------------------------------------------------------------------------------------------
	//! Fait-il nuit ? D'après le lever et le coucher du soleil du monde (marge de 20 min), sinon 21 h - 6 h.
	//! Même logique que SRP_EnemyComponent.IsNight (protégée là-bas) ; sert à l'éclairage et aux lanternes (1.0.26).
	static bool IsNight()
	{
		TimeAndWeatherManagerEntity manager = SRP_DayNightComponent.Weather();
		if (!manager)
			return false;
		float hour = manager.GetTimeOfTheDay();
		float sunrise;
		float sunset;
		if (manager.GetSunriseHour(sunrise) && manager.GetSunsetHour(sunset))
			return hour < sunrise - 0.3 || hour > sunset + 0.3;
		return hour >= 21 || hour <= 6;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!Replication.IsServer())
			return;
		s_Instance = this;
		SRP_Paths.EnsureDirectories();
		GetGame().GetCallqueue().CallLater(Start, 2000, false);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
		{
			GetGame().GetCallqueue().Remove(Start);
			GetGame().GetCallqueue().Remove(Save);
			if (m_bStarted)
				Save();
		}
		super.OnDelete(owner);
	}

	protected bool m_bStarted;

	//------------------------------------------------------------------------------------------------
	protected void Start()
	{
		TimeAndWeatherManagerEntity manager = SRP_DayNightComponent.Weather();
		if (!manager)
		{
			Print("[SRP] Jour/nuit : pas de TimeAndWeatherManager dans le monde", LogLevel.WARNING);
			return;
		}

		ApplySpeed(manager);

		if (m_bRestoreTime)
		{
			float hour = -1;
			if (FileIO.FileExists(PATH))
			{
				JsonLoadContext ctx = new JsonLoadContext();
				if (ctx.LoadFromFile(PATH))
					ctx.ReadValue("hour", hour);
			}
			else
				hour = m_fFirstLaunchHour;

			if (hour >= 0 && hour < 24)
				manager.SetTimeOfTheDay(hour, true);
		}

		m_bStarted = true;
		GetGame().GetCallqueue().CallLater(Save, 300000, true);
		SRP_JournalComponent.Log("SYSTEME", string.Format("Jour/nuit : une journée dure %1 h réelles, il est %2", m_fRealHoursPerDay, HourText(manager.GetTimeOfTheDay())));
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplySpeed(TimeAndWeatherManagerEntity manager)
	{
		if (m_fRealHoursPerDay <= 0)
			return;
		// SetDayDuration attend des secondes réelles par journée (24 h réelles = 86 400)
		manager.SetDayDuration(m_fRealHoursPerDay * 3600.0);
		manager.SetIsDayAutoAdvanced(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : vitesse du cycle
	float GetRealHoursPerDay()
	{
		return m_fRealHoursPerDay;
	}

	//------------------------------------------------------------------------------------------------
	//! Fixe l'heure (menu Staff)
	string SetHour(float hour, string author)
	{
		TimeAndWeatherManagerEntity manager = SRP_DayNightComponent.Weather();
		if (!manager)
			return "Pas de gestionnaire de temps dans le monde";
		manager.SetTimeOfTheDay(hour, true);
		Save();
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : heure fixée à %2", author, HourText(hour)));
		return "Heure : " + HourText(hour);
	}

	//------------------------------------------------------------------------------------------------
	//! Remet l'heure du tout premier lancement (remise à zéro)
	string ResetTime(string author)
	{
		TimeAndWeatherManagerEntity manager = SRP_DayNightComponent.Weather();
		if (!manager)
			return "Pas de gestionnaire de temps dans le monde";
		manager.SetTimeOfTheDay(m_fFirstLaunchHour, true);
		Save();
		SRP_JournalComponent.Log("STAFF", string.Format("%1 : heure remise à %2", author, HourText(m_fFirstLaunchHour)));
		return "Heure remise à " + HourText(m_fFirstLaunchHour);
	}

	//------------------------------------------------------------------------------------------------
	string SetSpeed(float realHoursPerDay, string author)
	{
		if (realHoursPerDay < 0.5 || realHoursPerDay > 240)
			return "Entre 0,5 et 240 heures réelles par journée";
		m_fRealHoursPerDay = realHoursPerDay;
		TimeAndWeatherManagerEntity manager = SRP_DayNightComponent.Weather();
		if (manager)
			ApplySpeed(manager);
		SRP_JournalComponent.Log("STAFF", string.Format("%1 règle la journée à %2 h réelles", author, realHoursPerDay));
		return string.Format("Une journée dure maintenant %1 h réelles", realHoursPerDay);
	}

	//------------------------------------------------------------------------------------------------
	static string HourText(float hour)
	{
		int h = hour;
		int m = (hour - h) * 60;
		return string.Format("%1:%2", SRP_Time.Pad2(h), SRP_Time.Pad2(m));
	}

	//------------------------------------------------------------------------------------------------
	void Save()
	{
		if (!Replication.IsServer() || !m_bStarted)
			return;
		TimeAndWeatherManagerEntity manager = SRP_DayNightComponent.Weather();
		if (!manager)
			return;
		JsonSaveContext ctx = new JsonSaveContext();
		ctx.WriteValue("hour", manager.GetTimeOfTheDay());
		ctx.WriteValue("savedAt", SRP_Time.Now());
		if (!ctx.SaveToFile(PATH))
			Print("[SRP] Échec de sauvegarde de l'heure : " + PATH, LogLevel.ERROR);
	}
}
