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


	struct Scene
	{
		void CreateResources(const Graphics::VulkanContext& context);
		void ReleaseResources(const Graphics::VulkanContext& context);

		void Upload(const std::string_view mesh, const Graphics::VulkanContext& context);
		void SetTlas(const Graphics::VulkanContext& context);

		void AddModel(const IndexedStaticMesh& mesh, const Math::Matrix4x4& transform);


#pragma region Upload
		static constexpr VkDeviceSize stagingBufferSize{ 1 * 1024 * 1024 };
		Graphics::GraphicsBuffer stagingBuffer{};
		VkFence stagingBufferReuse;
		VkCommandPool commandPool;
		VkCommandBuffer commandBuffer;
#pragma endregion

		VkDescriptorPool geometryDescriptorPool;
		VkDescriptorSet geometryDescriptorSet;
		Graphics::BindGroupLayout geometryBindGroupLayout;

		VkDescriptorSet accelerationStructureDescriptorSet;
		Graphics::BindGroupLayout  accelerationStructureBindGroupLayout;

		Graphics::GraphicsBuffer geometryBuffer{};
		U32 geometryBufferFreeOffset{ 0 };

		Graphics::GraphicsBuffer subMeshesBuffer{};

		std::vector<IndexedStaticMesh> meshes;
		std::vector<Animation::Skeleton> skeletons;
		Animation::AnimationDataSet animationDataSet;
		std::vector<Math::Matrix4x4> modelMatrices;


		// Skinned Cache
		Graphics::GraphicsBuffer skinnedCacheBuffer{};
	};

} // namespace Framework