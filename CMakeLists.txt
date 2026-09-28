cmake_minimum_required(VERSION 3.24...4.4)

project(
  Kyra
  LANGUAGES CXX)

get_property(KYRA_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(KYRA_MULTI_CONFIG)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
else()
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    endif()
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # For IDE support
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_compile_definitions(NOMINMAX)

include(cmake/Dependencies.cmake)

# ----------------------------------------------
# Game executable
# ----------------------------------------------
file(GLOB_RECURSE KYRA_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/source/*.cpp
    ${CMAKE_SOURCE_DIR}/include/*.h)
 
file(GLOB_RECURSE KYRA_SHADERS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/shaders/*.slang
    ${CMAKE_SOURCE_DIR}/shaders/*.h)
set_source_files_properties(${KYRA_SHADERS} PROPERTIES HEADER_FILE_ONLY ON)
 
set(KYRA_RESOURCES
    ${CMAKE_SOURCE_DIR}/assets/icons/Kyra.rc
    ${CMAKE_SOURCE_DIR}/assets/icons/resource.h)
 
add_executable(Kyra ${KYRA_SOURCES} ${KYRA_SHADERS} ${KYRA_RESOURCES})
 
target_include_directories(Kyra PRIVATE
    ${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include/renderer)
 
target_link_libraries(Kyra PRIVATE kyra_deps)
 
target_compile_definitions(Kyra PRIVATE UNICODE _UNICODE)
target_compile_definitions(Kyra PRIVATE FASTGLTF_ENABLE_DEPRECATED_EXT 1)
 
if(MSVC)
    target_compile_options(Kyra PRIVATE /MP)
endif()

# Show the same folder structure in Visual Studio's Solution Explorer
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${KYRA_SOURCES} ${KYRA_SHADERS} ${KYRA_RESOURCES})

set_target_properties(Kyra PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT Kyra)
 
kyra_copy_runtime_files(Kyra)
