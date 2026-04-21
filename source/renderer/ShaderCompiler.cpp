#include "ShaderCompiler.h"
#include "Shader.h"
#include "CommonDX.h"
#include "Log.h"

#include <Windows.h>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <iostream>
#include <array>

ShaderCompiler::ShaderCompiler(std::string directory)
	: m_includeDirectory(std::move(directory))
{
    slang::createGlobalSession(m_globalSession.writeRef());
    if (!m_globalSession)
    {
        ThrowError("Failed to create Slang global session");
    }

    m_directoryWatch = FindFirstChangeNotificationA(m_includeDirectory.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);

    if (m_directoryWatch == INVALID_HANDLE_VALUE)
    {
		std::cerr << "Failed to set up directory watch for shader hot reload: " << GetLastError() << "\n";
    }
}

ShaderCompiler::~ShaderCompiler()
{
    if (m_directoryWatch != INVALID_HANDLE_VALUE)
    {
	    FindCloseChangeNotification(m_directoryWatch);
    }
}

std::string ShaderCompiler::GetReloadError() const
{
	if (m_reloadErrors.empty()) return "";
	return m_reloadErrors.front().second;
}

std::vector<std::string> ShaderCompiler::ParseImports(const std::string& filePath)
{
    std::vector<std::string> dependencies;
    std::string directory = std::filesystem::path(filePath).parent_path().string();

    std::ifstream file(filePath);
    std::string line;
    while (std::getline(file, line))
    {
        // Skip leading whitespace
        size_t i = line.find_first_not_of(" \t");
        if (i == std::string::npos) continue;

        // Check for "import "
        if (line.compare(i, 7, "import ") != 0) continue;
        i += 7;

        // Skip whitespace
        i = line.find_first_not_of(" \t", i);
        if (i == std::string::npos) continue;

        // Get module name
        size_t end = line.find(';', i);
        if (end == std::string::npos)
        {
	        end = line.length();
        }
        std::string moduleName = line.substr(i, end - i);

        // Remove trailing whitespace
        moduleName.erase(moduleName.find_last_not_of(" \t") + 1);
        if (moduleName.empty()) continue;

        // Remove quotes
        if (moduleName.front() == '"' && moduleName.back() == '"')
        {
	        moduleName = moduleName.substr(1, moduleName.size() - 2);
        }

        // Replace period with slash
        std::ranges::replace(moduleName, '.', '/');

        std::string resolved = directory + "/" += moduleName + ".slang";
        dependencies.push_back(std::filesystem::canonical(resolved).string());
    }

    return dependencies;
}

void ShaderCompiler::RegisterShaderReload(Shader* shader)
{
	m_registeredShaders.emplace_back(shader);
}

void ShaderCompiler::ShaderRecompileCallback(const std::function<void(Shader*)>& callback)
{
    m_shaderCompileCallbacks.emplace_back(callback);
}

bool ShaderCompiler::CheckHotReload()
{
    if (m_directoryWatch == INVALID_HANDLE_VALUE) return false;

    DWORD waitResult = WaitForSingleObject(m_directoryWatch, 0);

    if (waitResult != WAIT_OBJECT_0) return false;

    FindNextChangeNotification(m_directoryWatch);

    Sleep(100);

    bool reloaded = false;
	bool anyFailed = false;
    std::vector<Shader*> reloadedShaders;
    for (const auto shader : m_registeredShaders)
    {
        auto lastCompile = shader->m_lastCompileTime;
        bool needsRecompile = std::filesystem::last_write_time(shader->GetPath()) > lastCompile;

        if (!needsRecompile)
        {
            for (const auto& dep : ParseImports(shader->GetPath()))
            {
                if (std::filesystem::last_write_time(dep) > lastCompile)
                {
                    needsRecompile = true;
                    break;
                }
            }
        }

        if (!needsRecompile) continue;

        auto result = shader->Load();
		reloadedShaders.push_back(shader);
        if (result.success)
        {
			// remove any previous error for this shader
            std::erase_if(m_reloadErrors, [shader](const auto& pair)
            {
	            return pair.first == shader;
            });
        	reloaded = true;
        }
        else
        {
			anyFailed = true;
			m_reloadErrors.emplace_back(shader, result.errorLog);
        	Log::Warning("Failed to hot reload shader {}:", shader->GetPath());
            Log::Error("{}", result.errorLog);
        }
    }

    if (reloaded)
    {
        if (anyFailed)
        {
            Log::Warning("\nHot reload triggered for the following shaders. See above for details.");
		}
        else
        {
			Log::Success("\nHot reload triggered for the following shaders:");
        }

        for (const auto shader : reloadedShaders)
        {
			Log::Info(" - {}", shader->GetPath());
            if (shader->IsValid())
            {
	            for (auto& callback : m_shaderCompileCallbacks)
	            {
	                callback(shader);
	            }
            }
        }
    }

    return reloaded;
}

void ShaderCompiler::CheckDiagnostics(slang::IBlob* diagnosticsBlob, CompilationResult& result)
{
    if (diagnosticsBlob != nullptr)
    {
	    //std::cerr << static_cast<const char*>(diagnosticsBlob->getBufferPointer()) << "\n";
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
    CheckDiagnostics(diagnostics.get(), result);
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
    CheckDiagnostics(diagnostics.get(), result);

    Slang::ComPtr<slang::IComponentType> linked;
    composed->link(linked.writeRef(), diagnostics.writeRef());
    CheckDiagnostics(diagnostics.get(), result);

    Slang::ComPtr<slang::IBlob> code;
    if (wholeProgram)
    {
        linked->getTargetCode(0, code.writeRef(), diagnostics.writeRef());
    }
    else
    {
        linked->getEntryPointCode(0, 0, code.writeRef(), diagnostics.writeRef());
    }
    CheckDiagnostics(diagnostics.get(), result);

    if (!code) { result.errorLog = "Failed to generate code"; return result; }

    result.blob.resize(code->getBufferSize());
    memcpy(result.blob.data(), code->getBufferPointer(), code->getBufferSize());
    result.success = true;
    return result;
}