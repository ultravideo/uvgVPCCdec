#include "bitstreamParsing.hpp"
#include "bitstream_common.hpp"
#include <cstring>

static const uint8_t* cbuf_;
static bitstream_position pos_;

/* TODO: make sure this function works in all cases */
void BitstreamParsing::advance_bitstream(std::size_t bits)
{
    std::size_t bytes = bits / 8 + (pos_.bits + bits % 8) / 8;
    pos_.bytes += bytes;
    pos_.bits = (pos_.bits + bits % 8) % 8;
}

/* NOTE -------------------- Almost straight from TMC2 -------------------- */
uint32_t BitstreamParsing::read(uint8_t bits) {
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
    uint8_t v3c_size_precision_bytes = read(3) + 1;
    std::cout << "V3C size precision in bytes: " << uint32_t(v3c_size_precision_bytes) << std::endl;

    advance_bitstream(5);

    while (true) {
        if (pos_.bytes >= data.size()) {
            break;
        }
        std::size_t v3c_unit_size = read(16);

        // Inside v3c unit now
        std::cout << "Current V3C unit location " << pos_.bytes << ", size " << v3c_unit_size << std::endl;
        

        // Next 4 bytes are the V3C unit header
        uint8_t vuh_unit_type = read(5);
        std::cout << "-- vuh_unit_type: " << (uint32_t)vuh_unit_type << std::endl;
        advance_bitstream(4 * 8 - 5); // skip the reat of v3c header for now

        std::size_t v3c_unit_payload_size_bytes = v3c_unit_size - 4;
        switch(vuh_unit_type) {
            case V3C_UNIT_TYPE::V3C_VPS:
                readV3CParameterSet(v3c_unit_payload_size_bytes);
                break;
            case V3C_UNIT_TYPE::V3C_AD:
                readAtlasData(v3c_unit_payload_size_bytes);
                break;
            case V3C_UNIT_TYPE::V3C_OVD:
            case V3C_UNIT_TYPE::V3C_GVD:
            case V3C_UNIT_TYPE::V3C_AVD:
                readVideoData(v3c_unit_payload_size_bytes);
                break;
            default: 
                std::cout << "error" << std::endl;
                break;
        }
    }
    std::cout << "File parsed" << std::endl;
}

void BitstreamParsing::readV3CParameterSet(std::size_t v3c_payload_size_bytes)
{
    std::cout << "Reading V3C parameter set " << std::endl;
    advance_bitstream(v3c_payload_size_bytes * 8);
}

void BitstreamParsing::readAtlasData(std::size_t v3c_payload_size_bytes)
{
    std::cout << "Reading Atlas data " << std::endl;
    advance_bitstream(v3c_payload_size_bytes * 8);
}

void BitstreamParsing::readVideoData(std::size_t v3c_payload_size_bytes)
{
    std::cout << "Reading video data " << std::endl;
    advance_bitstream(v3c_payload_size_bytes * 8);
}