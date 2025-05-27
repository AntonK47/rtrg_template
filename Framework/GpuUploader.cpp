#include "GpuUploader.hpp"
#include "Math.hpp"

#include <queue>

using namespace Framework;
using namespace Framework::Graphics;

void Framework::GpuUploader::Initialize(const Graphics::VulkanContext& context)
{
	{
		const auto poolCreateInfo = VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
															 .pNext = nullptr,
															 .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
															 .queueFamilyIndex = context.transferQueueFamilyIndex };

		const auto result = vkCreateCommandPool(context.device, &poolCreateInfo, nullptr, &commandPool);
		assert(result == VK_SUCCESS);
	}
	{
		const auto allocateCreateInfo = VkCommandBufferAllocateInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = nullptr,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		const auto result = vkAllocateCommandBuffers(context.device, &allocateCreateInfo, &commandBuffer);
		assert(result == VK_SUCCESS);
	}


	stagingBuffer = context.CreateBuffer(
		{ stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUsage::upload, "Host Upload Staging Buffer" });


	{
		const auto fenceCreateInfo = VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
														.pNext = nullptr,
														.flags = VK_FENCE_CREATE_SIGNALED_BIT };
		const auto result = vkCreateFence(context.device, &fenceCreateInfo, nullptr, &stagingBufferReuse);
		assert(result == VK_SUCCESS);
		context.SetObjectDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)stagingBufferReuse, "Staging Buffer Reuse Fence");
	}
}

void GpuUploader::Deinitialize(const Graphics::VulkanContext& context)
{
	context.DestroyBuffer(stagingBuffer);
}

void GpuUploader::Upload(const BinaryData& source, const UploadDestination& destination)
{
}

void GpuUploader::Upload(OnDemandDataSource& source, const UploadDestination& destination)
{
	auto stagingBufferData = BinaryData{ (std::byte*)stagingBuffer.mappedPtr, (size_t)stagingBufferSize };
	auto size = U32{};
	while (source.RequestNextChunk(stagingBufferData, size))
	{

		const auto region = VkBufferCopy2{ .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
										   .pNext = nullptr,
										   .srcOffset = 0,
										   .dstOffset = destination.offset,
										   .size = size };

		const auto copyBufferInfo = VkCopyBufferInfo2{ .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
													   .pNext = nullptr,
													   .srcBuffer = stagingBuffer.buffer,
													   .dstBuffer = destination.buffer.buffer,
													   .regionCount = 1,
													   .pRegions = &region };

		{
			const auto beginInfo = VkCommandBufferBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
															 .pNext = nullptr,
															 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
															 .pInheritanceInfo = nullptr };
			const auto result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			assert(result == VK_SUCCESS);
		}

		vkCmdCopyBuffer2(commandBuffer, &copyBufferInfo);

		{
			const auto result = vkEndCommandBuffer(commandBuffer);
			assert(result == VK_SUCCESS);
		}

		const auto bufferSubmitInfos =
			std::array{ VkCommandBufferSubmitInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
												   .pNext = nullptr,
												   .commandBuffer = commandBuffer,
												   .deviceMask = 1 } };

		const auto submit = VkSubmitInfo2{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
			.pNext = nullptr,
			.flags = 0,
			.waitSemaphoreInfoCount = 0,
			.pWaitSemaphoreInfos = nullptr,
			.commandBufferInfoCount = static_cast<uint32_t>(bufferSubmitInfos.size()),
			.pCommandBufferInfos = bufferSubmitInfos.data(),
			.signalSemaphoreInfoCount = 0,
			.pSignalSemaphoreInfos = nullptr,
		};
		const auto result = vkQueueSubmit2(rhi_->transferQueue, 1, &submit, stagingBufferReuse);
		assert(result == VK_SUCCESS);


		{
			const auto result = vkWaitForFences(rhi_->device, 1, &stagingBufferReuse, VK_TRUE, ~0ull);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetFences(rhi_->device, 1, &stagingBufferReuse);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetCommandPool(rhi_->device, commandPool, 0);
			assert(result == VK_SUCCESS);
		}
	}
}

void GpuUploader::UploadTextureData(OnDemandDataSource& source, const TextureDestination& destination)
{
	auto stagingBufferData = BinaryData{ (std::byte*)stagingBuffer.mappedPtr, (size_t)stagingBufferSize };
	auto size = U32{};

	const auto texelSize = texelSizeInBytes(destination.destinationResource.format);
	const auto rowSizeInBytes = destination.destinationResource.width * texelSize;
	const auto staggedTexels = (U32)stagingBufferSize / texelSize;

	auto texelOffset = U32{};

	while (source.RequestNextChunk(stagingBufferData, size))
	{
		{
			const auto beginInfo = VkCommandBufferBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
															 .pNext = nullptr,
															 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
															 .pInheritanceInfo = nullptr };
			const auto result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
			assert(result == VK_SUCCESS);
		}

		const auto rest = texelOffset % destination.destinationResource.width;
		const auto row = texelOffset / destination.destinationResource.width;

		if (rest != 0)
		{
			const auto texels = Math::Min(staggedTexels, destination.destinationResource.width - rest);

			const auto region =
				VkBufferImageCopy2{ .sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
									.pNext = nullptr,
									.bufferOffset = 0,
									.bufferRowLength = 0,
									.bufferImageHeight = 0,
									.imageSubresource = VkImageSubresourceLayers{ .aspectMask = VK_IMAGE_ASPECT_NONE,
																				  .mipLevel = destination.mipLevel,
																				  .baseArrayLayer = 0,
																				  .layerCount = 1 },
									.imageOffset = VkOffset3D{ (I32)rest, (I32)row, 1 },
									.imageExtent = VkExtent3D{ texels, 1, 1 }
				};

			texelOffset += texels;
		}




		if (size < rowSizeInBytes)

			const auto region = VkBufferImageCopy2{
				.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
				.pNext = nullptr,
				.bufferOffset = 0,
				.bufferRowLength = 0,
				.bufferImageHeight = 0,
				.imageSubresource = VkImageSubresourceLayers{ .aspectMask = VK_IMAGE_ASPECT_NONE,
															  .mipLevel = destination.mipLevel,
															  .baseArrayLayer = 0,
															  .layerCount = 1 },
				//.imageOffset =

			};

		const auto copyBufferToImageInfo =
			VkCopyBufferToImageInfo2{ .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2,
									  .pNext = nullptr,
									  .srcBuffer = stagingBuffer.buffer,
									  .dstImage = destination.destinationResource.image,
									  .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
									  .regionCount = 1 };

		vkCmdCopyBufferToImage2(commandBuffer, &copyBufferToImageInfo);
	}
}

bool LargeBufferDataSource::RequestNextChunk(BinaryData& destination, U32& size)
{
	const auto dstSize = destination.size();
	const auto sourceSize = source_.size();

	const auto offset = currentChunk_ * dstSize;

	size = std::min({ sourceSize - offset, dstSize });
	currentChunk_++;

	std::memcpy(source_.data(), destination.data(), size);

	return (offset + size) < sourceSize;
}
