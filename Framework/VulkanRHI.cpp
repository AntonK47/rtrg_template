#include "VulkanRHI.hpp"

#include <fstream>

using namespace Framework::Graphics;

namespace
{
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
	assert(std::filesystem::exists(path));
	auto stream = std::ifstream{ path, std::ios::ate };

	auto size = stream.tellg();
	auto shader = std::string(size, '\0'); // construct string to stream size
	stream.seekg(0);
	stream.read(&shader[0], size);

	const auto shaderInfo = Utils::ShaderInfo{ "main", {}, stage, Utils::GlslShaderCode{ shader } };

	Utils::ShaderByteCode code;
	const auto compilationResult = Utils::CompileToSpirv(shaderInfo, code);
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

VkShaderModule VulkanContext::ShaderModuleFromText(Utils::ShaderStage stage, const std::string& shader) const
{
	const auto shaderInfo = Utils::ShaderInfo{ "main", {}, stage, Utils::GlslShaderCode{ shader } };

	Utils::ShaderByteCode code;
	const auto compilationResult = Utils::CompileToSpirv(shaderInfo, code);
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
								  .presentMode = VK_PRESENT_MODE_FIFO_KHR, // VK_PRESENT_MODE_IMMEDIATE_KHR,
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