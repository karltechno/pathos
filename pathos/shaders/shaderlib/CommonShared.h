#ifndef COMMON_SHARED_H
#define COMMON_SHARED_H
#include "CPPInterop.h"

#define PATHOS_MAX_SHADOW_CASCADES 4

SHADERLIB_NAMESPACE_BEGIN

struct FrameConstants
{
    float4x4 mainViewProj;
    float4x4 mainProj;
    float4x4 mainView;
    float4x4 mainInvView;

	float4x4 cascadeMatrices[PATHOS_MAX_SHADOW_CASCADES];
	float4 cascadeSplits;

    float3 camPos;
    float camDist; // distance from origin

    float2 screenDims;
    float2 screenDimsRcp;

    float3 sunDir; float __pad0__;
    float3 sunColor; 
	
	uint numLights;

	// x = time, y = time/10, z = dt, w = ? 
	float4 time;
};

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

struct MaterialData
{
    float4 baseColour;
    float roughness;
    float metalness;
    float alphaCutoff;
    float _pad_;
};

struct BatchConstants
{
    uint materialIdx;
};

SHADERLIB_NAMESPACE_END

#endif // COMMON_SHARED_H