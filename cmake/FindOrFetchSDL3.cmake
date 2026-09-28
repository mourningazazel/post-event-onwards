# Prefer an installed SDL3 (vcpkg, brew, distro package); otherwise build it from
# source with FetchContent. The pinned tag is the only place the SDL version lives.
set(PEO_SDL3_TAG "release-3.2.30" CACHE STRING "SDL3 git tag used when fetching from source")

find_package(SDL3 CONFIG QUIET)
if(SDL3_FOUND)
    message(STATUS "SDL3: using installed package ${SDL3_VERSION}")
    return()
endif()

if(NOT PEO_FETCH_SDL)
    message(FATAL_ERROR "SDL3 not found and PEO_FETCH_SDL is OFF. Install SDL3 or enable fetching.")
endif()

message(STATUS "SDL3: not installed, fetching ${PEO_SDL3_TAG} from source")
include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG ${PEO_SDL3_TAG}
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(SDL3)
