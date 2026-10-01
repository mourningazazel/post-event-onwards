# Shared compiler settings, consumed via target_link_libraries(<t> PRIVATE peo::options).
add_library(peo_options INTERFACE)
add_library(peo::options ALIAS peo_options)

if(MSVC)
    target_compile_options(peo_options INTERFACE /W4 /permissive- /Zc:__cplusplus /utf-8)
    if(PEO_WARNINGS_AS_ERRORS)
        target_compile_options(peo_options INTERFACE /WX)
    endif()
else()
    target_compile_options(peo_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
        -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2)
    if(PEO_WARNINGS_AS_ERRORS)
        target_compile_options(peo_options INTERFACE -Werror)
    endif()
    if(PEO_ENABLE_SANITIZERS)
        # -O1: the sanitizers slow unoptimised code several-fold, and the headless suite
        # (this build) must stay under 1 s on CI (PEO-063, PEO-066). -O1 is the level the
        # ASan docs recommend; -Og left the town scenarios at ~1.1 s, -O1 runs ~0.3 s.
        target_compile_options(peo_options INTERFACE -O1 -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(peo_options INTERFACE -fsanitize=address,undefined)
    endif()
    if(PEO_ENABLE_TSAN)
        # PEO-080: the executor's threaded runs, checked for data races. TSan and ASan
        # cannot share a binary, hence its own preset (headless-tsan).
        target_compile_options(peo_options INTERFACE -O1 -g -fsanitize=thread)
        target_link_options(peo_options INTERFACE -fsanitize=thread)
    endif()
endif()
