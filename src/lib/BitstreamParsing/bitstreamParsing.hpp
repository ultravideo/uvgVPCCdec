#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "bitstream_common.hpp"
#include "atlas_data_structures.hpp"
#include "vps.hpp"

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
    static void parseV3CSampleStream(const std::vector<uint8_t> &data);

private:
    static uint32_t read_bits(uint8_t bits);
    static uint32_t read_bits_ue();
    static void read_v3c_parameter_set(std::size_t v3c_payload_size_bytes);
    static void read_profile_tier_level(profile_tier_level &ptl);


    static void read_atlas_sub_bitstream(std::size_t v3c_payload_size_bytes);
    static void read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size);
    static void read_asps(atlas_sequence_parameter_set &asps);
    static void read_afps(atlas_frame_parameter_set &afps);
    static void read_atlas_rbsp(atlas_tile_layer_rbsp &rbsp, NAL_UNIT_TYPE nalu_t);

    static void read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath);
    static void read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid);
    static void read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu);
    static void read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t);

    static void read_video_sub_bitstream(std::size_t v3c_payload_size_bytes);
};
