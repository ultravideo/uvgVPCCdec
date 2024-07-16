#include "reconstruction.hpp"

using namespace uvgvpcc_dec;

/*


a 4Darray occFramesNF[ compTimeIdx ][ 0 ][ y ][ x ] specifying the decoded occupancy frames in the
nominal format, where y is in the range of 0 to asps_frame_height – 1, inclusive, and x is in the range
of 0 to asps_frame_width – 1, inclusive

a 5D array geoFramesNF[ mapIdx ][ compTimeIdx ][ 0 ][ y ][ x ] specifying the decoded geometry
frames in the nominal format, where mapIdx is in the range of 0 to asps_map_count_minus1,
inclusive, y is in the range of 0 to asps_frame_height – 1, inclusive, and x is in the range of 0 to
asps_frame_width – 1, inclusive,
*/

void Reconstruction::reconstructPointCloud(decompressed_data* data)
{
    std::vector<picture> occFramesNF;
    std::vector<picture> geoFramesNF;
    std::vector<std::vector<picture>> attrFramesNF;
    auto i = data->asps.asps_frame_height;
}