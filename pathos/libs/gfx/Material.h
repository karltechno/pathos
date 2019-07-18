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
	struct Params
	{
		kt::Vec4 m_baseColour;
		float m_roughnessFactor;
		float m_metallicFactor;
		float m_alphaCutoff;
		AlphaMode m_alphaMode;
	};

	enum TextureType : uint32_t
	{
		Albedo,
		Normal,
		MetallicRoughness,
		Occlusion,
		Emissive,

		Num_TextureType
	};

	Params m_params;

	gfx::TextureResHandle m_textures[TextureType::Num_TextureType] = {};
};


}