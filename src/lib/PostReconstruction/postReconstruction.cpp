#include "postReconstruction.hpp"

using namespace uvgvpcc_dec;

void PostReconstruction::PostProcess(decompressed_data* data, point_cloud_frame* reconstruct)
{
    Logger::log(LogLevel::INFO, "Post-reconstruction", "Post-processing point cloud \n");
    bool multipleStreams = data->vps.vps_multiple_map_streams_present_flag.at(0);
    uint8_t attributeCount = data->vps.ai_attribute_count;
    color_point_cloud(reconstruct, data, multipleStreams, attributeCount);
}

size_t PostReconstruction::color_point_cloud(point_cloud_frame* reconstruct, decompressed_data* data, const size_t multipleStreams, const uint8_t attributeCount )
{
    Logger::log(LogLevel::INFO, "Post-reconstruction", "Start color point cloud \n");

    atlas_frame* current_atlas_frame = data->atlas_map.front().get();

    /*const*/ video_map &videoAttributeMap0 = data->attribute_maps.at(0);
    //const video_map &videoAttributeMap1 = data->attribute_maps.at(1);

    // FIXED ATLAS INDEX, TODO
    const size_t mapCount = data->vps.vps_map_count_minus1.at(0) + 1;
    if ( attributeCount == 0 ) {
        Logger::log(LogLevel::INFO, "Post-reconstruction", "No attribute data \n");
        return 0;
    }

    auto&  pointToPixel = current_atlas_frame->getPointToPixel(); //tile.getPointToPixel();
    auto&  color8bit = reconstruct->colors;
    color8bit.resize(reconstruct->getPointCount());
    size_t pointCount = reconstruct->getPointCount(); //tile.getTotalNumberOfRegularPoints();
    
    printf( "pointCount                   = %zu \n", pointCount );
    printf( "pointToPixel size            = %zu \n", pointToPixel.size() );
    printf( "multipleStreams              = %zu \n", multipleStreams );
    printf( "attributeCount               = %zu \n", (size_t)attributeCount );
    printf( "mapCount            = %zu \n", (size_t)mapCount );

    const size_t shift = multipleStreams ? current_atlas_frame->frame_index : current_atlas_frame->frame_index * mapCount;
    for ( size_t i = 0; i < pointCount; ++i ) {
        const vector3d location = pointToPixel[i];
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