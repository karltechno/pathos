#pragma once
#include <gpu/Types.h>
#include <gpu/HandleRef.h>
#include <kt/Mat4.h>


namespace input
{
struct Event;
}

namespace app
{

class ImGuiHandler
{
public:
	void Init(void* _nwh);
	void Shutdown();

	bool HandleInputEvent(input::Event const& _event);

	void BeginFrame(float _dt);
	void EndFrame();

private:
	// TODO: Fix constant buffer size in d3d and remove align hack
	struct KT_ALIGNAS(16) ImGuiCBuffer
	{
		kt::Mat4 m_orthoMtx;
	};

	void InternalRender();

	gpu::BufferRef m_idxBuf;
	gpu::BufferRef m_vtxBuf;
	gpu::BufferRef m_cbuf;
	gpu::TextureRef m_fontTex;
	gpu::PSORef m_pso;
};

}