#include "video_sub_bitstream.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <new>
#include <string>
#include <vector>

namespace {

size_t combine_bytes(const uint8_t *bytes, size_t len) {
    uint8_t *buf = new uint8_t[len];
    memcpy(buf, bytes, len);
    size_t result = 0;
    for (size_t i = 0; i < len; ++i) {
        result |= static_cast<size_t>(buf[i]) << (8 * (len - 1 - i));
    }
    delete[] buf;
    return result;
}
}  // anonymous namespace

void find_nals(std::vector<uint8_t> &input_data, std::vector<nal_info> &nals) {
    const uint8_t *buf = input_data.data();
    size_t ptr = 0;
    while (ptr < input_data.size()) {
        const size_t nal_size = combine_bytes(&buf[ptr], 4);

        // const uint8_t nal_type = input_data[ptr + 4] >> 1;
        // std::cout << "-- NAL size: " << nal_size << ", type: " << (uint32_t)nal_type << std::endl;

        nal_info current;
        current.location = ptr + 4;
        current.size = nal_size;
        // std::cout << "uvg: location " << current.location << ", size " << current.size << std::endl;
        nals.push_back(current);
        ptr += 4 + nal_size;
    }
}