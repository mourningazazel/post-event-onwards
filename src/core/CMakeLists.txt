# peo::core — the simulation. No SDL, no filesystem, no globals. Deterministic
# given a seed so tests and replays are reproducible.
add_library(peo_core STATIC
    src/scent_wave.cpp
    src/wave_kernels_avx2.cpp
    src/turn_input.cpp
    src/stage.cpp
    src/dead.cpp
    src/world.cpp)
add_library(peo::core ALIAS peo_core)

# PEO-085: GCC and Clang mark the AVX2 copy of the scent wave's row kernels with a target
# attribute; MSVC has none, so it builds that one file for AVX2. ScentWave calls it only
# after a CPU check.
if(MSVC AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|x86_64)$")
    set_source_files_properties(src/wave_kernels_avx2.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX2)
endif()

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
