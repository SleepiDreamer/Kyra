#pragma once
#include "GPUBuffer.h"
#include "DescriptorHeap.h"
#include "StructsDX.h"

#include <DirectXMath.h>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT4 tangent;
};

class CommandQueue;
class GPUAllocator;
class UploadContext;
class BLAS;

class Mesh
{
public:
    Mesh();
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Upload(const RenderContext& context, const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices, const std::string& name);

    void BuildBLAS(RenderContext& context, ID3D12GraphicsCommandList4* commandList, bool isAlphaTested);

    [[nodiscard]] D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc(bool isAlphaTested) const;
    [[nodiscard]] ID3D12Resource* GetVertexBuffer() const { return m_vertexBuffer.resource; }
    [[nodiscard]] ID3D12Resource* GetIndexBuffer() const { return m_indexBuffer.resource; }
    [[nodiscard]] uint32_t GetVertexCount() const { return m_vertexCount; }
    [[nodiscard]] uint32_t GetIndexCount() const { return m_indexCount; }
    [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
    [[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;
	[[nodiscard]] DescriptorHeap::Allocation GetVertexSRV() const { return m_vertexSRV; }
	[[nodiscard]] DescriptorHeap::Allocation GetIndexSRV() const { return m_indexSRV; }
	[[nodiscard]] D3D12_RAYTRACING_INSTANCE_DESC GetInstanceDesc(UINT instanceId) const;

    int32_t m_materialIndex = -1;
    DirectX::XMFLOAT4X4 m_transform;

private:
    GPUBuffer m_vertexBuffer;
    GPUBuffer m_indexBuffer;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
    std::unique_ptr<BLAS> m_blas = nullptr;

    DescriptorHeap::Allocation m_vertexSRV;
    DescriptorHeap::Allocation m_indexSRV;
};