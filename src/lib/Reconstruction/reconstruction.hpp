#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public: 
    static void reconstructPointCloud(decompressed_data* data, point_cloud_frame* reconstruct);

    static size_t patch_to_canvas(const size_t u, const size_t v, size_t canvasStride, size_t canvasHeight,
        size_t& x, size_t& y, const patch &p);
    static std::vector<point3d> generate_points( /*const GeneratePointCloudParameters&  params,
                                                  PCCFrameContext&                     tile,
                                                  const std::vector<PCCVideoGeometry>& videoGeometryMultiple,
                                                  const size_t                         videoFrameIndex,
                                                  const size_t                         patchIndex,
                                                  const size_t                         u,
                                                  const size_t                         v,
                                                  const size_t                         x,
                                                  const size_t                         y,
                                                  const bool                           interpolate,
                                                  const bool                           filling,
                                                  const size_t                         minD1,
                                                  const size_t                         neighbor*/ );

};