#pragma once
#include <entt.hpp>

struct lua_State;

namespace NEXUS_CORE::ECS {
	class Registry
	{
	private:
		std::unique_ptr<entt::registry> m_pRegistry;
	public:
		Registry();
		~Registry() = default;

		/*
		* @brief 获取实际的注册表。
		* @return 返回底层的 entt::registry。
		*/
		inline entt::registry& GetRegistry() { return *m_pRegistry; }

		/*
		* @brief 创建一个新实体并将其加入注册表。
		* @return 返回新创建的 entt::entity。
		*/
		inline entt::entity CreateEntity() { return m_pRegistry->create(); }
		
		/*
		* @brief 上下文是一个通用映射容器，可存放任意类型的变量。
		* 变量必须是可移动且可构造的。
		* @tparam 模板参数是想要存放的变量类型，可以是任意类型。
		* 例如：std::shared_ptr<YourType>。
		* @return 返回新添加上下文的引用。
		*/
		template <typename TContext>
		TContext AddToContext(TContext context);

		/*
		* @brief 在上下文中查找指定类型变量并返回（若存在）。
		* @return 以引用方式返回目标上下文。
		* @throw 若给定上下文不存在或不可用则抛出异常。
		*/
		template <typename TContext>
		TContext& GetContext();

		static void CreateLuaRegistryBind(lua_State* lua, Registry& registry);

		template <typename TComponent>
		static void RegisterMetaComponent();
	};

	template <typename TComponent>
	entt::runtime_view& add_component_to_view(Registry* registry, entt::runtime_view& view);

	template <typename TComponent>
	entt::runtime_view& exclude_component_from_view(Registry* registry, entt::runtime_view& view);
}

#include "Registry.inl"
