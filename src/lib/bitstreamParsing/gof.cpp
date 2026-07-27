#include "gof.hpp"

#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>
#include <vector>
//#include <mutex>

#include "bitstream_common.hpp"
#include "bitstream_util.hpp"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"
#include "video_sub_bitstream.hpp"
#include <cstdint>

void v3c_gof::read_v3c_chunk(uvgvpcc_dec::API::v3c_chunk& chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
    // Process chunk
    gof_id_ = chunk.gof_id;

    //v3c_unit_precision_ = in->v3c_unit_size_precision_bytes;

    bitstream_t stream;
    //stream.data = chunk.data.get()->data();
    stream.data = *chunk.data;
    // printf("%d\n", (int)chunk.data.get()->size());
    // printf("Done 1\n");

    // printf("chunk data SIZE initial: %zu\n", chunk.data->size());
    for (size_t unit_id = 0; unit_id < chunk.v3c_unit_sizes.size(); unit_id++) {
        size_t v3c_unit_size = chunk.v3c_unit_sizes.at(unit_id);
        size_t v3c_unit_payload_size = v3c_unit_size - 4;

        uint8_t vuh_unit_type = bitstream_read(&stream, 5);
        bitstream_advance(&stream, 27); // 4 * 8 - 5 skip the rest of v3c header for now (4 bytes total)
        //printf("len: %d, bit_pos: %d, type: %d\n", (int)stream.len, (int)stream.cur_bit, (int)vuh_unit_type);
        if (vuh_unit_type == V3C_UNIT_TYPE::V3C_VPS) {
            v3c_vps_sub_->vps_length_bytes_ = v3c_unit_size;
            v3c_vps_sub_->read_vps(&stream);
            //printf("V3C_VPS sub size: %zu\n", v3c_vps_sub_->get_vps_byte_len());
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AD) {
            v3c_ad_unit_->set_gof_id(gof_id_);
            v3c_ad_unit_->set_atlas_sub_size(v3c_unit_size);
            v3c_ad_unit_->read_atlas_sub_bitstream(v3c_unit_payload_size, gofUVG, &stream);
            //printf("len: %d, bit_pos: %d, type: %d\n", (int)stream.len, (int)stream.cur_bit, (int)vuh_unit_type);
            // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
            //printf("Done reading atlas sub bitstream\n");
            n_frames_ = v3c_ad_unit_->get_atlases().size();
            gofUVG->nbFrames = n_frames_;
            // printf("Done reading AD unit, n_frames_: %d\n", (int)n_frames_);
            // printf("Atlas sub size: %zu\n", v3c_ad_unit_->get_atlas_sub_size());
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_OVD) {
            gofUVG->bitstreamOccupancy.insert(
                gofUVG->bitstreamOccupancy.end(), 
                stream.data.data() + stream.len, 
                stream.data.data() + stream.len + v3c_unit_payload_size
            );
            // v3c_ovd_sub_ = std::make_unique<std::vector<uint8_t>>(
            //     stream.data + stream.len, 
            //     stream.data + stream.len + v3c_unit_payload_size
            // );
            //test_function(*v3c_ovd_sub_);
            //printf("OVD sub size: %zu\n", v3c_ovd_sub_->size());
            //printf("chunk data size: %zu\n", chunk.data->size());
            bitstream_advance(&stream, v3c_unit_payload_size * 8);
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_GVD) {
            gofUVG->bitstreamGeometry.insert(
                gofUVG->bitstreamGeometry.end(), 
                stream.data.data() + stream.len, 
                stream.data.data() + stream.len + v3c_unit_payload_size
            );
            // v3c_gvd_sub_ = std::make_unique<std::vector<uint8_t>>(
            //     stream.data + stream.len, 
            //     stream.data + stream.len + v3c_unit_payload_size 
            // );
            //test_function(*v3c_gvd_sub_);
            //printf("GVD sub size: %zu\n", v3c_gvd_sub_->size());
            //printf("chunk data size: %zu\n", chunk.data->size());
            bitstream_advance(&stream, v3c_unit_payload_size * 8);
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AVD) {
            gofUVG->bitstreamAttribute.insert(
                gofUVG->bitstreamAttribute.end(), 
                stream.data.data() + stream.len, 
                stream.data.data() + stream.len + v3c_unit_payload_size
            );
            // v3c_avd_sub_ = std::make_unique<std::vector<uint8_t>>(
            //     stream.data.data() + stream.len, 
            //     stream.data.data() + stream.len + v3c_unit_payload_size
            // );
            //test_function(*v3c_avd_sub_);
            //printf("AVD sub size: %zu\n", v3c_avd_sub_->size());
            //printf("chunk data size: %zu\n", chunk.data->size());
            bitstream_advance(&stream, v3c_unit_payload_size * 8);
        }
        //bitstream_advance(&stream, v3c_unit_payload_size * 8);
        
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("Parser", "V3C unit size " + std::to_string(v3c_unit_size)
            + ", type " + std::to_string(vuh_unit_type) + " v3c_unit_payload_size " + std::to_string(v3c_unit_payload_size) + " \n");
    }

    if (v3c_vps_sub_.get()->get_map_count(0) == 2) {
        gofUVG->doubleLayer = true;
    }
    chunk.data->clear();

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("BITSTREAM GENERATION",
                             "New V3C chunk read, " + std::to_string(gofUVG->gofCount) + " chunk(s) in buffer. \n");
}

void v3c_gof::read_v3c_chunk_parallel(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
    // Process chunk
    gof_id_ = chunk->gof_id;

    //v3c_unit_precision_ = in->v3c_unit_size_precision_bytes;

    bitstream_t stream;
    stream.data = *chunk->data;

    // printf("chunk data SIZE initial: %zu\n", chunk.data->size());
    for (size_t gof_id = 0; gof_id < chunk->v3c_unit_sizes.size(); gof_id++) {
        size_t v3c_unit_size = chunk->v3c_unit_sizes.at(gof_id);
        size_t v3c_unit_payload_size = v3c_unit_size - 4;

        uint8_t vuh_unit_type = bitstream_read(&stream, 5);
        bitstream_advance(&stream, 27); // 4 * 8 - 5 skip the rest of v3c header for now (4 bytes total)
        //printf("len: %d, bit_pos: %d, type: %d\n", (int)stream.len, (int)stream.cur_bit, (int)vuh_unit_type);
        if (vuh_unit_type == V3C_UNIT_TYPE::V3C_VPS) {
            v3c_vps_sub_->vps_length_bytes_ = v3c_unit_size;
            v3c_vps_sub_->read_vps(&stream);
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AD) {
            v3c_ad_unit_->set_gof_id(gof_id_);
            v3c_ad_unit_->set_atlas_sub_size(v3c_unit_size);
            v3c_ad_unit_->read_atlas_sub_bitstream(v3c_unit_payload_size, gofUVG, &stream);
            //printf("len: %d, bit_pos: %d, type: %d\n", (int)stream.len, (int)stream.cur_bit, (int)vuh_unit_type);
            // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
            //printf("Done reading atlas sub bitstream\n");
            n_frames_ = v3c_ad_unit_->get_atlases().size();
            gofUVG->nbFrames = n_frames_;
            // printf("Done reading AD unit, n_frames_: %d\n", (int)n_frames_);
            // printf("Atlas sub size: %zu\n", v3c_ad_unit_->get_atlas_sub_size());
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_OVD) {
            gofUVG->bitstreamOccupancy.insert(
                gofUVG->bitstreamOccupancy.end(), 
                stream.data.data() + stream.len, 
                stream.data.data() + stream.len + v3c_unit_payload_size
            );
            bitstream_advance(&stream, v3c_unit_payload_size * 8);
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_GVD) {
            gofUVG->bitstreamGeometry.insert(
                gofUVG->bitstreamGeometry.end(), 
                stream.data.data() + stream.len, 
                stream.data.data() + stream.len + v3c_unit_payload_size
            );
            bitstream_advance(&stream, v3c_unit_payload_size * 8);
        } 
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AVD) {
            gofUVG->bitstreamAttribute.insert(
                gofUVG->bitstreamAttribute.end(), 
                stream.data.data() + stream.len, 
                stream.data.data() + stream.len + v3c_unit_payload_size
            );
            bitstream_advance(&stream, v3c_unit_payload_size * 8);
        }
        
        // uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("Parser", "V3C unit size " + std::to_string(v3c_unit_size)
        //     + ", type " + std::to_string(vuh_unit_type) + " v3c_unit_payload_size " + std::to_string(v3c_unit_payload_size) + " \n");
    }

    if (v3c_vps_sub_.get()->get_map_count(0) == 2) {
        gofUVG->doubleLayer = true;
    }
    
    chunk->data->clear();

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("BITSTREAM GENERATION",
                             "New V3C chunk read, " + std::to_string(gofUVG->gofCount) + " chunk(s) in buffer. \n");
}


// void v3c_gof::read_v3c_chunk_separate_vuh_units(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
//     // Process chunk
//     gof_id_ = chunk->gof_id;

//     printf("read_v3c_chunk_separate_vuh_units of GOF %d\n", (int)gofUVG->gofId);

//     for (size_t unit_index = 0; unit_index < chunk->v3c_unit_sizes.size(); unit_index++) {
//         size_t v3c_unit_size = chunk->v3c_unit_sizes.at(unit_index);
//         size_t v3c_unit_payload_size = v3c_unit_size - 4;

//         bitstream_t stream;
//         stream.data = *chunk->vuh_units[unit_index];

//         uint8_t vuh_unit_type = bitstream_read(&stream, 5);
//         bitstream_advance(&stream, 27);

//         if (vuh_unit_type == V3C_UNIT_TYPE::V3C_VPS) {
//             v3c_vps_sub_->vps_length_bytes_ = v3c_unit_size;
//             v3c_vps_sub_->read_vps(&stream);
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AD) {
//             v3c_ad_unit_->set_gof_id(gof_id_);
//             v3c_ad_unit_->set_atlas_sub_size(v3c_unit_size);
//             v3c_ad_unit_->read_atlas_sub_bitstream(v3c_unit_payload_size, gofUVG, &stream);
//             n_frames_ = v3c_ad_unit_->get_atlases().size();
//             gofUVG->nbFrames = n_frames_;
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_OVD) {
//             gofUVG->bitstreamOccupancy.insert(
//                 gofUVG->bitstreamOccupancy.end(), 
//                 stream.data.data() + stream.len, 
//                 stream.data.data() + stream.len + v3c_unit_payload_size
//             );
//             // stream.len += v3c_unit_payload_size;
//             // printf("Occupancy map video ->%d B\n", (int)gofUVG->bitstreamOccupancy.size());
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_GVD) {
//             gofUVG->bitstreamGeometry.insert(
//                 gofUVG->bitstreamGeometry.end(), 
//                 stream.data.data() + stream.len, 
//                 stream.data.data() + stream.len + v3c_unit_payload_size
//             );
//             // printf("Geometry map video ->%d B\n", (int)gofUVG->bitstreamGeometry.size());
//         } 
//         else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AVD) {
//             gofUVG->bitstreamAttribute.insert(
//                 gofUVG->bitstreamAttribute.end(), 
//                 stream.data.data() + stream.len, 
//                 stream.data.data() + stream.len + v3c_unit_payload_size
//             );
//             // printf("Attribute map video ->%d B\n", (int)gofUVG->bitstreamAttribute.size());
//         }
//         chunk->vuh_units[unit_index].reset();
//     }

//     if (v3c_vps_sub_.get()->get_map_count(0) == 2) {
//         gofUVG->doubleLayer = true;
//     }

//     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("BITSTREAM GENERATION",
//                              "New V3C chunk read, " + std::to_string(gofUVG->gofCount) + " chunk(s) in buffer. \n");
// }


void v3c_gof::read_v3c_chunk_separate_vuh_units(std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
    // Process chunk
    gof_id_ = chunk->gof_id;

    printf("---------> read_v3c_chunk_separate_vuh_units of GOF %d\n", (int)gofUVG->gofId);

    bitstream_t stream;

    // Parse V3C_VPS data
    stream.data = std::move(chunk->vps_unit->data);
    bitstream_advance(&stream, 32);
    v3c_vps_sub_->vps_length_bytes_ = chunk->vps_unit->v3c_unit_size;
    v3c_vps_sub_->read_vps(&stream);
    // printf("---------> Parsing V3C_VPS data done!\n");

    // Parse V3C_AD data
    stream.data = std::move(chunk->ad_unit->data);
    stream.reset();
    bitstream_advance(&stream, 32);
    v3c_ad_unit_->set_gof_id(gof_id_);
    v3c_ad_unit_->set_atlas_sub_size(chunk->ad_unit->v3c_unit_size);
    v3c_ad_unit_->read_atlas_sub_bitstream(chunk->ad_unit->v3c_unit_size - 4, gofUVG, &stream);
    n_frames_ = v3c_ad_unit_->get_atlases().size();
    gofUVG->nbFrames = n_frames_;
    // printf("---------> Parsing V3C_AD data done!\n");

    // Parse V3C_OVD data
    stream.data = std::move(chunk->ovd_unit->data);
    stream.reset();
    bitstream_advance(&stream, 32);
    gofUVG->bitstreamOccupancy.insert(
        gofUVG->bitstreamOccupancy.end(), 
        stream.data.data() + stream.len, 
        stream.data.data() + stream.len + chunk->ovd_unit->v3c_unit_size - 4
    );
    // printf("Occupancy map video ->%d B\n", (int)gofUVG->bitstreamOccupancy.size());
    // printf("---------> Parsing V3C_OVD data done!\n");

    if (v3c_vps_sub_->vps_map_count_minus1_[0] > 0 && v3c_vps_sub_->vps_multiple_map_streams_present_flag_[0]) { // Multiple streams
        // // Parse V3C_GVD data
        // const auto& gvd_units = chunk->gvd_units.get();
        // gofUVG->streams_geometry.resize(gvd_units->size());
        // for (size_t unit_index = 0; unit_index < gvd_units->size(); unit_index++) {
        //     auto& vuh_unit = gvd_units->at(unit_index);
        //     stream.data = vuh_unit.data;
        //     stream.reset();
        //     bitstream_advance(&stream, 32);
            
        //     std::vector<uint8_t> geometryStream;
        //     geometryStream.insert(
        //         geometryStream.end(), 
        //         stream.data.data() + stream.len, 
        //         stream.data.data() + stream.len + vuh_unit.v3c_unit_size - 4
        //     );
        //     gofUVG->streams_geometry[unit_index] = geometryStream;
        //     printf("Geometry map video ->%d B\n", (int)gofUVG->streams_geometry[unit_index].size());
        // }
        // printf("---------> Parsing V3C_GVD data done!\n");

        // // Parse V3C_AVD data
        // const auto& avd_units = chunk->avd_units.get();
        // gofUVG->streams_attribute.resize(avd_units->size());
        // for (size_t unit_index = 0; unit_index < avd_units->size(); unit_index++) {
        //     auto& vuh_unit = avd_units->at(unit_index);
        //     stream.data = vuh_unit.data;
        //     stream.reset();
        //     bitstream_advance(&stream, 32);

        //     std::vector<uint8_t> attributeStream;
        //     attributeStream.insert(
        //         attributeStream.end(), 
        //         stream.data.data() + stream.len, 
        //         stream.data.data() + stream.len + vuh_unit.v3c_unit_size - 4
        //     );
        //     gofUVG->streams_attribute[unit_index] = attributeStream;
        //     printf("Attribute map video ->%d B\n", (int)gofUVG->streams_attribute[unit_index].size());
        // }
        // printf("---------> Parsing V3C_AVD data done!\n");

        // Parse V3C_GVD data
        const auto& gvd_units = chunk->gvd_units.get();
        uvgvpcc_dec::API::vuh_unit& vuh_unit = gvd_units->at(0);
        stream.data = std::move(vuh_unit.data);
        stream.reset();
        bitstream_advance(&stream, 32);
        gofUVG->bitstreamGeometry.insert(
            gofUVG->bitstreamGeometry.end(), 
            stream.data.data() + stream.len, 
            stream.data.data() + stream.len + vuh_unit.v3c_unit_size - 4
        );
        // printf("Geometry map video ->%d B\n", (int)gofUVG->bitstreamGeometry.size());
        // printf("---------> Parsing V3C_GVD data done!\n");

        // Parse V3C_AVD data
        const auto& avd_units = chunk->avd_units.get();
        vuh_unit = avd_units->at(0);
        stream.data = std::move(vuh_unit.data);
        stream.reset();
        bitstream_advance(&stream, 32);
        gofUVG->bitstreamAttribute.insert(
            gofUVG->bitstreamAttribute.end(), 
            stream.data.data() + stream.len, 
            stream.data.data() + stream.len + vuh_unit.v3c_unit_size - 4
        );
        // printf("Attribute map video ->%d B\n", (int)gofUVG->bitstreamAttribute.size());
        // printf("---------> Parsing V3C_AVD data done!\n");

    } else { // Single stream
        // Parse V3C_GVD data
        const auto& gvd_units = chunk->gvd_units.get();
        uvgvpcc_dec::API::vuh_unit& vuh_unit = gvd_units->at(0);
        stream.data = std::move(vuh_unit.data);
        stream.reset();
        bitstream_advance(&stream, 32);
        gofUVG->bitstreamGeometry.insert(
            gofUVG->bitstreamGeometry.end(), 
            stream.data.data() + stream.len, 
            stream.data.data() + stream.len + vuh_unit.v3c_unit_size - 4
        );
        // printf("Geometry map video ->%d B\n", (int)gofUVG->bitstreamGeometry.size());
        // printf("---------> Parsing V3C_GVD data done!\n");

        // Parse V3C_AVD data
        const auto& avd_units = chunk->avd_units.get();
        vuh_unit = avd_units->at(0);
        stream.data = std::move(vuh_unit.data);
        stream.reset();
        bitstream_advance(&stream, 32);
        gofUVG->bitstreamAttribute.insert(
            gofUVG->bitstreamAttribute.end(), 
            stream.data.data() + stream.len, 
            stream.data.data() + stream.len + vuh_unit.v3c_unit_size - 4
        );
        // printf("Attribute map video ->%d B\n", (int)gofUVG->bitstreamAttribute.size());
        // printf("---------> Parsing V3C_AVD data done!\n");
    }

    if (v3c_vps_sub_.get()->get_map_count(0) == 2 && !v3c_vps_sub_->vps_multiple_map_streams_present_flag_[0]) {
        gofUVG->doubleLayer = true;
    }

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("BITSTREAM GENERATION",
                             "New V3C chunk read, " + std::to_string(gofUVG->gofCount) + " chunk(s) in buffer. \n");
}