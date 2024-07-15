#include "bitstreamParsing.hpp"
#include "bitstream_common.hpp"
#include <cstring>

/* In debug mode print out some extra info */
#define BITSTREAM_DEBUG true

static const uint8_t* cbuf_;
static bitstream_position pos_;

/* TODO: make sure this function works in all cases */
void BitstreamParsing::advance_bitstream(std::size_t bits)
{
    std::size_t bytes = bits / 8 + (pos_.bits + bits % 8) / 8;
    pos_.bytes += bytes;
    pos_.bits = (pos_.bits + bits % 8) % 8;
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

void BitstreamParsing::parseV3CSampleStream(const std::vector<uint8_t> &data)
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
                read_v3c_parameter_set(v3c_unit_payload_size_bytes);
                break;
            case V3C_UNIT_TYPE::V3C_AD:
                read_atlas_sub_bitstream(v3c_unit_payload_size_bytes);
                break;
            case V3C_UNIT_TYPE::V3C_OVD:
            case V3C_UNIT_TYPE::V3C_GVD:
            case V3C_UNIT_TYPE::V3C_AVD:
                read_video_sub_bitstream(v3c_unit_payload_size_bytes);
                break;
            default: 
                std::cout << "error" << std::endl;
                break;
        }
        break;
    }
    std::cout << "File parsed" << std::endl;
}

void BitstreamParsing::read_v3c_parameter_set(std::size_t v3c_payload_size_bytes)
{
    std::cout << "Reading V3C parameter set, len " << v3c_payload_size_bytes << std::endl;
    //advance_bitstream(v3c_payload_size_bytes * 8); ----------------------------

    // profile_tier_level
    v3c_parameter_set vps;
    read_profile_tier_level(vps.ptl);
    
    vps.vps_v3c_parameter_set_id = read(4, "vps_v3c_parameter_set_id");
    uint8_t vps_reserved_zero_8bits = read(8, "vps_reserved_zero_8bits");
    vps.vps_atlas_count_minus1 = read(6, "vps_atlas_count_minus1");

    /* Resize vectors */
    vps.vps_atlas_id.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_frame_width.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_frame_height.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_map_count_minus1.resize(vps.vps_atlas_count_minus1 + 1);

    vps.vps_multiple_map_streams_present_flag.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_auxiliary_video_present_flag.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_occupancy_video_present_flag.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_geometry_video_present_flag.resize(vps.vps_atlas_count_minus1 + 1);
    vps.vps_attribute_video_present_flag.resize(vps.vps_atlas_count_minus1 + 1);

    vps.occupancy_info.resize(vps.vps_atlas_count_minus1 + 1);
    vps.geometry_info.resize(vps.vps_atlas_count_minus1 + 1);
    vps.attribute_info.resize(vps.vps_atlas_count_minus1 + 1);

    for (uint8_t j = 0; j < (vps.vps_atlas_count_minus1 + 1); j++) {
        vps.vps_atlas_id.at(j) = read(6, "vps_atlas_id");
        vps.vps_frame_width.at(j) = read_ue("vps_frame_width");
        vps.vps_frame_height.at(j) = read_ue("vps_frame_height");
        vps.vps_map_count_minus1.at(j) = read(4, "vps_map_count_minus1");

        if (vps.vps_map_count_minus1.at(j) > 0) {
            vps.vps_multiple_map_streams_present_flag.at(j) = read(1, "vps_multiple_map_streams_present_flag");
        }
        vps.vps_auxiliary_video_present_flag.at(j) = read(1, "vps_auxiliary_video_present_flag");
        vps.vps_occupancy_video_present_flag.at(j) = read(1, "vps_occupancy_video_present_flag");
        vps.vps_geometry_video_present_flag.at(j) = read(1, "vps_geometry_video_present_flag");
        vps.vps_attribute_video_present_flag.at(j) = read(1, "vps_attribute_video_present_flag");

        if (vps.vps_occupancy_video_present_flag.at(j)) {
            vps.occupancy_info.at(j).oi_occupancy_codec_id = read(8, "oi_occupancy_codec_id");
            vps.occupancy_info.at(j).oi_lossy_occupancy_compression_threshold = read(8, "oi_lossy_occupancy_compression_threshold");
            vps.occupancy_info.at(j).oi_occupancy_2d_bit_depth_minus1 = read(5, "oi_occupancy_2d_bit_depth_minus1");
            vps.occupancy_info.at(j).oi_occupancy_MSB_align_flag = read(1, "oi_occupancy_MSB_align_flag");
        }
        
        if (vps.vps_geometry_video_present_flag.at(j)) {
            vps.geometry_info.at(j).gi_geometry_codec_id = read(8, "gi_geometry_codec_id");
            vps.geometry_info.at(j).gi_geometry_2d_bit_depth_minus1 = read(5, "gi_geometry_2d_bit_depth_minus1");
            vps.geometry_info.at(j).gi_geometry_MSB_align_flag = read(1, "gi_geometry_MSB_align_flag");
            vps.geometry_info.at(j).gi_geometry_3d_coordinates_bit_depth_minus1 = read(5, "gi_geometry_3d_coordinates_bit_depth_minus1");
            
            if (vps.vps_auxiliary_video_present_flag.at(j)) {
                vps.geometry_info.at(j).gi_auxiliary_geometry_codec_id = read(8, "gi_auxiliary_geometry_codec_id");
            }
        }

        if (vps.vps_attribute_video_present_flag.at(j)) {
            vps.attribute_info.at(j).ai_attribute_count = read(7, "ai_attribute_count");

            vps.attribute_info.at(j).ai_attribute_type_id.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_codec_id.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_auxiliary_attribute_codec_id.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_map_absolute_coding_persistence_flag.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_dimension_minus1.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_dimension_partitions_minus1.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_partition_channels_minus1.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_2d_bit_depth_minus1.resize(vps.attribute_info.at(j).ai_attribute_count);
            vps.attribute_info.at(j).ai_attribute_MSB_align_flag.resize(vps.attribute_info.at(j).ai_attribute_count);


            for (uint8_t i = 0; i < vps.attribute_info.at(j).ai_attribute_count; ++i) {
                vps.attribute_info.at(j).ai_attribute_type_id.at(i) = read(4, "ai_attribute_type_id");
                vps.attribute_info.at(j).ai_attribute_codec_id.at(i) = read(8, "ai_attribute_codec_id");

                if(vps.vps_auxiliary_video_present_flag.at(j)) {
                    vps.attribute_info.at(j).ai_auxiliary_attribute_codec_id.at(i) = read(8, "ai_auxiliary_attribute_codec_id");
                }
                if(vps.vps_map_count_minus1.at(j) > 0) {
                    vps.attribute_info.at(j).ai_attribute_map_absolute_coding_persistence_flag.at(i) = read(1, "ai_attribute_map_absolute_coding_persistence_flag");
                }
                
                vps.attribute_info.at(j).ai_attribute_dimension_minus1.at(i) = read(6, "ai_attribute_dimension_minus1");
                uint8_t d = vps.attribute_info.at(j).ai_attribute_dimension_minus1.at(i);

                uint8_t m;
                if(d == 0) {
                    m = 0;
                    vps.attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i) = 0;
                }
                else {
                    vps.attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i) = read(6, "ai_attribute_dimension_partitions_minus1");
                    m = vps.attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i);
                }
                vps.attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).resize(vps.attribute_info.at(j).ai_attribute_dimension_minus1.at(i));
                uint16_t n = 0;
                for (uint8_t k = 0; k < m; k++) {
                    if(k + d == m) {
                        n = 0;
                        vps.attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = 0;
                    }
                    else {
                        vps.attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = read_ue("ai_attribute_partition_channels_minus1");
                        n = vps.attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k);
                    }
                    d -= n + 1;
                }
                vps.attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(m) = d;
                vps.attribute_info.at(j).ai_attribute_2d_bit_depth_minus1.at(i) = read(5, "ai_attribute_2d_bit_depth_minus1");
                vps.attribute_info.at(j).ai_attribute_MSB_align_flag.at(i) = read(1, "ai_attribute_MSB_align_flag");
            }
        }
        vps.vps_extension_present_flag = read(1, "vps_extension_present_flag");

        if(vps.vps_extension_present_flag) {
            vps.vps_packing_information_present_flag = read(1, "vps_packing_information_present_flag");
            vps.vps_miv_extension_present_flag = read(1, "vps_miv_extension_present_flag");
            vps.vps_extension_6bits = read(6, "vps_extension_6bits");
        }
        // No packing information
        // No MIV extension
        // No VPS extension
        // TODO: Do we need to align at all when reading?
        //uvg_bitstream_align(stream);
    }
}

void BitstreamParsing::read_profile_tier_level(profile_tier_level &ptl)
{
    ptl.ptl_profile_toolset_idc = read(8, "ptl_profile_toolset_idc");
    ptl.ptl_tier_flag = read(1, "ptl_tier_flag");
    ptl.ptl_profile_codec_group_idc = read(7, "ptl_profile_codec_group_idc");
    ptl.ptl_profile_reconstruction_idc = read(8, "ptl_profile_reconstruction_idc");
    uint16_t ptl_reserved_zero_16bits = read(16, "ptl_reserved_zero_16bits");
    ptl.ptl_max_decodes_idc = read(4, "ptl_max_decodes_idc");
    uint16_t ptl_reserved_0xfff_12bits = read(12, "ptl_reserved_0xfff_12bits");
    ptl.ptl_level_idc = read(8, "ptl_level_idc");
    ptl.ptl_num_sub_profiles = read(6, "ptl_num_sub_profiles");
    ptl.ptl_extended_sub_profile_flag = read(1, "ptl_extended_sub_profile_flag");
    ptl.ptl_toolset_constraints_present_flag = read(1, "ptl_toolset_constraints_present_flag");

    if(ptl.ptl_toolset_constraints_present_flag) {
        std::cout << "ERROR CANT HANDLE PTC" << std::endl;
        return;
    }
    // TODO: Parse PTC
}


void BitstreamParsing::read_atlas_sub_bitstream(std::size_t v3c_payload_size_bytes)
{
    std::cout << "Reading atlas data " << v3c_payload_size_bytes << std::endl;
    advance_bitstream(v3c_payload_size_bytes * 8);
}

void BitstreamParsing::read_video_sub_bitstream(std::size_t v3c_payload_size_bytes)
{
    std::cout << "Reading video data " << v3c_payload_size_bytes << std::endl;
    advance_bitstream(v3c_payload_size_bytes * 8);
}