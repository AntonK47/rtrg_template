cmake_minimum_required(VERSION 3.29)

project(rtrg VERSION 0.1 LANGUAGES CXX)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
	foreach(RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Assets")
		list(APPEND commands
			COMMAND ${CMAKE_COMMAND} -E copy_directory 
                ${RESOURCE_DIR}
                "${CMAKE_CURRENT_BINARY_DIR}/Application/Assets")
	endforeach()
endif()

add_custom_target(CopyAssets ALL COMMAND ${commands})
set_property(TARGET CopyAssets PROPERTY FOLDER "utility")

add_subdirectory(Framework)
add_subdirectory(Application)
add_subdirectory(ThirdParty EXCLUDE_FROM_ALL TRUE)
add_subdirectory(Test)

if(MSVC)
	add_compile_options( /W4 /WX)
else()
	add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()