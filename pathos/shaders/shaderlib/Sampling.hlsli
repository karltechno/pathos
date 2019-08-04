#ifndef SAMPLING_H
#define SAMPLING_H
#include "Constants.hlsli"

float3 UV_To_CubeMap(float2 _uv, uint _face)
{
    float2 xy = (float2(_uv.x, 1.0 - _uv.y) * 2.0) - float2(1.0, 1.0);
    float3 uvRet;
    // https://docs.microsoft.com/en-us/windows/desktop/direct3d9/cubic-environment-mapping
    switch(_face)
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
    // Identity: sin^2 + cos^2 = 1
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    return float3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
}

float3 SampleHemisphere_Cosine(float u1, float u2)
{
    float phi = u2 * k2Pi;
    float cosTheta = sqrt(1.0 - u1);
    // Identity: sin^2 + cos^2 = 1
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    return float3(cosPhi * sinTheta, sinPhi * sinTheta, cosTheta);
}

// Derivation: https://www.tobias-franke.eu/log/2014/03/30/notes_on_importance_sampling.html
float3 SampleGGX(float u1, float u2, float roughness2)
{
    float cosTheta = sqrt( (1. - u2) / (1. + (roughness2 - 1) * u2) );
    float phi = k2Pi * u1;

    // Identity: sin^2 + cos^2 = 1
    float sinTheta = sqrt(1. - cosTheta*cosTheta);
    float sinPhi, cosPhi;
    sincos(phi, sinPhi, cosPhi);
    return float3(sinTheta * cosPhi, sinTheta * sinPhi, cosTheta);
}

float GGX_NDF(float l_dot_h, float roughness2)
{
    float denom = (l_dot_h * l_dot_h) * (roughness2 - 1.0) + 1.0;
	return roughness2 / (kPi * denom * denom);
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

// http://lolengine.net/blog/2013/09/21/picking-orthogonal-vector-combing-coconuts
float3 PerpVec(float3 v)
{
    // TODO: Branchless
    return abs(v.x) > abs(v.y) ? float3(-v.y, v.x, 0.) : float3(0., -v.z, v.y);
}

void ConstructBasisAround(in float3 N, out float3 T, out float3 B)
{
    T = normalize(PerpVec(N));
    B = normalize(cross(T, N));
}

#endif // SAMPLING_H