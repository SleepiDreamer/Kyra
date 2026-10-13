#include "BLAS.h"
#include "GPUAllocator.h"
#include "CommandQueue.h"

BLAS::BLAS(const RenderContext& context)
    : m_allocator(*context.allocator), m_commandQueue(*context.commandQueue)
{
}

BLAS::~BLAS() = default;

void BLAS::Build(ID3D12Device10* device, const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometries)
{
    auto commandList = m_commandQueue.GetCommandList();
    Build(device, commandList.Get(), geometries);

    m_commandQueue.ExecuteCommandList(commandList);
    m_commandQueue.Flush();
}

void BLAS::Build(ID3D12Device10* device, ID3D12GraphicsCommandList4* commandList, const std::vector<D3D12_RAYTRACING_GEOMETRY_DESC>& geometries)
{
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs{};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.NumDescs = static_cast<UINT>(geometries.size());
    inputs.pGeometryDescs = geometries.data();
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuildInfo{};
    device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuildInfo);

    m_result = m_allocator.CreateBuffer(
        prebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "BLAS Result Buffer");

    m_scratch = m_allocator.CreateBuffer(
        prebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_TYPE_DEFAULT, "BLAS Scratch Buffer");

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc{};
    buildDesc.Inputs = inputs;
    buildDesc.DestAccelerationStructureData = m_result.resource->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = m_scratch.resource->GetGPUVirtualAddress();
    commandList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(nullptr);
    commandList->ResourceBarrier(1, &barrier);
}