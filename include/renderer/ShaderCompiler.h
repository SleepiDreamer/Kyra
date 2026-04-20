#pragma once
#include "CommonDX.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <string>
#include <vector>

class Shader;

class ShaderCompiler
{
public:
	ShaderCompiler(std::string directory);
    ~ShaderCompiler();

	struct CompilationResult
    {
        std::vector<uint8_t> blob;
        bool success = false;
        std::string errorLog;
    };

    CompilationResult Compile(const std::string& filePath, const std::vector<std::string>& entryPoints = {}, bool isRaytracing = true) const;
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

    [[nodiscard]] std::string GetReloadError() const;

	static std::vector<std::string> ParseImports(const std::string& filePath);

    void RegisterShaderReload(Shader* shader);
    void ShaderRecompileCallback(const std::function<void(Shader*)>& callback);
	bool CheckHotReload();

private:
    static void CheckDiagnostics(slang::IBlob* diagnosticsBlob, CompilationResult& result);

    Slang::ComPtr<slang::IGlobalSession> m_globalSession;
    HANDLE m_directoryWatch = INVALID_HANDLE_VALUE;

	std::string m_includeDirectory;
	std::vector<Shader*> m_registeredShaders;
    std::vector<std::function<void(Shader* shader)>> m_shaderCompileCallbacks;

    bool m_reloadFailed = false;
	std::vector<std::pair<Shader*, std::string>> m_reloadErrors;
};
