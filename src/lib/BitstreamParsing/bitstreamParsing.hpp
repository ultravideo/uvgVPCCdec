#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"

struct bitstream_position {
  uint64_t bytes = 0;
  uint8_t  bits = 0;
};

class BitstreamParsing {
public: 
    static uint32_t read(uint8_t bits);
    static void advance_bitstream(std::size_t bits);
    static void initializeStaticParameters(const uvgvpcc_dec::Parameters& param);
    static void parseV3CSampleStream(const std::vector<uint8_t> &data);
};
