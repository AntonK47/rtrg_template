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

		using DebugColor = std::array<F32, 4>;

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

		struct ComputePipeline
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

		struct ShaderSource
		{
			std::string_view name;
			std::string_view source;
			std::string_view entryPoint{ "main" };
		};

		struct GraphicsPipelineDesc
		{
			ShaderSource vertexShader;
			ShaderSource fragmentShader;
			std::array<Format, 8> renderTargets{ Format::none, Format::none, Format::none, Format::none,
												 Format::none, Format::none, Format::none, Format::none };
			Format depthRenderTarget{ Format::none };
			PipelineState state{};
			PipelineLayout pipelineLayout{};
			const char* debugName = "";
		};

		struct ComputePipelineDesc
		{
			ShaderSource computeShader;
			PipelineLayout pipelineLayout{};
			const char* debugName = "";
		};

		struct DeviceLimits
		{
			U32 maxUniformBufferRange{};
		};

		struct VulkanContext
		{
			VkInstance instance;
			VkPhysicalDevice physicalDevice;
			VkDevice device;

			DeviceLimits limits{};

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


			template <U32 segments>
			struct DynamicUniformAllocator
			{
				GraphicsBuffer buffer;
				mutable U32 nextOffset{ 0 };
				U32 totalSize{ 0 };
				mutable U32 frameIndex{ 0 };

				struct RingSegment
				{
					U32 begin{ 0 };
					U32 end{ 0 };
				};
				mutable std::array<RingSegment, segments> ringSegments{};

				const VulkanContext* context{ nullptr };

				void Initialize(const VulkanContext& context, U32 size)
				{

					this->context = &context;
					totalSize = size;
					buffer = context.CreateBuffer(BufferDesc{ .size = size,
															  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
															  .memoryUsage = MemoryUsage::upload,
															  .debugName = "DynamicUniformBuffer" });
				}
				void Deinitialize()
				{
					context->DestroyBuffer(buffer);
				}

				void* Allocate(U32 size, U8 alignment = 8) const
				{
					auto& segment = ringSegments[frameIndex % segments];
					auto alignedPtr = AlignUp((std::byte*)buffer.mappedPtr + nextOffset, alignment);

					auto boundPtr = (std::byte*)alignedPtr + size;

					if (boundPtr > (std::byte*)buffer.mappedPtr + totalSize)
					{
						alignedPtr = buffer.mappedPtr;
						boundPtr = (std::byte*)alignedPtr + size;
					}

					assert(boundPtr > (std::byte*)buffer.mappedPtr + segment.end);
					nextOffset = (U32)((std::byte*)boundPtr - (std::byte*)buffer.mappedPtr);

					segment.end = nextOffset;
					ringSegments[(frameIndex + 1) % segments].begin = nextOffset;

					return alignedPtr;
				}
				void NextFrame() const 
				{
					frameIndex++;
					auto& segment = ringSegments[frameIndex % segments];
					nextOffset = segment.begin;
				}
			};

			DynamicUniformAllocator<3> dynamicUniformAllocator{};


#ifdef RTRG_ENABLE_PROFILER
			TracyVkCtx gpuProfilerContext;
#endif
			std::unique_ptr<Utils::ShaderCompiler> shaderCompiler; // TODO: only required for editor application

		public:
			void Initialize(std::string_view applicationName, SDL_Window* window, const WindowViewport& windowViewport);

			void Deinitialize();
			void WaitIdle();


			void SetObjectDebugName(VkObjectType objectType, uint64_t objectHandle, const char* name) const;
			void BeginDebugLabelName(VkCommandBuffer cmd, const char* name, DebugColor color) const;
			void EndDebugLabelName(VkCommandBuffer cmd) const;

			GraphicsBuffer CreateBuffer(const BufferDesc&& desc) const;
			void DestroyBuffer(const GraphicsBuffer& buffer) const;

			VkShaderModule ShaderModuleFromFile(Utils::ShaderStage stage, const std::filesystem::path& path,
												std::string_view entryPoint) const;
			VkShaderModule ShaderModuleFromText(Utils::ShaderStage stage, std::string_view shader,
												std::string_view name, std::string_view entryPoint) const;

			Utils::ShaderByteCode SpirvFromFile(Utils::ShaderStage stage, const std::filesystem::path& path,
												std::string_view entryPoint) const;
			Utils::ShaderByteCode SpirvFromText(Utils::ShaderStage stage, std::string_view shader,
												std::string_view name, std::string_view entryPoint) const;

			std::string LoadShaderFileAsText(const std::filesystem::path& path) const;


			GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc&& desc) const;
			void DestroyGraphicsPipeline(const GraphicsPipeline& pipeline) const;

			ComputePipeline CreateComputePipeline(const ComputePipelineDesc&& desc) const;
			void DestroyComputePipeline(const ComputePipeline& pipeline) const;

			void RecreateSwapchain(const WindowViewport& windowViewport);
			void ReleaseSwapchainResources();
			void CreateSwapchain(const WindowViewport& windowViewport);
		};
	} // namespace Graphics
} // namespace Framework