#include "Device.h"
#include "CommonDX.h"
#include "Window.h"
#include "Log.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

using namespace Microsoft::WRL;

Device::Device(const int width, const int height, const bool debug)
{
    if (debug)
    {
        EnableDebugLayer();
		Log::Info("Debug layer enabled!");
    }

	CreateAdapter();

	CreateDevice();
}

Device::~Device() = default;

void Device::EnableDebugLayer()
{
    ComPtr<ID3D12Debug> debugInterface;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
    debugInterface->EnableDebugLayer();

#if 0 // GPU-based validation
    ComPtr<ID3D12Debug1> debug1;
    if (SUCCEEDED(debugInterface.As(&debug1)))
    {
        debug1->SetEnableGPUBasedValidation(TRUE);
    }
#endif
}

void Device::CreateAdapter()
{
    ComPtr<IDXGIFactory4> factory;
    UINT createFactoryFlags = 0;

#ifdef _DEBUG
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    // Create factory
    ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory)), "Failed to create DXGI Factory!");

    ComPtr<IDXGIAdapter1> adapter1; // Used to query for adapters
    DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;

    SIZE_T maxVram = 0;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter1) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        ThrowIfFailed(adapter1->GetDesc1(&dxgiAdapterDesc1));

        if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) &&
            dxgiAdapterDesc1.DedicatedVideoMemory > maxVram)
        {
            maxVram = dxgiAdapterDesc1.DedicatedVideoMemory;
            ThrowIfFailed(adapter1.As(&m_adapter));
        }
    }

    ThrowIfFailed(m_adapter->GetDesc1(&dxgiAdapterDesc1));

    std::wstring gpuName = dxgiAdapterDesc1.Description;
	Log::Info("GPU: {}", ToNarrowString(gpuName));
	Log::Info("VRAM: {:.1f} GB\n", static_cast<float>(dxgiAdapterDesc1.DedicatedVideoMemory) / 1024.0f / 1024.0f / 1024.0f);
}

void Device::CreateDevice()
{
    ThrowIfFailed(D3D12CreateDevice(m_adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&m_device)), "Failed to create device!");

    D3D12_FEATURE_DATA_D3D12_OPTIONS5 capabilities = {};
    ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &capabilities, sizeof(capabilities)));
	if (capabilities.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
	{
        ThrowError("Raytracing not supported on this device!");
	}

#ifdef _DEBUG // Enable debug messages, from https://www.3dgep.com/learning-directx-12-2/
    ComPtr<ID3D12InfoQueue> pInfoQueue;
    if (SUCCEEDED(m_device.As(&pInfoQueue)))
    {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

        // Suppress whole categories of messages
        // D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] = { D3D12_MESSAGE_SEVERITY_INFO };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,  // I'm really not sure how to avoid this message.
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,    // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,  // This warning occurs when using capture frame while graphics debugging.
            D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        // NewFilter.DenyList.NumCategories = _countof(Categories);
        // NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        ThrowIfFailed(pInfoQueue->PushStorageFilter(&NewFilter));
    }
#endif
}