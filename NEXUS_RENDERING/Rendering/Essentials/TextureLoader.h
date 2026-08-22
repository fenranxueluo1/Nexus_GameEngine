#pragma once
#include "Texture.h"
#include <memory>

namespace NEXUS_RENDERING {
	class TextureLoader
	{
	private:
		/*
		* @brief 用 SDL3_image 解码图片并通过 VulkanContext 上传为 GPU 纹理。
		* @return 返回描述符纹理数组的 1-based 索引，失败返回 0。
		*/
		static uint32_t LoadTexture(const std::string& filepath, int& width, int& height, bool blended);

	public:
		TextureLoader() = delete;
		static std::shared_ptr<Texture> Create(Texture::TextureType type, const std::string& texturePath);
	};
}