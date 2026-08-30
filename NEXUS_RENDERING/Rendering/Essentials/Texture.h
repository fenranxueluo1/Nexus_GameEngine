#pragma once
#include <cstdint>
#include <string>

namespace NEXUS_RENDERING {
	/*
	* @brief 一张已上传到 GPU 的纹理。
	* 纹理数据由 VulkanContext 持有，本类只携带描述符纹理数组中的索引。
	*/
	class Texture
	{
	public:
		enum class TextureType { PIXEL = 0, BLENDED, NONE };

	private:
		uint32_t m_TextureIndex; // 1-based 索引，0 表示无效纹理
		int m_Width, m_Height;
		// 声明顺序必须与 Texture.cpp 初始化列表的顺序一致，否则会触发 -Wreorder
		TextureType m_eType;
		std::string m_sPath;

	public:
		Texture();
		Texture(uint32_t index, int width, int height, TextureType type = TextureType::PIXEL, const std::string& texturePath = "");
		~Texture() = default;

		inline int GetWidth() const { return m_Width; }
		inline int GetHeight() const { return m_Height; }
		inline uint32_t GetID() const { return m_TextureIndex; }

		void Bind();
		void Unbind();
	};
}