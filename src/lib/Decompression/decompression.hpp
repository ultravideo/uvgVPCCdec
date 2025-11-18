#pragma once

#include "PCCSei.h"

#include "uvgvpccdec/bitstream_common.hpp"
#include "uvgvpccdec/uvgvpccdec.hpp"

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
    size_t gof_index = 0;
    v3c_parameter_set vps;
    atlas_sequence_parameter_set asps;
    atlas_frame_parameter_set afps; 
};

class Decompression {
public: 
    static void initializeStaticParameters(const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::context* context);

    /* Find the boundaries of different groupf of frames */
    static void parse_gofs(const uvgvpcc_dec::v3c_chunk &chunk, std::vector<uvgvpcc_dec::gof_info> &infos);

    static void decompress_v3c_video_unit(
      V3C_UNIT_TYPE vuh_t, 
      uint8_t* buf, 
      std::shared_ptr<uvgvpcc_dec::composition_unit_boundary> in_cu, 
      std::shared_ptr<composition_unit> out_cu
    );
    static void decompress_vps(const size_t location, const size_t gof_index);
    static void decompress_atlas_sub_bitstream(const size_t v3c_payload_size_bytes, composition_unit* output, const size_t location);

    const static parameter_sets &get_saved_params(const size_t gof_index);
private:


    /* ---------------- Bitstream parsing functions ---------------- */

    /* Read functions that can print the values if debug mode is enabled */
    static uint32_t read(uint8_t bits, bitstream_position &pos, const std::string &name = "");
    static uint32_t read_ue(bitstream_position &pos, const std::string &name = ""); // Exp-Golomb

    /* Read functions without debug prints. The above functions use these under the hood but add the prints */
    static uint32_t read_bits(uint8_t bits, bitstream_position &pos);
    static uint32_t read_bits_ue(bitstream_position &pos);

    /* Advance bitstream position */
    static void advance_bitstream(std::size_t bits, bitstream_position &pos);

    /* Advance to the next full byte. If already at the start of a byte, do nothing */
    static void align_bitstream(bitstream_position &pos);

    /* ---------------- "high-level" decompression functions ---------------- */

    /* First convert and then decode a video sub-bitstream */
    static void decompress_video_sub_bitstream(uint8_t* buf, const size_t ptr, const size_t v3c_payload_size_bytes, video_map* map, AVCodecContext* codec_ctx);

    /* V3C video sub-bitstreams have 4-byte fields denoting NAL unit sizes. Convert these to start codes for FFMPEG to work */
    static void convert_video_sub_bitstream(const uint8_t* buf, const size_t ptr, const size_t v3c_payload_size_bytes, std::vector<uint8_t> &output, std::vector<size_t> &frame_boundaries);
    
    /* Decode a video sub-bitstream with start codes as NAL unit delimiters. Save the frames into a video map */
    static void decode_video_sub_bitstream(std::vector<uint8_t> &input, std::vector<size_t> &frame_boundaries, video_map &map, AVCodecContext* codec_ctx);
    
    /* Decode video frames using FFMPEG. decode_video_sub_bitstream() calls this */
    static std::vector<AVFrame*> decode_video_frames(std::vector<uint8_t> &input, std::vector<size_t> &frame_boundaries, AVCodecContext* codec_ctx);
    
    /* Decode atlas frame. NOTE: Heavily from TMC2 */
    static void decode_atlas_frame(atlas_frame* frame, const atlas_tile_layer_rbsp &rbsp);

    /* ---------------- "low-level" parsing functions ----------------" */
    static void read_v3c_parameter_set(v3c_parameter_set* vps, bitstream_position &ptr);
    static void read_profile_tier_level(profile_tier_level* ptl, bitstream_position &ptr);
    static void read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size, composition_unit* output, bitstream_position &ptr);
    static void read_asps(atlas_sequence_parameter_set &asps, bitstream_position &ptr);
    static void read_afps(atlas_frame_parameter_set &afps, bitstream_position &ptr);
    static void read_atlas_rbsp(atlas_tile_layer_rbsp* rbsp, NAL_UNIT_TYPE nalu_t, bitstream_position &ptr);
    static void read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath, bitstream_position &ptr);
    static void read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid, bitstream_position &ptr);
    static void read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu, bitstream_position &ptr);
    static void read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t, bitstream_position &ptr);

    /* lf addition */
    static void rbspTrailingBits( bitstream_position &ptr);
    static void seiPayload( bitstream_position &ptr,
                                     pcc::NalUnitType         nalUnitType,
                                     pcc::SeiPayloadType      payloadType,
                                     size_t              payloadSize,
                                     pcc::PCCSEI&             seiList );
    static void seiMessage( bitstream_position &ptr,
                                     NAL_UNIT_TYPE         nalUnitType,
                                     pcc::PCCSEI&             sei );

    static void seiRbsp( 
                                  bitstream_position &ptr,
                                  NAL_UNIT_TYPE         nalUnitType,
                                  pcc::PCCSEI&             sei );                                 


};
