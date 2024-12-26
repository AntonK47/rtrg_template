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
			F32 time;
			F32 resolution[2];
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
			GraphicsPipeline pipeline{};
			PipelineLayout pipelineLayout{};

			VkImageView depthView{VK_NULL_HANDLE};
			VkImage depthImage{VK_NULL_HANDLE};
			VmaAllocation depthImageAllocation{VK_NULL_HANDLE};
			Format depthFormat{ Format::d32f };

			const VulkanContext* vulkanContext;

			std::vector<GraphicsPipeline> psoCache{};

			void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const Scene& scene,
						 const FrameData::PerFrameResources& frame, const Camera& camera,
						 const WindowViewport windowViewport, F32 deltaTime);

			void CreateViewDependentResources(const VulkanContext& context, const WindowViewport& windowViewport);
			void ReleaseViewDependentResources(const VulkanContext& context);
			void RecreateViewDependentResources(const VulkanContext& context, const WindowViewport& windowViewport);

			void CompileOpaqueMaterial(const VulkanContext& context, const MaterialAsset& materialAsset);
			GraphicsPipeline CompileOpaqueMaterialPsoOnly(const VulkanContext& context, const MaterialAsset& materialAsset);

			void CreateResources(const VulkanContext& context, Scene& scene, FrameData& frameData,
								 const WindowViewport& windowViewport);
			void ReleaseResources(const VulkanContext& context);
		};

		struct FullscreenQuadPass
		{
			GraphicsPipeline pipeline{};
			PipelineLayout pipelineLayout{};
			F32 time{ 0 };

			const VulkanContext* vulkanContext;

			void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const WindowViewport windowViewport,
						 F32 deltaTime);

			void CreateResources(const VulkanContext& context);
			void ReleaseResources(const VulkanContext& context);
		};
		struct ImGuiPass
		{
			VkDescriptorPool descriptorPool{};

			const VulkanContext* vulkanContext;

			void Execute(const VkCommandBuffer& cmd, VkImageView colorTarget, const WindowViewport windowViewport,
						 F32 deltaTime);

			void CreateResources(const VulkanContext& context);
			void ReleaseResources(const VulkanContext& context);
		};
	} // namespace Graphics
} // namespace Framework