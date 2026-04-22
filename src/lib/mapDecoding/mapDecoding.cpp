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
}