message(DEBUG "./src/lib/cmake")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

configure_file(version.cpp.in version.cpp)

add_subdirectory(utils)
add_subdirectory(Decompression)
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

# Find and link libavcodec using pkg-config
# find_package(PkgConfig REQUIRED)
# pkg_check_modules(AVCODEC REQUIRED libavcodec)
# find_package(avcodec CONFIG REQUIRED)
set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-Bsymbolic")
# find_package(FFMPEG REQUIRED)


find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET 
        libavcodec
        libavformat
        libavutil
    )
    message(STATUS "FFmpeg found: ${FFMPEG_VERSION}")
    # set(LIB_SOURCES ${LIB_SOURCES} encoderFFmpeg.cpp)
    set(FFMPEG_LIBS PkgConfig::FFMPEG)
else()
    message(WARNING "FFmpeg not found, skipping encoderFFmpeg")
    set(FFMPEG_LIBS "")
endif()



# include_directories(${FFMPEG_INCLUDE_DIRS})
# link_directories(${FFMPEG_LIBRARY_DIRS})


# if(FFMPEG_LIBS)
#     target_link_libraries(uvgvpccdec PUBLIC ${FFMPEG_LIBS})
# endif()


target_link_libraries(uvgvpccdec
    decompressionLibrary
    reconstructionLibrary
    postReconstructionLibrary
    adaptationLibrary
    utilsLibrary
    logLibrary
    ${FFMPEG_LIBS}
    # ${FFMPEG_LIBRARIES}
)

# Include directories for headers
target_include_directories(uvgvpccdec PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Optionally, add additional configurations, dependencies, or options
# target_compile_options(BitstreamGenerationLibrary PRIVATE -std=c++11)

# Define installation destinations
install(TARGETS uvgvpccdec DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION include)