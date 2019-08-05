#ifndef COMMON_SHARED_H
#define COMMON_SHARED_H
#include "CPPInterop.h"

#define PATHOS_MAX_SHADOW_CASCADES 4

SHADERLIB_NAMESPACE_BEGIN

#define PATHOS_LIGHT_TYPE_POINT (0)
#define PATHOS_LIGHT_TYPE_SPOT  (1)

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

    uint numPointLights; // point lights sorted first
    uint numSpotLights; // spot lights sorted second
    float2 __pad1__; 

	// x = time, y = time/10, z = dt, w = ? 
	float4 time;
};

struct LightData
{
    float3 posWS;
    float rcpRadius;

    float3 color;
	uint type;

    float3 direction;
	float intensity;
    
    // Scale and offset as described in https://google.github.io/filament/Filament.md.html
    float2 spotParams; float2 __pad0__;
};

struct MaterialData
{
    float4 baseColour;
    float roughness;
    float metalness;
    float alphaCutoff;
    uint albedoTexIdx;
    uint normalMapTexIdx;
    uint metalRoughTexIdx;
    uint occlusionTexIdx;
};

struct BatchConstants
{
    uint materialIdx;
};

SHADERLIB_NAMESPACE_END

#endif // COMMON_SHARED_H