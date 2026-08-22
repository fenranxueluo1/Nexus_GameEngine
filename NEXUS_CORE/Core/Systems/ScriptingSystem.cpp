#include "ScriptingSystem.h"
#include "../ECS/Components/ScriptComponent.h"
#include "../ECS/Entity.h"
#include <Logger/Logger.h>
#include <lua.hpp>
#include <LuaBridge3/LuaBridge.h>

// IntelliSense 专用补丁（仅在 IDE 解析时生效，不影响编译）：
// 为 entt::internal::view_iterator 补充 operator!=，规避 VS Code IntelliSense
// 对 C++20 重写规则（!= 由 == 推导）支持不完整导致的误报。
#ifdef __INTELLISENSE__
namespace entt::internal {
template<typename Type, bool Checked, std::size_t Get, std::size_t Exclude>
[[nodiscard]] constexpr bool operator!=(const view_iterator<Type, Checked, Get, Exclude> &lhs,
                                        const view_iterator<Type, Checked, Get, Exclude> &rhs) noexcept {
    return !(lhs == rhs);
}
} // namespace entt::internal
#endif

namespace NEXUS_CORE::Systems {

	ScriptingSystem::ScriptingSystem(NEXUS_CORE::ECS::Registry& registry)
		: m_Registry(registry), m_bMainLoaded{ false }
	{
	}

	bool ScriptingSystem::LoadMainScript(lua_State* lua)
	{
		//执行 main.lua 脚本，出错时错误信息位于栈顶
		if (luaL_dofile(lua, "./assets/scripts/main.lua") != LUA_OK)
		{
			NEXUS_ERROR("加载lua脚本时出错: {}", lua_tostring(lua, -1));
			lua_pop(lua, 1);
			return false;
		}

		//获取全局表 main
		luabridge::LuaRef main_lua = luabridge::getGlobal(lua, "main");
		if (main_lua.isNil())
		{
			NEXUS_ERROR("main.lua文件中没有全局 main 表");
			return false;
		}

		//main[1] 为更新脚本表
		luabridge::LuaRef update_script = main_lua[1];
		if (!update_script.isTable())
		{
			NEXUS_ERROR("main.lua文件中没有更新方法");
			return false;
		}
		luabridge::LuaRef update = update_script["update"];

		//main[2] 为渲染脚本表
		luabridge::LuaRef render_script = main_lua[2];
		if (!render_script.isTable())
		{
			NEXUS_ERROR("main.lua文件中没有渲染方法");
			return false;
		}
		luabridge::LuaRef render = render_script["render"];

		NEXUS_CORE::ECS::Entity mainLuaScript{m_Registry, "main_script", ""};
		mainLuaScript.AddComponent<NEXUS_CORE::ECS::ScriptComponent>(lua);
		auto& script = mainLuaScript.GetComponent<NEXUS_CORE::ECS::ScriptComponent>();
		script.update = update;
		script.render = render;

		m_bMainLoaded = true;
		return true;
	}

	void ScriptingSystem::Update()
	{
		if (!m_bMainLoaded)
		{
			NEXUS_ERROR("主lua脚本尚未加载!");
			return;
		}

		auto view = m_Registry.GetRegistry().view<NEXUS_CORE::ECS::ScriptComponent>();

		for (const auto& entity : view)
		{
			NEXUS_CORE::ECS::Entity ent{m_Registry, entity};
			if (ent.GetName() != "main_script")
				continue;

			auto& script = ent.GetComponent<NEXUS_CORE::ECS::ScriptComponent>();
			try
			{
				script.update();
			}
			catch (const luabridge::LuaException& err)
			{
				NEXUS_ERROR("运行更新脚本时出现错误: {}", err.what());
			}
		}
	}

	void ScriptingSystem::Render()
	{
		if (!m_bMainLoaded)
		{
			NEXUS_ERROR("主lua脚本尚未加载!");
			return;
		}

		auto view = m_Registry.GetRegistry().view<NEXUS_CORE::ECS::ScriptComponent>();

		for (const auto& entity : view)
		{
			NEXUS_CORE::ECS::Entity ent{m_Registry, entity};
			if (ent.GetName() != "main_script")
				continue;

			auto& script = ent.GetComponent<NEXUS_CORE::ECS::ScriptComponent>();
			try
			{
				script.render();
			}
			catch (const luabridge::LuaException& err)
			{
				NEXUS_ERROR("运行渲染脚本时出现错误: {}", err.what());
			}
		}
	}
}
