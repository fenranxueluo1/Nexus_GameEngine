#pragma once

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace NEXUS_RENDERING {

	class Shader;

	// 单帧使用的命令缓冲 / 信号量资源
	struct FrameResources
	{
		VkCommandPool commandPool = nullptr;
		VkCommandBuffer commandBuffer = nullptr;
		VkSemaphore imageAcquiredSemaphore = nullptr;
	};

	// GPU 图像（纹理）资源
	struct GPUImage
	{
		VkImage image = nullptr;
		VkImageView imageView = nullptr;
		VmaAllocation allocation = nullptr;
	};

	// 顶点着色器 / 片段着色器共享的 push constant 数据
	struct PushConstants
	{
		glm::mat4 uProjection{ 1.f };
		uint32_t textureIndex = 0;
	};

	/*
	* @brief 封装 Vulkan 核心上下文（实例 / 物理设备 / 逻辑设备 / 交换链 /
	* 渲染管线 / 帧同步），负责驱动整个 Vulkan 帧循环。
	* 写法参考 Vulkan_GameEngine 与 modern-vulkan-lesson_02。
	*/
	class VulkanContext
	{
	public:
		constexpr static uint32_t VulkanVersion{ VK_API_VERSION_1_4 };
		constexpr static uint32_t MaxFramesInFlight{ 2 };
		constexpr static uint32_t MaxTextures{ 1024 };
		constexpr static VkFormat SwapchainFormat{ VK_FORMAT_B8G8R8A8_SRGB };
		constexpr static VkFormat DepthFormat{ VK_FORMAT_D32_SFLOAT };

		static VulkanContext& Get();

		bool initialize(SDL_Window* window, uint32_t width, uint32_t height);
		void shutdown();

		/*
		* @brief 编译着色器并创建图形渲染管线。
		* @param vertexPath 顶点着色器文件路径（SPIR-V 二进制，构建时用 glslc 预编译）。
		* @param fragmentPath 片段着色器文件路径。
		* @return 返回包装了该管线的 Shader 对象，失败返回 nullptr。
		*/
		std::shared_ptr<Shader> loadShaders(const std::string& vertexPath, const std::string& fragmentPath);

		// 帧循环：beginFrame 获取交换链图像并开始动态渲染，endFrame 提交并呈现
		void beginFrame();
		void endFrame();

		/*
		* @brief 更新内部窗口尺寸，并标记交换链需要重建（下次 beginFrame 时生效）。
		*/
		void resize(uint32_t width, uint32_t height);

		/*
		* @brief 将像素数据上传为 GPU 纹理并注册到描述符纹理数组。
		* @param pixels RGBA 8bit 像素数据。
		* @return 返回 1-based 纹理索引（0 表示失败），BatchRenderer 用它索引纹理。
		*/
		uint32_t addTexture(unsigned char* pixels, uint32_t width, uint32_t height, bool pixelArt);

		// ---------- 访问器 ----------
		inline VkDevice device() const { return m_device; }
		inline VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
		inline VkQueue graphicsQueue() const { return m_gfxQueue; }
		inline uint32_t graphicsQueueFamilyIndex() const { return m_gfxQueueFamIdx; }
		inline VmaAllocator allocator() const { return m_vmaAllocator; }
		inline VkCommandBuffer currentCommandBuffer() const { return m_frameResources[m_frameResIndex].commandBuffer; }
		inline uint32_t currentFrameIndex() const { return m_frameResIndex; }
		inline VkPipeline pipeline() const { return m_pipeline; }
		inline VkPipelineLayout pipelineLayout() const { return m_pipelineLayout; }
		inline VkDescriptorSet descriptorSet() const { return m_globalDescSet; }
		inline uint32_t swapchainWidth() const { return m_swapchainWidth; }
		inline uint32_t swapchainHeight() const { return m_swapchainHeight; }

	private:
		SDL_Window* m_window = nullptr;
		uint32_t m_width = 1280;
		uint32_t m_height = 720;
		uint64_t m_frameIndex = 0;
		uint64_t m_nextSignalValue = MaxFramesInFlight + 1;
		uint32_t m_frameResIndex = 0;
		uint32_t m_imageIndex = 0;
		bool m_frameActive = false;

		// Vulkan 核心对象
		VkInstance m_vulkanInstance = nullptr;
		VkPhysicalDevice m_physicalDevice = nullptr;
		VkDevice m_device = nullptr;
		VkSurfaceKHR m_surface = nullptr;
		VmaAllocator m_vmaAllocator = nullptr;

		// 队列相关
		uint32_t m_gfxQueueFamIdx = UINT32_MAX;
		VkQueue m_gfxQueue = nullptr;
		VkCommandPool m_transientCommandPool = nullptr; // 一次性上传命令缓冲池

		// 交换链相关
		VkSwapchainKHR m_swapchain = nullptr;
		std::vector<VkImage> m_swapchainImages;
		std::vector<VkImageView> m_swapchainImageViews;
		std::vector<VkSemaphore> m_renderCompleteSemaphores;
		bool m_requireSwapchainRecreate = false;
		uint32_t m_swapchainWidth = 0;
		uint32_t m_swapchainHeight = 0;

		// 深度缓冲
		VkImage m_depthImage = nullptr;
		VkImageView m_depthImageView = nullptr;
		VmaAllocation m_depthImageAllocation = nullptr;

		// 图形渲染管线相关
		VkPipelineLayout m_pipelineLayout = nullptr;
		VkPipeline m_pipeline = nullptr;

		// 着色器资源
		VkShaderModule m_vertShader = nullptr;
		VkShaderModule m_fragShader = nullptr;

		// 描述符
		VkDescriptorSetLayout m_globalDSLayout = nullptr;
		VkDescriptorPool m_descPool = nullptr;
		VkDescriptorSet m_globalDescSet = nullptr;

		// 帧与同步资源
		VkSemaphore m_timelineSemaphore = nullptr;
		std::array<FrameResources, MaxFramesInFlight> m_frameResources;

		// 纹理资源
		std::vector<GPUImage> m_images;
		std::vector<VkSampler> m_samplers;
		std::vector<VkDescriptorImageInfo> m_descriptorImageInfos;

		void showError(const std::string& errorMessage) const;

		bool createVulkanInstance();
		bool createSurface();
		VkPhysicalDevice findPhysicalDevice();
		bool findGraphicsQueue();
		bool createDevice(VkPhysicalDevice physicalDevice);
		bool initializeVMA();
		bool createSwapchain(uint32_t width, uint32_t height);
		void destroySwapchain();
		VkShaderModule createShaderModule(const std::string& filePath) const;
		bool createGraphicsPipeline();
		bool createDescriptorSets();
		bool createSyncResources();
		bool createCommandBuffers();

		// 一次性命令缓冲（用于纹理上传等）
		VkCommandBuffer startTransientCommandBuffer();
		void submitTransientCommandBuffer(VkCommandBuffer commandBuffer);

		// 把指定纹理索引写入全局描述符集
		void updateTextureDescriptor(uint32_t textureIndex);

		VulkanContext() = default;
	};

}