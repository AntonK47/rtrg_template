set(APPLICATION_NAME "TemplateApplication")

find_package(SDL3 CONFIG REQUIRED)

add_executable(${APPLICATION_NAME})
target_compile_features(${APPLICATION_NAME} PUBLIC cxx_std_23)
target_sources(${APPLICATION_NAME} PRIVATE 
	main.cpp)
target_link_libraries(
	${APPLICATION_NAME} 
PRIVATE 
	SDL3::SDL3
    TemplateFramework
)

if(WIN32)
	target_link_options(${APPLICATION_NAME}  PRIVATE "/SUBSYSTEM:WINDOWS" "/ENTRY:mainCRTStartup")
endif()


add_dependencies(${APPLICATION_NAME} CopyAssets)