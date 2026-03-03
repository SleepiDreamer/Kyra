#include "DescriptorHeap.h"
#include "CommonDX.h"

#include <d3dx12.h>
#include <cassert>

DescriptorHeap::DescriptorHeap(ID3D12Device* device, const D3D12_DESCRIPTOR_HEAP_TYPE type,
							   const UINT capacity, const bool shaderVisible, const wchar_t* name)
	: m_type(type), m_capacity(capacity)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.NumDescriptors = capacity;
	desc.Type = type;
	desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_heap)));
	m_heap->SetName(name);

	m_incrementSize = device->GetDescriptorHandleIncrementSize(type);
	m_cpuStart = m_heap->GetCPUDescriptorHandleForHeapStart();

	if (shaderVisible)
	{
		m_gpuStart = m_heap->GetGPUDescriptorHandleForHeapStart();
	}
}

Descriptor DescriptorHeap::Allocate(const UINT count)
{
	Descriptor alloc;

	if (count == 1 && !m_freeList.empty())
	{
		alloc.index = m_freeList.back();
		m_freeList.pop_back();
	}
	else
	{
		assert(m_current + count <= m_capacity && "Descriptor heap capacity exceeded!");

		alloc.index = m_current;
		m_current += count;
	}

	alloc.cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cpuStart, static_cast<int>(alloc.index), m_incrementSize);
	alloc.gpuHandle = m_gpuStart.ptr
		? CD3DX12_GPU_DESCRIPTOR_HANDLE(m_gpuStart, static_cast<int>(alloc.index), m_incrementSize)
		: D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

	return alloc;
}

void DescriptorHeap::Free(const Descriptor& alloc, const UINT count)
{
	for (UINT i = 0; i < count; i++)
	{
		m_freeList.push_back(alloc.index + i);
	}
}