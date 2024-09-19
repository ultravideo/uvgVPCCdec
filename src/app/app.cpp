#include "uvgvpccdec/log.hpp"
#include "uvgvpccdec/uvgvpccdec.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <iostream>
#include <cstdio> 


void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream);
bool write_to_file_ = false;
bool fast_yuv_to_rgb_ = false;

/* ------------------------ ripped from tmc2------------------------ */
bool write( const std::string fileName, point_cloud_frame* frame, const bool asAscii = true );
void exportPointCloud(const std::string fileName, point_cloud_frame* frame);

enum PCCEndianness { PCC_BIG_ENDIAN = 0, PCC_LITTLE_ENDIAN = 1 };
static inline PCCEndianness PCCSystemEndianness() {
  uint32_t num = 1;
  return ( *( reinterpret_cast<char*>( &num ) ) == 1 ) ? PCC_LITTLE_ENDIAN : PCC_BIG_ENDIAN;
}

size_t read_value(const uint8_t* src, size_t len) {
    size_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value |= static_cast<size_t>(src[i]) << (8 * (len - 1 - i));
    }
    return value;
}

bool output_func(uvgvpcc_dec::API::decoded_output* output, const std::string& outputFilePath);

void displayHelp() {
    std::cout << "###################\n";
    std::cout << "Help for uvgVPCCdec\n";
    std::cout << "###################\n";
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {

    // LF : missing support -> RA
    // LF : missing support -> occupancyResolution=4
    // LF : missing support -> voxel 9 (and 11?)
    // LF : missing support -> having more than one GOF
    // LF : missing support -> TMC2 bitstream

    // LF : this works :  source my_env.sh && ./dev_utils.sh -i ready_for_winter_10 -f 38 -t 20 -b -v -d --uvgvpcc rate=16-22-2,geometry2DEncodingParam=Kvazaar-lossy-AI-YUV420-20-fast,attribute2DEncodingParam=Kvazaar-lossy-AI-YUV420-20-fast,preset=slow,occupancy2DEncodingParam=Kvazaar-lossless-AI-YUV420-20-fast

    uvgvpcc_dec::Logger::setLogLevel(uvgvpcc_dec::LogLevel::INFO);

    std::string input_file;
    std::string outputFilePath;


    for(int i = 1; i<argc; ++i) {
        if(!strcmp(argv[i], "-h")) {
            displayHelp();
            exit(EXIT_SUCCESS);
        }

        if(!strcmp(argv[i], "-i")) {
            input_file = argv[i+1];
        }

        if(!strcmp(argv[i], "-o")) {
            displayHelp();
            outputFilePath = argv[i+1];
        }


    }

    if(input_file.empty()) {
        displayHelp();
        std::cerr << "\n!!! Error : You didn't specify the input bitstream.\n" << std::endl;
        exit(EXIT_FAILURE);
    } else if(!std::filesystem::exists(input_file)) {
        std::cerr << "\n!!! Error : The specified input bitstream does not exist :" << input_file << "\n" << std::endl;
        exit(EXIT_FAILURE);
    }


    if(outputFilePath.empty()) {
        std::cerr << "\n!!! Warning : You didn't specify an output file path. No ply file will be writing.\n" << std::endl;
    } else if(!std::filesystem::exists(std::filesystem::path(outputFilePath).parent_path())) {
        std::cerr << "\n!!! Error : The directory in which you want to put the output ply files does not exist:" << std::filesystem::path(outputFilePath).parent_path() << "\n" << std::endl;
        exit(EXIT_FAILURE);
    }
    


    if (argc != 4) {
        std::cout << "invalid number of arguments, enter .vpcc filename, 1/0 (file writing), 1/0 (fast color conversion) " << std::endl;
    }
    //std::string input_file = "longdress-f1.vpcc";
    // std::string input_file = argv[1];
    if (*argv[2] == '1') {
        write_to_file_  = true;
    }

    uvgvpcc_dec::Parameters param;

    if (*argv[3] == '1') {
        param.fast_color_conversion  = true;
    }
    param.keep_intermediate_files = false;

    //param.max_points = 738590;
    uvgvpcc_dec::API::initializeDecoder(param);

    uvgvpcc_dec::API::v3c_unit_stream unit_stream;
    readFile(input_file, unit_stream);
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Application", "Number of V3C chunks " + std::to_string(unit_stream.v3c_chunks.size()) + " \n");
    
    uvgvpcc_dec::API::decoded_output output; // Each point cloud frame gets appended to the output as they are decoded
    std::thread file_writer_thread;
    file_writer_thread = std::thread(output_func, &output, outputFilePath);
    
    for (size_t i = 0; i < unit_stream.v3c_chunks.size(); ++i) {
        /*const*/ auto& chunk = unit_stream.v3c_chunks.front();
        uvgvpcc_dec::API::decodeV3CChunk(chunk, &output);
        unit_stream.v3c_chunks.pop();
    }
    uvgvpcc_dec::API::emptyFrameQueue();
    output.io_mutex.lock();
    std::shared_ptr<point_cloud_frame> empty = std::make_shared<point_cloud_frame>();
    output.frames.push(empty); // Push empty point cloud frame to signal end of data
    output.io_mutex.unlock();
    output.available_frames.release();
    file_writer_thread.join();
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "Application", "Done \n");
    return EXIT_SUCCESS;
}

void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream)
{
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "Opening file " + filename + " \n");
    std::ifstream input_file (filename);

    if(!input_file.is_open()) {
        throw std::runtime_error("File reading : Could not open input file " + filename);
    }
    uint8_t header_byte = 0;
    input_file.read(reinterpret_cast<char*> (&header_byte), sizeof(uint8_t));
    size_t v3c_unit_size_precision = (header_byte >> 5) + 1;

    size_t data_read = 1; // hdr byte already read
    unit_stream.v3c_unit_size_precision_bytes = v3c_unit_size_precision;
    
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::DEBUG, "Application", "V3C unit size precision " + std::to_string(v3c_unit_size_precision) + " \n");
    uvgvpcc_dec::v3c_chunk latest_chunk;
    while (input_file.peek() != EOF) {
        
        uint8_t v3c_size_array[v3c_unit_size_precision];
        input_file.read(reinterpret_cast<char*>(v3c_size_array), v3c_unit_size_precision);
        data_read += input_file.gcount();

        size_t v3c_unit_size = read_value(v3c_size_array, v3c_unit_size_precision);;
        latest_chunk.v3c_unit_sizes.push_back(v3c_unit_size);
        size_t old_size = latest_chunk.data.size();
        latest_chunk.data.resize(old_size + v3c_unit_size);
        input_file.read(reinterpret_cast<char*>(&latest_chunk.data[old_size]), v3c_unit_size);
        data_read += input_file.gcount();
        
    }
    unit_stream.v3c_chunks.push(std::make_shared<uvgvpcc_dec::v3c_chunk>(latest_chunk));
    input_file.close();
}

bool output_func(uvgvpcc_dec::API::decoded_output* output, const std::string& outputFilePath)
{   
    while (true) {
        output->available_frames.acquire();
        output->io_mutex.lock();
        std::shared_ptr<point_cloud_frame> frame = output->frames.front();

        if(frame->getPointCount() == 0) {
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "APPLICATION", "Empty frame: All frames written.\n");
            break;
        }
        if(!outputFilePath.empty() && frame->getPointCount() != 0) {
            char filename[255];
            std::snprintf(filename, sizeof(filename), outputFilePath.c_str(), frame->frameId);
            write(filename, frame.get(), true);
        }

        output->frames.pop();   
        output->io_mutex.unlock();
    }
    return true;
}

void exportPointCloud(const std::string fileName, point_cloud_frame* frame) {

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Adaptation", "Write to file " + fileName + " \n");

    std::ofstream fout(fileName, std::ofstream::out | std::ofstream::trunc);
    if (!fout.is_open()) {
        throw std::runtime_error("Error : can't create a stream from : " + fileName);
    }
    fout << "ply";
    fout << "\nformat ascii 1.0";
    fout << "\nelement vertex " << frame->getPointCount();
    fout << "\nproperty int x";
    fout << "\nproperty int y";
    fout << "\nproperty int z";

    fout << "\nproperty uchar red";
    fout << "\nproperty uchar green";
    fout << "\nproperty uchar blue";
    fout << "\nend_header\n";

    fout << std::setprecision(std::numeric_limits<double>::max_digits10);
    for (size_t i = 0; i < frame->getPointCount(); ++i) {
        point3d& position = frame->positions.at(i);
        fout << position.x() << " " << position.y() << " " << position.z();
        const uvg_color& color = frame->colors.at(i);
        fout << " " << static_cast<int>(color.data_[0]) << " " << static_cast<int>(color.data_[1]) << " "
             << static_cast<int>(color.data_[2]);
        fout << std::endl;
    }
    fout.close();
}

/* ------------------------ ripped from tmc2------------------------ */
bool write( const std::string fileName, point_cloud_frame* frame, const bool asAscii ) {

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Adaptation", "Write to file " + fileName + " \n");
    std::ofstream fout( fileName, std::ofstream::out );
    if ( !fout.is_open() ) { return false; }
    const size_t pointCount = frame->getPointCount();
    bool x = asAscii;
    fout << "ply" << std::endl;

    if ( asAscii ) {
        fout << "format ascii 1.0" << std::endl;
    } else {
        PCCEndianness endianess = PCCSystemEndianness();
        if ( endianess == PCC_BIG_ENDIAN ) {
        fout << "format binary_big_endian 1.0" << std::endl;
        } else {
        fout << "format binary_little_endian 1.0" << std::endl;
        }
    }
    fout << "element vertex " << pointCount << std::endl;
    if ( asAscii ) {
        fout << "property float x" << std::endl;
        fout << "property float y" << std::endl;
        fout << "property float z" << std::endl;
    } else {
        fout << "property float x" << std::endl;
        fout << "property float y" << std::endl;
        fout << "property float z" << std::endl;
    }
    if ( !frame->colors.empty() ) {
        fout << "property uchar red" << std::endl;
        fout << "property uchar green" << std::endl;
        fout << "property uchar blue" << std::endl;
    }
    fout << "element face 0" << std::endl;
    fout << "property list uint8 int32 vertex_index" << std::endl;
    fout << "end_header" << std::endl;
    if ( asAscii ) {
        fout << std::setprecision( std::numeric_limits<double>::max_digits10 );
        for ( size_t i = 0; i < pointCount; ++i ) {
            point3d& position = frame->positions.at(i);
            //const PCCPoint3D& position = ( *this )[i];
            fout << position.x() << " " << position.y() << " " << position.z();

            if ( !frame->colors.empty() ) {
                const uvg_color& color = frame->colors.at(i);
                fout << " " << static_cast<int>( color.data_[0] ) << " " << static_cast<int>( color.data_[1] ) << " "
                    << static_cast<int>( color.data_[2] );
            }

            fout << std::endl;
        }
    } else {
        fout.clear();
        fout.close();
        fout.open( fileName, std::ofstream::binary | std::ofstream::out | std::ofstream::app );
        for ( size_t i = 0; i < pointCount; ++i ) {
            point3d& position = frame->positions.at(i);
            //const PCCPoint3D& position = ( *this )[i];
            // fout.write( reinterpret_cast<const char* const>( &position ), sizeof( PCCType ) * 3 );
            float value[3];
            value[0] = position.data_[0];
            value[1] = position.data_[1];
            value[2] = position.data_[2];
            fout.write( reinterpret_cast<const char*>( &value ), sizeof( float ) * 3 );
            if ( !frame->colors.empty() ) {
                const uvg_color& color = frame->colors.at(i);
                fout.write( reinterpret_cast<const char*>( &color.data_ ), sizeof( uint8_t ) * 3 );
            }
        }
    }
    fout.close();
    return true;
}
