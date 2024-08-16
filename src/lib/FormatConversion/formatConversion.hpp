#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class FormatConversion {
public: 
    static void convertToNominalFormat(decompressed_cu* data, const size_t gof_index);

};
