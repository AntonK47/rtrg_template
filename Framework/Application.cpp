#include "Application.hpp"

#include <array>
#include <cassert>
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
	std::vector<PerFrameResource> perFrameResouces;
	std::vector<VkImage> swapchainImages;
};

void Framework::Application::run()
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

	// ADD FUTURE INSTANCE EXTENSIONS HERE

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

#pragma region Physical Device selection
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
			const auto found = std::find_if(formats.begin(), formats.end(),
											[](const VkSurfaceFormat2KHR& format)
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

		vulkanContext.perFrameResouces.resize(frameResourceCount);
		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto fenceCreateInfo =
				VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0 };

			const auto result = vkCreateFence(vulkanContext.device, &fenceCreateInfo, nullptr,
											  &vulkanContext.perFrameResouces[i].frameFinished);
			assert(result == VK_SUCCESS);
		}

		for (auto i = 0; i < frameResourceCount; i++)
		{
			const auto semaphoreCreateInfo =
				VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
			const auto result = vkCreateSemaphore(vulkanContext.device, &semaphoreCreateInfo, nullptr,
												  &vulkanContext.perFrameResouces[i].readyToPresent);
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
													&vulkanContext.perFrameResouces[i].commandPool);
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
											 .commandPool = vulkanContext.perFrameResouces[i].commandPool,
											 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
											 .commandBufferCount = 1 };
			const auto result = vkAllocateCommandBuffers(vulkanContext.device, &allocateInfo,
														 &vulkanContext.perFrameResouces[i].commandBuffer);
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

	bool shouldRun = true;

	auto frameIndex = uint32_t{ 0 };

	while (shouldRun)
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_QUIT)
			{
				shouldRun = false;
				break;
			}
		}

		const auto perFrameResourceIndex = frameIndex % vulkanContext.swapchainImageCount;
		auto imageIndex = uint32_t{ 0 };

		{

			const auto acquireNextImageInfo =
				VkAcquireNextImageInfoKHR{ .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
										   .pNext = nullptr,
										   .swapchain = vulkanContext.swapchain,
										   .timeout = ~0ull,
										   .semaphore = VK_NULL_HANDLE,
										   .fence = vulkanContext.perFrameResouces[perFrameResourceIndex].frameFinished,
										   .deviceMask = 1 };

			const auto result = vkAcquireNextImage2KHR(vulkanContext.device, &acquireNextImageInfo, &imageIndex);
			assert(result == VK_SUCCESS);
		}

		{

			const auto result =
				vkWaitForFences(vulkanContext.device, 1,
								&vulkanContext.perFrameResouces[perFrameResourceIndex].frameFinished, VK_TRUE, ~0ull);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetCommandPool(
				vulkanContext.device, vulkanContext.perFrameResouces[perFrameResourceIndex].commandPool, 0);
			assert(result == VK_SUCCESS);
		}
		{

			const auto result = vkResetFences(vulkanContext.device, 1,
											  &vulkanContext.perFrameResouces[perFrameResourceIndex].frameFinished);
			assert(result == VK_SUCCESS);
		}


		auto& cmd = vulkanContext.perFrameResouces[perFrameResourceIndex].commandBuffer;

#pragma region Begin Command Buffer
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

		{
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
														.renderArea = VkRect2D{ .offset = VkOffset2D{ 100, 400 },
																				.extent = VkExtent2D{ 200, 300 } },
														.layerCount = 1,
														.viewMask = 0,
														.colorAttachmentCount = 1,
														.pColorAttachments = &colorAttachment,
														.pDepthAttachment = nullptr,
														.pStencilAttachment = nullptr };
			vkCmdBeginRendering(cmd, &renderingInfo);
		}


		// DO RENDERING HERE

		vkCmdEndRendering(cmd);


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
				.commandBuffer = vulkanContext.perFrameResouces[perFrameResourceIndex].commandBuffer,
				.deviceMask = 1 } };
			const auto waitSemaphoreInfos = std::array<VkSemaphoreSubmitInfo, 0>{};
			const auto signalSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.pNext = nullptr,
				.semaphore = vulkanContext.perFrameResouces[perFrameResourceIndex].readyToPresent,
				.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.deviceIndex = 1 } };
			const auto submit = VkSubmitInfo2{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
				.pNext = nullptr,
				.flags = 0,
				.waitSemaphoreInfoCount = 0, // static_cast<uint32_t>(waitSemaphoreInfos.size()),
				.pWaitSemaphoreInfos = nullptr, // waitSemaphoreInfos.data(),
				.commandBufferInfoCount = static_cast<uint32_t>(bufferSubmitInfos.size()),
				.pCommandBufferInfos = bufferSubmitInfos.data(),
				.signalSemaphoreInfoCount = static_cast<uint32_t>(signalSemaphoreInfos.size()),
				.pSignalSemaphoreInfos = signalSemaphoreInfos.data(),
			};
			const auto result = vkQueueSubmit2(vulkanContext.graphicsQueue, 1, &submit, VK_NULL_HANDLE);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

		{

			const auto presentInfo =
				VkPresentInfoKHR{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
								  .pNext = nullptr,
								  .waitSemaphoreCount = 1,
								  .pWaitSemaphores =
									  &vulkanContext.perFrameResouces[perFrameResourceIndex].readyToPresent,
								  .swapchainCount = 1,
								  .pSwapchains = &vulkanContext.swapchain,
								  .pImageIndices = &imageIndex,
								  .pResults = nullptr };

			const auto result = vkQueuePresentKHR(vulkanContext.graphicsQueue, &presentInfo);
			assert(result == VK_SUCCESS);
		}
	}

	SDL_DestroyWindow(window);

	SDL_Quit();
}