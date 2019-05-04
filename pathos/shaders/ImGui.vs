#include "shaderlib/ShaderOutput.hlsli"

struct ImGui_Vtx
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD;
    float4 col : COLOR;
};

struct CBufferData
{
    float4x4 orthoMtx;
};

ConstantBuffer<CBufferData> g_cbuf : register(b0);

VSOut_XUC main(in ImGui_Vtx _vtx)
{
    VSOut_XUC ret;
    ret.pos = mul(float4(_vtx.pos, 0.0, 1.0), g_cbuf.orthoMtx);
    ret.uv =  _vtx.uv;
    ret.col = _vtx.col;
    return ret;
}