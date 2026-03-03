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
#include "OutputTexture.h"
#include "ShaderCompiler.h"
#include "RootSignature.h"
#include "RTPipeline.h"
#include "PostProcessPass.h"
#include "ImGuiWrapper.h"
#include "NGXWrapper.h"
#include "Scene.h"
#include "StructsDX.h"
#include "CommonDX.h"

#include <imgui.h>
#include <iostream>
#include <chrono>

using namespace Microsoft::WRL;

// TODO
// Rendering:
//   auto focus
//   explicit lights
//   Importance sampling
//   Spherical caps VNDF
//   DLSS specular MVs 
//   SHaRC
// Materials:
//   Clearcoat
// Tonemapping:
//   HDR tonemapping
// Performance:
//   Normal packing

Renderer::Renderer(Window& window, bool debug)
	: m_window(window), m_prevCamData()
{
	m_device = std::make_unique<Device>(window.GetWidth(), window.GetHeight(), debug);
	auto device = m_device->GetDevice();
	m_commandQueue = std::make_unique<CommandQueue>(m_device->GetDevice(), "Main", D3D12_COMMAND_LIST_TYPE_DIRECT);
	auto commandList = m_commandQueue->GetCommandList();
	m_descriptorHeap = std::make_unique<DescriptorHeap>(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, true, L"CBV SRV UAV Descriptor Heap");
	m_allocator = std::make_unique<GPUAllocator>(device, m_device->GetAdapter());
	m_uploadContext = std::make_unique<UploadContext>(*m_allocator, device);

	m_context = { device, m_allocator.get(), m_commandQueue.get(), m_descriptorHeap.get(), m_uploadContext.get() };

	m_swapChain = std::make_unique<SwapChain>(window, m_context, m_device->GetAdapter());
	m_imgui = std::make_unique<ImGuiWrapper>(window, m_context, m_swapChain->GetFormat(), NUM_FRAMES_IN_FLIGHT);

	m_scene = std::make_unique<Scene>(m_context);

	m_shaderCompiler = std::make_unique<ShaderCompiler>();

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
	m_outputBuffer = std::make_unique<OutputBuffer>(m_context, DXGI_FORMAT_R10G10B10A2_UNORM, width, height, L"Output Buffer");

	m_rootSignature = std::make_unique<RootSignature>();
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, "rtOutputBuffer");		// u0:0 RT output buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, "albedoBuffer");			// u1:0 albedo buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, "specularAlbedoBuffer");	// u2:0 specular albedo buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0, "normalRoughnessBuffer"); // u3:0 normal roughness buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 4, 0, "motionVectorsBuffer");	// u4:0 motion vectors buffer
	m_rootSignature->AddDescriptorTable(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 5, 0, "depthBuffer");			// u5:0 depth buffer
	m_rootSignature->AddRootSRV(0, 0, "sceneBVH");			 // t0:0 TLAS
	m_rootSignature->AddRootSRV(1, 0, "materials");			 // t1:0 materials
	m_rootSignature->AddRootCBV(0, 0, "renderSettings");	 // b0:0 render settings
	m_rootSignature->AddRootCBV(1, 0, "renderData");		 // b1:0 render data
	m_rootSignature->AddRootCBV(2, 0, "postProcessSettings");// b2:0 post processing settings
	m_rootSignature->AddStaticSampler(0);					 // s0:0 linear sampler
	m_rootSignature->Build(device, L"RT Root Signature");

	m_rtPipeline = std::make_unique<RTPipeline>(device, m_rootSignature->Get(), *m_shaderCompiler, m_scene->GetHitGroupRecords(), "shaders/raytracing.slang");

	m_tonemappingPass = std::make_unique<PostProcessPass>(m_context, *m_shaderCompiler, "shaders/tonemapping_pass.slang", "CSMain");
	m_autoExposurePass = std::make_unique<PostProcessPass>(m_context, *m_shaderCompiler, "shaders/autoexposure_pass.slang", "AutoExposure");
	
	m_autoExposureBuffer = std::make_unique<TypedBuffer>(
		m_context, 1, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "Auto Exposure Buffer");
	m_autoExposureReadback = m_allocator->CreateBuffer(
		sizeof(float), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_READBACK, "Auto Exposure Readback");

	m_renderSettingsCB = std::make_unique<CBVBuffer<RenderSettings>>(*m_allocator, "Render Settings CB");
	m_renderDataCB = std::make_unique<CBVBuffer<RenderData>>(*m_allocator, "Render Data CB");
	m_postProcessSettingsCB = std::make_unique<CBVBuffer<PostProcessSettings>>(*m_allocator, "Post Process Settings CB");

	m_commandQueue->ExecuteCommandList(commandList);
	m_commandQueue->Flush();
}

Renderer::~Renderer()
{
	m_commandQueue->Flush();
}

void Renderer::ToggleFullscreen()
{
	m_ngx->SetDLSSQuality(m_renderSettings.dlssQuality);
	m_swapChain->ToggleFullscreen();
	Resize(m_window.GetWidth(), m_window.GetHeight());
}

void Renderer::LoadModel(const std::string& path)
{
	if (m_scene->LoadModel(path))
	{
		m_rtPipeline->RebuildShaderTables(m_device->GetDevice(), m_scene->GetHitGroupRecords());
		ResetAccumulation();
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

	std::cout << "Window resized: " << width << "x" << height << "\n";
	auto device = m_device->GetDevice();
	ResetAccumulation();
	m_commandQueue->Flush();
	m_swapChain->Resize(m_context, width, height);

	glm::ivec2 renderSize = m_renderSettings.denoising ? m_ngx->Resize() : glm::ivec2(width, height);
	m_outputBuffer->Resize(device, width, height);
	m_dlssOutputBuffer->Resize(device, width, height);
	m_rtOutputBuffer->Resize(device, renderSize.x, renderSize.y);
	m_albedoBuffer->Resize(device, renderSize.x, renderSize.y);
	m_specularAlbedoBuffer->Resize(device, renderSize.x, renderSize.y);
	m_normalRoughnessBuffer->Resize(device, renderSize.x, renderSize.y);
	m_depthBuffer->Resize(device, renderSize.x, renderSize.y);
	m_motionVectorsBuffer->Resize(device, renderSize.x, renderSize.y);
}

void Renderer::Render(const float deltaTime)
{
	if (m_pendingResize)
	{
		m_ngx->SetDLSSQuality(m_renderSettings.dlssQuality);
		Resize(m_window.GetWidth(), m_window.GetHeight());
		m_pendingResize = false;
	}

	auto backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
	auto backBuffer = m_swapChain->GetCurrentBackBuffer();
	auto commandList = m_commandQueue->GetCommandList();
	auto commandQueue = m_commandQueue->GetQueue();
	glm::ivec2 windowSize = glm::ivec2(m_window.GetWidth(), m_window.GetHeight());
	glm::ivec2 renderSize = m_renderSettings.denoising ? glm::ivec2(m_ngx->GetRenderWidth(), m_ngx->GetRenderHeight()) : windowSize;

	float* mapped = nullptr;
	D3D12_RANGE readRange = { 0, sizeof(float) };
	m_autoExposureReadback.resource->Map(0, &readRange, reinterpret_cast<void**>(&mapped));
	m_postProcessSettings.exposure = *mapped;
	D3D12_RANGE writeRange = { 0, 0 };
	m_autoExposureReadback.resource->Unmap(0, &writeRange);

	if (m_reloadTimer >= 0.5f)
	{
		m_tonemappingPass->CheckHotReload(*m_commandQueue);
		m_autoExposurePass->CheckHotReload(*m_commandQueue);
		if (m_rtPipeline->CheckHotReload(m_device->GetDevice(), *m_commandQueue, m_scene->GetHitGroupRecords()))
		{
			ResetAccumulation();
		}
		m_reloadTimer = 0.0f;
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
	camData.fov = m_camera->m_fov;
	camData.aperture = m_camera->m_aperture;
	camData.focusDistance = m_camera->m_focusDistance;

	// Begin frame
	{
		m_imgui->BeginFrame();
		m_outputBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
	}

	// ImGui window
	{
		if (!m_rtPipeline->IsLastCompileSuccesful())
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
			ImGui::TextWrapped("%s", m_rtPipeline->GetLastCompileError().c_str());
			ImGui::PopTextWrapPos();
			ImGui::End();
		}

		auto config = ImSettings();
		config.push<float>().as_drag().min(0).max(10).speed(0.02f).pop();
		auto config2 = ImSettings();
		config2.push<float>().as_drag().min(0).max(100).speed(0.02f).pop();

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::Begin("Debug");
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame: %u", m_renderData.frame);
		ImGui::Text("Resolution: %ux%u", windowSize.x, windowSize.y);
		ImGui::Text("Render Resolution: %ux%u", renderSize.x, renderSize.y);
		auto responseRender = ImReflect::Input("Render Settings", m_renderSettings, config);
		auto responsePost = ImReflect::Input("Post Process Settings", m_postProcessSettings, config);
		auto responseCamera = ImReflect::Input("Camera", camData, config2);
		if (responseRender.get<RenderSettings>().is_changed()) ResetAccumulation();
		if (responseRender.get_member<&RenderSettings::denoising>().is_changed()) m_pendingResize = true;
		if (responseRender.get_member<&RenderSettings::dlssQuality>().is_changed()) m_pendingResize = true;
		if (responseCamera.get<CameraData>().is_changed())
		{
			ResetAccumulation();
			m_camera->SetPosition(camData.position);
			m_camera->SetDirection(camData.forward);
			m_camera->m_fov = camData.fov;
			m_camera->m_aperture = camData.aperture;
			m_camera->m_focusDistance = camData.focusDistance;
		}
		
		ImGui::End();
	}

	m_renderData.prevCamera = m_prevCamData;
	m_renderData.camera = camData;
	m_renderData.hdriIndex = m_scene->GetHDRIDescriptorIndex();
	m_renderData.deltaTime = deltaTime;
	glm::vec2 jitter = m_ngx->GetJitter(m_renderData.frame);
	m_renderData.camera.jitterX = jitter.x;
	m_renderData.camera.jitterY = jitter.y;
	m_renderSettingsCB->Update(backBufferIndex, m_renderSettings);
	m_renderDataCB->Update(backBufferIndex, m_renderData);
	m_postProcessSettingsCB->Update(backBufferIndex, m_postProcessSettings);

	// Record commands
	{
		// Raytracing pass
		{
			ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap() };
			commandList->SetDescriptorHeaps(1, heaps);
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

			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetTLASAddress(),			"sceneBVH");
			m_rootSignature->SetRootSRV(commandList.Get(), m_scene->GetMaterialsBufferAddress(),"materials");

			auto dispatchDesc = m_rtPipeline->GetDispatchRaysDesc();
			dispatchDesc.Width = m_renderSettings.denoising ? renderSize.x : windowSize.x;
			dispatchDesc.Height = m_renderSettings.denoising ? renderSize.y : windowSize.y;
			commandList->DispatchRays(&dispatchDesc);
		}

		// DLSS pass
		{
			if (m_ngx->IsDLSSSupported() && m_renderSettings.denoising)
			{
				D3D12_RESOURCE_BARRIER uavBarriers[] = {
					CD3DX12_RESOURCE_BARRIER::UAV(m_depthBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_motionVectorsBuffer->GetResource()),
					CD3DX12_RESOURCE_BARRIER::UAV(m_rtOutputBuffer->GetResource()),
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

				ID3D12DescriptorHeap* heaps[] = { m_descriptorHeap->GetHeap() };
				commandList->SetDescriptorHeaps(1, heaps);
			}
		}

		// Tonemapping pass
		{
			OutputBuffer& inputBuffer = m_renderSettings.denoising ? *m_dlssOutputBuffer : *m_rtOutputBuffer;
			inputBuffer.Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			m_outputBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			PostProcessPass::PostProcessBindings bindings;
			bindings.inputSrv = inputBuffer.GetSRV().gpuHandle;
			bindings.outputUav = m_outputBuffer->GetUAV().gpuHandle;
			bindings.constants[0] = m_renderSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.constants[1] = m_renderDataCB->GetGPUAddress(backBufferIndex);
			bindings.constants[2] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
			bindings.constantCount = 3;
			bindings.width = static_cast<uint32_t>(m_swapChain->GetViewport().Width);
			bindings.height = static_cast<uint32_t>(m_swapChain->GetViewport().Height);

			m_tonemappingPass->Dispatch(commandList.Get(), bindings);

			m_rtOutputBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		}

		// Auto exposure pass
		{
			if (m_postProcessSettings.autoExposure)
			{
				m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

				PostProcessPass::PostProcessBindings bindings;
				bindings.inputSrv = m_swapChain->GetCurrentBackBufferSRV().gpuHandle;
				bindings.outputUav = m_autoExposureBuffer->GetUAV().gpuHandle;
				bindings.constants[0] = m_renderDataCB->GetGPUAddress(backBufferIndex);
				bindings.constants[1] = m_postProcessSettingsCB->GetGPUAddress(backBufferIndex);
				bindings.constantCount = 2;
				bindings.width = 1;
				bindings.height = 1;

				m_autoExposurePass->Dispatch(commandList.Get(), bindings);

				m_autoExposureBuffer->GetBuffer().Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);

				commandList->CopyResource(m_autoExposureReadback.resource, m_autoExposureBuffer->GetResource());

				m_autoExposureBuffer->GetBuffer().Transition(commandList.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			}
		}
	}

	// End frame
	{
		m_outputBuffer->Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE);
		m_swapChain->Transition(commandList.Get(), D3D12_RESOURCE_STATE_COPY_DEST);
		commandList->CopyResource(backBuffer, m_outputBuffer->GetResource());

		// ImGui
		{
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
		m_commandQueue->WaitForFenceValue(m_fenceValues[m_swapChain->GetCurrentBackBufferIndex()]);
		
		m_renderData.frame++;
		if (camData.position != m_prevCamData.position || camData.forward != m_prevCamData.forward)
		{
			ResetAccumulation();
		}
		m_prevCamData = camData;
	}
}