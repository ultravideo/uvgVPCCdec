#include <fstream>

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "Decompression/decompression.hpp"
#include "FormatConversion/formatConversion.hpp"
#include "Reconstruction/reconstruction.hpp"
#include "PostReconstruction/postReconstruction.hpp"
#include "Adaptation/adaptation.hpp"

namespace uvgvpcc_dec
{

context dec_context_;
size_t num_threads_ = 16;

void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log(LogLevel::INFO, "API", "Initialize decoder " + std::to_string(param.hello) + "\n");
    dec_context_.queue = std::make_shared<ThreadQueue>(num_threads_);
    Decompression::initializeStaticParameters(param);
    Reconstruction::initializeStaticParameters(param, &dec_context_);
}

void API::decodeV3CChunk(v3c_chunk &chunk)
{
    std::vector<decompressed_gof> decompressed_gofs;

    // Unit stream decompression per input bitstream
    Decompression::decompressV3CUnitStream(chunk, &decompressed_gofs);
    for (size_t gof_i = 0; gof_i < decompressed_gofs.size(); gof_i++) {

        // Format conversion per GOF
        FormatConversion::convertToNominalFormat(&decompressed_gofs.at(gof_i));
        for (size_t frame_index = 0; frame_index < decompressed_gofs.at(gof_i).frame_count; frame_index++) {
            // Point cloud reconstruction -> per frame
            point_cloud_frame reconstructed_point_cloud_frame;
            Reconstruction::construct_point_cloud_frame(decompressed_gofs.at(gof_i), &reconstructed_point_cloud_frame, frame_index);
            PostReconstruction::PostProcess(decompressed_gofs.at(gof_i), &reconstructed_point_cloud_frame, frame_index);
            Adaptation::convertYUV8ToRGB8(&reconstructed_point_cloud_frame);
            std::string out_name = "output-test-gof" + std::to_string(gof_i) + "-f" + std::to_string(frame_index) + ".ply";
            Adaptation::write(out_name, &reconstructed_point_cloud_frame);
        }
    }
}

void API::decodeV3CSampleStream(std::vector<uint8_t> &data)
{
    std::vector<decompressed_gof> decompressed_gofs;

    // Sample stream decompression per input bitstream
    Decompression::decompressV3CSampleStream(data, &decompressed_gofs);
    for (size_t gof_i = 0; gof_i < decompressed_gofs.size(); gof_i++) {

        // Format conversion per GOF
        FormatConversion::convertToNominalFormat(&decompressed_gofs.at(gof_i));
        for (size_t frame_index = 0; frame_index < decompressed_gofs.at(gof_i).frame_count; frame_index++) {

            // Point cloud reconstruction -> per frame
            point_cloud_frame reconstructed_point_cloud_frame;
            Reconstruction::construct_point_cloud_frame(decompressed_gofs.at(gof_i), &reconstructed_point_cloud_frame, frame_index);
            PostReconstruction::PostProcess(decompressed_gofs.at(gof_i), &reconstructed_point_cloud_frame, frame_index);
            Adaptation::convertYUV8ToRGB8(&reconstructed_point_cloud_frame);
            std::string out_name = "output-test-gof" + std::to_string(gof_i) + "-f" + std::to_string(frame_index) + ".ply";
            Adaptation::write(out_name, &reconstructed_point_cloud_frame);
        }
    }
}
}

