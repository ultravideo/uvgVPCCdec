#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "uvgvpccdec/log.hpp"

extern "C" {
  #include <libavcodec/avcodec.h>
  #include <libavutil/opt.h>
  #include <libavutil/imgutils.h>
}

struct bitstream_position {
  uint64_t bytes = 0;
  uint8_t  bits = 0;
};

struct parameter_sets {
   v3c_parameter_set vps;
    atlas_sequence_parameter_set asps;
    atlas_frame_parameter_set afps; 
};

class Decompression {
public: 
    static void initializeStaticParameters(const uvgvpcc_dec::Parameters& param);
    static void decompressV3CSampleStream(const std::vector<uint8_t> &data, std::vector<decompressed_gof>* output, uvgvpcc_dec::video_parameter_set_nals* v_params);
    static void decompressV3CUnitStream(const uvgvpcc_dec::API::v3c_chunk &chunk, std::vector<decompressed_gof>* output, uvgvpcc_dec::video_parameter_set_nals* v_params);

    const static parameter_sets &get_saved_params(const size_t gof_index);

private:
    /* Read functions that can print the values if debug mode is enabled */
    static uint32_t read(uint8_t bits, const std::string &name = "");
    static uint32_t read_ue(const std::string &name = ""); // Exp-Golomb

    /* Read functions without debug prints. The above functions use these under the hood but add the prints */
    static uint32_t read_bits(uint8_t bits);
    static uint32_t read_bits_ue();

    /* Advance bitstream position */
    static void advance_bitstream(std::size_t bits);

    /* Advance to the next full byte. If already at the start of a byte, do nothing */
    static void align_bitstream();

    /* high-level" decompression functions */
    static void handle_v3c_unit(const uint8_t vuh_unit_type, const size_t payload_size, std::vector<decompressed_gof>* output, uvgvpcc_dec::video_parameter_set_nals* v_params);
    static void decode_atlas_frame(atlas_frame* frame, const atlas_tile_layer_rbsp &rbsp);

    /* FFMPEG LIB functions */
    static void convert_video_sub_bitstream(const std::size_t v3c_payload_size_bytes, std::vector<uint8_t> &output, std::vector<size_t> &cut_offs, std::vector<uvgvpcc_dec::video_parameter_set_nalu>* v_params);
    static void decode_video_sub_bitstream(std::vector<uint8_t> &input, std::vector<size_t> &cut_offs, video_map* map);
    static std::vector<AVFrame*> decode_video_data(std::vector<uint8_t> &input, std::vector<size_t> &cut_offs);

    /* FFMPEG APP functions */
    static void convert_video_sub_bitstream(const std::size_t v3c_payload_size_bytes, const std::string output_path, std::vector<uvgvpcc_dec::video_parameter_set_nalu>* v_params);
    static void decode_video_sub_bitstream(const std::string input_path, const std::string output_path, video_map* map);



    /* "low-level" parsing functions */
    static void read_v3c_parameter_set(v3c_parameter_set* vps);
    static void read_profile_tier_level(profile_tier_level* ptl);
    static void read_atlas_sub_bitstream(std::size_t v3c_payload_size_bytes, decompressed_gof* output);
    static void read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size, decompressed_gof* output);
    static void read_asps(atlas_sequence_parameter_set &asps);
    static void read_afps(atlas_frame_parameter_set &afps);
    static void read_atlas_rbsp(atlas_tile_layer_rbsp* rbsp, NAL_UNIT_TYPE nalu_t);
    static void read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath);
    static void read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid);
    static void read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu);
    static void read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t);


};
