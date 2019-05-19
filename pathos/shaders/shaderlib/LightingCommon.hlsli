#ifndef LIGHTING_COMMON_H
#define LIGHTING_COMMON_H
#include "LightingStructs.h"
#include "Constants.hlsli"

// References
// https://seblagarde.files.wordpress.com/2015/07/course_notes_moving_frostbite_to_pbr_v32.pdf
// https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf 
// https://disney-animation.s3.amazonaws.com/library/s2012_pbs_disney_brdf_notes_v2.pdf 
// https://google.github.io/filament/Filament.md.html
// http://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html 


// Cook torrance microfacet specular model:
//
//            D(h) F(v, h) G(l, v, h)
// f(l, v) = ----------------------------
//             4 (n_dot_l)(n_dot_v)
//
// D = Normal distribution function. 
// G = Geometric shadowing function.
// F = Fresnel.


struct SurfaceData
{
    float3 posWS;
    float3 norm;
    float3 baseCol;
    float roughness;
    float metalness;

    float3 f0;
};


float3 F_Schlick(float3 f0, float v_dot_h)
{
    return f0 + (1.0 - f0) * pow(1.0 - v_dot_h, 5.0);
}

float D_GGX(float n_dot_h, float rough2)
{
    float denom = (n_dot_h * n_dot_h) * (rough2 - 1.0) + 1.0;
    return rough2 / (denom * denom * kPi);
}

float G_Smith(float n_dot_l, float n_dot_v, float rough2)
{
	float Lambda_GGXV = n_dot_l * sqrt((-n_dot_v * rough2 + n_dot_v) * n_dot_v + rough2);
	float Lambda_GGXL = n_dot_v * sqrt((-n_dot_l * rough2 + n_dot_l) * n_dot_l + rough2);

	return 0.5 / (Lambda_GGXV + Lambda_GGXL);
}

float3 SpecularTerm_GGX
(
    float n_dot_l,
    float n_dot_v, 
    float n_dot_h,
    float v_dot_h,
    in SurfaceData surface
)
{
    float rough2 = surface.roughness * surface.roughness;
    // rough2 *= rough2;
    
    float3 F = F_Schlick(surface.f0, v_dot_h);
    float G = G_Smith(n_dot_l, n_dot_v, rough2);
    float D = D_GGX(n_dot_h, rough2);
    return (G*D)*F;
}

float3 DiffuseTerm_Lambert(in SurfaceData surface)
{
    return surface.baseCol * ((1.0 - surface.metalness) / kPi);
}

float3 ComputeLighting_Common(in float3 lightColor, in SurfaceData surface, in float3 L, in float3 V)
{
    float3 H = normalize(V + L);
    float n_dot_h = saturate(dot(surface.norm, H));
    float n_dot_v = saturate(abs(dot(surface.norm, V) + 1e-5));
    float v_dot_h = saturate(dot(H, V));
    float n_dot_l = saturate(dot(surface.norm, L));

    float3 specularTerm = SpecularTerm_GGX(n_dot_l, n_dot_v, n_dot_h, v_dot_h, surface);
    float3 diffuseTerm = DiffuseTerm_Lambert(surface);
    return n_dot_l * lightColor * (diffuseTerm + specularTerm); 
}


float PointLightAtten(float lightDist, float lightDistSq, float lightRadiusRcp)
{
    float numerator = saturate(1.0 - pow(lightDist * lightRadiusRcp, 4.0));
    return (numerator * numerator) * rcp(lightDistSq + 1.0);
}

float3 ComputeLighting_Point(in LightData light, in SurfaceData surface, in float3 V)
{
    float3 L = light.posWS - surface.posWS;
    float L_lenSq = dot(L, L);
    float L_len = sqrt(L_lenSq);  
    L *= rcp(L_len);

    return light.intensity * PointLightAtten(L_len, L_lenSq, light.rcpRadius) * ComputeLighting_Common(light.color, surface, L, V); 
}

#endif // LIGHTING_COMMON_H