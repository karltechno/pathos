#ifndef SHADER_OUTPUT_INCLUDED
#define SHADER_OUTPUT_INCLUDED

struct VSOut_XU
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
};

struct VSOut_XUC
{
    float4 pos  : SV_Position;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR;
};


#endif // SHADER_OUTPUT_INCLUDED