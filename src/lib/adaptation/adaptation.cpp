#include "adaptation.hpp"

#include <fstream>
//#include <iostream>


#include <mutex>
#include <cmath>


#include "uvgvpcc/log.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

using namespace uvgvpcc_dec;

namespace {

inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

inline float PCCClip( const float& n, const float& lower, const float& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}


/*
offset = 128.0 but for what ?
*/
// constexpr int32_t horizontal0_[2] = {0, 256};             // Pos = 1
// constexpr int32_t horizontal1_[4] = {-16, 144, 144, -16}; // Pos = 2
// constexpr int32_t vertical0_[4] = {-8, 64, 216, -16};     // Pos = 2
// constexpr int32_t vertical1_[4] = {-16, 216, 64, -8};     // Pos = 2

// void upsamling_horizontal0() {
//     constexpr float scale = 0.003906f; //  1 / (1 << 8) 

// }

// void upsampleYUV420toYUV444(
//     const std::vector<uint8_t>& src, 
//     const size_t& width, 
//     const size_t& height, 
//     std::vector<uint16_t>& dst, 
//     bool invertChroma = true
// ) {
//     dst.resize(width * height);

//     // Copy Y and convert to 16-bit
//     for(size_t i = 0; i < width * height; ++i)
//         Y16[i] = static_cast<uint16_t>(.Y[i]) << 8;

//     size_t width2 = width / 2;

//     size_t width2 = width >> 1;

//     size_t idx = 0;
//     for(size_t y = 0; y < height; y += 2) {
//         for(size_t x2 = 0; x2 < width2; ++x2, ++idx) {
//             size_t x = x2 * 2;
//             uint16_t val = static_cast<uint16_t>(src[idx]) << 8;
//             if (invertChroma) val = 65535 - val; // TMC2 color inversion
//             dst[y * width + x]     = val;
//             dst[y * width + x + 1] = val;
//             dst[(y+1) * width + x]     = val;
//             dst[(y+1) * width + x + 1] = val;
//         }
//     }

//     // // Helper lambda to upsample U or V
//     // auto upsampleChannel = [&](const std::vector<uint8_t>& src, std::vector<uint16_t>& dst) {
//     //     size_t idx = 0;
//     //     for(size_t y = 0; y < height; y += 2) {
//     //         for(size_t x2 = 0; x2 < width2; ++x2, ++idx) {
//     //             size_t x = x2 * 2;
//     //             uint16_t val = static_cast<uint16_t>(src[idx]) << 8;
//     //             if (invertChroma) val = 65535 - val; // TMC2 color inversion
//     //             dst[y * width + x]     = val;
//     //             dst[y * width + x + 1] = val;
//     //             dst[(y+1) * width + x]     = val;
//     //             dst[(y+1) * width + x + 1] = val;
//     //         }
//     //     }
//     // };

//     // upsampleChannel(old.U, U16);
//     // upsampleChannel(old.V, V16);
// }



// TMC2 : convert yuv444 (16bit) to normalized yuv444 (format double)
void convertYUV16ToRGB8(std::vector<Vector3<uint16_t>>& colors_16bits, std::vector<Vector3<uint8_t>>& colors) {
    double offset = 32768.0;
    double scale  = 65535.0;
    double weight = 1.0 / scale;
    colors.resize(colors_16bits.size());

    for(size_t i = 0; i < colors_16bits.size(); ++i) {
        double y = colors_16bits[i][0];
        double u = colors_16bits[i][1];
        double v = colors_16bits[i][2];

        y = weight * y;
        u = weight * ( u - offset );
        v = weight * ( v - offset );
        y = ( std::max )( y, 0.0 );
        y = ( std::min )( y, 1.0 );
        u = ( std::max )( u, -0.5 );
        u = ( std::min )( u, 0.5 );
        v = ( std::max )( v, -0.5 );
        v = ( std::min )( v, 0.5 );


        // convert normalized yuv444 to normalized rgb (fromat double)
        double r = y /*- 0.00000 * u1*/ + 1.57480 * v;
        double g = y - 0.18733 * u - 0.46813 * v;
        double b = y + 1.85563 * u /*+ 0.00000 * v1*/;

        // convert normalized rgb to 8-bit rgb
        r = PCCClip( round( r * 255 ), 0.0, 255.0 );
        g = PCCClip( round( g * 255 ), 0.0, 255.0 );
        b = PCCClip( round( b * 255 ), 0.0, 255.0 );

        colors[i][0] = static_cast<uint8_t>( r );
        colors[i][1] = static_cast<uint8_t>( g );
        colors[i][2] = static_cast<uint8_t>( b );
    }
    
}


// 
void convert_colors_slow(std::vector<Vector3<uint8_t>>& colors) {
    const double& offset = 128.0;
    const double& scale = 255.0;
    const double& weight = 1.0 / scale;

    for (auto& color : colors) {
        double y1 = weight * color[0];
        double u1 = weight * (color[1] - offset);
        double v1 = weight * (color[2] - offset);

        y1 = (std::max) (y1, 0.0);
        y1 = (std::min) (y1, 1.0);
        u1 = (std::max) (u1, -0.5);
        u1 = (std::min) (u1, 0.5);
        v1 = (std::max) (v1, -0.5);
        v1 = (std::min) (v1, 0.5);

        double r = y1 + 1.57480 * v1;
        double g = y1 - 0.18733 * u1 - 0.46813 * v1;
        double b = y1 + 1.85563 * u1;

        r = PCCClip( std::round( r * scale ), 0.0, scale );
        g = PCCClip( std::round( g * scale ), 0.0, scale );
        b = PCCClip( std::round( b * scale ), 0.0, scale );

        color[0] = static_cast<uint8_t>(r);
        color[1] = static_cast<uint8_t>(g);
        color[2] = static_cast<uint8_t>(b);
    }

}

void convert_colors_fast(std::vector<Vector3<uint8_t>>& colors) {
    for (auto& color : colors) {
        const float y = color[0];
        const float u = color[1] - 128;
        const float v = color[2] - 128;

        const float r = y + (v * 8 / 5);  // 1.57480 ≈ 8/5
        const float g = y - (u * 3 / 16) - (v * 6 / 13);  // Approximation of combined factors
        const float b = y + (u * 15 / 8);  // 1.85563 ≈ 15/8

        // Directly clamp to 8-bit range using bitwise operations
        color[0] = static_cast<uint8_t>(r < 0 ? 0 : r > 255 ? 255 : r);
        color[1] = static_cast<uint8_t>(g < 0 ? 0 : g > 255 ? 255 : g);
        color[2] = static_cast<uint8_t>(b < 0 ? 0 : b > 255 ? 255 : b);
    }
}


enum PCCEndianness { PCC_BIG_ENDIAN = 0, PCC_LITTLE_ENDIAN = 1 };
static inline PCCEndianness PCCSystemEndianness() {
  uint32_t num = 1;
  return ( *( reinterpret_cast<char*>( &num ) ) == 1 ) ? PCC_LITTLE_ENDIAN : PCC_BIG_ENDIAN;
}

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

} // anonymous namespace

void Adaptation::adapt_in_order(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output) {
    for (int i = 0; i < gofUVG->frames.size(); i++) {
        auto &frame = gofUVG->frames[i];

        if (p_->useTMC2AttributeYUVConversion) {

        } else {
            if (p_->fast_color_conversion) {

            } else {
                convert_colors_slow(frame->pointsAttribute);
            }
        }
    }

    output->io_mutex.lock();
    output->available_gofs[gofUVG->gofId].gof = gofUVG;
    output->available_gofs[gofUVG->gofId].ready = true;
    output->io_mutex.unlock();
    output->available_frames.release();
}

// void Adaptation::adapt(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const std::string& outputFilePath) {
//     for (int i = 0; i < gofUVG->frames.size(); i++) {
//         auto &frame = gofUVG->frames[i];

//         if (p_->useTMC2AttributeYUVConversion) {

//         } else {
//             if (p_->fast_color_conversion) {

//             } else {
//                 convert_colors_slow(frame->pointsAttribute);
//             }
//         }
        
//         // uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::INFO>("Adaptation", "Output decoded frame, GOF " + std::to_string(gofUVG->gofId)
//         //     + " frame " + std::to_string(i) + " \n");

//         char filename[255];
//         std::snprintf(filename,
//                     sizeof(filename),
//                     outputFilePath.c_str(),
//                     frame->gofId,
//                     frame->frameId);

//         write_point_cloud(filename, frame.get(), false);
//         //write_point_cloud(filename, frame.get(), true);
//     }
// }

void Adaptation::adapt_delay(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output) {
    for (int i = 0; i < gofUVG->frames.size(); i++) {
        auto &frame = gofUVG->frames[i];

        if (p_->useTMC2AttributeYUVConversion) {

        } else {
            if (p_->fast_color_conversion) {

            } else {
                convert_colors_slow(frame->pointsAttribute);
            }
        }
    }

    output->io_mutex.lock();
    output->available_gofs_queue.push(gofUVG);
    output->io_mutex.unlock();
    output->available_frames.release();
}

void Adaptation::adapt(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, uvgvpcc_dec::API::point_cloud_frame_stream* output, const bool& in_order_output, const std::string& outputFilePath) {
    for (int i = 0; i < gofUVG->frames.size(); i++) {
        auto &frame = gofUVG->frames[i];

        if (p_->useTMC2AttributeYUVConversion) {
            convertYUV16ToRGB8(frame->pointsAttribute16bits, frame->pointsAttribute);
        } else {
            if (p_->fast_color_conversion) {
                convert_colors_fast(frame->pointsAttribute);
            } else {
                // printf("Using slow color conversion, color size: %zu\n", frame->pointsAttribute.size());
                convert_colors_slow(frame->pointsAttribute);
            }
        }

        if (in_order_output) continue;

        char filename[255];
        std::snprintf(filename,
                    sizeof(filename),
                    outputFilePath.c_str(),
                    frame->gofId,
                    frame->frameId);

        write_point_cloud(filename, frame.get(), false);
    }

    if (in_order_output) {
        output->io_mutex.lock();
        output->available_gofs[gofUVG->gofId].gof = gofUVG;
        output->available_gofs[gofUVG->gofId].ready = true;
        output->io_mutex.unlock();
        output->available_frames.release();
    }
}

void Adaptation::adapt_remote_output(std::shared_ptr<uvgvpcc_dec::GOF>& gofUVG, const std::shared_ptr<zmqHandler> zmq_handler) {
    std::mutex io_mutex;
    for (int i = 0; i < gofUVG->frames.size(); i++) {
        auto &frame = gofUVG->frames[i];

        if (p_->useTMC2AttributeYUVConversion) {

        } else {
            if (p_->fast_color_conversion) {

            } else {
                convert_colors_slow(frame->pointsAttribute);
            }
        }
        
        // zmq_handler->zmq_mutex.lock();
        // zmq_send(zmq_handler->positionSocket, frame->pointsGeometry.data(), frame->pointsGeometry.size()*sizeof(uvgvpcc_dec::Vector3<uint16_t>), 0);
        // zmq_send(zmq_handler->colorSocket, frame->pointsAttribute.data(), frame->pointsAttribute.size()*sizeof(uvgvpcc_dec::Vector3<uint8_t>), 0);
        // zmq_handler->zmq_mutex.unlock();

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
        }


        /*Send remote output*/
        // io_mutex.lock();
        // frame->pointsGeometry;
        // frame->pointsAttribute;
        // io_mutex.unlock();
    }
    
    // gofUVG->bitstreamOccupancy.clear();
    // gofUVG->bitstreamGeometry.clear();
    // gofUVG->bitstreamAttribute.clear();
    // gofUVG->frames.clear();
    // printf("GOF %d sent\n", (int)gofUVG->gofId);
}
