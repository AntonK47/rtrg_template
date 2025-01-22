#include "UnwrapGeometryForAccelerationStructurePass.hpp"

using namespace Framework;
using namespace Framework::Graphics;

void UnwrapGeometryForAccelerationStructurePass::UnwrapMesh(const VkCommandBuffer& cmd, const IndexedStaticMesh& mesh)
{
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout.layout, 0, descriptorSets.size(),
							descriptorSets.data(), 0, nullptr);

	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, unwrapPositionComputePipeline.pipeline);
		auto args = UnwrapComputeConstants{};
		args.verticesOffset = mesh.verticesOffset;
		args.verticesStride = 8;
		args.verticesCount = mesh.verticesCount;
		args.dstOffset = 0;
		vkCmdPushConstants(cmd, pipelineLayout.layout, VK_SHADER_STAGE_ALL, 0, sizeof(UnwrapComputeConstants), &args);
		auto groups = (mesh.verticesCount + 31) / 32;
		assert(groups > 0);
		vkCmdDispatch(cmd, groups, 1, 1);
	}
	{
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, unwrapIndiciesComputePipeline.pipeline);
		auto args = UnwrapComputeConstants{};
		args.verticesOffset = mesh.indicesOffset;
		args.verticesStride = 1;
		args.verticesCount = mesh.indicesCount;
		args.dstOffset = mesh.verticesCount * 3;
		vkCmdPushConstants(cmd, pipelineLayout.layout, VK_SHADER_STAGE_ALL, 0, sizeof(UnwrapComputeConstants), &args);
		auto groups = (mesh.indicesCount + 31) / 32;
		assert(groups > 0);
		vkCmdDispatch(cmd, groups, 1, 1);
	}

	{
		const auto bufferMemoryBarrier = VkBufferMemoryBarrier2{
			.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
			.pNext = nullptr,
			.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
			.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
			.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.buffer = unwrappedGeometryBuffer.buffer,
			.offset = 0,
			.size = VK_WHOLE_SIZE,
		};

		const auto dependencyInfo = VkDependencyInfo{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.pNext = nullptr,
			.dependencyFlags = 0,
			.memoryBarrierCount = 0,
			.pMemoryBarriers = nullptr,
			.bufferMemoryBarrierCount = 1,
			.pBufferMemoryBarriers = &bufferMemoryBarrier,
			.imageMemoryBarrierCount = 0,
			.pImageMemoryBarriers = nullptr,
		};

		vkCmdPipelineBarrier2(cmd, &dependencyInfo);
	}
}

void UnwrapGeometryForAccelerationStructurePass::CreateResources(const VulkanContext& context, const Scene& scene)
{
	unwrappedGeometryBuffer = context.CreateBuffer(
		BufferDesc{ .size = 128 * 1024 * 1024,
					.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
					.memoryUsage = MemoryUsage::gpu,
					.debugName = "unwrappedGeometryBuffer" });


	bindGroupLayout = context.CreateBindGroupLayout(BindGroupLayoutDesc{ .bindings = { StorageBufferBinding{} } });

	pipelineLayout = context.CreatePipelineLayout(PipelineLayoutDesc{
		.bindGroupLayouts = { scene.geometryBindGroupLayout, bindGroupLayout },
		.pushConstantRanges = { VkPushConstantRange{
			.stageFlags = VK_SHADER_STAGE_ALL, .offset = 0, .size = sizeof(UnwrapComputeConstants) } } });


	const auto shaderCode = context.LoadShaderFileAsText("Assets/Shaders/LightBacker/UnwrapGeometryForAs.comp");
	unwrapPositionComputePipeline = context.CreateComputePipeline(
		ComputePipelineDesc{ .computeShader = { .name = "UnwrapGeometryForAs",
												.source = shaderCode,
												.entryPoint = "unwrap_geometry_positions_for_acceleration_structure" },
							 .pipelineLayout = PipelineLayout{ .layout = pipelineLayout.layout } });

	unwrapIndiciesComputePipeline = context.CreateComputePipeline(
		ComputePipelineDesc{ .computeShader = { .name = "UnwrapGeometryForAs",
												.source = shaderCode,
												.entryPoint = "unwrap_geometry_indicies_for_acceleration_structure" },
							 .pipelineLayout = PipelineLayout{ .layout = pipelineLayout.layout } });


	{
		const auto poolSizes =
			std::array{ VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1 } };

		const auto descriptorPoolCreateInfo =
			VkDescriptorPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
										.pNext = nullptr,
										.flags = 0,
										.maxSets = 1,
										.poolSizeCount = static_cast<U32>(poolSizes.size()),
										.pPoolSizes = poolSizes.data() };
		const auto result = vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, nullptr,
												   &unwrappedGeometryDescriptorPool);
		assert(result == VK_SUCCESS);
	}
	{
		const auto allocationInfo =
			VkDescriptorSetAllocateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
										 .pNext = nullptr,
										 .descriptorPool = unwrappedGeometryDescriptorPool,
										 .descriptorSetCount = 1,
										 .pSetLayouts = &bindGroupLayout.layout };
		const auto result = vkAllocateDescriptorSets(context.device, &allocationInfo, &unwrappedGeometryDescriptorSet);
		assert(result == VK_SUCCESS);
		context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)unwrappedGeometryDescriptorSet,
								   "unwrappedGeometryDS");
	}


	{

		const auto unwrappedGeometryBufferInfo =
			VkDescriptorBufferInfo{ .buffer = unwrappedGeometryBuffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE };
		const auto dsWrites = std::array{ VkWriteDescriptorSet{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr,
			.dstSet = unwrappedGeometryDescriptorSet,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.pImageInfo = nullptr,
			.pBufferInfo = &unwrappedGeometryBufferInfo,
			.pTexelBufferView = nullptr,
		} };
		vkUpdateDescriptorSets(context.device, dsWrites.size(), dsWrites.data(), 0, nullptr);
	}

	descriptorSets = std::array{ scene.geometryDescriptorSet, unwrappedGeometryDescriptorSet };

	const auto geometryBufferInfo = VkBufferDeviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
															   .pNext = nullptr,
															   .buffer = unwrappedGeometryBuffer.buffer };

	unwrappedGeometryData = vkGetBufferDeviceAddress(context.device, &geometryBufferInfo);
}

void UnwrapGeometryForAccelerationStructurePass::ReleaseResources(const VulkanContext& context)
{
	context.DestroyComputePipeline(unwrapIndiciesComputePipeline);
	context.DestroyComputePipeline(unwrapPositionComputePipeline);
	context.DestroyPipelineLayout(pipelineLayout);
	context.DestroyBindGroupLayout(bindGroupLayout);

	context.DestroyBuffer(unwrappedGeometryBuffer);

	vkDestroyDescriptorPool(context.device, unwrappedGeometryDescriptorPool, nullptr);
}