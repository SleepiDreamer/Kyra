#pragma once
#include "CommonDX.h"
#include <D3D12MemAlloc.h>

struct GPUBuffer
{
    D3D12MA::Allocation* allocation = nullptr;
    ID3D12Resource* resource = nullptr;
	D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    uint64_t size = 0;

    GPUBuffer() = default;
    ~GPUBuffer()
    {
        Reset();
    }

    // Move only
    GPUBuffer(const GPUBuffer&) = delete;
    GPUBuffer& operator=(const GPUBuffer&) = delete;

    GPUBuffer(GPUBuffer&& other) noexcept
        : allocation(other.allocation), resource(other.resource), size(other.size)
    {
        other.allocation = nullptr;
        other.resource = nullptr;
        other.size = 0;
    }

    GPUBuffer& operator=(GPUBuffer&& other) noexcept
    {
        if (this != &other)
        {
            Reset();
            allocation = other.allocation;
            resource = other.resource;
            size = other.size;
            other.allocation = nullptr;
            other.resource = nullptr;
            other.size = 0;
        }
        return *this;
    }

    void Reset()
    {
        if (allocation)
        {
            allocation->Release();
            allocation = nullptr;
            resource->Release();
            resource = nullptr;
            size = 0;
        }
    }

    void Transition(ID3D12GraphicsCommandList* commandList, const D3D12_RESOURCE_STATES newState)
    {
        if (newState != state)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.StateBefore = state;
            barrier.Transition.StateAfter = newState;
            commandList->ResourceBarrier(1, &barrier);
            state = newState;
        }
	}

    explicit operator bool() const { return allocation != nullptr; }
};