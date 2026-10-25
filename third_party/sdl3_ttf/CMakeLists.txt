include(FetchContent)

set(BUILD_SHARED_LIBS OFF)
set(SDLTTF_VENDORED ON)
set(SDLTTF_INSTALL OFF)
set(SDLTTF_SAMPLES OFF)

FetchContent_Declare(SDL3_ttf
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL_ttf"
    GIT_TAG "release-3.2.2"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SDL3_ttf)

include("${CMAKE_CURRENT_LIST_DIR}/prefer_outline_fonts.cmake")
FetchContent_GetProperties(SDL3_ttf SOURCE_DIR arcanum_sdl_ttf_source)
arcanum_prefer_outline_fonts("${arcanum_sdl_ttf_source}/src/SDL_ttf.c")
