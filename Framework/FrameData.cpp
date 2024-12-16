#include "FrameData.hpp"

using namespace Framework;
using namespace Framework::Graphics;

void FrameData::CreateResources(const VulkanContext& context, int frameInFlights)
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
											 .flags = 0, // VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
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

	vkDestroyDescriptorSetLayout(context.device, frameDescriptorSetLayout, nullptr);
}

void FrameData::UploadJointMatrices(const std::vector<Math::Matrix4x4>& jointMatrices)
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