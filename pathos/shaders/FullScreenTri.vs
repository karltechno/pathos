#include "shaderlib/ShaderOutput.hlsli"

VSOut_XU main(uint vid : SV_VertexID)
{
    VSOut_XU ret; 
    ret.pos.x = (float)(vid >> 1) * 4.0 - 1.0;
    ret.pos.y = (float)(vid & 1) * 4.0 - 1.0;
    ret.pos.z = 0.0;
    ret.pos.w = 1.0;

    ret.uv.x = (float)(vid >> 1) * 2.0;
    ret.uv.y = 1.0 - (float)(vid & 1) * 2.0;

    return ret;
}