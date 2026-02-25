#pragma once

/// \file Entry point for the whole reconstruction process.

#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

class ColorInversion {
   public:

   static void invertColors(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const uvgvpcc_dec::Parameters& param);
};