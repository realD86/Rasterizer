#define DLLEXPORTS
//#include "Windows.h"
//#include <memory>
#include "coold.h"
#include "Data\CoolD_Defines.h"
#include "Data\CoolD_Inlines.h"
#include "Render\CoolD_RenderModule.h"
#include "Render\CoolD_Transform.h"
#include "Render\CoolD_MeshManager.h"
#include "Render\CoolD_CustomMesh.h"
#include "Math\CoolD_Matrix33.h"
#include "Math\CoolD_Matrix44.h"
#include "Math\CoolD_Vector3.h"
#include "Console\CoolD_Command.h"
#include "Console\CoolD_ConsoleManager.h"
#include "Thread\CoolD_ThreadManager.h"
#include "Camera\\CoolD_Camera.h"

#pragma warning(disable: 4100)

using namespace CoolD;

Matrix44 g_matWorld;
Matrix44 g_matView;
Matrix44 g_matPers;

atomic<int> g_sGridCount(1);
atomic<int> g_sGridChangeCount(1);
atomic<bool> g_sThreadUse(false);
atomic<int> g_sThreadCount(0);

//Dbool WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID fImpLoad)
//{
//	switch( fdwReason )
//	{
//	case DLL_THREAD_ATTACH:
//	case DLL_THREAD_DETACH:		
//	case DLL_PROCESS_ATTACH:
//	case DLL_PROCESS_DETACH:
//		break;
//	}	
//	return true;
//}

static FunctionCommand ShowCommand("show_command", [] (initializer_list<string> commandNameList)
{
	for( auto& commandName : commandNameList )
	{
		GETSINGLE(ConsoleManager).ShowCommand(commandName);
	}
});

auto g_renderModule = new RenderModule();

EXTERN_FORM DLL_API Dvoid __cdecl coold_LoadMeshFromFile( const Dchar* filename )
{		
	GETSINGLE(MeshManager).Clear();	
	GETSINGLE(MeshManager).LoadMesh(filename);

	//쓰레드 테스트를 위한 추가 로직----------------------------------	
	for (Dint i = 0; i < g_sGridCount.load() * g_sGridCount.load() - 1; ++i)
	{
		CustomMesh* pMesh = GETSINGLE(MeshManager).GetMesh(filename);
		if (CustomMeshPLY* pPlyMesh = dynamic_cast<CustomMeshPLY*>(pMesh))
		{
			CustomMesh* pNewMesh = new CustomMeshPLY(*pPlyMesh);
			GETSINGLE(MeshManager).AddMesh(filename + to_string(i), pNewMesh);
		}		
	}
	//-----------------------------------------------------------
}

Dvoid RenderModuleInit(Dvoid* buffer, Dint width, Dint height)
{
	g_renderModule->SetScreenInfo(buffer, width, height);
	g_renderModule->ClearColorBuffer(BLACK);
	g_renderModule->SetTransform(WORLD, g_matWorld);
	g_renderModule->SetTransform(VIEW, g_matView);
	g_renderModule->SetTransform(PERSPECTIVE, g_matPers);
	g_renderModule->SetTransform(VIEWPORT, TransformHelper::CreateViewport(0, 0, width, height));
}

ThreadManager* g_pThreadManager = &GETSINGLE(ThreadManager);

Dvoid SerializeTask( Dbool threadUse )
{	
	if (g_sGridChangeCount.load() * g_sGridChangeCount.load() != (signed)GETSINGLE(MeshManager).GetMapMesh().size())
	{
		atomic_exchange(&g_sGridCount, g_sGridChangeCount.load());
		string strFileName( (*GETSINGLE(MeshManager).GetMapMesh().begin()).first );
		coold_LoadMeshFromFile(strFileName.c_str());
	}
	
	if (threadUse != g_sThreadUse.load())
	{
		g_pThreadManager->CleanThreads();

		if (g_sThreadUse.load())
		{	//쓰레드 사용시에만 초기화
			g_pThreadManager->Initialize(0);
		}
	}

	if (g_sThreadCount.load() != 0)
	{
		g_pThreadManager->ResetThreads(g_sThreadCount.load());
		atomic_exchange(&g_sThreadCount, 0);
	}
}

EXTERN_FORM DLL_API Dvoid __cdecl coold_RenderToBuffer(Dvoid* buffer, Dint width, Dint height, Dint bpp)
{
	RenderModuleInit(buffer, width, height);

	int sequence = 0;
	bool threadUse = g_sThreadUse.load();
	for (auto& Mesh : GETSINGLE(MeshManager).GetMapMesh())
	{
		if ( threadUse )
		{
			if (g_sGridCount.load() > 1)
			{
				g_renderModule->AdjustGridWorld(g_sGridCount.load(), sequence++);
			}

			RenderInfoParam param(g_renderModule, Mesh.second);
			g_pThreadManager->AssignWork(&param);
		}
		else
		{
			if (g_sGridCount.load() > 1)
			{
				g_renderModule->AdjustGridWorld(g_sGridCount.load(), sequence++);
			}
			g_renderModule->RenderBegin(Mesh.second);
			g_renderModule->RenderEnd();
		}
	}

	if ( threadUse )
		g_pThreadManager->WaitAllThreadWorking();
	
	//───────────────────────────────────────────────────────────────────────────────────
	//이 부분은 시리얼 동작한다.
	SerializeTask(threadUse);	
	//───────────────────────────────────────────────────────────────────────────────────

}

static VariableCommand cc_x_move("cc_x_move", "0");
static VariableCommand cc_y_move("cc_y_move", "0");
static VariableCommand cc_z_move("cc_z_move", "0");
EXTERN_FORM DLL_API Dvoid __cdecl coold_SetTransform(Dint transformType, const Dfloat* matrix4x4)
{	//월드만 넘겨 받는다.
	switch( transformType )
	{
	case 0:	//World
		{
			Matrix44 matWorld(matrix4x4);								
			g_matWorld = matWorld.Transpose();

			//원점 기준으로 이동
			if (cc_x_move.Bool())	g_matWorld[12] = cc_x_move.Float();
			if (cc_y_move.Bool())	g_matWorld[13] = cc_y_move.Float();
			if (cc_z_move.Bool())	g_matWorld[14] = cc_z_move.Float();								
		}
		break;
	default:
		break;
	}
}

static VariableCommand cc_use_camera("cc_use_camera", "1");
static VariableCommand cc_x_eye("cc_x_eye", "0");
static VariableCommand cc_y_eye("cc_y_eye", "0");
static VariableCommand cc_z_eye("cc_z_eye", "0");
EXTERN_FORM DLL_API Dvoid __cdecl coold_SetViewFactor(Dfloat* eye, Dfloat* lookat, Dfloat* up)
{
	//시점 이동
	if (!cc_use_camera.Bool())
	{
		if (cc_x_eye.Bool()) eye[0] += cc_x_eye.Float();
		if (cc_y_eye.Bool()) eye[1] += cc_y_eye.Float();
		if (cc_z_eye.Bool()) eye[2] += cc_z_eye.Float();

		g_matView = TransformHelper::CreateView(Vector3(eye), Vector3(lookat), Vector3(up));
	}
}

static VariableCommand cc_adjust_zn("cc_adjust_zn", "0");
static VariableCommand cc_adjust_zf("cc_adjust_zf", "0");
EXTERN_FORM DLL_API Dvoid __cdecl coold_SetPerspectiveFactor(Dfloat fovY, Dfloat aspect, Dfloat zn, Dfloat zf)
{	
	if ( !cc_use_camera.Bool() )
		g_matPers = TransformHelper::CreatePerspective(kPI / fovY, aspect, zn + cc_adjust_zn.Float(), zf + cc_adjust_zf.Float());
}
								  
EXTERN_FORM DLL_API Dvoid __cdecl coold_ExecuteCommand(const Dchar* cmd)
{
	GETSINGLE(ConsoleManager).CommandExecute(cmd);
}

EXTERN_FORM DLL_API Dvoid __cdecl coold_DetachModuleClear()
{
	GETSINGLE(ThreadManager).CleanThreads();
}

EXTERN_FORM DLL_API Dvoid __cdecl coold_CreateCamera(Dfloat* eye, Dfloat* lookat, Dfloat angle, Dfloat aspect)
{	
	GETSINGLE(Camera).CreateCamera(eye, lookat, angle, aspect);		
}

EXTERN_FORM DLL_API Dvoid __cdecl coold_UpdateCamera(Dint x, Dint y, Dint clickState, Dbool front, Dbool back, Dbool left, Dbool right, Dbool up, Dbool down)
{	
	if (cc_use_camera.Bool())
	{		
		MoveKey moveKey = { front, back, left, right, up, down };
		GETSINGLE(Camera).UpdateCamera(x, y, clickState, moveKey, g_matView, g_matPers);
	}
}

//-----------------------------------------------------------------------------------------------------------------------------
static FunctionCommand fc_bsculltype("fc_bsculltype", [] (initializer_list<string> strs)
{
	if( strs.size() > 0 )
	{
		Dint type = stoi((*begin(strs)));		//stoi는 숫자문자가 아닌 값이 들어오면 프로그램을 종료시킨다.
		g_renderModule->SetBackSpaceCullType(static_cast<BSCullType>(type));
	}
	else
	{
		LOG("don't input Backspace CullingType ( 0 : CW, 1 : CCW, the other number : ALL )");
	}
});

static FunctionCommand fc_thread_count("fc_thread_count", [](initializer_list<string> strs)
{
	atomic_exchange(&g_sThreadCount, stoi(*strs.begin()));	
});

static FunctionCommand fc_thread_use("fc_thread_use", [] (initializer_list<string> strs)
{
	if( strs.size() == 1 )
	{
		Dbool isUse = ( stoi(*strs.begin()) == 1 )? true:false;

		if (g_sThreadUse.load() != isUse)
		{			
			atomic_exchange(&g_sThreadUse, isUse);			
		}		
	}
});

static FunctionCommand fc_change_grid("fc_change_grid", [](initializer_list<string> strs)
{
	if (strs.size() == 1)
	{			
		atomic_exchange(&g_sGridChangeCount, stoi(*strs.begin()));

		VariableCommand* pZf = dynamic_cast<VariableCommand*>( GETSINGLE(ConsoleManager).FindCommand("cc_adjust_zf") );
		pZf->SetValue(g_sGridChangeCount.load() * 8.0f);

		VariableCommand* pZeye = dynamic_cast<VariableCommand*>(GETSINGLE(ConsoleManager).FindCommand("cc_z_eye"));
		pZeye->SetValue(-(g_sGridChangeCount.load() * 8.0f));
	}
});
