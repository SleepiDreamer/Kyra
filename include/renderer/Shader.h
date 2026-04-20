#pragma once
#include <d3d12.h>
#include <filesystem>
#include <string>
#include <vector>

#include "ShaderCompiler.h"

class ShaderCompiler;

class Shader
{
public:
    Shader(ShaderCompiler& compiler, std::string filePath, 
		   const std::vector<std::string>& entryPoints, bool isRaytracing);
    ~Shader();

    ShaderCompiler::CompilationResult Load();

    [[nodiscard]] D3D12_SHADER_BYTECODE GetBytecode() const;
    [[nodiscard]] const std::vector<uint8_t>& GetBlob() const { return m_blob; }
	[[nodiscard]] std::string GetPath() const { return m_filePath; }
	[[nodiscard]] std::vector<std::string> GetEntryPoints() const { return m_entryPoints; }
	[[nodiscard]] bool IsRaytracing() const { return m_isRaytracing; }
    [[nodiscard]] bool IsValid() const { return !m_blob.empty(); }

private:
	friend class ShaderCompiler;

    ShaderCompiler& m_compiler;
    std::string m_filePath;
    std::vector<std::string> m_entryPoints;
    std::vector<uint8_t> m_blob;
    bool m_isRaytracing;

    std::filesystem::file_time_type m_lastCompileTime;
};