//------------------------------------------------------------------------------------------------
// SimpleRP — Catalogue d'achat (fichier de config SRP_Catalogue.conf, édité dans le Workbench)
// - Véhicules : nom, prefab, catégorie de conduite, capacité, prix en euros, nombre au premier
//   lancement (0 = seulement à l'achat)
// - Ravitaillement : une caisse de paquets d'une ressource (nom, ressource, paquets, prix), ou un objet
//   quelconque (prefab, quantité, prix)
// Le terminal logistique, /acheter et la flotte initiale lisent ce fichier.
// Phase 3 — étape C
//------------------------------------------------------------------------------------------------

[BaseContainerProps(), BaseContainerCustomTitleField("m_sName")]
class SRP_CatalogueCrate
{
	[Attribute("Caisse de munitions", UIWidgets.EditBox, "Nom affiché")]
	string m_sName;

	[Attribute("1", UIWidgets.CheckBox, "Ressource : livrée en caisse de paquets (sinon, un objet quelconque via le prefab)")]
	bool m_bResource;

	[Attribute("0", UIWidgets.ComboBox, "Ressource livrée", "", ParamEnumArray.FromEnum(SRP_EResource))]
	SRP_EResource m_eResource;

	[Attribute("20", UIWidgets.EditBox, "Quantité proposée par défaut au terminal, et livrée par /acheter caisse <n> (si ressource)")]
	int m_iPackets;

	[Attribute("15", UIWidgets.EditBox, "Prix d'un paquet en euros (si ressource) : le terminal propose la quantité et calcule le total")]
	int m_iPricePerPacket;

	[Attribute("10", UIWidgets.EditBox, "Quantité minimale par commande (si ressource)")]
	int m_iMinPackets;

	[Attribute("200", UIWidgets.EditBox, "Quantité maximale par commande (si ressource)")]
	int m_iMaxPackets;

	[Attribute("10", UIWidgets.EditBox, "Pas du réglage de quantité au terminal (si ressource)")]
	int m_iStepPackets;

	[Attribute("", UIWidgets.ResourceNamePicker, "Objet livré si ce n'est pas une ressource (jerrican, caisse médicale…)", "et")]
	ResourceName m_sPrefab;

	[Attribute("1", UIWidgets.EditBox, "Nombre d'exemplaires de l'objet livrés par achat")]
	int m_iQuantity;

	[Attribute("300", UIWidgets.EditBox, "Prix en euros d'un objet (ignoré pour une ressource : voir le prix par paquet)")]
	int m_iPrice;

	//------------------------------------------------------------------------------------------------
	bool IsUsable()
	{
		if (m_bResource)
			return m_iPricePerPacket > 0 && MaxPackets() >= MinPackets();
		return !m_sPrefab.IsEmpty();
	}

	//------------------------------------------------------------------------------------------------
	int MinPackets()
	{
		if (m_iMinPackets < 1)
			return 1;
		return m_iMinPackets;
	}

	//------------------------------------------------------------------------------------------------
	int MaxPackets()
	{
		if (m_iMaxPackets < MinPackets())
			return MinPackets();
		return m_iMaxPackets;
	}

	//------------------------------------------------------------------------------------------------
	int StepPackets()
	{
		if (m_iStepPackets < 1)
			return 1;
		return m_iStepPackets;
	}

	//------------------------------------------------------------------------------------------------
	//! Quantité ramenée entre le minimum et le maximum
	int Clamp(int packets)
	{
		if (packets < MinPackets())
			return MinPackets();
		if (packets > MaxPackets())
			return MaxPackets();
		return packets;
	}

	//------------------------------------------------------------------------------------------------
	//! Nom affiché : pour une ressource, toujours « Paquets de … »
	string DisplayName()
	{
		if (m_bResource)
			return "Paquets de " + SRP_Resources.GetName(m_eResource);
		return m_sName;
	}

	//------------------------------------------------------------------------------------------------
	string Label()
	{
		if (m_bResource)
			return string.Format("%1 — %2 € le paquet (de %3 à %4)", DisplayName(), m_iPricePerPacket, MinPackets(), MaxPackets());
		if (m_iQuantity > 1)
			return string.Format("%1 (x%2)", m_sName, m_iQuantity);
		return m_sName;
	}
}

//------------------------------------------------------------------------------------------------
[BaseContainerProps(configRoot: true)]
class SRP_Catalogue
{
	[Attribute("", UIWidgets.Object, "Véhicules en vente au terminal (et flotte initiale via « nombre au premier lancement »)")]
	ref array<ref SRP_FleetEntry> m_aVehicules;

	[Attribute("", UIWidgets.Object, "Caisses et objets de ravitaillement en vente au terminal")]
	ref array<ref SRP_CatalogueCrate> m_aCaisses;

	//------------------------------------------------------------------------------------------------
	static SRP_Catalogue Load(ResourceName path)
	{
		if (path.IsEmpty())
			return null;

		Resource res = Resource.Load(path);
		if (!res || !res.IsValid())
		{
			Print("[SRP] Catalogue introuvable : " + path, LogLevel.ERROR);
			return null;
		}

		BaseContainer container = res.GetResource().ToBaseContainer();
		if (!container)
			return null;

		SRP_Catalogue catalogue = SRP_Catalogue.Cast(BaseContainerTools.CreateInstanceFromContainer(container));
		if (!catalogue)
			Print("[SRP] Le fichier n'est pas un SRP_Catalogue : " + path, LogLevel.ERROR);
		return catalogue;
	}

	//------------------------------------------------------------------------------------------------
	int GetVehicleCount()
	{
		if (!m_aVehicules)
			return 0;
		return m_aVehicules.Count();
	}

	//------------------------------------------------------------------------------------------------
	int GetCrateCount()
	{
		if (!m_aCaisses)
			return 0;
		return m_aCaisses.Count();
	}
}
