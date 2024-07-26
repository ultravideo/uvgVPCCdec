#pragma once

#include <cmath>
#include "uvgvpccdec/uvgvpccdec.hpp"
#include "bitstream_common.hpp"
#include <assert.h>
#include <cstring>

enum PCCCOLORFORMAT { UNKNOWN = 0, RGB444, YUV444, YUV420 };

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
    PCCCOLORFORMAT format;
    std::vector<uint8_t> Y;
    std::vector<uint8_t> U;
    std::vector<uint8_t> V;

    size_t get_Y_value(const size_t u, const size_t v ) { // Should be uint8_t??
        return Y.at(v * width + u);
    }
    // Works for 444. IF you want 420, this needs to change a bit
    // Check integer lengths
    uint8_t get_value(const size_t channel, const size_t u, const size_t v) {
        assert( channel < 3 );
        if(channel == 0) {
            return Y.at(v * width + u);
        } else if (channel == 1) {
            return U.at(v * width + u);
        } else if (channel == 2) {
            return V.at(v * width + u);
        }
        return 0;
    }

    void convert_yuv_420_to_444(const picture* old) {
        format = PCCCOLORFORMAT::YUV444;
        width = old->width;
        height = old->height;
        size_t size = old->width * old->height;
        Y.resize(size, 0);
        U.resize(size, 0);
        V.resize(size, 0);
        // Y
        std::copy(old->Y.begin(), old->Y.end(), Y.begin());

        // U
        const size_t width2 = width / 2;
        auto&    dst = U;
        const uint8_t* src = old->U.data();
        for ( size_t y = 0; y < height; y += 2 ) {
            auto* const buffer = dst.data() + y * width;
            for ( size_t x2 = 0; x2 < width2; ++x2, src++ ) {
                const size_t x = x2 * 2;
                buffer[x]      = *src;
                buffer[x + 1]  = *src;
            }
            std::memcpy( (char*)( buffer + width ), (char*)buffer, width * sizeof( uint8_t ) );
        }

        // V
        auto&    dst2 = V;
        const uint8_t* src2 = old->V.data();
        for ( size_t y = 0; y < height; y += 2 ) {
            auto* const buffer = dst2.data() + y * width;
            for ( size_t x2 = 0; x2 < width2; ++x2, src2++ ) {
                const size_t x = x2 * 2;
                buffer[x]      = *src2;
                buffer[x + 1]  = *src2;
            }
            memcpy( (char*)( buffer + width ), (char*)buffer, width * sizeof( uint8_t ) );
        }
    }
};

struct video_map {
    V3C_UNIT_TYPE type;
    size_t height = 0;
    size_t width = 0;
    size_t frame_count = 0;
    std::vector<picture> pictures = {};
};

struct vector3d {
    size_t data_[3];

    vector3d(const size_t x, size_t y, size_t z ) {
        data_[0] = x;
        data_[1] = y;
        data_[2] = z;
    }
    vector3d()        = default;
    size_t&       x() { return data_[0]; }
    size_t&       y() { return data_[1]; }
    size_t&       z() { return data_[2]; }
};

struct point3d {
    uint16_t data_[3];

    uint16_t& operator[]( size_t i ) {
        assert( i < 3 );
        return data_[i];
    }
    const uint16_t& operator[]( size_t i ) const {
        assert( i < 3 );
        return data_[i];
    }

    bool operator==(const point3d &cmp) const {
        return ( data_[0] == cmp.data_[0] && data_[1] == cmp.data_[1] && data_[2] == cmp.data_[2] );
    };
    bool operator!=(const point3d &cmp) const {
        return ( data_[0] != cmp.data_[0] || data_[1] != cmp.data_[1] || data_[2] != cmp.data_[2] );
    }

    uint16_t&       x() { return data_[0]; }
    uint16_t&       y() { return data_[1]; }
    uint16_t&       z() { return data_[2]; }
};

struct uvg_color {
    uint8_t data_[3];
    uint8_t&       r() { return data_[0]; }
    uint8_t&       g() { return data_[1]; }
    uint8_t&       b() { return data_[2]; } 
    uvg_color() {data_[0] = 0; data_[1] = 0; data_[2] = 0;}
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

    uint32_t TilePatch3dOffsetU = 0; // u1 tangential shift
    uint32_t TilePatch3dOffsetV = 0; // v1 bitangential shift
    uint32_t TilePatch3dOffsetD = 0; // d1 depth shift

    uint32_t TilePatch3dRangeD = 0; // sizeD size for depth??
    uint32_t TilePatchProjectionID = 0;
    uint32_t TilePatchOrientationIndex = 0;
    uint32_t TilePatchLoDScaleX = 1;
    uint32_t TilePatchLoDScaleY = 1;

    size_t occupancy_resolution = 0; //helper var tmc2
    size_t size2DXInPixel_; // needed? tmc2
    size_t size2DYInPixel_; // needed? tmc2

    size_t levelOfDetailX_ = 1; // TMC2, for point generation
    size_t levelOfDetailY_ = 1; // TMC2, for point generation
    size_t normalAxis_;     // x TMC2, for point generation
    size_t tangentAxis_;    // y TMC2, for point generation
    size_t bitangentAxis_;  // z TMC2, for point generation
    size_t viewId_;
    size_t axisOfAdditionalPlane_;

    inline double generateNormalCoordinate( const uint16_t depth, const size_t projectionMode ) const {
        uint32_t d1_ = TilePatch3dOffsetD;
        double coord = 0;
        if ( projectionMode == 0 ) {
        coord = ( (double)depth + (double)d1_ );
        } else {
        double tmp_depth = double( d1_ ) - double( depth );
        if ( tmp_depth > 0 ) { coord = tmp_depth; }
        }
        return coord;
    }
    point3d generatePoint( const size_t u, const size_t v, const uint16_t depth ) const {
        point3d point0;
        point0.data_[normalAxis_]    = generateNormalCoordinate( depth, TilePatchProjectionID );
        point0.data_[tangentAxis_]   = ( double( u ) * (double)levelOfDetailX_ + TilePatch3dOffsetU );
        point0.data_[bitangentAxis_] = ( double( v ) * (double)levelOfDetailY_ + TilePatch3dOffsetV );
        return point0;
    }
    void setAxis( size_t axisOfAdditionalPlane,
                size_t normalAxis,
                size_t tangentAxis,
                size_t bitangentAxis,
                size_t projectionMode ) {
        axisOfAdditionalPlane_ = axisOfAdditionalPlane;
        normalAxis_            = normalAxis;
        tangentAxis_           = tangentAxis;
        bitangentAxis_         = bitangentAxis;
        TilePatchProjectionID        = projectionMode;
    }
    void setViewId( size_t viewId ) {
        viewId_ = viewId;
        // now set the other variables according to the viewId
        switch ( viewId_ ) {
            case 0: setAxis( 0, 0, 2, 1, 0 ); break;
            case 1: setAxis( 0, 1, 2, 0, 0 ); break;
            case 2: setAxis( 0, 2, 0, 1, 0 ); break;
            case 3: setAxis( 0, 0, 2, 1, 1 ); break;
            case 4: setAxis( 0, 1, 2, 0, 1 ); break;
            case 5: setAxis( 0, 2, 0, 1, 1 ); break;
            case 6: setAxis( 1, 0, 2, 1, 0 ); break;
            case 7: setAxis( 1, 2, 0, 1, 0 ); break;
            case 8: setAxis( 1, 0, 2, 1, 1 ); break;
            case 9: setAxis( 1, 2, 0, 1, 1 ); break;
            case 10: setAxis( 2, 2, 0, 1, 0 ); break;
            case 11: setAxis( 2, 1, 2, 0, 0 ); break;
            case 12: setAxis( 2, 2, 0, 1, 1 ); break;
            case 13: setAxis( 2, 1, 2, 0, 1 ); break;
            case 14: setAxis( 3, 1, 2, 0, 0 ); break;
            case 15: setAxis( 3, 0, 2, 1, 0 ); break;
            case 16: setAxis( 3, 1, 2, 0, 1 ); break;
            case 17: setAxis( 3, 0, 2, 1, 1 ); break;
            default:
            std::cout << "ViewId (" << viewId_ << ") not allowed... exiting" << std::endl;
            exit( -1 );
            break;
        }
    }
};

inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

struct point_cloud_frame {
    std::vector<point3d> positions = {};
    std::vector<uvg_color> colors = {};

    point3d& operator[]( size_t i ) {
        return positions[i];
    }
    const point3d& operator[]( size_t i ) const {
        return positions[i];
    }

    void clear() {
        positions.clear();
        colors.clear();
    }

    void set_color( const size_t index, const uvg_color color ) {
        if (index >= colors.size()) {
        }
        assert( index < colors.size() );
        colors[index] = color;
    }

    std::vector<uvg_color>& getColors8bit() { return colors; }
    size_t getPointCount() const { return positions.size(); }

    void resize( const size_t size ) {
        positions.resize( size );
    }

    size_t addPoint( const point3d& position ) {
        const size_t index = getPointCount();
        resize( index + 1 ); // NOTE - COSTLY OPERATION?
        positions[index] = position;
        return index;
    }
    size_t addPoint( const vector3d& position ) {
        const size_t index = getPointCount();
        resize( index + 1 );
        positions[index].data_[0] = (int16_t)position.data_[0];
        positions[index].data_[1] = (int16_t)position.data_[1];
        positions[index].data_[2] = (int16_t)position.data_[2];
        return index;
    }
};

struct atlas_frame {
    // As we only currently support one tile per atlas frame, the frame directly contains the patch map
    // otherwise tiles would be in between frame and patch
    std::vector<patch> patches_map = {};
    std::vector<vector3d> pointToPixel_ = {};

    size_t atlas_index = 0;

    // These are here for now as only 1 tile per frame. TODO; FIX
    size_t tile_width = 0;
    size_t tile_height = 0;

    std::vector<size_t> block_to_patch;
    std::vector<vector3d>& getPointToPixel() { return pointToPixel_; }
    size_t number_of_points = 0;
    void set_number_of_points( size_t val ) { number_of_points = val; }
};

struct decompressed_data { // of a gof currently
    v3c_parameter_set vps;
    atlas_sequence_parameter_set asps;
    atlas_frame_parameter_set afps;
    size_t frame_count = 0; // this is fetched from the number of atlas frames
    std::vector<std::unique_ptr<atlas_frame>> atlas_map; // frames or tiles? currently 1 tile per frame
    video_map occupancy_map = {};
    std::vector<video_map> geometry_maps = {};
    std::vector<video_map> attribute_maps = {};
};