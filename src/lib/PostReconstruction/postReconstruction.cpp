#include "postReconstruction.hpp"

using namespace uvgvpcc_dec;

/* ------------------------ ripped from tmc2------------------------ */
void PostReconstruction::PostProcess(decompressed_data* data, point_cloud_frame* reconstruct, std::vector<uint32_t>& partition)
{
    Logger::log(LogLevel::INFO, "Post-reconstruction", "Post-processing point cloud \n");
    bool multipleStreams = data->vps.vps_multiple_map_streams_present_flag.at(0);
    uint8_t attributeCount = data->vps.ai_attribute_count;
    colorPointCloud(reconstruct, data, {}, multipleStreams, attributeCount, 0);

    Logger::log(LogLevel::INFO, "Post-reconstruction", "Convert colors YUV 8bit -> RGB 8bit \n");
    reconstruct->convertYUV8ToRGB8();
    auto c = partition.front();
}

size_t PostReconstruction::colorPointCloud(point_cloud_frame*                       reconstruct,
                                  decompressed_data* data,
                                  const std::vector<bool>&            absoluteT1List,
                                  const size_t                        multipleStreams,
                                  const uint8_t                       attributeCount,
                                  size_t                              accTilePointCount )
{
    Logger::log(LogLevel::INFO, "Post-reconstruction", "Start color point cloud \n");

    auto& test = absoluteT1List;
    atlas_frame* current_atlas_frame = data->atlas_map.front().get();
    if ( reconstruct->getPointCount() == 0 ) { return accTilePointCount; }
    reconstruct->colors.resize(reconstruct->getPointCount());

    /*const*/ video_map &videoAttributeMap0 = data->attribute_maps.at(0);
    //const video_map &videoAttributeMap1 = data->attribute_maps.at(1);

    // FIXED ATLAS INDEX, TODO
    const size_t mapCount = data->vps.vps_map_count_minus1.at(0) + 1; //params.mapCountMinus1_ + 1;
    if ( attributeCount == 0 ) {
        Logger::log(LogLevel::INFO, "Post-reconstruction", "No attribute data \n");
        return 0;
    }

    auto&  pointToPixel      = current_atlas_frame->getPointToPixel(); //tile.getPointToPixel();
    auto&  color16bit        = reconstruct->colors16;
    color16bit.resize(reconstruct->getPointCount());
    bool   useAuxVideo       = false; //tile.getUseRawPointsSeparateVideo();
    size_t numOfRawPointGeos = 0; //tile.getTotalNumberOfRawPoints();
    size_t numberOfEOMPoints = 0; //tile.getTotalNumberOfEOMPoints();
    size_t pointCount        = reconstruct->getPointCount(); //tile.getTotalNumberOfRegularPoints();
    if ( !useAuxVideo ) { pointCount += numOfRawPointGeos + numberOfEOMPoints; }
    
    printf( "pointCount                   = %zu \n", pointCount );
    printf( "reconstruct.getPointCount()  = %zu \n", reconstruct->getPointCount() );
    printf( "pointToPixel size            = %zu \n", pointToPixel.size() );
    printf( "multipleStreams              = %zu \n", multipleStreams );
    printf( "attributeCount               = %zu \n", (size_t)attributeCount );
    printf( "accTilePointCount            = %zu \n", (size_t)accTilePointCount );
    printf( "mapCount            = %zu \n", (size_t)mapCount );
    point_cloud_frame target;
    point_cloud_frame source;
    std::vector<size_t> targetIndex;
    targetIndex.resize( 0 );
    target.clear();
    source.clear();
    size_t test1 = 0;
    size_t test2 = 0;
    size_t test3 = 0;
    size_t test4 = 0;
    //target.addColors16bit();
    //source.addColors16bit();
    source.colors16.resize(pointCount);
    target.colors16.resize(pointCount);

    const size_t shift = multipleStreams ? current_atlas_frame->frame_index : current_atlas_frame->frame_index * mapCount;
    for ( size_t i = accTilePointCount; i < accTilePointCount + pointCount; ++i ) {
        test1++;
        const vector3d location = pointToPixel[i - accTilePointCount];
        const size_t x = current_atlas_frame->getLeftTopXInFrame() + location.data_[0]; //tile.getLeftTopXInFrame() + location[0];
        const size_t y = current_atlas_frame->getLeftTopYInFrame()+location.data_[1]; //tile.getLeftTopYInFrame() + location[1];
        const size_t f = location.data_[2];
        // false if ( params.singleMapPixelInterleaving_ ) {
        // false if ( multipleStreams != 0 ) {
        // else {
        test2++;
        if ( f < mapCount ) {
            test3++;
            /*const*/ picture &frame = videoAttributeMap0.pictures.at(shift + f);
            for ( size_t c = 0; c < 3; ++c ) {
                color16bit.at(i).data_[c] = frame.get_value(c, x, y);
            }
            int index = source.addPoint(reconstruct->positions[i]);
            source.set_color16( index, color16bit[i] );
        } else {
            target.addPoint( reconstruct->positions[i] );
            targetIndex.push_back( i );
        }
    }

    /* false if ( pointCount > 0 ) {
        transferColorWeight(&source, &target, reconstruct->getPointCount());*/

    // false if ( useAuxVideo ) {

    printf( "source pointCount = %zu \n", source.getPointCount() );
    printf( "target pointCount = %zu \n", target.getPointCount() );

    return accTilePointCount + reconstruct->getPointCount(); //tile.getTotalNumberOfRegularPoints()
}