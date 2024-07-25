message(DEBUG "./src/lib/PostReconstruction/cmake")

add_library(postReconstructionLibrary STATIC
    postReconstruction.cpp
)

target_compile_options(postReconstructionLibrary PRIVATE -fPIC)