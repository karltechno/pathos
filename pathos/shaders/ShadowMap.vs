#include "shaderlib/ShaderOutput.hlsli"

struct ShadowMtx
{
    float4x4 viewProj;
};

ConstantBuffer<ShadowMtx> g_cb : register(b0, space0);

float3 TransformInstanceData_Point(VSIn_ObjectPosInstanced _input, float3 _p)
{
    float3 ret = _input.instanceCol3;
    ret += _input.instanceCol0 * _p.x;
    ret += _input.instanceCol1 * _p.y;   
    ret += _input.instanceCol2 * _p.z;    
    return ret;
}

float4 main(VSIn_ObjectPosInstanced _in) : SV_Position
{
    float3 ws = TransformInstanceData_Point(_in, _in.pos);
    float4 p = mul(float4(ws, 1.0), g_cb.viewProj);
    p.z = max(p.z, 0.);
    return p;
}