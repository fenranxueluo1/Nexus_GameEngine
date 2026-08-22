#include "Texture.h"

namespace NEXUS_RENDERING {

	Texture::Texture()
		: Texture(0, 0, 0, TextureType::NONE)
	{
	}

	Texture::Texture(uint32_t index, int width, int height, TextureType type, const std::string& texturePath)
		: m_TextureIndex{ index }, m_Width{ width }, m_Height{ height }, m_eType{ type }, m_sPath{ texturePath }
	{
	}

	void Texture::Bind()
	{
		// Vulkan 中纹理通过描述符集访问，无需绑定操作
	}

	void Texture::Unbind()
	{
	}
}