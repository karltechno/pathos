#pragma once
#include <gpu/Types.h>
#include <gpu/HandleRef.h>
#include <kt/Mat4.h>


namespace input
{
struct Event;
}

namespace editor
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
	void InternalRender();

	gpu::BufferRef m_idxBuf;
	gpu::BufferRef m_vtxBuf;
	gpu::TextureRef m_fontTex;
	gpu::PSORef m_pso;
};

}