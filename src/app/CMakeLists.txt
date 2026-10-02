add_executable(peo main.cpp)
target_link_libraries(peo PRIVATE peo::core peo::options SDL3::SDL3)
# PEO-081: the scent field on the GPU, when peo_gpu was built (a shader compiler found).
if(TARGET peo::gpu)
    target_link_libraries(peo PRIVATE peo::gpu)
    target_compile_definitions(peo PRIVATE PEO_HAVE_GPU=1)
endif()
set_target_properties(peo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    WIN32_EXECUTABLE $<BOOL:${WIN32}>)
if(WIN32 AND TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET peo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_FILE_DIR:peo>)
endif()
