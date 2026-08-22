#pragma once
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <string>
#include <glm/glm.hpp>

namespace NEXUS_RENDERING
{
    /*
    * @brief Vulkan 图形渲染管线的一个轻量包装。
    * 管线由 VulkanContext 创建并持有，本类只负责携带管线句柄与
    * “uProjection” 等 uniform 值（Vulkan 中使用 push constant 传递）。
    */
    class Shader
    {
    private:
        VkPipeline m_Pipeline;
        std::string m_sVertexPath, m_sFragmentPath;
        glm::mat4 m_Projection{ 1.f };

    public:
        Shader() : m_Pipeline{ VK_NULL_HANDLE } {}
        Shader(VkPipeline pipeline, const std::string& vertexPath, const std::string& fragmentPath);
        ~Shader() = default;

        /*
        * @brief 存储 uniform 值。Vulkan 中对应的 push constant 会在绘制时
        * 由 BatchRenderer 一次性提交，因此这里仅缓存数值。
        */
        void SetUniformMat4(const std::string& name, glm::mat4& mat);

        void Enable();
        void Disable();

        inline const VkPipeline GetPipeline() const { return m_Pipeline; }
        inline const glm::mat4& GetProjection() const { return m_Projection; }
        inline bool IsValid() const { return m_Pipeline != VK_NULL_HANDLE; }
    };
}