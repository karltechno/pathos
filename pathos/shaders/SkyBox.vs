#include "ShaderLib/ShaderOutput.hlsli"

struct DummyCBuffer
{
    float4x4 mtx;
};

ConstantBuffer<DummyCBuffer> g_cbuf : register(b0);

VSOut_Pos main(in float3 _pos : POSITION)
{
    VSOut_Pos ret;
    _pos *= 100.;
    ret.pos = mul(float4(_pos, 1.0), g_cbuf.mtx);
    ret.posWS = _pos;
    return ret;
}