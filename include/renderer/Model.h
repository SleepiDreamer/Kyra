#pragma once
#include "CommonDX.h"
#include "Mesh.h"
#include "Texture.h"

#include <DirectXMath.h>
#include <fastgltf/core.hpp>
#include <filesystem>

class GPUAllocator;
struct MaterialData;

class Model
{
public:
    Model(RenderContext& context, ID3D12GraphicsCommandList4* commandList, const std::filesystem::path& path);
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = default;
    Model& operator=(Model&&) = default;

    [[nodiscard]] const std::vector<Mesh>& GetMeshes() const { return m_meshes; }
    [[nodiscard]] const std::vector<Texture>& GetTextures() const { return m_textures; }
	[[nodiscard]] const std::vector<MaterialData>& GetMaterials() const { return m_materials; }
    [[nodiscard]] const std::string& GetName() const { return m_name; }

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
    std::vector<Texture> m_textures;
    std::vector<MaterialData> m_materials;
    std::vector<Descriptor> m_samplerDescriptors;
    std::string m_name;
};