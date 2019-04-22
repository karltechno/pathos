#include "Resources.h"

#include <res/ResourceSystem.h>
#include <gpu/GPUDevice.h>

#include <kt/Serialization.h>
#include <kt/Logging.h>

#include <string.h>

namespace gfx
{

static bool LoadShader(kt::IReader& _reader, uint64_t _streamLen, char const* _filePath, void*& o_shader)
{
	kt::StringView const nameView(_filePath);
	
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
	void* shaderBytecode = kt::Malloc(_streamLen);
	KT_SCOPE_EXIT(kt::Free(shaderBytecode));
	if (!_reader.ReadBytes(shaderBytecode, _streamLen))
	{
		return false;
	}

	gpu::ShaderHandle shaderHandle = gpu::CreateShader(type, gpu::ShaderBytecode{ shaderBytecode, _streamLen }, _filePath);
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

static void ReloadShader(kt::IReader& _reader, uint64_t _streamLen, void* _shader)
{
	gfx::ShaderResource* res = (gfx::ShaderResource*)_shader;

	// TODO: Temp alloc.
	void* shaderBytecode = kt::Malloc(_streamLen);
	KT_SCOPE_EXIT(kt::Free(shaderBytecode));
	if (!_reader.ReadBytes(shaderBytecode, _streamLen))
	{
		return;
	}

	gpu::ReloadShader(res->m_shader, gpu::ShaderBytecode{ shaderBytecode, _streamLen });
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