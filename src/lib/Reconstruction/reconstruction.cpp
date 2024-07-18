#include "reconstruction.hpp"
#include "reconstruction_common.hpp"

using namespace uvgvpcc_dec;

/* ------------------------ ripped from tmc2------------------------ */
size_t Reconstruction::patch_to_canvas(const size_t u, const size_t v, size_t canvasStride, size_t canvasHeight,
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

std::vector<point3d> Reconstruction::generate_points( /*const GeneratePointCloudParameters&  params,
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
                                                const size_t                         neighbor*/ )
{
    return {};
}

/* ------------------------ ripped from tmc2------------------------ */
void Reconstruction::generateOccupancyMap( atlas_frame* frame, picture* videoFrame,
    std::vector<uint32_t>* occupancyMap, const size_t occupancyPrecision) {
    auto   width        = frame->tile_width;
    auto   height       = frame->tile_height;
    size_t u0           = 0; //tile.getLeftTopXInFrame() / occupancyPrecision;
    size_t v0           = 0; //tile.getLeftTopYInFrame() / occupancyPrecision;
    occupancyMap->resize( width * height, 0 );
    for ( size_t v = 0; v < height; ++v ) {
        for ( size_t u = 0; u < width; ++u ) {
            uint8_t pixel = videoFrame->get_Y_value( u0 + u / occupancyPrecision, v0 + v / occupancyPrecision );
            /*if ( !enhancedOccupancyMapForDepthFlag ) {
                occupancyMap[v * width + u] = ( pixel > thresholdLossyOM );
                pixel                       = occupancyMap[v * width + u];
            } else {*/
            occupancyMap->at(v * width + u) = pixel;
        }
    }
}

/* ------------------------ ripped from tmc2------------------------ */
int Reconstruction::patchBlock2CanvasBlock( const size_t uBlk,
                                      const size_t vBlk,
                                      size_t       canvasStrideBlk,
                                      size_t       canvasHeightBlk,
                                      const patch &p, const Tile tile ) {
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
    if ( x >= canvasStrideBlk ) { return -1; }
    if ( y >= canvasHeightBlk ) { return -1; }
    if ( tile.minU != -1 ) {
    if ( (int)x < tile.minU ) { return -1; }
    if ( (int)y < tile.minV ) { return -1; }
    if ( (int)x > tile.maxU ) { return -1; }
    if ( (int)y > tile.maxV ) { return -1; }
    }
    return int( x + canvasStrideBlk * y );
}

/* ------------------------ ripped from tmc2------------------------ */
void Reconstruction::generateBlockToPatchFromOccupancyMapVideo(atlas_frame* frame, picture* occupancyMapImage,
        const size_t occupancyResolution, const size_t occupancyPrecision )
{
    auto test = occupancyResolution;
    auto&        patches            = frame->patches_map;//tile.getPatches();
    const size_t patchCount         = patches.size();
    const size_t blockToPatchWidth  = frame->tile_width; //tile.getWidth() / occupancyResolution;
    const size_t blockToPatchHeight = frame->tile_height; //tile.getHeight() / occupancyResolution;
    const size_t blockCount         = blockToPatchWidth * blockToPatchHeight;
    auto&        blockToPatch       = frame->block_to_patch; //tile.getBlockToPatch();
    blockToPatch.resize( blockCount );
    std::fill( blockToPatch.begin(), blockToPatch.end(), 0 );
    for ( size_t patchIndex = 0; patchIndex < patchCount; ++patchIndex ) {
        std::cout << "patch " << patchIndex << std::endl;
        auto&  patch        = patches[patchIndex];
        size_t nonZeroPixel = 0;
        for ( size_t v0 = 0; v0 < patch.getSizeV0(); ++v0 ) {
            for ( size_t u0 = 0; u0 < patch.getSizeU0(); ++u0 ) {
                const size_t blockIndex = patchBlock2CanvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                nonZeroPixel            = 0;
                for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) {
                    const size_t v = v0 * patch.occupancy_resolution + v1;
                    for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                        const size_t u = u0 * patch.occupancy_resolution + u1;
                        size_t       x;
                        size_t       y;
                        patch_to_canvas(u, v, frame->tile_width, frame->tile_height, x, y, patch);
                        x += frame->getLeftTopXInFrame();
                        y += frame->getLeftTopYInFrame();
                        nonZeroPixel += static_cast<unsigned long long>(
                            occupancyMapImage->get_Y_value(x / occupancyPrecision, y / occupancyPrecision ) != 0);
                    }
                }
                if ( nonZeroPixel > 0 ) { blockToPatch[blockIndex] = patchIndex + 1; }
            }
        }
    }
}

void Reconstruction::reconstructPointCloud(decompressed_data* data, point_cloud_frame* reconstruct)
{
    Logger::log(LogLevel::INFO, "Reconstruction", "Reconstructing point cloud \n");

    auto b = reconstruct->positions.size();    
    //for(data->frame_count) only 1 frame at first ------------------------------
    auto current_atlas_frame = data->atlas_map.front().get();
    picture current_occupancy_frame = data->occupancy_map.pictures.front();

    size_t tileWidth = current_atlas_frame->tile_width;
    size_t tileHeight = current_atlas_frame->tile_height;

    // only one atlas = one VPS frame width
    uint32_t occupancyPrecision = data->vps.vps_frame_width.front() / data->occupancy_map.width;
    std::cout << "occupancyPrecision " << occupancyPrecision << std::endl;
    std::vector<uint32_t> occupancyMap = {}; // pre tile?

    generateOccupancyMap( current_atlas_frame, &current_occupancy_frame, &occupancyMap, occupancyPrecision);
    
    Logger::log(LogLevel::INFO, "Reconstruction", "Generate block to patch \n");
    // single tile
    generateBlockToPatchFromOccupancyMapVideo(
            current_atlas_frame, &current_occupancy_frame,
            size_t( 1 ) << data->asps.asps_log2_patch_packing_block_size, occupancyPrecision);

    if(true) { // if ( !params.pbfEnableFlag_ ) TODO ----------------------
        occupancyMap.resize( tileWidth * tileHeight, 0 );
        for ( size_t v = 0; v < tileHeight; ++v ) {
            for ( size_t u = 0; u < tileWidth; ++u ) {
                // tile.getLeftTopXInFrame() = 0 for single tile. Then, 0 divided by occupancyPrecision = 0
                occupancyMap[v * tileWidth + u] = current_occupancy_frame.get_Y_value(u / occupancyPrecision, v / occupancyPrecision);
            }
        }
    }
    std::cout << "occupancyMap.size() " << occupancyMap.size() << std::endl;

    auto &blockToPatch = current_atlas_frame->block_to_patch;
    for ( std::size_t patch_index = 0; patch_index < current_atlas_frame->patches_map.size(); patch_index++ ) {
        const patch &current_patch = current_atlas_frame->patches_map.at(patch_index);
        std::cout << "Patch index " << patch_index << std::endl;
        for ( size_t v0 = 0; v0 < current_patch.TilePatch2dPosY; ++v0 ) {
            std::cout << "loop Y" << std::endl;
            for ( size_t u0 = 0; u0 < current_patch.TilePatch2dPosX; ++u0 ) {
                std::cout << "loop X" << std::endl;
                // implement TODO
                const size_t blockIndex = 0;//patch.patchBlock2CanvasBlock( u0, v0, blockToPatchWidth, blockToPatchHeight );
                if ( blockToPatch[blockIndex] == (patch_index + 1) ) {
                    for ( size_t v1 = 0; v1 < current_patch.occupancy_resolution; ++v1 ) {
                        const size_t v = v0 * current_patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < current_patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * current_patch.occupancy_resolution + u1;
                            size_t   x;
                            size_t   y;
                            bool     occupancy     = false;
                            size_t   canvasIndex = patch_to_canvas(u, v, tileWidth, tileHeight, x, y, current_patch);
                            size_t   xInVideoFrame = x + 0; // tile.getLeftTopXInFrame(); is 0 for singletile
                            size_t   yInVideoFrame = y + 0; // tile.getLeftTopYInFrame(); is 0 for singletile
                            bool     isBoundary    = false;
                            /*if ( params.pbfEnableFlag_ ) {
                                occupancy = patch.getOccupancyMap( u, v ) != 0;
                                if ( occupancy ) { isBoundary = patch.isBorder( u, v ); }
                            } else {*/
                            occupancy = occupancyMap[canvasIndex] != 0; // TODO: -------------------

                            // ------------------- no enhanced occupancy map code -------------------
                            std::vector<point3d> createdPoints;
                            /*if ( params.pointLocalReconstruction_ ) { // false
                            auto& mode =
                                context.getPointLocalReconstructionMode( patch.getPointLocalReconstructionMode( u0, v0 ) );
                            createdPoints = generatePoints( params, tile, videoGeometryMultiple, videoFrameIndex, patchIndex, u,
                                                            v, xInVideoFrame, yInVideoFrame, mode.interpolate_, mode.filling_,
                                                            mode.minD1_, mode.neighbor_ );
                            }*/

                            bool test = occupancy;
                            Logger::log(LogLevel::INFO, "Reconstruction", "Generate point positions \n");
                            createdPoints = generate_points( /*params, tile, videoGeometryMultiple, videoFrameIndex, patchIndex, u,
                                v, xInVideoFrame, yInVideoFrame*/ );
                        }
                    }
                }
            }
        }   
    }
}
                    /*
                    // not params.enhancedOccupancyMapCode_
                        
                        if ( !createdPoints.empty() ) {
                        for ( size_t i = 0; i < createdPoints.size(); i++ ) {
                            if ( ( !params.removeDuplicatePoints_ ) ||
                                ( ( i == 0 ) || ( createdPoints[i] != createdPoints[0] ) ) ) {
                            size_t pointindex = 0;
                            if ( patch.getAxisOfAdditionalPlane() == 0 ) {
                                pointindex = reconstruct.addPoint( createdPoints[i] );
                                reconstruct.setPointPatchIndex( pointindex, tileIndex, patchIndex );
                            } else {
                                PCCVector3D tmp;
                                inverseRotatePosition45DegreeOnAxis( patch.getAxisOfAdditionalPlane(),
                                                                    params.geometryBitDepth3D_, createdPoints[i], tmp );
                                pointindex = reconstruct.addPoint( tmp );
                                reconstruct.setPointPatchIndex( pointindex, tileIndex, patchIndex );
                            }
                            const size_t pointindex_1 = pointindex;
                            reconstruct.setColor( pointindex_1, color );
                            if ( params.pbfEnableFlag_ ) { reconstruct.setBoundaryPointType( pointindex_1, isBoundary ); }
                            if ( PCC_SAVE_POINT_TYPE == 1 ) {
                                if ( params.singleMapPixelInterleaving_ ) {
                                size_t flag;
                                flag = ( i == 0 ) ? ( x + y ) % 2 : ( i == 1 ) ? ( x + y + 1 ) % 2 : g_intermediateLayerIndex;
                                reconstruct.setType( pointindex_1, flag == 0 ? POINT_D0 : flag == 1 ? POINT_D1 : POINT_DF );
                                } else {
                                reconstruct.setType( pointindex_1, i == 0 ? POINT_D0 : i == 1 ? POINT_D1 : POINT_DF );
                                }
                            }
                            partition.push_back( uint32_t( patchIndex ) );
                            if ( params.singleMapPixelInterleaving_ ) {
                                pointToPixel.emplace_back(
                                    x, y,
                                    i == 0 ? ( static_cast<size_t>( x + y ) % 2 )
                                        : i == 1 ? ( static_cast<size_t>( x + y + 1 ) % 2 ) : g_intermediateLayerIndex );
                            } else if ( params.pointLocalReconstruction_ ) {
                                pointToPixel.emplace_back(
                                    x, y, i == 0 ? 0 : i == 1 ? g_intermediateLayerIndex : g_intermediateLayerIndex + 1 );
                            } else {
                        pointToPixel.emplace_back( x, y, i < 2 ? i : g_intermediateLayerIndex + 1 );
                        }
                    }
                    }
                }
                }
            }
            }
        }
        }
    */