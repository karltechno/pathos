#include "Model.h"

#include <kt/Logging.h>
#include <kt/FilePath.h>
#include <kt/Serialization.h>
#include <kt/File.h>

#include "cgltf.h"
#include "mikktspace.h"

#include "Scene.h"
#include "Material.h"
#include "ResourceManager.h"


namespace gfx
{

static TextureLoadFlags const c_albedoTexLoadFlags		=	TextureLoadFlags::sRGB | TextureLoadFlags::GenMips;
static TextureLoadFlags const c_normalTexLoadFlags		=	TextureLoadFlags::Normalize | TextureLoadFlags::GenMips;
static TextureLoadFlags const c_metalRoughTexLoadFlags	=	TextureLoadFlags::GenMips;
static TextureLoadFlags const c_occlusionTexLoadFlags	=	TextureLoadFlags::GenMips;


gpu::VertexLayout Model::FullVertexLayout()
{
	gpu::VertexLayout layout;
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, false, 0, 0);
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Normal, false, 0, 1);
	layout.Add(gpu::Format::R32G32B32A32_Float, gpu::VertexSemantic::Tangent, false, 0, 1);
	layout.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::TexCoord, false, 0, 2);
	return layout;
}

gpu::VertexLayout Model::FullVertexLayoutInstanced()
{
	gpu::VertexLayout layout;
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Position, false, 0, 0);
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::Normal, false, 0, 1);
	layout.Add(gpu::Format::R32G32B32A32_Float, gpu::VertexSemantic::Tangent, false, 0, 1);
	layout.Add(gpu::Format::R32G32_Float, gpu::VertexSemantic::TexCoord, false, 0, 2);

	// instance data
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::TexCoord, true, 1, 3);
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::TexCoord, true, 2, 3);
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::TexCoord, true, 3, 3);
	layout.Add(gpu::Format::R32G32B32_Float, gpu::VertexSemantic::TexCoord, true, 4, 3);
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
		uint32_t indices[3];
		indices[0] = (uint32_t)*(IntT*)(src) + _vtxOffset;
		src += stride;
		indices[1] = (uint32_t)*(IntT*)(src) + _vtxOffset;
		src += stride;
		indices[2] = (uint32_t)*(IntT*)(src) + _vtxOffset;
		src += stride;

		// Note: Swizzling indices here to swap winding order from CCW -> CW.
		*o_dest++ = indices[1];
		*o_dest++ = indices[0];
		*o_dest++ = indices[2];
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

static void CopyPrecomputedTangentSpace(Mesh* _model, cgltf_accessor* _normalAccessor, cgltf_accessor* _tangentAccessor)
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

static void GenMikktTangents(Mesh* _model, uint32_t _idxBegin, uint32_t _idxEnd)
{
	SMikkTSpaceContext mikktCtx{};
	SMikkTSpaceInterface mikktInterface{};

	struct GenTangData
	{
		uint32_t GetMeshVertIdx(int _face, int _vert) const
		{
			uint32_t const faceIdx = (uint32_t)_face + m_faceBegin;
			KT_ASSERT(uint32_t(faceIdx * 3) < m_model->m_indices.Size());
			return m_model->m_indices[uint32_t(faceIdx * 3 + _vert)];
		}

		Mesh* m_model;
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

static void LoadNodes(Model& io_model, cgltf_data* _data)
{
	for (uint32_t i = 0; i < _data->nodes_count; ++i)
	{
		cgltf_node* gltfNode = _data->nodes + i;

		// For now, only care about nodes with meshes.
		if (!gltfNode->mesh)
		{
			continue;
		}

		Model::Node& instance = io_model.m_nodes.PushBack();
		instance.m_mtx = kt::Mat4::Identity();
		instance.m_internalMeshIdx = uint32_t(gltfNode->mesh - _data->meshes);
		cgltf_node_transform_world(gltfNode, instance.m_mtx.Data());
	}
}

static bool LoadMeshes(Model* _model, cgltf_data* _data, kt::Slice<ResourceManager::MaterialIdx> const& _materialIndicies)
{
	for (cgltf_size gltfMeshIdx = 0; gltfMeshIdx < _data->meshes_count; ++gltfMeshIdx)
	{
		_model->m_meshes.PushBack(gfx::ResourceManager::CreateMesh());
		gfx::Mesh& mesh = *gfx::ResourceManager::GetMesh(_model->m_meshes.Back());
		cgltf_mesh& gltfMesh = _data->meshes[gltfMeshIdx];
		
		if (gltfMesh.name)
		{
			mesh.m_name = gltfMesh.name;
		}
		else
		{
			mesh.m_name.AppendFmt("%s_mesh%u", _model->m_name.c_str(), uint32_t(gltfMeshIdx));
		}

		for (cgltf_size primIdx = 0; primIdx < gltfMesh.primitives_count; ++primIdx)
		{
			cgltf_primitive& gltfPrim = gltfMesh.primitives[primIdx];
			if (!gltfPrim.indices)
			{
				KT_LOG_ERROR("gltf mesh %s has un-indexed primitive at index %u", gltfMesh.name ? gltfMesh.name : "Unnamed", primIdx);
				return false;
			}

			Mesh::SubMesh& subMesh = mesh.m_subMeshes.PushBack();
			kt::AABB& boundingBox = mesh.m_subMeshBoundingBoxes.PushBack();

			if (gltfPrim.material)
			{
				cgltf_size const materialIdx = gltfPrim.material - _data->materials;
				subMesh.m_materialIdx = _materialIndicies[uint32_t(materialIdx)];
			}
			else
			{
				// TODO: gltf specifies a default material in this case.
				subMesh.m_materialIdx = ResourceManager::MaterialIdx{};
			}

			subMesh.m_indexBufferStartOffset = mesh.m_indices.Size();
			

			// Copy index buffer.
			cgltf_accessor* indices = gltfPrim.indices;
			subMesh.m_numIndices = uint32_t(indices->count);
			KT_ASSERT(!indices->is_sparse); // surely not for index buffers?
			uint32_t oldIndexSize = mesh.m_indices.Size();
			mesh.m_indices.Resize(uint32_t(oldIndexSize + indices->count));

			uint32_t const vertexBegin = mesh.m_posStream.Size();

			switch (indices->component_type)
			{
				case cgltf_component_type_r_8u:
				{
					CopyIndexBuffer<uint8_t>(indices, mesh.m_indices.Data() + oldIndexSize, vertexBegin);
				} break;

				case cgltf_component_type_r_16u:
				{
					CopyIndexBuffer<uint16_t>(indices, mesh.m_indices.Data() + oldIndexSize, vertexBegin);
				} break;

				case cgltf_component_type_r_32u:
				{
					CopyIndexBuffer<uint32_t>(indices, mesh.m_indices.Data() + oldIndexSize, vertexBegin);
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
						uint32_t const posStart = mesh.m_posStream.Size();
						mesh.m_posStream.Resize(uint32_t(posStart + attrib.data->count));
						KT_ASSERT(attrib.data->type == cgltf_type_vec3);
						CopyVertexStreamGeneric(attrib.data, (uint8_t*)(mesh.m_posStream.Data() + posStart), sizeof(kt::Vec3));
						
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
						uint32_t const uvStart = mesh.m_uvStream0.Size();
						mesh.m_uvStream0.Resize(uint32_t(uvStart + attrib.data->count));
						KT_ASSERT(attrib.data->type == cgltf_type_vec2);
						CopyVertexStreamGeneric(attrib.data, (uint8_t*)(mesh.m_uvStream0.Data() + uvStart), sizeof(kt::Vec2));
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
				CopyPrecomputedTangentSpace(&mesh, normalAttr->data, tangentAttr->data);
			}
			else
			{
				// copy normals first.
				if (normalAttr->data->component_type != cgltf_component_type_r_32f)
				{
					KT_LOG_ERROR("gltf mesh %s has non float positions", gltfMesh.name ? gltfMesh.name : "Unnamed");
					return false;
				}
				uint32_t const normalStart = mesh.m_tangentStream.Size();
				mesh.m_tangentStream.Resize(uint32_t(normalStart + normalAttr->data->count));
				KT_ASSERT(normalAttr->data->type == cgltf_type_vec3);
				uint32_t const normOffs = offsetof(TangentSpace, m_norm);
				CopyVertexStreamGeneric(normalAttr->data, (uint8_t*)(mesh.m_tangentStream.Data() + normalStart) + normOffs, sizeof(kt::Vec3), sizeof(TangentSpace));

				// TODO: We should really be recreating index buffer with new tangents (as verticies sharing faces will have different tangent space)
				GenMikktTangents(&mesh, oldIndexSize, mesh.m_indices.Size());
			}
		}

		mesh.m_boundingBox = kt::AABB::FloatMax();

		for (kt::AABB const& aabb : mesh.m_subMeshBoundingBoxes)
		{
			mesh.m_boundingBox = kt::Union(_model->m_boundingBox, aabb);
		}
	}

	return true;
}

void Mesh::CreateGPUBuffers(bool _keepDataOnCpu)
{
	char const* const baseDebugName = m_name.Empty() ? "UNKNOWN" : m_name.Data();
	kt::String64 name;
	name.Append(baseDebugName);

	if (m_posStream.Size() > 0)
	{
		gpu::BufferDesc posBufferDesc{};
		posBufferDesc.m_flags = gpu::BufferFlags::Vertex;
		posBufferDesc.m_strideInBytes = sizeof(kt::Vec3);
		posBufferDesc.m_sizeInBytes = m_posStream.Size() * sizeof(kt::Vec3);
		name.Append("_pos_stream");
		m_posGpuBuf = gpu::CreateBuffer(posBufferDesc, m_posStream.Data(), name.Data());
		
	}

	if (m_indices.Size() > 0)
	{
		gpu::BufferDesc indexBufferDesc{};
		indexBufferDesc.m_flags = gpu::BufferFlags::Index;
		// TODO: fix index buffer size, also add gpu api to create buffer and get back upload pointer, so we don't need to make a temp array to convert to r16.
		indexBufferDesc.m_format = gpu::Format::R32_Uint;
		indexBufferDesc.m_sizeInBytes = sizeof(uint32_t) * m_indices.Size();
		indexBufferDesc.m_strideInBytes = sizeof(uint32_t);
		name.Clear();
		name.Append(baseDebugName);
		name.Append("_index");
		m_indexGpuBuf = gpu::CreateBuffer(indexBufferDesc, m_indices.Data(), name.Data());
	}

	if (m_uvStream0.Size() > 0)
	{
		name.Clear();
		name.Append(baseDebugName);
		name.Append("_uv0");
		gpu::BufferDesc uv0Desc;
		uv0Desc.m_flags = gpu::BufferFlags::Vertex;
		uv0Desc.m_format = gpu::Format::R32G32_Float;
		uv0Desc.m_sizeInBytes = m_uvStream0.Size() * sizeof(kt::Vec2);
		uv0Desc.m_strideInBytes = sizeof(kt::Vec2);
		m_uv0GpuBuf = gpu::CreateBuffer(uv0Desc, m_uvStream0.Data(), name.Data());
	}

	if (m_tangentStream.Size() > 0)
	{
		name.Clear();
		name.Append(baseDebugName);
		name.Append("_norm_tang");
		gpu::BufferDesc tangentDesc;
		tangentDesc.m_flags = gpu::BufferFlags::Vertex;
		tangentDesc.m_format = gpu::Format::Unknown;
		tangentDesc.m_sizeInBytes = m_tangentStream.Size() * sizeof(TangentSpace);
		tangentDesc.m_strideInBytes = sizeof(TangentSpace);
		m_tangentGpuBuf = gpu::CreateBuffer(tangentDesc, m_tangentStream.Data(), name.Data());
	}

	if (!_keepDataOnCpu)
	{
		m_posStream.ClearAndFree();
		m_tangentStream.ClearAndFree();
		m_uvStream0.ClearAndFree();
		m_colourStream.ClearAndFree();
		m_indices.ClearAndFree();
	}
}

static ResourceManager::TextureIdx LoadTexture(char const* _path, TextureLoadFlags _loadFlags)
{
	return ResourceManager::CreateTextureFromFile(_path, _loadFlags);
}

static ResourceManager::TextureIdx LoadTexture(char const* _gltfPath, char const* _imageUri, TextureLoadFlags _loadFlags)
{
	kt::FilePath path(_gltfPath);
	path = path.GetPath();

	path.Append(_imageUri);

	return LoadTexture(path.Data(), _loadFlags);
}

static void LoadMaterials(Model* io_model, cgltf_data* _data, char const* _basePath, kt::Slice<ResourceManager::MaterialIdx> const& _materialIndicies)
{
	for (uint32_t materialIdx = 0; materialIdx < _data->materials_count; ++materialIdx)
	{
		gfx::Material& modelMat = *ResourceManager::GetMaterial(_materialIndicies[materialIdx]);
		cgltf_material const& gltfMat = _data->materials[materialIdx];

		if (gltfMat.name)
		{
			modelMat.m_name = gltfMat.name;
		}
		else
		{
			modelMat.m_name.AppendFmt("%s_mat%u", io_model->m_name.c_str(), materialIdx);
		}

		if (gltfMat.has_pbr_metallic_roughness)
		{
			cgltf_pbr_metallic_roughness const& pbrMetalRough = gltfMat.pbr_metallic_roughness;
			modelMat.m_params.m_baseColour[0] = pbrMetalRough.base_color_factor[0];
			modelMat.m_params.m_baseColour[1] = pbrMetalRough.base_color_factor[1];
			modelMat.m_params.m_baseColour[2] = pbrMetalRough.base_color_factor[2];
			modelMat.m_params.m_baseColour[3] = pbrMetalRough.base_color_factor[3];
		
			modelMat.m_params.m_roughnessFactor = pbrMetalRough.roughness_factor;
			modelMat.m_params.m_metallicFactor = pbrMetalRough.metallic_factor;

			modelMat.m_params.m_alphaCutoff = gltfMat.alpha_cutoff;

			switch (gltfMat.alpha_mode)
			{
				case cgltf_alpha_mode_blend:	modelMat.m_params.m_alphaMode = Material::AlphaMode::Transparent; break;
				case cgltf_alpha_mode_mask:		modelMat.m_params.m_alphaMode = Material::AlphaMode::Mask; break;
				case cgltf_alpha_mode_opaque:	modelMat.m_params.m_alphaMode = Material::AlphaMode::Opaque; break;
			}

			// TODO: Samplers
			// TOdo: Transform
			if (pbrMetalRough.base_color_texture.texture)
			{
				modelMat.m_textures[Material::Albedo] = LoadTexture(_basePath, pbrMetalRough.base_color_texture.texture->image->uri, c_albedoTexLoadFlags);
			}

			if (pbrMetalRough.metallic_roughness_texture.texture)
			{
				modelMat.m_textures[Material::MetallicRoughness] = LoadTexture(_basePath, pbrMetalRough.metallic_roughness_texture.texture->image->uri, c_metalRoughTexLoadFlags);
			}
			
		}
		else
		{
			KT_ASSERT(!"Support other material models.");
		}

		if (gltfMat.normal_texture.texture)
		{
			modelMat.m_textures[Material::Normal] = LoadTexture(_basePath, gltfMat.normal_texture.texture->image->uri, c_normalTexLoadFlags);
		}

		if (gltfMat.occlusion_texture.texture)
		{
			modelMat.m_textures[Material::Occlusion] = LoadTexture(_basePath, gltfMat.occlusion_texture.texture->image->uri, TextureLoadFlags::GenMips);
		}
	}
}

void SerializeMaterial(kt::ISerializer* _s, Material& _mat)
{
	kt::Serialize(_s, _mat.m_params);
	kt::Serialize(_s, _mat.m_name);

	auto serializeTex = [&_s, &_mat](ResourceManager::TextureIdx& _handle, TextureLoadFlags _flags)
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
			gfx::Texture* tex = ResourceManager::GetTexture(_handle);
			ok = tex != nullptr;
			kt::Serialize(_s, ok);
			if(ok)
			{
				pathSerialize = tex->m_path.c_str();
				kt::Serialize(_s, pathSerialize);
			}
		}

	};
	
	serializeTex(_mat.m_textures[Material::Albedo], c_albedoTexLoadFlags);
	serializeTex(_mat.m_textures[Material::Normal], c_normalTexLoadFlags);
	serializeTex(_mat.m_textures[Material::MetallicRoughness], c_metalRoughTexLoadFlags);
	serializeTex(_mat.m_textures[Material::Occlusion], c_occlusionTexLoadFlags);
}

uint32_t constexpr c_modelCacheVersion = 10;

static void SerializeMesh(kt::ISerializer* _s, Mesh& _mesh)
{
	kt::Serialize(_s, _mesh.m_posStream);
	kt::Serialize(_s, _mesh.m_tangentStream);
	kt::Serialize(_s, _mesh.m_uvStream0);
	kt::Serialize(_s, _mesh.m_colourStream);
	kt::Serialize(_s, _mesh.m_indices);
	kt::Serialize(_s, _mesh.m_subMeshes);
	kt::Serialize(_s, _mesh.m_boundingBox);
	kt::Serialize(_s, _mesh.m_subMeshBoundingBoxes);
}

bool SerializeModelCache(char const* _initialPath, kt::ISerializer* _s, Model& _model, kt::Slice<ResourceManager::MaterialIdx> const* _matsToWrite = nullptr)
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

	// Do the easy stuff first.
	kt::Serialize(_s, _model.m_boundingBox);
	kt::Serialize(_s, _model.m_nodes);

	struct MaterialRemapData
	{
		gfx::ResourceManager::MaterialIdx m_serializedIdx;
		gfx::ResourceManager::MaterialIdx m_newIdx;
	};
	kt::Array<MaterialRemapData> remapData;

	uint32_t materialSize = _matsToWrite ? _matsToWrite->Size() : 0;
	kt::Serialize(_s, materialSize);
	
	// First, serialize material remapping data.
	if (_s->SerializeMode() == kt::ISerializer::Mode::Write)
	{
		KT_ASSERT(_matsToWrite);
		for (uint32_t i = 0; i < _matsToWrite->Size(); ++i)
		{
			ResourceManager::MaterialIdx matIdx = (*_matsToWrite)[i];
			kt::Serialize(_s, matIdx);
			SerializeMaterial(_s, *ResourceManager::GetMaterial(matIdx));
		}
	}
	else
	{
		remapData.Resize(materialSize);
		for (MaterialRemapData& remapMaterial : remapData)
		{
			kt::Serialize(_s, remapMaterial.m_serializedIdx);
			remapMaterial.m_newIdx = ResourceManager::CreateMaterial();
			gfx::Material* newMat = ResourceManager::GetMaterial(remapMaterial.m_newIdx);
			SerializeMaterial(_s, *newMat);
		}
	}

	// Now serialize meshes.
	uint32_t meshCount = _model.m_meshes.Size();

	kt::Serialize(_s, meshCount);

	if (_s->SerializeMode() == kt::ISerializer::Mode::Read)
	{
		_model.m_meshes.Resize(meshCount);
		for (ResourceManager::MeshIdx& meshIdx : _model.m_meshes)
		{
			meshIdx = ResourceManager::CreateMesh();
			gfx::Mesh& newMesh = *ResourceManager::GetMesh(meshIdx);
			SerializeMesh(_s, newMesh);
			newMesh.CreateGPUBuffers();
			// if we are reading, we need to remap submesh material indices.
			for (gfx::Mesh::SubMesh& subMesh : newMesh.m_subMeshes)
			{
				bool remapped = false;
				for (MaterialRemapData const& remapMaterial : remapData)
				{
					if (remapMaterial.m_serializedIdx == subMesh.m_materialIdx)
					{
						remapped = true;
						subMesh.m_materialIdx = remapMaterial.m_newIdx;
						break;
					}
				}
				KT_ASSERT(remapped);
			}
		}

	}
	else
	{
		for (ResourceManager::MeshIdx& meshIdx : _model.m_meshes)
		{
			SerializeMesh(_s, *ResourceManager::GetMesh(meshIdx));
		}
	}
	
	return true;
}

bool Model::LoadFromGLTF(char const* _path)
{
	m_name = _path;

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

	ResourceManager::MaterialIdx* materialIndices = (ResourceManager::MaterialIdx*)KT_ALLOCA(sizeof(ResourceManager::MaterialIdx) * data->materials_count);

	kt::Slice<ResourceManager::MaterialIdx> materialSlice = kt::MakeSlice(materialIndices, materialIndices + data->materials_count);

	for (ResourceManager::MaterialIdx& materialIdx : materialSlice)
	{
		materialIdx = ResourceManager::CreateMaterial();
	}

	LoadMaterials(this, data, _path, materialSlice);

	if (!LoadMeshes(this, data, materialSlice))
	{
		return false;
	}

	LoadNodes(*this, data);

	m_boundingBox = kt::AABB::FloatMax();

	for (Model::Node const& node : m_nodes)
	{
		gfx::Mesh const& mesh = *gfx::ResourceManager::GetMesh(m_meshes[node.m_internalMeshIdx]);
		m_boundingBox = kt::Union(mesh.m_boundingBox.Transformed(node.m_mtx), m_boundingBox);
	}

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
			SerializeModelCache(_path, &serializer, *this, &materialSlice);
		}
	}

	// Create gpu buffers and free data on cpu.
	for (ResourceManager::MeshIdx meshIdx : m_meshes)
	{
		ResourceManager::GetMesh(meshIdx)->CreateGPUBuffers();
	}

	return true;
}

}