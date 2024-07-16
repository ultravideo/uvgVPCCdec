#include "formatConversion.hpp"

using namespace uvgvpcc_dec;

void FormatConversion::convertToNominalFormat(decompressed_data* data)
{
    Logger::log(LogLevel::INFO, "FormatConversion", "Converting data to nominal format \n");
    /*
    Nominal format:
    Bit depth
    Resolution
    Chroma format
    Composition time

    OccBitDepthNF, is set equal to
    oi_occupancy_2d_bit_depth_minus1[ ConvAtlasID ] + 1 or to
    pin_occupancy_2d_bit_depth_minus1[ ConvAtlasID ] + 1, if pin_occupancy_present_flag[ ConvAtlasID ]
    equal to 1. The nominal bit depth for each geometry video component, GeoBitDepthNF, is set equal to
    gi_geometry_2d_bit_depth_minus1[ ConvAtlasID ] + 1 or
    pin_geometry_2d_bit_depth_minus1[ ConvAtlasID ] + 1, if pin_geometry_present_flag[ ConvAtlasID ]
    equal to 1. Finally, the nominal bit depth for each attribute video component with attribute index attrIdx,
    AttrBitDepthNF[ attrIdx ], is set equal to ai_attribute_2d_bit_depth_minus1[ ConvAtlasID ][ attrIdx ] + 1
    or pin_attribute_2d_bit_depth_minus1[ ConvAtlasID ][ attrIdx ], if
    pin_attribute_present_flag[ ConvAtlasID ] equal to 1.

    The nominal frame resolution for non-auxiliary video components is defined by the nominal width,
    VideoWidthNF, set equal to asps_frame_width, and the nominal height, VideoHeightNF, set equal to
    asps_frame_height. The nominal frame resolution for auxiliary video components is defined by the
    nominal width and height specified by the variables AuxVideoWidthNF and AuxVideoHeightNF,
    respectively.

    The nominal chroma format is defined to be 4:4:4.
    */


    int oi_occupancy_2d_bit_depth_minus1[1] = {0};
    int gi_geometry_2d_bit_depth_minus1[1] = {0};
    int ai_attribute_2d_bit_depth_minus1[1][1] = {{0}};
    int ConvAtlasID = 0;
    int attrIdx = 0;

    uint32_t OccBitDepthNF = oi_occupancy_2d_bit_depth_minus1[ ConvAtlasID ] + 1;
    uint32_t GeoBitDepthNF = gi_geometry_2d_bit_depth_minus1[ ConvAtlasID ] + 1;
    uint32_t AttrBitDepthNF = ai_attribute_2d_bit_depth_minus1[ ConvAtlasID ][ attrIdx ] + 1;

    int asps_frame_width = 0;
    int asps_frame_height = 0;

    uint32_t VideoWidthNF = data->asps.asps_frame_width;
    uint32_t VideoHeightNF = asps_frame_height;

    std::string chromaFormat = "4:4:4";

}