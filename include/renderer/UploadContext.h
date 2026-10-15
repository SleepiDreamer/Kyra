#pragma once
#include "GPUBuffer.h"
#include "CommonDX.h"

class GPUAllocator;
class CommandQueue;

class UploadContext
{
public:
    UploadContext(GPUAllocator& allocator, ID3D12Device10* device);
    ~UploadContext();

    void Upload(const GPUBuffer& dest, const void* data, uint64_t size);
    void UploadTexture(const GPUBuffer& dest, const void* data, uint32_t width, uint32_t height, uint16_t depth, DXGI_FORMAT format);
    void Flush();
private:
	GPUAllocator& m_allocator;
    std::unique_ptr<CommandQueue> m_queue;
    std::vector<GPUBuffer> m_staged;
};
