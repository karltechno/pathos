#include <string.h>

#include <kt/Macros.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3d12.h>
#include <guiddef.h>

#if KT_DEBUG
	#define D3D_CHECK(_expr) \
		KT_MACRO_BLOCK_BEGIN \
			HRESULT const KT_STRING_JOIN(hr_, __LINE__) = _expr; \
			KT_ASSERT(SUCCEEDED(KT_STRING_JOIN(hr_, __LINE__))) \
		KT_MACRO_BLOCK_END
#else
	#define D3D_CHECK(_expr) _expr
#endif

#define D3D_SET_DEBUG_NAME(_obj, _name) \
	KT_MACRO_BLOCK_BEGIN \
		wchar_t buff[256]; \
		::MultiByteToWideChar(CP_UTF8, 0, _name, -1, buff, KT_ARRAY_COUNT(buff)); \
		D3D_CHECK((_obj)->SetName(buff)); \
	KT_MACRO_BLOCK_END 

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