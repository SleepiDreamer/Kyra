#include "GPUAllocator.h"

GPUAllocator::GPUAllocator(ID3D12Device* device, IDXGIAdapter* adapter)
{
    D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
    allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
    allocatorDesc.pDevice = device;
    allocatorDesc.pAdapter = adapter;
	ThrowIfFailed(D3D12MA::CreateAllocator(&allocatorDesc, &m_allocator), "Failed to create GPU memory allocator!");
}

GPUAllocator::~GPUAllocator()
{
    if (m_allocator)
    {
        m_allocator->Release();
        m_allocator = nullptr;
    }
}

GPUBuffer GPUAllocator::CreateBuffer(const uint64_t size, const D3D12_RESOURCE_STATES initialState,
									 const D3D12_RESOURCE_FLAGS flags, const D3D12_HEAP_TYPE heapType, const char* name) const
{
    if (size == 0)
    {
		Log::Warning("Creating zero-sized buffer ({})", name);
    }

    D3D12_RESOURCE_DESC resourceDesc = BUFFER_RESOURCE;
    resourceDesc.Width = std::max(size, 1ULL);
    resourceDesc.Flags = flags;

    D3D12MA::ALLOCATION_DESC allocDesc{};
    allocDesc.HeapType = heapType;

    GPUBuffer buffer{};
    buffer.size = std::max(size, 1ULL);

    ThrowIfFailed(m_allocator->CreateResource(
        &allocDesc,
        &resourceDesc,
        initialState,
        nullptr,
        &buffer.allocation,
        IID_PPV_ARGS(&buffer.resource)));
    buffer.resource->SetName(ToWideString(name).c_str());

    return buffer;
}

GPUBuffer GPUAllocator::CreateTexture(const uint32_t width, const uint32_t height, const DXGI_FORMAT format,
    const D3D12_RESOURCE_STATES initialState, const D3D12_RESOURCE_FLAGS flags, const wchar_t* name) const
{
    D3D12_RESOURCE_DESC resourceDesc = TEXTURE_RESOURCE;
    resourceDesc.Format = format;
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.Flags = flags;

    D3D12MA::ALLOCATION_DESC allocDesc{};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    GPUBuffer buffer{};
    buffer.size = 0;

    ThrowIfFailed(m_allocator->CreateResource(
        &allocDesc,
        &resourceDesc,
        initialState,
        nullptr,
        &buffer.allocation,
        IID_PPV_ARGS(&buffer.resource)));

    buffer.resource->SetName(name);
    return buffer;
}
