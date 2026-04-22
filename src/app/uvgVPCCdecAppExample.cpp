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
#include <cstring>

#include "../utils/utils.hpp"
#include "cli.hpp"
#include "extras/miniply.h"
#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

// #include "uvgvpcc/zmq_lib.h"
// #include "zmq.hpp"

#ifdef ENABLE_V3CRTP
#include <uvgv3crtp/version.h>
#include <uvgv3crtp/v3c_api.h>

#include <uvgrtp/util.hh>
#endif

#include "omp.h"

namespace {

// Semaphores for synchronization.
std::binary_semaphore available_input_slot{0};
std::binary_semaphore filled_input_slot{0};

enum class Retval : std::uint8_t { Running, Failure, Eof };

struct input_handler_args {
    // Parameters passed from main thread to input thread.

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const cli::opts_t& opts;

    std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk_in;

    std::string input_path;

    Retval retval;
    input_handler_args(const cli::opts_t& opts, std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk, Retval retval)
        : opts(opts), chunk_in(std::move(chunk)), retval(retval) {}
};

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
    while (1) {
        output->available_frames.acquire();
        output->io_mutex.lock();
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
                    frame->gofId,
                    frame->frameId);

        write_point_cloud(filename, frame.get(), false);
        // printf("Written frame %zu to file %s\n", frame->frameId, filename);
        output->frames.pop();
        output->io_mutex.unlock();
    }
    return true;
}


bool output_thread(uvgvpcc_dec::API::point_cloud_frame_stream* output, const std::string& outputFilePath)
{   
    int output_gof_count = 0;
    int expected_gof = 0;

    while (1) {
        output->available_frames.acquire();
        // output->io_mutex.lock();
        while (output->available_gofs[expected_gof].ready)
        {
            auto &decodedGof = output->available_gofs[expected_gof];
            if (decodedGof.terminated) {
                output_gof_count++;
                expected_gof++;
                continue;
            }

            for (auto &frame : decodedGof.gof->frames) {
                char filename[255];
                std::snprintf(filename,
                            sizeof(filename),
                            outputFilePath.c_str(),
                            frame->gofId,
                            frame->frameId);

                write_point_cloud(filename, frame.get(), false);
            }
            expected_gof++;
            output_gof_count++;
            // printf("output_gof_count: %d\n", output_gof_count);
        }
        // printf("output_gof_count: %d\n", output_gof_count);

        if (output_gof_count - output->total_gof == 0) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
                "APPLICATION",
                "Empty frame: All frames written.\n");
            return true;
        }
        // output->io_mutex.unlock();
    }
    return true;
}

bool output_thread_remote(uvgvpcc_dec::API::point_cloud_frame_stream* output, const std::shared_ptr<zmqHandler> zmq_handler)
{   
    int output_gof_count = 0;
    int expected_gof = 0;
    int frame_count = 0;

    while (1) {
        output->available_frames.acquire();
        // output->io_mutex.lock();
        while (output->available_gofs[expected_gof].ready)
        {
            auto &decodedGof = output->available_gofs[expected_gof];
            if (decodedGof.terminated) {
                output_gof_count++;
                expected_gof++;
                continue;
            }

            for (auto &frame : decodedGof.gof->frames) {
                {
                    std::lock_guard<std::mutex> lock(zmq_handler->zmq_mutex);

                    zmq_send(
                        zmq_handler->positionSocket,
                        frame->pointsGeometry.data(),
                        frame->pointsGeometry.size() * sizeof(uvgvpcc_dec::Vector3<uint16_t>),
                        0
                    );

                    zmq_send(
                        zmq_handler->colorSocket,
                        frame->pointsAttribute.data(),
                        frame->pointsAttribute.size() * sizeof(uvgvpcc_dec::Vector3<uint8_t>),
                        0
                    );
                    frame_count++;
                }
            }
            expected_gof++;
            output_gof_count++;
            // printf("output_gof_count: %d\n", output_gof_count);
        }
        // printf("output_gof_count: %d\n", output_gof_count);

        if (output_gof_count - output->total_gof == 0) {
            // uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
            //     "APPLICATION",
            //     "Empty frame: All frames written.\n");
            break;
            // return true;
        }
        // output->io_mutex.unlock();
    }
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>(
                "APPLICATION",
                "Empty frame: " + std::to_string(frame_count) + " frames sent.\n");
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

// constexpr int EXPECTED_NUM_GOFs = 16;
// constexpr int EXPECTED_NUM_VPSs = 16;
constexpr int EXPECTED_NUM_GOFs = 57;
constexpr int EXPECTED_NUM_VPSs = 57;

constexpr int EXPECTED_NUM_AD_NALU = 19;
constexpr int EXPECTED_NUM_OVD_NALU = 19;
constexpr int EXPECTED_NUM_GVD_NALU = 35;
constexpr int EXPECTED_NUM_AVD_NALU = 35;
constexpr uint8_t V3C_SIZE_PRECISION = 5;
constexpr uint8_t AtlasNAL_SIZE_PRECISION = 2;
constexpr uint8_t Video_SIZE_PRECISION = 4;
// constexpr int TIMEOUT = 6000;
constexpr int TIMEOUT = 10000;
// Auto size precision may not match orig bitstream
constexpr bool AUTO_PRECISION_MODE = false;
constexpr bool AUTO_EXPECTED_NUM_MODE = false;

constexpr char address[] = "127.0.0.1";
constexpr int port = 8890;

//#define ENABLE_V3CRTP true

void v3c_receiver(const std::shared_ptr<input_handler_args>& args) {
#ifdef ENABLE_V3CRTP
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>(
        "V3CRTP",
        "Using uvgV3CRTP lib version " + uvgV3CRTP::get_version() + "\n");

    std::cerr << "Initialize state...\n" << std::flush;
    uvgV3CRTP::V3C_UNIT_TYPE expected_units[] = { uvgV3CRTP::V3C_VPS, uvgV3CRTP::V3C_AD, uvgV3CRTP::V3C_OVD, uvgV3CRTP::V3C_GVD, uvgV3CRTP::V3C_AVD };
    
    uint16_t ports[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {};

    // Use lambda to pick correct constructor to get correct scope for state object
    uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver> state = ([&]() {
        if (args->opts.srcPort.size() == 1) {
            // Set ports to the same value
            std::fill(std::begin(ports), std::end(ports), args->opts.srcPort.front());
            return uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver>(
                uvgV3CRTP::INIT_FLAGS::AD | uvgV3CRTP::INIT_FLAGS::OVD | uvgV3CRTP::INIT_FLAGS::GVD | uvgV3CRTP::INIT_FLAGS::AVD |
                    uvgV3CRTP::INIT_FLAGS::VPS,  
                args->opts.srcAddress.c_str(), args->opts.srcPort.front()                                                    // Receiver address and port
            );
        } else {
            std::copy(args->opts.srcPort.begin(), args->opts.srcPort.end(), ports);  // Use all given ports

            return uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver>(
                uvgV3CRTP::INIT_FLAGS::AD | uvgV3CRTP::INIT_FLAGS::OVD | uvgV3CRTP::INIT_FLAGS::GVD | uvgV3CRTP::INIT_FLAGS::AVD |
                    uvgV3CRTP::INIT_FLAGS::VPS,  
                args->opts.srcAddress.c_str(), ports                                                               // Receiver address and ports
            );
        }
    })();

    // uvgV3CRTP::V3C_State<uvgV3CRTP::V3C_Receiver> state(
    //     uvgV3CRTP::INIT_FLAGS::VPS |
    //     uvgV3CRTP::INIT_FLAGS::AD  |
    //     uvgV3CRTP::INIT_FLAGS::OVD |
    //     uvgV3CRTP::INIT_FLAGS::GVD |
    //     uvgV3CRTP::INIT_FLAGS::AVD,
    //     dst_address.c_str(), dst_port.front()
    // ); // Create a new state in a receiver configuration
    // //state.init_sample_stream(v3c_size_precision); //Don't init sample stream here since receive_bistream() will create one
    // std::cerr << "Done\n" << std::flush;

    // Define necessary information for receiving a v3c stream
    uint8_t v3c_size_precision   = AUTO_PRECISION_MODE ? static_cast<uint8_t>(-1) : V3C_SIZE_PRECISION;
    uint8_t atlas_size_precision = AUTO_PRECISION_MODE ? static_cast<uint8_t>(-1) : AtlasNAL_SIZE_PRECISION;
    uint8_t video_size_precision = AUTO_PRECISION_MODE ? static_cast<uint8_t>(-1) : Video_SIZE_PRECISION;
    
    size_t expected_number_of_gof = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_GOFs;
    size_t num_vps                = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_VPSs;
    size_t num_ad_nalu            = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_AD_NALU;
    size_t num_ovd_nalu           = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_OVD_NALU;
    size_t num_gvd_nalu           = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_GVD_NALU;
    size_t num_avd_nalu           = AUTO_EXPECTED_NUM_MODE ? static_cast<size_t>(-1) : EXPECTED_NUM_AVD_NALU;
    size_t num_pvd_nalu           = 0;
    size_t num_cad_nalu           = 0;

    // Auto size precision if set to 0 (may not match orig bitstream)
    uint8_t size_precisions[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
        0,
        atlas_size_precision,
        video_size_precision,
        video_size_precision,
        video_size_precision,
        video_size_precision,
        atlas_size_precision,
    };
    size_t num_nalus[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
        num_vps,
        num_ad_nalu,
        num_ovd_nalu,
        num_gvd_nalu,
        num_avd_nalu,
        num_pvd_nalu,
        num_cad_nalu,
    };

    uvgV3CRTP::HeaderStruct header_defs[uvgV3CRTP::NUM_V3C_UNIT_TYPES] = {
        {uvgV3CRTP::V3C_VPS},
        {uvgV3CRTP::V3C_AD, 0, 0},
        {uvgV3CRTP::V3C_OVD, 0, 0},
        {uvgV3CRTP::V3C_GVD, 0, 0, 0, 0, 0, false},
        {uvgV3CRTP::V3C_AVD, 0, 0, 0, 0, 0, false},
        {uvgV3CRTP::V3C_PVD, 0, 0},
        {uvgV3CRTP::V3C_CAD, 0},
    };

    // ************************************************************************************
    std::cerr << "Initialize state sample stream...\n" << std::flush;
    state.init_sample_stream(v3c_size_precision); //Init sample stream here since receive_gof() assumes data is initialized
    std::cerr << "Done\n" << std::flush;

    std::cerr << "Receiving bitstream...\n" << std::flush;
    if (v3c_size_precision == 0) {
        std::cerr << "Error: V3C_SIZE_PRECISION cannot be 0 since it is used to determine if size precision is included in the bitstream. Please set it to a value larger than 0 or set AUTO_PRECISION_MODE to true." << std::endl;
        throw std::runtime_error("Invalid V3C_SIZE_PRECISION");
    }
    
    bool receiving = true;
    size_t min_unit_size = 4*8 + v3c_size_precision; // 4 bytes for header + size precision
    int chunk_count = 0;
    Retval returnValue = Retval::Running;

    while (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::OK && receiving) {
        std::cerr << "  Receiving GoF...\n" << std::flush;

        std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk = std::make_shared<uvgvpcc_dec::API::v3c_chunk>();
        chunk->data = std::make_unique<std::vector<uint8_t>>();
        std::vector<uint8_t>* chunk_data = chunk->data.get();

        for (auto unit_type : expected_units) {
            // std::cerr << "  Receiving Unit...\n" << std::flush;
            uvgV3CRTP::receive_unit(&state, unit_type, size_precisions[unit_type], num_nalus[unit_type], header_defs[unit_type], TIMEOUT);

            //Increment VPS id in headers when a new vps is expected (headers should be given as out-of-band info)
            if ((unit_type == uvgV3CRTP::V3C_VPS) && num_nalus[uvgV3CRTP::V3C_VPS] != 0)
            {
                num_nalus[uvgV3CRTP::V3C_VPS] -= 1;
            }
            else if (num_nalus[uvgV3CRTP::V3C_VPS] != 0)
            {
                header_defs[unit_type].vuh_v3c_parameter_set_id += 1;
            }

            // Dont't stop receiving even if timestamp error occurs, just print the error
            if (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::TIMESTAMP) {
                std::cerr << " Timestamp error: " << state.get_error_msg() << std::endl;
                state.reset_error_flag();
            }

            if (state.get_error_flag() == uvgV3CRTP::ERROR_TYPE::OK) {
                state.last_gof();
                // state.print_cur_gof_bitstream_info(unit_type);
                // std::cerr << " Received:\n" << std::flush;

                size_t rec_len = 0;
                auto rec = state.get_bitstream_cur_gof_unit(unit_type, &rec_len);
                if (!rec || rec_len < min_unit_size) { // Arbitrary minimum size check to ensure we have a valid bitstream (32 bytes for headers + size precision)
                    std::cerr << "No reconstructed bitstream available for unit type " << unit_type << "\n" << std::flush;
                    receiving = false;
                    break;
                }

                chunk->gof_id = state.cur_gof_ind();
                const uint8_t* ptr = reinterpret_cast<const uint8_t*>(rec);
                size_t unit_size = rec_len - v3c_size_precision;
                ptr += v3c_size_precision;
                chunk->v3c_unit_sizes.push_back(unit_size);
                size_t old_size = chunk->data->size();
                chunk->data->resize(old_size + unit_size);
                std::memcpy(chunk->data->data() + old_size, ptr, unit_size);

                std::cerr << "Stream size for current unit type of GOF " << static_cast<int>(chunk->gof_id) << " : " << unit_type << " : " << static_cast<int>(unit_size) << "\n" << std::flush;
            } else {
                std::cerr << "Failed receiving GOF\n" << std::flush;
                receiving = false;
                break;
            }

        }

        if (receiving) {
            available_input_slot.acquire();
            args->chunk_in = chunk;
            if (returnValue == Retval::Failure) {
                args->retval = Retval::Failure;
                break;
            } else {
                assert(returnValue == Retval::Running && args->retval == Retval::Running);
            }
            chunk_count++;
            filled_input_slot.release();
        }
    }
    available_input_slot.acquire();
    args->chunk_in = nullptr;
    args->retval = Retval::Eof;
    filled_input_slot.release();
    
    printf("Chunks received: %d\n", chunk_count);

    std::cerr << "Done Receiving GoFs\n" << std::flush;
#else
    throw std::runtime_error("V3C RTP not enabled, re-run cmake with '-DENABLE_V3CRTP=ON'.");
#endif

}

void inputReadThread_old(const std::shared_ptr<input_handler_args>& args) {
    Retval returnValue = Retval::Running;
    std::ifstream file (args->input_path);
    if (!file.is_open()) {
        throw std::runtime_error("Bitstream writing : Could not open input file " + args->input_path);
    }

    uint8_t v3c_sample_stream_header = 0;
    file.read(reinterpret_cast<char*> (&v3c_sample_stream_header), sizeof(uint8_t));
    const size_t v3c_unit_size_precision = (v3c_sample_stream_header >> 5U) + 1U;

    size_t gof_id = 0;

    while (file.peek() != EOF)
    {
        std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk = std::make_shared<uvgvpcc_dec::API::v3c_chunk>();
        chunk->data = std::make_unique<std::vector<uint8_t>>();
        std::vector<uint8_t>* chunk_data = chunk->data.get();

        for (size_t i = 0; i < 5; i++) { // 0:V3C_VPS, 1:V3C_AD, 2:V3C_OVD, 3:V3C_GVD, 4:V3C_AVD
            uint8_t v3c_size_array[v3c_unit_size_precision];
            file.read(reinterpret_cast<char*>(v3c_size_array), v3c_unit_size_precision);

            size_t v3c_unit_size = read_value(v3c_size_array, v3c_unit_size_precision);;
            chunk->v3c_unit_sizes.push_back(v3c_unit_size);

            size_t old_size = chunk_data->size();

            chunk_data->resize(old_size + v3c_unit_size);
            file.read(reinterpret_cast<char*>(&(*chunk_data)[old_size]), v3c_unit_size);

            printf("Read V3C unit of size %d\n", (int)v3c_unit_size);
        }
        chunk->gof_id = gof_id++;

        // Signal that an item has been produced
        available_input_slot.acquire();
        args->chunk_in = chunk;
        if (returnValue == Retval::Failure) {
            args->retval = Retval::Failure;
            break;
        } else {
            assert(returnValue == Retval::Running && args->retval == Retval::Running);
        }
        filled_input_slot.release();
        
    }
    file.close();

    // Signal that all input frames has been load
    available_input_slot.acquire();
    args->chunk_in = nullptr;
    args->retval = Retval::Eof;
    filled_input_slot.release();
    
}

void inputReadThread(const std::shared_ptr<input_handler_args>& args, uvgvpcc_dec::API::point_cloud_frame_stream* output) {
    Retval returnValue = Retval::Running;
    std::ifstream file (args->input_path);
    if (!file.is_open()) {
        throw std::runtime_error("Bitstream writing : Could not open input file " + args->input_path);
    }

    uint8_t v3c_sample_stream_header = 0;
    file.read(reinterpret_cast<char*> (&v3c_sample_stream_header), sizeof(uint8_t));
    const size_t v3c_unit_size_precision = (v3c_sample_stream_header >> 5U) + 1U;

    size_t gof_id = 0;

    while (file.peek() != EOF)
    {
        std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> chunk = std::make_shared<uvgvpcc_dec::API::v3c_chunk>();
        chunk->data = std::make_unique<std::vector<uint8_t>>();
        std::vector<uint8_t>* chunk_data = chunk->data.get();

        for (size_t i = 0; i < 5; i++) { // 0:V3C_VPS, 1:V3C_AD, 2:V3C_OVD, 3:V3C_GVD, 4:V3C_AVD
            uint8_t v3c_size_array[v3c_unit_size_precision];
            file.read(reinterpret_cast<char*>(v3c_size_array), v3c_unit_size_precision);

            size_t v3c_unit_size = read_value(v3c_size_array, v3c_unit_size_precision);;
            chunk->v3c_unit_sizes.push_back(v3c_unit_size);

            size_t old_size = chunk_data->size();

            chunk_data->resize(old_size + v3c_unit_size);
            file.read(reinterpret_cast<char*>(&(*chunk_data)[old_size]), v3c_unit_size);

            printf("Read V3C unit of size %d\n", (int)v3c_unit_size);
        }
        chunk->gof_id = gof_id++;

        // Signal that an item has been produced
        available_input_slot.acquire();
        args->chunk_in = chunk;
        uvgvpcc_dec::API::decodedGOF decodedGOF = {};
        output->available_gofs.push_back(decodedGOF);
        if (returnValue == Retval::Failure) {
            args->retval = Retval::Failure;
            break;
        } else {
            assert(returnValue == Retval::Running && args->retval == Retval::Running);
        }
        filled_input_slot.release();
        
    }
    file.close();

    output->total_gof = static_cast<int>(gof_id);

    // Signal that all input frames has been loaded
    available_input_slot.acquire();
    args->chunk_in = nullptr;
    args->retval = Retval::Eof;
    filled_input_slot.release();
    
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

    if(!appParameters.outputPath.empty() && !appParameters.srcAddress.empty()) {
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

    const std::shared_ptr<input_handler_args> in_args = std::make_shared<input_handler_args>(appParameters, nullptr, Retval::Running);
    in_args->input_path = inputPath;

    std::thread inputTh;

    if (appParameters.remoteOutputData) {
        printf("Remote output is on!\n");
        printf("%s, %s\n", appParameters.positionAddress.c_str(), appParameters.colorAddress.c_str());
        zmq_handler = std::make_shared<zmqHandler>();
        zmq_handler->positionSocket.connect(appParameters.positionAddress);
        zmq_handler->colorSocket.connect(appParameters.colorAddress);
    }


    uvgvpcc_dec::API::point_cloud_frame_stream output;
    std::thread outputTh;

    if(appParameters.in_order_output) outputTh = appParameters.remoteOutputData ? std::thread(&output_thread_remote, &output, zmq_handler) : std::thread(&output_thread, &output, appParameters.outputPath);

    if (appParameters.remoteInputData) {
        printf("Remote input data is on, using V3CRTP to receive the bitstream\n");
        printf("Listening on address %s and port %d\n", appParameters.srcAddress.c_str(), appParameters.srcPort[0]);

        //v3c_receiver(&input, appParameters.srcAddress, appParameters.srcPort);
        inputTh = std::thread(&v3c_receiver, in_args);
    } else {
        inputTh = std::thread(&inputReadThread, in_args, &output);
    }

    std::shared_ptr<uvgvpcc_dec::API::v3c_chunk> currChunk = nullptr;
    available_input_slot.release();

    std::thread file_writer_thread;

    for (;;) {
        filled_input_slot.acquire();
        currChunk = in_args->chunk_in;
        in_args->chunk_in = nullptr;
        if (in_args->retval == Retval::Eof) {
            break;
        }
        if (in_args->retval == Retval::Failure) {
            return EXIT_FAILURE;
        }
        available_input_slot.release();

        try {
            if (appParameters.remoteOutputData) {
                if (appParameters.in_order_output) {
                    uvgvpcc_dec::API::decodeFrame_parallel(currChunk, &output, appParameters.in_order_output, outputFilePath);
                } else {
                    uvgvpcc_dec::API::decodeFrame_parallel_remote_output(currChunk, zmq_handler);
                }
            } else {
                uvgvpcc_dec::API::decodeFrame_parallel(currChunk, &output, appParameters.in_order_output, outputFilePath);
                // if (appParameters.in_order_output) {
                //     uvgvpcc_dec::API::decodeFrame_parallel_in_order(currChunk, &output);
                // } else {    
                //     uvgvpcc_dec::API::decodeFrame_parallel(currChunk, outputFilePath);
                // }
            }
        } catch (const std::runtime_error& e) {
            // Only one try and catch block. All exceptions thrown by the library are catched here.
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::FATAL>(
                "APPLICATION", "Caught exception using uvgvpcc_dec library: " + std::string(e.what()) + " failed after processing\n");
            return EXIT_FAILURE;
        }
    }

    printf("Total GOF count: %d\n", output.total_gof);

    uvgvpcc_dec::API::emptyFrameQueue();

    inputTh.join();

    if (appParameters.in_order_output) {
        output.io_mutex.lock();
        output.available_gofs.emplace_back();
        output.io_mutex.unlock();
        output.available_frames.release();
    }

    if (outputTh.joinable() && appParameters.in_order_output) outputTh.join();

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("MAP ENCODING", "Decoding completed.\n");


    return EXIT_SUCCESS;
}