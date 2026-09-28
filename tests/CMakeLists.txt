# Headless core tests. Each test file is a doctest TEST_SUITE; all link into one
# binary so CTest can filter with -R and the whole suite stays fast (< 500 ms).
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

add_executable(peo_core_tests
    main.cpp
    core/test_rng.cpp
    core/test_grid.cpp
    core/test_scent.cpp
    core/test_stage.cpp
    core/test_horde.cpp)
target_link_libraries(peo_core_tests PRIVATE peo::core peo::options doctest)

add_test(NAME core COMMAND peo_core_tests --no-intro)
set_tests_properties(core PROPERTIES TIMEOUT 30)
