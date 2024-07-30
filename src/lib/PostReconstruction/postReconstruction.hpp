#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class PostReconstruction {
public:
    static void PostProcess(const decompressed_gof &gof, point_cloud_frame* reconstruct, const size_t frame_index);

    static size_t color_point_cloud( point_cloud_frame* reconstruct, const decompressed_gof &gof, const size_t frame_index, const size_t multipleStreams, const uint8_t attributeCount );
};