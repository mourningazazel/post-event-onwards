# peo::core — the simulation. No SDL, no filesystem, no globals. Deterministic
# given a seed so tests and replays are reproducible.
add_library(peo_core STATIC
    src/scent.cpp
    src/stage.cpp
    src/dead.cpp
    src/world.cpp)
add_library(peo::core ALIAS peo_core)

target_include_directories(peo_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(peo_core PUBLIC cxx_std_20)
target_link_libraries(peo_core PRIVATE peo::options)
