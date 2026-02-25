#include "uvgvpcc/uvgvpcc.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <regex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "bitstreamParsing/bitstreamParsing.hpp"
#include "bitstreamParsing/gof.hpp"
#include "mapDecoding/mapDecoding.hpp"
#include "reconstruction/reconstruction.hpp"
#include "adaptation/adaptation.hpp"
//#include "utils/fileExport.hpp"
//#include "utils/jobManagement.hpp"
#include "utils/parameters.hpp"
//#include "utils/preset.hpp"
#include "utils/threadqueue.hpp"
#include "uvgvpcc/log.hpp"

namespace uvgvpcc_dec {

namespace {

Parameters param;



} // anonymous namespace

void API::emptyFrameQueue(std::shared_ptr<uvgvpcc_dec::ThreadQueue>& queue, std::shared_ptr<uvgvpcc_dec::Job>& last_out) {
    if (last_out != nullptr) {
        queue->waitForJob(last_out);
    }
}

const Parameters* p_ = &param;

void API::initializeDecoder() {
    param.useTMC2AttributeYUVConversion = false;
    param.fast_color_conversion = false;

    param.exportIntermediateFiles = false;
    param.intermediateFilesDir = "/home/nhan/nhan/uvgvpccdec_WORKSPACE/intermediate_files";

    // Initialize decoders
    MapDecoding::initializeDecoderPointers();
}


void API::decodeFrame(uvgvpcc_dec::API::v3c_chunk& chunk, point_cloud_frame_stream* output) {

    std::shared_ptr<v3c_gof> v3c_gof_ = std::make_shared<v3c_gof>();
    std::shared_ptr<GOF> currentGOF = std::make_shared<GOF>();
    
    currentGOF->gofId = chunk.gof_id;
    currentGOF->gofCount = chunk.gof_count;

    // Bitstream parsing
    BitstreamParsing::parseV3CGOFBitstream(currentGOF, v3c_gof_, *(p_), chunk);
    
    // Map decoding
    MapDecoding::decodeGOFMaps(currentGOF);

    // Color inversion
    

    // Reconstruction 
    Reconstruction::reconstructPointCloud(currentGOF, v3c_gof_);

    // Adaptation
    Adaptation::adapt(currentGOF, output);
        
    // printf("Decoding completed.\n");
}

} // namespace uvgvpcc_dec
