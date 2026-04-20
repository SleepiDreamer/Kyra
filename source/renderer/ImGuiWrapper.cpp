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
    SetupImGuiStyle();

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

void ImGuiWrapper::SetupImGuiStyle()
{
    // All styles from @TheAncientOwl on GitHub: https://github.com/ocornut/imgui/issues/707

#if 0 // GruvboxHard
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- 1. Sizing and Spacing (Industrial & Square) ---
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;

    // --- 2. Borders & Rounding (Gruvbox usually looks best with sharp or low rounding) ---
    style.WindowRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    // --- 3. The Gruvbox Dark Hard Palette ---
    // Background: #1d2021 (Dark Hard) | Foreground: #ebdbb2
    // Red: #fb4934 | Green: #b8bb26 | Yellow: #fabd2f | Blue: #83a598
    // Gray: #928374 | Orange: #fe8019

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.92f, 0.86f, 0.70f, 1.00f); // #ebdbb2
    colors[ImGuiCol_TextDisabled] = ImVec4(0.57f, 0.51f, 0.45f, 1.00f); // #928374

    // Backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.13f, 0.13f, 1.00f); // #1d2021
    colors[ImGuiCol_ChildBg] = ImVec4(0.11f, 0.13f, 0.13f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.13f, 0.13f, 0.95f);

    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f); // #504945
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames
    colors[ImGuiCol_FrameBg] = ImVec4(0.24f, 0.22f, 0.21f, 1.00f); // #3c3836
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f); // #504945
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.40f, 0.36f, 0.33f, 1.00f); // #665c54

    // Title Bars
    colors[ImGuiCol_TitleBg] = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);

    // Menus
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.14f, 0.13f, 1.00f); // #282828

    // Scrollbars
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.40f, 0.36f, 0.33f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.57f, 0.51f, 0.45f, 1.00f);

    // Interactables
    colors[ImGuiCol_CheckMark] = ImVec4(0.72f, 0.73f, 0.15f, 1.00f); // #b8bb26 (Green)
    colors[ImGuiCol_SliderGrab] = ImVec4(0.51f, 0.65f, 0.60f, 1.00f); // #83a598 (Blue)
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.73f, 0.67f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.98f, 0.29f, 0.20f, 1.00f); // #fb4934 (Red)
    colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.20f, 0.15f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.24f, 0.22f, 0.21f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.36f, 0.33f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.24f, 0.22f, 0.21f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);

    // Misc
    colors[ImGuiCol_PlotLines] = ImVec4(0.98f, 0.74f, 0.18f, 1.00f); // #fabd2f (Yellow)
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.31f, 0.29f, 0.27f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.98f, 0.29f, 0.20f, 1.00f);

#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview] = ImVec4(0.72f, 0.73f, 0.15f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.11f, 0.13f, 0.13f, 1.00f);
#endif
#endif

#if 0 // CatppuccinMocha
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- 1. Sizing and Spacing (Soft & Modern) ---
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;

    // --- 2. Borders & Rounding ---
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 5.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 5.0f;
    style.TabRounding = 5.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f; // Minimalist look
    style.PopupBorderSize = 1.0f;

    // --- 3. The Catppuccin Mocha Palette ---
    // Base: #1e1e2e | Mantle: #181825 | Crust: #11111b
    // Text: #cdd6f4 | Subtext0: #a6adc8 | Surface0: #313244
    // Lavender: #b4befe | Sapphire: #74c7ec | Mauve: #cba6f7

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.80f, 0.84f, 0.96f, 1.00f); // Text
    colors[ImGuiCol_TextDisabled] = ImVec4(0.42f, 0.45f, 0.55f, 1.00f); // Surface1

    // Backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f); // Base
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f); // Mantle
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.11f, 0.96f); // Crust

    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.19f, 0.20f, 0.27f, 1.00f); // Surface0
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames (Inputs, etc.)
    colors[ImGuiCol_FrameBg] = ImVec4(0.19f, 0.20f, 0.27f, 1.00f); // Surface0
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.26f, 0.35f, 1.00f); // Surface1
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.31f, 0.32f, 0.42f, 1.00f); // Surface2

    // Title Bars
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f); // Mantle
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f); // Base
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.07f, 0.07f, 0.11f, 1.00f); // Crust

    // Menus
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);

    // Scrollbars
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.31f, 0.32f, 0.42f, 1.00f); // Surface2
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.37f, 0.38f, 0.51f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.42f, 0.45f, 0.55f, 1.00f);

    // Interactables
    colors[ImGuiCol_CheckMark] = ImVec4(0.71f, 0.75f, 1.00f, 1.00f); // Lavender
    colors[ImGuiCol_SliderGrab] = ImVec4(0.45f, 0.78f, 0.93f, 1.00f); // Sapphire
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.78f, 0.93f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.80f, 0.65f, 0.97f, 1.00f); // Mauve
    colors[ImGuiCol_ButtonActive] = ImVec4(0.70f, 0.55f, 0.87f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.26f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.31f, 0.32f, 0.42f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.31f, 0.32f, 0.42f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.19f, 0.20f, 0.27f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.09f, 0.09f, 0.15f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);

    // Misc
    colors[ImGuiCol_PlotLines] = ImVec4(0.94f, 0.72f, 0.42f, 1.00f); // Marigold
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.31f, 0.32f, 0.42f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.71f, 0.75f, 1.00f, 1.00f); // Lavender

#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview] = ImVec4(0.71f, 0.75f, 1.00f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
#endif
#endif

#if 0 // Dracula
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- 1. Sizing and Spacing (Clean & Balanced) ---
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 14.0f;
    style.GrabMinSize = 12.0f;

    // --- 2. Borders & Rounding ---
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    // --- 3. The Dracula Color Palette ---
    // Background: #282a36 | Selection: #44475a | Foreground: #f8f8f2
    // Comment: #6272a4    | Cyan: #8be9fd      | Green: #50fa7b
    // Orange: #ffb86c     | Pink: #ff79c6      | Purple: #bd93f9
    // Red: #ff5555        | Yellow: #f1fa8c

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.97f, 0.97f, 0.95f, 1.00f); // #f8f8f2
    colors[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f); // #6272a4

    // Backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f); // #282a36
    colors[ImGuiCol_ChildBg] = ImVec4(0.16f, 0.16f, 0.21f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.16f, 0.16f, 0.21f, 0.96f);

    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f); // #44475a
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames (Inputs, etc.)
    colors[ImGuiCol_FrameBg] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f); // #44475a
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f); // #6272a4
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);

    // Title Bars
    colors[ImGuiCol_TitleBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f); // Darker
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);

    // Menus
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);

    // Scrollbars
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);

    // Interactables
    colors[ImGuiCol_CheckMark] = ImVec4(0.31f, 0.98f, 0.48f, 1.00f); // #50fa7b (Green)
    colors[ImGuiCol_SliderGrab] = ImVec4(0.74f, 0.58f, 0.98f, 1.00f); // #bd93f9 (Purple)
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.84f, 0.68f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.47f, 0.78f, 1.00f); // #ff79c6 (Pink)
    colors[ImGuiCol_ButtonActive] = ImVec4(0.80f, 0.37f, 0.62f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.48f, 0.55f, 0.74f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.13f, 0.14f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);

    // Tables
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.38f, 0.45f, 0.64f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);

    // Misc
    colors[ImGuiCol_PlotLines] = ImVec4(0.55f, 0.91f, 0.99f, 1.00f); // #8be9fd (Cyan)
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.27f, 0.28f, 0.35f, 1.00f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.74f, 0.58f, 0.98f, 1.00f);

#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview] = ImVec4(0.74f, 0.58f, 0.98f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.16f, 0.16f, 0.21f, 1.00f);
#endif
#endif

#if 1 // Sapphire
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- 1. Sizing and Spacing ---
    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 4.0f);
    style.ScrollbarSize = 15.0f;
    style.GrabMinSize = 10.0f;

    // --- 2. Borders & Rounding ---
    style.WindowRounding = 5.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 12.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;

    // --- 3. Color Palette ---

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.93f, 0.97f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.40f, 0.50f, 0.65f, 1.00f);

    // Backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.09f, 0.12f, 1.00f); // Deep midnight
    colors[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.12f, 0.16f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.09f, 0.12f, 0.95f);

    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.15f, 0.25f, 0.35f, 0.70f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Frames (Inputs, Checkboxes, etc.)
    colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.18f, 0.26f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.18f, 0.28f, 0.40f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.38f, 0.55f, 1.00f);

    // Title Bars
    colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.22f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.05f, 0.08f, 0.12f, 1.00f);

    // Menus
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.16f, 0.22f, 1.00f);

    // Scrollbars
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.08f, 0.11f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.32f, 0.48f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.28f, 0.42f, 0.60f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.50f, 0.75f, 1.00f);

    // Interactables
    colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.55f, 0.85f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.45f, 0.75f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.48f, 0.75f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.48f, 0.75f, 1.00f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.35f, 0.60f, 0.90f, 1.00f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.20f, 0.32f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.45f, 0.70f, 1.00f);
    colors[ImGuiCol_TabActive] = ImVec4(0.18f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.12f, 0.20f, 0.32f, 1.00f);

    // Tables
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.15f, 0.25f, 0.40f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.20f, 0.35f, 0.55f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.15f, 0.25f, 0.40f, 1.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);

    // Misc
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.30f, 0.55f, 0.85f, 0.40f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.50f, 0.80f, 1.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.40f, 0.70f, 1.00f, 1.00f);

#ifdef IMGUI_HAS_DOCK
    colors[ImGuiCol_DockingPreview] = ImVec4(0.25f, 0.50f, 0.80f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.07f, 0.09f, 0.12f, 1.00f);
#endif
#endif
}