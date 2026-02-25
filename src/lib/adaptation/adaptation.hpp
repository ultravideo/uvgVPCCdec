#pragma once

#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

class Adaptation {

public:
    static void adapt(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output);

};