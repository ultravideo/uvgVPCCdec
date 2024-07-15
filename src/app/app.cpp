#include "uvgvpccdec/uvgvpccdec.hpp"
#include <iostream>

int main() {
    std::cout << "Hello, World!" << std::endl;
    uvgvpcc_dec::Parameters param;
    uvgvpcc_dec::API::initializeDecoder(param);
    uvgvpcc_dec::API::decodeV3CSampleStream("BITSTREAM-V3C.vpcc");
    return 0;
}