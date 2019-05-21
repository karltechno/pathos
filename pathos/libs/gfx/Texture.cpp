#include <kt/Logging.h>
#include <kt/FilePath.h>
#include <kt/File.h>
#include <kt/Serialization.h>

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

constexpr uint32_t c_textureCacheVersion = 1;

static void Serialize(kt::ISerializer* _s, Texture& _tex)
{
	kt::Serialize(_s, _tex.m_width);
	kt::Serialize(_s, _tex.m_height);
	kt::Serialize(_s, _tex.m_numMips);
	kt::Serialize(_s, _tex.m_mipOffsets);
	kt::Serialize(_s, _tex.m_texelData);
}

static bool LoadFromCache(Texture& o_tex, TextureLoadFlags _loadFlags, char const* _texPath)
{
	kt::String512 cachePath(_texPath);
	cachePath.Append(".cache");
	
	if (!kt::FileExists(cachePath.Data()))
	{
		return false;
	}

	KT_LOG_INFO("Found texture cache file: \"%s\".", cachePath.Data());
	FILE* f = fopen(cachePath.Data(), "rb");
	if (!f)
	{
		KT_LOG_INFO("Failed to open texture cache file: \"%s\"!", cachePath.Data());
		return false;
	}
	KT_SCOPE_EXIT(fclose(f));
	kt::FileReader reader(f);
	uint32_t version;
	if (!reader.Read(version)) { return false; }

	if (version != c_textureCacheVersion)
	{
		KT_LOG_INFO("%s has version %u, but texture cache version is %u.", version, c_textureCacheVersion);
		return false;
	}

	TextureLoadFlags writtenFlags;
	reader.Read(writtenFlags);
	if (writtenFlags != _loadFlags)
	{
		KT_LOG_INFO("Cached texture \"%s\" has different load flags than requested.", cachePath.Data());
		return false;
	}

	kt::ISerializer serializer(&reader, c_textureCacheVersion);
	Serialize(&serializer, o_tex);

	return true;
}

static void WriteToCache(Texture& o_tex, TextureLoadFlags _loadFlags, char const* _texPath)
{
	kt::String512 cachePath(_texPath);
	cachePath.Append(".cache");

	FILE* f = fopen(cachePath.Data(), "wb");
	if (!f)
	{
		KT_LOG_INFO("Failed to open texture cache file for writing: \"%s\"!", cachePath.Data());
		return;
	}
	KT_SCOPE_EXIT(fclose(f));
	kt::FileWriter writer(f);

	writer.Write(c_textureCacheVersion);
	writer.Write(_loadFlags);
	
	kt::ISerializer serializer(&writer, c_textureCacheVersion);
	Serialize(&serializer, o_tex);
}

static void CreateGPUBuffer2D(Texture& _tex, void const* _texelData, uint32_t _x, uint32_t _y, gpu::Format _fmt, uint32_t _numMips, char const* _debugName = nullptr)
{
	gpu::TextureDesc desc = gpu::TextureDesc::Desc2D(_x, _y, gpu::TextureUsageFlags::ShaderResource, _fmt);
	desc.m_mipLevels = _numMips;
	_tex.m_gpuTex = gpu::CreateTexture(desc, _texelData, _debugName);
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
	if (LoadFromCache(*this, _flags, _fileName))
	{
		gpu::Format const gpuFmt = !!(_flags & TextureLoadFlags::sRGB) ? gpu::Format::R8G8B8A8_UNorm_SRGB : gpu::Format::R8G8B8A8_UNorm;
		CreateGPUBuffer2D(*this, m_texelData.Data(), m_width, m_height, gpuFmt, m_numMips, _fileName);
		return true;
	}

	// TODO: Hack - should use a gpu friendly compressed format, or reconstruct z for normal map, etc.
	int constexpr c_requiredComp = 4;
	int x, y, comp;

	if (stbi_is_hdr(_fileName))
	{
		float* hdrPtr = stbi_loadf(_fileName, &x, &y, &comp, c_requiredComp);
		if (!hdrPtr)
		{
			KT_LOG_ERROR("Failed to load hdr image: %s - %s", _fileName, stbi_failure_reason());
			return false;
		}
		KT_SCOPE_EXIT(stbi_image_free(hdrPtr));
		m_width = uint32_t(x);
		m_height = uint32_t(y);
		m_numMips = 1;
		CreateGPUBuffer2D(*this, hdrPtr, m_width, m_height, gpu::Format::R32G32B32A32_Float, 1, _fileName);
		return true;
	}

	uint8_t* srcTexels = stbi_load(_fileName, &x, &y, &comp, c_requiredComp);
	if (!srcTexels)
	{
		KT_LOG_ERROR("Failed to load image %s - %s", _fileName, stbi_failure_reason());
		return false;
	}
	KT_SCOPE_EXIT(stbi_image_free(srcTexels));

	if(LoadFromRGBA8(srcTexels, uint32_t(x), uint32_t(y), _flags, _fileName))
	{
		WriteToCache(*this, _flags, _fileName);
		return true;
	}
	
	// TODO: Make a flag or something if we ever want to keep data around on CPU. 
	// Better yet, stream data straight into gpu memory.
	m_texelData.ClearAndFree();
	
	return false;
}

bool Texture::LoadFromRGBA8(uint8_t* _texels, uint32_t _width, uint32_t _height, TextureLoadFlags _flags, char const* _debugName)
{
	gpu::Format const gpuFmt = !!(_flags & TextureLoadFlags::sRGB) ? gpu::Format::R8G8B8A8_UNorm_SRGB : gpu::Format::R8G8B8A8_UNorm;

	uint32_t constexpr c_bytesPerPixel = 4;

	if (!(_flags & TextureLoadFlags::GenMips))
	{
		m_width = _width;
		m_height = _height;
		m_numMips = 1;
		CreateGPUBuffer2D(*this, _texels, _width, _height, gpuFmt, 1, _debugName);
		return true;
	}

	uint32_t const mipChainLen = MipChainLength(_width, _height);
	KT_ASSERT(mipChainLen <= c_maxMips);

	struct MipInfo
	{
		uint32_t x;
		uint32_t y;
		uint32_t dataOffs;
	};

	MipInfo mips[c_maxMips];

	uint32_t curDataOffs = 0;

	m_numMips = mipChainLen;
	m_width = _width;
	m_height = _height;

	for (uint32_t i = 0; i < mipChainLen; ++i)
	{
		mips[i].x = MipDimForLevel(_width, i);
		mips[i].y = MipDimForLevel(_height, i);
		mips[i].dataOffs = curDataOffs;
		m_mipOffsets[i] = curDataOffs;
		curDataOffs += mips[i].x * mips[i].y * c_bytesPerPixel;
	}

	m_texelData.Resize(curDataOffs);

	memcpy(m_texelData.Data(), _texels, c_bytesPerPixel * mips[0].x * mips[0].y);

	for (uint32_t i = 1; i < mipChainLen; ++i)
	{
		if (!!(_flags & TextureLoadFlags::sRGB))
		{
			uint32_t const stbir_flags = !!(_flags & TextureLoadFlags::Premultiplied) ? STBIR_FLAG_ALPHA_PREMULTIPLIED : 0;
			stbir_resize_uint8_srgb(m_texelData.Data() + mips[i - 1].dataOffs, int(mips[i - 1].x), int(mips[i - 1].y), 0,
									m_texelData.Data() + mips[i].dataOffs, int(mips[i].x), int(mips[i].y), 0, c_bytesPerPixel, 3, stbir_flags);
		}
		else
		{
			stbir_resize_uint8(m_texelData.Data() + mips[i - 1].dataOffs, int(mips[i - 1].x), int(mips[i - 1].y), 0,
							   m_texelData.Data() + mips[i].dataOffs, int(mips[i].x), int(mips[i].y), 0, c_bytesPerPixel);
		}

		if (!!(_flags & TextureLoadFlags::Normalize))
		{
			uint8_t* begin = m_texelData.Data() + mips[i].dataOffs;
			uint8_t* end = m_texelData.Data() + mips[i].dataOffs + mips[i].x * mips[i].y * c_bytesPerPixel;
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

	CreateGPUBuffer2D(*this, m_texelData.Data(), _width, _height, gpuFmt, mipChainLen, _debugName);
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

	bool const ret = LoadFromRGBA8(_textureData, uint32_t(x), uint32_t(y), _flags, _debugName);
	// TODO: Make a flag or something if we ever want to keep data around on CPU. 
	// Better yet, stream data straight into gpu memory.
	m_texelData.ClearAndFree();
	return ret;
}

}