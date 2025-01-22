#include "FrameData.hpp"
#include "Profiler.hpp"

using namespace Framework;
using namespace Framework::Graphics;

void FrameData::CreateResources(const VulkanContext& context, int frameInFlights)
{
	frameBindGroupLayout = context.CreateBindGroupLayout(
		BindGroupLayoutDesc{ .bindings = { UniformBufferBinding{} }, .debugName = "Uniform DS Layout" });

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
											 .pSetLayouts = &frameBindGroupLayout.layout };
			const auto result = vkAllocateDescriptorSets(context.device, &allocationInfo,
														 &perFrameResources[i].jointsMatricesDescriptorSet);
			assert(result == VK_SUCCESS);
			context.SetObjectDebugName(VK_OBJECT_TYPE_DESCRIPTOR_SET,
									   (uint64_t)perFrameResources[i].jointsMatricesDescriptorSet, "Joints Matrices");
		}
	}

	uniformBuffer = context.CreateBuffer(
		{ uniformMemorySize, VK_BUFFER_USAGE_2_UNIFORM_BUFFER_BIT_KHR, MemoryUsage::upload, "Uniform Buffer" });

	currentPtr = (std::byte*)uniformBuffer.mappedPtr;
}

void FrameData::ReleaseResources(const VulkanContext& context)
{
	context.DestroyBuffer(uniformBuffer);

	for (auto i = 0; i < perFrameResources.size(); i++)
	{
		vkDestroyDescriptorPool(context.device, perFrameResources[i].frameDescriptorPool, nullptr);
	}

	context.DestroyBindGroupLayout(frameBindGroupLayout);
}

void FrameData::UploadJointMatrices(const std::vector<Math::Matrix4x4>& jointMatrices)
{
	ZoneScoped;
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