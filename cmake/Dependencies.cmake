include(FetchContent)

set(GLFW_VERSION        3.4)
set(GLM_VERSION         1.0.3)
set(FASTGLTF_COMMIT	    f89e438230b6624d5e886fac0d1829b7c7299b2e)
set(D3D12MA_VERSION     v3.2.0)
set(IMGUI_VERSION       v1.92.6)
set(STB_COMMIT          2c980bb59875b0d32144a71867fbdebb2f77cd20)
set(MIKKTSPACE_COMMIT   3e895b49d05ea07e4c2133156cfa94369e19e409)
set(IMREFLECT_VERSION   c4a07dba3badc78285a5cfdf324ab3f8412c3d50)
set(AGILITY_SDK_VERSION 1.619.6)
set(PIX_VERSION         1.0.240308001)
set(DXC_VERSION         v1.10.2605.37)
set(DXC_ZIP_NAME        dxc_2026_08_11.zip)
set(SLANG_VERSION       2026.18.3)
set(DLSS_VERSION        v310.9.1)

set(KYRA_DEPS_DIR ${CMAKE_BINARY_DIR}/_deps)

set(KYRA_EXTRA_RUNTIME_FILES "")
set(KYRA_AGILITY_RUNTIME_FILES "")

function(kyra_download_files base_url dest_dir)
    foreach(rel_path IN LISTS ARGN)
        set(out ${dest_dir}/${rel_path})
        if(NOT EXISTS ${out})
            message(STATUS "Downloading ${base_url}/${rel_path}")
            file(DOWNLOAD ${base_url}/${rel_path} ${out} STATUS status TLS_VERIFY ON)
            list(GET status 0 code)
            if(NOT code EQUAL 0)
                file(REMOVE ${out})
                message(FATAL_ERROR "Download failed: ${base_url}/${rel_path}\n${status}")
            endif()
        endif()
    endforeach()
endfunction()

set(_agility_id microsoft.direct3d.d3d12)
FetchContent_Declare(agility_sdk
    URL https://api.nuget.org/v3-flatcontainer/${_agility_id}/${AGILITY_SDK_VERSION}/${_agility_id}.${AGILITY_SDK_VERSION}.nupkg
    DOWNLOAD_NAME agility_sdk.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)
FetchContent_MakeAvailable(agility_sdk)

set(AGILITY_ROOT ${agility_sdk_SOURCE_DIR}/build/native)

add_library(agility_sdk INTERFACE)
target_include_directories(agility_sdk INTERFACE ${AGILITY_ROOT}/include)
if(EXISTS ${AGILITY_ROOT}/include/d3dx12/d3dx12.h)
    target_include_directories(agility_sdk INTERFACE ${AGILITY_ROOT}/include/d3dx12)
endif()

string(REGEX MATCH "^[0-9]+\\.([0-9]+)" _ ${AGILITY_SDK_VERSION})
target_compile_definitions(agility_sdk INTERFACE KYRA_D3D12_SDK_VERSION=${CMAKE_MATCH_1})

list(APPEND KYRA_AGILITY_RUNTIME_FILES
    ${AGILITY_ROOT}/bin/x64/D3D12Core.dll
    ${AGILITY_ROOT}/bin/x64/d3d12SDKLayers.dll)

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/${GLFW_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/archive/refs/tags/${GLM_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)

FetchContent_Declare(fastgltf
    URL https://github.com/spnda/fastgltf/archive/${FASTGLTF_COMMIT}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    DOWNLOAD_NO_PROGRESS TRUE)

set(D3D12MA_AGILITY_SDK_DIRECTORY ${agility_sdk_SOURCE_DIR} CACHE STRING "" FORCE)
FetchContent_Declare(D3D12MA
    URL https://github.com/GPUOpen-LibrariesAndSDKs/D3D12MemoryAllocator/archive/refs/tags/${D3D12MA_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)

FetchContent_Declare(imgui
    URL https://github.com/ocornut/imgui/archive/refs/tags/${IMGUI_VERSION}.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)

FetchContent_MakeAvailable(glfw glm fastgltf D3D12MA imgui)

# --- ImGui ---
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp
    ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${imgui_SOURCE_DIR}/misc/cpp)
target_link_libraries(imgui PUBLIC glfw agility_sdk d3d12 dxgi)

# --- ImReflect ---
set(IMREFLECT_DIR ${KYRA_DEPS_DIR}/imreflect-${IMREFLECT_VERSION})
kyra_download_files(
    https://raw.githubusercontent.com/Sven-vh/ImReflect/${IMREFLECT_VERSION}/single_header
    ${IMREFLECT_DIR}
    ImReflect.hpp)
add_library(imreflect INTERFACE)
target_include_directories(imreflect INTERFACE ${IMREFLECT_DIR})
target_link_libraries(imreflect INTERFACE imgui)

# --- MikkTSpace ---
set(MIKKTSPACE_DIR ${KYRA_DEPS_DIR}/mikktspace-${MIKKTSPACE_COMMIT})
kyra_download_files(
    https://raw.githubusercontent.com/mmikk/MikkTSpace/${MIKKTSPACE_COMMIT}
    ${MIKKTSPACE_DIR}
    mikktspace.c mikktspace.h)
add_library(mikktspace STATIC ${MIKKTSPACE_DIR}/mikktspace.c)
target_include_directories(mikktspace PUBLIC ${MIKKTSPACE_DIR})

# --- stb_image ---
set(STB_DIR ${KYRA_DEPS_DIR}/stb-${STB_COMMIT})
kyra_download_files(
    https://raw.githubusercontent.com/nothings/stb/${STB_COMMIT}
    ${STB_DIR}
    stb_image.h)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${STB_DIR})

# --- mcphysics ---
add_library(mcphysics INTERFACE)
target_include_directories(mcphysics INTERFACE ${CMAKE_SOURCE_DIR}/external/mcphysics)

# --- DXC ---
FetchContent_Declare(dxc
    URL https://github.com/microsoft/DirectXShaderCompiler/releases/download/${DXC_VERSION}/${DXC_ZIP_NAME}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)
FetchContent_MakeAvailable(dxc)
add_library(dxc SHARED IMPORTED GLOBAL)
set_target_properties(dxc PROPERTIES
    IMPORTED_LOCATION             ${dxc_SOURCE_DIR}/bin/x64/dxcompiler.dll
    IMPORTED_IMPLIB               ${dxc_SOURCE_DIR}/lib/x64/dxcompiler.lib
    INTERFACE_INCLUDE_DIRECTORIES ${dxc_SOURCE_DIR}/inc)
list(APPEND KYRA_EXTRA_RUNTIME_FILES ${dxc_SOURCE_DIR}/bin/x64/dxil.dll)   # needed for DXIL signing

# --- Slang ---
FetchContent_Declare(slang
    URL https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-windows-x86_64.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)
FetchContent_MakeAvailable(slang)
find_package(slang CONFIG REQUIRED PATHS ${slang_SOURCE_DIR}/cmake NO_DEFAULT_PATH)
list(APPEND KYRA_EXTRA_RUNTIME_FILES
    ${slang_SOURCE_DIR}/bin/slang.dll
    ${slang_SOURCE_DIR}/bin/slang-rt.dll)

# --- WinPixEventRuntime ---
FetchContent_Declare(winpix
    URL https://api.nuget.org/v3-flatcontainer/winpixeventruntime/${PIX_VERSION}/winpixeventruntime.${PIX_VERSION}.nupkg
    DOWNLOAD_NAME winpix.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	DOWNLOAD_NO_PROGRESS TRUE)
FetchContent_MakeAvailable(winpix)
add_library(winpix SHARED IMPORTED GLOBAL)
set_target_properties(winpix PROPERTIES
    IMPORTED_LOCATION             ${winpix_SOURCE_DIR}/bin/x64/WinPixEventRuntime.dll
    IMPORTED_IMPLIB               ${winpix_SOURCE_DIR}/bin/x64/WinPixEventRuntime.lib
    INTERFACE_INCLUDE_DIRECTORIES "${winpix_SOURCE_DIR}/Include;${winpix_SOURCE_DIR}/Include/WinPixEventRuntime")

# --- NGX / DLSS ---
set(DLSS_DIR ${KYRA_DEPS_DIR}/dlss-${DLSS_VERSION})
kyra_download_files(
    https://raw.githubusercontent.com/NVIDIA/DLSS/${DLSS_VERSION}
    ${DLSS_DIR}
    lib/Windows_x86_64/x64/nvsdk_ngx_d.lib
    lib/Windows_x86_64/x64/nvsdk_ngx_d_dbg.lib
    lib/Windows_x86_64/rel/nvngx_dlssd.dll
    lib/Windows_x86_64/dev/nvngx_dlssd.dll
    include/nvsdk_ngx.h
    include/nvsdk_ngx_defs.h
    include/nvsdk_ngx_defs_dlssd.h
    include/nvsdk_ngx_defs_dlssg.h
    include/nvsdk_ngx_defs_vk.h
    include/nvsdk_ngx_helpers.h
    include/nvsdk_ngx_helpers_cuda.h
    include/nvsdk_ngx_helpers_d3d.h
    include/nvsdk_ngx_helpers_dlssd.h
    include/nvsdk_ngx_helpers_dlssd_cuda.h
    include/nvsdk_ngx_helpers_dlssd_d3d.h
    include/nvsdk_ngx_helpers_dlssd_vk.h
    include/nvsdk_ngx_helpers_dlssg.h
    include/nvsdk_ngx_helpers_dlssg_d3d.h
    include/nvsdk_ngx_helpers_dlssg_vk.h
    include/nvsdk_ngx_helpers_vk.h
    include/nvsdk_ngx_loader.h
    include/nvsdk_ngx_params.h
    include/nvsdk_ngx_params_dlssd.h
    include/nvsdk_ngx_params_dlssg.h
    include/nvsdk_ngx_standalone_common.h
    include/nvsdk_ngx_standalone_cuda.h
    include/nvsdk_ngx_vk.h)
add_library(ngx STATIC IMPORTED GLOBAL)
set_target_properties(ngx PROPERTIES
    IMPORTED_LOCATION             ${DLSS_DIR}/lib/Windows_x86_64/x64/nvsdk_ngx_d.lib
    IMPORTED_LOCATION_DEBUG       ${DLSS_DIR}/lib/Windows_x86_64/x64/nvsdk_ngx_d_dbg.lib
    INTERFACE_INCLUDE_DIRECTORIES ${DLSS_DIR}/include)
set(NGX_DLL_DIR ${DLSS_DIR}/lib/Windows_x86_64)
list(APPEND KYRA_EXTRA_RUNTIME_FILES
    $<IF:$<CONFIG:Debug>,${NGX_DLL_DIR}/dev/nvngx_dlssd.dll,${NGX_DLL_DIR}/rel/nvngx_dlssd.dll>)

add_library(kyra_deps INTERFACE)
target_link_libraries(kyra_deps INTERFACE
    agility_sdk d3d12 dxgi dxguid
    D3D12MemoryAllocator
    dxc slang::slang ngx winpix
    fastgltf::fastgltf
    glfw glm::glm
    imgui imreflect mikktspace stb mcphysics)

foreach(t glfw fastgltf D3D12MemoryAllocator imgui mikktspace)
    if(TARGET ${t})
        set_target_properties(${t} PROPERTIES FOLDER "Dependencies")
    endif()
endforeach()

function(kyra_copy_runtime_files target)
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:${target}>
                ${KYRA_EXTRA_RUNTIME_FILES}
                $<TARGET_FILE_DIR:${target}>
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target}>/D3D12
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${KYRA_AGILITY_RUNTIME_FILES}
                $<TARGET_FILE_DIR:${target}>/D3D12
        COMMAND_EXPAND_LISTS)
endfunction()