#include "Decompression/PCCBitstreamCommon.h"
#include "PCCSei.h"

#include "decompression.hpp"
#include "uvgvpccdec/bitstream_common.hpp"
#include "uvgvpccdec/data_structures.hpp"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <stdlib.h>

/* In debug mode print out some extra info */
#define BITSTREAM_DEBUG false

using namespace uvgvpcc_dec;

static const uint8_t* cbuf_;
uvgvpcc_dec::context* dec_context1_;

size_t current_gof_index_ = 0;
std::vector<parameter_sets> saved_params_ = {};

AVCodecContext* occupancy_codec_ctx_ = nullptr;
AVCodecContext* geometry_codec_ctx_ = nullptr;
AVCodecContext* attribute_codec_ctx_ = nullptr;
std::mutex occupancy_codec_mtx_;
std::mutex geometry_codec_mtx_;
std::mutex attribute_codec_mtx_;

static size_t read_value(const uint8_t* src, size_t len) {
    size_t value = 0;
    for (size_t i = 0; i < len; ++i) {
        value |= static_cast<size_t>(src[i]) << (8 * (len - 1 - i));
    }
    return value;
}

bool keep_intermediate_files_ = false;

const parameter_sets &Decompression::get_saved_params(const size_t gof_index)
{
    return saved_params_.at(gof_index);
}

/* TODO: make sure this function works in all cases */
void Decompression::advance_bitstream(std::size_t bits, bitstream_position &pos)
{
    std::size_t bytes = bits / 8 + (pos.bits + bits % 8) / 8;
    pos.bytes += bytes;
    pos.bits = (pos.bits + bits % 8) % 8;
}

/* TODO: make sure this function works in all cases */
void Decompression::align_bitstream(bitstream_position &pos)
{
    if(pos.bits != 0) {
        pos.bytes++;
        pos.bits = 0;
    }
}

/* NOTE -------------------- Straight from TMC2 -------------------- */
uint32_t Decompression::read_bits(uint8_t bits, bitstream_position &pos) {
    uint32_t value = 0;
    for ( std::size_t i = 0; i < bits; i++ ) {
      value |= ( ( cbuf_[pos.bytes] >> ( 7 - pos.bits ) ) & 1 ) << ( bits - 1 - i );
      if ( pos.bits == 7 ) {
        pos.bytes++;
        pos.bits = 0;
      } else {
        pos.bits++;
      }
    }
    return value;
}

/* NOTE -------------------- Straight from TMC2 -------------------- */
uint32_t Decompression::read_bits_ue(bitstream_position &pos)
{
    uint32_t value = 0, code = 0, length = 0;
    code = read_bits( 1, pos );
    if ( 0 == code ) {
      length = 0;
      while ( !( code & 1 ) ) {
        code = read_bits( 1, pos );
        length++;
      }
      value = read_bits( length, pos );
      value += ( 1 << length ) - 1;
    }
    return value;
}

uint32_t Decompression::read(uint8_t bits, bitstream_position &pos, const std::string &name) {
    uint32_t value = read_bits(bits, pos);
#if BITSTREAM_DEBUG
    printf("%-50s u(%u) : %d : %zu\n", name.c_str(),bits,value, (size_t)pos.bytes);
#endif
    (void)name; // Suppress unused parameter warning
    return value;
}

uint32_t Decompression::read_ue(bitstream_position &pos, const std::string &name)
{
    uint32_t value = read_bits_ue(pos);
#if BITSTREAM_DEBUG
    printf("%-50s u(v) : %d\n", name.c_str(),value);
#endif
    (void)name; // Suppress unused parameter warning
    return value;
}

void Decompression::initializeStaticParameters(const uvgvpcc_dec::Parameters& param, uvgvpcc_dec::context* context)
{
    dec_context1_ = context;
    keep_intermediate_files_ = param.keep_intermediate_files;

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Video decoder initialized \n");
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H265);
    if (!codec){
        throw std::runtime_error("Codec not found");
    }

    occupancy_codec_ctx_ = avcodec_alloc_context3(codec);
    geometry_codec_ctx_ = avcodec_alloc_context3(codec);
    attribute_codec_ctx_ = avcodec_alloc_context3(codec);

    if (!occupancy_codec_ctx_ || !geometry_codec_ctx_ || !attribute_codec_ctx_){
        throw std::runtime_error("Could not allocate avcodec context");
    }

    if (avcodec_open2(occupancy_codec_ctx_, codec, nullptr) < 0){
        throw std::runtime_error("Could not initialize avcodec context");
    }
    if (avcodec_open2(geometry_codec_ctx_, codec, nullptr) < 0){
        throw std::runtime_error("Could not initialize avcodec context");
    }
    if (avcodec_open2(attribute_codec_ctx_, codec, nullptr) < 0){
        throw std::runtime_error("Could not initialize avcodec context");
    }
}

void Decompression::parse_gofs(const uvgvpcc_dec::v3c_chunk &chunk, std::vector<uvgvpcc_dec::gof_info> &infos)
{
    cbuf_ = chunk.data.data();
    bitstream_position ptr;

    for (size_t i = 0; i < chunk.v3c_unit_sizes.size(); i++) {
        size_t v3c_unit_size = chunk.v3c_unit_sizes.at(i);
        size_t v3c_unit_payload_size = chunk.v3c_unit_sizes.at(i) - 4; // not incl. header
        size_t pre_header = ptr.bytes;
        // Next 4 bytes are the V3C unit header
        uint8_t vuh_unit_type = read(5, ptr, "vuh_unit_type");
        uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Parser", "V3C unit size " + std::to_string(chunk.v3c_unit_sizes.at(i))
            + ", type " + std::to_string(vuh_unit_type) + " \n");
        advance_bitstream(4 * 8 - 5, ptr); // skip the rest of v3c header for now

        if(vuh_unit_type == V3C_UNIT_TYPE::V3C_VPS) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Parser", "** Added GOF, start " + std::to_string(pre_header) + " ** \n");
            uvgvpcc_dec::gof_info new_gof;
            new_gof.vps_start = pre_header;
            new_gof.vps_size = v3c_unit_size;
            infos.push_back(new_gof);
        }
        else if (vuh_unit_type == V3C_UNIT_TYPE::V3C_AD) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Parser", "** Added composition unit, start " + std::to_string(pre_header) + " ** \n");

            // V3C_AD units signal the start of new composition units
            uvgvpcc_dec::composition_unit new_comp_unit ;
            new_comp_unit.ad_start = pre_header;
            new_comp_unit.ad_size = v3c_unit_size;

            infos.back().composition_units.push_back(new_comp_unit);
            infos.back().cu_count++;
        }
        else if (vuh_unit_type == V3C_UNIT_TYPE::V3C_OVD) {
            infos.back().composition_units.back().ovd_start = pre_header;
            infos.back().composition_units.back().ovd_size = v3c_unit_size;
        }
        else if (vuh_unit_type == V3C_UNIT_TYPE::V3C_GVD) {
            infos.back().composition_units.back().gvd_start = pre_header;
            infos.back().composition_units.back().gvd_size = v3c_unit_size;
        }
        else if(vuh_unit_type == V3C_UNIT_TYPE::V3C_AVD) {
            infos.back().composition_units.back().avd_start = pre_header;
            infos.back().composition_units.back().avd_size = v3c_unit_size;
        }
        else {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::FATAL>("Parser", "Unkown V3C unit type " + std::to_string(vuh_unit_type) + " \n");
        }
        advance_bitstream(v3c_unit_payload_size * 8, ptr);
    }
    saved_params_.resize(infos.size());
}


void convertYUV420ToYUV444uvgVPCCSimpleFrame( picture& picSrc, picture& picDst ) {

    size_t width = picSrc.width;
    size_t height = picSrc.height;

    picDst.width = width;
    picDst.height = height;
    
    picDst.Y = picSrc.Y;

    picDst.format = PCCCOLORFORMAT::YUV444;

    picDst.U.resize(width * height);
    picDst.V.resize(width * height);


    size_t widthChroma  = width / 2;
    size_t heightChroma = height / 2;
    for(size_t j = 0; j < heightChroma; ++j) {
        for(size_t i = 0; i < widthChroma; ++i) {
            size_t index = j*width*2 + i*2;

            const uint16_t tempU = picSrc.U[i+j*widthChroma];
            picDst.U[index] = tempU;
            picDst.U[index+1] = tempU;
            picDst.U[index+width] = tempU;
            picDst.U[index+width+1] = tempU;

            const uint16_t tempV = picSrc.V[i+j*widthChroma];
            picDst.V[index] = tempV;
            picDst.V[index+1] = tempV;
            picDst.V[index+width] = tempV;
            picDst.V[index+width+1] = tempV;
        }
    }
}

void convertYUV420ToYUV444uvgVPCCSimpleVideo(std::vector<video_map>& attribute_maps ) {
  

    video_map videoSrc = attribute_maps[0];
    video_map videoDst;
    videoDst.pictures.resize( videoSrc.frame_count );
    videoDst.height = videoSrc.height;
    videoDst.width = videoSrc.width;

    for ( size_t i = 0; i < videoSrc.frame_count; i++ ) {
        convertYUV420ToYUV444uvgVPCCSimpleFrame( videoSrc.pictures[i], videoDst.pictures[i]);
    }

    attribute_maps[0] = videoDst;

}

  uint8_t                   clamp( uint8_t v, uint8_t a, uint8_t b )  { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
  int                 clamp( int v, int a, int b )  { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
  float               clamp( float v, float a, float b )  { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
  double              clamp( double v, double a, double b )  { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
  static inline float fMin( float a, float b ) { return ( ( a ) < ( b ) ) ? ( a ) : ( b ); }
  static inline float fMax( float a, float b ) { return ( ( a ) > ( b ) ) ? ( a ) : ( b ); }
  static inline float fClip( float x, float low, float high ) { return fMin( fMax( x, low ), high ); }

void YUVtoFloatYUV( const std::vector<uint8_t>& src,
                                                  std::vector<float>&   dst,
                                                  const bool            chroma )  {
  size_t count = src.size();
  dst.resize( count );
  float    minV   = chroma ? -0.5f : 0.f;
  float    maxV   = chroma ? 0.5f : 1.f;
  uint16_t offset = chroma ? 1 == 1 ? 128 : 512 : 0;
  double   scale  = 1 == 1 ? 255. : 1023.;
  double   weight = 1.0 / scale;
  for ( size_t i = 0; i < count; i++ ) {
    dst[i] = clamp( (float)( weight * (double)( src[i] - offset ) ), minV, maxV );
  }
}

struct Filter {
  std::vector<float> data_;
  double             offset_;
  double             shift_;
};

struct Filter420to444 {
  Filter horizontal0_;
  Filter vertical0_;
  Filter horizontal1_;
  Filter vertical1_;
};

static Filter420to444 g_filter420to444_0 = {
    // 0 UF_F0
     {{0.0, +256.0}, +128.0, 8.0},
     {{-8.0, +64.0, +216.0, -16.0}, +128.0, 8.0},
     {{-16.0, +144.0, +144.0, -16.0}, +128.0, 8.0},
     {{-16.0, +216.0, +64.0, -8.0}, +128.0, 8.0}};


inline float upsamplingVertical0( const std::vector<float>& im,
                                const int                 width,
                                const int                 height,
                                const int                 i0,
                                const int                 j0 )  {
    const float scale    = 1.0f / ( (float)( 1 << ( (int)g_filter420to444_0.vertical0_.shift_ ) ) );
    const float offset   = 0.00000000;
    const int   position = int( g_filter420to444_0.vertical0_.data_.size() + 1 ) >> 1;
    float       value    = 0;
    for ( int i = 0; i < (int)g_filter420to444_0.vertical0_.data_.size(); i++ ) {
        value += g_filter420to444_0.vertical0_.data_[i] * (float)( im[clamp( i0 + i - position, 0, height - 1 ) * width + j0] );
    }
    return (float)( ( value + (float)offset ) * (float)scale );
}

inline float upsamplingVertical1( const std::vector<float>& im,
                                    int                       width,
                                    int                       height,
                                    int                       i0,
                                    int                       j0 )  {
    const float scale    = 1.0f / ( (float)( 1 << ( (int)g_filter420to444_0.vertical1_.shift_ ) ) );
    const float offset   = 0.00000000;
    const int   position = int( g_filter420to444_0.vertical1_.data_.size() + 1 ) >> 1;
    float       value    = 0;
    for ( int i = 0; i < (int)g_filter420to444_0.vertical1_.data_.size(); i++ ) {
      value += g_filter420to444_0.vertical1_.data_[i] * (float)( im[clamp( i0 + i - position, 0, height - 1 ) * width + j0] );
    }
    return (float)( ( value + (float)offset ) * (float)scale );
}

inline float upsamplingHorizontal0( const std::vector<float>& im,
                                      int                       width,
                                      int                       i0,
                                      int                       j0 )  {
    const float scale    = 1.0f / ( (float)( 1 << ( (int)g_filter420to444_0.horizontal0_.shift_ ) ) );
    const float offset   = 0.00000000;
    const int   position = int( g_filter420to444_0.horizontal0_.data_.size() + 1 ) >> 1;
    float       value    = 0;
    for ( int j = 0; j < (int)g_filter420to444_0.horizontal0_.data_.size(); j++ ) {
      value += g_filter420to444_0.horizontal0_.data_[j] * (float)( im[i0 * width + clamp( j0 + j - position, 0, width - 1 )] );
    }
    return (float)( ( value + (float)offset ) * (float)scale );
}

inline float upsamplingHorizontal1( const std::vector<float>& im,
                                      int                       width,
                                      int                       i0,
                                      int                       j0 )  {
    const float scale    = 1.0f / ( (float)( 1 << ( (int)g_filter420to444_0.horizontal1_.shift_ ) ) );
    const float offset   = 0.00000000;
    const int   position = int( g_filter420to444_0.horizontal1_.data_.size() + 1 ) >> 1;
    float       value    = 0;
    for ( int j = 0; j < (int)g_filter420to444_0.horizontal1_.data_.size(); j++ ) {
      value += g_filter420to444_0.horizontal1_.data_[j] * (float)( im[i0 * width + clamp( j0 + j - position, 0, width - 1 )] );
    }
    return (float)( ( value + (float)offset ) * (float)scale );
}


void upsampling( const std::vector<float>& chromaIn,
                                               std::vector<float>&       chromaOut,
                                               const int                 widthIn,
                                               const int                 heightIn
                                                )  {
  int                widthOut = widthIn * 2, heightOut = heightIn * 2;
  std::vector<float> temp;
  chromaOut.resize( widthOut * heightOut );
  temp.resize( widthIn * heightOut );
  for ( int i = 0; i < heightIn; i++ ) {
    for ( int j = 0; j < widthIn; j++ ) {
      temp[( 2 * i ) * widthIn + j] =
          upsamplingVertical0( chromaIn, widthIn, heightIn, i + 0, j );
      temp[( 2 * i + 1 ) * widthIn + j] =
          upsamplingVertical1( chromaIn, widthIn, heightIn, i + 1, j );
    }
  }
  for ( int i = 0; i < heightOut; i++ ) {
    for ( int j = 0; j < widthIn; j++ ) {
      chromaOut[i * widthOut + j * 2] =
          upsamplingHorizontal0( temp, widthIn, i, j + 0 );
      chromaOut[i * widthOut + j * 2 + 1] =
          upsamplingHorizontal1( temp, widthIn, i, j + 1 );
    }
  }
}


void floatYUVToYUV( const std::vector<float>& src,
                                                  std::vector<uint16_t>&           dst,
                                                  const bool                chroma,
                                                  const size_t              nbyte )  {
  size_t count = src.size();
  dst.resize( count );
  double offset = chroma ? nbyte == 1 ? 128. : 32768. : 0;
  double scale  = nbyte == 1 ? 255. : 65535.;

  for ( size_t i = 0; i < count; i++ ) {
    dst[i] = static_cast<uint16_t>( fClip( std::round( (float)( scale * (double)src[i] + offset ) ), 0.f, (float)scale ) );
  }
}

void convertYUV420ToYUV444TMC2Frame( picture& picSrc, picture& picDst ) {

    size_t width = picSrc.width;
    size_t height = picSrc.height;
    picDst.width = width;
    picDst.height = height;
    picDst.format = PCCCOLORFORMAT::YUV444;

    picDst.Y.resize(width * height);
    picDst.U.resize(width * height);
    picDst.V.resize(width * height);
    size_t             widthChroma  = width / 2;
    size_t             heightChroma = height / 2;
    std::vector<float> YUV444[3], YUV420[3];
    YUVtoFloatYUV( picSrc.Y, YUV420[0], 0);
    YUVtoFloatYUV( picSrc.U, YUV420[1], 1);
    YUVtoFloatYUV( picSrc.V, YUV420[2], 1);
    upsampling( YUV420[1], YUV444[1], widthChroma, heightChroma);
    upsampling( YUV420[2], YUV444[2], widthChroma, heightChroma);
    
    // floatYUVToYUV( YUV420[0], picDst.Y, 0, 2 );
    // floatYUVToYUV( YUV444[1], picDst.U, 1, 2 );
    // floatYUVToYUV( YUV444[2], picDst.V, 1, 2 );

    floatYUVToYUV( YUV420[0], picDst.Y16, 0, 2 );
    floatYUVToYUV( YUV444[1], picDst.U16, 1, 2 );
    floatYUVToYUV( YUV444[2], picDst.V16, 1, 2 );    

}



void convertYUV420ToYUV444TMC2Video(std::vector<video_map>& attribute_maps ) {
  

    video_map videoSrc = attribute_maps[0];
    video_map videoDst;
    videoDst.pictures.resize( videoSrc.frame_count );
    videoDst.height = videoSrc.height;
    videoDst.width = videoSrc.width;

    for ( size_t i = 0; i < videoSrc.frame_count; i++ ) {
        convertYUV420ToYUV444TMC2Frame( videoSrc.pictures[i], videoDst.pictures[i]);
    }

    attribute_maps[0] = videoDst;
}





void Decompression::decompress_v3c_video_unit(V3C_UNIT_TYPE vuh_t, uint8_t* buf, std::shared_ptr<uvgvpcc_dec::composition_unit> in_cu, std::shared_ptr<decompressed_cu> out_cu)
{    
    if(vuh_t == V3C_OVD) {
        occupancy_codec_mtx_.lock();
        std::cerr << "OCCUPANCY MAP" << std::endl;
            /* ------------------ OCCUPANCY ------------------ */
        size_t occupancy_payload_start = in_cu->ovd_start + 4;
        size_t occupancy_payload_size = in_cu->ovd_size - 4;
        decompress_video_sub_bitstream(buf, occupancy_payload_start, occupancy_payload_size,
            &out_cu->occupancy_map, occupancy_codec_ctx_);

        out_cu->occupancy_boolean_maps.resize(out_cu->occupancy_map.frame_count);
        out_cu->block_to_patches.resize(out_cu->cu_frame_count);
        
        occupancy_codec_mtx_.unlock();

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Nominal format", "Occupancy map frame count = "
    + std::to_string(out_cu->occupancy_map.frame_count) + " (" + std::to_string(out_cu->occupancy_map.width)
    + "x" + std::to_string(out_cu->occupancy_map.height) + ") \n" + "Channels (frame 0) Y="
    + std::to_string(out_cu->occupancy_map.pictures.front().Y.size()) + " U=" +
    std::to_string(out_cu->occupancy_map.pictures.front().U.size())
    + " V=" + std::to_string(out_cu->occupancy_map.pictures.front().V.size()) + "\n");

        if (out_cu->occupancy_map.pictures.front().format == PCCCOLORFORMAT::YUV420) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Note", "Occupancy map in YUV420 (nominal format would be 444) \n");
        }
    }
    else if (vuh_t == V3C_GVD) {
        geometry_codec_mtx_.lock();
        std::cerr << "GEOMETRY MAP" << std::endl;
        /* ------------------ GEOMETRY ------------------ */
        size_t geometry_payload_start = in_cu->gvd_start + 4;
        size_t geometry_payload_size = in_cu->gvd_size - 4;
        video_map new_geo_map;
        out_cu->geometry_maps.push_back(new_geo_map);
        decompress_video_sub_bitstream(buf, geometry_payload_start, geometry_payload_size,
        &out_cu->geometry_maps.back(), geometry_codec_ctx_);

        geometry_codec_mtx_.unlock();

        for (size_t i = 0; i < out_cu->geometry_maps.size(); i++) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Nominal format", "Geometry map [" + std::to_string(i)
                + "] frame count = " + std::to_string(out_cu->geometry_maps.at(i).frame_count) 
                + " (" + std::to_string(out_cu->geometry_maps.at(i).width) + "x" + std::to_string(out_cu->geometry_maps.at(i).height) + ") \n"
                + "Channels (frame 0) Y=" + std::to_string(out_cu->geometry_maps.at(i).pictures.front().Y.size())
                + " U=" + std::to_string(out_cu->geometry_maps.at(i).pictures.front().U.size())
                + " V=" + std::to_string(out_cu->geometry_maps.at(i).pictures.front().V.size()) + "\n");
            if (out_cu->geometry_maps.at(i).pictures.front().format == PCCCOLORFORMAT::YUV420) {
                uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Note", "Geometry map in YUV420 (nominal format would be 444) \n");
            }
        }
    }
    else if (vuh_t == V3C_AVD) {
        attribute_codec_mtx_.lock();
        std::cerr << "ATTRIBUTE MAP" << std::endl;
        /* ------------------ ATTRIBUTE ------------------ */
        size_t attribute_payload_start = in_cu->avd_start + 4;
        size_t attribute_payload_size = in_cu->avd_size - 4;
        video_map new_atr_map;
        out_cu->attribute_maps.push_back(new_atr_map);
        decompress_video_sub_bitstream(buf, attribute_payload_start, attribute_payload_size,
        &out_cu->attribute_maps.back(), attribute_codec_ctx_);

        attribute_codec_mtx_.unlock();


        // lf : convert from YUV420 to YUV444 //
        if(dec_context1_->p_->useTMC2AttributeYUVConversion) {
            convertYUV420ToYUV444TMC2Video(out_cu->attribute_maps);
        } else {
            convertYUV420ToYUV444uvgVPCCSimpleVideo(out_cu->attribute_maps);
        }
        




        /////////////////////////////////////////




        for (size_t i = 0; i < out_cu->attribute_maps.size(); i++) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Nominal format", "Attribute map [" + std::to_string(i)
                + "] frame count = " + std::to_string(out_cu->attribute_maps.at(i).frame_count) 
                + " (" + std::to_string(out_cu->attribute_maps.at(i).width) + "x" + std::to_string(out_cu->attribute_maps.at(i).height) + ") \n"
                + "Channels (frame 0) Y=" + std::to_string(out_cu->attribute_maps.at(i).pictures.front().Y.size())
                + " U=" + std::to_string(out_cu->attribute_maps.at(i).pictures.front().U.size())
                + " V=" + std::to_string(out_cu->attribute_maps.at(i).pictures.front().V.size()) + "\n");
            if (out_cu->attribute_maps.at(i).pictures.front().format == PCCCOLORFORMAT::YUV420) {
                uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Note", "Attribute map in YUV420 (nominal format would be 444) \n");
                std::cerr << "LF LOG : YUV420" << std::endl;
            } else {
                std::cerr << "LF LOG : YUV444" << std::endl;
            }
        }
    }
}

void Decompression::decompress_vps(const size_t location, const size_t gof_index)
{
    bitstream_position ptr;
    ptr.bytes = location;

    parameter_sets new_params;
    new_params.gof_index = gof_index;
    saved_params_[gof_index] = new_params;
    read_v3c_parameter_set(&saved_params_[gof_index].vps, ptr);
}

void Decompression::read_v3c_parameter_set(v3c_parameter_set* vps, bitstream_position &ptr)
{
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Reading V3C parameter set \n");
    // profile_tier_level
    read_profile_tier_level(&vps->ptl, ptr);
    vps->vps_v3c_parameter_set_id = read(4, ptr, "vps_v3c_parameter_set_id");
    uint8_t vps_reserved_zero_8bits = read(8, ptr, "vps_reserved_zero_8bits");
    vps->vps_atlas_count_minus1 = read(6, ptr, "vps_atlas_count_minus1");

    /* Resize vectors */
    vps->vps_atlas_id.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_frame_width.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_frame_height.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_map_count_minus1.resize(vps->vps_atlas_count_minus1 + 1);

    vps->vps_multiple_map_streams_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_map_absolute_coding_enabled_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_auxiliary_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_occupancy_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_geometry_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);
    vps->vps_attribute_video_present_flag.resize(vps->vps_atlas_count_minus1 + 1);

    vps->occupancy_info.resize(vps->vps_atlas_count_minus1 + 1);
    vps->geometry_info.resize(vps->vps_atlas_count_minus1 + 1);
    vps->attribute_info.resize(vps->vps_atlas_count_minus1 + 1);

    for (uint8_t j = 0; j < (vps->vps_atlas_count_minus1 + 1); j++) {
        vps->vps_atlas_id.at(j) = read(6, ptr, "vps_atlas_id");
        vps->vps_frame_width.at(j) = read_ue(ptr, "vps_frame_width");
        vps->vps_frame_height.at(j) = read_ue(ptr, "vps_frame_height");
        vps->vps_map_count_minus1.at(j) = read(4, ptr, "vps_map_count_minus1");

        if (vps->vps_map_count_minus1.at(j) > 0) {
            vps->vps_multiple_map_streams_present_flag.at(j) = read(1, ptr, "vps_multiple_map_streams_present_flag");
        }
        vps->vps_map_absolute_coding_enabled_flag.at(j).resize(vps->vps_map_count_minus1.at(j) + 1);
        for (uint8_t i = 1; i <= vps->vps_map_count_minus1.at(j); i++) {
            if(vps->vps_multiple_map_streams_present_flag.at(j)) {
                vps->vps_map_absolute_coding_enabled_flag.at(j).at(i) = read(1, ptr, "vps_map_absolute_coding_enabled_flag");
            }
            else {
                vps->vps_map_absolute_coding_enabled_flag.at(j).at(i) = true;
            }
        }
        vps->vps_auxiliary_video_present_flag.at(j) = read(1, ptr, "vps_auxiliary_video_present_flag");
        vps->vps_occupancy_video_present_flag.at(j) = read(1, ptr, "vps_occupancy_video_present_flag");
        vps->vps_geometry_video_present_flag.at(j) = read(1, ptr, "vps_geometry_video_present_flag");
        vps->vps_attribute_video_present_flag.at(j) = read(1, ptr, "vps_attribute_video_present_flag");

        if (vps->vps_occupancy_video_present_flag.at(j)) {
            vps->occupancy_info.at(j).oi_occupancy_codec_id = read(8, ptr, "oi_occupancy_codec_id");
            vps->occupancy_info.at(j).oi_lossy_occupancy_compression_threshold = read(8, ptr, "oi_lossy_occupancy_compression_threshold");
            vps->occupancy_info.at(j).oi_occupancy_2d_bit_depth_minus1 = read(5, ptr, "oi_occupancy_2d_bit_depth_minus1");
            vps->occupancy_info.at(j).oi_occupancy_MSB_align_flag = read(1, ptr, "oi_occupancy_MSB_align_flag");
        }
        
        if (vps->vps_geometry_video_present_flag.at(j)) {
            vps->geometry_info.at(j).gi_geometry_codec_id = read(8, ptr, "gi_geometry_codec_id");
            vps->geometry_info.at(j).gi_geometry_2d_bit_depth_minus1 = read(5, ptr, "gi_geometry_2d_bit_depth_minus1");
            vps->geometry_info.at(j).gi_geometry_MSB_align_flag = read(1, ptr, "gi_geometry_MSB_align_flag");
            vps->geometry_info.at(j).gi_geometry_3d_coordinates_bit_depth_minus1 = read(5, ptr, "gi_geometry_3d_coordinates_bit_depth_minus1");
            
            if (vps->vps_auxiliary_video_present_flag.at(j)) {
                vps->geometry_info.at(j).gi_auxiliary_geometry_codec_id = read(8, ptr, "gi_auxiliary_geometry_codec_id");
            }
        }

        if (vps->vps_attribute_video_present_flag.at(j)) {
            vps->ai_attribute_count = read(7, ptr, "ai_attribute_count");

            vps->attribute_info.at(j).ai_attribute_type_id.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_codec_id.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_auxiliary_attribute_codec_id.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_map_absolute_coding_persistence_flag.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_dimension_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_2d_bit_depth_minus1.resize(vps->ai_attribute_count);
            vps->attribute_info.at(j).ai_attribute_MSB_align_flag.resize(vps->ai_attribute_count);


            for (uint8_t i = 0; i < vps->ai_attribute_count; ++i) {
                vps->attribute_info.at(j).ai_attribute_type_id.at(i) = read(4, ptr, "ai_attribute_type_id");
                vps->attribute_info.at(j).ai_attribute_codec_id.at(i) = read(8, ptr, "ai_attribute_codec_id");

                if(vps->vps_auxiliary_video_present_flag.at(j)) {
                    vps->attribute_info.at(j).ai_auxiliary_attribute_codec_id.at(i) = read(8, ptr, "ai_auxiliary_attribute_codec_id");
                }
                if(vps->vps_map_count_minus1.at(j) > 0) {
                    vps->attribute_info.at(j).ai_attribute_map_absolute_coding_persistence_flag.at(i) = read(1, ptr, "ai_attribute_map_absolute_coding_persistence_flag");
                }
                
                vps->attribute_info.at(j).ai_attribute_dimension_minus1.at(i) = read(6, ptr, "ai_attribute_dimension_minus1");
                uint8_t d = vps->attribute_info.at(j).ai_attribute_dimension_minus1.at(i);

                uint8_t m;
                if(d == 0) {
                    m = 0;
                    vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i) = 0;
                }
                else {
                    vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i) = read(6, ptr, "ai_attribute_dimension_partitions_minus1");
                    m = vps->attribute_info.at(j).ai_attribute_dimension_partitions_minus1.at(i);
                }
                vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).resize(vps->attribute_info.at(j).ai_attribute_dimension_minus1.at(i));
                uint16_t n = 0;
                for (uint8_t k = 0; k < m; k++) {
                    if(k + d == m) {
                        n = 0;
                        vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = 0;
                    }
                    else {
                        vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k) = read_ue(ptr, "ai_attribute_partition_channels_minus1");
                        n = vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(k);
                    }
                    d -= n + 1;
                }
                vps->attribute_info.at(j).ai_attribute_partition_channels_minus1.at(i).at(m) = d;
                vps->attribute_info.at(j).ai_attribute_2d_bit_depth_minus1.at(i) = read(5, ptr, "ai_attribute_2d_bit_depth_minus1");
                vps->attribute_info.at(j).ai_attribute_MSB_align_flag.at(i) = read(1, ptr, "ai_attribute_MSB_align_flag");
            }
        }
        vps->vps_extension_present_flag = read(1, ptr, "vps_extension_present_flag");

        if(vps->vps_extension_present_flag) {
            vps->vps_packing_information_present_flag = read(1, ptr, "vps_packing_information_present_flag");
            vps->vps_miv_extension_present_flag = read(1, ptr, "vps_miv_extension_present_flag");
            vps->vps_extension_6bits = read(6, ptr, "vps_extension_6bits");
        }
        align_bitstream(ptr);

        // No packing information
        // No MIV extension
        // No VPS extension
        // TODO: Do we need to align at all when reading?
        //uvg_bitstream_align(stream);
    }
}

void Decompression::read_profile_tier_level(profile_tier_level* ptl, bitstream_position &ptr)
{
    ptl->ptl_profile_toolset_idc = read(8, ptr, "ptl_profile_toolset_idc");
    ptl->ptl_tier_flag = read(1, ptr, "ptl_tier_flag");
    ptl->ptl_profile_codec_group_idc = read(7, ptr, "ptl_profile_codec_group_idc");
    ptl->ptl_profile_reconstruction_idc = read(8, ptr, "ptl_profile_reconstruction_idc");
    uint16_t ptl_reserved_zero_16bits = read(16, ptr, "ptl_reserved_zero_16bits");
    ptl->ptl_max_decodes_idc = read(4, ptr, "ptl_max_decodes_idc");
    uint16_t ptl_reserved_0xfff_12bits = read(12, ptr, "ptl_reserved_0xfff_12bits");
    ptl->ptl_level_idc = read(8, ptr, "ptl_level_idc");
    ptl->ptl_num_sub_profiles = read(6, ptr, "ptl_num_sub_profiles");
    ptl->ptl_extended_sub_profile_flag = read(1, ptr, "ptl_extended_sub_profile_flag");
    ptl->ptl_toolset_constraints_present_flag = read(1, ptr, "ptl_toolset_constraints_present_flag");

    if(ptl->ptl_toolset_constraints_present_flag) {
        throw std::runtime_error("Handling PTL toolset constraints not implemented");
        return;
    }
    // TODO: Parse PTC
}

// lf : name of this function in TMC2 : PCCBitstreamReader::atlasSequenceParameterSetRbsp( AtlasSequenceParameterSetRbsp& asps,PCCHighLevelSyntax& syntax,PCCBitstream& bitstream )
void Decompression::read_asps(atlas_sequence_parameter_set &asps, bitstream_position &ptr)
{
    asps.asps_atlas_sequence_parameter_set_id = read_ue(ptr, "asps_atlas_sequence_parameter_set_id");
    asps.asps_frame_width = read_ue(ptr, "asps_frame_width");
    asps.asps_frame_height = read_ue(ptr, "asps_frame_height");
    asps.asps_geometry_3d_bit_depth_minus1 = read(5, ptr, "asps_geometry_3d_bit_depth_minus1");
    asps.asps_geometry_2d_bit_depth_minus1 = read(5, ptr, "asps_geometry_2d_bit_depth_minus1");
    asps.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 = read_ue(ptr, "asps_log2_max_atlas_frame_order_cnt_lsb_minus4");
    asps.asps_max_dec_atlas_frame_buffering_minus1 = read_ue(ptr, "asps_max_dec_atlas_frame_buffering_minus1");

    asps.asps_long_term_ref_atlas_frames_flag = read(1, ptr, "asps_long_term_ref_atlas_frames_flag");
    asps.asps_num_ref_atlas_frame_lists_in_asps = read_ue(ptr, "asps_num_ref_atlas_frame_lists_in_asps");

    asps.ref_lists.resize(asps.asps_num_ref_atlas_frame_lists_in_asps);
    std::size_t Log2MaxAtlasFrmOrderCntLsb = asps.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    for( uint32_t rlsIdx = 0; rlsIdx < asps.asps_num_ref_atlas_frame_lists_in_asps; rlsIdx++ ) {
        ref_list_struct &ref = asps.ref_lists.at(rlsIdx);
        ref.num_ref_entries = read_ue(ptr, "num_ref_entries");

        ref.st_ref_atlas_frame_flag.resize(ref.num_ref_entries);
        ref.abs_delta_afoc_st.resize(ref.num_ref_entries);
        ref.straf_entry_sign_flag.resize(ref.num_ref_entries);
        ref.afoc_lsb_lt.resize(ref.num_ref_entries);

        for (uint32_t i = 0; i < ref.num_ref_entries; ++i) {
            if(asps.asps_long_term_ref_atlas_frames_flag) {
                ref.st_ref_atlas_frame_flag.at(i) = read(1, ptr, "st_ref_atlas_frame_flag");
            }
            if(ref.st_ref_atlas_frame_flag.at(i)) {
                ref.abs_delta_afoc_st.at(i) = read_ue(ptr, "abs_delta_afoc_st");
                if(ref.abs_delta_afoc_st.at(i) > 0) {
                    ref.straf_entry_sign_flag.at(i) = read(1, ptr, "straf_entry_sign_flag");
                }
            }
            else {
                ref.afoc_lsb_lt.at(i) = read(Log2MaxAtlasFrmOrderCntLsb, ptr, "afoc_lsb_lt");
            }
        }
    }
    asps.asps_use_eight_orientations_flag = read(1, ptr, "asps_use_eight_orientations_flag");
    asps.asps_extended_projection_enabled_flag = read(1, ptr, "asps_extended_projection_enabled_flag");

    if( asps.asps_extended_projection_enabled_flag ) {
        asps.asps_max_number_projections_minus1 = read_ue(ptr, "asps_max_number_projections_minus1");
    }

    asps.asps_normal_axis_limits_quantization_enabled_flag = read(1, ptr, "asps_normal_axis_limits_quantization_enabled_flag");
    asps.asps_normal_axis_max_delta_value_enabled_flag = read(1, ptr, "asps_normal_axis_max_delta_value_enabled_flag");
    asps.asps_patch_precedence_order_flag = read(1, ptr, "asps_patch_precedence_order_flag");
    asps.asps_log2_patch_packing_block_size = read(3, ptr, "asps_log2_patch_packing_block_size");
    asps.asps_patch_size_quantizer_present_flag = read(1, ptr, "asps_patch_size_quantizer_present_flag");
    asps.asps_map_count_minus1 = read(4, ptr, "asps_map_count_minus1");
    asps.asps_pixel_deinterleaving_enabled_flag = read(1, ptr, "asps_pixel_deinterleaving_enabled_flag");

    if(asps.asps_pixel_deinterleaving_enabled_flag) {
        asps.asps_map_pixel_deinterleaving_flag.resize(asps.asps_map_count_minus1 + 1);
        for (size_t j = 0; j < asps.asps_map_count_minus1; ++j) {
            asps.asps_map_pixel_deinterleaving_flag.at(j) = read(1, ptr, "asps_map_pixel_deinterleaving_flag");
        }
    }
    asps.asps_raw_patch_enabled_flag = read(1, ptr, "asps_raw_patch_enabled_flag");
    asps.asps_eom_patch_enabled_flag = read(1, ptr, "asps_eom_patch_enabled_flag");

    if(asps.asps_eom_patch_enabled_flag && asps.asps_map_count_minus1 == 0) {
        asps.asps_eom_fix_bit_count_minus1 = read(4, ptr, "asps_eom_fix_bit_count_minus1");
    }
    if (asps.asps_raw_patch_enabled_flag || asps.asps_eom_patch_enabled_flag) {
        asps.asps_auxiliary_video_enabled_flag = read(1, ptr, "asps_auxiliary_video_enabled_flag");
    }
    asps.asps_plr_enabled_flag = read(1, ptr, "asps_plr_enabled_flag");
    if( asps.asps_plr_enabled_flag ) {
        throw std::runtime_error("Handling ASPS PLR not implemented");
    }
    asps.asps_vui_parameters_present_flag = read(1, ptr, "asps_vui_parameters_present_flag");
    if( asps.asps_vui_parameters_present_flag ) {
        throw std::runtime_error("Handling ASPS VUI parameters not implemented");
    }
    asps.asps_extension_present_flag = read(1, ptr, "asps_extension_present_flag");

    if(asps.asps_extension_present_flag) {
        asps.asps_vpcc_extension_present_flag = read(1, ptr, "asps_vpcc_extension_present_flag");
        asps.asps_miv_extension_present_flag = read(1, ptr, "asps_miv_extension_present_flag");
        asps.asps_extension_6bits = read(6, ptr, "asps_extension_6bits");
    }
    if (asps.asps_vpcc_extension_present_flag) {
        asps.asps_vpcc_remove_duplicate_point_enabled_flag = read(1, ptr, "asps_vpcc_remove_duplicate_point_enabled_flag");
        if(asps.asps_pixel_deinterleaving_enabled_flag || asps.asps_plr_enabled_flag) {
            asps.asps_vpcc_surface_thickness_minus1 = read_ue(ptr, "asps_vpcc_surface_thickness_minus1");
        }
    }
    align_bitstream(ptr);
}

void Decompression::read_afps(atlas_frame_parameter_set &afps, bitstream_position &ptr)
{
    afps.afps_atlas_frame_parameter_set_id = read_ue(ptr, "afps_atlas_frame_parameter_set_id");
    afps.afps_atlas_sequence_parameter_set_id = read_ue(ptr, "afps_atlas_sequence_parameter_set_id");
    afps.afti.afti_single_tile_in_atlas_frame_flag = read(1, ptr, "afti_single_tile_in_atlas_frame_flag");
    afps.afti.afti_signalled_tile_id_flag = read(1, ptr, "afti_signalled_tile_id_flag");
    afps.afps_output_flag_present_flag = read(1, ptr, "afps_output_flag_present_flag");
    afps.afps_num_ref_idx_default_active_minus1 = read_ue(ptr, "afps_num_ref_idx_default_active_minus1");
    afps.afps_additional_lt_afoc_lsb_len = read_ue(ptr, "afps_additional_lt_afoc_lsb_len");
    afps.afps_lod_mode_enabled_flag = read(1, ptr, "afps_lod_mode_enabled_flag");
    afps.afps_raw_3d_offset_bit_count_explicit_mode_flag = read(1, ptr, "afps_raw_3d_offset_bit_count_explicit_mode_flag");
    afps.afps_extension_present_flag = read(1, ptr, "afps_extension_present_flag");
    afps.afps_miv_extension_present_flag = read(1, ptr, "afps_miv_extension_present_flag");
    afps.afps_extension_7bits = read(7, ptr, "afps_extension_7bits");

    align_bitstream(ptr);
}

void Decompression::read_atlas_rbsp(atlas_tile_layer_rbsp* rbsp, NAL_UNIT_TYPE nalu_t, bitstream_position &ptr)
{
    read_atlas_tile_header(rbsp->ath, nalu_t, ptr);
    read_atlas_tile_data_unit(rbsp->atdu, rbsp->ath, ptr);
    align_bitstream(ptr);
}

void Decompression::read_atlas_tile_header(atlas_tile_header &ath, NAL_UNIT_TYPE nalu_t, bitstream_position &ptr)
{
    if(nalu_t >= NAL_GBLA_W_LP && nalu_t <= NAL_RSV_IRAP_ACL_29) {
        ath.ath_no_output_of_prior_atlas_frames_flag = read(1, ptr, "ath_no_output_of_prior_atlas_frames_flag");
    }
    ath.ath_atlas_frame_parameter_set_id = read_ue(ptr, "ath_atlas_frame_parameter_set_id");
    ath.ath_atlas_adaptation_parameter_set_id = read_ue(ptr, "ath_atlas_adaptation_parameter_set_id");
    //ath.ath_id = read("ath_id"); TODO: Figure out the dynamic bit length for this field
    uint16_t tileID = ath.ath_id; // default 0
    ath.ath_type = static_cast<ATH_TYPE>(read_ue(ptr, "ath_type"));

    if(saved_params_.back().afps.afps_output_flag_present_flag) {
        ath.ath_atlas_output_flag = read(1, ptr, "ath_atlas_output_flag");
    }
    size_t Log2MaxAtlasFrmOrderCntLsb = saved_params_.back().asps.asps_log2_max_atlas_frame_order_cnt_lsb_minus4 + 4;
    ath.ath_atlas_frm_order_cnt_lsb = read(uint8_t(Log2MaxAtlasFrmOrderCntLsb), ptr, "ath_atlas_frm_order_cnt_lsb"); //u(v)

    if(saved_params_.back().asps.asps_num_ref_atlas_frame_lists_in_asps > 0) {
        ath.ath_ref_atlas_frame_list_asps_flag = read(1, ptr, "ath_ref_atlas_frame_list_asps_flag");
    }
    if(ath.ath_ref_atlas_frame_list_asps_flag == 0) {
        throw std::runtime_error("Using atlas ref frame lists not implemented");
        return;
    }
    else if (saved_params_.back().asps.asps_num_ref_atlas_frame_lists_in_asps > 1) {
        size_t bit_len = std::ceil(std::log2(saved_params_.back().asps.asps_num_ref_atlas_frame_lists_in_asps));
        ath.ath_ref_atlas_frame_list_idx = read(uint8_t(bit_len), ptr, "ath_ref_atlas_frame_list_idx");
    }

    size_t NumLtrAtlasFrmEntries = 0; // default value, ref list is from ASPS TODO: dynamic
    ath.ath_additional_afoc_lsb_present_flag.resize(NumLtrAtlasFrmEntries);
    ath.ath_additional_afoc_lsb_val.resize(NumLtrAtlasFrmEntries);
    for (size_t j = 0; j < NumLtrAtlasFrmEntries; j++) {
        ath.ath_additional_afoc_lsb_present_flag.at(j) = read(1, ptr, "ath_additional_afoc_lsb_present_flag");
        if(ath.ath_additional_afoc_lsb_present_flag.at(j)) {
            ath.ath_additional_afoc_lsb_val.at(j) = read(saved_params_.back().afps.afps_additional_lt_afoc_lsb_len, ptr, "ath_additional_afoc_lsb_val");
        }
    }
    if(ath.ath_type != SKIP_TILE) {
        if(saved_params_.back().asps.asps_normal_axis_limits_quantization_enabled_flag) {
            ath.ath_pos_min_d_quantizer = read(5, ptr, "ath_pos_min_d_quantizer");
            if(saved_params_.back().asps.asps_normal_axis_max_delta_value_enabled_flag) {
                ath.ath_pos_delta_max_d_quantizer = read(5, ptr, "ath_pos_delta_max_d_quantizer");
            }
        }
        if(saved_params_.back().asps.asps_patch_size_quantizer_present_flag) {
            ath.ath_patch_size_x_info_quantizer = read(3, ptr, "ath_patch_size_x_info_quantizer");
            ath.ath_patch_size_y_info_quantizer = read(3, ptr, "ath_patch_size_y_info_quantizer");
        }
        if(saved_params_.back().afps.afps_raw_3d_offset_bit_count_explicit_mode_flag) {
            size_t bit_len = std::floor(std::log2(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 + 1));
            ath.ath_raw_3d_offset_axis_bit_count_minus1 = read(uint8_t(bit_len), ptr, "ath_raw_3d_offset_axis_bit_count_minus1");
        }
        if( ath.ath_type == ATH_TYPE::P_TILE && NumLtrAtlasFrmEntries > 1 ) {
            ath.ath_num_ref_idx_active_override_flag = read(1, ptr, "ath_num_ref_idx_active_override_flag");
            if(ath.ath_num_ref_idx_active_override_flag) {
                ath.ath_num_ref_idx_active_minus1 = read_ue(ptr, "ath_num_ref_idx_active_minus1");
            }
        }
    }
    align_bitstream(ptr);
}

void Decompression::read_patch_information_data(atlas_tile_header &ath, patch_information_data &pid, bitstream_position &ptr)
{
    if (ath.ath_type == SKIP_TILE) {
        // skip mode: currently not supported but added it for convenience. Could
        // easily be removed
    } else if (ath.ath_type == P_TILE) {
        if (pid.patchMode == P_SKIP) {
            // skip mode: currently not supported but added it for convenience. Could
            // easily be removed
            //skipPatchDataUnit(bitstream);
        } else if (pid.patchMode == P_MERGE) {
            auto &mpdu = pid.merge;
            //mergePatchDataUnit(mpdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_INTRA) {
            auto& pdu = pid.patch;
            //patchDataUnit(pdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_INTER) {
            auto& ipdu = pid.inter;
            //interPatchDataUnit(ipdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_RAW) {
            auto& rpdu = pid.raw;
            //rawPatchDataUnit(rpdu, ath, syntax, bitstream);
        } else if (pid.patchMode == P_EOM) {
            auto& epdu = pid.eom;
            //eomPatchDataUnit(epdu, ath, syntax, bitstream);
        }
    }
    else if (ath.ath_type == I_TILE) { // currently only use I_TILE types
        if (pid.patchMode == I_INTRA) {
            auto& pdu = pid.patch;
            read_patch_data_unit(ath, pdu, ptr);
            //patchDataUnit(pdu, ath, syntax, bitstream);
        } else if (pid.patchMode == I_RAW) {
            auto& rpdu = pid.raw;
            //rawPatchDataUnit(rpdu, ath, syntax, bitstream);
        } else if (pid.patchMode == I_EOM) {
            auto& epdu = pid.eom;
            //eomPatchDataUnit(epdu, ath, syntax, bitstream);
        }
    }
}

void Decompression::read_patch_data_unit(atlas_tile_header &ath, patch_data_unit &pdu, bitstream_position &ptr)
{
    pdu.pdu_2d_pos_x = read_ue(ptr, "pdu_2d_pos_x");
    pdu.pdu_2d_pos_y = read_ue(ptr, "pdu_2d_pos_y");
    pdu.pdu_2d_size_x_minus1 = read_ue(ptr, "pdu_2d_size_x_minus1");
    pdu.pdu_2d_size_y_minus1 = read_ue(ptr, "pdu_2d_size_y_minus1");

    pdu.pdu_3d_offset_u = read(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 + 1, ptr, "pdu_3d_offset_u");
    pdu.pdu_3d_offset_v = read(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 + 1, ptr, "pdu_3d_offset_v");
    pdu.pdu_3d_offset_d = read(saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1 - ath.ath_pos_min_d_quantizer + 1, ptr, "pdu_3d_offset_d");

    if(saved_params_.back().asps.asps_normal_axis_max_delta_value_enabled_flag) {
        uint32_t rangeDBitDepth = std::min(saved_params_.back().asps.asps_geometry_2d_bit_depth_minus1, saved_params_.back().asps.asps_geometry_3d_bit_depth_minus1) + 1;
        pdu.pdu_3d_range_d = read(rangeDBitDepth - ath.ath_pos_delta_max_d_quantizer, ptr, "pdu_3d_range_d");
    }
    pdu.pdu_projection_id = read(ceil(log2(6)), ptr, "pdu_projection_id");
    pdu.pdu_orientation_index = read(false ? 3 : 1, ptr, "pdu_orientation_index");

    if(saved_params_.back().afps.afps_lod_mode_enabled_flag) {
        pdu.pdu_lod_enabled_flag = read(1, ptr, "pdu_lod_enabled_flag");
        if(pdu.pdu_lod_enabled_flag) {

            pdu.pdu_lod_scale_x_minus1 = read_ue(ptr, "pdu_lod_scale_x_minus1");
            pdu.pdu_lod_scale_y_idc = read_ue(ptr, "pdu_lod_scale_y_idc");
        }
    }
    // if( asps_plr_enabled_flag )               == false
    // if( asps_miv_extension_present_flag )     == false
}


void Decompression::read_atlas_tile_data_unit(atlas_tile_data_unit &atdu, atlas_tile_header &ath, bitstream_position &ptr)
{
    uint16_t tileID = ath.ath_id;
    if (ath.ath_type == SKIP_TILE) {
        //skipPatchDataUnit(bitstream);
        // This is just empty?
    }
    else {
        while (true ) {
            atdu.atdu_patch_mode = read_ue(ptr, "atdu_patch_mode");
            if(atdu.atdu_patch_mode == ATDU_PATCH_MODE_I_TILE::I_END
                || atdu.atdu_patch_mode == ATDU_PATCH_MODE_P_TILE::P_END) {
                break;
            }
            patch_information_data pid;
            pid.patchMode = atdu.atdu_patch_mode;
            read_patch_information_data(ath, pid, ptr);
            atdu.pid_vec.push_back(pid);

        }
    }
}

// lf addition, from TMC2 //

// bool Decompression::moreData(bitstream_position &ptr) {
    
//     return position_.bytes_ < data_.size(); }

// bool moreRbspData( bitstream_position &ptr ) {
//   // Return false if there is no more data.
//   if (!bitstream.moreData()) {
//     return false;
//   }

//   // Store bitstream state.
//   auto position = bitstream.getPosition();


//   // Skip first bit. It may be part of a RBSP or a rbsp_one_stop_bit.
//   bitstream.read( 1 );

//   while ( bitstream.moreData() ) {
//     if ( bitstream.read( 1 ) ) {
//       // We found a one bit beyond the first bit. Restore bitstream state and return true.
//       bitstream.setPosition( position );
//       return true;
//     }
//   }

//   // We did not found a one bit beyond the first bit. Restore bitstream state and return false.
//   bitstream.setPosition( position );
//   return false;
// }

// F.2  SEI payload syntax
// F.2.1  General SEI message syntax



void Decompression::seiPayload( bitstream_position &ptr,
                                     pcc::NalUnitType         nalUnitType,
                                     pcc::SeiPayloadType      payloadType,
                                     size_t              payloadSize,
                                     pcc::PCCSEI&             seiList ) {


  (void)ptr;
  (void)payloadSize;

  pcc::SEI& sei = seiList.addSei( nalUnitType, payloadType );
//   printf( "        seiMessage: type = %d %s payloadSize = %zu \n", payloadType, toString( payloadType ).c_str(), payloadSize );
//   fflush( stdout );
  if ( nalUnitType == pcc::NAL_PREFIX_ESEI || nalUnitType == pcc::NAL_PREFIX_NSEI ) {
    // if ( payloadType == pcc::BUFFERING_PERIOD ) {  // 0
    //   bufferingPeriod( bitstream, sei );
    // } else if ( payloadType == pcc::ATLAS_FRAME_TIMING ) {  // 1
    //   assert( seiList.seiIsPresent( pcc::NAL_PREFIX_NSEI, pcc::BUFFERING_PERIOD ) );
    //   auto& bpsei = *seiList.getLastSei( pcc::NAL_PREFIX_NSEI, pcc::BUFFERING_PERIOD );
    //   atlasFrameTiming( bitstream, sei, bpsei, false );
    // } else if ( payloadType == pcc::FILLER_PAYLOAD ) {  // 2
    //   fillerPayload( bitstream, sei, payloadSize );
    // } else if ( payloadType == pcc::USER_DATAREGISTERED_ITUTT35 ) {  // 3
    //   userDataRegisteredItuTT35( bitstream, sei, payloadSize );
    // } else if ( payloadType == pcc::USER_DATA_UNREGISTERED ) {  // 4
    //   userDataUnregistered( bitstream, sei, payloadSize );
    // } else if ( payloadType == pcc::RECOVERY_POINT ) {  // 5
    //   recoveryPoint( bitstream, sei );
    // } else if ( payloadType == pcc::NO_RECONSTRUCTION ) {  // 6
    //   noReconstruction( bitstream, sei );
    // } else if ( payloadType == pcc::TIME_CODE ) {  // 7
    //   timeCode( bitstream, sei );
    // } else if ( payloadType == pcc::SEI_MANIFEST ) {  // 8
    //   seiManifest( bitstream, sei );
    // } else if ( payloadType == pcc::SEI_PREFIX_INDICATION ) {  // 9
    //   seiPrefixIndication( bitstream, sei );
    // } else if ( payloadType == pcc::ACTIVE_SUB_BITSTREAMS ) {  // 10
    //   activeSubBitstreams( bitstream, sei );
    // } else if ( payloadType == pcc::COMPONENT_CODEC_MAPPING ) {  // 11
    //   componentCodecMapping( bitstream, sei );
    // } else if ( payloadType == pcc::SCENE_OBJECT_INFORMATION ) {  // 12
    //   sceneObjectInformation( bitstream, sei );
    // } else if ( payloadType == pcc::OBJECT_LABEL_INFORMATION ) {  // 13
    //   objectLabelInformation( bitstream, sei );
    // } else if ( payloadType == pcc::PATCH_INFORMATION ) {  // 14
    //   patchInformation( bitstream, sei );
    // } else if ( payloadType == pcc::VOLUMETRIC_RECTANGLE_INFORMATION ) {  // 15
    //   volumetricRectangleInformation( bitstream, sei );
    // } else if ( payloadType == pcc::ATLAS_OBJECT_INFORMATION ) {  // 16
    //   atlasObjectInformation( bitstream, sei );
    // } else if ( payloadType == pcc::VIEWPORT_CAMERA_PARAMETERS ) {  // 17
    //   viewportCameraParameters( bitstream, sei );
    // } else if ( payloadType == pcc::VIEWPORT_POSITION ) {  // 18
    //   viewportPosition( bitstream, sei );
    // } else if ( payloadType == pcc::ATTRIBUTE_TRANSFORMATION_PARAMS ) {  // 64
    //   attributeTransformationParams( bitstream, sei );
    // } else if ( payloadType == pcc::OCCUPANCY_SYNTHESIS ) {  // 65
    //   occupancySynthesis( bitstream, sei );
    // } else if ( payloadType == pcc::GEOMETRY_SMOOTHING ) {  // 66
    //   geometrySmoothing( bitstream, sei );
    // } else if ( payloadType == pcc::ATTRIBUTE_SMOOTHING ) {  // 67
    //   attributeSmoothing( bitstream, sei );
    // } else {
    //   reservedSeiMessage( bitstream, sei, payloadSize );
    // }
  } else {
    // if ( payloadType == pcc::FILLER_PAYLOAD ) {  // 2
    //   fillerPayload( bitstream, sei, payloadSize );
    // } else if ( payloadType == pcc::USER_DATAREGISTERED_ITUTT35 ) {  // 3
    //   userDataRegisteredItuTT35( bitstream, sei, payloadSize );
    // } else if ( payloadType == pcc::USER_DATA_UNREGISTERED ) {  // 4
    //   userDataUnregistered( bitstream, sei, payloadSize );
    // } else if ( payloadType == pcc::DECODED_ATLAS_INFORMATION_HASH ) {  // 21
    //   decodedAtlasInformationHash( bitstream, sei );
    // } else {
    //   reservedSeiMessage( bitstream, sei, payloadSize );
    // }
  }
//   if ( moreDataInPayload( bitstream ) ) {
//     if ( payloadExtensionPresent( bitstream ) ) {
//       bitstream.read( 1 );  // u(v)
//     }
//     byteAlignment( bitstream );
//   }
}




// 8.3.8 Supplemental enhancement information message syntax
void Decompression::seiMessage( bitstream_position &ptr,
                                     NAL_UNIT_TYPE         nalUnitType,
                                     pcc::PCCSEI&             sei ) {
  int32_t payloadType = 0;
  int32_t payloadSize = 0;
  int32_t byte        = 0;
  do {
    // byte = bitstream.read( 8 );  // u(8)
    byte = read(8, ptr);

    payloadType += byte;
  } while ( byte == 0xff );
  do {
    // byte = bitstream.read( 8 );  // u(8)
    byte = read(8, ptr);
    payloadSize += byte;
  } while ( byte == 0xff );
  seiPayload( ptr, static_cast<pcc::NalUnitType>(nalUnitType), static_cast<pcc::SeiPayloadType>( payloadType ), payloadSize, sei );
}

bool byteAligned( bitstream_position &ptr) { return ( ptr.bits == 0 ); }

// 8.3.6.10 RBSP trailing bit syntax
void Decompression::rbspTrailingBits( bitstream_position &ptr) {
    // bitstream.read( 1 );  // f(1): equal to 1
    read(1, ptr, "rbspTrailingBits");

  while ( !byteAligned(ptr) ) {
    // bitstream.read( 1 );  // f(1): equal to 0
    read(1, ptr, "rbspTrailingBitsBis");
  }
}

// 8.3.6.4  Supplemental enhancement information Rbsp
void Decompression::seiRbsp( 
                                  bitstream_position &ptr,
                                  NAL_UNIT_TYPE         nalUnitType,
                                  pcc::PCCSEI&             sei ) {
    // do {
    seiMessage( ptr, nalUnitType, sei );
        // lf : I don't know how to find the size of the bitstream, so let's assume there is no more Rbsp Data : it might be VPS length in the parameter member of the current class Decompression
    // } while ( moreRbspData( bitstream ) );  
    rbspTrailingBits( ptr );
}

// end : lf addition, from TMC2 //




void Decompression::read_atlas_nal_unit(NAL_UNIT_TYPE nal_unit_type, std::size_t nal_unit_size, decompressed_cu* output, bitstream_position &ptr)
{


    pcc::PCCSEI prefixSEITemp; // lf addition, currently we don't have a PCCHighLevelSyntax to store this sei list 



    switch(nal_unit_type) {
        case NAL_UNIT_TYPE::NAL_ASPS:
            read_asps(saved_params_.back().asps, ptr);
            break;
        case NAL_UNIT_TYPE::NAL_AFPS:
            read_afps(saved_params_.back().afps, ptr);
            break;
        case NAL_UNIT_TYPE::NAL_IDR_N_LP: {
            atlas_tile_layer_rbsp rbsp;
            read_atlas_rbsp(&rbsp, nal_unit_type, ptr);
            auto frame = std::make_unique<atlas_frame>();
            frame.get()->atlas_index = 0;
            decode_atlas_frame(frame.get(), rbsp);
            output->atlas_map.push_back(std::move(frame));
            output->cu_frame_count++;
            break; }
        case NAL_UNIT_TYPE::NAL_EOB: {
            // No payload in EOB NAL unit
            break; }

        case NAL_UNIT_TYPE::NAL_TRAIL_N: {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::WARNING>("Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " (NAL_TRAIL_N -> Coded tile of a non-TSA, non STSA trailing atlas frame, atlas_tile_layer_rbsp() ).\n");
            break;
        }
        case NAL_UNIT_TYPE::NAL_TRAIL_R: {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::WARNING>("Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " (NAL_TRAIL_R -> Coded tile of a non-TSA, non STSA trailing atlas frame, atlas_tile_layer_rbsp() ).\n");
            break;
        }        
        case NAL_UNIT_TYPE::NAL_PREFIX_NSEI: {
            seiRbsp( ptr, nal_unit_type, prefixSEITemp );            
            // seiRbsp( ssnuBitstream, nalu.getType(), prefixSEI );            
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::WARNING>("Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " (NAL_PREFIX_NSEI -> Non-essential supplemental enhancement information sei_rbsp() ).\n");
            break;
        }
        case NAL_UNIT_TYPE::NAL_PREFIX_ESEI: {
            // seiRbsp(  ssnuBitstream, nalu.getType(), prefixSEI );            
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::WARNING>("Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " (NAL_PREFIX_ESEI -> Essential supplemental enhancement information, sei_rbsp() ).\n");
            break;
        }
        case NAL_UNIT_TYPE::NAL_RSV_ACL_32: {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::WARNING>("Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " (NAL_RSV_ACL_32 -> Reserved non-IRAP ACL NAL unit types).\n");
            break;
        }
        default: 
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::ERROR>("Decompression", "Unsupported Atlas NAL type " + std::to_string(nal_unit_type) + " \n");
            advance_bitstream(nal_unit_size * 8 - 16, ptr); // skip the rest of NAL unit for now 
            break;
    }
}

void Decompression::decompress_atlas_sub_bitstream(const size_t v3c_payload_size_bytes, decompressed_cu* output, const size_t location)
{
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Reading V3C atlas data, size "
        + std::to_string(v3c_payload_size_bytes) + ", location " + std::to_string(location) + " \n");

    bitstream_position ptr;
    ptr.bytes = location;

    std::size_t end_ptr = ptr.bytes + v3c_payload_size_bytes;
    //advance_bitstream(v3c_payload_size_bytes * 8);
    // 3 bits for nAL unit size precision - 1 and 5 reserved
    uint8_t nal_size_precision_bytes = read(3, ptr, "nal_size_precision_minus_1") + 1;
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "NAL size precision " + std::to_string(nal_size_precision_bytes) + " \n");
    std::size_t nal_unit_precision_bits = nal_size_precision_bytes * 8;

    advance_bitstream(5, ptr);

    while (true) {
        if (ptr.bytes >= end_ptr) {
            uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "End of atlas sub-bitstream at " + std::to_string(ptr.bytes) + " \n");
            break;
        }
        std::size_t nal_unit_size = read(nal_unit_precision_bits, ptr, "nal unit size");

        // Inside nal unit now        
        read(1, ptr, "nal_forbidden_zero_bit");
        NAL_UNIT_TYPE nal_unit_type = static_cast<NAL_UNIT_TYPE>(read(6, ptr, "nal_unit_type"));
        uint8_t nal_layer_id = read(6, ptr, "nal_layer_id");
        uint8_t nal_temporal_id_plus1 = read(3, ptr, "nal_temporal_id_plus1");
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Current NAL unit location " + std::to_string(ptr.bytes) + ", size "
            + std::to_string(nal_unit_size) + ", type " + std::to_string(nal_unit_type) +  " \n");

        read_atlas_nal_unit(nal_unit_type, nal_unit_size, output, ptr);
    }
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Number of expected point cloud frames in composition unit: "
        + std::to_string(output->cu_frame_count) + " \n");
}

void Decompression::decode_atlas_frame(atlas_frame* frame, const atlas_tile_layer_rbsp &rbsp)
{
    // 1 tile per frame
    frame->frame_width = saved_params_.back().asps.asps_frame_width;
    frame->frame_height = saved_params_.back().asps.asps_frame_height;

    std::size_t pid_count = rbsp.atdu.pid_vec.size();

    const size_t minLevel = pow( 2., double(rbsp.ath.ath_pos_min_d_quantizer)); // this line from TMC2
    int32_t quantizerSizeX = 1 << rbsp.ath.ath_patch_size_x_info_quantizer; // ath.getPatchSizeXinfoQuantizer(); // tmc2
    int32_t quantizerSizeY = 1 << rbsp.ath.ath_patch_size_y_info_quantizer; //ath.getPatchSizeYinfoQuantizer(); // tmc2
    int32_t packingBlockSize       = 1 << saved_params_.back().asps.asps_log2_patch_packing_block_size;
    double  packingBlockSizeD      = static_cast<double>( packingBlockSize );

    for(std::size_t i = 0; i < pid_count; ++i) {
        const patch_data_unit &pdu = rbsp.atdu.pid_vec.at(i).patch;
        patch p;
        p.occupancy_resolution = size_t(1) << saved_params_.back().asps.asps_log2_patch_packing_block_size;
        p.TilePatch2dPosX = pdu.pdu_2d_pos_x;
        p.TilePatch2dPosY = pdu.pdu_2d_pos_y;
        p.TilePatch3dOffsetU = pdu.pdu_3d_offset_u;
        p.TilePatch3dOffsetV = pdu.pdu_3d_offset_v;

        bool lodEnableFlag = pdu.pdu_lod_enabled_flag;
        if ( lodEnableFlag ) {
            p.TilePatchLoDScaleX = pdu.pdu_lod_scale_x_minus1 + 1;
            p.TilePatchLoDScaleY = pdu.pdu_lod_scale_y_idc + p.TilePatchLoDScaleX > 1 ? 1 : 2;
        } else {
            p.TilePatchLoDScaleX = 1;
            p.TilePatchLoDScaleY = 1;
        }
        p.TilePatch3dRangeD = pdu.pdu_3d_range_d == 0 ? 0 : (pdu.pdu_3d_range_d * minLevel - 1);
        if ( saved_params_.back().asps.asps_patch_size_quantizer_present_flag ) {
            p.size2DXInPixel_ = (pdu.pdu_2d_size_x_minus1 + 1) * quantizerSizeX;
            p.size2DYInPixel_ = (pdu.pdu_2d_size_y_minus1 + 1) * quantizerSizeY;
            p.TilePatch2dSizeX = ceil( static_cast<double>( p.size2DXInPixel_ ) / packingBlockSizeD );
            p.TilePatch2dSizeY = ceil( static_cast<double>( p.size2DYInPixel_ ) / packingBlockSizeD );
        } else {
            p.TilePatch2dSizeX = pdu.pdu_2d_size_x_minus1 + 1;
            p.TilePatch2dSizeY = pdu.pdu_2d_size_y_minus1 + 1;
        }
        p.TilePatchOrientationIndex = pdu.pdu_orientation_index;
        p.TilePatchProjectionID = pdu.pdu_projection_id;
        p.TilePatch3dOffsetD = static_cast<int32_t>( pdu.pdu_3d_offset_d ) * minLevel;
        p.setViewId( pdu.pdu_projection_id ); // this also sets a new value to TilePatchProjectionID
        if ( p.normalAxis_ == 0 ) {
            p.tangentAxis_ = 2 ;
            p.bitangentAxis_ = 1 ;
        } else if ( p.normalAxis_ == 1 ) {
            p.tangentAxis_ = 2;
            p.bitangentAxis_ = 0;
        } else {
            p.tangentAxis_ = 0;
            p.bitangentAxis_ = 1;
        }
        /*printf(
          "patch(Intra) %zu: UV0 %4zu %4zu UV1 %4zu %4zu D1=%4zu S=%4zu %4zu %4zu(%4zu) P=%zu O=%zu A=%u%u%u Lod "
          "=(%zu) %zu,%zu 45=%d ProjId=%4zu Axis=%zu \n",
          size_t(i), size_t(p.TilePatch2dPosX), size_t(p.TilePatch2dPosY), size_t(p.TilePatch3dOffsetU), size_t(p.TilePatch3dOffsetV), size_t(p.TilePatch3dOffsetD), size_t(p.TilePatch2dSizeX),
          size_t(p.TilePatch2dSizeY), size_t(p.TilePatch3dRangeD), size_t(pdu.pdu_3d_range_d), size_t(p.TilePatchProjectionID),
          size_t(p.TilePatchOrientationIndex), (uint32_t)p.normalAxis_, (uint32_t)p.tangentAxis_, (uint32_t)p.bitangentAxis_,
          (size_t)lodEnableFlag, (size_t)p.TilePatchLoDScaleX, (size_t)p.TilePatchLoDScaleY, saved_asps_.asps_extended_projection_enabled_flag,
          (size_t)pdu.pdu_projection_id, size_t(p.axisOfAdditionalPlane_) );*/

        frame->patches_map.push_back(p);
    }
}

void Decompression::decompress_video_sub_bitstream(uint8_t* buf, const size_t ptr, const size_t v3c_payload_size_bytes, video_map* map, AVCodecContext* codec_ctx)
{
    std::vector<uint8_t> temp = {};
    std::vector<size_t> frame_boundaries = {};
    std::cerr << "LF LOG ??? AAA" << std::endl;
    convert_video_sub_bitstream(buf, ptr, v3c_payload_size_bytes, temp, frame_boundaries);
    std::cerr << "LF LOG ??? BBB" << std::endl;
    decode_video_sub_bitstream(temp, frame_boundaries, *map, codec_ctx);
    std::cerr << "LF LOG ??? CCC" << std::endl;

}

void Decompression::convert_video_sub_bitstream(const uint8_t* buf, const size_t ptr, const size_t v3c_payload_size_bytes, std::vector<uint8_t> &output, std::vector<size_t> &frame_boundaries)
{
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Converting V3C video data " + std::to_string(v3c_payload_size_bytes) + " \n");
    output.resize(v3c_payload_size_bytes);
    size_t write_ptr = 0;
    frame_boundaries.push_back(write_ptr);

    // Copy the current location so we dont mess up the position on the whole V-PCC bitstream
    std::size_t read_ptr = ptr;
    const std::size_t end_point = read_ptr + v3c_payload_size_bytes;
    const char hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};
    while (true) {
        if (read_ptr >= end_point) {
            break;
        }
        std::size_t nalu_size = read_value(&buf[read_ptr], 4); //read(32, "hevc nal unit size");
        read_ptr += 4;
        std::size_t hevc_nal_type = buf[read_ptr] >> 1;
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "HEVC NAL unit (type " + std::to_string(hevc_nal_type) + ") found, size " + std::to_string(nalu_size) + " \n");
        
        memcpy(&output[write_ptr], hevc_start_code, 4);
        write_ptr += 4;

        memcpy(&output[write_ptr], &buf[read_ptr], nalu_size);
        write_ptr += nalu_size;
        read_ptr += nalu_size;
        if(hevc_nal_type == 19 || hevc_nal_type == 1) {
            frame_boundaries.push_back(write_ptr); 
        } 
    }
}

std::vector<AVFrame*> Decompression::decode_video_frames(std::vector<uint8_t> &input, std::vector<size_t> &frame_boundaries, AVCodecContext* codec_ctx)
{
    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Start decoding HEVC data \n");
    AVCodecContext* context = codec_ctx;
    if (!context) {
        throw std::runtime_error("Codec context is not initialized");
    }
    std::vector<AVFrame*> output = {};
    size_t data_size = input.size();
    /* From avcodec_send_packet() documentation: The input buffer, avpkt->data must be
    AV_INPUT_BUFFER_PADDING_SIZE larger than the actual read bytes. */
    size_t padded_size = data_size + AV_INPUT_BUFFER_PADDING_SIZE;
    input.resize(padded_size, 0);

    // Process full frames through individual packets
    for (size_t frame_index = 0; frame_index < frame_boundaries.size() - 1; frame_index++) {
        AVPacket* packet = av_packet_alloc();
        if (!packet) {
            throw std::runtime_error("Could not allocate AVPacket");
        }
        packet->data = &input.data()[frame_boundaries.at(frame_index)];
        size_t packet_size = frame_boundaries.at(frame_index + 1) - frame_boundaries.at(frame_index);
        packet->size = packet_size;

        int ret = avcodec_send_packet(context, packet);
        if (ret < 0) {
            av_packet_free(&packet);
            throw std::runtime_error("Error sending packet to decoder");
        }
        // Receive frame
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            throw std::runtime_error("Could not allocate video frame");
        }
        ret = avcodec_receive_frame(context, frame);

        if (ret == AVERROR(EAGAIN)) {
            av_frame_free(&frame);
        }
        else if (ret == AVERROR_EOF) {
            av_frame_free(&frame);
        }
        else if (ret < 0) {
            throw std::runtime_error("Error receiving frame from decoder");
        }
        else {
            output.push_back(frame);
        }
        av_packet_free(&packet);
    }

    uvgvpcc_dec::Logger::log<uvgvpcc_dec::LogLevel::TRACE>("Decompression", "Decoded " + std::to_string(output.size()) + " HEVC frames \n");
    return output;
}

void Decompression::decode_video_sub_bitstream(std::vector<uint8_t> &input, std::vector<size_t> &frame_boundaries, video_map &map, AVCodecContext* codec_ctx)
{
    std::vector<AVFrame*> frames = {};
    std::cerr << "LF LOG ??? BBB AAA" << std::endl;

    frames = decode_video_frames(input, frame_boundaries, codec_ctx);
    std::cerr << "LF LOG ??? BBB BBB" << std::endl;
    if(frames.empty()) {
        throw std::runtime_error("No frames decoded");
    }
    map.width = frames.front()->width; // not perfect solution to only take from the first frames stats
    map.height = frames.front()->height; // not perfect solution

    for (size_t i = 0; i < frames.size(); ++i) {
        AVFrame* fr = frames.at(i);
        if (fr->format != AV_PIX_FMT_YUV420P) {
            std::cout << " pixel format " << (int)fr->format << std::endl;
            throw std::runtime_error("Unsupported pixel format, expected YUV420P");
        }
        size_t width = fr->width;
        size_t height = fr->height;
        picture frame420;
        frame420.width = width;
        frame420.height = height;
        frame420.format = PCCCOLORFORMAT::YUV420;
        frame420.Y.resize(width * height);
        frame420.U.resize(width * height / 4);
        frame420.V.resize(width * height / 4);

        // Copy Y plane
        std::memcpy(frame420.Y.data(), fr->data[0], width * height);

        // Copy U plane
        std::memcpy(frame420.U.data(), fr->data[1], width * height / 4);

        // Copy V plane
        std::memcpy(frame420.V.data(), fr->data[2], width * height / 4);

        /*picture frame444;
        if(frame420.format == PCCCOLORFORMAT::YUV420) {
            frame444.convert_yuv_420_to_444(&frame420);
        }*/
        map.pictures.push_back(std::move(frame420)); // frame444 if we convert
        map.frame_count++;
        av_frame_free(&fr);
    }
}