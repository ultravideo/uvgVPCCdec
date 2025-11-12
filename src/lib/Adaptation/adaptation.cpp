#include "adaptation.hpp"
#include "uvgvpccdec/data_structures.hpp"
#include <fstream>
#include <iomanip>

using namespace uvgvpcc_dec;


inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

void Adaptation::output_decoded_frame(std::shared_ptr<point_cloud_frame> reconstruct, uvgvpcc_dec::API::decoded_output* out)
{
    Logger::log<LogLevel::INFO>("Adaptation", "Output decoded frame, GOF " + std::to_string(reconstruct->gof_index)
    + " frame " + std::to_string(reconstruct->frame_index_in_gof) + " \n");
    out->io_mutex.lock();
    out->frames.push(reconstruct);
    out->io_mutex.unlock();
    
    out->available_frames.release();
}

void Adaptation::convert_colors_fast(point_cloud_frame* reconstruct) {
    Logger::log<LogLevel::TRACE>("Adaptation", "Convert colors YUV 8bit -> RGB 8bit (FAST/APPROXIMATE) \n");
    std::vector<uvg_color>& colors = reconstruct->colors;
    // Precomputed constants to avoid recalculating
    const int offset = 128;

    for (size_t k = 0; k < reconstruct->getPointCount(); k++) {
        // Load YUV values as integers directly from the color array
        int y1 = colors[k].data_[0];
        int u1 = colors[k].data_[1] - offset;
        int v1 = colors[k].data_[2] - offset;

        // Fast approximate YUV to RGB conversion
        int r = y1 + (v1 * 8 / 5);  // 1.57480 ≈ 8/5
        int g = y1 - (u1 * 3 / 16) - (v1 * 6 / 13);  // Approximation of combined factors
        int b = y1 + (u1 * 15 / 8);  // 1.85563 ≈ 15/8

        // Directly clamp to 8-bit range using bitwise operations
        colors[k].data_[0] = static_cast<uint8_t>(r < 0 ? 0 : r > 255 ? 255 : r);
        colors[k].data_[1] = static_cast<uint8_t>(g < 0 ? 0 : g > 255 ? 255 : g);
        colors[k].data_[2] = static_cast<uint8_t>(b < 0 ? 0 : b > 255 ? 255 : b);
    }
}

void Adaptation::convert_colors_slow(point_cloud_frame* reconstruct)
{
    Logger::log<LogLevel::TRACE>("Adaptation", "Convert colors YUV 8bit -> RGB 8bit (SLOW/REFERENCE) \n");
    std::vector<uvg_color>& colors = reconstruct->colors;
    for ( size_t k = 0; k < reconstruct->getPointCount(); k++ ) {
        double y1     = colors[k].data_[0];
        double u1     = colors[k].data_[1];
        double v1     = colors[k].data_[2];
        double offset = 128.0;
        double scale  = 255.0;
        double weight = 1.0 / scale;

        y1 = weight * y1;
        u1 = weight * ( u1 - offset );
        v1 = weight * ( v1 - offset );
        y1 = ( std::max )( y1, 0.0 );
        y1 = ( std::min )( y1, 1.0 );
        u1 = ( std::max )( u1, -0.5 );
        u1 = ( std::min )( u1, 0.5 );
        v1 = ( std::max )( v1, -0.5 );
        v1 = ( std::min )( v1, 0.5 );

        //// convert normalized yuv444 to normalized rgb (fromat double)
        double r = y1 /*- 0.00000 * u1*/ + 1.57480 * v1;
        double g = y1 - 0.18733 * u1 - 0.46813 * v1;
        double b = y1 + 1.85563 * u1 /*+ 0.00000 * v1*/;

        //// convert normalized rgb to 8-bit rgb
        r = PCCClip( round( r * 255 ), 0.0, 255.0 );
        g = PCCClip( round( g * 255 ), 0.0, 255.0 );
        b = PCCClip( round( b * 255 ), 0.0, 255.0 );

        colors[k].data_[0] = static_cast<uint8_t>( r );
        colors[k].data_[1] = static_cast<uint8_t>( g );
        colors[k].data_[2] = static_cast<uint8_t>( b );
    }
}

/// TMC2 : convert yuv444 (16bit) to normalized yuv444 (format double)
void Adaptation::convertYUV16ToRGB8(point_cloud_frame* reconstruct) {

    std::vector<uvg_color16>& colors16 = reconstruct->colors16;
    std::vector<uvg_color>& colors8 = reconstruct->colors;

    colors8.resize(reconstruct->getPointCount());

    for ( size_t k = 0; k < reconstruct->getPointCount(); ++k ) {
        // double y1     = colors16bit_[k][0];
        // double u1     = colors16bit_[k][1];
        // double v1     = colors16bit_[k][2];


        double y1     = colors16[k].data_[0];
        double u1     = colors16[k].data_[1];
        double v1     = colors16[k].data_[2];    


        // std::cerr << "LF LOG : " << y1 << " " << u1 << " " << v1 << std::endl;


        double offset = 32768.0;
        double scale  = 65535.0;
        double weight = 1.0 / scale;

        y1 = weight * y1;
        u1 = weight * ( u1 - offset );
        v1 = weight * ( v1 - offset );
        y1 = ( std::max )( y1, 0.0 );
        y1 = ( std::min )( y1, 1.0 );
        u1 = ( std::max )( u1, -0.5 );
        u1 = ( std::min )( u1, 0.5 );
        v1 = ( std::max )( v1, -0.5 );
        v1 = ( std::min )( v1, 0.5 );

        //// convert normalized yuv444 to normalized rgb (fromat double)
        double r = y1 /*- 0.00000 * u1*/ + 1.57480 * v1;
        double g = y1 - 0.18733 * u1 - 0.46813 * v1;
        double b = y1 + 1.85563 * u1 /*+ 0.00000 * v1*/;

        //// convert normalized rgb to 8-bit rgb
        r = PCCClip( round( r * 255 ), 0.0, 255.0 );
        g = PCCClip( round( g * 255 ), 0.0, 255.0 );
        b = PCCClip( round( b * 255 ), 0.0, 255.0 );

        // colors_[k][0] = static_cast<uint8_t>( r );
        // colors_[k][1] = static_cast<uint8_t>( g );
        // colors_[k][2] = static_cast<uint8_t>( b );

        colors8[k].data_[0] = static_cast<uint8_t>( r );
        colors8[k].data_[1] = static_cast<uint8_t>( g );
        colors8[k].data_[2] = static_cast<uint8_t>( b );

    }
  }