#include <kt/Logging.h>

#include <res/ResourceSystem.h>

#include "stb_image.h"
#include "stb_image_resize.h"

#include "Texture.h"

namespace gfx
{

struct TextureLoader : res::IResourceHandler
{
	bool CreateFromFile(char const* _filePath, void*& o_res) override
	{
		Texture* tex = new Texture;
		if (!tex->LoadFromFile(_filePath, TextureLoadFlags::None))
		{
			delete tex;
			return false;
		}

		o_res = tex;
		return true;
	}

	bool CreateEmpty(void*& o_res) override
	{
		o_res = new Texture;
		return true;
	}

	void Destroy(void* _ptr) override
	{
		delete (Texture*)_ptr;
	}
};

static void CreateGPUBuffer2D(Texture& _tex, uint32_t _x, uint32_t _y, gpu::Format _fmt, uint32_t _numMips, char const* _debugName = nullptr)
{
	gpu::TextureDesc desc = gpu::TextureDesc::Desc2D(_x, _y, gpu::TextureUsageFlags::ShaderResource, _fmt);
	desc.m_mipLevels = _numMips;
	_tex.m_gpuTex = gpu::CreateTexture(desc, _tex.m_texels.Data(), _debugName);
}

void Texture::RegisterResourceLoader()
{
	char const* extensions[] = {
		".jpeg",
		".jpg",
		".png",
		".tga"
		/*
		".dds" // todo
		*/
	};

	res::RegisterResource<Texture>("Texture", kt::MakeSlice(extensions), new TextureLoader);
}

bool Texture::LoadFromFile(char const* _fileName, TextureLoadFlags _flags)
{
	// TODO: Hack - should use a gpu friendly compressed format, or reconstruct z for normal map, etc.
	int constexpr c_requiredComp = 4;

	int x, y, comp;

	uint8_t* srcTexels = stbi_load(_fileName, &x, &y, &comp, c_requiredComp);
	if (!srcTexels)
	{
		KT_LOG_ERROR("Failed to load image %s - %s", _fileName, stbi_failure_reason());
		return false;
	}
	KT_SCOPE_EXIT(stbi_image_free(srcTexels));

	return LoadFromRGBA8(srcTexels, uint32_t(x), uint32_t(y), _flags, _fileName);
}

bool Texture::LoadFromRGBA8(uint8_t* _texels, uint32_t _width, uint32_t _height, TextureLoadFlags _flags, char const* _debugName)
{
	gpu::Format const gpuFmt = !!(_flags & TextureLoadFlags::sRGB) ? gpu::Format::R8G8B8A8_UNorm_SRGB : gpu::Format::R8G8B8A8_UNorm;

	uint32_t constexpr c_bytesPerPixel = 4;

	if (!(_flags & TextureLoadFlags::GenMips))
	{
		m_texels.Resize(_width * _height * c_bytesPerPixel);
		memcpy(m_texels.Data(), _texels, m_texels.Size());
		CreateGPUBuffer2D(*this, _width, _height, gpuFmt, 1, _debugName);
		return true;
	}

	uint32_t const mipChainLen = MipChainLength(_width, _height);
	uint32_t constexpr c_maxMips = 15;
	KT_ASSERT(mipChainLen <= c_maxMips);

	struct MipInfo
	{
		uint32_t x;
		uint32_t y;
		uint32_t dataOffs;
	};

	MipInfo mips[c_maxMips];

	uint32_t curDataOffs = 0;


	for (uint32_t i = 0; i < mipChainLen; ++i)
	{
		mips[i].x = MipDimForLevel(_width, i);
		mips[i].y = MipDimForLevel(_height, i);
		mips[i].dataOffs = curDataOffs;
		curDataOffs += mips[i].x * mips[i].y * c_bytesPerPixel;
	}

	m_texels.Resize(curDataOffs);

	memcpy(m_texels.Data(), _texels, c_bytesPerPixel * mips[0].x * mips[0].y);

	for (uint32_t i = 1; i < mipChainLen; ++i)
	{
		if (!!(_flags & TextureLoadFlags::sRGB))
		{
			uint32_t const stbir_flags = !!(_flags & TextureLoadFlags::Premultiplied) ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0;
			stbir_resize_uint8_srgb(m_texels.Data() + mips[i - 1].dataOffs, int(mips[i - 1].x), int(mips[i - 1].y), 0,
									m_texels.Data() + mips[i].dataOffs, int(mips[i].x), int(mips[i].y), 0, c_bytesPerPixel, 3, stbir_flags);
		}
		else
		{
			stbir_resize_uint8(m_texels.Data() + mips[i - 1].dataOffs, int(mips[i - 1].x), int(mips[i - 1].y), 0,
							   m_texels.Data() + mips[i].dataOffs, int(mips[i].x), int(mips[i].y), 0, c_bytesPerPixel);
		}

		if (!!(_flags & TextureLoadFlags::Normalize))
		{
			uint8_t* begin = m_texels.Data() + mips[i].dataOffs;
			uint8_t* end = m_texels.Data() + mips[i].dataOffs + mips[i].x * mips[i].y * c_bytesPerPixel;
			while (begin != end)
			{
				// Ideally this is done in a higher precision than 8 bit, and before compression - but this is just a sandbox :)
				uint8_t rgb[3];
				rgb[0] = begin[0];
				rgb[1] = begin[1];
				rgb[2] = begin[2];

				// ignoring alpha
				kt::Vec3 texel{ float(rgb[0]), float(rgb[1]), float(rgb[2]) };
				texel *= 1.0f / 255.0f;
				texel *= 2.0f;
				texel -= kt::Vec3(1.0f);
				texel = kt::Clamp(texel, kt::Vec3(-1.0f), kt::Vec3(1.0f));
				float const len = kt::Length(texel);
				if (len < 0.001f)
				{
					texel = kt::Vec3(0.0f, 0.0f, 0.0f);
				}
				else
				{
					texel = texel / len;
				}

				for (uint32_t compIdx = 0; compIdx < 3; ++compIdx)
				{
					// [-1,1] -> [0,1] -> [0, 255] -> round
					begin[compIdx] = uint8_t(kt::Clamp(((texel[compIdx] + 1.0f) * 0.5f * 255.0f + 0.5f), 0.0f, 255.0f));
				}

				begin += 4;
			}
		}
	}

	CreateGPUBuffer2D(*this, _width, _height, gpuFmt, mipChainLen, _debugName);
	return true;
}

bool Texture::LoadFromMemory(uint8_t* _textureData, uint32_t const _size, TextureLoadFlags _flags /*= TextureLoadFlags::None*/, char const* _debugName /* = nullptr */)
{
	int x, y, comp;
	uint8_t* texels = stbi_load_from_memory(_textureData, _size, &x, &y, &comp, 4);

	if (!texels)
	{
		KT_LOG_ERROR("Failed to load image from memory (%s) - %s", _debugName, stbi_failure_reason());
		return false;
	}
	KT_SCOPE_EXIT(stbi_image_free(texels));

	return LoadFromRGBA8(_textureData, uint32_t(x), uint32_t(y), _flags, _debugName);
}

}