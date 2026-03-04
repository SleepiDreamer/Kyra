#include "Mesh.h"
#include "CommandQueue.h"
#include "GPUAllocator.h"
#include "UploadContext.h"
#include "BLAS.h"
#include "StructuredBuffer.h"
#include "TypedBuffer.h"

Mesh::Mesh() = default;

Mesh::~Mesh() = default;

Mesh::Mesh(Mesh&& other) noexcept
    : m_materialIndex(other.m_materialIndex), m_transform(other.m_transform), m_vertexBuffer(std::move(other.m_vertexBuffer))
    , m_indexBuffer(std::move(other.m_indexBuffer)), m_vertexCount(other.m_vertexCount), m_indexCount(other.m_indexCount)
	, m_blas(std::move(other.m_blas))
{
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        m_vertexBuffer = std::move(other.m_vertexBuffer);
        m_indexBuffer = std::move(other.m_indexBuffer);
        m_vertexCount = other.m_vertexCount;
        m_indexCount = other.m_indexCount;
        m_materialIndex = other.m_materialIndex;
        m_transform = other.m_transform;
		m_blas = std::move(other.m_blas);
    }
    return *this;
}

void Mesh::Upload(RenderContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name)
{
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());

    uint64_t verticesSize = vertices.size() * sizeof(Vertex);
    uint64_t indicesSize = indices.size() * sizeof(uint32_t);

    m_vertexBuffer = std::make_unique<StructuredBuffer>(
        context, m_vertexCount, sizeof(Vertex), D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT, false, (name + "_VB").c_str());

    m_indexBuffer = std::make_unique<TypedBuffer>(
        context, m_indexCount, DXGI_FORMAT_R32_UINT, D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT, (name + "_IB").c_str());

    context.uploadContext->Upload(m_vertexBuffer->GetBuffer(), vertices.data(), verticesSize);
    context.uploadContext->Upload(m_indexBuffer->GetBuffer(), indices.data(), indicesSize);
}

D3D12_RAYTRACING_GEOMETRY_DESC Mesh::GetGeometryDesc(const bool isAlphaTested) const
{
    D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
    desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    desc.Flags = isAlphaTested == 0 ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    desc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer->GetResource()->GetGPUVirtualAddress();
    desc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    desc.Triangles.VertexCount = m_vertexCount;
    desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    desc.Triangles.IndexBuffer = m_indexBuffer->GetResource()->GetGPUVirtualAddress();
    desc.Triangles.IndexCount = m_indexCount;
    desc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    return desc;
}

void Mesh::BuildBLAS(RenderContext& context, ID3D12GraphicsCommandList4* commandList, const bool isAlphaTested)
{
    m_blas = std::make_unique<BLAS>(context);
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> geometries = { GetGeometryDesc(isAlphaTested) };
    m_blas->Build(context.device, commandList, geometries);
}

ID3D12Resource* Mesh::GetVertexBuffer() const
{
    return m_vertexBuffer->GetResource();
}

ID3D12Resource* Mesh::GetIndexBuffer() const
{
    return m_indexBuffer->GetResource();
}

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() const
{
    return {
        .BufferLocation = m_vertexBuffer->GetResource()->GetGPUVirtualAddress(),
        .SizeInBytes = m_vertexBuffer->GetSize(),
        .StrideInBytes = sizeof(Vertex)
    };
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const
{
    return {
        .BufferLocation = m_indexBuffer->GetResource()->GetGPUVirtualAddress(),
        .SizeInBytes = m_indexBuffer->GetSize(),
        .Format = DXGI_FORMAT_R32_UINT
    };
}

Descriptor Mesh::GetVertexSRV() const
{
    return m_vertexBuffer->GetSRV();
}

Descriptor Mesh::GetIndexSRV() const
{
    return m_indexBuffer->GetSRV();
}

D3D12_RAYTRACING_INSTANCE_DESC Mesh::GetInstanceDesc(const UINT instanceId) const
{
    D3D12_RAYTRACING_INSTANCE_DESC desc = {};
	desc.AccelerationStructure = m_blas->GetResult()->GetGPUVirtualAddress();
    desc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
    desc.InstanceMask = 0xFF;
    desc.InstanceContributionToHitGroupIndex = instanceId;
    desc.InstanceID = instanceId;

    DirectX::XMMATRIX m = XMLoadFloat4x4(&m_transform);
    DirectX::XMMATRIX transposed = XMMatrixTranspose(m);
    DirectX::XMFLOAT4X4 t;
    DirectX::XMStoreFloat4x4(&t, transposed);

    memcpy(desc.Transform, &t, sizeof(desc.Transform));

    return desc;
}
