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


SHADERLIB_NAMESPACE_END

#endif // LIGHTING_STRUCTS_H