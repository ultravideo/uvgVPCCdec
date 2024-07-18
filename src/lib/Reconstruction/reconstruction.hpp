#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public: 
    static void reconstructPointCloud(decompressed_data* data, point_cloud_frame* reconstruct);

    static size_t patch_to_canvas(const size_t u, const size_t v, size_t canvasStride, size_t canvasHeight,
        size_t& x, size_t& y, const patch &p);
    static void generateBlockToPatchFromOccupancyMapVideo(atlas_frame* frame, picture* occupancyMapImage,
        const size_t occupancyResolution, const size_t occupancyPrecision );
    static void generateOccupancyMap( atlas_frame* frame, picture* videoFrame,
        std::vector<uint32_t>* occupancyMap, const size_t occupancyPrecision);

    static std::vector<point3d> generate_points( /*const GeneratePointCloudParameters&  params,*/
                                                  atlas_frame*                     tile,
                                                  /*const std::vector<PCCVideoGeometry>& videoGeometryMultiple,*/
                                                  const size_t                         videoFrameIndex,
                                                  const size_t                         patchIndex,
                                                  const size_t                         u,
                                                  const size_t                         v,
                                                  const size_t                         x,
                                                  const size_t                         y,
                                                  const bool                           interpolate = 0,
                                                  const bool                           filling = 0,
                                                  const size_t                         minD1 = 0,
                                                  const size_t                         neighbor = 0 );

    struct Tile {
        int minU;
        int maxU;
        int minV;
        int maxV;
        Tile() : minU( -1 ), maxU( -1 ), minV( -1 ), maxV( -1 ){};
    };
    static int patchBlock2CanvasBlock( const size_t uBlk,
                                      const size_t vBlk,
                                      size_t       canvasStrideBlk,
                                      size_t       canvasHeightBlk,
                                      const patch &p, const Tile tile = Tile() );

};