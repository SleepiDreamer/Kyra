#include "Mesh.h"
#include "CommandQueue.h"
#include "GPUAllocator.h"
#include "UploadContext.h"
#include "BLAS.h"

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

void Mesh::Upload(const RenderContext& context, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices, const std::string& name)
{
    m_vertexCount = static_cast<uint32_t>(vertices.size());
    m_indexCount = static_cast<uint32_t>(indices.size());

    uint64_t verticesSize = vertices.size() * sizeof(Vertex);
    uint64_t indicesSize = indices.size() * sizeof(uint32_t);

    m_vertexBuffer = context.allocator->CreateBuffer(
        verticesSize, D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT,
        (name + "_VB").c_str());

    m_indexBuffer = context.allocator->CreateBuffer(
        indicesSize, D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_TYPE_DEFAULT,
        (name + "_IB").c_str());

    context.uploadContext->Upload(m_vertexBuffer, vertices.data(), verticesSize);
    context.uploadContext->Upload(m_indexBuffer, indices.data(), indicesSize);

    m_vertexSRV = context.descriptorHeap->Allocate();
    m_indexSRV = context.descriptorHeap->Allocate();

    D3D12_SHADER_RESOURCE_VIEW_DESC vertexSrvDesc{};
    vertexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    vertexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    vertexSrvDesc.Buffer.NumElements = m_vertexCount;
    vertexSrvDesc.Buffer.StructureByteStride = sizeof(Vertex);
    vertexSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
    context.device->CreateShaderResourceView(m_vertexBuffer.resource, &vertexSrvDesc, m_vertexSRV.cpuHandle);

    D3D12_SHADER_RESOURCE_VIEW_DESC indexSrvDesc{};
    indexSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    indexSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    indexSrvDesc.Buffer.NumElements = m_indexCount;
    indexSrvDesc.Format = DXGI_FORMAT_R32_UINT;
    context.device->CreateShaderResourceView(m_indexBuffer.resource, &indexSrvDesc, m_indexSRV.cpuHandle);
}

D3D12_RAYTRACING_GEOMETRY_DESC Mesh::GetGeometryDesc(const bool isAlphaTested) const
{
    D3D12_RAYTRACING_GEOMETRY_DESC desc = {};
    desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    desc.Flags = isAlphaTested == 0 ? D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE : D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;
    desc.Triangles.VertexBuffer.StartAddress = m_vertexBuffer.resource->GetGPUVirtualAddress();
    desc.Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
    desc.Triangles.VertexCount = m_vertexCount;
    desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    desc.Triangles.IndexBuffer = m_indexBuffer.resource->GetGPUVirtualAddress();
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

D3D12_VERTEX_BUFFER_VIEW Mesh::GetVertexBufferView() const
{
    return {
        .BufferLocation = m_vertexBuffer.resource->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(m_vertexBuffer.size),
        .StrideInBytes = sizeof(Vertex)
    };
}

D3D12_INDEX_BUFFER_VIEW Mesh::GetIndexBufferView() const
{
    return {
        .BufferLocation = m_indexBuffer.resource->GetGPUVirtualAddress(),
        .SizeInBytes = static_cast<UINT>(m_indexBuffer.size),
        .Format = DXGI_FORMAT_R32_UINT
    };
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
