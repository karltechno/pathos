#include "shaderlib/GlobalSamplers.hlsli"
#include "shaderlib/Constants.hlsli"
#include "shaderlib/Sampling.hlsli"

TextureCube<float4> g_inRadiance : register(t0, space0);
RWTexture2DArray<float4> g_outIrradiance : register(u0, space0);

float3x3 MakeBasis(float3 normal)
{
    float3 up = abs(normal.z) < 0.99 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0); 
    float3 right = normalize(cross(normal, up));
    up = cross(normal, right);
    return float3x3(right, up, normal); 
}



[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 texDims;
    float elems;
    g_outIrradiance.GetDimensions(texDims.x, texDims.y, elems);
    float2 uv = (DTid.xy / texDims);

    float3x3 basis = MakeBasis(UV_To_CubeMap(uv, DTid.z));

    const int NUM_SAMPLES = 8 * 1024;
    const float NUM_SAMPLES_RCP = rcp(NUM_SAMPLES);

    float3 irrad = 0.xxx;

    for(int i = 0; i < NUM_SAMPLES; ++i)
    {
        float2 rng = Hammersley(i, NUM_SAMPLES_RCP);
        float3 sampleDir = mul(SampleHemisphere_Cosine(rng.x, rng.y), basis);
        float3 c = g_inRadiance.SampleLevel(g_samplerLinearWrap, sampleDir, 4).rgb;

        irrad += 1. - exp2(-c);
    }

    irrad *= NUM_SAMPLES_RCP;
    g_outIrradiance[DTid] = float4(irrad, 1.0);
}