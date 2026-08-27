#pragma once
#include "GPUBuffer.h"
#include "DescriptorHeap.h"

class StructuredBuffer
{
public:
	StructuredBuffer(RenderContext& context, uint32_t elementCount, uint32_t stride, 
		D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType, std::string name);
	~StructuredBuffer();
	StructuredBuffer(const StructuredBuffer&) = delete;
	StructuredBuffer& operator=(const StructuredBuffer&) = delete;
	StructuredBuffer(StructuredBuffer&&) = delete;

	void Init(uint32_t elementCount);
	void Update(const void* data, uint32_t elementCount = 0);
	void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES newState);
	void UAVBarrier(ID3D12GraphicsCommandList* commandList) const;

	[[nodiscard]] GPUBuffer& GetBuffer() { return m_buffer; }
	[[nodiscard]] ID3D12Resource* GetResource() const { return m_buffer.resource; }
	[[nodiscard]] Descriptor GetSRV() const { return m_srv; }
	[[nodiscard]] Descriptor GetUAV() const { return m_uav; }
	[[nodiscard]] Descriptor GetClearUAVGPU() const { return m_clearUav; }
	[[nodiscard]] uint32_t GetElementCount() const { return m_elementCount; }
	[[nodiscard]] uint32_t GetElementStride() const { return m_stride; }
	[[nodiscard]] uint32_t GetSize() const { return m_elementCount * m_stride; }
	[[nodiscard]] D3D12_RESOURCE_STATES GetState() const { return m_buffer.state; }
	[[nodiscard]] const std::string& GetName() const { return m_name; }
private:
	RenderContext& m_context;
	GPUBuffer m_buffer;
	Descriptor m_srv;
	Descriptor m_uav;
	Descriptor m_clearUav;

	uint32_t m_elementCount = 0;
	uint32_t m_stride = 0;
	D3D12_RESOURCE_FLAGS m_flags;
	D3D12_HEAP_TYPE m_heapType;
	D3D12_RESOURCE_STATES m_currentState = D3D12_RESOURCE_STATE_COMMON;
	std::string m_name;
};
