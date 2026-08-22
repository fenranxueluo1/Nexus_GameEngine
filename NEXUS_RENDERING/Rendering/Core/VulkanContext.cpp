#include "VulkanContext.h"
#include <Rendering/Essentials/Shader.h>
#include <Rendering/Essentials/Vertex.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#define VOLK_IMPLEMENTATION
#include <Volk/volk.h>
#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <cstddef>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace NEXUS_RENDERING {

	namespace {
		std::string readTextFile(const std::string& filePath)
		{
			std::ifstream infile(filePath);
			if (infile.is_open())
			{
				std::stringstream buffer;
				buffer << infile.rdbuf();
				infile.close();
				return buffer.str();
			}
			return std::string();
		}
	}

	VulkanContext& VulkanContext::Get()
	{
		static VulkanContext instance{};
		return instance;
	}

	void VulkanContext::showError(const std::string& errorMessage) const
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", errorMessage.c_str(), m_window);
	}

	bool VulkanContext::initialize(SDL_Window* window, uint32_t width, uint32_t height)
	{
		m_window = window;
		m_width = width;
		m_height = height;

		if (!createVulkanInstance())
		{
			showError("Couldn't create a vulkan instance");
			return false;
		}

		if (!createSurface())
		{
			showError("Couldn't create window surface");
			return false;
		}

		if (m_physicalDevice = findPhysicalDevice(); !m_physicalDevice)
		{
			showError("Unable to find an appropriate physical device");
			return false;
		}

		if (!findGraphicsQueue())
		{
			showError("Unable to find a compatible graphics queue");
			return false;
		}

		if (!createDevice(m_physicalDevice))
		{
			showError("Couldn't create the logical GPU device");
			return false;
		}

		if (!initializeVMA())
		{
			showError("Unable to create Vulkan Memory Allocator");
			return false;
		}

		if (!createSwapchain(m_width, m_height))
		{
			showError("Unable to create swapchain");
			return false;
		}

		if (!createDescriptorSets())
		{
			showError("Error creating descriptor sets");
			return false;
		}

		if (!createSyncResources())
		{
			showError("Couldn't create the sync related resources");
			return false;
		}

		if (!createCommandBuffers())
		{
			showError("Couldn't create command buffer objects");
			return false;
		}

		return true;
	}

	void VulkanContext::shutdown()
	{
		// 等待所有资源不再被 GPU 使用
		if (m_device)
		{
			vkDeviceWaitIdle(m_device);
		}

		// 帧 / 同步对象清理
		if (m_timelineSemaphore)
		{
			vkDestroySemaphore(m_device, m_timelineSemaphore, nullptr);
		}
		for (auto& res : m_frameResources)
		{
			if (res.imageAcquiredSemaphore)
			{
				vkDestroySemaphore(m_device, res.imageAcquiredSemaphore, nullptr);
			}
			if (res.commandPool)
			{
				vkDestroyCommandPool(m_device, res.commandPool, nullptr); // 销毁命令池（隐含销毁其中的命令缓冲）
			}
		}

		// 一次性命令缓冲池
		if (m_transientCommandPool)
		{
			vkDestroyCommandPool(m_device, m_transientCommandPool, nullptr);
		}

		// 渲染管线清理
		if (m_pipelineLayout)
		{
			vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
		}
		if (m_pipeline)
		{
			vkDestroyPipeline(m_device, m_pipeline, nullptr);
		}

		// 清理着色器
		if (m_vertShader)
		{
			vkDestroyShaderModule(m_device, m_vertShader, nullptr);
		}
		if (m_fragShader)
		{
			vkDestroyShaderModule(m_device, m_fragShader, nullptr);
		}

		// 清理描述符
		if (m_globalDSLayout)
		{
			vkDestroyDescriptorSetLayout(m_device, m_globalDSLayout, nullptr);
		}
		if (m_descPool)
		{
			vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
		}

		// 清理纹理与采样器
		for (const GPUImage& img : m_images)
		{
			vkDestroyImageView(m_device, img.imageView, nullptr);
			vkDestroyImage(m_device, img.image, nullptr);
			vmaFreeMemory(m_vmaAllocator, img.allocation);
		}
		m_images.clear();
		for (VkSampler sampler : m_samplers)
		{
			vkDestroySampler(m_device, sampler, nullptr);
		}
		m_samplers.clear();

		// 清理交换链
		destroySwapchain();

		// VMA 分配器
		if (m_vmaAllocator)
		{
			vmaDestroyAllocator(m_vmaAllocator);
		}

		// 清理 Vulkan
		if (m_surface)
		{
			vkDestroySurfaceKHR(m_vulkanInstance, m_surface, nullptr);
		}
		if (m_device)
		{
			vkDestroyDevice(m_device, nullptr);
		}
		if (m_vulkanInstance)
		{
			vkDestroyInstance(m_vulkanInstance, nullptr);
		}
		volkFinalize();
	}

	bool VulkanContext::createVulkanInstance()
	{
		// 初始化 Volk 并加载 Vk 函数指针
		if (volkInitialize() != VK_SUCCESS)
		{
			showError("Error initializing Volk");
			return false;
		}

		// 创建 Vulkan 应用实例
		VkApplicationInfo appInfo
		{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Nexus Game Engine",
			.apiVersion = VulkanVersion,
		};

		uint32_t instExtCount = 0;
		const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&instExtCount);

		std::vector<const char*> requestedLayers
		{
			"VK_LAYER_KHRONOS_validation"
		};

		VkInstanceCreateInfo instCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = static_cast<uint32_t>(requestedLayers.size()),
			.ppEnabledLayerNames = requestedLayers.data(),
			.enabledExtensionCount = instExtCount,
			.ppEnabledExtensionNames = extensions
		};

		if (vkCreateInstance(&instCreateInfo, nullptr, &m_vulkanInstance) != VK_SUCCESS)
		{
			return false;
		}

		volkLoadInstance(m_vulkanInstance);
		return true;
	}

	bool VulkanContext::createSurface()
	{
		if (!SDL_Vulkan_CreateSurface(m_window, m_vulkanInstance, nullptr, &m_surface))
		{
			return false;
		}
		return true;
	}

	VkPhysicalDevice VulkanContext::findPhysicalDevice()
	{
		// 枚举所有物理设备
		uint32_t physDeviceCount = 0;
		vkEnumeratePhysicalDevices(m_vulkanInstance, &physDeviceCount, nullptr);
		std::vector<VkPhysicalDevice> physicalDevices(physDeviceCount);
		vkEnumeratePhysicalDevices(m_vulkanInstance, &physDeviceCount, physicalDevices.data());

		VkPhysicalDevice physicalDevice = nullptr;
		if (physDeviceCount)
		{
			physicalDevice = physicalDevices[0]; // 默认使用第一个 GPU
			for (auto& pDev : physicalDevices)
			{
				VkPhysicalDeviceProperties props{};
				vkGetPhysicalDeviceProperties(pDev, &props);
				if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					physicalDevice = pDev;
					break;
				}
			}
		}

		// 确保目标交换链格式受支持
		uint32_t formatCount = 0;
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);
		std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, surfaceFormats.data());

		bool formatSupported = false;
		for (const VkSurfaceFormatKHR& surfFormat : surfaceFormats)
		{
			if (surfFormat.format == SwapchainFormat)
			{
				formatSupported = true;
				break;
			}
		}
		if (!formatSupported)
		{
			showError("Requested swapchain format is not supported by the surface");
			return nullptr;
		}

		return physicalDevice;
	}

	bool VulkanContext::findGraphicsQueue()
	{
		uint32_t queueFamCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamCount, nullptr);
		std::vector<VkQueueFamilyProperties2> queueFamProps(queueFamCount, { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2 });
		vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamCount, queueFamProps.data());

		for (int currentFamIdx = 0; currentFamIdx < queueFamProps.size(); currentFamIdx++)
		{
			VkBool32 hasPresentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, currentFamIdx, m_surface, &hasPresentSupport);

			const auto& props = queueFamProps[currentFamIdx];
			if (props.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT && hasPresentSupport)
			{
				m_gfxQueueFamIdx = currentFamIdx;
				return true;
			}
		}
		return false;
	}

	bool VulkanContext::createDevice(VkPhysicalDevice physicalDevice)
	{
		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo gfxQueueInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = m_gfxQueueFamIdx,
			.queueCount = 1,
			.pQueuePriorities = &queuePriority
		};

		// 查询设备支持的特性
		VkPhysicalDeviceVulkan14Features supportedFeatures14{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr };
		VkPhysicalDeviceVulkan13Features supportedFeatures13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &supportedFeatures14 };
		VkPhysicalDeviceVulkan12Features supportedFeatures12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &supportedFeatures13 };
		VkPhysicalDeviceFeatures2 supportedFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supportedFeatures12 };
		vkGetPhysicalDeviceFeatures2(physicalDevice, &supportedFeatures);

		// 检查所需特性是否受支持
		if (!supportedFeatures13.dynamicRendering || !supportedFeatures13.synchronization2 ||
			!supportedFeatures12.timelineSemaphore || !supportedFeatures12.descriptorIndexing ||
			!supportedFeatures12.descriptorBindingSampledImageUpdateAfterBind ||
			!supportedFeatures12.descriptorBindingPartiallyBound ||
			!supportedFeatures12.runtimeDescriptorArray ||
			!supportedFeatures12.shaderSampledImageArrayNonUniformIndexing)
		{
			showError("Physical device doesn't meet the feature requirements");
			return false;
		}

		// 构造用于创建设备的特性结构体链
		VkPhysicalDeviceVulkan14Features features14
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES,
			.pNext = nullptr,
		};
		VkPhysicalDeviceVulkan13Features features13
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
			.pNext = &features14,
			.synchronization2 = VK_TRUE,
			.dynamicRendering = VK_TRUE,
		};
		VkPhysicalDeviceVulkan12Features features12
		{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
			.pNext = &features13,
			.descriptorIndexing = VK_TRUE,
			.shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
			.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
			.descriptorBindingPartiallyBound = VK_TRUE,
			.runtimeDescriptorArray = VK_TRUE,
			.timelineSemaphore = VK_TRUE,
		};
		VkPhysicalDeviceFeatures2 features{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features12 };

		const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		VkDeviceCreateInfo devCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = &features,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &gfxQueueInfo,
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
			.pEnabledFeatures = nullptr // 特性结构体链已设置在 pNext 中
		};

		if (vkCreateDevice(physicalDevice, &devCreateInfo, nullptr, &m_device) != VK_SUCCESS)
		{
			return false;
		}

		// 最后获取 VkQueue 对象
		vkGetDeviceQueue(m_device, m_gfxQueueFamIdx, 0, &m_gfxQueue);
		if (!m_gfxQueue)
		{
			showError("Couldn't get the graphics queue");
			return false;
		}
		return true;
	}

	bool VulkanContext::initializeVMA()
	{
		VmaVulkanFunctions vmaFuncInfo{};
		VmaAllocatorCreateInfo vmaAllocInfo
		{
			.physicalDevice = m_physicalDevice,
			.device = m_device,
			.pVulkanFunctions = &vmaFuncInfo,
			.instance = m_vulkanInstance,
			.vulkanApiVersion = VulkanVersion
		};

		// VMA 可以直接从 volk 导入函数指针
		vmaImportVulkanFunctionsFromVolk(&vmaAllocInfo, &vmaFuncInfo);

		if (vmaCreateAllocator(&vmaAllocInfo, &m_vmaAllocator) != VK_SUCCESS)
		{
			return false;
		}
		return true;
	}

	bool VulkanContext::createSwapchain(uint32_t width, uint32_t height)
	{
		m_swapchainWidth = width;
		m_swapchainHeight = height;

		VkSurfaceCapabilitiesKHR surfaceCaps{};
		if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCaps) != VK_SUCCESS)
		{
			showError("Couldn't get the surface capabilities");
			return false;
		}

		VkSwapchainCreateInfoKHR swapchainCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = m_surface,
			.minImageCount = surfaceCaps.minImageCount,
			.imageFormat = SwapchainFormat,
			.imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
			.imageExtent{ .width = m_swapchainWidth, .height = m_swapchainHeight },
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_FIFO_KHR
		};

		if (vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain) != VK_SUCCESS)
		{
			showError("Error creating swapchain");
			return false;
		}

		// 获取交换链图像
		uint32_t imageCount = 0;
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
		m_swapchainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
		m_swapchainImageViews.resize(imageCount);

		// 创建交换链图像视图
		for (size_t i = 0; i < m_swapchainImages.size(); ++i)
		{
			VkImageViewCreateInfo imgViewInfo
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = m_swapchainImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = SwapchainFormat,
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.levelCount = 1,
					.layerCount = 1
				}
			};

			if (vkCreateImageView(m_device, &imgViewInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
			{
				showError("Error creating swapchain image view");
				return false;
			}
		}

		// 用于通知渲染完成的信号量
		m_renderCompleteSemaphores.resize(m_swapchainImages.size());
		for (VkSemaphore& semaphore : m_renderCompleteSemaphores)
		{
			VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
			{
				showError("Error creating the render-complete semaphore");
				return false;
			}
		}

		// 创建深度图像
		VkImageCreateInfo depthCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = DepthFormat,
			.extent{ .width = m_swapchainWidth, .height = m_swapchainHeight, .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};

		VmaAllocationCreateInfo allocInfo
		{
			.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO
		};
		if (vmaCreateImage(m_vmaAllocator, &depthCreateInfo, &allocInfo, &m_depthImage, &m_depthImageAllocation, nullptr) != VK_SUCCESS)
		{
			showError("Error allocating depth image");
			return false;
		}

		VkImageViewCreateInfo depthImgViewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = m_depthImage,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = DepthFormat,
			.subresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT, .levelCount = 1, .layerCount = 1 }
		};
		if (vkCreateImageView(m_device, &depthImgViewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
		{
			showError("Error creating depth image view");
			return false;
		}

		return true;
	}

	void VulkanContext::destroySwapchain()
	{
		for (VkImageView swapchainImgView : m_swapchainImageViews)
		{
			vkDestroyImageView(m_device, swapchainImgView, nullptr);
		}
		m_swapchainImageViews.clear();

		// 销毁渲染完成信号量
		for (VkSemaphore& semaphore : m_renderCompleteSemaphores)
		{
			vkDestroySemaphore(m_device, semaphore, nullptr);
		}
		m_renderCompleteSemaphores.clear();

		if (m_swapchain)
		{
			vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
			m_swapchain = nullptr;
		}

		// 与交换链一起销毁深度缓冲
		if (m_depthImageView)
		{
			vkDestroyImageView(m_device, m_depthImageView, nullptr);
			vmaDestroyImage(m_vmaAllocator, m_depthImage, m_depthImageAllocation);
			m_depthImageView = nullptr;
		}
	}

	VkShaderModule VulkanContext::createShaderModule(const std::string& fileName, const std::string& source, shaderc_shader_kind kind) const
	{
		// 将着色器编译为 SPIR-V
		shaderc::Compiler compiler;
		shaderc::CompileOptions opts;
		opts.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_4);
		opts.SetTargetSpirv(shaderc_spirv_version_1_6);
		opts.SetOptimizationLevel(shaderc_optimization_level_performance);
		shaderc::CompilationResult result = compiler.CompileGlslToSpv(source, kind, fileName.c_str(), opts);

		if (result.GetCompilationStatus() != shaderc_compilation_status_success)
		{
			std::cerr << "Shader Compilation Error: " << result.GetErrorMessage() << std::endl;
			return nullptr;
		}
		std::vector<uint32_t> spv = { result.cbegin(), result.cend() };

		// 把 SPIR-V 交给 Vulkan 创建着色器模块
		VkShaderModuleCreateInfo moduleCreateInfo
		{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.codeSize = spv.size() * sizeof(uint32_t),
			.pCode = spv.data()
		};
		VkShaderModule shaderModule = nullptr;
		if (vkCreateShaderModule(m_device, &moduleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			showError("Error creating shader module");
			return nullptr;
		}
		return shaderModule;
	}

	std::shared_ptr<Shader> VulkanContext::loadShaders(const std::string& vertexPath, const std::string& fragmentPath)
	{
		// 从磁盘读取着色器文件
		const std::string vertSource = readTextFile(vertexPath);
		const std::string fragSource = readTextFile(fragmentPath);
		if (vertSource.empty() || fragSource.empty())
		{
			showError("Specified shader file doesn't exist: " + vertexPath + " / " + fragmentPath);
			return nullptr;
		}

		// 销毁之前的着色器模块与管线
		if (m_pipeline)
		{
			vkDestroyPipeline(m_device, m_pipeline, nullptr);
			m_pipeline = nullptr;
		}
		if (m_vertShader)
		{
			vkDestroyShaderModule(m_device, m_vertShader, nullptr);
		}
		if (m_fragShader)
		{
			vkDestroyShaderModule(m_device, m_fragShader, nullptr);
		}

		// 创建图形渲染管线所需的着色器模块
		m_vertShader = createShaderModule(vertexPath, vertSource, shaderc_vertex_shader);
		m_fragShader = createShaderModule(fragmentPath, fragSource, shaderc_fragment_shader);
		if (!m_vertShader || !m_fragShader)
		{
			return nullptr;
		}

		if (!createGraphicsPipeline())
		{
			return nullptr;
		}

		return std::make_shared<Shader>(m_pipeline, vertexPath, fragmentPath);
	}

	bool VulkanContext::createGraphicsPipeline()
	{
		// 需要定义管线布局
		const VkPushConstantRange pushConstRange
		{
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset = 0,
			.size = sizeof(PushConstants)
		};

		std::array<VkDescriptorSetLayout, 1> dsLayouts{ m_globalDSLayout };

		VkPipelineLayoutCreateInfo pipelineLayoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount = static_cast<uint32_t>(dsLayouts.size()),
			.pSetLayouts = dsLayouts.data(),
			.pushConstantRangeCount = 1,
			.pPushConstantRanges = &pushConstRange
		};

		if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
		{
			showError("Unable to create the pipeline layout");
			return false;
		}

		// 配置着色器阶段结构体
		const char* entryPoint = "main";
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages
		{
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_VERTEX_BIT,
				.module = m_vertShader,
				.pName = entryPoint
			},
			{
				.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
				.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
				.module = m_fragShader,
				.pName = entryPoint
			}
		};

		// 顶点输入（与 NEXUS_RENDERING::Vertex 布局一致）
		VkVertexInputBindingDescription vertBinding
		{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX
		};
		std::array<VkVertexInputAttributeDescription, 3> vertAttributes
		{
			VkVertexInputAttributeDescription{ .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, position) },
			VkVertexInputAttributeDescription{ .location = 1, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = offsetof(Vertex, uvs) },
			VkVertexInputAttributeDescription{ .location = 2, .binding = 0, .format = VK_FORMAT_R8G8B8A8_UNORM, .offset = offsetof(Vertex, color) },
		};
		VkPipelineVertexInputStateCreateInfo vertInputInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &vertBinding,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertAttributes.size()),
			.pVertexAttributeDescriptions = vertAttributes.data()
		};

		// 输入装配，绘制三角形列表
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
		};

		// 深度 / 模板配置
		VkPipelineDepthStencilStateCreateInfo depthStencilInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable = VK_FALSE,
			.depthWriteEnable = VK_FALSE,
			.depthCompareOp = VK_COMPARE_OP_LESS,
			.stencilTestEnable = VK_FALSE
		};

		// 动态渲染允许在运行时动态设置视口
		// 但该结构体仍然需要
		VkPipelineViewportStateCreateInfo viewportInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.pViewports = nullptr,
			.scissorCount = 1,
			.pScissors = nullptr
		};

		// 光栅化设置
		VkPipelineRasterizationStateCreateInfo rasterInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_NONE,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
			.lineWidth = 1.0f,
		};

		// 不使用多重采样
		VkPipelineMultisampleStateCreateInfo multiSampleInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT
		};

		// Alpha 混合（与旧 OpenGL 管线的 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA) 一致）
		VkPipelineColorBlendAttachmentState attachState
		{
			.blendEnable = VK_TRUE,
			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,
			.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.alphaBlendOp = VK_BLEND_OP_ADD,
			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
				VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
		};
		VkPipelineColorBlendStateCreateInfo blendInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &attachState
		};

		// 启用动态状态
		std::vector<VkDynamicState> dynamicState
		{
			VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicStateInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamicState.size()),
			.pDynamicStates = dynamicState.data()
		};

		// 动态渲染所需的结构
		VkPipelineRenderingCreateInfo renderInfo
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 1,
			.pColorAttachmentFormats = &SwapchainFormat,
			.depthAttachmentFormat = DepthFormat
		};

		// 创建图形渲染管线
		VkGraphicsPipelineCreateInfo pipelineInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = &renderInfo,
			.stageCount = static_cast<uint32_t>(shaderStages.size()),
			.pStages = shaderStages.data(),
			.pVertexInputState = &vertInputInfo,
			.pInputAssemblyState = &inputAssemblyInfo,
			.pViewportState = &viewportInfo,
			.pRasterizationState = &rasterInfo,
			.pMultisampleState = &multiSampleInfo,
			.pDepthStencilState = &depthStencilInfo,
			.pColorBlendState = &blendInfo,
			.pDynamicState = &dynamicStateInfo,
			.layout = m_pipelineLayout,
			.renderPass = VK_NULL_HANDLE,
		};
		if (vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
		{
			showError("Error creating the pipeline");
			return false;
		}
		return true;
	}

	bool VulkanContext::createDescriptorSets()
	{
		std::array<VkDescriptorPoolSize, 1> poolSizes
		{
			VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = MaxTextures }
		};

		VkDescriptorPoolCreateInfo poolInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
			.maxSets = 1,
			.poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
			.pPoolSizes = poolSizes.data()
		};
		if (vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descPool) != VK_SUCCESS)
		{
			showError("Unable to create descriptor pool");
			return false;
		}

		// 全局描述符集
		std::array<VkDescriptorSetLayoutBinding, 1> bindings
		{
			VkDescriptorSetLayoutBinding
			{
				.binding = 0,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount = MaxTextures,
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
			}
		};
		std::array<VkDescriptorBindingFlags, 1> flags;
		flags[0] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

		VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(flags.size()),
			.pBindingFlags = flags.data()
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = &flagsInfo,
			.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data()
		};
		if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_globalDSLayout) != VK_SUCCESS)
		{
			showError("Unable to create descriptor set layout");
			return false;
		}

		// 创建实际的描述符集
		VkDescriptorSetAllocateInfo descSetAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = m_descPool,
			.descriptorSetCount = 1,
			.pSetLayouts = &m_globalDSLayout,
		};
		if (vkAllocateDescriptorSets(m_device, &descSetAllocInfo, &m_globalDescSet) != VK_SUCCESS)
		{
			showError("Unable to allocate descriptor set");
			return false;
		}

		// 预先扩容描述符图像信息数组（指针必须保持稳定）
		m_descriptorImageInfos.resize(MaxTextures);

		return true;
	}

	bool VulkanContext::createSyncResources()
	{
		VkSemaphoreTypeCreateInfo semaphoreTypeInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
			.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
			.initialValue = MaxFramesInFlight
		};
		VkSemaphoreCreateInfo semaphoreInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = &semaphoreTypeInfo
		};
		if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_timelineSemaphore) != VK_SUCCESS)
		{
			showError("Unable to create the timeline semaphore");
			return false;
		}

		// 每帧的图像获取信号量
		for (FrameResources& res : m_frameResources)
		{
			VkSemaphoreCreateInfo frameSemaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
			if (vkCreateSemaphore(m_device, &frameSemaphoreInfo, nullptr, &res.imageAcquiredSemaphore) != VK_SUCCESS)
			{
				showError("Error creating the per-frame image-acquire semaphore");
				return false;
			}
		}

		return true;
	}

	bool VulkanContext::createCommandBuffers()
	{
		// 创建一次性命令缓冲的命令池
		VkCommandPoolCreateInfo transientPoolInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
			.queueFamilyIndex = m_gfxQueueFamIdx
		};
		if (vkCreateCommandPool(m_device, &transientPoolInfo, nullptr, &m_transientCommandPool) != VK_SUCCESS)
		{
			showError("Unable to create the transient command buffer pool");
			return false;
		}

		for (FrameResources& res : m_frameResources)
		{
			// 每帧分配独立命令池，重置命令缓冲更快
			VkCommandPoolCreateInfo poolInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
				.queueFamilyIndex = m_gfxQueueFamIdx
			};
			if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &res.commandPool) != VK_SUCCESS)
			{
				showError("Unable to create command buffer pool");
				return false;
			}

			// 创建本帧的命令缓冲
			VkCommandBufferAllocateInfo cmdAllocInfo
			{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
				.commandPool = res.commandPool,
				.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
				.commandBufferCount = 1,
			};

			if (vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &res.commandBuffer) != VK_SUCCESS)
			{
				showError("Unable to allocate command buffer");
				return false;
			}
		}
		return true;
	}

	VkCommandBuffer VulkanContext::startTransientCommandBuffer()
	{
		VkCommandBufferAllocateInfo cmdAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = m_transientCommandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkCommandBuffer commandBuffer = nullptr;
		if (vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &commandBuffer) != VK_SUCCESS)
		{
			showError("Unable to allocate command buffer");
			return nullptr;
		}

		VkCommandBufferBeginInfo beginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			showError("Unable to begin command buffer");
			vkFreeCommandBuffers(m_device, m_transientCommandPool, 1, &commandBuffer);
			return nullptr;
		}

		return commandBuffer;
	}

	void VulkanContext::submitTransientCommandBuffer(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer
		};

		vkQueueSubmit(m_gfxQueue, 1, &submitInfo, nullptr);
		vkQueueWaitIdle(m_gfxQueue);
		vkFreeCommandBuffers(m_device, m_transientCommandPool, 1, &commandBuffer);
	}

	void VulkanContext::updateTextureDescriptor(uint32_t textureIndex)
	{
		VkWriteDescriptorSet descSetWrite
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet = m_globalDescSet,
			.dstBinding = 0,
			.dstArrayElement = textureIndex - 1,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.pImageInfo = &m_descriptorImageInfos[textureIndex - 1]
		};
		vkUpdateDescriptorSets(m_device, 1, &descSetWrite, 0, nullptr);
	}

	uint32_t VulkanContext::addTexture(unsigned char* pixels, uint32_t width, uint32_t height, bool pixelArt)
	{
		if (m_images.size() >= MaxTextures)
		{
			showError("Exceeded maximum texture count");
			return 0;
		}

		// 创建 Vulkan 图像与内存分配
		const VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		VkImageCreateInfo imageInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
			.imageType = VK_IMAGE_TYPE_2D,
			.format = imageFormat,
			.extent{ .width = width, .height = height, .depth = 1 },
			.mipLevels = 1,
			.arrayLayers = 1,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.tiling = VK_IMAGE_TILING_OPTIMAL,
			.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
		};
		VmaAllocationCreateInfo allocInfo{ .usage = VMA_MEMORY_USAGE_AUTO };
		GPUImage gpuImage;
		if (vmaCreateImage(m_vmaAllocator, &imageInfo, &allocInfo, &gpuImage.image, &gpuImage.allocation, nullptr) != VK_SUCCESS)
		{
			showError("Error creating image");
			return 0;
		}

		VkImageViewCreateInfo imgViewInfo
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = gpuImage.image,
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = imageFormat,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.levelCount = 1,
				.layerCount = 1
			}
		};
		if (vkCreateImageView(m_device, &imgViewInfo, nullptr, &gpuImage.imageView) != VK_SUCCESS)
		{
			showError("Error creating image view");
			vmaDestroyImage(m_vmaAllocator, gpuImage.image, gpuImage.allocation);
			return 0;
		}

		VkCommandBuffer commandBuffer = startTransientCommandBuffer();
		if (!commandBuffer)
		{
			vkDestroyImageView(m_device, gpuImage.imageView, nullptr);
			vmaDestroyImage(m_vmaAllocator, gpuImage.image, gpuImage.allocation);
			return 0;
		}

		// 把图像转换到 transfer-DST 布局
		VkImageMemoryBarrier2 transferBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
			.srcAccessMask = VK_ACCESS_2_NONE,
			.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
			.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.image = gpuImage.image,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		VkDependencyInfo transferDepInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &transferBarrier
		};
		vkCmdPipelineBarrier2(commandBuffer, &transferDepInfo);

		// 创建暂存缓冲并记录拷贝操作
		const size_t byteSize = static_cast<size_t>(width) * height * 4;
		VkBufferCreateInfo buffInfo
		{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.size = byteSize,
			.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE
		};
		VmaAllocationCreateInfo stageAllocInfo
		{
			.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
			.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
		};
		VkBuffer stageBuffer = nullptr;
		VmaAllocation stageAllocation = nullptr;
		if (vmaCreateBuffer(m_vmaAllocator, &buffInfo, &stageAllocInfo, &stageBuffer, &stageAllocation, nullptr) != VK_SUCCESS)
		{
			showError("Unable to create texture staging buffer");
			vkDestroyImageView(m_device, gpuImage.imageView, nullptr);
			vmaDestroyImage(m_vmaAllocator, gpuImage.image, gpuImage.allocation);
			return 0;
		}

		// 映射并拷贝像素数据
		void* buffPtr = nullptr;
		vmaMapMemory(m_vmaAllocator, stageAllocation, &buffPtr);
		std::memcpy(buffPtr, pixels, byteSize);
		vmaUnmapMemory(m_vmaAllocator, stageAllocation);

		VkBufferImageCopy buffImgCopy
		{
			.imageSubresource = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1 },
			.imageExtent = { .width = width, .height = height, .depth = 1 },
		};
		vkCmdCopyBufferToImage(commandBuffer, stageBuffer, gpuImage.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffImgCopy);

		// 把图像转换到供着色器读取/采样的布局
		VkImageMemoryBarrier2 shaderReadBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
			.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
			.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
			.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.image = gpuImage.image,
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		VkDependencyInfo shaderReadDepInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &shaderReadBarrier
		};
		vkCmdPipelineBarrier2(commandBuffer, &shaderReadDepInfo);

		submitTransientCommandBuffer(commandBuffer);

		// 清理暂存缓冲
		vmaDestroyBuffer(m_vmaAllocator, stageBuffer, stageAllocation);

		// 创建纹理采样器
		VkSamplerCreateInfo samplerInfo
		{
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.magFilter = pixelArt ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
			.minFilter = pixelArt ? VK_FILTER_NEAREST : VK_FILTER_LINEAR,
			.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
			.compareEnable = VK_FALSE,
			.minLod = 0.0f,
			.maxLod = VK_LOD_CLAMP_NONE
		};
		VkSampler sampler = nullptr;
		if (vkCreateSampler(m_device, &samplerInfo, nullptr, &sampler) != VK_SUCCESS)
		{
			showError("Unable to create texture sampler");
			vkDestroyImageView(m_device, gpuImage.imageView, nullptr);
			vmaDestroyImage(m_vmaAllocator, gpuImage.image, gpuImage.allocation);
			return 0;
		}

		m_samplers.push_back(sampler);
		m_images.push_back(gpuImage);
		const uint32_t textureIndex = static_cast<uint32_t>(m_images.size());

		// 注册到全局描述符集
		m_descriptorImageInfos[textureIndex - 1] = VkDescriptorImageInfo
		{
			.sampler = sampler,
			.imageView = gpuImage.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};
		updateTextureDescriptor(textureIndex);

		return textureIndex;
	}

	void VulkanContext::resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
		m_requireSwapchainRecreate = true;
	}

	void VulkanContext::beginFrame()
	{
		// 首先检查交换链是否仍然有效
		if (m_requireSwapchainRecreate)
		{
			vkDeviceWaitIdle(m_device);
			destroySwapchain();
			createSwapchain(m_width, m_height);
			m_requireSwapchainRecreate = false;
		}

		m_frameResIndex = m_frameIndex++ % MaxFramesInFlight;
		const uint64_t signalValue = m_nextSignalValue++;
		const uint64_t waitValue = signalValue - MaxFramesInFlight;

		VkSemaphoreWaitInfo waitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
			.semaphoreCount = 1,
			.pSemaphores = &m_timelineSemaphore,
			.pValues = &waitValue
		};
		vkWaitSemaphores(m_device, &waitInfo, UINT64_MAX);

		// 现在可以安全地开始记录命令
		FrameResources& res = m_frameResources[m_frameResIndex];
		vkResetCommandPool(m_device, res.commandPool, 0);

		VkSemaphore imageAcquireSemaphore = res.imageAcquiredSemaphore;

		VkResult acquireResult = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, imageAcquireSemaphore, VK_NULL_HANDLE, &m_imageIndex);

		// 处理窗口调整大小与过期的图像，可能需要重建交换链
		if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
		{
			m_requireSwapchainRecreate = true;
			m_frameActive = false;
			return;
		}
		else if (acquireResult == VK_SUBOPTIMAL_KHR)
		{
			// 本帧可以渲染，下一帧再重建交换链
			m_requireSwapchainRecreate = true;
		}

		m_frameActive = true;

		// 开始记录命令
		VkCommandBufferBeginInfo cmdBeginInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
		};
		vkBeginCommandBuffer(res.commandBuffer, &cmdBeginInfo);

		// 转换颜色与深度图像布局
		std::array<VkImageMemoryBarrier2, 2> layoutBarriers
		{
			VkImageMemoryBarrier2
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.image = m_swapchainImages[m_imageIndex],
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				}
			},
			VkImageMemoryBarrier2
			{
				.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
				.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask = 0,
				.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
				.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.newLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
				.image = m_depthImage,
				.subresourceRange
				{
					.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1,
				}
			}
		};
		VkDependencyInfo depInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = static_cast<uint32_t>(layoutBarriers.size()),
			.pImageMemoryBarriers = layoutBarriers.data()
		};
		vkCmdPipelineBarrier2(res.commandBuffer, &depInfo);

		// 配置附件（颜色与深度）并开始动态渲染
		VkRenderingAttachmentInfo colorAttachInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_swapchainImageViews[m_imageIndex],
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR, // 清除图像
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE, // 保留数据用于呈现
			.clearValue{ .color{ 1.f, 1.f, 1.f, 1.f } }
		};
		VkRenderingAttachmentInfo depthAttachInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = m_depthImageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.clearValue{ .depthStencil{ 1.0f, 0 } }
		};
		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea
			{
				.offset{ .x = 0, .y = 0 },
				.extent{ .width = m_swapchainWidth, .height = m_swapchainHeight }
			},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachInfo,
			.pDepthAttachment = &depthAttachInfo
		};

		// 开始动态渲染
		vkCmdBeginRendering(res.commandBuffer, &renderingInfo);

		// 设置视口与裁剪矩形（Y 轴翻转以匹配左上角为正交投影原点）
		VkViewport viewport
		{
			.x = 0,
			.y = static_cast<float>(m_swapchainHeight),
			.width = static_cast<float>(m_swapchainWidth),
			.height = -static_cast<float>(m_swapchainHeight),
			.minDepth = 0,
			.maxDepth = 1
		};
		vkCmdSetViewport(res.commandBuffer, 0, 1, &viewport);

		VkRect2D scissor
		{
			.offset{ .x = 0, .y = 0 },
			.extent{ .width = m_swapchainWidth, .height = m_swapchainHeight }
		};
		vkCmdSetScissor(res.commandBuffer, 0, 1, &scissor);

		// 绑定图形渲染管线与全局纹理描述符集
		vkCmdBindPipeline(res.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
		vkCmdBindDescriptorSets(res.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &m_globalDescSet, 0, nullptr);
	}

	void VulkanContext::endFrame()
	{
		if (!m_frameActive)
		{
			m_frameActive = false;
			return;
		}
		m_frameActive = false;

		FrameResources& res = m_frameResources[m_frameResIndex];

		// 先结束动态渲染，再把图像转换到呈现布局
		vkCmdEndRendering(res.commandBuffer);

		// 把图像从颜色附件转换为呈现布局以便显示
		VkImageMemoryBarrier2 presentLayoutBarrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_NONE,
			.dstAccessMask = 0,
			.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			.image = m_swapchainImages[m_imageIndex],
			.subresourceRange
			{
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			}
		};
		VkDependencyInfo presentDepInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &presentLayoutBarrier
		};
		vkCmdPipelineBarrier2(res.commandBuffer, &presentDepInfo);

		vkEndCommandBuffer(res.commandBuffer);

		// 确保交换链图像已就绪，可以开始颜色输出
		VkSemaphoreSubmitInfo imageAcquireWaitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.semaphore = res.imageAcquiredSemaphore,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT // 在绘制到图像之前等待
		};
		// 通知图像可以呈现
		const uint64_t signalValue = m_nextSignalValue - 1;
		std::array<VkSemaphoreSubmitInfo, 2> semaphoreSignals
		{
			VkSemaphoreSubmitInfo
			{ // 渲染工作完成信号
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = m_renderCompleteSemaphores[m_imageIndex],
				.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
			},
			VkSemaphoreSubmitInfo
			{ // 整帧已完成（时间线）
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.semaphore = m_timelineSemaphore,
				.value = signalValue,
				.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT
			}
		};
		VkCommandBufferSubmitInfo cmdSubmitInfo
		{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.commandBuffer = res.commandBuffer,
		};
		VkSubmitInfo2 submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.waitSemaphoreInfoCount = 1,
			.pWaitSemaphoreInfos = &imageAcquireWaitInfo, // 确保图像已就绪
			.commandBufferInfoCount = 1,
			.pCommandBufferInfos = &cmdSubmitInfo,
			.signalSemaphoreInfoCount = static_cast<uint32_t>(semaphoreSignals.size()),
			.pSignalSemaphoreInfos = semaphoreSignals.data()
		};
		vkQueueSubmit2(m_gfxQueue, 1, &submitInfo, VK_NULL_HANDLE);

		// 呈现图像
		VkPresentInfoKHR presentInfo
		{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &m_renderCompleteSemaphores[m_imageIndex], // 渲染工作完成信号量
			.swapchainCount = 1,
			.pSwapchains = &m_swapchain,
			.pImageIndices = &m_imageIndex,
			.pResults = nullptr
		};

		const VkResult presentResult = vkQueuePresentKHR(m_gfxQueue, &presentInfo);
		if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
		{
			m_requireSwapchainRecreate = true;
		}
	}

}