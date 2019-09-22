#include "../shaderlib/CommonShared.h"
#include "../shaderlib/DefinesShared.h"
#include "../shaderlib/CullingShared.h"


StructuredBuffer<uint> g_packedInstancesToCull : register(t0, PATHOS_PER_BATCH_SPACE);
StructuredBuffer<InstanceData_Xform> g_instanceXforms : register(t1, PATHOS_PER_BATCH_SPACE);
StructuredBuffer<GPUSubMeshData> g_submeshData : register(t2, PATHOS_PER_BATCH_SPACE);

RWBuffer<uint> g_outDrawArgs : register(u0, PATHOS_PER_BATCH_SPACE);
RWBuffer<uint> g_outPackedMeshInstance : register(u1, PATHOS_PER_BATCH_SPACE);

ConstantBuffer<CullingConstants> g_cb : register(b0, PATHOS_PER_BATCH_SPACE);

groupshared uint lds_numDraws;
groupshared uint lds_baseDrawIdx;

void WriteDrawArgs(uint _globalIdx, GPUSubMeshData _subMeshData, uint _packedCullingData)
{
    // uint indicesPerInstance;
    // uint instanceCount;
    // uint indexStart;
    // int baseVertex;
    // uint startInstance;

    // +1 because counter is stored at 0
    g_outDrawArgs[_globalIdx * 5 + 0 + 1] = _subMeshData.numIndices; // indicesPerInstance
    g_outDrawArgs[_globalIdx * 5 + 1 + 1] = 1; // instanceCount
    g_outDrawArgs[_globalIdx * 5 + 2 + 1] = _subMeshData.unifiedIndexBufferOffset; // indexStart
    g_outDrawArgs[_globalIdx * 5 + 3 + 1] = _subMeshData.unifiedVertexBufferOffset; // baseVertex
    g_outDrawArgs[_globalIdx * 5 + 4 + 1] = _globalIdx; // startInstance
    g_outPackedMeshInstance[_globalIdx] = _packedCullingData;
}

[numthreads(64, 1, 1)]
void main(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    if(GTid.x == 0)
    {
        lds_numDraws = 0;
    }
    
    bool writeDrawCall = false;
    uint packedCullingData;
    uint localDrawIdx;

    GroupMemoryBarrierWithGroupSync();
     
    if(DTid.x < g_cb.numSubmeshInstances)
    {
        writeDrawCall = true;
        packedCullingData = g_packedInstancesToCull[DTid.x];
        // TODO: Cull stuff!
        InterlockedAdd(lds_numDraws, 1, localDrawIdx);
    }

    GroupMemoryBarrierWithGroupSync();

    if(GTid.x == 0)
    {
        InterlockedAdd(g_outDrawArgs[0], lds_numDraws, lds_baseDrawIdx);
    }

    GroupMemoryBarrierWithGroupSync();

    if(writeDrawCall)
    {
        uint idx = lds_baseDrawIdx + localDrawIdx;
        GPUSubMeshData subMesh = g_submeshData[packedCullingData >> PATHOS_SUBMESH_ID_REMAP_SHIFT];
        WriteDrawArgs(idx, subMesh, packedCullingData);
    }
}