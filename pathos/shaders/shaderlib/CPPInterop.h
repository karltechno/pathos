#ifndef CPP_INTEROP
#define CPP_INTEROP

#ifdef __cplusplus
#include <kt/Vec2.h>
#include <kt/Vec3.h>
#include <kt/Vec4.h>
#include <kt/Mat4.h>
#include <kt/Mat3.h>

using float2 = kt::Vec2;
using float3 = kt::Vec3;
using float4 = kt::Vec4;
using float4x4 = kt::Mat4;
using float3x3 = kt::Mat3;

#endif // __cplusplus

#endif // CPP_INTEROP