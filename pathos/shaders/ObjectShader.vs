#include "shaderlib/CommonShared.h"
#include "shaderlib/ShaderOutput.hlsli"
#include "shaderlib/DefinesShared.h"

#include "shaderlib/GFXPerFrameBindings.hlsli"

StructuredBuffer<InstanceData_Xform> g_instanceData_Transform : register(t0, PATHOS_PER_VIEW_SPACE);
StructuredBuffer<InstanceData_UniformOffsets> g_instanceData_UniformOffsets : register(t1, PATHOS_PER_VIEW_SPACE);


VSOut_ObjectFull_Material main(uint _meshInstanceID : TEXCOORD, uint _vid : SV_VertexID)
{
    uint vtxOffset = g_instanceData_UniformOffsets[_meshInstanceID].baseVtx + _vid;

    TangentSpace tangentSpace = g_unifiedVtxTangent[vtxOffset];

    uint materialIdx = g_instanceData_UniformOffsets[_meshInstanceID].materialIdx;
    uint transformIdx = g_instanceData_UniformOffsets[_meshInstanceID].transformIdx;

    InstanceData_Xform instanceXform = g_instanceData_Transform[transformIdx];


    float3 pos = g_unifiedVtxPos[vtxOffset];
    float3 modelPos = TransformInstanceData(float4(pos, 1), instanceXform.row0, instanceXform.row1, instanceXform.row2);

    VSOut_ObjectFull_Material ret;

    ret.pos = mul(float4(modelPos, 1), g_frameCb.mainViewProj);
    ret.posWS = modelPos; 
    ret.uv =  g_unifiedVtxUv[vtxOffset];
    ret.viewDepth = ret.pos.w;

    ret.normal = TransformInstanceData(float4(tangentSpace.normal, 0), instanceXform.row0, instanceXform.row1, instanceXform.row2);

    float3 tangent = TransformInstanceData(float4(tangentSpace.tangentSign.xyz, 0), instanceXform.row0, instanceXform.row1, instanceXform.row2);
    ret.tangentSign = float4(tangent, tangentSpace.tangentSign.w);
    ret.materialIdx = materialIdx;

    return ret;
}