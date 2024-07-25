message(DEBUG "./src/lib/Adaptation/cmake")

add_library(adaptationLibrary STATIC
    adaptation.cpp
)

target_compile_options(adaptationLibrary PRIVATE -fPIC)