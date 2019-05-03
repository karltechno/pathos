#pragma once
#include <kt/kt.h>
#include <kt/Array.h>

#include <res/Resource.h>
#include <gpu/Types.h>
#include <gpu/HandleRef.h>

namespace gfx
{

enum class TextureLoadFlags
{
	None = 0x0,
	GenMips = 0x1,
	Normalize = 0x2,
	sRGB = 0x4,
	Premultiplied = 0x8 
};
KT_ENUM_CLASS_FLAG_OPERATORS(TextureLoadFlags);

struct Texture
{
	KT_NO_COPY(Texture);

	Texture() = default;

	Texture(Texture&&) = default;
	Texture& operator=(Texture&&) = default;

	static void RegisterResourceLoader();

	bool LoadFromFile(char const* _fileName, TextureLoadFlags _flags = TextureLoadFlags::None);
	bool LoadFromRGBA8(uint8_t* _texels, uint32_t _width, uint32_t _height, TextureLoadFlags _flags = TextureLoadFlags::None, char const* _debugName = nullptr);
	bool LoadFromMemory(uint8_t* _textureData, uint32_t const _size, TextureLoadFlags _flags = TextureLoadFlags::None, char const* _debugName = nullptr);

	kt::Array<uint8_t> m_texels;
	gpu::TextureRef m_gpuTex;
	
};

using TextureResHandle = res::ResourceHandle<Texture>;

KT_FORCEINLINE uint32_t MipDimForLevel(uint32_t _extent, uint32_t _level)
{
	return kt::Max<uint32_t>(1u, _extent >> _level);
}

KT_FORCEINLINE uint32_t MipChainLength(uint32_t _x)
{
	return kt::FloorLog2(_x) + 1; // +1 for base tex.
}


KT_FORCEINLINE uint32_t MipChainLength(uint32_t _x, uint32_t _y)
{
	return kt::FloorLog2(kt::Max(_x, _y)) + 1;
}

KT_FORCEINLINE uint32_t MipChainLength(uint32_t _x, uint32_t _y, uint32_t _z)
{
	return kt::FloorLog2(kt::Max(_x, kt::Max(_y, _z))) + 1;
}



}