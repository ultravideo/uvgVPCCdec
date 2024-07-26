#include "uvgvpccdec/uvgvpccdec.hpp"
#include <iostream>

int main() {
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Decode .vpcc file \n");
    uvgvpcc_dec::Parameters param;
    uvgvpcc_dec::API::initializeDecoder(param);
    //uvgvpcc_dec::API::decodeV3CSampleStream("BITSTREAM-V3C.vpcc");
    uvgvpcc_dec::API::decodeV3CSampleStream("decoder-testing-3f.vpcc");
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Done \n");
    return 0;
}