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
    
    std::cout << "-- Occupancy map: " << data->occupancy_map.frame_count << " frames, " << data->occupancy_map.width << "x" << data->occupancy_map.height << std::endl;
    std::cout << "-- Geometry map: " << data->geometry_map.frame_count << " frames, " << data->geometry_map.width << "x" << data->geometry_map.height << std::endl;
    std::cout << "-- Attribute map: " << data->attribute_map.frame_count << " frames, " << data->attribute_map.width << "x" << data->attribute_map.height << std::endl;

}