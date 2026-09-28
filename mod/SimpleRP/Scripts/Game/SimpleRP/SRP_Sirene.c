//------------------------------------------------------------------------------------------------
// SimpleRP — Sirène d'alerte des localités ennemies (carte #69, livraison 2)
// - Un poteau de sirène militaire, visible et destructible (prefab SRP_Sirene), est posé près du point clé CENTRE de
//   chaque localité ennemie gardée (repère posé au Workbench ; à défaut la ligne « point » du fichier de retouches, puis
//   le nom de la carte), loin des routes, hors de vue des joueurs, à la pose de sa garnison (SRP_Territory). Les
//   sirènes restent rattachées aux LOCALITÉS (par leur nom) : les postes du front et les rondes n'en ont jamais.
// - Quand un soldat de la garnison VOIT un joueur dans la localité (rayon de la localité + marge ; un simple bruit ne
//   suffit pas), la sirène sonne environ 8 s plus tard, pendant environ 1 min, chez TOUS les joueurs (ceux qui arrivent pendant
//   qu'elle sonne l'entendent aussi) ; une fois toutes les 10 min au plus par localité. Au départ, le groupe qui a vu
//   doit encore avoir un soldat en état de combattre, et le poteau doit être intact.
// - Détruire le poteau (environ 5 balles de fusil ou une grenade) la fait taire, chez tous.
// - Elle sonne même si l'opérateur radio est mort. Jamais pour une mission en campagne, ni pour un repérage de
//   l'hélicoptère. Après la prise de la localité (tous ses points clés saisis, SRP_ZoneFall), le poteau reste en place,
//   muet ; il reprend du service si l'ennemi reprend la zone et y repose une garnison.
// Serveur : pose des poteaux, demandes, départ, veille (fin, poteau abîmé ou supprimé). Chaque machine qui a un joueur
// (client, serveur hébergé, Workbench) : le son est une entité LOCALE sans maillage posée au-dessus du poteau (signal
// « Siren » à 1, événement SOUND_SIREN_LP relancé tant qu'il reste du temps ; la boucle du jeu est infinie, seul notre
// arrêt la coupe). Serveur dédié : jamais de son.
// Rien sur Discord ni aux joueurs : journal ENNEMI, et l'état de chaque sirène sur la page Staff « État des groupes ».
// Modèle : SRP_GenerateurComponent (RPC de diffusion, RplSave / RplLoad pour les arrivants).
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Sirène d'alerte SimpleRP : poteau au centre des localités ennemies, alerte sonore quand la garnison voit un joueur")]
class SRP_SireneComponentClass : SCR_BaseGameModeComponentClass
{
}

class SRP_SireneComponent : SCR_BaseGameModeComponent
{
	[Attribute("1", UIWidgets.CheckBox, "Sirène d'alerte des localités ennemies : un poteau au centre, l'alerte sonne quand la garnison voit un joueur", category: "SimpleRP - Sirène")]
	protected bool m_bEnabled;

	[Attribute("{6A5B0C0D0E0FB041}Prefabs/Props/SRP_Sirene.et", UIWidgets.ResourceNamePicker, "Poteau de sirène (visible, destructible)", "et", category: "SimpleRP - Sirène")]
	protected ResourceName m_sPolePrefab;

	[Attribute("{6A5B0C0D0E0FB042}Prefabs/Props/SRP_SireneSon.et", UIWidgets.ResourceNamePicker, "Son de la sirène : entité locale sans maillage (signaux et SoundComponent de la sirène militaire)", "et", category: "SimpleRP - Sirène")]
	protected ResourceName m_sSoundPrefab;

	[Attribute("SOUND_SIREN_LP", UIWidgets.EditBox, "Événement sonore joué (boucle de 9 s, de près et au loin)", category: "SimpleRP - Sirène")]
	protected string m_sSoundEvent;

	[Attribute("Siren", UIWidgets.EditBox, "Signal du son, mis à 1 avant l'événement et à 0 à l'arrêt", category: "SimpleRP - Sirène")]
	protected string m_sSignalName;

	[Attribute("60", UIWidgets.EditBox, "Durée de la sirène, en secondes", category: "SimpleRP - Sirène")]
	protected int m_iSeconds;

	[Attribute("10", UIWidgets.EditBox, "Repos d'une localité entre deux départs de sa sirène, en minutes", category: "SimpleRP - Sirène")]
	protected int m_iCooldownMinutes;

	[Attribute("8", UIWidgets.EditBox, "Délai entre le repérage et la sirène, en secondes", category: "SimpleRP - Sirène")]
	protected int m_iDelaySeconds;

	[Attribute("8", UIWidgets.EditBox, "Hauteur du son au-dessus du pied du poteau, en mètres", category: "SimpleRP - Sirène")]
	protected float m_fSoundHeight;

	[Attribute("40", UIWidgets.EditBox, "Pose du poteau : premier rayon de recherche autour du centre de la localité, en mètres", category: "SimpleRP - Sirène")]
	protected float m_fPoleSearchRadius;

	[Attribute("120", UIWidgets.EditBox, "Pose du poteau : rayon de recherche maximal, en mètres (trois paliers de 8 essais)", category: "SimpleRP - Sirène")]
	protected float m_fPoleSearchRadiusMax;

	[Attribute("12", UIWidgets.EditBox, "Pose du poteau : aucune route à moins de … mètres", category: "SimpleRP - Sirène")]
	protected float m_fPoleRoadClearance;

	[Attribute("6", UIWidgets.EditBox, "Pose du poteau : dégagement de route réduit à … mètres après 3 poses manquées", category: "SimpleRP - Sirène")]
	protected float m_fPoleRoadClearanceMin;

	[Attribute("60", UIWidgets.EditBox, "Pose du poteau manquée : nouvel essai au plus toutes les … secondes", category: "SimpleRP - Sirène")]
	protected int m_iPoleRetrySeconds;

	[Attribute("800", UIWidgets.EditBox, "Retrait de la garnison : le poteau n'est supprimé que si le joueur le plus proche est à plus de … mètres et ne le voit pas", category: "SimpleRP - Sirène")]
	protected float m_fPoleDeleteDistance;

	protected static const int MAX_SYNC = 32;			// sirènes lues au plus à la connexion (garde-fou)
	protected static const float POLE_PLAYER_MIN = 300;	// pose : aucun joueur à moins de … mètres

	protected static SRP_SireneComponent s_Instance;

	// Serveur : sirènes en train de sonner (tableaux parallèles), départs en attente, dernier départ par localité
	protected ref array<string> m_aTowns = {};
	protected ref array<IEntity> m_aPoles = {};
	protected ref array<RplId> m_aIds = {};
	protected ref array<vector> m_aPositions = {};
	protected ref array<int> m_aEnds = {};
	protected ref array<string> m_aPending = {};
	protected ref map<string, int> m_mLastStart = new map<string, int>();
	protected bool m_bWatching;
	protected bool m_bPoleBroken;
	protected bool m_bRplMissing;

	// Chaque machine qui joue le son : poteau (RplId), entité locale, son, fin (tick local)
	protected ref array<RplId> m_aLocalIds = {};
	protected ref array<IEntity> m_aLocalEntities = {};
	protected ref array<AudioHandle> m_aLocalHandles = {};
	protected ref array<int> m_aLocalEnds = {};
	protected bool m_bLocalTicking;
	protected bool m_bSoundBroken;

	// Arrivant en cours de partie : sirènes reçues à la connexion (temps restant à ce moment-là, tick local de la
	// réception), jouées une fois le monde en place ; un ordre d'arrêt reçu entre-temps les retire
	protected ref array<RplId> m_aPendingIds = {};
	protected ref array<vector> m_aPendingPositions = {};
	protected ref array<int> m_aPendingMs = {};
	protected int m_iPendingTick;

	//------------------------------------------------------------------------------------------------
	static SRP_SireneComponent GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;	// serveur et joueurs : les RPC et l'arrivée en cours de partie passent par ce composant
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(TriggerSiren);
		GetGame().GetCallqueue().Remove(ServerWatch);
		GetGame().GetCallqueue().Remove(LocalTick);
		GetGame().GetCallqueue().Remove(ApplyPending);
		StopAllLocal();
		if (s_Instance == this)
			s_Instance = null;
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! La sirène est-elle en service (case cochée, prefab de poteau réglé) ?
	bool IsSirenEnabled()
	{
		return m_bEnabled && !m_sPolePrefab.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	//! Délai entre deux essais de pose d'un poteau, en millisecondes
	int GetPoleRetryMs()
	{
		return Math.ClampInt(m_iPoleRetrySeconds, 10, 3600) * 1000;
	}

	//------------------------------------------------------------------------------------------------
	// Serveur : poteaux
	//------------------------------------------------------------------------------------------------
	//! Pose le poteau d'une localité : trois paliers de rayon autour de « center » (le point clé CENTRE de la localité,
	//! SRP_SectorDef.GetCenter), 8 essais chacun ; un point libre au sol,
	//! à ciel ouvert, sans route à moins du dégagement (réduit après 3 poses manquées), ni eau, ni base, aucun joueur à
	//! moins de 300 m ni qui le voie. « attempt » : poses déjà manquées pour cette localité. Null si rien ne convient
	//! (le Territoire réessaie au plus toutes les m_iPoleRetrySeconds).
	IEntity SpawnPole(string town, vector center, int attempt)
	{
		if (!Replication.IsServer() || !IsSirenEnabled())
			return null;
		Resource res = Resource.Load(m_sPolePrefab);
		if (!res || !res.IsValid())
		{
			if (!m_bPoleBroken)
			{
				m_bPoleBroken = true;
				Print("[SRP] Sirène : prefab de poteau introuvable " + m_sPolePrefab, LogLevel.WARNING);
			}
			return null;
		}

		float clearance = m_fPoleRoadClearance;
		if (attempt >= 3)
			clearance = m_fPoleRoadClearanceMin;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		float firstRadius = Math.Max(m_fPoleSearchRadius, 5);
		float lastRadius = Math.Max(m_fPoleSearchRadiusMax, firstRadius);

		// Un joueur si près du centre qu'aucun point de la recherche (point libre décalé de 10 m au plus compris) ne peut
		// être à plus de 300 m de lui : rien ne peut convenir, on n'essaie même pas (le Territoire réessaiera)
		bool hopeless = SRP_Placement.NearestPlayer(center, players) + lastRadius + 10 < POLE_PLAYER_MIN;
		for (int tier = 0; tier < 3 && !hopeless; tier++)
		{
			float radius = firstRadius + (lastRadius - firstRadius) * tier * 0.5;
			for (int probeIndex = 0; probeIndex < 8; probeIndex++)
			{
				float angle = Math.RandomFloat(0, Math.PI2);
				float distance = Math.RandomFloat(0, radius);
				vector probe = SRP_Placement.OnGround(center + Vector(Math.Cos(angle) * distance, 0, Math.Sin(angle) * distance));
				if (SRP_Placement.NearestPlayer(probe, players) < POLE_PLAYER_MIN)
					continue;	// test bon marché d'abord : un joueur trop près
				vector spot;
				if (!SCR_WorldTools.FindEmptyTerrainPosition(spot, probe, 10, 1.5, 4))
					continue;
				if (SRP_Placement.IsWater(spot) || SRP_Placement.IsNearBase(spot))
					continue;
				if (SRP_Placement.NearestPlayer(spot, players) < POLE_PLAYER_MIN)
					continue;
				vector road;
				if (SRP_Placement.FindRoadPoint(spot, clearance, road))
					continue;	// une route trop près
				if (IsUnderCover(spot))
					continue;	// sous un toit ou un arbre : il doit se voir
				if (IsPoleSeen(spot, players))
					continue;

				EntitySpawnParams params = new EntitySpawnParams();
				params.TransformMode = ETransformMode.WORLD;
				params.Transform[3] = spot;
				IEntity pole = GetGame().SpawnEntityPrefab(res, GetGame().GetWorld(), params);
				if (!pole)
				{
					Print("[SRP] Sirène : le poteau n'a pas pu être posé (" + m_sPolePrefab + ")", LogLevel.WARNING);
					return null;
				}
				SRP_EnemyComponent.Journal("ENNEMI", string.Format("Poteau de sirène posé à %1, à %2 m du centre", town, Math.Round(vector.Distance(spot, center))));
				return pole;
			}
		}

		// Une ligne à la première pose manquée, au passage au dégagement réduit, puis toutes les 10
		if (attempt == 0 || attempt == 3 || attempt - (attempt / 10) * 10 == 0)
			SRP_EnemyComponent.Journal("ENNEMI", string.Format("Poteau de sirène non posé à %1 (essai %2) : aucun point libre loin des routes, à ciel ouvert, hors de vue et à plus de 300 m des joueurs", town, attempt + 1));
		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Quelque chose au-dessus du point (toit, feuillage) ?
	protected static bool IsUnderCover(vector spot)
	{
		TraceParam trace = new TraceParam();
		trace.Start = spot + Vector(0, 0.5, 0);
		trace.End = spot + Vector(0, 15, 0);
		trace.Flags = TraceFlags.ENTS | TraceFlags.WORLD;
		trace.LayerMask = EPhysicsLayerDefs.Projectile;
		return GetGame().GetWorld().TraceMove(trace, null) < 0.99;
	}

	//------------------------------------------------------------------------------------------------
	//! Garnison retirée : la sirène se tait ; le poteau est supprimé si « force », ou si le joueur le plus proche est à
	//! plus de m_fPoleDeleteDistance et ne le voit pas. Vrai si le poteau n'existe plus (le Territoire l'oublie).
	bool RemovePole(string town, IEntity pole, bool force)
	{
		Silence(town);
		if (!pole || pole.IsDeleted())
			return true;
		if (!force && !DeletePoleIfUnseen(pole, m_fPoleDeleteDistance))
			return false;
		if (force)
			SCR_EntityHelper.DeleteEntityAndChildren(pole);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Supprime le poteau si le joueur le plus proche est à plus de « minDistance » et ne le voit pas ; vrai si supprimé
	bool DeletePoleIfUnseen(IEntity pole, float minDistance)
	{
		if (!pole || pole.IsDeleted())
			return true;
		array<IEntity> players = SRP_Utils.GetPlayerCharacters();
		vector position = pole.GetOrigin();
		if (SRP_Placement.NearestPlayer(position, players) <= minDistance)
			return false;
		if (IsPoleSeen(position, players))
			return false;
		SCR_EntityHelper.DeleteEntityAndChildren(pole);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Un joueur voit-il le poteau posé à « foot » ? Son pied, ou son sommet (le poteau fait une dizaine de mètres : on
	//! en voit le haut par-dessus un mur ou une crête), testé à la hauteur du son
	protected bool IsPoleSeen(vector foot, array<IEntity> players)
	{
		if (SRP_Placement.IsSeenByAnyPlayer(foot, players))
			return true;
		vector top = foot;
		top[1] = foot[1] + Math.Max(m_fSoundHeight, 2);
		return SRP_Placement.IsSeenByAnyPlayer(top, players);
	}

	//------------------------------------------------------------------------------------------------
	//! Le poteau est-il là et intact ?
	bool IsIntact(IEntity pole)
	{
		return pole && !pole.IsDeleted() && !IsDamaged(pole);
	}

	//------------------------------------------------------------------------------------------------
	//! Poteau abîmé : il a quitté sa première phase de dégâts, ou il est détruit
	static bool IsDamaged(IEntity pole)
	{
		if (!pole)
			return false;
		SCR_DestructionMultiPhaseComponent phases = SCR_DestructionMultiPhaseComponent.Cast(pole.FindComponent(SCR_DestructionMultiPhaseComponent));
		if (phases)
			return phases.GetDamagePhase() > 0 || phases.GetDestroyed();
		SCR_DestructionDamageManagerComponent damage = SCR_DestructionDamageManagerComponent.Cast(pole.FindComponent(SCR_DestructionDamageManagerComponent));
		return damage && damage.GetDestroyed();
	}

	//------------------------------------------------------------------------------------------------
	// Serveur : alerte
	//------------------------------------------------------------------------------------------------
	//! Un soldat de la garnison de « town » a vu un joueur dans la localité : départ dans m_iDelaySeconds. Refus
	//! silencieux : sirène coupée, poteau absent ou abîmé, déjà en train de sonner ou de partir, repos de CETTE localité.
	void Request(string town, IEntity pole, SCR_AIGroup spotter)
	{
		if (!Replication.IsServer() || !m_bEnabled || town.IsEmpty())
			return;
		if (!IsIntact(pole))
			return;
		if (m_aTowns.Contains(town) || m_aPending.Contains(town))
			return;
		int now = System.GetTickCount();
		int last;
		if (m_mLastStart.Find(town, last) && now - last < Math.ClampInt(m_iCooldownMinutes, 0, 1440) * 60 * 1000)
			return;
		m_aPending.Insert(town);
		GetGame().GetCallqueue().CallLater(TriggerSiren, Math.ClampInt(m_iDelaySeconds, 0, 120) * 1000, false, town, pole, spotter);
	}

	//------------------------------------------------------------------------------------------------
	//! Le départ : le groupe qui a vu doit encore avoir un soldat en état de combattre, et le poteau être intact (sinon
	//! rien) ; la sirène sonne chez tous les joueurs, la veille du serveur la coupe à la fin ou si le poteau est abîmé
	protected void TriggerSiren(string town, IEntity pole, SCR_AIGroup spotter)
	{
		if (!m_aPending.Contains(town))
			return;		// annulée entre-temps (capture, retrait de la garnison)
		m_aPending.RemoveItem(town);
		if (!m_bEnabled || m_aTowns.Contains(town))
			return;
		if (!IsIntact(pole))
		{
			SRP_EnemyComponent.Journal("ENNEMI", "Sirène de " + town + " : pas de départ, le poteau est abîmé ou retiré");
			return;
		}
		if (!HasActiveSoldier(spotter))
		{
			SRP_EnemyComponent.Journal("ENNEMI", "Sirène de " + town + " : pas de départ, le groupe qui a donné l'alerte est hors de combat");
			return;
		}
		RplComponent rpl = RplComponent.Cast(pole.FindComponent(RplComponent));
		if (!rpl)
		{
			if (!m_bRplMissing)
			{
				m_bRplMissing = true;
				Print("[SRP] Sirène : le poteau n'a pas de RplComponent, les joueurs ne peuvent pas l'entendre", LogLevel.WARNING);
			}
			return;
		}
		RplId id = rpl.Id();
		if (!id.IsValid())
			return;

		int now = System.GetTickCount();
		int durationMs = Math.ClampInt(m_iSeconds, 5, 600) * 1000;
		vector position = pole.GetOrigin() + Vector(0, m_fSoundHeight, 0);
		m_aTowns.Insert(town);
		m_aPoles.Insert(pole);
		m_aIds.Insert(id);
		m_aPositions.Insert(position);
		m_aEnds.Insert(now + durationMs);
		m_mLastStart.Set(town, now);

		Rpc(RpcDo_SRPSireneStart, id, position, durationMs);
		if (!System.IsConsoleApp())
			PlayLocal(id, position, durationMs);	// serveur hébergé ou Workbench : il a un joueur
		SRP_EnemyComponent.Journal("ENNEMI", "Sirène : " + town + " donne l'alerte");

		if (!m_bWatching)
		{
			m_bWatching = true;
			GetGame().GetCallqueue().CallLater(ServerWatch, 1000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le groupe a-t-il encore un soldat vivant et conscient ?
	protected static bool HasActiveSoldier(SCR_AIGroup group)
	{
		if (!group || group.IsDeleted())
			return false;
		array<AIAgent> agents = {};
		group.GetAgents(agents);
		foreach (AIAgent agent : agents)
		{
			if (!agent)
				continue;
			IEntity soldier = agent.GetControlledEntity();
			if (!soldier || soldier.IsDeleted() || SRP_Utils.IsDead(soldier))
				continue;
			SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(soldier.FindComponent(SCR_CharacterControllerComponent));
			if (controller && controller.IsUnconscious())
				continue;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque seconde tant qu'une sirène sonne : fin atteinte, poteau supprimé ou abîmé -> arrêt chez tous
	protected void ServerWatch()
	{
		int now = System.GetTickCount();
		for (int i = m_aTowns.Count() - 1; i >= 0; i--)
		{
			IEntity pole = m_aPoles[i];
			bool gone = !pole || pole.IsDeleted();
			bool broken = !gone && IsDamaged(pole);
			if (now < m_aEnds[i] && !gone && !broken)
				continue;
			if (broken)
				SRP_EnemyComponent.Journal("ENNEMI", "Sirène de " + m_aTowns[i] + " détruite");
			StopAt(i);
		}
		if (m_aTowns.IsEmpty())
		{
			m_bWatching = false;
			GetGame().GetCallqueue().Remove(ServerWatch);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Arrête la sirène d'indice « index » chez tous (et ici)
	protected void StopAt(int index)
	{
		RplId id = m_aIds[index];
		m_aTowns.Remove(index);
		m_aPoles.Remove(index);
		m_aIds.Remove(index);
		m_aPositions.Remove(index);
		m_aEnds.Remove(index);
		Rpc(RpcDo_SRPSireneStop, id);
		StopLocal(id);
	}

	//------------------------------------------------------------------------------------------------
	//! La sirène de cette localité se tait (et un départ en attente est annulé) : capture, retrait de la garnison
	void Silence(string town)
	{
		m_aPending.RemoveItem(town);
		int index = m_aTowns.Find(town);
		if (index >= 0)
			StopAt(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Staff (page « État des groupes ») : l'état des sirènes des localités à moins de « radius » (distance au point clé
	//! CENTRE) : prête, sonne, repos N min, détruite, absente ; une localité prise garde son poteau, muet. m_iOwner ==
	//! ENNEMI veut dire « hostile » (zone rouge, points clés pas tous saisis). Jamais montré aux joueurs.
	string GetAlertReport(vector from, float radius)
	{
		if (!m_bEnabled)
			return "Sirènes : désactivées (SRP_SireneComponent)";
		SRP_TerritoryComponent territory = SRP_TerritoryComponent.GetInstance();
		if (!territory)
			return "Sirènes : pas de territoire";
		array<SRP_SectorState> states = {};
		territory.GetLocalities(states);
		int now = System.GetTickCount();
		string text = "";
		foreach (SRP_SectorState state : states)
		{
			if (!state || !state.m_Sector)
				continue;
			bool ours = state.m_iOwner != SRP_ESectorOwner.ENNEMI;
			if (ours && (!state.m_Siren || state.m_Siren.IsDeleted()))
				continue;
			float distance = vector.Distance(from, state.m_Sector.GetCenter());
			if (distance > radius)
				continue;
			string status = PoleStatus(state.m_sName, state.m_Siren, now);
			if (ours)
				status += " (localité prise : muette)";
			string place = state.m_sName;
			if (!state.m_sZoneCode.IsEmpty())
				place = state.m_sName + " (" + state.m_sZoneCode + ")";
			if (!text.IsEmpty())
				text += "\n";
			text += string.Format("Sirène de %1 : %2 · %3 m", place, status, Math.Round(distance));
		}
		if (text.IsEmpty())
			return string.Format("Sirènes : aucune localité ennemie à moins de %1 m", Math.Round(radius));
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected string PoleStatus(string town, IEntity pole, int now)
	{
		if (m_aTowns.Contains(town))
			return "sonne";
		if (!pole || pole.IsDeleted())
			return "absente";
		if (IsDamaged(pole))
			return "détruite";
		if (m_aPending.Contains(town))
			return "départ imminent";
		int last;
		if (m_mLastStart.Find(town, last))
		{
			int rest = Math.ClampInt(m_iCooldownMinutes, 0, 1440) * 60 * 1000 - (now - last);
			if (rest > 0)
				return string.Format("repos %1 min", (rest + 59999) / 60000);
		}
		return "prête";
	}

	//------------------------------------------------------------------------------------------------
	// Joueurs : le son
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPSireneStart(RplId id, vector pos, int durationMs)
	{
		PlayLocal(id, pos, durationMs);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_SRPSireneStop(RplId id)
	{
		StopLocal(id);
	}

	//------------------------------------------------------------------------------------------------
	//! Sur cette machine : l'entité de son locale au-dessus du poteau, signal à 1, puis l'événement ; relancé chaque
	//! seconde s'il s'est arrêté, coupé à la fin (LocalTick)
	protected void PlayLocal(RplId id, vector pos, int durationMs)
	{
		if (System.IsConsoleApp() || m_bSoundBroken || durationMs <= 0)
			return;
		StopLocal(id);
		Resource res = Resource.Load(m_sSoundPrefab);
		if (!res || !res.IsValid())
		{
			m_bSoundBroken = true;
			Print("[SRP] Sirène : prefab de son introuvable " + m_sSoundPrefab, LogLevel.WARNING);
			return;
		}
		EntitySpawnParams params = new EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		IEntity sound = GetGame().SpawnEntityPrefabLocal(res, GetGame().GetWorld(), params);
		if (!sound)
			return;
		AudioHandle handle = StartSound(sound);
		m_aLocalIds.Insert(id);
		m_aLocalEntities.Insert(sound);
		m_aLocalHandles.Insert(handle);
		m_aLocalEnds.Insert(System.GetTickCount() + durationMs);
		if (!m_bLocalTicking)
		{
			m_bLocalTicking = true;
			GetGame().GetCallqueue().CallLater(LocalTick, 1000, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Signal « Siren » à 1 AVANT le son (le port 0 de l'événement y est relié), puis l'événement
	protected AudioHandle StartSound(IEntity sound)
	{
		SignalsManagerComponent signals = SignalsManagerComponent.Cast(sound.FindComponent(SignalsManagerComponent));
		if (signals)
			signals.SetSignalValue(signals.AddOrFindSignal(m_sSignalName), 1);
		SoundComponent soundComponent = SoundComponent.Cast(sound.FindComponent(SoundComponent));
		if (!soundComponent)
			return AudioHandle.Invalid;
		return soundComponent.SoundEvent(m_sSoundEvent);
	}

	//------------------------------------------------------------------------------------------------
	//! Chaque seconde sur cette machine : la fin est le SEUL arrêt garanti (la boucle du jeu est infinie) ; poteau vu
	//! abîmé ici -> arrêt ; son terminé alors qu'il reste du temps -> relancé
	protected void LocalTick()
	{
		int now = System.GetTickCount();
		for (int i = m_aLocalIds.Count() - 1; i >= 0; i--)
		{
			IEntity sound = m_aLocalEntities[i];
			bool over = !sound || now >= m_aLocalEnds[i];
			if (!over)
			{
				IEntity pole = FindPole(m_aLocalIds[i]);
				if (pole && IsDamaged(pole))
					over = true;	// poteau détruit : on n'attend pas l'ordre du serveur
			}
			if (over)
			{
				StopLocalAt(i);
				continue;
			}
			SoundComponent soundComponent = SoundComponent.Cast(sound.FindComponent(SoundComponent));
			if (soundComponent && soundComponent.IsFinishedPlaying(m_aLocalHandles[i]))
				m_aLocalHandles[i] = StartSound(sound);
		}
		if (m_aLocalIds.IsEmpty())
		{
			m_bLocalTicking = false;
			GetGame().GetCallqueue().Remove(LocalTick);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Le poteau sur cette machine (null s'il n'y est pas chargé : le son continue jusqu'à la fin ou l'ordre du serveur)
	protected static IEntity FindPole(RplId id)
	{
		if (!id.IsValid())
			return null;
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		if (!rpl)
			return null;
		return rpl.GetEntity();
	}

	//------------------------------------------------------------------------------------------------
	//! Arrête la sirène de ce poteau sur cette machine, et retire aussi celle reçue à la connexion qui n'a pas encore
	//! démarré (un arrivant ne doit pas l'entendre seul après l'ordre d'arrêt du serveur)
	protected void StopLocal(RplId id)
	{
		for (int pending = m_aPendingIds.Count() - 1; pending >= 0; pending--)
		{
			if (m_aPendingIds[pending] != id)
				continue;
			m_aPendingIds.Remove(pending);
			m_aPendingPositions.Remove(pending);
			m_aPendingMs.Remove(pending);
		}
		int index = m_aLocalIds.Find(id);
		if (index >= 0)
			StopLocalAt(index);
	}

	//------------------------------------------------------------------------------------------------
	//! Signal à 0, son coupé, entité locale supprimée
	protected void StopLocalAt(int index)
	{
		IEntity sound = m_aLocalEntities[index];
		AudioHandle handle = m_aLocalHandles[index];
		m_aLocalIds.Remove(index);
		m_aLocalEntities.Remove(index);
		m_aLocalHandles.Remove(index);
		m_aLocalEnds.Remove(index);
		if (sound)
		{
			SignalsManagerComponent signals = SignalsManagerComponent.Cast(sound.FindComponent(SignalsManagerComponent));
			if (signals)
				signals.SetSignalValue(signals.AddOrFindSignal(m_sSignalName), 0);
		}
		if (handle != AudioHandle.Invalid)
			AudioSystem.TerminateSound(handle);
		if (sound)
			delete sound;
	}

	//------------------------------------------------------------------------------------------------
	protected void StopAllLocal()
	{
		for (int i = m_aLocalIds.Count() - 1; i >= 0; i--)
			StopLocalAt(i);
		m_bLocalTicking = false;
	}

	//------------------------------------------------------------------------------------------------
	// Arrivée en cours de partie : le game mode transmet les sirènes qui sonnent (temps restant)
	//------------------------------------------------------------------------------------------------
	override bool RplSave(ScriptBitWriter writer)
	{
		int now = System.GetTickCount();
		int count = Math.ClampInt(m_aTowns.Count(), 0, MAX_SYNC);
		writer.WriteInt(count);
		for (int i = 0; i < count; i++)
		{
			int remaining = m_aEnds[i] - now;
			if (remaining < 0)
				remaining = 0;
			writer.WriteRplId(m_aIds[i]);
			writer.WriteVector(m_aPositions[i]);
			writer.WriteInt(remaining);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override bool RplLoad(ScriptBitReader reader)
	{
		int count;
		reader.ReadInt(count);
		if (count < 0 || count > MAX_SYNC)
			return false;
		RplId id;
		vector position;
		int remaining;
		for (int i = 0; i < count; i++)
		{
			reader.ReadRplId(id);
			reader.ReadVector(position);
			reader.ReadInt(remaining);
			m_aPendingIds.Insert(id);
			m_aPendingPositions.Insert(position);
			m_aPendingMs.Insert(remaining);
		}
		if (count > 0)
		{
			m_iPendingTick = System.GetTickCount();
			GetGame().GetCallqueue().CallLater(ApplyPending, 1500, false);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Arrivant : les sirènes reçues à la connexion (et pas arrêtées depuis) sonnent pour le temps qu'il leur reste,
	//! compté depuis la réception (le chargement peut durer plus que prévu)
	protected void ApplyPending()
	{
		// Copie d'abord : PlayLocal passe par StopLocal, qui retire les entrées en attente de ce poteau
		array<RplId> ids = {};
		array<vector> positions = {};
		array<int> remainings = {};
		for (int i = 0; i < m_aPendingIds.Count(); i++)
		{
			ids.Insert(m_aPendingIds[i]);
			positions.Insert(m_aPendingPositions[i]);
			remainings.Insert(m_aPendingMs[i]);
		}
		m_aPendingIds.Clear();
		m_aPendingPositions.Clear();
		m_aPendingMs.Clear();

		int elapsed = System.GetTickCount() - m_iPendingTick;
		for (int j = 0; j < ids.Count(); j++)
		{
			int remaining = remainings[j] - elapsed;
			if (remaining > 1000)
				PlayLocal(ids[j], positions[j], remaining);
		}
	}
}
