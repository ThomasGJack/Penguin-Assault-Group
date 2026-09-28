//------------------------------------------------------------------------------------------------
// SimpleRP — Caméra de mort
// Entité caméra créée côté client à la mort du joueur : elle tourne lentement autour du corps
// pendant l'attente du respawn, puis est supprimée quand le nouveau personnage apparaît.
// Pilotée par SCR_PlayerController (SRP_StartDeathCamera / SRP_StopDeathCamera).
// Phase 2 — étape E2
//------------------------------------------------------------------------------------------------

class SRP_DeathCameraClass : CameraBaseClass
{
}

class SRP_DeathCamera : CameraBase
{
	protected vector m_vTarget;			// point regardé (le corps)
	protected float m_fAngle = 0;			// angle courant de l'orbite, en degrés
	protected float m_fRadius = 3.5;		// distance au corps
	protected float m_fHeight = 1.8;		// hauteur de la caméra au-dessus du corps
	protected float m_fSpeed = 12;			// degrés par seconde

	//------------------------------------------------------------------------------------------------
	void Setup(vector target)
	{
		m_vTarget = target;
		m_fAngle = Math.RandomFloat(0, 360);
		SetEventMask(EntityEvent.FRAME);
		UpdateTransform();
	}

	//------------------------------------------------------------------------------------------------
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		m_fAngle += m_fSpeed * timeSlice;
		if (m_fAngle >= 360)
			m_fAngle -= 360;

		UpdateTransform();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateTransform()
	{
		float rad = m_fAngle * Math.DEG2RAD;
		vector position = m_vTarget + Vector(Math.Cos(rad) * m_fRadius, m_fHeight, Math.Sin(rad) * m_fRadius);

		// Ne pas passer sous le sol
		float ground = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);
		if (position[1] < ground + 0.5)
			position[1] = ground + 0.5;

		vector lookAt = m_vTarget + Vector(0, 0.4, 0);
		vector direction = (lookAt - position).Normalized();

		vector mat[4];
		Math3D.DirectionAndUpMatrix(direction, vector.Up, mat);
		mat[3] = position;
		SetTransform(mat);
	}
}
