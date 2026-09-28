//------------------------------------------------------------------------------------------------
// SimpleRP — Dépôts
// Un dépôt est un CONTENEUR de la base (soute, citerne, atelier, intendance) : un prefab ouvrable
// comme une caisse (SCR_UniversalInventoryStorageComponent + notre gestionnaire
// SRP_ContainerInventoryStorageManagerComponent), porteur de SRP_DepotComponent qui dit quelle
// ressource il stocke et sa capacité en paquets. Le stock, c'est ce qu'il y a dedans : les joueurs déposent et
// retirent des paquets par glisser-déposer, les scripts (SRP_ResourceManagerComponent) comptent,
// retirent et ajoutent. Un paquet de mission déposé ici fait avancer la mission.
// Action « Consulter le stock » : le panneau du dépôt.
// Phase 6 — étape A
//------------------------------------------------------------------------------------------------

[ComponentEditorProps(category: "SimpleRP", description: "Dépôt SimpleRP : conteneur d'une ressource de la base, capacité en paquets")]
class SRP_DepotComponentClass : ScriptComponentClass
{
}

class SRP_DepotComponent : ScriptComponent
{
	[Attribute("0", UIWidgets.ComboBox, "Ressource stockée dans ce dépôt", "", ParamEnumArray.FromEnum(SRP_EResource), category: "SimpleRP")]
	protected SRP_EResource m_eResource;

	[Attribute("300", UIWidgets.EditBox, "Capacité en paquets (la capacité physique du conteneur doit être au moins aussi grande)", category: "SimpleRP")]
	protected int m_iCapacity;

	protected static ref array<SRP_DepotComponent> s_aDepots = {};

	//------------------------------------------------------------------------------------------------
	static array<SRP_DepotComponent> GetDepots()
	{
		return s_aDepots;
	}

	//------------------------------------------------------------------------------------------------
	//! Le dépôt d'une entité, ou null
	static SRP_DepotComponent Of(IEntity entity)
	{
		if (!entity)
			return null;
		return SRP_DepotComponent.Cast(entity.FindComponent(SRP_DepotComponent));
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (Replication.IsServer() && !s_aDepots.Contains(this))
			s_aDepots.Insert(this);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		s_aDepots.RemoveItem(this);
		super.OnDelete(owner);
	}

	//------------------------------------------------------------------------------------------------
	int GetResource()
	{
		return m_eResource;
	}

	//------------------------------------------------------------------------------------------------
	int GetCapacity()
	{
		return m_iCapacity;
	}

	//------------------------------------------------------------------------------------------------
	string GetDepotName()
	{
		return SRP_Resources.GetDepotName(m_eResource);
	}

	//------------------------------------------------------------------------------------------------
	//! Texte du panneau : ce dépôt, puis le total de la ressource s'il y a plusieurs dépôts
	string GetStockText()
	{
		SRP_ResourceManagerComponent manager = SRP_ResourceManagerComponent.GetInstance();
		if (!manager)
			return "Gestionnaire de ressources absent du game mode";

		string text = string.Format("Ce dépôt : %1 / %2 %3", SRP_Packets.Count(GetOwner(), m_eResource), m_iCapacity, SRP_Resources.GetName(m_eResource));
		if (manager.GetCapacity(m_eResource) != m_iCapacity)
			text += "\n" + manager.GetStockText(m_eResource);
		return text + "\nOuvrir le conteneur pour déposer ou prendre des paquets";
	}
}

//------------------------------------------------------------------------------------------------
//! Gestionnaire d'inventaire des dépôts et des caisses de livraison : celui des conteneurs (véhicules,
//! caisses), pas celui des personnages, qui plante sur un prop. Sur un dépôt, un paquet de mission qui
//! entre fait avancer la mission.
[EntityEditorProps(category: "SimpleRP")]
class SRP_ContainerInventoryStorageManagerComponentClass : SCR_VehicleInventoryStorageManagerComponentClass
{
}

class SRP_ContainerInventoryStorageManagerComponent : SCR_VehicleInventoryStorageManagerComponent
{
	//------------------------------------------------------------------------------------------------
	override protected void OnItemAdded(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemAdded(storageOwner, item);
		SRP_MissionManagerComponent.NotifyItemAdded(GetOwner(), item);
		if (SRP_DepotComponent.Of(GetOwner()) && SRP_PacketComponent.Of(item))
			SRP_ResourceManagerComponent.MarkDirty();
	}

	//------------------------------------------------------------------------------------------------
	override protected void OnItemRemoved(BaseInventoryStorageComponent storageOwner, IEntity item)
	{
		super.OnItemRemoved(storageOwner, item);
		if (SRP_DepotComponent.Of(GetOwner()) && SRP_PacketComponent.Of(item))
			SRP_ResourceManagerComponent.MarkDirty();
	}
}

//------------------------------------------------------------------------------------------------
//! Action « Consulter le stock » : à ajouter dans Additional Actions de l'ActionsManagerComponent du prefab de dépôt
class SRP_DepotConsultAction : ScriptedUserAction
{
	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!Replication.IsServer())
			return;

		SRP_DepotComponent depot = SRP_DepotComponent.Of(pOwnerEntity);
		if (!depot)
			return;

		int playerId = SRP_Utils.GetPlayerIdFromEntity(pUserEntity);
		if (playerId < 0)
			return;

		SRP_Utils.HintPlayer(playerId, "Panneau — " + depot.GetDepotName(), depot.GetStockText());
	}

	//------------------------------------------------------------------------------------------------
	override bool GetActionNameScript(out string outName)
	{
		outName = "Consulter le stock";
		return true;
	}

	override bool CanBeShownScript(IEntity user) { return true; }
	override bool CanBePerformedScript(IEntity user) { return true; }
	override bool HasLocalEffectOnlyScript() { return false; }
}
