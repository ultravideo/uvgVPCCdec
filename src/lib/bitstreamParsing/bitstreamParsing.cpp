#include "bitstreamParsing.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "atlas_context.hpp"
#include "bitstream_common.hpp"
#include "gof.hpp"
#include "video_sub_bitstream.hpp"
#include "vps.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

/// \file Entry point for the whole bitstream parsing process.

using namespace uvgvpcc_dec;


void BitstreamParsing::parseV3CGOFBitstream(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_, 
                                            const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::API::v3c_chunk& chunk) {
    printf("Start parsing GOF %zu bitstream.\n", gofUVG->gofId);

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
        "BITSTREAM PARSING", "GOF " + std::to_string(gofUVG->gofId) + " : Parse V3C GOF bitstream using uvgVPCC.\n"); 
    
    v3c_gof &gof = *gof_.get();
    gof.read_v3c_chunk(chunk, gofUVG);

    size_t v3c_max_size = gof.get_v3c_vps()->vps_length_bytes_;
    size_t v3c_temp_size;
    v3c_temp_size = gof.get_v3c_atlas_context()->get_atlas_sub_size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamOccupancy.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamGeometry.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamAttribute.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    const uint32_t v3c_precision =
        static_cast<uint32_t>(std::min(std::max(static_cast<int>(ceil(static_cast<double>(ceilLog2(v3c_max_size)) / 8.0)), 1), 8));
    gof.set_v3c_unit_precision(v3c_precision);

    printf("Parsing GOF %zu bitstream completed.\n", gofUVG->gofId);

    // printf("OVD sub size: %d\n", (int)gofUVG->bitstreamOccupancy.size());
    // printf("GVD sub size: %d\n", (int)gofUVG->bitstreamGeometry.size());
    // printf("AVD sub size: %d\n", (int)gofUVG->bitstreamAttribute.size());
    // printf("GOF %zu: Bitstream parsing completed.\n", gofUVG->gofId);
}

void BitstreamParsing::parseV3CGOFBitstream_parallel(std::shared_ptr<uvgvpcc_dec::GOF> gofUVG, std::shared_ptr<v3c_gof> gof_, 
                                            const uvgvpcc_dec::Parameters& param, std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk) {
    // printf("Start parsing GOF %zu bitstream.\n", gofUVG->gofId);

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
        "BITSTREAM PARSING", "GOF " + std::to_string(gofUVG->gofId) + " : Parse V3C GOF bitstream using uvgVPCC.\n"); 
    
    v3c_gof &gof = *gof_;
    gof.read_v3c_chunk_parallel(chunk, gofUVG);

    size_t v3c_max_size = gof.get_v3c_vps()->vps_length_bytes_;
    size_t v3c_temp_size;
    v3c_temp_size = gof.get_v3c_atlas_context()->get_atlas_sub_size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamOccupancy.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamGeometry.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamAttribute.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    const uint32_t v3c_precision =
        static_cast<uint32_t>(std::min(std::max(static_cast<int>(ceil(static_cast<double>(ceilLog2(v3c_max_size)) / 8.0)), 1), 8));
    gof.set_v3c_unit_precision(v3c_precision);

    // printf("Parsing GOF %zu bitstream completed.\n", gofUVG->gofId);

    // printf("OVD sub size: %d\n", (int)gofUVG->bitstreamOccupancy.size());
    // printf("GVD sub size: %d\n", (int)gofUVG->bitstreamGeometry.size());
    // printf("AVD sub size: %d\n", (int)gofUVG->bitstreamAttribute.size());
    // printf("GOF %zu: Bitstream parsing completed.\n", gofUVG->gofId);
}

void BitstreamParsing::parseV3CGOFBitstream_separate_vuh_units(std::shared_ptr<uvgvpcc_dec::GOF> gofUVG, std::shared_ptr<v3c_gof> gof_, 
                                            const uvgvpcc_dec::Parameters& param, std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk) {
    // printf("Start parsing GOF %zu bitstream.\n", gofUVG->gofId);

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
        "BITSTREAM PARSING", "GOF " + std::to_string(gofUVG->gofId) + " : Parse V3C GOF bitstream using uvgVPCC.\n"); 
    
    v3c_gof &gof = *gof_;
    gof.read_v3c_chunk_separate_vuh_units(chunk, gofUVG);

    size_t v3c_max_size = gof.get_v3c_vps()->vps_length_bytes_;
    size_t v3c_temp_size;
    v3c_temp_size = gof.get_v3c_atlas_context()->get_atlas_sub_size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamOccupancy.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamGeometry.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    v3c_temp_size = gofUVG->bitstreamAttribute.size() + 4;
    if (v3c_temp_size > v3c_max_size) {
        v3c_max_size = v3c_temp_size;
    }
    const uint32_t v3c_precision =
        static_cast<uint32_t>(std::min(std::max(static_cast<int>(ceil(static_cast<double>(ceilLog2(v3c_max_size)) / 8.0)), 1), 8));
    gof.set_v3c_unit_precision(v3c_precision);
}




// void BitstreamParsing::parseV3CGOFBitstream(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const uvgvpcc_dec::Parameters& param,
//                                             uvgvpcc_dec::API::v3c_unit_stream* input) {
//     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
//         "BITSTREAM PARSING", "GOF " + std::to_string(gofUVG->gofId) + " : Parse V3C GOF bitstream using uvgVPCC.\n"); 
    
//     input->io_mutex.lock();
//     uvgvpcc_dec::API::v3c_chunk chunk = std::move(input->v3c_chunks.front());
//     input->v3c_chunks.pop();
//     input->io_mutex.unlock();

//     // Process chunk

//     bitstream_t stream;
//     stream.data = chunk.data->data();
//     // printf("%d\n", (int)chunk.data.get()->size());
//     // printf("Done 1\n");

//     // printf("chunk data SIZE initial: %zu\n", chunk.data->size());
//     for (size_t gof_id = 0; gof_id < chunk.v3c_unit_sizes.size(); gof_id++) {
//         size_t v3c_unit_size = chunk.v3c_unit_sizes.at(gof_id);
//         size_t v3c_unit_payload_size = v3c_unit_size - 4;

//         uint8_t vuh_unit_type = bitstream_read(&stream, 5);
//         bitstream_advance(&stream, 4 * 8 - 5); // skip the rest of v3c header for now (4 bytes total)
//         //printf("len: %d, bit_pos: %d, type: %d\n", (int)stream.len, (int)stream.cur_bit, (int)vuh_unit_type);
//         if (vuh_unit_type == V3C_UNIT_TYPE::V3C_VPS) {
//             gofUVG->v3c_vps_sub_.set_vps_byte_len(v3c_unit_size);
//             gofUVG->v3c_vps_sub_.read_vps(&stream);
//             if (gofUVG->v3c_vps_sub_.get_map_count(0) == 2) {
//                 gofUVG->doubleLayer = true;
//             }
//             //printf("V3C_VPS sub size: %zu\n", v3c_vps_sub_->get_vps_byte_len());
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AD) {
//             gofUVG->v3c_ad_unit_.set_gof_id(gofUVG->gofId);
//             gofUVG->v3c_ad_unit_.set_atlas_sub_size(v3c_unit_size);
//             gofUVG->v3c_ad_unit_.read_atlas_sub_bitstream(v3c_unit_payload_size, gofUVG, &stream);
//             gofUVG->nbFrames = gofUVG->v3c_ad_unit_.get_atlases().size();
//             //printf("len: %d, bit_pos: %d, type: %d\n", (int)stream.len, (int)stream.cur_bit, (int)vuh_unit_type);
//             // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
//             //printf("Done reading atlas sub bitstream\n");
//             // printf("Done reading AD unit, n_frames_: %d\n", (int)n_frames_);
//             printf("Atlas sub size: %zu\n", gofUVG->v3c_ad_unit_.get_atlas_sub_size());
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_OVD) {
//             // Occupancy map
//             gofUVG->bitstreamOccupancy.insert(
//                 gofUVG->bitstreamOccupancy.end(), 
//                 stream.data + stream.len, 
//                 stream.data + stream.len + v3c_unit_payload_size
//             );
//             //test_function(*v3c_ovd_sub_);
//             //printf("OVD sub size: %zu\n", gofUVG->bitstreamOccupancy.size());
//             //printf("chunk data size: %zu\n", chunk.data->size());
//             bitstream_advance(&stream, v3c_unit_payload_size * 8);
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_GVD) {
//             // Geometry map
//             gofUVG->bitstreamGeometry.insert(
//                 gofUVG->bitstreamGeometry.end(),
//                 stream.data + stream.len, 
//                 stream.data + stream.len + v3c_unit_payload_size 
//             );
//             //test_function(*v3c_gvd_sub_);
//             //printf("GVD sub size: %zu\n", gofUVG->bitstreamGeometry.size());
//             //printf("chunk data size: %zu\n", chunk.data->size());
//             bitstream_advance(&stream, v3c_unit_payload_size * 8);
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AVD) {
//             // Attribute map
//             gofUVG->bitstreamAttribute.insert(
//                 gofUVG->bitstreamAttribute.end(),
//                 stream.data + stream.len, 
//                 stream.data + stream.len + v3c_unit_payload_size 
//             );
//             //test_function(*v3c_avd_sub_);
//             //printf("AVD sub size: %zu\n", gofUVG->bitstreamAttribute.size());
//             //printf("chunk data size: %zu\n", chunk.data->size());
//             bitstream_advance(&stream, v3c_unit_payload_size * 8);
//         }
//         //bitstream_advance(&stream, v3c_unit_payload_size * 8);
        
//         uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("Parser", "V3C unit size " + std::to_string(v3c_unit_size)
//             + ", type " + std::to_string(vuh_unit_type) + " v3c_unit_payload_size " + std::to_string(v3c_unit_payload_size) + " \n");
//     }

//     chunk.data->clear();

//     size_t v3c_max_size = gofUVG->v3c_vps_sub_.get_vps_byte_len();
//     size_t v3c_temp_size;
//     v3c_temp_size = gofUVG->v3c_ad_unit_.get_atlas_sub_size() + 4;
//     if (v3c_temp_size > v3c_max_size) {
//         v3c_max_size = v3c_temp_size;
//     }
//     v3c_temp_size = gofUVG->bitstreamOccupancy.size() + 4;
//     if (v3c_temp_size > v3c_max_size) {
//         v3c_max_size = v3c_temp_size;
//     }
//     v3c_temp_size = gofUVG->bitstreamGeometry.size() + 4;
//     if (v3c_temp_size > v3c_max_size) {
//         v3c_max_size = v3c_temp_size;
//     }
//     v3c_temp_size = gofUVG->bitstreamAttribute.size() + 4;
//     if (v3c_temp_size > v3c_max_size) {
//         v3c_max_size = v3c_temp_size;
//     }
//     const uint32_t v3c_precision =
//         static_cast<uint32_t>(std::min(std::max(static_cast<int>(ceil(static_cast<double>(ceilLog2(v3c_max_size)) / 8.0)), 1), 8));
//     gofUVG->v3c_unit_precision_ = v3c_precision;

//     printf("OVD sub size: %d\n", (int)gofUVG->bitstreamOccupancy.size());
//     printf("GVD sub size: %d\n", (int)gofUVG->bitstreamGeometry.size());
//     printf("AVD sub size: %d\n", (int)gofUVG->bitstreamAttribute.size());

//     //gofUVG->nbFrames = gof.get_n_frames();

//     //gofUVG->gofId = gof.get_gof_id();

//     printf("GOF %zu: Bitstream parsing completed.\n", gofUVG->gofId);
// }