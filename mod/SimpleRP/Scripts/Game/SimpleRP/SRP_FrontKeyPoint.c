//------------------------------------------------------------------------------------------------
// SimpleRP — Ligne de front : le repère « point clé » posé par Jack au World Editor (D3, D6, arbitrage Q8).
//
// RÔLE : composant des deux prefabs repères Prefabs/Props/SRP_PointCle_Centre.et et SRP_PointCle_QG.et (drapeau
// visible au Workbench). Au lancement du jeu, chaque repère note sa définition (côté serveur) puis se supprime sur
// chaque machine : ni drapeau visible, ni collision en jeu. Remplace SRP_SectorComponent (SRP_Territory.c:61-156),
// qui n'a aucun exemplaire posé.
// Priorité des sources (arbitrage Q8, SRP_FrontGeometryBuilder.PlaceKeyPoints) : CE REPÈRE d'abord, puis, pour le
// CENTRE seulement, l'endroit du nom sur la carte (repli, listé comme manquant au menu Staff) ; un QG sans repère
// n'existe pas (ville à un seul point clé, listée manquante). Il n'y a pas d'autre source : une ligne « point » de
// front_retouches.txt est refusée par le bâtisseur (ReadRetouches, noté au rapport front_zones.txt).
// Aucun outil Staff « Point clé ICI » ni fichier points_cles.json : chaque retouche demande de republier (Q8).
// APPELÉ PAR : le moteur (OnPostInit) ; SRP_FrontGeometryBuilder lit GetDefs() 5 s après le démarrage ; le socle
// (SRP_FrontComponent.Start) vide la liste une fois la géométrie construite (ClearDefs).
// L'énumération SRP_EKeyPointRole est déclarée dans SRP_Front.c (seul jeu d'énumérations du front).
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
//! Définition brute d'un repère posé (serveur), lue par le bâtisseur de géométrie
class SRP_KeyPointDef
{
	string m_sLocality;							// nom exact de la localité, vide = la plus proche à 600 m au plus
	int m_iRole = SRP_EKeyPointRole.CENTRE;
	vector m_vPos;								// origine du repère
	float m_fYaw;								// orientation du repère (le poste de commandement la reprend)
	string m_sSource;							// « repère <nom de l'entité> »
}

//------------------------------------------------------------------------------------------------
[ComponentEditorProps(category: "SimpleRP", description: "Point clé du front : centre ou QG d'une localité (repère posé au Workbench)")]
class SRP_KeyPointComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
class SRP_KeyPointComponent : ScriptComponent
{
	[Attribute("", UIWidgets.EditBox, "Localité : nom exact sur la carte (ex. Montignac). Vide = la localité dont le nom est le plus proche, à 600 m au plus", category: "SimpleRP")]
	string m_sLocality;

	[Attribute("0", UIWidgets.ComboBox, "Rôle : centre (toutes les localités) ou QG (villes et bourgs)", "", ParamEnumArray.FromEnum(SRP_EKeyPointRole), category: "SimpleRP")]
	SRP_EKeyPointRole m_eRole;

	protected static ref array<ref SRP_KeyPointDef> s_aDefs = {};

	//------------------------------------------------------------------------------------------------
	//! Les repères posés dans le monde (serveur), dans l'ordre d'initialisation — SRP_FrontGeometryBuilder.PlaceKeyPoints
	static array<ref SRP_KeyPointDef> GetDefs()
	{
		return s_aDefs;
	}

	//------------------------------------------------------------------------------------------------
	//! Vide la liste une fois la géométrie construite : le Workbench garde les statiques d'une partie à l'autre, la
	//! partie suivante repart ainsi d'une liste propre — SRP_FrontComponent.Start (après SRP_FrontGeometryBuilder.Build)
	static void ClearDefs()
	{
		s_aDefs.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! super ; rien au World Editor (SCR_Global.IsEditMode(owner)) ; serveur : un SRP_KeyPointDef (m_sLocality.Trim(),
	//! m_eRole, origine, GetYawPitchRoll()[0], nom de l'entité) dans s_aDefs ; toutes machines :
	//! CallLater(RemoveMarker, 0) — le moteur
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (!owner || SCR_Global.IsEditMode(owner))
			return;	// au World Editor, le repère reste visible pour être placé

		// La définition est notée juste avant la suppression (image suivante) : l'origine et l'orientation de l'entité
		// sont alors sûrement en place, et le front ne lit la liste que 5 s après le démarrage
		GetGame().GetCallqueue().CallLater(RemoveMarker, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Supprime le repère (SCR_EntityHelper.DeleteEntityAndChildren) ; repli si la suppression d'une entité de calque
	//! gêne : ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE) — OnPostInit (CallLater)
	protected void RemoveMarker()
	{
		IEntity owner = GetOwner();
		if (!owner || owner.IsDeleted())
			return;

		if (Replication.IsServer())
			NoteDef(owner);

		// Invisible et sans collision tout de suite, même si la suppression attend l'autorité (entité répliquée)
		owner.ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE, true);
		SCR_EntityHelper.DeleteEntityAndChildren(owner);
	}

	//------------------------------------------------------------------------------------------------
	//! Serveur : la définition du repère (localité, rôle, position, orientation, nom) ajoutée à s_aDefs ; un repère déjà
	//! noté à la même place sous le même nom (partie précédente du Workbench) est remplacé — RemoveMarker
	protected void NoteDef(IEntity owner)
	{
		SRP_KeyPointDef def = new SRP_KeyPointDef();
		def.m_sLocality = m_sLocality.Trim();
		def.m_iRole = m_eRole;
		def.m_vPos = owner.GetOrigin();
		vector angles = owner.GetYawPitchRoll();
		def.m_fYaw = angles[0];

		string entityName = owner.GetName();
		if (entityName.IsEmpty())
			entityName = string.Format("sans nom (%1 / %2)", Math.Round(def.m_vPos[0]), Math.Round(def.m_vPos[2]));
		def.m_sSource = "repère " + entityName;

		for (int i = s_aDefs.Count() - 1; i >= 0; i--)
		{
			SRP_KeyPointDef old = s_aDefs[i];
			if (!old)
			{
				s_aDefs.RemoveOrdered(i);
				continue;
			}
			// Même repère (le nom d'entité est unique dans un monde ; sans nom, m_sSource porte la position arrondie) :
			// la définition d'une partie précédente du Workbench est remplacée, même si le repère a été déplacé
			if (old.m_sSource == def.m_sSource)
				s_aDefs.RemoveOrdered(i);
		}
		s_aDefs.Insert(def);
	}
}
