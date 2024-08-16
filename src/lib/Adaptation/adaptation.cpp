#include "adaptation.hpp"
#include <fstream>
#include <iomanip>

using namespace uvgvpcc_dec;


inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

void Adaptation::output_decoded_frame(std::shared_ptr<point_cloud_frame> reconstruct, uvgvpcc_dec::API::decoded_output* out)
{
    Logger::log(LogLevel::INFO, "Adaptation", "Output decoded frame, GOF " + std::to_string(reconstruct->gof_index)
    + " frame " + std::to_string(reconstruct->frame_index_in_gof) + " \n");
    out->io_mutex.lock();
    out->frames.push(reconstruct);
    out->io_mutex.unlock();
    
    out->available_frames.release();
}

void Adaptation::convertYUV8ToRGB8(point_cloud_frame* reconstruct)
{
    Logger::log(LogLevel::TRACE, "Adaptation", "Convert colors YUV 8bit -> RGB 8bit \n");

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