cmake_minimum_required(VERSION 3.29)

project(rtrg VERSION 0.1 LANGUAGES CXX)


add_subdirectory(Framework)
add_subdirectory(Application)
add_subdirectory(ThirdParty EXCLUDE_FROM_ALL TRUE)
add_subdirectory(Test)

if(MSVC)
	add_compile_options( /W4 /WX)
else()
	add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()