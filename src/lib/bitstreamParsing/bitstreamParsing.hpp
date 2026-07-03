#pragma once

/// \file Entry point for the whole bitstream parsing process.

#include "gof.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

class BitstreamParsing {
   public:
   // static void parseV3CGOFBitstream(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const uvgvpcc_dec::Parameters& param,
   //                                  uvgvpcc_dec::API::v3c_unit_stream* input);

   static void parseV3CGOFBitstream(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, std::shared_ptr<v3c_gof> gof_, 
                                    const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::API::v3c_chunk& chunk);

   static void parseV3CGOFBitstream_parallel(std::shared_ptr<uvgvpcc_dec::GOF> gofUVG, std::shared_ptr<v3c_gof> gof_,
                                    const uvgvpcc_dec::Parameters& param, std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk);

   static void parseV3CGOFBitstream_separate_vuh_units(std::shared_ptr<uvgvpcc_dec::GOF> gofUVG, std::shared_ptr<v3c_gof> gof_,
                                    const uvgvpcc_dec::Parameters& param, std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk);
};