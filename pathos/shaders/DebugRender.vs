#include "shaderlib/ShaderOutput.hlsli"
#include "shaderlib/CommonShared.h"

struct VSInput
{
    float3 pos : POSITION;
    float4 col : COLOR;
};

ConstantBuffer<FrameConstants> g_frame : register(b0, space1);

VSOut_PosCol main(VSInput _input)
{
    VSOut_PosCol ret;
    ret.pos = mul(float4(_input.pos, 1.0), g_frame.mainViewProj);
    ret.posWS = _input.pos;
    ret.col = _input.col;
    return ret;
}