#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class PostReconstruction {
public:
    static void PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, size_t frame_index);

    static size_t color_point_cloud( point_cloud_frame* reconstruct, decompressed_data* data, size_t frame_index, const size_t multipleStreams, const uint8_t attributeCount );
};