#include "BatchRenderer.h"
#include "VulkanContext.h"
#include <volk.h>
#include <algorithm>
#include <cstring>
#include <memory>

namespace NEXUS_RENDERING {
	constexpr size_t NUM_SPRITE_VERTICES = 4;
	constexpr size_t NUM_SPRITE_INDICES = 6;
	constexpr size_t MAX_SPRITES = 10000;
	constexpr size_t MAX_INDICES = MAX_SPRITES * NUM_SPRITE_INDICES;
	constexpr size_t MAX_VERTICES = MAX_SPRITES * NUM_SPRITE_VERTICES;

	BatchRenderer::GPUBuffer BatchRenderer::createBuffer(VkBufferUsageFlags usage, size_t byteSize) const
	{
		VkBufferCreateInfo bufferInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = byteSize,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		VmaAllocationCreateInfo allocInfo
		{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
		};
		GPUBuffer gpuBuffer;
		if (vmaCreateBuffer(m_pVulkan->allocator(), &bufferInfo, &allocInfo, &gpuBuffer.buffer, &gpuBuffer.allocation, nullptr) != VK_SUCCESS)
		{
			return GPUBuffer{};
		}
		return gpuBuffer;
	}

	void BatchRenderer::writeBuffer(const GPUBuffer& buffer, size_t byteSize, const void* data) const
	{
		void* ptr = nullptr;
		vmaMapMemory(m_pVulkan->allocator(), buffer.allocation, &ptr);
		std::memcpy(ptr, data, byteSize);
		vmaUnmapMemory(m_pVulkan->allocator(), buffer.allocation);
		// 非 HOST_COHERENT 内存需要显式刷新，才能让 GPU 看到新写入的数据
		vmaFlushAllocation(m_pVulkan->allocator(), buffer.allocation, 0, byteSize);
	}

	void BatchRenderer::Initialize()
	{
		// 每个帧资源各一份顶点缓冲，避免跨帧读写竞争
		for (GPUBuffer& vb : m_VertexBuffers)
		{
			vb = createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MAX_VERTICES * sizeof(Vertex));
		}

		// 索引缓冲（静态数据，所有帧共享）
		m_IndexBuffer = createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MAX_INDICES * sizeof(uint32_t));

		const uint32_t indices[NUM_SPRITE_INDICES]{ 0, 1, 2, 2, 3, 0 };
		auto indicesArr = std::make_unique<uint32_t[]>(MAX_INDICES);

		uint32_t offset{ 0 };
		for (size_t i = 0; i < MAX_INDICES; i += NUM_SPRITE_INDICES)
		{
			for (size_t j = 0; j < NUM_SPRITE_INDICES; j++)
				indicesArr[i + j] = indices[j] + offset;

			offset += NUM_SPRITE_VERTICES;
		}

		writeBuffer(m_IndexBuffer, MAX_INDICES * sizeof(uint32_t), indicesArr.get());
	}

	void BatchRenderer::GenerateBatches()
	{
		std::vector<Vertex> vertices;
		vertices.resize(m_Sprites.size() * NUM_SPRITE_VERTICES);

		int currentVertex{ 0 }, currentSprite{ 0 };
		uint32_t offset{ 0 }, prevTextureID{ 0 };

		for (const auto& sprite : m_Sprites)
		{
			if (currentSprite == 0)
				m_Batches.emplace_back(std::make_shared<Batch>(
					Batch{ .numIndices = static_cast<uint32_t>(NUM_SPRITE_INDICES), .offset = offset, .textureID = sprite->textureID }));
			else if (sprite->textureID != prevTextureID)
				m_Batches.emplace_back(std::make_shared<Batch>(
					Batch{ .numIndices = static_cast<uint32_t>(NUM_SPRITE_INDICES), .offset = offset, .textureID = sprite->textureID }));
			else
				m_Batches.back()->numIndices += static_cast<uint32_t>(NUM_SPRITE_INDICES);

			vertices[currentVertex++] = sprite->topLeft;
			vertices[currentVertex++] = sprite->topRight;
			vertices[currentVertex++] = sprite->bottomRight;
			vertices[currentVertex++] = sprite->bottomLeft;

			prevTextureID = sprite->textureID;
			offset += static_cast<uint32_t>(NUM_SPRITE_INDICES);
			currentSprite++;
		}

		// 把顶点数据写入当前帧的顶点缓冲
		const size_t byteSize = vertices.size() * sizeof(Vertex);
		const GPUBuffer& vb = m_VertexBuffers[m_pVulkan->currentFrameIndex()];
		writeBuffer(vb, byteSize, vertices.data());
	}

	BatchRenderer::BatchRenderer(VulkanContext* vulkan)
		: m_pVulkan{ vulkan }
		, m_Sprites{}, m_Batches{}
	{
		Initialize();
	}

	BatchRenderer::~BatchRenderer()
	{
		if (m_pVulkan && m_pVulkan->device())
		{
			for (const GPUBuffer& vb : m_VertexBuffers)
			{
				if (vb.buffer)
				{
					vkDestroyBuffer(m_pVulkan->device(), vb.buffer, nullptr);
					vmaFreeMemory(m_pVulkan->allocator(), vb.allocation);
				}
			}
			if (m_IndexBuffer.buffer)
			{
				vkDestroyBuffer(m_pVulkan->device(), m_IndexBuffer.buffer, nullptr);
				vmaFreeMemory(m_pVulkan->allocator(), m_IndexBuffer.allocation);
			}
		}
	}

	void BatchRenderer::Begin()
	{
		m_Sprites.clear();
		m_Batches.clear();
	}

	void BatchRenderer::End()
	{
		if (m_Sprites.empty())
			return;

		// 按图层对精灵进行排序
		std::sort(m_Sprites.begin(), m_Sprites.end(), [&](const auto& a, const auto& b)
			{
				return a->layer < b->layer;
			}
		);

		GenerateBatches();
	}

	void BatchRenderer::Render(const Shader& shader)
	{
		if (m_Batches.empty())
			return;

		VkCommandBuffer cmd = m_pVulkan->currentCommandBuffer();
		if (cmd == VK_NULL_HANDLE)
			return;

		// 管线与描述符集已在 VulkanContext::beginFrame 中绑定
		const GPUBuffer& vb = m_VertexBuffers[m_pVulkan->currentFrameIndex()];
		VkDeviceSize bufferOffset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &vb.buffer, &bufferOffset);
		vkCmdBindIndexBuffer(cmd, m_IndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

		PushConstants push;
		push.uProjection = shader.GetProjection();

		for (const auto& batch : m_Batches)
		{
			push.textureIndex = batch->textureID - 1; // 描述符数组为 0-based
			vkCmdPushConstants(cmd, m_pVulkan->pipelineLayout(),
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &push);
			vkCmdDrawIndexed(cmd, batch->numIndices, 1, batch->offset, 0, 0);
		}
	}

	void BatchRenderer::AddSprite(const glm::vec4& spriteRect, const glm::vec4 uvRect, uint32_t textureID, int layer, glm::mat4 model, const Color& color)
	{
		auto newSprite = std::make_shared<Sprite>(
			Sprite{
				.topLeft = Vertex {
					.position = model * glm::vec4{spriteRect.x, spriteRect.y + spriteRect.w, 0.f, 1.f},
					.uvs = glm::vec2{uvRect.x, uvRect.y + uvRect.w},
					.color = color
				},
				.bottomLeft = Vertex {
					.position = model * glm::vec4{spriteRect.x, spriteRect.y, 0.f, 1.f},
					.uvs = glm::vec2{uvRect.x, uvRect.y},
					.color = color
				},
				.topRight = Vertex {
					.position = model * glm::vec4{spriteRect.x + spriteRect.z, spriteRect.y + spriteRect.w, 0.f, 1.f},
					.uvs = glm::vec2{uvRect.x + uvRect.z, uvRect.y + uvRect.w},
					.color = color
				},
				.bottomRight = Vertex {
					.position = model * glm::vec4{spriteRect.x + spriteRect.z, spriteRect.y, 0.f, 1.f},
					.uvs = glm::vec2{uvRect.x + uvRect.z, uvRect.y},
					.color = color
				},
				.layer = layer,
				.textureID = textureID
			}
		);

		m_Sprites.push_back(std::move(newSprite));
	}
}