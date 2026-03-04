#pragma once
#include <d3d12.h>
#include <filesystem>
#include <string>
#include <vector>

class ShaderCompiler;

class Shader
{
public:
    Shader(ShaderCompiler& compiler, std::string filePath, const std::vector<std::string>& entryPoints, 
		   const std::vector<std::pair<std::string, std::string>>& defines, bool isRaytracing);
    ~Shader();

    bool NeedsReload() const;
    bool Reload();

    [[nodiscard]] D3D12_SHADER_BYTECODE GetBytecode() const;
    [[nodiscard]] const std::vector<uint8_t>& GetBlob() const { return m_blob; }
    [[nodiscard]] bool IsValid() const { return !m_blob.empty(); }
	[[nodiscard]] bool LastCompileFailed() const { return m_lastCompileFailed; }
	[[nodiscard]] std::string GetLastCompileError() const { return m_lastCompileError; }

private:
    ShaderCompiler& m_compiler;
    std::string m_filePath;
    std::vector<std::string> m_entryPoints;
	std::vector<std::pair<std::string, std::string>> m_defines;
    std::vector<uint8_t> m_blob;
    bool m_isRaytracing;

    std::filesystem::file_time_type m_lastWriteTime;
	bool m_lastCompileFailed = false;
	std::string m_lastCompileError;
};