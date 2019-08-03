#ifndef SHADER_OUTPUT_INCLUDED
#define SHADER_OUTPUT_INCLUDED

struct VSIn_ObjectFull
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangentSign : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSIn_ObjectFullInstanced
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangentSign : TANGENT;
    float2 uv : TEXCOORD0;

    float3 instanceCol0 : TEXCOORD1;
    float3 instanceCol1 : TEXCOORD2;
    float3 instanceCol2 : TEXCOORD3;
    float3 instanceCol3 : TEXCOORD4;
};

struct VSIn_ObjectPosInstanced
{
    float3 pos : POSITION;

    float3 instanceCol0 : TEXCOORD0;
    float3 instanceCol1 : TEXCOORD1;
    float3 instanceCol2 : TEXCOORD2;
    float3 instanceCol3 : TEXCOORD3;
};

struct VSOut_Pos
{
    float4 pos  : SV_Position;
    float3 posWS : POSITION;
};

struct VSOut_PosCol
{
    float4 pos : SV_Position;
    float3 posWS : POSITION;
    float4 col : COLOR;
};

struct VSOut_PosUV
{
    float4 pos  : SV_Position;
    float3 posWS : POSITION;
    float2 uv   : TEXCOORD0;
};

struct VSOut_PosUVCol
{
    float4 pos  : SV_Position;
    float3 posWS : POSITION;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR;
};

struct VSOut_ObjectFull
{
    float4 pos : SV_Position;
    float3 posWS : POSITION;
    float viewDepth : TEXCOORD1;
    float2 uv : TEXCOORD2;
    float3 normal : NORMAL;
    float4 tangentSign : TANGENT;
};

float3 ReconstructBitangent(in float3 _normal, in float4 _tangent)
{
    return normalize(cross(_normal, _tangent.xyz) * _tangent.w);
}

#endif // SHADER_OUTPUT_INCLUDED