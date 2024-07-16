#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "uvgvpccdec/log.hpp"

struct bitstream_position {
  uint64_t bytes = 0;
  uint8_t  bits = 0;
};

class BitstreamParsing {
public: 
    static uint32_t read(uint8_t bits, const std::string &name = "");
    static uint32_t read_ue(const std::string &name = "");
    static void advance_bitstream(std::size_t bits);

    /* Advance to the next full byte. If already at the start of a byte, do nothing */
    static void align_bitstream();
    static void initializeStaticParameters(const uvgvpcc_dec::Parameters& param);
    static void decompressV3CSampleStream(const std::vector<uint8_t> &data, decompressed_data* output);

private:
    static uint32_t read_bits(uint8_t bits);
    static uint32_t read_bits_ue();
    static void read_v3c_parameter_set(v3c_parameter_set* vps);
    static void read_profile_tier_level(profile_tier_level* ptl);


    static void read_atlas_sub_bitstream(std::size_t v3c_payload_size_bytes);
    static void read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size);
    static void read_asps(atlas_sequence_parameter_set &asps);
    static void read_afps(atlas_frame_parameter_set &afps);
    static void read_atlas_rbsp(atlas_tile_layer_rbsp &rbsp, NAL_UNIT_TYPE nalu_t);

    static void read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath);
    static void read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid);
    static void read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu);
    static void read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t);

    static void convert_video_sub_bitstream(std::size_t v3c_payload_size_bytes, std::string output_path);
    static void decode_video_sub_bitstream(std::string input_path, std::string output_path);
};
