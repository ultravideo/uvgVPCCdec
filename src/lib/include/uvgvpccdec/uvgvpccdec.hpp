#pragma once

#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"

namespace uvgvpcc_dec
{
    
struct Parameters {
    int hello = 0;
};

namespace API
{
    void initializeDecoder(const Parameters& param);
} // namespace API

} // namespace uvgvpcc_dec
