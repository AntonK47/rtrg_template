#pragma once

#include "Camera.hpp"
#include "FrameData.hpp"
#include "Scene.hpp"
#include "VulkanRHI.hpp"

namespace Framework
{
	namespace Graphics
	{
		struct MaterialAsset
		{
			std::string surfaceShadingCode;
		};


		struct ShaderToyConstant
		{
			Float time;
			Float resolution[2];
		};

		struct ConstantsData
		{
			Math::Matrix4x4 viewProjection;
			Math::Matrix4x4 view;
			Math::Matrix4x4 model;
			Math::Vector3 viewPositionWS;
		};

		struct BasicGeometryPass
		{
			VkPipeline pipeline{VK_NULL_HANDLE};
			VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
			VkPipelineRenderingCreateInfo pipelineRendering{};


			VkImageView depthView{VK_NULL_HANDLE};
			VkImage depthImage{VK_NULL_HANDLE};
			VmaAllocation depthImageAllocation{VK_NULL_HANDLE};
			VkFormat depthFormat{ VK_FORMAT_D32_SFLOAT };

			std::vector<VkPipeline> psoCache{};

			void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const Scene& scene,
						 const FrameData::PerFrameResources& frame, const Camera& camera,
						 const WindowViewport windowViewport, Float deltaTime);

			void CreateViewDependentResources(const VulkanContext& context, const WindowViewport& windowViewport);
			void ReleaseViewDependentResources(const VulkanContext& context);
			void RecreateViewDependentResources(const VulkanContext& context, const WindowViewport& windowViewport);

			void CompileOpaqueMaterial(const VulkanContext& context, const MaterialAsset& materialAsset);
			VkPipeline CompileOpaqueMaterialPsoOnly(const VulkanContext& context, const MaterialAsset& materialAsset);

			void CreateResources(const VulkanContext& context, Scene& scene, FrameData& frameData,
								 const WindowViewport& windowViewport);
			void ReleaseResources(const VulkanContext& context);
		};

		struct FullscreenQuadPass
		{
			VkPipeline pipeline{};
			VkPipelineLayout pipelineLayout{};
			Float time{ 0 };

			void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const WindowViewport windowViewport,
						 Float deltaTime);

			void CreateResources(const VulkanContext& context);
			void ReleaseResources(const VulkanContext& context);
		};
		struct ImGuiPass
		{
			VkDescriptorPool descriptorPool{};

			void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const WindowViewport windowViewport,
						 Float deltaTime);

			void CreateResources(const VulkanContext& context);
			void ReleaseResources(const VulkanContext& context);
		};
	} // namespace Graphics
} // namespace Framework