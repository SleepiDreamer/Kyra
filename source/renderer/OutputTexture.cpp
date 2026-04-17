#include "OutputTexture.h"
#include "GPUAllocator.h"

OutputBuffer::OutputBuffer(RenderContext& context, const DXGI_FORMAT format, const int width, const int height, std::wstring name)
	: m_context(context), m_format(format), m_name(std::move(name)), m_width(width), m_height(height)
{
    Create(m_context.device, m_width, m_height);
}

OutputBuffer::~OutputBuffer()
{
    if (m_initialized)
    {
        m_context.descriptorHeap->Free(m_uav);
        m_context.descriptorHeap->Free(m_srv);
    }
}

void OutputBuffer::Create(ID3D12Device* device, const uint32_t width, const uint32_t height)
{
    m_width = width;
    m_height = height;

    if (m_initialized)
    {
        m_context.descriptorHeap->Free(m_uav);
        m_context.descriptorHeap->Free(m_srv);
    }

	m_resource = m_context.allocator->CreateTexture(m_width, m_height, m_format, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, m_name.c_str());
    m_currentState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    m_uav = m_context.descriptorHeap->Allocate();
    m_srv = m_context.descriptorHeap->Allocate();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = m_format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = m_format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateUnorderedAccessView(m_resource.resource, nullptr, &uavDesc, m_uav.cpuHandle);
    device->CreateShaderResourceView(m_resource.resource, &srvDesc, m_srv.cpuHandle);

    m_initialized = true;
}

void OutputBuffer::Resize(ID3D12Device* device, const uint32_t width, const uint32_t height)
{
	m_width = width;
	m_height = height;
    Create(device, m_width, m_height);
}

void OutputBuffer::Transition(ID3D12GraphicsCommandList* commandList, const D3D12_RESOURCE_STATES newState)
{
    if (newState != m_currentState)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = m_resource.resource;
        barrier.Transition.StateBefore = m_currentState;
        barrier.Transition.StateAfter = newState;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        commandList->ResourceBarrier(1, &barrier);
        m_currentState = newState;
	}
}
