#include "Renderer.h"
#include "Window.h"
#include "Camera.h"
#include "Device.h"
#include "CommandQueue.h"
#include "DescriptorHeap.h"
#include "GPUAllocator.h"
#include "CBVBuffer.h"
#include "UploadContext.h"
#include "StructuredBuffer.h"
#include "TypedBuffer.h"
#include "SwapChain.h"
#include "FrameLimiter.h"
#include "OutputTexture.h"
#include "ShaderCompiler.h"
#include "RootSignature.h"
#include "RTPipeline.h"
#include "ComputePass.h"
#include "PostProcessPass.h"
#include "ImGuiWrapper.h"
#include "NGXWrapper.h"
#include "Scene.h"
#include "StructsDX.h"
#include "CommonDX.h"
#include "Log.h"

#include <pix3.h>
#include <imgui.h>
#include <iostream>
#include <chrono>

using namespace Microsoft::WRL;

// TODO
// Rendering:
//   DLSS specular MVs 
// Materials:
//   Clearcoat
// Performance:
//   Normal packing

Renderer::Renderer(Window& window, bool debug)
	: m_window(window), m_prevCamData()
{
	m_device = std::make_unique<Device>(window.GetWidth(), window.GetHeight(), debug);
	auto device = m_device->GetDevice();
	m_commandQueue = std::make_unique<CommandQueue>(m_device->GetDevice(), "Main", D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = m_commandQueue->GetCommandList();
	m_descriptorHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 524288, true, L"CBV SRV UAV Descriptor Heap");
	m_cpuDescriptorHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 64, false, L"CPU SRV UAV Descriptor Heap (CPU)");
	m_samplerHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 256, true, L"Sampler Descriptor Heap");
	m_allocator = std::make_unique<GPUAllocator>(device, m_device->GetAdapter());
	m_uploadContext = std::make_unique<UploadContext>(*m_allocator, device);
	m_shaderCompiler = std::make_unique<ShaderCompiler>("shaders/");

	m_context = { device, m_allocator.get(), m_commandQueue.get(), m_descriptorHeap.get(), m_cpuDescriptorHeap.get(), 
				  m_samplerHeap.get(), m_uploadContext.get(), m_shaderCompiler.get() };

	m_swapChain = std::make_unique<SwapChain>(window, m_context, *m_device);
	m_frameLimiter = std::make_unique<FrameLimiter>(m_swapChain->GetRefreshRate() - 3);
	m_renderSettings.maxFPS = m_frameLimiter->GetTargetFps();
	m_imgui = std::make_unique<ImGuiWrapper>(window, m_context, m_swapChain->GetFormat(), NUM_FRAMES_IN_FLIGHT);

	m_scene = std::make_unique<Scene>(m_context);

	m_ngx = std::make_unique<NGXWrapper>(m_context, window);
	m_ngx->Initialize();

	const int width = window.GetWidth();
	const int height = window.GetHeight();
	const int renderWidth = m_ngx->GetRenderWidth();
	const int renderHeight = m_ngx->GetRenderHeight();
	m_rtOutputBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, L"RT Output Buffer");
	m_albedoBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R8G8B8A8_UNORM, renderWidth, renderHeight, L"Albedo Buffer");
	m_specularAlbedoBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R8G8B8A8_UNORM, renderWidth, renderHeight, L"Specular Albedo Buffer");
	m_normalRoughnessBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, renderWidth, renderHeight, L"Normal Roughness Buffer");
	m_depthBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R32_FLOAT, renderWidth, renderHeight, L"Depth Buffer");
	m_motionVectorsBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R32G32_FLOAT, renderWidth, renderHeight, L"Motion Vectors Buffer");
	m_dlssOutputBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, L"DLSS Output Buffer");
	m_postProcessBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width, height, L"Post Process Buffer");
	m_outputBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R10G10B10A2_UNORM, width, height, L"Output Buffer");
	m_bloomBuffers.resize(6);
	for (size_t i = 0; i < m_bloomBuffers.size(); i++)
	{
		m_bloomBuffers[i] = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R16G16B16A16_FLOAT, width >> (i + 1), height >> (i + 1), L"Bloom Buffer " + std::to_wstring(i));
	}

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, "rtOutputBuffer");		// u0:0 RT output buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, "albedoBuffer");			// u1:0 albedo buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, "specularAlbedoBuffer");	// u2:0 specular albedo buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0, "normalRoughnessBuffer"); // u3:0 normal roughness buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4, 0, "motionVectorsBuffer");	// u4:0 motion vectors buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5, 0, "depthBuffer");			// u5:0 depth buffer
	m_rootSignature->AddRootSRV(0, 0, "sceneBVH");			 // t0:0 TLAS
	m_rootSignature->AddRootSRV(1, 0, "materials");			 // t1:0 materials
	m_rootSignature->AddRootSRV(2, 0, "lights");			 // t2:0 lights
	m_rootSignature->AddRootSRV(3, 0, "powerBuffer");		 // t3:0 alias table
	m_rootSignature->AddRootSRV(4, 0, "aliasTable");		 // t4:0 alias table
	m_rootSignature->AddRootCBV(0, 0, "renderSettings");	 // b0:0 render settings
	m_rootSignature->AddRootCBV(1, 0, "renderData");		 // b1:0 render data
	m_rootSignature->AddRootCBV(2, 0, "postProcessSettings");// b2:0 post processing settings
	m_rootSignature->AddRootUAV(0, 6, "sharcHashEntries");	 // u0:6 SHaRC hash entries
	m_rootSignature->AddRootUAV(1, 6, "sharcAccumulation");	 // u1:6 SHaRC accumulation buffer
	m_rootSignature->AddRootUAV(2, 6, "sharcResolved");		 // u2:6 SHaRC resolved buffer
	m_rootSignature->AddStaticSampler(0);					 // s0:0 linear sampler
	m_rootSignature->Build(device, L"RT Root Signature");

	std::vector<std::pair<std::string, std::string>> defines =
	{ {"SHARC_UPDATE", "0"}, {"SHARC_QUERY", "1"}, { "SHARC_ENABLE_64_BIT_ATOMICS", "1" }, { "SHARC_ENABLE_GLSL", "0" } };
	std::vector<std::pair<std::string, std::string>> definesUpdate =
	{ {"SHARC_UPDATE", "1"}, {"SHARC_QUERY", "0"}, { "SHARC_ENABLE_64_BIT_ATOMICS", "1" }, { "SHARC_ENABLE_GLSL", "0" } };
	m_rtPipeline = std::make_unique<RTPipeline>(
		m_context, m_rootSignature->Get(), *m_shaderCompiler, m_scene->GetHitGroupRecords(), "shaders/raytracing.slang", defines);
	m_sharcUpdatePipeline = std::make_unique<RTPipeline>(
		m_context, m_rootSignature->Get(), *m_shaderCompiler, m_scene->GetHitGroupRecords(), "shaders/raytracing.slang", definesUpdate);

	m_sharcResolvePass = std::make_unique<ComputePass>(m_context, "shaders/sharc/sharc_resolve.slang", "SharcResolve", "SHaRC Resolve Pass");

	m_shaderCompiler->ShaderRecompileCallback([this](const Shader* shader)
	{
		if (shader == m_rtPipeline->GetShader())
		{
			m_context.commandQueue->Flush();
			m_rtPipeline->Rebuild(m_context.device, m_scene->GetHitGroupRecords());
		}
		if (shader == m_sharcUpdatePipeline->GetShader())
		{
			m_context.commandQueue->Flush();
			m_sharcUpdatePipeline->Rebuild(m_context.device, m_scene->GetHitGroupRecords());
		}
	});

	// Post-process passes
	{
		m_copyRtPass = std::make_unique<PostProcessPass>(m_context, "shaders/copy_rt.slang", "CopyRT");
		m_tonemappingPass = std::make_unique<PostProcessPass>(m_context, "shaders/tonemapping_pass.slang", "Tonemapping");
		m_autoExposurePass = std::make_unique<PostProcessPass>(m_context, "shaders/autoexposure_pass.slang", "AutoExposure");
		m_autoFocusPass = std::make_unique<PostProcessPass>(m_context, "shaders/autofocus_pass.slang", "AutoFocus");
		
		D3D12_STATIC_SAMPLER_DESC bloomSampler = BLOOM_SAMPLER;
		m_bloomPasses.push_back(std::make_unique<PostProcessPass>(m_context, "shaders/bloom_downsample.slang", "BloomDownsample", bloomSampler));
		m_bloomPasses.push_back(std::make_unique<PostProcessPass>(m_context, "shaders/bloom_upsample.slang", "BloomUpsample", bloomSampler));
		m_bloomPasses.push_back(std::make_unique<PostProcessPass>(m_context, "shaders/bloom_composite.slang", "BloomComposite", bloomSampler));
	}

	m_exposureFocusBuffer = std::make_unique<TypedBuffer>(
		m_context, 2, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Auto Exposure Buffer");
	m_exposureFocusReadback = m_allocator->CreateBuffer(
		sizeof(float) * 2, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Auto Exposure Readback");

	m_renderSettingsCB = std::make_unique<CBVBuffer<RenderSettings>>(*m_allocator, "Render Settings CB");
	m_renderDataCB = std::make_unique<CBVBuffer<RenderData>>(*m_allocator, "Render Data CB");
	m_postProcessSettingsCB = std::make_unique<CBVBuffer<PostProcessSettings>>(*m_allocator, "Post Process Settings CB");

	constexpr uint32_t SHARC_CAPACITY = 1 << 22;
	m_sharcHashEntriesBuffer = std::make_unique<StructuredBuffer>(
		m_context, SHARC_CAPACITY, sizeof(uint64_t), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "SHaRC Hash Entries", true);
	m_sharcAccumulationBuffer = std::make_unique<StructuredBuffer>(
		m_context, SHARC_CAPACITY, sizeof(uint32_t) * 4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "SHaRC Accumulation", true);
	m_sharcResolvedBuffer = std::make_unique<StructuredBuffer>(
		m_context, SHARC_CAPACITY, sizeof(uint32_t) * 4, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "SHaRC Resolved", true);

	ClearSharcBuffers(commandList.Get());

	m_commandQueue->ExecuteCommandList(commandList);
	m_commandQueue->Flush();
}

Renderer::~Renderer()
{
	m_commandQueue->Flush();
}

void Renderer::ToggleHDR()
{
	m_postProcessSettings.hdr = m_swapChain->ToggleHDR();
}

void Renderer::ToggleFullscreen()
{
	m_ngx->SetDLSSQuality(m_renderSettings.dlssQuality);
	m_swapChain->ToggleFullscreen();
	Resize(m_window.GetWidth(), m_window.GetHeight());
}

void Renderer::ToggleImGui()
{
	m_showImgui = !m_showImgui;
}

void Renderer::ToggleDenoising()
{
	m_renderSettings.denoising = !m_renderSettings.denoising;
	m_pendingResize = true;
}

void Renderer::LoadModel(const std::string& path)
{
	if (m_scene->LoadModel(path))
	{
		m_rtPipeline->RebuildShaderTables(m_device->GetDevice(), m_scene->GetHitGroupRecords());
		m_sharcUpdatePipeline->RebuildShaderTables(m_device->GetDevice(), m_scene->GetHitGroupRecords());
		ResetAccumulation();

		auto commandList = m_commandQueue->GetCommandList();
		ClearSharcBuffers(commandList.Get());
		m_commandQueue->ExecuteCommandList(commandList);
		m_commandQueue->Flush();
	}
}

void Renderer::LoadHDRI(const std::string& path)
{
	m_scene->LoadHDRI(path);
	ResetAccumulation();
}

void Renderer::Resize(const int width, const int height)
{
	if (width == 0 || height == 0)
	{
		return;
	}

	Log::Info("Window resized: {}x{}", width, height);
	auto device = m_device->GetDevice();
	ResetAccumulation();
	m_commandQueue->Flush();
	m_swapChain->Resize(m_context, width, height);

	glm::ivec2 renderSize = m_renderSettings.denoising ? m_ngx->Resize() : glm::ivec2(width, height);
	m_dlssOutputBuffer->Resize(device, width, height);
	m_postProcessBuffer->Resize(device, width, height);
	m_outputBuffer->Resize(device, width, height);
	m_rtOutputBuffer->Resize(device, renderSize.x, renderSize.y);
	m_albedoBuffer->Resize(device, renderSize.x, renderSize.y);
	m_specularAlbedoBuffer->Resize(device, renderSize.x, renderSize.y);
	m_normalRoughnessBuffer->Resize(device, renderSize.x, renderSize.y);
	m_depthBuffer->Resize(device, renderSize.x, renderSize.y);
	m_motionVectorsBuffer->Resize(device, renderSize.x, renderSize.y);
	for (size_t i = 0; i < m_bloomBuffers.size(); i++)
	{
		m_bloomBuffers[i]->Resize(device, width >> (i + 1), height >> (i + 1));
	}
}

void Renderer::ClearSharcBuffers(ID3D12GraphicsCommandList* commandList) const
{
	ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap() };
	commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	constexpr UINT clearValue[4] = { 0, 0, 0, 0 };
	commandList->ClearUnorderedAccessViewUint(
		m_sharcHashEntriesBuffer->GetClearUAVGPU().gpuHandle, m_sharcHashEntriesBuffer->GetClearUAV().cpuHandle,
		m_sharcHashEntriesBuffer->GetResource(), clearValue, 0, nullptr);
	commandList->ClearUnorderedAccessViewUint(
		m_sharcAccumulationBuffer->GetClearUAVGPU().gpuHandle, m_sharcAccumulationBuffer->GetClearUAV().cpuHandle,
		m_sharcAccumulationBuffer->GetResource(), clearValue, 0, nullptr);
	commandList->ClearUnorderedAccessViewUint(
		m_sharcResolvedBuffer->GetClearUAVGPU().gpuHandle, m_sharcResolvedBuffer->GetClearUAV().cpuHandle,
		m_sharcResolvedBuffer->GetResource(), clearValue, 0, nullptr);
}

void Renderer::Render(const float deltaTime)
{
	if (m_pendingResize)
	{
		m_ngx->SetDLSSQuality(m_renderSettings.dlssQuality);
		Resize(m_window.GetWidth(), m_window.GetHeight());
		m_pendingResize = false;
	}
	if (m_resetAccumulation)
	{
		if (!m_renderSettings.denoising)
		{
			m_renderData.frame = 0;
		}
		m_resetAccumulation = false;
	}

	auto backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	auto backBuffer = m_swapChain->GetCurrentBackBuffer();
	auto commandList = m_commandQueue->GetCommandList();
	auto commandQueue = m_commandQueue->GetQueue();
	glm::ivec2 windowSize = glm::ivec2(m_window.GetWidth(), m_window.GetHeight());
	glm::ivec2 renderSize = m_renderSettings.denoising ? glm::ivec2(m_ngx->GetRenderWidth(), m_ngx->GetRenderHeight()) : windowSize;

	if (m_postProcessSettings.autoExposure || m_camera->m_autoFocus)
	{
		float* mapped = nullptr;
		D3D12_RANGE readRange = { 0, sizeof(float) * 2 };
		m_exposureFocusReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
		if (m_postProcessSettings.autoExposure) m_postProcessSettings.exposure = mapped[0];
		if (m_camera->m_autoFocus) m_camera->m_focusDistance = mapped[1];
		D3D12_RANGE writeRange = { 0, 0 };
		m_exposureFocusReadback.resource->Unmap(0, &writeRange);
	}

	if (m_reloadTimer >= 0.5f)
	{
		m_reloadTimer = 0.0f;
		if (m_shaderCompiler->CheckHotReload())
		{
			ResetAccumulation();
		}
	}
	else
	{
		m_reloadTimer += deltaTime;
	}

	CameraData camData{};
	camData.forward = m_camera->GetForward();
	camData.right = m_camera->GetRight();
	camData.up = m_camera->GetUp();
	camData.position = m_camera->GetPosition();
	camData.fov = m_camera->m_fov * m_camera->m_fovMultiplier;
	camData.aperture = m_camera->m_aperture;
	camData.focusDistance = m_camera->m_focusDistance;
	camData.autoFocus = m_camera->m_autoFocus;
	camData.squeezeFactor = m_camera->m_squeezeFactor;
	camData.swirliness = m_camera->m_swirliness;

	// Begin frame
	{
		m_imgui->BeginFrame();
		m_postProcessBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	m_renderData.prevCamera = m_prevCamData;
	m_renderData.camera = camData;
	m_renderData.hdriIndex = m_scene->GetHDRIDescriptorIndex();
	m_renderData.numLights = m_scene->GetNumLights();
	m_renderData.totalPower = m_scene->GetTotalLightPower();
	m_renderData.deltaTime = deltaTime;
	m_renderData.hdrEnabled = m_swapChain->IsHDR();
	glm::vec2 jitter = m_ngx->GetJitter(static_cast<int>(m_renderData.frame));
	m_renderData.camera.jitterX = jitter.x;
	m_renderData.camera.jitterY = jitter.y;
	m_renderSettingsCB->Update(backBufferIndex, m_renderSettings);
	m_renderDataCB->Update(backBufferIndex, m_renderData);
	m_postProcessSettings.hdr = m_swapChain->IsHDR();
	m_postProcessSettingsCB->Update(backBufferIndex, m_postProcessSettings);

	// Record commands
	{
		// SHaRC update pass
		{
			PIXScopedEvent(commandList.Get(), 0x1f77b4, "SHaRC Update");

			ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap(), m_samplerHeap->GetHeap() };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			commandList->SetComputeRootSignature(m_rootSignature->Get());
			commandList->SetPipelineState1(m_rtPipeline->GetPSO());

			if (m_renderSettings.sharc)
			{
				commandList->SetPipelineState1(m_sharcUpdatePipeline->GetPSO());

				m_rootSignature->SetDescriptorTable(commandList.Get(), m_rtOutputBuffer->GetUAV().gpuHandle, "rtOutputBuffer");
				m_rootSignature->SetDescriptorTable(commandList.Get(), m_albedoBuffer->GetUAV().gpuHandle, "albedoBuffer");
				m_rootSignature->SetDescriptorTable(commandList.Get(), m_specularAlbedoBuffer->GetUAV().gpuHandle, "specularAlbedoBuffer");
				m_rootSignature->SetDescriptorTable(commandList.Get(), m_normalRoughnessBuffer->GetUAV().gpuHandle, "normalRoughnessBuffer");
				m_rootSignature->SetDescriptorTable(commandList.Get(), m_motionVectorsBuffer->GetUAV().gpuHandle, "motionVectorsBuffer");
				m_rootSignature->SetDescriptorTable(commandList.Get(), m_depthBuffer->GetUAV().gpuHandle, "depthBuffer");

				m_rootSignature->SetRootCBV(commandList.Get(), m_renderSettingsCB->GetGPUAddress(backBufferIndex), "renderSettings");
				m_rootSignature->SetRootCBV(commandList.Get(), m_renderDataCB->GetGPUAddress(backBufferIndex), "renderData");
				m_rootSignature->SetRootCBV(commandList.Get(), m_postProcessSettingsCB->GetGPUAddress(backBufferIndex), "postProcessSettings");

				m_rootSignature->SetRootUAV(commandList.Get(), m_sharcHashEntriesBuffer->GetResource()->GetGPUVirtualAddress(), "sharcHashEntries");
				m_rootSignature->SetRootUAV(commandList.Get(), m_sharcAccumulationBuffer->GetResource()->GetGPUVirtualAddress(), "sharcAccumulation");
				m_rootSignature->SetRootUAV(commandList.Get(), m_sharcResolvedBuffer->GetResource()->GetGPUVirtualAddress(), "sharcResolved");

				m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetTLASAddress(), "sceneBVH");
				m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetMaterialsBufferAddress(), "materials");
				m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetLightBufferAddress(), "lights");
				m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetPowerBufferAddress(), "powerBuffer");
				m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetLightAliasTableBufferAddress(), "aliasTable");

				auto updateDesc = m_sharcUpdatePipeline->GetDispatchRaysDesc();
				updateDesc.Width = windowSize.x;
				updateDesc.Height = windowSize.y;
				commandList->DispatchRays(&updateDesc);

				D3D12_RESOURCE_BARRIER sharcBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(m_sharcHashEntriesBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_sharcAccumulationBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_sharcResolvedBuffer->GetResource()) };
				commandList->ResourceBarrier(_countof(sharcBarriers), sharcBarriers);
			}
		}

		// SHaRC resolve pass
		{
			PIXScopedEvent(commandList.Get(), 0x1f77b4, "SHaRC Resolve");

			if (m_renderSettings.sharc)
			{
				ComputePass::ComputeBindings bindings;
				bindings.uavs[0] = m_sharcHashEntriesBuffer->GetResource()->GetGPUVirtualAddress();
				bindings.uavs[1] = m_sharcAccumulationBuffer->GetResource()->GetGPUVirtualAddress();
				bindings.uavs[2] = m_sharcResolvedBuffer->GetResource()->GetGPUVirtualAddress();
				bindings.uavCount = 3;
				bindings.cbvs[0] = m_renderDataCB->GetGPUAddress(backBufferIndex);
				bindings.cbvCount = 1;
				bindings.threads = glm::uvec3(1 << 22, 1, 1);
				bindings.threadGroupSize = glm::uvec3(256, 1, 1);
				m_sharcResolvePass->Dispatch(commandList.Get(), bindings);
				commandList->SetComputeRootSignature(m_rootSignature->Get());

				D3D12_RESOURCE_BARRIER sharcBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(m_sharcHashEntriesBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_sharcAccumulationBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_sharcResolvedBuffer->GetResource()) };
				commandList->ResourceBarrier(_countof(sharcBarriers), sharcBarriers);
			}
		}

		// Raytracing pass
		{
			PIXScopedEvent(commandList.Get(), 0xeb4034, "Raytracing");

			ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap(), m_samplerHeap->GetHeap() };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			commandList->SetComputeRootSignature(m_rootSignature->Get());
			commandList->SetPipelineState1(m_rtPipeline->GetPSO());

			m_rootSignature->SetDescriptorTable(commandList.Get(), m_rtOutputBuffer->GetUAV().gpuHandle,		"rtOutputBuffer");
			m_rootSignature->SetDescriptorTable(commandList.Get(), m_albedoBuffer->GetUAV().gpuHandle,			"albedoBuffer");
			m_rootSignature->SetDescriptorTable(commandList.Get(), m_specularAlbedoBuffer->GetUAV().gpuHandle,	"specularAlbedoBuffer");
			m_rootSignature->SetDescriptorTable(commandList.Get(), m_normalRoughnessBuffer->GetUAV().gpuHandle, "normalRoughnessBuffer");
			m_rootSignature->SetDescriptorTable(commandList.Get(), m_motionVectorsBuffer->GetUAV().gpuHandle,	"motionVectorsBuffer");
			m_rootSignature->SetDescriptorTable(commandList.Get(), m_depthBuffer->GetUAV().gpuHandle,			"depthBuffer");

			m_rootSignature->SetRootCBV(commandList.Get(), m_renderSettingsCB->GetGPUAddress(backBufferIndex),		"renderSettings");
			m_rootSignature->SetRootCBV(commandList.Get(), m_renderDataCB->GetGPUAddress(backBufferIndex),			"renderData");
			m_rootSignature->SetRootCBV(commandList.Get(), m_postProcessSettingsCB->GetGPUAddress(backBufferIndex), "postProcessSettings");

			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetTLASAddress(),							"sceneBVH");
			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetMaterialsBufferAddress(),				"materials");
			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetLightBufferAddress(),					"lights");
			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetPowerBufferAddress(),					"powerBuffer");
			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetLightAliasTableBufferAddress(),			"aliasTable");

			m_rootSignature->SetRootUAV(commandList.Get(), m_sharcHashEntriesBuffer->GetResource()->GetGPUVirtualAddress(),	"sharcHashEntries");
			m_rootSignature->SetRootUAV(commandList.Get(), m_sharcAccumulationBuffer->GetResource()->GetGPUVirtualAddress(), "sharcAccumulation");
			m_rootSignature->SetRootUAV(commandList.Get(), m_sharcResolvedBuffer->GetResource()->GetGPUVirtualAddress(),		"sharcResolved");

			auto dispatchDesc = m_rtPipeline->GetDispatchRaysDesc();
			dispatchDesc.Width = m_renderSettings.denoising ? renderSize.x : windowSize.x;
			dispatchDesc.Height = m_renderSettings.denoising ? renderSize.y : windowSize.y;
			commandList->DispatchRays(&dispatchDesc);

			D3D12_RESOURCE_BARRIER uavBarriers[] = {
				CD3DX12_RESOURCE_BARRIER::UAV(m_rtOutputBuffer->GetResource())
			};
			commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);
		}

		// DLSS pass
		{
			PIXScopedEvent(commandList.Get(), 0x76b900, "DLSS");

			if (m_ngx->IsDLSSSupported() && m_renderSettings.denoising)
			{
				D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(m_depthBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_motionVectorsBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_albedoBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_specularAlbedoBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_normalRoughnessBuffer->GetResource()),
				};
				commandList->ResourceBarrier(_countof(uavBarriers), uavBarriers);

				NGXWrapper::DLSSInputs dlssInputs{};
				dlssInputs.albedo = m_albedoBuffer->GetResource();
				dlssInputs.specularAlbedo = m_specularAlbedoBuffer->GetResource();
				dlssInputs.normalRoughness = m_normalRoughnessBuffer->GetResource();
				dlssInputs.depth = m_depthBuffer->GetResource();
				dlssInputs.motionVectors = m_motionVectorsBuffer->GetResource();
				dlssInputs.input = m_rtOutputBuffer->GetResource();
				dlssInputs.output = m_dlssOutputBuffer->GetResource();
				dlssInputs.jitterX = m_renderData.camera.jitterX;
				dlssInputs.jitterY = m_renderData.camera.jitterY;

				m_ngx->EvaluateDLSS(commandList.Get(), dlssInputs);

				ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap(), m_samplerHeap->GetHeap() };
				commandList->SetDescriptorHeaps(_countof(heaps), heaps);
			}
		}

		OutputBuffer* currentBuffer = m_renderSettings.denoising ? m_dlssOutputBuffer.get() : m_rtOutputBuffer.get();

		// Copy to post process buffer
		{
			PIXScopedEvent(commandList.Get(), 0x007acc, "Copy to Post Process Buffer");

			currentBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_postProcessBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			PostProcessPass::PostProcessBindings bindings;
			bindings.inputSrv = currentBuffer->GetSRV().gpuHandle;
			bindings.outputUav = m_postProcessBuffer->GetUAV().gpuHandle;
			bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
			bindings.cbvCount = 2;
			bindings.rootConstants[0] = currentBuffer->GetWidth();
			bindings.rootConstants[1] = currentBuffer->GetHeight();
			bindings.rootConstantCount = 2;
			bindings.width = currentBuffer->GetWidth();
			bindings.height = currentBuffer->GetHeight();

			m_copyRtPass->Dispatch(commandList.Get(), bindings);

			currentBuffer = m_postProcessBuffer.get();
		}

		// Bloom passes
		if (m_postProcessSettings.bloomStrength > 0.0f)
		{
			PIXScopedEvent(commandList.Get(), 0xffdc7d, "Bloom Passes");

			OutputBuffer* bloomInput = currentBuffer;

			// Downsample passes
			{
				PIXScopedEvent(commandList.Get(), 0xffdc7d, "Bloom Downsample");

				for (size_t i = 0; i < m_bloomBuffers.size(); i++) // [0:numMips-1]
				{
					OutputBuffer* bloomOutput = m_bloomBuffers[i].get();
					bloomInput->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					bloomOutput->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					PostProcessPass::PostProcessBindings bindings;
					bindings.inputSrv = bloomInput->GetSRV().gpuHandle;
					bindings.outputUav = bloomOutput->GetUAV().gpuHandle;
					bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
					bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
					bindings.cbvs[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
					bindings.cbvCount = 3;
					bindings.rootConstants[0] = bloomInput->GetWidth();
					bindings.rootConstants[1] = bloomInput->GetHeight();
					bindings.rootConstants[2] = bloomOutput->GetWidth();
					bindings.rootConstants[3] = bloomOutput->GetHeight();
					bindings.rootConstants[4] = static_cast<uint32_t>(i); // mip level
					bindings.rootConstants[5] = static_cast<uint32_t>(m_bloomBuffers.size()); // total mip levels
					bindings.rootConstantCount = 6;
					bindings.width = bloomOutput->GetWidth();
					bindings.height = bloomOutput->GetHeight();

					m_bloomPasses[0]->Dispatch(commandList.Get(), bindings);

					bloomInput = m_bloomBuffers[i].get();

					for (size_t i = 0; i < m_bloomBuffers.size(); i++)
					{
						D3D12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(m_bloomBuffers[i].get()->GetResource());
						commandList->ResourceBarrier(1, &uavBarrier);
					}
				}
			}

			// Upsample passes
			{
				PIXScopedEvent(commandList.Get(), 0xffdc7d, "Bloom Upsample");

				for (size_t i = m_bloomBuffers.size(); i-- > 1;) // [numMips-1:1]
				{
					bloomInput = m_bloomBuffers[i].get();
					OutputBuffer* bloomOutput = m_bloomBuffers[i - 1].get();
					bloomInput->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
					bloomOutput->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

					PostProcessPass::PostProcessBindings bindings;
					bindings.inputSrv = bloomInput->GetSRV().gpuHandle;
					bindings.outputUav = bloomOutput->GetUAV().gpuHandle;
					bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
					bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
					bindings.cbvs[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
					bindings.cbvCount = 3;
					bindings.rootConstants[0] = bloomInput->GetWidth();
					bindings.rootConstants[1] = bloomInput->GetHeight();
					bindings.rootConstants[2] = bloomOutput->GetWidth();
					bindings.rootConstants[3] = bloomOutput->GetHeight();
					bindings.rootConstants[4] = static_cast<uint32_t>(i); // mip level
					bindings.rootConstants[5] = static_cast<uint32_t>(m_bloomBuffers.size()); // total mip levels
					bindings.rootConstantCount = 6;
					bindings.width = bloomOutput->GetWidth();
					bindings.height = bloomOutput->GetHeight();

					m_bloomPasses[1]->Dispatch(commandList.Get(), bindings);
				}
			}

			// Final upsample to output buffer
			{
				PIXScopedEvent(commandList.Get(), 0xffdc7d, "Bloom Combine");

				bloomInput = m_bloomBuffers[0].get();
				bloomInput->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
				currentBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				PostProcessPass::PostProcessBindings bindings;
				bindings.inputSrv = bloomInput->GetSRV().gpuHandle;
				bindings.outputUav = currentBuffer->GetUAV().gpuHandle;
				bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
				bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
				bindings.cbvs[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
				bindings.cbvCount = 3;
				bindings.rootConstants[0] = bloomInput->GetWidth();
				bindings.rootConstants[1] = bloomInput->GetHeight();
				bindings.rootConstants[2] = currentBuffer->GetWidth();
				bindings.rootConstants[3] = currentBuffer->GetHeight();
				bindings.rootConstants[4] = 0;
				bindings.rootConstants[5] = static_cast<uint32_t>(m_bloomBuffers.size()); // total mip levels
				bindings.rootConstantCount = 6;
				bindings.width = currentBuffer->GetWidth();
				bindings.height = currentBuffer->GetHeight();

				m_bloomPasses[2]->Dispatch(commandList.Get(), bindings);
			}
		}

		// Auto exposure pass
		if (m_postProcessSettings.autoExposure)
		{
			PIXScopedEvent(commandList.Get(), 0xac6bfa, "Auto Exposure");

			currentBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			PostProcessPass::PostProcessBindings bindings;
			bindings.inputSrv = currentBuffer->GetSRV().gpuHandle;
			bindings.outputUav = m_exposureFocusBuffer->GetUAV().gpuHandle;
			bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvCount = 3;
			bindings.width = 1;
			bindings.height = 1;

			m_autoExposurePass->Dispatch(commandList.Get(), bindings);

			m_exposureFocusBuffer->GetBuffer().Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);

			commandList->CopyResource(m_exposureFocusReadback.resource, m_exposureFocusBuffer->GetResource());

			m_exposureFocusBuffer->GetBuffer().Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		// Auto focus pass
		if (m_camera->m_autoFocus)
		{
			PIXScopedEvent(commandList.Get(), 0xac6bfa, "Auto Focus");

			m_depthBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

			PostProcessPass::PostProcessBindings bindings;
			bindings.inputSrv = m_depthBuffer->GetSRV().gpuHandle;
			bindings.outputUav = m_exposureFocusBuffer->GetUAV().gpuHandle;
			bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvCount = 3;
			bindings.width = 1;
			bindings.height = 1;

			m_autoFocusPass->Dispatch(commandList.Get(), bindings);

			m_exposureFocusBuffer->GetBuffer().Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);

			commandList->CopyResource(m_exposureFocusReadback.resource, m_exposureFocusBuffer->GetResource());

			m_exposureFocusBuffer->GetBuffer().Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		// Tonemapping pass
		{
			PIXScopedEvent(commandList.Get(), 0x6bfa8c, "Tonemapping");

			currentBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_outputBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			PostProcessPass::PostProcessBindings bindings;
			bindings.inputSrv = currentBuffer->GetSRV().gpuHandle;
			bindings.outputUav = m_outputBuffer->GetUAV().gpuHandle;
			bindings.cbvs[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
			bindings.cbvs[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.cbvCount = 3;
			bindings.width = static_cast<uint32_t>(m_swapChain->GetViewport().Width);
			bindings.height = static_cast<uint32_t>(m_swapChain->GetViewport().Height);

			m_tonemappingPass->Dispatch(commandList.Get(), bindings);

			currentBuffer = m_outputBuffer.get();
		}
	}

	// ImGui window
	if (m_showImgui)
	{
		if (!m_shaderCompiler->GetReloadError().empty())
		{
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 0.0f, 0.0f, 0.3f));
			ImGui::Begin("Border", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
			ImGui::SetWindowPos(ImVec2(0, 0));
			ImGui::SetWindowSize(ImGui::GetIO().DisplaySize);
			ImGui::End();
			ImGui::PopStyleColor();

			ImGui::SetNextWindowPos(ImVec2(windowSize.x / 2.0f - 400.0f, windowSize.y / 2.0f - 200.0f));
			ImGui::Begin("Shader Compile Errors", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
			ImGui::PushTextWrapPos(800.0f);
			ImGui::TextWrapped("%s", m_shaderCompiler->GetReloadError().c_str());
			ImGui::PopTextWrapPos();
			ImGui::End();
		}

		auto config = ImSettings();
		config.push<float>().as_drag().min(0).max(10).speed(0.02f).pop();
		auto config2 = ImSettings();
		config2.push<float>().as_drag().min(0).max(100).speed(0.02f).pop();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::Begin("Settings");
		ImGui::BeginTabBar("SettingsTabBar");
		ImGui::PushItemWidth(250.0f);
		if (ImGui::BeginTabItem("Main"))
		{
			ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, deltaTime * 1000.0f);
			ImGui::Text("Frame: %u", m_renderData.frame);
			ImGui::Text("Resolution: %ux%u", windowSize.x, windowSize.y);
			ImGui::Text("Render Resolution: %ux%u", renderSize.x, renderSize.y);

			if (ImGui::Button("Toggle"))
			{
				m_swapChain->ToggleHDR();
			}
			ImGui::SameLine();
			ImGui::Text(m_swapChain->IsHDR() ? "HDR: On" : "HDR: Off");

			auto responseRender = ImReflect::Input("Render Settings", m_renderSettings, config);
			auto responsePost = ImReflect::Input("Post Process Settings", m_postProcessSettings, config);
			if (responseRender.get<RenderSettings>().is_changed()) { ResetAccumulation(); }
			if (responseRender.get_member<&RenderSettings::maxFPS>().is_changed())
			{
				m_frameLimiter->SetTargetFps(m_renderSettings.maxFPS);
			}
			if (responseRender.get_member<&RenderSettings::denoising>().is_changed())
			{
				m_pendingResize = true;
			}
			if (responseRender.get_member<&RenderSettings::dlssQuality>().is_changed())
			{
				m_pendingResize = true;
			}
			if (m_postProcessSettings.hdr)
			{
				const char* enumNames[] = { "Linear", "Custom", "Psycho" };
				static int currentIndex = static_cast<int>(m_postProcessSettings.tonemapperHDR);
				if (ImGui::Combo("Tonemapper", &currentIndex, enumNames, IM_ARRAYSIZE(enumNames))) {
					m_postProcessSettings.tonemapperHDR = static_cast<TonemapOperatorHDR>(currentIndex);
				}
			}
			else
			{
				const char* enumNames[] = { "Linear", "ACES", "Reinhard", "AgX", "GT7" };
				static int currentIndex = static_cast<int>(m_postProcessSettings.tonemapperSDR);
				if (ImGui::Combo("Tonemapper", &currentIndex, enumNames, IM_ARRAYSIZE(enumNames))) {
					m_postProcessSettings.tonemapperSDR = static_cast<TonemapOperatorSDR>(currentIndex);
				}
			}
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Camera"))
		{
			CameraData uiCamData = camData;
			uiCamData.fov = m_camera->m_fov;

			auto responseCamera = ImReflect::Input("Camera", uiCamData, config2);
			if (responseCamera.get<CameraData>().is_changed())
			{
				ResetAccumulation();
				m_camera->SetPosition(uiCamData.position);
				m_camera->SetDirection(uiCamData.forward);
				m_camera->m_fov = uiCamData.fov;
				m_camera->m_aperture = uiCamData.aperture;
				m_camera->m_focusDistance = uiCamData.focusDistance;
				m_camera->m_autoFocus = uiCamData.autoFocus;
				m_camera->m_squeezeFactor = uiCamData.squeezeFactor;
				m_camera->m_swirliness = uiCamData.swirliness;
			}
			ImGui::EndTabItem();
		}
		ImGui::PopItemWidth();

		ImGui::EndTabBar();
		ImGui::End();
	}

	// End frame
	{
		PIXSetMarker(0xffffff, "Present");

		m_outputBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->CopyResource(backBuffer, m_outputBuffer->GetResource());

		// ImGui
		{
			PIXScopedEvent(commandList.Get(), 0xffffff, "ImGui");
			m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);

			D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_swapChain->GetCurrentBackBufferRTV().cpuHandle;
			commandList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);

			ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap() };
			commandList->SetDescriptorHeaps(1, heaps);

			m_imgui->EndFrame(commandList.Get());
		}

		m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_PRESENT);

		m_fenceValues[backBufferIndex] = m_commandQueue->ExecuteCommandList(commandList);
		m_swapChain->Present();
		m_frameLimiter->Wait();
		m_commandQueue->WaitForFenceValue(m_fenceValues[m_swapChain->GetCurrentBackBufferIndex()]);
		
		if (camData.position != m_prevCamData.position || camData.forward != m_prevCamData.forward)
		{
			ResetAccumulation();
		}
		m_renderData.frame++;
		m_prevCamData = camData;
	}
}