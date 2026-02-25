message(DEBUG "./src/app/cmake")

add_library(miniplyLibrary STATIC
    extras/miniply.cpp
)

add_library(getoptLibrary STATIC
    extras/getopt.c
)

set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_DEFAULT}")

# Define the executable
add_executable(uvgVPCCdec
    uvgVPCCdecAppExample.cpp
    cli.cpp
)

find_package(OpenMP REQUIRED)
target_link_libraries(uvgVPCCdec PRIVATE OpenMP::OpenMP_CXX)


set(CMAKE_CXX_CLANG_TIDY "")

target_compile_options(miniplyLibrary PRIVATE -fPIC)
target_compile_options(getoptLibrary PRIVATE -fPIC)

target_link_libraries(uvgVPCCdec PRIVATE uvgvpccdec)

# Define installation destinations
install(TARGETS uvgVPCCdec DESTINATION bin)