
find_package(GTest CONFIG REQUIRED)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/TestData")
	foreach(RESOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/TestData")
		list(APPEND commands
			COMMAND ${CMAKE_COMMAND} -E copy_directory 
                ${RESOURCE_DIR}
                "${CMAKE_CURRENT_BINARY_DIR}")
	endforeach()
endif()

add_custom_target(CopyTestData ALL COMMAND ${commands})
set_property(TARGET CopyTestData PROPERTY FOLDER "utility")

add_executable(Framework_test)
target_compile_features(Framework_test PUBLIC cxx_std_23)
target_sources(
	Framework_test
PRIVATE 
	Utils_test.cpp
	MeshImporter_test.cpp)
target_link_libraries(Framework_test
PRIVATE
	GTest::gtest_main
	TemplateFramework
)

set_property(TARGET Framework_test PROPERTY FOLDER "test")
include(GoogleTest)
gtest_discover_tests(Framework_test)


add_dependencies(Framework_test CopyTestData)