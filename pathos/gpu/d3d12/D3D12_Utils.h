#include <d3d12.h>
#include <guiddef.h>

#include <string.h>

#include <kt/Macros.h>

constexpr GUID c_d3dDebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };

#define D3D_SET_DEBUG_NAME(_obj, _name) \
	(_obj)->SetPrivateData(c_d3dDebugObjectName, UINT(strlen(_name)), _name)

#if KT_DEBUG
	#define D3D_CHECK(_expr) \
		KT_MACRO_BLOCK_BEGIN \
			HRESULT const KT_STRING_JOIN(hr_, __LINE__) = _expr; \
			KT_ASSERT(SUCCEEDED(KT_STRING_JOIN(hr_, __LINE__))) \
		KT_MACRO_BLOCK_END
#else
	#define D3D_CHECK(_expr) _expr
#endif

namespace gpu
{

template <typename T>
void SafeReleaseDX(T*& _p)
{
	IUnknown* u = static_cast<IUnknown*>(_p);
	KT_UNUSED(u);
	if (_p)
	{
		_p->Release();
		_p = nullptr;
	}
}


}