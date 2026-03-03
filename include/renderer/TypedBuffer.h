#pragma once
#include "GPUBuffer.h"
#include "DescriptorHeap.h"

class TypedBuffer
{
public:
	TypedBuffer(
		RenderContext& context, uint32_t elementCount, DXGI_FORMAT format,
		D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_TYPE heapType, const char* name);
	~TypedBuffer();
	TypedBuffer(const TypedBuffer&) = delete;
	TypedBuffer& operator=(const TypedBuffer&) = delete;
	TypedBuffer(TypedBuffer&&) = delete;

	[[nodiscard]] GPUBuffer& GetBuffer() { return m_buffer; }
	[[nodiscard]] ID3D12Resource* GetResource() const { return m_buffer.resource; }
	[[nodiscard]] Descriptor GetSRV() const { return m_srv; }
	[[nodiscard]] Descriptor GetUAV() const { return m_uav; }
	[[nodiscard]] uint32_t GetElementCount() const { return m_elementCount; }
	[[nodiscard]] uint32_t GetElementStride() const { return m_stride; }
	[[nodiscard]] uint32_t GetSize() const { return m_elementCount * m_stride; }
	[[nodiscard]] DXGI_FORMAT GetFormat() const { return m_format; }
private:
	RenderContext& m_context;
	GPUBuffer m_buffer;
	Descriptor m_srv;
	Descriptor m_uav;

	uint32_t m_elementCount;
	uint32_t m_stride;
	DXGI_FORMAT m_format;
};
