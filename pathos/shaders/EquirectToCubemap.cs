#include "Shaderlib/Constants.hlsli"
#include "Shaderlib/GlobalSamplers.hlsli"
#include "Shaderlib/Sampling.hlsli"

Texture2D<float4> g_inTex : register(t0, space0);
RWTexture2DArray<float4> g_outCube : register(u0, space0);


[numthreads(32, 32, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID) 
{
    float2 texDims;
    float elems;
    g_outCube.GetDimensions(texDims.x, texDims.y, elems);
    float3 dir = UV_To_CubeMap(DTid.xy / texDims, DTid.z);

    // Cartesian to spherical
    float theta = atan2(dir.z, dir.x);
    float phi = acos(dir.y);
    g_outCube[DTid] = g_inTex.SampleLevel(g_samplerLinearWrap, float2(theta/k2Pi, phi/kPi), 0);
}

