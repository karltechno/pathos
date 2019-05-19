#ifndef LIGHTING_STRUCTS_H
#define LIGHTING_STRUCTS_H
#include "CPPInterop.h"

SHADERLIB_NAMESPACE_BEGIN

struct LightData
{
    float3 posWS;
    float rcpRadius;
    float3 color;
    float spotInnerCos;
    float3 direction;
    float spotOuterCos;
    
	float intensity;
	float3 _pad0_;
};

#define MAX_LIGHTS 64

struct TestLightCBuffer
{
    LightData lights[MAX_LIGHTS];
    float3 sunColor;
    uint numLights;
    float3 sunDir;
    float _pad0_;
    float3 camPos;
};

SHADERLIB_NAMESPACE_END

#endif // LIGHTING_STRUCTS_H