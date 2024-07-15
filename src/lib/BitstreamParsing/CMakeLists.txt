message(DEBUG "./src/lib/BitstreamParsing/cmake")

add_library(bitstreamParsingLibrary STATIC
    bitstreamParsing.cpp
)

target_compile_options(bitstreamParsingLibrary PRIVATE -fPIC)
