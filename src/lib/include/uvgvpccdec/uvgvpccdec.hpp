#pragma once

#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"
#include "data_structures.hpp"

namespace uvgvpcc_dec
{
    
struct Parameters {
    int hello = 0;
};

namespace API
{
    void initializeDecoder(const Parameters& param);
    void decodeV3CSampleStream(const std::string filename);
} // namespace API

} // namespace uvgvpcc_dec
