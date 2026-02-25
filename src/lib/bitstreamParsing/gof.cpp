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

void read_map_bitstream(std::vector<uint8_t>& map_bitstream) {
    const char hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};
    size_t read_size = 0;
    size_t total_size = map_bitstream.size();
    
    int NUM_FRAME_CHECKED = 0;
    while (read_size < total_size)
    {
        size_t nalu_size = bitstream_read_size_from_poiter(&map_bitstream[read_size], 4);
        read_size += 4; // size field length

        size_t hevc_nal_type = map_bitstream[read_size] >> 1;
        printf("nalu_size: %d, hevc_nal_type: %d\n", (int)nalu_size, (int)hevc_nal_type);

        // Read hevc_start_code; write_ptr += 4; write_ptr += nalu_size;
        read_size += nalu_size;

        if(hevc_nal_type == 19 || hevc_nal_type == 1) {
            NUM_FRAME_CHECKED++;
            printf("Number of frame added: %d\n", NUM_FRAME_CHECKED);
        }
    }
}

void test_function(std::vector<uint8_t>& bitstream) {
    size_t read_ptr = 0;
    size_t write_ptr = 0;

    size_t map_size = bitstream.size();
    while (read_ptr < map_size)
    {
        size_t nalu_size = bitstream_read_size_from_poiter(&bitstream[write_ptr], 4);
        read_ptr += 4 + nalu_size; // size field length

        // uint8_t* bitstream_data;
        // memcpy(bitstream_data, hevc_start_code, 4); // Write HEVC start code to bitstream data
        write_ptr += 4;
        size_t hevc_nal_type = bitstream[write_ptr] >> 1;
        printf("nalu_size: %d, hevc_nal_type: %d\n", (int)nalu_size, (int)hevc_nal_type);

        // memcpy(bitstream_data, &bitstream[write_ptr], nalu_size);
        write_ptr += nalu_size;

        if (hevc_nal_type == 19 || hevc_nal_type == 1) {
            printf("data size %d\n", (int)write_ptr);
            bitstream.erase(bitstream.begin(), bitstream.begin() + write_ptr);
            write_ptr = 0;
        }
    }
}

void v3c_gof::read_v3c_chunk(uvgvpcc_dec::API::v3c_chunk& chunk, std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
    // in->io_mutex.lock();
    // uvgvpcc_dec::API::v3c_chunk chunk = std::move(in->v3c_chunks.front());
    // in->v3c_chunks.pop();
    // in->io_mutex.unlock();

    // uvgvpcc_dec::API::v3c_chunk chunk;
    // {   // scope for mutex lock_guard
    //     std::lock_guard<std::mutex> lock(in->io_mutex);

    //     chunk = std::move(in->v3c_chunks.front());
    //     in->v3c_chunks.pop();
    // }

    // Process chunk
    gof_id_ = chunk.gof_id;

    //v3c_unit_precision_ = in->v3c_unit_size_precision_bytes;

    bitstream_t stream;
    //stream.data = chunk.data.get()->data();
    stream.data = chunk.data->data();
    // printf("%d\n", (int)chunk.data.get()->size());
    // printf("Done 1\n");

    // printf("chunk data SIZE initial: %zu\n", chunk.data->size());
    for (size_t gof_id = 0; gof_id < chunk.v3c_unit_sizes.size(); gof_id++) {
        size_t v3c_unit_size = chunk.v3c_unit_sizes.at(gof_id);
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
                stream.data + stream.len, 
                stream.data + stream.len + v3c_unit_payload_size
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
                stream.data + stream.len, 
                stream.data + stream.len + v3c_unit_payload_size
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
                stream.data + stream.len, 
                stream.data + stream.len + v3c_unit_payload_size
            );
            // v3c_avd_sub_ = std::make_unique<std::vector<uint8_t>>(
            //     stream.data + stream.len, 
            //     stream.data + stream.len + v3c_unit_payload_size
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

    
    // // V3C_VPS
    // //stream.len = 4;
    // bitstream_advance(&stream, 32); 
    // const size_t vps_sub_len = chunk.v3c_unit_sizes.at(0) - 4U;
    // v3c_vps_sub_->set_vps_byte_len(vps_sub_len);
    // //printf("Done 2, v3c_vps_sub_ len: %d\n", (int)vps_sub_len);
    // v3c_vps_sub_->read_vps(&stream);
    // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);

    // //printf("Done reading VPS\n");

    // // V3C_AD
    // const size_t v3c_ad_sub_size = chunk.v3c_unit_sizes.at(1) - 4U;
    // v3c_ad_unit_->set_gof_id(gof_id_);
    // v3c_ad_unit_->set_atlas_sub_size(v3c_ad_sub_size);
    // //printf("Done 3, v3c_ad_unit_ size: %d\n", (int)v3c_ad_unit_->get_atlas_sub_size());
    // // V3C_AD header
    // //stream.len += 4;
    // bitstream_advance(&stream, 32); 
    // //printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
    // v3c_ad_unit_->read_atlas_sub_bitstream(&stream);
    // //printf("Done reading atlas sub bitstream\n");
    // n_frames_ = v3c_ad_unit_->get_atlases().size();
    // bitstream_advance(&stream, v3c_ad_sub_size * 8);
    // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
    // //printf("Done reading AD unit, n_frames_: %d\n", (int)n_frames_);

    // // V3C_OVD
    // const size_t v3c_ovd_sub_size = chunk.v3c_unit_sizes.at(2) - 4U;
    // // V3C_OVD header
    // /*
    //     vuh_unit_type            : 5
    //     vuh_v3c_parameter_set_id : 4
    //     vuh_atlas_id             : 6
    //     vuh_reserved_zero_17bits : 17
    // */
    // bitstream_advance(&stream, 5 + 4 + 6 + 17); 
    // // V3C_OVD NAL sub-bitstream
    // v3c_ovd_sub_ = std::make_unique<std::vector<uint8_t>>(
    //     stream.data + stream.len, 
    //     stream.data + stream.len + v3c_ovd_sub_size
    // );
    // //printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
    // bitstream_advance(&stream, v3c_ovd_sub_size * 8);
    // //bitstream_copy_bytes(v3c_ovd_sub_->data(), stream.data, v3c_ovd_sub_size);
    // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);

    // // V3C_GVD
    // const size_t v3c_gvd_sub_size = chunk.v3c_unit_sizes.at(3) - 4U;
    // // V3C_GVD header
    // /*
    //     vuh_unit_type            : 5
    //     vuh_v3c_parameter_set_id : 4
    //     vuh_atlas_id             : 6
    //     vuh_map_index            : 4
    //     vuh_auxiliary_video_flag : 1
    //     vuh_reserved_zero_12bits : 12
    // */
    // bitstream_advance(&stream, 5 + 4 + 6 + 4 + 1 + 12); 
    // // V3C_GVD NAL sub-bitstream
    // v3c_gvd_sub_ = std::make_unique<std::vector<uint8_t>>(
    //     stream.data + stream.len, 
    //     stream.data + stream.len + v3c_gvd_sub_size 
    // );
    // //printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
    // bitstream_advance(&stream, v3c_gvd_sub_size * 8);
    // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);


    // // V3C_AVD
    // const size_t v3c_avd_sub_size = chunk.v3c_unit_sizes.at(4) - 4U;
    // // V3C_AVD header
    // /*
    //     vuh_unit_type                 : 5
    //     vuh_v3c_parameter_set_id      : 4
    //     vuh_atlas_id                  : 6
    //     vuh_attribute_index           : 7
    //     vuh_attribute_partition_index : 5
    //     vuh_map_index                 : 4
    //     vuh_auxiliary_video_flag      : 1
    // */
    // bitstream_advance(&stream, 5 + 4 + 6 + 7 + 5 + 4 + 1);
    // // V3C_AVD NAL sub-bitstream
    // v3c_avd_sub_ = std::make_unique<std::vector<uint8_t>>(
    //     stream.data + stream.len, 
    //     stream.data + stream.len + v3c_avd_sub_size);
    // //printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);
    // bitstream_advance(&stream, v3c_avd_sub_size * 8);
    // printf("len: %d, bit_pos: %d\n", (int)stream.len, (int)stream.cur_bit);


    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("BITSTREAM GENERATION",
                             "New V3C chunk read, " + std::to_string(gofUVG->gofCount) + " chunk(s) in buffer. \n");
}