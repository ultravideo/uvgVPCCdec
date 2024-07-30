#include "postReconstruction.hpp"
#include "Decompression/decompression.hpp"

using namespace uvgvpcc_dec;

void PostReconstruction::PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, const size_t frame_index)
{
    Logger::log(LogLevel::TRACE, "Post-reconstruction", "Post-processing point cloud frame " + std::to_string(frame_index) + " \n");
    const v3c_parameter_set &vps = Decompression::get_saved_params(data->gof_index).vps;
    bool multipleStreams = vps.vps_multiple_map_streams_present_flag.at(0);
    uint8_t attributeCount = vps.ai_attribute_count;
    color_point_cloud(reconstruct, data, frame_index, multipleStreams, attributeCount);
}

size_t PostReconstruction::color_point_cloud(point_cloud_frame* reconstruct, decompressed_data* data, const size_t frame_index, const size_t multipleStreams, const uint8_t attributeCount )
{
    atlas_frame* current_atlas_frame = data->atlas_map.at(frame_index).get();

    /*const*/ video_map &videoAttributeMap0 = data->attribute_maps.at(0);

    const v3c_parameter_set &vps = Decompression::get_saved_params(data->gof_index).vps;
    const size_t mapCount = vps.vps_map_count_minus1.at(current_atlas_frame->atlas_index) + 1;
    if ( attributeCount == 0 ) {
        Logger::log(LogLevel::INFO, "Post-reconstruction", "No attribute data \n");
        return 0;
    }

    std::vector<point3d> &pointToPixel = current_atlas_frame->pointToPixel_;
    auto&  color8bit = reconstruct->colors;
    color8bit.resize(reconstruct->getPointCount());
    size_t pointCount = reconstruct->getPointCount();
    
    /*printf( "pointCount                   = %zu \n", pointCount );
    printf( "pointToPixel size            = %zu \n", pointToPixel.size() );
    printf( "multipleStreams              = %zu \n", multipleStreams );
    printf( "attributeCount               = %zu \n", (size_t)attributeCount );
    printf( "mapCount            = %zu \n", (size_t)mapCount );*/

    const size_t shift = multipleStreams ? frame_index : frame_index * mapCount;
    for ( size_t i = 0; i < pointCount; ++i ) {
        const point3d &location = pointToPixel[i];
        const size_t x = location.data_[0];
        const size_t y = location.data_[1];
        const size_t f = location.data_[2];

        if ( f < mapCount ) {
            picture &frame = videoAttributeMap0.pictures.at(shift + f);
            for ( size_t c = 0; c < 3; ++c ) {
                color8bit.at(i).data_[c] = frame.get_value(c, x, y);
            }

        }
    }

    return reconstruct->getPointCount(); //tile.getTotalNumberOfRegularPoints()
}