cmake_minimum_required(VERSION 4.3)
project(NEXUS_RENDERING)

# ---------- C++ 标准 ----------
# 注意：必须在 add_library 之前设置，CMake 只在目标创建时读取该变量，
# 否则目标会退回到编译器默认标准（MSVC 为 C++14，无法使用 std::format / std::source_location）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# ---------- 目标（静态链接库） ----------
add_library(NEXUS_RENDERING STATIC)

# MSVC编译器 启用最新C++特性（必须在目标创建之后调用）
if(MSVC)
    target_compile_options(NEXUS_RENDERING PRIVATE /std:c++latest)
endif()

# ---------- 源文件 ----------
target_sources(NEXUS_RENDERING
    PRIVATE
        Rendering/Core/VulkanContext.cpp
        Rendering/Essentials/Shader.cpp
        Rendering/Essentials/ShaderLoader.cpp
        Rendering/Essentials/Texture.cpp
        Rendering/Essentials/TextureLoader.cpp
        Rendering/Essentials/Vertex.h
        Rendering/Core/Camera2D.cpp
        Rendering/Core/BatchRenderer.cpp
)

# ---------- 对外头文件 ----------
# PUBLIC：使用方通过 target_link_libraries(NEXUS_RENDERING) 即可获得这些头文件路径，
# 这样可写 #include <Rendering/Essentials/ShaderLoader.h>
target_include_directories(NEXUS_RENDERING
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# ---------- glm 依赖（平台无关） ----------
# glm 是纯头文件库，两平台都使用仓库内的版本。
# 以 <glm/glm.hpp> 方式包含，故 include 路径指向其父目录 Dependencies/。
# SYSTEM：这些都是第三方库（glm / volk / vma），不应让它们的内部告警污染构建输出。
target_include_directories(NEXUS_RENDERING
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies
)

# ---------- Vulkan 依赖（平台无关） ----------
# Vulkan SDK 通过 find_package(Vulkan COMPONENTS volk glslc) 在顶层引入，
# 目标 Vulkan::Vulkan / Vulkan::volk 提供头文件与链接库
# （着色器用 glslc 离线预编译为 SPIR-V，因此不再链接 MSVC 专用的 shaderc）
#
# 注意：Linux 下 volk 内部用 dlopen/dlsym 动态加载 libvulkan，需要链接 dl。
target_link_libraries(NEXUS_RENDERING
    PUBLIC
        Vulkan::Vulkan
        Vulkan::volk
    PRIVATE
        NEXUS_LOGGER
)
if(UNIX AND NOT APPLE)
    target_link_libraries(NEXUS_RENDERING PRIVATE ${CMAKE_DL_LIBS})
endif()

# ==============================================================================
#  SDL3 / SDL3_image（Windows / Linux 分别配置）—— 仅为内部实现依赖，故为 PRIVATE
#
#  注意：导入目标只在调用 find_package 的目录及其子目录可见，
#  本库与 NEXUS_WINDOW 是兄弟目录，因此必须各自独立查找。
# ==============================================================================
if(WIN32)
    # Windows：使用仓库内 Dependencies/ 下随附的预编译库
    target_include_directories(NEXUS_RENDERING
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL/include
            ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL3_image/include
    )
    target_link_libraries(NEXUS_RENDERING
        PRIVATE
            SDL3
            SDL3_image
    )
else()
    # Linux：优先 CMake Config 包，未安装时回退 pkg-config
    find_package(PkgConfig QUIET)

    # --- SDL3 ---
    find_package(SDL3 QUIET CONFIG)
    if(TARGET SDL3::SDL3)
        target_link_libraries(NEXUS_RENDERING PRIVATE SDL3::SDL3)
    else()
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        target_link_libraries(NEXUS_RENDERING PRIVATE PkgConfig::SDL3)
    endif()

    # --- SDL3_image ---
    # 纹理加载（TextureLoader）依赖它，属于必需依赖
    find_package(SDL3_image QUIET CONFIG)
    if(TARGET SDL3_image::SDL3_image)
        target_link_libraries(NEXUS_RENDERING PRIVATE SDL3_image::SDL3_image)
    else()
        pkg_check_modules(SDL3_image REQUIRED IMPORTED_TARGET sdl3-image)
        target_link_libraries(NEXUS_RENDERING PRIVATE PkgConfig::SDL3_image)
    endif()
endif()