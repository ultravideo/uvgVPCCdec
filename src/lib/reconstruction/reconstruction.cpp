#include "reconstruction.hpp"
#include "bitstreamParsing/vps.hpp"

using namespace uvgvpcc_dec;

namespace { 
/* ------------------------ ripped from tmc2------------------------ */
void inverseRotatePosition45DegreeOnAxis( size_t axis, size_t lod, Vector3<typeGeometryInput>& input, Vector3<typeGeometryInput>& output ) {
    const size_t s = ( 1u << ( lod - 1 ) ) - 1;
    //output   = input;
    output[0] = input[0];
    output[1] = input[1];
    output[2] = input[2];
    
    if ( axis == 1 ) {  // projection plane is defined by Y Axis.
        output[0] = input[0] - input[2] + s;
        output[0] /= 2.0;
        output[2] = input[0] + input[2] - s;
        output[2] /= 2.0;
    }
    if ( axis == 2 ) {  // projection plane is defined by X Axis.
        output[2] = input[2] - input[1] + s;
        output[2] /= 2.0;
        output[1] = input[2] + input[1] - s;
        output[1] /= 2.0;
    }
    if ( axis == 3 ) {  // projection plane is defined by Z Axis.
        output[1] = input[1] - input[0] + s;
        output[1] /= 2.0;
        output[0] = input[1] + input[0] - s;
        output[0] /= 2.0;
    }
}

Vector3<uint16_t> get_attribute16(
    const std::vector<uint8_t>& attributeMap, 
    const size_t& offsetU,
    const size_t& offsetV,
    const size_t& width, 
    const size_t u, const size_t v
) {
    // size_t corrected_u = u / 2;
    // size_t corrected_v = v / 2;
    // size_t corrected_width = width / 2;
    
    size_t corrected_u = u >> 1;
    size_t corrected_v = v >> 1;
    size_t corrected_width = width >> 1;

    // if(format == YUV420) {
    //     corrected_u = u / 2;
    //     corrected_v = v / 2;
    //     corrected_width = width / 2;
    // }
    Vector3<uint16_t> color;
    size_t indexUV =  corrected_v * corrected_width + corrected_u;
    // Y
    color[0] = attributeMap[v * width + u]; 
    // U
    color[1] = attributeMap[offsetU + indexUV]; 
    // V
    color[2] = attributeMap[offsetV + indexUV];
    return color;    
}


Vector3<uint8_t> get_attribute(
    const std::vector<uint8_t>& attributeMap, 
    const size_t& offsetU,
    const size_t& offsetV,
    const size_t& width, 
    const size_t u, const size_t v
) {
    // size_t corrected_u = u / 2;
    // size_t corrected_v = v / 2;
    // size_t corrected_width = width / 2;

    size_t corrected_u = u >> 1;
    size_t corrected_v = v >> 1;
    size_t corrected_width = width >> 1;
    // if(format == YUV420) {
    //     corrected_u = u / 2;
    //     corrected_v = v / 2;
    //     corrected_width = width / 2;
    // }
    Vector3<uint8_t> color;
    size_t indexUV =  corrected_v * corrected_width + corrected_u;
    // Y
    color[0] = attributeMap[v * width + u]; 
    // U
    color[1] = attributeMap[offsetU + indexUV]; 
    // V
    color[2] = attributeMap[offsetV + indexUV];
    return color;    
}


// Vector3<uint16_t> get_attribute(
//     const std::vector<uint16_t>& attributeMap, 
//     const size_t& offsetU,
//     const size_t& offsetV,
//     const size_t& width, 
//     const size_t u, const size_t v
// ) {
//     size_t corrected_u = u >> 1;
//     size_t corrected_v = v >> 1;
//     size_t corrected_width = width >> 1;

//     Vector3<uint16_t> color;
//     size_t indexUV =  corrected_v * corrected_width + corrected_u;
//     // Y
//     color[0] = attributeMap[v * width + u]; 
//     // U
//     color[1] = attributeMap[offsetU + indexUV]; 
//     // V
//     color[2] = attributeMap[offsetV + indexUV];
//     return color;    
// }

Vector3<uint16_t> get_attribute(
    const std::vector<uint16_t>& attributeMap, 
    const size_t& offsetU,
    const size_t& offsetV,
    const size_t& width, 
    const size_t u, const size_t v
) {
    const size_t index = v * width + u;
    Vector3<uint16_t> color;
    // Y
    color[0] = attributeMap[index]; 
    // U
    color[1] = attributeMap[offsetU + index]; 
    // V
    color[2] = attributeMap[offsetV + index];
    return color;    
}

/* ------------------------ ripped from tmc2------------------------ */
// void generate_points(
//     const uvgvpcc_dec::Patch& patch, 
//     const std::shared_ptr<uvgvpcc_dec::Frame> &frame,
//     const size_t& u, const size_t& v, 
//     const size_t& x, const size_t& y, 
//     const size_t& map_count, 
//     const bool& multipleStreams, const bool& absoluteD1_,
//     const size_t& geometry_map_width,
//     std::vector<uvgvpcc_dec::point3d>& points
// ) {
//     const size_t geoIndex = y*geometry_map_width + x;
//     auto& geoMap0 = frame->geometryMapL1;
//     // printf("geoMap0 size: %d\n", (int)geoMap0.size());

//     // First layer
//     uvgvpcc_dec::point3d point0 = patch.generatePoint( u, v, geoMap0[geoIndex] );
//     points.push_back(point0);

//     // Second layer if Double Layer is enabled, map_count = 2
//     //if ( map_count > 1 )
//     if (map_count == 2) {
//         uvgvpcc_dec::point3d point1( point0 );
//         auto& geoMap1 = multipleStreams ? frame->geometryMapL1 : frame->geometryMapL2;
//         if ( absoluteD1_ ) {
//             point1 = patch.generatePoint( u, v, geoMap1[geoIndex] );
//         } else {
//             if ( patch.projectionMode_ == 0 ) {
//                 point1.data_[patch.normalAxis_] += geoMap1[geoIndex];
//             } else {
//                 point1.data_[patch.normalAxis_] -= geoMap1[geoIndex];
//             }
//         }
//         points.push_back( point1 );
//     }
//     // return createdPoints;
// }

void generate_points(
    const uvgvpcc_dec::Patch& patch, 
    const std::vector<uint8_t>& geoMap1,
    const std::vector<uint8_t>& geoMap2,
    const size_t& u, const size_t& v, 
    const size_t& x, const size_t& y, 
    const size_t& map_count, 
    const bool& absoluteD1_,
    const size_t& geometry_map_width,
    std::vector<Vector3<typeGeometryInput>>& points
) {
    const size_t geoIndex = y*geometry_map_width + x;

    // First layer
    Vector3<typeGeometryInput> point0 = patch.generatePoint( u, v, geoMap1[geoIndex] );
    points.push_back(point0);

    // Second layer if Double Layer is enabled, map_count = 2
    //if ( map_count > 1 )
    if (map_count == 2) {
        Vector3<typeGeometryInput> point1( point0 );
        if ( absoluteD1_ ) {
            point1 = patch.generatePoint( u, v, geoMap2[geoIndex] );
        } else {
            if ( patch.projectionMode_ == 0 ) {
                point1[patch.normalAxis_] += geoMap2[geoIndex];
            } else {
                point1[patch.normalAxis_] -= geoMap2[geoIndex];
            }
        }
        // Push only the points from two maps are different
        if (point0 != point1) points.push_back( point1 );
    }
    // return createdPoints;
}

/* ------------------------ ripped from tmc2------------------------ */
int patchBlock_to_canvasBlock(
    const size_t& wInOccBlk, const size_t& hInOccBlk, 
    const size_t& blockToPatchWidth, const size_t& blockToPatchHeight,
    const uvgvpcc_dec::Patch& patch
) {
    size_t x, y;
    const size_t& u0_ = patch.omDSPosX_;
    const size_t& v0_ = patch.omDSPosY_;

    switch (patch.orientationIndex) {
    case PATCH_ORIENTATION_DEFAULT:
        x = wInOccBlk + u0_;
        y = hInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_ROT90:
        x = (patch.heightInOccBlk_ - 1 - hInOccBlk) + u0_;
        y = wInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_ROT180:
        x = (patch.widthInOccBlk_ - 1 - wInOccBlk) + u0_;
        y = (patch.heightInOccBlk_ - 1 - hInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = hInOccBlk + u0_;
        y = (patch.widthInOccBlk_ - 1 - wInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_MIRROR:
        x = (patch.widthInOccBlk_ - 1 - wInOccBlk) + u0_;
        y = hInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_MROT90:
        x = (patch.heightInOccBlk_ - 1 - hInOccBlk) + u0_;
        y = (patch.widthInOccBlk_ - 1 - wInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = wInOccBlk + u0_;
        y = (patch.heightInOccBlk_ - 1 - hInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = hInOccBlk + u0_;
        y = wInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_SWAP:
        x = hInOccBlk + u0_;
        y = wInOccBlk + v0_;
        break;
    default:
        return -1;
        break;
    }
    if (x >= blockToPatchWidth || y >= blockToPatchHeight) {return -1;}
    return int(y*blockToPatchWidth + x);
}

/* ------------------------ ripped from tmc2------------------------ */
int patch_to_canvas(
    const size_t& w, const size_t& h, 
    const size_t& asps_frame_width, const size_t& asps_frame_height,
    const uvgvpcc_dec::Patch& patch,
    size_t& x, size_t& y
) {
    const size_t& u0_ = patch.omDSPosX_;
    const size_t& v0_ = patch.omDSPosY_;
    const size_t& occupancyResolution = patch.occupancy_resolution;

    switch (patch.orientationIndex) {
    case PATCH_ORIENTATION_DEFAULT:
        x = w + u0_ * occupancyResolution;
        y = h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT90:
        // x = ( patch.heightInOccBlk_ * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        x = occupancyResolution*(patch.heightInOccBlk_ + u0_) - 1 - h;
        y = w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT180:
        // x = ( patch.widthInOccBlk_ * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        // y = ( patch.heightInOccBlk_ * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        x = occupancyResolution*(patch.widthInOccBlk_ + u0_) - 1 - w;
        y = occupancyResolution*(patch.heightInOccBlk_ + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = h + u0_ * occupancyResolution;
        // y = ( patch.widthInOccBlk_ * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        y = occupancyResolution*(patch.widthInOccBlk_ + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MIRROR:
        // x = ( patch.widthInOccBlk_ * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        x = occupancyResolution*(patch.widthInOccBlk_ + u0_) - 1 - w;
        y = h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_MROT90:
        // x = ( patch.heightInOccBlk_ * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        // y = ( patch.widthInOccBlk_ * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        x = occupancyResolution*(patch.heightInOccBlk_ + u0_) - 1 -h;
        y = occupancyResolution*(patch.widthInOccBlk_ + v0_) -1 - w;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = w + u0_ * occupancyResolution;
        // y = ( patch.heightInOccBlk_ * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        y = occupancyResolution*(patch.heightInOccBlk_ + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = h + u0_ * occupancyResolution;
        y = w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_SWAP:
        x = h + u0_ * occupancyResolution;
        y = w + v0_ * occupancyResolution;
        break;
    default:
        assert( 0 ); 
        break;
    }
    assert( (int)x >= 0 );
    assert( (int)y >= 0 );
    assert( x < asps_frame_width );
    assert( y < asps_frame_height );

    return (y*asps_frame_width + x);
}

void generateBlockToPatchFromOccupancyDSVideo(
    const size_t& blockToPatchWidth, 
    const size_t& blockToPatchHeight, 
    const size_t& asps_frame_width, 
    const size_t& asps_frame_height,
    const std::vector<uint8_t>& occupancyMapDS,
    const size_t& occupancyPrecision,
    const size_t& step_height,
    const std::vector<uvgvpcc_dec::Patch>& patchList,
    std::vector<size_t>& block_to_patch
) {
    for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
        const uvgvpcc_dec::Patch& patch = patchList[patch_index];
        for (size_t v0 = 0; v0 < patch.heightInOccBlk_; v0++) { // Block
            const size_t vBase = v0*patch.occupancy_resolution;
            for (size_t u0 = 0; u0 < patch.widthInOccBlk_; u0++) {
                size_t x, y;
                const size_t canvasIndex = patch_to_canvas(u0*patch.occupancy_resolution, vBase, asps_frame_width, asps_frame_height, patch, x, y);
                if (occupancyMapDS[y*step_height + x/occupancyPrecision] != 0) {
                    const size_t blockIndex = patchBlock_to_canvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                    block_to_patch[blockIndex] = patch_index + 1;
                }
            }
        } // Block

    } // Patch
}

} // anonymous namespace


// void Reconstruction::reconstructPointCloud(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_) {
//     const vps* vps = gof_->get_v3c_vps();

//     const size_t occupancyPrecision = vps->vps_frame_width_.at(0) / gofUVG->occupancy_map_width;
//     const size_t step_height = gofUVG->occupancy_map_width / occupancyPrecision;    
//     const size_t& asps_frame_width = gof_->get_v3c_atlas_context()->get_asps().asps_frame_width;
//     const size_t& asps_frame_height = gof_->get_v3c_atlas_context()->get_asps().asps_frame_height;
    
//     const size_t patch_packing_block_size = size_t( 1 ) << gof_->get_v3c_atlas_context()->get_asps().asps_log2_patch_packing_block_size;
//     const size_t blockToPatchWidth = asps_frame_width / patch_packing_block_size;
//     const size_t blockToPatchHeight = asps_frame_height / patch_packing_block_size;
//     const size_t blockCount = blockToPatchWidth * blockToPatchHeight;

//     const bool& asps_patch_precedence_order_flag = gof_->get_v3c_atlas_context()->get_asps().asps_patch_precedence_order_flag;

//     const size_t layer_count = vps->vps_map_count_minus1_.at(0) + 1;

//     bool multipleStreams_ = vps->vps_multiple_map_streams_present_flag_.at(0);
//     bool absoluteD1_ = layer_count == 1 || vps->vps_map_absolute_coding_enabled_flag_.at(0).at(1);

//     const size_t& geometry_map_width = gofUVG->geometry_map_width;
//     const size_t& geo_bit_depth_3d = vps->geometry_info_.at(0).gi_geometry_2d_bit_depth_minus1 + 1;

//     const size_t& attribute_map_width = gofUVG->attribute_map_width;
//     const size_t offsetU = attribute_map_width * gofUVG->attribute_map_height;
//     const size_t offsetV = offsetU + (offsetU >> 2U);

//     //size_t frameId = gofUVG->gofId * gofUVG->gofCount;
//     for (auto &frame : gofUVG->frames) {
//         const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;

//         std::vector<size_t> block_to_patch = {};
//         block_to_patch.resize(blockCount, 0);
//         const std::vector<Patch>& patchList = frame->patchList;

//         generateBlockToPatchFromOccupancyDSVideo(
//             blockToPatchWidth, 
//             blockToPatchHeight, 
//             asps_frame_width, 
//             asps_frame_height, 
//             occupancyMapDS,
//             occupancyPrecision,
//             step_height,
//             patchList, 
//             block_to_patch
//         );

//         const auto& geoMap1 = frame->geometryMapL1;
//         const auto& geoMap2 = multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

//         // const auto& attributeMap1 = frame->attributeMapL1;
//         // const auto& attributeMap2 = multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2;

//         auto* attributeMaps = &frame->attributeMapL1;
//         if (layer_count > 1) attributeMaps[1] = (multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);

//         for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
//             const size_t patch_index_tmp = asps_patch_precedence_order_flag ? (patchList.size() - patch_index - 1) : patch_index;
//             const uvgvpcc_dec::Patch& patch = patchList[patch_index_tmp];
//             const size_t patch_index_plus_1 = patch_index_tmp + 1;

//             for (size_t v0 = 0; v0 < patch.heightInOccBlk_; v0++) { // Block
//                 const size_t vBase = v0*patch.occupancy_resolution;
//                 for (size_t u0 = 0; u0 < patch.widthInOccBlk_; u0++) {
//                     const size_t blockIndex = patchBlock_to_canvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
//                     if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
//                         continue;
//                     }
//                     const size_t uBase = u0 * patch.occupancy_resolution;
//                     size_t xBase, yBase;
//                     patch_to_canvas(uBase, vBase, asps_frame_width, asps_frame_height, patch, xBase, yBase);
//                     for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
//                         for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
//                             const size_t x = xBase+u1;
//                             const size_t y = yBase+v1;

//                             std::vector<Vector3<typeGeometryInput>> pointsGeometry;
//                             generate_points(patch, geoMap1, geoMap2, uBase+u1, vBase+v1, x, y, layer_count, absoluteD1_, geometry_map_width, pointsGeometry);
//                             /*
//                                 Size of generated points is 2 for Double layer,
//                                 Size of generated points is 1 for Single layer
//                             */
//                             // for (size_t i = 0; i < layer_count; i++) { // get pointsGeometry and color the points
//                             //     if ( (i==0) || (pointsGeometry[i] != pointsGeometry[0]) ) {
//                             //         if (patch.axisOfAdditionalPlane_ == 0) {
//                             //             frame->pointsGeometry.push_back(pointsGeometry[i]);
//                             //         } else {
//                             //             Vector3<typeGeometryInput> tmp;
//                             //             inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, geo_bit_depth_3d, pointsGeometry[i], tmp);
//                             //             frame->pointsGeometry.push_back(tmp);
//                             //         }
//                             //         // Get pointsAttribute, color the points
//                             //         if (p_->useTMC2AttributeYUVConversion) {

//                             //         } else {
//                             //             if (i == 0) {
//                             //                 frame->pointsAttribute.push_back(get_attribute(attributeMap1, offsetU, offsetV, attribute_map_width, x, y));
//                             //             } else {
//                             //                 frame->pointsAttribute.push_back(get_attribute(attributeMap2, offsetU, offsetV, attribute_map_width, x, y));
//                             //             }
//                             //         }
//                             //     }
//                             // } // get pointsGeometry and color the points

//                             for (size_t i = 0; i < pointsGeometry.size(); i++) { // get pointsGeometry and color the points
//                                 if (patch.axisOfAdditionalPlane_ == 0) {
//                                     frame->pointsGeometry.push_back(pointsGeometry[i]);
//                                 } else {
//                                     Vector3<typeGeometryInput> tmp;
//                                     inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, geo_bit_depth_3d, pointsGeometry[i], tmp);
//                                     frame->pointsGeometry.push_back(tmp);
//                                 }
//                                 // Get pointsAttribute, color the points
//                                 if (p_->useTMC2AttributeYUVConversion) {
                                    
//                                 } else {
//                                     frame->pointsAttribute.push_back(get_attribute(attributeMaps[i], offsetU, offsetV, attribute_map_width, x, y));
//                                 }
//                             } // get pointsGeometry and color the points
//                         }
//                     } // patch.occupancy_resolution
//                 }
//             } // Block
//         } // Patch
//         // printf("pointsPosList size: %d\n", (int)frame->pointsGeometry.size());
//         // printf("pointPixeList size: %d\n", (int)frame->pointsAttribute.size());
//         frame->pointCount = frame->pointsGeometry.size();
//         // frame->frameId = frameId++;
//     } // Frame

    
// }


void Reconstruction::reconstructPointCloud(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_) {
    const vps* vps = gof_->get_v3c_vps();

    const size_t occupancyPrecision = vps->vps_frame_width_.at(0) / gofUVG->occupancy_map_width;
    const size_t step_height = gofUVG->occupancy_map_width / occupancyPrecision;    
    const size_t& asps_frame_width = gof_->get_v3c_atlas_context()->get_asps().asps_frame_width;
    const size_t& asps_frame_height = gof_->get_v3c_atlas_context()->get_asps().asps_frame_height;
    
    const size_t patch_packing_block_size = size_t( 1 ) << gof_->get_v3c_atlas_context()->get_asps().asps_log2_patch_packing_block_size;
    const size_t blockToPatchWidth = asps_frame_width / patch_packing_block_size;
    const size_t blockToPatchHeight = asps_frame_height / patch_packing_block_size;
    const size_t blockCount = blockToPatchWidth * blockToPatchHeight;

    const bool& asps_patch_precedence_order_flag = gof_->get_v3c_atlas_context()->get_asps().asps_patch_precedence_order_flag;

    const size_t layer_count = vps->vps_map_count_minus1_.at(0) + 1;

    bool multipleStreams_ = vps->vps_multiple_map_streams_present_flag_.at(0);
    bool absoluteD1_ = layer_count == 1 || vps->vps_map_absolute_coding_enabled_flag_.at(0).at(1);

    const size_t& geometry_map_width = gofUVG->geometry_map_width;
    const size_t& geo_bit_depth_3d = vps->geometry_info_.at(0).gi_geometry_2d_bit_depth_minus1 + 1;

    const size_t& attribute_map_width = gofUVG->attribute_map_width;
    const size_t offsetU = attribute_map_width * gofUVG->attribute_map_height;
    const size_t offsetV = p_->useTMC2AttributeYUVConversion? (offsetU << 1U) : offsetU + (offsetU >> 2U);

    //size_t frameId = gofUVG->gofId * gofUVG->gofCount;
    for (auto &frame : gofUVG->frames) {
        const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;

        std::vector<size_t> block_to_patch = {};
        block_to_patch.resize(blockCount, 0);
        const std::vector<Patch>& patchList = frame->patchList;

        generateBlockToPatchFromOccupancyDSVideo(
            blockToPatchWidth, 
            blockToPatchHeight, 
            asps_frame_width, 
            asps_frame_height, 
            occupancyMapDS,
            occupancyPrecision,
            step_height,
            patchList, 
            block_to_patch
        );

        const auto& geoMap1 = frame->geometryMapL1;
        const auto& geoMap2 = multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

        // const auto& attributeMap1 = frame->attributeMapL1;
        // const auto& attributeMap2 = multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2;
        
        // auto* attributeMaps = &frame->attributeMapL1;
        // // if (layer_count > 1) attributeMaps[1] = (multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);
        // if (layer_count > 1) {
        //     attributeMaps[1] = (multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);
        // } 

        auto* attributeMaps = &frame->attributeMapL1;
        auto* attributeMaps_16bits = &frame->attributeMapL1_16bits;
        if (layer_count > 1) {
            attributeMaps[1] = (multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);
            attributeMaps_16bits[1] = (multipleStreams_ ? frame->attributeMapL1_16bits : frame->attributeMapL2_16bits);
        } 


        for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
            const size_t patch_index_tmp = asps_patch_precedence_order_flag ? (patchList.size() - patch_index - 1) : patch_index;
            const uvgvpcc_dec::Patch& patch = patchList[patch_index_tmp];
            const size_t patch_index_plus_1 = patch_index_tmp + 1;

            for (size_t v0 = 0; v0 < patch.heightInOccBlk_; v0++) { // Block
                for (size_t u0 = 0; u0 < patch.widthInOccBlk_; u0++) {
                    const size_t blockIndex = patchBlock_to_canvasBlock(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                    if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
                        continue;
                    }
                    
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
                        const size_t v = v0 * patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * patch.occupancy_resolution + u1;
                            size_t x;
                            size_t y;
                            patch_to_canvas(u, v, asps_frame_width, asps_frame_height, patch, x, y);
                            std::vector<Vector3<typeGeometryInput>> pointsGeometry;
                            generate_points(patch, geoMap1, geoMap2, u, v, x, y, layer_count, absoluteD1_, geometry_map_width, pointsGeometry);
                            /*
                                Size of generated points is 2 for Double layer,
                                Size of generated points is 1 for Single layer
                            */
                            for (size_t i = 0; i < pointsGeometry.size(); i++) { // get pointsGeometry and color the points
                                if (patch.axisOfAdditionalPlane_ == 0) {
                                    frame->pointsGeometry.push_back(pointsGeometry[i]);
                                } else {
                                    Vector3<typeGeometryInput> tmp;
                                    inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, geo_bit_depth_3d, pointsGeometry[i], tmp);
                                    frame->pointsGeometry.push_back(tmp);
                                }
                                // Get pointsAttribute, color the points
                                if (p_->useTMC2AttributeYUVConversion) {
                                    frame->pointsAttribute16bits.push_back(get_attribute(attributeMaps_16bits[i], offsetU, offsetV, attribute_map_width, x, y));
                                } else {
                                    frame->pointsAttribute.push_back(get_attribute(attributeMaps[i], offsetU, offsetV, attribute_map_width, x, y));
                                }
                                // frame->pointsAttribute.push_back(get_attribute(attributeMaps[i], offsetU, offsetV, attribute_map_width, x, y));
                            } // get pointsGeometry and color the points
                        }
                    } // patch.occupancy_resolution
                }
            } // Block
        } // Patch
        frame->pointCount = frame->pointsGeometry.size();
        // frame->frameId = frameId++;
    } // Frame

    
}