#include "reconstruction.hpp"
#include "reconstruction_common.hpp"
#include <fstream>
#include <iomanip>

using namespace uvgvpcc_dec;

const size_t g_intermediateLayerIndex = 100; //TMC2 what is this??

enum PCCEndianness { PCC_BIG_ENDIAN = 0, PCC_LITTLE_ENDIAN = 1 };
static inline PCCEndianness PCCSystemEndianness() {
  uint32_t num = 1;
  return ( *( reinterpret_cast<char*>( &num ) ) == 1 ) ? PCC_LITTLE_ENDIAN : PCC_BIG_ENDIAN;
}

/* ------------------------ ripped from tmc2------------------------ */
bool Reconstruction::write( const std::string& fileName, point_cloud_frame* frame, const bool asAscii ) {

    Logger::log(LogLevel::INFO, "Reconstruction", "Write to file \n");
    std::ofstream fout( fileName, std::ofstream::out );
    if ( !fout.is_open() ) { return false; }
    const size_t pointCount = frame->getPointCount();
    fout << "ply" << std::endl;

    if ( asAscii ) {
        fout << "format ascii 1.0" << std::endl;
    } else {
        PCCEndianness endianess = PCCSystemEndianness();
        if ( endianess == PCC_BIG_ENDIAN ) {
        fout << "format binary_big_endian 1.0" << std::endl;
        } else {
        fout << "format binary_little_endian 1.0" << std::endl;
        }
    }
    fout << "element vertex " << pointCount << std::endl;
    if ( asAscii ) {
        fout << "property float x" << std::endl;
        fout << "property float y" << std::endl;
        fout << "property float z" << std::endl;
    } else {
        // fout << "property int16 x" << std::endl;
        // fout << "property int16 y" << std::endl;
        // fout << "property int16 z" << std::endl;
        fout << "property float x" << std::endl;
        fout << "property float y" << std::endl;
        fout << "property float z" << std::endl;
    }
    /*if ( hasNormals() ) {
        fout << "property float nx" << std::endl;
        fout << "property float ny" << std::endl;
        fout << "property float nz" << std::endl;
    }*/
    if ( !frame->colors.empty() ) {
        fout << "property uchar red" << std::endl;
        fout << "property uchar green" << std::endl;
        fout << "property uchar blue" << std::endl;
    }
    /*if ( hasReflectances() ) { fout << "property uint16 refc" << std::endl; }
    if ( PCC_SAVE_POINT_TYPE != 0u ) {
        fout << "property uchar type" << std::endl;
        switch ( PCC_SAVE_POINT_TYPE ) {
        case 1: fout << "comment POINT_TYPE: Unset D0 D1 Filling Smooth InBetween" << std::endl; break;
        case 2: fout << "comment POINT_TYPE: type0 type1 type2  " << std::endl; break;
        default: break;
        }
    }*/
    fout << "element face 0" << std::endl;
    fout << "property list uint8 int32 vertex_index" << std::endl;
    fout << "end_header" << std::endl;
    if ( asAscii ) {
        fout << std::setprecision( std::numeric_limits<double>::max_digits10 );
        for ( size_t i = 0; i < pointCount; ++i ) {
            point3d& position = frame->positions.at(i);
            //const PCCPoint3D& position = ( *this )[i];
            fout << position.x() << " " << position.y() << " " << position.z();

            if ( !frame->colors.empty() ) {
                const uvg_color& color = frame->colors.at(i);
                fout << " " << static_cast<int>( color.data_[0] ) << " " << static_cast<int>( color.data_[1] ) << " "
                    << static_cast<int>( color.data_[2] );
            }

            fout << std::endl;
        }
    } else {
        fout.clear();
        fout.close();
        fout.open( fileName, std::ofstream::binary | std::ofstream::out | std::ofstream::app );
        for ( size_t i = 0; i < pointCount; ++i ) {
            point3d& position = frame->positions.at(i);
            //const PCCPoint3D& position = ( *this )[i];
            // fout.write( reinterpret_cast<const char* const>( &position ), sizeof( PCCType ) * 3 );
            float value[3];
            value[0] = position.data_[0];
            value[1] = position.data_[1];
            value[2] = position.data_[2];
            fout.write( reinterpret_cast<const char*>( &value ), sizeof( float ) * 3 );
            if ( !frame->colors.empty() ) {
                const uvg_color& color = frame->colors.at(i);
                fout.write( reinterpret_cast<const char*>( &color.data_ ), sizeof( uint8_t ) * 3 );
            }
        }
    }
    fout.close();
    return true;
}

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

/* ------------------------ ripped from tmc2------------------------ */
std::vector<point3d> Reconstruction::generate_points(atlas_frame* tile, std::vector<video_map>& videoGeometryMultiple,
    const size_t videoFrameIndex, const size_t patchIndex, const size_t u, const size_t v, const size_t x,
    const size_t y, const size_t mapCountMinus1, const bool multipleStreams, const bool absoluteD1_)
{
    const auto& patch  = tile->patches_map.at(patchIndex); //.getPatch( patchIndex );
    auto& frame0 = videoGeometryMultiple[0].pictures.at(videoFrameIndex);//.getFrame( videoFrameIndex );
    std::vector<point3d> createdPoints;
    point3d point0;
    // if ( params.pbfEnableFlag_ ) { else 
    //point0 = patch.generatePoint( u, v, frame0.getValue( 0, x, y ) );
    point0 = patch.generatePoint( u, v, frame0.get_Y_value(x, y) );

    createdPoints.push_back( point0 );
    //std::cout << "NOTE: hard coded no singleMapPixelInterleaving_ or pointLocalReconstruction_" << std::endl;
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
    }  // if ( params.mapCountMinus1_ > 0 ) {
    return createdPoints;
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
int Reconstruction::patchBlock2CanvasBlock( const size_t uBlk, const size_t vBlk, size_t canvasStrideBlk, size_t canvasHeightBlk,
    const patch &p, const Tile tile )
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
    auto&        patches            = frame->patches_map;//tile.getPatches();
    const size_t patchCount         = patches.size();
    const size_t blockToPatchWidth  = frame->tile_width / occupancyResolution;
    const size_t blockToPatchHeight = frame->tile_height / occupancyResolution;
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
        size_t nonZeroCount = 0;
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
                        //if(patchIndex == 34) { std::cout << "occupancy Y value " << occupancyMapImage->get_Y_value(x / occupancyPrecision, y / occupancyPrecision ) << std::endl; }
                    }
                }
                if ( nonZeroPixel > 0 ) { blockToPatch[blockIndex] = patchIndex + 1; nonZeroCount++; }
            }
        }
        //std::cout << "Patch " << patchIndex << " nonZeroCount " << nonZeroCount << std::endl;
    }
}

void Reconstruction::reconstructPointCloud(decompressed_data* data, point_cloud_frame* reconstruct)
{
    Logger::log(LogLevel::INFO, "Reconstruction", "Reconstructing point cloud \n");

    //for(data->frame_count) only 1 frame at first ------------------------------
    auto current_atlas_frame = data->atlas_map.front().get();
    picture current_occupancy_frame = data->occupancy_map.pictures.front();
    size_t tileWidth = current_atlas_frame->tile_width;
    size_t tileHeight = current_atlas_frame->tile_height;

    std::vector<vector3d> &pointToPixel = current_atlas_frame->getPointToPixel();
    pointToPixel.resize( 0 );

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

    std::cout << "NOTE: HARD CODED remove duplicate points to true" << std::endl;
    bool removeDuplicatePoints_ = true;

    size_t videoFrameIndex;
    std::cout << "NOTE: HARD CODED ATLAS INDEX TO 0, MAKE DYNAMIC" << std::endl;
    const size_t mapCount = data->vps.vps_map_count_minus1.at(0) + 1;

    const size_t geometryBitDepth3D_ = data->vps.geometry_info.at(0).gi_geometry_2d_bit_depth_minus1 + 1;
    videoFrameIndex = 0 * mapCount;
    std::cout << "NOTE: HARD CODED GEOMETRY MAP COUNT to 2, MAKE DYNAMIC" << std::endl;
    size_t geoFrameCount = 2;
    if ( geoFrameCount < ( videoFrameIndex + mapCount ) ) { std::cout << "ERROR before PC generation" << std::endl; return; }

    size_t generatePointsCalled = 0;
    size_t patchBlock2 = 0;
    size_t patch2C = 0;
    size_t if_true = 0;
    size_t test1 = 0;
    size_t test2 = 0;
    size_t first = 0;
    size_t second = 0;

    if(data->asps.asps_vpcc_remove_duplicate_point_enabled_flag) {
        std::cout << "TODO: Implement duplicate point removal" << std::endl;
    }
    
    for ( std::size_t index = 0; index < current_atlas_frame->patches_map.size(); index++ ) {
        patchIndex                     = ( bDecoder && patchPrecedenceOrderFlag ) ? ( totalPatchCount - index - 1 ) : index;
        const size_t patchIndexPlusOne = patchIndex + 1;
        auto& patch             = current_atlas_frame->patches_map[patchIndex];
        size_t patch_true = 0;
        for ( size_t v0 = 0; v0 < patch.getSizeV0(); ++v0 ) {
            for ( size_t u0 = 0; u0 < patch.getSizeU0(); ++u0 ) {
                const size_t blockIndex = patchBlock2CanvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                patchBlock2++;
                if ( blockToPatch[blockIndex] == patchIndexPlusOne ) {
                    patch_true++;
                    if_true++;
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) {
                        const size_t v = v0 * patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * patch.occupancy_resolution + u1;
                            size_t       x;
                            size_t       y;
                            bool         occupancy     = false;
                            size_t       canvasIndex = patch_to_canvas(u, v, tileWidth, tileHeight, x, y, patch);
                            patch2C++;
                            size_t       xInVideoFrame = x + current_atlas_frame->getLeftTopXInFrame();
                            size_t       yInVideoFrame = y + current_atlas_frame->getLeftTopYInFrame();
                            bool         isBoundary    = false;

                            occupancy = occupancyMap[canvasIndex] != 0;
                            //std::cout << "NOTE: hard coded vps mapCountMinus1 and multipleStreams atlas index number" << std::endl;
                            size_t mapCountMinus1_ = data->vps.vps_map_count_minus1.at(0);
                            bool multipleStreams_ = data->vps.vps_multiple_map_streams_present_flag.at(0);
                            bool absoluteD1_ = data->vps.vps_map_count_minus1.at(0) == 0 || data->vps.vps_map_absolute_coding_enabled_flag.at(0).at(1);
                            
                            if ( !occupancy ) { continue; }
                            std::vector<point3d> createdPoints;
                            //Logger::log(LogLevel::INFO, "Reconstruction", "Generate point positions \n");
                            createdPoints = generate_points( /*params, */current_atlas_frame, data->geometry_maps, videoFrameIndex, patchIndex, u,
                                                v, xInVideoFrame, yInVideoFrame, mapCountMinus1_, multipleStreams_, absoluteD1_);
                            generatePointsCalled++;
                            if ( !createdPoints.empty() ) {
                                for ( size_t i = 0; i < createdPoints.size(); i++ ) {
                                    test1++;
                                    if ( ( !removeDuplicatePoints_ ) || ( ( i == 0 ) || ( createdPoints[i] != createdPoints[0] ) ) ) {
                                        size_t pointindex = 0;
                                        size_t tileIndex = 0; // NOte hard coded single tile
                                        test2++;

                                        if ( patch.axisOfAdditionalPlane_ == 0 ) {
                                            first++;
                                            pointindex = reconstruct->addPoint( createdPoints[i] );
                                            reconstruct->setPointPatchIndex( pointindex, tileIndex, patchIndex );
                                        } else {
                                            second++;
                                            vector3d tmp;
                                            inverseRotatePosition45DegreeOnAxis( patch.axisOfAdditionalPlane_,
                                                                                geometryBitDepth3D_, createdPoints[i], tmp );
                                            pointindex = reconstruct->addPoint( tmp );
                                            reconstruct->setPointPatchIndex( pointindex, tileIndex, patchIndex );
                                        }
                                        const size_t pointindex_1 = pointindex;
                                        // IMPLEMENT THIS reconstruct.setColor( pointindex_1, color );
                                        //partition.push_back( uint32_t( patchIndex ) );
                                        /*if ( params.singleMapPixelInterleaving_ ) {
                                            pointToPixel.emplace_back(
                                                x, y,
                                                i == 0 ? ( static_cast<size_t>( x + y ) % 2 )
                                                    : i == 1 ? ( static_cast<size_t>( x + y + 1 ) % 2 ) : g_intermediateLayerIndex );
                                        } else if ( params.pointLocalReconstruction_ ) {
                                            pointToPixel.emplace_back(
                                                x, y, i == 0 ? 0 : i == 1 ? g_intermediateLayerIndex : g_intermediateLayerIndex + 1 );
                                        } else {*/
                                        pointToPixel.emplace_back( x, y, i < 2 ? i : g_intermediateLayerIndex + 1 );
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        //std::cout << "index " << index << ", patch_true count " << patch_true << std::endl;
    }   
    current_atlas_frame->setTotalNumberOfRegularPoints( reconstruct->getPointCount() );
    printf( "frame %zu, tile %zu: regularPoints %zu\n", (size_t)0, (size_t)0, reconstruct->getPointCount() );
    std::cout << "reconstruct->pointPatchIndexes_.size() " << reconstruct->pointPatchIndexes_.size() <<
        " test1 " << test1 << ", test2 " << test2 << 
        " reconstruct->positions.size() " << reconstruct->positions.size() << std::endl;
    std::cout << "patchBlock2 " << patchBlock2 << " if_true " << if_true <<
        " patch2C " << patch2C << " generatePointsCalled " << generatePointsCalled << std::endl;
    std::cout << "pointToPixel.size() " << pointToPixel.size() << ", second " << second << std::endl;
}

/* ------------------------ ripped from tmc2------------------------ */
void Reconstruction::inverseRotatePosition45DegreeOnAxis( size_t axis, size_t lod, point3d input, vector3d& output ) {
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