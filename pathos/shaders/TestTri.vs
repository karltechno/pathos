#include "lib/ShaderOutput.hlsli"

struct TestData
{
    float4 time;
    row_major float4x4 mvp;
};

ConstantBuffer<TestData> g_data : register(b0, space0);

VSOut_XU main(float3 inPos : POSITION)
{
    VSOut_XU ret;
    ret.pos = mul(float4(inPos + float3(0.0, 0.0, 2.0), 1.0), g_data.mvp);
    ret.uv = inPos.xy * 0.5 + 0.5;
    return ret;
}