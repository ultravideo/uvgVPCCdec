#pragma once

#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"
#include "data_structures.hpp"
#include <queue>
#include <semaphore>

namespace uvgvpcc_dec
{

struct gof_info {
    size_t vps_start = 0;
    size_t vps_size = 0;

    size_t ad_start = 0;
    size_t ad_size = 0;

    size_t ovd_start = 0;
    size_t ovd_size = 0;

    size_t gvd_start = 0;
    size_t gvd_size = 0;

    size_t avd_start = 0;
    size_t avd_size = 0;
};

struct Parameters {
    int hello = 0;
    size_t occupancy_width = 0;
    size_t occupancy_height = 0;

    // Geometry and attribute videos
    size_t video_width = 0;
    size_t video_height = 0;

    size_t max_points = 0;
    bool keep_intermediate_files = false;

};

struct context{
    const Parameters* p_;
    std::shared_ptr<ThreadQueue> queue;
};

// HEVC NAL units (intended for VPS, SPS or PPS)
/*struct video_parameter_set_nalu {
    size_t hevc_nal_type = 0;
    size_t len = 0;
    std::unique_ptr<uint8_t[]> data = nullptr;
};

// When the video parameter set NAL units are read, save them for possible later use (Low-delay mode)
struct video_parameter_set_nals {
    std::vector<video_parameter_set_nalu> occupancy_parameters = {};
    std::vector<video_parameter_set_nalu> geometry_parameters = {};
    std::vector<video_parameter_set_nalu> attribute_parameters = {};
};*/

namespace API
{

    struct v3c_chunk {
        size_t len = 0; // Length of data in buffer
        std::vector<uint8_t> data = {}; // Actual data
        std::vector<size_t> v3c_unit_sizes = {};
        //~v3c_chunk() { delete[] data;}
    };
    /* A V3C unit stream is composed of only V3C units without parsing information in the bitstream itself.
        The parsing information is here given separately. */
    struct v3c_unit_stream {
        size_t v3c_unit_size_precision_bytes = 0;
        std::queue <v3c_chunk> v3c_chunks = {};
        //~v3c_unit_stream() { v3c_chunks.clear();}
        std::mutex io_mutex; // Locks production and consumption in the v3c_chunks queue
    };

    void initializeDecoder(const Parameters& param);
    void decodeV3CSampleStream(std::vector<uint8_t> &data);
    void decodeV3CChunk(v3c_chunk &chunk);
} // namespace API

} // namespace uvgvpcc_dec
