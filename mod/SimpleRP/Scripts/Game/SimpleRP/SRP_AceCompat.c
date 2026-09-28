//------------------------------------------------------------------------------------------------
// SimpleRP — Compatibilité avec ACE Dev
// ACE Overheating pose son composant de surchauffe sur toute « arme » qui hérite de Rifle_Base. À l'initialisation
// il calcule l'échauffement d'après la munition par défaut, la masse de l'arme et la longueur du canon (distance
// entre deux os du modèle). L'appareil anti-drone de RHS (Device_SpectrumDevice, porté par des soldats ennemis)
// hérite de Rifle_Base mais n'a ni munition ni canon : ACE lève alors ses Debug.Error l'un après l'autre
// (« has no default bullets », puis « Could not determine barrel length »…). Dans le Workbench chacun est une
// exception qui interrompt l'initialisation ; en jeu, une ligne d'erreur à chaque ennemi équipé.
//
// Ce qu'on ne fait PAS : désactiver le composant. Son composant de fumée l'utilise sans vérifier qu'il existe,
// et plante (essayé le 20 sept. 2026). Faire taire une seule alerte ne suffit pas non plus : la suivante prend
// le relais (essayé aussi).
//
// Ce qu'on fait : pour une arme sans munition par défaut, on remplit directement les grandeurs avec les valeurs
// de repli qu'ACE prévoit lui-même, sans passer par ses calculs. Toute arme normale suit le chemin d'origine.
//------------------------------------------------------------------------------------------------

modded class ACE_Overheating_BarrelComponentClass
{
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity instanceOwner)
	{
		if (m_bInitDone)
			return;

		MuzzleComponent muzzle = MuzzleComponent.Cast(instanceOwner.FindComponent(MuzzleComponent));
		if (muzzle)
		{
			array<ResourceName> bullets = ACE_BulletTools.GetDefaultResourceNamesFromMuzzle(muzzle);
			if (!bullets || bullets.IsEmpty())
			{
				m_fBulletMass = FALLBACK_BULLET_MASS;
				m_fHeatPerShot = m_fHeatingScale * 0.5 * FALLBACK_BULLET_MASS * Math.Pow(FALLBACK_INITIAL_BULLET_SPEED, 2);
				m_fBarrelHeatCapacity = m_fBarrelSpecificHeatCapacity * FALLBACK_BARREL_MASS * 1000;
				m_fBarrelSurfaceArea = Math.PI * m_fBarrelDiameter * FALLBACK_BARREL_LENGTH;
				m_bInitDone = true;
				return;
			}
		}

		super.Init(instanceOwner);
	}
}

//------------------------------------------------------------------------------------------------
// Arme « hors service » (carte #57) : une arme en main peut perdre son mode de tir côté client (muzzle sans
// BaseFireMode). ACE Overheating, à l'appui de la touche « désenrayer », construit alors une machine d'états
// avec un barrel nul et plante dans ACE_Overheating_RackBoltState (m_pBarrel.IsJammed()). Ici, sans barrel,
// on prévient le joueur local et on n'appelle pas ACE. Une arme normale suit le chemin d'origine.
//------------------------------------------------------------------------------------------------
modded class SCR_CharacterControllerComponent
{
	//------------------------------------------------------------------------------------------------
	override void ACE_Overheating_TryClearJam(notnull BaseWeaponComponent weapon, BaseMuzzleComponent muzzle = null)
	{
		ACE_Overheating_BarrelComponent barrel;
		if (muzzle)
			barrel = ACE_Overheating_BarrelComponent.FromMuzzle(muzzle);
		else
			barrel = ACE_Overheating_BarrelComponent.FromWeapon(weapon);

		if (!barrel)
		{
			// Exécuté chez le joueur local (écouteur d'entrée) : pas de RPC, un message direct
			if (GetOwner() == SCR_PlayerController.GetLocalControlledEntity())
			{
				SRP_ChatCommands.ShowLocal("Arme hors service : changez d'arme puis reprenez-la (le Staff peut la recréer)");
				SCR_HintManagerComponent.ShowCustomHint("Cette arme n'a plus de canon reconnu : changez d'arme puis reprenez-la. Ne posez pas son chargeur au sol. Le Staff peut recréer l'arme.", "Arme hors service", 8.0);
			}
			Print("[SRP] Désenrayage refusé : arme sans barrel ACE (mode de tir perdu ?)", LogLevel.WARNING);
			return;
		}

		super.ACE_Overheating_TryClearJam(weapon, muzzle);
	}
}
