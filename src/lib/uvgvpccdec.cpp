#include <fstream>

#include "uvgvpccdec/uvgvpccdec.hpp"
#include "Decompression/decompression.hpp"
#include "FormatConversion/formatConversion.hpp"
#include "Reconstruction/reconstruction.hpp"
#include "PostReconstruction/postReconstruction.hpp"
#include "Adaptation/adaptation.hpp"

namespace uvgvpcc_dec
{
void readFile(const std::string filename, std::vector<uint8_t> &data);

void API::initializeDecoder(const Parameters& param)
{
    Logger::log(LogLevel::INFO, "API", "Initialize decoder " + std::to_string(param.hello) + "\n");

}

void API::decodeV3CChunk(v3c_chunk &chunk)
{
    decompressed_data decompressed;
    video_parameter_set_nals long_term_video_parameters;
    Decompression::decompressV3CUnitStream(chunk, &decompressed, &long_term_video_parameters);
    FormatConversion::convertToNominalFormat(&decompressed);
    for (size_t frame_index = 0; frame_index < decompressed.frame_count; frame_index++) {
        point_cloud_frame reconstructed_point_cloud_frame;
        Reconstruction::construct_point_cloud_frame(&decompressed, &reconstructed_point_cloud_frame, frame_index);
        PostReconstruction::PostProcess(&decompressed, &reconstructed_point_cloud_frame, frame_index);
        Adaptation::convertYUV8ToRGB8(&reconstructed_point_cloud_frame);
        std::string out_name = "output-test-f" + std::to_string(frame_index) + ".ply";
        Adaptation::write(out_name, &reconstructed_point_cloud_frame);
    }
}

void API::decodeV3CSampleStream(std::vector<uint8_t> &data)
{
    decompressed_data decompressed;
    video_parameter_set_nals long_term_video_parameters;
    Decompression::decompressV3CSampleStream(data, &decompressed, &long_term_video_parameters);
    FormatConversion::convertToNominalFormat(&decompressed);
    for (size_t frame_index = 0; frame_index < decompressed.frame_count; frame_index++) {
        point_cloud_frame reconstructed_point_cloud_frame;
        Reconstruction::construct_point_cloud_frame(&decompressed, &reconstructed_point_cloud_frame, frame_index);
        PostReconstruction::PostProcess(&decompressed, &reconstructed_point_cloud_frame, frame_index);
        Adaptation::convertYUV8ToRGB8(&reconstructed_point_cloud_frame);
        std::string out_name = "output-test-f" + std::to_string(frame_index) + ".ply";
        Adaptation::write(out_name, &reconstructed_point_cloud_frame);
    }
    
    
    
    //reconstruct_multiple_frames.appendPointSet( reconstructed_point_cloud_frame );
    //if ( !decoderParams.reconstructedDataPath_.empty() ) {
    //reconstructs.write( decoderParams.reconstructedDataPath_, frameNumber, decoderParams.nbThread_ );
    
    
    }
} // namespace uvgvpcc_dec

