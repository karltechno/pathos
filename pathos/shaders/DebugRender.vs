#include "shaderlib/ShaderOutput.hlsli"
#include "shaderlib/CommonShared.h"
#include "shaderlib/DefinesShared.h"
#include "shaderlib/GFXPerFrameBindings.hlsli"

struct VSInput
{
    float3 pos : POSITION;
    float4 col : COLOR;
};


VSOut_PosCol main(VSInput _input)
{
    VSOut_PosCol ret;
    ret.pos = mul(float4(_input.pos, 1.0), g_frameCb.mainViewProj);
    ret.posWS = _input.pos;
    ret.col = _input.col;
    return ret;
}