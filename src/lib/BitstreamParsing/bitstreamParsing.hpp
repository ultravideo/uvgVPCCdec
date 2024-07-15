#pragma once

#include "uvgvpccdec/uvgvpccdec.hpp"


class BitstreamParsing {
   public:
    static void initializeStaticParameters(const uvgvpcc_dec::Parameters& param);
    static void parseV3CSampleStream(const std::vector<uint8_t> &data);
};
