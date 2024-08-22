message(DEBUG "./src/app/cmake")

# Define the executable
add_executable(uvgVPCCdec
    app.cpp
    cli.cpp
)

set(CMAKE_CXX_CLANG_TIDY "")

find_package(PkgConfig REQUIRED)
pkg_check_modules(UVGRTP REQUIRED uvgrtp)

include_directories(${UVGRTP_INCLUDE_DIRS})
link_directories(${UVGRTP_LIBRARY_DIRS})

target_link_libraries(uvgVPCCdec uvgvpccdec ${UVGRTP_LIBRARIES})

# Define installation destinations
install(TARGETS uvgVPCCdec DESTINATION bin)