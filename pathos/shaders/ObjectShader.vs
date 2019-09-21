#include "shaderlib/CommonShared.h"
#include "shaderlib/ShaderOutput.hlsli"
#include "shaderlib/DefinesShared.h"

#include "shaderlib/GFXPerFrameBindings.hlsli"

StructuredBuffer<InstanceData_Xform> g_instanceData_Transform : register(t0, PATHOS_PER_VIEW_SPACE);


VSOut_ObjectFull_Material main(uint _instanceId_meshId : TEXCOORD, uint _vid : SV_VertexID)
{
    VSOut_ObjectFull_Material ret = (VSOut_ObjectFull_Material)0;

    uint instanceDataIdx = _instanceId_meshId & PATHOS_INSTANCE_ID_REMAP_MASK;
    uint submeshId = _instanceId_meshId >> PATHOS_SUBMESH_ID_REMAP_SHIFT;

    GPUSubMeshData subMeshData = g_subMeshData[submeshId];

    uint vtxOffset = subMeshData.unifiedVertexBufferOffset + _vid;

    TangentSpace tangentSpace = g_unifiedVtxTangent[vtxOffset];

    uint materialIdx = subMeshData.materialIdx;

    InstanceData_Xform instanceXform = g_instanceData_Transform[instanceDataIdx];

    float3 pos = g_unifiedVtxPos[vtxOffset];
    float3 modelPos = TransformInstanceData(float4(pos, 1), instanceXform.row0, instanceXform.row1, instanceXform.row2);

    float4 viewPos = mul(float4(modelPos, 1), g_frameCb.mainViewProj);

    ret.pos = viewPos;
    ret.posWS = modelPos; 
    ret.uv =  g_unifiedVtxUv[vtxOffset];
    ret.viewDepth = viewPos.w;


    ret.normal = TransformInstanceData(float4(tangentSpace.normal, 0), instanceXform.row0, instanceXform.row1, instanceXform.row2);

    float3 tangent = TransformInstanceData(float4(tangentSpace.tangentSign.xyz, 0), instanceXform.row0, instanceXform.row1, instanceXform.row2);
    ret.tangentSign = float4(tangent, tangentSpace.tangentSign.w);
    ret.materialIdx = materialIdx;

    return ret;
}