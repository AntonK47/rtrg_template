#include "BasicRenderPipeline.hpp"

#define TRACY_NO_SAMPLING
#include <tracy/Tracy.hpp>
#include <tracy/TracyVulkan.hpp>

namespace
{
	const char* sample_surface_01 =
		"void surface(in Geometry geometry, out vec4 color){ color = vec4(1.0f,0.0f,0.0f,1.0f);}";
	const char* sample_surface_02 =
		"void surface(in Geometry geometry, out vec4 color){ color = vec4(0.0f,1.0f,0.0f,1.0f);}";
} // namespace

void Framework::Graphics::BasicRenderPipeline::Initialize(const VulkanContext& context,
														  const WindowViewport& windowViewport)
{

	frameData.CreateResources(context, context.frameResourceCount);

	scene.CreateResources(context);

	basicGeometryPass.CreateResources(context, scene, frameData, windowViewport);

	MaterialAsset material01 = MaterialAsset{ sample_surface_01 };
	MaterialAsset material02 = MaterialAsset{ sample_surface_02 };

	basicGeometryPass.CompileOpaqueMaterial(context, material01);
	basicGeometryPass.CompileOpaqueMaterial(context, material02);

	fullscreenQuadPass.CreateResources(context);

	imGuiPass.CreateResources(context);
}

void Framework::Graphics::BasicRenderPipeline::Deinitialize(const VulkanContext& context)
{
	frameData.ReleaseResources(context);
	scene.ReleaseResources(context);
	imGuiPass.ReleaseResources(context);
	basicGeometryPass.ReleaseResources(context);
	fullscreenQuadPass.ReleaseResources(context);
}

void Framework::Graphics::BasicRenderPipeline::Execute(const VulkanContext& context,
													   const WindowViewport& windowViewport, const Camera& camera,
													   Float deltaTime)
{
	const auto perFrameResourceIndex = frameIndex % context.frameResourceCount;

#pragma region Wait for resource reuse
	{

		const auto result = vkWaitForFences(
			context.device, 1, &context.perFrameResources[perFrameResourceIndex].frameFinished, VK_TRUE, ~0ull);
		assert(result == VK_SUCCESS);
	}
	{
		const auto result =
			vkResetFences(context.device, 1, &context.perFrameResources[perFrameResourceIndex].frameFinished);
		assert(result == VK_SUCCESS);
	}
	{
		const auto result =
			vkResetCommandPool(context.device, context.perFrameResources[perFrameResourceIndex].commandPool, 0);
		assert(result == VK_SUCCESS);
	}
#pragma endregion

#pragma region Acquire Swapchain image
	auto imageIndex = uint32_t{ 0 };

	{

		const auto acquireNextImageInfo =
			VkAcquireNextImageInfoKHR{ .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
									   .pNext = nullptr,
									   .swapchain = context.swapchain,
									   .timeout = ~0ull,
									   .semaphore = context.perFrameResources[perFrameResourceIndex].readyToRender,
									   .fence = VK_NULL_HANDLE,
									   .deviceMask = 1 };

		const auto result = vkAcquireNextImage2KHR(context.device, &acquireNextImageInfo, &imageIndex);
		assert(result == VK_SUCCESS);
	}
#pragma endregion

#pragma region Begin CommandBuffer
	auto& cmd = context.perFrameResources[perFrameResourceIndex].commandBuffer;
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
								   .image = context.swapchainImages[imageIndex],
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
	

	context.BeginDebugLabelName(cmd, "Background Rendering", DebugColorPalette::Red);
	fullscreenQuadPass.Execute(cmd, context.swapchainImageViews[imageIndex], windowViewport, deltaTime);
	context.EndDebugLabelName(cmd);
	context.BeginDebugLabelName(cmd, "Mesh Rendering", DebugColorPalette::Green);

	const auto descriptorBufferInfo = VkDescriptorBufferInfo{
		.buffer = frameData.uniformBuffer.buffer,
		.offset = frameData.jointMatricesOffset,
		.range = frameData.jointMatricesSize,
	};

	const auto dsWrite =
		VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
							  .pNext = nullptr,
							  .dstSet = frameData.perFrameResources[perFrameResourceIndex].jointsMatricesDescriptorSet,
							  .dstBinding = 0,
							  .dstArrayElement = 0,
							  .descriptorCount = 1,
							  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							  .pImageInfo = nullptr,
							  .pBufferInfo = &descriptorBufferInfo,
							  .pTexelBufferView = nullptr };

	vkUpdateDescriptorSets(context.device, 1, &dsWrite, 0, nullptr);

	basicGeometryPass.Execute(cmd, context.swapchainImageViews[imageIndex], scene,
							  frameData.perFrameResources[perFrameResourceIndex], camera, windowViewport, deltaTime);
	context.EndDebugLabelName(cmd);
	context.BeginDebugLabelName(cmd, "GUI Rendering", DebugColorPalette::Blue);
	imGuiPass.Execute(cmd, context.swapchainImageViews[imageIndex], windowViewport, deltaTime);
	context.EndDebugLabelName(cmd);


	 TracyVkCollect(context.tracyVulkanContext, cmd);

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
								   .image = context.swapchainImages[imageIndex],
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

#pragma region Submit CommandBuffer
	{
		const auto bufferSubmitInfos = std::array{ VkCommandBufferSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
			.pNext = nullptr,
			.commandBuffer = context.perFrameResources[perFrameResourceIndex].commandBuffer,
			.deviceMask = 1 } };
		const auto waitSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = context.perFrameResources[perFrameResourceIndex].readyToRender,
			.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
			.deviceIndex = 1 } };
		const auto signalSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
			.pNext = nullptr,
			.semaphore = context.perFrameResources[perFrameResourceIndex].readyToPresent,
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
		const auto result = vkQueueSubmit2(context.graphicsQueue, 1, &submit,
										   context.perFrameResources[perFrameResourceIndex].frameFinished);
		assert(result == VK_SUCCESS);
	}
#pragma endregion

#pragma region Present image
	{

		const auto presentInfo =
			VkPresentInfoKHR{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
							  .pNext = nullptr,
							  .waitSemaphoreCount = 1,
							  .pWaitSemaphores = &context.perFrameResources[perFrameResourceIndex].readyToPresent,
							  .swapchainCount = 1,
							  .pSwapchains = &context.swapchain,
							  .pImageIndices = &imageIndex,
							  .pResults = nullptr };

		const auto result = vkQueuePresentKHR(context.graphicsQueue, &presentInfo);
		assert(result == VK_SUCCESS);
	}
#pragma endregion
	frameIndex++;
}
