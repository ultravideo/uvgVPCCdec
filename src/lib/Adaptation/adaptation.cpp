#include "adaptation.hpp"
#include <fstream>
#include <iomanip>

using namespace uvgvpcc_dec;

enum PCCEndianness { PCC_BIG_ENDIAN = 0, PCC_LITTLE_ENDIAN = 1 };
static inline PCCEndianness PCCSystemEndianness() {
  uint32_t num = 1;
  return ( *( reinterpret_cast<char*>( &num ) ) == 1 ) ? PCC_LITTLE_ENDIAN : PCC_BIG_ENDIAN;
}

/* ------------------------ ripped from tmc2------------------------ */
bool Adaptation::write( const std::string fileName, point_cloud_frame* frame, const bool asAscii ) {

    Logger::log(LogLevel::INFO, "Adaptation", "Write to file " + fileName + " \n");
    std::ofstream fout( fileName, std::ofstream::out );
    if ( !fout.is_open() ) { return false; }
    const size_t pointCount = frame->getPointCount();
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

inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
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