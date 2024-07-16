#pragma once

#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"
#include "atlas_data_structures.hpp"
#include "vps.hpp"

namespace uvgvpcc_dec
{
    
struct Parameters {
    int hello = 0;
};

struct decompressed_data { // of a gof currently
    v3c_parameter_set vps;
    atlas_sequence_parameter_set asps;
    atlas_frame_parameter_set afps;
    std::vector<atlas_tile_layer_rbsp> rbsp_vec = {};
    std::string occupancy_map_path;
    std::string geometry_map_path;
    std::string attribute_map_path;
};

namespace API
{
    void initializeDecoder(const Parameters& param);
    void decodeV3CSampleStream(const std::string filename);
} // namespace API

} // namespace uvgvpcc_dec
