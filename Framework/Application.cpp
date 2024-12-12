#include "Application.hpp"

#include <array>
#include <cassert>
#include <filesystem>
#include <format>
#include <fstream>
#include <queue>
#include <regex>
#include <string_view>
#include <tuple>
#include <vector>

#include "Animation.hpp"
#include "ImGuiUtils.hpp"
#include "Math.hpp"
#include "MeshImporter.hpp"
#include "MiniAssetImporterEditor.hpp"
#include "SDL3Utils.hpp"
#include "VulkanRHI.hpp"

using namespace Framework;
using namespace Framework::Animation;
using namespace Framework::Graphics;

template <typename... Args>
std::string runtime_format(std::string_view rt_fmt_str, Args&&... args)
{
	return std::vformat(rt_fmt_str, std::make_format_args(args...));
}

const char* sample_surface_01 =
	"void surface(in Geometry geometry, out vec4 color){ color = vec4(1.0f,0.0f,0.0f,1.0f);}";
const char* sample_surface_02 =
	"void surface(in Geometry geometry, out vec4 color){ color = vec4(0.0f,1.0f,0.0f,1.0f);}";

struct MaterialAsset
{
	std::string surfaceShadingCode;
};


struct ShaderToyConstant
{
	float time;
	float resolution[2];
};

struct IndexedStaticMesh
{
	// has only one stream position
	uint32_t indicesOffset;
	uint32_t indicesCount;
	uint32_t verticesOffset;
	uint32_t verticesCount;
	uint32_t stride;
};

struct ConstantsData
{
	glm::mat4 viewProjection;
	glm::mat4 view;
	glm::mat4 model;
	glm::vec3 viewPositionWS;
};

struct Camera
{
	glm::vec3 position;
	glm::vec3 forward;
	glm::vec3 up;
	float movementSpeed{ 1.0f };
	float movementSpeedScale{ 1.0f };
	float sensitivity{ 1.0f };
};

using ViewportSize = glm::vec2;

std::tuple<glm::vec2, bool> GetScreenSpacePosition(const ViewportSize& viewport, const glm::mat4& modelView,
												   const glm::mat4& projection, const glm::vec3& positionWS)
{
	const auto positionViewSpace = modelView * glm::vec4{ positionWS, 1.0f };
	bool isVisible = true;
	if (positionViewSpace.z < 0.0f)
	{
		isVisible = false;
	}

	const auto positionClipSpace = projection * positionViewSpace;
	const auto positionNDC =
		glm::vec3{ positionClipSpace.x / positionClipSpace.w, positionClipSpace.y / positionClipSpace.w,
				   positionClipSpace.z / positionClipSpace.w };


	const auto screenPosition =
		glm::vec2{ (positionNDC.x + 1.0f) * 0.5f * viewport.x, (positionNDC.y + 1.0f) * 0.5f * viewport.y };

	return std::make_tuple(screenPosition, isVisible);
}

struct Scene
{
	void LoadScene();
	void ReleaseResources();
	void CreateResources(const VulkanContext& context)
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
												 .flags =
													 0, // VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
												 .bindingCount = bindings.size(),
												 .pBindings = bindings.data() };

			const auto result = vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCreateInfo, nullptr,
															&geometryDescriptorSetLayout);
			assert(result == VK_SUCCESS);
			context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)geometryDescriptorSetLayout,
									   "geometryDSLayout");
		}

		{
			const auto poolSize =
				VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3 };

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
			const auto poolCreateInfo =
				VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
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

		geometryBuffer = context.CreateBuffer({ 128 * 1024 * 1024,
												VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
												MemoryUsage::gpu, "Global Vertex Buffer" });
		geometryIndexBuffer = context.CreateBuffer(
			{ 128 * 1024 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
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
			context.SetObjectDebugName(VK_OBJECT_TYPE_FENCE, (uint64_t)stagingBufferReuse,
									   "Staging Buffer Reuse Fence");
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

	void ReleaseResources(const VulkanContext& context)
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

	void Upload(const std::string_view mesh, const VulkanContext& context)
	{

		auto importer = Framework::AssetImporter(std::filesystem::path{ mesh });

		const auto importSettings = Framework::MeshImportSettings{
			.verticesStreamDeclarations = { Framework::VerticesStreamDeclaration{ .hasPosition = true,
																				  .hasNormal = true,
																				  .hasTextureCoordinate0 = true,
																				  .hasJointsIndexAndWeights = true } }
		};

		const auto info = importer.GetSceneInformation();
		auto indexOffset = 0u;
		auto vertexOffset = 0u;
		const auto skeleton = importer.ImportSkeleton(0);
		skeletons.push_back(skeleton);
		animationDataSet = importer.LoadAllAnimations(skeleton, 60);

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
				const auto uploadSize =
					i < buckets - 1 ? stagingBufferSize : mesh.indexStream.size() % stagingBufferSize;
				uploadRequests.push(DataUploadRegion{ (uint64_t)mesh.indexStream.data(),
													  (uint32_t)(i * stagingBufferSize), (uint32_t)uploadSize, true });
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

			this->meshes.push_back(
				IndexedStaticMesh{ .indicesOffset = indexOffset,
								   .indicesCount = static_cast<uint32_t>(meshData.indexStream.size() / 4),
								   .verticesOffset = vertexOffset,
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
					const auto beginInfo =
						VkCommandBufferBeginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
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


	struct GpuSubMesh
	{
	};

	struct GpuScene
	{
		void UpdateInstances(Scene& scene)
		{
		}

		void AllocateInstances();
		void AllocateMeshs();

		GraphicsBuffer instances;
		GraphicsBuffer meshes;
		GraphicsBuffer lodMeshes;
		GraphicsBuffer subMeshes;
	};

	static constexpr VkDeviceSize stagingBufferSize{ 1 * 1024 * 1024 };
	GraphicsBuffer stagingBuffer{};
	VkFence stagingBufferReuse;

	VkDescriptorPool geometryDescriptorPool;
	VkDescriptorSet geometryDescriptorSet;
	VkDescriptorSetLayout geometryDescriptorSetLayout;


	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;

	GraphicsBuffer geometryBuffer{};
	uint32_t geometryBufferFreeOffset{ 0 };
	GraphicsBuffer geometryIndexBuffer{};
	uint32_t geometryIndexBufferFreeOffset{ 0 };

	GraphicsBuffer subMeshesBuffer{};

	std::vector<IndexedStaticMesh> meshes;
	std::vector<Skeleton> skeletons;
	AnimationDataSet animationDataSet;
};

struct FrameData
{

	void CreateResources(const VulkanContext& context, int frameInFlights = 2)
	{

		{
			const auto bindings =
				std::array{ VkDescriptorSetLayoutBinding{ .binding = 0,
														  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
														  .descriptorCount = 1,
														  .stageFlags = VK_SHADER_STAGE_ALL,
														  .pImmutableSamplers = nullptr } };

			const auto descriptorSetLayoutCreateInfo =
				VkDescriptorSetLayoutCreateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
												 .pNext = nullptr,
												 .flags =
													 0, // VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
														// //VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT,
												 .bindingCount = bindings.size(),
												 .pBindings = bindings.data() };

			const auto result = vkCreateDescriptorSetLayout(context.device, &descriptorSetLayoutCreateInfo, nullptr,
															&frameDescriptorSetLayout);
			assert(result == VK_SUCCESS);
			context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)frameDescriptorSetLayout,
									   "Uniform DS Layout");
		}

		perFrameResources.resize(frameInFlights);
		for (auto i = 0; i < perFrameResources.size(); i++)
		{
			{
				const auto poolSize =
					VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1 };

				const auto descriptorPoolCreateInfo =
					VkDescriptorPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
												.pNext = nullptr,
												.flags = 0,
												.maxSets = 1,
												.poolSizeCount = 1,
												.pPoolSizes = &poolSize };
				const auto result = vkCreateDescriptorPool(context.device, &descriptorPoolCreateInfo, nullptr,
														   &perFrameResources[i].frameDescriptorPool);
				assert(result == VK_SUCCESS);
			}
			{
				const auto allocationInfo =
					VkDescriptorSetAllocateInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
												 .pNext = nullptr,
												 .descriptorPool = perFrameResources[i].frameDescriptorPool,
												 .descriptorSetCount = 1,
												 .pSetLayouts = &frameDescriptorSetLayout };
				const auto result = vkAllocateDescriptorSets(context.device, &allocationInfo,
															 &perFrameResources[i].jointsMatriciesDescriptorSet);
				assert(result == VK_SUCCESS);
				context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET,
										   (uint64_t)perFrameResources[i].jointsMatriciesDescriptorSet,
										   "Joints Matrices");
			}
		}

		uniformBuffer = context.CreateBuffer(
			{ uniformMemorySize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR, MemoryUsage::upload, "Uniform Buffer" });

		currentPtr = (std::byte*)uniformBuffer.mappedPtr;
	}

	void ReleaseResources(const VulkanContext& context)
	{
		context.DestroyBuffer(uniformBuffer);

		for (auto i = 0; i < perFrameResources.size(); i++)
		{
			vkDestroyDescriptorPool(context.device, perFrameResources[i].frameDescriptorPool, nullptr);
		}

		vkDestroyDescriptorSetLayout(context.device, frameDescriptorSetLayout, nullptr);
	}

	void UploadJointMatrices(const std::vector<Math::Matrix4x4>& jointMatrices)
	{
		const auto size = jointMatrices.size() * sizeof(Math::Matrix4x4);

		if ((currentPtr + size) >= ((std::byte*)uniformBuffer.mappedPtr + uniformMemorySize))
		{
			currentPtr = (std::byte*)uniformBuffer.mappedPtr;
		}

		std::memcpy(currentPtr, jointMatrices.data(), size);
		jointMatricesOffset = currentPtr - (std::byte*)uniformBuffer.mappedPtr;
		jointMatricesSize = size;
		currentPtr += size;
	}

	std::byte* currentPtr{ nullptr };
	static constexpr VkDeviceSize uniformMemorySize{ 16 * 1024 * 1024 };

	GraphicsBuffer uniformBuffer;
	U32 jointMatricesOffset;
	U32 jointMatricesSize;

	struct PerFrameResources
	{
		VkDescriptorSet jointsMatriciesDescriptorSet;
		VkDescriptorPool frameDescriptorPool;
	};

	std::vector<PerFrameResources> perFrameResources;
	VkDescriptorSetLayout frameDescriptorSetLayout;
};


struct GpuDrivenRenderingPipeline
{
	void CreateResources(VulkanContext& context, U32 passesSlots, U32 maxDrawsPerSlot)
	{
		constexpr auto countByteSize = sizeof(U32);
		constexpr auto argumentByteSize = sizeof(VkDrawIndexedIndirectCommand);


		// context.
	}

	void TestDraw(VkCommandBuffer cmd)
	{
		// vkCmdDrawIndexedIndirectCount(cmd, indirectArguments.buffer, 0, indirectArgumentsCount.buffer, 0, 2048, 0);
	}


	GraphicsBuffer drawBuffer;
	GraphicsBuffer indirectArgumentsCount;
};


struct BasicGeometryPass
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;
	VkPipelineRenderingCreateInfo pipelineRendering{};


	VkImageView depthView;
	VkImage depthImage;
	VmaAllocation depthImageAllocation;
	VkFormat depthFormat{ VK_FORMAT_D32_SFLOAT };

	std::vector<VkPipeline> psoCache;

	void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const Scene& scene,
				 const FrameData::PerFrameResources& frame, const Camera& camera, const WindowViewport windowViewport)
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
									   .clearValue =
										   VkClearValue{ .depthStencil = VkClearDepthStencilValue{ 1.0f, 0u } } };

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
			const auto viewport = VkViewport{
				0.0f, 0.0f, static_cast<float>(windowViewport.width), static_cast<float>(windowViewport.height),
				0.0f, 1.0f
			};
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			const auto scissor = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } };
			vkCmdSetScissor(cmd, 0, 1, &scissor);
			static float time = 0.0f;
			time += ImGui::GetIO().DeltaTime;
			ShaderToyConstant constants = { time, static_cast<float>(windowViewport.width),
											static_cast<float>(windowViewport.height) };

			vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ShaderToyConstant),
							   &constants);
			vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 32, sizeof(ConstantsData),
							   &constantsData);


			const auto descriptorSets = std::array{ scene.geometryDescriptorSet, frame.jointsMatriciesDescriptorSet };


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

	void CreateViewDependentResources(VulkanContext& vulkanContext, const WindowViewport& windowViewport)
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
			const auto result = vmaCreateImage(vulkanContext.allocator, &imageCreateInfo, &allocationInfo, &depthImage,
											   &depthImageAllocation, nullptr);
			assert(result == VK_SUCCESS);
		}

		{
			const auto imageViewCreateInfo = VkImageViewCreateInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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
			const auto result = vkCreateImageView(vulkanContext.device, &imageViewCreateInfo, nullptr, &depthView);
			assert(result == VK_SUCCESS);
		}
	}

	void ReleaseViewDependentResources(const VulkanContext& vulkanContext)
	{
		vkDestroyImageView(vulkanContext.device, depthView, nullptr);
		vmaDestroyImage(vulkanContext.allocator, depthImage, depthImageAllocation);
	}

	void RecreateViewDependentResources(VulkanContext& vulkanContext, const WindowViewport& windowViewport)
	{
		ReleaseViewDependentResources(vulkanContext);
		CreateViewDependentResources(vulkanContext, windowViewport);
	}

	void CompileOpaqueMaterial(const VulkanContext& context, const MaterialAsset& materialAsset)
	{

		auto stream = std::ifstream{ "Assets/Shaders/BasicGeometry_Template.frag", std::ios::ate };

		auto size = stream.tellg();
		auto shader = std::string(size, '\0'); // construct string to stream size
		stream.seekg(0);
		stream.read(&shader[0], size);

		shader =
			std::regex_replace(shader, std::regex("%%material_evaluation_code%%"), materialAsset.surfaceShadingCode);

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

	VkPipeline CompileOpaqueMaterialPsoOnly(const VulkanContext& context, const MaterialAsset& materialAsset)
	{

		auto stream = std::ifstream{ "Assets/Shaders/BasicGeometry_Template.frag", std::ios::ate };

		auto size = stream.tellg();
		auto shader = std::string(size, '\0'); // construct string to stream size
		stream.seekg(0);
		stream.read(&shader[0], size);

		shader =
			std::regex_replace(shader, std::regex("%%material_evaluation_code%%"), materialAsset.surfaceShadingCode);

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

	void CreateResources(VulkanContext& vulkanContext, Scene& scene, FrameData& frameData,
						 const WindowViewport& windowViewport)
	{
		{
			const auto pushConstants = std::array{ VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
																		.offset = 0,
																		.size = sizeof(ShaderToyConstant) },
												   VkPushConstantRange{ .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
																		.offset = 32,
																		.size = sizeof(ConstantsData) } };

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
				vkCreatePipelineLayout(vulkanContext.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
			assert(result == VK_SUCCESS);
		}


		VkShaderModule fragmentShaderModule =
			vulkanContext.ShaderModuleFromFile(Utils::ShaderStage::Fragment, "Assets/Shaders/BasicGeometry.frag");
		VkShaderModule vertexShaderModule =
			vulkanContext.ShaderModuleFromFile(Utils::ShaderStage::Vertex, "Assets/Shaders/BasicGeometry.vert");

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


		pipelineRendering =
			VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
										   .pNext = nullptr,
										   .viewMask = 0,
										   .colorAttachmentCount = 1,
										   .pColorAttachmentFormats = &vulkanContext.swapchainImageFormat,
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
			vkCreateGraphicsPipelines(vulkanContext.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
		assert(result == VK_SUCCESS);

		vkDestroyShaderModule(vulkanContext.device, vertexShaderModule, nullptr);
		vkDestroyShaderModule(vulkanContext.device, fragmentShaderModule, nullptr);
		psoCache.push_back(pipeline);
		CreateViewDependentResources(vulkanContext, windowViewport);
	}

	void ReleaseResources(const VulkanContext& vulkanContext)
	{
		ReleaseViewDependentResources(vulkanContext);
		vkDestroyPipelineLayout(vulkanContext.device, pipelineLayout, nullptr);

		for (auto i = 0; i < psoCache.size(); i++)
		{
			vkDestroyPipeline(vulkanContext.device, psoCache[i], nullptr);
		}
	}
};
struct FullscreenQuadPass
{
	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	void CreateResources(const VulkanContext& vulkanContext)
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
		const auto result =
			vkCreatePipelineLayout(vulkanContext.device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
		assert(result == VK_SUCCESS);

		{
			VkShaderModule vertexShaderModule =
				vulkanContext.ShaderModuleFromFile(Utils::ShaderStage::Vertex, "Assets/Shaders/FullscreenQuad.vert");

			VkShaderModule fragmentShaderModule =
				vulkanContext.ShaderModuleFromFile(Utils::ShaderStage::Fragment, "Assets/Shaders/ShaderToySample.frag");

			const auto shaderStages = std::array{
				VkPipelineShaderStageCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
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
												 .pSpecializationInfo = nullptr }
			};


			const auto pipelineRendering =
				VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
											   .pNext = nullptr,
											   .viewMask = 0,
											   .colorAttachmentCount = 1,
											   .pColorAttachmentFormats = &vulkanContext.swapchainImageFormat,
											   .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
											   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };


			const auto vertexInputState =
				VkPipelineVertexInputStateCreateInfo{ .sType =
														  VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
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
				VkPipelineRasterizationStateCreateInfo{ .sType =
															VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
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
				VkPipelineDepthStencilStateCreateInfo{ .sType =
														   VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
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
													 .colorWriteMask = VK_COLOR_COMPONENT_A_BIT |
														 VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_R_BIT |
														 VK_COLOR_COMPONENT_G_BIT };

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

			const auto result = vkCreateGraphicsPipelines(vulkanContext.device, VK_NULL_HANDLE, 1, &pipelineCreateInfo,
														  nullptr, &pipeline);
			assert(result == VK_SUCCESS);


			vkDestroyShaderModule(vulkanContext.device, vertexShaderModule, nullptr);
			vkDestroyShaderModule(vulkanContext.device, fragmentShaderModule, nullptr);
		}
	}
	void ReleaseResources(const VulkanContext& vulkanContext)
	{
		vkDestroyPipelineLayout(vulkanContext.device, pipelineLayout, nullptr);
		vkDestroyPipeline(vulkanContext.device, pipeline, nullptr);
	}
};
struct ImGuiPass
{
	VkDescriptorPool descriptorPool;

	void CreateResources(VulkanContext& vulkanContext)
	{
		const auto poolSizes = std::array{ VkDescriptorPoolSize{ .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
																 .descriptorCount = 1 } };
		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		poolInfo.maxSets = 1;
		poolInfo.poolSizeCount = poolSizes.size();
		poolInfo.pPoolSizes = poolSizes.data();
		const auto result = vkCreateDescriptorPool(vulkanContext.device, &poolInfo, nullptr, &descriptorPool);
		assert(result == VK_SUCCESS);
	}

	void ReleaseResources(const VulkanContext& vulkanContext)
	{
		vkDestroyDescriptorPool(vulkanContext.device, descriptorPool, nullptr);
	}
};

void Framework::Application::Run()
{
#pragma region SDL window initialization
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("SDL_Init failed (%s)", SDL_GetError());
		return;
	}

	const auto applicationName = "Template Application";

	SDL_Window* window = SDL_CreateWindow(applicationName, 1920, 1080,
										  SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
	// could be replaced with win32 window, see minimal example here
	// https://learn.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program?source=recommendations

	if (!window)
	{
		SDL_Log("SDL_CreateWindow failed (%s)", SDL_GetError());
		SDL_Quit();
		return;
	}
#pragma endregion

#pragma region Vulkan Instance creation
	{
		const auto result = volkInitialize();
		assert(result == VK_SUCCESS);
	}

	auto instanceExtensions = std::vector<const char*>{};

	{
		auto extensionsCount = uint32_t{};
		auto extensions = SDL_Vulkan_GetInstanceExtensions(&extensionsCount);

		instanceExtensions.resize(extensionsCount);
		for (auto i = 0; i < extensionsCount; i++)
		{
			instanceExtensions[i] = extensions[i];
		}
	}

	auto vulkanContext = VulkanContext{};

	{
		const auto instanceLayers = std::array{ "VK_LAYER_KHRONOS_validation" };
		instanceExtensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

		const auto applicationInfo = VkApplicationInfo{ .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
														.pNext = nullptr,
														.pApplicationName = applicationName,
														.applicationVersion = 0,
														.pEngineName = applicationName,
														.engineVersion = 0,
														.apiVersion = VK_API_VERSION_1_3 };

		const auto instanceCreateInfo =
			VkInstanceCreateInfo{ .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
								  .pNext = nullptr,
								  .flags = 0,
								  .pApplicationInfo = &applicationInfo,
								  .enabledLayerCount = instanceLayers.size(),
								  .ppEnabledLayerNames = instanceLayers.data(),
								  .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
								  .ppEnabledExtensionNames = instanceExtensions.data() };

		const auto result = vkCreateInstance(&instanceCreateInfo, nullptr, &vulkanContext.instance);
		assert(result == VK_SUCCESS);
	}

	volkLoadInstance(vulkanContext.instance);
#pragma endregion

#pragma region Physical device selection
	{
		auto physicalDeviceCount = uint32_t{};
		auto result = vkEnumeratePhysicalDevices(vulkanContext.instance, &physicalDeviceCount, nullptr);
		assert(result == VK_SUCCESS);

		std::vector<VkPhysicalDevice> physicalDevices{};
		physicalDevices.resize(physicalDeviceCount);

		result = vkEnumeratePhysicalDevices(vulkanContext.instance, &physicalDeviceCount, physicalDevices.data());
		assert(result == VK_SUCCESS);


		struct FamilyQueueQueryInfo
		{
			uint32_t rating{ 0 };
			bool hasGraphicsQueue{ false };
			uint32_t graphicsQueueFamilyIndex{};
			uint32_t transferQueueFamilyIndex{};
			bool isDiscrete{ false };
		};

		auto physicalDevicesQuery = std::vector<FamilyQueueQueryInfo>{};
		physicalDevicesQuery.resize(physicalDeviceCount);

		for (auto i = 0; i < physicalDevices.size(); i++)
		{
			const auto& physicalDevice = physicalDevices[i];

			// QUEUE CHECKS
			{

				auto queueFamilyPropertiesCount = uint32_t{ 0 };
				vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount, nullptr);

				auto queueFamilyProperties = std::vector<VkQueueFamilyProperties2>{};
				queueFamilyProperties.resize(queueFamilyPropertiesCount);
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					queueFamilyProperties[index].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
					queueFamilyProperties[index].pNext = nullptr;
				}

				vkGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &queueFamilyPropertiesCount,
														  queueFamilyProperties.data());

				auto hasRequiredGraphicsQueueFamily = false;
				auto foundGraphicsIndex = 0;

				// SEARCH FOR GRAPHICS QUEUE WITH PRESENT SUPPORT
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					const auto hasGraphicsBit = (queueFamilyProperties[i].queueFamilyProperties.queueFlags &
												 VK_QUEUE_GRAPHICS_BIT) == VK_QUEUE_GRAPHICS_BIT;
					const auto canPresent =
						SDL_Vulkan_GetPresentationSupport(vulkanContext.instance, physicalDevice, index);

					hasRequiredGraphicsQueueFamily = hasGraphicsBit && canPresent;

					if (hasRequiredGraphicsQueueFamily)
					{
						foundGraphicsIndex = index;
						break;
					}
				}

				auto hasRequiredTransferQueueFamily = false;
				auto foundTransferIndex = 0;
				// SEARCH FOR TRANSFER QUEUE
				for (auto index = 0; index < queueFamilyProperties.size(); index++)
				{
					hasRequiredTransferQueueFamily = (queueFamilyProperties[i].queueFamilyProperties.queueFlags &
													  VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT;

					if (hasRequiredTransferQueueFamily and (index != foundGraphicsIndex))
					{
						foundTransferIndex = index;
						break;
					}
				}

				if (hasRequiredGraphicsQueueFamily and hasRequiredTransferQueueFamily)
				{
					physicalDevicesQuery[i].hasGraphicsQueue = hasRequiredGraphicsQueueFamily;
					physicalDevicesQuery[i].graphicsQueueFamilyIndex = foundGraphicsIndex;
					physicalDevicesQuery[i].transferQueueFamilyIndex = foundTransferIndex;
					physicalDevicesQuery[i].rating += 100;
				}
			}

			// DEVICE TYPE CHECK
			{
				VkPhysicalDeviceProperties2 properties{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
														.pNext = nullptr };
				vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

				if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				{
					physicalDevicesQuery[i].rating += 1000;
					physicalDevicesQuery[i].isDiscrete = true;
				}
			}
		}


		// SELECT HEIGHT RATING DEVICE
		{
			const auto& bestCandidate = std::max_element(
				physicalDevicesQuery.begin(), physicalDevicesQuery.end(),
				[](const FamilyQueueQueryInfo& a, const FamilyQueueQueryInfo& b) { return a.rating > b.rating; });
			const auto bestCandidateIndex = std::distance(physicalDevicesQuery.begin(), bestCandidate);

			vulkanContext.physicalDevice = physicalDevices[bestCandidateIndex];
			vulkanContext.graphicsQueueFamilyIndex = physicalDevicesQuery[bestCandidateIndex].graphicsQueueFamilyIndex;
			vulkanContext.transferQueueFamilyIndex = physicalDevicesQuery[bestCandidateIndex].transferQueueFamilyIndex;
			// TODO: Again, we require independent graphics and transfer queue for now!
			assert(vulkanContext.graphicsQueueFamilyIndex != vulkanContext.transferQueueFamilyIndex);
		}
	}
#pragma endregion

#pragma region Device creation
	{
		const auto enabledDeviceExtensions = std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };


		const auto queuePriority = 1.0f;
		const auto queueCreateInfos =
			std::array{ VkDeviceQueueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
												 .pNext = nullptr,
												 .flags = 0,
												 .queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex,
												 .queueCount = 1,
												 .pQueuePriorities = &queuePriority },
						VkDeviceQueueCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
												 .pNext = nullptr,
												 .flags = 0,
												 .queueFamilyIndex = vulkanContext.transferQueueFamilyIndex,
												 .queueCount = 1,
												 .pQueuePriorities = &queuePriority } };

		auto physicalDeviceFeatures13 = VkPhysicalDeviceVulkan13Features{};
		physicalDeviceFeatures13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		physicalDeviceFeatures13.pNext = nullptr;
		physicalDeviceFeatures13.synchronization2 = VK_TRUE;
		physicalDeviceFeatures13.dynamicRendering = VK_TRUE;

		auto physicalDeviceFeatures12 = VkPhysicalDeviceVulkan12Features{};
		physicalDeviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
		physicalDeviceFeatures12.pNext = &physicalDeviceFeatures13;
		physicalDeviceFeatures12.scalarBlockLayout = VK_TRUE;

		auto physicalDeviceFeatures11 = VkPhysicalDeviceVulkan11Features{};
		physicalDeviceFeatures11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
		physicalDeviceFeatures11.pNext = &physicalDeviceFeatures12;
		physicalDeviceFeatures11.shaderDrawParameters = VK_TRUE;


		const auto deviceCreateInfo =
			VkDeviceCreateInfo{ .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
								.pNext = &physicalDeviceFeatures11,
								.flags = 0,
								.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
								.pQueueCreateInfos = queueCreateInfos.data(),
								.enabledLayerCount = 0,
								.ppEnabledLayerNames = nullptr,
								.enabledExtensionCount = enabledDeviceExtensions.size(),
								.ppEnabledExtensionNames = enabledDeviceExtensions.data(),
								.pEnabledFeatures = nullptr };

		const auto result =
			vkCreateDevice(vulkanContext.physicalDevice, &deviceCreateInfo, nullptr, &vulkanContext.device);
		assert(result == VK_SUCCESS);
	}
	volkLoadDevice(vulkanContext.device);
#pragma endregion

#pragma region CreateResources VMA
	{
		const auto vulkanFunctions =
			VmaVulkanFunctions{ .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
								.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
								.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
								.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
								.vkAllocateMemory = vkAllocateMemory,
								.vkFreeMemory = vkFreeMemory,
								.vkMapMemory = vkMapMemory,
								.vkUnmapMemory = vkUnmapMemory,
								.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
								.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
								.vkBindBufferMemory = vkBindBufferMemory,
								.vkBindImageMemory = vkBindImageMemory,
								.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
								.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
								.vkCreateBuffer = vkCreateBuffer,
								.vkDestroyBuffer = vkDestroyBuffer,
								.vkCreateImage = vkCreateImage,
								.vkDestroyImage = vkDestroyImage,
								.vkCmdCopyBuffer = vkCmdCopyBuffer,
								.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
								.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
								.vkBindBufferMemory2KHR = vkBindBufferMemory2,
								.vkBindImageMemory2KHR = vkBindImageMemory2,
								.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
								.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements,
								.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements };


		const auto allocatorCreateInfo = VmaAllocatorCreateInfo{ .flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT,
																 .physicalDevice = vulkanContext.physicalDevice,
																 .device = vulkanContext.device,
																 .pVulkanFunctions = &vulkanFunctions,
																 .instance = vulkanContext.instance,
																 .vulkanApiVersion = VK_API_VERSION_1_3 };

		const auto result = vmaCreateAllocator(&allocatorCreateInfo, &vulkanContext.allocator);
		assert(result == VK_SUCCESS);
	}
#pragma endregion


	WindowViewport windowViewport{};
	windowViewport.UpdateSize(window);
	windowViewport.shouldRecreateWindowSizeDependedResources = false;

#pragma region Swapchain creation
	{
		const auto result = SDL_Vulkan_CreateSurface(window, vulkanContext.instance, nullptr, &vulkanContext.surface);
		assert(result);
	}
	{
		auto supported = VkBool32{};
		const auto result = vkGetPhysicalDeviceSurfaceSupportKHR(
			vulkanContext.physicalDevice, vulkanContext.graphicsQueueFamilyIndex, vulkanContext.surface, &supported);
		assert(result == VK_SUCCESS);
		assert(supported == VK_TRUE);
	}
	{
		vulkanContext.CreateSwapchain(windowViewport);
	}
#pragma endregion

#pragma region Double-buffered resource creation
	// assume 2, for now
	vulkanContext.frameResourceCount = 2;

	vulkanContext.perFrameResources.resize(vulkanContext.frameResourceCount);
	for (auto i = 0; i < vulkanContext.frameResourceCount; i++)
	{
		const auto fenceCreateInfo = VkFenceCreateInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
														.pNext = nullptr,
														.flags = VK_FENCE_CREATE_SIGNALED_BIT };

		const auto result = vkCreateFence(vulkanContext.device, &fenceCreateInfo, nullptr,
										  &vulkanContext.perFrameResources[i].frameFinished);
		assert(result == VK_SUCCESS);
	}

	for (auto i = 0; i < vulkanContext.frameResourceCount; i++)
	{
		const auto semaphoreCreateInfo =
			VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
		const auto result = vkCreateSemaphore(vulkanContext.device, &semaphoreCreateInfo, nullptr,
											  &vulkanContext.perFrameResources[i].readyToPresent);
		assert(result == VK_SUCCESS);
	}

	for (auto i = 0; i < vulkanContext.frameResourceCount; i++)
	{
		const auto semaphoreCreateInfo =
			VkSemaphoreCreateInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = nullptr, .flags = 0 };
		const auto result = vkCreateSemaphore(vulkanContext.device, &semaphoreCreateInfo, nullptr,
											  &vulkanContext.perFrameResources[i].readyToRender);
		assert(result == VK_SUCCESS);
	}
#pragma endregion

#pragma region Command buffer creation

	for (auto i = 0; i < vulkanContext.frameResourceCount; i++)
	{
		const auto poolCreateInfo =
			VkCommandPoolCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
									 .pNext = nullptr,
									 .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
									 .queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex };
		const auto result = vkCreateCommandPool(vulkanContext.device, &poolCreateInfo, nullptr,
												&vulkanContext.perFrameResources[i].commandPool);
		assert(result == VK_SUCCESS);
	}

	// create only one command buffer per pool, fon now, it might change in the future
	for (auto i = 0; i < vulkanContext.frameResourceCount; i++)
	{
		const auto allocateInfo =
			VkCommandBufferAllocateInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
										 .pNext = nullptr,
										 .commandPool = vulkanContext.perFrameResources[i].commandPool,
										 .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
										 .commandBufferCount = 1 };
		const auto result = vkAllocateCommandBuffers(vulkanContext.device, &allocateInfo,
													 &vulkanContext.perFrameResources[i].commandBuffer);
		assert(result == VK_SUCCESS);
	}

#pragma endregion

#pragma region Request graphics and transfer queue
	{
		const auto deviceQueueInfo = VkDeviceQueueInfo2{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
														 .pNext = nullptr,
														 .flags = 0,
														 .queueFamilyIndex = vulkanContext.graphicsQueueFamilyIndex,
														 .queueIndex = 0 };
		vkGetDeviceQueue2(vulkanContext.device, &deviceQueueInfo, &vulkanContext.graphicsQueue);
	}
	{
		const auto deviceQueueInfo = VkDeviceQueueInfo2{ .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2,
														 .pNext = nullptr,
														 .flags = 0,
														 .queueFamilyIndex = vulkanContext.transferQueueFamilyIndex,
														 .queueIndex = 0 };
		vkGetDeviceQueue2(vulkanContext.device, &deviceQueueInfo, &vulkanContext.transferQueue);
	}
#pragma endregion

	FrameData frameData;
	frameData.CreateResources(vulkanContext, vulkanContext.frameResourceCount);

#pragma region Scene preparation
	auto scene = Scene{};
	scene.CreateResources(vulkanContext);
	// scene.Upload("Assets/Meshes/makarov-pm-fps-animations/source/Arms_Mak.fbx", vulkanContext);
	// scene.Upload("Assets/Meshes/junkrat/source/animated_2.fbx", vulkanContext);
	// scene.Upload("Assets/Meshes/mira/source/Mira2.fbx", vulkanContext);

	 //scene.Upload("Assets/Meshes/aaron/source/Aaron/SK_Aaron.gltf", vulkanContext);
	scene.Upload("Assets/Meshes/leslie_kornwell/scene.gltf", vulkanContext);
	// scene.Upload("Assets/Meshes/the_last_stronghold_animated/scene.gltf", vulkanContext);
	// scene.Upload("Assets/Meshes/CesiumMan.glb", vulkanContext);
	/*const auto myStaticMesh = scene.Upload("Assets/Meshes/bunny.obj", vulkanContext);*/
#pragma endregion

#pragma region Setup Camera
	auto camera = Camera{ .position = glm::vec3{ 0.0f, 0.0f, 0.0f },
						  .forward = glm::vec3{ 0.0f, 0.0f, 1.0f },
						  .up = glm::vec3{ 0.0f, 1.0f, 0.0f },
						  .movementSpeed = 0.01f,
						  .sensitivity = 0.2f };

	auto moveCameraFaster = false;
	const auto fastSpeed = 250.0f;
#pragma endregion

#pragma region Geometry pass initialization
	BasicGeometryPass basicGeometryPass;
	basicGeometryPass.CreateResources(vulkanContext, scene, frameData, windowViewport);

	MaterialAsset material01 = MaterialAsset{ sample_surface_01 };
	MaterialAsset material02 = MaterialAsset{ sample_surface_02 };

	basicGeometryPass.CompileOpaqueMaterial(vulkanContext, material01);
	basicGeometryPass.CompileOpaqueMaterial(vulkanContext, material02);

	FullscreenQuadPass fullscreenQuadPass;
	fullscreenQuadPass.CreateResources(vulkanContext);
#pragma endregion

#pragma region ImGui initialization
	ImGuiPass imGuiPass{};
	imGuiPass.CreateResources(vulkanContext);
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	float dpiScale = SDL_GetWindowDisplayScale(window);
	io.FontGlobalScale = dpiScale;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();

	const auto pipelineRendering =
		VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
									   .pNext = nullptr,
									   .viewMask = 0,
									   .colorAttachmentCount = 1,
									   .pColorAttachmentFormats = &vulkanContext.swapchainImageFormat,
									   .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
									   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };

	auto initInfo = ImGui_ImplVulkan_InitInfo{ .Instance = vulkanContext.instance,
											   .PhysicalDevice = vulkanContext.physicalDevice,
											   .Device = vulkanContext.device,
											   .QueueFamily = vulkanContext.graphicsQueueFamilyIndex,
											   .Queue = vulkanContext.graphicsQueue,
											   .DescriptorPool = imGuiPass.descriptorPool,
											   .MinImageCount = vulkanContext.swapchainImageCount,
											   .ImageCount = vulkanContext.swapchainImageCount,
											   .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
											   .UseDynamicRendering = true,
											   .PipelineRenderingCreateInfo = pipelineRendering,
											   .Allocator = nullptr };


	ImGui_ImplVulkan_Init(&initInfo);
	ImGui_ImplSDL3_InitForVulkan(window);
#pragma endregion

	bool shouldRun = true;
	auto frameIndex = uint32_t{ 0 };
	static std::vector<AnimationInstance> animationInstances;

	for (auto i = 0; i < scene.animationDataSet.animations.size(); i++)
	{
		const auto& animation = scene.animationDataSet.animations[i];
		animationInstances.push_back(AnimationInstance{
			.data = scene.animationDataSet.animations[i], .playbackRate = 1.00f, .startTime = 0.0f, .loop = true });
	}

	auto  assetImporterEditor = Editor::AssetImporterEditor{};
	while (shouldRun)
	{
#pragma region Handle window events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT)
			{
				shouldRun = false;
				break;
			}
			if (event.type == SDL_EVENT_WINDOW_RESIZED or event.type == SDL_EVENT_WINDOW_MAXIMIZED or
				event.type == SDL_EVENT_WINDOW_SHOWN or event.type == SDL_EVENT_WINDOW_RESTORED)
			{
				windowViewport.UpdateSize(window);
			}
			if (event.type == SDL_EVENT_WINDOW_MINIMIZED or event.type == SDL_EVENT_WINDOW_HIDDEN)
			{
				windowViewport.Reset();
			}
		}
#pragma endregion

		if (not windowViewport.IsVisible())
		{
			continue;
		}

#pragma region Recreate frame Dependent resources

		if (windowViewport.shouldRecreateWindowSizeDependedResources)
		{
			{
				const auto result = vkQueueWaitIdle(vulkanContext.graphicsQueue);
				assert(result == VK_SUCCESS);
			}
			vulkanContext.RecreateSwapchain(windowViewport);
			basicGeometryPass.RecreateViewDependentResources(vulkanContext, windowViewport);
			windowViewport.shouldRecreateWindowSizeDependedResources = false;
		}

#pragma endregion

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();

		ImGui::NewFrame();

#pragma region Camera Controller

		moveCameraFaster = ImGui::IsKeyDown(ImGuiKey_LeftShift);

		if (ImGui::IsKeyDown(ImGuiKey_W))
		{
			camera.position += camera.forward * camera.movementSpeed * (moveCameraFaster ? fastSpeed : 1.0f) *
				io.DeltaTime * camera.movementSpeedScale;
		}
		if (ImGui::IsKeyDown(ImGuiKey_S))
		{
			camera.position -= camera.forward * camera.movementSpeed * (moveCameraFaster ? fastSpeed : 1.0f) *
				io.DeltaTime * camera.movementSpeedScale;
		}
		if (ImGui::IsKeyDown(ImGuiKey_A))
		{
			camera.position += glm::normalize(glm::cross(camera.forward, camera.up)) * camera.movementSpeed *
				io.DeltaTime * (moveCameraFaster ? fastSpeed : 1.0f) * camera.movementSpeedScale;
		}
		if (ImGui::IsKeyDown(ImGuiKey_D))
		{
			camera.position -= glm::normalize(glm::cross(camera.forward, camera.up)) * camera.movementSpeed *
				io.DeltaTime * (moveCameraFaster ? fastSpeed : 1.0f) * camera.movementSpeedScale;
		}

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) and not io.WantCaptureMouse)
		{
			auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

			const auto right = glm::normalize(cross(camera.forward, camera.up));
			const auto up = glm::normalize(cross(right, camera.forward));

			const auto f = glm::normalize(camera.forward + right * delta.y + up * delta.x);

			auto rotationAxis = glm::normalize(glm::cross(f, camera.forward));

			if (glm::length(rotationAxis) >= 0.1f)
			{
				const auto rotation =
					glm::rotate(glm::identity<glm::mat4>(),
								glm::radians(glm::length(glm::vec2(delta.x, delta.y)) * camera.sensitivity), f);

				camera.forward = glm::normalize(glm::vec3(rotation * glm::vec4(camera.forward, 0.0f)));
			}
		}

#pragma endregion
		static float time = 0.0f;

		static bool show_demo_window = true;
		ImGui::ShowDemoWindow(&show_demo_window);

		assetImporterEditor.Draw();

		static float animationTime = 0.0f;
		static bool useGlobalTimeInAnimation = true;
		static int selectedAnimation = 0;
		static float blendFactor = 0.0f;
		static bool enableDebugDraw = { false };

		static std::vector<VkPipeline> psoForLateDestruction;

		/*if (not psoForLateDestruction.empty())
		{

			for (auto i = 0; psoForLateDestruction.size(); i++)
			{
				vkDestroyPipeline(vulkanContext.device, psoForLateDestruction[0], nullptr);
			}
			psoForLateDestruction.clear();
		}*/

		ImGui::Begin("Editor");

		ImGui::SeparatorText("Materials");

		for (auto i = 0; i < basicGeometryPass.psoCache.size(); i++)
		{
			ImGui::PushID(i);
			ImGui::Text("pso_%i", i);
			ImGui::SameLine();
			if (ImGui::Button("Alter Material"))
			{
				std::string generatedCode = "void surface(in Geometry geometry, out vec4 color){{ color = "
											"vec4({},{},{},1.0f);}}";
				generatedCode = runtime_format(generatedCode, (std::rand() % 255) / 255.0f, (std::rand() % 255) / 255.0f,
											   (std::rand() % 255) / 255.0f);

				MaterialAsset myNewMaterial{ generatedCode };
				const auto pso = basicGeometryPass.CompileOpaqueMaterialPsoOnly(vulkanContext, myNewMaterial);
				// psoForLateDestruction.push_back(basicGeometryPass.psoCache[i]);


				const auto result = vkQueueWaitIdle(vulkanContext.graphicsQueue);
				assert(result == VK_SUCCESS);
				vkDestroyPipeline(vulkanContext.device, basicGeometryPass.psoCache[i], nullptr);

				basicGeometryPass.psoCache[i] = pso;
			}
			ImGui::PopID();
		}


		ImGui::SeparatorText("Animations");

		ImGui::Checkbox("Enable Debug Draw", &enableDebugDraw);

		if (ImGui::BeginListBox("##animations_list_box", ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing())))
		{
			for (int n = 0; n < animationInstances.size(); n++)
			{
				bool is_selected = (selectedAnimation == n);
				ImGuiSelectableFlags flags = (selectedAnimation == n) ? ImGuiSelectableFlags_Highlight : 0;
				if (ImGui::Selectable(animationInstances[n].data.animationName.empty() ?
										  "[unnamed]" :
										  animationInstances[n].data.animationName.c_str(),
									  is_selected, flags))
				{
					selectedAnimation = n;
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndListBox();
		}
		for (auto i = 0; i < animationInstances.size(); i++)
		{
		}

		// ImGui::SliderFloat("blend with 4", &blendFactor, 0.0f, 1.0f);

		if (ImGui::Button("Reset Time"))
		{
			time = 0.0f;
		}

		ImGui::SliderFloat("Playback Rate", &animationInstances[selectedAnimation].playbackRate, 0.0f, 4.0f);
		ImGui::End();


		const auto pose0 = SamplePose(scene.animationDataSet, animationInstances[selectedAnimation],
									  useGlobalTimeInAnimation ? time : animationTime);

		const auto pose1 =
			SamplePose(scene.animationDataSet, animationInstances[std::min({ (int)animationInstances.size() - 1, 4 })],
					   useGlobalTimeInAnimation ? time : animationTime);

		const auto pose = BlendPose(pose0, pose1, blendFactor);

		auto jointMatrices = ComputeJointsMatrices(pose, scene.skeletons[0]);

		auto offsetMatrices = jointMatrices;
		ApplyBindPose(offsetMatrices, scene.skeletons[0]);

		frameData.UploadJointMatrices(offsetMatrices);


		if (enableDebugDraw)
		{
			auto model = glm::rotate(glm::identity<glm::mat4>(), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));

			const auto aspectRatio =
				static_cast<float>(windowViewport.width) / static_cast<float>(windowViewport.height);
			const auto projection = glm::perspective(glm::radians(60.0f), aspectRatio, 0.001f, 100.0f);
			const auto view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);

			auto& drawList = *ImGui::GetBackgroundDrawList();
			drawList.PushClipRectFullScreen();
			const auto debugDrawColor = IM_COL32(100, 100, 250, 255);

			/*if (isVisible)
			{
				auto p = ImVec2{ meshOriginPositionScreenSpace.x, meshOriginPositionScreenSpace.y };
				drawList.AddCircleFilled(p, 4.0f, debugDrawColor);
				drawList.AddText(ImVec2{ p.x - 40.0f, p.y + 16.0f }, debugDrawColor, "mesh");
			}*/

			const auto& skeleton = scene.skeletons.front();


			for (auto i = 0; i < skeleton.joints.size(); i++)
			{
				auto& joint = skeleton.joints[i];

				if (joint.parentIndex >= 0)
				{
					const auto p0 = glm::vec3{ jointMatrices[i][3] } * glm::vec3{ 1.0, -1.0f, 1.0f };
					const auto p1 = glm::vec3{ jointMatrices[joint.parentIndex][3] } * glm::vec3{ 1.0, -1.0f, 1.0f };
					const auto origin = glm::vec3{ 0.0f, 0.0f, 0.0f };

					const auto [p0screen, p0IsVisible] =
						GetScreenSpacePosition(glm::vec2{ windowViewport.width, windowViewport.height },
											   view * model * jointMatrices[i], projection, origin);

					const auto [p1screen, p1IsVisible] =
						GetScreenSpacePosition(glm::vec2{ windowViewport.width, windowViewport.height },
											   view * model * jointMatrices[joint.parentIndex], projection, origin);

					if (p0IsVisible and p1IsVisible)
					{
						drawList.AddLine(ImVec2{ p0screen.x, p0screen.y }, ImVec2{ p1screen.x, p1screen.y },
										 debugDrawColor, 3.0f);
						drawList.AddText(ImVec2{ (p1screen.x + p0screen.x) * 0.5f - 40.0f,
												 (p1screen.y + p0screen.y) * 0.5f + 16.0f },
										 debugDrawColor, joint.name.c_str());
					}
				}
			}

			drawList.PopClipRect();
		}

		ImGui::Render();
		ImDrawData* drawData = ImGui::GetDrawData();

		const auto perFrameResourceIndex = frameIndex % vulkanContext.frameResourceCount;

#pragma region Wait for resource reuse
		{

			const auto result =
				vkWaitForFences(vulkanContext.device, 1,
								&vulkanContext.perFrameResources[perFrameResourceIndex].frameFinished, VK_TRUE, ~0ull);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetFences(vulkanContext.device, 1,
											  &vulkanContext.perFrameResources[perFrameResourceIndex].frameFinished);
			assert(result == VK_SUCCESS);
		}
		{
			const auto result = vkResetCommandPool(
				vulkanContext.device, vulkanContext.perFrameResources[perFrameResourceIndex].commandPool, 0);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Acquire Swapchain image
		auto imageIndex = uint32_t{ 0 };

		{

			const auto acquireNextImageInfo =
				VkAcquireNextImageInfoKHR{ .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
										   .pNext = nullptr,
										   .swapchain = vulkanContext.swapchain,
										   .timeout = ~0ull,
										   .semaphore =
											   vulkanContext.perFrameResources[perFrameResourceIndex].readyToRender,
										   .fence = VK_NULL_HANDLE,
										   .deviceMask = 1 };

			const auto result = vkAcquireNextImage2KHR(vulkanContext.device, &acquireNextImageInfo, &imageIndex);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Begin CommandBuffer
		auto& cmd = vulkanContext.perFrameResources[perFrameResourceIndex].commandBuffer;
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
									   .image = vulkanContext.swapchainImages[imageIndex],
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
		const auto colorAttachment =
			VkRenderingAttachmentInfo{ .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
									   .pNext = nullptr,
									   .imageView = vulkanContext.swapchainImageViews[imageIndex],
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

		vulkanContext.BeginDebugLabelName(cmd, "Background Rendering", DebugColorPalette::Red);
		vkCmdBeginRendering(cmd, &renderingInfo);
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, fullscreenQuadPass.pipeline);
			const auto viewport = VkViewport{
				0.0f, 0.0f, static_cast<float>(windowViewport.width), static_cast<float>(windowViewport.height),
				0.0f, 1.0f
			};
			vkCmdSetViewport(cmd, 0, 1, &viewport);
			const auto scissor = VkRect2D{ { 0, 0 }, { windowViewport.width, windowViewport.height } };
			vkCmdSetScissor(cmd, 0, 1, &scissor);

			time += ImGui::GetIO().DeltaTime;


			ShaderToyConstant constants = { time, static_cast<float>(windowViewport.width),
											static_cast<float>(windowViewport.height) };

			vkCmdPushConstants(cmd, fullscreenQuadPass.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
							   sizeof(ShaderToyConstant), &constants);

			vkCmdDraw(cmd, 3, 1, 0, 0);
		}
		vkCmdEndRendering(cmd);
		vulkanContext.EndDebugLabelName(cmd);
		vulkanContext.BeginDebugLabelName(cmd, "Mesh Rendering", DebugColorPalette::Green);

		const auto descriptorBufferInfo = VkDescriptorBufferInfo{
			.buffer = frameData.uniformBuffer.buffer,
			.offset = frameData.jointMatricesOffset,
			.range = frameData.jointMatricesSize,
		};

		const auto dsWrite =
			VkWriteDescriptorSet{ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
								  .pNext = nullptr,
								  .dstSet =
									  frameData.perFrameResources[perFrameResourceIndex].jointsMatriciesDescriptorSet,
								  .dstBinding = 0,
								  .dstArrayElement = 0,
								  .descriptorCount = 1,
								  .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
								  .pImageInfo = nullptr,
								  .pBufferInfo = &descriptorBufferInfo,
								  .pTexelBufferView = nullptr };

		vkUpdateDescriptorSets(vulkanContext.device, 1, &dsWrite, 0, nullptr);

		basicGeometryPass.Execute(cmd, vulkanContext.swapchainImageViews[imageIndex], scene,
								  frameData.perFrameResources[perFrameResourceIndex], camera, windowViewport);

		vulkanContext.EndDebugLabelName(cmd);

		vulkanContext.BeginDebugLabelName(cmd, "GUI Rendering", DebugColorPalette::Blue);
		vkCmdBeginRendering(cmd, &renderingInfo);
		ImGui_ImplVulkan_RenderDrawData(drawData, cmd);
		vkCmdEndRendering(cmd);
		vulkanContext.EndDebugLabelName(cmd);

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
									   .image = vulkanContext.swapchainImages[imageIndex],
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
				.commandBuffer = vulkanContext.perFrameResources[perFrameResourceIndex].commandBuffer,
				.deviceMask = 1 } };
			const auto waitSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.pNext = nullptr,
				.semaphore = vulkanContext.perFrameResources[perFrameResourceIndex].readyToRender,
				.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
				.deviceIndex = 1 } };
			const auto signalSemaphoreInfos = std::array{ VkSemaphoreSubmitInfo{
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
				.pNext = nullptr,
				.semaphore = vulkanContext.perFrameResources[perFrameResourceIndex].readyToPresent,
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
			const auto result = vkQueueSubmit2(vulkanContext.graphicsQueue, 1, &submit,
											   vulkanContext.perFrameResources[perFrameResourceIndex].frameFinished);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

#pragma region Present image
		{

			const auto presentInfo =
				VkPresentInfoKHR{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
								  .pNext = nullptr,
								  .waitSemaphoreCount = 1,
								  .pWaitSemaphores =
									  &vulkanContext.perFrameResources[perFrameResourceIndex].readyToPresent,
								  .swapchainCount = 1,
								  .pSwapchains = &vulkanContext.swapchain,
								  .pImageIndices = &imageIndex,
								  .pResults = nullptr };

			const auto result = vkQueuePresentKHR(vulkanContext.graphicsQueue, &presentInfo);
			assert(result == VK_SUCCESS);
		}
#pragma endregion

		frameIndex++;
	}

#pragma region Vulkan objects cleanup

	// wait until all submitted work is finished
	{
		const auto result = vkQueueWaitIdle(vulkanContext.graphicsQueue);
		assert(result == VK_SUCCESS);
	}
	{
		const auto result = vkQueueWaitIdle(vulkanContext.transferQueue);
		assert(result == VK_SUCCESS);
	}

	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	frameData.ReleaseResources(vulkanContext);
	scene.ReleaseResources(vulkanContext);
	imGuiPass.ReleaseResources(vulkanContext);
	basicGeometryPass.ReleaseResources(vulkanContext);
	fullscreenQuadPass.ReleaseResources(vulkanContext);

	vmaDestroyAllocator(vulkanContext.allocator);

	{
		vulkanContext.ReleaseSwapchainResources();

		for (auto i = 0; i < vulkanContext.perFrameResources.size(); i++)
		{
			const auto& perFrameResource = vulkanContext.perFrameResources[i];
			vkDestroyFence(vulkanContext.device, perFrameResource.frameFinished, nullptr);
			vkDestroySemaphore(vulkanContext.device, perFrameResource.readyToPresent, nullptr);
			vkDestroySemaphore(vulkanContext.device, perFrameResource.readyToRender, nullptr);

			vkDestroyCommandPool(vulkanContext.device, perFrameResource.commandPool, nullptr);
		}

		SDL_Vulkan_DestroySurface(vulkanContext.instance, vulkanContext.surface, nullptr);


		vkDestroyDevice(vulkanContext.device, nullptr);
		vkDestroyInstance(vulkanContext.instance, nullptr);
	}
#pragma endregion

	SDL_DestroyWindow(window);
	SDL_Quit();
}