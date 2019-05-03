#pragma once
#include <kt/Vec4.h>

#include <gfx/Texture.h>

namespace gfx
{

struct Material
{
	// TODO: 
	// Samplers
	// Other material params
	// Alpha mode

	kt::Vec4 m_baseColour;
	float m_rougnessFactor;
	float m_metallicFactor;

	TextureResHandle m_diffuseTex;
	TextureResHandle m_normalTex;
	TextureResHandle m_metallicRoughnessTex;
	TextureResHandle m_occlusionTex;
};


}