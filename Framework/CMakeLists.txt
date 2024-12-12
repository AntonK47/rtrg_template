find_package(glslang CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

set(FRAMEWORK_NAME TemplateFramework)

add_library(${FRAMEWORK_NAME})
target_compile_features(${FRAMEWORK_NAME} PUBLIC cxx_std_23)

target_sources(
    ${FRAMEWORK_NAME}
PRIVATE 
	Utils.cpp
    Utils.hpp
	ImGuiUtils.hpp
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
)

