message(DEBUG "./src/lib/mapDecoding/cmake")

set(LIB_SOURCES "mapDecoding.cpp")

find_package(PkgConfig REQUIRED)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET 
        libavcodec
        libavformat
        libavutil
    )
    message(STATUS "FFmpeg found: ${FFMPEG_VERSION}")
    set(LIB_SOURCES ${LIB_SOURCES} decoderFFmpeg.cpp)
    set(FFMPEG_LIBS PkgConfig::FFMPEG)
else()
    message(WARNING "FFmpeg not found, skipping decoderFFmpeg")
    set(FFMPEG_LIBS "")
endif()

# Define the library
add_library(mapDecodingLibrary STATIC
    ${LIB_SOURCES}
)

if(FFMPEG_LIBS)
    target_link_libraries(mapDecodingLibrary PUBLIC ${FFMPEG_LIBS})
endif()


target_include_directories(mapDecodingLibrary PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/include)
target_compile_options(mapDecodingLibrary PRIVATE -fPIC)