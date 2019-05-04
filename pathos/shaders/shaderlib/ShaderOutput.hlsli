#ifndef SHADER_OUTPUT_INCLUDED
#define SHADER_OUTPUT_INCLUDED

struct VSOut_XU
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
};

struct VSOut_XUC
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR;
};

struct VSIn_ObjectFull
{
    float3 pos : POSITION;
    float3 normal : NORMAL;
    float4 tangentSign : TANGENT;
    float2 uv : TEXCOORD;
};

struct VSOut_ObjectFull
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float4 tangentSign : TANGENT;
};

float3 ReconstructBitangent(in float3 _normal, in float4 _tangent)
{
    return cross(_normal, _tangent.xyz) * _tangent.w;
}

#endif // SHADER_OUTPUT_INCLUDED