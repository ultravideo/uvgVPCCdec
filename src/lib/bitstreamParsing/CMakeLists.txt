message(DEBUG "./src/lib/bitstreamParsing/cmake")

add_library(bitstreamParsing STATIC
    bitstream_util.cpp
    sei.cpp
    vps.cpp
    atlas_context.cpp
    gof.cpp
    bitstreamParsing.cpp
)

target_compile_options(bitstreamParsing PRIVATE -fPIC)