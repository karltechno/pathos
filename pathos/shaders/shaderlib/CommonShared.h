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

	float4x4 cascadeMatricies[PATHOS_MAX_SHADOW_CASCADES];
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

// TODO: Temp - testing 
struct BatchConstants
{
    float4x4 modelMtx;
};


SHADERLIB_NAMESPACE_END

#endif // COMMON_SHARED_H