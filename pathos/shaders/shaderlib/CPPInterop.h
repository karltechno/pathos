#ifndef CPP_INTEROP
#define CPP_INTEROP

#ifdef __cplusplus

#define PATHOS_SHADER_SPACE(x) x
#define PATHOS_SRV_SLOT(x) x
#define PATHOS_UAV_SLOT(x) x
#define PATHOS_CBV_SLOT(x) x

#include <stdint.h>
#include <kt/Vec2.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/Mat4.h>
#include <kt/Mat3.h>

#define SHADERLIB_NAMESPACE_BEGIN namespace shaderlib {
#define SHADERLIB_NAMESPACE_END }

using float2 = kt::Vec2;
using float3 = kt::Vec3;
using float4 = kt::Vec4;
using float4x4 = kt::Mat4;
using float3x3 = kt::Mat3;

using uint = uint32_t;

#else

#define PATHOS_SHADER_SPACE(x) space##x
#define PATHOS_SRV_SLOT(x) t##x
#define PATHOS_UAV_SLOT(x) u##x
#define PATHOS_CBV_SLOT(x) b##x

#define SHADERLIB_NAMESPACE_BEGIN
#define SHADERLIB_NAMESPACE_END

#endif // __cplusplus

#endif // CPP_INTEROP