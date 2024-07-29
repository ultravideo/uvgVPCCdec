#include "formatConversion.hpp"

using namespace uvgvpcc_dec;

void FormatConversion::convertToNominalFormat(decompressed_data* data)
{
    Logger::log(LogLevel::TRACE, "FormatConversion", "Converting data to nominal format \n");
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
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Format conversion", "OccBitDepthNF = "
        + std::to_string(OccBitDepthNF) + ", GeoBitDepthNF " + std::to_string(GeoBitDepthNF) + " \n");

    for (auto attrIdx = 0; attrIdx < attrCount; ++attrIdx) {
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Format conversion", "AttrBitDepthNF["
            + std::to_string(attrIdx) + "] = " + std::to_string(AttrBitDepthNF.at(attrIdx)) + " \n");
    }
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Format conversion", "VideoWidthNF = "
        + std::to_string(VideoWidthNF) + ", VideoHeightNF = " + std::to_string(VideoHeightNF) 
        + ", chromaFormat = 4:4:4 \n");

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Format conversion", "V-PCC frame count = "
        + std::to_string(data->frame_count) + ", Occupancy map frame count = " + std::to_string(data->occupancy_map.frame_count) 
        + " (" + std::to_string(data->occupancy_map.width) + "x" + std::to_string(data->occupancy_map.height) + ") \n"
        + "Channels (frame 0) Y=" + std::to_string(data->occupancy_map.pictures.front().Y.size())
        + " U=" + std::to_string(data->occupancy_map.pictures.front().U.size())
        + " V=" + std::to_string(data->occupancy_map.pictures.front().V.size()) + "\n");

    for (size_t i = 0; i < data->geometry_maps.size(); i++) {
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Format conversion", "Geometry map [" + std::to_string(i)
            + "] frame count = " + std::to_string(data->geometry_maps.at(i).frame_count) 
            + " (" + std::to_string(data->geometry_maps.at(i).width) + "x" + std::to_string(data->geometry_maps.at(i).height) + ") \n"
            + "Channels (frame 0) Y=" + std::to_string(data->geometry_maps.at(i).pictures.front().Y.size())
            + " U=" + std::to_string(data->geometry_maps.at(i).pictures.front().U.size())
            + " V=" + std::to_string(data->geometry_maps.at(i).pictures.front().V.size()) + "\n");
    }
    for (size_t i = 0; i < data->attribute_maps.size(); i++) {
        uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Format conversion", "Attribute map [" + std::to_string(i)
            + "] frame count = " + std::to_string(data->attribute_maps.at(i).frame_count) 
            + " (" + std::to_string(data->attribute_maps.at(i).width) + "x" + std::to_string(data->attribute_maps.at(i).height) + ") \n"
            + "Channels (frame 0) Y=" + std::to_string(data->attribute_maps.at(i).pictures.front().Y.size())
            + " U=" + std::to_string(data->attribute_maps.at(i).pictures.front().U.size())
            + " V=" + std::to_string(data->attribute_maps.at(i).pictures.front().V.size()) + "\n");
    }
    
}