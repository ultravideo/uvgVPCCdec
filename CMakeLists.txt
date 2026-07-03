

cmake_minimum_required(VERSION 3.22)

# Include CMake modules

set(uvgVPCCdec_VER 0.1.0)
set(uvgVPCCdec_DESCR "uvgVPCCdec is an Real-Time V-PCC encoder library written in C++.")
set(uvgVPCCdec_URL "https://github.com/ultravideo/uvgVPCCdec")

option(BUILD_SHARED_LIBS "Build using shared uvgVPCCdec library" ON)

# Control if uvgVPCCdec should be build with RTP support
option(ENABLE_V3CRTP "Enable RTP support" OFF)

project(uvgVPCCdec VERSION ${uvgVPCCdec_VER})

#set(CMAKE_BUILD_TYPE Debug)

# if(MSVC)
# add_compile_options(/W3 /WX)
# else()
# add_compile_options(-W -Wall -Werror -Wno-unused-variable)
# endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

if($ENV{USE_CLANG_TIDY})
  set(CMAKE_CXX_CLANG_TIDY "clang-tidy;-checks=*,-llvmlibc-*,-llvm-*,-modernize-*,-fuchsia-*,-altera-*,-hicpp-use-auto,-bugprone-easily-swappable-parameters,-performance-avoid-endl,-cppcoreguidelines-pro-bounds-constant-array-index,-cppcoreguidelines-avoid-magic-numbers,-readability-magic-numbers,-readability-identifier-length")
endif()

# Usefull for debuging
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -fsanitize=undefined -fsanitize=leak")
endif()

# Run subdirectory make files
add_subdirectory(src/lib)
add_subdirectory(src/app)
# add_subdirectory(examples)

# Custom target to apply formating to all source file
# todo : check if user has the package : sudo apt-get install clang-format
add_custom_target(format
  find
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    -type f
    (
    -name "*.cc"
    -o
    -name "*.cpp"
    -o
    -name "*.h"
    -o
    -name "*.hpp"
    )
    -print0
  | xargs -0 clang-format -i
  COMMAND
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Formatting source code..."
  VERBATIM
)

# INSTALL
include(GNUInstallDirs)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/lib/include/uvgvpcc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# # Find Doxygen package
# find_package(Doxygen)

# # Check if Doxygen is found
# if(DOXYGEN_FOUND)
#     # Set input and output directories
#     set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
#     set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

#     # Configure Doxygen file
#     configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

#     # Add a target to generate the API documentation
#     add_custom_target(docs
#         COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
#         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
#         COMMENT "Generating API documentation with Doxygen"
#         VERBATIM
#     )
# endif()
