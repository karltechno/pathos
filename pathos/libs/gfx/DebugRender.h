#pragma once
#include <gpu/CommandContext.h>

namespace kt
{
struct Vec3;
struct Vec4;

struct Mat3;
struct Mat4;

struct AABB;
}

namespace gfx
{
struct Camera;

namespace DebugRender
{

void Init();
void Shutdown();

void Flush(gpu::cmd::Context* _ctx);

void Line(kt::Vec3 const& _p0, kt::Vec3 const& _p1, kt::Vec4 const& _color, bool _depth = true);

void LineBox(kt::Mat4 const& _mat, kt::Vec4 const& _color, bool _depth = true);
void LineBox(kt::AABB const& _aabb, kt::Mat4 const& _mtx, kt::Vec4 const& _color, bool _depth = true);
void LineBox(kt::AABB const& _aabb, kt::Mat3 const& _mtx, kt::Vec3 const& _pos, kt::Vec4 const& _color, bool _depth = true);

void LineFrustum(gfx::Camera const& _cam, kt::Vec4 const& _color, bool _depth = true);
}

}