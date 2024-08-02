#include "decompression.hpp"
#include <cstring>
#include <sstream>
#include <fstream>
#include <cstdio>
#include <stdlib.h>
#include <stdio.h>

/* In debug mode print out some extra info */
#define BITSTREAM_DEBUG false

using namespace uvgvpcc_dec;

static const uint8_t* cbuf_;
static bitstream_position pos_;
uvgvpcc_dec::context* dec_context1_;

size_t current_gof_index_ = 0;
std::vector<parameter_sets> saved_params_ = {};

size_t occupancy_width_ = 0;
size_t occupancy_height_ = 0;
// geometry and attribute
size_t video_width_ = 0;
size_t video_height_ = 0;

bool keep_intermediate_files_ = false;
std::string ffmpeg_path = "ffmpeg";

// HEVC map filenames Occ, Geo, Atr
std::string o_hevc;
std::string g_hevc;
std::string a_hevc;

// Decoded YUV map filenames Occ, Geo, Atr
std::string o_yuv;
std::string g_yuv;
std::string a_yuv;

AVCodecContext* occupancy_codec_ctx_ = nullptr;
AVCodecContext* geometry_codec_ctx_ = nullptr;
AVCodecContext* attribute_codec_ctx_ = nullptr;

const parameter_sets &Decompression::get_saved_params(const size_t gof_index)
{
    return saved_params_.at(gof_index);
}

/* TODO: make sure this function works in all cases */
void Decompression::advance_bitstream(std::size_t bits)
{
    std::size_t bytes = bits / 8 + (pos_.bits + bits % 8) / 8;
    pos_.bytes += bytes;
    pos_.bits = (pos_.bits + bits % 8) % 8;
}

/* TODO: make sure this function works in all cases */
void Decompression::align_bitstream()
{
    if(pos_.bits != 0) {
        pos_.bytes++;
        pos_.bits = 0;
    }
}

/* NOTE -------------------- Straight from TMC2 -------------------- */
uint32_t Decompression::read_bits(uint8_t bits) {
    uint32_t value = 0;
    for ( std::size_t i = 0; i < bits; i++ ) {
      value |= ( ( cbuf_[pos_.bytes] >> ( 7 - pos_.bits ) ) & 1 ) << ( bits - 1 - i );
      if ( pos_.bits == 7 ) {
        pos_.bytes++;
        pos_.bits = 0;
      } else {
        pos_.bits++;
      }
    }
    return value;
}

/* NOTE -------------------- Straight from TMC2 -------------------- */
uint32_t Decompression::read_bits_ue()
{
    uint32_t value = 0, code = 0, length = 0;
    code = read_bits( 1 );
    if ( 0 == code ) {
      length = 0;
      while ( !( code & 1 ) ) {
        code = read_bits( 1 );
        length++;
      }
      value = read_bits( length );
      value += ( 1 << length ) - 1;
    }
    return value;
}

uint32_t Decompression::read(uint8_t bits, const std::string &name) {
    uint32_t value = read_bits(bits);
#if BITSTREAM_DEBUG
    printf("%-50s u(%u) : %d\n", name.c_str(),bits,value);
#endif
    (void)name; // Suppress unused parameter warning
    return value;
}

uint32_t Decompression::read_ue(const std::string &name)
{
    uint32_t value = read_bits_ue();
#if BITSTREAM_DEBUG
    printf("%-50s u(v) : %d\n", name.c_str(),value);
#endif
    (void)name; // Suppress unused parameter warning
    return value;
}

void Decompression::initializeStaticParameters(const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::context* context)
{
    dec_context1_ = context;
    occupancy_width_ = param.occupancy_width;
    occupancy_height_ = param.occupancy_height;
    // geometry and attribute
    video_width_ = param.video_width;
    video_height_ = param.video_height;
    keep_intermediate_files_ = param.keep_intermediate_files;

    o_hevc = "OCCUPANCY-MAP.hevc";
    g_hevc = "GEOMETRY-MAP.hevc";
    a_hevc = "ATTRIBUTE-MAP.hevc";
    o_yuv = "OCCUPANCY-MAP-" + std::to_string(occupancy_width_) + "x" + std::to_string(occupancy_height_) + ".yuv";
    g_yuv = "GEOMETRY-MAP-" + std::to_string(video_width_) + "x" + std::to_string(video_height_) + ".yuv";
    a_yuv = "ATTRIBUTE-MAP-" + std::to_string(video_width_) + "x" + std::to_string(video_height_) + ".yuv";

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
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Video decoder initialized \n");
    
}

void Decompression::decompressV3CUnitStream(const uvgvpcc_dec::API::v3c_chunk &chunk, std::vector<decompressed_gof>* output)
{
    cbuf_ = chunk.data.data();
    pos_.bits = 0;
    pos_.bytes = 0;

    //std::vector<std::shared_ptr<Job>> video_dec_jobs = {};
    std::shared_ptr<Job> occ_job;
    std::shared_ptr<Job> geo_job;
    std::shared_ptr<Job> atr_job;

    /* In the current version, a single chunk should only have 1 of each type of V3C unit! */
    for (size_t i = 0; i < chunk.v3c_unit_sizes.size(); i++) {
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "V3C unit size " + std::to_string(chunk.v3c_unit_sizes.at(i)) + " \n");
        size_t v3c_unit_payload_size = chunk.v3c_unit_sizes.at(i) - 4; // not incl. header

        // Next 4 bytes are the V3C unit header
        uint8_t vuh_unit_type = read(5, "vuh_unit_type");
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "V3C unit type " + std::to_string(vuh_unit_type) + " \n");
        advance_bitstream(4 * 8 - 5); // skip the rest of v3c header for now

        if(vuh_unit_type == V3C_UNIT_TYPE::V3C_VPS) {
            decompressed_gof new_gof;
            current_gof_index_ = output->size();
            new_gof.gof_index = current_gof_index_;
            output->push_back(std::move(new_gof));
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Added GOF, index " + std::to_string(current_gof_index_) + " \n");
            parameter_sets new_params;
            saved_params_.push_back(new_params);
            read_v3c_parameter_set(&saved_params_.back().vps);
        }
        else if (vuh_unit_type == V3C_UNIT_TYPE::V3C_AD) {
            read_atlas_sub_bitstream(v3c_unit_payload_size, &output->at(current_gof_index_));
        }
        else if (vuh_unit_type == V3C_UNIT_TYPE::V3C_OVD) {
            occ_job = std::make_shared<Job>("Decompression::decompress_video_sub_bitstream type " + std::to_string(vuh_unit_type),
                3, Decompression::decompress_video_sub_bitstream, v3c_unit_payload_size, pos_.bytes,
                std::ref(output->at(current_gof_index_).occupancy_map), occupancy_codec_ctx_);
            dec_context1_->queue->submitJob(occ_job);
            advance_bitstream(v3c_unit_payload_size * 8);
        }
        else if (vuh_unit_type == V3C_UNIT_TYPE::V3C_GVD) {
            video_map new_geo_map;
            output->at(current_gof_index_).geometry_maps.push_back(new_geo_map);
            geo_job = std::make_shared<Job>("Decompression::decompress_video_sub_bitstream type " + std::to_string(vuh_unit_type),
                3, Decompression::decompress_video_sub_bitstream, v3c_unit_payload_size, pos_.bytes,
                std::ref(output->at(current_gof_index_).geometry_maps.back()), geometry_codec_ctx_);
            dec_context1_->queue->submitJob(geo_job);
            advance_bitstream(v3c_unit_payload_size * 8);
        }
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AVD) {
            video_map new_atr_map;
            output->at(current_gof_index_).attribute_maps.push_back(new_atr_map);
            atr_job = std::make_shared<Job>("Decompression::decompress_video_sub_bitstream type " + std::to_string(vuh_unit_type),
                3, Decompression::decompress_video_sub_bitstream, v3c_unit_payload_size, pos_.bytes,
                std::ref(output->at(current_gof_index_).attribute_maps.back()), attribute_codec_ctx_);
            dec_context1_->queue->submitJob(atr_job);
            advance_bitstream(v3c_unit_payload_size * 8);
        }
        else {
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::FATAL, "Decompression", "Unkown V3C unit type " + std::to_string(vuh_unit_type) + " \n");
        }
    }
    
    dec_context1_->queue->waitForJob(occ_job);
    dec_context1_->queue->waitForJob(geo_job);
    dec_context1_->queue->waitForJob(atr_job);
        
}

void Decompression::read_v3c_parameter_set(v3c_parameter_set* vps)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Reading V3C parameter set \n");
    // profile_tier_level
    read_profile_tier_level(&vps->ptl);
    
    vps->vps_v3c_parameter_set_id = read(4, "vps_v3c_parameter_set_id");
    uint8_t vps_reserved_zero_8bits = read(8, "vps_reserved_zero_8bits");
    vps->vps_atlas_count_minus1 = read(6, "vps_atlas_count_minus1");

    /* Resize vectors */
    vps->vps_atlas_id.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_frame_width.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_frame_height.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_map_count_minus1.resize(vps->vps_atlas_count_minus1 + 1);

    vps->vps_multiple_map_streams_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_map_absolute_coding_enabled_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_auxiliary_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_occupancy_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_geometry_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_attribute_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);

    vps->occupancy_info.resize(vps->vps_atlas_count_minus1 + 1);
    vps->geometry_info.resize(vps->vps_atlas_count_minus1 + 1);
    vps->attribute_info.resize(vps->vps_atlas_count_minus1 + 1);

    for (uint8_t j = 0; j < (vps->vps_atlas_count_minus1 + 1); j++) {
        vps->vps_atlas_id.at(j) = read(6, "vps_atlas_id");
        vps->vps_frame_width.at(j) = read_ue("vps_frame_width");
        vps->vps_frame_height.at(j) = read_ue("vps_frame_height");
        vps->vps_map_count_minus1.at(j) = read(4, "vps_map_count_minus1");

        if (vps->vps_map_count_minus1.at(j) > 0) {
            vps->vps_multiple_map_streams_present_flag.at(j) = read(1, "vps_multiple_map_streams_present_flag");
        }
        vps->vps_map_absolute_coding_enabled_flag.at(j).resize(vps->vps_map_count_minus1.at(j) + 1);
        for (uint8_t i = 1; i <= vps->vps_map_count_minus1.at(j); i++) {
            if(vps->vps_multiple_map_streams_present_flag.at(j)) {
                vps->vps_map_absolute_coding_enabled_flag.at(j).at(i) = read(1, "vps_map_absolute_coding_enabled_flag");
            }
            else {
                vps->vps_map_absolute_coding_enabled_flag.at(j).at(i) = true;
            }
        }
        vps->vps_auxiliary_video_present_flag.at(j) = read(1, "vps_auxiliary_video_present_flag");
        vps->vps_occupancy_video_present_flag.at(j) = read(1, "vps_occupancy_video_present_flag");
        vps->vps_geometry_video_present_flag.at(j) = read(1, "vps_geometry_video_present_flag");
        vps->vps_attribute_video_present_flag.at(j) = read(1, "vps_attribute_video_present_flag");

        if (vps->vps_occupancy_video_present_flag.at(j)) {
            vps->occupancy_info.at(j).oi_occupancy_codec_id = read(8, "oi_occupancy_codec_id");
            vps->occupancy_info.at(j).oi_lossy_occupancy_compression_threshold = read(8, "oi_lossy_occupancy_compression_threshold");
            vps->occupancy_info.at(j).oi_occupancy_2d_bit_depth_minus1 = read(5, "oi_occupancy_2d_bit_depth_minus1");
            vps->occupancy_info.at(j).oi_occupancy_MSB_align_flag = read(1, "oi_occupancy_MSB_align_flag");
        }
        
        if (vps->vps_geometry_video_present_flag.at(j)) {
            vps->geometry_info.at(j).gi_geometry_codec_id = read(8, "gi_geometry_codec_id");
            vps->geometry_info.at(j).gi_geometry_2d_bit_depth_minus1 = read(5, "gi_geometry_2d_bit_depth_minus1");
            vps->geometry_info.at(j).gi_geometry_MSB_align_flag = read(1, "gi_geometry_MSB_align_flag");
            vps->geometry_info.at(j).gi_geometry_3d_coordinates_bit_depth_minus1 = read(5, "gi_geometry_3d_coordinates_bit_depth_minus1");
            
            if (vps->vps_auxiliary_video_present_flag.at(j)) {
                vps->geometry_info.at(j).gi_auxiliary_geometry_codec_id = read(8, "gi_auxiliary_geometry_codec_id");
            }
        }

        if (vps->vps_attribute_video_present_flag.at(j)) {
            vps->ai_attribute_count = read(7, "ai_attribute_count");

            vps->attribute_info.at(j).ai_attribute_type_id.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_codec_id.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_auxiliary_attribute_codec_id.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_map_absolute_coding_persistence_flag.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_dimension_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_2d_bit_depth_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_MSB_align_flag.resize(vps->ai_attribute_count);


            for (uint8_t i = 0; i < vps->ai_attribute_count; ++i) {
                vps->attribute_info.at(j).ai_attribute_type_id.at(i) = read(4, "ai_attribute_type_id");
                vps->attribute_info.at(j).ai_attribute_codec_id.at(i) = read(8, "ai_attribute_codec_id");

                if(vps->vps_auxiliary_video_present_flag.at(j)) {
                    vps->attribute_info.at(j).ai_auxiliary_attribute_codec_id.at(i) = read(8, "ai_auxiliary_attribute_codec_id");
                }
                if(vps->vps_map_count_minus1.at(j) > 0) {
                    vps->attribute_info.at(j).ai_attribute_map_absolute_coding_persistence_flag.at(i) = read(1, "ai_attribute_map_absolute_coding_persistence_flag");
                }
                
                vps->attribute_info.at(j).ai_attribute_dimension_minus1.at(i) = read(6, "ai_attribute_dimension_minus1");
                uint8_t d = vps->attribute_info.at(j).ai_attribute_dimension_minus1.at(i);

                uint8_t m;
                if(d == 0) {
                    m = 0;
                    vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i) = 0;
                }
                else {
                    vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i) = read(6, "ai_attribute_dimension_partitions_minus1");
                    m = vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i);
                }
                vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).resize(vps->attribute_info.at(j).ai_attribute_dimension_minus1.at(i));
                uint16_t n = 0;
                for (uint8_t k = 0; k < m; k++) {
                    if(k + d == m) {
                        n = 0;
                        vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = 0;
                    }
                    else {
                        vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = read_ue("ai_attribute_partition_channels_minus1");
                        n = vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k);
                    }
                    d -= n + 1;
                }
                vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(m) = d;
                vps->attribute_info.at(j).ai_attribute_2d_bit_depth_minus1.at(i) = read(5, "ai_attribute_2d_bit_depth_minus1");
                vps->attribute_info.at(j).ai_attribute_MSB_align_flag.at(i) = read(1, "ai_attribute_MSB_align_flag");
            }
        }
        vps->vps_extension_present_flag = read(1, "vps_extension_present_flag");

        if(vps->vps_extension_present_flag) {
            vps->vps_packing_information_present_flag = read(1, "vps_packing_information_present_flag");
            vps->vps_miv_extension_present_flag = read(1, "vps_miv_extension_present_flag");
            vps->vps_extension_6bits = read(6, "vps_extension_6bits");
        }
        align_bitstream();

        // No packing information
        // No MIV extension
        // No VPS extension
        // TODO: Do we need to align at all when reading?
        //uvg_bitstream_align(stream);
    }
}

void Decompression::read_profile_tier_level(profile_tier_level* ptl)
{
    ptl->ptl_profile_toolset_idc = read(8, "ptl_profile_toolset_idc");
    ptl->ptl_tier_flag = read(1, "ptl_tier_flag");
    ptl->ptl_profile_codec_group_idc = read(7, "ptl_profile_codec_group_idc");
    ptl->ptl_profile_reconstruction_idc = read(8, "ptl_profile_reconstruction_idc");
    uint16_t ptl_reserved_zero_16bits = read(16, "ptl_reserved_zero_16bits");
    ptl->ptl_max_decodes_idc = read(4, "ptl_max_decodes_idc");
    uint16_t ptl_reserved_0xfff_12bits = read(12, "ptl_reserved_0xfff_12bits");
    ptl->ptl_level_idc = read(8, "ptl_level_idc");
    ptl->ptl_num_sub_profiles = read(6, "ptl_num_sub_profiles");
    ptl->ptl_extended_sub_profile_flag = read(1, "ptl_extended_sub_profile_flag");
    ptl->ptl_toolset_constraints_present_flag = read(1, "ptl_toolset_constraints_present_flag");

    if(ptl->ptl_toolset_constraints_present_flag) {
        throw std::runtime_error("Handling PTL toolset constraints not implemented");
        return;
    }
    // TODO: Parse PTC
}

void Decompression::read_asps(atlas_sequence_parameter_set &asps)
{
    asps.asps_atlas_sequence_parameter_set_id = read_ue("asps_atlas_sequence_parameter_set_id");
    asps.asps_frame_width = read_ue("asps_frame_width");
    asps.asps_frame_height = read_ue("asps_frame_height");
    asps.asps_geometry_3d_bit_depth_minus1 = read(5, "asps_geometry_3d_bit_depth_minus1");
    asps.asps_geometry_2d_bit_depth_minus1 = read(5, "asps_geometry_2d_bit_depth_minus1");
    asps.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 = read_ue("asps_log2_max_atlas_frame_order_cnt_lsb_minus4");
    asps.asps_max_dec_atlas_frame_buffering_minus1 = read_ue("asps_max_dec_atlas_frame_buffering_minus1");

    asps.asps_long_term_ref_atlas_frames_flag = read(1, "asps_long_term_ref_atlas_frames_flag");
    asps.asps_num_ref_atlas_frame_lists_in_asps = read_ue("asps_num_ref_atlas_frame_lists_in_asps");

    asps.ref_lists.resize(asps.asps_num_ref_atlas_frame_lists_in_asps);
    std::size_t Log2MaxAtlasFrmOrderCntLsb = asps.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    for( uint32_t rlsIdx = 0; rlsIdx < asps.asps_num_ref_atlas_frame_lists_in_asps; rlsIdx++ ) {
        ref_list_struct &ref = asps.ref_lists.at(rlsIdx);
        ref.num_ref_entries = read_ue("num_ref_entries");

        ref.st_ref_atlas_frame_flag.resize(ref.num_ref_entries);
        ref.abs_delta_afoc_st.resize(ref.num_ref_entries);
        ref.straf_entry_sign_flag.resize(ref.num_ref_entries);
        ref.afoc_lsb_lt.resize(ref.num_ref_entries);

        for (uint32_t i = 0; i < ref.num_ref_entries; ++i) {
            if(asps.asps_long_term_ref_atlas_frames_flag) {
                ref.st_ref_atlas_frame_flag.at(i) = read(1, "st_ref_atlas_frame_flag");
            }
            if(ref.st_ref_atlas_frame_flag.at(i)) {
                ref.abs_delta_afoc_st.at(i) = read_ue("abs_delta_afoc_st");
                if(ref.abs_delta_afoc_st.at(i) > 0) {
                    ref.straf_entry_sign_flag.at(i) = read(1, "straf_entry_sign_flag");
                }
            }
            else {
                ref.afoc_lsb_lt.at(i) = read(Log2MaxAtlasFrmOrderCntLsb, "afoc_lsb_lt");
            }
        }
    }
    asps.asps_use_eight_orientations_flag = read(1, "asps_use_eight_orientations_flag");
    asps.asps_extended_projection_enabled_flag = read(1, "asps_extended_projection_enabled_flag");

    if( asps.asps_extended_projection_enabled_flag ) {
        asps.asps_max_number_projections_minus1 = read_ue("asps_max_number_projections_minus1");
    }

    asps.asps_normal_axis_limits_quantization_enabled_flag = read(1, "asps_normal_axis_limits_quantization_enabled_flag");
    asps.asps_normal_axis_max_delta_value_enabled_flag = read(1, "asps_normal_axis_max_delta_value_enabled_flag");
    asps.asps_patch_precedence_order_flag = read(1, "asps_patch_precedence_order_flag");
    asps.asps_log2_patch_packing_block_size = read(3, "asps_log2_patch_packing_block_size");
    asps.asps_patch_size_quantizer_present_flag = read(1, "asps_patch_size_quantizer_present_flag");
    asps.asps_map_count_minus1 = read(4, "asps_map_count_minus1");
    asps.asps_pixel_deinterleaving_enabled_flag = read(1, "asps_pixel_deinterleaving_enabled_flag");

    if(asps.asps_pixel_deinterleaving_enabled_flag) {
        asps.asps_map_pixel_deinterleaving_flag.resize(asps.asps_map_count_minus1 + 1);
        for (size_t j = 0; j < asps.asps_map_count_minus1; ++j) {
            asps.asps_map_pixel_deinterleaving_flag.at(j) = read(1, "asps_map_pixel_deinterleaving_flag");
        }
    }
    asps.asps_raw_patch_enabled_flag = read(1, "asps_raw_patch_enabled_flag");
    asps.asps_eom_patch_enabled_flag = read(1, "asps_eom_patch_enabled_flag");

    if(asps.asps_eom_patch_enabled_flag && asps.asps_map_count_minus1 == 0) {
        asps.asps_eom_fix_bit_count_minus1 = read(4, "asps_eom_fix_bit_count_minus1");
    }
    if (asps.asps_raw_patch_enabled_flag || asps.asps_eom_patch_enabled_flag) {
        asps.asps_auxiliary_video_enabled_flag = read(1, "asps_auxiliary_video_enabled_flag");
    }
    asps.asps_plr_enabled_flag = read(1, "asps_plr_enabled_flag");
    if( asps.asps_plr_enabled_flag ) {
        throw std::runtime_error("Handling ASPS PLR not implemented");
    }
    asps.asps_vui_parameters_present_flag = read(1, "asps_vui_parameters_present_flag");
    if( asps.asps_vui_parameters_present_flag ) {
        throw std::runtime_error("Handling ASPS VUI parameters not implemented");
    }
    asps.asps_extension_present_flag = read(1, "asps_extension_present_flag");

    if(asps.asps_extension_present_flag) {
        asps.asps_vpcc_extension_present_flag = read(1, "asps_vpcc_extension_present_flag");
        asps.asps_miv_extension_present_flag = read(1, "asps_miv_extension_present_flag");
        asps.asps_extension_6bits = read(6, "asps_extension_6bits");
    }
    if (asps.asps_vpcc_extension_present_flag) {
        asps.asps_vpcc_remove_duplicate_point_enabled_flag = read(1, "asps_vpcc_remove_duplicate_point_enabled_flag");
        if(asps.asps_pixel_deinterleaving_enabled_flag || asps.asps_plr_enabled_flag) {
            asps.asps_vpcc_surface_thickness_minus1 = read_ue("asps_vpcc_surface_thickness_minus1");
        }
    }
    align_bitstream();
}

void Decompression::read_afps(atlas_frame_parameter_set &afps)
{
    afps.afps_atlas_frame_parameter_set_id = read_ue("afps_atlas_frame_parameter_set_id");
    afps.afps_atlas_sequence_parameter_set_id = read_ue("afps_atlas_sequence_parameter_set_id");
    afps.afti.afti_single_tile_in_atlas_frame_flag = read(1, "afti_single_tile_in_atlas_frame_flag");
    afps.afti.afti_signalled_tile_id_flag = read(1, "afti_signalled_tile_id_flag");
    afps.afps_output_flag_present_flag = read(1, "afps_output_flag_present_flag");
    afps.afps_num_ref_idx_default_active_minus1 = read_ue("afps_num_ref_idx_default_active_minus1");
    afps.afps_additional_lt_afoc_lsb_len = read_ue("afps_additional_lt_afoc_lsb_len");
    afps.afps_lod_mode_enabled_flag = read(1, "afps_lod_mode_enabled_flag");
    afps.afps_raw_3d_offset_bit_count_explicit_mode_flag = read(1, "afps_raw_3d_offset_bit_count_explicit_mode_flag");
    afps.afps_extension_present_flag = read(1, "afps_extension_present_flag");
    afps.afps_miv_extension_present_flag = read(1, "afps_miv_extension_present_flag");
    afps.afps_extension_7bits = read(7, "afps_extension_7bits");

    align_bitstream();
}

void Decompression::read_atlas_rbsp(atlas_tile_layer_rbsp* rbsp, NAL_UNIT_TYPE nalu_t)
{
    read_atlas_tile_header(rbsp->ath, nalu_t);
    read_atlas_tile_data_unit(rbsp->atdu, rbsp->ath);
    align_bitstream();
}

void Decompression::read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t)
{
    if(nalu_t >= NAL_GBLA_W_LP && nalu_t <= NAL_RSV_IRAP_ACL_29) {
        ath.ath_no_output_of_prior_atlas_frames_flag = read(1, "ath_no_output_of_prior_atlas_frames_flag");
    }
    ath.ath_atlas_frame_parameter_set_id = read_ue("ath_atlas_frame_parameter_set_id");
    ath.ath_atlas_adaptation_parameter_set_id = read_ue("ath_atlas_adaptation_parameter_set_id");
    //ath.ath_id = read("ath_id"); TODO: Figure out the dynamic bit length for this field
    uint16_t tileID = ath.ath_id; // default 0
    ath.ath_type = static_cast<ATH_TYPE>(read_ue("ath_type"));

    if(saved_params_.back().afps.afps_output_flag_present_flag) {
        ath.ath_atlas_output_flag = read(1, "ath_atlas_output_flag");
    }
    size_t Log2MaxAtlasFrmOrderCntLsb = saved_params_.back().asps.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    ath.ath_atlas_frm_order_cnt_lsb = read(uint8_t(Log2MaxAtlasFrmOrderCntLsb), "ath_atlas_frm_order_cnt_lsb"); //u(v)

    if(saved_params_.back().asps.asps_num_ref_atlas_frame_lists_in_asps > 0) {
        ath.ath_ref_atlas_frame_list_asps_flag = read(1, "ath_ref_atlas_frame_list_asps_flag");
    }
    if(ath.ath_ref_atlas_frame_list_asps_flag == 0) {
        throw std::runtime_error("Using atlas ref frame lists not implemented");
        return;
    }
    else if (saved_params_.back().asps.asps_num_ref_atlas_frame_lists_in_asps > 1) {
        size_t bit_len = std::ceil(std::log2(saved_params_.back().asps.asps_num_ref_atlas_frame_lists_in_asps));
        ath.ath_ref_atlas_frame_list_idx = read(uint8_t(bit_len), "ath_ref_atlas_frame_list_idx");
    }

    size_t NumLtrAtlasFrmEntries = 0; // default value, ref list is from ASPS TODO: dynamic
    ath.ath_additional_afoc_lsb_present_flag.resize(NumLtrAtlasFrmEntries);
    ath.ath_additional_afoc_lsb_val.resize(NumLtrAtlasFrmEntries);
    for (size_t j = 0; j < NumLtrAtlasFrmEntries; j++) {
        ath.ath_additional_afoc_lsb_present_flag.at(j) = read(1, "ath_additional_afoc_lsb_present_flag");
        if(ath.ath_additional_afoc_lsb_present_flag.at(j)) {
            ath.ath_additional_afoc_lsb_val.at(j) = read(saved_params_.back().afps.afps_additional_lt_afoc_lsb_len, "ath_additional_afoc_lsb_val");
        }
    }
    if(ath.ath_type != SKIP_TILE) {
        if(saved_params_.back().asps.asps_normal_axis_limits_quantization_enabled_flag) {
            ath.ath_pos_min_d_quantizer = read(5, "ath_pos_min_d_quantizer");
            if(saved_params_.back().asps.asps_normal_axis_max_delta_value_enabled_flag) {
                ath.ath_pos_delta_max_d_quantizer = read(5, "ath_pos_delta_max_d_quantizer");
            }
        }
        if(saved_params_.back().asps.asps_patch_size_quantizer_present_flag) {
            ath.ath_patch_size_x_info_quantizer = read(3, "ath_patch_size_x_info_quantizer");
            ath.ath_patch_size_y_info_quantizer = read(3, "ath_patch_size_y_info_quantizer");
        }
        if(saved_params_.back().afps.afps_raw_3d_offset_bit_count_explicit_mode_flag) {
            size_t bit_len = std::floor(std::log2(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 + 1));
            ath.ath_raw_3d_offset_axis_bit_count_minus1 = read(uint8_t(bit_len), "ath_raw_3d_offset_axis_bit_count_minus1");
        }
        if( ath.ath_type == ATH_TYPE::P_TILE && NumLtrAtlasFrmEntries > 1 ) {
            ath.ath_num_ref_idx_active_override_flag = read(1, "ath_num_ref_idx_active_override_flag");
            if(ath.ath_num_ref_idx_active_override_flag) {
                ath.ath_num_ref_idx_active_minus1 = read_ue("ath_num_ref_idx_active_minus1");
            }
        }
    }
    align_bitstream();
}

void Decompression::read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid)
{
    if (ath.ath_type == SKIP_TILE) {
        // skip mode: currently not supported but added it for convenience. Could
        // easily be removed
    } else if (ath.ath_type == P_TILE) {
        if (pid.patchMode == P_SKIP) {
            // skip mode: currently not supported but added it for convenience. Could
            // easily be removed
            //skipPatchDataUnit(bitstream);
        } else if (pid.patchMode == P_MERGE) {
            auto &mpdu = pid.merge;
            //mergePatchDataUnit(mpdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_INTRA) {
            auto& pdu = pid.patch;
            //patchDataUnit(pdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_INTER) {
            auto& ipdu = pid.inter;
            //interPatchDataUnit(ipdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_RAW) {
            auto& rpdu = pid.raw;
            //rawPatchDataUnit(rpdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_EOM) {
            auto& epdu = pid.eom;
            //eomPatchDataUnit(epdu, ath, syntax, bitstream);
        }
    }
    else if (ath.ath_type == I_TILE) { // currently only use I_TILE types
        if (pid.patchMode == I_INTRA) {
            auto& pdu = pid.patch;
            read_patch_data_unit(ath, pdu);
            //patchDataUnit(pdu, ath, syntax, bitstream);
        } else if (pid.patchMode == I_RAW) {
            auto& rpdu = pid.raw;
            //rawPatchDataUnit(rpdu, ath, syntax, bitstream);
        } else if (pid.patchMode == I_EOM) {
            auto& epdu = pid.eom;
            //eomPatchDataUnit(epdu, ath, syntax, bitstream);
        }
    }
}

void Decompression::read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu)
{
    pdu.pdu_2d_pos_x = read_ue("pdu_2d_pos_x");
    pdu.pdu_2d_pos_y = read_ue("pdu_2d_pos_y");
    pdu.pdu_2d_size_x_minus1 = read_ue("pdu_2d_size_x_minus1");
    pdu.pdu_2d_size_y_minus1 = read_ue("pdu_2d_size_y_minus1");

    pdu.pdu_3d_offset_u = read(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 + 1, "pdu_3d_offset_u");
    pdu.pdu_3d_offset_v = read(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 + 1, "pdu_3d_offset_v");
    pdu.pdu_3d_offset_d = read(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 - ath.ath_pos_min_d_quantizer + 1, "pdu_3d_offset_d");

    if(saved_params_.back().asps.asps_normal_axis_max_delta_value_enabled_flag) {
        uint32_t rangeDBitDepth = std::min(saved_params_.back().asps.asps_geometry_2d_bit_depth_minus1, saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1) + 1;
        pdu.pdu_3d_range_d = read(rangeDBitDepth - ath.ath_pos_delta_max_d_quantizer, "pdu_3d_range_d");
    }
    pdu.pdu_projection_id = read(ceil(log2(6)), "pdu_projection_id");
    pdu.pdu_orientation_index = read(false ? 3 : 1, "pdu_orientation_index");

    if(saved_params_.back().afps.afps_lod_mode_enabled_flag) {
        pdu.pdu_lod_enabled_flag = read(1, "pdu_lod_enabled_flag");
        if(pdu.pdu_lod_enabled_flag) {

            pdu.pdu_lod_scale_x_minus1 = read_ue("pdu_lod_scale_x_minus1");
            pdu.pdu_lod_scale_y_idc = read_ue("pdu_lod_scale_y_idc");
        }
    }
    // if( asps_plr_enabled_flag )               == false
    // if( asps_miv_extension_present_flag )     == false
}


void Decompression::read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath)
{
    uint16_t tileID = ath.ath_id;
    if (ath.ath_type == SKIP_TILE) {
        //skipPatchDataUnit(bitstream);
        // This is just empty?
    }
    else {
        while (true ) {
            atdu.atdu_patch_mode = read_ue("atdu_patch_mode");
            if(atdu.atdu_patch_mode == ATDU_PATCH_MODE_I_TILE::I_END
                || atdu.atdu_patch_mode == ATDU_PATCH_MODE_P_TILE::P_END) {
                break;
            }
            patch_information_data pid;
            pid.patchMode = atdu.atdu_patch_mode;
            read_patch_information_data(ath, pid);
            atdu.pid_vec.push_back(pid);

        }
    }
}

void Decompression::read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size, decompressed_gof* output)
{                
    switch(nal_unit_type) {
            case NAL_UNIT_TYPE::NAL_ASPS:
                read_asps(saved_params_.back().asps);
                break;
            case NAL_UNIT_TYPE::NAL_AFPS:
                read_afps(saved_params_.back().afps);
                break;
            case NAL_UNIT_TYPE::NAL_IDR_N_LP: {
                atlas_tile_layer_rbsp rbsp;
                read_atlas_rbsp(&rbsp, nal_unit_type);
                auto frame = std::make_unique<atlas_frame>();
                frame.get()->atlas_index = 0;
                decode_atlas_frame(frame.get(), rbsp);
                output->atlas_map.push_back(std::move(frame));
                output->frame_count++;
                break; }
            default: 
                uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::ERROR, "Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " \n");
                advance_bitstream(nal_unit_size * 8 - 16); // skip the rest of NAL unit for now 
                break;
        }
}

void Decompression::read_atlas_sub_bitstream(std::size_t v3c_payload_size_bytes, decompressed_gof* output)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Reading V3C atlas data, size " + std::to_string(v3c_payload_size_bytes) + " \n");

    std::size_t end_ptr = pos_.bytes + v3c_payload_size_bytes;
    //advance_bitstream(v3c_payload_size_bytes * 8);
    // 3 bits for nAL unit size precision - 1 and 5 reserved
    uint8_t nal_size_precision_bytes = read(3, "nal_size_precision_in_bytes") + 1;
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "NAL size precision " + std::to_string(nal_size_precision_bytes) + " \n");
    std::size_t nal_unit_precision_bits = nal_size_precision_bytes * 8;

    advance_bitstream(5);

    while (true) {
        if (pos_.bytes >= end_ptr) {
            break;
        }
        std::size_t nal_unit_size = read(nal_unit_precision_bits, "nal unit size");

        // Inside nal unit now
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Current NAL unit location " + std::to_string(pos_.bytes) + ", size " + std::to_string(nal_unit_size) + " \n");
        
        read(1, "nal_forbidden_zero_bit");
        NAL_UNIT_TYPE nal_unit_type = static_cast<NAL_UNIT_TYPE>(read(6, "nal_unit_type"));
        uint8_t nal_layer_id = read(6, "nal_layer_id");
        uint8_t nal_temporal_id_plus1 = read(3, "nal_temporal_id_plus1");
        read_atlas_nal_unit(nal_unit_type, nal_unit_size, output);
    }
}

void Decompression::decode_atlas_frame(atlas_frame* frame, const atlas_tile_layer_rbsp &rbsp)
{
    // 1 tile per frame
    frame->frame_width = saved_params_.back().asps.asps_frame_width;
    frame->frame_height = saved_params_.back().asps.asps_frame_height;

    std::size_t pid_count = rbsp.atdu.pid_vec.size();

    const size_t minLevel = pow( 2., double(rbsp.ath.ath_pos_min_d_quantizer)); // this line from TMC2
    int32_t quantizerSizeX = 1 << rbsp.ath.ath_patch_size_x_info_quantizer; // ath.getPatchSizeXinfoQuantizer(); // tmc2
    int32_t quantizerSizeY = 1 << rbsp.ath.ath_patch_size_y_info_quantizer; //ath.getPatchSizeYinfoQuantizer(); // tmc2
    int32_t packingBlockSize       = 1 << saved_params_.back().asps.asps_log2_patch_packing_block_size;
    double  packingBlockSizeD      = static_cast<double>( packingBlockSize );

    for(std::size_t i = 0; i < pid_count; ++i) {
        const patch_data_unit &pdu = rbsp.atdu.pid_vec.at(i).patch;
        patch p;
        p.occupancy_resolution = size_t(1) << saved_params_.back().asps.asps_log2_patch_packing_block_size;
        p.TilePatch2dPosX = pdu.pdu_2d_pos_x;
        p.TilePatch2dPosY = pdu.pdu_2d_pos_y;
        p.TilePatch3dOffsetU = pdu.pdu_3d_offset_u;
        p.TilePatch3dOffsetV = pdu.pdu_3d_offset_v;

        bool lodEnableFlag = pdu.pdu_lod_enabled_flag;
        if ( lodEnableFlag ) {
            p.TilePatchLoDScaleX = pdu.pdu_lod_scale_x_minus1 + 1;
            p.TilePatchLoDScaleY = pdu.pdu_lod_scale_y_idc + p.TilePatchLoDScaleX > 1 ? 1 : 2;
        } else {
            p.TilePatchLoDScaleX = 1;
            p.TilePatchLoDScaleY = 1;
        }
        p.TilePatch3dRangeD = pdu.pdu_3d_range_d == 0 ? 0 : (pdu.pdu_3d_range_d * minLevel - 1);
        if ( saved_params_.back().asps.asps_patch_size_quantizer_present_flag ) {
            p.size2DXInPixel_ = (pdu.pdu_2d_size_x_minus1 + 1) * quantizerSizeX;
            p.size2DYInPixel_ = (pdu.pdu_2d_size_y_minus1 + 1) * quantizerSizeY;
            p.TilePatch2dSizeX = ceil( static_cast<double>( p.size2DXInPixel_ ) / packingBlockSizeD );
            p.TilePatch2dSizeY = ceil( static_cast<double>( p.size2DYInPixel_ ) / packingBlockSizeD );
        } else {
            p.TilePatch2dSizeX = pdu.pdu_2d_size_x_minus1 + 1;
            p.TilePatch2dSizeY = pdu.pdu_2d_size_y_minus1 + 1;
        }
        p.TilePatchOrientationIndex = pdu.pdu_orientation_index;
        p.TilePatchProjectionID = pdu.pdu_projection_id;
        p.TilePatch3dOffsetD = static_cast<int32_t>( pdu.pdu_3d_offset_d ) * minLevel;
        p.setViewId( pdu.pdu_projection_id ); // this also sets a new value to TilePatchProjectionID
        if ( p.normalAxis_ == 0 ) {
            p.tangentAxis_ = 2 ;
            p.bitangentAxis_ = 1 ;
        } else if ( p.normalAxis_ == 1 ) {
            p.tangentAxis_ = 2;
            p.bitangentAxis_ = 0;
        } else {
            p.tangentAxis_ = 0;
            p.bitangentAxis_ = 1;
        }
        /*printf(
          "patch(Intra) %zu: UV0 %4zu %4zu UV1 %4zu %4zu D1=%4zu S=%4zu %4zu %4zu(%4zu) P=%zu O=%zu A=%u%u%u Lod "
          "=(%zu) %zu,%zu 45=%d ProjId=%4zu Axis=%zu \n",
          size_t(i), size_t(p.TilePatch2dPosX), size_t(p.TilePatch2dPosY), size_t(p.TilePatch3dOffsetU), size_t(p.TilePatch3dOffsetV), size_t(p.TilePatch3dOffsetD), size_t(p.TilePatch2dSizeX),
          size_t(p.TilePatch2dSizeY), size_t(p.TilePatch3dRangeD), size_t(pdu.pdu_3d_range_d), size_t(p.TilePatchProjectionID),
          size_t(p.TilePatchOrientationIndex), (uint32_t)p.normalAxis_, (uint32_t)p.tangentAxis_, (uint32_t)p.bitangentAxis_,
          (size_t)lodEnableFlag, (size_t)p.TilePatchLoDScaleX, (size_t)p.TilePatchLoDScaleY, saved_asps_.asps_extended_projection_enabled_flag,
          (size_t)pdu.pdu_projection_id, size_t(p.axisOfAdditionalPlane_) );*/

        frame->patches_map.push_back(p);
    }
}

/*void Decompression::convert_video_sub_bitstream(const std::size_t v3c_payload_size_bytes, const std::string output_path, std::vector<uvgvpcc_dec::video_parameter_set_nalu>* v_params)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Reading V3C video data " + std::to_string(v3c_payload_size_bytes) + " \n");
    std::ofstream file(output_path, std::ios::binary);
    if(!file.is_open()) {
        throw std::runtime_error("Bitstream writing : Could not open output file " + output_path);
    }
    std::size_t end_point = pos_.bytes + v3c_payload_size_bytes;
    const char hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};
    while (true) {
        if (pos_.bytes >= end_point) {
            break;
        }
        std::size_t nalu_size = read(32, "hevc nal unit size");
        std::size_t hevc_nal_type = cbuf_[pos_.bytes] >> 1;
        if (hevc_nal_type == 32 || hevc_nal_type == 33 || hevc_nal_type == 34) {
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Parameter set (type " + std::to_string(hevc_nal_type) + ") found, size " + std::to_string(nalu_size) + " \n");
            std::unique_ptr<uint8_t[]> data(new uint8_t[nalu_size]);
            memcpy(data.get(), &cbuf_[pos_.bytes], nalu_size);
            v_params->push_back({hevc_nal_type, nalu_size, std::move(data)});
        }
        
        file.write(hevc_start_code, 4);
        file.write(reinterpret_cast<const char*>(&cbuf_[pos_.bytes]), nalu_size);
        advance_bitstream(nalu_size * 8);
    }
    file.close();
}*/

void Decompression::decompress_video_sub_bitstream(const std::size_t ptr, const std::size_t v3c_payload_size_bytes, video_map &map, AVCodecContext* codec_ctx)
{
    std::vector<uint8_t> temp = {};
    std::vector<size_t> frame_boundaries = {};
    convert_video_sub_bitstream(v3c_payload_size_bytes, ptr, temp, frame_boundaries);
    decode_video_sub_bitstream(temp, frame_boundaries, map, codec_ctx);
}

void Decompression::convert_video_sub_bitstream(const std::size_t ptr, const std::size_t v3c_payload_size_bytes, std::vector<uint8_t> &output, std::vector<size_t> &frame_boundaries)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Converting V3C video data " + std::to_string(v3c_payload_size_bytes) + " \n");
    output.resize(v3c_payload_size_bytes);
    size_t write_ptr = 0;
    frame_boundaries.push_back(write_ptr);

    // Copy the current location so we dont mess up the position on the whole V-PCC bitstream
    std::size_t read_ptr = ptr;
    const std::size_t end_point = read_ptr + v3c_payload_size_bytes;
    const char hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};
    while (true) {
        if (read_ptr >= end_point) {
            break;
        }
        std::size_t nalu_size = read_value(&cbuf_[read_ptr], 4); //read(32, "hevc nal unit size");
        read_ptr += 4;
        std::size_t hevc_nal_type = cbuf_[read_ptr] >> 1;
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "HEVC NAL unit (type " + std::to_string(hevc_nal_type) + ") found, size " + std::to_string(nalu_size) + " \n");
        
        memcpy(&output[write_ptr], hevc_start_code, 4);
        write_ptr += 4;

        memcpy(&output[write_ptr], &cbuf_[read_ptr], nalu_size);
        write_ptr += nalu_size;
        read_ptr += nalu_size;
        if(hevc_nal_type == 19 || hevc_nal_type == 1) {
            //std::cout << "new cutoff at " << write_ptr << std::endl;
            frame_boundaries.push_back(write_ptr); 
        }
        
        // Dont do this here to not mess up the bitstream
        //advance_bitstream(nalu_size * 8);
        //std::cout << "write_ptr " << write_ptr << std::endl;
    }
    //std::cout << "end of s " << std::endl;
}

std::vector<AVFrame*> Decompression::decode_video_frames(std::vector<uint8_t> &input, std::vector<size_t> &frame_boundaries, AVCodecContext* codec_ctx)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Start decoding HEVC data \n");
    AVCodecContext* context = codec_ctx;
    if (!context) {
        throw std::runtime_error("Codec context is not initialized");
    }
    std::vector<AVFrame*> output = {};
    size_t data_size = input.size();
    /* From avcodec_send_packet() documentation: The input buffer, avpkt->data must be
    AV_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes. */
    size_t padded_size = data_size + AV_INPUT_BUFFER_PADDING_SIZE;
    input.resize(padded_size, 0);

    // Process full frames through individual packets
    for (size_t frame_index = 0; frame_index < frame_boundaries.size() - 1; frame_index++) {
        AVPacket* packet = av_packet_alloc();
        if (!packet) {
            throw std::runtime_error("Could not allocate AVPacket");
        }
        packet->data = &input.data()[frame_boundaries.at(frame_index)];
        size_t packet_size = frame_boundaries.at(frame_index + 1) - frame_boundaries.at(frame_index);
        packet->size = packet_size;

        //std::cout << "decoding data at " << frame_boundaries.at(frame_index) << " with size of " << packet_size << std::endl;
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "send_packet \n");
        int ret = avcodec_send_packet(context, packet);
        //std::cout << "send ret " << int(ret) << std::endl;
        if (ret < 0) {
            av_packet_free(&packet);
            throw std::runtime_error("Error sending packet to decoder");
        }
        // Receive frame
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            throw std::runtime_error("Could not allocate video frame");
        }
        ret = avcodec_receive_frame(context, frame);
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "receive frame \n");
        //std::cout << "recv ret " << int(ret) << std::endl;

        if (ret == AVERROR(EAGAIN)) {
            av_frame_free(&frame);
        }
        else if (ret == AVERROR_EOF) {
            av_frame_free(&frame);
        }
        else if (ret < 0) {
            throw std::runtime_error("Error receiving frame from decoder");
        }
        else {
            output.push_back(frame);
        }
        av_packet_free(&packet);
    }

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Decoded " + std::to_string(output.size()) + " HEVC frames \n");
    return output;
}

void Decompression::decode_video_sub_bitstream(std::vector<uint8_t> &input, std::vector<size_t> &frame_boundaries, video_map &map, AVCodecContext* codec_ctx)
{
    std::vector<AVFrame*> frames = {};
    frames = decode_video_frames(input, frame_boundaries, codec_ctx);
    if(frames.empty()) {
        throw std::runtime_error("No frames decoded");
    }
    map.width = frames.front()->width; // not perfect solution
    map.height = frames.front()->height; // not perfect solution

    for (size_t i = 0; i < frames.size(); ++i) {
        AVFrame* fr = frames.at(i);
        if (fr->format != AV_PIX_FMT_YUV420P) {
            std::cout << " pixel format " << (int)fr->format << std::endl;
            throw std::runtime_error("Unsupported pixel format, expected YUV420P");
        }
        size_t width = fr->width;
        size_t height = fr->height;
        picture frame420;
        frame420.width = width;
        frame420.height = height;
        frame420.format = PCCCOLORFORMAT::YUV420;
        frame420.Y.resize(width * height);
        frame420.U.resize(width * height / 4);
        frame420.V.resize(width * height / 4);

        // Copy Y plane
        std::memcpy(frame420.Y.data(), fr->data[0], width * height);

        // Copy U plane
        std::memcpy(frame420.U.data(), fr->data[1], width * height / 4);

        // Copy V plane
        std::memcpy(frame420.V.data(), fr->data[2], width * height / 4);

        picture frame444;
        if(frame420.format == PCCCOLORFORMAT::YUV420) {
            frame444.convert_yuv_420_to_444(&frame420);
        }
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Added video frame \n");
        map.pictures.push_back(std::move(frame444));
        map.frame_count++;
        av_frame_free(&fr);
    }
}

/*void Decompression::decode_video_sub_bitstream(const std::string input_path, const std::string output_path, video_map* map)
{
    std::stringstream cmd;
    cmd << ffmpeg_path;
    if (uvgvpcc_dec::Logger::getLogLevel() < uvgvpcc_dec::LogLevel::PROFILING) {
        cmd << " -hide_banner -loglevel error ";
    }
    cmd << " -f hevc -i " << input_path << " " << output_path;
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", cmd.str() + " \n");
    if (std::system(cmd.str().c_str()) != 0) {
        throw std::runtime_error("During the decoding of the sequence, an error occured while executing the following command: " +
            cmd.str());
        return;
    }

    // Read decompressed video into a map
    std::ifstream decompressed_video(output_path, std::ios::binary);
    if(!decompressed_video.is_open()) {
        throw std::runtime_error("Bitstream reading : Could not open output file " + output_path);
    }

    size_t width = video_width_;
    size_t height = video_height_;
    if(map->type == V3C_OVD) {
        width = occupancy_width_;
        height = occupancy_height_;
    }
    map->width = width;
    map->height = height;
    size_t frameSize = (width * height * 3) / 2;

    while (decompressed_video.peek() != EOF) {
        picture frame420;
        frame420.width = width;
        frame420.height = height;
        frame420.format = PCCCOLORFORMAT::YUV420;
        frame420.Y.resize(width * height);
        frame420.U.resize(width * height / 4);
        frame420.V.resize(width * height / 4);

        std::size_t data_read = 0;
        // Read Y plane
        decompressed_video.read(reinterpret_cast<char*>(frame420.Y.data()), width * height);
        data_read += decompressed_video.gcount();

        // Read U plane
        decompressed_video.read(reinterpret_cast<char*>(frame420.U.data()), frame420.U.size());
        data_read += decompressed_video.gcount();

        // Read V plane
        decompressed_video.read(reinterpret_cast<char*>(frame420.V.data()), frame420.V.size());
        data_read += decompressed_video.gcount();

        picture frame444;
        if(frame420.format == PCCCOLORFORMAT::YUV420) {
            frame444.convert_yuv_420_to_444(&frame420);
        }

        if (data_read == frameSize) {
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Decompression", "Added video frame \n");
            map->pictures.push_back(std::move(frame444));
            map->frame_count++;
        }
        else {
            throw std::runtime_error("Bitstream reading : framesize " + std::to_string(frameSize) + ", data read " + std::to_string(data_read));
            break;
        }
    }
    if (keep_intermediate_files_) {return;}
    std::remove(input_path.c_str());
    std::remove(output_path.c_str());
}*/