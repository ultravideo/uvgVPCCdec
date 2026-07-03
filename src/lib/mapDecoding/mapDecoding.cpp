/// \file Entry point for the map decoding process.

#include "mapDecoding.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>

#include "abstract2DMapDecoder.hpp"

#include "decoderFFmpeg.hpp" // Include FFmepg

#include "utils/parameters.hpp"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

using namespace uvgvpcc_dec;

namespace {

std::unique_ptr<Abstract2DMapDecoder> occupancyMapDSDecoder;
std::unique_ptr<Abstract2DMapDecoder> geometryMapDecoder;
std::unique_ptr<Abstract2DMapDecoder> attributeMapDecoder;

} // anonymous namespace

void MapDecoding::initializeStaticParameters() {
    
}

void MapDecoding::initializeDecoderPointers() {
    occupancyMapDSDecoder = std::make_unique<DecoderFFmpeg>(OCCUPANCY);
    geometryMapDecoder = std::make_unique<DecoderFFmpeg>(GEOMETRY);
    attributeMapDecoder = std::make_unique<DecoderFFmpeg>(ATTRIBUTE);
}

void MapDecoding::decodeGOFMaps(const std::shared_ptr<GOF>& gof) {
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("MAP ENCODING", "Encode maps of GOF " + std::to_string(gof->gofId) + ".\n");
    
    occupancyMapDSDecoder->decodeGOFMaps(gof);
    geometryMapDecoder->decodeGOFMaps(gof);
    attributeMapDecoder->decodeGOFMaps(gof);

    printf("--------------------> Map Decoding of GOF %zu Done!\n", gof->gofId);
}


// #include <future>
// void MapDecoding::decodeGOFMaps(const std::shared_ptr<GOF>& gof) {
//     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>(
//         "MAP DECODING",
//         "Decode maps of GOF " + std::to_string(gof->gofId) + ".\n"
//     );

//     auto occTask = std::async(std::launch::async, [&]() {
//         // printf("Occupancy Map Decoding Starts, GOF:%d\n", (int)gof->gofId);
//         occupancyMapDSDecoder->decodeGOFMaps(gof);
//         // printf("Occupancy Map Decoding Done, GOF:%d\n", (int)gof->gofId);
//     });

//     auto attrTask = std::async(std::launch::async, [&]() {
//         // printf("Attribute Map Decoding Starts, GOF:%d\n", (int)gof->gofId);
//         attributeMapDecoder->decodeGOFMaps(gof);
//         // printf("Attribute Map Decoding Done, GOF:%d\n", (int)gof->gofId);
//     });

//     auto geomTask = std::async(std::launch::async, [&]() {
//         // printf("Geometry Map Decoding Starts, GOF:%d\n", (int)gof->gofId);
//         geometryMapDecoder->decodeGOFMaps(gof);
//         // printf("Geometry Map Decoding Done, GOF:%d\n", (int)gof->gofId);
//     });

//     occTask.get();
//     attrTask.get();
//     geomTask.get();
// }