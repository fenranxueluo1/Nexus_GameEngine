#pragma once
#include "Registry.h"

namespace luabridge { class LuaRef; }

struct lua_State;

namespace NEXUS_CORE::ECS {

	struct LuaState
	{
		lua_State* state{ nullptr };
	};

	class Entity
	{
	private:
		Registry& m_Registry;
		entt::entity m_Entity;
		std::string m_sName, m_sGroup;

	public:
		Entity(Registry& registry);
		Entity(Registry& registry, const std::string& name = "", const std::string& group = "");
		
		Entity(Registry& registry, const entt::entity& entity);
		~Entity() = default;

		inline const std::string& GetName() const { return m_sName; }
		inline const std::string& GetGroup() const { return m_sGroup; }

		/*
		* @brief 销毁底层的 entt::entity，将其从注册表中移除。
		* 谨慎使用！！请确保没有其它地方仍持有对该实体的引用，
		* 访问一个已不存在的实体会导致问题。
		* @return 返回被销毁实体的 std::uint32_t 编号。
		*/
		inline std::uint32_t Kill() { return m_Registry.GetRegistry().destroy(m_Entity); }

		/*
		* @brief 获取实际实体。
		* @return 返回底层的 entt::entity。
		*/
		inline entt::entity& GetEntity() { return m_Entity; }

		/*
		* @brief 所有实体都持有对创建它的注册表的引用。
		* @return 以引用方式返回底层的 entt::registry。
		*/
		inline entt::registry& GetRegistry() { return m_Registry.GetRegistry(); }

		static void CreateLuaEntityBind(lua_State* lua, Registry& registry);

		template <typename TComponent>
		static void RegisterMetaComponent();

		/*
		* @brief 为实体添加一个组件。
		* @tparam TComponent 组件类型，Args 为构造该组件所需的各种参数。
		* @return 返回所添加组件的引用。
		*/
		template <typename TComponent, typename ...Args>
		TComponent& AddComponent(Args&& ...args);

		template <typename TComponent, typename ...Args>
		TComponent& ReplaceComponent(Args&& ...args);

		template <typename TComponent>
		TComponent& GetComponent();

		/*
		* @brief 检查实体是否拥有指定组件。
		* @tparam 要检查的组件类型。
		* @return 拥有该组件返回 true，否则返回 false。
		*/
		template <typename TComponent>
		bool HasComponent();
		

		template <typename TComponent>
		auto RemoveComponent();
	};

	template <typename TComponent>
	auto add_component(Entity& entity, const luabridge::LuaRef& comp, LuaState state);

	template <typename TComponent>
	bool has_component(Entity& entity);

	template <typename TComponent>
	auto get_component(Entity& entity, LuaState state);

	template <typename TComponent>
	auto remove_component(Entity& entity);
}

#include "Entity.inl"
