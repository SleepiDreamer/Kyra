#pragma once
#include <d3dx12.h>
#include <glm/glm.hpp>
#include <fastgltf/types.hpp>
#include <ImReflect.hpp>
#include <iostream>
#include <stdexcept>

inline void ThrowError(const std::string& msg)
{
    std::cerr << msg << std::endl;
    __debugbreak();
}

inline void ThrowIfFailed(const HRESULT hr, const char* msg = "")
{
    if (FAILED(hr))
    {
	    char* hrCstr = nullptr;
    	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		hr,
		0,
		reinterpret_cast<LPSTR>(&hrCstr),
		0,
		nullptr);

    	std::cerr << "HRESULT: " << hrCstr << std::endl;
        ThrowError(msg);
    }
}

inline void TransitionResource(ID3D12GraphicsCommandList2* commandList, ID3D12Resource* resource,
							   const D3D12_RESOURCE_STATES beforeState, const D3D12_RESOURCE_STATES afterState)
{
    const CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, beforeState, afterState);

    commandList->ResourceBarrier(1, &barrier);
}

inline std::wstring ToWideString(const char* str)
{
    if (!str || !*str) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring wstr(len - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, str, -1, wstr.data(), len);
    return wstr;
}

inline std::wstring ToWideString(const std::string& str)
{
    return ToWideString(str.c_str());
}

inline std::string ToNarrowString(const wchar_t* wstr)
{
    if (!wstr || !*wstr) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    std::string str(len - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str.data(), len, nullptr, nullptr);
    return str;
}

inline std::string ToNarrowString(const std::wstring& wstr)
{
    return ToNarrowString(wstr.c_str());
}

inline void tag_invoke(ImReflect::ImInput_t, const char* label, BOOL& value, ImSettings& settings, ImResponse& response) {
    auto& bool_response = response.get<BOOL>();

    bool temp = static_cast<bool>(value);
    if (ImGui::Checkbox(label, &temp)) {
        value = temp ? TRUE : FALSE;
        bool_response.changed();
    }

    ImReflect::Detail::check_input_states(bool_response);
}

inline void tag_invoke(ImReflect::ImInput_t, const char* label, glm::vec2& value, ImSettings& settings, ImResponse& response) {
    auto& vec2_response = response.get<glm::vec2>();

	bool changed = ImGui::DragFloat2(label, &value.x, 0.02f);
    if (changed) vec2_response.changed();

    ImReflect::Detail::check_input_states(vec2_response);
}

inline void tag_invoke(ImReflect::ImInput_t, const char* label, glm::vec3& value, ImSettings& settings, ImResponse& response) {
    auto& vec3_response = response.get<glm::vec3>();

    bool changed = ImGui::DragFloat3(label, &value.x, 0.02f);
    if (changed) vec3_response.changed();

    ImReflect::Detail::check_input_states(vec3_response);
}

inline uint32_t FormatByteSize(const DXGI_FORMAT format)
{
    switch (format)
    {
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_FLOAT:         return 4;
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32_UINT:       return 8;
    case DXGI_FORMAT_R32G32B32_FLOAT:   return 12;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_UINT: return 16;
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_FLOAT:         return 2;
    case DXGI_FORMAT_R16G16_FLOAT:      return 4;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:return 8;
    case DXGI_FORMAT_R8G8B8A8_UNORM:    return 4;
    default:
        ThrowError("Unknown DXGI_FORMAT byte size");
        return 0;
    }
}

inline D3D12_RESOURCE_STATES InitialStateFromHeapType(const D3D12_HEAP_TYPE heapType)
{
    switch (heapType)
    {
    case D3D12_HEAP_TYPE_UPLOAD:   return D3D12_RESOURCE_STATE_GENERIC_READ;
    case D3D12_HEAP_TYPE_READBACK: return D3D12_RESOURCE_STATE_COPY_DEST;
    default:                       return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
}

inline D3D12_FILTER GetGltfFilter(const fastgltf::Filter minFilter, const fastgltf::Filter magFilter)
{
    bool minLinear = (minFilter == fastgltf::Filter::Linear ||
        minFilter == fastgltf::Filter::LinearMipMapLinear ||
        minFilter == fastgltf::Filter::LinearMipMapNearest);
    bool magLinear = (magFilter == fastgltf::Filter::Linear);
    bool mipLinear = (minFilter == fastgltf::Filter::LinearMipMapLinear ||
        minFilter == fastgltf::Filter::NearestMipMapLinear);

    return static_cast<D3D12_FILTER>(
        (minLinear ? D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT : 0) |
        (magLinear ? D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : 0) |
        (mipLinear ? D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR : 0));
}

inline D3D12_TEXTURE_ADDRESS_MODE GetGltfWrap(const fastgltf::Wrap wrap)
{
    switch (wrap) {
    case fastgltf::Wrap::Repeat:         return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case fastgltf::Wrap::ClampToEdge:    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    case fastgltf::Wrap::MirroredRepeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    default:                             return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    }
}