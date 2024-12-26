#include "RenderPasses.hpp"

#include <fstream>
#include <regex>

#include "ImGuiUtils.hpp"
#include "Profiler.hpp"


using namespace Framework;
using namespace Framework::Graphics;

void BasicGeometryPass::Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const Scene& scene,
								const FrameData::PerFrameResources& frame, const Camera& camera,
								const WindowViewport windowViewport, F32 deltaTime)
{
	ZoneNamedNS(__tracy, "BasicGeometryPass::Execute", RTRG_PROFILER_CALLSTACK_DEPTH, true);
	TracyVkZone(vulkanContext->gpuProfilerContext, cmd, "BasicGeometryPass");
	const auto colorAttachment = VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
															.pNext = nullptr,
															.imageView = colorTarget,
															.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
															.resolveMode = VK_RESOLVE_MODE_NONE,
															.resolveImageView = VK_NULL_HANDLE,
															.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
															.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
															.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
															.clearValue = VkClearColorValue{ { 100, 0, 0, 255 } } };

	const auto depthAttachment =
		VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
								   .pNext = nullptr,
								   .imageView = depthView,
								   .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
								   .resolveMode = VK_RESOLVE_MODE_NONE,
								   .resolveImageView = VK_NULL_HANDLE,
								   .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
								   .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
								   .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
								   .clearValue = VkClearValue{ .depthStencil = VkClearDepthStencilValue{ 1.0f, 0u } } };

	const auto renderingInfo =
		VkRenderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
						 .pNext = nullptr,
						 .flags = 0,
						 .renderArea = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } },
						 .layerCount = 1,
						 .viewMask = 0,
						 .colorAttachmentCount = 1,
						 .pColorAttachments = &colorAttachment,
						 .pDepthAttachment = &depthAttachment,
						 .pStencilAttachment = nullptr };
	vkCmdBeginRendering(cmd, &renderingInfo);

	ConstantsData constantsData = ConstantsData{};

	const auto aspectRatio = static_cast<float>(windowViewport.width) / static_cast<float>(windowViewport.height);
	const auto projection = glm::perspective(glm::radians(60.0f), aspectRatio, 0.001f, 100.0f);
	const auto view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
	constantsData.viewProjection = projection * view;
	constantsData.view = view;
	constantsData.viewPositionWS = camera.position;

	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, psoCache[0].pipeline);
		const auto viewport =
			VkViewport{ 0.0f, 0.0f, static_cast<float>(windowViewport.width), static_cast<float>(windowViewport.height),
						0.0f, 1.0f };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		const auto scissor = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } };
		vkCmdSetScissor(cmd, 0, 1, &scissor);
		static float time = 0.0f;
		time += deltaTime;
		ShaderToyConstant constants = { time, static_cast<float>(windowViewport.width),
										static_cast<float>(windowViewport.height) };

		vkCmdPushConstants(cmd, pipelineLayout.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShaderToyConstant),
						   &constants);


		const auto descriptorSets = std::array{ scene.geometryDescriptorSet, frame.jointsMatricesDescriptorSet };


		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout.layout, 0,
								static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

		const auto gridSize = 10;

		const auto modelRotation =
			glm::rotate(glm::identity<glm::mat4>(), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));

		for (auto psoIndex = 0; psoIndex < psoCache.size(); psoIndex++)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, psoCache[psoIndex].pipeline);
			for (auto l = 0; l < gridSize; l++)
			{
				for (auto k = 0; k < gridSize; k++)
				{
					for (auto m = 0; m < gridSize; m++)
					{
						const auto model = glm::translate(modelRotation, Math::Vector3{ 2.0f * l, 2.0f * k, 2.0f * m });

						constantsData.model = model;
						vkCmdPushConstants(cmd, pipelineLayout.layout, VK_SHADER_STAGE_VERTEX_BIT, 32,
										   sizeof(ConstantsData), &constantsData);
						for (auto i = psoIndex; i < scene.meshes.size(); i += 3)
						{
							auto& mesh = scene.meshes[i];
							vkCmdDraw(cmd, mesh.indicesCount, 1, 0, i);
						}
					}
				}
			}
		}
	}
	vkCmdEndRendering(cmd);
}

void BasicGeometryPass::CreateViewDependentResources(const VulkanContext& context, const WindowViewport& windowViewport)
{
	{

		const auto imageCreateInfo =
			VkImageCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
							   .pNext = nullptr,
							   .flags = 0,
							   .imageType = VK_IMAGE_TYPE_2D,
							   .format = mapFormat(depthFormat),
							   .extent = VkExtent3D{ windowViewport.width, windowViewport.height, 1 },
							   .mipLevels = 1,
							   .arrayLayers = 1,
							   .samples = VK_SAMPLE_COUNT_1_BIT,
							   .tiling = VK_IMAGE_TILING_OPTIMAL,
							   .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
							   .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
							   .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED };

		const auto allocationInfo = VmaAllocationCreateInfo{ .usage = VMA_MEMORY_USAGE_GPU_ONLY };
		const auto result = vmaCreateImage(context.allocator, &imageCreateInfo, &allocationInfo, &depthImage,
										   &depthImageAllocation, nullptr);
		assert(result == VK_SUCCESS);
	}

	{
		const auto imageViewCreateInfo =
			VkImageViewCreateInfo{ .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
								   .pNext = nullptr,
								   .flags = 0,
								   .image = depthImage,
								   .viewType = VK_IMAGE_VIEW_TYPE_2D,
								   .format = mapFormat(depthFormat),
								   .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
												   VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
								   .subresourceRange = VkImageSubresourceRange{ .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
																				.baseMipLevel = 0,
																				.levelCount = 1,
																				.baseArrayLayer = 0,
																				.layerCount = 1 }

			};
		const auto result = vkCreateImageView(context.device, &imageViewCreateInfo, nullptr, &depthView);
		assert(result == VK_SUCCESS);
	}
}

void BasicGeometryPass::ReleaseViewDependentResources(const VulkanContext& context)
{
	vkDestroyImageView(context.device, depthView, nullptr);
	vmaDestroyImage(context.allocator, depthImage, depthImageAllocation);
}

void BasicGeometryPass::RecreateViewDependentResources(const VulkanContext& context,
													   const WindowViewport& windowViewport)
{
	ReleaseViewDependentResources(context);
	CreateViewDependentResources(context, windowViewport);
}

void BasicGeometryPass::CompileOpaqueMaterial(const VulkanContext& context, const MaterialAsset& materialAsset)
{
	pipeline = CompileOpaqueMaterialPsoOnly(context, materialAsset);
	psoCache.push_back(pipeline);
}

GraphicsPipeline BasicGeometryPass::CompileOpaqueMaterialPsoOnly(const VulkanContext& context,
																 const MaterialAsset& materialAsset)
{
	auto fragmentShader = context.LoadShaderFileAsText("Assets/Shaders/BasicGeometry_Template.frag");
	fragmentShader = std::regex_replace(fragmentShader, std::regex("%%material_evaluation_code%%"),
										materialAsset.surfaceShadingCode);

	const auto hash = std::hash<std::string>{}(fragmentShader);
	const auto vertexShader = context.LoadShaderFileAsText("Assets/Shaders/BasicSkinnedGeometry.vert");

	const auto pipeline = context.CreateGraphicsPipeline(GraphicsPipelineDesc{
		.vertexShader = { std::string{ runtime_format("BasicGeometry.vert") }, vertexShader },
		.fragmentShader = { std::string{ runtime_format("BasicGeometry.generated.{}.frag", hash) }, fragmentShader },
		.renderTargets = { Format::rgba8unorm },
		.depthRenderTarget = depthFormat,
		.state = PipelineState{ .enableDepthTest = true,
								.faceCullingMode = FaceCullingMode::conterClockwise,
								.blendMode = BlendMode::none },
		.pipelineLayout = pipelineLayout,
		.debugName = "Generated Geometry PSO" });
	return pipeline;
}

void BasicGeometryPass::CreateResources(const VulkanContext& context, Scene& scene, FrameData& frameData,
										const WindowViewport& windowViewport)
{
	vulkanContext = &context;
	{
		const auto pushConstants = std::array{
			VkPushConstantRange{
				.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT, .offset = 0, .size = sizeof(ShaderToyConstant) },
			VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, .offset = 32, .size = sizeof(ConstantsData) }
		};

		const auto setLayouts = std::array{ scene.geometryDescriptorSetLayout, frameData.frameDescriptorSetLayout };

		const auto pipelineLayoutCreateInfo =
			VkPipelineLayoutCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
										.pNext = nullptr,
										.flags = 0,
										.setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
										.pSetLayouts = setLayouts.data(),
										.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
										.pPushConstantRanges = pushConstants.data() };
		const auto result =
			vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout.layout);
		assert(result == VK_SUCCESS);
	}

	const auto vertexShader = context.LoadShaderFileAsText("Assets/Shaders/BasicGeometry.vert");
	const auto fragmentShader = context.LoadShaderFileAsText("Assets/Shaders/BasicGeometry.frag");
	pipeline = context.CreateGraphicsPipeline(
		GraphicsPipelineDesc{ .vertexShader = { "BasicGeometry.vert", vertexShader },
							  .fragmentShader = { "BasicGeometry.frag", fragmentShader },
							  .renderTargets = { Format::rgba8unorm },
							  .depthRenderTarget = depthFormat,
							  .state = PipelineState{ .enableDepthTest = true,
													  .faceCullingMode = FaceCullingMode::conterClockwise,
													  .blendMode = BlendMode::none },
							  .pipelineLayout = pipelineLayout,
							  .debugName = "Default Geometry PSO" });
	psoCache.push_back(pipeline);
	CreateViewDependentResources(context, windowViewport);
}

void BasicGeometryPass::ReleaseResources(const VulkanContext& context)
{
	ReleaseViewDependentResources(context);
	vkDestroyPipelineLayout(context.device, pipelineLayout.layout, nullptr);

	for (auto i = 0; i < psoCache.size(); i++)
	{
		context.DestroyGraphicsPipeline(psoCache[i]);
	}
}


void Framework::Graphics::FullscreenQuadPass::Execute(const VkCommandBuffer& cmd, VkImageView colorTarget,
													  const WindowViewport windowViewport, F32 deltaTime)
{
	ZoneNamedNS(__tracy, "FullscreenQuadPass::Execute", RTRG_PROFILER_CALLSTACK_DEPTH, true);
	TracyVkZone(vulkanContext->gpuProfilerContext, cmd, "FullscreenQuadPass");

	const auto colorAttachment = VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
															.pNext = nullptr,
															.imageView = colorTarget,
															.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
															.resolveMode = VK_RESOLVE_MODE_NONE,
															.resolveImageView = VK_NULL_HANDLE,
															.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
															.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
															.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
															.clearValue = VkClearColorValue{ { 100, 0, 0, 255 } } };

	const auto renderingInfo =
		VkRenderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
						 .pNext = nullptr,
						 .flags = 0,
						 .renderArea = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } },
						 .layerCount = 1,
						 .viewMask = 0,
						 .colorAttachmentCount = 1,
						 .pColorAttachments = &colorAttachment,
						 .pDepthAttachment = nullptr,
						 .pStencilAttachment = nullptr };
	vkCmdBeginRendering(cmd, &renderingInfo);
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
		const auto viewport =
			VkViewport{ 0.0f, 0.0f, static_cast<float>(windowViewport.width), static_cast<float>(windowViewport.height),
						0.0f, 1.0f };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		const auto scissor = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } };
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		time += deltaTime;


		ShaderToyConstant constants = { time, static_cast<float>(windowViewport.width),
										static_cast<float>(windowViewport.height) };

		vkCmdPushConstants(cmd, pipelineLayout.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShaderToyConstant),
						   &constants);

		vkCmdDraw(cmd, 3, 1, 0, 0);
	}
	vkCmdEndRendering(cmd);
}

void FullscreenQuadPass::CreateResources(const VulkanContext& context)
{
	vulkanContext = &context;
	const auto pushConstantRange = VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
														.offset = 0,
														.size = sizeof(ShaderToyConstant) };

	const auto pipelineLayoutCreateInfo =
		VkPipelineLayoutCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
									.pNext = nullptr,
									.flags = 0,
									.setLayoutCount = 0,
									.pSetLayouts = nullptr,
									.pushConstantRangeCount = 1,
									.pPushConstantRanges = &pushConstantRange };
	const auto result =
		vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout.layout);
	assert(result == VK_SUCCESS);

	const auto vertexShader = context.LoadShaderFileAsText("Assets/Shaders/FullscreenQuad.vert");
	const auto fragmentShader = context.LoadShaderFileAsText("Assets/Shaders/ShaderToySample.frag");
	pipeline = context.CreateGraphicsPipeline(
		GraphicsPipelineDesc{ .vertexShader = { "FullscreenQuad.vert", vertexShader },
							  .fragmentShader = { "ShaderToySample.frag", fragmentShader },
							  .renderTargets = { Format::rgba8unorm },
							  .depthRenderTarget = Format::none,
							  .state = PipelineState{ .enableDepthTest = false,
													  .faceCullingMode = FaceCullingMode::conterClockwise,
													  .blendMode = BlendMode::none },
							  .pipelineLayout = pipelineLayout,
							  .debugName = "Background PSO" });
}
void FullscreenQuadPass::ReleaseResources(const VulkanContext& context)
{
	vkDestroyPipelineLayout(context.device, pipelineLayout.layout, nullptr);
	context.DestroyGraphicsPipeline(pipeline);
}


void Framework::Graphics::ImGuiPass::Execute(const VkCommandBuffer& cmd, VkImageView colorTarget,
											 const WindowViewport windowViewport, F32 deltaTime)
{
	ZoneNamedNS(__tracy, "ImGuiPass::Execute", RTRG_PROFILER_CALLSTACK_DEPTH, true);
	TracyVkZone(vulkanContext->gpuProfilerContext, cmd, "ImGuiPass");
	ImGui::Render();
	ImDrawData* drawData = ImGui::GetDrawData();

	const auto colorAttachment = VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
															.pNext = nullptr,
															.imageView = colorTarget,
															.imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL,
															.resolveMode = VK_RESOLVE_MODE_NONE,
															.resolveImageView = VK_NULL_HANDLE,
															.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
															.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
															.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
															.clearValue = VkClearColorValue{ { 100, 0, 0, 255 } } };

	const auto renderingInfo =
		VkRenderingInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
						 .pNext = nullptr,
						 .flags = 0,
						 .renderArea = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } },
						 .layerCount = 1,
						 .viewMask = 0,
						 .colorAttachmentCount = 1,
						 .pColorAttachments = &colorAttachment,
						 .pDepthAttachment = nullptr,
						 .pStencilAttachment = nullptr };

	vkCmdBeginRendering(cmd, &renderingInfo);
	ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
	vkCmdEndRendering(cmd);
}

void ImGuiPass::CreateResources(const VulkanContext& context)
{
	vulkanContext = &context;
	const auto poolSizes =
		std::array{ VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1 } };
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	const auto result = vkCreateDescriptorPool(context.device, &poolInfo, nullptr, &descriptorPool);
	assert(result == VK_SUCCESS);
}

void ImGuiPass::ReleaseResources(const VulkanContext& context)
{
	vkDestroyDescriptorPool(context.device, descriptorPool, nullptr);
}