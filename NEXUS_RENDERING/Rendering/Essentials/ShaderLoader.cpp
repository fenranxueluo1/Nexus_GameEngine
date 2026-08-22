#include "ShaderLoader.h"
#include <Rendering/Core/VulkanContext.h>
#include <Logger/Logger.h>

namespace NEXUS_RENDERING {

    std::shared_ptr<Shader> ShaderLoader::Create(const std::string& vertexShaderPath, const std::string& fragmentShaderPath)
    {
        auto& vulkan = VulkanContext::Get();

        if (!vulkan.device())
        {
            NEXUS_ERROR("Vulkan 上下文尚未初始化，无法加载着色器!");
            return nullptr;
        }

        auto shader = vulkan.loadShaders(vertexShaderPath, fragmentShaderPath);
        if (!shader)
        {
            NEXUS_ERROR("无法加载着色器 [顶点: {0}] [片段: {1}]", vertexShaderPath, fragmentShaderPath);
            return nullptr;
        }

        return shader;
    }
}