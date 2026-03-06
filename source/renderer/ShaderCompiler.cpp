#include "ShaderCompiler.h"
#include "CommonDX.h"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <array>

ShaderCompiler::ShaderCompiler()
{
    slang::createGlobalSession(m_globalSession.writeRef());
    if (!m_globalSession)
    {
        ThrowError("Failed to create Slang global session");
    };
}

ShaderCompiler::~ShaderCompiler() = default;

void ShaderCompiler::diagnoseIfNeeded(slang::IBlob* diagnosticsBlob, CompilationResult& result)
{
    if (diagnosticsBlob != nullptr)
    {
	    std::cerr << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << "\n";
		result.errorLog += static_cast<const char*>(diagnosticsBlob->getBufferPointer());
    }
}

ShaderCompiler::CompilationResult ShaderCompiler::Compile(const std::string& filePath, const std::vector<std::string>& entryPoints, bool isRaytracing) const
{
    CompilationResult result;

    std::vector<std::string> epNames = entryPoints.empty()
        ? std::vector<std::string>{"RayGen", "ClosestHit", "Miss", "ShadowMiss", "AnyHit"}
		: entryPoints;

    bool wholeProgram = isRaytracing;

    slang::SessionDesc sessionDesc{};
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = m_globalSession->findProfile("sm_6_6");

    std::array<slang::CompilerOptionEntry, 1> options = { {{
        slang::CompilerOptionName::GenerateWholeProgram,
        {.kind = slang::CompilerOptionValueKind::Int,
          .intValue0 = wholeProgram ? 1 : 0, .intValue1 = 0,
          .stringValue0 = nullptr, .stringValue1 = nullptr }
    }} };
    sessionDesc.compilerOptionEntries = options.data();
    sessionDesc.compilerOptionEntryCount = static_cast<uint32_t>(options.size());
    sessionDesc.targets = &targetDesc;
    sessionDesc.targetCount = 1;

    std::string directory = std::filesystem::path(filePath).parent_path().string();
    const char* searchPaths[] = { directory.c_str() };
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = 1;

    Slang::ComPtr<slang::ISession> session;
    m_globalSession->createSession(sessionDesc, session.writeRef());

    Slang::ComPtr<slang::IBlob> diagnostics;

    slang::IModule* module = session->loadModule(filePath.c_str(), diagnostics.writeRef());
    diagnoseIfNeeded(diagnostics.get(), result);
    if (!module) { return result; }

    std::vector<slang::IComponentType*> components;
    components.push_back(module);

    std::vector<Slang::ComPtr<slang::IEntryPoint>> eps;
    for (const auto& name : epNames)
    {
        Slang::ComPtr<slang::IEntryPoint> ep;
        module->findEntryPointByName(name.c_str(), ep.writeRef());
        if (!ep)
        {
            result.errorLog = std::string("Entry point not found: ") + name;
            return result;
        }
        eps.push_back(ep);
        components.push_back(ep.get());
    }

    Slang::ComPtr<slang::IComponentType> composed;
    session->createCompositeComponentType(components.data(),
        static_cast<SlangInt>(components.size()),
        composed.writeRef(), diagnostics.writeRef());
    diagnoseIfNeeded(diagnostics.get(), result);

    Slang::ComPtr<slang::IComponentType> linked;
    composed->link(linked.writeRef(), diagnostics.writeRef());
    diagnoseIfNeeded(diagnostics.get(), result);

    Slang::ComPtr<slang::IBlob> code;
    if (wholeProgram)
    {
        linked->getTargetCode(0, code.writeRef(), diagnostics.writeRef());
    }
    else
    {
        linked->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
    }
    diagnoseIfNeeded(diagnostics.get(), result);

    if (!code) { result.errorLog = "Failed to generate code"; return result; }

    result.blob.resize(code->getBufferSize());
    memcpy(result.blob.data(), code->getBufferPointer(), code->getBufferSize());
    result.success = true;
    return result;
}