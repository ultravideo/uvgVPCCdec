#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Adaptation {
public:

    /* Convert 8bit YUV colors to 8bit RGB colors. Reference/slow implementation FROM TMC2 */
    static void convert_colors_slow(point_cloud_frame* reconstruct);

    /* Convert 8bit YUV colors to 8bit RGB colors. Approximate/fast implementation, no floating-point math */
    static void convert_colors_fast(point_cloud_frame* reconstruct);

    // lf : from TMC2
    static void convertYUV16ToRGB8(point_cloud_frame* reconstruct);

    static void output_decoded_frame(std::shared_ptr<point_cloud_frame> reconstruct, uvgvpcc_dec::API::decoded_output* out);
};