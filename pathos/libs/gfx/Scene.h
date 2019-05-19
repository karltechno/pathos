#pragma once
#include <shaderlib/LightingStructs.h>
#include <kt/Array.h>


namespace gfx
{

class Scene
{
public:
	void UpdateCBuffer(shaderlib::TestLightCBuffer* _cbuffer);
	kt::Array<shaderlib::LightData> m_lights;
	kt::Vec3 m_sunColor = { 0.99f, 0.95f, 0.85f };
	kt::Vec3 m_sunDir = { -0.7f, -0.7f, 0.175f };
};

}