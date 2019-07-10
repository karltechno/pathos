#ifndef GLOBAL_SAMPLERS_INCLUDED
#define GLOBAL_SAMPLERS_INCLUDED

SamplerState g_samplerPointClamp    : register(s0);
SamplerState g_samplerPointWrap     : register(s1);

SamplerState g_samplerLinearClamp   : register(s2);
SamplerState g_samplerLinearWrap    : register(s3);

SamplerState g_samplerAnisoWrap     : register(s4);

SamplerComparisonState g_samplerCmp : register(s5);
#endif // GLOBAL_SAMPLERS_INCLUDED