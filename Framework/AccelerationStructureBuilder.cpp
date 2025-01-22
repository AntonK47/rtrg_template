#include "AccelerationStructureBuilder.hpp"

#include <numeric>

using namespace Framework;
using namespace Framework::Graphics;

namespace
{
	template <int bytes>
	constexpr U32 RoundUpToMultipleOf(U32 value)
	{
		return (value + bytes - 1) & ~(bytes - 1);
	}

	constexpr U32 RoundUpToMultipleOf256(U32 value)
	{
		return RoundUpToMultipleOf<256>(value);
	}
} // namespace

void AccelerationStructureBuilder::ResizeScratchBuffer(const U32 newSize)
{
	vulkanContext->DestroyBuffer(scratchBuffer);
	scratchBufferSize = newSize;
	auto scratchBuffer = vulkanContext->CreateBuffer(BufferDesc{
		static_cast<U32>(newSize), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT });
	const auto scratchDataInfo = VkBufferDeviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
															.pNext = nullptr,
															.buffer = scratchBuffer.buffer };

	scratchData = vkGetBufferDeviceAddress(vulkanContext->device, &scratchDataInfo);
}

void AccelerationStructureBuilder::AddMeshAsNewBottomLevelAccelerationStructure(const IndexedStaticMesh& mesh,
																				Math::Matrix4x4 transform)
{
	auto sizeInfo = VkAccelerationStructureBuildSizesInfoKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, .pNext = nullptr
	};

	const auto geometry = VkAccelerationStructureGeometryTrianglesDataKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
		.pNext = nullptr,
		.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
		.vertexData = unwrapPass->unwrappedGeometryData,
		.vertexStride = 3 * sizeof(F32), // vec3
		.maxVertex = 128 * 1024 * 1024 / (3 * 4) - 1,
		.indexType = VK_INDEX_TYPE_UINT32,
		.indexData = unwrapPass->unwrappedGeometryData,
		.transformData = { 0 }
	};

	const auto geometryInfo =
		VkAccelerationStructureGeometryKHR{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
											.pNext = nullptr,
											.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
											.geometry = geometry,
											.flags = VK_GEOMETRY_OPAQUE_BIT_KHR };

	{
		const auto asBuildInfo = VkAccelerationStructureBuildGeometryInfoKHR{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			.pNext = nullptr,
			.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			.flags = 0,
			.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
			.srcAccelerationStructure = VK_NULL_HANDLE,
			.dstAccelerationStructure = VK_NULL_HANDLE,
			.geometryCount = 1,
			.pGeometries = &geometryInfo,
			.ppGeometries = nullptr,
			.scratchData = { 0 }
		};

		auto maxPrimitiveCount = U32{ mesh.indicesCount / 3 };

		vkGetAccelerationStructureBuildSizesKHR(vulkanContext->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
												&asBuildInfo, &maxPrimitiveCount, &sizeInfo);
	}

	asBuildInfos.push_back(TriangleBottomLevelAccelerationStrcutureBuildInfo{ sizeInfo, &mesh });
	transforms.push_back(transform);
}

void AccelerationStructureBuilder::BuildBottomLevelAccelerationStructures(const VkCommandBuffer& cmd)
{
	auto requiredScratchSize = U32{};
	for (auto& blasBuildInfo : asBuildInfos)
	{
		requiredScratchSize =
			std::max({ requiredScratchSize, static_cast<U32>(blasBuildInfo.sizeInfo.buildScratchSize) });
	}

	// NITICE:  VUID-VkAccelerationStructureCreateInfoKHR-offset-03734 requirenment must be hold
	const auto requiredTotalBufferSize =
		std::accumulate(asBuildInfos.begin(), asBuildInfos.end(), U32{ 0 },
						[](U32 a, const TriangleBottomLevelAccelerationStrcutureBuildInfo& b)
						{ return a + RoundUpToMultipleOf256(static_cast<U32>(b.sizeInfo.accelerationStructureSize)); });

	if (static_cast<U32>(requiredScratchSize) > scratchBufferSize)
	{
		ResizeScratchBuffer(requiredScratchSize);
	}

	bottomLevelAsBuffer = vulkanContext->CreateBuffer(BufferDesc{ static_cast<U32>(requiredTotalBufferSize),
														 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
															 VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
															 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
														 MemoryUsage::gpu, "Tlas Buffer" });

	auto tlasBufferOffset = U32{ 0 };

	for (auto& blasBuildInfo : asBuildInfos)
	{
		TriangleBottomLevelAccelerationStructure blas{};
		{
			const auto asCreateInfo = VkAccelerationStructureCreateInfoKHR{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
				.pNext = nullptr,
				.buffer = bottomLevelAsBuffer.buffer,
				.offset = VkDeviceSize{ tlasBufferOffset },
				.size = VkDeviceSize{ blasBuildInfo.sizeInfo.accelerationStructureSize },
				.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
				.deviceAddress = 0
			};

			blas.bufferSize = blasBuildInfo.sizeInfo.accelerationStructureSize;
			blas.bufferOffset = tlasBufferOffset;
			tlasBufferOffset += RoundUpToMultipleOf256(blasBuildInfo.sizeInfo.accelerationStructureSize);

			const auto result = vkCreateAccelerationStructureKHR(vulkanContext->device, &asCreateInfo, nullptr,
																 &blas.accelerationStructure);
			assert(result == VK_SUCCESS);


			const auto asDeviceAddressInfo = VkAccelerationStructureDeviceAddressInfoKHR{
				.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
				.pNext = nullptr,
				.accelerationStructure = blas.accelerationStructure
			};

			blas.deviceAddress =
				vkGetAccelerationStructureDeviceAddressKHR(vulkanContext->device, &asDeviceAddressInfo);
		}

		const auto geometry = VkAccelerationStructureGeometryTrianglesDataKHR{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
			.pNext = nullptr,
			.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
			.vertexData = unwrapPass->unwrappedGeometryData,
			.vertexStride = 3 * sizeof(F32), // vec3
			.maxVertex = 128 * 1024 * 1024 / (3 * 4) - 1,
			.indexType = VK_INDEX_TYPE_UINT32,
			.indexData = unwrapPass->unwrappedGeometryData,
			.transformData = { 0 }
		};

		const auto geometryInfo =
			VkAccelerationStructureGeometryKHR{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
												.pNext = nullptr,
												.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
												.geometry = geometry,
												.flags = VK_GEOMETRY_OPAQUE_BIT_KHR };

		const auto asBuildInfo = VkAccelerationStructureBuildGeometryInfoKHR{
			.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
			.pNext = nullptr,
			.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			.flags = 0,
			.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
			.srcAccelerationStructure = VK_NULL_HANDLE,
			.dstAccelerationStructure = blas.accelerationStructure,
			.geometryCount = 1,
			.pGeometries = &geometryInfo,
			.ppGeometries = nullptr,
			.scratchData = scratchData
		};

		auto& mesh = *blasBuildInfo.mesh;
		const auto rangeInfo = VkAccelerationStructureBuildRangeInfoKHR{ .primitiveCount = mesh.indicesCount / 3,
																		 .primitiveOffset = static_cast<U32>(
																			 mesh.verticesCount * 3 * sizeof(U32)),
																		 .firstVertex = 0,
																		 .transformOffset = 0 };
		auto pRangeInfo = &rangeInfo;


		unwrapPass->UnwrapMesh(cmd, *blasBuildInfo.mesh);
		vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildInfo, &pRangeInfo);

		{
			const auto bufferMemoryBarrier = VkBufferMemoryBarrier2{
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
				.pNext = nullptr,
				.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
				.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
				.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
				.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
				.buffer = unwrapPass->unwrappedGeometryBuffer.buffer,
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
		accelerationStructures.push_back(blas);
	}
}

void AccelerationStructureBuilder::BuildTopLevelAccelerationStructure(const VkCommandBuffer& cmd)
{
	std::vector<VkAccelerationStructureInstanceKHR> instances;

	for (auto i = 0; i < accelerationStructures.size(); i++)
	{
		auto instance = VkAccelerationStructureInstanceKHR{};
		auto& instanceTransform = transforms[i];
		instance.transform.matrix[0][0] = instanceTransform[0][0];
		instance.transform.matrix[0][1] = instanceTransform[1][0];
		instance.transform.matrix[0][2] = instanceTransform[2][0];
		instance.transform.matrix[0][3] = instanceTransform[3][0];
		instance.transform.matrix[1][0] = instanceTransform[0][1];
		instance.transform.matrix[1][1] = instanceTransform[1][1];
		instance.transform.matrix[1][2] = instanceTransform[2][1];
		instance.transform.matrix[1][3] = instanceTransform[3][1];
		instance.transform.matrix[2][0] = instanceTransform[0][2];
		instance.transform.matrix[2][1] = instanceTransform[1][2];
		instance.transform.matrix[2][2] = instanceTransform[2][2];
		instance.transform.matrix[2][3] = instanceTransform[3][2];

		instance.instanceCustomIndex = i;
		instance.mask = 0xff;
		instance.instanceShaderBindingTableRecordOffset = 0;
		instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
		instance.accelerationStructureReference = accelerationStructures[i].deviceAddress;
		instances.push_back(instance);
	}

	instanceData = vulkanContext->CreateBuffer(
		BufferDesc{ (U32)(instances.size() * sizeof(VkAccelerationStructureInstanceKHR)),
					VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
						VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
					MemoryUsage::upload, "Blas Instances" });

	std::memcpy(instanceData.mappedPtr, instances.data(),
				instances.size() * sizeof(VkAccelerationStructureInstanceKHR));


	const auto instanceDataAddressInfo = VkBufferDeviceAddressInfo{
		.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .pNext = nullptr, .buffer = instanceData.buffer
	};

	auto instanceDataAddress = vkGetBufferDeviceAddress(vulkanContext->device, &instanceDataAddressInfo);

	auto geometryInstance = VkAccelerationStructureGeometryInstancesDataKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
		.pNext = nullptr,
		.arrayOfPointers = VK_FALSE,
		.data = instanceDataAddress
	};

	const auto geometryData = VkAccelerationStructureGeometryDataKHR{ .instances = geometryInstance };
	const auto geometryInfo =
		VkAccelerationStructureGeometryKHR{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
											.pNext = nullptr,
											.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
											.geometry = geometryData,
											.flags = VK_GEOMETRY_OPAQUE_BIT_KHR };

	auto sizeInfo = VkAccelerationStructureBuildSizesInfoKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR, .pNext = nullptr
	};

	auto asBuildInfo = VkAccelerationStructureBuildGeometryInfoKHR{
		.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
		.pNext = nullptr,
		.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
		.flags = 0,
		.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
		.srcAccelerationStructure = VK_NULL_HANDLE,
		.dstAccelerationStructure = VK_NULL_HANDLE,
		.geometryCount = 1,
		.pGeometries = &geometryInfo,
		.ppGeometries = nullptr,
		.scratchData = { 0 }
	};

	const auto rangeInfo = VkAccelerationStructureBuildRangeInfoKHR{
		.primitiveCount = (U32)instances.size(),
		.primitiveOffset = 0,
		.firstVertex = 0,
		.transformOffset = 0,
	};

	const auto pRangeInfo = &rangeInfo;

	vkGetAccelerationStructureBuildSizesKHR(vulkanContext->device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
											&asBuildInfo, &rangeInfo.primitiveCount, &sizeInfo);

	TopLevelAccelerationStructure tlas{};

	tlas.buffer = vulkanContext->CreateBuffer(BufferDesc{ .size = (U32)sizeInfo.accelerationStructureSize,
														  .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
															  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
															  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR,
														  .memoryUsage = MemoryUsage::gpu,
														  .debugName = "tlas buffer" });

	{
		const auto asCreateInfo =
			VkAccelerationStructureCreateInfoKHR{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
												  .pNext = nullptr,
												  .buffer = tlas.buffer.buffer,
												  .offset = 0,
												  .size = sizeInfo.accelerationStructureSize,
												  .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
												  .deviceAddress = { 0 } };
		const auto result = vkCreateAccelerationStructureKHR(vulkanContext->device, &asCreateInfo, nullptr, &tlas.as);
		assert(result == VK_SUCCESS);
		vulkanContext->SetObjectDebugName(VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR, (uint64_t)tlas.as, "TLAS");
	}

	if (scratchBufferSize < sizeInfo.buildScratchSize)
	{
		ResizeScratchBuffer(sizeInfo.buildScratchSize);
	}
	asBuildInfo.scratchData.deviceAddress = scratchData;
	asBuildInfo.dstAccelerationStructure = tlas.as;

	vkCmdBuildAccelerationStructuresKHR(cmd, 1, &asBuildInfo, &pRangeInfo);
	topLevelAs = tlas;
}

void AccelerationStructureBuilder::CreateResources(const VulkanContext& context,
												   UnwrapGeometryForAccelerationStructurePass& pass)
{
	vulkanContext = &context;
	unwrapPass = &pass;
	scratchBufferSize = initialScratchBufferSize;
	scratchBuffer = vulkanContext->CreateBuffer(
		BufferDesc{ static_cast<U32>(initialScratchBufferSize),
					VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT });
	const auto scratchDataInfo = VkBufferDeviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
															.pNext = nullptr,
															.buffer = scratchBuffer.buffer };

	scratchData = vkGetBufferDeviceAddress(vulkanContext->device, &scratchDataInfo);
}

void AccelerationStructureBuilder::ReleaseResources(const VulkanContext& context)
{
	for (auto& as : accelerationStructures)
	{
		vkDestroyAccelerationStructureKHR(context.device, as.accelerationStructure, nullptr);
	}

	vkDestroyAccelerationStructureKHR(context.device, topLevelAs.as, nullptr);

	context.DestroyBuffer(scratchBuffer);
	context.DestroyBuffer(topLevelAs.buffer);
	context.DestroyBuffer(bottomLevelAsBuffer);
	context.DestroyBuffer(instanceData);
}
