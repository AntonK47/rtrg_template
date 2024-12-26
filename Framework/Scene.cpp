#include "Scene.hpp"
#include "MeshImporter.hpp"

#include <queue>

using namespace Framework;
using namespace Framework::Graphics;

void Scene::CreateResources(const VulkanContext& context)
{
	{
		const auto bindings =
			std::array{ VkDescriptorSetLayoutBinding{ .binding = 0,
													  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													  .descriptorCount = 1,
													  .stageFlags = VK_SHADER_STAGE_ALL,
													  .pImmutableSamplers = nullptr },
						VkDescriptorSetLayoutBinding{ .binding = 1,
													  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													  .descriptorCount = 1,
													  .stageFlags = VK_SHADER_STAGE_ALL,
													  .pImmutableSamplers = nullptr },
						VkDescriptorSetLayoutBinding{ .binding = 2,
													  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
													  .descriptorCount = 1,
													  .stageFlags = VK_SHADER_STAGE_ALL,
													  .pImmutableSamplers = nullptr } };

		const auto descriptorSetLayoutCreateInfo =
			VkDescriptorSetLayoutCreateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
											 .pNext = nullptr,
											 .flags = 0, // VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
											 .bindingCount = bindings.size(),
											 .pBindings = bindings.data() };

		const auto result = vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCreateInfo, nullptr,
														&geometryDescriptorSetLayout);
		assert(result == VK_SUCCESS);
		context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)geometryDescriptorSetLayout,
								   "geometryDSLayout");
	}

	{
		const auto poolSize = VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3 };

		const auto descriptorPoolCreateInfo =
			VkDescriptorPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
										.pNext = nullptr,
										.flags = 0,
										.maxSets = 1,
										.poolSizeCount = 1,
										.pPoolSizes = &poolSize };
		const auto result =
			vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, nullptr, &geometryDescriptorPool);
		assert(result == VK_SUCCESS);
	}
	{
		const auto allocationInfo =
			VkDescriptorSetAllocateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
										 .pNext = nullptr,
										 .descriptorPool = geometryDescriptorPool,
										 .descriptorSetCount = 1,
										 .pSetLayouts = &geometryDescriptorSetLayout };
		const auto result = vkAllocateDescriptorSets(context.device, &allocationInfo, &geometryDescriptorSet);
		assert(result == VK_SUCCESS);
		context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET, (uint64_t)geometryDescriptorSet, "geometryDS");
	}
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

	geometryBuffer =
		context.CreateBuffer({ 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   MemoryUsage::gpu, "Global Vertex Buffer" });
	geometryIndexBuffer =
		context.CreateBuffer({ 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
							   MemoryUsage::gpu, "Global Index Buffer" });
	stagingBuffer = context.CreateBuffer(
		{ stagingBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryUsage::upload, "Geometry Staging Buffer" });
	subMeshesBuffer = context.CreateBuffer({ sizeof(U32) * 2 * 1024,
											 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
											 MemoryUsage::gpu, "SubMeshes Buffer" });


	{
		const auto fenceCreateInfo = VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
														.pNext = nullptr,
														.flags = VK_FENCE_CREATE_SIGNALED_BIT };
		const auto result = vkCreateFence(context.device, &fenceCreateInfo, nullptr, &stagingBufferReuse);
		assert(result == VK_SUCCESS);
		context.SetObjectDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)stagingBufferReuse, "Staging Buffer Reuse Fence");
	}

	{
		const auto geometryBufferInfo =
			VkDescriptorBufferInfo{ .buffer = geometryBuffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE };

		const auto geometryIndexBufferInfo =
			VkDescriptorBufferInfo{ .buffer = geometryIndexBuffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE };
		const auto subMeshesBufferInfo =
			VkDescriptorBufferInfo{ .buffer = subMeshesBuffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE };
		const auto dsWrites = std::array{ VkWriteDescriptorSet{
											  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											  .pNext = nullptr,
											  .dstSet = geometryDescriptorSet,
											  .dstBinding = 0,
											  .dstArrayElement = 0,
											  .descriptorCount = 1,
											  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											  .pImageInfo = nullptr,
											  .pBufferInfo = &geometryBufferInfo,
											  .pTexelBufferView = nullptr,
										  },
										  VkWriteDescriptorSet{
											  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											  .pNext = nullptr,
											  .dstSet = geometryDescriptorSet,
											  .dstBinding = 1,
											  .dstArrayElement = 0,
											  .descriptorCount = 1,
											  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											  .pImageInfo = nullptr,
											  .pBufferInfo = &geometryIndexBufferInfo,
											  .pTexelBufferView = nullptr,
										  },
										  VkWriteDescriptorSet{
											  .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
											  .pNext = nullptr,
											  .dstSet = geometryDescriptorSet,
											  .dstBinding = 2,
											  .dstArrayElement = 0,
											  .descriptorCount = 1,
											  .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
											  .pImageInfo = nullptr,
											  .pBufferInfo = &subMeshesBufferInfo,
											  .pTexelBufferView = nullptr,
										  } };
		vkUpdateDescriptorSets(context.device, dsWrites.size(), dsWrites.data(), 0, nullptr);
	}
}

void Scene::ReleaseResources(const VulkanContext& context)
{
	vkDestroyFence(context.device, stagingBufferReuse, nullptr);

	context.DestroyBuffer(geometryBuffer);
	context.DestroyBuffer(geometryIndexBuffer);
	context.DestroyBuffer(stagingBuffer);
	context.DestroyBuffer(subMeshesBuffer);

	vkDestroyDescriptorSetLayout(context.device, geometryDescriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(context.device, geometryDescriptorPool, nullptr);

	vkDestroyCommandPool(context.device, commandPool, nullptr);
}

void Scene::Upload(const std::string_view mesh, const VulkanContext& context)
{

	auto importer = Framework::AssetImporter(std::filesystem::path{ mesh });

	const auto importSettings = Framework::MeshImportSettings{
		.verticesStreamDeclarations = { Framework::VerticesStreamDeclaration{
			.hasPosition = true, .hasNormal = true, .hasTextureCoordinate0 = true, .hasJointsIndexAndWeights = true } }
	};

	const auto& info = importer.GetSceneInformation();

	auto indexOffset = 0u;
	auto vertexOffset = 0u;
	const auto skeleton = importer.ImportSkeleton(0);
	skeletons.push_back(skeleton);
	const auto animations = importer.LoadAllAnimations(skeleton, 60);

	animationDataSet.animationDatabase.insert(animationDataSet.animationDatabase.end(),
											  animations.animationDatabase.begin(), animations.animationDatabase.end());

	animationDataSet.animations.insert(animationDataSet.animations.end(), animations.animations.begin(),
									   animations.animations.end());

	struct DataUploadRegion
	{
		uint64_t memoryPtr;
		uint32_t offset;
		uint32_t size;
		bool isIndexData{ false };
	};

	std::queue<DataUploadRegion> uploadRequests;
	auto requestMeshUpload = [&](const MeshData& mesh)
	{
		auto buckets = (mesh.indexStream.size() + stagingBufferSize) / stagingBufferSize;
		for (auto i = 0; i < buckets; i++)
		{
			const auto uploadSize = i < buckets - 1 ? stagingBufferSize : mesh.indexStream.size() % stagingBufferSize;
			uploadRequests.push(DataUploadRegion{ (uint64_t)mesh.indexStream.data(), (uint32_t)(i * stagingBufferSize),
												  (uint32_t)uploadSize, true });
		}

		buckets = (mesh.streams[0].data.size() + stagingBufferSize) / stagingBufferSize;
		for (auto i = 0; i < buckets; i++)
		{
			const auto uploadSize =
				i < buckets - 1 ? stagingBufferSize : mesh.streams[0].data.size() % stagingBufferSize;
			uploadRequests.push(DataUploadRegion{ (uint64_t)mesh.streams[0].data.data(),
												  (uint32_t)(i * stagingBufferSize), (uint32_t)uploadSize, false });
		}
	};

	const auto indexByteOffset = geometryIndexBufferFreeOffset;
	const auto vertexByteOffset = geometryBufferFreeOffset;

	for (auto i = 0; i < info.meshCount; i++)
	{
		const auto meshData = importer.ImportMesh(i, importSettings);
		requestMeshUpload(meshData);

		auto vertexSize = 0u;
		for (auto j = 0; j < meshData.streams[0].streamDescriptor.attributes.size(); j++)
		{
			vertexSize += meshData.streams[0].streamDescriptor.attributes[j].componentCount *
				meshData.streams[0].streamDescriptor.attributes[j].componentSize;
		}

		const auto baseIndex = indexByteOffset / 4;
		const auto baseVertex = vertexByteOffset / vertexSize;

		this->meshes.push_back(
			IndexedStaticMesh{ .indicesOffset = baseIndex + indexOffset,
							   .indicesCount = static_cast<uint32_t>(meshData.indexStream.size() / 4),
							   .verticesOffset = baseVertex + vertexOffset,
							   .verticesCount = static_cast<uint32_t>(meshData.streams[0].data.size() / vertexSize),
							   .stride = vertexSize });
		indexOffset += static_cast<uint32_t>(meshData.indexStream.size() / 4);
		vertexOffset += static_cast<uint32_t>(meshData.streams[0].data.size() / vertexSize);


		while (!uploadRequests.empty())
		{
			{
				const auto result = vkWaitForFences(context.device, 1, &stagingBufferReuse, VK_TRUE, ~0ull);
				assert(result == VK_SUCCESS);
			}
			{
				const auto result = vkResetFences(context.device, 1, &stagingBufferReuse);
				assert(result == VK_SUCCESS);
			}

			const auto request = uploadRequests.front();
			uploadRequests.pop();


			const auto isIndexData = request.isIndexData;

			std::memcpy(stagingBuffer.mappedPtr, (char*)request.memoryPtr + request.offset, request.size);
			vmaFlushAllocation(context.allocator,
							   isIndexData ? geometryIndexBuffer.allocation : geometryBuffer.allocation, 0,
							   stagingBufferSize);
			const auto region =
				VkBufferCopy2{ .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
							   .pNext = nullptr,
							   .srcOffset = 0,
							   .dstOffset = isIndexData ? geometryIndexBufferFreeOffset : geometryBufferFreeOffset,
							   .size = request.size };

			const auto copyBufferInfo =
				VkCopyBufferInfo2{ .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
								   .pNext = nullptr,
								   .srcBuffer = stagingBuffer.buffer,
								   .dstBuffer = isIndexData ? geometryIndexBuffer.buffer : geometryBuffer.buffer,
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
			const auto result = vkQueueSubmit2(context.transferQueue, 1, &submit, stagingBufferReuse);
			assert(result == VK_SUCCESS);

			if (isIndexData)
			{
				geometryIndexBufferFreeOffset += request.size;
			}
			else
			{
				geometryBufferFreeOffset += request.size;
			}
		}
	}

	{
		const auto result = vkWaitForFences(context.device, 1, &stagingBufferReuse, VK_TRUE, ~0ull);
		assert(result == VK_SUCCESS);
	}
	{
		const auto result = vkResetFences(context.device, 1, &stagingBufferReuse);
		assert(result == VK_SUCCESS);
	}
	struct SubMesh
	{
		U32 indexBase;
		U32 vertexBase;
		U32 vertexStride;
	};


	auto subMeshes = std::vector<SubMesh>{};

	for (auto i = 0; i < meshes.size(); i++)
	{
		subMeshes.push_back({ meshes[i].indicesOffset, meshes[i].verticesOffset, meshes[i].stride });
	}

	std::memcpy(stagingBuffer.mappedPtr, (char*)subMeshes.data(), subMeshes.size() * sizeof(SubMesh));
	vmaFlushAllocation(context.allocator, subMeshesBuffer.allocation, 0, stagingBufferSize);
	const auto region = VkBufferCopy2{ .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
									   .pNext = nullptr,
									   .srcOffset = 0,
									   .dstOffset = 0,
									   .size = subMeshes.size() * sizeof(SubMesh) };

	const auto copyBufferInfo = VkCopyBufferInfo2{ .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
												   .pNext = nullptr,
												   .srcBuffer = stagingBuffer.buffer,
												   .dstBuffer = subMeshesBuffer.buffer,
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
	const auto result = vkQueueSubmit2(context.transferQueue, 1, &submit, stagingBufferReuse);
	assert(result == VK_SUCCESS);
}