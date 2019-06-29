#include "shaderlib/CommonShared.h"
#include "shaderlib/ShaderOutput.hlsli"


ConstantBuffer<BatchConstants> g_batch : register(b0, space0);
ConstantBuffer<FrameConstants> g_frame : register(b0, space1);


VSOut_ObjectFull main(in VSIn_ObjectFull _input)
{
    float4 wsPos = mul(float4(_input.pos, 1.0), g_batch.modelMtx);
    VSOut_ObjectFull ret;
    ret.pos = mul(wsPos, g_frame.mainViewProj);
    ret.posWS = wsPos.xyz; // need model mtx
    ret.uv = _input.uv;
    ret.normal = _input.normal;
    ret.tangentSign = _input.tangentSign;
    return ret;
}