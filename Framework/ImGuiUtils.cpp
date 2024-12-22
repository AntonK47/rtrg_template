#include "ImGuiUtils.hpp"

using namespace Framework;

void Framework::GuiSystem::Initialize(const Graphics::VulkanContext& context, SDL_Window* window,
									  const Graphics::ImGuiPass& imGuiPass)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	float dpiScale = SDL_GetWindowDisplayScale(window);
	io.FontGlobalScale = dpiScale;

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

	ImGui::StyleColorsDark();

	const auto pipelineRendering =
		VkPipelineRenderingCreateInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
									   .pNext = nullptr,
									   .viewMask = 0,
									   .colorAttachmentCount = 1,
									   .pColorAttachmentFormats = &context.swapchainImageFormat,
									   .depthAttachmentFormat = VK_FORMAT_UNDEFINED,
									   .stencilAttachmentFormat = VK_FORMAT_UNDEFINED };

	auto initInfo = ImGui_ImplVulkan_InitInfo{ .Instance = context.instance,
											   .PhysicalDevice = context.physicalDevice,
											   .Device = context.device,
											   .QueueFamily = context.graphicsQueueFamilyIndex,
											   .Queue = context.graphicsQueue,
											   .DescriptorPool = imGuiPass.descriptorPool,
											   .MinImageCount = context.swapchainImageCount,
											   .ImageCount = context.swapchainImageCount,
											   .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
											   .UseDynamicRendering = true,
											   .PipelineRenderingCreateInfo = pipelineRendering,
											   .Allocator = nullptr };


	ImGui_ImplVulkan_Init(&initInfo);
	ImGui_ImplSDL3_InitForVulkan(window);
}

void Framework::GuiSystem::Deinitialize()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
}

void Framework::GuiSystem::NextFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
}
