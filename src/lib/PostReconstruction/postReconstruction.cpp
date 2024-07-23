#include "postReconstruction.hpp"

using namespace uvgvpcc_dec;

/* ------------------------ ripped from tmc2------------------------ */
void PostReconstruction::PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, std::vector<uint32_t>& partition)
{
    Logger::log(LogLevel::INFO, "Post-reconstruction", "Post-processing point cloud \n");
    int b = data->frame_count;
    int a = reconstruct->getPointCount();
    auto c = partition.front();
}