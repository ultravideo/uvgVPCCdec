#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public:

    static void initializeStaticParameters(const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::context* context);

    static void construct_point_cloud_frame(const decompressed_gof &gof, point_cloud_frame* reconstruct, const size_t frame_index);

    static void create_points_from_patch(point_cloud_frame* reconstruct, const patch &p, const decompressed_gof &gof, const size_t patch_index_plus_1,
        const size_t video_frame_index, const std::vector<uint8_t> &occupancy_map,
        const atlas_frame &atlas_frame, const std::vector<size_t> &block_to_patch);

    /* ------------------------ ripped from tmc2------------------------ */
    static size_t patch_to_canvas(const size_t u, const size_t v, const size_t canvasStride, const size_t canvasHeight,
        size_t& x, size_t& y, const patch &p);

    /* ------------------------ ripped from tmc2------------------------ */
    static void generateBlockToPatchFromOccupancyMapVideo(const atlas_frame &frame, const picture &occupancyMapImage,
        std::vector<size_t> &block_to_patch, const size_t patch_packing_block_size, const size_t occupancyPrecision );

    /* ------------------------ ripped from tmc2------------------------ */
    static void generateOccupancyMap( const size_t width, const size_t height, const picture &videoFrame,
        std::vector<uint8_t>* occupancyMap, const size_t occupancyPrecision);

    /* ------------------------ ripped from tmc2------------------------ */
    static std::vector<point3d> generate_points(const patch &patch, const std::vector<video_map>& videoGeometryMultiple,
        const size_t videoFrameIndex, const size_t u, const size_t v, const size_t x,
        const size_t y, const size_t map_count, const bool multipleStreams, const bool absoluteD1_);

    /* ------------------------ ripped from tmc2------------------------ */
    static int patchBlock2CanvasBlock( const size_t uBlk, const size_t vBlk, const size_t blockToPatchWidth, const size_t blockToPatchHeight,
        const patch &p);

    /* ------------------------ ripped from tmc2------------------------ */
    static void inverseRotatePosition45DegreeOnAxis( size_t Axis, size_t lod, point3d &input, point3d& output ); 

};