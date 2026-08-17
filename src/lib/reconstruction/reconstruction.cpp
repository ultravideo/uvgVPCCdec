#include "reconstruction.hpp"
#include "bitstreamParsing/vps.hpp"

#include "geometry_smoothing.hpp"

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

static inline Vector3<uint8_t> get_attribute(
    const std::vector<uint8_t>& attributeMap, 
    const size_t offsetU,
    const size_t offsetV,
    const size_t width, 
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

static inline Vector3<uint16_t> get_attribute(
    const std::vector<uint16_t>& attributeMap, 
    const size_t offsetU,
    const size_t offsetV,
    const size_t width, 
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

void generate_points_(
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
        // Push only the points from two maps that are different
        if (point0 != point1) points.push_back( point1 );
    }
    // return createdPoints;
}

size_t generate_points(
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

    size_t point_count = 1;

    // First layer
    Vector3<typeGeometryInput> point0 = patch.generatePoint( u, v, geoMap1[geoIndex] );
    points[0] = point0;

    // Second layer if Double Layer is enabled, map_count = 2
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
        // Push only the points from two maps that are different
        if (point0 != point1) {
            points[point_count++] = point1;
        }
    }
    return point_count;
    // return createdPoints;
}

/* ------------------------ ripped from tmc2------------------------ */
static inline size_t patchBlock_to_canvasBlock_tmc2(
    const size_t& wInOccBlk, const size_t& hInOccBlk, 
    const size_t& blockToPatchWidth, const size_t& blockToPatchHeight,
    const uvgvpcc_dec::Patch& patch
) {
    size_t x, y;
    const size_t u0_ = patch.patchPosXCanvasBlock; // patch pos x in canvas block
    const size_t v0_ = patch.patchPosYCanvasBlock; // patch pos y in canvas block

    switch (patch.orientationIndex) {
    case PATCH_ORIENTATION_DEFAULT:
        x = wInOccBlk + u0_;
        y = hInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_ROT90:
        x = (patch.patchHeightCanvasBlock - 1 - hInOccBlk) + u0_;
        y = wInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_ROT180:
        x = (patch.patchWidthCanvasBlock - 1 - wInOccBlk) + u0_;
        y = (patch.patchHeightCanvasBlock - 1 - hInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = hInOccBlk + u0_;
        y = (patch.patchWidthCanvasBlock - 1 - wInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_MIRROR:
        x = (patch.patchWidthCanvasBlock - 1 - wInOccBlk) + u0_;
        y = hInOccBlk + v0_;
        break;
    case PATCH_ORIENTATION_MROT90:
        x = (patch.patchHeightCanvasBlock - 1 - hInOccBlk) + u0_;
        y = (patch.patchWidthCanvasBlock - 1 - wInOccBlk) + v0_;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = wInOccBlk + u0_;
        y = (patch.patchHeightCanvasBlock - 1 - hInOccBlk) + v0_;
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
    return (y*blockToPatchWidth + x);
}

/* ------------------------ ripped from tmc2------------------------ */
size_t patch_to_canvas_tmc2(
    const size_t w, const size_t h, 
    const size_t tile_width, const size_t tile_height,
    const uvgvpcc_dec::Patch& patch,
    size_t& x, size_t& y
) {
    const size_t u0_ = patch.patchPosXCanvasBlock; // patch pos x in canvas block
    const size_t v0_ = patch.patchPosYCanvasBlock; // patch pos y in canvas block
    const size_t occupancyResolution = patch.occupancy_resolution;

    switch (patch.orientationIndex) {
    case PATCH_ORIENTATION_DEFAULT:
        x = w + u0_ * occupancyResolution;
        y = h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT90:
        // x = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        x = occupancyResolution*(patch.patchHeightCanvasBlock + u0_) - 1 - h;
        y = w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT180:
        // x = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        // y = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        x = occupancyResolution*(patch.patchWidthCanvasBlock + u0_) - 1 - w;
        y = occupancyResolution*(patch.patchHeightCanvasBlock + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = h + u0_ * occupancyResolution;
        // y = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        y = occupancyResolution*(patch.patchWidthCanvasBlock + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MIRROR:
        // x = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        x = occupancyResolution*(patch.patchWidthCanvasBlock + u0_) - 1 - w;
        y = h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_MROT90:
        // x = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        // y = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        x = occupancyResolution*(patch.patchHeightCanvasBlock + u0_) - 1 - h;
        y = occupancyResolution*(patch.patchWidthCanvasBlock + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = w + u0_ * occupancyResolution;
        // y = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        y = occupancyResolution*(patch.patchHeightCanvasBlock + v0_) - 1 - h;
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
    // assert( (int)x >= 0 );
    // assert( (int)y >= 0 );
    // assert( x < tile_width );
    // assert( y < tile_height );

    return(y*tile_width + x);
}


static inline size_t patch_to_canvas(
    const size_t w, const size_t h, 
    const size_t tile_width,
    const uvgvpcc_dec::Patch& patch,
    size_t& x, size_t& y
) {
    // const size_t u0_ = patch.patchPosXCanvasBlock; // patch pos x in canvas block
    // const size_t v0_ = patch.patchPosYCanvasBlock; // patch pos y in canvas block
    // const size_t occupancyResolution = patch.occupancy_resolution;

    switch (patch.orientationIndex) {
    case PATCH_ORIENTATION_DEFAULT:
        x = w + patch.patchPosXCanvas; // w + u0_ * occupancyResolution;
        y = h + patch.patchPosYCanvas; // h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT90:
        // x = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        x = patch.patchHeightCanvas + patch.patchPosXCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + u0_) - 1 - h;
        y = w + patch.patchPosYCanvas; // w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT180:
        // x = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        // y = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        x = patch.patchWidthCanvas + patch.patchPosXCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + u0_) - 1 - w;
        y = patch.patchHeightCanvas + patch.patchPosYCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = h + patch.patchPosXCanvas; // h + u0_ * occupancyResolution;
        // y = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        y = patch.patchWidthCanvas + patch.patchPosYCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MIRROR:
        // x = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        x = patch.patchWidthCanvas + patch.patchPosXCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + u0_) - 1 - w;
        y = h + patch.patchPosYCanvas; // h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_MROT90:
        // x = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        // y = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        x = patch.patchHeightCanvas + patch.patchPosXCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + u0_) - 1 -h;
        y = patch.patchWidthCanvas + patch.patchPosYCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = w + patch.patchPosXCanvas; // w + u0_ * occupancyResolution;
        // y = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        y = patch.patchHeightCanvas + patch.patchPosYCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = h + patch.patchPosXCanvas; // h + u0_ * occupancyResolution;
        y = w + patch.patchPosYCanvas; // w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_SWAP:
        x = h + patch.patchPosXCanvas; // h + u0_ * occupancyResolution;
        y = w + patch.patchPosYCanvas; // w + v0_ * occupancyResolution;
        break;
    default:
        assert( 0 ); 
        break;
    }
    
    return(y*tile_width + x);
}


static inline void patch_to_canvas(
    const size_t w, const size_t h, 
    const uvgvpcc_dec::Patch& patch,
    size_t& x, size_t& y
) {
    // const size_t u0_ = patch.patchPosXCanvasBlock; // patch pos x in canvas block
    // const size_t v0_ = patch.patchPosYCanvasBlock; // patch pos y in canvas block
    // const size_t occupancyResolution = patch.occupancy_resolution;

    switch (patch.orientationIndex) {
    case PATCH_ORIENTATION_DEFAULT:
        x = w + patch.patchPosXCanvas; // w + u0_ * occupancyResolution;
        y = h + patch.patchPosYCanvas; // h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT90:
        // x = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        x = patch.patchHeightCanvas + patch.patchPosXCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + u0_) - 1 - h;
        y = w + patch.patchPosYCanvas; // w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_ROT180:
        // x = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        // y = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        x = patch.patchWidthCanvas + patch.patchPosXCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + u0_) - 1 - w;
        y = patch.patchHeightCanvas + patch.patchPosYCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_ROT270:
        x = h + patch.patchPosXCanvas; // h + u0_ * occupancyResolution;
        // y = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        y = patch.patchWidthCanvas + patch.patchPosYCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MIRROR:
        // x = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + u0_ * occupancyResolution;
        x = patch.patchWidthCanvas + patch.patchPosXCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + u0_) - 1 - w;
        y = h + patch.patchPosYCanvas; // h + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_MROT90:
        // x = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + u0_ * occupancyResolution;
        // y = ( patch.patchWidthCanvasBlock * occupancyResolution - 1 - w ) + v0_ * occupancyResolution;
        x = patch.patchHeightCanvas + patch.patchPosXCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + u0_) - 1 -h;
        y = patch.patchWidthCanvas + patch.patchPosYCanvas - 1 - w; // occupancyResolution*(patch.patchWidthCanvasBlock + v0_) - 1 - w;
        break;
    case PATCH_ORIENTATION_MROT180:
        x = w + patch.patchPosXCanvas; // w + u0_ * occupancyResolution;
        // y = ( patch.patchHeightCanvasBlock * occupancyResolution - 1 - h ) + v0_ * occupancyResolution;
        y = patch.patchHeightCanvas + patch.patchPosYCanvas - 1 - h; // occupancyResolution*(patch.patchHeightCanvasBlock + v0_) - 1 - h;
        break;
    case PATCH_ORIENTATION_MROT270:
        x = h + patch.patchPosXCanvas; // h + u0_ * occupancyResolution;
        y = w + patch.patchPosYCanvas; // w + v0_ * occupancyResolution;
        break;
    case PATCH_ORIENTATION_SWAP:
        x = h + patch.patchPosXCanvas; // h + u0_ * occupancyResolution;
        y = w + patch.patchPosYCanvas; // w + v0_ * occupancyResolution;
        break;
    default:
        assert( 0 ); 
        break;
    }
}


/*
    enhancedOccupancyMapForDepthFlag
    Single Tile in Atlas Frame
*/
void generateOccupancyMap(
    std::vector<uint8_t>& occupancyMap,
    const std::vector<uint8_t>& occupancyMapDS,  
    const size_t occupancyMapDS_width,
    const size_t occupancyPrecision, 
    const size_t atlas_frame_width, // atlas_frame_width
    const size_t atlas_frame_height // atlas_frame_height
) {
    occupancyMap.resize(atlas_frame_width * atlas_frame_height, 0);
    for (size_t v = 0; v < atlas_frame_height; v++) {
        size_t column_offset = v * atlas_frame_width;
        size_t occupancyMapDS_offset = (v/occupancyPrecision) * occupancyMapDS_width;
        for (size_t u = 0; u < atlas_frame_width; u++) {
            uint8_t pixel = occupancyMapDS[occupancyMapDS_offset + (u/occupancyPrecision)];
            occupancyMap[column_offset + u] = pixel;
        }
    }
}

void generateOccupancyMap(
    std::vector<uint8_t>& occupancyMap,
    const std::vector<uint8_t>& occupancyMapDS,  
    const size_t occupancyMapDS_width,
    const size_t occupancyPrecision, 
    const size_t atlas_frame_width, // atlas_frame_width
    const size_t atlas_frame_height, // atlas_frame_height
    size_t& num_occupied_points
) {
    occupancyMap.resize(atlas_frame_width * atlas_frame_height, 0);
    for (size_t v = 0; v < atlas_frame_height; v++) {
        size_t column_offset = v * atlas_frame_width;
        size_t occupancyMapDS_offset = (v/occupancyPrecision) * occupancyMapDS_width;
        for (size_t u = 0; u < atlas_frame_width; u++) {
            uint8_t pixel = occupancyMapDS[occupancyMapDS_offset + (u/occupancyPrecision)];
            occupancyMap[column_offset + u] = pixel;
            num_occupied_points += (pixel != 0U);
        }
    }
}

void generateBlockToPatchFromOccupancyMapVideo(
    const size_t& blockToPatchWidth, 
    const size_t& blockToPatchHeight, 
    const size_t& asps_frame_width, 
    const size_t& asps_frame_height,
    const std::vector<uint8_t>& occupancyMap,
    const size_t& occupancyMap_width,
    const size_t& occupancyPrecision,
    const std::vector<uvgvpcc_dec::Patch>& patchList,
    std::vector<size_t>& block_to_patch
) {
    for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
        const uvgvpcc_dec::Patch& patch = patchList[patch_index];
        size_t nonZeroPixel = 0;
        for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
            const size_t vBase = v0*patch.occupancy_resolution;
            for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                const size_t uBase = u0*patch.occupancy_resolution;
                nonZeroPixel = 0;
                for (size_t v1 = 0; v1 < patch.occupancy_resolution; v1++) {
                    const size_t v = vBase + v1;
                    for (size_t u1 = 0; u1 < patch.occupancy_resolution; u1++) {
                        const size_t u = uBase + u1;
                        size_t x, y;
                        patch_to_canvas_tmc2(u, v, asps_frame_width, asps_frame_height, patch, x, y);
                        nonZeroPixel += static_cast<unsigned long long>(occupancyMap[((y/occupancyPrecision) * occupancyMap_width) + (x/occupancyPrecision)] != 0U);
                    }
                }
                if (nonZeroPixel > 0) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                    block_to_patch[blockIndex] = patch_index + 1;
                }
            }
        } // Block

    } // Patch
}

void generateBlockToPatchFromOccupancyMapVideo(
    const size_t& blockToPatchWidth, 
    const size_t& blockToPatchHeight, 
    const size_t& asps_frame_width, 
    const std::vector<uint8_t>& occupancyMap,
    const std::vector<uvgvpcc_dec::Patch>& patchList,
    std::vector<size_t>& block_to_patch
) {
    for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
        const uvgvpcc_dec::Patch& patch = patchList[patch_index];
        for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
            const size_t vBase = v0*patch.occupancy_resolution;
            for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                const size_t uBase = u0*patch.occupancy_resolution;
                size_t nonZeroPixel = 0;
                for (size_t v1 = 0; v1 < patch.occupancy_resolution; v1++) {
                    const size_t v = vBase + v1;
                    for (size_t u1 = 0; u1 < patch.occupancy_resolution; u1++) {
                        const size_t u = uBase + u1;
                        size_t x, y;
                        size_t canvasIndex = patch_to_canvas(u, v, asps_frame_width, patch, x, y);
                        // patch_to_canvas(u, v, patch, x, y);
                        nonZeroPixel += static_cast<unsigned long long>(occupancyMap[canvasIndex] != 0U);
                    }
                }
                if (nonZeroPixel > 0) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                    block_to_patch[blockIndex] = patch_index + 1;
                }
            }
        } // Block

    } // Patch
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
        for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
            const size_t vBase = v0*patch.occupancy_resolution;
            for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                size_t x, y;
                const size_t canvasIndex = patch_to_canvas_tmc2(u0*patch.occupancy_resolution, vBase, asps_frame_width, asps_frame_height, patch, x, y);
                if (occupancyMapDS[y*step_height + x/occupancyPrecision] != 0) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                    block_to_patch[blockIndex] = patch_index + 1;
                }
            }
        } // Block

    } // Patch
}

void set_reconstruction_parameters(
    std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof>& gof_, 
    reconstruction_options& rec_opts, sei_reconstruction_info& sei_rec_info,
    common_reconstruction_parameters& rec_params, other_reconstruction_parameters& other_rec_params
) {
    
    const vps* vps = gof_->get_v3c_vps();
    const atlas_sequence_parameter_set& asps = gof_->get_v3c_atlas_context()->get_asps();
    
    // General Reconstruction Options
    rec_opts.setReconstructionParameters(vps->ptl_.ptl_profile_reconstruction_idc);
    
    std::vector<atlas_tile_layer_rbsp> &atlas_data = gof_->get_v3c_atlas_context()->get_atlases();
    /* Set SEI reconstruction info 
        Assuming the same SEI message is applied to all the frames within a GOF
    */
    int sei_idx;
    if (rec_opts.applyAttrSmoothingType_ != 0 && atlas_data.at(0).sei_.prefix_sei_is_present(GEOMETRY_SMOOTHING, sei_idx)) {
        sei_geometry_smoothing* sei_geo_smoothing = static_cast<sei_geometry_smoothing*>(atlas_data.at(0).sei_.sei_prefix.at(sei_idx).get());
        for (size_t i = 0; i < sei_geo_smoothing->instancesUpdated; i++) {
            size_t k = sei_geo_smoothing->instanceIndices.at(i);
            if (!sei_geo_smoothing->instanceCancelFlags.at(k)) {
                sei_rec_info.geometry_smoothing_flag = true;
                if (sei_geo_smoothing->methodTypes.at(k) == 1) {
                    sei_rec_info.grid_smoothing      = true;
                    sei_rec_info.grid_size           = sei_geo_smoothing->gridSizeMinus2s.at(k) + 2;
                    sei_rec_info.threshold_smoothing = static_cast<double>(sei_geo_smoothing->thresholds.at(k));
                }
            }
        }
    }
    if (rec_opts.applyOccupanySynthesisType_ != 0 && atlas_data.at(0).sei_.prefix_sei_is_present(OCCUPANCY_SYNTHESIS, sei_idx)) {
        sei_occupancy_synthesis* sei_occ_synthesis = static_cast<sei_occupancy_synthesis*>(atlas_data.at(0).sei_.sei_prefix.at(sei_idx).get());
        for (size_t i = 0; i < sei_occ_synthesis->instancesUpdated; i++) {
            size_t k = sei_occ_synthesis->instanceIndices.at(i);
            if (!sei_occ_synthesis->instanceCancelFlags.at(k)) {
                sei_rec_info.geometry_smoothing_flag = true;
                if (sei_occ_synthesis->methodTypes.at(k) == 1) {
                    sei_rec_info.pbf_enable_flag    = true;
                    sei_rec_info.pbf_passes_count   = sei_occ_synthesis->pbfPassesCountMinus1.at(k) + 1;
                    sei_rec_info.pbf_filter_size    = sei_occ_synthesis->pbfFilterSizeMinus1.at(k) + 1;
                    sei_rec_info.pbf_log2_threshold = sei_occ_synthesis->pbfLog2ThresholdMinus1.at(k) + 1;
                }
            }
        }
    }
    if (rec_opts.applyAttrSmoothingType_ != 0 && atlas_data.at(0).sei_.prefix_sei_is_present(ATTRIBUTE_SMOOTHING, sei_idx)) {

    }
    /*---------------------------------*/

    /* Specific reconstruction paramers */
    other_rec_params.surface_thickness             = asps.asps_vpcc_surface_thickness_minus1 + 1;
    other_rec_params.threshold_lossy_om            = static_cast<size_t>(vps->occupancy_info_.at(0).oi_lossy_occupancy_compression_threshold);
    other_rec_params.enable_size_quantization      = asps.asps_patch_size_quantizer_present_flag; 
    other_rec_params.remove_duplicate_points       = rec_opts.duplicatedPointRemovalType_ != 0 && asps.asps_vpcc_remove_duplicate_point_enabled_flag;
    other_rec_params.point_local_reconstruction    = rec_opts.pointLocalReconstructionType_ != 0 && asps.asps_plr_enabled_flag;
    other_rec_params.single_map_pixel_interleaving = rec_opts.pixelDeinterleavingType_ != 0 && asps.asps_pixel_deinterleaving_enabled_flag;
    other_rec_params.use_additional_points_patch   =  rec_opts.reconstructRawType_ != 0 && asps.asps_raw_patch_enabled_flag;
    other_rec_params.use_aux_seperate_video        = asps.asps_auxiliary_video_enabled_flag;
    other_rec_params.enhanced_occupancy_map_code   = rec_opts.reconstructEomType_ != 0 && asps.asps_eom_patch_enabled_flag;
    other_rec_params.EOM_fix_bit_count             = asps.asps_eom_fix_bit_count_minus1 + 1;
    /*---------------------------------*/


    // Set common reconstruction paramers
    const atlas_frame_tile_information& afti = gof_->get_v3c_atlas_context()->get_afps().afti;
    rec_params.num_tiles_in_atlas_frame         = gof_->get_v3c_atlas_context()->get_afps().afti.afti_num_tiles_in_atlas_frame_minus1 + 1;
    // Check number of tiles in Atlas frame
    printf("---------------> numPartitionCols : %d, numPartitionRows : %d\n", 
        afti.afti_num_partition_columns_minus1 + 1, afti.afti_num_partition_rows_minus1 + 1);
    if (gof_->get_v3c_atlas_context()->get_afps().afti.afti_single_tile_in_atlas_frame_flag) {
        rec_params.tile_width = asps.asps_frame_width;
        rec_params.tile_height = asps.asps_frame_height;
    } else { // Multiple-tiles within an Atlas frame
        throw std::runtime_error("Multiple-tiles within an Atlas frame -> Not Yet Implemented\n");

    }
    rec_params.occupancyPrecision               = vps->vps_frame_width_.at(0) / gofUVG->occupancy_map_width;
    rec_params.patch_packing_block_size         = size_t( 1 ) << asps.asps_log2_patch_packing_block_size;
    rec_params.blockToPatchWidth                = rec_params.tile_width / rec_params.patch_packing_block_size;
    rec_params.blockToPatchHeight               = rec_params.tile_height / rec_params.patch_packing_block_size;
    rec_params.blockCount                       = rec_params.blockToPatchWidth * rec_params.blockToPatchHeight;
    rec_params.layer_count                      = vps->vps_map_count_minus1_.at(0) + 1;
    rec_params.asps_patch_precedence_order_flag = asps.asps_patch_precedence_order_flag;
    rec_params.multipleStreams_                 = vps->vps_multiple_map_streams_present_flag_.at(0);
    rec_params.absoluteD1_                      = rec_params.layer_count == 1 || vps->vps_map_absolute_coding_enabled_flag_.at(0).at(1);
    rec_params.geo_map_width                    = gofUVG->geometry_map_width;
    rec_params.geo_bit_depth_3d                 = vps->geometry_info_.at(0).gi_geometry_3d_coordinates_bit_depth_minus1 + 1;
    rec_params.attribute_map_width              = gofUVG->attribute_map_width;
    rec_params.offsetU                          = rec_params.attribute_map_width * gofUVG->attribute_map_height;
    rec_params.offsetV                          = p_->useTMC2AttributeYUVConversion ? (rec_params.offsetU << 1U) : rec_params.offsetU + (rec_params.offsetU >> 2U);
}


/* Single tile and no SEI applied -> fast reconstruction */
void reconstruct_singleTile_noSei_(
    std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_,
    reconstruction_options& rec_opts,
    common_reconstruction_parameters& rec_params
) {
    (void)rec_opts;
    int frameId = 0;
    std::vector<size_t> block_to_patch(rec_params.blockCount);
    for (auto &frame : gofUVG->frames) {    
        std::fill(block_to_patch.begin(), block_to_patch.end(), 0);

        const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;
        std::vector<uint8_t>& occupancyMap = frame->occupancyMap;
        const std::vector<Patch>& patchList = frame->patchList;

        generateOccupancyMap(
            occupancyMap, 
            occupancyMapDS, 
            gofUVG->occupancy_map_width, 
            rec_params.occupancyPrecision, 
            rec_params.tile_width, 
            rec_params.tile_height 
        );

        generateBlockToPatchFromOccupancyMapVideo(
            rec_params.blockToPatchWidth, 
            rec_params.blockToPatchHeight, 
            rec_params.tile_width, 
            rec_params.tile_height, 
            occupancyMapDS,
            gofUVG->occupancy_map_width,
            rec_params.occupancyPrecision,
            patchList, 
            block_to_patch
        );

        const auto& geoMap1 = frame->geometryMapL1;
        const auto& geoMap2 = rec_params.multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

        auto* attributeMaps = &frame->attributeMapL1;
        auto* attributeMaps_16bits = &frame->attributeMapL1_16bits;
        if (rec_params.layer_count > 1) {
            attributeMaps[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);
            attributeMaps_16bits[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1_16bits : frame->attributeMapL2_16bits);
        } 

        for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
            const size_t patch_index_tmp = rec_params.asps_patch_precedence_order_flag ? (patchList.size() - patch_index - 1) : patch_index;
            const uvgvpcc_dec::Patch& patch = patchList[patch_index_tmp];
            const size_t patch_index_plus_1 = patch_index_tmp + 1;

            // printf("patch %zu, patch occupancy size = %zu, patch size = %zu\n", patch_index_plus_1, 
            //     patch.occupancy_resolution*patch.occupancy_resolution, patch.patchHeightCanvasBlock*patch.patchWidthCanvasBlock);
            
            // int skip_block_count = 0;
            for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
                for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, rec_params.blockToPatchWidth, rec_params.blockToPatchHeight, patch);
                    if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
                        // skip_block_count++;
                        continue;
                    }
                    // int occupancy_block_skip_count = 0;
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
                        const size_t v = v0 * patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * patch.occupancy_resolution + u1;
                            size_t x;
                            size_t y;
                            size_t canvasIndex = patch_to_canvas_tmc2(u, v, rec_params.tile_width, rec_params.tile_height, patch, x, y);
                            bool occupied = occupancyMap.at(canvasIndex) != 0;
                            if (!occupied) {
                                // occupancy_block_skip_count++;
                                continue;
                            }

                            std::vector<Vector3<typeGeometryInput>> pointsGeometry;
                            generate_points_(patch, geoMap1, geoMap2, u, v, x, y, rec_params.layer_count, rec_params.absoluteD1_, rec_params.geo_map_width, pointsGeometry);
                            /*
                                Size of generated points is 2 for Double layer,
                                Size of generated points is 1 for Single layer
                            */
                            for (size_t i = 0; i < pointsGeometry.size(); i++) { // get pointsGeometry and color the points
                                if (patch.axisOfAdditionalPlane_ == 0) {
                                    frame->pointsGeometry.push_back(pointsGeometry[i]);
                                } else {
                                    Vector3<typeGeometryInput> tmp;
                                    inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, rec_params.geo_bit_depth_3d, pointsGeometry[i], tmp);
                                    frame->pointsGeometry.push_back(tmp);
                                }
                                // Get pointsAttribute, color the points
                                if (p_->useTMC2AttributeYUVConversion) {
                                    frame->pointsAttribute16bits.push_back(get_attribute(attributeMaps_16bits[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                                } else {
                                    frame->pointsAttribute.push_back(get_attribute(attributeMaps[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                                }
                            } // get pointsGeometry and color the points
                        }
                        // printf("skip_block_count = %d, occupancy_block_skip_count = %d\n", skip_block_count, occupancy_block_skip_count);
                    } // patch.occupancy_resolution
                }
            } // Block
        } // Patch
        frame->pointCount = frame->pointsGeometry.size();
        // frameId++;
        // printf("Reconstruct frame %d, size %d of GOF %d\n", frameId, (int)frame->pointCount, (int)gofUVG->gofId);
    }
}

/* Single tile and no SEI applied -> fast reconstruction */
void reconstruct_singleTile_noSei(
    std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_,
    reconstruction_options& rec_opts,
    const common_reconstruction_parameters& rec_params
) {
    (void)rec_opts;
    int frameId = 0;
    std::vector<size_t> block_to_patch(rec_params.blockCount);
    std::vector<Vector3<typeGeometryInput>> pointsGeometry(2);
    for (auto &frame : gofUVG->frames) {    
        std::fill(block_to_patch.begin(), block_to_patch.end(), 0);

        const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;
        std::vector<uint8_t>& occupancyMap = frame->occupancyMap;
        const std::vector<Patch>& patchList = frame->patchList;
        
        size_t num_occupied_points = 0;

        generateOccupancyMap(
            occupancyMap, 
            occupancyMapDS, 
            gofUVG->occupancy_map_width, 
            rec_params.occupancyPrecision, 
            rec_params.tile_width, 
            rec_params.tile_height,
            num_occupied_points 
        );

        // generateBlockToPatchFromOccupancyMapVideo(
        //     rec_params.blockToPatchWidth, 
        //     rec_params.blockToPatchHeight, 
        //     rec_params.tile_width, 
        //     rec_params.tile_height, 
        //     occupancyMapDS,
        //     gofUVG->occupancy_map_width,
        //     rec_params.occupancyPrecision,
        //     patchList, 
        //     block_to_patch
        // );

        generateBlockToPatchFromOccupancyMapVideo(
            rec_params.blockToPatchWidth, 
            rec_params.blockToPatchHeight, 
            rec_params.tile_width, 
            occupancyMap,
            patchList, 
            block_to_patch
        );

        const auto& geoMap1 = frame->geometryMapL1;
        const auto& geoMap2 = rec_params.multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

        auto* attributeMaps = &frame->attributeMapL1;
        auto* attributeMaps_16bits = &frame->attributeMapL1_16bits;
        if (rec_params.layer_count > 1) {
            attributeMaps[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);
            attributeMaps_16bits[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1_16bits : frame->attributeMapL2_16bits);
        } 

        frame->pointsGeometry.reserve(num_occupied_points * rec_params.layer_count);
        if (p_->useTMC2AttributeYUVConversion) {
            frame->pointsAttribute16bits.reserve(num_occupied_points * rec_params.layer_count);
        } else {
            frame->pointsAttribute.reserve(num_occupied_points * rec_params.layer_count);
        }

        size_t layer_count = rec_params.layer_count;
        if (rec_params.layer_count == 2 && rec_params.multipleStreams_) {
            layer_count = 1;
        }
        
        for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
            const size_t patch_index_tmp = rec_params.asps_patch_precedence_order_flag ? (patchList.size() - patch_index - 1) : patch_index;
            const uvgvpcc_dec::Patch& patch = patchList[patch_index_tmp];
            const size_t patch_index_plus_1 = patch_index_tmp + 1;

            // printf("patch %zu, patch_packing_block_size = %zu, occupancy_resolution = %zu, patch pdu_2d_pos_x = %zu, pdu_2d_pos_y = %zu, sizeU0_ = %zu, sizeV0_ = %zu\n", 
            //     patch_index_plus_1, rec_params.patch_packing_block_size, patch.occupancy_resolution, 
            //     patch.patchPosXCanvasBlock, patch.patchPosYCanvasBlock, 
            //     patch.patchWidthCanvasBlock, patch.patchHeightCanvasBlock
            // );
            
            for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
                const size_t vBase = v0*patch.occupancy_resolution;
                for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, rec_params.blockToPatchWidth, rec_params.blockToPatchHeight, patch);
                    if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
                        continue;
                    }

                    const size_t uBase = u0*patch.occupancy_resolution;
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
                        const size_t v = vBase + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = uBase + u1;
                            size_t x;
                            size_t y;
                            // size_t canvasIndex = patch_to_canvas_tmc2(u, v, rec_params.tile_width, rec_params.tile_height, patch, x, y);
                            size_t canvasIndex = patch_to_canvas(u, v, rec_params.tile_width, patch, x, y);
                            bool occupied = occupancyMap[canvasIndex] != 0;
                            if (!occupied) {
                                continue;
                            }
                            
                            size_t num_geo_points = generate_points(
                                patch, geoMap1, geoMap2, 
                                u, v, x, y, 
                                layer_count, rec_params.absoluteD1_, 
                                rec_params.geo_map_width, pointsGeometry
                            );
                            /*
                                Size of generated points is 2 for Double layer,
                                Size of generated points is 1 for Single layer
                            */
                            for (size_t i = 0; i < num_geo_points; i++) { // get pointsGeometry and color the points
                                if (patch.axisOfAdditionalPlane_ == 0) {
                                    frame->pointsGeometry.push_back(pointsGeometry[i]);
                                } else {
                                    Vector3<typeGeometryInput> tmp;
                                    inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, rec_params.geo_bit_depth_3d, pointsGeometry[i], tmp);
                                    frame->pointsGeometry.push_back(tmp);
                                }
                                // Get pointsAttribute, color the points
                                if (p_->useTMC2AttributeYUVConversion) {
                                    frame->pointsAttribute16bits.push_back(get_attribute(attributeMaps_16bits[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                                } else {
                                    frame->pointsAttribute.push_back(get_attribute(attributeMaps[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                                }
                            } // get pointsGeometry and color the points
                        }
                    } // patch.occupancy_resolution
                }
            } // Block
        } // Patch
        frame->pointCount = frame->pointsGeometry.size();
        // frameId++;
        // printf("Reconstruct frame %d, size %d of GOF %d, geo_map size = %zu\n", frameId++, (int)frame->pointCount, (int)gofUVG->gofId, geoMap1.size());
    }
}

// With geometry smoothing applied
void reconstruct_singleTile(
    std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_,
    const reconstruction_options& rec_opts,
    const common_reconstruction_parameters& rec_params,
    const other_reconstruction_parameters& other_rec_params,
    const sei_reconstruction_info& sei_params
) {
    (void)rec_opts;
    int frameId = 0;
    std::vector<size_t> block_to_patch(rec_params.blockCount);
    std::vector<Vector3<typeGeometryInput>> pointsGeometry(2);
    for (auto &frame : gofUVG->frames) {    
        std::fill(block_to_patch.begin(), block_to_patch.end(), 0);


        const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;
        std::vector<uint8_t>& occupancyMap = frame->occupancyMap;
        const std::vector<Patch>& patchList = frame->patchList;
        
        size_t num_occupied_points = 0;

        generateOccupancyMap(
            occupancyMap, 
            occupancyMapDS, 
            gofUVG->occupancy_map_width, 
            rec_params.occupancyPrecision, 
            rec_params.tile_width, 
            rec_params.tile_height,
            num_occupied_points 
        );

        generateBlockToPatchFromOccupancyMapVideo(
            rec_params.blockToPatchWidth, 
            rec_params.blockToPatchHeight, 
            rec_params.tile_width, 
            occupancyMap,
            patchList, 
            block_to_patch
        );

        const auto& geoMap1 = frame->geometryMapL1;
        const auto& geoMap2 = rec_params.multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

        auto* attributeMaps_16bits = &frame->attributeMapL1_16bits;
        if (rec_params.layer_count > 1) {
            attributeMaps_16bits[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1_16bits : frame->attributeMapL2_16bits);
        } 

        frame->pointsGeometry.reserve(num_occupied_points * rec_params.layer_count);        
        std::vector<Vector3<size_t>> pointToPixel;
        std::vector<uint32_t> partition;
        partition.reserve(num_occupied_points * rec_params.layer_count);
        pointToPixel.reserve(num_occupied_points * rec_params.layer_count);

        frame->pointsAttribute16bits.reserve(num_occupied_points * rec_params.layer_count);
        
        // Generate Point Cloud Geometry from Occupancy Map and Patch List
        for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
            const size_t patch_index_tmp = rec_params.asps_patch_precedence_order_flag ? (patchList.size() - patch_index - 1) : patch_index;
            const uvgvpcc_dec::Patch& patch = patchList[patch_index_tmp];
            const size_t patch_index_plus_1 = patch_index_tmp + 1;
            
            for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
                const size_t vBase = v0*patch.occupancy_resolution;
                for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, rec_params.blockToPatchWidth, rec_params.blockToPatchHeight, patch);
                    if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
                        continue;
                    }

                    const size_t uBase = u0*patch.occupancy_resolution;
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
                        const size_t v = vBase + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = uBase + u1;
                            size_t x;
                            size_t y;
                            // size_t canvasIndex = patch_to_canvas_tmc2(u, v, rec_params.tile_width, rec_params.tile_height, patch, x, y);
                            size_t canvasIndex = patch_to_canvas(u, v, rec_params.tile_width, patch, x, y);
                            bool occupied = occupancyMap[canvasIndex] != 0;
                            if (!occupied) {
                                continue;
                            }

                            size_t num_geo_points = generate_points(
                                patch, geoMap1, geoMap2, 
                                u, v, x, y, 
                                rec_params.layer_count, rec_params.absoluteD1_, 
                                rec_params.geo_map_width, pointsGeometry
                            );
                            /*
                                Size of generated points is 2 for Double layer,
                                Size of generated points is 1 for Single layer
                            */
                            for (size_t i = 0; i < num_geo_points; i++) { // get pointsGeometry and color the points
                                if (patch.axisOfAdditionalPlane_ == 0) {
                                    frame->pointsGeometry.push_back(pointsGeometry[i]);
                                } else {
                                    Vector3<typeGeometryInput> tmp;
                                    inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, rec_params.geo_bit_depth_3d, pointsGeometry[i], tmp);
                                    frame->pointsGeometry.push_back(tmp);
                                }
                                // Get pointsAttribute, color the points
                                frame->pointsAttribute16bits.push_back(get_attribute(attributeMaps_16bits[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                                
                                // if ( params.singleMapPixelInterleaving_ ) {
                                //     pointToPixel.emplace_back(
                                //         x, y,
                                //         i == 0 ? ( static_cast<size_t>( x + y ) % 2 )
                                //             : i == 1 ? ( static_cast<size_t>( x + y + 1 ) % 2 ) : g_intermediateLayerIndex );
                                // } else if ( params.pointLocalReconstruction_ ) {
                                //     pointToPixel.emplace_back(
                                //         x, y, i == 0 ? 0 : i == 1 ? g_intermediateLayerIndex : g_intermediateLayerIndex + 1 );
                                // } else {
                                //     printf("----------------------> pointToPixel.emplace_back( x, y, i < 2 ? i : g_intermediateLayerIndex + 1 );\n");
                                //     pointToPixel.emplace_back( x, y, i < 2 ? i : g_intermediateLayerIndex + 1 );
                                // }

                                if (other_rec_params.single_map_pixel_interleaving) {

                                } else if (other_rec_params.point_local_reconstruction) {

                                } else {
                                    pointToPixel.emplace_back(x, y, i < 2 ? i : 100 + 1);
                                }
                                partition.emplace_back(static_cast<uint32_t>(patch_index_tmp));
                            } // get pointsGeometry and color the points
                        }
                    } // patch.occupancy_resolution
                }
            } // Block
        } // Patch
        frame->pointCount = frame->pointsGeometry.size();

        size_t raw_points_count = 0;
        if (other_rec_params.enhanced_occupancy_map_code) {

        } else {

        }
        if (other_rec_params.use_additional_points_patch) {

        } // else -> raw_points_count = 0


        const size_t num_points = frame->pointsGeometry.size() - raw_points_count;
        std::vector<uint16_t> boundaryPointTypes;
        boundaryPointTypes.resize(frame->pointsGeometry.size(), 0);

        for (size_t i = 0; i < num_points; i++) {
            auto point = pointToPixel[i];
            identifyBoundaryPoints(occupancyMap, point[0], point[1], rec_params.tile_width, rec_params.tile_height, i, boundaryPointTypes);
        }

        if (sei_params.grid_smoothing) {
            //smoothReconstructedGeometry(frame->pointsGeometry, sei_params, boundaryPointTypes, partition);
            std::vector<smoothedGeoInfo> smoothed_geo_points;
            smoothReconstructedGeometry(frame->pointsGeometry, smoothed_geo_points, sei_params, boundaryPointTypes, partition);
            transferColors16bitBP_fast(frame, smoothed_geo_points);

            // std::vector<Vector3<typeGeometryInput>> sourceGeos = frame->pointsGeometry;
            // std::vector<Vector3<typeAttributeInput16bit>> sourceAttrs = frame->pointsAttribute16bits;
            // std::vector<size_t> smoothed_point_indices;
            // smoothReconstructedGeometry(frame->pointsGeometry, smoothed_point_indices, sei_params, boundaryPointTypes, partition);
            // transferColors16bitBP_fast(frame, sourceGeos, sourceAttrs, smoothed_point_indices);
        } else {
            
        }

        // frameId++;
        // printf("Reconstruct frame %d, size %d of GOF %d, geo_map size = %zu\n", frameId++, (int)frame->pointCount, (int)gofUVG->gofId, geoMap1.size());
    }
}


} // anonymous namespace


void Reconstruction::reconstructPointCloud(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_) {
    const vps* vps = gof_->get_v3c_vps();
    const atlas_sequence_parameter_set& asps = gof_->get_v3c_atlas_context()->get_asps();

    reconstruction_options rec_opts;
    sei_reconstruction_info sei_params;
    common_reconstruction_parameters rec_params;
    other_reconstruction_parameters other_rec_params;

    set_reconstruction_parameters(gofUVG, gof_, rec_opts, sei_params, rec_params, other_rec_params);

    if (!sei_params.pbf_enable_flag && rec_params.num_tiles_in_atlas_frame == 1) { // No patch block filtering and using single tile
        // reconstruct_singleTile_noSei(gofUVG, gof_, rec_opts, rec_params);
        if (sei_params.geometry_smoothing_flag && rec_opts.applyGeoSmoothingType_ != 0) {
            reconstruct_singleTile(gofUVG, gof_, rec_opts, rec_params, other_rec_params, sei_params);
        } else {
            printf("No geo Smoothing\n");
            reconstruct_singleTile_noSei(gofUVG, gof_, rec_opts, rec_params);
        }
    } else {
        throw std::runtime_error("Multiple-tiles within an Atlas frame -> Not Yet Implemented\n");
    }
}

void Reconstruction::setReconstructionParameters(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof>& gof_, std::shared_ptr<reconstruction_parameters>& rec_params) {

    set_reconstruction_parameters(gofUVG, gof_, rec_params->rec_opts, rec_params->rec_sei_info, rec_params->common_rec_params, rec_params->other_rec_params);
}

void Reconstruction::reconstructPointCloudFrame(std::shared_ptr<uvgvpcc_dec::Frame> frame, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, common_reconstruction_parameters& rec_params) {
    std::vector<size_t> block_to_patch(rec_params.blockCount);
    std::vector<Vector3<typeGeometryInput>> pointsGeometry(2);
    std::fill(block_to_patch.begin(), block_to_patch.end(), 0);

    const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;
    std::vector<uint8_t>& occupancyMap = frame->occupancyMap;
    const std::vector<Patch>& patchList = frame->patchList;
    
    size_t num_occupied_points = 0;
    
    generateOccupancyMap(
        occupancyMap, 
        occupancyMapDS, 
        gofUVG->occupancy_map_width, 
        rec_params.occupancyPrecision, 
        rec_params.tile_width, 
        rec_params.tile_height,
        num_occupied_points 
    );

    generateBlockToPatchFromOccupancyMapVideo(
        rec_params.blockToPatchWidth, 
        rec_params.blockToPatchHeight, 
        rec_params.tile_width, 
        rec_params.tile_height, 
        occupancyMapDS,
        gofUVG->occupancy_map_width,
        rec_params.occupancyPrecision,
        patchList, 
        block_to_patch
    );

    const auto& geoMap1 = frame->geometryMapL1;
    const auto& geoMap2 = rec_params.multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

    auto* attributeMaps = &frame->attributeMapL1;
    auto* attributeMaps_16bits = &frame->attributeMapL1_16bits;
    if (rec_params.layer_count > 1) {
        attributeMaps[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1 : frame->attributeMapL2);
        attributeMaps_16bits[1] = (rec_params.multipleStreams_ ? frame->attributeMapL1_16bits : frame->attributeMapL2_16bits);
    } 

    frame->pointsGeometry.reserve(num_occupied_points * rec_params.layer_count);
    if (p_->useTMC2AttributeYUVConversion) {
        frame->pointsAttribute16bits.reserve(num_occupied_points * rec_params.layer_count);
    } else {
        frame->pointsAttribute.reserve(num_occupied_points * rec_params.layer_count);
    }

    for (size_t patch_index = 0; patch_index < patchList.size(); patch_index++) { // Patch
        const size_t patch_index_tmp = rec_params.asps_patch_precedence_order_flag ? (patchList.size() - patch_index - 1) : patch_index;
        const uvgvpcc_dec::Patch& patch = patchList[patch_index_tmp];
        const size_t patch_index_plus_1 = patch_index_tmp + 1;

        // printf("patch %zu, patch occupancy size = %zu, patch size = %zu\n", patch_index_plus_1, 
        //     patch.occupancy_resolution*patch.occupancy_resolution, patch.patchHeightCanvasBlock*patch.patchWidthCanvasBlock);
        
        for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
            const size_t vBase = v0*patch.occupancy_resolution;
            for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, rec_params.blockToPatchWidth, rec_params.blockToPatchHeight, patch);
                if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
                    continue;
                }
                const size_t uBase = u0*patch.occupancy_resolution;
                for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
                    const size_t v = vBase + v1;
                    for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                        const size_t u = uBase + u1;
                        size_t x;
                        size_t y;
                        size_t canvasIndex = patch_to_canvas_tmc2(u, v, rec_params.tile_width, rec_params.tile_height, patch, x, y);
                        bool occupied = occupancyMap[canvasIndex] != 0;
                        if (!occupied) {
                            continue;
                        }

                        size_t num_geo_points = generate_points(
                            patch, geoMap1, geoMap2, 
                            u, v, x, y, 
                            rec_params.layer_count, rec_params.absoluteD1_, 
                            rec_params.geo_map_width, pointsGeometry
                        );
                        /*
                            Size of generated points is 2 for Double layer,
                            Size of generated points is 1 for Single layer
                        */
                        for (size_t i = 0; i < num_geo_points; i++) { // get pointsGeometry and color the points
                            if (patch.axisOfAdditionalPlane_ == 0) {
                                frame->pointsGeometry.push_back(pointsGeometry[i]);
                            } else {
                                Vector3<typeGeometryInput> tmp;
                                inverseRotatePosition45DegreeOnAxis(patch.axisOfAdditionalPlane_, rec_params.geo_bit_depth_3d, pointsGeometry[i], tmp);
                                frame->pointsGeometry.push_back(tmp);
                            }
                            // Get pointsAttribute, color the points
                            if (p_->useTMC2AttributeYUVConversion) {
                                frame->pointsAttribute16bits.push_back(get_attribute(attributeMaps_16bits[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                            } else {
                                frame->pointsAttribute.push_back(get_attribute(attributeMaps[i], rec_params.offsetU, rec_params.offsetV, rec_params.attribute_map_width, x, y));
                            }
                        } // get pointsGeometry and color the points
                    }
                } // patch.occupancy_resolution
            }
        } // Block
    } // Patch
    frame->pointCount = frame->pointsGeometry.size();
}


/* Case
    _ gof_->get_v3c_atlas_context()->get_afps().afti.afti_single_tile_in_atlas_frame_flag == True,
    _ Patch block filtering: pbf_enable_flag == False
    _ Poin local reconstruction: plr_enabled_flag == False
*/
void Reconstruction::reconstructPointCloud_(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_) {
    const vps* vps = gof_->get_v3c_vps();

    // General Reconstruction Options
    reconstruction_options rec_opts;
    rec_opts.setReconstructionParameters(vps->ptl_.ptl_profile_reconstruction_idc);

    const atlas_sequence_parameter_set& asps = gof_->get_v3c_atlas_context()->get_asps();

    /* Common reconstruction parameters */
    const size_t occupancyPrecision = vps->vps_frame_width_.at(0) / gofUVG->occupancy_map_width;
    const size_t step_height = gofUVG->occupancy_map_width / occupancyPrecision;    
    const size_t asps_frame_width  = asps.asps_frame_width;
    const size_t asps_frame_height = asps.asps_frame_height;
    
    const size_t patch_packing_block_size = size_t( 1 ) << asps.asps_log2_patch_packing_block_size;
    const size_t blockToPatchWidth  = asps_frame_width / patch_packing_block_size;
    const size_t blockToPatchHeight = asps_frame_height / patch_packing_block_size;
    const size_t blockCount = blockToPatchWidth * blockToPatchHeight;

    const size_t layer_count = vps->vps_map_count_minus1_.at(0) + 1;

    const bool asps_patch_precedence_order_flag = asps.asps_patch_precedence_order_flag;
    const bool multipleStreams_ = vps->vps_multiple_map_streams_present_flag_.at(0);
    const bool absoluteD1_ = layer_count == 1 || vps->vps_map_absolute_coding_enabled_flag_.at(0).at(1);
    
    const size_t geo_map_width = gofUVG->geometry_map_width;
    // const size_t geo_bit_depth_3d = vps->geometry_info_.at(0).gi_geometry_2d_bit_depth_minus1 + 1;
    const size_t geo_bit_depth_3d = vps->geometry_info_.at(0).gi_geometry_3d_coordinates_bit_depth_minus1 + 1;

    const size_t& attribute_map_width = gofUVG->attribute_map_width;
    const size_t offsetU = attribute_map_width * gofUVG->attribute_map_height;
    const size_t offsetV = p_->useTMC2AttributeYUVConversion ? (offsetU << 1U) : offsetU + (offsetU >> 2U);
    /*---------------------------------------------------------------------------------------------------*/


    const size_t surface_thickness  = asps.asps_vpcc_surface_thickness_minus1 + 1;
    const size_t threshold_lossy_om = static_cast<size_t>(vps->occupancy_info_.at(0).oi_lossy_occupancy_compression_threshold);
    const bool enable_size_quantization      = asps.asps_patch_size_quantizer_present_flag; 
    const bool remove_duplicate_points       = rec_opts.duplicatedPointRemovalType_ != 0 && asps.asps_vpcc_remove_duplicate_point_enabled_flag;
    const bool point_local_reconstruction    = rec_opts.pointLocalReconstructionType_ != 0 && asps.asps_plr_enabled_flag;
    const bool single_map_pixel_interleaving = rec_opts.pixelDeinterleavingType_ != 0 && asps.asps_pixel_deinterleaving_enabled_flag;
    const bool use_additional_points_patch   =  rec_opts.reconstructRawType_ != 0 && asps.asps_raw_patch_enabled_flag;
    const bool use_aux_seperate_video        = asps.asps_auxiliary_video_enabled_flag;
    const bool enhanced_occupancy_map_code   = rec_opts.reconstructEomType_ != 0 && asps.asps_eom_patch_enabled_flag;
    const bool EOM_fix_bit_count             = asps.asps_eom_fix_bit_count_minus1 + 1;


    std::vector<atlas_tile_layer_rbsp> &atlas_data = gof_->get_v3c_atlas_context()->get_atlases();
    printf("atlas_data size = %zu\n", atlas_data.size());
    /* Set SEI reconstruction info 
        Assuming the same SEI message is applied to all the frames within a GOF
    */
    sei_reconstruction_info sei_rec_info;
    int sei_idx;
    if (rec_opts.applyAttrSmoothingType_ != 0 && atlas_data.at(0).sei_.prefix_sei_is_present(GEOMETRY_SMOOTHING, sei_idx)) {
        sei_geometry_smoothing* sei_geo_smoothing = static_cast<sei_geometry_smoothing*>(atlas_data.at(0).sei_.sei_prefix.at(sei_idx).get());
        for (size_t i = 0; i < sei_geo_smoothing->instancesUpdated; i++) {
            size_t k = sei_geo_smoothing->instanceIndices.at(i);
            if (!sei_geo_smoothing->instanceCancelFlags.at(k)) {
                sei_rec_info.geometry_smoothing_flag = true;
                if (sei_geo_smoothing->methodTypes.at(k) == 1) {
                    sei_rec_info.grid_smoothing = true;
                    sei_rec_info.grid_size = sei_geo_smoothing->gridSizeMinus2s.at(k) + 2;
                    sei_rec_info.threshold_smoothing = static_cast<double>(sei_geo_smoothing->thresholds.at(k));
                }
            }
        }
    }
    if (rec_opts.applyOccupanySynthesisType_ != 0 ) {

    }
    if (rec_opts.applyAttrSmoothingType_ != 0 ) {

    }
    /*---------------------------------------------------------------------------------------------------*/

    std::vector<size_t> block_to_patch(blockCount);
    
    //size_t frameId = gofUVG->gofId * gofUVG->gofCount;
    int frameId = 0;
    printf("vps->occupancy_info_: size = %zu, #frames = %zu\n", vps->occupancy_info_.size(), gofUVG->frames.size());
    for (auto &frame : gofUVG->frames) {
        std::fill(block_to_patch.begin(), block_to_patch.end(), 0);

        const std::vector<uint8_t>& occupancyMapDS = frame->occupancyMapDS;
        std::vector<uint8_t>& occupancyMap = frame->occupancyMap;
        const std::vector<Patch>& patchList = frame->patchList;

        // setGeneratePointCloudParameters( gpcParams, context, atglIndex );
        // setPostProcessingSeiParameters( ppSEIParams, context, atglIndex );

        if (!sei_rec_info.pbf_enable_flag) {
            generateOccupancyMap( // -> if ( !ppSEIParams.pbfEnableFlag_ )
                occupancyMap, 
                occupancyMapDS, 
                gofUVG->occupancy_map_width, 
                occupancyPrecision, 
                asps_frame_width, 
                asps_frame_height 
            );
        }

        generateBlockToPatchFromOccupancyMapVideo(
            blockToPatchWidth, 
            blockToPatchHeight, 
            asps_frame_width, 
            asps_frame_height, 
            occupancyMapDS,
            gofUVG->occupancy_map_width,
            occupancyPrecision,
            patchList, 
            block_to_patch
        );

        // if ( ) {
            
        // }

        const auto& geoMap1 = frame->geometryMapL1;
        const auto& geoMap2 = multipleStreams_ ? frame->geometryMapL1 : frame->geometryMapL2;

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

            for (size_t v0 = 0; v0 < patch.patchHeightCanvasBlock; v0++) { // Block
                for (size_t u0 = 0; u0 < patch.patchWidthCanvasBlock; u0++) {
                    const size_t blockIndex = patchBlock_to_canvasBlock_tmc2(u0, v0, blockToPatchWidth, blockToPatchHeight, patch);
                    if (!(block_to_patch[blockIndex] == patch_index_plus_1)) { // Check if the block belongs to the patch
                        continue;
                    }
                    
                    for ( size_t v1 = 0; v1 < patch.occupancy_resolution; ++v1 ) { // patch.occupancy_resolution
                        const size_t v = v0 * patch.occupancy_resolution + v1;
                        for ( size_t u1 = 0; u1 < patch.occupancy_resolution; ++u1 ) {
                            const size_t u = u0 * patch.occupancy_resolution + u1;
                            size_t x;
                            size_t y;
                            size_t canvasIndex = patch_to_canvas_tmc2(u, v, asps_frame_width, asps_frame_height, patch, x, y);
                            bool occupied = occupancyMap.at(canvasIndex) != 0;
                            if (!occupied) {
                                continue;
                            }

                            std::vector<Vector3<typeGeometryInput>> pointsGeometry;
                            generate_points_(patch, geoMap1, geoMap2, u, v, x, y, layer_count, absoluteD1_, geo_map_width, pointsGeometry);
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
                            } // get pointsGeometry and color the points
                        }
                    } // patch.occupancy_resolution
                }
            } // Block
        } // Patch
        frame->pointCount = frame->pointsGeometry.size();
        // frameId++;
        // printf("Reconstruct frame %d, size %d of GOF %d, geo_map size = %zu\n", frameId++, (int)frame->pointCount, (int)gofUVG->gofId, geoMap1.size());
        // printf("Reconstruct frame %d, size %d of GOF %d\n", frameId, (int)frame->pointCount, (int)gofUVG->gofId);
    } // Frame
    
}
