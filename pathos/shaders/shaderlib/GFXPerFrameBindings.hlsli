#ifndef GFX_PER_FRAME_BINDINGS_INCLUDED
#define GFX_PER_FRAME_BINDINGS_INCLUDED

#include "LightingCommon.hlsli"
#include "CommonShared.h"
#include "DefinesShared.h"

TextureCube<float4> g_irrad                 :   register(t0, PATHOS_PER_FRAME_SPACE);
TextureCube<float4> g_ggxEnv                :   register(t1, PATHOS_PER_FRAME_SPACE);
Texture2D<float4> g_ggxLut                  :   register(t2, PATHOS_PER_FRAME_SPACE);
StructuredBuffer<LightData> g_lights        :   register(t3, PATHOS_PER_FRAME_SPACE);
Texture2DArray<float> g_shadowCascades      :   register(t4, PATHOS_PER_FRAME_SPACE);
StructuredBuffer<MaterialData> g_materials  :   register(t5, PATHOS_PER_FRAME_SPACE);

StructuredBuffer<float3> g_unifiedVtxPos                :   register(t6, PATHOS_PER_FRAME_SPACE);
StructuredBuffer<TangentSpace> g_unifiedVtxTangent      :   register(t7, PATHOS_PER_FRAME_SPACE);
StructuredBuffer<float2> g_unifiedVtxUv                 :   register(t8, PATHOS_PER_FRAME_SPACE);

ConstantBuffer<FrameConstants> g_frameCb    :   register(b0, PATHOS_PER_FRAME_SPACE);
Texture2D<float4> g_bindlessTexArray[]      :   register(t0, PATHOS_CUSTOM_SPACE);

#endif