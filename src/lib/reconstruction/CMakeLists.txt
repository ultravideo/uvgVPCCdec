message(DEBUG "./src/lib/reconstruction/cmake")

add_library(reconstruction STATIC
    pccKdTree.cpp
    geometry_smoothing.cpp
    reconstruction.cpp
)

target_compile_options(reconstruction PRIVATE -fPIC)