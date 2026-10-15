#include "UploadContext.h"
#include "GPUAllocator.h"
#include "CommandQueue.h"
#include "CommonDX.h"

UploadContext::UploadContext(GPUAllocator& allocator, ID3D12Device10* device)
    : m_allocator(allocator)
{
    m_queue = std::make_unique<CommandQueue>(device, "UploadContext", D3D12_COMMAND_LIST_TYPE_COPY);
}

UploadContext::~UploadContext()
{
    Flush();
}

void UploadContext::Upload(const GPUBuffer& dest, const void* data, const uint64_t size)
{
    GPUBuffer staging = m_allocator.CreateBuffer(
        size, D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD, "Upload Staging Buffer");

    void* mapped = nullptr;
    D3D12_RANGE readRange{ 0, 0 };
    ThrowIfFailed(staging.resource->Map(0, &readRange, &mapped));
    memcpy(mapped, data, size);
    staging.resource->Unmap(0, nullptr);

    auto cmdList = m_queue->GetCommandList();
    cmdList->CopyBufferRegion(dest.resource, 0, staging.resource, 0, size);
    m_queue->ExecuteCommandList(cmdList);

    m_staged.push_back(std::move(staging));
}

void UploadContext::UploadTexture(const GPUBuffer& dest, const void* data, const uint32_t width, const uint32_t height, const uint16_t depth, const DXGI_FORMAT format)
{
    auto commandList = m_queue->GetCommandList();

    const UINT64 uploadSize = GetRequiredIntermediateSize(dest.resource, 0, 1);

    GPUBuffer staging =
        m_allocator.CreateBuffer(uploadSize, D3D12_RESOURCE_STATE_GENERIC_READ, 
        D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_UPLOAD, "Texture Upload Staging Buffer");

    UINT bytesPerPixel = 4;
    switch (format)
    {
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        bytesPerPixel = 8;
		break;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
        bytesPerPixel = 8;
		break;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		bytesPerPixel = 16;
        break;
    default:
		Log::Warning("[UploadContext::UploadTexture] Unsupported texture format. Defaulting to 4 BPP");
    }

    D3D12_SUBRESOURCE_DATA subresourceData{};
    subresourceData.pData = data;
    subresourceData.RowPitch = static_cast<LONG_PTR>(width) * bytesPerPixel;
    subresourceData.SlicePitch = subresourceData.RowPitch * height;

    UpdateSubresources(commandList.Get(), dest.resource, staging.resource, 0, 0, 1, &subresourceData);

    m_queue->ExecuteCommandList(commandList);
    m_staged.push_back(std::move(staging));
}


void UploadContext::Flush()
{
    if (m_staged.empty())
        return;

    m_queue->Flush();
    m_staged.clear();
}
