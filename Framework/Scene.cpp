#include "Scene.hpp"
#include "MeshImporter.hpp"

#include <queue>
#include <stack>

using namespace Framework;
using namespace Framework::Graphics;

void Scene::CreateResources(const VulkanContext& context)
{
	geometryBindGroupLayout = context.CreateBindGroupLayout(
		BindGroupLayoutDesc{ .bindings = { StorageBufferBinding{}, StorageBufferBinding{}, StorageBufferBinding{} },
							 .debugName = "Uniform Geometry Buffer Descriptor Set Layout" });

	{
		const auto poolSizes =
			std::array{ VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3 } };

		const auto descriptorPoolCreateInfo =
			VkDescriptorPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
										.pNext = nullptr,
										.flags = 0,
										.maxSets = 1,
										.poolSizeCount = static_cast<U32>(poolSizes.size()),
										.pPoolSizes = poolSizes.data() };
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
										 .pSetLayouts = &geometryBindGroupLayout.layout };
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
							   MemoryUsage::gpu, "Unified Geometry Buffer" });


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

		const auto subMeshesBufferInfo =
			VkDescriptorBufferInfo{ .buffer = subMeshesBuffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE };
		const auto dynamicUniformBufferInfo = VkDescriptorBufferInfo{
			.buffer = context.dynamicUniformAllocator.buffer.buffer, .offset = 0, .range = VK_WHOLE_SIZE
		};
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
											  .pBufferInfo = &subMeshesBufferInfo,
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
											  .pBufferInfo = &dynamicUniformBufferInfo,
											  .pTexelBufferView = nullptr,
										  } };
		vkUpdateDescriptorSets(context.device, dsWrites.size(), dsWrites.data(), 0, nullptr);
	}
}

void Scene::ReleaseResources(const VulkanContext& context)
{
	vkDestroyFence(context.device, stagingBufferReuse, nullptr);


	context.DestroyBuffer(geometryBuffer);
	context.DestroyBuffer(stagingBuffer);
	context.DestroyBuffer(subMeshesBuffer);
	context.DestroyBindGroupLayout(geometryBindGroupLayout);

	vkDestroyDescriptorPool(context.device, geometryDescriptorPool, nullptr);

	vkDestroyCommandPool(context.device, commandPool, nullptr);
}

void Scene::Upload(const std::string_view mesh, const VulkanContext& context)
{
	auto importer = Framework::AssetImporter(std::filesystem::path{ mesh });

	const auto importSettings =
		Framework::MeshImportSettings{ .verticesStreamDeclarations = { Framework::VerticesStreamDeclaration{
										   .hasPosition = true, .hasNormal = true, .hasTextureCoordinate0 = true,
										   /*.hasJointsIndexAndWeights = true*/ } } };

	const auto& info = importer.GetSceneInformation();

	auto geometryByteOffset = 0u;

	const auto hasSkeletonAnimations = false;
	if (hasSkeletonAnimations)
	{

		const auto skeleton = importer.ImportSkeleton(0);
		skeletons.push_back(skeleton);
		const auto animations = importer.LoadAllAnimations(skeleton, 60);

		animationDataSet.animationDatabase.insert(animationDataSet.animationDatabase.end(),
												  animations.animationDatabase.begin(),
												  animations.animationDatabase.end());

		animationDataSet.animations.insert(animationDataSet.animations.end(), animations.animations.begin(),
										   animations.animations.end());
	}

	struct DataUploadRegion
	{
		uint64_t memoryPtr;
		uint32_t offset;
		uint32_t size;
	};

	std::queue<DataUploadRegion> uploadRequests;
	auto requestMeshUpload = [&](const MeshData& mesh)
	{
		auto buckets = (mesh.indexStream.size() + stagingBufferSize) / stagingBufferSize;
		for (auto i = 0; i < buckets; i++)
		{
			const auto uploadSize = i < buckets - 1 ? stagingBufferSize : mesh.indexStream.size() % stagingBufferSize;
			uploadRequests.push(DataUploadRegion{ (uint64_t)mesh.indexStream.data(), (uint32_t)(i * stagingBufferSize),
												  (uint32_t)uploadSize });
		}

		buckets = (mesh.streams[0].data.size() + stagingBufferSize) / stagingBufferSize;
		for (auto i = 0; i < buckets; i++)
		{
			const auto uploadSize =
				i < buckets - 1 ? stagingBufferSize : mesh.streams[0].data.size() % stagingBufferSize;
			uploadRequests.push(DataUploadRegion{ (uint64_t)mesh.streams[0].data.data(),
												  (uint32_t)(i * stagingBufferSize), (uint32_t)uploadSize });
		}
	};


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

		this->meshes.push_back(IndexedStaticMesh{
			.indicesOffset = (geometryBufferFreeOffset + geometryByteOffset) / 4,
			.indicesCount = static_cast<uint32_t>(meshData.indexStream.size() / 4),
			.verticesOffset =
				(geometryBufferFreeOffset + geometryByteOffset + static_cast<uint32_t>(meshData.indexStream.size())) /
				4,
			.verticesCount = static_cast<uint32_t>(meshData.streams[0].data.size() / vertexSize),
		});
		// geometryByteOffset += static_cast<uint32_t>(meshData.indexStream.size());
		// geometryByteOffset += static_cast<uint32_t>(meshData.streams[0].data.size());

		this->modelMatrices.push_back(importer.getModelMatrix(i));

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


			std::memcpy(stagingBuffer.mappedPtr, (char*)request.memoryPtr + request.offset, request.size);
			vmaFlushAllocation(context.allocator, geometryBuffer.allocation, 0, stagingBufferSize);
			const auto region = VkBufferCopy2{ .sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
											   .pNext = nullptr,
											   .srcOffset = 0,
											   .dstOffset = geometryBufferFreeOffset,
											   .size = request.size };

			const auto copyBufferInfo = VkCopyBufferInfo2{ .sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
														   .pNext = nullptr,
														   .srcBuffer = stagingBuffer.buffer,
														   .dstBuffer = geometryBuffer.buffer,
														   .regionCount = 1,
														   .pRegions = &region };

			{
				const auto result = vkResetCommandPool(context.device, commandPool, 0);
				assert(result == VK_SUCCESS);
			}

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

			geometryBufferFreeOffset += request.size;
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
	};

	auto subMeshes = std::vector<SubMesh>{};

	for (auto i = 0; i < meshes.size(); i++)
	{
		subMeshes.push_back({ meshes[i].indicesOffset, meshes[i].verticesOffset });
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
		const auto result = vkResetCommandPool(context.device, commandPool, 0);
		assert(result == VK_SUCCESS);
	}
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

void Framework::Scene::AddModel(const IndexedStaticMesh& mesh, const Math::Matrix4x4& transform)
{
	this->meshes.push_back(mesh);
	this->modelMatrices.push_back(transform);
}


void ManageUGB()
{
	constexpr auto totalLookupTableEntries = 1024;
	struct LookupTableEntry
	{
		U32 pageIndex;
		U32 physicalPageIndex;
	};

	struct LookupTable
	{
		std::array<LookupTableEntry, totalLookupTableEntries> entries;
	};


	struct Page
	{
		U32 bufferOffset;
	};


	std::stack<U32> freePages;

	/*
		GeometryData{ virtualAddress, size}



		const auto pages = UGB.allocatePages(geometryAsset.geometryData);

		TaskSystem::queueIoTask( [&, pages, geometryAsset???]()
			{
				loadAssetData(geometryAsset);
				uploadDataToUGB(geometryAsset, pages);

				UGB.notifyAssetLoaded(geometryAsset, pages);
			});



		UGB.updatePendingPages(); // iterates over all pending pages and check if they are loaded
		UGB.enqueueLookupTableUpdate(cmd); // generates lookup table fix dispatch



	*/
}