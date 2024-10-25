include(FetchContent)

set(SDL_STATIC TRUE)

FetchContent_Declare(
    SDL
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main  # Replace this with a particular git tag or git hash
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
message(STATUS "Using SDL3 via FetchContent")
FetchContent_MakeAvailable(SDL)
set_property(TARGET SDL3-shared SDL3-static SDL3_test SDL_uclibc uninstall PROPERTY FOLDER "thirdParty/sdl3")