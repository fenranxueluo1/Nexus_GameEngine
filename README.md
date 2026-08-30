# Nexus

一个基于 C++23 的轻量级 2D 游戏引擎框架，使用 SDL3 作为窗口/事件系统，**Vulkan** 作为图形渲染后端（volk 函数加载 + VMA 内存管理 + glslc 离线着色器编译），基于 entt 实现 ECS 实体组件系统。项目采用模块化分层设计，方便扩展与复用。

**支持 Windows 与 Linux 双平台**，CMake 会自动根据平台选择依赖来源：Windows 使用仓库内随附的预编译库，Linux 使用系统安装的库。

## 功能特性

- **模块化架构**：窗口、工具、渲染、核心 ECS、日志各成独立静态库，职责清晰、可独立复用
- **ECS 实体组件系统**：基于 [entt](https://github.com/skypjack/entt)，提供 `Entity` / `Registry` 封装与内置组件（变换、精灵、标识、动画、脚本）
- **Vulkan 2D 渲染**：`VulkanContext` 封装 Vulkan 实例 / 设备 / 交换链 / 图形管线 / 帧同步，`BatchRenderer` 基于 VMA 顶点缓冲 + 描述符纹理数组 + push constant 实现精灵批渲染
- **着色器离线编译**：构建时通过 `glslc` 把 GLSL 预编译为 SPIR-V（支持 Vulkan 1.4 / SPIR-V 1.6），运行时直接加载 `.spv`，无需链接平台专用的 shaderc 库
- **日志系统**：提供 `NEXUS_LOG` / `NEXUS_WARN` / `NEXUS_ERROR` 宏，基于 C++20 `<format>`，错误日志自动携带源位置（`source_location`），控制台输出按级别着色（Windows 用 Win32 控制台属性，Linux 用 ANSI 转义序列）
- **输入系统**：`Keyboard` / `Mouse` / `Gamepad` 三套状态机 + `InputManager` 统一派发 SDL 事件，支持按键按下/抬起、鼠标滚轮与移动、手柄热插拔
- **精灵动画系统**：`AnimationSystem` 按帧率推进 `AnimationComponent` 的 `currentFrame`，支持横向与纵向（`bVertical`）图集，自动计算 UV 偏移
- **Lua 脚本系统**：LuaBridge3 绑定 ECS 与 glm，实体可通过 `ScriptComponent` 挂载脚本，键鼠 / 手柄输入亦向 Lua 暴露
- **SDL3 现代化封装**：基于 `std::unique_ptr` / `std::shared_ptr` 与自定义删除器管理 SDL 资源（`SDL_Window` / `SDL_Gamepad` / `SDL_Cursor`），删除器内部分别调用 `SDL_DestroyWindow` / `SDL_CloseGamepad` / `SDL_DestroyCursor`，杜绝手动释放遗漏
- **跨平台构建**：Windows（MSVC / MinGW）与 Linux（GCC / Clang）双平台支持，各子项目的 CMakeLists 内以 `if(WIN32)` / `else()` 分别配置依赖，平台差异就地可见

## 技术栈

| 组件 | 技术 |
| --- | --- |
| 语言标准 | C++23 |
| 构建系统 | CMake ≥ 4.3 |
| 窗口/事件 | SDL3（含 image / mixer / ttf 扩展） |
| 图形 API | Vulkan 1.4 |
| Vulkan 函数加载 | volk（`Dependencies/volk/`，随仓库提供） |
| GPU 内存管理 | Vulkan Memory Allocator 3.4.0（`Dependencies/vma/`，随仓库提供） |
| 着色器编译 | glslc（构建期，输出 SPIR-V） |
| 数学库 | glm |
| ECS | entt（header-only） |
| 脚本 | Lua 5.5 + LuaBridge3 |

## 环境要求

- **CMake** ≥ 4.3
- **C++23 编译器**：MSVC 19.40+ / GCC 13+ / Clang 16+
- **Vulkan SDK**（1.4+）：`find_package(Vulkan REQUIRED COMPONENTS volk glslc)`，需要 SDK 提供 `volk` 与 `glslc`
  - **VMA 不需要单独安装**，头文件已随仓库提供在 `Dependencies/vma/`
- **SDL3** 及扩展库：
  - Windows：已随仓库附带在 `Dependencies/`
  - Linux：由系统包管理器提供（见下方「各平台依赖」）
- **Vulkan 驱动**：需硬件驱动支持

### 各平台依赖

| 依赖 | Windows | Linux |
| --- | --- | --- |
| Vulkan 头文件 / loader | 官方安装包（自动设置 `VULKAN_SDK`） | Arch / CachyOS：`vulkan-devel`（包组）；其它发行版：官方 SDK |
| volk（函数加载） | 官方安装包 | Arch / CachyOS：`volk`（已包含在 `vulkan-devel` 包组内） |
| glslc（着色器编译） | 官方安装包 | Arch / CachyOS：`shaderc`；其它发行版：官方 SDK |
| VMA（GPU 内存管理） | `Dependencies/vma/`（随仓库） | 同左 |
| SDL3 / SDL3_image | `Dependencies/` 预编译库 | 系统库（**必需**） |
| SDL3_mixer / SDL3_ttf | `Dependencies/` 预编译库 | 系统库（可选，缺失时跳过并给出提示） |
| Lua | `Dependencies/Lua_5.5`（5.5） | 系统库，CMake 依次尝试 `lua5.5` / `lua-5.5` / `lua55` / `lua` / `lua5.4` / `lua-5.4` / `lua54` |
| glm / entt / LuaBridge3 / volk / vma | `Dependencies/`（header-only） | 同左 |

Linux（Arch / CachyOS）安装系统依赖：

```bash
sudo pacman -S sdl3 sdl3_image sdl3_mixer sdl3_ttf lua vulkan-devel shaderc
```

Linux（Fedora）安装系统依赖：

```bash
sudo dnf install SDL3-devel SDL3_image-devel SDL3_mixer-devel SDL3_ttf-devel lua-devel
```

Linux（Debian/Ubuntu）：

```bash
sudo apt install libsdl3-dev libsdl3-image-dev libsdl3-mixer-dev libsdl3-ttf-dev
```

> **Arch / CachyOS 无需设置 `VULKAN_SDK`**：`vulkan-devel` 与 `volk` 把头文件装到 `/usr/include`、库装到 `/usr/lib`，`find_package(Vulkan COMPONENTS volk glslc)` 能直接找到，不必 `source setup-env.sh`。
>
> 其它发行版若没有打包 volk（例如 Debian 的 `libvolk-dev` 是 GNU Radio 的 VOLK，**不是** Vulkan 的 volk），请安装 LunarG 官方 SDK 并在构建前 `source <VulkanSDK>/setup-env.sh`，否则配置阶段会报缺少 volk 组件。

> **为何每个子项目都要各自 `find_package`**：CMake 的导入目标（如 `SDL3::SDL3`、`PkgConfig::SDL3`）只在**调用查找命令的目录及其子目录**内可见。而 `NEXUS_WINDOW` / `NEXUS_RENDERING` / `NEXUS_CORE` / `NEXUS_EDITOR` 是通过 `NEXUS_EDITOR` 平级引入的**兄弟目录**，彼此看不到对方查到的目标，因此必须各自查找。重复调用 `find_package` 与 `pkg_check_modules` 是安全的（内部有缓存，不会重复创建目标）。Vulkan 是在顶层 `add_subdirectory` 之前查找的，所以 `Vulkan::Vulkan` / `Vulkan::volk` / `Vulkan::glslc` 对所有子项目可见。

> **为何要 pkg-config 回退**：并非所有库都提供 CMake Config 包（例如 Fedora 上的 SDL3_mixer 只有 pkg-config），保留回退路径可避免因缺少 config 包而中断配置。

> **为何关闭 `-Wmissing-field-initializers`**：Vulkan 代码普遍用 C++ 指定初始化器写 `VkXxxCreateInfo{ .sType = ..., .foo = ... }`，按标准被省略的成员会被零初始化，但 GCC / Clang 仍会逐字段告警，属于误报。该警告在顶层 `CMakeLists.txt` 中关掉，第三方库（`Dependencies/`）则以 `SYSTEM` 方式引入以隔离其内部告警。

## 项目结构

```
Nexus/
├── CMakeLists.txt              # 顶层 CMake（find_package Vulkan，引入 NEXUS_EDITOR）
├── CMakePresets.json           # configure / build / workflow 预设
├── .zed/                       # Zed 编辑器构建与调试任务
├── LICENSE                     # GPL-3.0
├── Dependencies/
│   ├── SDL/                    # SDL3 预编译库与头文件（include/ lib/）
│   ├── SDL3_image/             # SDL3 图像加载扩展（include/ lib/）
│   ├── SDL3_mixer/             # SDL3 音频混音扩展（include/ lib/）
│   ├── SDL3_ttf/               # SDL3 字体渲染扩展（include/ lib/）
│   ├── Lua_5.5/                # Lua 5.5（Windows 用，include/ lib/）
│   ├── LuaBridge3/             # Lua 绑定层（header-only）
│   ├── glm/                    # 数学库（header-only）
│   ├── entt/                   # ECS 实体组件系统（header-only）
│   ├── volk/                   # Vulkan 函数加载器（header-only，随仓库）
│   └── vma/                    # Vulkan Memory Allocator（header-only，随仓库）
├── NEXUS_UTILITIES/            # 工具库（静态库）
│   └── Nexus_Utilities/
│       ├── SDL_Wrappers.h      # SDL 资源智能指针封装
│       └── SDL_Wrappers.cpp
├── NEXUS_WINDOW/               # 窗口库（静态库）
│   ├── Windowing/Window/       # Window.h / Window.cpp（SDL3 Vulkan 窗口）
│   └── Windowing/Inputs/       # Keyboard / Mouse / Gamepad / Button + 按键枚举
├── NEXUS_LOGGER/               # 日志库（静态库）
│   └── Logger/                 # Logger.h / Logger.cpp / Logger.inl
├── NEXUS_RENDERING/            # 渲染库（静态库）
│   └── Rendering/
│       ├── Core/               # VulkanContext（Vulkan 核心） / BatchRenderer / Camera2D
│       └── Essentials/         # Shader / ShaderLoader / Texture / TextureLoader / Vertex
├── NEXUS_CORE/                 # 核心库（静态库，ECS）
│   └── Core/
│       ├── ECS/                # Entity / Registry / Components / MetaUtilities
│       ├── Resources/          # AssetManager（纹理 / 着色器资源管理）
│       ├── Scripting/          # InputManager / GlmLuaBindings
│       └── Systems/            # RenderSystem / ScriptingSystem / AnimationSystem
└── NEXUS_EDITOR/               # 可执行项目（编辑器入口）
    ├── src/                    # main.cpp / Application.cpp / Application.h
    ├── assets/                 # 运行时资源（构建时自动拷贝到 exe 同目录）
    │   ├── shaders/            # basicShader.vert / basicShader.frag
    │   ├── scripts/            # main.lua（Lua 脚本）
    │   └── textures/           # tileset.png 等
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
   │     ├── Vulkan::glslc (build-time) │
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

项目提供 CMake Preset，推荐使用（无需手动指定生成器与构建类型）：

```bash
# Linux：配置 + 生成（一步到位，未配置过时会自动配置）
cmake --workflow --preset linux-debug

# 只想生成，不重新配置
cmake --build --preset linux-debug
```

可用的 preset：

| Preset | 作用 |
| --- | --- |
| `linux-debug` / `windows-debug` | 配置 + 生成 |
| `linux-debug-run` / `windows-debug-run` | 配置 + 生成 + 启动编辑器（依赖 `NEXUS_RUN` 目标） |

#### Windows（PowerShell + MSVC）

```powershell
# 在仓库根目录执行
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

#### Linux（GCC / Clang）

```bash
# Arch / CachyOS 装好系统依赖后无需额外准备；
# 使用 LunarG 官方 SDK 的发行版需先让 VULKAN_SDK 指向 SDK 根目录
source ~/vulkanSDK/1.4.357.1/x86_64/setup-env.sh

# 在仓库根目录执行
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

构建产物位于 `build/bin/`，静态库位于 `build/lib/`。

- **Windows**：构建后 SDL3 及扩展库的 dll、`assets/` 资源会被自动拷贝到可执行文件同目录
- **Linux**：SDL3 等由系统提供，无需拷贝；仅拷贝 `assets/`，并由 `glslc` 把 `assets/shaders` 下的着色器编译为 `.spv`

### 运行

```powershell
# Windows
./build/bin/Debug/NEXUS_EDITOR.exe
```

```bash
# Linux
./build/bin/NEXUS_EDITOR
```

也可通过 CMake 目标一步完成「构建并运行」：

```bash
cmake --build --preset linux-debug --target NEXUS_RUN
```

启动后会创建一个 640×480 的 Vulkan 窗口，使用 ECS 实体 + 精灵组件从 `tileset.png` 裁剪并渲染一个移动/旋转的精灵，按 `ESC` 或关闭窗口即可退出。

### Zed 编辑器

仓库内 `.zed/tasks.json` 提供四个任务：配置、生成、编译并运行、仅运行。它们直接调用 `cmake --preset` / `cmake --workflow --preset`，**不经过 shell**。

> **为何不用 `sh -c` 串联命令**：Zed 会把 task 的参数用空格拼接后交给 shell，任何含空格或 `&&` / `||` / `[]` 的脚本都会被拆碎（表现为 `-f: 第 1 行：[: 缺少 "]"`）。因此「编译 && 运行」由 CMake 的 `NEXUS_RUN` 目标承担，而不是写在任务脚本里。

## 核心模块说明

### NEXUS_UTILITIES

提供 SDL3 资源的 RAII 封装，避免手动调用 `SDL_DestroyWindow` 等释放函数。

- `SDL_Destroyer`：自定义删除器，特化于 `SDL_Window*` / `SDL_Gamepad*` / `SDL_Cursor*`，分别调用 `SDL_DestroyWindow` / `SDL_CloseGamepad` / `SDL_DestroyCursor`
- `WindowPtr`：`std::unique_ptr<SDL_Window, NEXUS_UTIL::SDL_Destroyer>` 的别名
- `Controller` / `Cursor`：基于 `std::shared_ptr` 的智能句柄，由 `make_shared_controller()` / `make_shared_cursor()` 创建（内部同样挂接 `SDL_Destroyer`）

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
- `SetXPos()` / `SetYPos()` —— 修改窗口位置
- `SetWindowName()` —— 动态修改窗口标题

> 默认构造（`Window()`）创建 640×480 的窗口并沿用默认的 `SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE` 标志组合。

同目录下的 `Windowing/Inputs/` 提供输入状态机：

- `Keyboard`：`OnKeyPressed` / `OnKeyReleased` 记录状态，`IsKeyPressed` / `IsKeyJustPressed` / `IsKeyJustReleased` 查询，按键见 `Keys.h`
- `Mouse`：按键同构（`OnBtnPressed` / `IsBtnPressed` / `IsBtnJustPressed` …），另有滚轮增量 `SetMouseWheelX/Y` 与移动状态 `IsMouseMoving`，按键见 `MouseButtons.h`
- `Gamepad`：手柄按键与连接管理，按键见 `GPButtons.h`

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

- `VulkanContext`：Vulkan 核心上下文（单例）——实例 / 物理设备 / 逻辑设备 / 交换链 / 图形管线 / 描述符 / 帧同步。提供 `initialize()`、`beginFrame()` / `endFrame()`（帧循环）、`addTexture()`（纹理上传）等接口。volk 的实现由本模块的 `VulkanContext.cpp` 以 `VOLK_IMPLEMENTATION` 编译进静态库，VMA 同理（`VMA_IMPLEMENTATION`），二者均为 header-only 库、无需额外链接
- `Shader` / `ShaderLoader`：加载构建期由 `glslc` 预编译好的 SPIR-V（`.spv`）并创建图形管线，`SetUniformMat4("uProjection", ...)` 缓存相机矩阵（经 push constant 提交）
- `Texture` / `TextureLoader`：用 SDL3_image 解码图片，经 VulkanContext 上传为 GPU 纹理，返回描述符纹理数组索引
- `BatchRenderer`：精灵批渲染器——每帧独立顶点缓冲（避免双缓冲竞争）、静态索引缓冲、按纹理分组批量绘制（`vkCmdDrawIndexed`）
- `Camera2D`：2D 正交相机，`SetScale()` 缩放、`Update()` 刷新投影矩阵、`GetCameraMatrix()` 取矩阵（near=-1, far=1，避免精灵落在近裁剪面上被剔除）
- `Vertex`：顶点结构（`position` / `uvs` / `color`）

> `Dependencies/` 以 `SYSTEM` 方式引入，因此 volk / VMA / glm 等第三方头文件的告警不会污染构建输出；项目自身代码在 GCC / Clang 下保持零警告。

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
- `AnimationComponent` —— 帧动画（`numFrames` / `frameRate` / `frameOffset` / `currentFrame` / `bVertical`）
- `ScriptComponent` —— 暴露 Lua 类型，配合 `ScriptingSystem` 挂载脚本

内置系统：
- `RenderSystem` —— 遍历同时具有 `SpriteComponent` 与 `TransformComponent` 的实体（跳过未指定纹理的），把位置 / 尺寸 / UV 提交给 `BatchRenderer`
- `AnimationSystem` —— 按帧率推进 `AnimationComponent` 的 `currentFrame`（对 `numFrames` 取模循环）并更新精灵 UV
- `ScriptingSystem` —— 驱动 Lua 脚本生命周期；输入与数学库分别由 `InputManager::CreateLuaInputBindings` 与 `GlmLuaBindings::CreateGLMBindings` 注册到 Lua

### NEXUS_EDITOR

可执行入口（`NEXUS_EDITOR/src/main.cpp`），通过 `Application` 组合所有模块：

1. 初始化日志、SDL，创建 Vulkan 窗口
2. 初始化 `VulkanContext`（实例 / 设备 / 交换链 / 管线）
3. 创建 `AssetManager` 并加载纹理、着色器
4. 创建 ECS `Registry` 与实体（变换 + 精灵 + 动画组件）
5. 初始化 Lua 脚本系统与渲染系统
6. 进入主循环：事件处理（键盘 / 鼠标 / 手柄）→ 逻辑更新 → `beginFrame` → 渲染 → `endFrame`

## 开发路线

- [x] ECS 实体组件系统（entt）
- [x] 日志系统
- [x] Vulkan 基础 2D 渲染（VulkanContext / BatchRenderer / Camera2D）
- [x] Lua 脚本系统（LuaBridge3 + Lua 5.5）
- [x] 输入系统（Keyboard / Mouse / Gamepad + InputManager）
- [x] 精灵动画系统（AnimationComponent + AnimationSystem）
- [ ] 3D 渲染 / 场景图
- [ ] 资源热加载与管线缓存
- [ ] 编辑器 UI（ImGui 集成）

## 许可证

本项目基于 [GNU General Public License v3.0](./LICENSE) 开源。

SDL3 的许可证详见 `Dependencies/SDL/LICENSE.txt`。
