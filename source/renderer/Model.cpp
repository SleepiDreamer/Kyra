#include "Model.h"
#include "MikkT.h"
#include "GPUAllocator.h"
#include "CommandQueue.h"
#include "StructsDX.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <stdexcept>
#include <vector>

using namespace DirectX;

Model::Model(RenderContext& context, ID3D12GraphicsCommandList4* commandList, const std::filesystem::path& path)
    : m_context(context), m_name(path.stem().string())
{
    LoadGLTF(commandList, path);
}

Model::~Model() = default;

void Model::LoadGLTF(ID3D12GraphicsCommandList4* commandList, const std::filesystem::path& path)
{
    fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual | fastgltf::Extensions::KHR_materials_transmission);

    auto data = fastgltf::GltfDataBuffer::FromPath(path);
    if (data.error() != fastgltf::Error::None)
    {
        ThrowError("Failed to load glTF file: " + path.string());
    }

    constexpr auto options =
        fastgltf::Options::LoadExternalBuffers |
        fastgltf::Options::LoadExternalImages;

    auto asset = parser.loadGltf(data.get(), path.parent_path(), options);
    if (asset.error() != fastgltf::Error::None)
    {
        ThrowError("Failed to parse glTF: " + path.string());
    }

    size_t sceneIndex = asset->defaultScene.value_or(0);
    const auto& scene = asset->scenes[sceneIndex];

    for (size_t nodeIndex : scene.nodeIndices)
    {
        TraverseNode(commandList, asset.get(), nodeIndex, XMMatrixIdentity());
    }

    LoadMaterials(asset.get());
}

void Model::TraverseNode(ID3D12GraphicsCommandList4* commandList, const fastgltf::Asset& asset, const size_t nodeIndex, const XMMATRIX& parentTransform)
{
    const auto& node = asset.nodes[nodeIndex];

    XMMATRIX localTransform = GetNodeTransform(node);
    XMMATRIX worldTransform = XMMatrixMultiply(localTransform, parentTransform);

    // Node has mesh
    if (node.meshIndex.has_value())
    {
        const auto& mesh = asset.meshes[node.meshIndex.value()];
        LoadMesh(commandList, asset, mesh, worldTransform);
    }

    // TODO: Handle lights when node.lightIndex.has_value()

    for (size_t childIndex : node.children)
    {
        TraverseNode(commandList, asset, childIndex, worldTransform);
    }
}

XMMATRIX Model::GetNodeTransform(const fastgltf::Node& node)
{
    return std::visit(fastgltf::visitor{
        [](const fastgltf::TRS& trs) -> XMMATRIX
        {
            XMVECTOR translation = XMVectorSet(trs.translation[0], trs.translation[1], trs.translation[2], 0.0f);
            XMVECTOR rotation = XMVectorSet(trs.rotation[0], trs.rotation[1], trs.rotation[2], trs.rotation[3]);
            XMVECTOR scale = XMVectorSet(trs.scale[0], trs.scale[1], trs.scale[2], 0.0f);

            return XMMatrixScalingFromVector(scale) *
                   XMMatrixRotationQuaternion(rotation) *
                   XMMatrixTranslationFromVector(translation);
        },
		[](const fastgltf::math::fmat4x4& matrix) -> XMMATRIX
		{
		    return XMMATRIX(
		        matrix[0][0], matrix[0][1], matrix[0][2], matrix[0][3],
		        matrix[1][0], matrix[1][1], matrix[1][2], matrix[1][3],
		        matrix[2][0], matrix[2][1], matrix[2][2], matrix[2][3],
		        matrix[3][0], matrix[3][1], matrix[3][2], matrix[3][3]
		    );
		}
        }, node.transform);
}

void Model::LoadMesh(ID3D12GraphicsCommandList4* commandList, const fastgltf::Asset& asset, const fastgltf::Mesh& gltfMesh, const XMMATRIX& transform)
{
    for (const auto& primitive : gltfMesh.primitives)
    {
        if (primitive.type != fastgltf::PrimitiveType::Triangles)
        {
	        continue;
        }

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        auto posIt = primitive.findAttribute("POSITION");
        if (posIt == primitive.attributes.end())
        {
	        continue;
        }

        const auto& posAccessor = asset.accessors[posIt->accessorIndex];
        vertices.resize(posAccessor.count);

        // Load positions
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, posAccessor,
            [&](const fastgltf::math::fvec3& pos, size_t idx)
            {
                vertices[idx].position = XMFLOAT3(pos.x(), pos.y(), pos.z());
            }
        );

        // Load normals
        auto normIt = primitive.findAttribute("NORMAL");
        if (normIt != primitive.attributes.end())
        {
            const auto& normAccessor = asset.accessors[normIt->accessorIndex];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(asset, normAccessor,
                [&](const fastgltf::math::fvec3& norm, size_t idx)
                {
                    vertices[idx].normal = XMFLOAT3(norm.x(), norm.y(), norm.z());
                }
            );
        }

        // Load texture coordinates
        auto texIt = primitive.findAttribute("TEXCOORD_0");
        if (texIt != primitive.attributes.end())
        {
            const auto& texAccessor = asset.accessors[texIt->accessorIndex];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(asset, texAccessor,
                [&](const fastgltf::math::fvec2& uv, size_t idx)
                {
                    vertices[idx].texCoord = XMFLOAT2(uv.x(), uv.y());
                }
            );
        }

        // Load indices
        if (primitive.indicesAccessor.has_value())
        {
            const auto& indexAccessor = asset.accessors[primitive.indicesAccessor.value()];
            indices.resize(indexAccessor.count);

            fastgltf::iterateAccessorWithIndex<uint32_t>(asset, indexAccessor,
                [&](uint32_t index, size_t idx)
                {
                    indices[idx] = index;
                });
        }
        else
        {
            indices.resize(vertices.size());
            for (size_t i = 0; i < vertices.size(); ++i)
                indices[i] = static_cast<uint32_t>(i);
        }

        // Load tangents
        auto tanIt = primitive.findAttribute("TANGENT");
        if (tanIt != primitive.attributes.end())
        {
            const auto& tanAccessor = asset.accessors[tanIt->accessorIndex];
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(asset, tanAccessor,
                [&](const fastgltf::math::fvec4& tan, size_t idx)
                {
                    vertices[idx].tangent = XMFLOAT4(tan.x(), tan.y(), tan.z(), -tan.w());
                }
            );
        }
        else
        {
            MikkT::Generate(vertices, indices);
        }

		Mesh mesh;
        mesh.m_materialIndex = primitive.materialIndex.has_value()
            ? static_cast<int32_t>(primitive.materialIndex.value()) : -1;

        XMStoreFloat4x4(&mesh.m_transform, transform);

        std::string meshName = m_name + "_" + std::string(gltfMesh.name) +
            "_prim" + std::to_string(m_meshes.size());

        bool isAlphaTested = false;
        if (primitive.materialIndex.has_value())
        {
            const auto& mat = asset.materials[primitive.materialIndex.value()];
			isAlphaTested = mat.alphaMode != fastgltf::AlphaMode::Opaque;
        }
        mesh.Upload(m_context, vertices, indices, meshName);
        mesh.BuildBLAS(m_context, commandList, isAlphaTested);
        m_meshes.push_back(std::move(mesh));
    }
}

void Model::LoadMaterials(const fastgltf::Asset& asset)
{
    std::unordered_set<size_t> linearImages;
    for (auto& mat : asset.materials)
    {
        if (mat.normalTexture.has_value())
        {
            auto texIdx = mat.normalTexture->textureIndex;
            auto imgIdx = asset.textures[texIdx].imageIndex;
            if (imgIdx.has_value()) linearImages.insert(imgIdx.value());
        }
        if (mat.pbrData.metallicRoughnessTexture.has_value())
        {
            auto texIdx = mat.pbrData.metallicRoughnessTexture->textureIndex;
            auto imgIdx = asset.textures[texIdx].imageIndex;
            if (imgIdx.has_value()) linearImages.insert(imgIdx.value());
        }
    }
    int index = 0;
    for (auto& image : asset.images)
    {
        auto tex = Texture();

        int width, height, nrChannels;
        unsigned char* data = nullptr;

        std::visit(fastgltf::visitor{
            [&](const fastgltf::sources::URI& filePath) {
                assert(filePath.fileByteOffset == 0);
                assert(filePath.uri.isLocalPath());
                const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
                data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
            },
            [&](const fastgltf::sources::Array& vector) {
                data = stbi_load_from_memory(
                    reinterpret_cast<const stbi_uc*>(vector.bytes.data()),
                    static_cast<int>(vector.bytes.size()),
                    &width, &height, &nrChannels, 4);
            },
            [&](const fastgltf::sources::BufferView& view) {
                auto& bufferView = asset.bufferViews[view.bufferViewIndex];
                auto& buffer = asset.buffers[bufferView.bufferIndex];
                std::visit(fastgltf::visitor{
                    [&](const fastgltf::sources::Array& vector) {
                        data = stbi_load_from_memory(
                            reinterpret_cast<const stbi_uc*>(vector.bytes.data() + bufferView.byteOffset),
                            static_cast<int>(bufferView.byteLength),
                            &width, &height, &nrChannels, 4);
                    },
                    [&](const fastgltf::sources::ByteView& bv) {
                        data = stbi_load_from_memory(
                            reinterpret_cast<const stbi_uc*>(bv.bytes.data() + bufferView.byteOffset),
                            static_cast<int>(bufferView.byteLength),
                            &width, &height, &nrChannels, 4);
                    },
                    [&](const fastgltf::sources::Vector& vec) {
                        data = stbi_load_from_memory(
                            reinterpret_cast<const stbi_uc*>(vec.bytes.data() + bufferView.byteOffset),
                            static_cast<int>(bufferView.byteLength),
                            &width, &height, &nrChannels, 4);
                    },
                    [&](const fastgltf::sources::CustomBuffer& cb) {
                        std::cerr << "[Model] Unhandled buffer source: CustomBuffer for image: " << image.name << "\n";
                    },
                    [&](auto& arg) {
                        std::cerr << "[Model] Unhandled buffer source type: " << typeid(arg).name()
                                  << " for image: " << image.name << "\n";
                    }
                }, buffer.data);
            },
            [&](auto& arg) {
                std::cerr << "[Model] Unhandled image source type: " << typeid(arg).name()
                          << " for image: " << image.name << "\n";
            }
            }, image.data);

        if (data)
        {
            DXGI_FORMAT fmt = linearImages.count(index)
                ? DXGI_FORMAT_R8G8B8A8_UNORM
                : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            tex.Create(m_context, data, width, height, fmt, image.name.c_str());
            stbi_image_free(data);
        }
        else
        {
            std::cerr << "[Model] Failed to load image: " << image.name << "\n";
        }

        m_textures.push_back(std::move(tex));
        index++;
    }

    for (const auto& mat : asset.materials)
    {
        MaterialData matData{};

        if (mat.pbrData.baseColorTexture.has_value())
        {
            auto texIndex = asset.textures[mat.pbrData.baseColorTexture->textureIndex].imageIndex.value();
            matData.albedoIndex = m_textures[texIndex].GetDescriptorIndex();
        }

        if (mat.pbrData.metallicRoughnessTexture.has_value())
        {
            auto texIndex = asset.textures[mat.pbrData.metallicRoughnessTexture->textureIndex].imageIndex.value();
            matData.metallicRoughnessIndex = m_textures[texIndex].GetDescriptorIndex();
        }

        if (mat.normalTexture.has_value())
        {
            auto texIndex = asset.textures[mat.normalTexture->textureIndex].imageIndex.value();
            matData.normalIndex = m_textures[texIndex].GetDescriptorIndex();
        }

        if (mat.emissiveTexture.has_value())
        {
            auto texIndex = asset.textures[mat.emissiveTexture->textureIndex].imageIndex.value();
            matData.emissiveIndex = m_textures[texIndex].GetDescriptorIndex();
        }

        bool hasTransmission = false;
        if (mat.transmission)
        {
    		hasTransmission = true;
        }

        auto& aFactor = mat.pbrData.baseColorFactor;
        matData.albedoFactor = { aFactor[0], aFactor[1], aFactor[2], hasTransmission };
        matData.metallicFactor = mat.pbrData.metallicFactor;
        matData.roughnessFactor = mat.pbrData.roughnessFactor;
		auto& eFactor = mat.emissiveFactor;
		matData.emissiveFactor = { eFactor[0], eFactor[1], eFactor[2] };
        matData.isAlphaTested = mat.alphaMode != fastgltf::AlphaMode::Opaque ? 1 : 0;

        m_materials.push_back(matData);
    }
}