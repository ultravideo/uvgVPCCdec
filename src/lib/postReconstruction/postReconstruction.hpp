#pragma once

/// \file Entry point for the point cloud post-reconstruction process.

#include "bitstreamParsing/gof.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

class PostReconstruction {
    public:
    static void applyPostProcess(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_);
};