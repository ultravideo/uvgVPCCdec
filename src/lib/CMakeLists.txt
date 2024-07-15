message(DEBUG "./src/lib/cmake")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

configure_file(version.cpp.in version.cpp)

add_subdirectory(utils)
add_subdirectory(BitstreamParsing)
add_subdirectory(FormatConversion)
add_subdirectory(PreReconstruction)
add_subdirectory(Reconstruction)
add_subdirectory(PostReconstruction)
add_subdirectory(Adaptation)

set(LIB_SOURCES
    version.cpp
    uvgvpccdec.cpp
)

if(BUILD_SHARED_LIBS)
  add_library(uvgvpccdec SHARED ${LIB_SOURCES})
else()
  add_library(uvgvpccdec STATIC ${LIB_SOURCES})
endif()

target_link_libraries(uvgvpccdec
    bitstreamParsingLibrary
    utilsLibrary
    logLibrary
)

# Include directories for headers
target_include_directories(uvgvpccdec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Optionally, add additional configurations, dependencies, or options
# target_compile_options(BitstreamGenerationLibrary PRIVATE -std=c++11)

# Define installation destinations
install(TARGETS uvgvpccdec DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION include)