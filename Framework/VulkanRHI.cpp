#include "VulkanRHI.hpp"
#include <fstream>
#include "Core.hpp"
#include "SDL3Utils.hpp"

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif

using namespace Framework;
using namespace Framework::Graphics;

namespace
{
	const char* const gpuMemoryPoolName = "GPU Memory Pool";

	void vmaAllocateCallback(VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory memory, VkDeviceSize size,
							 void* pUserData)
	{
		TracyAllocNS((void*)memory, size, RTRG_PROFILER_CALLSTACK_DEPTH, gpuMemoryPoolName);
	}

	void vmaFreeCallback(VmaAllocator allocator, uint32_t memoryType, VkDeviceMemory memory, VkDeviceSize size,
						 void* pUserData)
	{
		TracyFreeNS((void*)memory, RTRG_PROFILER_CALLSTACK_DEPTH, gpuMemoryPoolName);
	}

	VmaAllocationCreateInfo mapMemoryUsageToAllocationInfo(const MemoryUsage memoryUsage)
	{
		auto allocationInfo = VmaAllocationCreateInfo{};
		switch (memoryUsage)
		{
		case MemoryUsage::gpu:
			allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
			break;
		case MemoryUsage::upload:
			allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
			allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
			break;
		default:
			break;
		}

		return allocationInfo;
	}


	bool shouldMapMemory(const MemoryUsage memoryUsage)
	{
		auto shouldMap = false;
		switch (memoryUsage)
		{
		case MemoryUsage::gpu:
			break;
		case MemoryUsage::upload:
			shouldMap = true;
			break;
		default:
			break;
		}

		return shouldMap;
	}
} // namespace

void Framework::Graphics::VulkanContext::Initialize(std::string_view applicationName, SDL_Window* window,
													const WindowViewport& windowViewport)
{
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

	{
		auto instanceLayers = std::vector<const char*>{};
#ifdef RTRG_ENABLE_GRAPHICS_VALIDATION
		instanceLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif
		instanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);


		const auto applicationInfo = VkApplicationInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
														.pNext = nullptr,
														.pApplicationName = applicationName.data(),
														.applicationVersion = 0,
														.pEngineName = applicationName.data(),
														.engineVersion = 0,
														.apiVersion = VK_API_VERSION_1_3 };

		const auto instanceCreateInfo =
			VkInstanceCreateInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
								  .pNext = nullptr,
								  .flags = 0,
								  .pApplicationInfo = &applicationInfo,
								  .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
								  .ppEnabledLayerNames = instanceLayers.data(),
								  .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
								  .ppEnabledExtensionNames = instanceExtensions.data() };

		const auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
		assert(result == VK_SUCCESS);
	}

	volkLoadInstance(instance);
#pragma endregion

#pragma region Physical device selection
	{
		auto physicalDeviceCount = uint32_t{};
		auto result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
		assert(result == VK_SUCCESS);

		std::vector<VkPhysicalDevice> physicalDevices{};
		physicalDevices.resize(physicalDeviceCount);

		result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());
		assert(result == VK_SUCCESS);


		struct FamilyQueueQueryInfo
		{
			uint32_t rating{ 0 };
			bool hasGraphicsQueue{ false };
			uint32_t graphicsQueueFamilyIndex{};
			uint32_t transferQueueFamilyIndex{};
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
				auto foundGraphicsIndex = 0;

				// SEARCH FOR GRAPHICS QUEUE WITH PRESENT SUPPORT
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					const auto hasGraphicsBit = (queueFamilyProperties[i].queueFamilyProperties.queueFlags &
												 VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT;
					const auto canPresent = SDL_Vulkan_GetPresentationSupport(instance, physicalDevice, index);

					hasRequiredGraphicsQueueFamily = hasGraphicsBit && canPresent;

					if (hasRequiredGraphicsQueueFamily)
					{
						foundGraphicsIndex = index;
						break;
					}
				}

				auto hasRequiredTransferQueueFamily = false;
				auto foundTransferIndex = 0;
				// SEARCH FOR TRANSFER QUEUE
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					hasRequiredTransferQueueFamily = (queueFamilyProperties[i].queueFamilyProperties.queueFlags &
													  VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT;

					if (hasRequiredTransferQueueFamily and (index != foundGraphicsIndex))
					{
						foundTransferIndex = index;
						break;
					}
				}

				if (hasRequiredGraphicsQueueFamily and hasRequiredTransferQueueFamily)
				{
					physicalDevicesQuery[i].hasGraphicsQueue = hasRequiredGraphicsQueueFamily;
					physicalDevicesQuery[i].graphicsQueueFamilyIndex = foundGraphicsIndex;
					physicalDevicesQuery[i].transferQueueFamilyIndex = foundTransferIndex;
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


		// SELECT HEIGHT RATING DEVICE
		{
			const auto& bestCandidate = std::max_element(
				physicalDevicesQuery.begin(), physicalDevicesQuery.end(),
				[](const FamilyQueueQueryInfo& a, const FamilyQueueQueryInfo& b) { return a.rating > b.rating; });
			const auto bestCandidateIndex = std::distance(physicalDevicesQuery.begin(), bestCandidate);

			physicalDevice = physicalDevices[bestCandidateIndex];
			graphicsQueueFamilyIndex = physicalDevicesQuery[bestCandidateIndex].graphicsQueueFamilyIndex;
			transferQueueFamilyIndex = physicalDevicesQuery[bestCandidateIndex].transferQueueFamilyIndex;
			// TODO: Again, we require independent graphics and transfer queue for now!
			assert(graphicsQueueFamilyIndex != transferQueueFamilyIndex);
		}
	}
#pragma endregion

#pragma region Device creation
	{
		const auto enabledDeviceExtensions =
			std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef RTRG_ENABLE_PROFILER
						VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME, VK_KHR_CALIBRATED_TIMESTAMPS_EXTENSION_NAME
#endif
			};

		const auto queuePriority = 1.0f;
		const auto queueCreateInfos =
			std::array{ VkDeviceQueueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
												 .pNext = nullptr,
												 .flags = 0,
												 .queueFamilyIndex = graphicsQueueFamilyIndex,
												 .queueCount = 1,
												 .pQueuePriorities = &queuePriority },
						VkDeviceQueueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
												 .pNext = nullptr,
												 .flags = 0,
												 .queueFamilyIndex = transferQueueFamilyIndex,
												 .queueCount = 1,
												 .pQueuePriorities = &queuePriority } };

		auto physicalDeviceFeatures13 = VkPhysicalDeviceVulkan13Features{};
		physicalDeviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		physicalDeviceFeatures13.pNext = nullptr;
		physicalDeviceFeatures13.synchronization2 = VK_TRUE;
		physicalDeviceFeatures13.dynamicRendering = VK_TRUE;

		auto physicalDeviceFeatures12 = VkPhysicalDeviceVulkan12Features{};
		physicalDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		physicalDeviceFeatures12.pNext = &physicalDeviceFeatures13;
		physicalDeviceFeatures12.scalarBlockLayout = VK_TRUE;
#ifdef RTRG_ENABLE_PROFILER
		physicalDeviceFeatures12.hostQueryReset = VK_TRUE;
#else
		physicalDeviceFeatures12.hostQueryReset = VK_FALSE;
#endif
		auto physicalDeviceFeatures11 = VkPhysicalDeviceVulkan11Features{};
		physicalDeviceFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		physicalDeviceFeatures11.pNext = &physicalDeviceFeatures12;
		physicalDeviceFeatures11.shaderDrawParameters = VK_TRUE;


		const auto deviceCreateInfo =
			VkDeviceCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
								.pNext = &physicalDeviceFeatures11,
								.flags = 0,
								.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
								.pQueueCreateInfos = queueCreateInfos.data(),
								.enabledLayerCount = 0,
								.ppEnabledLayerNames = nullptr,
								.enabledExtensionCount = enabledDeviceExtensions.size(),
								.ppEnabledExtensionNames = enabledDeviceExtensions.data(),
								.pEnabledFeatures = nullptr };

		const auto result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
		assert(result == VK_SUCCESS);
	}
	volkLoadDevice(device);
#pragma endregion

#pragma region CreateResources VMA
	{
		const auto vulkanFunctions =
			VmaVulkanFunctions{ .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
								.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
								.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
								.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
								.vkAllocateMemory = vkAllocateMemory,
								.vkFreeMemory = vkFreeMemory,
								.vkMapMemory = vkMapMemory,
								.vkUnmapMemory = vkUnmapMemory,
								.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
								.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
								.vkBindBufferMemory = vkBindBufferMemory,
								.vkBindImageMemory = vkBindImageMemory,
								.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
								.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
								.vkCreateBuffer = vkCreateBuffer,
								.vkDestroyBuffer = vkDestroyBuffer,
								.vkCreateImage = vkCreateImage,
								.vkDestroyImage = vkDestroyImage,
								.vkCmdCopyBuffer = vkCmdCopyBuffer,
								.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
								.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
								.vkBindBufferMemory2KHR = vkBindBufferMemory2,
								.vkBindImageMemory2KHR = vkBindImageMemory2,
								.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
								.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
								.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements };


		const auto deviceMemoryCallbacks = VmaDeviceMemoryCallbacks{ .pfnAllocate = vmaAllocateCallback,
																	 .pfnFree = vmaFreeCallback,
																	 .pUserData = nullptr };

		const auto allocatorCreateInfo = VmaAllocatorCreateInfo{ .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
																 .physicalDevice = physicalDevice,
																 .device = device,
#ifdef RTRG_ENABLE_PROFILER
																 .pDeviceMemoryCallbacks = &deviceMemoryCallbacks,
#endif
																 .pVulkanFunctions = &vulkanFunctions,
																 .instance = instance,
																 .vulkanApiVersion = VK_API_VERSION_1_3 };

		const auto result = vmaCreateAllocator(&allocatorCreateInfo, &allocator);
		assert(result == VK_SUCCESS);
	}
#pragma endregion


#pragma region Swapchain creation
	{
		const auto result = SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface);
		assert(result);
	}
	{
		auto supported = VkBool32{};
		const auto result =
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, graphicsQueueFamilyIndex, surface, &supported);
		assert(result == VK_SUCCESS);
		assert(supported == VK_TRUE);
	}
	{
		CreateSwapchain(windowViewport);
	}
#pragma endregion

#pragma region Double-buffered resource creation
	// assume 2, for now
	frameResourceCount = 2;

	perFrameResources.resize(frameResourceCount);
	for (auto i = 0; i < frameResourceCount; i++)
	{
		const auto fenceCreateInfo = VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
														.pNext = nullptr,
														.flags = VK_FENCE_CREATE_SIGNALED_BIT };

		const auto result = vkCreateFence(device, &fenceCreateInfo, nullptr, &perFrameResources[i].frameFinished);
		assert(result == VK_SUCCESS);
	}

	for (auto i = 0; i < frameResourceCount; i++)
	{
		const auto semaphoreCreateInfo =
			VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
		const auto result =
			vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &perFrameResources[i].readyToPresent);
		assert(result == VK_SUCCESS);
	}

	for (auto i = 0; i < frameResourceCount; i++)
	{
		const auto semaphoreCreateInfo =
			VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
		const auto result =
			vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &perFrameResources[i].readyToRender);
		assert(result == VK_SUCCESS);
	}
#pragma endregion

#pragma region Command buffer creation

	for (auto i = 0; i < frameResourceCount; i++)
	{
		const auto poolCreateInfo = VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
															 .pNext = nullptr,
															 .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
															 .queueFamilyIndex = graphicsQueueFamilyIndex };
		const auto result = vkCreateCommandPool(device, &poolCreateInfo, nullptr, &perFrameResources[i].commandPool);
		assert(result == VK_SUCCESS);
	}

	// create only one command buffer per pool, fon now, it might change in the future
	for (auto i = 0; i < frameResourceCount; i++)
	{
		const auto allocateInfo = VkCommandBufferAllocateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
															   .pNext = nullptr,
															   .commandPool = perFrameResources[i].commandPool,
															   .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
															   .commandBufferCount = 1 };
		const auto result = vkAllocateCommandBuffers(device, &allocateInfo, &perFrameResources[i].commandBuffer);
		assert(result == VK_SUCCESS);
	}

#pragma endregion

#pragma region Request graphics and transfer queue
	{
		const auto deviceQueueInfo = VkDeviceQueueInfo2{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
														 .pNext = nullptr,
														 .flags = 0,
														 .queueFamilyIndex = graphicsQueueFamilyIndex,
														 .queueIndex = 0 };
		vkGetDeviceQueue2(device, &deviceQueueInfo, &graphicsQueue);
	}
	{
		const auto deviceQueueInfo = VkDeviceQueueInfo2{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
														 .pNext = nullptr,
														 .flags = 0,
														 .queueFamilyIndex = transferQueueFamilyIndex,
														 .queueIndex = 0 };
		vkGetDeviceQueue2(device, &deviceQueueInfo, &transferQueue);
	}
#pragma endregion

#ifdef RTRG_ENABLE_PROFILER
	gpuProfilerContext =
		TracyVkContextHostCalibrated(physicalDevice, device, vkResetQueryPool,
									 vkGetPhysicalDeviceCalibrateableTimeDomainsKHR, vkGetCalibratedTimestampsKHR);
	TracyVkContextName(gpuProfilerContext, "GPU Graphics Workload", 21);
#endif

	shaderCompiler = std::make_unique<Utils::ShaderCompiler>(
		Utils::CompilerOptions{ .optimize = false,
								.stripDebugInfo = false,
								.includePath = "Assets/Shaders/",
								.logCallback = [](const char* message)
								{
#ifdef WIN32
									OutputDebugString(runtime_format("[Shader Compiler]: {}\n", message).c_str());
#endif
								} });
}

void Framework::Graphics::VulkanContext::Deinitialize()
{
	vmaDestroyAllocator(allocator);
#ifdef RTRG_ENABLE_PROFILER
	TracyVkDestroy(gpuProfilerContext);
#endif
	{
		ReleaseSwapchainResources();

		for (auto i = 0; i < perFrameResources.size(); i++)
		{
			const auto& perFrameResource = perFrameResources[i];
			vkDestroyFence(device, perFrameResource.frameFinished, nullptr);
			vkDestroySemaphore(device, perFrameResource.readyToPresent, nullptr);
			vkDestroySemaphore(device, perFrameResource.readyToRender, nullptr);

			vkDestroyCommandPool(device, perFrameResource.commandPool, nullptr);
		}

		SDL_Vulkan_DestroySurface(instance, surface, nullptr);

		vkDestroyDevice(device, nullptr);
		vkDestroyInstance(instance, nullptr);
	}
}

void Framework::Graphics::VulkanContext::WaitIdle()
{
	// wait until all submitted work is finished
	{
		const auto result = vkQueueWaitIdle(graphicsQueue);
		assert(result == VK_SUCCESS);
	}
	{
		const auto result = vkQueueWaitIdle(transferQueue);
		assert(result == VK_SUCCESS);
	}
}

void VulkanContext::SetObjectDebugName(VkObjectType objectType, uint64_t objectHandle, const char* name) const
{

	const auto nameInfo = VkDebugUtilsObjectNameInfoEXT{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
														 .pNext = nullptr,
														 .objectType = objectType,
														 .objectHandle = objectHandle,
														 .pObjectName = name };
	const auto result = vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
	assert(result == VK_SUCCESS);
}

void VulkanContext::BeginDebugLabelName(VkCommandBuffer cmd, const char* name, DebugColor color) const
{
	const auto labelInfo = VkDebugUtilsLabelEXT{ .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
												 .pNext = nullptr,
												 .pLabelName = name,
												 .color = { color[0], color[1], color[2], color[3] } };

	vkCmdBeginDebugUtilsLabelEXT(cmd, &labelInfo);
}

void VulkanContext::EndDebugLabelName(VkCommandBuffer cmd) const
{
	vkCmdEndDebugUtilsLabelEXT(cmd);
}


GraphicsBuffer VulkanContext::CreateBuffer(const BufferDesc&& desc) const
{
	auto buffer = GraphicsBuffer{};

	const auto bufferInfo = VkBufferCreateInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
												.pNext = nullptr,
												.flags = 0,
												.size = VkDeviceSize{ desc.size },
												.usage = desc.usage,
												.sharingMode = VK_SHARING_MODE_EXCLUSIVE };

	const auto allocationInfo = mapMemoryUsageToAllocationInfo(desc.memoryUsage);

	const auto result =
		vmaCreateBuffer(allocator, &bufferInfo, &allocationInfo, &buffer.buffer, &buffer.allocation, nullptr);
	assert(result == VK_SUCCESS);
	SetObjectDebugName(VK_OBJECT_TYPE_BUFFER, (U64)buffer.buffer, desc.debugName);

	if (shouldMapMemory(desc.memoryUsage))
	{
		const auto result = vmaMapMemory(allocator, buffer.allocation, &buffer.mappedPtr);
		assert(result == VK_SUCCESS);
	}

	return buffer;
}

void VulkanContext::DestroyBuffer(const GraphicsBuffer& buffer) const
{
	if (buffer.mappedPtr != nullptr)
	{
		vmaUnmapMemory(allocator, buffer.allocation);
	}
	vmaDestroyBuffer(allocator, buffer.buffer, buffer.allocation);
}

VkShaderModule VulkanContext::ShaderModuleFromFile(Utils::ShaderStage stage, const std::filesystem::path& path) const
{
	const auto shader = LoadShaderFileAsText(path);

	return ShaderModuleFromText(stage, shader, path.filename().string());
}

VkShaderModule VulkanContext::ShaderModuleFromText(Utils::ShaderStage stage, std::string_view shader, std::string_view name) const
{
	const auto shaderInfo = Utils::ShaderInfo{ "main", {}, stage, Utils::GlslShaderCode{ shader }, true, std::string{name}};

	Utils::ShaderByteCode code;
	const auto compilationResult = shaderCompiler->CompileToSpirv(shaderInfo, code);
	assert(compilationResult == Utils::CompilationResult::Success);

	const auto shaderCreateInfo = VkShaderModuleCreateInfo{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
															.pNext = nullptr,
															.flags = 0,
															.codeSize = code.size() * 4,
															.pCode = code.data() };

	VkShaderModule shaderModule;
	const auto result = vkCreateShaderModule(device, &shaderCreateInfo, nullptr, &shaderModule);
	assert(result == VK_SUCCESS);

	return shaderModule;
}

std::string Framework::Graphics::VulkanContext::LoadShaderFileAsText(const std::filesystem::path& path) const
{
	assert(std::filesystem::exists(path));
	auto stream = std::ifstream{ path, std::ios::ate };
	const auto size = stream.tellg();
	auto shader = std::string(size, '\0');
	stream.seekg(0);
	stream.read(&shader[0], size);
	const auto charCount = stream.gcount();
	shader = shader.substr(0, charCount);
	return shader;
}

GraphicsPipeline VulkanContext::CreateGraphicsPipeline(const GraphicsPipelineDesc&& desc) const
{
	VkShaderModule fragmentShaderModule = ShaderModuleFromText(Utils::ShaderStage::Fragment, desc.fragmentShader.source, desc.fragmentShader.name);
	VkShaderModule vertexShaderModule = ShaderModuleFromText(Utils::ShaderStage::Vertex, desc.vertexShader.source, desc.vertexShader.name);


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


	const auto vertexInputState =
		VkPipelineVertexInputStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
											  .pNext = nullptr,
											  .flags = 0,
											  .vertexBindingDescriptionCount = 0,
											  .pVertexBindingDescriptions = nullptr,
											  .vertexAttributeDescriptionCount = 0,
											  .pVertexAttributeDescriptions = nullptr };

	const auto inputAssemblyState =
		VkPipelineInputAssemblyStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
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

	const auto cullMode =
		(VkCullModeFlags)((desc.state.faceCullingMode == FaceCullingMode::none) ? VK_CULL_MODE_NONE :
																				  VK_CULL_MODE_BACK_BIT);
	const auto rasterizationState = VkPipelineRasterizationStateCreateInfo{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.depthClampEnable = VK_FALSE,
		.rasterizerDiscardEnable = VK_FALSE,
		.polygonMode = VK_POLYGON_MODE_FILL,
		.cullMode = cullMode,
		.frontFace = (desc.state.faceCullingMode == FaceCullingMode::clockwise) ? VK_FRONT_FACE_CLOCKWISE :
																				  VK_FRONT_FACE_COUNTER_CLOCKWISE,
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f
	};

	const auto multisampleState =
		VkPipelineMultisampleStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
											  .pNext = nullptr,
											  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
											  .sampleShadingEnable = VK_FALSE,
											  .alphaToCoverageEnable = VK_FALSE };

	const auto enableDepthTest = desc.state.enableDepthTest ? VK_TRUE : VK_FALSE;
	const auto depthStencilState =
		VkPipelineDepthStencilStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
											   .pNext = nullptr,
											   .flags = 0,
											   .depthTestEnable = enableDepthTest,
											   .depthWriteEnable = enableDepthTest,
											   .depthCompareOp = VK_COMPARE_OP_LESS,
											   .depthBoundsTestEnable = VK_FALSE,
											   .stencilTestEnable = VK_FALSE };

	auto blendAttachment = VkPipelineColorBlendAttachmentState{};
	blendAttachment.blendEnable = VK_TRUE;

	switch (desc.state.blendMode)
	{
	case BlendMode::none:
		blendAttachment.blendEnable = VK_FALSE;
		break;
	case BlendMode::alphaBlend:
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		break;
	case BlendMode::additive:
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		break;
	case BlendMode::opaque:
		blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		break;
	}

	blendAttachment.colorBlendOp = VK_BLEND_OP_ADD, blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD,
	blendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT;

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


	auto colorAttachnmentsFormats = std::vector<VkFormat>{};
	colorAttachnmentsFormats.reserve(desc.renderTargets.size());

	for (auto i = 0; i < desc.renderTargets.size(); i++)
	{
		if (desc.renderTargets[i] == Format::none)
		{
			break;
		}
		else
		{
			colorAttachnmentsFormats.push_back(mapFormat(desc.renderTargets[i]));
		}
	}


	const auto pipelineRendering =
		VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
									   .pNext = nullptr,
									   .viewMask = 0,
									   .colorAttachmentCount = static_cast<U32>(colorAttachnmentsFormats.size()),
									   .pColorAttachmentFormats = colorAttachnmentsFormats.data(),
									   .depthAttachmentFormat = mapFormat(desc.depthRenderTarget),
									   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };

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
									  .layout = desc.pipelineLayout.layout,
									  .renderPass = VK_NULL_HANDLE,
									  .subpass = 0 };

	VkPipeline pipeline;
	const auto result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(result == VK_SUCCESS);

	vkDestroyShaderModule(device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(device, fragmentShaderModule, nullptr);

	SetObjectDebugName(VK_OBJECT_TYPE_PIPELINE, (U64)pipeline, desc.debugName);

	return GraphicsPipeline{ pipeline };
}

void Framework::Graphics::VulkanContext::DestroyGraphicsPipeline(const GraphicsPipeline& pipeline) const
{
	vkDestroyPipeline(device, pipeline.pipeline, nullptr);
}

void VulkanContext::RecreateSwapchain(const WindowViewport& windowViewport)
{
	ReleaseSwapchainResources();

	CreateSwapchain(windowViewport);
}

void VulkanContext::ReleaseSwapchainResources()
{
	for (auto i = 0; i < swapchainImageCount; i++)
	{
		vkDestroyImageView(device, swapchainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void VulkanContext::CreateSwapchain(const WindowViewport& windowViewport)
{
	{
		auto surfaceCapabilities =
			VkSurfaceCapabilities2KHR{ .sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR, .pNext = nullptr };
		const auto surfaceInfo = VkPhysicalDeviceSurfaceInfo2KHR{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, .pNext = nullptr, .surface = surface
		};
		const auto result =
			vkGetPhysicalDeviceSurfaceCapabilities2KHR(physicalDevice, &surfaceInfo, &surfaceCapabilities);
		assert(result == VK_SUCCESS);

		swapchainImageCount = std::max({ surfaceCapabilities.surfaceCapabilities.minImageCount, 3u });
	}

	{
		const auto surfaceInfo = VkPhysicalDeviceSurfaceInfo2KHR{
			.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR, .pNext = nullptr, .surface = surface
		};

		auto formatsCount = uint32_t{ 0 };
		auto result = vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &formatsCount, nullptr);
		assert(result == VK_SUCCESS);

		auto formats = std::vector<VkSurfaceFormat2KHR>{};
		formats.resize(formatsCount);

		for (auto i = 0; i < formatsCount; i++)
		{
			formats[i].sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
			formats[i].pNext = nullptr;
		}

		result = vkGetPhysicalDeviceSurfaceFormats2KHR(physicalDevice, &surfaceInfo, &formatsCount, formats.data());
		assert(result == VK_SUCCESS);

		assert(formats.size() > 0);
		// TODO: be sure the format is available on current device
		const auto found = std::find_if(formats.begin(), formats.end(), [](const VkSurfaceFormat2KHR& format)
										{ return format.surfaceFormat.format == VK_FORMAT_R8G8B8A8_UNORM; });
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


		swapchainImageFormat = format.surfaceFormat.format;
		swapchainImageColorSpace = format.surfaceFormat.colorSpace;
	}

	const auto swapchainCreateInfo =
		VkSwapchainCreateInfoKHR{ .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
								  .pNext = nullptr,
								  .flags = 0,
								  .surface = surface,
								  .minImageCount = swapchainImageCount,
								  .imageFormat = swapchainImageFormat,
								  .imageColorSpace = swapchainImageColorSpace,
								  .imageExtent =
									  VkExtent2D{ .width = windowViewport.width, .height = windowViewport.height },
								  .imageArrayLayers = 1,
								  .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
								  .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
								  .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
								  .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
								  .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR,//VK_PRESENT_MODE_FIFO_KHR, // VK_PRESENT_MODE_IMMEDIATE_KHR,
								  .clipped = VK_FALSE,
								  .oldSwapchain = VK_NULL_HANDLE };

	const auto result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain);
	assert(result == VK_SUCCESS);


#pragma region Create per swapchain image resources

	{
		auto imageCount = uint32_t{ 0 };
		auto result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
		assert(result == VK_SUCCESS);
		swapchainImages.resize(imageCount);

		result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());
		assert(result == VK_SUCCESS);
	}

	swapchainImageViews.resize(swapchainImageCount);
	for (auto i = 0; i < swapchainImageCount; i++)
	{
		const auto imageViewCreateInfo = VkImageViewCreateInfo{
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext = nullptr,
			.flags = 0,
			.image = swapchainImages[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = swapchainImageFormat,
			.components = VkComponentMapping{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
											  VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
			.subresourceRange = VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
														 .baseMipLevel = 0,
														 .levelCount = 1,
														 .baseArrayLayer = 0,
														 .layerCount = 1 }
		};
		const auto result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]);
		assert(result == VK_SUCCESS);
	}
#pragma endregion
}