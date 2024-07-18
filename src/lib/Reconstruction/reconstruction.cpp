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
        
        auto&  patch        = patches[patchIndex];
        /*printf(
            "patch(Intra) %zu: UV0 %4zu %4zu UV1 %4zu %4zu D1=%4zu S=%4zu %4zu %4zu(%4zu) P=%zu O=%zu A=%u%u%u Lod "
            "=(%zu) %zu,%zu 45=%d ProjId=%4zu Axis=%zu \n",
            size_t(patchIndex), size_t(patch.TilePatch2dPosX), size_t(patch.TilePatch2dPosY), size_t(patch.TilePatch3dOffsetU), size_t(patch.TilePatch3dOffsetV), size_t(patch.TilePatch3dOffsetD), size_t(patch.TilePatch2dSizeX),
            size_t(patch.TilePatch2dSizeY), size_t(patch.TilePatch3dRangeD), size_t(0), size_t(patch.TilePatchProjectionID),
            size_t(patch.TilePatchOrientationIndex), uint32_t(0), uint32_t(0), uint32_t(0),
            (size_t)1, (size_t)patch.TilePatchLoDScaleX, (size_t)patch.TilePatchLoDScaleY, 0,
            (size_t)0, size_t(0) );*/
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
                        /*if(patchIndex == 4 && u == 220 && v == 0) {
                            std::cout << "patch2canvas uv(" << u << "," << v << ") xy(" << x << "," << y << ")" << std::endl;
                            std::cout << "v0 " << v0 << ", u0 " << u0 << std::endl;
                        }*/
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

    /* --------------------------- this should work (UNTESTED) --------------------------- */
    generateOccupancyMap( current_atlas_frame, &current_occupancy_frame, &occupancyMap, occupancyPrecision);
    
    /* --------------------------- this should work (TESTED) --------------------------- */
    Logger::log(LogLevel::INFO, "Reconstruction", "Generate block to patch \n");
    // single tile
    generateBlockToPatchFromOccupancyMapVideo(
            current_atlas_frame, &current_occupancy_frame,
            size_t( 1 ) << data->asps.asps_log2_patch_packing_block_size, occupancyPrecision);

    // Above this belong to decoding instead of reconstruction??
    /* --------------------------- this should work (UNTESTED) --------------------------- */
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
    // Size quantiazation in this spot, needed? TODO check ------------------------
    
    /* ------------------------ reconstruction ------------------------ */
    auto &blockToPatch = current_atlas_frame->block_to_patch;
    uint32_t patchIndex = 0;
    const bool patchPrecedenceOrderFlag = data->asps.asps_patch_precedence_order_flag;
    const size_t totalPatchCount       = current_atlas_frame->patches_map.size();
    bool bDecoder = true; // Whats this? tmc2

    size_t occupancyResolution_ = 2;
    std::cout << "NOTE: HARD CODED OCCUPANCY RESOLUTION, MAKE DYNAMIC" << std::endl;
    const size_t blockToPatchWidth     = tileWidth / occupancyResolution_;
    const size_t blockToPatchHeight    = tileHeight / occupancyResolution_;

    for ( std::size_t index = 0; index < current_atlas_frame->patches_map.size(); index++ ) {
        patchIndex                     = ( bDecoder && patchPrecedenceOrderFlag ) ? ( totalPatchCount - index - 1 ) : index;
        const size_t patchIndexPlusOne = patchIndex + 1;
        auto& patch             = current_atlas_frame->patches_map[patchIndex];
        for ( size_t v0 = 0; v0 < patch.getSizeV0(); ++v0 ) {
            for ( size_t u0 = 0; u0 < patch.getSizeU0(); ++u0 ) {
                const size_t blockIndex = patchBlock2CanvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                if ( blockToPatch[blockIndex] == patchIndexPlusOne ) {
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) {
                        const size_t v = v0 * patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            /*const size_t u = u0 * patch.getOccupancyResolution() + u1;
                            size_t       x;
                            size_t       y;
                            bool         occupancy     = false;
                            size_t       canvasIndex   = patch.patch2Canvas( u, v, tileWidth, tileHeight, x, y );
                            size_t       xInVideoFrame = x + tile.getLeftTopXInFrame();
                            size_t       yInVideoFrame = y + tile.getLeftTopYInFrame();
                            bool         isBoundary    = false;
                            if ( params.pbfEnableFlag_ ) {
                                occupancy = patch.getOccupancyMap( u, v ) != 0;
                                if ( occupancy ) { isBoundary = patch.isBorder( u, v ); }
                            } else {
                                occupancy = occupancyMap[canvasIndex] != 0;
                            }
                            if ( !occupancy ) { continue; }
                            std::vector<PCCPoint3D> createdPoints;
                            Logger::log(LogLevel::INFO, "Reconstruction", "Generate point positions \n");
                            createdPoints = generatePoints( params, tile, videoGeometryMultiple, videoFrameIndex, patchIndex, u,
                                                            v, xInVideoFrame, yInVideoFrame );*/
                        }
                    }
                }
            }
        }
    }         
}