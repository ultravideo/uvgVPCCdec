#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class PostReconstruction {
public:
    static void PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, std::vector<uint32_t>& partition);

    static size_t colorPointCloud( point_cloud_frame*                       reconstruct,
                          decompressed_data* data,
                          const std::vector<bool>&            absoluteT1List,
                          const size_t                        multipleStreams,
                          const uint8_t                       attributeCount,
                          size_t                              accTilePointCount );
                          
    static bool transferColorWeight( point_cloud_frame* source, point_cloud_frame* target, const size_t point_count);
};