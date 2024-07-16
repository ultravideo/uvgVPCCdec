#include "decompression.hpp"
#include <cstring>
#include <sstream>
#include <fstream>
#include <cstdio>

/* In debug mode print out some extra info */
#define BITSTREAM_DEBUG false

static const uint8_t* cbuf_;
static bitstream_position pos_;

v3c_parameter_set saved_vps_;
atlas_sequence_parameter_set saved_asps_;
atlas_frame_parameter_set saved_afps_;

std::string ffmpeg_path = "ffmpeg";

std::string o_hevc = "OCCUPANCY.hevc";
std::string g_hevc = "GEOMETRY.hevc";
std::string a_hevc = "ATTRIBUTE.hevc";

std::string o_yuv = "OCCUPANCY-MAP-640x640-8bit.yuv";
std::string g_yuv = "GEOMETRY-MAP-1280x1280-8bit.yuv";
std::string a_yuv = "ATTRIBUTE-MAP-1280x1280-8bit.yuv";

/* TODO: make sure this function works in all cases */
void BitstreamParsing::advance_bitstream(std::size_t bits)
{
    std::size_t bytes = bits / 8 + (pos_.bits + bits % 8) / 8;
    pos_.bytes += bytes;
    pos_.bits = (pos_.bits + bits % 8) % 8;
}

/* TODO: make sure this function works in all cases */
void BitstreamParsing::align_bitstream()
{
    if(pos_.bits != 0) {
        pos_.bytes++;
        pos_.bits = 0;
    }
}

/* NOTE -------------------- Straight from TMC2 -------------------- */
uint32_t BitstreamParsing::read_bits(uint8_t bits) {
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
uint32_t BitstreamParsing::read_bits_ue()
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

uint32_t BitstreamParsing::read(uint8_t bits, const std::string &name) {
    uint32_t value = read_bits(bits);
#if BITSTREAM_DEBUG
    printf("%-50s u(%u) : %d\n", name.c_str(),bits,value);
#endif
    (void)name; // Suppress unused parameter warning
    return value;
}

uint32_t BitstreamParsing::read_ue(const std::string &name)
{
    uint32_t value = read_bits_ue();
#if BITSTREAM_DEBUG
    printf("%-50s u(v) : %d\n", name.c_str(),value);
#endif
    (void)name; // Suppress unused parameter warning
    return value;
}

void BitstreamParsing::initializeStaticParameters(const uvgvpcc_dec::Parameters& param)
{
    int x = param.hello;
}

void BitstreamParsing::decompressV3CSampleStream(const std::vector<uint8_t> &data, decompressed_data* output)
{
    cbuf_ = data.data();
    //std::size_t ptr = 0
    pos_.bits = 0;
    pos_.bytes = 0;

    // 3 bits for v3c unit size precision - 1 and 5 reserved
    uint8_t v3c_size_precision_bytes = read(3, "v3c_size_precision_in_bytes") + 1;
    std::cout << "V3C size precision in bytes: " << uint32_t(v3c_size_precision_bytes) << std::endl;
    std::size_t v3c_unit_precision_bits = v3c_size_precision_bytes * 8;

    advance_bitstream(5);

    while (true) {
        if (pos_.bytes >= data.size()) {
            break;
        }
        std::size_t v3c_unit_size = read(v3c_unit_precision_bits, "v3c unit size");

        // Inside v3c unit now
        std::cout << "Current V3C unit location " << pos_.bytes << ", size " << v3c_unit_size << std::endl;
        

        // Next 4 bytes are the V3C unit header
        uint8_t vuh_unit_type = read(5, "vuh_unit_type");
        std::cout << "V3C unit type " << uint32_t(vuh_unit_type) << std::endl;
        advance_bitstream(4 * 8 - 5); // skip the rest of v3c header for now

        std::size_t v3c_unit_payload_size_bytes = v3c_unit_size - 4;
        switch(vuh_unit_type) {
            case V3C_UNIT_TYPE::V3C_VPS:
                read_v3c_parameter_set(&output->vps);
                break;
            case V3C_UNIT_TYPE::V3C_AD:
                read_atlas_sub_bitstream(v3c_unit_payload_size_bytes, output);
                break;
            case V3C_UNIT_TYPE::V3C_OVD:
                convert_video_sub_bitstream(v3c_unit_payload_size_bytes, o_hevc);
                decode_video_sub_bitstream(o_hevc, o_yuv);
                break;
            case V3C_UNIT_TYPE::V3C_GVD:
                convert_video_sub_bitstream(v3c_unit_payload_size_bytes, g_hevc);
                decode_video_sub_bitstream(g_hevc, g_yuv);
                break;
            case V3C_UNIT_TYPE::V3C_AVD:
                convert_video_sub_bitstream(v3c_unit_payload_size_bytes, a_hevc);
                decode_video_sub_bitstream(a_hevc, a_yuv);
                break;
            default: 
                std::cout << "error" << std::endl;
                break;
        }
    }
    std::cout << "File parsed, pos bytes at " << pos_.bytes << std::endl;
}

void BitstreamParsing::read_v3c_parameter_set(v3c_parameter_set* vps)
{
    std::cout << "Reading V3C parameter set" << std::endl;
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

void BitstreamParsing::read_profile_tier_level(profile_tier_level* ptl)
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
        std::cout << "ERROR CANT HANDLE PTC" << std::endl;
        return;
    }
    // TODO: Parse PTC
}

void BitstreamParsing::read_asps(atlas_sequence_parameter_set &asps)
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
        std::cout << "error not implemented ASPS PLR enabled" << std::endl;
    }
    asps.asps_vui_parameters_present_flag = read(1, "asps_vui_parameters_present_flag");
    if( asps.asps_vui_parameters_present_flag ) {
        std::cout << "error not implemented ASPS VUI enabled" << std::endl;
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

void BitstreamParsing::read_afps(atlas_frame_parameter_set &afps)
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

void BitstreamParsing::read_atlas_rbsp(atlas_tile_layer_rbsp* rbsp, NAL_UNIT_TYPE nalu_t)
{
    read_atlas_tile_header(rbsp->ath, nalu_t);
    read_atlas_tile_data_unit(rbsp->atdu, rbsp->ath);
    align_bitstream();
}

void BitstreamParsing::read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t)
{
    if(nalu_t >= NAL_GBLA_W_LP && nalu_t <= NAL_RSV_IRAP_ACL_29) {
        ath.ath_no_output_of_prior_atlas_frames_flag = read(1, "ath_no_output_of_prior_atlas_frames_flag");
    }
    ath.ath_atlas_frame_parameter_set_id = read_ue("ath_atlas_frame_parameter_set_id");
    ath.ath_atlas_adaptation_parameter_set_id = read_ue("ath_atlas_adaptation_parameter_set_id");
    //ath.ath_id = read("ath_id"); TODO: Figure out the dynamic bit length for this field
    uint16_t tileID = ath.ath_id; // default 0
    ath.ath_type = static_cast<ATH_TYPE>(read_ue("ath_type"));

    if(saved_afps_.afps_output_flag_present_flag) {
        ath.ath_atlas_output_flag = read(1, "ath_atlas_output_flag");
    }
    size_t Log2MaxAtlasFrmOrderCntLsb = saved_asps_.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    ath.ath_atlas_frm_order_cnt_lsb = read(uint8_t(Log2MaxAtlasFrmOrderCntLsb), "ath_atlas_frm_order_cnt_lsb"); //u(v)

    if(saved_asps_.asps_num_ref_atlas_frame_lists_in_asps > 0) {
        ath.ath_ref_atlas_frame_list_asps_flag = read(1, "ath_ref_atlas_frame_list_asps_flag");
    }
    if(ath.ath_ref_atlas_frame_list_asps_flag == 0) {
        std::cout << "ERROR: NOT IMPLEMENTED" << std::endl;
        return;
    }
    else if (saved_asps_.asps_num_ref_atlas_frame_lists_in_asps > 1) {
        size_t bit_len = std::ceil(std::log2(saved_asps_.asps_num_ref_atlas_frame_lists_in_asps));
        ath.ath_ref_atlas_frame_list_idx = read(uint8_t(bit_len), "ath_ref_atlas_frame_list_idx");
    }

    size_t NumLtrAtlasFrmEntries = 0; // default value, ref list is from ASPS TODO: dynamic
    ath.ath_additional_afoc_lsb_present_flag.resize(NumLtrAtlasFrmEntries);
    ath.ath_additional_afoc_lsb_val.resize(NumLtrAtlasFrmEntries);
    for (size_t j = 0; j < NumLtrAtlasFrmEntries; j++) {
        ath.ath_additional_afoc_lsb_present_flag.at(j) = read(1, "ath_additional_afoc_lsb_present_flag");
        if(ath.ath_additional_afoc_lsb_present_flag.at(j)) {
            ath.ath_additional_afoc_lsb_val.at(j) = read(saved_afps_.afps_additional_lt_afoc_lsb_len, "ath_additional_afoc_lsb_val");
        }
    }
    if(ath.ath_type != SKIP_TILE) {
        if(saved_asps_.asps_normal_axis_limits_quantization_enabled_flag) {
            ath.ath_pos_min_d_quantizer = read(5, "ath_pos_min_d_quantizer");
            if(saved_asps_.asps_normal_axis_max_delta_value_enabled_flag) {
                ath.ath_pos_delta_max_d_quantizer = read(5, "ath_pos_delta_max_d_quantizer");
            }
        }
        if(saved_asps_.asps_patch_size_quantizer_present_flag) {
            ath.ath_patch_size_x_info_quantizer = read(3, "ath_patch_size_x_info_quantizer");
            ath.ath_patch_size_y_info_quantizer = read(3, "ath_patch_size_y_info_quantizer");
        }
        if(saved_afps_.afps_raw_3d_offset_bit_count_explicit_mode_flag) {
            size_t bit_len = std::floor(std::log2(saved_asps_.asps_geometry_3d_bit_depth_minus1 + 1));
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

void BitstreamParsing::read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid)
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

void BitstreamParsing::read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu)
{
    pdu.pdu_2d_pos_x = read_ue("pdu_2d_pos_x");
    pdu.pdu_2d_pos_y = read_ue("pdu_2d_pos_y");
    pdu.pdu_2d_size_x_minus1 = read_ue("pdu_2d_size_x_minus1");
    pdu.pdu_2d_size_y_minus1 = read_ue("pdu_2d_size_y_minus1");

    pdu.pdu_3d_offset_u = read(saved_asps_.asps_geometry_3d_bit_depth_minus1 + 1, "pdu_3d_offset_u");
    pdu.pdu_3d_offset_v = read(saved_asps_.asps_geometry_3d_bit_depth_minus1 + 1, "pdu_3d_offset_v");
    pdu.pdu_3d_offset_d = read(saved_asps_.asps_geometry_3d_bit_depth_minus1 - ath.ath_pos_min_d_quantizer + 1, "pdu_3d_offset_d");

    if(saved_asps_.asps_normal_axis_max_delta_value_enabled_flag) {
        uint32_t rangeDBitDepth = std::min(saved_asps_.asps_geometry_2d_bit_depth_minus1, saved_asps_.asps_geometry_3d_bit_depth_minus1) + 1;
        pdu.pdu_3d_range_d = read(rangeDBitDepth - ath.ath_pos_delta_max_d_quantizer, "pdu_3d_range_d");
    }
    pdu.pdu_projection_id = read(ceil(log2(6)), "pdu_projection_id");
    pdu.pdu_orientation_index = read(false ? 3 : 1, "pdu_orientation_index");

    if(saved_afps_.afps_lod_mode_enabled_flag) {
        pdu.pdu_lod_enabled_flag = read(1, "pdu_lod_enabled_flag");
        if(pdu.pdu_lod_enabled_flag) {

            pdu.pdu_lod_scale_x_minus1 = read_ue("pdu_lod_scale_x_minus1");
            pdu.pdu_lod_scale_y_idc = read_ue("pdu_lod_scale_y_idc");
        }
    }
    // if( asps_plr_enabled_flag )               == false
    // if( asps_miv_extension_present_flag )     == false
}


void BitstreamParsing::read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath)
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
            atdu.patches_vec.push_back(pid);

        }
    }
}

void BitstreamParsing::read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size, decompressed_data* output)
{                
    switch(nal_unit_type) {
            case NAL_UNIT_TYPE::NAL_ASPS:
                read_asps(output->asps);
                saved_asps_ = output->asps;
                break;
            case NAL_UNIT_TYPE::NAL_AFPS:
                read_afps(output->afps);
                saved_afps_ = output->afps;
                break;
            case NAL_UNIT_TYPE::NAL_IDR_N_LP: {
                auto rbsp = std::make_unique<atlas_tile_layer_rbsp>();
                read_atlas_rbsp(rbsp.get(), nal_unit_type);
                output->rbsp_vec.push_back(std::move(rbsp));
                break; }
            default: 
                std::cout << "Unsupported NAL type" << std::endl;
                advance_bitstream(nal_unit_size * 8 - 16); // skip the rest of NAL unit for now 
                break;
        }
}

void BitstreamParsing::read_atlas_sub_bitstream(std::size_t v3c_payload_size_bytes, decompressed_data* output)
{
    std::cout << "Reading atlas data " << v3c_payload_size_bytes << std::endl;

    std::size_t end_ptr = pos_.bytes + v3c_payload_size_bytes;
    //advance_bitstream(v3c_payload_size_bytes * 8);
    // 3 bits for nAL unit size precision - 1 and 5 reserved
    uint8_t nal_size_precision_bytes = read(3, "nal_size_precision_in_bytes") + 1;
    std::cout << "--- NAL size precision in bytes: " << uint32_t(nal_size_precision_bytes) << std::endl;
    std::size_t nal_unit_precision_bits = nal_size_precision_bytes * 8;

    advance_bitstream(5);

    while (true) {
        if (pos_.bytes >= end_ptr) {
            break;
        }
        std::size_t nal_unit_size = read(nal_unit_precision_bits, "nal unit size");

        // Inside nal unit now
        std::cout << "--- Current NAL unit location " << pos_.bytes << ", size " << nal_unit_size << std::endl;
        
        read(1, "nal_forbidden_zero_bit");
        NAL_UNIT_TYPE nal_unit_type = static_cast<NAL_UNIT_TYPE>(read(6, "nal_unit_type"));
        uint8_t nal_layer_id = read(6, "nal_layer_id");
        uint8_t nal_temporal_id_plus1 = read(3, "nal_temporal_id_plus1");
        read_atlas_nal_unit(nal_unit_type, nal_unit_size, output);
    }
}

void BitstreamParsing::convert_video_sub_bitstream(std::size_t v3c_payload_size_bytes, std::string output_path)
{
    std::cout << "Reading video data " << v3c_payload_size_bytes << std::endl;
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
        std::cout << "Current HEVC NAL unit location " << pos_.bytes << ", size " << nalu_size << std::endl;
        
        file.write(hevc_start_code, 4);
        file.write(reinterpret_cast<const char*>(&cbuf_[pos_.bytes]), nalu_size);
        advance_bitstream(nalu_size * 8);
    }
    file.close();
}

void BitstreamParsing::decode_video_sub_bitstream(std::string input_path, std::string output_path)
{
    std::stringstream cmd;
    cmd << ffmpeg_path << " -f hevc -i " << input_path;
    cmd << " -pix_fmt yuv444p"; // to get 8bit depth > " --OutputBitDepth=8 --OutputBitDepthC=8";
    cmd << " " << output_path;
    std::cout << cmd.str() << '\n';
    if (std::system(cmd.str().c_str()) != 0) {
        throw std::runtime_error("During the encoding of the sequence, an error occured while executing the following command: " +
            cmd.str());
        return;
    }
    //std::remove(input_path.c_str());
    //std::remove(output_path.c_str());
}