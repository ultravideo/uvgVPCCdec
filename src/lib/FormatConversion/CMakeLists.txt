message(DEBUG "./src/lib/FormatConversion/cmake")

add_library(formatConversionLibrary STATIC
    formatConversion.cpp
)

target_compile_options(formatConversionLibrary PRIVATE -fPIC)