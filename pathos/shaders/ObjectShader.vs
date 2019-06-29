#include "shaderlib/CommonShared.h"
#include "shaderlib/ShaderOutput.hlsli"


ConstantBuffer<BatchConstants> g_batch : register(b0, space0);
ConstantBuffer<FrameConstants> g_frame : register(b0, space1);

float3 TransformInstanceData_Point(VSIn_ObjectFullInstanced _input, float3 _p)
{
    float3 ret = _input.instanceCol3;
    ret += _input.instanceCol0 * _p.x;
    ret += _input.instanceCol1 * _p.y;   
    ret += _input.instanceCol2 * _p.z;    
    return ret;
}


float3 TransformInstanceData_Dir(VSIn_ObjectFullInstanced _input, float3 _p)
{
    float3 ret = _input.instanceCol0 * _p.x;
    ret += _input.instanceCol1 * _p.y;   
    ret += _input.instanceCol2 * _p.z;    
    return ret;
}

VSOut_ObjectFull main(in VSIn_ObjectFullInstanced _input)
{
    float3 modelPos = TransformInstanceData_Point(_input, _input.pos);

    VSOut_ObjectFull ret;
    ret.pos = mul(float4(modelPos, 1.0), g_frame.mainViewProj);
    ret.posWS = modelPos; 
    ret.uv = _input.uv;

    ret.normal = TransformInstanceData_Dir(_input, _input.normal);
    ret.tangentSign = float4(TransformInstanceData_Dir(_input, _input.tangentSign.xyz), _input.tangentSign.w);
    
    return ret;
}