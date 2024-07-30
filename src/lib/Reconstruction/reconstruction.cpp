#include "reconstruction.hpp"
#include "reconstruction_common.hpp"
#include "Decompression/decompression.hpp"

using namespace uvgvpcc_dec;

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
std::vector<point3d> Reconstruction::generate_points(const patch& patch, std::vector<video_map>& videoGeometryMultiple,
    const size_t videoFrameIndex, const size_t u, const size_t v, const size_t x,
    const size_t y, const size_t mapCountMinus1, const bool multipleStreams, const bool absoluteD1_)
{
    auto& frame0 = videoGeometryMultiple[0].pictures.at(videoFrameIndex);
    std::vector<point3d> createdPoints;
    point3d point0;

    point0 = patch.generatePoint( u, v, frame0.get_Y_value(x, y) );

    createdPoints.push_back( point0 );
    if ( mapCountMinus1 > 0 ) {
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
void Reconstruction::generateOccupancyMap( const size_t width, const size_t height, picture* videoFrame,
    std::vector<uint8_t>* occupancyMap, const size_t occupancyPrecision) {
    occupancyMap->resize( width * height, 0 );
    for ( size_t v = 0; v < height; ++v ) {
        for ( size_t u = 0; u < width; ++u ) {
            uint8_t pixel = videoFrame->get_Y_value(u / occupancyPrecision, v / occupancyPrecision );
            occupancyMap->at(v * width + u) = pixel;
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
void Reconstruction::generateBlockToPatchFromOccupancyMapVideo(atlas_frame* frame, picture* occupancyMapImage,
        const size_t blockToPatchWidth, const size_t blockToPatchHeight, const size_t occupancyPrecision )
{
    const size_t blockCount         = blockToPatchWidth * blockToPatchHeight;
    auto&        blockToPatch       = frame->block_to_patch;
    blockToPatch.resize( blockCount );
    std::fill( blockToPatch.begin(), blockToPatch.end(), 0 );
    for ( size_t patchIndex = 0; patchIndex < frame->patches_map.size(); ++patchIndex ) {
        const patch& patch = frame->patches_map.at(patchIndex);
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
                        patch_to_canvas(u, v, frame->tile_width, frame->tile_height, x, y, patch);
                        nonZeroPixel += static_cast<unsigned long long>(
                            occupancyMapImage->get_Y_value(x / occupancyPrecision, y / occupancyPrecision ) != 0);
                    }
                }
                if ( nonZeroPixel > 0 ) { blockToPatch[blockIndex] = patchIndex + 1; nonZeroCount++; }
            }
        }
    }
}

void Reconstruction::construct_point_cloud_frame(decompressed_data* data, point_cloud_frame* reconstruct, size_t const frame_index)
{
    Logger::log(LogLevel::INFO, "Reconstruction", "Reconstructing point cloud frame " + std::to_string(frame_index) + " \n");

    atlas_frame* current_atlas_frame = data->atlas_map.at(frame_index).get();
    size_t atlas_index = current_atlas_frame->atlas_index;
    picture current_occupancy_frame = data->occupancy_map.pictures.at(frame_index);
    size_t tileWidth = current_atlas_frame->tile_width;
    size_t tileHeight = current_atlas_frame->tile_height;

    const v3c_parameter_set &vps = Decompression::get_saved_vps();
    const atlas_sequence_parameter_set &asps = Decompression::get_saved_asps();
    const atlas_frame_parameter_set &afps = Decompression::get_saved_afps();

    std::vector<point3d> &pointToPixel = current_atlas_frame->pointToPixel_;
    pointToPixel.resize( 0 );
    size_t occupancyPrecision = vps.vps_frame_width.at(atlas_index) / data->occupancy_map.width;
    std::vector<uint8_t> occupancyMap = {};

    generateOccupancyMap( tileWidth, tileHeight, &current_occupancy_frame, &occupancyMap, occupancyPrecision);
    
    size_t patch_packing_block_size = size_t( 1 ) << asps.asps_log2_patch_packing_block_size;
    const size_t blockToPatchWidth = tileWidth / patch_packing_block_size;
    const size_t blockToPatchHeight = tileHeight / patch_packing_block_size;
    generateBlockToPatchFromOccupancyMapVideo(current_atlas_frame, &current_occupancy_frame,
        blockToPatchWidth, blockToPatchHeight, occupancyPrecision);
    // Above this belong to decoding instead of reconstruction??
    
    /* ------------------------ reconstruction ------------------------ */
    auto &blockToPatch = current_atlas_frame->block_to_patch;
    const bool patchPrecedenceOrderFlag = asps.asps_patch_precedence_order_flag;
    const size_t patch_count = current_atlas_frame->patches_map.size();

    bool removeDuplicatePoints_ = true;

    const size_t mapCount = vps.vps_map_count_minus1.at(atlas_index) + 1;

    const size_t geometryBitDepth3D_ = vps.geometry_info.at(0).gi_geometry_2d_bit_depth_minus1 + 1;
    size_t videoFrameIndex = frame_index * mapCount;
    size_t geoFrameCount = data->geometry_maps.at(0).frame_count;
    if ( geoFrameCount < ( videoFrameIndex + mapCount ) ) { throw std::runtime_error("Invalid geoFrameCount");}

    /*if(data->asps.asps_vpcc_remove_duplicate_point_enabled_flag) {
        < "TODO: Implement duplicate point removal" << std::endl;
    }*/
    
    for ( std::size_t index = 0; index < current_atlas_frame->patches_map.size(); index++ ) {
        size_t patchIndex = patchPrecedenceOrderFlag  ? ( patch_count - index - 1 ) : index;
        const size_t patchIndexPlusOne = patchIndex + 1;
        const patch& patch  = current_atlas_frame->patches_map[patchIndex];
        size_t patch_true = 0;
        for ( size_t v0 = 0; v0 < patch.TilePatch2dSizeY; ++v0 ) {
            for ( size_t u0 = 0; u0 < patch.TilePatch2dSizeX; ++u0 ) {
                const size_t blockIndex = patchBlock2CanvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                if ( blockToPatch[blockIndex] == patchIndexPlusOne ) {
                    patch_true++;
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) {
                        const size_t v = v0 * patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * patch.occupancy_resolution + u1;
                            size_t x; // value filled in patch_to_canvas
                            size_t y; // value filled in patch_to_canvas
                            size_t canvasIndex = patch_to_canvas(u, v, tileWidth, tileHeight, x, y, patch);
                            bool         isBoundary    = false;
                            bool         occupancy     = false;

                            occupancy = occupancyMap[canvasIndex] != 0;
                            size_t mapCountMinus1_ = vps.vps_map_count_minus1.at(atlas_index);
                            bool multipleStreams_ = vps.vps_multiple_map_streams_present_flag.at(atlas_index);
                            bool absoluteD1_ = vps.vps_map_count_minus1.at(atlas_index) == 0 || vps.vps_map_absolute_coding_enabled_flag.at(atlas_index).at(1);
                            
                            if ( !occupancy ) { continue; }
                            std::vector<point3d> createdPoints;
                            createdPoints = generate_points(patch, data->geometry_maps, videoFrameIndex, u,
                                                v, x, y, mapCountMinus1_, multipleStreams_, absoluteD1_);
                            if ( !createdPoints.empty() ) {
                                for ( size_t i = 0; i < createdPoints.size(); i++ ) {
                                    if ( ( !removeDuplicatePoints_ ) || ( ( i == 0 ) || ( createdPoints[i] != createdPoints[0] ) ) ) {

                                        if ( patch.axisOfAdditionalPlane_ == 0 ) {
                                            reconstruct->addPoint( createdPoints[i] );
                                        } else {
                                            point3d tmp;
                                            inverseRotatePosition45DegreeOnAxis( patch.axisOfAdditionalPlane_,
                                                                                geometryBitDepth3D_, createdPoints[i], tmp );
                                            reconstruct->addPoint( tmp );
                                        }
                                        assert(i < 2);
                                        pointToPixel.emplace_back( x, y, i);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }   
    Logger::log(LogLevel::DEBUG, "Reconstruction", "pointToPixel size " + std::to_string(frame_index) 
        + ", reconstruct.pointCount " + std::to_string(reconstruct->getPointCount()) + " \n");
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