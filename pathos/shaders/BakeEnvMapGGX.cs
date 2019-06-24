#include "shaderlib/Sampling.hlsli"
#include "shaderlib/Constants.hlsli"
#include "shaderlib/GlobalSamplers.hlsli"

TextureCube g_inCube : register(t0, space0);
RWTexture2DArray<float4> g_outGGXMap : register(u0, space0);

struct CBuf
{
    float2 inDimsMip0;
    float2 outDims;

    float rough2;
};

ConstantBuffer<CBuf> g_cbuf : register(b0, space0);

float3 TangentToWorld(float3 p, float3 T, float3 B, float3 N)
{
    return p.x * T + p.y * B + p.z * N;
}

[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    const uint NUM_SAMPLES = 1024;
    const float RCP_SAMPLES = rcp(float(NUM_SAMPLES));

    float3 N = UV_To_CubeMap(DTid.xy / g_cbuf.outDims, DTid.z);
    float3 T, B;

    ConstructBasisAround(N, T, B);

    float3 colAccum = 0.;
    float weight = 0.;

    float texelSolidAngle = 4.0 * kPi / (6.0 * g_cbuf.inDimsMip0.x * g_cbuf.inDimsMip0.y);

    for(uint i = 0; i < NUM_SAMPLES; ++i)
    {
        float2 rng = Hammersley(i, RCP_SAMPLES);
        float3 H = TangentToWorld(SampleGGX(rng.x, rng.y, g_cbuf.rough2), T, B, N);

        // Incident light direction
        float3 L = 2. * dot(H, N) * H - N;
        float n_dot_l = dot(N, L);

        if(n_dot_l > 0.)
        {
            float pdf = GGX_NDF(saturate(dot(N, H)), g_cbuf.rough2) * 0.25;
            // https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch20.html
            float sampleSolidAngle = rcp(pdf*NUM_SAMPLES);
            float mip = max(0., 1. + log2(sampleSolidAngle / texelSolidAngle)); // sqrt(log2(ssa/tsa))
            colAccum += g_inCube.SampleLevel(g_samplerLinearWrap, N, mip).xyz;
            weight += n_dot_l;
        }
    }

    g_outGGXMap[DTid] = float4(colAccum / weight, 1.);
}