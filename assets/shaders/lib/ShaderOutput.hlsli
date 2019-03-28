#ifndef SHADER_OUTPUT_INCLUDED
#define SHADER_OUTPUT_INCLUDED

struct VSOut_PosUV
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
};

#endif // SHADER_OUTPUT_INCLUDED