#include "Shader.h"
#include "ShaderCompiler.h"
#include "HelpersDX.h"
#include "Log.h"

#include <iostream>

Shader::Shader(ShaderCompiler& compiler, std::string filePath, const std::vector<std::string>& entryPoints, const bool isRaytracing)
	: m_compiler(compiler), m_filePath(std::move(filePath)), m_entryPoints(entryPoints), m_isRaytracing(isRaytracing)
{
    if (!std::filesystem::exists(m_filePath))
    {
		Log::Critical("Shader file not found: {}", m_filePath);
    }

    auto result = Load();

    if (!IsValid())
    {
		Log::Error("Failed to compile shader: {} \n{}", m_filePath, result.errorLog);
    }

    m_lastCompileTime = std::filesystem::file_time_type::clock::now();
}

Shader::~Shader() = default;

ShaderCompiler::CompilationResult Shader::Load()
{
    auto result = m_compiler.Compile(m_filePath, m_entryPoints, m_isRaytracing);

	m_lastCompileTime = std::filesystem::file_time_type::clock::now();

    if (!result.success)
    {
        return result;
    }

    m_blob = std::move(result.blob);
    return result;
}

D3D12_SHADER_BYTECODE Shader::GetBytecode() const
{
    return { m_blob.data(), m_blob.size() };
}