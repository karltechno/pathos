#include "shaderlib/GlobalSamplers.hlsli"
#include "shaderlib/Constants.hlsli"

TextureCube<float4> g_inRadiance : register(t0, space0);
RWTexture2DArray<float4> g_outIrradiance : register(u0, space0);


float3x3 MakeBasis(float3 normal)
{
    float3 up = abs(normal.z) < 0.99 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0); 
    float3 right = normalize(cross(normal, up));
    up = cross(normal, right);
    return float3x3(right, up, normal); 
}


float3 GetSampleDir(uint3 dtid)
{
    float2 texDims;
    float elems;
    g_outIrradiance.GetDimensions(texDims.x, texDims.y, elems);
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

// http://www.pbr-book.org/3ed-2018/Monte_Carlo_Integration/2D_Sampling_with_Multidimensional_Transformations.html
float3 SampleHemisphere_Uniform(float u1, float u2)
{
    float phi = u2 * k2Pi;
    float cosTheta = 1.0 - u1;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    return float3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
}

float3 SampleHemisphere_Cosine(float u1, float u2)
{
    float phi = u2 * k2Pi;
    float cosTheta = sqrt(1.0 - u1);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    return float3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
}

// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float RadicalInverse_VdC(uint bits) 
{
    return float(reversebits(bits)) * 2.3283064365386963e-10; // 0x100000000
}

float2 Hammersley(uint i, float rcpNumSamples)
{
	return float2(i * rcpNumSamples, RadicalInverse_VdC(i));
}

[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float3x3 basis = MakeBasis(GetSampleDir(DTid));

    const int NUM_SAMPLES = 8 * 1024;
    const float NUM_SAMPLES_RCP = rcp(NUM_SAMPLES);

    float3 irrad = 0.xxx;

    for(int i = 0; i < NUM_SAMPLES; ++i)
    {
        float2 rng = Hammersley(i, NUM_SAMPLES_RCP);
        float3 sampleDir = mul(SampleHemisphere_Cosine(rng.x, rng.y), basis);
        float3 c = g_inRadiance.SampleLevel(g_samplerLinearWrap, sampleDir, 0).rgb;

        irrad += 1. - exp2(-c);
    }

    irrad *= NUM_SAMPLES_RCP;
    g_outIrradiance[DTid] = float4(irrad, 1.0);
}