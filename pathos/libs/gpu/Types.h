#pragma once
#include <kt/kt.h>
#include <kt/Vec2.h>
#include <kt/Handles.h>

#include <string.h>


namespace gpu
{

// SRV is unbounded.
uint32_t constexpr c_cbvTableSize = 16;
uint32_t constexpr c_uavTableSize = 16;

uint32_t constexpr c_numShaderSpaces = 4;

uint32_t constexpr c_maxBufferedFrames = 3;

template <typename Tag>
struct TaggedHandle : kt::VersionedHandle
{
	TaggedHandle() = default;
	
	explicit TaggedHandle(kt::VersionedHandle _handle)
		: kt::VersionedHandle(_handle)
	{}

};

template<typename T>
bool operator==(TaggedHandle<T> const& _lhs, TaggedHandle<T> const& _rhs)
{
	return _lhs.m_packed == _rhs.m_packed;
}

template<typename T>
bool operator!=(TaggedHandle<T> const& _lhs, TaggedHandle<T> const& _rhs)
{
	return _lhs.m_packed != _rhs.m_packed;
}

struct ShaderTag;
struct PSOTag;
struct ResourceTag;
struct PersistentDescriptorTableTag;

using PSOHandle							= TaggedHandle<PSOTag>;
using ShaderHandle						= TaggedHandle<ShaderTag>;
using ResourceHandle					= TaggedHandle<ResourceTag>;
using PersistentDescriptorTableHandle	= TaggedHandle<PersistentDescriptorTableTag>;

using QueryIndex						= uint32_t;

struct BufferHandle : ResourceHandle
{
	BufferHandle() = default;

	explicit BufferHandle(ResourceHandle _handle)
		: ResourceHandle(_handle)
	{
	}
};

struct TextureHandle : ResourceHandle
{
	TextureHandle() = default;

	explicit TextureHandle(ResourceHandle _handle)
		: ResourceHandle(_handle)
	{
	}
};

struct ShaderBytecode
{
	void const* m_data = nullptr;
	size_t m_size = 0;
};

enum class ResourceState : uint8_t
{
	Unknown,

	Common,
	
	IndexBuffer,
	VertexBuffer,
	ConstantBuffer,

	RenderTarget,
	
	DepthStencilTarget,
	DepthStencilTarget_ReadOnly,
	
	ShaderResource,
	UnorderedAccess,
	
	CopyDest,
	CopySrc,

	IndirectArg,
	
	Present,

	Num_ResourceState
};

enum class ResourceType : uint8_t
{
	Buffer,

	Texture1D,
	Texture2D,
	Texture3D,
	TextureCube,

	Num_ResourceType
};

inline bool IsTexture(ResourceType _type)
{
	return _type != ResourceType::Buffer;
}

// Texture/Buffer format.
enum class Format : uint32_t
{
	Unknown,

	R8G8_UNorm,
	R8G8_SNorm,

	R8G8B8A8_UNorm,
	R8G8B8A8_UNorm_SRGB,
	R8G8B8A8_SNorm,

	R10G10B10A2_UNorm,

	R32_Float,
	R32G32_Float,
	R32G32B32_Float,
	R32G32B32A32_Float,

	R16_Float,
	R16B16_Float,
	R16B16G16A16_Float,

	R16_Uint,
	R32_Uint,
	
	D32_Float,

	Num_Format
};

uint32_t GetFormatSize(gpu::Format _fmt);
char const* GetFormatName(gpu::Format _fmt);

bool IsSRGBFormat(gpu::Format _fmt);
bool IsDepthFormat(gpu::Format _fmt);

enum class BufferFlags : uint32_t
{
	None				= 0x0,

	Vertex				= 0x1,	// Vertex buffer. 
	Index				= 0x2,	// Index buffer.

	UnorderedAccess		= 0x4,	// UAV	
	ShaderResource		= 0x8,	// SRV
	Constant			= 0x10,	// Constant buffer

	Transient			= 0x20,	// Transient resource, must be updated the same frame it is used.
	Dynamic				= 0x40 // Dynamic resource, can be updated.
};

KT_ENUM_CLASS_FLAG_OPERATORS(BufferFlags);

enum class TextureUsageFlags
{
	None = 0x0,

	ShaderResource = 0x1,
	UnorderedAccess = 0x2,
	RenderTarget = 0x4,
	DepthStencil = 0x8,
};

KT_ENUM_CLASS_FLAG_OPERATORS(TextureUsageFlags);

union ClearValue
{
	float m_colour[4];

	struct  
	{
		float m_depth;
		uint32_t m_stencil;
	} ds;
};

// Texture description.
struct TextureDesc
{
	static TextureDesc Desc1D(uint32_t _width, TextureUsageFlags _flags, Format _fmt);
	static TextureDesc Desc2D(uint32_t _width, uint32_t _height, TextureUsageFlags _flags, Format _fmt);
	static TextureDesc Desc3D(uint32_t _width, uint32_t _height, uint32_t _depth, TextureUsageFlags _flags, Format _fmt);
	static TextureDesc DescCube(uint32_t _width, uint32_t _height, TextureUsageFlags _flags, Format _fmt);

	ResourceType m_type = ResourceType::Num_ResourceType;
	Format m_format = Format::Num_Format;
	TextureUsageFlags m_usageFlags = TextureUsageFlags::None;

	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_depth = 0;

	uint32_t m_mipLevels = 1;
	uint32_t m_arraySlices = 1;

	ClearValue m_clear;
};

// Buffer description
struct BufferDesc
{
	BufferFlags m_flags = BufferFlags::None;
	Format m_format = Format::Unknown;
	uint32_t m_sizeInBytes = 0;
	uint32_t m_strideInBytes = 0;
};

enum class ShaderType
{
	Vertex,
	Pixel,
	Compute,

	Num_ShaderType
};

//////////////////////////////////////////////////////////////////////////
// Vertex decl
//////////////////////////////////////////////////////////////////////////

enum class VertexSemantic : uint8_t
{
	Position,
	TexCoord,
	Normal,
	Tangent,
	Bitangent,
	Color,

	Num_VertexSemantic
};


struct VertexDeclEntry
{
	VertexDeclEntry() = default;

	VertexDeclEntry(Format _fmt, VertexSemantic _semantic, bool _isInstanceData, uint8_t _semanticIdx = 0, uint8_t _streamIdx = 0)
		: m_format(_fmt)
		, m_semanticIndex(_semanticIdx)
		, m_streamIdx(_streamIdx)
		, m_semantic(_semantic)
		, m_isInstanceData(_isInstanceData)
	{}

	Format m_format;
	uint8_t m_semanticIndex;
	uint8_t m_streamIdx;
	VertexSemantic m_semantic : 7;
	uint8_t m_isInstanceData : 1;
};

uint32_t constexpr c_maxVertexElements = 16;
uint32_t constexpr c_maxVertexStreams = 8;

struct VertexLayout
{
	VertexLayout()
	{
		memset(this, 0, sizeof(VertexLayout));
	}

	VertexLayout& Add(VertexDeclEntry const& _decl)
	{
		KT_ASSERT(_decl.m_streamIdx < c_maxVertexStreams);
		KT_ASSERT(m_numElements < c_maxVertexElements);
		m_elements[m_numElements++] = _decl;
		return *this;
	}

	VertexLayout& Add(Format _fmt, VertexSemantic _semantic, bool _isInstanceData, uint8_t _semanticIdx = 0, uint8_t _streamIdx = 0)
	{
		Add(VertexDeclEntry{ _fmt, _semantic, _isInstanceData, _semanticIdx, _streamIdx });
		return *this;
	}

	VertexDeclEntry m_elements[c_maxVertexElements];
	uint32_t m_numElements = 0;
};

enum class PrimitiveType : uint8_t
{
	PointList,
	LineList,
	LineStrip,
	TriangleList,
	TriangleStrip,

	Num_PrimitiveType
};

//////////////////////////////////////////////////////////////////////////
// Rasterizer
//////////////////////////////////////////////////////////////////////////

enum class FillMode : uint8_t
{
	Solid,
	WireFrame,

	Num_FillMode
};
uint32_t constexpr c_fillModeBits = 1;
static_assert(uint32_t(FillMode::Num_FillMode) <= (1 << c_fillModeBits), "FillMode packing gone wrong.");

enum class CullMode : uint8_t
{
	None,
	Front,
	Back,

	Num_CullMode
};
uint32_t constexpr c_cullModeBits = 2;
static_assert(uint32_t(CullMode::Num_CullMode) <= (1 << c_cullModeBits), "CullMode packing gone wrong.");

struct RasterizerDesc
{
	RasterizerDesc()
	{
		m_fillMode = FillMode::Solid;
		m_cullMode = CullMode::Back;
		m_frontFaceCCW = 0;
	}

	float m_scopedScaledDepthBias = 0.0f;
	float m_depthBias = 0.0f;
	FillMode m_fillMode : c_fillModeBits;
	CullMode m_cullMode : c_cullModeBits;
	uint8_t m_frontFaceCCW : 1;
};

//////////////////////////////////////////////////////////////////////////
// Depth stencil
//////////////////////////////////////////////////////////////////////////

enum class ComparisonFn : uint8_t
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual, 
	Always,

	Num_ComparisonFn
};
uint32_t constexpr c_cmpFnBits = 3;
static_assert(uint32_t(ComparisonFn::Num_ComparisonFn) <= (1 << c_cmpFnBits), "ComparisonFn packing gone wrong.");

struct DepthStencilDesc
{
	DepthStencilDesc()
	{
		m_depthEnable = 1;
		m_depthWrite = 1;
		m_depthFn = ComparisonFn::Less;

		m_stencilEnable = 0;
	}

	uint8_t m_depthEnable		: 1;
	uint8_t m_depthWrite		: 1;
	ComparisonFn m_depthFn		: c_cmpFnBits;

	// Todo: Stencil..
	uint8_t m_stencilEnable		: 1;
	uint8_t m_stencilReadMask	= 0xFF;
	uint8_t m_stencilWriteMask	= 0xFF;
};

//////////////////////////////////////////////////////////////////////////
// Blend
//////////////////////////////////////////////////////////////////////////
enum class BlendMode : uint8_t
{
	Zero,
	One,
	
	SrcColor,
	InvSrcColor,

	SrcAlpha,
	InvSrcAlpha,

	DestAlpha,
	InvDestAlpha,

	DestColor,
	InvDestColor,

	Num_BlendMode
};
uint32_t constexpr c_blendModeBits = 4;
static_assert(uint32_t(BlendMode::Num_BlendMode) <= (1 << c_blendModeBits), "BlendMode packing gone wrong.");

enum class BlendOp : uint8_t
{
	Add,
	Sub,
	RevSub,
	Min,
	Max,

	Num_BlendOp
};
uint32_t constexpr c_blendOpBits = 3;
static_assert(uint32_t(BlendOp::Num_BlendOp) <= (1 << c_blendOpBits), "BlendOp packing gone wrong.");

struct BlendDesc
{
	BlendDesc()
	{
		SetOpaque();
	}

	void SetOpaque();

	uint32_t m_blendEnable : 1;
	uint32_t m_alphaToCoverageEnable : 1;

	BlendMode m_srcBlend : c_blendModeBits;
	BlendMode m_destBlend : c_blendModeBits;

	BlendOp m_blendOp : c_blendOpBits;

	BlendMode m_srcAlpha : c_blendModeBits;
	BlendMode m_destAlpha : c_blendModeBits;

	BlendOp m_blendOpAlpha : c_blendOpBits;
};

uint32_t constexpr c_maxRenderTargets = 16u;

struct GraphicsPSODesc
{
	ShaderHandle m_vs;
	ShaderHandle m_ps;

	RasterizerDesc m_rasterDesc;
	DepthStencilDesc m_depthStencilDesc;
	BlendDesc m_blendDesc;
	VertexLayout m_vertexLayout;
	PrimitiveType m_primType = PrimitiveType::TriangleList;

	Format m_renderTargetFormats[c_maxRenderTargets];
	uint32_t m_numRenderTargets = 1;

	Format m_depthFormat = Format::Unknown;
};

struct Rect
{
	Rect() = default;
	Rect(float _width, float _height)
		: m_topLeft(0.0f)
		, m_bottomRight(_width, _height)
	{}

	kt::Vec2 m_topLeft;
	kt::Vec2 m_bottomRight;
};


struct DescriptorData
{
	DescriptorData() : res(), m_type(Type::View) {}

	void Set(gpu::ResourceHandle _handle, uint32_t _uavMipIdx = 0)
	{
		m_type = Type::View;
		res.m_handle = _handle;
		res.m_uavMipIdx = _uavMipIdx;
		res.m_srvCubeAsArray = 0;
	}

	void SetSRVCubeAsArray(gpu::ResourceHandle _handle)
	{
		m_type = Type::View;
		res.m_handle = _handle;
		res.m_uavMipIdx = 0;
		res.m_srvCubeAsArray = 1;
	}

	void Set(void const* _ptr, uint32_t _size)
	{
		m_type = Type::ScratchConstant;
		constants.m_ptr = _ptr;
		constants.m_size = _size;
	}

	void SetNull()
	{
		m_type = Type::View;
		res.m_handle = gpu::ResourceHandle{};
	}

	union
	{
		struct Resource
		{
			Resource() : m_handle() {}

			gpu::ResourceHandle m_handle;
			uint32_t m_uavMipIdx : 31;
			uint32_t m_srvCubeAsArray : 1;
		} res;

		struct
		{
			void const* m_ptr;
			uint32_t m_size;
		} constants;
	};

	// TODO: Pedantic, but this could be packed into union padding.
	enum class Type
	{
		View,
		ScratchConstant
	} m_type;
};

struct IndexedDrawArguments
{
	uint32_t m_indicesPerInstance;
	uint32_t m_instanceCount;
	uint32_t m_indexStart;
	int32_t m_baseVertex;
	uint32_t m_startInstance;
};

}