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
#include "utils/jobManagement.hpp"
#include "utils/parameters.hpp"
//#include "utils/preset.hpp"
#include "utils/threadqueue.hpp"
#include "uvgvpcc/log.hpp"

namespace uvgvpcc_dec {

namespace {

Parameters param;

// struct ThreadHandler {
//     size_t gofId;
//     std::shared_ptr<GOF> currentGOF;
//     std::shared_ptr<v3c_gof> v3c_gof_;
// };

struct ThreadHandler {
    std::vector<size_t> gofIds;
    std::vector<std::shared_ptr<GOF>> currentGOFs;
    std::vector<std::shared_ptr<v3c_gof>> v3c_gofs;
};

ThreadHandler g_threadHandler;

static void initializeContext() {
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("API", "Initialize context.\n");
    JobManager::initThreadQueue(param.nbThreadPCPart);
    g_threadHandler.gofIds.push_back(0);
}

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
    initializeContext();
}

void API::decodeFrame_remote_output(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::shared_ptr<zmqHandler> zmq_handler) {

    auto v3c_gof_ = std::make_shared<v3c_gof>();
    auto currentGOF = std::make_shared<GOF>();

    currentGOF->gofId = chunk->gof_id;
    currentGOF->gofCount = chunk->gof_count;


    BitstreamParsing::parseV3CGOFBitstream_parallel(currentGOF, v3c_gof_, *(p_), chunk);
    try
    {
        MapDecoding::decodeGOFMaps(currentGOF);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        std::cerr << "Decoding GOF " << currentGOF->gofId << "FAILED!" << '\n';
        
        //current_processes--;
        // printf("Number of Remaning processes: %d\n", current_processes);
        // if (current_processes == 0) {
        //     zmq_handler->colorSocket.close();
        //     zmq_handler->positionSocket.close();
        // }
        return; 
    }
    
    Reconstruction::reconstructPointCloud(currentGOF, v3c_gof_);
    Adaptation::adapt_remote_output(currentGOF, zmq_handler);
    // current_processes--;
    // printf("Number of Remaning processes: %d\n", current_processes);
    // if (current_processes == 0) {
        // zmq_handler->colorSocket.close();
        // zmq_handler->positionSocket.close();
        // zmq_handler->context.close();
        // printf("Sent All Frames\n");
    // }
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API", "GOF " + std::to_string(currentGOF->gofId) + " decoded and adapted.\n");
}

void API::decodeFrame_parallel_remote_output(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::shared_ptr<zmqHandler> zmq_handler) {

    auto chunk_copy = std::make_shared<uvgvpcc_dec::API::v3c_chunk>(std::move(*chunk));

    JobManager::submitCurrentFrameJobs();

    if (chunk == nullptr) {
        Logger::log<LogLevel::ERROR>("API", "The chunk is null.\n");
        if (p_->errorsAreFatal) {
            throw std::runtime_error("The chunk is null.");
        }
        return;
    }

    const auto gofId = chunk_copy->gof_id;
    // printf("Start decoding GOF %zu / %zu\n", gofId + 1, chunk_copy->gof_count);

    auto currentGOF_job = JOBG(
        gofId,
        5,
        API::decodeFrame_remote_output,
        chunk_copy,
        zmq_handler
    );

    JobManager::submitCurrentGOFJobs();
}

// void API::decodeFrame(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::string& outputFilePath) {

//     auto v3c_gof_ = std::make_shared<v3c_gof>();
//     auto currentGOF = std::make_shared<GOF>();

//     currentGOF->gofId = chunk->gof_id;
//     currentGOF->gofCount = chunk->gof_count;

//     BitstreamParsing::parseV3CGOFBitstream_parallel(currentGOF, v3c_gof_, *(p_), chunk);
//     try
//     {
//         MapDecoding::decodeGOFMaps(currentGOF);
//     }
//     catch(const std::exception& e)
//     {
//         std::cerr << e.what() << '\n';
//         std::cerr << "Decoding GOF " << currentGOF->gofId << "FAILED!" << '\n';
//         return; 
//     }
    
//     Reconstruction::reconstructPointCloud(currentGOF, v3c_gof_);
//     Adaptation::adapt(currentGOF, outputFilePath);
//     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API", "GOF " + std::to_string(currentGOF->gofId) + " decoded and adapted.\n");
// }

// void API::decodeFrame_parallel(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, const std::string& outputFilePath) {
//     // if (chunk == nullptr) {
//     //     Logger::log<LogLevel::ERROR>("API", "The chunk is null.\n");
//     //     if (p_->errorsAreFatal) {
//     //         throw std::runtime_error("The chunk is null.");
//     //     }
//     //     return;
//     // }

//     // static std::shared_ptr<std::counting_semaphore<UINT16_MAX>> concurrentFrameSem =
//     //     std::make_shared<std::counting_semaphore<UINT16_MAX>>(
//     //         std::max<size_t>(1, std::min(p_->maxConcurrentFrames, size_t(UINT16_MAX)))
//     //     );

//     // concurrentFrameSem->acquire();
//     // chunk->conccurentFrameSem = concurrentFrameSem;

//     // const auto gofId = chunk->gof_id;
//     // printf("Start decoding GOF %zu / %zu\n", gofId + 1, chunk->gof_count);

//     // auto currentGOF = std::make_shared<GOF>();
//     // auto v3cGof = std::make_shared<v3c_gof>();

//     // currentGOF->gofId = chunk->gof_id;
//     // currentGOF->gofCount = chunk->gof_count;

//     // auto parseGOFMG = JOBG(
//     //     gofId, 5,
//     //     BitstreamParsing::parseV3CGOFBitstream_parallel,
//     //     currentGOF, v3cGof, *p_, chunk
//     // );

//     // auto decodeGOF = JOBG(
//     //     gofId, 4,
//     //     MapDecoding::decodeGOFMaps,
//     //     currentGOF
//     // );

//     // auto reconsGOF = JOBG(
//     //     gofId, 3,
//     //     Reconstruction::reconstructPointCloud,
//     //     currentGOF, v3cGof
//     // );

//     // auto adaptGOF = JOBG(
//     //     gofId, 2,
//     //     Adaptation::adapt,
//     //     currentGOF, output
//     // );

//     // decodeGOF->addDependency(parseGOFMG);
//     // reconsGOF->addDependency(decodeGOF);
//     // adaptGOF->addDependency(reconsGOF);

//     auto chunk_copy = std::make_shared<uvgvpcc_dec::API::v3c_chunk>(std::move(*chunk));

//     JobManager::submitCurrentFrameJobs();

//     if (chunk == nullptr) {
//         Logger::log<LogLevel::ERROR>("API", "The chunk is null.\n");
//         if (p_->errorsAreFatal) {
//             throw std::runtime_error("The chunk is null.");
//         }
//         return;
//     }

//     const auto gofId = chunk_copy->gof_id;
//     // printf("Start decoding GOF %zu / %zu\n", gofId + 1, chunk_copy->gof_count);

//     auto currentGOF_job = JOBG(
//         gofId,
//         5,
//         API::decodeFrame,
//         chunk_copy,
//         outputFilePath
//     );

//     JobManager::submitCurrentGOFJobs();
// }

void API::decodeFrame(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool in_order_output, const std::string& outputFilePath) {

    auto v3c_gof_ = std::make_shared<v3c_gof>();
    auto currentGOF = std::make_shared<GOF>();

    currentGOF->gofId = chunk->gof_id;
    currentGOF->gofCount = chunk->gof_count;

    BitstreamParsing::parseV3CGOFBitstream_parallel(currentGOF, v3c_gof_, *(p_), chunk);
    try
    {
        MapDecoding::decodeGOFMaps(currentGOF);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        std::cerr << "Decoding GOF " << currentGOF->gofId << "FAILED!" << '\n';
        if (in_order_output) {
            output->available_gofs.at(currentGOF->gofId).terminated = true;
            output->available_gofs.at(currentGOF->gofId).ready = true;
        }
        return; 
    }
    
    Reconstruction::reconstructPointCloud(currentGOF, v3c_gof_);
    Adaptation::adapt(currentGOF, output, in_order_output, outputFilePath);
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API", "GOF " + std::to_string(currentGOF->gofId) + " decoded and adapted.\n");
}

void API::decodeFrame_parallel(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool in_order_output, const std::string& outputFilePath) {
    auto chunk_copy = std::make_shared<uvgvpcc_dec::API::v3c_chunk>(std::move(*chunk));

    JobManager::submitCurrentFrameJobs();

    if (chunk == nullptr) {
        Logger::log<LogLevel::ERROR>("API", "The chunk is null.\n");
        if (p_->errorsAreFatal) {
            throw std::runtime_error("The chunk is null.");
        }
        return;
    }

    const auto gofId = chunk_copy->gof_id;
    // printf("Start decoding GOF %zu / %zu\n", gofId + 1, chunk_copy->gof_count);

    auto currentGOF_job = JOBG(
        gofId,
        5,
        API::decodeFrame,
        chunk_copy,
        output,
        in_order_output,
        outputFilePath
    );

    JobManager::submitCurrentGOFJobs();
}


void API::decodeFrame_in_order(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output) {

    auto v3c_gof_ = std::make_shared<v3c_gof>();
    auto currentGOF = std::make_shared<GOF>();

    currentGOF->gofId = chunk->gof_id;
    currentGOF->gofCount = chunk->gof_count;

    BitstreamParsing::parseV3CGOFBitstream_parallel(currentGOF, v3c_gof_, *(p_), chunk);
    try
    {
        MapDecoding::decodeGOFMaps(currentGOF);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        std::cerr << "Decoding GOF " << currentGOF->gofId << "FAILED!" << '\n';

        output->available_gofs.at(currentGOF->gofId).terminated = true;
        output->available_gofs.at(currentGOF->gofId).ready = true;
        return; 
    }
    
    Reconstruction::reconstructPointCloud(currentGOF, v3c_gof_);
    Adaptation::adapt_in_order(currentGOF, output);
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API", "GOF " + std::to_string(currentGOF->gofId) + " decoded and adapted.\n");
}


void API::decodeFrame_parallel_in_order(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output) {

    auto chunk_copy = std::make_shared<uvgvpcc_dec::API::v3c_chunk>(std::move(*chunk));

    JobManager::submitCurrentFrameJobs();

    if (chunk == nullptr) {
        Logger::log<LogLevel::ERROR>("API", "The chunk is null.\n");
        if (p_->errorsAreFatal) {
            throw std::runtime_error("The chunk is null.");
        }
        return;
    }

    const auto gofId = chunk_copy->gof_id;
    // printf("Start decoding GOF %zu / %zu\n", gofId + 1, chunk_copy->gof_count);

    auto currentGOF_job = JOBG(
        gofId,
        5,
        API::decodeFrame_in_order,
        chunk_copy,
        output
    );

    JobManager::submitCurrentGOFJobs();
}


void API::emptyFrameQueue() {
    if (!JobManager::threadQueue || !JobManager::previousGOFJobMap) {
        return;
    }

    for (const auto& [key, job] : *JobManager::previousGOFJobMap) {
        if (key.getFuncName() == TO_STRING(API::decodeFrame)) {
            JobManager::threadQueue->waitForJob(job);
        }
    }
}

} // namespace uvgvpcc_dec
