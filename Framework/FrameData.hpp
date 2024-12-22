#pragma once

#include "Math.hpp"
#include "VulkanRHI.hpp"

namespace Framework
{
	namespace Graphics
	{
		struct FrameData
		{

			void CreateResources(const VulkanContext& context, int frameInFlights = 2);
			void ReleaseResources(const VulkanContext& context);
			void UploadJointMatrices(const std::vector<Math::Matrix4x4>& jointMatrices);

			std::byte* currentPtr{ nullptr };
			static constexpr VkDeviceSize uniformMemorySize{ 16 * 1024 * 1024 };

			GraphicsBuffer uniformBuffer{};
			U32 jointMatricesOffset{};
			U32 jointMatricesSize{};

			struct PerFrameResources
			{
				VkDescriptorSet jointsMatricesDescriptorSet;
				VkDescriptorPool frameDescriptorPool;
			};

			std::vector<PerFrameResources> perFrameResources;
			VkDescriptorSetLayout frameDescriptorSetLayout;
		};
	} // namespace Graphics

} // namespace Framework