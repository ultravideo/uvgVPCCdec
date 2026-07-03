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
// Map storing all parameters intending to change the state of the decoder from outside of the library.
std::unordered_map<std::string, std::string> apiInputParameters;

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

void parseUvgvpccParameters() {
    // Special parameters need to be handle first
    // ...

    // Now that the preset is applied, all other parameters set by the application can overwrite the preset values.
    for (const auto& paramPair : apiInputParameters) {
        // if (paramPair.first == "presetName" || paramPair.first == "geoBitDepthInput" || paramPair.first == "rate" ||
        //     paramPair.first == "logLevel" || paramPair.first == "errorsAreFatal" || paramPair.first == "mode") {
        //     // Those parameters have been handled at the top of this function
        //     continue;
        // }
        setParameterValue(paramPair.first, paramPair.second, false);
    }

    const std::string detectedThreadNumber = std::to_string(std::thread::hardware_concurrency());
    if (p_->nbThreadPCPart == 0) {
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API",
                                                              "'nbThreadPCPart' is set to 0. The number of thread used for the Point Cloud "
                                                              "part of uvgVPCC is then the detected number of threads: " +
                                                                  detectedThreadNumber + "\n");
        setParameterValue("nbThreadPCPart", detectedThreadNumber, false);
    }
    // if (p_->maxConcurrentFrames == 0) {
    //     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API",
    //                                                           "'maxConcurrentFrames' is set to 0. The maximum number of frame processed in "
    //                                                           "parallel by uvgVPCC is then the four times GOF size: " +
    //                                                               std::to_string(4 * p_->sizeGOF) + "\n");
    //     setParameterValue("maxConcurrentFrames", std::to_string(4 * p_->sizeGOF), false);
    // }

    // if (p_->occupancyEncodingNbThread == 0) {
    //     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::DEBUG>("API",
    //                                                            "'occupancyEncodingNbThread' is set to 0. The number of thread used for the "
    //                                                            "occcupancy video 2D encoding is then the detected number of threads: " +
    //                                                                detectedThreadNumber + "\n");
    //     setParameterValue("occupancyEncodingNbThread", detectedThreadNumber, false);
    // }
    // if (p_->geometryEncodingNbThread == 0) {
    //     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::DEBUG>("API",
    //                                                            "'geometryEncodingNbThread' is set to 0. The number of thread used for the "
    //                                                            "geometry video 2D encoding is then the detected number of threads: " +
    //                                                                detectedThreadNumber + "\n");
    //     setParameterValue("geometryEncodingNbThread", detectedThreadNumber, false);
    // }
    // if (p_->attributeEncodingNbThread == 0) {
    //     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::DEBUG>("API",
    //                                                            "'attributeEncodingNbThread' is set to 0. The number of thread used for the "
    //                                                            "attribute video 2D encoding is then the detected number of threads: " +
    //                                                                detectedThreadNumber + "\n");
    //     setParameterValue("attributeEncodingNbThread", detectedThreadNumber, false);
    // }

    // if (p_->exportIntermediateFiles && p_->intermediateFilesDirTimeStamp) {
    //     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::DEBUG>(
    //         "API", "'intermediateFilesDirTimeStamp' is true, so a time stamp is added to the 'intermediateFilesDir' path.\n");

    //     std::time_t now = std::time(nullptr);
    //     std::tm* localTime = std::localtime(&now);

    //     std::ostringstream oss;
    //     oss << std::setfill('0') << std::setw(2) << (localTime->tm_year % 100) << std::setw(2) << (localTime->tm_mon + 1) << std::setw(2)
    //         << localTime->tm_mday << std::setw(2) << localTime->tm_hour << std::setw(2) << localTime->tm_min << std::setw(2)
    //         << localTime->tm_sec;

    //     std::string dir = p_->intermediateFilesDir;
    //     if (!dir.empty() && dir.back() == '/') {
    //         dir.pop_back();  // Remove trailing slash
    //     }

    //     setParameterValue("intermediateFilesDir", dir + oss.str(), false);
    // }
}

static void initializeContext() {
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("API", "Initialize context.\n");
    JobManager::initThreadQueue(param.nbThreadPCPart);
    printf("Decoder info: \n---> #threads = %zu, useTMC2AttributeYUVConversion = %d, fastColorConversion = %d\n", 
        p_->nbThreadPCPart, p_->useTMC2AttributeYUVConversion, p_->fastColorConversion);
    g_threadHandler.gofIds.push_back(0);
}

} // anonymous namespace

void API::setParameter(const std::string& parameterName, const std::string& parameterValue) {
    // if (initializationDone) {
    //     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::FATAL>(
    //         "API", "The API function 'setParameter' can't be called after the API function 'initializeEncoder'.\n");
    //     throw std::runtime_error("");
    // }
    if (apiInputParameters.find(parameterName) != apiInputParameters.end()) {
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::ERROR>("API", "The parameter '" + parameterName +
                                                                          "' has already been set. The value used is: '" +
                                                                          apiInputParameters.at(parameterName) + "'.\n");
        // errorInAPI = true;
    }
    apiInputParameters.emplace(parameterName, parameterValue);
}

void API::emptyFrameQueue(std::shared_ptr<uvgvpcc_dec::ThreadQueue>& queue, std::shared_ptr<uvgvpcc_dec::Job>& last_out) {
    if (last_out != nullptr) {
        queue->waitForJob(last_out);
    }
}

const Parameters* p_ = &param;

void API::initializeDecoder() {
    // param.useTMC2AttributeYUVConversion = false;
    // param.fastColorConversion = true;
    param.exportIntermediateFiles = false;
    // param.intermediateFilesDir = "/home/nhan/nhan/uvgvpccdec_WORKSPACE/intermediate_files";

    uvgvpcc_dec::initializeParameterMap(param);
    uvgvpcc_dec::parseUvgvpccParameters();

    // printf("Decoder info: \n---> #threads = %zu, useTMC2AttributeYUVConversion = %d, fastColorConversion = %d\n", 
    //     param.nbThreadPCPart, param.useTMC2AttributeYUVConversion, param.fastColorConversion);

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

void API::decodeFrame(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool in_order_output, const std::string& outputFilePath) {

    auto v3c_gof_ = std::make_shared<v3c_gof>();
    auto currentGOF = std::make_shared<GOF>();

    currentGOF->gofId = chunk->gof_id;
    currentGOF->gofCount = chunk->gof_count;

    BitstreamParsing::parseV3CGOFBitstream_separate_vuh_units(currentGOF, v3c_gof_, *(p_), chunk);
    try
    {
        MapDecoding::decodeGOFMaps(currentGOF);
        printf("--------------> Map Encodings Done, width: %d, height: %d\n", (int)currentGOF->attribute_map_width, (int)currentGOF->attribute_map_height);
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

    if (chunk == nullptr) {
        Logger::log<LogLevel::ERROR>("API", "The chunk is null.\n");
        if (p_->errorsAreFatal) {
            throw std::runtime_error("The chunk is null.");
        }
        return;
    }
    auto chunk_copy = std::make_shared<uvgvpcc_dec::API::v3c_chunk>(std::move(*chunk));
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


void API::decodeFrame_delay(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output) {
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
        return; 
    }
    
    Reconstruction::reconstructPointCloud(currentGOF, v3c_gof_);
    Adaptation::adapt_delay(currentGOF, output);
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("API", "GOF " + std::to_string(currentGOF->gofId) + " decoded and adapted.\n");
}

void API::decodeFrame_parallel_delay(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, uvgvpcc_dec::API::point_cloud_frame_stream* output) {
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

    auto currentGOF_job = JOBG(
        gofId,
        5,
        API::decodeFrame_delay,
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

// void API::emptyFrameQueue() {
//     if (!JobManager::threadQueue) return;

//     for (auto job : JobManager::job_vec) {
//         JobManager::threadQueue->waitForJob(job);
//     }
// }

} // namespace uvgvpcc_dec
