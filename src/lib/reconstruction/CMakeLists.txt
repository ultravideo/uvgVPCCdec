message(DEBUG "./src/lib/reconstruction/cmake")

add_library(reconstruction STATIC
    reconstruction.cpp
)

target_compile_options(reconstruction PRIVATE -fPIC)