#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public:
    static void construct_point_cloud_frame(decompressed_data* data, point_cloud_frame* reconstruct, size_t frame_index);

    /* ------------------------ ripped from tmc2------------------------ */
    static size_t patch_to_canvas(const size_t u, const size_t v, size_t canvasStride, size_t canvasHeight,
        size_t& x, size_t& y, const patch &p);

    /* ------------------------ ripped from tmc2------------------------ */
    static void generateBlockToPatchFromOccupancyMapVideo(atlas_frame* frame, picture* occupancyMapImage,
        const size_t blockToPatchWidth, const size_t blockToPatchHeight, const size_t occupancyPrecision );

    /* ------------------------ ripped from tmc2------------------------ */
    static void generateOccupancyMap( size_t width, size_t height, picture* videoFrame,
        std::vector<uint32_t>* occupancyMap, const size_t occupancyPrecision);

    /* ------------------------ ripped from tmc2------------------------ */
    static std::vector<point3d> generate_points(const patch &patch, std::vector<video_map>& videoGeometryMultiple,
        const size_t videoFrameIndex, const size_t u, const size_t v, const size_t x,
        const size_t y, const size_t mapCountMinus1, const bool multipleStreams, const bool absoluteD1_);

    /* ------------------------ ripped from tmc2------------------------ */
    struct Tile {
        int minU;
        int maxU;
        int minV;
        int maxV;
        Tile() : minU( -1 ), maxU( -1 ), minV( -1 ), maxV( -1 ){};
    };

    /* ------------------------ ripped from tmc2------------------------ */
    static int patchBlock2CanvasBlock( const size_t uBlk, const size_t vBlk, size_t canvasStrideBlk, size_t canvasHeightBlk,
        const patch &p, const Tile tile = Tile() );

    /* ------------------------ ripped from tmc2------------------------ */
    static void inverseRotatePosition45DegreeOnAxis( size_t Axis, size_t lod, point3d input, vector3d& output ); 

};