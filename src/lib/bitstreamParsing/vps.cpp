#include "vps.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>

#include "bitstream_util.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

// vps::vps(const uvgvpcc_dec::Parameters& paramUVG, const std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG) {
//     // size_t vps_length_bits = 0;
//     // ptl_ = fill_ptl(vps_length_bits);          // profile_tier_level
//     // gofId = gofUVG->gofId;                           // lf addition for exporting intermediate atlas information
//     // vps_v3c_parameter_set_id_ = gofUVG->gofId % 16;  // The value of vps_v3c_parameter_set_id shall be in the range of 0 to 15
//     // vps_atlas_count_minus1_ = 0;                     // for atlas count 1
//     // vps_length_bits += 18;                           // fixed fields
// }

bool vps::read_vps(bitstream_t* stream) {
    
    // profile_tier_level
    ptl_.ptl_profile_toolset_idc = readU(stream, 8, "ptl_profile_toolset_idc",gofId);
    ptl_.ptl_tier_flag = readU(stream, 1, "ptl_tier_flag",gofId);
    ptl_.ptl_profile_codec_group_idc = readU(stream, 7, "ptl_profile_codec_group_idc",gofId);
    // ptl_.ptl_profile_toolset_idc = readU(stream, 8, "ptl_profile_toolset_idc",gofId);
    ptl_.ptl_profile_reconstruction_idc = readU(stream, 8, "ptl_profile_reconstruction_idc",gofId);
    uint16_t ptl_reserved_zero_16bits = readU(stream, 16, "ptl_reserved_zero_16bits",gofId);
    ptl_.ptl_max_decodes_idc = readU(stream, 4, "ptl_max_decodes_idc",gofId);
    uint16_t ptl_reserved_0xfff_12bits =  readU(stream, 12, "ptl_reserved_0xfff_12bits",gofId);
    ptl_.ptl_level_idc = readU(stream, 8, "ptl_level_idc",gofId);
    ptl_.ptl_num_sub_profiles = readU(stream, 6, "ptl_num_sub_profiles",gofId);
    ptl_.ptl_extended_sub_profile_flag = readU(stream, 1, "ptl_extended_sub_profile_flag",gofId);
    ptl_.ptl_toolset_constraints_present_flag = readU(stream, 1, "ptl_toolset_constraints_present_flag",gofId);

    // profile_toolset_constraints_information
    // TODO(lf): Do we need profile_toolset_constraints_information? Not present from TMC2 interface

    vps_v3c_parameter_set_id_ = readU(stream, 4, "vps_v3c_parameter_set_id",gofId);
    uint8_t vps_reserved_zero_8bits = readU(stream, 8, "vps_reserved_zero_8bits",gofId);
    vps_atlas_count_minus1_ = readU(stream, 6, "vps_atlas_count_minus1",gofId);

    const size_t vps_atlas_count = vps_atlas_count_minus1_ + 1;

    /* Resize vectors */
    vps_atlas_id_.resize(vps_atlas_count);
    vps_frame_width_.resize(vps_atlas_count);
    vps_frame_height_.resize(vps_atlas_count);
    vps_map_count_minus1_.resize(vps_atlas_count);
    vps_multiple_map_streams_present_flag_.resize(vps_atlas_count);
    vps_map_absolute_coding_enabled_flag_.resize(vps_atlas_count);
    vps_auxiliary_video_present_flag_.resize(vps_atlas_count);
    vps_occupancy_video_present_flag_.resize(vps_atlas_count);
    vps_geometry_video_present_flag_.resize(vps_atlas_count);
    vps_attribute_video_present_flag_.resize(vps_atlas_count);

    occupancy_info_.resize(vps_atlas_count);
    geometry_info_.resize(vps_atlas_count);
    attribute_info_.resize(vps_atlas_count);

    for (uint8_t j = 0; j < vps_atlas_count; j++) {
        vps_atlas_id_.at(j) = readU(stream, 6, "vps_atlas_id",gofId);
        vps_frame_width_.at(j) = readUE(stream, "vps_frame_width",gofId);
        vps_frame_height_.at(j) = readUE(stream, "vps_frame_height",gofId);
        vps_map_count_minus1_.at(j) = readU(stream, 4, "vps_map_count_minus1",gofId);

        if (vps_map_count_minus1_.at(j) > 0) {
            vps_multiple_map_streams_present_flag_.at(j) = readU(stream, 1, "vps_multiple_map_streams_present_flag",gofId);
        }
        vps_map_absolute_coding_enabled_flag_.at(j).resize(vps_map_count_minus1_.at(j) + 1);
        for (uint8_t i = 1; i <= vps_map_count_minus1_.at(j); i++) {
            if(vps_multiple_map_streams_present_flag_.at(j)) {
                vps_map_absolute_coding_enabled_flag_.at(j).at(i) = readU(stream, 1, "vps_map_absolute_coding_enabled_flag", gofId);
            }
            else {
                vps_map_absolute_coding_enabled_flag_.at(j).at(i) = true;
            }
        }
        vps_auxiliary_video_present_flag_.at(j) = readU(stream, 1, "vps_auxiliary_video_present_flag",gofId);
        vps_occupancy_video_present_flag_.at(j) = readU(stream, 1, "vps_occupancy_video_present_flag",gofId);
        vps_geometry_video_present_flag_.at(j) = readU(stream, 1, "vps_geometry_video_present_flag",gofId);
        vps_attribute_video_present_flag_.at(j) = readU(stream, 1, "vps_attribute_video_present_flag",gofId);

        if (vps_occupancy_video_present_flag_.at(j)) {
            occupancy_info_.at(j).oi_occupancy_codec_id = readU(stream, 8, "oi_occupancy_codec_id",gofId);
            occupancy_info_.at(j).oi_lossy_occupancy_compression_threshold = readU(stream, 8, "oi_lossy_occupancy_compression_threshold",gofId);
            occupancy_info_.at(j).oi_occupancy_2d_bit_depth_minus1 = readU(stream, 5, "oi_occupancy_2d_bit_depth_minus1",gofId);
            occupancy_info_.at(j).oi_occupancy_MSB_align_flag = readU(stream, 1, "oi_occupancy_MSB_align_flag",gofId);
        }

        if (vps_geometry_video_present_flag_.at(j)) {
            geometry_info_.at(j).gi_geometry_codec_id = readU(stream, 8, "gi_geometry_codec_id",gofId);
            geometry_info_.at(j).gi_geometry_2d_bit_depth_minus1 = readU(stream, 5, "gi_geometry_2d_bit_depth_minus1",gofId);
            geometry_info_.at(j).gi_geometry_MSB_align_flag = readU(stream, 1, "gi_geometry_MSB_align_flag",gofId);
            geometry_info_.at(j).gi_geometry_3d_coordinates_bit_depth_minus1 = readU(stream, 5,
                    "gi_geometry_3d_coordinates_bit_depth_minus1",gofId);

            if (vps_auxiliary_video_present_flag_.at(j)) {
                geometry_info_.at(j).gi_auxiliary_geometry_codec_id = readU(stream, 8, "gi_auxiliary_geometry_codec_id",gofId);
            }
        }

        if (vps_attribute_video_present_flag_.at(j)) {
            const uint8_t ai_attribute_count = readU(stream, 7, "ai_attribute_count",gofId);
            attribute_info_.at(j).ai_attribute_count = ai_attribute_count;

            attribute_info_.at(j).ai_attribute_type_id.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_codec_id.resize(ai_attribute_count);
            attribute_info_.at(j).ai_auxiliary_attribute_codec_id.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_map_absolute_coding_persistence_flag.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_dimension_minus1.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_partition_channels_minus1.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_2d_bit_depth_minus1.resize(ai_attribute_count);
            attribute_info_.at(j).ai_attribute_MSB_align_flag.resize(ai_attribute_count);

            for (uint8_t i = 0; i < ai_attribute_count; ++i) {
                attribute_info_.at(j).ai_attribute_type_id.at(i) = readU(stream, 4, "ai_attribute_type_id",gofId);
                attribute_info_.at(j).ai_attribute_codec_id.at(i) = readU(stream, 8, "ai_attribute_codec_id",gofId);

                if (vps_auxiliary_video_present_flag_.at(j)) {
                    attribute_info_.at(j).ai_auxiliary_attribute_codec_id.at(i) = readU(stream, 8, "ai_auxiliary_attribute_codec_id",gofId);
                }
                if (vps_map_count_minus1_.at(j) > 0) {
                    attribute_info_.at(j).ai_attribute_map_absolute_coding_persistence_flag.at(i) = readU(stream, 1,
                            "ai_attribute_map_absolute_coding_persistence_flag",gofId);
                }

                uint8_t d = readU(stream, 6, "ai_attribute_dimension_minus1",gofId);
                attribute_info_.at(j).ai_attribute_dimension_minus1.at(i) = d;

                uint8_t m = 0;
                if (d == 0) {  // true
                    // m = 0;
                    attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.at(i) = 0;
                } else {
                    m = readU(stream, 6, "ai_attribute_dimension_partitions_minus1",gofId);
                    attribute_info_.at(j).ai_attribute_dimension_partitions_minus1.at(i) = m;
                }

                attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).resize(attribute_info_.at(j).ai_attribute_dimension_minus1.at(i));
                uint16_t n = 0;
                for (uint8_t k = 0; k < m; k++) {
                    if (k + d == m) {
                        n = 0;
                        attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = 0;
                    } else {
                        n = readUE(stream, "ai_attribute_partition_channels_minus1",gofId);
                        attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = n;
                    }
                    d -= n + 1;
                }

                attribute_info_.at(j).ai_attribute_partition_channels_minus1.at(i).at(m) = d;
                attribute_info_.at(j).ai_attribute_2d_bit_depth_minus1.at(i) = readU(stream, 5, "ai_attribute_2d_bit_depth_minus1",gofId);
                attribute_info_.at(j).ai_attribute_MSB_align_flag.at(i) = readU(stream, 1, "ai_attribute_MSB_align_flag",gofId);

            }
        }
        vps_extension_present_flag_ = readU(stream, 1, "vps_extension_present_flag",gofId);

        if (vps_extension_present_flag_) {
            vps_packing_information_present_flag_ = readU(stream, 1, "vps_packing_information_present_flag",gofId);
            vps_miv_extension_present_flag_ = readU(stream, 1, "vps_miv_extension_present_flag",gofId);
            vps_extension_6bits_ = readU(stream, 6, "vps_extension_6bits",gofId);
        }
        // No packing information
        // No MIV extension
        // No VPS extension
        bitstream_align(stream);
    }
    return true;
}