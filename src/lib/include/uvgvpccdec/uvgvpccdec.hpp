#pragma once

#include "uvgvpccdec/threadqueue.hpp"
#include "uvgvpccdec/log.hpp"
#include "data_structures.hpp"
#include <queue>
#include <semaphore>

namespace uvgvpcc_dec
{

/* 3.140 V3C composition unit
set of all sub-bitstream composition units (3.117) that share the same composition time (3.50), where one
of the sub-bitstream composition units (3.117) is a coded atlas access unit (3.35)

   3.50 composition time
time or time period at which a frame needs to be composed, used for reconstruction, or presented */
struct composition_unit_boundary {
    size_t ad_start = 0;
    size_t ad_size = 0;

    size_t ovd_start = 0;
    size_t ovd_size = 0;

    size_t gvd_start = 0;
    size_t gvd_size = 0;

    size_t avd_start = 0;
    size_t avd_size = 0;
};

struct gof_info {

    size_t vps_start = 0;
    size_t vps_size = 0;
    size_t cu_count = 0;
    std::vector<composition_unit_boundary> composition_unit_boundaries;
};

struct Parameters {
    int hello = 0;

    size_t max_points = 0;
    bool keep_intermediate_files = false;
    bool fast_color_conversion = false;

    bool useTMC2AttributeYUVConversion=true;
    bool nbThread=20;

};

struct v3c_chunk {
    size_t len = 0; // Length of data in buffer
    std::vector<uint8_t> data = {}; // Actual data
    std::vector<size_t> v3c_unit_sizes = {};
    //~v3c_chunk() { delete[] data;}
};

struct GOF {
    std::vector<std::shared_ptr<uvgvpcc_dec::Job>> out_jobs;
    std::vector<std::shared_ptr<uvgvpcc_dec::Job>> patch_jobs;
    std::shared_ptr<v3c_chunk> raw_chunk;
    std::shared_ptr<uint8_t*> buffer;
    std::shared_ptr<gof_info> raw_gof;
    std::shared_ptr<decompressed_gof> decoded_gof;
    std::shared_ptr<point_cloud_frame> reconstructed_point_cloud;
};

struct context{
    const Parameters* p_;
    std::shared_ptr<ThreadQueue> queue;
    std::shared_ptr<GOF> latest_gof;
    std::vector<gof_info> raw_gofs;
    std::vector<std::shared_ptr<GOF>> gofs;
};



namespace API
{

/* A V3C unit stream is composed of only V3C units without parsing information in the bitstream itself.
    The parsing information is here given separately. */
struct v3c_unit_stream {
    size_t v3c_unit_size_precision_bytes = 0;
    std::queue <std::shared_ptr<v3c_chunk>> v3c_chunks = {};
};

struct decoded_output {
    std::queue <std::shared_ptr<point_cloud_frame>> frames = {};
    //~v3c_unit_stream() { v3c_chunks.clear();}

    std::counting_semaphore<> available_frames{0};
    std::mutex io_mutex; // Locks production and consumption in the frames queue
};

    void initializeDecoder(const Parameters& param);
    void decodeV3CChunk(std::shared_ptr<v3c_chunk> chunk, uvgvpcc_dec::API::decoded_output* out);
    void decodeV3CChunk_serial(std::shared_ptr<v3c_chunk> chunk, uvgvpcc_dec::API::decoded_output* out);
    void emptyFrameQueue();
} // namespace API

} // namespace uvgvpcc_dec
