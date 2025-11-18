#include "reconstruction.hpp"
#include "reconstruction_common.hpp"
#include "Decompression/decompression.hpp"

using namespace uvgvpcc_dec;

size_t max_points_ = 0;
uvgvpcc_dec::context* dec_context_;

void Reconstruction::initializeStaticParameters(const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::context* context)
{
    if(param.max_points != 0) { // if its 0, its not specified
        max_points_ = param.max_points;
    }
    dec_context_ = context;
}

/* ------------------------ ripped from tmc2------------------------ */
size_t Reconstruction::patch_to_canvas(const size_t u, const size_t v, const size_t canvasStride, const size_t canvasHeight,
    size_t& x, size_t& y, const patch &p)
{
    size_t u0_ = p.TilePatch2dPosX;
    size_t v0_ = p.TilePatch2dPosY;
    size_t sizeU0_ = p.TilePatch2dSizeX;
    size_t sizeV0_ = p.TilePatch2dSizeY;
    size_t occupancyResolution_ = p.occupancy_resolution;
    switch ( p.TilePatchOrientationIndex ) {
    case PATCH_ORIENTATION_DEFAULT:
        x = u + u0_ * occupancyResolution_;
        y = v + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_ROT90:
        x = ( sizeV0_ * occupancyResolution_ - 1 - v ) + u0_ * occupancyResolution_;
        y = u + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_ROT180:
        x = ( sizeU0_ * occupancyResolution_ - 1 - u ) + u0_ * occupancyResolution_;
        y = ( sizeV0_ * occupancyResolution_ - 1 - v ) + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = v + u0_ * occupancyResolution_;
        y = ( sizeU0_ * occupancyResolution_ - 1 - u ) + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_MIRROR:
        x = ( sizeU0_ * occupancyResolution_ - 1 - u ) + u0_ * occupancyResolution_;
        y = v + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_MROT90:
        x = ( sizeV0_ * occupancyResolution_ - 1 - v ) + u0_ * occupancyResolution_;
        y = ( sizeU0_ * occupancyResolution_ - 1 - u ) + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = u + u0_ * occupancyResolution_;
        y = ( sizeV0_ * occupancyResolution_ - 1 - v ) + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = v + u0_ * occupancyResolution_;
        y = u + v0_ * occupancyResolution_;
        break;
    case PATCH_ORIENTATION_SWAP:  // swapAxis
        x = v + u0_ * occupancyResolution_;
        y = u + v0_ * occupancyResolution_;
        break;
    default: assert( 0 ); break;
    }
    // checking the results are within canvas boundary (missing y check)
    if ( x >= canvasStride || y >= canvasHeight ) {
        printf(
            "patch2Canvas (x,y) is out of boundary : frame %zu, tile %zu canvassize %zux%zu : uvstart(%zu,%zu) "
            "size(%zu,%zu), uv(%zu,%zu), xy(%zu,%zu), orientation(%zu)\n",
            size_t(1), size_t(1), canvasStride, canvasHeight, u0_ * occupancyResolution_, v0_ * occupancyResolution_,
            sizeU0_ * occupancyResolution_, sizeV0_ * occupancyResolution_, u, v, x, y, size_t(p.TilePatchOrientationIndex) );
        exit( 180 );
    }
    assert( (int)x >= 0 );
    assert( (int)y >= 0 );
    assert( x < canvasStride );
    assert( y < canvasHeight );
    return ( x + canvasStride * y );
}

/* ------------------------ ripped from tmc2------------------------ */
std::vector<point3d> Reconstruction::generate_points(const patch& patch, const std::vector<video_map>& videoGeometryMultiple,
    const size_t videoFrameIndex, const size_t u, const size_t v, 
    const size_t x, const size_t y, const size_t map_count, 
    const bool multipleStreams, const bool absoluteD1_)
{
    auto& frame0 = videoGeometryMultiple[0].pictures.at(videoFrameIndex);
    std::vector<point3d> createdPoints;
    point3d point0;

    // First layer
    point0 = patch.generatePoint( u, v, frame0.get_Y_value(x, y) );
    createdPoints.push_back( point0 );

    // Second layer if Double Layer is enabled, map_count = 2
    //if ( map_count > 1 )
    if ( map_count == 2 ) {
      point3d  point1( point0 );
      auto& frame1 = multipleStreams ? videoGeometryMultiple[1].pictures.at(videoFrameIndex) 
                                            : videoGeometryMultiple[0].pictures.at( 1 + videoFrameIndex );
      if ( absoluteD1_ ) {
        point1 = patch.generatePoint( u, v, frame1.get_Y_value( x, y ) );
      } else {
        if ( patch.TilePatchProjectionID == 0 ) {
          point1.data_[patch.normalAxis_] += frame1.get_Y_value( x, y );
        } else {
          point1.data_[patch.normalAxis_] -= frame1.get_Y_value( x, y );
        }
      }
      createdPoints.push_back( point1 );
    }
    return createdPoints;
}

/* ------------------------ somewhat ripped from tmc2------------------------ */
void Reconstruction::generateOccupancyMap( const size_t width, const size_t height, const picture &videoFrame,
    std::vector<uint8_t> &occupancyMap, const size_t occupancyPrecision) {
    Logger::log<LogLevel::TRACE>("Reconstruction", "Generate occupancy map with precision " + std::to_string(occupancyPrecision) + " \n");
    occupancyMap.resize( width * height, 0 );
    for ( size_t v = 0; v < height; ++v ) {
        for ( size_t u = 0; u < width; ++u ) {
            uint8_t pixel = videoFrame.get_Y_value(u / occupancyPrecision, v / occupancyPrecision );
            occupancyMap.at(v * width + u) = pixel;
        }
    }
}

/* ------------------------ ripped from tmc2------------------------ */
int Reconstruction::patchBlock2CanvasBlock( const size_t uBlk, const size_t vBlk, const size_t blockToPatchWidth, const size_t blockToPatchHeight,
    const patch &p)
{
    size_t x, y;
    size_t u0_ = p.TilePatch2dPosX;
    size_t v0_ = p.TilePatch2dPosY;
    size_t sizeU0_ = p.TilePatch2dSizeX;
    size_t sizeV0_ = p.TilePatch2dSizeY;

    switch ( p.TilePatchOrientationIndex ) {
    case PATCH_ORIENTATION_DEFAULT:
        x = uBlk + u0_;
        y = vBlk + v0_;
        break;
    case PATCH_ORIENTATION_ROT90:
        x = ( sizeV0_ - 1 - vBlk ) + u0_;
        y = uBlk + v0_;
        break;
    case PATCH_ORIENTATION_ROT180:
        x = ( sizeU0_ - 1 - uBlk ) + u0_;
        y = ( sizeV0_ - 1 - vBlk ) + v0_;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = vBlk + u0_;
        y = ( sizeU0_ - 1 - uBlk ) + v0_;
        break;
    case PATCH_ORIENTATION_MIRROR:
        x = ( sizeU0_ - 1 - uBlk ) + u0_;
        y = vBlk + v0_;
        break;
    case PATCH_ORIENTATION_MROT90:
        x = ( sizeV0_ - 1 - vBlk ) + u0_;
        y = ( sizeU0_ - 1 - uBlk ) + v0_;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = uBlk + u0_;
        y = ( sizeV0_ - 1 - vBlk ) + v0_;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = vBlk + u0_;
        y = uBlk + v0_;
        break;
    case PATCH_ORIENTATION_SWAP:  // swapAxis
        x = vBlk + u0_;
        y = uBlk + v0_;
        break;
    default: return -1; break;
    }
    // checking the results are within canvasHeightBlk boundary (missing y check)
    if ( x >= blockToPatchWidth ) { return -1; }
    if ( y >= blockToPatchHeight ) { return -1; }
    return int( x + blockToPatchWidth * y );
}

/* ------------------------ ripped from tmc2------------------------ */
void Reconstruction::generateBlockToPatchFromOccupancyMapVideo(const atlas_frame &frame, const picture &occupancyMapImage,
    std::vector<size_t> &block_to_patch, const size_t patch_packing_block_size, const size_t occupancyPrecision )
{
    const size_t blockToPatchWidth = frame.frame_width / patch_packing_block_size;
    const size_t blockToPatchHeight = frame.frame_height / patch_packing_block_size;

    const size_t blockCount         = blockToPatchWidth * blockToPatchHeight;
    block_to_patch.resize( blockCount, 0 );
    for ( size_t patchIndex = 0; patchIndex < frame.patches_map.size(); ++patchIndex ) {
        const patch& patch = frame.patches_map.at(patchIndex);
        size_t nonZeroPixel = 0;
        size_t nonZeroCount = 0;
        for ( size_t v0 = 0; v0 < patch.TilePatch2dSizeY; ++v0 ) {
            for ( size_t u0 = 0; u0 < patch.TilePatch2dSizeX; ++u0 ) {
                const size_t blockIndex = patchBlock2CanvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                nonZeroPixel            = 0;
                for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) {
                    const size_t v = v0 * patch.occupancy_resolution + v1;
                    for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                        const size_t u = u0 * patch.occupancy_resolution + u1;
                        size_t       x;
                        size_t       y;
                        patch_to_canvas(u, v, frame.frame_width, frame.frame_height, x, y, patch);
                        nonZeroPixel += static_cast<unsigned long long>(
                            occupancyMapImage.get_Y_value(x / occupancyPrecision, y / occupancyPrecision ) != 0);
                    }
                }
                if ( nonZeroPixel > 0 ) { block_to_patch[blockIndex] = patchIndex + 1; nonZeroCount++; }
            }
        }
    }
}

void add_points(point_cloud_frame* reconstruct, const std::vector<point3d> &created_points, const std::vector<point3d> &created_point_to_pixel)
{
    reconstruct->add_points_thread_safe(created_points, created_point_to_pixel);
}

void Reconstruction::setup_point_cloud_frame(composition_unit* cu, point_cloud_frame* reconstruct, const size_t cu_frame_index)
{
    const size_t &gof_index = reconstruct->gof_index;
    printf("Point cloud frame setup ------------------------>\n");
    Logger::log<LogLevel::INFO>("Reconstruction", "Setup point cloud frame " + std::to_string(reconstruct->frame_index_in_gof)
        + " (in composition unit " + std::to_string(cu->cu_index)
        + ") in GOF " + std::to_string(gof_index) + " \n");

    atlas_frame* current_atlas_frame = cu->atlas_map.at(cu_frame_index).get();
    size_t atlas_index = current_atlas_frame->atlas_index;
    const picture &current_occupancy_frame = cu->occupancy_map.pictures.at(cu_frame_index);
    std::vector<uint8_t> &current_occupancy_boolean_map = cu->occupancy_boolean_maps.at(cu_frame_index);
    std::vector<size_t> &current_block_to_patch = cu->block_to_patches.at(cu_frame_index);
    size_t frame_width = current_atlas_frame->frame_width;
    size_t frame_height = current_atlas_frame->frame_height;
    if(max_points_ != 0) {
        reconstruct->positions.reserve(max_points_);
        reconstruct->point_to_pixel.reserve(max_points_);
    }

    const v3c_parameter_set &vps = Decompression::get_saved_params(gof_index).vps;
    const atlas_sequence_parameter_set &asps = Decompression::get_saved_params(gof_index).asps;

    /* --- Strictly speaking, occupancy map generation and block to patch generation belong to decoding,
       --- but in terms of software structure (these are done per frame, occupandy decoding is done per gof),
       --- they may be better done here. */
    size_t occupancyPrecision = vps.vps_frame_width.at(atlas_index) / cu->occupancy_map.width;

    generateOccupancyMap( frame_width, frame_height, current_occupancy_frame, current_occupancy_boolean_map, occupancyPrecision);
    Logger::log<LogLevel::INFO>("Reconstruction", "Occupancy map generated \n");

    size_t patch_packing_block_size = size_t( 1 ) << asps.asps_log2_patch_packing_block_size;
    generateBlockToPatchFromOccupancyMapVideo(*current_atlas_frame, current_occupancy_frame,
        current_block_to_patch, patch_packing_block_size, occupancyPrecision);
    Logger::log<LogLevel::INFO>("Reconstruction", "Block to patch generated \n");
    printf("Point cloud frame setup ------------------------> Done\n");
}

void Reconstruction::process_patch(
    const size_t index, 
    composition_unit* cu, 
    point_cloud_frame* reconstruct, 
    const size_t cu_frame_index, 
    const size_t gof_index)
{
    atlas_frame* current_atlas_frame = cu->atlas_map.at(cu_frame_index).get();
    size_t atlas_index = current_atlas_frame->atlas_index;

    const v3c_parameter_set &vps = Decompression::get_saved_params(gof_index).vps;
    const atlas_sequence_parameter_set &asps = Decompression::get_saved_params(gof_index).asps;
    
    /* ------------------------ reconstruction ------------------------ */
    const bool patchPrecedenceOrderFlag = asps.asps_patch_precedence_order_flag;
    const size_t patch_count = current_atlas_frame->patches_map.size();

    const size_t mapCount = vps.vps_map_count_minus1.at(atlas_index) + 1;
    size_t videoFrameIndex = cu_frame_index * mapCount;
    size_t geoFrameCount = cu->geometry_maps.at(0).frame_count;
    if ( geoFrameCount < ( videoFrameIndex + mapCount ) ) { throw std::runtime_error("Invalid geoFrameCount");}
    
    size_t patchIndex = patchPrecedenceOrderFlag  ? ( patch_count - index - 1 ) : index;
    //printf("Processing patch %d / %d (patch index %d)\n", (int) index, (int) patch_count, (int) patchIndex);
    const size_t patchIndexPlusOne = patchIndex + 1;
    const patch& patch  = current_atlas_frame->patches_map[patchIndex];

    create_points_from_patch(
        reconstruct, 
        patch, 
        *cu, 
        patchIndexPlusOne, 
        videoFrameIndex, 
        cu->occupancy_boolean_maps.at(cu_frame_index),
        *current_atlas_frame, 
        cu->block_to_patches.at(cu_frame_index), 
        gof_index);
}

void Reconstruction::create_points_from_patch(
    point_cloud_frame* reconstruct, 
    const patch &p, 
    const composition_unit &cu, 
    const size_t patch_index_plus_1,
    const size_t video_frame_index, 
    const std::vector<uint8_t> &occupancy_map,
    const atlas_frame &atlas_frame, 
    const std::vector<size_t> &block_to_patch, 
    const size_t gof_index)
{
    std::vector<point3d> point_to_pixel = {};
    std::vector<point3d> created_points = {};

    const size_t frame_width = atlas_frame.frame_width;
    const size_t frame_height = atlas_frame.frame_height;
    const size_t atlas_index = atlas_frame.atlas_index;

    const atlas_sequence_parameter_set &asps = Decompression::get_saved_params(gof_index).asps;
    size_t patch_packing_block_size = size_t( 1 ) << asps.asps_log2_patch_packing_block_size;
    const size_t block_to_patch_width = frame_width / patch_packing_block_size;
    const size_t block_to_patch_height = frame_height / patch_packing_block_size;

    const v3c_parameter_set &vps = Decompression::get_saved_params(gof_index).vps;
    const size_t map_count = vps.vps_map_count_minus1.at(atlas_index) + 1;
    const size_t geo_bit_depth_3d = vps.geometry_info.at(0).gi_geometry_2d_bit_depth_minus1 + 1;

    for ( size_t v0 = 0; v0 < p.TilePatch2dSizeY; ++v0 ) {
        for ( size_t u0 = 0; u0 < p.TilePatch2dSizeX; ++u0 ) {
            const size_t blockIndex = patchBlock2CanvasBlock(u0, v0, block_to_patch_width, block_to_patch_height, p);
            if ( block_to_patch[blockIndex] == patch_index_plus_1 ) {
                for ( size_t v1 = 0; v1 < p.occupancy_resolution; ++v1 ) {
                    const size_t v = v0 * p.occupancy_resolution + v1;
                    for ( size_t u1 = 0; u1 < p.occupancy_resolution; ++u1 ) {
                        const size_t u = u0 * p.occupancy_resolution + u1;
                        size_t x; // value filled in patch_to_canvas
                        size_t y; // value filled in patch_to_canvas
                        size_t canvasIndex = patch_to_canvas(u, v, frame_width, frame_height, x, y, p);
                        bool occupancy     = false;

                        occupancy = occupancy_map[canvasIndex] != 0;
                        bool multipleStreams_ = vps.vps_multiple_map_streams_present_flag.at(atlas_index);
                        bool absoluteD1_ = map_count == 1 || vps.vps_map_absolute_coding_enabled_flag.at(atlas_index).at(1);
                        
                        if ( !occupancy ) { continue; }
                        std::vector<point3d> generated_points;
                        generated_points = generate_points(p, cu.geometry_maps, video_frame_index, u,
                                            v, x, y, map_count, multipleStreams_, absoluteD1_);
                        /*
                            Size of generated_points is 2 for Double layer,
                            Size of generated_points is 1 for Single layer
                        */
                        for ( size_t i = 0; i < generated_points.size(); i++ ) {
                            if ( ( i == 0 ) || ( generated_points[i] != generated_points[0] )  ) {

                                if ( p.axisOfAdditionalPlane_ == 0 ) {
                                    created_points.push_back(generated_points[i]);
                                } else {
                                    point3d tmp;
                                    inverseRotatePosition45DegreeOnAxis( p.axisOfAdditionalPlane_,
                                                                        geo_bit_depth_3d, generated_points[i], tmp );
                                    created_points.push_back(tmp);
                                }
                                //printf("i value: %d, generated_points.size: %d\n", (int)i, (int)generated_points.size());
                                assert(i < 2);
                                point_to_pixel.emplace_back( x, y, i);
                            }
                        }
                    }
                }
            }
        }
    }
    reconstruct->add_points_thread_safe(created_points, point_to_pixel);
}

/* ------------------------ ripped from tmc2------------------------ */
void Reconstruction::inverseRotatePosition45DegreeOnAxis( size_t axis, size_t lod, point3d &input, point3d& output ) {
    size_t s = ( 1u << ( lod - 1 ) ) - 1;
    //output   = input;
    output.data_[0] = input.data_[0];
    output.data_[1] = input.data_[1];
    output.data_[2] = input.data_[2];
    
    if ( axis == 1 ) {  // projection plane is defined by Y Axis.
        output.x() = input.x() - input.z() + s;
        output.x() /= 2.0;
        output.z() = input.x() + input.z() - s;
        output.z() /= 2.0;
    }
    if ( axis == 2 ) {  // projection plane is defined by X Axis.
        output.z() = input.z() - input.y() + s;
        output.z() /= 2.0;
        output.y() = input.z() + input.y() - s;
        output.y() /= 2.0;
    }
    if ( axis == 3 ) {  // projection plane is defined by Z Axis.
        output.y() = input.y() - input.x() + s;
        output.y() /= 2.0;
        output.x() = input.y() + input.x() - s;
        output.x() /= 2.0;
    }
}