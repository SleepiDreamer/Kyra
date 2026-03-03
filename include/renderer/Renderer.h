#pragma once
#include "DescriptorHeap.h"
#include "GPUBuffer.h"
#include "StructsDX.h"
#include "CommonDX.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <memory>

class Window;
class Camera;
class Device;
class CommandQueue;
class SwapChain;
class DescriptorHeap;
class CommandList;
class UploadContext;
class GPUAllocator;
class OutputBuffer;
class ShaderCompiler;
class RootSignature;
class RTPipeline;
class PostProcessPass;
class ImGuiWrapper;
class NGXWrapper;
class Scene;
template<typename T>
class CBVBuffer;

class Renderer
{
public:
	explicit Renderer(Window& window, bool debug);
	~Renderer();
	void Render(float deltaTime);

	void SetCamera(std::shared_ptr<Camera> camera) { m_camera = std::move(camera); }
	void ToggleFullscreen();
	void LoadModel(const std::string& path);
	void LoadHDRI(const std::string& path);
	void Resize(int width, int height);
	void ResetAccumulation() { if (!m_renderSettings.denoising) m_renderData.frame = 0; }

private:
	Window& m_window;
	std::unique_ptr<Device> m_device = nullptr;
	std::shared_ptr<Camera> m_camera = nullptr;
	CameraData m_prevCamData;

	RenderContext m_context;
	std::unique_ptr<GPUAllocator> m_allocator = nullptr;
	std::unique_ptr<CommandQueue> m_commandQueue = nullptr;
	std::unique_ptr<DescriptorHeap> m_descriptorHeap = nullptr;
	std::unique_ptr<UploadContext> m_uploadContext = nullptr;

	uint64_t m_fenceValues[NUM_FRAMES_IN_FLIGHT] = {};
	std::unique_ptr<SwapChain> m_swapChain = nullptr;
	std::unique_ptr<ShaderCompiler> m_shaderCompiler;
	std::unique_ptr<RootSignature> m_rootSignature;
	std::unique_ptr<RTPipeline>	m_rtPipeline;
	std::unique_ptr<ImGuiWrapper> m_imgui = nullptr;
	std::unique_ptr<NGXWrapper> m_ngx = nullptr;
	bool m_pendingResize = false;

	std::unique_ptr<Scene> m_scene;
	RenderSettings m_renderSettings{};
	RenderData m_renderData{};
	PostProcessSettings m_postProcessSettings{};
	std::unique_ptr<CBVBuffer<RenderSettings>> m_renderSettingsCB;
	std::unique_ptr<CBVBuffer<RenderData>> m_renderDataCB;
	std::unique_ptr<CBVBuffer<PostProcessSettings>> m_postProcessSettingsCB;
	float m_reloadTimer = 0.0f;

	std::unique_ptr<OutputBuffer> m_rtOutputBuffer;
	std::unique_ptr<OutputBuffer> m_albedoBuffer;
	std::unique_ptr<OutputBuffer> m_specularAlbedoBuffer;
	std::unique_ptr<OutputBuffer> m_normalRoughnessBuffer;
	std::unique_ptr<OutputBuffer> m_motionVectorsBuffer;
	std::unique_ptr<OutputBuffer> m_depthBuffer;
	std::unique_ptr<OutputBuffer> m_dlssOutputBuffer;
	std::unique_ptr<OutputBuffer> m_outputBuffer;

	std::unique_ptr<PostProcessPass> m_tonemappingPass;
	std::unique_ptr<PostProcessPass> m_autoExposurePass;
	DescriptorHeap::Allocation m_autoExposureBufferUav;
	GPUBuffer m_autoExposureReadback;
	GPUBuffer m_autoExposureBuffer;

	GPUBuffer m_sharcHashEntriesBuffer;
	DescriptorHeap::Allocation m_sharcHashEntriesBufferUav;
	GPUBuffer m_sharcAccumulationBuffer;
	DescriptorHeap::Allocation m_sharcAccumulationBufferUav;
	GPUBuffer m_sharcResolvedBuffer;
	DescriptorHeap::Allocation m_sharcResolvedBufferUav;
};

