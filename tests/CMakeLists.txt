# Headless core tests. Each test file is a doctest TEST_SUITE; all link into one
# binary so CTest can filter with -R. The time budget is stated in AGENTS.md and
# enforced by the suite_budget test below.
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

add_executable(peo_core_tests
    main.cpp
    core/test_rng.cpp
    core/test_grid.cpp
    core/test_scent.cpp
    core/test_stage.cpp
    core/test_dead.cpp
    core/test_world.cpp
    core/test_town.cpp
    core/test_scent_wave.cpp
    core/test_turn_input.cpp)
target_link_libraries(peo_core_tests PRIVATE peo::core peo::options doctest)

add_test(NAME core COMMAND peo_core_tests --no-intro)
set_tests_properties(core PROPERTIES TIMEOUT 30)

# PEO-033: fail when the suite's summed case times exceed the budget in AGENTS.md,
# naming the slowest cases. Skipped (with a message) only if Python is missing.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME suite_budget
             COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/suite_budget.py
                     $<TARGET_FILE:peo_core_tests> --agents ${CMAKE_SOURCE_DIR}/AGENTS.md)
    set_tests_properties(suite_budget PROPERTIES TIMEOUT 60)
else()
    message(WARNING "Python3 not found: the suite_budget test is not registered")
endif()
