#include "bitstream_util.hpp"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>

#include "utils/parameters.hpp"
#include "utils/fileExport.hpp"

bool bitsream_byte_aligned(bitstream_t* stream) {
    return (stream->cur_bit == 0);
};

void bitstream_advance(bitstream_t* stream, size_t bits) {
    // for ( std::size_t i = 0; i < bits; i++ ) {
    //     if ( stream->cur_bit == 7 ) {
    //         stream->len++;
    //         stream->cur_bit = 0;
    //     } else {
    //         stream->cur_bit++;
    //     }
    // }

    // for ( std::size_t i = 0; i < bits; i++ ) {
    //     stream->cur_bit++;
    //     if ( stream->cur_bit == 8 ) {
    //         stream->len++;
    //         stream->cur_bit = 0;
    //     } 
    // }

    size_t total_bits = stream->cur_bit + bits;
    stream->len += total_bits >> 3U;    // divide by 8, pos.bytes += total_bits >> 3U;
    stream->cur_bit = total_bits & 7U;  // modulo 8, pos.bits = total_bits & 7U;
}


void bitstream_align_rbsp_trailing_bits(bitstream_t* stream) {
    bitstream_advance(stream, 1);
    uint8_t needed_bits_to_zero = (8U - stream->cur_bit) & 7U;            
    bitstream_advance(stream, needed_bits_to_zero);
}

// 8.3.3 Byte alignment syntax
void bitstream_align(bitstream_t* stream) {
    // bitstream_advance(stream, 1);
    // while (!(stream->cur_bit == 0)) {
    //     bitstream_advance(stream, 1);
    // }
    const uint32_t bits_to_next_byte = 8U - stream->cur_bit;
    bitstream_advance(stream, bits_to_next_byte);
}

uint32_t bitstream_read(bitstream_t* stream, uint8_t bits) {
    /* Based on this:
    uint32_t value = 0;
    for ( std::size_t i = 0; i < bits; i++ ) {
      value |= ( ( cbuf_[pos.bytes] >> ( 7 - pos.bits ) ) & 1 ) << ( bits - 1 - i );
      if ( pos.bits == 7 ) {
        pos.bytes++;
        pos.bits = 0;
      } else {
        pos.bits++;
      }
    }
    return value; */

    uint32_t value = 0;
    uint64_t bytes = stream->len;
    uint8_t  cur_bits = stream->cur_bit;
    for (uint8_t i = 0; i < bits; i++) {
        value = (value << 1) | ((stream->data[bytes] >> (7 - cur_bits)) & 1);
        // value = ((stream->data[bytes] >> (7 - cur_bits)) & 1 ) << (bits - 1 - i); // tmc2
        cur_bits++;
        if (cur_bits == 8) {
            cur_bits = 0;
            bytes++;
        } 
    }

    stream->len = bytes;
    stream->cur_bit = cur_bits;
    return value;
}

uint32_t bitstream_read_no_advance(bitstream_t* stream, uint8_t bits) {
    uint32_t value = 0;
    uint64_t bytes = stream->len;
    uint8_t  cur_bits = stream->cur_bit;
    for (uint8_t i = 0; i < bits; i++) {
        value = (value << 1) | ((stream->data[bytes] >> (7 - cur_bits)) & 1);
        cur_bits++;
        if (cur_bits == 8) {
            cur_bits = 0;
            bytes++;
        } 
    }
    return value;
}

uint32_t bitstream_read_unit_header(std::vector<uint8_t>& data, uint8_t bits) {
    uint32_t value = 0;
    uint64_t bytes = 0;
    uint8_t  cur_bits = 0;
    for (uint8_t i = 0; i < bits; i++) {
        value = (value << 1) | ((data[bytes] >> (7 - cur_bits)) & 1);
        cur_bits++;
        if (cur_bits == 8) {
            cur_bits = 0;
            bytes++;
        } 
    }
    return value;
}

size_t bitstream_read_size_from_poiter(const uint8_t* src, size_t len) {
    size_t value = 0;
    // for (size_t i = 0; i < len; ++i) {
    //     value |= static_cast<size_t>(src[i]) << (8 * (len - 1 - i));
    // }
    while (len--) {
        value = (value << 8) | *src++;
    }
    return value;
}

void bitstream_copy_bytes(uint8_t* dest, const uint8_t* bytes, const size_t len) {
    memcpy(dest, bytes, len);
}

bool moreRbspData(bitstream_t* stream) {
    // Return false if there is no more data.
    // printf("----------------------> moreData, data_.size() = %d, len = %d\n", (int)stream->data.size(), stream->len);
    if (!(stream->len < stream->data.size())) {
        return false;
    }

    // Store bitstream state.
    auto cur_bit = stream->cur_bit;
    auto len = stream->len;

    // Skip first bit. It may be part of a RBSP or a rbsp_one_stop_bit.
    bitstream_advance(stream, 1);

    while (stream->len < stream->data.size()) {
        // printf("----------------------> moreData, data_.size() = %d, len = %d\n", (int)stream->data.size(), stream->len);
        if (bitstream_read(stream, 1)) {
            // We found a one bit beyond the first bit. Restore bitstream state and return true.
            stream->cur_bit = cur_bit;
            stream->len = len;
            return true;
        }
    }

    // We did not found a one bit beyond the first bit. Restore bitstream state and return false.
    stream->cur_bit = cur_bit;
    stream->len = len;
    return false;
}

uint32_t bitstream_read_ue(bitstream_t* stream) {
    /* Based on this:
    uint32_t value = 0;
    uint32_t code = 0;
    uint32_t length = 0;

    code = bitstream_read(stream, 1, pos);
    if (code == 0) {
        while(!(code & 1)) {
            code = bitstream_read(stream, 1, pos);
            length++;
        }
        value = bitstream_read(stream, length, pos);
        value += (1 << length) - 1;
    }
    return value; */

    uint32_t value = 0;
    uint32_t prefix = 0;

    while (bitstream_read(stream, 1) == 0) {
        prefix++;
    }

    if (prefix != 0) {
        value = bitstream_read(stream, prefix);
        value += (1u << prefix) - 1u;
    }

    return value;
}

int32_t bitstream_read_se(bitstream_t* stream) {
    uint32_t unsgined_bits = bitstream_read_ue(stream);
    int32_t signed_bits = ( int32_t )( unsgined_bits >> 1 );
    return ( unsgined_bits & 1 ) ? signed_bits + 1 : -signed_bits;
}

uint32_t readSE(bitstream_t* stream, std::string name, size_t gofId) {
    uint32_t value = bitstream_read_se(stream);
    // printf("readSE: %s, value: %d\n", name.c_str(), (int)value);
    (void)name; // Suppress unused parameter warning for now
    (void)gofId;
    // if(uvgvpcc_dec::p_->exportIntermediateFiles) {
    //     std::ostringstream oss;
    //     oss << std::left << std::setw(50) << name
    //     << " ue(v): " << value;
    //     std::string logLine = oss.str();
    //     FileExport::exportAtlasInformation(gofId,logLine);
    // }
    return value;
}

uint32_t readU(bitstream_t* stream, uint8_t bits, std::string name, size_t gofId) {
    uint32_t value = bitstream_read(stream, bits); 
    // printf("readU: %s, bits: %d, value: %d\n", name.c_str(), (int)bits, (int)value);
    (void)name; // Suppress unused parameter warning for now
    (void)gofId;
    // if(uvgvpcc_dec::p_->exportIntermediateFiles) {
    //     std::ostringstream oss;
    //     oss << std::left << std::setw(50) << name
    //         << " u(" << bits << ") : " << value;
    //     std::string logLine = oss.str();
    //     FileExport::exportAtlasInformation(gofId,logLine);
    // }
    return value;
}

uint32_t readUE(bitstream_t* stream, std::string name, size_t gofId) {
    uint32_t value = bitstream_read_ue(stream);
    // printf("readUE: %s, value: %d\n", name.c_str(), (int)value);
    (void)name; // Suppress unused parameter warning for now
    (void)gofId;
    // if(uvgvpcc_dec::p_->exportIntermediateFiles) {
    //     std::ostringstream oss;
    //     oss << std::left << std::setw(50) << name
    //     << " ue(v): " << value;
    //     std::string logLine = oss.str();
    //     FileExport::exportAtlasInformation(gofId,logLine);
    // }
    return value;
}




