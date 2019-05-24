cmake_minimum_required(VERSION 3.8 FATAL_ERROR)

project("TEShader")

add_subdirectory(external/glslang)
add_subdirectory(external/SPIRV-Cross)

# Headers
set(TESHADER_HEADERS "${PROJECT_SOURCE_DIR}/external/glslang/StandAlone/ResourceLimits.h"
                     "${PROJECT_SOURCE_DIR}/external/glslang/StandAlone/DirStackFileIncluder.h"
                     "${PROJECT_SOURCE_DIR}/external/glslang/StandAlone/Worklist.h")

# Sources
set(TESHADER_SOURCES "${PROJECT_SOURCE_DIR}/external/glslang/StandAlone/ResourceLimits.cpp"
                     "${PROJECT_SOURCE_DIR}/src/main.cpp")

# Source groups
source_group("Headers" FILES ${TESHADER_HEADERS})
source_group("Sources" FILES ${TESHADER_SOURCES})

# Include paths
set(TESHADER_INCLUDE_DIRS "${PROJECT_SOURCE_DIR}/external/glslang/glslang/Include"
                          "${PROJECT_SOURCE_DIR}/external/glslang/SPIRV"
                          "${PROJECT_SOURCE_DIR}/external/glslang/StandAlone"
                          "${PROJECT_SOURCE_DIR}/external/SPIRV-Cross")

include_directories("${TESHADER_INCLUDE_DIRS}")

if(APPLE)
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++14")
    set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")
endif()

add_executable(TEShader ${TESHADER_HEADERS} ${TESHADER_SOURCES})

set(LIBRARIES
    glslang
    SPIRV
    SPVRemapper
    glslang-default-resource-limits
    spirv-cross-glsl
    spirv-cross-hlsl
    spirv-cross-cpp
    spirv-cross-msl
    spirv-cross-core)

target_link_libraries(TEShader ${LIBRARIES})