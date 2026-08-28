cmake_minimum_required(VERSION 4.3)
project(NEXUS_EDITOR)

# ---------- C++ 标准 ----------
# 注意：必须在 add_library 之前设置，CMake 只在目标创建时读取该变量，
# 否则目标会退回到编译器默认标准（MSVC 为 C++14，无法使用 std::format / std::source_location）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# ---------- 引入 NEXUS_WINDOW 子项目 ----------
# NEXUS_WINDOW 会自动通过 add_subdirectory 引入 NEXUS_UTILITIES，
# 同时以 PUBLIC 方式传递它们的头文件路径与 SDL3 头文件路径。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../NEXUS_WINDOW
                 ${CMAKE_CURRENT_BINARY_DIR}/NEXUS_WINDOW)

# ---------- 引入 NEXUS_LOGGER 子项目（静态库） ----------
# 提供日志功能（NEXUS_LOG / NEXUS_WARN / NEXUS_ERROR），其 CMakeLists 已 PUBLIC 暴露头文件路径。
# 注意：必须在引入 NEXUS_RENDERING 之前先引入，因为 NEXUS_RENDERING 的 CMakeLists
# 会直接引用 NEXUS_LOGGER 目标（同一目录不可被 add_subdirectory 两次）。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../NEXUS_LOGGER
                 ${CMAKE_CURRENT_BINARY_DIR}/NEXUS_LOGGER)

# ---------- 引入 NEXUS_RENDERING 子项目（静态库） ----------
# 提供渲染相关功能（VulkanContext / Shader / ShaderLoader / Texture / BatchRenderer），
# 其 CMakeLists 已 PUBLIC 链接 Vulkan SDK、glm 与 NEXUS_LOGGER。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../NEXUS_RENDERING
                 ${CMAKE_CURRENT_BINARY_DIR}/NEXUS_RENDERING)

# ---------- 引入 NEXUS_CORE 子项目（静态库） ----------
# 提供 ECS 相关功能（Entity / Registry / Components），其 CMakeLists 已 PUBLIC 链接
# NEXUS_RENDERING 并暴露 entt、glm 等头文件路径。
# 注意：必须在引入 NEXUS_RENDERING 之后引入，因为其 CMakeLists 会直接引用
# NEXUS_RENDERING 目标（同一目录不可被 add_subdirectory 两次）。
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../NEXUS_CORE
                 ${CMAKE_CURRENT_BINARY_DIR}/NEXUS_CORE)

# ---------- 目标 ----------
add_executable(NEXUS_EDITOR "src/main.cpp"
                            "src/Application.cpp")

# MSVC编译器 启用最新C++特性（必须在目标创建之后调用）
if(MSVC)
    target_compile_options(NEXUS_EDITOR PRIVATE /std:c++latest)
    # shaderc 等第三方预编译库不附带 PDB，忽略 LNK4099 警告（不影响链接结果）
    target_link_options(NEXUS_EDITOR PRIVATE /ignore:4099)
endif()

# ---------- 对外头文件 ----------
target_include_directories(NEXUS_EDITOR
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ---------- 与平台无关的头文件依赖 ----------
# glm 的目录结构为 Dependencies/glm/glm.hpp，使用方以 <glm/glm.hpp> 方式包含，
# 故 include 路径需指向 glm 目录的父目录（Dependencies/），
# 这样 <glm/glm.hpp> 才能正确解析到 Dependencies/glm/glm.hpp。
target_include_directories(NEXUS_EDITOR
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies
)

# ---------- 链接依赖（平台无关部分） ----------
# 链接 NEXUS_WINDOW 即可级联获得 NEXUS_UTILITIES 的头文件路径
# Vulkan::Vulkan：最终可执行文件需要 Vulkan 符号（由顶层 find_package 提供）
target_link_libraries(NEXUS_EDITOR
    PRIVATE
        NEXUS_WINDOW
        NEXUS_UTILITIES
        NEXUS_RENDERING
        NEXUS_CORE
        NEXUS_LOGGER
        Vulkan::Vulkan
)

# ==============================================================================
#  SDL3 及扩展库（Windows / Linux 分别配置）
#
#  最终可执行文件需要这些符号，故在此显式链接。
#
#  注意：导入目标只在调用 find_package 的目录及其子目录可见，
#  这里与其它子项目是兄弟目录，必须各自独立查找。
#  重复调用 find_package 与 pkg_check_modules 是安全的（内部有缓存）。
# ==============================================================================
if(WIN32)
    # Windows：使用仓库内 Dependencies/ 下随附的预编译库
    set(SDL_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL)
    set(SDL_IMG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL3_image)
    set(SDL_MIX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL3_mixer)
    set(SDL_TTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL3_ttf)

    # 头文件路径：使用方以 <SDL3_image/SDL_image.h> 等方式包含
    target_include_directories(NEXUS_EDITOR
        PRIVATE
            ${SDL_IMG_DIR}/include
            ${SDL_MIX_DIR}/include
            ${SDL_TTF_DIR}/include
    )

    # 链接目录（.lib 所在）
    target_link_directories(NEXUS_EDITOR
        PRIVATE
            ${SDL_DIR}/lib
            ${SDL_IMG_DIR}/lib
            ${SDL_MIX_DIR}/lib
            ${SDL_TTF_DIR}/lib
    )

    target_link_libraries(NEXUS_EDITOR
        PRIVATE
            SDL3
            SDL3_image
            SDL3_mixer
            SDL3_ttf
    )
else()
    # Linux：优先 CMake Config 包，未安装时回退 pkg-config
    find_package(PkgConfig QUIET)

    # --- SDL3 ---
    find_package(SDL3 QUIET CONFIG)
    if(TARGET SDL3::SDL3)
        target_link_libraries(NEXUS_EDITOR PRIVATE SDL3::SDL3)
    else()
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        target_link_libraries(NEXUS_EDITOR PRIVATE PkgConfig::SDL3)
    endif()

    # --- SDL3_image ---（纹理加载必需）
    find_package(SDL3_image QUIET CONFIG)
    if(TARGET SDL3_image::SDL3_image)
        target_link_libraries(NEXUS_EDITOR PRIVATE SDL3_image::SDL3_image)
    else()
        pkg_check_modules(SDL3_image REQUIRED IMPORTED_TARGET sdl3-image)
        target_link_libraries(NEXUS_EDITOR PRIVATE PkgConfig::SDL3_image)
    endif()

    # --- SDL3_mixer ---
    # 当前代码尚未使用音频功能，故缺失时仅跳过而不中断配置
    find_package(SDL3_mixer QUIET CONFIG)
    if(TARGET SDL3_mixer::SDL3_mixer)
        target_link_libraries(NEXUS_EDITOR PRIVATE SDL3_mixer::SDL3_mixer)
    else()
        pkg_check_modules(SDL3_mixer QUIET IMPORTED_TARGET sdl3-mixer)
        if(TARGET PkgConfig::SDL3_mixer)
            target_link_libraries(NEXUS_EDITOR PRIVATE PkgConfig::SDL3_mixer)
        else()
            message(STATUS "未找到 SDL3_mixer（可选依赖，已跳过）")
        endif()
    endif()

    # --- SDL3_ttf ---
    # 当前代码尚未使用字体功能，故缺失时仅跳过而不中断配置
    find_package(SDL3_ttf QUIET CONFIG)
    if(TARGET SDL3_ttf::SDL3_ttf)
        target_link_libraries(NEXUS_EDITOR PRIVATE SDL3_ttf::SDL3_ttf)
    else()
        pkg_check_modules(SDL3_ttf QUIET IMPORTED_TARGET sdl3-ttf)
        if(TARGET PkgConfig::SDL3_ttf)
            target_link_libraries(NEXUS_EDITOR PRIVATE PkgConfig::SDL3_ttf)
        else()
            message(STATUS "未找到 SDL3_ttf（可选依赖，已跳过）")
        endif()
    endif()
endif()

# ==============================================================================
#  以下为 Windows 专有部分：把随附的运行时 dll 拷贝到可执行文件目录
#  Linux 下 SDL3 等由系统包管理器提供，位于默认动态库搜索路径，无需拷贝。
# ==============================================================================
if(WIN32)
    # ---------- 拷贝 SDL3 及扩展库 dll 到输出目录 ----------
    # SDL3_image / SDL3_mixer / SDL3_ttf 各自 lib/ 下都附带了运行时需要的 dll
    # （含它们内部依赖的格式解码 dll，如 libpng、libjpeg、libwebp、libogg 等）
    add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDL_DIR}/lib/SDL3.dll
            $<TARGET_FILE_DIR:NEXUS_EDITOR>
    )
    add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDL_IMG_DIR}/lib/SDL3_image.dll
            $<TARGET_FILE_DIR:NEXUS_EDITOR>
    )
    add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDL_MIX_DIR}/lib/SDL3_mixer.dll
            $<TARGET_FILE_DIR:NEXUS_EDITOR>
    )
    add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SDL_TTF_DIR}/lib/SDL3_ttf.dll
            $<TARGET_FILE_DIR:NEXUS_EDITOR>
    )

    # ---------- 拷贝 MinGW 运行时 DLL 到 exe 目录 ----------
    # 避免程序从 PATH 里误加载 Git 自带的 libstdc++-6.dll（版本不匹配会崩溃）。
    # Windows 优先从 exe 所在目录加载 DLL，因此把编译工具链的运行时 DLL 拷贝过去即可。
    if(MINGW)
        get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        foreach(runtime_dll libstdc++-6.dll libgcc_s_seh-1.dll libwinpthread-1.dll)
            add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${MINGW_BIN_DIR}/${runtime_dll}
                    $<TARGET_FILE_DIR:NEXUS_EDITOR>
            )
        endforeach()
    endif()

    # SDL3_image / SDL3_mixer 的 lib/optional/ 下是可选格式解码库
    # （libpng/libwebp/libtiff/libogg/libopus 等）。核心 dll 已支持常见格式，
    # 但加载 webp/tiff/opus 等需这些 dll，统一拷贝以防运行时缺失。
    # 注意：MinGW 发行版没有 optional/ 目录，故先检查源目录是否存在。
    if(EXISTS ${SDL_IMG_DIR}/lib/optional)
        add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SDL_IMG_DIR}/lib/optional
                $<TARGET_FILE_DIR:NEXUS_EDITOR>/optional
        )
    endif()
    if(EXISTS ${SDL_MIX_DIR}/lib/optional)
        add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${SDL_MIX_DIR}/lib/optional
                $<TARGET_FILE_DIR:NEXUS_EDITOR>/optional
        )
    endif()
endif()

# ---------- 拷贝 assets 资源到输出目录 ----------
# 运行时按相对路径 assets/textures/... 加载，需保证 exe 同目录下有 assets/
add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:NEXUS_EDITOR>/assets
)

# ---------- 用 glslc 预编译着色器为 SPIR-V ----------
# 把 assets/shaders 下的 .vert / .frag 编译成 .spv，输出到 exe 同目录的 assets/shaders/，
# 运行时 VulkanContext 直接加载 .spv（避免链接 MSVC 专用的 shaderc 库，兼容 MinGW）
add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
    COMMAND $<TARGET_FILE:Vulkan::glslc>
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/basicShader.vert
        -o $<TARGET_FILE_DIR:NEXUS_EDITOR>/assets/shaders/basicShader.vert.spv
)
add_custom_command(TARGET NEXUS_EDITOR POST_BUILD
    COMMAND $<TARGET_FILE:Vulkan::glslc>
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/basicShader.frag
        -o $<TARGET_FILE_DIR:NEXUS_EDITOR>/assets/shaders/basicShader.frag.spv
)
