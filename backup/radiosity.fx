
#define MAP_SIZE 512
#define EPSILON 0.00005f


float4x4 g_mWorldView;
float4x4 g_mProj;

texture  g_txScene;		// textura para la escena
texture  g_txShadow;	// textura para el shadow map

float3   g_vLightPos;  // posicion de la luz (en World Space)
float3   g_vLightDir;  // Direcion de la luz (en World Space)
float    g_fCosTheta;  // si quiero un spot light

float4x4 g_mObjWorld;	// matrix world del objeto 
float    g_mIL = 1.0f;			// Intensidad luminosa del patch actual

texture  g_RMap;		// Mapa de Radiance
						// Cada texel esta relacionado con un vertice
						// el valor del mismo representa la cantidad de luz acumulada en ese patch

sampler2D g_samScene =
sampler_state
{
    Texture = <g_txScene>;
    MinFilter = Point;
    MagFilter = Linear;
    MipFilter = Linear;
};


sampler2D g_samShadow =
sampler_state
{
    Texture = <g_txShadow>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = NONE;
    AddressU = Clamp;
    AddressV = Clamp;
};


sampler2D g_samRMap =
sampler_state
{
    Texture = <g_RMap>;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = Clamp;
    AddressV = Clamp;
};


//-----------------------------------------------------------------------------
// Vertex Shader para dibujar la escena pp dicha
//-----------------------------------------------------------------------------
void VertScene( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                float2 iTex : TEXCOORD0,
                float2 iId : TEXCOORD1,
				out float4 oPos : POSITION,                
				out float4 Diffuse : COLOR0,
                out float2 Tex : TEXCOORD0,
                out float2 Id : TEXCOORD1)
{
    // transformo al view space
    float4 vPos = mul( iPos, g_mWorldView );
    // y ahora a screen space
    oPos = mul( vPos, g_mProj );

    // Compute view space normal
    float3 vNormal = mul( iNormal, (float3x3)g_mWorldView );
    
    // obtengo el valor de radiance asociado al vertice
	Diffuse = tex2Dlod( g_samRMap, float4(iId/MAP_SIZE,0,0)).x;

	// propago coordenadas de textura y Id
    Tex = iTex;
    Id = iId;
	
}



//-----------------------------------------------------------------------------
// Pixel Shader para dibujar la escena
//-----------------------------------------------------------------------------
float4 PixScene(	float4 Diffuse : COLOR0,
					float2 Tex : TEXCOORD0,
					float4 vId : TEXCOORD1) : COLOR
{
	
	return tex2D( g_samScene, Tex )*Diffuse;
}



//-----------------------------------------------------------------------------
// Vertex y Pixel Shader para dibujar la flechita que representa el rayo de luz 
//-----------------------------------------------------------------------------
void VertLight( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                out float4 oPos : POSITION)
{
    oPos = mul( iPos, g_mWorldView );
    oPos = mul( oPos, g_mProj );
}

float4 PixLight( float4 vPos : TEXCOORD1 ) : COLOR
{
    return float4(1,1,0,0);
}


//-----------------------------------------------------------------------------
// Vertex Shader que implementa un shadow map. Lo necesito para calcular
// el valor de la funcion de visibilidad de la ecuacion de radiosity
//-----------------------------------------------------------------------------
void VertShadow( float4 Pos : POSITION,
                 float3 Normal : NORMAL,
                 out float4 oPos : POSITION,
                 out float2 Depth : TEXCOORD0 )
{
	// transformacion estandard 
    oPos = mul( Pos, g_mWorldView );
    oPos = mul( oPos, g_mProj );
    // devuelvo: profundidad = z/w 
    Depth.xy = oPos.zw;
}


//-----------------------------------------------------------------------------
// Pixel Shader para el shadow map, dibuja la "profundidad" 
//-----------------------------------------------------------------------------
void PixShadow( float2 Depth : TEXCOORD0,out float4 Color : COLOR )
{
    Color = Depth.x / Depth.y;
}


//-----------------------------------------------------------------------------
// Vertex Shader para procesar cada vertice. Calcula la ecuacion de radiosity 
//-----------------------------------------------------------------------------
void VertMapR( float4 Pos : POSITION,
               float3 iNormal : NORMAL,
               float2 iId : TEXCOORD1,
				out float4 Diffuse : COLOR0,
               out float4 oPos : POSITION,
               out float2 oId : TEXCOORD1 )
{

	float4 I = 0;			// intensidad de flujo de luz
	
	// Necesito la posicion del vertice real (en World Space)
	// para ello multiplico por la matrix World asociada al objeto
    float4 vPos = mul( Pos, g_mObjWorld);
	// vPos = pos real del vertice en el World
    float3 vLight = normalize( float3( vPos - g_vLightPos ) );
    // vLight apunta al pixel actual desde la posicion de la luz
    // verifico que el vertice este del lado correcto del semi-hemisferio
    // y calculo el form factor
    if(saturate(dot( vLight, g_vLightDir)>0.01)
	{
		float ff = dot(iNormal,vLigth)*dot(iNormal,vLigth)
	
		// para calcular el H(i,j) tengo que acceder al shadow map
		
		// se supone que estoy dibujando desde el pto de vista de la luz
		float4 vPosLight = mul( Pos, g_mWorldView );
		vPosLight = mul( vPosLight, g_mProj );

		// transformo del espacio de la pantalla al espacio de la textura para poder acceder al mapa
        float2 ShadowTexC = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
        ShadowTexC.y = 1.0f - ShadowTexC.y;
        
        // leo el shadow map pp dicho
        I = (tex2Dlod( g_samShadow, float4(ShadowTexC,0,0)) + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
        
        /*
			// anti-aliasing del shadowmap
			I = 0;
			float r = 2;
			for(int i=-r;i<=r;++i)
				for(int j=-r;j<=r;++j)
					I += (tex2Dlod( g_samShadow, float4(ShadowTexC + float2((float)i/MAP_SIZE, (float)j/MAP_SIZE),0,0) )
							 + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
			I /= (2*r+1)*(2*r+1);
		*/
        
        I*=ff;
	}

	// Artificio para usar una textura como una matriz para almacenar datos
	// necesito que la posicion proyectada, "caiga" en la pos i,j de pantalla, que representa luego la textura
    oPos.x = 2*iId.x/MAP_SIZE-1;
    oPos.y = 1-2*iId.y/MAP_SIZE;
    oPos.z = 1;
    oPos.w = 1;
    
  	// Propago el id
    oId = iId;
    
    // Propago I, usando el color asociado al vertice, ademas multiplico x 
    // la intensidad de la luz actual
    Diffuse = I*g_mIL;

}

void PixMapR( float4 Diffuse: COLOR0,float2 id : TEXCOORD1,out float4 Color : COLOR0 )
{
	Color = tex2D( g_samRMap, id/MAP_SIZE) + Diffuse;
}

//-----------------------------------------------------------------------------
// Technique: RenderScene
// Desc: Renders scene objects
//-----------------------------------------------------------------------------
technique RenderScene
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertScene();
        PixelShader = compile ps_3_0 PixScene();
    }
}




//-----------------------------------------------------------------------------
// dibuja la flechita, que representa el rayo casteado
//-----------------------------------------------------------------------------
technique RenderLight
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertLight();
        PixelShader = compile ps_3_0 PixLight();
    }
}




//-----------------------------------------------------------------------------
// Technique: RenderShadow
// Desc: Renders the shadow map
//-----------------------------------------------------------------------------
technique RenderShadow
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertShadow();
        PixelShader = compile ps_3_0 PixShadow();
    }
}

// Paso del algoritmo de Radiosity. Por cada vertice, se calcula la cantidad de energia
// que entra, y se acumula con el valor anterior. Generando asi un mapa de "Radiacion"
//
technique RadianceMap
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertMapR();
        PixelShader = compile ps_3_0 PixMapR();
    }
}

