#include "Application.h"

#include <SDL3/SDL.h>
#include <iostream>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <Rendering/Core/VulkanContext.h>
#include <Rendering/Essentials/ShaderLoader.h>
#include <Rendering/Essentials/TextureLoader.h>
#include <Rendering/Essentials/Vertex.h>
#include <Rendering/Core/Camera2D.h>
#include <Logger/Logger.h>
#include <Core/ECS/Entity.h>
#include <Core/ECS/Components/SpriteComponent.h>
#include <Core/ECS/Components/Identification.h>
#include <Core/ECS/Components/TransformComponent.h>
#include <Core/Resources/AssetManager.h>
#include <Core/Systems/ScriptingSystem.h>
#include <lua.hpp>
#include <LuaBridge3/LuaBridge.h>
#include <Core/Systems/RenderSystem.h>

namespace NEXUS_EDITOR {

    bool Application::Initialize()
    {
		NEXUS_INIT_LOGS(true, true);

    // SDL3 已移除 SDL_INIT_EVERYTHING，需显式列出要初始化的子系统
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        std::string error = SDL_GetError();
        NEXUS_ERROR("无法初始化SDL: {}", error);
        return false;
    }

    //创建窗口（Vulkan 需要 SDL_WINDOW_VULKAN 标志）
    m_pWindow = std::make_unique<NEXUS_WINDOWING::Window>("测试窗口", 640, 480, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, true, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    
    if (!m_pWindow->GetWindow())
    {
        NEXUS_ERROR("无法创建窗口！");
        return false;
    }

    //初始化 Vulkan 上下文（实例 / 设备 / 交换链 / 管线等）
    auto& vulkan = NEXUS_RENDERING::VulkanContext::Get();
    if (!vulkan.initialize(m_pWindow->GetWindow().get(), m_pWindow->GetWidth(), m_pWindow->GetHeight()))
    {
        NEXUS_ERROR("无法初始化 Vulkan 上下文!");
        return false;
    }

    auto assetManager = std::make_shared<NEXUS_RESOURCES::AssetManager>();
    if (!assetManager)
    {
    	NEXUS_ERROR("无法创建资产管理器!");
    	return false;
    }

    if (!assetManager->AddTexture("castle", "./assets/textures/tileset.png", true))
    {
    	NEXUS_ERROR("无法创建并添加纹理");
    	return false;
    }

	m_pRegistry = std::make_unique<NEXUS_CORE::ECS::Registry>();
	if (!m_pRegistry)
	{
		NEXUS_ERROR("无法创建注册表!");
		return false;
	}

	//把 Vulkan 上下文加入注册表上下文，供各系统（RenderSystem 构造时）使用
	if (!m_pRegistry->AddToContext<NEXUS_RENDERING::VulkanContext*>(&vulkan))
	{
		NEXUS_ERROR("无法添加 Vulkan 上下文到注册表上下文中！");
		return false;
	}

	//创建lua状态（LuaBridge3 基于原生 lua_State）
	auto lua = std::shared_ptr<lua_State>(luaL_newstate(), [](lua_State* L) { if (L) lua_close(L); });

	if (!lua)
	{
		NEXUS_ERROR("无法创建lua状态!");
		return false;
	}

	//打开 Lua 标准库（base、math、os、table、io、string 等）
	luaL_openlibs(lua.get());

	//启用异常，脚本运行出错时抛出 luabridge::LuaException
	luabridge::enableExceptions(lua.get());

	if (!m_pRegistry->AddToContext<std::shared_ptr<lua_State>>(lua))
	{
		NEXUS_ERROR("无法将lua状态添加到注册表上下文中!");
		return false;
	}
		
	auto scriptSystem = std::make_shared<NEXUS_CORE::Systems::ScriptingSystem>(*m_pRegistry);
	if (!scriptSystem)
	{
		NEXUS_ERROR("无法创建脚本系统!");
		return false;
	}
		
	if (!m_pRegistry->AddToContext<std::shared_ptr<NEXUS_CORE::Systems::ScriptingSystem>>(scriptSystem))
	{
		NEXUS_ERROR("无法将脚本系统添加到注册表上下文中!");
		return false;
	}

	auto renderSystem = std::make_shared<NEXUS_CORE::Systems::RenderSystem>(*m_pRegistry);
	if (!renderSystem)
	{
		NEXUS_ERROR("无法创建渲染系统!");
		return false;
	}

	if (!m_pRegistry->AddToContext<std::shared_ptr<NEXUS_CORE::Systems::RenderSystem>>(renderSystem))
	{
		NEXUS_ERROR("无法将渲染系统添加到注册表上下文中!");
		return false;
	}

    //创建临时相机
    auto camera = std::make_shared<NEXUS_RENDERING::Camera2D>();

	if (!m_pRegistry->AddToContext<std::shared_ptr<NEXUS_RESOURCES::AssetManager>>(assetManager))
	{
		NEXUS_ERROR("无法添加资产管理器到注册表上下文中！");
		return false;
	}

	if (!m_pRegistry->AddToContext<std::shared_ptr<NEXUS_RENDERING::Camera2D>>(camera))
	{
		NEXUS_ERROR("无法添加摄像机到注册表上下文中！");
		return false;
	}

	if (!LoadShaders())
	{
		NEXUS_ERROR("加载着色器失败！");
		return false;
	}

	NEXUS_CORE::Systems::ScriptingSystem::RegisterLuaBindings(lua.get(), *m_pRegistry);
		
	if (!scriptSystem->LoadMainScript(lua.get()))
	{
		NEXUS_ERROR("无法加载主lua脚本!");
		return false;
	}
	
    return true;
}

bool Application::LoadShaders()
{
	auto& assetManager = m_pRegistry->GetContext<std::shared_ptr<NEXUS_RESOURCES::AssetManager>>();

	if (!assetManager)
	{
		NEXUS_ERROR("无法从注册表中获取资产管理器！");
		return false;
	}

    if (!assetManager->AddShader("basic", "assets/shaders/basicShader.vert.spv",  "assets/shaders/basicShader.frag.spv"))
	{
		NEXUS_ERROR("无法添加着色器到资产管理器！");
		return false;
	}

	return true;
}

    void Application::ProcessEvents()
    {
		//处理事件
        while (SDL_PollEvent(&m_Event))
        {
            switch (m_Event.type)
            {
            case SDL_EVENT_QUIT:
                m_bIsRunning = false;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                NEXUS_RENDERING::VulkanContext::Get().resize(m_Event.window.data1, m_Event.window.data2);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (m_Event.key.key == SDLK_ESCAPE)
                {
                    m_bIsRunning = false;
                }
                break;
            default:
                break;
            }
        }
    }

    void Application::Update()
    {
		auto& camera = m_pRegistry->GetContext<std::shared_ptr<NEXUS_RENDERING::Camera2D>>();
		if (!camera)
		{
			NEXUS_ERROR("无法从注册表上下文中获取摄像机!");
			return;
		}
		
		camera->Update();

		auto& scriptSystem = m_pRegistry->GetContext<std::shared_ptr<NEXUS_CORE::Systems::ScriptingSystem>>();
		scriptSystem->Update();
    }

    void Application::Render()
    {
		auto& vulkan = NEXUS_RENDERING::VulkanContext::Get();

		// 开始 Vulkan 帧：获取交换链图像、清除颜色、开始动态渲染
		vulkan.beginFrame();

		auto& scriptSystem = m_pRegistry->GetContext<std::shared_ptr<NEXUS_CORE::Systems::ScriptingSystem>>();
		scriptSystem->Render();

		auto& renderSystem = m_pRegistry->GetContext<std::shared_ptr<NEXUS_CORE::Systems::RenderSystem>>();
		renderSystem->Update();

		// 结束 Vulkan 帧：提交命令并呈现
		vulkan.endFrame();
    }

    void Application::CleanUp()
    {
		// 先释放持有 VkBuffer / VkPipeline 的对象（渲染系统、资产管理器等），
		// 再销毁 Vulkan 上下文，避免访问已销毁的设备。
		if (m_pRegistry)
		{
			m_pRegistry->GetRegistry().ctx().clear();
		}

		NEXUS_RENDERING::VulkanContext::Get().shutdown();

		SDL_Quit();
    }

    Application::Application()
        : m_pWindow{nullptr}, m_pRegistry{nullptr}, m_Event{}, m_bIsRunning{true}
    {

    }

    Application& Application::GetInstance()
    {
		static Application app{};
		return app;
    }

    Application::~Application()
	{
    }

    void Application::Run()
    {
		if (!Initialize())
		{
			NEXUS_ERROR("初始化失败!");
			return;
		}

		while (m_bIsRunning)
		{
			ProcessEvents();
			Update();
			Render();
		}

		CleanUp();
    }
}