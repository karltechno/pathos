#include "Scene.h"

namespace gfx
{


void Scene::UpdateCBuffer(shaderlib::TestLightCBuffer* _cbuffer)
{
	uint32_t const numLights = kt::Min(m_lights.Size(), uint32_t(MAX_LIGHTS));
	_cbuffer->numLights = numLights;
	_cbuffer->sunColor = m_sunColor;
	_cbuffer->sunDir = m_sunDir;
	memcpy(_cbuffer->lights, m_lights.Data(), numLights * sizeof(shaderlib::LightData));
}

}