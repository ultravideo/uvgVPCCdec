#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class PostReconstruction {
public:
    static void PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, std::vector<uint32_t>& partition);

    static size_t color_point_cloud( point_cloud_frame* reconstruct, decompressed_data* data, const size_t multipleStreams, const uint8_t attributeCount );
};