#pragma once

#include "Core.hpp"
#include "VulkanRHI.hpp"

#include <span>

namespace Framework
{
	using BinaryData = std::span<std::byte>;
	struct OnDemandDataSource
	{
		virtual bool RequestNextChunk(BinaryData& destination, U32& size) = 0;
		virtual ~OnDemandDataSource(){}
	};

	struct LargeBufferDataSource : OnDemandDataSource
	{
		BinaryData source_;
		U32 currentChunk_{0};
		LargeBufferDataSource(BinaryData& source) : source_{source}{}

		bool RequestNextChunk(BinaryData& destination, U32& size) override;
	};

	struct UploadDestination
	{
		Graphics::GraphicsBuffer buffer;
		U32 offset;
	};

	struct TextureDestination
	{
		Graphics::GraphicsTexture2D destinationResource;
		U32 mipLevel;

	};

	struct GpuUploader
	{
		void Initialize(const Graphics::VulkanContext& context);
		void Deinitialize(const Graphics::VulkanContext& context);

		void Upload(const BinaryData& source, const UploadDestination& destination);
		void Upload(OnDemandDataSource& source, const UploadDestination& destination);

		void UploadTextureData(OnDemandDataSource& source, const TextureDestination& destination);

	private:

		Graphics::VulkanContext* rhi_;
		static constexpr VkDeviceSize stagingBufferSize{ 1 * 1024 * 1024 };
		Graphics::GraphicsBuffer stagingBuffer{};

		VkFence stagingBufferReuse;
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;
	};
} // namespace Framework