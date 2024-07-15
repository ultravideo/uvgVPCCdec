#include "bitstreamParsing.hpp"
#include "bitstream_common.hpp"
#include <cstring>

/* In debug mode print out some extra info */
#define BITSTREAM_DEBUG false

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
    code = read( 1 );
    if ( 0 == code ) {
      length = 0;
      while ( !( code & 1 ) ) {
        code = read( 1 );
        length++;
      }
      value = read( length );
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

    vps.ptl.ptl_tier_flag = read(1, "ptl_tier_flag");
    vps.ptl.ptl_profile_codec_group_idc = read(7, "ptl_profile_codec_group_idc");
    vps.ptl.ptl_profile_toolset_idc = read(8, "ptl_profile_toolset_idc");
    vps.ptl.ptl_profile_reconstruction_idc = read(8, "ptl_profile_reconstruction_idc");
    uint16_t ptl_reserved_zero_16bits = read(16, "ptl_reserved_zero_16bits");
    vps.ptl.ptl_max_decodes_idc = read(4, "ptl_max_decodes_idc");
    uint16_t ptl_reserved_0xfff_12bits = read(12, "ptl_reserved_0xfff_12bits");
    vps.ptl.ptl_level_idc = read(8, "ptl_level_idc");
    vps.ptl.ptl_num_sub_profiles = read(6, "ptl_num_sub_profiles");
    vps.ptl.ptl_extended_sub_profile_flag = read(1, "ptl_extended_sub_profile_flag");
    vps.ptl.ptl_toolset_constraints_present_flag = read(1, "ptl_toolset_constraints_present_flag");
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