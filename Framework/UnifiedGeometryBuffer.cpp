#include "UnifiedGeometryBuffer.hpp"

#include <fstream>

using namespace Framework;
using namespace Framework::Graphics;

namespace
{
	struct PageRange
	{
		UnifiedGeometryBuffer::PageIndex firstPageIndex;
		UnifiedGeometryBuffer::PageIndex lastPageIndex;
	};

	PageRange GetVirtualSubMeshPageRange(const UnifiedGeometryBuffer::VirtualSubMesh& vsm)
	{
		const auto firstPageIndex = vsm.byteOffset / UnifiedGeometryBuffer::pageSizeInBytes;
		const auto lastPageIndex = (vsm.byteOffset + vsm.byteSize) / UnifiedGeometryBuffer::pageSizeInBytes;
		assert(firstPageIndex < UnifiedGeometryBuffer::totalPages);
		assert(lastPageIndex < UnifiedGeometryBuffer::totalPages);
		return { firstPageIndex, lastPageIndex };
	}
} // namespace

void Framework::UnifiedGeometryBuffer::RequestSubMesh(const VirtualSubMesh& subMesh)
{
	std::lock_guard lock(requestLock);
	const auto pageRange = GetVirtualSubMeshPageRange(subMesh);
	for (auto i = pageRange.firstPageIndex; i <= pageRange.lastPageIndex; i++)
	{
		auto& pageInfo = pages[i];
		if (not pageInfo.isLoaded && not pageInfo.loadingIsPending)
		{
			streamingSystem_->Request({ [&]()
										{
											scene_->Upload("Assets/Meshes/after_the_rain..._-_vr__sound/scene.gltf",
														   *vulkanContext_);

											finishedUploads.push({});
										} });
		}
	}
}

void UnifiedGeometryBuffer::CreateResources(Graphics::VulkanContext& context, Scene& scene,
											StreamingSystem& streamingSystem)
{
	scene_ = &scene;
	streamingSystem_ = &streamingSystem;
	vulkanContext_ = &context;
	geometryLookupTableBindGroupLayout = context.CreateBindGroupLayout(
		{ { StorageBufferBinding{} }, "Uniform Geometry Buffer Lookup Table Descriptor Set Layout" });

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
												   &geometryLookupTableDescriptorPool);
		assert(result == VK_SUCCESS);
	}
	{
		const auto allocationInfo =
			VkDescriptorSetAllocateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
										 .pNext = nullptr,
										 .descriptorPool = geometryLookupTableDescriptorPool,
										 .descriptorSetCount = 1,
										 .pSetLayouts = &geometryLookupTableBindGroupLayout.layout };
		const auto result =
			vkAllocateDescriptorSets(context.device, &allocationInfo, &geometryLookupTableDescriptorSet);
		assert(result == VK_SUCCESS);
		context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)geometryLookupTableDescriptorSet,
								   "geometryLookupTableDS");
	}


	geometryLookupTableBuffer = context.CreateBuffer(
		{ 32 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryUsage::gpu, "Global Geometry Lookup Table Buffer" });

	const auto geometryLookupTableBufferInfo =
		VkDescriptorBufferInfo{ .buffer = geometryLookupTableBuffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE };

	lookupTableUpdatePipelineLayout = context.CreatePipelineLayout(PipelineLayoutDesc{
		.bindGroupLayouts = { scene.geometryBindGroupLayout, geometryLookupTableBindGroupLayout },
		.pushConstantRanges = { VkPushConstantRange{
			.stageFlags = VK_SHADER_STAGE_ALL, .offset = 0, .size = sizeof(WorkgroupItemArguments) } } });

	const auto shaderCode = context.LoadShaderFileAsText("Assets/Shaders/UnifiedGeometryBufferLookupTableUpdate.comp");
	lookupTableUpdatePipeline = context.CreateComputePipeline(
		ComputePipelineDesc{ .computeShader = { .name = "UnifiedGeometryBuffer",
												.source = shaderCode,
												.entryPoint = "unifiedGeometryBuffer_VirtualLookupTableUpdate" },
							 .pipelineLayout = PipelineLayout{ .layout = lookupTableUpdatePipelineLayout.layout } });
}

void UnifiedGeometryBuffer::ReleaseResources(const VulkanContext& context)
{
	context.DestroyBuffer(geometryLookupTableBuffer);
	context.DestroyBindGroupLayout(geometryLookupTableBindGroupLayout);
	context.DestroyPipelineLayout(lookupTableUpdatePipelineLayout);
	context.DestroyComputePipeline(lookupTableUpdatePipeline);
}

void UnifiedGeometryBuffer::UpdateUnifiedGeometryBufferLookupTable(VkCommandBuffer cmd, const Scene& scene)
{

	const auto ds = std::array{ scene.geometryDescriptorSet, geometryLookupTableDescriptorSet };
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lookupTableUpdatePipeline.pipeline);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, lookupTableUpdatePipelineLayout.layout, 0, ds.size(),
							ds.data(), 0, nullptr);
	const auto args = WorkgroupItemArguments{ 0, 2 };
	vkCmdPushConstants(cmd, lookupTableUpdatePipelineLayout.layout, VK_SHADER_STAGE_ALL, 0,
					   sizeof(WorkgroupItemArguments), &args);
	vkCmdDispatch(cmd, 1, 1, 1);


	const auto bufferMemoryBarrier = VkBufferMemoryBarrier2{
		.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
		.pNext = nullptr,
		.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
		.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
		.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
		.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.buffer = geometryLookupTableBuffer.buffer,
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

Framework::FileDataSource::FileDataSource(const std::filesystem::path& file) : filePath{ file }
{
}

const std::span<std::byte> Framework::FileDataSource::OnGetData() const
{
	auto stream = std::ifstream{ filePath, std::ios::ate | std::ios::binary };
	const auto size = stream.tellg();
	auto data = std::vector<std::byte>{};
	data.resize(size);
	stream.seekg(0);
	stream.read((char*)data.data(), size);

	return data;
}
