find_package(glslang CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(stduuid CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(MPMCQueue CONFIG REQUIRED)

set(FRAMEWORK_NAME TemplateFramework)

add_library(${FRAMEWORK_NAME})
target_compile_features(${FRAMEWORK_NAME} PUBLIC cxx_std_23)

target_sources(
    ${FRAMEWORK_NAME}
PRIVATE
	Utils.cpp
    Utils.hpp
	ImGuiUtils.hpp
	ImGuiUtils.cpp
	VolkUtils.hpp
	VolkUtils.cpp
	VmaUtils.hpp
	VmaUtils.cpp
	SDL3Utils.hpp
	Application.hpp
	Application.cpp
	MeshImporter.hpp
	MeshImporter.cpp
	Animation.hpp
	Math.hpp
	Core.hpp
	VulkanRHI.hpp
    VulkanRHI.cpp
	$<$<CXX_COMPILER_ID:MSVC>:assimp.natvis>>
	MiniAssetImporterEditor.hpp
	AssetHelper.hpp
	Scene.hpp
	Scene.cpp
	GpuScene.hpp
	GpuScene.cpp
	BasicRenderPipeline.hpp
	BasicRenderPipeline.cpp
	RenderDevice.hpp
	RenderPasses.hpp
	RenderPasses.cpp
	Camera.hpp
	FrameData.hpp
	FrameData.cpp
	Memory.hpp
	Memory.cpp
	Profiler.hpp
	UnifiedGeometryBuffer.hpp
	UnifiedGeometryBuffer.cpp
 	StreamingSystem.hpp
 	StreamingSystem.cpp
	GpuUploader.hpp
	GpuUploader.cpp
	Pool.hpp
	AccelerationStructureBuilder.hpp
	AccelerationStructureBuilder.cpp
	UnwrapGeometryForAccelerationStructurePass.hpp
	UnwrapGeometryForAccelerationStructurePass.cpp
 )

target_include_directories(${FRAMEWORK_NAME} PUBLIC /)

target_link_libraries(
	${FRAMEWORK_NAME}
PRIVATE
	glslang::glslang
	glslang::glslang-default-resource-limits
	volk::volk_headers
	Vulkan::Headers
	SDL3::SDL3
	imgui::imgui
	assimp::assimp
	GPUOpen::VulkanMemoryAllocator
	glm::glm-header-only
	nlohmann_json::nlohmann_json
	stduuid
	imgui-node-editor::imgui-node-editor
	MPMCQueue::MPMCQueue
)

target_compile_definitions(${FRAMEWORK_NAME} PUBLIC UUID_SYSTEM_GENERATOR)


if(${RTRG_ENABLE_PROFILER})
	target_compile_definitions(${FRAMEWORK_NAME} PUBLIC RTRG_ENABLE_PROFILER TRACY_ENABLE RTRG_PROFILER_CALLSTACK_DEPTH=30 TRACY_IMPORTS)
	target_link_libraries(
		${FRAMEWORK_NAME}
	PRIVATE	
		Tracy::TracyClient
	)
endif()


if(${RTRG_ENABLE_GRAPHICS_VALIDATION})
	target_compile_definitions(${FRAMEWORK_NAME} PUBLIC RTRG_ENABLE_GRAPHICS_VALIDATION)
endif()