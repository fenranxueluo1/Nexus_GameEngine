#include "Camera2D.h"

namespace NEXUS_RENDERING {
	Camera2D::Camera2D()
		: Camera2D(640, 480)
	{
	}

	Camera2D::Camera2D(int width, int height)
		: m_Width{ width }, m_Height{ height }, m_Scale{ 1.f }
		, m_Position{ glm::vec2{0} }, m_CameraMatrix{ 1.f }, m_OrthoProjection{ 1.f }, m_bNeedsUpdate{ true }
	{
		// 初始化正交投影
		// near=-1, far=1：确保 z=0 处的精灵映射到视锥内部（clip z=0），
		// 避免 2D 精灵正好落在 Vulkan 近裁剪面 (clip z=-w) 上被整体剔除。
		m_OrthoProjection = glm::ortho(
			0.f,							// 左边界
			static_cast<float>(m_Width),	// 右边界
			static_cast<float>(m_Height),	// 上边界
			0.f,							// 下边界
			-1.f,							// 近裁剪面
			1.f								// 远裁剪面
		);

		Update();
	}
	void Camera2D::Update()
	{
		if (!m_bNeedsUpdate)
			return;
		// 平移
		glm::vec3 translate{ -m_Position.x, -m_Position.y, 0.f };
		m_CameraMatrix = glm::translate(m_OrthoProjection, translate);

		// 缩放
		glm::vec3 scale{ m_Scale, m_Scale, 0.f };
		m_CameraMatrix *= glm::scale(glm::mat4(1.f), scale);

		m_bNeedsUpdate = false;
	}
}
