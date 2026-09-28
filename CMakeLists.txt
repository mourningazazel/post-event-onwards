cmake_minimum_required(VERSION 3.25)

project(post_event_onwards
    VERSION 0.1.0
    DESCRIPTION "Performance-heavy ASCII roguelike: scent-driven hordes, endless staged world"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options. The core simulation never depends on SDL so it can be built and
# tested headlessly (CI, cloud agent sessions). The SDL3 frontend is optional.
# ---------------------------------------------------------------------------
option(PEO_BUILD_FRONTEND "Build the SDL3 frontend (needs a display to run)" ON)
option(PEO_BUILD_TESTS "Build the headless core test suite" ON)
option(PEO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(PEO_ENABLE_SANITIZERS "Enable ASan/UBSan (Debug, GCC/Clang only)" OFF)
option(PEO_FETCH_SDL "Fetch SDL3 from source when no system package is found" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerOptions)   # defines peo::options interface target

# ---------------------------------------------------------------------------
# Core simulation library (no SDL, no I/O). Everything testable lives here.
# ---------------------------------------------------------------------------
add_subdirectory(src/core)

# ---------------------------------------------------------------------------
# Tests (doctest, vendored in third_party/ so no network is needed).
# ---------------------------------------------------------------------------
if(PEO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# SDL3 frontend.
# ---------------------------------------------------------------------------
if(PEO_BUILD_FRONTEND)
    include(FindOrFetchSDL3)
    add_subdirectory(src/app)
endif()
