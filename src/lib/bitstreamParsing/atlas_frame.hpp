#pragma once

#include <cmath>

#include "sei.hpp"
#include "bitstream_common.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

struct ref_list_struct {
    uint8_t num_ref_entries = 0;
    std::vector<bool> st_ref_atlas_frame_flags = {true};
    std::vector<uint8_t> abs_delta_afoc_st = {};
    std::vector<bool> strpf_entry_sign_flags = {};
    std::vector<uint8_t> afoc_lsb_lt = {};
}; 

struct atlas_tile_header {
    // from specification
    bool ath_no_output_of_prior_atlas_frames_flag = false;
    uint16_t ath_atlas_frame_parameter_set_id = 0;
    uint16_t ath_atlas_adaptation_parameter_set_id = 0;
    uint16_t ath_id = 0;
    ATH_TYPE ath_type;
    bool ath_atlas_output_flag = false;
    size_t ath_atlas_frm_order_cnt_lsb = 0;
    bool ath_ref_atlas_frame_list_asps_flag = false;
    ref_list_struct ref_list_struct_;
    uint8_t ath_ref_atlas_frame_list_idx = 0;
    std::vector<bool> ath_additional_afoc_lsb_present_flag = {0};
    std::vector<uint8_t> ath_additional_afoc_lsb_val = {0};
    uint8_t ath_pos_min_d_quantizer = 0;
    uint8_t ath_pos_delta_max_d_quantizer = 0;
    uint8_t ath_patch_size_x_info_quantizer = 0;
    uint8_t ath_patch_size_y_info_quantizer = 0;
    uint8_t ath_raw_3d_offset_axis_bit_count_minus1 = 0;
    bool ath_num_ref_idx_active_override_flag = false;
    uint8_t ath_num_ref_idx_active_minus1 = 0;
    uint8_t ath_tile_nalu_type_info;


};

// 8.3.7.9 Point local reconstruction data syntax
struct plr_data {
    size_t               block_to_patch_map_height_ = 0;
    size_t               block_to_patch_map_width_ = 0;
    bool                 level_flag = false;
    bool                 present_flag = false;
    uint8_t              mode_minus1 = 0;
    std::vector<bool>    block_present_flags = {};
    std::vector<uint8_t> block_mode_minus1s = {};

    void allocate(size_t block_to_patch_map_width, size_t block_to_patch_map_height) {
        block_to_patch_map_width_ = block_to_patch_map_width;
        block_to_patch_map_height_ = block_to_patch_map_height;
        block_present_flags.resize(block_to_patch_map_width_ * block_to_patch_map_height_, false);
        block_mode_minus1s.resize(block_to_patch_map_width_ * block_to_patch_map_height_, 0);
    } 
};

// 8.3.6.1.2 Point local reconstruction information syntax
struct plr_information {
    bool                 map_enabled_flag = false;
    uint8_t              num_modes_minus1 = 0;
    std::vector<bool>    interpolate_flags = {};
    std::vector<bool>    filling_flags = {};
    std::vector<uint8_t> minimum_depths = {};
    std::vector<uint8_t> neighbour_minus1s = {};
    uint8_t              block_threshold_per_patch_minus1 = 0;

    void allocate() {
        minimum_depths.resize(num_modes_minus1 + 1, 0);
        neighbour_minus1s.resize(num_modes_minus1 + 1, 0);
        interpolate_flags.resize(num_modes_minus1 + 1, false);
        filling_flags.resize(num_modes_minus1 + 1, false);
    }
};

struct patch_data_unit {
    // from spec
    size_t pdu_2d_pos_x = 0;
    size_t pdu_2d_pos_y = 0;
    uint64_t pdu_2d_size_x_minus1 = 0;
    uint64_t pdu_2d_size_y_minus1 = 0;
    size_t pdu_3d_offset_u = 0;
    size_t pdu_3d_offset_v = 0;
    size_t pdu_3d_offset_d = 0;
    size_t pdu_3d_range_d = 0;
    size_t pdu_projection_id = 0;
    size_t pdu_orientation_index = 0;
    bool pdu_lod_enabled_flag = false;
    uint8_t pdu_lod_scale_x_minus1 = 0;
    uint8_t pdu_lod_scale_y_idc = 0;
    plr_data plr_data_;

    size_t patchIndex_;
    size_t frameIndex_;
    size_t tileOrder_;
};
struct inter_patch_data_unit {
    int64_t ref_index = 0;
    int64_t ref_patch_index = 0;
    int64_t pdu_2d_pos_x = 0;
    int64_t pdu_2d_pos_y = 0;
    int64_t delta_2d_size_x = 0;
    int64_t delta_2d_size_y = 0;
    int64_t pdu_3d_offset_u = 0;
    int64_t pdu_3d_offset_v = 0;
    int64_t pdu_3d_offset_d = 0;
    int64_t pdu_3d_range_d = 0;
    plr_data plr_data_;

    size_t patchIndex_;
    size_t frameIndex_;
    size_t tileOrder_;
};
struct merge_patch_data_unit {};
struct skip_patch_data_unit {};
struct raw_patch_data_unit {};
struct eom_patch_data_unit {};

struct patch_information_data {
    // helper variable
    uint8_t patchMode_; // atdu_patch_mode
    size_t patchIndex_;
    size_t tileOrder_;

    // From specification
    patch_data_unit patch_data_unit_;
    inter_patch_data_unit inter_patch_data_unit_;
    merge_patch_data_unit merge_patch_data_unit_;
    skip_patch_data_unit skip_patch_data_unit_;
    raw_patch_data_unit raw_patch_data_unit_;
    eom_patch_data_unit eom_patch_data_unit_;
};

struct atlas_tile_data_unit {
    // from specification
    uint32_t tile_order_ = 0;
    std::vector<patch_information_data> patch_information_data_;
};

struct atlas_tile_layer_rbsp {
    atlas_tile_header ath_;
    atlas_tile_data_unit atdu_;
    pccsei sei_;
};

struct atlas_frame_tile_information {
    bool afti_single_tile_in_atlas_frame_flag = false;
    bool afti_uniform_partition_spacing_flag;
    uint32_t afti_num_partition_columns_minus1 = 0;
    uint32_t afti_num_partition_rows_minus1 = 0;
    std::vector<uint32_t> afti_partition_column_width_minus1 = {};
    std::vector<uint32_t> afti_partition_row_height_minus1 = {};
    bool afti_single_partition_per_tile_flag;
    uint32_t afti_num_tiles_in_atlas_frame_minus1 = 0;

    std::vector<uint32_t> afti_top_left_partition_idx;
    std::vector<uint32_t> afti_bottom_right_partition_column_offset;
    std::vector<uint32_t> afti_bottom_right_partition_row_offset;
    uint32_t afti_auxiliary_video_tile_row_width_minus1;
    std::vector<uint32_t> afti_auxiliary_video_tile_row_height;
    bool afti_signalled_tile_id_flag;
    uint32_t afti_signalled_tile_id_length_minus1;
    std::vector<uint32_t> afti_tile_id;
};

struct atlas_frame_parameter_set {
    uint8_t afps_atlas_frame_parameter_set_id = 0;
    uint8_t afps_atlas_sequence_parameter_set_id = 0;
    atlas_frame_tile_information afti;
    bool afps_output_flag_present_flag = false;
    uint8_t afps_num_ref_idx_default_active_minus1 = 0;
    uint8_t afps_additional_lt_afoc_lsb_len = 0;
    bool afps_lod_mode_enabled_flag = false;
    bool afps_raw_3d_offset_bit_count_explicit_mode_flag = false;
    bool afps_extension_flag = false;
    bool afps_miv_extension_present_flag = false;
    uint8_t afps_extension_7bits = 0;
    uint8_t afps_extension_8bits = 0;
    bool afps_extension_data_flag = false;
};

struct atlas_sequence_parameter_set {
    uint8_t asps_atlas_sequence_parameter_set_id = 0;
    uint16_t asps_frame_width = 0;
    uint16_t asps_frame_height = 0;
    uint16_t asps_geometry_3d_bit_depth_minus1 = 0;
    uint16_t asps_geometry_2d_bit_depth_minus1 = 0;
    uint8_t asps_log2_max_atlas_frame_order_cnt_lsb_minus4 = 4;
    uint8_t asps_max_dec_atlas_frame_buffering_minus1 = 0;
    bool asps_long_term_ref_atlas_frames_flag = false;
    uint8_t asps_num_ref_atlas_frame_lists_in_asps = 0;

    std::vector<ref_list_struct> ref_lists;

    bool asps_use_eight_orientations_flag = false;
    bool asps_extended_projection_enabled_flag = false;
    size_t asps_max_number_projections_minus1 = 5;
    bool asps_normal_axis_limits_quantization_enabled_flag = true;
    bool asps_normal_axis_max_delta_value_enabled_flag = false;
    bool asps_patch_precedence_order_flag = false;
    uint8_t asps_log2_patch_packing_block_size = 0;
    bool asps_patch_size_quantizer_present_flag = false;
    uint8_t asps_map_count_minus1 = 0;
    bool asps_pixel_deinterleaving_enabled_flag = false;
    std::vector<bool> asps_map_pixel_deinterleaving_flag = {};
    bool asps_raw_patch_enabled_flag = false;
    bool asps_eom_patch_enabled_flag = false;
    uint8_t asps_eom_fix_bit_count_minus1 = 0;
    bool asps_auxiliary_video_enabled_flag = false;
    bool asps_plr_enabled_flag = false;
    std::vector<plr_information> plr_information_list = {};
    bool asps_vui_parameters_present_flag = false;
    bool asps_extension_flag = false;
    bool asps_vpcc_extension_flag = false;
    bool asps_miv_extension_flag = false;
    uint8_t asps_extension_6bits = 0;
    uint8_t asps_extension_7bits = 0;

    bool asps_vpcc_remove_duplicate_point_enabled_flag;
    uint16_t asps_vpcc_surface_thickness_minus1;
};