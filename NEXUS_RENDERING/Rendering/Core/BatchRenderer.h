#pragma once
#define VK_NO_PROTOTYPES
#include "../Essentials/Vertex.h"
#include "../Essentials/Shader.h"
#include "VulkanContext.h"
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <array>
#include <vector>
#include <memory>

namespace NEXUS_RENDERING {

	/*
	* @brief 基于 Vulkan 的精灵批渲染器。
	* 每个帧资源各拥有一份顶点缓冲（避免多帧并发时共享缓冲的读写竞争），
	* 索引缓冲为静态数据所有帧共享。每帧把精灵顶点写入当前帧对应的缓冲，
	* 再按纹理分组提交绘制命令。
	*/
	class BatchRenderer
	{
	private:
		struct Batch
		{
			uint32_t numIndices{ 0 }, offset{ 0 };
			uint32_t textureID{ 0 }; // 描述符纹理数组的 1-based 索引
		};

		struct Sprite
		{
			Vertex topLeft, bottomLeft, topRight, bottomRight;
			int layer;
			uint32_t textureID;
		};

		struct GPUBuffer
		{
			VkBuffer buffer{ VK_NULL_HANDLE };
			VmaAllocation allocation{ VK_NULL_HANDLE };
		};

	private:
		VulkanContext* m_pVulkan;

		std::array<GPUBuffer, VulkanContext::MaxFramesInFlight> m_VertexBuffers;
		GPUBuffer m_IndexBuffer;

		std::vector<std::shared_ptr<Sprite>> m_Sprites;
		std::vector<std::shared_ptr<Batch>> m_Batches;

	private:
		void Initialize();
		void GenerateBatches();
		GPUBuffer createBuffer(VkBufferUsageFlags usage, size_t byteSize) const;
		void writeBuffer(const GPUBuffer& buffer, size_t byteSize, const void* data) const;

	public:
		explicit BatchRenderer(VulkanContext* vulkan);
		~BatchRenderer();

		/*
		* @brief 清空当前批次与精灵，使其可以开始接收新的批次。
		*/
		void Begin();

		/*
		* @brief 检查是否存在需要生成批次的精灵。
		* 按 layer 对精灵排序，然后生成要渲染的批次。
		*/
		void End();

		/*
		* @brief 把当前所有批次的绘制命令记录到当前帧的命令缓冲中
		* （必须在 VulkanContext::beginFrame 与 endFrame 之间调用）。
		* @param shader 持有管线句柄与 "uProjection" push constant 数据的着色器。
		*/
		void Render(const Shader& shader);

		/*
		* @brief 向精灵列表添加一个新精灵。
		* @param spriteRect 精灵四边形的变换位置（glm::vec4）。
		* @param uvRect 当前精灵所用纹理的 UV（glm::vec4）。
		* @param textureID 描述符纹理数组索引（1-based，uint32_t）。
		* @param model 应用到精灵顶点的模型矩阵（glm::mat4）。
		* @param color 精灵要使用的颜色（Color）。
		*/
		void AddSprite(const glm::vec4& spriteRect, const glm::vec4 uvRect, uint32_t textureID, int layer = 0
			, glm::mat4 model = glm::mat4{ 1.f }, const Color& color = Color{ .r = 255, .g = 255, .b = 255, .a = 255 });
	};
}