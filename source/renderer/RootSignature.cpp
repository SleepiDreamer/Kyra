#include "RootSignature.h"
#include "HelpersDX.h"
#include <iostream>

// Returns the root parameter index of the added descriptor table
UINT RootSignature::AddDescriptorTable(const D3D12_DESCRIPTOR_RANGE_TYPE type, const UINT count, 
									   const UINT baseRegister, const UINT space, const std::string& name)
{
    CD3DX12_DESCRIPTOR_RANGE1 storage;
    storage.Init(type, count, baseRegister, space);
    m_ranges.push_back(storage);

    CD3DX12_ROOT_PARAMETER1 param;
    param.InitAsDescriptorTable(1, nullptr);
    m_params.push_back(param);

    UINT index = static_cast<UINT>(m_params.size() - 1);
    m_paramNames[name] = index;
    return index;
}

// Returns the root parameter index of the added SRV
UINT RootSignature::AddRootSRV(const UINT reg, const UINT space, const std::string& name)
{
    CD3DX12_ROOT_PARAMETER1 param;
    param.InitAsShaderResourceView(reg, space);
    m_params.push_back(param);
	UINT index = static_cast<UINT>(m_params.size() - 1);
	m_paramNames[name] = index;
    return index;
}

// Returns the root parameter index of the added UAV
UINT RootSignature::AddRootUAV(const UINT reg, const UINT space, const std::string& name)
{
    CD3DX12_ROOT_PARAMETER1 param;
    param.InitAsUnorderedAccessView(reg, space);
    m_params.push_back(param);
    UINT index = static_cast<UINT>(m_params.size() - 1);
    m_paramNames[name] = index;
    return index;
}

// Returns the root parameter index of the added CBV
UINT RootSignature::AddRootCBV(const UINT reg, const UINT space, const std::string& name)
{
    CD3DX12_ROOT_PARAMETER1 param;
    param.InitAsConstantBufferView(reg, space);
    m_params.push_back(param);
    UINT index = static_cast<UINT>(m_params.size() - 1);
    m_paramNames[name] = index;
    return index;
}

// Returns the root parameter index of the added constants
UINT RootSignature::AddRootConstants(const UINT num32BitValues, const UINT reg, const UINT space, const std::string& name)
{
    CD3DX12_ROOT_PARAMETER1 param;
    param.InitAsConstants(num32BitValues, reg, space);
    m_params.push_back(param);
    UINT index = static_cast<UINT>(m_params.size() - 1);
    m_paramNames[name] = index;
    return index;
}

void RootSignature::AddStaticSampler(const UINT reg, const D3D12_FILTER filter,
									 const D3D12_TEXTURE_ADDRESS_MODE addressMode, const UINT space)
{
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = filter;
    sampler.AddressU = addressMode;
    sampler.AddressV = addressMode;
    sampler.AddressW = addressMode;
    sampler.MipLODBias = 0.0f;
    sampler.MaxAnisotropy = 16;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = reg;
    sampler.RegisterSpace = space;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    m_samplers.push_back(sampler);
}

void RootSignature::Build(ID3D12Device* device, const wchar_t* name)
{
    // Fix up descriptor table pointers. ranges may have moved due to vector reallocation
    UINT rangeIndex = 0;
    for (auto& param : m_params)
    {
        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            param.DescriptorTable.pDescriptorRanges = &m_ranges[rangeIndex];
            param.DescriptorTable.NumDescriptorRanges = 1;
            rangeIndex++;
        }
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
    desc.Init_1_1(
        static_cast<UINT>(m_params.size()), m_params.empty() ? nullptr : m_params.data(),
        static_cast<UINT>(m_samplers.size()), m_samplers.empty() ? nullptr : m_samplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED);

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_1, &sigBlob, &errorBlob);

    if (FAILED(hr))
    {
        if (errorBlob)
        {
	        std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << "\n";
        }
        ThrowIfFailed(hr);
    }

    ThrowIfFailed(device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

    m_rootSignature->SetName(name);
}

void RootSignature::SetDescriptorTable(ID3D12GraphicsCommandList* commandList, const D3D12_GPU_DESCRIPTOR_HANDLE handle, const std::string& name) const
{
    UINT index = GetParameterIndex(name);
	commandList->SetComputeRootDescriptorTable(index, handle);
}

void RootSignature::SetRootSRV(ID3D12GraphicsCommandList* commandList, const D3D12_GPU_VIRTUAL_ADDRESS address, const std::string& name) const
{
    UINT index = GetParameterIndex(name);
    commandList->SetComputeRootShaderResourceView(index, address);
}

void RootSignature::SetRootUAV(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS address, const std::string& name) const
{
    UINT index = GetParameterIndex(name);
    commandList->SetComputeRootUnorderedAccessView(index, address);
}

void RootSignature::SetRootCBV(ID3D12GraphicsCommandList* commandList, D3D12_GPU_VIRTUAL_ADDRESS address, const std::string& name) const
{
    UINT index = GetParameterIndex(name);
    commandList->SetComputeRootConstantBufferView(index, address);
}

void RootSignature::SetRootConstants(ID3D12GraphicsCommandList* commandList, const void* data, const UINT num32BitValues, const std::string& name) const
{
    UINT index = GetParameterIndex(name);
    commandList->SetComputeRoot32BitConstants(index, num32BitValues, data, 0);
}