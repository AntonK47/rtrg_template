#pragma once
#include <array>
#include <filesystem>
#include <vector>

#include "Core.hpp"
#include "VolkUtils.hpp"

#include "SDL3Utils.hpp"
#include "Utils.hpp"
#include "VmaUtils.hpp"

namespace Framework
{
	namespace Graphics
	{
		struct PerFrameResource
		{
			VkSemaphore readyToPresent;
			VkSemaphore readyToRender;

			VkFence frameFinished;

			VkCommandBuffer commandBuffer;
			VkCommandPool commandPool;
		};

		using DebugColor = std::array<Float, 4>;

		struct DebugColorPalette
		{
			static constexpr DebugColor Red = { 0.8f, 0.2f, 0.2f, 1.0f };
			static constexpr DebugColor Green = { 0.2f, 0.8f, 0.2f, 1.0f };
			static constexpr DebugColor Blue = { 0.2f, 0.2f, 0.8f, 1.0f };
		};

		struct GraphicsBuffer
		{
			VmaAllocation allocation;
			VkBuffer buffer;
			void* mappedPtr{ nullptr };
		};

		enum class MemoryUsage
		{
			gpu,
			upload
		};

		struct BufferDesc
		{
			U32 size{ 0 };
			VkBufferUsageFlags usage{ 0 };
			MemoryUsage memoryUsage{ MemoryUsage::gpu };
			const char* debugName = "";
		};

		struct VulkanContext
		{
			VkInstance instance;
			VkPhysicalDevice physicalDevice;
			VkDevice device;

			VmaAllocator allocator;

			uint32_t graphicsQueueFamilyIndex{};
			VkQueue graphicsQueue;

			// We assume that on the dedicated gpu we always have an graphics independent transfer queue
			uint32_t transferQueueFamilyIndex{};
			VkQueue transferQueue{};

			VkSurfaceKHR surface;
			VkSwapchainKHR swapchain;
			uint32_t swapchainImageCount{};
			VkFormat swapchainImageFormat;
			VkColorSpaceKHR swapchainImageColorSpace;
			std::vector<VkImageView> swapchainImageViews{};
			std::vector<VkImage> swapchainImages{};

			uint32_t frameResourceCount{};
			std::vector<PerFrameResource> perFrameResources{};

		public:
			void SetObjectDebugName(VkObjectType objectType, uint64_t objectHandle, const char* name) const;
			void BeginDebugLabelName(VkCommandBuffer cmd, const char* name, DebugColor color) const;
			void EndDebugLabelName(VkCommandBuffer cmd) const;

			GraphicsBuffer CreateBuffer(const BufferDesc&& desc) const;
			void DestroyBuffer(const GraphicsBuffer& buffer) const;

			VkShaderModule ShaderModuleFromFile(Utils::ShaderStage stage, const std::filesystem::path& path) const;
			VkShaderModule ShaderModuleFromText(Utils::ShaderStage stage, const std::string& text) const;

			void RecreateSwapchain(const WindowViewport& windowViewport);
			void ReleaseSwapchainResources();
			void CreateSwapchain(const WindowViewport& windowViewport);
		};
	} // namespace Graphics
} // namespace Framework