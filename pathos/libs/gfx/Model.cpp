#include "Model.h"

#include <kt/Logging.h>
#include <kt/FilePath.h>
#include <kt/Serialization.h>
#include <kt/File.h>

#include <res/ResourceSystem.h>

#include "cgltf.h"
#include "mikktspace.h"

#include "Scene.h"


namespace gfx
{

struct ModelLoader : res::IResourceHandler
{
	bool CreateFromFile(char const* _filePath, void*& o_res) override
	{
		kt::FilePath path(_filePath);
		Model* model = gfx::Scene::CreateModel(path.GetFileNameNoExt().Data());
		if (model->LoadFromGLTF(_filePath))
		{
			o_res = model;
			return true;
		}

		// pop model and model name
		gfx::Scene::s_modelNames.PopBack();
		gfx::Scene::s_models.PopBack();
		return false;
	}

	bool CreateEmpty(void*& o_res) override
	{
		o_res = gfx::Scene::CreateModel("UN-NAMED");
		return true;
	}

	void Destroy(void* _ptr) override
	{
		KT_UNUSED(_ptr);
		// No-op
	}
};



void Model::RegisterResourceLoader()
{
	static char const* extensions[] = {
		".gltf",
		".glb"
	};

	res::RegisterResource<Model>("Model", kt::MakeSlice(extensions), new ModelLoader);
}


gpu::VertexLayout Model::FullVertexLayout()
{
	gpu::VertexLayout layout;
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, 0, 0);
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Normal, 0, 1);
	layout.Add(gpu::Format::R32G32B32A32_Float, gpu::VertexSemantic::Tangent, 0, 1);
	layout.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::TexCoord, 0, 2);
	return layout;
}

uint8_t* AccessorStartOffset(cgltf_accessor* _accessor)
{
	return (uint8_t*)_accessor->buffer_view->buffer->data + _accessor->buffer_view->offset + _accessor->offset;
}

template <typename IntT>
static void CopyIndexBuffer(cgltf_accessor* _accessor, uint32_t* o_dest, uint32_t _vtxOffset)
{
	KT_ASSERT(_accessor->count % 3 == 0);
	uint8_t* src = AccessorStartOffset(_accessor);
	cgltf_size const stride = _accessor->stride;

	for (uint32_t i = 0; i < _accessor->count; i += 3)
	{
		uint32_t indicies[3];
		indicies[0] = (uint32_t)*(IntT*)(src) + _vtxOffset;
		src += stride;
		indicies[1] = (uint32_t)*(IntT*)(src) + _vtxOffset;
		src += stride;
		indicies[2] = (uint32_t)*(IntT*)(src) + _vtxOffset;
		src += stride;
		*o_dest++ = indicies[1];
		*o_dest++ = indicies[0];
		*o_dest++ = indicies[2];
	}
}
static void CopyVertexStreamGeneric(cgltf_accessor* _accessor, uint8_t* _dest, size_t _destSize, size_t _destStride = 0)
{
	cgltf_size const srcStride = _accessor->stride;
	uint8_t const* src = AccessorStartOffset(_accessor);
	uint8_t const* srcEnd = src + srcStride * _accessor->count;
	size_t const destStride = _destStride == 0 ? _destSize : _destStride;
	while (src != srcEnd)
	{
		memcpy(_dest, src, _destSize);
		src += srcStride;
		_dest += destStride;
	}
}

static void CopyPrecomputedTangentSpace(Model* _model, cgltf_accessor* _normalAccessor, cgltf_accessor* _tangentAccessor)
{
	// TODO: Stop mixing assert/log. Assert should log.
	KT_ASSERT(_normalAccessor->component_type == cgltf_component_type_r_32f);
	KT_ASSERT(_tangentAccessor->component_type == cgltf_component_type_r_32f);
	KT_ASSERT(_normalAccessor->type == cgltf_type_vec3);
	KT_ASSERT(_tangentAccessor->type == cgltf_type_vec4);
	KT_ASSERT(_tangentAccessor->count == _normalAccessor->count);
	uint32_t const normalStride = uint32_t(_normalAccessor->stride);
	uint32_t const tangentStride = uint32_t(_tangentAccessor->stride);

	uint8_t const* normalSrc = AccessorStartOffset(_normalAccessor);
	uint8_t const* tangentSrc = AccessorStartOffset(_tangentAccessor);
	uint8_t const* tangentSrcEnd = tangentSrc + _tangentAccessor->count * tangentStride;
	uint32_t const tangentDestBegin = _model->m_tangentStream.Size();
	_model->m_tangentStream.Resize(uint32_t(tangentDestBegin + _tangentAccessor->count));
	TangentSpace* destPtr = _model->m_tangentStream.Data() + tangentDestBegin;
	while (tangentSrc != tangentSrcEnd)
	{
		memcpy(&destPtr->m_norm, normalSrc, sizeof(kt::Vec3));
		memcpy(&destPtr->m_tangentWithSign, tangentSrc, sizeof(kt::Vec4));
		++destPtr;
		normalSrc += normalStride;
		tangentSrc += tangentStride;
	}
}

static void GenMikktTangents(Model* _model, uint32_t _idxBegin, uint32_t _idxEnd)
{
	SMikkTSpaceContext mikktCtx{};
	SMikkTSpaceInterface mikktInterface{};

	struct GenTangData
	{
		uint32_t GetMeshVertIdx(int _face, int _vert) const
		{
			uint32_t const faceIdx = (uint32_t)_face + m_faceBegin;
			KT_ASSERT(uint32_t(faceIdx * 3) < m_model->m_indicies.Size());
			return m_model->m_indicies[uint32_t(faceIdx * 3 + _vert)];
		}

		Model* m_model;
		uint32_t m_faceBegin;
		uint32_t m_numFaces;
	};

	mikktInterface.m_getNumFaces = [](SMikkTSpaceContext const* _ctx) -> int
	{
		GenTangData* data = (GenTangData*)_ctx->m_pUserData;
		return (int)(data->m_numFaces);
	};

	mikktInterface.m_getNumVerticesOfFace = [](SMikkTSpaceContext const* _ctx, int const _faceIdx) -> int
	{
		KT_UNUSED2(_ctx, _faceIdx);
		return 3;
	};

	mikktInterface.m_getPosition = [](SMikkTSpaceContext const* _ctx, float* _outPos, int const _face, int const _vert) -> void
	{
		GenTangData* data = (GenTangData*)_ctx->m_pUserData;
		memcpy(_outPos, &data->m_model->m_posStream[data->GetMeshVertIdx(_face, _vert)], sizeof(kt::Vec3));
	};

	mikktInterface.m_getNormal = [](SMikkTSpaceContext const* _ctx, float* _outNorm, int const _face, int const _vert) -> void
	{
		GenTangData* data = (GenTangData*)_ctx->m_pUserData;
		memcpy(_outNorm, &data->m_model->m_tangentStream[data->GetMeshVertIdx(_face, _vert)].m_norm, sizeof(kt::Vec3));
	};

	mikktInterface.m_getTexCoord = [](SMikkTSpaceContext const* _ctx, float* _texCoord, int const _face, int const _vert) -> void
	{
		GenTangData* data = (GenTangData*)_ctx->m_pUserData;
		memcpy(_texCoord, &data->m_model->m_uvStream0[data->GetMeshVertIdx(_face, _vert)], sizeof(kt::Vec2));
	};

	mikktInterface.m_setTSpaceBasic = [](SMikkTSpaceContext const* _ctx, float const* _tangent, float const _sign, int const _face, int const _vert)
	{
		GenTangData* data = (GenTangData*)_ctx->m_pUserData;
		uint32_t const vertIdx = data->GetMeshVertIdx(_face, _vert);
		kt::Vec4& destTangent = data->m_model->m_tangentStream[vertIdx].m_tangentWithSign;
		destTangent.x = _tangent[0];
		destTangent.y = _tangent[1];
		destTangent.z = _tangent[2];
		destTangent.w = -_sign;
	};

	GenTangData tangData;
	tangData.m_model = _model;
	tangData.m_numFaces = (_idxEnd - _idxBegin) / 3;
	tangData.m_faceBegin = _idxBegin / 3;

	mikktInterface.m_setTSpace = nullptr;
	mikktCtx.m_pInterface = &mikktInterface;
	mikktCtx.m_pUserData = &tangData;

	tbool const mikktOk = genTangSpaceDefault(&mikktCtx);
	KT_ASSERT(mikktOk);
}

static bool LoadMeshes(Model* _model, cgltf_data* _data)
{
	for (cgltf_size meshIdx = 0; meshIdx < _data->meshes_count; ++meshIdx)
	{
		cgltf_mesh& gltfMesh = _data->meshes[meshIdx];

		for (cgltf_size primIdx = 0; primIdx < gltfMesh.primitives_count; ++primIdx)
		{
			cgltf_primitive& gltfPrim = gltfMesh.primitives[primIdx];
			if (!gltfPrim.indices)
			{
				KT_LOG_ERROR("gltf mesh %s has un-indexed primitive at index %u", gltfMesh.name ? gltfMesh.name : "Unnamed", primIdx);
				return false;
			}

			Model::SubMesh& subMesh = _model->m_meshes.PushBack();
			kt::AABB& boundingBox = _model->m_meshBoundingBoxes.PushBack();

			if (gltfPrim.material)
			{
				cgltf_size const materialIdx = gltfPrim.material - _data->materials;
				KT_ASSERT(materialIdx < UINT16_MAX);
				subMesh.m_materialIdx = uint16_t(materialIdx);
			}
			else
			{
				// TODO: gltf specifies a default material in this case.
				subMesh.m_materialIdx = 0;
			}

			subMesh.m_indexBufferStartOffset = _model->m_indicies.Size();
			

			// Copy index buffer.
			cgltf_accessor* indicies = gltfPrim.indices;
			subMesh.m_numIndicies = uint32_t(indicies->count);
			KT_ASSERT(!indicies->is_sparse); // surely not for index buffers?
			uint32_t oldIndexSize = _model->m_indicies.Size();
			_model->m_indicies.Resize(uint32_t(oldIndexSize + indicies->count));

			uint32_t const vertexBegin = _model->m_posStream.Size();

			switch (indicies->component_type)
			{
				case cgltf_component_type_r_8u:
				{
					CopyIndexBuffer<uint8_t>(indicies, _model->m_indicies.Data() + oldIndexSize, vertexBegin);
				} break;

				case cgltf_component_type_r_16u:
				{
					CopyIndexBuffer<uint16_t>(indicies, _model->m_indicies.Data() + oldIndexSize, vertexBegin);
				} break;

				case cgltf_component_type_r_32u:
				{
					CopyIndexBuffer<uint32_t>(indicies, _model->m_indicies.Data() + oldIndexSize, vertexBegin);
				} break;

				default:
				{
					KT_ASSERT(!"Unexpected index type!");
					return false;
				} break;
			}

			cgltf_attribute* normalAttr = nullptr;
			cgltf_attribute* tangentAttr = nullptr;

			for (cgltf_size attribIdx = 0; attribIdx < gltfPrim.attributes_count; ++attribIdx)
			{
				cgltf_attribute& attrib = gltfPrim.attributes[attribIdx];
				switch (attrib.type)
				{
					case cgltf_attribute_type_position:
					{
						if (attrib.data->component_type != cgltf_component_type_r_32f)
						{
							KT_LOG_ERROR("gltf mesh %s has non float positions", gltfMesh.name ? gltfMesh.name : "Unnamed");
							return false;
						}
						uint32_t const posStart = _model->m_posStream.Size();
						_model->m_posStream.Resize(uint32_t(posStart + attrib.data->count));
						KT_ASSERT(attrib.data->type == cgltf_type_vec3);
						CopyVertexStreamGeneric(attrib.data, (uint8_t*)(_model->m_posStream.Data() + posStart), sizeof(kt::Vec3));
						
						KT_ASSERT(attrib.data->has_max && attrib.data->has_min);
						memcpy(&boundingBox.m_min, attrib.data->min, sizeof(float) * 3);
						memcpy(&boundingBox.m_max, attrib.data->max, sizeof(float) * 3);
					} break;

					case cgltf_attribute_type_texcoord:
					{
						if (attrib.data->component_type != cgltf_component_type_r_32f)
						{
							KT_LOG_ERROR("gltf mesh %s has non float uv's", gltfMesh.name ? gltfMesh.name : "Unnamed");
							return false;
						}
						if (attrib.index != 0)
						{
							KT_LOG_ERROR("We don't support more than one uv stream at the moment.");
							//return false;
							continue;
						}
						uint32_t const uvStart = _model->m_uvStream0.Size();
						_model->m_uvStream0.Resize(uint32_t(uvStart + attrib.data->count));
						KT_ASSERT(attrib.data->type == cgltf_type_vec2);
						CopyVertexStreamGeneric(attrib.data, (uint8_t*)(_model->m_uvStream0.Data() + uvStart), sizeof(kt::Vec2));
					} break;

					case cgltf_attribute_type_color:
					{
						// TODO:
					} break;


					case cgltf_attribute_type_normal:
					{
						normalAttr = &attrib;
					} break;

					case cgltf_attribute_type_tangent:
					{
						tangentAttr = &attrib;
					} break;

					default:
					{
						// TODO:
					} break;
				}
			}

			if (!normalAttr)
			{
				KT_LOG_WARNING("No tangent space in mesh %s!", gltfMesh.name ? gltfMesh.name : "Unnamed");
				continue;
			}

			if (!!normalAttr == !!tangentAttr)
			{
				CopyPrecomputedTangentSpace(_model, normalAttr->data, tangentAttr->data);
			}
			else
			{
				// copy normals first.
				if (normalAttr->data->component_type != cgltf_component_type_r_32f)
				{
					KT_LOG_ERROR("gltf mesh %s has non float positions", gltfMesh.name ? gltfMesh.name : "Unnamed");
					return false;
				}
				uint32_t const normalStart = _model->m_tangentStream.Size();
				_model->m_tangentStream.Resize(uint32_t(normalStart + normalAttr->data->count));
				KT_ASSERT(normalAttr->data->type == cgltf_type_vec3);
				uint32_t const normOffs = offsetof(TangentSpace, m_norm);
				CopyVertexStreamGeneric(normalAttr->data, (uint8_t*)(_model->m_tangentStream.Data() + normalStart) + normOffs, sizeof(kt::Vec3), sizeof(TangentSpace));

				// TODO: We should really be recreating index buffer with new tangents (as verticies sharing faces will have different tangent space)
				GenMikktTangents(_model, oldIndexSize, _model->m_indicies.Size());
			}
		}
	}

	_model->m_boundingBox = kt::AABB::FloatMax();
	for (kt::AABB const& aabb : _model->m_meshBoundingBoxes)
	{
		_model->m_boundingBox = kt::Union(_model->m_boundingBox, aabb);
	}

	return true;
}

static void CreateGPUBuffers(Model* _model, kt::StringView _debugNamePrefix = kt::StringView{})
{
	gpu::BufferDesc posBufferDesc{};
	posBufferDesc.m_flags = gpu::BufferFlags::Vertex;
	posBufferDesc.m_strideInBytes = sizeof(kt::Vec3);
	posBufferDesc.m_sizeInBytes = _model->m_posStream.Size() * sizeof(kt::Vec3);

	kt::String64 name;
	kt::StringView const baseDebugName = _debugNamePrefix.Empty() ? kt::StringView("UNKNOWN") : _debugNamePrefix;
	name.Append(baseDebugName);

	name.Append("_pos_stream");
	_model->m_posGpuBuf = gpu::CreateBuffer(posBufferDesc, _model->m_posStream.Data(), name.Data());

	gpu::BufferDesc indexBufferDesc{};
	indexBufferDesc.m_flags = gpu::BufferFlags::Index;
	// TODO: fix index buffer size, also add gpu api to create buffer and get back upload pointer, so we don't need to make a temp array to convert to r16.
	indexBufferDesc.m_format = gpu::Format::R32_Uint;
	indexBufferDesc.m_sizeInBytes = sizeof(uint32_t) * _model->m_indicies.Size();
	indexBufferDesc.m_strideInBytes = sizeof(uint32_t);
	name.Clear();
	name.Append(baseDebugName);
	name.Append("_index");
	_model->m_indexGpuBuf = gpu::CreateBuffer(indexBufferDesc, _model->m_indicies.Data(), name.Data());

	name.Clear();
	name.Append(baseDebugName);
	name.Append("_uv0");
	gpu::BufferDesc uv0Desc;
	uv0Desc.m_flags = gpu::BufferFlags::Vertex;
	uv0Desc.m_format = gpu::Format::R32G32_Float;
	uv0Desc.m_sizeInBytes = _model->m_uvStream0.Size() * sizeof(kt::Vec2);
	uv0Desc.m_strideInBytes = sizeof(kt::Vec2);
	_model->m_uv0GpuBuf = gpu::CreateBuffer(uv0Desc, _model->m_uvStream0.Data(), name.Data());

	name.Clear();
	name.Append(baseDebugName);
	name.Append("_norm_tang");
	gpu::BufferDesc tangentDesc;
	tangentDesc.m_flags = gpu::BufferFlags::Vertex;
	tangentDesc.m_format = gpu::Format::Unknown;
	tangentDesc.m_sizeInBytes = _model->m_tangentStream.Size() * sizeof(TangentSpace);
	tangentDesc.m_strideInBytes = sizeof(TangentSpace);
	_model->m_tangentGpuBuf = gpu::CreateBuffer(tangentDesc, _model->m_tangentStream.Data(), name.Data());
}

static TextureResHandle LoadTexture(char const* _path, TextureLoadFlags _loadFlags)
{
	Texture* tex;
	bool wasAlreadycreated;
	TextureResHandle handle = res::CreateEmptyResource(_path, tex, wasAlreadycreated);
	if (wasAlreadycreated)
	{
		return handle;
	}

	tex->LoadFromFile(_path, _loadFlags);
	// TODO: Delete resource if this fails.
	return handle;
}

static TextureResHandle LoadTexture(char const* _gltfPath, char const* _imageUri, TextureLoadFlags _loadFlags)
{
	kt::FilePath path(_gltfPath);
	path = path.GetPath();

	path.Append(_imageUri);

	return LoadTexture(path.Data(), _loadFlags);
}

static void LoadMaterials(Model* _model, cgltf_data* _data, char const* _basePath)
{
	_model->m_materials.Resize(uint32_t(_data->materials_count));
	for (uint32_t materialIdx = 0; materialIdx < _data->materials_count; ++materialIdx)
	{
		Material& modelMat = _model->m_materials[materialIdx];
		cgltf_material const& gltfMat = _data->materials[materialIdx];
		if (gltfMat.has_pbr_metallic_roughness)
		{
			cgltf_pbr_metallic_roughness const& pbrMetalRough = gltfMat.pbr_metallic_roughness;
			modelMat.m_baseColour[0] = pbrMetalRough.base_color_factor[0];
			modelMat.m_baseColour[1] = pbrMetalRough.base_color_factor[1];
			modelMat.m_baseColour[2] = pbrMetalRough.base_color_factor[2];
			modelMat.m_baseColour[3] = pbrMetalRough.base_color_factor[3];
		
			modelMat.m_rougnessFactor = pbrMetalRough.roughness_factor;
			modelMat.m_metallicFactor = pbrMetalRough.metallic_factor;

			modelMat.m_alphaCutoff = gltfMat.alpha_cutoff;

			switch (gltfMat.alpha_mode)
			{
				case cgltf_alpha_mode_blend:	modelMat.m_alphaMode = Material::AlphaMode::Transparent; break;
				case cgltf_alpha_mode_mask:		modelMat.m_alphaMode = Material::AlphaMode::Mask; break;
				case cgltf_alpha_mode_opaque:	modelMat.m_alphaMode = Material::AlphaMode::Opaque; break;
			}

			// TODO: Samplers
			// TOdo: Transform
			if (pbrMetalRough.base_color_texture.texture)
			{
				modelMat.m_albedoTex = LoadTexture(_basePath, pbrMetalRough.base_color_texture.texture->image->uri, TextureLoadFlags::sRGB | TextureLoadFlags::GenMips);
			}

			if (pbrMetalRough.metallic_roughness_texture.texture)
			{
				modelMat.m_metallicRoughnessTex = LoadTexture(_basePath, pbrMetalRough.metallic_roughness_texture.texture->image->uri, TextureLoadFlags::GenMips);
			}
			
		}
		else
		{
			KT_ASSERT(!"Support other material models.");
		}

		if (gltfMat.normal_texture.texture)
		{
			modelMat.m_normalTex = LoadTexture(_basePath, gltfMat.normal_texture.texture->image->uri, TextureLoadFlags::Normalize | TextureLoadFlags::GenMips);
		}

		if (gltfMat.occlusion_texture.texture)
		{
			modelMat.m_occlusionTex = LoadTexture(_basePath, gltfMat.occlusion_texture.texture->image->uri, TextureLoadFlags::GenMips);
		}
	}
}

void SerializeMaterial(kt::ISerializer* _s, Material& _mat)
{
	kt::Serialize(_s, _mat.m_baseColour);
	kt::Serialize(_s, _mat.m_rougnessFactor);
	kt::Serialize(_s, _mat.m_metallicFactor);
	kt::Serialize(_s, _mat.m_alphaCutoff);
	kt::Serialize(_s, _mat.m_alphaMode);

	auto serializeTex = [&_s, &_mat](TextureResHandle& _handle, TextureLoadFlags _flags)
	{
		bool ok;
		kt::StaticString<512> pathSerialize;

		if (_s->SerializeMode() == kt::ISerializer::Mode::Read)
		{
			kt::Serialize(_s, ok);
			if (ok)
			{
				kt::Serialize(_s, pathSerialize);
				_handle = LoadTexture(pathSerialize.Data(), _flags);
			}
		}
		else
		{
			char const* path = res::GetResourcePath(_handle);
			ok = path != nullptr;
			kt::Serialize(_s, ok);
			if(ok)
			{
				pathSerialize = path;
				kt::Serialize(_s, pathSerialize);
			}
		}

	};
	
	serializeTex(_mat.m_albedoTex, TextureLoadFlags::sRGB | TextureLoadFlags::GenMips);
	serializeTex(_mat.m_normalTex, TextureLoadFlags::Normalize | TextureLoadFlags::GenMips);
	serializeTex(_mat.m_metallicRoughnessTex, TextureLoadFlags::GenMips);
	serializeTex(_mat.m_occlusionTex, TextureLoadFlags::GenMips);
}

uint32_t constexpr c_modelCacheVersion = 3;

bool SerializeModelCache(char const* _initialPath, kt::ISerializer* _s, Model& _model)
{
	uint32_t cache = c_modelCacheVersion;
	kt::Serialize(_s, cache);
	if (_s->SerializeMode() == kt::ISerializer::Mode::Read)
	{
		if (cache != c_modelCacheVersion)
		{
			KT_LOG_INFO("Model cache for %s invalid (current version is %u - file is %u).", _initialPath, c_modelCacheVersion, cache);
			return false;
		}
	}

	kt::Serialize(_s, _model.m_posStream);
	kt::Serialize(_s, _model.m_tangentStream);
	kt::Serialize(_s, _model.m_uvStream0);
	kt::Serialize(_s, _model.m_colourStream);
	kt::Serialize(_s, _model.m_indicies);
	kt::Serialize(_s, _model.m_meshes);
	kt::Serialize(_s, _model.m_boundingBox);
	kt::Serialize(_s, _model.m_meshBoundingBoxes);
	kt::Serialize(_s, _model.m_materials, SerializeMaterial);
	
	return true;
}


bool Model::LoadFromGLTF(char const* _path)
{
	// try cache
	kt::String512 cachePath(_path);
	cachePath.Append(".cache");

	bool hasReadFromCache = false;

	if (kt::FileExists(cachePath.Data()))
	{
		FILE* file = fopen(cachePath.Data(), "rb");
		
		if (!file)
		{
			KT_LOG_ERROR("Failed to open cache file %s", cachePath.Data());
		}
		else
		{
			KT_SCOPE_EXIT(fclose(file));
			kt::FileReader reader(file);
			kt::ISerializer serializer(&reader, c_modelCacheVersion);
			hasReadFromCache = SerializeModelCache(_path, &serializer, *this);
		}
	}

	if (hasReadFromCache)
	{
		kt::FilePath const path(_path);
		CreateGPUBuffers(this, path.GetFileNameNoExt());
		return true;
	}

	cgltf_data* data;
	cgltf_options opts{};
	cgltf_result res = cgltf_parse_file(&opts, _path, &data);
	
	if (res != cgltf_result_success)
	{
		KT_LOG_ERROR("Failed to parse \"%s\" - cgltf returned code: %u", _path, res);
		return false;
	}
	KT_SCOPE_EXIT(cgltf_free(data));

	res = cgltf_load_buffers(&opts, data, _path);
	
	if (res != cgltf_result_success)
	{
		KT_LOG_ERROR("Failed to load gltf buffers for \"%s\" - cgltf returned code: %u", _path, res);
		return false;
	}

	kt::FilePath const path(_path);

	LoadMaterials(this, data, _path);

	if (!LoadMeshes(this, data))
	{
		return false;
	}

	CreateGPUBuffers(this, path.GetFileNameNoExt());

	// Serialize to cache

	{
		FILE* file = fopen(cachePath.Data(), "wb");

		if (!file)
		{
			KT_LOG_ERROR("Failed to open cache file %s", cachePath.Data());
		}
		else
		{
			KT_SCOPE_EXIT(fclose(file));
			kt::FileWriter writer(file);
			kt::ISerializer serializer(&writer, c_modelCacheVersion);
			SerializeModelCache(_path, &serializer, *this);
		}
	}

	return true;
}

}