#include "postReconstruction.hpp"
#include "Decompression/decompression.hpp"

using namespace uvgvpcc_dec;

void PostReconstruction::PostProcess(decompressed_cu* cu, point_cloud_frame* reconstruct, const size_t cu_frame_index, const size_t gof_index)
{
    Logger::log(LogLevel::TRACE, "Post-reconstruction", "Post-processing point cloud frame " + std::to_string(reconstruct->frame_index_in_gof)
        + " (in composition unit " + std::to_string(cu->cu_index)
        + ") in GOF " + std::to_string(gof_index) + " \n");

    const v3c_parameter_set &vps = Decompression::get_saved_params(gof_index).vps;
    color_point_cloud(reconstruct, *cu, cu_frame_index, gof_index);
}

void PostReconstruction::color_point_cloud(point_cloud_frame* reconstruct, const decompressed_cu &cu, const size_t cu_frame_index,
    const size_t gof_index )
{
    const size_t atlas_index = cu.atlas_map.at(cu_frame_index).get()->atlas_index;

    const video_map &videoAttributeMap0 = cu.attribute_maps.at(0);

    const v3c_parameter_set &vps = Decompression::get_saved_params(gof_index).vps;
    bool multipleStreams = vps.vps_multiple_map_streams_present_flag.at(0);
    uint8_t attributeCount = vps.ai_attribute_count;

    if ( attributeCount == 0 ) {
        Logger::log(LogLevel::INFO, "Post-reconstruction", "No attribute data \n");
        return;
    }
    const size_t mapCount = vps.vps_map_count_minus1.at(atlas_index) + 1;

    std::vector<point3d> &pointToPixel = reconstruct->point_to_pixel;
    auto&  color8bit = reconstruct->colors;
    color8bit.resize(reconstruct->getPointCount());
    size_t pointCount = reconstruct->getPointCount();
    
    /*printf( "pointCount                   = %zu \n", pointCount );
    printf( "pointToPixel size            = %zu \n", pointToPixel.size() );
    printf( "multipleStreams              = %zu \n", multipleStreams );
    printf( "attributeCount               = %zu \n", (size_t)attributeCount );
    printf( "mapCount            = %zu \n", (size_t)mapCount );*/

    const size_t shift = multipleStreams ? cu_frame_index : cu_frame_index * mapCount;
    for ( size_t i = 0; i < pointCount; ++i ) {
        const point3d &location = pointToPixel[i];
        const size_t x = location.data_[0];
        const size_t y = location.data_[1];
        const size_t f = location.data_[2];

        if ( f < mapCount ) {
            const picture &frame = videoAttributeMap0.pictures.at(shift + f);
            for ( size_t c = 0; c < 3; ++c ) {
                color8bit.at(i).data_[c] = frame.get_value(c, x, y);
            }

        }
    }
}