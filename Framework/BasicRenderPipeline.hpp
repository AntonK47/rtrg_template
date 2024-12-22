#pragma once

#include "FrameData.hpp"
#include "RenderPasses.hpp"
#include "Scene.hpp"
#include "VulkanRHI.hpp"

namespace Framework
{
	namespace Graphics
	{
		struct BasicRenderPipeline
		{
			void Initialize(const VulkanContext& context, const WindowViewport& windowViewport);
			void Deinitialize(const VulkanContext& context);
			void Execute(const VulkanContext& context, const WindowViewport& windowViewport, const Camera& camera,
						 Float deltaTime);

			Scene& GetScene()
			{
				return scene;
			}

			Scene scene;
			FrameData frameData;

			BasicGeometryPass basicGeometryPass;
			ImGuiPass imGuiPass;
			FullscreenQuadPass fullscreenQuadPass;

			U32 frameIndex{ 0 };
			Float time{ 0 };
		};
	} // namespace Graphics
} // namespace Framework