#pragma once
#include "Shader.h"
#include <memory>

namespace NEXUS_RENDERING {
    class ShaderLoader
    {
    public:
        ShaderLoader() = delete;

        /*
        * @brief 通过 VulkanContext 编译着色器并创建图形管线。
        * @return 返回包装了该管线的 Shader 对象，失败返回 nullptr。
        */
        static std::shared_ptr<Shader> Create(const std::string& vertexShaderPath, const std::string& fragmentShaderPath);
    };

}