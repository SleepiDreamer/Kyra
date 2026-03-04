#pragma once
#include "GPUBuffer.h"
#include "DescriptorHeap.h"

class StructuredBuffer
{
public:
	StructuredBuffer(
		RenderContext& context, uint32_t elementCount, uint32_t stride,
		D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType, bool supportClear, const char* name);
	~StructuredBuffer();
	StructuredBuffer(const StructuredBuffer&) = delete;
	StructuredBuffer& operator=(const StructuredBuffer&) = delete;
	StructuredBuffer(StructuredBuffer&&) = delete;

	[[nodiscard]] GPUBuffer& GetBuffer() { return m_buffer; }
	[[nodiscard]] ID3D12Resource* GetResource() const { return m_buffer.resource; }
	[[nodiscard]] Descriptor GetSRV() const { return m_srv; }
	[[nodiscard]] Descriptor GetUAV() const { return m_uav; }
	[[nodiscard]] Descriptor GetClearUAV() const { return m_clearUav; }
	[[nodiscard]] Descriptor GetClearUAVGPU() const { return m_clearUavGpu; }
	[[nodiscard]] uint32_t GetElementCount() const { return m_elementCount; }
	[[nodiscard]] uint32_t GetElementStride() const { return m_stride; }
	[[nodiscard]] uint32_t GetSize() const { return m_elementCount * m_stride; }
private:
	RenderContext& m_context;
	GPUBuffer m_buffer;
	Descriptor m_srv;
	Descriptor m_uav;
	Descriptor m_clearUav;
	Descriptor m_clearUavGpu;

	uint32_t m_elementCount = 0;
	uint32_t m_stride = 0;
};
