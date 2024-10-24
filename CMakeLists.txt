cmake_minimum_required(VERSION 3.29)

project(rtrg VERSION 0.1 LANGUAGES CXX)


find_package(glslang CONFIG REQUIRED)

add_subdirectory(thirdParty EXCLUDE_FROM_ALL TRUE)

set(APPLICATION_NAME "TemplateApplication")

add_executable(${APPLICATION_NAME})
target_compile_features(${APPLICATION_NAME} PUBLIC cxx_std_23)
target_sources(${APPLICATION_NAME} PRIVATE 
	main.cpp
	utils.hpp
	utils.cpp)
target_link_libraries(
	${APPLICATION_NAME} 
PRIVATE 
	SDL3::SDL3-static
	glslang::glslang
	glslang::glslang-default-resource-limits
)

if(WIN32)
	target_link_options(${APPLICATION_NAME}  PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()

if(MSVC)
  target_compile_options(${APPLICATION_NAME} PRIVATE /W4 /WX)
else()
  target_compile_options(${APPLICATION_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()



find_package(GTest CONFIG REQUIRED)
enable_testing()
add_executable(utils_test)
target_compile_features(utils_test PUBLIC cxx_std_23)
target_sources(utils_test PRIVATE 
	utils_test.cpp
	utils.cpp
	utils.hpp)
target_link_libraries(utils_test
PRIVATE
	GTest::gtest_main
	glslang::glslang
	glslang::glslang-default-resource-limits
)

set_property(TARGET utils_test PROPERTY FOLDER "test")
include(GoogleTest)
gtest_discover_tests(utils_test)