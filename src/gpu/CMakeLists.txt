# peo_gpu: the scent field on the GPU (ADR-0014, PEO-081), beside core. SDL_GPU on Vulkan
# (ADR-0001); the shaders are GLSL compiled to SPIR-V at build time and embedded.

set(PEO_GPU_SHADERS wave_pull wave_flow wave_fused)
# PEO-087: the fused shader's block width, in cells (a divisor of the 128-cell pull tile),
# shared by the shader and gpu_field.cpp. Measured on the M1, windy saturated 2048x2048,
# fused update: 32 wide 13.7 ms, 64 wide 14.4 ms (12.7 ms once each round computes only its
# exact cells), 128 wide 24.6 ms; 64 kept, 32 is within noise and 128 overruns occupancy.
set(PEO_GPU_FUSED_BLOCK_W 64 CACHE STRING "Fused GPU pull block width (32, 64 or 128)")
set(PEO_GPU_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(PEO_GPU_HEADERS)
foreach(shader IN LISTS PEO_GPU_SHADERS)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.comp)
    set(spirv ${PEO_GPU_GENERATED}/${shader}.spv)
    set(header ${PEO_GPU_GENERATED}/${shader}_spv.hpp)
    if(PEO_GLSLC)
        set(compile ${PEO_GLSLC} -fshader-stage=compute -O -DBLOCK_W=${PEO_GPU_FUSED_BLOCK_W} ${source} -o
                    ${spirv})
    else()
        set(compile ${PEO_GLSLANG} -V -S comp -DBLOCK_W=${PEO_GPU_FUSED_BLOCK_W} ${source} -o ${spirv})
    endif()
    add_custom_command(
        OUTPUT ${header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PEO_GPU_GENERATED}
        COMMAND ${compile}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${spirv} -DOUTPUT=${header} -DNAME=k_${shader}_spv
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedSpirv.cmake
        DEPENDS ${source} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/wave_common.glsl ${CMAKE_SOURCE_DIR}/cmake/EmbedSpirv.cmake
        COMMENT "SPIR-V: ${shader}.comp"
        VERBATIM)
    list(APPEND PEO_GPU_HEADERS ${header})
endforeach()

add_library(peo_gpu STATIC gpu_field.cpp ${PEO_GPU_HEADERS})
add_library(peo::gpu ALIAS peo_gpu)
target_include_directories(peo_gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${PEO_GPU_GENERATED})
target_link_libraries(peo_gpu PUBLIC peo::core SDL3::SDL3 PRIVATE peo::options)
target_compile_definitions(peo_gpu PRIVATE PEO_GPU_FUSED_BLOCK_W=${PEO_GPU_FUSED_BLOCK_W})
