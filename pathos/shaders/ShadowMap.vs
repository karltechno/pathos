#include "shaderlib/CommonShared.h"
#include "shaderlib/DefinesShared.h"
#include "shaderlib/ShaderOutput.hlsli"

#include "shaderlib/GFXPerFrameBindings.hlsli"

struct ShadowMtx
{
    float4x4 viewProj;
};

ConstantBuffer<ShadowMtx> g_viewCb : register(b0, PATHOS_PER_VIEW_SPACE);

StructuredBuffer<InstanceData_Xform> g_instanceData_xform : register(t0, PATHOS_PER_VIEW_SPACE);

float4 main(uint _instanceId_meshId : TEXCOORD0, uint _vid : SV_VertexID) : SV_Position
{
    uint instanceDataIdx = _instanceId_meshId & PATHOS_INSTANCE_ID_REMAP_MASK;
    uint submeshId = _instanceId_meshId >> PATHOS_SUBMESH_ID_REMAP_SHIFT;

    InstanceData_Xform instanceData = g_instanceData_xform[instanceDataIdx];
    uint vtxOffset = g_subMeshData[submeshId].unifiedVertexBufferOffset + _vid;
    float3 pos = g_unifiedVtxPos[vtxOffset];

    float3 ws = TransformInstanceData(float4(pos, 1), instanceData.row0, instanceData.row1, instanceData.row2);
    float4 p = mul(float4(ws, 1.0), g_viewCb.viewProj);
    p.z = max(p.z, 0.);
    return p;
}