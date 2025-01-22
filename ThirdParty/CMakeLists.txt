include(FetchContent)

#set(SDL_STATIC TRUE)

#FetchContent_Declare(
#    SDL
#    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
#    GIT_TAG main
#    GIT_SHALLOW TRUE
#    GIT_PROGRESS TRUE
#)
#message(STATUS "Using SDL3 via FetchContent")
#FetchContent_MakeAvailable(SDL)
#set_property(TARGET SDL3-shared SDL3-static SDL3_test SDL_uclibc uninstall PROPERTY FOLDER "thirdParty/sdl3")


find_package(volk CONFIG REQUIRED)
find_package(VulkanHeaders CONFIG)
find_package(SDL3 CONFIG REQUIRED)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG master
)

FetchContent_MakeAvailable(imgui)
add_library(imgui)
add_library(imgui::imgui ALIAS imgui)
target_sources(imgui 
PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
        
target_link_libraries(
    imgui
PRIVATE
    volk::volk_headers
	Vulkan::Headers
    SDL3::SDL3
)

target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_USE_VOLK)

target_include_directories(imgui
PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

set_target_properties(imgui PROPERTIES FOLDER "thirdParty/imgui")


FetchContent_Declare(imgui-node-editor
    GIT_REPOSITORY https://github.com/thedmd/imgui-node-editor.git
    GIT_TAG develop
)

FetchContent_MakeAvailable(imgui-node-editor)
add_library(imgui-node-editor)
add_library(imgui-node-editor::imgui-node-editor ALIAS imgui-node-editor)
target_sources(imgui-node-editor 
PRIVATE
    ${imgui-node-editor_SOURCE_DIR}/crude_json.cpp
    ${imgui-node-editor_SOURCE_DIR}/imgui_canvas.cpp
    ${imgui-node-editor_SOURCE_DIR}/imgui_node_editor.cpp
    ${imgui-node-editor_SOURCE_DIR}/imgui_node_editor_api.cpp
)
        
target_link_libraries(
    imgui-node-editor
PRIVATE
    imgui
)
target_include_directories(imgui
PUBLIC
    ${imgui-node-editor_SOURCE_DIR}
)


set_target_properties(imgui-node-editor PROPERTIES FOLDER "thirdParty/imgui-node-editor")