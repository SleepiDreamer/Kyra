#pragma once
#include "DescriptorHeap.h"
#include "CommonDX.h"

#include <d3d12.h>

class Window;
class DescriptorHeap;

class ImGuiWrapper
{
public:
    ImGuiWrapper(const Window& window, RenderContext& context, DXGI_FORMAT rtvFormat, uint32_t framesInFlight);
    ~ImGuiWrapper();
    ImGuiWrapper(const ImGuiWrapper&) = delete;
    ImGuiWrapper& operator=(const ImGuiWrapper&) = delete;

    void BeginFrame();
    void EndFrame(ID3D12GraphicsCommandList* cmdList);

private:
	RenderContext& m_context;
    Descriptor m_fontDescriptor;
};