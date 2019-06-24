Texture2DArray<float4> g_inTex : register(t0, space0);
RWTexture2DArray<float4> g_outTex : register(u0, space0);


struct CBuf
{
    uint inLevel;
};

ConstantBuffer<CBuf> g_cbuf : register(b0, space0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    g_outTex[DTid] = g_inTex.Load(int4(DTid, g_cbuf.inLevel));
}