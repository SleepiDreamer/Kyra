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
class StructuredBuffer;
class TypedBuffer;

class Mesh
{
public:
    Mesh();
    ~Mesh();

    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void Upload(RenderContext& context, const std::vector<Vertex>& vertices,
                const std::vector<uint32_t>& indices, const std::string& name);

    [[nodiscard]] D3D12_RAYTRACING_GEOMETRY_DESC GetGeometryDesc(const UINT64 transformBase, const uint32_t idx) const;
    [[nodiscard]] ID3D12Resource* GetVertexBuffer() const;
    [[nodiscard]] ID3D12Resource* GetIndexBuffer() const;
    [[nodiscard]] ID3D12Resource* GetPowerBuffer() const;
    [[nodiscard]] uint32_t GetVertexCount() const { return m_vertexCount; }
    [[nodiscard]] uint32_t GetIndexCount() const { return m_indexCount; }
    [[nodiscard]] D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
    [[nodiscard]] D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;
    [[nodiscard]] Descriptor GetVertexSRV() const;
    [[nodiscard]] Descriptor GetIndexSRV() const;
    [[nodiscard]] DirectX::XMFLOAT4X4 GetTransform() const { return m_transform; }

    bool m_isAlphaTested = false;
    int32_t m_materialIndex = -1;
    int32_t m_localMaterialIndex = -1;
    DirectX::XMFLOAT4X4 m_transform;

private:
	std::unique_ptr<StructuredBuffer> m_vertexBuffer = nullptr;
	std::unique_ptr<TypedBuffer> m_indexBuffer = nullptr;
	std::unique_ptr<TypedBuffer> m_powerBuffer = nullptr;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
};