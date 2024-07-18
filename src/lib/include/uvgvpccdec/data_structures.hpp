#pragma once

#include <cmath>
#include "uvgvpccdec/uvgvpccdec.hpp"
#include "bitstream_common.hpp"

using std::size_t;

struct ref_list_struct {
    uint8_t num_ref_entries = 0;
    std::vector<bool> st_ref_atlas_frame_flag = {true};
    std::vector<uint8_t> abs_delta_afoc_st = {};
    std::vector<bool> straf_entry_sign_flag = {};
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
    ref_list_struct refs;
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
};

// TODO: implement plr data
struct plr_data {};

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
    uint8_t pdu_lod_scale_y_idc = 1;
    plr_data plr_data_;
};
struct inter_patch_data_unit {};
struct merge_patch_data_unit {};
struct skip_patch_data_unit {};
struct raw_patch_data_unit {};
struct eom_patch_data_unit {};

struct patch_information_data {

    // helper variable
    uint8_t patchMode;

    // From specification
    patch_data_unit patch;
    inter_patch_data_unit inter;
    merge_patch_data_unit merge;
    skip_patch_data_unit skip;
    raw_patch_data_unit raw;
    eom_patch_data_unit eom;
};

struct atlas_tile_data_unit {
    // from specification
    uint32_t atdu_patch_mode = 0;
    std::vector<patch_information_data> pid_vec;
};

struct atlas_tile_layer_rbsp {
    atlas_tile_header ath;
    atlas_tile_data_unit atdu;
};

struct atlas_frame_tile_information {
    bool afti_single_tile_in_atlas_frame_flag;
    bool afti_uniform_partition_spacing_flag;
    uint32_t afti_partition_cols_width_minus1;
    uint32_t afti_partition_rows_height_minus1;
    uint32_t afti_num_partition_columns_minus1;
    uint32_t afti_num_partition_rows_minus1;
    std::vector<uint32_t> afti_partition_column_width_minus1;
    std::vector<uint32_t> afti_partition_row_height_minus1;
    bool afti_single_partition_per_tile_flag;
    uint32_t afti_num_tiles_in_atlas_frame_minus1;

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
    bool afps_extension_present_flag = false;
    bool afps_miv_extension_present_flag = false;
    uint8_t afps_extension_7bits = 0;
    bool afps_extension_data_flag  = false;
};

struct atlas_sequence_parameter_set {
    uint8_t asps_atlas_sequence_parameter_set_id = 0;
    uint16_t asps_frame_width = 0;
    uint16_t asps_frame_height = 0;
    uint8_t asps_geometry_3d_bit_depth_minus1 = 0;
    uint8_t asps_geometry_2d_bit_depth_minus1 = 0;
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
    bool asps_vui_parameters_present_flag = false;
    bool asps_extension_present_flag = false;
    bool asps_vpcc_extension_present_flag = false;
    bool asps_miv_extension_present_flag = false;
    uint8_t asps_extension_6bits = 0;

    bool asps_vpcc_remove_duplicate_point_enabled_flag;
    uint16_t asps_vpcc_surface_thickness_minus1;
};

struct profile_toolset_constraints_information {
    bool ptc_one_v3c_frame_only_flag = false;
    bool ptc_eom_constraint_flag = false;
    uint8_t ptc_max_map_count_minus1 = 1;
    uint8_t ptc_max_atlas_count_minus1 = 0;
    bool ptc_multiple_map_streams_constraint_flag = false;
    bool ptc_plr_constraint_flag = false;
    uint8_t ptc_attribute_max_dimension_minus1 = 2;
    uint8_t ptc_attribute_max_dimension_partitions_minus1 = 0;
    bool ptc_no_eight_orientations_constraint_flag = true;
    bool ptc_no_45degree_projection_patch_constraint_flag = true;
    bool ptc_restricted_geometry_flag = false;
    uint8_t ptc_num_reserved_constraint_bytes = 0;
    std::vector<uint8_t> ptc_reserved_constraint_byte = {};
};

struct profile_tier_level {
    bool ptl_tier_flag = false;
    uint8_t ptl_profile_codec_group_idc = 0;
    uint8_t ptl_profile_toolset_idc = 0;
    uint8_t ptl_profile_reconstruction_idc = 0;
    uint8_t ptl_max_decodes_idc = 0;
    uint8_t ptl_level_idc = 0;
    uint8_t ptl_num_sub_profiles = 0;
    bool ptl_extended_sub_profile_flag = false; // 0: use ptl_sub_profile_idc_32, 1: use ptl_sub_profile_idc_64
    std::vector<uint32_t> ptl_sub_profile_idc_32 = {};
    std::vector<uint64_t> ptl_sub_profile_idc_64 = {};
    bool ptl_toolset_constraints_present_flag = false;
    profile_toolset_constraints_information ptc;
};

struct occupancy_information {
    uint8_t oi_lossy_occupancy_compression_threshold = 0;
    uint8_t oi_occupancy_2d_bit_depth_minus1 = 10;
    bool oi_occupancy_MSB_align_flag = false;
    uint8_t oi_occupancy_codec_id = 0;
};

struct geometry_information {
    uint8_t gi_geometry_codec_id = 0;
    uint8_t gi_geometry_2d_bit_depth_minus1 = 10;
    bool gi_geometry_MSB_align_flag = false;
    uint8_t gi_geometry_3d_coordinates_bit_depth_minus1 = 9;
    uint8_t gi_auxiliary_geometry_codec_id = 0;
};

struct attribute_information {
    std::vector<uint8_t> ai_attribute_type_id = {};
    std::vector<uint8_t> ai_attribute_codec_id = {};
    std::vector<uint8_t> ai_auxiliary_attribute_codec_id = {};
    std::vector<bool> ai_attribute_map_absolute_coding_persistence_flag = {};
    std::vector<uint8_t> ai_attribute_dimension_minus1 = {};
    std::vector<uint8_t> ai_attribute_dimension_partitions_minus1 = {};
    std::vector<std::vector<uint16_t>> ai_attribute_partition_channels_minus1{};
    std::vector<uint8_t> ai_attribute_2d_bit_depth_minus1 = {};
    std::vector<bool> ai_attribute_MSB_align_flag = {};
};

struct v3c_parameter_set {

    // helper variables
    size_t vps_length_bytes;
    uint8_t codec_group; // AVD, VVC, HEVC, or other

    profile_tier_level ptl;
    uint8_t vps_v3c_parameter_set_id;
    uint8_t vps_atlas_count_minus1;
    
    std::vector<uint8_t> vps_atlas_id;
    std::vector<size_t> vps_frame_width;
    std::vector<size_t> vps_frame_height;
    std::vector<uint8_t> vps_map_count_minus1;
    std::vector<bool> vps_multiple_map_streams_present_flag;
    std::vector<std::vector<bool>> vps_map_absolute_coding_enabled_flag;
    std::vector<std::vector<uint16_t>> vps_map_predictor_index_diff;
    std::vector<bool> vps_auxiliary_video_present_flag;
    std::vector<bool> vps_occupancy_video_present_flag;
    std::vector<bool> vps_geometry_video_present_flag;
    std::vector<bool> vps_attribute_video_present_flag;
    std::vector<occupancy_information> occupancy_info;
    std::vector<geometry_information> geometry_info;
    uint8_t ai_attribute_count = 0;
    std::vector<attribute_information> attribute_info;
    
    bool vps_extension_present_flag;
    bool vps_packing_information_present_flag;
    bool vps_miv_extension_present_flag;
    uint8_t vps_extension_6bits;
    size_t vps_extension_length_minus1;
    uint8_t vps_extension_data_byte;
};

struct picture {
    size_t height = 0;
    size_t width = 0;
    std::vector<uint8_t> Y;
    std::vector<uint8_t> U;
    std::vector<uint8_t> V;

    size_t get_Y_value(const size_t u, const size_t v ) { // Should be uint8_t??
        return Y.at(v * width + u);
    }
};

struct video_map {
    size_t height = 0;
    size_t width = 0;
    size_t frame_count = 0;
    std::vector<picture> pictures = {};
};

struct patch {

    size_t index_ = 0;          // patch index
    size_t originalIndex_ = 0;  // patch original index
    size_t frameIndex_ = 0;     // Frame index
    size_t tileIndex_ = 0;      // Tile index
    size_t indexInFrame_ = 0;   // index in frame

    uint32_t TilePatchType = 0; // Should be PROJECTED
    uint32_t TilePatch2dPosX = 0; // u0 location in packed image (n*occupancyResolution_)
    uint32_t TilePatch2dPosY = 0; // v0 location in packed image (n*occupancyResolution_)
    uint32_t TilePatch2dSizeX = 1; // sizeU0 size of occupancy map (n*occupancyResolution_)
    uint32_t TilePatch2dSizeY = 1; // sizeV0 size of occupancy map (n*occupancyResolution_)

    uint32_t getSizeU0() {return TilePatch2dSizeY;}
    uint32_t getSizeV0() {return TilePatch2dSizeX;}

    uint32_t TilePatch3dOffsetU = 0; // u1 tangential shift
    uint32_t TilePatch3dOffsetV = 0; // v1 bitangential shift
    uint32_t TilePatch3dOffsetD = 0; // d1 depth shift

    uint32_t TilePatch3dRangeD = 0; // sizeD size for depth??
    uint32_t TilePatchProjectionID = 0;
    uint32_t TilePatchOrientationIndex = 0;
    uint32_t TilePatchLoDScaleX = 1;
    uint32_t TilePatchLoDScaleY = 1;

    size_t occupancy_resolution = 0; //helper var tmc2

    /*
uint32_t offsetY; // whats this?
    size_t                  u1_;             // tangential shift
    size_t                  v1_;             // bitangential shift
    size_t                  d1_;             // depth shift
    size_t                  sizeD_;          // size for depth
    size_t                  sizeDPixel_;     // Size D pixel
    size_t                  sizeU_;          // size for depth
    size_t                  sizeV_;          // size for depth
    size_t                  u0_;             // location in packed image (n*occupancyResolution_)
    size_t                  v0_;             // location in packed image (n*occupancyResolution_)
    size_t                  sizeU0_;         // size of occupancy map (n*occupancyResolution_)
    size_t                  sizeV0_;         // size of occupancy map (n*occupancyResolution_)
    size_t                  size2DXInPixel_;
    size_t                  size2DYInPixel_;
    size_t                  occupancyResolution_;  // occupancy map resolution
    size_t                  projectionMode_;       // 0: related to the min depth value; 1: related to the max value
    size_t                  levelOfDetailX_;
    size_t                  levelOfDetailY_;
    size_t                  normalAxis_;     // x
    size_t                  tangentAxis_;    // y
    size_t                  bitangentAxis_;  // z
    std::vector<int16_t>    depth_[2];       // depth
    std::vector<bool>       occupancy_;      // occupancy map*/
};

struct point3d {
    uint16_t pos3D[3] = {0, 0, 0};

};

struct point_cloud_frame {
    std::vector<point3d> positions = {};
};

struct atlas_frame {
    // As we only currently support one tile per atlas frame, the frame directly contains the patch map
    // otherwise tiles would be in between frame and patch
    std::vector<patch> patches_map = {};

    // These are here for now as only 1 tile per frame. TODO; FIX
    size_t tile_width = 0;
    size_t tile_height = 0;

    std::vector<size_t> block_to_patch;
    size_t getLeftTopXInFrame() {return 0;}
    size_t getLeftTopYInFrame() {return 0;}
};

struct decompressed_data { // of a gof currently
    v3c_parameter_set vps;
    atlas_sequence_parameter_set asps;
    atlas_frame_parameter_set afps;
    size_t frame_count = 0; // this is fetched from the number of atlas frames
    std::vector<std::unique_ptr<atlas_frame>> atlas_map; // frames or tiles? currently 1 tile per frame
    video_map occupancy_map = {};
    video_map geometry_map = {};
    video_map attribute_map = {};
};