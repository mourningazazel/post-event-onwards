# PEO-081: the GPU field's tests, in their own binary (they link SDL3 and peo_gpu, which
# the core tests never do). They skip, saying so, on a machine with no Vulkan device.
add_executable(peo_gpu_tests ${CMAKE_SOURCE_DIR}/tests/main.cpp test_gpu_field.cpp)
target_link_libraries(peo_gpu_tests PRIVATE peo::gpu peo::options doctest)
add_test(NAME gpu COMMAND peo_gpu_tests --no-intro)
# Vulkan drivers keep allocations to process exit, which LeakSanitizer would report.
set_tests_properties(gpu PROPERTIES TIMEOUT 120 ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0")
