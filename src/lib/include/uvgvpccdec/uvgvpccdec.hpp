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

struct decoded_output {
    std::queue <std::shared_ptr<point_cloud_frame>> frames = {};
    //~v3c_unit_stream() { v3c_chunks.clear();}

    std::counting_semaphore<> available_frames{0};
    std::mutex io_mutex; // Locks production and consumption in the frames queue
};

    void initializeDecoder(const Parameters& param);
    void decodeV3CSampleStream(std::vector<uint8_t> &data);
    void decodeV3CChunk(v3c_chunk &chunk, uvgvpcc_dec::API::decoded_output* out);
} // namespace API

} // namespace uvgvpcc_dec
