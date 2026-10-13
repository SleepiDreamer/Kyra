#pragma once
#include "CommonDX.h"
#include "Mesh.h"
#include "Texture.h"

#include <DirectXMath.h>
#include <fastgltf/core.hpp>
#include <filesystem>

class GPUAllocator;
class BLAS;
struct MaterialData;
struct Light;

class Model
{
public:
    Model(RenderContext& context, ID3D12GraphicsCommandList4* commandList, const std::filesystem::path& path);
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) noexcept;
    Model& operator=(Model&&) noexcept;

    [[nodiscard]] std::vector<Mesh>& GetMeshes() { return m_meshes; }
    [[nodiscard]] const std::vector<Texture>& GetTextures() const { return m_textures; }
	[[nodiscard]] const std::vector<MaterialData>& GetMaterials() const { return m_materials; }
	[[nodiscard]] const std::vector<Light>& GetLights() const { return m_lights; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }
    [[nodiscard]] D3D12_RAYTRACING_INSTANCE_DESC GetInstanceDesc(UINT instanceId, UINT hitGroupOffset) const;
    [[nodiscard]] DirectX::XMFLOAT4X4 GetTransform() const { return m_transform; }

    void BuildBLAS(ID3D12GraphicsCommandList4* commandList);

private:
    void LoadGLTF(ID3D12GraphicsCommandList4* commandList, const std::filesystem::path& path);

    void TraverseNode(ID3D12GraphicsCommandList4* commandList,
                      const fastgltf::Asset& asset, size_t nodeIndex, const DirectX::XMMATRIX& parentTransform);

    static DirectX::XMMATRIX GetNodeTransform(const fastgltf::Node& node);

    void LoadMesh(ID3D12GraphicsCommandList4* commandList,
                  const fastgltf::Asset& asset, const fastgltf::Mesh& gltfMesh, const DirectX::XMMATRIX& transform);
    void LoadMaterials(const fastgltf::Asset& asset);

    RenderContext& m_context;
    std::vector<Mesh> m_meshes;
    std::vector<Light> m_lights;
    std::vector<Texture> m_textures;
    std::vector<MaterialData> m_materials;
    std::vector<Descriptor> m_samplerDescriptors;
    std::unique_ptr<BLAS> m_blas = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_transformBuffer;

    std::string m_name;
    DirectX::XMFLOAT4X4 m_transform;
};