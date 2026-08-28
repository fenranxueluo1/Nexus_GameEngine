cmake_minimum_required(VERSION 4.3)
project(NEXUS_LOGGER)

# ---------- C++ 标准 ----------
# 注意：必须在 add_library 之前设置，CMake 只在目标创建时读取该变量，
# 否则目标会退回到编译器默认标准（MSVC 为 C++14，无法使用 std::format / std::source_location）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# ---------- 目标（静态链接库） ----------
add_library(NEXUS_LOGGER STATIC)

# MSVC编译器 启用最新C++特性（必须在目标创建之后调用）
if(MSVC)
    target_compile_options(NEXUS_LOGGER PRIVATE /std:c++latest)
endif()

# 通过编译特性再次强制并向外传播 C++23（MSVC 下映射到 /std:c++latest）
target_compile_features(NEXUS_LOGGER PUBLIC cxx_std_23)

# ---------- 源文件 ----------
# Logger.ini 是模板实现（被 Logger.h 末尾 #include），属于头文件，不参与独立编译。
target_sources(NEXUS_LOGGER
    PRIVATE
        Logger/Logger.cpp
)

# ---------- 对外头文件 ----------
# PUBLIC：使用方通过 target_link_libraries(NEXUS_LOGGER) 即可获得这些头文件路径，
# 这样可写 #include <Logger/Logger.h>
target_include_directories(NEXUS_LOGGER
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

# ---------- 平台相关实现 ----------
# Logger 的控制台着色在源码内做了平台分支：
#   Windows : <Windows.h> / GetStdHandle / SetConsoleTextAttribute
#   Linux   : ANSI 转义序列
# 两者都只依赖系统自带的头文件与运行时，无需额外链接第三方库。
