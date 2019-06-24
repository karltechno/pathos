#include "ShaderLib/Sampling.hlsli"
#include "ShaderLib/LightingCommon.hlsli"

RWTexture2D<float4> g_outGGXLut : register(u0, space0);

// Bake GGX LUT as described in UE4 split sum approximation 
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf


float2 IntegrateGGX(float roughness, float n_dot_v)
{
    float3 V;
    V.x = sqrt(1. - n_dot_v * n_dot_v); // sin identity
    V.y = 0.;
    V.z = n_dot_v; // cos

    float2 texel = 0.;

    const int NUM_SAMPLES = 1024;
    const float RCP_SAMPLES = 1. / float(NUM_SAMPLES);

    float r2 = roughness * roughness;
    r2 *= r2; // remap to pow(r, 4);

    for(int i = 0; i < NUM_SAMPLES; ++i)
    {
        float2 rng = Hammersley(i, RCP_SAMPLES);
        float3 H = SampleGGX(rng.x, rng.y, r2);
        float3 L = 2. * dot(V, H) * H - V;
        
        // N is Z axis.
        float n_dot_l = saturate(L.z);
        float n_dot_h = saturate(H.z);
        float v_dot_h = saturate(dot(V, H));

        if(n_dot_l > 0.)
        {
            float G = G_Smith(n_dot_l, n_dot_v, r2);
            float G_Vis = n_dot_l * G * (4.0f * v_dot_h / n_dot_h);
            
            float F = pow(1. - v_dot_h, 5.);
            texel.x += (1. - F) * G_Vis;
            texel.y += F * G_Vis;
        }
    }

    return texel * RCP_SAMPLES;
}

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float2 dims;
    g_outGGXLut.GetDimensions(dims.x, dims.y);

    float2 uv = DTid.xy / dims;

    g_outGGXLut[DTid.xy] = float4(IntegrateGGX(uv.y, max(uv.x, 0.001)), 0., 0.);
}