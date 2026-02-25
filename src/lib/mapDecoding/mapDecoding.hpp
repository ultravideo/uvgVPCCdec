/// \file Entry point for the map decoding process.

#include "uvgvpcc/uvgvpcc.hpp"

namespace MapDecoding {
void initializeStaticParameters();
void initializeDecoderPointers();
void decodeGOFMaps(const std::shared_ptr<uvgvpcc_dec::GOF>& gof);
}; // namespace MapDecoding