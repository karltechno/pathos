#include "shaderlib/ShaderOutput.hlsli"

struct TestData
{
    float4x4 mvp;
};

ConstantBuffer<TestData> g_data : register(b0, space0);

VSOut_ObjectFull main(in VSIn_ObjectFull _input)
{
    VSOut_ObjectFull ret;
    ret.pos = mul(float4(_input.pos, 1.0), g_data.mvp);
    ret.posWS = _input.pos; // need model mtx
    ret.uv = _input.uv;
    ret.normal = _input.normal;
    ret.tangentSign = _input.tangentSign;
    return ret;
}