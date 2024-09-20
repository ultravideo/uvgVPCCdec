#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class PostReconstruction {
public:
    static void initializeStaticParameters(uvgvpcc_dec::context* context);

    static void PostProcess(decompressed_cu* cu, point_cloud_frame* reconstruct, const size_t cu_frame_index, const size_t gof_index);

    static void color_point_cloud( point_cloud_frame* reconstruct, const decompressed_cu &cu, const size_t cu_frame_index,
        const size_t gof_index );
};