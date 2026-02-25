#include "colorInversion.hpp"

#include "utils/parameters.hpp"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

using namespace uvgvpcc_dec;

ColorInversion::invertColors(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const uvgvpcc_dec::Parameters& param) {
    for(int i = 0; i < gofUVG->nbFrames; i++) {
        const auto& frame_L1 = gofUVG->frames[i];
    }
}