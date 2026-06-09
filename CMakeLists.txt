cmake_minimum_required(VERSION 3.25)
project(dsprites_viewer CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

# SDL3 (static). First configure downloads & builds SDL (~few minutes); cached after.
set(SDL_STATIC       ON  CACHE BOOL "" FORCE)
set(SDL_SHARED       OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(SDL3)

add_executable(dsprites-view
    src/main.cpp
    src/DSprites.cpp
)
target_include_directories(dsprites-view PRIVATE src)
target_link_libraries(dsprites-view PRIVATE SDL3::SDL3)
