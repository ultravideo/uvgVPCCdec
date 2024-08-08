#include "uvgvpccdec/uvgvpccdec.hpp"
#include <iostream>
#include <thread>
#include <fstream>
#include <iomanip>

void readFile(const std::string filename, uvgvpcc_dec::API::v3c_unit_stream &unit_stream);

/* ------------------------ ripped from tmc2------------------------ */
bool write( const std::string fileName, point_cloud_frame* frame, const bool asAscii = true );
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

bool output_func(uvgvpcc_dec::API::decoded_output* output);

int main(int argc, char* argv[]) {

    if (argc != 2) {
        std::cout << "invalid number of arguments, enter .vpcc filename " << std::endl;
    }
    //std::string input_file = "longdress-f1.vpcc";
    std::string input_file = argv[1];

    uvgvpcc_dec::Logger::setLogLevel(uvgvpcc_dec::LogLevel::TRACE);
    uvgvpcc_dec::Parameters param;
    param.occupancy_width = 640;
    param.occupancy_height = 640;
    param.video_width = 1280;
    param.video_height = 1280;
    param.keep_intermediate_files = false;
    //param.max_points = 738590;
    uvgvpcc_dec::API::initializeDecoder(param);

    uvgvpcc_dec::API::v3c_unit_stream unit_stream;
    readFile(input_file, unit_stream);
    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Application", "Number of V3C chunks " + std::to_string(unit_stream.v3c_chunks.size()) + " \n");
    
    uvgvpcc_dec::API::decoded_output output; // Each point cloud frame gets appended to the output as they are decoded
    std::thread file_writer_thread;
    file_writer_thread = std::thread(output_func, &output);
    
    for (size_t i = 0; i < unit_stream.v3c_chunks.size(); ++i) {
        /*const*/ auto& chunk = unit_stream.v3c_chunks.front();
        uvgvpcc_dec::API::decodeV3CChunk(chunk, &output);
        unit_stream.v3c_chunks.pop();
    }
    std::cout << "Pushing empty frame" << std::endl;
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
    uvgvpcc_dec::API::v3c_chunk latest_chunk;
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
    unit_stream.v3c_chunks.push(latest_chunk);
    input_file.close();
}

bool output_func(uvgvpcc_dec::API::decoded_output* output)
{
    while (true) {
        output->available_frames.acquire();
        output->io_mutex.lock();
        std::shared_ptr<point_cloud_frame> frame = output->frames.front();

        if(frame->getPointCount() == 0) {
            uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::INFO, "APPLICATION", "Empty frame: All frames written.\n");
            break;
        }
        if(frame->getPointCount() != 0) {
            std::string out_name = "output-test-gof" + std::to_string(frame->gof_index) + "-f" + std::to_string(frame->frame_index) + ".ply";
            write(out_name, frame.get());
        }

        output->frames.pop();   
        output->io_mutex.unlock();
    }
    return true;
}

/* ------------------------ ripped from tmc2------------------------ */
bool write( const std::string fileName, point_cloud_frame* frame, const bool asAscii ) {

    uvgvpcc_dec::Logger::log(uvgvpcc_dec::LogLevel::TRACE, "Adaptation", "Write to file " + fileName + " \n");
    //std::ofstream fout( fileName, std::ofstream::out );
    //if ( !fout.is_open() ) { return false; }
    const size_t pointCount = frame->getPointCount();
    bool x = asAscii;
    /*fout << "ply" << std::endl;

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
    fout.close();*/
    return true;
}
