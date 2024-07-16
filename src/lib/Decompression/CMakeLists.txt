message(DEBUG "./src/lib/Decompression/cmake")

add_library(decompressionLibrary STATIC
    decompression.cpp
)

target_compile_options(decompressionLibrary PRIVATE -fPIC)
