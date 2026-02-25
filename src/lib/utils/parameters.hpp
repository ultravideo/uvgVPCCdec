/// \file Library parameters related operations.

#pragma once

#include <stdexcept>
#include <string>
#include <vector>

#include "utils.hpp"

namespace uvgvpcc_dec {

struct Parameters {
    int hello = 0;

    size_t max_points = 0;
    bool keep_intermediate_files = false;
    bool fast_color_conversion = false;
    
    bool useTMC2AttributeYUVConversion=false;
    bool nbThread=20;

    bool color_inversion_16bits = true;

    bool exportIntermediateFiles = false;
    std::string intermediateFilesDir;
};

extern const Parameters* p_;  // Const pointer to a non-const Parameter struct instance in parameters.cpp

} // namespace uvgvpcc_dec

