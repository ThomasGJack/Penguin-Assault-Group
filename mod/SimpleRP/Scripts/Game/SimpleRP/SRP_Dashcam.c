//------------------------------------------------------------------------------------------------
// SimpleRP — Caméras de casque (mod FOF Dashcam)
// Les écrans du mod ont une action « Etat-Major : libérer canal », réservée à qui porte un
// FOF_CommandAuthorityComponent coché sur son personnage : un réglage de prefab, le même pour tout le monde.
// Ici le droit suit la fiche du joueur : officier ou Staff (SRP_PlayerManagerComponent.IsOfficier).
// Le serveur tranche avec la fiche. Le client ne connaît pas son grade : il le demande au serveur quand
// il regarde un écran, au plus une fois toutes les 10 s, et garde la dernière réponse.
// Le reste de l'intégration est en prefabs : emplacement « Camera » sur les casques MSA TCNVG CCE et F3,
// réglages de Prefabs/Items/Equipment/CamHead/CamHead.et, écrans SRP_TV_32 et SRP_TV_Geante.
//------------------------------------------------------------------------------------------------

modded class SCR_PlayerController
{
	protected static const int SRP_DASHCAM_ASK_MS = 10000;

	protected bool m_bSRPDashcamAuthority;
	protected bool m_bSRPDashcamAsked;
	protected int m_iSRPDashcamAskedAt;

	//------------------------------------------------------------------------------------------------
	//! Côté client : dernière réponse du serveur, redemandée si elle a plus de 10 s
	bool SRP_HasDashcamAuthority()
	{
		int now = System.GetTickCount();
		if (!m_bSRPDashcamAsked || now - m_iSRPDashcamAskedAt > SRP_DASHCAM_ASK_MS)
		{
			m_bSRPDashcamAsked = true;
			m_iSRPDashcamAskedAt = now;
			Rpc(RpcAsk_SRPDashcamAuthority);
		}
		return m_bSRPDashcamAuthority;
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_SRPDashcamAuthority()
	{
		bool allowed = false;
		SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
		if (players)
			allowed = players.IsOfficier(GetPlayerId());
		Rpc(RpcDo_SRPDashcamAuthority, allowed);
	}

	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_SRPDashcamAuthority(bool allowed)
	{
		m_bSRPDashcamAuthority = allowed;
	}
}

//------------------------------------------------------------------------------------------------
modded class FOF_TVReceiverComponent
{
	//------------------------------------------------------------------------------------------------
	//! Qui peut libérer un canal : le réglage d'origine du mod, sinon les officiers et le Staff de la PAG
	override bool UserHasCommandAuthority(IEntity user)
	{
		if (super.UserHasCommandAuthority(user))
			return true;
		if (!user)
			return false;

		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(user);
		if (playerId <= 0)
			return false;

		if (Replication.IsServer())
		{
			SRP_PlayerManagerComponent players = SRP_PlayerManagerComponent.GetInstance();
			if (!players)
				return false;
			return players.IsOfficier(playerId);
		}

		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!controller || controller.GetPlayerId() != playerId)
			return false;
		return controller.SRP_HasDashcamAuthority();
	}
}
