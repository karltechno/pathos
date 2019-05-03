#include "Resources.h"

#include <res/ResourceSystem.h>
#include <gpu/GPUDevice.h>

#include <kt/Serialization.h>
#include <kt/Logging.h>

#include <string.h>

#include "Model.h"

namespace gfx
{

struct ShaderLoader : res::IResourceHandler
{
	bool CreateFromFile(char const* _filePath, void*& o_res) override
	{
		FILE* f = fopen(_filePath, "rb");
		if (!f)
		{
			KT_LOG_ERROR("Failed to open shader file: \"%s\"", _filePath);
			return false;
		}
		KT_SCOPE_EXIT(fclose(f));

		kt::FileReader reader(f);

		gpu::ShaderType type;
		if (strstr(_filePath, PATHOS_VERTEX_SHADER_EXT))
		{
			type = gpu::ShaderType::Vertex;
		}
		else if (strstr(_filePath, PATHOS_PIXEL_SHADER_EXT))
		{
			type = gpu::ShaderType::Pixel;
		}
		else if (strstr(_filePath, PATHOS_COMPUTE_SHADER_EXT))
		{
			type = gpu::ShaderType::Compute;
		}
		else
		{
			KT_LOG_ERROR("Failed to determine shader type for \"%s\", remove this hack or fix the code!", _filePath);
			return false;
		}

		// TODO: Temp alloc.
		void* shaderBytecode = kt::Malloc(reader.OriginalSize());
		KT_SCOPE_EXIT(kt::Free(shaderBytecode));
		if (!reader.ReadBytes(shaderBytecode, reader.OriginalSize()))
		{
			return false;
		}

		gpu::ShaderHandle shaderHandle = gpu::CreateShader(type, gpu::ShaderBytecode{ shaderBytecode, reader.OriginalSize() }, _filePath);
		if (!shaderHandle.IsValid())
		{
			return false;
		}

		o_res = new ShaderResource{ shaderHandle };
		return true;
	}

	bool CreateEmpty(void*& o_res) override
	{
		o_res = new ShaderResource;
		return true;
	}


	void Destroy(void* _ptr) override
	{
		ShaderResource* res = ((ShaderResource*)_ptr);
		gpu::Release(res->m_shader);
		delete res;
	}

	void ReloadFromFile(char const* _filePath, void* _oldRes) override
	{
		gfx::ShaderResource* res = (gfx::ShaderResource*)_oldRes;

		FILE* f = fopen(_filePath, "rb");
		if (!f)
		{
			KT_LOG_ERROR("Failed to open shader file for reloading: \"%s\"", _filePath);
			return;
		}
		KT_SCOPE_EXIT(fclose(f));

		kt::FileReader reader(f);

		// TODO: Temp alloc.
		void* shaderBytecode = kt::Malloc(reader.OriginalSize());
		KT_SCOPE_EXIT(kt::Free(shaderBytecode));
		if (!reader.ReadBytes(shaderBytecode, reader.OriginalSize()))
		{
			return;
		}

		gpu::ReloadShader(res->m_shader, gpu::ShaderBytecode{ shaderBytecode, reader.OriginalSize() });
	}

	bool SupportsReload() override
	{
		return true;
	}
};

void RegisterResourceLoaders()
{
	static char const* s_shaderExtensions[] =
	{
		".cso"
	};

	res::RegisterResource<ShaderResource>("Shader", kt::MakeSlice(s_shaderExtensions), new ShaderLoader);
	Model::RegisterResourceLoader();
	Texture::RegisterResourceLoader();
}

}