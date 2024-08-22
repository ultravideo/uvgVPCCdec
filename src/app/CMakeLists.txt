message(DEBUG "./src/app/cmake")

# Define the executable
add_executable(uvgVPCCdec
    app.cpp
    cli.cpp
)

set(CMAKE_CXX_CLANG_TIDY "")

target_link_libraries(uvgVPCCdec uvgvpccdec)

# Define installation destinations
install(TARGETS uvgVPCCdec DESTINATION bin)