#pragma once
#include <array>
#include <filesystem>
#include <string_view>
#include <vector>

#include "Core.hpp"
#include "VolkUtils.hpp"

#include "Profiler.hpp"
#include "SDL3Utils.hpp"
#include "Utils.hpp"
#include "VmaUtils.hpp"

#include <tracy/TracyVulkan.hpp>

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
			VmaAllocation allocation{};
			VkBuffer buffer{};
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

		enum class Format
		{
			none,
			d32f,
			rgba8unorm
		};

		inline VkFormat mapFormat(Format format)
		{
			switch (format)
			{
			case Format::none:
				return VK_FORMAT_UNDEFINED;
			case Format::rgba8unorm:
				return VK_FORMAT_R8G8B8A8_UNORM;
			case Format::d32f:
				return VK_FORMAT_D32_SFLOAT;
			}
			return VK_FORMAT_UNDEFINED;
		}

		struct GraphicsPipeline
		{
			VkPipeline pipeline;
		};

		struct PipelineLayout
		{
			VkPipelineLayout layout;
		};

		enum class FaceCullingMode
		{
			none,
			clockwise,
			conterClockwise
		};

		enum class BlendMode
		{
			none,
			alphaBlend,
			additive,
			opaque
		};

		struct PipelineState
		{
			bool enableDepthTest{ false };
			FaceCullingMode faceCullingMode{ FaceCullingMode::none };
			BlendMode blendMode{ BlendMode::none };
		};

		struct GraphicsPipelineDesc
		{
			std::string_view vertexShader;
			std::string_view fragmentShader;
			std::array<Format, 8> renderTargets{ Format::none, Format::none, Format::none, Format::none,
												 Format::none, Format::none, Format::none, Format::none };
			Format depthRenderTarget{ Format::none };
			PipelineState state{};
			PipelineLayout pipelineLayout{};
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

#ifdef RTRG_ENABLE_PROFILER
			TracyVkCtx gpuProfilerContext;
#endif
			std::unique_ptr<Utils::ShaderCompiler> shaderCompiler; //TODO: only required for editor application

		public:
			void Initialize(std::string_view applicationName, SDL_Window* window, const WindowViewport& windowViewport);

			void Deinitialize();
			void WaitIdle();


			void SetObjectDebugName(VkObjectType objectType, uint64_t objectHandle, const char* name) const;
			void BeginDebugLabelName(VkCommandBuffer cmd, const char* name, DebugColor color) const;
			void EndDebugLabelName(VkCommandBuffer cmd) const;

			GraphicsBuffer CreateBuffer(const BufferDesc&& desc) const;
			void DestroyBuffer(const GraphicsBuffer& buffer) const;

			VkShaderModule ShaderModuleFromFile(Utils::ShaderStage stage, const std::filesystem::path& path) const;
			VkShaderModule ShaderModuleFromText(Utils::ShaderStage stage, std::string_view shader) const;

			std::string LoadShaderFileAsText(const std::filesystem::path& path) const;


			GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc&& desc) const;
			void DestroyGraphicsPipeline(const GraphicsPipeline& pipeline) const;

			void RecreateSwapchain(const WindowViewport& windowViewport);
			void ReleaseSwapchainResources();
			void CreateSwapchain(const WindowViewport& windowViewport);
		};
	} // namespace Graphics
} // namespace Framework