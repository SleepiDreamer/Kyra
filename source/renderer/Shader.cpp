#include "Shader.h"
#include "ShaderCompiler.h"
#include "HelpersDX.h"

#include <iostream>

Shader::Shader(ShaderCompiler& compiler, std::string filePath, const std::vector<std::string>& entryPoints,
			   const std::vector<std::pair<std::string, std::string>>& defines, bool isRaytracing)
	: m_compiler(compiler), m_filePath(std::move(filePath)), m_entryPoints(entryPoints), m_defines(defines), m_isRaytracing(isRaytracing)
{
    if (std::filesystem::exists(m_filePath))
    {
        m_lastWriteTime = std::filesystem::file_time_type::clock::now();
    }

    Reload();
}

Shader::~Shader() = default;

bool Shader::NeedsReload() const
{
    try
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator("shaders/")) {
            if (entry.is_regular_file() && entry.last_write_time() > m_lastWriteTime) {
                auto ext = entry.path().extension();
                if (ext == ".slang" )
                {
					return true;
                }
            }
        }
        return false;
    }
    catch (const std::filesystem::filesystem_error&)
    {
        return false;
    }
}

bool Shader::Reload()
{
    auto result = m_compiler.Compile(m_filePath, m_entryPoints, m_defines, m_isRaytracing);

    if (!result.success)
    {
        std::cerr << "Failed to compile shader: " << m_filePath << "\n" << result.errorLog << "\n";
        m_lastWriteTime = std::filesystem::file_time_type::clock::now();
		m_lastCompileError = result.errorLog;
		m_lastCompileFailed = true;
        return false;
    }

    m_blob = std::move(result.blob);
    m_lastWriteTime = std::filesystem::file_time_type::clock::now();
	m_lastCompileFailed = false;
    return true;
}

D3D12_SHADER_BYTECODE Shader::GetBytecode() const
{
    return { m_blob.data(), m_blob.size() };
}