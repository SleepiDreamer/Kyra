#pragma once
#include "DescriptorHeap.h"
#include "CommonDX.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

class Window;
class CommandQueue;

class SwapChain
{
public:
	SwapChain(Window& window, RenderContext& context, IDXGIAdapter4* adapter);
	~SwapChain();

	[[nodiscard]] static bool CheckTearingSupport();
	[[nodiscard]] static bool CheckHDRSupport(IDXGIAdapter4* adapter);
	[[nodiscard]] UINT GetCurrentBackBufferIndex() const { return m_currentBackBufferIndex; }
	[[nodiscard]] ID3D12Resource* GetCurrentBackBuffer() const { return m_backBuffers[m_currentBackBufferIndex].Get(); }
	[[nodiscard]] Descriptor GetCurrentBackBufferRTV() const { return m_backBufferRtvs[m_currentBackBufferIndex]; }
	[[nodiscard]] Descriptor GetCurrentBackBufferSRV() const { return m_backBufferSrvs[m_currentBackBufferIndex]; }
	[[nodiscard]] D3D12_VIEWPORT GetViewport() const { return m_viewport; }
	[[nodiscard]] D3D12_RECT GetScissorRect() const { return m_scissorRect; }
	[[nodiscard]] DXGI_FORMAT GetFormat() const { return m_format; }

	void ToggleFullscreen();
	void CreateBackBuffers(const RenderContext& context);
	void Resize(const RenderContext& context, uint32_t width, uint32_t height);
	void Present();
	void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState);

private:
	Window& m_window;
	std::unique_ptr<DescriptorHeap> m_rtvHeap = nullptr;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_swapChain = nullptr;
	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_backBuffers[NUM_FRAMES_IN_FLIGHT];
	Descriptor m_backBufferRtvs[NUM_FRAMES_IN_FLIGHT];
	Descriptor m_backBufferSrvs[NUM_FRAMES_IN_FLIGHT];
	UINT m_currentBackBufferIndex = 0;
	DXGI_FORMAT m_format = DXGI_FORMAT_R10G10B10A2_UNORM;
	D3D12_RESOURCE_STATES m_currentState = D3D12_RESOURCE_STATE_COMMON;

	bool m_useAdaptiveSync = false;
	bool m_useVsync = false;
	bool m_useHdr = false;

	bool m_fullscreen = false;
	int m_windowedX = 0;
	int m_windowedY = 0;
	int m_windowedWidth = 0;
	int m_windowedHeight = 0;
};
