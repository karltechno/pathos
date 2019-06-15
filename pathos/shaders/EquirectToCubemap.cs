#include "Shaderlib/Constants.hlsli"
#include "Shaderlib/GlobalSamplers.hlsli"

Texture2D<float4> g_inTex : register(t0, space0);
RWTexture2DArray<float4> g_outCube : register(u0, space0);


float3 GetSampleDir(uint3 dtid)
{
    float2 texDims;
    float elems;
    g_outCube.GetDimensions(texDims.x, texDims.y, elems);
    float2 uv = (dtid.xy / texDims);
    float2 xy = (float2(uv.x, 1.0 - uv.y) * 2.0) - float2(1.0, 1.0);
    float3 uvRet;
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d9/cubic-environment-mapping
    switch(dtid.z)
    {
        // +X
        case 0: uvRet = float3(1.0, xy.y, -xy.x); break;
        // -X
        case 1: uvRet = float3(-1.0, xy.y, xy.x); break;
        // +Y
        case 2: uvRet = float3(xy.x, 1.0, -xy.y); break;
        // -Y
        case 3: uvRet = float3(xy.x, -1.0, xy.y); break;
        // +Z
        case 4: uvRet = float3(xy.x, xy.y, 1.0); break;
        // -Z
        case 5: uvRet = float3(-xy.x, xy.y, -1.0); break;
    }
    return normalize(uvRet);
}

[numthreads(32, 32, 1)]
void main(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID) 
{
    float3 dir = GetSampleDir(DTid);
    // Cartesian to spherical
    float theta = atan2(dir.z, dir.x);
    float phi = acos(dir.y);
    g_outCube[DTid] = g_inTex.SampleLevel(g_samplerLinearWrap, float2(theta/k2Pi, phi/kPi), 0);
}

