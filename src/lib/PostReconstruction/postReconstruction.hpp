#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class PostReconstruction {
public:
    static void PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, std::vector<uint32_t>& partition);
};