//------------------------------------------------------------------------------------------------
// SimpleRP — Bouchons d'oreilles
// Touche « SRP_Earplugs » (F8 par défaut, réglable dans Contrôles > PAG) : −30 %, −60 %, −90 %, retirés.
// Le moteur règle le volume par canal (AudioSystem.SetMasterVolume, de 0 à 1). On baisse l'environnement
// (SFX : tirs, moteurs, explosions, ambiance), les voix des personnages IA (Dialog) et la musique.
// La voix des joueurs et la radio ont leur propre canal (VoiceChat) : on n'y touche pas. L'interface non plus.
// Le jeu remet lui-même ces canaux à 1 à certains moments (écran de déploiement, chargement) : tant que les
// bouchons sont en place, on réapplique donc le niveau toutes les deux secondes.
// Purement local : rien ne passe par le serveur, chaque joueur règle ses propres oreilles.
//------------------------------------------------------------------------------------------------

modded class SCR_VONController
{
	static const string SRP_ACTION_EARPLUGS = "SRP_Earplugs";
	protected static const int SRP_EARPLUGS_REAPPLY_MS = 2000;

	protected int m_iSRPEarplugs = 0;				// 0 = retirés, 1 à 3 = niveau
	protected bool m_bSRPEarplugsListening;
	protected ref array<float> m_aSRPEarplugsVolume = {1.0, 0.7, 0.4, 0.1};
	protected ref array<string> m_aSRPEarplugsLabel = {"retirés", "niveau 1 (-30 %)", "niveau 2 (-60 %)", "niveau 3 (-90 %)"};

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		if (System.IsConsoleApp())
			return;	// serveur dédié : ni clavier ni son
		// Le contrôleur local n'est pas forcément désigné à cet instant : on réessaie chaque seconde
		GetGame().GetCallqueue().CallLater(SRP_EnsureEarplugsListener, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_EnsureEarplugsListener()
	{
		if (m_bSRPEarplugsListening)
		{
			GetGame().GetCallqueue().Remove(SRP_EnsureEarplugsListener);
			return;
		}
		if (GetGame().GetPlayerController() != GetOwner())
			return;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		input.AddActionListener(SRP_ACTION_EARPLUGS, EActionTrigger.DOWN, SRP_CycleEarplugs);
		m_bSRPEarplugsListening = true;
		GetGame().GetCallqueue().Remove(SRP_EnsureEarplugsListener);
	}

	//------------------------------------------------------------------------------------------------
	//! Touche : niveau suivant, puis retour à « retirés »
	protected void SRP_CycleEarplugs(float value, EActionTrigger reason)
	{
		m_iSRPEarplugs = m_iSRPEarplugs + 1;
		if (m_iSRPEarplugs >= m_aSRPEarplugsVolume.Count())
			m_iSRPEarplugs = 0;

		SRP_ApplyEarplugs();

		GetGame().GetCallqueue().Remove(SRP_ApplyEarplugs);
		if (m_iSRPEarplugs > 0)
			GetGame().GetCallqueue().CallLater(SRP_ApplyEarplugs, SRP_EARPLUGS_REAPPLY_MS, true);

		SCR_HintManagerComponent.ShowCustomHint("Bouchons d'oreilles : " + m_aSRPEarplugsLabel[m_iSRPEarplugs], "Bouchons d'oreilles", 3);
	}

	//------------------------------------------------------------------------------------------------
	protected void SRP_ApplyEarplugs()
	{
		float volume = m_aSRPEarplugsVolume[m_iSRPEarplugs];
		AudioSystem.SetMasterVolume(AudioSystem.SFX, volume);
		AudioSystem.SetMasterVolume(AudioSystem.Dialog, volume);
		AudioSystem.SetMasterVolume(AudioSystem.Music, volume);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		GetGame().GetCallqueue().Remove(SRP_EnsureEarplugsListener);
		GetGame().GetCallqueue().Remove(SRP_ApplyEarplugs);
		if (m_bSRPEarplugsListening)
		{
			InputManager input = GetGame().GetInputManager();
			if (input)
				input.RemoveActionListener(SRP_ACTION_EARPLUGS, EActionTrigger.DOWN, SRP_CycleEarplugs);
			m_bSRPEarplugsListening = false;

			// On ne laisse pas le son baissé derrière soi (retour au menu, changement de serveur)
			if (m_iSRPEarplugs > 0)
			{
				m_iSRPEarplugs = 0;
				SRP_ApplyEarplugs();
			}
		}
		super.OnDelete(owner);
	}
}
