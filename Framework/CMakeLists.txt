find_package(glslang CONFIG REQUIRED)

set(FRAMEWORK_NAME TemplateFramework)

add_library(${FRAMEWORK_NAME})
target_compile_features(${FRAMEWORK_NAME} PUBLIC cxx_std_23)

target_sources(
    ${FRAMEWORK_NAME}
PRIVATE 
	utils.cpp
    utils.hpp)

target_include_directories(${FRAMEWORK_NAME} PUBLIC /)

target_link_libraries(
	${FRAMEWORK_NAME} 
PRIVATE 
	glslang::glslang
	glslang::glslang-default-resource-limits
)

