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
#include <algorithm>

ShaderCompiler::ShaderCompiler(std::string directory)
	: m_includeDirectory(std::move(directory))
{
    slang::createGlobalSession(m_globalSession.writeRef());
    if (!m_globalSession)
    {
        Log::Critical("Failed to create Slang global session");
    }

    m_directoryWatch = FindFirstChangeNotificationA(m_includeDirectory.c_str(), TRUE, FILE_NOTIFY_CHANGE_LAST_WRITE);

    if (m_directoryWatch == INVALID_HANDLE_VALUE)
    {
		Log::Error("Failed to set up directory watch for shader hot reload: {}", GetLastError());
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

std::vector<std::string> ShaderCompiler::ParseImports(const std::string& filePath, bool recurse)
{
	std::unordered_set<std::string> dependencies;
    ParseImportsRecursive(filePath, dependencies, true);
    return { dependencies.begin(), dependencies.end() };
}


void ShaderCompiler::ParseImportsRecursive(const std::string& filePath, std::unordered_set<std::string>& dependencies, const bool recurse)
{
	std::vector<std::string> newDependencies;
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

        std::vector<std::string> searchPaths = { directory, m_includeDirectory };
        for (const auto& searchPath : searchPaths)
        {
            std::string potentialPath = searchPath + "/" + moduleName + ".slang";
            if (std::filesystem::exists(potentialPath))
            {
                std::string resolved = std::filesystem::canonical(potentialPath).string();
                if (dependencies.insert(resolved).second)
                {
	                newDependencies.push_back(resolved);
                }

                break;
            }
        }
    }

    if (recurse)
    {
        for (const auto& dependency : newDependencies)
        {
            ParseImportsRecursive(dependency, dependencies, true);
        }
    }
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
            for (const auto& dep : ParseImports(shader->GetPath(), true))
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

static uint32_t CountScalars(slang::TypeReflection* type)
{
    if (type == nullptr) return 0;

    switch (type->getKind())
    {
    case slang::TypeReflection::Kind::Scalar:
        return 1;
    case slang::TypeReflection::Kind::Vector:
        return static_cast<uint32_t>(type->getElementCount()) * CountScalars(type->getElementType());
    case slang::TypeReflection::Kind::Matrix:
        return type->getRowCount() * type->getColumnCount();
    case slang::TypeReflection::Kind::Array:
        return static_cast<uint32_t>(type->getElementCount()) * CountScalars(type->getElementType());
    case slang::TypeReflection::Kind::Struct:
    {
        uint32_t scalars = 0;
        for (unsigned int field = 0; field < type->getFieldCount(); field++)
        {
            scalars += CountScalars(type->getFieldByIndex(field)->getType());
        }
        return scalars;
    }
    default:
        return 0;
    }
}

static bool HasCategory(slang::VariableLayoutReflection* parameter, slang::ParameterCategory category)
{
    for (unsigned int i = 0; i < parameter->getCategoryCount(); i++)
    {
        if (parameter->getCategoryByIndex(i) == category) return true;
    }
    return parameter->getCategory() == category;
}

void ShaderCompiler::ReflectRaytracingSizes(slang::IComponentType* linked, CompilationResult& result)
{
    if (linked == nullptr) return;

    Slang::ComPtr<slang::IBlob> diagnostics;
    slang::ProgramLayout* layout = linked->getLayout(0, diagnostics.writeRef());
    CheckDiagnostics(diagnostics.get(), result);
    if (layout == nullptr) return;

    for (SlangUInt i = 0; i < layout->getEntryPointCount(); i++)
    {
        slang::EntryPointReflection* entryPoint = layout->getEntryPointByIndex(i);
        if (entryPoint == nullptr) continue;

        for (unsigned int p = 0; p < entryPoint->getParameterCount(); p++)
        {
            slang::VariableLayoutReflection* parameter = entryPoint->getParameterByIndex(p);
            if (parameter == nullptr) continue;

            const uint32_t sizeInBytes = CountScalars(parameter->getType()) * 4;

            if (HasCategory(parameter, slang::ParameterCategory::RayPayload))
            {
                result.payloadSizeInBytes = std::max(result.payloadSizeInBytes, sizeInBytes);
            }
            else if (HasCategory(parameter, slang::ParameterCategory::HitAttributes))
            {
                result.attributeSizeInBytes = std::max(result.attributeSizeInBytes, sizeInBytes);
            }
        }
    }
}

void ShaderCompiler::CheckDiagnostics(slang::IBlob* diagnosticsBlob, CompilationResult& result)
{
    if (diagnosticsBlob != nullptr)
    {
		result.errorLog += static_cast<const char*>(diagnosticsBlob->getBufferPointer());
    }
}

ShaderCompiler::CompilationResult ShaderCompiler::Compile(const std::string& filePath, const std::vector<std::string>& entryPoints, bool isRaytracing,
                                                          const std::vector<std::pair<std::string, std::string>>& defines) const
{
    CompilationResult result;

    std::vector<std::string> epNames = entryPoints.empty()
        ? std::vector<std::string>{"RayGen", "ClosestHit", "Miss", "ShadowMiss", "AnyHit"}
		: entryPoints;

    bool wholeProgram = isRaytracing;

    slang::SessionDesc sessionDesc{};
    slang::TargetDesc targetDesc{};
    targetDesc.format = SLANG_DXIL;
    targetDesc.profile = m_globalSession->findProfile("sm_6_9");

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

    std::vector<slang::PreprocessorMacroDesc> macros;
    macros.reserve(defines.size());
    for (const auto& [name, value] : defines)
    {
        macros.push_back({ name.c_str(), value.c_str() });
    }
    sessionDesc.preprocessorMacros = macros.data();
    sessionDesc.preprocessorMacroCount = static_cast<SlangInt>(macros.size());

    std::string directory = std::filesystem::path(filePath).parent_path().string();
    const char* searchPaths[] = { directory.c_str(), m_includeDirectory.c_str() };
    sessionDesc.searchPaths = searchPaths;
    sessionDesc.searchPathCount = _countof(searchPaths);

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

    if (isRaytracing)
    {
        ReflectRaytracingSizes(linked.get(), result);
    }

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

    if (!code)
    {
	    result.errorLog = "Failed to generate code"; 
    	return result;
    }

    result.blob.resize(code->getBufferSize());
    memcpy(result.blob.data(), code->getBufferPointer(), code->getBufferSize());
    result.success = true;
    return result;
}