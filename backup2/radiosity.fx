
#define MAP_SIZE 512
#define SHADOWMAP_SIZE 256
#define EPSILON 0.00005f


// nota: convenciones de las variables
// las globales para efectos empiezan con g_ luego viene el tipo y el nombre de la variable
// m = matrices 
// v = vector 
// f = float
// tx = textura
// sam = samplers


float4x4 g_mWorldView;
float4x4 g_mProj;

texture  g_txScene;		// textura para la escena
texture  g_txShadow;	// textura para el shadow map

float3   g_vLightPos;  // posicion de la luz (en World Space) = pto que representa patch emisor Bj 
float3   g_vLightDir;  // Direcion de la luz (en World Space) = normal al patch Bj

float4   g_vLightColor;  //
float4   g_vColorFlecha;  //

float4x4 g_mObjWorld;	// matrix world del objeto 
float   g_fIL = 1.0;	// Intensidad luminosa del patch actual

texture  g_txRMap;		// Mapa de Radiance
						// Cada texel esta relacionado con un vertice
						// el valor del mismo representa la cantidad de luz acumulada en ese patch

texture  g_txRayT;		// Mapa de Intersecciones

float g_fTime = 0;
int ondas = 0;
int smap_aa = 0;

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


sampler2D g_samRayT =
sampler_state
{
    Texture = <g_txRayT>;
    MinFilter = Point;
    MagFilter = Point;
    MipFilter = NONE;
};

//-----------------------------------------------------------------------------
// Vertex Shader para dibujar la escena pp dicha
//-----------------------------------------------------------------------------
void VertScene( float4 iPos : POSITION,
                float3 iNormal :  NORMAL,
                float2 iTex : TEXCOORD0,
                float2 iId : TEXCOORD1,
				out float4 oPos : POSITION,                
				out float4 Diffuse : COLOR0,
                out float2 Tex : TEXCOORD0,
                out float2 Id : TEXCOORD1)
{


    // tx ondas
    /*
    //if(ondas==1)
    {
		float a0 = 4*g_fTime;
		float k = 3.1415*0.5;
		float r = (sin(a0+k*iPos.x) + sin(a0+k*iPos.z))*0.1;
		iPos += float4(iNormal,0)*r;
	}
    
    if(ondas==2)
    {
		iPos.y += 0.1*sin(8*g_fTime);
	}*/
	
    if(ondas==3)
    {
		float a0 = 5*g_fTime;
		float k = 3.1415*0.5;
		iPos.x += sin(a0+k*iPos.y);
		float r = (sin(a0+k*iPos.x) + sin(a0+k*iPos.y) + sin(a0+k*iPos.z))*0.1;
		iPos += float4(iNormal,0)*r;
		
	}

    // transformo al view space
    float4 vPos = mul( iPos, g_mWorldView );
    // y ahora a screen space
    oPos = mul( vPos, g_mProj );
    
    // obtengo el valor de radiance asociado al vertice
    // (ojo que la textura tiene un formato D3DFMT_R32F, 
    // con lo cual el valor es un float32 en el canal R
	Diffuse = tex2Dlod( g_samRMap, float4(iId/MAP_SIZE,0,0));
	
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

	if(ondas==3)
		return tex2D( g_samScene, Tex );
	else
		return tex2D( g_samScene, Tex )*Diffuse;

	//return Diffuse;
}



//-----------------------------------------------------------------------------
// Vertex y Pixel Shader para dibujar la flechita que representa el rayo de luz 
//-----------------------------------------------------------------------------
void VertFlecha( float4 iPos : POSITION,
                float3 iNormal : NORMAL,
                out float4 oPos : POSITION)
{
    oPos = mul( iPos, g_mWorldView );
    oPos = mul( oPos, g_mProj );
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
    if(cono>0.401)
	{
	
		// ecuacion de Radiosity
		// (j es la superficie emisora y i es la que esta recibiendo la luz)
		// Bi = Ei + ri Sum (Bj * Fij)
		// Bi = Radiosidad de la superficie i
		// Ei = Emision superficie i
		// ri = reflectividad de la sup i
		// Bj = Radiosidad de la superficie j
		// Fij = Form factor de la superficie j relativa a la superficie i
		
		// se consideran que la superficies i es lo suf. peque�a, y la j es un infenitesimo.
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
		float4 vPosLight = mul( Pos, g_mWorldView );
		vPosLight = mul( vPosLight, g_mProj );

		// transformo del espacio de la pantalla al espacio de la textura para poder acceder al mapa
        float2 auxCoord = 0.5 * vPosLight.xy / vPosLight.w + float2( 0.5, 0.5 );
        auxCoord.y = 1.0f - auxCoord.y;
        
        // leo el shadow map pp dicho
        I = (tex2Dlod( g_samShadow, float4(auxCoord,0,0)) + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
        
        
		// interpolacion standard bi-lineal del shadow map
		/*
        // auxCoord va de 0 a 1, lo multiplico x el tama�o de la textura
        // la parte fraccionaria indica cuanto tengo que tomar del vecino 
        float2 vecino = frac( auxCoord*MAP_SIZE);
        // leo 4 valores
        float s0 = (tex2Dlod( g_samShadow, float4(auxCoord,0,0)) + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
        float s1 = (tex2Dlod( g_samShadow, float4(auxCoord+ float2(1.0/SHADOWMAP_SIZE,0),0,0)) 
							+ EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
        float s2 = (tex2Dlod( g_samShadow, float4(auxCoord+ float2(0,1.0/SHADOWMAP_SIZE),0,0)) 
							+ EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
        float s3 = (tex2Dlod( g_samShadow, float4(auxCoord+ float2(1.0/SHADOWMAP_SIZE,1.0/SHADOWMAP_SIZE),0,0)) 
							+ EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
        I = lerp( lerp( s0, s1, vecino.x ),lerp( s2, s3, vecino.x ),vecino.y);
        */

		/*
		float z_ocluder = tex2Dlod( g_samShadow, float4(auxCoord,0,0));
		float z_receiver = vPosLight.z / vPosLight.w;
		float dz = z_receiver - z_ocluder;
		float r = dz*1000;
		*/

		// penumbra
		/*
		float r = 10;
        
        if(r>0)
        {
			I = 0.25*((tex2Dlod( g_samShadow, float4(auxCoord,0,0)) + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f);  
			I += 0.25*((tex2Dlod( g_samShadow, float4(auxCoord + float2((float)r/SHADOWMAP_SIZE, (float)r/SHADOWMAP_SIZE),0,0) )
								 + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f);  
			I += 0.25*((tex2Dlod( g_samShadow, float4(auxCoord + float2((float)r/SHADOWMAP_SIZE, 0),0,0) )
								 + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f);  
			I += 0.25*((tex2Dlod( g_samShadow, float4(auxCoord + float2(0, (float)r/SHADOWMAP_SIZE),0,0) )
								 + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f);  
		}
		else
			I = (tex2Dlod( g_samShadow, float4(auxCoord,0,0)) + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
		*/

		if(smap_aa)
		{
			// anti-aliasing del shadowmap
			float I2 = 0;
			float r = 1;
			for(int i=-r;i<=r;++i)
				for(int j=-r;j<=r;++j)
					I2 += (tex2Dlod( g_samShadow, float4(auxCoord + float2((float)(i*2)/SHADOWMAP_SIZE, (float)(j*2)/SHADOWMAP_SIZE),0,0) )
							 + EPSILON < vPosLight.z / vPosLight.w)? 0.0f: 1.0f;  
			I2 /= (2*r+1)*(2*r+1);
			I = I*0.2 + I2*0.8;
		}

        //I*=pow(ff,2);
        I*=ff;
        //I = ff*ff;
        
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
				float3 iNormal : NORMAL,
                float2 iId : TEXCOORD1,
				out float4 oPos : POSITION,
				//out float4 Diffuse : COLOR0,
                out float2 oId : TEXCOORD1)
{
	// transformacion estandard 
    float4 vPos = mul( iPos, g_mWorldView );
    oPos = mul( vPos, g_mProj );
    
    /*
    iNormal = normalize(mul( iNormal, g_mObjWorld));
    float I = dot(iNormal,-g_vLightDir);
    // prueba para ver que sale
	Diffuse.rgb = I;
	Diffuse.a = 1;
	*/
    
	// propago el Id
    oId = iId;
}

void PixRayT( float4 Diffuse : COLOR0,float2 iId : TEXCOORD1,out float4 Color : COLOR )
{
	// el formato de la textura de salida es D3DFMT_G16R16 o D3DFMT_A16B16G16R16F
	// por eso divido el id por 65535
    //Color = Diffuse;
    Color.rg = iId;
	Color.b = tex2D( g_samRMap, iId/MAP_SIZE).r;
    Color.a = 0;
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

// ShadowMap estandard, para calcular el factor de visibilidad V(i,j)
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
technique RadianceMap
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertMapR();
        PixelShader = compile ps_3_0 PixMapR();
    }
}


// dibuja la flechita, que representa el rayo casteado
technique RenderFlecha
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertFlecha();
        PixelShader = compile ps_3_0 PixFlecha();
    }
}

// RayTray
technique RayTracing
{
    pass p0
    {
        VertexShader = compile vs_3_0 VertRayT();
        PixelShader = compile ps_3_0 PixRayT();
    }
}

