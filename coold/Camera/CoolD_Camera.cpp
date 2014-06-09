#include "CoolD_Camera.h"
#include "..\Math\CoolD_Matrix33.h"
#include "..\Math\CoolD_Vector4.h"

namespace CoolD
{
	Dvoid Camera::CreateCamera(Vector3 vPos, Vector3 vLook, Dfloat fAngle, Dfloat aspect)
	{
		m_vPos = vPos;
		m_vLook = vLook;
		m_fAngle = fAngle;		
		m_fNear = 1.0f;
		m_fFar = 30.0f;
		m_fAspect = aspect;
		m_bMove = false;

		m_fMoveSpeed = 40.f;
		m_fMouseSens = 0.3f;	

		m_prevMousePos = { -1, -1 };
		m_prevMouseStatus = ClickStatus::Release;
		m_vMoveDir = { 0, 0, 0 };		
		m_MouseMoveInterval = { 0, 0 };

		m_matCam[CM_VIEW].Identity();
		m_matCam[CM_PROJ].Identity();
		m_vDir[CD_FRONT].Clean();
		m_vDir[CD_RIGHT].Clean();
		m_vDir[CD_UP] = Vector3(0, 1, 0);

		DirectionVectorInit();
		ViewMatrixCalculate();
		ProjectionCalculate();

		TimeDeltaInit();
	}

	Dvoid Camera::DirectionVectorInit()
	{
		m_vDir[CD_FRONT] = m_vLook - m_vPos;
		m_vDir[CD_FRONT].Normalize();

		m_vDir[CD_RIGHT] = Cross(m_vDir[CD_FRONT], m_vDir[CD_UP]);
		m_vDir[CD_RIGHT].Normalize();

		m_vDir[CD_UP] = Cross(m_vDir[CD_RIGHT], m_vDir[CD_FRONT]);
		m_vDir[CD_UP].Normalize();
	}

	Dvoid Camera::ViewMatrixCalculate()
	{	
		Matrix33 rotate;
		rotate.SetColumns(m_vDir[CD_RIGHT], m_vDir[CD_UP], -m_vDir[CD_FRONT]);
				
		rotate.Transpose();
		m_matCam[CM_VIEW].Rotation(rotate);

		// set translation (rotate into view space)
		Vector3 invEye = -(rotate* m_vPos);
		m_matCam[CM_VIEW](0, 3) = invEye.x;
		m_matCam[CM_VIEW](1, 3) = invEye.y;
		m_matCam[CM_VIEW](2, 3) = invEye.z;		
	}

	Dvoid Camera::ProjectionCalculate()
	{
		Dfloat d = 1.0f / Tan( m_fAngle );
		Dfloat recip = 1.0f / ( 1.0f - m_fFar);
		
		m_matCam[CM_PROJ](0, 0) = d / m_fAspect;
		m_matCam[CM_PROJ](1, 1) = d;
		m_matCam[CM_PROJ](2, 2) = (m_fNear + m_fFar)*recip;
		m_matCam[CM_PROJ](2, 3) = 2 * m_fNear*m_fFar*recip;
		m_matCam[CM_PROJ](3, 2) = -1.0f;
		m_matCam[CM_PROJ](3, 3) = 0.0f;		
	}

	Dvoid Camera::UpdateCamera(Dint x, Dint y, Dint clickState, MoveKey moveKey, Matrix44& viewMatrix, Matrix44& perspectiveMatrix)
	{		
		UpdateTimeDelta();
		m_bMove = false;

		//───────────────────────────────────────────────────────────────────────────────────
		//키 입력 처리
		InputHandle(x, y, clickState, moveKey);
		//───────────────────────────────────────────────────────────────────────────────────

		m_vPos += m_vMoveDir;
		m_vLook += m_vMoveDir;

		Pitch();
		Yaw();

		if (m_bMove)
		{
			ViewMatrixCalculate();
		}

		viewMatrix = m_matCam[CM_VIEW];
		perspectiveMatrix = m_matCam[CM_PROJ];

		m_MouseMoveInterval = { 0, 0 };
	}

	Dvoid Camera::Pitch()
	{
		if ( m_MouseMoveInterval.y == 0)
			return;

		m_bMove = true;

		Matrix44	matRot;
		float fAngle = m_MouseMoveInterval.y * m_fMouseSens * m_fTimeDelta;		
		
		matRot.Rotation( m_vDir[CD_RIGHT], fAngle );

		// Right축을 제외한 축을 변환한다.
		for ( Duint i = 0; i < CD_MAX; ++i )
		{
			if (i == CD_RIGHT)	continue;

			m_vDir[i] = Vec4ToVec3(matRot * Vector4(m_vDir[i], 0), Vector4::W_IGNORE);
			m_vDir[i].Normalize();
		}
	}

	Dvoid Camera::Yaw()
	{
		if (m_MouseMoveInterval.x == 0)
			return;

		m_bMove = true;
		
		Matrix44	matRot;
		Vector3	vUp(0, 1, 0);
		float	fAngle = m_MouseMoveInterval.x * m_fMouseSens * m_fTimeDelta;
				
		matRot.Rotation(vUp, fAngle);
				
		for (Duint i = 0; i < CD_MAX; ++i)
		{			
			m_vDir[i] = Vec4ToVec3(matRot * Vector4(m_vDir[i], 0), Vector4::W_IGNORE);
			m_vDir[i].Normalize();
		}
	}

	Dvoid Camera::TimeDeltaInit()
	{
		QueryPerformanceFrequency(&m_CPUFrequency);
		QueryPerformanceCounter(&m_CurrentCounter);
		QueryPerformanceCounter(&m_PrevCPUCounter);
		QueryPerformanceCounter(&m_PrevTimeCounter);
	}

	Dvoid Camera::UpdateTimeDelta()
	{
		QueryPerformanceCounter(&m_CurrentCounter);

		if (m_CurrentCounter.QuadPart - m_PrevCPUCounter.QuadPart >= m_CPUFrequency.QuadPart)
		{
			QueryPerformanceFrequency(&m_CPUFrequency);
			m_PrevCPUCounter = m_CurrentCounter;
		}

		m_fTimeDelta = (m_CurrentCounter.QuadPart - m_PrevTimeCounter.QuadPart) / static_cast<float>(m_CPUFrequency.QuadPart);
		m_PrevTimeCounter = m_CurrentCounter;
	}

	Dvoid Camera::InputHandle(Dint x, Dint y, Dint clickState, MoveKey moveKey)
	{
		MouseInputHandle(x, y, clickState);
		KeyboardInputHandle(moveKey);
	}

	Dvoid Camera::MouseInputHandle(Dint x, Dint y, Dint clickState)
	{
		if (clickState == ClickStatus::Click && m_prevMouseStatus != ClickStatus::Click && m_prevMouseStatus != ClickStatus::Motion)
		{	//처음 클릭
			m_prevMousePos = { x, y };			
			m_prevMouseStatus = ClickStatus::Click;
		}
		else if (clickState == ClickStatus::Click && (m_prevMouseStatus == ClickStatus::Click || m_prevMouseStatus == ClickStatus::Motion))
		{	//클릭 후 누른 상태
			m_MouseMoveInterval.x = m_prevMousePos.x - x;
			m_MouseMoveInterval.y = m_prevMousePos.y - y;
			m_prevMousePos = { x, y };

			m_prevMouseStatus = ClickStatus::Motion;
		}
		else //Release 상태
		{			
			m_prevMouseStatus = ClickStatus::Release;
		}
	}

	Dvoid Camera::KeyboardInputHandle(MoveKey moveKey)
	{
		m_vMoveDir = { 0, 0, 0 };
		Dfloat fSpeed = m_fMoveSpeed * m_fTimeDelta;

		if (moveKey.front)
			m_vMoveDir += m_vDir[CD_FRONT] * fSpeed;

		if (moveKey.back)
			m_vMoveDir -= m_vDir[CD_FRONT] * fSpeed;

		if (moveKey.left)
			m_vMoveDir -= m_vDir[CD_RIGHT] * fSpeed;

		if (moveKey.right)
			m_vMoveDir += m_vDir[CD_RIGHT] * fSpeed;

		if (moveKey.up)
			m_vMoveDir += m_vDir[CD_UP] * fSpeed;

		if (moveKey.down)
			m_vMoveDir -= m_vDir[CD_UP] * fSpeed;

		if (m_vMoveDir.x != 0 || m_vMoveDir.y != 0 || m_vMoveDir.z != 0)
			m_bMove = true;
	}

}
