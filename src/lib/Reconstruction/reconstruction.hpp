#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

class Reconstruction {
public:
    static void construct_point_cloud_frame(decompressed_data* data, point_cloud_frame* reconstruct, const size_t frame_index);

    /* ------------------------ ripped from tmc2------------------------ */
    static size_t patch_to_canvas(const size_t u, const size_t v, const size_t canvasStride, const size_t canvasHeight,
        size_t& x, size_t& y, const patch &p);

    /* ------------------------ ripped from tmc2------------------------ */
    static void generateBlockToPatchFromOccupancyMapVideo(atlas_frame* frame, picture* occupancyMapImage,
        const size_t blockToPatchWidth, const size_t blockToPatchHeight, const size_t occupancyPrecision );

    /* ------------------------ ripped from tmc2------------------------ */
    static void generateOccupancyMap( const size_t width, const size_t height, picture* videoFrame,
        std::vector<uint8_t>* occupancyMap, const size_t occupancyPrecision);

    /* ------------------------ ripped from tmc2------------------------ */
    static std::vector<point3d> generate_points(const patch &patch, std::vector<video_map>& videoGeometryMultiple,
        const size_t videoFrameIndex, const size_t u, const size_t v, const size_t x,
        const size_t y, const size_t map_count, const bool multipleStreams, const bool absoluteD1_);

    /* ------------------------ ripped from tmc2------------------------ */
    static int patchBlock2CanvasBlock( const size_t uBlk, const size_t vBlk, const size_t blockToPatchWidth, const size_t blockToPatchHeight,
        const patch &p);

    /* ------------------------ ripped from tmc2------------------------ */
    static void inverseRotatePosition45DegreeOnAxis( size_t Axis, size_t lod, point3d &input, point3d& output ); 

};