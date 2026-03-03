#include "TypedBuffer.h"
#include "GPUAllocator.h"

TypedBuffer::TypedBuffer(RenderContext& context, const uint32_t elementCount, DXGI_FORMAT format,
						 const D3D12_RESOURCE_FLAGS flags, const D3D12_HEAP_TYPE heapType, const char* name)
	: m_context(context), m_elementCount(elementCount), m_stride(FormatByteSize(format)), m_format(format)
{
    const bool wantsUav = (flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;

    m_buffer = m_context.allocator->CreateBuffer(
        (uint64_t)m_elementCount * m_stride, InitialStateFromHeapType(heapType), flags, heapType, name);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Buffer.NumElements = m_elementCount;
    srvDesc.Format = m_format;

    m_srv = m_context.descriptorHeap->Allocate();
    m_context.device->CreateShaderResourceView(m_buffer.resource, &srvDesc, m_srv.cpuHandle);

    if (wantsUav)
    {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = m_format;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.NumElements = m_elementCount;

        m_uav = m_context.descriptorHeap->Allocate();
        m_context.device->CreateUnorderedAccessView(m_buffer.resource, nullptr, &uavDesc, m_uav.cpuHandle);
    }
}

TypedBuffer::~TypedBuffer()
{
    m_context.descriptorHeap->Free(m_srv);
    m_context.descriptorHeap->Free(m_uav);
}