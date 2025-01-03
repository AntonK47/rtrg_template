#pragma once

#include "Animation.hpp"
#include "VulkanRHI.hpp"

#include <string_view>
#include <vector>

namespace Framework
{
	struct IndexedStaticMesh
	{
		// has only one stream position
		U32 indicesOffset;
		U32 indicesCount;
		U32 verticesOffset;
		U32 verticesCount;
	};
	};

	struct Scene
	{
		void CreateResources(const Graphics::VulkanContext& context);
		void ReleaseResources(const Graphics::VulkanContext& context);

		void Upload(const std::string_view mesh, const Graphics::VulkanContext& context);


		/*struct GpuSubMesh
		{
		};

		struct GpuScene
		{
			void UpdateInstances(Scene& scene)
			{
			}

			void AllocateInstances();
			void AllocateMeshes();

			Graphics::GraphicsBuffer instances;
			Graphics::GraphicsBuffer meshes;
			Graphics::GraphicsBuffer lodMeshes;
			Graphics::GraphicsBuffer subMeshes;
		};*/

		static constexpr VkDeviceSize stagingBufferSize{ 1 * 1024 * 1024 };
		Graphics::GraphicsBuffer stagingBuffer{};
		VkFence stagingBufferReuse;

		VkDescriptorPool geometryDescriptorPool;
		VkDescriptorSet geometryDescriptorSet;
		VkDescriptorSetLayout geometryDescriptorSetLayout;


		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;

		Graphics::GraphicsBuffer geometryBuffer{};
		U32 geometryBufferFreeOffset{ 0 };


		Graphics::GraphicsBuffer subMeshesBuffer{};

		std::vector<IndexedStaticMesh> meshes;
		std::vector<Animation::Skeleton> skeletons;
		Animation::AnimationDataSet animationDataSet;
		std::vector<Math::Matrix4x4> modelMatrices;


		// Unified Geometry Buffer
		Graphics::GraphicsBuffer geometryLookupTableBuffer{};
		Graphics::ComputePipeline lookupTableUpdatePipeline{};
		Graphics::PipelineLayout lookupTableUpdatePipelineLayout{};

		// Skinned Cache
		Graphics::GraphicsBuffer skinnedCacheBuffer{};
	};

} // namespace Framework