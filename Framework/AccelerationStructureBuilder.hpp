#pragma once

#include "Core.hpp"
#include "Scene.hpp"
#include "UnwrapGeometryForAccelerationStructurePass.hpp"
#include "VulkanRHI.hpp"

#include <vector>


namespace Framework
{
	namespace Graphics
	{
		struct TriangleBottomLevelAccelerationStrcutureBuildInfo
		{
			VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
			const IndexedStaticMesh* mesh;
		};

		struct TriangleBottomLevelAccelerationStructure
		{
			VkAccelerationStructureKHR accelerationStructure;
			VkDeviceAddress deviceAddress;
			U32 bufferOffset;
			U32 bufferSize;
		};

		struct TopLevelAccelerationStructure
		{
			VkAccelerationStructureKHR as;
			GraphicsBuffer buffer;
			VkDeviceAddress deviceAddress;
		};

		struct AccelerationStructureBuilder
		{
			constexpr static U32 initialScratchBufferSize{ 16 * 1024 * 1024 };
			GraphicsBuffer scratchBuffer{};
			VkDeviceAddress scratchData{};
			U32 scratchBufferSize{};

			std::vector<TriangleBottomLevelAccelerationStructure> accelerationStructures{};
			std::vector<Math::Matrix4x4> transforms{};
			std::vector<TriangleBottomLevelAccelerationStrcutureBuildInfo> asBuildInfos{};
			TopLevelAccelerationStructure topLevelAs{};
			GraphicsBuffer bottomLevelAsBuffer{};

			GraphicsBuffer instanceData{};

			const VulkanContext* vulkanContext;
			UnwrapGeometryForAccelerationStructurePass* unwrapPass;

			void AddMeshAsNewBottomLevelAccelerationStructure(const IndexedStaticMesh& mesh, Math::Matrix4x4 transform);

			void BuildBottomLevelAccelerationStructures(const VkCommandBuffer& cmd);
			void BuildTopLevelAccelerationStructure(const VkCommandBuffer& cmd);

			void CreateResources(const VulkanContext& context, UnwrapGeometryForAccelerationStructurePass& pass);
			void ReleaseResources(const VulkanContext& context);

		private:
			void ResizeScratchBuffer(const U32 newSize);
		};
	} // namespace Graphics
} // namespace Framework