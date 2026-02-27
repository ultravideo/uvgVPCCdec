#include <algorithm>
#include <array>
#include <cassert>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <regex>
#include <semaphore>
#include <chrono>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include <filesystem>

#include "../utils/utils.hpp"
#include "cli.hpp"
#include "extras/miniply.h"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

#include "omp.h"

namespace {


enum PCCEndianness { PCC_BIG_ENDIAN = 0, PCC_LITTLE_ENDIAN = 1 };
static inline PCCEndianness PCCSystemEndianness() {
  uint32_t num = 1;
  return ( *( reinterpret_cast<char*>( &num ) ) == 1 ) ? PCC_LITTLE_ENDIAN : PCC_BIG_ENDIAN;
}

/* ------------------------ ripped from tmc2------------------------ */
// bool write_point_cloud( const std::string fileName, uvgvpcc_dec::Frame* frame, const bool asAscii ) {

//     uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Adaptation", "Write to file " + fileName + " \n");
//     std::ofstream fout( fileName, std::ofstream::out );
//     if ( !fout.is_open() ) { return false; }
//     const size_t pointCount = frame->pointCount;

//     fout << "ply\n";
//     if ( asAscii ) {
//         fout << "format ascii 1.0\n";
//     } else {
//         PCCEndianness endianess = PCCSystemEndianness();
//         if ( endianess == PCC_BIG_ENDIAN ) {
//         fout << "format binary_big_endian 1.0\n";
//         } else {
//         fout << "format binary_little_endian 1.0\n";
//         }
//     }
//     fout << "element vertex " << pointCount << '\n';
//     fout << "property float x\n";
//     fout << "property float y\n";
//     fout << "property float z\n";
//     if ( !frame->pointsAttribute.empty() ) {
//         fout << "property uchar red\n";
//         fout << "property uchar green\n";
//         fout << "property uchar blue\n";
//     }
//     fout << "element face 0\n";
//     fout << "property list uint8 int32 vertex_index\n";
//     fout << "end_header\n";
//     if ( asAscii ) {
//         fout << std::setprecision( std::numeric_limits<double>::max_digits10 );
//         for ( size_t i = 0; i < pointCount; ++i ) {
//             const uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>& position = frame->pointsGeometry[i];
//             //const PCCPoint3D& position = ( *this )[i];
//             fout << position[0] << " " << position[1] << " " << position[2];

//             if ( !frame->pointsAttribute.empty() ) {
//                 const uvgvpcc_dec::Vector3<uint8_t>& color = frame->pointsAttribute[i];
//                 fout << " " << static_cast<int>( color[0] ) << " " << static_cast<int>( color[1] ) << " "
//                     << static_cast<int>( color[2] );
//             }

//             //fout << std::endl;
//             fout << '\n';
//         }
//     } else {
//         fout.clear();
//         fout.close();
//         fout.open( fileName, std::ofstream::binary | std::ofstream::out | std::ofstream::app );
//         for ( size_t i = 0; i < pointCount; ++i ) {
//             const uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>& position = frame->pointsGeometry[i];
//             float value[3];
//             value[0] = position[0];
//             value[1] = position[1];
//             value[2] = position[2];
//             fout.write( reinterpret_cast<const char*>( &value ), sizeof( float ) * 3 );
//             if ( !frame->pointsAttribute.empty() ) {
//                 const uvgvpcc_dec::Vector3<uint8_t>& color = frame->pointsAttribute[i];
//                 fout.write( reinterpret_cast<const char*>( &color ), sizeof( uint8_t ) * 3 );
//             }
//         }
//     }
//     fout.close();
//     return true;
// }


bool write_point_cloud( const std::string fileName, uvgvpcc_dec::Frame* frame, const bool asAscii ) {

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Adaptation", "Write to file " + fileName + " \n");

    // Open in the proper mode from the start
    std::ofstream fout;
    fout.open(fileName, asAscii ? std::ofstream::out : std::ofstream::binary | std::ofstream::out);
    if (!fout.is_open()) return false;
    
    const size_t pointCount = frame->pointCount;

    fout << "ply\n";
    if ( asAscii ) {
        fout << "format ascii 1.0\n";
    } else {
        if ( PCCSystemEndianness() == PCC_BIG_ENDIAN ) {
        fout << "format binary_big_endian 1.0\n";
        } else {
        fout << "format binary_little_endian 1.0\n";
        }
    }
    fout << "element vertex " << pointCount << '\n';
    fout << "property float x\nproperty float y\nproperty float z\n";

    const bool hasAttri = !frame->pointsAttribute.empty();

    if (hasAttri) {
        fout << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
    }
    fout << "element face 0\nproperty list uint8 int32 vertex_index\nend_header\n";

    if ( asAscii ) {
        fout << std::setprecision( std::numeric_limits<double>::max_digits10 );
        for ( size_t i = 0; i < pointCount; ++i ) {
            const uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>& position = frame->pointsGeometry[i];
            fout << position[0] << " " << position[1] << " " << position[2];

            if ( hasAttri ) {
                const uvgvpcc_dec::Vector3<uint8_t>& color = frame->pointsAttribute[i];
                fout << " " << static_cast<int>( color[0] ) << " " << static_cast<int>( color[1] ) << " "
                    << static_cast<int>( color[2] );
            }

            //fout << std::endl;
            fout << '\n';
        }
    } else {
        for ( size_t i = 0; i < pointCount; ++i ) {
            const uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>& position = frame->pointsGeometry[i];
            float value[3] = { static_cast<float>(position[0]), static_cast<float>(position[1]), static_cast<float>(position[2]) };
            fout.write( reinterpret_cast<const char*>( &value ), sizeof( float ) * 3 );
            if ( hasAttri ) {
                const uvgvpcc_dec::Vector3<uint8_t>& color = frame->pointsAttribute[i];
                fout.write( reinterpret_cast<const char*>( &color ), sizeof( uint8_t ) * 3 );
            }
        }
    }
    fout.close();
    return true;
}


bool write_point_cloud_16bits( const std::string fileName, uvgvpcc_dec::Frame* frame, const bool asAscii ) {

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Adaptation", "Write to file " + fileName + " \n");

    // Open in the proper mode from the start
    std::ofstream fout;
    fout.open(fileName, asAscii ? std::ofstream::out : std::ofstream::binary | std::ofstream::out);
    if (!fout.is_open()) return false;
    
    const size_t pointCount = frame->pointCount;

    fout << "ply\n";
    if ( asAscii ) {
        fout << "format ascii 1.0\n";
    } else {
        if ( PCCSystemEndianness() == PCC_BIG_ENDIAN ) {
        fout << "format binary_big_endian 1.0\n";
        } else {
        fout << "format binary_little_endian 1.0\n";
        }
    }
    fout << "element vertex " << pointCount << '\n';
    fout << "property float x\nproperty float y\nproperty float z\n";

    const bool hasAttri = !frame->pointsAttribute.empty();

    if (hasAttri) {
        fout << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
    }
    fout << "element face 0\nproperty list uint8 int32 vertex_index\nend_header\n";

    if ( asAscii ) {
        fout << std::setprecision( std::numeric_limits<double>::max_digits10 );
        for ( size_t i = 0; i < pointCount; ++i ) {
            const uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>& position = frame->pointsGeometry[i];
            fout << position[0] << " " << position[1] << " " << position[2];

            if ( hasAttri ) {
                const uvgvpcc_dec::Vector3<uint8_t>& color = frame->pointsAttribute[i];
                fout << " " << static_cast<int>( color[0] ) << " " << static_cast<int>( color[1] ) << " "
                    << static_cast<int>( color[2] );
            }

            //fout << std::endl;
            fout << '\n';
        }
    } else {
        for ( size_t i = 0; i < pointCount; ++i ) {
            const uvgvpcc_dec::Vector3<uvgvpcc_dec::typeGeometryInput>& position = frame->pointsGeometry[i];
            float value[3] = { static_cast<float>(position[0]), static_cast<float>(position[1]), static_cast<float>(position[2]) };
            fout.write( reinterpret_cast<const char*>( &value ), sizeof( float ) * 3 );
            if ( hasAttri ) {
                const uvgvpcc_dec::Vector3<uint8_t>& color = frame->pointsAttribute[i];
                fout.write( reinterpret_cast<const char*>( &color ), sizeof( uint8_t ) * 3 );
            }
        }
    }
    fout.close();
    return true;
}


bool output_func(uvgvpcc_dec::API::point_cloud_frame_stream* output, const std::string& outputFilePath)
{   
    while (true) {
        output->available_frames.acquire();
        output->io_mutex.lock();
        while (!output->frames.empty())
        {
            auto &frame = output->frames.front();
            if (!frame || frame->pointCount == 0) {
                uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
                    "APPLICATION",
                    "Empty frame: All frames written.\n");
                output->frames.pop();
                return true;
            }
            char filename[255];
            std::snprintf(filename,
                            sizeof(filename),
                            outputFilePath.c_str(),
                            frame->frameId);

            write_point_cloud(filename, frame.get(), true);
            //write_point_cloud(filename, frame.get(), false);
            output->frames.pop();
        }
        output->io_mutex.unlock();

    }
    return true;
}

// This function return a number (v3c_unit_size)
size_t read_value(const uint8_t* src, size_t len) {
    size_t value = 0;
    // for (size_t i = 0; i < len; ++i) {
    //     value |= static_cast<size_t>(src[i]) << (8 * (len - 1 - i));
    // }
    while (len--) {
        value = (value << 8) | *src++;
    }
    return value;
}


/// @brief Application thread reading the input bitstream.
/// @param chunks
/// @param input_path
void file_reader(uvgvpcc_dec::API::v3c_unit_stream* chunks, const std::string& input_path) {
    std::ifstream file (input_path);
    if (!file.is_open()) {
        throw std::runtime_error("Bitstream writing : Could not open input file " + input_path);
    }

    uint8_t v3c_sample_stream_header = 0;
    file.read(reinterpret_cast<char*> (&v3c_sample_stream_header), sizeof(uint8_t));
    const size_t v3c_unit_size_precision = (v3c_sample_stream_header >> 5U) + 1U;
    chunks->v3c_unit_size_precision_bytes = v3c_unit_size_precision;
    
    size_t gof_id = 0;
    while (file.peek() != EOF)
    {
        uvgvpcc_dec::API::v3c_chunk chunk;
        chunk.data = std::make_unique<std::vector<uint8_t>>();
        std::vector<uint8_t>* chunk_data = chunk.data.get();

        for (size_t i = 0; i < 5; i++) { // 0:V3C_VPS, 1:V3C_AD, 2:V3C_OVD, 3:V3C_GVD, 4:V3C_AVD
            uint8_t v3c_size_array[v3c_unit_size_precision];
            file.read(reinterpret_cast<char*>(v3c_size_array), v3c_unit_size_precision);

            size_t v3c_unit_size = read_value(v3c_size_array, v3c_unit_size_precision);;
            chunk.v3c_unit_sizes.push_back(v3c_unit_size);

            size_t old_size = chunk_data->size();

            chunk_data->resize(old_size + v3c_unit_size);
            file.read(reinterpret_cast<char*>(&(*chunk_data)[old_size]), v3c_unit_size);

            printf("Read V3C unit of size %d\n", (int)v3c_unit_size);
        }
        chunk.gof_id = gof_id++;
        chunks->v3c_chunks.push(std::move(chunk));
        
    }
    file.close();
}

}  // anonymous namespace

int main(int argc, char* argv[]) {
    bool write_to_file_ = true;

    cli::opts_t appParameters;
    bool exitOnParse = false;
    try {
        exitOnParse = cli::opts_parse(appParameters, argc, argv);
    } catch (const std::exception& e) {
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::FATAL>("APPLICATION",
                                                               "An exception was caught during the parsing of the application parameters.\n");
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::FATAL>("APPLICATION", e.what() + std::string("\n"));
        cli::print_usage();
        return EXIT_FAILURE;
    }
    if (exitOnParse) {
        // --version or --help //
        return EXIT_SUCCESS;
    }

    if(!std::filesystem::exists(appParameters.inputPath)) {
        std::cerr << "\n!!! Error : The specified input bitstream does not exist :" << appParameters.inputPath << "\n" << std::endl;
        exit(EXIT_FAILURE);
    }

    if(!appParameters.outputPath.empty() && !appParameters.dstAddress.empty()) {
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::WARNING>("APPLICATION",
            "!!! Warning : No out path given or output path is '/dev/null'. No ply file will be writing.\n");
        if (*argv[2] == '1') {
            write_to_file_  = false;
        }
    } else if(!std::filesystem::exists(std::filesystem::path(appParameters.outputPath).parent_path())) {
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::FATAL>("APPLICATION",
            std::string("!!! Error : The directory in which you want to put the output ply files does not exist:") + std::filesystem::path(appParameters.outputPath).parent_path().string() + "\n");
        exit(EXIT_FAILURE);
    }

    uvgvpcc_dec::API::initializeDecoder();

    uvgvpcc_dec::API::v3c_unit_stream input;
    std::string inputPath = appParameters.inputPath;
    std::string outputFilePath = appParameters.outputPath;
    file_reader(&input, inputPath);



    // uvgvpcc_dec::API::point_cloud_frame_stream output;
    // std::thread file_writer_thread;
    // if (!outputFilePath.empty()) file_writer_thread = std::thread(output_func, &output, outputFilePath);

    // std::shared_ptr<uvgvpcc_dec::ThreadQueue> queue = std::make_shared<uvgvpcc_dec::ThreadQueue>();
    // queue->initThreadQueue(20);
    // std::shared_ptr<uvgvpcc_dec::Job> last_out = nullptr;

    // const size_t num_gof = input.v3c_chunks.size();
    // std::cout << "Num gof: " << std::to_string(num_gof) << std::endl;
    // for (size_t i = 0; i < num_gof; ++i) {
        
    //     input.io_mutex.lock();
    //     uvgvpcc_dec::API::v3c_chunk chunk = std::move(input.v3c_chunks.front());
    //     input.v3c_chunks.pop();
    //     input.io_mutex.unlock();
        
    //     chunk.gof_id = i;
    //     chunk.gof_count = num_gof;
    //     uvgvpcc_dec::API::decodeFrame(chunk, &output);

    // }
    // uvgvpcc_dec::API::emptyFrameQueue(queue, last_out);

    // output.io_mutex.lock();
    // output.frames.emplace(); // push an empty frame to signal the end of the stream
    // output.io_mutex.unlock();
    // output.available_frames.release();
    // if (file_writer_thread.joinable()) file_writer_thread.join();


    
    const size_t num_gof = input.v3c_chunks.size();
    // Step 1: Extract all chunks into a vector
    std::vector<uvgvpcc_dec::API::v3c_chunk> chunks;
    chunks.reserve(num_gof);
    while (!input.v3c_chunks.empty()) {
        chunks.push_back(std::move(input.v3c_chunks.front()));
        input.v3c_chunks.pop();
    }
    // Step 2: Parallel decode
    #pragma omp parallel for 
    for (size_t i = 0; i < num_gof; ++i) {
        uvgvpcc_dec::API::point_cloud_frame_stream output;
        std::thread file_writer_thread;
        if (!outputFilePath.empty()) file_writer_thread = std::thread(output_func, &output, outputFilePath);
        
        uvgvpcc_dec::API::v3c_chunk& chunk = chunks[i];
        chunk.gof_id = i;
        chunk.gof_count = num_gof;

        uvgvpcc_dec::API::decodeFrame(chunk, &output);

        output.frames.emplace(); // push an empty frame to signal the end of the stream
        output.available_frames.release();
        if (file_writer_thread.joinable()) file_writer_thread.join();
    }



    printf("Done\n");
    return EXIT_SUCCESS;
}