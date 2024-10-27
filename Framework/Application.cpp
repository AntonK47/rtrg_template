#include "Application.hpp"

#include "Utils.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <vector>

#define VOLK_IMPLEMENTATION
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <volk.h>


struct PerFrameResource
{
	VkSemaphore readyToPresent;
	VkSemaphore readyToRender;

	VkFence frameFinished;

	VkCommandBuffer commandBuffer;
	VkCommandPool commandPool;
};

struct VulkanContext
{
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkDevice device;

	uint32_t graphicsQueueFamilyIndex{};
	VkQueue graphicsQueue;

	VkSurfaceKHR surface;
	VkSwapchainKHR swapchain;
	uint32_t swapchainImageCount;
	VkFormat swapchainImageFormat;
	VkColorSpaceKHR swapchainImageColorSpace;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkImage> swapchainImages;

	std::vector<PerFrameResource> perFrameResources;
};

struct FullscreenQuadPassResources
{

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	void ReleaseResouces(VkDevice device)
	{
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyPipeline(device, pipeline, nullptr);
	}
};

void Framework::Application::Run()
{
#pragma region SDL window initialization
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("SDL_Init failed (%s)", SDL_GetError());
		return;
	}

	const auto applicationName = "Template Application";

	SDL_Window* window =
		SDL_CreateWindow(applicationName, 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	// could be replaced with win32 window, see minimul example here
	// https://learn.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program?source=recommendations

	if (!window)
	{
		SDL_Log("SDL_CreateWindow failed (%s)", SDL_GetError());
		SDL_Quit();
		return;
	}
#pragma endregion

#pragma region Vulkan Instance creation
	{
		const auto result = volkInitialize();
		assert(result == VK_SUCCESS);
	}

	auto instanceExtensions = std::vector<const char*>{};

	{
		auto extensionsCount = uint32_t{};
		auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionsCount);

		instanceExtensions.resize(extensionsCount);
		for (auto i = 0; i < extensionsCount; i++)
		{
			instanceExtensions[i] = extensions[i];
		}
	}

	auto vulkanContext = VulkanContext{};

	{
		const auto instanceLayers = std::array{ "VK_LAYER_KHRONOS_validation" };
		instanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);

		const auto applicationInfo = VkApplicationInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
														.pNext = nullptr,
														.pApplicationName = applicationName,
														.applicationVersion = 0,
														.pEngineName = applicationName,
														.engineVersion = 0,
														.apiVersion = VK_API_VERSION_1_3 };

		const auto instanceCreateInfo =
			VkInstanceCreateInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
								  .pNext = nullptr,
								  .flags = 0,
								  .pApplicationInfo = &applicationInfo,
								  .enabledLayerCount = instanceLayers.size(),
								  .ppEnabledLayerNames = instanceLayers.data(),
								  .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
								  .ppEnabledExtensionNames = instanceExtensions.data() };

		const auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &vulkanContext.instance);
		assert(result == VK_SUCCESS);
	}

	volkLoadInstance(vulkanContext.instance);
#pragma endregion

#pragma region Physical device selection
	{
		auto physicalDeviceCount = uint32_t{};
		auto result = vkEnumeratePhysicalDevices(vulkanContext.instance, &physicalDeviceCount, nullptr);
		assert(result == VK_SUCCESS);

		std::vector<VkPhysicalDevice> physicalDevices{};
		physicalDevices.resize(physicalDeviceCount);

		result = vkEnumeratePhysicalDevices(vulkanContext.instance, &physicalDeviceCount, physicalDevices.data());
		assert(result == VK_SUCCESS);


		struct FamilyQueueQueryInfo
		{
			uint32_t rating{ 0 };
			bool hasGraphicsQueue{ false };
			uint32_t graphicsQueueFamilyIndex{};
			bool isDiscrete{ false };
		};

		auto physicalDevicesQuery = std::vector<FamilyQueueQueryInfo>{};
		physicalDevicesQuery.resize(physicalDeviceCount);

		for (auto i = 0; i < physicalDevices.size(); i++)
		{
			const auto& physicalDevice = physicalDevices[i];

			// QUEUE CHECKS
			{

				auto queueFamilyPropertiesCount = uint32_t{ 0 };
				vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, nullptr);

				auto queueFamilyProperties = std::vector<VkQueueFamilyProperties2>{};
				queueFamilyProperties.resize(queueFamilyPropertiesCount);
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					queueFamilyProperties[index].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
					queueFamilyProperties[index].pNext = nullptr;
				}

				vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount,
														  queueFamilyProperties.data());

				auto hasRequiredGraphicsQueueFamily = false;
				auto foundIndex = 0;

				// SEARCH FOR GRAPHICS QUEUE WITH PRESENT SUPPORT
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					const auto hasGraphicsBit = (queueFamilyProperties[i].queueFamilyProperties.queueFlags &
												 VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT;
					const auto canPresent =
						SDL_Vulkan_GetPresentationSupport(vulkanContext.instance, physicalDevice, index);

					hasRequiredGraphicsQueueFamily = hasGraphicsBit && canPresent;

					if (hasRequiredGraphicsQueueFamily)
					{
						foundIndex = index;
						break;
					}
				}

				if (hasRequiredGraphicsQueueFamily)
				{
					physicalDevicesQuery[i].hasGraphicsQueue = hasRequiredGraphicsQueueFamily;
					physicalDevicesQuery[i].graphicsQueueFamilyIndex = foundIndex;
					physicalDevicesQuery[i].rating += 100;
				}
			}

			// DEVICE TYPE CHECK
			{
				VkPhysicalDeviceProperties2 properties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
														.pNext = nullptr };
				vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

				if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					physicalDevicesQuery[i].rating += 1000;
					physicalDevicesQuery[i].isDiscrete = true;
				}
			}
		}


		// SELECT HIGHT RATING DEVICE
		{
			const auto& bestCandidate = std::max_element(
				physicalDevicesQuery.begin(), physicalDevicesQuery.end(),
				[](const FamilyQueueQueryInfo& a, const FamilyQueueQueryInfo& b) { return a.rating > b.rating; });
			const auto bestCandidateIndex = std::distance(physicalDevicesQuery.begin(), bestCandidate);

			vulkanContext.physicalDevice = physicalDevices[bestCandidateIndex];
			vulkanContext.graphicsQueueFamilyIndex = physicalDevicesQuery[bestCandidateIndex].graphicsQueueFamilyIndex;
		}
	}
#pragma endregion

#pragma region Device creation
	{
		const auto enabledDeviceExtensions = std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };


		const auto queuePriority = 1.0f;
		const auto queueCreateInfos =
			std::array{ VkDeviceQueueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
												 .pNext = nullptr,
												 .flags = 0,
												 .queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex,
												 .queueCount = 1,
												 .pQueuePriorities = &queuePriority } };


		auto physicalDeviceFeatures13 = VkPhysicalDeviceVulkan13Features{};
		physicalDeviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		physicalDeviceFeatures13.pNext = nullptr;
		physicalDeviceFeatures13.synchronization2 = VK_TRUE;
		physicalDeviceFeatures13.dynamicRendering = VK_TRUE;


		const auto deviceCreateInfo =
			VkDeviceCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
								.pNext = &physicalDeviceFeatures13,
								.flags = 0,
								.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
								.pQueueCreateInfos = queueCreateInfos.data(),
								.enabledLayerCount = 0,
								.ppEnabledLayerNames = nullptr,
								.enabledExtensionCount = enabledDeviceExtensions.size(),
								.ppEnabledExtensionNames = enabledDeviceExtensions.data(),
								.pEnabledFeatures = nullptr };

		const auto result =
			vkCreateDevice(vulkanContext.physicalDevice, &deviceCreateInfo, nullptr, &vulkanContext.device);
		assert(result == VK_SUCCESS);
	}
	volkLoadDevice(vulkanContext.device);
#pragma endregion

#pragma region Swapchain creation
	{
		const auto result = SDL_Vulkan_CreateSurface(window, vulkanContext.instance, nullptr, &vulkanContext.surface);
		assert(result);
	}
	{

		auto supported = VkBool32{};
		const auto result = vkGetPhysicalDeviceSurfaceSupportKHR(
			vulkanContext.physicalDevice, vulkanContext.graphicsQueueFamilyIndex, vulkanContext.surface, &supported);
		assert(result == VK_SUCCESS);
		assert(supported == VK_TRUE);
	}

	{
		auto width{ 0 };
		auto height{ 0 };

		SDL_GetWindowSize(window, &width, &height);

		{
			auto surfaceCapabilities =
				VkSurfaceCapabilities2KHR{ .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, .pNext = nullptr };
			const auto surfaceInfo =
				VkPhysicalDeviceSurfaceInfo2KHR{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
												 .pNext = nullptr,
												 .surface = vulkanContext.surface };
			const auto result = vkGetPhysicalDeviceSurfaceCapabilities2KHR(vulkanContext.physicalDevice, &surfaceInfo,
																		   &surfaceCapabilities);
			assert(result == VK_SUCCESS);

			vulkanContext.swapchainImageCount = std::max({ surfaceCapabilities.surfaceCapabilities.minImageCount, 3u });
		}

		{
			const auto surfaceInfo =
				VkPhysicalDeviceSurfaceInfo2KHR{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
												 .pNext = nullptr,
												 .surface = vulkanContext.surface };

			auto formatsCount = uint32_t{ 0 };
			auto result = vkGetPhysicalDeviceSurfaceFormats2KHR(vulkanContext.physicalDevice, &surfaceInfo,
																&formatsCount, nullptr);
			assert(result == VK_SUCCESS);

			auto formats = std::vector<VkSurfaceFormat2KHR>{};
			formats.resize(formatsCount);

			for (auto i = 0; i < formatsCount; i++)
			{
				formats[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
				formats[i].pNext = nullptr;
			}

			result = vkGetPhysicalDeviceSurfaceFormats2KHR(vulkanContext.physicalDevice, &surfaceInfo, &formatsCount,
														   formats.data());
			assert(result == VK_SUCCESS);

			assert(formats.size() > 0);
			// check if rgba srgb available
			const auto found = std::find_if(formats.begin(), formats.end(), [](const VkSurfaceFormat2KHR& format)
											{ return format.surfaceFormat.format == VK_FORMAT_R8G8B8A8_SRGB; });
			auto format = VkSurfaceFormat2KHR{};
			if (found != formats.end())
			{
				format = *found;
			}
			else
			{
				// otherwise choose the first fit
				format = formats.front();
			}


			vulkanContext.swapchainImageFormat = format.surfaceFormat.format;
			vulkanContext.swapchainImageColorSpace = format.surfaceFormat.colorSpace;
		}

		const auto swapchainCreateInfo =
			VkSwapchainCreateInfoKHR{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
									  .pNext = nullptr,
									  .flags = 0,
									  .surface = vulkanContext.surface,
									  .minImageCount = vulkanContext.swapchainImageCount,
									  .imageFormat = vulkanContext.swapchainImageFormat,
									  .imageColorSpace = vulkanContext.swapchainImageColorSpace,
									  .imageExtent = VkExtent2D{ .width = static_cast<uint32_t>(width),
																 .height = static_cast<uint32_t>(height) },
									  .imageArrayLayers = 1,
									  .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
									  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
									  .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
									  .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
									  .presentMode = VK_PRESENT_MODE_FIFO_KHR,
									  .clipped = VK_FALSE,
									  .oldSwapchain = VK_NULL_HANDLE };

		const auto result =
			vkCreateSwapchainKHR(vulkanContext.device, &swapchainCreateInfo, nullptr, &vulkanContext.swapchain);
		assert(result == VK_SUCCESS);
	}
#pragma endregion

#pragma region Create per swapchain image resources
	{
		{
			auto imageCount = uint32_t{ 0 };
			auto result = vkGetSwapchainImagesKHR(vulkanContext.device, vulkanContext.swapchain, &imageCount, nullptr);
			assert(result == VK_SUCCESS);
			vulkanContext.swapchainImages.resize(imageCount);

			result = vkGetSwapchainImagesKHR(vulkanContext.device, vulkanContext.swapchain, &imageCount,
											 vulkanContext.swapchainImages.data());
			assert(result == VK_SUCCESS);
		}

		const auto frameResourceCount = vulkanContext.swapchainImageCount;

		vulkanContext.perFrameResources.resize(frameResourceCount);
		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto fenceCreateInfo = VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
															.pNext = nullptr,
															.flags = VK_FENCE_CREATE_SIGNALED_BIT };

			const auto result = vkCreateFence(vulkanContext.device, &fenceCreateInfo, nullptr,
											  &vulkanContext.perFrameResources[i].frameFinished);
			assert(result == VK_SUCCESS);
		}

		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto semaphoreCreateInfo =
				VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
			const auto result = vkCreateSemaphore(vulkanContext.device, &semaphoreCreateInfo, nullptr,
												  &vulkanContext.perFrameResources[i].readyToPresent);
			assert(result == VK_SUCCESS);
		}

		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto semaphoreCreateInfo =
				VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
			const auto result = vkCreateSemaphore(vulkanContext.device, &semaphoreCreateInfo, nullptr,
												  &vulkanContext.perFrameResources[i].readyToRender);
			assert(result == VK_SUCCESS);
		}

		vulkanContext.swapchainImageViews.resize(frameResourceCount);
		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto imageViewCreateInfo = VkImageViewCreateInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = nullptr,
				.flags = 0,
				.image = vulkanContext.swapchainImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = vulkanContext.swapchainImageFormat,
				.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
												  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
				.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
			};
			const auto result = vkCreateImageView(vulkanContext.device, &imageViewCreateInfo, nullptr,
												  &vulkanContext.swapchainImageViews[i]);
			assert(result == VK_SUCCESS);
		}

#pragma region Command buffer creation

		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto poolCreateInfo =
				VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
										 .pNext = nullptr,
										 .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
										 .queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex };
			const auto result = vkCreateCommandPool(vulkanContext.device, &poolCreateInfo, nullptr,
													&vulkanContext.perFrameResources[i].commandPool);
			assert(result == VK_SUCCESS);
		}

		// create only one command buffer per pool, fon now, it might change in the future
		for (auto i = 0; i < frameResourceCount; i++)
		{
			VkStructureType sType;
			const void* pNext;
			VkCommandPool commandPool;
			VkCommandBufferLevel level;
			uint32_t commandBufferCount;

			const auto allocateInfo =
				VkCommandBufferAllocateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
											 .pNext = nullptr,
											 .commandPool = vulkanContext.perFrameResources[i].commandPool,
											 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
											 .commandBufferCount = 1 };
			const auto result = vkAllocateCommandBuffers(vulkanContext.device, &allocateInfo,
														 &vulkanContext.perFrameResources[i].commandBuffer);
			assert(result == VK_SUCCESS);
		}

#pragma endregion
	}
#pragma endregion

#pragma region Request graphics queue
	{
		const auto deviceQueueInfo = VkDeviceQueueInfo2{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
														 .pNext = nullptr,
														 .flags = 0,
														 .queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex,
														 .queueIndex = 0 };
		vkGetDeviceQueue2(vulkanContext.device, &deviceQueueInfo, &vulkanContext.graphicsQueue);
	}
#pragma endregion

#pragma region Geometry Pass initialization
	FullscreenQuadPassResources fullscreenQuadPassResources;

	{
		const auto pushConstantRange =
			VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(float) };

		const auto pipelineLayoutCreateInfo =
			VkPipelineLayoutCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
										.pNext = nullptr,
										.flags = 0,
										.setLayoutCount = 0,
										.pSetLayouts = nullptr,
										.pushConstantRangeCount = 1,
										.pPushConstantRanges = &pushConstantRange };
		const auto result = vkCreatePipelineLayout(vulkanContext.device, &pipelineLayoutCreateInfo, nullptr,
												   &fullscreenQuadPassResources.pipelineLayout);
		assert(result == VK_SUCCESS);
	}

	{
		VkShaderModule vertexShaderModule;
		{
			const auto shader =
				R"(#version 460

const vec3 triangle[] =
{
	vec3(1.0,3.0,0.0),
	vec3(1.0,-1.0,0.0),
	vec3(-3.0,-1.0,0.0)
};

const vec2 uvs[] =
{
	vec2(1.0,-1.0),
	vec2(1.0,1.0),
	vec2(-1.0,1.0)
};

layout(location = 0 ) out vec2 uv;

void main()
{
	uv = uvs[gl_VertexIndex].xy;
	gl_Position = vec4(triangle[gl_VertexIndex], 1.0);
})";

			const auto shaderInfo =
				Utils::ShaderInfo{ "main", {}, Utils::ShaderStage::Vertex, Utils::GlslShaderCode{ shader } };

			Utils::ShaderByteCode code;
			const auto compilationResult = Utils::CompileToSpirv(shaderInfo, code);
			assert(compilationResult == Utils::CompilationResult::Success);

			const auto vertexShaderCreateInfo =
				VkShaderModuleCreateInfo{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = 0,
										  .codeSize = code.size() * 4,
										  .pCode = code.data() };

			const auto result =
				vkCreateShaderModule(vulkanContext.device, &vertexShaderCreateInfo, nullptr, &vertexShaderModule);
			assert(result == VK_SUCCESS);
		}

		VkShaderModule fragmentShaderModule;
		{
			const auto fragmentShaderFile = std::filesystem::path("Assets/Shaders/ShaderToySample.frag");
			assert(std::filesystem::exists(fragmentShaderFile));
			auto stream = std::ifstream{ fragmentShaderFile, std::ios::ate };

			auto size = stream.tellg();
			auto shader = std::string(size, '\0'); // construct string to stream size
			stream.seekg(0);
			stream.read(&shader[0], size);

			const auto shaderInfo =
				Utils::ShaderInfo{ "main", {}, Utils::ShaderStage::Fragment, Utils::GlslShaderCode{ shader } };

			Utils::ShaderByteCode code;
			const auto compilationResult = Utils::CompileToSpirv(shaderInfo, code);
			assert(compilationResult == Utils::CompilationResult::Success);

			const auto fragmentShaderCreateInfo =
				VkShaderModuleCreateInfo{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
										  .pNext = nullptr,
										  .flags = 0,
										  .codeSize = code.size() * 4,
										  .pCode = code.data() };

			const auto result =
				vkCreateShaderModule(vulkanContext.device, &fragmentShaderCreateInfo, nullptr, &fragmentShaderModule);
			assert(result == VK_SUCCESS);
		}


		const auto shaderStages =
			std::array{ VkPipelineShaderStageCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
														 .pNext = nullptr,
														 .flags = 0,
														 .stage = VK_SHADER_STAGE_VERTEX_BIT,
														 .module = vertexShaderModule,
														 .pName = "main",
														 .pSpecializationInfo = nullptr },
						VkPipelineShaderStageCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
														 .pNext = nullptr,
														 .flags = 0,
														 .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
														 .module = fragmentShaderModule,
														 .pName = "main",
														 .pSpecializationInfo = nullptr } };


		const auto pipelineRendering =
			VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
										   .pNext = nullptr,
										   .viewMask = 0,
										   .colorAttachmentCount = 1,
										   .pColorAttachmentFormats = &vulkanContext.swapchainImageFormat,
										   .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
										   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };


		const auto vertexInputState =
			VkPipelineVertexInputStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
												  .pNext = nullptr,
												  .flags = 0,
												  .vertexBindingDescriptionCount = 0,
												  .pVertexBindingDescriptions = nullptr,
												  .vertexAttributeDescriptionCount = 0,
												  .pVertexAttributeDescriptions = nullptr };

		const auto inputAssemblyState =
			VkPipelineInputAssemblyStateCreateInfo{ .sType =
														VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
													.pNext = nullptr,
													.flags = 0,
													.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
													.primitiveRestartEnable = VK_FALSE };

		const auto viewportState =
			VkPipelineViewportStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
											   .pNext = nullptr,
											   .viewportCount = 1,
											   .pViewports = nullptr, // we will use dynamic state
											   .scissorCount = 1,
											   .pScissors = nullptr }; // we will use dynamic state

		const auto rasterizationState =
			VkPipelineRasterizationStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
													.pNext = nullptr,
													.flags = 0,
													.depthClampEnable = VK_FALSE,
													.rasterizerDiscardEnable = VK_FALSE,
													.polygonMode = VK_POLYGON_MODE_FILL,
													.cullMode = VK_CULL_MODE_BACK_BIT,
													.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
													.depthBiasEnable = VK_FALSE,
													.lineWidth = 1.0f };

		const auto multisampleState =
			VkPipelineMultisampleStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
												  .pNext = nullptr,
												  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
												  .sampleShadingEnable = VK_FALSE,
												  .alphaToCoverageEnable = VK_FALSE };

		const auto depthStencilState =
			VkPipelineDepthStencilStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
												   .pNext = nullptr,
												   .flags = 0,
												   .depthTestEnable = VK_FALSE,
												   .depthWriteEnable = VK_FALSE,
												   .depthBoundsTestEnable = VK_FALSE,
												   .stencilTestEnable = VK_FALSE };

		const auto blendAttachment =
			VkPipelineColorBlendAttachmentState{ .blendEnable = VK_TRUE,
												 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
												 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
												 .colorBlendOp = VK_BLEND_OP_ADD,
												 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
												 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
												 .alphaBlendOp = VK_BLEND_OP_ADD,
												 .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
													 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT };

		const auto blendState =
			VkPipelineColorBlendStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
												 .pNext = nullptr,
												 .flags = 0,
												 //.logicOpEnable = VK_FALSE,
												 .attachmentCount = 1,
												 .pAttachments = &blendAttachment };

		const auto dynamicStates = std::array{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		const auto dynamicState =
			VkPipelineDynamicStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = 0,
											  .dynamicStateCount = dynamicStates.size(),
											  .pDynamicStates = dynamicStates.data() };

		const auto pipelineCreateInfo =
			VkGraphicsPipelineCreateInfo{ .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
										  .pNext = &pipelineRendering,
										  .flags = 0,
										  .stageCount = shaderStages.size(),
										  .pStages = shaderStages.data(),
										  .pVertexInputState = &vertexInputState,
										  .pInputAssemblyState = &inputAssemblyState,
										  .pTessellationState = nullptr,
										  .pViewportState = &viewportState,
										  .pRasterizationState = &rasterizationState,
										  .pMultisampleState = &multisampleState,
										  .pDepthStencilState = &depthStencilState,
										  .pColorBlendState = &blendState,
										  .pDynamicState = &dynamicState,
										  .layout = fullscreenQuadPassResources.pipelineLayout,
										  .renderPass = VK_NULL_HANDLE,
										  .subpass = 0 };

		const auto result = vkCreateGraphicsPipelines(vulkanContext.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
													  nullptr, &fullscreenQuadPassResources.pipeline);
		assert(result == VK_SUCCESS);


		vkDestroyShaderModule(vulkanContext.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(vulkanContext.device, fragmentShaderModule, nullptr);
	}


#pragma endregion

	bool shouldRun = true;
	auto frameIndex = uint32_t{ 0 };

	while (shouldRun)
	{
#pragma region Handle window events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				shouldRun = false;
				break;
			}
		}
#pragma endregion

		const auto perFrameResourceIndex = frameIndex % vulkanContext.swapchainImageCount;

#pragma region Wait for resource reuse
		{

			const auto result =
				vkWaitForFences(vulkanContext.device, 1,
								&vulkanContext.perFrameResources[perFrameResourceIndex].frameFinished, VK_TRUE, ~0ull);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetFences(vulkanContext.device, 1,
											  &vulkanContext.perFrameResources[perFrameResourceIndex].frameFinished);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetCommandPool(
				vulkanContext.device, vulkanContext.perFrameResources[perFrameResourceIndex].commandPool, 0);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Acquire Swapchain image
		auto imageIndex = uint32_t{ 0 };

		{

			const auto acquireNextImageInfo =
				VkAcquireNextImageInfoKHR{ .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
										   .pNext = nullptr,
										   .swapchain = vulkanContext.swapchain,
										   .timeout = ~0ull,
										   .semaphore =
											   vulkanContext.perFrameResources[perFrameResourceIndex].readyToRender,
										   .fence = VK_NULL_HANDLE,
										   .deviceMask = 1 };

			const auto result = vkAcquireNextImage2KHR(vulkanContext.device, &acquireNextImageInfo, &imageIndex);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Begin Command Buffer
		auto& cmd = vulkanContext.perFrameResources[perFrameResourceIndex].commandBuffer;
		{
			const auto beginInfo = VkCommandBufferBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
															 .pNext = nullptr,
															 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
															 .pInheritanceInfo = nullptr };
			const auto result = vkBeginCommandBuffer(cmd, &beginInfo);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Resource transition [presentable image -> color attachment]
		{
			const auto imageBarrier =
				VkImageMemoryBarrier2{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
									   .pNext = nullptr,
									   .srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
									   .srcAccessMask = VK_ACCESS_2_NONE,
									   .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
									   .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									   .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
									   .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									   .image = vulkanContext.swapchainImages[imageIndex],
									   .subresourceRange =
										   VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

			const auto dependency = VkDependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
													  .pNext = nullptr,
													  .memoryBarrierCount = 0,
													  .pMemoryBarriers = nullptr,
													  .bufferMemoryBarrierCount = 0,
													  .pBufferMemoryBarriers = nullptr,
													  .imageMemoryBarrierCount = 1,
													  .pImageMemoryBarriers = &imageBarrier };
			vkCmdPipelineBarrier2(cmd, &dependency);
		}
#pragma endregion

#pragma region Rendering
		const auto colorAttachment =
			VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
									   .pNext = nullptr,
									   .imageView = vulkanContext.swapchainImageViews[imageIndex],
									   .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
									   .resolveMode = VK_RESOLVE_MODE_NONE,
									   .resolveImageView = VK_NULL_HANDLE,
									   .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
									   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
									   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
									   .clearValue = VkClearColorValue{ { 100, 0, 0, 255 } } };

		const auto renderingInfo = VkRenderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
													.pNext = nullptr,
													.flags = 0,
													.renderArea = VkRect2D{ { 0, 0 }, { 1280, 720 } },
													.layerCount = 1,
													.viewMask = 0,
													.colorAttachmentCount = 1,
													.pColorAttachments = &colorAttachment,
													.pDepthAttachment = nullptr,
													.pStencilAttachment = nullptr };
		vkCmdBeginRendering(cmd, &renderingInfo);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenQuadPassResources.pipeline);
		const auto viewport = VkViewport{ 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		const auto scissor = VkRect2D{ { 0, 0 }, { 1280, 720 } };
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		static float time = 0.0f;
		time += 0.01f;
		vkCmdPushConstants(cmd, fullscreenQuadPassResources.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						   sizeof(float), &time);
		vkCmdDraw(cmd, 3, 1, 0, 0);

		vkCmdEndRendering(cmd);
#pragma endregion

#pragma region Resource transition [color attachment -> presentable image]
		{
			const auto imageBarrier =
				VkImageMemoryBarrier2{ .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
									   .pNext = nullptr,
									   .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
									   .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
									   .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
									   .dstAccessMask = VK_ACCESS_2_NONE,
									   .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
									   .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
									   .image = vulkanContext.swapchainImages[imageIndex],
									   .subresourceRange =
										   VkImageSubresourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } };

			const auto dependency = VkDependencyInfo{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
													  .pNext = nullptr,
													  .memoryBarrierCount = 0,
													  .pMemoryBarriers = nullptr,
													  .bufferMemoryBarrierCount = 0,
													  .pBufferMemoryBarriers = nullptr,
													  .imageMemoryBarrierCount = 1,
													  .pImageMemoryBarriers = &imageBarrier };
			vkCmdPipelineBarrier2(cmd, &dependency);
		}
#pragma endregion

#pragma region End CommandBuffer
		{
			const auto result = vkEndCommandBuffer(cmd);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Submit Command Buffer
		{
			const auto bufferSubmitInfos = std::array{ VkCommandBufferSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
				.pNext = nullptr,
				.commandBuffer = vulkanContext.perFrameResources[perFrameResourceIndex].commandBuffer,
				.deviceMask = 1 } };
			const auto waitSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.pNext = nullptr,
				.semaphore = vulkanContext.perFrameResources[perFrameResourceIndex].readyToRender,
				.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.deviceIndex = 1 } };
			const auto signalSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.pNext = nullptr,
				.semaphore = vulkanContext.perFrameResources[perFrameResourceIndex].readyToPresent,
				.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.deviceIndex = 1 } };
			const auto submit = VkSubmitInfo2{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
				.pNext = nullptr,
				.flags = 0,
				.waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphoreInfos.size()),
				.pWaitSemaphoreInfos = waitSemaphoreInfos.data(),
				.commandBufferInfoCount = static_cast<uint32_t>(bufferSubmitInfos.size()),
				.pCommandBufferInfos = bufferSubmitInfos.data(),
				.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size()),
				.pSignalSemaphoreInfos = signalSemaphoreInfos.data(),
			};
			const auto result = vkQueueSubmit2(vulkanContext.graphicsQueue, 1, &submit,
											   vulkanContext.perFrameResources[perFrameResourceIndex].frameFinished);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Present image
		{

			const auto presentInfo =
				VkPresentInfoKHR{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
								  .pNext = nullptr,
								  .waitSemaphoreCount = 1,
								  .pWaitSemaphores =
									  &vulkanContext.perFrameResources[perFrameResourceIndex].readyToPresent,
								  .swapchainCount = 1,
								  .pSwapchains = &vulkanContext.swapchain,
								  .pImageIndices = &imageIndex,
								  .pResults = nullptr };

			const auto result = vkQueuePresentKHR(vulkanContext.graphicsQueue, &presentInfo);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

		frameIndex++;
	}

#pragma region Vulkan objects cleanup

	// wait until all submitted work is finished
	{
		const auto result = vkQueueWaitIdle(vulkanContext.graphicsQueue);
		assert(result == VK_SUCCESS);
	}

	{
		for (auto i = 0; i < vulkanContext.swapchainImageCount; i++)
		{
			vkDestroyImageView(vulkanContext.device, vulkanContext.swapchainImageViews[i], nullptr);
		}

		vkDestroySwapchainKHR(vulkanContext.device, vulkanContext.swapchain, nullptr);

		for (auto i = 0; i < vulkanContext.perFrameResources.size(); i++)
		{
			const auto& perFrameResource = vulkanContext.perFrameResources[i];
			vkDestroyFence(vulkanContext.device, perFrameResource.frameFinished, nullptr);
			vkDestroySemaphore(vulkanContext.device, perFrameResource.readyToPresent, nullptr);
			vkDestroySemaphore(vulkanContext.device, perFrameResource.readyToRender, nullptr);

			vkDestroyCommandPool(vulkanContext.device, perFrameResource.commandPool, nullptr);
		}

		SDL_Vulkan_DestroySurface(vulkanContext.instance, vulkanContext.surface, nullptr);

		fullscreenQuadPassResources.ReleaseResouces(vulkanContext.device);

		vkDestroyDevice(vulkanContext.device, nullptr);
		vkDestroyInstance(vulkanContext.instance, nullptr);
	}

#pragma endregion

	SDL_DestroyWindow(window);
	SDL_Quit();
}