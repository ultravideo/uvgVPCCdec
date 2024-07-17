#include "reconstruction.hpp"
#include "reconstruction_common.hpp"

using namespace uvgvpcc_dec;

/* ------------------------ ripped from tmc2------------------------ */
size_t Reconstruction::patch_to_canvas(const size_t u, const size_t v, size_t canvasStride, size_t canvasHeight,
    size_t& x, size_t& y, const patch &p)
{
    switch ( p.TilePatchOrientationIndex ) {
    case PATCH_ORIENTATION_DEFAULT:
        x = u + p.TilePatch2dPosX * p.occupancy_resolution;
        y = v + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_ROT90:
        x = ( p.TilePatch2dSizeY * p.occupancy_resolution - 1 - v ) + p.TilePatch2dPosX * p.occupancy_resolution;
        y = u + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_ROT180:
        x = ( p.TilePatch2dSizeX * p.occupancy_resolution - 1 - u ) + p.TilePatch2dPosX * p.occupancy_resolution;
        y = ( p.TilePatch2dSizeY * p.occupancy_resolution - 1 - v ) + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = v + p.TilePatch2dPosX * p.occupancy_resolution;
        y = ( p.TilePatch2dSizeX * p.occupancy_resolution - 1 - u ) + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_MIRROR:
        x = ( p.TilePatch2dSizeX * p.occupancy_resolution - 1 - u ) + p.TilePatch2dPosX * p.occupancy_resolution;
        y = v + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_MROT90:
        x = ( p.TilePatch2dSizeY * p.occupancy_resolution - 1 - v ) + p.TilePatch2dPosX * p.occupancy_resolution;
        y = ( p.TilePatch2dSizeX * p.occupancy_resolution - 1 - u ) + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = u + p.TilePatch2dPosX * p.occupancy_resolution;
        y = ( p.TilePatch2dSizeY * p.occupancy_resolution - 1 - v ) + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = v + p.TilePatch2dPosX * p.occupancy_resolution;
        y = u + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    case PATCH_ORIENTATION_SWAP:  // swapAxis
        x = v + p.TilePatch2dPosX * p.occupancy_resolution;
        y = u + p.TilePatch2dPosY * p.occupancy_resolution;
        break;
    default: assert( 0 ); break;
    }
    // checking the results are within canvas boundary (missing y check)
    /*if ( x >= canvasStride || y >= canvasHeight ) {
    printf(
        "patch2Canvas (x,y) is out of boundary : frame invalid val %zu, tile invalid val %zu canvassize %zux%zu : uvstart(%zu,%zu) "
        "size(%zu,%zu), uv(%zu,%zu), xy(%zu,%zu), orientation(%zu)\n",
        p.frameIndex_, p.tileIndex_, canvasStride, canvasHeight, p.TilePatch2dPosX * p.occupancy_resolution, p.TilePatch2dPosY * p.occupancy_resolution,
        p.TilePatch2dSizeX * p.occupancy_resolution, p.TilePatch2dSizeY * p.occupancy_resolution, u, v, x, y, p.TilePatchOrientationIndex );
    exit( 180 );
    }*/
    //assert( x >= 0 );
    //assert( y >= 0 );
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

void Reconstruction::reconstructPointCloud(decompressed_data* data, point_cloud_frame* reconstruct)
{
    auto b = reconstruct->positions.size();    
    //for(data->frame_count) only 1 frame at first ------------------------------
    auto current_atlas_frame = data->atlas_map.front().get();
    picture current_occupancy_frame = data->occupancy_map.data.front();

    //auto&        blockToPatch          = tile.getBlockToPatch();
    std::vector<uint32_t> blockToPatch = {};
    //auto& occupancyMap = tile.getOccupancyMap();
    std::vector<uint32_t> occupancyMap = {};

    for ( std::size_t patch_index = 0; patch_index < current_atlas_frame->patches_map.size(); patch_index++ ) {
        const patch &current_patch = current_atlas_frame->patches_map.at(patch_index);

        for ( size_t v0 = 0; v0 < current_patch.TilePatch2dPosY; ++v0 ) {
            for ( size_t u0 = 0; u0 < current_patch.TilePatch2dPosX; ++u0 ) {
                
                // implement TODO
                const size_t blockIndex = 0;//patch.patchBlock2CanvasBlock( u0, v0, blockToPatchWidth, blockToPatchHeight );
                if ( blockToPatch[blockIndex] == (patch_index + 1) ) {
                    for ( size_t v1 = 0; v1 < current_patch.occupancy_resolution; ++v1 ) {
                        const size_t v = v0 * current_patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < current_patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * current_patch.occupancy_resolution + u1;
                            size_t tileWidth = current_atlas_frame->tile_width;
                            size_t tileHeight = current_atlas_frame->tile_height;
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
                            createdPoints = generate_points( /*params, tile, videoGeometryMultiple, videoFrameIndex, patchIndex, u,
                                v, xInVideoFrame, yInVideoFrame*/ );
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
}