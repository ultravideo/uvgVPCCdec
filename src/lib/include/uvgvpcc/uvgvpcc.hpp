#pragma once

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <semaphore>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>

#include <assert.h>
#include "../utils/utils.hpp"
#include "../utils/threadqueue.hpp"

#include "zmq_lib.h"
#include "zmq.hpp"

namespace uvgvpcc_dec {

struct point3d {
    uint16_t data_[3];

    uint16_t& operator[]( size_t i ) {
        assert( i < 3 );
        return data_[i];
    }
    const uint16_t& operator[]( size_t i ) const {
        assert( i < 3 );
        return data_[i];
    }

    point3d(const uint16_t x, uint16_t y, uint16_t z ) {
        data_[0] = x;
        data_[1] = y;
        data_[2] = z;
    }

    point3d() = default;
    
    bool operator==(const point3d &cmp) const {
        return ( data_[0] == cmp.data_[0] && data_[1] == cmp.data_[1] && data_[2] == cmp.data_[2] );
    };
    bool operator!=(const point3d &cmp) const {
        return ( data_[0] != cmp.data_[0] || data_[1] != cmp.data_[1] || data_[2] != cmp.data_[2] );
    }

    uint16_t&       x() { return data_[0]; }
    uint16_t&       y() { return data_[1]; }
    uint16_t&       z() { return data_[2]; }
};

struct Patch
{
    size_t patchIndex_; // (Not used!)
    size_t patchPpi_;  // viewId // TilePatchProjectionID
    size_t originalIndex_ = 0;  // (Not used!) patch original index size_t referencePatchId_

    size_t normalAxis_;     // x for point generation
    size_t tangentAxis_;    // y for point generation
    size_t bitangentAxis_;  // z for point generation

    size_t posU_; // u1_ minU_       // tangential shift    TilePatch3dOffsetU
    size_t posV_; // v1_ minV_       // bitangential shift  TilePatch3dOffsetV
    size_t posD_; // d1_ minD_       // depth shift         TilePatch3dOffsetD

    size_t projectionMode_;  // TilePatchProjectionID 

    size_t sizeD_; // TilePatch3dRangeD

    std::vector<uint8_t> patchOccupancyMap_;
    size_t widthInPixel_ = 0;   // size for U  // width of the patch occupancy map (in pixels) size2DXInPixel_
    size_t heightInPixel_ = 0;  // size for V  // height of the patch occupancy map (in pixels) size2DYInPixel_
    size_t occupancy_resolution = 0;

    size_t area_ = 0; // occupancy_resolution

    size_t widthInOccBlk_ = 1;  // sizeU0_     // width of the patch occupancy map within the down-scaled frame occupancy map (in DS occupancy map
                            // blocks).  // TilePatch2dSizeX
    size_t heightInOccBlk_ = 1;  // sizeV0_     // height of the patch occupancy map within the down-scaled frame occupancy map (in DS occupancy
                             // map blocks). // TilePatch2dSizeY
    size_t patchWidthCanvasBlock = 1;   // sizeU0_ = TilePatch2dSizeX
    size_t patchHeightCanvasBlock = 1;  // sizeV0_ = TilePatch2dSizeY
    size_t patchWidthCanvas;   // sizeU0_, patchWidthCanvasBlock  * occupancy_resolution = patch width in canvas
    size_t patchHeightCanvas;  // sizeV0_, patchHeightCanvasBlock * occupancy_resolution = patch height in canvas

    size_t omDSPosX_;  // u0_    // location in down-scaled occupancy map  // lf  posBlkU TilePatch2dPosX
    size_t omDSPosY_;  // v0_    // location in down-scaled occupancy map                 TilePatch2dPosY
    size_t patchPosXCanvasBlock;  // u0_    // TilePatch2dPosX // patch pos x in canvas block
    size_t patchPosYCanvasBlock;  // v0_    // TilePatch2dPosY // patch pos y in canvas block
    size_t patchPosXCanvas; // u0_, patchPosXCanvasBlock * occupancy_resolution = patch pos x in canvas
    size_t patchPosYCanvas; // v0_, patchPosYCanvasBlock * occupancy_resolution = patch pos y in canvas 

    uint32_t orientationIndex = 0; // patch orientation in canvas atlas

    std::vector<uint16_t> depthL1_;  // depth value First layer // TODO(lf): Using the geo type here might lead to issue?
    std::vector<uint16_t> depthL2_;  // depth value Second layer

    size_t axisOfAdditionalPlane_; 

    size_t levelOfDetailX_ = 1; // TMC2, for point generation
    size_t levelOfDetailY_ = 1; // TMC2, for point generation

    int32_t bestMatchIdx_ = 0;
    size_t refAtlasFrameIdx_ = 0;

    inline double generateNormalCoordinate( const uint16_t depth, const size_t projectionMode ) const {
        uint32_t d1_ = static_cast<uint32_t>(posD_);
        double coord = 0;
        if ( projectionMode == 0 ) {
        coord = ( (double)depth + (double)d1_ );
        } else {
        double tmp_depth = double( d1_ ) - double( depth );
        if ( tmp_depth > 0 ) { coord = tmp_depth; }
        }
        return coord;
    }
    // point3d generatePoint( const size_t u, const size_t v, const uint16_t depth ) const {
    //     point3d point0;
    //     point0.data_[normalAxis_]    = generateNormalCoordinate( depth, projectionMode_ );
    //     point0.data_[tangentAxis_]   = ( double( u ) * (double)levelOfDetailX_ + posU_ );
    //     point0.data_[bitangentAxis_] = ( double( v ) * (double)levelOfDetailY_ + posV_ );
    //     return point0;
    // }

    Vector3<typeGeometryInput> generatePoint( const size_t u, const size_t v, const uint16_t depth ) const {
        Vector3<typeGeometryInput> point0;
        point0[normalAxis_]    = generateNormalCoordinate( depth, projectionMode_ );
        point0[tangentAxis_]   = ( double( u ) * (double)levelOfDetailX_ + posU_ );
        point0[bitangentAxis_] = ( double( v ) * (double)levelOfDetailY_ + posV_ );
        return point0;
    }

    void setAxis( 
        size_t axisOfAdditionalPlane, 
        size_t normalAxis, 
        size_t tangentAxis,
        size_t bitangentAxis, 
        size_t projectionMode 
    ) {
        axisOfAdditionalPlane_ = axisOfAdditionalPlane;
        normalAxis_            = normalAxis;
        tangentAxis_           = tangentAxis;
        bitangentAxis_         = bitangentAxis;
        projectionMode_        = projectionMode;
    }

    inline void setPatchPpiAndAxis(size_t patchPpi) {
        patchPpi_ = patchPpi;
        // now set the other variables according to the viewId
        switch (patchPpi_) {
            case 0: setAxis( 0, 0, 2, 1, 0 ); break;
            case 1: setAxis( 0, 1, 2, 0, 0 ); break;
            case 2: setAxis( 0, 2, 0, 1, 0 ); break;
            case 3: setAxis( 0, 0, 2, 1, 1 ); break;
            case 4: setAxis( 0, 1, 2, 0, 1 ); break;
            case 5: setAxis( 0, 2, 0, 1, 1 ); break;
            case 6: setAxis( 1, 0, 2, 1, 0 ); break;
            case 7: setAxis( 1, 2, 0, 1, 0 ); break;
            case 8: setAxis( 1, 0, 2, 1, 1 ); break;
            case 9: setAxis( 1, 2, 0, 1, 1 ); break;
            case 10: setAxis( 2, 2, 0, 1, 0 ); break;
            case 11: setAxis( 2, 1, 2, 0, 0 ); break;
            case 12: setAxis( 2, 2, 0, 1, 1 ); break;
            case 13: setAxis( 2, 1, 2, 0, 1 ); break;
            case 14: setAxis( 3, 1, 2, 0, 0 ); break;
            case 15: setAxis( 3, 0, 2, 1, 0 ); break;
            case 16: setAxis( 3, 1, 2, 0, 1 ); break;
            case 17: setAxis( 3, 0, 2, 1, 1 ); break;
            default:
            throw std::runtime_error("ViewId (" + std::to_string(patchPpi) + ") not allowed... exiting");
            break;
        }
    }
};

struct GOF;

// TODO(lf): Avoid using both constant sized and dynamic sized memory member within the same struct.
struct Frame {
    size_t frameId;      // aka relative index (0 if first encoded frame)
    size_t frameNumber;  // aka number from the input frame file name (TODO(lf): correct)
    std::weak_ptr<GOF> gof;
    size_t gofId; 
    // std::shared_ptr<std::counting_semaphore<UINT16_MAX>> conccurentFrameSem;

    std::vector<point3d> pointsPosList; // Positions of 3D-points
    std::vector<point3d> pointsPixelsList; // 3D-points of the 2D map that correspond to the points in the pointsPosList, it is used to for coloring 3D-points

    std::string pointCloudPath;

    size_t pointCount = 0;
    std::vector<Vector3<typeGeometryInput>> pointsGeometry; // Positions of 3D-points
    std::vector<Vector3<uint8_t>> pointsAttribute;          // Colors of each point (8 bits)
    std::vector<Vector3<uint16_t>> pointsAttribute16bits;

    std::vector<Patch> patchList;

    size_t mapHeight = 0;  // TODO(lf): Will be a gof parameter ?
    size_t mapHeightDS = 0;

    std::vector<uint8_t> occupancyMap = {};    // (boolean vector)
    std::vector<uint8_t> occupancyMapDS = {};  // Down-scaled occupancy map of the frame (boolean vector)

    std::vector<uint8_t> geometryMaps[2];  // Multiple-streams
    std::vector<uint8_t> geometryMapL1 = {};  // first layer
    std::vector<uint8_t> geometryMapL2 = {};  // second layer

    std::vector<uint8_t> attributeMaps[2];  // Multiple-streams
    std::vector<uint8_t> attributeMapL1 = {};  // Store the three channels continuously (all R, then all G, than all B)
    std::vector<uint8_t> attributeMapL2 = {};

    std::vector<uint16_t> attributeMapL1_16bits = {};  // Store the three channels continuously (all R, then all G, than all B)
    std::vector<uint16_t> attributeMapL2_16bits = {};


    // Frame(const size_t& frameId, const size_t& frameNumber, const std::string& pointCloudPath)
    //     : frameId(frameId), frameNumber(frameNumber), pointCloudPath(pointCloudPath), pointCount(0) {}
    Frame() {}
    ~Frame() {
        // if (conccurentFrameSem) {
        //     conccurentFrameSem->release();
        // }
    };
    void printInfo() const;
};

struct GOF {
    std::vector<std::shared_ptr<Frame>> frames;
    size_t occupancy_map_width;
    size_t occupancy_map_height;
    size_t geometry_map_width;
    size_t geometry_map_height;
    size_t attribute_map_width;
    size_t attribute_map_height;

    bool doubleLayer = false;

    size_t baseFrameId = 0;
    size_t gofCount;
    size_t nbFrames;
    size_t gofId;

    size_t mapHeightGOF;
    size_t mapHeightDSGOF;

    std::vector<uint8_t> bitstreamOccupancy;
    std::vector<uint8_t> bitstreamGeometry;
    std::vector<uint8_t> bitstreamAttribute;

    std::vector<std::vector<uint8_t>> streams_geometry;
    std::vector<std::vector<uint8_t>> streams_attribute;


    // std::shared_ptr<std::counting_semaphore<UINT16_MAX>> conccurentFrameSem;
    // ~GOF() {
    //     if (conccurentFrameSem) {
    //         conccurentFrameSem->release();
    //     }
    // };

};

/// @brief API of the uvgVPCCdec library
namespace API {

struct vuh_unit {
    std::vector<uint8_t> data;
    size_t v3c_unit_size = 0;
};

/// @brief Bitstream reading miscellaneous
struct v3c_chunk {
    size_t len = 0;                 // Length of data in buffer
    std::unique_ptr<std::vector<uint8_t>> data;
    std::unique_ptr<std::vector<uint8_t>> vuh_units[5]; // VPS, AD, OVD, GVD, AVD
    std::unique_ptr<vuh_unit> vps_unit; // vps
    std::unique_ptr<vuh_unit> ad_unit;  // atlas data
    std::unique_ptr<vuh_unit> ovd_unit; // occupancy
    std::unique_ptr<std::vector<vuh_unit>> gvd_units; // geometry
    std::unique_ptr<std::vector<vuh_unit>> avd_units; // attribute
    std::vector<size_t> v3c_unit_sizes = {};
    size_t gof_id = std::numeric_limits<size_t>::max();
    size_t gof_count = std::numeric_limits<size_t>::max();

    //std::shared_ptr<std::counting_semaphore<UINT16_MAX>> conccurentFrameSem;

    v3c_chunk() {
        vps_unit  = std::make_unique<vuh_unit>();
        ad_unit   = std::make_unique<vuh_unit>();
        ovd_unit  = std::make_unique<vuh_unit>();
        gvd_units = std::make_unique<std::vector<vuh_unit>>();
        avd_units = std::make_unique<std::vector<vuh_unit>>();
    };

    v3c_chunk(bool sep) {
        if (sep) {
            for (int i = 0; i < 5; i++) {
                vuh_units[i] = std::make_unique<std::vector<uint8_t>>();
                // vuh_units[i]->reserve(1024*1024);
            }
        } else {
            data = std::make_unique<std::vector<uint8_t>>();
        }
    };

    // ~v3c_chunk() {
    //     if (conccurentFrameSem) {
    //         conccurentFrameSem->release();
    //     }
    // };
    // v3c_chunk(size_t len, std::unique_ptr<std::vector<uint8_t>> data) : len(len), data(std::move(data)) {}
};

// ht: A V3C unit stream is composed of only V3C units without parsing information in the bitstream itself. The parsing information is here
// given separately.
struct v3c_unit_stream {
    size_t v3c_unit_size_precision_bytes = 0;
    std::queue<v3c_chunk> v3c_chunks = {};
    std::counting_semaphore<> available_chunks{0};
    std::mutex io_mutex;  // Locks production and consumption in the v3c_chunks queue
};

struct decodedGOF {
    std::shared_ptr<uvgvpcc_dec::GOF> gof;
    bool terminated = false;
    bool ready = false;
};

struct point_cloud_frame_stream {
    std::queue<std::shared_ptr<Frame>> frames = {};
    std::vector<decodedGOF> available_gofs;
    std::queue<std::shared_ptr<uvgvpcc_dec::GOF>> available_gofs_queue;
    std::counting_semaphore<> available_frames{0};
    int total_gof = -1;
    std::mutex io_mutex;  // Locks production and consumption in the v3c_chunks queue
};

void initializeDecoder();
void setParameter(const std::string& parameterName, const std::string& parameterValue);

// void decodeFrame(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::string& outputFilePath);
// void decodeFrame_parallel(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::string& outputFilePath);

void decodeFrame(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool in_order_output, const std::string& outputFilePath);
void decodeFrame_parallel(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool in_order_output, const std::string& outputFilePath);

void decodeFrame_delay(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output);
void decodeFrame_parallel_delay(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output);

void decodeFrame_in_order(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output);
void decodeFrame_parallel_in_order(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output);

void decodeFrame_remote_output(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::shared_ptr<zmqHandler> zmq_handler);
void decodeFrame_parallel_remote_output(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::shared_ptr<zmqHandler> zmq_handler);

void emptyFrameQueue(std::shared_ptr<uvgvpcc_dec::ThreadQueue>& queue, std::shared_ptr<uvgvpcc_dec::Job>& last_out);
void emptyFrameQueue();
void stopDecoder();

}  // namespace API

}; // namespace uvgvpcc_dec