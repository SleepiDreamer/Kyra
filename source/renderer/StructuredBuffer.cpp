#include "StructuredBuffer.h"
#include "GPUAllocator.h"

StructuredBuffer::StructuredBuffer(RenderContext& context, const uint32_t elementCount, const uint32_t stride, const D3D12_RESOURCE_FLAGS flags, 
								   const D3D12_HEAP_TYPE heapType, bool supportClear, const char* name)
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

    D3D12_UNORDERED_ACCESS_VIEW_DESC clearUavDesc{};
    clearUavDesc.Format = DXGI_FORMAT_R32_UINT;
    clearUavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    clearUavDesc.Buffer.FirstElement = 0;
    clearUavDesc.Buffer.NumElements = m_elementCount * m_stride / sizeof(uint32_t);
    clearUavDesc.Buffer.StructureByteStride = 0;

    m_srv = m_context.descriptorHeap->Allocate();
    m_uav = m_context.descriptorHeap->Allocate();
    if (supportClear) m_clearUav = m_context.cpuDescriptorHeap->Allocate();
    if (supportClear) m_clearUavGpu = m_context.descriptorHeap->Allocate();
    m_context.device->CreateShaderResourceView(m_buffer.resource, &srvDesc, m_srv.cpuHandle);
    m_context.device->CreateUnorderedAccessView(m_buffer.resource, nullptr, &uavDesc, m_uav.cpuHandle);
    if (supportClear) m_context.device->CreateUnorderedAccessView(m_buffer.resource, nullptr, &clearUavDesc, m_clearUav.cpuHandle);
    if (supportClear) m_context.device->CreateUnorderedAccessView(m_buffer.resource, nullptr, &clearUavDesc, m_clearUavGpu.cpuHandle);
}

StructuredBuffer::~StructuredBuffer()
{
    m_context.descriptorHeap->Free(m_srv);
    m_context.descriptorHeap->Free(m_uav);
}