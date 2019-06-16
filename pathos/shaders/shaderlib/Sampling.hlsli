#ifndef SAMPLING_H
#define SAMPLING_H

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

#endif // SAMPLING_H