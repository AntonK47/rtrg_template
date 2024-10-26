
find_package(GTest CONFIG REQUIRED)
enable_testing()
add_executable(Utils_test)
target_compile_features(Utils_test PUBLIC cxx_std_23)
target_sources(
	Utils_test
PRIVATE 
	Utils_test.cpp
)
target_link_libraries(Utils_test
PRIVATE
	GTest::gtest_main
	TemplateFramework
)

set_property(TARGET Utils_test PROPERTY FOLDER "test")
include(GoogleTest)
gtest_discover_tests(Utils_test)