// Upgrade NOTE: replaced 'mul(UNITY_MATRIX_MVP,*)' with 'UnityObjectToClipPos(*)'

//========= Copyright 2015-2019, HTC Corporation. All rights reserved. ===========

Shader "Unlit/YUV2RGBA"
{
	Properties
	{
		_MainTex ("Texture", 2D) = "black" {}
		_YTex("Y channel", 2D) = "black" {}
		_UTex("U channel", 2D) = "gray" {}
		_VTex("V channel", 2D) = "gray" {}
	}
	SubShader
	{
		Tags { "RenderType"="Opaque" }
		LOD 100

		Pass
		{
			CGPROGRAM
			#pragma vertex vert
			#pragma fragment frag
			// make fog work
			#pragma multi_compile_fog
			
			#include "UnityCG.cginc"

			struct appdata
			{
				float4 vertex : POSITION;
				float2 uv : TEXCOORD0;
			};

			struct v2f
			{
				float2 uv : TEXCOORD0;
				UNITY_FOG_COORDS(1)
				float4 vertex : SV_POSITION;
			};

			sampler2D _MainTex;
			sampler2D _YTex;
			sampler2D _UTex;
			sampler2D _VTex;
			float4 _MainTex_ST;
			
			v2f vert (appdata v)
			{
				v2f o;
				o.vertex = UnityObjectToClipPos(v.vertex);
				o.uv = TRANSFORM_TEX(float2(v.uv.x, 1.0 - v.uv.y), _MainTex);
				UNITY_TRANSFER_FOG(o,o.vertex);
				return o;
			}
			
			fixed4 frag (v2f i) : SV_Target
			{
				float ych = tex2D(_YTex, i.uv).a;
				float uch = tex2D(_UTex, i.uv).a * 0.872 - 0.436;		//	Scale from 0 ~ 1 to -0.436 ~ +0.436
				float vch = tex2D(_VTex, i.uv).a * 1.230 - 0.615;		//	Scale from 0 ~ 1 to -0.615 ~ +0.615
				/*	BT.601	*/
				float rch = ych + 1.13983 * vch;
				float gch = ych - 0.39465 * uch - 0.58060 * vch;
				float bch = ych + 2.03211 * uch;
				
				fixed4 col = clamp(fixed4(rch, gch, bch, 1.0), 0.0, 1.0);

				if(!IsGammaSpace()) {	//	If linear space.
					col = pow(col, 2.2);
				}

				UNITY_APPLY_FOG(i.fogCoord, col);
				return col;
			}
			ENDCG
		}
	}
}
