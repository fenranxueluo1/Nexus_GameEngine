#include "Shader.h"
#include <Logger/Logger.h>

namespace NEXUS_RENDERING {

    Shader::Shader(VkPipeline pipeline, const std::string& vertexPath, const std::string& fragmentPath)
        : m_Pipeline{ pipeline }, m_sVertexPath{ vertexPath }, m_sFragmentPath{ fragmentPath }
    {

    }

    void Shader::SetUniformMat4(const std::string& name, glm::mat4& mat)
    {
        if (name == "uProjection")
        {
            m_Projection = mat;
        }
        else
        {
            NEXUS_WARN("Unknown uniform [{}] -- Vulkan 渲染目前仅支持 push constant: uProjection", name);
        }
    }

    void Shader::Enable()
    {
        // Vulkan 中管线在 VulkanContext::beginFrame 时绑定，无需额外操作
    }

    void Shader::Disable()
    {
    }

}