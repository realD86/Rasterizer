#pragma once
#include "..\Data\CoolD_Type.h"
#include "..\Data\CoolD_Struct.h"
#include "..\Data\CoolD_Singleton.h"
#include "..\Math\CoolD_Matrix44.h"
#include <windows.h>

namespace CoolD
{
	class Camera : public CSingleton<Camera>
	{
		friend class CSingleton<Camera>;
	private:
		Camera() = default;
		Camera(const Camera&) = delete;
		Camera& operator=(const Camera&) = delete;

	public:
		Dvoid CreateCamera(Vector3 vPos, Vector3 vLook, Dfloat fAngle, Dfloat aspect);				
		Dvoid UpdateCamera(Dint x, Dint y, Dint clickState, MoveKey moveKey, Matrix44& viewMatrix, Matrix44& perspectiveMatrix);
		
	private:
		Dvoid DirectionVectorInit();
		Dvoid ViewMatrixCalculate();
		Dvoid ProjectionCalculate();
		Dvoid Pitch();
		Dvoid Yaw();

	//카메라 관련
	private:
		Vector3		m_vPos;
		Vector3		m_vLook;
		Vector3		m_vDir[CD_MAX];
		Matrix44	m_matCam[CM_MAX];
		Dfloat		m_fAngle;		
		Dfloat		m_fNear;
		Dfloat		m_fFar;
		Dfloat		m_fAspect;

	//이동 관련
	private:
		Dfloat		m_fMoveSpeed;
		Dfloat		m_fMouseSens;
		Dbool		m_bMove;
		Point		m_prevMousePos;
		ClickStatus m_prevMouseStatus;
		Point		m_MouseMoveInterval;	
		Vector3		m_vMoveDir;

	//시간과 이동 입력처리......나중에 분리...
	private:
		Dvoid TimeDeltaInit();
		Dvoid UpdateTimeDelta();
		Dvoid InputHandle(Dint x, Dint y, Dint clickState, MoveKey moveKey);
		Dvoid MouseInputHandle(Dint x, Dint y, Dint clickState);
		Dvoid KeyboardInputHandle(MoveKey moveKey);

	//TimeDelta 관련
	private:
		LARGE_INTEGER		m_CPUFrequency;
		LARGE_INTEGER		m_CurrentCounter;
		LARGE_INTEGER		m_PrevCPUCounter;
		LARGE_INTEGER		m_PrevTimeCounter;
		float				m_fTimeDelta;
	};
//───────────────────────────────────────────────────────────────────────────────────────────────────────────────
}