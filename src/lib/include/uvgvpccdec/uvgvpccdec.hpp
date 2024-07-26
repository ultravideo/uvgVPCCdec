#pragma once

#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"
#include "data_structures.hpp"

namespace uvgvpcc_dec
{

struct Parameters {
    int hello = 0;
};

// HEVC NAL units (intended for VPS, SPS or PPS)
struct video_parameter_set_nalu {
    size_t hevc_nal_type = 0;
    size_t len = 0;
    std::unique_ptr<uint8_t[]> data = nullptr;
};

// When the video parameter set NAL units are read, save them for possible later use (Low-delay mode)
struct video_parameter_set_nals {
    std::vector<video_parameter_set_nalu> occupancy_parameters = {};
    std::vector<video_parameter_set_nalu> geometry_parameters = {};
    std::vector<video_parameter_set_nalu> attribute_parameters = {};
};

namespace API
{
    void initializeDecoder(const Parameters& param);
    void decodeV3CSampleStream(const std::string filename);
} // namespace API

} // namespace uvgvpcc_dec
