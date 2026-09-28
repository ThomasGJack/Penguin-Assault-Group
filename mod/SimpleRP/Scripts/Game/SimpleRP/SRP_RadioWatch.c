//------------------------------------------------------------------------------------------------
// SimpleRP — Surveillance des radios
// Symptôme constaté en dédié : loin de la base, deux joueurs à 100 m l'un de l'autre ne s'entendent plus à la
// radio, comme si la portée se comptait depuis la base. La portée se mesure entre les OBJETS radio : si un objet
// radio ne suit plus son porteur (position figée là où il a été pris), c'est exactement ce qu'on obtient.
// Ce fichier, côté serveur, toutes les 5 s (appelé par SRP_PlayerManagerComponent.RefreshPositions) :
//   - mesure l'écart entre chaque joueur et sa radio (radio individuelle et radio dorsale) ;
//   - note au journal (catégorie RADIO) le premier écart anormal de chaque radio, avec ses détails ;
//   - recale la radio sur son porteur ;
//   - impose la portée illimitée et la clé de chiffrement de la faction sur chaque radio portée.
// Report() donne l'état de toutes les radios : commande « radio » de la passerelle Discord.
//------------------------------------------------------------------------------------------------

class SRP_RadioWatch
{
	protected static const float UNLIMITED_RANGE = 65535;	// le maximum accepté par le moteur : toute l'île, et bien au-delà
	protected static const float MAX_GAP = 10;			// au-delà de 10 m entre un joueur et sa radio, c'est anormal
	protected static ref set<IEntity> s_Reported = new set<IEntity>();
	protected static int s_iFixes = 0;

	//------------------------------------------------------------------------------------------------
	//! Les objets radio portés par un personnage
	static void GetRadios(IEntity character, out array<IEntity> radios)
	{
		SCR_GadgetManagerComponent gadgets = SCR_GadgetManagerComponent.GetGadgetManager(character);
		if (!gadgets)
			return;
		IEntity personal = gadgets.GetGadgetByType(EGadgetType.RADIO);
		if (personal)
			radios.Insert(personal);
		IEntity backpack = gadgets.GetGadgetByType(EGadgetType.RADIO_BACKPACK);
		if (backpack && backpack != personal)
			radios.Insert(backpack);
	}

	//------------------------------------------------------------------------------------------------
	static void Tick()
	{
		if (!Replication.IsServer())
			return;

		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		foreach (int playerId : ids)
		{
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!character)
				continue;

			array<IEntity> radios = {};
			GetRadios(character, radios);
			foreach (IEntity radio : radios)
			{
				EnforceSettings(character, radio);

				float gap = vector.Distance(radio.GetOrigin(), character.GetOrigin());
				if (gap <= MAX_GAP)
					continue;

				if (!s_Reported.Contains(radio))
				{
					s_Reported.Insert(radio);
					string parentText = "aucun";
					if (radio.GetParent())
						parentText = "oui";
					SRP_JournalComponent.Log("RADIO", string.Format("%1 : sa radio est à %2 m de lui (radio en %3, joueur en %4, parent : %5) — recalée",
						GetGame().GetPlayerManager().GetPlayerName(playerId), Math.Round(gap), SRP_FleetManagerComponent.GridOf(radio.GetOrigin()),
						SRP_FleetManagerComponent.GridOf(character.GetOrigin()), parentText));
				}

				vector mat[4];
				character.GetWorldTransform(mat);
				radio.SetWorldTransform(mat);
				radio.Update();
				s_iFixes++;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Portée illimitée et clé de la faction sur toute radio portée (filet de sécurité : les prefabs
	//! Radio_ER328 et Radio_Deployable_Base sont aussi surchargés, mais une radio d'un autre mod passerait à côté)
	protected static void EnforceSettings(IEntity character, IEntity radio)
	{
		BaseRadioComponent component = BaseRadioComponent.Cast(radio.FindComponent(BaseRadioComponent));
		if (!component)
			return;

		for (int t = 0; t < component.TransceiversCount(); t++)
		{
			BaseTransceiver transceiver = component.GetTransceiver(t);
			if (transceiver && transceiver.GetRange() < UNLIMITED_RANGE)
				transceiver.SetRange(UNLIMITED_RANGE);
		}

		SCR_Faction faction = SCR_Faction.Cast(SCR_Faction.GetEntityFaction(character));
		if (!faction)
			return;
		string key = faction.GetFactionRadioEncryptionKey();
		if (!key.IsEmpty() && component.GetEncryptionKey() != key)
		{
			string radioName = "radio";
			EntityPrefabData prefabData = radio.GetPrefabData();
			if (prefabData)
				radioName = SRP_Utils.PrefabShortName(prefabData.GetPrefabName());
			SRP_JournalComponent.Log("RADIO", string.Format("%1 : radio avec la clé « %2 », remise sur la clé de la faction", radioName, component.GetEncryptionKey()));
			component.SetEncryptionKey(key);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! État des radios de tous les joueurs en ligne, une ligne par joueur
	static string Report()
	{
		array<int> ids = {};
		GetGame().GetPlayerManager().GetPlayers(ids);
		if (ids.IsEmpty())
			return "personne en ligne";

		string text = string.Format("%1 recalage(s) de radio depuis le démarrage.", s_iFixes);
		foreach (int playerId : ids)
		{
			string line = "\n• " + GetGame().GetPlayerManager().GetPlayerName(playerId) + " : ";
			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!character)
			{
				text += line + "pas de personnage";
				continue;
			}
			line += "carré " + SRP_FleetManagerComponent.GridOf(character.GetOrigin());

			array<IEntity> radios = {};
			GetRadios(character, radios);
			if (radios.IsEmpty())
				line += ", AUCUNE radio";
			foreach (IEntity radio : radios)
			{
				BaseRadioComponent component = BaseRadioComponent.Cast(radio.FindComponent(BaseRadioComponent));
				if (!component)
					continue;
				int gap = Math.Round(vector.Distance(radio.GetOrigin(), character.GetOrigin()));
				string power = "allumée";
				if (!component.IsPowered())
					power = "ÉTEINTE";
				line += string.Format(" | radio à %1 m, %2, clé %3", gap, power, component.GetEncryptionKey());
				for (int t = 0; t < component.TransceiversCount(); t++)
				{
					BaseTransceiver transceiver = component.GetTransceiver(t);
					if (!transceiver)
						continue;
					int channel = t + 1;
					line += string.Format(", canal %1 : %2 kHz portée %3 m", channel, transceiver.GetFrequency(), Math.Round(transceiver.GetRange()));
				}
			}
			text += line;
		}
		return text;
	}
}
