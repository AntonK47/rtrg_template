
find_package(GTest CONFIG REQUIRED)
enable_testing()
add_executable(utils_test)
target_compile_features(utils_test PUBLIC cxx_std_23)
target_sources(utils_test PRIVATE 
	utils_test.cpp)
target_link_libraries(utils_test
PRIVATE
	GTest::gtest_main
	TemplateFramework
)

set_property(TARGET utils_test PROPERTY FOLDER "test")
include(GoogleTest)
gtest_discover_tests(utils_test)