#include "ImGuiWrapper.h"
#include "CommandQueue.h"
#include "DescriptorHeap.h"
#include "Window.h"

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_dx12.h>
#include <glfw3.h>

ImGuiWrapper::ImGuiWrapper(const Window& window, RenderContext& context, const DXGI_FORMAT rtvFormat, const uint32_t framesInFlight)
    : m_context(context)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    float xScale, yScale;
    glfwGetWindowContentScale(window.GetGLFWWindow(), &xScale, &yScale);
    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = xScale;
    io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::GetStyle().ScaleAllSizes(xScale);

    ImGui_ImplGlfw_InitForOther(window.GetGLFWWindow(), true);

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = m_context.device;
    initInfo.CommandQueue = m_context.commandQueue->GetQueue().Get();
    initInfo.NumFramesInFlight = static_cast<int>(framesInFlight);
    initInfo.RTVFormat = rtvFormat;
    initInfo.SrvDescriptorHeap = m_context.descriptorHeap->GetHeap();
    initInfo.UserData = m_context.descriptorHeap;

    initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
        D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
        {
            auto* heap = static_cast<DescriptorHeap*>(info->UserData);
            auto alloc = heap->Allocate();
            *outCpu = alloc.cpuHandle;
            *outGpu = alloc.gpuHandle;
        };

    initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info,
        D3D12_CPU_DESCRIPTOR_HANDLE cpu,
        D3D12_GPU_DESCRIPTOR_HANDLE gpu)
        {
            auto* heap = static_cast<DescriptorHeap*>(info->UserData);
            // Reconstruct allocation from handles to free
            Descriptor alloc;
            alloc.cpuHandle = cpu;
            alloc.gpuHandle = gpu;
            alloc.index = static_cast<UINT>((cpu.ptr - heap->GetCPUStartHandle().ptr) / heap->GetIncrementSize());
            heap->Free(alloc);
        };

    ImGui_ImplDX12_Init(&initInfo);
}

ImGuiWrapper::~ImGuiWrapper()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_context.descriptorHeap->Free(m_fontDescriptor);
}

void ImGuiWrapper::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiWrapper::EndFrame(ID3D12GraphicsCommandList* cmdList)
{
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
}