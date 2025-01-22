#pragma once

#include "Core.hpp"
#include "Scene.hpp"
#include "VulkanRHI.hpp"

namespace Framework
{
	namespace Graphics
	{
		struct UnwrapComputeConstants
		{
			U32 verticesOffset;
			U32 verticesCount;
			U32 verticesStride;
			U32 dstOffset;
		};

		struct UnwrapGeometryForAccelerationStructurePass
		{
			ComputePipeline unwrapPositionComputePipeline{};
			ComputePipeline unwrapIndiciesComputePipeline{};
			PipelineLayout pipelineLayout{};
			BindGroupLayout bindGroupLayout{};
			GraphicsBuffer unwrappedGeometryBuffer{};
			VkDeviceAddress unwrappedGeometryData{};

			VkDescriptorPool unwrappedGeometryDescriptorPool{};
			VkDescriptorSet unwrappedGeometryDescriptorSet{};
			std::array<VkDescriptorSet, 2> descriptorSets{};

			void UnwrapMesh(const VkCommandBuffer& cmd, const IndexedStaticMesh& mesh);

			void CreateResources(const VulkanContext& context, const Scene& scene);
			void ReleaseResources(const VulkanContext& context);
		};
	} // namespace Graphics
} // namespace Framework