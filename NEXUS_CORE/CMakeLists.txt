cmake_minimum_required(VERSION 4.3)
project(NEXUS_CORE)

# ---------- C++ 标准 ----------
# 注意：必须在 add_library 之前设置，CMake 只在目标创建时读取该变量，
# 否则目标会退回到编译器默认标准（MSVC 为 C++14，无法使用 std::format / std::source_location）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# ---------- 目标（静态链接库） ----------
add_library(NEXUS_CORE STATIC)

# MSVC编译器 启用最新C++特性（必须在目标创建之后调用）
if(MSVC)
    target_compile_options(NEXUS_CORE PRIVATE /std:c++latest)
endif()

# ---------- 源文件 ----------
target_sources(NEXUS_CORE
    PRIVATE
        Core/ECS/Entity.cpp
        Core/ECS/Registry.cpp
        Core/ECS/MetaUtilities.cpp
        Core/Resources/AssetManager.cpp
        Core/Systems/ScriptingSystem.cpp
        Core/Systems/RenderSystem.cpp
        Core/ECS/Components/TransformComponent.cpp
        Core/ECS/Components/SpriteComponent.cpp
        Core/Scripting/GlmLuaBindings.cpp
        Core/ECS/Components/AnimationComponent.cpp
        Core/Systems/AnimationSystem.cpp

)

# ---------- 对外头文件 ----------
# PUBLIC：使用方通过 target_link_libraries(NEXUS_CORE) 即可获得这些头文件路径，
# 这样可写 #include <Core/ECS/Entity.h>
target_include_directories(NEXUS_CORE
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# ---------- 与平台无关的第三方头文件依赖 ----------
# Registry.h           使用 <entt.hpp>
# TransformComponent.h 使用 <glm/glm.hpp>（include 路径指向 glm 的父目录 Dependencies/）
# ScriptComponent.h    使用 <LuaBridge3/LuaBridge.h>
# SpriteComponent.h    使用 <Rendering/Essentials/Vertex.h>（由 NEXUS_RENDERING 级联传递）
# 这些都是纯头文件库，两平台都使用仓库内的版本，故以 PUBLIC 暴露。
# SYSTEM：第三方库的内部告警（如 entt 的 -Wmissing-field-initializers）不应污染构建输出。
target_include_directories(NEXUS_CORE
    SYSTEM PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/entt
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies
        ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/LuaBridge3
)

# ---------- 链接依赖 ----------
# NEXUS_RENDERING：SpriteComponent.h / AssetManager 依赖（PUBLIC，头文件路径级联传递）
# NEXUS_LOGGER：AssetManager.cpp 使用 <Logger/Logger.h>（NEXUS_ERROR 等日志宏）
target_link_libraries(NEXUS_CORE
    PUBLIC
        NEXUS_RENDERING
    PRIVATE
        NEXUS_LOGGER
)

# ==============================================================================
#  Lua（Windows / Linux 分别配置）
#
#  ScriptComponent.h 通过 <lua.hpp> 暴露 Lua 类型，故头文件路径需 PUBLIC。
#  LuaBridge3 只是绑定层，真正提供实现的是 Lua 库本身，必须显式链接。
#
#  注意：Lua 官方不提供 CMake Config 包，Linux 下统一用 find_path / find_library
#  定位系统安装的版本（find_path / find_library 的结果会写入 CACHE，
#  重复调用无副作用，也不存在导入目标的目录作用域问题）。
# ==============================================================================
if(WIN32)
    # Windows：使用仓库内随附的 Lua 5.5
    target_include_directories(NEXUS_CORE
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/Lua_5.5/include
    )
    target_link_directories(NEXUS_CORE
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/Lua_5.5/lib
    )
    target_link_libraries(NEXUS_CORE
        PUBLIC
            lua55
    )
else()
     # Linux：优先用 pkg-config 定位，保证头文件与库来自同一版本。
    #
    # 不能用 find_path / find_library 分开找：多版本共存时会各取所需，
    # 出现「头文件 5.4 + 库 5.5」的错配。Lua 5.5 已把 luaL_openlibs
    # 由函数改成展开为 luaL_openselectedlibs 的宏，拿 5.4 头文件去链 5.5 库
    # 就会得到 undefined reference to `luaL_openlibs'。
    #
    # pkg_check_modules 一次传多个模块名时要求全部命中，而这些名字是同一库在
    # 不同发行版下的别名（Arch: lua/lua5.5，Fedora/Ubuntu: lua5.4…），故逐个试。
    find_package(PkgConfig QUIET)

        set(NEXUS_LUA_PC_MODULE "")
    if(PKG_CONFIG_FOUND)
        foreach(mod IN ITEMS lua5.5 lua-5.5 lua55 lua lua5.4 lua-5.4 lua54)
            pkg_check_modules(NEXUS_LUA_PROBE QUIET ${mod})
            if(NEXUS_LUA_PROBE_FOUND)
                set(NEXUS_LUA_PC_MODULE ${mod})
                break()
            endif()
        endforeach()
    endif()

    if(NEXUS_LUA_PC_MODULE)
        # IMPORTED_TARGET 把头文件路径与库绑成一个目标，天然版本一致
        pkg_check_modules(NEXUS_LUA REQUIRED IMPORTED_TARGET ${NEXUS_LUA_PC_MODULE})
        target_link_libraries(NEXUS_CORE
            PUBLIC
                PkgConfig::NEXUS_LUA
        )
    else()
        # 回退：系统没有 .pc 文件时（如源码安装）手工查找
        find_path(NEXUS_CORE_LUA_INCLUDE_DIR
            NAMES lua.hpp lua.h
            PATH_SUFFIXES lua5.5 lua-5.5 lua lua5.4 lua-5.4
        )
        find_library(NEXUS_CORE_LUA_LIBRARY
            NAMES lua5.5 lua-5.5 lua55 lua lua5.4 lua-5.4 lua54
        )

        if(NOT NEXUS_CORE_LUA_INCLUDE_DIR OR NOT NEXUS_CORE_LUA_LIBRARY)
            message(FATAL_ERROR
                "未找到 Lua，请安装其开发包（例如 Ubuntu: sudo apt install liblua5.5-dev; "
                "Fedora: sudo dnf install lua-devel）")
        endif()

        target_include_directories(NEXUS_CORE
            PUBLIC
                ${NEXUS_CORE_LUA_INCLUDE_DIR}
        )
        target_link_libraries(NEXUS_CORE
            PUBLIC
                ${NEXUS_CORE_LUA_LIBRARY}
        )
    endif()
endif()

# ==============================================================================
#  SDL3（Windows / Linux 分别配置）—— 仅为内部实现依赖，故为 PRIVATE
#
#  注意：导入目标只在调用 find_package 的目录及其子目录可见，
#  本库与 NEXUS_WINDOW 是兄弟目录，因此必须各自独立查找。
# ==============================================================================
if(WIN32)
    target_include_directories(NEXUS_CORE
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../Dependencies/SDL/include
    )
    target_link_libraries(NEXUS_CORE
        PRIVATE
            SDL3
    )
else()
    find_package(SDL3 QUIET CONFIG)
    if(TARGET SDL3::SDL3)
        target_link_libraries(NEXUS_CORE PRIVATE SDL3::SDL3)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL3 REQUIRED IMPORTED_TARGET sdl3)
        target_link_libraries(NEXUS_CORE PRIVATE PkgConfig::SDL3)
    endif()
endif()
