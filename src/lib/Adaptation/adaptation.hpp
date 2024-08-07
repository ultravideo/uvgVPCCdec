#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Adaptation {
public:
    /* ------------------------ ripped from tmc2------------------------ */
    static void convertYUV8ToRGB8(point_cloud_frame* reconstruct);

    static void output_decoded_frame(std::shared_ptr<point_cloud_frame> reconstruct, uvgvpcc_dec::API::decoded_output* out);
};