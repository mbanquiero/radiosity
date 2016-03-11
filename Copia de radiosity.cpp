//--------------------------------------------------------------------------------------
// File: Radiosity.cpp
//
// Starting point for new Direct3D applications
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------
#include "dxstdafx.h"
#include "dxutmesh.h"
#include "resource.h"

//#define DEBUG_VS   // Uncomment this line to debug vertex shaders 
//#define DEBUG_PS   // Uncomment this line to debug pixel shaders 

#define IDC_MOVER_SOL 1

#define SHADOWMAP_SIZE 256
#define MAP_SIZE 512

#define HELPTEXTCOLOR D3DXCOLOR( 0.0f, 1.0f, 0.3f, 1.0f )

// helper
HRESULT OptimizarMesh( ID3DXMesh** ppInOutMesh );
void rotar_xy(D3DXVECTOR3 *v,double an)
{
	float x =v->x*cos(an)+v->y*sin(an);
	float y =v->y*cos(an)-v->x*sin(an); 
	v->x = x;
	v->y = y;
}

void rotar_xz(D3DXVECTOR3 *v,double an)
{
	float x =v->x*cos(an)-v->z*sin(an);
	float z =v->x*sin(an)+v->z*cos(an); 
	v->x = x;
	v->z = z;
}

void rotar_zy(D3DXVECTOR3 *v,double an)
{
	float y =v->y*cos(an)+v->z*sin(an);
	float z =v->z*cos(an)-v->y*sin(an); 
	v->y = y;
	v->z = z;
}

// Ambiente de prueba: esta compuesto de 2 habitaciones iguales, una atras de la otra
LPCWSTR g_aszMeshFile[] =
{
    L"room.x",
    L"room.x",
	L"cube_on_plane.x",
	L"cube_on_plane.x",
};

#define NUM_OBJ	(sizeof(g_aszMeshFile)/sizeof(g_aszMeshFile[0]))

// matrices de transf. de model a world
D3DXMATRIXA16 g_amInitObjWorld[NUM_OBJ] =
{
	// malla que funciona como piso (+ pared izquierda inferior)
	D3DXMATRIXA16(	1, 0, 0, 0, 
					0, 1, 0, 0, 
					0, 0, 1, 0, 
					-6*1, -2.5*1, -5*1, 1 ),
	// techo + pared izquierda superior
	D3DXMATRIXA16(	1, 0, 0, 0, 
					0, -1, 0, 0,						
					0, 0, 1, 0, 
					-6*1, 12-2.5*1, -5*1, 1 ),			
					// 0, 1, 0, 0,						// atras
					//-6*1, -2.5*1, -5*1+15, 1 ),		// atras

	// pared de atras para que no se escape tanta luz
	D3DXMATRIXA16(	1.5, 0, 0, 0, 
					0, 0, -1, 0, 
					0, 1.5, 0, 0, 
					3, 3, 7, 1 ),

	// pared derecha
	D3DXMATRIXA16(	0, 0, 1.7, 0, 
					-1, 0, 0, 0, 
					0, 1.5, 0, 0, 
					8.7, 3, 0, 1 ),

};


// necesito que el fomato para el mesh, tenga una coordenada de textura adicional
// que voy a utilizar para incrustar el id de cada vertice
D3DVERTEXELEMENT9 g_aVertDecl[] =
{
    { 0, 0,  D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
    { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
    { 0, 32, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1 },
    D3DDECL_END()
};

// Vertex format para mesh
struct VERTEX
{
    D3DXVECTOR3 position;
    D3DXVECTOR3 normal;
    D3DXVECTOR2 texcoord;
    D3DXVECTOR2 texcoord_id;
};

#define D3DFVF_VERTEX (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX2)

// encapsula el mesh con su transformacion y lo relaciono con el primer vertice 
struct CObj
{
    CDXUTMesh m_Mesh;
    D3DXMATRIXA16 m_mWorld;
	int primer_vertice;
	D3DXVECTOR3 N[50000];		// normales
};


// Esto estas currado del DXUtil, lo dejo tal como esta por el momento
//-----------------------------------------------------------------------------
// Name: class CViewCamera
// Desc: A camera class derived from CFirstPersonCamera.  The arrow keys and
//       numpad keys are disabled for this type of camera.
//-----------------------------------------------------------------------------
class CViewCamera : public CFirstPersonCamera
{
protected:
    virtual D3DUtil_CameraKeys MapKey( UINT nKey )
    {
        // Provide custom mapping here.
        // Same as default mapping but disable arrow keys.
        switch( nKey )
        {
            case 'A':      return CAM_STRAFE_LEFT;
            case 'D':      return CAM_STRAFE_RIGHT;
            case 'W':      return CAM_MOVE_FORWARD;
            case 'S':      return CAM_MOVE_BACKWARD;
            case 'Q':      return CAM_MOVE_DOWN;
            case 'E':      return CAM_MOVE_UP;

            case VK_HOME:   return CAM_RESET;
        }

        return CAM_UNKNOWN;
    }
};

//-----------------------------------------------------------------------------
// Name: class CLightCamera
// Desc: A camera class derived from CFirstPersonCamera.  The letter keys
//       are disabled for this type of camera.  This class is intended for use
//       by the spot light.
//-----------------------------------------------------------------------------
class CLightCamera : public CFirstPersonCamera
{
protected:
    virtual D3DUtil_CameraKeys MapKey( UINT nKey )
    {
        // Provide custom mapping here.
        // Same as default mapping but disable arrow keys.
        switch( nKey )
        {
            case VK_LEFT:  return CAM_STRAFE_LEFT;
            case VK_RIGHT: return CAM_STRAFE_RIGHT;
            case VK_UP:    return CAM_MOVE_FORWARD;
            case VK_DOWN:  return CAM_MOVE_BACKWARD;
            case VK_PRIOR: return CAM_MOVE_UP;        // pgup
            case VK_NEXT:  return CAM_MOVE_DOWN;      // pgdn

            case VK_NUMPAD4: return CAM_STRAFE_LEFT;
            case VK_NUMPAD6: return CAM_STRAFE_RIGHT;
            case VK_NUMPAD8: return CAM_MOVE_FORWARD;
            case VK_NUMPAD2: return CAM_MOVE_BACKWARD;
            case VK_NUMPAD9: return CAM_MOVE_UP;        
            case VK_NUMPAD3: return CAM_MOVE_DOWN;      

            case VK_HOME:   return CAM_RESET;
        }

        return CAM_UNKNOWN;
    }
};


//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

// esto esta currado del dXutil, para que dibuje info de pantalla: fps, etc. lo dejo tal como esta
ID3DXFont*              g_pFont = NULL;         // Font for drawing text
ID3DXFont*              g_pFontSmall = NULL;    // Font for drawing text
ID3DXSprite*            g_pTextSprite = NULL;   // Sprite for batching draw text calls
ID3DXEffect*            g_pEffect = NULL;       // D3DX effect interface
CDXUTDialogResourceManager g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg         g_SettingsDlg;          // Device settings dialog
CDXUTDialog             g_HUD;                  // dialog for standard controls

//--------------------------------------------------------------------------------------
CFirstPersonCamera      g_VCamera;              // camara pp dicha
CFirstPersonCamera      g_LCamera;              // esta la uso para poder ajustar la luz usando el DXutil 

//--------------------------------------------------------------------------------------
// Esto tambien esta currado del DXUtil, para manejar el input del mouse etc
bool                    g_bRightMouseDown = false;// Indicates whether right mouse button is held
bool                    g_bCameraPerspective    // Indicates whether we should render view from
                          = true;               // the camera's or the light's perspective
//--------------------------------------------------------------------------------------


// escena pp dicha 
CObj                    g_Obj[NUM_OBJ];
LPDIRECT3DVERTEXDECLARATION9 g_pVertDecl = NULL;
LPDIRECT3DTEXTURE9      g_pTexDef = NULL;       // textura por defecto si falla la carga
CDXUTMesh               g_LightMesh;			// malla para mostrar un conito que representa la luz
bool                    g_bSolcito = false;		// mover el sol desde afuera de la habitacion

//--------------------------------------------------------------------------------------
// Aca viene la posta: 
//--------------------------------------------------------------------------------------
LPDIRECT3DTEXTURE9      g_pShadowMap = NULL;    // Texture to which the shadow map is rendered
LPDIRECT3DSURFACE9      g_pDSShadow = NULL;     // Depth-stencil buffer for rendering to shadow map
float                   g_fLightFov;            // FOV de la luz
D3DXMATRIXA16           g_mShadowProj;          // Projection matrix for shadow map
LPDIRECT3DVERTEXBUFFER9 g_pVB = NULL;			// Buffer para vertices
LPDIRECT3DTEXTURE9      g_pRMap = NULL;			// Radiosity Map Texture para almacenar la cantidad de luz acumulada x vertice

int cant_vertices = 0;
int nro_luz = 0;
int cant_luces = 2;
BOOL primera_vez = TRUE;
D3DXVECTOR3 g_LightPos;							// posicion de la luz actual (la que estoy analizando)
D3DXVECTOR3 g_LightDir;							// direccion de la luz actual
D3DXMATRIXA16 g_LightView;						// 
D3DXVECTOR3 LightPos[256],LightDir[256];


//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void             InitializeDialogs();
bool    CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
bool    CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, const D3DCAPS9* pCaps, void* pUserContext );
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void    CALLBACK OnFrameMove( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void    CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext );
void             RenderText();
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void    CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void    CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext );
void    CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void    CALLBACK OnLostDevice( void* pUserContext );
void    CALLBACK OnDestroyDevice( void* pUserContext );
void             RenderScene( IDirect3DDevice9* pd3dDevice, bool bRenderShadow, float fElapsedTime, const D3DXMATRIX *pmView, const D3DXMATRIX *pmProj );
void			 ProcesarVertices( IDirect3DDevice9* pd3dDevice,const D3DXMATRIX *pmView, const D3DXMATRIX *pmProj);


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Initialize the camera
    g_VCamera.SetScalers( 0.01f, 2.5f );
    g_LCamera.SetScalers( 0.01f, 1.0f );
    g_VCamera.SetRotateButtons( true, false, false );
    g_LCamera.SetRotateButtons( false, false, true );

    // Set up the view parameters for the camera
    D3DXVECTOR3 vFromPt   = D3DXVECTOR3( 0.0f, 5.0f, -18.0f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
    g_VCamera.SetViewParams( &vFromPt, &vLookatPt );

    vFromPt = D3DXVECTOR3( 0.0f, 8.0f, 2.0f );
    //vLookatPt = D3DXVECTOR3( 0.0f, -2.0f, 1.0f );
	vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    g_LCamera.SetViewParams( &vFromPt, &vLookatPt );
	
    // Initialize the spot light
    g_fLightFov = D3DX_PI / 2.0f;


	float H = 10000*99+100*99+99;
	int b = H/10000;
	H-=b*10000;
	int g = H/100;
	H-=g*100;
	int r = H;


    // Set the callback functions. These functions allow DXUT to notify
    // the application about device changes, user input, and windows messages.  The 
    // callbacks are optional so you need only set callbacks for events you're interested 
    // in. However, if you don't handle the device reset/lost callbacks then the sample 
    // framework won't be able to reset your device since the application must first 
    // release all device resources before resetting.  Likewise, if you don't handle the 
    // device created/destroyed callbacks then DXUT won't be able to 
    // recreate your device resources.
    DXUTSetCallbackDeviceCreated( OnCreateDevice );
    DXUTSetCallbackDeviceReset( OnResetDevice );
    DXUTSetCallbackDeviceLost( OnLostDevice );
    DXUTSetCallbackDeviceDestroyed( OnDestroyDevice );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackMouse( MouseProc );
    DXUTSetCallbackFrameRender( OnFrameRender );
    DXUTSetCallbackFrameMove( OnFrameMove );

    InitializeDialogs();

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit( true, true, true ); // Parse the command line, handle the default hotkeys, and show msgboxes
    DXUTCreateWindow( L"Radiosity" );
    DXUTCreateDevice( D3DADAPTER_DEFAULT, true, 800, 600, IsDeviceAcceptable, ModifyDeviceSettings );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Sets up the dialogs
//--------------------------------------------------------------------------------------
void InitializeDialogs()
{
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_HUD.SetCallback( OnGUIEvent );
    g_HUD.AddCheckBox( IDC_MOVER_SOL, L"Mover Sol", 0, 0, 160, 22, false, L'1' );

}


//--------------------------------------------------------------------------------------
// Called during device initialization, this code checks the device for some 
// minimum set of capabilities, and rejects those that don't pass by returning false.
//--------------------------------------------------------------------------------------
bool CALLBACK IsDeviceAcceptable( D3DCAPS9* pCaps, D3DFORMAT AdapterFormat, 
                                  D3DFORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    // Skip backbuffer formats that don't support alpha blending
    IDirect3D9* pD3D = DXUTGetD3DObject(); 
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                    AdapterFormat, D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING, 
                    D3DRTYPE_TEXTURE, BackBufferFormat ) ) )
        return false;

    // necesitamos shader 3.0
    if( pCaps->PixelShaderVersion < D3DPS_VERSION( 3, 0 ) )
        return false;

    // necesitamos D3DFMT_R32F para render target (para el shadow map y el Radiosity map) 
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                    AdapterFormat, D3DUSAGE_RENDERTARGET, 
				D3DRTYPE_CUBETEXTURE, D3DFMT_R32F ) ) )
					
        return false;

    // tambien D3DFMT_A8R8G8B8 para render target
    if( FAILED( pD3D->CheckDeviceFormat( pCaps->AdapterOrdinal, pCaps->DeviceType,
                    AdapterFormat, D3DUSAGE_RENDERTARGET, 
                    D3DRTYPE_CUBETEXTURE, D3DFMT_A8R8G8B8 ) ) )
        return false;

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise IDirect3D9::CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, const D3DCAPS9* pCaps, void* pUserContext )
{
    // Turn vsync off
    pDeviceSettings->pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    g_SettingsDlg.GetDialogControl()->GetComboBox( DXUTSETTINGSDLG_PRESENT_INTERVAL )->SetEnabled( false );

    // If device doesn't support HW T&L or doesn't support 1.1 vertex shaders in HW 
    // then switch to SWVP.
    if( (pCaps->DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0 ||
         pCaps->VertexShaderVersion < D3DVS_VERSION(1,1) )
    {
        pDeviceSettings->BehaviorFlags = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }

    // Debugging vertex shaders requires either REF or software vertex processing 
    // and debugging pixel shaders requires REF.  
#ifdef DEBUG_VS
    if( pDeviceSettings->DeviceType != D3DDEVTYPE_REF )
    {
        pDeviceSettings->BehaviorFlags &= ~D3DCREATE_HARDWARE_VERTEXPROCESSING;
        pDeviceSettings->BehaviorFlags &= ~D3DCREATE_PUREDEVICE;                            
        pDeviceSettings->BehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    }
#endif
#ifdef DEBUG_PS
    pDeviceSettings->DeviceType = D3DDEVTYPE_REF;
#endif
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( pDeviceSettings->DeviceType == D3DDEVTYPE_REF )
            DXUTDisplaySwitchingToREFWarning();
    }

    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnDestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnCreateDevice( IDirect3DDevice9* pd3dDevice, const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;


    V_RETURN( g_DialogResourceManager.OnCreateDevice( pd3dDevice ) );
    V_RETURN( g_SettingsDlg.OnCreateDevice( pd3dDevice ) );
    // Initialize the font
    V_RETURN( D3DXCreateFont( pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                         OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                         L"Arial", &g_pFont ) );
    V_RETURN( D3DXCreateFont( pd3dDevice, 12, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
                         OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
                         L"Arial", &g_pFontSmall ) );

    // Define DEBUG_VS and/or DEBUG_PS to debug vertex and/or pixel shaders with the 
    // shader debugger. Debugging vertex shaders requires either REF or software vertex 
    // processing, and debugging pixel shaders requires REF.  The 
    // D3DXSHADER_FORCE_*_SOFTWARE_NOOPT flag improves the debug experience in the 
    // shader debugger.  It enables source level debugging, prevents instruction 
    // reordering, prevents dead code elimination, and forces the compiler to compile 
    // against the next higher available software target, which ensures that the 
    // unoptimized shaders do not exceed the shader model limitations.  Setting these 
    // flags will cause slower rendering since the shaders will be unoptimized and 
    // forced into software.  See the DirectX documentation for more information about 
    // using the shader debugger.
    DWORD dwShaderFlags = D3DXFX_NOT_CLONEABLE;
    #ifdef DEBUG_VS
        dwShaderFlags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
    #endif
    #ifdef DEBUG_PS
        dwShaderFlags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
    #endif

	// Cargo y compilo el efecto 
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Radiosity.fx" ) );
	ID3DXBuffer *pBuffer = NULL;
    if( FAILED(D3DXCreateEffectFromFile( pd3dDevice, str, NULL, NULL, dwShaderFlags, 
                                        NULL, &g_pEffect, &pBuffer ) ))
	{
		// si da error, aca lo puedo analizar: 
		char *saux = (char *)pBuffer->GetBufferPointer();
		return hr;
	}

    // Creo el vertex declaration
    V_RETURN( pd3dDevice->CreateVertexDeclaration( g_aVertDecl, &g_pVertDecl ) );

    // Inicializo los mesh
    for( int i = 0; i < NUM_OBJ; ++i )
    {
        V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, g_aszMeshFile[i] ) );
        if( FAILED( g_Obj[i].m_Mesh.Create( pd3dDevice, str ) ) )
            return DXUTERR_MEDIANOTFOUND;

		//V_RETURN(OptimizarMesh(&g_Obj[i].m_Mesh.m_pMesh));

        V_RETURN( g_Obj[i].m_Mesh.SetVertexDecl( pd3dDevice, g_aVertDecl ) );
        g_Obj[i].m_mWorld = g_amInitObjWorld[i];
    }

	/*
	// Determino la extension del objeto
	{
		LPD3DXMESH g_pMesh = g_Obj[0].m_Mesh.m_pMesh;
		D3DXVECTOR3 min,max;
		min = max = D3DXVECTOR3 (0,0,0);
		BYTE* pVertices=NULL; 
		g_pMesh->LockVertexBuffer(D3DLOCK_READONLY, (LPVOID*)&pVertices); 
		D3DXComputeBoundingBox((D3DXVECTOR3 *)pVertices,g_pMesh->GetNumVertices(),g_pMesh->GetNumBytesPerVertex(),&min,&max);
		g_pMesh->UnlockVertexBuffer(); 
	}
	*/

	// necesito que la habitacion tenga mas rectangulos en las paredes
	/*
	{
		LPD3DXMESH pMeshOut;
		D3DXTessellateNPatches(g_Obj[0].m_Mesh.m_pMesh,NULL,50,FALSE,&pMeshOut,NULL);
		g_Obj[0].m_Mesh.m_pMesh->Release();
		g_Obj[0].m_Mesh.m_pMesh = pMeshOut;
	}
	*/


	//--------------------------------------------------------------------------------------
	// Voy a crear un buffer de vertices, que contiene TODOS los vertices de cada una de las mallas. 
	// Si existia de antes lo borro 
	if(g_pVB)
		g_pVB->Release();
	g_pVB = NULL;
	// primero cuento cuantos vertices hay, asi puedo allocar suficiente memoria
	cant_vertices = 0;		// Cantidad total de vertices
	int cant_f = 0;
    for(int i = 0; i < NUM_OBJ; ++i )
    {
		LPD3DXMESH pMesh = g_Obj[i].m_Mesh.GetMesh();
		cant_vertices += pMesh->GetNumVertices();
		cant_f += pMesh->GetNumFaces();
	}
	// Alloco memoria y creo un buffer para todos los vertices
	VERTEX * pVertices;
	size_t size = sizeof(VERTEX)*cant_vertices;
	if( FAILED( pd3dDevice->CreateVertexBuffer( size,
				0 , D3DFVF_VERTEX, D3DPOOL_DEFAULT, &g_pVB, NULL ) ) )
		return E_FAIL;
	if( FAILED( g_pVB->Lock( 0, size, (void**)&pVertices, 0 ) ) )
		return E_FAIL;


	// lleno los vertices con un ID unico 
	// El id lo formo con un par de coordenadas i,j que permiten relacionar
	// una textura con los vertices
	// tex2d(i,j) --> representa el vertice i,j
	int id_i = 0;
	int id_j = 0;
	int k = 0;		// nro de vertice	
    for(int i = 0; i < NUM_OBJ; ++i )
    {
		LPD3DXMESH pMesh = g_Obj[i].m_Mesh.GetMesh();
		g_Obj[i].primer_vertice = k;
		int cant_v = pMesh->GetNumVertices();
		size_t bpv = pMesh->GetNumBytesPerVertex();
		BYTE *pVerticesMesh=NULL;
		pMesh->LockVertexBuffer(0, (LPVOID*)&pVerticesMesh); 
		// Recorro los vertices y los copio al buffer global (que agrupa TODAS los mesh)
		for(int j=0;j<cant_v;j++)
		{
			VERTEX *V = (VERTEX *)(pVerticesMesh+ bpv*j);
			// Creo el indice
			V->texcoord_id.x = id_i;			// el valor u lo uso para almacenar el id_i
			V->texcoord_id.y = id_j;
			if(++id_i>=MAP_SIZE)
			{
				id_i = 0;
				id_j++;
			}
			// Almaceno el Vertice en el global
			pVertices[k++] = *V;
		}

		{
			// alloco memoria para los indices (para almacenar las normales)
			WORD* pIndices=NULL;
			pMesh->LockIndexBuffer(D3DLOCK_READONLY, (LPVOID*)&pIndices); 
			int cant_f = pMesh->GetNumFaces();
			for(int f=0;f<cant_f;++f)
			{
				// tomo como normal el primer vertice
				VERTEX *V = (VERTEX *)(pVerticesMesh+ bpv*pIndices[3*f]);
				g_Obj[i].N[f] = V->normal;
			}
			pMesh->UnlockIndexBuffer(); 
		}
		pMesh->UnlockVertexBuffer();
	}
	g_pVB->Unlock();

    // Creo e inicializo la malla flechita que representa la direccion de luz 
	// (solo cosmetica, para ver donde esta la luz. 
	// Guarda!: que luz es un patch como cualquier otro, la unica diferencia
	// es que es el patch inicial. NO ES UN SPOT LIGHT, . y ojo que 
	// si bien tiene una direccion, (que en gral es la normal al patch), en teoria el angulo 
	// solido es todo el hemisferio (fov = 180grados) 
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"UI\\arrow.x" ) );
    if( FAILED( g_LightMesh.Create( pd3dDevice, str ) ) )
        return DXUTERR_MEDIANOTFOUND;
    V_RETURN( g_LightMesh.SetVertexDecl( pd3dDevice, g_aVertDecl ) );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// reset, which will happen after a lost device scenario. This is the best location to 
// create D3DPOOL_DEFAULT resources since these resources need to be reloaded whenever 
// the device is lost. Resources created here should be released in the OnLostDevice 
// callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnResetDevice( IDirect3DDevice9* pd3dDevice, 
                                const D3DSURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

	//--------------------------------------------------------------------------------------
	// esto es todo currado del dxutil, para la cosmetica de la pantalla
    V_RETURN( g_DialogResourceManager.OnResetDevice() );
    V_RETURN( g_SettingsDlg.OnResetDevice() );
    if( g_pFont )
        V_RETURN( g_pFont->OnResetDevice() );
    if( g_pFontSmall )
        V_RETURN( g_pFontSmall->OnResetDevice() );
    if( g_pEffect )
        V_RETURN( g_pEffect->OnResetDevice() );
    // Create a sprite to help batch calls when drawing many lines of text
    V_RETURN( D3DXCreateSprite( pd3dDevice, &g_pTextSprite ) );

	g_HUD.SetLocation( pBackBufferSurfaceDesc->Width-170, 0 );
    g_HUD.SetSize( 170, pBackBufferSurfaceDesc->Height );

	//--------------------------------------------------------------------------------------

    // incializo las camaras
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_VCamera.SetProjParams( D3DX_PI/4, fAspectRatio, 0.1f, 100.0f );
    g_LCamera.SetProjParams( D3DX_PI, fAspectRatio, 0.1f, 100.0f );

    // creo una textura por defecto, color gris, para que se use cuando falla alguna carga de textura
	// la textura tiene 1 x 1, con lo cual ocupa un solo 32bits 
    V_RETURN( pd3dDevice->CreateTexture( 1, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_pTexDef, NULL ) );
    // asi que no hay duda, accedo directo a los 32 bits 
	// (si fuese de n x m, tengo que estudiar el tema del pitch) que no lo tengo del todo desculado
	D3DLOCKED_RECT lr;
    V_RETURN( g_pTexDef->LockRect( 0, &lr, NULL, 0 ) );
    *(LPDWORD)lr.pBits = D3DCOLOR_RGBA( 128, 128, 128, 255 );
    V_RETURN( g_pTexDef->UnlockRect( 0 ) );
	
    // Restore the scene objects
    for( int i = 0; i < NUM_OBJ; ++i )
        V_RETURN( g_Obj[i].m_Mesh.RestoreDeviceObjects( pd3dDevice ) );
    V_RETURN( g_LightMesh.RestoreDeviceObjects( pd3dDevice ) );

	//--------------------------------------------------------------------------------------
    // Creo el shadowmap. 
	// lo voy a usar para calcular el factor de visibilidad de la ecuacion del radiosity
	// esto es V(i,j) = 1 si el patch j es visible desde el patch, 0 caso contrario. 
	V_RETURN( pd3dDevice->CreateTexture( SHADOWMAP_SIZE, SHADOWMAP_SIZE,
                                         1, D3DUSAGE_RENDERTARGET,
                                         D3DFMT_R32F,
                                         D3DPOOL_DEFAULT,
                                         &g_pShadowMap,
                                         NULL ) );
	// tengo que crear un stencilbuffer para el shadowmap manualmente
	// para asegurarme que tenga la el mismo tamaño que el shadowmap, y que no tenga 
	// multisample, etc etc. 
	DXUTDeviceSettings d3dSettings = DXUTGetDeviceSettings();
    V_RETURN( pd3dDevice->CreateDepthStencilSurface( SHADOWMAP_SIZE,
                                                     SHADOWMAP_SIZE,
                                                     d3dSettings.pp.AutoDepthStencilFormat,
                                                     D3DMULTISAMPLE_NONE,
                                                     0,
                                                     TRUE,
                                                     &g_pDSShadow,
                                                     NULL ) );
    // por ultimo necesito una matriz de proyeccion para el shadowmap, ya 
	// que voy a dibujar desde el pto de vista de la luz. 
    D3DXMatrixPerspectiveFovLH( &g_mShadowProj, g_fLightFov, 1, 0.01f, 100.0f);


	//--------------------------------------------------------------------------------------
	// Create the mapa para guardar la radiosidad
	// nota: en el VS, tengo que acceder a esta textura, para ello tengo 
	// que usar tex2Dlod, encontre que solo  funciona para texturas en formato float: 
	
	// Vertex Shader 3.0 supports texture sampling
	// The HLSL function that performs texture sampling in VS is tex2Dlod(). It takes two arguments, 
	// your sampler and a float4 in the format (u, v, mip1, mip2). 
	// Set mip1 and mip2 to 0 unless you want to do mipmap blending
	// Current hardware only supports floating point texture formats for vertex 
	// texture sampling. When you load the texture that you're going to sample 
	// in the VS pass D3DFMT_A32B32G32R32F or D3DFMT_R16F as the format.
	V_RETURN( pd3dDevice->CreateTexture( MAP_SIZE, MAP_SIZE,
                                         1, D3DUSAGE_RENDERTARGET,
                                         D3DFMT_R32F,
                                         D3DPOOL_DEFAULT,
                                         &g_pRMap,
                                         NULL ) );


		// asigno las texturas al efecto para que el sampler las pueda acceder
		g_pEffect->SetTexture( "g_txRMap", g_pRMap);
		g_pEffect->SetTexture( "g_txShadow", g_pShadowMap);

		pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE /*3DCULL_CCW */);


    return S_OK;
}


//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_VCamera.FrameMove( fElapsedTime );
    g_LCamera.FrameMove( fElapsedTime );

}


//--------------------------------------------------------------------------------------
// Renders the scene onto the current render target using the current
// technique in the effect.
//--------------------------------------------------------------------------------------

// A RenderScene lo voy a llamar en 2 ocaciones:
// 1-Cuando genero el shadowmap: en ese caso me genera en una textura, un mapa donde en lugar
// de dibujar los colores pp dichos, "dibuja" la profundidad (el valor z/w). 
// 2-Para dibujar la escena pp dicha. 

void RenderScene( IDirect3DDevice9* pd3dDevice, bool bRenderShadow, float fElapsedTime, const D3DXMATRIX *pmView, const D3DXMATRIX *pmProj )
{
    HRESULT hr;

	// inicializacion standard: 
    V( g_pEffect->SetMatrix( "g_mProj", pmProj ) );
    V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0x000000ff, 1.0f, 0L ) );

	// Seteo la tecnica: estoy generando la sombra o estoy dibujando la escena
	V( g_pEffect->SetTechnique(bRenderShadow?"RenderShadow":"RenderScene") );

    // Begin the scene
    if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
        // Render the objects
        for( int obj = 0; obj < NUM_OBJ; ++obj )
        {
			// seteo las matrices de transformacion. 
            D3DXMATRIXA16 mWorldView = g_Obj[obj].m_mWorld;
            D3DXMatrixMultiply( &mWorldView, &mWorldView, pmView );
            V( g_pEffect->SetMatrix( "g_mWorldView", &mWorldView ) );
			V( g_pEffect->SetMatrix( "g_mObjWorld", &g_Obj[obj].m_mWorld) );

            LPD3DXMESH pMesh = g_Obj[obj].m_Mesh.GetMesh();
            UINT cPass;
            V( g_pEffect->Begin( &cPass, 0 ) );
            for( UINT p = 0; p < cPass; ++p )
            {
                V( g_pEffect->BeginPass( p ) );

                for( DWORD i = 0; i < g_Obj[obj].m_Mesh.m_dwNumMaterials; ++i )
                {
                    if( g_Obj[obj].m_Mesh.m_pTextures[i] )
                        V( g_pEffect->SetTexture( "g_txScene", g_Obj[obj].m_Mesh.m_pTextures[i] ) )
                    else
                        V( g_pEffect->SetTexture( "g_txScene", g_pTexDef ) )
                    V( g_pEffect->CommitChanges() );
                    V( pMesh->DrawSubset( i ) );
                }
                V( g_pEffect->EndPass() );

            }
            V( g_pEffect->End() );


			// dibujo los bordes?
			if(!bRenderShadow && 0)
			{
				// si tengo que dibujar los bordes, como no hay efecto y dibujo a traves 
				// del fixed pipeline, tengo que: 
				// Seteo las matrices de transf
				pd3dDevice->SetTransform( D3DTS_PROJECTION,pmProj);
				pd3dDevice->SetTransform( D3DTS_VIEW,pmView);

				int cant_v = pMesh->GetNumVertices();
				int cant_f = pMesh->GetNumFaces();
				size_t bpv = pMesh->GetNumBytesPerVertex();
				BYTE *pVertices=NULL;
				pMesh->LockVertexBuffer(D3DLOCK_READONLY, (LPVOID*)&pVertices); 
				WORD* pIndices=NULL;
				pMesh->LockIndexBuffer(D3DLOCK_READONLY, (LPVOID*)&pIndices); 

				// Seteo todos los states del render para que dibujo las lineas de un color
				// anulo la iluminacion, textura, blending, antialias, etc
				D3DMATERIAL9 mtrl,mtrl_ant;
				ZeroMemory( &mtrl, sizeof(mtrl) );
				BYTE r =255 ,b = 255,g = 128;
				mtrl.Emissive.r = mtrl.Diffuse.r = mtrl.Ambient.r = (double)r/255.0;
				mtrl.Emissive.g = mtrl.Diffuse.g = mtrl.Ambient.g = (double)g/255.0;
				mtrl.Emissive.b = mtrl.Diffuse.b = mtrl.Ambient.b = (double)b/255.0;
				mtrl.Emissive.a = mtrl.Diffuse.a = mtrl.Ambient.a = 0;
				pd3dDevice->GetMaterial( &mtrl_ant );
				pd3dDevice->SetMaterial( &mtrl );
				pd3dDevice->SetTexture( 0, NULL);
				pd3dDevice->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL );

				// seteo la matriz de trans. para este mesh
				pd3dDevice->SetTransform( D3DTS_WORLD, &g_Obj[obj].m_mWorld);

				// dibujo los bordes pp dichas como lineas
				WORD *ndx = new WORD[cant_f*6];
				for(int i=0;i<cant_f;++i)
				{
					ndx[i*6+5] = ndx[i*6] = pIndices[3*i];
					ndx[i*6+2] = ndx[i*6+1] = pIndices[3*i+1];
					ndx[i*6+3] = ndx[i*6+4] = pIndices[3*i+2];
				}

				pd3dDevice->DrawIndexedPrimitiveUP(D3DPT_LINELIST ,0,cant_v,3*cant_f,ndx,D3DFMT_INDEX16,pVertices,bpv);

				pMesh->UnlockVertexBuffer(); 
				pMesh->UnlockIndexBuffer(); 
				delete ndx;

				// Restauro el estado del render
				pd3dDevice->SetMaterial( &mtrl_ant );
			}
        }

        if( !bRenderShadow)
		{
			// dibujar la flechita que representa la posicion de la luz
			if(0)
			{
				V( g_pEffect->SetTechnique( "RenderFlecha" ) );
				D3DXVECTOR3 vUp(0,1,0);
				for(int i=0;i<cant_luces;++i)
				{
					D3DXMATRIXA16 mWorldView;
					D3DXMatrixLookAtLH( &g_LightView, &(LightPos[i]+LightDir[i]), &LightPos[i],&vUp );
					D3DXMatrixInverse( &mWorldView, NULL, &g_LightView);
					D3DXMatrixMultiply( &mWorldView, &mWorldView, pmView );
					V( g_pEffect->SetMatrix( "g_mWorldView", &mWorldView ) );

					UINT cPass;
					LPD3DXMESH pMesh = g_LightMesh.GetMesh();
					V( g_pEffect->Begin( &cPass, 0 ) );
					for( UINT p = 0; p < cPass; ++p )
					{
						V( g_pEffect->BeginPass( p ) );
						for( DWORD i = 0; i < g_LightMesh.m_dwNumMaterials; ++i )
							V( pMesh->DrawSubset( i ) );
						V( g_pEffect->EndPass() );
					}
					V( g_pEffect->End() );
				}
			}

            // Render stats and help text
            RenderText();
            g_HUD.OnRender( fElapsedTime );
		}

        V( pd3dDevice->EndScene() );
    }
}


// Dibuja las vertices, a los efectos de llamar al VS y al PS, para generar los distinos mapas
// Para ello dibuja cada vertice como un punto
//--------------------------------------------------------------------------------------
void ProcesarVertices( IDirect3DDevice9* pd3dDevice,const D3DXMATRIX *pmView, const D3DXMATRIX *pmProj )
{
    HRESULT hr;

	if(primera_vez)
	{
		// solo la primera vez limpia el target, que representa la matriz donde acumulo
		// la radiosidad asociada a cada vertice
		V( pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET,0, 1.0f, 0L ) );
		primera_vez = FALSE;
	}

	// matriz de proyeccion
	V( g_pEffect->SetMatrix( "g_mProj", pmProj ) );

    D3DXMATRIXA16 mIdent;
    D3DXMatrixIdentity( &mIdent );
    // Actualizo la posicion y direccion de "la luz" en el efecto
	// ("la luz" = patch que estoy tomando como emisor en este paso) 
	// ojo que para el radiance necesito todo en coordenadas de World, 
	// independientes del pto de vista
	{
        D3DXVECTOR4 v4 = D3DXVECTOR4(g_LightPos,1);
        V( g_pEffect->SetVector( "g_vLightPos", &v4 ) );
		v4 = D3DXVECTOR4(g_LightDir,0);
        V( g_pEffect->SetVector( "g_vLightDir", &v4 ) );
    }

	if( SUCCEEDED( pd3dDevice->BeginScene() ) )
    {
		pd3dDevice->SetStreamSource( 0, g_pVB, 0, sizeof(VERTEX) );
		pd3dDevice->SetFVF( D3DFVF_VERTEX);
		UINT cPass;
		V( g_pEffect->Begin( &cPass, 0 ) );
		for( UINT p = 0; p < cPass; ++p )
		{
			V( g_pEffect->BeginPass( p ) );

			// Proceso x objeto, ya que cada mesh tiene sus propias matrices 			
			for( int i = 0; i < NUM_OBJ; i++)
			{
				// Genero las matrices de transf. y se las paso al Shader
				D3DXMATRIXA16 mWorldView = g_Obj[i].m_mWorld;
				D3DXMatrixMultiply( &mWorldView, &mWorldView, pmView );
				V( g_pEffect->SetMatrix( "g_mWorldView", &mWorldView ) );
				V( g_pEffect->SetMatrix( "g_mObjWorld", &g_Obj[i].m_mWorld) );

				LPD3DXMESH pMesh = g_Obj[i].m_Mesh.GetMesh();
				int cant_v = pMesh->GetNumVertices() ;
				V( g_pEffect->CommitChanges() );
				pd3dDevice->DrawPrimitive( D3DPT_POINTLIST,g_Obj[i].primer_vertice, cant_v);
			}
			V( g_pEffect->EndPass() );
		}
		V( g_pEffect->End() );
		
		V( pd3dDevice->EndScene() );
    }
}


//--------------------------------------------------------------------------------------
// This callback function will be called at the end of every frame to perform all the 
// rendering calls for the scene, and it will also be called if the window needs to be 
// repainted. After this function has returned, DXUT will call 
// IDirect3DDevice9::Present to display the contents of the next buffer in the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameRender( IDirect3DDevice9* pd3dDevice, double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    HRESULT hr;
	primera_vez = TRUE;		// para que la primera vez borre el mapa de radiosity


	D3DXVECTOR3 vUp(0,1,0);
	if(g_bSolcito)
	{
	    // la primera luz sale del sol
		double an = D3DX_PI * fTime / 6.0f; 
		LightPos[0].x = sinf(an)*15;
		LightPos[0].y = cosf(an)*20;
		LightPos[0].z = 0;

		LightDir[0] =  -LightPos[0] ;
		D3DXVec3Normalize(&LightDir[0],&LightDir[0]);

	}
	else
	{
	    // la primera luz es la que sale de camara g_LCamera
		LightPos[0] = *g_LCamera.GetEyePt();
		LightDir[0] = *g_LCamera.GetLookAtPt() -LightPos[0] ;
		D3DXVec3Normalize(&LightDir[0],&LightDir[0]);
	}

	cant_luces = 1;
	
	// prueba de concepto
	// Calculo el primer rebote con la CPU:
	// la otra opcion es el algoritmo que estoy probando para calcular mapas de promedio 
	// bajando el tamaño del mapa en factores de a 2 en cada paso. 
    if(1)
	{
        D3DXVECTOR3 pRayPos = LightPos[0];
		D3DXVECTOR3 pRayDir[256];
		pRayDir[0]	= LightDir[0];
		D3DXVec3Normalize(&pRayDir[0],&pRayDir[0]);
		for(int t=1;t<50;++t)
			pRayDir[t] = pRayDir[0];
		int k = 1;
		double an = 0.25;
		int Q = 2;
		for(int t=0;t<Q;++t)
		{
			rotar_xy(&pRayDir[k++],an);
			rotar_xy(&pRayDir[k++],-an);
			rotar_xz(&pRayDir[k++],an);
			rotar_xz(&pRayDir[k++],-an);
			rotar_zy(&pRayDir[k++],an);
			rotar_zy(&pRayDir[k++],-an);

			an+=0.25;
		}

		// solo prueba la interseccion con el primer objeto! 
		LPD3DXMESH g_pMesh = g_Obj[0].m_Mesh.m_pMesh;
		// ahora D3DXIntersect Trabaja en el espacio local del objeto, y el rayo 
		// esta en el espacio global. 
		// Calculo la tf inversa y se aplico al rayo
		D3DXMATRIXA16 m;
		D3DXMatrixInverse( &m, NULL, &g_Obj[0].m_mWorld); 
		D3DXVec3TransformCoord(&pRayPos,&pRayPos,&m);

		for(int t=0;t<6*Q+1;++t)
		{
			D3DXVec3TransformNormal(&pRayDir[t],&pRayDir[t],&m);
			BOOL pHit;
			DWORD face;
			float pU,pV,pDist;
			D3DXIntersect(g_pMesh,&pRayPos,&pRayDir[t],&pHit,&face,&pU,&pV,&pDist,NULL,NULL);

			if(pHit)
			{
				// calculo el pto de interseccion
				// paso del espacio del modelo al espacio de la escena
				//D3DXVec3TransformCoord(&pRayPos,&pRayPos,&g_Obj[0].m_mWorld);
				D3DXVec3TransformNormal(&pRayDir[t],&pRayDir[t],&g_Obj[0].m_mWorld);
				LightPos[cant_luces] = LightPos[0] + pRayDir[t]*pDist;
				// tengo que calcular el rayo reflejado
				// v = i - 2 * dot(i, n) * n
				LightDir[cant_luces] = pRayDir[t] - 2 * D3DXVec3Dot(&pRayDir[t],&g_Obj[0].N[face]) *g_Obj[0].N[face];
				cant_luces++;
			}
		}

    } 

	// la primera luz tiene intensidad total (1.0f)
	g_pEffect->SetFloat( "g_fIL", 1.0f);
	// recorro todas las luces, L0 es la primer luz, y las siguientes son los sucesivos
	// rebotes. En esta prueba de concepto estan limitados, por eso hacemos un bucle y listo
	for(int L=0;L<cant_luces;++L)
	{
		// tomo los datos de la fuente emisora Bj
		g_LightPos = LightPos[L];
		g_LightDir = LightDir[L];

		// Primero genero el shadow map, para ello dibujo desde el pto de vista de luz
		// a una textura, con el VS y PS que generan un mapa de profundidades. 
		D3DXMatrixLookAtLH( &g_LightView, &g_LightPos, &(g_LightPos + g_LightDir), &vUp );
		pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE);
		{
			LPDIRECT3DSURFACE9 pOldRT = NULL;
			V( pd3dDevice->GetRenderTarget( 0, &pOldRT ) );
			LPDIRECT3DSURFACE9 pShadowSurf;
			if( SUCCEEDED( g_pShadowMap->GetSurfaceLevel( 0, &pShadowSurf ) ) )
			{
				pd3dDevice->SetRenderTarget( 0, pShadowSurf );
				SAFE_RELEASE( pShadowSurf );
			}
			LPDIRECT3DSURFACE9 pOldDS = NULL;
			if( SUCCEEDED( pd3dDevice->GetDepthStencilSurface( &pOldDS ) ) )
				pd3dDevice->SetDepthStencilSurface( g_pDSShadow );

			{
				RenderScene( pd3dDevice, true, fElapsedTime, &g_LightView, &g_mShadowProj );
				//D3DXSaveTextureToFile(L"test.bmp",D3DXIFF_BMP,g_pShadowMap,NULL);
			}

			if( pOldDS )
			{
				pd3dDevice->SetDepthStencilSurface( pOldDS );
				pOldDS->Release();
			}
			pd3dDevice->SetRenderTarget( 0, pOldRT );
			SAFE_RELEASE( pOldRT );
		}

		// ya tengo el shadow map, ahora calculo el mapa de radiacion
		pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE);
		// Genero e Inicializo una textura que representa la cantidad de luz
		// que ingresa en cada vertice, esta textura funciona como un acumulador 
		// Aca basicamente redirecciono la salida de la GPU a la textura, de 
		// tal forma de usar el pipeline como un procesador
		LPDIRECT3DSURFACE9 pOldRT = NULL;
		V( pd3dDevice->GetRenderTarget( 0, &pOldRT ) );
		LPDIRECT3DSURFACE9 pSurf;
		if( SUCCEEDED( g_pRMap->GetSurfaceLevel( 0, &pSurf ) ) )
		{
			pd3dDevice->SetRenderTarget( 0, pSurf );
			SAFE_RELEASE( pSurf );
		}
		LPDIRECT3DSURFACE9 pOldDS = NULL;
		if( SUCCEEDED( pd3dDevice->GetDepthStencilSurface( &pOldDS ) ) )
			pd3dDevice->SetDepthStencilSurface( NULL);		// no necesito z-buffer
		{
			g_pEffect->SetTechnique( "RadianceMap");
			// Para que se ejecute el VertexShader, necesito forzar una primitiva que lo
			// ejecute, ProcesarVertices llama a dibujar puntos (uno por cada vertices)
			ProcesarVertices( pd3dDevice,&g_LightView, &g_mShadowProj);
			//D3DXSaveTextureToFile(L"test.bmp",D3DXIFF_BMP,g_pRMap,NULL);

		}

		// Restauro el zbuffer y el render target
		if( pOldDS )
		{
			pd3dDevice->SetDepthStencilSurface( pOldDS );
			pOldDS->Release();
		}
		pd3dDevice->SetRenderTarget( 0, pOldRT );
		SAFE_RELEASE( pOldRT );

		if(L==0 && cant_luces>1)    
			// las siguientes luces tienen la intensidad mucho menor, hay que
			// distribuir la luz, de acuerdo a la cantidad de rebotes (la integral = 1) 
			g_pEffect->SetFloat( "g_fIL",1.0f/(float)cant_luces);
			//g_pEffect->SetFloat( "g_fIL",0.25f);

		// paso a la siguiente luz
	}

	// ahora que tengo el mapa de radiance calculado hasta donde pude, 
	// dibujo la escena desde el pto de vista de la camara pp dicha.
	// Esta es la unica tecnica que efectivamente dibuja en la pantalla! 
	pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE);
	g_pEffect->SetTechnique( "RenderScene");
    pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff000000, 1.0f, 0L );
	RenderScene( pd3dDevice, false, fElapsedTime, g_VCamera.GetViewMatrix(), g_VCamera.GetProjMatrix());

    // Render the UI elements
	RenderText();
    g_HUD.OnRender( fElapsedTime );

}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    // The helper object simply helps keep track of text position, and color
    // and then it calls pFont->DrawText( m_pSprite, strMsg, -1, &rc, DT_NOCLIP, m_clr );
    // If NULL is passed in as the sprite object, then it will work however the 
    // pFont->DrawText() will not be batched together.  Batching calls will improves performance.
    CDXUTTextHelper txtHelper( g_pFont, g_pTextSprite, 15 );

    // Output statistics
    txtHelper.Begin();
    txtHelper.SetInsertionPos( 5, 5 );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    txtHelper.DrawTextLine( DXUTGetFrameStats(true) ); // Show FPS
    txtHelper.DrawTextLine( DXUTGetDeviceStats() );
	WCHAR str[256];
    StringCchPrintf(str, 256,L"Cant.Rayos = %d", cant_luces);
    txtHelper.DrawTextLine(str );
    txtHelper.SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    txtHelper.End();
}


//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera and dialogs so they can respond to user input
    if( WM_KEYDOWN != uMsg || g_bRightMouseDown )
        g_LCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    if( WM_KEYDOWN != uMsg || !g_bRightMouseDown )
    {
        if( g_bCameraPerspective )
            g_VCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
        else
            g_LCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// As a convenience, DXUT inspects the incoming windows messages for
// keystroke messages and decodes the message parameters to pass relevant keyboard
// messages to the application.  The framework does not remove the underlying keystroke 
// messages, which are still passed to the application's MsgProc callback.
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
}


void CALLBACK MouseProc( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, int xPos, int yPos, void* pUserContext )
{
    g_bRightMouseDown = bRightButtonDown;
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_MOVER_SOL:
			g_bSolcito = !g_bSolcito;
			break;
	}
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// entered a lost state and before IDirect3DDevice9::Reset is called. Resources created
// in the OnResetDevice callback should be released here, which generally includes all 
// D3DPOOL_DEFAULT resources. See the "Lost Devices" section of the documentation for 
// information about lost devices.
//--------------------------------------------------------------------------------------
void CALLBACK OnLostDevice( void* pUserContext )
{
    g_DialogResourceManager.OnLostDevice();
    g_SettingsDlg.OnLostDevice();
    if( g_pFont )
        g_pFont->OnLostDevice();
    if( g_pFontSmall )
        g_pFontSmall->OnLostDevice();
    if( g_pEffect )
        g_pEffect->OnLostDevice();
    SAFE_RELEASE(g_pTextSprite);

    SAFE_RELEASE( g_pDSShadow );
    SAFE_RELEASE( g_pShadowMap );
    SAFE_RELEASE( g_pTexDef );
	SAFE_RELEASE( g_pRMap );
	SAFE_RELEASE( g_pVB );

    for( int i = 0; i < NUM_OBJ; ++i )
        g_Obj[i].m_Mesh.InvalidateDeviceObjects();
    g_LightMesh.InvalidateDeviceObjects();
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnCreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnDestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnDestroyDevice();
    g_SettingsDlg.OnDestroyDevice();
    SAFE_RELEASE( g_pEffect );
    SAFE_RELEASE( g_pFont );
    SAFE_RELEASE( g_pFontSmall );
    SAFE_RELEASE( g_pVertDecl );

    SAFE_RELEASE( g_pEffect );
	SAFE_RELEASE( g_pVB );

    for( int i = 0; i < NUM_OBJ; ++i )
        g_Obj[i].m_Mesh.Destroy();
    g_LightMesh.Destroy();
}




HRESULT OptimizarMesh( ID3DXMesh** ppInOutMesh )
{
	HRESULT hr;
	ID3DXMesh* pInputMesh = *ppInOutMesh;

	ID3DXMesh* pTempMesh = NULL;
	DWORD* rgdwAdjacency = new DWORD[pInputMesh->GetNumFaces() * 3];
	if( rgdwAdjacency == NULL )
		return E_OUTOFMEMORY;
	DWORD* rgdwAdjacencyOut = new DWORD[pInputMesh->GetNumFaces() * 3];
	if( rgdwAdjacencyOut == NULL )
	{
		delete []rgdwAdjacency;
		return E_OUTOFMEMORY;
	}

	V( pInputMesh->GenerateAdjacency(1e-6f,rgdwAdjacency) );
	V( pInputMesh->Optimize( D3DXMESHOPT_ATTRSORT, 
						rgdwAdjacency, rgdwAdjacencyOut, NULL, NULL, &pTempMesh ) );
	SAFE_RELEASE( pInputMesh );
	pInputMesh = pTempMesh;

	if( rgdwAdjacency == NULL )
		return E_OUTOFMEMORY;
	V( pInputMesh->Optimize( D3DXMESHOPT_VERTEXCACHE | D3DXMESHOPT_IGNOREVERTS, 
						rgdwAdjacencyOut, NULL, NULL, NULL, &pTempMesh ) );

	delete []rgdwAdjacency;
	delete []rgdwAdjacencyOut;
	SAFE_RELEASE( pInputMesh );
	*ppInOutMesh = pTempMesh;

	return S_OK;
}
