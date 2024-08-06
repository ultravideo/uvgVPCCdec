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

AVCodecContext* occupancy_codec_ctx_ = nullptr;
AVCodecContext* geometry_codec_ctx_ = nullptr;
AVCodecContext* attribute_codec_ctx_ = nullptr;

void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log(LogLevel::INFO, "API", "Initialize decoder " + std::to_string(param.hello) + "\n");
    dec_context_.queue = std::make_shared<ThreadQueue>(num_threads_);
    Decompression::initializeStaticParameters(param, &dec_context_);
    Reconstruction::initializeStaticParameters(param, &dec_context_);

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H265);
    if (!codec){
        throw std::runtime_error("Codec not found");
    }

    occupancy_codec_ctx_ = avcodec_alloc_context3(codec);
    geometry_codec_ctx_ = avcodec_alloc_context3(codec);
    attribute_codec_ctx_ = avcodec_alloc_context3(codec);

    if (!occupancy_codec_ctx_ || !geometry_codec_ctx_ || !attribute_codec_ctx_){
        throw std::runtime_error("Could not allocate avcodec context");
    }

    if (avcodec_open2(occupancy_codec_ctx_, codec, nullptr) < 0){
        throw std::runtime_error("Could not initialize avcodec context");
    }
    if (avcodec_open2(geometry_codec_ctx_, codec, nullptr) < 0){
        throw std::runtime_error("Could not initialize avcodec context");
    }
    if (avcodec_open2(attribute_codec_ctx_, codec, nullptr) < 0){
        throw std::runtime_error("Could not initialize avcodec context");
    }
}

void API::decodeV3CChunk(v3c_chunk &chunk) 
{
    std::vector<uvgvpcc_dec::gof_info> gofs_infos = {};
    Decompression::parse_gofs(chunk, gofs_infos);
    for (size_t gof_index = 0; gof_index < gofs_infos.size(); gof_index++) {
        uvgvpcc_dec::gof_info current_raw_gof = gofs_infos.at(gof_index);
        decompressed_gof current_gof;
        const uint8_t* buf = chunk.data.data();

        size_t vps_payload_start = current_raw_gof.vps_start + 4;
        Decompression::decompress_vps(vps_payload_start, gof_index);
        size_t atlas_payload_start = current_raw_gof.ad_start + 4;
        size_t atlas_payload_size = current_raw_gof.ad_size - 4;
        Decompression::decompress_atlas_sub_bitstream(atlas_payload_size, &current_gof, atlas_payload_start);
        
        /* ------------------ OCCUPANCY ------------------ */
        std::cout << "occupancy v3c nit header start " << current_raw_gof.ovd_start << std::endl;
        size_t occupancy_payload_start = current_raw_gof.ovd_start + 4;
        size_t occupancy_payload_size = current_raw_gof.ovd_size - 4;

        auto occ_dec = std::make_shared<Job>("Decompression::decompress_video_sub_bitstream OCCUPANCY ",
            3, Decompression::decompress_video_sub_bitstream, buf, occupancy_payload_start, occupancy_payload_size,
            std::ref(current_gof.occupancy_map), occupancy_codec_ctx_);
        dec_context_.queue->submitJob(occ_dec);

        /* ------------------ GEOMETRY ------------------ */
        size_t geometry_payload_start = current_raw_gof.gvd_start + 4;
        size_t geometry_payload_size = current_raw_gof.gvd_size - 4;
        video_map new_geo_map;
        current_gof.geometry_maps.push_back(new_geo_map);
        std::cout << "geometry_payload_start " << geometry_payload_start << std::endl;
        auto geo_dec = std::make_shared<Job>("Decompression::decompress_video_sub_bitstream GEOMETRY ",
            3, Decompression::decompress_video_sub_bitstream, buf, geometry_payload_start, geometry_payload_size,
            std::ref(current_gof.geometry_maps.back()), geometry_codec_ctx_);
        dec_context_.queue->submitJob(geo_dec);

        /* ------------------ ATTRIBUTE ------------------ */
        size_t attribute_payload_start = current_raw_gof.avd_start + 4;
        size_t attribute_payload_size = current_raw_gof.avd_size - 4;
        video_map new_atr_map;
        current_gof.attribute_maps.push_back(new_atr_map);
        auto atr_dec = std::make_shared<Job>("Decompression::decompress_video_sub_bitstream ATTRIBUTE ",
            3, Decompression::decompress_video_sub_bitstream, buf, attribute_payload_start, attribute_payload_size,
            std::ref(current_gof.attribute_maps.back()), attribute_codec_ctx_);
        dec_context_.queue->submitJob(atr_dec);

        /* ------------------ FORMAT CONVERSION ------------------ */
        auto format_conversion = std::make_shared<Job>("FormatConversion::convertToNominalFormat ",
            3, FormatConversion::convertToNominalFormat, &current_gof);
        format_conversion->addDependency(occ_dec);
        format_conversion->addDependency(geo_dec);
        format_conversion->addDependency(atr_dec);
        dec_context_.queue->submitJob(format_conversion);

        dec_context_.queue->waitForJob(format_conversion);
        
        
    }

    return;
    /*std::vector<decompressed_gof> decompressed_gofs;
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
    }*/
}

}

