# Headless core tests. Each test file is a doctest TEST_SUITE; all link into one
# binary so CTest can filter with -R. The time budgets are stated in AGENTS.md and
# enforced by the suite_budget and scenario_budget tests below (D-033).
add_library(doctest INTERFACE)
target_include_directories(doctest SYSTEM INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

add_executable(peo_core_tests
    main.cpp
    core/test_rng.cpp
    core/test_grid.cpp
    core/test_stage.cpp
    core/test_dead.cpp
    core/test_world.cpp
    core/test_town.cpp
    core/test_scent_wave.cpp
    core/test_turn_input.cpp
    core/test_executor.cpp
    core/test_swarm.cpp
    core/test_save.cpp
    core/test_revisit.cpp
    core/test_noise.cpp
    core/test_geography.cpp
    core/test_settlement_site.cpp
    core/test_camera.cpp
    core/test_hud.cpp
    core/test_glyph_atlas.cpp
    core/test_sight.cpp
    core/test_look.cpp
    core/test_memory.cpp
    core/test_peek.cpp
    core/test_content.cpp
    core/test_content_migration.cpp
    core/test_look_label.cpp
    core/test_peo_format.cpp
    core/test_item_store.cpp
    core/test_chunk.cpp
    siege/test_siege_layouts.cpp)
target_link_libraries(peo_core_tests PRIVATE peo::core peo::options doctest)
# PEO-080: the executor tests also run the frontend's thread pool (header-only, no SDL).
target_include_directories(peo_core_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
# PEO-092: the scenario case that migrates against the real content/.
target_compile_definitions(peo_core_tests PRIVATE PEO_CONTENT_DIR="${CMAKE_SOURCE_DIR}/content")
find_package(Threads REQUIRED)
target_link_libraries(peo_core_tests PRIVATE Threads::Threads)

# PEO-088: the siege suite's runner. Built with everything else so it never rots, but no
# test runs it: it runs on demand (docs/testing.md), and in a manual CI workflow.
add_executable(peo_siege siege/siege.cpp)
target_link_libraries(peo_siege PRIVATE peo::core peo::options Threads::Threads)
target_include_directories(peo_siege PRIVATE ${CMAKE_SOURCE_DIR}/src/app)
target_compile_definitions(peo_siege PRIVATE PEO_SIEGE_BASELINE_DIR="${CMAKE_SOURCE_DIR}/tests/siege/baseline")

# PEO-095: draws geography squares as an image for review (docs/testing.md). Built with
# everything else so it never rots; no test runs it.
add_executable(peo_geo geo/geo_dump.cpp)
target_link_libraries(peo_geo PRIVATE peo::core peo::options)

# PEO-008: prints every resolved item for the content_golden diff against derive.py. Built
# with everything else so it never rots; content_golden runs it.
add_executable(peo_content_dump content_dump/content_dump.cpp)
target_link_libraries(peo_content_dump PRIVATE peo::core peo::options)

# PEO-135: prints parsed .peo files as canonical lines for the peo_conformance diff against
# tools/content/peo.py. Built with everything else so it never rots.
add_executable(peo_peo_dump peo_dump/peo_dump.cpp)
target_link_libraries(peo_peo_dump PRIVATE peo::core peo::options)

add_test(NAME core COMMAND peo_core_tests --no-intro)
set_tests_properties(core PROPERTIES TIMEOUT 30)

# PEO-033, D-033: fail when a tier's summed case times exceed its budget in AGENTS.md,
# naming the slowest cases: the unit tier (everything but the scenario* suites) and the
# scenario tier. Skipped (with a message) only if Python is missing.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(NAME suite_budget
             COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/suite_budget.py
                     $<TARGET_FILE:peo_core_tests> --tier unit --agents ${CMAKE_SOURCE_DIR}/AGENTS.md)
    set_tests_properties(suite_budget PROPERTIES TIMEOUT 60)
    add_test(NAME scenario_budget
             COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/suite_budget.py
                     $<TARGET_FILE:peo_core_tests> --tier scenario --agents ${CMAKE_SOURCE_DIR}/AGENTS.md)
    set_tests_properties(scenario_budget PROPERTIES TIMEOUT 120)
    # PEO-008: every item in content/ resolves in the engine exactly as derive.py resolves it.
    add_test(NAME content_golden
             COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/content/golden.py
                     --engine $<TARGET_FILE:peo_content_dump> --content ${CMAKE_SOURCE_DIR}/content)
    set_tests_properties(content_golden PROPERTIES TIMEOUT 60)
    # PEO-135: the engine's .peo parser gives every tests/content/peo case its .expect lines.
    add_test(NAME peo_conformance
             COMMAND Python3::Interpreter ${CMAKE_SOURCE_DIR}/tools/content/peo_conformance.py
                     --engine $<TARGET_FILE:peo_peo_dump>)
    set_tests_properties(peo_conformance PROPERTIES TIMEOUT 60)
else()
    message(WARNING "Python3 not found: the suite_budget, content_golden and peo_conformance tests are not registered")
endif()
