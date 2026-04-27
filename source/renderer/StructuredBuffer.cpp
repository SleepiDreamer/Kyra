#include "StructuredBuffer.h"
#include "CommandQueue.h"
#include "GPUAllocator.h"
#include "UploadContext.h"
#include "Log.h"

StructuredBuffer::StructuredBuffer(RenderContext& context, const uint32_t elementCount, const uint32_t stride,
                                   const D3D12_RESOURCE_FLAGS flags, const D3D12_HEAP_TYPE heapType, std::string name)
	: m_context(context), m_elementCount(elementCount), m_stride(stride), m_flags(flags), m_heapType(heapType), m_name(std::move(name))
{
	Init(m_elementCount);
}

StructuredBuffer::~StructuredBuffer()
{
    m_context.descriptorHeap->Free(m_srv);
    m_context.descriptorHeap->Free(m_uav);
}

void StructuredBuffer::Init(const uint32_t elementCount)
{
    m_elementCount = elementCount;

    m_buffer = m_context.allocator->CreateBuffer(
        static_cast<uint64_t>(m_elementCount) * m_stride, D3D12_RESOURCE_STATE_COMMON, 
        m_flags | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, m_heapType, m_name.c_str());

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

    if (m_srv.cpuHandle.ptr == 0) m_srv = m_context.descriptorHeap->Allocate();
    if (m_uav.cpuHandle.ptr == 0) m_uav = m_context.descriptorHeap->Allocate();
    m_context.device->CreateShaderResourceView(m_buffer.resource, &srvDesc, m_srv.cpuHandle);
    m_context.device->CreateUnorderedAccessView(m_buffer.resource, nullptr, &uavDesc, m_uav.cpuHandle);
}

void StructuredBuffer::Update(const void* data, uint32_t elementCount)
{
    if (!data)
    {
        Log::Error("StructuredBuffer::Update called with nullptr data on \"{}\"", m_name);
        return;
	}
    if (elementCount == 0)
    {
		elementCount = m_elementCount;
    }
    else if (elementCount > m_elementCount)
    {
		Log::Info("Resizing structured buffer \"{}\" from {} to {} elements", m_name, m_elementCount, elementCount);
        m_context.commandQueue->Flush();
        Init(elementCount);
    }

	m_context.uploadContext->Upload(m_buffer, data, static_cast<uint64_t>(elementCount) * m_stride);
}
