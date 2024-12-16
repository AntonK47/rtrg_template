#include "RenderPasses.hpp"

#include <fstream>
#include <regex>

#include "ImGuiUtils.hpp"

using namespace Framework;
using namespace Framework::Graphics;

void BasicGeometryPass::Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const Scene& scene,
								const FrameData::PerFrameResources& frame, const Camera& camera,
								const WindowViewport windowViewport, Float deltaTime)
{
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

	auto model = glm::rotate(glm::identity<glm::mat4>(), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
	constantsData.model = model;
	const auto aspectRatio = static_cast<float>(windowViewport.width) / static_cast<float>(windowViewport.height);
	const auto projection = glm::perspective(glm::radians(60.0f), aspectRatio, 0.001f, 100.0f);
	const auto view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);
	constantsData.viewProjection = projection * view;
	constantsData.view = view;
	constantsData.viewPositionWS = camera.position;

	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, psoCache[0]);
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

		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShaderToyConstant), &constants);
		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 32, sizeof(ConstantsData), &constantsData);


		const auto descriptorSets = std::array{ scene.geometryDescriptorSet, frame.jointsMatricesDescriptorSet };


		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0,
								static_cast<uint32_t>(descriptorSets.size()), descriptorSets.data(), 0, nullptr);

		for (auto i = 0; i < scene.meshes.size(); i++)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, psoCache[i % psoCache.size()]);
			auto& mesh = scene.meshes[i];
			vkCmdDraw(cmd, mesh.indicesCount, 1, 0, i);
			// break;
		}
		// vkCmdDraw(cmd, 100000, 1, 0, 0);
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
							   .format = depthFormat,
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
								   .format = depthFormat,
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

	auto stream = std::ifstream{ "Assets/Shaders/BasicGeometry_Template.frag", std::ios::ate };

	auto size = stream.tellg();
	auto shader = std::string(size, '\0'); // construct string to stream size
	stream.seekg(0);
	stream.read(&shader[0], size);

	shader = std::regex_replace(shader, std::regex("%%material_evaluation_code%%"), materialAsset.surfaceShadingCode);

	VkShaderModule fragmentShaderModule = context.ShaderModuleFromText(Utils::ShaderStage::Fragment, shader);

	VkShaderModule vertexShaderModule =
		context.ShaderModuleFromFile(Utils::ShaderStage::Vertex, "Assets/Shaders/BasicGeometry.vert");


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

	const auto rasterizationState =
		VkPipelineRasterizationStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
												.pNext = nullptr,
												.flags = 0,
												.depthClampEnable = VK_FALSE,
												.rasterizerDiscardEnable = VK_FALSE,
												.polygonMode = VK_POLYGON_MODE_FILL,
												.cullMode = VK_CULL_MODE_BACK_BIT,
												.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
												.depthBiasEnable = VK_FALSE,
												.lineWidth = 1.0f };

	const auto multisampleState =
		VkPipelineMultisampleStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
											  .pNext = nullptr,
											  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
											  .sampleShadingEnable = VK_FALSE,
											  .alphaToCoverageEnable = VK_FALSE };

	const auto depthStencilState =
		VkPipelineDepthStencilStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
											   .pNext = nullptr,
											   .flags = 0,
											   .depthTestEnable = VK_TRUE,
											   .depthWriteEnable = VK_TRUE,
											   .depthCompareOp = VK_COMPARE_OP_LESS,
											   .depthBoundsTestEnable = VK_FALSE,
											   .stencilTestEnable = VK_FALSE };

	const auto blendAttachment =
		VkPipelineColorBlendAttachmentState{ .blendEnable = VK_TRUE,
											 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
											 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											 .colorBlendOp = VK_BLEND_OP_ADD,
											 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
											 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											 .alphaBlendOp = VK_BLEND_OP_ADD,
											 .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
												 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT };

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
									  .layout = pipelineLayout,
									  .renderPass = VK_NULL_HANDLE,
									  .subpass = 0 };

	VkPipeline pipeline;
	const auto result =
		vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(result == VK_SUCCESS);

	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
	psoCache.push_back(pipeline);
}

VkPipeline BasicGeometryPass::CompileOpaqueMaterialPsoOnly(const VulkanContext& context,
														   const MaterialAsset& materialAsset)
{

	auto stream = std::ifstream{ "Assets/Shaders/BasicGeometry_Template.frag", std::ios::ate };

	auto size = stream.tellg();
	auto shader = std::string(size, '\0'); // construct string to stream size
	stream.seekg(0);
	stream.read(&shader[0], size);

	shader = std::regex_replace(shader, std::regex("%%material_evaluation_code%%"), materialAsset.surfaceShadingCode);

	VkShaderModule fragmentShaderModule = context.ShaderModuleFromText(Utils::ShaderStage::Fragment, shader);

	VkShaderModule vertexShaderModule =
		context.ShaderModuleFromFile(Utils::ShaderStage::Vertex, "Assets/Shaders/BasicGeometry.vert");


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

	const auto rasterizationState =
		VkPipelineRasterizationStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
												.pNext = nullptr,
												.flags = 0,
												.depthClampEnable = VK_FALSE,
												.rasterizerDiscardEnable = VK_FALSE,
												.polygonMode = VK_POLYGON_MODE_FILL,
												.cullMode = VK_CULL_MODE_BACK_BIT,
												.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
												.depthBiasEnable = VK_FALSE,
												.lineWidth = 1.0f };

	const auto multisampleState =
		VkPipelineMultisampleStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
											  .pNext = nullptr,
											  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
											  .sampleShadingEnable = VK_FALSE,
											  .alphaToCoverageEnable = VK_FALSE };

	const auto depthStencilState =
		VkPipelineDepthStencilStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
											   .pNext = nullptr,
											   .flags = 0,
											   .depthTestEnable = VK_TRUE,
											   .depthWriteEnable = VK_TRUE,
											   .depthCompareOp = VK_COMPARE_OP_LESS,
											   .depthBoundsTestEnable = VK_FALSE,
											   .stencilTestEnable = VK_FALSE };

	const auto blendAttachment =
		VkPipelineColorBlendAttachmentState{ .blendEnable = VK_TRUE,
											 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
											 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											 .colorBlendOp = VK_BLEND_OP_ADD,
											 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
											 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											 .alphaBlendOp = VK_BLEND_OP_ADD,
											 .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
												 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT };

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
									  .layout = pipelineLayout,
									  .renderPass = VK_NULL_HANDLE,
									  .subpass = 0 };

	VkPipeline pipeline;
	const auto result =
		vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(result == VK_SUCCESS);

	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
	return pipeline;
}

void BasicGeometryPass::CreateResources(const VulkanContext& context, Scene& scene, FrameData& frameData,
										const WindowViewport& windowViewport)
{
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
		const auto result = vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		assert(result == VK_SUCCESS);
	}


	VkShaderModule fragmentShaderModule =
		context.ShaderModuleFromFile(Utils::ShaderStage::Fragment, "Assets/Shaders/BasicGeometry.frag");
	VkShaderModule vertexShaderModule =
		context.ShaderModuleFromFile(Utils::ShaderStage::Vertex, "Assets/Shaders/BasicGeometry.vert");

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


	pipelineRendering = VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
													   .pNext = nullptr,
													   .viewMask = 0,
													   .colorAttachmentCount = 1,
													   .pColorAttachmentFormats = &context.swapchainImageFormat,
													   .depthAttachmentFormat = depthFormat,
													   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };


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

	const auto rasterizationState =
		VkPipelineRasterizationStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
												.pNext = nullptr,
												.flags = 0,
												.depthClampEnable = VK_FALSE,
												.rasterizerDiscardEnable = VK_FALSE,
												.polygonMode = VK_POLYGON_MODE_FILL,
												.cullMode = VK_CULL_MODE_BACK_BIT,
												.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
												.depthBiasEnable = VK_FALSE,
												.lineWidth = 1.0f };

	const auto multisampleState =
		VkPipelineMultisampleStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
											  .pNext = nullptr,
											  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
											  .sampleShadingEnable = VK_FALSE,
											  .alphaToCoverageEnable = VK_FALSE };

	const auto depthStencilState =
		VkPipelineDepthStencilStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
											   .pNext = nullptr,
											   .flags = 0,
											   .depthTestEnable = VK_TRUE,
											   .depthWriteEnable = VK_TRUE,
											   .depthCompareOp = VK_COMPARE_OP_LESS,
											   .depthBoundsTestEnable = VK_FALSE,
											   .stencilTestEnable = VK_FALSE };

	const auto blendAttachment =
		VkPipelineColorBlendAttachmentState{ .blendEnable = VK_TRUE,
											 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
											 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											 .colorBlendOp = VK_BLEND_OP_ADD,
											 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
											 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
											 .alphaBlendOp = VK_BLEND_OP_ADD,
											 .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
												 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT };

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
									  .layout = pipelineLayout,
									  .renderPass = VK_NULL_HANDLE,
									  .subpass = 0 };

	const auto result =
		vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
	assert(result == VK_SUCCESS);

	vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
	psoCache.push_back(pipeline);
	CreateViewDependentResources(context, windowViewport);
}

void BasicGeometryPass::ReleaseResources(const VulkanContext& context)
{
	ReleaseViewDependentResources(context);
	vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);

	for (auto i = 0; i < psoCache.size(); i++)
	{
		vkDestroyPipeline(context.device, psoCache[i], nullptr);
	}
}


void Framework::Graphics::FullscreenQuadPass::Execute(const VkCommandBuffer& cmd, VkImageView colorTarget,
													  const WindowViewport windowViewport, Float deltaTime)
{

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
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		const auto viewport =
			VkViewport{ 0.0f, 0.0f, static_cast<float>(windowViewport.width), static_cast<float>(windowViewport.height),
						0.0f, 1.0f };
		vkCmdSetViewport(cmd, 0, 1, &viewport);
		const auto scissor = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } };
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		time += deltaTime;


		ShaderToyConstant constants = { time, static_cast<float>(windowViewport.width),
										static_cast<float>(windowViewport.height) };

		vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
						   sizeof(ShaderToyConstant), &constants);

		vkCmdDraw(cmd, 3, 1, 0, 0);
	}
	vkCmdEndRendering(cmd);
}

void FullscreenQuadPass::CreateResources(const VulkanContext& context)
{
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
	const auto result = vkCreatePipelineLayout(context.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	assert(result == VK_SUCCESS);

	{
		VkShaderModule vertexShaderModule =
			context.ShaderModuleFromFile(Utils::ShaderStage::Vertex, "Assets/Shaders/FullscreenQuad.vert");

		VkShaderModule fragmentShaderModule =
			context.ShaderModuleFromFile(Utils::ShaderStage::Fragment, "Assets/Shaders/ShaderToySample.frag");

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


		const auto pipelineRendering =
			VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
										   .pNext = nullptr,
										   .viewMask = 0,
										   .colorAttachmentCount = 1,
										   .pColorAttachmentFormats = &context.swapchainImageFormat,
										   .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
										   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };


		const auto vertexInputState =
			VkPipelineVertexInputStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
												  .pNext = nullptr,
												  .flags = 0,
												  .vertexBindingDescriptionCount = 0,
												  .pVertexBindingDescriptions = nullptr,
												  .vertexAttributeDescriptionCount = 0,
												  .pVertexAttributeDescriptions = nullptr };

		const auto inputAssemblyState =
			VkPipelineInputAssemblyStateCreateInfo{ .sType =
														VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
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

		const auto rasterizationState =
			VkPipelineRasterizationStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
													.pNext = nullptr,
													.flags = 0,
													.depthClampEnable = VK_FALSE,
													.rasterizerDiscardEnable = VK_FALSE,
													.polygonMode = VK_POLYGON_MODE_FILL,
													.cullMode = VK_CULL_MODE_BACK_BIT,
													.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
													.depthBiasEnable = VK_FALSE,
													.lineWidth = 1.0f };

		const auto multisampleState =
			VkPipelineMultisampleStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
												  .pNext = nullptr,
												  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
												  .sampleShadingEnable = VK_FALSE,
												  .alphaToCoverageEnable = VK_FALSE };

		const auto depthStencilState =
			VkPipelineDepthStencilStateCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
												   .pNext = nullptr,
												   .flags = 0,
												   .depthTestEnable = VK_FALSE,
												   .depthWriteEnable = VK_FALSE,
												   .depthBoundsTestEnable = VK_FALSE,
												   .stencilTestEnable = VK_FALSE };

		const auto blendAttachment =
			VkPipelineColorBlendAttachmentState{ .blendEnable = VK_TRUE,
												 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
												 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
												 .colorBlendOp = VK_BLEND_OP_ADD,
												 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
												 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
												 .alphaBlendOp = VK_BLEND_OP_ADD,
												 .colorWriteMask = VK_COLOR_COMPONENT_A_BIT | VK_COLOR_COMPONENT_B_BIT |
													 VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT };

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
										  .layout = pipelineLayout,
										  .renderPass = VK_NULL_HANDLE,
										  .subpass = 0 };

		const auto result =
			vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
		assert(result == VK_SUCCESS);


		vkDestroyShaderModule(context.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(context.device, fragmentShaderModule, nullptr);
	}
}
void FullscreenQuadPass::ReleaseResources(const VulkanContext& context)
{
	vkDestroyPipelineLayout(context.device, pipelineLayout, nullptr);
	vkDestroyPipeline(context.device, pipeline, nullptr);
}


void Framework::Graphics::ImGuiPass::Execute(const VkCommandBuffer& cmd, VkImageView colorTarget,
											 const WindowViewport windowViewport, Float deltaTime)
{
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