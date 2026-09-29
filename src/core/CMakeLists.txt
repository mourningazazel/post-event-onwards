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

# D-002 needs bit-identical floats across Debug/Release, compilers and CPUs. GCC and
# Clang default to -ffp-contract=fast when optimising, fusing a*b+c into an FMA on
# some builds and not others. PUBLIC: core headers are compiled into consumers too.
target_compile_options(peo_core PUBLIC
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-ffp-contract=off>
    $<$<CXX_COMPILER_ID:MSVC>:/fp:precise>)
