#include "Resources.h"

#include <res/ResourceSystem.h>
#include <gpu/GPUDevice.h>

#include <kt/Serialization.h>
#include <kt/Logging.h>

#include <string.h>

namespace gfx
{

static bool LoadShader(char const* _filePath, void*& o_shader)
{
	FILE* f = fopen(_filePath, "rb");
	if (!f)
	{
		KT_LOG_ERROR("Failed to open shader file: \"%s\"", _filePath);
		return false;
	}
	KT_SCOPE_EXIT(fclose(f));

	kt::FileReader reader(f);
	
	// TODO: HACK
	gpu::ShaderType type;
	if (strstr(_filePath, ".vertex"))
	{
		type = gpu::ShaderType::Vertex;
	}
	else if (strstr(_filePath, ".pixel"))
	{
		type = gpu::ShaderType::Pixel;
	}
	else if (strstr(_filePath, ".compute"))
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

	o_shader = new ShaderResource{ shaderHandle };
	return true;
}

static void DestroyShader(void* _shader)
{
	ShaderResource* res = ((ShaderResource*)_shader);
	gpu::Release(res->m_shader);
	delete res;
}

static void ReloadShader(char const* _path, void* _shader)
{
	gfx::ShaderResource* res = (gfx::ShaderResource*)_shader;

	FILE* f = fopen(_path, "rb");
	if (!f)
	{
		KT_LOG_ERROR("Failed to open shader file for reloading: \"%s\"", _path);
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

void RegisterResourceLoaders()
{
	static char const* s_shaderExtensions[] =
	{
		".cso"
	};

	res::RegisterResource<ShaderResource>("Shader", kt::MakeSlice(s_shaderExtensions), LoadShader, DestroyShader, ReloadShader);
}

}