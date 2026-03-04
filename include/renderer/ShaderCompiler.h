#pragma once
#include "CommonDX.h"

#include <slang.h>
#include <slang-com-ptr.h>
#include <string>
#include <vector>

class ShaderCompiler
{
public:
    ShaderCompiler();
    ~ShaderCompiler();

	struct CompilationResult
    {
        std::vector<uint8_t> blob;
        bool success = false;
        std::string errorLog;
    };

    CompilationResult Compile(const std::string& filePath, const std::vector<std::string>& entryPoints = {}, 
							  const std::vector<std::pair<std::string, std::string>>& defines = {}, 
							  bool isRaytracing = true) const;
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;

private:
    static void diagnoseIfNeeded(slang::IBlob* diagnosticsBlob, CompilationResult& result);

    Slang::ComPtr<slang::IGlobalSession> m_globalSession;
};