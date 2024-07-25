#include "formatConversion.hpp"

using namespace uvgvpcc_dec;

void FormatConversion::convertToNominalFormat(decompressed_data* data)
{
    Logger::log(LogLevel::INFO, "FormatConversion", "Converting data to nominal format \n");
    /*
        Nominal format:
        Bit depth - specified in VPS - correct from FFMPEG (FOR CURRENT FILE)
        Resolution - specified in ASPS - correct from FFMPEG (FOR CURRENT FILE)
        Chroma format - 4:4:4 - correct from FFMPEG
        Composition time - yeah ok
    */

    int ConvAtlasID = 0;
    int attrCount = data->vps.ai_attribute_count;

    uint32_t OccBitDepthNF = data->vps.occupancy_info.at(ConvAtlasID).oi_occupancy_2d_bit_depth_minus1 + 1;
    uint32_t GeoBitDepthNF = data->vps.geometry_info.at(ConvAtlasID).gi_geometry_2d_bit_depth_minus1 + 1;

    std::vector<uint32_t> AttrBitDepthNF;
    AttrBitDepthNF.resize(attrCount);
    for (auto attrIdx = 0; attrIdx < attrCount; ++attrIdx) {
        AttrBitDepthNF.at(attrIdx) = data->vps.attribute_info.at(ConvAtlasID).ai_attribute_2d_bit_depth_minus1.at(attrIdx) + 1;
    }

    uint32_t VideoWidthNF = data->asps.asps_frame_width;
    uint32_t VideoHeightNF = data->asps.asps_frame_height;

    std::string chromaFormat = "4:4:4";
    std::cout << "-- OccBitDepthNF = " << OccBitDepthNF << std::endl;
    std::cout << "-- GeoBitDepthNF = " << GeoBitDepthNF << std::endl;
    for (auto attrIdx = 0; attrIdx < attrCount; ++attrIdx) {
        std::cout << "-- AttrBitDepthNF[" << attrIdx << "] = " << AttrBitDepthNF.at(attrIdx) << std::endl;
    }
    std::cout << "-- VideoWidthNF = " << VideoWidthNF << std::endl;
    std::cout << "-- VideoHeightNF = " << VideoHeightNF << std::endl;
    std::cout << "-- Chroma format = " << chromaFormat << std::endl;

    std::cout << "-- V-PCC frame count: " << data->frame_count << " frames" << std::endl;
    std::cout << "-- Occupancy map: " << data->occupancy_map.frame_count << " frames, " << data->occupancy_map.width << "x" << data->occupancy_map.height << std::endl;
    printf("Channel sizes Y=%zu U=%zu V=%zu \n", data->occupancy_map.pictures.front().Y.size(),
        data->occupancy_map.pictures.front().U.size(), data->occupancy_map.pictures.front().V.size());
    for (size_t i = 0; i < data->geometry_maps.size(); i++) {
        std::cout << "-- Geometry map " << i << ": " << data->geometry_maps.at(i).frame_count << " frames, " << data->geometry_maps.at(i).width << "x" << data->geometry_maps.at(i).height << std::endl;
        printf("Channel sizes Y=%zu U=%zu V=%zu \n", data->geometry_maps.at(i).pictures.front().Y.size(),
        data->geometry_maps.at(i).pictures.front().U.size(), data->geometry_maps.at(i).pictures.front().V.size());
    }
    for (size_t i = 0; i < data->attribute_maps.size(); i++) {
        std::cout << "-- Attribute map " << i << ": " << data->attribute_maps.at(i).frame_count << " frames, " << data->attribute_maps.at(i).width << "x" << data->attribute_maps.at(i).height << std::endl;
        printf("Channel sizes Y=%zu U=%zu V=%zu \n", data->attribute_maps.at(i).pictures.front().Y.size(),
        data->attribute_maps.at(i).pictures.front().U.size(), data->attribute_maps.at(i).pictures.front().V.size());
    }
    
}