message(DEBUG "./src/app/cmake")

# Take care of external dependencies (e.g. uvgV3CRTP)
include(dependencies/FindDependencies.cmake)

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

find_package(cppzmq CONFIG REQUIRED)
target_link_libraries(uvgVPCCdec PRIVATE cppzmq)

# Add uvgV3CRTP to link targets if enabled
if(ENABLE_V3CRTP)
    target_link_libraries(uvgVPCCdec PRIVATE uvgv3crtp)
    # Include define to uvgVPCCdec so uvgV3CRTP code is included
    target_compile_definitions(uvgVPCCdec PRIVATE ENABLE_V3CRTP)
endif()

# Define installation destinations
install(TARGETS uvgVPCCdec DESTINATION bin)