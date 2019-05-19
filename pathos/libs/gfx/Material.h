#pragma once
#include <kt/Vec4.h>

#include <gfx/Texture.h>

namespace gfx
{

struct Material
{
	enum class AlphaMode : uint8_t
	{
		Opaque,
		Mask,
		Transparent
	};

	// TODO: 
	// Samplers
	// Other material params

	kt::Vec4 m_baseColour;
	float m_rougnessFactor;
	float m_metallicFactor;
	float m_alphaCutoff;
	AlphaMode m_alphaMode;

	TextureResHandle m_albedoTex;
	TextureResHandle m_normalTex;
	TextureResHandle m_metallicRoughnessTex;
	TextureResHandle m_occlusionTex;
};


}