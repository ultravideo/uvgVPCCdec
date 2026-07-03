#include "decoderFFmpeg.hpp"

#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "abstract2DMapDecoder.hpp"
#include "bitstreamParsing/bitstream_util.hpp"
//#include "catchLibLog.hpp"
#include "utils/fileExport.hpp"
#include "utils/parameters.hpp"
#include "uvgvpcc/uvgvpcc.hpp"

using namespace uvgvpcc_dec;

void DecoderFFmpeg::initializeLogCallback() {}

namespace {

void setMapList(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, std::vector<std::reference_wrapper<std::vector<uint8_t>>>& mapList,
                const DECODER_TYPE& decoderType) {
    mapList.reserve(gof->nbFrames);
    if (decoderType == OCCUPANCY) {
        for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
            mapList.emplace_back(frame->occupancyMapDS);
        }
        // bitstream = &gof->bitstreamOccupancy;

    } else if (decoderType == GEOMETRY) {
        for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
            mapList.emplace_back(frame->geometryMapL1);
            if (gof->doubleLayer) {
                mapList.emplace_back(frame->geometryMapL2);
            }
        }
        // bitstream = &gof->bitstreamGeometry;

    } else if (decoderType == ATTRIBUTE) {
        for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
            mapList.emplace_back(frame->attributeMapL1);
            if (gof->doubleLayer) {
                mapList.emplace_back(frame->attributeMapL2);
            }
        }
        // bitstream = &gof->bitstreamAttribute;
    } else {
        assert(false);
    }
}

void setMapList_16bit(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, std::vector<std::reference_wrapper<std::vector<uint16_t>>>& mapList) {
    mapList.reserve(gof->nbFrames);
    for (const std::shared_ptr<uvgvpcc_dec::Frame>& frame : gof->frames) {
        mapList.emplace_back(frame->attributeMapL1_16bits);
        if (gof->doubleLayer) {
            mapList.emplace_back(frame->attributeMapL2_16bits);
        }
    }
}

void setBitstream(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, std::vector<uint8_t>& bitstream, const DECODER_TYPE& decoderType) {
    switch (decoderType) {
        case OCCUPANCY:
            bitstream = gof->bitstreamOccupancy;
            break;
        case GEOMETRY:
            bitstream = gof->bitstreamGeometry;
            break;
        case ATTRIBUTE:
            bitstream = gof->bitstreamAttribute;
            break;
        default:
            assert(false);
    }
}

std::vector<uint8_t>& getBitstream(const std::shared_ptr<uvgvpcc_dec::GOF>& gof, const DECODER_TYPE& decoderType) {
    switch (decoderType) {
        case OCCUPANCY:
            return gof->bitstreamOccupancy;
            break;
        case GEOMETRY:
            return gof->bitstreamGeometry;
            break;
        case ATTRIBUTE:
            return gof->bitstreamAttribute;
            break;
        default:
            assert(false);
            static std::vector<uint8_t> dummy;
            return dummy;
    }
}

void setFFmpegConfig(const AVCodec* codec, AVCodecContext* codec_ctx, const DECODER_TYPE& decoderType) {

}


void sampleStreamToByteStream_HEVC(size_t precision, std::vector<uint8_t>& bitstream) {
    size_t sizeStartCode = 4, startIndex = 0, endIndex = 0;
    std::vector<uint8_t> data;
    bool newFrame = true;

    do {
        int32_t nalu_size = 0;
        for (size_t i = 0; i < precision; i++) {
            nalu_size = (nalu_size << 8) + bitstream[startIndex + i];
        }
        endIndex = startIndex + precision + nalu_size;
        // Push start code to mark the NAL unit
        for ( size_t i = 0; i < sizeStartCode - 1; i++) {
            data.push_back(0);
        }
        data.push_back(1);

        // if (emulationPreventionBytes) {
        //     for ( size_t i = startIndex + precision, zeroCount = 0; i < endIndex; i++ ) {
        //         if ( zeroCount == 3 && data_[i] <= 0x03 ) {
        //             data.push_back( 0x03 );
        //             zeroCount = 0;
        //         }
        //         zeroCount = ( data_[i] == 0x00 ) ? zeroCount + 1 : 0;
        //         data.push_back( data_[i] );
        //     }
        // } else {
        //     for ( size_t i = startIndex + precision; i < endIndex; i++ ) { data.push_back( data_[i] ); }
        // }

        // Assume NO emulation prevetion bytes
        for (size_t i = startIndex + precision; i < endIndex; i++) {
            data.push_back(bitstream[i]);
        }
        startIndex = endIndex;
        if ((startIndex + precision) < bitstream.size()) {
            int nalu_type = 0;
            bool useLongStartCode = false;
            newFrame = false;

            nalu_type = ((bitstream[startIndex + precision]) & 126) >> 1;
            useLongStartCode = newFrame || (nalu_type >= 32 && nalu_type < 41);
            if (nalu_type < 12 ) {
                newFrame = true;
            }
            sizeStartCode = useLongStartCode ? 4 : 3;
        }
        // if ((startIndex + precision) < bitstream.size()) {
        //     int nalu_type = ((bitstream[startIndex + precision]) & 126) >> 1;
        //     sizeStartCode = (nalu_type >= 32 && nalu_type < 41) ? 4 : 3;
        // }
    } while (endIndex < bitstream.size());
    bitstream.swap(data);
}

void decodeVideoFFmpeg( std::vector<AVFrame*> &frames, AVCodecContext* codec_ctx, std::vector<uint8_t>& bitstream, const std::string& decoderName ) {

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        throw std::runtime_error(decoderName + ": Failed to allocate AVPacket.");
    }

    const uint8_t hevc_start_code[4] = {0x00, 0x00, 0x00, 0x01};
    size_t read_ptr = 0;
    size_t write_ptr = 0;
    size_t map_size = bitstream.size();

    std::vector<uint8_t> data_buffer = {};

    // printf("%s\n", decoderName.c_str());
    while (write_ptr < map_size) {
        size_t nalu_size = bitstream_read_size_from_poiter(&bitstream[write_ptr], 4);

        data_buffer.insert(data_buffer.end(), hevc_start_code, hevc_start_code + 4);
        write_ptr += 4;

        size_t hevc_nal_type = (bitstream[write_ptr] >> 1) & 0x3F;
        // printf("nalu_size: %d, hevc_nal_type: %d\n", (int)nalu_size, (int)hevc_nal_type);

        data_buffer.insert(data_buffer.end(), bitstream.begin()+write_ptr, bitstream.begin()+write_ptr+nalu_size);
        write_ptr += nalu_size;

        if (/* hevc_nal_type == 19 || hevc_nal_type == 1 */ hevc_nal_type <= 21) {
            // size_t padded_size = data_buffer.size() + AV_INPUT_BUFFER_PADDING_SIZE;
            // data_buffer.resize(padded_size, 0);
            pkt->data = data_buffer.data();
            pkt->size = data_buffer.size();
            // av_new_packet(pkt, data_buffer.size());
            // memcpy(pkt->data, data_buffer.data(), data_buffer.size());
            // printf("Packet_size: %d, data size: %d\n", (int)pkt->size, (int)data_buffer.size());

            // Send AVPacket
            int ret = avcodec_send_packet(codec_ctx, pkt);
            if (ret < 0) {
                av_packet_free(&pkt);
                throw std::runtime_error(decoderName + ": Failed to send AVPacket.");
            }

            // Create AVFrame
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                throw std::runtime_error(decoderName + ": Failed to allocate AVFrame.");
            }

            while (true) {
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                    break;
                if (ret < 0)
                    throw std::runtime_error(decoderName + ": Failed to receive AVFrame.");
                frames.push_back(frame);
                frame = av_frame_alloc();
            }
            av_frame_free(&frame);
            data_buffer.clear();
        }
    }
    
    // Flush decoder
    avcodec_send_packet(codec_ctx, nullptr);
    AVFrame* frame = av_frame_alloc();
    while (avcodec_receive_frame(codec_ctx, frame) == 0)
    {
        frames.push_back(frame);
        frame = av_frame_alloc();
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    bitstream.clear();
    bitstream.shrink_to_fit();
}

void decodeVideoFFmpeg_(std::vector<AVFrame*>& frames,
                       AVCodecContext* codec_ctx,
                       std::vector<uint8_t>& bitstream,
                       const std::string& decoderName)
{
    constexpr size_t precision = 4;
    const uint8_t startCode[4] = {0x00, 0x00, 0x00, 0x01};

    auto receiveFrames = [&]() {
        while (true) {
            AVFrame* frame = av_frame_alloc();
            if (!frame) {
                throw std::runtime_error(decoderName + ": Failed to allocate AVFrame.");
            }

            int ret = avcodec_receive_frame(codec_ctx, frame);

            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_frame_free(&frame);
                break;
            }

            if (ret < 0) {
                av_frame_free(&frame);
                throw std::runtime_error(decoderName + ": Failed to receive AVFrame.");
            }

            frames.push_back(frame);
        }
    };

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        throw std::runtime_error(decoderName + ": Failed to allocate AVPacket.");
    }

    size_t pos = 0;

    while (pos < bitstream.size()) {
        if (pos + precision > bitstream.size()) {
            av_packet_free(&pkt);
            throw std::runtime_error(decoderName + ": Incomplete NAL size field.");
        }

        size_t nalu_size = 0;
        for (size_t i = 0; i < precision; ++i) {
            nalu_size = (nalu_size << 8) + bitstream[pos + i];
        }

        size_t nalu_start = pos + precision;
        size_t nalu_end   = nalu_start + nalu_size;

        if (nalu_end > bitstream.size()) {
            av_packet_free(&pkt);
            throw std::runtime_error(decoderName + ": NAL size exceeds buffer.");
        }

        if (nalu_size > 0) {
            size_t hevc_nal_type = (bitstream[nalu_start] >> 1) & 0x3F;

            printf("%s: nalu_size=%zu, hevc_nal_type=%zu\n",
                   decoderName.c_str(), nalu_size, hevc_nal_type);

            std::vector<uint8_t> packetData;
            packetData.reserve(4 + nalu_size);

            packetData.insert(packetData.end(), startCode, startCode + 4);
            packetData.insert(packetData.end(),
                              bitstream.begin() + nalu_start,
                              bitstream.begin() + nalu_end);

            av_packet_unref(pkt);

            int ret = av_new_packet(pkt, static_cast<int>(packetData.size()));
            if (ret < 0) {
                av_packet_free(&pkt);
                throw std::runtime_error(decoderName + ": Failed to allocate packet.");
            }

            std::memcpy(pkt->data, packetData.data(), packetData.size());

            ret = avcodec_send_packet(codec_ctx, pkt);

            if (ret == AVERROR(EAGAIN)) {
                receiveFrames();
                ret = avcodec_send_packet(codec_ctx, pkt);
            }

            av_packet_unref(pkt);

            if (ret < 0) {
                av_packet_free(&pkt);
                throw std::runtime_error(decoderName + ": Failed to send packet.");
            }

            receiveFrames();
        }

        pos = nalu_end;
    }

    int ret = avcodec_send_packet(codec_ctx, nullptr);
    if (ret < 0 && ret != AVERROR_EOF) {
        av_packet_free(&pkt);
        throw std::runtime_error(decoderName + ": Failed to flush decoder.");
    }

    receiveFrames();

    av_packet_free(&pkt);

    bitstream.clear();
    bitstream.shrink_to_fit();
}


int                 clamp( int v, int a, int b ) { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
float               clamp( float v, float a, float b ) { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
double              clamp( double v, double a, double b ) { return ( ( v < a ) ? a : ( ( v > b ) ? b : v ) ); }
static inline float fMin( float a, float b ) { return ( ( a ) < ( b ) ) ? ( a ) : ( b ); }
static inline float fMax( float a, float b ) { return ( ( a ) > ( b ) ) ? ( a ) : ( b ); }
static inline float fClip( float x, float low, float high ) { return fMin( fMax( x, low ), high ); }

inline double PCCClip( const double& n, const double& lower, const double& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

inline float PCCClip( const float& n, const float& lower, const float& upper ) {
    return ( std::max )( lower, ( std::min )( n, upper ) );
}

void YUVtoFloatYUV(const std::vector<uint8_t>& src, std::vector<float>& dst, uint16_t offset, float minV, float maxV) {
    size_t count = src.size();
    dst.resize(count);
    
    double scale = 255.0;
    double weight = 1.0 / scale;


    for (size_t i = 0; i < count; i++) {
        dst[i] = std::clamp( (float)(weight * (double)(src[i] - offset)), minV, maxV);
    }
}

void YUVtoFloatYUV_fromDecodedFrame(const uint8_t* src, int srcStride, int width, int height, float* dst, uint16_t offset, float minV, float maxV) {
    // double scale = 255.0;
    // double weight = 1.0 / scale; 
    constexpr double weight = 1.0 / 255.0;

    for (int y = 0; y < height; ++y) {
        const uint8_t* srcRow = src + y * srcStride;
        float* dstRow = dst + y * width;

        for (int x = 0; x < width; ++x) {
            dstRow[x] = std::clamp((float)(weight * (double)(srcRow[x] - offset)), minV, maxV);
            // dstRow[x] = clamp((float)(weight * (double)(srcRow[x] - offset)), minV, maxV );
        }
    }
}

void YUVtoFloatYUV_fromDecodedFrame_yuv42010ple(const uint8_t* src, int srcStride, int width, int height, float* dst, uint16_t offset, float minV, float maxV) {
    constexpr double weight = 1.0 / 255.0;

    for (int y = 0; y < height; ++y) {
        const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(src + y * srcStride);
        float* dstRow = dst + y * width;

        for (int x = 0; x < width; ++x) {
            dstRow[x] = std::clamp((float)(weight * (double)(std::min<uint16_t>((srcRow[x]+2)>>2, 255) - offset)), minV, maxV);
        }
    }
}


void floatYUVtoYUV(const std::vector<float>& src, std::vector<uint16_t>& dst, double offset, double scale) {
    size_t count = src.size();
    dst.resize(count);

    for (size_t i = 0; i < count; i++) {
        dst[i] = static_cast<uint16_t>( PCCClip(std::round((float)(scale * (double)src[i] + offset)), 0.f, (float)scale) );
    }
}

void floatYUVtoYUV(const std::vector<float>& src, uint16_t* dst, double offset, double scale) {
    size_t count = src.size();

    for (size_t i = 0; i < count; i++) {
        dst[i] = static_cast<uint16_t>( PCCClip( std::round((float)(scale * (double)src[i] + offset)), 0.f, (float)scale ) );
    }
}

void upsampling(const std::vector<float>& chromaIn, std::vector<float>& chromaOut, const int widthIn, const int heightIn) {
    const int widthOut = widthIn * 2;
    const int heightOut = heightIn * 2;
    chromaOut.resize(widthOut * heightOut);
    std::vector<float> temp;
    temp.resize(widthIn * heightOut);

    constexpr float scale = 1.0f / 256.0f;
    //const float scale = 1.0f / ( (float)( 1 << ( (int)8 ) ) );

    // Vertical upsampling
    for (int i = 0; i < heightIn; i++) {
        // i0 - 0
        const int i0_clamp = std::clamp(i, 0, heightIn - 1) * widthIn;
        // i0 - 1
        const int i0_m1_clamp = std::clamp(i - 1, 0, heightIn - 1) * widthIn;
        // i0 + 1 
        const int i0_p1_clamp = std::clamp(i + 1, 0, heightIn - 1) * widthIn;
        // i0 - 2
        const int i0_m2_clamp = std::clamp(i - 2, 0, heightIn - 1) * widthIn;
        // i0 + 2
        const int i0_p2_clamp = std::clamp(i + 2, 0, heightIn - 1) * widthIn;

        for (int j = 0; j < widthIn; j++) {
            temp[(2 * i) * widthIn + j] = (
                -8.0f*chromaIn[i0_m2_clamp+j] + 
                64.0f*chromaIn[i0_m1_clamp+j] + 
                216.0f*chromaIn[i0_clamp+j] - 
                16.0f*chromaIn[i0_p1_clamp+j]
            ) * scale;
            temp[(2 * i + 1) * widthIn + j] = (
                -16.0f*chromaIn[i0_m1_clamp+j] + 
                216.0f*chromaIn[i0_clamp+j] + 
                64.0f*chromaIn[i0_p1_clamp+j] - 
                8.0f*chromaIn[i0_p2_clamp+j]
            ) * scale;
        }
    }

    // Horizontal upsampling
    for (int i = 0; i < heightOut; i++) {
        const int step = i * widthIn;
        for (int j = 0; j < widthIn; j++) {
            chromaOut[i * widthOut + j * 2] = temp[step + std::clamp(j, 0, widthIn - 1)]; // ( 256*temp[step + std::clamp(j, 0, widthIn - 1)] ) * scale
            // chromaOut[i * widthOut + j * 2 + 1][1] = temp[0][step + std::clamp(j+1, 0, widthIn - 1)]; // ( 256*temp[step + std::clamp(j+1, 0, widthIn - 1)] ) * scale
            chromaOut[i * widthOut + j * 2 + 1] = (
                -16.0f*temp[step + std::clamp(j - 1, 0, widthIn - 1)] + 
                144.0f*temp[step + std::clamp(j, 0, widthIn - 1)] + 
                144.0f*temp[step + std::clamp(j + 1, 0, widthIn - 1)] - 
                16.0f*temp[step + std::clamp(j + 2, 0, widthIn - 1)]  
            ) * scale;
        }
    }
}

void upsampling(const std::vector<float>& chromaIn, std::vector<float>& chromaOut, std::vector<float>& temp, const int widthIn, const int heightIn) {
    const int widthOut = widthIn * 2;
    const int heightOut = heightIn * 2;
    constexpr float scale = 1.0f / 256.0f;
    //const float scale = 1.0f / ( (float)( 1 << ( (int)8 ) ) );

    // Vertical upsampling
    for (int i = 0; i < heightIn; i++) {
        // i0 - 0
        const int i0_clamp = std::clamp(i, 0, heightIn - 1) * widthIn;
        // i0 - 1
        const int i0_m1_clamp = std::clamp(i - 1, 0, heightIn - 1) * widthIn;
        // i0 + 1 
        const int i0_p1_clamp = std::clamp(i + 1, 0, heightIn - 1) * widthIn;
        // i0 - 2
        const int i0_m2_clamp = std::clamp(i - 2, 0, heightIn - 1) * widthIn;
        // i0 + 2
        const int i0_p2_clamp = std::clamp(i + 2, 0, heightIn - 1) * widthIn;

        const int offset_0 = (2 * i) * widthIn;
        const int offset_1 = (2 * i + 1) * widthIn;

        for (int j = 0; j < widthIn; j++) {
            temp[offset_0 + j] = (
                -8.0f*chromaIn[i0_m2_clamp+j] + 
                64.0f*chromaIn[i0_m1_clamp+j] + 
                216.0f*chromaIn[i0_clamp+j] - 
                16.0f*chromaIn[i0_p1_clamp+j]
            ) * scale;
            temp[offset_1 + j] = (
                -16.0f*chromaIn[i0_m1_clamp+j] + 
                216.0f*chromaIn[i0_clamp+j] + 
                64.0f*chromaIn[i0_p1_clamp+j] - 
                8.0f*chromaIn[i0_p2_clamp+j]
            ) * scale;
        }
    }

    // Horizontal upsampling
    for (int i = 0; i < heightOut; i++) {
        const int step = i * widthIn;
        const int offset_0 = i * widthOut;
        const int offset_1 = i * widthOut + 1;
        for (int j = 0; j < widthIn; j++) {
            chromaOut[offset_0 + j * 2]     = temp[step + std::clamp(j, 0, widthIn - 1)]; // ( 256*temp[step + std::clamp(j, 0, widthIn - 1)] ) * scale
            chromaOut[offset_1 + j * 2] = (
                -16.0f*temp[step + std::clamp(j - 1, 0, widthIn - 1)] + 
                144.0f*temp[step + std::clamp(j, 0, widthIn - 1)] + 
                144.0f*temp[step + std::clamp(j + 1, 0, widthIn - 1)] - 
                16.0f*temp[step + std::clamp(j + 2, 0, widthIn - 1)]  
            ) * scale;
        }
    }
}

inline uint16_t chromaFloatToU16(float v) {
    float x = 65535.0 * (double)v + 32768.0;
    return static_cast<uint16_t>( PCCClip( std::round(x), 0.f, 65535.0f ));
}

void upsamplingToU16(const std::vector<float>& chromaIn,
                     std::vector<float>& temp,
                     uint16_t* chromaOut,
                     int widthIn,
                     int heightIn)
{
    const int widthOut = widthIn * 2;
    const int heightOut = heightIn * 2;
    constexpr float scale = 1.0f / 256.0f;

    for (int i = 0; i < heightIn; i++) {
        const int i0   = i * widthIn;
        const int im1  = std::clamp(i - 1, 0, heightIn - 1) * widthIn;
        const int ip1  = std::clamp(i + 1, 0, heightIn - 1) * widthIn;
        const int im2  = std::clamp(i - 2, 0, heightIn - 1) * widthIn;
        const int ip2  = std::clamp(i + 2, 0, heightIn - 1) * widthIn;

        float* row0 = temp.data() + (2 * i) * widthIn;
        float* row1 = temp.data() + (2 * i + 1) * widthIn;

        for (int j = 0; j < widthIn; j++) {
            row0[j] = (-8.0f * chromaIn[im2 + j]
                    + 64.0f * chromaIn[im1 + j]
                    +216.0f * chromaIn[i0  + j]
                    - 16.0f * chromaIn[ip1 + j]) * scale;

            row1[j] = (-16.0f * chromaIn[im1 + j]
                    +216.0f * chromaIn[i0  + j]
                    + 64.0f * chromaIn[ip1 + j]
                    -  8.0f * chromaIn[ip2 + j]) * scale;
        }
    }

    for (int i = 0; i < heightOut; i++) {
        const int step = i * widthIn;
        uint16_t* dst = chromaOut + i * widthOut;

        for (int j = 0; j < widthIn; j++) {
            const float even = temp[step + j];

            const float odd =
                (-16.0f * temp[step + std::clamp(j - 1, 0, widthIn - 1)]
                +144.0f * temp[step + j]
                +144.0f * temp[step + std::clamp(j + 1, 0, widthIn - 1)]
                - 16.0f * temp[step + std::clamp(j + 2, 0, widthIn - 1)]) * scale;

            dst[j * 2]     = chromaFloatToU16(even);
            dst[j * 2 + 1] = chromaFloatToU16(odd);
        }
    }
}


void writeDecodedFramesToMapList_tmc2_yuv420yuv444_conversion(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint16_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    gof->attribute_map_width  = (size_t)width;
    gof->attribute_map_height = (size_t)height;
    
    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY << 1;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;

    float minV_uv = -0.5f; // Chroma
    float maxV_uv = 0.5f; // Chroma
    float minV_y =  0.f;
    float maxV_y = 1.f;

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint16_t>& map = mapList[frame_index].get();

        std::vector<uint8_t> yuv420[3];
        std::vector<float> yuv420_f[3];
        std::vector<float> yuv444_f[3];
        std::vector<uint16_t> yuv444[3];

        for (int y = 0; y < height; ++y) {
            yuv420[0].insert(yuv420[0].end(), frame->data[0] + y*frame->linesize[0], frame->data[0] + y*frame->linesize[0] + width);
        }
        for (int y = 0; y < UV_height; ++y) {
            yuv420[1].insert(yuv420[1].end(), frame->data[1] + y*frame->linesize[1], frame->data[1] + y*frame->linesize[1] + UV_width);
            yuv420[2].insert(yuv420[2].end(), frame->data[2] + y*frame->linesize[2], frame->data[2] + y*frame->linesize[2] + UV_width);
        }

        // Apply tmc2 color inversion
        YUVtoFloatYUV(yuv420[0], yuv420_f[0], 0, minV_y, maxV_y);
        YUVtoFloatYUV(yuv420[1], yuv420_f[1], 128, minV_uv, maxV_uv);
        YUVtoFloatYUV(yuv420[2], yuv420_f[2], 128, minV_uv, maxV_uv);
        upsampling(yuv420_f[1], yuv444_f[1], (int)UV_width, (int)UV_height);
        upsampling(yuv420_f[2], yuv444_f[2], (int)UV_width, (int)UV_height);
        floatYUVtoYUV(yuv420_f[0], yuv444[0], 0.0, 65535.);
        floatYUVtoYUV(yuv444_f[1], yuv444[1], 32768., 65535.);
        floatYUVtoYUV(yuv444_f[2], yuv444[2], 32768., 65535.);

        // Copy data to mapList
        map.insert(map.end(), yuv444[0].data(), yuv444[0].data() + yuv444[0].size());
        map.insert(map.end(), yuv444[1].data(), yuv444[1].data() + yuv444[1].size());
        map.insert(map.end(), yuv444[2].data(), yuv444[2].data() + yuv444[2].size());


        av_frame_free(&frame);
    }

}


void writeDecodedFramesToMapList_tmc2Yuv420yuv444Conversion_fast_(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint16_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    gof->attribute_map_width  = (size_t)width;
    gof->attribute_map_height = (size_t)height;
    
    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;

    float minV_uv = -0.5f; // Chroma
    float maxV_uv = 0.5f; // Chroma
    float minV_y =  0.f;
    float maxV_y = 1.f;

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint16_t>& map = mapList[frame_index].get();

        std::vector<float> yuv420_f[3];
        std::vector<float> yuv444_f[3];

        yuv420_f[0].resize(sizeY);
        yuv420_f[1].resize(sizeUV);
        yuv420_f[2].resize(sizeUV);

        // Apply tmc2 color inversion
        YUVtoFloatYUV_fromDecodedFrame(frame->data[0], frame->linesize[0], width,    height,    yuv420_f[0].data(), 0,   minV_y,  maxV_y);
        YUVtoFloatYUV_fromDecodedFrame(frame->data[1], frame->linesize[1], UV_width, UV_height, yuv420_f[1].data(), 128, minV_uv, maxV_uv);
        YUVtoFloatYUV_fromDecodedFrame(frame->data[2], frame->linesize[2], UV_width, UV_height, yuv420_f[2].data(), 128, minV_uv, maxV_uv);
        upsampling(yuv420_f[1], yuv444_f[1], (int)UV_width, (int)UV_height);
        upsampling(yuv420_f[2], yuv444_f[2], (int)UV_width, (int)UV_height);

        map.resize(sizeY*3); // Y + U + V
        uint16_t* dstY = map.data();
        uint16_t* dstU = map.data() + sizeY;
        uint16_t* dstV = map.data() + sizeY * 2;
        floatYUVtoYUV(yuv420_f[0], dstY, 0, 65535.);
        floatYUVtoYUV(yuv444_f[1], dstU, 32768., 65535.);
        floatYUVtoYUV(yuv444_f[2], dstV, 32768., 65535.);

        av_frame_free(&frame);
    }

}

void writeDecodedFramesToMapList_tmc2Yuv420yuv444Conversion_fast(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint16_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    gof->attribute_map_width  = (size_t)width;
    gof->attribute_map_height = (size_t)height;
    
    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;
    const size_t map_size = sizeY*3;

    float minV_uv = -0.5f; // Chroma
    float maxV_uv = 0.5f; // Chroma
    float minV_y =  0.f;
    float maxV_y = 1.f;

    std::vector<float> yuv420_f[3];
    yuv420_f[1].resize(sizeUV);
    yuv420_f[2].resize(sizeUV);
    //std::vector<float> yuv444_f[3];
    //yuv444_f[1].resize(sizeY);
    //yuv444_f[2].resize(sizeY);

    std::vector<float> buffer_yuv444;
    buffer_yuv444.resize(UV_width * size_t(height));

    bool is_yuv42010ple = frames.front()->format == AV_PIX_FMT_YUV420P10LE;

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint16_t>& map = mapList[frame_index].get();

        map.resize(map_size); // Y + U + V
        uint16_t* dstY = map.data();
        uint16_t* dstU = map.data() + sizeY;
        uint16_t* dstV = map.data() + sizeY * 2;

        if (is_yuv42010ple) {
            // Apply tmc2 color inversion to Y
            for (int y = 0; y < height; ++y) {
                const uint16_t* srcRow = reinterpret_cast<const uint16_t*>(frame->data[0] + y * frame->linesize[0]);
                uint16_t* dstRow = dstY + y * width;

                for (int x = 0; x < width; ++x) {
                    dstRow[x] = static_cast<uint16_t>( std::min<uint16_t>((srcRow[x]+2) >> 2, 255) ) * 257u;
                }
            }
            YUVtoFloatYUV_fromDecodedFrame_yuv42010ple(frame->data[1], frame->linesize[1], UV_width, UV_height, yuv420_f[1].data(), 128, minV_uv, maxV_uv);
            YUVtoFloatYUV_fromDecodedFrame_yuv42010ple(frame->data[2], frame->linesize[2], UV_width, UV_height, yuv420_f[2].data(), 128, minV_uv, maxV_uv);
        } else {
            // Apply tmc2 color inversion to Y
            for (int y = 0; y < height; ++y) {
                const uint8_t* srcRow = frame->data[0] + y * frame->linesize[0];
                uint16_t* dstRow = dstY + y * width;

                for (int x = 0; x < width; ++x) {
                    dstRow[x] = static_cast<uint16_t>(srcRow[x]) * 257u;
                }
            }

            // Apply tmc2 color inversion to UV
            YUVtoFloatYUV_fromDecodedFrame(frame->data[1], frame->linesize[1], UV_width, UV_height, yuv420_f[1].data(), 128, minV_uv, maxV_uv);
            YUVtoFloatYUV_fromDecodedFrame(frame->data[2], frame->linesize[2], UV_width, UV_height, yuv420_f[2].data(), 128, minV_uv, maxV_uv);
        }

        // // Apply tmc2 color inversion to Y
        // for (int y = 0; y < height; ++y) {
        //     const uint8_t* srcRow = frame->data[0] + y * frame->linesize[0];
        //     uint16_t* dstRow = dstY + y * width;

        //     for (int x = 0; x < width; ++x) {
        //         dstRow[x] = static_cast<uint16_t>(srcRow[x]) * 257u;
        //     }
        // }

        // // Apply tmc2 color inversion to UV
        // YUVtoFloatYUV_fromDecodedFrame(frame->data[1], frame->linesize[1], UV_width, UV_height, yuv420_f[1].data(), 128, minV_uv, maxV_uv);
        // YUVtoFloatYUV_fromDecodedFrame(frame->data[2], frame->linesize[2], UV_width, UV_height, yuv420_f[2].data(), 128, minV_uv, maxV_uv);
        upsamplingToU16(yuv420_f[1], buffer_yuv444, dstU, UV_width, UV_height);
        upsamplingToU16(yuv420_f[2], buffer_yuv444, dstV, UV_width, UV_height);

        av_frame_free(&frame);
    }

}

void copyYuv420p10leTo8bitMap(AVFrame* frame, std::vector<uint8_t>& map)
{
    const int width  = frame->width;
    const int height = frame->height;

    const int uvWidth  = width / 2;
    const int uvHeight = height / 2;

    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = size_t(uvWidth) * size_t(uvHeight);

    // map.resize(sizeY + 2 * sizeUV);
    map.resize(sizeY + (sizeY >> 1));

    uint8_t* dstY = map.data();
    uint8_t* dstU = dstY + sizeY;
    uint8_t* dstV = dstU + sizeUV;

    for (int y = 0; y < height; ++y) {
        const uint16_t* srcY =
            reinterpret_cast<const uint16_t*>(frame->data[0] + y * frame->linesize[0]);

        for (int x = 0; x < width; ++x) {
            dstY[y * width + x] = static_cast<uint8_t>(srcY[x] >> 2);
        }
    }

    for (int y = 0; y < uvHeight; ++y) {
        const uint16_t* srcU =
            reinterpret_cast<const uint16_t*>(frame->data[1] + y * frame->linesize[1]);
        const uint16_t* srcV =
            reinterpret_cast<const uint16_t*>(frame->data[2] + y * frame->linesize[2]);

        for (int x = 0; x < uvWidth; ++x) {
            dstU[y * uvWidth + x] = static_cast<uint8_t>(srcU[x] >> 2);
            dstV[y * uvWidth + x] = static_cast<uint8_t>(srcV[x] >> 2);
        }
    }
}

void writeDecodedFramesToMapList__(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint8_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    const int height = frames.front()->height;
    const int width  = frames.front()->width;

    // Store the map size
    if (decoderType == OCCUPANCY) {
        gof->occupancy_map_width  = width;
        gof->occupancy_map_height = height;
    } else if (decoderType == GEOMETRY) {
        gof->geometry_map_width  = width;
        gof->geometry_map_height = height;
    } else {
        gof->attribute_map_width  = width;
        gof->attribute_map_height = height;
    }

    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY << 1;
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;

    int Y_widths[height];
    int Y_linesizes[height];
    for (int y = 0; y < height; y++) {
        Y_widths[y] = y*width;
        Y_linesizes[y] = y*frames.front()->linesize[0];
    }

    int UV_widths[UV_height];
    int UV_linesizes[UV_height];
    for (int y = 0; y < UV_height; y++) {
        UV_widths[y] = y*UV_width;
        UV_linesizes[y] = y*frames.front()->linesize[1];
    }

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint8_t>& map = mapList[frame_index].get();

        if (frame->format == AV_PIX_FMT_YUV420P10LE) {
            printf("AV_PIX_FMT_YUV420P10LE\n");
            copyYuv420p10leTo8bitMap(frame, map);
        } else {
            map.resize(total_size); // Y + U + V
            uint8_t* dstY = map.data();
            uint8_t* dstU = dstY + sizeY;
            uint8_t* dstV = dstU + sizeUV;

            // Copy Y plane
            for (int y = 0; y < height; ++y) {
                memcpy(dstY + Y_widths[y], frame->data[0] + Y_linesizes[y], width);
            }

            // Copy U and V planes
            for (int y = 0; y < UV_height; ++y) {
                memcpy(dstU + UV_widths[y], frame->data[1] + UV_linesizes[y], UV_width);
                memcpy(dstV + UV_widths[y], frame->data[2] + UV_linesizes[y], UV_width);
            }
        }

        // map.resize(total_size); // Y + U + V
        // uint8_t* dstY = map.data();
        // uint8_t* dstU = dstY + sizeY;
        // uint8_t* dstV = dstU + sizeUV;

        // // Copy Y plane
        // for (int y = 0; y < height; ++y) {
        //     memcpy(dstY + Y_widths[y], frame->data[0] + Y_linesizes[y], width);
        // }

        // // Copy U and V planes
        // for (int y = 0; y < UV_height; ++y) {
        //     memcpy(dstU + UV_widths[y], frame->data[1] + UV_linesizes[y], UV_width);
        //     memcpy(dstV + UV_widths[y], frame->data[2] + UV_linesizes[y], UV_width);
        // }

        // printf("frames.size() = %zu, mapList.size() = %zu\n", frames.size(), mapList.size());
        // printf("pix_fmt=%d, w=%d, h=%d, linesize=%d,%d,%d\n",
        // frames.front()->format,
        // frames.front()->width,
        // frames.front()->height,
        // frames.front()->linesize[0],
        // frames.front()->linesize[1],
        // frames.front()->linesize[2]);
        
        av_frame_free(&frame);

    }

}

void writeDecodedFramesToMapList_(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint8_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    int height = frames.front()->height;
    int width  = frames.front()->width;

    // Store the map size
    if (decoderType == OCCUPANCY) {
        gof->occupancy_map_width  = width;
        gof->occupancy_map_height = height;
    } else if (decoderType == GEOMETRY) {
        gof->geometry_map_width  = width;
        gof->geometry_map_height = height;
    } else {
        gof->attribute_map_width  = width;
        gof->attribute_map_height = height;
    }

    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY + (sizeY >> 1);

    int Y_widths[height];
    int Y_linesizes[height];
    for (int y = 0; y < height; y++) {
        Y_widths[y] = y*width;
        Y_linesizes[y] = y*frames.front()->linesize[0];
    }

    int UV_widths[UV_height];
    int UV_linesizes[UV_height];
    for (int y = 0; y < UV_height; y++) {
        UV_widths[y] = y*UV_width;
        UV_linesizes[y] = y*frames.front()->linesize[1];
    }

    bool is_yuv42010ple = frames.front()->format == AV_PIX_FMT_YUV420P10LE;

    for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
        AVFrame* frame = frames[frame_index];
        std::vector<uint8_t>& map = mapList[frame_index].get();

        map.resize(total_size); // Y + U + V
        uint8_t* dstY = map.data();
        uint8_t* dstU = dstY + sizeY;
        uint8_t* dstV = dstU + sizeUV;

        if (is_yuv42010ple) { // bit depth = 10
            // Copy Y plane
            for (int y = 0; y < height; ++y) {
                const uint16_t* srcY = reinterpret_cast<const uint16_t*>(frame->data[0] + Y_linesizes[y]);
                for (int x = 0; x < width; ++x) {
                    // dstY[Y_widths[y] + x] = static_cast<uint8_t>(srcY[x] >> 2);

                    uint16_t out = (srcY[x] + 2) >> 2;
                    dstY[Y_widths[y] + x] = static_cast<uint8_t>(std::min<uint16_t>(out, 255));
                }
            }

            // Copy U and V planes
            for (int y = 0; y < UV_height; ++y) {
                const uint16_t* srcU = reinterpret_cast<const uint16_t*>(frame->data[1] + UV_linesizes[y]);
                const uint16_t* srcV = reinterpret_cast<const uint16_t*>(frame->data[2] + UV_linesizes[y]);
                for (int x = 0; x < UV_width; ++x) {
                    // dstU[UV_widths[y] + x] = static_cast<uint8_t>(srcU[x] >> 2);
                    // dstV[UV_widths[y] + x] = static_cast<uint8_t>(srcV[x] >> 2);

                    uint16_t out_u = (srcU[x] + 2) >> 2;
                    uint16_t out_v = (srcV[x] + 2) >> 2;
                    dstU[UV_widths[y] + x] = static_cast<uint8_t>(std::min<uint16_t>(out_u, 255));
                    dstV[UV_widths[y] + x] = static_cast<uint8_t>(std::min<uint16_t>(out_v, 255));
                }
            }
        } else { // bit depth = 8
            // Copy Y plane
            for (int y = 0; y < height; ++y) {
                memcpy(dstY + Y_widths[y], frame->data[0] + Y_linesizes[y], width);
            }

            // Copy U and V planes
            for (int y = 0; y < UV_height; ++y) {
                memcpy(dstU + UV_widths[y], frame->data[1] + UV_linesizes[y], UV_width);
                memcpy(dstV + UV_widths[y], frame->data[2] + UV_linesizes[y], UV_width);
            }
        }
        av_frame_free(&frame);
    }

}

void writeDecodedFramesToMapList(
    const std::shared_ptr<uvgvpcc_dec::GOF>& gof, 
    std::vector<std::reference_wrapper<std::vector<uint8_t>>> &mapList, 
    std::vector<AVFrame*> &frames,
    const DECODER_TYPE& decoderType
) {

    if (frames.empty()) {
        throw std::runtime_error("FFmpeg decoder produced no frames.");
    }

    int height = frames.front()->height;
    int width  = frames.front()->width;

    // Store the map size
    if (decoderType == OCCUPANCY) {
        gof->occupancy_map_width  = width;
        gof->occupancy_map_height = height;
    } else if (decoderType == GEOMETRY) {
        gof->geometry_map_width  = width;
        gof->geometry_map_height = height;
    } else {
        gof->attribute_map_width  = width;
        gof->attribute_map_height = height;
    }

    const size_t sizeY  = size_t(width) * size_t(height);
    const size_t UV_width = width >> 1; // width / 2
    const size_t UV_height = height >> 1;
    const size_t sizeUV = sizeY >> 2U;
    const size_t total_size = sizeY + (sizeY >> 1);

    int Y_widths[height];
    int Y_linesizes[height];
    for (int y = 0; y < height; y++) {
        Y_widths[y] = y*width;
        Y_linesizes[y] = y*frames.front()->linesize[0];
    }

    int UV_widths[UV_height];
    int UV_linesizes[UV_height];
    for (int y = 0; y < UV_height; y++) {
        UV_widths[y] = y*UV_width;
        UV_linesizes[y] = y*frames.front()->linesize[1];
    }

    bool is_yuv42010ple = frames.front()->format == AV_PIX_FMT_YUV420P10LE;

    if (is_yuv42010ple) { // bit depth = 10
        for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
            AVFrame* frame = frames[frame_index];
            std::vector<uint8_t>& map = mapList[frame_index].get();

            map.resize(total_size); // Y + U + V
            uint8_t* dstY = map.data();
            uint8_t* dstU = dstY + sizeY;
            uint8_t* dstV = dstU + sizeUV;
            // Copy Y plane
            for (int y = 0; y < height; ++y) {
                const uint16_t* srcY = reinterpret_cast<const uint16_t*>(frame->data[0] + Y_linesizes[y]);
                for (int x = 0; x < width; ++x) {
                    // dstY[Y_widths[y] + x] = static_cast<uint8_t>(srcY[x] >> 2);

                    uint16_t out = (srcY[x] + 2) >> 2;
                    dstY[Y_widths[y] + x] = static_cast<uint8_t>(std::min<uint16_t>(out, 255));
                }
            }

            // Copy U and V planes
            for (int y = 0; y < UV_height; ++y) {
                const uint16_t* srcU = reinterpret_cast<const uint16_t*>(frame->data[1] + UV_linesizes[y]);
                const uint16_t* srcV = reinterpret_cast<const uint16_t*>(frame->data[2] + UV_linesizes[y]);
                for (int x = 0; x < UV_width; ++x) {
                    // dstU[UV_widths[y] + x] = static_cast<uint8_t>(srcU[x] >> 2);
                    // dstV[UV_widths[y] + x] = static_cast<uint8_t>(srcV[x] >> 2);

                    uint16_t out_u = (srcU[x] + 2) >> 2;
                    uint16_t out_v = (srcV[x] + 2) >> 2;
                    dstU[UV_widths[y] + x] = static_cast<uint8_t>(std::min<uint16_t>(out_u, 255));
                    dstV[UV_widths[y] + x] = static_cast<uint8_t>(std::min<uint16_t>(out_v, 255));
                }
            }
            
            av_frame_free(&frame);
        }
    } else { // bit depth = 8
        for (size_t frame_index = 0; frame_index < frames.size(); ++frame_index) {
            AVFrame* frame = frames[frame_index];
            std::vector<uint8_t>& map = mapList[frame_index].get();

            map.resize(total_size); // Y + U + V
            uint8_t* dstY = map.data();
            uint8_t* dstU = dstY + sizeY;
            uint8_t* dstV = dstU + sizeUV;
            // Copy Y plane
            for (int y = 0; y < height; ++y) {
                memcpy(dstY + Y_widths[y], frame->data[0] + Y_linesizes[y], width);
            }

            // Copy U and V planes
            for (int y = 0; y < UV_height; ++y) {
                memcpy(dstU + UV_widths[y], frame->data[1] + UV_linesizes[y], UV_width);
                memcpy(dstV + UV_widths[y], frame->data[2] + UV_linesizes[y], UV_width);
            }
        }
    }
}


} // anonymous namespace

void DecoderFFmpeg::decodeGOFMaps(const std::shared_ptr<uvgvpcc_dec::GOF>& gof) {
    std::string decoderName;  // For log and debug
    switch (decoderType_) {
        case OCCUPANCY:
            decoderName = "FFmpeg occupancy map decoder";
            break;
        case GEOMETRY:
            decoderName = "FFmpeg geometry map decoder";
            break;
        case ATTRIBUTE:
            decoderName = "FFmpeg attribute map decoder";
            break;
        default:
            assert(false);
    }

    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_H265);
    if (!codec) {
        throw std::runtime_error(decoderName + ": Failed to find codec.");
    }

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    codec_ctx->thread_count = 0;
    codec_ctx->thread_type = FF_THREAD_FRAME;

    if ((avcodec_open2(codec_ctx, codec, nullptr)) < 0) {
        throw std::runtime_error(decoderName + ": Failed to open codec.");
    }

    std::vector<std::reference_wrapper<std::vector<uint8_t>>> mapList;
    setMapList(gof, mapList, decoderType_);

    std::vector<uint8_t>& bitstream = getBitstream(gof, decoderType_);

    std::vector<AVFrame*> frames = {};
    decodeVideoFFmpeg(frames, codec_ctx, bitstream, decoderName);
    avcodec_free_context(&codec_ctx);

    if (decoderType_ == ATTRIBUTE && p_->useTMC2AttributeYUVConversion) {
        std::vector<std::reference_wrapper<std::vector<uint16_t>>> mapList_16bit;
        setMapList_16bit(gof, mapList_16bit);
        writeDecodedFramesToMapList_tmc2Yuv420yuv444Conversion_fast(gof, mapList_16bit, frames, decoderType_);
    } else {
        writeDecodedFramesToMapList(gof, mapList, frames, decoderType_);
    }

    if (p_->exportIntermediateFiles) {
        for(const auto& frame : gof->frames) {
            switch (decoderType_) {
                case OCCUPANCY: {}
                    FileExport::exportImageOccupancy(frame, gof->occupancy_map_width);
                    break;
                case GEOMETRY:
                    FileExport::exportImageGeometryBgFill(frame, gof->geometry_map_width, gof->geometry_map_height, gof->doubleLayer);
                    break;
                case ATTRIBUTE:
                    FileExport::exportImageAttributeYUV(frame, gof->attribute_map_width, gof->attribute_map_height, gof->doubleLayer);
                    break;
                default:
                    assert(false);
            }
        }
    }
}