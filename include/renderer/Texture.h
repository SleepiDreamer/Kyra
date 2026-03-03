#pragma once
#include "GPUBuffer.h"
#include "DescriptorHeap.h"
#include "CommonDX.h"

class Texture
{
public:
    void Create(const RenderContext& context, const void* data, uint32_t width, uint32_t height,
                DXGI_FORMAT format, const std::string& name);

    [[nodiscard]] int32_t GetDescriptorIndex() const;
	[[nodiscard]] ID3D12Resource* GetResource() const { return m_resource.resource; }

private:
    GPUBuffer m_resource;
    Descriptor m_srv;
};

