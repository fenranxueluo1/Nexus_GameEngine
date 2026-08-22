#include "TextureLoader.h"
#include <Rendering/Core/VulkanContext.h>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <Logger/Logger.h>

namespace NEXUS_RENDERING {

    uint32_t TextureLoader::LoadTexture(const std::string& filepath, int& width, int& height, bool blended)
    {
        // 用 SDL3_image 解码图片为 SDL_Surface
        SDL_Surface* surface = IMG_Load(filepath.c_str());
        if (!surface)
        {
            const char* err = SDL_GetError();
            NEXUS_ERROR("无法加载纹理 [{}] -- {}", filepath, err);
            return 0;
        }

        // 统一转换为当前平台的 "RGBA 逐字节数组" 格式。
        // 注意：SDL 的 PACKEDORDER_RGBA 在小端机内存里实际是 A B G R，
        // 与 OpenGL 的 GL_RGBA/UNSIGNED_BYTE（按内存字节序 R,G,B,A）相反。
        // 因此不能用 SDL_PIXELFORMAT_RGBA8888，而要用 SDL_PIXELFORMAT_RGBA32——
        // 它正是 SDL 为 "当前平台的 RGBA 字节数组" 定义的别名（见 SDL_pixels.h 注释），
        // 与 VK_FORMAT_R8G8B8A8_SRGB 的字节序一致，可避免红蓝颠倒。
        SDL_Surface* converted = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if (!converted)
        {
            const char* err = SDL_GetError();
            NEXUS_ERROR("无法转换纹理格式 [{}] -- {}", filepath, err);
            return 0;
        }

        width = converted->w;
        height = converted->h;

        auto& vulkan = VulkanContext::Get();
        const uint32_t textureIndex = vulkan.addTexture(
            static_cast<unsigned char*>(converted->pixels),
            width, height,
            !blended
        );

        SDL_DestroySurface(converted);

        if (textureIndex == 0)
        {
            NEXUS_ERROR("无法上传纹理到 GPU [{}]", filepath);
        }

        return textureIndex;
    }

    std::shared_ptr<Texture> TextureLoader::Create(Texture::TextureType type, const std::string& texturePath)
    {
        int width = 0, height = 0;

        uint32_t textureIndex = 0;
        switch (type)
        {
        case Texture::TextureType::PIXEL:
            textureIndex = LoadTexture(texturePath, width, height, false);
            break;
        case Texture::TextureType::BLENDED:
            textureIndex = LoadTexture(texturePath, width, height, true);
            break;
        // TODO: 根据需要提供其它纹理类型以供加载 -- 例如 Framebuffer texture
        default:
            assert(false && "当前类型尚未定义，请使用已定义的纹理类型！");
            return nullptr;
        }

        if (textureIndex == 0)
        {
            return nullptr;
        }

        return std::make_shared<Texture>(textureIndex, width, height, type, texturePath);
    }

}