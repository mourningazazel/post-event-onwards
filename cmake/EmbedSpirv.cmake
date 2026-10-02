# PEO-081: turn a compiled SPIR-V file into a header holding it as a byte array, so the
# GPU library carries its shaders and reads no file at run time.
#   cmake -DINPUT=<file.spv> -DOUTPUT=<file.hpp> -DNAME=<identifier> -P EmbedSpirv.cmake
file(READ "${INPUT}" bytes HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${bytes}")
file(WRITE "${OUTPUT}"
    "#pragma once\n// Generated from ${INPUT} by cmake/EmbedSpirv.cmake: do not edit.\n"
    "inline constexpr unsigned char ${NAME}[] = {${bytes}};\n")
