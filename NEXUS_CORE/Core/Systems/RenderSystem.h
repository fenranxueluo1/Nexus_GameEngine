#pragma once
#include "../ECS/Registry.h"
#include <Rendering/Core/BatchRenderer.h>

namespace NEXUS_CORE::Systems {
	class RenderSystem
	{
	private: 
		NEXUS_CORE::ECS::Registry& m_Registry;
		std::unique_ptr<NEXUS_RENDERING::BatchRenderer> m_pBatchRenderer;

	public:
		RenderSystem(NEXUS_CORE::ECS::Registry& registry);
		~RenderSystem() = default;

		/*
		* @brief 遍历注册表中所有同时拥有精灵与变换组件的实体，
		* 应用必要的变换并把它们加入批次进行渲染。
		* @note 必须在 VulkanContext::beginFrame 与 endFrame 之间调用。
		*/
		void Update();
	};
}