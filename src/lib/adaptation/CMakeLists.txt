message(DEBUG "./src/lib/adaptation/cmake")

add_library(adaptation STATIC
    adaptation.cpp
)

target_compile_options(adaptation PRIVATE -fPIC)