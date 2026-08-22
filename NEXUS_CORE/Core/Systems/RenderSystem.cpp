#include "RenderSystem.h"
#include "../Resources/AssetManager.h"
#include "../ECS/Components/SpriteComponent.h"
#include "../ECS/Components/TransformComponent.h"
#include <Rendering/Core/VulkanContext.h>
#include <Rendering/Core/Camera2D.h>
#include <Rendering/Essentials/Shader.h>
#include <Logger/Logger.h>

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

using namespace NEXUS_CORE::ECS;
using namespace NEXUS_RENDERING;
using namespace NEXUS_RESOURCES;

namespace NEXUS_CORE::Systems {
	RenderSystem::RenderSystem(NEXUS_CORE::ECS::Registry& registry)
		: m_Registry(registry), m_pBatchRenderer{nullptr}
	{
		auto& vulkan = m_Registry.GetContext<VulkanContext*>();
		m_pBatchRenderer = std::make_unique<BatchRenderer>(vulkan);
	}

	void RenderSystem::Update()
	{
		auto& camera = m_Registry.GetContext<std::shared_ptr<Camera2D>>();
		auto& assetManager = m_Registry.GetContext<std::shared_ptr<AssetManager>>();

		auto& spriteShader = assetManager->GetShader("basic");
		auto cam_mat = camera->GetCameraMatrix();

		if (!spriteShader.IsValid())
		{
			NEXUS_ERROR("精灵着色器程序设置错误!");
			return;
		}

		// 启用着色器
		spriteShader.Enable();
		spriteShader.SetUniformMat4("uProjection", cam_mat);

		m_pBatchRenderer->Begin();
		auto view = m_Registry.GetRegistry().view<SpriteComponent, TransformComponent>();

		for (const auto& entity : view)
		{
			const auto& transform = view.get<TransformComponent>(entity);
			const auto& sprite = view.get<SpriteComponent>(entity);

			if (sprite.texture_name.empty())
				continue;

			const auto& texture = assetManager->GetTexture(sprite.texture_name);
			if (texture.GetID() == 0)
			{
				NEXUS_ERROR("Texture [{0}] was not created correctly!", sprite.texture_name);
				return;
			}

			glm::vec4 spriteRect{transform.position.x, transform.position.y, sprite.width, sprite.height};
			glm::vec4 uvRect{sprite.uvs.u, sprite.uvs.v, sprite.uvs.uv_width, sprite.uvs.uv_height};

			glm::mat4 model{1.f};

			if (transform.rotation > 0.f || transform.rotation < 0.f ||
				transform.scale.x > 1.f || transform.scale.x < 1.f ||
				transform.scale.y > 1.f || transform.scale.y < 1.f)
			{
				model = glm::translate(model, glm::vec3{transform.position, 0.f});
				model = glm::translate(model, glm::vec3{ (sprite.width* transform.scale.x) * 0.5f, (sprite.height* transform.scale.y) * 0.5f, 0.f});

				model = glm::rotate(model, glm::radians(transform.rotation), glm::vec3{0.f, 0.f, 1.f});
				model = glm::translate(model, glm::vec3{ (sprite.width* transform.scale.x) * -0.5f, (sprite.height* transform.scale.y) * -0.5f, 0.f});

				model = glm::scale(model, glm::vec3{transform.scale, 1.f});

				model = glm::translate(model, glm::vec3{-transform.position, 0.f});
			}

			m_pBatchRenderer->AddSprite(spriteRect, uvRect, texture.GetID(), sprite.layer, model, sprite.color);
		}

		m_pBatchRenderer->End();
		m_pBatchRenderer->Render(spriteShader);

		spriteShader.Disable();
	}

}