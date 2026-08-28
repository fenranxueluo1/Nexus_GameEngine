cmake_minimum_required(VERSION 4.3)
project(Nexus)

# ---------- C++ 标准 ----------
# 注意：必须在 add_library 之前设置，CMake 只在目标创建时读取该变量，
# 否则目标会退回到编译器默认标准（MSVC 为 C++14，无法使用 std::format / std::source_location）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# MSVC编译器 启用最新C++特性
# 注意：顶层没有可执行目标，使用 add_compile_options 使其对
# 后续 add_subdirectory 引入的所有子目标生效
if(MSVC)
    add_compile_options(/std:c++latest)
endif()

# ---------- 全局输出目录 ----------
# 把所有构建产物集中到 build/bin，方便运行和调试
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ---------- 默认构建类型 ----------
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ---------- 编译选项 ----------
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------- Vulkan 开发包：修正新版 SDK 的目录布局 ----------
# 自 1.4.3xx 起，Vulkan SDK 把 loader 从 $VULKAN_SDK/lib 挪到了
# $VULKAN_SDK/lib/VulkanLoader/lib，而 CMake 的 FindVulkan 只搜索前者，
# 会导致 "Could NOT find Vulkan (missing: Vulkan_LIBRARY)"。这里补上搜索路径。
# Windows 的 SDK 仍沿用 lib/ 布局，无需处理。
if(UNIX AND DEFINED ENV{VULKAN_SDK})
    set(_NEXUS_VULKAN_LOADER_DIR "$ENV{VULKAN_SDK}/lib/VulkanLoader/lib")
    if(IS_DIRECTORY "${_NEXUS_VULKAN_LOADER_DIR}")
        list(APPEND CMAKE_LIBRARY_PATH "${_NEXUS_VULKAN_LOADER_DIR}")
    endif()
endif()

# ---------- Vulkan 开发包 ----------
# 查找 Vulkan SDK，包含 volk（Vulkan 加载器）与 glslc（GLSL 着色器离线编译器）
# 着色器在构建时用 glslc 预编译为 SPIR-V，运行时直接加载 .spv（兼容 MinGW 工具链）
# 两平台都需要 Vulkan SDK：
#   Windows：安装官方 SDK 即可（Installer 会写入 VULKAN_SDK 环境变量）
#   Linux  ：下载 SDK 后需先 source setup-env.sh，使 VULKAN_SDK 指向 SDK 根目录
find_package(Vulkan REQUIRED COMPONENTS volk glslc)

# volk / glslc 并非所有 Vulkan 运行时包都附带，缺失时给出可操作的提示
if(NOT Vulkan_volk_FOUND)
    message(FATAL_ERROR
        "未找到 volk（Vulkan 加载器）。请安装完整版 Vulkan SDK，\n"
        "Linux 下请先执行：source <VulkanSDK>/setup-env.sh")
endif()
if(NOT Vulkan_glslc_FOUND)
    message(FATAL_ERROR
        "未找到 glslc（GLSL 离线编译器）。着色器需要在构建时预编译为 SPIR-V。\n"
        "请安装完整版 Vulkan SDK，Linux 下请先执行：source <VulkanSDK>/setup-env.sh")
endif()

# ---------- 引入顶层可执行项目 ----------
# NEXUS_EDITOR 会自动级联引入：
#   NEXUS_WINDOW  -> NEXUS_UTILITIES
#   NEXUS_LOGGER
#   NEXUS_RENDERING（内部使用 Vulkan SDK）
#   NEXUS_CORE
# SDL3 在 Windows 上取自 Dependencies/，在 Linux 上由系统包管理器提供
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/NEXUS_EDITOR
                 ${CMAKE_CURRENT_BINARY_DIR}/NEXUS_EDITOR)
