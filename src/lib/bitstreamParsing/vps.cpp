#include "vps.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include "bitstream_util.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

bool vps::read_vps(bitstream_t* stream) {
    // profile_tier_level
    ptl_.ptl_tier_flag                        = readU(stream, 1, "ptl_tier_flag",gofId) != 0U;
    ptl_.ptl_profile_codec_group_idc          = readU(stream, 7, "ptl_profile_codec_group_idc",gofId);
    ptl_.ptl_profile_toolset_idc              = readU(stream, 8, "ptl_profile_toolset_idc",gofId);
    ptl_.ptl_profile_reconstruction_idc       = readU(stream, 8, "ptl_profile_reconstruction_idc",gofId);
                                                // readU(stream, 16, "ptl_reserved_zero_16bits",gofId);
                                                // readU(stream, 16, "ptl_reserved_zero_16bits",gofId);
                                                bitstream_advance(stream, 16);
                                                bitstream_advance(stream, 16);
    ptl_.ptl_level_idc                        = readU(stream, 8, "ptl_level_idc",gofId);
    ptl_.ptl_num_sub_profiles                 = readU(stream, 6, "ptl_num_sub_profiles",gofId);
    ptl_.ptl_extended_sub_profile_flag        = readU(stream, 1, "ptl_extended_sub_profile_flag",gofId);
    ptl_.ptl_sub_profile_idc.resize(ptl_.ptl_num_sub_profiles, 0);
    const uint32_t bitcount = ptl_.ptl_extended_sub_profile_flag == 0 ? 32 : 64;
    for (size_t i = 0; i < ptl_.ptl_num_sub_profiles; i++) {
        ptl_.ptl_sub_profile_idc.at(i)        = readU(stream, bitcount, "ptl_sub_profile_idc.at(i)",gofId);
    }
    ptl_.ptl_toolset_constraints_present_flag = readU(stream, 1, "ptl_toolset_constraints_present_flag",gofId);
    if (ptl_.ptl_toolset_constraints_present_flag) {
        // profileToolsetConstraintsInformation( ptl.getProfileToolsetConstraintsInformation(), bitstream );
    }

    vps_v3c_parameter_set_id_ = readU(stream, 4, "vps_v3c_parameter_set_id",gofId);
    uint8_t vps_reserved_zero_8bits = readU(stream, 8, "vps_reserved_zero_8bits",gofId);
    vps_atlas_count_minus1_ = readU(stream, 6, "vps_atlas_count_minus1",gofId);

    const size_t vps_atlas_count = vps_atlas_count_minus1_ + 1;
    /* Resize vectors */
    vps_atlas_id_.resize(vps_atlas_count, 0);
    vps_frame_width_.resize(vps_atlas_count, 1);
    vps_frame_height_.resize(vps_atlas_count);
    vps_map_count_minus1_.resize(vps_atlas_count);
    vps_multiple_map_streams_present_flag_.resize(vps_atlas_count);
    vps_map_absolute_coding_enabled_flag_.resize(vps_atlas_count);
    vps_auxiliary_video_present_flag_.resize(vps_atlas_count);
    vps_occupancy_video_present_flag_.resize(vps_atlas_count);
    vps_geometry_video_present_flag_.resize(vps_atlas_count);
    vps_attribute_video_present_flag_.resize(vps_atlas_count);

    vps_map_predictor_index_diff_.resize(vps_atlas_count);

    occupancy_info_.resize(vps_atlas_count);
    geometry_info_.resize(vps_atlas_count);
    attribute_info_.resize(vps_atlas_count);

    for (uint8_t j = 0; j < vps_atlas_count; j++) {
        vps_atlas_id_.at(j) = readU(stream, 6, "vps_atlas_id",gofId);
        vps_frame_width_.at(j) = readUE(stream, "vps_frame_width",gofId);
        vps_frame_height_.at(j) = readUE(stream, "vps_frame_height",gofId);
        vps_map_count_minus1_.at(j) = readU(stream, 4, "vps_map_count_minus1",gofId);

        if (vps_map_count_minus1_.at(j) > 0) {
            vps_multiple_map_streams_present_flag_.at(j) = readU(stream, 1, "vps_multiple_map_streams_present_flag",gofId) != 0U;
        }
        vps_map_absolute_coding_enabled_flag_.at(j).resize(vps_map_count_minus1_.at(j) + 1, 1);
        vps_map_predictor_index_diff_.at(j).resize(vps_map_count_minus1_.at(j) + 1);
        for (uint8_t i = 1; i <= vps_map_count_minus1_.at(j); i++) {
            if(vps_multiple_map_streams_present_flag_.at(j)) {
                vps_map_absolute_coding_enabled_flag_.at(j).at(i) = readU(stream, 1, "vps_map_absolute_coding_enabled_flag", gofId) != 0U;
            } else {
                vps_map_absolute_coding_enabled_flag_.at(j).at(i) = true;
            }
            if (static_cast<int>( vps_map_absolute_coding_enabled_flag_.at(j).at(i) ) == 0) {
                vps_map_predictor_index_diff_.at(j).at(i) = readUE(stream, "vps_map_predictor_index_diff_.at(j).at(i)",gofId) != 0U;
            }
        }
        vps_auxiliary_video_present_flag_.at(j) = readU(stream, 1, "vps_auxiliary_video_present_flag",gofId);
        vps_occupancy_video_present_flag_.at(j) = readU(stream, 1, "vps_occupancy_video_present_flag",gofId);
        vps_geometry_video_present_flag_.at(j) = readU(stream, 1, "vps_geometry_video_present_flag",gofId);
        vps_attribute_video_present_flag_.at(j) = readU(stream, 1, "vps_attribute_video_present_flag",gofId);

        // 8.3.4.3 Occupancy Information
        if (vps_occupancy_video_present_flag_.at(j)) {
            occupancy_info_.at(j).oi_occupancy_codec_id = readU(stream, 8, "oi_occupancy_codec_id",gofId);
            occupancy_info_.at(j).oi_lossy_occupancy_compression_threshold = readU(stream, 8, "oi_lossy_occupancy_compression_threshold",gofId);
            occupancy_info_.at(j).oi_occupancy_2d_bit_depth_minus1 = readU(stream, 5, "oi_occupancy_2d_bit_depth_minus1",gofId);
            occupancy_info_.at(j).oi_occupancy_MSB_align_flag = readU(stream, 1, "oi_occupancy_MSB_align_flag",gofId) != 0U;
        }

        // 8.3.4.4 Geometry information
        if (vps_geometry_video_present_flag_.at(j)) {
            geometry_info_.at(j).gi_geometry_codec_id = readU(stream, 8, "gi_geometry_codec_id",gofId);
            geometry_info_.at(j).gi_geometry_2d_bit_depth_minus1 = readU(stream, 5, "gi_geometry_2d_bit_depth_minus1",gofId);
            geometry_info_.at(j).gi_geometry_MSB_align_flag = readU(stream, 1, "gi_geometry_MSB_align_flag",gofId) != 0U;
            geometry_info_.at(j).gi_geometry_3d_coordinates_bit_depth_minus1 = readU(stream, 5,
                    "gi_geometry_3d_coordinates_bit_depth_minus1",gofId);
            if (vps_auxiliary_video_present_flag_.at(j)) {
                geometry_info_.at(j).gi_auxiliary_geometry_codec_id = readU(stream, 8, "gi_auxiliary_geometry_codec_id",gofId);
            }
        }

        // 8.3.4.5 Attribute information
        if (vps_attribute_video_present_flag_.at(j)) {
            const uint32_t ai_attribute_count = readU(stream, 7, "ai_attribute_count",gofId);
            attribute_info_.at(j).ai_attribute_count = ai_attribute_count;

            attribute_info_.at(j).ai_attribute_type_id.resize(ai_attribute_count, 0);
            attribute_info_.at(j).ai_attribute_codec_id.resize(ai_attribute_count, 0);
            attribute_info_.at(j).ai_auxiliary_attribute_codec_id.resize(ai_attribute_count, 0);
            attribute_info_.at(j).ai_attribute_dimension_minus1.resize(ai_attribute_count, 0);
            attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.resize(ai_attribute_count, 0);
            attribute_info_.at(j).ai_attribute_2d_bit_depth_minus1.resize(ai_attribute_count, 0);
            attribute_info_.at(j).ai_attribute_partition_channels_minus1.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_map_absolute_coding_persistence_flag.resize(ai_attribute_count, true);
            attribute_info_.at(j).ai_attribute_MSB_align_flag.resize(ai_attribute_count, 0);
            
            for (uint8_t i = 0; i < ai_attribute_count; ++i) {
                attribute_info_.at(j).ai_attribute_type_id.at(i) = readU(stream, 4, "ai_attribute_type_id",gofId);
                attribute_info_.at(j).ai_attribute_codec_id.at(i) = readU(stream, 8, "ai_attribute_codec_id",gofId);

                if (vps_auxiliary_video_present_flag_.at(0)) {
                    attribute_info_.at(j).ai_auxiliary_attribute_codec_id.at(i) = readU(stream, 8, "ai_auxiliary_attribute_codec_id",gofId);
                }
                if (vps_map_count_minus1_.at(j) > 0) {
                    attribute_info_.at(j).ai_attribute_map_absolute_coding_persistence_flag.at(i) = readU(stream, 1,
                            "ai_attribute_map_absolute_coding_persistence_flag",gofId) != 0U;
                }

                uint8_t d = readU(stream, 6, "ai_attribute_dimension_minus1",gofId);
                attribute_info_.at(j).ai_attribute_dimension_minus1.at(i) = d;

                if (attribute_info_.at(j).ai_attribute_dimension_minus1.at(i) > 0) {
                    attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.at(i) = readU(stream, 6, "ai_attribute_dimension_partitions_minus1.at(i)",gofId);
                    int32_t remainingDimensions = attribute_info_.at(j).ai_attribute_dimension_minus1.at(i);
                    int32_t k                   = attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.at(i);
                    attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).resize(k+1, 0);
                    for (int32_t jj = 0; jj < k; jj++) {
                        if (k - jj != remainingDimensions) {
                            attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(jj) = readUE(stream, "ai_attribute_partition_channels_minus1",gofId);
                        } // else -> attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(jj) = 0
                        remainingDimensions -= attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(jj) + 1;
                    }
                    attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = remainingDimensions;
                }
                attribute_info_.at(j).ai_attribute_2d_bit_depth_minus1.at(i) = readU(stream, 5, "ai_attribute_2d_bit_depth_minus1",gofId);
                attribute_info_.at(j).ai_attribute_MSB_align_flag.at(i) = readU(stream, 1, "ai_attribute_MSB_align_flag",gofId) != 0U;

                // uint8_t m = 0;
                // if (d == 0) {  // true
                //     // m = 0;
                //     attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.at(i) = 0;
                // } else {
                //     m = readU(stream, 6, "ai_attribute_dimension_partitions_minus1",gofId);
                //     attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.at(i) = m;
                // }
                // attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).resize(attribute_info_.at(j).ai_attribute_dimension_minus1.at(i));
                // uint16_t n = 0;
                // for (uint8_t k = 0; k < m; k++) {
                //     if (k + d == m) {
                //         n = 0;
                //         attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = 0;
                //     } else {
                //         n = readUE(stream, "ai_attribute_partition_channels_minus1",gofId);
                //         attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = n;
                //     }
                //     d -= n + 1;
                // }
                // attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(m) = d;
                // attribute_info_.at(j).ai_attribute_2d_bit_depth_minus1.at(i) = readU(stream, 5, "ai_attribute_2d_bit_depth_minus1",gofId);
                // attribute_info_.at(j).ai_attribute_MSB_align_flag.at(i) = readU(stream, 1, "ai_attribute_MSB_align_flag",gofId);
            }
        }
        vps_extension_present_flag_ = readU(stream, 1, "vps_extension_present_flag",gofId);
        if (vps_extension_present_flag_) {
            vps_extension_8bits_ = readU(stream, 8, "vps_extension_8bits_",gofId);
        }
        if (vps_extension_8bits_) {
            vps_extension_length_minus1_ = readUE(stream, "vps_extension_length_minus1_",gofId);
            vps_extension_data_byte_.resize(vps_extension_length_minus1_ + 1);
            for (size_t i = 0; i < vps_extension_length_minus1_ + 1; i++) {
                vps_extension_data_byte_.at(i) = readU(stream, 8, "vps_extension_data_byte_.at(i)",gofId);
            }
        }

        bitstream_align(stream); 
    }
    return true;
}