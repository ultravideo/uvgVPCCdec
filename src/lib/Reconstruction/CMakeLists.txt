message(DEBUG "./src/lib/Reconstruction/cmake")

add_library(reconstructionLibrary STATIC
    reconstruction.cpp
)

target_compile_options(reconstructionLibrary PRIVATE -fPIC)