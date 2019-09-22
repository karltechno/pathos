#include "../shaderlib/CommonShared.h"
#include "../shaderlib/DefinesShared.h"
#include "../shaderlib/CullingShared.h"

RWBuffer<uint> g_outDrawArgs : register(u0, PATHOS_PER_BATCH_SPACE);

[numthreads(1, 1, 1)]
void main()
{
    g_outDrawArgs[0] = 0;
}