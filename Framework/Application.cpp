#include "Application.hpp"

#include "Animation.hpp"
#include "BasicRenderPipeline.hpp"
#include "ImGuiUtils.hpp"
#include "MiniAssetImporterEditor.hpp"
#include "SDL3Utils.hpp"
#include "VulkanRHI.hpp"

#include "Memory.hpp"

using namespace Framework;
using namespace Framework::Animation;
using namespace Framework::Graphics;

using ViewportSize = glm::vec2;

namespace
{
	std::tuple<glm::vec2, bool> GetScreenSpacePosition(const ViewportSize& viewport, const glm::mat4& modelView,
													   const glm::mat4& projection, const glm::vec3& positionWS)
	{
		const auto positionViewSpace = modelView * glm::vec4{ positionWS, 1.0f };
		bool isVisible = true;
		if (positionViewSpace.z < 0.0f)
		{
			isVisible = false;
		}

		const auto positionClipSpace = projection * positionViewSpace;
		const auto positionNDC =
			glm::vec3{ positionClipSpace.x / positionClipSpace.w, positionClipSpace.y / positionClipSpace.w,
					   positionClipSpace.z / positionClipSpace.w };


		const auto screenPosition =
			glm::vec2{ (positionNDC.x + 1.0f) * 0.5f * viewport.x, (positionNDC.y + 1.0f) * 0.5f * viewport.y };

		return std::make_tuple(screenPosition, isVisible);
	}

	void UpdateCamera(Camera& camera)
	{
#pragma region Camera Controller

		auto& io = ImGui::GetIO();
		auto moveCameraFaster = false;
		const auto fastSpeed = 250.0f;
		moveCameraFaster = ImGui::IsKeyDown(ImGuiKey_LeftShift);

		if (ImGui::IsKeyDown(ImGuiKey_W))
		{
			camera.position += camera.forward * camera.movementSpeed * (moveCameraFaster ? fastSpeed : 1.0f) *
				io.DeltaTime * camera.movementSpeedScale;
		}
		if (ImGui::IsKeyDown(ImGuiKey_S))
		{
			camera.position -= camera.forward * camera.movementSpeed * (moveCameraFaster ? fastSpeed : 1.0f) *
				io.DeltaTime * camera.movementSpeedScale;
		}
		if (ImGui::IsKeyDown(ImGuiKey_A))
		{
			camera.position += glm::normalize(glm::cross(camera.forward, camera.up)) * camera.movementSpeed *
				io.DeltaTime * (moveCameraFaster ? fastSpeed : 1.0f) * camera.movementSpeedScale;
		}
		if (ImGui::IsKeyDown(ImGuiKey_D))
		{
			camera.position -= glm::normalize(glm::cross(camera.forward, camera.up)) * camera.movementSpeed *
				io.DeltaTime * (moveCameraFaster ? fastSpeed : 1.0f) * camera.movementSpeedScale;
		}

		if (ImGui::IsMouseDown(ImGuiMouseButton_Left) and not io.WantCaptureMouse)
		{
			auto delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0.0f);
			ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);

			const auto right = glm::normalize(cross(camera.forward, camera.up));
			const auto up = glm::normalize(cross(right, camera.forward));

			const auto f = glm::normalize(camera.forward + right * delta.y + up * delta.x);

			auto rotationAxis = glm::normalize(glm::cross(f, camera.forward));

			if (glm::length(rotationAxis) >= 0.1f)
			{
				const auto rotation =
					glm::rotate(glm::identity<glm::mat4>(),
								glm::radians(glm::length(glm::vec2(delta.x, delta.y)) * camera.sensitivity), f);

				camera.forward = glm::normalize(glm::vec3(rotation * glm::vec4(camera.forward, 0.0f)));
			}
		}
	}
} // namespace
void Framework::Application::Run()
{
	const auto applicationName = "Template Application";

	TracySetProgramName(applicationName);
#pragma region SDL window initialization
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		SDL_Log("SDL_Init failed (%s)", SDL_GetError());
		return;
	}


	SDL_Window* window = SDL_CreateWindow(applicationName, 1920, 1080,
										  SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE);
	// could be replaced with win32 window, see minimal example here
	// https://learn.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program?source=recommendations

	if (!window)
	{
		SDL_Log("SDL_CreateWindow failed (%s)", SDL_GetError());
		SDL_Quit();
		return;
	}
#pragma endregion

#pragma region Setup
	WindowViewport windowViewport{};
	windowViewport.UpdateSize(window);
	windowViewport.shouldRecreateWindowSizeDependedResources = false;

	auto vulkanContext = VulkanContext{};
	vulkanContext.Initialize(applicationName, window, windowViewport);

	auto basicRenderPipeline = BasicRenderPipeline{};
	basicRenderPipeline.Initialize(vulkanContext, windowViewport);

	auto guiSystem = GuiSystem{};
	guiSystem.Initialize(vulkanContext, window, basicRenderPipeline.imGuiPass);
#pragma endregion

#pragma region Scene preparation
	basicRenderPipeline.GetScene().Upload("Assets/Meshes/CesiumMan.glb", vulkanContext);
#pragma endregion

#pragma region Setup Camera
	auto camera = Camera{ .position = glm::vec3{ 0.0f, 0.0f, 0.0f },
						  .forward = glm::vec3{ 0.0f, 0.0f, 1.0f },
						  .up = glm::vec3{ 0.0f, 1.0f, 0.0f },
						  .movementSpeed = 0.01f,
						  .sensitivity = 0.2f };
#pragma endregion

	bool shouldRun = true;
	static std::vector<AnimationInstance> animationInstances;

	for (auto i = 0; i < basicRenderPipeline.GetScene().animationDataSet.animations.size(); i++)
	{
		const auto& animation = basicRenderPipeline.GetScene().animationDataSet.animations[i];
		animationInstances.push_back(
			AnimationInstance{ .data = basicRenderPipeline.GetScene().animationDataSet.animations[i],
							   .playbackRate = 1.00f,
							   .startTime = 0.0f,
							   .loop = true });
	}

	auto assetImporterEditor = Editor::AssetImporterEditor{};
	while (shouldRun)
	{
		ZoneScopedN("GameLoop Tick");
		{
			ZoneScopedN("Pool Window Events");
#pragma region Handle window events
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				ImGui_ImplSDL3_ProcessEvent(&event);
				if (event.type == SDL_EVENT_QUIT)
				{
					shouldRun = false;
					break;
				}
				if (event.type == SDL_EVENT_WINDOW_RESIZED or event.type == SDL_EVENT_WINDOW_MAXIMIZED or
					event.type == SDL_EVENT_WINDOW_SHOWN or event.type == SDL_EVENT_WINDOW_RESTORED)
				{
					windowViewport.UpdateSize(window);
				}
				if (event.type == SDL_EVENT_WINDOW_MINIMIZED or event.type == SDL_EVENT_WINDOW_HIDDEN)
				{
					windowViewport.Reset();
				}
			}

			if (not windowViewport.IsVisible())
			{
				continue;
			}
		}
#pragma endregion

#pragma region Recreate frame Dependent resources
		if (windowViewport.shouldRecreateWindowSizeDependedResources)
		{
			vulkanContext.WaitIdle();
			vulkanContext.RecreateSwapchain(windowViewport);
			basicRenderPipeline.basicGeometryPass.RecreateViewDependentResources(vulkanContext, windowViewport);
			windowViewport.shouldRecreateWindowSizeDependedResources = false;
		}
#pragma endregion
		{
			ZoneScopedN("Update");
#pragma region Update State
			guiSystem.NextFrame();
			UpdateCamera(camera);

			static float time = 0.0f;
			time += ImGui::GetIO().DeltaTime;

			static bool show_demo_window = true;
			ImGui::ShowDemoWindow(&show_demo_window);

			assetImporterEditor.Draw();

			static float animationTime = 0.0f;
			static bool useGlobalTimeInAnimation = true;
			static int selectedAnimation = 0;
			static float blendFactor = 0.0f;
			static bool enableDebugDraw = { false };


			ImGui::Begin("Editor");

			ImGui::SeparatorText("Materials");

			for (auto i = 0; i < basicRenderPipeline.basicGeometryPass.psoCache.size(); i++)
			{
				ImGui::PushID(i);
				ImGui::Text("pso_%i", i);
				ImGui::SameLine();
				if (ImGui::Button("Alter Material"))
				{
					std::string generatedCode = "void surface(in Geometry geometry, out vec4 color){{ color = "
												"vec4({},{},{},1.0f);}}";
					generatedCode = runtime_format(generatedCode, (std::rand() % 255) / 255.0f,
												   (std::rand() % 255) / 255.0f, (std::rand() % 255) / 255.0f);

					MaterialAsset myNewMaterial{ generatedCode };
					const auto pso = basicRenderPipeline.basicGeometryPass.CompileOpaqueMaterialPsoOnly(vulkanContext,
																										myNewMaterial);

					vulkanContext.WaitIdle();
					vulkanContext.DestroyGraphicsPipeline(basicRenderPipeline.basicGeometryPass.psoCache[i]);

					basicRenderPipeline.basicGeometryPass.psoCache[i] = pso;
				}
				ImGui::PopID();
			}


			ImGui::SeparatorText("Animations");

			ImGui::Checkbox("Enable Debug Draw", &enableDebugDraw);

			if (ImGui::BeginListBox("##animations_list_box",
									ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing())))
			{
				for (int n = 0; n < animationInstances.size(); n++)
				{
					bool is_selected = (selectedAnimation == n);
					ImGuiSelectableFlags flags = (selectedAnimation == n) ? ImGuiSelectableFlags_Highlight : 0;
					if (ImGui::Selectable(animationInstances[n].data.animationName.empty() ?
											  "[unnamed]" :
											  animationInstances[n].data.animationName.c_str(),
										  is_selected, flags))
					{
						selectedAnimation = n;
					}
					if (is_selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndListBox();
			}

			if (ImGui::Button("Reset Time"))
			{
				time = 0.0f;
			}

			ImGui::SliderFloat("Playback Rate", &animationInstances[selectedAnimation].playbackRate, 0.0f, 4.0f);
			ImGui::End();

			auto& scene = basicRenderPipeline.GetScene();
			const auto pose0 = SamplePose(scene.animationDataSet, animationInstances[selectedAnimation],
										  useGlobalTimeInAnimation ? time : animationTime);

			const auto pose1 = SamplePose(scene.animationDataSet,
										  animationInstances[std::min({ (int)animationInstances.size() - 1, 4 })],
										  useGlobalTimeInAnimation ? time : animationTime);

			const auto pose = BlendPose(pose0, pose1, blendFactor);

			auto jointMatrices = ComputeJointsMatrices(pose, scene.skeletons[0]);

			auto offsetMatrices = jointMatrices;
			ApplyBindPose(offsetMatrices, scene.skeletons[0]);

			basicRenderPipeline.frameData.UploadJointMatrices(offsetMatrices);

			if (enableDebugDraw)
			{
				auto model = glm::rotate(glm::identity<glm::mat4>(), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));

				const auto aspectRatio =
					static_cast<float>(windowViewport.width) / static_cast<float>(windowViewport.height);
				const auto projection = glm::perspective(glm::radians(60.0f), aspectRatio, 0.001f, 100.0f);
				const auto view = glm::lookAt(camera.position, camera.position + camera.forward, camera.up);

				auto& drawList = *ImGui::GetBackgroundDrawList();
				drawList.PushClipRectFullScreen();
				const auto debugDrawColor = IM_COL32(100, 100, 250, 255);

				/*if (isVisible)
				{
					auto p = ImVec2{ meshOriginPositionScreenSpace.x, meshOriginPositionScreenSpace.y };
					drawList.AddCircleFilled(p, 4.0f, debugDrawColor);
					drawList.AddText(ImVec2{ p.x - 40.0f, p.y + 16.0f }, debugDrawColor, "mesh");
				}*/

				const auto& skeleton = scene.skeletons.front();


				for (auto i = 0; i < skeleton.joints.size(); i++)
				{
					auto& joint = skeleton.joints[i];

					if (joint.parentIndex >= 0)
					{
						const auto p0 = glm::vec3{ jointMatrices[i][3] } * glm::vec3{ 1.0, -1.0f, 1.0f };
						const auto p1 =
							glm::vec3{ jointMatrices[joint.parentIndex][3] } * glm::vec3{ 1.0, -1.0f, 1.0f };
						const auto origin = glm::vec3{ 0.0f, 0.0f, 0.0f };

						const auto [p0screen, p0IsVisible] =
							GetScreenSpacePosition(glm::vec2{ windowViewport.width, windowViewport.height },
												   view * model * jointMatrices[i], projection, origin);

						const auto [p1screen, p1IsVisible] =
							GetScreenSpacePosition(glm::vec2{ windowViewport.width, windowViewport.height },
												   view * model * jointMatrices[joint.parentIndex], projection, origin);

						if (p0IsVisible and p1IsVisible)
						{
							drawList.AddLine(ImVec2{ p0screen.x, p0screen.y }, ImVec2{ p1screen.x, p1screen.y },
											 debugDrawColor, 3.0f);
							drawList.AddText(ImVec2{ (p1screen.x + p0screen.x) * 0.5f - 40.0f,
													 (p1screen.y + p0screen.y) * 0.5f + 16.0f },
											 debugDrawColor, joint.name.c_str());
						}
					}
				}
				drawList.PopClipRect();
			}
#pragma endregion
		}
#pragma region Render State
		basicRenderPipeline.Execute(vulkanContext, windowViewport, camera, ImGui::GetIO().DeltaTime);
#pragma endregion
		FrameMark;
	}
#pragma region Cleanup
	vulkanContext.WaitIdle();
	guiSystem.Deinitialize();
	basicRenderPipeline.Deinitialize(vulkanContext);
	vulkanContext.Deinitialize();
	SDL_DestroyWindow(window);
	SDL_Quit();
#pragma endregion

#ifdef RTRG_ENABLE_PROFILER
	tracy::GetProfiler().RequestShutdown();
	while (!tracy::GetProfiler().HasShutdownFinished())
	{
		// TODO: It will not stop when the sampling profiling is enabled
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
#endif // RTRG_ENABLE_PROFILER
}