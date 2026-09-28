//------------------------------------------------------------------------------------------------
// SimpleRP — Points de livraison et garnisons
// Des marqueurs SRP_PointLivraison (composant SRP_DeliveryPointComponent) posés dans le monde, avec
// deux cases : caisses, véhicules. À chaque livraison, le gestionnaire (SRP_DeliveryComponent, sur
// le game mode) choisit un point du bon type sans joueur à moins de m_fClearRadius (2 km) — à
// défaut le plus éloigné des joueurs — puis tire l'emprise ennemie : une garnison IA (groupes
// du SRP_EnemyComponent, effectif adapté aux joueurs connectés) apparaît autour du point
// avec un ordre de défense, avant la marchandise. La garnison reste jusqu'à être tuée, ou est
// retirée après un délai si personne n'est venu.
// Sans point posé, le marqueur SRP_Livraison d'avant reste utilisé.
// Front (#75, G13) : parmi les points libres, ceux d'un carré BLEU passent d'abord (m_bPreferBlue) ; sans point bleu
// libre, choix comme avant. La chance d'emprise suit la menace de l'île (SRP_FrontComponent.GetThreat). La garnison
// demande sa place aux 120 soldats ennemis (SRP_CmdCapacity.Ask, classe GARDE) : refusée, le tirage est perdu.
// 1.0.26 : une livraison en cours est retenue (SRP_ActiveDelivery) ; la nuit, une lanterne allumée est posée
// à côté (SRP_GenerateurComponent.SpawnLantern) et retirée le jour ou quand la livraison se termine (quelqu'un
// est venu la chercher puis est reparti, ou délai écoulé).
// Phase 3 — étape D
//------------------------------------------------------------------------------------------------

enum SRP_EDeliveryKind
{
	CRATES,
	VEHICLE
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Point de livraison SimpleRP : caisses et/ou véhicules, emprise ennemie possible")]
class SRP_DeliveryPointComponentClass : ScriptComponentClass
{
}

class SRP_DeliveryPointComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Nom affiché aux joueurs (vide = nom de l'entité)", category: "SimpleRP")]
	string m_sLabel;

	[Attribute("1", UIWidgets.CheckBox, "Reçoit les caisses de ravitaillement", category: "SimpleRP")]
	bool m_bCrates;

	[Attribute("0", UIWidgets.CheckBox, "Reçoit les véhicules", category: "SimpleRP")]
	bool m_bVehicles;

	[Attribute("-1", UIWidgets.EditBox, "Chance d'emprise ennemie à ce point, en % (-1 = valeur globale du gestionnaire)", category: "SimpleRP")]
	float m_fEnemyChanceOverride;

	protected static ref array<SRP_DeliveryPointComponent> s_aPoints = {};

	//------------------------------------------------------------------------------------------------
	static array<SRP_DeliveryPointComponent> GetPoints()
	{
		return s_aPoints;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer() && !s_aPoints.Contains(this))
			s_aPoints.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aPoints.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	string GetLabel()
	{
		if (!m_sLabel.IsEmpty())
			return m_sLabel;
		string name = GetOwner().GetName();
		if (name.IsEmpty())
			return "point de livraison";
		return name;
	}

	//------------------------------------------------------------------------------------------------
	bool Accepts(int kind)
	{
		if (kind == SRP_EDeliveryKind.VEHICLE)
			return m_bVehicles;
		return m_bCrates;
	}
}

//------------------------------------------------------------------------------------------------
//! Une garnison posée à un point
class SRP_Garrison
{
	ref array<ref SRP_EnemyGroup> m_aGroups = {};
	vector m_vPosition;
	string m_sLabel;
	int m_iSpawnTick;
}

//------------------------------------------------------------------------------------------------
//! Une livraison en cours à un point : lanterne la nuit
class SRP_ActiveDelivery
{
	vector m_vPosition;
	vector m_vLanternPosition;
	string m_sLabel;
	int m_iStartTick;
	bool m_bVisited;
	IEntity m_Lantern;
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Livraisons SimpleRP : choix du point, emprise ennemie, garnisons, lanterne la nuit")]
class SRP_DeliveryComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_DeliveryComponent : SCR_BaseGameModeComponent
{
	[Attribute("2000", UIWidgets.EditBox, "Un point n'est retenu que si aucun joueur n'est à moins de … mètres (sinon le plus éloigné)", category: "SimpleRP - Points")]
	protected float m_fClearRadius;

	[Attribute("SRP_Livraison", UIWidgets.EditBox, "Marqueur de secours si aucun point de livraison n'est posé", category: "SimpleRP - Points")]
	protected string m_sFallbackMarker;

	[Attribute("1", UIWidgets.CheckBox, "Préférer les points de livraison en territoire bleu (carré à nous sur la carte du front) ; sans point bleu libre, choix comme avant", category: "SimpleRP - Points")]
	protected bool m_bPreferBlue;

	[Attribute("30", UIWidgets.EditBox, "Chance qu'un point de livraison soit sous emprise ennemie, en %", category: "SimpleRP - Garnison")]
	protected float m_fEnemyChance;

	[Attribute("1", UIWidgets.EditBox, "Groupes minimum par garnison (prefabs et effectif : SRP_EnemyComponent)", category: "SimpleRP - Garnison")]
	protected int m_iGroupsMin;

	[Attribute("3", UIWidgets.EditBox, "Groupes maximum par garnison", category: "SimpleRP - Garnison")]
	protected int m_iGroupsMax;

	[Attribute("40", UIWidgets.EditBox, "Rayon de dispersion des groupes autour du point, en mètres", category: "SimpleRP - Garnison")]
	protected float m_fGarrisonRadius;

	[Attribute("180", UIWidgets.EditBox, "Une garnison que personne n'est venu voir est retirée après … minutes", category: "SimpleRP - Garnison")]
	protected int m_iGarrisonLifetimeMinutes;

	[Attribute("0", UIWidgets.CheckBox, "Dire aux officiers si le point est gardé (sinon ils le découvrent sur place)", category: "SimpleRP - Garnison")]
	protected bool m_bAnnounceEnemy;

	[Attribute("1", UIWidgets.CheckBox, "La nuit, une lanterne allumée à côté d'une livraison en cours (prefab : SRP_GenerateurComponent)", category: "SimpleRP - Lanterne")]
	protected bool m_bNightLantern;

	[Attribute("30", UIWidgets.EditBox, "Un joueur à moins de … mètres du point est venu chercher la livraison", category: "SimpleRP - Lanterne")]
	protected float m_fVisitRadius;

	[Attribute("300", UIWidgets.EditBox, "Livraison terminée quand, après une visite, plus aucun joueur n'est à moins de … mètres", category: "SimpleRP - Lanterne")]
	protected float m_fLeaveRadius;

	[Attribute("180", UIWidgets.EditBox, "Une livraison que personne n'est venu chercher est considérée terminée après … minutes (lanterne retirée)", category: "SimpleRP - Lanterne")]
	protected int m_iDeliveryLifetimeMinutes;

	protected static const int GARRISON_GROUP_SOLDIERS = 5;	// soldats comptés par groupe de garnison (demande de place)

	protected static SRP_DeliveryComponent s_Instance;

	protected ref array<ref SRP_Garrison> m_aGarrisons = {};
	protected ref array<ref SRP_ActiveDelivery> m_aDeliveries = {};
	protected int m_iGarrisonSerial;		// numéro de la clé de demande de place (« livraison-12 »)

	//------------------------------------------------------------------------------------------------
	static SRP_DeliveryComponent GetInstance()
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
		GetGame().GetCallqueue().CallLater(Tick, 60000, true);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (Replication.IsServer())
			GetGame().GetCallqueue().Remove(Tick);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	// Choix du point
	//------------------------------------------------------------------------------------------------
	//! Retourne la transformation du point choisi, son nom, et si une garnison a été posée. false si aucun point n'existe.
	bool ChoosePoint(int kind, out vector mat[4], out string label, out bool hostile)
	{
		hostile = false;
		label = "";

		// Le front, une seule fois : carrés bleus (G13) et menace de l'île
		SRP_FrontComponent front = SRP_FrontComponent.GetInstance();
		bool frontReady = front && front.IsReady();

		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		array<SRP_DeliveryPointComponent> blueCandidates = {};
		array<SRP_DeliveryPointComponent> candidates = {};
		SRP_DeliveryPointComponent farthest;
		float farthestDistance = -1;

		foreach (SRP_DeliveryPointComponent point : SRP_DeliveryPointComponent.GetPoints())
		{
			if (!point || !point.Accepts(kind))
				continue;

			vector pointPosition = point.GetOwner().GetOrigin();
			float distance = SRP_Utils.NearestPlayerDistance(pointPosition, players);
			if (distance >= m_fClearRadius)
			{
				candidates.Insert(point);
				if (m_bPreferBlue && frontReady && front.IsBlueAt(pointPosition))
					blueCandidates.Insert(point);
			}
			if (distance > farthestDistance)
			{
				farthestDistance = distance;
				farthest = point;
			}
		}

		// Un point libre en territoire bleu d'abord, puis un point libre, puis le plus éloigné des joueurs
		SRP_DeliveryPointComponent chosen;
		if (!blueCandidates.IsEmpty())
			chosen = blueCandidates.GetRandomElement();
		else if (!candidates.IsEmpty())
		{
			chosen = candidates.GetRandomElement();
			if (m_bPreferBlue && frontReady)
				SRP_JournalComponent.Log("LIVRAISON", string.Format("Aucun point libre en territoire bleu : %1 retenu", chosen.GetLabel()));
		}
		else if (farthest)
		{
			chosen = farthest;
			SRP_JournalComponent.Log("LIVRAISON", string.Format("Tous les points ont un joueur à moins de %1 m : %2 retenu (le plus éloigné)", Math.Round(m_fClearRadius), chosen.GetLabel()));
		}

		if (chosen)
		{
			chosen.GetOwner().GetWorldTransform(mat);
			label = chosen.GetLabel();

			// Emprise : min(40, 10 + 3 x menace de l'île), inchangée ; sans front prêt, la valeur du gestionnaire
			float chance = m_fEnemyChance;
			if (frontReady)
				chance = Math.Min(40.0, 10 + 3 * front.GetThreat());
			if (chosen.m_fEnemyChanceOverride >= 0)
				chance = chosen.m_fEnemyChanceOverride;
			if (Math.RandomFloat(0, 100) < chance)
				hostile = SpawnGarrison(mat[3], label);
			Track(mat, label);
			return true;
		}

		// Secours : l'ancien marqueur unique
		IEntity fallback = GetGame().GetWorld().FindEntityByName(m_sFallbackMarker);
		if (!fallback)
			return false;
		fallback.GetWorldTransform(mat);
		label = m_sFallbackMarker;
		Track(mat, label);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// Livraisons en cours : lanterne la nuit
	//------------------------------------------------------------------------------------------------
	//! Retient la livraison qui vient d'être posée à ce point
	protected void Track(vector mat[4], string label)
	{
		SRP_ActiveDelivery delivery = new SRP_ActiveDelivery();
		delivery.m_vPosition = mat[3];
		// À côté de la marchandise (les caisses partent à 1,5 m devant le marqueur) : 1,5 m derrière, 2,5 m sur le côté
		delivery.m_vLanternPosition = mat[3] - mat[0] * 1.5 - mat[2] * 2.5;
		delivery.m_sLabel = label;
		delivery.m_iStartTick = System.GetTickCount();
		m_aDeliveries.Insert(delivery);
		UpdateDeliveries(SRP_Utils.GetPlayerCharacters(), delivery.m_iStartTick);
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque minute : lanterne posée la nuit, retirée le jour ; livraison terminée quand quelqu'un est venu puis
	//! reparti, ou après le délai
	protected void UpdateDeliveries(array<IEntity> players, int now)
	{
		if (m_aDeliveries.IsEmpty())
			return;

		bool night = SRP_DayNightComponent.IsNight();
		for (int i = m_aDeliveries.Count() - 1; i >= 0; i--)
		{
			SRP_ActiveDelivery delivery = m_aDeliveries[i];
			float distance = SRP_Utils.NearestPlayerDistance(delivery.m_vPosition, players);
			if (distance <= m_fVisitRadius)
				delivery.m_bVisited = true;

			bool finished = delivery.m_bVisited && distance > m_fLeaveRadius;
			if (now - delivery.m_iStartTick > m_iDeliveryLifetimeMinutes * 60 * 1000)
				finished = true;
			if (finished)
			{
				if (delivery.m_Lantern)
					SRP_JournalComponent.Log("LIVRAISON", string.Format("Lanterne retirée au point « %1 » (livraison terminée)", delivery.m_sLabel));
				SRP_GenerateurComponent.RemoveLantern(delivery.m_Lantern);
				delivery.m_Lantern = null;
				m_aDeliveries.Remove(i);
				continue;
			}

			if (delivery.m_Lantern && delivery.m_Lantern.IsDeleted())
				delivery.m_Lantern = null;

			if (night && m_bNightLantern)
			{
				if (!delivery.m_Lantern)
				{
					delivery.m_Lantern = SRP_GenerateurComponent.SpawnLantern(delivery.m_vLanternPosition);
					if (delivery.m_Lantern)
						SRP_JournalComponent.Log("LIVRAISON", string.Format("Nuit : lanterne posée au point « %1 »", delivery.m_sLabel));
				}
			}
			else if (delivery.m_Lantern)
			{
				SRP_GenerateurComponent.RemoveLantern(delivery.m_Lantern);
				delivery.m_Lantern = null;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Message d'arrivée pour les officiers
	string ArrivalText(string what, string label, bool hostile)
	{
		string text = string.Format("%1 : arrivé au point « %2 »", what, label);
		if (hostile && m_bAnnounceEnemy)
			text += " — zone sous emprise ennemie, à sécuriser";
		return text;
	}

	//------------------------------------------------------------------------------------------------
	// Garnison
	//------------------------------------------------------------------------------------------------
	protected bool SpawnGarrison(vector center, string label)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (!enemies || !enemies.HasPrefabs())
		{
			Print("[SRP] Livraison : emprise ennemie tirée mais SRP_EnemyComponent absent ou sans prefab de groupe", LogLevel.WARNING);
			return false;
		}

		SRP_Garrison garrison = new SRP_Garrison();
		garrison.m_vPosition = center;
		garrison.m_sLabel = label;
		garrison.m_iSpawnTick = System.GetTickCount();

		int groups = Math.Max(m_iGroupsMin, enemies.ScaleGroupsNear(center, 0, m_iGroupsMax));

		// Q7 : la place des 120 soldats ennemis (classe GARDE). Refusée, pas de garnison : le tirage est perdu, et la
		// demande est retirée de la file puisque la livraison ne réessaie pas
		m_iGarrisonSerial++;
		string roomKey = "livraison-" + m_iGarrisonSerial.ToString();
		SRP_CmdCapacity capacity = SRP_CmdCapacity.Get();
		string refusal = capacity.Ask(roomKey, SRP_ECmdCapClass.GARDE, groups * GARRISON_GROUP_SOLDIERS, groups, center, "livraison");
		if (!refusal.IsEmpty())
		{
			capacity.ClearDemand(roomKey);
			SRP_JournalComponent.Log("LIVRAISON", string.Format("Emprise ennemie tirée au point « %1 », mais pas de garnison (%2)", label, refusal));
			return false;
		}

		int spawned = enemies.SpawnGroups(center, m_fGarrisonRadius, groups, true, center, "livraison", garrison.m_aGroups);
		if (spawned <= 0)
			return false;
		foreach (SRP_EnemyGroup record : garrison.m_aGroups)
			SRP_CmdCapacity.Tag(record, SRP_ECmdCapClass.GARDE);

		m_aGarrisons.Insert(garrison);
		SRP_JournalComponent.Log("LIVRAISON", string.Format("Garnison ennemie de %1 groupe(s) au point « %2 »", spawned, label));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Entretien : garnisons mortes ou oubliées
	protected void Tick()
	{
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		int now = System.GetTickCount();
		UpdateDeliveries(players, now);

		if (m_aGarrisons.IsEmpty())
			return;

		for (int i = m_aGarrisons.Count() - 1; i >= 0; i--)
		{
			SRP_Garrison garrison = m_aGarrisons[i];

			if (SRP_EnemyComponent.IsWipedOut(garrison.m_aGroups, now))	// pose terminée (file d'apparition de la 1.8), plus personne
			{
				SRP_JournalComponent.Log("LIVRAISON", string.Format("Garnison du point « %1 » neutralisée", garrison.m_sLabel));
				Remove(garrison);
				m_aGarrisons.Remove(i);
				continue;
			}

			bool expired = now - garrison.m_iSpawnTick > m_iGarrisonLifetimeMinutes * 60 * 1000;
			if (expired && SRP_Utils.NearestPlayerDistance(garrison.m_vPosition, players) > 500)
			{
				SRP_JournalComponent.Log("LIVRAISON", string.Format("Garnison du point « %1 » retirée (personne n'est venu)", garrison.m_sLabel));
				Remove(garrison);
				m_aGarrisons.Remove(i);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Staff : toutes les garnisons retirées
	void ResetAll()
	{
		foreach (SRP_Garrison garrison : m_aGarrisons)
			Remove(garrison);
		m_aGarrisons.Clear();
		foreach (SRP_ActiveDelivery delivery : m_aDeliveries)
			SRP_GenerateurComponent.RemoveLantern(delivery.m_Lantern);
		m_aDeliveries.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void Remove(SRP_Garrison garrison)
	{
		SRP_EnemyComponent enemies = SRP_EnemyComponent.GetInstance();
		if (enemies)
			enemies.DeleteAll(garrison.m_aGroups);
		garrison.m_aGroups.Clear();
	}

	//------------------------------------------------------------------------------------------------
	string GetStatusText()
	{
		array<SRP_DeliveryPointComponent> points = SRP_DeliveryPointComponent.GetPoints();
		string text = string.Format("Points de livraison : %1 — garnisons actives : %2", points.Count(), m_aGarrisons.Count());
		foreach (SRP_DeliveryPointComponent point : points)
		{
			string kinds = "";
			if (point.m_bCrates)
				kinds += "caisses ";
			if (point.m_bVehicles)
				kinds += "véhicules";
			text += string.Format("\n%1 : %2", point.GetLabel(), kinds);
		}
		foreach (SRP_Garrison garrison : m_aGarrisons)
			text += string.Format("\nGarnison « %1 » : %2 groupe(s)", garrison.m_sLabel, garrison.m_aGroups.Count());
		foreach (SRP_ActiveDelivery delivery : m_aDeliveries)
		{
			string lantern = "";
			if (delivery.m_Lantern)
				lantern = ", lanterne posée";
			text += string.Format("\nLivraison en cours « %1 » (visitée : %2%3)", delivery.m_sLabel, SRP_Utils.OuiNon(delivery.m_bVisited), lantern);
		}
		return text;
	}
}
