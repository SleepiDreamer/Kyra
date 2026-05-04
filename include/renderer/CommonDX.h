#pragma once
#include "HelpersDX.h"

#include <d3d12.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

class GPUAllocator;
class CommandQueue;
class DescriptorHeap;
class UploadContext;
struct ID3D12Device10;

inline constexpr uint8_t NUM_FRAMES_IN_FLIGHT = 3;

constexpr DXGI_SAMPLE_DESC DEFAULT_SAMPLER = { 1, 0 };
constexpr D3D12_RESOURCE_DESC BUFFER_RESOURCE = {
    D3D12_RESOURCE_DIMENSION_BUFFER,
    0,
    0,
    1,
    1,
    1,
    DXGI_FORMAT_UNKNOWN,
    DEFAULT_SAMPLER,
    D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
    D3D12_RESOURCE_FLAG_NONE
};
constexpr D3D12_RESOURCE_DESC TEXTURE_RESOURCE = {
    D3D12_RESOURCE_DIMENSION_TEXTURE2D,
    0,
    0,
    1,
    1,
    1,
    DXGI_FORMAT_UNKNOWN,
    DEFAULT_SAMPLER,
    D3D12_TEXTURE_LAYOUT_UNKNOWN,
    D3D12_RESOURCE_FLAG_NONE
};
constexpr D3D12_STATIC_SAMPLER_DESC BLOOM_SAMPLER = {
    D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    0.0f,
    0,
    D3D12_COMPARISON_FUNC_NEVER,
    D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK,
    0.0f,
    D3D12_FLOAT32_MAX,
    0,
    0,
    D3D12_SHADER_VISIBILITY_ALL
};

struct RenderContext
{
	ID3D12Device10* device = nullptr;
	GPUAllocator* allocator = nullptr;
    CommandQueue* commandQueue = nullptr;
    DescriptorHeap* descriptorHeap = nullptr;
    DescriptorHeap* samplerHeap = nullptr;
    UploadContext* uploadContext = nullptr;
};