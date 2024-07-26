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
    Logger::log(LogLevel::INFO, "API", "Hello, World! " + std::to_string(param.hello) + "\n");

}

void API::decodeV3CSampleStream(const std::string filename)
{
    std::vector<uint8_t> data;
    readFile(filename, data);

    decompressed_data decompressed;
    video_parameter_set_nals long_term_video_parameters;
    
    BitstreamParsing::decompressV3CSampleStream(data, &decompressed, &long_term_video_parameters);
    FormatConversion::convertToNominalFormat(&decompressed);
    Logger::log(LogLevel::INFO, "uvgVPCC", "Start reconstructing " + std::to_string(decompressed.frame_count) + " frames \n");
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

void readFile(const std::string filename, std::vector<uint8_t> &data)
{
    std::cout << "Opening file " << filename << std::endl;
    std::ifstream input_file (filename);
    if (input_file.is_open()) {
        input_file.seekg(0, std::ios::end);
        std::size_t size = input_file.tellg();
        input_file.seekg(0, std::ios::beg);
        std::cout << "size of file is " << size << std::endl;

        data.resize(static_cast<std::size_t>(size)); // Allocate required storage
        input_file.read(reinterpret_cast<char*> (&data[0]), size);

        input_file.close();
    }
    else {
        throw std::runtime_error("Error reading input file");
    }
}
} // namespace uvgvpcc_dec

