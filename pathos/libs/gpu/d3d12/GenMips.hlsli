#include "shaderlib/GlobalSamplers.hlsli"
#include "shaderlib/Sampling.hlsli"

// TODO: Undersampling for non square textures.
// TODO: Better than billinear filtering (eg bicubic, catmull-rom, mitchell, kaiser etc)

struct GenMipsCbuffer
{
    uint srcMip;
    uint numOutputMips;
    float2 rcpTexelSize;
};

ConstantBuffer<GenMipsCbuffer> g_cbuf : register(b0, space0);

#if defined(GEN_MIPS_ARRAY)
    #define UAV_TYPE RWTexture2DArray<float4>
    #define SRV_TYPE Texture2DArray<float4>
#elif defined(GEN_MIPS_CUBE)
    #define UAV_TYPE RWTexture2DArray<float4>
    #define SRV_TYPE TextureCube<float4>
#else
    #define UAV_TYPE RWTexture2D<float4>
    #define SRV_TYPE Texture2D<float4>
#endif

UAV_TYPE g_destMip0 : register(u0);
UAV_TYPE g_destMip1 : register(u1);
UAV_TYPE g_destMip2 : register(u2);
UAV_TYPE g_destMip3 : register(u3);
SRV_TYPE g_srcMip : register(t0);

float4 ApplySRGB(float4 _col)
{
    float4 linearCol = _col;
#ifdef GEN_MIPS_SRGB
    linearCol.rgb = linearCol.rgb < 0.0031308 ? 12.92 * linearCol.rgb : 1.055 * pow(abs(linearCol.rgb), 1.0 / 2.4) - 0.055;
#endif
    return linearCol;
}

#define GEN_MIPS_DIM 8

groupshared float g_ldsR[GEN_MIPS_DIM * GEN_MIPS_DIM];
groupshared float g_ldsG[GEN_MIPS_DIM * GEN_MIPS_DIM]; 
groupshared float g_ldsB[GEN_MIPS_DIM * GEN_MIPS_DIM]; 
groupshared float g_ldsA[GEN_MIPS_DIM * GEN_MIPS_DIM]; 

void StoreTexelLDS(uint _index, float4 _texelLinear)
{
    g_ldsR[_index] = _texelLinear.r;
    g_ldsG[_index] = _texelLinear.g;
    g_ldsB[_index] = _texelLinear.b;
    g_ldsA[_index] = _texelLinear.a;
}

float4 LoadTexelLDS(uint _index)
{
    float4 ret;
    ret.r = g_ldsR[_index];
    ret.g = g_ldsG[_index];
    ret.b = g_ldsB[_index];
    ret.a = g_ldsA[_index];
    return ret;
}

#if defined(GEN_MIPS_ARRAY) || defined(GEN_MIPS_CUBE)
void StoreTexelToOutput(RWTexture2DArray<float4> _outImage, uint3 _DTid, uint _scaleFactor, float4 _linearCol)
{
    _outImage[uint3(_DTid.xy / _scaleFactor, _DTid.z)] = ApplySRGB(_linearCol);
}
#else
void StoreTexelToOutput(RWTexture2D<float4> _outImage, uint3 _DTid, uint _scaleFactor, float4 _linearCol)
{
    _outImage[_DTid.xy / _scaleFactor] = ApplySRGB(_linearCol);
}
#endif

[numthreads(GEN_MIPS_DIM, GEN_MIPS_DIM, 1)]
void main(uint Gi : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
    float2 uv = g_cbuf.rcpTexelSize * (DTid.xy + 0.5); // .5 texel center.
#if defined(GEN_MIPS_ARRAY)
    float4 srcTexel = g_srcMip.SampleLevel(g_samplerLinearClamp, float3(uv, DTid.z), g_cbuf.srcMip);
#elif defined(GEN_MIPS_CUBE)
    float4 srcTexel = g_srcMip.SampleLevel(g_samplerLinearWrap, UV_To_CubeMap(uv, DTid.z), g_cbuf.srcMip);
#else
    float4 srcTexel = g_srcMip.SampleLevel(g_samplerLinearClamp, uv, g_cbuf.srcMip);
#endif

    StoreTexelToOutput(g_destMip0, DTid, 1, srcTexel);

    if(g_cbuf.numOutputMips == 1)
    {
        return;
    }

    // Share tile across lds
    // TODO: Look at wave ops
    StoreTexelLDS(Gi, srcTexel);

    GroupMemoryBarrierWithGroupSync();

    // Even texels
    uint gtidOr = GTid.x | GTid.y;
    if((gtidOr & 0x1) == 0)
    {
        float4 topRight = LoadTexelLDS(Gi + 1);
        float4 bottomLeft = LoadTexelLDS(Gi + GEN_MIPS_DIM);
        float4 bottomRight = LoadTexelLDS(Gi + 1 + GEN_MIPS_DIM);
        float4 col = 0.25 * (srcTexel + topRight + bottomLeft + bottomRight);
        StoreTexelToOutput(g_destMip1, DTid, 2, col);
        StoreTexelLDS(Gi, col);
    }

    if(g_cbuf.numOutputMips == 2)
    {
        return;
    }

    GroupMemoryBarrierWithGroupSync();

    // multiple of 4
    if((gtidOr & 0x3) == 0)
    {
        float4 topRight = LoadTexelLDS(Gi + 2);
        float4 bottomLeft = LoadTexelLDS(Gi + GEN_MIPS_DIM*2);
        float4 bottomRight = LoadTexelLDS(Gi + 2 + GEN_MIPS_DIM*2);
        float4 col = 0.25 * (srcTexel + topRight + bottomLeft + bottomRight);
        StoreTexelToOutput(g_destMip2, DTid, 4, col);
        StoreTexelLDS(Gi, col);
    }

    if(g_cbuf.numOutputMips == 3)
    {
        return;
    }

    GroupMemoryBarrierWithGroupSync();

    // multiple of 8 (8x8 numthreads, so only top left thread)
    if(Gi == 0)
    {
        float4 topRight = LoadTexelLDS(Gi + 4);
        float4 bottomLeft = LoadTexelLDS(Gi + GEN_MIPS_DIM*4);
        float4 bottomRight = LoadTexelLDS(Gi + 4 + GEN_MIPS_DIM*4);
        float4 col = 0.25 * (srcTexel + topRight + bottomLeft + bottomRight);
        StoreTexelToOutput(g_destMip3, DTid, 8, col);
    }
}