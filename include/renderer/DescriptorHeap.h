#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>

struct Descriptor
{
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle{};
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{};
	UINT index = 0;
};

class DescriptorHeap
{
public:

	DescriptorHeap(ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE type,
		UINT capacity,
		bool shaderVisible,
		const wchar_t* name);
	~DescriptorHeap() = default;

	Descriptor Allocate(UINT count = 1);
	void Free(const Descriptor& alloc, UINT count = 1);

	[[nodiscard]] ID3D12DescriptorHeap* GetHeap() const { return m_heap.Get(); }
	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(const UINT index) const { return { m_cpuStart.ptr + index * m_incrementSize }; }
	[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(const UINT index) const { return { m_gpuStart.ptr + index * m_incrementSize }; }
	[[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetCPUStartHandle() const { return m_cpuStart; }
	[[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGPUStartHandle() const { return m_gpuStart; }
	[[nodiscard]] UINT GetIncrementSize() const { return m_incrementSize; }

private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
	D3D12_DESCRIPTOR_HEAP_TYPE m_type;
	D3D12_CPU_DESCRIPTOR_HANDLE m_cpuStart{};
	D3D12_GPU_DESCRIPTOR_HANDLE m_gpuStart{};
	UINT m_incrementSize = 0;
	UINT m_capacity = 0;
	UINT m_current = 0;
	std::vector<UINT> m_freeList;
};