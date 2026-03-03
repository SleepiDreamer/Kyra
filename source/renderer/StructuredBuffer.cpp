#include "StructuredBuffer.h"
#include "GPUAllocator.h"

StructuredBuffer::StructuredBuffer(RenderContext& context, const uint32_t elementCount, const uint32_t stride,
								   const D3D12_RESOURCE_FLAGS flags, const D3D12_HEAP_TYPE heapType, const char* name)
	: m_context(context), m_elementCount(elementCount), m_stride(stride)
{
	m_buffer = m_context.allocator->CreateBuffer(
        (uint64_t)m_elementCount * m_stride, D3D12_RESOURCE_STATE_COMMON, flags | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, heapType, name);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = m_elementCount;
    srvDesc.Buffer.StructureByteStride = m_stride;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = m_elementCount;
    uavDesc.Buffer.StructureByteStride = m_stride;

    m_srv = m_context.descriptorHeap->Allocate();
    m_uav = m_context.descriptorHeap->Allocate();
    m_context.device->CreateShaderResourceView(m_buffer.resource, &srvDesc, m_srv.cpuHandle);
    m_context.device->CreateUnorderedAccessView(m_buffer.resource, nullptr, &uavDesc, m_uav.cpuHandle);
}

StructuredBuffer::~StructuredBuffer()
{
    m_context.descriptorHeap->Free(m_srv);
    m_context.descriptorHeap->Free(m_uav);
}