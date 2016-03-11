
// nota :
// revisar esto
//Can not render to a render target that is also used as a texture


#define MAP_SIZE 512
#define SHADOWMAP_SIZE 256
#define EPSILON 0.0001f
#define RAYTMAP_SIZE 64


// nota: convenciones de las variables
// las globales para efectos empiezan con g_ luego viene el tipo y el nombre de la variable
// m = matrices 
// v = vector 
// f = float
// tx = textura
// sam = samplers


float4x4 g_mWorldViewProj;
float4x4 g_mWorldView;

texture  g_txScene;		// textura para la escena
texture  g_txShadow;	// textura para el shadow map

float3   g_vLightPos;  // posicion de la luz (en World Space) = pto que representa patch emisor Bj 
float3   g_vLightDir;  // Direcion de la luz (en World Space) = normal al patch Bj

float4   g_vLightColor;  //
float4   g_vColorFlecha;  //
float4   g_vColorObj;		// Se usar para el cornellbox
bool	cornell_box;

float4x4 g_mObjWorld;	// matrix world del objeto 
float   g_fIL = 1.0;	// Intensidad luminosa del patch actual

texture  g_txRMap;		// Mapa de Radiance
						// Cada texel esta relacionado con un vertice
						// el valor del mismo representa la cantidad de luz acumulada en ese patch
texture  g_txRMap2;		// Mapa de Radiance secundario
texture  g_txRMap3;		// Mapa de Radiance auxiliar

texture  g_txRayT;		// Mapa de Intersecciones

float g_fTime = 0;
// antialiasing del shadow map
int smap_aa = 0;
int aa_radio = 1;
int dpenumbra = 2;

float dcono = 0.1;		// tamaño del cono de luz

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
    Texture = <g_txRMap>;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler2D g_samRMap2 =
sampler_state
{
    Texture = <g_txRMap2>;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler2D g_samRMap3 =
sampler_state
{
    Texture = <g_txRMap3>;
    MinFilter = POINT;
    MagFilter = POINT;
    MipFilter = NONE;
    AddressU = Clamp;
    AddressV = Clamp;
};

sampler2D g_samRayT =
sampler_state
{
    Texture = <g_txRayT>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = NONE;
};

Texture g_txZBuffer;
sampler2D RawZSampler =
sampler_state
{
    Texture = <g_txZBuffer>;
    MinFilter = NONE;
    MagFilter = NONE;
    MipFilter = NONE;
};

float DS = RAYTMAP_SIZE/2.0;
// Textura de entrada
texture  g_txDatos;
sampler2D g_samDatos =
sampler_state
{
    Texture = <g_txDatos>;
    MinFilter = NONE;
    MagFilter = NONE;
    MipFilter = NONE;
    AddressU = Clamp;
    AddressV = Clamp;
    
};

//-----------------------------------------------------------------------------
// Vertex Shader para dibujar la escena pp dicha
//-----------------------------------------------------------------------------
void VertScene( float4 iPos : POSITION,
                float2 iTex : TEXCOORD0,
                float2 iId : TEXCOORD1,
				out float4 oPos : POSITION,                
				out float4 Diffuse : COLOR0,
                out float2 Tex : TEXCOORD0,
                out float2 Id : TEXCOORD1)
{
    // transformacion Standard
    oPos = mul( iPos, g_mWorldViewProj);
    float4 Q = mul(iPos, g_mWorldView); 
	oPos.xy = Q.xy / length(Q.xyz) * oPos.w;     
	
    
    // obtengo el valor de radiance asociado al vertice
	Diffuse = tex2Dlod( g_samRMap, float4(iId/MAP_SIZE,0,0))
			+ tex2Dlod( g_samRMap2, float4(iId/MAP_SIZE,0,0))
			+ tex2Dlod( g_samRMap3, float4(iId/MAP_SIZE,0,0));
	
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
	if(cornell_box)
		return g_vColorObj*Diffuse*0.7;
	else
		return tex2D( g_samScene, Tex )*Diffuse;
}



//-----------------------------------------------------------------------------
// Vertex y Pixel Shader para dibujar la flechita que representa el rayo de luz 
//-----------------------------------------------------------------------------
void VertFlecha( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                out float4 oPos : POSITION)
{
    oPos = mul( iPos, g_mWorldViewProj );
}

float4 PixFlecha( float4 vPos : TEXCOORD1 ) : COLOR
{
    return g_vColorFlecha;
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
    oPos = mul( Pos, g_mWorldViewProj );
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
void VertMapR(	float4 Pos : POSITION,
				float3 iNormal : NORMAL,
				float2 iId : TEXCOORD1,
				out float4 Diffuse : COLOR0,
				out float4 oPos : POSITION,
				out float2 oId : TEXCOORD1 )
{

	float I = 0;			// intensidad de flujo de luz
	
	// nota: todos los calculos se realizan en el World Space
	// Necesito la posicion del vertice real (en World Space)
	// para ello multiplico por la matrix World asociada al objeto
	// vPos = pos real del vertice en el World
    float4 vPos = mul( Pos, g_mObjWorld);
    
    // nota: para acelerar podria ya tener calculada la normal x el objworld 
    iNormal = normalize(mul( iNormal, g_mObjWorld));
    
    float3 vRayo_ij = normalize( float3( vPos - g_vLightPos ) );
    // vRayo_ij es el vector que va desde la posicion del patch Bj (que representa una luz) 
    // hasta el vertice actual, que estoy calulando que es el patch Bi
    
    // verifico que el vertice este del lado correcto del hemisferio y calculo el form factor
    float cono = dot( vRayo_ij, g_vLightDir);
    if(cono>dcono)
	{
	
		// ecuacion de Radiosity
		// (j es la superficie emisora y i es la que esta recibiendo la luz)
		// Bi = Ei + ri Sum (Bj * Fij)
		// Bi = Radiosidad de la superficie i
		// Ei = Emision superficie i
		// ri = reflectividad de la sup i
		// Bj = Radiosidad de la superficie j
		// Fij = Form factor de la superficie j relativa a la superficie i
		
		// se consideran que la superficies i es lo suf. pequeña, y la j es un infenitesimo.
		// hay que verlo como un punto. 
		
		// en nuestro caso en el primer paso: 
		// Ei = 0 para todos las superficies, menos para la primer luz 
		// luego, Ei sale de la textura que usamos para almacenar la radiosidad
		// ri = 1 y a otra cosa
		// Bi = text2d(indice2d(i),g_txRMap) 
		
		// y el form factor se calcula asi: 
		// si vRayo_ij es el vector que une el pto en la superficie i con la superficie emisora j
		// Fij = cos(an_i)*(an_j)/(pi*||vRayo_ij||)*Vij
		// an_i = angulo entre la normal de la superficie i y el vector vRayo_ij (an_j = idem j)
		// Vij es un factor de visibilidad, que indica si la superficie j que emite la luz 
		// es visible desde la punto i, y por lo tanto se definie como
		// Vij = 1 si j es visible desde i, o 0 caso contrario
		
		// en nuestro caso:
		// la division por pi, surge de integrar la ecuacion en el area, y lo eliminamos directamente
		// (no queremos un resultado exacto, eso se usa cuando se necesita el valor exacto de 
		// radicion en un cierto punto)
		// de la misma forma la norma de vRayo_ij es muy cara de conseguir y la vamos a desestimar
		// (responde a que la intesidad de la luz decrece con el cuadrado de la distancia) 
		
		// por otra parte, como no podemos calcular todas las luces se produce que en general
		// la imagen queda mas oscura de lo que deberia, y por eso multiplico por 2 la ecuacion
		// con lo cual nuestro form factor simplificado queda asi (sin el factor de visibilidad)
		// Fij = 2*cos(an_i)*(an_j)
		// Este ff depende de la orientacion  relativa de ambas superficies
		
		float ff = saturate(2*dot(-vRayo_ij,iNormal)*dot(g_vLightDir,vRayo_ij));
	
		// para calcular el V(i,j) tengo que acceder al shadow map
		// se supone que estoy dibujando desde el pto de vista de la luz
		float4 vPosLight = mul( Pos, g_mWorldViewProj );

		// transformo del espacio de la pantalla al espacio de la textura para poder acceder al mapa
        float2 auxCoord = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
        auxCoord.y = 1.0f - auxCoord.y;
        
        // leo el shadow map pp dicho
		if(smap_aa)
		{
			// anti-aliasing del shadowmap
			float I2 = 0;
			for(int i=-aa_radio;i<=aa_radio;++i)
				for(int j=-aa_radio;j<=aa_radio;++j)
					I2 += (tex2Dlod( g_samShadow, float4(auxCoord + float2((float)(i*dpenumbra)/SHADOWMAP_SIZE, (float)(j*dpenumbra)/SHADOWMAP_SIZE),0,0) )
							 + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
			I2 /= (2*aa_radio+1)*(2*aa_radio+1);
			I = I*0.2 + I2*0.8;
		}
		else
			// sin anti-aliasing : una sola muestra
			I = (tex2Dlod( g_samShadow, float4(auxCoord,0,0)) + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
		
        I*=ff;
	}

	// Artificio para usar una textura como una matriz para almacenar datos
	// necesito que la posicion proyectada, "caiga" en la pos i,j de pantalla, 
	// que representa luego la textura
    oPos.x = 2*iId.x/MAP_SIZE-1;
    oPos.y = 1-2*iId.y/MAP_SIZE;
    oPos.z = 1;
    oPos.w = 1;
    
  	// Propago el id
    oId = iId;
    
    // Propago I, usando el color asociado al vertice, ademas multiplico x 
    // la intensidad de la luz actual
    Diffuse = I*g_vLightColor*g_fIL;
	
}

void PixMapR( float4 Diffuse: COLOR0,float2 id : TEXCOORD1,out float4 Color : COLOR0 )
{
	Color = tex2D( g_samRMap, id/MAP_SIZE) + Diffuse;
}


//-----------------------------------------------------------------------------
// Vertex Shader que sirve para reemplazar el raytracing de los rebotes de la luz
//-----------------------------------------------------------------------------
void VertRayT(	float4 iPos : POSITION,
                float2 iId : TEXCOORD1,
				out float4 oPos : POSITION,
                out float2 oId : TEXCOORD1)
{
	// transformacion estandard 
    oPos = mul( iPos, g_mWorldViewProj );
	// propago el Id
    oId = iId;
}

void PixRayT( float4 Diffuse : COLOR0,float2 iId : TEXCOORD1,out float4 Color : COLOR )
{
	// el formato de la textura de salida es D3DFMT_G16R16 o D3DFMT_A16B16G16R16F
    Color.rg = iId;
    float4 E = tex2D( g_samRMap, iId/MAP_SIZE);
	Color.b = (E.r + E.g + E.b)*0.333;
    //Color.b = tex2D( g_samRMap, iId/MAP_SIZE).r;
    Color.a = 0;
}



//-----------------------------------------------------------------------------
void VertMax(	float4 Pos : POSITION,
				float2 Tex : TEXCOORD0,
				out float4 oPos : POSITION,
				out float2 oTex : TEXCOORD0 )
{
	// Corrijo la pos. para usar siempre el mismo tamano de textura
	// quiero que la parte usada quede arriba a la izquierda 
	double K = DS/RAYTMAP_SIZE;
	oPos.x = Pos.x*K-1+K;
	oPos.y = Pos.y*K+1-K;
	oPos.z = Pos.z;
	oPos.w = 1;
	// Propago la textura
	oTex = Tex;

}


//-----------------------------------------------------------------------------
// calcula el maximo de los 4 pixeles vecinos
void PixMax( float2 Tex : TEXCOORD0 , out float4 Color : COLOR0 )
{
	Color = tex2D( g_samDatos, Tex);
	float4 r1 = tex2D( g_samDatos, Tex + float2(1/DS,0));
	float4 r2 = tex2D( g_samDatos, Tex + float2(0,1/DS));
	float4 r3 = tex2D( g_samDatos, Tex + float2(1/DS,1/DS));
	if(r1.b>Color.b)
		Color = r1;
	if(r2.b>Color.b)
		Color = r2;
	if(r3.b>Color.b)
		Color = r3;
}

//-----------------------------------------------------------------------------
void VSZBuffer(	float4 Pos : POSITION,
				float2 Tex : TEXCOORD0,
				out float4 oPos : POSITION,
				out float2 oTex : TEXCOORD0 )
{
	oPos = Pos;
	oPos.z = 0;
	oPos.w = 1;
	// Propago la textura
	oTex = Tex;
}

void PSZBuffer( float2 Tex : TEXCOORD0 , out float4 Color : COLOR0 )
{
	Color.rgb = 0;
	float z = dot(tex2D(RawZSampler, Tex).arg,
	float3(0.996093809371817670572857294849,
	0.0038909914428586627756752238080039,
	1.5199185323666651467481343000015e-5));	
	Color = tex2D(RawZSampler, Tex);
	Color.a = 1;
}



// dibuja la escena pp dicha
//-----------------------------------------------------------------------------
technique RenderScene
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertScene();
        PixelShader = compile ps_3_0 PixScene();
    }
}

// ShadowMap estandard, para calcular el factor de visibilidad V(i,j)
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
//-----------------------------------------------------------------------------
technique RadianceMap
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertMapR();
        PixelShader = compile ps_3_0 PixMapR();
    }
}


// dibuja la flechita, que representa el rayo casteado
//-----------------------------------------------------------------------------
technique RenderFlecha
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertFlecha();
        PixelShader = compile ps_3_0 PixFlecha();
    }
}

// RayTray
//-----------------------------------------------------------------------------
technique RayTracing
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertRayT();
        PixelShader = compile ps_3_0 PixRayT();
    }
}

// Calcula el maximo de un mapa
//-----------------------------------------------------------------------------
technique CalcMax
{
    pass p0
    {
        VertexShader = compile vs_2_0 VertMax();
        PixelShader = compile ps_2_0 PixMax();
    }
}

// Direct Zbuffer


//-----------------------------------------------------------------------------
technique ZBuffer
{
    pass p0
    {
        VertexShader = compile vs_3_0 VSZBuffer();
        PixelShader = compile ps_3_0 PSZBuffer();
    }
}
