# peo_gpu: the scent field on the GPU (ADR-0014, PEO-081), beside core. SDL_GPU on Vulkan
# (ADR-0001); the shaders are GLSL compiled to SPIR-V at build time and embedded.

set(PEO_GPU_SHADERS wave_pull wave_flow)
set(PEO_GPU_GENERATED ${CMAKE_CURRENT_BINARY_DIR}/generated)
set(PEO_GPU_HEADERS)
foreach(shader IN LISTS PEO_GPU_SHADERS)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${shader}.comp)
    set(spirv ${PEO_GPU_GENERATED}/${shader}.spv)
    set(header ${PEO_GPU_GENERATED}/${shader}_spv.hpp)
    if(PEO_GLSLC)
        set(compile ${PEO_GLSLC} -fshader-stage=compute -O ${source} -o ${spirv})
    else()
        set(compile ${PEO_GLSLANG} -V -S comp ${source} -o ${spirv})
    endif()
    add_custom_command(
        OUTPUT ${header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PEO_GPU_GENERATED}
        COMMAND ${compile}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${spirv} -DOUTPUT=${header} -DNAME=k_${shader}_spv
                -P ${CMAKE_SOURCE_DIR}/cmake/EmbedSpirv.cmake
        DEPENDS ${source} ${CMAKE_SOURCE_DIR}/cmake/EmbedSpirv.cmake
        COMMENT "SPIR-V: ${shader}.comp"
        VERBATIM)
    list(APPEND PEO_GPU_HEADERS ${header})
endforeach()

add_library(peo_gpu STATIC gpu_field.cpp ${PEO_GPU_HEADERS})
add_library(peo::gpu ALIAS peo_gpu)
target_include_directories(peo_gpu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} PRIVATE ${PEO_GPU_GENERATED})
target_link_libraries(peo_gpu PUBLIC peo::core SDL3::SDL3 PRIVATE peo::options)
