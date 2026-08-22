# Nexus

一个基于 C++23 的轻量级 2D 游戏引擎框架，使用 SDL3 作为窗口/事件系统，**Vulkan** 作为图形渲染后端（volk 函数加载 + VMA 内存管理 + shaderc 运行时着色器编译），基于 entt 实现 ECS 实体组件系统。项目采用模块化分层设计，方便扩展与复用。

## 功能特性

- **模块化架构**：窗口、工具、渲染、核心 ECS、日志各成独立静态库，职责清晰、可独立复用
- **ECS 实体组件系统**：基于 [entt](https://github.com/skypjack/entt)，提供 `Entity` / `Registry` 封装与内置组件（变换、精灵、标识）
- **Vulkan 2D 渲染**：`VulkanContext` 封装 Vulkan 实例 / 设备 / 交换链 / 图形管线 / 帧同步，`BatchRenderer` 基于 VMA 顶点缓冲 + 描述符纹理数组 + push constant 实现精灵批渲染
- **运行时着色器编译**：通过 shaderc 把 GLSL 编译为 SPIR-V（支持 Vulkan 1.4 / SPIR-V 1.6），无需预编译 `.spv` 文件
- **日志系统**：提供 `NEXUS_LOG` / `NEXUS_WARN` / `NEXUS_ERROR` 宏，基于 C++20 `<format>`，错误日志自动携带源位置（`source_location`）
- **SDL3 现代化封装**：基于 `std::unique_ptr` 与自定义删除器管理 SDL 资源（`SDL_Window`、`SDL_Gamepad`、`SDL_Cursor`），杜绝手动释放遗漏
- **跨平台构建**：使用 CMake 统一管理，兼容 MSVC 与 GCC/Clang

## 技术栈

| 组件 | 技术 |
| --- | --- |
| 语言标准 | C++23 |
| 构建系统 | CMake ≥ 4.4.2 |
| 窗口/事件 | SDL3（含 image / mixer / ttf 扩展） |
| 图形 API | Vulkan 1.4（volk 加载 + VMA 内存 + shaderc 编译） |
| 数学库 | glm |
| ECS | entt（header-only） |

## 环境要求

- **CMake** ≥ 4.4.2
- **C++23 编译器**：MSVC 19.40+ / GCC 13+ / Clang 16+
- **Vulkan SDK**（1.4+）：`find_package(Vulkan REQUIRED COMPONENTS volk shaderc_combined)`，SDK 需包含 volk、VMA、shaderc 组件
- **SDL3** 及扩展库：已随仓库附带在 `Dependencies/`（Windows）
- **Vulkan 驱动**：需硬件驱动支持

## 项目结构

```
Nexus/
├── CMakeLists.txt              # 顶层 CMake（find_package Vulkan，引入 NEXUS_EDITOR）
├── LICENSE                    # GPL-3.0
├── Dependencies/
│   ├── SDL/                   # SDL3 预编译库与头文件（include/ lib/）
│   ├── SDL3_image/            # SDL3 图像加载扩展（include/ lib/）
│   ├── SDL3_mixer/            # SDL3 音频混音扩展（include/ lib/）
│   ├── SDL3_ttf/              # SDL3 字体渲染扩展（include/ lib/）
│   ├── glm/                   # 数学库（header-only）
│   └── entt/                  # ECS 实体组件系统（header-only）
├── NEXUS_UTILITIES/           # 工具库（静态库）
│   └── Nexus_Utilities/
│       ├── SDL_Wrappers.h     # SDL 资源智能指针封装
│       └── SDL_Wrappers.cpp
├── NEXUS_WINDOW/              # 窗口库（静态库）
│   └── Windowing/Window/      # Window.h / Window.cpp（SDL3 Vulkan 窗口）
├── NEXUS_LOGGER/              # 日志库（静态库）
│   └── Logger/                # Logger.h / Logger.cpp / Logger.inl
├── NEXUS_RENDERING/           # 渲染库（静态库）
│   └── Rendering/
│       ├── Core/              # VulkanContext（Vulkan 核心） / BatchRenderer / Camera2D
│       └── Essentials/        # Shader / ShaderLoader / Texture / TextureLoader / Vertex
├── NEXUS_CORE/                # 核心库（静态库，ECS）
│   └── Core/
│       ├── ECS/               # Entity / Registry / Components
│       ├── Resources/         # AssetManager（纹理 / 着色器资源管理）
│       └── Systems/           # RenderSystem / ScriptingSystem
└── NEXUS_EDITOR/              # 可执行项目（编辑器入口）
    ├── src/                   # main.cpp / Application.cpp / Application.h
    ├── assets/                # 运行时资源（构建时自动拷贝到 exe 同目录）
    │   ├── shaders/           # basicShader.vert / basicShader.frag
    │   ├── scripts/           # main.lua（Lua 脚本）
    │   └── textures/          # tileset.png 等
    └── CMakeLists.txt
```

## 模块依赖关系

NEXUS_EDITOR 作为顶层可执行项目，会自动级联引入所有依赖：

```
NEXUS_EDITOR (executable)
   ├── NEXUS_WINDOW  (static lib)
   │     └── NEXUS_UTILITIES ───────────┐
   ├── NEXUS_RENDERING (static lib)     │
   │     ├── Vulkan::Vulkan / volk      ├── SDL3 (头文件 + lib)
   │     ├── Vulkan::shaderc_combined   │
   │     └── NEXUS_LOGGER               │
   ├── NEXUS_CORE  (static lib)         │
   │     └── NEXUS_RENDERING (见上)     │
   ├── NEXUS_LOGGER  (static lib)       │
   ├── glm / entt (header-only)         │
   └── SDL3 (lib)
```

依赖以 `PUBLIC` 方式传递：链接 `NEXUS_CORE` 即自动获得 entt、glm、渲染（含 Vulkan SDK 头文件）与日志的头文件路径。

## 快速开始

### 构建

#### Windows（PowerShell + MSVC）

```powershell
# 在仓库根目录执行
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

构建产物位于 `build/bin/`，静态库位于 `build/lib/`。构建后 SDL3 及扩展库的 dll、`assets/` 资源会被自动拷贝到可执行文件同目录。

### 运行

```powershell
./build/bin/Debug/NEXUS_EDITOR.exe
```

启动后会创建一个 640×480 的 Vulkan 窗口，使用 ECS 实体 + 精灵组件从 `tileset.png` 裁剪并渲染一个移动/旋转的精灵，按 `ESC` 或关闭窗口即可退出。

## 核心模块说明

### NEXUS_UTILITIES

提供 SDL3 资源的 RAII 封装，避免手动调用 `SDL_DestroyWindow` 等释放函数。

- `SDL_Destroyer`：自定义删除器，特化于 `SDL_Window*` / `SDL_Gamepad*` / `SDL_Cursor*`
- `WindowPtr`：`std::unique_ptr<SDL_Window, NEXUS_UTIL::SDL_Destroyer>` 的别名
- `Controller` / `Cursor`：基于 `std::shared_ptr` 的智能句柄

### NEXUS_WINDOW

封装 SDL3 窗口（使用 `SDL_WINDOW_VULKAN` 标志创建，供 Vulkan 表面使用）。

```cpp
NEXUS_WINDOWING::Window window(
    "测试窗口",
    640, 480,
    SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
    true,                       // 启用 VSync
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
);
```

主要接口：
- `GetWindow()` —— 获取原生 SDL 窗口句柄
- `GetWidth()` / `GetHeight()` / `GetXPos()` / `GetYPos()` —— 窗口几何信息
- `SetWindowName()` —— 动态修改窗口标题

### NEXUS_LOGGER

基于 C++20 `<format>` 的控制台日志系统，错误日志自动附加文件名、函数与行号（`std::source_location`）。

```cpp
NEXUS_INIT_LOGS(true, true);                                   // 初始化（控制台输出 + 保留历史）
NEXUS_LOG("加载纹理：[宽度 = {0}, 高度 = {1}]", w, h);          // 普通日志，支持格式化占位符
NEXUS_WARN("资源即将耗尽");                                     // 警告
NEXUS_ERROR("纹理创建失败！");                                  // 错误（自动带调用位置）
```

### NEXUS_RENDERING

封装 Vulkan 渲染相关组件：

- `VulkanContext`：Vulkan 核心上下文（单例）——实例 / 物理设备 / 逻辑设备 / 交换链 / 图形管线 / 描述符 / 帧同步。提供 `initialize()`、`beginFrame()` / `endFrame()`（帧循环）、`addTexture()`（纹理上传）等接口
- `Shader` / `ShaderLoader`：通过 shaderc 把 GLSL 编译为 SPIR-V 并创建图形管线，`SetUniformMat4("uProjection", ...)` 缓存相机矩阵（经 push constant 提交）
- `Texture` / `TextureLoader`：用 SDL3_image 解码图片，经 VulkanContext 上传为 GPU 纹理，返回描述符纹理数组索引
- `BatchRenderer`：精灵批渲染器——每帧独立顶点缓冲（避免双缓冲竞争）、静态索引缓冲、按纹理分组批量绘制（`vkCmdDrawIndexed`）
- `Camera2D`：2D 正交相机，`SetScale()` 缩放、`Update()` 刷新投影矩阵、`GetCameraMatrix()` 取矩阵（near=-1, far=1，避免精灵落在近裁剪面上被剔除）
- `Vertex`：顶点结构（`position` / `uvs` / `color`）

### NEXUS_CORE

基于 entt 的 ECS 实体组件系统：

```cpp
auto pRegistry = std::make_unique<NEXUS_CORE::ECS::Registry>();

NEXUS_CORE::ECS::Entity entity{*pRegistry, "Ent1", "Test"};

auto& transform = entity.AddComponent<NEXUS_CORE::ECS::TransformComponent>(
    {.position = glm::vec2{10.f, 10.f}, .scale = glm::vec2{1.f}, .rotation = 0.f});

auto& sprite = entity.AddComponent<NEXUS_CORE::ECS::SpriteComponent>(
    {.width = 16.f, .height = 16.f, .color = {.r = 255, .g = 255, .b = 255, .a = 255},
     .start_x = 0, .start_y = 1});

auto& id = entity.GetComponent<NEXUS_CORE::ECS::Identification>();
NEXUS_LOG("名称: {}, 分类: {}, ID: {}", id.name, id.group, id.entity_id);
```

内置组件：
- `TransformComponent` —— 位置（`position`）/ 缩放（`scale`）/ 旋转（`rotation`）
- `SpriteComponent` —— 精灵尺寸（`width` / `height`）、颜色、图集裁剪（`start_x` / `start_y` / `generate_uvs`）、纹理名（`texture_name`）、图层（`layer`）
- `Identification` —— 实体标识（`name` / `group` / `entity_id`）

### NEXUS_EDITOR

可执行入口（`NEXUS_EDITOR/src/main.cpp`），通过 `Application` 组合所有模块：

1. 初始化日志、SDL，创建 Vulkan 窗口
2. 初始化 `VulkanContext`（实例 / 设备 / 交换链 / 管线）
3. 创建 `AssetManager` 并加载纹理、着色器
4. 创建 ECS `Registry` 与实体（变换 + 精灵组件）
5. 初始化 Lua 脚本系统与渲染系统
6. 进入主循环：事件处理 → 逻辑更新 → `beginFrame` → 渲染 → `endFrame`

## 开发路线

- [x] ECS 实体组件系统（entt）
- [x] 日志系统
- [x] Vulkan 基础 2D 渲染（VulkanContext / BatchRenderer / Camera2D）
- [x] Lua 脚本系统（LuaBridge3 + Lua 5.5）
- [ ] 输入系统（`NEXUS_WINDOW/Windowing/Iputs/`，目前为预留目录）
- [ ] 3D 渲染 / 场景图
- [ ] 资源热加载与管线缓存
- [ ] 编辑器 UI（ImGui 集成）

## 许可证

本项目基于 [GNU General Public License v3.0](./LICENSE) 开源。

SDL3 的许可证详见 `Dependencies/SDL/LICENSE.txt`。